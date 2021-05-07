# minimum required CMAKE version
CMAKE_MINIMUM_REQUIRED(VERSION 3.7 FATAL_ERROR)

project("Zero")

option(JIT_AVAILABLE "JIT available for the target arch" ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# compiler must be 11 or 14
set(CMAKE_CXX_STANDARD 11)

add_definitions(-DASMJIT_STATIC)

# antlr commit hash
set(ANTLR4_TAG 4dfacf63e228d616232df90e4273e608d69a7f44)

# add external build for antlrcpp
include(ExternalAntlr4Cpp)

# add antrl4cpp artifacts to project environment
include_directories(${ANTLR4_INCLUDE_DIRS})

# set variable pointing to the antlr tool that supports C++
# this is not required if the jar file can be found under PATH environment
set(ANTLR_EXECUTABLE ${CMAKE_CURRENT_SOURCE_DIR}/antlr-4.9.2-complete.jar)
# add macros to generate ANTLR Cpp code from grammar
find_package(ANTLR REQUIRED)


# Call macro to add lexer and grammar to your build dependencies.
antlr_target(ZeroLexer ZLexer.g4 LEXER
        PACKAGE zero)
antlr_target(ZeroParser ZParser.g4 PARSER
        PACKAGE zero
        DEPENDS_ANTLR ZeroLexer
        COMPILE_FLAGS -lib ${ANTLR_ZeroLexer_OUTPUT_DIR})

# include generated files in project environment
include_directories(${ANTLR_ZeroLexer_OUTPUT_DIR})
include_directories(${ANTLR_ZeroParser_OUTPUT_DIR})

include("lib/asmjit/CMakeLists.txt")

message(STATUS "asmjt dir = ${ASMJIT_DIRS}")

#project headers
include_directories("include")

#source files
file(GLOB_RECURSE SRC "src/*.cpp")

if (JIT_AVAILABLE)
    # add asmjit
    include_directories(${ASMJIT_DIRS}/src)
    list(FILTER SRC EXCLUDE REGEX ".*direct_threaded_vm.cpp$")
else ()
    list(FILTER SRC EXCLUDE REGEX ".*context_threaded_vm.cpp$")
endif ()


# add generated grammar to demo binary target
add_executable(zero
        ${SRC}
        ${ANTLR_ZeroLexer_CXX_OUTPUTS}
        ${ANTLR_ZeroParser_CXX_OUTPUTS})

target_link_libraries(zero antlr4_static)

if (JIT_AVAILABLE)
    target_link_libraries(zero ${ASMJIT_LIBS})
endif()
#set_target_properties(zero PROPERTIES COMPILE_FLAGS " -O3")