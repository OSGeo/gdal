# CMake4GDAL project is distributed under X/MIT license. See accompanying file LICENSE.txt.

option(AUTOTEST_DOWNLOAD_TEST_DATA "Autotest to download test data" OFF)
option(AUTOTEST_SLOW_TEST "Autotest to run slow test" OFF)
if (NOT DEFINED ENV{CTEST_PARALLEL_LEVEL})
  set(PARALLEL_OPTION "-j1")
endif ()

if (Python_Interpreter_FOUND)

  set(PYTHON_RUN_ENV)

  # Set PATH / LD_LIBRARY_PATH
  set(GDAL_OUTPUT_DIR "$<SHELL_PATH:$<TARGET_FILE_DIR:${GDAL_LIB_TARGET_NAME}>>")
  set(GDAL_APPS_OUTPUT_DIR "$<SHELL_PATH:$<TARGET_FILE_DIR:gdalinfo>>")

  if (WIN32)
    # If running GDAL as a CustomBuild Command os MSBuild, "ERROR bla:" is considered as failing the job. This is rarely
    # the intended behavior
    list(APPEND PYTHON_RUN_ENV "CPL_ERROR_SEPARATOR=\\;")

    string(REPLACE ";" "\\;" PATH_ESCAPED "$ENV{PATH}")
    list(APPEND PYTHON_RUN_ENV "PATH=${GDAL_OUTPUT_DIR}\\;${GDAL_APPS_OUTPUT_DIR}\\;${PATH_ESCAPED}")
  else ()
    list(APPEND PYTHON_RUN_ENV "PATH=${GDAL_APPS_OUTPUT_DIR}:$ENV{PATH}")
    list(APPEND PYTHON_RUN_ENV "LD_LIBRARY_PATH=${GDAL_OUTPUT_DIR}:$ENV{LD_LIBRARY_PATH}")
  endif ()

  list(APPEND PYTHON_RUN_ENV "PYTHONPATH=${PROJECT_BINARY_DIR}/swig/python/")

  # Set GDAL_DRIVER_PATH We request the TARGET_FILE_DIR of one of the plugins, since the PLUGIN_OUTPUT_DIR will not
  # contain the \Release suffix with MSVC generator
  get_property(PLUGIN_MODULES GLOBAL PROPERTY PLUGIN_MODULES)
  list(LENGTH PLUGIN_MODULES PLUGIN_MODULES_LENGTH)
  if (PLUGIN_MODULES_LENGTH GREATER_EQUAL 1)
    list(GET PLUGIN_MODULES 0 FIRST_TARGET)
    set(PLUGIN_OUTPUT_DIR "$<SHELL_PATH:$<TARGET_FILE_DIR:${FIRST_TARGET}>>")
    list(APPEND PYTHON_RUN_ENV "GDAL_DRIVER_PATH=${PLUGIN_OUTPUT_DIR}")
  else ()
    # if no plugins are configured, set up a dummy path, to avoid loading installed plugins (from a previous install
    # execution) that could have a different ABI
    list(APPEND PYTHON_RUN_ENV "GDAL_DRIVER_PATH=dummy")
  endif ()

  # Set TEST_ENV that goes into pytest.ini

  # Set GDAL_DATA
  file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}/data" GDAL_DATA)
  set(_TEST_ENV GDAL_DATA=${GDAL_DATA})

  if (GDAL_DOWNLOAD_TEST_DATA)
    list(APPEND _TEST_ENV GDAL_DOWNLOAD_TEST_DATA=YES)
  else ()
    list(APPEND _TEST_ENV GDAL_DOWNLOAD_TEST_DATA=NO)
  endif ()
  if (GDAL_SLOW_TESTS)
    list(APPEND _TEST_ENV GDAL_RUN_SLOW_TESTS=YES)
  else ()
    list(APPEND _TEST_ENV GDAL_RUN_SLOW_TESTS=NO)
  endif ()
  # Conda enable PROJ_NETWORK but this does interfere with some of our tests due to some unexpected grid being used
  list(APPEND _TEST_ENV PROJ_NETWORK=OFF)

  string(REPLACE ";" "\n   " TEST_ENV "${_TEST_ENV}")

  set(AUTOTEST_LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/autotest.log")
  configure_file(${GDAL_CMAKE_TEMPLATE_PATH}/pytest.ini.in ${CMAKE_CURRENT_BINARY_DIR}/pytest.ini @ONLY)
  file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/conftest.py DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

  function (symlink_or_copy source destination)

    if (CMAKE_VERSION VERSION_GREATER 3.14)
      file(
        CREATE_LINK ${source} ${destination}
        RESULT res
        SYMBOLIC)
      if (NOT res EQUAL 0)
        message("Copying content of ${source} to ${destination}")
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${source} ${destination})
      endif ()
    else ()
      if (NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${source} ${destination})
      else ()
        message("Copying content of ${source} to ${destination}")
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${source} ${destination})
      endif ()
    endif ()

  endfunction ()

  # Needed for a autotest/osr test
  symlink_or_copy(${CMAKE_CURRENT_SOURCE_DIR}/cpp/data ${CMAKE_CURRENT_BINARY_DIR}/cpp/data)
  if (NOT SKIP_COPYING_AUTOTEST_SUBDIRS)
    symlink_or_copy(${CMAKE_CURRENT_SOURCE_DIR}/pymod ${CMAKE_CURRENT_BINARY_DIR}/pymod)
  endif ()

  foreach (
    tgt IN
    ITEMS ogr
          gcore
          gdrivers
          alg
          osr
          gnm
          pyscripts
          utilities)
    if (NOT SKIP_COPYING_AUTOTEST_SUBDIRS)
      symlink_or_copy(${CMAKE_CURRENT_SOURCE_DIR}/${tgt} ${CMAKE_CURRENT_BINARY_DIR}/${tgt})
    endif ()
    add_custom_target(
      autotest_${tgt}
      COMMAND ${CMAKE_COMMAND} -E env ${PYTHON_RUN_ENV} ${Python_EXECUTABLE} -m pytest -c
              ${CMAKE_CURRENT_BINARY_DIR}/pytest.ini ${tgt}
      DEPENDS ${GDAL_LIB_TARGET_NAME} gdalapps python_binding)
    add_test(NAME autotest_${tgt} COMMAND ${Python_EXECUTABLE} -m pytest -c ${CMAKE_CURRENT_BINARY_DIR}/pytest.ini
                                          ${tgt})
    set_property(TEST autotest_${tgt} PROPERTY ENVIRONMENT "${PYTHON_RUN_ENV}")
  endforeach ()
  add_custom_target(
    autotest
    COMMAND ${CMAKE_COMMAND} -E env ${PYTHON_RUN_ENV} ${Python_EXECUTABLE} -m pytest -c
            ${CMAKE_CURRENT_BINARY_DIR}/pytest.ini
    DEPENDS ${GDAL_LIB_TARGET_NAME} gdalapps python_binding)

  # Generating Python bindings in Debug mode of MSVC tends to be problematic, since it requires a python debug library
  # not easily found, hence we disable pytest_runner for that situation We might not need it at all
  get_property(_isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
  if (Python_Development_FOUND AND NOT (_isMultiConfig AND WIN32))
    file(
      WRITE ${CMAKE_CURRENT_BINARY_DIR}/pytest_runner.cpp
      "
#include <string>
#include <Python.h>
int main(int argc, char **argv) {
    std::string args;
    if ( argc > 1) {
        args.append(\"[\");
        for (int i = 1; i < argc; i++) {
            if (i > 2)
                args.append(\",\");
            args.append(\"\\\"\");
            args.append(argv[i]);
            args.append(\"\\\"\");
        }
        args.append(\"]\");
    }
    std::string pycode = \"import pytest\\npytest.main(\" + args + \")\\n\";
    wchar_t * program_name = Py_DecodeLocale(argv[0], NULL);
    Py_SetProgramName(program_name);
    Py_Initialize();
    PyRun_SimpleString(&*pycode.begin());
    Py_Finalize();
    return 0;
}")
    add_executable(pytest_runner ${CMAKE_CURRENT_BINARY_DIR}/pytest_runner.cpp)
    set_property(TARGET pytest_runner PROPERTY CXX_STANDARD 11)
    target_link_libraries(pytest_runner PUBLIC ${GDAL_LIB_TARGET_NAME})
    target_include_directories(pytest_runner PRIVATE ${Python_INCLUDE_DIRS})
    target_link_libraries(pytest_runner PRIVATE ${Python_LIBRARIES})
    add_custom_target(
      autotest_runner
      COMMAND ${CMAKE_COMMAND} -E env ${PYTHON_RUN_ENV} $<TARGET_NAME:pytest_runner> -c
              ${CMAKE_CURRENT_BINARY_DIR}/pytest.ini
      DEPENDS ${GDAL_LIB_TARGET_NAME} gdalapps python_binding pytest_runner
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  endif ()
endif ()
