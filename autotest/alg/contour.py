#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  ContourGenerate() testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
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

import struct
import os


from osgeo import gdal
from osgeo import ogr
import gdaltest
import ogrtest
import pytest

###############################################################################
# Test with -a and -i options


def test_contour_1():

    try:
        os.remove('tmp/contour.shp')
    except OSError:
        pass
    try:
        os.remove('tmp/contour.dbf')
    except OSError:
        pass
    try:
        os.remove('tmp/contour.shx')
    except OSError:
        pass

    drv = gdal.GetDriverByName('GTiff')
    wkt = 'GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9108\"]],AUTHORITY[\"EPSG\",\"4326\"]]'

    size = 160
    precision = 1.0 / size

    ds = drv.Create('tmp/gdal_contour.tif', size, size, 1)
    ds.SetProjection(wkt)
    ds.SetGeoTransform([1, precision, 0, 50, 0, -precision])

    raw_data = struct.pack('h', 10) * int(size / 2)
    for i in range(int(size / 2)):
        ds.WriteRaster(int(size / 4), i + int(size / 4), int(size / 2), 1, raw_data,
                       buf_type=gdal.GDT_Int16,
                       band_list=[1])

    raw_data = struct.pack('h', 20) * int(size / 2)
    for i in range(int(size / 4)):
        ds.WriteRaster(int(size / 4) + int(size / 8), i + int(size / 4) + int(size / 8), int(size / 4), 1, raw_data,
                       buf_type=gdal.GDT_Int16,
                       band_list=[1])

    raw_data = struct.pack('h', 25) * int(size / 4)
    for i in range(int(size / 8)):
        ds.WriteRaster(int(size / 4) + int(size / 8) + int(size / 16), i + int(size / 4) + int(size / 8) + int(size / 16), int(size / 8), 1, raw_data,
                       buf_type=gdal.GDT_Int16,
                       band_list=[1])

    ogr_ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/contour.shp')
    ogr_lyr = ogr_ds.CreateLayer('contour')
    field_defn = ogr.FieldDefn('ID', ogr.OFTInteger)
    ogr_lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('elev', ogr.OFTReal)
    ogr_lyr.CreateField(field_defn)

    gdal.ContourGenerate(ds.GetRasterBand(1), 10, 0, [], 0, 0, ogr_lyr, 0, 1)

    ds = None

    expected_envelopes = [[1.25, 1.75, 49.25, 49.75],
                          [1.25 + 0.125, 1.75 - 0.125, 49.25 + 0.125, 49.75 - 0.125]]
    expected_height = [10, 20]

    lyr = ogr_ds.ExecuteSQL("select * from contour order by elev asc")

    assert lyr.GetFeatureCount() == len(expected_envelopes)

    i = 0
    feat = lyr.GetNextFeature()
    while feat is not None:
        envelope = feat.GetGeometryRef().GetEnvelope()
        assert feat.GetField('elev') == expected_height[i]
        for j in range(4):
            if expected_envelopes[i][j] != pytest.approx(envelope[j], abs=precision / 2 * 1.001):
                print('i=%d, wkt=%s' % (i, feat.GetGeometryRef().ExportToWkt()))
                print(feat.GetGeometryRef().GetEnvelope())
                pytest.fail('%f, %f' % (expected_envelopes[i][j] - envelope[j], precision / 2))
        i = i + 1
        feat = lyr.GetNextFeature()

    ogr_ds.ReleaseResultSet(lyr)
    ogr_ds.Destroy()

###############################################################################
# Test with -fl option and -3d option


def test_contour_2():

    try:
        os.remove('tmp/contour.shp')
    except OSError:
        pass
    try:
        os.remove('tmp/contour.dbf')
    except OSError:
        pass
    try:
        os.remove('tmp/contour.shx')
    except OSError:
        pass

    ogr_ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/contour.shp')
    ogr_lyr = ogr_ds.CreateLayer('contour', geom_type=ogr.wkbLineString25D)
    field_defn = ogr.FieldDefn('ID', ogr.OFTInteger)
    ogr_lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('elev', ogr.OFTReal)
    ogr_lyr.CreateField(field_defn)

    ds = gdal.Open('tmp/gdal_contour.tif')
    gdal.ContourGenerate(ds.GetRasterBand(1), 0, 0, [10, 20, 25], 0, 0, ogr_lyr, 0, 1)
    ds = None

    size = 160
    precision = 1. / size

    expected_envelopes = [[1.25, 1.75, 49.25, 49.75],
                          [1.25 + 0.125, 1.75 - 0.125, 49.25 + 0.125, 49.75 - 0.125],
                          [1.25 + 0.125 + 0.0625, 1.75 - 0.125 - 0.0625, 49.25 + 0.125 + 0.0625, 49.75 - 0.125 - 0.0625]]
    expected_height = [10, 20, 25, 10000]

    lyr = ogr_ds.ExecuteSQL("select * from contour order by elev asc")

    assert lyr.GetFeatureCount() == len(expected_envelopes)

    i = 0
    feat = lyr.GetNextFeature()
    while feat is not None:
        assert feat.GetGeometryRef().GetZ(0) == expected_height[i]
        envelope = feat.GetGeometryRef().GetEnvelope()
        assert feat.GetField('elev') == expected_height[i]
        for j in range(4):
            if expected_envelopes[i][j] != pytest.approx(envelope[j], abs=precision / 2 * 1.001):
                print('i=%d, wkt=%s' % (i, feat.GetGeometryRef().ExportToWkt()))
                print(feat.GetGeometryRef().GetEnvelope())
                pytest.fail('%f, %f' % (expected_envelopes[i][j] - envelope[j], precision / 2))
        i = i + 1
        feat = lyr.GetNextFeature()

    ogr_ds.ReleaseResultSet(lyr)
    ogr_ds.Destroy()

###############################################################################
#


def test_contour_real_world_case():

    ogr_ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    ogr_lyr = ogr_ds.CreateLayer('contour', geom_type=ogr.wkbLineString)
    field_defn = ogr.FieldDefn('ID', ogr.OFTInteger)
    ogr_lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('elev', ogr.OFTReal)
    ogr_lyr.CreateField(field_defn)

    ds = gdal.Open('data/contour_in.tif')
    gdal.ContourGenerate(ds.GetRasterBand(1), 10, 0, [], 0, 0, ogr_lyr, 0, 1)
    ds = None

    ogr_lyr.SetAttributeFilter('elev = 330')
    assert ogr_lyr.GetFeatureCount() == 1
    f = ogr_lyr.GetNextFeature()
    assert ogrtest.check_feature_geometry(f, 'LINESTRING (4.50497512437811 11.5,4.5 11.501996007984,3.5 11.8333333333333,2.5 11.5049751243781,2.490099009901 11.5,2.0 10.5,2.5 10.1666666666667,3.0 9.5,3.5 9.21428571428571,4.49800399201597 8.5,4.5 8.49857346647646,5.5 8.16666666666667,6.5 8.0,7.5 8.0,8.0 7.5,8.5 7.0,9.490099009901 6.5,9.5 6.49667774086379,10.5 6.16666666666667,11.4950248756219 5.5,11.5 5.49833610648919,12.5 5.49667774086379,13.5 5.49800399201597,13.501996007984 5.5,13.5 5.50199600798403,12.501996007984 6.5,12.5 6.50142653352354,11.5 6.509900990099,10.509900990099 7.5,10.5 7.50142653352354,9.5 7.9,8.50332225913621 8.5,8.5 8.50249376558603,7.83333333333333 9.5,7.5 10.0,7.0 10.5,6.5 10.7857142857143,5.5 11.1666666666667,4.50497512437811 11.5)', 0.01) == 0

# Test with -p option (polygonize)

def test_contour_3():

    try:
        os.remove('tmp/contour.shp')
    except OSError:
        pass
    try:
        os.remove('tmp/contour.dbf')
    except OSError:
        pass
    try:
        os.remove('tmp/contour.shx')
    except OSError:
        pass

    ogr_ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/contour.shp')
    ogr_lyr = ogr_ds.CreateLayer('contour', geom_type=ogr.wkbMultiPolygon)
    field_defn = ogr.FieldDefn('ID', ogr.OFTInteger)
    ogr_lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('elevMin', ogr.OFTReal)
    ogr_lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('elevMax', ogr.OFTReal)
    ogr_lyr.CreateField(field_defn)

    ds = gdal.Open('tmp/gdal_contour.tif')
    #gdal.ContourGenerateEx(ds.GetRasterBand(1), 0, 0, 0, [10, 20, 25], 0, 0, ogr_lyr, 0, 1, 1)
    gdal.ContourGenerateEx(ds.GetRasterBand(1), ogr_lyr, options = [ "FIXED_LEVELS=10,20,25",
                                                                     "ID_FIELD=0",
                                                                     "ELEV_FIELD_MIN=1",
                                                                     "ELEV_FIELD_MAX=2",
                                                                     "POLYGONIZE=TRUE" ] )
    ds = None

    size = 160
    precision = 1. / size

    expected_envelopes = [[1.0, 2.0, 49.0, 50.0],
                          [1.25, 1.75, 49.25, 49.75],
                          [1.25 + 0.125, 1.75 - 0.125, 49.25 + 0.125, 49.75 - 0.125],
                          [1.25 + 0.125 + 0.0625, 1.75 - 0.125 - 0.0625, 49.25 + 0.125 + 0.0625, 49.75 - 0.125 - 0.0625]]
    expected_height = [10, 20, 25, 10000]

    lyr = ogr_ds.ExecuteSQL("select * from contour order by elevMin asc")

    assert lyr.GetFeatureCount() == len(expected_envelopes)

    i = 0
    feat = lyr.GetNextFeature()
    while feat is not None:
        if i < 3 and feat.GetField('elevMax') != expected_height[i]:
            pytest.fail('Got %f as z. Expected %f' % (feat.GetField('elevMax'), expected_height[i]))
        elif i > 0 and i < 3 and feat.GetField('elevMin') != expected_height[i-1]:
            pytest.fail('Got %f as z. Expected %f' % (feat.GetField('elevMin'), expected_height[i-1]))

        envelope = feat.GetGeometryRef().GetEnvelope()
        for j in range(4):
            if expected_envelopes[i][j] != pytest.approx(envelope[j], abs=precision / 2 * 1.001):
                print('i=%d, wkt=%s' % (i, feat.GetGeometryRef().ExportToWkt()))
                print(feat.GetGeometryRef().GetEnvelope())
                pytest.fail('%f, %f' % (expected_envelopes[i][j] - envelope[j], precision / 2))
        i = i + 1
        feat = lyr.GetNextFeature()

    ogr_ds.ReleaseResultSet(lyr)
    ogr_ds.Destroy()


# Check behaviour when the nodata value as a double isn't exactly the Float32 pixel value
def test_contour_nodata_precision_issue_float32():

    ogr_ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/vsimem/contour.shp')
    ogr_lyr = ogr_ds.CreateLayer('contour', geom_type=ogr.wkbLineString)
    field_defn = ogr.FieldDefn('ID', ogr.OFTInteger)
    ogr_lyr.CreateField(field_defn)

    ds = gdal.Open('data/nodata_precision_issue_float32.tif')
    gdal.ContourGenerateEx(ds.GetRasterBand(1), ogr_lyr, options = [ "LEVEL_INTERVAL=0.1",
                                                                     "ID_FIELD=0",
                                                                     "NODATA=%.19g" % ds.GetRasterBand(1).GetNoDataValue()] )
    ds = None
    assert ogr_lyr.GetFeatureCount() == 0
    ogr_ds = None
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/contour.shp')



def test_contour_too_many_levels():

    ogr_ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/vsimem/contour.shp')
    ogr_lyr = ogr_ds.CreateLayer('contour', geom_type=ogr.wkbLineString)
    field_defn = ogr.FieldDefn('ID', ogr.OFTInteger)
    ogr_lyr.CreateField(field_defn)

    content1 = """ncols        2
nrows        2
xllcorner    0
yllcorner    0
cellsize     1
 1e30 0
 0 0"""

    content2 = """ncols        2
nrows        2
xllcorner    0
yllcorner    0
cellsize     1
 1e6 0
 0 0"""
    for content in (content1, content2):

        with gdaltest.tempfile('/vsimem/test.asc', content):
            ds = gdal.Open('/vsimem/test.asc')
            with gdaltest.error_handler():
                assert gdal.ContourGenerateEx(ds.GetRasterBand(1), ogr_lyr,
                                                options = [ "LEVEL_INTERVAL=1",
                                                            "ID_FIELD=0"] ) != 0

        with gdaltest.tempfile('/vsimem/test.asc', content):
            ds = gdal.Open('/vsimem/test.asc')
            with gdaltest.error_handler():
                assert gdal.ContourGenerateEx(ds.GetRasterBand(1), ogr_lyr,
                                                options = [ "LEVEL_INTERVAL=1",
                                                            "LEVEL_EXP_BASE=1.0001",
                                                            "ID_FIELD=0"] ) != 0

    ogr_ds = None
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/contour.shp')

###############################################################################


def test_contour_raster_acquisition_error():

    ogr_ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    ogr_lyr = ogr_ds.CreateLayer('contour', geom_type=ogr.wkbLineString)
    field_defn = ogr.FieldDefn('ID', ogr.OFTInteger)
    ogr_lyr.CreateField(field_defn)
    ds = gdal.Open('../gcore/data/byte_truncated.tif')

    with gdaltest.error_handler():
        assert gdal.ContourGenerateEx(ds.GetRasterBand(1), ogr_lyr,
                                        options = [ "LEVEL_INTERVAL=1",
                                                    "ID_FIELD=0"] ) != 0

###############################################################################
# Cleanup


def test_contour_cleanup():
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/contour.shp')
    try:
        os.remove('tmp/gdal_contour.tif')
    except OSError:
        pass

    



