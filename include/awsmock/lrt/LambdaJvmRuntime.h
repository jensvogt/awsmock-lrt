//
// Created by vogje01 on 6/18/26.
//

#pragma once

// C++ includes
#include <cstdlib>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// Java includes
#include <jni.h>

// Awsmock includes
#include <awsmock/core/SystemUtils.h>
#include <awsmock/core/logging/LogStream.h>
#include <awsmock/dto/lambda/model/LambdaStatus.h>
#include <awsmock/lrt/ILambdaRuntime.h>
#include <awsmock/lrt/StatusReporter.h>

namespace Awsmock::Lrt {

    class LambdaJvmRuntime final : public ILambdaRuntime {
      public:

        /**
         * @brief Constructor for the awsmock lambda runtime
         *
         * @param jarPath       full path to the Lambda JAR
         * @param handler       "com.example.Handler::handleRequest" (lambda convention)
         * @param envVars       OS environment variables set before JVM creation
         * @param jvmArgs       extra JVM options, e.g. {"-Xmx512m", "-Dserver.port=9000"}
         * @param runtimeJars   additional JARs to prepend to the classpath (AWS runtime libs)
         */
        LambdaJvmRuntime(const std::string &jarPath, const std::string &handler, const std::map<std::string, std::string> &envVars = {}, const std::vector<std::string> &jvmArgs = {}, const std::vector<std::string> &runtimeJars = {});

        /**
         * @brief Destructor
         */
        ~LambdaJvmRuntime() override;

        /**
         * @brief Trigger a graceful JVM shutdown via System.exit(0).
         *
         * Calls System.exit(0) through JNI so Java shutdown hooks run and all
         * non-daemon threads are forcibly stopped. This call does not return —
         * the process terminates inside the JVM after hooks complete.
         * Call this explicitly before returning from main; the destructor skips
         * DestroyJavaVM() once the JVM has been shut down this way.
         */
        void shutdown() override;

        /**
         * @brief Invoke lambda function
         *
         * @param eventJson event JSON
         * @return invocation result
         */
        [[nodiscard]]
        std::string invoke(const std::string &eventJson) override;

        /**
         * @brief Return current status
         *
         * @return current status
         */
        Dto::Lambda::LambdaStatus getStatus() const override { return _status; }

      private:

        /**
         * @brief Channeled logger
         */
        logger_t _logger{boost::log::keywords::channel = "lambdaRuntime"};

        /**
         * @brief Invoke method.
         *
         * @par
         * Detected at construction time.
         */
        enum class InvokeMode { StringFunction,
                                RequestStreamHandler };

        /**
         * @brief Pointer to JVM
         */
        JavaVM *_jvm = nullptr;

        /**
         * @brief Pointer to environment
         */
        JNIEnv *_env = nullptr;

        /**
         * @brief Classname of handler method
         */
        std::string _className;

        /**
         * @brief Handler method name
         */
        std::string _methodName;

        /**
         * @brief Handler instance
         */
        jobject _handlerInstance = nullptr;

        /**
         * @brief Handler method ID
         */
        jmethodID _handleMethod = nullptr;

        /**
         * @brief Handler class
         */
        jclass _handlerClass = nullptr;

        /**
         * @brief Invoke method
         */
        InvokeMode _invokeMode = InvokeMode::StringFunction;

        // RequestStreamHandler helpers — populated only when _invokeMode == RequestStreamHandler
        jclass _baisClass = nullptr;// ByteArrayInputStream
        jmethodID _baisCtor = nullptr;
        jclass _baosClass = nullptr;// ByteArrayOutputStream
        jmethodID _baosCtor = nullptr;
        jmethodID _baosToBytes = nullptr;

        void parseHandler(const std::string &handler);
        void checkException(JNIEnv *env);
        void setSystemClassLoader(JNIEnv *env);

        // Returns the JNIEnv for the calling thread, attaching it to the JVM if needed.
        JNIEnv *getEnv();

        // Invocation paths
        std::string invokeStringFunction(JNIEnv *env, const std::string &eventJson);
        std::string invokeStreamHandler(JNIEnv *env, const std::string &eventJson);

        /**
         * @brief Status
         */
        Dto::Lambda::LambdaStatus _status;
    };
}// namespace Awsmock::GRT