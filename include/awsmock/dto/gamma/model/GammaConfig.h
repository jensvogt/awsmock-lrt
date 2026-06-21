//
// Created by vogje01 on 6/20/26.
//

#pragma once

// Awsmock includes
#include <awsmock/core/JsonUtils.h>
#include <awsmock/core/config/Configuration.h>
#include <awsmock/dto/common/BaseObject.h>

namespace Awsmock::Dto::Gamma {

    struct GammaConfig : Common::BaseObject<GammaConfig> {

        /**
         * @brief Version
         */
        std::string version = Core::Configuration::getVersion();

        /**
         * @brief Boost version
         */
        std::string boostVersion = BOOST_LIB_VERSION;

        /**
         * @brief Port
         */
        int port{};

        /**
         * @brief Runtime
         */
        std::string runtime;

      private:

        friend GammaConfig tag_invoke(boost::json::value_to_tag<GammaConfig>, boost::json::value const &v) {
            GammaConfig r;
            r.version = Core::Json::GetStringValue(v, "version");
            r.boostVersion = Core::Json::GetStringValue(v, "boostVersion");
            r.port = Core::Json::GetIntValue(v, "port");
            r.runtime = Core::Json::GetStringValue(v, "runtime");
            return r;
        }

        friend void tag_invoke(boost::json::value_from_tag, boost::json::value &jv, GammaConfig const &obj) {
            jv = {
                    {"version", obj.version},
                    {"boostVersion", obj.boostVersion},
                    {"port", obj.port},
                    {"runtime", obj.runtime},
            };
        }
    };
}// namespace Awsmock::Dto::Gamma
