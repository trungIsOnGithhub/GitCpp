cmake_minimum_required(VERSION 3.13)

project(http-server-starter-cpp)

set(SOURCE_FILES src/server.cpp)

set(CMAKE_CXX_STANDARD 11)

add_executable(server ${SOURCE_FILES})
