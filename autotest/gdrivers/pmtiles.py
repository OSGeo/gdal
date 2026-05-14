#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test PMTiles driver raster functionality.
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2026, Even Rouault
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal, osr

pytestmark = [
    pytest.mark.require_driver("PMTiles"),
    pytest.mark.require_driver("GPKG"),
    pytest.mark.require_driver("GTI"),
]


###############################################################################


@pytest.mark.require_driver("PNG")
def test_pmtiles_read_png():

    ds = gdal.Open("data/pmtiles/byte_png.pmtiles")
    assert ds.RasterCount == 2
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.GetSpatialRef().GetAuthorityCode() == "3857"
    assert ds.GetGeoTransform() == pytest.approx(
        (
            -13095879.619070962,
            76.43702828517625,
            0.0,
            4015772.155018305,
            0.0,
            -76.43702828517625,
        )
    )
    assert ds.GetRasterBand(1).Checksum() == 4575
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_GrayIndex
    assert ds.GetRasterBand(2).Checksum() == 4457
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_AlphaBand
    assert ds.GetRasterBand(1).GetOverviewCount() == 2
    assert ds.GetRasterBand(1).GetOverview(-1) is None
    assert ds.GetRasterBand(1).GetOverview(2) is None
    ovr_band = ds.GetRasterBand(1).GetOverview(0)
    assert ovr_band
    assert ovr_band.XSize == 10
    assert ovr_band.YSize == 10
    ovr_ds = ovr_band.GetDataset()
    assert ovr_ds.GetSpatialRef().GetAuthorityCode() == "3857"
    assert ovr_ds.GetGeoTransform() == pytest.approx(
        (
            -13095879.619070962,
            2 * 76.43702828517625,
            0.0,
            4015772.155018305,
            0.0,
            2 * -76.43702828517625,
        )
    )
    assert ovr_band.Checksum() == 952
    assert ds.ReadRaster(0, 0, 20, 20, 10, 10) == ovr_ds.ReadRaster()
    ovr_band = ds.GetRasterBand(1).GetOverview(1)
    assert ovr_band
    assert ovr_band.XSize == 5
    assert ovr_band.YSize == 5


###############################################################################


@pytest.mark.require_driver("JPEG")
def test_pmtiles_read_jpeg():

    ds = gdal.Open("data/pmtiles/byte_jpg.pmtiles")
    assert ds.RasterCount == 3
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(1).Checksum() != 0
    assert ds.GetRasterBand(2).Checksum() != 0
    assert ds.GetRasterBand(3).Checksum() != 0


###############################################################################


@pytest.mark.require_driver("WEBP")
def test_pmtiles_read_webp():

    ds = gdal.Open("data/pmtiles/byte_webp.pmtiles")
    assert ds.RasterCount == 4
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_AlphaBand
    assert ds.GetRasterBand(1).Checksum() != 0
    assert ds.GetRasterBand(2).Checksum() != 0
    assert ds.GetRasterBand(3).Checksum() != 0
    assert ds.GetRasterBand(4).Checksum() == 4457


###############################################################################


@pytest.mark.require_driver("AVIF")
def test_pmtiles_read_avif():

    ds = gdal.Open("data/pmtiles/byte_avif.pmtiles")
    assert ds.RasterCount == 2
    assert ds.GetRasterBand(1).Checksum() != 0
    assert ds.GetRasterBand(2).Checksum() == 4457


###############################################################################


@pytest.mark.require_driver("JPEG")
def test_pmtiles_read_width_different_height():

    ds = gdal.Open("data/pmtiles/small_world_jpg.pmtiles")
    assert ds.RasterCount == 3
    assert ds.RasterXSize == 256
    assert ds.RasterYSize == 212
    assert ds.GetGeoTransform() == pytest.approx(
        (
            -20037508.342789244,
            156543.03392804097,
            0.0,
            20037508.342789244,
            0.0,
            -156543.03392804097,
        )
    )
    assert ds.GetRasterBand(1).ComputeStatistics(False) == pytest.approx(
        [0.0, 255.0, 44.70631264740566, 62.8515915384191], abs=1
    )
    assert ds.GetRasterBand(2).ComputeStatistics(False) == pytest.approx(
        [0.0, 255.0, 44.77585126768868, 61.4027882670042], abs=1
    )
    assert ds.GetRasterBand(3).ComputeStatistics(False) == pytest.approx(
        [0.0, 255.0, 62.815816627358494, 47.68216081549546], abs=1
    )


###############################################################################


@pytest.mark.require_driver("MBTILES")
@pytest.mark.parametrize("tile_format", ["PNG", "JPEG", "WEBP"])
def test_pmtiles_convert_from_pmtiles(tmp_path, tile_format):

    if gdal.GetDriverByName(tile_format) is None:
        pytest.skip(f"Driver {tile_format} is not available")

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 2, 3)
    src_ds.SetSpatialRef(osr.SpatialReference(epsg=3857))
    # Aligns perfectly with tiling scheme at level = 17
    # 1.194328566955879 = 6378137.0 * math.pi * 2 / (1 << 17) / 256
    src_ds.SetGeoTransform(
        [
            999999.3658264879,
            1.194328566955879,
            0,
            -999999.3658264879,
            0,
            -1.194328566955879,
        ]
    )
    src_ds.GetRasterBand(1).Fill(255)
    src_ds.GetRasterBand(2).Fill(255)
    src_ds.GetRasterBand(3).Fill(255)
    gdal.alg.raster.convert(
        input=src_ds,
        output=tmp_path / "tmp.mbtiles",
        creation_option={"TILE_FORMAT": tile_format},
    )
    gdal.alg.raster.convert(
        input=tmp_path / "tmp.mbtiles", output=tmp_path / "out.pmtiles"
    )

    ds = gdal.Open(tmp_path / "out.pmtiles")
    assert ds.RasterXSize == 1
    assert ds.RasterYSize == 2
    if tile_format == "PNG":
        assert ds.GetMetadataItem("format") == "png"
    elif tile_format == "JPEG":
        assert ds.GetMetadataItem("format") == "jpg"
    else:
        assert ds.GetMetadataItem("format") == "webp"

    if tile_format != "JPEG":
        assert ds.GetRasterBand(1).ComputeRasterMinMax() == (255, 255)


###############################################################################


@pytest.mark.require_driver("MBTILES")
@pytest.mark.parametrize("tile_format", ["PNG", "JPEG", "WEBP"])
@pytest.mark.parametrize("use_gdal_raster_tile", [True, False])
def test_pmtiles_convert_from_non_pmtiles(
    tmp_vsimem, tile_format, use_gdal_raster_tile
):

    if gdal.GetDriverByName(tile_format) is None:
        pytest.skip(f"Driver {tile_format} is not available")

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 2, 3)
    src_ds.SetSpatialRef(osr.SpatialReference(epsg=3857))
    # Aligns perfectly with tiling scheme at level = 17
    # 1.194328566955879 = 6378137.0 * math.pi * 2 / (1 << 17) / 256
    src_ds.SetGeoTransform(
        [
            999999.3658264879,
            1.194328566955879,
            0,
            -999999.3658264879,
            0,
            -1.194328566955879,
        ]
    )
    src_ds.GetRasterBand(1).Fill(255)
    src_ds.GetRasterBand(2).Fill(255)
    src_ds.GetRasterBand(3).Fill(255)
    options = {"TILE_FORMAT": tile_format}
    if not use_gdal_raster_tile:
        options["ZOOM_LEVEL_STRATEGY"] = "LOWER"
    gdal.alg.raster.convert(
        input=src_ds,
        output=tmp_vsimem / "out.pmtiles",
        creation_option=options,
    )

    ds = gdal.Open(tmp_vsimem / "out.pmtiles")
    assert ds.RasterXSize == 1
    assert ds.RasterYSize == 2
    if tile_format == "PNG":
        assert ds.GetMetadataItem("format") == "png"
    elif tile_format == "JPEG":
        assert ds.GetMetadataItem("format") == "jpg"
    else:
        assert ds.GetMetadataItem("format") == "webp"

    if tile_format != "JPEG":
        assert ds.GetRasterBand(1).ComputeRasterMinMax() == (255, 255)


###############################################################################


@pytest.mark.require_driver("MBTILES")
@pytest.mark.require_driver("PNG")
@pytest.mark.parametrize("format", ["PNG", "PNG8"])
@pytest.mark.parametrize(
    "size,expected_ovr_count", [(256, 0), (257, 1), (512, 1), (513, 2)]
)
def test_pmtiles_convert_from_non_pmtiles_auto_add_overviews(
    tmp_path, format, size, expected_ovr_count
):

    src_ds = gdal.GetDriverByName("MEM").Create("", size, size, 3)
    src_ds.SetSpatialRef(osr.SpatialReference(epsg=3857))
    # Aligns perfectly with tiling scheme at level = 17
    # 1.194328566955879 = 6378137.0 * math.pi * 2 / (1 << 17) / 256
    src_ds.SetGeoTransform(
        [
            999999.3658264879,
            1.194328566955879,
            0,
            -999999.3658264879,
            0,
            -1.194328566955879,
        ]
    )
    src_ds.GetRasterBand(1).Fill(255)
    src_ds.GetRasterBand(2).Fill(255)
    src_ds.GetRasterBand(3).Fill(255)
    gdal.alg.raster.convert(
        input=src_ds,
        output=tmp_path / "out.pmtiles",
        creation_option={"TILE_FORMAT": format},
    )

    ds = gdal.Open(tmp_path / "out.pmtiles")
    assert ds.GetRasterBand(1).GetOverviewCount() == expected_ovr_count
