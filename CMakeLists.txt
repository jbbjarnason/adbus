cmake_minimum_required(VERSION 3.26)
project(adbus)

set(CMAKE_CXX_STANDARD 23)

include(cmake/CPM.cmake)

CPMAddPackage(
  glaze
  GITHUB_REPOSITORY stephenberry/glaze
  GIT_TAG        v2.6.9
  GIT_SHALLOW TRUE
)

find_package(fmt CONFIG REQUIRED)
add_executable(adbus main.cpp)
target_link_libraries(adbus PRIVATE fmt::fmt glaze::glaze)
target_include_directories(adbus PRIVATE ${CMAKE_CURRENT_LIST_DIR})
