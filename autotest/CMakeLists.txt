# CMake4GDAL project is distributed under MIT license. See accompanying file LICENSE.txt.

#[[
option(AUTOTEST_DOWNLOAD_TEST_DATA "Autotest to download test data" OFF)
option(AUTOTEST_SLOW_TEST "Autotest to run slow test" OFF)
if (NOT DEFINED ENV{CTEST_PARALLEL_LEVEL})
  set(PARALLEL_OPTION "-j1")
endif ()
]]

if (Python_Interpreter_FOUND)

  include(GdalSetRuntimeEnv)
  gdal_set_runtime_env(PYTHON_RUN_ENV)

  if (WIN32)
    # If running GDAL as a CustomBuild Command os MSBuild, "ERROR bla:" is considered as failing the job. This is rarely
    # the intended behavior
    list(APPEND PYTHON_RUN_ENV "CPL_ERROR_SEPARATOR=\\;")
  endif ()

  # Set TEST_ENV that goes into pytest.ini

  # Set GDAL_DATA
  file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}/data" GDAL_DATA)
  set(_TEST_ENV GDAL_DATA=${GDAL_DATA})

  if (GDAL_DOWNLOAD_TEST_DATA)
    list(APPEND _TEST_ENV GDAL_DOWNLOAD_TEST_DATA=YES)
  else ()
    list(APPEND _TEST_ENV "#GDAL_DOWNLOAD_TEST_DATA=YES")
  endif ()
  if (GDAL_SLOW_TESTS)
    list(APPEND _TEST_ENV GDAL_RUN_SLOW_TESTS=YES)
  else ()
    list(APPEND _TEST_ENV "#GDAL_RUN_SLOW_TESTS=YES")
  endif ()
  # Conda enable PROJ_NETWORK but this does interfere with some of our tests due to some unexpected grid being used
  list(APPEND _TEST_ENV PROJ_NETWORK=OFF)

  string(REPLACE ";" "\n   " TEST_ENV "${_TEST_ENV}")

  set(AUTOTEST_LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/autotest.log")
  if ("${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}" AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/pytest.ini)
    # FIXME: remove this branch if we remove pytest.ini from source tree
  else()
    configure_file(${GDAL_CMAKE_TEMPLATE_PATH}/pytest.ini.in ${CMAKE_CURRENT_BINARY_DIR}/pytest.ini @ONLY)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/conftest.py DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
  endif()

  function (symlink_or_copy source destination)

    if (CMAKE_VERSION VERSION_GREATER 3.14)
      file(
        CREATE_LINK ${source} ${destination}
        RESULT res
        SYMBOLIC)
      if (NOT res EQUAL 0)
        message(STATUS "Copying content of ${source} to ${destination}")
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${source} ${destination})
      endif ()
    else ()
      if (NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${source} ${destination})
      else ()
        message(STATUS "Copying content of ${source} to ${destination}")
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${source} ${destination})
      endif ()
    endif ()

  endfunction ()

  # Needed for a autotest/osr test
  if (NOT "${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
      symlink_or_copy(${CMAKE_CURRENT_SOURCE_DIR}/cpp/data ${CMAKE_CURRENT_BINARY_DIR}/cpp/data)
      if (SKIP_COPYING_AUTOTEST_SUBDIRS)
        message(STATUS "Skipping copying ${CMAKE_CURRENT_SOURCE_DIR}/pymod")
      else ()
        symlink_or_copy(${CMAKE_CURRENT_SOURCE_DIR}/pymod ${CMAKE_CURRENT_BINARY_DIR}/pymod)
      endif ()
  endif()

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
    if (NOT "${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
        if (SKIP_COPYING_AUTOTEST_SUBDIRS)
          message(STATUS "Skipping copying ${CMAKE_CURRENT_SOURCE_DIR}/${tgt}")
        else ()
          symlink_or_copy(${CMAKE_CURRENT_SOURCE_DIR}/${tgt} ${CMAKE_CURRENT_BINARY_DIR}/${tgt})
        endif ()
    endif()
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
