set(GEOD_SRC geod.c
             geod_set.c geod_interface.c )
set(GEOD_INCLUDE geod_interface.h)

source_group("Source Files\\Bin" FILES ${GEOD_SRC} ${GEOD_INCLUDE})

if(WIN32)
    set(GEOD_SRC ${GEOD_SRC} emess.c)
endif(WIN32)

#Executable
add_executable(geod ${GEOD_SRC} ${GEOD_INCLUDE})
target_link_libraries(geod ${PROJ_LIBRARIES})
install(TARGETS geod
        RUNTIME DESTINATION ${BINDIR})

