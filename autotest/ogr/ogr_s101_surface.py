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
import ogrtest
import pytest

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_driver("S101")

from .ogr_s101 import _alter_xml

###############################################################################
# Test file with surface


def test_ogr_s101_read_surface():

    with gdaltest.error_raised(gdal.CE_None):
        ds = ogr.Open("data/s101/surface.000")

    lyr = ds.GetLayerByName("Surface")
    assert lyr.GetSpatialRef().GetAuthorityCode() == "4326"
    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {
            "type": "Polygon",
            "coordinates": [
                [[2.0, 49.0], [2.0, 50.0], [3.0, 50.0], [3.0, 49.0], [2.0, 49.0]],
                [[2.01, 49.1], [2.99, 49.1], [2.99, 49.9], [2.01, 49.9], [2.01, 49.1]],
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

    assert lyr.GetNextFeature() is None


###############################################################################
# Test alteration of file with surface


test_ogr_s101_read_surface_altered_params = [
    (
        "(//DDFField[@name='INAS']/DDFSubfield[@name='RRNM'])[1]",
        0,
        "Record index=0 of SRID: Invalid value for RRNM subfield of INAS field: got 0, expected 150",
        "dataset_opening",
        True,
        "Surface",
        {
            "type": "Feature",
            "geometry": {
                "type": "Polygon",
                "coordinates": [
                    [[2.0, 49.0], [2.0, 50.0], [3.0, 50.0], [3.0, 49.0], [2.0, 49.0]],
                    [
                        [2.01, 49.1],
                        [2.99, 49.1],
                        [2.99, 49.9],
                        [2.01, 49.9],
                        [2.01, 49.1],
                    ],
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
        "(//DDFField[@name='RIAS'])[1]",
        None,
        (
            "Record index 9, RCNM=130, RCID=1: invalid sequence of fields",
            "Record index=0 of SRID: no RIAS field",
        ),
        "dataset_opening",
        True,
        "Surface",
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
        "(//DDFField[@name='RIAS']/DDFSubfield[@name='RRNM'])[1]",
        0,
        "Record index=0 of SRID: Invalid value for RRNM subfield of 0 instance of RIAS field: got 0, expected 120 or 125",
        "feature_iteration",
        True,
        "Surface",
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
        "(//DDFField[@name='RIAS']/DDFSubfield[@name='ORNT'])[1]",
        0,
        "Record index=0 of SRID: Invalid value for ORNT subfield of 0 instance of RIAS field: got 0, expected 1 or 2",
        "feature_iteration",
        True,
        "Surface",
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
        "(//DDFField[@name='RIAS']/DDFSubfield[@name='RRID'])[1]",
        0,
        "Record index=0 of SRID: Value (RRNM=120, RRID=0) of instance 0 of RIAS field does not point to an existing record",
        "feature_iteration",
        True,
        "Surface",
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
        "(//DDFField[@name='RIAS']/DDFSubfield[@name='RRID'])[2]",
        0,
        "Record index=0 of SRID: Value (RRNM=125, RRID=0) of instance 1 of RIAS field does not point to an existing record",
        "feature_iteration",
        True,
        "Surface",
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
        "(//DDFField[@name='RIAS']/DDFSubfield[@name='USAG'])[1]",
        0,
        "Record index=0 of SRID: Invalid value for USAG subfield of 0 instance of RIAS field: got 0, expected 1 or 2",
        "feature_iteration",
        True,
        "Surface",
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
        "(//DDFField[@name='RIAS']/DDFSubfield[@name='RAUI'])[1]",
        0,
        "Record index=0 of SRID: wrong value 0 for RAUI subfield of 0 instance of RIAS field.",
        "feature_iteration",
        True,
        "Surface",
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
        None,
        "Invalid value for RRNM subfield of 0 instance of CUCO field: got 0, expected 120 or 125",
        "feature_iteration",
        True,
        "Surface",
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
        "(//DDFField[@name='RIAS']/DDFSubfield[@name='ORNT'])[1]",
        1,
        "Record index=0 of SRID: exterior ring orientation is not clockwise",
        "feature_iteration",
        True,
        "Surface",
        {
            "type": "Feature",
            "geometry": {
                "type": "Polygon",
                "coordinates": [
                    [[2.0, 49.0], [3.0, 49.0], [3.0, 50.0], [2.0, 50.0], [2.0, 49.0]],
                    [
                        [2.01, 49.1],
                        [2.99, 49.1],
                        [2.99, 49.9],
                        [2.01, 49.9],
                        [2.01, 49.1],
                    ],
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
        "(//DDFField[@name='RIAS']/DDFSubfield[@name='ORNT'])[2]",
        2,
        "Record index=0 of SRID: orientation of interior ring of index 1 is not counter-clockwise",
        "feature_iteration",
        True,
        "Surface",
        {
            "type": "Feature",
            "geometry": {
                "type": "Polygon",
                "coordinates": [
                    [[2.0, 49.0], [2.0, 50.0], [3.0, 50.0], [3.0, 49.0], [2.0, 49.0]],
                    [
                        [2.01, 49.1],
                        [2.01, 49.9],
                        [2.99, 49.9],
                        [2.99, 49.1],
                        [2.01, 49.1],
                    ],
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
        "(//DDFField[@name='RIAS'])[1]",
        """<DDFField name="RIAS">
    <DDFSubfield name="RRNM" type="integer">120</DDFSubfield>
    <DDFSubfield name="RRID" type="integer">3</DDFSubfield>
    <DDFSubfield name="ORNT" type="integer">1</DDFSubfield>
    <DDFSubfield name="USAG" type="integer">1</DDFSubfield>
    <DDFSubfield name="RAUI" type="integer">1</DDFSubfield>
  </DDFField>""",
        "Record index=0 of SRID: Ring of index 0 is not closed",
        "feature_iteration",
        True,
        "Surface",
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
        "(//DDFField[@name='RIAS'])[1]",
        """<DDFField name="RIAS">
    <DDFSubfield name="RRNM" type="integer">125</DDFSubfield>
    <DDFSubfield name="RRID" type="integer">1</DDFSubfield>
    <DDFSubfield name="ORNT" type="integer">1</DDFSubfield>
    <DDFSubfield name="USAG" type="integer">2</DDFSubfield>
    <DDFSubfield name="RAUI" type="integer">1</DDFSubfield>
  </DDFField>""",
        "Record index=0 of SRID: no ring tagged as exterior ring",
        "feature_iteration",
        True,
        "Surface",
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
        "(//DDFField[@name='RIAS'])[1]",
        """<DDFField name="RIAS">
    <DDFSubfield name="RRNM" type="integer">120</DDFSubfield>
    <DDFSubfield name="RRID" type="integer">1</DDFSubfield>
    <DDFSubfield name="ORNT" type="integer">2</DDFSubfield>
    <DDFSubfield name="USAG" type="integer">1</DDFSubfield>
    <DDFSubfield name="RAUI" type="integer">1</DDFSubfield>

    <DDFSubfield name="RRNM" type="integer">120</DDFSubfield>
    <DDFSubfield name="RRID" type="integer">1</DDFSubfield>
    <DDFSubfield name="ORNT" type="integer">2</DDFSubfield>
    <DDFSubfield name="USAG" type="integer">1</DDFSubfield>
    <DDFSubfield name="RAUI" type="integer">1</DDFSubfield>
  </DDFField>""",
        "Record index=0 of SRID: several rings tagged as exterior rings",
        "feature_iteration",
        True,
        "Surface",
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
        "(//DDFField[@name='RIAS'])[1]",
        """<DDFField name="RIAS">
    <DDFSubfield name="RRNM" type="integer">120</DDFSubfield>
    <DDFSubfield name="RRID" type="integer">1</DDFSubfield>
    <DDFSubfield name="ORNT" type="integer">2</DDFSubfield>
    <DDFSubfield name="USAG" type="integer">1</DDFSubfield>
    <DDFSubfield name="RAUI" type="integer">1</DDFSubfield>

    <DDFSubfield name="RRNM" type="integer">120</DDFSubfield>
    <DDFSubfield name="RRID" type="integer">1</DDFSubfield>
    <DDFSubfield name="ORNT" type="integer">1</DDFSubfield>
    <DDFSubfield name="USAG" type="integer">2</DDFSubfield>
    <DDFSubfield name="RAUI" type="integer">1</DDFSubfield>
  </DDFField>""",
        "Record index=0 of SRID: surface is invalid: Self-intersection",
        "feature_iteration",
        True,
        "Surface",
        {
            "type": "Feature",
            "geometry": {
                "type": "Polygon",
                "coordinates": [
                    [[2.0, 49.0], [2.0, 50.0], [3.0, 50.0], [3.0, 49.0], [2.0, 49.0]],
                    [[2.0, 49.0], [3.0, 49.0], [3.0, 50.0], [2.0, 50.0], [2.0, 49.0]],
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
    test_ogr_s101_read_surface_altered_params,
    ids=[f"case{i}" for i in range(len(test_ogr_s101_read_surface_altered_params))],
)
def test_ogr_s101_read_surface_altered(
    tmp_path,
    xpath,
    value,
    exception_text,
    exception_location,
    recoverable,
    layer_name,
    expected_json,
):
    if "Self-intersection" in exception_text and not ogrtest.have_geos():
        pytest.skip("GEOS missing")

    ds = _alter_xml(
        __file__,
        "data/s101/surface.xml",
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
