#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for ECW driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import array
import os
import os.path
import shutil
import struct
import sys

import gdaltest
import pytest

from osgeo import gdal, osr

pytestmark = pytest.mark.require_driver("ECW")

###############################################################################


def has_write_support():
    if hasattr(gdaltest, "b_ecw_has_write_support"):
        return gdaltest.b_ecw_has_write_support
    gdaltest.b_ecw_has_write_support = False

    ecw_drv = gdal.GetDriverByName("ECW")
    if ecw_drv is None or ecw_drv.GetMetadataItem("DMD_CREATIONDATATYPES") is None:
        return False

    if (
        "ECW_ENCODE_KEY" in ecw_drv.GetMetadataItem("DMD_CREATIONOPTIONLIST")
        and gdal.GetConfigOption("ECW_ENCODE_KEY") is None
    ):
        print("ECW_ENCODE_KEY not defined. Write support not available")
        return False

    ds = gdal.Open("data/ecw/jrc.ecw")
    if ds:
        out_ds = ecw_drv.CreateCopy("tmp/jrc_out.ecw", ds, options=["TARGET=75"])
        if out_ds:
            out_ds = None
            gdaltest.b_ecw_has_write_support = True

            try:
                os.remove("tmp/jrc_out.ecw")
            except OSError:
                pass
            try:
                os.remove("tmp/jrc_out.ecw.aux.xml")
            except OSError:
                pass
        else:
            if "ECW_ENCODE_KEY" not in gdal.GetLastErrorMsg():
                pytest.fail("ECW creation failed for unknown reason")

    return gdaltest.b_ecw_has_write_support


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def startup_and_cleanup():

    gdaltest.ecw_drv = gdal.GetDriverByName("ECW")
    assert gdaltest.ecw_drv is not None
    gdaltest.jp2ecw_drv = gdal.GetDriverByName("JP2ECW")

    gdaltest.deregister_all_jpeg2000_drivers_but("JP2ECW")

    longname = gdaltest.ecw_drv.GetMetadataItem("DMD_LONGNAME")

    sdk_off = longname.find("SDK ")
    if sdk_off != -1:
        gdaltest.ecw_drv.major_version = int(float(longname[sdk_off + 4]))
        sdk_minor_off = longname.find(".", sdk_off)
        if sdk_minor_off >= 0:
            if longname[sdk_minor_off + 1] == "x":
                gdaltest.ecw_drv.minor_version = 3
            else:
                gdaltest.ecw_drv.minor_version = int(longname[sdk_minor_off + 1])
        else:
            gdaltest.ecw_drv.minor_version = 0
    else:
        gdaltest.ecw_drv.major_version = 3
        gdaltest.ecw_drv.minor_version = 3
    gdaltest.ecw_drv.version = (
        gdaltest.ecw_drv.major_version,
        gdaltest.ecw_drv.minor_version,
    )

    # we set ECW to not resolve projection and datum strings to get 3.x behavior.
    with gdal.config_option("ECW_DO_NOT_RESOLVE_DATUM_PROJECTION", "YES"):
        yield

    gdaltest.reregister_all_jpeg2000_drivers()


###############################################################################
# Verify various information about our test image.


def test_ecw_2():

    ds = gdal.Open("data/ecw/jrc.ecw")

    if gdaltest.ecw_drv.major_version == 3:
        exp_mean, exp_stddev = (141.172, 67.3636)
    else:
        if gdaltest.ecw_drv.major_version == 5:
            exp_mean, exp_stddev = (141.606, 67.2919)
        else:
            exp_mean, exp_stddev = (140.332, 67.611)

    mean, stddev = ds.GetRasterBand(1).ComputeBandStats()

    assert mean == pytest.approx(exp_mean, abs=0.5) and stddev == pytest.approx(
        exp_stddev, abs=0.5
    ), "mean/stddev of (%g,%g) diffs from expected(%g,%g)" % (
        mean,
        stddev,
        exp_mean,
        exp_stddev,
    )

    geotransform = ds.GetGeoTransform()
    assert (
        geotransform[0] == pytest.approx(467498.5, abs=0.1)
        and geotransform[1] == pytest.approx(16.5475, abs=0.001)
        and geotransform[2] == pytest.approx(0, abs=0.001)
        and geotransform[3] == pytest.approx(5077883.2825, abs=0.1)
        and geotransform[4] == pytest.approx(0, abs=0.001)
        and geotransform[5] == pytest.approx(-16.5475, abs=0.001)
    ), "geotransform differs from expected"


###############################################################################
# Verify various information about our generated image.


def test_ecw_4(tmp_path):

    if not has_write_support():
        pytest.skip("ECW write support not available")

    src_ds = gdal.Open("data/ecw/jrc.ecw")
    gdaltest.ecw_drv.CreateCopy(tmp_path / "jrc_out.ecw", src_ds, options=["TARGET=75"])
    if os.path.exists(tmp_path / "jrc_out.ecw.aux.xml"):
        gdal.Unlink(tmp_path / "jrc_out.ecw.aux.xml")

    ds = gdal.Open(tmp_path / "jrc_out.ecw")
    version = ds.GetMetadataItem("VERSION")
    assert version == "2", "bad VERSION"

    if gdaltest.ecw_drv.major_version == 3:
        exp_mean, exp_stddev = (140.290, 66.6303)
    else:
        if gdaltest.ecw_drv.major_version == 5:
            exp_mean, exp_stddev = (141.517, 67.1285)
        else:
            exp_mean, exp_stddev = (138.971, 67.716)

    mean, stddev = ds.GetRasterBand(1).ComputeBandStats()

    assert mean == pytest.approx(exp_mean, abs=1.5) and stddev == pytest.approx(
        exp_stddev, abs=0.5
    ), "mean/stddev of (%g,%g) diffs from expected(%g,%g)" % (
        mean,
        stddev,
        exp_mean,
        exp_stddev,
    )

    geotransform = ds.GetGeoTransform()
    assert (
        geotransform[0] == pytest.approx(467498.5, abs=0.1)
        and geotransform[1] == pytest.approx(16.5475, abs=0.001)
        and geotransform[2] == pytest.approx(0, abs=0.001)
        and geotransform[3] == pytest.approx(5077883.2825, abs=0.1)
        and geotransform[4] == pytest.approx(0, abs=0.001)
        and geotransform[5] == pytest.approx(-16.5475, abs=0.001)
    ), "geotransform differs from expected"

    ds = None


###############################################################################
# Now try writing a JPEG2000 compressed version of the same with the ECW driver


def test_ecw_5(tmp_path):
    if gdaltest.jp2ecw_drv is None or not has_write_support():
        pytest.skip("ECW write support not available")

    ds = gdal.Open("data/small.vrt")
    ds_out = gdaltest.jp2ecw_drv.CreateCopy(
        tmp_path / "ecw_5.jp2", ds, options=["TARGET=75"]
    )
    assert ds_out.GetDriver().ShortName == "JP2ECW"
    version = ds_out.GetMetadataItem("VERSION")
    assert version == "1", "bad VERSION"
    ds = None
    ds_out = None

    ###############################################################################
    # Verify various information about our generated image.

    ds = gdal.Open(tmp_path / "ecw_5.jp2")

    if gdaltest.ecw_drv.major_version == 3:
        exp_mean, exp_stddev = (144.422, 44.9075)
    else:
        exp_mean, exp_stddev = (143.375, 44.8539)
    mean, stddev = ds.GetRasterBand(1).ComputeBandStats()

    # The difference in the stddev is outrageously large between win32 and
    # Linux, but I don't know why.
    assert mean == pytest.approx(exp_mean, abs=1.5) and stddev == pytest.approx(
        exp_stddev, abs=6
    ), "mean/stddev of (%g,%g) diffs from " "expected(%g,%g)" % (
        mean,
        stddev,
        exp_mean,
        exp_stddev,
    )

    mean, stddev = ds.GetRasterBand(2).ComputeBandStats()

    # The difference in the stddev is outrageously large between win32 and
    # Linux, but I don't know why.
    assert mean == pytest.approx(exp_mean, abs=1.0) and stddev == pytest.approx(
        exp_stddev, abs=6
    ), "mean/stddev of (%g,%g) diffs from " "expected(%g,%g)" % (
        mean,
        stddev,
        exp_mean,
        exp_stddev,
    )

    geotransform = ds.GetGeoTransform()
    assert (
        geotransform[0] == pytest.approx(440720, abs=0.1)
        and geotransform[1] == pytest.approx(60, abs=0.001)
        and geotransform[2] == pytest.approx(0, abs=0.001)
        and geotransform[3] == pytest.approx(3751320, abs=0.1)
        and geotransform[4] == pytest.approx(0, abs=0.001)
        and geotransform[5] == pytest.approx(-60, abs=0.001)
    ), "geotransform differs from expected"

    prj = ds.GetProjectionRef()
    assert not (
        prj.find("UTM") == -1 or prj.find("NAD27") == -1 or prj.find("one 11") == -1
    ), "Coordinate system not UTM 11, NAD27?"

    ds = None


###############################################################################
# Write the same image to NITF.


def test_ecw_7(tmp_path):
    if gdaltest.jp2ecw_drv is None or not has_write_support():
        pytest.skip("ECW write support not available")

    ds = gdal.Open("data/small.vrt")
    drv = gdal.GetDriverByName("NITF")
    drv.CreateCopy(tmp_path / "ecw_7.ntf", ds, options=["IC=C8", "TARGET=75"], strict=0)
    ds = None

    ###############################################################################
    # Verify various information about our generated image.

    ds = gdal.Open(tmp_path / "ecw_7.ntf")

    exp_mean, exp_stddev = (145.57, 43.1712)
    mean, stddev = ds.GetRasterBand(1).ComputeBandStats()

    assert mean == pytest.approx(exp_mean, abs=1.0) and stddev == pytest.approx(
        exp_stddev, abs=1.0
    ), "mean/stddev of (%g,%g) diffs from expected(%g,%g)" % (
        mean,
        stddev,
        exp_mean,
        exp_stddev,
    )

    geotransform = ds.GetGeoTransform()
    assert (
        geotransform[0] == pytest.approx(440720, abs=0.1)
        and geotransform[1] == pytest.approx(60, abs=0.001)
        and geotransform[2] == pytest.approx(0, abs=0.001)
        and geotransform[3] == pytest.approx(3751320, abs=0.1)
        and geotransform[4] == pytest.approx(0, abs=0.001)
        and geotransform[5] == pytest.approx(-60, abs=0.001)
    ), "geotransform differs from expected"

    prj = ds.GetProjectionRef()
    assert (
        prj.find(
            'PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0]'
        )
        == -1
        or prj.find("WGS 84") > 0
    ), "Coordinate system not UTM 11, WGS 84?"

    ds = None


###############################################################################
# Try writing 16bit JP2 file directly using Create().


def test_ecw_9(tmp_path):
    if gdaltest.jp2ecw_drv is None or not has_write_support():
        pytest.skip("ECW write support not available")

    # This always crashes on Frank's machine - some bug in old sdk.
    if os.getenv("USER") == "warmerda" and gdaltest.ecw_drv.major_version == 3:
        pytest.skip()

    ds = gdaltest.jp2ecw_drv.Create(
        tmp_path / "ecw9.jp2", 200, 100, 1, gdal.GDT_Int16, options=["TARGET=75"]
    )
    ds.SetGeoTransform((100, 0.1, 0.0, 30.0, 0.0, -0.1))

    ds.SetProjection(
        'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9108"]],AXIS["Lat",NORTH],AXIS["Long",EAST],AUTHORITY["EPSG","4326"]]'
    )

    raw_data = array.array("h", list(range(200))).tobytes()

    for line in range(100):
        ds.WriteRaster(0, line, 200, 1, raw_data, buf_type=gdal.GDT_Int16)
    ds = None

    ###############################################################################
    # Verify previous 16bit file.

    ds = gdal.Open(tmp_path / "ecw9.jp2")

    exp_mean, exp_stddev = (98.49, 57.7129)
    mean, stddev = ds.GetRasterBand(1).ComputeBandStats()

    assert mean == pytest.approx(exp_mean, abs=1.1) and stddev == pytest.approx(
        exp_stddev, abs=0.1
    ), "mean/stddev of (%g,%g) diffs from expected(%g,%g)" % (
        mean,
        stddev,
        exp_mean,
        exp_stddev,
    )

    geotransform = ds.GetGeoTransform()
    assert (
        geotransform[0] == pytest.approx(100, abs=0.1)
        and geotransform[1] == pytest.approx(0.1, abs=0.001)
        and geotransform[2] == pytest.approx(0, abs=0.001)
        and geotransform[3] == pytest.approx(30, abs=0.1)
        and geotransform[4] == pytest.approx(0, abs=0.001)
        and geotransform[5] == pytest.approx(-0.1, abs=0.001)
    ), "geotransform differs from expected"


###############################################################################
# Test direct creation of an NITF/JPEG2000 file.


def test_ecw_11(tmp_path):
    if gdaltest.jp2ecw_drv is None or not has_write_support():
        pytest.skip("ECW write support not available")

    drv = gdal.GetDriverByName("NITF")
    ds = drv.Create(tmp_path / "test_11.ntf", 200, 100, 3, gdal.GDT_UInt8, ["ICORDS=G"])
    ds.SetGeoTransform((100, 0.1, 0.0, 30.0, 0.0, -0.1))

    my_list = list(range(200)) + list(range(20, 220)) + list(range(30, 230))
    raw_data = array.array("h", my_list).tobytes()

    for line in range(100):
        ds.WriteRaster(
            0, line, 200, 1, raw_data, buf_type=gdal.GDT_Int16, band_list=[1, 2, 3]
        )

    ds.GetRasterBand(1).SetRasterColorInterpretation(gdal.GCI_BlueBand)
    ds.GetRasterBand(2).SetRasterColorInterpretation(gdal.GCI_GreenBand)
    ds.GetRasterBand(3).SetRasterColorInterpretation(gdal.GCI_RedBand)

    ds = None

    ###############################################################################
    # Verify previous file

    ds = gdal.Open(tmp_path / "test_11.ntf")

    geotransform = ds.GetGeoTransform()
    assert (
        geotransform[0] == pytest.approx(100, abs=0.1)
        and geotransform[1] == pytest.approx(0.1, abs=0.001)
        and geotransform[2] == pytest.approx(0, abs=0.001)
        and geotransform[3] == pytest.approx(30.0, abs=0.1)
        and geotransform[4] == pytest.approx(0, abs=0.001)
        and geotransform[5] == pytest.approx(-0.1, abs=0.001)
    ), "geotransform differs from expected"

    assert (
        ds.GetRasterBand(1).GetRasterColorInterpretation() == gdal.GCI_BlueBand
    ), "Got wrong color interpretation."

    assert (
        ds.GetRasterBand(2).GetRasterColorInterpretation() == gdal.GCI_GreenBand
    ), "Got wrong color interpretation."

    assert (
        ds.GetRasterBand(3).GetRasterColorInterpretation() == gdal.GCI_RedBand
    ), "Got wrong color interpretation."

    ds = None


###############################################################################
# This is intended to verify that the ECWDataset::RasterIO() special case
# works properly.  It is used to copy subwindow into a memory dataset
# which we then checksum.  To stress the RasterIO(), we also change data
# type and select an altered band list.


def test_ecw_13():
    if gdaltest.jp2ecw_drv is None:
        pytest.skip()

    ds = gdal.Open("data/jpeg2000/rgb16_ecwsdk.jp2")

    wrktype = gdal.GDT_Float32
    raw_data = ds.ReadRaster(10, 10, 40, 40, buf_type=wrktype, band_list=[3, 2, 1])
    ds = None

    drv = gdal.GetDriverByName("MEM")
    ds = drv.Create("workdata", 40, 40, 3, wrktype)
    ds.WriteRaster(0, 0, 40, 40, raw_data, buf_type=wrktype)

    checksums = (
        ds.GetRasterBand(1).Checksum(),
        ds.GetRasterBand(2).Checksum(),
        ds.GetRasterBand(3).Checksum(),
    )
    ds = None

    assert checksums == (
        19253,
        17848,
        19127,
    ), "Expected checksums do match expected checksums"


###############################################################################
# Write out image with GCPs.


def test_ecw_14(tmp_path):
    if gdaltest.jp2ecw_drv is None or not has_write_support():
        pytest.skip("ECW write support not available")

    ds = gdal.Open("data/rgb_gcp.vrt")
    gdaltest.jp2ecw_drv.CreateCopy(tmp_path / "rgb_gcp.jp2", ds)
    ds = None

    ###############################################################################
    # Verify various information about our generated image.

    ds = gdal.Open(tmp_path / "rgb_gcp.jp2")

    gcp_srs = ds.GetGCPProjection()
    assert not (
        gcp_srs[:6] != "GEOGCS" or gcp_srs.find("WGS") == -1 or gcp_srs.find("84") == -1
    ), "GCP Projection not retained."

    gcps = ds.GetGCPs()
    assert (
        len(gcps) == 4
        and gcps[1].GCPPixel == 0
        and gcps[1].GCPLine == 50
        and gcps[1].GCPX == 0
        and gcps[1].GCPY == 50
        and gcps[1].GCPZ == 0
    ), "GCPs wrong."

    ds = None


###############################################################################
# Open byte.jp2


def test_ecw_16():

    if gdaltest.jp2ecw_drv is None:
        pytest.skip()

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

    tst = gdaltest.GDALTest("JP2ECW", "jpeg2000/byte.jp2", 1, 50054)
    tst.testOpen(check_prj=srs, check_gt=gt)


###############################################################################
# Open int16.jp2


def test_ecw_17():

    if gdaltest.jp2ecw_drv is None:
        pytest.skip()

    if gdaltest.ecw_drv.major_version == 4:
        pytest.skip("4.x SDK gets unreliable results for jp2")

    ds = gdal.Open("data/jpeg2000/int16.jp2")
    ds_ref = gdal.Open("data/int16.tif")

    maxdiff = gdaltest.compare_ds(ds, ds_ref)

    ds = None
    ds_ref = None

    # Quite a bit of difference...
    assert maxdiff <= 6, "Image too different from reference"


###############################################################################
# Open byte.jp2.gz (test use of the VSIL API)


def test_ecw_18(tmp_path):

    if gdaltest.jp2ecw_drv is None:
        pytest.skip()

    shutil.copy("data/jpeg2000/byte.jp2.gz", tmp_path)

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

    tst = gdaltest.GDALTest(
        "JP2ECW", f"/vsigzip/{tmp_path}/byte.jp2.gz", 1, 50054, filename_absolute=True
    )
    tst.testOpen(check_prj=srs, check_gt=gt)


###############################################################################
# Test a JPEG2000 with the 3 bands having 13bit depth and the 4th one 1 bit


def test_ecw_19():

    if gdaltest.jp2ecw_drv is None:
        pytest.skip()

    ds = gdal.Open("data/jpeg2000/3_13bit_and_1bit.jp2")

    # 31324 is got with ECW SDK 5.5 on Windows that seems to promote the 1 bit
    # channel to 8 bit
    expected_checksums = [(64570,), (57277,), (56048,), (61292, 31324)]

    for i in range(4):
        assert ds.GetRasterBand(i + 1).Checksum() in expected_checksums[i]

    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16, "unexpected data type"


###############################################################################
# Confirm that we have an overview for this image and that the statistics
# are as expected.


def test_ecw_20():

    ds = gdal.Open("data/ecw/jrc.ecw")

    band = ds.GetRasterBand(1)
    assert band.GetOverviewCount() == 1, "did not get expected number of overview"

    # Both requests should go *exactly* to the same code path
    data_subsampled = band.ReadRaster(0, 0, 400, 400, 200, 200)
    data_overview = band.GetOverview(0).ReadRaster(0, 0, 200, 200)
    assert data_subsampled == data_overview, "inconsistent overview behaviour"

    if gdaltest.ecw_drv.major_version == 3:
        exp_mean, exp_stddev = (141.644, 67.2186)
    else:
        if gdaltest.ecw_drv.major_version == 5:
            exp_mean, exp_stddev = (142.189, 62.4223)
        else:
            exp_mean, exp_stddev = (140.889, 62.742)
    mean, stddev = band.GetOverview(0).ComputeBandStats()

    assert mean == pytest.approx(exp_mean, abs=0.5) and stddev == pytest.approx(
        exp_stddev, abs=0.5
    ), "mean/stddev of (%g,%g) diffs from " "expected(%g,%g)" % (
        mean,
        stddev,
        exp_mean,
        exp_stddev,
    )


###############################################################################
# This test is intended to go through an optimized data path (likely
# one big interleaved read) in the CreateCopy() instead of the line by
# line access typical of ComputeBandStats.  Make sure we get the same as
# line by line.


def test_ecw_21():

    ds = gdal.Open("data/ecw/jrc.ecw")
    mem_ds = gdal.GetDriverByName("MEM").CreateCopy(
        "xxxyyy", ds, options=["INTERLEAVE=PIXEL"]
    )
    ds = None

    if gdaltest.ecw_drv.major_version == 3:
        exp_mean, exp_stddev = (141.172, 67.3636)
    else:
        if gdaltest.ecw_drv.major_version == 5:
            exp_mean, exp_stddev = (141.606, 67.2919)
        else:
            exp_mean, exp_stddev = (140.332, 67.611)

    mean, stddev = mem_ds.GetRasterBand(1).ComputeBandStats()

    assert mean == pytest.approx(exp_mean, abs=0.5) and stddev == pytest.approx(
        exp_stddev, abs=0.5
    ), "mean/stddev of (%g,%g) diffs from expected(%g,%g)" % (
        mean,
        stddev,
        exp_mean,
        exp_stddev,
    )


###############################################################################
# This tests reading of georeferencing and coordinate system from within an
# ECW file.


def test_ecw_22():

    ds = gdal.Open("data/ecw/spif83.ecw")

    expected_wkt = """PROJCS["L2CAL6M",GEOGCS["NAD83",DATUM["North_American_Datum_1983",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6269"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4269"]],PROJECTION["Lambert_Conformal_Conic_2SP"],PARAMETER["standard_parallel_1",32.7833333078095],PARAMETER["standard_parallel_2",33.8833333208765],PARAMETER["latitude_of_origin",32.166666682432],PARAMETER["central_meridian",-116.249999974595],PARAMETER["false_easting",2000000],PARAMETER["false_northing",500000],UNIT["Metre",1],AXIS["Easting",EAST],AXIS["Northing",NORTH]]"""
    expected_srs = osr.SpatialReference()
    expected_srs.ImportFromWkt(expected_wkt)
    assert ds.GetSpatialRef().IsSame(expected_srs), "did not get expected SRS."


###############################################################################
# This tests overriding the coordinate system from an .aux.xml file, while
# preserving the ecw derived georeferencing.


def test_ecw_23(tmp_path):

    shutil.copyfile("data/ecw/spif83.ecw", tmp_path / "spif83.ecw")
    shutil.copyfile(
        "data/ecw/spif83_hidden.ecw.aux.xml", tmp_path / "spif83.ecw.aux.xml"
    )

    ds = gdal.Open(tmp_path / "spif83.ecw")

    wkt = ds.GetProjectionRef()

    assert "36 / British National Grid" in wkt
    assert "TOWGS84[446.448,-125.157,542.06,0.15,0.247,0.842,-20.489]" in wkt
    assert 'AUTHORITY["EPSG","27700"]' in wkt

    gt = ds.GetGeoTransform()
    expected_gt = (
        6138559.5576418638,
        195.5116973254697,
        0.0,
        2274798.7836679211,
        0.0,
        -198.32414964918371,
    )
    assert gt == expected_gt, "did not get expected geotransform."


###############################################################################
# Test that we can alter geotransform on existing ECW


def test_ecw_24(tmp_path):

    shutil.copyfile("data/ecw/spif83.ecw", tmp_path / "spif83.ecw")

    ds = gdal.Open(tmp_path / "spif83.ecw", gdal.GA_Update)
    if (
        ds is None
        and gdaltest.ecw_drv.major_version == 3
        and gdal.GetConfigOption("APPVEYOR") is not None
    ):
        pytest.skip()
    gt = [1, 2, 0, 3, 0, -4]
    ds.SetGeoTransform(gt)
    ds = None

    assert not os.path.exists(tmp_path / "spif83.ecw.aux.xml")

    ds = gdal.Open(tmp_path / "spif83.ecw")
    got_gt = ds.GetGeoTransform()
    ds = None

    for i in range(6):
        assert gt[i] == pytest.approx(got_gt[i], abs=1e-5)


###############################################################################
# Test that we can alter projection info on existing ECW (through SetProjection())


def test_ecw_25(tmp_path):

    shutil.copyfile("data/ecw/spif83.ecw", tmp_path / "spif83.ecw")

    proj = "NUTM31"
    datum = "WGS84"
    units = "FEET"

    ds = gdal.Open(tmp_path / "spif83.ecw", gdal.GA_Update)
    if (
        ds is None
        and gdaltest.ecw_drv.major_version == 3
        and gdal.GetConfigOption("APPVEYOR") is not None
    ):
        pytest.skip()
    sr = osr.SpatialReference()
    sr.ImportFromERM(proj, datum, units)
    wkt = sr.ExportToWkt()
    ds.SetProjection(wkt)
    ds = None

    assert not os.path.exists(tmp_path / "spif83.ecw.aux.xml")

    ds = gdal.Open(tmp_path / "spif83.ecw")
    got_proj = ds.GetMetadataItem("PROJ", "ECW")
    got_datum = ds.GetMetadataItem("DATUM", "ECW")
    got_units = ds.GetMetadataItem("UNITS", "ECW")
    got_wkt = ds.GetProjectionRef()
    ds = None

    assert got_proj == proj
    assert got_datum == datum
    assert got_units == units

    assert wkt == got_wkt


###############################################################################
# Test that we can alter projection info on existing ECW (through SetMetadataItem())


def test_ecw_26(tmp_path):

    shutil.copyfile("data/ecw/spif83.ecw", tmp_path / "spif83.ecw")

    proj = "NUTM31"
    datum = "WGS84"
    units = "FEET"

    ds = gdal.Open(tmp_path / "spif83.ecw", gdal.GA_Update)
    if (
        ds is None
        and gdaltest.ecw_drv.major_version == 3
        and gdal.GetConfigOption("APPVEYOR") is not None
    ):
        pytest.skip()
    ds.SetMetadataItem("PROJ", proj, "ECW")
    ds.SetMetadataItem("DATUM", datum, "ECW")
    ds.SetMetadataItem("UNITS", units, "ECW")
    ds = None

    assert not os.path.exists(tmp_path / "spif83.ecw.aux.xml")

    ds = gdal.Open(tmp_path / "spif83.ecw")
    got_proj = ds.GetMetadataItem("PROJ", "ECW")
    got_datum = ds.GetMetadataItem("DATUM", "ECW")
    got_units = ds.GetMetadataItem("UNITS", "ECW")
    got_wkt = ds.GetProjectionRef()
    ds = None

    assert got_proj == proj
    assert got_datum == datum
    assert got_units == units

    sr = osr.SpatialReference()
    sr.ImportFromERM(proj, datum, units)
    wkt = sr.ExportToWkt()

    assert wkt == got_wkt


###############################################################################
# Check that we can use .j2w world files (#4651)


def test_ecw_27():

    if gdaltest.jp2ecw_drv is None or not has_write_support():
        pytest.skip("ECW write support not available")

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
# Check picking use case


def test_ecw_28():

    x = y = 50

    ds = gdal.Open("data/ecw/jrc.ecw")
    multiband_data = ds.ReadRaster(x, y, 1, 1)
    ds = None

    ds = gdal.Open("data/ecw/jrc.ecw")
    data1 = ds.GetRasterBand(1).ReadRaster(x, y, 1, 1)
    data2 = ds.GetRasterBand(2).ReadRaster(x, y, 1, 1)
    data3 = ds.GetRasterBand(3).ReadRaster(x, y, 1, 1)
    ds = None

    struct.unpack("B" * 3, multiband_data)
    struct.unpack("B" * 3, data1 + data2 + data3)


###############################################################################
# Test supersampling


def test_ecw_29():

    ds = gdal.Open("data/ecw/jrc.ecw")
    data_b1 = ds.GetRasterBand(1).ReadRaster(0, 0, 400, 400)
    ds = None

    ds = gdal.Open("data/ecw/jrc.ecw")
    data_ecw_supersampled_b1 = ds.GetRasterBand(1).ReadRaster(0, 0, 400, 400, 800, 800)
    ds = None

    ds = gdal.GetDriverByName("GTiff").Create("/vsimem/ecw_29_0.tif", 400, 400, 1)
    ds.WriteRaster(0, 0, 400, 400, data_b1)
    data_tiff_supersampled_b1 = ds.GetRasterBand(1).ReadRaster(0, 0, 400, 400, 800, 800)
    ds = None

    ds1 = gdal.GetDriverByName("GTiff").Create("/vsimem/ecw_29_1.tif", 800, 800, 1)
    ds1.WriteRaster(0, 0, 800, 800, data_ecw_supersampled_b1)

    ds2 = gdal.GetDriverByName("GTiff").Create("/vsimem/ecw_29_2.tif", 800, 800, 1)
    ds2.WriteRaster(0, 0, 800, 800, data_tiff_supersampled_b1)

    if gdaltest.ecw_drv.major_version < 5:
        maxdiff = gdaltest.compare_ds(ds1, ds2)
        assert maxdiff == 0
    else:
        # Compare the images by comparing their statistics on subwindows
        nvals = 0
        sum_abs_diff_mean = 0
        sum_abs_diff_stddev = 0
        tile = 32
        for j in range(2 * int((ds1.RasterYSize - tile / 2) / tile)):
            for i in range(2 * int((ds1.RasterXSize - tile / 2) / tile)):
                tmp_ds1 = gdal.GetDriverByName("MEM").Create("", tile, tile, 1)
                tmp_ds2 = gdal.GetDriverByName("MEM").Create("", tile, tile, 1)
                data1 = ds1.ReadRaster(i * int(tile / 2), j * int(tile / 2), tile, tile)
                data2 = ds2.ReadRaster(i * int(tile / 2), j * int(tile / 2), tile, tile)
                tmp_ds1.WriteRaster(0, 0, tile, tile, data1)
                tmp_ds2.WriteRaster(0, 0, tile, tile, data2)
                _, _, mean1, stddev1 = tmp_ds1.GetRasterBand(1).GetStatistics(1, 1)
                _, _, mean2, stddev2 = tmp_ds2.GetRasterBand(1).GetStatistics(1, 1)
                nvals = nvals + 1
                sum_abs_diff_mean = sum_abs_diff_mean + abs(mean1 - mean2)
                sum_abs_diff_stddev = sum_abs_diff_stddev + abs(stddev1 - stddev2)
                assert mean1 == pytest.approx(mean2, abs=(stddev1 + stddev2) / 2), (
                    j,
                    i,
                    abs(mean1 - mean2),
                    abs(stddev1 - stddev2),
                )
                assert stddev1 == pytest.approx(stddev2, abs=30), (
                    j,
                    i,
                    abs(mean1 - mean2),
                    abs(stddev1 - stddev2),
                )

        assert sum_abs_diff_mean / nvals <= 4
        assert sum_abs_diff_stddev / nvals <= 3

    ds1 = None
    ds2 = None

    gdal.Unlink("/vsimem/ecw_29_0.tif")
    gdal.Unlink("/vsimem/ecw_29_1.tif")
    gdal.Unlink("/vsimem/ecw_29_2.tif")


###############################################################################
# Test IReadBlock()


def test_ecw_30():

    ds = gdal.Open("data/ecw/jrc.ecw")
    blockxsize, blockysize = ds.GetRasterBand(1).GetBlockSize()
    data_readraster = ds.GetRasterBand(1).ReadRaster(0, 0, blockxsize, blockysize)
    data_readblock = ds.GetRasterBand(1).ReadBlock(0, 0)
    ds = None

    assert data_readraster == data_readblock


###############################################################################
# Test async reader interface ( SDK >= 4.x )


def test_ecw_31():

    if gdaltest.ecw_drv.major_version < 4:
        pytest.skip("Requires ECW SDK >= 4.0")

    ds = gdal.Open("data/ecw/jrc.ecw")
    ref_buf = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    ds = None

    ds = gdal.Open("data/ecw/jrc.ecw")

    asyncreader = ds.BeginAsyncReader(0, 0, ds.RasterXSize, ds.RasterYSize)
    while True:
        result = asyncreader.GetNextUpdatedRegion(0.05)
        if result[0] == gdal.GARIO_COMPLETE:
            break
        elif result[0] != gdal.GARIO_ERROR:
            continue
        else:
            ds.EndAsyncReader(asyncreader)
            pytest.fail("error occurred")

    if result != [gdal.GARIO_COMPLETE, 0, 0, ds.RasterXSize, ds.RasterYSize]:
        print(result)
        ds.EndAsyncReader(asyncreader)
        pytest.fail("wrong return values for GetNextUpdatedRegion()")

    async_buf = asyncreader.GetBuffer()

    ds.EndAsyncReader(asyncreader)
    asyncreader = None
    ds = None

    assert async_buf == ref_buf, "async_buf != ref_buf"


###############################################################################
# ECW SDK 3.3 has a bug with the ECW format when we query the
# number of bands of the dataset, but not in the "natural order".
# It ignores the content of panBandMap. (#4234)


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_ecw_32():

    ds = gdal.Open("data/ecw/jrc.ecw")
    data_123 = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize, band_list=[1, 2, 3])
    data_321 = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize, band_list=[3, 2, 1])
    assert data_123 != data_321

    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="400" rasterYSize="400">
    <VRTRasterBand dataType="Byte" band="1">
        <SimpleSource>
        <SourceFilename relativeToVRT="0">data/ecw/jrc.ecw</SourceFilename>
        <SourceBand>3</SourceBand>
        </SimpleSource>
    </VRTRasterBand>
    <VRTRasterBand dataType="Byte" band="2">
        <SimpleSource>
        <SourceFilename relativeToVRT="0">data/ecw/jrc.ecw</SourceFilename>
        <SourceBand>2</SourceBand>
        </SimpleSource>
    </VRTRasterBand>
    <VRTRasterBand dataType="Byte" band="3">
        <SimpleSource>
        <SourceFilename relativeToVRT="0">data/ecw/jrc.ecw</SourceFilename>
        <SourceBand>1</SourceBand>
        </SimpleSource>
    </VRTRasterBand>
    </VRTDataset>""")
    data_vrt = vrt_ds.ReadRaster(
        0, 0, ds.RasterXSize, ds.RasterYSize, band_list=[1, 2, 3]
    )

    assert data_321 == data_vrt


###############################################################################
# Test heuristics that detect successive band reading pattern


def test_ecw_33():

    ds = gdal.Open("data/ecw/jrc.ecw")
    multiband_data = ds.ReadRaster(100, 100, 50, 50)
    ds = None

    ds = gdal.Open("data/ecw/jrc.ecw")

    # To feed the heuristics
    ds.GetRasterBand(1).ReadRaster(10, 10, 50, 50)
    ds.GetRasterBand(2).ReadRaster(10, 10, 50, 50)
    ds.GetRasterBand(3).ReadRaster(10, 10, 50, 50)

    # Now the heuristics should be set to ON
    data1_1 = ds.GetRasterBand(1).ReadRaster(100, 100, 50, 50)
    data2_1 = ds.GetRasterBand(2).ReadRaster(100, 100, 50, 50)
    data3_1 = ds.GetRasterBand(3).ReadRaster(100, 100, 50, 50)

    # Break heuristics
    ds.GetRasterBand(2).ReadRaster(100, 100, 50, 50)
    ds.GetRasterBand(1).ReadRaster(100, 100, 50, 50)

    # To feed the heuristics again
    ds.GetRasterBand(1).ReadRaster(10, 10, 50, 50)
    ds.GetRasterBand(2).ReadRaster(10, 10, 50, 50)
    ds.GetRasterBand(3).ReadRaster(10, 10, 50, 50)

    # Now the heuristics should be set to ON
    data1_2 = ds.GetRasterBand(1).ReadRaster(100, 100, 50, 50)
    data2_2 = ds.GetRasterBand(2).ReadRaster(100, 100, 50, 50)
    data3_2 = ds.GetRasterBand(3).ReadRaster(100, 100, 50, 50)

    ds = None

    assert data1_1 == data1_2 and data2_1 == data2_2 and data3_1 == data3_2

    # When heuristics is ON, returned values should be the same as
    # 3-band at a time reading
    tab1 = struct.unpack("B" * 3 * 50 * 50, multiband_data)
    tab2 = struct.unpack("B" * 3 * 50 * 50, data1_1 + data2_1 + data3_2)
    assert tab1 == tab2

    ds = None


###############################################################################
# Check bugfix for #5262


def test_ecw_33_bis():

    ds = gdal.Open("data/ecw/jrc.ecw")
    data_ref = ds.ReadRaster(0, 0, 50, 50)

    ds = gdal.Open("data/ecw/jrc.ecw")

    # To feed the heuristics
    ds.GetRasterBand(1).ReadRaster(0, 0, 50, 50, buf_pixel_space=4)
    ds.GetRasterBand(2).ReadRaster(0, 0, 50, 50, buf_pixel_space=4)
    ds.GetRasterBand(3).ReadRaster(0, 0, 50, 50, buf_pixel_space=4)

    # Now the heuristics should be set to ON
    data1 = ds.GetRasterBand(1).ReadRaster(0, 0, 50, 50, buf_pixel_space=4)
    data2 = ds.GetRasterBand(2).ReadRaster(0, 0, 50, 50, buf_pixel_space=4)
    data3 = ds.GetRasterBand(3).ReadRaster(0, 0, 50, 50, buf_pixel_space=4)

    # Note: we must compare with the dataset RasterIO() buffer since
    # with SDK 3.3, the results of band RasterIO() and dataset RasterIO() are
    # not consistent. (which seems to be no longer the case with more recent
    # SDK such as 5.0)
    for i in range(50 * 50):
        assert data1[i * 4] == data_ref[i]
        assert data2[i * 4] == data_ref[50 * 50 + i]
        assert data3[i * 4] == data_ref[2 * 50 * 50 + i]

    ds = None


###############################################################################
# Verify that an write the imagery out to a new ecw file. Source file is 16 bit.


def test_ecw_34(tmp_path):

    if not has_write_support():
        pytest.skip("ECW write support not available")
    if gdaltest.ecw_drv.major_version < 5:
        pytest.skip("Requires ECW SDK >= 5.0")

    ds = gdal.GetDriverByName("MEM").Create("MEM:::", 128, 128, 1, gdal.GDT_UInt16)
    ds.GetRasterBand(1).Fill(65535)
    ref_data = ds.GetRasterBand(1).ReadRaster(0, 0, 128, 128, buf_type=gdal.GDT_UInt16)
    out_ds = gdaltest.ecw_drv.CreateCopy(
        tmp_path / "UInt16_big_out.ecw",
        ds,
        options=["ECW_FORMAT_VERSION=3", "TARGET=1"],
    )
    del out_ds
    ds = None

    ds = gdal.Open(tmp_path / "UInt16_big_out.ecw")
    got_data = ds.GetRasterBand(1).ReadRaster(0, 0, 128, 128, buf_type=gdal.GDT_UInt16)
    version = ds.GetMetadataItem("VERSION")
    ds = None

    assert got_data == ref_data
    assert version == "3", "bad VERSION"


###############################################################################
# Verify that an write the imagery out to a new JP2 file. Source file is 16 bit.


def test_ecw_35(tmp_path):
    if gdaltest.jp2ecw_drv is None or not has_write_support():
        pytest.skip("ECW write support not available")

    ds = gdal.GetDriverByName("MEM").Create("MEM:::", 128, 128, 1, gdal.GDT_UInt16)
    ds.GetRasterBand(1).Fill(65535)
    ref_data = ds.GetRasterBand(1).ReadRaster(0, 0, 128, 128, buf_type=gdal.GDT_UInt16)
    out_ds = gdaltest.jp2ecw_drv.CreateCopy(
        tmp_path / "UInt16_big_out.jp2", ds, options=["TARGET=1"]
    )
    del out_ds
    ds = None

    ds = gdal.Open(tmp_path / "UInt16_big_out.jp2")
    got_data = ds.GetRasterBand(1).ReadRaster(0, 0, 128, 128, buf_type=gdal.GDT_UInt16)
    ds = None

    assert got_data == ref_data


###############################################################################
# Make sure that band descriptions are preserved for version 3 ECW files.


def test_ecw_36(tmp_path):

    if not has_write_support():
        pytest.skip("ECW write support not available")
    if gdaltest.ecw_drv.major_version < 5:
        pytest.skip("Requires ECW SDK >= 5.0")

    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="400" rasterYSize="400">
    <VRTRasterBand dataType="Byte" band="1">
        <ColorInterp>Blue</ColorInterp>
        <SimpleSource>
        <SourceFilename relativeToVRT="0">data/ecw/jrc.ecw</SourceFilename>
        <SourceBand>3</SourceBand>
        </SimpleSource>
    </VRTRasterBand>
    <VRTRasterBand dataType="Byte" band="2">
        <ColorInterp>Red</ColorInterp>
        <SimpleSource>
        <SourceFilename relativeToVRT="0">data/ecw/jrc.ecw</SourceFilename>
        <SourceBand>1</SourceBand>
        </SimpleSource>
    </VRTRasterBand>
    <VRTRasterBand dataType="Byte" band="3">
        <ColorInterp>Green</ColorInterp>
        <SimpleSource>
        <SourceFilename relativeToVRT="0">data/ecw/jrc.ecw</SourceFilename>
        <SourceBand>2</SourceBand>
        </SimpleSource>
    </VRTRasterBand>
    </VRTDataset>""")

    dswr = gdaltest.ecw_drv.CreateCopy(
        tmp_path / "jrc312.ecw", vrt_ds, options=["ECW_FORMAT_VERSION=3", "TARGET=75"]
    )

    assert dswr.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_BlueBand, (
        "Band 1 color interpretation should be Blue  but is : "
        + gdal.GetColorInterpretationName(
            dswr.GetRasterBand(1).GetColorInterpretation()
        )
    )
    assert dswr.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_RedBand, (
        "Band 2 color interpretation should be Red but is : "
        + gdal.GetColorInterpretationName(
            dswr.GetRasterBand(2).GetColorInterpretation()
        )
    )
    assert dswr.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_GreenBand, (
        "Band 3 color interpretation should be Green but is : "
        + gdal.GetColorInterpretationName(
            dswr.GetRasterBand(3).GetColorInterpretation()
        )
    )

    dswr = None

    dsr = gdal.Open(tmp_path / "jrc312.ecw")

    assert dsr.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_BlueBand, (
        "Band 1 color interpretation should be Blue  but is : "
        + gdal.GetColorInterpretationName(dsr.GetRasterBand(1).GetColorInterpretation())
    )
    assert dsr.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_RedBand, (
        "Band 2 color interpretation should be Red but is : "
        + gdal.GetColorInterpretationName(dsr.GetRasterBand(2).GetColorInterpretation())
    )
    assert dsr.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_GreenBand, (
        "Band 3 color interpretation should be Green but is : "
        + gdal.GetColorInterpretationName(dsr.GetRasterBand(3).GetColorInterpretation())
    )

    dsr = None


###############################################################################
# Make sure that band descriptions are preserved for version 2 ECW files when
# color space set implicitly to sRGB.


def test_ecw_37(tmp_path):

    if not has_write_support():
        pytest.skip("ECW write support not available")
    if gdaltest.ecw_drv.major_version < 5:
        pytest.skip("Requires ECW SDK >= 5.0")

    ds = gdal.Open("data/ecw/jrc.ecw")

    dswr = gdaltest.ecw_drv.CreateCopy(
        tmp_path / "jrc123.ecw", ds, options=["ECW_FORMAT_VERSION=3", "TARGET=75"]
    )

    assert dswr.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand, (
        "Band 1 color interpretation should be Red but is : "
        + gdal.GetColorInterpretationName(
            dswr.GetRasterBand(1).GetColorInterpretation()
        )
    )
    assert dswr.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand, (
        "Band 2 color interpretation should be Green but is : "
        + gdal.GetColorInterpretationName(
            dswr.GetRasterBand(2).GetColorInterpretation()
        )
    )
    assert dswr.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand, (
        "Band 3 color interpretation should be Blue but is : "
        + gdal.GetColorInterpretationName(
            dswr.GetRasterBand(3).GetColorInterpretation()
        )
    )

    dswr = None

    dsr = gdal.Open(tmp_path / "jrc123.ecw")

    assert dsr.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand, (
        "Band 1 color interpretation should be Red  but is : "
        + gdal.GetColorInterpretationName(dsr.GetRasterBand(1).GetColorInterpretation())
    )
    assert dsr.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand, (
        "Band 2 color interpretation should be Green but is : "
        + gdal.GetColorInterpretationName(dsr.GetRasterBand(2).GetColorInterpretation())
    )
    assert dsr.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand, (
        "Band 3 color interpretation should be Blue but is : "
        + gdal.GetColorInterpretationName(dsr.GetRasterBand(3).GetColorInterpretation())
    )

    dsr = None


###############################################################################
# Check opening unicode files.


def test_ecw_38(tmp_path):

    fname = (
        tmp_path / "za\u017c\u00f3\u0142\u0107g\u0119\u015bl\u0105ja\u017a\u0144.ecw"
    )

    if gdaltest.ecw_drv.major_version < 4:
        pytest.skip("Requires ECW SDK >= 4.0")

    shutil.copyfile("data/ecw/jrc.ecw", fname)

    ds = gdal.Open("data/ecw/jrc.ecw")

    ds_ref = gdal.Open(fname)

    maxdiff = gdaltest.compare_ds(ds, ds_ref)

    ds = None
    ds_ref = None

    # Quite a bit of difference...
    assert maxdiff <= 0, "Image too different from reference"


###############################################################################
# Check writing histograms.


def test_ecw_39(tmp_path):

    if not has_write_support():
        pytest.skip("ECW write support not available")
    if gdaltest.ecw_drv.major_version < 5:
        pytest.skip("Requires ECW SDK >= 5.0")

    ds = gdal.Open("data/ecw/jrc.ecw")

    dswr = gdaltest.ecw_drv.CreateCopy(
        tmp_path / "jrcstats.ecw", ds, options=["ECW_FORMAT_VERSION=3", "TARGET=75"]
    )
    ds = None
    hist = (0, 255, 2, [3, 4])

    dswr.GetRasterBand(1).SetDefaultHistogram(0, 255, [3, 4])
    dswr = None

    ds = gdal.Open(tmp_path / "jrcstats.ecw")

    result = hist == ds.GetRasterBand(1).GetDefaultHistogram(force=0)

    ds = None
    assert result, "Default histogram written incorrectly"


###############################################################################
# Check reading a ECW v3 file


def test_ecw_40():

    with gdaltest.disable_exceptions():
        ds = gdal.Open("data/ecw/stefan_full_rgba_ecwv3_meta.ecw")
    if ds is None:
        if gdaltest.ecw_drv.major_version < 5:
            if gdal.GetLastErrorMsg().find("requires ECW SDK 5.0") >= 0:
                pytest.skip()
            pytest.fail("explicit error message expected")
        pytest.fail()

    expected_md = [
        ("CLOCKWISE_ROTATION_DEG", "0.000000"),
        ("COLORSPACE", "RGB"),
        ("COMPRESSION_DATE", "2013-04-04T09:20:03Z"),
        ("COMPRESSION_RATE_ACTUAL", "3.165093"),
        ("COMPRESSION_RATE_TARGET", "20"),
        (
            "FILE_METADATA_COMPRESSION_SOFTWARE",
            "python2.7/GDAL v1.10.0.0/ECWJP2 SDK v5.0.0.0",
        ),
        ("FILE_METADATA_ACQUISITION_DATE", "2012-09-12"),
        ("FILE_METADATA_ACQUISITION_SENSOR_NAME", "Leica ADS-80"),
        (
            "FILE_METADATA_ADDRESS",
            "2 Abbotsford Street, West Leederville WA 6007 Australia",
        ),
        ("FILE_METADATA_AUTHOR", "Unknown"),
        ("FILE_METADATA_CLASSIFICATION", "test gdal image"),
        ("FILE_METADATA_COMPANY", "ERDAS-QA"),
        (
            "FILE_METADATA_COMPRESSION_SOFTWARE",
            "python2.7/GDAL v1.10.0.0/ECWJP2 SDK v5.0.0.0",
        ),
        ("FILE_METADATA_COPYRIGHT", "Intergraph 2013"),
        ("FILE_METADATA_EMAIL", "support@intergraph.com"),
        ("FILE_METADATA_TELEPHONE", "+61 8 9388 2900"),
        ("VERSION", "3"),
    ]

    got_md = ds.GetMetadata()
    for key, value in expected_md:
        assert key in got_md and got_md[key] == value

    expected_cs_list = [28760, 59071, 54087, 22499]
    for i in range(4):
        got_cs = ds.GetRasterBand(i + 1).Checksum()
        assert got_cs == expected_cs_list[i]


###############################################################################
# Check generating statistics & histogram for a ECW v3 file


def test_ecw_41(tmp_path):

    if gdaltest.ecw_drv.major_version < 5:
        pytest.skip("Requires ECW SDK >= 5.0")

    shutil.copy(
        "data/ecw/stefan_full_rgba_ecwv3_meta.ecw",
        tmp_path / "stefan_full_rgba_ecwv3_meta.ecw",
    )

    ds = gdal.Open(tmp_path / "stefan_full_rgba_ecwv3_meta.ecw")

    # Check that no statistics is already included in the file
    assert ds.GetRasterBand(1).GetMinimum() is None
    assert ds.GetRasterBand(1).GetMaximum() is None
    assert ds.GetRasterBand(1).GetStatistics(1, 0) is None
    assert ds.GetRasterBand(1).GetDefaultHistogram(force=0) is None

    # Now compute the stats
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    expected_stats = [0.0, 255.0, 41.122843450479, 66.517011844777]
    for i in range(4):
        assert stats[i] == pytest.approx(expected_stats[i], abs=1)

    ds = None

    # Check that there's no .aux.xml file
    assert not os.path.exists(tmp_path / "stefan_full_rgba_ecwv3_meta.ecw.aux.xml")

    ds = gdal.Open(tmp_path / "stefan_full_rgba_ecwv3_meta.ecw")
    assert ds.GetRasterBand(1).GetMinimum() == 0
    assert ds.GetRasterBand(1).GetMaximum() == 255
    stats = ds.GetRasterBand(1).GetStatistics(0, 0)
    expected_stats = [0.0, 255.0, 41.122843450479, 66.517011844777]
    for i in range(4):
        assert stats[i] == pytest.approx(expected_stats[i], abs=1)
    ds = None

    ds = gdal.Open(tmp_path / "stefan_full_rgba_ecwv3_meta.ecw")
    # And compute the histogram
    got_hist = ds.GetRasterBand(1).GetDefaultHistogram()
    expected_hist = (
        -0.5,
        255.5,
        256,
        [
            603,
            4870,
            428,
            78,
            13,
            24,
            62,
            118,
            58,
            125,
            162,
            180,
            133,
            146,
            70,
            81,
            84,
            97,
            90,
            60,
            79,
            70,
            85,
            77,
            73,
            63,
            60,
            64,
            56,
            69,
            63,
            73,
            70,
            72,
            61,
            66,
            40,
            52,
            65,
            44,
            62,
            54,
            56,
            55,
            63,
            51,
            47,
            39,
            58,
            44,
            36,
            43,
            47,
            45,
            54,
            28,
            40,
            41,
            37,
            36,
            33,
            31,
            28,
            34,
            19,
            32,
            19,
            23,
            23,
            33,
            16,
            34,
            32,
            54,
            29,
            33,
            40,
            37,
            27,
            34,
            24,
            29,
            26,
            21,
            22,
            24,
            25,
            19,
            29,
            22,
            24,
            14,
            20,
            20,
            29,
            28,
            13,
            19,
            21,
            19,
            19,
            21,
            13,
            19,
            13,
            14,
            22,
            15,
            13,
            26,
            10,
            13,
            13,
            14,
            10,
            17,
            15,
            19,
            11,
            18,
            11,
            14,
            8,
            12,
            20,
            12,
            17,
            10,
            15,
            15,
            16,
            14,
            11,
            7,
            7,
            10,
            8,
            12,
            7,
            8,
            14,
            7,
            9,
            12,
            4,
            6,
            12,
            5,
            5,
            4,
            11,
            8,
            4,
            8,
            7,
            10,
            11,
            6,
            7,
            5,
            6,
            8,
            10,
            10,
            7,
            5,
            3,
            5,
            5,
            6,
            4,
            10,
            7,
            6,
            8,
            4,
            6,
            6,
            4,
            6,
            6,
            7,
            10,
            4,
            5,
            2,
            5,
            6,
            1,
            1,
            2,
            6,
            2,
            1,
            7,
            4,
            1,
            3,
            3,
            2,
            6,
            2,
            3,
            3,
            3,
            3,
            5,
            5,
            4,
            2,
            3,
            2,
            1,
            3,
            5,
            5,
            4,
            1,
            1,
            2,
            5,
            10,
            5,
            9,
            3,
            5,
            3,
            5,
            4,
            5,
            4,
            4,
            6,
            7,
            9,
            17,
            13,
            15,
            14,
            13,
            20,
            18,
            16,
            27,
            35,
            53,
            60,
            51,
            46,
            40,
            38,
            50,
            66,
            36,
            45,
            13,
        ],
    )
    assert got_hist == expected_hist
    ds = None

    # Remove the .aux.xml file
    try:
        os.remove(tmp_path / "stefan_full_rgba_ecwv3_meta.ecw.aux.xml")
    except OSError:
        pass

    ds = gdal.Open(tmp_path / "stefan_full_rgba_ecwv3_meta.ecw")
    assert ds.GetRasterBand(1).GetMinimum() == 0
    assert ds.GetRasterBand(1).GetMaximum() == 255
    got_hist = ds.GetRasterBand(1).GetDefaultHistogram(force=0)
    assert got_hist == expected_hist
    ds = None

    # Check that there's no .aux.xml file
    assert not os.path.exists(tmp_path / "stefan_full_rgba_ecwv3_meta.ecw.aux.xml")


###############################################################################
# Test setting/unsetting file metadata of a ECW v3 file


def test_ecw_42(tmp_path):

    if gdaltest.ecw_drv.major_version < 5:
        pytest.skip("Requires ECW SDK >= 5.0")

    shutil.copy(
        "data/ecw/stefan_full_rgba_ecwv3_meta.ecw",
        tmp_path / "stefan_full_rgba_ecwv3_meta.ecw",
    )

    ds = gdal.Open(tmp_path / "stefan_full_rgba_ecwv3_meta.ecw", gdal.GA_Update)
    md = {}
    md["FILE_METADATA_CLASSIFICATION"] = "FILE_METADATA_CLASSIFICATION"
    md["FILE_METADATA_ACQUISITION_DATE"] = "2013-04-04"
    md["FILE_METADATA_ACQUISITION_SENSOR_NAME"] = (
        "FILE_METADATA_ACQUISITION_SENSOR_NAME"
    )
    md["FILE_METADATA_COMPRESSION_SOFTWARE"] = "FILE_METADATA_COMPRESSION_SOFTWARE"
    md["FILE_METADATA_AUTHOR"] = "FILE_METADATA_AUTHOR"
    md["FILE_METADATA_COPYRIGHT"] = "FILE_METADATA_COPYRIGHT"
    md["FILE_METADATA_COMPANY"] = "FILE_METADATA_COMPANY"
    md["FILE_METADATA_EMAIL"] = "FILE_METADATA_EMAIL"
    md["FILE_METADATA_ADDRESS"] = "FILE_METADATA_ADDRESS"
    md["FILE_METADATA_TELEPHONE"] = "FILE_METADATA_TELEPHONE"
    ds.SetMetadata(md)
    ds = None

    # Check that there's no .aux.xml file
    assert not os.path.exists(tmp_path / "stefan_full_rgba_ecwv3_meta.ecw.aux.xml")

    # Check item values
    ds = gdal.Open(tmp_path / "stefan_full_rgba_ecwv3_meta.ecw")
    got_md = ds.GetMetadata()
    for item in md:
        assert got_md[item] == md[item]
    ds = None

    # Test unsetting all the stuff
    ds = gdal.Open(tmp_path / "stefan_full_rgba_ecwv3_meta.ecw", gdal.GA_Update)
    md = {}
    md["FILE_METADATA_CLASSIFICATION"] = ""
    md["FILE_METADATA_ACQUISITION_DATE"] = "1970-01-01"
    md["FILE_METADATA_ACQUISITION_SENSOR_NAME"] = ""
    md["FILE_METADATA_COMPRESSION_SOFTWARE"] = ""
    md["FILE_METADATA_AUTHOR"] = ""
    md["FILE_METADATA_COPYRIGHT"] = ""
    md["FILE_METADATA_COMPANY"] = ""
    md["FILE_METADATA_EMAIL"] = ""
    md["FILE_METADATA_ADDRESS"] = ""
    md["FILE_METADATA_TELEPHONE"] = ""
    ds.SetMetadata(md)
    ds = None

    # Check that there's no .aux.xml file
    assert not os.path.exists(tmp_path / "stefan_full_rgba_ecwv3_meta.ecw.aux.xml")

    # Check item values
    ds = gdal.Open(tmp_path / "stefan_full_rgba_ecwv3_meta.ecw")
    got_md = ds.GetMetadata()
    for item in md:
        assert item not in got_md or item == "FILE_METADATA_ACQUISITION_DATE", md[item]
    ds = None


###############################################################################
# Test auto-promotion of 1bit alpha band to 8bit
# Note: only works on reversible files like this one


def test_ecw_43():

    if gdaltest.jp2ecw_drv is None:
        pytest.skip()

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

    tmp_ds = gdal.GetDriverByName("GTiff").CreateCopy("/vsimem/ecw_43.tif", ds)
    fourth_band = tmp_ds.GetRasterBand(4)
    got_cs = fourth_band.Checksum()
    gtiff_bands_data = tmp_ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    gtiff_fourth_band_data = fourth_band.ReadRaster(
        0, 0, ds.RasterXSize, ds.RasterYSize
    )
    # gtiff_fourth_band_subsampled_data = fourth_band.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize,ds.RasterXSize/16,ds.RasterYSize/16)
    tmp_ds = None
    gdal.GetDriverByName("GTiff").Delete("/vsimem/ecw_43.tif")
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
# Test metadata retrieval from JP2 file


def test_ecw_44():

    if gdaltest.jp2ecw_drv is None:
        pytest.skip()
    if gdaltest.ecw_drv.major_version < 5 or (
        gdaltest.ecw_drv.major_version == 5 and gdaltest.ecw_drv.minor_version < 1
    ):
        pytest.skip("Requires ECW SDK >= 5.1")

    ds = gdal.Open("data/jpeg2000/stefan_full_rgba_alpha_1bit.jp2")

    expected_md = [
        ("CODE_BLOCK_SIZE_X", "64"),
        ("CODE_BLOCK_SIZE_Y", "64"),
        ("GML_JP2_DATA", "FALSE"),
        ("PRECINCT_SIZE_X", "128,128"),
        ("PRECINCT_SIZE_Y", "128,128"),
        ("PRECISION", "8,8,8,1"),
        ("PROFILE", "0"),
        ("PROGRESSION_ORDER", "RPCL"),
        ("QUALITY_LAYERS", "1"),
        ("RESOLUTION_LEVELS", "2"),
        ("PROGRESSION_ORDER", "RPCL"),
        ("TILE_HEIGHT", "150"),
        ("TILE_WIDTH", "162"),
        ("TILES_X", "1"),
        ("TILES_Y", "1"),
        ("TRANSFORMATION_TYPE", "5x3"),
        ("USE_EPH", "TRUE"),
        ("USE_SOP", "FALSE"),
    ]

    got_md = ds.GetMetadata("JPEG2000")
    for key, value in expected_md:
        assert key in got_md and got_md[key] == value


###############################################################################
# Test metadata reading & writing


def RemoveDriverMetadata(md):
    if "COMPRESSION_RATE_TARGET" in md:
        del md["COMPRESSION_RATE_TARGET"]
    if "COLORSPACE" in md:
        del md["COLORSPACE"]
    if "VERSION" in md:
        del md["VERSION"]
    return md


def test_ecw_45():
    if gdaltest.jp2ecw_drv is None or not has_write_support():
        pytest.skip("ECW write support not available")

    # No metadata
    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    out_ds = gdaltest.jp2ecw_drv.CreateCopy(
        "/vsimem/ecw_45.jp2", src_ds, options=["WRITE_METADATA=YES"]
    )
    del out_ds
    assert gdal.VSIStatL("/vsimem/ecw_45.jp2.aux.xml") is None
    ds = gdal.Open("/vsimem/ecw_45.jp2")
    md = RemoveDriverMetadata(ds.GetMetadata())
    assert md == {}
    gdal.Unlink("/vsimem/ecw_45.jp2")

    # Simple metadata in main domain
    for options in [["WRITE_METADATA=YES"]]:
        src_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
        src_ds.SetMetadataItem("FOO", "BAR")
        out_ds = gdaltest.jp2ecw_drv.CreateCopy(
            "/vsimem/ecw_45.jp2", src_ds, options=options
        )
        del out_ds
        assert gdal.VSIStatL("/vsimem/ecw_45.jp2.aux.xml") is None
        ds = gdal.Open("/vsimem/ecw_45.jp2")
        md = RemoveDriverMetadata(ds.GetMetadata())
        assert md == {"FOO": "BAR"}
        gdal.Unlink("/vsimem/ecw_45.jp2")

    # Simple metadata in auxiliary domain
    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    src_ds.SetMetadataItem("FOO", "BAR", "SOME_DOMAIN")
    out_ds = gdaltest.jp2ecw_drv.CreateCopy(
        "/vsimem/ecw_45.jp2", src_ds, options=["WRITE_METADATA=YES"]
    )
    del out_ds
    assert gdal.VSIStatL("/vsimem/ecw_45.jp2.aux.xml") is None
    ds = gdal.Open("/vsimem/ecw_45.jp2")
    md = RemoveDriverMetadata(ds.GetMetadata("SOME_DOMAIN"))
    assert md == {"FOO": "BAR"}
    gdal.Unlink("/vsimem/ecw_45.jp2")

    # Simple metadata in auxiliary XML domain
    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    src_ds.SetMetadata(["<some_arbitrary_xml_box/>"], "xml:SOME_DOMAIN")
    out_ds = gdaltest.jp2ecw_drv.CreateCopy(
        "/vsimem/ecw_45.jp2", src_ds, options=["WRITE_METADATA=YES"]
    )
    del out_ds
    assert gdal.VSIStatL("/vsimem/ecw_45.jp2.aux.xml") is None
    ds = gdal.Open("/vsimem/ecw_45.jp2")
    assert ds.GetMetadata("xml:SOME_DOMAIN")[0] == "<some_arbitrary_xml_box />\n"
    gdal.Unlink("/vsimem/ecw_45.jp2")

    # Special xml:BOX_ metadata domain
    for options in [["WRITE_METADATA=YES"]]:
        src_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
        src_ds.SetMetadata(["<some_arbitrary_xml_box/>"], "xml:BOX_1")
        out_ds = gdaltest.jp2ecw_drv.CreateCopy(
            "/vsimem/ecw_45.jp2", src_ds, options=options
        )
        del out_ds
        assert gdal.VSIStatL("/vsimem/ecw_45.jp2.aux.xml") is None
        ds = gdal.Open("/vsimem/ecw_45.jp2")
        assert ds.GetMetadata("xml:BOX_0")[0] == "<some_arbitrary_xml_box/>"
        gdal.Unlink("/vsimem/ecw_45.jp2")

    # Special xml:XMP metadata domain
    for options in [["WRITE_METADATA=YES"]]:
        src_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
        src_ds.SetMetadata(["<fake_xmp_box/>"], "xml:XMP")
        out_ds = gdaltest.jp2ecw_drv.CreateCopy(
            "/vsimem/ecw_45.jp2", src_ds, options=options
        )
        del out_ds
        assert gdal.VSIStatL("/vsimem/ecw_45.jp2.aux.xml") is None
        ds = gdal.Open("/vsimem/ecw_45.jp2")
        assert ds.GetMetadata("xml:XMP")[0] == "<fake_xmp_box/>"
        gdal.Unlink("/vsimem/ecw_45.jp2")


###############################################################################
# Test non nearest upsampling


def test_ecw_46():

    if gdaltest.jp2ecw_drv is None or not has_write_support():
        pytest.skip("ECW write support not available")

    tmp_ds = gdaltest.jp2ecw_drv.CreateCopy(
        "/vsimem/ecw_46.jp2", gdal.Open("data/int16.tif")
    )
    tmp_ds = None
    tmp_ds = gdal.Open("/vsimem/ecw_46.jp2")
    full_res_data = tmp_ds.ReadRaster(0, 0, 20, 20)
    upsampled_data = tmp_ds.ReadRaster(
        0, 0, 20, 20, 40, 40, resample_alg=gdal.GRIORA_Cubic
    )
    tmp_ds = None
    gdal.Unlink("/vsimem/ecw_46.jp2")

    tmp_ds = gdal.GetDriverByName("MEM").Create("", 20, 20, 1, gdal.GDT_Int16)
    tmp_ds.GetRasterBand(1).WriteRaster(0, 0, 20, 20, full_res_data)
    ref_upsampled_data = tmp_ds.ReadRaster(
        0, 0, 20, 20, 40, 40, resample_alg=gdal.GRIORA_Cubic
    )

    mem_ds = gdal.GetDriverByName("MEM").Create("", 40, 40, 1, gdal.GDT_Int16)
    mem_ds.GetRasterBand(1).WriteRaster(0, 0, 40, 40, ref_upsampled_data)
    ref_cs = mem_ds.GetRasterBand(1).Checksum()
    mem_ds.GetRasterBand(1).WriteRaster(0, 0, 40, 40, upsampled_data)
    cs = mem_ds.GetRasterBand(1).Checksum()
    assert cs == ref_cs


###############################################################################
# Test non nearest upsampling on multiband data


def test_ecw_non_nearest_upsampling_multiband():

    if gdaltest.jp2ecw_drv is None:
        pytest.skip()

    ds = gdal.Open("data/jpeg2000/stefan_full_rgba.jp2")
    full_res_data = ds.ReadRaster(0, 0, 162, 150)
    upsampled_data = ds.ReadRaster(
        0, 0, 162, 150, 162 * 2, 150 * 2, resample_alg=gdal.GRIORA_Cubic
    )

    tmp_ds = gdal.GetDriverByName("MEM").Create("", 162, 150, ds.RasterCount)
    tmp_ds.WriteRaster(0, 0, 162, 150, full_res_data)
    ref_upsampled_data = tmp_ds.ReadRaster(
        0, 0, 162, 150, 162 * 2, 150 * 2, resample_alg=gdal.GRIORA_Cubic
    )

    assert upsampled_data == ref_upsampled_data


###############################################################################
# /vsi reading with ECW (#6482)


def test_ecw_47():

    data = open("data/ecw/jrc.ecw", "rb").read()
    gdal.FileFromMemBuffer("/vsimem/ecw_47.ecw", data)

    ds = gdal.Open("/vsimem/ecw_47.ecw")
    assert ds is not None

    mean_tolerance = 0.5

    if gdaltest.ecw_drv.major_version == 5:
        exp_mean, exp_stddev = (141.606, 67.2919)
    elif gdaltest.ecw_drv.major_version == 4:
        exp_mean, exp_stddev = (140.332, 67.611)
    elif gdaltest.ecw_drv.major_version == 3:
        exp_mean, exp_stddev = (141.172, 67.3636)

    mean, stddev = ds.GetRasterBand(1).ComputeBandStats()

    assert mean == pytest.approx(
        exp_mean, abs=mean_tolerance
    ) and stddev == pytest.approx(
        exp_stddev, abs=0.5
    ), "mean/stddev of (%g,%g) diffs from expected(%g,%g)" % (
        mean,
        stddev,
        exp_mean,
        exp_stddev,
    )

    gdal.Unlink("/vsimem/ecw_47.ecw")


###############################################################################
# Test "Upward" orientation is forced by default


def test_ecw_48():

    ecw_upward = gdal.GetConfigOption("ECW_ALWAYS_UPWARD", "TRUE")
    assert (
        ecw_upward == "TRUE" or ecw_upward == "ON"
    ), "ECW_ALWAYS_UPWARD default value must be TRUE."

    ds = gdal.Open("data/ecw/spif83_downward.ecw")
    gt = ds.GetGeoTransform()

    # expect Y resolution negative
    expected_gt = (
        6138559.5576418638,
        195.5116973254697,
        0.0,
        2274798.7836679211,
        0.0,
        -198.32414964918371,
    )
    assert gt == expected_gt, "did not get expected geotransform."


###############################################################################
# Test "Upward" orientation can be overridden with ECW_ALWAYS_UPWARD=FALSE


def test_ecw_49():

    with gdal.config_option("ECW_ALWAYS_UPWARD", "FALSE"):
        ds = gdal.Open("data/ecw/spif83_downward.ecw")
        gt = ds.GetGeoTransform()

    # expect Y resolution positive
    expected_gt = (
        6138559.5576418638,
        195.5116973254697,
        0.0,
        2274798.7836679211,
        0.0,
        198.32414964918371,
    )
    assert gt == expected_gt, "did not get expected geotransform."


###############################################################################
# Test reading UInt32 file


@gdaltest.disable_exceptions()
def test_ecw_read_uint32_jpeg2000():

    ds = gdal.OpenEx("data/jpeg2000/uint32_2x2_lossless_nbits_20.j2k", gdal.OF_RASTER)
    if gdaltest.ecw_drv.major_version == 3:
        assert ds is not None
    else:
        if ds is None:
            pytest.skip(
                "This version of the ECW SDK has issues to decode UInt32 JPEG2000 files"
            )
    assert struct.unpack("I" * 4, ds.ReadRaster(0, 0, 2, 2)) == (
        0,
        1048575,
        1048574,
        524288,
    )


###############################################################################
# Test unsupported XML SRS


def test_jp2ecw_unsupported_srs_for_gmljp2(tmp_vsimem):

    if gdaltest.jp2ecw_drv is None or not has_write_support():
        pytest.skip("ECW write support not available")

    filename = str(tmp_vsimem / "out.jp2")
    # There is no EPSG code and Albers Equal Area is not supported by OGRSpatialReference::exportToXML()
    wkt = """PROJCRS["Africa_Albers_Equal_Area_Conic",
    BASEGEOGCRS["WGS 84",
        DATUM["World Geodetic System 1984",
            ELLIPSOID["WGS 84",6378137,298.257223563,
                LENGTHUNIT["metre",1]]],
        PRIMEM["Greenwich",0,
            ANGLEUNIT["degree",0.0174532925199433]],
        ID["EPSG",4326]],
    CONVERSION["Albers Equal Area",
        METHOD["Albers Equal Area",
            ID["EPSG",9822]],
        PARAMETER["Latitude of false origin",0,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8821]],
        PARAMETER["Longitude of false origin",25,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8822]],
        PARAMETER["Latitude of 1st standard parallel",20,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8823]],
        PARAMETER["Latitude of 2nd standard parallel",-23,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8824]],
        PARAMETER["Easting at false origin",0,
            LENGTHUNIT["metre",1],
            ID["EPSG",8826]],
        PARAMETER["Northing at false origin",0,
            LENGTHUNIT["metre",1],
            ID["EPSG",8827]]],
    CS[Cartesian,2],
        AXIS["easting",east,
            ORDER[1],
            LENGTHUNIT["metre",1,
                ID["EPSG",9001]]],
        AXIS["northing",north,
            ORDER[2],
            LENGTHUNIT["metre",1,
                ID["EPSG",9001]]]]"""
    gdal.ErrorReset()
    assert gdal.Translate(filename, "data/byte.tif", outputSRS=wkt, format="JP2ECW")
    assert gdal.GetLastErrorMsg() == ""
    ds = gdal.Open(filename)
    ref_srs = osr.SpatialReference()
    ref_srs.ImportFromWkt(wkt)
    assert ds.GetSpatialRef().IsSame(ref_srs)
    # Check that we do *not* have a GMLJP2 box
    assert "xml:gml.root-instance" not in ds.GetMetadataDomainList()


###############################################################################


def test_ecw_online_1():
    if gdaltest.jp2ecw_drv is None:
        pytest.skip()

    gdaltest.download_or_skip(
        "http://download.osgeo.org/gdal/data/jpeg2000/7sisters200.j2k",
        "7sisters200.j2k",
    )

    # checksum = 32316 on my PC
    tst = gdaltest.GDALTest(
        "JP2ECW", "tmp/cache/7sisters200.j2k", 1, None, filename_absolute=1
    )

    tst.testOpen()

    ds = gdal.Open("tmp/cache/7sisters200.j2k")
    ds.GetRasterBand(1).Checksum()
    ds = None


###############################################################################


def test_ecw_online_2():
    if gdaltest.jp2ecw_drv is None:
        pytest.skip()

    gdaltest.download_or_skip(
        "http://download.osgeo.org/gdal/data/jpeg2000/gcp.jp2", "gcp.jp2"
    )

    # checksum = 1292 on my PC
    tst = gdaltest.GDALTest("JP2ECW", "tmp/cache/gcp.jp2", 1, None, filename_absolute=1)

    tst.testOpen()

    ds = gdal.Open("tmp/cache/gcp.jp2")
    ds.GetRasterBand(1).Checksum()
    assert len(ds.GetGCPs()) == 15, "bad number of GCP"

    assert ds.GetGCPSpatialRef().GetAuthorityCode(None) == "4326"

    ds = None


###############################################################################


def ecw_online_3():
    if gdaltest.jp2ecw_drv is None:
        pytest.skip()
    if gdaltest.ecw_drv.major_version == 4:
        pytest.skip("4.x SDK gets unreliable results for jp2")

    gdaltest.download_or_skip(
        "http://www.openjpeg.org/samples/Bretagne1.j2k", "Bretagne1.j2k"
    )

    gdaltest.download_or_skip(
        "http://www.openjpeg.org/samples/Bretagne1.bmp", "Bretagne1.bmp"
    )

    # checksum = 16481 on my PC
    tst = gdaltest.GDALTest(
        "JP2ECW", "tmp/cache/Bretagne1.j2k", 1, None, filename_absolute=1
    )

    tst.testOpen()

    ds = gdal.Open("tmp/cache/Bretagne1.j2k")
    ds_ref = gdal.Open("tmp/cache/Bretagne1.bmp")
    maxdiff = gdaltest.compare_ds(ds, ds_ref)
    print(ds.GetRasterBand(1).Checksum())
    print(ds_ref.GetRasterBand(1).Checksum())

    ds = None
    ds_ref = None

    # Difference between the image before and after compression
    assert maxdiff <= 16, "Image too different from reference"


###############################################################################


def test_ecw_online_4():

    if gdaltest.jp2ecw_drv is None:
        pytest.skip()

    if gdaltest.ecw_drv.major_version == 5 and gdaltest.ecw_drv.minor_version == 2:
        pytest.skip("This test hangs on Linux in a mutex in the SDK 5.2.1")

    gdaltest.download_or_skip(
        "http://www.openjpeg.org/samples/Bretagne2.j2k", "Bretagne2.j2k"
    )

    gdaltest.download_or_skip(
        "http://www.openjpeg.org/samples/Bretagne2.bmp", "Bretagne2.bmp"
    )

    # Checksum = 53054 on my PC
    tst = gdaltest.GDALTest(
        "JP2ECW", "tmp/cache/Bretagne2.j2k", 1, None, filename_absolute=1
    )

    tst.testOpen()

    ds = gdal.Open("tmp/cache/Bretagne2.j2k")
    ds_ref = gdal.Open("tmp/cache/Bretagne2.bmp")
    maxdiff = gdaltest.compare_ds(ds, ds_ref, width=256, height=256)
    #    print(ds.GetRasterBand(1).Checksum())
    #    print(ds_ref.GetRasterBand(1).Checksum())

    ds = None
    ds_ref = None

    # Difference between the image before and after compression
    assert maxdiff <= 1, "Image too different from reference"


###############################################################################


def test_ecw_online_5():

    gdaltest.download_or_skip(
        "http://download.osgeo.org/gdal/data/ecw/red_flower.ecw", "red_flower.ecw"
    )

    ds = gdal.Open("tmp/cache/red_flower.ecw")

    if gdaltest.ecw_drv.major_version == 3:
        exp_mean, exp_stddev = (112.801, 52.0431)
        # on Tamas slavebots, (mean,stddev)  = (113.301,52.0434)
        mean_tolerance = 1
    else:
        mean_tolerance = 0.5
        if gdaltest.ecw_drv.major_version == 5:
            exp_mean, exp_stddev = (113.345, 52.1259)
        else:
            exp_mean, exp_stddev = (114.337, 52.1751)

    mean, stddev = ds.GetRasterBand(2).ComputeBandStats()

    assert mean == pytest.approx(
        exp_mean, abs=mean_tolerance
    ) and stddev == pytest.approx(
        exp_stddev, abs=0.5
    ), "mean/stddev of (%g,%g) diffs from expected(%g,%g)" % (
        mean,
        stddev,
        exp_mean,
        exp_stddev,
    )


###############################################################################
# This tests the HTTP driver in fact. To ensure if keeps the original filename,
# and in particular the .ecw extension, to make the ECW driver happy


@pytest.mark.require_driver("HTTP")
def test_ecw_online_6():

    url = "http://download.osgeo.org/gdal/data/ecw/spif83.ecw"
    ds = gdal.Open(url)

    if ds is None:
        # The ECW driver (3.3) doesn't manage to open in /vsimem, thus fallbacks
        # to writing to /tmp, which doesn't work on Windows
        if sys.platform == "win32":
            pytest.skip()

        conn = gdaltest.gdalurlopen(url)
        if conn is None:
            pytest.skip("cannot open URL")
        conn.close()

        pytest.fail()
    ds = None


###############################################################################
# ECWv2 file with alpha channel (#6028)


def test_ecw_online_7():

    gdaltest.download_or_skip(
        "http://download.osgeo.org/gdal/data/ecw/sandiego2m_null.ecw",
        "sandiego2m_null.ecw",
    )

    ds = gdal.Open("tmp/cache/sandiego2m_null.ecw")
    if gdaltest.ecw_drv.major_version == 3:
        expected_band_count = 3
    else:
        expected_band_count = 4
    assert ds.RasterCount == expected_band_count, "Expected %d bands, got %d" % (
        expected_band_count,
        ds.RasterCount,
    )


###############################################################################
# Test reading a dataset whose tile dimensions are larger than dataset ones


def test_ecw_byte_tile_2048():

    ds = gdal.Open("data/jpeg2000/byte_tile_2048.jp2")
    blockxsize, blockysize = ds.GetRasterBand(1).GetBlockSize()
    assert (blockxsize, blockysize) == (
        (20, 20) if gdaltest.ecw_drv.version >= (5, 1) else (256, 256)
    )
