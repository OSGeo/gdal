# ******************************************************************************
# * Project:  CMake4GDAL
# * Purpose:  CMake build scripts
# * Author: Dmitriy Baryshnikov (aka Bishop), polimax@mail.ru
# ******************************************************************************
# * Copyright (C) 2012 Bishop
# * Copyright (C) 2017,2018 Hiroshi Miura *
# * Permission is hereby granted, free of charge, to any person obtaining a
# * copy of this software and associated documentation files (the "Software"),
# * to deal in the Software without restriction, including without limitation
# * the rights to use, copy, modify, merge, publish, distribute, sublicense,
# * and/or sell copies of the Software, and to permit persons to whom the
# * Software is furnished to do so, subject to the following conditions: *
# * The above copyright notice and this permission notice shall be included
# * in all copies or substantial portions of the Software. *
# * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# * DEALINGS IN THE SOFTWARE.
# ******************************************************************************

add_library(
  ogrsf_generic OBJECT
  ogrsfdriverregistrar.cpp
  ogrlayer.cpp
  ogrdatasource.cpp
  ogrsfdriver.cpp
  # handled in parent directory. ogrregisterall.cpp
  ogr_gensql.cpp
  ogr_attrind.cpp
  ogr_miattrind.cpp
  ogrwarpedlayer.cpp
  ogrunionlayer.cpp
  ogrlayerpool.cpp
  ogrlayerdecorator.cpp
  ogreditablelayer.cpp
  ogremulatedtransaction.cpp
  ogrmutexeddatasource.cpp
  ogrmutexedlayer.cpp)
gdal_standard_includes(ogrsf_generic)
add_dependencies(ogrsf_generic generate_gdal_version_h)
target_compile_options(ogrsf_generic PRIVATE ${GDAL_CXX_WARNING_FLAGS} ${WFLAG_OLD_STYLE_CAST} ${WFLAG_EFFCXX})
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:ogrsf_generic>)
set_property(TARGET ogrsf_generic PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})

if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
  set_property(SOURCE ogr_gensql.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endif ()

if (FALSE)
  # the following are not installed by current autoconf builds
  include(TargetPublicHeader)
  target_public_header(
    TARGET
    gdal
    HEADERS
    ogr_gensql.h
    ogreditablelayer.h
    ogremulatedtransaction.h
    ogrmutexeddatasource.h
    ogrmutexedlayer.h
    ogrwarpedlayer.h)
endif ()
