//
// Created by vogje01 on 6/21/26.
//

#include <awsmock/lrt/LambdaProcessRuntime.h>

// C++ includes
#include <chrono>
#include <stdexcept>
#include <string>

// POSIX includes
#include <cerrno>
#include <csignal>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>

namespace Awsmock::Lrt {

    // ── spawn ─────────────────────────────────────────────────────────────────

    void LambdaProcessRuntime::spawn(const std::string &executable,
                                    const std::vector<std::string> &args,
                                    const std::map<std::string, std::string> &envVars) {
        _status.runtimeStatus = RuntimeStatus::starting;
        _status.pid = Core::SystemUtils::GetPid();

        int stdinPipe[2], stdoutPipe[2];
        if (pipe(stdinPipe) < 0 || pipe(stdoutPipe) < 0)
            throw std::runtime_error(std::string("pipe() failed: ") + strerror(errno));

        const pid_t pid = fork();
        if (pid < 0)
            throw std::runtime_error(std::string("fork() failed: ") + strerror(errno));

        if (pid == 0) {
            // ── child ──
            dup2(stdinPipe[0], STDIN_FILENO);
            dup2(stdoutPipe[1], STDOUT_FILENO);
            close(stdinPipe[0]);
            close(stdinPipe[1]);
            close(stdoutPipe[0]);
            close(stdoutPipe[1]);

            for (const auto &[k, v]: envVars)
                setenv(k.c_str(), v.c_str(), 1);

            // Build argv
            std::vector<const char *> argv;
            argv.push_back(executable.c_str());
            for (const auto &a: args)
                argv.push_back(a.c_str());
            argv.push_back(nullptr);

            execvp(executable.c_str(), const_cast<char *const *>(argv.data()));
            // exec failed — write error to stderr and exit
            const std::string msg = "execvp(" + executable + ") failed: " + strerror(errno) + "\n";
            if (const ssize_t written=write(STDERR_FILENO, msg.data(), msg.size()); written < 0) {
                log_error << "write to stderr failed: " << strerror(errno);
            }
            _exit(1);
        }

        // ── parent ──
        close(stdinPipe[0]);
        close(stdoutPipe[1]);
        _pid = pid;
        _stdinFd = stdinPipe[1];
        _stdoutFd = stdoutPipe[0];
        _status.runtimeStatus = RuntimeStatus::idle;
        log_info << "Subprocess spawned, pid: " << _pid << ", executable: " << executable;
    }

    // ── invoke ────────────────────────────────────────────────────────────────

    std::string LambdaProcessRuntime::invoke(const std::string &eventJson) {
        std::lock_guard lock(_invokeMtx);
        _status.runtimeStatus = RuntimeStatus::running;

        const auto t0 = std::chrono::steady_clock::now();

        const std::string line = eventJson + "\n";
        const ssize_t written = write(_stdinFd, line.data(), line.size());
        if (written < 0)
            throw std::runtime_error(std::string("write to subprocess stdin failed: ") + strerror(errno));

        const std::string result = readLine();

        const double elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        _status.invocations++;
        _status.avgDuration += (elapsed - _status.avgDuration) / _status.invocations;
        _status.runtimeStatus = RuntimeStatus::idle;
        return result;
    }

    // ── readLine ──────────────────────────────────────────────────────────────

    std::string LambdaProcessRuntime::readLine() const {
        std::string result;
        char c;
        while (true) {
            ssize_t n;
            do { n = read(_stdoutFd, &c, 1); } while (n == -1 && errno == EINTR);
            if (n == 0)
                throw std::runtime_error("subprocess exited unexpectedly");
            if (n < 0)
                throw std::runtime_error(std::string("read from subprocess stdout failed: ") + strerror(errno));
            if (c == '\n')
                return result;
            result += c;
        }
    }

    // ── shutdown / destructor ─────────────────────────────────────────────────

    void LambdaProcessRuntime::shutdown() {
        killChild();
    }

    LambdaProcessRuntime::~LambdaProcessRuntime() {
        killChild();
    }

    void LambdaProcessRuntime::killChild() {
        if (_stdinFd >= 0) { close(_stdinFd); _stdinFd = -1; }
        if (_stdoutFd >= 0) { close(_stdoutFd); _stdoutFd = -1; }

        if (_pid > 0) {
            kill(_pid, SIGTERM);
            // Give the child up to 3 seconds to exit cleanly, then SIGKILL.
            for (int i = 0; i < 30; ++i) {
                int wstatus;
                const pid_t r = waitpid(_pid, &wstatus, WNOHANG);
                if (r == _pid) { _pid = -1; return; }
                usleep(100'000);
            }
            kill(_pid, SIGKILL);
            waitpid(_pid, nullptr, 0);
            _pid = -1;
        }
    }

}// namespace Awsmock::GRT
