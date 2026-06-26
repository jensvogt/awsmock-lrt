//
// Created by vogje01 on 6/21/26.
//

#include <awsmock/lrt/LambdaCppRuntime.h>

// C++ includes
#include <chrono>
#include <stdexcept>

// POSIX includes
#include <dlfcn.h>

// Awsmock includes
#include <awsmock/lrt/StatusReporter.h>

namespace Awsmock::Lrt {

    LambdaCppRuntime::LambdaCppRuntime(const std::string &libPath,const std::string &handler,const std::map<std::string, std::string> &envVars) {
        _status.runtimeStatus = RuntimeStatus::starting;
        _status.pid = Core::SystemUtils::GetPid();
        StatusReporter::instance().reportStatus();

        for (const auto &[k, v]: envVars)
            setenv(k.c_str(), v.c_str(), 1);

        _handle = dlopen(libPath.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!_handle)
            throw std::runtime_error(std::string("dlopen failed: ") + dlerror());

        const std::string symbol = handler.empty() ? "lambda_invoke" : handler;
        _invokeFn = reinterpret_cast<InvokeFn>(dlsym(_handle, symbol.c_str()));
        if (!_invokeFn)
            throw std::runtime_error("Symbol not found: " + symbol + " — " + dlerror());

        // lambda_free is optional
        _freeFn = reinterpret_cast<FreeFn>(dlsym(_handle, "lambda_free"));

        _status.runtimeStatus = RuntimeStatus::idle;
        _status.lastStart = std::chrono::system_clock::now();
        StatusReporter::instance().reportStatus();
        log_info << "C++ runtime loaded, lib: " << libPath << ", symbol: " << symbol;
    }

    LambdaCppRuntime::~LambdaCppRuntime() {
        shutdown();
    }

    std::string LambdaCppRuntime::invoke(const std::string &eventJson) {
        std::lock_guard lock(_invokeMtx);
        _status.runtimeStatus = RuntimeStatus::running;

        const auto t0 = std::chrono::steady_clock::now();
        const char *raw = _invokeFn(eventJson.c_str());
        std::string result(raw ? raw : "null");
        if (_freeFn && raw) _freeFn(raw);

        const double elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        _status.invocations++;
        _status.avgDuration += (elapsed - _status.avgDuration) / _status.invocations;
        _status.lastInvocation = std::chrono::system_clock::now();
        _status.runtimeStatus = RuntimeStatus::idle;
        StatusReporter::instance().reportStatus();
        return result;
    }

    void LambdaCppRuntime::shutdown() {
        if (_handle) {
            dlclose(_handle);
            _handle = nullptr;
            _invokeFn = nullptr;
            _freeFn = nullptr;
            _status.runtimeStatus = RuntimeStatus::stopped;
            _status.lastStop = std::chrono::system_clock::now();
            StatusReporter::instance().reportStatus();
            log_info << "C++ runtime unloaded";
        }
    }

}// namespace Awsmock::GRT
