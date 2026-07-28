#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR S-101 driver functionality.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_driver("S101")

from .ogr_s101 import _alter_xml

###############################################################################
# Test file with 2D point


def test_ogr_s101_read_point_2d_minimum():

    with gdaltest.error_raised(gdal.CE_None):
        ds = ogr.Open("data/s101/point_2d_minimum.000")

    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayerByName("Point2D")
    assert lyr.GetSpatialRef().GetAuthorityCode() == "4326"
    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {"coordinates": [2.5, 49.5], "type": "Point"},
        "properties": {
            "recordId": 1,
            "recordVersion": 1,
        },
        "id": 1,
    }


###############################################################################
# Test alteration of file with 2D point


test_ogr_s101_read_point_2d_altered_params = [
    (
        ".//DDFField[@name='PRID']//DDFSubfield[@name='RCID']",
        0,
        "Record index 2: invalid value 0 for RCID subfield of PRID",
        "dataset_opening",
        True,
        None,
    ),
    (
        ".//DDFField[@name='C2IT']",
        None,
        (
            "Record index 2, RCNM=110, RCID=1: invalid sequence of fields.",
            "Record index=0 of PRID: No C2IT or C3IT field found",
        ),
        "dataset_opening",
        True,
        None,
    ),
    (
        ".//DDFSubfield[@name='YCOO']",
        905000000,
        "Record index=0 of PRID: wrong coordinate value: lon=2.500000, lat=90.500000",
        "feature_iteration",
        True,
        {
            "type": "Feature",
            "geometry": {"coordinates": [2.5, 90.5], "type": "Point"},
            "properties": {
                "recordId": 1,
                "recordVersion": 1,
            },
            "id": 1,
        },
    ),
    (
        ".//DDFSubfield[@name='XCOO']",
        1805000000,
        "Record index=0 of PRID: wrong coordinate value: lon=180.500000, lat=49.500000",
        "feature_iteration",
        True,
        {
            "type": "Feature",
            "geometry": {"coordinates": [180.5, 49.5], "type": "Point"},
            "properties": {
                "recordId": 1,
                "recordVersion": 1,
            },
            "id": 1,
        },
    ),
]


@pytest.mark.parametrize(
    "xpath,value,exception_text,exception_location,recoverable,expected_json",
    test_ogr_s101_read_point_2d_altered_params,
    ids=[f"case{i}" for i in range(len(test_ogr_s101_read_point_2d_altered_params))],
)
def test_ogr_s101_read_point_2d_altered(
    tmp_path,
    xpath,
    value,
    exception_text,
    exception_location,
    recoverable,
    expected_json,
):
    ds = _alter_xml(
        __file__,
        "data/s101/point_2d_minimum.xml",
        tmp_path,
        xpath,
        value,
        exception_text,
        recoverable,
        exception_location=exception_location,
    )
    if ds:
        lyr = ds.GetLayerByName("Point2D")
        if lyr is None:
            assert expected_json is None
        else:
            if isinstance(exception_text, tuple):
                exception_text = exception_text[1]

            with gdaltest.error_raised(
                gdal.CE_Warning if exception_text else gdal.CE_None,
                match=exception_text,
            ):
                f = lyr.GetNextFeature()
            if f:
                assert f.ExportToJson(as_object=True) == expected_json
            else:
                assert expected_json is None
    else:
        assert expected_json is None


###############################################################################
# Test file with 3D point


def test_ogr_s101_read_point_3d_minimum():

    with gdaltest.error_raised(gdal.CE_None):
        ds = ogr.Open("data/s101/point_3d_minimum.000")

    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayerByName("Point3D_meanHighWaterSprings")
    assert lyr.GetSpatialRef().GetName() == "WGS 84 + meanHighWaterSprings depth"
    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {"coordinates": [2.5, 49.5, 15.5], "type": "Point"},
        "properties": {
            "recordId": 1,
            "recordVersion": 1,
        },
        "id": 1,
    }


###############################################################################
# Test alteration of file with 3D point


test_ogr_s101_read_point_3d_altered_params = [
    (
        ".//DDFField[@name='PRID']//DDFSubfield[@name='RCID']",
        0,
        "Record index 2: invalid value 0 for RCID subfield of PRID",
        "dataset_opening",
        True,
        None,
    ),
    (
        ".//DDFField[@name='C3IT']",
        None,
        (
            "Record index 2, RCNM=110, RCID=1: invalid sequence of fields",
            "Record index=0 of PRID: No C2IT or C3IT field found",
        ),
        "dataset_opening",
        True,
        None,
    ),
    (
        ".//DDFSubfield[@name='VCID']",
        1,
        "Record index=0 of PRID: VCID subfield = 1 of C3IT field points to a non-3D CRS",
        "dataset_opening",
        True,
        None,
    ),
    (
        ".//DDFSubfield[@name='VCID']",
        3,
        "Record index=0 of PRID: Unknown value 3 for VCID subfield of C3IT field",
        "dataset_opening",
        True,
        None,
    ),
    (
        ".//DDFSubfield[@name='YCOO']",
        905000000,
        "Record index=0 of PRID: wrong coordinate value: lon=2.500000, lat=90.500000",
        "feature_iteration",
        True,
        {
            "type": "Feature",
            "geometry": {"coordinates": [2.5, 90.5, 15.5], "type": "Point"},
            "properties": {
                "recordId": 1,
                "recordVersion": 1,
            },
            "id": 1,
        },
    ),
    (
        ".//DDFSubfield[@name='XCOO']",
        1805000000,
        "Record index=0 of PRID: wrong coordinate value: lon=180.500000, lat=49.500000",
        "feature_iteration",
        True,
        {
            "type": "Feature",
            "geometry": {"coordinates": [180.5, 49.5, 15.5], "type": "Point"},
            "properties": {
                "recordId": 1,
                "recordVersion": 1,
            },
            "id": 1,
        },
    ),
]


@pytest.mark.parametrize(
    "xpath,value,exception_text,exception_location,recoverable,expected_json",
    test_ogr_s101_read_point_3d_altered_params,
    ids=[f"case{i}" for i in range(len(test_ogr_s101_read_point_3d_altered_params))],
)
def test_ogr_s101_read_point_3d_altered(
    tmp_path,
    xpath,
    value,
    exception_text,
    exception_location,
    recoverable,
    expected_json,
):
    ds = _alter_xml(
        __file__,
        "data/s101/point_3d_minimum.xml",
        tmp_path,
        xpath,
        value,
        exception_text,
        recoverable,
        exception_location=exception_location,
    )
    if ds:
        lyr = ds.GetLayerByName("Point3D_meanHighWaterSprings")
        if lyr is None:
            assert expected_json is None
        else:

            if isinstance(exception_text, tuple):
                exception_text = exception_text[1]

            with gdaltest.error_raised(
                gdal.CE_Warning if exception_text else gdal.CE_None,
                match=exception_text,
            ):
                f = lyr.GetNextFeature()
            if f:
                assert f.ExportToJson(as_object=True) == expected_json
            else:
                assert expected_json is None
    else:
        assert expected_json is None


###############################################################################
# Test file with mix of 2D and 3D points


def test_ogr_s101_read_point():

    with gdaltest.error_raised(gdal.CE_None):
        ds = ogr.Open("data/s101/point.000")

    assert ds.GetLayerCount() == 4

    lyr = ds.GetLayerByName("informationType")
    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": None,
        "properties": {
            "recordId": 1,
            "recordVersion": 1,
            "informationType": "SpatialQuality",
            "text": "my text",
        },
        "id": 1,
    }

    lyr = ds.GetLayerByName("Point2D")
    assert lyr.GetSpatialRef().GetAuthorityCode() == "4326"

    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {"coordinates": [2.0, 49.0], "type": "Point"},
        "properties": {
            "infoAssociationRoleCode": None,
            "colour": None,
            "infoAssociationCode": None,
            "infoAssociationRecordId": None,
            "recordId": 1,
            "recordVersion": 1,
        },
        "id": 1,
    }

    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {"coordinates": [2.5, 49.5], "type": "Point"},
        "properties": {
            "recordId": 4,
            "recordVersion": 1,
            "infoAssociationRoleCode": "defines",
            "infoAssociationCode": "SpatialAssociation",
            "infoAssociationRecordId": 1,
            "colour": 2,
        },
        "id": 2,
    }

    lyr = ds.GetLayerByName("Point3D_meanHighWaterSprings")
    assert lyr.GetSpatialRef().GetName() == "WGS 84 + meanHighWaterSprings depth"
    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {"coordinates": [2.5, 49.5, 15.5], "type": "Point"},
        "properties": {
            "recordId": 2,
            "recordVersion": 1,
        },
        "id": 1,
    }

    lyr = ds.GetLayerByName("Point3D_approximateLowestAstronomicalTide")
    assert (
        lyr.GetSpatialRef().GetName()
        == "WGS 84 + approximateLowestAstronomicalTide depth"
    )
    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {"coordinates": [-2.5, -49.5, -15.5], "type": "Point"},
        "properties": {
            "infoAssociationRoleCode": "defines",
            "infoAssociationCode": "SpatialAssociation",
            "recordId": 3,
            "recordVersion": 1,
            "infoAssociationRecordId": 1,
        },
        "id": 1,
    }


###############################################################################
# Test alteration of file with mix of 2D and 3D points

test_ogr_s101_read_point_altered_params = [
    (
        "(.//DDFSubfield[@name='RRNM'])[1]",
        0,
        "Record index=2 of PRID: Invalid value for RRNM subfield of INAS field: got 0, expected 150",
        "dataset_opening",
        True,
        "Point3D_approximateLowestAstronomicalTide",
        {
            "type": "Feature",
            "geometry": {"type": "Point", "coordinates": [-2.5, -49.5, -15.5]},
            "properties": {
                "recordId": 3,
                "recordVersion": 1,
                "infoAssociationRecordId": 1,
                "infoAssociationCode": "SpatialAssociation",
                "infoAssociationRoleCode": "defines",
            },
            "id": 1,
        },
    ),
    (
        "(.//DDFSubfield[@name='RRID'])[1]",
        0,
        "Record index=2 of PRID: Invalid value 0 for RRID subfield of INAS field: does not match the record identifier of an existing InformationType record",
        "dataset_opening",
        True,
        "Point3D_approximateLowestAstronomicalTide",
        {
            "type": "Feature",
            "geometry": {"type": "Point", "coordinates": [-2.5, -49.5, -15.5]},
            "properties": {
                "recordId": 3,
                "recordVersion": 1,
                "infoAssociationRecordId": 0,
                "infoAssociationCode": "SpatialAssociation",
                "infoAssociationRoleCode": "defines",
            },
            "id": 1,
        },
    ),
    (
        "(.//DDFSubfield[@name='NIAC'])[1]",
        0,
        "Record index=2 of PRID: cannot find attribute code 0 in IACS field of the Dataset General Information Record",
        "dataset_opening",
        True,
        "Point3D_approximateLowestAstronomicalTide",
        {
            "type": "Feature",
            "geometry": {"type": "Point", "coordinates": [-2.5, -49.5, -15.5]},
            "properties": {
                "recordId": 3,
                "recordVersion": 1,
                "infoAssociationRecordId": 1,
                "infoAssociationCode": "informationAssociationCode0",
                "infoAssociationRoleCode": "defines",
            },
            "id": 1,
        },
    ),
    (
        "(.//DDFSubfield[@name='NARC'])[1]",
        0,
        "Record index=2 of PRID: cannot find attribute code 0 in ARCS field of the Dataset General Information Record",
        "dataset_opening",
        True,
        "Point3D_approximateLowestAstronomicalTide",
        {
            "type": "Feature",
            "geometry": {"type": "Point", "coordinates": [-2.5, -49.5, -15.5]},
            "properties": {
                "recordId": 3,
                "recordVersion": 1,
                "infoAssociationRecordId": 1,
                "infoAssociationCode": "SpatialAssociation",
                "infoAssociationRoleCode": "associationRoleCode0",
            },
            "id": 1,
        },
    ),
    (
        "(.//DDFRecord[.//DDFField[@name='PRID']])[1]",
        """<DDFRecord>
  <DDFField name="PRID">
    <DDFSubfield name="RCNM" type="integer">110</DDFSubfield>
    <DDFSubfield name="RCID" type="integer">1</DDFSubfield>
    <DDFSubfield name="RVER" type="integer">1</DDFSubfield>
    <DDFSubfield name="RUIN" type="integer">1</DDFSubfield>
  </DDFField>
  <DDFField name="C2IT">
    <DDFSubfield name="YCOO" type="integer">490000000</DDFSubfield>
    <DDFSubfield name="XCOO" type="integer">20000000</DDFSubfield>
  </DDFField>
  <DDFField name="C3IT">
    <DDFSubfield name="VCID" type="integer">2</DDFSubfield>
    <DDFSubfield name="YCOO" type="integer">495000000</DDFSubfield>
    <DDFSubfield name="XCOO" type="integer">25000000</DDFSubfield>
    <DDFSubfield name="ZCOO" type="integer">155</DDFSubfield>
  </DDFField>
</DDFRecord>""",
        "Record index 3, RCNM=110, RCID=1: invalid sequence of fields",
        "dataset_opening",
        True,
        "Point3D_meanHighWaterSprings",
        {
            "type": "Feature",
            "geometry": {"type": "Point", "coordinates": [2.5, 49.5, 15.5]},
            "properties": {"recordId": 1, "recordVersion": 1},
            "id": 1,
        },
    ),
    (
        "(.//DDFRecord[.//DDFField[@name='PRID']])[1]",
        """<DDFRecord>
  <DDFField name="PRID">
    <DDFSubfield name="RCNM" type="integer">110</DDFSubfield>
    <DDFSubfield name="RCID" type="integer">1</DDFSubfield>
    <DDFSubfield name="RVER" type="integer">1</DDFSubfield>
    <DDFSubfield name="RUIN" type="integer">1</DDFSubfield>
  </DDFField>
</DDFRecord>""",
        (
            "Record index 3, RCNM=110, RCID=1: invalid sequence of fields",
            "Record index=0 of PRID: No C2IT or C3IT field found",
        ),
        "dataset_opening",
        True,
        None,
        None,
    ),
]


@pytest.mark.parametrize(
    "xpath,value,exception_text,exception_location,recoverable,layer_name,expected_json",
    test_ogr_s101_read_point_altered_params,
    ids=[f"case{i}" for i in range(len(test_ogr_s101_read_point_altered_params))],
)
def test_ogr_s101_read_point_altered(
    tmp_path,
    xpath,
    value,
    exception_text,
    exception_location,
    recoverable,
    layer_name,
    expected_json,
):
    ds = _alter_xml(
        __file__,
        "data/s101/point.xml",
        tmp_path,
        xpath,
        value,
        exception_text,
        recoverable,
        exception_location=exception_location,
    )
    if ds:
        lyr = ds.GetLayerByName(layer_name)
        if lyr is None:
            assert expected_json is None
        else:
            if isinstance(exception_text, tuple):
                exception_text = exception_text[1]

            if "invalid sequence of fields" in exception_text:
                exception_text = None

            with gdaltest.error_raised(
                gdal.CE_Warning if exception_text else gdal.CE_None,
                match=exception_text,
            ):
                f = lyr.GetNextFeature()
            if f:
                assert f.ExportToJson(as_object=True) == expected_json
            else:
                assert expected_json is None
    else:
        assert expected_json is None
