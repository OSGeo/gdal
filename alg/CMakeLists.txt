# CMake4GDAL project is distributed under X/MIT license. See accompanying file LICENSE.txt.

add_library(
  alg OBJECT
  contour.cpp
  delaunay.c
  gdal_crs.cpp
  gdal_octave.cpp
  gdal_rpc.cpp
  gdal_tps.cpp
  gdalapplyverticalshiftgrid.cpp
  gdalchecksum.cpp
  gdalcutline.cpp
  gdaldither.cpp
  gdalgeoloc.cpp
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
  llrasterize.cpp
  polygonize.cpp
  rasterfill.cpp
  thinplatespline.cpp
  gdal_simplesurf.cpp
  viewshed.cpp)

add_dependencies(alg generate_gdal_version_h)

include(GdalStandardIncludes)
gdal_standard_includes(alg)
target_compile_options(alg PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${GDAL_CXX_WARNING_FLAGS} ${WFLAG_OLD_STYLE_CAST}
                                   ${WFLAG_EFFCXX}>)
target_compile_options(alg PRIVATE $<$<COMPILE_LANGUAGE:C>:${GDAL_C_WARNING_FLAGS}>)

target_include_directories(
  alg PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/marching_squares> $<TARGET_PROPERTY:gdal_GTIFF,SOURCE_DIR>
              $<TARGET_PROPERTY:gdal_vrt,SOURCE_DIR> $<TARGET_PROPERTY:gdal_MEM,SOURCE_DIR>)
set_property(TARGET alg PROPERTY POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:alg>)
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
  set_property(SOURCE gdaldither.cpp gdalmediancut.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endif ()

if (GDAL_USE_GEOS)
  target_include_directories(alg PRIVATE ${GEOS_INCLUDE_DIRS})
  target_compile_definitions(alg PRIVATE -DHAVE_GEOS=1)
  gdal_add_private_link_libraries(GEOS::GEOS)
endif ()

if (GDAL_USE_ARMADILLO)
  target_include_directories(alg PRIVATE ${ARMADILLO_INCLUDE_DIRS})
  target_compile_definitions(alg PRIVATE -DHAVE_ARMADILLO)
  gdal_add_private_link_libraries(${ARMADILLO_LIBRARIES})
endif ()

if (GDAL_USE_OPENCL)
  target_include_directories(alg PRIVATE ${OpenCL_INCLUDE_DIRS})
  gdal_add_private_link_libraries(OpenCL::OpenCL)
  target_compile_definitions(alg PRIVATE -DHAVE_OPENCL)
  target_sources(alg PRIVATE gdalwarpkernel_opencl.h gdalwarpkernel_opencl.cpp)
endif ()

target_include_directories(alg PRIVATE ${PROJ_INCLUDE_DIRS})
gdal_add_private_link_libraries(PROJ::PROJ)

if (GDAL_ENABLE_QHULL)
  if (GDAL_USE_QHULL_INTERNAL)
    target_compile_definitions(alg PRIVATE -DINTERNAL_QHULL)
    target_sources(alg PRIVATE internal_qhull_headers.h)
    target_include_directories(alg PRIVATE $<TARGET_PROPERTY:qhull,SOURCE_DIR>)
  else ()
    target_compile_definitions(alg PRIVATE -DEXTERNAL_QHULL)
    if (QHULL_INCLUDE_SUBDIR STREQUAL "libqhull")
      target_compile_definitions(alg PRIVATE -DQHULL_INCLUDE_SUBDIR_IS_LIBQHULL=1)
    else ()
      target_compile_definitions(alg PRIVATE -DQHULL_INCLUDE_SUBDIR_IS_LIBQHULL=0)
    endif ()
    gdal_add_private_link_libraries(${QHULL_LIBRARY})
  endif ()
endif ()

if (HAVE_SSE_AT_COMPILE_TIME)
  target_sources(alg PRIVATE gdalgridsse.cpp)
  target_compile_definitions(alg PRIVATE -DHAVE_SSE_AT_COMPILE_TIME)
  if (NOT "${GDAL_SSE_FLAG}" STREQUAL "")
    set_property(
      SOURCE gdalgridsse.cpp
      APPEND
      PROPERTY COMPILE_FLAGS ${GDAL_SSE_FLAG})
  endif ()
endif ()
if (HAVE_AVX_AT_COMPILE_TIME)
  target_sources(alg PRIVATE gdalgridavx.cpp)
  target_compile_definitions(alg PRIVATE -DHAVE_AVX_AT_COMPILE_TIME)
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
