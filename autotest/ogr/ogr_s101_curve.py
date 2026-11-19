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
# Test file with curve


def test_ogr_s101_read_curve():

    with gdaltest.error_raised(gdal.CE_None):
        ds = ogr.Open("data/s101/curve.000")

    lyr = ds.GetLayerByName("Curve")
    assert lyr.GetSpatialRef().GetAuthorityCode() == "4326"
    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {
            "type": "LineString",
            "coordinates": [[2.0, 49.0], [2.5, 49.5], [3.0, 50.0]],
        },
        "properties": {
            "recordId": 1,
            "recordVersion": 1,
            "infoAssociationRecordId": 1,
            "infoAssociationCode": "SpatialAssociation",
            "infoAssociationRoleCode": "defines",
        },
        "id": 1,
    }

    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {
            "type": "LineString",
            "coordinates": [[2.0, 49.0], [2.5, 49.5], [2.0, 49.0]],
        },
        "properties": {
            "recordId": 2,
            "recordVersion": 1,
            "infoAssociationRecordId": None,
            "infoAssociationCode": None,
            "infoAssociationRoleCode": None,
        },
        "id": 2,
    }

    assert lyr.GetNextFeature() is None


###############################################################################
# Test alteration of file with curve


test_ogr_s101_read_curve_altered_params = [
    (
        "(//DDFField[@name='INAS']/DDFSubfield[@name='RRNM'])[1]",
        0,
        "Record index=0 of CRID: Invalid value for RRNM subfield of INAS field: got 0, expected 150",
        "dataset_opening",
        True,
        "Curve",
        {
            "type": "Feature",
            "geometry": {
                "type": "LineString",
                "coordinates": [[2.0, 49.0], [2.5, 49.5], [3.0, 50.0]],
            },
            "properties": {
                "recordId": 1,
                "recordVersion": 1,
                "infoAssociationRecordId": 1,
                "infoAssociationCode": "SpatialAssociation",
                "infoAssociationRoleCode": "defines",
            },
            "id": 1,
        },
    ),
    (
        "(//DDFField[@name='C2IL']/DDFSubfield[@name='YCOO'])[1]",
        905000000,
        "Record index=0 of CRID: wrong coordinate value: lon=2.000000, lat=90.500000",
        "feature_iteration",
        True,
        "Curve",
        {
            "type": "Feature",
            "geometry": {
                "type": "LineString",
                "coordinates": [[2.0, 90.5], [2.5, 49.5], [3.0, 50.0]],
            },
            "properties": {
                "recordId": 1,
                "recordVersion": 1,
                "infoAssociationRecordId": 1,
                "infoAssociationCode": "SpatialAssociation",
                "infoAssociationRoleCode": "defines",
            },
            "id": 1,
        },
    ),
    (
        "(//DDFField[@name='C2IL']/DDFSubfield[@name='YCOO'])[1]",
        105000000,
        "Record index 0 of CRID: Point record 10 pointed by 0 instance of PTAS field does not match first point of curve",
        "feature_iteration",
        True,
        "Curve",
        {
            "type": "Feature",
            "geometry": {
                "type": "LineString",
                "coordinates": [[2.0, 10.5], [2.5, 49.5], [3.0, 50.0]],
            },
            "properties": {
                "recordId": 1,
                "recordVersion": 1,
                "infoAssociationRecordId": 1,
                "infoAssociationCode": "SpatialAssociation",
                "infoAssociationRoleCode": "defines",
            },
            "id": 1,
        },
    ),
    (
        "(//DDFField[@name='C2IL']/DDFSubfield[@name='YCOO'])[3]",
        105000000,
        "Record index 0 of CRID: Point record 20 pointed by 1 instance of PTAS field does not match end point of curve",
        "feature_iteration",
        True,
        "Curve",
        {
            "type": "Feature",
            "geometry": {
                "type": "LineString",
                "coordinates": [[2.0, 49.0], [2.5, 49.5], [3.0, 10.5]],
            },
            "properties": {
                "recordId": 1,
                "recordVersion": 1,
                "infoAssociationRecordId": 1,
                "infoAssociationCode": "SpatialAssociation",
                "infoAssociationRoleCode": "defines",
            },
            "id": 1,
        },
    ),
    (
        "(//DDFField[@name='PTAS']/DDFSubfield[@name='RRNM'])[1]",
        1,
        "Record index 0 of CRID: Invalid value for RRNM subfield of 0 instance of PTAS field: got 1, expected 110",
        "feature_iteration",
        True,
        "Curve",
        {
            "type": "Feature",
            "geometry": {
                "type": "LineString",
                "coordinates": [[2.0, 49.0], [2.5, 49.5], [3.0, 50.0]],
            },
            "properties": {
                "recordId": 1,
                "recordVersion": 1,
                "infoAssociationRecordId": 1,
                "infoAssociationCode": "SpatialAssociation",
                "infoAssociationRoleCode": "defines",
            },
            "id": 1,
        },
    ),
    (
        "(//DDFField[@name='PTAS']/DDFSubfield[@name='TOPI'])[1]",
        0,
        "Record index 0 of CRID: Invalid value for TOPI subfield of 0 instance of PTAS field: got 0, expected 1",
        "feature_iteration",
        True,
        "Curve",
        {
            "type": "Feature",
            "geometry": {
                "type": "LineString",
                "coordinates": [[2.0, 49.0], [2.5, 49.5], [3.0, 50.0]],
            },
            "properties": {
                "recordId": 1,
                "recordVersion": 1,
                "infoAssociationRecordId": 1,
                "infoAssociationCode": "SpatialAssociation",
                "infoAssociationRoleCode": "defines",
            },
            "id": 1,
        },
    ),
    (
        "(//DDFField[@name='PTAS']/DDFSubfield[@name='TOPI'])[2]",
        0,
        "Record index 0 of CRID: Invalid value for TOPI subfield of 1 instance of PTAS field: got 0, expected 2",
        "feature_iteration",
        True,
        "Curve",
        {
            "type": "Feature",
            "geometry": {
                "type": "LineString",
                "coordinates": [[2.0, 49.0], [2.5, 49.5], [3.0, 50.0]],
            },
            "properties": {
                "recordId": 1,
                "recordVersion": 1,
                "infoAssociationRecordId": 1,
                "infoAssociationCode": "SpatialAssociation",
                "infoAssociationRoleCode": "defines",
            },
            "id": 1,
        },
    ),
    (
        "(//DDFField[@name='PTAS']/DDFSubfield[@name='RRID'])[1]",
        1,
        "Record index 0 of CRID: No point record matching RRID=1 value of 0 instance of PTAS field",
        "feature_iteration",
        True,
        "Curve",
        {
            "type": "Feature",
            "geometry": {
                "type": "LineString",
                "coordinates": [[2.0, 49.0], [2.5, 49.5], [3.0, 50.0]],
            },
            "properties": {
                "recordId": 1,
                "recordVersion": 1,
                "infoAssociationRecordId": 1,
                "infoAssociationCode": "SpatialAssociation",
                "infoAssociationRoleCode": "defines",
            },
            "id": 1,
        },
    ),
    (
        "(//DDFField[@name='PTAS']/DDFSubfield[@name='TOPI'])[3]",
        0,
        "Record index 1 of CRID: Invalid value for TOPI subfield of 0 instance of PTAS field: got 0, expected 3",
        "feature_iteration",
        True,
        "Curve",
        {
            "type": "Feature",
            "geometry": {
                "type": "LineString",
                "coordinates": [[2.0, 49.0], [2.5, 49.5], [2.0, 49.0]],
            },
            "properties": {
                "recordId": 2,
                "recordVersion": 1,
                "infoAssociationRecordId": None,
                "infoAssociationCode": None,
                "infoAssociationRoleCode": None,
            },
            "id": 2,
        },
    ),
    (
        "(//DDFField[@name='SEGH']/DDFSubfield[@name='INTP'])[1]",
        1,
        "Record index 0 of CRID: Invalid value for INTP subfield of SEGH field: got 1, expected 4",
        "feature_iteration",
        True,
        "Curve",
        {
            "type": "Feature",
            "geometry": {
                "type": "LineString",
                "coordinates": [[2.0, 49.0], [2.5, 49.5], [3.0, 50.0]],
            },
            "properties": {
                "recordId": 1,
                "recordVersion": 1,
                "infoAssociationRecordId": 1,
                "infoAssociationCode": "SpatialAssociation",
                "infoAssociationRoleCode": "defines",
            },
            "id": 1,
        },
    ),
    (
        "(.//DDFField[@name='PTAS'])[1]",
        """<DDFField name="PTAS">
        <DDFSubfield name="RRNM" type="integer">110</DDFSubfield>
        <DDFSubfield name="RRID" type="integer">10</DDFSubfield>
        <DDFSubfield name="TOPI" type="integer">1</DDFSubfield>
        <DDFSubfield name="RRNM" type="integer">110</DDFSubfield>
        <DDFSubfield name="RRID" type="integer">20</DDFSubfield>
        <DDFSubfield name="TOPI" type="integer">2</DDFSubfield>
        <DDFSubfield name="RRNM" type="integer">110</DDFSubfield>
        <DDFSubfield name="RRID" type="integer">20</DDFSubfield>
        <DDFSubfield name="TOPI" type="integer">3</DDFSubfield>
        </DDFField>""",
        "Record index 0 of CRID: Invalid repeat count for PTAS field: got 3, expected 1 or 2",
        "feature_iteration",
        True,
        "Curve",
        {
            "type": "Feature",
            "geometry": {
                "type": "LineString",
                "coordinates": [[2.0, 49.0], [2.5, 49.5], [3.0, 50.0]],
            },
            "properties": {
                "recordId": 1,
                "recordVersion": 1,
                "infoAssociationRecordId": 1,
                "infoAssociationCode": "SpatialAssociation",
                "infoAssociationRoleCode": "defines",
            },
            "id": 1,
        },
    ),
]


@pytest.mark.parametrize(
    "xpath,value,exception_text,exception_location,recoverable,layer_name,expected_json",
    test_ogr_s101_read_curve_altered_params,
    ids=[f"case{i}" for i in range(len(test_ogr_s101_read_curve_altered_params))],
)
def test_ogr_s101_read_curve_altered(
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
        "data/s101/curve.xml",
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
            if "invalid sequence of fields" in exception_text:
                exception_text = None

            if "Record index 1 of CRID" in exception_text:
                lyr.GetNextFeature()

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
