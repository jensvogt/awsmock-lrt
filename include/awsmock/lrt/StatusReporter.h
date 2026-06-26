//
// Created by vogje01 on 21/06/2026.
//

#pragma once

// C++ includes
#include <mutex>
#include <string>
#include <utility>

// Boost includes
#include <boost/beast/http/verb.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

// AwsMock includes
#include <awsmock/lrt/ILambdaRuntime.h>
#include <awsmock/core/HttpSocket.h>
#include <awsmock/core/logging/LogStream.h>

namespace Awsmock::Lrt {

    /**
     * @brief Singleton that periodically reports the GRT status to the AwsMock manager.
     *
     * @author jens.vogt\@opitz-consulting.com
     */
    class StatusReporter {

    public:
        /**
         * @brief Initialize the singleton before the runtime exists (phase 1).
         *
         * Call this early so the reporter can send "starting" status while the
         * runtime is still being created. Follow up with setRuntime() once the
         * runtime is ready.
         *
         * @param functionName lambda function name
         * @param port         local HTTP port
         * @param managerHost  AwsMock manager endpoint URL
         * @param managerPort  AwsMock manager port
         * @return reference to the singleton instance
         */
        static StatusReporter &initialize(const std::string &functionName, int port, const std::string &managerHost, int managerPort);

        /**
         * @brief Initialize the singleton with a runtime in one step (convenience overload).
         *
         * Equivalent to calling initialize(functionName, port, managerHost, managerPort)
         * followed by setRuntime(runtime).
         *
         * @param runtime         lambda runtime instance
         * @param functionName    lambda function name
         * @param port            local HTTP port
         * @param managerHost  AwsMock manager endpoint URL
         * @param managerPort  AwsMock manager port
         * @return reference to the singleton instance
         */
        static StatusReporter &initialize(const ILambdaRuntime &runtime, const std::string &functionName, int port, const std::string &managerHost, int managerPort);

        /**
         * @brief Bind the runtime after phase-1 initialization.
         *
         * @param runtime lambda runtime instance (must outlive this singleton)
         */
        void setRuntime(const ILambdaRuntime &runtime);

        /**
         * @brief Access the singleton instance.
         *
         * @return reference to the singleton instance
         */
        static StatusReporter &instance();

        /**
         * @brief Report the current runtime status to the AwsMock manager.
         *
         * Catches and logs all exceptions so callers (e.g. a scheduler task) never throw.
         */
        void reportStatus() const;

        // Non-copyable, non-movable
        StatusReporter(const StatusReporter &) = delete;

        StatusReporter &operator=(const StatusReporter &) = delete;

        StatusReporter(StatusReporter &&) = delete;

        StatusReporter &operator=(StatusReporter &&) = delete;

    private:
        /**
         * @brief Channeled logger
         */
        mutable logger_t _logger{boost::log::keywords::channel = "LambdaRuntime"};

        /**
         * @brief Private constructor (phase-1, no runtime yet).
         */
        StatusReporter(std::string functionName, int port, const std::string &managerHost, int managerPort);

        const ILambdaRuntime *_runtime = nullptr;
        std::string _functionName;
        std::string _managerHost;
        std::string _instanceId;
        int _port;
        int _managerPort;

        static StatusReporter *_instance;
        static std::mutex _mutex;
    };

} // namespace Awsmock::Lrt
