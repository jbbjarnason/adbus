cmake_minimum_required(VERSION 3.26)
project(asio_dbus)

set(CMAKE_CXX_STANDARD 23)

find_package(fmt CONFIG REQUIRED)
add_executable(asio_dbus main.cpp)
target_link_libraries(asio_dbus PRIVATE fmt::fmt)
