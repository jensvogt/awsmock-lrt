//
// Created by vogje01 on 21/06/2026.
//

// C++ includes
#include <chrono>

// AwsMock includes
#include <awsmock/lrt/StatusReporter.h>

namespace http = boost::beast::http;

namespace Awsmock::Lrt {

    StatusReporter *StatusReporter::_instance = nullptr;
    std::mutex StatusReporter::_mutex;

    StatusReporter::StatusReporter(std::string functionName, std::string instanceId, const int port, const std::string &managerHost, const int managerPort)
        : _functionName(std::move(functionName)), _instanceId(std::move(instanceId)), _managerHost(managerHost), _port(port), _managerPort(managerPort) {
    }

    StatusReporter &StatusReporter::initialize(const std::string &functionName, const std::string &instanceId, const int port, const std::string &managerHost, const int managerPort) {
        std::lock_guard lock(_mutex);
        if (!_instance) {
            _instance = new StatusReporter(functionName, instanceId, port, managerHost, managerPort);
        }
        return *_instance;
    }

    StatusReporter &StatusReporter::initialize(const ILambdaRuntime &runtime, const std::string &functionName, const std::string &instanceId, const int port, const std::string &managerHost, const int managerPort) {
        initialize(functionName, instanceId, port, managerHost, managerPort);
        _instance->setRuntime(runtime);
        return *_instance;
    }

    void StatusReporter::setRuntime(const ILambdaRuntime &runtime) {
        std::lock_guard lock(_mutex);
        _runtime = &runtime;
    }

    StatusReporter &StatusReporter::instance() {
        std::lock_guard lock(_mutex);
        if (!_instance) {
            throw std::runtime_error("StatusReporter not initialized — call initialize() first");
        }
        return *_instance;
    }

    void StatusReporter::reportStatus() const {
        try {
            Dto::Lambda::LambdaStatus status;
            if (_runtime) {
                status = _runtime->getStatus();
            } else {
                status.runtimeStatus = RuntimeStatus::starting;
            }
            status.functionName = _functionName;
            status.port = _port;
            status.instanceId = _instanceId;
            const std::map<std::string, std::string> headers = {
                    {"x-awsmock-target", "lambda"},
                    {"x-awsmock-action", "lambda-runtime-status"},
            };
            Core::HttpSocket::SendJson(http::verb::post, _managerHost, _managerPort, "/", status.ToJson(), headers);
            log_debug << "Lambda runtime status reported to manager, function: " << _functionName;
        } catch (const std::exception &ex) {
            log_warning << "Lambda runtime status report failed: " << ex.what();
        }
    }

    void StatusReporter::reportStopped() const {
        try {
            Dto::Lambda::LambdaStatus status;
            if (_runtime) {
                status = _runtime->getStatus();
            }
            status.runtimeStatus = RuntimeStatus::stopped;
            status.lastStop = std::chrono::system_clock::now();
            status.functionName = _functionName;
            status.port = _port;
            status.instanceId = _instanceId;
            const std::map<std::string, std::string> headers = {
                    {"x-awsmock-target", "lambda"},
                    {"x-awsmock-action", "lambda-runtime-status"},
            };
            Core::HttpSocket::SendJson(http::verb::post, _managerHost, _managerPort, "/", status.ToJson(), headers);
            log_info << "Lambda runtime stopped status reported to manager, function: " << _functionName;
        } catch (const std::exception &ex) {
            log_warning << "Lambda runtime stopped status report failed: " << ex.what();
        }
    }

}// namespace Awsmock::Lrt
