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
option(MKPDEMO_BUILD_ALL "Build everything" OFF)


# do not allow building in-source
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there.")
endif()


# dependencies
include(cmake/CPM.cmake)
CPMAddPackage(
    NAME mkpoker
    GITHUB_REPOSITORY MichaelUnknown/mkpoker
    GIT_TAG 61e958ffba5919faef2202d85b6afd216c56c0fe
)


# configure according to options
if(MKPDEMO_BUILD_ALL)
    message(STATUS "Building everything")
    add_subdirectory(example)
endif()


# make installable target
#string(TOLOWER ${PROJECT_NAME}/version.h VERSION_HEADER_LOCATION)
#packageProject(
#    NAME ${PROJECT_NAME}
#    VERSION ${PROJECT_VERSION}
#    NAMESPACE ${PROJECT_NAMESPACE}
#    BINARY_DIR ${PROJECT_BINARY_DIR}
#    INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include
#    INCLUDE_DESTINATION include/${PROJECT_NAME}-${PROJECT_VERSION}
#    VERSION_HEADER "${VERSION_HEADER_LOCATION}"
#    DEPENDENCIES ""
#)
