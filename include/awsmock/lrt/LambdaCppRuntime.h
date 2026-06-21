//
// Created by vogje01 on 6/21/26.
//

#pragma once

// C++ includes
#include <map>
#include <mutex>
#include <string>

// Awsmock includes
#include <awsmock/core/SystemUtils.h>
#include <awsmock/core/logging/LogStream.h>
#include <awsmock/lrt/ILambdaRuntime.h>

namespace Awsmock::Lrt {

    /**
     * @brief Lambda runtime for native C++ shared libraries.
     *
     * Loads the handler shared library with dlopen() and resolves the
     * following exported symbols:
     *
     *   extern "C" const char* lambda_invoke(const char* eventJson);
     *
     * The returned pointer must remain valid until the next call to
     * lambda_invoke().  Storing the result in a module-level std::string
     * is the recommended pattern for implementors.
     *
     * Optionally:
     *   extern "C" void lambda_free(const char* result);
     *
     * If lambda_free is exported the runtime calls it after copying the result.
     *
     * Handler format: symbol name of the invoke function (default: "lambda_invoke")
     * Code path:      full path to the .so file
     */
    class LambdaCppRuntime final : public ILambdaRuntime {
      public:

        /**
         * @param libPath   full path to the Lambda shared library (.so)
         * @param handler   exported C symbol name (default: "lambda_invoke")
         * @param envVars   environment variables set before dlopen
         */
        LambdaCppRuntime(const std::string &libPath,
                        const std::string &handler,
                        const std::map<std::string, std::string> &envVars = {});

        ~LambdaCppRuntime() override;

        std::string invoke(const std::string &eventJson) override;
        Dto::Lambda::LambdaStatus getStatus() const override { return _status; }
        void shutdown() override;

      private:

        logger_t _logger{boost::log::keywords::channel = "lambdaRuntime"};

        using InvokeFn = const char *(*)(const char *);
        using FreeFn = void (*)(const char *);

        void *_handle = nullptr;
        InvokeFn _invokeFn = nullptr;
        FreeFn _freeFn = nullptr;
        std::mutex _invokeMtx;
        Dto::Lambda::LambdaStatus _status;
    };

}// namespace Awsmock::GRT
