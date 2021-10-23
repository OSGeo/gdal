option(GDAL_ENABLE_FRMT_JP2LURA_PLUGIN "Build LURA Driver as plugin" ON)
set(_SOURCES jp2luracallbacks.h jp2luradataset.h jp2lurarasterband.h jp2luracallbacks.cpp jp2luradataset.cpp
             jp2lurarasterband.cpp)
if (GDAL_ENABLE_FRMT_JP2LURA_PLUGIN)
  add_gdal_driver(
    TARGET gdal_JP2LURA
    SOURCES ${_SOURCES}
    PLUGIN)
else ()
  add_gdal_driver(TARGET gdal_JP2LURA SOURCES ${_SOURCES})
endif ()
unset(_SOURCES)
gdal_standard_includes(gdal_JP2LURA)
target_include_directories(gdal_JP2LURA PRIVATE ${LURATECH_JP2SDK_DIRECTORY}/include)
gdal_target_link_libraries(TARGET gdal_JP2LURA LIBRARIES ${LURATECH_JP2SDK_LIBRARIES})
