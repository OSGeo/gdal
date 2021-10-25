# CMake4GDAL project is distributed under X/MIT license. See accompanying file LICENSE.txt.

add_library(
  qhull OBJECT
  geom.c
  io.c
  mem.c
  poly.c
  qset.c
  rboxlib.c
  user.h
  geom.h
  io.h
  mem.h
  poly.h
  qset.h
  stat.c
  usermem.c
  geom2.c
  libqhull.c
  merge.c
  poly2.c
  random.c
  stat.h
  userprintf.c
  global.c
  libqhull.h
  merge.h
  qhull_a.h
  random.h
  user.c
  userprintf_rbox.c)
include(GdalStandardIncludes)
gdal_standard_includes(qhull)
set_property(TARGET qhull PROPERTY POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})
if (MSVC)
  target_compile_definitions(qhull PRIVATE -D_USE_MATH_DEFINES)
  if (CMAKE_CL_64)
    target_compile_definitions(qhull PRIVATE -DWIN64)
  else ()
    target_compile_definitions(qhull PRIVATE -DWIN32)
  endif ()
endif ()

target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:qhull>)
