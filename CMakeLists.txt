##########################################
## main library ##
##########################################

# /W3 warning in msvc fixed with 3.15
cmake_minimum_required(VERSION 3.15 FATAL_ERROR)


project(
    mkpdemo
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "A small demo that uses mkpoker."
)


# project options
option(MKPDEMO_BUILD_ALL "Build everything" ON)


# do not allow building in-source
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there.")
endif()


# dependencies
include(cmake/CPM.cmake)
CPMAddPackage("gh:MichaelUnknown/mkpoker#dc0839624f30bdd6e72a3b92b77f70cd564fbef1")
CPMAddPackage("gh:fmtlib/fmt#7.1.3")
CPMAddPackage("gh:MichaelUnknown/sqlite-cpm@0.1.0")


# configure according to options
if(MKPDEMO_BUILD_ALL)
    message(STATUS "Building everything")
    add_subdirectory(example)
endif()

