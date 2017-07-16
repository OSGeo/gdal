#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test ISIS3 formats.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2017, Hobu Inc
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

import json
import sys

sys.path.append( '../pymod' )

from osgeo import gdal
from osgeo import osr
import gdaltest

###############################################################################
# Perform simple read test on isis3 detached dataset.

def isis_1():
    srs = """PROJCS["Equirectangular Mars",
    GEOGCS["GCS_Mars",
        DATUM["D_Mars",
            SPHEROID["Mars_localRadius",3394813.857978216,0]],
        PRIMEM["Reference_Meridian",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Equirectangular"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",184.4129944],
    PARAMETER["standard_parallel_1",-15.1470003],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0]]
"""
    gt = (-4766.96484375, 10.102499961853027, 0.0,
          -872623.625, 0.0, -10.102499961853027)

    tst = gdaltest.GDALTest( 'ISIS3', 'isis3_detached.lbl', 1, 9978 )
    return tst.testOpen( check_prj = srs, check_gt = gt )

###############################################################################
# Perform simple read test on isis3 detached dataset.

def isis_2():
    srs = """PROJCS["Equirectangular mars",
    GEOGCS["GCS_mars",
        DATUM["D_mars",
            SPHEROID["mars_localRadius",3388271.702979241,0]],
        PRIMEM["Reference_Meridian",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Equirectangular"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",195.92],
    PARAMETER["standard_parallel_1",-38.88],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0]]
"""
    gt = (653.132641495800044, 0.38, 0,
          -2298409.710162799805403, 0, -0.38)

    tst = gdaltest.GDALTest( 'ISIS3', 'isis3_unit_test.cub', 1, 42403 )
    return tst.testOpen( check_prj = srs, check_gt = gt )

###############################################################################
# Perform simple read test on isis3 detached dataset with GeoTIFF image file

def isis_3():
    srs = """PROJCS["Equirectangular Mars",
    GEOGCS["GCS_Mars",
        DATUM["D_Mars",
            SPHEROID["Mars_localRadius",3394813.857978216,0]],
        PRIMEM["Reference_Meridian",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Equirectangular"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",184.4129944],
    PARAMETER["standard_parallel_1",-15.1470003],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0]]
"""
    gt = (-4766.96484375, 10.102499961853027, 0.0,
          -872623.625, 0.0, -10.102499961853027)

    tst = gdaltest.GDALTest( 'ISIS3', 'isis3_geotiff.lbl', 1, 9978 )
    return tst.testOpen( check_prj = srs, check_gt = gt )

# ISIS3 -> ISIS3 conversion
def isis_4():

    tst = gdaltest.GDALTest( 'ISIS3', 'isis3_detached.lbl', 1, 9978 )
    ret = tst.testCreateCopy( new_filename = '/vsimem/isis_tmp.lbl',
                             delete_copy = 0 )
    if ret != 'success':
        return ret
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    if ds.GetMetadataDomainList() != [ '', 'json:ISIS3' ]:
        gdaltest.post_reason('fail')
        print(ds.GetMetadataDomainList())
        return 'fail'
    lbl = ds.GetMetadata_List('json:ISIS3')[0]
    # Couldn't be preserved, since points to dangling file
    if lbl.find('OriginalLabel') >= 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if lbl.find('PositiveWest') >= 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if ds.GetRasterBand(1).GetMaskFlags() != 0:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetMaskFlags())
        return 'fail'
    if ds.GetRasterBand(1).GetMaskBand().Checksum() != 12220:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetMaskBand().Checksum())
        return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')

    # Preserve source Mapping group as well
    tst = gdaltest.GDALTest( 'ISIS3', 'isis3_detached.lbl', 1, 9978,
                            options = ['USE_SRC_MAPPING=YES'] )
    ret = tst.testCreateCopy( new_filename = '/vsimem/isis_tmp.lbl',
                             delete_copy = 0 )
    if ret != 'success':
        return ret
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    lbl = ds.GetMetadata_List('json:ISIS3')[0]
    if lbl.find('PositiveWest') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if lbl.find('Planetographic') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')

    # Preserve source Mapping group, but with a few overrides
    tst = gdaltest.GDALTest( 'ISIS3', 'isis3_detached.lbl', 1, 9978,
                            options = ['USE_SRC_MAPPING=YES',
                                       'LONGITUDE_DIRECTION=PositiveEast',
                                       'LATITUDE_TYPE=Planetocentric',
                                       'TARGET_NAME=my_label'] )
    ret = tst.testCreateCopy( new_filename = '/vsimem/isis_tmp.lbl',
                             delete_copy = 0 )
    if ret != 'success':
        return ret
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    lbl = ds.GetMetadata_List('json:ISIS3')[0]
    if lbl.find('PositiveEast') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if lbl.find('Planetocentric') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if lbl.find('my_label') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')

    return 'success'

# Label+image creation + WRITE_BOUNDING_DEGREES=NO option
def isis_5():

    tst = gdaltest.GDALTest( 'ISIS3', 'isis3_detached.lbl', 1, 9978,
                            options = ['USE_SRC_LABEL=NO',
                                       'WRITE_BOUNDING_DEGREES=NO'] )
    ret = tst.testCreateCopy( new_filename = '/vsimem/isis_tmp.lbl',
                             delete_copy = 0 )
    if ret != 'success':
        return ret
    if gdal.VSIStatL('/vsimem/isis_tmp.cub') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    lbl = ds.GetMetadata_List('json:ISIS3')[0]
    if lbl.find('MinimumLongitude') >= 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')
    return 'success'

# Detached label creation and COMMENT option
def isis_6():

    tst = gdaltest.GDALTest( 'ISIS3', 'isis3_detached.lbl', 1, 9978,
                            options = ['DATA_LOCATION=EXTERNAL',
                                       'USE_SRC_LABEL=NO',
                                       'COMMENT=my comment'] )
    ret = tst.testCreateCopy( new_filename = '/vsimem/isis_tmp.lbl',
                             delete_copy = 0 )
    if ret != 'success':
        return ret
    if gdal.VSIStatL('/vsimem/isis_tmp.cub') is None:
        gdaltest.post_reason('fail')
        return 'fail'
    f = gdal.VSIFOpenL('/vsimem/isis_tmp.lbl', 'rb')
    content = gdal.VSIFReadL(1, 10000, f).decode('ASCII')
    gdal.VSIFCloseL(f)
    if content.find('#my comment') < 0:
        gdaltest.post_reason('fail')
        print(content)
        return 'fail'
    if len(content) == 10000:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.Open('/vsimem/isis_tmp.lbl', gdal.GA_Update)
    ds.GetRasterBand(1).Fill(0)
    ds = None
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    if ds.GetRasterBand(1).Checksum() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')
    return 'success'

# Uncompressed GeoTIFF creation
def isis_7():

    tst = gdaltest.GDALTest( 'ISIS3', 'isis3_detached.lbl', 1, 9978,
                            options = ['DATA_LOCATION=GEOTIFF',
                                       'USE_SRC_LABEL=NO'] )
    ret = tst.testCreateCopy( new_filename = '/vsimem/isis_tmp.lbl',
                             delete_copy = 0 )
    if ret != 'success':
        return ret
    if gdal.VSIStatL('/vsimem/isis_tmp.tif') is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    lbl = ds.GetMetadata_List('json:ISIS3')[0]
    if lbl.find('"Format":"BandSequential"') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')

    # Test GEOTIFF_AS_REGULAR_EXTERNAL = NO
    tst = gdaltest.GDALTest( 'ISIS3', 'isis3_detached.lbl', 1, 9978,
                            options = ['DATA_LOCATION=GEOTIFF',
                                       'GEOTIFF_AS_REGULAR_EXTERNAL=NO',
                                       'USE_SRC_LABEL=NO'] )
    ret = tst.testCreateCopy( new_filename = '/vsimem/isis_tmp.lbl',
                             delete_copy = 0 )
    if ret != 'success':
        return ret
    if gdal.VSIStatL('/vsimem/isis_tmp.tif') is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    lbl = ds.GetMetadata_List('json:ISIS3')[0]
    if lbl.find('"Format":"GeoTIFF"') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')

    return 'success'

# Compressed GeoTIFF creation
def isis_8():

    tst = gdaltest.GDALTest( 'ISIS3', 'isis3_detached.lbl', 1, 9978,
                            options = ['DATA_LOCATION=GEOTIFF',
                                       'USE_SRC_LABEL=NO',
                                       'GEOTIFF_OPTIONS=COMPRESS=LZW'] )
    ret = tst.testCreateCopy( new_filename = '/vsimem/isis_tmp.lbl',
                             delete_copy = 0 )
    if ret != 'success':
        return ret
    if gdal.VSIStatL('/vsimem/isis_tmp.tif') is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    lbl = ds.GetMetadata_List('json:ISIS3')[0]
    if lbl.find('"Format":"GeoTIFF"') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    ds = None
    ds = gdal.Open('/vsimem/isis_tmp.lbl', gdal.GA_Update)
    ds.GetRasterBand(1).Fill(0)
    ds = None
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    if ds.GetRasterBand(1).Checksum() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')
    return 'success'

# Tiled creation + EXTERNAL_FILENAME
def isis_9():

    tst = gdaltest.GDALTest( 'ISIS3', 'isis3_detached.lbl', 1, 9978,
                            options = ['DATA_LOCATION=EXTERNAL',
                                       'USE_SRC_LABEL=NO',
                                       'TILED=YES',
                                       'EXTERNAL_FILENAME=/vsimem/foo.bin'] )
    ret = tst.testCreateCopy( new_filename = '/vsimem/isis_tmp.lbl',
                             delete_copy = 0 )
    if ret != 'success':
        return ret
    if gdal.VSIStatL('/vsimem/foo.bin') is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    lbl = ds.GetMetadata_List('json:ISIS3')[0]
    if lbl.find('"Format":"Tile"') < 0 or lbl.find('"TileSamples":256') < 0 or \
       lbl.find('"TileLines":256') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')
    if gdal.VSIStatL('/vsimem/foo.bin') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    return 'success'

# Tiled creation + regular GeoTIFF + EXTERNAL_FILENAME
def isis_10():

    tst = gdaltest.GDALTest( 'ISIS3', 'isis3_detached.lbl', 1, 9978,
                            options = ['USE_SRC_LABEL=NO',
                                       'DATA_LOCATION=GEOTIFF',
                                       'TILED=YES',
                                       'BLOCKXSIZE=16', 'BLOCKYSIZE=32',
                                       'EXTERNAL_FILENAME=/vsimem/foo.tif'] )
    ret = tst.testCreateCopy( new_filename = '/vsimem/isis_tmp.lbl',
                             delete_copy = 0 )
    if ret != 'success':
        return ret
    ds = gdal.Open('/vsimem/foo.tif')
    if ds.GetRasterBand(1).GetBlockSize() != [16,32]:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetBlockSize())
        return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')
    if gdal.VSIStatL('/vsimem/foo.tif') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    return 'success'

# Tiled creation + compressed GeoTIFF
def isis_11():

    tst = gdaltest.GDALTest( 'ISIS3', 'isis3_detached.lbl', 1, 9978,
                            options = ['USE_SRC_LABEL=NO',
                                       'DATA_LOCATION=GEOTIFF',
                                       'TILED=YES',
                                       'GEOTIFF_OPTIONS=COMPRESS=LZW'] )
    ret = tst.testCreateCopy( new_filename = '/vsimem/isis_tmp.lbl',
                             delete_copy = 0 )
    if ret != 'success':
        return ret
    ds = gdal.Open('/vsimem/isis_tmp.tif')
    if ds.GetRasterBand(1).GetBlockSize() != [256,256]:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetBlockSize())
        return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')
    return 'success'

# Multiband
def isis_12():

    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    gdal.Translate('/vsimem/isis_tmp.lbl', src_ds, format = 'ISIS3')
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    for i in range(4):
        cs = ds.GetRasterBand(i+1).Checksum()
        expected_cs = src_ds.GetRasterBand(i+1).Checksum()
        if cs != expected_cs:
            gdaltest.post_reason('fail')
            print(i+1, cs, expected_cs)
            return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')
    return 'success'

# Multiband tiled
def isis_13():

    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    gdal.Translate('/vsimem/isis_tmp.lbl', src_ds, format = 'ISIS3',
                   creationOptions = ['TILED=YES', 'BLOCKXSIZE=16',
                                      'BLOCKYSIZE=32'])
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    for i in range(4):
        cs = ds.GetRasterBand(i+1).Checksum()
        expected_cs = src_ds.GetRasterBand(i+1).Checksum()
        if cs != expected_cs:
            gdaltest.post_reason('fail')
            print(i+1, cs, expected_cs)
            return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')
    return 'success'

# Multiband with uncompressed GeoTIFF
def isis_14():

    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    gdal.Translate('/vsimem/isis_tmp.lbl', src_ds, format = 'ISIS3',
                   creationOptions = ['DATA_LOCATION=GEOTIFF'])
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    for i in range(4):
        cs = ds.GetRasterBand(i+1).Checksum()
        expected_cs = src_ds.GetRasterBand(i+1).Checksum()
        if cs != expected_cs:
            gdaltest.post_reason('fail')
            print(i+1, cs, expected_cs)
            return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')
    return 'success'

# Multiband with uncompressed tiled GeoTIFF
def isis_15():

    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    gdal.Translate('/vsimem/isis_tmp.lbl', src_ds, format = 'ISIS3',
                   creationOptions = ['DATA_LOCATION=GEOTIFF', 'TILED=YES',
                                      'BLOCKXSIZE=16', 'BLOCKYSIZE=32'])
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    for i in range(4):
        cs = ds.GetRasterBand(i+1).Checksum()
        expected_cs = src_ds.GetRasterBand(i+1).Checksum()
        if cs != expected_cs:
            gdaltest.post_reason('fail')
            print(i+1, cs, expected_cs)
            return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')
    return 'success'

# Test Create() without anything else
def isis_16():

    for read_before_write in [ False, True ]:
        for init_nd in [ False, True ]:
            for dt, cs, nd, options in [
                                    [gdal.GDT_Byte, 0, 0, []],
                                    [gdal.GDT_Byte, 0, 0, ['TILED=YES']],
                                    [gdal.GDT_Byte, 0, 0, ['DATA_LOCATION=GEOTIFF', 'GEOTIFF_OPTIONS=COMPRESS=LZW']],
                                    [gdal.GDT_Int16, 65525, -32768, []],
                                    [gdal.GDT_UInt16, 0, 0, []],
                                    [gdal.GDT_Float32, 65534, -3.4028226550889045e+38, []]
            ]:

                ds = gdal.GetDriverByName('ISIS3').Create('/vsimem/isis_tmp.lbl',
                                                        1, 2, 1, dt, options = options)
                ds.GetRasterBand(1).SetOffset(10)
                ds.GetRasterBand(1).SetScale(20)
                if read_before_write:
                    ds.GetRasterBand(1).ReadRaster()
                if init_nd:
                    ds.GetRasterBand(1).Fill(nd)
                ds = None
                ds = gdal.Open('/vsimem/isis_tmp.lbl')
                if ds.GetRasterBand(1).Checksum() != cs:
                    gdaltest.post_reason('fail')
                    print(dt, cs, nd, options, init_nd, ds.GetRasterBand(1).Checksum())
                    return 'fail'
                if ds.GetRasterBand(1).GetMaskFlags() != 0:
                    gdaltest.post_reason('fail')
                    print(dt, cs, nd, options, init_nd, ds.GetRasterBand(1).GetMaskFlags())
                    return 'fail'
                if ds.GetRasterBand(1).GetMaskBand().Checksum() != 0:
                    gdaltest.post_reason('fail')
                    print(dt, cs, nd, options, init_nd, ds.GetRasterBand(1).GetMaskBand().Checksum())
                    return 'fail'
                if ds.GetRasterBand(1).GetOffset() != 10:
                    gdaltest.post_reason('fail')
                    print(dt, cs, nd, options, init_nd, ds.GetRasterBand(1).GetOffset())
                    return 'fail'
                if ds.GetRasterBand(1).GetScale() != 20:
                    gdaltest.post_reason('fail')
                    print(dt, cs, nd, options, init_nd, ds.GetRasterBand(1).GetScale())
                    return 'fail'
                if ds.GetRasterBand(1).GetNoDataValue() != nd:
                    gdaltest.post_reason('fail')
                    print(dt, cs, nd, options, init_nd, ds.GetRasterBand(1).GetNoDataValue())
                    return 'fail'
                ds = None
                gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')

    return 'success'

# Test create copy through Create()
def isis_17():

    tst = gdaltest.GDALTest( 'ISIS3', 'isis3_detached.lbl', 1, 9978 )
    return tst.testCreate( vsimem = 1 )

# Test SRS serialization and deserialization
def isis_18():

    sr = osr.SpatialReference()
    sr.SetEquirectangular2(0,1,2,0,0)
    sr.SetGeogCS( "GEOG_NAME", "D_DATUM_NAME", "", 123456, 200 )
    ds = gdal.GetDriverByName('ISIS3').Create('/vsimem/isis_tmp.lbl', 1, 1)
    ds.SetProjection(sr.ExportToWkt())
    ds = None
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    wkt = ds.GetProjectionRef()
    ds = None
    if not osr.SpatialReference(wkt).IsSame(osr.SpatialReference('PROJCS["Equirectangular DATUM_NAME",GEOGCS["GCS_DATUM_NAME",DATUM["D_DATUM_NAME",SPHEROID["DATUM_NAME_localRadius",123455.2424988797,0]],PRIMEM["Reference_Meridian",0],UNIT["degree",0.0174532925199433]],PROJECTION["Equirectangular"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",1],PARAMETER["standard_parallel_1",2],PARAMETER["false_easting",0],PARAMETER["false_northing",0]]')):
        gdaltest.post_reason('fail')
        print(wkt)
        return 'fail'

    sr = osr.SpatialReference()
    sr.SetEquirectangular2(123456,1,2,987654,3210123)
    sr.SetGeogCS( "GEOG_NAME", "D_DATUM_NAME", "", 123456, 200 )
    ds = gdal.GetDriverByName('ISIS3').Create('/vsimem/isis_tmp.lbl', 1, 1)
    ds.SetProjection(sr.ExportToWkt())
    gdal.PushErrorHandler()
    # Will warn that latitude_of_origin, false_easting and false_northing are ignored
    ds = None
    gdal.PopErrorHandler()
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    wkt = ds.GetProjectionRef()
    ds = None
    if not osr.SpatialReference(wkt).IsSame(osr.SpatialReference('PROJCS["Equirectangular DATUM_NAME",GEOGCS["GCS_DATUM_NAME",DATUM["D_DATUM_NAME",SPHEROID["DATUM_NAME_localRadius",123455.2424988797,0]],PRIMEM["Reference_Meridian",0],UNIT["degree",0.0174532925199433]],PROJECTION["Equirectangular"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",1],PARAMETER["standard_parallel_1",2],PARAMETER["false_easting",0],PARAMETER["false_northing",0]]')):
        gdaltest.post_reason('fail')
        print(wkt)
        return 'fail'

    sr = osr.SpatialReference()
    sr.SetOrthographic(1,2,0,0)
    sr.SetGeogCS( "GEOG_NAME", "D_DATUM_NAME", "", 123456, 200 )
    ds = gdal.GetDriverByName('ISIS3').Create('/vsimem/isis_tmp.lbl', 1, 1)
    ds.SetProjection(sr.ExportToWkt())
    ds = None
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    wkt = ds.GetProjectionRef()
    ds = None
    if not osr.SpatialReference(wkt).IsSame(osr.SpatialReference('PROJCS["Orthographic DATUM_NAME",GEOGCS["GCS_DATUM_NAME",DATUM["D_DATUM_NAME",SPHEROID["DATUM_NAME",123456,0]],PRIMEM["Reference_Meridian",0],UNIT["degree",0.0174532925199433]],PROJECTION["Orthographic"],PARAMETER["latitude_of_origin",1],PARAMETER["central_meridian",2],PARAMETER["false_easting",0],PARAMETER["false_northing",0]]')):
        gdaltest.post_reason('fail')
        print(wkt)
        return 'fail'

    sr = osr.SpatialReference()
    sr.SetSinusoidal(1,0,0)
    sr.SetGeogCS( "GEOG_NAME", "D_DATUM_NAME", "", 123456, 200 )
    ds = gdal.GetDriverByName('ISIS3').Create('/vsimem/isis_tmp.lbl', 1, 1)
    ds.SetProjection(sr.ExportToWkt())
    ds = None
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    wkt = ds.GetProjectionRef()
    ds = None
    if not osr.SpatialReference(wkt).IsSame(osr.SpatialReference('PROJCS["Sinusoidal DATUM_NAME",GEOGCS["GCS_DATUM_NAME",DATUM["D_DATUM_NAME",SPHEROID["DATUM_NAME",123456,0]],PRIMEM["Reference_Meridian",0],UNIT["degree",0.0174532925199433]],PROJECTION["Sinusoidal"],PARAMETER["longitude_of_center",1],PARAMETER["false_easting",0],PARAMETER["false_northing",0]]')):
        gdaltest.post_reason('fail')
        print(wkt)
        return 'fail'

    sr = osr.SpatialReference()
    sr.SetMercator(1,2,0.9,0,0)
    sr.SetGeogCS( "GEOG_NAME", "D_DATUM_NAME", "", 123456, 200 )
    ds = gdal.GetDriverByName('ISIS3').Create('/vsimem/isis_tmp.lbl', 1, 1)
    ds.SetProjection(sr.ExportToWkt())
    ds = None
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    wkt = ds.GetProjectionRef()
    ds = None
    if not osr.SpatialReference(wkt).IsSame(osr.SpatialReference('PROJCS["Mercator DATUM_NAME",GEOGCS["GCS_DATUM_NAME",DATUM["D_DATUM_NAME",SPHEROID["DATUM_NAME",123456,0]],PRIMEM["Reference_Meridian",0],UNIT["degree",0.0174532925199433]],PROJECTION["Mercator_1SP"],PARAMETER["latitude_of_origin",1],PARAMETER["central_meridian",2],PARAMETER["scale_factor",0.9],PARAMETER["false_easting",0],PARAMETER["false_northing",0]]')):
        gdaltest.post_reason('fail')
        print(wkt)
        return 'fail'

    sr = osr.SpatialReference()
    sr.SetPS(1,2,0.9,0,0)
    sr.SetGeogCS( "GEOG_NAME", "D_DATUM_NAME", "", 123456, 200 )
    ds = gdal.GetDriverByName('ISIS3').Create('/vsimem/isis_tmp.lbl', 1, 1)
    ds.SetProjection(sr.ExportToWkt())
    ds = None
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    wkt = ds.GetProjectionRef()
    ds = None
    if not osr.SpatialReference(wkt).IsSame(osr.SpatialReference('PROJCS["PolarStereographic DATUM_NAME",GEOGCS["GCS_DATUM_NAME",DATUM["D_DATUM_NAME",SPHEROID["DATUM_NAME_polarRadius",122838.72,0]],PRIMEM["Reference_Meridian",0],UNIT["degree",0.0174532925199433]],PROJECTION["Polar_Stereographic"],PARAMETER["latitude_of_origin",1],PARAMETER["central_meridian",2],PARAMETER["scale_factor",0.9],PARAMETER["false_easting",0],PARAMETER["false_northing",0]]')):
        gdaltest.post_reason('fail')
        print(wkt)
        return 'fail'

    sr = osr.SpatialReference()
    sr.SetTM(1,2,0.9,0,0)
    sr.SetGeogCS( "GEOG_NAME", "D_DATUM_NAME", "", 123456, 200 )
    ds = gdal.GetDriverByName('ISIS3').Create('/vsimem/isis_tmp.lbl', 1, 1)
    ds.SetProjection(sr.ExportToWkt())
    ds = None
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    wkt = ds.GetProjectionRef()
    ds = None
    if not osr.SpatialReference(wkt).IsSame(osr.SpatialReference('PROJCS["TransverseMercator DATUM_NAME",GEOGCS["GCS_DATUM_NAME",DATUM["D_DATUM_NAME",SPHEROID["DATUM_NAME",123456,0]],PRIMEM["Reference_Meridian",0],UNIT["degree",0.0174532925199433]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",1],PARAMETER["central_meridian",2],PARAMETER["scale_factor",0.9],PARAMETER["false_easting",0],PARAMETER["false_northing",0]]')):
        gdaltest.post_reason('fail')
        print(wkt)
        return 'fail'

    sr = osr.SpatialReference()
    sr.SetLCC(1,2,3,4,0,0)
    sr.SetGeogCS( "GEOG_NAME", "D_DATUM_NAME", "", 123456, 200 )
    ds = gdal.GetDriverByName('ISIS3').Create('/vsimem/isis_tmp.lbl', 1, 1)
    ds.SetProjection(sr.ExportToWkt())
    ds = None
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    wkt = ds.GetProjectionRef()
    ds = None
    if not osr.SpatialReference(wkt).IsSame(osr.SpatialReference('PROJCS["LambertConformal DATUM_NAME",GEOGCS["GCS_DATUM_NAME",DATUM["D_DATUM_NAME",SPHEROID["DATUM_NAME",123456,0]],PRIMEM["Reference_Meridian",0],UNIT["degree",0.0174532925199433]],PROJECTION["Lambert_Conformal_Conic_2SP"],PARAMETER["standard_parallel_1",1],PARAMETER["standard_parallel_2",2],PARAMETER["latitude_of_origin",3],PARAMETER["central_meridian",4],PARAMETER["false_easting",0],PARAMETER["false_northing",0]]')):
        gdaltest.post_reason('fail')
        print(wkt)
        return 'fail'



    sr = osr.SpatialReference()
    sr.SetEquirectangular2(0,1,2,0,0)
    sr.SetGeogCS( "GEOG_NAME", "D_DATUM_NAME", "", 123456, 200 )
    ds = gdal.GetDriverByName('ISIS3').Create('/vsimem/isis_tmp.lbl', 1, 1,
                                options = ['LATITUDE_TYPE=Planetographic',
                                           'TARGET_NAME=my_target',
                                           'BOUNDING_DEGREES=1.5,2.5,3.5,4.5'])
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform( [1000,1,0,2000,0,-1] )
    ds = None
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    lbl = ds.GetMetadata_List('json:ISIS3')[0]
    if lbl.find('"TargetName":"my_target"') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if lbl.find('"LatitudeType":"Planetographic"') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if lbl.find('"MinimumLatitude":2.5') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if lbl.find('"MinimumLongitude":1.5') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if lbl.find('"MaximumLatitude":4.5') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if lbl.find('"MaximumLongitude":3.5') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    ds = None


    sr = osr.SpatialReference()
    sr.SetGeogCS( "GEOG_NAME", "D_DATUM_NAME", "", 123456, 200 )
    ds = gdal.GetDriverByName('ISIS3').Create('/vsimem/isis_tmp.lbl', 100, 100,
                                options = ['LONGITUDE_DIRECTION=PositiveWest'])
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform( [10,1,0,40,0,-1] )
    ds = None
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    lbl = ds.GetMetadata_List('json:ISIS3')[0]
    if lbl.find('"LongitudeDirection":"PositiveWest"') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if lbl.find('"LongitudeDomain":180') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if lbl.find('"MinimumLatitude":-60') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if lbl.find('"MinimumLongitude":-110') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if lbl.find('"MaximumLatitude":40') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if lbl.find('"MaximumLongitude":-10') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    ds = None


    sr = osr.SpatialReference()
    sr.SetGeogCS( "GEOG_NAME", "D_DATUM_NAME", "", 123456, 200 )
    ds = gdal.GetDriverByName('ISIS3').Create('/vsimem/isis_tmp.lbl', 100, 100,
                                options = ['FORCE_360=YES'])
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform( [-10,1,0,40,0,-1] )
    ds = None
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    lbl = ds.GetMetadata_List('json:ISIS3')[0]
    if lbl.find('"MinimumLatitude":-60') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if lbl.find('"MinimumLongitude":90') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if lbl.find('"MaximumLatitude":40') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if lbl.find('"MaximumLongitude":350') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if lbl.find('"UpperLeftCornerX":-21547') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if lbl.find('"UpperLeftCornerY":86188') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    ds = None

    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')

    return 'success'

# Test gdal.Info() with json:ISIS3 metadata domain
def isis_19():

    ds = gdal.Open('data/isis3_detached.lbl')
    res = gdal.Info(ds, format = 'json', extraMDDomains = ['json:ISIS3'])
    if res['metadata']['json:ISIS3']['IsisCube']['_type'] != 'object':
        gdaltest.post_reason('fail')
        print(res)
        return 'fail'

    ds = gdal.Open('data/isis3_detached.lbl')
    res = gdal.Info(ds, extraMDDomains = ['json:ISIS3'])
    if res.find('IsisCube') < 0:
        gdaltest.post_reason('fail')
        print(res)
        return 'fail'

    return 'success'

# Test gdal.Translate() subsetting and label preservation
def isis_20():

    with gdaltest.error_handler():
        gdal.Translate( '/vsimem/isis_tmp.lbl', 'data/isis3_detached.lbl',
                        format = 'ISIS3', srcWin = [ 0, 0, 1, 1 ] )
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    lbl = ds.GetMetadata_List('json:ISIS3')[0]
    if lbl.find('AMadeUpValue') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')

    return 'success'

# Test gdal.Warp() and label preservation
def isis_21():

    with gdaltest.error_handler():
        gdal.Warp( '/vsimem/isis_tmp.lbl', 'data/isis3_detached.lbl',
                   format = 'ISIS3' )
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    if ds.GetRasterBand(1).Checksum() != 9978:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'
    lbl = ds.GetMetadata_List('json:ISIS3')[0]
    if lbl.find('AMadeUpValue') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')

    return 'success'

# Test source JSon use
def isis_22():

    ds = gdal.GetDriverByName('ISIS3').Create('/vsimem/isis_tmp.lbl', 1, 1)
    # Invalid Json
    js = """invalid"""
    with gdaltest.error_handler():
        if ds.SetMetadata( [js], 'json:ISIS3') == 0:
            gdaltest.post_reason('fail')
            return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')

    ds = gdal.GetDriverByName('ISIS3').Create('/vsimem/isis_tmp.lbl', 1, 1)
    # Invalid type for IsisCube
    js = """{ "IsisCube": 5 }"""
    ds.SetMetadata( [js], 'json:ISIS3')
    lbl = ds.GetMetadata_List('json:ISIS3')
    if lbl is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.SetMetadata( [js], 'json:ISIS3')
    ds = None
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')

    ds = gdal.GetDriverByName('ISIS3').Create('/vsimem/isis_tmp.lbl', 1, 1)
    # Invalid type for IsisCube.Core
    js = """{ "IsisCube": { "_type": "object", "Core": 5 } }"""
    ds.SetMetadata( [js], 'json:ISIS3')
    ds = None
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')

    ds = gdal.GetDriverByName('ISIS3').Create('/vsimem/isis_tmp.lbl', 1, 1)
    # Invalid type for IsisCube.Core.Dimensions and IsisCube.Core.Pixels
    js = """{ "IsisCube": { "_type": "object", "Core": { "_type": "object",
                                        "Dimensions": 5, "Pixels": 5 } } }"""
    ds.SetMetadata( [js], 'json:ISIS3')
    ds = None
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')

    ds = gdal.GetDriverByName('ISIS3').Create('/vsimem/isis_tmp.lbl', 1, 1,
                                              options = ['DATA_LOCATION=EXTERNAL'] )
    js = """{ "IsisCube": { "foo": "bar", "bar": [ 123, 124.0, 2.5, "xyz", "anotherveeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeerylooooongtext",
    234, 456, 789, 234, 567, 890, 123456789.0, 123456789.0, 123456789.0, 123456789.0, 123456789.0 ],
                         "baz" : { "value": 5, "unit": "M" }, "baw": "with space",
                         "very_long": "aveeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeerylooooongtext"} }"""
    ds.SetMetadata( [js], 'json:ISIS3')
    ds = None

    f = gdal.VSIFOpenL('/vsimem/isis_tmp.lbl', 'rb')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    content = gdal.VSIFReadL(1, 10000, f).decode('ASCII')
    gdal.VSIFCloseL(f)

    if content.find('foo       = bar') < 0 or \
       content.find('  bar       = (123, 124.0, 2.5, xyz') < 0 or \
       content.find('               anotherveeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee-') < 0 or \
       content.find('               eeeeeeeeeeeeeeeeeeeeeeeerylooooongtext, 234, 456, 789, 234, 567,') < 0 or \
       content.find('               890, 123456789.0, 123456789.0, 123456789.0, 123456789.0,') < 0 or \
       content.find('               123456789.0)') < 0 or \
       content.find('baz       = 5 <M>') < 0 or \
       content.find('baw       = "with space"') < 0 or \
       content.find('very_long = aveeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee-') < 0:
        gdaltest.post_reason('fail')
        print(content)
        return 'fail'

    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    lbl = ds.GetMetadata_List('json:ISIS3')[0]
    if lbl.find('"foo":"bar"') < 0 or lbl.find('123') < 0 or \
       lbl.find('2.5') < 0 or lbl.find('xyz') < 0 or \
       lbl.find('"value":5') < 0 or lbl.find('"unit":"M"') < 0 or \
           lbl.find('"baw":"with space"') < 0 or \
       lbl.find('"very_long":"aveeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeerylooooongtext"') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')

    return 'success'

# Test nodata remapping
def isis_23():

    mem_ds = gdal.Translate('', 'data/byte.tif', format = 'MEM')
    mem_ds.SetProjection('')
    mem_ds.SetGeoTransform([0,1,0,0,0,1])
    mem_ds.GetRasterBand(1).SetNoDataValue(74)
    ref_data = mem_ds.GetRasterBand(1).ReadRaster()
    gdal.Translate('/vsimem/isis_tmp.lbl', mem_ds,
                   format = 'ISIS3')
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    if ref_data == ds.GetRasterBand(1).ReadRaster():
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')

    gdal.Translate('/vsimem/isis_tmp.lbl', mem_ds,
                   format = 'ISIS3', creationOptions = [ 'DATA_LOCATION=GeoTIFF'] )
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    if ref_data == ds.GetRasterBand(1).ReadRaster():
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')

    gdal.Translate('/vsimem/isis_tmp.lbl', mem_ds,
                   format = 'ISIS3', creationOptions = [ 'TILED=YES'] )
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    if ref_data == ds.GetRasterBand(1).ReadRaster():
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')


    for dt in [ gdal.GDT_Int16, gdal.GDT_UInt16, gdal.GDT_Float32 ]:
        mem_ds = gdal.Translate('', 'data/byte.tif', format = 'MEM', outputType = dt)
        mem_ds.SetProjection('')
        mem_ds.SetGeoTransform([0,1,0,0,0,1])
        mem_ds.GetRasterBand(1).SetNoDataValue(74)
        ref_data = mem_ds.GetRasterBand(1).ReadRaster()
        gdal.Translate('/vsimem/isis_tmp.lbl', mem_ds,
                    format = 'ISIS3')
        ds = gdal.Open('/vsimem/isis_tmp.lbl')
        if ref_data == ds.GetRasterBand(1).ReadRaster():
            gdaltest.post_reason('fail')
            return 'fail'
        ds = None
        gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')

    return 'success'

def cancel_cbk(pct, msg, user_data):
    return 0

# Test error cases
def isis_24():

    # For DATA_LOCATION=EXTERNAL, the main filename should have a .lbl extension
    with gdaltest.error_handler():
        ds = gdal.GetDriverByName('ISIS3').Create('/vsimem/error.txt', 1, 1,
                                        options = ['DATA_LOCATION=EXTERNAL'])
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # cannot create external filename
    with gdaltest.error_handler():
        ds = gdal.GetDriverByName('ISIS3').Create('/vsimem/error.lbl', 1, 1,
            options = ['DATA_LOCATION=EXTERNAL',
                       'EXTERNAL_FILENAME=/i_dont/exist/error.cub'])
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # no GTiff driver
    #with gdaltest.error_handler():
    #    gtiff_drv = gdal.GetDriverByName('GTiff')
    #    gtiff_drv.Deregister()
    #    ds = gdal.GetDriverByName('ISIS3').Create('/vsimem/error.lbl', 1, 1,
    #        options = ['DATA_LOCATION=GEOTIFF' ])
    #    gtiff_drv.Register()
    #if ds is not None:
    #    gdaltest.post_reason('fail')
    #    return 'fail'

    # cannot create GeoTIFF
    with gdaltest.error_handler():
        ds = gdal.GetDriverByName('ISIS3').Create('/vsimem/error.lbl', 1, 1,
            options = ['DATA_LOCATION=GEOTIFF',
                       'EXTERNAL_FILENAME=/i_dont/exist/error.tif'])
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Output file has same name as input file
    src_ds = gdal.Translate('/vsimem/out.tif', 'data/byte.tif')
    with gdaltest.error_handler():
        ds = gdal.GetDriverByName('ISIS3').CreateCopy('/vsimem/out.lbl',
                                        src_ds,
                                        options = ['DATA_LOCATION=GEOTIFF' ])
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink('/vsimem/out.tif')

    # Missing /vsimem/out.cub
    src_ds = gdal.Open('data/byte.tif')
    with gdaltest.error_handler():
        gdal.GetDriverByName('ISIS3').CreateCopy('/vsimem/out.lbl',
                                        src_ds,
                                        options = ['DATA_LOCATION=EXTERNAL' ])
    gdal.Unlink('/vsimem/out.cub')
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/out.lbl')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/out.lbl', gdal.GA_Update)
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    # Delete would fail since ds is None
    gdal.Unlink('/vsimem/out.lbl')

    # Missing /vsimem/out.tif
    src_ds = gdal.Open('data/byte.tif')
    with gdaltest.error_handler():
        gdal.GetDriverByName('ISIS3').CreateCopy('/vsimem/out.lbl',
                                        src_ds,
                                        options = ['DATA_LOCATION=GEOTIFF',
                                            'GEOTIFF_OPTIONS=COMPRESS=LZW'])
    gdal.Unlink('/vsimem/out.tif')
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/out.lbl')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    # Delete would fail since ds is None
    gdal.Unlink('/vsimem/out.lbl')

    # Invalid StartByte
    gdal.GetDriverByName('ISIS3').Create('/vsimem/out.lbl', 1, 1,
                                         options = ['DATA_LOCATION=GEOTIFF',
                                               'GEOTIFF_OPTIONS=COMPRESS=LZW'])
    gdal.FileFromMemBuffer('/vsimem/out.lbl', """Object = IsisCube
  Object = Core
    StartByte = 2
    Format = GeoTIFF
    ^Core = out.tif
    Group = Dimensions
      Samples = 1
      Lines   = 1
      Bands   = 1
    End_Group
    Group = Pixels
      Type       = UnsignedByte
      ByteOrder  = Lsb
      Base       = 0.0
      Multiplier = 1.0
    End_Group
  End_Object
End_Object
End""")
    with gdaltest.error_handler():
        gdal.Open('/vsimem/out.lbl')
    gdal.Unlink('/vsimem/out.tif')
    with gdaltest.error_handler():
        gdal.GetDriverByName('ISIS3').Delete('/vsimem/out.lbl')

    gdal.FileFromMemBuffer('/vsimem/out.lbl', 'IsisCube')
    if gdal.IdentifyDriver('/vsimem/out.lbl') is None:
        gdaltest.post_reason('fail')
        return 'fail'
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/out.lbl')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    # Delete would fail since ds is None
    gdal.Unlink('/vsimem/out.lbl')


    gdal.FileFromMemBuffer('/vsimem/out.lbl', """Object = IsisCube
  Object = Core
    Format = Tile
    Group = Dimensions
      Samples = 1
      Lines   = 1
      Bands   = 1
    End_Group
    Group = Pixels
      Type       = Real
      ByteOrder  = Lsb
      Base       = 0.0
      Multiplier = 1.0
    End_Group
  End_Object
End_Object
End""")
    # Wrong tile dimensions : 0 x 0
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/out.lbl')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    # Delete would fail since ds is None
    gdal.Unlink('/vsimem/out.lbl')


    gdal.FileFromMemBuffer('/vsimem/out.lbl', """Object = IsisCube
  Object = Core
    Format = BandSequential
    Group = Dimensions
      Samples = 0
      Lines   = 0
      Bands   = 0
    End_Group
    Group = Pixels
      Type       = Real
      ByteOrder  = Lsb
      Base       = 0.0
      Multiplier = 1.0
    End_Group
  End_Object
End_Object
End""")
    # Invalid dataset dimensions : 0 x 0
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/out.lbl')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    # Delete would fail since ds is None
    gdal.Unlink('/vsimem/out.lbl')

    gdal.FileFromMemBuffer('/vsimem/out.lbl', """Object = IsisCube
  Object = Core
    Format = BandSequential
    Group = Dimensions
      Samples = 1
      Lines   = 1
      Bands   = 0
    End_Group
    Group = Pixels
      Type       = Real
      ByteOrder  = Lsb
      Base       = 0.0
      Multiplier = 1.0
    End_Group
  End_Object
End_Object
End""")
    # Invalid band count : 0
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/out.lbl')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    # Delete would fail since ds is None
    gdal.Unlink('/vsimem/out.lbl')

    gdal.FileFromMemBuffer('/vsimem/out.lbl', """Object = IsisCube
  Object = Core
    Format = BandSequential
    Group = Dimensions
      Samples = 1
      Lines   = 1
      Bands   = 1
    End_Group
    Group = Pixels
      Type       = unhandled
      ByteOrder  = Lsb
      Base       = 0.0
      Multiplier = 1.0
    End_Group
  End_Object
End_Object
End""")

    # unhandled format
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/out.lbl')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    # Delete would fail since ds is None
    gdal.Unlink('/vsimem/out.lbl')

    gdal.FileFromMemBuffer('/vsimem/out.lbl', """Object = IsisCube
  Object = Core
    Format = unhandled
    Group = Dimensions
      Samples = 1
      Lines   = 1
      Bands   = 1
    End_Group
    Group = Pixels
      Type       = UnsignedByte
      ByteOrder  = Lsb
      Base       = 0.0
      Multiplier = 1.0
    End_Group
  End_Object
End_Object
End""")

    # bad PDL formatting
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/out.lbl')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    # Delete would fail since ds is None
    gdal.Unlink('/vsimem/out.lbl')

    gdal.FileFromMemBuffer('/vsimem/out.lbl', """Object = IsisCube
  Object = Core
    Format = BandSequential
    Group = Dimensions
      Samples = 1
      Lines   = 1
      Bands   = 1
    End_Group
  End_Object
End_Object
End""")

    # missing Group = Pixels. This is actually valid. Assuming Real
    ds = gdal.Open('/vsimem/out.lbl')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    # Delete would fail since ds is None
    gdal.Unlink('/vsimem/out.lbl')

    gdal.GetDriverByName('ISIS3').Create('/vsimem/out.lbl', 1, 1,
                                         options = ['DATA_LOCATION=GEOTIFF',
                                            'GEOTIFF_OPTIONS=COMPRESS=LZW'])
    # /vsimem/out.tif has incompatible characteristics with the ones declared in the label
    gdal.GetDriverByName('GTiff').Create('/vsimem/out.tif', 1, 2)
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/out.lbl')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.GetDriverByName('GTiff').Create('/vsimem/out.tif', 2, 1)
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/out.lbl')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.GetDriverByName('GTiff').Create('/vsimem/out.tif', 1, 1, 2)
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/out.lbl')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.GetDriverByName('GTiff').Create('/vsimem/out.tif', 1, 1, 1, gdal.GDT_Int16)
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/out.lbl')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    # Delete would fail since ds is None
    gdal.Unlink('/vsimem/out.lbl')
    gdal.Unlink('/vsimem/out.tif')


    gdal.GetDriverByName('ISIS3').Create('/vsimem/out.lbl', 1, 1,
                                         options = ['DATA_LOCATION=GEOTIFF'])
    # /vsimem/out.tif has incompatible characteristics with the ones declared in the label
    gdal.GetDriverByName('GTiff').Create('/vsimem/out.tif', 1, 2)
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/out.lbl')
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.GetDriverByName('GTiff').Create('/vsimem/out.tif', 2, 1)
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/out.lbl')
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.GetDriverByName('GTiff').Create('/vsimem/out.tif', 1, 1, 2)
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/out.lbl')
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.GetDriverByName('GTiff').Create('/vsimem/out.tif', 1, 1, 1, gdal.GDT_Int16)
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/out.lbl')
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.GetDriverByName('GTiff').Create('/vsimem/out.tif', 1, 1, options = ['COMPRESS=LZW'])
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/out.lbl')
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.GetDriverByName('GTiff').Create('/vsimem/out.tif', 1, 1, options = ['TILED=YES'])
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/out.lbl')
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/out.tif', 1, 1)
    ds.GetRasterBand(1).SetNoDataValue(0)
    ds.SetMetadataItem('foo', 'bar')
    ds = None
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/out.lbl')
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'
    with gdaltest.error_handler():
        gdal.GetDriverByName('ISIS3').Delete('/vsimem/out.lbl')
    gdal.Unlink('/vsimem/out.tif')

    mem_ds = gdal.GetDriverByName('MEM').Create('',1,1)
    with gdaltest.error_handler():
        ds = gdal.GetDriverByName('ISIS3').CreateCopy('/vsimem/out.lbl',
                                                      mem_ds,
                                                      callback = cancel_cbk)
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    # Delete would fail since ds is None
    gdal.Unlink('/vsimem/out.lbl')

    return 'success'

# Test CreateCopy() and scale and offset
def isis_25():

    mem_ds = gdal.GetDriverByName('MEM').Create('',1,1,1)
    mem_ds.GetRasterBand(1).SetScale(10)
    mem_ds.GetRasterBand(1).SetOffset(20)
    gdal.GetDriverByName('ISIS3').CreateCopy('/vsimem/out.lbl', mem_ds)
    ds = gdal.Open('/vsimem/out.lbl')
    if ds.GetRasterBand(1).GetScale() != 10:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetOffset() != 20:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/out.lbl')

    return 'success'

# Test objects with same name
def isis_26():
    gdal.FileFromMemBuffer('/vsimem/in.lbl', """Object = IsisCube
  Object = Core
    StartByte = 1
    Format = BandSequential
    Group = Dimensions
      Samples = 1
      Lines   = 1
      Bands   = 1
    End_Group
    Group = Pixels
      Type       = UnsignedByte
      ByteOrder  = Lsb
      Base       = 0.0
      Multiplier = 1.0
    End_Group
  End_Object
End_Object

Object = Table
  Name = first_table
End_Object

Object = Table
  Name = second_table
End_Object

Object = foo
  x = A
End_Object

Object = foo
  x = B
End_Object

Object = foo
  x = C
End_Object

End""")

    gdal.Translate('/vsimem/out.lbl', '/vsimem/in.lbl', format = 'ISIS3')

    f = gdal.VSIFOpenL('/vsimem/out.lbl', 'rb')
    content = gdal.VSIFReadL(1, 10000, f).decode('ASCII')
    gdal.VSIFCloseL(f)

    if content.find(
"""Object = Table
  Name = first_table
End_Object

Object = Table
  Name = second_table
End_Object

Object = foo
  x = A
End_Object

Object = foo
  x = B
End_Object

Object = foo
  x = C
End_Object
""") < 0:
        gdaltest.post_reason('fail')
        print(content)
        return 'fail'

    gdal.Unlink('/vsimem/in.lbl')
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/out.lbl')

    return 'success'

# Test history
def isis_27():

    for src_location in ['LABEL', 'EXTERNAL']:
      for dst_location in ['LABEL', 'EXTERNAL']:
        gdal.GetDriverByName('ISIS3').Create('/vsimem/out.lbl', 1, 1,
                                    options = ['DATA_LOCATION=' + src_location])
        gdal.Translate('/vsimem/out2.lbl', '/vsimem/out.lbl', format = 'ISIS3',
                        creationOptions = ['DATA_LOCATION=' + dst_location])

        f = gdal.VSIFOpenL('/vsimem/out2.lbl', 'rb')
        content = None
        if f is not None:
            content = gdal.VSIFReadL(1, 10000, f).decode('ASCII')
            gdal.VSIFCloseL(f)

        ds = gdal.Open('/vsimem/out2.lbl')
        lbl = ds.GetMetadata_List('json:ISIS3')[0]
        lbl = json.loads(lbl)
        offset = lbl["History"]["StartByte"] - 1
        size = lbl["History"]["Bytes"]

        if dst_location == 'EXTERNAL':
            history_filename = lbl['History']['^History']
            if history_filename != 'out2.History.IsisCube':
                gdaltest.post_reason('fail')
                print(src_location)
                print(dst_location)
                print(content)
                return 'fail'

            f = gdal.VSIFOpenL('/vsimem/' + history_filename, 'rb')
            history = None
            if f is not None:
                history = gdal.VSIFReadL(1, 10000, f).decode('ASCII')
                gdal.VSIFCloseL(f)

            if offset != 0 or size != len(history):
                gdaltest.post_reason('fail')
                print(src_location)
                print(dst_location)
                print(content)
                print(history)
                return 'fail'
        else:
            if offset + size != len(content):
                gdaltest.post_reason('fail')
                print(src_location)
                print(dst_location)
                print(content)
                return 'fail'
            history = content[offset:]

        if history.find('Object = ') != 0 or \
        history.find('FROM = out.lbl') < 0 or \
        history.find('TO   = out2.lbl') < 0 or \
        history.find('TO = out.lbl') < 0:
                gdaltest.post_reason('fail')
                print(src_location)
                print(dst_location)
                print(content)
                print(history)
                return 'fail'

        gdal.GetDriverByName('ISIS3').Delete('/vsimem/out.lbl')
        gdal.GetDriverByName('ISIS3').Delete('/vsimem/out2.lbl')

    # Test GDAL_HISTORY
    gdal.GetDriverByName('ISIS3').Create('/vsimem/out.lbl', 1, 1,
                                    options = ['GDAL_HISTORY=foo'])
    f = gdal.VSIFOpenL('/vsimem/out.lbl', 'rb')
    content = None
    if f is not None:
        content = gdal.VSIFReadL(1, 10000, f).decode('ASCII')
        gdal.VSIFCloseL(f)
    if content.find('foo') < 0:
        gdaltest.post_reason('fail')
        print(content)
        return 'fail'
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/out.lbl')

    return 'success'

# Test preservation of non-pixel sections
def isis_28():

    gdal.FileFromMemBuffer('/vsimem/in_table',"FOO")
    gdal.FileFromMemBuffer('/vsimem/in.lbl', """Object = IsisCube
  Object = Core
    StartByte = 1
    Format = BandSequential
    Group = Dimensions
      Samples = 1
      Lines   = 1
      Bands   = 1
    End_Group
    Group = Pixels
      Type       = UnsignedByte
      ByteOrder  = Lsb
      Base       = 0.0
      Multiplier = 1.0
    End_Group
  End_Object
End_Object

Object = Table
  Name = first_table
  StartByte = 1
  Bytes = 3
  ^Table = in_table
End_Object
End""")

    ds = gdal.Open('/vsimem/in.lbl')
    fl = ds.GetFileList()
    if fl != ['/vsimem/in.lbl', '/vsimem/in_table']:
        gdaltest.post_reason('fail')
        print(fl)
        return 'success'
    ds = None

    gdal.Translate('/vsimem/in_label.lbl', '/vsimem/in.lbl', format = 'ISIS3')

    for src_location in ['LABEL', 'EXTERNAL']:
      if src_location == 'LABEL':
          src = '/vsimem/in_label.lbl'
      else:
          src = '/vsimem/in.lbl'
      for dst_location in ['LABEL', 'EXTERNAL']:
        gdal.Translate('/vsimem/out.lbl', src, format = 'ISIS3',
                            creationOptions = ['DATA_LOCATION=' + dst_location])
        f = gdal.VSIFOpenL('/vsimem/out.lbl', 'rb')
        content = None
        if f is not None:
            content = gdal.VSIFReadL(1, 10000, f).decode('ASCII')
            gdal.VSIFCloseL(f)

        ds = gdal.Open('/vsimem/out.lbl')
        lbl = ds.GetMetadata_List('json:ISIS3')[0]
        lbl = json.loads(lbl)
        offset = lbl["Table_first_table"]["StartByte"] - 1
        size = lbl["Table_first_table"]["Bytes"]

        if dst_location == 'EXTERNAL':
            table_filename = lbl['Table_first_table']['^Table']
            if table_filename != 'out.Table.first_table':
                gdaltest.post_reason('fail')
                print(src_location)
                print(dst_location)
                print(content)
                return 'fail'

            f = gdal.VSIFOpenL('/vsimem/' + table_filename, 'rb')
            table = None
            if f is not None:
                table = gdal.VSIFReadL(1, 10000, f).decode('ASCII')
                gdal.VSIFCloseL(f)

            if offset != 0 or size != 3 or size != len(table):
                gdaltest.post_reason('fail')
                print(src_location)
                print(dst_location)
                print(content)
                print(table)
                return 'fail'
        else:
            if offset + size != len(content):
                gdaltest.post_reason('fail')
                print(src_location)
                print(dst_location)
                print(content)
                return 'fail'
            table = content[offset:]

        if table != 'FOO':
                gdaltest.post_reason('fail')
                print(src_location)
                print(dst_location)
                print(content)
                print(table)
                return 'fail'


        gdal.GetDriverByName('ISIS3').Delete('/vsimem/out.lbl')

    gdal.GetDriverByName('ISIS3').Delete('/vsimem/in_label.lbl')
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/in.lbl')
    return 'success'

# Test complete removal of history
def isis_29():

    with gdaltest.error_handler():
        gdal.Translate('/vsimem/in.lbl', 'data/byte.tif', format = 'ISIS3')

    gdal.Translate('/vsimem/out.lbl', '/vsimem/in.lbl',
                options = '-of ISIS3 -co USE_SRC_HISTORY=NO -co ADD_GDAL_HISTORY=NO')

    ds = gdal.Open('/vsimem/out.lbl')
    lbl = ds.GetMetadata_List('json:ISIS3')[0]
    lbl = json.loads(lbl)
    if 'History' in lbl:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    ds = None

    gdal.GetDriverByName('ISIS3').Delete('/vsimem/out.lbl')

    gdal.Translate('/vsimem/out.lbl', '/vsimem/in.lbl',
                options = '-of ISIS3 -co USE_SRC_HISTORY=NO -co ADD_GDAL_HISTORY=NO -co DATA_LOCATION=EXTERNAL')

    ds = gdal.Open('/vsimem/out.lbl')
    lbl = ds.GetMetadata_List('json:ISIS3')[0]
    lbl = json.loads(lbl)
    if 'History' in lbl:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    ds = None
    if gdal.VSIStatL('/vsimem/out.History.IsisCube') is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.GetDriverByName('ISIS3').Delete('/vsimem/out.lbl')
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/in.lbl')

    return 'success'

gdaltest_list = [
    isis_1,
    isis_2,
    isis_3,
    isis_4,
    isis_5,
    isis_6,
    isis_7,
    isis_8,
    isis_9,
    isis_10,
    isis_11,
    isis_12,
    isis_13,
    isis_14,
    isis_15,
    isis_16,
    isis_17,
    isis_18,
    isis_19,
    isis_20,
    isis_21,
    isis_22,
    isis_23,
    isis_24,
    isis_25,
    isis_26,
    isis_27,
    isis_28,
    isis_29 ]


if __name__ == '__main__':

    gdaltest.setup_run( 'isis' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

