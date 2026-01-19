#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  PDF Testing.
# Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2010-2019, Even Rouault <even.rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json
import os
import shutil
import sys

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_driver("PDF")

###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################
# Test driver presence


@pytest.fixture(params=["POPPLER", "PODOFO", "PDFIUM"])
def poppler_or_pdfium_or_podofo(request):
    """
    Runs tests with all three backends.
    """
    backend = request.param
    gdaltest.pdf_drv = gdal.GetDriverByName("PDF")

    md = gdaltest.pdf_drv.GetMetadata()
    if "HAVE_%s" % backend not in md:
        pytest.skip()

    with gdaltest.config_option("GDAL_PDF_LIB", backend):
        yield backend


@pytest.fixture(params=["POPPLER", "PDFIUM"])
def poppler_or_pdfium(request):
    """
    Runs tests with poppler or pdfium, but not podofo
    """
    backend = request.param
    gdaltest.pdf_drv = gdal.GetDriverByName("PDF")

    md = gdaltest.pdf_drv.GetMetadata()
    if "HAVE_%s" % backend not in md:
        pytest.skip()

    with gdaltest.config_option("GDAL_PDF_LIB", backend):
        yield backend


def have_read_support():
    drv = gdal.GetDriverByName("PDF")
    if drv is None:
        return False
    md = drv.GetMetadata()
    return "HAVE_POPPLER" in md or "HAVE_PDFIUM" in md or "HAVE_PODOFO" in md


###############################################################################
# Returns True if we run with poppler


def pdf_is_poppler():

    md = gdaltest.pdf_drv.GetMetadata()
    val = gdal.GetConfigOption("GDAL_PDF_LIB", "POPPLER")
    if val == "POPPLER" and "HAVE_POPPLER" in md:
        return not pdf_is_pdfium()
    return False


###############################################################################
# Returns True if we run with pdfium


def pdf_is_pdfium():

    md = gdaltest.pdf_drv.GetMetadata()
    val = gdal.GetConfigOption("GDAL_PDF_LIB", "PDFIUM")
    if val == "PDFIUM" and "HAVE_PDFIUM" in md:
        return True
    return False


###############################################################################
# Returns True if we can compute the checksum


def pdf_checksum_available():

    try:
        ret = gdaltest.pdf_is_checksum_available
        if ret is True or ret is False:
            return ret
    except AttributeError:
        pass

    if pdf_is_poppler() or pdf_is_pdfium():
        gdaltest.pdf_is_checksum_available = True
        return gdaltest.pdf_is_checksum_available

    (_, err) = gdaltest.runexternal_out_and_err("pdftoppm -v")
    if err.startswith("pdftoppm version"):
        gdaltest.pdf_is_checksum_available = True
        return gdaltest.pdf_is_checksum_available
    print("Cannot compute to checksum due to missing pdftoppm")
    print(err)
    gdaltest.pdf_is_checksum_available = False
    return gdaltest.pdf_is_checksum_available


###############################################################################
# Test OGC best practice geospatial PDF


def test_pdf_online_1(poppler_or_pdfium):
    gdaltest.download_or_skip(
        "http://www.agc.army.mil/GeoPDFgallery/Imagery/Cherrydale_eDOQQ_1m_0_033_R1C1.pdf",
        "Cherrydale_eDOQQ_1m_0_033_R1C1.pdf",
    )

    try:
        os.stat("tmp/cache/Cherrydale_eDOQQ_1m_0_033_R1C1.pdf")
    except OSError:
        pytest.skip()

    ds = gdal.Open("tmp/cache/Cherrydale_eDOQQ_1m_0_033_R1C1.pdf")
    assert ds is not None

    assert ds.RasterXSize == 1241, "bad dimensions"

    gt = ds.GetGeoTransform()
    wkt = ds.GetProjectionRef()

    if pdf_is_pdfium():
        expected_gt = (
            -77.11232757568358,
            9.166339328135623e-06,
            0.0,
            38.89784240741306,
            0.0,
            -9.166503628631888e-06,
        )
    elif pdf_is_poppler():
        expected_gt = (
            -77.112328333299999,
            9.1666559999999995e-06,
            0.0,
            38.897842488372,
            -0.0,
            -9.1666559999999995e-06,
        )
    else:
        expected_gt = (
            -77.112328333299956,
            9.1666560000051172e-06,
            0.0,
            38.897842488371978,
            0.0,
            -9.1666560000046903e-06,
        )

    for i in range(6):
        if gt[i] != pytest.approx(expected_gt[i], abs=1e-15):
            # The remote file has been updated...
            other_expected_gt = (
                -77.112328333299928,
                9.1666560000165691e-06,
                0.0,
                38.897842488371978,
                0.0,
                -9.1666560000046903e-06,
            )
            for j in range(6):
                assert gt[j] == pytest.approx(
                    other_expected_gt[j], abs=1e-15
                ), "bad geotransform"

    assert wkt.startswith('GEOGCS["WGS 84"'), "bad WKT"

    if pdf_checksum_available():
        cs = ds.GetRasterBand(1).Checksum()
        assert cs != 0, "bad checksum"


###############################################################################


def test_pdf_online_2(poppler_or_pdfium):
    try:
        os.stat("tmp/cache/Cherrydale_eDOQQ_1m_0_033_R1C1.pdf")
    except OSError:
        pytest.skip()

    ds = gdal.Open("PDF:1:tmp/cache/Cherrydale_eDOQQ_1m_0_033_R1C1.pdf")
    assert ds is not None

    gt = ds.GetGeoTransform()
    wkt = ds.GetProjectionRef()

    if pdf_is_pdfium():
        expected_gt = (
            -77.11232757568358,
            9.166339328135623e-06,
            0.0,
            38.89784240741306,
            0.0,
            -9.166503628631888e-06,
        )
    elif pdf_is_poppler():
        expected_gt = (
            -77.112328333299999,
            9.1666559999999995e-06,
            0.0,
            38.897842488372,
            -0.0,
            -9.1666559999999995e-06,
        )
    else:
        expected_gt = (
            -77.112328333299956,
            9.1666560000051172e-06,
            0.0,
            38.897842488371978,
            0.0,
            -9.1666560000046903e-06,
        )

    for i in range(6):
        if gt[i] != pytest.approx(expected_gt[i], abs=1e-15):
            # The remote file has been updated...
            other_expected_gt = (
                -77.112328333299928,
                9.1666560000165691e-06,
                0.0,
                38.897842488371978,
                0.0,
                -9.1666560000046903e-06,
            )
            for j in range(6):
                assert gt[j] == pytest.approx(
                    other_expected_gt[j], abs=1e-15
                ), "bad geotransform"

    assert wkt.startswith('GEOGCS["WGS 84"')


###############################################################################
# Test Adobe style geospatial pdf


def test_pdf_1(poppler_or_pdfium):
    with gdal.config_option("GDAL_PDF_DPI", "200"):
        ds = gdal.Open("data/pdf/adobe_style_geospatial.pdf")
    assert ds is not None

    gt = ds.GetGeoTransform()
    wkt = ds.GetProjectionRef()

    if pdf_is_pdfium():
        expected_gt = (
            333274.5701626293,
            31.765012397640078,
            0.0,
            4940391.755002656,
            0.0,
            -31.794694104865194,
        )
    else:
        expected_gt = (
            333274.61654367246,
            31.764802242655662,
            0.0,
            4940391.7593506984,
            0.0,
            -31.794745501708238,
        )

    for i in range(6):
        assert gt[i] == pytest.approx(expected_gt[i], abs=1e-6), "bad geotransform"

    expected_wkt = 'PROJCS["WGS 84 / UTM zone 20N",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0],UNIT["Degree",0.0174532925199433]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-63],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH]]'
    assert wkt == expected_wkt, "bad WKT"

    if pdf_checksum_available():
        cs = ds.GetRasterBand(1).Checksum()
        # if cs != 17740 and cs != 19346:
        assert cs != 0, "bad checksum"

    neatline = ds.GetMetadataItem("NEATLINE")
    got_geom = ogr.CreateGeometryFromWkt(neatline)
    if pdf_is_pdfium():
        expected_geom = ogr.CreateGeometryFromWkt(
            "POLYGON ((338304.274926337 4896673.68220244,338304.206397453 4933414.86906802,382774.239238623 4933414.4314159,382774.990878249 4896674.38093959,338304.274926337 4896673.68220244))"
        )
    else:
        expected_geom = ogr.CreateGeometryFromWkt(
            "POLYGON ((338304.150125828920864 4896673.639421294443309,338304.177293475600891 4933414.799376524984837,382774.271384406310972 4933414.546264361590147,382774.767329963855445 4896674.273581005632877,338304.150125828920864 4896673.639421294443309))"
        )

    ogrtest.check_feature_geometry(got_geom, expected_geom)


###############################################################################
# Test write support with ISO32000 geo encoding


def test_pdf_iso32000(poppler_or_pdfium_or_podofo):
    tst = gdaltest.GDALTest("PDF", "byte.tif", 1, None)
    tst.testCreateCopy(
        check_minmax=0,
        check_gt=1,
        check_srs=True,
        check_checksum_not_null=pdf_checksum_available(),
    )


###############################################################################
# Test write support with ISO32000 geo encoding, with DPI=300


def test_pdf_iso32000_dpi_300(poppler_or_pdfium):
    tst = gdaltest.GDALTest("PDF", "byte.tif", 1, None, options=["DPI=300"])
    tst.testCreateCopy(
        check_minmax=0,
        check_gt=1,
        check_srs=True,
        check_checksum_not_null=pdf_checksum_available(),
    )


###############################################################################
# Test no compression


def test_pdf_no_compression(poppler_or_pdfium):
    tst = gdaltest.GDALTest("PDF", "byte.tif", 1, None, options=["COMPRESS=NONE"])
    tst.testCreateCopy(
        check_minmax=0,
        check_gt=0,
        check_srs=None,
        check_checksum_not_null=pdf_checksum_available(),
    )


###############################################################################
# Test compression methods


def _test_pdf_jpeg_compression(filename):

    tst = gdaltest.GDALTest("PDF", filename, 1, None, options=["COMPRESS=JPEG"])
    tst.testCreateCopy(
        check_minmax=0,
        check_gt=0,
        check_srs=None,
        check_checksum_not_null=pdf_checksum_available(),
    )


@pytest.mark.require_driver("JPEG")
def test_pdf_jpeg_compression(poppler_or_pdfium):
    _test_pdf_jpeg_compression("byte.tif")


def pdf_get_J2KDriver(drv_name):
    drv = gdal.GetDriverByName(drv_name)
    if drv is None:
        return None
    if drv_name == "JP2ECW":
        import ecw

        if not ecw.has_write_support():
            return None
    return drv


@pytest.mark.parametrize(
    "filename,drv_name",
    [
        ("utm.tif", None),
        ("utm.tif", "JP2KAK"),
        ("utm.tif", "JP2ECW"),
        ("utm.tif", "JP2OpenJpeg"),
        ("rgbsmall.tif", "JP2ECW"),
    ],
)
def test_pdf_jpx_compression(filename, drv_name):
    if drv_name is None:
        if (
            pdf_get_J2KDriver("JP2KAK") is None
            and pdf_get_J2KDriver("JP2ECW") is None
            and pdf_get_J2KDriver("JP2OpenJpeg") is None
            and pdf_get_J2KDriver("JPEG2000") is None
        ):
            pytest.skip()
    elif pdf_get_J2KDriver(drv_name) is None:
        pytest.skip()

    if drv_name is None:
        options = ["COMPRESS=JPEG2000"]
    else:
        options = ["COMPRESS=JPEG2000", "JPEG2000_DRIVER=%s" % drv_name]

    tst = gdaltest.GDALTest("PDF", filename, 1, None, options=options)
    tst.testCreateCopy(
        check_minmax=0,
        check_gt=0,
        check_srs=None,
        check_checksum_not_null=pdf_checksum_available(),
    )


@pytest.mark.require_driver("JPEG")
def test_pdf_jpeg_compression_rgb(poppler_or_pdfium):
    return _test_pdf_jpeg_compression("rgbsmall.tif")


###############################################################################
# Test RGBA


def pdf_rgba_default_compression(options_param=None):
    options_param = [] if options_param is None else options_param
    if not pdf_checksum_available():
        pytest.skip()

    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    out_ds = gdaltest.pdf_drv.CreateCopy("tmp/rgba.pdf", src_ds, options=options_param)
    out_ds = None

    # gdal.SetConfigOption('GDAL_PDF_BANDS', '4')
    with gdal.config_options(
        {"PDF_DUMP_OBJECT": "tmp/rgba.pdf.txt", "PDF_DUMP_PARENT": "YES"}
    ):
        out_ds = gdal.Open("tmp/rgba.pdf")
    content = open("tmp/rgba.pdf.txt", "rt").read()
    os.unlink("tmp/rgba.pdf.txt")
    cs1 = out_ds.GetRasterBand(1).Checksum()
    cs2 = out_ds.GetRasterBand(2).Checksum()
    cs3 = out_ds.GetRasterBand(3).Checksum()
    if out_ds.RasterCount == 4:
        cs4 = out_ds.GetRasterBand(4).Checksum()
    else:
        cs4 = -1

    src_cs1 = src_ds.GetRasterBand(1).Checksum()
    src_cs2 = src_ds.GetRasterBand(2).Checksum()
    src_cs3 = src_ds.GetRasterBand(3).Checksum()
    src_cs4 = src_ds.GetRasterBand(4).Checksum()
    out_ds = None
    # gdal.SetConfigOption('GDAL_PDF_BANDS', None)

    gdal.GetDriverByName("PDF").Delete("tmp/rgba.pdf")

    if cs4 < 0:
        pytest.skip()

    assert (
        content.startswith("Type = dictionary, Num = 3, Gen = 0")
        and "      Type = dictionary, Num = 3, Gen = 0" in content
    ), "wrong object dump"

    assert cs4 != 0, "wrong checksum"

    if cs1 != src_cs1 or cs2 != src_cs2 or cs3 != src_cs3 or cs4 != src_cs4:
        print(cs1)
        print(cs2)
        print(cs3)
        print(cs4)
        print(src_cs1)
        print(src_cs2)
        print(src_cs3)
        print(src_cs4)


def test_pdf_rgba_default_compression_tiled(poppler_or_pdfium_or_podofo):
    return pdf_rgba_default_compression(["BLOCKXSIZE=32", "BLOCKYSIZE=32"])


@pytest.mark.require_driver("JPEG")
def test_pdf_jpeg_compression_rgba(poppler_or_pdfium):
    return _test_pdf_jpeg_compression("../../gcore/data/stefan_full_rgba.tif")


###############################################################################
# Test PREDICTOR=2


def test_pdf_predictor_2(poppler_or_pdfium):
    tst = gdaltest.GDALTest("PDF", "utm.tif", 1, None, options=["PREDICTOR=2"])
    tst.testCreateCopy(
        check_minmax=0,
        check_gt=0,
        check_srs=None,
        check_checksum_not_null=pdf_checksum_available(),
    )


def test_pdf_predictor_2_rgb(poppler_or_pdfium):
    tst = gdaltest.GDALTest("PDF", "rgbsmall.tif", 1, None, options=["PREDICTOR=2"])
    tst.testCreateCopy(
        check_minmax=0,
        check_gt=0,
        check_srs=None,
        check_checksum_not_null=pdf_checksum_available(),
    )


###############################################################################
# Test tiling


def test_pdf_tiled(poppler_or_pdfium):
    tst = gdaltest.GDALTest(
        "PDF", "utm.tif", 1, None, options=["COMPRESS=DEFLATE", "TILED=YES"]
    )
    tst.testCreateCopy(
        check_minmax=0,
        check_gt=0,
        check_srs=None,
        check_checksum_not_null=pdf_checksum_available(),
    )


def test_pdf_tiled_128(poppler_or_pdfium):
    tst = gdaltest.GDALTest(
        "PDF", "utm.tif", 1, None, options=["BLOCKXSIZE=128", "BLOCKYSIZE=128"]
    )
    tst.testCreateCopy(
        check_minmax=0,
        check_gt=0,
        check_srs=None,
        check_checksum_not_null=pdf_checksum_available(),
    )


###############################################################################
# Test raster with color table


@pytest.mark.require_driver("GIF")
def test_pdf_color_table(poppler_or_pdfium):

    tst = gdaltest.GDALTest("PDF", "small_world_pct.tif", 1, None)
    tst.testCreateCopy(
        check_minmax=0,
        check_gt=0,
        check_srs=None,
        check_checksum_not_null=pdf_checksum_available(),
    )


###############################################################################
# Test XMP support


def test_pdf_xmp(poppler_or_pdfium):
    src_ds = gdal.Open("data/pdf/adobe_style_geospatial_with_xmp.pdf")
    gdaltest.pdf_drv.CreateCopy("tmp/pdf_xmp.pdf", src_ds, options=["WRITE_INFO=NO"])
    out_ds = gdal.Open("tmp/pdf_xmp.pdf")
    if out_ds is None:
        # Some Poppler versions cannot re-open the file
        gdal.GetDriverByName("PDF").Delete("tmp/pdf_xmp.pdf")
        pytest.skip()

    ref_md = src_ds.GetMetadata("xml:XMP")
    got_md = out_ds.GetMetadata("xml:XMP")
    base_md = out_ds.GetMetadata()
    out_ds = None
    src_ds = None

    gdal.GetDriverByName("PDF").Delete("tmp/pdf_xmp.pdf")

    assert ref_md[0] == got_md[0]

    assert len(base_md) == 2


###############################################################################
# Test Info


def test_pdf_info(poppler_or_pdfium):
    try:
        val = "\xc3\xa9".decode("UTF-8")
    except Exception:
        val = "\u00e9"

    options = [
        "AUTHOR=%s" % val,
        "CREATOR=creator",
        "KEYWORDS=keywords",
        "PRODUCER=producer",
        "SUBJECT=subject",
        "TITLE=title",
    ]

    src_ds = gdal.Open("data/byte.tif")
    out_ds = gdaltest.pdf_drv.CreateCopy("tmp/pdf_info.pdf", src_ds, options=options)
    # print(out_ds.GetMetadata())
    out_ds2 = gdaltest.pdf_drv.CreateCopy("tmp/pdf_info_2.pdf", out_ds)
    md = out_ds2.GetMetadata()
    # print(md)
    out_ds2 = None
    out_ds = None
    src_ds = None

    gdal.GetDriverByName("PDF").Delete("tmp/pdf_info.pdf")
    gdal.GetDriverByName("PDF").Delete("tmp/pdf_info_2.pdf")

    assert (
        md["AUTHOR"] == val
        and md["CREATOR"] == "creator"
        and md["KEYWORDS"] == "keywords"
        and md["PRODUCER"] == "producer"
        and md["SUBJECT"] == "subject"
        and md["TITLE"] == "title"
    ), "metadata doesn't match"


###############################################################################
# Check SetGeoTransform() / SetProjection()


def test_pdf_update_gt(poppler_or_pdfium_or_podofo):
    src_ds = gdal.Open("data/byte.tif")
    ds = gdaltest.pdf_drv.CreateCopy("tmp/pdf_update_gt.pdf", src_ds)
    ds = None
    src_ds = None

    # Alter geotransform
    ds = gdal.Open("tmp/pdf_update_gt.pdf", gdal.GA_Update)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds = None

    assert not os.path.exists("tmp/pdf_update_gt.pdf.aux.xml")

    # Check geotransform
    ds = gdal.Open("tmp/pdf_update_gt.pdf")
    gt = ds.GetGeoTransform()
    ds = None

    expected_gt = [2, 1, 0, 49, 0, -1]
    for i in range(6):
        assert gt[i] == pytest.approx(
            expected_gt[i], abs=1e-8
        ), "did not get expected gt"

    # Clear geotransform
    ds = gdal.Open("tmp/pdf_update_gt.pdf", gdal.GA_Update)
    ds.SetProjection("")
    ds = None

    # Check geotransform
    ds = gdal.Open("tmp/pdf_update_gt.pdf")
    gt = ds.GetGeoTransform()
    ds = None

    expected_gt = [0.0, 1.0, 0.0, 0.0, 0.0, 1.0]
    for i in range(6):
        assert gt[i] == pytest.approx(
            expected_gt[i], abs=1e-8
        ), "did not get expected gt"

    # Set geotransform again
    ds = gdal.Open("tmp/pdf_update_gt.pdf", gdal.GA_Update)
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([3, 1, 0, 50, 0, -1])
    ds = None

    # Check geotransform
    ds = gdal.Open("tmp/pdf_update_gt.pdf")
    gt = ds.GetGeoTransform()
    ds = None

    expected_gt = [3, 1, 0, 50, 0, -1]
    for i in range(6):
        assert gt[i] == pytest.approx(
            expected_gt[i], abs=1e-8
        ), "did not get expected gt"

    gdaltest.pdf_drv.Delete("tmp/pdf_update_gt.pdf")


###############################################################################
# Check SetMetadataItem() for Info


def test_pdf_update_info(poppler_or_pdfium_or_podofo):
    src_ds = gdal.Open("data/byte.tif")
    ds = gdaltest.pdf_drv.CreateCopy("tmp/pdf_update_info.pdf", src_ds)
    ds = None
    src_ds = None

    # Add info
    ds = gdal.Open("tmp/pdf_update_info.pdf", gdal.GA_Update)
    ds.SetMetadataItem("AUTHOR", "author")
    ds = None

    # Check
    ds = gdal.Open("tmp/pdf_update_info.pdf")
    author = ds.GetMetadataItem("AUTHOR")
    ds = None

    assert author == "author", "did not get expected metadata"

    # Update info
    ds = gdal.Open("tmp/pdf_update_info.pdf", gdal.GA_Update)
    ds.SetMetadataItem("AUTHOR", "author2")
    ds = None

    # Check
    ds = gdal.Open("tmp/pdf_update_info.pdf")
    author = ds.GetMetadataItem("AUTHOR")
    ds = None

    assert author == "author2", "did not get expected metadata"

    # Clear info
    ds = gdal.Open("tmp/pdf_update_info.pdf", gdal.GA_Update)
    ds.SetMetadataItem("AUTHOR", None)
    ds = None

    # Check PAM doesn't exist
    if os.path.exists("tmp/pdf_update_info.pdf.aux.xml"):
        print(author)
        pytest.fail("did not expected .aux.xml")

    # Check
    ds = gdal.Open("tmp/pdf_update_info.pdf")
    author = ds.GetMetadataItem("AUTHOR")
    ds = None

    assert author is None, "did not get expected metadata"

    gdaltest.pdf_drv.Delete("tmp/pdf_update_info.pdf")


###############################################################################
# Check SetMetadataItem() for xml:XMP


def test_pdf_update_xmp(poppler_or_pdfium_or_podofo):
    src_ds = gdal.Open("data/byte.tif")
    ds = gdaltest.pdf_drv.CreateCopy("tmp/pdf_update_xmp.pdf", src_ds)
    ds = None
    src_ds = None

    # Add info
    ds = gdal.Open("tmp/pdf_update_xmp.pdf", gdal.GA_Update)
    ds.SetMetadata(["<?xpacket begin='a'/><a/>"], "xml:XMP")
    ds = None

    # Check
    ds = gdal.Open("tmp/pdf_update_xmp.pdf")
    xmp = ds.GetMetadata("xml:XMP")[0]
    ds = None

    assert xmp == "<?xpacket begin='a'/><a/>", "did not get expected metadata"

    # Update info
    ds = gdal.Open("tmp/pdf_update_xmp.pdf", gdal.GA_Update)
    ds.SetMetadata(["<?xpacket begin='a'/><a_updated/>"], "xml:XMP")
    ds = None

    # Check
    ds = gdal.Open("tmp/pdf_update_xmp.pdf")
    xmp = ds.GetMetadata("xml:XMP")[0]
    ds = None

    assert xmp == "<?xpacket begin='a'/><a_updated/>", "did not get expected metadata"

    # Check PAM doesn't exist
    assert not os.path.exists(
        "tmp/pdf_update_xmp.pdf.aux.xml"
    ), "did not expected .aux.xml"

    # Clear info
    ds = gdal.Open("tmp/pdf_update_xmp.pdf", gdal.GA_Update)
    ds.SetMetadata(None, "xml:XMP")
    ds = None

    # Check
    ds = gdal.Open("tmp/pdf_update_xmp.pdf")
    xmp = ds.GetMetadata("xml:XMP")
    ds = None

    assert xmp is None, "did not get expected metadata"

    gdaltest.pdf_drv.Delete("tmp/pdf_update_xmp.pdf")


###############################################################################
# Check SetGCPs() but with GCPs that resolve to a geotransform


def _pdf_update_gcps(poppler_or_pdfium):
    dpi = 300
    out_filename = "tmp/pdf_update_gcps.pdf"

    src_ds = gdal.Open("data/byte.tif")
    src_wkt = src_ds.GetProjectionRef()
    src_gt = src_ds.GetGeoTransform()
    ds = gdaltest.pdf_drv.CreateCopy(
        out_filename, src_ds, options=["GEO_ENCODING=NONE", "DPI=%d" % dpi]
    )
    ds = None
    src_ds = None

    gcp = [[2.0, 8.0, 0, 0], [2.0, 18.0, 0, 0], [16.0, 18.0, 0, 0], [16.0, 8.0, 0, 0]]

    for i in range(4):
        gcp[i][2] = src_gt[0] + gcp[i][0] * src_gt[1] + gcp[i][1] * src_gt[2]
        gcp[i][3] = src_gt[3] + gcp[i][0] * src_gt[4] + gcp[i][1] * src_gt[5]

    vrt_txt = """<VRTDataset rasterXSize="20" rasterYSize="20">
<GCPList Projection=''>
    <GCP Id="" Pixel="%f" Line="%f" X="%f" Y="%f"/>
    <GCP Id="" Pixel="%f" Line="%f" X="%f" Y="%f"/>
    <GCP Id="" Pixel="%f" Line="%f" X="%f" Y="%f"/>
    <GCP Id="" Pixel="%f" Line="%f" X="%f" Y="%f"/>
</GCPList>
<VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
    <SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>
    <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
    <SourceBand>1</SourceBand>
    </SimpleSource>
</VRTRasterBand>
</VRTDataset>""" % (
        gcp[0][0],
        gcp[0][1],
        gcp[0][2],
        gcp[0][3],
        gcp[1][0],
        gcp[1][1],
        gcp[1][2],
        gcp[1][3],
        gcp[2][0],
        gcp[2][1],
        gcp[2][2],
        gcp[2][3],
        gcp[3][0],
        gcp[3][1],
        gcp[3][2],
        gcp[3][3],
    )
    vrt_ds = gdal.Open(vrt_txt)
    gcps = vrt_ds.GetGCPs()
    vrt_ds = None

    # Set GCPs()
    ds = gdal.Open(out_filename, gdal.GA_Update)
    ds.SetGCPs(gcps, src_wkt)
    ds = None

    # Check
    ds = gdal.Open(out_filename)
    got_gt = ds.GetGeoTransform()
    got_wkt = ds.GetProjectionRef()
    got_gcp_count = ds.GetGCPCount()
    ds.GetGCPs()
    got_gcp_wkt = ds.GetGCPProjection()
    got_neatline = ds.GetMetadataItem("NEATLINE")

    if pdf_is_pdfium():
        max_error = 1
    else:
        max_error = 0.0001

    ds = None

    assert got_wkt != "", "did not expect null GetProjectionRef"

    assert got_gcp_wkt == "", "did not expect non null GetGCPProjection"

    for i in range(6):
        assert got_gt[i] == pytest.approx(
            src_gt[i], abs=1e-8
        ), "did not get expected gt"

    assert got_gcp_count == 0, "did not expect GCPs"

    got_geom = ogr.CreateGeometryFromWkt(got_neatline)
    expected_lr = ogr.Geometry(ogr.wkbLinearRing)
    for i in range(4):
        expected_lr.AddPoint_2D(gcp[i][2], gcp[i][3])
    expected_lr.AddPoint_2D(gcp[0][2], gcp[0][3])
    expected_geom = ogr.Geometry(ogr.wkbPolygon)
    expected_geom.AddGeometry(expected_lr)

    ogrtest.check_feature_geometry(got_geom, expected_geom, max_error=max_error)

    gdaltest.pdf_drv.Delete(out_filename)


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_pdf_update_gcps_iso32000(poppler_or_pdfium):
    gdal.SetConfigOption("GDAL_PDF_GEO_ENCODING", None)
    _pdf_update_gcps(poppler_or_pdfium)


###############################################################################
# Check NEATLINE support


def _pdf_set_neatline(pdf_backend, geo_encoding, dpi=300):
    out_filename = "tmp/pdf_set_neatline.pdf"

    if geo_encoding == "ISO32000":
        neatline = "POLYGON ((441720 3751320,441720 3750120,441920 3750120,441920 3751320,441720 3751320))"
    else:  # For OGC_BP, we can use more than 4 points
        neatline = "POLYGON ((441720 3751320,441720 3751000,441720 3750120,441920 3750120,441920 3751320,441720 3751320))"

    # Test CreateCopy() with NEATLINE
    src_ds = gdal.Open("data/byte.tif")
    expected_gt = src_ds.GetGeoTransform()
    ds = gdaltest.pdf_drv.CreateCopy(
        out_filename,
        src_ds,
        options=[
            "NEATLINE=%s" % neatline,
            "GEO_ENCODING=%s" % geo_encoding,
            "DPI=%d" % dpi,
        ],
    )
    ds = None
    src_ds = None

    # Check
    ds = gdal.Open(out_filename)
    got_gt = ds.GetGeoTransform()
    got_neatline = ds.GetMetadataItem("NEATLINE")

    if pdf_is_pdfium():
        max_error = 1
    else:
        max_error = 0.0001
    ds = None

    for i in range(6):
        assert got_gt[i] == pytest.approx(
            expected_gt[i], abs=2e-7
        ), "did not get expected gt"

    got_geom = ogr.CreateGeometryFromWkt(got_neatline)
    expected_geom = ogr.CreateGeometryFromWkt(neatline)

    ogrtest.check_feature_geometry(got_geom, expected_geom, max_error=max_error)

    # Test SetMetadataItem()
    ds = gdal.Open(out_filename, gdal.GA_Update)
    neatline = "POLYGON ((440720 3751320,440720 3750120,441920 3750120,441920 3751320,440720 3751320))"
    ds.SetMetadataItem("NEATLINE", neatline)
    ds = None

    # Check
    with gdal.config_option("GDAL_PDF_GEO_ENCODING", geo_encoding):
        ds = gdal.Open(out_filename)
        got_gt = ds.GetGeoTransform()
        got_neatline = ds.GetMetadataItem("NEATLINE")

        if pdf_is_pdfium():
            if geo_encoding == "ISO32000":
                expected_gt = (
                    440722.36151923181,
                    59.93217744208814,
                    0.0,
                    3751318.7819266757,
                    0.0,
                    -59.941906300000845,
                )

        ds = None

    for i in range(6):
        assert not (
            expected_gt[i] == 0 and got_gt[i] != pytest.approx(expected_gt[i], abs=1e-7)
        ) or (
            expected_gt[i] != 0
            and abs((got_gt[i] - expected_gt[i]) / expected_gt[i]) > 1e-7
        ), "did not get expected gt"

    got_geom = ogr.CreateGeometryFromWkt(got_neatline)
    expected_geom = ogr.CreateGeometryFromWkt(neatline)

    ogrtest.check_feature_geometry(got_geom, expected_geom, max_error=max_error)

    gdaltest.pdf_drv.Delete(out_filename)


def test_pdf_set_neatline_iso32000(poppler_or_pdfium):
    return _pdf_set_neatline(poppler_or_pdfium, "ISO32000")


###############################################################################
# Check that we can generate identical file


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_pdf_check_identity_iso32000(poppler_or_pdfium):
    out_filename = "tmp/pdf_check_identity_iso32000.pdf"

    src_ds = gdal.Open("data/pdf/test_pdf.vrt")
    out_ds = gdaltest.pdf_drv.CreateCopy(
        out_filename, src_ds, options=["STREAM_COMPRESS=NONE"]
    )
    del out_ds
    src_ds = None

    f = open("data/pdf/test_iso32000.pdf", "rb")
    data_ref = f.read()
    f.close()

    f = open("data/pdf/test_iso32000_libpng_1_6_40.pdf", "rb")
    data_ref2 = f.read()
    f.close()

    f = open(out_filename, "rb")
    data_got = f.read()
    f.close()

    assert (
        data_got == data_ref or data_got == data_ref2
    ), "content does not match reference content"

    gdaltest.pdf_drv.Delete(out_filename)


###############################################################################
# Check layers support


def test_pdf_layers(poppler_or_pdfium):
    if not pdf_is_poppler() and not pdf_is_pdfium():
        pytest.skip()

    ds = gdal.Open("data/pdf/adobe_style_geospatial.pdf")
    layers = ds.GetMetadata_List("LAYERS")
    cs1 = ds.GetRasterBand(1).Checksum()
    ds = None

    # if layers != ['LAYER_00_INIT_STATE=ON', 'LAYER_00_NAME=New_Data_Frame', 'LAYER_01_INIT_STATE=ON', 'LAYER_01_NAME=New_Data_Frame.Graticule', 'LAYER_02_INIT_STATE=ON', 'LAYER_02_NAME=Layers', 'LAYER_03_INIT_STATE=ON', 'LAYER_03_NAME=Layers.Measured_Grid', 'LAYER_04_INIT_STATE=ON', 'LAYER_04_NAME=Layers.Graticule']:
    assert layers == [
        "LAYER_00_NAME=New_Data_Frame",
        "LAYER_01_NAME=New_Data_Frame.Graticule",
        "LAYER_02_NAME=Layers",
        "LAYER_03_NAME=Layers.Measured_Grid",
        "LAYER_04_NAME=Layers.Graticule",
    ], "did not get expected layers"

    if not pdf_checksum_available():
        pytest.skip()

    # Turn a layer off
    with gdal.config_option("GDAL_PDF_LAYERS_OFF", "New_Data_Frame"):
        ds = gdal.Open("data/pdf/adobe_style_geospatial.pdf")
        cs2 = ds.GetRasterBand(1).Checksum()
        ds = None

    assert cs2 != cs1, "did not get expected checksum"

    # Turn the other layer on
    with gdal.config_option("GDAL_PDF_LAYERS", "Layers"):
        ds = gdal.Open("data/pdf/adobe_style_geospatial.pdf")
        cs3 = ds.GetRasterBand(1).Checksum()
        ds = None

    # So the end result must be identical
    assert cs3 == cs2, "did not get expected checksum"

    # Turn another sublayer on
    ds = gdal.OpenEx(
        "data/pdf/adobe_style_geospatial.pdf",
        open_options=["LAYERS=Layers.Measured_Grid"],
    )
    cs4 = ds.GetRasterBand(1).Checksum()
    ds = None

    assert not (cs4 == cs1 or cs4 == cs2), "did not get expected checksum"


###############################################################################
# Check layers support


def test_pdf_layers_with_same_name_on_different_pages(poppler_or_pdfium):

    ds = gdal.Open("data/pdf/layer_with_same_name_on_different_pages.pdf")
    layers = ds.GetMetadata_List("LAYERS")
    assert layers == [
        "LAYER_00_NAME=Map_Frame (page 1)",
        "LAYER_01_NAME=Map_Frame.Map (page 1)",
        "LAYER_02_NAME=Map_Frame.Map.States (page 1)",
        "LAYER_03_NAME=Map_Frame (page 2)",
        "LAYER_04_NAME=Map_Frame.Map (page 2)",
        "LAYER_05_NAME=Map_Frame.Map.States (page 2)",
        "LAYER_06_NAME=Map_Frame (page 3)",
        "LAYER_07_NAME=Map_Frame.Map (page 3)",
        "LAYER_08_NAME=Map_Frame.Map.States (page 3)",
        "LAYER_09_NAME=Map_Frame (page 4)",
        "LAYER_10_NAME=Map_Frame.Map (page 4)",
        "LAYER_11_NAME=Map_Frame.Map.States (page 4)",
    ]

    for page in (1, 2, 3, 4):
        ds = gdal.Open(
            f"PDF:{page}:data/pdf/layer_with_same_name_on_different_pages.pdf"
        )
        layers = ds.GetMetadata_List("LAYERS")
        assert layers == [
            "LAYER_00_NAME=Map_Frame",
            "LAYER_01_NAME=Map_Frame.Map",
            "LAYER_02_NAME=Map_Frame.Map.States",
        ]
        cs_all = ds.GetRasterBand(1).Checksum()

        ds = gdal.OpenEx(
            f"PDF:{page}:data/pdf/layer_with_same_name_on_different_pages.pdf",
            open_options=["LAYERS_OFF=Map_Frame.Map.States"],
        )
        assert ds.GetRasterBand(1).Checksum() != cs_all


###############################################################################
# Test MARGIN, EXTRA_STREAM, EXTRA_LAYER_NAME and EXTRA_IMAGES options


def test_pdf_custom_layout(poppler_or_pdfium):
    js = """button = app.alert({cMsg: 'This file was generated by GDAL. Do you want to visit its website ?', cTitle: 'Question', nIcon:2, nType:2});
if (button == 4) app.launchURL('http://gdal.org/');"""

    options = [
        "LEFT_MARGIN=1",
        "TOP_MARGIN=2",
        "RIGHT_MARGIN=3",
        "BOTTOM_MARGIN=4",
        "DPI=300",
        "LAYER_NAME=byte_tif",
        "EXTRA_STREAM=BT 255 0 0 rg /FTimesRoman 1 Tf 1 0 0 1 1 1 Tm (Footpage string) Tj ET",
        "EXTRA_LAYER_NAME=Footpage_and_logo",
        "EXTRA_IMAGES=data/byte.tif,0.5,0.5,0.2,link=http://gdal.org/,data/byte.tif,0.5,1.5,0.2",
        "JAVASCRIPT=%s" % js,
    ]

    src_ds = gdal.Open("data/byte.tif")
    ds = gdaltest.pdf_drv.CreateCopy(
        "tmp/pdf_custom_layout.pdf", src_ds, options=options
    )
    ds = None
    src_ds = None

    if pdf_is_poppler() or pdf_is_pdfium():
        ds = gdal.Open("tmp/pdf_custom_layout.pdf")
        ds.GetRasterBand(1).Checksum()
        layers = ds.GetMetadata_List("LAYERS")
        ds = None

    gdal.GetDriverByName("PDF").Delete("tmp/pdf_custom_layout.pdf")

    if pdf_is_poppler() or pdf_is_pdfium():
        assert layers == [
            "LAYER_00_NAME=byte_tif",
            "LAYER_01_NAME=Footpage_and_logo",
        ], "did not get expected layers"


###############################################################################
# Test CLIPPING_EXTENT, EXTRA_RASTERS, EXTRA_RASTERS_LAYER_NAME, OFF_LAYERS, EXCLUSIVE_LAYERS options


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_pdf_extra_rasters(poppler_or_pdfium):
    subbyte = """<VRTDataset rasterXSize="10" rasterYSize="10">
  <SRS>PROJCS["NAD27 / UTM zone 11N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke 1866",6378206.4,294.9786982139006,AUTHORITY["EPSG","7008"]],AUTHORITY["EPSG","6267"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4267"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AUTHORITY["EPSG","26711"]]</SRS>
  <GeoTransform>  4.4102000000000000e+05,  6.0000000000000000e+01,  0.0000000000000000e+00,  3.7510200000000000e+06,  0.0000000000000000e+00, -6.0000000000000000e+01</GeoTransform>
  <Metadata>
    <MDI key="AREA_OR_POINT">Area</MDI>
  </Metadata>
  <Metadata domain="IMAGE_STRUCTURE">
    <MDI key="INTERLEAVE">BAND</MDI>
  </Metadata>
  <VRTRasterBand dataType="Byte" band="1">
    <Metadata />
    <ColorInterp>Gray</ColorInterp>
    <SimpleSource>
      <SourceFilename relativeToVRT="1">../data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="5" yOff="5" xSize="10" ySize="10" />
      <DstRect xOff="0" yOff="0" xSize="10" ySize="10" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>"""

    f = open("tmp/subbyte.vrt", "wt")
    f.write(subbyte)
    f.close()

    options = [
        "MARGIN=1",
        "DPI=300",
        "WRITE_USERUNIT=YES",
        "CLIPPING_EXTENT=440780,3750180,441860,3751260",
        "LAYER_NAME=byte_tif",
        "EXTRA_RASTERS=tmp/subbyte.vrt",
        "EXTRA_RASTERS_LAYER_NAME=subbyte",
        "OFF_LAYERS=byte_tif",
        "EXCLUSIVE_LAYERS=byte_tif,subbyte",
    ]

    src_ds = gdal.Open("data/byte.tif")
    ds = gdaltest.pdf_drv.CreateCopy(
        "tmp/pdf_extra_rasters.pdf", src_ds, options=options
    )
    ds = None
    src_ds = None

    if pdf_is_poppler() or pdf_is_pdfium():
        ds = gdal.Open("tmp/pdf_extra_rasters.pdf")
        cs = ds.GetRasterBand(1).Checksum()
        layers = ds.GetMetadata_List("LAYERS")
        ds = None

    gdal.GetDriverByName("PDF").Delete("tmp/pdf_extra_rasters.pdf")
    os.unlink("tmp/subbyte.vrt")

    if pdf_is_poppler() or pdf_is_pdfium():
        assert layers == [
            "LAYER_00_NAME=byte_tif",
            "LAYER_01_NAME=subbyte",
        ], "did not get expected layers"
    if pdf_is_poppler():
        assert cs in (7926, 8177, 8174, 8165, 8172, 8191, 8193)


###############################################################################
# Test adding a OGR datasource


@pytest.mark.require_driver("CSV")
def test_pdf_write_ogr(poppler_or_pdfium):
    f = gdal.VSIFOpenL("tmp/test.csv", "wb")
    data = """id,foo,WKT,style
1,bar,"MULTIPOLYGON (((440720 3751320,440720 3750120,441020 3750120,441020 3751320,440720 3751320),(440800 3751200,440900 3751200,440900 3751000,440800 3751000,440800 3751200)),((441720 3751320,441720 3750120,441920 3750120,441920 3751320,441720 3751320)))",
2,baz,"LINESTRING(440720 3751320,441920 3750120)","PEN(c:#FF0000,w:5pt,p:""2px 1pt"")"
3,baz2,"POINT(441322.400 3750717.600)","PEN(c:#FF00FF,w:10px);BRUSH(fc:#FFFFFF);LABEL(c:#FF000080, f:""Arial, Helvetica"", a:45, s:12pt, t:""Hello World!"")"
4,baz3,"POINT(0 0)",""
"""
    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFCloseL(f)

    f = gdal.VSIFOpenL("tmp/test.vrt", "wb")
    data = """<OGRVRTDataSource>
  <OGRVRTLayer name="test">
    <SrcDataSource relativeToVRT="0" shared="1">tmp/test.csv</SrcDataSource>
    <SrcLayer>test</SrcLayer>
    <GeometryType>wkbUnknown</GeometryType>
    <LayerSRS>EPSG:26711</LayerSRS>
    <Field name="id" type="Integer" src="id"/>
    <Field name="foo" type="String" src="foo"/>
    <Field name="WKT" type="String" src="WKT"/>
    <Style>style</Style>
  </OGRVRTLayer>
</OGRVRTDataSource>
"""
    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFCloseL(f)

    options = [
        "OGR_DATASOURCE=tmp/test.vrt",
        "OGR_DISPLAY_LAYER_NAMES=A_Layer",
        "OGR_DISPLAY_FIELD=foo",
    ]

    src_ds = gdal.Open("data/byte.tif")
    ds = gdaltest.pdf_drv.CreateCopy("tmp/pdf_write_ogr.pdf", src_ds, options=options)
    ds = None
    src_ds = None

    if pdf_is_poppler() or pdf_is_pdfium():
        ds = gdal.Open("tmp/pdf_write_ogr.pdf")
        cs_ref = ds.GetRasterBand(1).Checksum()
        layers = ds.GetMetadata_List("LAYERS")
        ds = None

    ogr_ds = ogr.Open("tmp/pdf_write_ogr.pdf")
    ogr_lyr = ogr_ds.GetLayer(0)
    feature_count = ogr_lyr.GetFeatureCount()
    ogr_ds = None

    if pdf_is_poppler() or pdf_is_pdfium():

        cs_tab = []
        rendering_options = [
            "RASTER",
            "VECTOR",
            "TEXT",
            "RASTER,VECTOR",
            "RASTER,TEXT",
            "VECTOR,TEXT",
            "RASTER,VECTOR,TEXT",
        ]
        for opt in rendering_options:
            gdal.ErrorReset()
            ds = gdal.OpenEx(
                "tmp/pdf_write_ogr.pdf", open_options=["RENDERING_OPTIONS=%s" % opt]
            )
            cs = ds.GetRasterBand(1).Checksum()
            # When misconfigured Poppler with fonts, use this to avoid error
            if "TEXT" in opt and gdal.GetLastErrorMsg().find("font") >= 0:
                cs = -cs
            cs_tab.append(cs)
            ds = None

        # Test that all combinations give a different result
        for i, roi in enumerate(rendering_options):
            # print('Checksum %s: %d' % (rendering_options[i], cs_tab[i]) )
            for j in range(i + 1, len(rendering_options)):
                if cs_tab[i] == cs_tab[j] and cs_tab[i] >= 0 and cs_tab[j] >= 0:
                    print("Checksum %s: %d" % (roi, cs_tab[i]))
                    pytest.fail("Checksum %s: %d" % (rendering_options[j], cs_tab[j]))

        # And test that RASTER,VECTOR,TEXT is the default rendering
        assert abs(cs_tab[len(rendering_options) - 1]) == cs_ref

    gdal.GetDriverByName("PDF").Delete("tmp/pdf_write_ogr.pdf")

    gdal.Unlink("tmp/test.csv")
    gdal.Unlink("tmp/test.vrt")

    if pdf_is_poppler() or pdf_is_pdfium():
        assert layers == [
            "LAYER_00_NAME=A_Layer",
            "LAYER_01_NAME=A_Layer.Text",
        ], "did not get expected layers"

    # Should have filtered out id = 4
    assert feature_count == 3, "did not get expected feature count"


###############################################################################
# Test adding a OGR datasource with reprojection of OGR SRS to GDAL SRS


@pytest.mark.require_driver("CSV")
def test_pdf_write_ogr_with_reprojection(poppler_or_pdfium):

    f = gdal.VSIFOpenL("tmp/test.csv", "wb")
    data = """WKT,id
"POINT (-117.641059792392142 33.902263065734573)",1
"POINT (-117.64098016484607 33.891620919037436)",2
"POINT (-117.62829768175105 33.902328822481238)",3
"POINT (-117.628219639160108 33.891686649558416)",4
"POINT (-117.634639319537328 33.896975031776485)",5
"POINT (-121.488694798047447 0.0)",6
"""

    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFCloseL(f)

    f = gdal.VSIFOpenL("tmp/test.vrt", "wb")
    data = """<OGRVRTDataSource>
  <OGRVRTLayer name="test">
    <SrcDataSource relativeToVRT="0" shared="1">tmp/test.csv</SrcDataSource>
    <SrcLayer>test</SrcLayer>
    <GeometryType>wkbUnknown</GeometryType>
    <LayerSRS>+proj=longlat +datum=NAD27</LayerSRS>
    <Field name="id" type="Integer" src="id"/>
  </OGRVRTLayer>
</OGRVRTDataSource>
"""
    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFCloseL(f)

    options = [
        "OGR_DATASOURCE=tmp/test.vrt",
        "OGR_DISPLAY_LAYER_NAMES=A_Layer",
        "OGR_DISPLAY_FIELD=foo",
    ]

    src_ds = gdal.Open("data/byte.tif")
    ds = gdaltest.pdf_drv.CreateCopy(
        "tmp/pdf_write_ogr_with_reprojection.pdf", src_ds, options=options
    )
    del ds
    src_ds = None

    ogr_ds = ogr.Open("tmp/pdf_write_ogr_with_reprojection.pdf")
    ogr_lyr = ogr_ds.GetLayer(0)
    feature_count = ogr_lyr.GetFeatureCount()
    ogr_ds = None

    gdal.GetDriverByName("PDF").Delete("tmp/pdf_write_ogr_with_reprojection.pdf")

    gdal.Unlink("tmp/test.csv")
    gdal.Unlink("tmp/test.vrt")

    # Should have filtered out id = 6
    assert feature_count == 5, "did not get expected feature count"


###############################################################################
# Test direct copy of source JPEG file


@pytest.mark.require_driver("JPEG")
def test_pdf_jpeg_direct_copy(poppler_or_pdfium):

    src_ds = gdal.Open("data/jpeg/byte_with_xmp.jpg")
    ds = gdaltest.pdf_drv.CreateCopy(
        "tmp/pdf_jpeg_direct_copy.pdf", src_ds, options=["XMP=NO"]
    )
    ds = None
    src_ds = None

    ds = gdal.Open("tmp/pdf_jpeg_direct_copy.pdf")
    # No XMP at PDF level
    assert ds.GetMetadata("xml:XMP") is None
    assert ds.RasterXSize == 20
    assert not (pdf_checksum_available() and ds.GetRasterBand(1).Checksum() == 0)
    ds = None

    # But we can find the original XMP from the JPEG file !
    f = open("tmp/pdf_jpeg_direct_copy.pdf", "rb")
    data = f.read().decode("ISO-8859-1")
    f.close()
    offset = data.find("ns.adobe.com")

    gdal.Unlink("tmp/pdf_jpeg_direct_copy.pdf")

    assert offset != -1


###############################################################################
# Test direct copy of source JPEG file within VRT file


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
@pytest.mark.require_driver("JPEG")
def test_pdf_jpeg_in_vrt_direct_copy(poppler_or_pdfium):

    src_ds = gdal.Open(
        """<VRTDataset rasterXSize="20" rasterYSize="20">
  <SRS>PROJCS["NAD27 / UTM zone 11N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke 1866",6378206.4,294.9786982139006,AUTHORITY["EPSG","7008"]],AUTHORITY["EPSG","6267"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4267"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AUTHORITY["EPSG","26711"]]</SRS>
  <GeoTransform>  4.4072000000000000e+05,  6.0000000000000000e+01,  0.0000000000000000e+00,  3.7513200000000000e+06,  0.0000000000000000e+00, -6.0000000000000000e+01</GeoTransform>
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/jpeg/byte_with_xmp.jpg</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="1" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>"""
    )
    ds = gdaltest.pdf_drv.CreateCopy("tmp/pdf_jpeg_in_vrt_direct_copy.pdf", src_ds)
    ds = None
    src_ds = None

    ds = gdal.Open("tmp/pdf_jpeg_in_vrt_direct_copy.pdf")
    # No XMP at PDF level
    assert ds.GetMetadata("xml:XMP") is None
    assert ds.RasterXSize == 20
    assert not (pdf_checksum_available() and ds.GetRasterBand(1).Checksum() == 0)
    ds = None

    # But we can find the original XMP from the JPEG file !
    f = open("tmp/pdf_jpeg_in_vrt_direct_copy.pdf", "rb")
    data = f.read().decode("ISO-8859-1")
    f.close()
    offset = data.find("ns.adobe.com")

    gdal.Unlink("tmp/pdf_jpeg_in_vrt_direct_copy.pdf")

    assert offset != -1


###############################################################################
# Test reading georeferencing attached to an image, and not to the page (#4695)


@pytest.mark.parametrize("src_filename", ["data/byte.tif", "data/rgbsmall.tif"])
def pdf_georef_on_image(src_filename, pdf_backend):
    src_ds = gdal.Open(src_filename)
    with gdal.config_option("GDAL_PDF_WRITE_GEOREF_ON_IMAGE", "YES"):
        out_ds = gdaltest.pdf_drv.CreateCopy(
            "tmp/pdf_georef_on_image.pdf",
            src_ds,
            options=["MARGIN=10", "GEO_ENCODING=NONE"],
        )
        del out_ds
    if pdf_checksum_available():
        src_cs = src_ds.GetRasterBand(1).Checksum()
    else:
        src_cs = 0
    src_ds = None

    ds = gdal.Open("tmp/pdf_georef_on_image.pdf")
    subdataset_name = ds.GetMetadataItem("SUBDATASET_1_NAME", "SUBDATASETS")
    ds = None

    ds = gdal.Open(subdataset_name)
    got_wkt = ds.GetProjectionRef()
    if pdf_checksum_available():
        got_cs = ds.GetRasterBand(1).Checksum()
    else:
        got_cs = 0
    ds = None

    gdal.GetDriverByName("PDF").Delete("tmp/pdf_georef_on_image.pdf")

    assert got_wkt != "", "did not get projection"

    assert not pdf_checksum_available() or src_cs == got_cs, "did not get same checksum"


###############################################################################
# Test writing a PDF that hits Acrobat limits in term of page dimensions (#5412)


def test_pdf_write_huge(poppler_or_pdfium):
    if pdf_is_poppler() or pdf_is_pdfium():
        tmp_filename = "/vsimem/pdf_write_huge.pdf"
    else:
        tmp_filename = "tmp/pdf_write_huge.pdf"

    for (xsize, ysize) in [(19200, 1), (1, 19200)]:
        src_ds = gdal.GetDriverByName("MEM").Create("", xsize, ysize, 1)
        ds = gdaltest.pdf_drv.CreateCopy(tmp_filename, src_ds)
        ds = None
        ds = gdal.Open(tmp_filename)
        assert int(ds.GetMetadataItem("DPI")) == 96
        assert (
            ds.RasterXSize == src_ds.RasterXSize
            and ds.RasterYSize == src_ds.RasterYSize
        )
        ds = None

        gdal.ErrorReset()
        with gdal.quiet_errors():
            ds = gdaltest.pdf_drv.CreateCopy(tmp_filename, src_ds, options=["DPI=72"])
        msg = gdal.GetLastErrorMsg()
        assert msg != ""
        ds = None
        ds = gdal.Open(tmp_filename)
        assert int(ds.GetMetadataItem("DPI")) == 72
        ds = None

        src_ds = None

    for option in ["LEFT_MARGIN=14400", "TOP_MARGIN=14400"]:
        src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
        gdal.ErrorReset()
        with gdal.quiet_errors():
            ds = gdaltest.pdf_drv.CreateCopy(tmp_filename, src_ds, options=[option])
        msg = gdal.GetLastErrorMsg()
        assert msg != ""
        ds = None
        ds = gdal.Open(tmp_filename)
        assert int(ds.GetMetadataItem("DPI")) == 72
        ds = None

        src_ds = None

    gdal.Unlink(tmp_filename)


###############################################################################
# Test creating overviews


def test_pdf_overviews(poppler_or_pdfium):
    if not pdf_is_poppler() and not pdf_is_pdfium():
        pytest.skip()
    tmp_filename = "/vsimem/pdf_overviews.pdf"

    src_ds = gdal.GetDriverByName("MEM").Create("", 1024, 1024, 3)
    for i in range(3):
        src_ds.GetRasterBand(i + 1).Fill(255)
    ds = gdaltest.pdf_drv.CreateCopy(tmp_filename, src_ds)
    src_ds = None
    ds = None
    ds = gdal.Open(tmp_filename)
    before = ds.GetRasterBand(1).GetOverviewCount()
    ds.GetRasterBand(1).GetOverview(-1)
    ds.GetRasterBand(1).GetOverview(10)
    assert before >= 1
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 5934
    ds.BuildOverviews("NONE", [2])
    after = ds.GetRasterBand(1).GetOverviewCount()
    assert after == 1
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 0
    ds = None

    gdaltest.pdf_drv.Delete(tmp_filename)


###############################################################################
# Test password


def test_pdf_password(poppler_or_pdfium_or_podofo):

    if gdaltest.is_travis_branch("alpine_32bit") or gdaltest.is_travis_branch(
        "cmake-ubuntu-jammy"
    ):
        pytest.skip()

    # User password of this test file is user_password and owner password is
    # owner_password

    # No password
    with gdal.quiet_errors():
        ds = gdal.Open("data/pdf/byte_enc.pdf")
    assert ds is None

    # Wrong password
    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            "data/pdf/byte_enc.pdf", open_options=["USER_PWD=wrong_password"]
        )
    assert ds is None

    # Correct password
    ds = gdal.OpenEx("data/pdf/byte_enc.pdf", open_options=["USER_PWD=user_password"])
    assert ds is not None

    import test_cli_utilities

    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    # Test ASK_INTERACTIVE with wrong password
    cmd_line = (
        test_cli_utilities.get_gdal_translate_path()
        + " data/pdf/byte_enc.pdf /vsimem/out.tif -q -oo USER_PWD=ASK_INTERACTIVE < tmp/password.txt"
    )
    if sys.platform != "win32":
        cmd_line += " >/dev/null 2>/dev/null"

    open("tmp/password.txt", "wb").write("wrong_password".encode("ASCII"))
    ret = os.system(cmd_line)
    os.unlink("tmp/password.txt")
    assert ret != 0

    # Test ASK_INTERACTIVE with correct password
    open("tmp/password.txt", "wb").write("user_password".encode("ASCII"))
    ret = os.system(cmd_line)
    os.unlink("tmp/password.txt")
    assert ret == 0


###############################################################################
# Test multi page support


def test_pdf_multipage(poppler_or_pdfium_or_podofo):
    # byte_and_rgbsmall_2pages.pdf was generated with :
    # 1) gdal_translate gcore/data/byte.tif byte.pdf -of PDF
    # 2) gdal_translate gcore/data/rgbsmall.tif rgbsmall.pdf -of PDF
    # 3)  ~/install-podofo-0.9.3/bin/podofomerge byte.pdf rgbsmall.pdf byte_and_rgbsmall_2pages.pdf

    ds = gdal.Open("data/pdf/byte_and_rgbsmall_2pages.pdf")
    subdatasets = ds.GetSubDatasets()
    expected_subdatasets = [
        (
            "PDF:1:data/pdf/byte_and_rgbsmall_2pages.pdf",
            "Page 1 of data/pdf/byte_and_rgbsmall_2pages.pdf",
        ),
        (
            "PDF:2:data/pdf/byte_and_rgbsmall_2pages.pdf",
            "Page 2 of data/pdf/byte_and_rgbsmall_2pages.pdf",
        ),
    ]
    assert subdatasets == expected_subdatasets, "did not get expected subdatasets"
    ds = None

    ds = gdal.Open("PDF:1:data/pdf/byte_and_rgbsmall_2pages.pdf")
    assert ds.RasterXSize == 20, "wrong width"

    ds2 = gdal.Open("PDF:2:data/pdf/byte_and_rgbsmall_2pages.pdf")
    assert ds2.RasterXSize == 50, "wrong width"

    with gdal.quiet_errors():
        ds3 = gdal.Open("PDF:0:data/pdf/byte_and_rgbsmall_2pages.pdf")
    assert ds3 is None

    with gdal.quiet_errors():
        ds3 = gdal.Open("PDF:3:data/pdf/byte_and_rgbsmall_2pages.pdf")
    assert ds3 is None

    with gdal.quiet_errors():
        ds = gdal.Open("PDF:1:/does/not/exist.pdf")
    assert ds is None


###############################################################################
# Test PAM metadata support


def test_pdf_metadata(poppler_or_pdfium):
    gdal.Translate(
        "tmp/pdf_metadata.pdf",
        "data/byte.tif",
        format="PDF",
        metadataOptions=["FOO=BAR"],
    )
    ds = gdal.Open("tmp/pdf_metadata.pdf")
    md = ds.GetMetadata()
    assert "FOO" in md
    ds = None
    ds = gdal.Open("tmp/pdf_metadata.pdf")
    assert ds.GetMetadataItem("FOO") == "BAR"
    ds = None

    gdal.GetDriverByName("PDF").Delete("tmp/pdf_metadata.pdf")


###############################################################################
# Test PAM support with subdatasets


def test_pdf_pam_subdatasets(poppler_or_pdfium, tmp_path):

    tmpfilename = str(tmp_path / "test_pdf_pam_subdatasets.pdf")
    shutil.copy("data/pdf/byte_and_rgbsmall_2pages.pdf", tmpfilename)

    ds = gdal.Open("PDF:1:" + tmpfilename)
    ds.GetRasterBand(1).ComputeStatistics(False)
    ds = None
    assert gdal.VSIStatL(tmpfilename + ".aux.xml")
    ds = gdal.Open("PDF:1:" + tmpfilename)
    assert ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MINIMUM") is not None
    ds = None


###############################################################################
# Test PAM georef support


def test_pdf_pam_georef(poppler_or_pdfium):
    src_ds = gdal.Open("data/byte.tif")

    # Default behaviour should result in no PAM file
    gdaltest.pdf_drv.CreateCopy("tmp/pdf_pam_georef.pdf", src_ds)
    assert not os.path.exists("tmp/pdf_pam_georef.pdf.aux.xml")

    # Now disable internal georeferencing, so georef should go to PAM
    gdaltest.pdf_drv.CreateCopy(
        "tmp/pdf_pam_georef.pdf", src_ds, options=["GEO_ENCODING=NONE"]
    )
    assert os.path.exists("tmp/pdf_pam_georef.pdf.aux.xml")

    ds = gdal.Open("tmp/pdf_pam_georef.pdf")
    assert ds.GetGeoTransform() == src_ds.GetGeoTransform()
    assert ds.GetProjectionRef() == src_ds.GetProjectionRef()
    ds = None

    gdal.GetDriverByName("PDF").Delete("tmp/pdf_pam_georef.pdf")


###############################################################################
# Test XML composition


@pytest.mark.require_driver("CSV")
def test_pdf_composition():

    xml_content = """<PDFComposition>
    <Metadata>
        <Author>Even</Author>
        <XMP>&lt;?xpacket begin='' id='W5M0MpCehiHzreSzNTczkc9d'?&gt;
&lt;x:xmpmeta xmlns:x='adobe:ns:meta/' x:xmptk='Image::ExifTool 7.89'&gt;
&lt;rdf:RDF xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#'&gt;
&lt;/rdf:RDF&gt;
&lt;/x:xmpmeta&gt;
&lt;?xpacket end='w'?&gt;
</XMP>
    </Metadata>
    <Javascript>button = app.alert({cMsg: 'This file was generated by GDAL. Do you want to visit its website ?', cTitle: 'Question', nIcon:2, nType:2});
if (button == 4) app.launchURL('http://gdal.org/');</Javascript>

    <LayerTree>
        <Layer id="l1" name="Satellite imagery"/>
        <Layer id="l2" name="OSM data">
            <Layer id="l2.1" name="Roads" initiallyVisible="false"/>
            <Layer id="l2.2" name="Buildings" mutuallyExclusiveGroupId="group1">
                <Layer id="l2.2.text" name="Buildings name"/>
            </Layer>
            <Layer id="l2.3" name="Cadastral parcels" mutuallyExclusiveGroupId="group1"/>
        </Layer>
    </LayerTree>

    <Page>
        <DPI>72</DPI>
        <Width>20</Width>
        <Height>10</Height>

        <Georeferencing ISO32000ExtensionFormat="true">
            <SRS dataAxisToSRSAxisMapping="2,1">EPSG:4326</SRS>
            <BoundingBox x1="1" y1="1" x2="19" y2="9"/>
            <BoundingPolygon>POLYGON((1 1,19 1,19 9,1 9,1 1))</BoundingPolygon>
            <ControlPoint x="1"  y="1"  GeoY="-90"  GeoX="-180"/>
            <ControlPoint x="19" y="1"  GeoY="90"   GeoX="180"/>
            <ControlPoint x="1"  y="9"  GeoY="-90"  GeoX="-180"/>
            <ControlPoint x="19" y="9"  GeoY="90"   GeoX="180"/>
        </Georeferencing>

        <Content streamCompression="NONE">
            <IfLayerOn layerId="l1">
                <Raster dataset="data/byte.tif" x1="1" y1="1" x2="9" y2="9">
                    <Blending function="Multiply" opacity="0.7"/>
                </Raster>
            </IfLayerOn>
            <Vector dataset="/vsimem/test.csv" layer="test" visible="false">
                <LogicalStructure>
                    <ExcludeAllFields>true</ExcludeAllFields>
                </LogicalStructure>
            </Vector>
            <Vector dataset="/vsimem/test2.csv" layer="test2" visible="false"
                    linkAttribute="link">
                <LogicalStructure displayLayerName="another layer" fieldToDisplay="id">
                    <IncludeField>id</IncludeField>
                    <IncludeField>link</IncludeField>
                </LogicalStructure>
            </Vector>
            <Vector dataset="/vsimem/sym.csv" layer="sym">
                <LogicalStructure>
                    <ExcludeField>WKT</ExcludeField>
                    <ExcludeField>OGR_STYLE</ExcludeField>
                </LogicalStructure>
            </Vector>
            <VectorLabel dataset="/vsimem/label.csv" layer="label"/>
        </Content>
    </Page>
</PDFComposition>"""

    gdal.FileFromMemBuffer(
        "/vsimem/test.csv",
        """id,WKT
1,"POLYGON((0.5 0.5,0.5 9.5,10 9.5,10 0.5,0.5 0.5))"
2,"POLYGON((10.5 0.5,10.5 4.5,20 4.5,20 0.5,10.5 0.5))"
""",
    )

    gdal.FileFromMemBuffer(
        "/vsimem/test2.csv",
        """id,link,WKT
3,"http://gdal.org","POLYGON((10.5 5.5,10.5 9.5,20 9.5,20 5.5,10.5 5.5))"
""",
    )

    gdal.FileFromMemBuffer(
        "/vsimem/sym.csv",
        """id,WKT,OGR_STYLE
1,"POINT(15 7)","SYMBOL(c:#00FF00,id:""ogr-sym-1"",s:1pt)"
2,"POINT(15 5)","SYMBOL(c:#00FF0077,id:""ogr-sym-1"",s:1pt)"
3,"POINT(15 3)","SYMBOL(id:""data/byte.tif"",s:1pt)"
""",
    )

    gdal.FileFromMemBuffer(
        "/vsimem/label.csv",
        """id,WKT,OGR_STYLE
1,"POINT(15 1)","LABEL(t:""foo"",s:1pt)"
""",
    )

    out_filename = "tmp/tmp.pdf"
    out_ds = gdaltest.pdf_drv.Create(
        out_filename,
        0,
        0,
        0,
        gdal.GDT_Unknown,
        options=["COMPOSITION_FILE=" + xml_content],
    )
    gdal.Unlink("/vsimem/test.csv")
    gdal.Unlink("/vsimem/test2.csv")
    gdal.Unlink("/vsimem/sym.csv")
    gdal.Unlink("/vsimem/label.csv")
    assert out_ds
    assert gdal.GetLastErrorMsg() == ""

    f = open("data/pdf/test_pdf_composition.pdf", "rb")
    data_ref = f.read()
    f.close()

    f = open("data/pdf/test_pdf_composition_libpng_1_6_40.pdf", "rb")
    data_ref2 = f.read()
    f.close()

    f = open(out_filename, "rb")
    data_got = f.read()
    f.close()

    assert (
        data_got == data_ref or data_got == data_ref2
    ), "content does not match reference content"

    gdal.Unlink(out_filename)


def test_pdf_composition_raster_tiled_blending():

    xml_content = """<PDFComposition>
    <Page>
        <DPI>72</DPI>
        <Width>20</Width>
        <Height>20</Height>
        <Content streamCompression="NONE">
            <Raster dataset="data/byte.tif" tileSize="16">
                <Blending function="Multiply" opacity="0.7"/>
            </Raster>
        </Content>
    </Page>
</PDFComposition>"""

    out_filename = "tmp/tmp.pdf"
    out_ds = gdaltest.pdf_drv.Create(
        out_filename,
        0,
        0,
        0,
        gdal.GDT_Unknown,
        options=["COMPOSITION_FILE=" + xml_content],
    )
    assert out_ds
    assert gdal.GetLastErrorMsg() == ""

    f = open("data/pdf/test_pdf_composition_raster_tiled_blending.pdf", "rb")
    data_ref = f.read()
    f.close()

    f = open(
        "data/pdf/test_pdf_composition_raster_tiled_blending_libpng_1_6_40.pdf", "rb"
    )
    data_ref2 = f.read()
    f.close()

    f = open(out_filename, "rb")
    data_got = f.read()
    f.close()

    assert (
        data_got == data_ref or data_got == data_ref2
    ), "content does not match reference content"

    gdal.Unlink(out_filename)


def test_pdf_composition_pdf_content(poppler_or_pdfium_or_podofo):

    xml_content = """<PDFComposition>
    <Page>
        <Width>20</Width>
        <Height>20</Height>
        <Content streamCompression="NONE">
            <PDF dataset="data/pdf/test_iso32000.pdf">
                <Blending function="Normal" opacity="1"/>
            </PDF>
        </Content>
    </Page>
</PDFComposition>"""

    out_filename = "tmp/tmp.pdf"
    out_ds = gdaltest.pdf_drv.Create(
        out_filename,
        0,
        0,
        0,
        gdal.GDT_Unknown,
        options=["COMPOSITION_FILE=" + xml_content],
    )
    assert out_ds
    assert gdal.GetLastErrorMsg() == ""

    # Poppler output
    f = open("data/pdf/test_pdf_composition_pdf_content.pdf", "rb")
    data_ref = f.read()
    f.close()

    # PDFium output
    f = open("data/pdf/test_pdf_composition_pdf_content_pdfium.pdf", "rb")
    data_ref_pdfium = f.read()
    f.close()

    f = open(out_filename, "rb")
    data_got = f.read()
    f.close()

    assert data_got in (
        data_ref,
        data_ref_pdfium,
    ), "content does not match reference content"

    gdal.Unlink(out_filename)


def test_pdf_composition_error_pdf_content_missing_filename(
    poppler_or_pdfium_or_podofo,
):

    xml_content = """<PDFComposition>
    <Page>
        <Width>20</Width>
        <Height>20</Height>
        <Content>
            <PDF/>
        </Content>
    </Page>
</PDFComposition>"""

    out_filename = "/vsimem/tmp.pdf"
    with gdal.quiet_errors():
        out_ds = gdaltest.pdf_drv.Create(
            out_filename,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=["COMPOSITION_FILE=" + xml_content],
        )
    assert not out_ds
    assert gdal.GetLastErrorMsg() == "Missing dataset"
    gdal.Unlink(out_filename)


def test_pdf_composition_error_pdf_content_non_existing(poppler_or_pdfium_or_podofo):

    xml_content = """<PDFComposition>
    <Page>
        <Width>20</Width>
        <Height>20</Height>
        <Content>
            <PDF dataset="/vsimem/non_existing.pdf"/>
        </Content>
    </Page>
</PDFComposition>"""

    out_filename = "/vsimem/tmp.pdf"
    with gdal.quiet_errors():
        out_ds = gdaltest.pdf_drv.Create(
            out_filename,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=["COMPOSITION_FILE=" + xml_content],
        )
    assert not out_ds
    assert gdal.GetLastErrorMsg() == "/vsimem/non_existing.pdf is not a valid PDF file"
    gdal.Unlink(out_filename)


def test_pdf_composition_error_pdf_content_missing_contents(
    poppler_or_pdfium_or_podofo,
):

    xml_content = """<PDFComposition>
    <Page>
        <Width>20</Width>
        <Height>20</Height>
        <Content>
            <PDF dataset="data/pdf/missing_contents.pdf"/>
        </Content>
    </Page>
</PDFComposition>"""

    out_filename = "/vsimem/tmp.pdf"
    with gdal.quiet_errors():
        out_ds = gdaltest.pdf_drv.Create(
            out_filename,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=["COMPOSITION_FILE=" + xml_content],
        )
    assert not out_ds
    assert gdal.GetLastErrorMsg() == "Missing Contents"
    gdal.Unlink(out_filename)


def test_pdf_composition_error_pdf_content_missing_contents_stream(
    poppler_or_pdfium_or_podofo,
):

    xml_content = """<PDFComposition>
    <Page>
        <Width>20</Width>
        <Height>20</Height>
        <Content>
            <PDF dataset="data/pdf/missing_stream.pdf"/>
        </Content>
    </Page>
</PDFComposition>"""

    out_filename = "/vsimem/tmp.pdf"
    with gdal.quiet_errors():
        out_ds = gdaltest.pdf_drv.Create(
            out_filename,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=["COMPOSITION_FILE=" + xml_content],
        )
    assert not out_ds
    assert gdal.GetLastErrorMsg() in (
        "Missing Contents",
        "Missing Contents stream",
        "data/pdf/missing_stream.pdf is not a valid PDF file",
    )
    gdal.Unlink(out_filename)


def test_pdf_composition_error_pdf_content_missing_resources(
    poppler_or_pdfium_or_podofo,
):

    xml_content = """<PDFComposition>
    <Page>
        <Width>20</Width>
        <Height>20</Height>
        <Content>
            <PDF dataset="data/pdf/missing_resources.pdf"/>
        </Content>
    </Page>
</PDFComposition>"""

    out_filename = "/vsimem/tmp.pdf"
    with gdal.quiet_errors():
        out_ds = gdaltest.pdf_drv.Create(
            out_filename,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=["COMPOSITION_FILE=" + xml_content],
        )
    assert not out_ds
    assert gdal.GetLastErrorMsg() == "Missing Resources"
    gdal.Unlink(out_filename)


def test_pdf_composition_raster_georeferenced():

    xml_content = """<PDFComposition>
    <Page>
        <DPI>72</DPI>
        <Width>110</Width>
        <Height>110</Height>

        <Georeferencing id="georeferenced">
            <SRS>EPSG:26711</SRS>
            <BoundingBox x1="10" y1="5" x2="105" y2="100"/>
            <ControlPoint x="5"  y="5"   GeoY="3750120"  GeoX="440720"/>
            <ControlPoint x="5"  y="105"  GeoY="3751320"  GeoX="440720"/>
            <ControlPoint x="105"  y="5"  GeoY="3750120"  GeoX="441920"/>
            <ControlPoint x="105" y="105"  GeoY="3751320"  GeoX="441920"/>
        </Georeferencing>

        <Content streamCompression="NONE">
                <Raster georeferencingId="georeferenced" dataset="data/byte.tif"/>
        </Content>

    </Page>

</PDFComposition>
"""

    out_filename = "tmp/tmp.pdf"
    with gdaltest.config_option("PDF_COORD_DOUBLE_PRECISION", "12"):
        out_ds = gdaltest.pdf_drv.Create(
            out_filename,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=["COMPOSITION_FILE=" + xml_content],
        )
    assert out_ds
    assert gdal.GetLastErrorMsg() == ""

    f = open("data/pdf/test_pdf_composition_raster_georeferenced.pdf", "rb")
    data_ref = f.read()
    f.close()

    f = open(
        "data/pdf/test_pdf_composition_raster_georeferenced_libpng_1_6_40.pdf", "rb"
    )
    data_ref2 = f.read()
    f.close()

    f = open(out_filename, "rb")
    data_got = f.read()
    f.close()

    assert (
        data_got == data_ref or data_got == data_ref2
    ), "content does not match reference content"

    gdal.Unlink(out_filename)


def test_pdf_composition_vector_georeferenced():

    xml_content = """<PDFComposition>
    <Page>
        <DPI>72</DPI>
        <Width>110</Width>
        <Height>110</Height>

        <Georeferencing id="georeferenced">
            <SRS dataAxisToSRSAxisMapping="2,1">EPSG:4326</SRS>
            <BoundingBox x1="10" y1="5" x2="105" y2="105"/>
            <ControlPoint x="5"  y="5"   GeoY="49"  GeoX="2"/>
            <ControlPoint x="5"  y="105"  GeoY="50"  GeoX="2"/>
            <ControlPoint x="105"  y="5"  GeoY="49"  GeoX="3"/>
            <ControlPoint x="105" y="105"  GeoY="50"  GeoX="3"/>
        </Georeferencing>

        <Content streamCompression="NONE">
                <Vector georeferencingId="georeferenced" dataset="/vsimem/test.shp" layer="test">
                    <LogicalStructure/>
                </Vector>
                <VectorLabel georeferencingId="georeferenced" dataset="/vsimem/test.shp" layer="test"/>
        </Content>

    </Page>

</PDFComposition>
"""

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource("/vsimem/test.shp")
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = ds.CreateLayer("test", srs=srs)
    lyr.CreateField(ogr.FieldDefn("OGR_STYLE"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(2.5 49.5)"))
    f["OGR_STYLE"] = 'SYMBOL(c:#00FF00,id:"ogr-sym-1",s:1pt);LABEL(t:"foo",s:12pt)'
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(0 0)"))
    lyr.CreateFeature(f)
    ds = None

    out_filename = "tmp/tmp.pdf"
    out_ds = gdaltest.pdf_drv.Create(
        out_filename,
        0,
        0,
        0,
        gdal.GDT_Unknown,
        options=["COMPOSITION_FILE=" + xml_content],
    )
    assert out_ds
    assert gdal.GetLastErrorMsg() == ""

    ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource("/vsimem/test.shp")

    f = open("data/pdf/test_pdf_composition_vector_georeferenced.pdf", "rb")
    data_ref = f.read()
    f.close()

    f = open(out_filename, "rb")
    data_got = f.read()
    f.close()

    assert data_ref == data_got, "content does not match reference content"

    gdal.Unlink(out_filename)


def test_pdf_composition_vector_georeferenced_reprojected():

    xml_content = """<PDFComposition>
    <Page>
        <DPI>72</DPI>
        <Width>110</Width>
        <Height>110</Height>

        <Georeferencing id="georeferenced">
            <SRS dataAxisToSRSAxisMapping="2,1">EPSG:4326</SRS>
            <BoundingBox x1="10" y1="5" x2="105" y2="105"/>
            <ControlPoint x="5"  y="5"   GeoY="49"  GeoX="2"/>
            <ControlPoint x="5"  y="105"  GeoY="50"  GeoX="2"/>
            <ControlPoint x="105"  y="5"  GeoY="49"  GeoX="3"/>
            <ControlPoint x="105" y="105"  GeoY="50"  GeoX="3"/>
        </Georeferencing>

        <Content streamCompression="NONE">
                <Vector georeferencingId="georeferenced" dataset="/vsimem/test.shp" layer="test">
                    <LogicalStructure/>
                </Vector>
                <VectorLabel georeferencingId="georeferenced" dataset="/vsimem/test.shp" layer="test"/>
        </Content>

    </Page>

</PDFComposition>
"""

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource("/vsimem/test.shp")
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    lyr = ds.CreateLayer("test", srs=srs)
    lyr.CreateField(ogr.FieldDefn("OGR_STYLE"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(
        ogr.CreateGeometryFromWkt("POINT(463796.280705071 5483160.94881072)")
    )
    f["OGR_STYLE"] = 'SYMBOL(c:#00FF00,id:"ogr-sym-1",s:1pt);LABEL(t:"foo",s:12pt)'
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(0 0)"))
    lyr.CreateFeature(f)
    ds = None

    out_filename = "tmp/tmp.pdf"
    out_ds = gdaltest.pdf_drv.Create(
        out_filename,
        0,
        0,
        0,
        gdal.GDT_Unknown,
        options=["COMPOSITION_FILE=" + xml_content],
    )
    assert out_ds
    assert gdal.GetLastErrorMsg() == ""

    ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource("/vsimem/test.shp")

    f = open("data/pdf/test_pdf_composition_vector_georeferenced.pdf", "rb")
    data_ref = f.read()
    f.close()

    f = open(out_filename, "rb")
    data_got = f.read()
    f.close()

    assert data_ref == data_got, "content does not match reference content"

    gdal.Unlink(out_filename)


def test_pdf_composition_layer_tree_displayOnlyOnVisiblePages():

    xml_content = """<PDFComposition>
    <LayerTree displayOnlyOnVisiblePages="true">
        <Layer id="l1" name="Layer of page 1"/>
        <Layer id="l2" name="Layer of page 2"/>
    </LayerTree>
    <Page>
        <Width>20</Width>
        <Height>20</Height>
        <Content streamCompression="NONE">
            <IfLayerOn layerId="l1"/>
        </Content>
    </Page>
    <Page>
        <Width>20</Width>
        <Height>20</Height>
        <Content streamCompression="NONE">
            <IfLayerOn layerId="l2"/>
        </Content>
    </Page>
</PDFComposition>"""

    out_filename = "tmp/tmp.pdf"
    out_ds = gdaltest.pdf_drv.Create(
        out_filename,
        0,
        0,
        0,
        gdal.GDT_Unknown,
        options=["COMPOSITION_FILE=" + xml_content],
    )
    assert out_ds
    assert gdal.GetLastErrorMsg() == ""

    f = open(
        "data/pdf/test_pdf_composition_layer_tree_displayOnlyOnVisiblePages.pdf", "rb"
    )
    data_ref = f.read()
    f.close()

    f = open(out_filename, "rb")
    data_got = f.read()
    f.close()

    assert data_ref == data_got, "content does not match reference content"

    gdal.Unlink(out_filename)


def test_pdf_composition_outline():

    xml_content = """<PDFComposition>
    <LayerTree>
        <Layer id="foo" name="foo"/>
        <Layer id="bar" name="bar"/>
    </LayerTree>

    <Page id="1">
        <Width>20</Width>
        <Height>20</Height>
        <Content streamCompression="NONE"/>
    </Page>
    <Page id="2">
        <Width>20</Width>
        <Height>20</Height>
        <Content streamCompression="NONE"/>
    </Page>

    <Outline>
        <OutlineItem name="turn all on" italic="true">
            <Actions>
                <SetAllLayersStateAction visible="true"/>
            </Actions>
        </OutlineItem>
        <OutlineItem name="turn all off" bold="true">
            <Actions>
                <SetAllLayersStateAction visible="false"/>
            </Actions>
        </OutlineItem>
        <OutlineItem name="foo on, bar off + fullscreen">
            <Actions>
                <SetLayerStateAction visible="true" layerId="foo"/>
                <SetLayerStateAction visible="false" layerId="bar"/>
                <JavascriptAction>app.fs.isFullScreen = true;</JavascriptAction>
            </Actions>
        </OutlineItem>
        <OutlineItem name="1: page 1">
            <Actions>
                <GotoPageAction pageId="1"/>
            </Actions>
            <OutlineItem name="1.1" open="false">
                <OutlineItem name="1.1.1: Page 1">
                    <Actions>
                        <GotoPageAction pageId="1"/>
                    </Actions>
                </OutlineItem>
                <OutlineItem name="1.1.2: Page 2">
                    <Actions>
                        <GotoPageAction pageId="2"/>
                    </Actions>
                </OutlineItem>
            </OutlineItem>
            <OutlineItem name="1.2: Page 2(zoomed in) + foo on">
                <Actions>
                    <GotoPageAction pageId="2" x1="1" y1="2" x2="3" y2="4"/>
                    <SetLayerStateAction visible="true" layerId="foo"/>
                </Actions>
            </OutlineItem>
        </OutlineItem>
        <OutlineItem name="2: page 2">
            <Actions>
                <GotoPageAction pageId="2"/>
            </Actions>
        </OutlineItem>
    </Outline>

</PDFComposition>"""

    out_filename = "tmp/tmp.pdf"
    out_ds = gdaltest.pdf_drv.Create(
        out_filename,
        0,
        0,
        0,
        gdal.GDT_Unknown,
        options=["COMPOSITION_FILE=" + xml_content],
    )
    assert out_ds
    assert gdal.GetLastErrorMsg() == ""

    f = open("data/pdf/test_pdf_composition_outline.pdf", "rb")
    data_ref = f.read()
    f.close()

    f = open(out_filename, "rb")
    data_got = f.read()
    f.close()

    assert data_ref == data_got, "content does not match reference content"

    gdal.Unlink(out_filename)


def test_pdf_composition_error_missing_file():

    out_filename = "/vsimem/tmp.pdf"
    with gdal.quiet_errors():
        out_ds = gdaltest.pdf_drv.Create(
            out_filename,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=["COMPOSITION_FILE=/vsimem/missing.xml"],
        )
    assert not out_ds
    assert gdal.GetLastErrorMsg() == "Cannot open file '/vsimem/missing.xml'"
    gdal.Unlink(out_filename)


def test_pdf_composition_error_missing_page():

    xml_content = """<PDFComposition></PDFComposition>"""

    out_filename = "/vsimem/tmp.pdf"
    with gdal.quiet_errors():
        out_ds = gdaltest.pdf_drv.Create(
            out_filename,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=["COMPOSITION_FILE=" + xml_content],
        )
    assert not out_ds
    assert gdal.GetLastErrorMsg() == "At least one page should be defined"
    gdal.Unlink(out_filename)


def test_pdf_composition_error_missing_page_width():

    xml_content = """<PDFComposition><Page/></PDFComposition>"""

    out_filename = "/vsimem/tmp.pdf"
    with gdal.quiet_errors():
        out_ds = gdaltest.pdf_drv.Create(
            out_filename,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=["COMPOSITION_FILE=" + xml_content],
        )
    assert not out_ds
    assert gdal.GetLastErrorMsg() == "Missing or invalid Width and/or Height"
    gdal.Unlink(out_filename)


def test_pdf_composition_error_missing_page_content():

    xml_content = """<PDFComposition><Page><Width>1</Width><Height>1</Height></Page></PDFComposition>"""

    out_filename = "/vsimem/tmp.pdf"
    with gdal.quiet_errors():
        out_ds = gdaltest.pdf_drv.Create(
            out_filename,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=["COMPOSITION_FILE=" + xml_content],
        )
    assert not out_ds
    assert gdal.GetLastErrorMsg() == "Missing Content"
    gdal.Unlink(out_filename)


def test_pdf_composition_error_invalid_layer_missing_id():

    xml_content = """<PDFComposition>
    <LayerTree>
        <Layer name="foo"/>
    </LayerTree>
    <Page><Width>1</Width><Height>1</Height><Content/></Page>
</PDFComposition>"""

    out_filename = "/vsimem/tmp.pdf"
    with gdal.quiet_errors():
        out_ds = gdaltest.pdf_drv.Create(
            out_filename,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=["COMPOSITION_FILE=" + xml_content],
        )
    assert not out_ds
    assert gdal.GetLastErrorMsg() == "Missing id attribute in Layer"
    gdal.Unlink(out_filename)


def test_pdf_composition_error_invalid_layer_missing_name():

    xml_content = """<PDFComposition>
    <LayerTree>
        <Layer id="foo"/>
    </LayerTree>
    <Page><Width>1</Width><Height>1</Height><Content/></Page>
</PDFComposition>"""

    out_filename = "/vsimem/tmp.pdf"
    with gdal.quiet_errors():
        out_ds = gdaltest.pdf_drv.Create(
            out_filename,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=["COMPOSITION_FILE=" + xml_content],
        )
    assert not out_ds
    assert gdal.GetLastErrorMsg() == "Missing name attribute in Layer"
    gdal.Unlink(out_filename)


def test_pdf_composition_error_duplicate_layer_id():

    xml_content = """<PDFComposition>
    <LayerTree>
        <Layer id="foo" name="x"/>
        <Layer id="foo" name="y"/>
    </LayerTree>
    <Page><Width>1</Width><Height>1</Height><Content/></Page>
</PDFComposition>"""

    out_filename = "/vsimem/tmp.pdf"
    with gdal.quiet_errors():
        out_ds = gdaltest.pdf_drv.Create(
            out_filename,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=["COMPOSITION_FILE=" + xml_content],
        )
    assert not out_ds
    assert gdal.GetLastErrorMsg() == "Layer.id = foo is not unique"
    gdal.Unlink(out_filename)


def test_pdf_composition_error_referencing_invalid_layer_id():

    xml_content = """<PDFComposition>
    <LayerTree>
        <Layer id="foo" name="x"/>
    </LayerTree>
    <Page>
        <Width>1</Width><Height>1</Height>
        <Content>
            <IfLayerOn layerId="nonexisting"/>
        </Content>
    </Page>
</PDFComposition>"""

    out_filename = "/vsimem/tmp.pdf"
    with gdal.quiet_errors():
        out_ds = gdaltest.pdf_drv.Create(
            out_filename,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=["COMPOSITION_FILE=" + xml_content],
        )
    assert not out_ds
    assert gdal.GetLastErrorMsg() == "Referencing layer of unknown id: nonexisting"
    gdal.Unlink(out_filename)


def test_pdf_composition_error_missing_srs():

    xml_content = """<PDFComposition>
    <Page>

        <Georeferencing>
            <!--<SRS dataAxisToSRSAxisMapping="2,1">EPSG:4326</SRS> -->
            <BoundingBox x1="1" y1="1" x2="19" y2="9"/>
            <BoundingPolygon>POLYGON((1 1,19 1,19 9,1 9,1 1))</BoundingPolygon>
            <ControlPoint x="1"  y="1"  GeoY="-90"  GeoX="-180"/>
            <ControlPoint x="19" y="1"  GeoY="90"   GeoX="180"/>
            <ControlPoint x="1"  y="9"  GeoY="-90"  GeoX="-180"/>
            <ControlPoint x="19" y="9"  GeoY="90"   GeoX="180"/>
        </Georeferencing>

        <Width>1</Width><Height>1</Height>
        <Content>
        </Content>
    </Page>
</PDFComposition>"""

    out_filename = "/vsimem/tmp.pdf"
    with gdal.quiet_errors():
        out_ds = gdaltest.pdf_drv.Create(
            out_filename,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=["COMPOSITION_FILE=" + xml_content],
        )
    assert not out_ds
    assert gdal.GetLastErrorMsg() == "Missing SRS"
    gdal.Unlink(out_filename)


def test_pdf_composition_error_missing_control_point():

    xml_content = """<PDFComposition>
    <Page>

        <Georeferencing>
            <SRS dataAxisToSRSAxisMapping="2,1">EPSG:4326</SRS>
            <ControlPoint x="1"  y="1"  GeoY="-90"  GeoX="-180"/>
            <ControlPoint x="19" y="1"  GeoY="90"   GeoX="180"/>
            <ControlPoint x="1"  y="9"  GeoY="-90"  GeoX="-180"/>
            <!--<ControlPoint x="19" y="9"  GeoY="90"   GeoX="180"/>-->
        </Georeferencing>

        <Width>1</Width><Height>1</Height>
        <Content>
        </Content>
    </Page>
</PDFComposition>"""

    out_filename = "/vsimem/tmp.pdf"
    with gdal.quiet_errors():
        out_ds = gdaltest.pdf_drv.Create(
            out_filename,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=["COMPOSITION_FILE=" + xml_content],
        )
    assert not out_ds
    assert gdal.GetLastErrorMsg() == "At least 4 ControlPoint are required"
    gdal.Unlink(out_filename)


def test_pdf_composition_error_missing_attribute_in_control_point():

    xml_content = """<PDFComposition>
    <Page>

        <Georeferencing>
            <SRS dataAxisToSRSAxisMapping="2,1">EPSG:4326</SRS>
            <BoundingBox x1="1" y1="1" x2="19" y2="9"/>
            <BoundingPolygon>POLYGON((1 1,19 1,19 9,1 9,1 1))</BoundingPolygon>
            <ControlPoint x="1"  y="1"  GeoY="-90"  GeoX="-180"/>
            <ControlPoint x="19" y="1"  GeoY="90"   GeoX="180"/>
            <ControlPoint x="1"  y="9"  GeoY="-90"  GeoX="-180"/>
            <ControlPoint x="19" y="9"  GeoY="90"   missing___GeoX="180"/>
        </Georeferencing>

        <Width>1</Width><Height>1</Height>
        <Content>
        </Content>
    </Page>
</PDFComposition>"""

    out_filename = "/vsimem/tmp.pdf"
    with gdal.quiet_errors():
        out_ds = gdaltest.pdf_drv.Create(
            out_filename,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=["COMPOSITION_FILE=" + xml_content],
        )
    assert not out_ds
    assert (
        gdal.GetLastErrorMsg()
        == "At least one of x, y, GeoX or GeoY attribute missing on ControlPoint"
    )
    gdal.Unlink(out_filename)


def test_pdf_composition_error_invalid_bbox():

    xml_content = """<PDFComposition>
    <Page>

        <Georeferencing>
            <SRS dataAxisToSRSAxisMapping="2,1">EPSG:4326</SRS>
            <BoundingBox x1="1" y1="1" x2="1" y2="9"/>
            <BoundingPolygon>POLYGON((1 1,19 1,19 9,1 9,1 1))</BoundingPolygon>
            <ControlPoint x="1"  y="1"  GeoY="-90"  GeoX="-180"/>
            <ControlPoint x="19" y="1"  GeoY="90"   GeoX="180"/>
            <ControlPoint x="1"  y="9"  GeoY="-90"  GeoX="-180"/>
            <ControlPoint x="19" y="9"  GeoY="90"   GeoX="180"/>
        </Georeferencing>

        <Width>1</Width><Height>1</Height>
        <Content>
        </Content>
    </Page>
</PDFComposition>"""

    out_filename = "/vsimem/tmp.pdf"
    with gdal.quiet_errors():
        out_ds = gdaltest.pdf_drv.Create(
            out_filename,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=["COMPOSITION_FILE=" + xml_content],
        )
    assert not out_ds
    assert gdal.GetLastErrorMsg() == "Invalid BoundingBox"
    gdal.Unlink(out_filename)


def test_pdf_composition_error_missing_dataset_attribute():

    xml_content = """<PDFComposition>
    <Page>
        <Width>1</Width><Height>1</Height>
        <Content>
            <Raster/>
        </Content>
    </Page>
</PDFComposition>"""

    out_filename = "/vsimem/tmp.pdf"
    with gdal.quiet_errors():
        out_ds = gdaltest.pdf_drv.Create(
            out_filename,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=["COMPOSITION_FILE=" + xml_content],
        )
    assert not out_ds
    assert gdal.GetLastErrorMsg() == "Missing dataset"
    gdal.Unlink(out_filename)


def test_pdf_composition_error_invalid_dataset():

    xml_content = """<PDFComposition>
    <Page>
        <Width>1</Width><Height>1</Height>
        <Content>
            <Raster dataset="non_existing"/>
        </Content>
    </Page>
</PDFComposition>"""

    out_filename = "/vsimem/tmp.pdf"
    with gdal.quiet_errors():
        out_ds = gdaltest.pdf_drv.Create(
            out_filename,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=["COMPOSITION_FILE=" + xml_content],
        )
    assert not out_ds
    assert gdal.GetLastErrorMsg() != ""
    gdal.Unlink(out_filename)


def test_pdf_composition_duplicate_page_id():

    xml_content = """<PDFComposition>
    <Page id="1">
        <Width>20</Width>
        <Height>20</Height>
        <Content streamCompression="NONE"/>
    </Page>
    <Page id="1">
        <Width>20</Width>
        <Height>20</Height>
        <Content streamCompression="NONE"/>
    </Page>

</PDFComposition>"""

    out_filename = "/vsimem/tmp.pdf"
    with gdal.quiet_errors():
        out_ds = gdaltest.pdf_drv.Create(
            out_filename,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=["COMPOSITION_FILE=" + xml_content],
        )
    assert not out_ds
    assert gdal.GetLastErrorMsg() == "Duplicated page id 1"
    gdal.Unlink(out_filename)


def test_pdf_composition_outline_item_gotopage_action_missing_page_id():

    xml_content = """<PDFComposition>
    <Page id="1">
        <Width>20</Width>
        <Height>20</Height>
        <Content streamCompression="NONE"/>
    </Page>

    <Outline>
        <OutlineItem name="name">
            <Actions>
                <GotoPageAction/>
            </Actions>
        </OutlineItem>
    </Outline>

</PDFComposition>"""

    out_filename = "/vsimem/tmp.pdf"
    with gdal.quiet_errors():
        out_ds = gdaltest.pdf_drv.Create(
            out_filename,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=["COMPOSITION_FILE=" + xml_content],
        )
    assert not out_ds
    assert gdal.GetLastErrorMsg() == "Missing pageId attribute in GotoPageAction"
    gdal.Unlink(out_filename)


def test_pdf_composition_outline_item_gotopage_action_pointing_to_invalid_page_id():

    xml_content = """<PDFComposition>
    <Page id="1">
        <Width>20</Width>
        <Height>20</Height>
        <Content streamCompression="NONE"/>
    </Page>

    <Outline>
        <OutlineItem name="name">
            <Actions>
                <GotoPageAction pageId="non_existing"/>
            </Actions>
        </OutlineItem>
    </Outline>

</PDFComposition>"""

    out_filename = "/vsimem/tmp.pdf"
    with gdal.quiet_errors():
        out_ds = gdaltest.pdf_drv.Create(
            out_filename,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=["COMPOSITION_FILE=" + xml_content],
        )
    assert not out_ds
    assert (
        gdal.GetLastErrorMsg()
        == "GotoPageAction.pageId = non_existing not pointing to a Page.id"
    )
    gdal.Unlink(out_filename)


def test_pdf_composition_outline_item_setlayerstate_missing_layer_id():

    xml_content = """<PDFComposition>
    <Page id="1">
        <Width>20</Width>
        <Height>20</Height>
        <Content streamCompression="NONE"/>
    </Page>

    <Outline>
        <OutlineItem name="name">
            <Actions>
                <SetLayerStateAction visible="true"/>
            </Actions>
        </OutlineItem>
    </Outline>

</PDFComposition>"""

    out_filename = "/vsimem/tmp.pdf"
    with gdal.quiet_errors():
        out_ds = gdaltest.pdf_drv.Create(
            out_filename,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=["COMPOSITION_FILE=" + xml_content],
        )
    assert not out_ds
    assert gdal.GetLastErrorMsg() == "Missing layerId"
    gdal.Unlink(out_filename)


def test_pdf_composition_outline_item_setlayerstate_pointing_to_invalid_layer_id():

    xml_content = """<PDFComposition>
    <Page id="1">
        <Width>20</Width>
        <Height>20</Height>
        <Content streamCompression="NONE"/>
    </Page>

    <Outline>
        <OutlineItem name="name">
            <Actions>
                <SetLayerStateAction layerId="non_existing" visible="true"/>
            </Actions>
        </OutlineItem>
    </Outline>

</PDFComposition>"""

    out_filename = "/vsimem/tmp.pdf"
    with gdal.quiet_errors():
        out_ds = gdaltest.pdf_drv.Create(
            out_filename,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=["COMPOSITION_FILE=" + xml_content],
        )
    assert not out_ds
    assert gdal.GetLastErrorMsg() == "Referencing layer of unknown id: non_existing"
    gdal.Unlink(out_filename)


###############################################################################
# Test reading ISO:32000 CRS in ESRI namespace written as EPSG code
# (https://github.com/OSGeo/gdal/issues/6522)


@pytest.mark.skipif(not have_read_support(), reason="no read support available")
def test_pdf_iso32000_esri_as_epsg():

    gdal.ErrorReset()
    ds = gdal.Open("data/pdf/esri_102422_as_epsg_code.pdf")
    sr = ds.GetSpatialRef()
    assert sr.GetAuthorityName(None) == "ESRI"
    assert sr.GetAuthorityCode(None) == "102422"
    assert gdal.GetLastErrorMsg() == ""


###############################################################################
# Test bugfix for https://issues.oss-fuzz.com/issues/376126833


@pytest.mark.skipif(not have_read_support(), reason="no read support available")
def test_pdf_iso32000_invalid_srs():

    # Just test that this does not crash
    gdal.Open("data/pdf/invalid_srs.pdf")


###############################################################################
#


@gdaltest.enable_exceptions()
@pytest.mark.skipif(not have_read_support(), reason="no read support available")
def test_pdf_gdal_driver_pdf_list_layer():

    alg = gdal.GetGlobalAlgorithmRegistry()["driver"]
    assert alg.GetName() == "driver"

    alg = gdal.GetGlobalAlgorithmRegistry()["driver"]["pdf"]
    assert alg.GetName() == "pdf"

    alg = gdal.GetGlobalAlgorithmRegistry()["driver"]["pdf"]["list-layers"]
    assert alg.GetName() == "list-layers"
    alg["input"] = "data/pdf/adobe_style_geospatial.pdf"
    assert alg.Run()
    assert json.loads(alg["output-string"]) == [
        "New_Data_Frame",
        "New_Data_Frame.Graticule",
        "Layers",
        "Layers.Measured_Grid",
        "Layers.Graticule",
    ]

    alg = gdal.GetGlobalAlgorithmRegistry()["driver"]["pdf"]["list-layers"]
    alg["input"] = "data/pdf/adobe_style_geospatial.pdf"
    alg["output-format"] = "text"
    assert alg.Run()
    assert (
        alg["output-string"]
        == "New_Data_Frame\nNew_Data_Frame.Graticule\nLayers\nLayers.Measured_Grid\nLayers.Graticule\n"
    )

    alg = gdal.GetGlobalAlgorithmRegistry()["driver"]["pdf"]["list-layers"]
    assert alg.GetName() == "list-layers"
    alg["input"] = "data/byte.tif"
    with pytest.raises(Exception, match="is not a PDF"):
        alg.Run()


###############################################################################
#


@gdaltest.enable_exceptions()
@pytest.mark.require_driver("L1B")
def test_pdf_create_copy_from_ysize_0(tmp_vsimem):
    src_ds = gdal.Open("../gdrivers/data/l1b/n12gac8bit_truncated_ysize_0_1band.l1b")
    with pytest.raises(Exception, match="nWidth == 0 || nHeight == 0 not supported"):
        gdal.GetDriverByName("PDF").CreateCopy(tmp_vsimem / "out.pdf", src_ds)


###############################################################################
#


@gdaltest.enable_exceptions()
@pytest.mark.skipif(not have_read_support(), reason="no read support available")
def test_pdf_dpi_save_to_pam(tmp_vsimem):

    with gdal.VSIFile(tmp_vsimem / "test.pdf", "wb") as f:
        f.write(open("data/pdf/test_ogc_bp.pdf", "rb").read())

    with gdal.OpenEx(tmp_vsimem / "test.pdf", open_options=["DPI=144"]) as ds:
        assert ds.RasterXSize == 40
        assert ds.RasterYSize == 40
        assert ds.GetRasterBand(1).XSize == 40
        assert ds.GetRasterBand(1).YSize == 40
        assert ds.GetMetadataItem("DPI") == "144"
    assert gdal.VSIStatL(tmp_vsimem / "test.pdf.aux.xml") is None

    with gdal.Open(tmp_vsimem / "test.pdf") as ds:
        assert ds.RasterXSize == 20
        assert ds.RasterYSize == 20
        assert ds.GetRasterBand(1).XSize == 20
        assert ds.GetRasterBand(1).YSize == 20
        assert ds.GetMetadataItem("DPI") == "72"
    assert gdal.VSIStatL(tmp_vsimem / "test.pdf.aux.xml") is None

    with gdal.OpenEx(
        tmp_vsimem / "test.pdf", open_options=["DPI=144", "SAVE_DPI_TO_PAM=YES"]
    ) as ds:
        assert ds.RasterXSize == 40
        assert ds.RasterYSize == 40
        assert ds.GetRasterBand(1).XSize == 40
        assert ds.GetRasterBand(1).YSize == 40
        assert ds.GetMetadataItem("DPI") == "144"
    assert gdal.VSIStatL(tmp_vsimem / "test.pdf.aux.xml") is not None
    with gdal.VSIFile(tmp_vsimem / "test.pdf.aux.xml", "rb") as f:
        pam_content = f.read()
    assert (
        pam_content
        == b"""<PAMDataset>\n  <Metadata>\n    <MDI key="DPI">144</MDI>\n  </Metadata>\n</PAMDataset>\n"""
    )

    with gdal.Open(tmp_vsimem / "test.pdf") as ds:
        assert ds.RasterXSize == 40
        assert ds.RasterYSize == 40
        assert ds.GetRasterBand(1).XSize == 40
        assert ds.GetRasterBand(1).YSize == 40
        assert ds.GetMetadataItem("DPI") == "144"

    with gdal.OpenEx(tmp_vsimem / "test.pdf", open_options=["DPI=72"]) as ds:
        assert ds.RasterXSize == 20
        assert ds.RasterYSize == 20
        assert ds.GetRasterBand(1).XSize == 20
        assert ds.GetRasterBand(1).YSize == 20
        assert ds.GetMetadataItem("DPI") == "72"

    with gdal.VSIFile(tmp_vsimem / "test.pdf.aux.xml", "rb") as f:
        pam_content = f.read()
    assert (
        pam_content
        == b"""<PAMDataset>\n  <Metadata>\n    <MDI key="DPI">144</MDI>\n  </Metadata>\n</PAMDataset>\n"""
    )
