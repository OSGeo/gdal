#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  librarified ogr2ogr testing
# Author:   Faza Mahamood <fazamhd @ gmail dot com>
#
###############################################################################
# Copyright (c) 2015, Faza Mahamood <fazamhd at gmail dot com>
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

sys.path.append( '../pymod' )

from osgeo import gdal, ogr
import gdaltest
import ogrtest

###############################################################################
# Simple test

def test_ogr2ogr_lib_1():

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    ds = gdal.VectorTranslate('',srcDS, format = 'Memory')
    if ds is None or ds.GetLayer(0).GetFeatureCount() != 10:
        return 'fail'

    feat0 = ds.GetLayer(0).GetFeature(0)
    if feat0.GetFieldAsDouble('AREA') != 215229.266:
        print(feat0.GetFieldAsDouble('AREA'))
        gdaltest.post_reason('Did not get expected value for field AREA')
        return 'fail'
    if feat0.GetFieldAsString('PRFEDEA') != '35043411':
        print(feat0.GetFieldAsString('PRFEDEA'))
        gdaltest.post_reason('Did not get expected value for field PRFEDEA')
        return 'fail'

    return 'success'

###############################################################################
# Test SQLStatement

def test_ogr2ogr_lib_2():

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    ds = gdal.VectorTranslate('', srcDS, format = 'Memory', SQLStatement='select * from poly', SQLDialect = 'OGRSQL')
    if ds is None or ds.GetLayer(0).GetFeatureCount() != 10:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test @filename syntax
    gdal.FileFromMemBuffer('/vsimem/sql.txt', 'select * from poly')
    ds = gdal.VectorTranslate('', srcDS, format = 'Memory', SQLStatement='@/vsimem/sql.txt')
    if ds is None or ds.GetLayer(0).GetFeatureCount() != 10:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink('/vsimem/sql.txt')

    # Test @filename syntax with a UTF-8 BOM
    if sys.version_info >= (3,0,0):
        gdal.FileFromMemBuffer('/vsimem/sql.txt', '\xEF\xBB\xBFselect * from poly'.encode('LATIN1'))
    else:
        gdal.FileFromMemBuffer('/vsimem/sql.txt', '\xEF\xBB\xBFselect * from poly')
    ds = gdal.VectorTranslate('', srcDS, format = 'Memory', SQLStatement='@/vsimem/sql.txt')
    if ds is None or ds.GetLayer(0).GetFeatureCount() != 10:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink('/vsimem/sql.txt')

    return 'success'

###############################################################################
# Test WHERE

def test_ogr2ogr_lib_3():

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    ds = gdal.VectorTranslate('', srcDS, format = 'Memory', where='EAS_ID=171')
    if ds is None or ds.GetLayer(0).GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test @filename syntax
    gdal.FileFromMemBuffer('/vsimem/filter.txt', 'EAS_ID=171')
    ds = gdal.VectorTranslate('', srcDS, format = 'Memory', where='@/vsimem/filter.txt')
    if ds is None or ds.GetLayer(0).GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink('/vsimem/filter.txt')

    return 'success'

###############################################################################
# Test accessMode

def test_ogr2ogr_lib_4():

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    ds = gdal.VectorTranslate('/vsimem/poly.shp', srcDS)
    if ds.GetLayer(0).GetFeatureCount() != 10:
        gdaltest.post_reason('wrong feature count')
        print(ds.GetLayer(0).GetFeatureCount())
        return 'fail'
    ds= None

    ds = gdal.VectorTranslate('/vsimem/poly.shp', srcDS, accessMode='append')
    if ds is None:
        gdaltest.post_reason('ds is None')
        return 'fail'
    if ds.GetLayer(0).GetFeatureCount() != 20:
        gdaltest.post_reason('wrong feature count')
        print(ds.GetLayer(0).GetFeatureCount())
        return 'fail'

    ret = gdal.VectorTranslate(ds, srcDS, accessMode='append')
    if ret != 1:
        gdaltest.post_reason('ds is None')
        return 'fail'
    if ds.GetLayer(0).GetFeatureCount() != 30:
        gdaltest.post_reason('wrong feature count')
        print(ds.GetLayer(0).GetFeatureCount())
        return 'fail'

    feat10 = ds.GetLayer(0).GetFeature(10)
    if feat10.GetFieldAsDouble('AREA') != 215229.266:
        print(feat10.GetFieldAsDouble('AREA'))
        gdaltest.post_reason('Did not get expected value for field AREA')
        return 'fail'
    if feat10.GetFieldAsString('PRFEDEA') != '35043411':
        print(feat10.GetFieldAsString('PRFEDEA'))
        gdaltest.post_reason('Did not get expected value for field PRFEDEA')
        return 'fail'

    ds = None
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/poly.shp')

    return 'success'

###############################################################################
# Test dstSRS

def test_ogr2ogr_lib_5():

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    ds = gdal.VectorTranslate('', srcDS, format = 'Memory', dstSRS='EPSG:4326')
    if str(ds.GetLayer(0).GetSpatialRef()).find('1984') == -1:
        return 'fail'

    return 'success'

###############################################################################
# Test selFields

def test_ogr2ogr_lib_6():

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    # Voluntary don't use the exact case of the source field names (#4502)
    ds = gdal.VectorTranslate('', srcDS, format = 'Memory', selectFields=['eas_id','prfedea'])
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetFieldCount() != 2:
        return 'fail'
    feat = lyr.GetNextFeature()
    ret = 'success'
    if feat.GetFieldAsDouble('EAS_ID') != 168:
        gdaltest.post_reason('did not get expected value for EAS_ID')
        print(feat.GetFieldAsDouble('EAS_ID'))
        ret = 'fail'
    elif feat.GetFieldAsString('PRFEDEA') != '35043411':
        gdaltest.post_reason('did not get expected value for PRFEDEA')
        print(feat.GetFieldAsString('PRFEDEA'))
        ret = 'fail'

    return ret

###############################################################################
# Test LCO

def test_ogr2ogr_lib_7():

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    ds = gdal.VectorTranslate('/vsimem/poly.shp', srcDS, layerCreationOptions=['SHPT=POLYGONZ'])
    if ds.GetLayer(0).GetLayerDefn().GetGeomType() != ogr.wkbPolygon25D:
        return 'fail'

    ds = None
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/poly.shp')

    return 'success'

###############################################################################
# Add explicit source layer name

def test_ogr2ogr_lib_8():

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    ds = gdal.VectorTranslate('', srcDS, format = 'Memory', layers=['poly'])
    if ds is None or ds.GetLayer(0).GetFeatureCount() != 10:
        return 'fail'

    # Test also with just a string and not an array
    ds = gdal.VectorTranslate('', srcDS, format = 'Memory', layers='poly')
    if ds is None or ds.GetLayer(0).GetFeatureCount() != 10:
        return 'fail'

    return 'success'

###############################################################################
# Test -segmentize

def test_ogr2ogr_lib_9():

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    ds = gdal.VectorTranslate('', srcDS, format = 'Memory', segmentizeMaxDist=100)
    if ds is None or ds.GetLayer(0).GetFeatureCount() != 10:
        return 'fail'
    feat = ds.GetLayer(0).GetNextFeature()
    if feat.GetGeometryRef().GetGeometryRef(0).GetPointCount() != 36:
        return 'fail'

    return 'success'

###############################################################################
# Test overwrite with a shapefile

def test_ogr2ogr_lib_10():

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    ds = gdal.VectorTranslate('/vsimem/tmp/poly.shp',srcDS)
    if ds is None or ds.GetLayer(0).GetFeatureCount() != 10:
        return 'fail'
    ds= None

    # Overwrite
    ds = gdal.VectorTranslate('/vsimem/tmp',srcDS,accessMode='overwrite')
    if ds is None or ds.GetLayer(0).GetFeatureCount() != 10:
        return 'fail'
    ds= None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/tmp')
    return 'success'

###############################################################################
# Test filter

def test_ogr2ogr_lib_11():

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    ds = gdal.VectorTranslate('',srcDS, format = 'Memory',spatFilter = [479609,4764629,479764,4764817])
    if ogrtest.have_geos():
        if ds is None or ds.GetLayer(0).GetFeatureCount() != 4:
            return 'fail'
    else:
        if ds is None or ds.GetLayer(0).GetFeatureCount() != 5:
            return 'fail'

    return 'success'

###############################################################################
# Test callback

def mycallback(pct, msg, user_data):
    user_data[0] = pct
    return 1

def test_ogr2ogr_lib_12():

    tab = [ 0 ]
    ds = gdal.VectorTranslate('', '../ogr/data/poly.shp', format = 'Memory', callback = mycallback, callback_data = tab)
    if ds is None or ds.GetLayer(0).GetFeatureCount() != 10:
        return 'fail'

    if tab[0] != 1.0:
        gdaltest.post_reason('Bad percentage')
        return 'fail'

    return 'success'

###############################################################################
# Test callback with failure

def mycallback_with_failure(pct, msg, user_data):
    if pct > 0.5:
        return 0
    return 1

def test_ogr2ogr_lib_13():

    with gdaltest.error_handler():
        ds = gdal.VectorTranslate('', '../ogr/data/poly.shp', format = 'Memory', callback = mycallback_with_failure)
    if ds is not None:
        return 'fail'

    return 'success'

###############################################################################
# Test internal wrappers

def test_ogr2ogr_lib_14():

    # Null dest name and no option
    try:
        gdal.wrapper_GDALVectorTranslateDestName(None, gdal.OpenEx('../ogr/data/poly.shp'), None)
    except:
        pass

    return 'success'

###############################################################################
# Test non existing zfield

def test_ogr2ogr_lib_15():

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    with gdaltest.error_handler():
        ds = gdal.VectorTranslate('',srcDS, format = 'Memory', zField = 'foo')
    lyr = ds.GetLayer(0)
    if lyr.GetGeomType() != ogr.wkbPolygon:
        return 'fail'

    return 'success'

###############################################################################
# Test -dim

def test_ogr2ogr_lib_16():

    tests = [ [ 'POINT M (1 2 3)', None, 'POINT M (1 2 3)' ],
              [ 'POINT M (1 2 3)', 'XY', 'POINT (1 2)' ],
              [ 'POINT M (1 2 3)', 'XYZ', 'POINT Z (1 2 0)' ],
              [ 'POINT M (1 2 3)', 'XYM', 'POINT M (1 2 3)' ],
              [ 'POINT M (1 2 3)', 'XYZM', 'POINT ZM (1 2 0 3)' ],
              [ 'POINT M (1 2 3)', 'layer_dim', 'POINT M (1 2 3)' ],
              [ 'POINT ZM (1 2 3 4)', None, 'POINT ZM (1 2 3 4)' ],
              [ 'POINT ZM (1 2 3 4)', 'XY', 'POINT (1 2)' ],
              [ 'POINT ZM (1 2 3 4)', 'XYZ', 'POINT Z (1 2 3)' ],
              [ 'POINT ZM (1 2 3 4)', 'XYM', 'POINT M (1 2 4)' ],
              [ 'POINT ZM (1 2 3 4)', 'XYZM', 'POINT ZM (1 2 3 4)' ],
              [ 'POINT ZM (1 2 3 4)', 'layer_dim', 'POINT ZM (1 2 3 4)' ],
            ]
    for (wkt_before, dim, wkt_after) in tests:
        srcDS = gdal.GetDriverByName('Memory').Create('',0,0,0)
        geom = ogr.CreateGeometryFromWkt(wkt_before)
        lyr = srcDS.CreateLayer('test', geom_type = geom.GetGeometryType())
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(geom)
        lyr.CreateFeature(f)

        ds = gdal.VectorTranslate('',srcDS, format = 'Memory', dim = dim)
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        if f.GetGeometryRef().ExportToIsoWkt() != wkt_after:
            gdaltest.post_reason('failure')
            print(wkt_before)
            print(dim)
            print(wkt_after)
            print(f.GetGeometryRef().ExportToIsoWkt())
            return 'fail'

    return 'success'

###############################################################################
# Test gdal.VectorTranslate(dst_ds, ...) without accessMode specified (#6612)

def test_ogr2ogr_lib_17():

    ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0)
    gdal.VectorTranslate(ds, gdal.OpenEx('../ogr/data/poly.shp'))
    lyr = ds.GetLayer(0)
    if lyr.GetFeatureCount() != 10:
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test -limit

def test_ogr2ogr_lib_18():

    ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0)
    gdal.VectorTranslate(ds, gdal.OpenEx('../ogr/data/poly.shp'), limit = 1)
    lyr = ds.GetLayer(0)
    if lyr.GetFeatureCount() != 1:
        return 'fail'
    ds = None

    return 'success'

gdaltest_list = [
    test_ogr2ogr_lib_1,
    test_ogr2ogr_lib_2,
    test_ogr2ogr_lib_3,
    test_ogr2ogr_lib_4,
    test_ogr2ogr_lib_5,
    test_ogr2ogr_lib_6,
    test_ogr2ogr_lib_7,
    test_ogr2ogr_lib_8,
    test_ogr2ogr_lib_9,
    test_ogr2ogr_lib_10,
    test_ogr2ogr_lib_11,
    test_ogr2ogr_lib_12,
    test_ogr2ogr_lib_13,
    test_ogr2ogr_lib_14,
    test_ogr2ogr_lib_15,
    test_ogr2ogr_lib_16,
    test_ogr2ogr_lib_17,
    test_ogr2ogr_lib_18
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'test_ogr2ogr_lib' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

