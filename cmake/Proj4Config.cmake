################################################################################
# SociConfig.cmake - CMake build configuration of SOCI library
################################################################################
# Copyright (C) 2010 Mateusz Loskot <mateusz@loskot.net>
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
################################################################################
include (CheckIncludeFiles)
include (CheckLibraryExists) 
include (CheckFunctionExists)

# check needed include file
check_include_files (dlfcn.h HAVE_DLFCN_H)
check_include_files (inttypes.h HAVE_INTTYPES_H)
check_include_files (jni.h HAVE_JNI_H)
check_include_files (memory.h HAVE_MEMORY_H)
check_include_files (stdint.h HAVE_STDINT_H)
check_include_files (stdlib.h HAVE_STDLIB_H)
check_include_files (string.h HAVE_STRING_H)
check_include_files (sys/stat.h HAVE_SYS_STAT_H)
check_include_files (sys/types.h HAVE_SYS_TYPES_H)
check_include_files (unistd.h HAVE_UNISTD_H)
check_include_files("stdlib.h;stdarg.h;string.h;float.h" STDC_HEADERS)

CHECK_FUNCTION_EXISTS(localeconv HAVE_LOCALECONV)

# check libm need on unix 
check_library_exists(m ceil "" HAVE_LIBM) 

set(PACKAGE "proj")
set(PACKAGE_BUGREPORT "warmerdam@pobox.com")
set(PACKAGE_NAME "PROJ.4 Projections")
set(PACKAGE_STRING "PROJ.4 Projections ${${PROJECT_INTERN_NAME}_VERSION}")
set(PACKAGE_TARNAME "proj")
set(PACKAGE_VERSION "${${PROJECT_INTERN_NAME}_VERSION}")

configure_file(cmake/proj_config.cmake.in src/proj_config.h)


