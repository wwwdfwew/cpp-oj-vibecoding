cmake_minimum_required(VERSION 3.16)
project(oj_server CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wno-unused-parameter)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(MYSQL REQUIRED IMPORTED_TARGET mysqlclient)

find_path(HTTPLIB_INCLUDE_DIR
    NAMES httplib.h
    PATHS /usr/include /usr/local/include
)
if(NOT HTTPLIB_INCLUDE_DIR)
    message(FATAL_ERROR "cpp-httplib not found. Expected /usr/include/httplib.h")
endif()

set(OJ_SOURCES
    src/main.cpp
    src/util/strutil.cpp
    src/util/json.cpp
    src/util/log.cpp
    src/util/config.cpp
    src/auth/password.cpp
    src/auth/session.cpp
    src/db/mysql_client.cpp
    src/judge/tempdir.cpp
    src/judge/compiler.cpp
    src/judge/runner.cpp
    src/judge/diff.cpp
    src/judge/judge.cpp
    src/routes/auth.cpp
    src/routes/problems.cpp
    src/routes/submit.cpp
    src/routes/admin.cpp
    src/routes/static_files.cpp
)

add_executable(oj_server ${OJ_SOURCES})

target_include_directories(oj_server PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${HTTPLIB_INCLUDE_DIR}
)

target_link_libraries(oj_server PRIVATE
    Threads::Threads
    OpenSSL::Crypto
    PkgConfig::MYSQL
)

target_compile_definitions(oj_server PRIVATE
    CPPHTTPLIB_THREAD_POOL_COUNT=6
)

add_executable(seed_admin scripts/seed_admin.cpp)
target_link_libraries(seed_admin PRIVATE OpenSSL::Crypto)
