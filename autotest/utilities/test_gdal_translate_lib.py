#!/usr/bin/env pytest
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

import os


from osgeo import gdal
import gdaltest
import pytest

###############################################################################
# Simple test


def test_gdal_translate_lib_1():

    ds = gdal.Open('../gcore/data/byte.tif')

    ds = gdal.Translate('tmp/test1.tif', ds)
    assert ds is not None, 'got error/warning'

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    ds = None

    ds = gdal.Open('tmp/test1.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    ds = None

###############################################################################
# Test format option and callback


def mycallback(pct, msg, user_data):
    # pylint: disable=unused-argument
    user_data[0] = pct
    return 1


def test_gdal_translate_lib_2():

    src_ds = gdal.Open('../gcore/data/byte.tif')
    tab = [0]
    ds = gdal.Translate('tmp/test2.tif', src_ds, format='GTiff', callback=mycallback, callback_data=tab)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    assert tab[0] == 1.0, 'Bad percentage'

    ds = None

###############################################################################
# Test outputType option


def test_gdal_translate_lib_3():

    ds = gdal.Open('../gcore/data/byte.tif')
    ds = gdal.Translate('tmp/test3.tif', ds, outputType=gdal.GDT_Int16)
    assert ds is not None

    assert ds.GetRasterBand(1).DataType == gdal.GDT_Int16, 'Bad data type'

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    ds = None

###############################################################################
# Test bandList option


def test_gdal_translate_lib_4():

    ds = gdal.Open('../gcore/data/rgbsmall.tif')

    ds = gdal.Translate('tmp/test4.tif', ds, bandList=[3, 2, 1])
    assert ds is not None, 'got error/warning'

    assert ds.GetRasterBand(1).Checksum() == 21349, 'Bad checksum'

    assert ds.GetRasterBand(2).Checksum() == 21053, 'Bad checksum'

    assert ds.GetRasterBand(3).Checksum() == 21212, 'Bad checksum'

    ds = None

###############################################################################
# Test rgbExpand option


def test_gdal_translate_lib_5():

    ds = gdal.Open('../gdrivers/data/gif/bug407.gif')
    ds = gdal.Translate('tmp/test5.tif', ds, rgbExpand='rgb')
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
# Test oXSizePixel and oYSizePixel option


def test_gdal_translate_lib_6():

    ds = gdal.Open('../gcore/data/byte.tif')
    ds = gdal.Translate('tmp/test6.tif', ds, width=40, height=40)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 18784, 'Bad checksum'

    ds = None

###############################################################################
# Test oXSizePct and oYSizePct option


def test_gdal_translate_lib_7():

    ds = gdal.Open('../gcore/data/byte.tif')
    ds = gdal.Translate('tmp/test7.tif', ds, widthPct=200.0, heightPct=200.0)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 18784, 'Bad checksum'

    ds = None

###############################################################################
# Test outputSRS and GCPs options


def test_gdal_translate_lib_8():

    gcpList = [gdal.GCP(440720.000, 3751320.000, 0, 0, 0), gdal.GCP(441920.000, 3751320.000, 0, 20, 0), gdal.GCP(441920.000, 3750120.000, 0, 20, 20), gdal.GCP(440720.000, 3750120.000, 0, 0, 20)]
    ds = gdal.Open('../gcore/data/byte.tif')
    ds = gdal.Translate('tmp/test8.tif', ds, outputSRS='EPSG:26711', GCPs=gcpList)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    gcps = ds.GetGCPs()
    assert len(gcps) == 4, 'GCP count wrong.'

    assert ds.GetGCPProjection().find('26711') != -1, 'Bad GCP projection.'

    ds = None

###############################################################################
# Test nodata option


def test_gdal_translate_lib_9():

    ds = gdal.Open('../gcore/data/byte.tif')
    ds = gdal.Translate('tmp/test9.tif', ds, noData=1)
    assert ds is not None

    assert ds.GetRasterBand(1).GetNoDataValue() == 1, 'Bad nodata value'

    ds = None

###############################################################################
# Test srcWin option


def test_gdal_translate_lib_10():

    ds = gdal.Open('../gcore/data/byte.tif')
    ds = gdal.Translate('tmp/test10.tif', ds, srcWin=[0, 0, 1, 1])
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 2, 'Bad checksum'

    ds = None

###############################################################################
# Test projWin option


def test_gdal_translate_lib_11():

    ds = gdal.Open('../gcore/data/byte.tif')
    ds = gdal.Translate('tmp/test11.tif', ds, projWin=[440720.000, 3751320.000, 441920.000, 3750120.000])
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    assert gdaltest.geotransform_equals(gdal.Open('../gcore/data/byte.tif').GetGeoTransform(), ds.GetGeoTransform(), 1e-9), \
        'Bad geotransform'

    ds = None

###############################################################################
# Test outputBounds option


def test_gdal_translate_lib_12():

    ds = gdal.Open('../gcore/data/byte.tif')
    ds = gdal.Translate('tmp/test12.tif', ds, outputBounds=[440720.000, 3751320.000, 441920.000, 3750120.000])
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    assert gdaltest.geotransform_equals(gdal.Open('../gcore/data/byte.tif').GetGeoTransform(), ds.GetGeoTransform(), 1e-9), \
        'Bad geotransform'

    ds = None

###############################################################################
# Test metadataOptions


def test_gdal_translate_lib_13():

    ds = gdal.Open('../gcore/data/byte.tif')
    ds = gdal.Translate('tmp/test13.tif', ds, metadataOptions=['TIFFTAG_DOCUMENTNAME=test13'])
    assert ds is not None

    md = ds.GetMetadata()
    assert 'TIFFTAG_DOCUMENTNAME' in md, 'Did not get TIFFTAG_DOCUMENTNAME'

    ds = None

###############################################################################
# Test creationOptions


def test_gdal_translate_lib_14():

    ds = gdal.Open('../gcore/data/byte.tif')
    ds = gdal.Translate('tmp/test14.tif', ds, creationOptions=['COMPRESS=LZW'])
    assert ds is not None

    md = ds.GetMetadata('IMAGE_STRUCTURE')
    assert 'COMPRESSION' in md and md['COMPRESSION'] == 'LZW', 'Did not get COMPRESSION'

    ds = None

###############################################################################
# Test internal wrappers


def test_gdal_translate_lib_100():

    # No option
    with gdaltest.error_handler():
        gdal.TranslateInternal('', gdal.Open('../gcore/data/byte.tif'), None)

    # Will create an implicit options structure
    with gdaltest.error_handler():
        gdal.TranslateInternal('', gdal.Open('../gcore/data/byte.tif'), None, gdal.TermProgress)

    # Null dest name
    try:
        gdal.TranslateInternal(None, gdal.Open('../gcore/data/byte.tif'), None)
    except:
        pass

    
###############################################################################
# Test behaviour with SIGNEDBYTE


def test_gdal_translate_lib_101():

    ds = gdal.Translate('/vsimem/test_gdal_translate_lib_101.tif', gdal.Open('../gcore/data/byte.tif'), creationOptions=['PIXELTYPE=SIGNEDBYTE'], noData='-128')
    assert ds.GetRasterBand(1).GetMetadataItem('PIXELTYPE', 'IMAGE_STRUCTURE') == 'SIGNEDBYTE', \
        'Did not get SIGNEDBYTE'
    assert ds.GetRasterBand(1).GetNoDataValue() == -128, 'Did not get -128'
    ds2 = gdal.Translate('/vsimem/test_gdal_translate_lib_101_2.tif', ds, noData=-127)
    assert ds2.GetRasterBand(1).GetNoDataValue() == -127, 'Did not get -127'
    ds = None
    ds2 = None
    gdal.Unlink('/vsimem/test_gdal_translate_lib_101.tif')
    gdal.Unlink('/vsimem/test_gdal_translate_lib_101_2.tif')

###############################################################################
# Test -scale


def test_gdal_translate_lib_102():

    ds = gdal.Translate('', gdal.Open('../gcore/data/byte.tif'), format='MEM', scaleParams=[[0, 255, 0, 65535]], outputType=gdal.GDT_UInt16)
    result = ds.GetRasterBand(1).ComputeRasterMinMax(False)
    assert result == (19018.0, 65535.0)

    (approx_min, approx_max) = ds.GetRasterBand(1).ComputeRasterMinMax(True)
    ds2 = gdal.Translate('', ds, format='MEM', scaleParams=[[approx_min, approx_max]], outputType=gdal.GDT_Byte)
    expected_stats = ds2.GetRasterBand(1).ComputeStatistics(False)

    # Implicit source statistics use approximate source min/max
    ds2 = gdal.Translate('', ds, format='MEM', scaleParams=[[]], outputType=gdal.GDT_Byte)
    stats = ds2.GetRasterBand(1).ComputeStatistics(False)
    for i in range(4):
        assert stats[i] == pytest.approx(expected_stats[i], abs=1e-3)

    
###############################################################################
# Test that -projwin with nearest neighbor resampling uses integer source
# pixel boundaries (#6610)


def test_gdal_translate_lib_103():

    ds = gdal.Translate('', '../gcore/data/byte.tif', format='MEM', projWin=[440730, 3751310, 441910, 3750140])
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    assert gdaltest.geotransform_equals(gdal.Open('../gcore/data/byte.tif').GetGeoTransform(), ds.GetGeoTransform(), 1e-9), \
        'Bad geotransform'

###############################################################################
# Test translate with a MEM source to a anonymous VRT


def test_gdal_translate_lib_104():

    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
    src_ds.GetRasterBand(1).Fill(255)
    ds = gdal.Translate('', '../gcore/data/byte.tif', format='VRT', width=1, height=1)
    assert ds.GetRasterBand(1).Checksum() == 3, 'Bad checksum'

###############################################################################
# Test GCPs propagation in "VRT path"


def test_gdal_translate_lib_gcp_vrt_path():

    src_ds = gdal.Open('../gcore/data/gcps.vrt')
    ds = gdal.Translate('', src_ds, format='MEM', metadataOptions=['FOO=BAR'])
    assert len(ds.GetGCPs()) == len(src_ds.GetGCPs())
    for i in range(len(src_ds.GetGCPs())):
        assert ds.GetGCPs()[i].GCPX == src_ds.GetGCPs()[i].GCPX
        assert ds.GetGCPs()[i].GCPY == src_ds.GetGCPs()[i].GCPY
        assert ds.GetGCPs()[i].GCPPixel == src_ds.GetGCPs()[i].GCPPixel
        assert ds.GetGCPs()[i].GCPLine == src_ds.GetGCPs()[i].GCPLine

    
###############################################################################
# Test RPC propagation in "VRT path"


def test_gdal_translate_lib_rcp_vrt_path():

    src_ds = gdal.Open('../gcore/data/rpc.vrt')
    ds = gdal.Translate('', src_ds, format='MEM', metadataOptions=['FOO=BAR'])
    assert ds.GetMetadata('RPC') == src_ds.GetMetadata('RPC')

###############################################################################
# Test GeoLocation propagation in "VRT path"


def test_gdal_translate_lib_geolocation_vrt_path():

    src_ds = gdal.Open('../gcore/data/sstgeo.vrt')
    ds = gdal.Translate('/vsimem/temp.vrt', src_ds, format='VRT', metadataOptions=['FOO=BAR'])
    assert ds.GetMetadata('GEOLOCATION') == src_ds.GetMetadata('GEOLOCATION')
    gdal.Unlink('/vsimem/temp.vrt')

###############################################################################
# Test -colorinterp and -colorinterp_X


def test_gdal_translate_lib_colorinterp():

    src_ds = gdal.Open('../gcore/data/rgbsmall.tif')

    # Less bands specified than available
    ds = gdal.Translate('', src_ds, options='-f MEM -colorinterp blue,gray')
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GrayIndex
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand

    # More bands specified than available and a unknown color interpretation
    with gdaltest.error_handler():
        ds = gdal.Translate('', src_ds, options='-f MEM -colorinterp alpha,red,undefined,foo')
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_AlphaBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_Undefined

    # Test colorinterp_
    ds = gdal.Translate('', src_ds, options='-f MEM -colorinterp_2 alpha')
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_AlphaBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand

    # Test invalid colorinterp_
    with pytest.raises(Exception):
        with gdaltest.error_handler():
            gdal.Translate('', src_ds, options='-f MEM -colorinterp_0 alpha')
            

###############################################################################
# Test nogcp options


def test_gdal_translate_lib_110():

    ds = gdal.Open('../gcore/data/byte_gcp.tif')
    ds = gdal.Translate('tmp/test110.tif', ds, nogcp='True')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    gcps = ds.GetGCPs()
    assert len(gcps) == 0, 'GCP count wrong.'

    ds = None

###############################################################################
# Test gdal_translate foo.tif foo.tif.ovr


def test_gdal_translate_lib_generate_ovr():

    gdal.FileFromMemBuffer('/vsimem/foo.tif',
                           open('../gcore/data/byte.tif', 'rb').read())
    gdal.GetDriverByName('GTiff').Create('/vsimem/foo.tif.ovr', 10, 10)
    ds = gdal.Translate('/vsimem/foo.tif.ovr',
                        '/vsimem/foo.tif',
                        resampleAlg = gdal.GRA_Average,
                        format = 'GTiff', width = 10, height = 10)
    assert ds
    assert ds.GetRasterBand(1).Checksum() == 1152, 'Bad checksum'
    ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/foo.tif')

###############################################################################
# Cleanup


def test_gdal_translate_lib_cleanup():
    for i in range(14):
        try:
            os.remove('tmp/test' + str(i + 1) + '.tif')
        except OSError:
            pass
        try:
            os.remove('tmp/test' + str(i + 1) + '.tif.aux.xml')
        except OSError:
            pass

    



