# proj-config.cmake for the install tree.  It's installed in
# ${INSTALL_CMAKE_DIR} and @PROJECT_ROOT_DIR@ is the relative
# path to the root from there.  (Note that the whole install tree can
# be relocated.)
if (NOT WIN32)
  set (INSTALL_CMAKE_DIR "share/cmake/${PROJECT_NAME}")
  set (PROJECT_ROOT_DIR "../../..")
else ()
  set (INSTALL_CMAKE_DIR "cmake")
  set (PROJECT_ROOT_DIR "..")
endif ()

string(TOLOWER "${PROJECT_NAME}" PROJECT_NAME_LOWER)
configure_file (project-config.cmake.in project-config.cmake @ONLY)
configure_file (project-config-version.cmake.in
  project-config-version.cmake @ONLY)
install (FILES
  "${CMAKE_CURRENT_BINARY_DIR}/project-config.cmake"
  DESTINATION "${INSTALL_CMAKE_DIR}"
  RENAME "${PROJECT_NAME_LOWER}-config.cmake")
install (FILES
  "${CMAKE_CURRENT_BINARY_DIR}/project-config-version.cmake"
  DESTINATION "${INSTALL_CMAKE_DIR}"
  RENAME "${PROJECT_NAME_LOWER}-config-version.cmake")
# Make information about the cmake targets (the library and the tools)
# available.
install (EXPORT targets
  FILE ${PROJECT_NAME_LOWER}-targets.cmake
  DESTINATION "${INSTALL_CMAKE_DIR}")
