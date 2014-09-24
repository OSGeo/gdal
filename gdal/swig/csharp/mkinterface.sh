#!/bin/sh

BINDING='csharp'
SWIG='swig'
SWIGARGS='-Wall -I../include -I../include/csharp -I../..'

$SWIG -$BINDING -namespace OSGeo.GDAL -dllimport gdalconst_wrap -o const/gdalconst_wrap.c ../include/gdalconst.i
$SWIG $SWIGARGS -c++ -$BINDING -namespace OSGeo.GDAL -dllimport gdal_wrap -o gdal/gdal_wrap.cpp ../include/gdal.i
$SWIG $SWIGARGS -c++ -$BINDING -namespace OSGeo.OSR -dllimport osr_wrap -o osr/osr_wrap.cpp ../include/osr.i
$SWIG $SWIGARGS -c++ -$BINDING -namespace OSGeo.OGR -dllimport osr_wrap -o ogr/osr_wrap.cpp ../include/osr.i
$SWIG $SWIGARGS -c++ -$BINDING -namespace OSGeo.OGR -dllimport ogr_wrap -o ogr/ogr_wrap.cpp ../include/ogr.i
