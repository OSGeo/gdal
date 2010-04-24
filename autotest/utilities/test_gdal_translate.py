#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_translate testing
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault @ mines-paris dot org>
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

import gdal
import gdaltest
import test_cli_utilities

###############################################################################
# Simple test

def test_gdal_translate_1():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' ../gcore/data/byte.tif tmp/test1.tif')

    ds = gdal.Open('tmp/test1.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'


###############################################################################
# Test -of option

def test_gdal_translate_2():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -of GTiff ../gcore/data/byte.tif tmp/test2.tif')

    ds = gdal.Open('tmp/test2.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'


###############################################################################
# Test -ot option

def test_gdal_translate_3():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -ot Int16 ../gcore/data/byte.tif tmp/test3.tif')

    ds = gdal.Open('tmp/test3.tif')
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
# Test -b option

def test_gdal_translate_4():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -b 3 -b 2 -b 1 ../gcore/data/rgbsmall.tif tmp/test4.tif')

    ds = gdal.Open('tmp/test4.tif')
    if ds is None:
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
# Test -expand option

def test_gdal_translate_5():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -expand rgb ../gdrivers/data/bug407.gif tmp/test5.tif')

    ds = gdal.Open('tmp/test5.tif')
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
# Test -outsize option in absolute mode

def test_gdal_translate_6():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -outsize 40 40 ../gcore/data/byte.tif tmp/test6.tif')

    ds = gdal.Open('tmp/test6.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 18784:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -outsize option in percentage mode

def test_gdal_translate_7():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -outsize 200% 200% ../gcore/data/byte.tif tmp/test7.tif')

    ds = gdal.Open('tmp/test7.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 18784:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -a_srs and -gcp options

def test_gdal_translate_8():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -a_srs EPSG:26711 -gcp 0 0  440720.000 3751320.000 -gcp 20 0 441920.000 3751320.000 -gcp 20 20 441920.000 3750120.000 0 -gcp 0 20 440720.000 3750120.000 ../gcore/data/byte.tif tmp/test8.tif')

    ds = gdal.Open('tmp/test8.tif')
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
# Test -a_nodata option

def test_gdal_translate_9():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -a_nodata 1 ../gcore/data/byte.tif tmp/test9.tif')

    ds = gdal.Open('tmp/test9.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).GetNoDataValue() != 1:
        gdaltest.post_reason('Bad nodata value')
        return 'fail'

    ds = None

    return 'success'


###############################################################################
# Test -srcwin option

def test_gdal_translate_10():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -srcwin 0 0 1 1 ../gcore/data/byte.tif tmp/test10.tif')

    ds = gdal.Open('tmp/test10.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 2:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -projwin option

def test_gdal_translate_11():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -projwin 440720.000 3751320.000 441920.000 3750120.000 ../gcore/data/byte.tif tmp/test11.tif')

    ds = gdal.Open('tmp/test11.tif')
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
# Test -a_ullr option

def test_gdal_translate_12():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -a_ullr 440720.000 3751320.000 441920.000 3750120.000 ../gcore/data/byte.tif tmp/test12.tif')

    ds = gdal.Open('tmp/test12.tif')
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
# Test -mo option

def test_gdal_translate_13():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -mo TIFFTAG_DOCUMENTNAME=test13 ../gcore/data/byte.tif tmp/test13.tif')

    ds = gdal.Open('tmp/test13.tif')
    if ds is None:
        return 'fail'

    md = ds.GetMetadata() 
    if 'TIFFTAG_DOCUMENTNAME' not in md:
        gdaltest.post_reason('Did not get TIFFTAG_DOCUMENTNAME')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -co option

def test_gdal_translate_14():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -co COMPRESS=LZW ../gcore/data/byte.tif tmp/test14.tif')

    ds = gdal.Open('tmp/test14.tif')
    if ds is None:
        return 'fail'

    md = ds.GetMetadata('IMAGE_STRUCTURE') 
    if 'COMPRESSION' not in md or md['COMPRESSION'] != 'LZW':
        gdaltest.post_reason('Did not get COMPRESSION')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -sds option

def test_gdal_translate_15():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -sds ../gdrivers/data/A.TOC tmp/test15.tif')

    ds = gdal.Open('tmp/test15.tif1')
    if ds is None:
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -of VRT which is a special case

def test_gdal_translate_16():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -of VRT ../gcore/data/byte.tif tmp/test16.vrt')

    ds = gdal.Open('tmp/test16.vrt')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -expand option to VRT

def test_gdal_translate_17():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -of VRT -expand rgba ../gdrivers/data/bug407.gif tmp/test17.vrt')

    ds = gdal.Open('tmp/test17.vrt')
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

    if ds.GetRasterBand(4).GetRasterColorInterpretation() != gdal.GCI_AlphaBand:
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

    if ds.GetRasterBand(4).Checksum() != 63052:
        print(ds.GetRasterBand(3).Checksum())
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'


###############################################################################
# Test translation of a VRT made of VRT

def test_gdal_translate_18():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' ../gcore/data/8bit_pal.bmp -of VRT tmp/test18_1.vrt')
    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' tmp/test18_1.vrt -expand rgb -of VRT tmp/test18_2.vrt')
    (ret_stdout, ret_stderr) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path() + ' tmp/test18_2.vrt tmp/test18_2.tif')

    # Check that all datasets are closed
    if ret_stderr.find('Open GDAL Datasets') != -1:
        print(ret)
        return 'fail'

    ds = gdal.Open('tmp/test18_2.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'


###############################################################################
# Test -expand rgba on a color indexed dataset with an alpha band

def test_gdal_translate_19():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    ds = gdaltest.tiff_drv.Create('tmp/test_gdal_translate_19_src.tif',1,1,2)
    ct = gdal.ColorTable() 
    ct.SetColorEntry( 127, (255,255,255,255) )
    ds.GetRasterBand( 1 ).SetRasterColorTable( ct )
    ds.GetRasterBand( 1 ).Fill(127)
    ds.GetRasterBand( 2 ).Fill(255)
    ds = None

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -expand rgba tmp/test_gdal_translate_19_src.tif tmp/test_gdal_translate_19_dst.tif')

    ds = gdal.Open('tmp/test_gdal_translate_19_dst.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 1:
        gdaltest.post_reason('Bad checksum for band 1')
        return 'fail'
    if ds.GetRasterBand(2).Checksum() != 2:
        gdaltest.post_reason('Bad checksum for band 2')
        return 'fail'
    if ds.GetRasterBand(3).Checksum() != 3:
        gdaltest.post_reason('Bad checksum for band 3')
        return 'fail'
    if ds.GetRasterBand(4).Checksum() != 255 % 7:
        gdaltest.post_reason('Bad checksum for band 4')
        return 'fail'

    ds = None

    return 'success'
###############################################################################
# Cleanup

def test_gdal_translate_cleanup():
    for i in range(14):
        try:
            os.remove('tmp/test' + str(i+1) + '.tif')
        except:
            pass
        try:
            os.remove('tmp/test' + str(i+1) + '.tif.aux.xml')
        except:
            pass
    try:
        os.remove('tmp/test15.tif1')
    except:
        pass
    try:
        os.remove('tmp/test16.vrt')
    except:
        pass
    try:
        os.remove('tmp/test17.vrt')
    except:
        pass
    try:
        os.remove('tmp/test18_1.vrt')
    except:
        pass
    try:
        os.remove('tmp/test18_2.vrt')
    except:
        pass
    try:
        os.remove('tmp/test18_2.tif')
    except:
        pass
    try:
        os.remove('tmp/test_gdal_translate_19_src.tif')
    except:
        pass
    try:
        os.remove('tmp/test_gdal_translate_19_dst.tif')
    except:
        pass
    return 'success'

gdaltest_list = [
    test_gdal_translate_1,
    test_gdal_translate_2,
    test_gdal_translate_3,
    test_gdal_translate_4,
    test_gdal_translate_5,
    test_gdal_translate_6,
    test_gdal_translate_7,
    test_gdal_translate_8,
    test_gdal_translate_9,
    test_gdal_translate_10,
    test_gdal_translate_11,
    test_gdal_translate_12,
    test_gdal_translate_13,
    test_gdal_translate_14,
    test_gdal_translate_15,
    test_gdal_translate_16,
    test_gdal_translate_17,
    test_gdal_translate_18,
    test_gdal_translate_cleanup
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdal_translate' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
