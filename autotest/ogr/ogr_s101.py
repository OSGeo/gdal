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

import glob
import os
import shutil
import sys

import gdaltest
import pytest

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_driver("S101")


###############################################################################
# Run test_ogrsf


def _get_files():
    return glob.glob(os.path.join(os.path.dirname(__file__), "data/s101/*.000"))


@pytest.mark.parametrize("filename", _get_files())
def test_ogr_s101_run_test_ogrsf(filename):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + f" -ro {filename}"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################
# Return path of 8211createfromxml utility, or pytest.skip if not available


def _get_create_from_xml_path(cur_file):

    if sys.platform == "win32":
        ext_binary = ".exe"
    else:
        ext_binary = ""

    build_dir = os.path.dirname(os.path.dirname(os.path.dirname(cur_file)))
    path = os.path.join(build_dir, "frmts", "iso8211", "8211createfromxml" + ext_binary)
    if not os.path.exists(path):
        pytest.skip("8211createfromxml not found")

    return path


###############################################################################
# Perform alteration of an XML file based on xpath selection


def _alter_xml(
    cur_file,
    in_xml,
    out_path,
    xpath,
    value,
    exception_text,
    recoverable,
    exception_location="dataset_opening",
):

    create_from_xml_path = _get_create_from_xml_path(cur_file)
    from lxml import etree

    xml_string = open(in_xml, "rb").read()
    tree = etree.fromstring(xml_string)
    node = tree.xpath(xpath)
    assert len(node) == 1
    if value is None:
        node[0].getparent().remove(node[0])
    elif isinstance(value, tuple):
        if isinstance(value[0], tuple):
            for item in value:
                node[0].set(item[0], item[1])
        else:
            node[0].set(value[0], value[1])
    elif isinstance(value, str) and value[0] == "<" and value[-1] == ">":
        node[0].getparent().replace(node[0], etree.fromstring(value))
    else:
        node[0].text = str(value)

    tmp_xml = out_path / "out.xml"
    with open(tmp_xml, "wb") as f:
        new_xml = etree.tostring(tree, pretty_print=True)
        f.write(new_xml)

    if in_xml.endswith(".001.xml"):
        in_000 = in_xml[0 : -len(".001.xml")] + ".000"
        tmp_000 = out_path / "out.000"
        shutil.copy(in_000, tmp_000)

        tmp_001 = out_path / "out.001"
        gdaltest.runexternal(f"{create_from_xml_path} {tmp_xml} {tmp_001}")
    else:
        tmp_000 = out_path / "out.000"
        gdaltest.runexternal(f"{create_from_xml_path} {tmp_xml} {tmp_000}")

    if b"PLACEHOLDER_FF" in new_xml:
        bin_data = open(tmp_000, "rb").read()
        bin_data = bin_data.replace(b"PLACEHOLDER_FF", b"PLACEHOLDER_\xff\xff")
        with open(tmp_000, "wb") as f:
            f.write(bin_data)

    if isinstance(exception_text, tuple):
        exception_text = exception_text[0]

    if exception_text is None:
        with gdaltest.error_raised(gdal.CE_None):
            ds = gdal.Open(tmp_000, open_options=["STRICT=NO"])
            assert ds is not None
            return ds

    regexp = (
        exception_text.replace("(", "\\(")
        .replace(")", "\\)")
        .replace("[", "\\[")
        .replace("]", "\\]")
    )
    if exception_location == "dataset_opening":
        with pytest.raises(Exception, match=regexp):
            ogr.Open(tmp_000)

        if recoverable:
            with gdaltest.error_raised(gdal.CE_Warning, match=exception_text):
                ds = gdal.Open(tmp_000, open_options=["STRICT=NO"])
                assert ds is not None
                return ds
        else:
            with pytest.raises(Exception, match=regexp):
                gdal.Open(tmp_000, open_options=["STRICT=NO"])
            return None
    else:
        ds = ogr.Open(tmp_000)
        with pytest.raises(Exception, match=regexp):
            for lyr in ds:
                for f in lyr:
                    pass

        ds = gdal.Open(tmp_000, open_options=["STRICT=NO"])
        assert ds is not None
        if recoverable:
            with gdaltest.error_raised(gdal.CE_Warning, match=exception_text):
                for lyr in ds:
                    for f in lyr:
                        pass

            for lyr in ds:
                lyr.ResetReading()
            return ds
        else:
            with pytest.raises(Exception, match=regexp):
                for lyr in ds:
                    for f in lyr:
                        pass
            return None


###############################################################################
# Minimal test file that contains nothing


def test_ogr_s101_read_minimal():

    with gdaltest.error_raised(gdal.CE_None):
        ds = ogr.Open("data/s101/minimal.000")
    assert ds.GetDriver().GetDescription() == "S101"
    assert ds.GetLayerCount() == 0
    assert ds.GetMetadata_Dict() == {
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


###############################################################################
# Test alterations of minimal test file


test_ogr_s101_read_minimal_altered_field_definition = [
    (
        ".//DDFFieldDefn[@tag='0000']",
        None,
        "Field definition of 0000 control field not found",
        True,
    ),
    (
        ".//DDFFieldDefn[@tag='0000']",
        (("dataStructCode", "vector"), ("formatControls", "(A)")),
        "Data struct code of field definition of 0000 control field must be elementary",
        True,
    ),
    (
        ".//DDFFieldDefn[@tag='0000']",
        ("dataTypeCode", "mixed_data_type"),
        "Data type code of field definition of 0000 control field must be char_string",
        True,
    ),
    (
        ".//DDFFieldDefn[@tag='0000']",
        ("formatControls", "(A)"),
        "Format controls of field definition of 0000 control field must be empty",
        True,
    ),
    (
        ".//DDFFieldDefn[@tag='0000']",
        ("arrayDescr", "x"),
        "Length of field tag pairs of field definition of 0000 control field must be a multiple of 8",
        True,
    ),
    (
        ".//DDFFieldDefn[@tag='0000']",
        ("arrayDescr", "XXXXDSIDDSIDDSSICSIDCRSHCRSHCSAXCRSHVDAT"),
        "Field 'XXXX' referenced in field definition of 0000 control field does not exist",
        True,
    ),
    (
        ".//DDFFieldDefn[@tag='0000']",
        ("arrayDescr", "DSIDXXXXDSIDDSSICSIDCRSHCRSHCSAXCRSHVDAT"),
        "Field 'XXXX' referenced in field definition of 0000 control field does not exist",
        True,
    ),
    (
        ".//DDFFieldDefn[@tag='0000']",
        ("arrayDescr", "DSSIDSSIDSIDDSSICSIDCRSHCRSHCSAXCRSHVDAT"),
        "Field 'DSSI' referenced in field definition of 0000 control field is not a parent of a registered (parent,child) pair",
        True,
    ),
    (
        ".//DDFFieldDefn[@tag='0000']",
        ("arrayDescr", "DSIDDSIDDSIDDSSICSIDCRSHCRSHCSAXCRSHVDAT"),
        "Field 'DSID' referenced in field definition of 0000 control field is not an allowed child of a registered ('DSID',child) pair",
        True,
    ),
    (
        ".//DDFFieldDefn[@tag='0000']",
        ("arrayDescr", "DSIDDSSI"),
        "Field 'CSID' is not referenced as a parent in field definition of 0000 control field",
        True,
    ),
    (
        ".//DDFFieldDefn[@tag='0000']",
        ("arrayDescr", "DSIDDSSICSIDCRSHCRSHCSAX"),
        "Field 'VDAT' is not referenced as a child in field definition of 0000 control field",
        True,
    ),
    (
        ".//DDFFieldDefn[@tag='0000']",
        ("arrayDescr", "DSIDDSSICSIDCRSHCRSHCSAXCRSHVDATDSIDDSSI"),
        "Pair ('DSID','DSSI') referenced multiple time in field definition of 0000 control field",
        True,
    ),
    (
        ".//DDFFieldDefn[@tag='DSID']",
        ("dataTypeCode", "elementary"),
        "Data type code of field definition 'DSID', got '0' whereas '6' is expected",
        True,
    ),
    (
        ".//DDFFieldDefn[@tag='DSID']",
        ("formatControls", "(b11,b14,7A,A(8),A,A,A,(b11))"),
        "For format controls of field definition 'DSID', got '(b11,b14,7A,A(8),A,A,A,(b11))' whereas '(b11,b14,7A,A(8),3A,(b11))",
        True,
    ),
    (
        ".//DDFFieldDefn[@tag='VDAT']",
        ("arrayDescr", "DTNM!DTID!DTSR!xxxx"),
        "For array description of field definition 'VDAT', got 'DTNM!DTID!DTSR!xxxx' whereas 'DTNM!DTID!DTSR!SCRI' is expected",
        True,
    ),
    (
        ".//DDFFieldDefn[@tag='VDAT']",
        ("tag", "UNKN"),
        "Unknown field definition 'UNKN'",
        True,
    ),
]


@pytest.mark.parametrize(
    "xpath,value,exception_text,recoverable",
    test_ogr_s101_read_minimal_altered_field_definition,
    ids=[
        f"case{i}"
        for i in range(len(test_ogr_s101_read_minimal_altered_field_definition))
    ],
)
def test_ogr_s101_read_minimal_altered_field_definition(
    tmp_path, xpath, value, exception_text, recoverable
):
    _alter_xml(
        __file__,
        "data/s101/minimal.xml",
        tmp_path,
        xpath,
        value,
        exception_text,
        recoverable,
    )


begin_CSID_CRSH_for_two_crs = """
      <DDFField name="CSID">
        <DDFSubfield name="RCNM" type="integer">15</DDFSubfield>
        <DDFSubfield name="RCID" type="integer">1</DDFSubfield>
        <DDFSubfield name="NCRC" type="integer">2</DDFSubfield>
      </DDFField>
      <DDFField name="CRSH">
        <DDFSubfield name="CRIX" type="integer">1</DDFSubfield>
        <DDFSubfield name="CRST" type="integer">1</DDFSubfield>
        <DDFSubfield name="CSTY" type="integer">1</DDFSubfield>
        <DDFSubfield name="CRNM" type="string">WGS84</DDFSubfield>
        <DDFSubfield name="CRSI" type="string">4326</DDFSubfield>
        <DDFSubfield name="CRSS" type="integer">2</DDFSubfield>
        <DDFSubfield name="SCRI" type="string"></DDFSubfield>
      </DDFField>"""

begin_CSID_CRSH_CRSH_for_two_crs = f"""
      {begin_CSID_CRSH_for_two_crs}
      <DDFField name="CRSH">
        <DDFSubfield name="CRIX" type="integer">2</DDFSubfield>
        <DDFSubfield name="CRST" type="integer">5</DDFSubfield>
        <DDFSubfield name="CSTY" type="integer">3</DDFSubfield>
        <DDFSubfield name="CRNM" type="string">meanHighWaterSprings</DDFSubfield>
        <DDFSubfield name="CRSI" type="string"></DDFSubfield>
        <DDFSubfield name="CRSS" type="integer">255</DDFSubfield>
        <DDFSubfield name="SCRI" type="string"></DDFSubfield>
      </DDFField>"""

begin_CSID_CRSH_CRSH_CSAX_for_two_crs = f"""
      {begin_CSID_CRSH_CRSH_for_two_crs}
      <DDFField name="CSAX">
        <DDFSubfield name="AXTY" type="integer">12</DDFSubfield>
        <DDFSubfield name="AXUM" type="integer">4</DDFSubfield>
      </DDFField>"""

test_ogr_s101_read_minimal_altered_record_params = [
    (
        ".//DDFField[@name='DSID']",
        None,
        "DSID field not found",
        False,
    ),
    (
        ".//DDFField[@name='DSID']/DDFSubfield[@name='RCNM']",
        "0",
        "Invalid value for RCNM subfield of DSID",
        True,
    ),
    (
        ".//DDFField[@name='DSID']/DDFSubfield[@name='RCID']",
        "2",
        "Invalid value for RCID subfield of DSID",
        True,
    ),
    (
        ".//DDFField[@name='DSID']/DDFSubfield[@name='PRSP']",
        "INT.IHO.S-101.3.0",
        "Product identifier is 'INT.IHO.S-101.3.0', but only 'INT.IHO.S-101.2.0' is nominally handled",
        True,
    ),
    (
        ".//DDFField[@name='DSSI']",
        None,
        "DSSI field not found",
        False,
    ),
    (
        ".//DDFField[@name='DSSI']/DDFSubfield[@name='DCOX']",
        "nan",
        "NaN value in DCOX subfield of DSSI",
        False,
    ),
    (
        ".//DDFField[@name='DSSI']/DDFSubfield[@name='DCOY']",
        "nan",
        "NaN value in DCOY subfield of DSSI",
        False,
    ),
    (
        ".//DDFField[@name='DSSI']/DDFSubfield[@name='DCOZ']",
        "nan",
        "NaN value in DCOZ subfield of DSSI",
        False,
    ),
    (
        ".//DDFField[@name='DSSI']/DDFSubfield[@name='DCOX']",
        "1",
        "Value of DCOX subfield of DSSI is not at official value",
        True,
    ),
    (
        ".//DDFField[@name='DSSI']/DDFSubfield[@name='DCOY']",
        "1",
        "Value of DCOY subfield of DSSI is not at official value",
        True,
    ),
    (
        ".//DDFField[@name='DSSI']/DDFSubfield[@name='DCOZ']",
        "1",
        "Value of DCOZ subfield of DSSI is not at official value",
        True,
    ),
    (
        ".//DDFField[@name='DSSI']/DDFSubfield[@name='CMFX']",
        "1",
        "Value of CMFX subfield of DSSI is not at official value",
        True,
    ),
    (
        ".//DDFField[@name='DSSI']/DDFSubfield[@name='CMFX']",
        "0",
        "Invalid CMFX/CMFY/CMFZ scale factor in DSSI",
        False,
    ),
    (
        ".//DDFField[@name='DSSI']/DDFSubfield[@name='CMFY']",
        "1",
        "Value of CMFY subfield of DSSI is not at official value",
        True,
    ),
    (
        ".//DDFField[@name='DSSI']/DDFSubfield[@name='CMFY']",
        "0",
        "Invalid CMFX/CMFY/CMFZ scale factor in DSSI",
        False,
    ),
    (
        ".//DDFField[@name='DSSI']/DDFSubfield[@name='CMFZ']",
        "1",
        "Value of CMFZ subfield of DSSI is not at official value",
        True,
    ),
    (
        ".//DDFField[@name='DSSI']/DDFSubfield[@name='CMFZ']",
        "0",
        "Invalid CMFX/CMFY/CMFZ scale factor in DSSI",
        False,
    ),
    (
        ".//DDFField[@name='DSSI']/DDFSubfield[@name='NOIR']",
        "-1",
        "Invalid value for NOIR subfield of DSSI",
        True,
    ),
    (
        ".//DDFField[@name='DSSI']/DDFSubfield[@name='NOIR']",
        "1",
        "1 information type records mentioned in DSSI field, but 0 actually found",
        True,
    ),
    (
        ".//DDFField[@name='DSSI']/DDFSubfield[@name='NOPN']",
        "-1",
        "Invalid value for NOPN subfield of DSSI",
        True,
    ),
    (
        ".//DDFField[@name='DSSI']/DDFSubfield[@name='NOMN']",
        "-1",
        "Invalid value for NOMN subfield of DSSI",
        True,
    ),
    (
        ".//DDFField[@name='DSSI']/DDFSubfield[@name='NOCN']",
        "-1",
        "Invalid value for NOCN subfield of DSSI",
        True,
    ),
    (
        ".//DDFField[@name='DSSI']/DDFSubfield[@name='NOXN']",
        "-1",
        "Invalid value for NOXN subfield of DSSI",
        True,
    ),
    (
        ".//DDFField[@name='DSSI']/DDFSubfield[@name='NOSN']",
        "-1",
        "Invalid value for NOSN subfield of DSSI",
        True,
    ),
    (
        ".//DDFField[@name='DSSI']/DDFSubfield[@name='NOFR']",
        "-1",
        "Invalid value for NOFR subfield of DSSI",
        True,
    ),
    (
        ".//DDFField[@name='CSID']",
        None,
        "CSID field not found",
        True,
    ),
    (
        ".//DDFField[@name='CSID']/DDFSubfield[@name='RCNM']",
        "0",
        "Invalid value for RCNM subfield of CSID",
        True,
    ),
    (
        ".//DDFField[@name='CSID']/DDFSubfield[@name='RCID']",
        "0",
        "Invalid value for RCID subfield of CSID",
        True,
    ),
    (
        ".//DDFField[@name='CSID']/DDFSubfield[@name='NCRC']",
        "0",
        "Invalid value for NCRC subfield of CSID",
        True,
    ),
    (
        ".//DDFField[@name='CSID']/DDFSubfield[@name='NCRC']",
        "2",
        "NCRC field of CSID is not consistent with number of CRSH fields",
        True,
    ),
    (
        ".//DDFField[@name='CRSH']",
        None,
        "NCRC field of CSID is not consistent with number of CRSH fields",
        True,
    ),
    (
        ".//DDFField[@name='CRSH']/DDFSubfield[@name='CRIX']",
        "0",
        "Invalid value for CRIX field of CRSH idx 0",
        True,
    ),
    (
        ".//DDFField[@name='CRSH']/DDFSubfield[@name='CRST']",
        "0",
        "Invalid value for CRST field of CRSH idx 0",
        True,
    ),
    (
        ".//DDFField[@name='CRSH']/DDFSubfield[@name='CSTY']",
        "0",
        "Invalid value for CSTY field of CRSH idx 0",
        True,
    ),
    (
        ".//DDFField[@name='CRSH']/DDFSubfield[@name='CRNM']",
        "my crs",
        "Invalid value for CRNM field of CRSH idx 0",
        True,
    ),
    (
        ".//DDFField[@name='CRSH']/DDFSubfield[@name='CRSI']",
        "12345",
        "Invalid value for CRSI field of CRSH idx 0",
        True,
    ),
    (
        ".//DDFField[@name='CRSH']/DDFSubfield[@name='CRSS']",
        "0",
        "Invalid value for CRSS field of CRSH idx 0",
        True,
    ),
    (
        ".//DDFRecord[.//DDFField[@name='CRSH']]",
        """<DDFRecord>
                 <DDFField name="CSID">
                   <DDFSubfield name="RCNM" type="integer">15</DDFSubfield>
                   <DDFSubfield name="RCID" type="integer">1</DDFSubfield>
                   <DDFSubfield name="NCRC" type="integer">1</DDFSubfield>
                 </DDFField>
                 <DDFField name="CRSH">
                   <DDFSubfield name="CRIX" type="integer">1</DDFSubfield>
                   <DDFSubfield name="CRST" type="integer">1</DDFSubfield>
                   <DDFSubfield name="CSTY" type="integer">1</DDFSubfield>
                   <DDFSubfield name="CRNM" type="string">WGS84</DDFSubfield>
                   <DDFSubfield name="CRSI" type="string">4326</DDFSubfield>
                   <DDFSubfield name="CRSS" type="integer">2</DDFSubfield>
                   <DDFSubfield name="SCRI" type="string"></DDFSubfield>
                 </DDFField>
                 <DDFField name="CSAX">
                    <DDFSubfield name="AXTY" type="integer">12</DDFSubfield>
                    <DDFSubfield name="AXUM" type="integer">4</DDFSubfield>
                 </DDFField>
              </DDFRecord>""",
        "Unexpected CSAX field associated to first CRSH field",
        True,
    ),
    (
        ".//DDFRecord[.//DDFField[@name='CRSH']]",
        """<DDFRecord>
                 <DDFField name="CSID">
                   <DDFSubfield name="RCNM" type="integer">15</DDFSubfield>
                   <DDFSubfield name="RCID" type="integer">1</DDFSubfield>
                   <DDFSubfield name="NCRC" type="integer">1</DDFSubfield>
                 </DDFField>
                 <DDFField name="CSAX">
                    <DDFSubfield name="AXTY" type="integer">12</DDFSubfield>
                    <DDFSubfield name="AXUM" type="integer">4</DDFSubfield>
                 </DDFField>
                 <DDFField name="CRSH">
                   <DDFSubfield name="CRIX" type="integer">1</DDFSubfield>
                   <DDFSubfield name="CRST" type="integer">1</DDFSubfield>
                   <DDFSubfield name="CSTY" type="integer">1</DDFSubfield>
                   <DDFSubfield name="CRNM" type="string">WGS84</DDFSubfield>
                   <DDFSubfield name="CRSI" type="string">4326</DDFSubfield>
                   <DDFSubfield name="CRSS" type="integer">2</DDFSubfield>
                   <DDFSubfield name="SCRI" type="string"></DDFSubfield>
                 </DDFField>
              </DDFRecord>""",
        "CSAX field found before CRSH",
        True,
    ),
    (
        ".//DDFRecord[.//DDFField[@name='CRSH']]",
        """<DDFRecord>
                 <DDFField name="CSID">
                   <DDFSubfield name="RCNM" type="integer">15</DDFSubfield>
                   <DDFSubfield name="RCID" type="integer">1</DDFSubfield>
                   <DDFSubfield name="NCRC" type="integer">1</DDFSubfield>
                 </DDFField>
                 <DDFField name="CRSH">
                   <DDFSubfield name="CRIX" type="integer">1</DDFSubfield>
                   <DDFSubfield name="CRST" type="integer">1</DDFSubfield>
                   <DDFSubfield name="CSTY" type="integer">1</DDFSubfield>
                   <DDFSubfield name="CRNM" type="string">WGS84</DDFSubfield>
                   <DDFSubfield name="CRSI" type="string">4326</DDFSubfield>
                   <DDFSubfield name="CRSS" type="integer">2</DDFSubfield>
                   <DDFSubfield name="SCRI" type="string"></DDFSubfield>
                 </DDFField>
                  <DDFField name="VDAT">
                    <DDFSubfield name="DTNM" type="string">meanHighWaterSprings</DDFSubfield>
                    <DDFSubfield name="DTID" type="string">17</DDFSubfield>
                    <DDFSubfield name="DTSR" type="integer">2</DDFSubfield>
                    <DDFSubfield name="SCRI" type="string"></DDFSubfield>
                  </DDFField>
              </DDFRecord>""",
        "Unexpected VDAT field associated to first CRSH field",
        True,
    ),
    (
        ".//DDFRecord[.//DDFField[@name='CRSH']]",
        """<DDFRecord>
                 <DDFField name="CSID">
                   <DDFSubfield name="RCNM" type="integer">15</DDFSubfield>
                   <DDFSubfield name="RCID" type="integer">1</DDFSubfield>
                   <DDFSubfield name="NCRC" type="integer">1</DDFSubfield>
                 </DDFField>
                 <DDFField name="VDAT">
                    <DDFSubfield name="DTNM" type="string">meanHighWaterSprings</DDFSubfield>
                    <DDFSubfield name="DTID" type="string">17</DDFSubfield>
                    <DDFSubfield name="DTSR" type="integer">2</DDFSubfield>
                    <DDFSubfield name="SCRI" type="string"></DDFSubfield>
                 </DDFField>
                 <DDFField name="CRSH">
                   <DDFSubfield name="CRIX" type="integer">1</DDFSubfield>
                   <DDFSubfield name="CRST" type="integer">1</DDFSubfield>
                   <DDFSubfield name="CSTY" type="integer">1</DDFSubfield>
                   <DDFSubfield name="CRNM" type="string">WGS84</DDFSubfield>
                   <DDFSubfield name="CRSI" type="string">4326</DDFSubfield>
                   <DDFSubfield name="CRSS" type="integer">2</DDFSubfield>
                   <DDFSubfield name="SCRI" type="string"></DDFSubfield>
                 </DDFField>
              </DDFRecord>""",
        "VDAT field found before CRSH",
        True,
    ),
    (
        ".//DDFRecord[.//DDFField[@name='CRSH']]",
        f"""<DDFRecord>
                  {begin_CSID_CRSH_for_two_crs}
                  <DDFField name="CRSH">
                    <DDFSubfield name="CRIX" type="integer">1 <!-- should be 2 --></DDFSubfield>
                    <DDFSubfield name="CRST" type="integer">5</DDFSubfield>
                    <DDFSubfield name="CSTY" type="integer">3</DDFSubfield>
                    <DDFSubfield name="CRNM" type="string">meanHighWaterSprings</DDFSubfield>
                    <DDFSubfield name="CRSI" type="string"></DDFSubfield>
                    <DDFSubfield name="CRSS" type="integer">255</DDFSubfield>
                    <DDFSubfield name="SCRI" type="string"></DDFSubfield>
                  </DDFField>
             </DDFRecord>""",
        "Invalid value for CRIX field of CRSH idx 1",
        True,
    ),
    (
        ".//DDFRecord[.//DDFField[@name='CRSH']]",
        f"""<DDFRecord>
                  {begin_CSID_CRSH_for_two_crs}
                  <DDFField name="CRSH">
                    <DDFSubfield name="CRIX" type="integer">2</DDFSubfield>
                    <DDFSubfield name="CRST" type="integer">1 <!-- should be 5 --></DDFSubfield>
                    <DDFSubfield name="CSTY" type="integer">3</DDFSubfield>
                    <DDFSubfield name="CRNM" type="string">meanHighWaterSprings</DDFSubfield>
                    <DDFSubfield name="CRSI" type="string"></DDFSubfield>
                    <DDFSubfield name="CRSS" type="integer">255</DDFSubfield>
                    <DDFSubfield name="SCRI" type="string"></DDFSubfield>
                  </DDFField>
            </DDFRecord>""",
        "Invalid value for CRST field of CRSH idx 1",
        True,
    ),
    (
        ".//DDFRecord[.//DDFField[@name='CRSH']]",
        f"""<DDFRecord>
                  {begin_CSID_CRSH_for_two_crs}
                  <DDFField name="CRSH">
                    <DDFSubfield name="CRIX" type="integer">2</DDFSubfield>
                    <DDFSubfield name="CRST" type="integer">5</DDFSubfield>
                    <DDFSubfield name="CSTY" type="integer">1 <!-- should be 3 --></DDFSubfield>
                    <DDFSubfield name="CRNM" type="string">meanHighWaterSprings</DDFSubfield>
                    <DDFSubfield name="CRSI" type="string"></DDFSubfield>
                    <DDFSubfield name="CRSS" type="integer">255</DDFSubfield>
                    <DDFSubfield name="SCRI" type="string"></DDFSubfield>
                  </DDFField>
            </DDFRecord>""",
        "Invalid value for CSTY field of CRSH idx 1",
        True,
    ),
    (
        ".//DDFRecord[.//DDFField[@name='CRSH']]",
        f"""<DDFRecord>
                  {begin_CSID_CRSH_for_two_crs}
                  <DDFField name="CRSH">
                    <DDFSubfield name="CRIX" type="integer">2</DDFSubfield>
                    <DDFSubfield name="CRST" type="integer">5</DDFSubfield>
                    <DDFSubfield name="CSTY" type="integer">3</DDFSubfield>
                    <DDFSubfield name="CRNM" type="string">meanHighWaterSprings</DDFSubfield>
                    <DDFSubfield name="CRSI" type="string"></DDFSubfield>
                    <DDFSubfield name="CRSS" type="integer">1 <!-- should be 255 --></DDFSubfield>
                    <DDFSubfield name="SCRI" type="string"></DDFSubfield>
                  </DDFField>
            </DDFRecord>""",
        "Invalid value for CRSS field of CRSH idx 1",
        True,
    ),
    (
        ".//DDFRecord[.//DDFField[@name='CRSH']]",
        f"""<DDFRecord>
                  {begin_CSID_CRSH_for_two_crs}
                  <DDFField name="CRSH">
                    <DDFSubfield name="CRIX" type="integer">2</DDFSubfield>
                    <DDFSubfield name="CRST" type="integer">5</DDFSubfield>
                    <DDFSubfield name="CSTY" type="integer">3</DDFSubfield>
                    <DDFSubfield name="CRNM" type="string">meanHighWaterSprings</DDFSubfield>
                    <DDFSubfield name="CRSI" type="string"></DDFSubfield>
                    <DDFSubfield name="CRSS" type="integer">255</DDFSubfield>
                    <DDFSubfield name="SCRI" type="string"></DDFSubfield>
                  </DDFField>
            </DDFRecord>""",
        "No CSAX field for CRSH idx 1",
        True,
    ),
    (
        ".//DDFRecord[.//DDFField[@name='CRSH']]",
        f"""<DDFRecord>
                 {begin_CSID_CRSH_CRSH_for_two_crs}
                 <DDFField name="CSAX">
                    <DDFSubfield name="AXTY" type="integer">1 <!-- should be 12--></DDFSubfield>
                    <DDFSubfield name="AXUM" type="integer">4</DDFSubfield>
                 </DDFField>
            </DDFRecord>""",
        "Invalid value for AXTY field of CSAX idx 0",
        True,
    ),
    (
        ".//DDFRecord[.//DDFField[@name='CRSH']]",
        f"""<DDFRecord>
                 {begin_CSID_CRSH_CRSH_for_two_crs}
                 <DDFField name="CSAX">
                    <DDFSubfield name="AXTY" type="integer">12</DDFSubfield>
                    <DDFSubfield name="AXUM" type="integer">1 <!-- should be 4--></DDFSubfield>
                 </DDFField>
            </DDFRecord>""",
        "Invalid value for AXUM field of CSAX idx 0",
        True,
    ),
    (
        ".//DDFRecord[.//DDFField[@name='CRSH']]",
        f"""<DDFRecord>
                 {begin_CSID_CRSH_CRSH_CSAX_for_two_crs}
            </DDFRecord>""",
        "No VDAT field for CRSH idx 1",
        True,
    ),
    (
        ".//DDFRecord[.//DDFField[@name='CRSH']]",
        f"""<DDFRecord>
                 {begin_CSID_CRSH_CRSH_CSAX_for_two_crs}
                 <DDFField name="CSAX">
                    <DDFSubfield name="AXTY" type="integer">12</DDFSubfield>
                    <DDFSubfield name="AXUM" type="integer">4</DDFSubfield>
                 </DDFField>
            </DDFRecord>""",
        "Several CSAX fields associated to CRSH instance of index 1",
        True,
    ),
    (
        ".//DDFRecord[.//DDFField[@name='CRSH']]",
        f"""<DDFRecord>
                 {begin_CSID_CRSH_CRSH_CSAX_for_two_crs}
                 <DDFField name="VDAT">
                    <DDFSubfield name="DTNM" type="string">meanHighWaterSprings</DDFSubfield>
                    <DDFSubfield name="DTID" type="string">17</DDFSubfield>
                    <DDFSubfield name="DTSR" type="integer">255 <!-- should be 2 --></DDFSubfield>
                    <DDFSubfield name="SCRI" type="string"></DDFSubfield>
                 </DDFField>
            </DDFRecord>""",
        "Invalid value for DTSR field of VDAT idx 0",
        True,
    ),
    (
        ".//DDFRecord[.//DDFField[@name='CRSH']]",
        f"""<DDFRecord>
                 {begin_CSID_CRSH_CRSH_CSAX_for_two_crs}
                 <DDFField name="VDAT">
                    <DDFSubfield name="DTNM" type="string">with " double quote "</DDFSubfield>
                    <DDFSubfield name="DTID" type="string">17</DDFSubfield>
                    <DDFSubfield name="DTSR" type="integer">2</DDFSubfield>
                    <DDFSubfield name="SCRI" type="string"></DDFSubfield>
                 </DDFField>
            </DDFRecord>""",
        "Double quote not allowed in vertical datum name",
        True,
    ),
    (
        ".//DDFRecord[.//DDFField[@name='CRSH']]",
        f"""<DDFRecord>
                 {begin_CSID_CRSH_CRSH_CSAX_for_two_crs}
                 <DDFField name="VDAT">
                    <DDFSubfield name="DTNM" type="string">meanHighWaterSprings</DDFSubfield>
                    <DDFSubfield name="DTID" type="string">17</DDFSubfield>
                    <DDFSubfield name="DTSR" type="integer">2</DDFSubfield>
                    <DDFSubfield name="SCRI" type="string"></DDFSubfield>
                 </DDFField>
                 <DDFField name="VDAT">
                    <DDFSubfield name="DTNM" type="string">meanHighWaterSprings</DDFSubfield>
                    <DDFSubfield name="DTID" type="string">17</DDFSubfield>
                    <DDFSubfield name="DTSR" type="integer">2</DDFSubfield>
                    <DDFSubfield name="SCRI" type="string"></DDFSubfield>
                 </DDFField>
            </DDFRecord>""",
        "Several VDAT fields associated to CRSH instance of index 1",
        True,
    ),
    (
        ".//DDFRecord[.//DDFField[@name='CRSH']]",
        f"""<DDFRecord>
                 {begin_CSID_CRSH_for_two_crs}
                 <DDFField name="CRSH">
                    <DDFSubfield name="CRIX" type="integer">2</DDFSubfield>
                    <DDFSubfield name="CRST" type="integer">5</DDFSubfield>
                    <DDFSubfield name="CSTY" type="integer">3</DDFSubfield>
                    <DDFSubfield name="CRNM" type="string">with " double quote "</DDFSubfield>
                    <DDFSubfield name="CRSI" type="string"></DDFSubfield>
                    <DDFSubfield name="CRSS" type="integer">255</DDFSubfield>
                    <DDFSubfield name="SCRI" type="string"></DDFSubfield>
                 </DDFField>
                 <DDFField name="CSAX">
                    <DDFSubfield name="AXTY" type="integer">12</DDFSubfield>
                    <DDFSubfield name="AXUM" type="integer">4</DDFSubfield>
                 </DDFField>
                 <DDFField name="VDAT">
                    <DDFSubfield name="DTNM" type="string">meanHighWaterSprings</DDFSubfield>
                    <DDFSubfield name="DTID" type="string">17</DDFSubfield>
                    <DDFSubfield name="DTSR" type="integer">2</DDFSubfield>
                    <DDFSubfield name="SCRI" type="string"></DDFSubfield>
                 </DDFField>
              </DDFRecord>""",
        "Double quote not allowed in vertical CRS name",
        True,
    ),
]


@pytest.mark.parametrize(
    "xpath,value,exception_text,recoverable",
    test_ogr_s101_read_minimal_altered_record_params,
    ids=[
        f"case{i}" for i in range(len(test_ogr_s101_read_minimal_altered_record_params))
    ],
)
def test_ogr_s101_read_minimal_altered_record_content(
    tmp_path, xpath, value, exception_text, recoverable
):
    _alter_xml(
        __file__,
        "data/s101/minimal.xml",
        tmp_path,
        xpath,
        value,
        exception_text,
        recoverable,
    )


def test_ogr_s101_read_invalid_non_increasing_field_offset():

    with pytest.raises(Exception, match="Field `CRSH' overlapping with previous one"):
        ogr.Open("data/s101/invalid/non_increasing_field_offset.000")
