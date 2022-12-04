file(GLOB SOURCES "${TARGET_SUBDIR}/*.cs")
list(APPEND SOURCES ${SOURCE_DIR}/AssemblyInfo.cs)
set(NATIVE_SOURCES)
foreach(_src IN LISTS SOURCES)
    if(WIN32)
        file(TO_NATIVE_PATH "${_src}" _src)
    endif()
    list(APPEND NATIVE_SOURCES "${_src}")
endforeach()
execute_process(
    COMMAND ${CSHARP_COMPILER} ${CSC_OPTIONS} ${NATIVE_SOURCES}
    WORKING_DIRECTORY "${WORKING}")
