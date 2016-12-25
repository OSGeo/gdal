set(PROJ_SRC proj.c 
             gen_cheb.c 
             p_series.c)

source_group("Source Files\\Bin" FILES ${PROJ_SRC})

if(WIN32)
    set(PROJ_SRC ${PROJ_SRC} emess.c)
endif(WIN32)
    
#Executable
add_executable(binproj ${PROJ_SRC})
set_target_properties(binproj
    PROPERTIES
    OUTPUT_NAME proj)
target_link_libraries(binproj ${PROJ_LIBRARIES})
install(TARGETS binproj 
        RUNTIME DESTINATION ${BINDIR})

