# CMake4GDAL project is distributed under MIT license. See accompanying file LICENSE.txt.

add_library(
  alg OBJECT
  contour.cpp
  delaunay.c
  gdal_crs.cpp
  gdal_homography.cpp
  gdal_octave.cpp
  gdal_rpc.cpp
  gdal_tps.cpp
  gdalapplyverticalshiftgrid.cpp
  gdalchecksum.cpp
  gdalcutline.cpp
  gdaldither.cpp
  gdalgeoloc.cpp
  gdalgeolocquadtree.cpp
  gdalgrid.cpp
  gdallinearsystem.cpp
  gdalmatching.cpp
  gdalmediancut.cpp
  gdalpansharpen.cpp
  gdalproximity.cpp
  gdalrasterize.cpp
  gdalrasterpolygonenumerator.cpp
  gdalsievefilter.cpp
  gdalsimplewarp.cpp
  gdaltransformer.cpp
  gdaltransformgeolocs.cpp
  gdalwarper.cpp
  gdalwarpkernel.cpp
  gdalwarpoperation.cpp
  hilbert.cpp
  llrasterize.cpp
  los.cpp
  polygonize.cpp
  polygonize_polygonizer_impl.cpp
  rasterfill.cpp
  thinplatespline.cpp
  gdal_simplesurf.cpp
  viewshed/combiner.cpp
  viewshed/cumulative.cpp
  viewshed/progress.cpp
  viewshed/util.cpp
  viewshed/viewshed.cpp
  viewshed/viewshed_executor.cpp
  zonal.cpp
  gdalgenericinverse.cpp
  gdal_interpolateatpoint.cpp
)

add_dependencies(alg generate_gdal_version_h)

include(GdalStandardIncludes)
include(GdalDriverHelper)

gdal_standard_includes(alg)
target_compile_options(alg PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${GDAL_CXX_WARNING_FLAGS} ${WFLAG_OLD_STYLE_CAST}
                                   ${WFLAG_EFFCXX}> ${WFLAG_DOUBLE_PROMOTION})
target_compile_options(alg PRIVATE $<$<COMPILE_LANGUAGE:C>:${GDAL_C_WARNING_FLAGS} ${WFLAG_DOUBLE_PROMOTION}>)

target_compile_definitions(alg PUBLIC $<$<CONFIG:DEBUG>:GDAL_DEBUG>)
if (USE_PRECOMPILED_HEADERS)
    target_precompile_headers(alg REUSE_FROM gdal_priv_header)
endif()

target_include_directories(
  alg PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/marching_squares>
              $<TARGET_PROPERTY:gdal_vrt,SOURCE_DIR> $<TARGET_PROPERTY:gdal_MEM,SOURCE_DIR>)
set_property(TARGET alg PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:alg>)

if (GDAL_ENABLE_DRIVER_GTIFF)
  target_include_directories(alg PRIVATE $<TARGET_PROPERTY:gdal_GTIFF,SOURCE_DIR>)
endif()

if (GDAL_USE_GEOS)
  target_compile_definitions(alg PRIVATE -DHAVE_GEOS=1)
  gdal_target_link_libraries(alg PRIVATE ${GEOS_TARGET})
endif ()

if (GDAL_USE_ARMADILLO)
  target_include_directories(alg PRIVATE ${ARMADILLO_INCLUDE_DIRS})
  target_compile_definitions(alg PRIVATE -DHAVE_ARMADILLO)
  gdal_target_link_libraries(alg PRIVATE ${ARMADILLO_LIBRARIES})
endif ()

gdal_target_link_libraries(alg PRIVATE PROJ::proj)

if (GDAL_USE_QHULL_INTERNAL)
  target_compile_definitions(alg PRIVATE -DINTERNAL_QHULL)
  target_sources(alg PRIVATE internal_qhull_headers.h)
  target_include_directories(alg PRIVATE internal_libqhull)
elseif (GDAL_USE_QHULL)
  target_compile_definitions(alg PRIVATE -DEXTERNAL_QHULL)
  target_include_directories(alg PRIVATE ${QHULL_INCLUDE_DIR})
  gdal_target_link_libraries(alg PRIVATE ${QHULL_LIBRARY})
endif ()

if (GDAL_ENABLE_ARM_NEON_OPTIMIZATIONS)
  target_compile_definitions(alg PRIVATE -DHAVE_SSE_AT_COMPILE_TIME -DUSE_NEON_OPTIMIZATIONS)
  target_sources(alg PRIVATE gdalgridsse.cpp)
endif()

if (HAVE_SSE_AT_COMPILE_TIME)
  add_library(alg_gdalgridsse OBJECT gdalgridsse.cpp)
  add_dependencies(alg_gdalgridsse generate_gdal_version_h)
  target_compile_definitions(alg_gdalgridsse PRIVATE -DHAVE_SSE_AT_COMPILE_TIME)
  target_compile_options(alg_gdalgridsse PRIVATE ${WFLAG_DOUBLE_PROMOTION})
  gdal_standard_includes(alg_gdalgridsse)
  set_property(TARGET alg_gdalgridsse PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})
  target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:alg_gdalgridsse>)
  if (NOT "${GDAL_SSE_FLAG}" STREQUAL "")
    set_property(
      SOURCE gdalgridsse.cpp
      APPEND
      PROPERTY COMPILE_FLAGS ${GDAL_SSE_FLAG})
  endif ()
endif ()
if (HAVE_AVX_AT_COMPILE_TIME)
  add_library(alg_gdalgridavx OBJECT gdalgridavx.cpp)
  add_dependencies(alg_gdalgridavx generate_gdal_version_h)
  target_compile_definitions(alg_gdalgridavx PRIVATE -DHAVE_AVX_AT_COMPILE_TIME)
  target_compile_options(alg_gdalgridavx PRIVATE ${WFLAG_DOUBLE_PROMOTION})
  gdal_standard_includes(alg_gdalgridavx)
  set_property(TARGET alg_gdalgridavx PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})
  target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:alg_gdalgridavx>)
  if (NOT "${GDAL_AVX_FLAG}" STREQUAL "")
    set_property(
      SOURCE gdalgridavx.cpp
      APPEND
      PROPERTY COMPILE_FLAGS ${GDAL_AVX_FLAG})
  endif ()
endif ()

include(TargetPublicHeader)
target_public_header(
  TARGET
  gdal
  HEADERS
  gdal_alg.h
  gdal_alg_priv.h
  gdalgrid.h
  gdalgrid_priv.h
  gdalwarper.h
  gdal_simplesurf.h
  gdalpansharpen.h)
