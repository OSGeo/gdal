add_gdal_driver(TARGET gdal_TileDB SOURCES tiledbdataset.cpp PLUGIN_CAPABLE)
gdal_standard_includes(gdal_TileDB)
target_compile_definitions(gdal_TileDB PRIVATE -DTILEDB_DEPRECATED=)

# TileDB 2.7 requires C++17
get_property(_dirs TARGET TileDB::tiledb_shared PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
foreach(_dir IN LISTS _dirs)
  set(TILEDB_VERSION_FILENAME "${_dir}/tiledb/tiledb_version.h")
  if(EXISTS ${TILEDB_VERSION_FILENAME})
    file(READ ${TILEDB_VERSION_FILENAME} _tiledb_version_contents)
    string(REGEX REPLACE "^.*TILEDB_VERSION_MAJOR +([0-9]+).*$" "\\1" TILEDB_VERSION_MAJOR "${_tiledb_version_contents}")
    string(REGEX REPLACE "^.*TILEDB_VERSION_MINOR +([0-9]+).*$" "\\1" TILEDB_VERSION_MINOR "${_tiledb_version_contents}")
    if("${TILEDB_VERSION_MAJOR}.${TILEDB_VERSION_MINOR}" VERSION_GREATER_EQUAL "2.7")
      target_compile_features(gdal_TileDB PRIVATE cxx_std_17)
    endif()
  endif()
endforeach()

gdal_target_link_libraries(gdal_TileDB PRIVATE TileDB::tiledb_shared)
