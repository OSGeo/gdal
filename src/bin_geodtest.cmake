set(GEODTEST_SRC geodtest.c )
set(GEODTEST_INCLUDE)

source_group("Source Files\\Bin" FILES ${GEODTEST_SRC} ${GEODTEST_INCLUDE})

#Executable
add_executable(geodtest ${GEODTEST_SRC} ${GEODTEST_INCLUDE})
target_link_libraries(geodtest ${PROJ_LIBRARIES})
# Do not install

# Instead run as a test
add_test (NAME geodesic-test COMMAND geodtest)
