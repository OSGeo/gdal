# CMake4GDAL project is distributed under MIT license. See accompanying file LICENSE.txt.

include(GdalStandardIncludes)
add_library(
  appslib OBJECT
  commonutils.h
  gdal_utils.h
  gdalargumentparser.cpp
  gdalalg_main.cpp
  gdalgetgdalpath.cpp
  gdalinfo_lib.cpp
  gdalbuildvrt_lib.cpp
  gdal_grid_lib.cpp
  gdal_translate_lib.cpp
  gdal_rasterize_lib.cpp
  gdal_contour_lib.cpp
  gdalwarp_lib.cpp
  commonutils.cpp
  ogrinfo_lib.cpp
  ogr2ogr_lib.cpp
  gdaldem_lib.cpp
  nearblack_lib.cpp
  nearblack_lib_floodfill.cpp
  gdal_footprint_lib.cpp
  gdalmdiminfo_lib.cpp
  gdalmdimtranslate_lib.cpp
  gdaltindex_lib.cpp)

if(GDAL_ENABLE_ALGORITHMS)
target_sources(appslib PRIVATE
  gdalalg_abstract_pipeline.cpp
  gdalalg_clip_common.cpp
  gdalalg_convert.cpp
  gdalalg_info.cpp
  gdalalg_dataset.cpp
  gdalalg_dataset_identify.cpp
  gdalalg_dataset_copy.cpp
  gdalalg_dataset_rename.cpp
  gdalalg_dataset_delete.cpp
  gdalalg_materialize.cpp
  gdalalg_mdim.cpp
  gdalalg_mdim_info.cpp
  gdalalg_mdim_convert.cpp
  gdalalg_mdim_mosaic.cpp
  gdalalg_pipeline.cpp
  gdalalg_raster.cpp
  gdalalg_raster_info.cpp
  gdalalg_raster_as_features.cpp
  gdalalg_raster_aspect.cpp
  gdalalg_raster_blend.cpp
  gdalalg_raster_calc.cpp
  gdalalg_raster_clean_collar.cpp
  gdalalg_raster_clip.cpp
  gdalalg_raster_color_map.cpp
  gdalalg_raster_compare.cpp
  gdalalg_raster_convert.cpp
  gdalalg_raster_create.cpp
  gdalalg_raster_contour.cpp
  gdalalg_raster_edit.cpp
  gdalalg_raster_footprint.cpp
  gdalalg_raster_fill_nodata.cpp
  gdalalg_raster_hillshade.cpp
  gdalalg_raster_index.cpp
  gdalalg_raster_mosaic.cpp
  gdalalg_raster_mosaic_stack_common.cpp
  gdalalg_raster_neighbors.cpp
  gdalalg_raster_nodata_to_alpha.cpp
  gdalalg_raster_pipeline.cpp
  gdalalg_raster_pixel_info.cpp
  gdalalg_raster_polygonize.cpp
  gdalalg_raster_proximity.cpp
  gdalalg_raster_overview_add.cpp
  gdalalg_raster_overview_delete.cpp
  gdalalg_raster_overview_refresh.cpp
  gdalalg_raster_pansharpen.cpp
  gdalalg_raster_rgb_to_palette.cpp
  gdalalg_raster_read.cpp
  gdalalg_raster_reclassify.cpp
  gdalalg_raster_reproject.cpp
  gdalalg_raster_resize.cpp
  gdalalg_raster_roughness.cpp
  gdalalg_raster_scale.cpp
  gdalalg_raster_select.cpp
  gdalalg_raster_set_type.cpp
  gdalalg_raster_sieve.cpp
  gdalalg_raster_slope.cpp
  gdalalg_raster_stack.cpp
  gdalalg_raster_tile.cpp
  gdalalg_raster_tpi.cpp
  gdalalg_raster_tri.cpp
  gdalalg_raster_unscale.cpp
  gdalalg_raster_update.cpp
  gdalalg_raster_viewshed.cpp
  gdalalg_raster_write.cpp
  gdalalg_raster_zonal_stats.cpp
  gdalalg_tee.cpp
  gdalalg_vector.cpp
  gdalalg_vector_info.cpp
  gdalalg_vector_check_coverage.cpp
  gdalalg_vector_check_geometry.cpp
  gdalalg_vector_clip.cpp
  gdalalg_vector_clean_coverage.cpp
  gdalalg_vector_concat.cpp
  gdalalg_vector_convert.cpp
  gdalalg_vector_edit.cpp
  gdalalg_vector_pipeline.cpp
  gdalalg_vector_rasterize.cpp
  gdalalg_vector_read.cpp
  gdalalg_vector_filter.cpp
  gdalalg_vector_geom.cpp
  gdalalg_vector_limit.cpp
  gdalalg_vector_make_point.cpp
  gdalalg_vector_set_geom_type.cpp
  gdalalg_vector_explode_collections.cpp
  gdalalg_vector_make_valid.cpp
  gdalalg_vector_segmentize.cpp
  gdalalg_vector_simplify.cpp
  gdalalg_vector_simplify_coverage.cpp
  gdalalg_vector_buffer.cpp
  gdalalg_vector_swap_xy.cpp
  gdalalg_vector_grid.cpp
  gdalalg_vector_grid_average.cpp
  gdalalg_vector_grid_data_metrics.cpp
  gdalalg_vector_grid_invdist.cpp
  gdalalg_vector_grid_invdistnn.cpp
  gdalalg_vector_grid_linear.cpp
  gdalalg_vector_grid_nearest.cpp
  gdalalg_vector_index.cpp
  gdalalg_vector_layer_algebra.cpp
  gdalalg_vector_output_abstract.cpp
  gdalalg_vector_partition.cpp
  gdalalg_vector_reproject.cpp
  gdalalg_vector_select.cpp
  gdalalg_vector_set_field_type.cpp
  gdalalg_vector_sort.cpp
  gdalalg_vector_sql.cpp
  gdalalg_vector_update.cpp
  gdalalg_vector_write.cpp
  gdalalg_vsi.cpp
  gdalalg_vsi_copy.cpp
  gdalalg_vsi_delete.cpp
  gdalalg_vsi_list.cpp
  gdalalg_vsi_move.cpp
  gdalalg_vsi_sozip.cpp
  gdalalg_vsi_sync.cpp
)
endif()

add_dependencies(appslib generate_gdal_version_h)
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:appslib>)
gdal_standard_includes(appslib)
target_compile_options(appslib PRIVATE ${GDAL_CXX_WARNING_FLAGS} ${WFLAG_OLD_STYLE_CAST} ${WFLAG_EFFCXX} ${WFLAG_DOUBLE_PROMOTION})
target_include_directories(
  appslib PRIVATE $<TARGET_PROPERTY:gdal_vrt,SOURCE_DIR> $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)
if (HAVE_ATOMIC_UINT64_T_WITH_ATOMIC)
    gdal_target_link_libraries(appslib PRIVATE atomic)
endif()
gdal_target_link_libraries(appslib PRIVATE PROJ::proj)

if (GDAL_ENABLE_ARM_NEON_OPTIMIZATIONS)
  target_compile_definitions(appslib PRIVATE -DUSE_NEON_OPTIMIZATIONS)
endif()

if (NOT BUILD_SHARED_LIBS)
  target_compile_definitions(appslib PRIVATE STATIC_BUILD)
endif()

set_property(TARGET appslib PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})

if (GDAL_USE_GEOS)
  target_compile_definitions(appslib PRIVATE -DHAVE_GEOS=1)
  target_include_directories(appslib PRIVATE $<TARGET_PROPERTY:${GEOS_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)
endif ()

if (GDAL_ENABLE_DRIVER_GTIFF)
  target_compile_definitions(appslib PRIVATE -DHAVE_TIFF)
endif()

target_compile_definitions(appslib PUBLIC $<$<CONFIG:DEBUG>:GDAL_DEBUG>)

if (NOT GDAL_ENABLE_DRIVER_GTI OR GDAL_ENABLE_DRIVER_GTI_PLUGIN)
    target_compile_definitions(appslib PRIVATE -DGTI_DRIVER_DISABLED_OR_PLUGIN)
endif()

if (GDAL_USE_JSONC_INTERNAL)
  gdal_add_vendored_lib(appslib libjson)
else ()
  gdal_target_link_libraries(appslib PRIVATE ${JSONC_TARGET})
endif ()

if (GDAL_USE_ZLIB_INTERNAL)
  gdal_add_vendored_lib(appslib libz)
else()
  target_compile_definitions(appslib PRIVATE -DHAVE_ZLIB_H -DHAVE_ZLIB)
  if (MSVC AND NOT ZLIB_IS_STATIC)
    target_compile_definitions(appslib PRIVATE -DZLIB_DLL)
  endif ()
  gdal_target_link_libraries(appslib PRIVATE ZLIB::ZLIB)
endif ()

include(TargetPublicHeader)
target_public_header(TARGET ${GDAL_LIB_TARGET_NAME} HEADERS gdal_utils.h)

if (BUILD_APPS)
  # Default Apps

  # MSBuild has case-insensitive target names, so as GDAL is used as the
  # target name for the library, we cannot reuse it for the "gdal" binary.
  add_executable(gdal_app gdal_utils_priv.h gdal.cpp)
  set_target_properties(gdal_app PROPERTIES OUTPUT_NAME gdal)

  add_executable(gdalinfo gdal_utils_priv.h gdalinfo_bin.cpp)
  add_executable(gdalbuildvrt gdal_utils_priv.h gdalbuildvrt_bin.cpp)
  add_executable(gdaladdo gdal_utils_priv.h gdaladdo.cpp)
  add_executable(gdal_grid gdal_utils_priv.h commonutils.h gdal_grid_bin.cpp)
  add_executable(gdal_translate gdal_utils_priv.h commonutils.h gdal_translate_bin.cpp)
  add_executable(gdalwarp commonutils.h gdalwarp_bin.cpp)
  add_executable(gdal_contour gdal_utils_priv.h commonutils.h gdal_contour_bin.cpp)
  add_executable(gdalenhance gdalenhance.cpp)
  add_executable(gdallocationinfo gdallocationinfo.cpp)
  add_executable(gdalmanage gdalmanage.cpp)
  add_executable(gdalsrsinfo gdalsrsinfo.cpp)
  target_link_libraries(gdalsrsinfo PRIVATE PROJ::proj)
  add_executable(gdaltindex gdal_utils_priv.h gdaltindex_bin.cpp)
  add_executable(gdal_rasterize gdal_rasterize_bin.cpp)
  add_executable(gdaldem gdaldem_bin.cpp)
  add_executable(gdaltransform gdaltransform.cpp)
  add_executable(gdal_create gdal_create.cpp)
  add_executable(gdal_viewshed gdal_viewshed.cpp)
  add_executable(gdal_footprint commonutils.h gdal_footprint_bin.cpp)
  add_executable(ogrinfo commonutils.h ogrinfo_bin.cpp)
  add_executable(ogr2ogr ogr2ogr_bin.cpp)

  add_executable(ogrlineref ogrlineref.cpp)
  if (GDAL_USE_GEOS)
    target_compile_definitions(ogrlineref PRIVATE -DHAVE_GEOS=1)
  endif ()

  if(GDAL_ENABLE_ALGORITHMS)
    add_executable(ogrtindex ogrtindex.cpp)
  endif()
  add_executable(nearblack nearblack_bin.cpp)
  add_executable(gdalmdiminfo gdalmdiminfo_bin.cpp)
  add_executable(gdalmdimtranslate gdalmdimtranslate_bin.cpp)

  add_executable(sozip sozip.cpp)

  add_library(utils_common OBJECT gdalargumentparser.cpp)
  gdal_standard_includes(utils_common)
  target_compile_options(utils_common PRIVATE ${GDAL_CXX_WARNING_FLAGS} ${WFLAG_OLD_STYLE_CAST} ${WFLAG_DOUBLE_PROMOTION})
  add_dependencies(utils_common generate_gdal_version_h)

  set(APPS_TARGETS
      gdal_app
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
      gdalwarp
      gdal_contour
      gdallocationinfo
      gdal_footprint
      ogrinfo
      ogr2ogr
      gdalmdiminfo
      gdalmdimtranslate
      sozip)
  if (GDAL_ENABLE_ALGORITHMS)
    list(APPEND APPS_TARGETS ogrtindex)
  endif()
  if (ENABLE_GNM)
    add_executable(gnmanalyse gnmanalyse.cpp)
    add_executable(gnmmanage gnmmanage.cpp)
    list(APPEND APPS_TARGETS gnmanalyse gnmmanage)
    target_include_directories(gnmanalyse PRIVATE $<TARGET_PROPERTY:gnm,SOURCE_DIR>)
    target_include_directories(gnmmanage PRIVATE $<TARGET_PROPERTY:gnm,SOURCE_DIR>)
  endif ()

  add_custom_target(gdalapps DEPENDS ${APPS_TARGETS})
  foreach (UTILCMD IN ITEMS ${APPS_TARGETS})
    if (WIN32)
        target_sources(${UTILCMD} PRIVATE longpathaware.manifest)
    endif()
    # Add that include directory before all others so the gdal_version.h from it is included and not
    # gcore/gdal_version.h
    target_include_directories(${UTILCMD} PRIVATE $<TARGET_PROPERTY:gcore,BINARY_DIR>/gdal_version_full)
    gdal_standard_includes(${UTILCMD})
    target_compile_options(${UTILCMD} PRIVATE ${GDAL_CXX_WARNING_FLAGS} ${WFLAG_OLD_STYLE_CAST})
    add_dependencies(${UTILCMD} generate_gdal_version_h)
    target_include_directories(
      ${UTILCMD} PRIVATE $<TARGET_PROPERTY:gdal_vrt,SOURCE_DIR> $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)
    if (MSVC)
      set_target_properties(${UTILCMD} PROPERTIES LINK_FLAGS "wsetargv.obj")
    endif ()
    if (MINGW)
        if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.13)
            target_link_options(${UTILCMD} PRIVATE -municode)
        else()
            target_link_libraries(${UTILCMD} PRIVATE -municode)
        endif()
    endif ()
    if (MSVC OR MINGW)
      target_compile_definitions(${UTILCMD} PRIVATE -DSUPPORTS_WMAIN)
    endif ()
    target_link_libraries(${UTILCMD} PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}> utils_common)

    if (NOT GDAL_ENABLE_DRIVER_GTI OR GDAL_ENABLE_DRIVER_GTI_PLUGIN)
        target_compile_definitions(${UTILCMD} PRIVATE -DGTI_DRIVER_DISABLED_OR_PLUGIN)
    endif()

  endforeach ()
  install(TARGETS ${APPS_TARGETS} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

  # NON-Default Apps
  add_executable(dumpoverviews EXCLUDE_FROM_ALL dumpoverviews.cpp)
  add_executable(gdalflattenmask EXCLUDE_FROM_ALL gdalflattenmask.c)
  add_executable(gdaltorture EXCLUDE_FROM_ALL gdaltorture.cpp)
  add_executable(gdal2ogr EXCLUDE_FROM_ALL gdal2ogr.c)
  add_executable(gdalasyncread EXCLUDE_FROM_ALL gdalasyncread.cpp)
  add_executable(multireadtest EXCLUDE_FROM_ALL multireadtest.cpp)
  if(NOT MSVC AND CMAKE_THREAD_LIBS_INIT)
    target_link_libraries(multireadtest PRIVATE ${CMAKE_THREAD_LIBS_INIT})
  endif()
  add_executable(test_ogrsf test_ogrsf.cpp)
  add_executable(testreprojmulti EXCLUDE_FROM_ALL testreprojmulti.cpp)

  foreach (
    UTILCMD IN
    ITEMS dumpoverviews
          gdal2ogr
          gdalasyncread
          gdaltorture
          gdalflattenmask
          multireadtest
          test_ogrsf
          testreprojmulti)
    if (WIN32)
        target_sources(${UTILCMD} PRIVATE longpathaware.manifest)
    endif()
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
        if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.13)
            target_link_options(${UTILCMD} PRIVATE -municode)
        else()
            target_link_libraries(${UTILCMD} PRIVATE -municode)
        endif()
    endif ()
    # Windows(Mingw/MSVC) link libraries
    if (CMAKE_SYSTEM_NAME MATCHES "Windows")
      target_link_libraries(${UTILCMD} PRIVATE wsock32 ws2_32 psapi)
    endif ()
    target_include_directories(
      ${UTILCMD} PRIVATE $<TARGET_PROPERTY:gdal_vrt,SOURCE_DIR> $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)
    target_link_libraries(${UTILCMD} PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}>)
  endforeach ()
endif ()

set(GDAL_DATA_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/data/gdal_algorithm.schema.json
    ${CMAKE_CURRENT_SOURCE_DIR}/data/gdalinfo_output.schema.json
    ${CMAKE_CURRENT_SOURCE_DIR}/data/gdalmdiminfo_output.schema.json
    ${CMAKE_CURRENT_SOURCE_DIR}/data/ogrinfo_output.schema.json
    ${CMAKE_CURRENT_SOURCE_DIR}/data/leaflet_template.html
)
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY RESOURCE "${GDAL_DATA_FILES}")
