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
          zarr_v3_codec_vlen_utf8.cpp
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

find_path(PCODEC_C_INCLUDE_DIR cpcodec.h)
find_library(PCODEC_C_LIBRARY cpcodec)

if (PCODEC_C_INCLUDE_DIR AND PCODEC_C_LIBRARY)
    set(GDAL_USE_PCODEC_DEFAULT ON)
else()
    set(GDAL_USE_PCODEC_DEFAULT OFF)
endif()

option(GDAL_USE_PCODEC "Whether to use cpcodec Rust crate (Apache 2 licensed) for Zarr. If setting this to ON, and that PCODEC_C_INCLUDE_DIR or PCODEC_C_LIBRARY are not defined, https://github.com/corrosion-rs/corrosion.git and https://github.com/pcodec/pcodec.git will be downloaded" ${GDAL_USE_PCODEC_DEFAULT})

if (GDAL_USE_PCODEC AND NOT (PCODEC_C_INCLUDE_DIR AND PCODEC_C_LIBRARY))

    # Corrosion v0.6 requires CMake >= 3.22)
    if (CMAKE_VERSION VERSION_LESS 3.22)
        message(FATAL_ERROR "CMake >= 3.22 required for GDAL_USE_PCODEC=ON")
    endif()

    find_package(Git)
    find_program(CARGO cargo)
    find_program(RUSTC rustc)
    if (NOT GIT_FOUND AND ((NOT CORROSION_SOURCE_DIR) OR (NOT PCODEC_SOURCE_DIR)))
        message(FATAL_ERROR "git not available and CORROSION_SOURCE_DIR and/or PCODEC_SOURCE_DIR not set: GDAL_USE_PCODEC=ON failed")
    elseif (RUSTC AND CARGO)
        execute_process(COMMAND ${RUSTC} --version
                        RESULT_VARIABLE RUSTC_VERSION_RES)
        if(NOT RUSTC_VERSION_RES EQUAL 0)
            message(FATAL_ERROR "rustc not configured: GDAL_USE_PCODEC=ON failed")
        else()
        endif()
    else()
        message(WARNING "cargo and/or rustc not found: GDAL_USE_PCODEC=ON failed")
    endif()

    include(FetchContent)

    set(CORROSION_SOURCE_DIR "" CACHE STRING
        "Source directory to content from https://github.com/corrosion-rs/corrosion.git (known to work with 0.6.1)")
    if (CORROSION_SOURCE_DIR)
        FetchContent_Declare(
            Corrosion
            SOURCE_DIR ${CORROSION_SOURCE_DIR}
        )
    else()
        # v0.6.1
        set(CORROSION_TAG "1499b14e4906a2890f5cee1547c8848db261753d" CACHE STRING
            "Git tag/sha1sum for from https://github.com/corrosion-rs/corrosion.git")
        FetchContent_Declare(
            Corrosion
            GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
            GIT_TAG ${CORROSION_TAG}
        )
    endif()

    # Set any global configuration variables such as `Rust_TOOLCHAIN` before this line!
    FetchContent_MakeAvailable(Corrosion)

    set(PCODEC_SOURCE_DIR "" CACHE STRING
        "Source directory to content from https://github.com/pcodec/pcodec.git (known to work with v1.0.2)")
    if (PCODEC_SOURCE_DIR)
        FetchContent_Declare(
            pcodec
            SOURCE_DIR ${PCODEC_SOURCE_DIR}
        )
    else()
        # v1.0.2
        set(PCODEC_TAG "2d8555888b21bbaa19326580b740fa24b7da6bd3" CACHE STRING
            "Git tag/sha1sum for from https://github.com/pcodec/pcodec.git")
        FetchContent_Declare(
            pcodec
            GIT_REPOSITORY https://github.com/pcodec/pcodec.git
            GIT_TAG ${PCODEC_TAG}
        )
    endif()

    FetchContent_MakeAvailable(pcodec)

    set(PCODEC_PATCHED_SOURCE_DIR "${CMAKE_BINARY_DIR}/pcodec")
    file(COPY "${pcodec_SOURCE_DIR}/" DESTINATION "${PCODEC_PATCHED_SOURCE_DIR}")

    # We patch pco_c/Cargo.toml because corrosion_import_crate() only works with
    # staticlib or cdylib crates
    file(READ
        "${PCODEC_PATCHED_SOURCE_DIR}/pco_c/Cargo.toml"
        CARGO_TOML
    )

    set(PCODEC_BUILD_TYPE_DEFAULT "staticlib")
    set(PCODEC_BUILD_TYPE "${PCODEC_BUILD_TYPE_DEFAULT}" CACHE STRING
        "Type of pcodec build (staticlib or cdylib)")
    mark_as_advanced(PCODEC_BUILD_TYPE)

    string(REPLACE
        "crate-type = [\"dylib\"]"
        "crate-type = [\"${PCODEC_BUILD_TYPE}\"]\n# patched"
        CARGO_TOML
        "${CARGO_TOML}"
    )

    file(WRITE
        "${PCODEC_PATCHED_SOURCE_DIR}/pco_c/Cargo.toml"
        "${CARGO_TOML}"
    )

    corrosion_import_crate(
        MANIFEST_PATH ${PCODEC_PATCHED_SOURCE_DIR}/Cargo.toml
        CRATES  cpcodec
    )

    set(PCODEC_C_INCLUDE_DIR "${PCODEC_PATCHED_SOURCE_DIR}/pco_c/include")
    set(PCODEC_C_LIBRARY "cpcodec")

    install(FILES ${pcodec_SOURCE_DIR}/README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/cpcodec)
    install(FILES ${pcodec_SOURCE_DIR}/LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/cpcodec)
endif()

if(GDAL_USE_PCODEC AND PCODEC_C_INCLUDE_DIR AND PCODEC_C_LIBRARY)
    target_sources(gdal_Zarr PRIVATE zarr_v3_codec_pcodec.cpp)
    target_compile_definitions(gdal_Zarr PRIVATE -DHAVE_PCODEC)
    target_include_directories(gdal_Zarr PRIVATE "${PCODEC_C_INCLUDE_DIR}")
    gdal_target_link_libraries(gdal_Zarr PRIVATE "${PCODEC_C_LIBRARY}")
endif()
