//
// Created by vogje01 on 6/18/26.
//

#pragma once

// C++ includes
#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// Boost includes
#include <boost/asio.hpp>
#include <boost/beast.hpp>

// Project includes
#include <awsmock/dto/gamma/model/RuntimeStatus.h>
#include <awsmock/core/logging/LogStream.h>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;

using Request = http::request<http::string_body>;
using Response = http::response<http::string_body>;
using Handler = std::function<Response(const Request &)>;

namespace Awsmock::Lrt {

    class HttpServer {
      public:

        /**
         * @param address  bind address, e.g. "0.0.0.0"
         * @param port     TCP port to listen on
         * @param threads  size of the io_context thread pool (default: hardware concurrency)
         */
        explicit HttpServer(const std::string &address, unsigned short port, std::size_t threads = std::thread::hardware_concurrency());

        /**
         * @brief Destructor
         */
        ~HttpServer();

        /**
         * @brief Register a handler for a specific HTTP method + path prefix.
         *        The first matching registration wins.
         */
        void addRoute(http::verb method, const std::string &path, Handler handler);

        /**
         * @brief Start accepting connections (non-blocking — spawns thread pool internally).
         */
        void start();

        /**
         * @brief Stop the server and join all threads.
         */
        void stop();

        /**
         * @brief Return current runtime status
         *
         * @return current status
         */
        [[nodiscard]]
        RuntimeStatus getStatus() const { return _status.load(); }

        /**
         * @brief Sets the status manually
         *
         * @param s status
         */
        void setStatus(const RuntimeStatus s) { _status.store(s); }

      private:

        /**
         * @brief Channeled logger
         */
        logger_t _logger{boost::log::keywords::channel = "GammaRuntime"};

        /**
         * @brief Routing structure
         */
        struct Route {
            http::verb method;
            std::string path;
            Handler handler;
        };

        /**
         * @brief HTTP request router
         *
         * @param req request structure
         * @return
         */
        Response dispatch(const Request &req) const;

        /**
         * @brief HTTP socket acceptor
         */
        void doAccept();

        /**
         * @brief Boost IO context
         */
        net::io_context _ioc;

        /**
         * @brief HTTP socket acceptor
         */
        net::ip::tcp::acceptor _acceptor;

        /**
         * @brief Thread pool
         */
        std::vector<std::thread> _pool;

        /**
         * @brief HTTP routes
         */
        std::vector<Route> _routes;

        /**
         * @brief Number of threads
         */
        std::size_t _threads;

        /**
         * @brief Runtime status
         */
        std::atomic<RuntimeStatus> _status{RuntimeStatus::idle};
    };
}// namespace Awsmock::GRT