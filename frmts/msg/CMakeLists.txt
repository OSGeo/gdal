add_gdal_driver(
  TARGET gdal_MSG
  SOURCES msgcommand.cpp
          msgcommand.h
          msgdataset.cpp
          msgdataset.h
          prologue.cpp
          prologue.h
          PublicDecompWT_all.cpp
          PublicDecompWT_headers.h
          reflectancecalculator.h
          reflectancecalculator.cpp
          xritheaderparser.cpp
          xritheaderparser.h
  CORE_SOURCES
          msgdrivercore.cpp
  PLUGIN_CAPABLE
  NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET gdal_MSG)
    return()
endif()

if (WIN32)
    set_property(TARGET gdal_MSG PROPERTY UNITY_BUILD OFF)
    # Otherwise we get errors like the following, likely related to the PublicDecompWT SDK
    # C:\Program Files (x86)\Windows Kits\10\include\10.0.22621.0\shared\rpcndr.h(203): error C2872: 'byte': ambiguous symbol
    # C:\Program Files (x86)\Windows Kits\10\include\10.0.22621.0\shared\rpcndr.h(202): note: could be 'unsigned char byte'
    # C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.37.32822\include\cstddef(29): note: or       'std::byte'
endif()

gdal_standard_includes(gdal_MSG)

set(PUBLICDECOMPWT_URL
    "https://gitlab.eumetsat.int/open-source/PublicDecompWT.git"
    CACHE STRING "URL of EUMETSAT PublicDecompWT repository")
set(PUBLICDECOMPWT_TAG
    "ef59964488026c88734ff7337eaff365d0467e57"
    CACHE STRING "Tag/hash of EUMETSAT PublicDecompWT repository")

include(ExternalProject)
ExternalProject_Add(
  PublicDecompWT
  GIT_REPOSITORY "${PUBLICDECOMPWT_URL}"
  GIT_TAG "${PUBLICDECOMPWT_TAG}"
  SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/PublicDecompWT"
  CONFIGURE_COMMAND ${CMAKE_COMMAND} "-DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}" "-DIS_WIN32=${WIN32}" -P
                    "${CMAKE_CURRENT_SOURCE_DIR}/generate_PublicDecompWT_all.h.cmake"
  BUILD_COMMAND ""
  INSTALL_COMMAND "")
add_dependencies(gdal_MSG PublicDecompWT)

if (WIN32)
  target_compile_definitions(gdal_MSG PRIVATE -DWIN32)
  set_property(
    SOURCE PublicDecompWT_all.cpp
    APPEND
    PROPERTY COMPILE_OPTIONS /wd4189 /wd4244 /wd4267 /wd4456 /wd4706)
endif ()

target_include_directories(
  gdal_MSG
  PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/PublicDecompWT/DISE>"
          "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/PublicDecompWT/COMP/WT/Inc>"
          "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/PublicDecompWT/COMP/Inc>"
          "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>")
