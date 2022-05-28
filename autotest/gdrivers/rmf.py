#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Raster Matrix Format used in GISes "Panorama"/"Integratsia".
# Author:   Andrey Kiselev <dron@ak4719.spb.edu>
#
###############################################################################
# Copyright (c) 2008, Andrey Kiselev <dron@ak4719.spb.edu>
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


import gdaltest
from osgeo import gdal
from osgeo import osr
import pytest

###############################################################################
# Perform simple read tests.


def test_rmf_1():

    tst = gdaltest.GDALTest('rmf', 'rmf/byte.rsw', 1, 4672)
    return tst.testOpen(check_gt=(440720, 60, 0, 3751320, 0, -60))


def test_rmf_2():

    tst = gdaltest.GDALTest('rmf', 'rmf/byte-lzw.rsw', 1, 40503)
    with gdaltest.error_handler():
        return tst.testOpen()


def test_rmf_3():

    tst = gdaltest.GDALTest('rmf', 'rmf/float64.mtw', 1, 4672)
    with gdaltest.error_handler():
        return tst.testOpen(check_gt=(440720, 60, 0, 3751320, 0, -60))


def test_rmf_4():

    tst = gdaltest.GDALTest('rmf', 'rmf/rgbsmall.rsw', 1, 21212)
    tst.testOpen(check_gt=(-44.840320, 0.003432, 0,
                                 -22.932584, 0, -0.003432))

    tst = gdaltest.GDALTest('rmf', 'rmf/rgbsmall.rsw', 2, 21053)
    tst.testOpen(check_gt=(-44.840320, 0.003432, 0,
                                 -22.932584, 0, -0.003432))

    tst = gdaltest.GDALTest('rmf', 'rmf/rgbsmall.rsw', 3, 21349)
    tst.testOpen(check_gt=(-44.840320, 0.003432, 0,
                                  -22.932584, 0, -0.003432))


def test_rmf_5():

    tst = gdaltest.GDALTest('rmf', 'rmf/rgbsmall-lzw.rsw', 1, 40503)
    with gdaltest.error_handler():
        tst.testOpen()

    
    tst = gdaltest.GDALTest('rmf', 'rmf/rgbsmall-lzw.rsw', 2, 41429)
    with gdaltest.error_handler():
        tst.testOpen()

    
    tst = gdaltest.GDALTest('rmf', 'rmf/rgbsmall-lzw.rsw', 3, 40238)
    with gdaltest.error_handler():
        return tst.testOpen()


def test_rmf_6():

    tst = gdaltest.GDALTest('rmf', 'rmf/big-endian.rsw', 1, 7782)
    with gdaltest.error_handler():
        tst.testOpen()
    
    tst = gdaltest.GDALTest('rmf', 'rmf/big-endian.rsw', 2, 8480)
    with gdaltest.error_handler():
        tst.testOpen()
    
    tst = gdaltest.GDALTest('rmf', 'rmf/big-endian.rsw', 3, 4195)
    with gdaltest.error_handler():
        return tst.testOpen()

###############################################################################
# Create simple copy and check.


def test_rmf_7():

    tst = gdaltest.GDALTest('rmf', 'rmf/byte.rsw', 1, 4672)

    return tst.testCreateCopy(check_srs=1, check_gt=1, vsimem=1)


def test_rmf_8():

    tst = gdaltest.GDALTest('rmf', 'rmf/rgbsmall.rsw', 2, 21053)

    return tst.testCreateCopy(check_srs=1, check_gt=1)

###############################################################################
# Create RMFHUGE=YES


def test_rmf_9():

    tst = gdaltest.GDALTest('rmf', 'rmf/byte.rsw', 1, 4672, options=['RMFHUGE=YES'])

    return tst.testCreateCopy(check_srs=1, check_gt=1, vsimem=1)

###############################################################################
# Compressed DEM


def test_rmf_10():

    tst = gdaltest.GDALTest('rmf', 'rmf/t100.mtw', 1, 6388)

    with gdaltest.error_handler():
        return tst.testOpen()

###############################################################################
# Overviews


def test_rmf_11():

    test_fn = '/vsigzip/data/rmf/overviews.rsw.gz'
    src_ds = gdal.Open(test_fn)

    assert src_ds is not None, 'Failed to open test dataset.'

    band1 = src_ds.GetRasterBand(1)

    assert band1.GetOverviewCount() == 3, 'overviews is missing'

    ovr_n = (0, 1, 2)
    ovr_size = (256, 64, 16)
    ovr_checksum = (32756, 51233, 3192)

    for i in ovr_n:
        ovr_band = band1.GetOverview(i)
        if ovr_band.XSize != ovr_size[i] or ovr_band.YSize != ovr_size[i]:
            msg = 'overview wrong size: overview %d, size = %d * %d,' % \
                  (i, ovr_band.XSize, ovr_band.YSize)
            pytest.fail(msg)

        if ovr_band.Checksum() != ovr_checksum[i]:
            msg = 'overview wrong checksum: overview %d, checksum = %d,' % \
                  (i, ovr_band.Checksum())
            pytest.fail(msg)

    
###############################################################################
# Check file open with cucled header offsets .


def test_rmf_12a():

    tst = gdaltest.GDALTest('rmf', 'rmf/cucled-1.rsw', 1, 4672)
    with gdaltest.error_handler():
        return tst.testOpen(check_gt=(440720, 60, 0, 3751320, 0, -60))

###############################################################################
# Check file open with cucled header offsets .


def test_rmf_12b():

    tst = gdaltest.GDALTest('rmf', 'rmf/cucled-2.rsw', 1, 4672)
    with gdaltest.error_handler():
        return tst.testOpen(check_gt=(440720, 60, 0, 3751320, 0, -60))

###############################################################################
# Check file open with invalid subheader marker.


def test_rmf_12c():

    tst = gdaltest.GDALTest('rmf', 'rmf/invalid-subheader.rsw', 1, 4672)
    with gdaltest.error_handler():
        return tst.testOpen(check_gt=(440720, 60, 0, 3751320, 0, -60))

###############################################################################
# Check file open with corrupted subheader.


def test_rmf_12d():

    tst = gdaltest.GDALTest('rmf', 'rmf/corrupted-subheader.rsw', 1, 4672)
    return tst.testOpen(check_gt=(440720, 60, 0, 3751320, 0, -60))

###############################################################################
# Build overviews and check


def rmf_build_ov(source, testid, options, ov_sizes, crs, reopen=False, pass_count=1):

    rmf_drv = gdal.GetDriverByName('RMF')
    assert rmf_drv is not None, 'RMF driver not found.'

    src_ds = gdal.Open(source, gdal.GA_ReadOnly)

    assert src_ds is not None, 'Failed to open test dataset.'

    test_ds_name = 'tmp/ov-' + testid + '.tst'
    src_ds = rmf_drv.CreateCopy(test_ds_name, src_ds, options=options)
    assert src_ds is not None, 'Failed to create test dataset copy.'

    for _ in range(pass_count):
        if reopen:
            src_ds = None
            src_ds = gdal.Open(test_ds_name, gdal.GA_Update)

            assert src_ds is not None, 'Failed to open test dataset.'

        reopen = True
        err = src_ds.BuildOverviews(overviewlist=[2, 4])
        assert err == 0, 'BuildOverviews reports an error'

        src_ds = None
        src_ds = gdal.Open(test_ds_name, gdal.GA_ReadOnly)

        for iBand in range(src_ds.RasterCount):
            band = src_ds.GetRasterBand(iBand + 1)

            assert band.GetOverviewCount() == 2, 'overviews missing'

            for iOverview in range(band.GetOverviewCount()):
                ovr_band = band.GetOverview(iOverview)
                if ovr_band.XSize != ov_sizes[iOverview][0] or \
                   ovr_band.YSize != ov_sizes[iOverview][1]:
                    msg = 'overview wrong size: band %d, overview %d, size = %d * %d,' % \
                          (iBand, iOverview, ovr_band.XSize, ovr_band.YSize)
                    pytest.fail(msg)

                if ovr_band.Checksum() != crs[iOverview][iBand]:
                    msg = 'overview wrong checksum: band %d, overview %d, checksum = %d,' % \
                          (iBand, iOverview, ovr_band.Checksum())
                    pytest.fail(msg)

    src_ds = None
    os.remove(test_ds_name)

###############################################################################
# Build overviews on newly created RSW file


def test_rmf_13():
    return rmf_build_ov(source='data/rmf/byte.rsw',
                        testid='13',
                        options=['RMFHUGE=NO'],
                        ov_sizes=[[10, 10], [5, 5]],
                        crs=[[1087, 1087, 1087], [328, 328, 328]],
                        reopen=False)

###############################################################################
# Build overviews on newly created huge RSW file


def test_rmf_14():
    return rmf_build_ov(source='data/rmf/byte.rsw',
                        testid='14',
                        options=['RMFHUGE=YES'],
                        ov_sizes=[[10, 10], [5, 5]],
                        crs=[[1087, 1087, 1087], [328, 328, 328]],
                        reopen=False)

###############################################################################
# Build overviews on closed and reopened RSW file


def test_rmf_15():
    return rmf_build_ov(source='data/rmf/byte.rsw',
                        testid='15',
                        options=['RMFHUGE=NO'],
                        ov_sizes=[[10, 10], [5, 5]],
                        crs=[[1087, 1087, 1087], [328, 328, 328]],
                        reopen=True)

###############################################################################
# Build overviews on closed and reopened huge RSW file


def test_rmf_16():
    return rmf_build_ov(source='data/rmf/byte.rsw',
                        testid='16',
                        options=['RMFHUGE=YES'],
                        ov_sizes=[[10, 10], [5, 5]],
                        crs=[[1087, 1087, 1087], [328, 328, 328]],
                        reopen=True)

###############################################################################
# Build overviews on newly created MTW file


def test_rmf_17():
    return rmf_build_ov(source='data/rmf/float64.mtw',
                        testid='17',
                        options=['RMFHUGE=NO', 'MTW=YES'],
                        ov_sizes=[[10, 10], [5, 5]],
                        crs=[[1087, 1087, 1087], [328, 328, 328]],
                        reopen=False)

###############################################################################
# Build overviews on newly created MTW file


def test_rmf_18():
    return rmf_build_ov(source='data/rmf/float64.mtw',
                        testid='18',
                        options=['RMFHUGE=YES', 'MTW=YES'],
                        ov_sizes=[[10, 10], [5, 5]],
                        crs=[[1087], [328]],
                        reopen=False)

###############################################################################
# Build overviews on closed and reopened MTW file


def test_rmf_19():
    return rmf_build_ov(source='data/rmf/float64.mtw',
                        testid='19',
                        options=['RMFHUGE=NO', 'MTW=YES'],
                        ov_sizes=[[10, 10], [5, 5]],
                        crs=[[1087], [328]],
                        reopen=True)

###############################################################################
# Build overviews on closed and reopened huge MTW file


def test_rmf_20():
    return rmf_build_ov(source='data/rmf/float64.mtw',
                        testid='20',
                        options=['RMFHUGE=YES', 'MTW=YES'],
                        ov_sizes=[[10, 10], [5, 5]],
                        crs=[[1087], [328]],
                        reopen=True)

###############################################################################
# Recreate overviews on newly created MTW file


def test_rmf_21():
    return rmf_build_ov(source='data/rmf/float64.mtw',
                        testid='21',
                        options=['RMFHUGE=NO', 'MTW=YES'],
                        ov_sizes=[[10, 10], [5, 5]],
                        crs=[[1087], [328]],
                        reopen=False,
                        pass_count=2)

###############################################################################
# Recreate overviews on newly created huge MTW file


def test_rmf_22():
    return rmf_build_ov(source='data/rmf/float64.mtw',
                        testid='22',
                        options=['RMFHUGE=YES', 'MTW=YES'],
                        ov_sizes=[[10, 10], [5, 5]],
                        crs=[[1087], [328]],
                        reopen=False,
                        pass_count=2)


###############################################################################
# Recreate overviews on closed and reopened MTW file

def test_rmf_23():
    return rmf_build_ov(source='data/rmf/float64.mtw',
                        testid='23',
                        options=['RMFHUGE=NO', 'MTW=YES'],
                        ov_sizes=[[10, 10], [5, 5]],
                        crs=[[1087], [328]],
                        reopen=True,
                        pass_count=2)

###############################################################################
# Recreate overviews on closed and reopened huge MTW file


def test_rmf_24():
    return rmf_build_ov(source='data/rmf/float64.mtw',
                        testid='24',
                        options=['RMFHUGE=YES', 'MTW=YES'],
                        ov_sizes=[[10, 10], [5, 5]],
                        crs=[[1087], [328]],
                        reopen=True,
                        pass_count=2)

###############################################################################
# Nodata write test


def test_rmf_25():
    rmf_drv = gdal.GetDriverByName('RMF')
    assert rmf_drv is not None, 'RMF driver not found.'

    src_ds = gdal.Open('data/rmf/byte.rsw', gdal.GA_ReadOnly)

    assert src_ds is not None, 'Failed to open test dataset.'

    test_ds_name = 'tmp/nodata.rsw'
    test_ds = rmf_drv.CreateCopy(test_ds_name, src_ds)
    assert test_ds is not None, 'Failed to create test dataset copy.'

    test_ds.GetRasterBand(1).SetNoDataValue(33)
    nd = test_ds.GetRasterBand(1).GetNoDataValue()
    assert nd == 33, 'Invalid NoData value after CreateCopy.'
    test_ds = None

    test_ds = gdal.Open(test_ds_name, gdal.GA_Update)
    assert test_ds is not None, 'Failed to reopen test dataset.'
    nd = test_ds.GetRasterBand(1).GetNoDataValue()
    assert nd == 33, 'Invalid NoData value after dataset reopen.'
    test_ds.GetRasterBand(1).SetNoDataValue(55)
    test_ds = None

    test_ds = gdal.Open(test_ds_name, gdal.GA_ReadOnly)
    assert test_ds is not None, 'Failed to reopen test dataset.'
    nd = test_ds.GetRasterBand(1).GetNoDataValue()
    assert nd == 55, 'Invalid NoData value after dataset update.'

    test_ds = None
    os.remove(test_ds_name)
###############################################################################
# Unit write test


def test_rmf_26():
    rmf_drv = gdal.GetDriverByName('RMF')
    assert rmf_drv is not None, 'RMF driver not found.'

    src_ds = gdal.Open('data/rmf/float64.mtw', gdal.GA_ReadOnly)

    assert src_ds is not None, 'Failed to open test dataset.'

    test_ds_name = 'tmp/unit.mtw'
    test_ds = rmf_drv.CreateCopy(test_ds_name, src_ds, options=['MTW=YES'])
    assert test_ds is not None, 'Failed to create test dataset copy.'

    test_ds.GetRasterBand(1).SetUnitType('cm')
    unittype = test_ds.GetRasterBand(1).GetUnitType()
    assert unittype == 'cm', 'Invalid UnitType after CreateCopy.'
    test_ds = None

    test_ds = gdal.Open(test_ds_name, gdal.GA_Update)
    assert test_ds is not None, 'Failed to reopen test dataset.'
    unittype = test_ds.GetRasterBand(1).GetUnitType()
    assert unittype == 'cm', 'Invalid UnitType after dataset reopen.'
    test_ds.GetRasterBand(1).SetUnitType('mm')
    test_ds = None

    test_ds = gdal.Open(test_ds_name, gdal.GA_ReadOnly)
    assert test_ds is not None, 'Failed to reopen test dataset.'
    unittype = test_ds.GetRasterBand(1).GetUnitType()
    assert unittype == 'mm', 'Invalid UnitType after dataset update.'

    test_ds.GetRasterBand(1).SetUnitType('ft')
    unittype = test_ds.GetRasterBand(1).GetUnitType()
    assert unittype == 'mm', 'Invalid UnitType after dataset update.'

    test_ds = None
    os.remove(test_ds_name)

###############################################################################
# Test read JPEG compressed RMF dataset


def test_rmf_27():

    if gdal.GetDriverByName('JPEG') is None:
        pytest.skip()

    cs1 = [50553, 27604, 36652] #
    cs2 = [51009, 27640, 37765] # osx, clang

    ds = gdal.Open('data/rmf/jpeg-in-rmf.rsw', gdal.GA_ReadOnly)
    assert ds is not None, 'Failed to open test dataset.'

    md = ds.GetMetadata('IMAGE_STRUCTURE')
    assert md['COMPRESSION'] == 'JPEG', \
        ('"COMPRESSION" value is "%s" but expected "JPEG"' %
                              md['COMPRESSION'])

    cs = [0, 0, 0]
    for iBand in range(ds.RasterCount):
        band = ds.GetRasterBand(iBand + 1)
        cs[iBand] = band.Checksum()

    assert cs == cs1 or cs == cs2, ('Invalid checksum %s expected %s or %s.' %
                             (str(cs), str(cs1), str(cs2)))


###############################################################################
# Check compression metadata


def test_rmf_28a():

    ds = gdal.Open('data/rmf/byte-lzw.rsw', gdal.GA_ReadOnly)
    assert ds is not None, 'Failed to open test dataset.'

    md = ds.GetMetadata('IMAGE_STRUCTURE')
    assert md['COMPRESSION'] == 'LZW', \
        ('"COMPRESSION" value is "%s" but expected "LZW"' %
                              md['COMPRESSION'])


def test_rmf_28b():

    ds = gdal.Open('data/rmf/t100.mtw', gdal.GA_ReadOnly)
    assert ds is not None, 'Failed to open test dataset.'

    md = ds.GetMetadata('IMAGE_STRUCTURE')
    assert md['COMPRESSION'] == 'RMF_DEM', \
        ('"COMPRESSION" value is "%s" but expected "RMF_DEM"' %
                              md['COMPRESSION'])


###############################################################################
# Check EPSG code

def test_rmf_29():

    rmf_drv = gdal.GetDriverByName('RMF')
    assert rmf_drv is not None, 'RMF driver not found.'

    ds = gdal.Open('data/rmf/byte.rsw', gdal.GA_ReadOnly)
    assert ds is not None, 'Failed to open test dataset.'

    test_ds_name = 'tmp/epsg.rsw'
    test_ds = rmf_drv.CreateCopy(test_ds_name, ds)
    assert test_ds is not None, 'Failed to create test dataset copy.'

    sr = osr.SpatialReference()
    sr.SetFromUserInput('EPSG:3388')
    test_ds.SetProjection(sr.ExportToWkt())
    test_ds = None;
    ds = None

    test_ds = gdal.Open(test_ds_name, gdal.GA_ReadOnly)
    assert test_ds is not None, 'Failed to open test dataset.'

    wkt = test_ds.GetProjectionRef()
    sr = osr.SpatialReference()
    sr.SetFromUserInput(wkt)
    assert str(sr.GetAuthorityCode(None)) == '3388', ('EPSG code is %s expected 3388.' %
                             str(sr.GetAuthorityCode(None)))


###############################################################################
# Check interleaved access

def test_rmf_30():

    ds_name = 'tmp/interleaved.tif'
    gdal.Translate(ds_name, 'data/rmf/rgbsmall-lzw.rsw',
                   format='GTiff')

    ds = gdal.Open(ds_name)
    assert ds is not None, ('Can\'t open ' + ds_name)
    expected_cs = [40503, 41429, 40238]
    cs = [ds.GetRasterBand(1).Checksum(),
          ds.GetRasterBand(2).Checksum(),
          ds.GetRasterBand(3).Checksum()]
    assert cs == expected_cs, ('Invalid checksum %s expected %s.' %
                             (str(cs), str(expected_cs)))


###############################################################################
# Check compressed write


def test_rmf_31a():

    tst = gdaltest.GDALTest('rmf', 'small_world.tif', 1,
                            30111, options=['COMPRESS=NONE'])

    return tst.testCreateCopy(check_minmax=0, check_srs=1, check_gt=1)


def test_rmf_31b():

    tst = gdaltest.GDALTest('rmf', 'small_world.tif', 1,
                            30111, options=['COMPRESS=LZW'])

    return tst.testCreateCopy(check_minmax=0, check_srs=1, check_gt=1)


def test_rmf_31c():

    ds_name = 'tmp/rmf_31c.rsw'
    gdal.Translate(ds_name, 'data/small_world.tif',
                   format='RMF', options='-co COMPRESS=JPEG')

    ds = gdal.Open(ds_name)
    assert ds is not None, ('Can\'t open ' + ds_name)
    expected_cs1 = [25789, 27405, 31974]
    expected_cs2 = [23764, 25265, 33585] # osx
    expected_cs_jpeg9e = [21031, 26574, 34780] # libjpeg 9e
    cs = [ds.GetRasterBand(1).Checksum(),
          ds.GetRasterBand(2).Checksum(),
          ds.GetRasterBand(3).Checksum()]

    assert cs in (expected_cs1, expected_cs2, expected_cs_jpeg9e)


def test_rmf_31d():

    tst = gdaltest.GDALTest('rmf', 'rmf/t100.mtw', 1,
                            6388, options=['MTW=YES', 'COMPRESS=RMF_DEM'])

    return tst.testCreateCopy(check_minmax=0, check_srs=1, check_gt=1)


def rmf_31e_data_gen(min_val, max_val, stripeSize, sx):
    numpy = pytest.importorskip('numpy')
    x = numpy.array([[min_val,max_val//2],[max_val,1]], dtype = numpy.int32)
    x = numpy.block([[x, numpy.flip(x,0)],
                    [numpy.flip(x, 1), x.transpose()]])
    x = numpy.tile(x, (stripeSize // x.shape[0], sx // x.shape[1]))
    return x


def test_rmf_31e():
    numpy = pytest.importorskip('numpy')

    drv = gdal.GetDriverByName('Gtiff')
    if drv is None:
        pytest.skip()
    # Create test data
    stripeSize = 32
    sx = 256
    sy = 8*stripeSize
    tst_name = 'tmp/rmf_31e.tif'
    tst_ds = drv.Create(tst_name, sx, sy, 1, gdal.GDT_Int32 )
    assert tst_ds is not None, ('Can\'t create ' + tst_name)

    # No deltas
    buff = numpy.zeros((sx, stripeSize), dtype = numpy.int32)
    tst_ds.GetRasterBand(1).WriteArray(buff, 0, 0)

    # 4-bit deltas
    buff = rmf_31e_data_gen(0, 16 - 1, stripeSize, sx)
    tst_ds.GetRasterBand(1).WriteArray(buff, 0, stripeSize)

    # 8-bit deltas
    buff = rmf_31e_data_gen(0, 256 - 1, stripeSize, sx)
    tst_ds.GetRasterBand(1).WriteArray(buff, 0, stripeSize*2)

    # 12-bit deltas
    buff = rmf_31e_data_gen(0, 256*16 - 1, stripeSize, sx)
    tst_ds.GetRasterBand(1).WriteArray(buff, 0, stripeSize*3)

    # 16-bit deltas
    buff = rmf_31e_data_gen(0, 256*256 - 1, stripeSize, sx)
    tst_ds.GetRasterBand(1).WriteArray(buff, 0, stripeSize*4)

    # 24-bit deltas
    buff = rmf_31e_data_gen(0, 256*256*256 - 1, stripeSize, sx)
    tst_ds.GetRasterBand(1).WriteArray(buff, 0, stripeSize*5)

    # 32-bit deltas
    buff = rmf_31e_data_gen(0, 256*256*256*128 - 1, stripeSize, sx)
    tst_ds.GetRasterBand(1).WriteArray(buff, 0, stripeSize*6)

    # Negative values
    buff = rmf_31e_data_gen(-(256*256*256 - 2), 256*256*256 - 2, stripeSize, sx)
    tst_ds.GetRasterBand(1).WriteArray(buff, 0, stripeSize*7)

    tst_ds = None
    tst_ds = gdal.Open(tst_name)
    assert tst_ds is not None, ('Can\'t open ' + tst_name)

    cs = tst_ds.GetRasterBand(1).Checksum()
    tst_ds = None

    tst = gdaltest.GDALTest('rmf', '../' + tst_name, 1,
                            cs, options=['MTW=YES', 'COMPRESS=RMF_DEM'])

    return tst.testCreateCopy(check_minmax=0, check_srs=1, check_gt=1)


###############################################################################
# Check parallel compression

def test_rmf_32a():

    ds_name = 'tmp/rmf_32a.rsw'
    gdal.Translate(ds_name, 'data/small_world.tif', format='RMF',
                   options='-outsize 400% 400% -co COMPRESS=LZW -co NUM_THREADS=0')

    tst = gdaltest.GDALTest('rmf', '../' + ds_name, 1, 5540)
    res = tst.testOpen(check_gt=None)
    os.remove(ds_name)

    return res


def test_rmf_32b():

    ds_name = 'tmp/rmf_32b.rsw'
    gdal.Translate(ds_name, 'data/small_world.tif', format='RMF',
                   options='-outsize 400% 400% -co COMPRESS=LZW -co NUM_THREADS=4')

    tst = gdaltest.GDALTest('rmf', '../' + ds_name, 1, 5540)
    res = tst.testOpen(check_gt=None)
    os.remove(ds_name)

    return res


###############################################################################
# Parallel build overviews on newly created RSW file


def test_rmf_32c():
    ds_name = 'tmp/rmf_32c.rsw'
    gdal.Translate(ds_name, 'data/small_world.tif', format='RMF',
                   options='-outsize 400% 400% -co COMPRESS=LZW -co NUM_THREADS=4')

    res = rmf_build_ov(source=ds_name,
                       testid='32c',
                       options=['RMFHUGE=NO', 'COMPRESS=LZW', 'NUM_THREADS=4'],
                       ov_sizes=[[800, 400], [400, 200]],
                       crs=[[50261, 64846, 28175], [30111, 32302, 40026]],
                       reopen=False)
    os.remove(ds_name)

    return res


###############################################################################
# Read 1-bit & 4-bit files


def test_rmf_33a():

    tst = gdaltest.GDALTest('rmf', 'rmf/1bit.rsw', 1, 34325)
    return tst.testOpen()


def test_rmf_33b():

    tst = gdaltest.GDALTest('rmf', 'rmf/4bit.rsw', 1, 55221)
    return tst.testOpen()


def test_rmf_33c():

    tst = gdaltest.GDALTest('rmf', 'rmf/4bit-lzw.rsw', 1, 55221)
    return tst.testOpen()


###############################################################################
# Flush NoData blocks in MTW 


def test_rmf_34():
    numpy = pytest.importorskip('numpy')

    drv = gdal.GetDriverByName('RMF')
    tst_name = 'tmp/rmf_34.mtw'
    tst_ds = drv.Create(tst_name, 32, 32, 1, gdal.GDT_Int32, 
                        options=['MTW=YES', 'BLOCKXSIZE=16', 'BLOCKYSIZE=16'] )
    assert tst_ds is not None, ('Can\'t create ' + tst_name)
    nodata = 0
    tst_ds.GetRasterBand(1).SetNoDataValue(nodata)
    # Write NoData block
    buff = numpy.full((16, 16), nodata, dtype = numpy.int32)
    assert gdal.CE_None == tst_ds.GetRasterBand(1).WriteArray(buff, 0, 0)
    gdal.ErrorReset()
    tst_ds.FlushCache()
    assert gdal.GetLastErrorType() == gdal.CE_None, 'Flush cache failed: ' + gdal.GetLastErrorMsg()
    # Write valid block
    min_valid_data, max_valid_data = 1, 2
    buff = numpy.full((16, 16), min_valid_data, dtype = numpy.int32)
    assert gdal.CE_None == tst_ds.GetRasterBand(1).WriteArray(buff, 16, 0)
    buff = numpy.full((16, 16), max_valid_data, dtype = numpy.int32)
    assert gdal.CE_None == tst_ds.GetRasterBand(1).WriteArray(buff, 16, 16)
    tst_ds.FlushCache()
    assert gdal.GetLastErrorType() == gdal.CE_None, 'Flush cache failed: ' + gdal.GetLastErrorMsg()
    tst_ds = None

    tst_ds = gdal.Open(tst_name)
    assert tst_ds is not None, ('Can\'t open ' + tst_name)
    assert min_valid_data == int(tst_ds.GetMetadataItem('ELEVATION_MINIMUM'))
    assert max_valid_data == int(tst_ds.GetMetadataItem('ELEVATION_MAXIMUM'))
    tst_ds = None
    os.remove(tst_name)

###############################################################################
