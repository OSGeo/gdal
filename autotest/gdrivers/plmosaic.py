#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  PlanetLabs mosaic driver test suite.
# Author:   Even Rouault, even dot rouault at spatialys.com
# 
###############################################################################
# Copyright (c) 2015, Planet Labs
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
import shutil
import struct

sys.path.append( '../pymod' )

from osgeo import gdal

import gdaltest

###############################################################################
# Find PLMosaic driver

def plmosaic_1():

    gdaltest.plmosaic_drv = gdal.GetDriverByName('PLMosaic')
    
    if gdaltest.plmosaic_drv is not None:
        return 'success'
    else:
        return 'skip'

###############################################################################
# Error: no API_KEY

def plmosaic_2():

    if gdaltest.plmosaic_drv is None:
        return 'skip'

    gdal.PushErrorHandler()
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER)
    gdal.SetConfigOption('PL_URL', None)
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Error case: invalid root URL

def plmosaic_3():

    if gdaltest.plmosaic_drv is None:
        return 'skip'

    gdal.PushErrorHandler()
    gdal.SetConfigOption('PL_URL', '/vsimem/does_not_exist/')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options = ['API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Error case: invalid JSON

def plmosaic_4():

    if gdaltest.plmosaic_drv is None:
        return 'skip'
    
    gdal.FileFromMemBuffer('/vsimem/root', """{""")

    gdal.PushErrorHandler()
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options = ['API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Error case: not a JSON dictionary

def plmosaic_5():

    if gdaltest.plmosaic_drv is None:
        return 'skip'
    
    gdal.FileFromMemBuffer('/vsimem/root', """null""")

    gdal.PushErrorHandler()
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options = ['API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Error case: missing "mosaics" element

def plmosaic_6():

    if gdaltest.plmosaic_drv is None:
        return 'skip'
    
    gdal.FileFromMemBuffer('/vsimem/root', """{}""")

    gdal.PushErrorHandler()
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options = ['API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Valid root but no mosaics

def plmosaic_7():

    if gdaltest.plmosaic_drv is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/root', """{
    "mosaics": [],
}""")

    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options = ['API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    if ds.GetMetadata("SUBDATASETS") != {}:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Valid root with 2 mosaics

def plmosaic_8():

    if gdaltest.plmosaic_drv is None:
        return 'skip'
    
    gdal.FileFromMemBuffer('/vsimem/root', """{
    "links" : { "next": "/vsimem/root/?page=2" },
    "mosaics": [
        {
            "name": "my_mosaic",
            "title": "My mosaic",
            "coordinate_system": "EPSG:3857",
            "links" : {
                "self": "/vsimem/root/my_mosaic"
            }
        }
    ],
}""")
    gdal.FileFromMemBuffer('/vsimem/root/?page=2', """{
    "links" : { "next": null },
    "mosaics": [
        {
            "name": "another_mosaic",
            "coordinate_system": "EPSG:3857",
            "links" : {
                "self": "/vsimem/root/another_mosaic"
            }
        },
        {
            "name": "this_one_will_be_ignored",
            "coordinate_system": "EPSG:1234",
            "links" : {
                "self": "/vsimem/root/this_one_will_be_ignored"
            }
        }
    ],
}""")

    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options = ['API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    if ds.GetMetadata("SUBDATASETS") != {'SUBDATASET_2_NAME': 'PLMOSAIC:mosaic=another_mosaic', 'SUBDATASET_2_DESC': 'Mosaic another_mosaic', 'SUBDATASET_1_NAME': 'PLMOSAIC:mosaic=my_mosaic', 'SUBDATASET_1_DESC': 'My mosaic'}:
        gdaltest.post_reason('fail')
        print(ds.GetMetadata("SUBDATASETS"))
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Error case: invalid mosaic

def plmosaic_9():

    if gdaltest.plmosaic_drv is None:
        return 'skip'

    gdal.PushErrorHandler()
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options = ['API_KEY=foo', 'MOSAIC=does_not_exist'])
    gdal.SetConfigOption('PL_URL', None)
    gdal.PopErrorHandler()
    if ds is not None or gdal.GetLastErrorMsg().find('Cannot find /vsimem/root/does_not_exist') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
# Invalid mosaic definition: invalid JSON

def plmosaic_9bis():

    if gdaltest.plmosaic_drv is None:
        return 'skip'
    
    gdal.FileFromMemBuffer('/vsimem/root/my_mosaic', """{""")
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    gdal.PushErrorHandler()
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options = ['API_KEY=foo', 'MOSAIC=my_mosaic'])
    gdal.PopErrorHandler()
    gdal.SetConfigOption('PL_URL', None)
    if ds is not None or gdal.GetLastErrorMsg().find('JSON parsing error') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
# Invalid mosaic definition: missing parameters

def plmosaic_10():

    if gdaltest.plmosaic_drv is None:
        return 'skip'
    
    gdal.FileFromMemBuffer('/vsimem/root/my_mosaic', """{
    "name": "my_mosaic"
}""")
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    gdal.PushErrorHandler()
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options = ['API_KEY=foo', 'MOSAIC=my_mosaic'])
    gdal.PopErrorHandler()
    gdal.SetConfigOption('PL_URL', None)
    if ds is not None or gdal.GetLastErrorMsg().find('Missing required parameter') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
# Invalid mosaic definition: unsupported projection

def plmosaic_11():

    if gdaltest.plmosaic_drv is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/root/my_mosaic', """{
    "name": "my_mosaic",
    "coordinate_system": "EPSG:1234",
    "datatype": "byte",
    "quad_pattern": "my_{glevel:d}_{tilex:04d}_{tiley:04d}",
    "quad_size": 4096,
    "resolution": 4.77731426716,
    "links" : {
        "quads" : "/vsimem/root/my_mosaic/quads/"
    }
}""")
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    gdal.PushErrorHandler()
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options = ['API_KEY=foo', 'MOSAIC=my_mosaic'])
    gdal.PopErrorHandler()
    gdal.SetConfigOption('PL_URL', None)
    if ds is not None or gdal.GetLastErrorMsg().find('Unsupported coordinate_system') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
# Invalid mosaic definition: unsupported datatype

def plmosaic_12():

    if gdaltest.plmosaic_drv is None:
        return 'skip'
    
    gdal.FileFromMemBuffer('/vsimem/root/my_mosaic', """{
    "name": "my_mosaic",
    "coordinate_system": "EPSG:3857",
    "datatype": "blabla",
    "quad_pattern": "my_{glevel:d}_{tilex:04d}_{tiley:04d}",
    "quad_size": 4096,
    "resolution": 4.77731426716,
    "links" : {
        "quads" : "/vsimem/root/my_mosaic/quads/"
    }
}""")
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    gdal.PushErrorHandler()
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options = ['API_KEY=foo', 'MOSAIC=my_mosaic'])
    gdal.PopErrorHandler()
    gdal.SetConfigOption('PL_URL', None)
    if ds is not None or gdal.GetLastErrorMsg().find('Unsupported data_type') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
# Invalid mosaic definition: unsupported resolution

def plmosaic_13():

    if gdaltest.plmosaic_drv is None:
        return 'skip'
    
    gdal.FileFromMemBuffer('/vsimem/root/my_mosaic', """{
    "name": "my_mosaic",
    "coordinate_system": "EPSG:3857",
    "datatype": "byte",
    "quad_pattern": "my_{glevel:d}_{tilex:04d}_{tiley:04d}",
    "quad_size": 4096,
    "resolution": 1.234,
    "links" : {
        "quads" : "/vsimem/root/my_mosaic/quads/"
    }
}""")
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    gdal.PushErrorHandler()
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options = ['API_KEY=foo', 'MOSAIC=my_mosaic'])
    gdal.PopErrorHandler()
    gdal.SetConfigOption('PL_URL', None)
    if ds is not None or gdal.GetLastErrorMsg().find('Unsupported resolution') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
# Invalid mosaic definition: invalid quad_pattern

def plmosaic_14():

    if gdaltest.plmosaic_drv is None:
        return 'skip'
    
    gdal.FileFromMemBuffer('/vsimem/root/my_mosaic', """{
    "name": "my_mosaic",
    "coordinate_system": "EPSG:3857",
    "datatype": "byte",
    "quad_pattern": "my_{glevel:d}_missing_tilex_and_tiley",
    "quad_size": 4096,
    "resolution": 4.77731426716,
    "links" : {
        "quads" : "/vsimem/root/my_mosaic/quads/"
    }
}""")
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    gdal.PushErrorHandler()
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options = ['API_KEY=foo', 'MOSAIC=my_mosaic'])
    gdal.PopErrorHandler()
    gdal.SetConfigOption('PL_URL', None)
    if ds is not None or gdal.GetLastErrorMsg().find('Invalid quad_pattern') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
# Nearly valid mosaic definition. Warning about invalid links.tiles

def plmosaic_15():

    if gdaltest.plmosaic_drv is None:
        return 'skip'
    
    gdal.FileFromMemBuffer('/vsimem/root/my_mosaic', """{
    "name": "my_mosaic",
    "title": "My Mosaic",
    "coordinate_system": "EPSG:3857",
    "datatype": "byte",
    "quad_pattern": "my_{glevel:d}_{tilex:04d}_{tiley:04d}",
    "quad_size": 4096,
    "resolution": 4.77731426716,
    "first_acquired": "first_date",
    "last_acquired": "last_date",
    "links" : {
        "quads" : "/vsimem/root/my_mosaic/quads/",
        "tiles" : "/vsimem/root/my_mosaic/tiles/foo"
    }
}""")
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    gdal.PushErrorHandler()
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options = ['API_KEY=foo', 'MOSAIC=my_mosaic', 'CACHE_PATH=tmp'])
    gdal.PopErrorHandler()
    gdal.SetConfigOption('PL_URL', None)
    if gdal.GetLastErrorMsg().find('Invalid links.tiles') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'
    if ds.GetRasterBand(1).GetOverviewCount() != 0:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetOverviewCount())
        return 'fail'
    if ds.GetRasterBand(1).GetOverview(0) is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Valid mosaic definition

def plmosaic_16():

    if gdaltest.plmosaic_drv is None:
        return 'skip'
    
    try:
        shutil.rmtree('tmp/plmosaic_cache')
    except:
        pass

    gdal.FileFromMemBuffer('/vsimem/root/my_mosaic', """{
    "name": "my_mosaic",
    "title": "My Mosaic",
    "coordinate_system": "EPSG:3857",
    "datatype": "byte",
    "quad_pattern": "my_{glevel:d}_{tilex:04d}_{tiley:04d}",
    "quad_size": 4096,
    "resolution": 4.77731426716,
    "first_acquired": "first_date",
    "last_acquired": "last_date",
    "links" : {
        "quads" : "/vsimem/root/my_mosaic/quads/",
        "tiles" : "/vsimem/root/my_mosaic/tiles{0-3}/{z}/{x}/{y}.png"
    }
}""")

    # Valid root: one single mosaic, should open the dataset directly
    gdal.FileFromMemBuffer('/vsimem/root', """{
    "mosaics": [
        {
            "name": "my_mosaic",
            "title": "My Mosaic",
            "coordinate_system": "EPSG:3857",
            "links" : {
                "self": "/vsimem/root/my_mosaic"
            }
        }
    ],
}""")
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options = ['API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    if ds.GetMetadata("SUBDATASETS") != {}:
        gdaltest.post_reason('fail')
        print(ds.GetMetadata("SUBDATASETS"))
        return 'fail'
    if ds.GetMetadata() != {'LAST_ACQUIRED': 'last_date', 'TITLE': 'My Mosaic', 'FIRST_ACQUIRED': 'first_date'}:
        gdaltest.post_reason('fail')
        print(ds.GetMetadata())
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Open with explicit MOSAIC dataset open option

def plmosaic_17():

    if gdaltest.plmosaic_drv is None:
        return 'skip'
    
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options = ['API_KEY=foo', 'MOSAIC=my_mosaic', 'CACHE_PATH=tmp'])
    gdal.SetConfigOption('PL_URL', None)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetMetadata() != {'LAST_ACQUIRED': 'last_date', 'TITLE': 'My Mosaic', 'FIRST_ACQUIRED': 'first_date'}:
        gdaltest.post_reason('fail')
        print(ds.GetMetadata())
        return 'fail'
    if ds.GetProjectionRef().find('3857') < 0:
        gdaltest.post_reason('fail')
        print(ds.GetProjectionRef())
        return 'fail'
    if ds.RasterXSize != 8388608:
        gdaltest.post_reason('fail')
        print(ds.RasterXSize)
        return 'fail'
    if ds.RasterYSize != 8388608:
        gdaltest.post_reason('fail')
        print(ds.RasterYSize)
        return 'fail'
    got_gt = ds.GetGeoTransform()
    expected_gt = (-20037508.34, 4.7773142671600004, 0.0, 20037508.34, 0.0, -4.7773142671600004)
    for i in range(6):
        if abs(got_gt[i] - expected_gt[i]) > 1e-8:
            gdaltest.post_reason('fail')
            print(ds.GetGeoTransform())
            return 'fail'
    if ds.GetMetadataItem('INTERLEAVE', 'IMAGE_STRUCTURE') != 'PIXEL':
        gdaltest.post_reason('fail')
        print(ds.GetMetadata('IMAGE_STRUCTURE'))
        print(ds.GetMetadataItem('INTERLEAVE', 'IMAGE_STRUCTURE'))
        return 'fail'
    if ds.GetRasterBand(1).GetOverviewCount() != 15:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetOverviewCount())
        return 'fail'
    if ds.GetRasterBand(1).GetOverview(-1) is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetOverview(ds.GetRasterBand(1).GetOverviewCount()) is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetOverview(0) is None:
        gdaltest.post_reason('fail')
        return 'fail'

    try:
        shutil.rmtree('tmp/plmosaic_cache')
    except:
        pass

    for i in range(12):
        # Read at one inexisting position
        ds.GetRasterBand(1).ReadRaster(4096 * i,0,1,1)
        if gdal.GetLastErrorMsg() != '':
            gdaltest.post_reason('fail')
            return 'fail'
    for i in range(11,-1,-1):
        # Again in the same quad, but in different block, to test cache
        ds.GetRasterBand(1).ReadRaster(4096 * i+256,0,1,1)
        if gdal.GetLastErrorMsg() != '':
            gdaltest.post_reason('fail')
            return 'fail'
    for i in range(12):
        # Again in the same quad, but in different block, to test cache
        ds.GetRasterBand(1).ReadRaster(4096 * i+512,256,1,1)
        if gdal.GetLastErrorMsg() != '':
            gdaltest.post_reason('fail')
            return 'fail'

    ds.FlushCache()

    # Invalid tile content
    gdal.FileFromMemBuffer('/vsimem/root/my_mosaic/quads/my_15_0000_2047/full', 'garbage')
    gdal.PushErrorHandler()
    ds.GetRasterBand(1).ReadRaster(0,0,1,1)
    gdal.PopErrorHandler()

    os.stat('tmp/plmosaic_cache/my_mosaic/my_mosaic_my_15_0000_2047.tif')

    ds.FlushCache()
    shutil.rmtree('tmp/plmosaic_cache')

    # GeoTIFF but with wrong dimensions
    gdal.GetDriverByName('GTiff').Create('/vsimem/root/my_mosaic/quads/my_15_0000_2047/full', 1, 1, 1)
    gdal.PushErrorHandler()
    ds.GetRasterBand(1).ReadRaster(0,0,1,1)
    gdal.PopErrorHandler()

    os.stat('tmp/plmosaic_cache/my_mosaic/my_mosaic_my_15_0000_2047.tif')

    ds.FlushCache()
    shutil.rmtree('tmp/plmosaic_cache')

    # Good GeoTIFF
    tmp_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/root/my_mosaic/quads/my_15_0000_2047/full', 4096, 4096, 4, options = ['INTERLEAVE=BAND', 'SPARSE_OK=YES'])
    tmp_ds.GetRasterBand(1).Fill(255)
    tmp_ds = None
    filesize = gdal.VSIStatL('/vsimem/root/my_mosaic/quads/my_15_0000_2047/full').size

    val = ds.GetRasterBand(1).ReadRaster(0,0,1,1)
    val = struct.unpack('B', val)[0]
    if val != 255:
        gdaltest.post_reason('fail')
        print(val)
        return 'fail'

    os.stat('tmp/plmosaic_cache/my_mosaic/my_mosaic_my_15_0000_2047.tif')

    ds.FlushCache()

    # Read again from file cache. Will fail since /vsimem/root/my_mosaic/quads/my_15_0000_2047 does not exist
    gdal.PushErrorHandler()
    val = ds.GetRasterBand(1).ReadRaster(0,0,1,1)
    gdal.PopErrorHandler()
    val = struct.unpack('B', val)[0]
    if val != 0:
        gdaltest.post_reason('fail')
        print(val)
        return 'fail'

    ds = None

    # Read again from file cache, but with TRUST_CACHE=YES
    # delete the full GeoTIFF before
    gdal.Unlink('/vsimem/root/my_mosaic/quads/my_15_0000_2047/full')
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options = ['API_KEY=foo', 'MOSAIC=my_mosaic', 'CACHE_PATH=tmp', 'TRUST_CACHE=YES'])
    gdal.SetConfigOption('PL_URL', None)    

    val = ds.GetRasterBand(1).ReadRaster(0,0,1,1)
    val = struct.unpack('B', val)[0]
    if val != 255:
        gdaltest.post_reason('fail')
        print(val)
        return 'fail'
    ds = None

    # Read again from file cache, with JSon quad definition this time
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options = ['API_KEY=foo', 'MOSAIC=my_mosaic', 'CACHE_PATH=tmp'])
    gdal.SetConfigOption('PL_URL', None)    

    gdal.Unlink('/vsimem/root/my_mosaic/quads/my_15_0000_2047/full')
    gdal.FileFromMemBuffer('/vsimem/root/my_mosaic/quads/my_15_0000_2047', """{
    "id": "my_15_0000_2047",
    "properties": {
        "file_size": %d, 
    }
}""" % filesize)
    val = ds.GetRasterBand(1).ReadRaster(0,0,1,1)
    val = struct.unpack('B', val)[0]
    if val != 255:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.FlushCache()

    # Read again from file cache but the metatile has changed in between
    tmp_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/root/my_mosaic/quads/my_15_0000_2047/full', 4096, 4096, 4, options = ['INTERLEAVE=BAND', 'SPARSE_OK=YES'])
    tmp_ds.SetMetadataItem('foo', 'bar')
    tmp_ds.GetRasterBand(1).Fill(254)
    tmp_ds = None
    filesize = gdal.VSIStatL('/vsimem/root/my_mosaic/quads/my_15_0000_2047/full').size
    gdal.FileFromMemBuffer('/vsimem/root/my_mosaic/quads/my_15_0000_2047', """{
    "id": "my_15_0000_2047",
    "type": "Feature",
    "geometry": {
        "type": "Polygon",
        "coordinates" : [ [ [ 0, -200 ],
                            [ 0, 200 ],
                            [ 200, 200 ],
                            [ 200, -200 ],
                            [ 0, -200 ] ] ]
    },
    "properties": {
        "file_size": %d, 
    }
}""" % filesize)
    val = ds.ReadRaster(0,0,1,1)
    val = struct.unpack('B' * 4, val)
    if val != (254, 0, 0, 0):
        gdaltest.post_reason('fail')
        print(val)
        return 'fail'

    return 'success'

###############################################################################
# Test location info

def plmosaic_18():

    if gdaltest.plmosaic_drv is None:
        return 'skip'
    
    shutil.rmtree('tmp/plmosaic_cache')
    
    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options = ['API_KEY=foo', 'MOSAIC=my_mosaic', 'CACHE_PATH=tmp'])
    gdal.SetConfigOption('PL_URL', None)    
    
    ret = ds.GetRasterBand(1).GetMetadataItem('Pixel_0_0', 'LocationInfo')
    if ret != """<LocationInfo>
  <Quad>
    <id>my_15_0000_2047</id>
    <file_size>16843000</file_size>
    <geometry>POLYGON ((0 -200,0 200,200 200,200 -200,0 -200))</geometry>
  </Quad>
</LocationInfo>
""":
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'
    old_ret = ret
    ret = ds.GetRasterBand(1).GetMetadataItem('Pixel_0_0', 'LocationInfo')
    if ret != old_ret:
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/root/my_mosaic/quads/my_15_0000_2047/scenes', """{
    "type": "FeatureCollection",
    "features": [
        {
            "type" : "Feature",
            "geometry": {
                "type": "Polygon",
                "coordinates" : [ [ [ -200, -200 ],
                                  [ -200, 200 ],
                                  [ 0, 200 ],
                                  [ 0, -200 ],
                                  [ -200, -200 ] ] ]
            },
            "properties" : {
                "foo": "bar"
            }
        },
        {
            "type" : "Feature",
            "geometry": {
                "type": "Polygon",
                "coordinates" : [ [ [ 0, -200 ],
                                  [ 0, 200 ],
                                  [ 200, 200 ],
                                  [ 200, -200 ],
                                  [ 0, -200 ] ] ]
            },
            "properties" : {
                "foo": "baz"
            }
        }
    ]
}""")

    ds.FlushCache()
    
    ret = ds.GetRasterBand(1).GetMetadataItem('Pixel_0_0', 'LocationInfo')
    if ret != """<LocationInfo>
  <Quad>
    <id>my_15_0000_2047</id>
    <file_size>16843000</file_size>
    <geometry>POLYGON ((0 -200,0 200,200 200,200 -200,0 -200))</geometry>
  </Quad>
  <Scenes>
    <Scene>
      <foo>bar</foo>
      <geometry>POLYGON ((-200 -200,-200 200,0 200,0 -200,-200 -200))</geometry>
    </Scene>
  </Scenes>
</LocationInfo>
""":
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Try error in saving in cache

def plmosaic_19():

    if gdaltest.plmosaic_drv is None:
        return 'skip'

    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options = ['API_KEY=foo', 'MOSAIC=my_mosaic', 'CACHE_PATH=/does_not_exist'])
    gdal.SetConfigOption('PL_URL', None)
    gdal.PushErrorHandler()
    val = ds.ReadRaster(0,0,1,1)
    gdal.PopErrorHandler()
    val = struct.unpack('B' * 4, val)
    if val != (254, 0, 0, 0):
        gdaltest.post_reason('fail')
        print(val)
        return 'fail'

    val = ds.ReadRaster(256,0,1,1)
    val = struct.unpack('B' * 4, val)
    if val != (254, 0, 0, 0):
        gdaltest.post_reason('fail')
        print(val)
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Try disabling cache

def plmosaic_20():

    if gdaltest.plmosaic_drv is None:
        return 'skip'

    gdal.SetConfigOption('PL_URL', '/vsimem/root')
    ds = gdal.OpenEx('PLMosaic:', gdal.OF_RASTER, open_options = ['API_KEY=foo', 'MOSAIC=my_mosaic', 'CACHE_PATH='])
    gdal.SetConfigOption('PL_URL', None)
    val = ds.ReadRaster(0,0,1,1)
    val = struct.unpack('B' * 4, val)
    if val != (254, 0, 0, 0):
        gdaltest.post_reason('fail')
        print(val)
        return 'fail'

    val = ds.ReadRaster(256,0,1,1)
    val = struct.unpack('B' * 4, val)
    if val != (254, 0, 0, 0):
        gdaltest.post_reason('fail')
        print(val)
        return 'fail'
    ds = None

    return 'success'

###############################################################################
#

def plmosaic_cleanup():

    if gdaltest.plmosaic_drv is None:
        return 'skip'

    gdal.Unlink('/vsimem/root_no_mosaics')
    gdal.Unlink('/vsimem/root')
    gdal.Unlink('/vsimem/root/?page=2')
    gdal.Unlink('/vsimem/root/my_mosaic')
    gdal.Unlink('/vsimem/root/my_mosaic/quads/my_15_0000_2047')
    gdal.Unlink('/vsimem/root/my_mosaic/quads/my_15_0000_2047/full')
    gdal.Unlink('/vsimem/root/my_mosaic/quads/my_15_0000_2047/scenes')

    try:
        shutil.rmtree('tmp/plmosaic_cache')
    except:
        pass

    return 'success'


gdaltest_list = [ 
    plmosaic_1,
    plmosaic_2,
    plmosaic_3,
    plmosaic_4,
    plmosaic_5,
    plmosaic_6,
    plmosaic_7,
    plmosaic_8,
    plmosaic_9,
    plmosaic_9bis,
    plmosaic_10,
    plmosaic_11,
    plmosaic_12,
    plmosaic_13,
    plmosaic_14,
    plmosaic_15,
    plmosaic_16,
    plmosaic_17,
    plmosaic_18,
    plmosaic_19,
    plmosaic_20,
    plmosaic_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'plmosaic' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

