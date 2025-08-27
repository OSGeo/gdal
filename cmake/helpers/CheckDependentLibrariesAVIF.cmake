define_find_package2(AVIF avif/avif.h avif PKGCONFIG_NAME libavif)
gdal_check_package(AVIF "AVIF" CAN_DISABLE)

# Check if libavif supports opaque properties
include(CheckCXXSourceCompiles)
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_INCLUDES "${AVIF_INCLUDE_DIRS}")
check_cxx_source_compiles(
    "
    #include <avif/avif.h>
    int main()
    {
        offsetof(avifImage, numProperties);
        return 0;
    }
    "
    AVIF_HAS_OPAQUE_PROPERTIES
)
cmake_pop_check_state()

if (AVIF_HAS_OPAQUE_PROPERTIES)
    set_property(TARGET AVIF::AVIF APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "AVIF_HAS_OPAQUE_PROPERTIES")
endif ()

option(AVIF_VERSION_CHECK "Check libavif runtime vs compile-time versions" ON)
mark_as_advanced(AVIF_VERSION_CHECK)
if (HAVE_AVIF AND AVIF_VERSION_CHECK)
    set_property(TARGET AVIF::AVIF APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "AVIF_VERSION_CHECK")
endif ()
