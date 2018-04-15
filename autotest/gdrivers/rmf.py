#!/usr/bin/env python
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
import sys

sys.path.append('../pymod')

import gdaltest
import gdal

###############################################################################
# Perform simple read tests.


def rmf_1():

    tst = gdaltest.GDALTest('rmf', 'byte.rsw', 1, 4672)
    return tst.testOpen(check_gt=(440720, 60, 0, 3751320, 0, -60))


def rmf_2():

    tst = gdaltest.GDALTest('rmf', 'byte-lzw.rsw', 1, 4672)
    with gdaltest.error_handler():
        return tst.testOpen(check_gt=(440720, 60, 0, 3751320, 0, -60))


def rmf_3():

    tst = gdaltest.GDALTest('rmf', 'float64.mtw', 1, 4672)
    with gdaltest.error_handler():
        return tst.testOpen(check_gt=(440720, 60, 0, 3751320, 0, -60))


def rmf_4():

    tst = gdaltest.GDALTest('rmf', 'rgbsmall.rsw', 1, 21212)
    ret = tst.testOpen(check_gt=(-44.840320, 0.003432, 0,
                                 -22.932584, 0, -0.003432))
    if ret != 'success':
        return 'fail'

    tst = gdaltest.GDALTest('rmf', 'rgbsmall.rsw', 2, 21053)
    ret = tst.testOpen(check_gt=(-44.840320, 0.003432, 0,
                                 -22.932584, 0, -0.003432))
    if ret != 'success':
        return 'fail'

    tst = gdaltest.GDALTest('rmf', 'rgbsmall.rsw', 3, 21349)
    return tst.testOpen(check_gt=(-44.840320, 0.003432, 0,
                                  -22.932584, 0, -0.003432))


def rmf_5():

    tst = gdaltest.GDALTest('rmf', 'rgbsmall-lzw.rsw', 1, 21212)
    with gdaltest.error_handler():
        ret = tst.testOpen(check_gt=(-44.840320, 0.003432, 0,
                                     -22.932584, 0, -0.003432))
    if ret != 'success':
        return 'fail'

    tst = gdaltest.GDALTest('rmf', 'rgbsmall-lzw.rsw', 2, 21053)
    with gdaltest.error_handler():
        ret = tst.testOpen(check_gt=(-44.840320, 0.003432, 0,
                                     -22.932584, 0, -0.003432))
    if ret != 'success':
        return 'fail'

    tst = gdaltest.GDALTest('rmf', 'rgbsmall-lzw.rsw', 3, 21349)
    with gdaltest.error_handler():
        return tst.testOpen(check_gt=(-44.840320, 0.003432, 0,
                                      -22.932584, 0, -0.003432))


def rmf_6():

    tst = gdaltest.GDALTest('rmf', 'big-endian.rsw', 1, 7782)
    with gdaltest.error_handler():
        ret = tst.testOpen()
    if ret != 'success':
        return 'fail'

    tst = gdaltest.GDALTest('rmf', 'big-endian.rsw', 2, 8480)
    with gdaltest.error_handler():
        ret = tst.testOpen()
    if ret != 'success':
        return 'fail'

    tst = gdaltest.GDALTest('rmf', 'big-endian.rsw', 3, 4195)
    with gdaltest.error_handler():
        return tst.testOpen()

###############################################################################
# Create simple copy and check.


def rmf_7():

    tst = gdaltest.GDALTest('rmf', 'byte.rsw', 1, 4672)

    return tst.testCreateCopy(check_srs=1, check_gt=1, vsimem=1)


def rmf_8():

    tst = gdaltest.GDALTest('rmf', 'rgbsmall.rsw', 2, 21053)

    return tst.testCreateCopy(check_srs=1, check_gt=1)

###############################################################################
# Create RMFHUGE=YES


def rmf_9():

    tst = gdaltest.GDALTest('rmf', 'byte.rsw', 1, 4672, options=['RMFHUGE=YES'])

    return tst.testCreateCopy(check_srs=1, check_gt=1, vsimem=1)

###############################################################################
# Compressed DEM


def rmf_10():

    tst = gdaltest.GDALTest('rmf', 't100.mtw', 1, 6388)

    with gdaltest.error_handler():
        return tst.testOpen()

###############################################################################
# Overviews


def rmf_11():

    test_fn = '/vsigzip/data/overviews.rsw.gz'
    src_ds = gdal.Open(test_fn)

    if src_ds is None:
        gdaltest.post_reason('Failed to open test dataset.')
        return 'fail'

    band1 = src_ds.GetRasterBand(1)

    if band1.GetOverviewCount() != 3:
        gdaltest.post_reason('overviews is missing')
        return 'fail'

    ovr_n = (0, 1, 2)
    ovr_size = (256, 64, 16)
    ovr_checksum = (32756, 51233, 3192)

    for i in ovr_n:
        ovr_band = band1.GetOverview(i)
        if ovr_band.XSize != ovr_size[i] or ovr_band.YSize != ovr_size[i]:
            msg = 'overview wrong size: overview %d, size = %d * %d,' % \
                  (i, ovr_band.XSize, ovr_band.YSize)
            gdaltest.post_reason(msg)
            return 'fail'

        if ovr_band.Checksum() != ovr_checksum[i]:
            msg = 'overview wrong checkum: overview %d, checksum = %d,' % \
                  (i, ovr_band.Checksum())
            gdaltest.post_reason(msg)
            return 'fail'

    return 'success'

###############################################################################
# Check file open with cucled header offsets .


def rmf_12a():

    tst = gdaltest.GDALTest('rmf', 'cucled-1.rsw', 1, 4672)
    with gdaltest.error_handler():
        return tst.testOpen(check_gt=(440720, 60, 0, 3751320, 0, -60))

###############################################################################
# Check file open with cucled header offsets .


def rmf_12b():

    tst = gdaltest.GDALTest('rmf', 'cucled-2.rsw', 1, 4672)
    with gdaltest.error_handler():
        return tst.testOpen(check_gt=(440720, 60, 0, 3751320, 0, -60))

###############################################################################
# Check file open with invalid subheader marker.


def rmf_12c():

    tst = gdaltest.GDALTest('rmf', 'invalid-subheader.rsw', 1, 4672)
    with gdaltest.error_handler():
        return tst.testOpen(check_gt=(440720, 60, 0, 3751320, 0, -60))

###############################################################################
# Check file open with corrupted subheader.


def rmf_12d():

    tst = gdaltest.GDALTest('rmf', 'corrupted-subheader.rsw', 1, 4672)
    return tst.testOpen(check_gt=(440720, 60, 0, 3751320, 0, -60))

###############################################################################
# Build overviews and check


def rmf_build_ov(source, testid, options, ov_sizes, crs, reopen=False, pass_count=1):

    rmf_drv = gdal.GetDriverByName('RMF')
    if rmf_drv is None:
        gdaltest.post_reason('RMF driver not found.')
        return 'fail'

    src_ds = gdal.Open('data/' + source, gdal.GA_ReadOnly)

    if src_ds is None:
        gdaltest.post_reason('Failed to open test dataset.')
        return 'fail'

    test_ds_name = 'tmp/ov-' + testid + '-' + source
    src_ds = rmf_drv.CreateCopy(test_ds_name, src_ds, options=options)
    if src_ds is None:
        gdaltest.post_reason('Failed to create test dataset copy.')
        return 'fail'

    for pass_n in range(pass_count):
        if reopen:
            src_ds = None
            src_ds = gdal.Open(test_ds_name, gdal.GA_Update)

            if src_ds is None:
                gdaltest.post_reason('Failed to open test dataset.')
                return 'fail'

        reopen = True
        err = src_ds.BuildOverviews(overviewlist=[2, 4])
        if err != 0:
            gdaltest.post_reason('BuildOverviews reports an error')
            return 'fail'

        src_ds = None
        src_ds = gdal.Open(test_ds_name, gdal.GA_ReadOnly)

        for iBand in range(src_ds.RasterCount):
            band = src_ds.GetRasterBand(iBand + 1)

            if band.GetOverviewCount() != 2:
                gdaltest.post_reason('overviews missing')
                return 'fail'

            for iOverview in range(band.GetOverviewCount()):
                ovr_band = band.GetOverview(iOverview)
                if ovr_band.XSize != ov_sizes[iOverview][0] or \
                   ovr_band.YSize != ov_sizes[iOverview][1]:
                    msg = 'overview wrong size: band %d, overview %d, size = %d * %d,' % \
                          (iBand, iOverview, ovr_band.XSize, ovr_band.YSize)
                    gdaltest.post_reason(msg)
                    return 'fail'

                if ovr_band.Checksum() != crs[iOverview][iBand]:
                    msg = 'overview wrong checkum: band %d, overview %d, checksum = %d,' % \
                          (iBand, iOverview, ovr_band.Checksum())
                    gdaltest.post_reason(msg)
                    return 'fail'

    src_ds = None
    os.remove(test_ds_name)

    return 'success'

###############################################################################
# Build overviews on newly created RSW file


def rmf_13():
    return rmf_build_ov(source='byte.rsw',
                        testid='13',
                        options=['RMFHUGE=NO'],
                        ov_sizes=[[10, 10], [5, 5]],
                        crs=[[1087, 1087, 1087], [328, 328, 328]],
                        reopen=False)

###############################################################################
# Build overviews on newly created huge RSW file


def rmf_14():
    return rmf_build_ov(source='byte.rsw',
                        testid='14',
                        options=['RMFHUGE=YES'],
                        ov_sizes=[[10, 10], [5, 5]],
                        crs=[[1087, 1087, 1087], [328, 328, 328]],
                        reopen=False)

###############################################################################
# Build overviews on closed and reopened RSW file


def rmf_15():
    return rmf_build_ov(source='byte.rsw',
                        testid='15',
                        options=['RMFHUGE=NO'],
                        ov_sizes=[[10, 10], [5, 5]],
                        crs=[[1087, 1087, 1087], [328, 328, 328]],
                        reopen=True)

###############################################################################
# Build overviews on closed and reopened huge RSW file


def rmf_16():
    return rmf_build_ov(source='byte.rsw',
                        testid='16',
                        options=['RMFHUGE=YES'],
                        ov_sizes=[[10, 10], [5, 5]],
                        crs=[[1087, 1087, 1087], [328, 328, 328]],
                        reopen=True)

###############################################################################
# Build overviews on newly created MTW file


def rmf_17():
    return rmf_build_ov(source='float64.mtw',
                        testid='17',
                        options=['RMFHUGE=NO', 'MTW=YES'],
                        ov_sizes=[[10, 10], [5, 5]],
                        crs=[[1087, 1087, 1087], [328, 328, 328]],
                        reopen=False)

###############################################################################
# Build overviews on newly created MTW file


def rmf_18():
    return rmf_build_ov(source='float64.mtw',
                        testid='18',
                        options=['RMFHUGE=YES', 'MTW=YES'],
                        ov_sizes=[[10, 10], [5, 5]],
                        crs=[[1087], [328]],
                        reopen=False)

###############################################################################
# Build overviews on closed and reopened MTW file


def rmf_19():
    return rmf_build_ov(source='float64.mtw',
                        testid='19',
                        options=['RMFHUGE=NO', 'MTW=YES'],
                        ov_sizes=[[10, 10], [5, 5]],
                        crs=[[1087], [328]],
                        reopen=True)

###############################################################################
# Build overviews on closed and reopened huge MTW file


def rmf_20():
    return rmf_build_ov(source='float64.mtw',
                        testid='20',
                        options=['RMFHUGE=YES', 'MTW=YES'],
                        ov_sizes=[[10, 10], [5, 5]],
                        crs=[[1087], [328]],
                        reopen=True)

###############################################################################
# Recreate overviews on newly created MTW file


def rmf_21():
    return rmf_build_ov(source='float64.mtw',
                        testid='21',
                        options=['RMFHUGE=NO', 'MTW=YES'],
                        ov_sizes=[[10, 10], [5, 5]],
                        crs=[[1087], [328]],
                        reopen=False,
                        pass_count=2)

###############################################################################
# Recreate overviews on newly created huge MTW file


def rmf_22():
    return rmf_build_ov(source='float64.mtw',
                        testid='22',
                        options=['RMFHUGE=YES', 'MTW=YES'],
                        ov_sizes=[[10, 10], [5, 5]],
                        crs=[[1087], [328]],
                        reopen=False,
                        pass_count=2)


###############################################################################
# Recreate overviews on closed and reopened MTW file

def rmf_23():
    return rmf_build_ov(source='float64.mtw',
                        testid='23',
                        options=['RMFHUGE=NO', 'MTW=YES'],
                        ov_sizes=[[10, 10], [5, 5]],
                        crs=[[1087], [328]],
                        reopen=True,
                        pass_count=2)

###############################################################################
# Recreate overviews on closed and reopened huge MTW file


def rmf_24():
    return rmf_build_ov(source='float64.mtw',
                        testid='24',
                        options=['RMFHUGE=YES', 'MTW=YES'],
                        ov_sizes=[[10, 10], [5, 5]],
                        crs=[[1087], [328]],
                        reopen=True,
                        pass_count=2)

###############################################################################
# Nodata write test


def rmf_25():
    rmf_drv = gdal.GetDriverByName('RMF')
    if rmf_drv is None:
        gdaltest.post_reason('RMF driver not found.')
        return 'fail'

    src_ds = gdal.Open('data/byte.rsw', gdal.GA_ReadOnly)

    if src_ds is None:
        gdaltest.post_reason('Failed to open test dataset.')
        return 'fail'

    test_ds_name = 'tmp/nodata.rsw'
    test_ds = rmf_drv.CreateCopy(test_ds_name, src_ds)
    if test_ds is None:
        gdaltest.post_reason('Failed to create test dataset copy.')
        return 'fail'

    test_ds.GetRasterBand(1).SetNoDataValue(33)
    nd = test_ds.GetRasterBand(1).GetNoDataValue()
    if nd != 33:
        gdaltest.post_reason('Invalid NoData value after CreateCopy.')
        return 'fail'
    test_ds = None

    test_ds = gdal.Open(test_ds_name, gdal.GA_Update)
    if test_ds is None:
        gdaltest.post_reason('Failed to reopen test dataset.')
        return 'fail'
    nd = test_ds.GetRasterBand(1).GetNoDataValue()
    if nd != 33:
        gdaltest.post_reason('Invalid NoData value after dataset reopen.')
        return 'fail'
    test_ds.GetRasterBand(1).SetNoDataValue(55)
    test_ds = None

    test_ds = gdal.Open(test_ds_name, gdal.GA_ReadOnly)
    if test_ds is None:
        gdaltest.post_reason('Failed to reopen test dataset.')
        return 'fail'
    nd = test_ds.GetRasterBand(1).GetNoDataValue()
    if nd != 55:
        gdaltest.post_reason('Invalid NoData value after dataset update.')
        return 'fail'

    test_ds = None
    os.remove(test_ds_name)

    return 'success'
###############################################################################
# Unit write test


def rmf_26():
    rmf_drv = gdal.GetDriverByName('RMF')
    if rmf_drv is None:
        gdaltest.post_reason('RMF driver not found.')
        return 'fail'

    src_ds = gdal.Open('data/float64.mtw', gdal.GA_ReadOnly)

    if src_ds is None:
        gdaltest.post_reason('Failed to open test dataset.')
        return 'fail'

    test_ds_name = 'tmp/unit.mtw'
    test_ds = rmf_drv.CreateCopy(test_ds_name, src_ds, options=['MTW=YES'])
    if test_ds is None:
        gdaltest.post_reason('Failed to create test dataset copy.')
        return 'fail'

    test_ds.GetRasterBand(1).SetUnitType('cm')
    unittype = test_ds.GetRasterBand(1).GetUnitType()
    if unittype != 'cm':
        gdaltest.post_reason('Invalid UnitType after CreateCopy.')
        return 'fail'
    test_ds = None

    test_ds = gdal.Open(test_ds_name, gdal.GA_Update)
    if test_ds is None:
        gdaltest.post_reason('Failed to reopen test dataset.')
        return 'fail'
    unittype = test_ds.GetRasterBand(1).GetUnitType()
    if unittype != 'cm':
        gdaltest.post_reason('Invalid UnitType after dataset reopen.')
        return 'fail'
    test_ds.GetRasterBand(1).SetUnitType('mm')
    test_ds = None

    test_ds = gdal.Open(test_ds_name, gdal.GA_ReadOnly)
    if test_ds is None:
        gdaltest.post_reason('Failed to reopen test dataset.')
        return 'fail'
    unittype = test_ds.GetRasterBand(1).GetUnitType()
    if unittype != 'mm':
        gdaltest.post_reason('Invalid UnitType after dataset update.')
        return 'fail'

    test_ds.GetRasterBand(1).SetUnitType('ft')
    unittype = test_ds.GetRasterBand(1).GetUnitType()
    if unittype != 'mm':
        gdaltest.post_reason('Invalid UnitType after dataset update.')
        return 'fail'

    test_ds = None
    os.remove(test_ds_name)

    return 'success'

###############################################################################


gdaltest_list = [
    rmf_1,
    rmf_2,
    rmf_3,
    rmf_4,
    rmf_5,
    rmf_6,
    rmf_7,
    rmf_8,
    rmf_9,
    rmf_10,
    rmf_11,
    rmf_12a,
    rmf_12b,
    rmf_12c,
    rmf_12d,
    rmf_13,
    rmf_14,
    rmf_15,
    rmf_16,
    rmf_17,
    rmf_18,
    rmf_19,
    rmf_20,
    rmf_21,
    rmf_22,
    rmf_23,
    rmf_24,
    rmf_25,
    rmf_26
]

if __name__ == '__main__':

    gdaltest.setup_run('rmf')

    gdaltest.run_tests(gdaltest_list)

    gdaltest.summarize()
