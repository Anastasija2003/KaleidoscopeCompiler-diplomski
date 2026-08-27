include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.7.1)
FetchContent_MakeAvailable(Catch2)

add_executable(kaleidoscope_tests
  lexer_test.cpp
  ${CMAKE_SOURCE_DIR}/src/Lexer.cpp)
target_include_directories(kaleidoscope_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(kaleidoscope_tests PRIVATE Catch2::Catch2WithMain)

add_test(NAME kaleidoscope_tests COMMAND kaleidoscope_tests)
