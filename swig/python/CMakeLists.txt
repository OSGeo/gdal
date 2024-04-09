# CMake4GDAL project is distributed under MIT license. See accompanying file LICENSE.txt.

if (CMAKE_CXX_FLAGS)
  string(REPLACE "-Werror " " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ")
  string(REPLACE "/WX " " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ")
endif ()

set(GDAL_DATA_FILES
    LICENSE.TXT
    ${CMAKE_CURRENT_SOURCE_DIR}/data/template_tiles.mapml
)
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY RESOURCE "${GDAL_DATA_FILES}")

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/osgeo/")
set(GDAL_PYTHON_CSOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/extensions/")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/extensions/")
set(GDAL_PYTHON_CSOURCES
    ${GDAL_PYTHON_CSOURCE_DIR}/gdal_wrap.cpp ${GDAL_PYTHON_CSOURCE_DIR}/gdalconst_wrap.c
    ${GDAL_PYTHON_CSOURCE_DIR}/gnm_wrap.cpp ${GDAL_PYTHON_CSOURCE_DIR}/ogr_wrap.cpp
    ${GDAL_PYTHON_CSOURCE_DIR}/osr_wrap.cpp)

  list(APPEND GDAL_SWIG_COMMON_INTERFACE_FILES
       ${PROJECT_SOURCE_DIR}/swig/include/python/callback.i
       ${PROJECT_SOURCE_DIR}/swig/include/python/gdal_python.i
       ${PROJECT_SOURCE_DIR}/swig/include/python/gnm_python.i
       ${PROJECT_SOURCE_DIR}/swig/include/python/ogr_python.i
       ${PROJECT_SOURCE_DIR}/swig/include/python/osr_python.i
       ${PROJECT_SOURCE_DIR}/swig/include/python/python_exceptions.i
       ${PROJECT_SOURCE_DIR}/swig/include/python/python_strings.i
       ${PROJECT_SOURCE_DIR}/swig/include/python/typemaps_python.i
       ${PROJECT_SOURCE_DIR}/swig/include/python/docs/gdal_docs.i
       ${PROJECT_SOURCE_DIR}/swig/include/python/docs/gdal_band_docs.i
       ${PROJECT_SOURCE_DIR}/swig/include/python/docs/gdal_dataset_docs.i
       ${PROJECT_SOURCE_DIR}/swig/include/python/docs/gdal_driver_docs.i
       ${PROJECT_SOURCE_DIR}/swig/include/python/docs/ogr_docs.i
       ${PROJECT_SOURCE_DIR}/swig/include/python/docs/ogr_datasource_docs.i
       ${PROJECT_SOURCE_DIR}/swig/include/python/docs/ogr_driver_docs.i
       ${PROJECT_SOURCE_DIR}/swig/include/python/docs/ogr_featuredef_docs.i
       ${PROJECT_SOURCE_DIR}/swig/include/python/docs/ogr_feature_docs.i
       ${PROJECT_SOURCE_DIR}/swig/include/python/docs/ogr_fielddef_docs.i
       ${PROJECT_SOURCE_DIR}/swig/include/python/docs/ogr_fielddomain_docs.i
       ${PROJECT_SOURCE_DIR}/swig/include/python/docs/ogr_geometry_docs.i
       ${PROJECT_SOURCE_DIR}/swig/include/python/docs/ogr_layer_docs.i
       ${PROJECT_SOURCE_DIR}/swig/include/python/docs/osr_docs.i
       ${PROJECT_SOURCE_DIR}/swig/include/python/docs/osr_spatialreference_docs.i
       ${PROJECT_SOURCE_DIR}/swig/include/python/docs/osr_coordinatetransformation_docs.i)

  set(GDAL_PYTHON_PYSOURCES
      ${CMAKE_CURRENT_BINARY_DIR}/osgeo/gdal.py ${CMAKE_CURRENT_BINARY_DIR}/osgeo/gdalconst.py
      ${CMAKE_CURRENT_BINARY_DIR}/osgeo/gnm.py
      ${CMAKE_CURRENT_BINARY_DIR}/osgeo/ogr.py ${CMAKE_CURRENT_BINARY_DIR}/osgeo/osr.py)
  include(GdalSwigBindings)
  if( ${SWIG_VERSION} VERSION_LESS "4.1" )
      set(SWIG_PY3_DEPRECATED "-py3")
  endif()
  gdal_swig_bindings(
    BINDING
    python
    ARGS
    -I${PROJECT_SOURCE_DIR}/swig/include/python/docs
    ${SWIG_PY3_DEPRECATED}
    -threads
    -relativeimport
    -outdir
    ${CMAKE_CURRENT_BINARY_DIR}/osgeo
    OUTPUT
    ${GDAL_PYTHON_PYSOURCES}
    ${GDAL_PYTHON_CSOURCES}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/modify_cpp_files.cmake)

  # gdal_array_wrap.cpp when NumPy exist
  if (Python_NumPy_FOUND)
    set(ARRAY_INPUT ${PROJECT_SOURCE_DIR}/swig/include/gdal_array.i)
    set(ARRAY_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/extensions/gdal_array_wrap.cpp)
    add_custom_command(
      OUTPUT ${ARRAY_OUTPUT} "${CMAKE_CURRENT_BINARY_DIR}/osgeo/gdal_array.py"
      COMMAND
        ${SWIG_EXECUTABLE} -Wall -I${PROJECT_SOURCE_DIR}/swig/include -I${PROJECT_SOURCE_DIR}/swig/include/python
        -I${PROJECT_SOURCE_DIR}/swig/include/python/docs ${SWIG_PY3_DEPRECATED} -threads -relativeimport -outdir
        ${CMAKE_CURRENT_BINARY_DIR}/osgeo ${SWIG_DEFINES} -I${PROJECT_SOURCE_DIR}/gdal -c++ -python -o ${ARRAY_OUTPUT}
        ${ARRAY_INPUT}
      DEPENDS ${ARRAY_INPUT}
              ${PROJECT_SOURCE_DIR}/swig/include/python/typemaps_python.i
              ${CMAKE_CURRENT_SOURCE_DIR}/modify_cpp_files.cmake
      COMMENT "Generate _gdal_array_wrap.so and gdal_array.py by SWiG command")
    list(APPEND GDAL_PYTHON_CSOURCES "${GDAL_PYTHON_CSOURCE_DIR}/gdal_array_wrap.cpp")
    list(APPEND GDAL_PYTHON_PYSOURCES "${CMAKE_CURRENT_BINARY_DIR}/osgeo/gdal_array.py")
  endif ()

  if (NOT "${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
      add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/osgeo/gdalnumeric.py"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/osgeo/gdalnumeric.py"
                "${CMAKE_CURRENT_BINARY_DIR}/osgeo"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/osgeo/gdalnumeric.py")
      list(APPEND GDAL_PYTHON_PYSOURCES "${CMAKE_CURRENT_BINARY_DIR}/osgeo/gdalnumeric.py")
  endif()

  if (NOT "${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
      add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/osgeo/__init__.py"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/osgeo/__init__.py" "${CMAKE_CURRENT_BINARY_DIR}/osgeo"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/osgeo/__init__.py")
      list(APPEND GDAL_PYTHON_PYSOURCES "${CMAKE_CURRENT_BINARY_DIR}/osgeo/__init__.py")
  endif()

  foreach(_file IN ITEMS ${GDAL_PYTHON_CSOURCES})
      add_custom_command(
        OUTPUT ${_file}
        APPEND
        COMMAND ${CMAKE_COMMAND} ${WERROR_DEV_FLAG} "-DFILE=${_file}" -P ${CMAKE_CURRENT_SOURCE_DIR}/modify_cpp_files.cmake)
  endforeach()

add_custom_target(python_generated_files
  DEPENDS ${GDAL_PYTHON_PYSOURCES} ${GDAL_PYTHON_CSOURCES}
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

function (symlink_or_copy source destination)
    file(
      CREATE_LINK ${source} ${destination}
      RESULT res
      SYMBOLIC)
    if (NOT res EQUAL 0)
      message(STATUS "Copying content of ${source} to ${destination}")
      execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${source} ${destination})
    endif ()
endfunction ()

if (NOT "${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
    symlink_or_copy(${CMAKE_CURRENT_SOURCE_DIR}/gdal-utils ${CMAKE_CURRENT_BINARY_DIR}/gdal-utils)
endif()
symlink_or_copy(${CMAKE_CURRENT_SOURCE_DIR}/gdal-utils/osgeo_utils ${CMAKE_CURRENT_BINARY_DIR}/osgeo_utils)

get_property(_isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (_isMultiConfig AND WIN32)
  set(ONLY_GENERATE_FOR_NON_DEBUG ON)
endif ()

if (Python_Interpreter_FOUND)
  if (ENABLE_GNM)
    set(GNM_ENABLED "True")
  else ()
    set(GNM_ENABLED "False")
  endif ()

  # Use the 'X.Y.Z' numbering scheme from libgdal by default for the gdal python package version number
  # And append a ".patch" if the patch is not 0.
  set(GDAL_PYTHON_VERSION "${GDAL_VERSION_NO_DEV_SUFFIX}")
  if( NOT "${GDAL_VERSION_BUILD}" STREQUAL "0" )
      string(APPEND GDAL_PYTHON_VERSION ".${GDAL_VERSION_BUILD}")
  endif()

  file(TO_NATIVE_PATH "${GDAL_PYTHON_CSOURCE_DIR}" GDAL_PYTHON_EXT_SOURCE_DIR)
  if (_isMultiConfig)
    set(GDAL_LIB_DIR "os.path.dirname('$<TARGET_FILE:${GDAL_LIB_TARGET_NAME}>')")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in ${CMAKE_CURRENT_BINARY_DIR}/setup.py.tmp @ONLY)
    if (ONLY_GENERATE_FOR_NON_DEBUG)
      file(
        GENERATE
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/setup.py
        INPUT ${CMAKE_CURRENT_BINARY_DIR}/setup.py.tmp
        CONDITION "$<CONFIG:Release>")
      set(SETUP_PY_FILENAME "${CMAKE_CURRENT_BINARY_DIR}/setup_$<CONFIG>.py")
    else ()
      set(SETUP_PY_FILENAME "${CMAKE_CURRENT_BINARY_DIR}/setup.py")
    endif ()
    file(
      GENERATE
      OUTPUT "${SETUP_PY_FILENAME}"
      INPUT ${CMAKE_CURRENT_BINARY_DIR}/setup.py.tmp)
  else ()
    # Do not use file(GENERATE) as, for some unknown reason, it would cause cmake to be reinvoked every time make is
    # involved
    get_target_property(_gdal_lib_directory ${GDAL_LIB_TARGET_NAME} LIBRARY_OUTPUT_DIRECTORY)
    set(GDAL_LIB_DIR "'${_gdal_lib_directory}'")
    set(SETUP_PY_FILENAME "${CMAKE_CURRENT_BINARY_DIR}/setup.py")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in ${SETUP_PY_FILENAME} @ONLY)
  endif ()

  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/README.rst ${CMAKE_CURRENT_BINARY_DIR}/README.rst @ONLY)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pyproject.toml ${CMAKE_CURRENT_BINARY_DIR}/pyproject.toml @ONLY)

  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/get_suffix.py
       "from importlib.machinery import EXTENSION_SUFFIXES; print(EXTENSION_SUFFIXES[0])\n")
  execute_process(
    COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/get_suffix.py
    OUTPUT_VARIABLE PY_EXT_SUFFIX
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  set(PY_SO_LIST osgeo/_gdal${PY_EXT_SUFFIX} osgeo/_gdalconst${PY_EXT_SUFFIX} osgeo/_gnm${PY_EXT_SUFFIX}
                 osgeo/_ogr${PY_EXT_SUFFIX} osgeo/_osr${PY_EXT_SUFFIX})
  if (Python_NumPy_FOUND)
    set(PY_SO_LIST ${PY_SO_LIST} osgeo/_gdal_array${PY_EXT_SUFFIX})
  endif ()

  # Generate extension
  if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.20 )
    cmake_path(CONVERT "${Python_EXECUTABLE}" TO_CMAKE_PATH_LIST Python_EXECUTABLE_CMAKE)
  else()
    set(Python_EXECUTABLE_CMAKE "${Python_EXECUTABLE}")
  endif()
  if (ONLY_GENERATE_FOR_NON_DEBUG)
    set(BUILD_EXT_FILENAME "${CMAKE_CURRENT_BINARY_DIR}/build_ext_$<CONFIG>.cmake")
    file(
      GENERATE
      OUTPUT "${BUILD_EXT_FILENAME}"
      CONTENT
        "file(REMOVE_RECURSE ${CMAKE_CURRENT_BINARY_DIR}/build)
         execute_process(COMMAND $<IF:$<NOT:$<CONFIG:Debug>>,\"${Python_EXECUTABLE_CMAKE}\" \"${SETUP_PY_FILENAME}\" build_ext --inplace,${CMAKE_COMMAND} -E echo \"setup.py build_ext only run in configuration != Debug\"> RESULT_VARIABLE res)
         if(NOT res EQUAL 0)
           message(FATAL_ERROR \"setup.py bdist_wheel failed\")
         endif()"
      )
  else ()
    set(BUILD_EXT_FILENAME "${CMAKE_CURRENT_BINARY_DIR}/build_ext.cmake")
    file(
      GENERATE
      OUTPUT "${BUILD_EXT_FILENAME}"
      CONTENT
         "file(REMOVE_RECURSE ${CMAKE_CURRENT_BINARY_DIR}/build)\n
          execute_process(COMMAND \"${Python_EXECUTABLE_CMAKE}\" \"${SETUP_PY_FILENAME}\" build_ext --inplace RESULT_VARIABLE res)
          if(NOT res EQUAL 0)
            message(FATAL_ERROR \"setup.py bdist_wheel failed\")
          endif()"
      )
  endif ()

  if(CMAKE_NM AND BUILD_SHARED_LIBS AND NOT _isMultiConfig)
      # Optimization to avoid rebuilding the .o and .so each time libgdal
      # is touched. We only consider the list of exported symbols as the
      # dependency rule
      set(GDAL_LIB_DEP_FOR_PY_SO "gdal_symbols.txt")
      if(WIN32 OR APPLE)
          set(NM_OPTIONS "-g")
      else()
          set(NM_OPTIONS "-gD")
      endif()
      file(
          GENERATE
          OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generate_gdal_symbols_txt.cmake"
          CONTENT
              "cmake_policy(VERSION 3.12)
               execute_process(COMMAND ${CMAKE_NM} ${NM_OPTIONS} \"$<TARGET_FILE:${GDAL_LIB_TARGET_NAME}>\"
                               OUTPUT_VARIABLE symbols
                               RESULT_VARIABLE res)
               if( \${res} EQUAL 0 )
                 string(REGEX REPLACE \"\\n\" \";\" symbols \"\${symbols}\")
                 # Match lines like '00000000006dddb0 T GDALAllRegister'
                 list(FILTER symbols INCLUDE REGEX \"^[0-9a-f]+ T (.*)$\")
                 list(TRANSFORM symbols REPLACE \"^[0-9a-f]+ T (.*)$\" \"\\\\1\")
                 if(symbols)
                     list(SORT symbols)
                     file(WRITE ${GDAL_LIB_DEP_FOR_PY_SO}.tmp \"$<TARGET_FILE:${GDAL_LIB_TARGET_NAME}>;\${symbols}\")
                     execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GDAL_LIB_DEP_FOR_PY_SO}.tmp ${GDAL_LIB_DEP_FOR_PY_SO})
                 else()
                     file(TOUCH ${GDAL_LIB_DEP_FOR_PY_SO})
                 endif()
               else()
                 file(TOUCH ${GDAL_LIB_DEP_FOR_PY_SO})
               endif()")
      add_custom_command(
        OUTPUT ${GDAL_LIB_DEP_FOR_PY_SO}
        COMMAND
            ${CMAKE_COMMAND} ${WERROR_DEV_FLAG} -P "generate_gdal_symbols_txt.cmake"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        DEPENDS ${GDAL_LIB_TARGET_NAME})
  else()
      set(GDAL_LIB_DEP_FOR_PY_SO "${GDAL_LIB_TARGET_NAME}")
  endif()

  if(WIN32)
       set(PATH_SEP ";")
    else()
       set(PATH_SEP ":")
    endif()
  add_custom_command(
    OUTPUT ${PY_SO_LIST}
    COMMAND ${CMAKE_COMMAND} -E env "\"PATH=${PROJECT_BINARY_DIR}/apps${PATH_SEP}$ENV{PATH}\"" ${CMAKE_COMMAND} ${WERROR_DEV_FLAG} -P "${BUILD_EXT_FILENAME}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${GDAL_LIB_DEP_FOR_PY_SO} ${GDAL_PYTHON_PYSOURCES} ${GDAL_PYTHON_CSOURCES} "${BUILD_EXT_FILENAME}"
            ${PROJECT_SOURCE_DIR}/gcore/gdal_priv.h)

  set(INSTALL_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
  set(PY_SO_LIST_WITH_RPATH)
  if (NOT ONLY_GENERATE_FOR_NON_DEBUG)
    if(${GDAL_SET_INSTALL_RELATIVE_RPATH})
        # TODO: we should support that, but that's involved as it requires
        # knowing where Python will install the generated .so files
        message(WARNING "The Python bindings target does not currently "
                        "support setting relative RPATH on the Python wrapper "
                        "shared libraries. "
                        "Disable GDAL_SET_INSTALL_RELATIVE_RPATH and set "
                        "an absolute value for CMAKE_INSTALL_RPATH")
    elseif(NOT "${CMAKE_INSTALL_RPATH}" STREQUAL "")

        # If a CMAKE_INSTALL_RPATH is specified, we need to do a build in a
        # dedicated "for_install" subdirectory that adds --rpath to python setup.py build_ext
        # and install that one when the "install" target is run
        # We cannot set this install rpath to regular build artifacts, to avoid
        # inconsistencies.
        # This is admittedly quite convoluted because CMake cannot do the usual
        # magic of setting/unsetting RPATH on binaries it is not aware of.

        set(INSTALL_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/for_install")
        file(MAKE_DIRECTORY "${INSTALL_WORKING_DIRECTORY}")
        # We need to copy a few things from the build directory to the for_install one:
        # the gdal-utils and extensions subdirectories, README.rst, and osgeo/*.py files
        file(MAKE_DIRECTORY "${INSTALL_WORKING_DIRECTORY}/osgeo")
        symlink_or_copy("${CMAKE_CURRENT_SOURCE_DIR}/gdal-utils" "${INSTALL_WORKING_DIRECTORY}/gdal-utils")
        set(BUILD_EXT_WITH_RPATH_CONTENT)
        string(APPEND BUILD_EXT_WITH_RPATH_CONTENT "configure_file(\"${CMAKE_CURRENT_SOURCE_DIR}/README.rst\" \"${INSTALL_WORKING_DIRECTORY}/README.rst\" @ONLY)\n")
        string(APPEND BUILD_EXT_WITH_RPATH_CONTENT "configure_file(\"${CMAKE_CURRENT_SOURCE_DIR}/pyproject.toml\" \"${INSTALL_WORKING_DIRECTORY}/pyproject.toml\" @ONLY)\n")
        string(APPEND BUILD_EXT_WITH_RPATH_CONTENT "file(COPY \"${CMAKE_CURRENT_BINARY_DIR}/extensions\" DESTINATION \"${INSTALL_WORKING_DIRECTORY}\")\n")
        string(APPEND BUILD_EXT_WITH_RPATH_CONTENT "file(GLOB PY_FILES \"${CMAKE_CURRENT_BINARY_DIR}/osgeo/*.py\")\n")
        string(APPEND BUILD_EXT_WITH_RPATH_CONTENT "foreach(_file IN LISTS PY_FILES)\n")
        string(APPEND BUILD_EXT_WITH_RPATH_CONTENT "  file(COPY \"\${_file}\" DESTINATION \"${INSTALL_WORKING_DIRECTORY}/osgeo\")\n")
        string(APPEND BUILD_EXT_WITH_RPATH_CONTENT "endforeach()\n")
        string(APPEND BUILD_EXT_WITH_RPATH_CONTENT "file(REMOVE_RECURSE ${INSTALL_WORKING_DIRECTORY}/build)\n")
        string(APPEND BUILD_EXT_WITH_RPATH_CONTENT "execute_process(COMMAND ${Python_EXECUTABLE_CMAKE} ${SETUP_PY_FILENAME} build_ext --rpath=${CMAKE_INSTALL_RPATH} --inplace WORKING_DIRECTORY ${INSTALL_WORKING_DIRECTORY})\n")

        foreach (f IN LISTS PY_SO_LIST)
          list(APPEND PY_SO_LIST_WITH_RPATH "${INSTALL_WORKING_DIRECTORY}/${f}")
        endforeach ()

        set(BUILD_EXT_WITH_RPATH_FILENAME ${CMAKE_CURRENT_BINARY_DIR}/build_ext_with_rpath.cmake)
        file(
          GENERATE
          OUTPUT ${BUILD_EXT_WITH_RPATH_FILENAME}
          CONTENT "${BUILD_EXT_WITH_RPATH_CONTENT}")
        add_custom_command(
          OUTPUT ${PY_SO_LIST_WITH_RPATH}
          COMMAND ${CMAKE_COMMAND} ${WERROR_DEV_FLAG} -P ${BUILD_EXT_WITH_RPATH_FILENAME}
          WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
          DEPENDS ${GDAL_LIB_DEP_FOR_PY_SO} ${GDAL_PYTHON_PYSOURCES} ${GDAL_PYTHON_CSOURCES} ${BUILD_EXT_WITH_RPATH_FILENAME}
                  ${PROJECT_SOURCE_DIR}/gcore/gdal_priv.h)

    endif()
  endif()

  add_custom_target(python_binding ALL DEPENDS ${PY_SO_LIST} ${PY_SO_LIST_WITH_RPATH} ${GDAL_PYTHON_PYSOURCES} ${GDAL_LIB_TARGET_NAME})

  # Generate wheel
  if (ONLY_GENERATE_FOR_NON_DEBUG)
    set(BUILD_BDIST_WHEEL_FILENAME ${CMAKE_CURRENT_BINARY_DIR}/build_bdist_wheel_$<CONFIG>.cmake)
    file(
      GENERATE
      OUTPUT ${BUILD_BDIST_WHEEL_FILENAME}
      CONTENT
        "execute_process(COMMAND $<IF:$<NOT:$<CONFIG:Debug>>,${Python_EXECUTABLE_CMAKE} ${SETUP_PY_FILENAME} bdist_wheel,${CMAKE_COMMAND} -E echo \"setup.py bdist_wheel only run in configuration != Debug\"> RESULT_VARIABLE res)
        if(NOT res EQUAL 0)
           message(FATAL_ERROR \"setup.py bdist_wheel failed\")
        endif()
        "
      )
  else ()
    set(BUILD_BDIST_WHEEL_FILENAME ${CMAKE_CURRENT_BINARY_DIR}/build_bdist_wheel.cmake)
    file(WRITE ${BUILD_BDIST_WHEEL_FILENAME}
         "execute_process(COMMAND ${Python_EXECUTABLE_CMAKE} ${SETUP_PY_FILENAME} bdist_wheel RESULT_VARIABLE res)
          if(NOT res EQUAL 0)
            message(FATAL_ERROR \"setup.py bdist_wheel failed\")
          endif()")
  endif ()
  add_custom_target(
    python_wheel
    COMMAND ${CMAKE_COMMAND} ${WERROR_DEV_FLAG} -P ${BUILD_BDIST_WHEEL_FILENAME}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${PY_SO_LIST})

  # Install extension
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/trimmedsysconfig.py ${CMAKE_CURRENT_BINARY_DIR}/trimmedsysconfig.py @ONLY)
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/get_python_lib.py
       "from trimmedsysconfig import get_python_lib;print(get_python_lib(prefix=\"${GDAL_PYTHON_INSTALL_PREFIX}\"))\n")
  execute_process(
    COMMAND ${Python_EXECUTABLE_CMAKE} ${CMAKE_CURRENT_BINARY_DIR}/get_python_lib.py
    OUTPUT_VARIABLE SITE_PACKAGE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  set(INSTALL_ARGS "--single-version-externally-managed --record=record.txt")
  set(CAN_USE_CMAKE_INSTALL_PREFIX_FROM_CMAKE_INSTALL FALSE)
  set(IGNORE_CMAKE_INSTALL_PREFIX_CONFIGURE_TIME_DIFFERENT_FROM_RUNTIME FALSE)
  if (DEFINED GDAL_PYTHON_INSTALL_PREFIX)
    file(TO_NATIVE_PATH "${GDAL_PYTHON_INSTALL_PREFIX}" NATIVE_GDAL_PYTHON_INSTALL_PREFIX)
    if (WIN32)
      string(REPLACE "\\" "\\\\" NATIVE_GDAL_PYTHON_INSTALL_PREFIX "${NATIVE_GDAL_PYTHON_INSTALL_PREFIX}")
    endif ()
    set(INSTALL_ARGS "${INSTALL_ARGS} \"--prefix=${NATIVE_GDAL_PYTHON_INSTALL_PREFIX}\"")
    set(IGNORE_CMAKE_INSTALL_PREFIX_CONFIGURE_TIME_DIFFERENT_FROM_RUNTIME TRUE)
  elseif (CMAKE_INSTALL_PREFIX)
    if (WIN32 AND _isMultiConfig)
      file(TO_NATIVE_PATH "${CMAKE_INSTALL_PREFIX}" NATIVE_CMAKE_INSTALL_PREFIX)
      string(REPLACE "\\" "\\\\" NATIVE_CMAKE_INSTALL_PREFIX "${NATIVE_CMAKE_INSTALL_PREFIX}")
      set(INSTALL_ARGS "${INSTALL_ARGS} \"--prefix=${NATIVE_CMAKE_INSTALL_PREFIX}\"")
      set(IGNORE_CMAKE_INSTALL_PREFIX_CONFIGURE_TIME_DIFFERENT_FROM_RUNTIME TRUE)
    elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
      file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/is_std_unix_layout.py
           "import sys;print(\"FALSE\" if \"framework\" in sys.prefix.lower() else \"TRUE\")\n")
      execute_process(
        COMMAND ${Python_EXECUTABLE_CMAKE} ${CMAKE_CURRENT_BINARY_DIR}/is_std_unix_layout.py
        OUTPUT_VARIABLE STD_UNIX_LAYOUT
        OUTPUT_STRIP_TRAILING_WHITESPACE)
      if ("${STD_UNIX_LAYOUT}" STREQUAL "TRUE")
        set(CAN_USE_CMAKE_INSTALL_PREFIX_FROM_CMAKE_INSTALL TRUE)
      elseif (DEFINED GDAL_PYTHON_INSTALL_LIB)
        set(INSTALL_ARGS "${INSTALL_ARGS} \"--install-lib=${GDAL_PYTHON_INSTALL_LIB}\"")
      endif ()
    else ()
      set(CAN_USE_CMAKE_INSTALL_PREFIX_FROM_CMAKE_INSTALL TRUE)
    endif ()
  endif ()

  # setuptools 60.0 defaults to SETUPTOOLS_USE_DISTUTILS=local, and thus Debian's specific --install-layout distutils
  # option cannot be used
  if (DEFINED GDAL_PYTHON_INSTALL_LAYOUT)
    set(INSTALL_ARGS "${INSTALL_ARGS} --install-layout=${GDAL_PYTHON_INSTALL_LAYOUT}")
    set(SETUPTOOLS_USE_DISTUTILS stdlib)
  elseif ("${SITE_PACKAGE_DIR}" MATCHES "dist-packages")
    set(INSTALL_ARGS "${INSTALL_ARGS} --install-layout=deb")
    if(Python_VERSION VERSION_LESS 3.12)
        # It no longer seems needed to mess with SETUPTOOLS_USE_DISTUTILS
        # with Debian Python 3.12 (or maybe its setuptools 68.1.2 version...),
        # so only do that with older versions
        set(SETUPTOOLS_USE_DISTUTILS stdlib)
    endif()
  endif ()

  if (ONLY_GENERATE_FOR_NON_DEBUG)
    if (POLICY CMP0087)
      # install(SCRIPT) can use generator expressions, since CMake 3.14
      cmake_policy(SET CMP0087 NEW)
      configure_file(${CMAKE_CURRENT_SOURCE_DIR}/install_python.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/install_python.cmake.tmp
                     @ONLY)
      file(
        GENERATE
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/install_python_$<CONFIG>.cmake
        INPUT ${CMAKE_CURRENT_BINARY_DIR}/install_python.cmake.tmp
      )
      file(
        GENERATE
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/install_binding_$<CONFIG>.cmake
        CONTENT
          "execute_process(COMMAND $<IF:$<NOT:$<CONFIG:Debug>>,${CMAKE_COMMAND} ${WERROR_DEV_FLAG} -P \"${CMAKE_CURRENT_BINARY_DIR}/install_python_$<CONFIG>.cmake\",${CMAKE_COMMAND} -E echo \"setup.py install only run in configuration != Debug\">
                                          WORKING_DIRECTORY \"${CMAKE_CURRENT_BINARY_DIR}\")")
      install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/install_binding_$<CONFIG>.cmake)
    else ()
      message(WARNING "Installing Python bindings with a multi generator requires CMake >= 3.14")
    endif ()
  else ()
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/install_python.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/install_python.cmake"
                   @ONLY)
    install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/install_python.cmake")
  endif ()

elseif (Python_Development_FOUND)
  macro (py_ext name v source)
    set(GDAL_PYTHON_EXT_SOURCE_DIR "${GDAL_PYTHON_CSOURCE_DIR}")
    python_add_library(${name}${v} MODULE ${GDAL_PYTHON_EXT_SOURCE_DIR}/${source})
    set_property(TARGET ${name}${v} PROPERTY LIBRARY_OUTPUT_NAME ${name})
    if (_isMultiConfig)
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
    target_link_libraries(${name}${v} PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}>)
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
    py_ext(_gdal_array ${v} gdal_array_wrap.cpp)
    target_include_directories(_gdal_array${v} PRIVATE ${Python_NumPy_INCLUDE_DIRS})
    list(APPEND _py_depends _gdal_array${v})
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
