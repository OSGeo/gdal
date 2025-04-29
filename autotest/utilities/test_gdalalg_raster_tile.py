#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster tile' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

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
        alg["tilesize"] = tilesize
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

    with gdal.VSIFile(tmp_vsimem / "openlayers.html", "rb") as f:
        got = f.read()
        # Uncomment below line to regenerate expected file
        # open("data/gdal_raster_tile_expected_openlayers.html", "wb").write(got)
        assert (
            got == open("data/gdal_raster_tile_expected_openlayers.html", "rb").read()
        )


@pytest.mark.parametrize(
    "tiling_scheme,xyz", [("WorldCRS84Quad", True), ("geodetic", False)]
)
def test_gdalalg_raster_tile_small_world_geodetic(tmp_vsimem, tiling_scheme, xyz):

    alg = get_alg()
    alg["input"] = "../gdrivers/data/small_world.tif"
    alg["output"] = tmp_vsimem
    alg["tiling-scheme"] = tiling_scheme
    if not xyz:
        alg["convention"] = "tms"
    with gdal.config_option("GDAL_RASTER_TILE_HTML_PREC", "10"):
        assert alg.Run()

    assert gdal.ReadDirRecursive(tmp_vsimem) == [
        "0/",
        "0/0/",
        "0/0/0.png",
        "0/1/",
        "0/1/0.png",
        "openlayers.html",
    ]

    with gdal.Open(tmp_vsimem / "0/0/0.png") as ds:
        assert ds.RasterCount == 4
        assert ds.RasterXSize == 256
        assert ds.RasterYSize == 256
        assert [ds.GetRasterBand(i + 1).Checksum() for i in range(4)] == pytest.approx(
            [
                1315,
                63955,
                5106,
                17849,
            ],
            abs=1,
        )

    with gdal.Open(tmp_vsimem / "0/1/0.png") as ds:
        assert ds.RasterCount == 4
        assert ds.RasterXSize == 256
        assert ds.RasterYSize == 256
        assert [ds.GetRasterBand(i + 1).Checksum() for i in range(4)] == pytest.approx(
            [
                24456,
                25846,
                15674,
                17849,
            ],
            abs=1,
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


def test_gdalalg_raster_tile_nodata(tmp_path):

    alg = get_alg()
    alg["input"] = gdal.Translate(
        "", "../gcore/data/byte.tif", format="MEM", outputSRS="EPSG:32611", noData=0
    )
    alg["output"] = tmp_path
    alg["output-format"] = "GTiff"
    alg["min-zoom"] = 10
    alg["skip-blank"] = True
    alg["webviewer"] = "none"
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
        assert ds.GetRasterBand(1).Checksum() == 4160
        assert ds.GetRasterBand(1).GetNoDataValue() == 0

    with gdal.Open(tmp_path / "10" / "177" / "409.tif") as ds:
        assert ds.RasterCount == 1
        assert ds.RasterXSize == 256
        assert ds.RasterYSize == 256
        assert ds.GetRasterBand(1).Checksum() == 1191
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
        "openlayers.html",
    ]

    with gdal.Open(tmp_vsimem / "11/354/818.png") as ds:
        assert ds.GetRasterBand(1).GetColorTable() is not None

    with gdal.Open(tmp_vsimem / "10/177/409.png") as ds:
        assert ds.GetRasterBand(1).GetColorTable() is not None


def test_gdalalg_raster_tile_no_mem(tmp_vsimem):

    mem_drv = gdal.GetDriverByName("MEM")
    try:
        mem_drv.Deregister()

        alg = get_alg()
        alg["input"] = "../gcore/data/byte.tif"
        alg["output"] = tmp_vsimem
        with pytest.raises(Exception, match="Cannot find MEM driver"):
            alg.Run()
    finally:
        mem_drv.Register()


def test_gdalalg_raster_tile_min_zoom_larger_max_zoom(tmp_vsimem):

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = tmp_vsimem
    alg["min-zoom"] = 1
    alg["max-zoom"] = 0
    with pytest.raises(
        Exception, match="'min-zoom' should be lesser or equal to 'max-zoom'"
    ):
        alg.Run()


def test_gdalalg_raster_tile_too_large_max_zoom(tmp_vsimem):

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = tmp_vsimem
    alg["max-zoom"] = 31
    with pytest.raises(
        Exception, match=r"max-zoom = 31 is invalid. It should be in \[0,30\] range"
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


def test_gdalalg_raster_tile_cannot_determine_extent_in_target_crs(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetSpatialRef(osr.SpatialReference("+proj=longlat +datum=WGS84"))
    src_ds.SetGeoTransform([-180, 360, 0, 90, 0, -0.001])
    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem
    alg["tiling-scheme"] = "NZTM2000"
    with pytest.raises(
        Exception,
        match="Extent of source dataset is not compatible with extent of tiling scheme",
    ):
        alg.Run()


def test_gdalalg_raster_tile_addalpha_dstnodata_exclusive(tmp_vsimem):

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = tmp_vsimem
    alg["addalpha"] = True
    alg["dstnodata"] = 0
    with pytest.raises(
        Exception, match="'addalpha' and 'dstnodata' are mutually exclusive"
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
        assert ds.RasterCount == 4
        assert ds.RasterXSize == 256
        assert ds.RasterYSize == 256
        assert [ds.GetRasterBand(i + 1).Checksum() for i in range(4)] == [
            24650,
            23280,
            16559,
            17849,
        ]


def test_gdalalg_raster_tile_rgba(tmp_vsimem):

    alg = get_alg()
    alg["input"] = gdal.Translate(
        "",
        "../gdrivers/data/small_world.tif",
        options="-of MEM -b 1 -b 2 -b 3 -b mask -colorinterp_4 alpha",
    )
    alg["output"] = tmp_vsimem
    alg["webviewer"] = "none"
    assert alg.Run()

    assert gdal.ReadDirRecursive(tmp_vsimem) == ["0/", "0/0/", "0/0/0.png"]

    with gdal.Open(tmp_vsimem / "0/0/0.png") as ds:
        assert ds.RasterCount == 4
        assert ds.RasterXSize == 256
        assert ds.RasterYSize == 256
        assert [ds.GetRasterBand(i + 1).Checksum() for i in range(4)] == [
            24650,
            23280,
            16559,
            17849,
        ]


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
    alg["tilesize"] = 512
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
    alg["tilesize"] = 32768
    alg["output-format"] = "GTiff"
    with pytest.raises(
        Exception,
        match="Tile size and/or number of bands too large compared to available RAM",
    ):
        alg.Run()
