# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

find_path(IDB_INCLUDE_DIR c++/it.h PATHS ${IDB_ROOT} PATH_SUFFIXES incl)
# following libs are what is needed with ibm.csdk.4.50.FC7.LNX.tar
find_library(IDB_IFCPP_LIBRARY NAMES ifc++ PATHS ${IDB_ROOT} PATH_SUFFIXES lib/c++)
find_library(IDB_IFDMI_LIBRARY NAMES ifdmi PATHS ${IDB_ROOT} PATH_SUFFIXES lib/dmi)
find_library(IDB_IFSQL_LIBRARY NAMES ifsql PATHS ${IDB_ROOT} PATH_SUFFIXES lib/esql)
find_library(IDB_IFCLI_LIBRARY NAMES ifcli PATHS ${IDB_ROOT} PATH_SUFFIXES lib/cli)
mark_as_advanced(IDB_INCLUDE_DIR IDB_IFCPP_LIBRARY IDB_IFDMI_LIBRARY IDB_IFSQL_LIBRARY IDB_IFCLI_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(IDB
                                  FOUND_VAR IDB_FOUND
                                  REQUIRED_VARS IDB_INCLUDE_DIR
                                                IDB_IFCPP_LIBRARY
                                                IDB_IFDMI_LIBRARY
                                                IDB_IFSQL_LIBRARY
                                                IDB_IFCLI_LIBRARY)

### Import targets ############################################################
if(IDB_FOUND)
  set(IDB_INCLUDE_DIRS "${IDB_INCLUDE_DIR}/dmi;${IDB_INCLUDE_DIR}/c++")
  set(IDB_LIBRARIES "${IDB_IFCPP_LIBRARY};${IDB_IFDMI_LIBRARY};${IDB_IFSQL_LIBRARY};${IDB_IFCLI_LIBRARY}")
  if(NOT TARGET IDB::IDB)
    add_library(IDB::IDB UNKNOWN IMPORTED)
                set_target_properties(IDB::IDB PROPERTIES
                IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                IMPORTED_LOCATION "${IDB_IFCPP_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${IDB_INCLUDE_DIRS}")

    set(IDB_TARGETS)
    set(IDB_LIBRARIES IDB_IFDMI_LIBRARY IDB_IFSQL_LIBRARY IDB_IFCLI_LIBRARY)
    foreach(_IDB_LIBRARY_VARNAME IN LISTS IDB_LIBRARIES)
        add_library(IDB::IDB_${_IDB_LIBRARY_VARNAME} UNKNOWN IMPORTED)
        set_target_properties(IDB::IDB_${_IDB_LIBRARY_VARNAME} PROPERTIES
                              IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                              IMPORTED_LOCATION "${${_IDB_LIBRARY_VARNAME}}")
        list(APPEND IDB_TARGETS IDB::IDB_${_IDB_LIBRARY_VARNAME})
    endforeach()

    set_property(TARGET IDB::IDB APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES "${IDB_TARGETS}")
  endif()
endif()
