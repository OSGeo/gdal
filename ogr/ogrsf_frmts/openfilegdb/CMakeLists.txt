add_gdal_driver(
  TARGET ogr_OpenFileGDB
  SOURCES filegdbindex.cpp
          filegdbindex_write.cpp
          filegdbtable.cpp
          filegdbtable_write.cpp
          filegdbtable_write_fields.cpp
          filegdbtable_freelist.cpp
          filegdbtable.h
          filegdbtable_priv.h
          ogr_openfilegdb.h
          ogropenfilegdbdatasource.cpp
          ogropenfilegdbdatasource_write.cpp
          ogropenfilegdb_generate_uuid.cpp
          ogropenfilegdbdriver.cpp
          ogropenfilegdblayer.cpp
          ogropenfilegdblayer_write.cpp
          gdalopenfilegdbrasterband.cpp
  CORE_SOURCES
          ogropenfilegdbdrivercore.cpp
  PLUGIN_CAPABLE NO_DEPS
  STRONG_CXX_WFLAGS
  NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET ogr_OpenFileGDB)
    return()
endif()

# because of use of GDAL_RELEASE_NAME
set_property(SOURCE filegdbtable_write.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)

if (CMAKE_CXX_FLAGS MATCHES "-fno-sanitize-recover=unsigned-integer-overflow")
    # Remove '-fno-sanitize-recover=unsigned-integer-overflow' from global flags
    string(REPLACE "-fno-sanitize-recover=unsigned-integer-overflow" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

    define_property(
        SOURCE
        PROPERTY COMPILE_FLAGS
        INHERITED
        BRIEF_DOCS "brief-doc"
        FULL_DOCS  "full-doc"
    )

    # Add '-fno-sanitize-recover=unsigned-integer-overflow' for directory
    set_directory_properties(PROPERTIES COMPILE_FLAGS "-fno-sanitize-recover=unsigned-integer-overflow")

    # Remove '-fno-sanitize-recover=unsigned-integer-overflow' from source file
    set_source_files_properties("ogropenfilegdb_generate_uuid.cpp" PROPERTIES COMPILE_FLAGS "")
endif()


gdal_standard_includes(ogr_OpenFileGDB)
target_include_directories(ogr_OpenFileGDB PRIVATE $<TARGET_PROPERTY:ogr_MEM,SOURCE_DIR>)

add_executable(test_ofgdb_write EXCLUDE_FROM_ALL
               test_ofgdb_write.cpp
               filegdbindex.cpp
               filegdbtable.cpp
               filegdbtable_write.cpp
               filegdbtable_write_fields.cpp
               filegdbindex_write.cpp
               filegdbtable_freelist.cpp)
gdal_standard_includes(test_ofgdb_write)
target_link_libraries(test_ofgdb_write PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}>)
