# CMake4GDAL project is distributed under MIT license. See accompanying file LICENSE.txt.

find_package(Perl)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib/Geo/GDAL)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/blib/arch/Geo/GDAL)
include(GdalSwigBindings)
gdal_swig_bindings(
  BINDING
  perl
  ARGS
  -DPERL_CPAN_NAMESPACE
  -outdir
  ${CMAKE_CURRENT_BINARY_DIR}/lib/Geo
  OUTPUT
  ${CMAKE_CURRENT_BINARY_DIR}/lib/Geo/GDAL.pm
  ${CMAKE_CURRENT_BINARY_DIR}/lib/Geo/Const.pm
  ${CMAKE_CURRENT_BINARY_DIR}/lib/Geo/OGR.pm
  ${CMAKE_CURRENT_BINARY_DIR}/lib/Geo/OSR.pm)
# ------------------------------------------------------------
# HACK to compile interface source add_library() effects all perl wrapper c++ sources to compile into
# ${GDAL_PERL_WRAP_OBJ_DIR}/*.o GDAL_PERL_WRAP_OBJ_DIR=CMakeFiles/swig_perl_wrap.dir/extensions Generating custom
# Makefile.PL using above information
#
find_package(PerlLibs)
if (ENABLE_GNM)
  add_library(
    swig_perl_wrap OBJECT
    ${CMAKE_CURRENT_BINARY_DIR}/extensions/gdal_wrap.cpp ${CMAKE_CURRENT_BINARY_DIR}/extensions/gdalconst_wrap.c
    ${CMAKE_CURRENT_BINARY_DIR}/extensions/gnm_wrap.cpp ${CMAKE_CURRENT_BINARY_DIR}/extensions/ogr_wrap.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/extensions/osr_wrap.cpp)
  target_include_directories(swig_perl_wrap PRIVATE $<TARGET_PROPERTY:gnm,SOURCE_DIR>)
else ()
  add_library(
    swig_perl_wrap OBJECT
    ${CMAKE_CURRENT_BINARY_DIR}/extensions/gdal_wrap.cpp ${CMAKE_CURRENT_BINARY_DIR}/extensions/gdalconst_wrap.c
    ${CMAKE_CURRENT_BINARY_DIR}/extensions/ogr_wrap.cpp ${CMAKE_CURRENT_BINARY_DIR}/extensions/osr_wrap.cpp)
endif ()

target_include_directories(swig_perl_wrap PRIVATE ${PERL_INCLUDE_PATH})
set_target_properties(swig_perl_wrap PROPERTIES CXX_STANDARD 98 POSITION_INDEPENDENT_CODE ON)
gdal_standard_includes(swig_perl_wrap)
target_compile_definitions(swig_perl_wrap PRIVATE -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE)
# Generate Makefile.PL
set(GDAL_PERL_WRAP_OBJ_DIR "CMakeFiles/swig_perl_wrap.dir/extensions")
configure_file(${GDAL_CMAKE_TEMPLATE_PATH}/Makefile.PL.in ${CMAKE_CURRENT_BINARY_DIR}/Makefile.PL @ONLY)
# --------------------------------------------------------------
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lib/Geo/GDAL/Const.pm
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/lib/Geo/Const.pm
          ${CMAKE_CURRENT_BINARY_DIR}/lib/Geo/GDAL/)
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/Makefile_Geo__GDAL ${CMAKE_CURRENT_BINARY_DIR}/Makefile_Geo__GDAL__Const
         ${CMAKE_CURRENT_BINARY_DIR}/Makefile_Geo__GNM ${CMAKE_CURRENT_BINARY_DIR}/Makefile_Geo__OSR
         ${CMAKE_CURRENT_BINARY_DIR}/Makefile_Geo__OGR
  COMMAND ${PERL_EXECUTABLE} Makefile.PL INSTALL_BASE=${CMAKE_INSTALL_PREFIX}
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS swig_perl_wrap ${CMAKE_CURRENT_BINARY_DIR}/Makefile.PL ${CMAKE_CURRENT_BINARY_DIR}/lib/Geo/GDAL/Const.pm)
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/blib/arch/Geo/GDAL.so
  COMMAND make -f Makefile_Geo__GDAL
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/Makefile_Geo__GDAL gdal)
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/blib/arch/Geo/GDAL/GDAL_const.so
  COMMAND make -f Makefile_Geo__GDAL__Const
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/Makefile_Geo__GDAL__Const gdal ${CMAKE_CURRENT_BINARY_DIR}/blib/arch/Geo/GDAL.so)
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/blib/arch/Geo/GDAL_osr.so
  COMMAND make -f Makefile_Geo__OSR
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/Makefile_Geo__OSR gdal ${CMAKE_CURRENT_BINARY_DIR}/blib/arch/Geo/GDAL.so)
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/blib/arch/Geo/GDAL_ogr.so
  COMMAND make -f Makefile_Geo__OGR
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/Makefile_Geo__OGR gdal ${CMAKE_CURRENT_BINARY_DIR}/blib/arch/Geo/GDAL.so)
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/blib/arch/Geo/GDAL_gnm.so
  COMMAND make -f Makefile_Geo__GNM
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/Makefile_Geo__GNM gdal ${CMAKE_CURRENT_BINARY_DIR}/blib/arch/Geo/GDAL.so)
if (ENABLE_GNM)
  add_custom_target(
    perl_binding ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/blib/arch/Geo/GDAL.so
            ${CMAKE_CURRENT_BINARY_DIR}/blib/arch/Geo/GDAL/GDAL_const.so
            ${CMAKE_CURRENT_BINARY_DIR}/blib/arch/Geo/GDAL_osr.so
            ${CMAKE_CURRENT_BINARY_DIR}/blib/arch/Geo/GDAL_ogr.so
            ${CMAKE_CURRENT_BINARY_DIR}/blib/arch/Geo/GDAL_gnm.so)
else ()
  add_custom_target(
    perl_binding ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/blib/arch/Geo/GDAL.so
            ${CMAKE_CURRENT_BINARY_DIR}/blib/arch/Geo/GDAL/GDAL_const.so
            ${CMAKE_CURRENT_BINARY_DIR}/blib/arch/Geo/GDAL_osr.so ${CMAKE_CURRENT_BINARY_DIR}/blib/arch/Geo/GDAL_ogr.so)
endif ()
# ------------------------------------------------------------
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/cmake_perl_install.cmake
     "execute_process(COMMAND make -f Makefile_Geo__GDAL install WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})\n")
file(
  APPEND ${CMAKE_CURRENT_BINARY_DIR}/cmake_perl_install.cmake
  "execute_process(COMMAND make -f Makefile_Geo__GDAL__Const install WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/cmake_perl_install.cmake
     "execute_process(COMMAND make -f Makefile_Geo__OSR install  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/cmake_perl_install.cmake
     "execute_process(COMMAND make -f Makefile_Geo__OGR install  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})\n")
if (ENABLE_GNM)
  file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/cmake_perl_install.cmake
       "execute_process(COMMAND make -f Makefile_Geo__GNM install  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})\n")
endif ()
install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/cmake_perl_install.cmake COMPONENT runtime)

# vim: ts=4 sw=4 sts=4 et
