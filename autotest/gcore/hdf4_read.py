#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic read support for a all datatypes from a HDF file.
# Author:   Andrey Kiselev, dron@remotesensing.org
# 
###############################################################################
# Copyright (c) 2003, Andrey Kiselev <dron@remotesensing.org>
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
# 
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################

import os
import sys

sys.path.append( '../pymod' )

import gdaltest
import gdal

###############################################################################
# When imported build a list of units based on the files available.

gdaltest_list = []

init_list = [ \
    ('byte_3.hdf', 1, 4672, None),
    ('int16_3.hdf', 1, 4672, None),
    ('uint16_3.hdf', 1, 4672, None),
    ('int32_3.hdf', 1, 4672, None),
    ('uint32_3.hdf', 1, 4672, None),
    ('float32_3.hdf', 1, 4672, None),
    ('float64_3.hdf', 1, 4672, None),
    ('utmsmall_3.hdf', 1, 50054, None),
    ('byte_2.hdf', 1, 4672, None),
    ('int16_2.hdf', 1, 4672, None),
    ('uint16_2.hdf', 1, 4672, None),
    ('int32_2.hdf', 1, 4672, None),
    ('uint32_2.hdf', 1, 4672, None),
    ('float32_2.hdf', 1, 4672, None),
    ('float64_2.hdf', 1, 4672, None),
    ('utmsmall_2.hdf', 1, 50054, None)]

###############################################################################
# Test HDF4_SDS with single subdataset

def hdf4_read_online_1():

    try:
        gdaltest.hdf4_drv = gdal.GetDriverByName('HDF4')
    except:
        gdaltest.hdf4_drv = None

    if gdaltest.hdf4_drv is None:
        return 'skip'

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/hdf4/A2004259075000.L2_LAC_SST.hdf', 'A2004259075000.L2_LAC_SST.hdf'):
        return 'skip'

    tst = gdaltest.GDALTest( 'HDF4Image', 'tmp/cache/A2004259075000.L2_LAC_SST.hdf', 1, 28189, filename_absolute = 1 )

    return tst.testOpen()

###############################################################################
# Test HDF4_SDS with GEOLOCATION info

def hdf4_read_online_2():

    if gdaltest.hdf4_drv is None:
        return 'skip'

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/hdf4/A2006005182000.L2_LAC_SST.x.hdf', 'A2006005182000.L2_LAC_SST.x.hdf'):
        return 'skip'

    tst = gdaltest.GDALTest( 'HDF4Image', 'HDF4_SDS:UNKNOWN:"tmp/cache/A2006005182000.L2_LAC_SST.x.hdf":13', 1, 13209, filename_absolute = 1 )

    ret = tst.testOpen()
    if ret != 'success':
        return ret

    ds = gdal.Open('HDF4_SDS:UNKNOWN:"tmp/cache/A2006005182000.L2_LAC_SST.x.hdf":13')
    md = ds.GetMetadata('GEOLOCATION')
    ds = None

    if md['X_DATASET'] != 'HDF4_SDS:UNKNOWN:"tmp/cache/A2006005182000.L2_LAC_SST.x.hdf":11':
        gdaltest.post_reason('Did not get expected X_DATASET')
        return 'fail'

    return 'success'


###############################################################################
# Test HDF4_EOS:EOS_GRID

def hdf4_read_online_3():

    if gdaltest.hdf4_drv is None:
        return 'skip'

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/hdf4/MO36MW14.chlor_MODIS.ADD2001089.004.2002186190207.hdf', 'MO36MW14.chlor_MODIS.ADD2001089.004.2002186190207.hdf'):
        return 'skip'

    tst = gdaltest.GDALTest( 'HDF4Image', 'tmp/cache/MO36MW14.chlor_MODIS.ADD2001089.004.2002186190207.hdf', 1, 34723, filename_absolute = 1 )

    ret = tst.testOpen()
    if ret != 'success':
        return ret

    ds = gdal.Open('tmp/cache/MO36MW14.chlor_MODIS.ADD2001089.004.2002186190207.hdf')
    gt = ds.GetGeoTransform()
    expected_gt = [-180.0, 0.3515625, 0.0, 90.0, 0.0, -0.3515625]
    for i in range(6):
        if (abs(gt[i] - expected_gt[i]) > 1e-8):
            print(gt)
            gdaltest.post_reason('did not get expected gt')
            return 'fail'

    srs = ds.GetProjectionRef()
    if srs.find('Clarke') == -1:
        gdaltest.post_reason('did not get expected projection')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test HDF4_SDS:SEAWIFS_L1A

def hdf4_read_online_4():

    if gdaltest.hdf4_drv is None:
        return 'skip'

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/hdf4/S2002196124536.L1A_HDUN.BartonBendish.extract.hdf', 'S2002196124536.L1A_HDUN.BartonBendish.extract.hdf'):
        return 'skip'

    tst = gdaltest.GDALTest( 'HDF4Image', 'tmp/cache/S2002196124536.L1A_HDUN.BartonBendish.extract.hdf', 1, 33112, filename_absolute = 1 )

    ret = tst.testOpen()
    if ret != 'success':
        return ret

    ds = gdal.Open('tmp/cache/S2002196124536.L1A_HDUN.BartonBendish.extract.hdf')
    if ds.RasterCount != 8:
        gdaltest.post_reason('did not get expected band number')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test fix for #2208

def hdf4_read_online_5():

    if gdaltest.hdf4_drv is None:
        return 'skip'

    # 13 MB
    if not gdaltest.download_file('ftp://data.nodc.noaa.gov/pub/data.nodc/pathfinder/Version5.0/Monthly/1991/199101.s04m1pfv50-sst-16b.hdf', '199101.s04m1pfv50-sst-16b.hdf'):
        return 'skip'

    tst = gdaltest.GDALTest( 'HDF4Image', 'tmp/cache/199101.s04m1pfv50-sst-16b.hdf', 1, 41173, filename_absolute = 1 )

    ret = tst.testOpen()
    if ret != 'success':
        return ret

    return 'success'

###############################################################################
# Test fix for #3386 where block size is dataset size

def hdf4_read_online_6():

    if gdaltest.hdf4_drv is None:
        return 'skip'

    # 1 MB
    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/hdf4/MOD09Q1G_EVI.A2006233.h07v03.005.2008338190308.hdf', 'MOD09Q1G_EVI.A2006233.h07v03.005.2008338190308.hdf'):
        return 'skip'

    tst = gdaltest.GDALTest( 'HDF4Image', 'HDF4_EOS:EOS_GRID:tmp/cache/MOD09Q1G_EVI.A2006233.h07v03.005.2008338190308.hdf:MODIS_NACP_EVI:MODIS_EVI', 1, 12197, filename_absolute = 1 )

    ret = tst.testOpen()
    if ret != 'success':
        return ret

    ds = gdal.Open('HDF4_EOS:EOS_GRID:tmp/cache/MOD09Q1G_EVI.A2006233.h07v03.005.2008338190308.hdf:MODIS_NACP_EVI:MODIS_EVI')
    if 'GetBlockSize' in dir(gdal.Band):
        (blockx, blocky) = ds.GetRasterBand(1).GetBlockSize()
        if blockx != 4800 or blocky != 4800:
            gdaltest.post_reason("Did not get expected block size")
            return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test fix for #3386 where block size is smaller than dataset size

def hdf4_read_online_7():

    if gdaltest.hdf4_drv is None:
        return 'skip'

    # 4 MB
    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/hdf4/MOD09A1.A2010041.h06v03.005.2010051001103.hdf', 'MOD09A1.A2010041.h06v03.005.2010051001103.hdf'):
        return 'skip'

    tst = gdaltest.GDALTest( 'HDF4Image', 'HDF4_EOS:EOS_GRID:tmp/cache/MOD09A1.A2010041.h06v03.005.2010051001103.hdf:MOD_Grid_500m_Surface_Reflectance:sur_refl_b01', 1, 54894, filename_absolute = 1 )

    ret = tst.testOpen()
    if ret != 'success':
        return ret

    ds = gdal.Open('HDF4_EOS:EOS_GRID:tmp/cache/MOD09A1.A2010041.h06v03.005.2010051001103.hdf:MOD_Grid_500m_Surface_Reflectance:sur_refl_b01')
    if 'GetBlockSize' in dir(gdal.Band):
        (blockx, blocky) = ds.GetRasterBand(1).GetBlockSize()
        if blockx != 2400 or blocky != 32:
            gdaltest.post_reason("Did not get expected block size")
            return 'fail'

    ds = None

    return 'success'


###############################################################################
# Test reading a HDF4_EOS:EOS_GRID where preferred block height reported would be 1
# but that will lead to very poor performance (#3386)

def hdf4_read_online_8():

    if gdaltest.hdf4_drv is None:
        return 'skip'

    # 5 MB
    if not gdaltest.download_file('ftp://e4ftl01u.ecs.nasa.gov/MODIS_Composites/MOLT/MOD13Q1.005/2006.06.10/MOD13Q1.A2006161.h21v13.005.2008234103220.hdf', 'MOD13Q1.A2006161.h21v13.005.2008234103220.hdf'):
        return 'skip'

    tst = gdaltest.GDALTest( 'HDF4Image', 'HDF4_EOS:EOS_GRID:tmp/cache/MOD13Q1.A2006161.h21v13.005.2008234103220.hdf:MODIS_Grid_16DAY_250m_500m_VI:250m 16 days NDVI', 1, 53837, filename_absolute = 1 )

    ret = tst.testOpen()
    if ret != 'success':
        return ret

    ds = gdal.Open('HDF4_EOS:EOS_GRID:tmp/cache/MOD13Q1.A2006161.h21v13.005.2008234103220.hdf:MODIS_Grid_16DAY_250m_500m_VI:250m 16 days NDVI')
    if 'GetBlockSize' in dir(gdal.Band):
        (blockx, blocky) = ds.GetRasterBand(1).GetBlockSize()
        if blockx != 4800 or blocky == 1:
            print('blockx=%d, blocky=%d' % (blockx, blocky))
            gdaltest.post_reason("Did not get expected block size")
            return 'fail'

    ds = None

    return 'success'


for item in init_list:
    ut = gdaltest.GDALTest( 'HDF4Image', item[0], item[1], item[2] )
    if ut is None:
        print( 'HDF4 tests skipped' )
        sys.exit()
    gdaltest_list.append( (ut.testOpen, item[0]) )

gdaltest_list.append( hdf4_read_online_1 )
gdaltest_list.append( hdf4_read_online_2 )
gdaltest_list.append( hdf4_read_online_3 )
gdaltest_list.append( hdf4_read_online_4 )
gdaltest_list.append( hdf4_read_online_5 )
gdaltest_list.append( hdf4_read_online_6 )
gdaltest_list.append( hdf4_read_online_7 )
gdaltest_list.append( hdf4_read_online_8 )

if __name__ == '__main__':

    gdaltest.setup_run( 'hdf4_read' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

