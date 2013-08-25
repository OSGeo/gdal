#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_contour testing
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2009, Even Rouault <even dot rouault @ mines-paris dot org>
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
import test_cli_utilities
import array

###############################################################################
# Test with -a and -i options

def test_gdal_contour_1():
    if test_cli_utilities.get_gdal_contour_path() is None:
        return 'skip'

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

    ds = None

    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_contour_path() + ' -a elev -i 10 tmp/gdal_contour.tif tmp/contour.shp')
    if not (err is None or err == '') :
        gdaltest.post_reason('got error/warning')
        print(err)
        return 'fail'

    ds = ogr.Open('tmp/contour.shp')

    expected_envelopes = [ [ 1.25, 1.75, 49.25, 49.75 ],
                           [ 1.25+0.125, 1.75-0.125, 49.25+0.125, 49.75-0.125 ] ]
    expected_height = [ 10, 20 ]

    lyr = ds.ExecuteSQL("select * from contour order by elev asc")

    if lyr.GetSpatialRef().ExportToWkt().find('GCS_WGS_1984') == -1:
        print('Did not get expected spatial ref')
        return 'fail'

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

    ds.ReleaseResultSet(lyr)
    ds.Destroy()

    return 'success'

###############################################################################
# Test with -fl option and -3d option

def test_gdal_contour_2():
    if test_cli_utilities.get_gdal_contour_path() is None:
        return 'skip'

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

    # put -3d just after -fl to test #2793
    gdaltest.runexternal(test_cli_utilities.get_gdal_contour_path() + ' -a elev -fl 10 20 25 -3d tmp/gdal_contour.tif tmp/contour.shp')

    size = 160
    precision = 1. / size

    ds = ogr.Open('tmp/contour.shp')

    expected_envelopes = [ [ 1.25, 1.75, 49.25, 49.75 ],
                           [ 1.25+0.125, 1.75-0.125, 49.25+0.125, 49.75-0.125 ],
                           [ 1.25+0.125+0.0625, 1.75-0.125-0.0625, 49.25+0.125+0.0625, 49.75-0.125-0.0625 ] ]
    expected_height = [ 10, 20, 25 ]

    lyr = ds.ExecuteSQL("select * from contour order by elev asc")

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

    ds.ReleaseResultSet(lyr)
    ds.Destroy()

    return 'success'

###############################################################################
# Test on a real DEM

def test_gdal_contour_3():
    if test_cli_utilities.get_gdal_contour_path() is None:
        return 'skip'

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

    # put -3d just after -fl to test #2793
    gdaltest.runexternal(test_cli_utilities.get_gdal_contour_path() + ' -a elev -i 50 ../gdrivers/data/n43.dt0 tmp/contour.shp')

    ds = ogr.Open('tmp/contour.shp')

    lyr = ds.ExecuteSQL("select distinct elev from contour order by elev asc")

    expected_heights = [ 100, 150, 200, 250, 300, 350, 400, 450 ]
    if lyr.GetFeatureCount() != len(expected_heights):
        print('Got %d features. Expected %d' % (lyr.GetFeatureCount(), len(expected_heights)))
        return 'fail'

    i = 0
    feat = lyr.GetNextFeature()
    while feat is not None:
        if feat.GetField('elev') != expected_heights[i]:
            return 'fail'
        i = i + 1
        feat = lyr.GetNextFeature()

    ds.ReleaseResultSet(lyr)
    ds.Destroy()

    return 'success'

###############################################################################
# Test contour orientation

def test_gdal_contour_4():
    if test_cli_utilities.get_gdal_contour_path() is None:
        return 'skip'

    try:
        os.remove('tmp/contour_orientation.shp')
    except:
        pass
    try:
        os.remove('tmp/contour_orientation.dbf')
    except:
        pass
    try:
        os.remove('tmp/contour_orientation.shx')
    except:
        pass

    drv = gdal.GetDriverByName('GTiff')
    wkt = 'GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9108\"]],AUTHORITY[\"EPSG\",\"4326\"]]'

    size = 160
    precision = 1. / size

    ds = drv.Create('tmp/gdal_contour_orientation.tif', size, size, 1)
    ds.SetProjection( wkt )
    ds.SetGeoTransform( [ 1, precision, 0, 50, 0, -precision ] )

# Make the elevation 15 for the whole image
    raw_data = array.array('h',[15 for i in range(int(size))]).tostring()
    for i in range(int(size)):
        ds.WriteRaster( 0, i, int(size), 1, raw_data,
                        buf_type = gdal.GDT_Int16,
                        band_list = [1] )

# Create a hill with elevation 25
    raw_data = array.array('h',[25 for i in range(2)]).tostring()
    for i in range(2):
        ds.WriteRaster( int(size/4)+int(size/8)-1, i+int(size/2)-1, 2, 1, raw_data,
                        buf_type = gdal.GDT_Int16,
                        band_list = [1] )

# Create a depression with elevation 5
    raw_data = array.array('h',[5 for i in range(2)]).tostring()
    for i in range(2):
        ds.WriteRaster( int(size/2)+int(size/8)-1, i+int(size/2)-1, 2, 1, raw_data,
                        buf_type = gdal.GDT_Int16,
                        band_list = [1] )

    ds = None

    gdaltest.runexternal(test_cli_utilities.get_gdal_contour_path() + ' -a elev -i 10 tmp/gdal_contour_orientation.tif tmp/contour_orientation1.shp')

    ds = ogr.Open('tmp/contour_orientation1.shp')

    expected_contours = [ 'LINESTRING (1.621875 49.493749999999999,'+
                                      '1.628125 49.493749999999999,'+
                                      '1.63125 49.496875000000003,'+
                                      '1.63125 49.503124999999997,'+
                                      '1.628125 49.50625,'+
                                      '1.621875 49.50625,'+
                                      '1.61875 49.503124999999997,'+
                                      '1.61875 49.496875000000003,'+
                                      '1.621875 49.493749999999999)',
                          'LINESTRING (1.371875 49.493749999999999,'+
                                      '1.36875 49.496875000000003,'+
                                      '1.36875 49.503124999999997,'+
                                      '1.371875 49.50625,'+
                                      '1.378125 49.50625,'+
                                      '1.38125 49.503124999999997,'+
                                      '1.38125 49.496875000000003,'+
                                      '1.378125 49.493749999999999,'+
                                      '1.371875 49.493749999999999)' ]
    expected_elev = [ 10, 20 ]

    lyr = ds.ExecuteSQL("select * from contour_orientation1 order by elev asc")

    if lyr.GetFeatureCount() != len(expected_contours):
        print('Got %d features. Expected %d' % (lyr.GetFeatureCount(), len(expected_contours)))
        return 'fail'

    i = 0
    test_failed = False
    feat = lyr.GetNextFeature()
    while feat is not None:
        expected_geom = ogr.CreateGeometryFromWkt(expected_contours[i])
        if feat.GetField('elev') != expected_elev[i]:
            print('Got %f. Expected %f' % (feat.GetField('elev'), expected_elev[i]))
            return 'fail'
        if ogrtest.check_feature_geometry(feat, expected_geom) != 0:
            print('Got      %s.\nExpected %s' % (feat.GetGeometryRef().ExportToWkt(),expected_contours[i]))
            test_failed = True
        i = i + 1
        feat = lyr.GetNextFeature()

    ds.ReleaseResultSet(lyr)
    ds.Destroy()

    if test_failed:
        return 'fail'
    else:
        return 'success'

###############################################################################
# Test contour orientation

def test_gdal_contour_5():
    if test_cli_utilities.get_gdal_contour_path() is None:
        return 'skip'

    ds = None

    gdaltest.runexternal(test_cli_utilities.get_gdal_contour_path() + ' -a elev -i 10 data/contour_orientation.tif tmp/contour_orientation2.shp')

    ds = ogr.Open('tmp/contour_orientation2.shp')

    expected_contours = [ 'LINESTRING (0 2,'+
                                      '0.5 2.0,'+
                                      '1.5 2.0,'+
                                      '1.954542932445554 2.5,'+
                                      '2.124997615823304 3.5,'+
                                      '1.5 3.954546085074803,'+
                                      '0.5 4.066665649414062,'+
                                      '0.0 4.066665649414062)' ]
    expected_elev = [ 140 ]

    lyr = ds.ExecuteSQL("select * from contour_orientation2 order by elev asc")

    if lyr.GetFeatureCount() != len(expected_contours):
        print('Got %d features. Expected %d' % (lyr.GetFeatureCount(), len(expected_contours)))
        return 'fail'

    i = 0
    test_failed = False
    feat = lyr.GetNextFeature()
    while feat is not None:
        expected_geom = ogr.CreateGeometryFromWkt(expected_contours[i])
        if feat.GetField('elev') != expected_elev[i]:
            print('Got %f. Expected %f' % (feat.GetField('elev'), expected_elev[i]))
            return 'fail'
        if ogrtest.check_feature_geometry(feat, expected_geom) != 0:
            print('Got      %s.\nExpected %s' % (feat.GetGeometryRef().ExportToWkt(),expected_contours[i]))
            test_failed = True
        i = i + 1
        feat = lyr.GetNextFeature()

    ds.ReleaseResultSet(lyr)
    ds.Destroy()

    if test_failed:
        return 'fail'
    else:
        return 'success'

###############################################################################
# Cleanup

def test_gdal_contour_cleanup():
    if test_cli_utilities.get_gdal_contour_path() is None:
        return 'skip'

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/contour.shp')
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/contour_orientation1.shp')
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/contour_orientation2.shp')
    try:
        os.remove('tmp/gdal_contour.tif')
        os.remove('tmp/gdal_contour_orientation.tif')
    except:
        pass

    return 'success'


gdaltest_list = [
    test_gdal_contour_1,
    test_gdal_contour_2,
    test_gdal_contour_3,
    test_gdal_contour_4,
    test_gdal_contour_5,
    test_gdal_contour_cleanup
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdal_contour' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


