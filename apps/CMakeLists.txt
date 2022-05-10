# CMake4GDAL project is distributed under MIT license. See accompanying file LICENSE.txt.

include(GdalStandardIncludes)
add_library(
  appslib OBJECT
  commonutils.h
  gdal_utils.h
  gdalinfo_lib.cpp
  gdalbuildvrt_lib.cpp
  gdal_grid_lib.cpp
  gdal_translate_lib.cpp
  gdal_rasterize_lib.cpp
  gdalwarp_lib.cpp
  commonutils.cpp
  ogr2ogr_lib.cpp
  gdaldem_lib.cpp
  nearblack_lib.cpp
  gdalmdiminfo_lib.cpp
  gdalmdimtranslate_lib.cpp)
add_dependencies(appslib generate_gdal_version_h)
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:appslib>)
gdal_standard_includes(appslib)
target_compile_options(appslib PRIVATE ${GDAL_CXX_WARNING_FLAGS} ${WFLAG_OLD_STYLE_CAST})
target_include_directories(
  appslib PRIVATE $<TARGET_PROPERTY:gdal_vrt,SOURCE_DIR> $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>
                  $<TARGET_PROPERTY:ogr_geojson,SOURCE_DIR> $<TARGET_PROPERTY:ogr_MEM,SOURCE_DIR>)

gdal_target_link_libraries(appslib PRIVATE PROJ::proj)

set_property(TARGET appslib PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
  set_property(SOURCE ogr2ogr_bin.cpp gdal_grid_lib.cpp gdal_translate_lib.cpp gdalbuildvrt_lib.cpp gdaldem_lib.cpp
                      gdal_rasterize_lib.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endif ()

if (GDAL_USE_JSONC_INTERNAL)
  gdal_add_vendored_lib(appslib libjson)
else ()
  gdal_target_link_libraries(appslib PRIVATE ${JSONC_TARGET})
endif ()

include(TargetPublicHeader)
target_public_header(TARGET ${GDAL_LIB_TARGET_NAME} HEADERS gdal_utils.h)

if (BUILD_APPS)
  # Default Apps
  add_executable(gdalinfo gdal_utils_priv.h gdalinfo_bin.cpp)
  add_executable(gdalbuildvrt gdal_utils_priv.h gdalbuildvrt_bin.cpp)
  add_executable(gdaladdo gdal_utils_priv.h gdaladdo.cpp)
  add_executable(gdal_grid gdal_utils_priv.h commonutils.h gdal_grid_bin.cpp)
  add_executable(gdal_translate gdal_utils_priv.h commonutils.h gdal_translate_bin.cpp)
  add_executable(gdalwarp commonutils.h gdalwarp_bin.cpp)
  add_executable(gdal_contour commonutils.h gdal_contour.cpp)
  add_executable(gdalenhance gdalenhance.cpp)
  add_executable(gdallocationinfo gdallocationinfo.cpp)
  add_executable(gdalmanage gdalmanage.cpp)
  add_executable(gdalsrsinfo gdalsrsinfo.cpp)
  target_link_libraries(gdalsrsinfo PRIVATE PROJ::proj)
  add_executable(gdaltindex gdaltindex.cpp)
  add_executable(gdal_rasterize gdal_rasterize_bin.cpp)
  add_executable(gdaldem gdaldem_bin.cpp)
  add_executable(gdaltransform gdaltransform.cpp)
  add_executable(gdal_create gdal_create.cpp)
  add_executable(gdal_viewshed gdal_viewshed.cpp)
  add_executable(ogrinfo commonutils.h ogrinfo.cpp)
  add_executable(ogr2ogr ogr2ogr_bin.cpp)

  add_executable(ogrlineref ogrlineref.cpp)
  if (GDAL_USE_GEOS)
    target_compile_definitions(ogrlineref PRIVATE -DHAVE_GEOS=1)
  endif ()

  add_executable(ogrtindex ogrtindex.cpp)
  add_executable(nearblack nearblack_bin.cpp)
  add_executable(gdalmdiminfo gdalmdiminfo_bin.cpp)
  add_executable(gdalmdimtranslate gdalmdimtranslate_bin.cpp)
  set(APPS_TARGETS
      gdalinfo
      gdalbuildvrt
      gdaladdo
      gdal_grid
      gdal_translate
      gdal_rasterize
      gdalsrsinfo
      gdalenhance
      gdalmanage
      gdaltransform
      gdaltindex
      gdaldem
      gdal_create
      gdal_viewshed
      nearblack
      ogrlineref
      ogrtindex
      gdalwarp
      gdal_contour
      gdallocationinfo
      ogrinfo
      ogr2ogr
      gdalmdiminfo
      gdalmdimtranslate)
  if (ENABLE_GNM)
    add_executable(gnmanalyse gnmanalyse.cpp)
    add_executable(gnmmanage gnmmanage.cpp)
    list(APPEND APPS_TARGETS gnmanalyse gnmmanage)
    target_include_directories(gnmanalyse PRIVATE $<TARGET_PROPERTY:gnm,SOURCE_DIR>)
    target_include_directories(gnmmanage PRIVATE $<TARGET_PROPERTY:gnm,SOURCE_DIR>)
  endif ()

  add_custom_target(gdalapps DEPENDS ${APPS_TARGETS})
  foreach (UTILCMD IN ITEMS ${APPS_TARGETS})
    # Add that include directory before all others so the gdal_version.h from it is included and not
    # gcore/gdal_version.h
    target_include_directories(${UTILCMD} PRIVATE $<TARGET_PROPERTY:gcore,BINARY_DIR>/gdal_version_full)
    gdal_standard_includes(${UTILCMD})
    target_compile_options(${UTILCMD} PRIVATE ${GDAL_CXX_WARNING_FLAGS} ${WFLAG_OLD_STYLE_CAST})
    add_dependencies(${UTILCMD} generate_gdal_version_h)
    target_include_directories(
      ${UTILCMD} PRIVATE $<TARGET_PROPERTY:gdal_vrt,SOURCE_DIR> $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>
                         $<TARGET_PROPERTY:ogr_geojson,SOURCE_DIR> $<TARGET_PROPERTY:ogr_MEM,SOURCE_DIR>)
    if (MSVC)
      set_target_properties(${UTILCMD} PROPERTIES LINK_FLAGS "wsetargv.obj")
    endif ()
    if (MINGW)
      target_link_options(${UTILCMD} PRIVATE -municode)
    endif ()
    if (MSVC OR MINGW)
      target_compile_definitions(${UTILCMD} PRIVATE -DSUPPORTS_WMAIN)
    endif ()
    target_link_libraries(${UTILCMD} PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}>)
  endforeach ()
  install(TARGETS ${APPS_TARGETS} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

  # NON-Default Apps
  add_executable(dumpoverviews EXCLUDE_FROM_ALL dumpoverviews.cpp)
  add_executable(gdalflattenmask EXCLUDE_FROM_ALL gdalflattenmask.c)
  add_executable(gdaltorture EXCLUDE_FROM_ALL gdaltorture.cpp)
  add_executable(gdal2ogr EXCLUDE_FROM_ALL gdal2ogr.c)
  add_executable(gdalasyncread EXCLUDE_FROM_ALL gdalasyncread.cpp)
  add_executable(gdalwarpsimple EXCLUDE_FROM_ALL gdalwarpsimple.c)
  add_executable(multireadtest EXCLUDE_FROM_ALL multireadtest.cpp)
  add_executable(test_ogrsf test_ogrsf.cpp)
  add_executable(testreprojmulti EXCLUDE_FROM_ALL testreprojmulti.cpp)

  foreach (
    UTILCMD IN
    ITEMS dumpoverviews
          gdal2ogr
          gdalasyncread
          gdaltorture
          gdalflattenmask
          gdalwarpsimple
          multireadtest
          test_ogrsf
          testreprojmulti)
    # Add that include directory before all others so the gdal_version.h from it is included and not
    # gcore/gdal_version.h
    target_include_directories(${UTILCMD} PRIVATE $<TARGET_PROPERTY:gcore,BINARY_DIR>/gdal_version_full)
    gdal_standard_includes(${UTILCMD})
    target_compile_options(${UTILCMD} PRIVATE ${GDAL_CXX_WARNING_FLAGS} ${WFLAG_OLD_STYLE_CAST})
    add_dependencies(${UTILCMD} generate_gdal_version_h)
    if (MSVC OR MINGW)
      target_compile_definitions(${UTILCMD} PRIVATE -DSUPPORTS_WMAIN)
    endif ()
    if (MINGW)
      target_link_options(${UTILCMD} PRIVATE -municode)
    endif ()
    # Windows(Mingw/MSVC) link libraries
    if (CMAKE_SYSTEM_NAME MATCHES "Windows")
      target_link_libraries(${UTILCMD} PRIVATE wsock32 ws2_32 psapi)
    endif ()
    target_include_directories(
      ${UTILCMD} PRIVATE $<TARGET_PROPERTY:gdal_vrt,SOURCE_DIR> $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>
                         $<TARGET_PROPERTY:ogr_geojson,SOURCE_DIR> $<TARGET_PROPERTY:ogr_MEM,SOURCE_DIR>)
    target_link_libraries(${UTILCMD} PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}>)
  endforeach ()
endif ()
