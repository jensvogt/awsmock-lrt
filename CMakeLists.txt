cmake_minimum_required(VERSION 4.2)
project(awsmock-lrt)

set(CMAKE_CXX_STANDARD 23)

# Point to the vcpkg installation that libawsmockcore.a was compiled against.
# Override with -DAWSMOCK_VCPKG_DIR=<path> if the awsmock build tree moves.
set(AWSMOCK_VCPKG_DIR "" CACHE PATH "vcpkg installed dir used by awsmockcore/awsmockdto")
if(NOT AWSMOCK_VCPKG_DIR)
    foreach(_candidate
            "${CMAKE_CURRENT_SOURCE_DIR}/../awsmock/cmake-build-debug/vcpkg_installed/x64-linux"
            "${CMAKE_CURRENT_SOURCE_DIR}/../awsmock/cmake-build-release/vcpkg_installed/x64-linux"
            "${CMAKE_CURRENT_SOURCE_DIR}/../awsmock/build/vcpkg_installed/x64-linux")
        if(EXISTS "${_candidate}/share/boost/BoostConfig.cmake")
            set(AWSMOCK_VCPKG_DIR "${_candidate}")
            message(STATUS "Auto-detected AWSMOCK_VCPKG_DIR: ${AWSMOCK_VCPKG_DIR}")
            break()
        endif()
    endforeach()
endif()
if(EXISTS "${AWSMOCK_VCPKG_DIR}")
    list(PREPEND CMAKE_PREFIX_PATH "${AWSMOCK_VCPKG_DIR}")
endif()

find_package(JNI REQUIRED)
find_package(Boost CONFIG REQUIRED COMPONENTS program_options json log log_setup process url)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)

set(GRT_SOURCES src/main.cpp src/LambdaJvmRuntime.cpp src/LambdaProcessRuntime.cpp src/LambdaNodeRuntime.cpp
        src/LambdaPythonRuntime.cpp src/LambdaCppRuntime.cpp src/HttpServer.cpp src/HttpHandler.cpp
        src/StatusReporter.cpp)
set(GRT_LIBS lib/)

add_executable(awsmock-lrt ${GRT_SOURCES})
target_include_directories(awsmock-lrt PRIVATE include/ ${JNI_INCLUDE_DIRS})
target_link_directories(awsmock-lrt PRIVATE lib)
target_link_libraries(awsmock-lrt PRIVATE ${JNI_LIBRARIES} awsmockcore awsmockdto awsmockdb OpenSSL::SSL OpenSSL::Crypto
        Boost::program_options Boost::json Boost::log Boost::log_setup Boost::program_options Boost::process Boost::url
        Threads::Threads ${CMAKE_DL_LIBS})