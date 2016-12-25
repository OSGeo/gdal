# first include proj library
# always need
include(lib_proj.cmake)

# configure executable build
option(BUILD_CS2CS "Build cs2cs (coordinate systems to coordinate systems translation tool)" ON)
option(BUILD_PROJ "Build proj (cartographic projection tool : latlong <-> projected coordinates" ON)
option(BUILD_GEOD "Build geod (computation of geodesic lines)" ON)
option(BUILD_NAD2BIN "Build nad2bin (format conversion tool) " ON)

if(NOT MSVC)
  if (NOT APPLE)
    # Use relative path so that package is relocatable
    set(CMAKE_INSTALL_RPATH "\$ORIGIN/../${LIBDIR}")
  else ()
    set (CMAKE_INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/${LIBDIR}")
    # TO DO: cmake 2.8.12 introduces a way to make the install tree
    # relocatable with OSX via
    # (1) set(CMAKE_MACOSX_RPATH ON) and
    # (2) setting the INSTALL_RPATH property on the executables to
    # "@loader_path/../${LIBDIR}"
  endif ()
endif ()

if(BUILD_CS2CS)
 include(bin_cs2cs.cmake)
endif(BUILD_CS2CS)

if(BUILD_PROJ)
  include(bin_proj.cmake)
endif(BUILD_PROJ)

if(BUILD_GEOD)
  include(bin_geod.cmake)
  include(bin_geodtest.cmake)
endif(BUILD_GEOD)

if(BUILD_NAD2BIN)
  include(bin_nad2bin.cmake)
endif(BUILD_NAD2BIN)

if (MSVC OR CMAKE_CONFIGURATION_TYPES)
  # Add _d suffix for your debug versions of the tools
  set_target_properties (cs2cs binproj geod nad2bin PROPERTIES
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
endif ()
