if (Python_Interpreter_FOUND)

  include(GdalSetRuntimeEnv)
  gdal_set_runtime_env(RUN_ENV)

  add_custom_target(
    completion
    COMMAND ${CMAKE_COMMAND} -E env ${RUN_ENV} ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/completionFinder.py
            ${CMAKE_CURRENT_SOURCE_DIR}/gdal-bash-completion.sh
    COMMENT "Refresh ${CMAKE_CURRENT_SOURCE_DIR}/gdal-bash-completion.sh")

else ()
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/completion_error.cmake
       "message(FATAL_ERROR \"Python is needed for completion target\")")
  add_custom_target(completion COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/completion_error.cmake)
endif ()

find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
  pkg_check_modules(PC_BASH_COMPLETION QUIET bash-completion)
  if (PC_BASH_COMPLETION_FOUND)
    pkg_get_variable(BASH_COMPLETIONS_FULL_DIR bash-completion completionsdir)
    pkg_get_variable(BASH_COMPLETIONS_PREFIX bash-completion prefix)
    if (BASH_COMPLETIONS_FULL_DIR
        AND BASH_COMPLETIONS_PREFIX
        AND BASH_COMPLETIONS_FULL_DIR MATCHES "^${BASH_COMPLETIONS_PREFIX}/")
      string(REGEX REPLACE "^${BASH_COMPLETIONS_PREFIX}/" "" BASH_COMPLETIONS_DIR_DEFAULT ${BASH_COMPLETIONS_FULL_DIR})
    endif ()
  endif ()
endif ()

if (NOT DEFINED BASH_COMPLETIONS_DIR_DEFAULT AND NOT GDAL_ENABLE_MACOSX_FRAMEWORK)
  include(GNUInstallDirs)
  set(BASH_COMPLETIONS_DIR_DEFAULT ${CMAKE_INSTALL_DATADIR}/bash-completion/completions)
endif ()

set(BASH_COMPLETIONS_DIR
    "${BASH_COMPLETIONS_DIR_DEFAULT}"
    CACHE PATH "Installation sub-directory for bash completion scripts")

if (NOT BASH_COMPLETIONS_DIR STREQUAL "")
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/install_bash_completions.cmake.in
                 ${CMAKE_CURRENT_BINARY_DIR}/install_bash_completions.cmake @ONLY)
  install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/install_bash_completions.cmake)
endif ()
