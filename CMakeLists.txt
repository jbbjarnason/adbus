cmake_minimum_required(VERSION 3.26)
project(adbus)

include(FeatureSummary)
option(BUILD_TESTING "Build tests" ON)
add_feature_info(BUILD_TESTING BUILD_TESTING "Build tests")

#set(CMAKE_CXX_STANDARD 26)
add_compile_options(-std=c++2c)
#add_compile_options(-Wall -Wextra -Werror -Wpedantic -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-private-field -Wno-unused-const-variable -Wno-unused-value)

include(cmake/CPM.cmake)

CPMAddPackage(
  glaze
  GITHUB_REPOSITORY stephenberry/glaze
  GIT_TAG        v2.6.9
  GIT_SHALLOW TRUE
)

find_package(fmt CONFIG REQUIRED)
find_package(glaze REQUIRED)

add_library(adbus INTERFACE)
add_library(adbus::adbus ALIAS adbus)
target_include_directories(adbus INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(adbus INTERFACE fmt::fmt glaze::glaze)

add_executable(adbus_example main.cpp)
target_link_libraries(adbus_example PRIVATE adbus::adbus)

if (BUILD_TESTING)
  add_subdirectory(test)
endif()
