#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_translate testing
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
#
###############################################################################
# Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
import test_cli_utilities

###############################################################################
# Simple test

def test_gdal_translate_1():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path() + ' ../gcore/data/byte.tif tmp/test1.tif')
    if not (err is None or err == '') :
        gdaltest.post_reason('got error/warning')
        print(err)
        return 'fail'

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

    ds = gdal.Open('tmp/test15_1.tif')
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

    ds = gdal.GetDriverByName('GTiff').Create('tmp/test_gdal_translate_19_src.tif',1,1,2)
    ct = gdal.ColorTable() 
    ct.SetColorEntry( 127, (1,2,3,255) )
    ds.GetRasterBand( 1 ).SetRasterColorTable( ct )
    ds.GetRasterBand( 1 ).Fill(127)
    ds.GetRasterBand( 2 ).Fill(250)
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
    if ds.GetRasterBand(4).Checksum() != 250 % 7:
        gdaltest.post_reason('Bad checksum for band 4')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -a_nodata None

def test_gdal_translate_20():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -a_nodata 255 ../gcore/data/byte.tif tmp/test_gdal_translate_20_src.tif')
    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -a_nodata None tmp/test_gdal_translate_20_src.tif tmp/test_gdal_translate_20_dst.tif')

    ds = gdal.Open('tmp/test_gdal_translate_20_dst.tif')
    if ds is None:
        return 'fail'

    nodata = ds.GetRasterBand(1).GetNoDataValue()
    if nodata != None:
        print(nodata)
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test that statistics are copied only when appropriate (#3889)
# in that case, they must be copied

def test_gdal_translate_21():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -of HFA ../gcore/data/utmsmall.img tmp/test_gdal_translate_21.img')

    ds = gdal.Open('tmp/test_gdal_translate_21.img')
    md = ds.GetRasterBand(1).GetMetadata()
    ds = None

    if md['STATISTICS_MINIMUM'] != '8':
        gdaltest.post_reason( 'STATISTICS_MINIMUM is wrong.' )
        print(md['STATISTICS_MINIMUM'])
        return 'fail'

    if md['STATISTICS_HISTOBINVALUES'] != '0|0|0|0|0|0|0|0|8|0|0|0|0|0|0|0|23|0|0|0|0|0|0|0|0|29|0|0|0|0|0|0|0|46|0|0|0|0|0|0|0|69|0|0|0|0|0|0|0|99|0|0|0|0|0|0|0|0|120|0|0|0|0|0|0|0|178|0|0|0|0|0|0|0|193|0|0|0|0|0|0|0|212|0|0|0|0|0|0|0|281|0|0|0|0|0|0|0|0|365|0|0|0|0|0|0|0|460|0|0|0|0|0|0|0|533|0|0|0|0|0|0|0|544|0|0|0|0|0|0|0|0|626|0|0|0|0|0|0|0|653|0|0|0|0|0|0|0|673|0|0|0|0|0|0|0|629|0|0|0|0|0|0|0|0|586|0|0|0|0|0|0|0|541|0|0|0|0|0|0|0|435|0|0|0|0|0|0|0|348|0|0|0|0|0|0|0|341|0|0|0|0|0|0|0|0|284|0|0|0|0|0|0|0|225|0|0|0|0|0|0|0|237|0|0|0|0|0|0|0|172|0|0|0|0|0|0|0|0|159|0|0|0|0|0|0|0|105|0|0|0|0|0|0|0|824|':
        gdaltest.post_reason( 'STATISTICS_HISTOBINVALUES is wrong.' )
        return 'fail'

    return 'success'

###############################################################################
# Test that statistics are copied only when appropriate (#3889)
# in that case, they must *NOT* be copied

def test_gdal_translate_22():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -of HFA -scale 0 255 0 128 ../gcore/data/utmsmall.img tmp/test_gdal_translate_22.img')

    ds = gdal.Open('tmp/test_gdal_translate_22.img')
    md = ds.GetRasterBand(1).GetMetadata()
    ds = None

    if 'STATISTICS_MINIMUM' in md:
        gdaltest.post_reason( 'did not expected a STATISTICS_MINIMUM value.' )
        return 'fail'

    if 'STATISTICS_HISTOBINVALUES' in md:
        gdaltest.post_reason( 'did not expected a STATISTICS_MINIMUM value.' )
        return 'fail'

    return 'success'

###############################################################################
# Test -stats option (#3889)

def test_gdal_translate_23():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -stats ../gcore/data/byte.tif tmp/test_gdal_translate_23.tif')

    ds = gdal.Open('tmp/test_gdal_translate_23.tif')
    md = ds.GetRasterBand(1).GetMetadata()
    ds = None

    if md['STATISTICS_MINIMUM'] != '74':
        gdaltest.post_reason( 'STATISTICS_MINIMUM is wrong.' )
        print(md['STATISTICS_MINIMUM'])
        return 'fail'

    try:
        os.stat('tmp/test_gdal_translate_23.tif.aux.xml')
        gdaltest.post_reason( 'did not expect .aux.xml file presence' )
        return 'fail'
    except:
        pass

    return 'success'



###############################################################################
# Test -srcwin option when partially outside

def test_gdal_translate_24():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -q -srcwin -10 -10 40 40 ../gcore/data/byte.tif tmp/test_gdal_translate_24.tif')

    ds = gdal.Open('tmp/test_gdal_translate_24.tif')
    if ds is None:
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 4620:
        gdaltest.post_reason('Bad checksum')
        print(cs)
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -norat

def test_gdal_translate_25():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -q ../gdrivers/data/int.img tmp/test_gdal_translate_25.tif -norat')

    ds = gdal.Open('tmp/test_gdal_translate_25.tif')
    if ds.GetRasterBand(1).GetDefaultRAT() is not None:
        gdaltest.post_reason('RAT unexpected')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -a_nodata and -stats (#5463)

def test_gdal_translate_26():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    f = open('tmp/test_gdal_translate_26.xyz', 'wb')
    f.write("""X Y Z
0 0 -999
1 0 10
0 1 15
1 1 20""".encode('ascii'))
    f.close()
    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -a_nodata -999 -stats tmp/test_gdal_translate_26.xyz tmp/test_gdal_translate_26.tif')

    ds = gdal.Open('tmp/test_gdal_translate_26.tif')
    if ds.GetRasterBand(1).GetMinimum() != 10:
        gdaltest.post_reason('failure')
        print(ds.GetRasterBand(1).GetMinimum())
        return 'fail'
    if ds.GetRasterBand(1).GetNoDataValue() != -999:
        gdaltest.post_reason('failure')
        print(ds.GetRasterBand(1).GetNoDataValue())
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test that we don't preserve statistics when we ought not.

def test_gdal_translate_27():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'
    if test_cli_utilities.get_gdalinfo_path() is None:
        return 'skip'

    f = open('tmp/test_gdal_translate_27.asc', 'wb')
    f.write("""ncols        2
nrows        2
xllcorner    440720.000000000000
yllcorner    3750120.000000000000
cellsize     60.000000000000
 0 256
 0 0""".encode('ascii'))
    f.close()

    gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -stats tmp/test_gdal_translate_27.asc')

    # Translate to an output type that accepts 256 as maximum
    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' tmp/test_gdal_translate_27.asc tmp/test_gdal_translate_27.tif -ot UInt16')

    ds = gdal.Open('tmp/test_gdal_translate_27.tif')
    if ds.GetRasterBand(1).GetMetadataItem('STATISTICS_MINIMUM') is None:
        gdaltest.post_reason('failure')
        return 'fail'
    ds = None

    # Translate to an output type that accepts 256 as maximum
    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' tmp/test_gdal_translate_27.asc tmp/test_gdal_translate_27.tif -ot Float64')

    ds = gdal.Open('tmp/test_gdal_translate_27.tif')
    if ds.GetRasterBand(1).GetMetadataItem('STATISTICS_MINIMUM') is None:
        gdaltest.post_reason('failure')
        return 'fail'
    ds = None

    # Translate to an output type that doesn't accept 256 as maximum
    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' tmp/test_gdal_translate_27.asc tmp/test_gdal_translate_27.tif -ot Byte')

    ds = gdal.Open('tmp/test_gdal_translate_27.tif')
    if ds.GetRasterBand(1).GetMetadataItem('STATISTICS_MINIMUM') is not None:
        gdaltest.post_reason('failure')
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test -oo

def test_gdal_translate_28():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' ../gdrivers/data/float64.asc tmp/test_gdal_translate_28.tif -oo datatype=float64')

    ds = gdal.Open('tmp/test_gdal_translate_28.tif')
    if ds.GetRasterBand(1).DataType != gdal.GDT_Float64:
        gdaltest.post_reason('failure')
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test -r

def test_gdal_translate_29():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path() + ' ../gcore/data/byte.tif tmp/test_gdal_translate_29.tif -outsize 50% 50% -r cubic')
    if not (err is None or err == '') :
        gdaltest.post_reason('got error/warning')
        print(err)
        return 'fail'

    ds = gdal.Open('tmp/test_gdal_translate_29.tif')
    if ds is None:
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 1059:
        gdaltest.post_reason('Bad checksum')
        print(cs)
        return 'fail'

    ds = None

    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path() + ' ../gcore/data/byte.tif tmp/test_gdal_translate_29.vrt -outsize 50% 50% -r cubic -of VRT')
    if not (err is None or err == '') :
        gdaltest.post_reason('got error/warning')
        print(err)
        return 'fail'
    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path() + ' tmp/test_gdal_translate_29.vrt tmp/test_gdal_translate_29.tif')
    if not (err is None or err == '') :
        gdaltest.post_reason('got error/warning')
        print(err)
        return 'fail'

    ds = gdal.Open('tmp/test_gdal_translate_29.tif')
    if ds is None:
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 1059:
        gdaltest.post_reason('Bad checksum')
        print(cs)
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -tr option

def test_gdal_translate_30():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -tr 30 30 ../gcore/data/byte.tif tmp/test_gdal_translate_30.tif')

    ds = gdal.Open('tmp/test_gdal_translate_30.tif')
    if ds is None:
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 18784:
        print(cs)
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -projwin_srs option

def test_gdal_translate_31():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -projwin_srs EPSG:4267 -projwin -117.641168620797 33.9023526904262 -117.628110837847 33.8915970129613 ../gcore/data/byte.tif tmp/test_gdal_translate_31.tif')

    ds = gdal.Open('tmp/test_gdal_translate_31.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    if not gdaltest.geotransform_equals(gdal.Open('../gcore/data/byte.tif').GetGeoTransform(), ds.GetGeoTransform(), 1e-7) :
        gdaltest.post_reason('Bad geotransform')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test subsetting a file with a RPC

def test_gdal_translate_32():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' ../gcore/data/byte_rpc.tif tmp/test_gdal_translate_32.tif -srcwin 1 2 13 14 -outsize 150% 300%')
    ds = gdal.Open('tmp/test_gdal_translate_32.tif')
    md = ds.GetMetadata('RPC')
    if abs(float(md['LINE_OFF']) - 47496) > 1e-5 or \
       abs(float(md['LINE_SCALE']) - 47502) > 1e-5 or \
       abs(float(md['SAMP_OFF']) - 19676.6923076923) > 1e-5 or \
       abs(float(md['SAMP_SCALE']) - 19678.1538461538) > 1e-5:
           gdaltest.post_reason('fail')
           print(md)
           return 'fail'

    return 'success'

###############################################################################
# Test -outsize option in auto mode

def test_gdal_translate_33():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -outsize 100 0 ../gdrivers/data/small_world.tif tmp/test_gdal_translate_33.tif')

    ds = gdal.Open('tmp/test_gdal_translate_33.tif')
    if ds.RasterYSize != 50:
        gdaltest.post_reason('fail')
        print(ds.RasterYSize)
        return 'fail'
    ds = None

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -outsize 0 100 ../gdrivers/data/small_world.tif tmp/test_gdal_translate_33.tif')

    ds = gdal.Open('tmp/test_gdal_translate_33.tif')
    if ds.RasterXSize != 200:
        gdaltest.post_reason('fail')
        print(ds.RasterYSize)
        return 'fail'
    ds = None

    os.unlink('tmp/test_gdal_translate_33.tif')

    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path() + ' -outsize 0 0 ../gdrivers/data/small_world.tif tmp/test_gdal_translate_33.tif')
    if err.find('-outsize 0 0 invalid') < 0:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test NBITS is preserved

def test_gdal_translate_34():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' ../gcore/data/oddsize1bit.tif tmp/test_gdal_translate_34.vrt -of VRT -mo FOO=BAR')

    ds = gdal.Open('tmp/test_gdal_translate_34.vrt')
    if ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') != '1':
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    os.unlink('tmp/test_gdal_translate_34.vrt')

    return 'success'

###############################################################################
# Test various errors (missing source or dest...)

def test_gdal_translate_35():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path())
    if err.find('No source dataset specified') < 0:
        gdaltest.post_reason('fail')
        print(err)
        return 'fail'

    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path() + ' ../gcore/data/byte.tif')
    if err.find('No target dataset specified') < 0:
        gdaltest.post_reason('fail')
        print(err)
        return 'fail'

    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path() + ' /non_existing_path/non_existing.tif /vsimem/out.tif')
    if err.find('does not exist in the file system') < 0:
        gdaltest.post_reason('fail')
        print(err)
        return 'fail'

    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path() + ' ../gcore/data/byte.tif /non_existing_path/non_existing.tif')
    if err.find('Attempt to create new tiff file') < 0:
        gdaltest.post_reason('fail')
        print(err)
        return 'fail'

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
        os.remove('tmp/test15_1.tif')
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
    try:
        os.remove('tmp/test_gdal_translate_20_src.tif')
    except:
        pass
    try:
        os.remove('tmp/test_gdal_translate_20_dst.tif')
    except:
        pass
    try:
        gdal.GetDriverByName('HFA').Delete('tmp/test_gdal_translate_21.img')
    except:
        pass
    try:
        gdal.GetDriverByName('HFA').Delete('tmp/test_gdal_translate_22.img')
    except:
        pass
    try:
        gdal.GetDriverByName('GTiff').Delete('tmp/test_gdal_translate_23.tif')
    except:
        pass
    try:
        os.remove('tmp/test_gdal_translate_24.tif')
    except:
        pass
    try:
        gdal.GetDriverByName('GTiff').Delete('tmp/test_gdal_translate_25.tif')
    except:
        pass
    try:
        gdal.GetDriverByName('XYZ').Delete('tmp/test_gdal_translate_26.xyz')
        gdal.GetDriverByName('GTiff').Delete('tmp/test_gdal_translate_26.tif')
    except:
        pass
    try:
        gdal.GetDriverByName('AAIGRID').Delete('tmp/test_gdal_translate_27.asc')
        gdal.GetDriverByName('GTiff').Delete('tmp/test_gdal_translate_27.tif')
    except:
        pass
    try:
        gdal.GetDriverByName('GTiff').Delete('tmp/test_gdal_translate_28.tif')
    except:
        pass
    try:
        os.remove('tmp/test_gdal_translate_29.tif')
    except:
        pass
    try:
        os.remove('tmp/test_gdal_translate_29.vrt')
    except:
        pass
    try:
        os.remove('tmp/test_gdal_translate_30.tif')
    except:
        pass
    try:
        os.remove('tmp/test_gdal_translate_31.tif')
    except:
        pass
    try:
        os.remove('tmp/test_gdal_translate_32.tif')
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
    test_gdal_translate_19,
    test_gdal_translate_20,
    test_gdal_translate_21,
    test_gdal_translate_22,
    test_gdal_translate_23,
    test_gdal_translate_24,
    test_gdal_translate_25,
    test_gdal_translate_26,
    test_gdal_translate_27,
    test_gdal_translate_28,
    test_gdal_translate_29,
    test_gdal_translate_30,
    test_gdal_translate_31,
    test_gdal_translate_32,
    test_gdal_translate_33,
    test_gdal_translate_34,
    test_gdal_translate_35,
    test_gdal_translate_cleanup
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdal_translate' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
