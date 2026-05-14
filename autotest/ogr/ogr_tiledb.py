#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test TileDB driver vector functionality.
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2023, Even Rouault <even dot rouault at spatialys dot com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json
import math
import os
import shutil

import gdaltest
import ogrtest
import pytest
import test_cli_utilities

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_driver("TileDB")


def get_tiledb_version():
    drv = gdal.GetDriverByName("TileDB")
    if drv is None:
        return (0, 0, 0)
    x, y, z = [int(x) for x in drv.GetMetadataItem("TILEDB_VERSION").split(".")]
    return (x, y, z)


###############################################################################


def create_tiledb_dataset(nullable, batch_size, extra_feature=False):

    ds = ogr.GetDriverByName("TileDB").CreateDataSource("tmp/test.tiledb")
    srs = osr.SpatialReference()
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    srs.ImportFromEPSG(4326)
    options = []
    if batch_size:
        options += ["BATCH_SIZE=" + str(batch_size)]
    lyr = ds.CreateLayer("test", srs=srs, options=options)

    fld_defn = ogr.FieldDefn("strfield", ogr.OFTString)
    fld_defn.SetNullable(nullable)
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("intfield", ogr.OFTInteger)
    fld_defn.SetNullable(nullable)
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("int16field", ogr.OFTInteger)
    fld_defn.SetNullable(nullable)
    fld_defn.SetSubType(ogr.OFSTInt16)
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("uint8field", ogr.OFTInteger)
    fld_defn.SetNullable(nullable)
    with gdal.config_option("TILEDB_INT_TYPE", "UINT8"):
        lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("uint16field", ogr.OFTInteger)
    fld_defn.SetNullable(nullable)
    with gdal.config_option("TILEDB_INT_TYPE", "UINT16"):
        lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("boolfield", ogr.OFTInteger)
    fld_defn.SetNullable(nullable)
    fld_defn.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("int64field", ogr.OFTInteger64)
    fld_defn.SetNullable(nullable)
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("doublefield", ogr.OFTReal)
    fld_defn.SetNullable(nullable)
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("floatfield", ogr.OFTReal)
    fld_defn.SetNullable(nullable)
    fld_defn.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("binaryfield", ogr.OFTBinary)
    fld_defn.SetNullable(nullable)
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("intlistfield", ogr.OFTIntegerList)
    fld_defn.SetNullable(nullable)
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("int16listfield", ogr.OFTIntegerList)
    fld_defn.SetNullable(nullable)
    fld_defn.SetSubType(ogr.OFSTInt16)
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("boollistfield", ogr.OFTIntegerList)
    fld_defn.SetNullable(nullable)
    fld_defn.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("doublelistfield", ogr.OFTRealList)
    fld_defn.SetNullable(nullable)
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("floatlistfield", ogr.OFTRealList)
    fld_defn.SetNullable(nullable)
    fld_defn.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("datetimefield", ogr.OFTDateTime)
    fld_defn.SetNullable(nullable)
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("datefield", ogr.OFTDate)
    fld_defn.SetNullable(nullable)
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("timefield", ogr.OFTTime)
    fld_defn.SetNullable(nullable)
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("intfieldextra", ogr.OFTInteger)
    fld_defn.SetNullable(nullable)
    lyr.CreateField(fld_defn)

    field_count = lyr.GetLayerDefn().GetFieldCount()

    f = ogr.Feature(lyr.GetLayerDefn())
    f["strfield"] = "foo"
    f["intfield"] = -123456789
    f["int16field"] = -32768
    f["boolfield"] = True
    f["uint8field"] = 0
    f["uint16field"] = 0
    f["int64field"] = -1234567890123456
    f["doublefield"] = 1.2345
    f["floatfield"] = 1.5
    f["binaryfield"] = b"\xde\xad\xbe\xef"
    f["intlistfield"] = [-123456789, 123]
    f["int16listfield"] = [-32768, 32767]
    f["boollistfield"] = [True, False]
    f["doublelistfield"] = [1.2345, -1.2345]
    f["floatlistfield"] = [1.5, -1.5, 0]
    f["datetimefield"] = "2023-04-07T12:34:56.789Z"
    f["datefield"] = "2023-04-07"
    f["timefield"] = "12:34:56.789"
    f["intfieldextra"] = 1
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((1 2,1 3,4 3,1 2))"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert f.GetFID() == 1

    f = ogr.Feature(lyr.GetLayerDefn())
    f["intfieldextra"] = 2
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((1 2,1 3,4 3,1 2))"))
    if not nullable:
        with gdal.quiet_errors():
            assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    else:
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert f.GetFID() == 2

    f = ogr.Feature(lyr.GetLayerDefn())
    f["strfield"] = "barbaz"
    f["intfield"] = 123456789
    f["int16field"] = 32767
    f["boolfield"] = False
    f["uint8field"] = 255
    f["uint16field"] = 65535
    f["int64field"] = 1234567890123456
    f["doublefield"] = -1.2345
    f["floatfield"] = -1.5
    f["binaryfield"] = b"\xbe\xef\xde\xad"
    f["intlistfield"] = [123456789, -123]
    f["int16listfield"] = [32767, -32768]
    f["boollistfield"] = [False, True]
    f["doublelistfield"] = [-1.2345, 1.2345]
    f["floatlistfield"] = [0.0, -1.5, 1.5]
    # Will be transformed to "2023/04/07 10:19:56.789+00"
    f["datetimefield"] = "2023-04-07T12:34:56.789+0215"
    f["datefield"] = "2023-04-08"
    f["timefield"] = "13:34:56.789"
    f["intfieldextra"] = 3
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((-10 -20,-1 -3,-4 -3,-10 -20))"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert f.GetFID() == 3

    if extra_feature:
        f = ogr.Feature(lyr.GetLayerDefn())
        f["strfield"] = "something"
        f["intfield"] = 8765432
        f["int16field"] = 32767
        f["boolfield"] = False
        f["uint8field"] = 255
        f["uint16field"] = 65535
        f["int64field"] = 9876543210123456
        f["doublefield"] = -1.2345
        f["floatfield"] = -1.5
        f["binaryfield"] = b"\xde\xad\xbe\xef"
        f["intlistfield"] = [-123456789, -123]
        f["int16listfield"] = [32767, -32768]
        f["boollistfield"] = [False, True]
        f["doublelistfield"] = [-1.2345, 1.2345]
        f["floatlistfield"] = [0.0, -1.5, 1.5]
        # Will be transformed to "2023/04/07 10:19:56.789+00"
        f["datetimefield"] = "2023-04-07T12:34:56.789+0215"
        f["datefield"] = "2023-04-08"
        f["timefield"] = "13:34:56.789"
        f["intfieldextra"] = 4
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (-0.9 -0.9)"))
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        assert f.GetFID() == 4

    ds = None
    return field_count, srs, options


###############################################################################


@pytest.mark.parametrize("nullable,batch_size", [(True, None), (False, 2)])
def test_ogr_tiledb_basic(nullable, batch_size):

    if os.path.exists("tmp/test.tiledb"):
        shutil.rmtree("tmp/test.tiledb")

    field_count, srs, options = create_tiledb_dataset(nullable, batch_size)

    ds = gdal.OpenEx("tmp/test.tiledb", open_options=options)
    lyr = ds.GetLayer(0)
    assert lyr.GetDataset().GetDescription() == ds.GetDescription()
    assert lyr.GetGeomType() == ogr.wkbUnknown
    assert lyr.GetSpatialRef().IsSame(srs)
    assert lyr.GetFeatureCount() == 3
    assert lyr.GetExtent() == (-10.0, 4.0, -20.0, 3.0)
    assert lyr.GetLayerDefn().GetFieldCount() == field_count
    for i in range(field_count):
        assert lyr.GetLayerDefn().GetFieldDefn(i).IsNullable() == nullable

    for i in range(3):
        f = lyr.GetNextFeature()
        if f.GetFID() == 1:
            assert f["strfield"] == "foo"
            assert f["intfield"] == -123456789
            assert f["int16field"] == -32768
            assert f["boolfield"] == True
            assert f["uint8field"] == 0
            assert f["uint16field"] == 0
            assert f["int64field"] == -1234567890123456
            assert f["doublefield"] == 1.2345
            assert f["floatfield"] == 1.5
            assert f.GetFieldAsBinary("binaryfield") == b"\xde\xad\xbe\xef"
            assert f["intlistfield"] == [-123456789, 123]
            assert f["int16listfield"] == [-32768, 32767]
            assert f["boollistfield"] == [True, False]
            assert f["doublelistfield"] == [1.2345, -1.2345]
            assert f["floatlistfield"] == [1.5, -1.5, 0]
            assert f["datetimefield"] == "2023/04/07 12:34:56.789+00"
            assert f["datefield"] == "2023/04/07"
            assert f["timefield"] == "12:34:56.789"
            assert f.GetGeometryRef().ExportToWkt() == "POLYGON ((1 2,1 3,4 3,1 2))"
        elif f.GetFID() == 2:
            assert f["intfieldextra"] == 2
            if nullable:
                for i in range(field_count):
                    if lyr.GetLayerDefn().GetFieldDefn(i).GetName() != "intfieldextra":
                        assert f.IsFieldNull(i)
            else:
                for i in range(field_count):
                    assert not f.IsFieldNull(i)
            assert f.GetGeometryRef().ExportToWkt() == "POLYGON ((1 2,1 3,4 3,1 2))"
        elif f.GetFID() == 3:
            assert f["strfield"] == "barbaz"
            assert f["intfield"] == 123456789
            assert f["int16field"] == 32767
            assert f["boolfield"] == False
            assert f["uint8field"] == 255
            assert f["uint16field"] == 65535
            assert f["int64field"] == 1234567890123456
            assert f["doublefield"] == -1.2345
            assert f["floatfield"] == -1.5
            assert f.GetFieldAsBinary("binaryfield") == b"\xbe\xef\xde\xad"
            assert f["intlistfield"] == [123456789, -123]
            assert f["int16listfield"] == [32767, -32768]
            assert f["boollistfield"] == [False, True]
            assert f["doublelistfield"] == [-1.2345, 1.2345]
            assert f["floatlistfield"] == [0.0, -1.5, 1.5]
            assert f["datetimefield"] == "2023/04/07 10:19:56.789+00"
            assert f["datefield"] == "2023/04/08"
            assert f["timefield"] == "13:34:56.789"
            assert (
                f.GetGeometryRef().ExportToWkt()
                == "POLYGON ((-10 -20,-1 -3,-4 -3,-10 -20))"
            )
        else:
            assert False

    f = lyr.GetNextFeature()
    assert f is None

    f = lyr.GetFeature(0)
    assert f is None

    f = lyr.GetFeature(3)
    assert f.GetFID() == 3
    assert f["strfield"] == "barbaz"

    lyr.SetSpatialFilterRect(0, 0, 10, 10)
    assert lyr.GetFeatureCount() == 2
    assert set(f.GetFID() for f in lyr) == set([1, 2])

    f = lyr.GetFeature(3)
    assert f.GetFID() == 3
    assert f["strfield"] == "barbaz"

    lyr.SetSpatialFilterRect(-10, -10, 0, 0)
    assert lyr.GetFeatureCount() == 1
    assert set(f.GetFID() for f in lyr) == set([3])

    lyr.SetSpatialFilterRect(100, 100, 110, 110)
    assert lyr.GetFeatureCount() == 0
    assert set(f.GetFID() for f in lyr) == set()

    lyr.SetSpatialFilter(None)

    lyr.SetAttributeFilter("strfield = 'foo'")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([1])

    lyr.SetAttributeFilter("'foo' = strfield")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([1])

    lyr.SetAttributeFilter("strfield = 'non_existing'")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set()

    lyr.SetAttributeFilter("strfield <> 'foo'")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    res = set(f.GetFID() for f in lyr)
    assert 3 in res
    assert 1 not in res

    lyr.SetAttributeFilter("'foo' <> strfield")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    res = set(f.GetFID() for f in lyr)
    assert 3 in res
    assert 1 not in res

    lyr.SetAttributeFilter("intfield = -123456789")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([1])

    lyr.SetAttributeFilter("intfield = -123456789.0")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([1])

    lyr.SetAttributeFilter("intfield = -9876543")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([])

    lyr.SetAttributeFilter("intfield >= 123456790")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set()

    lyr.SetAttributeFilter("123456790 <= intfield")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set()

    lyr.SetAttributeFilter("intfield >= 123456789")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([3])

    lyr.SetAttributeFilter("123456789 <= intfield")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([3])

    lyr.SetAttributeFilter("intfield > 123456788")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([3])

    lyr.SetAttributeFilter("123456788 < intfield")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([3])

    lyr.SetAttributeFilter("intfield > 123456789")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([])

    lyr.SetAttributeFilter("123456789 < intfield")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([])

    lyr.SetAttributeFilter("intfield < -123456788")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([1])

    lyr.SetAttributeFilter("-123456788 > intfield")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([1])

    lyr.SetAttributeFilter("-123456788 > intfield")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([1])

    lyr.SetAttributeFilter("intfield < -123456789")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([])

    lyr.SetAttributeFilter("-123456789 > intfield")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([])

    lyr.SetAttributeFilter("intfield <= -123456790")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set()

    lyr.SetAttributeFilter("-123456790 >= intfield")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set()

    lyr.SetAttributeFilter("intfield <= -123456789")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([1])

    lyr.SetAttributeFilter("-123456789 >= intfield")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([1])

    lyr.SetAttributeFilter("boolfield = 1")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([1])

    lyr.SetAttributeFilter("boolfield = 0")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == (set([3]) if nullable else set([2, 3]))

    # Out of domain
    lyr.SetAttributeFilter("boolfield = 2")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([])

    lyr.SetAttributeFilter("boolfield <> 2")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == (set([1, 3]) if nullable else set([1, 2, 3]))

    # Out of domain
    lyr.SetAttributeFilter("int16field = -32769")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([])

    lyr.SetAttributeFilter("int16field <> -32769")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == (set([1, 3]) if nullable else set([1, 2, 3]))

    lyr.SetAttributeFilter("int16field > -32769")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == (set([1, 3]) if nullable else set([1, 2, 3]))

    lyr.SetAttributeFilter("int16field >= -32769")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == (set([1, 3]) if nullable else set([1, 2, 3]))

    lyr.SetAttributeFilter("int16field < -32769")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set()

    lyr.SetAttributeFilter("int16field <= -32769")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set()

    lyr.SetAttributeFilter("int16field = 32768")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([])

    lyr.SetAttributeFilter("int16field <> 32768")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == (set([1, 3]) if nullable else set([1, 2, 3]))

    lyr.SetAttributeFilter("int16field < 32768")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == (set([1, 3]) if nullable else set([1, 2, 3]))

    lyr.SetAttributeFilter("int16field <= 32768")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == (set([1, 3]) if nullable else set([1, 2, 3]))

    lyr.SetAttributeFilter("int16field > 32768")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set()

    lyr.SetAttributeFilter("int16field >= 32768")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set()

    lyr.SetAttributeFilter("int64field = 1234567890123456")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([3])

    lyr.SetAttributeFilter("int64field = 1234567890123456.0")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([3])

    lyr.SetAttributeFilter("int64field > 2000000000")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([3])

    lyr.SetAttributeFilter("doublefield = 1.2345")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([1])

    lyr.SetAttributeFilter("doublefield = 1.2345999")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([])

    lyr.SetAttributeFilter("doublefield = 1")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([])

    lyr.SetAttributeFilter("floatfield = 1.5")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([1])

    lyr.SetAttributeFilter("datetimefield = '2023-04-07T12:34:56.789Z'")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([1])

    with pytest.raises(Exception):
        lyr.SetAttributeFilter("datetimefield = 'invalid'")

    lyr.SetAttributeFilter("datefield = '2023-04-07'")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([1])

    with pytest.raises(Exception):
        lyr.SetAttributeFilter("datefield = 'invalid'")

    lyr.SetAttributeFilter("timefield = '12:34:56.789'")
    # timefield comparison not supported by tiledb currently
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "NONE"
    assert set(f.GetFID() for f in lyr) == set([1])

    # Test AND
    lyr.SetAttributeFilter("int16field = -32768 AND intfield = -123456789")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([1])

    lyr.SetAttributeFilter("int16field = 0 AND intfield = -123456789")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([])

    lyr.SetAttributeFilter("intfield = -123456789 AND int16field = 0")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([])

    lyr.SetAttributeFilter("intfield = -123456789 AND (1 = 1)")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "PARTIAL"
    assert set(f.GetFID() for f in lyr) == set([1])

    lyr.SetAttributeFilter("(1 = 1) AND intfield = -123456789")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "PARTIAL"
    assert set(f.GetFID() for f in lyr) == set([1])

    # Test OR
    lyr.SetAttributeFilter("intfield = 321 OR intfield = -123456789")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([1])

    lyr.SetAttributeFilter("intfield = -123456789 OR intfield = 321")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([1])

    lyr.SetAttributeFilter("intfield = 321 OR intfield = 123")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([])

    lyr.SetAttributeFilter("(1 = 1) OR intfield = -123456789")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "NONE"
    assert set(f.GetFID() for f in lyr) == set([1, 2, 3])

    lyr.SetAttributeFilter("(1 = 0) OR intfield = -123456789")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "NONE"
    assert set(f.GetFID() for f in lyr) == set([1])

    lyr.SetAttributeFilter("intfield = -123456789 OR (1 = 1)")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "NONE"
    assert set(f.GetFID() for f in lyr) == set([1, 2, 3])

    lyr.SetAttributeFilter("intfield = -123456789 OR (1 = 0)")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "NONE"
    assert set(f.GetFID() for f in lyr) == set([1])

    # Test NOT
    lyr.SetAttributeFilter("NOT (intfield = -123456789)")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == (set([3]) if nullable else set([2, 3]))

    # Test IN
    lyr.SetAttributeFilter("intfield IN (321, -123456789)")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([1])

    lyr.SetAttributeFilter("intfield IN (-123456789, 321)")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([1])

    # Test IS NULL / IS NOT NULL
    lyr.SetAttributeFilter("strfield IS NULL")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == (set([2]) if nullable else set())

    lyr.SetAttributeFilter("strfield IS NOT NULL")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == (set([1, 3]) if nullable else set([1, 2, 3]))

    lyr.SetAttributeFilter("intfield IS NULL")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == (set([2]) if nullable else set())

    lyr.SetAttributeFilter("intfield IS NOT NULL")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == (set([1, 3]) if nullable else set([1, 2, 3]))

    # Test IS NULL and AND (for always_false situations)

    lyr.SetAttributeFilter("intfield IS NULL AND intfieldextra <> 4")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == (set([2]) if nullable else set())

    lyr.SetAttributeFilter("intfield IS NULL AND intfield IS NULL")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == (set([2]) if nullable else set())

    lyr.SetAttributeFilter("intfieldextra <> 4 AND intfield IS NULL")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == (set([2]) if nullable else set())

    lyr.SetAttributeFilter("intfield IS NULL AND intfieldextra = 4")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set()

    lyr.SetAttributeFilter("intfieldextra = 4 AND intfield IS NULL")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set()

    # Test IS NOT NULL and AND (for always_true situations)

    lyr.SetAttributeFilter("intfield IS NOT NULL AND intfieldextra <> 4")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == (set([1, 3]) if nullable else set([1, 2, 3]))

    lyr.SetAttributeFilter("intfield IS NOT NULL AND intfield IS NOT NULL")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == (set([1, 3]) if nullable else set([1, 2, 3]))

    lyr.SetAttributeFilter("intfieldextra <> 4 AND intfield IS NOT NULL")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == (set([1, 3]) if nullable else set([1, 2, 3]))

    lyr.SetAttributeFilter("intfield IS NOT NULL AND intfieldextra = 4")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set()

    lyr.SetAttributeFilter("intfieldextra = 4 AND intfield IS NOT NULL")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set()

    # Test IS NULL and OR (for always_false situations)
    lyr.SetAttributeFilter("intfield IS NULL OR intfieldextra <> 4")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([1, 2, 3])

    lyr.SetAttributeFilter("intfield IS NULL OR intfield IS NULL")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == (set([2]) if nullable else set())

    lyr.SetAttributeFilter("intfieldextra <> 4 OR intfield IS NULL")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([1, 2, 3])

    lyr.SetAttributeFilter("intfield IS NULL OR intfieldextra = 4")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == (set([2]) if nullable else set())

    lyr.SetAttributeFilter("intfieldextra = 4 OR intfield IS NULL")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == (set([2]) if nullable else set())

    # Test IS NOT NULL and OR (for always_true situations)
    lyr.SetAttributeFilter("intfield IS NOT NULL OR intfieldextra <> 4")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([1, 2, 3])

    lyr.SetAttributeFilter("intfield IS NOT NULL OR intfield IS NOT NULL")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == (set([1, 3]) if nullable else set([1, 2, 3]))

    lyr.SetAttributeFilter("intfieldextra <> 4 OR intfield IS NOT NULL")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == set([1, 2, 3])

    lyr.SetAttributeFilter("intfield IS NOT NULL OR intfieldextra = 4")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == (set([1, 3]) if nullable else set([1, 2, 3]))

    lyr.SetAttributeFilter("intfieldextra = 4 OR intfield IS NOT NULL")
    assert lyr.GetMetadataItem("ATTRIBUTE_FILTER_TRANSLATION", "_DEBUG_") == "WHOLE"
    assert set(f.GetFID() for f in lyr) == (set([1, 3]) if nullable else set([1, 2, 3]))

    tiledb_md = json.loads(lyr.GetMetadata_List("json:TILEDB")[0])
    md = tiledb_md["array"]["metadata"]
    del md["CRS"]
    assert md == {
        "dataset_type": {"type": "STRING_UTF8", "value": "geometry"},
        "FEATURE_COUNT": {"type": "INT64", "value": 3},
        "FID_ATTRIBUTE_NAME": {"type": "STRING_UTF8", "value": "FID"},
        "GEOMETRY_ATTRIBUTE_NAME": {"type": "STRING_UTF8", "value": "wkb_geometry"},
        "GeometryType": {"type": "STRING_ASCII", "value": "Unknown"},
        "LAYER_EXTENT_MAXX": {"type": "FLOAT64", "value": 4.0},
        "LAYER_EXTENT_MAXY": {"type": "FLOAT64", "value": 3.0},
        "LAYER_EXTENT_MINX": {"type": "FLOAT64", "value": -10.0},
        "LAYER_EXTENT_MINY": {"type": "FLOAT64", "value": -20.0},
        "PAD_X": {"type": "FLOAT64", "value": 4.5},
        "PAD_Y": {"type": "FLOAT64", "value": 8.5},
    }

    ds = None

    shutil.rmtree("tmp/test.tiledb")


###############################################################################


@pytest.mark.parametrize(
    "wkt",
    [
        "POINT (1 2)",
        "POINT Z (1 2 3)",
        "POINT M (1 2 3)",
        "POINT ZM (1 2 3 4)",
        "LINESTRING (1 2,3 4)",
        "POLYGON ((0 0,0 1,1 1,0 0))",
        "MULTIPOINT ((0 0))",
        "MULTILINESTRING ((1 2,3 4))",
        "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))",
        "GEOMETRYCOLLECTION (POINT (1 2))",
        "CIRCULARSTRING (0 0,1 1,2 0)",
        "COMPOUNDCURVE ((1 2,3 4))",
        "CURVEPOLYGON ((0 0,0 1,1 1,0 0))",
        "MULTICURVE ((1 2,3 4))",
        "MULTISURFACE (((0 0,0 1,1 1,0 0)))",
        "POLYHEDRALSURFACE (((0 0,0 1,1 1,0 0)))",
        "TIN (((0 0,0 1,1 1,0 0)))",
    ],
)
def test_ogr_tiledb_geometry_types(wkt):

    if os.path.exists("tmp/test.tiledb"):
        shutil.rmtree("tmp/test.tiledb")

    g = ogr.CreateGeometryFromWkt(wkt)
    ds = ogr.GetDriverByName("TileDB").CreateDataSource("tmp/test.tiledb")
    options = ["BOUNDS=-1e4,-1e4,1e4,1e4"]
    if g.GetGeometryType() in (ogr.wkbPoint, ogr.wkbPoint25D):
        options += ["GEOMETRY_NAME="]
    lyr = ds.CreateLayer("test", geom_type=g.GetGeometryType(), options=options)
    assert lyr.GetDataset().GetDescription() == ds.GetDescription()
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(g)
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    ds = None

    ds = ogr.Open("tmp/test.tiledb")
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == g.GetGeometryType()
    if g.GetGeometryType() in (ogr.wkbPoint, ogr.wkbPoint25D):
        assert lyr.GetGeometryColumn() == ""
    else:
        assert lyr.GetGeometryColumn() == "wkb_geometry"
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToIsoWkt() == wkt
    ds = None

    shutil.rmtree("tmp/test.tiledb")


###############################################################################


def test_ogr_tiledb_compression():

    if os.path.exists("tmp/test.tiledb"):
        shutil.rmtree("tmp/test.tiledb")

    ds = ogr.GetDriverByName("TileDB").CreateDataSource("tmp/test.tiledb")
    lyr = ds.CreateLayer(
        "test", options=["BOUNDS=-1e4,-1e4,1e4,1e4", "COMPRESSION=ZSTD"]
    )
    for typ, subtype in [
        (ogr.OFTInteger, ogr.OFSTNone),
        (ogr.OFTInteger, ogr.OFSTBoolean),
        (ogr.OFTInteger, ogr.OFSTInt16),
        (ogr.OFTReal, ogr.OFSTNone),
        (ogr.OFTReal, ogr.OFSTFloat32),
        (ogr.OFTInteger64, ogr.OFSTNone),
        (ogr.OFTIntegerList, ogr.OFSTNone),
        (ogr.OFTIntegerList, ogr.OFSTBoolean),
        (ogr.OFTIntegerList, ogr.OFSTInt16),
        (ogr.OFTRealList, ogr.OFSTNone),
        (ogr.OFTRealList, ogr.OFSTFloat32),
        (ogr.OFTInteger64List, ogr.OFSTNone),
        (ogr.OFTString, ogr.OFSTNone),
        (ogr.OFTBinary, ogr.OFSTNone),
        (ogr.OFTTime, ogr.OFSTNone),
        (ogr.OFTDate, ogr.OFSTNone),
        (ogr.OFTDateTime, ogr.OFSTNone),
    ]:
        fld_defn = ogr.FieldDefn("field%d_subtype%d" % (typ, subtype), typ)
        fld_defn.SetSubType(subtype)
        lyr.CreateField(fld_defn)
    ds = None

    ds = ogr.Open("tmp/test.tiledb")
    lyr = ds.GetLayer(0)
    tiledb_md = json.loads(lyr.GetMetadata_List("json:TILEDB")[0])
    ds = None

    assert tiledb_md["schema"]["coords_filter_list"] == ["ZSTD"]
    for attr in tiledb_md["schema"]["attributes"]:
        assert attr["filter_list"] == ["ZSTD"], attr

    shutil.rmtree("tmp/test.tiledb")


###############################################################################
# Run test_ogrsf


@pytest.mark.skipif(
    test_cli_utilities.get_test_ogrsf_path() is None, reason="test_ogrsf not available"
)
def test_ogr_tiledb_test_ogrsf():

    if os.path.exists("tmp/poly.tiledb"):
        shutil.rmtree("tmp/poly.tiledb")

    gdal.VectorTranslate("tmp/poly.tiledb", "data/poly.shp", format="TileDB")

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + " tmp/poly.tiledb"
    )

    shutil.rmtree("tmp/poly.tiledb")

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################


def test_ogr_tiledb_dimension_names_open_option():

    if os.path.exists("tmp/test.tiledb"):
        shutil.rmtree("tmp/test.tiledb")

    ds = ogr.GetDriverByName("TileDB").CreateDataSource("tmp/test.tiledb")
    lyr = ds.CreateLayer(
        "test",
        geom_type=ogr.wkbPoint,
        options=["BOUNDS=-1e4,-1e4,1e4,1e4", "FID=", "GEOMETRY_NAME="],
    )
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    ds = None

    ds = ogr.Open("tmp/test.tiledb")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToIsoWkt() == "POINT (1 2)"
    ds = None

    ds = gdal.OpenEx("tmp/test.tiledb", open_options=["DIM_X=_Y", "DIM_Y=_X"])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToIsoWkt() == "POINT (2 1)"
    ds = None

    with pytest.raises(Exception):
        gdal.OpenEx("tmp/test.tiledb", open_options=["DIM_X=invalid", "DIM_Y=_Y"])

    with pytest.raises(Exception):
        gdal.OpenEx(
            "tmp/test.tiledb",
            gdal.OF_UPDATE,
            open_options=["DIM_X=invalid", "DIM_Y=_Y"],
        )

    shutil.rmtree("tmp/test.tiledb")


###############################################################################


def test_ogr_tiledb_switch_between_read_and_write():

    if os.path.exists("tmp/test.tiledb"):
        shutil.rmtree("tmp/test.tiledb")

    ds = ogr.GetDriverByName("TileDB").CreateDataSource("tmp/test.tiledb")
    lyr = ds.CreateLayer("test", options=["BOUNDS=-1e4,-1e4,1e4,1e4"])
    lyr.ResetReading()
    assert lyr.TestCapability(ogr.OLCSequentialWrite)
    assert lyr.TestCapability(ogr.OLCCreateField)
    assert lyr.CreateField(ogr.FieldDefn("intfield", ogr.OFTInteger)) == ogr.OGRERR_NONE
    f = ogr.Feature(lyr.GetLayerDefn())
    f["intfield"] = 1
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert f.GetFID() == 1

    assert lyr.TestCapability(ogr.OLCCreateField) == 0
    with pytest.raises(Exception):
        lyr.CreateField(ogr.FieldDefn("intfield2", ogr.OFTInteger))

    f = ogr.Feature(lyr.GetLayerDefn())
    f["intfield"] = 2
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (2 3)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert f.GetFID() == 2

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f.GetFID() == 1

    f = ogr.Feature(lyr.GetLayerDefn())
    f["intfield"] = 3
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 4)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert f.GetFID() == 3

    f = lyr.GetNextFeature()
    assert f.GetFID() == 1

    f = lyr.GetNextFeature()
    assert f.GetFID() == 2

    f = lyr.GetNextFeature()
    assert f.GetFID() == 3

    f = lyr.GetNextFeature()
    assert f is None

    ds = None

    ds = ogr.Open("tmp/test.tiledb", update=1)
    lyr = ds.GetLayer(0)
    assert lyr.TestCapability(ogr.OLCSequentialWrite)
    assert lyr.GetFeatureCount() == 3

    f = ogr.Feature(lyr.GetLayerDefn())
    f["intfield"] = 4
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (4 5)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert f.GetFID() == 4

    f = lyr.GetNextFeature()
    assert f.GetFID() == 1

    f = lyr.GetNextFeature()
    assert f.GetFID() == 2

    f = lyr.GetNextFeature()
    assert f.GetFID() == 3

    f = lyr.GetNextFeature()
    assert f.GetFID() == 4

    f = lyr.GetNextFeature()
    assert f is None

    ds = None

    shutil.rmtree("tmp/test.tiledb")


###############################################################################


def test_ogr_tiledb_create_group():

    if os.path.exists("tmp/test.tiledb"):
        shutil.rmtree("tmp/test.tiledb")

    ds = ogr.GetDriverByName("TileDB").CreateDataSource(
        "tmp/test.tiledb", options=["CREATE_GROUP=YES"]
    )
    assert ds.TestCapability(ogr.ODsCCreateLayer)
    lyr = ds.CreateLayer("test", options=["BOUNDS=-1e4,-1e4,1e4,1e4"])
    lyr.CreateField(ogr.FieldDefn("field", ogr.OFTString))
    assert ds.TestCapability(ogr.ODsCCreateLayer)
    lyr2 = ds.CreateLayer("test2", options=["BOUNDS=-1e4,-1e4,1e4,1e4"])
    lyr2.CreateField(ogr.FieldDefn("field2", ogr.OFTString))
    ds = None

    assert os.path.exists("tmp/test.tiledb/layers/test")
    assert os.path.exists("tmp/test.tiledb/layers/test2")

    ds = ogr.Open("tmp/test.tiledb")
    assert ds.GetLayerCount() == 2
    lyr = ds.GetLayerByName("test")
    assert lyr
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "field"
    lyr = ds.GetLayerByName("test2")
    assert lyr
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "field2"

    # Cannot create layer: read-only connection
    assert ds.TestCapability(ogr.ODsCCreateLayer) == 0
    with pytest.raises(Exception):
        ds.CreateLayer("failed", options=["BOUNDS=-1e4,-1e4,1e4,1e4"])
    ds = None

    ds = ogr.Open("tmp/test.tiledb", update=1)
    assert ds.TestCapability(ogr.ODsCCreateLayer)
    lyr = ds.CreateLayer("test3", options=["BOUNDS=-1e4,-1e4,1e4,1e4"])
    assert lyr
    ds = None

    assert os.path.exists("tmp/test.tiledb/layers/test3")

    ds = ogr.Open("tmp/test.tiledb")
    assert ds.GetLayerCount() == 3
    lyr = ds.GetLayerByName("test3")
    assert lyr
    ds = None

    shutil.rmtree("tmp/test.tiledb")


###############################################################################


def test_ogr_tiledb_errors():

    if os.path.exists("tmp/test.tiledb"):
        shutil.rmtree("tmp/test.tiledb")

    ds = ogr.GetDriverByName("TileDB").CreateDataSource("tmp/test.tiledb")

    with pytest.raises(Exception):
        ds.CreateLayer("test", geom_type=ogr.wkbNone)

    with pytest.raises(Exception):
        ds.CreateLayer("test")  # missing bounds

    with pytest.raises(Exception):
        ds.CreateLayer("test", options=["BOUNDS=invalid"])

    lyr = ds.CreateLayer("test", options=["BOUNDS=1,2,3,4,5,6", "ADD_Z_DIM=YES"])

    with pytest.raises(Exception):
        ds.CreateLayer("another_layer", options=["BOUNDS=1,2,3,4,5,6"])

    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    for field_name in ("FID", "wkb_geometry", "_X", "_Y", "_Z", "foo"):
        # Existing field name
        with pytest.raises(Exception):
            lyr.CreateField(ogr.FieldDefn("FID", ogr.OFTString))

    with pytest.raises(Exception):
        # feature without geom
        lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))

    # feature with empty geom
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.Geometry(ogr.wkbPoint))
    with pytest.raises(Exception):
        lyr.CreateFeature(f)
    ds = None

    shutil.rmtree("tmp/test.tiledb")


###############################################################################


@pytest.mark.parametrize("nullable,batch_size", [(True, None), (False, 2)])
def test_ogr_tiledb_arrow_stream_pyarrow(nullable, batch_size):
    pytest.importorskip("pyarrow")

    if os.path.exists("tmp/test.tiledb"):
        shutil.rmtree("tmp/test.tiledb")

    _, _, options = create_tiledb_dataset(nullable, batch_size)

    ds = gdal.OpenEx("tmp/test.tiledb", open_options=options)
    lyr = ds.GetLayer(0)

    mapFeatures = {}
    for f in lyr:
        mapFeatures[f.GetFID()] = f

    stream = lyr.GetArrowStreamAsPyArrow()
    schema = stream.schema
    fields = set(
        [
            (schema.field(i).name, str(schema.field(i).type))
            for i in range(schema.num_fields)
        ]
    )
    expected_fields = set(
        [
            ("FID", "int64"),
            ("strfield", "large_string"),
            ("intfield", "int32"),
            ("int16field", "int16"),
            ("int64field", "int64"),
            ("uint8field", "uint8"),
            ("uint16field", "uint16"),
            ("doublefield", "double"),
            ("floatfield", "float"),
            ("binaryfield", "large_binary"),
            ("intlistfield", "large_list<item: int32 not null>"),
            ("int16listfield", "large_list<item: int16 not null>"),
            ("doublelistfield", "large_list<item: double not null>"),
            ("floatlistfield", "large_list<item: float not null>"),
            ("datetimefield", "timestamp[ms]"),
            ("datefield", "date32[day]"),
            ("timefield", "time32[ms]"),
            ("intfieldextra", "int32"),
            ("wkb_geometry", "large_binary"),
            ("boolfield", "bool"),
            ("boollistfield", "large_list<item: bool not null>"),
        ]
    )
    assert fields == expected_fields

    def check_batch(batch):
        for idx, fid in enumerate(batch.field("FID")):
            f = mapFeatures[fid.as_py()]
            for field_idx in range(lyr.GetLayerDefn().GetFieldCount()):
                field_defn = lyr.GetLayerDefn().GetFieldDefn(field_idx)
                got_val = batch.field(field_defn.GetName())[idx].as_py()
                field_type = field_defn.GetType()
                if field_type == ogr.OFTDateTime:
                    if f.IsFieldSetAndNotNull(field_idx):
                        expected_val = f.GetFieldAsDateTime(field_idx)
                        assert [
                            got_val.year,
                            got_val.month,
                            got_val.day,
                            got_val.hour,
                            got_val.minute,
                            got_val.second + got_val.microsecond * 1e-6,
                        ] == pytest.approx(
                            expected_val[0:-1], abs=1e-4
                        ), field_defn.GetName()
                elif field_type == ogr.OFTBinary:
                    if f.IsFieldSetAndNotNull(field_idx):
                        got_val = bytes(got_val)
                        assert got_val == f.GetFieldAsBinary(
                            field_idx
                        ), field_defn.GetName()
                    else:
                        assert got_val is None, field_defn.GetName()
                elif field_type not in (
                    ogr.OFTDate,
                    ogr.OFTTime,
                ):
                    if isinstance(got_val, float) and math.isnan(got_val):
                        assert math.isnan(f.GetField(field_idx)), field_defn.GetName()
                    else:
                        assert got_val == f.GetField(field_idx), field_defn.GetName()

    for batch in stream:
        check_batch(batch)

    # Collect all batches (that is do not release them immediately)
    stream = lyr.GetArrowStreamAsPyArrow()
    batches = [batch for batch in stream]
    for batch in batches:
        check_batch(batch)

    ds = None

    shutil.rmtree("tmp/test.tiledb")


###############################################################################


@pytest.mark.parametrize("nullable,batch_size", [(True, None), (False, 2)])
def test_ogr_tiledb_arrow_stream_numpy(nullable, batch_size):
    gdaltest.importorskip_gdal_array()
    numpy = pytest.importorskip("numpy")
    import datetime

    if os.path.exists("tmp/test.tiledb"):
        shutil.rmtree("tmp/test.tiledb")

    _, _, options = create_tiledb_dataset(nullable, batch_size, extra_feature=True)

    ds = gdal.OpenEx("tmp/test.tiledb", open_options=options)
    lyr = ds.GetLayer(0)

    mapFeatures = {}
    for f in lyr:
        mapFeatures[f.GetFID()] = f

    ds = gdal.OpenEx("tmp/test.tiledb", open_options=options)
    lyr = ds.GetLayer(0)

    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])

    def check_batch(batch):
        for idx, fid in enumerate(batch["FID"]):
            f = mapFeatures[fid]

            for field_idx in range(lyr.GetLayerDefn().GetFieldCount()):
                field_defn = lyr.GetLayerDefn().GetFieldDefn(field_idx)
                got_val = batch[field_defn.GetName()][idx]
                field_type = field_defn.GetType()
                if field_type in (ogr.OFTDateTime, ogr.OFTDate):
                    if f.IsFieldSetAndNotNull(field_idx):
                        expected_val = f.GetFieldAsDateTime(field_idx)
                        # Convert numpy.datetime64 to datetime.datetime
                        got_val = (
                            got_val - numpy.datetime64("1970-01-01T00:00:00")
                        ) / numpy.timedelta64(1, "s")
                        got_val = datetime.datetime.fromtimestamp(
                            got_val, tz=datetime.timezone.utc
                        )
                        assert [
                            got_val.year,
                            got_val.month,
                            got_val.day,
                            got_val.hour,
                            got_val.minute,
                            got_val.second + got_val.microsecond * 1e-6,
                        ] == pytest.approx(
                            expected_val[0:-1], abs=1e-4
                        ), field_defn.GetName()
                elif field_type == ogr.OFTString:
                    if f.IsFieldSetAndNotNull(field_idx):
                        assert got_val == f.GetField(field_idx).encode(
                            "UTF-8"
                        ), field_defn.GetName()
                    else:
                        assert len(got_val) == 0, field_defn.GetName()
                elif field_type == ogr.OFTBinary:
                    if f.IsFieldSetAndNotNull(field_idx):
                        got_val = bytes(got_val)
                        assert got_val == f.GetFieldAsBinary(
                            field_idx
                        ), field_defn.GetName()
                    else:
                        assert got_val is None, field_defn.GetName()
                else:
                    if f.IsFieldSetAndNotNull(field_idx):
                        if (
                            isinstance(got_val, numpy.float64)
                            or isinstance(got_val, numpy.float32)
                        ) and math.isnan(got_val):
                            assert math.isnan(
                                f.GetField(field_idx)
                            ), field_defn.GetName()
                        else:
                            expected_val = f.GetField(field_idx)
                            if isinstance(expected_val, list):
                                got_val = list(got_val)
                                assert got_val == expected_val, field_defn.GetName()

            got_geom = ogr.CreateGeometryFromWkb(batch["wkb_geometry"][idx])
            assert got_geom.ExportToIsoWkt() == f.GetGeometryRef().ExportToIsoWkt()

    for batch in stream:
        expected_fields = {
            "FID",
            "strfield",
            "intfield",
            "int16field",
            "int64field",
            "uint8field",
            "uint16field",
            "doublefield",
            "floatfield",
            "binaryfield",
            "intlistfield",
            "int16listfield",
            "doublelistfield",
            "floatlistfield",
            "datetimefield",
            "datefield",
            "timefield",
            "intfieldextra",
            "wkb_geometry",
            "boolfield",
            "boollistfield",
        }
        assert batch.keys() == expected_fields

        check_batch(batch)

    # Collect all batches (that is do not release them immediately)
    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    batches = [batch for batch in stream]
    assert len(batches) == (1 if batch_size is None else 2)

    for batch in batches:
        check_batch(batch)

    # Test spatial filter that intersects all features
    minx, maxx, miny, maxy = lyr.GetExtent()
    lyr.SetSpatialFilterRect(minx + 0.5, miny + 0.5, maxx - 0.5, maxy - 0.5)
    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    fids = []
    for batch in stream:
        fids += batch["FID"].tolist()
        check_batch(batch)
    assert set(fids) == set([1, 2, 3, 4])
    lyr.SetSpatialFilter(None)

    # Test spatial filter that intersects 1st, 2nd and 4th features only, but given how
    # spatial filtering works, more intermediate features will be collected
    # before being discarded
    lyr.SetSpatialFilterRect(-0.99, -2.99, 3, 3)
    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    fids = []
    for batch in stream:
        fids += batch["FID"].tolist()
        check_batch(batch)
    assert set(fids) == set([1, 2, 4])
    lyr.SetSpatialFilter(None)

    # Test spatial filter that intersects 1st and 2nd features only, but given how
    # spatial filtering works, more intermediate features will be collected
    # before being discarded
    lyr.SetSpatialFilterRect(0, 0, 3, 3)
    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    fids = []
    for batch in stream:
        fids += batch["FID"].tolist()
        check_batch(batch)
    assert set(fids) == set([1, 2])
    lyr.SetSpatialFilter(None)

    # Test spatial filter that intersects 3rd feature only
    lyr.SetSpatialFilterRect(-3, -3, -0.95, -0.95)
    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    fids = []
    for batch in stream:
        fids += batch["FID"].tolist()
        check_batch(batch)
    assert set(fids) == set([3])
    lyr.SetSpatialFilter(None)

    # Test spatial filter that intersects 4th feature only
    lyr.SetSpatialFilterRect(-0.95, -0.95, -0.85, -0.85)
    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    fids = []
    for batch in stream:
        fids += batch["FID"].tolist()
        check_batch(batch)
    assert set(fids) == set([4])
    lyr.SetSpatialFilter(None)

    # Test spatial filter that intersects no feature
    lyr.SetSpatialFilterRect(-0.5, -0.5, 0.5, 0.5)
    stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
    fids = []
    for batch in stream:
        fids += batch["FID"].tolist()
    assert set(fids) == set()
    lyr.SetSpatialFilter(None)

    stream = lyr.GetArrowStreamAsNumPy(
        options=["USE_MASKED_ARRAYS=NO", "INCLUDE_FID=NO", "MAX_FEATURES_IN_BATCH=1000"]
    )
    batches = [batch for batch in stream]
    assert len(batches) == 1
    batch = batches[0]
    assert "FID" not in batch.keys()
    strfield_values = [x.decode("UTF-8") for x in batch["strfield"]]
    assert "foo" in strfield_values
    assert "barbaz" in strfield_values
    got_geom = ogr.CreateGeometryFromWkb(batch["wkb_geometry"][0])
    assert got_geom

    lyr.SetIgnoredFields(["strfield"])
    stream = lyr.GetArrowStreamAsNumPy(
        options=["USE_MASKED_ARRAYS=NO", "MAX_FEATURES_IN_BATCH=1000"]
    )
    batches = [batch for batch in stream]
    assert len(batches) == 1
    batch = batches[0]
    assert "strfield" not in batch.keys()
    assert "intfield" in batch.keys()
    assert set([x for x in batch["intfield"]]) == set(
        [-123456789, 0, 123456789, 8765432]
    )

    lyr.SetIgnoredFields(["wkb_geometry"])
    stream = lyr.GetArrowStreamAsNumPy(
        options=["USE_MASKED_ARRAYS=NO", "MAX_FEATURES_IN_BATCH=1000"]
    )
    batches = [batch for batch in stream]
    assert len(batches) == 1
    batch = batches[0]
    assert "wkb_geometry" not in batch.keys()
    assert "intfield" in batch.keys()
    assert set([x for x in batch["intfield"]]) == set(
        [-123456789, 0, 123456789, 8765432]
    )

    ds = None

    shutil.rmtree("tmp/test.tiledb")


###############################################################################


def test_ogr_tiledb_arrow_stream_numpy_point_no_wkb_geometry_col():
    gdaltest.importorskip_gdal_array()
    pytest.importorskip("numpy")

    if os.path.exists("tmp/test.tiledb"):
        shutil.rmtree("tmp/test.tiledb")

    ds = ogr.GetDriverByName("TileDB").CreateDataSource("tmp/test.tiledb")
    srs = osr.SpatialReference()
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    srs.ImportFromEPSG(4326)
    lyr = ds.CreateLayer(
        "test", srs=srs, geom_type=ogr.wkbPoint, options=["GEOMETRY_NAME="]
    )
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 4)"))
    lyr.CreateFeature(f)
    ds = None

    ds = gdal.OpenEx("tmp/test.tiledb")
    lyr = ds.GetLayer(0)

    stream = lyr.GetArrowStreamAsNumPy()
    batches = [batch for batch in stream]
    assert len(batches) == 1
    batch = batches[0]
    for idx, fid in enumerate(batch["FID"]):
        got_geom = ogr.CreateGeometryFromWkb(batch["wkb_geometry"][idx])
        if fid == 1:
            assert got_geom.ExportToIsoWkt() == "POINT (1 2)"
        else:
            assert got_geom.ExportToIsoWkt() == "POINT (3 4)"

    ds = None

    shutil.rmtree("tmp/test.tiledb")


###############################################################################


def test_ogr_tiledb_arrow_stream_numpy_pointz_no_fid_and_wkb_geometry_col():
    gdaltest.importorskip_gdal_array()
    pytest.importorskip("numpy")

    if os.path.exists("tmp/test.tiledb"):
        shutil.rmtree("tmp/test.tiledb")

    ds = ogr.GetDriverByName("TileDB").CreateDataSource("tmp/test.tiledb")
    srs = osr.SpatialReference()
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    srs.ImportFromEPSG(4326)
    lyr = ds.CreateLayer(
        "test", srs=srs, geom_type=ogr.wkbPoint25D, options=["FID=", "GEOMETRY_NAME="]
    )
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT Z (1 2 3)"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT Z (4 5 6)"))
    lyr.CreateFeature(f)
    ds = None

    ds = gdal.OpenEx("tmp/test.tiledb")
    lyr = ds.GetLayer(0)

    stream = lyr.GetArrowStreamAsNumPy()
    batches = [batch for batch in stream]
    assert len(batches) == 1
    batch = batches[0]
    assert [x for x in batch["OGC_FID"]] == [1, 2]
    assert set(
        [ogr.CreateGeometryFromWkb(x).ExportToIsoWkt() for x in batch["wkb_geometry"]]
    ) == {"POINT Z (1 2 3)", "POINT Z (4 5 6)"}

    ds = None

    shutil.rmtree("tmp/test.tiledb")


###############################################################################


def test_ogr_tiledb_arrow_stream_numpy_detailed_spatial_filter():
    gdaltest.importorskip_gdal_array()
    pytest.importorskip("numpy")

    if os.path.exists("tmp/test.tiledb"):
        shutil.rmtree("tmp/test.tiledb")

    ds = ogr.GetDriverByName("TileDB").CreateDataSource("tmp/test.tiledb")
    srs = osr.SpatialReference()
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    srs.ImportFromEPSG(32631)
    lyr = ds.CreateLayer("test", srs=srs, options=["FID=fid"])
    for idx, wkt in enumerate(
        [
            "POINT(1 2)",
            "MULTIPOINT(0 0,1 2)",
            "LINESTRING(3 4,5 6)",
            "MULTILINESTRING((7 8,7.5 8.5),(3 4,5 6))",
            "POLYGON((10 20,10 30,20 30,10 20),(11 21,11 29,19 29,11 21))",
            "MULTIPOLYGON(((100 100,100 200,200 200,100 100)),((10 20,10 30,20 30,10 20),(11 21,11 29,19 29,11 21)))",
        ]
    ):
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(idx)
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt(wkt))
        lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open("tmp/test.tiledb")
    lyr = ds.GetLayer(0)

    eps = 1e-1

    # Select nothing
    with ogrtest.spatial_filter(lyr, 6, 0, 8, 1):
        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        batches = [batch for batch in stream]
        assert len(batches) == 1
        assert list(batches[0]["fid"]) == []

    # Select POINT and MULTIPOINT
    with ogrtest.spatial_filter(lyr, 1 - eps, 2 - eps, 1 + eps, 2 + eps):
        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        batches = [batch for batch in stream]
        assert len(batches) == 1
        assert set(list(batches[0]["fid"])) == set([0, 1])
        assert set([f.GetFID() for f in lyr]) == set([0, 1])

    # Select LINESTRING and MULTILINESTRING due to point falling in bbox
    with ogrtest.spatial_filter(lyr, 3 - eps, 4 - eps, 3 + eps, 4 + eps):
        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        batches = [batch for batch in stream]
        assert len(batches) == 1
        assert set(list(batches[0]["fid"])) == set([2, 3])
        assert set([f.GetFID() for f in lyr]) == set([2, 3])

    # Select LINESTRING and MULTILINESTRING due to point falling in bbox
    with ogrtest.spatial_filter(lyr, 5 - eps, 6 - eps, 5 + eps, 6 + eps):
        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        batches = [batch for batch in stream]
        assert len(batches) == 1
        assert set(list(batches[0]["fid"])) == set([2, 3])
        assert set([f.GetFID() for f in lyr]) == set([2, 3])

    # Select LINESTRING and MULTILINESTRING due to more generic intersection
    with ogrtest.spatial_filter(lyr, 4 - eps, 5 - eps, 4 + eps, 5 + eps):
        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        batches = [batch for batch in stream]
        assert len(batches) == 1
        assert set(list(batches[0]["fid"])) == set([2, 3])
        assert set([f.GetFID() for f in lyr]) == set([2, 3])

    # Select POLYGON and MULTIPOLYGON due to point falling in bbox
    with ogrtest.spatial_filter(lyr, 10 - eps, 20 - eps, 10 + eps, 20 + eps):
        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        batches = [batch for batch in stream]
        assert len(batches) == 1
        assert set(list(batches[0]["fid"])) == set([4, 5])
        assert set([f.GetFID() for f in lyr]) == set([4, 5])

    # bbox with polygon hole
    with ogrtest.spatial_filter(lyr, 12 - eps, 20.5 - eps, 12 + eps, 20.5 + eps):
        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        batches = [batch for batch in stream]
        if ogrtest.have_geos():
            assert len(batches) == 1
            assert list(batches[0]["fid"]) == []
        else:
            assert len(batches) == 1
            assert set(list(batches[0]["fid"])) == set([4, 5])
            assert set([f.GetFID() for f in lyr]) == set([4, 5])

    ds = None

    shutil.rmtree("tmp/test.tiledb")


###############################################################################


@pytest.mark.skipif(get_tiledb_version() < (2, 21, 0), reason="tiledb 2.21 required")
@pytest.mark.parametrize(
    "TILEDB_WKB_GEOMETRY_TYPE, OGR_TILEDB_WRITE_GEOMETRY_ATTRIBUTE_NAME",
    (("BLOB", True), ("GEOM_WKB", True), (None, False)),
)
def test_ogr_tiledb_tiledb_geometry_type(
    tmp_path, TILEDB_WKB_GEOMETRY_TYPE, OGR_TILEDB_WRITE_GEOMETRY_ATTRIBUTE_NAME
):
    with gdal.config_options(
        {
            "TILEDB_WKB_GEOMETRY_TYPE": TILEDB_WKB_GEOMETRY_TYPE,
            "OGR_TILEDB_WRITE_GEOMETRY_ATTRIBUTE_NAME": (
                "YES" if OGR_TILEDB_WRITE_GEOMETRY_ATTRIBUTE_NAME else "NO"
            ),
        }
    ):
        filename = str(tmp_path / "test.tiledb")
        with ogr.GetDriverByName("TileDB").CreateDataSource(filename) as ds:
            srs = osr.SpatialReference()
            srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
            srs.ImportFromEPSG(32631)
            lyr = ds.CreateLayer("test", srs=srs)
            f = ogr.Feature(lyr.GetLayerDefn())
            f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (1 2)"))
            lyr.CreateFeature(f)

        with ogr.Open(filename) as ds:
            lyr = ds.GetLayer(0)
            tiledb_md = json.loads(lyr.GetMetadata_List("json:TILEDB")[0])
            expected = {
                "name": "wkb_geometry",
                "type": (
                    TILEDB_WKB_GEOMETRY_TYPE if TILEDB_WKB_GEOMETRY_TYPE else "GEOM_WKB"
                ),
                "cell_val_num": "variable",
                "nullable": False,
                "filter_list": [],
            }
            assert expected in tiledb_md["schema"]["attributes"]
            f = lyr.GetNextFeature()
            assert f.GetGeometryRef().ExportToWkt() == "POINT (1 2)"
