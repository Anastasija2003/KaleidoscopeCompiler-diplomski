include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.7.1)
FetchContent_MakeAvailable(Catch2)

add_executable(kaleidoscope_tests
  lexer_test.cpp
  canonicalize_test.cpp
  callgraph_test.cpp
  functioncache_test.cpp
  dirtyset_test.cpp
  CodegenStub.cpp
  ${CMAKE_SOURCE_DIR}/src/Lexer.cpp
  ${CMAKE_SOURCE_DIR}/src/Canonicalize.cpp
  ${CMAKE_SOURCE_DIR}/src/CallGraph.cpp
  ${CMAKE_SOURCE_DIR}/src/FunctionHasher.cpp
  ${CMAKE_SOURCE_DIR}/src/FunctionCache.cpp
  ${CMAKE_SOURCE_DIR}/src/DirtySet.cpp)
target_include_directories(kaleidoscope_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(kaleidoscope_tests PRIVATE Catch2::Catch2WithMain)

add_test(NAME kaleidoscope_tests COMMAND kaleidoscope_tests)

add_executable(kaleidoscope_integration_tests
  parser_test.cpp
  ${COMMON_SRCS})
target_include_directories(kaleidoscope_integration_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(kaleidoscope_integration_tests PRIVATE Catch2::Catch2WithMain ${LLVM_LIBS})

add_test(NAME kaleidoscope_integration_tests COMMAND kaleidoscope_integration_tests)
