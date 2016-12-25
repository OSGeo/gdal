if(WIN32 AND BUILD_LIBPROJ_SHARED)
    message(warning " nad2nad can't be build with a DLL proj4 library you need a static lib")
endif(WIN32 AND BUILD_LIBPROJ_SHARED)


set(NAD2BIN_SRC nad2bin.c)
source_group("Source Files\\Bin" FILES ${NAD2BIN_SRC})
if(WIN32)
    set(NAD2BIN_SRC ${NAD2BIN_SRC} emess.c)
endif(WIN32)
    
#Executable
add_executable(nad2bin ${NAD2BIN_SRC})
target_link_libraries(nad2bin ${PROJ_LIBRARIES})
install(TARGETS nad2bin 
        RUNTIME DESTINATION ${BINDIR})

