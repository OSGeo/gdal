add_gdal_driver(
  TARGET gdal_Icechunk
  SOURCES icechunkdriver.cpp
          icechunkmanifest.cpp
          icechunkrepo.cpp
          icechunksnapshot.cpp
          icechunkutils.cpp
          vsiicechunk.cpp
  CORE_SOURCES
          icechunkdrivercore.cpp
  PLUGIN_CAPABLE
  NO_DEPS
  NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET gdal_Icechunk)
    return()
endif()

# Can optionally use std::ranges::lower_bound()
# target_compile_features(gdal_Icechunk PRIVATE cxx_std_20)

gdal_standard_includes(gdal_Icechunk)

# Disable Unity builds therwise we get errrs n Winws like
# error C2248: 'gdal_flexbuffers::Reference::byte_width_': cannot access private member declared in class 'gdal_flexbuffers::Reference'
set_property(TARGET gdal_Icechunk PROPERTY UNITY_BUILD OFF)

target_include_directories(gdal_Icechunk PRIVATE ${PROJECT_SOURCE_DIR}/third_party)

# Quick and dirty way of modifying the default flatbuffers namespace to gdal_flatbuffers
target_compile_definitions(gdal_Icechunk PRIVATE -Dflatbuffers=gdal_flatbuffers)
target_compile_definitions(gdal_Icechunk PRIVATE -Dflexbuffers=gdal_flexbuffers)
target_compile_definitions(gdal_Icechunk PRIVATE -Dgenerated=gdal_generated_icechunk)
target_compile_definitions(gdal_Icechunk PRIVATE FLATBUFFERS_LOCALE_INDEPENDENT=0)

gdal_target_link_libraries(gdal_Icechunk PRIVATE ${ZSTD_TARGET})
