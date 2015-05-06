#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  PlanetLabs scene driver test suite.
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

import sys

sys.path.append( '../pymod' )

from osgeo import ogr
from osgeo import gdal

import gdaltest

###############################################################################
# Find PLScenes driver

def ogr_plscenes_1():

    gdaltest.plscenes_drv = ogr.GetDriverByName('PLScenes')
    
    if gdaltest.plscenes_drv is not None:
        return 'success'
    else:
        return 'skip'

###############################################################################
# Various tests on a /vsimem/ "server"

def ogr_plscenes_2():

    if gdaltest.plscenes_drv is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/root', '{"ortho":"/vsimem/root/ortho/"}')

    gdal.FileFromMemBuffer('/vsimem/valid_root_but_invalid_child',
                           '{"ortho":"/vsimem/valid_root_but_invalid_child/invalid_child/"}')

    # Error: no API_KEY
    gdal.PushErrorHandler()
    gdal.SetConfigOption('PL_URL', '/vsimem/root/')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR)
    gdal.SetConfigOption('PL_URL', None)
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Error case
    gdal.PushErrorHandler()
    gdal.SetConfigOption('PL_URL', '/vsimem/does_not_exist/')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR, open_options = ['API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Error case
    gdal.SetConfigOption('PL_URL', '/vsimem/valid_root_but_invalid_child/')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR, open_options = ['API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    gdal.PushErrorHandler()
    ret = ds.GetLayer(0).GetFeatureCount()
    gdal.PopErrorHandler()
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Error cases
    for ortho_json in [ """{}""",
                        """{ invalid_json,""",
                        """{ "type": "FeatureCollection" }""",
                        """{ "type": "FeatureCollection", "count": -1 }""",
                        """{ "type": "FeatureCollection", "count": 0 }""",
                        """{ "type": "FeatureCollection", "count": 1 }""",
                        """{ "type": "FeatureCollection", "count": 1, "features": [] }""",
    ]:

        gdal.FileFromMemBuffer('/vsimem/root/ortho/?count=1000', ortho_json)

        gdal.SetConfigOption('PL_URL', '/vsimem/root/')
        ds = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR, open_options = ['API_KEY=foo'])
        gdal.SetConfigOption('PL_URL', None)
        lyr = ds.GetLayer(0)
        gdal.PushErrorHandler()
        f = lyr.GetNextFeature()
        gdal.PopErrorHandler()
        if f:
            gdaltest.post_reason('fail')
            return 'fail'

    gdal.FileFromMemBuffer('/vsimem/root/ortho/?count=1', """{
    "count": 2,
}""")

    gdal.FileFromMemBuffer('/vsimem/root/ortho/?count=1000', """{
    "type": "FeatureCollection",
    "count": 2,
    "features": [
        {
            "type": "Feature",
            "id": "my_id",
            "geometry": {
                    "coordinates": [ [ [2,49],[2,50],[3,50],[3,49],[2,49] ] ],
                    "type": "Polygon"
            },
            "properties": {
                "acquired" : "2015-03-27T12:34:56.123+00",
                "camera" : {
                    "bit_depth" : 12,
                    "color_mode": "RGB"
                },
                "cloud_cover" : {
                    "estimated" : 0.25
                }
            }
        }
    ],
    "links": {
        "next" : "/vsimem/root/ortho/?count=1000&page=2"
    }
}""")

    gdal.FileFromMemBuffer('/vsimem/root/ortho/?count=1000&page=2', """{
    "type": "FeatureCollection",
    "count": 1,
    "features": [
        {
            "type": "Feature",
            "id": "my_id2",
            "geometry": null,
            "properties": {}
        }
    ],
    "links": {
        "next" : null
    }
}""")

    my_id_only = """{
    "type": "FeatureCollection",
    "count": 1,
    "features": [
        {
            "type": "Feature",
            "id": "my_id",
            "geometry": {
                    "coordinates": [ [ [2,49],[2,50],[3,50],[3,49],[2,49] ] ],
                    "type": "Polygon"
            },
            "properties": {
                "acquired" : "2015-03-27T12:34:56.123+00",
                "camera" : {
                    "bit_depth" : 12,
                    "color_mode": "RGB"
                },
                "cloud_cover" : {
                    "estimated" : 0.25
                }
            }
        }
    ],
    "links": {
        "next" : null
    }
}"""

    gdal.FileFromMemBuffer('/vsimem/root/ortho/?count=1&intersects=POINT%282.5%2049.5%29',
                           my_id_only)

    gdal.FileFromMemBuffer('/vsimem/root/ortho/?count=1&intersects=POLYGON%20%28%282%2049%2C2%2050%2C3%2050%2C3%2049%2C2%2049%29%29',
                           my_id_only)

    gdal.FileFromMemBuffer('/vsimem/root/ortho/?count=1000&intersects=POINT%282.5%2049.5%29',
                           my_id_only)

    gdal.FileFromMemBuffer('/vsimem/root/ortho/?count=1000&camera.color_mode.eq=RGB&acquired.gte=2015-03-27T12:34:56&acquired.lt=2015-03-27T12:34:57&cloud_cover.estimated.gt=0.20000000&camera.bit_depth.lte=12&camera.bit_depth.gte=12&camera.bit_depth.lt=13',
                          my_id_only)

    gdal.FileFromMemBuffer('/vsimem/root/ortho/?count=1&camera.color_mode.eq=RGB&acquired.gte=2015-03-27T12:34:56&acquired.lt=2015-03-27T12:34:57&cloud_cover.estimated.gt=0.20000000&camera.bit_depth.lte=12&camera.bit_depth.gte=12&camera.bit_depth.lt=13',
                          my_id_only)

    gdal.SetConfigOption('PL_URL', '/vsimem/root/')
    gdal.PushErrorHandler()
    ds = gdal.OpenEx('PLScenes:api_key=foo,unsupported_option=val', gdal.OF_VECTOR)
    gdal.PopErrorHandler()
    gdal.SetConfigOption('PL_URL', None)
    if ds is not None or gdal.GetLastErrorMsg().find('Unsupported option unsupported_option') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    gdal.SetConfigOption('PL_URL', '/vsimem/root/')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR, open_options = ['API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetLayer(-1) or ds.GetLayer(1):
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.GetLayerByName('ortho')
    if lyr.TestCapability(ogr.OLCFastFeatureCount) != 1 or \
       lyr.TestCapability(ogr.OLCStringsAsUTF8) != 1 or \
       lyr.TestCapability(ogr.OLCRandomRead) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetFeatureCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    ext = lyr.GetExtent()
    if ext != (2.0, 3.0, 49.0, 50.0):
        gdaltest.post_reason('fail')
        print(ext)
        return 'fail'
    #lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.id != 'my_id' or f.acquired != '2015/03/27 12:34:56.123+00' or \
       f['cloud_cover.estimated'] != 0.25:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    if f.GetGeometryRef().ExportToWkt() != 'MULTIPOLYGON (((2 49,2 50,3 50,3 49,2 49)))':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    f = lyr.GetNextFeature()
    if f.id != 'my_id2':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    if lyr.GetFeatureCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    lyr.SetSpatialFilterRect(-1000,-1000,1000,1000)
    if lyr.GetFeatureCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr.SetSpatialFilterRect(2.5,49.5,2.5,49.5)
    f = lyr.GetNextFeature()
    if f.id != 'my_id':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    lyr.SetSpatialFilter(None)

    # Filter that can be passed to server side
    filterstring = "\"camera.color_mode\" = 'RGB' AND acquired = '2015/03/27 12:34:56' AND \"cloud_cover.estimated\" > 0.2 AND \"camera.bit_depth\" <= 12 AND \"camera.bit_depth\" >= 12 AND \"camera.bit_depth\" < 13"
    lyr.SetAttributeFilter(filterstring)
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f.id != 'my_id':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Same but invert GetNextFeature() and GetFeatureCount()
    lyr.SetAttributeFilter(filterstring)
    f = lyr.GetNextFeature()
    if f.id != 'my_id':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Filter that can be - partly - passed to server side
    filterstring = "fid = 1 AND \"camera.color_mode\" = 'RGB' AND acquired = '2015/03/27 12:34:56' AND \"cloud_cover.estimated\" > 0.2 AND \"camera.bit_depth\" <= 12 AND \"camera.bit_depth\" >= 12 AND \"camera.bit_depth\" < 13"
    lyr.SetAttributeFilter(filterstring)
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f.id != 'my_id':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    # Same but invert GetNextFeature() and GetFeatureCount()
    lyr.SetAttributeFilter(filterstring)
    f = lyr.GetNextFeature()
    if f.id != 'my_id':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # Filter that cannot be passed to server side
    filterstring = "fid = 1"
    lyr.SetAttributeFilter(filterstring)
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f.id != 'my_id':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    # Same but invert GetNextFeature() and GetFeatureCount()
    lyr.SetAttributeFilter(filterstring)
    f = lyr.GetNextFeature()
    if f.id != 'my_id':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # Filter on id
    gdal.FileFromMemBuffer('/vsimem/root/ortho/my_id', """{
            "type": "Feature",
            "id": "my_id",
            "geometry": {
                    "coordinates": [ [ [2,49],[2,50],[3,50],[3,49],[2,49] ] ],
                    "type": "Polygon"
            },
            "properties": {
                "acquired" : "2015-03-27T12:34:56.123+00",
                "camera" : {
                    "bit_depth" : 12,
                    "color_mode": "RGB"
                },
                "cloud_cover" : {
                    "estimated" : 0.25
                }
            }
        }""")

    filterstring = "id = 'my_id'"
    lyr.SetAttributeFilter(filterstring)
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f.id != 'my_id':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    # Same but invert GetNextFeature() and GetFeatureCount()
    lyr.SetAttributeFilter(filterstring)
    f = lyr.GetNextFeature()
    if f.id != 'my_id':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.ErrorReset()
    lyr.SetAttributeFilter("id = 'non_existing_id'")
    if lyr.GetFeatureCount() != 0 or gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f is not None or gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Unset attribute filter
    lyr.SetAttributeFilter(None)
    f = lyr.GetNextFeature()
    if f.id != 'my_id':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Regular ExecuteSQL
    sql_lyr = ds.ExecuteSQL("select * from ortho")
    f = sql_lyr.GetNextFeature()
    if f.id != 'my_id':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    # Test ordered by optimization
    gdal.FileFromMemBuffer('/vsimem/root/ortho/?count=1000&order_by=acquired%20asc', """{
    "type": "FeatureCollection",
    "count": 2,
    "features": [
        {
            "type": "Feature",
            "id": "my_id2",
            "geometry": null,
            "properties": {}
        }
    ],
    "links": {
        "next" : null
    }
}""")

    sql_lyr = ds.ExecuteSQL("select * from ortho order by acquired asc")
    f = sql_lyr.GetNextFeature()
    if f.id != 'my_id2':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    # Test spat option
    ds = None
    gdal.SetConfigOption('PL_URL', '/vsimem/root/')
    ds = gdal.OpenEx('PLScenes:spat=2 49 3 50', gdal.OF_VECTOR, open_options = ['API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    lyr = ds.GetLayer(0)
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr.SetSpatialFilterRect(2.5,49.5,2.5,49.5)
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.SetConfigOption('PL_URL', '/vsimem/root/')
    ds = gdal.OpenEx('PLScenes:spat=2.5 49.5 2.5 49.5', gdal.OF_VECTOR, open_options = ['API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    lyr = ds.GetLayer(0)
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/root')
    gdal.Unlink('/vsimem/valid_root_but_invalid_child')
    gdal.Unlink('/vsimem/root/ortho/?count=1')
    gdal.Unlink('/vsimem/root/ortho/?count=1000')
    gdal.Unlink('/vsimem/root/ortho/?count=1000&page=2')
    gdal.Unlink('/vsimem/root/ortho/?count=1&intersects=POINT%282.5%2049.5%29')
    gdal.Unlink('/vsimem/root/ortho/?count=1&intersects=POLYGON%20%28%282%2049%2C2%2050%2C3%2050%2C3%2049%2C2%2049%29%29')
    gdal.Unlink('/vsimem/root/ortho/?count=1000&intersects=POINT%282.5%2049.5%29')
    gdal.Unlink('/vsimem/root/ortho/?count=1&camera.color_mode.eq=RGB&acquired.gte=2015-03-27T12:34:56&acquired.lt=2015-03-27T12:34:57&cloud_cover.estimated.gt=0.20000000&camera.bit_depth.lte=12&camera.bit_depth.gte=12&camera.bit_depth.lt=13')
    gdal.Unlink('/vsimem/root/ortho/?count=1000&camera.color_mode.eq=RGB&acquired.gte=2015-03-27T12:34:56&acquired.lt=2015-03-27T12:34:57&cloud_cover.estimated.gt=0.20000000&camera.bit_depth.lte=12&camera.bit_depth.gte=12&camera.bit_depth.lt=13')
    gdal.Unlink('/vsimem/root/ortho/?count=1000&order_by=acquired%20asc')
    gdal.Unlink('/vsimem/root/ortho/my_id')

    return 'success'

###############################################################################
# Raster access on a /vsimem/ "server"

def ogr_plscenes_3():

    if gdaltest.plscenes_drv is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/root', '{"ortho":"/vsimem/root/ortho/"}')

    # Error case: missing scene
    gdal.SetConfigOption('PL_URL', '/vsimem/root/')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_RASTER, open_options = ['API_KEY=foo', 'SCENE=not_existing_scene'])
    gdal.PopErrorHandler()
    gdal.SetConfigOption('PL_URL', None)
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Error case: invalid scene JSon
    gdal.FileFromMemBuffer('/vsimem/root/ortho/my_id', """{""")

    gdal.SetConfigOption('PL_URL', '/vsimem/root/')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_RASTER, open_options = ['API_KEY=foo', 'SCENE=my_id'])
    gdal.PopErrorHandler()
    gdal.SetConfigOption('PL_URL', None)
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Error case: mssing properties
    gdal.FileFromMemBuffer('/vsimem/root/ortho/my_id', """{}""")

    gdal.SetConfigOption('PL_URL', '/vsimem/root/')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_RASTER, open_options = ['API_KEY=foo', 'SCENE=my_id'])
    gdal.PopErrorHandler()
    gdal.SetConfigOption('PL_URL', None)
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'


    gdal.FileFromMemBuffer('/vsimem/root/ortho/my_id', """{
        "type": "Feature",
        "id": "my_id",
        "geometry": {
                "coordinates": [ [ [2,49],[2,50],[3,50],[3,49],[2,49] ] ],
                "type": "Polygon"
        },
        "properties": {
            "acquired" : "2015-03-27T12:34:56.123+00",
            "camera" : {
                "bit_depth" : 12,
                "color_mode": "RGB"
            },
            "cloud_cover" : {
                "estimated" : 0.25
            }
        }
    }""")

    # Error case: missing links
    gdal.SetConfigOption('PL_URL', '/vsimem/root/')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_RASTER, open_options = ['API_KEY=foo', 'SCENE=my_id'])
    gdal.PopErrorHandler()
    gdal.SetConfigOption('PL_URL', None)
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/root/ortho/my_id', """{
        "type": "Feature",
        "id": "my_id",
        "geometry": {
                "coordinates": [ [ [2,49],[2,50],[3,50],[3,49],[2,49] ] ],
                "type": "Polygon"
        },
        "properties": {
            "acquired" : "2015-03-27T12:34:56.123+00",
            "camera" : {
                "bit_depth" : 12,
                "color_mode": "RGB"
            },
            "cloud_cover" : {
                "estimated" : 0.25
            },
            "data": {
                "products": {
                    "visual": {
                        "full": "/vsimem/root/ortho/my_id/full?product=visual"
                    },
                    "analytic": {
                        "full": "/vsimem/root/ortho/my_id/full?product=analytic"
                    }
                }
            },
            "links": {
                "thumbnail": "/vsimem/root/ortho/my_id/thumb"
            }
        }
    }""")

    # Error case: raster file not accessible
    gdal.SetConfigOption('PL_URL', '/vsimem/root/')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_RASTER, open_options = ['API_KEY=foo', 'SCENE=my_id'])
    gdal.PopErrorHandler()
    gdal.SetConfigOption('PL_URL', None)
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Now everything ok
    gdal.FileFromMemBuffer('/vsimem/root/ortho/my_id/full?product=visual',
                           open('../gcore/data/byte.tif', 'rb').read())
    gdal.FileFromMemBuffer('/vsimem/root/ortho/my_id/full?product=analytic',
                           open('../gcore/data/byte.tif', 'rb').read())
    gdal.FileFromMemBuffer('/vsimem/root/ortho/my_id/thumb',
                           open('../gcore/data/byte.tif', 'rb').read())

    gdal.SetConfigOption('PL_URL', '/vsimem/root/')
    gdal.PushErrorHandler()
    ds = gdal.OpenEx('PLScenes:api_key=foo,scene=my_id,unsupported_option=val', gdal.OF_RASTER)
    gdal.PopErrorHandler()
    gdal.SetConfigOption('PL_URL', None)
    if ds is not None or gdal.GetLastErrorMsg().find('Unsupported option unsupported_option') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    gdal.SetConfigOption('PL_URL', '/vsimem/root/')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_RASTER, open_options = ['API_KEY=foo', 'SCENE=my_id'])
    gdal.SetConfigOption('PL_URL', None)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    md = ds.GetMetadata()
    if md['id'] != 'my_id':
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.SetConfigOption('PL_URL', '/vsimem/root/')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_RASTER, open_options = ['API_KEY=foo', 'SCENE=my_id', 'PRODUCT_TYPE=analytic'])
    gdal.SetConfigOption('PL_URL', None)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    md = ds.GetMetadata()
    if md['id'] != 'my_id':
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.SetConfigOption('PL_URL', '/vsimem/root/')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_RASTER, open_options = ['API_KEY=foo', 'SCENE=my_id', 'PRODUCT_TYPE=thumb'])
    gdal.SetConfigOption('PL_URL', None)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/root')
    gdal.Unlink('/vsimem/root/ortho/my_id/full?product=visual')
    gdal.Unlink('/vsimem/root/ortho/my_id/full?product=analytic')
    gdal.Unlink('/vsimem/root/ortho/my_id/thumb')
    gdal.Unlink('/vsimem/root/ortho/my_id')

    return 'success'

###############################################################################
# Test accessing non-ortho scene type

def ogr_plscenes_4():

    if gdaltest.plscenes_drv is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/root', '{"ortho":"/vsimem/root/ortho/"}')

    gdal.FileFromMemBuffer('/vsimem/root/another_layer/?count=10', """{
    "type": "FeatureCollection",
    "count": 1,
    "features": [
        {
            "type": "Feature",
            "id": "my_id",
            "properties": {
                "prop_10": "prop_10",
                "prop_1" : "prop_1"
            }
        }
    ]
}""")

    gdal.FileFromMemBuffer('/vsimem/root/another_layer/?count=1000', """{
    "type": "FeatureCollection",
    "count": 1,
    "features": [
        {
            "type": "Feature",
            "id": "my_id",
            "properties": {
                "prop_10": "prop_10",
                "prop_1" : "prop_1"
            }
        }
    ]
}""")

    gdal.SetConfigOption('PL_URL', '/vsimem/root/')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR, open_options = ['API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.GetLayerByName('another_layer')
    if lyr is None:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if f['prop_1'] != 'prop_1' or f['prop_10'] != 'prop_10':
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.PushErrorHandler()
    lyr = ds.GetLayerByName('does_not_exist')
    gdal.PopErrorHandler()
    if lyr is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.FileFromMemBuffer('/vsimem/root', '{"ortho":"/vsimem/root/another_layer/"}')
    gdal.SetConfigOption('PL_URL', '/vsimem/root/')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR, open_options = ['API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/root')
    gdal.Unlink('/vsimem/root/another_layer/?count=10')
    gdal.Unlink('/vsimem/root/another_layer/?count=1000')

    return 'success'

gdaltest_list = [ 
    ogr_plscenes_1,
    ogr_plscenes_2,
    ogr_plscenes_3,
    ogr_plscenes_4 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_plscenes' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

