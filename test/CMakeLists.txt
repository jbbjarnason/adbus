
CPMAddPackage(
  ut
  GITHUB_REPOSITORY boost-ext/ut
  GIT_TAG        v2.0.1
  GIT_SHALLOW TRUE
)

add_executable(write_test write_test.cpp)
target_link_libraries(write_test PRIVATE fmt::fmt glaze::glaze)