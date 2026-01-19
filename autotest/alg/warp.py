#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test the image reprojection functions. Try to test as many
#           resamplers as possible (we have optimized resamplers for some
#           data types, test them too).
# Author:   Andrey Kiselev, dron16@ak4719.spb.edu
#
###############################################################################
# Copyright (c) 2008, Andrey Kiselev <dron16@ak4719.spb.edu>
# Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import math
import os
import shutil
import struct
import sys

import gdaltest
import pytest

pytestmark = pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)

from osgeo import gdal, osr

###############################################################################
# Verify that we always getting the same image when warping.
# Warp the image using the VRT file and compare result with reference image

# Upsampling


def test_warp_1():

    ds = gdal.Open("data/utmsmall_near.vrt")
    ref_ds = gdal.Open("data/utmsmall_near.tiff")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


def test_warp_1_short():

    ds = gdal.Open("data/utmsmall_near_short.vrt")
    ref_ds = gdal.Open("data/utmsmall_near.tiff")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


def test_warp_1_ushort():

    ds = gdal.Open("data/utmsmall_near_ushort.vrt")
    ref_ds = gdal.Open("data/utmsmall_near.tiff")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


def test_warp_1_float():

    ds = gdal.Open("data/utmsmall_near_float.vrt")
    ref_ds = gdal.Open("data/utmsmall_near.tiff")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


def test_warp_2():

    ds = gdal.Open("data/utmsmall_blinear.vrt")
    ref_ds = gdal.Open("data/utmsmall_blinear.tiff")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


def test_warp_2_short():

    ds = gdal.Open("data/utmsmall_blinear_short.vrt")
    ref_ds = gdal.Open("data/utmsmall_blinear.tiff")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


def test_warp_2_ushort():

    ds = gdal.Open("data/utmsmall_blinear_ushort.vrt")
    ref_ds = gdal.Open("data/utmsmall_blinear.tiff")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


def test_warp_2_downsize():

    ds = gdal.Open("data/utmsmall_bilinear_2.vrt")
    ref_ds = gdal.Open("data/utmsmall_bilinear_2.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


def test_warp_3():

    ds = gdal.Open("data/utmsmall_cubic.vrt")
    ref_ds = gdal.Open("data/utmsmall_cubic.tiff")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


def test_warp_3_short():

    ds = gdal.Open("data/utmsmall_cubic_short.vrt")
    ref_ds = gdal.Open("data/utmsmall_cubic.tiff")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


def test_warp_3_ushort():

    ds = gdal.Open("data/utmsmall_cubic_ushort.vrt")
    ref_ds = gdal.Open("data/utmsmall_cubic.tiff")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


def test_warp_3_downsize():

    ds = gdal.Open("data/utmsmall_cubic_2.vrt")
    ref_ds = gdal.Open("data/utmsmall_cubic_2.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


def test_warp_3_float_downsize():

    ds = gdal.Open("data/utmsmall_cubic_2_float.vrt")
    ref_ds = gdal.Open("data/utmsmall_cubic_2.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


def test_warp_4():

    ds = gdal.Open("data/utmsmall_cubicspline.vrt")
    ref_ds = gdal.Open("data/utmsmall_cubicspline.tiff")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


def test_warp_4_short():

    ds = gdal.Open("data/utmsmall_cubicspline_short.vrt")
    ref_ds = gdal.Open("data/utmsmall_cubicspline.tiff")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


def test_warp_4_ushort():

    ds = gdal.Open("data/utmsmall_cubicspline_ushort.vrt")
    ref_ds = gdal.Open("data/utmsmall_cubicspline.tiff")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


def test_warp_4_downsize():

    ds = gdal.Open("data/utmsmall_cubicspline_2.vrt")
    ref_ds = gdal.Open("data/utmsmall_cubicspline_2.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


def test_warp_4_short_downsize():

    ds = gdal.Open("data/utmsmall_cubicspline_wt_short.vrt")
    ref_ds = gdal.Open("data/utmsmall_cubicspline_2.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


def test_warp_4_float_downsize():

    ds = gdal.Open("data/utmsmall_cubicspline_wt_float32.vrt")
    ref_ds = gdal.Open("data/utmsmall_cubicspline_2.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


def test_warp_5():

    ds = gdal.Open("data/utmsmall_lanczos.vrt")
    ref_ds = gdal.Open("data/utmsmall_lanczos.tiff")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


def test_warp_5_downsize():

    ds = gdal.Open("data/utmsmall_lanczos_2.vrt")
    ref_ds = gdal.Open("data/utmsmall_lanczos_2.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


def test_warp_lanczos_downsize_50_75():

    ds = gdal.Open("data/utmsmall_lanczos_50_75.vrt")
    ref_ds = gdal.Open("data/utmsmall_lanczos_50_75.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


# Downsampling


def test_warp_6():

    tst = gdaltest.GDALTest("VRT", "utmsmall_ds_near.vrt", 1, 4770)

    tst.testOpen()


def test_warp_7():

    tst = gdaltest.GDALTest("VRT", "utmsmall_ds_blinear.vrt", 1, 4755)

    tst.testOpen()


def test_warp_8():

    tst = gdaltest.GDALTest("VRT", "utmsmall_ds_cubic.vrt", 1, 4833)

    tst.testOpen()


def test_warp_9():

    ds = gdal.Open("data/utmsmall_ds_cubicspline.vrt")
    ref_ds = gdal.Open("data/utmsmall_ds_cubicspline.tiff")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


def test_warp_10():

    ds = gdal.Open("data/utmsmall_ds_lanczos.vrt")
    ref_ds = gdal.Open("data/utmsmall_ds_lanczos.tiff")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


def test_warp_11():

    tst = gdaltest.GDALTest("VRT", "rgbsmall_dstalpha.vrt", 4, 30658)

    tst.testOpen()


# Test warping an empty RGBA with bilinear resampling


def test_warp_12():

    tiff_drv = gdal.GetDriverByName("GTiff")
    ds = tiff_drv.Create("tmp/empty.tif", 20, 20, 4)
    ds.GetRasterBand(1).Fill(0)
    ds.GetRasterBand(2).Fill(0)
    ds.GetRasterBand(3).Fill(0)
    ds.GetRasterBand(4).Fill(0)
    ds = None

    # The alpha channel must be empty
    tst = gdaltest.GDALTest("VRT", "empty_rb.vrt", 4, 0)

    tst.testOpen()

    tiff_drv.Delete("tmp/empty.tif")


# Test warping an empty RGBA with cubic resampling


def test_warp_13():

    tiff_drv = gdal.GetDriverByName("GTiff")
    ds = tiff_drv.Create("tmp/empty.tif", 20, 20, 4)
    ds.GetRasterBand(1).Fill(0)
    ds.GetRasterBand(2).Fill(0)
    ds.GetRasterBand(3).Fill(0)
    ds.GetRasterBand(4).Fill(0)
    ds = None

    # The alpha channel must be empty
    tst = gdaltest.GDALTest("VRT", "empty_rc.vrt", 4, 0)

    tst.testOpen()

    tiff_drv.Delete("tmp/empty.tif")


# Test warping an empty RGBA with cubic spline resampling


def test_warp_14():

    tiff_drv = gdal.GetDriverByName("GTiff")
    ds = tiff_drv.Create("tmp/empty.tif", 20, 20, 4)
    ds.GetRasterBand(1).Fill(0)
    ds.GetRasterBand(2).Fill(0)
    ds.GetRasterBand(3).Fill(0)
    ds.GetRasterBand(4).Fill(0)
    ds = None

    # The alpha channel must be empty
    tst = gdaltest.GDALTest("VRT", "empty_rcs.vrt", 4, 0)

    tst.testOpen()

    tiff_drv.Delete("tmp/empty.tif")


# Test GWKNearestFloat with transparent source alpha band


def test_warp_15():

    tiff_drv = gdal.GetDriverByName("GTiff")
    ds = tiff_drv.Create("tmp/test.tif", 20, 20, 4)
    ds.GetRasterBand(1).Fill(0)
    ds.GetRasterBand(2).Fill(0)
    ds.GetRasterBand(3).Fill(0)
    ds.GetRasterBand(4).Fill(0)
    ds = None

    # The alpha channel must be empty
    tst = gdaltest.GDALTest("VRT", "test_nearest_float.vrt", 4, 0)

    tst.testOpen()

    tiff_drv.Delete("tmp/test.tif")


# Test GWKNearestFloat with opaque source alpha band


def test_warp_16():

    tiff_drv = gdal.GetDriverByName("GTiff")
    ds = tiff_drv.Create("tmp/test.tif", 20, 20, 4)
    ds.GetRasterBand(1).Fill(255)
    ds.GetRasterBand(2).Fill(0)
    ds.GetRasterBand(3).Fill(0)
    ds.GetRasterBand(4).Fill(255)
    ds = None

    # The alpha channel must be empty
    tst = gdaltest.GDALTest("VRT", "test_nearest_float.vrt", 4, 4921)

    tst.testOpen()

    tiff_drv.Delete("tmp/test.tif")


# Test GWKNearestShort with transparent source alpha band


def test_warp_17():

    tiff_drv = gdal.GetDriverByName("GTiff")
    ds = tiff_drv.Create("tmp/test.tif", 20, 20, 4)
    ds.GetRasterBand(1).Fill(0)
    ds.GetRasterBand(2).Fill(0)
    ds.GetRasterBand(3).Fill(0)
    ds.GetRasterBand(4).Fill(0)
    ds = None

    # The alpha channel must be empty
    tst = gdaltest.GDALTest("VRT", "test_nearest_short.vrt", 4, 0)

    tst.testOpen()

    tiff_drv.Delete("tmp/test.tif")


# Test GWKNearestShort with opaque source alpha band


def test_warp_18():

    tiff_drv = gdal.GetDriverByName("GTiff")
    ds = tiff_drv.Create("tmp/test.tif", 20, 20, 4)
    ds.GetRasterBand(1).Fill(255)
    ds.GetRasterBand(2).Fill(0)
    ds.GetRasterBand(3).Fill(0)
    ds.GetRasterBand(4).Fill(255)
    ds = None

    # The alpha channel must be empty
    tst = gdaltest.GDALTest("VRT", "test_nearest_short.vrt", 4, 4921)

    tst.testOpen()

    tiff_drv.Delete("tmp/test.tif")


# Test all data types and resampling methods for very small images
# to test edge behaviour
@pytest.mark.parametrize("size", [1, 2, 3, 7])
@pytest.mark.parametrize(
    "resampling_string",
    ["near", "bilinear", "cubic", "cubicspline", "lanczos", "average"],
)
@pytest.mark.parametrize(
    "datatype",
    [
        gdal.GDT_UInt8,
        gdal.GDT_Int16,
        gdal.GDT_CInt16,
        gdal.GDT_UInt16,
        gdal.GDT_Int32,
        gdal.GDT_CInt32,
        gdal.GDT_UInt32,
        gdal.GDT_Float32,
        gdal.GDT_CFloat32,
        gdal.GDT_Float64,
        gdal.GDT_CFloat64,
    ],
    ids=gdal.GetDataTypeName,
)
def test_warp_19(tmpdir, size, datatype, resampling_string):

    test_file = str(tmpdir.join("test.tif"))
    warp_file = str(tmpdir.join("testwarp.tif"))

    tiff_drv = gdal.GetDriverByName("GTiff")
    ds = tiff_drv.Create(test_file, size, size, 1, datatype)
    ds.SetGeoTransform((10, 5, 0, 30, 0, -5))
    ds.SetProjection(
        'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]'
    )
    ds.GetRasterBand(1).Fill(10.1, 20.1)
    ds = None

    gdal.Warp(warp_file, test_file, options=f"-r {resampling_string}")

    ref_ds = gdal.Open(test_file)
    ds = gdal.Open(warp_file)
    checksum = ds.GetRasterBand(1).Checksum()
    checksum_ref = ref_ds.GetRasterBand(1).Checksum()
    ds = None
    ref_ds = None

    tiff_drv.Delete(warp_file)

    assert checksum == checksum_ref

    tiff_drv.Delete(test_file)


# Test fix for #2724 (initialization of destination area to nodata in warped VRT)
def test_warp_20():

    tst = gdaltest.GDALTest("VRT", "white_nodata.vrt", 1, 1705)

    tst.testOpen()


###############################################################################
# Test overviews on warped VRT files


def test_warp_21():

    shutil.copy("data/utmsmall_near.vrt", "tmp/warp_21.vrt")

    ds = gdal.Open("tmp/warp_21.vrt", gdal.GA_Update)
    ds.BuildOverviews("NEAR", overviewlist=[2])
    ds = None

    ds = gdal.Open("tmp/warp_21.vrt")
    if ds.GetRasterBand(1).GetOverviewCount() != 1:
        pytest.skip()

    ds.GetRasterBand(1).GetOverview(0).Checksum()

    ds = None

    os.remove("tmp/warp_21.vrt")


###############################################################################
# Test warping with datasets which are "bigger" than the wm parameter.
# Would have detected issue of #3458


@pytest.mark.parametrize(
    "option1", ["", "-wo OPTIMIZE_SIZE=TRUE"], ids=["default", "optimizeSize"]
)
@pytest.mark.parametrize(
    "option2",
    [
        "",
        "-co TILED=YES",
        "-co TILED=YES -co BLOCKXSIZE=16 -co BLOCKYSIZE=16",
    ],
    ids=["default", "tiled", "tiled16"],
)
def test_warp_22(tmpdir, option1, option2):

    src = str(tmpdir.join("warp_22_src.tif"))
    dst = str(tmpdir.join("warp_22_dst.tif"))

    # Generate source image with non uniform data
    w = 1001
    h = 1001
    ds = gdal.GetDriverByName("GTiff").Create(src, w, h, 1)
    ds.SetGeoTransform([2, 0.01, 0, 49, 0, -0.01])
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    ds.SetProjection(sr.ExportToWkt())

    for j in range(h):
        line = ""
        for i in range(w):
            line = line + "%c" % int((i * i + h * j / (i + 1)) % 256)
        ds.GetRasterBand(1).WriteRaster(0, j, w, 1, line)

    expected_cs = ds.GetRasterBand(1).Checksum()
    ds = None

    # -wm should not be greater than 2 * w * h. Let's put it at its minimum
    # value.
    gdal.Warp(dst, src, options=f"-wm 100000 {option1} {option2}")
    ds = gdal.Open(dst)
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == expected_cs
    ds = None


###############################################################################
# Test warping with datasets where some RasterIO() requests involve nBufXSize == 0 (#3582)


def test_warp_23():

    gcp1 = gdal.GCP()
    gcp1.GCPPixel = 3213
    gcp1.GCPLine = 2225
    gcp1.GCPX = -88.834495
    gcp1.GCPY = 29.979959

    gcp2 = gdal.GCP()
    gcp2.GCPPixel = 2804
    gcp2.GCPLine = 2236
    gcp2.GCPX = -88.836706
    gcp2.GCPY = 29.979516

    gcp3 = gdal.GCP()
    gcp3.GCPPixel = 3157
    gcp3.GCPLine = 4344
    gcp3.GCPX = -88.833389
    gcp3.GCPY = 29.969519

    gcp4 = gdal.GCP()
    gcp4.GCPPixel = 3768
    gcp4.GCPLine = 5247
    gcp4.GCPX = -88.830168
    gcp4.GCPY = 29.964958

    gcp5 = gdal.GCP()
    gcp5.GCPPixel = 2697
    gcp5.GCPLine = 9225
    gcp5.GCPX = -88.83516
    gcp5.GCPY = 29.945386

    gcp6 = gdal.GCP()
    gcp6.GCPPixel = 4087
    gcp6.GCPLine = 12360
    gcp6.GCPX = -88.827899
    gcp6.GCPY = 29.929807

    gcp7 = gdal.GCP()
    gcp7.GCPPixel = 4629
    gcp7.GCPLine = 11258
    gcp7.GCPX = -88.825102
    gcp7.GCPY = 29.93527

    gcp8 = gdal.GCP()
    gcp8.GCPPixel = 4480
    gcp8.GCPLine = 7602
    gcp8.GCPX = -88.826733
    gcp8.GCPY = 29.95304

    gcps = [gcp1, gcp2, gcp3, gcp4, gcp5, gcp6, gcp7, gcp8]
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)

    ds = gdal.GetDriverByName("GTiff").Create(
        "tmp/test3582.tif", 70, 170, 4, options=["SPARSE_OK=YES"]
    )
    for i, gcp in enumerate(gcps):
        gcps[i].GCPPixel = gcp.GCPPixel / 10
        gcps[i].GCPLine = gcp.GCPLine / 10
    ds.SetGCPs(gcps, sr.ExportToWkt())
    ds = None

    assert gdal.Warp("", "tmp/test3582.tif", format="MEM") is not None

    os.remove("tmp/test3582.tif")


###############################################################################
# Test fix for #3658 (numerical imprecision with Ubuntu 8.10 GCC 4.4.3 -O2 leading to upper
# left pixel being not set in GWKBilinearResample() case)


def test_warp_24():

    ds_ref = gdal.Open("data/test3658.tif")
    cs_ref = ds_ref.GetRasterBand(1).Checksum()
    ds = gdal.Warp("", ds_ref, options="-srcnodata none -of MEM -r bilinear")
    cs = ds.GetRasterBand(1).Checksum()

    assert cs == cs_ref, "did not get expected checksum"


###############################################################################
# Test -refine_gcps (#4143)


def test_warp_25():

    ds = gdal.Open("data/refine_gcps.vrt")
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    assert cs == 4672, "did not get expected checksum"


###############################################################################
# Test serializing and deserializing TPS transformer


def test_warp_26():

    gdal.Translate(
        "tmp/warp_25_gcp.vrt",
        "../gcore/data/byte.tif",
        options="-of VRT -gcp 0 0 0 20 -gcp 0 20 0  0 "
        "-gcp 20 0 20 20 -gcp 20 20 20 0",
    )
    gdal.Warp("tmp/warp_25_warp.vrt", "tmp/warp_25_gcp.vrt", options="-of VRT -tps")

    ds = gdal.Open("tmp/warp_25_warp.vrt")
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    assert cs == 4672, "did not get expected checksum"

    os.unlink("tmp/warp_25_gcp.vrt")
    os.unlink("tmp/warp_25_warp.vrt")


###############################################################################
# Pure Python reprojection example. Nothing particular, just make use of existing
# API.


def warp_27_progress_callback(pct, message, user_data):
    # pylint: disable=unused-argument
    return 1  # 1 to continue, 0 to stop


def test_warp_27():

    # Open source dataset
    src_ds = gdal.Open("../gcore/data/byte.tif")

    # Desfine target SRS
    dst_srs = osr.SpatialReference()
    dst_srs.ImportFromEPSG(4326)
    dst_wkt = dst_srs.ExportToWkt()

    error_threshold = 0.125  # error threshold --> use same value as in gdalwarp
    resampling = gdal.GRA_Bilinear

    # Call AutoCreateWarpedVRT() to fetch default values for target raster dimensions and geotransform
    tmp_ds = gdal.AutoCreateWarpedVRT(
        src_ds,
        None,  # src_wkt : left to default value --> will use the one from source \
        dst_wkt,
        resampling,
        error_threshold,
    )
    dst_xsize = tmp_ds.RasterXSize
    dst_ysize = tmp_ds.RasterYSize
    dst_gt = tmp_ds.GetGeoTransform()
    tmp_ds = None

    # Now create the true target dataset
    dst_ds = gdal.GetDriverByName("GTiff").Create(
        "tmp/warp_27.tif", dst_xsize, dst_ysize, src_ds.RasterCount
    )
    dst_ds.SetProjection(dst_wkt)
    dst_ds.SetGeoTransform(dst_gt)

    # And run the reprojection

    cbk = warp_27_progress_callback
    cbk_user_data = None  # value for last parameter of above warp_27_progress_callback

    gdal.ReprojectImage(
        src_ds,
        dst_ds,
        None,  # src_wkt : left to default value --> will use the one from source \
        None,  # dst_wkt : left to default value --> will use the one from destination \
        resampling,
        0,  # WarpMemoryLimit : left to default value \
        error_threshold,
        cbk,  # Progress callback : could be left to None or unspecified for silent progress
        cbk_user_data,
    )  # Progress callback user data

    # Done !
    dst_ds = None

    # Check that we have the same result as produced by 'gdalwarp -rb -t_srs EPSG:4326 ../gcore/data/byte.tif tmp/warp_27.tif'
    ds = gdal.Open("tmp/warp_27.tif")
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    ds = gdal.Warp(
        "tmp/warp_27_ref.tif", "../gcore/data/byte.tif", options="-rb -t_srs EPSG:4326"
    )
    ref_cs = ds.GetRasterBand(1).Checksum()
    ds = None

    assert cs == ref_cs

    gdal.Unlink("tmp/warp_27.tif")
    gdal.Unlink("tmp/warp_27_ref.tif")


###############################################################################
# Test reading a VRT with a destination alpha band, but no explicit
# INIT_DEST setting


def test_warp_28():

    ds = gdal.Open("data/utm_alpha_noinit.vrt")
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    assert not (cs1 == 0 or cs2 == 0), "bad checksum"
    ds = None


###############################################################################
# Test multi-thread computations


def test_warp_29():

    ds = gdal.Open("data/white_nodata.vrt")
    cs_monothread = ds.GetRasterBand(1).Checksum()
    ds = None

    with gdal.config_options(
        {"GDAL_NUM_THREADS": "ALL_CPUS", "WARP_THREAD_CHUNK_SIZE": "0"}
    ):
        ds = gdal.Open("data/white_nodata.vrt")
        cs_multithread = ds.GetRasterBand(1).Checksum()
        ds = None

    assert cs_monothread == cs_multithread

    with gdal.config_options({"GDAL_NUM_THREADS": "2", "WARP_THREAD_CHUNK_SIZE": "0"}):
        ds = gdal.Open("data/white_nodata.vrt")
        cs_multithread = ds.GetRasterBand(1).Checksum()
        ds = None

    assert cs_monothread == cs_multithread

    src_ds = gdal.Open("../gcore/data/byte.tif")

    ds = gdal.Open("data/byte_gcp.vrt")
    with gdal.config_options({"GDAL_NUM_THREADS": "2", "WARP_THREAD_CHUNK_SIZE": "0"}):
        got_cs = ds.GetRasterBand(1).Checksum()
    ds = None

    assert got_cs == src_ds.GetRasterBand(1).Checksum()

    ds = gdal.Open("data/byte_tps.vrt")
    with gdal.config_options({"GDAL_NUM_THREADS": "2", "WARP_THREAD_CHUNK_SIZE": "0"}):
        got_cs = ds.GetRasterBand(1).Checksum()
    ds = None

    assert got_cs == src_ds.GetRasterBand(1).Checksum()

    src_ds = None


###############################################################################
# Test warping interruption


def warp_30_progress_callback(pct, message, user_data):
    # pylint: disable=unused-argument
    return bool(pct <= 0.2)


def test_warp_30():

    # Open source dataset
    src_ds = gdal.Open("../gcore/data/byte.tif")

    # Desfine target SRS
    dst_srs = osr.SpatialReference()
    dst_srs.ImportFromEPSG(4326)
    dst_wkt = dst_srs.ExportToWkt()

    error_threshold = 0.125  # error threshold --> use same value as in gdalwarp
    resampling = gdal.GRA_Bilinear

    # Call AutoCreateWarpedVRT() to fetch default values for target raster dimensions and geotransform
    tmp_ds = gdal.AutoCreateWarpedVRT(
        src_ds,
        None,  # src_wkt : left to default value --> will use the one from source \
        dst_wkt,
        resampling,
        error_threshold,
    )
    dst_xsize = tmp_ds.RasterXSize
    dst_ysize = tmp_ds.RasterYSize
    dst_gt = tmp_ds.GetGeoTransform()
    tmp_ds = None

    # Now create the true target dataset
    dst_ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/warp_30.tif", dst_xsize, dst_ysize, src_ds.RasterCount
    )
    dst_ds.SetProjection(dst_wkt)
    dst_ds.SetGeoTransform(dst_gt)

    # And run the reprojection

    cbk = warp_30_progress_callback
    cbk_user_data = None  # value for last parameter of above warp_27_progress_callback

    with pytest.raises(Exception):
        gdal.ReprojectImage(
            src_ds,
            dst_ds,
            None,  # src_wkt : left to default value --> will use the one from source \
            None,  # dst_wkt : left to default value --> will use the one from destination \
            resampling,
            0,  # WarpMemoryLimit : left to default value \
            error_threshold,
            cbk,  # Progress callback : could be left to None or unspecified for silent progress
            cbk_user_data,
        )  # Progress callback user data

    with gdal.config_option("GDAL_NUM_THREADS", "2"), pytest.raises(Exception):
        gdal.ReprojectImage(
            src_ds,
            dst_ds,
            None,  # src_wkt : left to default value --> will use the one from source \
            None,  # dst_wkt : left to default value --> will use the one from destination \
            resampling,
            0,  # WarpMemoryLimit : left to default value \
            error_threshold,
            cbk,  # Progress callback : could be left to None or unspecified for silent progress
            cbk_user_data,
        )  # Progress callback user data

    gdal.Unlink("/vsimem/warp_30.tif")


# Average (Byte)


def test_warp_31():

    ds = gdal.Open("data/utmsmall_average.vrt")
    ref_ds = gdal.Open("data/utmsmall_average.tiff")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


# Average (Float)


def test_warp_32():

    ds = gdal.Open("data/utmsmall_average_float.vrt")
    ref_ds = gdal.Open("data/utmsmall_average_float.tiff")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


# Mode (Byte)


def test_warp_33():

    ds = gdal.Open("data/utmsmall_mode.vrt")
    ref_ds = gdal.Open("data/utmsmall_mode.tiff")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


# Mode (Int16)


def test_warp_34():

    ds = gdal.Open("data/utmsmall_mode_int16.vrt")
    ref_ds = gdal.Open("data/utmsmall_mode_int16.tiff")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


# Mode (Int16 - signed with negative values)


def test_warp_35():

    ds = gdal.Open("data/utmsmall-int16-neg_mode.vrt")
    ref_ds = gdal.Open("data/utmsmall-int16-neg_mode.tiff")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


# Mode (Int32) - this uses algorithm 2 (inefficient)


def test_warp_36():

    ds = gdal.Open("data/utmsmall_mode_int32.vrt")
    ref_ds = gdal.Open("data/utmsmall_mode_int32.tiff")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


###############################################################################
# Test a few error cases


def test_warp_37():

    # Open source dataset
    src_ds = gdal.Open("../gcore/data/byte.tif")

    # Dummy proj.4 method
    sr = osr.SpatialReference()
    sr.ImportFromWkt(
        """PROJCS["unnamed",
    GEOGCS["unnamed ellipse",
        DATUM["unknown",
            SPHEROID["unnamed",6378137,298.257223563]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["custom_proj4"],
    UNIT["Meter",1],
    EXTENSION["PROJ4","+proj=dummy_method +units=m +wktext"]]"""
    )
    dst_wkt = sr.ExportToWkt()

    with pytest.raises(Exception):
        gdal.AutoCreateWarpedVRT(src_ds, None, dst_wkt)


###############################################################################
# Test a warp with GCPs on the *destination* image.


def test_warp_38():

    # Create an output file with GCPs.
    out_file = "tmp/warp_38.tif"
    ds = gdal.GetDriverByName("GTiff").Create(out_file, 50, 50, 3)

    gcp_list = [
        gdal.GCP(397000, 5642000, 0, 0, 0),
        gdal.GCP(397000, 5641990, 0, 0, 50),
        gdal.GCP(397010, 5642000, 0, 50, 0),
        gdal.GCP(397010, 5641990, 0, 50, 50),
        gdal.GCP(397005, 5641995, 0, 25, 25),
    ]
    ds.SetGCPs(gcp_list, gdaltest.user_srs_to_wkt("EPSG:32632"))
    ds = None

    gdal.Warp(
        out_file,
        "data/test3658.tif",
        options="-srcnodata none -to DST_METHOD=GCP_POLYNOMIAL",
    )

    ds = gdal.Open(out_file)
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    # Should exactly match the source file.
    exp_cs = 30546
    assert cs == exp_cs

    os.unlink(out_file)


###############################################################################
# Test a warp with GCPs for TPS on the *destination* image.


def test_warp_39():

    # Create an output file with GCPs.
    out_file = "tmp/warp_39.tif"
    ds = gdal.GetDriverByName("GTiff").Create(out_file, 50, 50, 3)

    gcp_list = [
        gdal.GCP(397000, 5642000, 0, 0, 0),
        gdal.GCP(397000, 5641990, 0, 0, 50),
        gdal.GCP(397010, 5642000, 0, 50, 0),
        gdal.GCP(397010, 5641990, 0, 50, 50),
        gdal.GCP(397005, 5641995, 0, 25, 25),
    ]
    ds.SetGCPs(gcp_list, gdaltest.user_srs_to_wkt("EPSG:32632"))
    ds = None

    gdal.Warp(
        out_file, "data/test3658.tif", options="-srcnodata none -to DST_METHOD=GCP_TPS"
    )

    ds = gdal.Open(out_file)
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    # Should exactly match the source file.
    exp_cs = 30546
    assert cs == exp_cs

    os.unlink(out_file)


###############################################################################
# Test a warp with GCPs for homography on the *destination* image.


def test_warp_homography():

    # Create an output file with GCPs.
    out_file = "tmp/warp_homography.tif"
    ds = gdal.GetDriverByName("GTiff").Create(out_file, 50, 50, 3)

    gcp_list = [
        gdal.GCP(397000, 5642000, 0, 0, 0),
        gdal.GCP(397000, 5641990, 0, 0, 50),
        gdal.GCP(397010, 5642000, 0, 50, 0),
        gdal.GCP(397010, 5641990, 0, 50, 50),
        gdal.GCP(397005, 5641995, 0, 25, 25),
    ]
    ds.SetGCPs(gcp_list, gdaltest.user_srs_to_wkt("EPSG:32632"))
    ds = None

    gdal.Warp(
        out_file,
        "data/test3658.tif",
        options="-srcnodata none -to DST_METHOD=GCP_HOMOGRAPHY",
    )

    ds = gdal.Open(out_file)
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    # Should exactly match the source file.
    exp_cs = 30546
    assert cs == exp_cs

    os.unlink(out_file)


###############################################################################
# test average (#5311)


def test_warp_40():

    ds = gdal.Open("data/2by2.vrt")
    ref_ds = gdal.Open("data/2by2.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


###############################################################################
# test weighted average


def test_warp_weighted_average():

    ds = gdal.Open("data/3by3_average.vrt")
    ref_ds = gdal.Open("data/3by3_average.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


###############################################################################
# test weighted mode


@pytest.mark.parametrize(
    "dtype", (gdal.GDT_Int16, gdal.GDT_Int32), ids=gdal.GetDataTypeName
)
def test_warp_weighted_mode(dtype):

    gdaltest.importorskip_gdal_array()
    np = pytest.importorskip("numpy")

    with gdal.GetDriverByName("MEM").Create("", 3, 3, eType=dtype) as src_ds:
        src_ds.SetGeoTransform([0, 1, 0, 3, 0, -1])
        src_ds.WriteArray(np.array([[1, 1, 1], [-1, -1, -1], [3, 3, 3]]))

        dst_ds = gdal.Warp(
            "",
            src_ds,
            format="MEM",
            resampleAlg="mode",
            width=1,
            height=1,
            outputBounds=(0.5, 0.5, 2.5, 2.5),
        )

    result = dst_ds.ReadAsArray()[0, 0]

    assert result == -1


###############################################################################
# test weighted average, with src offset (fix for #2665)


def test_warp_weighted_average_with_srcoffset():

    ds = gdal.Open("data/3by3_average_with_srcoffset.vrt")
    val = struct.unpack("d", ds.ReadRaster(0, 0, 1, 1))[0]
    assert val == pytest.approx(8.5, abs=1e-5)


###############################################################################
# test sum


def test_warp_sum():

    ds = gdal.Open("data/3by3_sum.vrt")
    ref_ds = gdal.Open("data/3by3_sum.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


###############################################################################
# test GDALSuggestedWarpOutput (#5693)


def test_warp_41():

    src_ds = gdal.Open(
        """<VRTDataset rasterXSize="67108864" rasterYSize="67108864">
  <GeoTransform> -2.0037508340000000e+07,  5.9716428339481353e-01,  0.0000000000000000e+00,  2.0037508340000000e+07,  0.0000000000000000e+00, -5.9716428339481353e-01</GeoTransform>
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="0">dummy</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="67108864" RasterYSize="67108864" DataType="Byte" BlockXSize="256" BlockYSize="256" />
      <SrcRect xOff="0" yOff="0" xSize="67108864" ySize="67108864" />
      <DstRect xOff="0" yOff="0" xSize="67108864" ySize="67108864" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>"""
    )

    vrt_ds = gdal.AutoCreateWarpedVRT(
        src_ds, None, None, gdal.GRA_NearestNeighbour, 0.3
    )
    assert vrt_ds.RasterXSize == src_ds.RasterXSize
    assert vrt_ds.RasterYSize == src_ds.RasterYSize
    src_gt = src_ds.GetGeoTransform()
    vrt_gt = vrt_ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(vrt_gt[i], abs=1e-5)


def test_warp_suggestedwarp_output_invalid_input():
    with pytest.raises(Exception):
        gdal.SuggestedWarpOutput(None, {"DST_SRS": "EPSG:4326"})


###############################################################################

# Maximum


def test_warp_42():

    ds = gdal.Open("data/utmsmall_max.vrt")
    ref_ds = gdal.Open("data/utmsmall_max.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


# Minimum


def test_warp_43():

    ds = gdal.Open("data/utmsmall_min.vrt")
    ref_ds = gdal.Open("data/utmsmall_min.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


# Median


def test_warp_44():

    ds = gdal.Open("data/utmsmall_med.vrt")
    ref_ds = gdal.Open("data/utmsmall_med.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


# Quartile 1


def test_warp_45():

    ds = gdal.Open("data/utmsmall_Q1.vrt")
    ref_ds = gdal.Open("data/utmsmall_Q1.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


# Quartile 3


def test_warp_46():

    ds = gdal.Open("data/utmsmall_Q3.vrt")
    ref_ds = gdal.Open("data/utmsmall_Q3.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


# Maximum (Int16 - signed with negative values)


def test_warp_47():

    ds = gdal.Open("data/utmsmall-int16-neg_max.vrt")
    ref_ds = gdal.Open("data/utmsmall-int16-neg_max.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


# Minimum (Int16 - signed with negative values)


def test_warp_48():

    ds = gdal.Open("data/utmsmall-int16-neg_min.vrt")
    ref_ds = gdal.Open("data/utmsmall-int16-neg_min.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


# Median (Int16 - signed with negative values)


def test_warp_49():

    ds = gdal.Open("data/utmsmall-int16-neg_med.vrt")
    ref_ds = gdal.Open("data/utmsmall-int16-neg_med.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


# Quartile 1 (Int16 - signed with negative values)


def test_warp_50():

    ds = gdal.Open("data/utmsmall-int16-neg_Q1.vrt")
    ref_ds = gdal.Open("data/utmsmall-int16-neg_Q1.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


# Quartile 3 (Int16 - signed with negative values)


def test_warp_51():

    ds = gdal.Open("data/utmsmall-int16-neg_Q3.vrt")
    ref_ds = gdal.Open("data/utmsmall-int16-neg_Q3.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None

    assert maxdiff <= 1, "Image too different from reference"


###############################################################################
# Test fix for #6182


def test_warp_52():

    src_ds = gdal.GetDriverByName("MEM").Create("", 4096, 4096, 3, gdal.GDT_UInt16)
    rpc = [
        "HEIGHT_OFF=1466.05894327379",
        "HEIGHT_SCALE=144.837606185489",
        "LAT_OFF=38.9266809014185",
        "LAT_SCALE=-0.108324009570885",
        "LINE_DEN_COEFF=1 -0.000392404256440504 -0.0027925489381758 0.000501819414812054 0.00216726134806561 -0.00185617059201599 0.000183834173326118 -0.00290342803717354 -0.00207181007131322 -0.000900223247894285 -0.00132518281680544 0.00165598132063197 0.00681015244696305 0.000547865679631528 0.00516214646283021 0.00795287690785699 -0.000705040639059332 -0.00254360623317078 -0.000291154885056484 0.00070943440010757",
        "LINE_NUM_COEFF=-0.000951099635749339 1.41709976082781 -0.939591985038569 -0.00186609235173885 0.00196881101098923 0.00361741523740639 -0.00282449434932066 0.0115361898794214 -0.00276027843825304 9.37913944402154e-05 -0.00160950221565737 0.00754053609977256 0.00461831968713819 0.00274991122620312 0.000689605203796422 -0.0042482778732957 -0.000123966494595151 0.00307976709897974 -0.000563274426174409 0.00049981716767074",
        "LINE_OFF=2199.50159296339",
        "LINE_SCALE=2195.852519621",
        "LONG_OFF=76.0381768085136",
        "LONG_SCALE=0.130066683772651",
        "SAMP_DEN_COEFF=1 -0.000632078047521022 -0.000544107268758971 0.000172438016778527 -0.00206391734870399 -0.00204445747536872 -0.000715754551621987 -0.00195545265530244 -0.00168532972557267 -0.00114709980708329 -0.00699131177532728 0.0038551339822296 0.00283631282133365 -0.00436885468926666 -0.00381335885955994 0.0018742043611019 -0.0027263909314293 -0.00237054119407013 0.00246374716379501 -0.00121074576302219",
        "SAMP_NUM_COEFF=0.00249293151551852 -0.581492592442025 -1.00947448466175 0.00121597346320039 -0.00552825219917498 -0.00194683170765094 -0.00166012459012905 -0.00338315804553888 -0.00152062885009498 -0.000214562164393127 -0.00219914905535387 -0.000662800177832777 -0.00118644828432841 -0.00180061222825708 -0.00364756875260519 -0.00287273485650089 -0.000540077934728493 -0.00166800463003749 0.000201057249109451 -8.49620129025469e-05",
        "SAMP_OFF=3300.34602166792",
        "SAMP_SCALE=3297.51222987611",
    ]
    src_ds.SetMetadata(rpc, "RPC")

    import time

    start = time.time()

    out_ds = gdal.Warp(
        "",
        src_ds,
        format="MEM",
        outputBounds=[8453323.83095, 4676723.13796, 8472891.71018, 4696291.0172],
        xRes=4.77731426716,
        yRes=4.77731426716,
        dstSRS="EPSG:3857",
        warpOptions=["SKIP_NOSOURCE=YES", "DST_ALPHA_MAX=255"],
        transformerOptions=["RPC_DEM=data/warp_52_dem.tif"],
        dstAlpha=True,
        errorThreshold=0,
        resampleAlg=gdal.GRA_Cubic,
    )

    end = time.time()
    assert end - start <= 10, "processing time was way too long"

    cs = out_ds.GetRasterBand(4).Checksum()
    assert cs == 3177


###############################################################################
# Test Grey+Alpha


@pytest.mark.parametrize("typestr", ("Byte", "UInt16", "Int16"))
@pytest.mark.parametrize(
    "option", ("-wo USE_GENERAL_CASE=TRUE", ""), ids=["generalCase", "default"]
)
@pytest.mark.parametrize(
    "alg_name, expected_cs",
    (
        pytest.param("near", [1192], id="near"),
        pytest.param("cubic", [1061], id="cubic"),
        pytest.param("cubicspline", [1252], id="cubicspline"),
        pytest.param("bilinear", [1206, 1204], id="bilinear"),
    ),
)
def test_warp_53(typestr, option, alg_name, expected_cs):

    src_ds = gdal.Translate(
        "",
        "../gcore/data/byte.tif",
        options=f"-a_srs EPSG:32611 -of MEM -b 1 -b 1 -ot {typestr}",
    )
    src_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)
    src_ds.GetRasterBand(2).Fill(255)
    zero = struct.pack("B" * 1, 0)
    src_ds.GetRasterBand(2).WriteRaster(10, 10, 1, 1, zero, buf_type=gdal.GDT_UInt8)

    dst_ds = gdal.Translate(
        "", src_ds, options="-outsize 10 10 -of MEM -a_srs EPSG:32611"
    )

    dst_ds.GetRasterBand(1).Fill(0)
    dst_ds.GetRasterBand(2).Fill(0)
    gdal.Warp(dst_ds, src_ds, options=f"-r {alg_name} {option}")
    cs1 = dst_ds.GetRasterBand(1).Checksum()
    cs2 = dst_ds.GetRasterBand(2).Checksum()
    assert cs1 in expected_cs
    assert cs2 == 1218


###############################################################################
# Test Alpha on UInt16/Int16


@pytest.mark.parametrize("use_optim", ["YES", "NO"])
def test_warp_54(use_optim):

    # UInt16
    src_ds = gdal.Translate(
        "",
        "../gcore/data/stefan_full_rgba.tif",
        options="-of MEM -scale 0 255 0 65535 -ot UInt16 -a_ullr -162 150 0 0",
    )
    with gdaltest.config_option("GDAL_WARP_USE_TRANSLATION_OPTIM", use_optim):
        dst_ds = gdal.Warp("", src_ds, format="MEM")
    for i in range(4):
        expected_cs = src_ds.GetRasterBand(i + 1).Checksum()
        got_cs = dst_ds.GetRasterBand(i + 1).Checksum()
        assert expected_cs == got_cs, i

    # Int16
    src_ds = gdal.Translate(
        "",
        "../gcore/data/stefan_full_rgba.tif",
        options="-of MEM -scale 0 255 0 32767 -ot Int16 -a_ullr -162 150 0 0",
    )
    with gdaltest.config_option("GDAL_WARP_USE_TRANSLATION_OPTIM", use_optim):
        dst_ds = gdal.Warp("", src_ds, format="MEM")
    for i in range(4):
        expected_cs = src_ds.GetRasterBand(i + 1).Checksum()
        got_cs = dst_ds.GetRasterBand(i + 1).Checksum()
        assert expected_cs == got_cs, i

    # Test NBITS
    src_ds = gdal.Translate(
        "",
        "../gcore/data/stefan_full_rgba.tif",
        options="-of MEM -scale 0 255 0 32767 -ot UInt16 -a_ullr -162 150 0 0",
    )
    for i in range(4):
        src_ds.GetRasterBand(i + 1).SetMetadataItem("NBITS", "15", "IMAGE_STRUCTURE")
    with gdaltest.config_option("GDAL_WARP_USE_TRANSLATION_OPTIM", use_optim):
        dst_ds = gdal.Warp("/vsimem/warp_54.tif", src_ds, options="-co NBITS=15")
    for i in range(4):
        expected_cs = src_ds.GetRasterBand(i + 1).Checksum()
        got_cs = dst_ds.GetRasterBand(i + 1).Checksum()
        assert expected_cs == got_cs, i
    dst_ds = None

    gdal.Unlink("/vsimem/warp_54.tif")


###############################################################################
# Test warped VRT with source overview, target GT != GenImgProjection target GT
# and subsampling (#6972)


@pytest.mark.require_driver("PNG")  # data/warpedvrt_with_ovr.vrt uses a PNG
def test_warp_55():

    ds = gdal.Open("data/warpedvrt_with_ovr.vrt")
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 51966
    ds = None


###############################################################################
# Test bilinear interpolation when warping into same coordinate system (and
# same size). This test crops a single pixel out of a 3-by-3 image.


@pytest.mark.parametrize("use_optim", ["YES", "NO"])
def test_warp_56(use_optim):

    gdaltest.importorskip_gdal_array()
    numpy = pytest.importorskip("numpy")

    pix_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 3)
    src_ds.GetRasterBand(1).WriteArray(numpy.array([[0, 0, 0], [0, 0, 0], [0, 0, 100]]))
    src_ds.SetGeoTransform([1, 1, 0, 1, 0, 1])

    for off in numpy.linspace(0, 2, 21):
        pix_ds.SetGeoTransform([off + 1, 1, 0, off + 1, 0, 1])
        with gdaltest.config_option("GDAL_WARP_USE_TRANSLATION_OPTIM", use_optim):
            gdal.Warp(pix_ds, src_ds, resampleAlg="bilinear")

        exp = 0 if off < 1 else 100 * (off - 1) ** 2
        warped = pix_ds.GetRasterBand(1).ReadAsArray()[0, 0]
        assert warped == pytest.approx(
            exp, abs=0.6
        ), "offset: {}, expected: {:.0f}, got: {}".format(off, exp, warped)


###############################################################################
# Test bugfix for #1656


def test_warp_nearest_real_nodata_multiple_band():

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 2, gdal.GDT_Float64)
    src_ds.GetRasterBand(1).SetNoDataValue(65535)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 1, 1, struct.pack("d", 65535))
    src_ds.GetRasterBand(2).SetNoDataValue(65535)
    src_ds.GetRasterBand(2).WriteRaster(0, 0, 1, 1, struct.pack("d", 65535))
    src_ds.SetGeoTransform([1, 1, 0, 1, 0, 1])
    out_ds = gdal.Warp("", src_ds, options="-of MEM")
    assert struct.unpack("d" * 4, out_ds.ReadRaster()) == struct.unpack(
        "d" * 4, src_ds.ReadRaster()
    )


###############################################################################
# Test bugfix for #2365


def test_warp_med_out_of_bounds_src_pixels():

    ds = gdal.Open("data/test_bug_2365_wraped_med.vrt")
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 0
    ds = None


###############################################################################
# Test fix for #2460


def test_warp_rpc_source_has_geotransform():

    out_ds = gdal.Warp(
        "",
        "data/test_rpc_with_gt_bug_2460.tif",
        format="MEM",
        transformerOptions=["METHOD=RPC", "RPC_HEIGHT=1118"],
    )
    cs = out_ds.GetRasterBand(1).Checksum()
    assert cs == 60397


###############################################################################
# Test RMS resampling


def test_warp_ds_rms():

    tst = gdaltest.GDALTest("VRT", "utmsmall_ds_rms.vrt", 1, 4926)

    tst.testOpen()


def test_warp_rms_1():

    tst = gdaltest.GDALTest("VRT", "utmsmall_rms_float.vrt", 1, 29819)

    tst.testOpen()


def test_warp_rms_2():

    ds = gdal.Open("data/utmsmall_rms.vrt")
    # 29818 on non-Intel archs
    assert ds.GetRasterBand(1).Checksum() in (
        29818,
        29819,
        29828,  # Intel(R) oneAPI DPC++/C++ Compiler 2022.1.0
    )


@pytest.mark.parametrize("tie_strategy", ("FIRST", "MIN", "MAX", "HOPE"))
@pytest.mark.parametrize("dtype", (gdal.GDT_Int16, gdal.GDT_Int32))
def test_warp_mode_ties(tie_strategy, dtype):

    gdaltest.importorskip_gdal_array()
    numpy = pytest.importorskip("numpy")

    # 1 and 5 are tied for the mode; 1 encountered first
    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 3, 1, dtype)
    src_ds.SetGeoTransform([1, 1, 0, 1, 0, 1])
    src_ds.GetRasterBand(1).WriteArray(numpy.array([[1, 1, 1], [2, 3, 4], [5, 5, 5]]))

    with gdaltest.disable_exceptions(), gdal.quiet_errors():
        gdal.ErrorReset()
        out_ds = gdal.Warp(
            "",
            src_ds,
            format="MEM",
            resampleAlg="mode",
            xRes=3,
            yRes=3,
            warpOptions={"MODE_TIES": tie_strategy},
        )

    if tie_strategy == "HOPE":
        assert (
            gdal.GetLastErrorMsg()
            == "'HOPE' is an unexpected value for MODE_TIES option of type string-select."
        )
        assert out_ds is None
        return

    result = out_ds.GetRasterBand(1).ReadAsArray()[0, 0]

    if tie_strategy in ("FIRST", "MIN"):
        assert result == 1
    else:
        assert result == 5

    # 1 and 5 are tied for the mode; 5 encountered first
    src_ds.GetRasterBand(1).WriteArray(numpy.array([[1, 5, 1], [2, 5, 4], [5, 1, 0]]))
    out_ds = gdal.Warp(
        "",
        src_ds,
        format="MEM",
        resampleAlg="mode",
        xRes=3,
        yRes=3,
        warpOptions={"MODE_TIES": tie_strategy},
    )

    result = out_ds.GetRasterBand(1).ReadAsArray()[0, 0]

    if tie_strategy in ("FIRST", "MAX"):
        assert result == 5
    else:
        assert result == 1


###############################################################################
# Test bugfix for #6526


def test_warp_max_downsampling_missed_edges():

    ds = gdal.Open("data/bug_6526_warped.vrt")
    assert ds.GetRasterBand(1).ComputeRasterMinMax() == (1, 1)


###############################################################################
# Test bugfix for #7733


def test_warp_average_oversampling():

    ds = gdal.Open("data/warp_average_oversampling.vrt")
    got_data = struct.unpack("d" * (14 * 14), ds.GetRasterBand(1).ReadRaster())
    # 4 first and last lines are all nan
    for y in range(4):
        for x in range(14):
            assert math.isnan(got_data[y * 14 + x])
            assert math.isnan(got_data[(14 - 1 - y) * 14 + x])

    # Middle lines start and end with 4 nans, and with 3 at the middle
    for y in range(6):
        for x in range(4):
            assert math.isnan(got_data[(y + 4) * 14 + x])
            assert math.isnan(got_data[(y + 4) * 14 + (14 - 1 - x)])
        for x in range(6):
            assert got_data[(y + 4) * 14 + (x + 4)] == 3.0


###############################################################################
# Test bug fix for https://github.com/qgis/QGIS/issues/56288


@pytest.mark.require_driver("XYZ")
def test_non_square():

    # bottom up
    content = """y x z
30.0 10.0 1
30.0 10.2 2
30.0 10.4 3
30.1 10.0 4
30.1 10.2 5
30.1 10.4 6
30.2 10.0 7
30.2 10.2 8
30.2 10.4 9
"""

    with gdaltest.tempfile("/vsimem/grid.xyz", content):
        ds = gdal.Open("/vsimem/grid.xyz")
        assert ds.RasterXSize == 3 and ds.RasterYSize == 3
        assert ds.GetGeoTransform() == pytest.approx((9.9, 0.2, 0.0, 29.95, 0.0, 0.1))
        ulx, xres, xskew, uly, yskew, yres = ds.GetGeoTransform()
        lrx = ulx + (ds.RasterXSize * xres)
        lry = uly + (ds.RasterYSize * yres)
        assert lrx == pytest.approx(10.5)
        assert lry == pytest.approx(30.25)
        assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt8
        assert struct.unpack("b" * (3 * 3), ds.ReadRaster()) == (
            1,
            2,
            3,
            4,
            5,
            6,
            7,
            8,
            9,
        )

        warped = gdal.AutoCreateWarpedVRT(ds)
        assert warped.GetRasterBand(1).DataType == gdal.GDT_UInt8
        assert (
            warped.RasterXSize == ds.RasterXSize
            and warped.RasterYSize == ds.RasterYSize
        )
        assert warped.GetGeoTransform() == pytest.approx(
            (9.9, 0.2, 0.0, 30.25, 0.0, -0.1)
        )
        assert struct.unpack("b" * (3 * 3), warped.ReadRaster()) == (
            7,
            8,
            9,
            4,
            5,
            6,
            1,
            2,
            3,
        )

        # Test extent calculation
        res = gdal.SuggestedWarpOutput(ds, [])
        assert res.ymin == pytest.approx(29.95)
        assert res.ymax == pytest.approx(30.25)
        assert res.xmin == pytest.approx(9.9)
        assert res.xmax == pytest.approx(10.5)


###############################################################################
# Test EXCLUDED_VALUES warping option with average resampling


def test_warp_average_excluded_values():

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 2, 3, gdal.GDT_UInt8)
    src_ds.GetRasterBand(1).WriteRaster(
        0, 0, 2, 2, struct.pack("B" * 4, 10, 20, 30, 40)
    )
    src_ds.GetRasterBand(2).WriteRaster(
        0, 0, 2, 2, struct.pack("B" * 4, 11, 21, 31, 41)
    )
    src_ds.GetRasterBand(3).WriteRaster(
        0, 0, 2, 2, struct.pack("B" * 4, 12, 22, 32, 42)
    )
    src_ds.SetGeoTransform([1, 1, 0, 1, 0, 1])

    with pytest.raises(
        Exception,
        match="EXCLUDED_VALUES should contain one or several tuples of 3 values",
    ):
        out_ds = gdal.Warp(
            "", src_ds, options="-of MEM -ts 1 1 -r average -wo EXCLUDED_VALUES=30,31"
        )

    # The excluded value is just ignored in contributing source pixels that are average, as it represents only 25% of contributing pixels
    out_ds = gdal.Warp(
        "", src_ds, options="-of MEM -ts 1 1 -r average -wo EXCLUDED_VALUES=(30,31,32)"
    )
    assert struct.unpack("B" * 3, out_ds.ReadRaster()) == (
        (10 + 20 + 40) // 3,
        (11 + 21 + 41) // 3,
        (12 + 22 + 42) // 3,
    )

    # The excluded value is selected because its contributing 25% is >= 0%
    out_ds = gdal.Warp(
        "",
        src_ds,
        options="-of MEM -ts 1 1 -r average -wo EXCLUDED_VALUES=(30,31,32) -wo EXCLUDED_VALUES_PCT_THRESHOLD=0",
    )
    assert struct.unpack("B" * 3, out_ds.ReadRaster()) == (30, 31, 32)

    # The excluded value is selected because its contributing 25% is >= 24%
    out_ds = gdal.Warp(
        "",
        src_ds,
        options="-of MEM -ts 1 1 -r average -wo EXCLUDED_VALUES=(30,31,32) -wo EXCLUDED_VALUES_PCT_THRESHOLD=24",
    )
    assert struct.unpack("B" * 3, out_ds.ReadRaster()) == (30, 31, 32)

    # The excluded value is selected because its contributing 25% is < 26%
    out_ds = gdal.Warp(
        "",
        src_ds,
        options="-of MEM -ts 1 1 -r average -wo EXCLUDED_VALUES=(30,31,32) -wo EXCLUDED_VALUES_PCT_THRESHOLD=26",
    )
    assert struct.unpack("B" * 3, out_ds.ReadRaster()) == (
        (10 + 20 + 40) // 3,
        (11 + 21 + 41) // 3,
        (12 + 22 + 42) // 3,
    )

    # No match of excluded value
    out_ds = gdal.Warp(
        "", src_ds, options="-of MEM -ts 1 1 -r average -wo EXCLUDED_VALUES=(30,31,0)"
    )
    assert struct.unpack("B" * 3, out_ds.ReadRaster()) == (
        (10 + 20 + 30 + 40) // 4,
        (11 + 21 + 31 + 41) // 4,
        (12 + 22 + 32 + 42) // 4,
    )


###############################################################################
# Test NODATA_VALUES_PCT_THRESHOLD warping option with average resampling


def test_warp_average_NODATA_VALUES_PCT_THRESHOLD():

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 2, 1, gdal.GDT_UInt8)
    src_ds.GetRasterBand(1).WriteRaster(
        0, 0, 2, 2, struct.pack("B" * 4, 10, 20, 30, 40)
    )
    src_ds.SetGeoTransform([1, 1, 0, 1, 0, 1])
    src_ds.GetRasterBand(1).SetNoDataValue(20)

    out_ds = gdal.Warp("", src_ds, options="-of MEM -ts 1 1 -r average")
    assert struct.unpack("B", out_ds.ReadRaster())[0] == round((10 + 30 + 40) / 3)

    out_ds = gdal.Warp(
        "",
        src_ds,
        options="-of MEM -ts 1 1 -r average -wo NODATA_VALUES_PCT_THRESHOLD=80",
    )
    assert struct.unpack("B", out_ds.ReadRaster())[0] == round((10 + 30 + 40) / 3)

    out_ds = gdal.Warp(
        "",
        src_ds,
        options="-of MEM -ts 1 1 -r average -wo NODATA_VALUES_PCT_THRESHOLD=30",
    )
    assert struct.unpack("B", out_ds.ReadRaster())[0] == round((10 + 30 + 40) / 3)

    out_ds = gdal.Warp(
        "",
        src_ds,
        options="-of MEM -ts 1 1 -r average -wo NODATA_VALUES_PCT_THRESHOLD=25",
    )
    assert struct.unpack("B", out_ds.ReadRaster())[0] == 20


###############################################################################
#


@pytest.mark.parametrize(
    "dt,expected_val",
    [
        (gdal.GDT_UInt8, 1.0),
        (gdal.GDT_Int8, -1.0),
        (gdal.GDT_UInt16, 1.0),
        (gdal.GDT_Int16, -1.0),
        (gdal.GDT_UInt32, 1.0),
        (gdal.GDT_Int32, -1.0),
        (gdal.GDT_UInt64, 1.0),
        (gdal.GDT_Int64, -1.0),
        (gdal.GDT_Float32, 1.401298464324817e-45),
        (gdal.GDT_Float64, 5e-324),
    ],
)
@pytest.mark.parametrize("resampling", ["nearest", "bilinear"])
def test_warp_nodata_substitution(dt, expected_val, resampling):

    src_ds = gdal.GetDriverByName("MEM").Create("", 4, 4, 1, dt)
    src_ds.SetGeoTransform([1, 1, 0, 1, 0, 1])

    with gdaltest.error_raised(gdal.CE_Warning):
        out_ds = gdal.Warp(
            "",
            src_ds,
            options=f"-of MEM -dstnodata 0 -r {resampling}",
        )
    assert (
        struct.unpack("d", out_ds.ReadRaster(0, 0, 1, 1, buf_type=gdal.GDT_Float64))[0]
        == expected_val
    )

    src_ds = gdal.GetDriverByName("MEM").Create("", 4, 4, 2, dt)
    src_ds.SetGeoTransform([1, 1, 0, 1, 0, 1])
    src_ds.GetRasterBand(2).Fill(1)

    with gdaltest.error_raised(gdal.CE_Warning):
        out_ds = gdal.Warp(
            "",
            src_ds,
            options=f"-of MEM -dstnodata 0 -r {resampling}",
        )
    assert (
        struct.unpack(
            "d",
            out_ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1, buf_type=gdal.GDT_Float64),
        )[0]
        == expected_val
    )
    assert (
        struct.unpack(
            "d",
            out_ds.GetRasterBand(2).ReadRaster(0, 0, 1, 1, buf_type=gdal.GDT_Float64),
        )[0]
        == 1
    )

    out_ds = gdal.Warp(
        "",
        src_ds,
        options=f"-of MEM -dstnodata 0 -r {resampling} -wo UNIFIED_SRC_NODATA=YES",
    )
    assert (
        struct.unpack(
            "d",
            out_ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1, buf_type=gdal.GDT_Float64),
        )[0]
        == 0
    )
    assert (
        struct.unpack(
            "d",
            out_ds.GetRasterBand(2).ReadRaster(0, 0, 1, 1, buf_type=gdal.GDT_Float64),
        )[0]
        == 1
    )

    src_ds.GetRasterBand(2).Fill(0)

    with gdaltest.error_raised(gdal.CE_Warning):
        out_ds = gdal.Warp(
            "",
            src_ds,
            options=f"-of MEM -dstnodata 0 -r {resampling} -wo UNIFIED_SRC_NODATA=YES",
        )
    assert (
        struct.unpack(
            "d",
            out_ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1, buf_type=gdal.GDT_Float64),
        )[0]
        == expected_val
    )
    assert (
        struct.unpack(
            "d",
            out_ds.GetRasterBand(2).ReadRaster(0, 0, 1, 1, buf_type=gdal.GDT_Float64),
        )[0]
        == expected_val
    )

    out_ds = gdal.Warp(
        "",
        src_ds,
        options=f"-of MEM -srcnodata 0 -dstnodata 0 -r {resampling} -wo UNIFIED_SRC_NODATA=YES",
    )
    assert (
        struct.unpack(
            "d",
            out_ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1, buf_type=gdal.GDT_Float64),
        )[0]
        == 0
    )
    assert (
        struct.unpack(
            "d",
            out_ds.GetRasterBand(2).ReadRaster(0, 0, 1, 1, buf_type=gdal.GDT_Float64),
        )[0]
        == 0
    )


###############################################################################
# Test propagation of errors from I/O threads to main thread in multi-threaded reading


@gdaltest.enable_exceptions()
def test_warp_multi_threaded_errors(tmp_vsimem):

    filename1 = str(tmp_vsimem / "tmp1.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename1, 1, 1)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds.Close()

    filename2 = str(tmp_vsimem / "tmp2.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename2, 1, 1)
    ds.SetGeoTransform([3, 1, 0, 49, 0, -1])
    ds.Close()

    vrt_filename = str(tmp_vsimem / "tmp.vrt")
    gdal.BuildVRT(vrt_filename, [filename1, filename2])

    gdal.Unlink(filename2)

    with gdal.Open(vrt_filename) as ds:
        with pytest.raises(Exception):
            gdal.Warp("", ds, format="MEM", multithread=True)


###############################################################################


def test_warp_validate_options():

    if (
        gdaltest.is_travis_branch("mingw64")
        or gdaltest.is_travis_branch("build-windows-conda")
        or gdaltest.is_travis_branch("build-windows-minimum")
    ):
        pytest.skip("Crashes for unknown reason")

    from lxml import etree

    schema_optionlist = etree.XML(
        r"""
    <xs:schema attributeFormDefault="unqualified" elementFormDefault="qualified" xmlns:xs="http://www.w3.org/2001/XMLSchema">
        <xs:element name="Value">
        <xs:complexType>
          <xs:simpleContent>
            <xs:extension base="xs:string">
              <xs:attribute type="xs:string" name="alias" use="optional"/>
            </xs:extension>
          </xs:simpleContent>
        </xs:complexType>
      </xs:element>
      <xs:element name="Option">
        <xs:complexType mixed="true">
          <xs:sequence>
            <xs:element ref="Value" maxOccurs="unbounded" minOccurs="0"/>
          </xs:sequence>
          <xs:attribute name="name" use="required">
            <xs:simpleType>
              <xs:restriction base="xs:string">
                <xs:pattern value="[^\s]*"/>
              </xs:restriction>
            </xs:simpleType>
          </xs:attribute>
          <xs:attribute name="type" use="required">
            <xs:simpleType>
              <xs:restriction base="xs:string">
                <xs:enumeration value="int" />
                <xs:enumeration value="float" />
                <xs:enumeration value="boolean" />
                <xs:enumeration value="string-select" />
                <xs:enumeration value="string" />
              </xs:restriction>
            </xs:simpleType>
          </xs:attribute>
          <xs:attribute type="xs:string" name="description" use="optional"/>
          <xs:attribute type="xs:string" name="default" use="optional"/>
          <xs:attribute type="xs:string" name="alias" use="optional"/>
          <xs:attribute type="xs:string" name="min" use="optional"/>
          <xs:attribute type="xs:string" name="max" use="optional"/>
        </xs:complexType>
      </xs:element>
      <xs:element name="OptionList">
        <xs:complexType>
          <xs:sequence>
            <xs:element ref="Option" maxOccurs="unbounded" minOccurs="0"/>
          </xs:sequence>
        </xs:complexType>
      </xs:element>
    </xs:schema>
    """
    )

    schema = etree.XMLSchema(schema_optionlist)

    xml = gdal.WarpGetOptionList()
    try:
        parser = etree.XMLParser(schema=schema)
        etree.fromstring(xml, parser)
    except Exception:
        print(xml)
        raise


###############################################################################
# Test mode resampling with all data types


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "dt",
    [
        gdal.GDT_Int8,
        gdal.GDT_UInt8,
        gdal.GDT_Int16,
        gdal.GDT_UInt16,
        gdal.GDT_Int32,
        gdal.GDT_UInt32,
        gdal.GDT_Int64,
        gdal.GDT_UInt64,
        gdal.GDT_Float16,
        gdal.GDT_Float32,
        gdal.GDT_Float64,
        gdal.GDT_CInt16,
        gdal.GDT_CInt32,
        gdal.GDT_CFloat16,
        gdal.GDT_CFloat32,
        gdal.GDT_CFloat64,
    ],
)
def test_warp_mode(dt):

    ds = gdal.GetDriverByName("MEM").Create("", 3, 1, 1, dt)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    dtsize = gdal.GetDataTypeSizeBytes(dt)
    val = (
        b"\x38"
        if dt
        in (
            gdal.GDT_Float16,
            gdal.GDT_Float32,
            gdal.GDT_Float64,
            gdal.GDT_CFloat16,
            gdal.GDT_CFloat32,
            gdal.GDT_CFloat64,
        )
        else b"\xFF"
    )
    ds.WriteRaster(1, 0, 2, 1, val * (2 * dtsize))

    out_ds = gdal.Warp("", ds, options="-f MEM -r mode -ts 1 1")
    assert out_ds.ReadRaster(0, 0, 1, 1) == val * dtsize, gdal.GetDataTypeName(dt)


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "dt",
    [
        gdal.GDT_Float16,
        gdal.GDT_Float32,
        gdal.GDT_Float64,
        gdal.GDT_CFloat16,
        gdal.GDT_CFloat32,
        gdal.GDT_CFloat64,
    ],
)
def test_warp_mode_nan(dt):

    ds = gdal.GetDriverByName("MEM").Create("", 5, 1, 1, dt)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    dtsize = gdal.GetDataTypeSizeBytes(dt)
    # 3 different encodings of NaN
    ds.WriteRaster(1, 0, 1, 1, b"\xFF" * dtsize)
    if sys.byteorder == "little":
        ds.WriteRaster(2, 0, 1, 1, b"\xFE" + b"\xFF" * (dtsize - 1))
        ds.WriteRaster(3, 0, 1, 1, b"\xFD" + b"\xFF" * (dtsize - 1))
    else:
        ds.WriteRaster(2, 0, 1, 1, b"\xFF" * (dtsize - 1) + b"\xFE")
        ds.WriteRaster(3, 0, 1, 1, b"\xFF" * (dtsize - 1) + b"\xFD")

    out_ds = gdal.Warp("", ds, options="-f MEM -r mode -ts 1 1")
    assert out_ds.ReadRaster(0, 0, 1, 1) == b"\xFF" * dtsize, gdal.GetDataTypeName(dt)


def test_warp_zero_sized_target_extent():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    out_ds = gdal.Warp(
        "",
        ds,
        format="MEM",
        outputBounds=[0, 1, 0, 1],
        xRes=1,
        yRes=1,
    )
    assert out_ds.RasterXSize == 1
    assert out_ds.RasterYSize == 1
