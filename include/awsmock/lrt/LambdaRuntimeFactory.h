//
// Created by vogje01 on 6/21/26.
//

#pragma once

// C++ includes
#include <algorithm>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Awsmock includes
#include <awsmock/lrt/LambdaCppRuntime.h>
#include <awsmock/lrt/LambdaJvmRuntime.h>
#include <awsmock/lrt/LambdaNodeRuntime.h>
#include <awsmock/lrt/LambdaPythonRuntime.h>
#include <awsmock/lrt/ILambdaRuntime.h>

namespace Awsmock::Lrt {

    /**
     * @brief Parameters forwarded to the runtime constructor.
     *
     * Not all fields are used by every runtime — each impl picks what it needs.
     */
    struct RuntimeParams {
        std::string codePath;
        std::string handler;
        std::map<std::string, std::string> envVars;

        // JVM runtimes
        std::vector<std::string> jvmArgs;
        std::vector<std::string> runtimeJars;

        // Node.js runtimes
        std::string nodeExecutable = "node";

        // Python runtimes
        std::string pythonExecutable = "python3";

        // Manager endpoint
        std::string managerHost = "host.docker.internal";
        int managerPort = 4566;
        int reportPeriod = 30;
    };

    /**
     * @brief Creates the appropriate ILambdaRuntime for the given AWS runtime identifier.
     *
     * Supported runtime strings (case-insensitive):
     *   Java:   java8, java11, java17, java21, java25
     *   Node:   nodejs18.x, nodejs20.x, nodejs22.x
     *   Python: python3.9, python3.10, python3.11, python3.12, python3.13
     *   C++:    provided, provided.al2, provided.al2023
     */
    class LambdaRuntimeFactory {
      public:

        static std::unique_ptr<ILambdaRuntime> create(const std::string &runtime, const RuntimeParams &p) {
            const std::string rt = toLower(runtime);

            if (rt == "java17" || rt == "java21" || rt == "java25")
                return std::make_unique<LambdaJvmRuntime>(p.codePath, p.handler, p.envVars, p.jvmArgs, p.runtimeJars);

            if (rt == "nodejs20.x" || rt == "nodejs22.x")
                return std::make_unique<LambdaNodeRuntime>(p.codePath, p.handler, p.envVars, p.nodeExecutable);

            if (rt == "python3.11" || rt == "python3.12" || rt == "python3.13")
                return std::make_unique<LambdaPythonRuntime>(p.codePath, p.handler, p.envVars, p.pythonExecutable);

            if (rt == "provided" || rt == "provided.al2" || rt == "provided.al2023")
                return std::make_unique<LambdaCppRuntime>(p.codePath, p.handler, p.envVars);

            throw std::runtime_error("Unsupported runtime: " + runtime);
        }

      private:

        static std::string toLower(std::string s) {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
            return s;
        }
    };

}// namespace Awsmock::GRT
