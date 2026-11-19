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
# Test file with multipoints


def test_ogr_s101_read_multipoint():

    with gdaltest.error_raised(gdal.CE_None):
        ds = ogr.Open("data/s101/multipoint.000")

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

    lyr = ds.GetLayerByName("MultiPoint2D")
    assert lyr.GetSpatialRef().GetAuthorityCode() == "4326"

    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {"coordinates": [[2.0, 49.0], [3.0, 48.0]], "type": "MultiPoint"},
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
        "geometry": {"coordinates": [[2.5, 49.5]], "type": "MultiPoint"},
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

    lyr = ds.GetLayerByName("MultiPoint3D_meanHighWaterSprings")
    assert lyr.GetSpatialRef().GetName() == "WGS 84 + meanHighWaterSprings depth"
    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {
            "coordinates": [[2.5, 49.5, 15.5], [3.5, 48.5, -15.5]],
            "type": "MultiPoint",
        },
        "properties": {
            "recordId": 2,
            "recordVersion": 1,
        },
        "id": 1,
    }

    lyr = ds.GetLayerByName("MultiPoint3D_approximateLowestAstronomicalTide")
    assert (
        lyr.GetSpatialRef().GetName()
        == "WGS 84 + approximateLowestAstronomicalTide depth"
    )
    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {"coordinates": [[-2.5, -49.5, -15.5]], "type": "MultiPoint"},
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
# Test alteration of file with multipoint

test_ogr_s101_read_multipoint_altered_params = [
    (
        "(.//DDFSubfield[@name='YCOO'])[1]",
        905000000,
        "Record index=0 of MRID: wrong coordinate value: lon=2.000000, lat=90.500000",
        "feature_iteration",
        True,
        "MultiPoint2D",
        {
            "type": "Feature",
            "geometry": {
                "type": "MultiPoint",
                "coordinates": [[2.0, 90.5], [3.0, 48.0]],
            },
            "properties": {
                "recordId": 1,
                "recordVersion": 1,
                "infoAssociationRecordId": None,
                "infoAssociationCode": None,
                "infoAssociationRoleCode": None,
                "colour": None,
            },
            "id": 1,
        },
    ),
    (
        "(.//DDFSubfield[@name='VCID'])[1]",
        1,
        "Record index=1 of MRID: VCID subfield = 1 of C3IL field points to a non-3D CRS",
        "dataset_opening",
        True,
        None,
        None,
    ),
    (
        "(.//DDFSubfield[@name='VCID'])[1]",
        4,
        "Record index=1 of MRID: Unknown value 4 for VCID subfield of C3IL field",
        "dataset_opening",
        True,
        None,
        None,
    ),
    (
        "(.//DDFRecord[.//DDFField[@name='MRID']])[1]",
        """<DDFRecord>
  <DDFField name="MRID">
    <DDFSubfield name="RCNM" type="integer">115</DDFSubfield>
    <DDFSubfield name="RCID" type="integer">1</DDFSubfield>
    <DDFSubfield name="RVER" type="integer">1</DDFSubfield>
    <DDFSubfield name="RUIN" type="integer">1</DDFSubfield>
  </DDFField>
</DDFRecord>""",
        (
            "Record index 3, RCNM=115, RCID=1: invalid sequence of fields",
            "Record index=0 of MRID: No C2IL or C3IL field found",
        ),
        "dataset_opening",
        True,
        None,
        None,
    ),
]


@pytest.mark.parametrize(
    "xpath,value,exception_text,exception_location,recoverable,layer_name,expected_json",
    test_ogr_s101_read_multipoint_altered_params,
    ids=[f"case{i}" for i in range(len(test_ogr_s101_read_multipoint_altered_params))],
)
def test_ogr_s101_read_multipoint_altered(
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
        "data/s101/multipoint.xml",
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
