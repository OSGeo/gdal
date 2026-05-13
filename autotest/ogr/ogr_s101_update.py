#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test support for update files in OGR S-101 driver
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_driver("S101")

from .ogr_s101 import _alter_xml


def test_ogr_s101_update_cancellation():

    ds = ogr.Open("data/s101/cancelled.000")
    assert ds.GetLayerCount() == 0
    assert ds.GetMetadata() == {
        "APPLICATION_PROFILE": "2",
        "DATASET_EDITION": "0",
        "DATASET_IDENTIFIER": "file name",
        "DATASET_LANGUAGE": "EN",
        "DATASET_REFERENCE_DATE": "20260401",
        "DATASET_TITLE": "dataset title",
        "ENCODING_SPECIFICATION": "S-100 Part 10a",
        "ENCODING_SPECIFICATION_EDITION": "5.2",
        "PRODUCT_EDITION": "2.0",
        "PRODUCT_IDENTIFIER": "INT.IHO.S-101.2.0",
        "STATUS": "CANCELLED",
    }


def test_ogr_s101_update_updates_ignore():

    ds = gdal.OpenEx("data/s101/cancelled.000", open_options=["UPDATES=IGNORE"])
    assert ds.GetLayerCount() == 1
    assert ds.GetMetadata() == {
        "APPLICATION_PROFILE": "1",
        "DATASET_EDITION": "10.0",
        "DATASET_IDENTIFIER": "file name",
        "DATASET_LANGUAGE": "EN",
        "DATASET_REFERENCE_DATE": "20260401",
        "DATASET_TITLE": "dataset title",
        "ENCODING_SPECIFICATION": "S-100 Part 10a",
        "ENCODING_SPECIFICATION_EDITION": "5.2",
        "PRODUCT_EDITION": "2.0",
        "PRODUCT_IDENTIFIER": "INT.IHO.S-101.2.0",
        "STATUS": "VALID",
    }


def test_ogr_s101_update_information_type():

    ds = ogr.Open("data/s101/information_type_update.000")
    lyr = ds.GetLayer("informationType")
    assert lyr.GetFeatureCount() == 2

    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": None,
        "properties": {
            "recordId": 1,
            "recordVersion": 2,
            "informationType": "SpatialQuality",
            "spatialAccuracy.horizontalPositionUncertainty.uncertaintyFixed": 3.5,
            "lightSector.sectorInformation[1].language": "EN",
            "lightSector.sectorInformation[2].language": "GER",
            "lightSector.sectorInformation[3].language": "FR",
            "lightSector.sectorInformation[1].text": "my text",
            "lightSector.sectorInformation[2].text": "mein Text",
            "lightSector.sectorInformation[3].text": "mon texte",
            "lightSector.colour": [1, 2],
            "infoAssociationRecordId[1]": 1,
            "infoAssociationCode[1]": "SpatialAssociation",
            "infoAssociationRoleCode[1]": "defines",
            "infoAssociationRecordId[2]": 10,
            "infoAssociationCode[2]": "SpatialAssociation",
            "infoAssociationRoleCode[2]": "defines",
            "association[1]_text": "assoc with 1 modified",
            "association[2]_text": "assoc with 10 re-added",
        },
        "id": 1,
    }


def test_ogr_s101_update_point_2d():

    ds = ogr.Open("data/s101/point_2d_update.000")
    assert ds.GetLayerCount() == 1
    assert ds.GetMetadata() == {
        "APPLICATION_PROFILE": "2",
        "DATASET_EDITION": "10.1",
        "DATASET_IDENTIFIER": "file name",
        "DATASET_LANGUAGE": "EN",
        "DATASET_REFERENCE_DATE": "20260401",
        "DATASET_TITLE": "dataset title",
        "ENCODING_SPECIFICATION": "S-100 Part 10a",
        "ENCODING_SPECIFICATION_EDITION": "5.2",
        "PRODUCT_EDITION": "2.0",
        "PRODUCT_IDENTIFIER": "INT.IHO.S-101.2.0",
        "STATUS": "VALID",
    }
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 4

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

    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {"type": "Point", "coordinates": [3.0, 30.0]},
        "properties": {"recordId": 3, "recordVersion": 2},
        "id": 2,
    }

    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {"type": "Point", "coordinates": [4.0, 40.0]},
        "properties": {"recordId": 4, "recordVersion": 1},
        "id": 3,
    }

    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {"type": "Point", "coordinates": [5.0, 50.0]},
        "properties": {"recordId": 5, "recordVersion": 1},
        "id": 4,
    }


def test_ogr_s101_update_point_3d():

    ds = ogr.Open("data/s101/point_3d_update.000")
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1

    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {"type": "Point", "coordinates": [-2.5, -49.5, -15.5]},
        "properties": {"recordId": 1, "recordVersion": 2},
        "id": 1,
    }


def test_ogr_s101_update_multipoint_2d():

    ds = ogr.Open("data/s101/multipoint_2d_update.000")
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1

    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {
            "type": "MultiPoint",
            "coordinates": [[1.0, 11.0], [3.0, -49.0], [-3.0, -48.0], [3.0, 48.0]],
        },
        "properties": {"recordId": 1, "recordVersion": 4},
        "id": 1,
    }


def test_ogr_s101_update_multipoint_3d():

    ds = ogr.Open("data/s101/multipoint_3d_update.000")
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1

    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {
            "type": "MultiPoint",
            "coordinates": [[-2.5, -49.5, -15.5], [3.5, 48.5, -15.5]],
        },
        "properties": {"recordId": 10, "recordVersion": 2},
        "id": 1,
    }


def test_ogr_s101_update_curve():

    ds = ogr.Open("data/s101/curve_update.000")
    lyr = ds.GetLayer("Curve")
    assert lyr.GetFeatureCount() == 1

    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {
            "type": "LineString",
            "coordinates": [[3.0, 50.0], [2.1, 49.1], [2.0, 49.0]],
        },
        "properties": {"recordId": 1, "recordVersion": 2},
        "id": 1,
    }


def test_ogr_s101_update_compositecurve():

    ds = ogr.Open("data/s101/compositecurve_update.000")
    lyr = ds.GetLayer("CompositeCurve")
    assert lyr.GetFeatureCount() == 1

    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {
            "type": "LineString",
            "coordinates": [
                [3.0, 50.0],
                [2.0, 50.0],
                [2.0, 49.0],
                [-3.0, -49.0],
                [3.0, 50.0],
            ],
        },
        "properties": {"recordId": 1, "recordVersion": 4},
        "id": 1,
    }


def test_ogr_s101_update_surface():

    ds = ogr.Open("data/s101/surface_update.000")
    lyr = ds.GetLayer("Surface")
    assert lyr.GetFeatureCount() == 1

    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": {
            "type": "Polygon",
            "coordinates": [
                [[2.0, 49.0], [2.0, 50.0], [3.0, 50.0], [3.0, 49.0], [2.0, 49.0]],
                [[2.01, 49.1], [2.99, 49.05], [2.99, 49.9], [2.01, 49.9], [2.01, 49.1]],
            ],
        },
        "properties": {
            "recordId": 1,
            "recordVersion": 2,
            "infoAssociationRecordId": 1,
            "infoAssociationCode": "SpatialAssociation",
            "infoAssociationRoleCode": "defines",
        },
        "id": 1,
    }


def test_ogr_s101_update_featureType_spas_mask():

    ds = ogr.Open("data/s101/feature_spas_mask_update.000")
    lyr = ds.GetLayer("FeatureType7_MultiPolygon")
    assert lyr.GetFeatureCount() == 1

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
            "recordVersion": 2,
            "producingAgency": 54321,
            "featureIdentificationNumber": 12000,
            "featureIdentificationSubdivision": 100,
            "geometryLayerName": ["Surface", "Surface"],
            "geometryRecordId": [1, 1],
            "scaleMinimum": [1000, 10000],
            "scaleMaximum": [2000, 20000],
            "maskLayerName": "Curve",
            "maskRecordId": 1,
            "maskIndicator": "truncatedByDataCoverageLimit",
        },
        "id": 1,
    }


def test_ogr_s101_update_featureType_attr_inas_fasc():

    ds = ogr.Open("data/s101/feature_update_attr_inas_fasc.000")
    lyr = ds.GetLayer("FeatureType1_NoGeom")
    assert lyr.GetFeatureCount() == 1

    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": None,
        "properties": {
            "recordId": 1,
            "recordVersion": 2,
            "producingAgency": 12345,
            "featureIdentificationNumber": 11,
            "featureIdentificationSubdivision": 1,
            "text": "my text ATTR updated",
            "infoAssociationRecordId": 1,
            "infoAssociationCode": "SpatialAssociation",
            "infoAssociationRoleCode": "defines",
            "infoAssociation_text": "my text INAS updated",
            "featureAssociationRefLayerName": "FeatureType1_NoGeom",
            "featureAssociationRefRecordId": 1,
            "featureAssociationCode": "StructureEquipment",
            "featureAssociationRoleCode": "defines",
            "featureAssociation_text": "my text FASC updated",
        },
        "id": 1,
    }


test_ogr_s101_update_errors_params = [
    (
        "cancelled.001.xml",
        "//DDFSubfield[@name='PROF']",
        "1",
        "has APPLICATION_PROFILE=1",
        "dataset_opening",
        True,
    ),
    (
        "cancelled.001.xml",
        "//DDFSubfield[@name='PROF']",
        "3",
        "APPLICATION_PROFILE='3' is invalid",
        "dataset_opening",
        True,
    ),
    (
        "cancelled.001.xml",
        "//DDFSubfield[@name='DSED']",
        "10.0",
        "has the same DATASET_EDITION value '10.0' than the previous update/initial file",
        "dataset_opening",
        True,
    ),
    (
        "cancelled.001.xml",
        "//DDFSubfield[@name='DSTL']",
        "different",
        "has a different value DSTL='different' than the previous update/initial file",
        "dataset_opening",
        True,
    ),
    (
        "point_2d_update.001.xml",
        "(//DDFSubfield[@name='RCNM'])[1]",
        1,
        "Invalid value for RCNM subfield of DSID",
        "dataset_opening",
        True,
    ),
    (
        "point_2d_update.001.xml",
        "(//DDFSubfield[@name='RCNM'])[2]",
        1,
        "Record index 1: invalid value 1 for RCNM subfield of PRID",
        "dataset_opening",
        True,
    ),
    (
        "point_2d_update.001.xml",
        "(//DDFSubfield[@name='RCID'])[2]",
        -1,
        "Record index 1: invalid value -1 for RCID subfield of PRID",
        "dataset_opening",
        True,
    ),
    (
        "point_2d_update.001.xml",
        "(//DDFField[@name='PRID']//DDFSubfield[@name='RCID'])[2]",
        100,
        "Record index 2, RCNM=110, RCID=100: no such record",
        "dataset_opening",
        True,
    ),
    (
        "point_2d_update.001.xml",
        "(//DDFField[@name='PRID']//DDFSubfield[@name='RVER'])[2]",
        1,
        "Record index 2, RCNM=110, RCID=2: got RVER=1, expected 2",
        "dataset_opening",
        True,
    ),
    (
        "point_2d_update.001.xml",
        "(//DDFField[@name='PRID']//DDFSubfield[@name='RCID'])[1]",
        1,
        "Record index 1: several PRID records have RCID = 1",
        "dataset_opening",
        True,
    ),
    (
        "point_2d_update.001.xml",
        "(//DDFField[@name='PRID']//DDFSubfield[@name='RUIN'])[1]",
        4,
        "Record index 1, RCNM=110, RCID=4: wrong value 4 for RUIN subfield of PRID field",
        "dataset_opening",
        True,
    ),
    (
        "information_type_update.001.xml",
        "(//DDFSubfield[@name='NITC'])[1]",
        0,
        "RCNM=150, RCID=1: unknown NITC=0",
        "dataset_opening",
        True,
    ),
    (
        "feature_spas_mask_update.001.xml",
        "(//DDFSubfield[@name='NFTC'])[1]",
        0,
        "RCNM=100, RCID=12: unknown NFTC=0",
        "dataset_opening",
        True,
    ),
    (
        "information_type_update.001.xml",
        "(//DDFSubfield[@name='NATC'])[1]",
        0,
        "RCNM=150, RCID=1, ATTR[iField=0, iSubField=0]: unknown NATC=0",
        "dataset_opening",
        True,
    ),
    (
        "feature_update_attr_inas_fasc.001.xml",
        "(//DDFField[@name='INAS']//DDFSubfield[@name='NATC'])[1]",
        0,
        "RCNM=100, RCID=1, INAS[iField=0, iSubField=0]: unknown NATC=0",
        "dataset_opening",
        True,
    ),
    (
        "feature_update_attr_inas_fasc.001.xml",
        "(//DDFField[@name='INAS']//DDFSubfield[@name='NARC'])[1]",
        0,
        "RCNM=100, RCID=1, INAS[iField=0, iSubField=0]: unknown NARC=0",
        "dataset_opening",
        True,
    ),
    (
        "feature_update_attr_inas_fasc.001.xml",
        "(//DDFField[@name='FASC']//DDFSubfield[@name='NFAC'])[1]",
        0,
        "RCNM=100, RCID=1, FASC[iField=0, iSubField=0]: unknown NFAC=0",
        "dataset_opening",
        True,
    ),
    (
        "feature_update_attr_inas_fasc.001.xml",
        "(//DDFField[@name='FASC']//DDFSubfield[@name='NARC'])[1]",
        0,
        "RCNM=100, RCID=1, FASC[iField=0, iSubField=0]: unknown NARC=0",
        "dataset_opening",
        True,
    ),
    (
        "multipoint_2d_update.001.xml",
        "(//DDFField[@name='COCC']//DDFSubfield[@name='COUI'])[1]",
        0,
        "RCNM=115, RCID=1: invalid COUI = 0",
        "dataset_opening",
        True,
    ),
    (
        "multipoint_2d_update.001.xml",
        "(//DDFField[@name='COCC']//DDFSubfield[@name='COIX'])[1]",
        0,
        "RCNM=115, RCID=1: invalid COIX = 0",
        "dataset_opening",
        True,
    ),
    (
        "multipoint_2d_update.001.xml",
        "(//DDFField[@name='COCC']//DDFSubfield[@name='COIX'])[1]",
        4,
        "RCNM=115, RCID=1: invalid COIX = 4. Must be in [1,3]",
        "dataset_opening",
        True,
    ),
    (
        "multipoint_2d_update.001.xml",
        "(//DDFField[@name='COCC']//DDFSubfield[@name='NCOR'])[1]",
        1,
        "RCNM=115, RCID=1: invalid NCOR = 1. Expected 3",
        "dataset_opening",
        True,
    ),
    (
        "multipoint_2d_update.001.xml",
        "(//DDFField[@name='COCC']//DDFSubfield[@name='COUI'])[1]",
        2,
        "RCNM=115, RCID=1: unexpected C2IL field in update record in COUI = 2 (delete) mode",
        "dataset_opening",
        True,
    ),
    (
        "multipoint_2d_update.001.xml",
        "(//DDFField[@name='COCC']//DDFSubfield[@name='NCOR'])[2]",
        5,
        "RCNM=115, RCID=1: invalid COIX = 2 and/or NCOR=5",
        "dataset_opening",
        True,
    ),
    (
        "curve_update.001.xml",
        "(//DDFField[@name='SECC']//DDFSubfield[@name='SEUI'])[1]",
        0,
        "RCNM=120, RCID=1: invalid SEUI = 0",
        "dataset_opening",
        True,
    ),
    (
        "curve_update.001.xml",
        "(//DDFField[@name='SECC']//DDFSubfield[@name='SEUI'])[1]",
        1,
        "RCNM=120, RCID=1: SEUI=1 (insert) not supported",
        "dataset_opening",
        True,
    ),
    (
        "curve_update.001.xml",
        "(//DDFField[@name='SECC']//DDFSubfield[@name='SEUI'])[1]",
        2,
        "RCNM=120, RCID=1: SEUI=2 (delete) not supported",
        "dataset_opening",
        True,
    ),
    (
        "curve_update.001.xml",
        "(//DDFField[@name='SECC']//DDFSubfield[@name='SEIX'])[1]",
        0,
        "RCNM=120, RCID=1: invalid SEIX = 0",
        "dataset_opening",
        True,
    ),
    (
        "curve_update.001.xml",
        "(//DDFField[@name='SECC']//DDFSubfield[@name='NSEG'])[1]",
        0,
        "RCNM=120, RCID=1: invalid NSEG = 0",
        "dataset_opening",
        True,
    ),
    (
        "curve_update.001.xml",
        "(//DDFField[@name='C2IL'])[1]",
        None,
        "RCNM=120, RCID=1: missing C2IL field in update record",
        "dataset_opening",
        True,
    ),
    (
        "compositecurve_update.001.xml",
        "(//DDFField[@name='CCOC']//DDFSubfield[@name='CCUI'])[1]",
        0,
        "RCNM=125, RCID=1: invalid CCUI = 0",
        "dataset_opening",
        True,
    ),
    (
        "compositecurve_update.001.xml",
        "(//DDFField[@name='CCOC']//DDFSubfield[@name='CCIX'])[1]",
        0,
        "RCNM=125, RCID=1: invalid CCIX = 0",
        "dataset_opening",
        True,
    ),
    (
        "compositecurve_update.001.xml",
        "(//DDFField[@name='CCOC']//DDFSubfield[@name='NCCO'])[1]",
        0,
        "RCNM=125, RCID=1: invalid NCCO = 0",
        "dataset_opening",
        True,
    ),
    (
        "compositecurve_update.001.xml",
        "(//DDFField[@name='CCOC'])[1]",
        """<DDFField name="CCOC">
            <DDFSubfield name="CCUI" type="integer">2</DDFSubfield> <!-- delete -->
            <DDFSubfield name="CCIX" type="integer">1</DDFSubfield>
            <DDFSubfield name="NCCO" type="integer">1</DDFSubfield>
          </DDFField>""",
        "RCNM=125, RCID=1: unexpected CUCO field in update record in CCUI = 2 (delete) mode",
        "dataset_opening",
        True,
    ),
    (
        "compositecurve_update.001.xml",
        "(//DDFField[@name='CCOC']//DDFSubfield[@name='NCCO'])[2]",
        4,
        "RCNM=125, RCID=1: invalid CCIX = 1 and/or NCCO=4",
        "dataset_opening",
        True,
    ),
    (
        "surface_update.001.xml",
        "(//DDFField[@name='RIAS']//DDFSubfield[@name='RRNM'])[1]",
        0,
        "RCNM=130, RCID=1, RIAS iSubField=0: update field references RRNM=0, RRID=1 which does not exist in initial or previous update",
        "dataset_opening",
        True,
    ),
    (
        "surface_update.001.xml",
        "(//DDFField[@name='RIAS']//DDFSubfield[@name='RAUI'])[1]",
        3,
        "RCNM=130, RCID=1, SPAS iSubField=0: invalid RAUI=3",
        "dataset_opening",
        True,
    ),
    (
        "feature_spas_mask_update.001.xml",
        "(//DDFField[@name='SPAS']//DDFSubfield[@name='RRNM'])[1]",
        0,
        "RCNM=100, RCID=12, SPAS iSubField=0: update field references RRNM=0, RRID=1 which does not exist in initial or previous update",
        "dataset_opening",
        True,
    ),
    (
        "feature_spas_mask_update.001.xml",
        "(//DDFField[@name='SPAS']//DDFSubfield[@name='SAUI'])[1]",
        0,
        "RCNM=100, RCID=12, SPAS iSubField=0: invalid SAUI=0",
        "dataset_opening",
        True,
    ),
    (
        "feature_spas_mask_update.001.xml",
        "(//DDFField[@name='MASK']//DDFSubfield[@name='RRNM'])[1]",
        0,
        "RCNM=100, RCID=12, MASK iSubField=0: update field references RRNM=0, RRID=1 which does not exist in initial or previous update",
        "dataset_opening",
        True,
    ),
    (
        "feature_spas_mask_update.001.xml",
        "(//DDFField[@name='MASK']//DDFSubfield[@name='MUIN'])[1]",
        0,
        "RCNM=100, RCID=12, MASK iSubField=0: invalid MUIN=0",
        "dataset_opening",
        True,
    ),
    (
        "information_type_update.001.xml",
        "(//DDFField[@name='ATTR']//DDFSubfield[@name='ATIN'])[1]",
        0,
        "RCNM=150, RCID=1, ATTR field, instance 0, entry 0: invalid ATIN=0",
        "dataset_opening",
        True,
    ),
    (
        "information_type_update.001.xml",
        "(//DDFField[@name='ATTR']//DDFSubfield[@name='NATC'])[8]",
        7,
        "RCNM=150, RCID=1, ATTR field, instance 0: entry 7 refers to (NATC,ATIX,PAIX)=(7,1,6) already encountered",
        "dataset_opening",
        True,
    ),
    (
        "information_type_update.001.xml",
        "(//DDFField[@name='ATTR']//DDFSubfield[@name='PAIX'])[1]",
        2,
        " RCNM=150, RCID=1, ATTR field, instance 0: entry 0 refers to a PAIX=2 that does not exist",
        "dataset_opening",
        True,
    ),
    (
        "information_type_update.001.xml",
        "(//DDFField[@name='ATTR']//DDFSubfield[@name='NATC'])[3]",
        4,
        "RCNM=150, RCID=1, ATTR field, instance 0: entry 2 references unexisting entry (NATC, ATIX)=(4,1)",
        "dataset_opening",
        True,
    ),
    (
        "information_type_update.001.xml",
        "(//DDFField[@name='INAS']//DDFSubfield[@name='RRNM'])[1]",
        0,
        "RCNM=150, RCID=1, INAS field, 0 instance: found no matching (RRNM,RRID)=(0,1) to update",
        "dataset_opening",
        True,
    ),
    (
        "information_type_update.001.xml",
        "(//DDFField[@name='INAS']//DDFSubfield[@name='IUIN'])[1]",
        0,
        "RCNM=150, RCID=1, INAS field, 0 instance: invalid instruction = 0",
        "dataset_opening",
        True,
    ),
]


@pytest.mark.parametrize(
    "filename,xpath,value,exception_text,exception_location,recoverable",
    test_ogr_s101_update_errors_params,
    ids=[f"case{i}" for i in range(len(test_ogr_s101_update_errors_params))],
)
def test_ogr_s101_update_errors(
    tmp_path,
    filename,
    xpath,
    value,
    exception_text,
    exception_location,
    recoverable,
):
    _alter_xml(
        __file__,
        "data/s101/" + filename,
        tmp_path,
        xpath,
        value,
        exception_text,
        recoverable,
        exception_location=exception_location,
    )


test_ogr_s101_update_errors_bis_params = [
    (
        "point_2d_update_with_3d.000",
        "RCNM=110, RCID=1: cannot find C3IT field in target record",
    ),
    (
        "point_3d_update_with_2d.000",
        "Cannot find field definition C2IT in target module",
    ),
]


@pytest.mark.parametrize(
    "filename,exception_text",
    test_ogr_s101_update_errors_bis_params,
    ids=[f"case{i}" for i in range(len(test_ogr_s101_update_errors_bis_params))],
)
def test_ogr_s101_update_errors_bis(filename, exception_text):

    with pytest.raises(Exception, match=exception_text):
        ogr.Open("data/s101/invalid/" + filename)
