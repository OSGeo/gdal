#
# add test with sh script
#

function(proj_add_test_script_sh SH_NAME BIN_USE)
  if(UNIX)
    get_filename_component(testname ${SH_NAME} NAME_WE)
    
    set(TEST_OK 1)
    if(ARGV2)
         set(TEST_OK 0)
         set(GRID_FULLNAME ${PROJECT_SOURCE_DIR}/nad/${ARGV2})
         if(EXISTS ${GRID_FULLNAME})
            set(TEST_OK 1)
         endif(EXISTS ${GRID_FULLNAME})
    endif(ARGV2)
    
    if( CMAKE_VERSION VERSION_LESS 2.8.4 )
       set(TEST_OK 0)
       message(STATUS "test with bash script need a cmake version >= 2.8.4")
    endif()
 
    if(${TEST_OK})
      add_test( NAME "${testname}"
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/nad
                COMMAND  ${PROJECT_SOURCE_DIR}/nad/${SH_NAME} 
                         ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${${BIN_USE}} 
               )
    endif(${TEST_OK})
    
  endif(UNIX)
endfunction()
