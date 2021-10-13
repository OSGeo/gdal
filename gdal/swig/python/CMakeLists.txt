# CMake4GDAL project is distributed under X/MIT license. See accompanying file LICENSE.txt.

# SWIG generates wrapper sources
option(SWIG_REGENERATE_PYTHON "Generate python source with SWIG." OFF)
if (SWIG_REGENERATE_PYTHON)
  set(GDAL_PYTHON_CSOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/extensions/")
  set(GDAL_PYTHON_CSOURCES
      ${GDAL_PYTHON_CSOURCE_DIR}/gdal_wrap.cpp ${GDAL_PYTHON_CSOURCE_DIR}/gdalconst_wrap.c
      ${GDAL_PYTHON_CSOURCE_DIR}/gnm_wrap.cpp ${GDAL_PYTHON_CSOURCE_DIR}/ogr_wrap.cpp
      ${GDAL_PYTHON_CSOURCE_DIR}/osr_wrap.cpp)
  set(GDAL_PYTHON_PYSOURCES
      ${CMAKE_CURRENT_BINARY_DIR}/osgeo/gdal.py ${CMAKE_CURRENT_BINARY_DIR}/osgeo/gdalconst.py
      ${CMAKE_CURRENT_BINARY_DIR}/osgeo/gdalnumeric.py ${CMAKE_CURRENT_BINARY_DIR}/osgeo/gnm.py
      ${CMAKE_CURRENT_BINARY_DIR}/osgeo/ogr.py ${CMAKE_CURRENT_BINARY_DIR}/osgeo/osr.py)
  include(GdalSwigBindings)
  if (Pyhton_VERSION_MAJOR EQUAL 3)
    set(PY3ARG "-py3")
  else ()
    unset(PY3ARG)
  endif ()
  gdal_swig_bindings(
    BINDING
    python
    ARGS
    -I${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs
    ${PY3ARG}
    -threads
    -outdir
    ${CMAKE_CURRENT_BINARY_DIR}
    OUTPUT
    ${GDAL_PYTHON_PYSOURCES}
    ${GDAL_PYTHON_CSOURCES})

  # gdal_array_wrap.cpp when NumPy exist
  if (NumPy_FOUND)
    set(ARRAY_INPUT ${CMAKE_SOURCE_DIR}/gdal/swig/include/gdal_array.i)
    set(ARRAY_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/extensions/gdal_array_wrap.cpp)
    add_custom_command(
      OUTPUT ${ARRAY_OUTPUT} "${CMAKE_CURRENT_BINARY_DIR}/osgeo/gdal_array.py"
      COMMAND
        ${SWIG_EXECUTABLE} -Wall -I${CMAKE_SOURCE_DIR}/gdal/swig/include -I${CMAKE_SOURCE_DIR}/gdal/swig/include/python
        -I${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs ${PY3ARG} -threads -outdir ${CMAKE_CURRENT_BINARY_DIR}
        ${SWIG_DEFINES} -I${CMAKE_SOURCE_DIR}/gdal -c++ -python -o ${ARRAY_OUTPUT} ${ARRAY_INPUT}
      DEPENDS ${GDAL_SWIG_COMMON_INCLUDE} ${ARRAY_INPUT} ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/typemaps_python.i
      COMMENT "Generate _gdal_array_wrap.so and gdal_array.py by SWiG command")
    list(APPEND GDAL_PYTHON_CSOURCES "${GDAL_PYTHON_CSOURCE_DIR}/gdal_array_wrap.cpp")
    list(APPEND GDAL_PYTHON_PYSOURCES "${CMAKE_CURRENT_BINARY_DIR}/osgeo/gdal_array.py")
  endif ()
else ()
  set(GDAL_PYTHON_CSOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extensions")
  set(GDAL_PYTHON_PYSOURCES
      ${CMAKE_CURRENT_BINARY_DIR}/osgeo/__init__.py
      ${CMAKE_CURRENT_BINARY_DIR}/osgeo/gdal.py
      ${CMAKE_CURRENT_BINARY_DIR}/osgeo/gdalconst.py
      ${CMAKE_CURRENT_BINARY_DIR}/osgeo/gdalnumeric.py
      ${CMAKE_CURRENT_BINARY_DIR}/osgeo/gdal_array.py
      ${CMAKE_CURRENT_BINARY_DIR}/osgeo/gnm.py
      ${CMAKE_CURRENT_BINARY_DIR}/osgeo/ogr.py
      ${CMAKE_CURRENT_BINARY_DIR}/osgeo/osr.py)
  file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/osgeo/" OSGEO_BINARY_DIR)
  file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/osgeo/" OSGEO_SOURCE_DIR)
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/osgeo/")
  if (Python_VERSION_MAJOR EQUAL 3 AND Python_EXECUTABLE)
    add_custom_command(
      OUTPUT ${GDAL_PYTHON_PYSOURCES}
      COMMAND
        ${Python_EXECUTABLE} -m lib2to3 --fix=import --fix=next --fix=renames --fix=unicode --fix=ws_comma --fix=xrange
        --write --write-unchanged-files --nobackups --output-dir=${OSGEO_BINARY_DIR} ${OSGEO_SOURCE_DIR}__init__.py
        ${OSGEO_SOURCE_DIR}gdal.py ${OSGEO_SOURCE_DIR}gdalconst.py ${OSGEO_SOURCE_DIR}gdalnumeric.py
        ${OSGEO_SOURCE_DIR}ogr.py ${OSGEO_SOURCE_DIR}osr.py ${OSGEO_SOURCE_DIR}gnm.py ${OSGEO_SOURCE_DIR}gdal_array.py
      BYPRODUCTS ${GDAL_PYTHON_PYSOURCES})
  else ()
    add_custom_command(
      OUTPUT ${GDAL_PYTHON_PYSOURCES}
      COMMAND
        ${CMAKE_COMMAND} -E copy ${OSGEO_SOURCE_DIR}__init__.py ${OSGEO_SOURCE_DIR}gdal.py
        ${OSGEO_SOURCE_DIR}gdalconst.py ${OSGEO_SOURCE_DIR}gdalnumeric.py ${OSGEO_SOURCE_DIR}ogr.py
        ${OSGEO_SOURCE_DIR}osr.py ${OSGEO_SOURCE_DIR}gnm.py ${OSGEO_SOURCE_DIR}gdal_array.py ${OSGEO_BINARY_DIR}
      BYPRODUCTS ${GDAL_PYTHON_PYSOURCES})
  endif ()
  set(GDAL_PYTHON_CSOURCES
      ${GDAL_PYTHON_CSOURCE_DIR}/gdal_wrap.cpp ${GDAL_PYTHON_CSOURCE_DIR}/gdalconst_wrap.c
      ${GDAL_PYTHON_CSOURCE_DIR}/gnm_wrap.cpp ${GDAL_PYTHON_CSOURCE_DIR}/ogr_wrap.cpp
      ${GDAL_PYTHON_CSOURCE_DIR}/osr_wrap.cpp)
endif ()
if (Python_FOUND AND NOT WIN32)
  if (ENABLE_GNM)
    set(GNM_ENABLED "True")
  else ()
    set(GNM_ENABLED "False")
  endif ()
  file(TO_NATIVE_PATH "${GDAL_PYTHON_CSOURCE_DIR}" GDAL_PYTHON_EXT_SOURCE_DIR)
  configure_file(${GDAL_CMAKE_TEMPLATE_PATH}/setup.py.in ${CMAKE_CURRENT_BINARY_DIR}/setup.py @ONLY)
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/get_ext.py "import sysconfig\nprint(sysconfig.get_config_var('EXT_SUFFIX'))\n")
  execute_process(
    COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/get_ext.py
    OUTPUT_VARIABLE PY_EXT_EXT
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  add_custom_command(
    OUTPUT osgeo._gdal${PY_EXT_EXT} osgeo._gdalconst${PY_EXT_EXT} osgeo._gdal_array${PY_EXT_EXT} osgeo._gnm${PY_EXT_EXT}
           osgeo._ogr${PY_EXT_EXT} osgeo._osr${PY_EXT_EXT}
    COMMAND ${Python_EXECUTABLE} setup.py build_ext --inplace
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    BYPRODUCTS osgeo._gdal${PY_EXT_EXT}
               osgeo._gdalconst${PY_EXT_EXT}
               osgeo._gdal_array${PY_EXT_EXT}
               osgeo._gnm${PY_EXT_EXT}
               osgeo._ogr${PY_EXT_EXT}
               osgeo._osr${PY_EXT_EXT}
               SOURCES
               ${GDAL_PYTHON_PYSOURCDES}
               ${GDAL_PYTHON_CSOURCES})
  add_custom_target(
    python_binding ALL
    DEPENDS osgeo._gdal${PY_EXT_EXT}
            osgeo._gdalconst${PY_EXT_EXT}
            osgeo._gdal_array${PY_EXT_EXT}
            osgeo._gnm${PY_EXT_EXT}
            osgeo._ogr${PY_EXT_EXT}
            osgeo._osr${PY_EXT_EXT}
            ${GDAL_PYTHON_PYSOURCES}
            gdal)
  add_custom_target(
    python_wheel
    COMMAND ${Python_EXECUTABLE} setup.py bdist_wheel
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS osgeo._gdal${PY_EXT_EXT} osgeo._gdalconst${PY_EXT_EXT} osgeo._gdal_array${PY_EXT_EXT}
            osgeo._gnm${PY_EXT_EXT} osgeo._ogr${PY_EXT_EXT} osgeo._osr${PY_EXT_EXT})
  if (CMAKE_INSTALL_PREFIX)
    install(CODE "execute_process(COMMAND ${Python_EXECUTABLE} setup.py install --prefix=\"${CMAKE_INSTALL_PREFIX}\"
                                  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})")
  else ()
    install(CODE "execute_process(COMMAND ${Python_EXECUTABLE} setup.py install
                                  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})")
  endif ()
elseif (Python_Development_FOUND)
  macro (py_ext name v source)
    set(GDAL_PYTHON_EXT_SOURCE_DIR "${GDAL_PYTHON_CSOURCE_DIR}")
    python_add_library(${name}${v} MODULE ${GDAL_PYTHON_EXT_SOURCE_DIR}/${source})
    set_property(TARGET ${name}${v} PROPERTY LIBRARY_OUTPUT_NAME ${name})
    if (GENERATOR_IS_MULTI_CONFIG)
      set_target_properties(
        ${name}${v}
        PROPERTIES LIBRARY_OUTPUT_NAME ${name}
                   LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR}/debug/osgeo
                   LIBRARY_OUTPUT_DIRECTORY_REL ${CMAKE_CURRENT_BINARY_DIR}/osgeo)
    else ()
      set_target_properties(${name}${v} PROPERTIES LIBRARY_OUTPUT_NAME ${name} LIBRARY_OUTPUT_DIRECTORY
                                                                               ${CMAKE_CURRENT_BINARY_DIR}/osgeo)
    endif ()

    gdal_standard_includes(${name}${v})
    target_include_directories(${name}${v} PRIVATE $<TARGET_PROPERTY:appslib,SOURCE_DIR>
                                                   $<TARGET_PROPERTY:gnm,SOURCE_DIR>)
    if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
      target_compile_definitions(${name}${v} PRIVATE -D__MSVCRT_VERSION__=0x0601)
    endif ()
    target_link_libraries(${name}${v} PRIVATE $<TARGET_NAME:gdal>)
    install(
      TARGETS ${name}${v}
      COMPONENT python
      DESTINATION ${Python_SITEARCH}/osgeo)
  endmacro ()
  set(v ${Python_VERSION_MAJOR})
  py_ext(_gdal ${v} gdal_wrap.cpp)
  py_ext(_gdalconst ${v} gdalconst_wrap.c)
  py_ext(_gnm ${v} gnm_wrap.cpp)
  py_ext(_ogr ${v} ogr_wrap.cpp)
  py_ext(_osr ${v} osr_wrap.cpp)
  set(_py_depends)
  if (Python_NumPy_FOUND)
    if (NOT Pyhton_NumPy_VERSION VERSION_LESS 1.0.0)
      py_ext(_gdal_array ${v} gdal_array_wrap.cpp)
      target_include_directories(_gdal_array${v} PRIVATE ${Python_NumPy_INCLUDE_DIRS})
      list(APPEND _py_depends _gdal_array${v})
    endif ()
  endif ()
  if (ENABLE_GNM)
    list(APPEND _py_depends _gnm${v})
  endif ()
  add_custom_target(
    python_ext_mod
    DEPENDS _gdal${v} _gdalconst${v} _ogr${v} _osr${v} ${_py_depends}
    COMMENT "Build python binding modules.")
  add_custom_target(
    python_binding ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/osgeo/__init__.py
            ${CMAKE_CURRENT_BINARY_DIR}/osgeo/gdal.py
            ${CMAKE_CURRENT_BINARY_DIR}/osgeo/gdalconst.py
            ${CMAKE_CURRENT_BINARY_DIR}/osgeo/gdalnumeric.py
            ${CMAKE_CURRENT_BINARY_DIR}/osgeo/gnm.py
            ${CMAKE_CURRENT_BINARY_DIR}/osgeo/ogr.py
            ${CMAKE_CURRENT_BINARY_DIR}/osgeo/osr.py
            python_ext_mod
    COMMENT "Build all python binding files.")
  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/osgeo/__init__.py
          ${CMAKE_CURRENT_BINARY_DIR}/osgeo/gdal.py
          ${CMAKE_CURRENT_BINARY_DIR}/osgeo/gdalconst.py
          ${CMAKE_CURRENT_BINARY_DIR}/osgeo/gdalnumeric.py
          ${CMAKE_CURRENT_BINARY_DIR}/osgeo/gnm.py
          ${CMAKE_CURRENT_BINARY_DIR}/osgeo/ogr.py
          ${CMAKE_CURRENT_BINARY_DIR}/osgeo/osr.py
    COMPONENT python
    DESTINATION ${Python_SITELIB}/osgeo)
else ()

endif ()
# FIXME !!!! add_subdirectory(scripts)
file(COPY ${CMAKE_SOURCE_DIR}/gdal/Doxyfile DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile "GENERATE_XML=YES")
add_custom_command(
  OUTPUT ${CMAKE_SOURCE_DIR}/gdal/xml/ogrlayer_8cpp.xml ${CMAKE_SOURCE_DIR}/gdal/xml/ogrgeometry_8cpp.xml
         ${CMAKE_SOURCE_DIR}/gdal/xml/ogrsfdriver_8cpp.xml ${CMAKE_SOURCE_DIR}/gdal/xml/ogrfeature_8cpp.xml
         ${CMAKE_SOURCE_DIR}/gdal/xml/ogrfeaturedefn_8cpp.xml ${CMAKE_SOURCE_DIR}/gdal/xml/ogrfielddefn_8cpp.xml
  COMMAND doxygen ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
  COMMENT "Generate xml by doxygen"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/gdal)
add_custom_target(
  python_interface_docs
  DEPENDS ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/ogr_layer_docs.i
          ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/ogr_geometry_docs.i
          ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/ogr_datasource_docs.i
          ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/ogr_driver_docs.i
          ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/ogr_feature_docs.i
          ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/ogr_featuredef_docs.i
          ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/ogr_fielddef_docs.i
  COMMENT "Generate document interfaces for python")
add_custom_command(
  OUTPUT ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/ogr_layer_docs.i
  COMMAND
    ${Python_EXECUTABLE} ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/doxy2swig.py
    ${CMAKE_SOURCE_DIR}/gdal/xml/ogrlayer_8cpp.xml ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/ogr_layer_docs.i
    OGRLayerShadow OGR_L_
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS ${CMAKE_SOURCE_DIR}/gdal/xml/ogrlayer_8cpp.xml
  COMMENT "Generate ogr_layer_docs.i by doxy2swig")
add_custom_command(
  OUTPUT ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/ogr_geometry_docs.i
  COMMAND
    ${Python_EXECUTABLE} ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/doxy2swig.py
    ${CMAKE_SOURCE_DIR}/gdal/xml/ogrgeometry_8cpp.xml
    ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/ogr_geometry_docs.i OGRGeometryShadow OGR_G_
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS ${CMAKE_SOURCE_DIR}/gdal/xml/ogrgeometry_8cpp.xml
  COMMENT "Generate ogr_geometory_docs.i by doxy2swig")
add_custom_command(
  OUTPUT ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/ogr_datasource_docs.i
  COMMAND
    ${Python_EXECUTABLE} ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/doxy2swig.py
    ${CMAKE_SOURCE_DIR}/gdal/xml/ogrdatasource_8cpp.xml
    ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/ogr_datasource_docs.i OGRDataSourceShadow OGR_DS_
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS ${CMAKE_SOURCE_DIR}/gdal/xml/ogrdatasource_8cpp.xml
  COMMENT "Generate ogr_datasource_docs.i by doxy2swig")
add_custom_command(
  OUTPUT ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/ogr_driver_docs.i
  COMMAND
    ${Python_EXECUTABLE} ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/doxy2swig.py
    ${CMAKE_SOURCE_DIR}/gdal/xml/ogrsfdriver_8cpp.xml
    ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/ogr_driver_docs.i OGRDriverShadow OGR_Dr_
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS ${CMAKE_SOURCE_DIR}/gdal/xml/ogrsfdriver_8cpp.xml
  COMMENT "Generate ogr_driver_docs.i by doxy2swig")
add_custom_command(
  OUTPUT ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/ogr_feature_docs.i
  COMMAND
    ${Python_EXECUTABLE} ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/doxy2swig.py
    ${CMAKE_SOURCE_DIR}/gdal/xml/ogrfeature_8cpp.xml
    ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/ogr_feature_docs.i OGRFeatureShadow OGR_F_
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS ${CMAKE_SOURCE_DIR}/gdal/xml/ogrfeature_8cpp.xml
  COMMENT "Generate ogr_feature_docs.i by doxy2swig")
add_custom_command(
  OUTPUT ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/ogr_featuredef_docs.i
  COMMAND
    ${Python_EXECUTABLE} ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/doxy2swig.py
    ${CMAKE_SOURCE_DIR}/gdal/xml/ogrfeaturedefn_8cpp.xml
    ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/ogr_featuredef_docs.i OGRFeatureDefnShadow OGR_FD_
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS ${CMAKE_SOURCE_DIR}/gdal/xml/ogrfeaturedefn_8cpp.xml
  COMMENT "Generate ogr_featuredef_docs.i by doxy2swig")
add_custom_command(
  OUTPUT ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/ogr_fielddef_docs.i
  COMMAND
    ${Python_EXECUTABLE} ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/doxy2swig.py
    ${CMAKE_SOURCE_DIR}/gdal/xml/ogrfielddefn_8cpp.xml
    ${CMAKE_SOURCE_DIR}/gdal/swig/include/python/docs/ogr_fielddef_docs.i OGRFieldDefnShadow OGR_Fld_
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS ${CMAKE_SOURCE_DIR}/gdal/xml/ogrfielddefn_8cpp.xml
  COMMENT "Generate ogr_fielddef_docs.i by doxy2swig")
add_custom_target(
  epydoc
  COMMAND epydoc --config ${CMAKE_CURRENT_SOURCE_DIR}/epydoc.conf
  COMMENT "Generate epydoc")
