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

import json
import struct
import sys

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
        "stacta.json",
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

    if tiling_scheme == "mercator":
        with gdal.VSIFile(tmp_vsimem / "stacta.json", "rb") as f:
            got = f.read()
            # Uncomment below line to regenerate expected file
            # open("data/gdal_raster_tile_expected_stacta.json", "wb").write(got)
            got = json.loads(got)
            expected = json.loads(
                open("data/gdal_raster_tile_expected_stacta.json", "rb").read()
            )
            assert got["bbox"] == pytest.approx(expected["bbox"])
            assert len(got["geometry"]["coordinates"]) == 1
            assert len(got["geometry"]["coordinates"][0]) == 5
            for i in range(5):
                assert got["geometry"]["coordinates"][0][i] == pytest.approx(
                    expected["geometry"]["coordinates"][0][i]
                )
            assert got["properties"]["proj:transform"] == pytest.approx(
                expected["properties"]["proj:transform"]
            )
            got["bbox"] = expected["bbox"]
            got["geometry"]["coordinates"] = expected["geometry"]["coordinates"]
            got["properties"]["proj:transform"] = expected["properties"][
                "proj:transform"
            ]
            assert got == expected, (got, expected)

    drv = gdal.GetDriverByName("STACTA")
    if drv:
        ds = gdal.Open(tmp_vsimem / "stacta.json")
        assert ds.RasterXSize == 256
        assert ds.RasterYSize == 256
        assert ds.GetSpatialRef().GetAuthorityCode(None) == "3857"
        assert ds.GetGeoTransform() == pytest.approx(
            (
                -13110479.09147343,
                76.43702828517625,
                0.0,
                4030983.1236470547,
                0.0,
                -76.43702828517625,
            )
        )
        assert ds.GetMetadata_Dict() == {
            "datetime": "1970-01-01T00:00:00.000Z",
            "end_datetime": "9999-12-31T23:59:59.999Z",
            "start_datetime": "0001-01-01T00:00:00.000Z",
        }
        with pytest.raises(
            Exception, match=r"Cannot open http://example.com/.*/11/354/818.png"
        ):
            ds.GetRasterBand(1).Checksum()


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
            "stacta.json",
        ]
        if xyz
        else [
            "0/",
            "0/0/",
            "0/0/0.png",
            "0/1/",
            "0/1/0.png",
            "openlayers.html",
            "stacta.json",
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
        "stacta.json",
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

    assert len(gdal.ReadDirRecursive(tmp_vsimem)) == 108


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
    with gdaltest.config_option("GDAL_THRESHOLD_MIN_TILES_PER_JOB", "1"):
        alg.Run(my_progress)

    assert last_pct[0] == 1.0

    assert len(gdal.ReadDirRecursive(tmp_vsimem)) == 108


def test_gdalalg_raster_tile_multithread_interrupt_in_base_tiles(tmp_path):

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return False

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_path / "subdir"
    alg["parallel-method"] = "thread"
    alg["max-zoom"] = 10
    with pytest.raises(Exception, match="Process interrupted by user"):
        alg.Run(my_progress)

    assert last_pct[0] != 1.0


def test_gdalalg_raster_tile_multithread_interrupt_in_ovr_tiles(tmp_path):

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return pct > 0.75

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_path / "subdir"
    alg["parallel-method"] = "thread"
    alg["min-zoom"] = 0
    alg["max-zoom"] = 3
    with pytest.raises(Exception, match="Process interrupted by user"):
        alg.Run(my_progress)

    assert last_pct[0] != 1.0

    assert len(gdal.ReadDirRecursive(tmp_path / "subdir")) != 108


def _get_effective_cpus():
    val = gdal.GetConfigOption("GDAL_NUM_THREADS", None)
    if val:
        return int(val)
    return gdal.GetNumCPUs()


def test_gdalalg_raster_tile_spawn_auto(tmp_path):

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_path / "subdir"
    alg["min-zoom"] = 0
    alg["max-zoom"] = 3
    with gdaltest.config_options(
        {
            "GDAL_THRESHOLD_MIN_THREADS_FOR_SPAWN": "1",
            "GDAL_THRESHOLD_MIN_TILES_PER_JOB": "1",
        }
    ):
        alg.Run()

    assert len(gdal.ReadDirRecursive(tmp_path / "subdir")) == 108

    if _get_effective_cpus() >= 2:
        assert alg["parallel-method"] == "spawn"
    else:
        assert alg["parallel-method"] == ""


@pytest.mark.skipif(_get_effective_cpus() <= 1, reason="needs more than one CPU")
def test_gdalalg_raster_tile_spawn_incompatible_source(tmp_path):

    alg = get_alg()
    alg["input"] = gdal.Translate("", "../gdrivers/data/small_world.tif", format="MEM")
    alg["output"] = tmp_path / "subdir"
    alg["parallel-method"] = "spawn"
    with pytest.raises(
        Exception,
        match="Unnamed or memory dataset sources are not supported with spawn parallelization method",
    ):
        alg.Run()


@pytest.mark.skipif(_get_effective_cpus() <= 1, reason="needs more than one CPU")
def test_gdalalg_raster_tile_spawn_incompatible_output(tmp_vsimem):

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_vsimem
    alg["parallel-method"] = "spawn"
    with pytest.raises(
        Exception,
        match="/vsimem/ output directory not supported with spawn parallelization method",
    ):
        alg.Run()


@pytest.mark.skipif(_get_effective_cpus() <= 1, reason="needs more than one CPU")
def test_gdalalg_raster_tile_spawn_gdal_not_found(tmp_path):

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_path
    alg["parallel-method"] = "spawn"
    with pytest.raises(
        Exception, match="No 'gdal' binary can be found in '/i_do/not/exist'"
    ):
        with gdal.config_option("GDAL_PATH", "/i_do/not/exist"):
            alg.Run()


def _has_GetCurrentThreadCount_and_fork():
    return (
        sys.platform in ("linux", "darwin")
        or sys.platform.startswith("freebsd")
        or sys.platform.startswith("netbsd")
    )


@pytest.mark.skipif(_get_effective_cpus() <= 1, reason="needs more than one CPU")
@pytest.mark.skipif(
    not _has_GetCurrentThreadCount_and_fork(),
    reason="needs Linux, Mac, FreeBSD or netBSD",
)
def test_gdalalg_raster_tile_fork_auto(tmp_path):

    monothreaded = gdal.GetCurrentThreadCount() == 1

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_path / "subdir"
    alg["min-zoom"] = 0
    alg["max-zoom"] = 3
    with gdaltest.config_options(
        {
            "GDAL_THRESHOLD_MIN_THREADS_FOR_SPAWN": "1",
            "GDAL_THRESHOLD_MIN_TILES_PER_JOB": "1",
            "GDAL_PATH": "/i_do/not/exist",
        }
    ):
        alg.Run()

    if monothreaded:
        assert alg["parallel-method"] == "fork"
    else:
        print("Fork cannot be used due to other threads being active")
        assert alg["parallel-method"] == ""

    assert len(gdal.ReadDirRecursive(tmp_path / "subdir")) == 108


@pytest.mark.skipif(_get_effective_cpus() <= 1, reason="needs more than one CPU")
@pytest.mark.skipif(
    not _has_GetCurrentThreadCount_and_fork(),
    reason="needs Linux, Mac, FreeBSD or netBSD",
)
def test_gdalalg_raster_tile_fork_auto_incompatible_output(tmp_vsimem):

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_vsimem / "subdir"
    alg["min-zoom"] = 0
    alg["max-zoom"] = 3
    with gdaltest.config_options(
        {
            "GDAL_THRESHOLD_MIN_THREADS_FOR_SPAWN": "1",
            "GDAL_THRESHOLD_MIN_TILES_PER_JOB": "1",
            "GDAL_PATH": "/i_do/not/exist",
        }
    ):
        alg.Run()

    assert alg["parallel-method"] != "fork"

    assert len(gdal.ReadDirRecursive(tmp_vsimem / "subdir")) == 108


@pytest.mark.skipif(_get_effective_cpus() <= 1, reason="needs more than one CPU")
@pytest.mark.skipif(
    sys.platform == "win32", reason="Windows not supported for this test"
)
def test_gdalalg_raster_tile_fork_forced(tmp_path):

    alg = get_alg()
    alg["input"] = gdal.Translate("", "../gdrivers/data/small_world.tif", format="MEM")
    alg["output"] = tmp_path / "subdir"
    alg["min-zoom"] = 0
    alg["max-zoom"] = 3
    alg["parallel-method"] = "fork"
    with gdaltest.config_options(
        {
            "GDAL_THRESHOLD_MIN_THREADS_FOR_SPAWN": "1",
            "GDAL_THRESHOLD_MIN_TILES_PER_JOB": "1",
        }
    ):
        alg.Run()

    assert alg["parallel-method"] == "fork"
    assert len(gdal.ReadDirRecursive(tmp_path / "subdir")) == 108


@pytest.mark.skipif(_get_effective_cpus() <= 1, reason="needs more than one CPU")
@pytest.mark.skipif(
    sys.platform == "win32", reason="Windows not supported for this test"
)
def test_gdalalg_raster_tile_fork_incompatible_source(tmp_vsimem):

    src_ds = gdal.Open("../gdrivers/data/small_world.tif")
    src_ds.SetDescription("")

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    alg["parallel-method"] = "fork"
    with pytest.raises(
        Exception,
        match="Unnamed non-MEM source are not supported with fork parallelization method",
    ):
        alg.Run()


@pytest.mark.skipif(_get_effective_cpus() <= 1, reason="needs more than one CPU")
@pytest.mark.skipif(
    sys.platform == "win32", reason="Windows not supported for this test"
)
def test_gdalalg_raster_tile_fork_incompatible_output(tmp_vsimem):

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_vsimem
    alg["parallel-method"] = "fork"
    with pytest.raises(
        Exception,
        match="/vsimem/ output directory not supported with fork parallelization method",
    ):
        alg.Run()


@pytest.mark.skipif(_get_effective_cpus() <= 1, reason="needs more than one CPU")
def test_gdalalg_raster_tile_spawn_error_in_child(tmp_path):

    input_filename = tmp_path / "in.tif"
    gdal.Translate(input_filename, "../gdrivers/data/small_world.tif")
    f = gdal.VSIFOpenL(input_filename, "rb+")
    assert f
    gdal.VSIFTruncateL(f, 4096)
    gdal.VSIFCloseL(f)

    alg = get_alg()
    alg["input"] = input_filename
    alg["output"] = tmp_path
    alg["parallel-method"] = "spawn"
    alg["max-zoom"] = 3
    alg["num-threads"] = 2
    with pytest.raises(Exception, match="Child process.*failed"):
        with gdal.config_option("GDAL_THRESHOLD_MIN_TILES_PER_JOB", "1"):
            alg.Run()


@pytest.mark.skipif(_get_effective_cpus() <= 2, reason="needs more than 2 CPUs")
@pytest.mark.skipif(
    sys.platform == "win32", reason="Windows not supported for this test"
)
def test_gdalalg_raster_tile_spawn_limit(tmp_path):

    import resource

    if resource.getrlimit(resource.RLIMIT_NOFILE)[0] > 4096:
        pytest.skip("Limit for number of opened files is too high")

    fd = []
    num_threads = 3
    num_fds_per_threads = 3
    while True:
        try:
            fd.append(open("/dev/null", "rb"))
        except Exception:
            break
    for i in range(1 + num_threads * num_fds_per_threads):
        fd.pop()

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_path
    alg["parallel-method"] = "spawn"
    alg["min-zoom"] = 0
    alg["max-zoom"] = 3
    alg["num-threads"] = num_threads
    try:
        with gdal.config_option("GDAL_THRESHOLD_MIN_TILES_PER_JOB", "1"):
            with gdaltest.error_raised(
                gdal.CE_Warning, "Limiting the number of child workers to"
            ):
                alg.Run()
    finally:
        del fd

    assert len(gdal.ReadDirRecursive(tmp_path)) == 108


@pytest.mark.skipif(_get_effective_cpus() <= 1, reason="needs more than one CPU")
def test_gdalalg_raster_tile_spawn(tmp_path):

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= last_pct[0]
        last_pct[0] = pct
        return True

    got_spurious = [False]

    def my_handler(errorClass, errno, msg):
        if "Spurious" in msg:
            got_spurious[0] = True
        return

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_path / "subdir"
    alg["min-zoom"] = 0
    alg["max-zoom"] = 3
    alg["parallel-method"] = "spawn"
    alg["metadata"] = {"foo": "bar"}
    with gdaltest.config_options(
        {
            "CPL_DEBUG": "ON",
            "CPL_LOG": str(tmp_path / "log.txt"),
            "GDAL_RASTER_TILE_EMIT_SPURIOUS_CHARS": "YES",
            "GDAL_THRESHOLD_MIN_TILES_PER_JOB": "1",
        }
    ):
        with gdaltest.error_handler(my_handler):
            alg.Run(my_progress)

    assert last_pct[0] == 1.0

    assert len(gdal.ReadDirRecursive(tmp_path / "subdir")) == 108
    assert gdal.VSIStatL(tmp_path / "log.txt") is not None
    assert got_spurious[0]


@pytest.mark.skipif(_get_effective_cpus() <= 1, reason="needs more than one CPU")
def test_gdalalg_raster_tile_spawn_interrupt_in_base_tiles(tmp_path):

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return False

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_path / "subdir"
    alg["max-zoom"] = 10
    with gdaltest.config_option("GDAL_THRESHOLD_MIN_TILES_PER_JOB", "1"):
        with pytest.raises(Exception, match="Process interrupted by user"):
            alg.Run(my_progress)

    assert last_pct[0] != 1.0


@pytest.mark.skipif(_get_effective_cpus() <= 1, reason="needs more than one CPU")
def test_gdalalg_raster_tile_spawn_interrupt_in_ovr_tiles(tmp_path):

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return pct > 0.75

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_path / "subdir"
    alg["min-zoom"] = 0
    alg["max-zoom"] = 3
    with gdaltest.config_option("GDAL_THRESHOLD_MIN_TILES_PER_JOB", "1"):
        with pytest.raises(Exception, match="Process interrupted by user"):
            alg.Run(my_progress)

    assert last_pct[0] != 1.0

    assert len(gdal.ReadDirRecursive(tmp_path / "subdir")) != 108


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


def test_gdalalg_raster_tile_cannot_determine_target_extent(tmp_vsimem):

    src_ds = gdal.Warp(
        "", "../gdrivers/data/small_world.tif", options="-f MEM -t_srs +proj=ortho"
    )
    src_ds = gdal.Translate("", src_ds, options="-f MEM -srcwin 0 0 1 1")
    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    with pytest.raises(
        Exception,
        match="Cannot determine extent of raster in target CRS",
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
                16108,
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
@pytest.mark.parametrize(
    "output_format,tile_size", [("GTiff", None), ("GTiff", 1024), ("COG", None)]
)
def test_gdalalg_raster_tile_output_format_gtiff(tmp_vsimem, output_format, tile_size):

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
    if tile_size:
        alg["tile-size"] = tile_size
    assert alg.Run()

    if not tile_size:
        tile_size = 256

    assert gdal.ReadDirRecursive(tmp_vsimem) == [
        "10/",
        "10/177/",
        "10/177/409.tif",
        "11/",
        "11/354/",
        "11/354/818.tif",
    ]

    with gdal.Open(tmp_vsimem / "10/177/409.tif") as ds:
        assert ds.RasterXSize == tile_size
        assert ds.RasterYSize == tile_size
        assert ds.GetSpatialRef().GetAuthorityCode(None) == "3857"
        assert list(ds.GetGeoTransform()) == pytest.approx(
            [
                -13110479.09147343,
                152.8740565703556 / tile_size * 256,
                0.0,
                4030983.1236470547,
                0.0,
                -152.87405657035197 / tile_size * 256,
            ]
        )
        assert ds.GetMetadata_Dict() == {"AREA_OR_POINT": "Area", "FOO": "BAR"}
        assert ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "LZW"
        assert ds.GetRasterBand(1).GetBlockSize() == (
            [tile_size, tile_size] if tile_size <= 512 else [256, 256]
        )

    with gdal.Open(tmp_vsimem / "11/354/818.tif") as ds:
        assert ds.GetSpatialRef().GetAuthorityCode(None) == "3857"
        assert list(ds.GetGeoTransform()) == pytest.approx(
            [
                -13110479.09147343,
                76.43702828517625 / tile_size * 256,
                0.0,
                4030983.1236470547,
                0.0,
                -76.43702828517625 / tile_size * 256,
            ]
        )
        assert ds.GetMetadata_Dict() == {"AREA_OR_POINT": "Area", "FOO": "BAR"}
        assert ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "LZW"
        assert ds.GetRasterBand(1).GetBlockSize() == (
            [tile_size, tile_size] if tile_size <= 512 else [256, 256]
        )


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


def test_gdalalg_raster_tile_raster(tmp_vsimem):

    src_ds = gdal.Open("../gcore/data/byte.tif")
    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    alg["tiling-scheme"] = "raster"
    alg["output-format"] = "GTiff"
    with gdal.config_option("GDAL_RASTER_TILE_HTML_PREC", "10"):
        assert alg.Run()

    assert gdal.ReadDirRecursive(tmp_vsimem) == [
        "0/",
        "0/0/",
        "0/0/0.tif",
        "openlayers.html",
        "stacta.json",
    ]

    with gdal.Open(tmp_vsimem / "0/0/0.tif") as ds:
        assert ds.RasterCount == 2
        assert ds.RasterXSize == 256
        assert ds.RasterYSize == 256
        assert [ds.GetRasterBand(i + 1).Checksum() for i in range(2)] == [4333, 4898]
        assert (
            ds.GetRasterBand(1).ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize)
            == src_ds.ReadRaster()
        )
        assert [x for x in ds.GetGeoTransform()] == pytest.approx(
            [x for x in src_ds.GetGeoTransform()]
        )
        assert ds.GetSpatialRef().IsSame(src_ds.GetSpatialRef())

    with gdal.VSIFile(tmp_vsimem / "openlayers.html", "rb") as f:
        got = f.read()
        # Uncomment below line to regenerate expected file
        # open("data/gdal_raster_tile_raster_expected_openlayers.html", "wb").write(got)
        assert (
            got
            == open(
                "data/gdal_raster_tile_raster_expected_openlayers.html", "rb"
            ).read()
        )


def test_gdalalg_raster_tile_raster_ungeoreferenced(tmp_vsimem):

    src_ds = gdal.Open("../gcore/data/byte.tif")
    wrk_ds = gdal.GetDriverByName("MEM").Create(
        "", src_ds.RasterXSize, src_ds.RasterYSize
    )
    wrk_ds.WriteRaster(
        0, 0, src_ds.RasterXSize, src_ds.RasterYSize, src_ds.ReadRaster()
    )

    alg = get_alg()
    alg["input"] = wrk_ds
    alg["output"] = tmp_vsimem
    alg["tiling-scheme"] = "raster"
    alg["output-format"] = "GTiff"
    assert alg.Run()

    assert gdal.ReadDirRecursive(tmp_vsimem) == [
        "0/",
        "0/0/",
        "0/0/0.tif",
        "openlayers.html",
        "stacta.json",
    ]

    with gdal.Open(tmp_vsimem / "0/0/0.tif") as ds:
        assert ds.RasterCount == 2
        assert ds.RasterXSize == 256
        assert ds.RasterYSize == 256
        assert [ds.GetRasterBand(i + 1).Checksum() for i in range(2)] == [4333, 4898]
        assert (
            ds.GetRasterBand(1).ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize)
            == src_ds.ReadRaster()
        )
        assert [x for x in ds.GetGeoTransform()] == pytest.approx([0, 1, 0, 0, 0, -1])
        assert ds.GetSpatialRef() is None


def test_gdalalg_raster_tile_raster_min_max_zoom(tmp_vsimem):

    src_ds = gdal.Open("../gdrivers/data/small_world.tif")

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    alg["tiling-scheme"] = "raster"
    alg["min-zoom"] = 0
    alg["max-zoom"] = 2
    alg["resampling"] = "nearest"
    assert alg.Run()

    assert gdal.ReadDirRecursive(tmp_vsimem) == [
        "0/",
        "0/0/",
        "0/0/0.png",
        "1/",
        "1/0/",
        "1/0/0.png",
        "2/",
        "2/0/",
        "2/0/0.png",
        "2/1/",
        "2/1/0.png",
        "openlayers.html",
        "stacta.json",
    ]

    with gdal.Open(tmp_vsimem / "2/0/0.png") as ds:
        assert ds.RasterCount == 4
        assert ds.RasterXSize == 256
        assert ds.RasterYSize == 256
        assert [ds.GetRasterBand(i + 1).Checksum() for i in range(4)] == [
            60550,
            62572,
            46338,
            38489,
        ]
        assert ds.ReadRaster(0, 0, 256, 200, band_list=[1, 2, 3]) == src_ds.ReadRaster(
            0, 0, 256, 200
        )

    with gdal.Open(tmp_vsimem / "2/1/0.png") as ds:
        assert ds.RasterCount == 4
        assert ds.RasterXSize == 256
        assert ds.RasterYSize == 256
        assert [ds.GetRasterBand(i + 1).Checksum() for i in range(4)] == [
            54448,
            61647,
            44944,
            38489,
        ]
        assert ds.ReadRaster(
            0, 0, 400 - 256, 200, band_list=[1, 2, 3]
        ) == src_ds.ReadRaster(256, 0, 400 - 256, 200)

    with gdal.Open(tmp_vsimem / "1/0/0.png") as ds:
        assert ds.ReadRaster(0, 0, 200, 100, band_list=[1, 2, 3]) == src_ds.ReadRaster(
            buf_xsize=200, buf_ysize=100
        )


def test_gdalalg_raster_tile_raster_kml(tmp_vsimem):

    alg = get_alg()
    alg["input"] = gdal.Translate(
        "", "../gcore/data/byte.tif", format="MEM", outputSRS="EPSG:32611"
    )
    alg["output"] = tmp_vsimem
    alg["min-zoom"] = 10
    alg["max-zoom"] = 11
    alg["resampling"] = "nearest"
    alg["kml"] = True
    with gdal.config_option("GDAL_RASTER_TILE_KML_PREC", "10"):
        assert alg.Run()

    assert gdal.ReadDirRecursive(tmp_vsimem) == [
        "10/",
        "10/177/",
        "10/177/409.kml",
        "10/177/409.png",
        "11/",
        "11/354/",
        "11/354/818.kml",
        "11/354/818.png",
        "doc.kml",
        "leaflet.html",
        "mapml.mapml",
        "openlayers.html",
        "stacta.json",
    ]

    with gdal.VSIFile(tmp_vsimem / "doc.kml", "rb") as f:
        got = f.read()
        # Uncomment below line to regenerate expected file
        # open("data/gdal_raster_tile_expected_byte_10_11_doc.kml", "wb").write(got)
        assert (
            got
            == open("data/gdal_raster_tile_expected_byte_10_11_doc.kml", "rb").read()
        )

    with gdal.VSIFile(tmp_vsimem / "10" / "177" / "409.kml", "rb") as f:
        got = f.read()
        # Uncomment below line to regenerate expected file
        # open("data/gdal_raster_tile_expected_byte_10_11_10_177_409.kml", "wb").write(got)
        assert (
            got
            == open(
                "data/gdal_raster_tile_expected_byte_10_11_10_177_409.kml", "rb"
            ).read()
        )

    with gdal.VSIFile(tmp_vsimem / "11" / "354" / "818.kml", "rb") as f:
        got = f.read()
        # Uncomment below line to regenerate expected file
        # open("data/gdal_raster_tile_expected_byte_10_11_11_354_818.kml", "wb").write(got)
        assert (
            got
            == open(
                "data/gdal_raster_tile_expected_byte_10_11_11_354_818.kml", "rb"
            ).read()
        )

    if gdal.GetDriverByName("KMLSuperOverlay"):
        ds = gdal.Open(tmp_vsimem / "doc.kml")
        assert ds.GetRasterBand(1).Checksum() == 4215


def test_gdalalg_raster_tile_raster_kml_with_gx_latlonquad(tmp_vsimem):

    alg = get_alg()
    alg["input"] = gdal.Translate(
        "", "../gcore/data/byte.tif", format="MEM", outputSRS="EPSG:32611"
    )
    alg["output"] = tmp_vsimem
    alg["tiling-scheme"] = "raster"
    alg["resampling"] = "nearest"
    alg["kml"] = True
    with gdal.config_option("GDAL_RASTER_TILE_KML_PREC", "10"):
        assert alg.Run()

    assert gdal.ReadDirRecursive(tmp_vsimem) == [
        "0/",
        "0/0/",
        "0/0/0.kml",
        "0/0/0.png",
        "doc.kml",
        "openlayers.html",
        "stacta.json",
    ]

    with gdal.VSIFile(tmp_vsimem / "doc.kml", "rb") as f:
        got = f.read()
        # Uncomment below line to regenerate expected file
        # open("data/gdal_raster_tile_expected_byte_raster_doc.kml", "wb").write(got)
        assert (
            got
            == open("data/gdal_raster_tile_expected_byte_raster_doc.kml", "rb").read()
        )

    with gdal.VSIFile(tmp_vsimem / "0" / "0" / "0.kml", "rb") as f:
        got = f.read()
        # Uncomment below line to regenerate expected file
        # open("data/gdal_raster_tile_expected_byte_raster_0_0_0.kml", "wb").write(got)
        assert (
            got
            == open("data/gdal_raster_tile_expected_byte_raster_0_0_0.kml", "rb").read()
        )


def test_gdalalg_raster_tile_excluded_values_error(tmp_vsimem):

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = tmp_vsimem
    alg["excluded-values"] = "0"
    with pytest.raises(
        Exception,
        match="'excluded-values' can only be specified if 'resampling' is set to 'average'",
    ):
        alg.Run()

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = tmp_vsimem
    alg["excluded-values"] = "0"
    alg["resampling"] = "average"
    alg["overview-resampling"] = "near"
    with pytest.raises(
        Exception,
        match="'excluded-values' can only be specified if 'overview-resampling' is set to 'average'",
    ):
        alg.Run()


def test_gdalalg_raster_tile_excluded_values(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 256, 256, 3, gdal.GDT_Byte)
    src_ds.GetRasterBand(1).WriteRaster(
        0, 0, 2, 2, struct.pack("B" * 4, 10, 20, 30, 40)
    )
    src_ds.GetRasterBand(2).WriteRaster(
        0, 0, 2, 2, struct.pack("B" * 4, 11, 21, 31, 41)
    )
    src_ds.GetRasterBand(3).WriteRaster(
        0, 0, 2, 2, struct.pack("B" * 4, 12, 22, 32, 42)
    )
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(3857)
    src_ds.SetSpatialRef(srs)
    MAX_GM = 20037508.342789244
    RES_Z0 = 2 * MAX_GM / 256
    RES_Z1 = RES_Z0 / 2
    # Spatial extent of tile (0,0) at zoom level 1
    src_ds.SetGeoTransform([-MAX_GM, RES_Z1, 0, MAX_GM, 0, -RES_Z1])

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    alg["resampling"] = "average"
    alg["excluded-values"] = "30,31,32"
    alg["excluded-values-pct-threshold"] = 50
    alg["min-zoom"] = 0
    alg["max-zoom"] = 1
    assert alg.Run()

    ds = gdal.Open(tmp_vsimem / "0/0/0.png")
    assert struct.unpack("B" * 4, ds.ReadRaster(0, 0, 1, 1)) == (
        (10 + 20 + 40) // 3,
        (11 + 21 + 41) // 3,
        (12 + 22 + 42) // 3,
        255,
    )


def test_gdalalg_raster_tile_nodata_values_pct_threshold(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 256, 256, 3, gdal.GDT_Byte)
    for i in range(3):
        src_ds.GetRasterBand(i + 1).SetNoDataValue(20)
        src_ds.GetRasterBand(i + 1).WriteRaster(
            0, 0, 2, 2, struct.pack("B" * 4, 10, 20, 30, 40)
        )
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(3857)
    src_ds.SetSpatialRef(srs)
    MAX_GM = 20037508.342789244
    RES_Z0 = 2 * MAX_GM / 256
    RES_Z1 = RES_Z0 / 2
    # Spatial extent of tile (0,0) at zoom level 1
    src_ds.SetGeoTransform([-MAX_GM, RES_Z1, 0, MAX_GM, 0, -RES_Z1])

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    alg["resampling"] = "average"
    alg["min-zoom"] = 0
    alg["max-zoom"] = 1
    assert alg.Run()

    ds = gdal.Open(tmp_vsimem / "0/0/0.png")
    assert struct.unpack("B" * 2, ds.ReadRaster(0, 0, 1, 1, band_list=[1, 4])) == (
        round((10 + 30 + 40) / 3),
        191,
    )

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    alg["resampling"] = "average"
    alg["nodata-values-pct-threshold"] = 50
    alg["min-zoom"] = 0
    alg["max-zoom"] = 1
    assert alg.Run()

    ds = gdal.Open(tmp_vsimem / "0/0/0.png")
    assert struct.unpack("B" * 2, ds.ReadRaster(0, 0, 1, 1, band_list=[1, 4])) == (
        round((10 + 30 + 40) / 3),
        255,
    )

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    alg["resampling"] = "average"
    alg["nodata-values-pct-threshold"] = 25
    alg["min-zoom"] = 0
    alg["max-zoom"] = 1
    assert alg.Run()

    ds = gdal.Open(tmp_vsimem / "0/0/0.png")
    assert struct.unpack("B" * 2, ds.ReadRaster(0, 0, 1, 1, band_list=[1, 4])) == (0, 0)


def test_gdalalg_raster_tile_red_tile_with_alpha(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 256, 256, 4, gdal.GDT_Byte)
    src_ds.GetRasterBand(1).Fill(255)
    src_ds.GetRasterBand(2).Fill(0)
    src_ds.GetRasterBand(3).Fill(0)
    src_ds.GetRasterBand(4).Fill(255)
    src_ds.GetRasterBand(4).SetColorInterpretation(gdal.GCI_AlphaBand)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(3857)
    src_ds.SetSpatialRef(srs)
    MAX_GM = 20037508.342789244
    RES_Z0 = 2 * MAX_GM / 256
    # Spatial extent of tile (0,0) at zoom level 0
    src_ds.SetGeoTransform([-MAX_GM, RES_Z0, 0, MAX_GM, 0, -RES_Z0])

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    alg["max-zoom"] = 0
    assert alg.Run()

    ds = gdal.Open(tmp_vsimem / "0/0/0.png")
    assert ds.RasterCount == 3
    assert ds.GetRasterBand(1).ComputeRasterMinMax() == (255, 255)
    assert ds.GetRasterBand(2).ComputeRasterMinMax() == (0, 0)
    assert ds.GetRasterBand(3).ComputeRasterMinMax() == (0, 0)


@pytest.mark.parametrize("GDAL_RASTER_TILE_PNG_FILTER", ["", "AVERAGE", "PAETH"])
@pytest.mark.parametrize("nbands", [1, 2, 3, 4])
def test_gdalalg_raster_tile_png_optim(tmp_vsimem, GDAL_RASTER_TILE_PNG_FILTER, nbands):

    src_ds = gdal.GetDriverByName("MEM").Create("", 256, 256, nbands, gdal.GDT_Byte)
    if nbands == 2 or nbands == 4:
        src_ds.GetRasterBand(nbands).SetColorInterpretation(gdal.GCI_AlphaBand)
        src_ds.GetRasterBand(nbands).Fill(127)
        real_bands = nbands - 1
    else:
        real_bands = nbands

    for i in range(real_bands):
        src_ds.GetRasterBand(i + 1).WriteRaster(
            0,
            0,
            256,
            256,
            b"".join(
                [
                    bytes(
                        chr((i + j * 257 if (j // 256 % 4) == 3 else 0) % 256),
                        encoding="latin1",
                    )
                    for j in range(256 * 256)
                ]
            ),
        )

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(3857)
    src_ds.SetSpatialRef(srs)
    MAX_GM = 20037508.342789244
    RES_Z0 = 2 * MAX_GM / 256
    # Spatial extent of tile (0,0) at zoom level 0
    src_ds.SetGeoTransform([-MAX_GM, RES_Z0, 0, MAX_GM, 0, -RES_Z0])

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    alg["max-zoom"] = 0
    with gdal.config_option("GDAL_RASTER_TILE_PNG_FILTER", GDAL_RASTER_TILE_PNG_FILTER):
        assert alg.Run()

    ds = gdal.Open(tmp_vsimem / "0/0/0.png")
    assert [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)] == [
        src_ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)
    ]


@pytest.mark.require_driver("JPEG")
def test_gdalalg_raster_tile_png_optim_2(tmp_vsimem):

    alg = get_alg()
    alg["input"] = "data/test_gdal_raster_tile_paeth.jpg"
    alg["output"] = tmp_vsimem
    alg.Run()

    size_auto = gdal.VSIStatL(tmp_vsimem / "19/509173/334573.png").size

    alg = get_alg()
    alg["input"] = "data/test_gdal_raster_tile_paeth.jpg"
    alg["output"] = tmp_vsimem
    with gdal.config_option("GDAL_RASTER_TILE_PNG_FILTER", "AVERAGE"):
        alg.Run()

    size_avg = gdal.VSIStatL(tmp_vsimem / "19/509173/334573.png").size

    assert size_auto != size_avg


def test_gdalalg_raster_tile_pipeline(tmp_path):

    out_dirname = tmp_path / "subdir"
    with gdaltest.config_options(
        {
            "GDAL_THRESHOLD_MIN_THREADS_FOR_SPAWN": "1",
            "GDAL_THRESHOLD_MIN_TILES_PER_JOB": "1",
        }
    ):
        gdal.Run(
            "raster pipeline",
            pipeline=f"mosaic ../gdrivers/data/small_world.tif ! tile {out_dirname} --min-zoom=0 --max-zoom=3",
        )

    assert len(gdal.ReadDirRecursive(out_dirname)) == 108


@pytest.mark.skipif(_get_effective_cpus() <= 2, reason="needs more than 2 CPUs")
def test_gdalalg_raster_tile_pipeline_error(tmp_path):

    src_ds = gdal.Translate("", "../gdrivers/data/small_world.tif", format="MEM")

    out_dirname = tmp_path / "subdir"
    with gdaltest.config_options(
        {
            "GDAL_THRESHOLD_MIN_THREADS_FOR_SPAWN": "1",
            "GDAL_THRESHOLD_MIN_TILES_PER_JOB": "1",
        }
    ):
        with pytest.raises(
            Exception, match="Cannot execute this pipeline in parallel mode"
        ):
            gdal.Run(
                "raster pipeline",
                input=src_ds,
                pipeline=f"mosaic ! tile {out_dirname} --min-zoom=0 --max-zoom=3",
            )
