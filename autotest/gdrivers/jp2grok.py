#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for JP2Grok driver.
# Author:   Aaron Boxer <aaron.boxer@grokcompression.com>
#
###############################################################################
# Copyright (c) 2026, Grok Image Compression Inc.
#
# SPDX-License-Identifier: MIT
###############################################################################


import os
import shutil
import sys

import gdaltest
import pytest
from test_py_scripts import samples_path

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_driver("JP2Grok")


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def startup_and_cleanup():

    gdaltest.jp2grok_drv = gdal.GetDriverByName("JP2Grok")
    assert gdaltest.jp2grok_drv is not None

    gdaltest.deregister_all_jpeg2000_drivers_but("JP2Grok")

    yield

    gdaltest.reregister_all_jpeg2000_drivers()


###############################################################################
# Open byte.jp2


def test_jp2grok_2():

    srs = """PROJCS["NAD27 / UTM zone 11N",
    GEOGCS["NAD27",
        DATUM["North_American_Datum_1927",
            SPHEROID["Clarke 1866",6378206.4,294.9786982138982,
                AUTHORITY["EPSG","7008"]],
            AUTHORITY["EPSG","6267"]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433],
        AUTHORITY["EPSG","4267"]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",-117],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AUTHORITY["EPSG","26711"]]
"""
    gt = (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)

    tst = gdaltest.GDALTest("JP2Grok", "jpeg2000/byte.jp2", 1, 50054)
    tst.testOpen(check_prj=srs, check_gt=gt)


###############################################################################
# Open int16.jp2


def test_jp2grok_3():

    ds = gdal.Open("data/jpeg2000/int16.jp2")
    ds_ref = gdal.Open("data/int16.tif")

    # 9x7 wavelet
    assert ds.GetMetadata("IMAGE_STRUCTURE")["COMPRESSION_REVERSIBILITY"] == "LOSSY"
    assert ds.GetMetadataItem("COMPRESSION_REVERSIBILITY", "IMAGE_STRUCTURE") == "LOSSY"

    maxdiff = gdaltest.compare_ds(ds, ds_ref)

    ds = None
    ds_ref = None

    # Quite a bit of difference...
    assert maxdiff <= 6, "Image too different from reference"

    ds = ogr.Open("data/jpeg2000/int16.jp2")
    assert ds is None


###############################################################################
# Test copying byte.jp2


def test_jp2grok_4(out_filename="tmp/jp2grok_4.jp2"):

    src_ds = gdal.Open("data/jpeg2000/byte.jp2")
    assert (
        src_ds.GetMetadataItem("COMPRESSION_REVERSIBILITY", "IMAGE_STRUCTURE")
        == "LOSSLESS"
    )
    src_wkt = src_ds.GetProjectionRef()
    src_gt = src_ds.GetGeoTransform()

    vrt_ds = gdal.GetDriverByName("VRT").CreateCopy("/vsimem/jp2grok_4.vrt", src_ds)
    vrt_ds.SetMetadataItem("TIFFTAG_XRESOLUTION", "300")
    vrt_ds.SetMetadataItem("TIFFTAG_YRESOLUTION", "200")
    vrt_ds.SetMetadataItem("TIFFTAG_RESOLUTIONUNIT", "3 (pixels/cm)")

    gdal.Unlink(out_filename)

    out_ds = gdal.GetDriverByName("JP2Grok").CreateCopy(
        out_filename, vrt_ds, options=["REVERSIBLE=YES", "QUALITY=100"]
    )
    del out_ds

    vrt_ds = None
    gdal.Unlink("/vsimem/jp2grok_4.vrt")

    assert gdal.VSIStatL(out_filename + ".aux.xml") is None

    ds = gdal.Open(out_filename)
    assert (
        ds.GetMetadataItem("COMPRESSION_REVERSIBILITY", "IMAGE_STRUCTURE") == "LOSSLESS"
    )
    cs = ds.GetRasterBand(1).Checksum()
    got_wkt = ds.GetProjectionRef()
    got_gt = ds.GetGeoTransform()
    xres = ds.GetMetadataItem("TIFFTAG_XRESOLUTION")
    yres = ds.GetMetadataItem("TIFFTAG_YRESOLUTION")
    resunit = ds.GetMetadataItem("TIFFTAG_RESOLUTIONUNIT")
    ds = None

    gdal.Unlink(out_filename)

    assert (
        xres == "300" and yres == "200" and resunit == "3 (pixels/cm)"
    ), "bad resolution"

    sr1 = osr.SpatialReference()
    sr1.SetFromUserInput(got_wkt)
    sr2 = osr.SpatialReference()
    sr2.SetFromUserInput(src_wkt)

    if sr1.IsSame(sr2) == 0:
        print(got_wkt)
        print(src_wkt)
        pytest.fail("bad spatial reference")

    for i in range(6):
        assert got_gt[i] == pytest.approx(src_gt[i], abs=1e-8), "bad geotransform"

    assert cs == 50054, "bad checksum"


def test_jp2grok_4_vsimem():
    return test_jp2grok_4("/vsimem/jp2grok_4.jp2")


###############################################################################
# Test copying int16.jp2


def test_jp2grok_5():

    tst = gdaltest.GDALTest(
        "JP2Grok",
        "jpeg2000/int16.jp2",
        1,
        None,
        options=["REVERSIBLE=YES", "QUALITY=100", "CODEC=J2K"],
    )
    tst.testCreateCopy()


###############################################################################
# Test reading ll.jp2


def test_jp2grok_6():

    tst = gdaltest.GDALTest("JP2Grok", "jpeg2000/ll.jp2", 1, None)
    tst.testOpen()

    ds = gdal.Open("data/jpeg2000/ll.jp2")
    ds.GetRasterBand(1).Checksum()
    ds = None


###############################################################################
# Open byte.jp2.gz (test use of the VSIL API)


def test_jp2grok_7():

    tst = gdaltest.GDALTest(
        "JP2Grok",
        "/vsigzip/data/jpeg2000/byte.jp2.gz",
        1,
        50054,
        filename_absolute=1,
    )
    tst.testOpen()
    gdal.Unlink("data/jpeg2000/byte.jp2.gz.properties")


###############################################################################
# Test a JP2Grok with the 3 bands having 13bit depth and the 4th one 1 bit


def test_jp2grok_8():

    ds = gdal.Open("data/jpeg2000/3_13bit_and_1bit.jp2")

    expected_checksums = [64570, 57277, 56048, 61292]

    for i in range(4):
        assert (
            ds.GetRasterBand(i + 1).Checksum() == expected_checksums[i]
        ), "unexpected checksum (%d) for band %d" % (expected_checksums[i], i + 1)

    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16, "unexpected data type"


###############################################################################
# Check that we can use .j2w world files


def test_jp2grok_9():

    ds = gdal.Open("data/jpeg2000/byte_without_geotransform.jp2")

    geotransform = ds.GetGeoTransform()
    assert (
        geotransform[0] == pytest.approx(440720, abs=0.1)
        and geotransform[1] == pytest.approx(60, abs=0.001)
        and geotransform[2] == pytest.approx(0, abs=0.001)
        and geotransform[3] == pytest.approx(3751320, abs=0.1)
        and geotransform[4] == pytest.approx(0, abs=0.001)
        and geotransform[5] == pytest.approx(-60, abs=0.001)
    ), "geotransform differs from expected"

    ds = None


###############################################################################
# Test YCBCR420 creation option


def test_jp2grok_10():

    src_ds = gdal.Open("data/rgbsmall.tif")
    out_ds = gdaltest.jp2grok_drv.CreateCopy(
        "/vsimem/jp2grok_10.jp2", src_ds, options=["YCBCR420=YES", "RESOLUTIONS=3"]
    )
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    assert out_ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert out_ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert out_ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
    del out_ds
    src_ds = None
    gdal.Unlink("/vsimem/jp2grok_10.jp2")

    # Quite a bit of difference...
    assert maxdiff <= 38, "Image too different from reference"


###############################################################################
# Test auto-promotion of 1bit alpha band to 8bit


def test_jp2grok_11():

    ds = gdal.Open("data/jpeg2000/stefan_full_rgba_alpha_1bit.jp2")
    fourth_band = ds.GetRasterBand(4)
    assert fourth_band.GetMetadataItem("NBITS", "IMAGE_STRUCTURE") is None
    got_cs = fourth_band.Checksum()
    assert got_cs == 8527
    jp2_bands_data = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    jp2_fourth_band_data = fourth_band.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    fourth_band.ReadRaster(
        0,
        0,
        ds.RasterXSize,
        ds.RasterYSize,
        int(ds.RasterXSize / 16),
        int(ds.RasterYSize / 16),
    )

    tmp_ds = gdal.GetDriverByName("GTiff").CreateCopy("/vsimem/jp2grok_11.tif", ds)
    fourth_band = tmp_ds.GetRasterBand(4)
    got_cs = fourth_band.Checksum()
    gtiff_bands_data = tmp_ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    gtiff_fourth_band_data = fourth_band.ReadRaster(
        0, 0, ds.RasterXSize, ds.RasterYSize
    )
    tmp_ds = None
    gdal.GetDriverByName("GTiff").Delete("/vsimem/jp2grok_11.tif")
    assert got_cs == 8527

    assert jp2_bands_data == gtiff_bands_data
    assert jp2_fourth_band_data == gtiff_fourth_band_data

    ds = gdal.OpenEx(
        "data/jpeg2000/stefan_full_rgba_alpha_1bit.jp2",
        open_options=["1BIT_ALPHA_PROMOTION=NO"],
    )
    fourth_band = ds.GetRasterBand(4)
    assert fourth_band.GetMetadataItem("NBITS", "IMAGE_STRUCTURE") == "1"


###############################################################################
# Check that PAM overrides internal georeferencing


def test_jp2grok_12():

    # Override projection
    shutil.copy("data/jpeg2000/byte.jp2", "tmp/jp2grok_12.jp2")

    ds = gdal.Open("tmp/jp2grok_12.jp2")
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    ds.SetProjection(sr.ExportToWkt())
    ds = None

    ds = gdal.Open("tmp/jp2grok_12.jp2")
    wkt = ds.GetProjectionRef()
    ds = None

    gdaltest.jp2grok_drv.Delete("tmp/jp2grok_12.jp2")

    assert "32631" in wkt

    # Override geotransform
    shutil.copy("data/jpeg2000/byte.jp2", "tmp/jp2grok_12.jp2")

    ds = gdal.Open("tmp/jp2grok_12.jp2")
    ds.SetGeoTransform([1000, 1, 0, 2000, 0, -1])
    ds = None

    ds = gdal.Open("tmp/jp2grok_12.jp2")
    gt = ds.GetGeoTransform()
    ds = None

    gdaltest.jp2grok_drv.Delete("tmp/jp2grok_12.jp2")

    assert gt == (1000, 1, 0, 2000, 0, -1)


###############################################################################
# Check that PAM overrides internal GCPs


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_jp2grok_13():

    # Create a dataset with GCPs
    src_ds = gdal.Open("data/rgb_gcp.vrt")
    ds = gdaltest.jp2grok_drv.CreateCopy("tmp/jp2grok_13.jp2", src_ds)
    ds = None
    src_ds = None

    assert gdal.VSIStatL("tmp/jp2grok_13.jp2.aux.xml") is None

    ds = gdal.Open("tmp/jp2grok_13.jp2")
    count = ds.GetGCPCount()
    gcps = ds.GetGCPs()
    wkt = ds.GetGCPProjection()
    assert count == 4
    assert len(gcps) == 4
    assert "4326" in wkt
    ds = None

    # Override GCP
    ds = gdal.Open("tmp/jp2grok_13.jp2")
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    gcps = [gdal.GCP(0, 1, 2, 3, 4)]
    ds.SetGCPs(gcps, sr.ExportToWkt())
    ds = None

    ds = gdal.Open("tmp/jp2grok_13.jp2")
    count = ds.GetGCPCount()
    gcps = ds.GetGCPs()
    wkt = ds.GetGCPProjection()
    ds = None

    gdaltest.jp2grok_drv.Delete("tmp/jp2grok_13.jp2")

    assert count == 1
    assert len(gcps) == 1
    assert "32631" in wkt


###############################################################################
# Check that we get GCPs even there's no projection info


def test_jp2grok_14():

    ds = gdal.Open("data/jpeg2000/byte_2gcps.jp2")
    assert ds.GetGCPCount() == 2


###############################################################################
# Test multi-band reading with custom block size


def test_jp2grok_15():

    src_ds = gdal.GetDriverByName("MEM").Create("", 256, 256)
    src_ds.GetRasterBand(1).Fill(255)
    data = src_ds.ReadRaster()
    ds = gdaltest.jp2grok_drv.CreateCopy(
        "/vsimem/jp2grok_15.jp2",
        src_ds,
        options=["BLOCKXSIZE=33", "BLOCKYSIZE=34"],
    )
    src_ds = None
    got_data = ds.ReadRaster()
    ds = None
    gdaltest.jp2grok_drv.Delete("/vsimem/jp2grok_15.jp2")
    assert got_data == data


###############################################################################
# Test reading PixelIsPoint file


def test_jp2grok_16():

    ds = gdal.Open("data/jpeg2000/byte_point.jp2")
    gt = ds.GetGeoTransform()
    assert (
        ds.GetMetadataItem("AREA_OR_POINT") == "Point"
    ), "did not get AREA_OR_POINT = Point"
    ds = None

    gt_expected = (440690.0, 60.0, 0.0, 3751350.0, 0.0, -60.0)

    assert gt == gt_expected, "did not get expected geotransform"

    with gdal.config_option("GTIFF_POINT_GEO_IGNORE", "TRUE"):

        ds = gdal.Open("data/jpeg2000/byte_point.jp2")
        gt = ds.GetGeoTransform()
        ds = None

    gt_expected = (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)

    assert (
        gt == gt_expected
    ), "did not get expected geotransform with GTIFF_POINT_GEO_IGNORE TRUE"


###############################################################################
# Test writing PixelIsPoint file


def test_jp2grok_17():

    src_ds = gdal.Open("data/jpeg2000/byte_point.jp2")
    ds = gdaltest.jp2grok_drv.CreateCopy("/vsimem/jp2grok_17.jp2", src_ds)
    ds = None
    src_ds = None

    assert gdal.VSIStatL("/vsimem/jp2grok_17.jp2.aux.xml") is None

    ds = gdal.Open("/vsimem/jp2grok_17.jp2")
    gt = ds.GetGeoTransform()
    assert (
        ds.GetMetadataItem("AREA_OR_POINT") == "Point"
    ), "did not get AREA_OR_POINT = Point"
    ds = None

    gt_expected = (440690.0, 60.0, 0.0, 3751350.0, 0.0, -60.0)

    assert gt == gt_expected, "did not get expected geotransform"

    gdal.Unlink("/vsimem/jp2grok_17.jp2")


###############################################################################
# Test large tile decode area


def test_jp2grok_18():

    src_ds = gdal.GetDriverByName("Mem").Create("", 2000, 2000)
    ds = gdaltest.jp2grok_drv.CreateCopy(
        "/vsimem/jp2grok_18.jp2",
        src_ds,
        options=["BLOCKXSIZE=2000", "BLOCKYSIZE=2000"],
    )
    ds = None
    src_ds = None

    ds = gdal.Open("/vsimem/jp2grok_18.jp2")
    ds.GetRasterBand(1).Checksum()
    assert gdal.GetLastErrorMsg() == ""
    ds = None

    gdal.Unlink("/vsimem/jp2grok_18.jp2")


###############################################################################
# Test reading file where GMLJP2 has nul character instead of \n


def test_jp2grok_19():

    ds = gdal.Open("data/jpeg2000/byte_gmljp2_with_nul_car.jp2")
    assert ds.GetProjectionRef() != ""
    ds = None


###############################################################################
# Test YCC=NO creation option


def test_jp2grok_21():

    src_ds = gdal.Open("data/rgbsmall.tif")
    out_ds = gdaltest.jp2grok_drv.CreateCopy(
        "/vsimem/jp2grok_21.jp2",
        src_ds,
        options=["QUALITY=100", "REVERSIBLE=YES", "YCC=NO"],
    )
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    gdal.Unlink("/vsimem/jp2grok_21.jp2")

    # Quite a bit of difference...
    assert maxdiff <= 1, "Image too different from reference"


###############################################################################
# Test RGBA support


def test_jp2grok_22():

    # RGBA
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    out_ds = gdaltest.jp2grok_drv.CreateCopy(
        "/vsimem/jp2grok_22.jp2", src_ds, options=["QUALITY=100", "REVERSIBLE=YES"]
    )
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    assert gdal.VSIStatL("/vsimem/jp2grok_22.jp2.aux.xml") is None
    ds = gdal.Open("/vsimem/jp2grok_22.jp2")
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_AlphaBand
    ds = None
    gdal.Unlink("/vsimem/jp2grok_22.jp2")

    assert maxdiff <= 0, "Image too different from reference"

    # RGBA with 1BIT_ALPHA=YES
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    out_ds = gdaltest.jp2grok_drv.CreateCopy(
        "/vsimem/jp2grok_22.jp2", src_ds, options=["1BIT_ALPHA=YES"]
    )
    del out_ds
    src_ds = None
    assert gdal.VSIStatL("/vsimem/jp2grok_22.jp2.aux.xml") is None
    ds = gdal.OpenEx("/vsimem/jp2grok_22.jp2", open_options=["1BIT_ALPHA_PROMOTION=NO"])
    fourth_band = ds.GetRasterBand(4)
    assert fourth_band.GetMetadataItem("NBITS", "IMAGE_STRUCTURE") == "1"
    ds = None
    ds = gdal.Open("/vsimem/jp2grok_22.jp2")
    assert ds.GetRasterBand(4).Checksum() == 26477
    ds = None
    gdal.Unlink("/vsimem/jp2grok_22.jp2")

    # RGBA with YCBCR420=YES
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    out_ds = gdaltest.jp2grok_drv.CreateCopy(
        "/vsimem/jp2grok_22.jp2", src_ds, options=["YCBCR420=YES"]
    )
    del out_ds
    src_ds = None
    assert gdal.VSIStatL("/vsimem/jp2grok_22.jp2.aux.xml") is None
    ds = gdal.Open("/vsimem/jp2grok_22.jp2")
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_AlphaBand
    ds = None
    gdal.Unlink("/vsimem/jp2grok_22.jp2")

    # RGB,undefined
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba_photometric_rgb.tif")
    out_ds = gdaltest.jp2grok_drv.CreateCopy(
        "/vsimem/jp2grok_22.jp2", src_ds, options=["QUALITY=100", "REVERSIBLE=YES"]
    )
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    assert gdal.VSIStatL("/vsimem/jp2grok_22.jp2.aux.xml") is None
    ds = gdal.Open("/vsimem/jp2grok_22.jp2")
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_Undefined
    ds = None
    gdal.Unlink("/vsimem/jp2grok_22.jp2")

    assert maxdiff <= 0, "Image too different from reference"

    # RGB,undefined with ALPHA=YES
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba_photometric_rgb.tif")
    out_ds = gdaltest.jp2grok_drv.CreateCopy(
        "/vsimem/jp2grok_22.jp2",
        src_ds,
        options=["QUALITY=100", "REVERSIBLE=YES", "ALPHA=YES"],
    )
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    assert gdal.VSIStatL("/vsimem/jp2grok_22.jp2.aux.xml") is None
    ds = gdal.Open("/vsimem/jp2grok_22.jp2")
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_AlphaBand
    ds = None
    gdal.Unlink("/vsimem/jp2grok_22.jp2")

    assert maxdiff <= 0, "Image too different from reference"


###############################################################################
# Test NBITS support


def test_jp2grok_23():

    src_ds = gdal.Open("../gcore/data/uint16.tif")
    out_ds = gdaltest.jp2grok_drv.CreateCopy(
        "/vsimem/jp2grok_23.jp2",
        src_ds,
        options=["NBITS=9", "QUALITY=100", "REVERSIBLE=YES"],
    )
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    ds = gdal.Open("/vsimem/jp2grok_23.jp2")
    assert ds.GetRasterBand(1).GetMetadataItem("NBITS", "IMAGE_STRUCTURE") == "9"

    out_ds = gdaltest.jp2grok_drv.CreateCopy("/vsimem/jp2grok_23_2.jp2", ds)
    assert out_ds.GetRasterBand(1).GetMetadataItem("NBITS", "IMAGE_STRUCTURE") == "9"
    del out_ds

    ds = None
    assert gdal.VSIStatL("/vsimem/jp2grok_23.jp2.aux.xml") is None
    gdal.Unlink("/vsimem/jp2grok_23.jp2")
    gdal.Unlink("/vsimem/jp2grok_23_2.jp2")

    assert maxdiff <= 1, "Image too different from reference"


###############################################################################
# Test Grey+alpha support


def test_jp2grok_24():

    #  Grey+alpha
    src_ds = gdal.Open("../gcore/data/stefan_full_greyalpha.tif")
    out_ds = gdaltest.jp2grok_drv.CreateCopy(
        "/vsimem/jp2grok_24.jp2", src_ds, options=["QUALITY=100", "REVERSIBLE=YES"]
    )
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    assert gdal.VSIStatL("/vsimem/jp2grok_24.jp2.aux.xml") is None
    ds = gdal.Open("/vsimem/jp2grok_24.jp2")
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_GrayIndex
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_AlphaBand
    ds = None
    gdal.Unlink("/vsimem/jp2grok_24.jp2")

    assert maxdiff <= 0, "Image too different from reference"

    #  Grey+alpha with 1BIT_ALPHA=YES
    src_ds = gdal.Open("../gcore/data/stefan_full_greyalpha.tif")
    out_ds = gdaltest.jp2grok_drv.CreateCopy(
        "/vsimem/jp2grok_24.jp2", src_ds, options=["1BIT_ALPHA=YES"]
    )
    del out_ds
    src_ds = None
    assert gdal.VSIStatL("/vsimem/jp2grok_24.jp2.aux.xml") is None
    ds = gdal.OpenEx("/vsimem/jp2grok_24.jp2", open_options=["1BIT_ALPHA_PROMOTION=NO"])
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_GrayIndex
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_AlphaBand
    assert ds.GetRasterBand(2).GetMetadataItem("NBITS", "IMAGE_STRUCTURE") == "1"
    ds = None
    ds = gdal.Open("/vsimem/jp2grok_24.jp2")
    assert ds.GetRasterBand(2).GetMetadataItem("NBITS", "IMAGE_STRUCTURE") is None
    assert ds.GetRasterBand(2).Checksum() == 27389
    ds = None
    gdal.Unlink("/vsimem/jp2grok_24.jp2")


###############################################################################
# Test multiband support


def test_jp2grok_25():

    src_ds = gdal.GetDriverByName("MEM").Create("", 100, 100, 5)
    src_ds.GetRasterBand(1).Fill(255)
    src_ds.GetRasterBand(2).Fill(250)
    src_ds.GetRasterBand(3).Fill(245)
    src_ds.GetRasterBand(4).Fill(240)
    src_ds.GetRasterBand(5).Fill(235)

    out_ds = gdaltest.jp2grok_drv.CreateCopy(
        "/vsimem/jp2grok_25.jp2", src_ds, options=["QUALITY=100", "REVERSIBLE=YES"]
    )
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    ds = gdal.Open("/vsimem/jp2grok_25.jp2")
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_Undefined
    ds = None
    assert gdal.VSIStatL("/vsimem/jp2grok_25.jp2.aux.xml") is None

    gdal.Unlink("/vsimem/jp2grok_25.jp2")

    assert maxdiff <= 0, "Image too different from reference"


###############################################################################


def validate(
    filename,
    expected_gmljp2=True,
    return_error_count=False,
    oidoc=None,
    inspire_tg=True,
):

    for path in ("../ogr", samples_path):
        if path not in sys.path:
            sys.path.append(path)

    validate_jp2 = pytest.importorskip("validate_jp2")

    try:
        os.stat("tmp/cache/SCHEMAS_OPENGIS_NET")
        os.stat("tmp/cache/SCHEMAS_OPENGIS_NET/xlink.xsd")
        os.stat("tmp/cache/SCHEMAS_OPENGIS_NET/xml.xsd")
        ogc_schemas_location = "tmp/cache/SCHEMAS_OPENGIS_NET"
    except OSError:
        ogc_schemas_location = "disabled"

    if ogc_schemas_location != "disabled":
        try:
            import xmlvalidate

            xmlvalidate.validate  # to make pyflakes happy
        except (ImportError, AttributeError):
            ogc_schemas_location = "disabled"

    res = validate_jp2.validate(
        filename, oidoc, inspire_tg, expected_gmljp2, ogc_schemas_location
    )
    if return_error_count:
        return (res.error_count, res.warning_count)
    if res.error_count == 0 and res.warning_count == 0:
        return
    pytest.fail()


###############################################################################
# Test INSPIRE_TG support


def test_jp2grok_26():

    src_ds = gdal.GetDriverByName("MEM").Create("", 2048, 2048, 1)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.SetGeoTransform([450000, 1, 0, 5000000, 0, -1])

    # Nominal case: tiled
    out_ds = gdaltest.jp2grok_drv.CreateCopy(
        "/vsimem/jp2grok_26.jp2", src_ds, options=["INSPIRE_TG=YES"]
    )
    overview_count = out_ds.GetRasterBand(1).GetOverviewCount()
    assert (
        out_ds.GetRasterBand(1).GetOverview(overview_count - 1).XSize == 2 * 128
        and out_ds.GetRasterBand(1).GetOverview(overview_count - 1).YSize == 2 * 128
    )
    out_ds = None
    assert gdal.VSIStatL("/vsimem/jp2grok_26.jp2.aux.xml") is None
    assert validate("/vsimem/jp2grok_26.jp2") != "fail"
    gdal.Unlink("/vsimem/jp2grok_26.jp2")

    # Nominal case: untiled
    out_ds = gdaltest.jp2grok_drv.CreateCopy(
        "/vsimem/jp2grok_26.jp2",
        src_ds,
        options=["INSPIRE_TG=YES", "BLOCKXSIZE=2048", "BLOCKYSIZE=2048"],
    )
    overview_count = out_ds.GetRasterBand(1).GetOverviewCount()
    assert (
        out_ds.GetRasterBand(1).GetOverview(overview_count - 1).XSize == 128
        and out_ds.GetRasterBand(1).GetOverview(overview_count - 1).YSize == 128
    )
    gdal.ErrorReset()
    out_ds.GetRasterBand(1).Checksum()
    assert gdal.GetLastErrorMsg() == ""
    out_ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert gdal.GetLastErrorMsg() == ""
    out_ds = None
    assert gdal.VSIStatL("/vsimem/jp2grok_26.jp2.aux.xml") is None
    assert validate("/vsimem/jp2grok_26.jp2") != "fail"
    gdal.Unlink("/vsimem/jp2grok_26.jp2")

    # Nominal case: RGBA
    src_ds = gdal.GetDriverByName("MEM").Create("", 128, 128, 4)
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.SetGeoTransform([450000, 1, 0, 5000000, 0, -1])
    out_ds = gdaltest.jp2grok_drv.CreateCopy(
        "/vsimem/jp2grok_26.jp2", src_ds, options=["INSPIRE_TG=YES", "ALPHA=YES"]
    )
    out_ds = None
    ds = gdal.OpenEx("/vsimem/jp2grok_26.jp2", open_options=["1BIT_ALPHA_PROMOTION=NO"])
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_AlphaBand
    assert ds.GetRasterBand(4).GetMetadataItem("NBITS", "IMAGE_STRUCTURE") == "1"
    ds = None
    assert gdal.VSIStatL("/vsimem/jp2grok_26.jp2.aux.xml") is None
    assert validate("/vsimem/jp2grok_26.jp2") != "fail"
    gdal.Unlink("/vsimem/jp2grok_26.jp2")

    # Warning case: disabling JPX
    gdal.ErrorReset()
    with gdal.quiet_errors():
        out_ds = gdaltest.jp2grok_drv.CreateCopy(
            "/vsimem/jp2grok_26.jp2", src_ds, options=["INSPIRE_TG=YES", "JPX=NO"]
        )
    assert gdal.GetLastErrorMsg() != ""
    out_ds = None
    assert gdal.VSIStatL("/vsimem/jp2grok_26.jp2.aux.xml") is None
    res = validate("/vsimem/jp2grok_26.jp2", return_error_count=True)
    assert res == "skip" or res == (2, 0)
    gdal.Unlink("/vsimem/jp2grok_26.jp2")

    # Bilevel (1 bit)
    src_ds = gdal.GetDriverByName("MEM").Create("", 128, 128, 1)
    src_ds.GetRasterBand(1).SetMetadataItem("NBITS", "1", "IMAGE_STRUCTURE")
    out_ds = gdaltest.jp2grok_drv.CreateCopy(
        "/vsimem/jp2grok_26.jp2", src_ds, options=["INSPIRE_TG=YES"]
    )
    assert out_ds.GetRasterBand(1).GetMetadataItem("NBITS", "IMAGE_STRUCTURE") == "1"
    ds = None
    assert gdal.VSIStatL("/vsimem/jp2grok_26.jp2.aux.xml") is None
    assert validate("/vsimem/jp2grok_26.jp2", expected_gmljp2=False) != "fail"
    gdal.Unlink("/vsimem/jp2grok_26.jp2")

    # Auto-promotion 12->16 bits
    src_ds = gdal.GetDriverByName("MEM").Create("", 128, 128, 1, gdal.GDT_UInt16)
    src_ds.GetRasterBand(1).SetMetadataItem("NBITS", "12", "IMAGE_STRUCTURE")
    out_ds = gdaltest.jp2grok_drv.CreateCopy(
        "/vsimem/jp2grok_26.jp2", src_ds, options=["INSPIRE_TG=YES"]
    )
    assert out_ds.GetRasterBand(1).GetMetadataItem("NBITS", "IMAGE_STRUCTURE") is None
    ds = None
    assert gdal.VSIStatL("/vsimem/jp2grok_26.jp2.aux.xml") is None
    assert validate("/vsimem/jp2grok_26.jp2", expected_gmljp2=False) != "fail"
    gdal.Unlink("/vsimem/jp2grok_26.jp2")

    src_ds = gdal.GetDriverByName("MEM").Create("", 2048, 2048, 1)

    # Error case: too big tile
    with gdal.quiet_errors():
        out_ds = gdaltest.jp2grok_drv.CreateCopy(
            "/vsimem/jp2grok_26.jp2",
            src_ds,
            options=["INSPIRE_TG=YES", "BLOCKXSIZE=1536", "BLOCKYSIZE=1536"],
        )
    assert out_ds is None

    # Error case: non square tile
    with gdal.quiet_errors():
        out_ds = gdaltest.jp2grok_drv.CreateCopy(
            "/vsimem/jp2grok_26.jp2",
            src_ds,
            options=["INSPIRE_TG=YES", "BLOCKXSIZE=512", "BLOCKYSIZE=128"],
        )
    assert out_ds is None

    # Error case: incompatible PROFILE
    with gdal.quiet_errors():
        out_ds = gdaltest.jp2grok_drv.CreateCopy(
            "/vsimem/jp2grok_26.jp2",
            src_ds,
            options=["INSPIRE_TG=YES", "PROFILE=UNRESTRICTED"],
        )
    assert out_ds is None

    # Error case: valid but too small number of resolutions for PROFILE_1
    with gdal.quiet_errors():
        out_ds = gdaltest.jp2grok_drv.CreateCopy(
            "/vsimem/jp2grok_26.jp2",
            src_ds,
            options=["INSPIRE_TG=YES", "RESOLUTIONS=1"],
        )
    assert out_ds is None

    # Too big resolution number — will fallback to default
    with gdal.quiet_errors():
        out_ds = gdaltest.jp2grok_drv.CreateCopy(
            "/vsimem/jp2grok_26.jp2",
            src_ds,
            options=["INSPIRE_TG=YES", "RESOLUTIONS=100"],
        )
    assert out_ds is not None
    out_ds = None
    gdal.Unlink("/vsimem/jp2grok_26.jp2")

    # Error case: unsupported NBITS
    with gdal.quiet_errors():
        out_ds = gdaltest.jp2grok_drv.CreateCopy(
            "/vsimem/jp2grok_26.jp2", src_ds, options=["INSPIRE_TG=YES", "NBITS=2"]
        )
    assert out_ds is None

    # Error case: unsupported CODEC (J2K)
    with gdal.quiet_errors():
        out_ds = gdaltest.jp2grok_drv.CreateCopy(
            "/vsimem/jp2grok_26.j2k", src_ds, options=["INSPIRE_TG=YES"]
        )
    assert out_ds is None

    # Error case: invalid CODEBLOCK_WIDTH/HEIGHT
    with gdal.quiet_errors():
        out_ds = gdaltest.jp2grok_drv.CreateCopy(
            "/vsimem/jp2grok_26.jp2",
            src_ds,
            options=["INSPIRE_TG=YES", "CODEBLOCK_WIDTH=128", "CODEBLOCK_HEIGHT=32"],
        )
    assert out_ds is None
    with gdal.quiet_errors():
        out_ds = gdaltest.jp2grok_drv.CreateCopy(
            "/vsimem/jp2grok_26.jp2",
            src_ds,
            options=["INSPIRE_TG=YES", "CODEBLOCK_WIDTH=32", "CODEBLOCK_HEIGHT=128"],
        )
    assert out_ds is None


###############################################################################
# Test CreateCopy() from a JPEG2000 with a 2048x2048 tiling


def test_jp2grok_27():

    src_ds = gdal.GetDriverByName("MEM").Create("", 2049, 2049, 4)
    out_ds = gdaltest.jp2grok_drv.CreateCopy(
        "/vsimem/jp2grok_27.jp2",
        src_ds,
        options=["RESOLUTIONS=1", "BLOCKXSIZE=2048", "BLOCKYSIZE=2048"],
    )
    src_ds = None
    out2_ds = gdal.GetDriverByName("GTiff").CreateCopy(
        "/vsimem/jp2grok_27.tif", out_ds, options=["TILED=YES"]
    )
    out_ds = None
    del out2_ds
    gdal.Unlink("/vsimem/jp2grok_27.jp2")
    gdal.Unlink("/vsimem/jp2grok_27.tif")


###############################################################################
# Test lossless round-trip of byte data


def test_jp2grok_lossless_byte():

    src_ds = gdal.Open("data/byte.tif")
    out_ds = gdaltest.jp2grok_drv.CreateCopy(
        "/vsimem/jp2grok_lossless.jp2",
        src_ds,
        options=["REVERSIBLE=YES", "QUALITY=100"],
    )
    assert (
        out_ds.GetMetadataItem("COMPRESSION_REVERSIBILITY", "IMAGE_STRUCTURE")
        == "LOSSLESS"
    )
    maxdiff = gdaltest.compare_ds(src_ds, out_ds)
    del out_ds
    src_ds = None
    gdal.Unlink("/vsimem/jp2grok_lossless.jp2")
    assert maxdiff == 0, "Lossless round-trip failed"


###############################################################################
# Test multi-tile multi-row round-trip via DirectRasterIO swath path.
# Regression test for TileCompletion releasing tile images too early:
# TileCache::release() must respect tile_cache_strategy so that
# GRK_TILE_CACHE_IMAGE keeps per-tile images alive until copied.


def test_jp2grok_multitile_multirow(tmp_vsimem):
    gdaltest.importorskip_gdal_array()
    np = pytest.importorskip("numpy")

    # 256x256 image with 64x64 tiles → 4x4 tile grid (4 tile rows)
    width, height = 256, 256
    src_ds = gdal.GetDriverByName("MEM").Create("", width, height, 3, gdal.GDT_Byte)
    # Fill each band with a distinct gradient so we can detect corruption
    for b in range(3):
        band = src_ds.GetRasterBand(b + 1)
        arr = np.empty((height, width), dtype=np.uint8)
        for y in range(height):
            arr[y, :] = (y + b * 80) % 256
        band.WriteArray(arr)

    out_path = tmp_vsimem / "jp2grok_multitile_multirow.jp2"
    out_ds = gdaltest.jp2grok_drv.CreateCopy(
        out_path,
        src_ds,
        options=["BLOCKXSIZE=64", "BLOCKYSIZE=64", "REVERSIBLE=YES", "QUALITY=100"],
    )
    del out_ds

    # Re-open and read via DirectRasterIO (triggers async swath decompress)
    ds = gdal.Open(out_path)
    assert ds is not None
    for b in range(3):
        ref_arr = np.empty((height, width), dtype=np.uint8)
        for y in range(height):
            ref_arr[y, :] = (y + b * 80) % 256
        got_arr = ds.GetRasterBand(b + 1).ReadAsArray()
        assert np.array_equal(
            ref_arr, got_arr
        ), f"Band {b + 1} data mismatch after multi-row tile decompress"
    ds = None
    src_ds = None


###############################################################################
# Test DirectRasterIO decompression path into a Float32/Float64 buffer


def test_jp2grok_directrasterio_float():
    gdaltest.importorskip_gdal_array()
    np = pytest.importorskip("numpy")

    ds = gdal.Open("data/jpeg2000/byte.jp2")
    assert ds is not None

    arr_f32 = ds.GetRasterBand(1).ReadAsArray(buf_type=gdal.GDT_Float32)
    assert arr_f32.dtype == np.float32

    arr_u8 = ds.GetRasterBand(1).ReadAsArray()
    assert np.array_equal(arr_f32, arr_u8.astype(np.float32))

    arr_f64 = ds.GetRasterBand(1).ReadAsArray(buf_type=gdal.GDT_Float64)
    assert arr_f64.dtype == np.float64

    arr_u8 = ds.GetRasterBand(1).ReadAsArray()
    assert np.array_equal(arr_f64, arr_u8.astype(np.float64))


###############################################################################
# Test in-place rewrite of JP2 boxes via GA_Update


def test_jp2grok_rewrite_boxes(tmp_path):
    gdaltest.importorskip_gdal_array()
    np = pytest.importorskip("numpy")

    tmpfile = tmp_path / "jp2grok_rewrite.jp2"

    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)

    # Create a file with known pixel data and no georef
    src_ds = gdal.GetDriverByName("MEM").Create("", 20, 20)

    pixel_data = np.arange(20 * 20, dtype=np.uint8).reshape(20, 20)
    src_ds.GetRasterBand(1).WriteArray(pixel_data)
    out_ds = gdaltest.jp2grok_drv.CreateCopy(
        tmpfile, src_ds, options=["REVERSIBLE=YES"]
    )
    del out_ds
    del src_ds

    # Add geotransform and projection
    ds = gdal.Open(tmpfile, gdal.GA_Update)
    ds.SetSpatialRef(sr)
    ds.SetGeoTransform([0, 1, 0, 3, 0, -1])
    ds = None

    # Check they were written into the file (not PAM)
    assert gdal.VSIStatL(tmp_path / ".aux.xml") is None
    ds = gdal.Open(tmpfile)
    assert ds.GetSpatialRef().GetAuthorityCode() == "32631"
    assert ds.GetGeoTransform() == (0, 1, 0, 3, 0, -1)
    # Verify pixel data survived the transcode
    got = ds.GetRasterBand(1).ReadAsArray()
    assert np.array_equal(got, pixel_data)
    ds = None

    # Modify geotransform
    ds = gdal.Open(tmpfile, gdal.GA_Update)
    ds.SetGeoTransform([1, 2, 0, 4, 0, -2])
    ds = None
    assert gdal.VSIStatL(tmp_path / ".aux.xml") is None

    ds = gdal.Open(tmpfile)
    assert ds.GetGeoTransform() == (1, 2, 0, 4, 0, -2)
    assert np.array_equal(ds.GetRasterBand(1).ReadAsArray(), pixel_data)
    ds = None

    # Replace projection+geotransform with GCPs
    ds = gdal.Open(tmpfile, gdal.GA_Update)
    gcps = [gdal.GCP(0, 1, 2, 3, 4)]
    ds.SetGCPs(gcps, sr.ExportToWkt())
    ds = None
    assert gdal.VSIStatL(tmp_path / ".aux.xml") is None

    ds = gdal.Open(tmpfile)
    assert ds.GetGCPCount() == 1
    got_gcp = ds.GetGCPs()[0]
    assert got_gcp.GCPX == 0
    assert got_gcp.GCPY == 1
    assert got_gcp.GCPZ == 2
    assert got_gcp.GCPPixel == 3
    assert got_gcp.GCPLine == 4
    assert ds.GetGCPSpatialRef().GetAuthorityCode() == "32631"
    ds = None

    # Add metadata
    ds = gdal.Open(tmpfile, gdal.GA_Update)
    ds.SetMetadataItem("FOO", "BAR")
    ds = None
    assert gdal.VSIStatL(tmp_path / ".aux.xml") is None

    ds = gdal.Open(tmpfile)
    assert ds.GetMetadata() == {"FOO": "BAR"}
    ds = None

    # Add IPR box
    ds = gdal.Open(tmpfile, gdal.GA_Update)
    ds.SetMetadata(["<fake_ipr_box/>"], "xml:IPR")
    ds = None
    assert gdal.VSIStatL(tmp_path / ".aux.xml") is None

    ds = gdal.Open(tmpfile)
    assert ds.GetMetadata("xml:IPR")[0] == "<fake_ipr_box/>"
    assert np.array_equal(ds.GetRasterBand(1).ReadAsArray(), pixel_data)
    ds = None


###############################################################################
# Test driver metadata


###############################################################################
# Helper: build a small georeferenced source JP2 for transcode tests.


def _make_transcode_source(tmp_path, epsg, gt, src_options=None):
    np = pytest.importorskip("numpy")
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(epsg)
    mem_ds = gdal.GetDriverByName("MEM").Create("", 20, 20)
    pixel_data = np.arange(20 * 20, dtype=np.uint8).reshape(20, 20)
    mem_ds.GetRasterBand(1).WriteArray(pixel_data)
    mem_ds.SetSpatialRef(sr)
    mem_ds.SetGeoTransform(list(gt))

    src_jp2 = str(tmp_path / "transcode_src.jp2")
    out_ds = gdaltest.jp2grok_drv.CreateCopy(
        src_jp2, mem_ds, options=(src_options or ["REVERSIBLE=YES"])
    )
    del out_ds
    del mem_ds
    return src_jp2, pixel_data


###############################################################################
# Test transcode with PLT/TLM/SOP/EPH markers and progression order changes.
#
# A single parametrized test verifies that:
#  - each marker option actually causes the corresponding marker to be
#    written (checked via GetJPEG2000StructureAsString)
#  - PROGRESSION actually changes the COD SGcod_Progress field
#  - pixel data and georeferencing are preserved across transcode


@pytest.mark.parametrize(
    "options,expect_plt,expect_tlm,expect_sop,expect_eph,expect_progression",
    [
        # PLT + TLM only
        (["PLT=YES", "TLM=YES"], True, True, False, False, None),
        # SOP + EPH only
        ([], False, False, False, False, None),
        (["SOP=YES", "EPH=YES"], False, False, True, True, None),
        # Progression-order change (RPCL); also keep PLT/TLM on to exercise
        # the combined code path
        (
            ["PLT=YES", "TLM=YES", "PROGRESSION=RPCL"],
            True,
            True,
            False,
            False,
            "RPCL",
        ),
    ],
    ids=["plt_tlm", "default", "sop_eph", "progression_rpcl"],
)
def test_jp2grok_transcode_markers(
    tmp_path,
    options,
    expect_plt,
    expect_tlm,
    expect_sop,
    expect_eph,
    expect_progression,
):
    gdaltest.importorskip_gdal_array()
    np = pytest.importorskip("numpy")

    src_jp2, pixel_data = _make_transcode_source(
        tmp_path, 32631, (500000, 1, 0, 5000000, 0, -1)
    )

    dst_jp2 = str(tmp_path / "transcode_dst.jp2")
    src_ds = gdal.Open(src_jp2)
    out_ds = gdaltest.jp2grok_drv.CreateCopy(
        dst_jp2, src_ds, options=["TRANSCODE=YES"] + options
    )
    del out_ds
    del src_ds

    # Pixel data and georeferencing must survive transcode
    ds = gdal.Open(dst_jp2)
    assert ds is not None
    assert ds.GetSpatialRef().GetAuthorityCode() == "32631"
    assert ds.GetGeoTransform() == (500000, 1, 0, 5000000, 0, -1)
    assert np.array_equal(ds.GetRasterBand(1).ReadAsArray(), pixel_data)
    ds = None

    # Check that the requested markers and progression were actually written
    structure = gdal.GetJPEG2000StructureAsString(dst_jp2, ["ALL=YES"])

    if expect_plt:
        assert '<Marker name="PLT"' in structure
    else:
        assert '<Marker name="PLT"' not in structure

    if expect_tlm:
        assert '<Marker name="TLM"' in structure
    else:
        assert '<Marker name="TLM"' not in structure

    # SOP/EPH are encoded in the COD Scod field's description text.
    if expect_sop:
        assert "SOP marker segments may be used" in structure
    else:
        assert "No SOP marker segments" in structure
    if expect_eph:
        assert "EPH marker segments may be used" in structure
    else:
        assert "No EPH marker segments" in structure

    if expect_progression is not None:
        assert (
            f'<Field name="SGcod_Progress" type="uint8" '
            f'description="{expect_progression}">' in structure
        )


###############################################################################
# Test transcode from the existing byte.jp2 test fixture.
#
# Unlike the parametrized cases above, this exercises transcode on a real
# third-party JP2 with a GeoTIFF UUID box already present, checking that
# metadata (SRS, GeoTransform) and pixel data survive a round-trip through
# grk_transcode() when the source was NOT created by the Grok driver itself.
# PLT=YES is included to exercise PLT insertion on a file that did not
# originally carry a PLT marker.


def test_jp2grok_transcode_byte(tmp_path):

    src_ds = gdal.Open("data/jpeg2000/byte.jp2")
    src_wkt = src_ds.GetProjectionRef()
    src_gt = src_ds.GetGeoTransform()
    src_cs = src_ds.GetRasterBand(1).Checksum()

    dst_jp2 = str(tmp_path / "transcode_byte.jp2")
    out_ds = gdaltest.jp2grok_drv.CreateCopy(
        dst_jp2, src_ds, options=["TRANSCODE=YES", "PLT=YES"]
    )
    del out_ds
    del src_ds

    ds = gdal.Open(dst_jp2)
    assert ds is not None

    sr1 = osr.SpatialReference()
    sr1.SetFromUserInput(ds.GetProjectionRef())
    sr2 = osr.SpatialReference()
    sr2.SetFromUserInput(src_wkt)
    assert sr1.IsSame(sr2), "bad spatial reference after transcode"

    got_gt = ds.GetGeoTransform()
    for i in range(6):
        assert got_gt[i] == pytest.approx(src_gt[i], abs=1e-8)

    assert ds.GetRasterBand(1).Checksum() == src_cs
    ds = None

    # PLT=YES should have actually inserted a PLT marker
    structure = gdal.GetJPEG2000StructureAsString(dst_jp2, ["ALL=YES"])
    assert '<Marker name="PLT"' in structure


###############################################################################
# Test transcode warns about ignored options


def test_jp2grok_transcode_ignored_options(tmp_path):

    src_ds = gdal.Open("data/jpeg2000/byte.jp2")
    dst_jp2 = str(tmp_path / "transcode_ignored.jp2")

    with gdaltest.error_raised(gdal.CE_Warning, match="ignored when TRANSCODE=YES"):
        out_ds = gdaltest.jp2grok_drv.CreateCopy(
            dst_jp2,
            src_ds,
            options=["TRANSCODE=YES", "QUALITY=50", "REVERSIBLE=YES"],
        )
        del out_ds

    del src_ds

    # Verify the file was still created successfully despite warnings
    ds = gdal.Open(dst_jp2)
    assert ds is not None
    ds = None


###############################################################################
# Test overview decode returns correct data (not a top-left crop)


def test_jp2grok_overview_decode():
    np = pytest.importorskip("numpy")

    # Create a 256x256 image with a diagonal gradient so each quadrant
    # has a distinct mean value, making it easy to detect a crop bug.
    src_ds = gdal.GetDriverByName("MEM").Create("", 256, 256, 1, gdal.GDT_Byte)
    arr = np.zeros((256, 256), dtype=np.uint8)
    for y in range(256):
        for x in range(256):
            arr[y, x] = (x + y) * 255 // 510  # 0 at (0,0) .. 255 at (255,255)
    src_ds.GetRasterBand(1).WriteArray(arr)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.SetGeoTransform([450000, 1, 0, 5000000, 0, -1])

    # Encode lossless with 5-3 wavelet so overviews are exact
    fname = "/vsimem/test_jp2grok_overview_decode.jp2"
    out_ds = gdaltest.jp2grok_drv.CreateCopy(
        fname, src_ds, options=["REVERSIBLE=YES", "QUALITY=100"]
    )
    del out_ds, src_ds

    ds = gdal.Open(fname)
    assert ds is not None
    band = ds.GetRasterBand(1)
    ov_count = band.GetOverviewCount()
    assert ov_count > 0, "Expected at least one overview level"

    # The full-resolution checksum
    full_cs = band.Checksum()
    assert full_cs == 53143

    # Read overview level 0 (factor-of-2 reduction => 128×128)
    ov0 = band.GetOverview(0)
    assert ov0.XSize == 128 and ov0.YSize == 128

    ov_data = ov0.ReadAsArray()

    # The mean of the diagonal gradient at full res is ~127.
    # At the overview the mean should be similar.
    ov_mean = float(np.mean(ov_data))
    assert (
        100 < ov_mean < 155
    ), f"Overview mean {ov_mean} out of range — likely a crop bug"

    # Check that bottom-right quadrant is brighter than top-left quadrant
    tl_mean = float(np.mean(ov_data[:64, :64]))
    br_mean = float(np.mean(ov_data[64:, 64:]))
    assert br_mean > tl_mean + 30, (
        f"Bottom-right mean ({br_mean}) should be much larger "
        f"than top-left mean ({tl_mean}) — overview may be a crop"
    )

    # Also test ReadRaster at overview resolution via the main band
    data = band.ReadRaster(0, 0, 256, 256, buf_xsize=128, buf_ysize=128)
    assert data == ov_data

    ds = None
    gdal.Unlink(fname)


###############################################################################
# Test overview decode on a multi-tiled image.
# Regression test for coordinate-space mismatch in CopyTileData when
# iLevel > 0: Grok returns tile img->x0/y0 in full-resolution coordinates,
# but the request window is in reduced overview space.  Without the fix,
# tiles not at the origin are skipped and the overview is mostly zero.


def test_jp2grok_multitile_overview_decode():
    np = pytest.importorskip("numpy")

    # Create a 512×512 3-band image with a distinctive pattern:
    # each pixel = (x % 256, y % 256, (x+y) % 256) so every region
    # has non-zero data and is verifiable.
    width = 512
    height = 512
    nbands = 3
    src_ds = gdal.GetDriverByName("MEM").Create(
        "", width, height, nbands, gdal.GDT_Byte
    )
    for b in range(nbands):
        arr = np.zeros((height, width), dtype=np.uint8)
        for y in range(height):
            for x in range(width):
                if b == 0:
                    arr[y, x] = x * 255 // (width - 1)
                elif b == 1:
                    arr[y, x] = y * 255 // (height - 1)
                else:
                    arr[y, x] = (x + y) * 255 // (width + height - 2)
        src_ds.GetRasterBand(b + 1).WriteArray(arr)

    # Encode lossless with small tiles (128×128) to create a 4×4 tile grid,
    # and 3 resolution levels so we get at least 2 overview levels.
    fname = "/vsimem/test_jp2grok_multitile_overview_decode.jp2"
    out_ds = gdaltest.jp2grok_drv.CreateCopy(
        fname,
        src_ds,
        options=[
            "REVERSIBLE=YES",
            "QUALITY=100",
            "BLOCKXSIZE=128",
            "BLOCKYSIZE=128",
            "RESOLUTIONS=3",
        ],
    )
    del out_ds, src_ds

    ds = gdal.Open(fname)
    assert ds is not None
    assert ds.RasterXSize == width
    assert ds.RasterYSize == height
    band = ds.GetRasterBand(1)
    ov_count = band.GetOverviewCount()
    assert ov_count >= 1, "Expected at least one overview level"

    # Test each overview level
    for ov_idx in range(ov_count):
        ov = band.GetOverview(ov_idx)
        ov_data = ov.ReadAsArray()
        ov_w = ov.XSize
        ov_h = ov.YSize
        assert ov_w > 0 and ov_h > 0

        # The x-gradient pattern means the mean of band 1 should be ~127
        # (half of 0..255 cycle).  If overview tiles are skipped, the mean
        # drops dramatically because skipped regions are zero-filled.
        ov_mean = float(np.mean(ov_data))
        assert ov_mean > 80, (
            f"Overview {ov_idx} ({ov_w}x{ov_h}) band 1 mean {ov_mean:.1f} "
            f"is too low — tiles likely skipped due to coordinate mismatch"
        )

        # Verify that the right half has a higher mean than the left half
        # (x-gradient: left starts near 0, right near 255).
        mid = ov_w // 2
        left_mean = float(np.mean(ov_data[:, :mid]))
        right_mean = float(np.mean(ov_data[:, mid:]))
        assert right_mean > left_mean + 20, (
            f"Overview {ov_idx}: right-half mean ({right_mean:.1f}) should "
            f"exceed left-half mean ({left_mean:.1f}) — data may be corrupt"
        )

        # Verify that no large contiguous zero blocks exist (which would
        # indicate missing tiles).  At overview level, every 8×8 block
        # should have some non-zero pixels.
        block = 8
        for by in range(0, ov_h - block + 1, block):
            for bx in range(0, ov_w - block + 1, block):
                patch = ov_data[by : by + block, bx : bx + block]
                assert np.any(patch > 0), (
                    f"Overview {ov_idx}: all-zero block at ({bx},{by}) "
                    f"— tile data likely missing"
                )

    # Also test band 2 (y-gradient) at overview 0
    ov2 = ds.GetRasterBand(2).GetOverview(0)
    ov2_data = ov2.ReadAsArray()
    top_mean = float(np.mean(ov2_data[: ov2.YSize // 2, :]))
    bot_mean = float(np.mean(ov2_data[ov2.YSize // 2 :, :]))
    assert bot_mean > top_mean + 20, (
        f"Band 2 overview: bottom mean ({bot_mean:.1f}) should exceed "
        f"top mean ({top_mean:.1f}) — y-gradient not preserved"
    )

    ds = None
    gdal.Unlink(fname)


###############################################################################
# Test driver metadata


def test_jp2grok_driver_metadata():

    drv = gdal.GetDriverByName("JP2Grok")
    assert drv is not None
    assert drv.GetMetadataItem(gdal.DCAP_RASTER) == "YES"
    assert drv.GetMetadataItem(gdal.DCAP_VIRTUALIO) == "YES"
    assert drv.GetMetadataItem(gdal.DCAP_CREATECOPY) == "YES"
    assert "jp2" in drv.GetMetadataItem(gdal.DMD_EXTENSIONS)
    assert "j2k" in drv.GetMetadataItem(gdal.DMD_EXTENSIONS)


###############################################################################
# Sync-decompress fallback (no AdviseRead before a partial read).
#
# Without AdviseRead the async path locks decode window to first
# swath, so later reads outside initial decode window returned zeros.
# The driver now falls back to a per-swath synchronous decode.
# Multi-tile-row sources are used so we detect these zeros if still broken.

SYNC_SRC = "data/jpeg2000/513x513.jp2"


def _truth(src):
    # Reference pixels from use of correct AdviseRead async path.
    ds = gdal.Open(src)
    ds.AdviseRead(0, 0, ds.RasterXSize, ds.RasterYSize)
    return ds.GetRasterBand(1).ReadAsArray()


def test_jp2grok_sync_fallback_full_image():
    # Full-image read without AdviseRead - must match async result.
    np = pytest.importorskip("numpy")
    ds = gdal.Open(SYNC_SRC)
    got = ds.GetRasterBand(1).ReadAsArray()
    assert np.array_equal(got, _truth(SYNC_SRC))


def test_jp2grok_sync_fallback_many_swaths():
    # Disjoint partial reads recreate codec and must match truth.
    np = pytest.importorskip("numpy")
    truth = _truth(SYNC_SRC)
    ds = gdal.Open(SYNC_SRC)
    band = ds.GetRasterBand(1)
    for x in (0, 150, 300):
        for y in (0, 150, 300):
            got = band.ReadAsArray(x, y, 100, 100)
            assert np.array_equal(got, truth[y : y + 100, x : x + 100])


def test_jp2grok_advise_read_keeps_async_path():
    # With AdviseRead the driver stays async: no warning, correct pixels.
    np = pytest.importorskip("numpy")
    truth = _truth(SYNC_SRC)
    ds = gdal.Open(SYNC_SRC)
    ds.AdviseRead(0, 0, 200, 200)
    gdal.ErrorReset()
    got = ds.GetRasterBand(1).ReadAsArray(0, 0, 200, 200)
    assert np.array_equal(got, truth[0:200, 0:200])


def test_jp2grok_vrt_read_no_advise():
    # VRT SimpleSource reads via RasterIO without AdviseRead; this exercises
    # fallback through the VRT front end.
    pytest.importorskip("numpy")
    src = "data/jpeg2000/tile_size_16.jp2"
    vrt = f"""<VRTDataset rasterXSize="256" rasterYSize="256">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="0">{src}</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="256" ySize="256" />
      <DstRect xOff="0" yOff="0" xSize="256" ySize="256" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>"""
    ds = gdal.Open(vrt)
    arr = ds.GetRasterBand(1).ReadAsArray()
    assert arr[:128].max() > 0 and arr[128:].max() > 0


def test_jp2grok_translate_scale_multi_tile_row():
    # gdal_translate -scale wraps source in a VRT and
    # reads it without AdviseRead. Identity scale preserves pixels, so the
    # output must match source below first tile row.
    np = pytest.importorskip("numpy")
    truth = _truth(SYNC_SRC)
    src_ds = gdal.Open(SYNC_SRC)
    smin, smax, _, _ = src_ds.GetRasterBand(1).GetStatistics(False, True)
    out = "/vsimem/jp2grok_scale.tif"
    ds = gdal.Translate(
        out, SYNC_SRC, format="GTiff", scaleParams=[[smin, smax, smin, smax]]
    )
    assert ds.RasterYSize == 513
    assert np.array_equal(ds.GetRasterBand(1).ReadAsArray(), truth)
    ds = None
    gdal.Unlink(out)


def test_jp2grok_statistics_16bit_multi_tile():
    # 16-bit uses Grok's int32_t code path
    # GetStatistics reads with ReadBlock, using full/partial tiles.
    np = pytest.importorskip("numpy")
    arr = (np.arange(300 * 300, dtype=np.uint16) % 4096).reshape(300, 300)
    src_ds = gdal.GetDriverByName("MEM").Create("", 300, 300, 1, gdal.GDT_UInt16)
    src_ds.GetRasterBand(1).WriteArray(arr)
    out = "/vsimem/jp2grok_16bit_tiled.jp2"
    gdal.GetDriverByName("JP2Grok").CreateCopy(
        out,
        src_ds,
        options=["REVERSIBLE=YES", "QUALITY=100", "BLOCKXSIZE=128", "BLOCKYSIZE=128"],
    )
    ds = gdal.Open(out)
    assert ds.GetRasterBand(1).GetBlockSize() == [128, 128]
    smin, smax, smean, _ = ds.GetRasterBand(1).GetStatistics(False, True)
    assert (smin, smax) == (float(arr.min()), float(arr.max()))
    assert smean == pytest.approx(float(arr.mean()))
    ds = None
    gdal.Unlink(out)


def _jp2grok_checksums_in_subprocess(src, num_threads):
    # The driver resolves Grok's thread count once per process (std::call_once),
    # so single-threaded mode must be exercised in a fresh process
    import subprocess

    code = (
        "import sys; from osgeo import gdal; gdal.UseExceptions();"
        "ds = gdal.OpenEx(sys.argv[1], allowed_drivers=['JP2Grok']);"
        "print(' '.join(str(ds.GetRasterBand(b + 1).Checksum())"
        " for b in range(ds.RasterCount)))"
    )
    env = dict(os.environ)
    env["GDAL_NUM_THREADS"] = str(num_threads)
    out = subprocess.check_output(
        [sys.executable, "-c", code, os.path.abspath(src)], env=env
    )
    return out.decode().strip()


@pytest.mark.parametrize(
    "src", ["data/jpeg2000/byte.jp2", "data/jpeg2000/tile_size_16.jp2"]
)
def test_jp2grok_single_threaded_matches_multi(src):
    # GDAL_NUM_THREADS=1 runs Grok fully single-threaded
    # output must match multi-threaded decode.
    if not os.path.exists(src):
        pytest.skip(f"{src} not available")
    st = _jp2grok_checksums_in_subprocess(src, 1)
    mt = _jp2grok_checksums_in_subprocess(src, 8)
    assert st == mt


def test_jp2grok_reuse_dataset_consecutive_reads():
    # Two reads on the same dataset instance must both be correct: the
    # single-shot codec is rebuilt for the second operation.
    np = pytest.importorskip("numpy")
    truth = _truth(SYNC_SRC)
    ds = gdal.Open(SYNC_SRC)
    out1 = gdal.Translate("", ds, format="MEM")
    out2 = gdal.Translate("", ds, format="MEM")
    assert np.array_equal(out1.GetRasterBand(1).ReadAsArray(), truth)
    assert np.array_equal(out2.GetRasterBand(1).ReadAsArray(), truth)


def test_jp2grok_partial_reads_after_full_no_advise():
    # Full read (drains the codec) then partial reads on the same instance
    # without a new AdviseRead: each must rebuild/reuse correctly.
    np = pytest.importorskip("numpy")
    truth = _truth(SYNC_SRC)
    ds = gdal.Open(SYNC_SRC)
    full = gdal.Translate("", ds, format="MEM")
    assert np.array_equal(full.GetRasterBand(1).ReadAsArray(), truth)
    band = ds.GetRasterBand(1)
    for x, y, w, h in [(0, 0, 100, 100), (200, 200, 100, 100), (0, 400, 100, 100)]:
        got = band.ReadAsArray(x, y, w, h)
        assert np.array_equal(got, truth[y : y + h, x : x + w])
