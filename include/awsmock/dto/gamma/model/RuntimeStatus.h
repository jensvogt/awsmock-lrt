//
// Created by vogje01 on 6/18/26.
//

#pragma once

// C++ includes
#include <stdexcept>
#include <string>

enum class RuntimeStatus {
    starting,
    idle,
    running,
    failed,
    stopped,
};

inline std::string runtimeStatusToString(const RuntimeStatus s) {
    switch (s) {
        case RuntimeStatus::starting:
            return "starting";
        case RuntimeStatus::idle:
            return "idle";
        case RuntimeStatus::running:
            return "running";
        case RuntimeStatus::failed:
            return "failed";
        case RuntimeStatus::stopped:
            return "stopped";
    }
    throw std::invalid_argument("unknown RuntimeStatus");
}

inline RuntimeStatus runtimeStatusFromString(const std::string &s) {
    if (s == "starting") return RuntimeStatus::starting;
    if (s == "idle") return RuntimeStatus::idle;
    if (s == "running") return RuntimeStatus::running;
    if (s == "failed") return RuntimeStatus::failed;
    if (s == "stopped") return RuntimeStatus::stopped;
    throw std::invalid_argument("unknown RuntimeStatus: " + s);
}
