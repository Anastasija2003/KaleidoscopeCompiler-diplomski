cmake_minimum_required(VERSION 3.20)
# C is enabled alongside C++ only because LLVMConfig.cmake's own optional
# LibEdit detection (find_package(LibEdit) below, via find_package(LLVM))
# runs a C try_compile check -- we never compile any C ourselves.
project(KaleidoscopeCompiler C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# Homebrew's LLVM is keg-only (not symlinked into PATH/CMAKE_PREFIX_PATH by
# default), same reason the old Makefile had to go through `brew --prefix
# llvm` for llvm-config. Do the same here so `cmake -B build` works without
# extra flags on a normal Homebrew setup; -DCMAKE_PREFIX_PATH=... still
# overrides this on any other machine.
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
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# main and kcc share Parser.cpp/CodeGen.cpp (COMMON_SRCS below), so both
# need the same LLVM components regardless of which binary actually uses
# ORC JIT at runtime -- Parser.cpp references orc::JIT APIs even in the
# branch that's dead when JIT is nullptr (kcc's mode), and CodeGen.cpp's
# optimization pipeline needs PassBuilder/StandardInstrumentations
# ('passes'). kcc additionally needs 'mc'/'target' for TargetMachine and
# object-file emission.
llvm_map_components_to_libnames(LLVM_LIBS core orcjit native passes mc target)

set(COMMON_SRCS src/Lexer.cpp src/Parser.cpp src/CodeGen.cpp)
set(COMMON_HDRS src/Lexer.h src/AST.h src/Parser.h src/CodeGenContext.h
                 src/KaleidoscopeJIT.h src/SourceLocation.h)

# main - JIT REPL (see src/main.cpp).
add_executable(main src/main.cpp ${COMMON_SRCS} ${COMMON_HDRS})
target_link_libraries(main PRIVATE ${LLVM_LIBS})

# kcc - static (ahead-of-time) compiler to a native object file.
add_executable(kcc src/compile_main.cpp ${COMMON_SRCS} ${COMMON_HDRS})
target_link_libraries(kcc PRIVATE ${LLVM_LIBS})

# lex - lexer-only token dump, deliberately dependency-free (no LLVM).
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
