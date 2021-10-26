# Find TEIGHA
# ~~~~~~~~~
#
# Copyright (c) 2017,2018, Hiroshi Miura <miurahr@linux.com>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
# If it's found it sets TEIGHA_FOUND to TRUE
# and following variables are set:
#    TEIGHA_INCLUDE_DIR
#    TEIGHA_LIBRARIES
#
#  Before call here user should set TEIGHA_PLATFORM variable
if(NOT WINDOWS)
    return()
endif()

if(TEIGHA_PLATFORM)
    set(TEIGHA_DIR ${TEIGHA_PLATFORM})
else()
    set(TEIGHA_DIR "C:/Program Files(x86)/Common Files/Teigha")
endif()

find_path(TEIGHA_INCLUDE_DIR  OdaCommon.h
          PATHS ${TEIGHA_DIR}/Include)

find_library(TEIGHA_TG_ExamplesCommon_LIBRARY
             NAMES TG_ExamplesCommon
             PATHS ${TEIGHA_DIR}/lib)
find_library(TEIGHA_TD_ExamplesCommon_LIBRARY
             NAMES TD_ExamplesCommon
             PATHS ${TEIGHA_DIR}/lib)
find_library(TEIGHA_TD_Key_LIBRARY
             NAMES TD_Key
             PATHS ${TEIGHA_DIR}/lib)
foreach(TGT IN ITEMS TD_Db TD_DbRoot TD_Root TD_Ge TD_Alloc)
    find_library(TEIGHA_${TGT}_LIBRARY
                 NAMES ${TGT}
                 PATHS ${TEIGHA_DIR}/lib
    )
endforeach()

# When search in /exe and found *.dll
# set(TEIGHA_CPPFLAGS "-D_TOOLKIT_IN_DLL_")
