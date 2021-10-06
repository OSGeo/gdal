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
import struct


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

    src_ds = gdal.Open('../gcore/data/byte.tif')

    ds = gdal.Translate('/vsimem/test13.tif', src_ds, metadataOptions=['TIFFTAG_DOCUMENTNAME=test13'])
    assert ds is not None

    md = ds.GetMetadata()
    assert 'TIFFTAG_DOCUMENTNAME' in md, 'Did not get TIFFTAG_DOCUMENTNAME'
    ds = None
    gdal.Unlink('/vsimem/test13.tif')

    ds = gdal.Translate('/vsimem/test13.tif', src_ds, metadataOptions='TIFFTAG_DOCUMENTNAME=test13')
    assert ds is not None

    md = ds.GetMetadata()
    assert 'TIFFTAG_DOCUMENTNAME' in md, 'Did not get TIFFTAG_DOCUMENTNAME'
    ds = None
    gdal.Unlink('/vsimem/test13.tif')

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
        gdal.TranslateInternal('', gdal.Open('../gcore/data/byte.tif'), None, gdal.TermProgress_nocb)

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
# Test noxmp options


def test_gdal_translate_lib_111():

    ds = gdal.Open('../gdrivers/data/gtiff/byte_with_xmp.tif')
    new_ds = gdal.Translate('tmp/test111noxmp.tif', ds, options='-noxmp')
    assert new_ds is not None
    xmp = new_ds.GetMetadata('xml:XMP')
    new_ds = None
    assert xmp is None

    # codepath if some other options are set is different, creating a VRTdataset
    new_ds = gdal.Translate('tmp/test111notcopied.tif', ds, nogcp='True')
    assert new_ds is not None
    new_ds = None
    new_ds = gdal.Open('tmp/test111notcopied.tif')
    xmp = new_ds.GetMetadata('xml:XMP')
    new_ds = None
    assert 'W5M0MpCehiHzreSzNTczkc9d' in xmp[0], 'Wrong output file without XMP'

    # normal codepath calling CreateCopy directly
    new_ds = gdal.Translate('tmp/test111.tif', ds)
    assert new_ds is not None
    new_ds = None
    new_ds = gdal.Open('tmp/test111.tif')
    xmp = new_ds.GetMetadata('xml:XMP')
    new_ds = None
    assert 'W5M0MpCehiHzreSzNTczkc9d' in xmp[0], 'Wrong output file without XMP'

    ds = None


def test_gdal_translate_lib_112():

    ds = gdal.Open('../gdrivers/data/gtiff/byte_with_xmp.tif')
    new_ds = gdal.Translate('tmp/test112noxmp.tif', ds, options='-of COG -noxmp')
    assert new_ds is not None
    xmp = new_ds.GetMetadata('xml:XMP')
    new_ds = None
    assert xmp is None

    new_ds = gdal.Translate('tmp/test112.tif', ds, format='COG')
    assert new_ds is not None
    new_ds = None
    new_ds = gdal.Open('tmp/test112.tif')
    xmp = new_ds.GetMetadata('xml:XMP')
    new_ds = None
    assert 'W5M0MpCehiHzreSzNTczkc9d' in xmp[0], 'Wrong output file without XMP'

    ds = None


###############################################################################
# Test gdal_translate foo.tif foo.tif.ovr


def test_gdal_translate_lib_generate_ovr():

    gdal.FileFromMemBuffer('/vsimem/foo.tif',
                           open('../gcore/data/byte.tif', 'rb').read())
    gdal.GetDriverByName('GTiff').Create('/vsimem/foo.tif.ovr', 10, 10)
    ds = gdal.Translate('/vsimem/foo.tif.ovr',
                        '/vsimem/foo.tif',
                        resampleAlg = gdal.GRIORA_Average,
                        format = 'GTiff', width = 10, height = 10)
    assert ds
    assert ds.GetRasterBand(1).Checksum() == 1152, 'Bad checksum'
    ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/foo.tif')


###############################################################################
# Test gdal_translate -tr with non-nearest resample

def _get_src_ds_test_gdal_translate_lib_tr_non_nearest():

    src_w = 5
    src_h = 3
    src_ds = gdal.GetDriverByName('MEM').Create('', src_w, src_h)
    src_ds.SetGeoTransform([100, 10, 0, 1000, 0, -10])
    src_ds.WriteRaster(0, 0, src_w, src_h,
                       struct.pack('B' * src_w * src_h,
                                   100, 100, 200, 200, 10,
                                   100, 100, 200, 200, 20,
                                   30,  30,  30,  30,  30))
    return src_ds

def test_gdal_translate_lib_tr_non_nearest_case_1():

    ds = gdal.Translate('',
                        _get_src_ds_test_gdal_translate_lib_tr_non_nearest(),
                        resampleAlg = gdal.GRIORA_Average,
                        format = 'MEM', xRes = 20, yRes = 20)
    assert ds.RasterXSize == 3 # case where we round up
    assert ds.RasterYSize == 2
    assert struct.unpack('B' * 6, ds.ReadRaster()) == (100, 200, 15,
                                                       30,  30,  30)

def test_gdal_translate_lib_tr_non_nearest_case_2():

    ds = gdal.Translate('',
                        _get_src_ds_test_gdal_translate_lib_tr_non_nearest(),
                        resampleAlg = gdal.GRIORA_Average,
                        format = 'MEM', xRes = 40, yRes = 20)
    assert ds.RasterXSize == 1 # case where we round down
    assert ds.RasterYSize == 2
    assert struct.unpack('B' * 2, ds.ReadRaster()) == (150, 30)


def test_gdal_translate_lib_tr_non_nearest_case_3():

    ds = gdal.Translate('',
                        _get_src_ds_test_gdal_translate_lib_tr_non_nearest(),
                        resampleAlg = gdal.GRIORA_Average,
                        format = 'MEM', xRes = 25, yRes = 20)
    assert ds.RasterXSize == 2 # case where src_w * src_res / dst_res is integer
    assert ds.RasterYSize == 2
    assert struct.unpack('B' * 4, ds.ReadRaster()) == (120, 126,
                                                       30, 30)


def test_gdal_translate_lib_tr_non_nearest_oversampling():

    ds = gdal.Translate('',
                        _get_src_ds_test_gdal_translate_lib_tr_non_nearest(),
                        resampleAlg = gdal.GRIORA_Bilinear,
                        format = 'MEM', xRes = 4, yRes = 10)
    assert ds.RasterXSize == 13
    assert ds.RasterYSize == 3
    assert 0 not in struct.unpack('B' * 13 * 3, ds.ReadRaster())


def test_gdal_translate_lib_preserve_block_size():

    src_ds = gdal.GetDriverByName('GTiff').Create(
        '/vsimem/tmp.tif', 256, 256, 1, options = ['TILED=YES', 'BLOCKXSIZE=32', 'BLOCKYSIZE=64'])

    # VRT created by CreateCopy() of VRT driver
    ds = gdal.Translate('', src_ds, format = 'VRT')
    assert ds.GetRasterBand(1).GetBlockSize() == [32, 64]

    # VRT created by GDALTranslate()
    ds = gdal.Translate('', src_ds, format = 'VRT', metadataOptions=['FOO=BAR'])
    assert ds.GetRasterBand(1).GetBlockSize() == [32, 64]
    src_ds = None
    gdal.Unlink('/vsimem/tmp.tif')

###############################################################################
# Test parsing all resampling methods

@pytest.mark.parametrize("resampleAlg,resampleAlgStr",
                         [ (gdal.GRIORA_NearestNeighbour, "near"),
                           (gdal.GRIORA_Cubic, "cubic"),
                           (gdal.GRIORA_CubicSpline, "cubicspline"),
                           (gdal.GRIORA_Lanczos, "lanczos"),
                           (gdal.GRIORA_Average, "average"),
                           (gdal.GRIORA_RMS, "rms"),
                           (gdal.GRIORA_Mode, "mode"),
                           (gdal.GRIORA_Gauss, "gauss") ])
def test_gdal_translate_lib_resampling_methods(resampleAlg, resampleAlgStr):

    option_list = gdal.TranslateOptions(resampleAlg=resampleAlg, options='__RETURN_OPTION_LIST__')
    assert option_list == ['-r', resampleAlgStr]
    assert gdal.Translate('', '../gcore/data/byte.tif',
                          format='MEM', width=2, height=2, resampleAlg=resampleAlg) is not None

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

