//
// Created by vogje01 on 6/21/26.
//

#include <awsmock/lrt/LambdaNodeRuntime.h>

// C++ includes
#include <chrono>
#include <fstream>
#include <stdexcept>

// POSIX includes
#include <unistd.h>

// Awsmock includes
#include <awsmock/lrt/StatusReporter.h>

namespace Awsmock::Lrt {

    // Inline shim: argv[2]=codePath argv[3]=handler ("file.fn")
    static constexpr auto NODE_SHIM = R"JS(
'use strict';
const path = require('path');

// Redirect all console output to stderr so only the handler return value goes to stdout.
// The real Lambda runtime sends console.log to CloudWatch Logs, not the invocation response.
const stderrWrite = (...args) => process.stderr.write(args.map(String).join(' ') + '\n');
console.log   = stderrWrite;
console.info  = stderrWrite;
console.warn  = stderrWrite;
console.error = stderrWrite;
console.debug = stderrWrite;

const codePath = process.argv[2];
const handlerExpr = process.argv[3];
const dot = handlerExpr.lastIndexOf('.');
if (dot < 0) { process.stderr.write('Invalid handler: ' + handlerExpr + '\n'); process.exit(1); }
const moduleName = handlerExpr.slice(0, dot);
const fnName = handlerExpr.slice(dot + 1);
const mod = require(path.resolve(codePath, moduleName));
const handlerFn = mod[fnName];
if (typeof handlerFn !== 'function') {
    process.stderr.write('Handler not found: ' + handlerExpr + '\n');
    process.exit(1);
}
process.stdin.setEncoding('utf8');
let buf = '';
process.stdin.on('data', chunk => {
    buf += chunk;
    let nl;
    while ((nl = buf.indexOf('\n')) !== -1) {
        const line = buf.slice(0, nl);
        buf = buf.slice(nl + 1);
        if (!line.trim()) continue;
        let event;
        try { event = JSON.parse(line); } catch(e) {
            process.stdout.write(JSON.stringify({ error: 'parse error: ' + e.message }) + '\n');
            continue;
        }
        Promise.resolve()
            .then(() => handlerFn(event, {}))
            .then(r  => process.stdout.write(JSON.stringify(r !== undefined ? r : null) + '\n'))
            .catch(e => process.stdout.write(JSON.stringify({ error: e.message || String(e) }) + '\n'));
    }
});
process.stdin.on('end', () => process.exit(0));
)JS";

    LambdaNodeRuntime::LambdaNodeRuntime(const std::string &codePath,const std::string &handler,const std::map<std::string, std::string> &envVars,const std::string &nodeExecutable) {
        _shimPath = "/tmp/awsmock-lrt-node-" + std::to_string(getpid()) + ".js";
        writeShim(codePath, handler);

        StatusReporter::instance().setRuntime(*this);
        _status.runtimeStatus = RuntimeStatus::starting;
        _status.pid = Core::SystemUtils::GetPid();
        StatusReporter::instance().reportStatus();

        spawn(nodeExecutable, {_shimPath, codePath, handler}, envVars);

        _status.lastStart = std::chrono::system_clock::now();
        StatusReporter::instance().reportStatus();

        log_info << "Node.js runtime started, handler: " << handler << ", code: " << codePath;
    }

    LambdaNodeRuntime::~LambdaNodeRuntime() {
        _status.runtimeStatus = RuntimeStatus::stopped;
        _status.lastStop = std::chrono::system_clock::now();
        StatusReporter::instance().reportStatus();

        if (!_shimPath.empty())
            unlink(_shimPath.c_str());
    }

    void LambdaNodeRuntime::writeShim(const std::string & /*codePath*/, const std::string & /*handler*/) const {
        std::ofstream f(_shimPath);
        if (!f)
            throw std::runtime_error("Cannot write Node.js shim to " + _shimPath);
        f << NODE_SHIM;
    }

}// namespace Awsmock::GRT
