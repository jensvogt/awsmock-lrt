//
// Created by vogje01 on 6/21/26.
//

#include <awsmock/lrt/LambdaPythonRuntime.h>

// C++ includes
#include <chrono>
#include <fstream>
#include <stdexcept>

// POSIX includes
#include <unistd.h>

// Awsmock includes
#include <awsmock/lrt/StatusReporter.h>

namespace Awsmock::Lrt {

    // Inline shim: sys.argv[1]=codePath  sys.argv[2]=handler ("module.fn")
    static constexpr const char *PYTHON_SHIM = R"PY(
import sys, json, importlib.util, os

code_path   = sys.argv[1]
handler_expr = sys.argv[2]
dot          = handler_expr.rfind('.')
if dot < 0:
    sys.stderr.write('Invalid handler: ' + handler_expr + '\n')
    sys.exit(1)

module_name = handler_expr[:dot]
fn_name     = handler_expr[dot + 1:]

sys.path.insert(0, code_path)
module_file = os.path.join(code_path, module_name + '.py')
spec = importlib.util.spec_from_file_location(module_name, module_file)
mod  = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
handler_fn = getattr(mod, fn_name)

for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        event  = json.loads(line)
        result = handler_fn(event, None)
        sys.stdout.write(json.dumps(result) + '\n')
        sys.stdout.flush()
    except Exception as e:
        sys.stdout.write(json.dumps({'error': str(e)}) + '\n')
        sys.stdout.flush()
)PY";

    LambdaPythonRuntime::LambdaPythonRuntime(const std::string &codePath,const std::string &handler,const std::map<std::string, std::string> &envVars,const std::string &pythonExecutable) {
        _shimPath = "/tmp/awsmock-grt-python-" + std::to_string(getpid()) + ".py";
        writeShim();

        StatusReporter::instance().setRuntime(*this);
        _status.runtimeStatus = RuntimeStatus::starting;
        _status.pid = Core::SystemUtils::GetPid();
        StatusReporter::instance().reportStatus();

        spawn(pythonExecutable, {_shimPath, codePath, handler}, envVars);

        _status.lastStart = std::chrono::system_clock::now();
        StatusReporter::instance().reportStatus();

        log_info << "Python runtime started, handler: " << handler << ", code: " << codePath;
    }

    LambdaPythonRuntime::~LambdaPythonRuntime() {
        _status.runtimeStatus = RuntimeStatus::stopped;
        _status.lastStop = std::chrono::system_clock::now();
        StatusReporter::instance().reportStatus();

        if (!_shimPath.empty())
            ::unlink(_shimPath.c_str());
    }

    void LambdaPythonRuntime::writeShim() const {
        std::ofstream f(_shimPath);
        if (!f)
            throw std::runtime_error("Cannot write Python shim to " + _shimPath);
        f << PYTHON_SHIM;
    }

}// namespace Awsmock::GRT
