set(CS2CS_SRC cs2cs.c 
              gen_cheb.c 
              p_series.c)

source_group("Source Files\\Bin" FILES ${CS2CS_SRC})

if(WIN32)
    set(CS2CS_SRC ${CS2CS_SRC} emess.c)
endif(WIN32)

add_executable(cs2cs ${CS2CS_SRC} ${CS2CS_INCLUDE})
target_link_libraries(cs2cs ${PROJ_LIBRARIES})
install(TARGETS cs2cs 
        RUNTIME DESTINATION ${BINDIR})
