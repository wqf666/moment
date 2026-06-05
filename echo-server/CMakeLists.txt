cmake_minimum_required(VERSION 3.16)

project(echo-server)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
find_package(Drogon CONFIG REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

file(GLOB_RECURSE SERVER_SOURCES
    src/*.cpp
)

add_executable(echo-server ${SERVER_SOURCES})

target_include_directories(echo-server PRIVATE
    include
    src
    ${JSONCPP_INCLUDE_DIRS}
)

target_link_libraries(echo-server PRIVATE
    Drogon::Drogon
    ${JSONCPP_LIBRARIES}
)