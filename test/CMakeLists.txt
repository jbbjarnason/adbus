
CPMAddPackage(
  ut
  GITHUB_REPOSITORY boost-ext/ut
  GIT_TAG        v2.0.1
  GIT_SHALLOW TRUE
)

add_executable(write_test write_test.cpp)
target_link_libraries(write_test PRIVATE adbus::adbus Boost::ut)
add_test(NAME write_test COMMAND write_test)

add_executable(read_test read_test.cpp)
target_link_libraries(read_test PRIVATE adbus::adbus Boost::ut)
add_test(NAME read_test COMMAND read_test)

add_executable(signature_test.cpp signature_test.cpp)
target_link_libraries(signature_test.cpp PRIVATE adbus::adbus)
add_test(NAME signature_test COMMAND signature_test)

add_executable(path_test.cpp path_test.cpp)
target_link_libraries(path_test.cpp PRIVATE adbus::adbus)
add_test(NAME path_test COMMAND path_test)

add_executable(message_header_test message_header_test.cpp)
target_link_libraries(message_header_test PRIVATE adbus::adbus Boost::ut)
add_test(NAME message_header_test COMMAND message_header_test)
