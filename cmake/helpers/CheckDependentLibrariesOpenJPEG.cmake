# OpenJPEG's cmake-CONFIG is broken with older OpenJPEG releases, so call module explicitly
set(GDAL_FIND_PACKAGE_OpenJPEG_MODE "MODULE" CACHE STRING "Mode to use for find_package(OpenJPEG): CONFIG, MODULE or empty string")
set_property(CACHE GDAL_FIND_PACKAGE_OpenJPEG_MODE PROPERTY STRINGS "CONFIG" "MODULE" "")
# "openjp2" target name is for the one coming from the OpenJPEG CMake configuration
# "OPENJPEG::OpenJPEG" is the one used by cmake/modules/packages/FindOpenJPEG.cmake
gdal_check_package(OpenJPEG "Enable JPEG2000 support with OpenJPEG library"
                   ${GDAL_FIND_PACKAGE_OpenJPEG_MODE}
                   CAN_DISABLE
                   TARGETS "openjp2;OPENJPEG::OpenJPEG"
                   VERSION "2.3.1")
