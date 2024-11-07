gdal_check_package(JPEG "JPEG compression library (external)" CAN_DISABLE RECOMMENDED)
if (GDAL_USE_JPEG AND (JPEG_LIBRARY MATCHES ".*turbojpeg\.(so|lib)"))
  message(
    FATAL_ERROR
      "JPEG_LIBRARY should point to a library with libjpeg ABI, not TurboJPEG. See https://libjpeg-turbo.org/About/TurboJPEG for the difference"
    )
endif ()
if (GDAL_USE_JPEG AND TARGET JPEG::JPEG)
  set(EXPECTED_JPEG_LIB_VERSION "" CACHE STRING "Expected libjpeg version number")
  mark_as_advanced(GDAL_CHECK_PACKAGE_${name}_NAMES)
  if (EXPECTED_JPEG_LIB_VERSION)
    get_property(_jpeg_old_icd TARGET JPEG::JPEG PROPERTY INTERFACE_COMPILE_DEFINITIONS)
    set_property(TARGET JPEG::JPEG PROPERTY
                 INTERFACE_COMPILE_DEFINITIONS "${_jpeg_old_icd};EXPECTED_JPEG_LIB_VERSION=${EXPECTED_JPEG_LIB_VERSION}")
  endif()

  # Check for jpeg12_read_scanlines() which has been added in libjpeg-turbo 2.2
  # for dual 8/12 bit mode.
  include(CheckCSourceCompiles)
  include(CMakePushCheckState)
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_INCLUDES "${JPEG_INCLUDE_DIRS}")
  set(CMAKE_REQUIRED_LIBRARIES "${JPEG_LIBRARIES}")
  check_c_source_compiles(
      "
      #include <stddef.h>
      #include <stdio.h>
      #include \"jpeglib.h\"
      int main()
      {
          jpeg_read_scanlines(0,0,0);
          jpeg12_read_scanlines(0,0,0);
          return 0;
      }
      "
      HAVE_JPEGTURBO_DUAL_MODE_8_12)
  cmake_pop_check_state()

endif()
gdal_internal_library(JPEG)
