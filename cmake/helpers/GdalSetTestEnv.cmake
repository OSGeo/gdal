function(gdal_set_test_env res)
  # Set PATH / LD_LIBRARY_PATH
  set(GDAL_OUTPUT_DIR "$<SHELL_PATH:$<TARGET_FILE_DIR:${GDAL_LIB_TARGET_NAME}>>")
  if (WIN32)
    string(REPLACE ";" "\\;" PATH_ESCAPED "$ENV{PATH}")
    list(APPEND TEST_ENV "PATH=${GDAL_OUTPUT_DIR}\\;${PATH_ESCAPED}")
  else ()
    list(APPEND TEST_ENV "LD_LIBRARY_PATH=${GDAL_OUTPUT_DIR}:$ENV{LD_LIBRARY_PATH}")
  endif ()

  # Set GDAL_DRIVER_PATH We request the TARGET_FILE_DIR of one of the plugins, since the PLUGIN_OUTPUT_DIR will not
  # contain the \Release suffix with MSVC generator
  get_property(PLUGIN_MODULES GLOBAL PROPERTY PLUGIN_MODULES)
  list(LENGTH PLUGIN_MODULES PLUGIN_MODULES_LENGTH)
  if (PLUGIN_MODULES_LENGTH GREATER_EQUAL 1)
    list(GET PLUGIN_MODULES 0 FIRST_TARGET)
    set(PLUGIN_OUTPUT_DIR "$<SHELL_PATH:$<TARGET_FILE_DIR:${FIRST_TARGET}>>")
    list(APPEND TEST_ENV "GDAL_DRIVER_PATH=${PLUGIN_OUTPUT_DIR}")
  else ()
    # if no plugins are configured, set up a dummy path, to avoid loading installed plugins (from a previous install
    # execution) that could have a different ABI
    list(APPEND TEST_ENV "GDAL_DRIVER_PATH=dummy")
  endif ()

  # Set GDAL_DATA
  file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}/data" GDAL_DATA)
  list(APPEND TEST_ENV "GDAL_DATA=${GDAL_DATA}")

  set(${res} "${TEST_ENV}" PARENT_SCOPE)
endfunction()
