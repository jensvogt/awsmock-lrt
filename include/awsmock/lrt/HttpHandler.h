
// Created by vogje01 on 6/20/26.
//

#pragma once

// Boost includes
#include <boost/beast/http.hpp>

// Project includes
#include <awsmock/core/logging/LogStream.h>
#include <awsmock/dto/gamma/model/GammaConfig.h>
#include <awsmock/lrt/IGammaRuntime.h>
#include <awsmock/lrt/HttpServer.h>

namespace Awsmock::Lrt {

    class HttpHandler {
      public:

        /**
         * @param server   the HTTP server to register routes on
         * @param runtime  the runtime used by the /invoke handler
         * @param config   configuration
         */
        HttpHandler(HttpServer &server, IGammaRuntime &runtime, const Dto::Gamma::GammaConfig &config);

        /**
         * @brief Register GET, POST, and PUT catch-all routes on the server.
         *        Call once before HttpServer::start().
         */
        void registerRoutes();

      private:

        /**
         * @brief Channeled logger
         */
        logger_t _logger{boost::log::keywords::channel = "GammaRuntime"};

        HttpServer &_server;
        IGammaRuntime &_runtime;

        /**
         * @brief Dispatch all GET requests by path.
         */
        Response handleGet(const Request &req) const;

        /**
         * @brief Dispatch all POST requests by path.
         */
        Response handlePost(const Request &req) const;

        /**
         * @brief Dispatch all PUT requests by path.
         */
        Response handlePut(const Request &req);

        // ── individual route implementations ─────────────────────────────────

        Response handleStatus(const Request &req) const;
        Response handleConfig(const Request &req) const;
        Response handleInvoke(const Request &req) const;

        static Response notFound(const Request &req);
        static Response methodNotAllowed(const Request &req);

        Dto::Gamma::GammaConfig _config;
    };

}// namespace Awsmock::GRT
