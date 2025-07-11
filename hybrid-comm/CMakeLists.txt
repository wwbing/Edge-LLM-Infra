cmake_minimum_required(VERSION 3.10)
project(hybrid-comm)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)

include_directories(include)

add_library(hybrid-comm src/pzmq_data.cpp)