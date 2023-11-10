cmake_minimum_required(VERSION 3.26)
project(adbus)

set(CMAKE_CXX_STANDARD 23)

find_package(fmt CONFIG REQUIRED)
add_executable(adbus main.cpp)
target_link_libraries(adbus PRIVATE fmt::fmt)
target_include_directories(adbus PRIVATE ${CMAKE_CURRENT_LIST_DIR})
