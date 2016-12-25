#################################################################################
#
# This file is part of CMake configuration for PROJ4 library (inspired from SOCI
# CMake,  Copyright (C) 2009-2010 Mateusz Loskot <mateusz@loskot.net> )
#
# Copyright (C) 2011 Nicolas David <nicolas.david@ign.fr>
# Distributed under the MIT license
#
#################################################################################
# General settings
#################################################################################
cmake_minimum_required(VERSION 2.6.0 FATAL_ERROR)

# proj4 is an ANSI C project
project(PROJ4 C)
set(PROJECT_INTERN_NAME PROJ)

#################################################################################
# PROJ4 CMake modules
#################################################################################
# Path to additional CMake modules
set(CMAKE_MODULE_PATH ${PROJ4_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
set(CMAKE_MODULE_PATH ${PROJ4_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})

include(Proj4Utilities)

message(STATUS "")
colormsg(_HIBLUE_ "Configuring PROJ:")

#################################################################################
#PROJ version information
#################################################################################
include(Proj4Version)
proj_version(MAJOR 4 MINOR 9 PATCH 3)
set(PROJ_API_VERSION "12")
set(PROJ_BUILD_VERSION "12.0.0")

#################################################################################
# Build features and variants
#################################################################################
include(Proj4SystemInfo)
include(Proj4Config)
include(Proj4Mac)
include(policies)

#################################################################################
# Self-test build config
#################################################################################

option(SELFTEST "Include self-test in build" OFF)
if(SELFTEST)
    add_definitions(-DPJ_SELFTEST)
endif(SELFTEST)

#################################################################################
# threading configuration
#################################################################################
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package (Threads)

include(CheckIncludeFiles)
include(CheckSymbolExists)
CHECK_SYMBOL_EXISTS(PTHREAD_MUTEX_RECURSIVE pthread.h HAVE_PTHREAD_MUTEX_RECURSIVE_DEFN)
if (HAVE_PTHREAD_MUTEX_RECURSIVE_DEFN)
    add_definitions(-DHAVE_PTHREAD_MUTEX_RECURSIVE=1)
endif()

boost_report_value(PROJ_PLATFORM_NAME)
boost_report_value(PROJ_COMPILER_NAME)

# Set a default build type for single-configuration cmake generators if
# no build type is set.
if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE Release)
endif ()

if (MSVC OR CMAKE_CONFIGURATION_TYPES)
  # For multi-config systems and for Visual Studio, the debug version of
  # the library has _d appended.
  set (CMAKE_DEBUG_POSTFIX _d)
endif ()

option(PROJ4_TESTS "Enable build of collection of PROJ4 tests" ON)
boost_report_value(PROJ4_TESTS)
if(PROJ4_TESTS)
    include(CTest)
    enable_testing()
endif(PROJ4_TESTS)
include(Proj4Test)

# Put the libaries and binaries that get built into directories at the
# top of the build tree rather than in hard-to-find leaf
# directories. This simplifies manual testing and the use of the build
# tree rather than installed Boost libraries.
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
link_directories(${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

#################################################################################
# Installation
#################################################################################
include(Proj4InstallPath)
set(BINDIR "${DEFAULT_BINDIR}" CACHE PATH "The directory to install binaries into.")
set(LIBDIR "${DEFAULT_LIBDIR}" CACHE PATH "The directory to install libraries into.")
set(DATADIR "${DEFAULT_DATADIR}" CACHE PATH "The directory to install data files into.")
set(DOCDIR "${DEFAULT_DOCDIR}" CACHE PATH "The directory to install doc files into.")
set(INCLUDEDIR "${DEFAULT_INCLUDEDIR}" CACHE PATH "The directory to install includes into.")

#################################################################################
# Build configured components
#################################################################################
include_directories(${PROJ4_SOURCE_DIR}/src)

message(STATUS "")
add_subdirectory(nad)
add_subdirectory(src)
add_subdirectory(man)
add_subdirectory(cmake)

