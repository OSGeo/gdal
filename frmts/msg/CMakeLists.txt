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
          PLUGIN_CAPABLE)
gdal_standard_includes(gdal_MSG)
target_compile_definitions(gdal_MSG PRIVATE -DDO_NOT_USE_DEBUG_BOOL)

include(ExternalProject)
ExternalProject_Add(
  PublicDecompWT
  GIT_REPOSITORY "https://gitlab.eumetsat.int/open-source/PublicDecompWT.git"
  GIT_TAG "ef59964488026c88734ff7337eaff365d0467e57"
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
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
  # These files has CPL_CVSID("$id$") which break ODR and UNITY_BUILD
  set_property(SOURCE reflectancecalculator.cpp msgcommand.cpp msgdataset.cpp xritheaderparser.cpp prologue.cpp
               PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endif ()
