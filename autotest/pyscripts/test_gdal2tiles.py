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
import sys

import pytest
import test_py_scripts  # noqa  # pylint: disable=E0401

from osgeo import gdal  # noqa
from osgeo_utils.gdalcompare import compare_db


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


def test_gdal2tiles_py_simple():
    script_path = test_py_scripts.get_py_script("gdal2tiles")
    if script_path is None:
        pytest.skip()

    shutil.copy(
        test_py_scripts.get_data_path("gdrivers") + "small_world.tif",
        "tmp/out_gdal2tiles_smallworld.tif",
    )

    os.chdir("tmp")
    test_py_scripts.run_py_script(
        script_path, "gdal2tiles", "-q out_gdal2tiles_smallworld.tif"
    )
    os.chdir("..")

    os.unlink("tmp/out_gdal2tiles_smallworld.tif")

    _verify_raster_band_checksums(
        "tmp/out_gdal2tiles_smallworld/0/0/0.png",
        expected_cs=[31420, 32522, 16314, 17849],
    )

    for filename in [
        "googlemaps.html",
        "leaflet.html",
        "openlayers.html",
        "tilemapresource.xml",
    ]:
        assert os.path.exists("tmp/out_gdal2tiles_smallworld/" + filename), (
            "%s missing" % filename
        )


def test_gdal2tiles_py_zoom_option():

    script_path = test_py_scripts.get_py_script("gdal2tiles")
    if script_path is None:
        pytest.skip()

    shutil.rmtree("tmp/out_gdal2tiles_smallworld", ignore_errors=True)

    # Because of multiprocessing, run as external process, to avoid issues with
    # Ubuntu 12.04 and socket.setdefaulttimeout()
    # as well as on Windows that doesn't manage to fork
    test_py_scripts.run_py_script_as_external_script(
        script_path,
        "gdal2tiles",
        "-q --force-kml --processes=2 -z 0-1 "
        + test_py_scripts.get_data_path("gdrivers")
        + "small_world.tif tmp/out_gdal2tiles_smallworld",
    )

    _verify_raster_band_checksums(
        "tmp/out_gdal2tiles_smallworld/1/0/0.png",
        expected_cs=[24063, 23632, 14707, 17849],
    )

    assert not os.path.exists("tmp/out_gdal2tiles_smallworld/0/0/0.png.aux.xml")
    assert not os.path.exists("tmp/out_gdal2tiles_smallworld/1/0/0.png.aux.xml")

    ds = gdal.Open("tmp/out_gdal2tiles_smallworld/doc.kml")
    assert ds is not None, "did not get kml"


def test_gdal2tiles_py_resampling_option():

    script_path = test_py_scripts.get_py_script("gdal2tiles")
    if script_path is None:
        pytest.skip()

    resampling_list = [
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
    ]
    try:
        import numpy
        from PIL import Image

        import osgeo.gdal_array as gdalarray

        del Image, numpy, gdalarray
    except ImportError:
        # 'antialias' resampling is not available
        resampling_list.remove("antialias")

    out_dir = "tmp/out_gdal2tiles_smallworld"

    for resample in resampling_list:

        shutil.rmtree(out_dir, ignore_errors=True)

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
        ds = gdal.Open("tmp/out_gdal2tiles_smallworld/0/0/0.png")
        if ds is None:
            pytest.fail("resample option {0!r} failed".format(resample))
        ds = None

    shutil.rmtree(out_dir, ignore_errors=True)


def test_gdal2tiles_py_xyz():
    script_path = test_py_scripts.get_py_script("gdal2tiles")
    if script_path is None:
        pytest.skip()

    try:
        shutil.copy(
            test_py_scripts.get_data_path("gdrivers") + "small_world.tif",
            "tmp/out_gdal2tiles_smallworld_xyz.tif",
        )

        os.chdir("tmp")
        ret = test_py_scripts.run_py_script(
            script_path,
            "gdal2tiles",
            "-q --xyz --zoom=0-1 out_gdal2tiles_smallworld_xyz.tif",
        )
        os.chdir("..")

        assert "ERROR ret code" not in ret

        os.unlink("tmp/out_gdal2tiles_smallworld_xyz.tif")

        _verify_raster_band_checksums(
            "tmp/out_gdal2tiles_smallworld_xyz/0/0/0.png",
            expected_cs=[31747, 33381, 18447, 17849],
        )
        _verify_raster_band_checksums(
            "tmp/out_gdal2tiles_smallworld_xyz/1/0/0.png",
            expected_cs=[15445, 16942, 13681, 17849],
        )

        for filename in ["googlemaps.html", "leaflet.html", "openlayers.html"]:
            assert os.path.exists("tmp/out_gdal2tiles_smallworld_xyz/" + filename), (
                "%s missing" % filename
            )
        assert not os.path.exists(
            "tmp/out_gdal2tiles_smallworld_xyz/tilemapresource.xml"
        )
    finally:
        shutil.rmtree("tmp/out_gdal2tiles_smallworld_xyz")


def test_gdal2tiles_py_invalid_srs():
    """
    Case where the input image is not georeferenced, i.e. it's missing the SRS info,
    and no --s_srs option is provided. The script should fail validation and terminate.
    """
    script_path = test_py_scripts.get_py_script("gdal2tiles")
    if script_path is None:
        pytest.skip()

    shutil.copy(
        test_py_scripts.get_data_path("gdrivers") + "test_nosrs.vrt",
        "tmp/out_gdal2tiles_test_nosrs.vrt",
    )
    shutil.copy(test_py_scripts.get_data_path("gdrivers") + "byte.tif", "tmp/byte.tif")

    os.chdir("tmp")
    # try running on image with missing SRS
    ret = test_py_scripts.run_py_script(
        script_path, "gdal2tiles", "-q --zoom=0-1 out_gdal2tiles_test_nosrs.vrt"
    )

    # this time pass the spatial reference system via cli options
    ret2 = test_py_scripts.run_py_script(
        script_path,
        "gdal2tiles",
        "-q --zoom=0-1 --s_srs EPSG:4326 out_gdal2tiles_test_nosrs.vrt",
    )
    os.chdir("..")

    os.unlink("tmp/out_gdal2tiles_test_nosrs.vrt")
    os.unlink("tmp/byte.tif")
    shutil.rmtree("tmp/out_gdal2tiles_test_nosrs")

    assert "ERROR ret code = 2" in ret
    assert "ERROR ret code" not in ret2


def test_does_not_error_when_source_bounds_close_to_tiles_bound():
    """
    Case where the border coordinate of the input file is inside a tile T but the first pixel is
    actually assigned to the tile next to T (nearest neighbour), meaning that when the query is done
    to get the content of T, nothing is returned from the raster.
    """
    in_files = [
        "./data/test_bounds_close_to_tile_bounds_x.vrt",
        "./data/test_bounds_close_to_tile_bounds_y.vrt",
    ]
    out_folder = "tmp/out_gdal2tiles_bounds_approx"
    try:
        shutil.rmtree(out_folder)
    except Exception:
        pass

    script_path = test_py_scripts.get_py_script("gdal2tiles")
    if script_path is None:
        pytest.skip()

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


def test_does_not_error_when_nothing_to_put_in_the_low_zoom_tile():
    """
    Case when the highest zoom level asked is actually too low for any pixel of the raster to be
    selected
    """
    in_file = "./data/test_bounds_close_to_tile_bounds_x.vrt"
    out_folder = "tmp/out_gdal2tiles_bounds_approx"
    try:
        shutil.rmtree(out_folder)
    except OSError:
        pass

    script_path = test_py_scripts.get_py_script("gdal2tiles")
    if script_path is None:
        pytest.skip()

    try:
        test_py_scripts.run_py_script(
            script_path, "gdal2tiles", "-q -z 10 %s %s" % (in_file, out_folder)
        )
    except TypeError:
        pytest.fail(
            "Case of low level tile not getting any data not handled properly "
            "(tile at a zoom level too low)"
        )


def test_handle_utf8_filename():
    input_file = "data/test_utf8_漢字.vrt"
    script_path = test_py_scripts.get_py_script("gdal2tiles")
    if script_path is None:
        pytest.skip()

    out_folder = "tmp/utf8_test"

    try:
        shutil.rmtree(out_folder)
    except OSError:
        pass

    args = f"-q -z 21 {input_file} {out_folder}"

    test_py_scripts.run_py_script(script_path, "gdal2tiles", args)

    openlayers_html = open(
        os.path.join(out_folder, "openlayers.html"), "rt", encoding="utf-8"
    ).read()
    assert "<title>test_utf8_漢字.vrt</title>" in openlayers_html

    try:
        shutil.rmtree(out_folder)
    except OSError:
        pass


def test_gdal2tiles_py_cleanup():

    lst = ["tmp/out_gdal2tiles_smallworld", "tmp/out_gdal2tiles_bounds_approx"]
    for filename in lst:
        try:
            shutil.rmtree(filename)
        except Exception:
            pass


def test_exclude_transparent_tiles():
    script_path = test_py_scripts.get_py_script("gdal2tiles")
    if script_path is None:
        pytest.skip()

    output_folder = "tmp/test_exclude_transparent_tiles"
    os.makedirs(output_folder)

    try:
        test_py_scripts.run_py_script_as_external_script(
            script_path,
            "gdal2tiles",
            "-x -z 14-16 data/test_gdal2tiles_exclude_transparent.tif %s"
            % output_folder,
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

    finally:
        shutil.rmtree(output_folder)


def test_gdal2tiles_py_profile_raster():

    script_path = test_py_scripts.get_py_script("gdal2tiles")
    if script_path is None:
        pytest.skip()

    shutil.rmtree("tmp/out_gdal2tiles_smallworld", ignore_errors=True)

    test_py_scripts.run_py_script_as_external_script(
        script_path,
        "gdal2tiles",
        "-q -p raster -z 0-1 "
        + test_py_scripts.get_data_path("gdrivers")
        + "small_world.tif tmp/out_gdal2tiles_smallworld",
    )

    if sys.platform != "win32":
        # For some reason, the checksums on the kml file on Windows are the ones of the below png
        _verify_raster_band_checksums(
            "tmp/out_gdal2tiles_smallworld/0/0/0.kml",
            expected_cs=[29839, 34244, 42706, 64319],
        )
    _verify_raster_band_checksums(
        "tmp/out_gdal2tiles_smallworld/0/0/0.png",
        expected_cs=[10125, 10802, 27343, 48852],
    )
    _verify_raster_band_checksums(
        "tmp/out_gdal2tiles_smallworld/1/0/0.png",
        expected_cs=[62125, 59756, 43894, 38539],
    )

    shutil.rmtree("tmp/out_gdal2tiles_smallworld", ignore_errors=True)


def test_gdal2tiles_py_profile_raster_oversample():

    script_path = test_py_scripts.get_py_script("gdal2tiles")
    if script_path is None:
        pytest.skip()

    shutil.rmtree("tmp/out_gdal2tiles_smallworld", ignore_errors=True)

    test_py_scripts.run_py_script_as_external_script(
        script_path,
        "gdal2tiles",
        "-q -p raster -z 0-2 "
        + test_py_scripts.get_data_path("gdrivers")
        + "small_world.tif tmp/out_gdal2tiles_smallworld",
    )

    assert os.path.exists("tmp/out_gdal2tiles_smallworld/2/0/0.png")
    assert os.path.exists("tmp/out_gdal2tiles_smallworld/2/3/1.png")
    _verify_raster_band_checksums(
        "tmp/out_gdal2tiles_smallworld/2/0/0.png",
        expected_cs=[[51434, 55441, 63427, 17849], [51193, 55320, 63324, 17849]],  # icc
    )
    _verify_raster_band_checksums(
        "tmp/out_gdal2tiles_smallworld/2/3/1.png",
        expected_cs=[[44685, 45074, 50871, 56563], [44643, 45116, 50863, 56563]],  # icc
    )
    shutil.rmtree("tmp/out_gdal2tiles_smallworld", ignore_errors=True)


def test_gdal2tiles_py_profile_raster_xyz():

    script_path = test_py_scripts.get_py_script("gdal2tiles")
    if script_path is None:
        pytest.skip()

    shutil.rmtree("tmp/out_gdal2tiles_smallworld", ignore_errors=True)

    test_py_scripts.run_py_script_as_external_script(
        script_path,
        "gdal2tiles",
        "-q -p raster --xyz -z 0-1 "
        + test_py_scripts.get_data_path("gdrivers")
        + "small_world.tif tmp/out_gdal2tiles_smallworld",
    )

    if sys.platform != "win32":
        # For some reason, the checksums on the kml file on Windows are the ones of the below png
        _verify_raster_band_checksums(
            "tmp/out_gdal2tiles_smallworld/0/0/0.kml",
            expected_cs=[27644, 31968, 38564, 64301],
        )
    _verify_raster_band_checksums(
        "tmp/out_gdal2tiles_smallworld/0/0/0.png",
        expected_cs=[11468, 10719, 27582, 48827],
    )
    _verify_raster_band_checksums(
        "tmp/out_gdal2tiles_smallworld/1/0/0.png",
        expected_cs=[60550, 62572, 46338, 38489],
    )

    shutil.rmtree("tmp/out_gdal2tiles_smallworld", ignore_errors=True)


def test_gdal2tiles_py_profile_geodetic_tmscompatible_xyz():

    script_path = test_py_scripts.get_py_script("gdal2tiles")
    if script_path is None:
        pytest.skip()

    shutil.rmtree("tmp/out_gdal2tiles_smallworld", ignore_errors=True)

    test_py_scripts.run_py_script_as_external_script(
        script_path,
        "gdal2tiles",
        "-q -p geodetic --tmscompatible --xyz -z 0-1 "
        + test_py_scripts.get_data_path("gdrivers")
        + "small_world.tif tmp/out_gdal2tiles_smallworld",
    )

    if sys.platform != "win32":
        # For some reason, the checksums on the kml file on Windows are the ones of the below png
        _verify_raster_band_checksums(
            "tmp/out_gdal2tiles_smallworld/0/0/0.kml",
            expected_cs=[12361, 18212, 21827, 5934],
        )
    _verify_raster_band_checksums(
        "tmp/out_gdal2tiles_smallworld/0/0/0.png", expected_cs=[8560, 8031, 7209, 17849]
    )
    _verify_raster_band_checksums(
        "tmp/out_gdal2tiles_smallworld/1/0/0.png", expected_cs=[2799, 3468, 8686, 17849]
    )

    shutil.rmtree("tmp/out_gdal2tiles_smallworld", ignore_errors=True)


def test_gdal2tiles_py_mapml():

    script_path = test_py_scripts.get_py_script("gdal2tiles")
    if script_path is None:
        pytest.skip()

    shutil.rmtree("tmp/out_gdal2tiles_mapml", ignore_errors=True)

    gdal.Translate(
        "tmp/byte_APS.tif",
        test_py_scripts.get_data_path("gcore") + "byte.tif",
        options="-a_srs EPSG:5936 -a_ullr 0 40 40 0",
    )

    test_py_scripts.run_py_script_as_external_script(
        script_path,
        "gdal2tiles",
        '-q -p APSTILE -w mapml -z 16-18 --url "https://foo" tmp/byte_APS.tif tmp/out_gdal2tiles_mapml',
    )

    mapml = open("tmp/out_gdal2tiles_mapml/mapml.mapml", "rb").read().decode("utf-8")
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

    shutil.rmtree("tmp/out_gdal2tiles_mapml", ignore_errors=True)
    gdal.Unlink("tmp/byte_APS.tif")


def _convert_png_to_webp(frm, to, quality):
    src_ds = gdal.Open(frm)
    driver = gdal.GetDriverByName("WEBP")
    driver.CreateCopy(to, src_ds, 0, options=["LOSSLESS=True"])


def _run_webp_test(script_path, resampling):

    shutil.rmtree("tmp/out_gdal2tiles_smallworld_png", ignore_errors=True)
    shutil.rmtree("tmp/out_gdal2tiles_smallworld_webp_from_png", ignore_errors=True)
    shutil.rmtree("tmp/out_gdal2tiles_smallworld_webp", ignore_errors=True)

    base_args = "-q --processes=2 -z 0-1 -r " + resampling + " "
    test_py_scripts.run_py_script_as_external_script(
        script_path,
        "gdal2tiles",
        base_args
        + test_py_scripts.get_data_path("gdrivers")
        + "small_world.tif tmp/out_gdal2tiles_smallworld_png",
    )

    quality = 50
    test_py_scripts.run_py_script_as_external_script(
        script_path,
        "gdal2tiles",
        base_args
        + "--tiledriver=WEBP --webp-lossless "
        + test_py_scripts.get_data_path("gdrivers")
        + "small_world.tif tmp/out_gdal2tiles_smallworld_webp",
    )

    to_convert = glob.glob("tmp/out_gdal2tiles_smallworld_png/*/*/*.png")
    for filename in to_convert:
        to_filename = filename.replace(
            "tmp/out_gdal2tiles_smallworld_png/",
            "tmp/out_gdal2tiles_smallworld_webp_from_png/",
        )
        to_filename = to_filename.replace(".png", ".webp")
        to_folder = os.path.dirname(to_filename)
        os.makedirs(to_folder, exist_ok=True)

        _convert_png_to_webp(filename, to_filename, quality)

    to_compare = glob.glob("tmp/out_gdal2tiles_smallworld_webp_from_png/*/*/*.webp")
    for filename in to_compare:
        webp_filename = filename.replace(
            "tmp/out_gdal2tiles_smallworld_webp_from_png/",
            "tmp/out_gdal2tiles_smallworld_webp/",
        )
        diff_found = compare_db(gdal.Open(webp_filename), gdal.Open(filename))
        assert not diff_found, (resampling, filename)

    shutil.rmtree("tmp/out_gdal2tiles_smallworld_png", ignore_errors=True)
    shutil.rmtree("tmp/out_gdal2tiles_smallworld_webp_from_png", ignore_errors=True)
    shutil.rmtree("tmp/out_gdal2tiles_smallworld_webp", ignore_errors=True)


def test_gdal2tiles_py_webp():

    script_path = test_py_scripts.get_py_script("gdal2tiles")
    if script_path is None:
        pytest.skip()

    if gdal.GetDriverByName("WEBP") is None:
        pytest.skip()

    _run_webp_test(script_path, "average")
    try:
        import numpy
        from PIL import Image

        import osgeo.gdal_array as gdalarray

        del Image, numpy, gdalarray
        pil_available = True
    except ImportError:
        pil_available = False

    if pil_available:
        _run_webp_test(script_path, "antialias")
