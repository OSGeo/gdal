#!/usr/bin/env pytest
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

import os


from osgeo import gdal
import gdaltest
import test_cli_utilities
import pytest

###############################################################################
# Simple test


def test_gdal_translate_1():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path() + ' ../gcore/data/byte.tif tmp/test1.tif')
    assert (err is None or err == ''), 'got error/warning'

    ds = gdal.Open('tmp/test1.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    ds = None


###############################################################################
# Test -of option

def test_gdal_translate_2():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -of GTiff ../gcore/data/byte.tif tmp/test2.tif')

    ds = gdal.Open('tmp/test2.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    ds = None


###############################################################################
# Test -ot option

def test_gdal_translate_3():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -ot Int16 ../gcore/data/byte.tif tmp/test3.tif')

    ds = gdal.Open('tmp/test3.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).DataType == gdal.GDT_Int16, 'Bad data type'

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    ds = None

###############################################################################
# Test -b option


def test_gdal_translate_4():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -b 3 -b 2 -b 1 ../gcore/data/rgbsmall.tif tmp/test4.tif')

    ds = gdal.Open('tmp/test4.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 21349, 'Bad checksum'

    assert ds.GetRasterBand(2).Checksum() == 21053, 'Bad checksum'

    assert ds.GetRasterBand(3).Checksum() == 21212, 'Bad checksum'

    ds = None

###############################################################################
# Test -expand option


def test_gdal_translate_5():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -expand rgb ../gdrivers/data/bug407.gif tmp/test5.tif')

    ds = gdal.Open('tmp/test5.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).GetRasterColorInterpretation() == gdal.GCI_RedBand, \
        'Bad color interpretation'

    assert ds.GetRasterBand(2).GetRasterColorInterpretation() == gdal.GCI_GreenBand, \
        'Bad color interpretation'

    assert ds.GetRasterBand(3).GetRasterColorInterpretation() == gdal.GCI_BlueBand, \
        'Bad color interpretation'

    assert ds.GetRasterBand(1).Checksum() == 20615, 'Bad checksum'

    assert ds.GetRasterBand(2).Checksum() == 59147, 'Bad checksum'

    assert ds.GetRasterBand(3).Checksum() == 63052, 'Bad checksum'

    ds = None


###############################################################################
# Test -outsize option in absolute mode

def test_gdal_translate_6():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -outsize 40 40 ../gcore/data/byte.tif tmp/test6.tif')

    ds = gdal.Open('tmp/test6.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 18784, 'Bad checksum'

    ds = None

###############################################################################
# Test -outsize option in percentage mode


def test_gdal_translate_7():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -outsize 200% 200% ../gcore/data/byte.tif tmp/test7.tif')

    ds = gdal.Open('tmp/test7.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 18784, 'Bad checksum'

    ds = None

###############################################################################
# Test -a_srs and -gcp options


def test_gdal_translate_8():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -a_srs EPSG:26711 -gcp 0 0  440720.000 3751320.000 -gcp 20 0 441920.000 3751320.000 -gcp 20 20 441920.000 3750120.000 0 -gcp 0 20 440720.000 3750120.000 ../gcore/data/byte.tif tmp/test8.tif')

    ds = gdal.Open('tmp/test8.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    gcps = ds.GetGCPs()
    assert len(gcps) == 4, 'GCP count wrong.'

    assert ds.GetGCPProjection().find('26711') != -1, 'Bad GCP projection.'

    ds = None


###############################################################################
# Test -a_nodata option

def test_gdal_translate_9():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -a_nodata 1 ../gcore/data/byte.tif tmp/test9.tif')

    ds = gdal.Open('tmp/test9.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).GetNoDataValue() == 1, 'Bad nodata value'

    ds = None


###############################################################################
# Test -srcwin option

def test_gdal_translate_10():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -srcwin 0 0 1 1 ../gcore/data/byte.tif tmp/test10.tif')

    ds = gdal.Open('tmp/test10.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 2, 'Bad checksum'

    ds = None

###############################################################################
# Test -projwin option


def test_gdal_translate_11():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -projwin 440720.000 3751320.000 441920.000 3750120.000 ../gcore/data/byte.tif tmp/test11.tif')

    ds = gdal.Open('tmp/test11.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    assert gdaltest.geotransform_equals(gdal.Open('../gcore/data/byte.tif').GetGeoTransform(), ds.GetGeoTransform(), 1e-9), \
        'Bad geotransform'

    ds = None

###############################################################################
# Test -a_ullr option


def test_gdal_translate_12():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -a_ullr 440720.000 3751320.000 441920.000 3750120.000 ../gcore/data/byte.tif tmp/test12.tif')

    ds = gdal.Open('tmp/test12.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    assert gdaltest.geotransform_equals(gdal.Open('../gcore/data/byte.tif').GetGeoTransform(), ds.GetGeoTransform(), 1e-9), \
        'Bad geotransform'

    ds = None

###############################################################################
# Test -mo option


def test_gdal_translate_13():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -mo TIFFTAG_DOCUMENTNAME=test13 ../gcore/data/byte.tif tmp/test13.tif')

    ds = gdal.Open('tmp/test13.tif')
    assert ds is not None

    md = ds.GetMetadata()
    assert 'TIFFTAG_DOCUMENTNAME' in md, 'Did not get TIFFTAG_DOCUMENTNAME'

    ds = None

###############################################################################
# Test -co option


def test_gdal_translate_14():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -co COMPRESS=LZW ../gcore/data/byte.tif tmp/test14.tif')

    ds = gdal.Open('tmp/test14.tif')
    assert ds is not None

    md = ds.GetMetadata('IMAGE_STRUCTURE')
    assert 'COMPRESSION' in md and md['COMPRESSION'] == 'LZW', 'Did not get COMPRESSION'

    ds = None

###############################################################################
# Test -sds option


def test_gdal_translate_15():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -sds ../gdrivers/data/A.TOC tmp/test15.tif')

    ds = gdal.Open('tmp/test15_1.tif')
    assert ds is not None

    ds = None

###############################################################################
# Test -of VRT which is a special case


def test_gdal_translate_16():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -of VRT ../gcore/data/byte.tif tmp/test16.vrt')

    ds = gdal.Open('tmp/test16.vrt')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    ds = None

###############################################################################
# Test -expand option to VRT


def test_gdal_translate_17():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -of VRT -expand rgba ../gdrivers/data/bug407.gif tmp/test17.vrt')

    ds = gdal.Open('tmp/test17.vrt')
    assert ds is not None

    assert ds.GetRasterBand(1).GetRasterColorInterpretation() == gdal.GCI_RedBand, \
        'Bad color interpretation'

    assert ds.GetRasterBand(2).GetRasterColorInterpretation() == gdal.GCI_GreenBand, \
        'Bad color interpretation'

    assert ds.GetRasterBand(3).GetRasterColorInterpretation() == gdal.GCI_BlueBand, \
        'Bad color interpretation'

    assert ds.GetRasterBand(4).GetRasterColorInterpretation() == gdal.GCI_AlphaBand, \
        'Bad color interpretation'

    assert ds.GetRasterBand(1).Checksum() == 20615, 'Bad checksum'

    assert ds.GetRasterBand(2).Checksum() == 59147, 'Bad checksum'

    assert ds.GetRasterBand(3).Checksum() == 63052, 'Bad checksum'

    assert ds.GetRasterBand(4).Checksum() == 63052, 'Bad checksum'

    ds = None


###############################################################################
# Test translation of a VRT made of VRT

def test_gdal_translate_18():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' ../gcore/data/8bit_pal.bmp -of VRT tmp/test18_1.vrt')
    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' tmp/test18_1.vrt -expand rgb -of VRT tmp/test18_2.vrt')
    (_, ret_stderr) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path() + ' tmp/test18_2.vrt tmp/test18_2.tif')

    # Check that all datasets are closed
    assert ret_stderr.find('Open GDAL Datasets') == -1

    ds = gdal.Open('tmp/test18_2.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    ds = None


###############################################################################
# Test -expand rgba on a color indexed dataset with an alpha band

def test_gdal_translate_19():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    ds = gdal.GetDriverByName('GTiff').Create('tmp/test_gdal_translate_19_src.tif', 1, 1, 2)
    ct = gdal.ColorTable()
    ct.SetColorEntry(127, (1, 2, 3, 255))
    ds.GetRasterBand(1).SetRasterColorTable(ct)
    ds.GetRasterBand(1).Fill(127)
    ds.GetRasterBand(2).Fill(250)
    ds = None

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -expand rgba tmp/test_gdal_translate_19_src.tif tmp/test_gdal_translate_19_dst.tif')

    ds = gdal.Open('tmp/test_gdal_translate_19_dst.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 1, 'Bad checksum for band 1'
    assert ds.GetRasterBand(2).Checksum() == 2, 'Bad checksum for band 2'
    assert ds.GetRasterBand(3).Checksum() == 3, 'Bad checksum for band 3'
    assert ds.GetRasterBand(4).Checksum() == 250 % 7, 'Bad checksum for band 4'

    ds = None

###############################################################################
# Test -a_nodata None


def test_gdal_translate_20():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -a_nodata 255 ../gcore/data/byte.tif tmp/test_gdal_translate_20_src.tif')
    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -a_nodata None tmp/test_gdal_translate_20_src.tif tmp/test_gdal_translate_20_dst.tif')

    ds = gdal.Open('tmp/test_gdal_translate_20_dst.tif')
    assert ds is not None

    nodata = ds.GetRasterBand(1).GetNoDataValue()
    assert nodata is None

    ds = None

###############################################################################
# Test that statistics are copied only when appropriate (#3889)
# in that case, they must be copied


def test_gdal_translate_21():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -of HFA ../gcore/data/utmsmall.img tmp/test_gdal_translate_21.img')

    ds = gdal.Open('tmp/test_gdal_translate_21.img')
    md = ds.GetRasterBand(1).GetMetadata()
    ds = None

    assert md['STATISTICS_MINIMUM'] == '8', 'STATISTICS_MINIMUM is wrong.'

    assert md['STATISTICS_HISTOBINVALUES'] == '0|0|0|0|0|0|0|0|8|0|0|0|0|0|0|0|23|0|0|0|0|0|0|0|0|29|0|0|0|0|0|0|0|46|0|0|0|0|0|0|0|69|0|0|0|0|0|0|0|99|0|0|0|0|0|0|0|0|120|0|0|0|0|0|0|0|178|0|0|0|0|0|0|0|193|0|0|0|0|0|0|0|212|0|0|0|0|0|0|0|281|0|0|0|0|0|0|0|0|365|0|0|0|0|0|0|0|460|0|0|0|0|0|0|0|533|0|0|0|0|0|0|0|544|0|0|0|0|0|0|0|0|626|0|0|0|0|0|0|0|653|0|0|0|0|0|0|0|673|0|0|0|0|0|0|0|629|0|0|0|0|0|0|0|0|586|0|0|0|0|0|0|0|541|0|0|0|0|0|0|0|435|0|0|0|0|0|0|0|348|0|0|0|0|0|0|0|341|0|0|0|0|0|0|0|0|284|0|0|0|0|0|0|0|225|0|0|0|0|0|0|0|237|0|0|0|0|0|0|0|172|0|0|0|0|0|0|0|0|159|0|0|0|0|0|0|0|105|0|0|0|0|0|0|0|824|', \
        'STATISTICS_HISTOBINVALUES is wrong.'

###############################################################################
# Test that statistics are copied only when appropriate (#3889)
# in that case, they must *NOT* be copied


def test_gdal_translate_22():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -of HFA -scale 0 255 0 128 ../gcore/data/utmsmall.img tmp/test_gdal_translate_22.img')

    ds = gdal.Open('tmp/test_gdal_translate_22.img')
    md = ds.GetRasterBand(1).GetMetadata()
    ds = None

    assert 'STATISTICS_MINIMUM' not in md, \
        'did not expected a STATISTICS_MINIMUM value.'

    assert 'STATISTICS_HISTOBINVALUES' not in md, \
        'did not expected a STATISTICS_HISTOBINVALUES value.'

###############################################################################
# Test -stats option (#3889)


def test_gdal_translate_23():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -stats ../gcore/data/byte.tif tmp/test_gdal_translate_23.tif')

    ds = gdal.Open('tmp/test_gdal_translate_23.tif')
    md = ds.GetRasterBand(1).GetMetadata()
    ds = None

    assert md['STATISTICS_MINIMUM'] == '74', 'STATISTICS_MINIMUM is wrong.'

    assert not os.path.exists('tmp/test_gdal_translate_23.tif.aux.xml')

    gdal.Unlink('../gcore/data/byte.tif.aux.xml')

###############################################################################
# Test -srcwin option when partially outside


def test_gdal_translate_24():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -q -srcwin -10 -10 40 40 ../gcore/data/byte.tif tmp/test_gdal_translate_24.tif')

    ds = gdal.Open('tmp/test_gdal_translate_24.tif')
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 4620, 'Bad checksum'

    ds = None

###############################################################################
# Test -norat


def test_gdal_translate_25():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -q ../gdrivers/data/int.img tmp/test_gdal_translate_25.tif -norat')

    ds = gdal.Open('tmp/test_gdal_translate_25.tif')
    assert ds.GetRasterBand(1).GetDefaultRAT() is None, 'RAT unexpected'

    ds = None

###############################################################################
# Test -a_nodata and -stats (#5463)


def test_gdal_translate_26():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    f = open('tmp/test_gdal_translate_26.xyz', 'wb')
    f.write("""X Y Z
0 0 -999
1 0 10
0 1 15
1 1 20""".encode('ascii'))
    f.close()
    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -a_nodata -999 -stats tmp/test_gdal_translate_26.xyz tmp/test_gdal_translate_26.tif')

    ds = gdal.Open('tmp/test_gdal_translate_26.tif')
    assert ds.GetRasterBand(1).GetMinimum() == 10
    assert ds.GetRasterBand(1).GetNoDataValue() == -999

    ds = None

###############################################################################
# Test that we don't preserve statistics when we ought not.


def test_gdal_translate_27():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()
    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

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
    assert ds.GetRasterBand(1).GetMetadataItem('STATISTICS_MINIMUM') is not None
    ds = None

    # Translate to an output type that accepts 256 as maximum
    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' tmp/test_gdal_translate_27.asc tmp/test_gdal_translate_27.tif -ot Float64')

    ds = gdal.Open('tmp/test_gdal_translate_27.tif')
    assert ds.GetRasterBand(1).GetMetadataItem('STATISTICS_MINIMUM') is not None
    ds = None

    # Translate to an output type that doesn't accept 256 as maximum
    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' tmp/test_gdal_translate_27.asc tmp/test_gdal_translate_27.tif -ot Byte')

    ds = gdal.Open('tmp/test_gdal_translate_27.tif')
    assert ds.GetRasterBand(1).GetMetadataItem('STATISTICS_MINIMUM') is None
    ds = None

###############################################################################
# Test -oo


def test_gdal_translate_28():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' ../gdrivers/data/float64.asc tmp/test_gdal_translate_28.tif -oo datatype=float64')

    ds = gdal.Open('tmp/test_gdal_translate_28.tif')
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Float64
    ds = None

###############################################################################
# Test -r


def test_gdal_translate_29():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path() + ' ../gcore/data/byte.tif tmp/test_gdal_translate_29.tif -outsize 50% 50% -r cubic')
    assert (err is None or err == ''), 'got error/warning'

    ds = gdal.Open('tmp/test_gdal_translate_29.tif')
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 1059, 'Bad checksum'

    ds = None

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path() + ' ../gcore/data/byte.tif tmp/test_gdal_translate_29.vrt -outsize 50% 50% -r cubic -of VRT')
    assert (err is None or err == ''), 'got error/warning'
    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path() + ' tmp/test_gdal_translate_29.vrt tmp/test_gdal_translate_29.tif')
    assert (err is None or err == ''), 'got error/warning'

    ds = gdal.Open('tmp/test_gdal_translate_29.tif')
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 1059, 'Bad checksum'

    ds = None

###############################################################################
# Test -tr option


def test_gdal_translate_30():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -tr 30 30 ../gcore/data/byte.tif tmp/test_gdal_translate_30.tif')

    ds = gdal.Open('tmp/test_gdal_translate_30.tif')
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 18784, 'Bad checksum'

    ds = None

###############################################################################
# Test -projwin_srs option


def test_gdal_translate_31():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -projwin_srs EPSG:4267 -projwin -117.641168620797 33.9023526904262 -117.628110837847 33.8915970129613 ../gcore/data/byte.tif tmp/test_gdal_translate_31.tif')

    ds = gdal.Open('tmp/test_gdal_translate_31.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    assert gdaltest.geotransform_equals(gdal.Open('../gcore/data/byte.tif').GetGeoTransform(), ds.GetGeoTransform(), 1e-6), \
        'Bad geotransform'

    ds = None

###############################################################################
# Test subsetting a file with a RPC


def test_gdal_translate_32():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' ../gcore/data/byte_rpc.tif tmp/test_gdal_translate_32.tif -srcwin 1 2 13 14 -outsize 150% 300%')
    ds = gdal.Open('tmp/test_gdal_translate_32.tif')
    md = ds.GetMetadata('RPC')
    assert (abs(float(md['LINE_OFF']) - 47496) <= 1e-5 and \
       abs(float(md['LINE_SCALE']) - 47502) <= 1e-5 and \
       abs(float(md['SAMP_OFF']) - 19676.6923076923) <= 1e-5 and \
       abs(float(md['SAMP_SCALE']) - 19678.1538461538) <= 1e-5)

    gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path() + ' ../gcore/data/byte_rpc.tif tmp/test_gdal_translate_32.tif -srcwin -10 -5 20 20')
    ds = gdal.Open('tmp/test_gdal_translate_32.tif')
    md = ds.GetMetadata('RPC')
    assert (abs(float(md['LINE_OFF']) - (15834 - -5)) <= 1e-5 and \
       abs(float(md['LINE_SCALE']) - 15834) <= 1e-5 and \
       abs(float(md['SAMP_OFF']) - (13464 - -10)) <= 1e-5 and \
       abs(float(md['SAMP_SCALE']) - 13464) <= 1e-5)

###############################################################################
# Test -outsize option in auto mode


def test_gdal_translate_33():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -outsize 100 0 ../gdrivers/data/small_world.tif tmp/test_gdal_translate_33.tif')

    ds = gdal.Open('tmp/test_gdal_translate_33.tif')
    assert ds.RasterYSize == 50
    ds = None

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -outsize 0 100 ../gdrivers/data/small_world.tif tmp/test_gdal_translate_33.tif')

    ds = gdal.Open('tmp/test_gdal_translate_33.tif')
    assert ds.RasterXSize == 200, ds.RasterYSize
    ds = None

    os.unlink('tmp/test_gdal_translate_33.tif')

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path() + ' -outsize 0 0 ../gdrivers/data/small_world.tif tmp/test_gdal_translate_33.tif')
    assert '-outsize 0 0 invalid' in err

###############################################################################
# Test NBITS is preserved


def test_gdal_translate_34():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' ../gcore/data/oddsize1bit.tif tmp/test_gdal_translate_34.vrt -of VRT -mo FOO=BAR')

    ds = gdal.Open('tmp/test_gdal_translate_34.vrt')
    assert ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') == '1'
    ds = None

    os.unlink('tmp/test_gdal_translate_34.vrt')

###############################################################################
# Test various errors (missing source or dest...)


def test_gdal_translate_35():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path())
    assert 'No source dataset specified' in err

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path() + ' ../gcore/data/byte.tif')
    assert 'No target dataset specified' in err

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path() + ' /non_existing_path/non_existing.tif /vsimem/out.tif')
    assert 'does not exist in the file system' in err or 'No such file or directory' in err

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path() + ' ../gcore/data/byte.tif /non_existing_path/non_existing.tif')
    assert 'Attempt to create new tiff file' in err

###############################################################################
# Test RAT is copied from hfa to gtiff - continuous/athematic

def test_gdal_translate_36():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -of gtiff data/onepixelcontinuous.img tmp/test_gdal_translate_36.tif')

    ds = gdal.Open('tmp/test_gdal_translate_36.tif')
    assert ds is not None

    rat = ds.GetRasterBand(1).GetDefaultRAT()
    assert rat, 'Did not get RAT'

    assert rat.GetRowCount() == 256, 'RAT has incorrect row count'

    assert rat.GetTableType() == 1, 'RAT not athematic'
    rat = None
    ds = None

###############################################################################
# Test RAT is copied from hfa to gtiff - thematic

def test_gdal_translate_37():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -q -of gtiff data/onepixelthematic.img tmp/test_gdal_translate_37.tif')

    ds = gdal.Open('tmp/test_gdal_translate_37.tif')
    assert ds is not None

    rat = ds.GetRasterBand(1).GetDefaultRAT()
    assert rat, 'Did not get RAT'

    assert rat.GetRowCount() == 256, 'RAT has incorrect row count'

    assert rat.GetTableType() == 0, 'RAT not thematic'
    rat = None
    ds = None

# Test RAT is copied round trip back to hfa

def test_gdal_translate_38():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -q -of hfa tmp/test_gdal_translate_37.tif tmp/test_gdal_translate_38.img')

    ds = gdal.Open('tmp/test_gdal_translate_38.img')
    assert ds is not None

    rat = ds.GetRasterBand(1).GetDefaultRAT()
    assert rat, 'Did not get RAT'

    assert rat.GetRowCount() == 256, 'RAT has incorrect row count'

    assert rat.GetTableType() == 0, 'RAT not thematic'
    rat = None
    ds = None

###############################################################################
# Cleanup


def test_gdal_translate_cleanup():
    for i in range(14):
        try:
            os.remove('tmp/test' + str(i + 1) + '.tif')
        except OSError:
            pass
        try:
            os.remove('tmp/test' + str(i + 1) + '.tif.aux.xml')
        except OSError:
            pass
    try:
        os.remove('tmp/test15_1.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/test16.vrt')
    except OSError:
        pass
    try:
        os.remove('tmp/test17.vrt')
    except OSError:
        pass
    try:
        os.remove('tmp/test18_1.vrt')
    except OSError:
        pass
    try:
        os.remove('tmp/test18_2.vrt')
    except OSError:
        pass
    try:
        os.remove('tmp/test18_2.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/test_gdal_translate_19_src.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/test_gdal_translate_19_dst.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/test_gdal_translate_20_src.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/test_gdal_translate_20_dst.tif')
    except OSError:
        pass
    try:
        gdal.GetDriverByName('HFA').Delete('tmp/test_gdal_translate_21.img')
    except (AttributeError, RuntimeError):
        pass
    try:
        gdal.GetDriverByName('HFA').Delete('tmp/test_gdal_translate_22.img')
    except (AttributeError, RuntimeError):
        pass
    try:
        gdal.GetDriverByName('GTiff').Delete('tmp/test_gdal_translate_23.tif')
    except (AttributeError, RuntimeError):
        pass
    try:
        os.remove('tmp/test_gdal_translate_24.tif')
    except OSError:
        pass
    try:
        gdal.GetDriverByName('GTiff').Delete('tmp/test_gdal_translate_25.tif')
    except (AttributeError, RuntimeError):
        pass
    try:
        gdal.GetDriverByName('XYZ').Delete('tmp/test_gdal_translate_26.xyz')
        gdal.GetDriverByName('GTiff').Delete('tmp/test_gdal_translate_26.tif')
    except (AttributeError, RuntimeError):
        pass
    try:
        gdal.GetDriverByName('AAIGRID').Delete('tmp/test_gdal_translate_27.asc')
        gdal.GetDriverByName('GTiff').Delete('tmp/test_gdal_translate_27.tif')
    except (AttributeError, RuntimeError):
        pass
    try:
        gdal.GetDriverByName('GTiff').Delete('tmp/test_gdal_translate_28.tif')
    except (AttributeError, RuntimeError):
        pass
    try:
        os.remove('tmp/test_gdal_translate_29.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/test_gdal_translate_29.vrt')
    except OSError:
        pass
    try:
        os.remove('tmp/test_gdal_translate_30.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/test_gdal_translate_31.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/test_gdal_translate_32.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/test_gdal_translate_36.tif')
    except:
        pass
    try:
        os.remove('tmp/test_gdal_translate_36.tif.aux.xml')
    except:
        pass
    try:
        os.remove('tmp/test_gdal_translate_37.tif')
    except:
        pass
    try:
        os.remove('tmp/test_gdal_translate_37.tif.aux.xml')
    except:
        pass
    try:
        gdal.GetDriverByName('HFA').Delete('tmp/test_gdal_translate_38.img')
    except:
        pass
    



