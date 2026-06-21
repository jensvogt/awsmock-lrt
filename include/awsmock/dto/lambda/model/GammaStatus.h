//
// Created by vogje01 on 6/20/26.
//

#pragma once

// Awsmock includes
#include <awsmock/core/JsonUtils.h>
#include <awsmock/core/config/Configuration.h>
#include <awsmock/dto/common/BaseObject.h>
#include <awsmock/dto/gamma/model/RuntimeStatus.h>

namespace Awsmock::Dto::Gamma {

    struct GammaStatus : Common::BaseObject<GammaStatus> {

        /**
         * @brief Runtime status
         */
        RuntimeStatus runtimeStatus = RuntimeStatus::stopped;

        /**
         * @brief PID
         */
        int pid{};

        /**
         * @brief Invocation count
         */
        int invocations{};

        /**
         * @brief Average invocation duration in milliseconds
         */
        double avgDuration{};

        /**
         * @brief Lambda function name — set by the GRT before reporting to awsmockmgr
         */
        std::string functionName;

        /**
         * @brief HTTP port the GRT is listening on — identifies the instance within a function
         */
        int port{};

      private:

        friend GammaStatus tag_invoke(boost::json::value_to_tag<GammaStatus>, boost::json::value const &v) {
            GammaStatus r;
            r.runtimeStatus = runtimeStatusFromString(Core::Json::GetStringValue(v, "runtimeStatus"));
            r.pid = Core::Json::GetIntValue(v, "pid");
            r.invocations = Core::Json::GetIntValue(v, "invocations");
            r.avgDuration = Core::Json::GetDoubleValue(v, "avgDuration");
            r.functionName = Core::Json::GetStringValue(v, "functionName");
            r.port = Core::Json::GetIntValue(v, "port");
            return r;
        }

        friend void tag_invoke(boost::json::value_from_tag, boost::json::value &jv, GammaStatus const &obj) {
            jv = {
                    {"runtimeStatus", runtimeStatusToString(obj.runtimeStatus)},
                    {"pid", obj.pid},
                    {"invocations", obj.invocations},
                    {"avgDuration", obj.avgDuration},
                    {"functionName", obj.functionName},
                    {"port", obj.port},
            };
        }
    };
}// namespace Awsmock::Dto::Gamma
