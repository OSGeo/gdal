#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test functionality for OGR JSONFG driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json

import gdaltest
import pytest

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_driver("JSONFG")


###############################################################################
# Test parsing valid formats of coordRefSys


def _get_epsg_crs(code, epoch=None):
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(code)
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    if epoch:
        srs.SetCoordinateEpoch(epoch)
    return srs


def _get_compound_crs(horiz_code, vert_code, epoch=None):
    horiz_srs = osr.SpatialReference()
    horiz_srs.ImportFromEPSG(horiz_code)
    vert_srs = osr.SpatialReference()
    vert_srs.ImportFromEPSG(vert_code)
    srs = osr.SpatialReference()
    srs.SetCompoundCS(
        horiz_srs.GetName() + " + " + vert_srs.GetName(), horiz_srs, vert_srs
    )
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    if epoch:
        srs.SetCoordinateEpoch(epoch)
    return srs


@pytest.mark.parametrize(
    "coordRefSys,expected_crs",
    [
        ("[EPSG:32631]", _get_epsg_crs(32631)),
        ("http://www.opengis.net/def/crs/EPSG/0/32631", _get_epsg_crs(32631)),
        ({"type": "Reference", "href": "[EPSG:32631]"}, _get_epsg_crs(32631)),
        (
            {
                "type": "Reference",
                "href": "http://www.opengis.net/def/crs/EPSG/0/32631",
            },
            _get_epsg_crs(32631),
        ),
        (
            {"type": "Reference", "href": "[EPSG:4326]", "epoch": 2023.4},
            _get_epsg_crs(4326, epoch=2023.4),
        ),
        (
            {
                "type": "Reference",
                "href": "http://www.opengis.net/def/crs/EPSG/0/4326",
                "epoch": 2023.4,
            },
            _get_epsg_crs(4326, epoch=2023.4),
        ),
        # Compound CRS
        (["[EPSG:4258]", "[EPSG:7837]"], _get_compound_crs(4258, 7837)),
        (
            ["http://www.opengis.net/def/crs/EPSG/0/4258", "[EPSG:7837]"],
            _get_compound_crs(4258, 7837),
        ),
        (
            ["[EPSG:4258]", "http://www.opengis.net/def/crs/EPSG/0/7837"],
            _get_compound_crs(4258, 7837),
        ),
        (
            [
                "http://www.opengis.net/def/crs/EPSG/0/4258",
                "http://www.opengis.net/def/crs/EPSG/0/7837",
            ],
            _get_compound_crs(4258, 7837),
        ),
        (
            [
                {"type": "Reference", "href": "[EPSG:4258]", "epoch": 2023.4},
                "http://www.opengis.net/def/crs/EPSG/0/7837",
            ],
            _get_compound_crs(4258, 7837, epoch=2023.4),
        ),
    ],
)
def test_jsonfg_read_coordRefSys_valid(coordRefSys, expected_crs):

    j = {
        "type": "FeatureCollection",
        "conformsTo": ["[ogc-json-fg-1-0.1:core]"],
        "coordRefSys": coordRefSys,
        "features": [{"type": "Feature", "properties": {}, "geometry": None}],
    }

    ds = gdal.OpenEx(json.dumps(j))
    assert ds.GetDriver().GetDescription() == "JSONFG"
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs
    assert srs.IsSame(expected_crs)


###############################################################################
# Test parsing invalid formats of coordRefSys


@pytest.mark.parametrize(
    "coordRefSys",
    [
        None,
        12,
        "",
        "[",
        "[]",
        "[EPSG]",
        "[EPSG:]",
        "[EPSG:0]",
        "http://www.opengis.net/def/crs/EPSG/0/0",
        {},
        {"type": None},
        {"type": 12},
        {"type": "invalid"},
        {"type": "Reference"},
        {"type": "Reference", "href": None},
        {"type": "Reference", "href": 12},
        {"type": "Reference", "href": "[EPSG:]"},
        {"type": "Reference", "href": "[EPSG:4326]", "epoch": "invalid"},
        [],
        ["[EPSG:4326]"],
        ["[EPSG:4326]", "invalid"],
        ["invalid", "[EPSG:4326]"],
    ],
)
def test_jsonfg_read_coordRefSys_invalid(coordRefSys):

    j = {
        "type": "FeatureCollection",
        "conformsTo": ["[ogc-json-fg-1-0.1:core]"],
        "coordRefSys": coordRefSys,
        "features": [{"type": "Feature", "properties": {}, "geometry": None}],
    }

    gdal.ErrorReset()
    ds = gdal.OpenEx(json.dumps(j))
    assert gdal.GetLastErrorMsg() != ""
    assert ds.GetDriver().GetDescription() == "JSONFG"
    lyr = ds.GetLayer(0)
    assert lyr.GetDataset().GetDescription() == ds.GetDescription()
    srs = lyr.GetSpatialRef()
    assert srs is None


###############################################################################
# Test handling of geometry vs place vs coordRefSys


@pytest.mark.parametrize(
    "filename,epsg_code_lyr,mapping_lyr,x,y,epsg_code_feat,mapping_feat",
    [
        (
            "data/jsonfg/crs_32631_fc_only.json",
            32631,
            [1, 2],
            500000,
            4500000,
            None,
            None,
        ),
        (
            "data/jsonfg/crs_32631_feat.json",
            32631,
            [1, 2],
            500000,
            4500000,
            None,
            None,
        ),
        (
            "data/jsonfg/crs_32631_fc_and_feat.json",
            32631,
            [1, 2],
            500000,
            4500000,
            None,
            None,
        ),
        (
            "data/jsonfg/crs_32631_fc_place_null.json",
            32631,
            [1, 2],
            500000,
            0,
            None,
            None,
        ),
        (
            "data/jsonfg/crs_32631_fc_geometry_null.json",
            32631,
            [1, 2],
            500000,
            4500000,
            None,
            None,
        ),
        (
            "data/jsonfg/crs_32631_fc_4326_feat.json",
            4326,
            [2, 1],
            3,
            0,
            None,
            None,
        ),
        (
            "data/jsonfg/crs_32631_feat_only.json",
            32631,
            [1, 2],
            500000,
            4500000,
            None,
            None,
        ),
        (
            "data/jsonfg/crs_32631_geom_only.json",
            32631,
            [1, 2],
            500000,
            4500000,
            None,
            None,
        ),
        (
            "data/jsonfg/crs_32631_fc_mixed_feat.json",
            32631,
            [1, 2],
            [500000, 0],
            [0, 0],
            None,
            None,
        ),
        (
            "data/jsonfg/crs_none_fc_mixed_feat.json",
            None,
            None,
            [3, 0],
            [0, 10000000],
            [4326, 32731],
            [[2, 1], [1, 2]],
        ),
        ("data/jsonfg/crs_4326_fc_only.json", 4326, [2, 1], 2, 49, None, None),
        ("data/jsonfg/crs_4326_fc_and_feat.json", 4326, [2, 1], 2, 49, None, None),
        ("data/jsonfg/crs_4326_fc_place_null.json", 4326, [2, 1], 2, 49, None, None),
        (
            "data/jsonfg/crs_4326_fc_32631_feat.json",
            32631,
            [1, 2],
            500000,
            0,
            None,
            None,
        ),
        ("data/jsonfg/crs_4326_feat_only.json", 4326, [2, 1], 2, 49, None, None),
        ("data/jsonfg/crs_none.json", 4326, [2, 1], 2, 49, None, None),
        (
            "data/jsonfg/crs_none_fc_mixed_feat_no_conformsTo.json",
            4326,
            [2, 1],
            2,
            49,
            None,
            None,
        ),
    ],
)
def test_jsonfg_read_crs(
    filename, epsg_code_lyr, mapping_lyr, x, y, epsg_code_feat, mapping_feat
):
    ds = gdal.OpenEx(filename)
    assert ds.GetDriver().GetDescription() == "JSONFG"
    lyr = ds.GetLayer(0)
    assert lyr.GetDataset().GetDescription() == ds.GetDescription()
    lyr_srs = lyr.GetSpatialRef()
    if epsg_code_lyr:
        assert lyr_srs
        assert lyr_srs.GetAuthorityCode(None) == str(epsg_code_lyr)
        assert lyr_srs.GetDataAxisToSRSAxisMapping() == mapping_lyr
    else:
        assert lyr_srs is None
    if isinstance(x, list):
        for i in range(len(x)):
            f = lyr.GetNextFeature()
            geom = f.GetGeometryRef()
            assert geom.GetX(0) == pytest.approx(x[i], abs=1e-8)
            assert geom.GetY(0) == pytest.approx(y[i], abs=1e-8)
            geom_srs = geom.GetSpatialReference()
            if epsg_code_lyr is not None and epsg_code_feat is None:
                assert geom_srs
                assert geom_srs.IsSame(lyr_srs)
            elif epsg_code_lyr is None and epsg_code_feat is not None:
                assert geom_srs.GetAuthorityCode(None) == str(epsg_code_feat[i])
                assert geom_srs.GetDataAxisToSRSAxisMapping() == mapping_feat[i]

    else:
        f = lyr.GetNextFeature()
        geom = f.GetGeometryRef()
        assert geom.GetX(0) == pytest.approx(x, abs=1e-8)
        assert geom.GetY(0) == pytest.approx(y, abs=1e-8)
        geom_srs = geom.GetSpatialReference()
        if epsg_code_lyr is not None and epsg_code_feat is None:
            assert geom_srs
            assert geom_srs.IsSame(lyr_srs)
        elif epsg_code_feat is not None:
            assert geom_srs
            assert geom_srs.GetAuthorityCode(None) == str(epsg_code_feat)
            assert geom_srs.GetDataAxisToSRSAxisMapping() == mapping_feat


###############################################################################
@pytest.mark.parametrize(
    "filename,open_options,epsg_code_lyr,mapping_lyr,x,y",
    [
        ("data/jsonfg/crs_32631_fc_only.json", [], 32631, [1, 2], 500000, 4500000),
        (
            "data/jsonfg/crs_32631_fc_only.json",
            ["GEOMETRY_ELEMENT=AUTO"],
            32631,
            [1, 2],
            500000,
            4500000,
        ),
        (
            "data/jsonfg/crs_32631_fc_only.json",
            ["GEOMETRY_ELEMENT=PLACE"],
            32631,
            [1, 2],
            500000,
            4500000,
        ),
        (
            "data/jsonfg/crs_32631_fc_only.json",
            ["GEOMETRY_ELEMENT=GEOMETRY"],
            4326,
            [2, 1],
            0,
            0,
        ),
        (
            "data/jsonfg/crs_32631_fc_place_null.json",
            ["GEOMETRY_ELEMENT=PLACE"],
            32631,
            [1, 2],
            None,
            None,
        ),
        (
            "data/jsonfg/crs_32631_fc_place_null.json",
            ["GEOMETRY_ELEMENT=GEOMETRY"],
            4326,
            [2, 1],
            3,
            0,
        ),
        (
            "data/jsonfg/crs_32631_fc_geometry_null.json",
            ["GEOMETRY_ELEMENT=PLACE"],
            32631,
            [1, 2],
            500000,
            4500000,
        ),
        (
            "data/jsonfg/crs_32631_fc_geometry_null.json",
            ["GEOMETRY_ELEMENT=GEOMETRY"],
            4326,
            [2, 1],
            None,
            None,
        ),
    ],
)
def test_jsonfg_read_GEOMETRY_ELEMENT_open_option(
    filename, open_options, epsg_code_lyr, mapping_lyr, x, y
):
    ds = gdal.OpenEx(filename, open_options=open_options)
    assert ds.GetDriver().GetDescription() == "JSONFG"
    lyr = ds.GetLayer(0)
    lyr_srs = lyr.GetSpatialRef()
    assert lyr_srs
    assert lyr_srs.GetAuthorityCode(None) == str(epsg_code_lyr)
    assert lyr_srs.GetDataAxisToSRSAxisMapping() == mapping_lyr
    f = lyr.GetNextFeature()
    geom = f.GetGeometryRef()
    if x is not None:
        assert geom.GetX(0) == pytest.approx(x, abs=1e-8)
        assert geom.GetY(0) == pytest.approx(y, abs=1e-8)
    else:
        assert geom is None


###############################################################################
# Test reading file with featureType at FeatureCollection level


def test_jsonfg_read_feature_type_top_level():

    ds = gdal.OpenEx("data/jsonfg/feature_type_top_level.json")
    assert ds.GetDriver().GetDescription() == "JSONFG"
    lyr = ds.GetLayerByName("type1")
    assert lyr


###############################################################################
# Test reading file with several feature types


def test_jsonfg_read_two_features_types():

    ds = gdal.OpenEx("data/jsonfg/two_feature_types.json")
    assert ds.GetDriver().GetDescription() == "JSONFG"
    assert ds.GetLayerCount() == 2
    lyr = ds.GetLayerByName("type1")
    assert lyr.GetFeatureCount() == 2
    assert lyr.GetGeomType() == ogr.wkbPoint
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    f = lyr.GetNextFeature()
    assert f.GetField("foo") == "1"
    assert f.GetFID() == 1
    f = lyr.GetNextFeature()
    assert f.GetField("foo") == "bar"
    assert f.GetFID() == 2

    lyr = ds.GetLayerByName("type2")
    assert lyr.GetFeatureCount() == 1
    assert lyr.GetGeomType() == ogr.wkbLineString25D
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    f = lyr.GetNextFeature()
    assert f.GetField("bar") == "baz"
    assert f.GetFID() == 1


###############################################################################
# Test reading file with a single Feature larger than 6000 bytes


def test_jsonfg_read_single_feature_large(tmp_vsimem):

    tmp_file = str(tmp_vsimem / "test.json")
    content = """{
      "type": "Feature",
      "conformsTo" : [ "[ogc-json-fg-1-0.1:core]" ],
      %s
      "id": 1,
      "geometry": { "type": "Point", "coordinates": [2, 49] },
      "properties": { "foo": 1 },
      "place": null,
      "time": null
    }""" % (
        " " * 100000
    )

    gdal.FileFromMemBuffer(tmp_file, content)

    ds = gdal.OpenEx(tmp_file)
    assert ds.GetDriver().GetDescription() == "JSONFG"


###############################################################################
# Test time handling


@pytest.mark.parametrize(
    "time_values,expected_fields,expected_values_array",
    [
        ([None], [], []),
        ([{"date": "2023-06-05"}], [("time", ogr.OFTDate)], [{"time": "2023/06/05"}]),
        (
            [{"timestamp": "2023-06-05T12:34:56Z"}],
            [("time", ogr.OFTDateTime)],
            [{"time": "2023/06/05 12:34:56+00"}],
        ),
        # Interval of Date
        (
            [{"interval": ["2023-06-05", "2023-06-06"]}],
            [("time_start", ogr.OFTDate), ("time_end", ogr.OFTDate)],
            [{"time_start": "2023/06/05", "time_end": "2023/06/06"}],
        ),
        (
            [{"interval": ["2023-06-05", ".."]}],
            [("time_start", ogr.OFTDate), ("time_end", ogr.OFTDate)],
            [{"time_start": "2023/06/05", "time_end": None}],
        ),
        (
            [{"interval": ["..", "2023-06-06"]}],
            [("time_start", ogr.OFTDate), ("time_end", ogr.OFTDate)],
            [{"time_start": None, "time_end": "2023/06/06"}],
        ),
        # Interval of DateTime
        (
            [{"interval": ["2023-06-05T12:34:56Z", "2023-06-06T12:34:56Z"]}],
            [("time_start", ogr.OFTDateTime), ("time_end", ogr.OFTDateTime)],
            [
                {
                    "time_start": "2023/06/05 12:34:56+00",
                    "time_end": "2023/06/06 12:34:56+00",
                }
            ],
        ),
        (
            [{"interval": ["2023-06-05T12:34:56Z", ".."]}],
            [("time_start", ogr.OFTDateTime), ("time_end", ogr.OFTDateTime)],
            [{"time_start": "2023/06/05 12:34:56+00", "time_end": None}],
        ),
        (
            [{"interval": ["..", "2023-06-06T12:34:56Z"]}],
            [("time_start", ogr.OFTDateTime), ("time_end", ogr.OFTDateTime)],
            [{"time_start": None, "time_end": "2023/06/06 12:34:56+00"}],
        ),
        # Mix of Date and DateTime
        (
            [{"date": "2023-06-05"}, {"timestamp": "2023-06-05T12:34:56Z"}],
            [("time", ogr.OFTDateTime)],
            [{"time": "2023/06/05 00:00:00"}, {"time": "2023/06/05 12:34:56+00"}],
        ),
        (
            [
                {"interval": ["2023-06-05", "2023-06-06"]},
                {"interval": ["2023-06-05T12:34:56Z", "2023-06-06T12:34:56Z"]},
            ],
            [("time_start", ogr.OFTDateTime), ("time_end", ogr.OFTDateTime)],
            [
                {
                    "time_start": "2023/06/05 00:00:00",
                    "time_end": "2023/06/06 00:00:00",
                },
                {
                    "time_start": "2023/06/05 12:34:56+00",
                    "time_end": "2023/06/06 12:34:56+00",
                },
            ],
        ),
    ],
)
def test_jsonfg_read_time(time_values, expected_fields, expected_values_array):

    j = {
        "type": "FeatureCollection",
        "conformsTo": ["[ogc-json-fg-1-0.1:core]"],
        "features": [],
    }
    for time in time_values:
        j["features"].append(
            {"type": "Feature", "properties": {}, "geometry": None, "time": time}
        )

    ds = gdal.OpenEx(json.dumps(j))
    assert ds.GetDriver().GetDescription() == "JSONFG"
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    for (field_name, field_type) in expected_fields:
        idx = lyr_defn.GetFieldIndex(field_name)
        assert idx >= 0, field_name
        assert lyr_defn.GetFieldDefn(idx).GetType() == field_type
    for expected_values in expected_values_array:
        feat = lyr.GetNextFeature()
        for key in expected_values:
            if expected_values[key]:
                assert feat.GetFieldAsString(key) == expected_values[key]
            else:
                assert not feat.IsFieldSet(key)


###############################################################################
# Test time handling


def test_jsonfg_read_time_with_time_property():

    j = {
        "type": "FeatureCollection",
        "conformsTo": ["[ogc-json-fg-1-0.1:core]"],
        "features": [
            {
                "type": "Feature",
                "properties": {"time": "my_time"},
                "geometry": None,
                "time": {"date": "2023-06-05"},
            }
        ],
    }

    ds = gdal.OpenEx(json.dumps(j))
    assert ds.GetDriver().GetDescription() == "JSONFG"
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f["time"] == "my_time"
    assert f.GetFieldAsString("jsonfg_time") == "2023/06/05"


###############################################################################
# Test reading Prism with point base


def test_jsonfg_read_prism_with_point_base():

    ds = gdal.OpenEx("data/jsonfg/pylon.json")
    assert ds.GetDriver().GetDescription() == "JSONFG"
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert (
        f.GetGeometryRef().ExportToIsoWkt()
        == "LINESTRING Z (81220.15 455113.71 2.02,81220.15 455113.71 8.02)"
    )


###############################################################################
# Test reading Prism with line string base


def test_jsonfg_read_prism_with_line_base():

    ds = gdal.OpenEx("data/jsonfg/fence.json")
    assert ds.GetDriver().GetDescription() == "JSONFG"
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert (
        f.GetGeometryRef().ExportToIsoWkt()
        == "MULTIPOLYGON Z (((81220.15 455113.71 2.02,81223.15 455116.71 2.02,81223.15 455116.71 3.22,81220.15 455113.71 3.22,81220.15 455113.71 2.02)))"
    )


###############################################################################
# Test reading Prism with polygon base


def test_jsonfg_read_prism_with_polygon_base():

    ds = gdal.OpenEx("data/jsonfg/prism_with_polygon_base.json")
    assert ds.GetDriver().GetDescription() == "JSONFG"
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert (
        f.GetGeometryRef().ExportToIsoWkt()
        == "POLYHEDRALSURFACE Z (((0 0 10,1 0 10,0 2 10,0 0 10)),((0 0 10,1 0 10,1 0 20,0 0 20,0 0 10)),((1 0 10,0 2 10,0 2 20,1 0 20,1 0 10)),((0 2 10,0 0 10,0 0 20,0 2 20,0 2 10)),((0 0 20,1 0 20,0 2 20,0 0 20)))"
    )


###############################################################################
# Test writing coordRefSys, geometry, place


@pytest.mark.parametrize(
    "crs,expected_coordRefSys,input_x,input_y,geom_x,geom_y,place_x,place_y",
    [
        (_get_epsg_crs(32631), "[EPSG:32631]", 500000, 0, 3, 0, 500000, 0),
        (_get_epsg_crs(4326), "[EPSG:4326]", 2, 49, 2, 49, None, None),
        (_get_epsg_crs(4258), "[EPSG:4258]", 2, 49, 2, 49, 49, 2),
        (
            _get_epsg_crs(4326, epoch=2023.4),
            {"type": "Reference", "href": "[EPSG:4326]", "epoch": 2023.4},
            2,
            49,
            2,
            49,
            None,
            None,
        ),
        # Compound CRS
        (
            _get_compound_crs(4258, 7837),
            ["[EPSG:4258]", "[EPSG:7837]"],
            2,
            49,
            2,
            49,
            49,
            2,
        ),
        (
            _get_compound_crs(4258, 7837, epoch=2023.4),
            [
                {"type": "Reference", "href": "[EPSG:4258]", "epoch": 2023.4},
                "[EPSG:7837]",
            ],
            2,
            49,
            2,
            49,
            49,
            2,
        ),
    ],
)
def test_jsonfg_write_coordRefSys_geometry_place(
    crs, expected_coordRefSys, input_x, input_y, geom_x, geom_y, place_x, place_y
):

    filename = "/vsimem/test_jsonfg_write_coordRefSys_geometry_place.json"
    try:
        ds = ogr.GetDriverByName("JSONFG").CreateDataSource(
            filename, options=["SINGLE_LAYER=YES"]
        )
        lyr = ds.CreateLayer("test", srs=crs, geom_type=ogr.wkbUnknown)
        assert lyr.GetDataset().GetDescription() == ds.GetDescription()
        f = ogr.Feature(lyr.GetLayerDefn())
        g = ogr.Geometry(ogr.wkbPoint)
        g.AddPoint_2D(input_x, input_y)
        f.SetGeometry(g)
        lyr.CreateFeature(f)
        f = None
        ds = None

        f = gdal.VSIFOpenL(filename, "rb")
        if f:
            data = gdal.VSIFReadL(1, 10000, f)
            gdal.VSIFCloseL(f)

        j = json.loads(data)
        assert j["coordRefSys"] == expected_coordRefSys
        feat = j["features"][0]
        if geom_x:
            assert feat["geometry"]["coordinates"][0] == pytest.approx(geom_x)
            assert feat["geometry"]["coordinates"][1] == pytest.approx(geom_y)
        else:
            assert feat["geometry"] is None
        if place_x:
            assert feat["place"]["coordinates"][0] == pytest.approx(place_x)
            assert feat["place"]["coordinates"][1] == pytest.approx(place_y)
        else:
            assert feat["place"] is None

        ds = ogr.Open(filename)
        lyr = ds.GetLayer(0)
        assert lyr.GetSpatialRef().IsSame(crs)
        ds = None

    finally:
        if gdal.VSIStatL(filename):
            gdal.Unlink(filename)


###############################################################################
# Test IAU: CRS


@pytest.mark.require_proj(8, 2)
def test_jsonfg_write_coordRefSys_IAU():
    srs = osr.SpatialReference()
    srs.SetFromUserInput("IAU:49910")
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    test_jsonfg_write_coordRefSys_geometry_place(
        srs, "[IAU:49910]", 2, 49, None, None, 2, 49
    )


###############################################################################


def test_jsonfg_write_basic():

    filename = "/vsimem/test_jsonfg_write_basic.json"
    try:
        ds = ogr.GetDriverByName("JSONFG").CreateDataSource(filename)
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(32631)
        lyr = ds.CreateLayer("test", srs=srs, geom_type=ogr.wkbUnknown)
        lyr.CreateField(ogr.FieldDefn("strfield", ogr.OFTString))
        lyr.CreateField(ogr.FieldDefn("intfield", ogr.OFTInteger))
        lyr.CreateField(ogr.FieldDefn("int64field", ogr.OFTInteger64))
        lyr.CreateField(ogr.FieldDefn("doublefield", ogr.OFTReal))
        lyr.CreateField(ogr.FieldDefn("strlistfield", ogr.OFTStringList))
        lyr.CreateField(ogr.FieldDefn("intlistfield", ogr.OFTIntegerList))
        lyr.CreateField(ogr.FieldDefn("int64listfield", ogr.OFTInteger64List))
        lyr.CreateField(ogr.FieldDefn("doublelistfield", ogr.OFTRealList))
        boolfield = ogr.FieldDefn("boolfield", ogr.OFTInteger)
        boolfield.SetSubType(ogr.OFSTBoolean)
        lyr.CreateField(boolfield)
        jsonfield = ogr.FieldDefn("jsonfield", ogr.OFTString)
        jsonfield.SetSubType(ogr.OFSTJSON)
        lyr.CreateField(jsonfield)
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(10)
        f["strfield"] = "foo"
        f["intfield"] = 123456789
        f["int64field"] = 12345678912345678
        f["doublefield"] = 1.5
        f["strlistfield"] = ["foo", "bar"]
        f["intlistfield"] = [123456789]
        f["int64listfield"] = [12345678912345678]
        f["doublelistfield"] = [1.5]
        f["boolfield"] = True
        f["jsonfield"] = json.dumps({"a": [1, 2]})
        f.SetGeometry(
            ogr.CreateGeometryFromWkt("POINT(1.23456789 2.23456789 3.23456789)")
        )
        lyr.CreateFeature(f)
        f = None
        ds = None

        f = gdal.VSIFOpenL(filename, "rb")
        if f:
            data = gdal.VSIFReadL(1, 10000, f)
            gdal.VSIFCloseL(f)

        j = json.loads(data)
        assert j["features"][0]["id"] == 10
        assert j["features"][0]["properties"] == {
            "strfield": "foo",
            "intfield": 123456789,
            "int64field": 12345678912345678,
            "doublefield": 1.5,
            "strlistfield": ["foo", "bar"],
            "intlistfield": [123456789],
            "int64listfield": [12345678912345678],
            "doublelistfield": [1.5],
            "boolfield": True,
            "jsonfield": {"a": [1, 2]},
        }
        assert "xy_coordinate_resolution" not in j
        assert "z_coordinate_resolution" not in j
        assert "xy_coordinate_resolution_place" not in j
        assert "z_coordinate_resolution_place" not in j
        assert j["features"][0]["geometry"]["coordinates"][2] == 3.235

        ds = ogr.Open(filename)
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f["strfield"] == "foo"
        assert f["intfield"] == 123456789
        assert f["int64field"] == 12345678912345678
        assert f["doublefield"] == 1.5
        assert f["strlistfield"] == ["foo", "bar"]
        assert f["intlistfield"] == [123456789]
        assert f["int64listfield"] == [12345678912345678]
        assert f["doublelistfield"] == [1.5]
        assert f["boolfield"] == True
        assert json.loads(f["jsonfield"]) == {"a": [1, 2]}
        assert (
            f.GetGeometryRef().ExportToIsoWkt()
            == "POINT Z (1.23456789 2.23456789 3.23456789)"
        )
        ds = None

    finally:
        if gdal.VSIStatL(filename):
            gdal.Unlink(filename)


###############################################################################


def test_jsonfg_write_several_layers():

    filename = "/vsimem/test_jsonfg_write_several_layers.json"
    try:
        ds = ogr.GetDriverByName("JSONFG").CreateDataSource(filename)
        srs1 = osr.SpatialReference()
        srs1.ImportFromEPSG(32631)
        lyr = ds.CreateLayer("test1", srs=srs1, geom_type=ogr.wkbUnknown)
        f = ogr.Feature(lyr.GetLayerDefn())
        g = ogr.Geometry(ogr.wkbPoint)
        g.AddPoint_2D(1, 2)
        f.SetGeometry(g)
        lyr.CreateFeature(f)
        f = None
        srs2 = osr.SpatialReference()
        srs2.ImportFromEPSG(32632)
        lyr = ds.CreateLayer("test2", srs=srs2, geom_type=ogr.wkbUnknown)
        f = ogr.Feature(lyr.GetLayerDefn())
        g = ogr.Geometry(ogr.wkbPoint)
        g.AddPoint_2D(3, 4)
        f.SetGeometry(g)
        lyr.CreateFeature(f)
        f = None
        ds = None

        f = gdal.VSIFOpenL(filename, "rb")
        if f:
            data = gdal.VSIFReadL(1, 10000, f)
            gdal.VSIFCloseL(f)

        j = json.loads(data)
        assert "coordRefSys" not in j

        ds = ogr.Open(filename)
        assert ds.GetLayerCount() == 2
        lyr = ds.GetLayerByName("test1")
        assert lyr.GetSpatialRef().IsSame(srs1)
        lyr = ds.GetLayerByName("test2")
        assert lyr.GetSpatialRef().IsSame(srs2)
        ds = None

    finally:
        if gdal.VSIStatL(filename):
            gdal.Unlink(filename)


###############################################################################


@pytest.mark.parametrize(
    "wkts,expect_geom_type",
    [
        [["POINT (1 2)"], ogr.wkbPoint],
        [["POINT Z (1 2 3)"], ogr.wkbPoint25D],
        [["LINESTRING (1 2,3 4)"], ogr.wkbLineString],
        [["LINESTRING Z (1 2 3,4 5 6)"], ogr.wkbLineString25D],
        [["POLYGON ((0 0,0 1,1 1,0 0))"], ogr.wkbPolygon],
        [["POLYGON Z ((0 0 10,0 1 10,1 1 10,0 0 10))"], ogr.wkbPolygon25D],
        [["MULTIPOINT ((1 2))"], ogr.wkbMultiPoint],
        [["MULTIPOINT Z ((1 2 3))"], ogr.wkbMultiPoint25D],
        [["MULTILINESTRING ((1 2,3 4))"], ogr.wkbMultiLineString],
        [["MULTILINESTRING Z ((1 2 3,4 5 6))"], ogr.wkbMultiLineString25D],
        [["MULTIPOLYGON (((0 0,0 1,1 1,0 0)))"], ogr.wkbMultiPolygon],
        [["MULTIPOLYGON Z (((0 0 10,0 1 10,1 1 10,0 0 10)))"], ogr.wkbMultiPolygon25D],
        [["GEOMETRYCOLLECTION (POINT (1 2))"], ogr.wkbGeometryCollection],
        [["GEOMETRYCOLLECTION Z (POINT Z (1 2 3))"], ogr.wkbGeometryCollection25D],
        [["POINT (1 2)", "LINESTRING (1 2,3 4)"], ogr.wkbUnknown],
        [["POLYHEDRALSURFACE Z EMPTY"], ogr.wkbPolyhedralSurfaceZ],
        [
            [
                "POLYHEDRALSURFACE Z (((0 0 0,0 1 0,1 1 0,0 0 0)),((0 0 0,1 0 0,0 0 1,0 0 0)),((0 0 0,0 1 0,0 0 1,0 0 0)),((0 1 0,1 0 0,0 0 1,0 1 0)))"
            ],
            ogr.wkbPolyhedralSurfaceZ,
        ],
    ],
)
def test_jsonfg_write_all_geom_types(wkts, expect_geom_type):

    filename = "/vsimem/test_jsonfg_read_write_all_geom_types.json"
    try:
        ds = ogr.GetDriverByName("JSONFG").CreateDataSource(filename)
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(32631)
        lyr = ds.CreateLayer("test", srs=srs, geom_type=ogr.wkbUnknown)
        for wkt in wkts:
            f = ogr.Feature(lyr.GetLayerDefn())
            f.SetGeometry(ogr.CreateGeometryFromWkt(wkt))
            lyr.CreateFeature(f)
        ds = None

        ds = ogr.Open(filename)
        lyr = ds.GetLayer(0)
        assert lyr.GetGeomType() == expect_geom_type
        for wkt in wkts:
            f = lyr.GetNextFeature()
            assert f.GetGeometryRef().ExportToIsoWkt() == wkt
        ds = None

    finally:
        if gdal.VSIStatL(filename):
            gdal.Unlink(filename)


###############################################################################
# Test time handling


@pytest.mark.parametrize(
    "input_field_defns,input_field_values_array,expected_times,expected_properties",
    [
        [
            [ogr.FieldDefn("time", ogr.OFTDate)],
            [{"time": "2023-06-06"}, {}],
            [{"date": "2023-06-06"}, None],
            [{}, {}],
        ],
        [
            [ogr.FieldDefn("time", ogr.OFTDateTime)],
            [
                {"time": "2023-06-06T12:34:56Z"},
                {"time": "2023-06-06T12:34:56.789Z"},
                {"time": "2023-06-06T12:34:56+0200"},
                {},
            ],
            [
                {"timestamp": "2023-06-06T12:34:56Z"},
                {"timestamp": "2023-06-06T12:34:56.789Z"},
                {"timestamp": "2023-06-06T10:34:56Z"},
                None,
            ],
            [{}, {}, {}, {}],
        ],
        [
            [ogr.FieldDefn("jsonfg_time", ogr.OFTDate)],
            [{"jsonfg_time": "2023-06-06"}],
            [{"date": "2023-06-06"}],
            [{}],
        ],
        [
            [
                ogr.FieldDefn("time", ogr.OFTString),
                ogr.FieldDefn("jsonfg_time", ogr.OFTDate),
            ],
            [{"time": "my_time", "jsonfg_time": "2023-06-06"}],
            [{"date": "2023-06-06"}],
            [{"time": "my_time"}],
        ],
        [
            [
                ogr.FieldDefn("time_start", ogr.OFTDate),
                ogr.FieldDefn("time_end", ogr.OFTDate),
            ],
            [
                {"time_start": "2023-06-06", "time_end": "2023-06-07"},
                {"time_start": "2023-06-06"},
                {"time_end": "2023-06-07"},
                {},
            ],
            [
                {"interval": ["2023-06-06", "2023-06-07"]},
                {"interval": ["2023-06-06", ".."]},
                {"interval": ["..", "2023-06-07"]},
                None,
            ],
            [{}, {}, {}, {}],
        ],
        [
            [
                ogr.FieldDefn("time_start", ogr.OFTDateTime),
                ogr.FieldDefn("time_end", ogr.OFTDateTime),
            ],
            [
                {
                    "time_start": "2023-06-06T12:34:56Z",
                    "time_end": "2023-06-07T12:34:56Z",
                }
            ],
            [{"interval": ["2023-06-06T12:34:56Z", "2023-06-07T12:34:56Z"]}],
            [{}],
        ],
        [
            [
                ogr.FieldDefn("jsonfg_time_start", ogr.OFTDate),
                ogr.FieldDefn("jsonfg_time_end", ogr.OFTDate),
            ],
            [{"jsonfg_time_start": "2023-06-06", "jsonfg_time_end": "2023-06-07"}],
            [{"interval": ["2023-06-06", "2023-06-07"]}],
            [{}],
        ],
        [
            [ogr.FieldDefn("time_start", ogr.OFTDate)],
            [{"time_start": "2023-06-06"}],
            [{"interval": ["2023-06-06", ".."]}],
            [{}],
        ],
        [
            [ogr.FieldDefn("time_end", ogr.OFTDate)],
            [{"time_end": "2023-06-06"}],
            [{"interval": ["..", "2023-06-06"]}],
            [{}],
        ],
    ],
)
def test_jsonfg_write_time(
    input_field_defns, input_field_values_array, expected_times, expected_properties
):

    filename = "/vsimem/test_jsonfg_write_time.json"
    try:
        ds = ogr.GetDriverByName("JSONFG").CreateDataSource(filename)
        lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)
        for input_field_defn in input_field_defns:
            lyr.CreateField(input_field_defn)
        for input_field_values in input_field_values_array:
            f = ogr.Feature(lyr.GetLayerDefn())
            for key in input_field_values:
                f.SetField(key, input_field_values[key])
            lyr.CreateFeature(f)
        ds = None

        f = gdal.VSIFOpenL(filename, "rb")
        if f:
            data = gdal.VSIFReadL(1, 10000, f)
            gdal.VSIFCloseL(f)

        j = json.loads(data)
        features = j["features"]
        for feat_idx in range(len(features)):
            feat = features[feat_idx]
            assert feat["time"] == expected_times[feat_idx]
            assert feat["properties"] == expected_properties[feat_idx]

    finally:
        if gdal.VSIStatL(filename):
            gdal.Unlink(filename)


###############################################################################
# Test gdal.VectorTranslate()


def test_jsonfg_vector_translate():

    filename = "tmp/out.json"
    try:
        ds = gdal.VectorTranslate(filename, "data/poly.shp", format="JSONFG")
        assert ds
        ds = None

        import test_cli_utilities

        if test_cli_utilities.get_test_ogrsf_path() is not None:
            ret = gdaltest.runexternal(
                test_cli_utilities.get_test_ogrsf_path() + " -ro " + filename
            )

            assert "INFO" in ret
            assert "ERROR" not in ret

    finally:
        if gdal.VSIStatL(filename):
            gdal.Unlink(filename)


###############################################################################
# Test WRITE_GEOMETRY=NO layer creation option


def test_jsonfg_write_WRITE_GEOMETRY_NO():

    filename = "/vsimem/test_jsonfg_write_WRITE_GEOMETRY_NO.json"
    try:
        ds = ogr.GetDriverByName("JSONFG").CreateDataSource(filename)
        lyr = ds.CreateLayer(
            "test", srs=_get_epsg_crs(32631), options=["WRITE_GEOMETRY=NO"]
        )
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (500000 4500000)"))
        lyr.CreateFeature(f)
        ds = None

        f = gdal.VSIFOpenL(filename, "rb")
        if f:
            data = gdal.VSIFReadL(1, 10000, f)
            gdal.VSIFCloseL(f)

        j = json.loads(data)
        feature = j["features"][0]
        assert feature["geometry"] is None
        assert feature["place"] == {
            "coordinates": [500000.0, 4500000.0],
            "type": "Point",
        }

    finally:
        if gdal.VSIStatL(filename):
            gdal.Unlink(filename)


###############################################################################
# Test COORDINATE_PRECISION_* layer creation option


def test_jsonfg_write_COORDINATE_PRECISION():

    filename = "/vsimem/test_jsonfg_write_COORDINATE_PRECISION.json"
    try:
        ds = ogr.GetDriverByName("JSONFG").CreateDataSource(
            filename, options=["SINGLE_LAYER=YES"]
        )
        lyr = ds.CreateLayer(
            "test",
            srs=_get_epsg_crs(32631),
            options=["COORDINATE_PRECISION_GEOMETRY=3", "COORDINATE_PRECISION_PLACE=2"],
        )
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (500000.12345 4500000.12345)"))
        lyr.CreateFeature(f)
        ds = None

        f = gdal.VSIFOpenL(filename, "rb")
        if f:
            data = gdal.VSIFReadL(1, 10000, f)
            gdal.VSIFCloseL(f)

        j = json.loads(data)
        assert j["xy_coordinate_resolution"] == 1e-3
        assert j["xy_coordinate_resolution_place"] == 1e-2
        feature = j["features"][0]
        assert feature["geometry"]["coordinates"] == pytest.approx(
            [3.0, 40.651], abs=1e-3
        )
        assert feature["place"]["coordinates"] == pytest.approx(
            [500000.12, 4500000.12], abs=1e-2
        )

    finally:
        if gdal.VSIStatL(filename):
            gdal.Unlink(filename)


###############################################################################
# Test FlushCache()


def test_jsonfg_write_flushcache():

    filename = "/vsimem/test_jsonfg_write_flushcache.json"
    try:
        ds = gdal.GetDriverByName("JSONFG").Create(filename, 0, 0, 0, gdal.GDT_Unknown)
        lyr1 = ds.CreateLayer("test1", srs=_get_epsg_crs(32631))
        lyr2 = ds.CreateLayer("test2", srs=_get_epsg_crs(32631))
        f = ogr.Feature(lyr1.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
        lyr1.CreateFeature(f)
        f = ogr.Feature(lyr2.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 4)"))
        lyr2.CreateFeature(f)
        ds.FlushCache()

        ds2 = ogr.Open(filename)
        assert ds2.GetLayer(0).GetFeatureCount() == 1
        assert ds2.GetLayer(1).GetFeatureCount() == 1
        ds2 = None

        f = ogr.Feature(lyr1.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 4)"))
        lyr1.CreateFeature(f)
        ds = None

        ds2 = ogr.Open(filename)
        assert ds2.GetLayer(0).GetFeatureCount() == 2
        assert ds2.GetLayer(1).GetFeatureCount() == 1
        ds2 = None

    finally:
        if gdal.VSIStatL(filename):
            gdal.Unlink(filename)


###############################################################################
# Test geometry coordinate precision support


@pytest.mark.parametrize("single_layer", [True, False])
def test_ogr_jsonfg_geom_coord_precision(tmp_vsimem, single_layer):

    filename = str(tmp_vsimem / "test.json")
    ds = gdal.GetDriverByName("JSONFG").Create(
        filename,
        0,
        0,
        0,
        gdal.GDT_Unknown,
        options=["SINGLE_LAYER=" + ("YES" if single_layer else "NO")],
    )
    geom_fld = ogr.GeomFieldDefn("geometry", ogr.wkbUnknown)
    prec = ogr.CreateGeomCoordinatePrecision()
    prec.Set(1e-2, 1e-3, 0)
    geom_fld.SetCoordinatePrecision(prec)
    srs = osr.SpatialReference()
    srs.SetFromUserInput("EPSG:32631+3855")
    geom_fld.SetSpatialRef(srs)
    lyr = ds.CreateLayerFromGeomFieldDefn("test", geom_fld)
    geom_fld = lyr.GetLayerDefn().GetGeomFieldDefn(0)
    prec = geom_fld.GetCoordinatePrecision()
    assert prec.GetXYResolution() == 1e-2
    assert prec.GetZResolution() == 1e-3
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt("POINT(4500000.23456789 5000000.34567891 9.87654321)")
    )
    lyr.CreateFeature(f)
    ds.Close()

    f = gdal.VSIFOpenL(filename, "rb")
    if f:
        data = gdal.VSIFReadL(1, 10000, f)
        gdal.VSIFCloseL(f)

    assert b"[ 46.44289405, 36.08688377, 9.877 ]" in data
    assert b"[ 4500000.23, 5000000.35, 9.877 ]" in data

    j = json.loads(data)
    assert j["xy_coordinate_resolution"] == pytest.approx(8.98315e-08)
    assert j["z_coordinate_resolution"] == 1e-3
    assert j["xy_coordinate_resolution_place"] == 1e-2
    assert j["z_coordinate_resolution_place"] == 1e-3

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    geom_fld = lyr.GetLayerDefn().GetGeomFieldDefn(0)
    prec = geom_fld.GetCoordinatePrecision()
    assert prec.GetXYResolution() == 1e-2
    assert prec.GetZResolution() == 1e-3


###############################################################################
# Test force opening a GeoJSON file with JSONFG


def test_ogr_jsonfg_force_opening():

    if ogr.GetDriverByName("GeoJSON"):
        ds = gdal.OpenEx("data/geojson/featuretype.json")
        assert ds.GetDriver().GetDescription() == "GeoJSON"

    ds = gdal.OpenEx("data/geojson/featuretype.json", allowed_drivers=["JSONFG"])
    assert ds.GetDriver().GetDescription() == "JSONFG"


###############################################################################
# Test force opening a URL as JSONFG


def test_ogr_jsonfg_force_opening_url():

    drv = gdal.IdentifyDriverEx("http://example.com", allowed_drivers=["JSONFG"])
    assert drv.GetDescription() == "JSONFG"
