cmake_minimum_required(VERSION 4.2)
project(awsmocklrt)

set(CMAKE_CXX_STANDARD 23)
set(BINARY awsmocklrt)

# Point to the vcpkg installation that libawsmockcore.a was compiled against.
# Override with -DAWSMOCK_VCPKG_DIR=<path> if the awsmock build tree moves.
set(AWSMOCK_VCPKG_DIR "" CACHE PATH "vcpkg installed dir used by awsmockcore/awsmockdto")
if(NOT AWSMOCK_VCPKG_DIR)
    get_filename_component(_awsmock_root "${CMAKE_CURRENT_SOURCE_DIR}/../awsmock" ABSOLUTE)
    foreach(_triplet "x64-linux" "x64-windows" "x64-windows-static")
        foreach(_build "cmake-build-debug" "cmake-build-release" "build")
            set(_candidate "${_awsmock_root}/${_build}/vcpkg_installed/${_triplet}")
            if(EXISTS "${_candidate}/include/openssl/ssl.h")
                set(AWSMOCK_VCPKG_DIR "${_candidate}")
                message(STATUS "Auto-detected AWSMOCK_VCPKG_DIR: ${AWSMOCK_VCPKG_DIR}")
                break()
            endif()
        endforeach()
        if(AWSMOCK_VCPKG_DIR)
            break()
        endif()
    endforeach()
endif()
if(AWSMOCK_VCPKG_DIR)
    list(PREPEND CMAKE_PREFIX_PATH "${AWSMOCK_VCPKG_DIR}")
    if(NOT OPENSSL_ROOT_DIR)
        set(OPENSSL_ROOT_DIR "${AWSMOCK_VCPKG_DIR}" CACHE PATH "OpenSSL root inferred from AWSMOCK_VCPKG_DIR")
        message(STATUS "Setting OPENSSL_ROOT_DIR: ${OPENSSL_ROOT_DIR}")
    endif()
else()
    message(WARNING "AWSMOCK_VCPKG_DIR not found — set -DAWSMOCK_VCPKG_DIR=<path> manually")
endif()

# On Windows, Boost may live in a separate vcpkg from the one awsmock was built against.
if(WIN32)
    get_filename_component(_awsmock_root "${CMAKE_CURRENT_SOURCE_DIR}/../awsmock" ABSOLUTE)
    foreach(_boost_candidate
            "${_awsmock_root}/cmake-build-debug/vcpkg_installed/x64-windows"
            "${_awsmock_root}/cmake-build-release/vcpkg_installed/x64-windows"
            "${_awsmock_root}/build/vcpkg_installed/x64-windows"
            "$ENV{VCPKG_ROOT}/installed/x64-windows"
            "$ENV{VCPKG_ROOT}/installed/x64-windows-static"
            "C:/work/private/extern/vcpkg/installed/x64-windows"
            "C:/work/private/extern/vcpkg/installed/x64-windows-static")
        if(EXISTS "${_boost_candidate}/share/boost/BoostConfig.cmake")
            list(APPEND CMAKE_PREFIX_PATH "${_boost_candidate}")
            message(STATUS "Auto-detected Boost vcpkg: ${_boost_candidate}")
            break()
        endif()
    endforeach()
endif()

find_package(JNI REQUIRED)
find_package(Boost CONFIG REQUIRED COMPONENTS program_options json log log_setup process url)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)

set(GRT_SOURCES src/main.cpp src/LambdaJvmRuntime.cpp src/LambdaProcessRuntime.cpp src/LambdaNodeRuntime.cpp
        src/LambdaPythonRuntime.cpp src/LambdaCppRuntime.cpp src/HttpServer.cpp src/HttpHandler.cpp
        src/StatusReporter.cpp)
set(GRT_LIBS lib/)

add_executable(${BINARY} ${GRT_SOURCES})
target_include_directories(${BINARY} PRIVATE include/ ${JNI_INCLUDE_DIRS})
target_link_directories(${BINARY} PRIVATE lib)
target_link_libraries(${BINARY} PRIVATE ${JNI_LIBRARIES} awsmockcore awsmockdto awsmockdb OpenSSL::SSL OpenSSL::Crypto
        Boost::program_options Boost::json Boost::log Boost::log_setup Boost::program_options Boost::process Boost::url
        Threads::Threads ${CMAKE_DL_LIBS})