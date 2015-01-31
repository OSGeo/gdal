#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for USGSDEM driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2008-2011, Even Rouault <even dot rouault at mines-paris dot org>
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
from osgeo import gdal
from osgeo import osr

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Test truncated version of http://download.osgeo.org/gdal/data/usgsdem/022gdeme

def usgsdem_1():

    tst = gdaltest.GDALTest( 'USGSDEM', '022gdeme_truncated', 1, 1583 )
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS('NAD27')
    return tst.testOpen( check_prj = srs.ExportToWkt(),
                         check_gt = ( -67.00041667, 0.00083333, 0.0, 50.000416667, 0.0, -0.00083333 ) )

###############################################################################
# Test truncated version of http://download.osgeo.org/gdal/data/usgsdem/114p01_0100_deme.dem

def usgsdem_2():

    tst = gdaltest.GDALTest( 'USGSDEM', '114p01_0100_deme_truncated.dem', 1, 53864 )
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS('NAD27')
    return tst.testOpen( check_prj = srs.ExportToWkt(),
                         check_gt = ( -136.25010416667, 0.000208333, 0.0, 59.25010416667, 0.0, -0.000208333 ) )

###############################################################################
# Test truncated version of file that triggered bug #2348

def usgsdem_3():

    tst = gdaltest.GDALTest( 'USGSDEM', '39079G6_truncated.dem', 1, 61424 )
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS('WGS72')
    srs.SetUTM(17)
    return tst.testOpen( check_prj = srs.ExportToWkt(),
                         check_gt = ( 606855.0, 30.0, 0.0, 4414605.0, 0.0, -30.0 ) )

###############################################################################
# Test CreateCopy()

def usgsdem_4():

    tst = gdaltest.GDALTest( 'USGSDEM', '39079G6_truncated.dem', 1, 61424, \
        options = [ 'RESAMPLE=Nearest' ] )
    return tst.testCreateCopy ( check_gt = 1, check_srs = 1, vsimem = 1 )


###############################################################################
# Test CreateCopy() without any creation options

def usgsdem_5():

    ds = gdal.Open('data/n43.dt0')
    ds2 = gdal.GetDriverByName('USGSDEM').CreateCopy('tmp/n43.dem', ds, \
        options = [ 'RESAMPLE=Nearest' ])

    if ds.GetRasterBand(1).Checksum() != ds2.GetRasterBand(1).Checksum():
        gdaltest.post_reason( 'Bad cechksum.' )
        return 'fail'

    gt1 = ds.GetGeoTransform()
    gt2 = ds2.GetGeoTransform()
    for i in range(6):
        if abs(gt1[i]-gt2[i]) > 1e-5:
            print('')
            print('old = ', gt1)
            print('new = ', gt2)
            gdaltest.post_reason( 'Geotransform differs.' )
            return 'fail'

    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS('WGS84')
    if ds2.GetProjectionRef() != srs.ExportToWkt():
        gdaltest.post_reason( 'Bad SRS.' )
        return 'fail'

    ds2 = None

    return 'success'

###############################################################################
# Test CreateCopy() without a few creation options. Then create a new copy with TEMPLATE
# creation option and check that both files are binary identical.

def usgsdem_6():

    ds = gdal.Open('data/n43.dt0')
    ds2 = gdal.GetDriverByName('USGSDEM').CreateCopy('tmp/file_1.dem', ds, \
        options = [ 'PRODUCER=GDAL', 'OriginCode=GDAL', 'ProcessCode=A', \
        'RESAMPLE=Nearest' ] )

    ds3 = gdal.GetDriverByName('USGSDEM').CreateCopy('tmp/file_2.dem', ds2, \
        options = [ 'TEMPLATE=tmp/file_1.dem', 'RESAMPLE=Nearest' ] )

    del ds2
    del ds3

    f1 = open('tmp/file_1.dem', 'rb')
    f2 = open('tmp/file_2.dem', 'rb')

    # Skip the 40 first bytes because the dataset name will differ
    f1.seek(40, 0)
    f2.seek(40, 0)

    data1 = f1.read()
    data2 = f2.read()

    if data1 != data2:
        return 'fail'

    f1.close()
    f2.close()

    return 'success'

###############################################################################
# Test CreateCopy() with CDED50K profile

def usgsdem_7():

    ds = gdal.Open('data/n43.dt0')

    # To avoid warning about 'Unable to find NTS mapsheet lookup file: NTS-50kindex.csv'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds2 = gdal.GetDriverByName('USGSDEM').CreateCopy('tmp/000a00DEMz', ds,
                                                     options = [ 'PRODUCT=CDED50K', 'TOPLEFT=80w,44n', 'RESAMPLE=Nearest', 'ZRESOLUTION=1.1', 'INTERNALNAME=GDAL' ] )
    gdal.PopErrorHandler()

    if ds2.RasterXSize != 1201 or ds2.RasterYSize != 1201:
        gdaltest.post_reason( 'Bad image dimensions.' )
        return 'fail'

    expected_gt = (-80.000104166666674,0.000208333333333,0,44.000104166666667,0,-0.000208333333333)
    got_gt = ds2.GetGeoTransform()
    for i in range(6):
        if abs(expected_gt[i]-got_gt[i]) > 1e-5:
            print('')
            print('expected = ', expected_gt)
            print('got = ', got_gt)
            gdaltest.post_reason( 'Geotransform differs.' )
            return 'fail'

    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS('NAD83')
    if ds2.GetProjectionRef() != srs.ExportToWkt():
        gdaltest.post_reason( 'Bad SRS.' )
        return 'fail'

    ds2 = None

    return 'success'

###############################################################################
# Test truncated version of http://download.osgeo.org/gdal/data/usgsdem/various.zip/39109h1.dem
# Undocumented format

def usgsdem_8():

    tst = gdaltest.GDALTest( 'USGSDEM', '39109h1_truncated.dem', 1, 39443 )
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS('NAD27')
    srs.SetUTM(12)
    return tst.testOpen( check_prj = srs.ExportToWkt(),
                         check_gt = ( 660055.0, 10.0, 0.0, 4429465.0, 0.0, -10.0 ) )

###############################################################################
# Test truncated version of http://download.osgeo.org/gdal/data/usgsdem/various.zip/4619old.dem
# Old format

def usgsdem_9():

    tst = gdaltest.GDALTest( 'USGSDEM', '4619old_truncated.dem', 1, 10659 )
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS('NAD27')
    return tst.testOpen( check_prj = srs.ExportToWkt(),
                         check_gt = ( 18.99958333, 0.0008333, 0.0, 47.000416667, 0.0, -0.0008333 ) )

###############################################################################
# Cleanup

def usgsdem_cleanup():

    try:
        os.remove('tmp/n43.dem')
        os.remove('tmp/n43.dem.aux.xml')

        os.remove('tmp/file_1.dem')
        os.remove('tmp/file_1.dem.aux.xml')
        os.remove('tmp/file_2.dem')
        os.remove('tmp/file_2.dem.aux.xml')

        os.remove('tmp/000a00DEMz')
        os.remove('tmp/000a00DEMz.aux.xml')
    except:
        pass

    return 'success'


gdaltest_list = [
    usgsdem_1,
    usgsdem_2,
    usgsdem_3,
    usgsdem_4,
    usgsdem_5,
    usgsdem_6,
    usgsdem_7,
    usgsdem_8,
    usgsdem_9,
    usgsdem_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'usgsdem' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

