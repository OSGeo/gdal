#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  CartoDB driver testing.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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
import uuid

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import gdal
from osgeo import ogr
from osgeo import osr

###############################################################################
# Test if driver is available

def ogr_cartodb_init():

    ogrtest.cartodb_drv = None

    try:
        ogrtest.cartodb_drv = ogr.GetDriverByName('CartoDB')
    except:
        pass

    if ogrtest.cartodb_drv is None:
        return 'skip'

    return 'success'


###############################################################################
#

def ogr_cartodb_vsimem():
    if ogrtest.cartodb_drv is None:
        return 'skip'

    ogrtest.cartodb_api_key_ori = gdal.GetConfigOption('CARTODB_API_KEY')
    gdal.SetConfigOption('CARTODB_API_URL', '/vsimem/cartodb')
    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')
    
    gdal.PushErrorHandler()
    ds = ogr.Open('CARTODB:foo')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/cartodb&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
"""Content-Type: text/html\r

Error""")
    gdal.PushErrorHandler()
    ds = ogr.Open('CARTODB:foo')
    gdal.PopErrorHandler()
    if ds is not None or gdal.GetLastErrorMsg().find('HTML error page') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/cartodb&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
"""""")
    ds = ogr.Open('CARTODB:foo')
    if ds is not None:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/cartodb&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
"""{""")
    gdal.PushErrorHandler()
    ds = ogr.Open('CARTODB:foo')
    gdal.PopErrorHandler()
    if ds is not None or gdal.GetLastErrorMsg().find('JSON parsing error') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/cartodb&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
""" "not_expected_json" """)
    ds = ogr.Open('CARTODB:foo')
    if ds is not None:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'
        
    gdal.FileFromMemBuffer('/vsimem/cartodb&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
"""{ "error" : [ "bla"] }""")
    gdal.PushErrorHandler()
    ds = ogr.Open('CARTODB:foo')
    gdal.PopErrorHandler()
    if ds is not None or gdal.GetLastErrorMsg().find('Error returned by server : bla') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/cartodb&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
"""{ "fields" : null } """)
    ds = ogr.Open('CARTODB:foo')
    if ds is not None:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/cartodb&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
"""{ "fields" : "invalid" } """)
    ds = ogr.Open('CARTODB:foo')
    if ds is not None:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/cartodb&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
"""{ "fields" : {} } """)
    ds = ogr.Open('CARTODB:foo')
    if ds is not None:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/cartodb&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
"""{ "fields" : { "foo": "invalid" } } """)
    ds = ogr.Open('CARTODB:foo')
    if ds is not None:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/cartodb&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
"""{ "fields" : { "foo": {} } } """)
    ds = ogr.Open('CARTODB:foo')
    if ds is not None:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/cartodb&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
"""{ "fields" : { "foo": { "type" : null } } } """)
    ds = ogr.Open('CARTODB:foo')
    if ds is not None:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/cartodb&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
"""{ "fields" : { "foo": { "type" : {} } } } """)
    ds = ogr.Open('CARTODB:foo')
    if ds is not None:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail' 

    gdal.FileFromMemBuffer('/vsimem/cartodb&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
"""{ "fields" : { "foo": { "type" : "string" } } } """)
    ds = ogr.Open('CARTODB:foo')
    if ds is not None:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail' 

    gdal.FileFromMemBuffer('/vsimem/cartodb&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
"""{"rows":[ {"field1": "foo", "field2": "bar"} ],"fields":{"field1":{"type":"string"}, "field2":{"type":"string"}}}""")
    ds = ogr.Open('CARTODB:foo')
    if ds is not None:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail' 

    gdal.FileFromMemBuffer('/vsimem/cartodb&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
"""{"rows":[],"fields":{"current_schema":{"type":"string"}}}""")
    gdal.PushErrorHandler()
    ds = ogr.Open('CARTODB:foo')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail' 

    gdal.FileFromMemBuffer('/vsimem/cartodb&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
"""{"rows":[{"current_schema":"public"}],"fields":{"current_schema":{"type":"unknown(19)"}}}""")
    gdal.PushErrorHandler()
    ds = ogr.Open('CARTODB:foo')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail' 

    gdal.FileFromMemBuffer('/vsimem/cartodb&POSTFIELDS=q=SELECT CDB_UserTables() LIMIT 500 OFFSET 0',
"""{"rows":[{"cdb_usertables":"table1"}],"fields":{"cdb_usertables":{"type":"string"}}}""")
    ds = ogr.Open('CARTODB:foo')
    if ds is None or ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail' 

    gdal.PushErrorHandler()
    lyr_defn = ds.GetLayer(0).GetLayerDefn()
    gdal.PopErrorHandler()
    
    if lyr_defn.GetFieldCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail' 

    gdal.FileFromMemBuffer('/vsimem/cartodb&POSTFIELDS=q=SELECT * FROM "table1" LIMIT 0',
"""{"rows":[],"fields":{"strfield":{"type":"string"}, "realfield":{"type":"number"}, "boolfield":{"type":"boolean"}, "datefield":{"type":"date"}}}""")
    ds = ogr.Open('CARTODB:foo')
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    if lyr_defn.GetFieldCount() != 4:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr_defn.GetFieldDefn(0).GetName() != 'strfield' or \
       lyr_defn.GetFieldDefn(0).GetType() != ogr.OFTString:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr_defn.GetFieldDefn(1).GetName() != 'realfield' or \
       lyr_defn.GetFieldDefn(1).GetType() != ogr.OFTReal:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr_defn.GetFieldDefn(2).GetName() != 'boolfield' or \
       lyr_defn.GetFieldDefn(2).GetType() != ogr.OFTInteger or \
       lyr_defn.GetFieldDefn(2).GetSubType() != ogr.OFSTBoolean:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr_defn.GetFieldDefn(3).GetName() != 'datefield' or \
       lyr_defn.GetFieldDefn(3).GetType() != ogr.OFTDateTime:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/cartodb&POSTFIELDS=q=SELECT * FROM "table1" LIMIT 500 OFFSET 0',
"""{"rows":[{ "strfield": "foo", "realfield": 1.23, "boolfield": true, "datefield": "2015-04-24T12:34:56.123Z" }],"fields":{"strfield":{"type":"string"}, "realfield":{"type":"number"}, "boolfield":{"type":"boolean"}, "datefield":{"type":"date"}}}""")
    f = lyr.GetNextFeature()
    if f['strfield'] != 'foo' or f['realfield'] != 1.23 or f['boolfield'] != 1 or \
       f['datefield'] != '2015/04/24 12:34:56.123+00':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    gdal.SetConfigOption('CARTODB_API_KEY', 'foo')
    gdal.FileFromMemBuffer('/vsimem/cartodb&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0&api_key=foo',
"""{"rows":[{"current_schema":"public"}],"fields":{"current_schema":{"type":"unknown(19)"}}}""")
    gdal.FileFromMemBuffer('/vsimem/cartodb&POSTFIELDS=q=SELECT CDB_UserTables() LIMIT 500 OFFSET 0&api_key=foo',
"""{"rows":[{"cdb_usertables":"table1"}],"fields":{"cdb_usertables":{"type":"string"}}}""")
    ds = ogr.Open('CARTODB:foo')
    gdal.PushErrorHandler()
    lyr_defn = ds.GetLayer(0).GetLayerDefn()
    gdal.PopErrorHandler()
    if lyr_defn.GetFieldCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
        
    get_full_details_fields_url = """/vsimem/cartodb&POSTFIELDS=q=SELECT DISTINCT a.attname, t.typname, a.attlen, format_type(a.atttypid,a.atttypmod), a.attnum, a.attnotnull, a.atthasdef FROM pg_class c, pg_attribute a, pg_type t, pg_namespace n WHERE c.relname = 'table1' AND a.attnum > 0 AND a.attrelid = c.oid AND a.atttypid = t.oid AND c.relnamespace=n.oid AND n.nspname= 'public' ORDER BY a.attnum LIMIT 500 OFFSET 0&api_key=foo"""
    gdal.FileFromMemBuffer(get_full_details_fields_url, '')
    ds = ogr.Open('CARTODB:foo')
    gdal.PushErrorHandler()
    lyr_defn = ds.GetLayer(0).GetLayerDefn()
    gdal.PopErrorHandler()
    if lyr_defn.GetFieldCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer(get_full_details_fields_url,
    """{"rows":[{"attname":"foo"}], "fields":{"attname":{"type":"string"}}}""")
    ds = ogr.Open('CARTODB:foo')
    lyr = ds.GetLayer(0)
    gdal.PushErrorHandler()
    lyr_defn = lyr.GetLayerDefn()
    gdal.PopErrorHandler()
    if lyr_defn.GetFieldCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.PushErrorHandler()
    f = lyr.GetFeature(0)
    gdal.PopErrorHandler()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer(get_full_details_fields_url,
    """{"rows":[{"attname":"strfield", "typname":"varchar", "attnotnull": true, "atthasdef": true},
               {"attname":"intfield", "typname":"int4"},
               {"attname":"doublefield", "typname":"float"},
               {"attname":"boolfield", "typname":"bool"},
               {"attname":"datetimefield", "typname":"timestamp"},
               {"attname":"cartodb_id","typname":"integer"},
               {"attname":"created_at","typname":"date"},
               {"attname":"updated_at","typname":"date"},
               {"attname":"my_geom","typname":"geometry"},
               {"attname":"the_geom_webmercator","typname":"geometry"}],
        "fields":{"attname":{"type":"string"},
                  "typname":{"type":"string"},
                  "attlen":{"type":"number"},
                  "format_type":{"type":"string"},
                  "attnum":{"type":"number"},
                  "attnotnull":{"type":"boolean"},
                  "atthasdef":{"type":"boolean"}}}""")

    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=SELECT a.attname, pg_get_expr(def.adbin, c.oid) FROM pg_attrdef def, pg_class c, pg_attribute a, pg_type t, pg_namespace n WHERE c.relname = 'table1' AND a.attnum > 0 AND a.attrelid = c.oid AND a.atttypid = t.oid AND c.relnamespace=n.oid AND def.adrelid = c.oid AND def.adnum = a.attnum AND n.nspname= 'public' ORDER BY a.attnum LIMIT 500 OFFSET 0&api_key=foo""",
    """{"rows":[{"attname": "strfield", "pg_get_expr": "def_value"}],
        "fields":{"attname":{"type":"string"},
                  "pg_get_expr":{"type":"string"}}}""")

    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=SELECT srid, srtext FROM spatial_ref_sys WHERE srid IN (SELECT Find_SRID('public', 'table1', 'my_geom'))&api_key=foo""",
    """{"rows":[{"srid":4326, "srtext":"GEOGCS[\\"WGS 84\\",DATUM[\\"WGS_1984\\",SPHEROID[\\"WGS 84\\",6378137,298.257223563,AUTHORITY[\\"EPSG\\",\\"7030\\"]],AUTHORITY[\\"EPSG\\",\\"6326\\"]],PRIMEM[\\"Greenwich\\",0,AUTHORITY[\\"EPSG\\",\\"8901\\"]],UNIT[\\"degree\\",0.0174532925199433,AUTHORITY[\\"EPSG\\",\\"9122\\"]],AUTHORITY[\\"EPSG\\",\\"4326\\"]]"}],
        "fields":{"srid":{"type":"number"},
                  "srtext":{"type":"string"}}}""")

    ds = ogr.Open('CARTODB:foo')
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    if lyr_defn.GetFieldCount() != 5:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr_defn.GetFieldDefn(0).GetName() != 'strfield' or \
       lyr_defn.GetFieldDefn(0).GetType() != ogr.OFTString or \
       lyr_defn.GetFieldDefn(0).IsNullable() or \
       lyr_defn.GetFieldDefn(0).GetDefault() != 'def_value':
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr_defn.GetGeomFieldCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr_defn.GetGeomFieldDefn(0).GetName() != 'my_geom':
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr_defn.GetGeomFieldDefn(0).GetSpatialRef().ExportToWkt().find('4326') < 0:
        gdaltest.post_reason('fail')
        return 'fail'
  
    gdal.PushErrorHandler()
    fc = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    if fc != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=SELECT COUNT(*) FROM "table1"&api_key=foo""",
        """{"rows":[{"foo":1}],
            "fields":{"foo":{"type":"number"}}}""")
    gdal.PushErrorHandler()
    fc = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    if fc != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=SELECT COUNT(*) FROM "table1"&api_key=foo""",
        """{"rows":[{"count":9876543210}],
            "fields":{"count":{"type":"number"}}}""")
    if lyr.GetFeatureCount() != 9876543210:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.PushErrorHandler()
    extent = lyr.GetExtent()
    gdal.PopErrorHandler()
    if extent != (0,0,0,0):
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=SELECT ST_Extent("my_geom") FROM "table1"&api_key=foo""",
        """{"rows":[{"foo":1}],
            "fields":{"foo":{"type":"number"}}}""")

    gdal.PushErrorHandler()
    extent = lyr.GetExtent()
    gdal.PopErrorHandler()
    if extent != (0,0,0,0):
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=SELECT ST_Extent("my_geom") FROM "table1"&api_key=foo""",
        """{"rows":[{"st_extent":""}],
            "fields":{"st_extent":{"type":"string"}}}""")
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    lyr.GetExtent()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=SELECT ST_Extent("my_geom") FROM "table1"&api_key=foo""",
        """{"rows":[{"st_extent":"("}],
            "fields":{"st_extent":{"type":"string"}}}""")
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    lyr.GetExtent()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=SELECT ST_Extent("my_geom") FROM "table1"&api_key=foo""",
        """{"rows":[{"st_extent":"BOX()"}],
            "fields":{"st_extent":{"type":"string"}}}""")
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    lyr.GetExtent()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=SELECT ST_Extent("my_geom") FROM "table1"&api_key=foo""",
        """{"rows":[{"st_extent":"BOX(0,1,2,3)"}],
            "fields":{"st_extent":{"type":"string"}}}""")
    if lyr.GetExtent() != (0.0, 2.0, 1.0, 3.0):
        gdaltest.post_reason('fail')
        print(lyr.GetExtent())
        return 'fail'

    gdal.PushErrorHandler()
    f = lyr.GetFeature(0)
    gdal.PopErrorHandler()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=SELECT * FROM "table1" WHERE "cartodb_id" = 0&api_key=foo""",
        """""")

    gdal.PushErrorHandler()
    f = lyr.GetFeature(0)
    gdal.PopErrorHandler()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=SELECT * FROM "table1" WHERE "cartodb_id" = 0&api_key=foo""",
        """{"rows":[{"st_extent":"BOX(0,1,2,3)"}],
            "fields":{"st_extent":{"type":"string"}}}""")

    f = lyr.GetFeature(0)
    if f.GetFID() != -1 or f.IsFieldSet(0):
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=SELECT * FROM "table1" WHERE "cartodb_id" = 0&api_key=foo""",
        """{"rows":[{"cartodb_id":0}],
            "fields":{"cartodb_id":{"type":"numeric"}}}""")

    f = lyr.GetFeature(0)
    if f.GetFID() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr.ResetReading()
    gdal.PushErrorHandler()
    f = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=SELECT * FROM "table1" ORDER BY "cartodb_id" ASC LIMIT 500 OFFSET 0&api_key=foo""",
        """{"rows":[{"cartodb_id":0}],
            "fields":{"cartodb_id":{"type":"numeric"}}}""")
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetFID() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.SetConfigOption('CARTODB_PAGE_SIZE', '2')
    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=SELECT * FROM "table1" ORDER BY "cartodb_id" ASC LIMIT 2 OFFSET 0&api_key=foo""",
        """{"rows":[{"cartodb_id":0},{"cartodb_id":10}],
            "fields":{"cartodb_id":{"type":"numeric"}}}""")
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetFID() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetFID() != 10:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=SELECT * FROM "table1" ORDER BY "cartodb_id" ASC LIMIT 2 OFFSET 2&api_key=foo""",
        """{"rows":[{"cartodb_id":20}],
            "fields":{"cartodb_id":{"type":"numeric"}}}""")
    f = lyr.GetNextFeature()
    if f.GetFID() != 20:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr.SetAttributeFilter('strfield is NULL')

    gdal.PushErrorHandler()
    fc = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    if fc != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=SELECT * FROM "table1" WHERE strfield is NULL ORDER BY "cartodb_id" ASC LIMIT 2 OFFSET 0&api_key=foo""",
        """{"rows":[{"cartodb_id":20}],
            "fields":{"cartodb_id":{"type":"numeric"}}}""")
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=SELECT COUNT(*) FROM "table1" WHERE strfield is NULL&api_key=foo""",
        """{"rows":[{"count":9876543210}],
            "fields":{"count":{"type":"number"}}}""")
    if lyr.GetFeatureCount() != 9876543210:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr.SetSpatialFilterRect(-180,-90,180,90)

    gdal.PushErrorHandler()
    fc = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    if fc != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=SELECT * FROM "table1" WHERE "my_geom" %26%26 'BOX3D(-180 -90, 180 90)'::box3d AND strfield is NULL ORDER BY "cartodb_id" ASC LIMIT 2 OFFSET 0&api_key=foo""",
        """{"rows":[{"cartodb_id":20, "my_geom": "010100000000000000000000400000000000804840" }],
            "fields":{"cartodb_id":{"type":"numeric"}, "my_geom":{"type":"string"}}}""")

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f is None or f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)':
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.PushErrorHandler()
    fc = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    if fc != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=SELECT COUNT(*) FROM "table1" WHERE "my_geom" %26%26 'BOX3D(-180 -90, 180 90)'::box3d AND strfield is NULL&api_key=foo""",
        """{"rows":[{"count":9876543210}],
            "fields":{"count":{"type":"number"}}}""")
    if lyr.GetFeatureCount() != 9876543210:
        gdaltest.post_reason('fail')
        return 'fail'

    # Not permitted in read-only mode
    f = ogr.Feature(lyr.GetLayerDefn())
    gdal.PushErrorHandler()
    lyr.CreateFeature(f)
    lyr.SetFeature(f)
    lyr.DeleteFeature(0)
    gdal.PopErrorHandler()

    ds = None

    gdal.SetConfigOption('CARTODB_PAGE_SIZE', None)
    ds = ogr.Open('CARTODB:foo', update = 1)
    lyr = ds.CreateLayer('MY_LAYER')

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    lyr.GetNextFeature()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    ds = ogr.Open('CARTODB:foo', update = 1)
    gdal.SetConfigOption('CARTODB_MAX_CHUNK_SIZE', '0')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    lyr = ds.CreateLayer('MY_LAYER', srs = sr)
    fld_defn = ogr.FieldDefn('STRFIELD', ogr.OFTString)
    fld_defn.SetNullable(0)
    fld_defn.SetDefault("'DEFAULT VAL'")
    fld_defn.SetWidth(20)
    lyr.CreateField(fld_defn)
    
    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=CREATE TABLE "my_layer" ( cartodb_id SERIAL,the_geom GEOMETRY(GEOMETRY, 4326), the_geom_webmercator GEOMETRY(GEOMETRY, 3857),"strfield" VARCHAR NOT NULL DEFAULT 'DEFAULT VAL',PRIMARY KEY (cartodb_id) );DROP SEQUENCE IF EXISTS "my_layer_cartodb_id_seq" CASCADE;CREATE SEQUENCE "my_layer_cartodb_id_seq" START 1;ALTER TABLE "my_layer" ALTER COLUMN cartodb_id SET DEFAULT nextval('"my_layer_cartodb_id_seq"')&api_key=foo""",
        """{"rows":[],
            "fields":{}}""")
    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=SELECT cdb_cartodbfytable('my_layer')&api_key=foo""",
        """{"rows":[],
            "fields":{}}""")
    
    f = ogr.Feature(lyr.GetLayerDefn())
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
        
    fld_defn = ogr.FieldDefn('INTFIELD', ogr.OFTInteger)
    gdal.PushErrorHandler()
    ret = lyr.CreateField(fld_defn)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    
    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=ALTER TABLE "my_layer" ADD COLUMN "intfield" INTEGER&api_key=foo""",
        """{"rows":[],
            "fields":{}}""")
    if lyr.CreateField(fld_defn) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    fld_defn = ogr.FieldDefn('boolfield', ogr.OFTInteger)
    fld_defn.SetSubType(ogr.OFSTBoolean)
    
    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=ALTER TABLE "my_layer" ADD COLUMN "boolfield" BOOLEAN&api_key=foo""",
        """{"rows":[],
            "fields":{}}""")
    if lyr.CreateField(fld_defn) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    f = ogr.Feature(lyr.GetLayerDefn())
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('strfield', 'foo')
    f.SetField('intfield', 1)
    f.SetField('boolfield', 1)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2 49)'))
    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=INSERT INTO "my_layer" ("strfield", "intfield", "boolfield", "the_geom", "cartodb_id") VALUES ('foo', 1, 't', '0101000020E610000000000000000000400000000000804840', nextval('my_layer_cartodb_id_seq')) RETURNING "cartodb_id"&api_key=foo""",
        """{"rows":[ {"cartodb_id": 1} ],
            "fields":{"cartodb_id":{"type":"integer"}}}""")
    ret = lyr.CreateFeature(f)
    if ret != 0 or f.GetFID() != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    f.SetFID(-1)
    gdal.PushErrorHandler()
    ret = lyr.SetFeature(f)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    f.SetFID(3)
    gdal.PushErrorHandler()
    ret = lyr.SetFeature(f)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=UPDATE "my_layer" SET "strfield" = 'foo', "intfield" = 1, "boolfield" = 't', "the_geom" = '0101000020E610000000000000000000400000000000804840' WHERE "cartodb_id" = 3&api_key=foo""",
        """{"total_rows": 0}""")
    ret = lyr.SetFeature(f)
    if ret != ogr.OGRERR_NON_EXISTING_FEATURE:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=UPDATE "my_layer" SET "strfield" = 'foo', "intfield" = 1, "boolfield" = 't', "the_geom" = '0101000020E610000000000000000000400000000000804840' WHERE "cartodb_id" = 3&api_key=foo""",
        """{"total_rows": 1}""")
    ret = lyr.SetFeature(f)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    f = ogr.Feature(lyr.GetLayerDefn())
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=INSERT INTO "my_layer"  DEFAULT VALUES RETURNING "cartodb_id"&api_key=foo""",
        """{"rows":[ {"cartodb_id": 4} ],
            "fields":{"cartodb_id":{"type":"integer"}}}""")
    ret = lyr.CreateFeature(f)
    if ret != 0 or f.GetFID() != 4:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.PushErrorHandler()
    ret = lyr.DeleteFeature(0)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=DELETE FROM "my_layer" WHERE "cartodb_id" = 0&api_key=foo""",
        """{"total_rows": 0}""")
    ret = lyr.DeleteFeature(0)
    if ret != ogr.OGRERR_NON_EXISTING_FEATURE:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/cartodb&POSTFIELDS=q=DELETE FROM "my_layer" WHERE "cartodb_id" = 0&api_key=foo""",
        """{"total_rows": 1}""")
    ret = lyr.DeleteFeature(0)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
#

def ogr_cartodb_vsimem_cleanup():
    if ogrtest.cartodb_drv is None:
        return 'skip'

    gdal.SetConfigOption('CARTODB_API_URL', None)
    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', None)
    gdal.SetConfigOption('CARTODB_PAGE_SIZE', None)
    gdal.SetConfigOption('CARTODB_MAX_CHUNK_SIZE', None)
    gdal.SetConfigOption('CARTODB_API_KEY', ogrtest.cartodb_api_key_ori)

    for f in gdal.ReadDir('/vsimem/'):
        gdal.Unlink('/vsimem/' + f)

    return 'success'

###############################################################################
#  Run test_ogrsf

def ogr_cartodb_test_ogrsf():
    if ogrtest.cartodb_drv is None:
        return 'skip'

    ogrtest.cartodb_test_server = 'https://gdalautotest2.cartodb.com'
   
    if gdaltest.gdalurlopen(ogrtest.cartodb_test_server) is None:
        print('cannot open %s' % ogrtest.cartodb_test_server)
        ogrtest.cartodb_drv = None
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' --config CARTODB_HTTPS NO -ro "CARTODB:gdalautotest2 tables=tm_world_borders_simpl_0_3"')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test if driver is available

def ogr_cartodb_rw_init():

    ogrtest.cartodb_drv = None
    
    ogrtest.cartodb_connection = gdal.GetConfigOption('CARTODB_CONNECTION')
    if ogrtest.cartodb_connection is None:
        print('CARTODB_CONNECTION missing')
        return 'skip'
    if gdal.GetConfigOption('CARTODB_API_KEY') is None:
        print('CARTODB_API_KEY missing')
        return 'skip'

    try:
        ogrtest.cartodb_drv = ogr.GetDriverByName('CartoDB')
    except:
        pass

    if ogrtest.cartodb_drv is None:
        return 'skip'

    return 'success'

###############################################################################
# Read/write/update test

def ogr_cartodb_rw_1():

    if ogrtest.cartodb_drv is None:
        return 'skip'

    ds = ogr.Open(ogrtest.cartodb_connection, update = 1)
    if ds is None:
        return 'fail'

    a_uuid = str(uuid.uuid1()).replace('-', '_')
    lyr_name = "LAYER_" + a_uuid

    # No-op
    lyr = ds.CreateLayer(lyr_name)
    ds.DeleteLayer(ds.GetLayerCount()-1)

    # Differed table creation
    lyr = ds.CreateLayer(lyr_name)
    lyr.CreateField(ogr.FieldDefn("STRFIELD", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("intfield", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("int64field", ogr.OFTInteger64))
    lyr.CreateField(ogr.FieldDefn("doublefield", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("dt", ogr.OFTDateTime))
    fd = ogr.FieldDefn("bool", ogr.OFTInteger)
    fd.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(fd)

    f = ogr.Feature(lyr.GetLayerDefn())

    lyr.CreateFeature(f)
    f.SetField('STRFIELD', "fo'o")
    f.SetField('intfield', 123)
    f.SetField('int64field', 12345678901234)
    f.SetField('doublefield', 1.23)
    f.SetField('dt', '2014/12/04 12:34:56')
    f.SetField('bool', 0)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (1 2)'))
    lyr.CreateFeature(f)

    f.SetField('intfield', 456)
    f.SetField('bool', 1)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (3 4)'))
    lyr.SetFeature(f)
    fid = f.GetFID()

    ds = None

    ds = ogr.Open(ogrtest.cartodb_connection, update = 1)
    lyr_name = "layer_" + a_uuid
    found = False
    for lyr in ds:
        if lyr.GetName() == lyr_name:
            found = True
    if not found:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.GetLayerByName(lyr_name)
    found = False
    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        if lyr.GetLayerDefn().GetFieldDefn(i).GetName() == 'strfield':
            found = True
    if not found:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetFeature(fid)
    if f.GetField('strfield') != "fo'o" or \
       f.GetField('intfield') != 456 or \
       f.GetField('int64field') != 12345678901234 or \
       f.GetField('doublefield') != 1.23 or \
       f.GetField('dt') != '2014/12/04 12:34:56+00' or \
       f.GetField('bool') != 1 or \
       f.GetGeometryRef().ExportToWkt() != 'POINT (3 4)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        ds.ExecuteSQL("DELLAYER:" + lyr_name)
        return 'fail'

    lyr.DeleteFeature(fid)
    f = lyr.GetFeature(fid)
    if f is not None:
        gdaltest.post_reason('fail')
        ds.ExecuteSQL("DELLAYER:" + lyr_name)
        return 'fail'

    # Non-differed field creation
    lyr.CreateField(ogr.FieldDefn("otherstrfield", ogr.OFTString))

    ds.ExecuteSQL("DELLAYER:" + lyr_name)

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = ds.CreateLayer(lyr_name, geom_type = ogr.wkbMultiPolygon, srs = srs)
    lyr.GetNextFeature()
    ds.ExecuteSQL("DELLAYER:" + lyr_name)
    
    # Layer without geometry
    lyr = ds.CreateLayer(lyr_name, geom_type = ogr.wkbNone)
    fd = ogr.FieldDefn("nullable", ogr.OFTString)
    lyr.CreateField(fd)
    fd = ogr.FieldDefn("not_nullable", ogr.OFTString)
    fd.SetNullable(0)
    lyr.CreateField(fd)

    field_defn = ogr.FieldDefn( 'field_string', ogr.OFTString )
    field_defn.SetDefault("'a''b'")
    lyr.CreateField(field_defn)
    
    field_defn = ogr.FieldDefn( 'field_datetime_with_default', ogr.OFTDateTime )
    field_defn.SetDefault("CURRENT_TIMESTAMP")
    lyr.CreateField(field_defn)
    
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('not_nullable', 'foo')
    lyr.CreateFeature(f)
    f = None
    ds = None

    ds = ogr.Open(ogrtest.cartodb_connection, update = 1)
    lyr = ds.GetLayerByName(lyr_name)
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('nullable')).IsNullable() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('not_nullable')).IsNullable() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_string')).GetDefault() != "'a''b'":
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime_with_default')).GetDefault() != 'CURRENT_TIMESTAMP':
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f is None or f.GetField('field_string') != 'a\'b' or not f.IsFieldSet('field_datetime_with_default'):
        gdaltest.post_reason('fail')
        ds.ExecuteSQL("DELLAYER:" + lyr_name)
        return 'fail'
    ds.ExecuteSQL("DELLAYER:" + lyr_name)

    return 'success'

gdaltest_list = [ 
    ogr_cartodb_init,
    ogr_cartodb_vsimem,
    ogr_cartodb_vsimem_cleanup,
    ogr_cartodb_test_ogrsf
    ]


gdaltest_rw_list = [
    ogr_cartodb_rw_init,
    ogr_cartodb_rw_1,
]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_cartodb' )

    if gdal.GetConfigOption('CARTODB_CONNECTION') is None:
        gdaltest.run_tests( gdaltest_list )
    else:
        gdaltest.run_tests( gdaltest_rw_list )

    gdaltest.summarize()
