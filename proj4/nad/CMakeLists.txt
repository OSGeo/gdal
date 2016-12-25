#
# files containing dictionnary of useful projection
#

set(PROJ_DICTIONARY  epsg
                     esri
                     world
                     esri.extra
                     other.extra
                     IGNF
                     nad27
                     GL27
                     nad83
                     nad.lst
                     proj_def.dat
                     CH )

#
# gridshift file
#

file(GLOB GSB_FILES  *.gsb)
set(GRIDSHIFT_FILES ${GSB_FILES})
set(GRIDSHIFT_FILES ${GRIDSHIFT_FILES}
                     )
option(CONVERT_DATA "convert some ascii file to binary file for use in proj4" OFF)
if(CONVERT_DATA AND nad2bin)
   message(ERROR " you need to compile nad2bin exe in order to convert data file" )
else(CONVERT_DATA AND nad2bin)
   set(LLA_GRID_DIR ${CMAKE_CURRENT_SOURCE_DIR})
   proj_convert_grid_lla2gsb(LLA_GRID_DIR)
   file(GLOB LLA_FILES  *.lla)
   proj_append_lla_output_file(LLA_FILES GRIDSHIFT_FILES)
endif(CONVERT_DATA AND nad2bin)


#
#install
#
set(ALL_DATA_FILE ${PROJ_DICTIONARY}
                  ${GRIDSHIFT_FILES}
                  ${GEOID_FILES})
install(FILES ${ALL_DATA_FILE}
       DESTINATION ${DATADIR})

#
# test
#
set(CS2CS_BIN "cs2cs")
set(PROJ_BIN "proj")
proj_add_test_script_sh("test27" PROJ_BIN )
proj_add_test_script_sh("test83" PROJ_BIN )
proj_add_test_script_sh("testvarious" CS2CS_BIN )
proj_add_test_script_sh("testdatumfile" CS2CS_BIN "connu")
proj_add_test_script_sh("testIGNF" CS2CS_BIN "ntf_r93.gsb")
proj_add_test_script_sh("testntv2" CS2CS_BIN "ntv2_0.gsb")

