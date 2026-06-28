//
// Created by vogje01 on 6/18/26.
//

#include <chrono>
#include <pthread.h>

#include <awsmock/lrt/LambdaJvmRuntime.h>


namespace Awsmock::Lrt {

    LambdaJvmRuntime::LambdaJvmRuntime(const std::string &jarPath, const std::string &handler, const std::map<std::string, std::string> &envVars, const std::vector<std::string> &jvmArgs, const std::vector<std::string> &runtimeJars) {

        _status.runtimeStatus = RuntimeStatus::starting;
        _status.pid = Core::SystemUtils::GetPid();
        StatusReporter::instance().reportStatus();

        parseHandler(handler);

        for (const auto &[key, value]: envVars) {
            setenv(key.c_str(), value.c_str(), 1);
        }

        // Build classpath: runtime JARs first (so they shadow nothing in the customer JAR), then the customer JAR.
        std::string cp = jarPath;
        for (const auto &jar: runtimeJars){
            cp += ":" + jar;
        }
        std::string cpOption = "-Djava.class.path=" + cp;
        std::vector<JavaVMOption> opts;
        opts.push_back({cpOption.data(), nullptr});
        for (const auto &arg: jvmArgs)
            opts.push_back({const_cast<char *>(arg.c_str()), nullptr});

        JavaVMInitArgs args{};
        args.version = JNI_VERSION_1_8;
        args.nOptions = static_cast<jint>(opts.size());
        args.options = opts.data();
        args.ignoreUnrecognized = JNI_FALSE;

        if (const jint rc = JNI_CreateJavaVM(&_jvm, reinterpret_cast<void **>(&_env), &args); rc != JNI_OK)
            throw std::runtime_error("JNI_CreateJavaVM failed: " + std::to_string(rc));

        // Pin the system classloader as the context classloader of the JVM's main thread.
        // JNI-created JVMs leave the context classloader unset (null/bootstrap), so Spring
        // threads inherit null, and SerializeUtil.loadCustomerClass fails with
        // ClassNotFoundException for classes that ARE on the system classpath.
        setSystemClassLoader(_env);

        // Load handler class
        _handlerClass = _env->FindClass(_className.c_str());
        checkException(_env);
        if (!_handlerClass) {
            log_error << "Class not found" << _className;
            throw std::runtime_error("Class not found: " + _className);
        }

        // ── Detect invocation mode ────────────────────────────────────────────────
        // If the class implements RequestStreamHandler we use the stream path;
        // otherwise we fall back to String handleRequest(String).
        jclass const streamHandlerIface = _env->FindClass("com/amazonaws/services/lambda/runtime/RequestStreamHandler");
        _env->ExceptionClear();// not fatal if AWS SDK is absent
        if (streamHandlerIface && _env->IsAssignableFrom(_handlerClass, streamHandlerIface)) {
            _invokeMode = InvokeMode::RequestStreamHandler;
        }

        // ── Instantiate handler ───────────────────────────────────────────────────
        jmethodID const ctor = _env->GetMethodID(_handlerClass, "<init>", "()V");
        checkException(_env);
        _handlerClass = static_cast<jclass>(_env->NewGlobalRef(_handlerClass));
        jobject localInstance = _env->NewObject(_handlerClass, ctor);
        checkException(_env);
        _handlerInstance = _env->NewGlobalRef(localInstance);
        _env->DeleteLocalRef(localInstance);

        if (_invokeMode == InvokeMode::RequestStreamHandler) {
            // void handleRequest(InputStream, OutputStream, Context)
            _handleMethod = _env->GetMethodID(_handlerClass, "handleRequest", "(Ljava/io/InputStream;Ljava/io/OutputStream;"
                                                                              "Lcom/amazonaws/services/lambda/runtime/Context;)V");
            checkException(_env);

            // Cache ByteArrayInputStream — promote to global ref so GC cannot collect it
            jclass localBais = _env->FindClass("java/io/ByteArrayInputStream");
            checkException(_env);
            _baisCtor = _env->GetMethodID(localBais, "<init>", "([B)V");
            checkException(_env);
            _baisClass = static_cast<jclass>(_env->NewGlobalRef(localBais));
            _env->DeleteLocalRef(localBais);

            // Cache ByteArrayOutputStream — promote to global ref
            jclass localBaos = _env->FindClass("java/io/ByteArrayOutputStream");
            checkException(_env);
            _baosCtor = _env->GetMethodID(localBaos, "<init>", "()V");
            checkException(_env);
            _baosToBytes = _env->GetMethodID(localBaos, "toByteArray", "()[B");
            checkException(_env);
            _baosClass = static_cast<jclass>(_env->NewGlobalRef(localBaos));
            _env->DeleteLocalRef(localBaos);
            std::cout << "Handler mode: RequestStreamHandler (FunctionInvoker path)\n";

        } else {

            // String handleRequest(String)  — or whatever _methodName is
            _handleMethod = _env->GetMethodID(_handlerClass, _methodName.c_str(), "(Ljava/lang/String;)Ljava/lang/String;");
            checkException(_env);
            std::cout << "Handler mode: String function\n";
        }
        _status.runtimeStatus = RuntimeStatus::idle;
        _status.lastStart = std::chrono::system_clock::now();
        StatusReporter::instance().reportStatus();
    }

    // ── invoke ────────────────────────────────────────────────────────────────────

    std::string LambdaJvmRuntime::invoke(const std::string &eventJson) {
        log_debug << "Starting invocation";
        _status.runtimeStatus = RuntimeStatus::running;

        // JNI calls from an attached native thread consume that thread's native stack,
        // not the JVM's -Xss budget.  Boost.Asio io_context threads have a default
        // native stack of ~8 MB on Linux — not enough for Spring Cloud Function's deep
        // class-loading chain on first invocation, which causes a StackOverflowError.
        // Run the JNI work on a dedicated pthread with a 32 MB stack.
        struct JniWork {
            LambdaJvmRuntime *self;
            const std::string *ev;
            std::string result;
            std::exception_ptr ex;
            double elapsed = 0.0;
        };
        JniWork work{this, &eventJson};

        pthread_t tid;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, 32 * 1024 * 1024);
        if (pthread_create(&tid, &attr, [](void *p) -> void * {
                auto &w = *static_cast<JniWork *>(p);
                try {
                    JNIEnv *env = w.self->getEnv();
                    const auto t0 = std::chrono::steady_clock::now();
                    w.result = (w.self->_invokeMode == InvokeMode::RequestStreamHandler)
                                       ? w.self->invokeStreamHandler(env, *w.ev)
                                       : w.self->invokeStringFunction(env, *w.ev);
                    w.elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
                } catch (...) {
                    w.ex = std::current_exception();
                }
                // Always detach: this pthread was freshly created so getEnv() attached it.
                w.self->_jvm->DetachCurrentThread();
                return nullptr;
            }, &work) != 0) {
            throw std::runtime_error("pthread_create for JNI invocation failed");
        }
        pthread_attr_destroy(&attr);
        pthread_join(tid, nullptr);

        if (work.ex) std::rethrow_exception(work.ex);

        _status.invocations++;
        _status.lastInvocation = std::chrono::system_clock::now();
        _status.avgDuration += (work.elapsed - _status.avgDuration) / _status.invocations;
        _status.runtimeStatus = RuntimeStatus::idle;
        log_debug << "Invocation finished, invocations: " << _status.invocations << ", avg duration: " << _status.avgDuration << "ms";
        return work.result;
    }

    std::string LambdaJvmRuntime::invokeStringFunction(JNIEnv *env, const std::string &eventJson) {
        const auto jEvent = env->NewStringUTF(eventJson.c_str());
        const auto jResult = reinterpret_cast<jstring>(env->CallObjectMethod(_handlerInstance, _handleMethod, jEvent));
        checkException(env);

        const char *chars = env->GetStringUTFChars(jResult, nullptr);
        std::string result(chars);
        env->ReleaseStringUTFChars(jResult, chars);
        return result;
    }

    std::string LambdaJvmRuntime::invokeStreamHandler(JNIEnv *env, const std::string &eventJson) {
        // Build byte[] from event JSON
        const auto len = static_cast<jsize>(eventJson.size());
        const auto jBytes = env->NewByteArray(len);
        env->SetByteArrayRegion(jBytes, 0, len, reinterpret_cast<const jbyte *>(eventJson.data()));

        // ByteArrayInputStream(byte[])
        const auto jInput = env->NewObject(_baisClass, _baisCtor, jBytes);
        checkException(env);

        // ByteArrayOutputStream()
        const auto jOutput = env->NewObject(_baosClass, _baosCtor);
        checkException(env);

        // Pass null for Context — FunctionInvoker null-checks it and falls through
        // to auto-detection or spring.cloud.function.definition when it is absent.
        env->CallVoidMethod(_handlerInstance, _handleMethod, jInput, jOutput, nullptr);
        checkException(env);

        // Read result bytes from ByteArrayOutputStream
        const auto jResultBytes = reinterpret_cast<jbyteArray>(env->CallObjectMethod(jOutput, _baosToBytes));
        checkException(env);

        const jsize resultLen = env->GetArrayLength(jResultBytes);
        const jbyte *resultData = env->GetByteArrayElements(jResultBytes, nullptr);
        std::string result(reinterpret_cast<const char *>(resultData), static_cast<std::size_t>(resultLen));
        env->ReleaseByteArrayElements(jResultBytes, const_cast<jbyte *>(resultData), JNI_ABORT);

        return result;
    }

    // ── Lifecycle ─────────────────────────────────────────────────────────────────

    LambdaJvmRuntime::~LambdaJvmRuntime() {
        if (_env) {
            if (_handlerInstance) _env->DeleteGlobalRef(_handlerInstance);
            if (_handlerClass) _env->DeleteGlobalRef(_handlerClass);
            if (_baisClass) _env->DeleteGlobalRef(_baisClass);
            if (_baosClass) _env->DeleteGlobalRef(_baosClass);
        }
        // shutdown() already terminated the process via System.exit(0); nothing to do.
        if (_jvm) _jvm->DestroyJavaVM();

        // Send status to manager
        _status.runtimeStatus = RuntimeStatus::stopped;
        _status.lastStop = std::chrono::system_clock::now();
        StatusReporter::instance().reportStatus();
    }

    void LambdaJvmRuntime::shutdown() {
        if (!_jvm || !_env) return;

        // System.exit(0): runs Java shutdown hooks, stops non-daemon threads, then
        // calls the native exit() — so this call does not return to C++.
        if (const auto systemClass = _env->FindClass("java/lang/System")) {
            if (const auto exitMethod = _env->GetStaticMethodID(systemClass, "exit", "(I)V")) {
                log_info << "Starting graceful JVM shutdown";
                _env->CallStaticVoidMethod(systemClass, exitMethod, static_cast<jint>(0));
            }
        }
        // Reached only if System.exit() somehow returned (should not happen).
        _jvm->DestroyJavaVM();
        _jvm = nullptr;
        log_info << "JVM shutdown complete";

        // Send status to manager
        _status.runtimeStatus = RuntimeStatus::stopped;
        _status.lastStop = std::chrono::system_clock::now();
        StatusReporter::instance().reportStatus();
        log_info << "Graceful JVM finished";
    }

    void LambdaJvmRuntime::parseHandler(const std::string &handler) {
        const auto sep = handler.rfind("::");
        if (sep == std::string::npos)
            throw std::runtime_error("Handler must be ClassName::methodName");

        _methodName = handler.substr(sep + 2);
        std::string cls = handler.substr(0, sep);
        for (auto &c: cls)
            if (c == '.') c = '/';
        _className = cls;
    }

    JNIEnv *LambdaJvmRuntime::getEnv() {
        JNIEnv *env = nullptr;
        if (const jint rc = _jvm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_8); rc == JNI_EDETACHED) {
            if (_jvm->AttachCurrentThread(reinterpret_cast<void **>(&env), nullptr) != JNI_OK)
                throw std::runtime_error("JNI AttachCurrentThread failed");
            // AttachCurrentThread leaves the context classloader null/bootstrap; set it
            // to the system classloader so Spring threads inherit it correctly.
            setSystemClassLoader(env);
        } else if (rc != JNI_OK) {
            throw std::runtime_error("JNI GetEnv failed: " + std::to_string(rc));
        }
        return env;
    }

    void LambdaJvmRuntime::checkException(JNIEnv *env) {
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            log_error << "Java exception thrown";
            throw std::runtime_error("Java exception thrown");
        }
    }

    void LambdaJvmRuntime::setSystemClassLoader(JNIEnv *env) {
        const auto threadCls = env->FindClass("java/lang/Thread");
        const auto clCls = env->FindClass("java/lang/ClassLoader");
        const auto curThreadMid = env->GetStaticMethodID(threadCls, "currentThread", "()Ljava/lang/Thread;");
        const auto getSysCLMid = env->GetStaticMethodID(clCls, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
        const auto setCtxCLMid = env->GetMethodID(threadCls, "setContextClassLoader", "(Ljava/lang/ClassLoader;)V");
        const auto curThread = env->CallStaticObjectMethod(threadCls, curThreadMid);
        const auto sysCL = env->CallStaticObjectMethod(clCls, getSysCLMid);
        env->CallVoidMethod(curThread, setCtxCLMid, sysCL);
        env->DeleteLocalRef(curThread);
        env->DeleteLocalRef(sysCL);
        env->DeleteLocalRef(threadCls);
        env->DeleteLocalRef(clCls);
        log_debug << "System classloader set on attached thread";
    }
}// namespace Awsmock::GRT