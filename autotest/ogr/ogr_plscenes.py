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
    old_key = gdal.GetConfigOption('PL_API_KEY')
    if old_key:
        gdal.SetConfigOption('PL_API_KEY', '')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR)
    if old_key:
        gdal.SetConfigOption('PL_API_KEY', old_key)
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
                        '"valid_json_but_not_a_json_object"',
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

    gdal.FileFromMemBuffer('/vsimem/root/ortho/?count=1&intersects=POINT(2.5%2049.5)',
                           my_id_only)

    gdal.FileFromMemBuffer('/vsimem/root/ortho/?count=1&intersects=POLYGON%20((2%2049,2%2050,3%2050,3%2049,2%2049))',
                           my_id_only)

    gdal.FileFromMemBuffer('/vsimem/root/ortho/?count=1000&intersects=POINT(2.5%2049.5)',
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
    if ds is not None or gdal.GetLastErrorMsg().find('Unsupported option') < 0:
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
    if lyr.GetFeatureCount() != 1 and gdal.GetLastErrorMsg().find('GEOS support not enabled') < 0:
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
    gdal.Unlink('/vsimem/root/ortho/?count=1&intersects=POINT(2.5%2049.5)')
    gdal.Unlink('/vsimem/root/ortho/?count=1&intersects=POLYGON%20((2%2049,2%2050,3%2050,3%2049,2%2049))')
    gdal.Unlink('/vsimem/root/ortho/?count=1000&intersects=POINT(2.5%2049.5)')
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

    # Error case: missing properties.
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

    gdal.FileFromMemBuffer('/vsimem/root', '{"another_layer":"/vsimem/root/another_layer/"}')
    gdal.SetConfigOption('PL_URL', '/vsimem/root/')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR, open_options = ['API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if f['prop_1'] != 'prop_1' or f['prop_10'] != 'prop_10':
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/root')
    gdal.Unlink('/vsimem/root/another_layer/?count=10')
    gdal.Unlink('/vsimem/root/another_layer/?count=1000')

    return 'success'

###############################################################################
# Test V1 API catalog listing with a single catalog

def ogr_plscenes_v1_catalog_no_paging():

    if gdaltest.plscenes_drv is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/v1/catalogs', '{"_links": {}, "catalogs": [{"item_count": 2, "_links": { "items": "/vsimem/v1/catalogs/my_catalog/items/", "spec": "/vsimem/v1/catalogs/my_catalog/spec"}, "id": "my_catalog"}]}')
    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR, open_options = ['VERSION=v1', 'API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    with gdaltest.error_handler():
        if ds.GetLayerByName('non_existing') is not None:
            gdaltest.post_reason('fail')
            return 'fail'
    if ds.GetLayerByName('my_catalog') is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    with gdaltest.error_handler():
        if ds.GetLayerByName('non_existing') is not None:
            gdaltest.post_reason('fail')
            return 'fail'

    gdal.Unlink('/vsimem/v1/catalogs')

    return 'success'

###############################################################################
# Test V1 API catalog listing with catalog paging

def ogr_plscenes_v1_catalog_paging():

    if gdaltest.plscenes_drv is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/v1/catalogs', '{"_links": { "_next" : "/vsimem/v1/catalogs_page_2"}, "catalogs": [{"item_count": 2, "_links": { "items": "/vsimem/v1/catalogs/my_catalog/items/", "spec": "/vsimem/v1/catalogs/my_catalog/spec"}, "id": "my_catalog"}]}')
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs_page_2', '{ "_links": { "_next" : "/vsimem/v1/catalogs_page_3"}, "catalogs": [{"item_count": 2, "_links": { "items": "/vsimem/v1/catalogs/my_catalog_2/items/", "spec": "/vsimem/v1/catalogs/my_catalog_2/spec"}, "id": "my_catalog_2"}]}')
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs_page_3', '{ "catalogs": [{"item_count": 2, "_links": { "items": "/vsimem/v1/catalogs/my_catalog_3/items/", "spec": "/vsimem/v1/catalogs/my_catalog_3/spec"}, "id": "my_catalog_3"}]}')
    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR, open_options = ['VERSION=v1', 'API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    with gdaltest.error_handler():
        if ds.GetLayerByName('non_existing') is not None:
            gdaltest.post_reason('fail')
            return 'fail'
    if ds.GetLayerByName('my_catalog') is None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog_2', '{ "_links": { "items": "/vsimem/v1/catalogs/my_catalog_2/items/", "spec": "/vsimem/v1/catalogs/my_catalog_2/spec"}, "id": "my_catalog_2"} }')
    if ds.GetLayerByName('my_catalog_2') is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetLayerCount() != 3:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetLayerByName('my_catalog_2') is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetLayerByName('my_catalog_3') is None:
        gdaltest.post_reason('fail')
        return 'fail'
    with gdaltest.error_handler():
        if ds.GetLayerByName('non_existing') is not None:
            gdaltest.post_reason('fail')
            return 'fail'

    gdal.Unlink('/vsimem/v1/catalogs')
    gdal.Unlink('/vsimem/v1/catalogs_page_2')
    gdal.Unlink('/vsimem/v1/catalogs_page_3')
    gdal.Unlink('/vsimem/v1/catalogs/my_catalog_2')

    return 'success'

###############################################################################
# Test V1 API

def ogr_plscenes_v1_nominal():

    if gdaltest.plscenes_drv is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/v1/catalogs',
"""{"_links": {}, "catalogs": [
    {"item_count": 2,
     "_links": { "items": "/vsimem/v1/catalogs/my_catalog/items/", "spec": "/vsimem/v1/catalogs/my_catalog/spec"},
     "display_description" : "display_description",
     "display_name" : "display_name",
     "id": "my_catalog"}
]}""")
    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR, open_options = ['VERSION=v1', 'API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    ds = gdal.OpenEx('PLScenes:version=v1,api_key=foo,FOLLOW_LINKS=YES', gdal.OF_VECTOR)
    gdal.SetConfigOption('PL_URL', None)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr.GetName() != 'my_catalog':
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.TestCapability(ogr.OLCFastFeatureCount) != 1 or \
       lyr.TestCapability(ogr.OLCStringsAsUTF8) != 1 or \
       lyr.TestCapability(ogr.OLCRandomRead) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetFeatureCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetGeomType() != ogr.wkbMultiPolygon:
        gdaltest.post_reason('fail')
        return 'fail'
    ext = lyr.GetExtent()
    if ext != (-180.0, 180.0, -90.0, 90.0):
        gdaltest.post_reason('fail')
        print(ext)
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/spec',
"""{
    "paths": {
        "/catalogs/my_catalog/items/" : {
            "get": {
                "responses": {
                    "200": {
                        "schema": {
                            "$ref": "#/definitions/ItemPage"
                        }
                    }
                },
                "parameters": [
                    {
                        "$ref":"#/parameters/qFloat"
                    },
                    {
                        "$ref":"#/parameters/qCreated"
                    },
                    {
                        "$ref":"#/parameters/qInt32"
                    },
                    {
                        "$ref":"#/parameters/qString"
                    }
                ]
            }
        }
    },
    "parameters": {
        "qFloat":{
            "in": "query",
            "name": "catalog::float"
        },
        "qCreated":{
            "in": "query",
            "name": "created"
        },
        "qInt32":{
            "in": "query",
            "name": "catalog::int32"
        },
        "qString":{
            "in": "query",
            "name": "catalog::string"
        },
        "qPageSize":{
            "in": "query",
            "maximum": 100,
            "name": "_page_size"
        }
    },
    "definitions": {
        "PermissionFilter": {
          "allOf": [
            {
              "$ref": "#/definitions/Filter"
            },
            {
              "properties": {
                "config": {
                  "items": {
                    "enum": [
                      "assets:download",
                      "assets.visual:download",
                      "assets.analytic:download"
                    ],
                    "type": "string"
                  },
                  "type": "array"
                }
              },
              "required": [
                "config"
              ],
              "type": "object"
            }
          ]
        },

        "Asset": {
          "properties": {
            "_links": {
              "$ref": "#/definitions/SelfLink"
            },
            "_permissions": {
              "items": {
                "enum": [
                  "download"
                ],
                "type": "string"
              },
              "type": "array",
              "uniqueItems": true
            },
            "files": {
              "additionalProperties": {
                "$ref": "#/definitions/AssetFile"
              },
              "description": "Various AssetFiles indicating how a user may download the image data associated with this Asset. The keys of this object reflect the type of each available AssetFile.",
              "type": "object"
            },
            "mimetype": {
              "description": "The MIME type of the underlying asset file.",
              "type": "string"
            },
            "type": {
              "description": "Type identifier of this Asset.",
              "type": "string"
            }
          },
          "required": [
            "type",
            "mimetype",
            "files",
            "_links",
            "_permissions"
          ],
          "type": "object"
        },

        "AssetFile": {
          "description": "An AssetFile describes the means of downloading the image data associated with a specific Asset",
          "properties": {
            "_links": {
              "properties": {
                "activate": {
                  "description": "If present, RFC 3986 URI indicating where an authenticated user may trigger activation of this AssetFile via a POST request. A 202 response indicates the activation request has been accepted. A 204 response indicates the AssetFile is already active. After successful activation, this AssetFile will have a non-empty location.",
                  "type": "string"
                }
              },
              "type": "object"
            },
            "expires_at": {
              "description": "If present, RFC 3339 timestamp indicating when this AssetFile will become inactive and will require reactivation.",
              "format": "date-time",
              "type": "string"
            },
            "location": {
              "description": "If present, RFC 3986 URI that indicates a location that will yield image data. Consult the documentation of the AssetFile type to understand how to use this URI.",
              "type": "string"
            },
            "status": {
              "description": "Current status of the AssetFile. 'inactive' indicates that the AssetFile is not currently available for download, but may be after activation. 'activating' indicates the AssetFile is currently undergoing activation, and may be available for download shortly. 'active' indicates the AssetFile has been activated, and may currently be available for download if the authentication context permits.",
              "enum": [
                "inactive",
                "activating",
                "active"
              ],
              "type": "string"
            },
            "type": {
              "description": "An identifier of the methodology that must be used to download the image data from the indicated location. In the case of 'http', the user must make an HTTP GET request against the provided URL.",
              "enum": [
                "http"
              ],
              "type": "string"
            }
          },
          "required": [
            "type",
            "status",
            "_links"
          ],
          "type": "object"
        },

        "ItemPage": {
            "type": "object",
            "allOf": [
                {
                    "$ref": "#/definitions/GeoJSONFeatureCollection"
                },
                {
                "type": "object",
                "properties": {
                    "_links": {
                        "$ref": "#/definitions/PageLinks"
                    },
                    "features": {
                    "items": {
                        "$ref": "#/definitions/Item"
                    },
                    "type": "array"
                    }
                }
                }
            ]
        },

        "Item": {
            "allOf": [
                {
                    "$ref": "#/definitions/GeoJSONFeature"
                },
                {
                    "properties": {
                        "_embeds": {
                            "$ref": "#/definitions/ItemEmbeds"
                        },
                        "_links": {
                            "$ref": "#/definitions/ItemLinks"
                        },
                        "id": {
                            "type": "string"
                        },
                        "properties": {
                            "$ref": "#/definitions/ItemProperties"
                        }
                    }
                }
            ]
        },

        "ItemLinks": {
            "allOf": [
                {
                "$ref": "#/definitions/SelfLink"
                },
                {
                "type": "object",
                "properties": {
                    "assets": {
                    "type": "string",
                    }
                }
                }
            ]
        },

        "SelfLink": {
            "type": "object",
            "properties": {
                "_self": {
                "type": "string",
                }
            }
        },

        "ItemProperties": {
            "allOf": [
                {
                "$ref": "#/definitions/CoreItemProperties"
                },
                {
                "$ref": "#/definitions/ExtraItemProperties"
                }
            ]
        },

        "CoreItemProperties": {
            "required": [
                "created"
            ],
            "type": "object",
            "properties": {
                "created": {
                "type": "string",
                "format": "date-time"
                }
            }
        },

        "ExtraItemProperties": {
            "type": "object",
            "properties": {
                "catalog::float": {
                "format": "float",
                "type": "number",
                },
                "catalog::string": {
                "type": "string",
                },
                "catalog::int32": {
                "format": "int32",
                "type": "integer",
                },
                "catalog::int64": {
                "format": "int64",
                "type": "integer",
                }
            }
        }
    }
}
""")

    expected_md = """{
  "id":{
    "type":"string",
    "src_field":"id",
    "server_queryable":true
  },
  "self_link":{
    "type":"string",
    "src_field":"_links._self",
    "server_queryable":false
  },
  "assets_link":{
    "type":"string",
    "src_field":"_links.assets",
    "server_queryable":false
  },
  "created":{
    "type":"string",
    "format":"date-time",
    "src_field":"properties.created",
    "server_queryable":true
  },
  "float":{
    "format":"float",
    "type":"number",
    "src_field":"properties.catalog::float",
    "server_queryable":true
  },
  "string":{
    "type":"string",
    "src_field":"properties.catalog::string",
    "server_queryable":true
  },
  "int32":{
    "format":"int32",
    "type":"integer",
    "src_field":"properties.catalog::int32",
    "server_queryable":true
  },
  "int64":{
    "format":"int64",
    "type":"integer",
    "src_field":"properties.catalog::int64",
    "server_queryable":false
  },
  "asset_visual_self_link":{
    "description":"RFC 3986 URI representing the canonical location of this asset.",
    "type":"string",
    "src_field":"\/assets.visual._links._self",
    "server_queryable":false
  },
  "asset_visual_permissions":{
    "items":{
      "enum":[
        "download"
      ],
      "type":"string"
    },
    "type":"array",
    "uniqueItems":true,
    "src_field":"\/assets.visual._permissions",
    "server_queryable":false
  },
  "asset_visual_activate_link":{
    "description":"If present, RFC 3986 URI indicating where an authenticated user may trigger activation of this AssetFile via a POST request. A 202 response indicates the activation request has been accepted. A 204 response indicates the AssetFile is already active. After successful activation, this AssetFile will have a non-empty location.",
    "type":"string",
    "src_field":"\/assets.visual.files._links.activate",
    "server_queryable":false
  },
  "asset_visual_expires_at":{
    "description":"If present, RFC 3339 timestamp indicating when this AssetFile will become inactive and will require reactivation.",
    "format":"date-time",
    "type":"string",
    "src_field":"\/assets.visual.files.expires_at",
    "server_queryable":false
  },
  "asset_visual_product_link":{
    "description":"If present, RFC 3986 URI that indicates a location that will yield image data. Consult the documentation of the AssetFile type to understand how to use this URI.",
    "type":"string",
    "src_field":"\/assets.visual.files.location",
    "server_queryable":false
  },
  "asset_visual_product_link_status":{
    "description":"Current status of the AssetFile. 'inactive' indicates that the AssetFile is not currently available for download, but may be after activation. 'activating' indicates the AssetFile is currently undergoing activation, and may be available for download shortly. 'active' indicates the AssetFile has been activated, and may currently be available for download if the authentication context permits.",
    "enum":[
      "inactive",
      "activating",
      "active"
    ],
    "type":"string",
    "src_field":"\/assets.visual.files.status",
    "server_queryable":false
  },
  "asset_visual_mimetype":{
    "description":"The MIME type of the underlying asset file.",
    "type":"string",
    "src_field":"\/assets.visual.mimetype",
    "server_queryable":false
  },
  "asset_analytic_self_link":{
    "description":"RFC 3986 URI representing the canonical location of this asset.",
    "type":"string",
    "src_field":"\/assets.analytic._links._self",
    "server_queryable":false
  },
  "asset_analytic_permissions":{
    "items":{
      "enum":[
        "download"
      ],
      "type":"string"
    },
    "type":"array",
    "uniqueItems":true,
    "src_field":"\/assets.analytic._permissions",
    "server_queryable":false
  },
  "asset_analytic_activate_link":{
    "description":"If present, RFC 3986 URI indicating where an authenticated user may trigger activation of this AssetFile via a POST request. A 202 response indicates the activation request has been accepted. A 204 response indicates the AssetFile is already active. After successful activation, this AssetFile will have a non-empty location.",
    "type":"string",
    "src_field":"\/assets.analytic.files._links.activate",
    "server_queryable":false
  },
  "asset_analytic_expires_at":{
    "description":"If present, RFC 3339 timestamp indicating when this AssetFile will become inactive and will require reactivation.",
    "format":"date-time",
    "type":"string",
    "src_field":"\/assets.analytic.files.expires_at",
    "server_queryable":false
  },
  "asset_analytic_product_link":{
    "description":"If present, RFC 3986 URI that indicates a location that will yield image data. Consult the documentation of the AssetFile type to understand how to use this URI.",
    "type":"string",
    "src_field":"\/assets.analytic.files.location",
    "server_queryable":false
  },
  "asset_analytic_product_link_status":{
    "description":"Current status of the AssetFile. 'inactive' indicates that the AssetFile is not currently available for download, but may be after activation. 'activating' indicates the AssetFile is currently undergoing activation, and may be available for download shortly. 'active' indicates the AssetFile has been activated, and may currently be available for download if the authentication context permits.",
    "enum":[
      "inactive",
      "activating",
      "active"
    ],
    "type":"string",
    "src_field":"\/assets.analytic.files.status",
    "server_queryable":false
  },
  "asset_analytic_mimetype":{
    "description":"The MIME type of the underlying asset file.",
    "type":"string",
    "src_field":"\/assets.analytic.mimetype",
    "server_queryable":false
  }
}"""

    md = lyr.GetMetadataItem('FIELDS_DESCRIPTION')
    if md != expected_md:
        gdaltest.post_reason('fail')
        print(md)
        return 'fail'

    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR, open_options = ['VERSION=v1', 'API_KEY=foo', 'FOLLOW_LINKS=YES'])
    gdal.SetConfigOption('PL_URL', None)
    lyr = ds.GetLayer(0)

    md = lyr.GetMetadata()['FIELDS_DESCRIPTION']
    if md != expected_md:
        gdaltest.post_reason('fail')
        print(md)
        return 'fail'

    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR, open_options = ['VERSION=v1', 'API_KEY=foo', 'FOLLOW_LINKS=YES'])
    gdal.SetConfigOption('PL_URL', None)
    lyr = ds.GetLayer(0)

    field_count = lyr.GetLayerDefn().GetFieldCount()
    if field_count != 22:
        gdaltest.post_reason('fail')
        print(field_count)
        return 'fail'

    # Test QuickSearch
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/quick-search&POSTFIELDS={}',
"""{
    "features" : [
        {
            "id": "id",
        }
    ]
}""")

    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR, open_options = ['VERSION=v1', 'API_KEY=foo', 'FILTER={}'])
    gdal.SetConfigOption('PL_URL', None)
    lyr = ds.GetLayer(0)

    f = lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/v1/catalogs/my_catalog/quick-search&POSTFIELDS={}')


    # Regular /items/ fetching
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/items/?_page_size=100&_permissions=assets.visual:download',
"""{
    "_links":
    {
        "_next": "/vsimem/v1/catalogs/my_catalog/items_page2/?_page_size=100"
    },
    "features" : [
        {
            "id": "id",
            "_links" : {
                "_self" : "self",
                "assets" : "/vsimem/v1/catalogs/my_catalog/items/id/assets"
            },
            "_permissions" : [ "download" ],
            "properties": {
                "created" : "2016-02-11T12:34:56.789Z",
                "catalog::float": 1.23,
                "catalog::string": "string",
                "catalog::int32": 123,
                "catalog::int64": 1234567890123
            },
            "geometry":
            {
                "type": "Polygon",
                "coordinates" : [ [ [2,49],[2,49.1],[2.1,49.1],[2.1,49],[2,49] ] ]
            }
        }
    ]
}""")

    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/items/id/assets',
"""{
  "visual" : {
      "_links": {
        "_self": "visual_links_self"
      },
      "_permissions": ["download"],
      "files": {
        "http": {
          "_links": {
            "activate": "/vsimem/v1/catalogs/my_catalog/items/id/assets/visual/activate",
          },
          "location": "/vsimem/v1/catalogs/my_catalog/items/id/assets/visual/my.tiff",
          "status": "active",
          "expires_at": "2016-02-11T12:34:56.789",
          "type": "http"
        }
      },
      "mimetype": "visual_mimetype",
      "type": "visual"
  }
}""")


    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR, open_options = ['VERSION=v1', 'API_KEY=foo', 'FOLLOW_LINKS=YES', 'FILTER=_permissions=assets.visual:download'])
    gdal.SetConfigOption('PL_URL', None)
    lyr = ds.GetLayer(0)

    f = lyr.GetNextFeature()
    if f.GetFID() != 1 or f['id'] != 'id' or f['self_link'] != 'self' or \
       f['assets_link'] != '/vsimem/v1/catalogs/my_catalog/items/id/assets' or \
       f['created'] != '2016/02/11 12:34:56.789+00' or \
       f['float'] != 1.23 or f['string'] != 'string' or f['int32'] != 123 or \
       f['int64'] != 1234567890123 or \
       f['asset_visual_self_link'] != 'visual_links_self' or \
       f['asset_visual_permissions'] != '["download"]' or \
       f['asset_visual_activate_link'] != '/vsimem/v1/catalogs/my_catalog/items/id/assets/visual/activate' or \
       f['asset_visual_expires_at'] != '2016/02/11 12:34:56.789' or \
       f['asset_visual_product_link'] != '/vsimem/v1/catalogs/my_catalog/items/id/assets/visual/my.tiff' or \
       f['asset_visual_product_link_status'] != 'active' or \
       f['asset_visual_mimetype'] != 'visual_mimetype' or \
       f.GetGeometryRef().ExportToWkt() != 'MULTIPOLYGON (((2 49,2.0 49.1,2.1 49.1,2.1 49.0,2 49)))':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetFID() != 1:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Try raster access

    # Missing catalog
    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    with gdaltest.error_handler():
        ds_raster = gdal.OpenEx('PLScenes:', gdal.OF_RASTER, open_options = ['VERSION=v1', 'API_KEY=foo', 'SCENE=id'])
    gdal.SetConfigOption('PL_URL', None)
    if ds_raster is not None or gdal.GetLastErrorMsg().find('Missing catalog') < 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Invalid catalog
    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    with gdaltest.error_handler():
        ds_raster = gdal.OpenEx('PLScenes:', gdal.OF_RASTER, open_options = ['VERSION=v1', 'API_KEY=foo', 'CATALOG=invalid', 'SCENE=id'])
    gdal.SetConfigOption('PL_URL', None)

    # visual not an object
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/items/id/assets',
"""{ "visual": false }""")
    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    with gdaltest.error_handler():
        ds_raster = gdal.OpenEx('PLScenes:', gdal.OF_RASTER, open_options = ['VERSION=v1', 'API_KEY=foo', 'CATALOG=my_catalog', 'SCENE=id'])
    gdal.SetConfigOption('PL_URL', None)
    if ds_raster is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Missing files
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/items/id/assets',
"""{ "visual": { } }""")
    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    with gdaltest.error_handler():
        ds_raster = gdal.OpenEx('PLScenes:', gdal.OF_RASTER, open_options = ['VERSION=v1', 'API_KEY=foo', 'CATALOG=my_catalog', 'SCENE=id'])
    gdal.SetConfigOption('PL_URL', None)
    if ds_raster is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Empty files
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/items/id/assets',
"""{ "visual": { "files": "" } }""")
    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    with gdaltest.error_handler():
        ds_raster = gdal.OpenEx('PLScenes:', gdal.OF_RASTER, open_options = ['VERSION=v1', 'API_KEY=foo', 'CATALOG=my_catalog', 'SCENE=id'])
    gdal.SetConfigOption('PL_URL', None)
    if ds_raster is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Inactive file, and activation link not working
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/items/id/assets',
"""{
  "visual" : {
      "_links": {
        "_self": "visual_links_self"
      },
      "_permissions": ["download"],
      "files": {
        "http": {
          "_links": {
            "activate": "/vsimem/v1/catalogs/my_catalog/items/id/assets/visual/activate",
          },
          "status": "inactive",
          "type": "http"
        }
      },
      "mimetype": "visual_mimetype",
      "type": "visual"
  }
}""")
    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    with gdaltest.error_handler():
        ds_raster = gdal.OpenEx('PLScenes:', gdal.OF_RASTER, open_options = ['VERSION=v1', 'API_KEY=foo', 'CATALOG=my_catalog', 'SCENE=id', 'ACTIVATION_TIMEOUT=1'])
    gdal.SetConfigOption('PL_URL', None)
    if ds_raster is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # File in activation
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/items/id/assets',
"""{
  "visual" : {
      "_links": {
        "_self": "visual_links_self"
      },
      "_permissions": ["download"],
      "files": {
        "http": {
          "_links": {
            "activate": "/vsimem/v1/catalogs/my_catalog/items/id/assets/visual/activate",
          },
          "status": "activating",
          "type": "http"
        }
      },
      "mimetype": "visual_mimetype",
      "type": "visual"
  }
}""")
    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    with gdaltest.error_handler():
        ds_raster = gdal.OpenEx('PLScenes:', gdal.OF_RASTER, open_options = ['VERSION=v1', 'API_KEY=foo', 'CATALOG=my_catalog', 'SCENE=id', 'ACTIVATION_TIMEOUT=1'])
    gdal.SetConfigOption('PL_URL', None)
    if ds_raster is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/items/id/assets',
"""{
  "visual" : {
      "_links": {
        "_self": "visual_links_self"
      },
      "_permissions": ["download"],
      "files": {
        "http": {
          "_links": {
            "activate": "/vsimem/v1/catalogs/my_catalog/items/id/assets/visual/activate",
          },
          "location": "/vsimem/v1/catalogs/my_catalog/items/id/assets/visual/my.tiff",
          "status": "active",
          "expires_at": "2016-02-11T12:34:56.789",
          "type": "http"
        }
      },
      "mimetype": "visual_mimetype",
      "type": "visual"
  }
}""")

    # Missing /vsimem/v1/catalogs/my_catalog/items/id/assets/visual/my.tiff
    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    with gdaltest.error_handler():
        ds_raster = gdal.OpenEx('PLScenes:', gdal.OF_RASTER, open_options = ['VERSION=v1', 'API_KEY=foo', 'CATALOG=my_catalog', 'SCENE=id'])
    gdal.SetConfigOption('PL_URL', None)
    if ds_raster is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # JSon content for /vsimem/v1/catalogs/my_catalog/items/id/assets/visual/my.tiff
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/items/id/assets/visual/my.tiff',
                           """{}""")
    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    with gdaltest.error_handler():
        ds_raster = gdal.OpenEx('PLScenes:', gdal.OF_RASTER, open_options = ['VERSION=v1', 'API_KEY=foo', 'CATALOG=my_catalog', 'SCENE=id'])
    gdal.SetConfigOption('PL_URL', None)
    if ds_raster is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Missing /vsimem/v1/catalogs/my_catalog
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/items/id/assets/visual/my.tiff',
                           open('../gcore/data/byte.tif', 'rb').read())
    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    with gdaltest.error_handler():
        ds_raster = gdal.OpenEx('PLScenes:', gdal.OF_RASTER, open_options = ['VERSION=v1', 'API_KEY=foo', 'CATALOG=my_catalog', 'SCENE=id'])
    gdal.SetConfigOption('PL_URL', None)
    if ds_raster is None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Failed filter by scene id
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog',
                           """{"item_count": 2,
     "_links": { "items": "/vsimem/v1/catalogs/my_catalog/items/", "spec": "/vsimem/v1/catalogs/my_catalog/spec"},
     "id": "my_catalog"}""")
    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    ds_raster = gdal.OpenEx('PLScenes:', gdal.OF_RASTER, open_options = ['VERSION=v1', 'API_KEY=foo', 'CATALOG=my_catalog', 'SCENE=id'])
    gdal.SetConfigOption('PL_URL', None)
    if ds_raster is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds_raster = None

    # Test metadata items attached to dataset
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/items/id',
"""{
    "id": "id",
    "properties": {
        "catalog::int32": 123,
    },
}""")
    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    ds_raster = gdal.OpenEx('PLScenes:', gdal.OF_RASTER, open_options = ['VERSION=v1', 'API_KEY=foo', 'CATALOG=my_catalog', 'PRODUCT_TYPE=visual', 'SCENE=id'])
    gdal.SetConfigOption('PL_URL', None)
    if ds_raster is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds_raster.GetMetadataItem('int32') != '123':
        gdaltest.post_reason('fail')
        return 'fail'
    ds_raster = None

    # Test invalid product_type
    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    with gdaltest.error_handler():
        ds_raster = gdal.OpenEx('PLScenes:', gdal.OF_RASTER, open_options = ['VERSION=v1', 'API_KEY=foo', 'CATALOG=my_catalog', 'PRODUCT_TYPE=invalid', 'SCENE=id'])
    gdal.SetConfigOption('PL_URL', None)
    if ds_raster is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test subdatasets
    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    ds_raster = gdal.OpenEx('PLScenes:', gdal.OF_RASTER, open_options = ['VERSION=v1', 'API_KEY=foo', 'CATALOG=my_catalog', 'PRODUCT_TYPE=list', 'SCENE=id'])
    gdal.SetConfigOption('PL_URL', None)
    if len(ds_raster.GetSubDatasets()) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    ds_raster = None

    # Unsupported option
    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    with gdaltest.error_handler():
        ds_raster = gdal.OpenEx('PLScenes:unsupported=yes', gdal.OF_RASTER, open_options = ['VERSION=v1', 'API_KEY=foo', 'CATALOG=my_catalog', 'SCENE=id'])
    gdal.SetConfigOption('PL_URL', None)
    if ds_raster is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test catalog with vector access
    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    ds2 = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR, open_options = ['VERSION=v1', 'API_KEY=foo', 'CATALOG=my_catalog'])
    gdal.SetConfigOption('PL_URL', None)
    if ds2 is None or ds2.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    with gdaltest.error_handler():
        ds2 = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR, open_options = ['VERSION=v1', 'API_KEY=foo', 'CATALOG=invalid'])
    gdal.SetConfigOption('PL_URL', None)
    if ds2 is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.Unlink('/vsimem/v1/catalogs/my_catalog')
    gdal.Unlink('/vsimem/v1/catalogs/my_catalog/items/id/assets')
    gdal.Unlink('/vsimem/v1/catalogs/my_catalog/items/id/assets/visual/my.tiff')
    gdal.Unlink('/vsimem/v1/catalogs/my_catalog/items/id')


    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/items/id/assets',
"""{
  "visual" : {
      "_links": {
        "_self": "visual_links_self"
      },
      "_permissions": ["download"],
      "files": {
        "http": {
          "_links": {
            "activate": "/vsimem/v1/catalogs/my_catalog/items/id/assets/visual/activate",
          },
          "location": "/vsimem/v1/catalogs/my_catalog/items/id/assets/visual/my.tiff",
          "status": "active",
          "expires_at": "2016-02-11T12:34:56.789",
          "type": "http"
        }
      },
      "mimetype": "visual_mimetype",
      "type": "visual"
  }
}""")


    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/items_page2/?_page_size=100',
"""{
    "features" : [
        {
            "id": "id2"
        }
    ]
}""")

    f = lyr.GetNextFeature()
    if f.GetFID() != 2 or f['id'] != 'id2':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetFID() != 1:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    f = lyr.GetNextFeature()
    if f.GetFID() != 2:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/items/?_page_size=100&_permissions=assets.visual:download&geometry=POINT(2%2049)',
"""{
    "features" : [
        {
            "id": "id3"
        }
    ]
}""")

    # POINT spatial filter
    lyr.SetSpatialFilterRect(2,49,2,49)
    f = lyr.GetNextFeature()
    if f['id'] != 'id3':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/items/?_page_size=100&_permissions=assets.visual:download&geometry=POINT(2%2049)',
"""{"features" : [ { "id": "id3", "geometry": { "type": "Point", "coordinates": [2,49]} } }""")

    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/items/?_page_size=100&_permissions=assets.visual:download&geometry=POLYGON%20((2%2049,2.0%2049.1,2.1%2049.1,2.1%2049.0,2%2049))',
"""{
    "features" : [
        {
            "id": "id4"
        }
    ]
}""")

    # POLYGON spatial filter
    lyr.SetSpatialFilterRect(2,49,2.1,49.1)
    f = lyr.GetNextFeature()
    if f['id'] != 'id4':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Reset spatial filter
    lyr.SetSpatialFilter(0, None)
    f = lyr.GetNextFeature()
    if f['id'] != 'id':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # All world filter
    lyr.SetSpatialFilterRect(-1000,-1000,1000,1000)
    f = lyr.GetNextFeature()
    if f['id'] != 'id':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Reset spatial filter
    lyr.SetSpatialFilter(None)
    f = lyr.GetNextFeature()
    if f['id'] != 'id':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Test attribute filter on id (special case)
    lyr.SetAttributeFilter("id = 'filtered_id'")
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/items/filtered_id',
"""{
    "id": "filtered_id",
    "properties": {}
}""")

    f = lyr.GetNextFeature()
    if f['id'] != 'filtered_id':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Test attribute filter fully evaluated on server side.
    lyr.SetAttributeFilter("float >= 0 AND int32 < 3 AND float <= 1 AND string = 'foo' AND int32 > 1 AND created = '2016/02/11 12:34:56'")
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/items/?_page_size=100&_permissions=assets.visual:download&created=[2016-02-11T12:34:56Z:2016-02-11T12:34:57Z]&catalog::float=[0.00000000:1.00000000]&catalog::string=foo&catalog::int32=[1:3]',
"""{
    "features" : [
        {
            "id": "filtered_0",
            "properties": {
                "catalog::float": 0.5,
                "catalog::int32" : 3,
                "catalog::string": "foo",
                "created": "2016-02-11T12:34:56.789Z"
            }
        },
        {
            "id": "filtered_1",
            "properties": {
                "catalog::float": 0.5,
                "catalog::int32" : 2,
                "catalog::string": "foo",
                "created": "2016-02-11T12:34:56.789Z"
            }
        }
    ]
}""")
    f = lyr.GetNextFeature()
    if f['id'] != 'filtered_1':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Another one but with no range
    lyr.SetAttributeFilter("float > 0 AND int32 < 3 AND created > '2016/02/11 12:34:56'")
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/items/?_page_size=100&_permissions=assets.visual:download&created=[2016-02-11T12:34:56Z:]&catalog::float=[0.00000000:]&catalog::int32=[:3]',
"""{
    "features" : [
        {
            "id": "filtered_2",
            "properties": {
                "catalog::float": 0.5,
                "catalog::int32" : 2,
                "catalog::string": "foo",
                "created": "2016-02-11T12:34:56.789Z"
            }
        }
    ]
}""")
    f = lyr.GetNextFeature()
    if f['id'] != 'filtered_2':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Partly server / partly client
    lyr.SetAttributeFilter("int64 = 4 AND string = 'foo' AND int32 >= 3 AND int32 >= 3 AND float >= 0 AND float <= 2 AND float <= 3")
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/items/?_page_size=100&_permissions=assets.visual:download&catalog::float=[:3.00000000]&catalog::string=foo&catalog::int32=[3:]',
"""{
    "features" : [
        {
            "id": "filtered_3",
            "properties": {
                "catalog::int64" : 4,
                "catalog::int32" : 4,
                "catalog::float" : 1,
                "catalog::string": "foo",
            }
        }
    ]
}""")
    f = lyr.GetNextFeature()
    if f['id'] != 'filtered_3':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Completely client side
    lyr.SetAttributeFilter("int32 = 123 OR string = 'foo'")
    f = lyr.GetNextFeature()
    if f['id'] != 'id':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Reset attribute filter
    lyr.SetAttributeFilter(None)
    f = lyr.GetNextFeature()
    if f['id'] != 'id':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    gdal.Unlink('/vsimem/v1/catalogs')
    gdal.Unlink('/vsimem/v1/catalogs/my_catalog/spec')
    gdal.Unlink('/vsimem/v1/catalogs/my_catalog/items/id/assets')
    gdal.Unlink('/vsimem/v1/catalogs/my_catalog/items/?_page_size=100&_permissions=assets.visual:download')
    gdal.Unlink('/vsimem/v1/catalogs/my_catalog/items_page2/?_page_size=100')
    gdal.Unlink('/vsimem/v1/catalogs/my_catalog/items/?_page_size=100&_permissions=assets.visual:download&geometry=POINT(2%2049)')
    gdal.Unlink('/vsimem/v1/catalogs/my_catalog/items/?_page_size=100&_permissions=assets.visual:download&geometry=POLYGON%20((2%2049,2.0%2049.1,2.1%2049.1,2.1%2049.0,2%2049))')
    gdal.Unlink('/vsimem/v1/catalogs/my_catalog/items/filtered_id')
    gdal.Unlink('/vsimem/v1/catalogs/my_catalog/items/?_page_size=100&_permissions=assets.visual:download&created=[2016-02-11T12:34:56Z:2016-02-11T12:34:57Z]&catalog::float=[0.00000000:1.00000000]&catalog::string=foo&catalog::int32=[1:3]')
    gdal.Unlink('/vsimem/v1/catalogs/my_catalog/items/?_page_size=100&_permissions=assets.visual:download&created=[2016-02-11T12:34:56Z:]&catalog::float=[0.00000000:]&catalog::int32=[:3]')
    gdal.Unlink('/vsimem/v1/catalogs/my_catalog/items/?_page_size=100&_permissions=assets.visual:download&catalog::float=[:3.00000000]&catalog::string=foo&catalog::int32=[3:]')

    return 'success'

###############################################################################
# Test robustness to errors in V1 API

def ogr_plscenes_v1_errors():

    if gdaltest.plscenes_drv is None:
        return 'skip'

    # No PL_API_KEY
    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    old_key = gdal.GetConfigOption('PL_API_KEY')
    if old_key:
        gdal.SetConfigOption('PL_API_KEY', '')
    with gdaltest.error_handler():
        ds = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR, open_options = ['VERSION=v1'])
    if old_key:
        gdal.SetConfigOption('PL_API_KEY', old_key)
    gdal.SetConfigOption('PL_URL', None)
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Invalid option
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs', '{ "catalogs": [] }')
    with gdaltest.error_handler():
        gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
        ds = gdal.OpenEx('PLScenes:version=v1,api_key=foo,invalid=invalid', gdal.OF_VECTOR)
        gdal.SetConfigOption('PL_URL', None)
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Invalid JSON
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs', '{invalid_json')
    with gdaltest.error_handler():
        gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
        ds = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR, open_options = ['VERSION=v1', 'API_KEY=foo'])
        gdal.SetConfigOption('PL_URL', None)
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Not an object
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs', 'false')
    with gdaltest.error_handler():
        gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
        ds = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR, open_options = ['VERSION=v1', 'API_KEY=foo'])
        gdal.SetConfigOption('PL_URL', None)
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Lack of "catalogs"
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs', '{}')
    with gdaltest.error_handler():
        gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
        ds = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR, open_options = ['VERSION=v1', 'API_KEY=foo'])
        gdal.SetConfigOption('PL_URL', None)
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Invalid catalog objects
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs', """{"catalogs": [{}, [], null, {"id":null},
    {"id":"foo"},{"id":"foo", "_links":null},{"id":"foo", "_links":[]},{"id":"foo", "_links":{}},
    {"id":"foo", "_links":{"spec": []}}, {"id":"foo", "_links":{"spec": "x", "items": []}}]}""")
    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR, open_options = ['VERSION=v1', 'API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    if ds.GetLayerCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Invalid next URL
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs', '{"_links": { "_next": "/vsimem/inexisting" }, "catalogs": [{"item_count": 2, "_links": { "items": "/vsimem/v1/catalogs/my_catalog/items/", "spec": "/vsimem/v1/catalogs/my_catalog/spec"}, "id": "my_catalog"}]}')
    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR, open_options = ['VERSION=v1', 'API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    with gdaltest.error_handler():
        lyr_count = ds.GetLayerCount()
    if lyr_count != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/v1/catalogs', '{ "catalogs": [{ "_links": { "items": "/vsimem/v1/catalogs/my_catalog/items/", "spec": "/vsimem/invalid_spec"}, "id": "my_catalog"}]}')
    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR, open_options = ['VERSION=v1', 'API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        lyr.GetLayerDefn().GetFieldCount()

    gdal.FileFromMemBuffer('/vsimem/v1/catalogs', '{ "catalogs": [{ "_links": { "items": "/vsimem/v1/catalogs/my_catalog/items/", "spec": "/vsimem/v1/catalogs/my_catalog/spec"}, "id": "my_catalog"}]}')

    # Test various errors in spec
    for spec in [ '{}', # no path
                  '{ "paths": [] }', # bad type
                  '{ "paths": {} }', # no path for /vsimem/v1/catalogs/my_catalog/items/
                  '{ "paths": { "/catalogs/my_catalog/items/" : false } }', # wrong type
                  '{ "paths": { "/catalogs/my_catalog/items/" : {} } }', # no schema
                  """{ "paths": { "/catalogs/my_catalog/items/" : {"get": {
                        "responses": {
                            "200": {
                                "schema": {
                                    "$ref": "#/definitions/ItemPage"
                                }
                            }
                        }
                    }} } }""", # wrong link

                  """{ "paths": { "/catalogs/my_catalog/items/" : {"get": {
                        "responses": {
                            "200": {
                                "schema": {
                                    "$ref": false
                                }
                            }
                        }
                    }} } }""", # invalid type for $ref

                    """{ "paths": { "/catalogs/my_catalog/items/" : {"get": {
                        "responses": {
                            "200": {
                                "schema": {
                                    "$ref": "#/definitions/ItemPage"
                                }
                            }
                        }
                    }} },
                        "definitions" :
                        {
                            "ItemPage": {}
                        }
                        }""", # Cannot find ItemPage allOf

                    """{ "paths": { "/catalogs/my_catalog/items/" : {"get": {
                        "responses": {
                            "200": {
                                "schema": {
                                    "properties": false
                                }
                            }
                        }
                    }} }}""", # Cannot find ItemPage properties

                    """{ "paths": { "/catalogs/my_catalog/items/" : {"get": {
                        "responses": {
                            "200": {
                                "schema": {
                                    "properties": {}
                                }
                            }
                        }
                    }} }}""", # Cannot find ItemPage properties.features.items

                    """{ "paths": { "/catalogs/my_catalog/items/" : {"get": {
                        "responses": {
                            "200": {
                                "schema": {
                                    "properties": {
                                        "features": {
                                            "items": {
                                                "$ref": "#/definitions/Item"
                                            }
                                        }
                                     }
                               }
                            }
                        }
                       }}}}""", # Cannot find object 'Item' of '#/definitions/Item'

                    """{ "paths": { "/catalogs/my_catalog/items/" : {"get": {
                        "responses": {
                            "200": {
                                "schema": {
                                    "properties": {
                                        "features": {
                                            "items": {
                                            }
                                        }
                                     }
                               }
                            }
                        }
                       }}}}""", # Cannot find Item allOf

                    """{ "paths": { "/catalogs/my_catalog/items/" : {"get": {
                        "responses": {
                            "200": {
                                "schema": {
                                    "properties": {
                                        "features": {
                                            "items": {
                                                "properties": false
                                            }
                                        }
                                     }
                               }
                            }
                        }
                       }}}}""", # Cannot find Item properties

                    """{ "paths": { "/catalogs/my_catalog/items/" : {"get": {
                        "responses": {
                            "200": {
                                "schema": {
                                    "properties": {
                                        "features": {
                                            "items": {
                                                "properties": {
                                                }
                                            }
                                        }
                                     }
                               }
                            }
                        }
                       }}}}""", # Cannot find Item properties.properties

                    """{ "paths": { "/catalogs/my_catalog/items/" : {"get": {
                        "responses": {
                            "200": {
                                "schema": {
                                    "properties": {
                                        "features": {
                                            "items": {
                                                "properties": {
                                                    "properties": {
                                                        "$ref": "inexisting"
                                                    }
                                                }
                                            }
                                        }
                                     }
                               }
                            }
                        }
                       }}}}""", # Cannot expand ref inexisting

                    """{ "paths": { "/catalogs/my_catalog/items/" : {"get": {
                        "responses": {
                            "200": {
                                "schema": {
                                    "properties": {
                                        "features": {
                                            "items": {
                                                "properties": {
                                                    "properties": {
                                                    }
                                                }
                                            }
                                        }
                                     }
                               }
                            }
                        },
                        "parameters": false
                       }}}}""", # Invalid parameters

                    """{ "paths": { "/catalogs/my_catalog/items/" : {"get": {
                        "responses": {
                            "200": {
                                "schema": {
                                    "properties": {
                                        "features": {
                                            "items": {
                                                "properties": {
                                                    "properties": {
                                                    }
                                                }
                                            }
                                        }
                                     }
                               }
                            }
                        },
                        "parameters": [
                            null,
                            false,
                            {
                                "$ref": "inexisting2"
                            },
                            {},
                            {"name":false},
                            {"name":""},
                            {"name":"","in":false},
                            {"name":"","in":"foo"}
                        ]
                       }}}}""", # Invalid parameters

                        ]:
        gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
        ds = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR, open_options = ['VERSION=v1', 'API_KEY=foo'])
        gdal.SetConfigOption('PL_URL', None)
        lyr = ds.GetLayer(0)

        gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/spec', spec)
        with gdaltest.error_handler():
            field_count =lyr.GetLayerDefn().GetFieldCount()
        if field_count != 0:
          gdaltest.post_reason('fail')
          return 'fail'

    gdal.SetConfigOption('PL_URL', '/vsimem/v1/catalogs/')
    ds = gdal.OpenEx('PLScenes:', gdal.OF_VECTOR, open_options = ['VERSION=v1', 'API_KEY=foo'])
    gdal.SetConfigOption('PL_URL', None)
    lyr = ds.GetLayer(0)

    # Invalid index
    ds.GetLayer(-1)
    ds.GetLayer(1)

    with gdaltest.error_handler():
        ds.GetLayerByName('invalid_name')

    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/spec',
    """{ "paths": { "/catalogs/my_catalog/items/" : {"get": {
                        "responses": {
                            "200": {
                                "schema": {
                                    "properties": {
                                        "features": {
                                            "items": {
                                                "properties": {
                                                    "properties": {
                                                    }
                                                }
                                            }
                                        }
                                     }
                               }
                            }
                        }
                       }}}}""")

    # Cannot find /vsimem/v1/catalogs/my_catalog/items/?_page_size=250
    with gdaltest.error_handler():
        lyr.GetNextFeature()

    # Empty object
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/items/?_page_size=250', '{}')
    lyr.ResetReading()
    lyr.GetNextFeature()

    # null feature
    gdal.FileFromMemBuffer('/vsimem/v1/catalogs/my_catalog/items/?_page_size=250', '{ "features": [ null ] }')
    lyr.ResetReading()
    lyr.GetNextFeature()

    gdal.Unlink('/vsimem/v1/catalogs')
    gdal.Unlink('/vsimem/v1/catalogs/my_catalog/spec')
    gdal.Unlink('/vsimem/v1/catalogs/my_catalog/items/?_page_size=250')

    return 'success'

###############################################################################
# Test V1 API against real server

def ogr_plscenes_v1_live():

    if gdaltest.plscenes_drv is None:
        return 'skip'

    api_key = gdal.GetConfigOption('PL_API_KEY')
    if api_key is None:
        print('Skipping test as PL_API_KEY not defined')
        return 'skip'

    gdal.SetConfigOption('PLSCENES_PAGE_SIZE', '10')
    ds = ogr.Open('PLScenes:version=v1,filter=_permissions=assets.visual:download')
    gdal.SetConfigOption('PLSCENES_PAGE_SIZE', None)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr is None:
        gdaltest.post_reason('fail')
        return 'fail'

    f = lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    catalog = lyr.GetName()
    scene = f['id']

    lyr_defn = lyr.GetLayerDefn()
    acquired_field = lyr_defn.GetFieldIndex('acquired')
    if acquired_field < 0 or lyr_defn.GetFieldDefn(acquired_field).GetType() != ogr.OFTDateTime:
        gdaltest.post_reason('fail')
        return 'fail'

    if not f.IsFieldSet(acquired_field):
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    int_field = -1
    float_field = -1
    string_field = -1
    for i in range(lyr_defn.GetFieldCount()):
        typ = lyr_defn.GetFieldDefn(i).GetType()
        if int_field < 0 and typ == ogr.OFTInteger and f.IsFieldSet(i):
            int_field = i
        elif float_field < 0 and typ == ogr.OFTReal and f.IsFieldSet(i):
            float_field = i
        elif string_field < 0 and typ == ogr.OFTString and f.IsFieldSet(i):
            string_field = i

    filter = "acquired='%s'" % f.GetFieldAsString(acquired_field)
    if int_field >= 0:
        name = lyr_defn.GetFieldDefn(int_field).GetName()
        min = f.GetField(int_field) - 1
        max = f.GetField(int_field) + 1
        filter += ' AND %s >= %d AND %s <= %d' % (name, min, name, max)
    if float_field >= 0:
        name = lyr_defn.GetFieldDefn(float_field).GetName()
        min = f.GetField(float_field) - 0.01
        max = f.GetField(float_field) + 0.01
        filter += ' AND %s BETWEEN %f AND %f' % (name, min, max)
    if string_field >= 0:
        name = lyr_defn.GetFieldDefn(string_field).GetName()
        value = f.GetField(string_field)
        filter += " AND %s = '%s'" % (name, value)

    lyr.SetAttributeFilter(filter)
    f = lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = None

    ds = gdal.Open('PLScenes:version=v1,catalog=%s,scene=%s' % (catalog, scene))
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'


gdaltest_list = [
    ogr_plscenes_1,
    ogr_plscenes_2,
    ogr_plscenes_3,
    ogr_plscenes_4,
    ogr_plscenes_v1_catalog_no_paging,
    ogr_plscenes_v1_catalog_paging,
    ogr_plscenes_v1_nominal,
    ogr_plscenes_v1_errors,
    ogr_plscenes_v1_live
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_plscenes' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

