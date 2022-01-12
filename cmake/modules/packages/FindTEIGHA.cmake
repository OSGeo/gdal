# Find TEIGHA
# ~~~~~~~~~
#
# Copyright (c) 2017,2018, Hiroshi Miura <miurahr@linux.com>
# Copyright (c) 2021 Even Rouault <even.rouault@spatialys.com>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
# If it's found it sets TEIGHA_FOUND to TRUE
# and following variables are set:
#    TEIGHA_INCLUDE_DIR
#    TEIGHA_LIBRARIES
#
#  Before call here user should set TEIGHA_ROOT variable (and possibly TEIGHA_PLATFORM as well)

if(NOT TEIGHA_ROOT)
    return()
endif()

if(NOT EXISTS "${TEIGHA_ROOT}")
    message(FATAL_ERROR "TEIGHA_ROOT=${TEIGHA_ROOT} does not exist")
endif()

if(NOT TEIGHA_PLATFORM)
    if(NOT EXISTS "${TEIGHA_ROOT}/lib")
        message(FATAL_ERROR "TEIGHA_ROOT=${TEIGHA_ROOT} should point to a directory with a lib/ subdirectory")
    endif()
    file(GLOB TEIGHA_PLATFORM RELATIVE "${TEIGHA_ROOT}/lib" "${TEIGHA_ROOT}/lib/*")
    list(LENGTH TEIGHA_PLATFORM TEIGHA_PLATFORM_LIST_LENGTH)
    if(NOT TEIGHA_PLATFORM_LIST_LENGTH EQUAL "1")
        message(FATAL_ERROR "TEIGHA_ROOT=${TEIGHA_ROOT}/lib has several directories. You should also set the TEIGHA_PLATFORM variable to the subdirectory of interest")
    endif()
endif()

set(TEIGHA_INCLUDE_SUBDIRS_NAMES
    ${TEIGHA_INCLUDE_SUBDIRS_NAMES}
    Core/Include
    Core/Extensions/ExServices
    Core/Examples/Common
    ThirdParty/activation
    KernelBase/Include
    Kernel/Include
    Kernel/Extensions/ExServices
    Drawing/Include
    Drawing/Extensions/ExServices
    Dgn/include
    Dgn/Extensions/ExServices)
set(TEIGHA_INCLUDE_DIRS)
foreach(_TEIGHA_INCLUDE_SUBDIR IN LISTS TEIGHA_INCLUDE_SUBDIRS_NAMES)
    if(EXISTS "${TEIGHA_ROOT}/${_TEIGHA_INCLUDE_SUBDIR}")
        list(APPEND TEIGHA_INCLUDE_DIRS "${TEIGHA_ROOT}/${_TEIGHA_INCLUDE_SUBDIR}")
    endif()
endforeach()

if(DEFINED TEIGHA_ACTIVATION_FILE_DIRECTORY)
    list(APPEND TEIGHA_INCLUDE_DIRS "${TEIGHA_ACTIVATION_FILE_DIRECTORY}")
else()
    file(GLOB _od_activation_info "${TEIGHA_ROOT}/OdActivationInfo")
    if(_od_activation_info)
        list(LENGTH _od_activation_info _od_activation_info_length)
        if(_od_activation_info_length EQUAL 1)
            get_filename_component(TEIGHA_ACTIVATION_FILE_DIRECTORY "${_od_activation_info}" DIRECTORY)
            list(APPEND TEIGHA_INCLUDE_DIRS "${TEIGHA_ACTIVATION_FILE_DIRECTORY}")
        else()
            message(FATAL_ERROR "Several OdActivationInfo files found under ${TEIGHA_ROOT}. Point to one of them by setting TEIGHA_ACTIVATION_FILE_DIRECTORY")
        endif()
    else()
        message(WARNING "No OdActivationInfo file found under ${TEIGHA_ROOT}. You'll likely need to point to one by setting TEIGHA_ACTIVATION_FILE_DIRECTORY")
    endif()
endif()

set(TEIGHA_LIBRARIES)

if(EXISTS "${TEIGHA_ROOT}/bin/${TEIGHA_PLATFORM}/TG_Db.tx")
    # Linux shared libs
    list(APPEND TEIGHA_LIBRARIES_NAMES
        "bin/${TEIGHA_PLATFORM}/TG_Db.tx"
        "bin/${TEIGHA_PLATFORM}/TD_DbEntities.tx"
        "lib/${TEIGHA_PLATFORM}/libTD_DrawingsExamplesCommon.a"
        "lib/${TEIGHA_PLATFORM}/libTG_ExamplesCommon.a"
        "lib/${TEIGHA_PLATFORM}/libTD_ExamplesCommon.a"
        "lib/${TEIGHA_PLATFORM}/libTD_Key.a"
        "bin/${TEIGHA_PLATFORM}/libTD_Db.so"
        "bin/${TEIGHA_PLATFORM}/libTD_DbCore.so"
        "bin/${TEIGHA_PLATFORM}/libTD_DbRoot.so"
        "bin/${TEIGHA_PLATFORM}/libTD_Gi.so"
        "bin/${TEIGHA_PLATFORM}/libTD_SpatialIndex.so"
        "bin/${TEIGHA_PLATFORM}/libTD_Root.so"
        "bin/${TEIGHA_PLATFORM}/libTD_Ge.so"
        "bin/${TEIGHA_PLATFORM}/libTD_Zlib.so"
        "bin/${TEIGHA_PLATFORM}/libTD_Alloc.so"
        "bin/${TEIGHA_PLATFORM}/liboless.so"
        "bin/${TEIGHA_PLATFORM}/liblibcrypto.so"
        )
elseif (EXISTS "${TEIGHA_ROOT}/lib/${TEIGHA_PLATFORM}/libTG_Db.a")
    # Linux static libs
    list(APPEND TEIGHA_LIBRARIES_NAMES
        "lib/${TEIGHA_PLATFORM}/libTD_Db.a"
        "lib/${TEIGHA_PLATFORM}/libTD_DbCore.a"
        "lib/${TEIGHA_PLATFORM}/libTD_DbEntities.a"
        "lib/${TEIGHA_PLATFORM}/libTD_DrawingsExamplesCommon.a"
        "lib/${TEIGHA_PLATFORM}/libTG_Db.a"
        "lib/${TEIGHA_PLATFORM}/libTG_ExamplesCommon.a"
        "lib/${TEIGHA_PLATFORM}/libTD_Alloc.a"
        "lib/${TEIGHA_PLATFORM}/libTD_DbRoot.a"
        "lib/${TEIGHA_PLATFORM}/libTD_ExamplesCommon.a"
        "lib/${TEIGHA_PLATFORM}/libWinBitmap.a"
        "lib/${TEIGHA_PLATFORM}/libTD_Gi.a"
        "lib/${TEIGHA_PLATFORM}/libtinyxml.a"
        "lib/${TEIGHA_PLATFORM}/libTD_Zlib.a"
        "lib/${TEIGHA_PLATFORM}/libTD_SpatialIndex.a"
        "lib/${TEIGHA_PLATFORM}/libTD_DgnImport.a"
        "lib/${TEIGHA_PLATFORM}/libTD_DgnUnderlay.a"
        "lib/${TEIGHA_PLATFORM}/libModelerGeometry.a"
        "lib/${TEIGHA_PLATFORM}/libTD_BrepRenderer.a"
        "lib/${TEIGHA_PLATFORM}/libRecomputeDimBlock.a"
        "lib/${TEIGHA_PLATFORM}/libTD_AcisBuilder.a"
        "lib/${TEIGHA_PLATFORM}/libTD_Br.a"
        "lib/${TEIGHA_PLATFORM}/libTD_Gs.a"
        "lib/${TEIGHA_PLATFORM}/libTD_Ge.a"
        "lib/${TEIGHA_PLATFORM}/libTD_Root.a"
        "lib/${TEIGHA_PLATFORM}/liboless.a"
        "lib/${TEIGHA_PLATFORM}/liblibcrypto.a"
        "lib/${TEIGHA_PLATFORM}/libsisl.a"
        #"lib/${TEIGHA_PLATFORM}/libFreeType.a"  # System one added later
        #"lib/${TEIGHA_PLATFORM}/libzlib.a"      # System one added later
        )
elseif (EXISTS "${TEIGHA_ROOT}/lib/${TEIGHA_PLATFORM}/TD_DrawingsExamplesCommon.lib")
    # Windows ODA 2021
    set(TD_DRAWINGS_LIBDIR "lib/${TEIGHA_PLATFORM}")
    set(TD_KERNEL_LIBDIR   "lib/${TEIGHA_PLATFORM}")
    list(APPEND TEIGHA_LIBRARIES_NAMES
        "${TD_DRAWINGS_LIBDIR}/TD_Db.lib"
        "${TD_DRAWINGS_LIBDIR}/TD_DbCore.lib"
        "${TD_DRAWINGS_LIBDIR}/TD_DbEntities.lib"
        "${TD_DRAWINGS_LIBDIR}/TD_DrawingsExamplesCommon.lib"
        "${TD_DRAWINGS_LIBDIR}/TG_Db.lib"
        "${TD_DRAWINGS_LIBDIR}/TG_ExamplesCommon.lib"
        "${TD_KERNEL_LIBDIR}/TD_Alloc.lib"
        "${TD_KERNEL_LIBDIR}/TD_DbRoot.lib"
        "${TD_KERNEL_LIBDIR}/TD_ExamplesCommon.lib"
        "${TD_KERNEL_LIBDIR}/TD_Ge.lib"
        "${TD_KERNEL_LIBDIR}/TD_Root.lib")
    if (EXISTS "${TEIGHA_ROOT}/exe/${TEIGHA_PLATFORM}/oless.dll")
    # Dynamic libs
    list(APPEND TEIGHA_LIBRARIES_NAMES
        "${TD_DRAWINGS_LIBDIR}/TD_Key.lib")
    else()
    # Static libs
    list(APPEND TEIGHA_LIBRARIES_NAMES
        "${TD_KERNEL_LIBDIR}/TD_Gi.lib"
        "${TD_KERNEL_LIBDIR}/tinyxml.lib"
        "${TD_KERNEL_LIBDIR}/TD_Zlib.lib"
        "${TD_DRAWINGS_LIBDIR}/TD_DbIO.lib"
        "${TD_DRAWINGS_LIBDIR}/ISM.lib"
        "${TD_DRAWINGS_LIBDIR}/WipeOut.lib"
        "${TD_DRAWINGS_LIBDIR}/RText.lib"
        "${TD_DRAWINGS_LIBDIR}/ATEXT.lib"
        "${TD_DRAWINGS_LIBDIR}/SCENEOE.lib"
        "${TD_DRAWINGS_LIBDIR}/ACCAMERA.lib"
        "${TD_DRAWINGS_LIBDIR}/AcMPolygonObj15.lib"
        "${TD_KERNEL_LIBDIR}/TD_Gs.lib"
        "${TD_KERNEL_LIBDIR}/TD_SpatialIndex.lib"
        "${TD_KERNEL_LIBDIR}/oless.lib"
        "${TD_KERNEL_LIBDIR}/UTF.lib"
        "${TD_KERNEL_LIBDIR}/WinBitmap.lib"
        "${TD_KERNEL_LIBDIR}/OdBrepModeler.lib"
        "${TD_KERNEL_LIBDIR}/TD_Br.lib"
        "${TD_KERNEL_LIBDIR}/TD_AcisBuilder.lib"
        "${TD_KERNEL_LIBDIR}/TD_BrepBuilder.lib"
        "${TD_KERNEL_LIBDIR}/TD_BrepBuilderFiller.lib"
        "${TD_KERNEL_LIBDIR}/TD_BrepRenderer.lib"
        "${TD_DRAWINGS_LIBDIR}/ModelerGeometry.lib"
        "${TD_DRAWINGS_LIBDIR}/RecomputeDimBlock.lib")
    list(APPEND TEIGHA_LIBRARIES crypt32.lib
                                 secur32.lib
                                 user32.lib
                                 gdi32.lib
                                 ole32.lib)
    endif()
elseif (EXISTS "${TEIGHA_ROOT}/lib/${TEIGHA_PLATFORM}/TD_Key.lib")
    # Windows Teigha 4.2.2 shared libs
    list(APPEND TEIGHA_LIBRARIES_NAMES
         "lib/${TEIGHA_PLATFORM}/TD_Key.lib"
         "lib/${TEIGHA_PLATFORM}/TD_ExamplesCommon.lib"
         "lib/${TEIGHA_PLATFORM}/TD_Db.lib"
         "lib/${TEIGHA_PLATFORM}/TD_DbRoot.lib"
         "lib/${TEIGHA_PLATFORM}/TD_Root.lib"
         "lib/${TEIGHA_PLATFORM}/TD_Ge.lib"
         "lib/${TEIGHA_PLATFORM}/TD_Alloc.lib"
         "lib/${TEIGHA_PLATFORM}/TG_Db.lib"
         "lib/${TEIGHA_PLATFORM}/TG_ExamplesCommon.lib")
else()
    # TODO ODA 2021.2 shared and static libs to migrate from nmake.opt
    message(FATAL_ERROR "Teigha/ODA library layout not handled currently")
endif()

foreach(_TEIGHA_LIBRARY IN LISTS TEIGHA_LIBRARIES_NAMES)
    if(EXISTS "${TEIGHA_ROOT}/${_TEIGHA_LIBRARY}")
        list(APPEND TEIGHA_LIBRARIES "${TEIGHA_ROOT}/${_TEIGHA_LIBRARY}")
    endif()
endforeach()

if(WIN32)
    list(APPEND TEIGHA_LIBRARIES advapi32.lib)
endif()
if (EXISTS "${TEIGHA_ROOT}/lib/${TEIGHA_PLATFORM}/libTG_Db.a")
    find_library(FREETYPE_LIBRARY NAMES freetype)
    mark_as_advanced(FREETYPE_LIBRARY)
    list(APPEND TEIGHA_LIBRARIES ${FREETYPE_LIBRARY})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TEIGHA
                                  FOUND_VAR TEIGHA_FOUND
                                  REQUIRED_VARS TEIGHA_LIBRARIES TEIGHA_INCLUDE_DIRS)
if(TEIGHA_FOUND)
    if(NOT TARGET TEIGHA::TEIGHA)
        set(INCR 1)
        set(TEIGHA_TARGETS)
        foreach(_TEIGHA_LIBRARY IN LISTS TEIGHA_LIBRARIES)
            add_library(TEIGHA::TEIGHA_${INCR} UNKNOWN IMPORTED)
            set_target_properties(TEIGHA::TEIGHA_${INCR} PROPERTIES
                                  IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                                  IMPORTED_LOCATION "${_TEIGHA_LIBRARY}")
            list(APPEND TEIGHA_TARGETS TEIGHA::TEIGHA_${INCR})
            math(EXPR INCR "${INCR}+1")
        endforeach()
        if (EXISTS "${TEIGHA_ROOT}/lib/${TEIGHA_PLATFORM}/libTG_Db.a")
            find_library(FREETYPE_LIBRARY NAMES freetype)
            list(APPEND TEIGHA_TARGETS ZLIB::ZLIB)
        endif()
        add_library(TEIGHA::TEIGHA INTERFACE IMPORTED)
        set_target_properties(TEIGHA::TEIGHA PROPERTIES
                     INTERFACE_INCLUDE_DIRECTORIES "${TEIGHA_INCLUDE_DIRS}"
                     INTERFACE_LINK_LIBRARIES "${TEIGHA_TARGETS}")
        if(EXISTS "${TEIGHA_ROOT}/bin/${TEIGHA_PLATFORM}/liboless.so" OR EXISTS "${TEIGHA_ROOT}/exe/${TEIGHA_PLATFORM}/oless.dll")
            set_property(TARGET TEIGHA::TEIGHA APPEND PROPERTY
                         INTERFACE_COMPILE_DEFINITIONS "_TOOLKIT_IN_DLL_")
        endif()
    endif()
endif()
