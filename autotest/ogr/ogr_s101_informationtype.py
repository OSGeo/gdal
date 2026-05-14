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
# Test file with InformationType record


def test_ogr_s101_read_information_type():

    with gdaltest.error_raised(gdal.CE_None):
        ds = ogr.Open("data/s101/information_type.000")

    lyr = ds.GetLayerByName("informationType")
    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": None,
        "properties": {
            "recordId": 1,
            "recordVersion": 1,
            "informationType": "SpatialQuality",
            "spatialAccuracy.horizontalPositionUncertainty.uncertaintyFixed": 4.5,
            "spatialAccuracy.verticalUncertainty.uncertaintyFixed": 5.5,
            "lightSector.sectorInformation[1].language": "EN",
            "lightSector.sectorInformation[2].language": "FR",
            "lightSector.sectorInformation[1].text": "my text",
            "lightSector.sectorInformation[2].text": "mon texte",
            "lightSector.colour": [1, 2],
        },
        "id": 1,
    }

    feat_defn = lyr.GetLayerDefn()
    idx = feat_defn.GetFieldIndex("lightSector.colour")

    if not ds.TestCapability("HasFeatureCatalog"):
        pytest.skip("feature catalog not available")

    assert feat_defn.GetFieldDefn(idx).GetDomainName() == "colour"

    fld_domain = ds.GetFieldDomain("colour")
    assert fld_domain.GetDomainType() == ogr.OFDT_CODED
    assert fld_domain.GetEnumeration() == {
        "1": "White",
        "2": "Black",
        "3": "Red",
        "4": "Green",
        "5": "Blue",
        "6": "Yellow",
        "7": "Grey",
        "8": "Brown",
        "9": "Amber",
        "10": "Violet",
        "11": "Orange",
        "12": "Magenta",
        "13": "Pink",
    }


###############################################################################
# Test file with InformationType record with multiple instances of ATTR


def test_ogr_s101_read_information_type_multi_attr():

    with gdaltest.error_raised(gdal.CE_None):
        ds = ogr.Open("data/s101/information_type_multi_attr.000")

    lyr = ds.GetLayerByName("informationType")
    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": None,
        "properties": {
            "recordId": 1,
            "recordVersion": 1,
            "informationType": "SpatialQuality",
            "spatialAccuracy[1].horizontalPositionUncertainty.uncertaintyFixed": 4.5,
            "spatialAccuracy[2].horizontalPositionUncertainty.uncertaintyFixed": 45,
            "spatialAccuracy[1].verticalUncertainty.uncertaintyFixed": 5.5,
            "spatialAccuracy[2].verticalUncertainty.uncertaintyFixed": 55,
            "lightSector[1].sectorInformation[1].language": "EN",
            "lightSector[1].sectorInformation[2].language": "FR",
            "lightSector[1].sectorInformation[1].text": "my text",
            "lightSector[1].sectorInformation[2].text": "mon texte",
            "lightSector[1].colour": [1, 2],
        },
        "id": 1,
    }


###############################################################################
# Test file with InformationType record and INAS field


def test_ogr_s101_read_information_type_with_inas():

    with gdaltest.error_raised(gdal.CE_None):
        ds = ogr.Open("data/s101/information_type_with_inas.000")

    lyr = ds.GetLayerByName("informationType")

    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True)["properties"] == {
        "recordId": 1,
        "recordVersion": 1,
        "informationType": "SpatialQuality",
        "text": "ATTR only",
        "infoAssociationRecordId": None,
        "infoAssociationCode": None,
        "infoAssociationRoleCode": None,
        "association_text": None,
    }

    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True)["properties"] == {
        "recordId": 2,
        "recordVersion": 1,
        "informationType": "SpatialQuality",
        "text": None,
        "infoAssociationRecordId": 1,
        "infoAssociationCode": "SpatialAssociation",
        "infoAssociationRoleCode": "defines",
        "association_text": None,
    }

    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True)["properties"] == {
        "recordId": 3,
        "recordVersion": 1,
        "informationType": "SpatialQuality",
        "text": None,
        "infoAssociationRecordId": 1,
        "infoAssociationCode": "SpatialAssociation",
        "infoAssociationRoleCode": "defines",
        "association_text": "INAS only",
    }

    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True)["properties"] == {
        "recordId": 4,
        "recordVersion": 1,
        "informationType": "SpatialQuality",
        "text": "ATTR",
        "infoAssociationRecordId": 1,
        "infoAssociationCode": "SpatialAssociation",
        "infoAssociationRoleCode": "defines",
        "association_text": "INAS",
    }


###############################################################################
# Test alterations of InformationType record

test_ogr_s101_read_information_type_altered_params = [
    (
        ".//DDFField[@name='IRID']//DDFSubfield[@name='RCID']",
        0,
        "Record index 2: invalid value 0 for RCID subfield of IRID",
        True,
        None,
    ),
    (
        ".//DDFField[@name='IRID']//DDFSubfield[@name='NITC']",
        2,
        (None, "Unknown value 2 for NITC subfield of IRID field"),
        True,
        {
            "recordId": 1,
            "recordVersion": 1,
            "informationType": "informationTypeCode2",
            "spatialAccuracy.horizontalPositionUncertainty.uncertaintyFixed": 4.5,
            "spatialAccuracy.verticalUncertainty.uncertaintyFixed": 5.5,
            "lightSector.sectorInformation[1].language": "EN",
            "lightSector.sectorInformation[2].language": "FR",
            "lightSector.sectorInformation[1].text": "my text",
            "lightSector.sectorInformation[2].text": "mon texte",
            "lightSector.colour": [1, 2],
        },
    ),
    (
        ".//DDFField[@name='ATCS']",
        None,
        "Record index=0 of IRID, ATTR field, attribute idx=0: cannot find attribute code 1 in ATCS field of the Dataset General Information Record",
        True,
        {
            "recordId": 1,
            "recordVersion": 1,
            "informationType": "SpatialQuality",
            "code_1.code_2.code_3": 4.5,
            "code_1.code_4.code_3": 5.5,
            "code_5.code_6[1].code_7": "EN",
            "code_5.code_6[2].code_7": "FR",
            "code_5.code_6[1].code_8": "my text",
            "code_5.code_6[2].code_8": "mon texte",
            "code_5.code_9": [1, 2],
        },
    ),
    (
        ".//DDFSubfield[@name='ATVL'][3]",
        # Replace 4.5 by integer 4
        4,
        None,
        True,
        {
            "recordId": 1,
            "recordVersion": 1,
            "informationType": "SpatialQuality",
            "spatialAccuracy.horizontalPositionUncertainty.uncertaintyFixed": 4,
            "spatialAccuracy.verticalUncertainty.uncertaintyFixed": 5.5,
            "lightSector.sectorInformation[1].language": "EN",
            "lightSector.sectorInformation[2].language": "FR",
            "lightSector.sectorInformation[1].text": "my text",
            "lightSector.sectorInformation[2].text": "mon texte",
            "lightSector.colour": [1, 2],
        },
    ),
    (
        ".//DDFField[@name='ATTR']",
        """<DDFField name="ATTR">
                <DDFSubfield name="NATC" type="integer">1</DDFSubfield>  <!-- spatialAccuracy -->
                <DDFSubfield name="ATIX" type="integer">1</DDFSubfield>
                <DDFSubfield name="PAIX" type="integer">0</DDFSubfield>
                <DDFSubfield name="ATIN" type="integer">1</DDFSubfield>
                <DDFSubfield name="ATVL" type="string"></DDFSubfield>
           </DDFField>""",
        None,
        True,
        {
            "informationType": "SpatialQuality",
            "recordId": 1,
            "recordVersion": 1,
            "spatialAccuracy": "",
        },
    ),
    (
        ".//DDFField[@name='ATTR']",
        """<DDFField name="ATTR">
                <DDFSubfield name="NATC" type="integer">1</DDFSubfield>  <!-- spatialAccuracy -->
                <DDFSubfield name="ATIX" type="integer">2</DDFSubfield>
                <DDFSubfield name="PAIX" type="integer">0</DDFSubfield>
                <DDFSubfield name="ATIN" type="integer">1</DDFSubfield>
                <DDFSubfield name="ATVL" type="string"></DDFSubfield>
           </DDFField>""",
        "Record index=0 of IRID, ATTR field, attribute idx=0: wrong value 2 for ATIX subfield. Must be in [1, 1]",
        True,
        {"informationType": "SpatialQuality", "recordId": 1, "recordVersion": 1},
    ),
    (
        ".//DDFField[@name='ATTR']",
        """<DDFField name="ATTR">
                <DDFSubfield name="NATC" type="integer">1</DDFSubfield>  <!-- spatialAccuracy -->
                <DDFSubfield name="ATIX" type="integer">1</DDFSubfield>
                <DDFSubfield name="PAIX" type="integer">0</DDFSubfield>
                <DDFSubfield name="ATIN" type="integer">1</DDFSubfield>
                <DDFSubfield name="ATVL" type="string"></DDFSubfield>

                <DDFSubfield name="NATC" type="integer">1</DDFSubfield>  <!-- spatialAccuracy -->
                <DDFSubfield name="ATIX" type="integer">1</DDFSubfield>
                <DDFSubfield name="PAIX" type="integer">0</DDFSubfield>
                <DDFSubfield name="ATIN" type="integer">1</DDFSubfield>
                <DDFSubfield name="ATVL" type="string"></DDFSubfield>
           </DDFField>""",
        "Record index=0 of IRID, ATTR field, attribute idx=1: wrong value 1 for ATIX subfield. Expected 2",
        True,
        {
            "informationType": "SpatialQuality",
            "recordId": 1,
            "recordVersion": 1,
            "spatialAccuracy": "",
        },
    ),
    (
        ".//DDFField[@name='ATTR']",
        """<DDFField name="ATTR">
                <DDFSubfield name="NATC" type="integer">1</DDFSubfield>  <!-- spatialAccuracy -->
                <DDFSubfield name="ATIX" type="integer">1</DDFSubfield>
                <DDFSubfield name="PAIX" type="integer">0</DDFSubfield>
                <DDFSubfield name="ATIN" type="integer">1</DDFSubfield>
                <DDFSubfield name="ATVL" type="string"></DDFSubfield>

                <DDFSubfield name="NATC" type="integer">2</DDFSubfield>  <!-- horizontalPositionUncertainty -->
                <DDFSubfield name="ATIX" type="integer">2</DDFSubfield>
                <DDFSubfield name="PAIX" type="integer">0</DDFSubfield>
                <DDFSubfield name="ATIN" type="integer">1</DDFSubfield>
                <DDFSubfield name="ATVL" type="string"></DDFSubfield>
           </DDFField>""",
        "Record index=0 of IRID, ATTR field, attribute idx=1: wrong value 2 for ATIX subfield. Expected 1",
        True,
        {
            "informationType": "SpatialQuality",
            "recordId": 1,
            "recordVersion": 1,
            "spatialAccuracy": "",
            "horizontalPositionUncertainty": "",
        },
    ),
    (
        ".//DDFField[@name='ATTR']",
        """<DDFField name="ATTR">
                <DDFSubfield name="NATC" type="integer">1</DDFSubfield>  <!-- spatialAccuracy -->
                <DDFSubfield name="ATIX" type="integer">1</DDFSubfield>
                <DDFSubfield name="PAIX" type="integer">0</DDFSubfield>
                <DDFSubfield name="ATIN" type="integer">1</DDFSubfield>
                <DDFSubfield name="ATVL" type="string"></DDFSubfield>

                <DDFSubfield name="NATC" type="integer">2</DDFSubfield>  <!-- horizontalPositionUncertainty -->
                <DDFSubfield name="ATIX" type="integer">1</DDFSubfield>
                <DDFSubfield name="PAIX" type="integer">0</DDFSubfield>
                <DDFSubfield name="ATIN" type="integer">1</DDFSubfield>
                <DDFSubfield name="ATVL" type="string"></DDFSubfield>

                <DDFSubfield name="NATC" type="integer">1</DDFSubfield>  <!-- spatialAccuracy -->
                <DDFSubfield name="ATIX" type="integer">1</DDFSubfield>
                <DDFSubfield name="PAIX" type="integer">0</DDFSubfield>
                <DDFSubfield name="ATIN" type="integer">1</DDFSubfield>
                <DDFSubfield name="ATVL" type="string"></DDFSubfield>
           </DDFField>""",
        "Record index=0 of IRID, ATTR field, attribute idx=2: several instances of (NATC,ATIX,PAIX)=(1,1,0) in field ATTR of the same record",
        True,
        {
            "informationType": "SpatialQuality",
            "recordId": 1,
            "recordVersion": 1,
            "spatialAccuracy": "",
            "horizontalPositionUncertainty": "",
        },
    ),
    (
        ".//DDFField[@name='ATTR']",
        """<DDFField name="ATTR">
                <DDFSubfield name="NATC" type="integer">1</DDFSubfield>  <!-- spatialAccuracy -->
                <DDFSubfield name="ATIX" type="integer">1</DDFSubfield>
                <DDFSubfield name="PAIX" type="integer">1</DDFSubfield>
                <DDFSubfield name="ATIN" type="integer">1</DDFSubfield>
                <DDFSubfield name="ATVL" type="string"></DDFSubfield>
           </DDFField>""",
        "Record index=0 of IRID, ATTR field, attribute idx=0: wrong value 1 for PAIX subfield. Must be in [0, 0]",
        True,
        {"informationType": "SpatialQuality", "recordId": 1, "recordVersion": 1},
    ),
    (
        ".//DDFField[@name='ATTR']",
        """<DDFField name="ATTR">
                <DDFSubfield name="NATC" type="integer">1</DDFSubfield>  <!-- spatialAccuracy -->
                <DDFSubfield name="ATIX" type="integer">1</DDFSubfield>
                <DDFSubfield name="PAIX" type="integer">0</DDFSubfield>
                <DDFSubfield name="ATIN" type="integer">0</DDFSubfield>
                <DDFSubfield name="ATVL" type="string"></DDFSubfield>
           </DDFField>""",
        "Record index=0 of IRID, ATTR field, attribute idx=0: wrong value 0 for ATIN subfield",
        True,
        {"informationType": "SpatialQuality", "recordId": 1, "recordVersion": 1},
    ),
    (
        ".//DDFField[@name='ATTR']",
        """<DDFField name="ATTR">
                <DDFSubfield name="NATC" type="integer">1</DDFSubfield>  <!-- spatialAccuracy -->
                <DDFSubfield name="ATIX" type="integer">1</DDFSubfield>
                <DDFSubfield name="PAIX" type="integer">0</DDFSubfield>
                <DDFSubfield name="ATIN" type="integer">1</DDFSubfield>
                <DDFSubfield name="ATVL" type="string">non empty</DDFSubfield>

                    <DDFSubfield name="NATC" type="integer">2</DDFSubfield>  <!-- horizontalPositionUncertainty -->
                    <DDFSubfield name="ATIX" type="integer">1</DDFSubfield>
                    <DDFSubfield name="PAIX" type="integer">1</DDFSubfield>
                    <DDFSubfield name="ATIN" type="integer">1</DDFSubfield>
                    <DDFSubfield name="ATVL" type="string"></DDFSubfield>

           </DDFField>""",
        "Record index=0 of IRID, ATTR field, attribute idx=1: parent attribute of index PAIX=1 has a non empty ATVL subfield",
        True,
        {
            "recordId": 1,
            "recordVersion": 1,
            "informationType": "SpatialQuality",
            "spatialAccuracy.horizontalPositionUncertainty": "",
        },
    ),
    (
        ".//DDFField[@name='ATTR']",
        """<DDFField name="ATTR">
                <DDFSubfield name="NATC" type="integer">3</DDFSubfield>  <!-- uncertaintyFixed -->
                <DDFSubfield name="ATIX" type="integer">1</DDFSubfield>
                <DDFSubfield name="PAIX" type="integer">0</DDFSubfield>
                <DDFSubfield name="ATIN" type="integer">1</DDFSubfield>
                <DDFSubfield name="ATVL" type="string">4</DDFSubfield>

                <DDFSubfield name="NATC" type="integer">3</DDFSubfield>  <!-- uncertaintyFixed -->
                <DDFSubfield name="ATIX" type="integer">2</DDFSubfield>
                <DDFSubfield name="PAIX" type="integer">0</DDFSubfield>
                <DDFSubfield name="ATIN" type="integer">1</DDFSubfield>
                <DDFSubfield name="ATVL" type="string">5.5</DDFSubfield>
           </DDFField>""",
        None,
        True,
        {
            "recordId": 1,
            "recordVersion": 1,
            "informationType": "SpatialQuality",
            "uncertaintyFixed": [4.0, 5.5],
        },
    ),
    (
        ".//DDFField[@name='ATTR']",
        """<DDFField name="ATTR">
                <DDFSubfield name="NATC" type="integer">3</DDFSubfield>  <!-- uncertaintyFixed -->
                <DDFSubfield name="ATIX" type="integer">1</DDFSubfield>
                <DDFSubfield name="PAIX" type="integer">0</DDFSubfield>
                <DDFSubfield name="ATIN" type="integer">1</DDFSubfield>
                <DDFSubfield name="ATVL" type="string">4</DDFSubfield>

                <DDFSubfield name="NATC" type="integer">3</DDFSubfield>  <!-- uncertaintyFixed -->
                <DDFSubfield name="ATIX" type="integer">2</DDFSubfield>
                <DDFSubfield name="PAIX" type="integer">0</DDFSubfield>
                <DDFSubfield name="ATIN" type="integer">1</DDFSubfield>
                <DDFSubfield name="ATVL" type="string">5</DDFSubfield>
           </DDFField>""",
        None,
        True,
        {
            "recordId": 1,
            "recordVersion": 1,
            "informationType": "SpatialQuality",
            "uncertaintyFixed": [4, 5],
        },
    ),
    (
        ".//DDFField[@name='ATTR']",
        """<DDFField name="ATTR">
                <DDFSubfield name="NATC" type="integer">3</DDFSubfield>  <!-- uncertaintyFixed -->
                <DDFSubfield name="ATIX" type="integer">1</DDFSubfield>
                <DDFSubfield name="PAIX" type="integer">0</DDFSubfield>
                <DDFSubfield name="ATIN" type="integer">1</DDFSubfield>
                <DDFSubfield name="ATVL" type="string">123456789123</DDFSubfield>
           </DDFField>""",
        (
            None,
            "Record index=0 of IRID, attribute uncertaintyFixed: non integer value '123456789123'",
        ),
        True,
        {
            "recordId": 1,
            "recordVersion": 1,
            "informationType": "SpatialQuality",
            "uncertaintyFixed": None,
        },
    ),
    (
        ".//DDFField[@name='ATTR']",
        """<DDFField name="ATTR">
                <DDFSubfield name="NATC" type="integer">3</DDFSubfield>  <!-- uncertaintyFixed -->
                <DDFSubfield name="ATIX" type="integer">1</DDFSubfield>
                <DDFSubfield name="PAIX" type="integer">0</DDFSubfield>
                <DDFSubfield name="ATIN" type="integer">1</DDFSubfield>
                <DDFSubfield name="ATVL" type="string">1d300</DDFSubfield>
           </DDFField>""",
        (
            None,
            "Record index=0 of IRID, attribute uncertaintyFixed: non double value '1d300'",
        ),
        True,
        {
            "recordId": 1,
            "recordVersion": 1,
            "informationType": "SpatialQuality",
            "uncertaintyFixed": None,
        },
    ),
    (
        ".//DDFField[@name='ATTR']",
        """<DDFField name="ATTR">
                <DDFSubfield name="NATC" type="integer">8</DDFSubfield>  <!-- text -->
                <DDFSubfield name="ATIX" type="integer">1</DDFSubfield>
                <DDFSubfield name="PAIX" type="integer">0</DDFSubfield>
                <DDFSubfield name="ATIN" type="integer">1</DDFSubfield>
                <DDFSubfield name="ATVL" type="string">xPLACEHOLDER_FFx</DDFSubfield>
           </DDFField>""",
        (
            None,
            b"Record index=0 of IRID, attribute text: non UTF-8 string 'xPLACEHOLDER_\xff\xffx'",
        ),
        True,
        {
            "recordId": 1,
            "recordVersion": 1,
            "informationType": "SpatialQuality",
            "text": "xPLACEHOLDER___x",
        },
    ),
]


@pytest.mark.parametrize(
    "xpath,value,exception_text,recoverable,expected_json",
    test_ogr_s101_read_information_type_altered_params,
    ids=[
        f"case{i}"
        for i in range(len(test_ogr_s101_read_information_type_altered_params))
    ],
)
def test_ogr_s101_read_information_type_altered(
    tmp_path, xpath, value, exception_text, recoverable, expected_json
):
    ds = _alter_xml(
        __file__,
        "data/s101/information_type.xml",
        tmp_path,
        xpath,
        value,
        exception_text,
        recoverable,
    )
    if ds:
        lyr = ds.GetLayerByName("informationType")
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
                assert f.ExportToJson(as_object=True)["properties"] == expected_json
            else:
                assert expected_json is None
    else:
        assert expected_json is None


def test_ogr_s101_read_information_type_alter_enumerated_value(tmp_path):

    ds = ogr.Open("data/s101/information_type.000")
    if not ds.TestCapability("HasFeatureCatalog"):
        pytest.skip("feature catalog not available")

    ds = _alter_xml(
        __file__,
        "data/s101/information_type.xml",
        tmp_path,
        ".//DDFSubfield[@name='ATVL'][14]",
        987654,
        "Record index=0 of IRID: value 987654 does not belong to enumeration of attribute code colour",
        True,
    )
    lyr = ds.GetLayerByName("informationType")
    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True)["properties"] == {
        "recordId": 1,
        "recordVersion": 1,
        "informationType": "SpatialQuality",
        "spatialAccuracy.horizontalPositionUncertainty.uncertaintyFixed": 4.5,
        "spatialAccuracy.verticalUncertainty.uncertaintyFixed": 5.5,
        "lightSector.sectorInformation[1].language": "EN",
        "lightSector.sectorInformation[2].language": "FR",
        "lightSector.sectorInformation[1].text": "my text",
        "lightSector.sectorInformation[2].text": "mon texte",
        "lightSector.colour": [1, 987654],
    }


###############################################################################
# Test Date field


def test_ogr_s101_read_information_type_date_field():

    with gdaltest.error_raised(gdal.CE_None):
        ds = ogr.Open("data/s101/information_type_date.000")

    lyr = ds.GetLayerByName("informationType")
    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": None,
        "properties": {
            "recordId": 1,
            "recordVersion": 1,
            "informationType": "ContactDetails",
            "fixedDateRange.dateStart": "2026/01/01",
            "fixedDateRange.dateEnd": "2026/12/31",
        },
        "id": 1,
    }


###############################################################################
# Test Time field


def test_ogr_s101_read_information_type_time_field():

    with gdaltest.error_raised(gdal.CE_None):
        ds = ogr.Open("data/s101/information_type_time.000")

    lyr = ds.GetLayerByName("informationType")
    f = lyr.GetNextFeature()
    assert f.ExportToJson(as_object=True) == {
        "type": "Feature",
        "geometry": None,
        "properties": {
            "recordId": 1,
            "recordVersion": 1,
            "informationType": "ServiceHours",
            "fixedDateRange.timeIntervalsByDayOfWeek.timeOfDayStart": "12:34:56",
            "fixedDateRange.timeIntervalsByDayOfWeek.timeOfDayEnd": "23:45:12",
        },
        "id": 1,
    }
