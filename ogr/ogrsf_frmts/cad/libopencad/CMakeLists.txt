add_library(
  libopencad STATIC
  dwg/io.cpp
  dwg/r2000.cpp
  cadclasses.cpp
  cadcolors.cpp
  caddictionary.cpp
  cadfile.cpp
  cadfileio.cpp
  cadgeometry.cpp
  cadfilestreamio.cpp
  cadheader.cpp
  cadlayer.cpp
  cadobjects.cpp
  cadtables.cpp
  opencad.cpp)
target_compile_definitions(libopencad PRIVATE "OCAD_EXTERN=")
target_include_directories(
  libopencad PRIVATE $<TARGET_PROPERTY:cpl,SOURCE_DIR> $<TARGET_PROPERTY:cpl,BINARY_DIR>
                     $<TARGET_PROPERTY:ogr_CAD,SOURCE_DIR> ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/dwg)
set_target_properties(libopencad PROPERTIES POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS} ARCHIVE_OUTPUT_DIRECTORY
                                                                                           ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(GDAL_LINK_LIBRARY INTERFACE libopencad)
