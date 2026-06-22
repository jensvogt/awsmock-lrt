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
         * @brief Initialize the singleton (call once at startup).
         *
         * @param runtime   lambda runtime instance
         * @param functionName lambda function name
         * @param port      local HTTP port
         * @param managerHost AwsMock manager host
         * @param managerPort AwsMock manager port
         * @return reference to the singleton instance
         */
        static StatusReporter &initialize(const ILambdaRuntime &runtime,const std::string &functionName,int port,const std::string &managerHost,int managerPort);

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
         * @brief Private constructor
         *
         * @param runtime lambda runtime
         * @param functionName name of the lambda function
         * @param port lambda private port
         * @param managerHost name of the manager host
         * @param managerPort port of the manager
         */
        StatusReporter(const ILambdaRuntime &runtime,std::string functionName,int port,const std::string &managerHost,int managerPort);

        const ILambdaRuntime &_runtime;
        std::string _functionName;
        int _port;
        std::string _managerHost;
        int _managerPort;

        static StatusReporter *_instance;
        static std::mutex _mutex;
    };

}// namespace Awsmock::Lrt
