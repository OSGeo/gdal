#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test SRTMHGT support.
# Author:   Even Rouault < even dot rouault @ mines-paris dot org >
#
###############################################################################
# Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2010, Even Rouault <even dot rouault at mines-paris dot org>
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

sys.path.append( '../pymod' )

import gdaltest


###############################################################################
# Test a SRTMHGT Level 1 (made from a DTED Level 0)

def srtmhgt_1():

    ds = gdal.Open( 'data/n43.dt0' )

    bandSrc = ds.GetRasterBand(1)

    driver = gdal.GetDriverByName( "GTiff" );
    dsDst = driver.Create( 'tmp/n43.dt1.tif', 1201, 1201, 1, gdal.GDT_Int16 )
    dsDst.SetProjection('GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]')
    dsDst.SetGeoTransform((-80.0004166666666663, 0.0008333333333333, 0, 44.0004166666666670, 0, -0.0008333333333333))

    bandDst = dsDst.GetRasterBand(1)

    data = bandSrc.ReadRaster( 0, 0, 121, 121, 1201, 1201, gdal.GDT_Int16 )
    bandDst.WriteRaster( 0, 0, 1201, 1201, data, 1201, 1201, gdal.GDT_Int16 )

    bandDst.FlushCache()

    bandDst = None
    ds = None
    dsDst = None

    ds = gdal.Open( 'tmp/n43.dt1.tif' )
    driver = gdal.GetDriverByName( "SRTMHGT" );
    dsDst = driver.CreateCopy( 'tmp/n43w080.hgt', ds)

    band = dsDst.GetRasterBand(1)
    chksum = band.Checksum()

    if chksum != 60918:
        gdaltest.post_reason('Wrong checksum. Checksum found %d' % chksum)
        return 'fail'

    return 'success'


###############################################################################
# Test creating an in memory copy.

def srtmhgt_2():

    ds = gdal.Open( 'tmp/n43w080.hgt' )
    driver = gdal.GetDriverByName( "SRTMHGT" );
    dsDst = driver.CreateCopy( '/vsimem/n43w080.hgt', ds)

    band = dsDst.GetRasterBand(1)
    chksum = band.Checksum()

    if chksum != 60918:
        gdaltest.post_reason('Wrong checksum. Checksum found %d' % chksum)
        return 'fail'
    dsDst = None

    # Test update support
    dsDst = gdal.Open( '/vsimem/n43w080.hgt', gdal.GA_Update )
    dsDst.WriteRaster(0, 0, dsDst.RasterXSize, dsDst.RasterYSize,
                      dsDst.ReadRaster())
    dsDst.FlushCache()

    if chksum != 60918:
        gdaltest.post_reason('Wrong checksum. Checksum found %d' % chksum)
        return 'fail'
    dsDst = None

    return 'success'

###############################################################################
# Test reading from a .hgt.zip file

def srtmhgt_3():

    ds = gdal.Open( 'tmp/n43w080.hgt' )
    driver = gdal.GetDriverByName( "SRTMHGT" );
    driver.CreateCopy( '/vsizip//vsimem/N43W080.SRTMGL1.hgt.zip/N43W080.hgt', ds)

    dsDst = gdal.Open('/vsimem/N43W080.SRTMGL1.hgt.zip')

    band = dsDst.GetRasterBand(1)
    chksum = band.Checksum()

    if chksum != 60918:
        gdaltest.post_reason('Wrong checksum. Checksum found %d' % chksum)
        return 'fail'

    return 'success'

###############################################################################
# Cleanup.

def srtmhgt_cleanup():
    try:
        gdal.GetDriverByName( "SRTMHGT" ).Delete('tmp/n43w080.hgt')
        gdal.GetDriverByName( "SRTMHGT" ).Delete('/vsimem/n43w080.hgt')
        gdal.Unlink('/vsimem/N43W080.SRTMGL1.hgt.zip')
        os.remove( 'tmp/n43.dt1.tif' )
    except:
        pass
    return 'success'

gdaltest_list = [
    srtmhgt_1,
    srtmhgt_2,
    srtmhgt_3,
    srtmhgt_cleanup
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'srtmhgt' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
