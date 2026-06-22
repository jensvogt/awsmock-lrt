//
// Created by vogje01 on 21/06/2026.
//

// AwsMock includes
#include <awsmock/lrt/StatusReporter.h>

namespace http = boost::beast::http;

namespace Awsmock::Lrt {

    StatusReporter *StatusReporter::_instance = nullptr;
    std::mutex StatusReporter::_mutex;

    StatusReporter::StatusReporter(const ILambdaRuntime &runtime,std::string functionName,const int port,const std::string &managerHost,const int managerPort)
        : _runtime(runtime), _functionName(std::move(functionName)), _port(port), _managerHost(managerHost), _managerPort(managerPort) {}

    StatusReporter &StatusReporter::initialize(const ILambdaRuntime &runtime,const std::string &functionName,const int port,const std::string &managerHost,const int managerPort) {
        std::lock_guard lock(_mutex);
        if (!_instance) {
            _instance = new StatusReporter(runtime, functionName, port, managerHost, managerPort);
        }
        return *_instance;
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
            Dto::Lambda::LambdaStatus status = _runtime.getStatus();
            status.functionName = _functionName;
            status.port = _port;
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

}// namespace Awsmock::Lrt
