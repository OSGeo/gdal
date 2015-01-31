#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test PCIDSK driver functionality.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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

import gdaltest
import ogrtest
from osgeo import ogr
from osgeo import osr
from osgeo import gdal


wkts = [ ('POINT (0 1 2)', 'points', 0),
         ('LINESTRING (0 1 2,3 4 5)', 'lines', 0),
         ('POINT (0 1 2)', 'points2', 4326),
         ('LINESTRING (0 1 2,3 4 5)', 'lines2', 32631),
        ]

###############################################################################
# Test creation

def ogr_pcidsk_1():

    ogr_drv = ogr.GetDriverByName('PCIDSK')
    if ogr_drv is None:
        return 'skip'

    ds = ogr_drv.CreateDataSource('tmp/ogr_pcidsk_1.pix')

    lyr = ds.CreateLayer('nothing', geom_type = ogr.wkbNone)
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('failure')
        return 'fail'

    lyr = ds.CreateLayer('fields', geom_type = ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn('strfield', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('intfield', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('realfield', ogr.OFTReal))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'foo2')
    feat.SetField(1, 1)
    feat.SetField(2, 3.45)
    lyr.CreateFeature(feat)

    feat.SetField(0, 'foo')
    lyr.SetFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'bar')
    lyr.CreateFeature(feat)

    if lyr.GetFeatureCount() != 2:
        gdaltest.post_reason('failure')
        return 'fail'

    lyr.DeleteFeature(1)

    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('failure')
        return 'fail'

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('failure')
        return 'fail'
    if feat.GetField(0) != 'foo':
        gdaltest.post_reason('failure')
        return 'fail'
    if feat.GetField(1) != 1:
        gdaltest.post_reason('failure')
        return 'fail'
    if feat.GetField(2) != 3.45:
        gdaltest.post_reason('failure')
        return 'fail'

    for (wkt, layername, epsgcode) in wkts:
        geom = ogr.CreateGeometryFromWkt(wkt)
        if epsgcode != 0:
            srs = osr.SpatialReference()
            srs.ImportFromEPSG(epsgcode)
        else:
            srs = None
        lyr = ds.CreateLayer(layername, geom_type = geom.GetGeometryType(), srs = srs)
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeometry(geom)
        lyr.CreateFeature(feat)

        lyr.ResetReading()
        feat = lyr.GetNextFeature()
        if feat is None:
            gdaltest.post_reason('failure')
            print(layername)
            return 'fail'
        if feat.GetGeometryRef().ExportToWkt() != wkt:
            gdaltest.post_reason('failure')
            feat.DumpReadable()
            print(layername)
            return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test reading

def ogr_pcidsk_2():

    ogr_drv = ogr.GetDriverByName('PCIDSK')
    if ogr_drv is None:
        return 'skip'

    ds = ogr.Open('tmp/ogr_pcidsk_1.pix')
    if ds.GetLayerCount() != 2 + len(wkts):
        return 'fail'

    lyr = ds.GetLayerByName('nothing')
    if lyr.GetGeomType() != ogr.wkbNone:
        gdaltest.post_reason('failure')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('failure')
        return 'fail'

    lyr = ds.GetLayerByName('fields')
    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('failure')
        return 'fail'
    if feat.GetField(0) != 'foo':
        gdaltest.post_reason('failure')
        return 'fail'
    if feat.GetField(1) != 1:
        gdaltest.post_reason('failure')
        return 'fail'
    if feat.GetField(2) != 3.45:
        gdaltest.post_reason('failure')
        return 'fail'

    for (wkt, layername, epsgcode) in wkts:
        geom = ogr.CreateGeometryFromWkt(wkt)
        lyr = ds.GetLayerByName(layername)
        if lyr.GetGeomType() != geom.GetGeometryType():
            gdaltest.post_reason('failure')
            print(layername)
            return 'fail'

        srs = lyr.GetSpatialRef()
        if epsgcode != 0:
            ref_srs = osr.SpatialReference()
            ref_srs.ImportFromEPSG(epsgcode)
            if srs is None or ref_srs.IsSame(srs) != 1:
                gdaltest.post_reason('failure')
                print(layername)
                print(ref_srs)
                print(srs)
                return 'fail'

        feat = lyr.GetNextFeature()
        if feat is None:
            gdaltest.post_reason('failure')
            print(layername)
            return 'fail'
        if feat.GetGeometryRef().ExportToWkt() != wkt:
            gdaltest.post_reason('failure')
            feat.DumpReadable()
            print(layername)
            return 'fail'

    ds = None

    return 'success'

###############################################################################
# Check with test_ogrsf

def ogr_pcidsk_3():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    if ogr.GetDriverByName('PCIDSK') is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' tmp/ogr_pcidsk_1.pix')

    ret_str = 'success'

    if ret.find("ERROR: The feature was not deleted") != -1:
        # Expected fail for now
        print("ERROR: The feature was not deleted")
        ret = ret.replace("ERROR: The feature was not deleted", "ARGHH: The feature was not deleted")
        ret_str = 'expected_fail'

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        ret_str = 'fail'

    return ret_str

###############################################################################
# Test that we cannot open a raster only pcidsk in read-only mode

def ogr_pcidsk_4():

    if ogr.GetDriverByName('PCIDSK') is None:
        return 'skip'

    if gdal.GetDriverByName('PCIDSK') is None:
        return 'skip'

    ds = ogr.Open('../gdrivers/data/utm.pix')
    if ds is not None:
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test that we can open a raster only pcidsk in update mode

def ogr_pcidsk_5():

    if ogr.GetDriverByName('PCIDSK') is None:
        return 'skip'

    if gdal.GetDriverByName('PCIDSK') is None:
        return 'skip'

    ds = ogr.Open('../gdrivers/data/utm.pix', update = 1)
    if ds is None:
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Check a polygon layer

def ogr_pcidsk_online_1():

    if ogr.GetDriverByName('PCIDSK') is None:
        return 'skip'

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/pcidsk/sdk_testsuite/polygon.pix', 'polygon.pix'):
        return 'skip'

    ds = ogr.Open('tmp/cache/polygon.pix')
    if ds is None:
        gdaltest.post_reason('failure')
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr is None:
        gdaltest.post_reason('failure')
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('failure')
        return 'fail'

    geom = 'POLYGON ((479819.84375 4765180.5 0,479690.1875 4765259.5 0,479647.0 4765369.5 0,479730.375 4765400.5 0,480039.03125 4765539.5 0,480035.34375 4765558.5 0,480159.78125 4765610.5 0,480202.28125 4765482.0 0,480365.0 4765015.5 0,480389.6875 4764950.0 0,480133.96875 4764856.5 0,480080.28125 4764979.5 0,480082.96875 4765049.5 0,480088.8125 4765139.5 0,480059.90625 4765239.5 0,480019.71875 4765319.5 0,479980.21875 4765409.5 0,479909.875 4765370.0 0,479859.875 4765270.0 0,479819.84375 4765180.5 0))'
    if ogrtest.check_feature_geometry(feat, geom) != 0:
        gdaltest.post_reason('failure')
        feat.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Check a polygon layer

def ogr_pcidsk_online_2():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    if ogr.GetDriverByName('PCIDSK') is None:
        return 'skip'

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/pcidsk/sdk_testsuite/polygon.pix', 'polygon.pix'):
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/cache/polygon.pix')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Cleanup

def ogr_pcidsk_cleanup():

    gdal.Unlink('tmp/ogr_pcidsk_1.pix')

    return 'success'

gdaltest_list = [ 
    ogr_pcidsk_1,
    ogr_pcidsk_2,
    ogr_pcidsk_3,
    ogr_pcidsk_4,
    ogr_pcidsk_5,
    ogr_pcidsk_online_1,
    ogr_pcidsk_online_2,
    ogr_pcidsk_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_pcidsk' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

