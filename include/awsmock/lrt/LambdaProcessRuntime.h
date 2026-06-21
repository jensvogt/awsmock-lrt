//
// Created by vogje01 on 6/21/26.
//

#pragma once

// C++ includes
#include <map>
#include <mutex>
#include <string>
#include <vector>

// POSIX includes
#include <sys/types.h>

// Awsmock includes
#include <awsmock/core/SystemUtils.h>
#include <awsmock/core/logging/LogStream.h>
#include <awsmock/lrt/ILambdaRuntime.h>

namespace Awsmock::Lrt {

    /**
     * @brief Shared base for runtimes that delegate to a long-lived child process.
     *
     * Subclasses (LambdaNodeRuntime, LambdaPythonRuntime) call spawn() from their
     * constructor after writing any necessary shim files. The parent/child
     * communicate with newline-delimited JSON over a pair of pipes:
     *
     *   parent writes: <eventJson>\n
     *   parent reads:  <responseJson>\n
     *
     * invoke() is serialised by a mutex — the HTTP server may be multi-threaded
     * but the child process handles one request at a time.
     */
    class LambdaProcessRuntime : public ILambdaRuntime {
      public:

        ~LambdaProcessRuntime() override;

        std::string invoke(const std::string &eventJson) override;
        Dto::Lambda::LambdaStatus getStatus() const override { return _status; }
        void shutdown() override;

      protected:

        /**
         * @brief Fork and exec the child process.
         *
         * @param executable  path or name (searched via PATH) of the interpreter
         * @param args        argv[1..] passed to the executable (shim path, handler, etc.)
         * @param envVars     extra environment variables set in the child
         */
        void spawn(const std::string &executable,
                   const std::vector<std::string> &args,
                   const std::map<std::string, std::string> &envVars);

        /**
         * @brief Channeled logger
         */
        logger_t _logger{boost::log::keywords::channel = "LambdaRuntime"};

        /**
         * @brief Runtime status
         */
        Dto::Lambda::LambdaStatus _status;

      private:

        /**
         * @brief Read one newline-terminated line from the child's stdout.
         */
        std::string readLine() const;

        /**
         * @brief Terminate and reap the child process.
         */
        void killChild();

        pid_t _pid = -1;
        int _stdinFd = -1;
        int _stdoutFd = -1;
        std::mutex _invokeMtx;
    };

}// namespace Awsmock::GRT
