//
// Created by vogje01 on 6/18/26.
//

#include <awsmock/lrt/HttpServer.h>

// ── per-connection session ────────────────────────────────────────────────────

namespace Awsmock::Lrt {

    void runSession(beast::tcp_stream stream, const std::function<Response(const Request &)> &dispatch) {
        logger_t _logger{boost::log::keywords::channel = "GammaRuntime"};
        beast::flat_buffer buffer;
        beast::error_code ec;

        for (;;) {
            stream.expires_after(std::chrono::seconds(30));

            Request req;
            http::read(stream, buffer, req, ec);
            if (ec == http::error::end_of_stream) break;
            if (ec) {
                std::cerr << "read: " << ec.message() << '\n';
                break;
            }

            const bool keepAlive = req.keep_alive();
            Response res = dispatch(req);
            res.keep_alive(keepAlive);
            res.prepare_payload();

            http::write(stream, res, ec);
            if (ec) {
                std::cerr << "write: " << ec.message() << '\n';
                break;
            }
            if (!keepAlive) break;
        }

        ec = stream.socket().shutdown(net::ip::tcp::socket::shutdown_send, ec);
        log_debug << "shutdown: " << ec.message() << '\n';
    }


    // ── HttpServer ────────────────────────────────────────────────────────────────

    HttpServer::HttpServer(const std::string &address, unsigned short port, const std::size_t threads) : _ioc(static_cast<int>(threads)), _acceptor(_ioc, {net::ip::make_address(address), port}), _threads(threads) {
        _acceptor.set_option(net::socket_base::reuse_address(true));
    }

    HttpServer::~HttpServer() {
        stop();
    }

    void HttpServer::addRoute(const http::verb method, const std::string &path, Handler handler) {
        _routes.push_back({method, path, std::move(handler)});
    }

    Response HttpServer::dispatch(const Request &req) const {
        for (const auto &[method, path, handler]: _routes) {
            if (method == req.method() &&
                req.target().starts_with(path)) {
                return handler(req);
            }
        }
        Response res{http::status::not_found, req.version()};
        res.set(http::field::content_type, "text/plain");
        res.body() = "Not Found";
        return res;
    }

    void HttpServer::doAccept() {
        _acceptor.async_accept([this](const beast::error_code &ec, net::ip::tcp::socket sock) {
            if (ec == net::error::operation_aborted) return;
            if (!ec) {
                net::post(_ioc, [stream = beast::tcp_stream(std::move(sock)), this]() mutable {
                    runSession(std::move(stream), [this](const Request &r) { return dispatch(r); });
                });
            } else {
                std::cerr << "accept: " << ec.message() << '\n';
            }
            doAccept();
        });
    }

    void HttpServer::start() {
        doAccept();
        for (std::size_t i = 0; i < _threads; ++i) {
            _pool.emplace_back([this] { _ioc.run(); });
        }
        std::cout << "Listening on port " << _acceptor.local_endpoint().port() << " (" << _threads << " threads)\n";
    }

    void HttpServer::stop() {
        _ioc.stop();
        for (auto &t: _pool) {
            if (t.joinable()) t.join();
        }
        _pool.clear();
    }
}// namespace Awsmock::GRT
