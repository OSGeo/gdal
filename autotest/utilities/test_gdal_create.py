#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_create testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2020, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os

import gdaltest
import pytest
import test_cli_utilities

from osgeo import gdal


def get_gdal_create_path():
    return test_cli_utilities.get_cli_utility_path("gdal_create")


pytestmark = pytest.mark.skipif(
    get_gdal_create_path() is None, reason="gdal_create not available"
)


@pytest.fixture()
def gdal_create_path():
    return get_gdal_create_path()


###############################################################################


@pytest.mark.parametrize("burn", ("-burn 1.1 2", '-burn "1 2"', "-burn 1 -burn 2"))
def test_gdal_create_pdf_tif(gdal_create_path, tmp_path, burn):

    output_tif = str(tmp_path / "tmp.tif")

    _, err = gdaltest.runexternal_out_and_err(
        gdal_create_path
        + f" -bands 3 -outsize 1 2 -a_srs EPSG:4326 -a_ullr 2 50 3 49 -a_nodata 5 {burn} -ot UInt16 -co COMPRESS=DEFLATE -mo FOO=BAR  {output_tif}"
    )
    assert err is None or err == "", "got error/warning"

    ds = gdal.Open(output_tif)
    assert ds.RasterCount == 3
    assert ds.RasterXSize == 1
    assert ds.RasterYSize == 2
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
    assert ds.GetRasterBand(1).GetNoDataValue() == 5
    assert "4326" in ds.GetProjectionRef()
    assert ds.GetGeoTransform() == (2.0, 1.0, 0.0, 50.0, 0.0, -0.5)
    assert ds.GetRasterBand(1).Checksum() == 2
    assert ds.GetRasterBand(2).Checksum() == 4
    assert ds.GetRasterBand(3).Checksum() == 4
    assert ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "DEFLATE"
    assert ds.GetMetadataItem("FOO") == "BAR"
    ds = None


###############################################################################


@pytest.mark.require_driver("PNG")
def test_gdal_create_pdf_no_direct_write_capabilities(gdal_create_path, tmp_path):

    output_png = str(tmp_path / "tmp.png")

    _, err = gdaltest.runexternal_out_and_err(
        gdal_create_path + f" {output_png} -of PNG -outsize 1 2"
    )
    assert err is None or err == "", "got error/warning"

    ds = gdal.Open(output_png)
    assert ds.RasterCount == 1
    assert ds.RasterXSize == 1
    assert ds.RasterYSize == 2
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt8
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    assert ds.GetProjectionRef() == ""
    assert ds.GetGeoTransform(can_return_null=True) is None
    assert ds.GetRasterBand(1).Checksum() == 0
    ds = None


###############################################################################


@pytest.mark.skipif(
    gdal.GetDriverByName("PDF") is None
    or gdal.GetDriverByName("PDF").GetMetadataItem("HAVE_POPPLER") is None,
    reason="Poppler PDF support missing",
)
def test_gdal_create_pdf_composition(gdal_create_path, tmp_path):

    tmp_xml = str(tmp_path / "tmp.xml")
    tmp_pdf = str(tmp_path / "tmp.pdf")

    open(tmp_xml, "wt").write("""<PDFComposition>
    <Page>
        <DPI>72</DPI>
        <Width>20</Width>
        <Height>20</Height>
        <Content streamCompression="NONE">
            <Raster dataset="../gcore/data/byte.tif" tileSize="16">
                <Blending function="Multiply" opacity="0.7"/>
            </Raster>
        </Content>
    </Page>
</PDFComposition>""")

    _, err = gdaltest.runexternal_out_and_err(
        gdal_create_path + f" {tmp_pdf} -co COMPOSITION_FILE={tmp_xml}"
    )
    assert err is None or err == "", "got error/warning"

    assert os.path.exists(tmp_pdf)


###############################################################################


@pytest.mark.require_driver("TGA")
def test_gdal_create_not_write_driver(gdal_create_path):

    _, err = gdaltest.runexternal_out_and_err(
        gdal_create_path + " /vsimem/tmp.tga -of TGA -outsize 1 2"
    )
    assert "This driver has no creation capabilities" in err


###############################################################################


def test_gdal_create_input_file_invalid(gdal_create_path, tmp_path):

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_create_path} {tmp_path}/tmp.tif -if ../gdrivers/data/i_do_not_exist"
    )
    assert err != ""

    assert not os.path.exists(f"{tmp_path}/tmp.tif")


###############################################################################


def test_gdal_create_input_file(gdal_create_path, tmp_path):

    output_tif = str(tmp_path / "tmp.tif")

    _, err = gdaltest.runexternal_out_and_err(
        gdal_create_path + f" {output_tif} -if ../gdrivers/data/small_world.tif"
    )
    assert err is None or err == "", "got error/warning"

    assert os.path.exists(output_tif)

    ds = gdal.Open(output_tif)
    ref_ds = gdal.Open("../gdrivers/data/small_world.tif")
    assert ds.RasterCount == ref_ds.RasterCount
    assert ds.RasterXSize == ref_ds.RasterXSize
    assert ds.RasterYSize == ref_ds.RasterYSize
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    assert ds.GetGeoTransform() == ref_ds.GetGeoTransform()
    assert ds.GetProjectionRef() == ref_ds.GetProjectionRef()
    ds = None


###############################################################################


def test_gdal_create_input_file_overrrides(gdal_create_path, tmp_path):

    output_tif = str(tmp_path / "tmp.tif")

    _, err = gdaltest.runexternal_out_and_err(
        gdal_create_path
        + f" {output_tif} -if ../gdrivers/data/small_world.tif -bands 2 -outsize 1 3 -a_nodata 1"
    )
    assert err is None or err == "", "got error/warning"

    assert os.path.exists(output_tif)

    ds = gdal.Open(output_tif)
    ref_ds = gdal.Open("../gdrivers/data/small_world.tif")
    assert ds.RasterCount == 2
    assert ds.RasterXSize == 1
    assert ds.RasterYSize == 3
    assert ds.GetRasterBand(1).GetNoDataValue() == 1
    assert ds.GetGeoTransform() == ref_ds.GetGeoTransform()
    assert ds.GetProjectionRef() == ref_ds.GetProjectionRef()
    ds = None


###############################################################################


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdal_create_input_file_gcps(gdal_create_path, tmp_path):

    output_tif = str(tmp_path / "tmp.tif")

    _, err = gdaltest.runexternal_out_and_err(
        gdal_create_path + f" {output_tif} -if ../gcore/data/gcps.vrt"
    )
    assert err is None or err == "", "got error/warning"

    assert os.path.exists(output_tif)

    ds = gdal.Open(output_tif)
    ref_ds = gdal.Open("../gcore/data/gcps.vrt")
    assert ds.GetGCPCount() == ref_ds.GetGCPCount()
    assert ds.GetGCPSpatialRef().IsSame(ref_ds.GetGCPSpatialRef())
    ds = None
