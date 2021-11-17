#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Carto driver testing.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
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

import uuid


import gdaltest
import ogrtest
from osgeo import gdal
from osgeo import ogr
from osgeo import osr
import pytest


pytestmark = pytest.mark.require_driver('Carto')

###############################################################################
#

def test_ogr_carto_vsimem():

    ogrtest.carto_api_key_ori = gdal.GetConfigOption('CARTO_API_KEY')
    gdal.SetConfigOption('CARTO_API_URL', '/vsimem/carto')
    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    gdal.FileFromMemBuffer('/vsimem/carto&POSTFIELDS=q=SELECT postgis_version() LIMIT 500 OFFSET 0&api_key=foo',
                           """{"rows":[{"postgis_version":"2.1 USE_GEOS=1 USE_PROJ=1 USE_STATS=1"}],"time":0.001,"fields":{"postgis_version":{"type":"string"}},"total_rows":1}""")

    gdal.PushErrorHandler()
    ds = ogr.Open('CARTO:foo')
    gdal.PopErrorHandler()
    assert ds is None

    gdal.FileFromMemBuffer('/vsimem/carto&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
                           """Content-Type: text/html\r

Error""")
    gdal.PushErrorHandler()
    ds = ogr.Open('CARTO:foo')
    gdal.PopErrorHandler()
    assert ds is None and gdal.GetLastErrorMsg().find('HTML error page') >= 0

    gdal.FileFromMemBuffer('/vsimem/carto&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
                           """""")
    ds = ogr.Open('CARTO:foo')
    assert ds is None, gdal.GetLastErrorMsg()

    gdal.FileFromMemBuffer('/vsimem/carto&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
                           """{""")
    gdal.PushErrorHandler()
    ds = ogr.Open('CARTO:foo')
    gdal.PopErrorHandler()
    assert ds is None and gdal.GetLastErrorMsg().find('JSON parsing error') >= 0

    gdal.FileFromMemBuffer('/vsimem/carto&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
                           """ "not_expected_json" """)
    ds = ogr.Open('CARTO:foo')
    assert ds is None, gdal.GetLastErrorMsg()

    gdal.FileFromMemBuffer('/vsimem/carto&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
                           """{ "error" : [ "bla"] }""")
    gdal.PushErrorHandler()
    ds = ogr.Open('CARTO:foo')
    gdal.PopErrorHandler()
    assert ds is None and gdal.GetLastErrorMsg().find('Error returned by server : bla') >= 0

    gdal.FileFromMemBuffer('/vsimem/carto&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
                           """{ "fields" : null } """)
    ds = ogr.Open('CARTO:foo')
    assert ds is None, gdal.GetLastErrorMsg()

    gdal.FileFromMemBuffer('/vsimem/carto&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
                           """{ "fields" : "invalid" } """)
    ds = ogr.Open('CARTO:foo')
    assert ds is None, gdal.GetLastErrorMsg()

    gdal.FileFromMemBuffer('/vsimem/carto&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
                           """{ "fields" : {} } """)
    ds = ogr.Open('CARTO:foo')
    assert ds is None, gdal.GetLastErrorMsg()

    gdal.FileFromMemBuffer('/vsimem/carto&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
                           """{ "fields" : { "foo": "invalid" } } """)
    ds = ogr.Open('CARTO:foo')
    assert ds is None, gdal.GetLastErrorMsg()

    gdal.FileFromMemBuffer('/vsimem/carto&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
                           """{ "fields" : { "foo": {} } } """)
    ds = ogr.Open('CARTO:foo')
    assert ds is None, gdal.GetLastErrorMsg()

    gdal.FileFromMemBuffer('/vsimem/carto&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
                           """{ "fields" : { "foo": { "type" : null } } } """)
    ds = ogr.Open('CARTO:foo')
    assert ds is None, gdal.GetLastErrorMsg()

    gdal.FileFromMemBuffer('/vsimem/carto&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
                           """{ "fields" : { "foo": { "type" : {} } } } """)
    ds = ogr.Open('CARTO:foo')
    assert ds is None, gdal.GetLastErrorMsg()

    gdal.FileFromMemBuffer('/vsimem/carto&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
                           """{ "fields" : { "foo": { "type" : "string" } } } """)
    ds = ogr.Open('CARTO:foo')
    assert ds is None, gdal.GetLastErrorMsg()

    gdal.FileFromMemBuffer('/vsimem/carto&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
                           """{"rows":[ {"field1": "foo", "field2": "bar"} ],"fields":{"field1":{"type":"string"}, "field2":{"type":"string"}}}""")
    ds = ogr.Open('CARTO:foo')
    assert ds is None, gdal.GetLastErrorMsg()

    gdal.FileFromMemBuffer('/vsimem/carto&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
                           """{"rows":[],"fields":{"current_schema":{"type":"string"}}}""")
    gdal.PushErrorHandler()
    ds = ogr.Open('CARTO:foo')
    gdal.PopErrorHandler()
    assert ds is None, gdal.GetLastErrorMsg()

    gdal.FileFromMemBuffer('/vsimem/carto&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0',
                           """{"rows":[{"current_schema":"public"}],"fields":{"current_schema":{"type":"unknown(19)"}}}""")
    gdal.PushErrorHandler()
    ds = ogr.Open('CARTO:foo')
    gdal.PopErrorHandler()
    assert ds is None, gdal.GetLastErrorMsg()

    gdal.FileFromMemBuffer('/vsimem/carto&POSTFIELDS=q=SELECT CDB_UserTables() LIMIT 500 OFFSET 0',
                           """{"rows":[{"cdb_usertables":"table1"}],"fields":{"cdb_usertables":{"type":"string"}}}""")
    ds = ogr.Open('CARTO:foo')
    assert ds is not None and ds.GetLayerCount() == 1, gdal.GetLastErrorMsg()

    gdal.PushErrorHandler()
    lyr_defn = ds.GetLayer(0).GetLayerDefn()
    gdal.PopErrorHandler()

    assert lyr_defn.GetFieldCount() == 0

    # Empty layer
    gdal.FileFromMemBuffer('/vsimem/carto&POSTFIELDS=q=SELECT * FROM "table1" LIMIT 0',
                           """{"rows":[],"fields":{}}""")
    ds = ogr.Open('CARTO:foo')
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldCount() == 0

    gdal.FileFromMemBuffer('/vsimem/carto&POSTFIELDS=q=SELECT * FROM "table1" LIMIT 500 OFFSET 0',
                           """{"rows":[{}],"fields":{}}}""")
    f = lyr.GetNextFeature()
    if f.GetFID() != 0:
        f.DumpReadable()
        pytest.fail()

    # Layer without geometry or primary key
    gdal.FileFromMemBuffer('/vsimem/carto&POSTFIELDS=q=SELECT * FROM "table1" LIMIT 0',
                           """{"rows":[],"fields":{"strfield":{"type":"string"}, "realfield":{"type":"number"}, "boolfield":{"type":"boolean"}, "datefield":{"type":"date"}}}""")
    ds = ogr.Open('CARTO:foo')
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldCount() == 4
    assert (lyr_defn.GetFieldDefn(0).GetName() == 'strfield' and \
       lyr_defn.GetFieldDefn(0).GetType() == ogr.OFTString)
    assert (lyr_defn.GetFieldDefn(1).GetName() == 'realfield' and \
       lyr_defn.GetFieldDefn(1).GetType() == ogr.OFTReal)
    assert (lyr_defn.GetFieldDefn(2).GetName() == 'boolfield' and \
       lyr_defn.GetFieldDefn(2).GetType() == ogr.OFTInteger and \
       lyr_defn.GetFieldDefn(2).GetSubType() == ogr.OFSTBoolean)
    assert (lyr_defn.GetFieldDefn(3).GetName() == 'datefield' and \
       lyr_defn.GetFieldDefn(3).GetType() == ogr.OFTDateTime)

    gdal.FileFromMemBuffer('/vsimem/carto&POSTFIELDS=q=SELECT "strfield", "realfield", "boolfield", "datefield" FROM "table1" LIMIT 500 OFFSET 0',
                           """{"rows":[{ "strfield": "foo", "realfield": 1.23, "boolfield": true, "datefield": "2015-04-24T12:34:56.123Z" }],"fields":{"strfield":{"type":"string"}, "realfield":{"type":"number"}, "boolfield":{"type":"boolean"}, "datefield":{"type":"date"}}}""")
    f = lyr.GetNextFeature()
    if f['strfield'] != 'foo' or f['realfield'] != 1.23 or f['boolfield'] != 1 or \
       f['datefield'] != '2015/04/24 12:34:56.123+00':
        f.DumpReadable()
        pytest.fail()

    gdal.SetConfigOption('CARTO_API_KEY', 'foo')
    gdal.FileFromMemBuffer('/vsimem/carto&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0&api_key=foo',
                           """{"rows":[{"current_schema":"public"}],"fields":{"current_schema":{"type":"unknown(19)"}}}""")
    gdal.FileFromMemBuffer('/vsimem/carto&POSTFIELDS=q=SELECT CDB_UserTables() LIMIT 500 OFFSET 0&api_key=foo',
                           """{"rows":[{"cdb_usertables":"table1"}],"fields":{"cdb_usertables":{"type":"string"}}}""")
    ds = ogr.Open('CARTO:foo')
    gdal.PushErrorHandler()
    lyr_defn = ds.GetLayer(0).GetLayerDefn()
    gdal.PopErrorHandler()
    assert lyr_defn.GetFieldCount() == 0

    get_full_details_fields_url = """/vsimem/carto&POSTFIELDS=q=SELECT a.attname, t.typname, a.attlen, format_type(a.atttypid,a.atttypmod), a.attnum, a.attnotnull, i.indisprimary, pg_get_expr(def.adbin, c.oid) AS defaultexpr, postgis_typmod_dims(a.atttypmod) dim, postgis_typmod_srid(a.atttypmod) srid, postgis_typmod_type(a.atttypmod)::text geomtyp, srtext FROM pg_class c JOIN pg_attribute a ON a.attnum > 0 AND a.attrelid = c.oid AND c.relname = 'table1' JOIN pg_type t ON a.atttypid = t.oid JOIN pg_namespace n ON c.relnamespace=n.oid AND n.nspname= 'public' LEFT JOIN pg_index i ON c.oid = i.indrelid AND i.indisprimary = 't' AND a.attnum = ANY(i.indkey) LEFT JOIN pg_attrdef def ON def.adrelid = c.oid AND def.adnum = a.attnum LEFT JOIN spatial_ref_sys srs ON srs.srid = postgis_typmod_srid(a.atttypmod) ORDER BY a.attnum LIMIT 500 OFFSET 0&api_key=foo"""
    gdal.FileFromMemBuffer(get_full_details_fields_url, '')
    ds = ogr.Open('CARTO:foo')
    gdal.PushErrorHandler()
    lyr_defn = ds.GetLayer(0).GetLayerDefn()
    gdal.PopErrorHandler()
    assert lyr_defn.GetFieldCount() == 0

    gdal.FileFromMemBuffer(get_full_details_fields_url,
                           """{"rows":[{"attname":"foo"}], "fields":{"attname":{"type":"string"}}}""")
    ds = ogr.Open('CARTO:foo')
    lyr = ds.GetLayer(0)
    gdal.PushErrorHandler()
    lyr_defn = lyr.GetLayerDefn()
    gdal.PopErrorHandler()
    assert lyr_defn.GetFieldCount() == 1

    gdal.PushErrorHandler()
    f = lyr.GetFeature(0)
    gdal.PopErrorHandler()
    assert f is None

    gdal.FileFromMemBuffer(get_full_details_fields_url,
                           """{"rows":[{"attname":"strfield", "typname":"varchar", "attnotnull": true, "defaultexpr": "def_value"},
               {"attname":"intfield", "typname":"int4"},
               {"attname":"doublefield", "typname":"float"},
               {"attname":"boolfield", "typname":"bool"},
               {"attname":"datetimefield", "typname":"timestamp"},
               {"attname":"cartodb_id","typname":"int4","indisprimary":true},
               {"attname":"created_at","typname":"date"},
               {"attname":"updated_at","typname":"date"},
               {"attname":"my_geom","typname":"geometry","dim":3,"srid":4326,"geomtyp":"Point",
                "srtext":"GEOGCS[\\"WGS 84\\",DATUM[\\"WGS_1984\\",SPHEROID[\\"WGS 84\\",6378137,298.257223563,AUTHORITY[\\"EPSG\\",\\"7030\\"]],AUTHORITY[\\"EPSG\\",\\"6326\\"]],PRIMEM[\\"Greenwich\\",0,AUTHORITY[\\"EPSG\\",\\"8901\\"]],UNIT[\\"degree\\",0.0174532925199433,AUTHORITY[\\"EPSG\\",\\"9122\\"]],AUTHORITY[\\"EPSG\\",\\"4326\\"]]"},
               {"attname":"the_geom_webmercator","typname":"geometry"}],
        "fields":{"attname":{"type":"string"},
                  "typname":{"type":"string"},
                  "attlen":{"type":"number"},
                  "format_type":{"type":"string"},
                  "attnum":{"type":"number"},
                  "attnotnull":{"type":"boolean"},
                  "indisprimary":{"type":"boolean"},
                  "defaultexpr":{"type":"string"},
                  "dim":{"type":"number"},
                  "srid":{"type":"number"},
                  "geomtyp":{"type":"string"},
                  "srtext":{"type":"string"}}}""")

    ds = ogr.Open('CARTO:foo')
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldCount() == 5
    assert (lyr_defn.GetFieldDefn(0).GetName() == 'strfield' and \
       lyr_defn.GetFieldDefn(0).GetType() == ogr.OFTString and not \
       lyr_defn.GetFieldDefn(0).IsNullable() and \
       lyr_defn.GetFieldDefn(0).GetDefault() == 'def_value')
    assert lyr_defn.GetGeomFieldCount() == 1
    assert lyr_defn.GetGeomFieldDefn(0).GetName() == 'my_geom'
    assert lyr_defn.GetGeomFieldDefn(0).GetType() == ogr.wkbPoint25D
    assert lyr_defn.GetGeomFieldDefn(0).GetSpatialRef().ExportToWkt().find('4326') >= 0

    gdal.PushErrorHandler()
    fc = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert fc == 0

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=SELECT COUNT(*) FROM "table1"&api_key=foo""",
                           """{}""")
    gdal.PushErrorHandler()
    fc = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert fc == 0

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=SELECT COUNT(*) FROM "table1"&api_key=foo""",
                           """{"rows":[{"foo":1}],
            "fields":{"foo":{"type":"number"}}}""")
    gdal.PushErrorHandler()
    fc = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert fc == 0

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=SELECT COUNT(*) FROM "table1"&api_key=foo""",
                           """{"rows":[{"count":9876543210}],
            "fields":{"count":{"type":"number"}}}""")
    assert lyr.GetFeatureCount() == 9876543210

    gdal.PushErrorHandler()
    extent = lyr.GetExtent()
    gdal.PopErrorHandler()
    assert extent == (0, 0, 0, 0)

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=SELECT ST_Extent("my_geom") FROM "table1"&api_key=foo""",
                           """{"rows":[{"foo":1}],
            "fields":{"foo":{"type":"number"}}}""")

    gdal.PushErrorHandler()
    extent = lyr.GetExtent()
    gdal.PopErrorHandler()
    assert extent == (0, 0, 0, 0)

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=SELECT ST_Extent("my_geom") FROM "table1"&api_key=foo""",
                           """{"rows":[{"st_extent":""}],
            "fields":{"st_extent":{"type":"string"}}}""")
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    lyr.GetExtent()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=SELECT ST_Extent("my_geom") FROM "table1"&api_key=foo""",
                           """{"rows":[{"st_extent":"("}],
            "fields":{"st_extent":{"type":"string"}}}""")
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    lyr.GetExtent()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=SELECT ST_Extent("my_geom") FROM "table1"&api_key=foo""",
                           """{"rows":[{"st_extent":"BOX()"}],
            "fields":{"st_extent":{"type":"string"}}}""")
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    lyr.GetExtent()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=SELECT ST_Extent("my_geom") FROM "table1"&api_key=foo""",
                           """{"rows":[{"st_extent":"BOX(0,1,2,3)"}],
            "fields":{"st_extent":{"type":"string"}}}""")
    assert lyr.GetExtent() == (0.0, 2.0, 1.0, 3.0)

    gdal.PushErrorHandler()
    f = lyr.GetFeature(0)
    gdal.PopErrorHandler()
    assert f is None

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=SELECT "cartodb_id", "my_geom", "strfield", "intfield", "doublefield", "boolfield", "datetimefield" FROM "table1" WHERE "cartodb_id" = 0&api_key=foo""",
                           """""")

    gdal.PushErrorHandler()
    f = lyr.GetFeature(0)
    gdal.PopErrorHandler()
    assert f is None

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=SELECT "cartodb_id", "my_geom", "strfield", "intfield", "doublefield", "boolfield", "datetimefield" FROM "table1" WHERE "cartodb_id" = 0&api_key=foo""",
                           """{"rows":[{"st_extent":"BOX(0,1,2,3)"}],
            "fields":{"st_extent":{"type":"string"}}}""")

    f = lyr.GetFeature(0)
    assert f.GetFID() == -1 and f.IsFieldNull(0)

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=SELECT "cartodb_id", "my_geom", "strfield", "intfield", "doublefield", "boolfield", "datetimefield" FROM "table1" WHERE "cartodb_id" = 0&api_key=foo""",
                           """{"rows":[{"cartodb_id":0}],
            "fields":{"cartodb_id":{"type":"numeric"}}}""")

    f = lyr.GetFeature(0)
    assert f.GetFID() == 0

    lyr.ResetReading()
    gdal.PushErrorHandler()
    f = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    assert f is None

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=SELECT "cartodb_id", "my_geom", "strfield", "intfield", "doublefield", "boolfield", "datetimefield" FROM "table1" WHERE "cartodb_id" >= 0 ORDER BY "cartodb_id" ASC LIMIT 500&api_key=foo""",
                           """{"rows":[{"cartodb_id":0}],
            "fields":{"cartodb_id":{"type":"numeric"}}}""")
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f.GetFID() == 0
    gdal.PushErrorHandler()
    f = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    assert f is None

    gdal.SetConfigOption('CARTO_PAGE_SIZE', '2')
    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=SELECT "cartodb_id", "my_geom", "strfield", "intfield", "doublefield", "boolfield", "datetimefield" FROM "table1" WHERE "cartodb_id" >= 0 ORDER BY "cartodb_id" ASC LIMIT 2&api_key=foo""",
                           """{"rows":[{"cartodb_id":0},{"cartodb_id":10}],
            "fields":{"cartodb_id":{"type":"numeric"}}}""")
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f.GetFID() == 0
    f = lyr.GetNextFeature()
    assert f.GetFID() == 10

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=SELECT "cartodb_id", "my_geom", "strfield", "intfield", "doublefield", "boolfield", "datetimefield" FROM "table1" WHERE "cartodb_id" >= 11 ORDER BY "cartodb_id" ASC LIMIT 2&api_key=foo""",
                           """{"rows":[{"cartodb_id":12}],
            "fields":{"cartodb_id":{"type":"numeric"}}}""")
    f = lyr.GetNextFeature()
    assert f.GetFID() == 12
    gdal.ErrorReset()
    f = lyr.GetNextFeature()
    assert f is None and gdal.GetLastErrorMsg() == ''

    lyr.SetAttributeFilter('strfield is NULL')

    gdal.PushErrorHandler()
    fc = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert fc == 0

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=SELECT "cartodb_id", "my_geom", "strfield", "intfield", "doublefield", "boolfield", "datetimefield" FROM "table1" WHERE (strfield is NULL) AND "cartodb_id" >= 0 ORDER BY "cartodb_id" ASC LIMIT 2&api_key=foo""",
                           """{"rows":[{"cartodb_id":0}],
            "fields":{"cartodb_id":{"type":"numeric"}}}""")
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f is not None
    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=SELECT "cartodb_id", "my_geom", "strfield", "intfield", "doublefield", "boolfield", "datetimefield" FROM "table1" WHERE (strfield is NULL) AND "cartodb_id" >= 1 ORDER BY "cartodb_id" ASC LIMIT 2&api_key=foo""",
                           """{"rows":[],
            "fields":{"cartodb_id":{"type":"numeric"}}}""")
    gdal.ErrorReset()
    f = lyr.GetNextFeature()
    assert f is None and gdal.GetLastErrorMsg() == ''

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=SELECT COUNT(*) FROM "table1" WHERE (strfield is NULL)&api_key=foo""",
                           """{"rows":[{"count":9876543210}],
            "fields":{"count":{"type":"number"}}}""")
    assert lyr.GetFeatureCount() == 9876543210

    lyr.SetSpatialFilterRect(-180, -90, 180, 90)

    gdal.PushErrorHandler()
    fc = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert fc == 0

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=SELECT "cartodb_id", "my_geom", "strfield", "intfield", "doublefield", "boolfield", "datetimefield" FROM "table1" WHERE ("my_geom" %26%26 'BOX3D(-180 -90, 180 90)'::box3d) AND (strfield is NULL) AND "cartodb_id" >= 0 ORDER BY "cartodb_id" ASC LIMIT 2&api_key=foo""",
                           """{"rows":[{"cartodb_id":20, "my_geom": "010100000000000000000000400000000000804840" }],
            "fields":{"cartodb_id":{"type":"numeric"}, "my_geom":{"type":"string"}}}""")

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f is not None and f.GetGeometryRef().ExportToWkt() == 'POINT (2 49)'

    gdal.PushErrorHandler()
    fc = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert fc == 1

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=SELECT COUNT(*) FROM "table1" WHERE ("my_geom" %26%26 'BOX3D(-180 -90, 180 90)'::box3d) AND (strfield is NULL)&api_key=foo""",
                           """{"rows":[{"count":9876543210}],
            "fields":{"count":{"type":"number"}}}""")
    assert lyr.GetFeatureCount() == 9876543210

    # Not permitted in read-only mode
    f = ogr.Feature(lyr.GetLayerDefn())
    gdal.PushErrorHandler()
    ds.CreateLayer('foo')
    ds.DeleteLayer(0)
    lyr.CreateFeature(f)
    lyr.SetFeature(f)
    lyr.DeleteFeature(0)
    lyr.CreateField(ogr.FieldDefn('foo'))
    lyr.DeleteField(0)
    gdal.PopErrorHandler()

    ds = None

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=DROP FUNCTION IF EXISTS ogr_table_metadata(TEXT,TEXT); CREATE OR REPLACE FUNCTION ogr_table_metadata(schema_name TEXT, table_name TEXT) RETURNS TABLE (attname TEXT, typname TEXT, attlen INT, format_type TEXT, attnum INT, attnotnull BOOLEAN, indisprimary BOOLEAN, defaultexpr TEXT, dim INT, srid INT, geomtyp TEXT, srtext TEXT) AS $$ SELECT a.attname::text, t.typname::text, a.attlen::int, format_type(a.atttypid,a.atttypmod)::text, a.attnum::int, a.attnotnull::boolean, i.indisprimary::boolean, pg_get_expr(def.adbin, c.oid)::text AS defaultexpr, (CASE WHEN t.typname = 'geometry' THEN postgis_typmod_dims(a.atttypmod) ELSE NULL END)::int dim, (CASE WHEN t.typname = 'geometry' THEN postgis_typmod_srid(a.atttypmod) ELSE NULL END)::int srid, (CASE WHEN t.typname = 'geometry' THEN postgis_typmod_type(a.atttypmod) ELSE NULL END)::text geomtyp, srtext FROM pg_class c JOIN pg_attribute a ON a.attnum > 0 AND a.attrelid = c.oid AND c.relname = $2 AND c.relname IN (SELECT CDB_UserTables())JOIN pg_type t ON a.atttypid = t.oid JOIN pg_namespace n ON c.relnamespace=n.oid AND n.nspname = $1 LEFT JOIN pg_index i ON c.oid = i.indrelid AND i.indisprimary = 't' AND a.attnum = ANY(i.indkey) LEFT JOIN pg_attrdef def ON def.adrelid = c.oid AND def.adnum = a.attnum LEFT JOIN spatial_ref_sys srs ON srs.srid = postgis_typmod_srid(a.atttypmod) ORDER BY a.attnum $$ LANGUAGE SQL&api_key=foo""",
                           """""""")
    gdal.SetConfigOption('CARTO_PAGE_SIZE', None)
    ds = ogr.Open('CARTO:foo', update=1)
    lyr = ds.CreateLayer('MY_LAYER')

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    lyr.GetNextFeature()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=SELECT cdb_cartodbfytable('my_layer')&api_key=foo""",
                           """{"rows":[],
            "fields":{}}""")
    ds = None
    gdal.Unlink("""/vsimem/carto&POSTFIELDS=q=SELECT cdb_cartodbfytable('my_layer')&api_key=foo""")

    ds = gdal.OpenEx('CARTO:foo', gdal.OF_VECTOR | gdal.OF_UPDATE, open_options=['COPY_MODE=NO'])
    gdal.SetConfigOption('CARTO_MAX_CHUNK_SIZE', '0')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    lyr = ds.CreateLayer('MY_LAYER', srs=sr)
    fld_defn = ogr.FieldDefn('STRFIELD', ogr.OFTString)
    fld_defn.SetNullable(0)
    fld_defn.SetDefault("'DEFAULT VAL'")
    fld_defn.SetWidth(20)
    lyr.CreateField(fld_defn)

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=CREATE TABLE "my_layer" ( cartodb_id SERIAL,the_geom GEOMETRY(GEOMETRY, 4326),"strfield" VARCHAR NOT NULL DEFAULT 'DEFAULT VAL',PRIMARY KEY (cartodb_id) );DROP SEQUENCE IF EXISTS "my_layer_cartodb_id_seq" CASCADE;CREATE SEQUENCE "my_layer_cartodb_id_seq" START 1;ALTER SEQUENCE "my_layer_cartodb_id_seq" OWNED BY "my_layer".cartodb_id;ALTER TABLE "my_layer" ALTER COLUMN cartodb_id SET DEFAULT nextval('"my_layer_cartodb_id_seq"')&api_key=foo""",
                           """{"rows":[],
            "fields":{}}""")

    f = ogr.Feature(lyr.GetLayerDefn())
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    assert ret != 0
    f = None

    fld_defn = ogr.FieldDefn('INTFIELD', ogr.OFTInteger)
    # No server answer
    with gdaltest.error_handler():
        ret = lyr.CreateField(fld_defn)
    assert ret != 0

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=ALTER TABLE "my_layer" ADD COLUMN "intfield" INTEGER&api_key=foo""",
                           """{"rows":[],
            "fields":{}}""")
    assert lyr.CreateField(fld_defn) == 0

    fld_defn = ogr.FieldDefn('boolfield', ogr.OFTInteger)
    fld_defn.SetSubType(ogr.OFSTBoolean)

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=ALTER TABLE "my_layer" ADD COLUMN "boolfield" BOOLEAN&api_key=foo""",
                           """{"rows":[],
            "fields":{}}""")
    assert lyr.CreateField(fld_defn) == 0

    # Invalid field
    with gdaltest.error_handler():
        assert lyr.DeleteField(-1) != 0

    # No server answer
    with gdaltest.error_handler():
        assert lyr.DeleteField(0) != 0

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=ALTER TABLE "my_layer" DROP COLUMN "boolfield"&api_key=foo""",
                           """{"rows":[],
            "fields":{}}""")
    fld_pos = lyr.GetLayerDefn().GetFieldIndex(fld_defn.GetName())
    assert lyr.DeleteField(fld_pos) == 0

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=ALTER TABLE "my_layer" ADD COLUMN "boolfield" BOOLEAN&api_key=foo""",
                           """{"rows":[],
            "fields":{}}""")
    assert lyr.CreateField(fld_defn) == 0

    f = ogr.Feature(lyr.GetLayerDefn())
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    assert ret != 0

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('strfield', 'foo')
    f.SetField('intfield', 1)
    f.SetField('boolfield', 1)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2 49)'))
    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=INSERT INTO "my_layer" ("strfield", "intfield", "boolfield", "the_geom") VALUES ('foo', 1, 't', '0101000020E610000000000000000000400000000000804840') RETURNING "cartodb_id"&api_key=foo""",
                           """{"rows":[ {"cartodb_id": 1} ],
            "fields":{"cartodb_id":{"type":"integer"}}}""")
    ret = lyr.CreateFeature(f)
    assert ret == 0 and f.GetFID() == 1

    f.SetFID(-1)
    gdal.PushErrorHandler()
    ret = lyr.SetFeature(f)
    gdal.PopErrorHandler()
    assert ret != 0

    f.SetFID(3)
    gdal.PushErrorHandler()
    ret = lyr.SetFeature(f)
    gdal.PopErrorHandler()
    assert ret != 0

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=UPDATE "my_layer" SET "strfield" = 'foo', "intfield" = 1, "boolfield" = 't', "the_geom" = '0101000020E610000000000000000000400000000000804840' WHERE "cartodb_id" = 3&api_key=foo""",
                           """{"total_rows": 0}""")
    ret = lyr.SetFeature(f)
    assert ret == ogr.OGRERR_NON_EXISTING_FEATURE

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=UPDATE "my_layer" SET "strfield" = 'foo', "intfield" = 1, "boolfield" = 't', "the_geom" = '0101000020E610000000000000000000400000000000804840' WHERE "cartodb_id" = 3&api_key=foo""",
                           """{"total_rows": 1}""")
    ret = lyr.SetFeature(f)
    assert ret == 0

    f = ogr.Feature(lyr.GetLayerDefn())
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    assert ret != 0

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=INSERT INTO "my_layer" DEFAULT VALUES RETURNING "cartodb_id"&api_key=foo""",
                           """{"rows":[ {"cartodb_id": 4} ],
            "fields":{"cartodb_id":{"type":"integer"}}}""")
    ret = lyr.CreateFeature(f)
    assert ret == 0 and f.GetFID() == 4

    gdal.PushErrorHandler()
    ret = lyr.DeleteFeature(0)
    gdal.PopErrorHandler()
    assert ret != 0

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=DELETE FROM "my_layer" WHERE "cartodb_id" = 0&api_key=foo""",
                           """{"total_rows": 0}""")
    ret = lyr.DeleteFeature(0)
    assert ret == ogr.OGRERR_NON_EXISTING_FEATURE

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=DELETE FROM "my_layer" WHERE "cartodb_id" = 0&api_key=foo""",
                           """{"total_rows": 1}""")
    ret = lyr.DeleteFeature(0)
    assert ret == 0

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=SELECT cdb_cartodbfytable('my_layer')&api_key=foo""",
                           """{"rows":[],
            "fields":{}}""")
    ds = None

    gdal.SetConfigOption('CARTO_MAX_CHUNK_SIZE', None)
    ds = gdal.OpenEx('CARTO:foo', gdal.OF_VECTOR | gdal.OF_UPDATE, open_options=['COPY_MODE=NO'])
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=SELECT pg_catalog.pg_get_serial_sequence('table1', 'cartodb_id') AS seq_name&api_key=foo""",
                           """{"rows":[{"seq_name":"table1_cartodb_id_seq0"}],"fields":{"seq_name":{"type":"string"}}}""")

    gdal.FileFromMemBuffer("""/vsimem/carto&POSTFIELDS=q=SELECT nextval('table1_cartodb_id_seq0') AS nextid&api_key=foo""",
                           """{"rows":[{"nextid":11}],"fields":{"nextid":{"type":"number"}}}""")

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('strfield', 'foo')
    ret = lyr.CreateFeature(f)
    assert ret == 0 and f.GetFID() == 11

    f = ogr.Feature(lyr.GetLayerDefn())

    with gdaltest.tempfile("""/vsimem/carto&POSTFIELDS=q=BEGIN;INSERT INTO "table1" ("strfield", "cartodb_id") VALUES ('foo', 11);INSERT INTO "table1" DEFAULT VALUES;COMMIT;&api_key=foo""",
                           """{"rows":[],
            "fields":{}}"""):
        ret = lyr.CreateFeature(f)
        ds = None
    if ret != 0 or f.GetFID() != 12:
        f.DumpReadable()
        pytest.fail()

    ds = gdal.OpenEx('CARTO:foo', gdal.OF_VECTOR | gdal.OF_UPDATE, open_options=['COPY_MODE=NO'])
    lyr = ds.GetLayer(0)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFieldNull('strfield')
    with gdaltest.tempfile("""/vsimem/carto&POSTFIELDS=q=BEGIN;INSERT INTO "table1" ("strfield", "cartodb_id") VALUES (NULL, 11);COMMIT;&api_key=foo""",
                           """{"rows":[],
            "fields":{}}"""):
        ret = lyr.CreateFeature(f)
        ds = None

    if ret != 0 or f.GetFID() != 11:
        f.DumpReadable()
        pytest.fail()

    # Now remove default value to strfield
    gdal.FileFromMemBuffer(get_full_details_fields_url,
                           """{"rows":[{"attname":"strfield", "typname":"varchar"},
               {"attname":"intfield", "typname":"int4"},
               {"attname":"doublefield", "typname":"float"},
               {"attname":"boolfield", "typname":"bool"},
               {"attname":"datetimefield", "typname":"timestamp"},
               {"attname":"cartodb_id","typname":"int4","indisprimary":true},
               {"attname":"created_at","typname":"date"},
               {"attname":"updated_at","typname":"date"},
               {"attname":"my_geom","typname":"geometry","dim":3,"srid":4326,"geomtyp":"Point",
                "srtext":"GEOGCS[\\"WGS 84\\",DATUM[\\"WGS_1984\\",SPHEROID[\\"WGS 84\\",6378137,298.257223563,AUTHORITY[\\"EPSG\\",\\"7030\\"]],AUTHORITY[\\"EPSG\\",\\"6326\\"]],PRIMEM[\\"Greenwich\\",0,AUTHORITY[\\"EPSG\\",\\"8901\\"]],UNIT[\\"degree\\",0.0174532925199433,AUTHORITY[\\"EPSG\\",\\"9122\\"]],AUTHORITY[\\"EPSG\\",\\"4326\\"]]"},
               {"attname":"the_geom_webmercator","typname":"geometry"}],
        "fields":{"attname":{"type":"string"},
                  "typname":{"type":"string"},
                  "attlen":{"type":"number"},
                  "format_type":{"type":"string"},
                  "attnum":{"type":"number"},
                  "attnotnull":{"type":"boolean"},
                  "indisprimary":{"type":"boolean"},
                  "defaultexpr":{"type":"string"},
                  "dim":{"type":"number"},
                  "srid":{"type":"number"},
                  "geomtyp":{"type":"string"},
                  "srtext":{"type":"string"}}}""")

    ds = gdal.OpenEx('CARTO:foo', gdal.OF_VECTOR | gdal.OF_UPDATE, open_options=['COPY_MODE=NO'])
    lyr = ds.GetLayer(0)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('strfield', 'foo')
    with gdaltest.tempfile("""/vsimem/carto&POSTFIELDS=q=SELECT nextval('table1_cartodb_id_seq') AS nextid&api_key=foo""",
                           """{"rows":[{"nextid":11}],"fields":{"nextid":{"type":"number"}}}"""):
        ret = lyr.CreateFeature(f)
    assert ret == 0 and f.GetFID() == 11

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('strfield', 'bar')
    ret = lyr.CreateFeature(f)
    assert ret == 0 and f.GetFID() == 12

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('strfield', 'baz')
    ret = lyr.CreateFeature(f)
    assert ret == 0 and f.GetFID() == 13

    gdal.ErrorReset()
    with gdaltest.tempfile("""/vsimem/carto&POSTFIELDS=q=BEGIN;INSERT INTO "table1" ("strfield", "cartodb_id") VALUES ('foo', 11);INSERT INTO "table1" ("strfield", "intfield", "doublefield", "boolfield", "datetimefield", "my_geom") VALUES ('bar', NULL, NULL, NULL, NULL, NULL), ('baz', NULL, NULL, NULL, NULL, NULL);COMMIT;&api_key=foo""",
                           """{"rows":[], "fields":{}}"""):
        ds = None
    assert gdal.GetLastErrorMsg() == ''

    ds = gdal.OpenEx('CARTO:foo', gdal.OF_VECTOR | gdal.OF_UPDATE, open_options=['COPY_MODE=NO'])

    gdal.PushErrorHandler()
    lyr = ds.CreateLayer('table1')
    gdal.PopErrorHandler()
    assert lyr is None

    with gdaltest.tempfile("""/vsimem/carto&POSTFIELDS=q=BEGIN; DROP TABLE IF EXISTS "table1";CREATE TABLE "table1" ( cartodb_id SERIAL,the_geom Geometry(MULTIPOLYGON,0),PRIMARY KEY (cartodb_id) );DROP SEQUENCE IF EXISTS "table1_cartodb_id_seq" CASCADE;CREATE SEQUENCE "table1_cartodb_id_seq" START 1;ALTER SEQUENCE "table1_cartodb_id_seq" OWNED BY "table1".cartodb_id;ALTER TABLE "table1" ALTER COLUMN cartodb_id SET DEFAULT nextval('"table1_cartodb_id_seq"'); COMMIT;&api_key=foo""",
                           """{"rows":[], "fields":{}}"""):
        lyr = ds.CreateLayer('table1', geom_type=ogr.wkbPolygon, options=['OVERWRITE=YES', 'CARTODBFY=NO'])
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((0 0,0 1,1 0,0 0))'))
        assert lyr.CreateFeature(f) == 0

    gdal.ErrorReset()
    with gdaltest.tempfile("""/vsimem/carto&POSTFIELDS=q=BEGIN;INSERT INTO "table1" ("the_geom") VALUES ('0106000020E61000000100000001030000000100000004000000000000000000000000000000000000000000000000000000000000000000F03F000000000000F03F000000000000000000000000000000000000000000000000');COMMIT;&api_key=foo""",
                           """{"rows":[],
            "fields":{}}"""):
        ds = None
    assert gdal.GetLastErrorMsg() == ''

    ds = gdal.OpenEx('CARTO:foo', gdal.OF_VECTOR | gdal.OF_UPDATE, open_options=['COPY_MODE=NO'])

    with gdaltest.tempfile("""/vsimem/carto&POSTFIELDS=q=BEGIN; DROP TABLE IF EXISTS "table1";CREATE TABLE "table1" ( cartodb_id SERIAL,the_geom Geometry(MULTIPOLYGON,0),PRIMARY KEY (cartodb_id) );DROP SEQUENCE IF EXISTS "table1_cartodb_id_seq" CASCADE;CREATE SEQUENCE "table1_cartodb_id_seq" START 1;ALTER SEQUENCE "table1_cartodb_id_seq" OWNED BY "table1".cartodb_id;ALTER TABLE "table1" ALTER COLUMN cartodb_id SET DEFAULT nextval('"table1_cartodb_id_seq"'); COMMIT;&api_key=foo""",
                           """{"rows":[], "fields":{}}"""):
        lyr = ds.CreateLayer('table1', geom_type=ogr.wkbPolygon, options=['OVERWRITE=YES', 'CARTODBFY=NO'])
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(100)

        with gdaltest.tempfile("""/vsimem/carto&POSTFIELDS=q=BEGIN;INSERT INTO "table1" ("cartodb_id") VALUES (100);COMMIT;&api_key=foo""",
                               """{"rows":[], "fields":{}}"""):
            assert lyr.CreateFeature(f) == 0
            assert f.GetFID() == 100
            ds = None

    ds = ogr.Open('CARTO:foo', update=1)

    gdal.PushErrorHandler()
    ret = ds.DeleteLayer(0)
    gdal.PopErrorHandler()
    assert ret != 0

    gdal.ErrorReset()
    ds = gdal.OpenEx('CARTO:foo', gdal.OF_VECTOR | gdal.OF_UPDATE, open_options=['COPY_MODE=YES'])
    assert ds is not None
    lyr = ds.GetLayerByName('table1')
    assert lyr is not None

    with gdaltest.tempfile("""/vsimem/carto/copyfrom?q=COPY%20%22table1%22%20(%22strfield%22,%22my_geom%22,%22cartodb_id%22)%20FROM%20STDIN%20WITH%20(FORMAT%20text,%20ENCODING%20UTF8)&api_key=foo&POSTFIELDS=copytest\t0101000020E610000000000000000059400000000000005940\t11\n\\.\n""","""{}"""):

        with gdaltest.tempfile("""/vsimem/carto/copyfrom?q=COPY%20%22table1%22%20(%22intfield%22,%22my_geom%22)%20FROM%20STDIN%20WITH%20(FORMAT%20text,%20ENCODING%20UTF8)&api_key=foo&POSTFIELDS=12\t0101000020E610000000000000000059400000000000005940\n\\.\n%22""","""{}"""):

            f = ogr.Feature(lyr.GetLayerDefn())
            f.SetField('strfield', 'copytest')
            f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(100 100)'))
            assert lyr.CreateFeature(f) == 0

            f = ogr.Feature(lyr.GetLayerDefn())
            f.SetField('intfield', 12)
            f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(100 100)'))
            assert lyr.CreateFeature(f) == 0

            ds = None # force flush

    ds = ogr.Open('CARTO:foo', update=1)

    gdal.ErrorReset()
    with gdaltest.tempfile("""/vsimem/carto&POSTFIELDS=q=DROP TABLE "table1"&api_key=foo""",
                           """{"rows":[], "fields":{}}"""):
        ds.ExecuteSQL('DELLAYER:table1')
    assert gdal.GetLastErrorMsg() == '' and ds.GetLayerByName('table1') is None

    with gdaltest.tempfile('/vsimem/carto&POSTFIELDS=q=SELECT current_schema() LIMIT 500 OFFSET 0&api_key=foo',
                           """{"rows":[{"current_schema":"my_schema"}],"fields":{"current_schema":{"type":"unknown(19)"}}}"""):
        with gdaltest.tempfile('/vsimem/carto&POSTFIELDS=q=SELECT CDB_UserTables() LIMIT 500 OFFSET 0&api_key=foo',
                               """{"rows":[],"fields":{"cdb_usertables":{"type":"string"}}}"""):
            with gdaltest.tempfile("""/vsimem/carto&POSTFIELDS=q=SELECT c.relname FROM pg_class c, pg_namespace n WHERE c.relkind in ('r', 'v') AND c.relname !~ '^pg_' AND c.relnamespace=n.oid AND n.nspname = 'my_schema' LIMIT 500 OFFSET 0&api_key=foo""",
                                   """{"rows":[{"relname": "a_layer"}],"fields":{"relname":{"type":"string"}}}"""):
                ds = ogr.Open('CARTO:foo')
    assert ds.GetLayerByName('a_layer') is not None

###############################################################################
#


def test_ogr_carto_vsimem_cleanup():

    gdal.SetConfigOption('CARTO_API_URL', None)
    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', None)
    gdal.SetConfigOption('CARTO_PAGE_SIZE', None)
    gdal.SetConfigOption('CARTO_MAX_CHUNK_SIZE', None)
    gdal.SetConfigOption('CARTO_API_KEY', ogrtest.carto_api_key_ori)

    for f in gdal.ReadDir('/vsimem/'):
        gdal.Unlink('/vsimem/' + f)


###############################################################################
#  Run test_ogrsf

@pytest.mark.skipif(gdaltest.is_ci(), reason='test skipped on CI due to regular failures on it due to platform limits')
def test_ogr_carto_test_ogrsf():
    if gdal.GetConfigOption('SKIP_SLOW') is not None:
        pytest.skip()

    ogrtest.carto_test_server = 'https://gdalautotest2.carto.com'

    if gdaltest.gdalurlopen(ogrtest.carto_test_server) is None:
        pytest.skip('cannot open %s' % ogrtest.carto_test_server)

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' --config CARTO_HTTPS NO --config CARTO_PAGE_SIZE 300 -ro "CARTO:gdalautotest2 tables=tm_world_borders_simpl_0_3"')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################
# Test if driver is available


def ogr_carto_rw_init():

    ogrtest.carto_connection = gdal.GetConfigOption('CARTO_CONNECTION')
    if ogrtest.carto_connection is None:
        pytest.skip('CARTO_CONNECTION missing')
    if gdal.GetConfigOption('CARTO_API_KEY') is None:
        pytest.skip('CARTO_API_KEY missing')


###############################################################################
# Read/write/update test


def ogr_carto_rw_1():

    ds = ogr.Open(ogrtest.carto_connection, update=1)
    assert ds is not None

    a_uuid = str(uuid.uuid1()).replace('-', '_')
    lyr_name = "LAYER_" + a_uuid

    # No-op
    with gdaltest.error_handler():
        lyr = ds.CreateLayer(lyr_name)
    ds.DeleteLayer(ds.GetLayerCount() - 1)

    # Deferred table creation
    with gdaltest.error_handler():
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

    ret = lyr.CreateFeature(f)
    assert ret == 0

    f.SetFID(-1)
    f.SetField('STRFIELD', "fo'o")
    f.SetField('intfield', 123)
    f.SetField('int64field', 12345678901234)
    f.SetField('doublefield', 1.23)
    f.SetField('dt', '2014/12/04 12:34:56')
    f.SetField('bool', 0)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (1 2)'))
    ret = lyr.CreateFeature(f)
    assert ret == 0

    f.SetField('intfield', 456)
    f.SetField('bool', 1)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (3 4)'))
    ret = lyr.SetFeature(f)
    assert ret == 0
    fid = f.GetFID()

    ds = None

    ds = ogr.Open(ogrtest.carto_connection, update=1)
    lyr_name = "layer_" + a_uuid
    found = False
    for lyr in ds:
        if lyr.GetName() == lyr_name:
            found = True
    assert found
    lyr = ds.GetLayerByName(lyr_name)
    found = False
    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        if lyr.GetLayerDefn().GetFieldDefn(i).GetName() == 'strfield':
            found = True
    assert found
    f = lyr.GetFeature(fid)
    if f.GetField('strfield') != "fo'o" or \
       f.GetField('intfield') != 456 or \
       f.GetField('int64field') != 12345678901234 or \
       f.GetField('doublefield') != 1.23 or \
       f.GetField('dt') != '2014/12/04 12:34:56+00' or \
       f.GetField('bool') != 1 or \
       f.GetGeometryRef().ExportToWkt() != 'POINT (3 4)':
        f.DumpReadable()
        ds.ExecuteSQL("DELLAYER:" + lyr_name)
        pytest.fail()

    lyr.DeleteFeature(fid)
    f = lyr.GetFeature(fid)
    if f is not None:
        ds.ExecuteSQL("DELLAYER:" + lyr_name)
        pytest.fail()

    # Non-differed field creation
    lyr.CreateField(ogr.FieldDefn("otherstrfield", ogr.OFTString))

    ds.ExecuteSQL("DELLAYER:" + lyr_name)

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = ds.CreateLayer(lyr_name, geom_type=ogr.wkbMultiPolygon, srs=srs)
    lyr.GetNextFeature()
    ds.ExecuteSQL("DELLAYER:" + lyr_name)

    # Test that the_geom_webmercator is properly created
    lyr = ds.CreateLayer(lyr_name, geom_type=ogr.wkbPoint, srs=srs)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (3 4)'))
    lyr.CreateFeature(f)

    ds = None

    ds = ogr.Open(ogrtest.carto_connection, update=1)

    sql_lyr = ds.ExecuteSQL('SELECT ST_AsText(the_geom_webmercator) AS foo FROM ' + lyr_name)
    f = sql_lyr.GetNextFeature()
    if not f.GetField(0).startswith('POINT'):
        ds.ExecuteSQL("DELLAYER:" + lyr_name)
        ds.ReleaseResultSet(sql_lyr)
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

    ds.ExecuteSQL("DELLAYER:" + lyr_name)

    # Layer without geometry
    lyr = ds.CreateLayer(lyr_name, geom_type=ogr.wkbNone)
    fd = ogr.FieldDefn("nullable", ogr.OFTString)
    lyr.CreateField(fd)
    fd = ogr.FieldDefn("not_nullable", ogr.OFTString)
    fd.SetNullable(0)
    lyr.CreateField(fd)

    field_defn = ogr.FieldDefn('field_string', ogr.OFTString)
    field_defn.SetDefault("'a''b'")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn('field_datetime_with_default', ogr.OFTDateTime)
    field_defn.SetDefault("CURRENT_TIMESTAMP")
    lyr.CreateField(field_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('not_nullable', 'foo')
    lyr.CreateFeature(f)
    f = None

    ds = None

    ds = ogr.Open(ogrtest.carto_connection, update=1)

    lyr = ds.GetLayerByName(lyr_name)
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('nullable')).IsNullable() != 1:
        ds.ExecuteSQL("DELLAYER:" + lyr_name)
        pytest.fail()
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('not_nullable')).IsNullable() != 0:
        ds.ExecuteSQL("DELLAYER:" + lyr_name)
        pytest.fail()
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_string')).GetDefault() != "'a''b'":
        ds.ExecuteSQL("DELLAYER:" + lyr_name)
        pytest.fail()
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime_with_default')).GetDefault() != 'CURRENT_TIMESTAMP':
        ds.ExecuteSQL("DELLAYER:" + lyr_name)
        pytest.fail()
    f = lyr.GetNextFeature()
    if f is None or f.GetField('field_string') != 'a\'b' or not f.IsFieldSet('field_datetime_with_default'):
        ds.ExecuteSQL("DELLAYER:" + lyr_name)
        pytest.fail()
    ds.ExecuteSQL("DELLAYER:" + lyr_name)




gdaltest_rw_list = [
    ogr_carto_rw_init,
    ogr_carto_rw_1,
]
