#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdaldem testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os

import gdaltest
import pytest
import test_cli_utilities

from osgeo import gdal, osr

pytestmark = pytest.mark.skipif(
    test_cli_utilities.get_gdaldem_path() is None, reason="gdaldem not available"
)


@pytest.fixture()
def gdaldem_path():
    return test_cli_utilities.get_gdaldem_path()


###############################################################################
# Test gdaldem hillshade


def test_gdaldem_hillshade(gdaldem_path, tmp_path):

    output_tif = str(tmp_path / "n43_hillshade.tif")

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdaldem_path} hillshade -s 111120 -z 30 ../gdrivers/data/n43.tif {output_tif}"
    )
    assert err is None or err == "", "got error/warning"

    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    ds = gdal.Open(output_tif)
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 45587, "Bad checksum"

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(dst_gt[i], abs=1e-10), "Bad geotransform"

    dst_wkt = ds.GetProjectionRef()
    assert dst_wkt.find('AUTHORITY["EPSG","4326"]') != -1, "Bad projection"

    assert ds.GetRasterBand(1).GetNoDataValue() == 0, "Bad nodata value"

    src_ds = None
    ds = None


###############################################################################
# Test gdaldem hillshade


def test_gdaldem_hillshade_compressed_tiled_output(gdaldem_path, tmp_path):

    output_tif = str(tmp_path / "n43_hillshade_compressed_tiled.tif")

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdaldem_path} hillshade -s 111120 -z 30 ../gdrivers/data/n43.tif {output_tif} -co TILED=YES -co COMPRESS=DEFLATE --config GDAL_CACHEMAX 0"
    )
    assert err is None or err == "", "got error/warning"

    ds = gdal.Open(output_tif)
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 45587, "Bad checksum"

    ds = None

    stat_compressed = os.stat(output_tif)

    assert (
        stat_compressed.st_size <= 15027
    ), "compressed size greater than uncompressed one"


###############################################################################
# Test gdaldem hillshade -combined


def test_gdaldem_hillshade_combined(gdaldem_path, tmp_path):

    output_tif = str(tmp_path / "n43_hillshade_combined.tif")

    gdaltest.runexternal(
        f"{gdaldem_path} hillshade -s 111120 -z 30 -combined ../gdrivers/data/n43.tif {output_tif}"
    )

    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    ds = gdal.Open(output_tif)
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 43876, "Bad checksum"

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(dst_gt[i], abs=1e-10), "Bad geotransform"

    dst_wkt = ds.GetProjectionRef()
    assert dst_wkt.find('AUTHORITY["EPSG","4326"]') != -1, "Bad projection"

    assert ds.GetRasterBand(1).GetNoDataValue() == 0, "Bad nodata value"

    src_ds = None
    ds = None


###############################################################################
# Test gdaldem hillshade with -compute_edges


def test_gdaldem_hillshade_compute_edges(gdaldem_path, tmp_path):

    output_tif = str(tmp_path / "n43_hillshade_compute_edges.tif")

    gdaltest.runexternal(
        f"{gdaldem_path} hillshade -compute_edges -s 111120 -z 30 ../gdrivers/data/n43.tif {output_tif}"
    )

    ds = gdal.Open(output_tif)
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 50239, "Bad checksum"

    ds = None


###############################################################################
# Test gdaldem hillshade with -az parameter


def test_gdaldem_hillshade_azimuth(gdaldem_path, tmp_path):

    input_tif = str(tmp_path / "pyramid.tif")
    output_tif = str(tmp_path / "pyramid_shaded.tif")

    ds = gdal.GetDriverByName("GTiff").Create(input_tif, 100, 100, 1)
    ds.SetGeoTransform([2, 0.01, 0, 49, 0, -0.01])
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    ds.SetProjection(sr.ExportToWkt())
    for j in range(100):
        data = ""
        for i in range(100):
            val = 255 - 5 * max(abs(50 - i), abs(50 - j))
            data = data + chr(val)
        data = data.encode("ISO-8859-1")
        ds.GetRasterBand(1).WriteRaster(0, j, 100, 1, data)

    ds = None

    # Light from the east
    gdaltest.runexternal(
        f"{gdaldem_path} hillshade -s 111120 -z 100 -az 90 -co COMPRESS=LZW {input_tif} {output_tif}"
    )

    ds_ref = gdal.Open("data/pyramid_shaded_ref.tif")
    ds = gdal.Open(output_tif)
    assert gdaltest.compare_ds(ds, ds_ref, verbose=1) <= 1, "Bad checksum"
    ds = None
    ds_ref = None


###############################################################################
# Test gdaldem hillshade to PNG


@pytest.mark.require_driver("PNG")
def test_gdaldem_hillshade_png(gdaldem_path, tmp_path):

    output_png = str(tmp_path / "n43_hillshade.png")

    gdaltest.runexternal(
        f"{gdaldem_path} hillshade -of PNG  -s 111120 -z 30 ../gdrivers/data/n43.tif {output_png}"
    )

    ds = gdal.Open(output_png)
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 45587, "Bad checksum"

    ds = None


###############################################################################
# Test gdaldem hillshade to PNG with -compute_edges


@pytest.mark.require_driver("PNG")
def test_gdaldem_hillshade_png_compute_edges(gdaldem_path, tmp_path):

    output_png = str(tmp_path / "n43_hillshade_compute_edges.png")

    gdaltest.runexternal(
        f"{gdaldem_path} hillshade -compute_edges -of PNG  -s 111120 -z 30 ../gdrivers/data/n43.tif {output_png}"
    )

    ds = gdal.Open(output_png)
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 50239, "Bad checksum"

    ds = None


###############################################################################
# Test gdaldem slope


def test_gdaldem_slope(gdaldem_path, tmp_path):

    output_tif = str(tmp_path / "n43_slope.tif")

    gdaltest.runexternal(
        f"{gdaldem_path} slope -s 111120 ../gdrivers/data/n43.tif {output_tif}"
    )

    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    ds = gdal.Open(output_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 63748, "Bad checksum"

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(dst_gt[i], abs=1e-10), "Bad geotransform"

    dst_wkt = ds.GetProjectionRef()
    assert dst_wkt.find('AUTHORITY["EPSG","4326"]') != -1, "Bad projection"

    assert ds.GetRasterBand(1).GetNoDataValue() == -9999.0, "Bad nodata value"

    src_ds = None
    ds = None


###############################################################################
# Test gdaldem aspect


def test_gdaldem_aspect(gdaldem_path, tmp_path):

    output_tif = str(tmp_path / "n43_aspect.tif")

    gdaltest.runexternal(f"{gdaldem_path} aspect ../gdrivers/data/n43.tif {output_tif}")

    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    ds = gdal.Open(output_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 54885, "Bad checksum"

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(dst_gt[i], abs=1e-10), "Bad geotransform"

    dst_wkt = ds.GetProjectionRef()
    assert dst_wkt.find('AUTHORITY["EPSG","4326"]') != -1, "Bad projection"

    assert ds.GetRasterBand(1).GetNoDataValue() == -9999.0, "Bad nodata value"

    src_ds = None
    ds = None


###############################################################################
# Test gdaldem color relief


@pytest.fixture()
def n43_colorrelief_tif(gdaldem_path, tmp_path):
    n43_colorrelief_tif = str(tmp_path / "n43_colorrelief.tif")

    gdaltest.runexternal(
        f"{gdaldem_path} color-relief ../gdrivers/data/n43.tif data/color_file.txt {n43_colorrelief_tif}"
    )

    yield n43_colorrelief_tif


def test_gdaldem_color_relief(gdaldem_path, n43_colorrelief_tif):

    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    ds = gdal.Open(n43_colorrelief_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 55066, "Bad checksum"

    assert ds.GetRasterBand(2).Checksum() == 37594, "Bad checksum"

    assert ds.GetRasterBand(3).Checksum() == 47768, "Bad checksum"

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(dst_gt[i], abs=1e-10), "Bad geotransform"

    dst_wkt = ds.GetProjectionRef()
    assert dst_wkt.find('AUTHORITY["EPSG","4326"]') != -1, "Bad projection"

    src_ds = None
    ds = None


###############################################################################
# Test gdaldem color relief on a GMT .cpt file


def test_gdaldem_color_relief_cpt(gdaldem_path, tmp_path):

    output_tif = str(tmp_path / "n43_colorrelief_cpt.tif")

    gdaltest.runexternal(
        f"{gdaldem_path} color-relief ../gdrivers/data/n43.tif data/color_file.cpt {output_tif}"
    )
    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    ds = gdal.Open(output_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 55066, "Bad checksum"

    assert ds.GetRasterBand(2).Checksum() == 37594, "Bad checksum"

    assert ds.GetRasterBand(3).Checksum() == 47768, "Bad checksum"

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(dst_gt[i], abs=1e-10), "Bad geotransform"

    dst_wkt = ds.GetProjectionRef()
    assert dst_wkt.find('AUTHORITY["EPSG","4326"]') != -1, "Bad projection"

    src_ds = None
    ds = None


###############################################################################
# Test gdaldem color relief to VRT


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdaldem_color_relief_vrt(gdaldem_path, n43_colorrelief_tif, tmp_path):

    output_vrt = str(tmp_path / "n43_colorrelief.vrt")

    gdaltest.runexternal(
        f"{gdaldem_path} color-relief -of VRT ../gdrivers/data/n43.tif data/color_file.txt {output_vrt}"
    )
    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    ds = gdal.Open(output_vrt)
    assert ds is not None

    ds_ref = gdal.Open(n43_colorrelief_tif)
    assert gdaltest.compare_ds(ds, ds_ref, verbose=0) <= 1, "Bad checksum"
    ds_ref = None

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(dst_gt[i], abs=1e-10), "Bad geotransform"

    dst_wkt = ds.GetProjectionRef()
    assert dst_wkt.find('AUTHORITY["EPSG","4326"]') != -1, "Bad projection"

    src_ds = None
    ds = None


###############################################################################
# Test gdaldem color relief from a Float32 dataset


@pytest.fixture()
def n43_float32_tif(tmp_path):

    n43_float32_path = str(tmp_path / "n43_float32.tif")

    gdal.Translate(n43_float32_path, "../gdrivers/data/n43.tif", options="-ot Float32")

    yield n43_float32_path


def test_gdaldem_color_relief_from_float32(gdaldem_path, n43_float32_tif, tmp_path):

    output_tif = str(tmp_path / "n43_colorrelief_from_float32.tif")

    gdaltest.runexternal(
        f"{gdaldem_path} color-relief {n43_float32_tif} data/color_file.txt {output_tif}"
    )
    ds = gdal.Open(output_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 55066, "Bad checksum"

    assert ds.GetRasterBand(2).Checksum() == 37594, "Bad checksum"

    assert ds.GetRasterBand(3).Checksum() == 47768, "Bad checksum"

    ds = None


###############################################################################
# Test gdaldem color relief to PNG


@pytest.mark.require_driver("PNG")
def test_gdaldem_color_relief_png(gdaldem_path, tmp_path):

    output_png = str(tmp_path / "n43_colorrelief.png")

    gdaltest.runexternal(
        f"{gdaldem_path} color-relief -of PNG ../gdrivers/data/n43.tif data/color_file.txt {output_png}"
    )
    ds = gdal.Open(output_png)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 55066, "Bad checksum"

    assert ds.GetRasterBand(2).Checksum() == 37594, "Bad checksum"

    assert ds.GetRasterBand(3).Checksum() == 47768, "Bad checksum"

    ds = None


###############################################################################
# Test gdaldem color relief from a Float32 to PNG


@pytest.mark.require_driver("PNG")
def test_gdaldem_color_relief_from_float32_to_png(
    gdaldem_path, n43_float32_tif, tmp_path
):

    output_png = str(tmp_path / "n43_colorrelief_from_float32.png")

    gdaltest.runexternal(
        f"{gdaldem_path} color-relief -of PNG {n43_float32_tif} data/color_file.txt {output_png}"
    )
    ds = gdal.Open(output_png)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 55066, "Bad checksum"

    assert ds.GetRasterBand(2).Checksum() == 37594, "Bad checksum"

    assert ds.GetRasterBand(3).Checksum() == 47768, "Bad checksum"

    ds = None


###############################################################################
# Test gdaldem color relief with -nearest_color_entry


def test_gdaldem_color_relief_nearest_color_entry(gdaldem_path, tmp_path):

    output_tif = str(tmp_path / "n43_colorrelief_nearest.tif")

    gdaltest.runexternal(
        f"{gdaldem_path} color-relief -nearest_color_entry ../gdrivers/data/n43.tif data/color_file.txt {output_tif}"
    )
    ds = gdal.Open(output_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 57296, "Bad checksum"

    assert ds.GetRasterBand(2).Checksum() == 42926, "Bad checksum"

    assert ds.GetRasterBand(3).Checksum() == 47181, "Bad checksum"

    ds = None


###############################################################################
# Test gdaldem color relief with -nearest_color_entry and -of VRT


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdaldem_color_relief_nearest_color_entry_vrt(gdaldem_path, tmp_path):

    output_vrt = str(tmp_path / "n43_colorrelief_nearest.vrt")

    gdaltest.runexternal(
        f"{gdaldem_path} color-relief -of VRT -nearest_color_entry ../gdrivers/data/n43.tif data/color_file.txt {output_vrt}"
    )
    ds = gdal.Open(output_vrt)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 57296, "Bad checksum"

    assert ds.GetRasterBand(2).Checksum() == 42926, "Bad checksum"

    assert ds.GetRasterBand(3).Checksum() == 47181, "Bad checksum"

    ds = None


###############################################################################
# Test gdaldem color relief with a nan nodata


@pytest.mark.require_driver("AAIGRID")
def test_gdaldem_color_relief_nodata_nan(gdaldem_path, tmp_path):

    input_asc = str(tmp_path / "nodata_nan_src.asc")
    colors_txt = str(tmp_path / "nodata_nan_plt.txt")
    output_tif = str(tmp_path / "nodata_nan_out.tif")

    f = open(input_asc, "wt")
    f.write("""ncols        2
nrows        2
xllcorner    440720
yllcorner    3750120
cellsize     60
NODATA_value nan
 0.0 0
 0 nan""")
    f.close()

    f = open(colors_txt, "wt")
    f.write("0 0 0 0\n")
    f.write("nv 1 1 1\n")
    f.close()

    gdaltest.runexternal(
        f"{gdaldem_path} color-relief {input_asc} {colors_txt} {output_tif}"
    )

    ds = gdal.Open(output_tif)
    val = ds.GetRasterBand(1).ReadRaster()
    ds = None

    import struct

    val = struct.unpack("B" * 4, val)
    assert val == (0, 0, 0, 1)


###############################################################################
# Test gdaldem color relief with entries with repeated DEM values in the color table (#6422)


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
@pytest.mark.require_driver("AAIGRID")
def test_gdaldem_color_relief_repeated_entry(gdaldem_path, tmp_path):

    input_asc = str(tmp_path / "test_gdaldem_color_relief_repeated_entry.asc")
    colors_txt = str(tmp_path / "test_gdaldem_color_relief_repeated_entry.txt")
    output_tif = str(tmp_path / "test_gdaldem_color_relief_repeated_entry_out.tif")
    output_vrt = str(tmp_path / "test_gdaldem_color_relief_repeated_entry_out.vrt")

    f = open(input_asc, "wt")
    f.write("""ncols        2
nrows        3
xllcorner    440720
yllcorner    3750120
cellsize     60
NODATA_value 5
 1 4.9
 5 5.1
 6 7 """)
    f.close()

    f = open(colors_txt, "wt")
    f.write("1 1 1 1\n")
    f.write("6 10 10 10\n")
    f.write("6 20 20 20\n")
    f.write("8 30 30 30\n")
    f.write("nv 5 5 5\n")
    f.close()

    gdaltest.runexternal(
        f"{gdaldem_path} color-relief {input_asc} {colors_txt} {output_tif}",
        display_live_on_parent_stdout=True,
    )

    ds = gdal.Open(output_tif)
    val = ds.GetRasterBand(1).ReadRaster()
    ds = None

    import struct

    val = struct.unpack("B" * 6, val)
    assert val == (1, 1, 5, 10, 10, 25)

    gdaltest.runexternal(
        f"{gdaldem_path} color-relief {input_asc} {colors_txt} {output_vrt} -of VRT",
        display_live_on_parent_stdout=True,
    )

    ds = gdal.Open(output_vrt)
    val = ds.GetRasterBand(1).ReadRaster()
    ds = None

    val = struct.unpack("B" * 6, val)
    assert val == (1, 1, 5, 10, 10, 25)
