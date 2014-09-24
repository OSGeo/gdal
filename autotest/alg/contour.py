#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  ContourGenerate() testing
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at mines-paris dot org>
# 
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################

import sys
import os

sys.path.append( '../pymod' )

from osgeo import gdal
from osgeo import ogr
import gdaltest
import ogrtest
import array

###############################################################################
# Test with -a and -i options

def contour_1():

    try:
        os.remove('tmp/contour.shp')
    except:
        pass
    try:
        os.remove('tmp/contour.dbf')
    except:
        pass
    try:
        os.remove('tmp/contour.shx')
    except:
        pass

    drv = gdal.GetDriverByName('GTiff')
    wkt = 'GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9108\"]],AUTHORITY[\"EPSG\",\"4326\"]]'

    size = 160
    precision = 1. / size

    ds = drv.Create('tmp/gdal_contour.tif', size, size, 1)
    ds.SetProjection( wkt )
    ds.SetGeoTransform( [ 1, precision, 0, 50, 0, -precision ] )

    raw_data = array.array('h',[10 for i in range(int(size/2))]).tostring()
    for i in range(int(size/2)):
        ds.WriteRaster( int(size/4), i+int(size/4), int(size/2), 1, raw_data,
                        buf_type = gdal.GDT_Int16,
                        band_list = [1] )

    raw_data = array.array('h',[20 for i in range(int(size/2))]).tostring()
    for i in range(int(size/4)):
        ds.WriteRaster( int(size/4)+int(size/8), i+int(size/4)+int(size/8), int(size/4), 1, raw_data,
                        buf_type = gdal.GDT_Int16,
                        band_list = [1] )

    raw_data = array.array('h',[25 for i in range(int(size/4))]).tostring()
    for i in range(int(size/8)):
        ds.WriteRaster( int(size/4)+int(size/8)+int(size/16), i+int(size/4)+int(size/8)+int(size/16), int(size/8), 1, raw_data,
                        buf_type = gdal.GDT_Int16,
                        band_list = [1] )


    ogr_ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/contour.shp')
    ogr_lyr = ogr_ds.CreateLayer('contour')
    field_defn = ogr.FieldDefn('ID', ogr.OFTInteger)
    ogr_lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('elev', ogr.OFTReal)
    ogr_lyr.CreateField(field_defn)

    gdal.ContourGenerate(ds.GetRasterBand(1), 10, 0, [], 0, 0, ogr_lyr, 0, 1)

    ds = None

    expected_envelopes = [ [ 1.25, 1.75, 49.25, 49.75 ],
                           [ 1.25+0.125, 1.75-0.125, 49.25+0.125, 49.75-0.125 ] ]
    expected_height = [ 10, 20 ]

    lyr = ogr_ds.ExecuteSQL("select * from contour order by elev asc")

    if lyr.GetFeatureCount() != len(expected_envelopes):
        print('Got %d features. Expected %d' % (lyr.GetFeatureCount(), len(expected_envelopes)))
        return 'fail'

    i = 0
    feat = lyr.GetNextFeature()
    while feat is not None:
        envelope = feat.GetGeometryRef().GetEnvelope()
        if feat.GetField('elev') != expected_height[i]:
            print('Got %f. Expected %f' % (feat.GetField('elev'), expected_height[i]))
            return 'fail'
        for j in range(4):
            if abs(expected_envelopes[i][j] - envelope[j]) > precision/2*1.001:
                print('i=%d, wkt=%s' % (i, feat.GetGeometryRef().ExportToWkt()))
                print(feat.GetGeometryRef().GetEnvelope())
                print(expected_envelopes[i])
                print('%f, %f' % (expected_envelopes[i][j] - envelope[j], precision / 2))
                return 'fail'
        i = i + 1
        feat = lyr.GetNextFeature()

    ogr_ds.ReleaseResultSet(lyr)
    ogr_ds.Destroy()

    return 'success'

###############################################################################
# Test with -fl option and -3d option

def contour_2():

    try:
        os.remove('tmp/contour.shp')
    except:
        pass
    try:
        os.remove('tmp/contour.dbf')
    except:
        pass
    try:
        os.remove('tmp/contour.shx')
    except:
        pass

    ogr_ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/contour.shp')
    ogr_lyr = ogr_ds.CreateLayer('contour', geom_type = ogr.wkbLineString25D)
    field_defn = ogr.FieldDefn('ID', ogr.OFTInteger)
    ogr_lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('elev', ogr.OFTReal)
    ogr_lyr.CreateField(field_defn)

    ds = gdal.Open('tmp/gdal_contour.tif')
    gdal.ContourGenerate(ds.GetRasterBand(1), 0, 0, [10,20,25], 0, 0, ogr_lyr, 0, 1)
    ds = None

    size = 160
    precision = 1. / size

    expected_envelopes = [ [ 1.25, 1.75, 49.25, 49.75 ],
                           [ 1.25+0.125, 1.75-0.125, 49.25+0.125, 49.75-0.125 ],
                           [ 1.25+0.125+0.0625, 1.75-0.125-0.0625, 49.25+0.125+0.0625, 49.75-0.125-0.0625 ] ]
    expected_height = [ 10, 20, 25 ]

    lyr = ogr_ds.ExecuteSQL("select * from contour order by elev asc")

    if lyr.GetFeatureCount() != len(expected_envelopes):
        print('Got %d features. Expected %d' % (lyr.GetFeatureCount(), len(expected_envelopes)))
        return 'fail'

    i = 0
    feat = lyr.GetNextFeature()
    while feat is not None:
        if feat.GetGeometryRef().GetZ(0) != expected_height[i]:
            print('Got %f as z. Expected %f' % (feat.GetGeometryRef().GetZ(0), expected_height[i]))
            return 'fail'
        envelope = feat.GetGeometryRef().GetEnvelope()
        if feat.GetField('elev') != expected_height[i]:
            print('Got %f. Expected %f' % (feat.GetField('elev'), expected_height[i]))
            return 'fail'
        for j in range(4):
            if abs(expected_envelopes[i][j] - envelope[j]) > precision/2*1.001:
                print('i=%d, wkt=%s' % (i, feat.GetGeometryRef().ExportToWkt()))
                print(feat.GetGeometryRef().GetEnvelope())
                print(expected_envelopes[i])
                print('%f, %f' % (expected_envelopes[i][j] - envelope[j], precision / 2))
                return 'fail'
        i = i + 1
        feat = lyr.GetNextFeature()

    ogr_ds.ReleaseResultSet(lyr)
    ogr_ds.Destroy()

    return 'success'

###############################################################################
# Cleanup

def contour_cleanup():

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/contour.shp')
    try:
        os.remove('tmp/gdal_contour.tif')
    except:
        pass

    return 'success'


gdaltest_list = [
    contour_1,
    contour_2,
    contour_cleanup
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'contour' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

