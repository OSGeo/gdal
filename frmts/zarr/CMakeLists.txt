add_gdal_driver(
  TARGET gdal_Zarr
  SOURCES zarr_array.cpp
          zarr_v2_array.cpp
          zarr_v3_array.cpp
          zarr_compressors.cpp
          zarr_filters.cpp
          zarr_attribute.cpp
          zarr_v3_codec.cpp
          zarr_v3_codec_abstract_compressor.cpp
          zarr_v3_codec_blosc.cpp
          zarr_v3_codec_bytes.cpp
          zarr_v3_codec_crc32c.cpp
          zarr_v3_codec_gzip.cpp
          zarr_v3_codec_sequence.cpp
          zarr_v3_codec_sharding.cpp
          zarr_v3_codec_transpose.cpp
          zarr_v3_codec_zstd.cpp
          zarr_dimension.cpp
          zarr_group.cpp
          zarr_v2_group.cpp
          zarr_v3_group.cpp
          zarr_sharedresource.cpp
          zarrdriver.cpp
          vsikerchunk.cpp
          vsikerchunk_json_ref.cpp
          vsikerchunk_parquet_ref.cpp
          crc32c.cpp
  CORE_SOURCES
          zarrdrivercore.cpp
  PLUGIN_CAPABLE
  NO_DEPS
  NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET gdal_Zarr)
    return()
endif()

gdal_standard_includes(gdal_Zarr)
target_include_directories(gdal_Zarr PRIVATE ${GDAL_RASTER_FORMAT_SOURCE_DIR}/netcdf)

if (GDAL_USE_BLOSC)
  target_compile_definitions(gdal_Zarr PRIVATE -DHAVE_BLOSC)
  gdal_target_link_libraries(gdal_Zarr PRIVATE Blosc::Blosc)
endif ()
