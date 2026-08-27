cmake_minimum_required(VERSION 3.20)
project(KaleidoscopeCompiler C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

if(APPLE AND NOT LLVM_DIR)
  execute_process(
    COMMAND brew --prefix llvm
    OUTPUT_VARIABLE HOMEBREW_LLVM_PREFIX
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(HOMEBREW_LLVM_PREFIX)
    list(APPEND CMAKE_PREFIX_PATH "${HOMEBREW_LLVM_PREFIX}")
  endif()
endif()

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} at ${LLVM_DIR}")

list(APPEND CMAKE_MODULE_PATH ${LLVM_CMAKE_DIR})
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/jit)
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

llvm_map_components_to_libnames(LLVM_LIBS core orcjit native passes mc target)

set(COMMON_SRCS src/Lexer.cpp src/Parser.cpp src/CodeGen.cpp)
set(COMMON_HDRS src/Lexer.h src/AST.h src/Parser.h src/CodeGenContext.h
                 src/ModuleSink.h src/SourceLocation.h)

set(JIT_SRCS jit/main.cpp jit/JITModuleSink.cpp)
set(JIT_HDRS jit/KaleidoscopeJIT.h jit/JITModuleSink.h)

add_executable(main ${JIT_SRCS} ${JIT_HDRS} ${COMMON_SRCS} ${COMMON_HDRS})
target_link_libraries(main PRIVATE ${LLVM_LIBS})

add_executable(kcc src/compile_main.cpp ${COMMON_SRCS} ${COMMON_HDRS})
target_link_libraries(kcc PRIVATE ${LLVM_LIBS})

add_executable(lex src/lex_main.cpp src/Lexer.cpp src/Lexer.h)

set(KS_FILE ${CMAKE_SOURCE_DIR}/test.ks)
add_custom_target(run COMMAND main ${KS_FILE} DEPENDS main
                   COMMENT "Running JIT REPL on ${KS_FILE}")
add_custom_target(run-lex COMMAND lex ${KS_FILE} DEPENDS lex
                   COMMENT "Running lexer token dump on ${KS_FILE}")
add_custom_target(compile COMMAND kcc ${KS_FILE} DEPENDS kcc
                   COMMENT "Compiling ${KS_FILE} to output.o")

enable_testing()
add_subdirectory(tests)
