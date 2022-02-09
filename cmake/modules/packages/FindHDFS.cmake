# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindHDFS
--------

The following vars are set if HDFS (the native library provided with Hadoop) is found.

.. variable:: HDFS_FOUND

  True if found, otherwise all other vars are undefined

.. variable:: HDFS_INCLUDE_DIR

  The include dir for hdfs.h

.. variable:: HDFS_LIBRARY

  The library file

#]=======================================================================]


include(FindPackageHandleStandardArgs)

find_package(JNI)
if(JNI_FOUND)
    add_library(HDFS::JVM UNKNOWN IMPORTED)
    set_target_properties(HDFS::JVM PROPERTIES
                          IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                          IMPORTED_LOCATION "${JAVA_JVM_LIBRARY}")
endif()

find_path(HDFS_INCLUDE_DIR NAMES hdfs.h)
find_library(HDFS_LIBRARY NAMES hdfs PATH_SUFFIXES native)

mark_as_advanced(HDFS_INCLUDE_DIR HDFS_LIBRARY)
find_package_handle_standard_args(HDFS
                                  FOUND_VAR HDFS_FOUND
                                  REQUIRED_VARS HDFS_LIBRARY HDFS_INCLUDE_DIR)

if(HDFS_FOUND)
    set(HDFS_LIBRARIES ${HDFS_LIBRARY})
    set(HDFS_INCLUDE_DIRS ${HDFS_INCLUDE_DIR})

    if(NOT TARGET HDFS::HDFS)
        add_library(HDFS::HDFS UNKNOWN IMPORTED)
        set_target_properties(HDFS::HDFS PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES "${HDFS_INCLUDE_DIR}"
                              IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                              IMPORTED_LOCATION "${HDFS_LIBRARY}")
    endif()
endif()
