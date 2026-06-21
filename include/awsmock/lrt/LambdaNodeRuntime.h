//
// Created by vogje01 on 6/21/26.
//

#pragma once

// C++ includes
#include <map>
#include <string>

// Awsmock includes
#include <awsmock/lrt/LambdaProcessRuntime.h>

namespace Awsmock::Lrt {

    /**
     * @brief Lambda runtime for Node.js functions.
     *
     * Writes a small shim script to a temp file, then forks a persistent
     * `node` child process.  The shim reads newline-delimited JSON events
     * from stdin, calls the handler, and writes the JSON response to stdout.
     *
     * Handler format: "index.handler"  (fileName.exportedFunction)
     * Code path:      directory containing the unpacked Lambda zip
     */
    class LambdaNodeRuntime final : public LambdaProcessRuntime {
      public:

        /**
         * @param codePath       directory containing the Lambda source (e.g. /var/task)
         * @param handler        "fileName.functionName", e.g. "index.handler"
         * @param envVars        environment variables to set in the Node process
         * @param nodeExecutable path or name of the node binary (default: "node")
         */
        LambdaNodeRuntime(const std::string &codePath,
                         const std::string &handler,
                         const std::map<std::string, std::string> &envVars = {},
                         const std::string &nodeExecutable = "node");

        ~LambdaNodeRuntime() override;

      private:

        std::string _shimPath;

        void writeShim(const std::string &codePath, const std::string &handler);
    };

}// namespace Awsmock::GRT
