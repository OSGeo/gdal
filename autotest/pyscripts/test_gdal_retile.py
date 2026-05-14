#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_retile.py testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import glob
import os

import gdaltest
import pytest
import test_py_scripts

from osgeo import gdal, osr

pytestmark = pytest.mark.skipif(
    test_py_scripts.get_py_script("gdal_retile") is None,
    reason="gdal_retile not available",
)


@pytest.fixture()
def script_path():
    return test_py_scripts.get_py_script("gdal_retile")


###############################################################################
#


def test_gdal_retile_help(script_path):

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdal_retile", "--help"
    )


###############################################################################
#


def test_gdal_retile_version(script_path):

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdal_retile", "--version"
    )


###############################################################################
# Test gdal_retile.py


def test_gdal_retile_1(script_path, tmp_path):

    from osgeo_utils import gdal_retile

    out_dir = tmp_path / "outretile2"
    out_dir.mkdir()

    gdal_retile.main(
        [
            gdal_retile.__file__,
            "-v",
            "-levels",
            2,
            "-r",
            "bilinear",
            "-targetDir",
            out_dir,
            test_py_scripts.get_data_path("gcore") + "byte.tif",
        ]
    )

    with gdal.Open(f"{out_dir}/byte_1_1.tif") as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672

    with gdal.Open(f"{out_dir}/1/byte_1_1.tif") as ds:
        assert ds.RasterXSize == 10
        # if ds.GetRasterBand(1).Checksum() != 1152:
        #    print(ds.GetRasterBand(1).Checksum())
        #    return 'fail'

    with gdal.Open(f"{out_dir}/2/byte_1_1.tif") as ds:
        assert ds.RasterXSize == 5
        # if ds.GetRasterBand(1).Checksum() != 215:
        #    print(ds.GetRasterBand(1).Checksum())
        #    return 'fail'


###############################################################################
# Test gdal_retile.py with RGBA dataset


def test_gdal_retile_2(script_path, tmp_path):

    out_dir = tmp_path / "outretile2"
    out_dir.mkdir()

    _, err = test_py_scripts.run_py_script(
        script_path,
        "gdal_retile",
        f"-v -levels 2 -r bilinear -targetDir {out_dir} "
        + test_py_scripts.get_data_path("gcore")
        + "rgba.tif",
        return_stderr=True,
    )
    assert "UseExceptions" not in err

    ds = gdal.Open(f"{out_dir}/2/rgba_1_1.tif")
    assert ds.GetRasterBand(1).Checksum() == 35, "wrong checksum for band 1"
    assert ds.GetRasterBand(4).Checksum() == 35, "wrong checksum for band 4"
    ds = None


###############################################################################
# test gdal_retile.py with input having gaps

# This test first creates input rasters which would create an tile index having some tiles with no input data.
# if these 100x100 rasters are retiled to 20x20, the upper left hand corner should have tiles.

#
# 30 N               ---------------
#                    |             |
#                    |   100X00    |
#                    |   Image 2   |
#                    |             |
# 15 N -----------------------------
#      |             |             |
#      |   100X00    |   100X00    |
#      |   Image 1   |   Image 3   |
#      |             |             |
#  0 N -----------------------------
#      0 E           15 E         30 E


def test_gdal_retile_non_contigous(script_path, tmp_path):
    drv = gdal.GetDriverByName("GTiff")
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS("WGS84")
    wkt = srs.ExportToWkt()

    in1_tif = str(tmp_path / "in1.tif")
    with drv.Create(in1_tif, 100, 100, 1) as ds:
        px1_x = 15.0 / ds.RasterXSize
        px1_y = 15.0 / ds.RasterYSize
        ds.SetProjection(wkt)
        ds.SetGeoTransform([0, px1_x, 0, 15, 0, -px1_y])
        ds.GetRasterBand(1).Fill(0)

    in2_tif = str(tmp_path / "in2.tif")
    with drv.Create(in2_tif, 100, 100, 1) as ds:
        px2_x = 15.0 / ds.RasterXSize
        px2_y = 15.0 / ds.RasterYSize
        ds.SetProjection(wkt)
        ds.SetGeoTransform([15, px2_x, 0, 30, 0, -px2_y])
        ds.GetRasterBand(1).Fill(21)

    in3_tif = str(tmp_path / "in3.tif")
    with drv.Create(in3_tif, 100, 100, 1) as ds:
        px3_x = 15.0 / ds.RasterXSize
        px3_y = 15.0 / ds.RasterYSize
        ds.SetProjection(wkt)
        ds.SetGeoTransform([15, px3_x, 0, 15, 0, -px3_y])
        ds.GetRasterBand(1).Fill(42)

    out_dir = tmp_path / "outretile_noncontigous"
    out_dir.mkdir()

    script_out = test_py_scripts.run_py_script(
        script_path,
        "gdal_retile",
        f"-v -levels 2 -r bilinear -ps 20 20 -targetDir {out_dir} {in1_tif} {in2_tif} {in3_tif}",
    )

    assert "ERROR ret code = 1" not in script_out

    assert os.path.exists(f"{out_dir}/in1_01_05.tif")
    assert os.path.exists(f"{out_dir}/1/in1_1_2.tif")


###############################################################################
# Test gdal_retile.py with input images of different pixel sizes


def test_gdal_retile_3(script_path, tmp_path):

    drv = gdal.GetDriverByName("GTiff")
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS("WGS84")
    wkt = srs.ExportToWkt()

    # Create two images to tile together. The images will cover the geographic
    # range 0E-30E and 0-60N, split horizontally at 30N. The pixel size in the
    # second image will be twice that of the first time. If the make the first
    # image black and the second gray, then the result of tiling these two
    # together should be gray square stacked on top of a black square.
    #
    # 60 N ---------------
    #      |             | \
    #      |    50x50    |  \ Image 2
    #      |             |  /
    #      |             | /
    # 30 N ---------------
    #      |             | \
    #      |  100x100    |  \ Image 1
    #      |             |  /
    #      |             | /
    #  0 N ---------------
    #      0 E           30 E

    in1_tif = str(tmp_path / "in1.tif")
    with drv.Create(in1_tif, 100, 100, 1) as ds:
        px1_x = 30.0 / ds.RasterXSize
        px1_y = 30.0 / ds.RasterYSize
        ds.SetProjection(wkt)
        ds.SetGeoTransform([0, px1_x, 0, 30, 0, -px1_y])
        ds.GetRasterBand(1).Fill(0)

    in2_tif = str(tmp_path / "in2.tif")
    with drv.Create(in2_tif, 50, 50, 1) as ds:
        px2_x = 30.0 / ds.RasterXSize
        px2_y = 30.0 / ds.RasterYSize
        ds.SetProjection(wkt)
        ds.SetGeoTransform([0, px2_x, 0, 60, 0, -px2_y])
        ds.GetRasterBand(1).Fill(42)

    out_dir = tmp_path / "outretile4"
    out_dir.mkdir()

    test_py_scripts.run_py_script(
        script_path,
        "gdal_retile",
        f"-v -levels 2 -r bilinear -targetDir {out_dir} {in1_tif} {in2_tif}",
    )

    ds = gdal.Open(f"{out_dir}/in1_1_1.tif")
    assert "WGS 84" in ds.GetProjectionRef(), "Expected WGS 84\nGot : %s" % (
        ds.GetProjectionRef()
    )

    gt = ds.GetGeoTransform()
    expected_gt = [0, px1_x, 0, 60, 0, -px1_y]
    for i in range(6):
        assert not abs(gt[i] - expected_gt[i] > 1e-5), "Expected : %s\nGot : %s" % (
            expected_gt,
            gt,
        )

    assert (
        ds.RasterXSize == 100 and ds.RasterYSize == 200
    ), "Wrong raster dimensions : %d x %d" % (ds.RasterXSize, ds.RasterYSize)

    assert ds.RasterCount == 1, "Wrong raster count : %d " % (ds.RasterCount)

    assert ds.GetRasterBand(1).Checksum() == 38999, "Wrong checksum"


###############################################################################
# Test gdal_retile.py -overlap


def test_gdal_retile_4(script_path, tmp_path):

    out_dir = tmp_path / "outretile4"
    out_dir.mkdir()

    test_py_scripts.run_py_script(
        script_path,
        "gdal_retile",
        f"-v -ps 8 7 -overlap 3 -targetDir {out_dir} "
        + test_py_scripts.get_data_path("gcore")
        + "byte.tif",
    )

    expected_results = [
        [f"{out_dir}/byte_1_1.tif", 8, 7],
        [f"{out_dir}/byte_1_2.tif", 8, 7],
        [f"{out_dir}/byte_1_3.tif", 8, 7],
        [f"{out_dir}/byte_1_4.tif", 5, 7],
        [f"{out_dir}/byte_2_1.tif", 8, 7],
        [f"{out_dir}/byte_2_2.tif", 8, 7],
        [f"{out_dir}/byte_2_3.tif", 8, 7],
        [f"{out_dir}/byte_2_4.tif", 5, 7],
        [f"{out_dir}/byte_3_1.tif", 8, 7],
        [f"{out_dir}/byte_3_2.tif", 8, 7],
        [f"{out_dir}/byte_3_3.tif", 8, 7],
        [f"{out_dir}/byte_3_4.tif", 5, 7],
        [f"{out_dir}/byte_4_1.tif", 8, 7],
        [f"{out_dir}/byte_4_2.tif", 8, 7],
        [f"{out_dir}/byte_4_3.tif", 8, 7],
        [f"{out_dir}/byte_4_4.tif", 5, 7],
        [f"{out_dir}/byte_5_1.tif", 8, 4],
        [f"{out_dir}/byte_5_2.tif", 8, 4],
        [f"{out_dir}/byte_5_3.tif", 8, 4],
        [f"{out_dir}/byte_5_4.tif", 5, 4],
    ]

    for filename, width, height in expected_results:
        ds = gdal.Open(filename)
        assert ds.RasterXSize == width, filename
        assert ds.RasterYSize == height, filename
        ds = None

    test_py_scripts.run_py_script(
        script_path,
        "gdal_retile",
        f"-v -levels 1 -ps 8 8 -overlap 4 -targetDir {out_dir} "
        + test_py_scripts.get_data_path("gcore")
        + "byte.tif",
    )

    expected_results = [
        [f"{out_dir}/byte_1_1.tif", 8, 8],
        [f"{out_dir}/byte_1_2.tif", 8, 8],
        [f"{out_dir}/byte_1_3.tif", 8, 8],
        [f"{out_dir}/byte_1_4.tif", 8, 8],
        [f"{out_dir}/byte_2_1.tif", 8, 8],
        [f"{out_dir}/byte_2_2.tif", 8, 8],
        [f"{out_dir}/byte_2_3.tif", 8, 8],
        [f"{out_dir}/byte_2_4.tif", 8, 8],
        [f"{out_dir}/byte_3_1.tif", 8, 8],
        [f"{out_dir}/byte_3_2.tif", 8, 8],
        [f"{out_dir}/byte_3_3.tif", 8, 8],
        [f"{out_dir}/byte_3_4.tif", 8, 8],
        [f"{out_dir}/byte_4_1.tif", 8, 8],
        [f"{out_dir}/byte_4_2.tif", 8, 8],
        [f"{out_dir}/byte_4_3.tif", 8, 8],
        [f"{out_dir}/byte_4_4.tif", 8, 8],
        [f"{out_dir}/1/byte_1_1.tif", 8, 8],
        [f"{out_dir}/1/byte_1_2.tif", 6, 8],
        [f"{out_dir}/1/byte_2_1.tif", 8, 6],
        [f"{out_dir}/1/byte_2_2.tif", 6, 6],
    ]

    for filename, width, height in expected_results:
        ds = gdal.Open(filename)
        assert ds.RasterXSize == width, filename
        assert ds.RasterYSize == height, filename
        ds = None


###############################################################################
# Test gdal_retile.py with input having a NoData value


def test_gdal_retile_5(script_path, tmp_path):

    gdaltest.importorskip_gdal_array()
    np = pytest.importorskip("numpy")

    nodata_value = -3.4028234663852886e38
    raster_array = np.array(([0.0, 2.0], [-1.0, nodata_value]))

    drv = gdal.GetDriverByName("GTiff")
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS("WGS84")
    wkt = srs.ExportToWkt()

    input_tif = str(tmp_path / "in5.tif")

    with drv.Create(input_tif, 2, 2, 1, gdal.GDT_Float32) as ds:
        px1_x = 0.1 / ds.RasterXSize
        px1_y = 0.1 / ds.RasterYSize
        ds.SetProjection(wkt)
        ds.SetGeoTransform([0, px1_x, 0, 30, 0, -px1_y])
        raster_band = ds.GetRasterBand(1)
        raster_band.SetNoDataValue(nodata_value)
        raster_band.WriteArray(raster_array)
        raster_band = None

    out_dir = tmp_path / "outretile5"
    out_dir.mkdir()

    test_py_scripts.run_py_script(
        script_path, "gdal_retile", f"-v -targetDir {out_dir} {input_tif}"
    )

    with gdal.Open(str(out_dir / "in5_1_1.tif")) as ds:
        raster_band = ds.GetRasterBand(1)

        assert (
            raster_band.GetNoDataValue() == nodata_value
        ), "Wrong nodata value.\nExpected %f, Got: %f" % (
            nodata_value,
            raster_band.GetNoDataValue(),
        )

        min_val, max_val = raster_band.ComputeRasterMinMax()
        assert max_val, "Wrong maximum value.\nExpected 2.0, Got: %f" % max_val

        assert min_val == -1.0, "Wrong minimum value.\nExpected -1.0, Got: %f" % min_val


###############################################################################
# Test gdal_retile.py


@pytest.mark.require_driver("PNG")
def test_gdal_retile_png(script_path, tmp_path):

    out_dir = tmp_path / "outretile_png"
    out_dir.mkdir()

    test_py_scripts.run_py_script(
        script_path,
        "gdal_retile",
        "-v -levels 2 -r bilinear -of PNG -targetDir "
        + '"'
        + str(out_dir)
        + '"'
        + " "
        + test_py_scripts.get_data_path("gcore")
        + "byte.tif",
    )

    with gdal.Open(str(out_dir / "byte_1_1.png")) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672

    assert os.path.exists(out_dir / "byte_1_1.png.aux.xml")


###############################################################################
# Test gdal_retile on a input file with a geotransform with rotational terms
# (unsupported)


def test_gdal_retile_rotational_geotransform(script_path, tmp_path):

    src_filename = str(tmp_path / "in.tif")
    ds = gdal.GetDriverByName("GTiff").Create(src_filename, 2, 2)
    ds.SetGeoTransform([2, 0.1, 0.001, 49, -0.01, -0.1])
    ds.Close()

    out_dir = tmp_path / "outretile"
    out_dir.mkdir()

    _, err = test_py_scripts.run_py_script(
        script_path,
        "gdal_retile",
        "-ps 1 1 -targetDir " + '"' + str(out_dir) + '"' + " " + src_filename,
        return_stderr=True,
    )
    assert "has a geotransform matrix with rotational terms" in err
    assert len(glob.glob(os.path.join(str(out_dir), "*.tif"))) == 0


###############################################################################
# Test gdal_retile on input files with different projections
# (unsupported)


@pytest.mark.parametrize(
    "srs1,srs2,expected_err",
    [
        (32631, 32632, "has a SRS different from other tiles"),
        (32631, None, "has no SRS whether other tiles have one"),
        (None, 32631, "has a SRS whether other tiles do not"),
    ],
)
def test_gdal_retile_different_srs(script_path, tmp_path, srs1, srs2, expected_err):

    src_filename1 = str(tmp_path / "in1.tif")
    ds = gdal.GetDriverByName("GTiff").Create(src_filename1, 2, 2)
    ds.SetGeoTransform([2, 0.1, 0, 49, 0, -0.1])
    if srs1:
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(srs1)
        ds.SetSpatialRef(srs)
    ds.Close()

    src_filename2 = str(tmp_path / "in2.tif")
    ds = gdal.GetDriverByName("GTiff").Create(src_filename2, 2, 2)
    ds.SetGeoTransform([2, 0.1, 0, 49, 0, -0.1])
    if srs2:
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(srs2)
        ds.SetSpatialRef(srs)
    ds.Close()

    out_dir = tmp_path / "outretile"
    out_dir.mkdir()

    _, err = test_py_scripts.run_py_script(
        script_path,
        "gdal_retile",
        "-ps 1 1 -targetDir "
        + '"'
        + str(out_dir)
        + '"'
        + " "
        + src_filename1
        + " "
        + src_filename2,
        return_stderr=True,
    )
    assert expected_err in err
    assert len(glob.glob(os.path.join(str(out_dir), "*.tif"))) == 0
