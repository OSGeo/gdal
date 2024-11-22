define_find_package2(AVIF avif/avif.h avif PKGCONFIG_NAME libavif)
gdal_check_package(AVIF "AVIF" CAN_DISABLE)

# Check if libavif supports opaque properties
include(CheckCXXSourceCompiles)
check_cxx_source_compiles(
    "
    #include <avif/avif.h>
    int main()
    {
        avifImage *image;
        image->numProperties;
        return 0;
    }
    "
    AVIF_HAS_OPAQUE_PROPERTIES
)
if (AVIF_HAS_OPAQUE_PROPERTIES)
    set_property(TARGET AVIF::AVIF APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "AVIF_HAS_OPAQUE_PROPERTIES")
endif ()
