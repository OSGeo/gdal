#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for RPFTOC driver.
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2010, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import shutil

import gdaltest
import pytest

from osgeo import gdal

pytestmark = [pytest.mark.require_driver("RPFTOC"), pytest.mark.require_driver("NITF")]

###############################################################################
# Read a simple and hand-made RPFTOC dataset, made of one single CADRG frame
# whose content is fully empty.


def test_rpftoc_1():
    tst = gdaltest.GDALTest(
        "RPFTOC",
        "NITF_TOC_ENTRY:CADRG_ONC_1,000,000_2_0:data/nitf/A.TOC",
        1,
        53599,
        filename_absolute=1,
    )
    gt = (
        1.9999416000000001,
        0.0017833876302083334,
        0.0,
        36.000117500000002,
        0.0,
        -0.0013461816406249993,
    )
    tst.testOpen(check_gt=gt)

    ds = gdal.Open("NITF_TOC_ENTRY:CADRG_ONC_1,000,000_2_0:data/nitf/A.TOC")
    assert ds.GetRasterBand(1).GetColorTable().GetCount() == 217
    assert ds.GetRasterBand(1).GetNoDataValue() == 216


###############################################################################
# Same test as rpftoc_1, but the dataset is forced to be opened in RGBA mode


def test_rpftoc_2():
    with gdal.config_option("RPFTOC_FORCE_RGBA", "YES"), gdal.Open(
        "NITF_TOC_ENTRY:CADRG_ONC_1,000,000_2_0:data/nitf/A.TOC"
    ) as ds:
        assert ds.RasterCount == 4
        assert [ds.GetRasterBand(i + 1).Checksum() for i in range(4)] == [0, 0, 0, 0]


###############################################################################
# Same test as rpftoc_1, but the dataset is forced to be opened in RGBA mode


def test_rpftoc_force_rgba_open_option():
    with gdal.OpenEx(
        "NITF_TOC_ENTRY:CADRG_ONC_1,000,000_2_0:data/nitf/A.TOC",
        open_options=["FORCE_RGBA=YES"],
    ) as ds:
        assert ds.RasterCount == 4
        assert [ds.GetRasterBand(i + 1).Checksum() for i in range(4)] == [0, 0, 0, 0]


###############################################################################
# Test reading the metadata


def test_rpftoc_3():
    ds = gdal.Open("data/nitf/A.TOC")
    md = ds.GetMetadata("SUBDATASETS")
    assert (
        "SUBDATASET_1_NAME" in md
        and md["SUBDATASET_1_NAME"]
        == "NITF_TOC_ENTRY:CADRG_ONC_1,000,000_2_0:data/nitf/A.TOC"
    ), "missing SUBDATASET_1_NAME metadata"

    ds = gdal.Open("NITF_TOC_ENTRY:CADRG_ONC_1,000,000_2_0:data/nitf/A.TOC")
    md = ds.GetMetadata()
    assert (
        "FILENAME_0" in md
        and md["FILENAME_0"].replace("\\", "/") == "data/nitf/RPFTOC01.ON2"
    )


###############################################################################
# Test reading polar zone


def test_rpftoc_zone9():
    ds = gdal.Open("NITF_TOC_ENTRY:CADRG_ONC_1M_9_0:data/rpftoc/zone9/RPF/A.TOC")
    ref_ds = gdal.Open("data/rpftoc/zone9/RPF/ZONE9/00027010.ON9")
    assert ds.GetSpatialRef().IsSame(ref_ds.GetSpatialRef())
    assert ds.GetGeoTransform() == pytest.approx(ref_ds.GetGeoTransform())
    assert ds.GetRasterBand(1).Checksum() == ref_ds.GetRasterBand(1).Checksum()


###############################################################################
# Add an overview


def test_rpftoc_4():

    shutil.copyfile("data/nitf/A.TOC", "tmp/A.TOC")
    shutil.copyfile("data/nitf/RPFTOC01.ON2", "tmp/RPFTOC01.ON2")

    with gdal.config_option("RPFTOC_FORCE_RGBA", "YES"):
        ds = gdal.Open("NITF_TOC_ENTRY:CADRG_ONC_1,000,000_2_0:tmp/A.TOC")
        err = ds.BuildOverviews(overviewlist=[2, 4])

        assert err == 0, "BuildOverviews reports an error"

        assert (
            ds.GetRasterBand(1).GetOverviewCount() == 2
        ), "Overview missing on target file."

        ds = None
        ds = gdal.Open("NITF_TOC_ENTRY:CADRG_ONC_1,000,000_2_0:tmp/A.TOC")
        assert (
            ds.GetRasterBand(1).GetOverviewCount() == 2
        ), "Overview missing on target file after re-open."

        ds = None

    os.unlink("tmp/A.TOC")
    os.unlink("tmp/A.TOC.1.ovr")
    os.unlink("tmp/RPFTOC01.ON2")


###############################################################################
# Create a A.TOC file


def test_rpftoc_create_simple(tmp_vsimem):

    if gdaltest.is_travis_branch("fedora_rawhide"):
        pytest.skip(
            "randomly fails on CI, but not when trying locally within a docker image"
        )

    gdal.Mkdir(tmp_vsimem / "subdir", 0o755)
    gdal.alg.vsi.copy(
        source="data/nitf/RPFTOC01.ON2",
        destination=tmp_vsimem / "subdir" / "0005U001.ON2",
    )
    with gdaltest.error_raised(gdal.CE_None):
        assert gdal.alg.driver.rpftoc.create(input=tmp_vsimem)

    with gdaltest.error_raised(
        gdal.CE_Warning,
        match="appears to be an NITF file, but no image blocks were found on it",
    ):
        with gdal.OpenEx(tmp_vsimem / "A.TOC", allowed_drivers=["NITF"]) as ds:
            tre = ds.GetMetadata("xml:TRE")[0]

        assert tre.startswith("""<tres>
  <tre name="RPFHDR" location="file">
    <field name="LITTLE_BIG_ENDIAN_INDICATOR" value="0" />
    <field name="HEADER_SECTION_LENGTH" value="48" />
    <field name="FILENAME" value="       A.TOC" />
    <field name="NEW_REPLACEMENT_UPDATE_INDICATOR" value="0" />
    <field name="GOVERNING_STANDARD_NUMBER" value="MIL-C-89038" />
    <field name="GOVERNING_STANDARD_DATE" value="19941006" />
    <field name="SECURITY_CLASSIFICATION" value="U" />
    <field name="SECURITY_COUNTRY_INTERNATIONAL_CODE" value="" />
    <field name="SECURITY_RELEASE_MARKING" value="" />
    <field name="LOCATION_SECTION_LOCATION" value="683" />
  </tre>
  <tre name="RPFDES" location="des Registered Extensions">
    <field name="LOCATION_SECTION_LENGTH" value="54" />
    <field name="COMPONENT_LOCATION_OFFSET" value="14" />
    <field name="NUMBER_OF_COMPONENT_LOCATION_RECORDS" value="4" />
    <field name="COMPONENT_LOCATION_RECORD_LENGTH" value="10" />
    <field name="COMPONENT_AGGREGATE_LENGTH" value="197" />
    <repeated name="CLR" number="4">
      <group index="0">
        <field name="COMPONENT_ID" value="148" />
        <field name="COMPONENT_LENGTH" value="8" />
        <field name="COMPONENT_LOCATION" value="737" />
        <content ComponentName="BoundaryRectangleSectionSubheader">
          <field name="BOUNDARY_RECTANGLE_TABLE_OFFSET" value="0" />
          <field name="NUMBER_OF_BOUNDARY_RECTANGLE_RECORDS" value="1" />
          <field name="BOUNDARY_RECTANGLE_RECORD_LENGTH" value="132" />
        </content>
      </group>
      <group index="1">
        <field name="COMPONENT_ID" value="149" />
        <field name="COMPONENT_LENGTH" value="132" />
        <field name="COMPONENT_LOCATION" value="745" />
        <content ComponentName="BoundaryRectangleTable">
          <repeated name="BRR" number="1">
            <group index="0">
              <field name="PRODUCT_DATA_TYPE" value="CADRG" />
              <field name="COMPRESSION_RATIO" value="55:1" />
              <field name="SCALE_OR_RESOLUTION" value="1:1M" />
              <field name="ZONE" value="2" />
              <field name="PRODUCER" value="" />""")

        if False:
            """
            <field name="NORTHWEST_LATITUDE" value="35.172413793103452" />
            <field name="NORTHWEST_LONGITUDE" value="0.91370558375635369" />
            <field name="SOUTHWEST_LATITUDE" value="33.103448275862071" />
            <field name="SOUTHWEST_LONGITUDE" value="0.91370558375635369" />
            <field name="NORTHEAST_LATITUDE" value="35.172413793103452" />
            <field name="NORTHEAST_LONGITUDE" value="3.654822335025389" />
            <field name="SOUTHEAST_LATITUDE" value="33.103448275862071" />
            <field name="SOUTHEAST_LONGITUDE" value="3.654822335025389" />
            <field name="NORTH_SOUTH_VERTICAL_RESOLUTION" value="149.65862068965518" />
            <field name="EAST_WEST_HORIZONTAL_RESOLUTION" value="149.84999999999999" />
            <field name="LATITUDE_VERTICAL_INTERVAL" value="0.0013469827586206897" />
            <field name="LONGITUDE_HORIZONTAL_INTERVAL" value="0.0017845812182741116" />
            """
        assert '<field name="NORTHWEST_LATITUDE" value="35.172' in tre
        assert '<field name="NORTHWEST_LONGITUDE" value="0.913' in tre
        assert '<field name="SOUTHWEST_LATITUDE" value="33.103' in tre
        assert '<field name="SOUTHWEST_LONGITUDE" value="0.913' in tre
        assert '<field name="NORTHEAST_LATITUDE" value="35.172' in tre
        assert '<field name="NORTHEAST_LONGITUDE" value="3.654' in tre
        assert '<field name="SOUTHEAST_LATITUDE" value="33.103' in tre
        assert '<field name="SOUTHEAST_LONGITUDE" value="3.654' in tre
        assert '<field name="NORTH_SOUTH_VERTICAL_RESOLUTION" value="149.658' in tre
        assert (
            '<field name="EAST_WEST_HORIZONTAL_RESOLUTION" value="149.849' in tre
            or '<field name="EAST_WEST_HORIZONTAL_RESOLUTION" value="149.850' in tre
        )
        assert '<field name="LATITUDE_VERTICAL_INTERVAL" value="0.00134' in tre
        assert '<field name="LONGITUDE_HORIZONTAL_INTERVAL" value="0.00178' in tre

        assert tre.endswith(
            """<field name="NUMBER_OF_FRAMES_NORTH_SOUTH_DIRECTION" value="1" />
              <field name="NUMBER_OF_FRAMES_EAST_WEST_DIRECTION" value="1" />
            </group>
          </repeated>
        </content>
      </group>
      <group index="2">
        <field name="COMPONENT_ID" value="150" />
        <field name="COMPONENT_LENGTH" value="13" />
        <field name="COMPONENT_LOCATION" value="877" />
        <content ComponentName="FrameFileIndexSectionSubHeader">
          <field name="HIGHEST_SECURITY_CLASSIFICATION" value="U" />
          <field name="FRAME_FILE_INDEX_TABLE_OFFSET" value="0" />
          <field name="NUMBER_OF_FRAME_FILE_INDEX_RECORDS" value="1" />
          <field name="NUMBER_OF_PATH_NAME_RECORDS" value="1" />
          <field name="FRAME_FILE_INDEX_RECORD_LENGTH" value="33" />
        </content>
      </group>
      <group index="3">
        <field name="COMPONENT_ID" value="151" />
        <field name="COMPONENT_LENGTH" value="44" />
        <field name="COMPONENT_LOCATION" value="890" />
        <content ComponentName="FrameFileIndexSubsection">
          <repeated name="FFIR" number="1">
            <group index="0">
              <field name="BOUNDARY_RECTANGLE_RECORD_NUMBER" value="0" />
              <field name="FRAME_LOCATION_ROW_NUMBER" value="0" />
              <field name="FRAME_LOCATION_COLUMN_NUMBER" value="0" />
              <field name="PATHNAME_RECORD_OFFSET" value="33" />
              <field name="FRAME_FILE_NAME" value="0005U001.ON2" />
              <field name="GEOGRAPHIC_LOCATION" value="NJBD59" />
              <field name="FRAME_FILE_SECURITY_CLASSIFICATION" value="U" />
              <field name="FRAME_FILE_SECURITY_COUNTRY_CODE" value="" />
              <field name="FRAME_FILE_SECURITY_RELEASE_MARKING" value="" />
            </group>
          </repeated>
          <repeated name="PNR" number="1">
            <group index="0">
              <field name="PATHNAME_LENGTH" value="9" />
              <field name="PATHNAME" value="./subdir/" />
            </group>
          </repeated>
        </content>
      </group>
    </repeated>
    <warning>197 remaining bytes at end of RPFDES TRE</warning>
  </tre>
</tres>
"""
        )

    with gdal.Open(tmp_vsimem / "A.TOC") as ds:
        assert ds.GetMetadata() == {
            "NITF_CLEVEL": "03",
            "NITF_ENCRYP": "0",
            "NITF_FDT": "11111111ZJAN26",
            "NITF_FHDR": "NITF02.00",
            "NITF_FSCAUT": "",
            "NITF_FSCLAS": "U",
            "NITF_FSCODE": "",
            "NITF_FSCOP": "00000",
            "NITF_FSCPYS": "00000",
            "NITF_FSCTLH": "",
            "NITF_FSCTLN": "",
            "NITF_FSDWNG": "",
            "NITF_FSREL": "",
            "NITF_FTITLE": "       A.TOC",
            "NITF_ONAME": "",
            "NITF_OPHONE": "",
            "NITF_OSTAID": "GDAL",
            "NITF_STYPE": "",
        }
        assert ds.GetSubDatasets() == [
            (
                f"NITF_TOC_ENTRY:CADRG_ONC_1M_2_0:{tmp_vsimem}/A.TOC",
                "CADRG:ONC:Operational Navigation Chart:1M:2:0",
            )
        ]

    with gdal.Open(f"NITF_TOC_ENTRY:CADRG_ONC_1M_2_0:{tmp_vsimem}/A.TOC") as ds:
        assert ds.GetGeoTransform() == pytest.approx(
            (
                0.9137055837563537,
                0.0017833876302083334,
                0.0,
                35.17241379310345,
                0.0,
                -0.0013461816406249993,
            )
        )


###############################################################################
# Test errors in creating a A.TOC file


def test_rpftoc_create_errors(tmp_vsimem):

    with pytest.raises(
        Exception, match="/i_do/not/exist is not a directory or cannot be opened"
    ):
        gdal.alg.driver.rpftoc.create(input="/i_do/not/exist")

    with pytest.raises(Exception, match="No CADRG frame found"):
        gdal.alg.driver.rpftoc.create(input=tmp_vsimem)

    gdal.alg.vsi.copy(
        source="data/nitf/RPFTOC01.ON2",
        destination=tmp_vsimem / "subdir" / "0005U001.ON2",
    )
    with pytest.raises(Exception, match="Unable to create file"):
        gdal.alg.driver.rpftoc.create(
            input=tmp_vsimem / "subdir", output="/i_do/not/exist/a.toc"
        )


###############################################################################


@gdaltest.enable_exceptions()
def test_rpftoc_create_two_scales(tmp_vsimem):

    if gdaltest.is_travis_branch("fedora_rawhide"):
        pytest.skip(
            "randomly fails on CI, but not when trying locally within a docker image"
        )

    src_ds = gdal.Translate(
        "",
        "data/byte.tif",
        format="MEM",
        bandList=[1, 1, 1],
        colorInterpretation=["red", "green", "blue"],
    )
    gdal.GetDriverByName("NITF").CreateCopy(
        tmp_vsimem / "100k",
        src_ds,
        options=["PRODUCT_TYPE=CADRG", "SERIES_CODE=TC"],
    )
    gdal.GetDriverByName("NITF").CreateCopy(
        tmp_vsimem / "200k",
        src_ds,
        options=["PRODUCT_TYPE=CADRG", "SERIES_CODE=AT"],
    )

    gdal.Mkdir(tmp_vsimem / "final", 0o755)
    gdal.Mkdir(tmp_vsimem / "final" / "100k", 0o755)
    gdal.Mkdir(tmp_vsimem / "final" / "200k", 0o755)
    gdal.alg.vsi.copy(
        source=tmp_vsimem / "100k/RPF/ZONE2/00AEH010.TC2",
        destination=tmp_vsimem / "final" / "100k",
    )
    gdal.alg.vsi.copy(
        source=tmp_vsimem / "200k/RPF/ZONE2/002CM010.AT2",
        destination=tmp_vsimem / "final" / "200k",
    )

    gdal.alg.driver.rpftoc.create(input=tmp_vsimem / "final")

    with gdaltest.error_raised(
        gdal.CE_Warning,
        match="appears to be an NITF file, but no image blocks were found on it",
    ):
        with gdal.OpenEx(
            tmp_vsimem / "final" / "A.TOC", allowed_drivers=["NITF"]
        ) as ds:
            tre = ds.GetMetadata("xml:TRE")[0]

    # Check that 200K is before 100K

    assert """<group index="0">
              <field name="PRODUCT_DATA_TYPE" value="CADRG" />
              <field name="COMPRESSION_RATIO" value="55:1" />
              <field name="SCALE_OR_RESOLUTION" value="1:200K" />""" in tre

    assert """<group index="1">
              <field name="PRODUCT_DATA_TYPE" value="CADRG" />
              <field name="COMPRESSION_RATIO" value="55:1" />
              <field name="SCALE_OR_RESOLUTION" value="1:100K" />""" in tre
