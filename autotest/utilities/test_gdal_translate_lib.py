#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  test librarified gdal_translate
# Author:   Faza Mahamood <fazamhd at gmail dot com>
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
import os

sys.path.append( '../pymod' )

from osgeo import gdal
import gdaltest

###############################################################################
# Simple test

def test_gdal_translate_lib_1():
    
    ds = gdal.Open('../gcore/data/byte.tif')

    ds = gdal.Translate('tmp/test1.tif', ds)
    if ds is None:
        gdaltest.post_reason('got error/warning')
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None
    
    ds = gdal.Open('tmp/test1.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test format option and callback

def mycallback(pct, msg, user_data):
    user_data[0] = pct
    return 1

def test_gdal_translate_lib_2():

    src_ds = gdal.Open('../gcore/data/byte.tif')
    tab = [ 0 ]
    ds = gdal.Translate('tmp/test2.tif', src_ds, format = 'GTiff', callback = mycallback, callback_data = tab)
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    if tab[0] != 1.0:
        gdaltest.post_reason('Bad percentage')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test outputType option

def test_gdal_translate_lib_3():
    
    ds = gdal.Open('../gcore/data/byte.tif')
    ds = gdal.Translate('tmp/test3.tif', ds, outputType = gdal.GDT_Int16)
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).DataType != gdal.GDT_Int16:
        gdaltest.post_reason('Bad data type')
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test bandList option

def test_gdal_translate_lib_4():

    ds = gdal.Open('../gcore/data/rgbsmall.tif')

    ds = gdal.Translate('tmp/test4.tif', ds, bandList = [3,2,1])
    if ds is None:
        gdaltest.post_reason('got error/warning')
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 21349:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    if ds.GetRasterBand(2).Checksum() != 21053:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    if ds.GetRasterBand(3).Checksum() != 21212:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test rgbExpand option

def test_gdal_translate_lib_5():

    ds = gdal.Open('../gdrivers/data/bug407.gif')
    ds = gdal.Translate('tmp/test5.tif', ds, rgbExpand = 'rgb')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).GetRasterColorInterpretation() != gdal.GCI_RedBand:
        gdaltest.post_reason('Bad color interpretation')
        return 'fail'

    if ds.GetRasterBand(2).GetRasterColorInterpretation() != gdal.GCI_GreenBand:
        gdaltest.post_reason('Bad color interpretation')
        return 'fail'

    if ds.GetRasterBand(3).GetRasterColorInterpretation() != gdal.GCI_BlueBand:
        gdaltest.post_reason('Bad color interpretation')
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 20615:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    if ds.GetRasterBand(2).Checksum() != 59147:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    if ds.GetRasterBand(3).Checksum() != 63052:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test oXSizePixel and oYSizePixel option

def test_gdal_translate_lib_6():

    ds = gdal.Open('../gcore/data/byte.tif')
    ds = gdal.Translate('tmp/test6.tif', ds, width = 40, height = 40)
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 18784:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test oXSizePct and oYSizePct option

def test_gdal_translate_lib_7():

    ds = gdal.Open('../gcore/data/byte.tif')
    ds = gdal.Translate('tmp/test7.tif', ds, widthPct = 200.0, heightPct = 200.0)
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 18784:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test outputSRS and GCPs options

def test_gdal_translate_lib_8():

    gcpList = [gdal.GCP(440720.000,3751320.000,0,0,0), gdal.GCP(441920.000,3751320.000,0,20,0), gdal.GCP(441920.000,3750120.000,0,20,20), gdal.GCP(440720.000,3750120.000,0,0,20)]
    ds = gdal.Open('../gcore/data/byte.tif')
    ds = gdal.Translate('tmp/test8.tif', ds, outputSRS = 'EPSG:26711', GCPs = gcpList)
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    gcps = ds.GetGCPs()
    if len(gcps) != 4:
        gdaltest.post_reason( 'GCP count wrong.' )
        return 'fail'

    if ds.GetGCPProjection().find('26711') == -1:
        gdaltest.post_reason( 'Bad GCP projection.' )
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test nodata option

def test_gdal_translate_lib_9():

    ds = gdal.Open('../gcore/data/byte.tif')
    ds = gdal.Translate('tmp/test9.tif', ds, noData = 1)
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).GetNoDataValue() != 1:
        gdaltest.post_reason('Bad nodata value')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test srcWin option

def test_gdal_translate_lib_10():

    ds = gdal.Open('../gcore/data/byte.tif')
    ds = gdal.Translate('tmp/test10.tif', ds, srcWin = [0,0,1,1])
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 2:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test projWin option

def test_gdal_translate_lib_11():

    ds = gdal.Open('../gcore/data/byte.tif')
    ds = gdal.Translate('tmp/test11.tif', ds, projWin = [440720.000, 3751320.000, 441920.000, 3750120.000])
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    if not gdaltest.geotransform_equals(gdal.Open('../gcore/data/byte.tif').GetGeoTransform(), ds.GetGeoTransform(), 1e-9) :
        gdaltest.post_reason('Bad geotransform')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test outputBounds option

def test_gdal_translate_lib_12():

    ds = gdal.Open('../gcore/data/byte.tif')
    ds = gdal.Translate('tmp/test12.tif', ds, outputBounds = [440720.000,3751320.000,441920.000,3750120.000])
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    if not gdaltest.geotransform_equals(gdal.Open('../gcore/data/byte.tif').GetGeoTransform(), ds.GetGeoTransform(), 1e-9) :
        gdaltest.post_reason('Bad geotransform')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test metadataOptions

def test_gdal_translate_lib_13():
    
    ds = gdal.Open('../gcore/data/byte.tif')
    ds = gdal.Translate('tmp/test13.tif', ds, metadataOptions = ['TIFFTAG_DOCUMENTNAME=test13'])
    if ds is None:
        return 'fail'

    md = ds.GetMetadata() 
    if 'TIFFTAG_DOCUMENTNAME' not in md:
        gdaltest.post_reason('Did not get TIFFTAG_DOCUMENTNAME')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test creationOptions

def test_gdal_translate_lib_14():

    ds = gdal.Open('../gcore/data/byte.tif')
    ds = gdal.Translate('tmp/test14.tif', ds, creationOptions = ['COMPRESS=LZW'])
    if ds is None:
        return 'fail'

    md = ds.GetMetadata('IMAGE_STRUCTURE') 
    if 'COMPRESSION' not in md or md['COMPRESSION'] != 'LZW':
        gdaltest.post_reason('Did not get COMPRESSION')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Cleanup

def test_gdal_translate_lib_cleanup():
    for i in range(14):
        try:
            os.remove('tmp/test' + str(i+1) + '.tif')
        except:
            pass
        try:
            os.remove('tmp/test' + str(i+1) + '.tif.aux.xml')
        except:
            pass

    return 'success'

gdaltest_list = [
    test_gdal_translate_lib_1,
    test_gdal_translate_lib_2,
    test_gdal_translate_lib_3,
    test_gdal_translate_lib_4,
    test_gdal_translate_lib_5,
    test_gdal_translate_lib_6,
    test_gdal_translate_lib_7,
    test_gdal_translate_lib_8,
    test_gdal_translate_lib_9,
    test_gdal_translate_lib_10,
    test_gdal_translate_lib_11,
    test_gdal_translate_lib_12,
    test_gdal_translate_lib_13,
    test_gdal_translate_lib_14,
    test_gdal_translate_lib_cleanup
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdal_translate_lib' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
