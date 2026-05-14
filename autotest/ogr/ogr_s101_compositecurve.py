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
# Test file with composite curve


def test_ogr_s101_read_compositecurve():

    with gdaltest.error_raised(gdal.CE_None):
        ds = ogr.Open("data/s101/compositecurve.000")

    lyr = ds.GetLayerByName("CompositeCurve")
    assert lyr.GetSpatialRef().GetAuthorityCode() == "4326"
    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {
            "type": "LineString",
            "coordinates": [
                [2.0, 49.0],
                [3.0, 49.0],
                [3.0, 50.0],
                [2.0, 50.0],
                [2.0, 49.0],
            ],
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
            "coordinates": [
                [2.0, 49.0],
                [2.0, 50.0],
                [3.0, 50.0],
                [3.0, 49.0],
                [2.0, 49.0],
            ],
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

    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {
            "type": "LineString",
            "coordinates": [[2.0, 49.0], [3.0, 49.0], [3.0, 50.0]],
        },
        "properties": {
            "recordId": 3,
            "recordVersion": 1,
            "infoAssociationRecordId": None,
            "infoAssociationCode": None,
            "infoAssociationRoleCode": None,
        },
        "id": 3,
    }

    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {
            "type": "LineString",
            "coordinates": [[2.0, 49.0], [3.0, 49.0], [3.0, 50.0]],
        },
        "properties": {
            "recordId": 4,
            "recordVersion": 1,
            "infoAssociationRecordId": None,
            "infoAssociationCode": None,
            "infoAssociationRoleCode": None,
        },
        "id": 4,
    }

    assert lyr.GetNextFeature() is None


###############################################################################
# Test alteration of file with composite curve


test_ogr_s101_read_compositecurve_altered_params = [
    (
        "(//DDFField[@name='INAS']/DDFSubfield[@name='RRNM'])[1]",
        0,
        "Record index=0 of CCID: Invalid value for RRNM subfield of INAS field: got 0, expected 150",
        "dataset_opening",
        True,
        "CompositeCurve",
        {
            "type": "Feature",
            "geometry": {
                "type": "LineString",
                "coordinates": [
                    [2.0, 49.0],
                    [3.0, 49.0],
                    [3.0, 50.0],
                    [2.0, 50.0],
                    [2.0, 49.0],
                ],
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
        "(//DDFField[@name='CUCO'])[1]",
        None,
        (
            "Record index 7, RCNM=125, RCID=1: invalid sequence of fields",
            "Record index=0 of CCID: no CUCO field",
        ),
        "dataset_opening",
        True,
        "CompositeCurve",
        {
            "type": "Feature",
            "geometry": None,
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
        "(//DDFField[@name='CUCO']/DDFSubfield[@name='RRNM'])[1]",
        0,
        "Record index=0 of CCID: Invalid value for RRNM subfield of 0 instance of CUCO field: got 0, expected 120 or 125",
        "feature_iteration",
        True,
        "CompositeCurve",
        {
            "type": "Feature",
            "geometry": None,
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
        "(//DDFField[@name='CUCO']/DDFSubfield[@name='ORNT'])[1]",
        0,
        "Record index=0 of CCID: Invalid value for ORNT subfield of 0 instance of CUCO field: got 0, expected 1 or 2",
        "feature_iteration",
        True,
        "CompositeCurve",
        {
            "type": "Feature",
            "geometry": None,
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
        "(//DDFField[@name='CUCO']/DDFSubfield[@name='RRID'])[1]",
        0,
        "Record index=0 of CCID: Value (RRNM=120, RRID=0) of instance 0 of CUCO field does not point to an existing record",
        "feature_iteration",
        True,
        "CompositeCurve",
        {
            "type": "Feature",
            "geometry": None,
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
        "(//DDFField[@name='CUCO']/DDFSubfield[@name='ORNT'])[1]",
        2,
        "Record index=0 of CCID: Value (RRNM=120, RRID=2) of curve instance 1 extremity does not match composite curve extremity",
        "feature_iteration",
        True,
        "CompositeCurve",
        {
            "type": "Feature",
            "geometry": None,
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
        "(//DDFField[@name='CUCO']/DDFSubfield[@name='RRID'])[6]",
        0,
        "Record index=3 of CCID: Value (RRNM=125, RRID=0) of instance 0 of CUCO field does not point to an existing record",
        "feature_iteration",
        True,
        "CompositeCurve",
        {
            "type": "Feature",
            "geometry": None,
            "properties": {
                "recordId": 4,
                "recordVersion": 1,
                "infoAssociationRecordId": None,
                "infoAssociationCode": None,
                "infoAssociationRoleCode": None,
            },
            "id": 4,
        },
    ),
    (
        "(//DDFSubfield[@name='INTP'])[2]",
        0,
        "Invalid value for INTP subfield of SEGH field",
        "feature_iteration",
        True,
        "CompositeCurve",
        {
            "type": "Feature",
            "geometry": {
                "type": "LineString",
                "coordinates": [
                    [2.0, 49.0],
                    [3.0, 49.0],
                    [3.0, 50.0],
                    [2.0, 50.0],
                    [2.0, 49.0],
                ],
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
    test_ogr_s101_read_compositecurve_altered_params,
    ids=[
        f"case{i}" for i in range(len(test_ogr_s101_read_compositecurve_altered_params))
    ],
)
def test_ogr_s101_read_compositecurve_altered(
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
        "data/s101/compositecurve.xml",
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

            if "Record index=3 of CCID" in exception_text:
                lyr.GetNextFeature()
                lyr.GetNextFeature()
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
