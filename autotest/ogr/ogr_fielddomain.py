#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGRFieldDomain
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2021, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal, ogr


def test_ogr_fielddomain_range():

    with pytest.raises(Exception):
        ogr.CreateRangeFieldDomain(
            None, "desc", ogr.OFTInteger, ogr.OFSTNone, 1, True, 2, True
        )

    with gdal.quiet_errors():
        assert (
            ogr.CreateRangeFieldDomain(
                "name", "desc", ogr.OFTString, ogr.OFSTNone, 1, True, 2, True
            )
            is None
        )

    domain = ogr.CreateRangeFieldDomain(
        "name", "desc", ogr.OFTInteger, ogr.OFSTNone, 1, True, 2, True
    )
    assert domain.GetName() == "name"
    assert domain.GetDescription() == "desc"
    assert domain.GetDomainType() == ogr.OFDT_RANGE
    assert domain.GetFieldType() == ogr.OFTInteger
    assert domain.GetMinAsDouble() == 1.0
    assert domain.IsMinInclusive()
    assert domain.GetMaxAsDouble() == 2.0
    assert domain.IsMaxInclusive()
    assert domain.GetSplitPolicy() == ogr.OFDSP_DEFAULT_VALUE
    assert domain.GetMergePolicy() == ogr.OFDMP_DEFAULT_VALUE

    domain.SetSplitPolicy(ogr.OFDSP_DUPLICATE)
    assert domain.GetSplitPolicy() == ogr.OFDSP_DUPLICATE

    domain.SetMergePolicy(ogr.OFDMP_SUM)
    assert domain.GetMergePolicy() == ogr.OFDMP_SUM

    domain = ogr.CreateRangeFieldDomain(
        "name",
        None,
        ogr.OFTInteger64,
        ogr.OFSTNone,
        1234567890123,
        False,
        -1234567890123,
        False,
    )
    assert domain.GetName() == "name"
    assert domain.GetDescription() == ""
    assert domain.GetDomainType() == ogr.OFDT_RANGE
    assert domain.GetFieldType() == ogr.OFTInteger64
    assert domain.GetMinAsDouble() == 1234567890123.0
    assert not domain.IsMinInclusive()
    assert domain.GetMaxAsDouble() == -1234567890123.0
    assert not domain.IsMaxInclusive()

    domain = ogr.CreateRangeFieldDomainDateTime(
        "datetime_range",
        "datetime_range_desc",
        "2023-07-03T12:13:14",
        True,
        "2023-07-03T12:13:15",
        True,
    )
    assert domain.GetName() == "datetime_range"
    assert domain.GetDescription() == "datetime_range_desc"
    assert domain.GetDomainType() == ogr.OFDT_RANGE
    assert domain.GetFieldType() == ogr.OFTDateTime
    assert domain.GetFieldSubType() == ogr.OFSTNone
    assert domain.GetMinAsString() == "2023-07-03T12:13:14"
    assert domain.GetMaxAsString() == "2023-07-03T12:13:15"
    ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    ds.AddFieldDomain(domain)
    ret = gdal.VectorInfo(ds, format="json")
    assert ret["domains"] == {
        "datetime_range": {
            "description": "datetime_range_desc",
            "fieldType": "DateTime",
            "maxValue": "2023-07-03T12:13:15",
            "maxValueIncluded": True,
            "mergePolicy": "default value",
            "minValue": "2023-07-03T12:13:14",
            "minValueIncluded": True,
            "splitPolicy": "default value",
            "type": "range",
        }
    }
    ret = gdal.VectorInfo(ds, options="-fielddomain datetime_range")
    assert "Description: datetime_range_desc" in ret
    assert "Type: range" in ret
    assert "Field type: DateTime" in ret
    assert "Split policy: default value" in ret
    assert "Merge policy: default value" in ret
    assert "Minimum value: 2023-07-03T12:13:14" in ret
    assert "Maximum value: 2023-07-03T12:13:15" in ret

    with pytest.raises(Exception, match="should be called with a coded field domain"):
        domain.GetEnumeration()

    with pytest.raises(Exception, match="should be called with a glob field domain"):
        domain.GetGlob()


def test_ogr_fielddomain_coded():

    domain = ogr.CreateCodedFieldDomain(
        "name", "desc", ogr.OFTInteger, ogr.OFSTNone, {1: "one", "2": None}
    )
    assert domain.GetName() == "name"
    assert domain.GetDescription() == "desc"
    assert domain.GetDomainType() == ogr.OFDT_CODED
    assert domain.GetFieldType() == ogr.OFTInteger
    assert domain.GetEnumeration() == {"1": "one", "2": None}

    domain = ogr.CreateCodedFieldDomain("name", None, ogr.OFTInteger, ogr.OFSTNone, {})
    assert domain.GetEnumeration() == {}

    with pytest.raises(Exception):
        domain = ogr.CreateCodedFieldDomain(
            "name", "desc", ogr.OFTInteger, ogr.OFSTNone, None
        )

    with pytest.raises(Exception):
        domain = ogr.CreateCodedFieldDomain(
            "name", "desc", ogr.OFTInteger, ogr.OFSTNone, 5
        )

    with pytest.raises(Exception):
        domain = ogr.CreateCodedFieldDomain("name", "desc", ogr.OFTInteger, "x")


def test_ogr_fielddomain_glob():

    domain = ogr.CreateGlobFieldDomain("name", "desc", ogr.OFTString, ogr.OFSTNone, "*")
    assert domain.GetName() == "name"
    assert domain.GetDescription() == "desc"
    assert domain.GetDomainType() == ogr.OFDT_GLOB
    assert domain.GetFieldType() == ogr.OFTString
    assert domain.GetGlob() == "*"

    domain = ogr.CreateGlobFieldDomain("name", None, ogr.OFTString, ogr.OFSTNone, "*")
    assert domain.GetDescription() == ""

    with pytest.raises(Exception):
        domain = ogr.CreateGlobFieldDomain(
            "name", "desc", ogr.OFTString, ogr.OFSTNone, None
        )


def test_ogr_fielddomain_mem_driver():

    ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)

    assert ds.GetFieldDomainNames() is None

    assert ds.GetFieldDomain("foo") is None

    assert ds.AddFieldDomain(
        ogr.CreateGlobFieldDomain("name", "desc", ogr.OFTString, ogr.OFSTNone, "*")
    )

    assert ds.GetFieldDomainNames() == ["name"]

    assert ds.GetFieldDomain("name").GetDomainType() == ogr.OFDT_GLOB

    with pytest.raises(Exception, match="Received a NULL pointer"):
        ds.GetFieldDomain(None)

    with pytest.raises(Exception, match="Received a NULL pointer"):
        ds.AddFieldDomain(None)

    # Duplicate domain
    assert not ds.AddFieldDomain(
        ogr.CreateGlobFieldDomain("name", "desc", ogr.OFTString, ogr.OFSTNone, "*")
    )

    assert ds.GetFieldDomainNames() == ["name"]

    assert ds.AddFieldDomain(
        ogr.CreateGlobFieldDomain("name2", "desc", ogr.OFTString, ogr.OFSTNone, "*")
    )

    assert set(ds.GetFieldDomainNames()) == {"name", "name2"}

    # try deleting a domain which doesn't exist
    assert not ds.DeleteFieldDomain("xxx")

    assert ds.DeleteFieldDomain("name")

    assert ds.GetFieldDomain("name") is None
    assert ds.GetFieldDomainNames() == ["name2"]

    assert not ds.DeleteFieldDomain("name")

    assert ds.DeleteFieldDomain("name2")

    assert ds.GetFieldDomain("name2") is None
    assert ds.GetFieldDomainNames() is None


def test_ogr_fielddomain_get_set_domain_name():

    fld_defn = ogr.FieldDefn("foo", ogr.OFTInteger)
    fld_defn.SetDomainName("fooDomain")
    assert fld_defn.GetDomainName() == "fooDomain"


def test_delete_domain_assigned_to_field():
    ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    assert ds.AddFieldDomain(
        ogr.CreateGlobFieldDomain("name", "desc", ogr.OFTString, ogr.OFSTNone, "*")
    )
    assert ds.AddFieldDomain(
        ogr.CreateGlobFieldDomain("name2", "desc", ogr.OFTString, ogr.OFSTNone, "*")
    )

    lyr = ds.CreateLayer("ogr_mem_1")
    field_defn = ogr.FieldDefn("new_string", ogr.OFTString)
    field_defn.SetDomainName("name")
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn("new_string2", ogr.OFTString)
    field_defn.SetWidth(16)
    field_defn.SetDomainName("name2")
    lyr.CreateField(field_defn)

    lyr2 = ds.CreateLayer("ogr_mem_2")
    field_defn = ogr.FieldDefn("new_string3", ogr.OFTString)
    field_defn.SetDomainName("name")
    lyr2.CreateField(field_defn)

    assert lyr.GetLayerDefn().GetFieldDefn(0).GetDomainName() == "name"
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetDomainName() == "name2"
    assert lyr2.GetLayerDefn().GetFieldDefn(0).GetDomainName() == "name"

    sql_lyr = ds.ExecuteSQL("SELECT * FROM ogr_mem_1")
    assert sql_lyr.GetLayerDefn().GetFieldDefn(0).GetDomainName() == "name"
    ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL("SELECT new_string AS renamed FROM ogr_mem_1")
    assert sql_lyr.GetLayerDefn().GetFieldDefn(0).GetDomainName() == "name"
    ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL("SELECT COUNT(new_string) FROM ogr_mem_1")
    assert sql_lyr.GetLayerDefn().GetFieldDefn(0).GetDomainName() == ""
    ds.ReleaseResultSet(sql_lyr)

    if gdal.GetDriverByName("SQLITE") is not None:
        sql_lyr = ds.ExecuteSQL("SELECT * FROM ogr_mem_1", dialect="SQLITE")
        assert sql_lyr.GetLayerDefn().GetFieldDefn(0).GetDomainName() == "name"
        assert sql_lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString
        assert sql_lyr.GetLayerDefn().GetFieldDefn(1).GetDomainName() == "name2"
        assert sql_lyr.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTString
        ds.ReleaseResultSet(sql_lyr)

    # deleting domain should remove it from field definitions too
    assert ds.DeleteFieldDomain("name")

    assert not lyr.GetLayerDefn().GetFieldDefn(0).GetDomainName()
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetDomainName() == "name2"
    assert not lyr2.GetLayerDefn().GetFieldDefn(0).GetDomainName()

    assert ds.DeleteFieldDomain("name2")

    assert not lyr.GetLayerDefn().GetFieldDefn(0).GetDomainName()
    assert not lyr.GetLayerDefn().GetFieldDefn(1).GetDomainName()
    assert not lyr2.GetLayerDefn().GetFieldDefn(0).GetDomainName()


def test_update_field_domain():
    ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    assert ds.AddFieldDomain(
        ogr.CreateGlobFieldDomain("name", "desc", ogr.OFTString, ogr.OFSTNone, "*")
    )
    assert ds.AddFieldDomain(
        ogr.CreateGlobFieldDomain("name2", "desc2", ogr.OFTString, ogr.OFSTNone, "*a")
    )

    assert ds.GetFieldDomain("name").GetName() == "name"
    assert ds.GetFieldDomain("name").GetDescription() == "desc"
    assert ds.GetFieldDomain("name").GetGlob() == "*"

    assert ds.GetFieldDomain("name2").GetName() == "name2"
    assert ds.GetFieldDomain("name2").GetDescription() == "desc2"
    assert ds.GetFieldDomain("name2").GetGlob() == "*a"

    # try updating domain which doesn't exist
    no_matching_domain = ogr.CreateGlobFieldDomain(
        "nomatch", "desc", ogr.OFTString, ogr.OFSTNone, "*"
    )
    assert not ds.UpdateFieldDomain(no_matching_domain)

    new_domain1 = ogr.CreateGlobFieldDomain(
        "name", "different desc", ogr.OFTString, ogr.OFSTNone, "*b"
    )
    assert ds.UpdateFieldDomain(new_domain1)
    assert ds.GetFieldDomain("name").GetName() == "name"
    assert ds.GetFieldDomain("name").GetDescription() == "different desc"
    assert ds.GetFieldDomain("name").GetGlob() == "*b"

    assert ds.GetFieldDomain("name2").GetName() == "name2"
    assert ds.GetFieldDomain("name2").GetDescription() == "desc2"
    assert ds.GetFieldDomain("name2").GetGlob() == "*a"

    new_domain2 = ogr.CreateGlobFieldDomain(
        "name2", "different desc 2", ogr.OFTString, ogr.OFSTNone, "*c"
    )
    assert ds.UpdateFieldDomain(new_domain2)
    assert ds.GetFieldDomain("name").GetName() == "name"
    assert ds.GetFieldDomain("name").GetDescription() == "different desc"
    assert ds.GetFieldDomain("name").GetGlob() == "*b"

    assert ds.GetFieldDomain("name2").GetName() == "name2"
    assert ds.GetFieldDomain("name2").GetDescription() == "different desc 2"
    assert ds.GetFieldDomain("name2").GetGlob() == "*c"
