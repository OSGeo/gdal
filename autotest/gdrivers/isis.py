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
    lbl = ds.GetMetadata_List('json:ISIS3')[0]
    if lbl.find('OriginalLabel') < 0:
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
                                    [gdal.GDT_Byte, 0, 0, ['DATA_LOCATION=GEOTIFF']],
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
    if not osr.SpatialReference(wkt).IsSame(osr.SpatialReference('PROJCS["Mercator DATUM_NAME",GEOGCS["GCS_DATUM_NAME",DATUM["D_DATUM_NAME",SPHEROID["DATUM_NAME",123456,200.0000000000004]],PRIMEM["Reference_Meridian",0],UNIT["degree",0.0174532925199433]],PROJECTION["Mercator_1SP"],PARAMETER["latitude_of_origin",1],PARAMETER["central_meridian",2],PARAMETER["scale_factor",0.9],PARAMETER["false_easting",0],PARAMETER["false_northing",0]]')):
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
    if not osr.SpatialReference(wkt).IsSame(osr.SpatialReference('PROJCS["PolarStereographic DATUM_NAME",GEOGCS["GCS_DATUM_NAME",DATUM["D_DATUM_NAME",SPHEROID["DATUM_NAME",123456,200.0000000000004]],PRIMEM["Reference_Meridian",0],UNIT["degree",0.0174532925199433]],PROJECTION["Polar_Stereographic"],PARAMETER["latitude_of_origin",1],PARAMETER["central_meridian",2],PARAMETER["scale_factor",0.9],PARAMETER["false_easting",0],PARAMETER["false_northing",0]]')):
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
    if not osr.SpatialReference(wkt).IsSame(osr.SpatialReference('PROJCS["TransverseMercator DATUM_NAME",GEOGCS["GCS_DATUM_NAME",DATUM["D_DATUM_NAME",SPHEROID["DATUM_NAME",123456,200.0000000000004]],PRIMEM["Reference_Meridian",0],UNIT["degree",0.0174532925199433]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",1],PARAMETER["central_meridian",2],PARAMETER["scale_factor",0.9],PARAMETER["false_easting",0],PARAMETER["false_northing",0]]')):
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
    if not osr.SpatialReference(wkt).IsSame(osr.SpatialReference('PROJCS["LambertConformal DATUM_NAME",GEOGCS["GCS_DATUM_NAME",DATUM["D_DATUM_NAME",SPHEROID["DATUM_NAME",123456,200.0000000000004]],PRIMEM["Reference_Meridian",0],UNIT["degree",0.0174532925199433]],PROJECTION["Lambert_Conformal_Conic_2SP"],PARAMETER["standard_parallel_1",1],PARAMETER["standard_parallel_2",2],PARAMETER["latitude_of_origin",3],PARAMETER["central_meridian",4],PARAMETER["false_easting",0],PARAMETER["false_northing",0]]')):
        gdaltest.post_reason('fail')
        print(wkt)
        return 'fail'



    sr = osr.SpatialReference()
    sr.SetEquirectangular2(0,1,2,0,0)
    sr.SetGeogCS( "GEOG_NAME", "D_DATUM_NAME", "", 123456, 200 )
    ds = gdal.GetDriverByName('ISIS3').Create('/vsimem/isis_tmp.lbl', 1, 1,
                                options = ['LATITUDE_TYPE=Planetocentric',
                                           'LONGITUDE_DIRECTION=PositiveWest',
                                           'LONGITUDE_DOMAIN=360',
                                           'BOUNDING_DEGREES=1.2,2.3,3.4,4.5'])
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform( [1000,1,0,2000,0,-1] )
    ds = None
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    lbl = ds.GetMetadata_List('json:ISIS3')[0]
    if lbl.find('"LatitudeType":"Planetocentric"') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if lbl.find('"LongitudeDirection":"PositiveWest"') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if lbl.find('"LongitudeDomain":360') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if lbl.find('"MinimumLatitude":2.3') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if lbl.find('"MinimumLongitude":1.2') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if lbl.find('"MaximumLatitude":4.5') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    if lbl.find('"MaximumLongitude":3.4') < 0:
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

    gdal.Translate( '/vsimem/isis_tmp.lbl', 'data/isis3_detached.lbl',
                   format = 'ISIS3', srcWin = [ 0, 0, 1, 1 ] )
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    lbl = ds.GetMetadata_List('json:ISIS3')[0]
    if lbl.find('OriginalLabel') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')

    return 'success'

# Test gdal.Warp() and label preservation
def isis_21():

    gdal.Unlink('/vsimem/isis_tmp.lbl')
    gdal.Warp( '/vsimem/isis_tmp.lbl', 'data/isis3_detached.lbl',
              format = 'ISIS3' )
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    if ds.GetRasterBand(1).Checksum() != 9978:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'
    lbl = ds.GetMetadata_List('json:ISIS3')[0]
    if lbl.find('OriginalLabel') < 0:
        gdaltest.post_reason('fail')
        print(lbl)
        return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')

    return 'success'

# Test source JSon use
def isis_22():

    ds = gdal.GetDriverByName('ISIS3').Create('/vsimem/isis_tmp.lbl', 1, 1)
    # Invalid type for IsisCube
    js = """{ "IsisCube": 5 }"""
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
    js = """{ "IsisCube": { "foo": "bar", "bar": [ 123, 124.0, 2.5, "xyz" ],
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
       content.find('bar       = (123, 124.0, 2.5, xyz)') < 0 or \
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
    ref_data = mem_ds.GetRasterBand(1).ReadRaster()
    gdal.Translate('/vsimem/isis_tmp.lbl', mem_ds, noData = 74,
                   format = 'ISIS3')
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    if ref_data == ds.GetRasterBand(1).ReadRaster():
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')

    gdal.Translate('/vsimem/isis_tmp.lbl', mem_ds, noData = 74,
                   format = 'ISIS3', options = [ 'DATA_LOCATION=GeoTIFF'] )
    ds = gdal.Open('/vsimem/isis_tmp.lbl')
    if ref_data == ds.GetRasterBand(1).ReadRaster():
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')

    gdal.Translate('/vsimem/isis_tmp.lbl', mem_ds, noData = 74,
                   format = 'ISIS3', options = [ 'TILED=YES'] )
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
        ref_data = mem_ds.GetRasterBand(1).ReadRaster()
        gdal.Translate('/vsimem/isis_tmp.lbl', mem_ds, noData = 74,
                    format = 'ISIS3')
        ds = gdal.Open('/vsimem/isis_tmp.lbl')
        if ref_data == ds.GetRasterBand(1).ReadRaster():
            gdaltest.post_reason('fail')
            return 'fail'
        ds = None
        gdal.GetDriverByName('ISIS3').Delete('/vsimem/isis_tmp.lbl')


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
    isis_23 ]


if __name__ == '__main__':

    gdaltest.setup_run( 'isis' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

