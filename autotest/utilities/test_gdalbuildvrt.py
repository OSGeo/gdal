#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdalbuildvrt testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest
import test_cli_utilities

from osgeo import gdal, osr

pytestmark = [
    pytest.mark.skipif(
        test_cli_utilities.get_gdalbuildvrt_path() is None,
        reason="gdalbuildvrt not available",
    ),
    pytest.mark.skipif(
        not gdaltest.vrt_has_open_support(),
        reason="VRT driver open missing",
    ),
]


@pytest.fixture(scope="module")
def gdalbuildvrt_path():
    return test_cli_utilities.get_gdalbuildvrt_path()


@pytest.fixture(scope="module")
def sample_tifs(tmp_path_factory):

    tmpdir = tmp_path_factory.mktemp("tmp")

    drv = gdal.GetDriverByName("GTiff")
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS("WGS84")
    wkt = srs.ExportToWkt()

    sample1_tif = str(tmpdir / "gdalbuildvrt1.tif")
    with drv.Create(sample1_tif, 10, 10, 1) as ds:
        ds.SetProjection(wkt)
        ds.SetGeoTransform([2, 0.1, 0, 49, 0, -0.1])
        ds.GetRasterBand(1).Fill(0)

    sample2_tif = str(tmpdir / "gdalbuildvrt2.tif")
    with drv.Create(sample2_tif, 10, 10, 1) as ds:
        ds.SetProjection(wkt)
        ds.SetGeoTransform([3, 0.1, 0, 49, 0, -0.1])
        ds.GetRasterBand(1).Fill(63)

    sample3_tif = str(tmpdir / "gdalbuildvrt3.tif")
    with drv.Create(sample3_tif, 10, 10, 1) as ds:
        ds.SetProjection(wkt)
        ds.SetGeoTransform([2, 0.1, 0, 48, 0, -0.1])
        ds.GetRasterBand(1).Fill(127)

    sample4_tif = str(tmpdir / "gdalbuildvrt4.tif")
    with drv.Create(sample4_tif, 10, 10, 1) as ds:
        ds.SetProjection(wkt)
        ds.SetGeoTransform([3, 0.1, 0, 48, 0, -0.1])
        ds.GetRasterBand(1).Fill(255)

    yield (sample1_tif, sample2_tif, sample3_tif, sample4_tif)


###############################################################################
def gdalbuildvrt_check(mosaic_vrt):

    with gdal.Open(mosaic_vrt) as ds:

        assert (
            ds.GetProjectionRef().find("WGS 84") != -1
        ), "Expected WGS 84\nGot : %s" % (ds.GetProjectionRef())

        gt = ds.GetGeoTransform()
        expected_gt = [2, 0.1, 0, 49, 0, -0.1]
        for i in range(6):
            assert not abs(gt[i] - expected_gt[i] > 1e-5), "Expected : %s\nGot : %s" % (
                expected_gt,
                gt,
            )

        assert (
            ds.RasterXSize == 20 and ds.RasterYSize == 20
        ), "Wrong raster dimensions : %d x %d" % (ds.RasterXSize, ds.RasterYSize)

        assert ds.RasterCount == 1, "Wrong raster count : %d " % (ds.RasterCount)

        assert ds.GetRasterBand(1).Checksum() == 3508, "Wrong checksum"


###############################################################################
# Simple test


def test_gdalbuildvrt_1(gdalbuildvrt_path, tmp_path, sample_tifs):

    mosaic_vrt = str(tmp_path / "mosaic.vrt")

    _, err = gdaltest.runexternal_out_and_err(
        gdalbuildvrt_path + f" {mosaic_vrt} {' '.join(sample_tifs)}"
    )
    assert err is None or err == "", "got error/warning"

    return gdalbuildvrt_check(mosaic_vrt)


###############################################################################
# Test with tile index


def test_gdalbuildvrt_2(gdalbuildvrt_path, tmp_path, sample_tifs):

    if test_cli_utilities.get_gdaltindex_path() is None:
        pytest.skip()

    tileindex_shp = str(tmp_path / "tileindex.shp")
    mosaic_vrt = str(tmp_path / "mosaic.vrt")

    gdaltest.runexternal(
        test_cli_utilities.get_gdaltindex_path()
        + f" {tileindex_shp} {' '.join(sample_tifs)}"
    )

    gdaltest.runexternal(gdalbuildvrt_path + f" {mosaic_vrt} {tileindex_shp}")

    return gdalbuildvrt_check(mosaic_vrt)


###############################################################################
# Test with file list


def test_gdalbuildvrt_3(gdalbuildvrt_path, tmp_path, sample_tifs):

    mosaic_vrt = str(tmp_path / "mosaic.vrt")
    filelist_txt = str(tmp_path / "filelist.txt")

    with open(filelist_txt, "wt") as filelist:
        filelist.write("\n".join(sample_tifs))

    gdaltest.runexternal(
        gdalbuildvrt_path + f" -input_file_list {filelist_txt} {mosaic_vrt}"
    )

    return gdalbuildvrt_check(mosaic_vrt)


###############################################################################
# Try adding a raster in another projection


def test_gdalbuildvrt_4(gdalbuildvrt_path, tmp_path, sample_tifs):

    mosaic_vrt = str(tmp_path / "mosaic.vrt")

    drv = gdal.GetDriverByName("GTiff")
    wkt = """GEOGCS["WGS 72",
    DATUM["WGS_1972",
        SPHEROID["WGS 72",6378135,298.26],
        TOWGS84[0,0,4.5,0,0,0.554,0.2263]],
    PRIMEM["Greenwich",0],
    UNIT["degree",0.0174532925199433]]"""

    input5_tif = str(tmp_path / "gdalbuildvrt5.tif")

    with drv.Create(input5_tif, 10, 10, 1) as ds:
        ds.SetProjection(wkt)
        ds.SetGeoTransform([47, 0.1, 0, 2, 0, -0.1])

    gdaltest.runexternal(
        gdalbuildvrt_path + f" {mosaic_vrt} {' '.join(sample_tifs)} {input5_tif}"
    )

    return gdalbuildvrt_check(mosaic_vrt)


###############################################################################
# Try adding a raster with different band count


# commented out originally in 4ef886421c99a4451f8873cb6e094d45ecc86d3f, not sure why
def test_gdalbuildvrt_5(gdalbuildvrt_path, tmp_path, sample_tifs):

    mosaic_vrt = str(tmp_path / "mosaic.vrt")

    drv = gdal.GetDriverByName("GTiff")
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS("WGS84")
    wkt = srs.ExportToWkt()

    input5_tif = str(tmp_path / "gdalbuildvrt5.tif")

    with drv.Create(input5_tif, 10, 10, 2) as ds:
        ds.SetProjection(wkt)
        ds.SetGeoTransform([47, 0.1, 0, 2, 0, -0.1])

    gdaltest.runexternal(
        gdalbuildvrt_path + f" {mosaic_vrt} {' '.join(sample_tifs)} {input5_tif}"
    )

    return gdalbuildvrt_check(mosaic_vrt)


###############################################################################
# Test -separate option


def test_gdalbuildvrt_6(gdalbuildvrt_path, tmp_path, sample_tifs):

    output_vrt = str(tmp_path / "stacked.vrt")

    gdaltest.runexternal(
        gdalbuildvrt_path + f" -separate {output_vrt} {' '.join(sample_tifs)}"
    )

    ds = gdal.Open(output_vrt)
    assert ds.GetProjectionRef().find("WGS 84") != -1, "Expected WGS 84\nGot : %s" % (
        ds.GetProjectionRef()
    )

    gt = ds.GetGeoTransform()
    expected_gt = [2, 0.1, 0, 49, 0, -0.1]
    for i in range(6):
        assert not abs(gt[i] - expected_gt[i] > 1e-5), "Expected : %s\nGot : %s" % (
            expected_gt,
            gt,
        )

    assert (
        ds.RasterXSize == 20 and ds.RasterYSize == 20
    ), "Wrong raster dimensions : %d x %d" % (ds.RasterXSize, ds.RasterYSize)

    assert ds.RasterCount == 4, "Wrong raster count : %d " % (ds.RasterCount)

    assert ds.GetRasterBand(1).Checksum() == 0, "Wrong checksum"


###############################################################################
# Test source rasters with nodata


def test_gdalbuildvrt_7(gdalbuildvrt_path, tmp_path):

    input1_tif = str(tmp_path / "vrtnull1.tif")
    input2_tif = str(tmp_path / "vrtnull2.tif")
    output_vrt = str(tmp_path / "gdalbuildvrt7.vrt")

    with gdal.GetDriverByName("GTiff").Create(
        input1_tif, 20, 10, 3, gdal.GDT_UInt16
    ) as out_ds:
        out_ds.SetGeoTransform([2, 0.1, 0, 49, 0, -0.1])
        srs = osr.SpatialReference()
        srs.SetFromUserInput("EPSG:4326")
        out_ds.SetProjection(srs.ExportToWkt())
        out_ds.GetRasterBand(1).SetRasterColorInterpretation(gdal.GCI_RedBand)
        out_ds.GetRasterBand(2).SetRasterColorInterpretation(gdal.GCI_GreenBand)
        out_ds.GetRasterBand(3).SetRasterColorInterpretation(gdal.GCI_BlueBand)
        out_ds.GetRasterBand(1).SetNoDataValue(256)

        try:
            ff = "\xff".encode("latin1")
        except UnicodeDecodeError:
            ff = "\xff"

        out_ds.GetRasterBand(1).WriteRaster(
            0, 0, 10, 10, ff, buf_type=gdal.GDT_UInt8, buf_xsize=1, buf_ysize=1
        )
        out_ds.GetRasterBand(2).WriteRaster(
            0, 0, 10, 10, "\x00", buf_type=gdal.GDT_UInt8, buf_xsize=1, buf_ysize=1
        )
        out_ds.GetRasterBand(3).WriteRaster(
            0, 0, 10, 10, "\x00", buf_type=gdal.GDT_UInt8, buf_xsize=1, buf_ysize=1
        )

    with gdal.GetDriverByName("GTiff").Create(
        input2_tif, 20, 10, 3, gdal.GDT_UInt16
    ) as out_ds:
        out_ds.SetGeoTransform([2, 0.1, 0, 49, 0, -0.1])
        srs = osr.SpatialReference()
        srs.SetFromUserInput("EPSG:4326")
        out_ds.SetProjection(srs.ExportToWkt())
        out_ds.GetRasterBand(1).SetRasterColorInterpretation(gdal.GCI_RedBand)
        out_ds.GetRasterBand(2).SetRasterColorInterpretation(gdal.GCI_GreenBand)
        out_ds.GetRasterBand(3).SetRasterColorInterpretation(gdal.GCI_BlueBand)
        out_ds.GetRasterBand(1).SetNoDataValue(256)

        out_ds.GetRasterBand(1).WriteRaster(
            10, 0, 10, 10, "\x00", buf_type=gdal.GDT_UInt8, buf_xsize=1, buf_ysize=1
        )
        out_ds.GetRasterBand(2).WriteRaster(
            10, 0, 10, 10, ff, buf_type=gdal.GDT_UInt8, buf_xsize=1, buf_ysize=1
        )
        out_ds.GetRasterBand(3).WriteRaster(
            10, 0, 10, 10, "\x00", buf_type=gdal.GDT_UInt8, buf_xsize=1, buf_ysize=1
        )

    gdaltest.runexternal(gdalbuildvrt_path + f" {output_vrt} {input1_tif} {input2_tif}")

    with gdal.Open(output_vrt) as ds:

        assert ds.GetRasterBand(1).Checksum() == 1217, "Wrong checksum"

        assert ds.GetRasterBand(2).Checksum() == 1218, "Wrong checksum"

        assert ds.GetRasterBand(3).Checksum() == 0, "Wrong checksum"


###############################################################################
# Test -tr option


def test_gdalbuildvrt_8(gdalbuildvrt_path, tmp_path, sample_tifs):

    mosaic_vrt = str(tmp_path / "mosiac.vrt")
    mosaic2_vrt = str(tmp_path / "mosaic2.vrt")

    gdaltest.runexternal(
        gdalbuildvrt_path + f" -tr 0.05 0.05 {mosaic2_vrt} {' '.join(sample_tifs)}"
    )

    ds = gdal.Open(mosaic2_vrt)

    gt = ds.GetGeoTransform()
    expected_gt = [2, 0.05, 0, 49, 0, -0.05]
    for i in range(6):
        assert not abs(gt[i] - expected_gt[i] > 1e-5), "Expected : %s\nGot : %s" % (
            expected_gt,
            gt,
        )

    assert (
        ds.RasterXSize == 40 and ds.RasterYSize == 40
    ), "Wrong raster dimensions : %d x %d" % (ds.RasterXSize, ds.RasterYSize)

    gdaltest.runexternal(gdalbuildvrt_path + f" -tr 0.1 0.1 {mosaic_vrt} {mosaic2_vrt}")

    return gdalbuildvrt_check(mosaic_vrt)


###############################################################################
# Test -te option


def test_gdalbuildvrt_9(gdalbuildvrt_path, tmp_path, sample_tifs):

    mosaic_vrt = str(tmp_path / "mosiac.vrt")
    mosaic2_vrt = str(tmp_path / "mosaic2.vrt")

    gdaltest.runexternal(
        gdalbuildvrt_path + f" -te 1 46 5 50 {mosaic2_vrt} {' '.join(sample_tifs)}"
    )

    ds = gdal.Open(mosaic2_vrt)

    gt = ds.GetGeoTransform()
    expected_gt = [1, 0.1, 0, 50, 0, -0.1]
    for i in range(6):
        assert not abs(gt[i] - expected_gt[i] > 1e-5), "Expected : %s\nGot : %s" % (
            expected_gt,
            gt,
        )

    assert (
        ds.RasterXSize == 40 and ds.RasterYSize == 40
    ), "Wrong raster dimensions : %d x %d" % (ds.RasterXSize, ds.RasterYSize)

    gdaltest.runexternal(
        gdalbuildvrt_path + f" -te 2 47 4 49 {mosaic_vrt} {mosaic2_vrt}"
    )

    return gdalbuildvrt_check(mosaic_vrt)


###############################################################################
# Test explicit nodata setting (#3254)


def test_gdalbuildvrt_10(gdalbuildvrt_path, tmp_path):

    input1_tif = str(tmp_path / "test_gdalbuildvrt_10_1.tif")
    input2_tif = str(tmp_path / "test_gdalbuildvrt_10_2.tif")
    output_vrt = str(tmp_path / "gdalbuildvrt10.vrt")

    with gdal.GetDriverByName("GTiff").Create(
        input1_tif,
        10,
        10,
        1,
        gdal.GDT_UInt8,
        options=["NBITS=1", "PHOTOMETRIC=MINISWHITE"],
    ) as out_ds:
        out_ds.SetGeoTransform([2, 0.1, 0, 49, 0, -0.1])
        srs = osr.SpatialReference()
        srs.SetFromUserInput("EPSG:4326")
        out_ds.SetProjection(srs.ExportToWkt())

        out_ds.GetRasterBand(1).WriteRaster(
            1, 1, 3, 3, "\x01", buf_type=gdal.GDT_UInt8, buf_xsize=1, buf_ysize=1
        )

    with gdal.GetDriverByName("GTiff").Create(
        input2_tif,
        10,
        10,
        1,
        gdal.GDT_UInt8,
        options=["NBITS=1", "PHOTOMETRIC=MINISWHITE"],
    ) as out_ds:
        out_ds.SetGeoTransform([2, 0.1, 0, 49, 0, -0.1])
        srs = osr.SpatialReference()
        srs.SetFromUserInput("EPSG:4326")
        out_ds.SetProjection(srs.ExportToWkt())

        out_ds.GetRasterBand(1).WriteRaster(
            6, 6, 3, 3, "\x01", buf_type=gdal.GDT_UInt8, buf_xsize=1, buf_ysize=1
        )

    gdaltest.runexternal(
        gdalbuildvrt_path + f" -srcnodata 0 {output_vrt} {input1_tif} {input2_tif}"
    )

    ds = gdal.Open(output_vrt)

    assert ds.GetRasterBand(1).Checksum() == 18, "Wrong checksum"

    ds = None


###############################################################################
# Test that we can stack ungeoreference single band images with -separate (#3432)


def test_gdalbuildvrt_11(gdalbuildvrt_path, tmp_path):

    output_vrt = str(tmp_path / "gdalbuildvrt11.vrt")

    input_tif1 = str(tmp_path / "test_gdalbuildvrt_11_1.tif")
    input_tif2 = str(tmp_path / "test_gdalbuildvrt_11_2.tif")

    with gdal.GetDriverByName("GTiff").Create(input_tif1, 10, 10, 1) as out_ds:
        out_ds.GetRasterBand(1).Fill(255)
        cs1 = out_ds.GetRasterBand(1).Checksum()

    with gdal.GetDriverByName("GTiff").Create(input_tif2, 10, 10, 1) as out_ds:
        out_ds.GetRasterBand(1).Fill(127)
        cs2 = out_ds.GetRasterBand(1).Checksum()

    gdaltest.runexternal(
        gdalbuildvrt_path + f" -separate {output_vrt} {input_tif1} {input_tif2}"
    )

    with gdal.Open(output_vrt) as ds:

        assert ds.GetRasterBand(1).Checksum() == cs1, "Wrong checksum"

        assert ds.GetRasterBand(2).Checksum() == cs2, "Wrong checksum"


###############################################################################
# Test -tap option


def test_gdalbuildvrt_12(gdalbuildvrt_path, tmp_path):

    output_vrt = str(tmp_path / "gdalbuildvrt12.vrt")

    _, err = gdaltest.runexternal_out_and_err(
        gdalbuildvrt_path + f" -tap {output_vrt} ../gcore/data/byte.tif",
        check_memleak=False,
    )

    assert "-tap option cannot be used without using -tr" in err, "expected error"

    gdaltest.runexternal(
        gdalbuildvrt_path + f" -tr 100 50 -tap {output_vrt} ../gcore/data/byte.tif"
    )

    ds = gdal.Open(output_vrt)

    gt = ds.GetGeoTransform()
    expected_gt = [440700.0, 100.0, 0.0, 3751350.0, 0.0, -50.0]
    for i in range(6):
        assert not abs(gt[i] - expected_gt[i] > 1e-5), "Expected : %s\nGot : %s" % (
            expected_gt,
            gt,
        )

    assert (
        ds.RasterXSize == 13 and ds.RasterYSize == 25
    ), "Wrong raster dimensions : %d x %d" % (ds.RasterXSize, ds.RasterYSize)


###############################################################################
# Test -a_srs


def test_gdalbuildvrt_13(gdalbuildvrt_path, tmp_path):

    output_vrt = str(tmp_path / "gdalbuildvrt13.vrt")

    gdaltest.runexternal(
        gdalbuildvrt_path + f" {output_vrt} ../gcore/data/byte.tif -a_srs EPSG:4326"
    )

    with gdal.Open(output_vrt) as ds:
        assert "4326" in ds.GetProjectionRef()


###############################################################################
# Test -r


def test_gdalbuildvrt_14(gdalbuildvrt_path, tmp_path):
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    buildvrt_output_vrt = str(tmp_path / "test_gdalbuildvrt_14.vrt")

    gdaltest.runexternal(
        gdalbuildvrt_path
        + f" {buildvrt_output_vrt} ../gcore/data/byte.tif -r cubic -tr 30 30"
    )

    translate_output_vrt = str(tmp_path / "test_gdalbuildvrt_14_ref.vrt")

    gdaltest.runexternal(
        test_cli_utilities.get_gdal_translate_path()
        + f" -of VRT ../gcore/data/byte.tif {translate_output_vrt} -r cubic -outsize 40 40"
    )

    with gdal.Open(buildvrt_output_vrt) as ds:
        cs = ds.GetRasterBand(1).Checksum()

    with gdal.Open(translate_output_vrt) as ds_ref:
        cs_ref = ds_ref.GetRasterBand(1).Checksum()

    assert cs == cs_ref


###############################################################################
# Test -b


def test_gdalbuildvrt_15(gdalbuildvrt_path, tmp_path):

    output_vrt = str(tmp_path / "test_gdalbuildvrt_15.vrt")

    gdaltest.runexternal(
        gdalbuildvrt_path + f" {output_vrt}  ../gcore/data/byte.tif -b 1"
    )

    with gdal.Open(output_vrt) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672


###############################################################################
# Test output to non writable file


def test_gdalbuildvrt_16(gdalbuildvrt_path):

    out, err = gdaltest.runexternal_out_and_err(
        gdalbuildvrt_path
        + " /non_existing_dir/non_existing_subdir/out.vrt ../gcore/data/byte.tif"
    )

    if not gdaltest.is_travis_branch("mingw"):
        assert "ERROR ret code = 1" in err, out
    else:
        # We don't get the error code on Travis mingw
        assert "ERROR" in err, out
