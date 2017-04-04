# ******************************************************************************
# * Project:  CMake4GDAL
# * Purpose:  CMake build scripts
# * Author: Dmitriy Baryshnikov (aka Bishop), polimax@mail.ru
# ******************************************************************************
# * Copyright (C) 2012,2013 Bishop
# * 
# * Permission is hereby granted, free of charge, to any person obtaining a
# * copy of this software and associated documentation files (the "Software"),
# * to deal in the Software without restriction, including without limitation
# * the rights to use, copy, modify, merge, publish, distribute, sublicense,
# * and/or sell copies of the Software, and to permit persons to whom the
# * Software is furnished to do so, subject to the following conditions:
# *
# * The above copyright notice and this permission notice shall be included
# * in all copies or substantial portions of the Software.
# *
# * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# * DEALINGS IN THE SOFTWARE.
# ******************************************************************************

cmake_minimum_required (VERSION 2.8.10)

set(LIB_NAME "ogr")
project ("lib${LIB_NAME}")

# there are a problems with: 
# 1. ogrsf_frmts/generic - which is not driver
# 2. ogrsf_frmts/mitab - which need to link whole lib because OGRLayerAttrIndex CPL_DLL *OGRCreateDefaultLayerIndex();
#add_definitions(-DHAVE_MITAB)

include_directories(${GDAL_ROOT_SOURCE_DIR}/gcore)
include_directories(${GDAL_ROOT_SOURCE_DIR}/ogr)
include_directories(${GDAL_ROOT_SOURCE_DIR}/ogr/ogrsf_frmts)  


find_package(GEOS)
if(GEOS_FOUND)
	get_filename_component(GEOS_ROOT ${GEOS_INCLUDE_DIR} PATH)
    include_directories(${GEOS_INCLUDE_DIR})
    include_directories(${GEOS_ROOT}/capi)	
	add_definitions(-DHAVE_GEOS=1)
	set(GDAL_HAVE_GEOS ON PARENT_SCOPE)
endif(GEOS_FOUND)

find_package(PROJ)
if(PROJ_FOUND)
    include_directories(${PROJ_INCLUDE_DIR})
    add_definitions(-DHAVE_PROJ -DPROJ_STATIC)
endif(PROJ_FOUND)

#check zip and internal zip
if(GDAL_USE_LIBZ)
	add_definitions(-DHAVE_LIBZ -DZIP_SUPPORT)
	if(GDAL_USE_LIBZ_INTERNAL)
		include_directories(${GDAL_ROOT_SOURCE_DIR}/frmts/zlib)
		# add_subdirectory(${GDAL_ROOT_SOURCE_DIR}/frmts/zlib ${GDAL_ROOT_BINARY_DIR}/frmts/zlib)
	else(GDAL_USE_LIBZ_INTERNAL)
		find_package(ZLIB REQUIRED)
		if(ZLIB_FOUND)
			include_directories(${ZLIB_INCLUDE_DIRS})
			add_definitions(-DHAVE_ZLIB_H -DHAVE_ZLIB)
			if(MSVC)
				add_definitions(-DZLIB_DLL)
			endif(MSVC)
		else(ZLIB_FOUND)	
			MESSAGE(FATAL_ERROR "Could not find ZLIB")
		endif(ZLIB_FOUND)		
	endif(GDAL_USE_LIBZ_INTERNAL)
endif(GDAL_USE_LIBZ)

set(LIB_INST_HEADERS
	ogr_api.h
	ogr_core.h
	ogr_feature.h
	ogr_featurestyle.h
	ogr_geometry.h
	ogr_p.h
	ogr_spatialref.h
	ogr_srs_api.h
)

set(LIB_HHEADERS ${LIB_INST_HEADERS}
	ogrsf_frmts/ogrsf_frmts.h
	ogrsf_frmts/ogr_attrind.h
    ogrgeomediageometry.h
    ogrpgeogeometry.h
    ogr_geos.h
    ogr_srs_esri_names.h
    swq.h
    osr_cs_wkt.h
    osr_cs_wkt_parser.h
)

set(LIB_CSOURCES
    gml2ogrgeometry.cpp 
    ogr2gmlgeometry.cpp 
    ograssemblepolygon.cpp
    ogrct.cpp 
    ogrcurve.cpp
    ogrfeature.cpp
    ogrfeaturedefn.cpp
    ogrgeomfielddefn.cpp
    ogrfeaturestyle.cpp 
    ogrfielddefn.cpp
    ogrgeometry.cpp 
    ogrgeometrycollection.cpp 
    ogrgeometryfactory.cpp
    ogrlinearring.cpp 
    ogrlinestring.cpp 
    ogrmultilinestring.cpp
    ogrmultipoint.cpp 
    ogrmultipolygon.cpp
    ogrpoint.cpp
    ogrpolygon.cpp
    ogrspatialreference.cpp 
    ogrsurface.cpp
    ogrutils.cpp
    ogr_api.cpp 
    # #ogr_capi_test.c
    ogr_fromepsg.cpp
    ogr_opt.cpp 
    ogr_srsnode.cpp 
    ogr_srs_dict.cpp
    ogr_srs_erm.cpp 
    ogr_srs_esri.cpp
    ogr_srs_ozi.cpp 
    ogr_srs_panorama.cpp
    ogr_srs_pci.cpp 
    ogr_srs_proj4.cpp 
    ogr_srs_usgs.cpp
    ogr_srs_validate.cpp
    ogr_srs_xml.cpp
    osr_cs_wkt.c    
    osr_cs_wkt_parser.c
    swq.cpp 
    swq_expr_node.cpp 
    swq_op_general.cpp
    swq_op_registrar.cpp
    swq_parser.cpp
    swq_select.cpp 
    ogrgeomediageometry.cpp 
	ogrpgeogeometry.cpp 
    ogrfeaturequery.cpp 
)

# needed packages
find_package(EXPAT)
if(EXPAT_FOUND)
    include_directories(${EXPAT_INCLUDE_DIRS})
    add_definitions(-DHAVE_EXPAT)
	set(GDAL_HAVE_EXPAT ON PARENT_SCOPE)
	set(LIB_HHEADERS ${LIB_HHEADERS}
		ogr_expat.h
	)
	set(LIB_CSOURCES ${LIB_CSOURCES}
		ogr_expat.cpp
	)
endif(EXPAT_FOUND)

add_subdirectory(ogrsf_frmts)

set(GDAL_TARGET_OBJECTS ${GDAL_TARGET_OBJECTS} $<TARGET_OBJECTS:${LIB_NAME}> PARENT_SCOPE)
add_library(${LIB_NAME} OBJECT ${LIB_HHEADERS} ${LIB_CSOURCES})

set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB})

if(EXPAT_FOUND)
	set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB} ${EXPAT_LIBRARIES})
endif(EXPAT_FOUND)

if(ZLIB_FOUND)
	set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB} ${ZLIB_LIBRARIES})
endif(ZLIB_FOUND)

if(GEOS_FOUND)
	set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB} ${GEOS_LIBRARY})
endif(GEOS_FOUND)

if(PROJ_FOUND)
    set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB} ${PROJ_LIBRARY})
endif(PROJ_FOUND)

set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB} PARENT_SCOPE)

install(FILES ${LIB_INST_HEADERS} DESTINATION include/gdal)
