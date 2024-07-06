#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal2tiles.py testing
# Author:   Even Rouault <even dot rouault @ spatialys dot com>
#
###############################################################################
# Copyright (c) 2015, Even Rouault <even dot rouault @ spatialys dot com>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################

import glob
import os
import os.path
import shutil
import struct
import sys

import gdaltest
import pytest
import test_py_scripts  # noqa  # pylint: disable=E0401

from osgeo import gdal, osr  # noqa
from osgeo_utils.gdalcompare import compare_db

pytestmark = pytest.mark.skipif(
    test_py_scripts.get_py_script("gdal2tiles") is None,
    reason="gdal2tiles not available",
)


@pytest.fixture()
def script_path():
    return test_py_scripts.get_py_script("gdal2tiles")


###############################################################################
#


def test_gdal2tiles_help(script_path):

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdal2tiles", "--help"
    )


###############################################################################
#


def test_gdal2tiles_version(script_path):

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdal2tiles", "--version"
    )


def pil_available():
    try:
        import numpy  # noqa
        from PIL import Image  # noqa

        import osgeo.gdal_array as gdalarray  # noqa

        return True
    except ImportError:
        return False


def _verify_raster_band_checksums(filename, expected_cs=[]):
    ds = gdal.Open(filename)
    if ds is None:
        pytest.fail('cannot open output file "%s"' % filename)

    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]
    if isinstance(expected_cs[0], list):
        assert got_cs in expected_cs
    else:
        assert got_cs == expected_cs

    ds = None


@pytest.mark.require_driver("PNG")
def test_gdal2tiles_py_simple(script_path, tmp_path):

    input_tif = str(tmp_path / "out_gdal2tiles_smallworld.tif")

    shutil.copy(
        test_py_scripts.get_data_path("gdrivers") + "small_world.tif",
        input_tif,
    )

    # test execution without specifying output directory
    prev_wd = os.getcwd()
    try:
        os.chdir(tmp_path)
        _, err = test_py_scripts.run_py_script(
            script_path, "gdal2tiles", f"-q {input_tif}", return_stderr=True
        )
    finally:
        os.chdir(prev_wd)

    assert "UseExceptions" not in err

    _verify_raster_band_checksums(
        f"{tmp_path}/out_gdal2tiles_smallworld/0/0/0.png",
        expected_cs=[31420, 32522, 16314, 17849],
    )

    for filename in [
        "googlemaps.html",
        "leaflet.html",
        "openlayers.html",
        "tilemapresource.xml",
    ]:
        assert os.path.exists(f"{tmp_path}/out_gdal2tiles_smallworld/" + filename), (
            "%s missing" % filename
        )


@pytest.mark.require_driver("PNG")
def test_gdal2tiles_py_zoom_option(script_path, tmp_path):

    tiles_dir = str(tmp_path / "out_gdal2tiles_smallworld")

    # Because of multiprocessing, run as external process, to avoid issues
    # for example on Windows that doesn't manage to fork
    # Also test non-file input dataset (e.g. vrt://)
    test_py_scripts.run_py_script_as_external_script(
        script_path,
        "gdal2tiles",
        "-q --force-kml --processes=2 -z 0-1 "
        + "vrt://"
        + test_py_scripts.get_data_path("gdrivers")
        + f"small_world.tif {tiles_dir}",
    )

    _verify_raster_band_checksums(
        f"{tiles_dir}/1/0/0.png",
        expected_cs=[24063, 23632, 14707, 17849],
    )

    assert not os.path.exists(f"{tiles_dir}/0/0/0.png.aux.xml")
    assert not os.path.exists(f"{tiles_dir}/1/0/0.png.aux.xml")

    if gdal.GetDriverByName("KMLSuperOverlay") is None:
        pytest.skip("KMLSuperOverlay driver missing")

    ds = gdal.Open(f"{tiles_dir}/doc.kml")
    assert ds is not None, "did not get kml"


@pytest.mark.require_driver("PNG")
@pytest.mark.parametrize(
    "resample",
    [
        "average",
        "near",
        "bilinear",
        "cubic",
        "cubicspline",
        "lanczos",
        "antialias",
        "mode",
        "max",
        "min",
        "med",
        "q1",
        "q3",
    ],
)
def test_gdal2tiles_py_resampling_option(script_path, tmp_path, resample):

    if resample == "antialias" and not pil_available():
        pytest.skip("'antialias' resampling is not available")

    out_dir = str(tmp_path / "out_gdal2tiles_smallworld")

    test_py_scripts.run_py_script_as_external_script(
        script_path,
        "gdal2tiles",
        "-q --resampling={0} {1} {2}".format(
            resample,
            test_py_scripts.get_data_path("gdrivers") + "small_world.tif",
            out_dir,
        ),
    )

    # very basic check
    ds = gdal.Open(f"{out_dir}/0/0/0.png")
    assert ds is not None, f"resample option {resample} failed"
    ds = None


@pytest.mark.require_driver("PNG")
def test_gdal2tiles_py_xyz(script_path, tmp_path):

    if gdaltest.is_travis_branch("sanitize"):
        pytest.skip("fails on sanitize for unknown reason")

    input_tif = str(tmp_path / "out_gdal2tiles_smallworld_xyz.tif")
    out_dir = input_tif.strip(".tif")

    shutil.copy(
        test_py_scripts.get_data_path("gdrivers") + "small_world.tif",
        input_tif,
    )

    ret = test_py_scripts.run_py_script(
        script_path,
        "gdal2tiles",
        f"-q --xyz --zoom=0-1 {input_tif} {out_dir}",
    )

    assert "ERROR ret code" not in ret

    _verify_raster_band_checksums(
        f"{out_dir}/0/0/0.png",
        expected_cs=[31747, 33381, 18447, 17849],
    )
    _verify_raster_band_checksums(
        f"{out_dir}/1/0/0.png",
        expected_cs=[15445, 16942, 13681, 17849],
    )

    for filename in ["googlemaps.html", "leaflet.html", "openlayers.html"]:
        assert os.path.exists(f"{out_dir}/{filename}"), "%s missing" % filename
    assert not os.path.exists(f"{out_dir}/tilemapresource.xml")


@pytest.mark.require_driver("PNG")
def test_gdal2tiles_py_invalid_srs(script_path, tmp_path):
    """
    Case where the input image is not georeferenced, i.e. it's missing the SRS info,
    and no --s_srs option is provided. The script should fail validation and terminate.
    """

    if gdaltest.is_travis_branch("sanitize"):
        pytest.skip("fails on sanitize for unknown reason")

    input_vrt = str(tmp_path / "out_gdal2tiles_test_nosrs.vrt")
    byte_tif = str(tmp_path / "byte.tif")
    output_dir = input_vrt.strip(".vrt")

    shutil.copy(
        test_py_scripts.get_data_path("gdrivers") + "test_nosrs.vrt",
        input_vrt,
    )
    shutil.copy(test_py_scripts.get_data_path("gdrivers") + "byte.tif", byte_tif)

    # try running on image with missing SRS
    ret = test_py_scripts.run_py_script(
        script_path, "gdal2tiles", f"-q --zoom=0-1 {input_vrt} {output_dir}"
    )

    assert "ERROR ret code = 2" in ret

    # this time pass the spatial reference system via cli options
    ret2 = test_py_scripts.run_py_script(
        script_path,
        "gdal2tiles",
        f"-q --zoom=0-1 --s_srs EPSG:4326 {input_vrt}",
    )

    assert "ERROR ret code" not in ret2


def test_does_not_error_when_source_bounds_close_to_tiles_bound(script_path, tmp_path):
    """
    Case where the border coordinate of the input file is inside a tile T but the first pixel is
    actually assigned to the tile next to T (nearest neighbour), meaning that when the query is done
    to get the content of T, nothing is returned from the raster.
    """
    in_files = [
        "./data/test_bounds_close_to_tile_bounds_x.vrt",
        "./data/test_bounds_close_to_tile_bounds_y.vrt",
    ]
    out_folder = str(tmp_path / "out_gdal2tiles_bounds_approx")

    try:
        for in_file in in_files:
            test_py_scripts.run_py_script(
                script_path, "gdal2tiles", "-q -z 21-21 %s %s" % (in_file, out_folder)
            )
    except TypeError:
        pytest.fail(
            "Case of tile not getting any data not handled properly "
            "(tiles at the border of the image)"
        )


def test_does_not_error_when_nothing_to_put_in_the_low_zoom_tile(script_path, tmp_path):
    """
    Case when the highest zoom level asked is actually too low for any pixel of the raster to be
    selected
    """
    in_file = "./data/test_bounds_close_to_tile_bounds_x.vrt"
    out_folder = str(tmp_path / "out_gdal2tiles_bounds_approx")

    try:
        test_py_scripts.run_py_script(
            script_path, "gdal2tiles", "-q -z 10 %s %s" % (in_file, out_folder)
        )
    except TypeError:
        pytest.fail(
            "Case of low level tile not getting any data not handled properly "
            "(tile at a zoom level too low)"
        )


@pytest.mark.require_driver("PNG")
def test_handle_utf8_filename(script_path, tmp_path):
    input_file = "data/test_utf8_漢字.vrt"

    out_folder = str(tmp_path / "utf8_test")

    args = f"-q -z 21 {input_file} {out_folder}"

    test_py_scripts.run_py_script(script_path, "gdal2tiles", args)

    openlayers_html = open(
        os.path.join(out_folder, "openlayers.html"), "rt", encoding="utf-8"
    ).read()
    assert "<title>test_utf8_漢字.vrt</title>" in openlayers_html


@pytest.mark.require_driver("PNG")
def test_exclude_transparent_tiles(script_path, tmp_path):

    output_folder = str(tmp_path / "test_exclude_transparent_tiles")
    os.makedirs(output_folder)

    test_py_scripts.run_py_script_as_external_script(
        script_path,
        "gdal2tiles",
        "-x -z 14-16 data/test_gdal2tiles_exclude_transparent.tif %s" % output_folder,
    )

    # First row totally transparent - no tiles
    tiles_folder = os.path.join(output_folder, "15", "21898")
    dir_files = os.listdir(tiles_folder)
    assert not dir_files, "Generated empty tiles for row 21898: %s" % dir_files

    # Second row - only 2 non-transparent tiles
    tiles_folder = os.path.join(output_folder, "15", "21899")
    dir_files = sorted(os.listdir(tiles_folder))
    assert ["22704.png", "22705.png"] == dir_files, (
        "Generated empty tiles for row 21899: %s" % dir_files
    )

    # Third row - only 1 non-transparent tile
    tiles_folder = os.path.join(output_folder, "15", "21900")
    dir_files = os.listdir(tiles_folder)
    assert ["22705.png"] == dir_files, (
        "Generated empty tiles for row 21900: %s" % dir_files
    )


@pytest.mark.require_driver("PNG")
def test_gdal2tiles_py_profile_raster(script_path, tmp_path):

    out_folder = str(tmp_path / "out_gdal2tiles_smallworld")

    test_py_scripts.run_py_script_as_external_script(
        script_path,
        "gdal2tiles",
        "-q -p raster -z 0-1 "
        + test_py_scripts.get_data_path("gdrivers")
        + f"small_world.tif {out_folder}",
    )

    _verify_raster_band_checksums(
        f"{out_folder}/0/0/0.png",
        expected_cs=[10125, 10802, 27343, 48852],
    )
    _verify_raster_band_checksums(
        f"{out_folder}/1/0/0.png",
        expected_cs=[62125, 59756, 43894, 38539],
    )

    if gdal.GetDriverByName("KMLSuperOverlay") is None:
        pytest.skip("KMLSuperOverlay driver missing")

    if sys.platform != "win32":
        # For some reason, the checksums on the kml file on Windows are the ones of the below png
        _verify_raster_band_checksums(
            f"{out_folder}/0/0/0.kml",
            expected_cs=[29839, 34244, 42706, 64319],
        )


@pytest.mark.require_driver("PNG")
def test_gdal2tiles_py_profile_raster_oversample(script_path, tmp_path):

    out_folder = str(tmp_path / "out_gdal2tiles_smallworld")

    test_py_scripts.run_py_script_as_external_script(
        script_path,
        "gdal2tiles",
        "-q -p raster -z 0-2 "
        + test_py_scripts.get_data_path("gdrivers")
        + f"small_world.tif {out_folder}",
    )

    assert os.path.exists(f"{out_folder}/2/0/0.png")
    assert os.path.exists(f"{out_folder}/2/3/1.png")
    _verify_raster_band_checksums(
        f"{out_folder}/2/0/0.png",
        expected_cs=[[51434, 55441, 63427, 17849], [51193, 55320, 63324, 17849]],  # icc
    )
    _verify_raster_band_checksums(
        f"{out_folder}/2/3/1.png",
        expected_cs=[[44685, 45074, 50871, 56563], [44643, 45116, 50863, 56563]],  # icc
    )


@pytest.mark.require_driver("PNG")
def test_gdal2tiles_py_profile_raster_xyz(script_path, tmp_path):

    out_folder = str(tmp_path / "out_gdal2tiles_smallworld")

    test_py_scripts.run_py_script_as_external_script(
        script_path,
        "gdal2tiles",
        "-q -p raster --xyz -z 0-1 "
        + test_py_scripts.get_data_path("gdrivers")
        + f"small_world.tif {out_folder}",
    )

    _verify_raster_band_checksums(
        f"{out_folder}/0/0/0.png",
        expected_cs=[11468, 10719, 27582, 48827],
    )
    _verify_raster_band_checksums(
        f"{out_folder}/1/0/0.png",
        expected_cs=[60550, 62572, 46338, 38489],
    )

    if gdal.GetDriverByName("KMLSuperOverlay") is None:
        pytest.skip("KMLSuperOverlay driver missing")

    if sys.platform != "win32":
        # For some reason, the checksums on the kml file on Windows are the ones of the below png
        _verify_raster_band_checksums(
            f"{out_folder}/0/0/0.kml",
            expected_cs=[27644, 31968, 38564, 64301],
        )


@pytest.mark.require_driver("PNG")
def test_gdal2tiles_py_profile_geodetic_tmscompatible_xyz(script_path, tmp_path):

    out_folder = str(tmp_path / "out_gdal2tiles_smallworld")

    test_py_scripts.run_py_script_as_external_script(
        script_path,
        "gdal2tiles",
        "-q -p geodetic --tmscompatible --xyz -z 0-1 "
        + test_py_scripts.get_data_path("gdrivers")
        + f"small_world.tif {out_folder}",
    )

    _verify_raster_band_checksums(
        f"{out_folder}/0/0/0.png",
        expected_cs=[8560, 8031, 7209, 17849],
    )
    _verify_raster_band_checksums(
        f"{out_folder}/1/0/0.png",
        expected_cs=[2799, 3468, 8686, 17849],
    )

    if gdal.GetDriverByName("KMLSuperOverlay") is None:
        pytest.skip("KMLSuperOverlay driver missing")

    if sys.platform != "win32":
        # For some reason, the checksums on the kml file on Windows are the ones of the below png
        _verify_raster_band_checksums(
            f"{out_folder}/0/0/0.kml",
            expected_cs=[12361, 18212, 21827, 5934],
        )


@pytest.mark.require_driver("PNG")
def test_gdal2tiles_py_mapml(script_path, tmp_path):

    input_tif = str(tmp_path / "byte_APS.tif")
    output_folder = str(tmp_path / "out_gdal2tiles_mapml")

    gdal.Translate(
        input_tif,
        test_py_scripts.get_data_path("gcore") + "byte.tif",
        options="-a_srs EPSG:5936 -a_ullr 0 40 40 0",
    )

    test_py_scripts.run_py_script_as_external_script(
        script_path,
        "gdal2tiles",
        f'-q -p APSTILE -w mapml -z 16-18 --url "https://foo" {input_tif} {output_folder}',
    )

    mapml = open(f"{output_folder}/mapml.mapml", "rb").read().decode("utf-8")
    # print(mapml)
    assert '<extent units="APSTILE">' in mapml
    assert '<input name="z" type="zoom" value="18" min="16" max="18" />' in mapml
    assert (
        '<input name="x" type="location" axis="column" units="tilematrix" min="122496" max="122496" />'
        in mapml
    )
    assert (
        '<input name="y" type="location" axis="row" units="tilematrix" min="139647" max="139647" />'
        in mapml
    )
    assert (
        '<link tref="https://foo/out_gdal2tiles_mapml/{z}/{x}/{y}.png" rel="tile" />'
        in mapml
    )


def _convert_png_to_webp(frm, to, quality):
    src_ds = gdal.Open(frm)
    driver = gdal.GetDriverByName("WEBP")
    driver.CreateCopy(to, src_ds, 0, options=["LOSSLESS=True"])


@pytest.mark.require_driver("WEBP")
@pytest.mark.parametrize("resampling", ("average", "antialias"))
def test_gdal2tiles_py_webp(script_path, tmp_path, resampling):

    if resampling == "antialias" and not pil_available():
        pytest.skip("'antialias' resampling is not available")

    out_dir_png = str(tmp_path / "out_gdal2tiles_smallworld_png")
    out_dir_webp = str(tmp_path / "out_gdal2tiles_smallworld_webp")
    out_dir_webp_from_png = str(tmp_path / "out_gdal2tiles_smallworld_webp_from_png")

    base_args = "-q --processes=2 -z 0-1 -r " + resampling + " "
    test_py_scripts.run_py_script_as_external_script(
        script_path,
        "gdal2tiles",
        base_args
        + test_py_scripts.get_data_path("gdrivers")
        + f"small_world.tif {out_dir_png}",
    )

    quality = 50
    test_py_scripts.run_py_script_as_external_script(
        script_path,
        "gdal2tiles",
        base_args
        + "--tiledriver=WEBP --webp-lossless "
        + test_py_scripts.get_data_path("gdrivers")
        + f"small_world.tif {out_dir_webp}",
    )

    to_convert = glob.glob(f"{out_dir_png}/*/*/*.png")
    for filename in to_convert:
        to_filename = filename.replace(
            out_dir_png,
            out_dir_webp_from_png,
        )
        to_filename = to_filename.replace(".png", ".webp")
        to_folder = os.path.dirname(to_filename)
        os.makedirs(to_folder, exist_ok=True)

        _convert_png_to_webp(filename, to_filename, quality)

    to_compare = glob.glob(f"{out_dir_webp_from_png}/*/*/*.webp")
    for filename in to_compare:
        webp_filename = filename.replace(
            out_dir_webp_from_png,
            out_dir_webp,
        )
        diff_found = compare_db(gdal.Open(webp_filename), gdal.Open(filename))
        assert not diff_found, (resampling, filename)


@pytest.mark.require_driver("PNG")
def test_gdal2tiles_excluded_values(script_path, tmp_path):

    input_tif = str(tmp_path / "test_gdal2tiles_excluded_values.tif")
    output_folder = str(tmp_path / "test_gdal2tiles_excluded_values")

    src_ds = gdal.GetDriverByName("GTiff").Create(input_tif, 256, 256, 3, gdal.GDT_Byte)
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
    src_ds = None

    test_py_scripts.run_py_script_as_external_script(
        script_path,
        "gdal2tiles",
        f"-q -z 0-1 --excluded-values=30,31,32 --excluded-values-pct-threshold=50 {input_tif} {output_folder}",
    )

    ds = gdal.Open(f"{output_folder}/0/0/0.png")
    assert struct.unpack("B" * 4, ds.ReadRaster(0, 0, 1, 1)) == (
        (10 + 20 + 40) // 3,
        (11 + 21 + 41) // 3,
        (12 + 22 + 42) // 3,
        255,
    )


@pytest.mark.require_driver("PNG")
def test_gdal2tiles_nodata_values_pct_threshold(script_path, tmp_path):

    input_tif = str(tmp_path / "test_gdal2tiles_nodata_values_pct_threshold.tif")
    output_folder = str(tmp_path / "test_gdal2tiles_nodata_values_pct_threshold")

    src_ds = gdal.GetDriverByName("GTiff").Create(input_tif, 256, 256, 3, gdal.GDT_Byte)
    src_ds.GetRasterBand(1).SetNoDataValue(20)
    src_ds.GetRasterBand(1).WriteRaster(
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
    src_ds = None

    test_py_scripts.run_py_script_as_external_script(
        script_path,
        "gdal2tiles",
        f"-q -z 0-1 {input_tif} {output_folder}",
    )

    ds = gdal.Open(f"{output_folder}/0/0/0.png")
    assert struct.unpack("B" * 2, ds.ReadRaster(0, 0, 1, 1, band_list=[1, 4])) == (
        round((10 + 30 + 40) / 3),
        255,
    )

    test_py_scripts.run_py_script_as_external_script(
        script_path,
        "gdal2tiles",
        f"-q -z 0-1 --nodata-values-pct-threshold=50 {input_tif} {output_folder}",
    )

    ds = gdal.Open(f"{output_folder}/0/0/0.png")
    assert struct.unpack("B" * 2, ds.ReadRaster(0, 0, 1, 1, band_list=[1, 4])) == (
        round((10 + 30 + 40) / 3),
        255,
    )

    test_py_scripts.run_py_script_as_external_script(
        script_path,
        "gdal2tiles",
        f"-q -z 0-1 --nodata-values-pct-threshold=25 {input_tif} {output_folder}",
    )

    ds = gdal.Open(f"{output_folder}/0/0/0.png")
    assert struct.unpack("B" * 2, ds.ReadRaster(0, 0, 1, 1, band_list=[1, 4])) == (0, 0)


@pytest.mark.require_driver("JPEG")
@pytest.mark.parametrize(
    "resampling, expected_stats_z0, expected_stats_z1",
    (
        (
            "average",
            [
                [0.0, 255.0, 62.789886474609375, 71.57543623020909],
                [0.0, 255.0, 62.98188781738281, 70.54545410356597],
                [0.0, 255.0, 77.94142150878906, 56.07427114858068],
            ],
            [
                [0.0, 255.0, 63.620819091796875, 68.38688881060699],
                [0.0, 255.0, 63.620819091796875, 68.38688881060699],
                [0.0, 255.0, 87.09403991699219, 53.07665243601322],
            ],
        ),
        (
            "antialias",
            [
                [0.0, 255.0, 62.66636657714844, 71.70766144632985],
                [0.0, 255.0, 62.91070556640625, 70.705889259777],
                [0.0, 255.0, 77.78370666503906, 56.251290816620596],
            ],
            [
                [0.0, 255.0, 63.61163330078125, 68.49625328462534],
                [0.0, 255.0, 63.61163330078125, 68.49625328462534],
                [0.0, 255.0, 87.04747009277344, 53.1751939061486],
            ],
        ),
    ),
)
def test_gdal2tiles_py_jpeg_3band_input(
    script_path, tmp_path, resampling, expected_stats_z0, expected_stats_z1
):

    if resampling == "antialias" and not pil_available():
        pytest.skip("'antialias' resampling is not available")

    out_dir_jpeg = str(tmp_path / "out_gdal2tiles_smallworld_jpeg")

    test_py_scripts.run_py_script_as_external_script(
        script_path,
        "gdal2tiles",
        "-q -z 0-1 -r "
        + resampling
        + " --tiledriver=JPEG "
        + test_py_scripts.get_data_path("gdrivers")
        + f"small_world.tif {out_dir_jpeg}",
    )

    ds = gdal.Open(f"{out_dir_jpeg}/0/0/0.jpg")
    got_stats_0 = [
        ds.GetRasterBand(i + 1).ComputeStatistics(approx_ok=0)
        for i in range(ds.RasterCount)
    ]

    ds = gdal.Open(f"{out_dir_jpeg}/1/0/0.jpg")
    got_stats_1 = [
        ds.GetRasterBand(i + 1).ComputeStatistics(approx_ok=0)
        for i in range(ds.RasterCount)
    ]

    for i in range(ds.RasterCount):
        assert got_stats_0[i] == pytest.approx(expected_stats_z0[i], rel=0.05), (
            i,
            got_stats_0,
            got_stats_1,
        )

    for i in range(ds.RasterCount):
        assert got_stats_1[i] == pytest.approx(expected_stats_z1[i], rel=0.05), (
            i,
            got_stats_0,
            got_stats_1,
        )


@pytest.mark.require_driver("JPEG")
@pytest.mark.parametrize(
    "resampling, expected_stats_z14, expected_stats_z13",
    (
        (
            (
                "average",
                [[0.0, 255.0, 44.11726379394531, 61.766206763153946]],
                [[0.0, 255.0, 11.057342529296875, 36.182401045647644]],
            ),
            (
                "antialias",
                [[0.0, 255.0, 43.9254150390625, 61.58666064861184]],
                [[0.0, 255.0, 11.013427734375, 36.12022842174338]],
            ),
        )
    ),
)
def test_gdal2tiles_py_jpeg_1band_input(
    script_path, tmp_path, resampling, expected_stats_z14, expected_stats_z13
):

    if resampling == "antialias" and not pil_available():
        pytest.skip("'antialias' resampling is not available")

    out_dir_jpeg = str(tmp_path / "out_gdal2tiles_byte_jpeg")

    test_py_scripts.run_py_script_as_external_script(
        script_path,
        "gdal2tiles",
        "-q -z 13-14 -r "
        + resampling
        + " --tiledriver=JPEG "
        + test_py_scripts.get_data_path("gcore")
        + f"byte.tif {out_dir_jpeg}",
    )

    ds = gdal.Open(f"{out_dir_jpeg}/14/2838/9833.jpg")
    got_stats_14 = [
        ds.GetRasterBand(i + 1).ComputeStatistics(approx_ok=0)
        for i in range(ds.RasterCount)
    ]

    ds = gdal.Open(f"{out_dir_jpeg}/13/1419/4916.jpg")
    got_stats_13 = [
        ds.GetRasterBand(i + 1).ComputeStatistics(approx_ok=0)
        for i in range(ds.RasterCount)
    ]

    for i in range(ds.RasterCount):
        assert got_stats_14[i] == pytest.approx(expected_stats_z14[i], rel=0.05), (
            i,
            got_stats_14,
            got_stats_13,
        )
    for i in range(ds.RasterCount):
        assert got_stats_13[i] == pytest.approx(expected_stats_z13[i], rel=0.05), (
            i,
            got_stats_14,
            got_stats_13,
        )
