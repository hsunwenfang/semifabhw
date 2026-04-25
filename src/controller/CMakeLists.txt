cmake_minimum_required(VERSION 3.16)
project(cvd_controller CXX)
set(CMAKE_CXX_STANDARD 17)

add_executable(cvd_controller main.cpp)
target_include_directories(cvd_controller PRIVATE ${CMAKE_SOURCE_DIR}/../common)
