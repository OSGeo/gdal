add_gdal_driver(TARGET gdal_HEIF
                SOURCES heifdataset.cpp heifdataset.h 
                        heifdatasetcreatecopy.cpp
                        boxes/box.cpp boxes/box.h
                        boxes/fullbox.cpp boxes/fullbox.h
                        boxes/cmpd.cpp boxes/cmpd.h
                        boxes/hdlr.cpp boxes/hdlr.h
                        boxes/iloc.cpp boxes/iloc.h
                        boxes/iprp.cpp boxes/iprp.h 
                        boxes/ispe.cpp boxes/ispe.h
                        boxes/mdat.cpp boxes/mdat.h
                        boxes/meta.cpp boxes/meta.h
                        boxes/pitm.cpp boxes/pitm.h
                        boxes/uncc.cpp boxes/uncc.h
                CORE_SOURCES heifdrivercore.cpp
                PLUGIN_CAPABLE)

if(TARGET gdal_HEIF_core)
    target_include_directories(gdal_HEIF_core PRIVATE $<TARGET_PROPERTY:HEIF::HEIF,INTERFACE_INCLUDE_DIRECTORIES>)
    target_compile_definitions(gdal_HEIF_core PRIVATE $<TARGET_PROPERTY:HEIF::HEIF,INTERFACE_COMPILE_DEFINITIONS>)
endif()

if(NOT TARGET gdal_HEIF)
    return()
endif()

gdal_standard_includes(gdal_HEIF)
gdal_target_link_libraries(gdal_HEIF PRIVATE HEIF::HEIF)
