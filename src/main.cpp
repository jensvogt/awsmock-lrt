// C++ includes
#include <cerrno>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// Boost includes
#include <boost/program_options.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

// Project includes
#include <awsmock/core/config/Configuration.h>
#include <awsmock/core/scheduler/Scheduler.h>
#include <awsmock/lrt/HttpHandler.h>
#include <awsmock/lrt/HttpServer.h>
#include <awsmock/lrt/LambdaRuntimeFactory.h>
#include <awsmock/lrt/StatusReporter.h>

namespace po = boost::program_options;

namespace {
    logger_t _logger{boost::log::keywords::channel = "LambdaRuntime"};
}

#define DEFAULT_CONFIG_FILE "/usr/local/awsmock/etc/awsmock.json"
#define DEFAULT_LOG_FILE "/usr/local/awsmock/log/awsmock-lrt.log"
#define DEFAULT_LOG_LEVEL "debug"
#define DEFAULT_RUNTIME "java21"


// Parse "KEY=VALUE" strings into a map.
static std::map<std::string, std::string> parseEnvVars(const std::vector<std::string> &raw) {
    std::map<std::string, std::string> result;
    for (const auto &kv: raw) {
        const auto eq = kv.find('=');
        if (eq == std::string::npos)
            throw po::invalid_option_value("--env must be KEY=VALUE, got: " + kv);
        result.emplace(kv.substr(0, eq), kv.substr(eq + 1));
    }
    return result;
}

int main(const int argc, char *argv[]) {

    std::string codePath;
    std::string handler;
    std::string functionName;
    std::string address = "0.0.0.0";
    std::string runtime = DEFAULT_RUNTIME;
    std::string configFile = DEFAULT_CONFIG_FILE;
    std::string logLevel = DEFAULT_LOG_LEVEL;
    std::string runtimeLibDir = "/usr/local/awsmock/lib/java";
    std::string nodeExecutable = "node";
    std::string pythonExecutable = "python3";
    std::string managerHost = "host.docker.internal";
    std::string instanceId = Awsmock::Core::StringUtils::GenerateRandomAlphanumericString(12);

    int port = 8080;
    int lifetime = 0;
    int managerPort = 4566;
    int reportPeriod = 30;
    std::vector<std::string> rawEnv;
    std::vector<std::string> jvmArgs;

    po::options_description desc("awsmock-lrt options", 256);
    // clang-format off
    desc.add_options()("help,h", "Show help message")
    ("code-path,c",        po::value<std::string>(&codePath)->required(),                              "Path to the Lambda code artifact (JAR, .so, or source directory)")
    ("handler,H",          po::value<std::string>(&handler)->required(),                               "Handler identifier (format depends on runtime)")
    ("function-name,f",    po::value<std::string>(&functionName)->default_value(""),                "Lambda function name")
    ("address,a",          po::value<std::string>(&address)->default_value(address),                   "Bind address (default: 0.0.0.0)")
    ("port,p",             po::value<int>(&port)->default_value(port),                                 "HTTP port to listen on (default: 8080)")
    ("lifetime,l",         po::value<int>(&lifetime)->default_value(lifetime),                         "Runtime lifetime in seconds (0 = run forever)")
    ("env,e",              po::value<std::vector<std::string>>(&rawEnv)->composing(),                  "Environment variable: KEY=VALUE (repeatable)")
    ("jvm-arg,J",          po::value<std::vector<std::string>>(&jvmArgs)->composing(),                 "Extra JVM option, e.g. -Xmx512m (repeatable, Java only)")
    ("runtime,r",          po::value<std::string>(&runtime)->default_value(runtime),                   "Runtime identifier: java21, nodejs20.x, python3.12, provided …")
    ("runtime-lib-dir,R",  po::value<std::string>(&runtimeLibDir)->default_value(runtimeLibDir),       "Directory with AWS Lambda runtime JARs (Java only)")
    ("node-executable,n",  po::value<std::string>(&nodeExecutable)->default_value(nodeExecutable),     "Node.js binary name or path (default: node)")
    ("python-executable,y",po::value<std::string>(&pythonExecutable)->default_value(pythonExecutable), "Python binary name or path (default: python3)")
    ("manager-host,m",     po::value<std::string>(&managerHost)->default_value(managerHost),           "Hostname of the manager (default: host.docker.internal)")
    ("manager-port,P",     po::value<int>(&managerPort)->default_value(managerPort),                   "Manager port (default: 4566)")
    ("report-period,M",    po::value<int>(&reportPeriod)->default_value(reportPeriod),                 "Manager report period (default: 30)")
    ("instance-id,i",      po::value<std::string>(&instanceId)->default_value(instanceId),             "Instance ID (default: random UUID)")
    ("config,C",           po::value<std::string>(&configFile)->default_value(configFile),             "Configuration file name")
    ("loglevel,L",         po::value<std::string>(&logLevel)->default_value(logLevel),                 "Logging level");
    // clang-format on

    try {
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.contains("help")) {
            std::cout << desc << '\n';
            return 0;
        }
        po::notify(vm);

    } catch (const po::error &e) {
        std::cerr << "Error: " << e.what() << '\n'
                << desc << '\n';
        return 1;
    }

    const auto envVars = parseEnvVars(rawEnv);

    // Collect runtime JARs from the runtime lib directory (Java only)
    std::vector<std::string> runtimeJars;
    if (std::filesystem::is_directory(runtimeLibDir)) {
        for (const auto &entry: std::filesystem::directory_iterator(runtimeLibDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".jar")
                runtimeJars.push_back(entry.path().string());
        }
    }

    std::cout
            << "code-path  : " << codePath << '\n'
            << "name       : " << functionName << '\n'
            << "handler    : " << handler << '\n'
            << "runtime    : " << runtime << '\n'
            << "port       : " << port << '\n'
            << "lifetime   : " << lifetime << '\n'
            << "config     : " << configFile << '\n'
            << "loglevel   : " << logLevel << '\n'
            << "manager    : " << managerHost << ":" << managerPort << '\n'
            << "period     : " << reportPeriod << '\n'
            << "instanceId : " << instanceId << '\n';
    for (const auto &[k, v]: envVars) std::cout << "  env     : " << k << '=' << v << '\n';
    for (const auto &a: jvmArgs) std::cout << "  jvm     : " << a << '\n';
    for (const auto &j: runtimeJars) std::cout << "  rt-jar  : " << j << '\n';

    // Block SIGINT/SIGTERM before spawning any threads, so every thread inherits the mask.
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);

    // Load configuration
    Awsmock::Core::Configuration::instance().load(configFile);

    // Create the runtime via factory
    Awsmock::Lrt::RuntimeParams params;
    params.codePath = codePath;
    params.handler = handler;
    params.envVars = envVars;
    params.jvmArgs = jvmArgs;
    params.runtimeJars = runtimeJars;
    params.nodeExecutable = nodeExecutable;
    params.pythonExecutable = pythonExecutable;
    params.managerHost = managerHost;
    params.managerPort = managerPort;
    params.reportPeriod = reportPeriod;

    // Phase 1: StatusReporter up before JVM/runtime start, so "starting" status
    // can be reported during the (potentially slow) runtime initialization.
    Awsmock::Lrt::StatusReporter::initialize(functionName, instanceId, port, managerHost, managerPort);

    std::unique_ptr<Awsmock::Lrt::ILambdaRuntime> runtime_ptr;
    try {
        runtime_ptr = Awsmock::Lrt::LambdaRuntimeFactory::create(runtime, params);
        Awsmock::Lrt::StatusReporter::instance().setRuntime(*runtime_ptr);// Phase 2
    } catch (const std::exception &ex) {
        std::cerr << "Failed to create runtime '" << runtime << "': " << ex.what() << '\n';
        return 1;
    }

    // Config
    Awsmock::Dto::Lambda::LambdaConfig lambdaConfig;
    lambdaConfig.port = port;
    lambdaConfig.runtime = runtime;

    // Start HTTP server
    Awsmock::Lrt::HttpServer server(address, static_cast<unsigned short>(port), 4);
    Awsmock::Lrt::HttpHandler httpHandler(server, *runtime_ptr, lambdaConfig);
    httpHandler.registerRoutes();
    server.start();
    log_info << "Server started. Waiting for invocations.";

    boost::asio::io_context schedulerIoc;
    auto workGuard = boost::asio::make_work_guard(schedulerIoc);
    Awsmock::Core::Scheduler::initialize(schedulerIoc);
    Awsmock::Core::Scheduler::instance().AddTask("grt-status-reporter", [] { Awsmock::Lrt::StatusReporter::instance().reportStatus(); }, reportPeriod, reportPeriod);
    std::thread schedulerThread([&] { schedulerIoc.run(); });

    if (lifetime > 0) {
        timespec ts{lifetime, 0};
        if (const int sig = sigtimedwait(&mask, nullptr, &ts); sig < 0 && errno == EAGAIN) {
            log_info << "Lifetime of " << lifetime << " seconds expired, shutting down";
        } else if (sig > 0) {
            log_info << "Received signal " << sig << ", shutting down";
        }
    } else {
        std::cout << "Press Ctrl+C to stop.\n";
        int signum = 0;
        sigwait(&mask, &signum);
        std::cout << "\nReceived signal " << signum << ", shutting down...\n";
    }

    log_info << "Initiating graceful shutdown";
    Awsmock::Core::Scheduler::instance().Shutdown();
    workGuard.reset();
    schedulerIoc.stop();
    if (schedulerThread.joinable()) {
        schedulerThread.join();
    }
    server.setStatus(RuntimeStatus::stopped);
    server.stop();
    runtime_ptr->shutdown();
    Awsmock::Lrt::StatusReporter::instance().reportStatus();

    return 0;
}
