//
// Created by vogje01 on 6/20/26.
//

#include <awsmock/lrt/HttpHandler.h>

namespace Awsmock::Lrt {

    HttpHandler::HttpHandler(HttpServer &server, ILambdaRuntime &runtime, const Dto::Lambda::LambdaConfig &config) : _server(server), _runtime(runtime), _config(config) {}

    void HttpHandler::registerRoutes() {
        _server.addRoute(http::verb::get, "/", [this](const Request &req) { return handleGet(req); });
        _server.addRoute(http::verb::post, "/", [this](const Request &req) { return handlePost(req); });
        _server.addRoute(http::verb::put, "/", [this](const Request &req) { return handlePut(req); });
    }

    // ── verb dispatchers ──────────────────────────────────────────────────────

    Response HttpHandler::handleGet(const Request &req) const {
        const auto target = req.target();
        if (target == "/status" || target.starts_with("/status?")) return handleStatus(req);
        if (target == "/config" || target.starts_with("/config?")) return handleConfig(req);
        return notFound(req);
    }

    Response HttpHandler::handlePost(const Request &req) {
        const auto target = req.target();
        if (target == "/stop" || target.starts_with("/stop?")) return handleStop(req);
        if (target == "/invoke" ||
            target.starts_with("/invoke/") ||
            target.starts_with("/2015-03-31/functions/"))
            return handleInvoke(req);
        return notFound(req);
    }

    Response HttpHandler::handlePut(const Request &req) {
        log_debug << "PUT " << req.target();
        return notFound(req);
    }

    // ── route implementations ─────────────────────────────────────────────────

    Response HttpHandler::handleStatus(const Request &req) const {
        Response res{http::status::ok, req.version()};
        res.set(http::field::content_type, "application/json");
        res.body() = _runtime.getStatus().ToJson();
        return res;
    }

    Response HttpHandler::handleConfig(const Request &req) const {
        Response res{http::status::ok, req.version()};
        res.set(http::field::content_type, "application/json");
        res.body() = _config.ToJson();
        return res;
    }

    Response HttpHandler::handleInvoke(const Request &req) const {
        _server.setStatus(RuntimeStatus::running);
        Response res{http::status::ok, req.version()};
        res.set(http::field::content_type, "application/json");
        try {
            res.body() = _runtime.invoke(req.body());
            res.result(http::status::ok);
            _server.setStatus(RuntimeStatus::idle);
        } catch (const std::exception &ex) {
            res.body() = std::string(R"({"error":")") + ex.what() + R"("})";
            res.result(http::status::internal_server_error);
            _server.setStatus(RuntimeStatus::failed);
        }
        return res;
    }

    Response HttpHandler::handleStop(const Request &req) {
        log_info << "Stop requested via HTTP — sending SIGTERM";
        _server.setStatus(RuntimeStatus::stopped);
        // Notify the manager before the process starts tearing down.  For JVM
        // runtimes System.exit() terminates the process during shutdown(), so
        // the normal post-shutdown reportStatus() in main() is never reached.
        StatusReporter::instance().reportStopped();
        Response res{http::status::accepted, req.version()};
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"message":"shutting down"})";
        // SIGTERM is blocked on all threads; kill() merely queues it so the
        // response is delivered before main()'s sigwait() picks it up.
        kill(getpid(), SIGTERM);
        return res;
    }

    Response HttpHandler::notFound(const Request &req) {
        Response res{http::status::not_found, req.version()};
        res.set(http::field::content_type, "text/plain");
        res.body() = "Not Found";
        return res;
    }

    Response HttpHandler::methodNotAllowed(const Request &req) {
        Response res{http::status::method_not_allowed, req.version()};
        res.set(http::field::content_type, "text/plain");
        res.body() = "Method Not Allowed";
        return res;
    }

}// namespace Awsmock::GRT
