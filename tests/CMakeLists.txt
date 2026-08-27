include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.7.1)
FetchContent_MakeAvailable(Catch2)

# Deliberately links only against Lexer.cpp, not the rest of the compiler --
# no LLVM needed here, same "fast, dependency-free" philosophy as the `lex`
# target in the root CMakeLists.txt. Tests that need CodeGenContext (once
# Faza 1/2 add hashing and codegen-level tests) will need LLVM linked in;
# this target stays lexer/parser/AST-only for as long as possible.
add_executable(kaleidoscope_tests
  lexer_test.cpp
  ${CMAKE_SOURCE_DIR}/src/Lexer.cpp)
target_include_directories(kaleidoscope_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(kaleidoscope_tests PRIVATE Catch2::Catch2WithMain)

add_test(NAME kaleidoscope_tests COMMAND kaleidoscope_tests)
