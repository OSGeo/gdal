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
# Test file with features


def test_ogr_s101_read_feature():

    with gdaltest.error_raised(gdal.CE_None):
        ds = ogr.Open("data/s101/feature.000")

    assert set([lyr.GetName() for lyr in ds]) == set(
        [
            "informationType",
            "Point2D",
            "MultiPoint2D",
            "Curve",
            "CompositeCurve",
            "Surface",
            "FeatureType1_NoGeom",
            "FeatureType1_Point2D",
            "FeatureType2_MultiPoint2D",
            "FeatureType3_MultiPoint2D",
            "FeatureType4_CollectionOfMultiPoint",
            "FeatureType4_Line",
            "FeatureType5_MultiLine",
            "FeatureType6_Polygon",
            "FeatureType7_MultiPolygon",
        ]
    )

    lyr = ds.GetLayerByName("FeatureType1_NoGeom")
    assert lyr.GetSpatialRef() is None
    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": None,
        "properties": {
            "recordId": 1,
            "recordVersion": 1,
            "producingAgency": 12345,
            "featureIdentificationNumber": 11,
            "featureIdentificationSubdivision": 1,
            "text": "my text ATTR",
            "infoAssociationRecordId": 1,
            "infoAssociationCode": "SpatialAssociation",
            "infoAssociationRoleCode": "defines",
            "infoAssociation_text": "my text INAS",
            "featureAssociationRefRecordId": 1,
            "featureAssociationRefLayerName": "FeatureType1_NoGeom",
            "featureAssociationCode": "StructureEquipment",
            "featureAssociationRoleCode": "defines",
            "featureAssociation_text": "my text FASC",
        },
        "id": 1,
    }

    assert lyr.GetNextFeature() is None

    lyr = ds.GetLayerByName("FeatureType1_Point2D")
    assert lyr.GetSpatialRef().GetAuthorityCode() == "4326"
    assert lyr.GetGeomType() == ogr.wkbPoint
    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {"type": "Point", "coordinates": [2.0, 49.0]},
        "properties": {
            "recordId": 2,
            "recordVersion": 1,
            "producingAgency": 12345,
            "featureIdentificationNumber": 22,
            "featureIdentificationSubdivision": 1,
            "geometryLayerName": "Point2D",
            "geometryRecordId": 10,
            "scaleMinimum": None,
            "scaleMaximum": None,
        },
        "id": 1,
    }

    assert lyr.GetNextFeature() is None

    lyr = ds.GetLayerByName("FeatureType2_MultiPoint2D")
    assert lyr.GetSpatialRef().GetAuthorityCode() == "4326"
    assert lyr.GetGeomType() == ogr.wkbMultiPoint
    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {"type": "MultiPoint", "coordinates": [[2.0, 49.0], [2.01, 49.1]]},
        "properties": {
            "recordId": 3,
            "recordVersion": 1,
            "producingAgency": 12345,
            "featureIdentificationNumber": 33,
            "featureIdentificationSubdivision": 1,
            "geometryLayerName": ["Point2D", "Point2D"],
            "geometryRecordId": [10, 20],
            "scaleMinimum": None,
            "scaleMaximum": None,
        },
        "id": 1,
    }

    assert lyr.GetNextFeature() is None

    lyr = ds.GetLayerByName("FeatureType3_MultiPoint2D")
    assert lyr.GetSpatialRef().GetAuthorityCode() == "4326"
    assert lyr.GetGeomType() == ogr.wkbMultiPoint
    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {"type": "MultiPoint", "coordinates": [[2.0, 49.0], [2.01, 49.1]]},
        "properties": {
            "recordId": 4,
            "recordVersion": 1,
            "producingAgency": 12345,
            "featureIdentificationNumber": 44,
            "featureIdentificationSubdivision": 1,
            "geometryLayerName": ["Point2D", "Point2D"],
            "geometryRecordId": [10, 20],
            "scaleMinimum": None,
            "scaleMaximum": None,
        },
        "id": 1,
    }

    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {
            "type": "MultiPoint",
            "coordinates": [[-2.0, -49.0], [-2.01, -49.1]],
        },
        "properties": {
            "recordId": 5,
            "recordVersion": 1,
            "producingAgency": 12345,
            "featureIdentificationNumber": 55,
            "featureIdentificationSubdivision": 1,
            "geometryLayerName": ["MultiPoint2D"],
            "geometryRecordId": [1],
            "scaleMinimum": [1000],
            "scaleMaximum": [2000],
        },
        "id": 2,
    }

    assert lyr.GetNextFeature() is None

    lyr = ds.GetLayerByName("FeatureType4_CollectionOfMultiPoint")
    assert lyr.GetSpatialRef().GetAuthorityCode() == "4326"
    assert lyr.GetGeomType() == ogr.wkbGeometryCollection
    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {
            "type": "GeometryCollection",
            "geometries": [
                {"type": "MultiPoint", "coordinates": [[-2.0, -49.0], [-2.01, -49.1]]},
                {"type": "MultiPoint", "coordinates": [[-2.0, -49.0], [-2.01, -49.1]]},
            ],
        },
        "properties": {
            "recordId": 6,
            "recordVersion": 1,
            "producingAgency": 12345,
            "featureIdentificationNumber": 66,
            "featureIdentificationSubdivision": 1,
            "geometryLayerName": ["MultiPoint2D", "MultiPoint2D"],
            "geometryRecordId": [1, 1],
            "scaleMinimum": [1000, 3000],
            "scaleMaximum": [2000, 4000],
        },
        "id": 1,
    }

    assert lyr.GetNextFeature() is None

    lyr = ds.GetLayerByName("FeatureType4_Line")
    assert lyr.GetSpatialRef().GetAuthorityCode() == "4326"
    assert lyr.GetGeomType() == ogr.wkbLineString
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
            "recordId": 7,
            "recordVersion": 1,
            "producingAgency": 12345,
            "featureIdentificationNumber": 77,
            "featureIdentificationSubdivision": 1,
            "geometryLayerName": "Curve",
            "geometryRecordId": 1,
            "geometryOrientation": "forward",
            "scaleMinimum": 1000,
            "scaleMaximum": 2000,
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
            "recordId": 8,
            "recordVersion": 1,
            "producingAgency": 12345,
            "featureIdentificationNumber": 88,
            "featureIdentificationSubdivision": 1,
            "geometryLayerName": "Curve",
            "geometryRecordId": 1,
            "geometryOrientation": "reverse",
            "scaleMinimum": 1000,
            "scaleMaximum": 2000,
        },
        "id": 2,
    }

    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {
            "type": "LineString",
            "coordinates": [
                [2.01, 49.1],
                [2.99, 49.1],
                [2.99, 49.9],
                [2.01, 49.9],
                [2.01, 49.1],
            ],
        },
        "properties": {
            "recordId": 9,
            "recordVersion": 1,
            "producingAgency": 12345,
            "featureIdentificationNumber": 99,
            "featureIdentificationSubdivision": 1,
            "geometryLayerName": "CompositeCurve",
            "geometryRecordId": 1,
            "geometryOrientation": "forward",
            "scaleMinimum": 1000,
            "scaleMaximum": 2000,
        },
        "id": 3,
    }

    assert lyr.GetNextFeature() is None

    lyr = ds.GetLayerByName("FeatureType5_MultiLine")
    assert lyr.GetSpatialRef().GetAuthorityCode() == "4326"
    assert lyr.GetGeomType() == ogr.wkbMultiLineString
    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {
            "type": "MultiLineString",
            "coordinates": [
                [[2.0, 49.0], [3.0, 49.0], [3.0, 50.0], [2.0, 50.0], [2.0, 49.0]],
                [[2.01, 49.1], [2.01, 49.9], [2.99, 49.9], [2.99, 49.1], [2.01, 49.1]],
            ],
        },
        "properties": {
            "recordId": 10,
            "recordVersion": 1,
            "producingAgency": 12345,
            "featureIdentificationNumber": 100,
            "featureIdentificationSubdivision": 1,
            "geometryLayerName": ["Curve", "Curve"],
            "geometryRecordId": [1, 2],
            "geometryOrientation": ["forward", "reverse"],
            "scaleMinimum": [1000, 1000],
            "scaleMaximum": [2000, 2000],
        },
        "id": 1,
    }

    assert lyr.GetNextFeature() is None

    lyr = ds.GetLayerByName("FeatureType6_Polygon")
    assert lyr.GetSpatialRef().GetAuthorityCode() == "4326"
    assert lyr.GetGeomType() == ogr.wkbPolygon
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
            "recordId": 11,
            "recordVersion": 1,
            "producingAgency": 12345,
            "featureIdentificationNumber": 110,
            "featureIdentificationSubdivision": 1,
            "geometryLayerName": "Surface",
            "geometryRecordId": 1,
            "scaleMinimum": 1000,
            "scaleMaximum": 2000,
        },
        "id": 1,
    }

    assert lyr.GetNextFeature() is None

    lyr = ds.GetLayerByName("FeatureType7_MultiPolygon")
    assert lyr.GetSpatialRef().GetAuthorityCode() == "4326"
    assert lyr.GetGeomType() == ogr.wkbMultiPolygon
    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {
            "type": "MultiPolygon",
            "coordinates": [
                [
                    [[2.0, 49.0], [2.0, 50.0], [3.0, 50.0], [3.0, 49.0], [2.0, 49.0]],
                    [
                        [2.01, 49.1],
                        [2.99, 49.1],
                        [2.99, 49.9],
                        [2.01, 49.9],
                        [2.01, 49.1],
                    ],
                ],
                [
                    [[2.0, 49.0], [2.0, 50.0], [3.0, 50.0], [3.0, 49.0], [2.0, 49.0]],
                    [
                        [2.01, 49.1],
                        [2.99, 49.1],
                        [2.99, 49.9],
                        [2.01, 49.9],
                        [2.01, 49.1],
                    ],
                ],
            ],
        },
        "properties": {
            "recordId": 12,
            "recordVersion": 1,
            "producingAgency": 12345,
            "featureIdentificationNumber": 120,
            "featureIdentificationSubdivision": 1,
            "geometryLayerName": ["Surface", "Surface"],
            "geometryRecordId": [1, 1],
            "scaleMinimum": [1000, 1000],
            "scaleMaximum": [2000, 2000],
            "maskLayerName": ["Curve", "Curve", "CompositeCurve"],
            "maskRecordId": [1, 2, 1],
            "maskIndicator": [
                "truncatedByDataCoverageLimit",
                "truncatedByDataCoverageLimit",
                "suppressPortrayal",
            ],
        },
        "id": 1,
    }

    assert lyr.GetNextFeature() is None


###############################################################################
# Test alteration of file with feature type


test_ogr_s101_read_feature_altered_params = [
    (
        "(//DDFField[@name='FOID'])[1]",
        None,
        (
            "Record index 11, RCNM=100, RCID=1: invalid sequence of fields",
            "Feature type record index 0: no FOID field",
        ),
        "dataset_opening",
        True,
        "FeatureType1_NoGeom",
        {
            "type": "Feature",
            "geometry": None,
            "properties": {
                "recordId": 1,
                "recordVersion": 1,
                "text": "my text ATTR",
                "featureIdentificationNumber": None,
                "featureIdentificationSubdivision": None,
                "producingAgency": None,
                "infoAssociationRecordId": 1,
                "infoAssociationCode": "SpatialAssociation",
                "infoAssociationRoleCode": "defines",
                "infoAssociation_text": "my text INAS",
                "featureAssociationRefRecordId": 1,
                "featureAssociationRefLayerName": "FeatureType1_NoGeom",
                "featureAssociationCode": "StructureEquipment",
                "featureAssociationRoleCode": "defines",
                "featureAssociation_text": "my text FASC",
            },
            "id": 1,
        },
    ),
    (
        "(//DDFSubfield[@name='NFTC'])[1]",
        0,
        ("Features pointing at unknown feature type code 0", None),
        "dataset_opening",
        True,
        "unknownFeatureType0_NoGeom",
        {
            "type": "Feature",
            "geometry": None,
            "properties": {
                "recordId": 1,
                "recordVersion": 1,
                "producingAgency": 12345,
                "featureIdentificationNumber": 11,
                "featureIdentificationSubdivision": 1,
                "text": "my text ATTR",
                "infoAssociationRecordId": 1,
                "infoAssociationCode": "SpatialAssociation",
                "infoAssociationRoleCode": "defines",
                "infoAssociation_text": "my text INAS",
                "featureAssociationRefRecordId": 1,
                "featureAssociationRefLayerName": "unknownFeatureType0_NoGeom",
                "featureAssociationCode": "StructureEquipment",
                "featureAssociationRoleCode": "defines",
                "featureAssociation_text": "my text FASC",
            },
            "id": 1,
        },
    ),
    (
        "(//DDFField[@name='SPAS']/DDFSubfield[@name='RRNM'])[1]",
        0,
        (
            "Features pointing at unknown spatial record type 0",
            "Feature type record index 1, SPAS instance 0: Invalid RRNM = 0",
        ),
        "dataset_opening",
        True,
        "FeatureType1_UnknownGeomType0",
        {
            "type": "Feature",
            "geometry": None,
            "properties": {
                "recordId": 2,
                "recordVersion": 1,
                "producingAgency": 12345,
                "featureIdentificationNumber": 22,
                "featureIdentificationSubdivision": 1,
            },
            "id": 1,
        },
    ),
    (
        "(//DDFField[@name='SPAS']/DDFSubfield[@name='RRID'])[1]",
        0,
        (
            "Record index 1 of FRID: Point of id 0 does not exist",
            "Feature type record index 1, SPAS instance 0: Point of ID=0 does not exist",
        ),
        "dataset_opening",
        True,
        "FeatureType1_Point2D",
        {
            "type": "Feature",
            "geometry": None,
            "properties": {
                "recordId": 2,
                "recordVersion": 1,
                "producingAgency": 12345,
                "featureIdentificationNumber": 22,
                "featureIdentificationSubdivision": 1,
                "geometryLayerName": "Point2D",
                "geometryRecordId": 0,
                "scaleMaximum": None,
                "scaleMinimum": None,
            },
            "id": 1,
        },
    ),
    (
        "(//DDFField[@name='SPAS']/DDFSubfield[@name='SAUI'])[1]",
        0,
        "Feature type record index 1, SPAS instance 0: SAUI value 0 is invalid",
        "feature_iteration",
        True,
        "FeatureType1_Point2D",
        {
            "type": "Feature",
            "geometry": {"type": "Point", "coordinates": [2.0, 49.0]},
            "properties": {
                "recordId": 2,
                "recordVersion": 1,
                "producingAgency": 12345,
                "featureIdentificationNumber": 22,
                "featureIdentificationSubdivision": 1,
                "geometryLayerName": "Point2D",
                "geometryRecordId": 10,
                "scaleMinimum": None,
                "scaleMaximum": None,
            },
            "id": 1,
        },
    ),
    (
        "(//DDFField[@name='SPAS']/DDFSubfield[@name='ORNT'])[1]",
        2,
        "Feature type record index 1, SPAS instance 0: ORNT = Reverse is invalid for non-curve geometry",
        "feature_iteration",
        True,
        "FeatureType1_Point2D",
        {
            "type": "Feature",
            "geometry": {"type": "Point", "coordinates": [2.0, 49.0]},
            "properties": {
                "recordId": 2,
                "recordVersion": 1,
                "producingAgency": 12345,
                "featureIdentificationNumber": 22,
                "featureIdentificationSubdivision": 1,
                "geometryLayerName": "Point2D",
                "geometryRecordId": 10,
                "scaleMinimum": None,
                "scaleMaximum": None,
            },
            "id": 1,
        },
    ),
    (
        "(//DDFField[@name='SPAS']/DDFSubfield[@name='ORNT'])[1]",
        0,
        "Feature type record index 1, SPAS instance 0: ORNT = 0 is invalid",
        "feature_iteration",
        True,
        "FeatureType1_Point2D",
        {
            "type": "Feature",
            "geometry": {"type": "Point", "coordinates": [2.0, 49.0]},
            "properties": {
                "recordId": 2,
                "recordVersion": 1,
                "producingAgency": 12345,
                "featureIdentificationNumber": 22,
                "featureIdentificationSubdivision": 1,
                "geometryLayerName": "Point2D",
                "geometryRecordId": 10,
                "scaleMinimum": None,
                "scaleMaximum": None,
            },
            "id": 1,
        },
    ),
    (
        "(.//DDFSubfield[@name='XCOO'])[1]",
        1805000000,
        (
            "Record index=0 of PRID: wrong coordinate value: lon=180.500000, lat=49.000000",
            "Record ID=10 of PRID: wrong coordinate value: lon=180.500000, lat=49.000000",
        ),
        "feature_iteration",
        True,
        "FeatureType1_Point2D",
        {
            "type": "Feature",
            "geometry": {"type": "Point", "coordinates": [180.5, 49.0]},
            "properties": {
                "recordId": 2,
                "recordVersion": 1,
                "producingAgency": 12345,
                "featureIdentificationNumber": 22,
                "featureIdentificationSubdivision": 1,
                "geometryLayerName": "Point2D",
                "geometryRecordId": 10,
                "scaleMinimum": None,
                "scaleMaximum": None,
            },
            "id": 1,
        },
    ),
    (
        "(//DDFField[@name='SPAS']/DDFSubfield[@name='RRNM'])[3]",
        100,
        "Record index 2 of FRID: has 2 spatial associations with at least 2 not being of the same geometry type",
        "dataset_opening",
        True,
        None,
        None,
    ),
    (
        "(//DDFField[@name='C2IL']/DDFSubfield[@name='XCOO'])[1]",
        1805000000,
        (
            "Record index=0 of MRID: wrong coordinate value: lon=180.500000, lat=-49.000000",
            None,
        ),
        "feature_iteration",
        True,
        "FeatureType3_MultiPoint2D",
        {
            "type": "Feature",
            "geometry": {
                "type": "MultiPoint",
                "coordinates": [[2.0, 49.0], [2.01, 49.1]],
            },
            "properties": {
                "recordId": 4,
                "recordVersion": 1,
                "producingAgency": 12345,
                "featureIdentificationNumber": 44,
                "featureIdentificationSubdivision": 1,
                "geometryLayerName": ["Point2D", "Point2D"],
                "geometryRecordId": [10, 20],
                "scaleMinimum": None,
                "scaleMaximum": None,
            },
            "id": 1,
        },
    ),
    (
        "(//DDFField[@name='SPAS']/DDFSubfield[@name='RRID'])[9]",
        0,
        "Feature type record index 6, SPAS instance 0: Curve of ID=0 does not exist",
        "feature_iteration",
        True,
        "FeatureType4_Line",
        {
            "type": "Feature",
            "geometry": None,
            "properties": {
                "recordId": 7,
                "recordVersion": 1,
                "producingAgency": 12345,
                "featureIdentificationNumber": 77,
                "featureIdentificationSubdivision": 1,
                "geometryLayerName": "Curve",
                "geometryRecordId": 0,
                "geometryOrientation": "forward",
                "scaleMinimum": 1000,
                "scaleMaximum": 2000,
            },
            "id": 1,
        },
    ),
    (
        "(//DDFField[@name='SPAS']/DDFSubfield[@name='RRID'])[11]",
        0,
        "Feature type record index 8, SPAS instance 0: CompositeCurve of ID=0 does not exist",
        "feature_iteration",
        True,
        "FeatureType4_Line",
        {
            "type": "Feature",
            "geometry": None,
            "properties": {
                "recordId": 9,
                "recordVersion": 1,
                "producingAgency": 12345,
                "featureIdentificationNumber": 99,
                "featureIdentificationSubdivision": 1,
                "geometryLayerName": "CompositeCurve",
                "geometryRecordId": 0,
                "geometryOrientation": "forward",
                "scaleMinimum": 1000,
                "scaleMaximum": 2000,
            },
            "id": 3,
        },
    ),
    (
        "(//DDFField[@name='SPAS']/DDFSubfield[@name='RRID'])[14]",
        0,
        "Feature type record index 10, SPAS instance 0: Surface of ID=0 does not exist",
        "feature_iteration",
        True,
        "FeatureType6_Polygon",
        {
            "type": "Feature",
            "geometry": None,
            "properties": {
                "recordId": 11,
                "recordVersion": 1,
                "producingAgency": 12345,
                "featureIdentificationNumber": 110,
                "featureIdentificationSubdivision": 1,
                "geometryLayerName": "Surface",
                "geometryRecordId": 0,
                "scaleMinimum": 1000,
                "scaleMaximum": 2000,
            },
            "id": 1,
        },
    ),
    (
        "(//DDFSubfield[@name='MUIN'])[1]",
        0,
        "Feature type record index 11, MASK instance 0: MUIN value 0 is invalid",
        "feature_iteration",
        True,
        "FeatureType7_MultiPolygon",
        {
            "type": "Feature",
            "geometry": {
                "type": "MultiPolygon",
                "coordinates": [
                    [
                        [
                            [2.0, 49.0],
                            [2.0, 50.0],
                            [3.0, 50.0],
                            [3.0, 49.0],
                            [2.0, 49.0],
                        ],
                        [
                            [2.01, 49.1],
                            [2.99, 49.1],
                            [2.99, 49.9],
                            [2.01, 49.9],
                            [2.01, 49.1],
                        ],
                    ],
                    [
                        [
                            [2.0, 49.0],
                            [2.0, 50.0],
                            [3.0, 50.0],
                            [3.0, 49.0],
                            [2.0, 49.0],
                        ],
                        [
                            [2.01, 49.1],
                            [2.99, 49.1],
                            [2.99, 49.9],
                            [2.01, 49.9],
                            [2.01, 49.1],
                        ],
                    ],
                ],
            },
            "properties": {
                "recordId": 12,
                "recordVersion": 1,
                "producingAgency": 12345,
                "featureIdentificationNumber": 120,
                "featureIdentificationSubdivision": 1,
                "geometryLayerName": ["Surface", "Surface"],
                "geometryRecordId": [1, 1],
                "scaleMinimum": [1000, 1000],
                "scaleMaximum": [2000, 2000],
                "maskLayerName": ["Curve", "Curve", "CompositeCurve"],
                "maskRecordId": [1, 2, 1],
                "maskIndicator": [
                    "truncatedByDataCoverageLimit",
                    "truncatedByDataCoverageLimit",
                    "suppressPortrayal",
                ],
            },
            "id": 1,
        },
    ),
    (
        "(//DDFSubfield[@name='MIND'])[1]",
        0,
        "Feature type record index 11, MASK instance 0: MIND value 0 is invalid",
        "feature_iteration",
        True,
        "FeatureType7_MultiPolygon",
        {
            "type": "Feature",
            "geometry": {
                "type": "MultiPolygon",
                "coordinates": [
                    [
                        [
                            [2.0, 49.0],
                            [2.0, 50.0],
                            [3.0, 50.0],
                            [3.0, 49.0],
                            [2.0, 49.0],
                        ],
                        [
                            [2.01, 49.1],
                            [2.99, 49.1],
                            [2.99, 49.9],
                            [2.01, 49.9],
                            [2.01, 49.1],
                        ],
                    ],
                    [
                        [
                            [2.0, 49.0],
                            [2.0, 50.0],
                            [3.0, 50.0],
                            [3.0, 49.0],
                            [2.0, 49.0],
                        ],
                        [
                            [2.01, 49.1],
                            [2.99, 49.1],
                            [2.99, 49.9],
                            [2.01, 49.9],
                            [2.01, 49.1],
                        ],
                    ],
                ],
            },
            "properties": {
                "recordId": 12,
                "recordVersion": 1,
                "producingAgency": 12345,
                "featureIdentificationNumber": 120,
                "featureIdentificationSubdivision": 1,
                "geometryLayerName": ["Surface", "Surface"],
                "geometryRecordId": [1, 1],
                "scaleMinimum": [1000, 1000],
                "scaleMaximum": [2000, 2000],
                "maskLayerName": ["Curve", "Curve", "CompositeCurve"],
                "maskRecordId": [1, 2, 1],
                "maskIndicator": [
                    "unknown0",
                    "truncatedByDataCoverageLimit",
                    "suppressPortrayal",
                ],
            },
            "id": 1,
        },
    ),
    (
        "(//DDFField[@name='MASK']/DDFSubfield[@name='RRNM'])[1]",
        0,
        "Feature type record index 11, MASK instance 0: Invalid value for RRNM subfield: got 0, expected 120 or 125",
        "feature_iteration",
        True,
        "FeatureType7_MultiPolygon",
        {
            "type": "Feature",
            "geometry": {
                "type": "MultiPolygon",
                "coordinates": [
                    [
                        [
                            [2.0, 49.0],
                            [2.0, 50.0],
                            [3.0, 50.0],
                            [3.0, 49.0],
                            [2.0, 49.0],
                        ],
                        [
                            [2.01, 49.1],
                            [2.99, 49.1],
                            [2.99, 49.9],
                            [2.01, 49.9],
                            [2.01, 49.1],
                        ],
                    ],
                    [
                        [
                            [2.0, 49.0],
                            [2.0, 50.0],
                            [3.0, 50.0],
                            [3.0, 49.0],
                            [2.0, 49.0],
                        ],
                        [
                            [2.01, 49.1],
                            [2.99, 49.1],
                            [2.99, 49.9],
                            [2.01, 49.9],
                            [2.01, 49.1],
                        ],
                    ],
                ],
            },
            "properties": {
                "recordId": 12,
                "recordVersion": 1,
                "producingAgency": 12345,
                "featureIdentificationNumber": 120,
                "featureIdentificationSubdivision": 1,
                "geometryLayerName": ["Surface", "Surface"],
                "geometryRecordId": [1, 1],
                "scaleMinimum": [1000, 1000],
                "scaleMaximum": [2000, 2000],
                "maskLayerName": ["", "Curve", "CompositeCurve"],
                "maskRecordId": [1, 2, 1],
                "maskIndicator": [
                    "truncatedByDataCoverageLimit",
                    "truncatedByDataCoverageLimit",
                    "suppressPortrayal",
                ],
            },
            "id": 1,
        },
    ),
]


@pytest.mark.parametrize(
    "xpath,value,exception_text,exception_location,recoverable,layer_name,expected_json",
    test_ogr_s101_read_feature_altered_params,
    ids=[f"case{i}" for i in range(len(test_ogr_s101_read_feature_altered_params))],
)
def test_ogr_s101_read_feature_altered(
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
        "data/s101/feature.xml",
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

            if (
                exception_text is not None
                and "invalid sequence of fields" in exception_text
            ):
                exception_text = None

            if (
                exception_text
                and "Feature type record index 8, SPAS instance 0: CompositeCurve of ID=0 does not exist"
                in exception_text
            ):
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


###############################################################################
# Test file with features with multi ATTR, INAS and FACS fields


def test_ogr_s101_read_feature_multi_inas_and_fasc():

    with gdaltest.error_raised(gdal.CE_None):
        ds = ogr.Open("data/s101/feature_multi_inas_and_fasc.000")

    lyr = ds.GetLayerByName("FeatureType1_NoGeom")
    assert lyr.GetSpatialRef() is None
    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": None,
        "properties": {
            "recordId": 1,
            "recordVersion": 1,
            "producingAgency": 12345,
            "featureIdentificationNumber": 11,
            "featureIdentificationSubdivision": 1,
            "text[1]": "my text ATTR",
            "text[2]": "my text ATTR2",
            "infoAssociationRecordId[1]": 1,
            "infoAssociationCode[1]": "SpatialAssociation",
            "infoAssociationRoleCode[1]": "defines",
            "infoAssociationRecordId[2]": 1,
            "infoAssociationCode[2]": "SpatialAssociation",
            "infoAssociationRoleCode[2]": "providesInformation",
            "infoAssociation[1]_text": "my text INAS",
            "infoAssociation[2]_text": "my text INAS2",
            "featureAssociationRefLayerName[1]": "FeatureType1_NoGeom",
            "featureAssociationRefRecordId[1]": 1,
            "featureAssociationCode[1]": "StructureEquipment",
            "featureAssociationRoleCode[1]": "defines",
            "featureAssociationRefLayerName[2]": "FeatureType1_NoGeom",
            "featureAssociationRefRecordId[2]": 1,
            "featureAssociationCode[2]": "StructureEquipment",
            "featureAssociationRoleCode[2]": "providesInformation",
            "featureAssociation[1]_text": "my text FASC",
            "featureAssociation[2]_text": "my text FASC2",
        },
        "id": 1,
    }
