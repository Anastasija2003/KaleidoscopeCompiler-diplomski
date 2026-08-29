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

llvm_map_components_to_libnames(LLVM_LIBS core orcjit native passes mc target
                                 linker bitreader bitwriter)

set(PICOSHA2_HEADER ${CMAKE_BINARY_DIR}/picosha2/picosha2.h)
if(NOT EXISTS ${PICOSHA2_HEADER})
  file(DOWNLOAD
    https://raw.githubusercontent.com/okdshin/PicoSHA2/v1.0.1/picosha2.h
    ${PICOSHA2_HEADER}
    EXPECTED_HASH SHA256=b13c180161ffac8d0adc81e033e493c409457c4d1258ab9781ac80579ba3bdd8
    SHOW_PROGRESS)
endif()
include_directories(${CMAKE_BINARY_DIR}/picosha2)

set(NLOHMANN_JSON_HEADER ${CMAKE_BINARY_DIR}/nlohmann_json/nlohmann/json.hpp)
if(NOT EXISTS ${NLOHMANN_JSON_HEADER})
  file(DOWNLOAD
    https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
    ${NLOHMANN_JSON_HEADER}
    EXPECTED_HASH SHA256=9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6
    SHOW_PROGRESS)
endif()
include_directories(${CMAKE_BINARY_DIR}/nlohmann_json)

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(COMMON_SRCS ${SRC_DIR}/Lexer.cpp ${SRC_DIR}/Parser.cpp ${SRC_DIR}/CodeGen.cpp
                 ${SRC_DIR}/Canonicalize.cpp ${SRC_DIR}/CallGraph.cpp
                 ${SRC_DIR}/FunctionHasher.cpp ${SRC_DIR}/FunctionCache.cpp
                 ${SRC_DIR}/DirtySet.cpp ${SRC_DIR}/IncrementalDriver.cpp)
set(COMMON_HDRS ${SRC_DIR}/Lexer.h ${SRC_DIR}/AST.h ${SRC_DIR}/Parser.h
                 ${SRC_DIR}/CodeGenContext.h ${SRC_DIR}/ModuleSink.h
                 ${SRC_DIR}/SourceLocation.h ${SRC_DIR}/CanonState.h
                 ${SRC_DIR}/CallGraph.h ${SRC_DIR}/FunctionHasher.h
                 ${SRC_DIR}/ParsedProgram.h ${SRC_DIR}/FunctionCache.h
                 ${SRC_DIR}/DirtySet.h ${SRC_DIR}/IncrementalDriver.h)

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
