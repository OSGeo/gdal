function(gdal_set_runtime_env res)
  # Set PATH / LD_LIBRARY_PATH
  set(GDAL_OUTPUT_DIR "$<SHELL_PATH:$<TARGET_FILE_DIR:${GDAL_LIB_TARGET_NAME}>>")
  if(BUILD_APPS)
      set(GDAL_APPS_OUTPUT_DIR "$<SHELL_PATH:$<TARGET_FILE_DIR:gdalinfo>>")
  endif()

  if(TARGET gtest_for_gdal)
      if(USE_EXTERNAL_GTEST)
          if(TARGET GTest::gtest)
              set(GTEST_DIR "$<SHELL_PATH:$<TARGET_FILE_DIR:GTest::gtest>>")
          else()
              set(GTEST_DIR "$<SHELL_PATH:$<TARGET_FILE_DIR:GTest::GTest>>")
          endif()
      else()
          set(GTEST_DIR "$<SHELL_PATH:$<TARGET_FILE_DIR:gtest_for_gdal>>")
      endif()
  endif()

  if (Python_Interpreter_FOUND)
    set(GDAL_PYTHON_SCRIPTS_DIR "${PROJECT_BINARY_DIR}/swig/python/gdal-utils/scripts")
  endif()

  if (WIN32)
    string(REPLACE ";" "\\;" PATH_ESCAPED "$ENV{PATH}")
    if (Python_Interpreter_FOUND)
      file(TO_NATIVE_PATH "${GDAL_PYTHON_SCRIPTS_DIR}" NATIVE_GDAL_PYTHON_SCRIPTS_DIR)
      string(REPLACE ";" "\\;" NATIVE_GDAL_PYTHON_SCRIPTS_DIR_WITH_SEP "${NATIVE_GDAL_PYTHON_SCRIPTS_DIR};")
    endif()
    if (GDAL_APPS_OUTPUT_DIR)
      set(GDAL_APPS_OUTPUT_DIR_WITH_SEP "${GDAL_APPS_OUTPUT_DIR}\\;")
    endif()
    if (GTEST_DIR)
      set(SEP_GTEST_DIR "\\;${GTEST_DIR}")
    endif()
    list(APPEND RUNTIME_ENV "PATH=${GDAL_OUTPUT_DIR}\\;${GDAL_APPS_OUTPUT_DIR_WITH_SEP}${NATIVE_GDAL_PYTHON_SCRIPTS_DIR_WITH_SEP}${PATH_ESCAPED}${SEP_GTEST_DIR}")
  else ()
    if (Python_Interpreter_FOUND)
      set(GDAL_PYTHON_SCRIPTS_DIR_WITH_SEP "${GDAL_PYTHON_SCRIPTS_DIR}:")
    endif()
    if (GDAL_APPS_OUTPUT_DIR)
      set(GDAL_APPS_OUTPUT_DIR_WITH_SEP "${GDAL_APPS_OUTPUT_DIR}:")
    endif()
    if (GTEST_DIR)
      set(SEP_GTEST_DIR ":${GTEST_DIR}")
    endif()
    list(APPEND RUNTIME_ENV "PATH=${GDAL_APPS_OUTPUT_DIR_WITH_SEP}${GDAL_PYTHON_SCRIPTS_DIR_WITH_SEP}$ENV{PATH}")
    # We put GTEST_DIR at the end as it can contains a path like /usr/lib/x86_64-linux-gnu
    # and we want to see it as close to the end as possible to avoid clashes
    # if using a custom libproj.so.X in a non-system directory, and aliasing it
    # to the system library (definitely not a recommended setup! but something
    # I use)
    list(APPEND RUNTIME_ENV "LD_LIBRARY_PATH=${GDAL_OUTPUT_DIR}:$ENV{LD_LIBRARY_PATH}${SEP_GTEST_DIR}")
  endif ()

  list(APPEND RUNTIME_ENV "PYTHONPATH=${PROJECT_BINARY_DIR}/swig/python/")

  # Set GDAL_DRIVER_PATH We request the TARGET_FILE_DIR of one of the plugins, since the PLUGIN_OUTPUT_DIR will not
  # contain the \Release suffix with MSVC generator
  get_property(PLUGIN_MODULES GLOBAL PROPERTY PLUGIN_MODULES)
  list(LENGTH PLUGIN_MODULES PLUGIN_MODULES_LENGTH)
  if (PLUGIN_MODULES_LENGTH GREATER_EQUAL 1)
    list(GET PLUGIN_MODULES 0 FIRST_TARGET)
    set(PLUGIN_OUTPUT_DIR "$<SHELL_PATH:$<TARGET_FILE_DIR:${FIRST_TARGET}>>")
    list(APPEND RUNTIME_ENV "GDAL_DRIVER_PATH=${PLUGIN_OUTPUT_DIR}")
  else ()
    # if no plugins are configured, set up a dummy path, to avoid loading installed plugins (from a previous install
    # execution) that could have a different ABI
    list(APPEND RUNTIME_ENV "GDAL_DRIVER_PATH=dummy")
  endif ()

  # Set GDAL_DATA
  if(WIN32)
      file(TO_NATIVE_PATH "${PROJECT_BINARY_DIR}/data" GDAL_DATA)
  else()
      set(GDAL_DATA "${PROJECT_BINARY_DIR}/data")
  endif()
  list(APPEND RUNTIME_ENV "GDAL_DATA=${GDAL_DATA}")

  set(${res} "${RUNTIME_ENV}" PARENT_SCOPE)
endfunction()
