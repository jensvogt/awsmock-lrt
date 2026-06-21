//
// Created by vogje01 on 6/21/26.
//

#pragma once

// C++ includes
#include <map>
#include <string>

// Awsmock includes
#include <awsmock/lrt/GammaProcessRuntime.h>

namespace Awsmock::Lrt {

    /**
     * @brief Lambda runtime for Python functions.
     *
     * Writes a small shim script to a temp file, then forks a persistent
     * `python3` child process.  The shim reads newline-delimited JSON events
     * from stdin, calls the handler, and writes the JSON response to stdout.
     *
     * Handler format: "lambda_function.lambda_handler"  (module.function)
     * Code path:      directory containing the unpacked Lambda zip
     */
    class GammaPythonRuntime final : public GammaProcessRuntime {
      public:

        /**
         * @param codePath           directory containing the Lambda source
         * @param handler            "module.function", e.g. "lambda_function.lambda_handler"
         * @param envVars            environment variables to set in the Python process
         * @param pythonExecutable   path or name of the python binary (default: "python3")
         */
        GammaPythonRuntime(const std::string &codePath,
                           const std::string &handler,
                           const std::map<std::string, std::string> &envVars = {},
                           const std::string &pythonExecutable = "python3");

        ~GammaPythonRuntime() override;

      private:

        std::string _shimPath;

        void writeShim();
    };

}// namespace Awsmock::GRT
