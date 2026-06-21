//
// Created by vogje01 on 6/21/26.
//

#pragma once

// C++ includes
#include <string>

// Awsmock includes
#include <awsmock/dto/lambda/model/LambdaStatus.h>

namespace Awsmock::Lrt {

    class ILambdaRuntime {
      public:

        virtual ~ILambdaRuntime() = default;

        /**
         * @brief Invoke the Lambda handler with the given event JSON.
         *
         * @param eventJson  serialized Lambda event
         * @return           handler response as JSON string
         */
        virtual std::string invoke(const std::string &eventJson) = 0;

        /**
         * @brief Return the current runtime status snapshot.
         */
        virtual Dto::Lambda::LambdaStatus getStatus() const = 0;

        /**
         * @brief Initiate graceful shutdown.
         *
         * Implementations must release all resources (terminate subprocesses,
         * unload shared libraries, destroy VMs) before returning.
         */
        virtual void shutdown() = 0;
    };

}// namespace Awsmock::GRT
