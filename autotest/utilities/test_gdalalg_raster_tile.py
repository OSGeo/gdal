#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster tile' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal, osr

pytestmark = pytest.mark.require_driver("PNG")


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["tile"]


@pytest.mark.parametrize("tiling_scheme,tilesize", [(None, None), ("mercator", 256)])
def test_gdalalg_raster_tile_basic(tmp_vsimem, tiling_scheme, tilesize):

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    alg = get_alg()
    alg["input"] = gdal.Translate(
        "", "../gcore/data/byte.tif", format="MEM", outputSRS="EPSG:32611"
    )
    alg["output"] = tmp_vsimem
    if tiling_scheme:
        alg["tiling-scheme"] = tiling_scheme
    if tilesize:
        alg["tile-size"] = tilesize
    alg["url"] = "http://example.com"
    alg["title"] = "my title"
    with gdal.config_option("GDAL_RASTER_TILE_HTML_PREC", "10"):
        assert alg.Run(my_progress)

    assert last_pct[0] == 1.0

    assert gdal.ReadDirRecursive(tmp_vsimem) == [
        "11/",
        "11/354/",
        "11/354/818.png",
        "leaflet.html",
        "mapml.mapml",
        "openlayers.html",
    ]

    with gdal.Open(tmp_vsimem / "11/354/818.png") as ds:
        assert ds.RasterCount == 2
        assert ds.RasterXSize == 256
        assert ds.RasterYSize == 256
        assert [ds.GetRasterBand(i + 1).Checksum() for i in range(2)] == [4160, 4415]

    with gdal.VSIFile(tmp_vsimem / "leaflet.html", "rb") as f:
        got = f.read()
        # Uncomment below line to regenerate expected file
        # open("data/gdal_raster_tile_expected_leaflet.html", "wb").write(got)
        assert got == open("data/gdal_raster_tile_expected_leaflet.html", "rb").read()

    if tiling_scheme is None:
        with gdal.VSIFile(tmp_vsimem / "mapml.mapml", "rb") as f:
            got = f.read()
            # Uncomment below line to regenerate expected file
            # open("data/gdal_raster_tile_expected_mapml.mapml", "wb").write(got)
            assert (
                got == open("data/gdal_raster_tile_expected_mapml.mapml", "rb").read()
            )

    with gdal.VSIFile(tmp_vsimem / "openlayers.html", "rb") as f:
        got = f.read()
        # Uncomment below line to regenerate expected file
        # open("data/gdal_raster_tile_expected_openlayers.html", "wb").write(got)
        assert (
            got == open("data/gdal_raster_tile_expected_openlayers.html", "rb").read()
        )


@pytest.mark.parametrize(
    "tiling_scheme,xyz,addalpha",
    [("WorldCRS84Quad", True, True), ("geodetic", False, False)],
)
def test_gdalalg_raster_tile_small_world_geodetic(
    tmp_vsimem, tiling_scheme, xyz, addalpha
):

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_vsimem
    alg["tiling-scheme"] = tiling_scheme
    if not xyz:
        alg["convention"] = "tms"
    if addalpha:
        alg["add-alpha"] = True
        nbands = 4
    else:
        nbands = 3
    with gdal.config_option("GDAL_RASTER_TILE_HTML_PREC", "10"):
        assert alg.Run()

    assert (
        gdal.ReadDirRecursive(tmp_vsimem)
        == [
            "0/",
            "0/0/",
            "0/0/0.png",
            "0/1/",
            "0/1/0.png",
            "mapml.mapml",
            "openlayers.html",
        ]
        if xyz
        else [
            "0/",
            "0/0/",
            "0/0/0.png",
            "0/1/",
            "0/1/0.png",
            "openlayers.html",
        ]
    )

    with gdal.Open(tmp_vsimem / "0/0/0.png") as ds:
        assert ds.RasterCount == nbands
        assert ds.RasterXSize == 256
        assert ds.RasterYSize == 256
        assert [
            ds.GetRasterBand(i + 1).Checksum() for i in range(nbands)
        ] == pytest.approx(
            [
                1315,
                63955,
                5106,
                17849,
            ][0:nbands],
            abs=1,
        )

    with gdal.Open(tmp_vsimem / "0/1/0.png") as ds:
        assert ds.RasterCount == nbands
        assert ds.RasterXSize == 256
        assert ds.RasterYSize == 256
        assert [
            ds.GetRasterBand(i + 1).Checksum() for i in range(nbands)
        ] == pytest.approx(
            [
                24456,
                25846,
                15674,
                17849,
            ][0:nbands],
            abs=1,
        )

    if xyz:
        with gdal.VSIFile(tmp_vsimem / "mapml.mapml", "rb") as f:
            got = f.read()
            # Uncomment below line to regenerate expected file
            # open("data/gdal_raster_tile_expected_geodetic_mapml.mapml", "wb").write(got)
            assert (
                got
                == open(
                    "data/gdal_raster_tile_expected_geodetic_mapml.mapml", "rb"
                ).read()
            )

    with gdal.VSIFile(tmp_vsimem / "openlayers.html", "rb") as f:
        got = f.read()
        # Uncomment below line to regenerate expected file
        # open(f"data/gdal_raster_tile_expected_openlayers_geodetic_{xyz}.html", "wb").write(got)
        assert (
            got
            == open(
                f"data/gdal_raster_tile_expected_openlayers_geodetic_{xyz}.html", "rb"
            ).read()
        )


def test_gdalalg_raster_tile_invalid_output_directory():

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = "/i_do/not/exist"
    with pytest.raises(
        Exception, match="Cannot create output directory /i_do/not/exist"
    ):
        alg.Run()


@pytest.mark.parametrize("xyz", [True, False])
def test_gdalalg_raster_tile_small_world_GoogleCRS84Quad(tmp_vsimem, xyz):

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_vsimem
    alg["tiling-scheme"] = "GoogleCRS84Quad"
    if not xyz:
        alg["convention"] = "tms"
    alg["webviewer"] = ["openlayers", "leaflet"]
    assert alg.Run()

    assert gdal.ReadDirRecursive(tmp_vsimem) == [
        "1/",
        "1/0/",
        "1/0/0.png",
        "1/0/1.png",
        "1/1/",
        "1/1/0.png",
        "1/1/1.png",
        "openlayers.html",
    ]


def test_gdalalg_raster_tile_invalid_tile_min_max_x(tmp_vsimem):

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_vsimem
    alg["min-x"] = 1
    alg["max-x"] = 0
    with pytest.raises(Exception, match="'min-x' must be lesser or equal to 'max-x'"):
        alg.Run()


def test_gdalalg_raster_tile_invalid_tile_min_max_y(tmp_vsimem):

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_vsimem
    alg["min-y"] = 1
    alg["max-y"] = 0
    with pytest.raises(Exception, match="'min-y' must be lesser or equal to 'max-y'"):
        alg.Run()


def test_gdalalg_raster_tile_invalid_tile_min_x(tmp_vsimem):

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_vsimem
    alg["tiling-scheme"] = "GoogleCRS84Quad"
    alg["min-x"] = 2
    with pytest.raises(Exception, match=r"'min-x' value must be in \[0,1\] range"):
        alg.Run()


def test_gdalalg_raster_tile_invalid_tile_min_y(tmp_vsimem):

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_vsimem
    alg["tiling-scheme"] = "GoogleCRS84Quad"
    alg["min-y"] = 2
    with pytest.raises(Exception, match=r"'min-y' value must be in \[0,1\] range"):
        alg.Run()


def test_gdalalg_raster_tile_invalid_tile_max_x(tmp_vsimem):

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_vsimem
    alg["tiling-scheme"] = "GoogleCRS84Quad"
    alg["max-x"] = 2
    with pytest.raises(Exception, match=r"'max-x' value must be in \[0,1\] range"):
        alg.Run()


def test_gdalalg_raster_tile_invalid_tile_max_y(tmp_vsimem):

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_vsimem
    alg["tiling-scheme"] = "GoogleCRS84Quad"
    alg["max-y"] = 2
    with pytest.raises(Exception, match=r"'max-y' value must be in \[0,1\] range"):
        alg.Run()


@pytest.mark.parametrize(
    "min_x,min_y,max_x,max_y,expected_files",
    [
        (
            0,
            0,
            1,
            1,
            ["1/", "1/0/", "1/0/0.png", "1/0/1.png", "1/1/", "1/1/0.png", "1/1/1.png"],
        ),
        (
            None,
            0,
            1,
            1,
            ["1/", "1/0/", "1/0/0.png", "1/0/1.png", "1/1/", "1/1/0.png", "1/1/1.png"],
        ),
        (
            0,
            None,
            1,
            1,
            ["1/", "1/0/", "1/0/0.png", "1/0/1.png", "1/1/", "1/1/0.png", "1/1/1.png"],
        ),
        (
            0,
            0,
            None,
            1,
            ["1/", "1/0/", "1/0/0.png", "1/0/1.png", "1/1/", "1/1/0.png", "1/1/1.png"],
        ),
        (
            0,
            0,
            1,
            None,
            ["1/", "1/0/", "1/0/0.png", "1/0/1.png", "1/1/", "1/1/0.png", "1/1/1.png"],
        ),
        (0, 0, 0, 0, ["1/", "1/0/", "1/0/0.png"]),
        (0, 0, 1, 0, ["1/", "1/0/", "1/0/0.png", "1/1/", "1/1/0.png"]),
        (0, 0, 0, 1, ["1/", "1/0/", "1/0/0.png", "1/0/1.png"]),
        (1, 0, 1, 1, ["1/", "1/1/", "1/1/0.png", "1/1/1.png"]),
        (1, 1, 1, 1, ["1/", "1/1/", "1/1/1.png"]),
    ],
)
def test_gdalalg_raster_tile_min_max_xy_coordinate(
    tmp_vsimem, min_x, min_y, max_x, max_y, expected_files
):

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_vsimem
    alg["tiling-scheme"] = "GoogleCRS84Quad"
    alg["webviewer"] = "none"
    if min_x is not None:
        alg["min-x"] = min_x
    if min_y is not None:
        alg["min-y"] = min_y
    if max_x is not None:
        alg["max-x"] = max_x
    if max_y is not None:
        alg["max-y"] = max_y
    assert alg.Run()

    assert gdal.ReadDirRecursive(tmp_vsimem) == expected_files


def test_gdalalg_raster_tile_min_max_xy_coordinate_not_intersecting(tmp_vsimem):

    alg = get_alg()
    # West hemisphere
    alg["input"] = gdal.Translate(
        "", "../gdrivers/data/small_world.tif", options="-of MEM -srcwin 0 0 199 200"
    )
    alg["output"] = tmp_vsimem
    alg["tiling-scheme"] = "GoogleCRS84Quad"
    alg["min-x"] = 1
    with pytest.raises(
        Exception,
        match="Dataset extent not intersecting specified min/max X/Y tile coordinates",
    ):
        alg.Run()


def test_gdalalg_raster_tile_min_max_xy_coordinate_not_intersecting_ok(tmp_vsimem):

    alg = get_alg()
    # West hemisphere
    alg["input"] = gdal.Translate(
        "", "../gdrivers/data/small_world.tif", options="-of MEM -srcwin 0 0 199 200"
    )
    alg["output"] = tmp_vsimem
    alg["tiling-scheme"] = "GoogleCRS84Quad"
    alg["min-x"] = 1
    alg["no-intersection-ok"] = True
    with gdaltest.error_raised(
        gdal.CE_Warning,
        match="Dataset extent not intersecting specified min/max X/Y tile coordinates",
    ):
        assert alg.Run()
    assert gdal.ReadDirRecursive(tmp_vsimem) is None


@pytest.mark.parametrize(
    "resampling,overview_resampling,cs_11,cs_10",
    [
        (None, None, 4160, 1191),
        ("cubic", None, 4160, 1191),
        (None, "cubic", 4160, 1191),
        ("cubic", "cubic", 4160, 1191),
        ("cubic", "near", 4160, 1209),
        ("cubic", "q1", 4160, 1281),  # q1 for overview go throw warp code
        ("near", "cubic", 4217, 1223),
        ("q1", None, 4896, 1228),
        ("q1", "q1", 4896, 1228),
    ],
)
def test_gdalalg_raster_tile_nodata_and_resampling(
    tmp_path, resampling, overview_resampling, cs_11, cs_10
):

    alg = get_alg()
    alg["input"] = gdal.Translate(
        "", "../gcore/data/byte.tif", format="MEM", outputSRS="EPSG:32611", noData=0
    )
    alg["output"] = tmp_path
    alg["output-format"] = "GTiff"
    alg["min-zoom"] = 10
    alg["skip-blank"] = True
    alg["webviewer"] = "none"
    if resampling:
        alg["resampling"] = resampling
    if overview_resampling:
        alg["overview-resampling"] = overview_resampling
    assert alg.Run()

    assert set([x.replace("\\", "/") for x in gdal.ReadDirRecursive(tmp_path)]) == set(
        [
            "10/",
            "10/177/",
            "10/177/409.tif",
            "11/",
            "11/354/",
            "11/354/818.tif",
        ]
    )

    with gdal.Open(tmp_path / "11" / "354" / "818.tif") as ds:
        assert ds.RasterCount == 1
        assert ds.RasterXSize == 256
        assert ds.RasterYSize == 256
        assert ds.GetRasterBand(1).Checksum() == cs_11
        assert ds.GetRasterBand(1).GetNoDataValue() == 0

    with gdal.Open(tmp_path / "10" / "177" / "409.tif") as ds:
        assert ds.RasterCount == 1
        assert ds.RasterXSize == 256
        assert ds.RasterYSize == 256
        assert ds.GetRasterBand(1).Checksum() == cs_10
        assert ds.GetRasterBand(1).GetNoDataValue() == 0


def test_gdalalg_raster_tile_skip_blank(tmp_path):

    alg = get_alg()
    alg["input"] = gdal.Translate(
        "",
        "../gcore/data/byte.tif",
        format="MEM",
        outputSRS="EPSG:32611",
        scaleParams=[[0, 1, 0, 0]],
        noData=0,
    )
    alg["output"] = tmp_path
    alg["min-zoom"] = 10
    alg["skip-blank"] = True
    alg["webviewer"] = "none"
    assert alg.Run()

    assert set([x.replace("\\", "/") for x in gdal.ReadDirRecursive(tmp_path)]) == set(
        [
            "10/",
            "10/177/",
        ]
    )


def test_gdalalg_raster_tile_invalid_output_format():

    alg = get_alg()
    with pytest.raises(
        Exception,
        match="Invalid value for argument 'output-format'. Driver 'MEM' does not advertise any file format extension",
    ):
        alg["output-format"] = "MEM"


def test_gdalalg_raster_tile_invalid_output_format_vrt():

    alg = get_alg()
    with pytest.raises(Exception, match="VRT output is not supported"):
        alg["output-format"] = "VRT"


def test_gdalalg_raster_tile_png_not_available(tmp_vsimem):

    drv = gdal.GetDriverByName("PNG")
    drv.Deregister()
    try:
        alg = get_alg()
        alg["input"] = "../gcore/data/byte.tif"
        alg["output"] = tmp_vsimem
        with pytest.raises(
            Exception,
            match="Invalid value for argument 'output-format'. Driver 'PNG' does not exist",
        ):
            alg.Run()
    finally:
        drv.Register()


def test_gdalalg_raster_tile_invalid_input(tmp_vsimem):

    alg = get_alg()
    alg["input"] = gdal.GetDriverByName("MEM").Create("", 1, 1, 0)
    alg["output"] = tmp_vsimem
    with pytest.raises(Exception, match="Invalid source dataset"):
        alg.Run()


def test_gdalalg_raster_tile_palette_not_supported(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    src_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_PaletteIndex)
    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    with pytest.raises(
        Exception,
        match="Datasets with color table not supported with non-nearest or non-mode resampling. Run 'gdal raster color-map' before or set the 'resampling' argument to 'nearest' or 'mode'",
    ):
        alg.Run()


def test_gdalalg_raster_tile_palette_nearest(tmp_vsimem):

    alg = get_alg()
    alg["input"] = "data/byte_pct.tif"
    alg["output"] = tmp_vsimem
    alg["resampling"] = "nearest"
    alg["min-zoom"] = 10
    alg["max-zoom"] = 11
    assert alg.Run()

    assert gdal.ReadDirRecursive(tmp_vsimem) == [
        "10/",
        "10/177/",
        "10/177/409.png",
        "11/",
        "11/354/",
        "11/354/818.png",
        "leaflet.html",
        "mapml.mapml",
        "openlayers.html",
    ]

    with gdal.Open(tmp_vsimem / "11/354/818.png") as ds:
        assert ds.GetRasterBand(1).GetColorTable() is not None

    with gdal.Open(tmp_vsimem / "10/177/409.png") as ds:
        assert ds.GetRasterBand(1).GetColorTable() is not None


def test_gdalalg_raster_tile_min_zoom_larger_max_zoom(tmp_vsimem):

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = tmp_vsimem
    alg["min-zoom"] = 1
    alg["max-zoom"] = 0
    with pytest.raises(
        Exception, match="'min-zoom' must be lesser or equal to 'max-zoom'"
    ):
        alg.Run()


def test_gdalalg_raster_tile_too_large_max_zoom(tmp_vsimem):

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = tmp_vsimem
    alg["max-zoom"] = 31
    with pytest.raises(
        Exception, match=r"max-zoom = 31 is invalid. It must be in \[0,30\] range"
    ):
        alg.Run()


def test_gdalalg_raster_tile_too_large_min_zoom(tmp_vsimem):

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = tmp_vsimem
    alg["min-zoom"] = 31
    with pytest.raises(
        Exception,
        match="Could not find an appropriate zoom level. Perhaps min-zoom is too large?",
    ):
        alg.Run()


def test_gdalalg_raster_tile_too_large_virtual_daaset(tmp_vsimem):

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_vsimem
    alg["max-zoom"] = 30
    with pytest.raises(
        Exception,
        match="Too large zoom level",
    ):
        alg.Run()


@pytest.mark.parametrize(
    "output_format,datatype,nbands,message",
    [
        ("PNG", gdal.GDT_Byte, 5, "Only up to 4 bands supported for PNG"),
        ("PNG", gdal.GDT_Int32, 1, "Only Byte and UInt16 data types supported for PNG"),
        ("JPEG", gdal.GDT_Byte, 5, "Only up to 4 bands supported for JPEG"),
        ("JPEG", gdal.GDT_Int32, 1, "Only Byte"),
        ("WEBP", gdal.GDT_Byte, 1, "Only 3 or 4 bands supported for WEBP"),
        ("WEBP", gdal.GDT_UInt16, 3, "Only Byte data type supported for WEBP"),
        (
            "GTX",
            gdal.GDT_Byte,
            1,
            "Attempt to create gtx file with unsupported data type 'Byte'",
        ),
    ],
)
def test_gdalalg_raster_tile_input_not_supported_by_output(
    tmp_vsimem, output_format, datatype, nbands, message
):

    if gdal.GetDriverByName(output_format) is None:
        pytest.skip(f"{output_format} driver not available")

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, nbands, datatype)
    src_ds.SetSpatialRef(osr.SpatialReference("+proj=longlat +datum=WGS84"))
    src_ds.SetGeoTransform([-180, 360, 0, 90, 0, -180])
    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    alg["output-format"] = output_format
    with pytest.raises(Exception, match=message):
        alg.Run()


@pytest.mark.require_driver("GTX")
def test_gdalalg_raster_tile_input_not_supported_by_output_multithread(tmp_vsimem):

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_vsimem
    alg["output-format"] = "GTX"
    alg["max-zoom"] = 2
    with pytest.raises(
        Exception, match="Attempt to create gtx file with unsupported data type 'Byte'"
    ):
        alg.Run()


def test_gdalalg_raster_tile_multithread(tmp_vsimem):

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_vsimem
    alg["min-zoom"] = 0
    alg["max-zoom"] = 3
    alg.Run()

    assert len(gdal.ReadDirRecursive(tmp_vsimem)) == 107


def test_gdalalg_raster_tile_multithread_progress(tmp_vsimem):

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_vsimem
    alg["min-zoom"] = 0
    alg["max-zoom"] = 3
    alg.Run(my_progress)

    assert last_pct[0] == 1.0

    assert len(gdal.ReadDirRecursive(tmp_vsimem)) == 107


def check_12_bit_jpeg():

    if "UInt16" not in gdal.GetDriverByName("JPEG").GetMetadataItem(
        gdal.DMD_CREATIONDATATYPES
    ):
        pytest.skip("12bit JPEG not available")


@pytest.mark.require_driver("JPEG")
def test_gdalalg_raster_tile_jpeg_12_bit_ok(tmp_vsimem):

    check_12_bit_jpeg()

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_UInt16)
    src_ds.GetRasterBand(1).Fill(4095)
    src_ds.SetSpatialRef(osr.SpatialReference("+proj=longlat +datum=WGS84"))
    src_ds.SetGeoTransform([-180, 360, 0, 90, 0, -180])
    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    alg["output-format"] = "JPEG"
    alg["webviewer"] = "none"
    assert alg.Run()

    with gdal.Open(tmp_vsimem / "0/0/0.jpg") as ds:
        assert ds.RasterCount == 1
        assert ds.RasterXSize == 256
        assert ds.RasterYSize == 256
        assert ds.GetRasterBand(1).ComputeRasterMinMax() == (4095, 4095)


@pytest.mark.require_driver("JPEG")
def test_gdalalg_raster_tile_jpeg_12_bit_too_large_values(tmp_vsimem):

    check_12_bit_jpeg()

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_UInt16)
    src_ds.GetRasterBand(1).Fill(4096)
    src_ds.SetSpatialRef(osr.SpatialReference("+proj=longlat +datum=WGS84"))
    src_ds.SetGeoTransform([-180, 360, 0, 90, 0, -180])
    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    alg["output-format"] = "JPEG"
    with pytest.raises(Exception, match="JPEG output only supported up to 12 bits"):
        alg.Run()


@pytest.mark.require_driver("JPEG")
def test_gdalalg_raster_tile_jpeg_12_bit_too_large_nbits(tmp_vsimem):

    check_12_bit_jpeg()

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_UInt16)
    src_ds.GetRasterBand(1).SetMetadataItem("NBITS", "13", "IMAGE_STRUCTURE")
    src_ds.SetSpatialRef(osr.SpatialReference("+proj=longlat +datum=WGS84"))
    src_ds.SetGeoTransform([-180, 360, 0, 90, 0, -180])
    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    alg["output-format"] = "JPEG"
    with pytest.raises(Exception, match="JPEG output only supported up to 12 bits"):
        alg.Run()


def test_gdalalg_raster_tile_missing_gt(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetSpatialRef(osr.SpatialReference(epsg=32600))
    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    with pytest.raises(Exception, match="Ungeoreferenced datasets are not supported"):
        alg.Run()


def test_gdalalg_raster_tile_missing_srs(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    with pytest.raises(Exception, match="Ungeoreferenced datasets are not supported"):
        alg.Run()


def test_gdalalg_raster_tile_not_earth_crs(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetSpatialRef(osr.SpatialReference("+proj=longlat +a=1000"))
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    with pytest.raises(
        Exception,
        match="Source and target ellipsoid do not belong to the same celestial body",
    ):
        alg.Run()


def test_gdalalg_raster_tile_extent_not_compatible_tile_matrix(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetSpatialRef(osr.SpatialReference("+proj=longlat +datum=WGS84"))
    src_ds.SetGeoTransform([-180, 360, 0, 90, 0, -0.001])
    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    alg["tiling-scheme"] = "NZTM2000"
    with pytest.raises(
        Exception,
        match="Extent of source dataset is not compatible with extent of tile matrix 7",
    ):
        alg.Run()


def test_gdalalg_raster_tile_extent_not_compatible_tile_matrix_as_warning(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetSpatialRef(osr.SpatialReference("+proj=longlat +datum=WGS84"))
    src_ds.SetGeoTransform([-180, 360, 0, 90, 0, -0.001])
    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    alg["tiling-scheme"] = "NZTM2000"
    alg["no-intersection-ok"] = True
    with gdaltest.error_raised(
        gdal.CE_Warning,
        match="Extent of source dataset is not compatible with extent of tile matrix 7",
    ):
        assert alg.Run()


def test_gdalalg_raster_tile_addalpha_dstnodata_exclusive(tmp_vsimem):

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = tmp_vsimem
    alg["add-alpha"] = True
    alg["dst-nodata"] = 0
    with pytest.raises(
        Exception, match="'add-alpha' and 'dst-nodata' are mutually exclusive"
    ):
        alg.Run()


def test_gdalalg_raster_tile_rgb(tmp_vsimem):

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_vsimem
    alg["webviewer"] = "none"
    assert alg.Run()

    assert gdal.ReadDirRecursive(tmp_vsimem) == ["0/", "0/0/", "0/0/0.png"]

    with gdal.Open(tmp_vsimem / "0/0/0.png") as ds:
        assert ds.RasterCount == 3
        assert ds.RasterXSize == 256
        assert ds.RasterYSize == 256
        assert [ds.GetRasterBand(i + 1).Checksum() for i in range(3)] == [
            24650,
            23280,
            16559,
        ]


def test_gdalalg_raster_tile_rgba_all_opaque(tmp_vsimem):

    src_ds = gdal.Translate(
        "",
        "../gdrivers/data/small_world.tif",
        options="-of MEM -b 1 -b 2 -b 3 -b mask -colorinterp_4 alpha",
    )
    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    alg["webviewer"] = "none"
    alg["min-zoom"] = 0
    alg["max-zoom"] = 1
    assert alg.Run()

    assert gdal.ReadDirRecursive(tmp_vsimem) == [
        "0/",
        "0/0/",
        "0/0/0.png",
        "1/",
        "1/0/",
        "1/0/0.png",
        "1/0/1.png",
        "1/1/",
        "1/1/0.png",
        "1/1/1.png",
    ]

    with gdal.Open(tmp_vsimem / "0/0/0.png") as ds:
        assert ds.RasterCount == 3
        assert ds.RasterXSize == 256
        assert ds.RasterYSize == 256
        assert [ds.GetRasterBand(i + 1).Checksum() for i in range(3)] == pytest.approx(
            [
                25111,
                24737,
                16107,
            ],
            abs=10,
        )

    with gdal.Open(tmp_vsimem / "1/0/0.png") as ds:
        assert ds.RasterCount == 3
        assert ds.RasterXSize == 256
        assert ds.RasterYSize == 256
        assert [ds.GetRasterBand(i + 1).Checksum() for i in range(3)] == pytest.approx(
            [
                6241,
                4330,
                13703,
            ],
            abs=10,
        )


@pytest.mark.parametrize("skip_blank", [True, False])
def test_gdalalg_raster_tile_rgba_partially_opaque(tmp_vsimem, skip_blank):

    src_ds = gdal.Translate(
        "",
        "../gdrivers/data/small_world.tif",
        options="-of MEM -b 1 -b 2 -b 3 -b mask -colorinterp_4 alpha",
    )
    src_ds.GetRasterBand(4).WriteRaster(0, 0, 10, 10, b"\x00" * 100)
    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    alg["webviewer"] = "none"
    alg["min-zoom"] = 0
    alg["max-zoom"] = 1
    alg["skip-blank"] = skip_blank
    assert alg.Run()

    assert gdal.ReadDirRecursive(tmp_vsimem) == [
        "0/",
        "0/0/",
        "0/0/0.png",
        "1/",
        "1/0/",
        "1/0/0.png",
        "1/0/1.png",
        "1/1/",
        "1/1/0.png",
        "1/1/1.png",
    ]

    with gdal.Open(tmp_vsimem / "0/0/0.png") as ds:
        assert ds.RasterCount == 4
        assert ds.RasterXSize == 256
        assert ds.RasterYSize == 256
        assert [ds.GetRasterBand(i + 1).Checksum() for i in range(4)] == pytest.approx(
            [
                23761,
                23390,
                14544,
                16124,
            ],
            abs=10,
        )

    with gdal.Open(tmp_vsimem / "1/0/0.png") as ds:
        assert ds.RasterCount == 4
        assert ds.RasterXSize == 256
        assert ds.RasterYSize == 256
        assert [ds.GetRasterBand(i + 1).Checksum() for i in range(4)] == pytest.approx(
            [
                289,
                63903,
                6812,
                10052,
            ],
            abs=10,
        )


def test_gdalalg_raster_tile_rgba_all_transparent_skip_blank(tmp_vsimem):

    src_ds = gdal.Translate(
        "",
        "../gdrivers/data/small_world.tif",
        options="-of MEM -b 1 -b 2 -b 3 -b mask -colorinterp_4 alpha",
    )
    src_ds.GetRasterBand(4).Fill(0)
    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    alg["webviewer"] = "none"
    alg["min-zoom"] = 0
    alg["max-zoom"] = 1
    alg["skip-blank"] = True
    assert alg.Run()

    assert gdal.ReadDirRecursive(tmp_vsimem) == ["0/", "0/0/"]


def test_gdalalg_raster_tile_no_alpha(tmp_vsimem):

    alg = get_alg()
    alg["input"] = gdal.Translate(
        "", "../gcore/data/byte.tif", format="MEM", outputSRS="EPSG:32611"
    )
    alg["output"] = tmp_vsimem
    alg["no-alpha"] = True
    alg["webviewer"] = "none"
    assert alg.Run()

    assert gdal.ReadDirRecursive(tmp_vsimem) == ["11/", "11/354/", "11/354/818.png"]

    with gdal.Open(tmp_vsimem / "11/354/818.png") as ds:
        assert ds.RasterCount == 1
        assert ds.RasterXSize == 256
        assert ds.RasterYSize == 256
        assert ds.GetRasterBand(1).Checksum() == 4160


def test_gdalalg_raster_tile_rgba_no_alpha(tmp_vsimem):

    alg = get_alg()
    alg["input"] = gdal.Translate(
        "",
        "../gdrivers/data/small_world.tif",
        options="-of MEM -b 1 -b 2 -b 3 -b mask -colorinterp_4 alpha",
    )
    alg["output"] = tmp_vsimem
    alg["no-alpha"] = True
    alg["webviewer"] = "none"
    assert alg.Run()

    assert gdal.ReadDirRecursive(tmp_vsimem) == ["0/", "0/0/", "0/0/0.png"]

    with gdal.Open(tmp_vsimem / "0/0/0.png") as ds:
        assert ds.RasterCount == 3
        assert ds.RasterXSize == 256
        assert ds.RasterYSize == 256
        assert [ds.GetRasterBand(i + 1).Checksum() for i in range(3)] == [
            24650,
            23280,
            16559,
        ]


def test_gdalalg_raster_tile_max_zoom(tmp_vsimem):

    src_ds = gdal.Translate(
        "", "../gcore/data/byte.tif", format="MEM", outputSRS="EPSG:32611"
    )

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    alg["max-zoom"] = 15
    alg["webviewer"] = "none"
    assert alg.Run()

    assert gdal.ReadDirRecursive(tmp_vsimem) == [
        "15/",
        "15/5676/",
        "15/5676/13100.png",
        "15/5676/13101.png",
        "15/5677/",
        "15/5677/13100.png",
        "15/5677/13101.png",
    ]


def test_gdalalg_raster_tile_convention_tms(tmp_vsimem):

    src_ds = gdal.Translate(
        "", "../gcore/data/byte.tif", format="MEM", outputSRS="EPSG:32611"
    )

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    alg["max-zoom"] = 15
    alg["convention"] = "tms"
    alg["webviewer"] = "none"
    assert alg.Run()

    assert gdal.ReadDirRecursive(tmp_vsimem) == [
        "15/",
        "15/5676/",
        "15/5676/19666.png",
        "15/5676/19667.png",
        "15/5677/",
        "15/5677/19666.png",
        "15/5677/19667.png",
    ]


def test_gdalalg_raster_tile_min_zoom_metadata_aux_xml(tmp_vsimem):

    alg = get_alg()
    alg["input"] = gdal.Translate(
        "", "../gcore/data/byte.tif", format="MEM", outputSRS="EPSG:32611"
    )
    alg["output"] = tmp_vsimem
    alg["min-zoom"] = 10
    alg["aux-xml"] = True
    alg["copy-src-metadata"] = True
    alg["metadata"] = {"FOO": "BAR"}
    alg["webviewer"] = "none"
    assert alg.Run()

    assert gdal.ReadDirRecursive(tmp_vsimem) == [
        "10/",
        "10/177/",
        "10/177/409.png",
        "10/177/409.png.aux.xml",
        "11/",
        "11/354/",
        "11/354/818.png",
        "11/354/818.png.aux.xml",
    ]

    with gdal.Open(tmp_vsimem / "10/177/409.png") as ds:
        assert ds.GetMetadata_Dict() == {"AREA_OR_POINT": "Area", "FOO": "BAR"}

    with gdal.Open(tmp_vsimem / "11/354/818.png") as ds:
        assert ds.GetMetadata_Dict() == {"AREA_OR_POINT": "Area", "FOO": "BAR"}


@pytest.mark.require_driver("GTiff")
@pytest.mark.require_driver("COG")
@pytest.mark.parametrize("output_format", ["GTiff", "COG"])
def test_gdalalg_raster_tile_output_format_gtiff(tmp_vsimem, output_format):

    alg = get_alg()
    alg["input"] = gdal.Translate(
        "", "../gcore/data/byte.tif", format="MEM", outputSRS="EPSG:32611"
    )
    alg["output"] = tmp_vsimem
    alg["output-format"] = output_format
    alg["min-zoom"] = 10
    alg["aux-xml"] = True
    alg["copy-src-metadata"] = True
    alg["metadata"] = {"FOO": "BAR"}
    alg["webviewer"] = "none"
    assert alg.Run()

    assert gdal.ReadDirRecursive(tmp_vsimem) == [
        "10/",
        "10/177/",
        "10/177/409.tif",
        "11/",
        "11/354/",
        "11/354/818.tif",
    ]

    with gdal.Open(tmp_vsimem / "10/177/409.tif") as ds:
        assert ds.GetSpatialRef().GetAuthorityCode(None) == "3857"
        assert list(ds.GetGeoTransform()) == pytest.approx(
            [
                -13110479.09147343,
                152.8740565703556,
                0.0,
                4030983.1236470547,
                0.0,
                -152.87405657035197,
            ]
        )
        assert ds.GetMetadata_Dict() == {"AREA_OR_POINT": "Area", "FOO": "BAR"}

    with gdal.Open(tmp_vsimem / "11/354/818.tif") as ds:
        assert ds.GetSpatialRef().GetAuthorityCode(None) == "3857"
        assert list(ds.GetGeoTransform()) == pytest.approx(
            [
                -13110479.09147343,
                76.43702828517625,
                0.0,
                4030983.1236470547,
                0.0,
                -76.43702828517625,
            ]
        )
        assert ds.GetMetadata_Dict() == {"AREA_OR_POINT": "Area", "FOO": "BAR"}


def test_gdalalg_raster_tile_resume(tmp_vsimem):

    alg = get_alg()
    alg["input"] = gdal.Translate(
        "", "../gcore/data/byte.tif", format="MEM", outputSRS="EPSG:32611"
    )
    alg["output"] = tmp_vsimem
    alg["min-zoom"] = 10
    alg["webviewer"] = "none"
    assert alg.Run()

    assert gdal.ReadDirRecursive(tmp_vsimem) == [
        "10/",
        "10/177/",
        "10/177/409.png",
        "11/",
        "11/354/",
        "11/354/818.png",
    ]

    gdal.FileFromMemBuffer(tmp_vsimem / "10/177/409.png", "")
    gdal.FileFromMemBuffer(tmp_vsimem / "11/354/818.png", "")

    alg = get_alg()
    alg["input"] = gdal.Translate(
        "", "../gcore/data/byte.tif", format="MEM", outputSRS="EPSG:32611"
    )
    alg["output"] = tmp_vsimem
    alg["min-zoom"] = 10
    alg["resume"] = True
    assert alg.Run()

    assert gdal.VSIStatL(tmp_vsimem / "10/177/409.png").size == 0
    assert gdal.VSIStatL(tmp_vsimem / "11/354/818.png").size == 0


def test_gdalalg_raster_tile_tilesize(tmp_vsimem):

    alg = get_alg()
    alg["input"] = gdal.Translate(
        "", "../gcore/data/byte.tif", format="MEM", outputSRS="EPSG:32611"
    )
    alg["output"] = tmp_vsimem
    alg["tile-size"] = 512
    assert alg.Run()

    with gdal.Open(tmp_vsimem / "11/354/818.png") as ds:
        assert ds.RasterCount == 2
        assert ds.RasterXSize == 512
        assert ds.RasterYSize == 512
        assert [ds.GetRasterBand(i + 1).Checksum() for i in range(2)] == [17181, 17734]


def test_gdalalg_raster_tile_tilesize_too_large(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 65536)
    src_ds.SetSpatialRef(osr.SpatialReference("+proj=longlat +datum=WGS84"))
    src_ds.SetGeoTransform([-180, 360, 0, 90, 0, -180])

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    alg["tile-size"] = 32768
    alg["output-format"] = "GTiff"
    with pytest.raises(
        Exception,
        match="Tile size and/or number of bands too large compared to available RAM",
    ):
        alg.Run()


def test_gdalalg_raster_tile_cannot_reopen_tile(tmp_vsimem):

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_vsimem
    alg["max-zoom"] = 1
    assert alg.Run()

    gdal.FileFromMemBuffer(tmp_vsimem / "1/0/0.png", "")

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_vsimem
    alg["min-zoom"] = 0
    alg["max-zoom"] = 1
    alg["resume"] = True
    with pytest.raises(Exception, match="exists but cannot be opened with PNG driver"):
        alg.Run()
