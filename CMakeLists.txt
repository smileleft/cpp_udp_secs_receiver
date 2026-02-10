cmake_minimum_required(VERSION 3.20)
project(secs-udp-receiver VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Release 빌드 최적화
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG -flto")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")

# ══════════════════════════════════════════════════════════
# Dependencies
# ══════════════════════════════════════════════════════════
find_package(Threads REQUIRED)
find_package(Boost 1.74 REQUIRED COMPONENTS system)

# PostgreSQL libpqxx
find_path(PQXX_INCLUDE_DIR pqxx/pqxx REQUIRED)
find_library(PQXX_LIB pqxx REQUIRED)
find_library(PQ_LIB pq REQUIRED)

# spdlog
find_package(spdlog REQUIRED)

# nlohmann/json (헤더 온리)
include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

# ══════════════════════════════════════════════════════════
# Executable
# ══════════════════════════════════════════════════════════
add_executable(secs-receiver
    src/main.cpp
    src/config.cpp
    src/udp_receiver.cpp
    src/parser.cpp
    src/db_writer.cpp
    src/worker_pool.cpp
)

target_include_directories(secs-receiver PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
    ${PQXX_INCLUDE_DIR}
)

target_link_libraries(secs-receiver PRIVATE
    Threads::Threads
    Boost::system
    ${PQXX_LIB}
    ${PQ_LIB}
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

# ══════════════════════════════════════════════════════════
# Install
# ══════════════════════════════════════════════════════════
install(TARGETS secs-receiver DESTINATION bin)
