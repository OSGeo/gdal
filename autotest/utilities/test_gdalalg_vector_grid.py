#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector grid' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import sys

import ogrtest
import pytest

from osgeo import gdal, ogr


def get_alg(subalg):
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["grid"][subalg]


def test_gdalalg_vector_grid_error():
    alg = gdal.GetGlobalAlgorithmRegistry()["vector"]["grid"]
    with pytest.raises(Exception, match="method should not be called directly"):
        alg.Run()


def get_src_ds(geom3D):
    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("test")
    src_lyr.CreateField(ogr.FieldDefn("z", ogr.OFTReal))
    for x, y, z in [
        (100, 1000, 500),
        (110, 1000, 400),
        (100, 1010, 300),
        (110, 1010, 600),
        (109, 1009, 610),
    ]:
        f = ogr.Feature(src_lyr.GetLayerDefn())
        if geom3D:
            f.SetGeometry(ogr.CreateGeometryFromWkt(f"POINT Z({x} {y} {z})"))
        else:
            f["z"] = z
            f.SetGeometry(ogr.CreateGeometryFromWkt(f"POINT({x} {y})"))
        src_lyr.CreateFeature(f)
    return src_ds


@pytest.mark.parametrize(
    "subalg,geom3D,options,checksum,ret_value,msg",
    [
        #
        # General argument testing
        #
        ("invdist", False, {"zfield": "z"}, 51948, "success", None),
        (
            "invdist",
            False,
            {"layer": "test"},
            0,
            "warning",
            "At least one geometry of layer 'test' lacks a Z component. You may need to set the 'zfield' argument",
        ),
        ("invdist", True, {"layer": "test"}, 51948, "success", None),
        (
            "invdist",
            True,
            {"layer": "invalid"},
            None,
            "exception",
            'Unable to find layer "invalid"',
        ),
        ("invdist", True, {"sql": "SELECT * FROM test"}, 51948, "success", None),
        (
            "invdist",
            True,
            {"sql": "SELECT * FROM invalid"},
            None,
            "exception",
            "SELECT from table invalid failed",
        ),
        (
            "invdist",
            True,
            {"bbox": [100 - 0.1, 1000 - 0.1, 110 + 0.1, 1010 + 0.1]},
            51948,
            "success",
            None,
        ),
        (
            "invdist",
            True,
            {"bbox": [100 - 0.1, 1000 - 0.1, 109 + 0.1, 1009 + 0.1]},
            65227,
            "success",
            None,
        ),
        ("invdist", True, {"zoffset": 10}, 54219, "success", None),
        ("invdist", True, {"zmultiply": 1.01}, 47965, "success", None),
        #
        # invdist testing
        #
        ("invdist", True, {}, 51948, "success", None),
        (
            "invdist",
            False,
            {},
            0,
            "warning",
            "At least one geometry of layer 'test' lacks a Z component. You may need to set the 'zfield' argument",
        ),
        ("invdist", True, {"power": 1.5}, 54471, "success", None),
        ("invdist", True, {"smoothing": 1.5}, 55409, "success", None),
        ("invdist", True, {"radius": 10}, 52638, "success", None),
        (
            "invdist",
            True,
            {"radius": 10, "radius1": 5, "radius2": 5},
            None,
            "exception",
            "Argument 'radius1' is mutually exclusive with 'radius'",
        ),
        ("invdist", True, {"radius": 5}, 32388, "success", None),
        (
            "invdist",
            True,
            {"radius1": 10},
            None,
            "exception",
            "'radius2' should be defined when 'radius1' is",
        ),
        (
            "invdist",
            True,
            {"radius2": 10},
            None,
            "exception",
            "'radius1' should be defined when 'radius2' is",
        ),
        ("invdist", True, {"radius1": 10, "radius2": 20}, 52085, "success", None),
        (
            "invdist",
            True,
            {"radius1": 10, "radius2": 20, "angle": 90},
            51987,
            "success",
            None,
        ),
        ("invdist", True, {"radius1": 10, "radius2": 10}, 52638, "success", None),
        ("invdist", True, {"radius": 5, "nodata": -1}, 23105, "success", None),
        ("invdist", True, {"min-points": 4, "radius": 10}, 4070, "success", None),
        (
            "invdist",
            True,
            {"min-points": 1},
            None,
            "exception",
            "'radius' or 'radius1' and 'radius2' should be defined when 'min-points' is",
        ),
        ("invdist", True, {"max-points": 2, "radius": 10}, 54334, "success", None),
        (
            "invdist",
            True,
            {"max-points": 1},
            None,
            "exception",
            "'radius' or 'radius1' and 'radius2' should be defined when 'max-points' is",
        ),
        (
            "invdist",
            True,
            {"min-points-per-quadrant": 1, "radius": 10},
            22047,
            "success",
            None,
        ),
        (
            "invdist",
            True,
            {"max-points-per-quadrant": 1, "radius": 10},
            54602,
            "success",
            None,
        ),
        (
            "invdist",
            True,
            {"resolution": [1, 2]},
            None,
            "exception",
            "'resolution' should be defined when 'extent' is",
        ),
        #
        # invdistnn testing
        #
        ("invdistnn", True, {"radius": 20}, 51945, "success", None),
        ("invdistnn", True, {"radius": 10}, 52638, "success", None),
        ("invdistnn", True, {"radius": 20, "power": 1.5}, 54471, "success", None),
        ("invdistnn", True, {"radius": 20, "smoothing": 1.5}, 55409, "success", None),
        ("invdistnn", True, {"radius": 20, "max-points": 1}, 58195, "success", None),
        ("invdistnn", True, {"radius": 20, "min-points": 6}, 0, "success", None),
        (
            "invdistnn",
            True,
            {"radius": 20, "min-points-per-quadrant": 2},
            0,
            "success",
            None,
        ),
        (
            "invdistnn",
            True,
            {"radius": 20, "max-points-per-quadrant": 1},
            52361,
            "success",
            None,
        ),
        ("invdistnn", True, {"radius": 20, "nodata": -1}, 51945, "success", None),
    ],
)
def test_gdalalg_vector_grid_regular(subalg, geom3D, options, checksum, ret_value, msg):

    if "bbox" in options and not ogrtest.have_geos():
        pytest.skip("GEOS not available")

    alg = get_alg(subalg)
    alg["input"] = get_src_ds(geom3D)
    alg["output"] = ""
    alg["output-format"] = "MEM"
    for k in options:
        alg[k] = options[k]
    gdal.ErrorReset()
    if ret_value == "success":
        assert alg.Run()
        assert gdal.GetLastErrorMsg() == ""
    elif ret_value == "warning":
        with gdal.quiet_errors():
            assert alg.Run()
            assert msg in gdal.GetLastErrorMsg()
    elif ret_value == "exception":
        with pytest.raises(Exception, match=msg):
            alg.Run()
        return
    else:
        assert False
    ds = alg["output"].GetDataset()
    assert ds.GetRasterBand(1).Checksum() == pytest.approx(checksum, abs=1)
    if "nodata" in options:
        assert ds.GetRasterBand(1).GetNoDataValue() == options["nodata"]


def test_gdalalg_vector_grid_progress(tmp_vsimem):

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    alg = get_alg("invdist")
    alg["input"] = get_src_ds(True)
    alg["output"] = tmp_vsimem / "out.tif"
    assert alg.ParseRunAndFinalize(
        [],
        my_progress,
    )
    assert last_pct[0] == 1.0


def test_gdalalg_vector_grid_failed_update_existing_file(tmp_vsimem):

    out_ds = gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "out.tif", 1, 1)

    alg = get_alg("invdist")
    alg["input"] = get_src_ds(True)
    alg["output"] = out_ds
    with pytest.raises(
        Exception,
        match="gdal vector grid does not support outputting to an already opened output dataset",
    ):
        alg.Run()


def test_gdalalg_vector_grid_creation_option(tmp_vsimem):

    alg = get_alg("invdist")
    alg["input"] = get_src_ds(True)
    alg["output"] = tmp_vsimem / "out.tif"
    alg["creation-option"] = {"COMPRESS": "LZW"}
    assert alg.Run() and alg.Finalize()
    with gdal.Open(alg["output"].GetName()) as ds:
        assert ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "LZW"


def test_gdalalg_vector_grid_extent():

    alg = get_alg("invdist")
    alg["input"] = get_src_ds(True)
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["extent"] = [90, 990, 120, 1110]
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetGeoTransform() == (90.0, 0.1171875, 0.0, 1110.0, 0.0, -0.46875)


def test_gdalalg_vector_grid_size():

    alg = get_alg("invdist")
    alg["input"] = get_src_ds(True)
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["size"] = [10, 20]
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.RasterXSize == 10
    assert ds.RasterYSize == 20


def test_gdalalg_vector_grid_resolution():

    alg = get_alg("invdist")
    alg["input"] = get_src_ds(True)
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["resolution"] = [0.1, 0.2]
    alg["extent"] = [90, 990, 120, 1110]
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetGeoTransform() == (90.0, 0.1, 0.0, 1110.0, 0.0, -0.2)


def test_gdalalg_vector_grid_output_type():

    alg = get_alg("invdist")
    alg["input"] = get_src_ds(True)
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-data-type"] = "Float32"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Float32


def test_gdalalg_vector_grid_crs():

    alg = get_alg("invdist")
    alg["input"] = get_src_ds(True)
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["crs"] = "EPSG:4326"
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "4326"


def test_gdalalg_vector_grid_overwrite(tmp_vsimem):

    out_filename = tmp_vsimem / "out.tif"
    with gdal.GetDriverByName("GTiff").Create(out_filename, 1, 1) as ds:
        pass

    alg = get_alg("invdist")
    alg["input"] = get_src_ds(True)
    alg["output"] = out_filename
    with pytest.raises(
        Exception,
        match="already exists. Specify the --overwrite option to overwrite it",
    ):
        alg.Run()

    alg = get_alg("invdist")
    alg["input"] = get_src_ds(True)
    alg["output"] = out_filename
    alg["overwrite"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds.GetRasterBand(1).XSize == 256


@pytest.mark.skipif(sys.platform != "linux", reason="Incorrect platform")
def test_gdalalg_vector_grid_overwrite_failed_unlink(tmp_path):

    out_filename = tmp_path / "out.tif"
    with gdal.GetDriverByName("GTiff").Create(out_filename, 1, 1):
        pass

    os.chmod(tmp_path, 0o555)
    try:
        alg = get_alg("invdist")
        alg["input"] = get_src_ds(True)
        alg["output"] = out_filename
        alg["overwrite"] = True
        with pytest.raises(Exception, match="Failed to delete"):
            alg.Run()
    except Exception:
        os.chmod(tmp_path, 0o755)
        raise
    finally:
        os.chmod(tmp_path, 0o755)
