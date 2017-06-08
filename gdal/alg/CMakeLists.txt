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

set(LIB_NAME "alg")
project ("lib${LIB_NAME}")

include_directories(${GDAL_ROOT_SOURCE_DIR}/frmts/gtiff)

find_package(GEOS)
if(GEOS_FOUND)
	get_filename_component(GEOS_ROOT ${GEOS_INCLUDE_DIR} PATH)
    include_directories(${GEOS_INCLUDE_DIR})
    include_directories(${GEOS_ROOT}/capi)	
	add_definitions(-DHAVE_GEOS=1)
	set(GDAL_HAVE_GEOS ON PARENT_SCOPE)
endif(GEOS_FOUND)

find_package(Armadillo)
if(ARMADILLO_FOUND)
	include_directories(${GEOS_INCLUDE_DIRS})
	add_definitions(-DHAVE_ARMADILLO)
	set(GDAL_HAVE_ARMADILLO ON PARENT_SCOPE)
endif(ARMADILLO_FOUND)

if(GDAL_USE_CPL)
	add_definitions(-DUSE_CPL)
endif(GDAL_USE_CPL)

if(GDAL_PAM_ENABLED)
	add_definitions(-DPAM_ENABLED)
endif(GDAL_PAM_ENABLED)

include_directories(${GDAL_ROOT_SOURCE_DIR}/gcore)
include_directories(${GDAL_ROOT_SOURCE_DIR}/frmts/vrt)

set(LIB_HHEADERS
        gdalgrid.h
        gdalwarper.h
        gdal_alg.h
        gdal_alg_priv.h
        gdalgrid.h
        gdalgrid_priv.h
        gvgcpfit.h
        thinplatespline.h
        gdal_simplesurf.h
)

set(LIB_CSOURCES
        contour.cpp
        delaunay.c
        gdal_crs.c
        gdal_nrgcrs.c
        gdal_octave.cpp
        gdal_rpc.cpp
        gdal_tps.cpp
        gdalchecksum.cpp
        gdalcutline.cpp
        gdaldither.cpp
        gdalgeoloc.cpp
        gdalgrid.cpp
        gdalmediancut.cpp
        gdalproximity.cpp
        gdalrasterize.cpp
        gdalrasterpolygonenumerator.cpp
        gdalsievefilter.cpp
        gdalsimplewarp.cpp
        gdaltransformer.cpp
        gdalwarper.cpp
        gdalwarpkernel.cpp
        gdalwarpoperation.cpp
        #gdal_nrgcrs.c
        llrasterize.cpp
        polygonize.cpp
        rasterfill.cpp
        thinplatespline.cpp
        gdal_simplesurf.cpp
)

find_package(OpenCL)
if(OPENCL_FOUND)
	include_directories(${GEOS_INCLUDE_DIRS})
	add_definitions(-DHAVE_OPENCL)
	set(GDAL_HAVE_OPENCL ON PARENT_SCOPE)
	
	set(LIB_HHEADERS ${LIB_HHEADERS}
		gdalwarpkernel_opencl.h
	)
	
	set(LIB_CSOURCES
		gdalwarpkernel_opencl.cpp
	)
endif(OPENCL_FOUND)

option(GDAL_QHULL_INTERNAL "use internal qhull libs" ON)
if(GDAL_QHULL_INTERNAL)
    add_definitions(-DINTERNAL_QHULL)
    include_directories(internal_libqhull)
    set(LIB_HHEADERS ${LIB_HHEADERS}
            internal_qhull_headers.h
       )
else()
    add_definitions(-DEXTERNAL_QHULL)
    add_definitions(-DQHULL_INCLUDE_SUBDIR_IS_LIBQHULL=${QHULL_INCLUDE_SUBDIR_IS_LIBQHULL})
endif()

set(GDAL_TARGET_OBJECTS ${GDAL_TARGET_OBJECTS} $<TARGET_OBJECTS:${LIB_NAME}> PARENT_SCOPE)
add_library(${LIB_NAME} OBJECT ${LIB_HHEADERS} ${LIB_CSOURCES})

set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB})

if(GEOS_FOUND)
	set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB} ${GEOS_LIBRARY})
endif(GEOS_FOUND)

if(OPENCL_FOUND)
	set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB} ${OPENCL_LIBRARIES})
endif(OPENCL_FOUND)

if(ARMADILLO_FOUND)
	set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB} ${ARMADILLO_LIBRARIES})
endif(ARMADILLO_FOUND)

set(GDAL_TARGET_LINK_LIB ${GDAL_TARGET_LINK_LIB} PARENT_SCOPE)

install(FILES ${LIB_HHEADERS} DESTINATION include/gdal)
