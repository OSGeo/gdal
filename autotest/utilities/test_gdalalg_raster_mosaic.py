#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster mosaic' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal, osr


def get_mosaic_alg():
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["mosaic"]


def test_gdalalg_raster_mosaic_from_dataset_handle():

    alg = get_mosaic_alg()
    alg["input"] = [
        gdal.Translate(
            "", "../gcore/data/byte.tif", options="-f MEM -srcwin 0 0 10 20"
        ),
        gdal.Translate(
            "", "../gcore/data/byte.tif", options="-f MEM -srcwin 10 0 10 20"
        ),
    ]
    alg["output-format"] = "stream"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.GetGeoTransform() == pytest.approx(
        (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)
    )
    assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdalalg_raster_mosaic_from_dataset_name():

    alg = get_mosaic_alg()
    alg["input"] = ["../gcore/data/byte.tif"]
    alg["output-format"] = "stream"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.GetGeoTransform() == pytest.approx(
        (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)
    )
    assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdalalg_raster_mosaic_overwrite(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.vrt")

    alg = get_mosaic_alg()
    assert alg.ParseRunAndFinalize(["../gcore/data/utmsmall.tif", out_filename])

    with gdal.Open(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 50054

    alg = get_mosaic_alg()
    with pytest.raises(Exception, match="already exists"):
        alg.ParseRunAndFinalize(["../gcore/data/byte.tif", out_filename])

    alg = get_mosaic_alg()
    assert alg.ParseRunAndFinalize(
        ["--overwrite", "../gcore/data/byte.tif", out_filename]
    )

    with gdal.Open(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdalalg_raster_mosaic_bbox():

    alg = get_mosaic_alg()
    assert alg.ParseCommandLineArguments(
        [
            "--bbox=440780.0,3750180.0,441860.0,3751260.0",
            "../gcore/data/byte.tif",
            "",
            "--of=stream",
        ]
    )
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.RasterXSize == 18
    assert ds.RasterYSize == 18
    assert ds.GetGeoTransform() == pytest.approx(
        (440780.0, 60.0, 0.0, 3751260.0, 0.0, -60.0)
    )
    assert ds.GetRasterBand(1).Checksum() == 3695


def test_gdalalg_raster_mosaic_resolution_average():

    src1_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src1_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    src2_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    src2_ds.SetGeoTransform([3, 0.5, 0, 49, 0, -0.5])

    alg = get_mosaic_alg()
    alg["input"] = [src1_ds, src2_ds]
    alg["resolution"] = "average"
    alg["output-format"] = "stream"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.RasterXSize == 3
    assert ds.RasterYSize == 1
    assert ds.GetGeoTransform() == pytest.approx((2.0, 0.75, 0.0, 49.0, 0.0, -0.75))


def test_gdalalg_raster_mosaic_resolution_highest():

    src1_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src1_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    src2_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    src2_ds.SetGeoTransform([3, 0.5, 0, 49, 0, -0.5])

    alg = get_mosaic_alg()
    alg["input"] = [src1_ds, src2_ds]
    alg["output-format"] = "stream"
    alg["resolution"] = "highest"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.RasterXSize == 4
    assert ds.RasterYSize == 2
    assert ds.GetGeoTransform() == pytest.approx((2.0, 0.5, 0.0, 49.0, 0.0, -0.5))


def test_gdalalg_raster_mosaic_resolution_lowest():

    src1_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src1_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    src2_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    src2_ds.SetGeoTransform([3, 0.5, 0, 49, 0, -0.5])

    alg = get_mosaic_alg()
    alg["input"] = [src1_ds, src2_ds]
    alg["output-format"] = "stream"
    alg["resolution"] = "lowest"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.RasterXSize == 2
    assert ds.RasterYSize == 1
    assert ds.GetGeoTransform() == pytest.approx((2.0, 1.0, 0.0, 49.0, 0.0, -1.0))


def test_gdalalg_raster_mosaic_resolution_common():

    # resolution = 3
    src1_ds = gdal.GetDriverByName("MEM").Create("", 5, 5)
    src1_ds.SetGeoTransform([2, 3, 0, 49, 0, -3])

    # resolution = 5
    src2_ds = gdal.GetDriverByName("MEM").Create("", 3, 3)
    src2_ds.SetGeoTransform([17, 5, 0, 49, 0, -5])

    alg = get_mosaic_alg()
    alg["input"] = [src1_ds, src2_ds]
    alg["output-format"] = "stream"
    alg["resolution"] = "common"
    assert alg.Run()
    ds = alg["output"].GetDataset()

    assert ds.RasterXSize == 30
    assert ds.RasterYSize == 15
    assert ds.GetGeoTransform() == pytest.approx((2.0, 1.0, 0.0, 49.0, 0.0, -1.0))


def test_gdalalg_raster_mosaic_resolution_custom():

    src1_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src1_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    src2_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    src2_ds.SetGeoTransform([3, 0.5, 0, 49, 0, -0.5])

    alg = get_mosaic_alg()
    alg["input"] = [src1_ds, src2_ds]
    alg["output-format"] = "stream"
    alg["resolution"] = "0.5,1"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.RasterXSize == 4
    assert ds.RasterYSize == 1
    assert ds.GetGeoTransform() == pytest.approx((2.0, 0.5, 0.0, 49.0, 0.0, -1.0))


def test_gdalalg_raster_mosaic_target_aligned_pixels():

    src1_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src1_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    src2_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    src2_ds.SetGeoTransform([3, 0.5, 0, 49, 0, -0.5])

    alg = get_mosaic_alg()
    alg["input"] = [src1_ds, src2_ds]
    alg["output-format"] = "stream"
    alg["resolution"] = "0.3,0.6"
    alg["target-aligned-pixels"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.RasterXSize == 8
    assert ds.RasterYSize == 2
    assert ds.GetGeoTransform() == pytest.approx((1.8, 0.3, 0.0, 49.2, 0.0, -0.6))


def test_gdalalg_raster_mosaic_target_aligned_pixels_error():

    src1_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src1_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    src2_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    src2_ds.SetGeoTransform([3, 0.5, 0, 49, 0, -0.5])

    alg = get_mosaic_alg()
    alg["input"] = [src1_ds, src2_ds]
    alg["output-format"] = "stream"
    alg["target-aligned-pixels"] = True
    with pytest.raises(
        Exception,
        match="Argument 'target-aligned-pixels' can only be specified if argument 'resolution' is also specified",
    ):
        alg.Run()


def test_gdalalg_raster_mosaic_resolution_same_default():

    src1_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src1_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    src2_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    src2_ds.SetGeoTransform([3, 0.5, 0, 49, 0, -0.5])

    alg = get_mosaic_alg()
    alg["input"] = [src1_ds, src2_ds]
    alg["output-format"] = "stream"
    with pytest.raises(
        Exception,
        match="whereas previous sources have resolution",
    ):
        assert alg.Run()


def test_gdalalg_raster_mosaic_resolution_invalid():

    alg = get_mosaic_alg()
    with pytest.raises(
        Exception,
        match="resolution: two comma separated positive values should be provided, or ",
    ):
        alg["resolution"] = "invalid"

    with pytest.raises(
        Exception,
        match="resolution: two comma separated positive values should be provided, or ",
    ):
        alg["resolution"] = "0.5"

    with pytest.raises(
        Exception,
        match="resolution: two comma separated positive values should be provided, or ",
    ):
        alg["resolution"] = "-0.5,-0.5"


def test_gdalalg_raster_mosaic_srcnodata_dstnodata():

    src1_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src1_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    src1_ds.GetRasterBand(1).Fill(1)

    alg = get_mosaic_alg()
    alg["input"] = [src1_ds]
    alg["output-format"] = "stream"
    alg["src-nodata"] = [1]
    alg["dst-nodata"] = [2]
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetRasterBand(1).Checksum() == 2
    assert ds.GetRasterBand(1).GetNoDataValue() == 2


def test_gdalalg_raster_mosaic_hidenodata():

    src1_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src1_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    src1_ds.GetRasterBand(1).Fill(1)

    alg = get_mosaic_alg()
    alg["input"] = [src1_ds]
    alg["output-format"] = "stream"
    alg["src-nodata"] = [1]
    alg["dst-nodata"] = [2]
    alg["hide-nodata"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetRasterBand(1).Checksum() == 2
    assert ds.GetRasterBand(1).GetNoDataValue() is None


def test_gdalalg_raster_mosaic_addalpha():

    alg = get_mosaic_alg()
    alg["input"] = ["../gcore/data/rgbsmall.tif"]
    alg["output-format"] = "stream"
    alg["band"] = [3, 2]
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.RasterCount == 2
    assert ds.GetRasterBand(1).Checksum() == 21349
    assert ds.GetRasterBand(2).Checksum() == 21053


def test_gdalalg_raster_mosaic_glob():

    alg = get_mosaic_alg()
    alg["input"] = ["../gcore/data/rgbsm?ll.tif"]
    alg["output-format"] = "stream"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.RasterCount == 3


def test_gdalalg_raster_mosaic_at_filename(tmp_vsimem):

    input_file_list = str(tmp_vsimem / "tmp.txt")
    gdal.FileFromMemBuffer(input_file_list, "../gcore/data/byte.tif")

    alg = get_mosaic_alg()
    alg["input"] = [f"@{input_file_list}"]
    alg["output-format"] = "stream"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdalalg_raster_mosaic_at_filename_error():

    alg = get_mosaic_alg()
    alg["input"] = ["@i_do_not_exist"]
    alg["output-format"] = "stream"
    with pytest.raises(Exception, match="mosaic: Cannot open i_do_not_exist"):
        alg.Run()


def test_gdalalg_raster_mosaic_co(tmp_vsimem):

    alg = get_mosaic_alg()
    alg["input"] = ["../gcore/data/byte.tif"]
    alg["output"] = tmp_vsimem / "out.tif"
    alg["creation-option"] = ["BLOCKYSIZE=10"]
    assert alg.Run() and alg.Finalize()
    with gdal.Open(tmp_vsimem / "out.tif") as ds:
        assert ds.GetRasterBand(1).GetBlockSize() == [20, 10]


def test_gdalalg_raster_mosaic_tif_output_implicit(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    alg = get_mosaic_alg()
    assert alg.ParseRunAndFinalize(["../gcore/data/utmsmall.tif", out_filename])

    with gdal.Open(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 50054


def test_gdalalg_raster_mosaic_tif_output_explicit(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.xxx")

    alg = get_mosaic_alg()
    assert alg.ParseRunAndFinalize(
        ["--of=GTiff", "../gcore/data/utmsmall.tif", out_filename]
    )

    with gdal.Open(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 50054


def test_gdalalg_raster_mosaic_tif_creation_options(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.xxx")

    alg = get_mosaic_alg()
    assert alg.ParseRunAndFinalize(
        ["--of=GTiff", "--co=TILED=YES", "../gcore/data/utmsmall.tif", out_filename]
    )

    with gdal.Open(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 50054
        assert ds.GetRasterBand(1).GetBlockSize() == [256, 256]


def test_gdalalg_raster_mosaic_inconsistent_characteristics():

    src1_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src1_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    src2_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    src2_ds.SetGeoTransform([3, 0.5, 0, 49, 0, -0.5])
    srs = osr.SpatialReference()
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    srs.SetFromUserInput("WGS84")
    src2_ds.SetSpatialRef(srs)

    alg = get_mosaic_alg()
    alg["input"] = [src1_ds, src2_ds]
    alg["output-format"] = "stream"
    with pytest.raises(
        Exception, match="gdal raster mosaic does not support heterogeneous projection"
    ):
        assert alg.Run()


def test_gdalalg_raster_mosaic_abolute_path(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.vrt")

    gdal.Translate(tmp_vsimem / "byte.tif", "../gcore/data/byte.tif")

    gdal.Run(
        "raster",
        "mosaic",
        input=tmp_vsimem / "byte.tif",
        output=out_filename,
        absolute_path=True,
    )

    with gdal.Open(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672

    with gdal.VSIFile(out_filename, "rb") as f:
        content = f.read().decode("utf-8")
    assert (
        '<SourceFilename relativeToVRT="0">' + str(tmp_vsimem / "byte.tif") in content
    )
    assert '<SourceFilename relativeToVRT="1">byte.tif' not in content


@pytest.mark.parametrize("pixfn,args", [("sum", ["k=4"]), ("min", [])])
def test_gdalalg_raster_mosaic_pixel_function(pixfn, args):

    gdaltest.importorskip_gdal_array()
    np = pytest.importorskip("numpy")

    src1_ds = gdal.GetDriverByName("MEM").Create("", 3, 1, eType=gdal.GDT_Int16)
    src1_ds.SetGeoTransform([0, 1, 0, 1, 0, -1])
    src1_ds.GetRasterBand(1).Fill(1)

    src2_ds = gdal.GetDriverByName("MEM").Create("", 3, 1, eType=gdal.GDT_Int16)
    src2_ds.SetGeoTransform([1, 1, 0, 1, 0, -1])
    src2_ds.GetRasterBand(1).Fill(2)

    alg = get_mosaic_alg()
    alg["input"] = [src1_ds, src2_ds]
    alg["output-format"] = "stream"
    alg["dst-nodata"] = [-999]
    alg["pixel-function"] = pixfn
    alg["pixel-function-arg"] = args
    assert alg.Run()
    ds = alg["output"].GetDataset()

    dst_values = ds.ReadAsArray()
    if pixfn == "sum":
        np.testing.assert_array_equal(dst_values, np.array([[1, 3, 3, 2]]) + 4)
    elif pixfn == "min":
        np.testing.assert_array_equal(dst_values, np.array([[1, 1, 1, 2]]))
    else:
        pytest.fail("Unexpected pixel function")

    assert alg.Finalize()


def test_gdalalg_raster_mosaic_pixel_function_invalid():

    alg = get_mosaic_alg()
    with pytest.raises(
        RuntimeError,
        match="Invalid value 'does_not_exist' for string argument 'pixel-function'",
    ):
        alg["pixel-function"] = "does_not_exist"


def test_gdalalg_raster_mosaic_pixel_function_arg_invalid():

    alg = get_mosaic_alg()
    with pytest.raises(
        RuntimeError,
        match="Invalid value for argument 'pixel-function-arg'. <KEY>=<VALUE> expected",
    ):
        alg["pixel-function-arg"] = "key_without_value"


def test_gdalalg_raster_mosaic_pixel_function_arg_complete():
    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster mosaic --pixel-function-arg"
    )
    assert (
        "Specify argument(s) to pass to the pixel function".replace(" ", "\\ ") in out
    )

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster mosaic --pixel-function=invalid --pixel-function-arg"
    )
    assert "Invalid pixel function name".replace(" ", "\\ ") in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster mosaic --pixel-function=scale --pixel-function-arg"
    )
    assert "No pixel function arguments for pixel function".replace(" ", "\\ ") in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster mosaic --pixel-function=mean --pixel-function-arg"
    )
    assert out == "propagateNoData="

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster mosaic --pixel-function=mean --pixel-function-arg propagateNoData="
    ).split(" ")
    assert out == ["NO", "YES"]

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster mosaic --pixel-function=mean --pixel-function-arg propagateNoData=YES"
    )
    assert out == ""


def test_gdalalg_raster_mosaic_pipeline():

    src1_ds = gdal.GetDriverByName("MEM").Create("", 3, 1, eType=gdal.GDT_Int16)
    src1_ds.SetGeoTransform([0, 1, 0, 1, 0, -1])
    src1_ds.GetRasterBand(1).Fill(1)

    src2_ds = gdal.GetDriverByName("MEM").Create("", 3, 1, eType=gdal.GDT_Int16)
    src2_ds.SetGeoTransform([1, 1, 0, 1, 0, -1])
    src2_ds.GetRasterBand(1).Fill(2)

    with gdal.Run(
        "raster",
        "pipeline",
        pipeline="mosaic ! write --of=stream streamed_output",
        input=[src1_ds, src2_ds],
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.GetRasterBand(1).Checksum() == 7
