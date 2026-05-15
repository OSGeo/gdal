#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal mdim get-refs' testing
# Author:   Michael Sumner <mdsumner at gmail.com>
#
###############################################################################
# Copyright (c) 2026, Michael Sumner <mdsumner at gmail.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest
import test_cli_utilities

from osgeo import gdal, ogr

pytestmark = [
    pytest.mark.require_driver("VRT"),
    pytest.mark.require_driver("GPKG"),
    pytest.mark.skipif(
        test_cli_utilities.get_gdal_path() is None,
        reason="gdal binary not available",
    ),
]


@pytest.fixture()
def gdal_path():
    return test_cli_utilities.get_gdal_path()


def get_mdim_get_refs_alg():
    return gdal.GetGlobalAlgorithmRegistry()["mdim"]["get-refs"]


def get_mdim_convert_alg():
    return gdal.GetGlobalAlgorithmRegistry()["mdim"]["convert"]


###############################################################################
# Algorithm-level tests, arguments, errors, framework behaviour.
###############################################################################

# Helper to extract type+subtype for a named field
def field_info(name, defn):
    i = defn.GetFieldIndex(name)
    assert i >= 0, f"field {name!r} missing from layer"
    fd = defn.GetFieldDefn(i)
    return fd.GetType(), fd.GetSubType()


def test_gdalalg_mdim_get_refs_basic(tmp_vsimem):
    """Round-trip a small mdim input through get-refs; verify layer + schema."""
    tmpfile = tmp_vsimem / "out.gpkg"
    alg = get_mdim_get_refs_alg()
    alg["array"] = "/var"
    alg["output-format"] = "GPKG"
    assert alg.ParseRunAndFinalize(["data/mdim_zarr.vrt", str(tmpfile)])
    ds = gdal.OpenEx(str(tmpfile), gdal.OF_VECTOR)
    assert ds is not None
    layer = ds.GetLayer(0)
    assert layer is not None
    defn = layer.GetLayerDefn()

    # dim_0, dim_1 (assuming a 2D array in the VRT) — Integer64, no subtype
    assert field_info("dim_0", defn) == (ogr.OFTInteger64, ogr.OFSTNone)
    assert field_info("dim_1", defn) == (ogr.OFTInteger64, ogr.OFSTNone)

    # present — Integer with Boolean subtype
    assert field_info("present", defn) == (ogr.OFTInteger, ogr.OFSTBoolean)

    # path, info — plain String, no subtype
    assert field_info("path", defn) == (ogr.OFTString, ogr.OFSTNone)
    assert field_info("info", defn) == (ogr.OFTString, ogr.OFSTNone)

    # offset, size — Integer64, no subtype
    assert field_info("offset", defn) == (ogr.OFTInteger64, ogr.OFSTNone)
    assert field_info("size", defn) == (ogr.OFTInteger64, ogr.OFSTNone)


def test_gdalalg_mdim_get_refs_array_required():
    """--array is required at parse time; absence is rejected by the framework."""
    alg = get_mdim_get_refs_alg()
    with pytest.raises(Exception, match="array"):
        alg.ParseRunAndFinalize(["data/mdim_zarr.vrt", "/tmp/dummy.gpkg"])


def test_gdalalg_mdim_get_refs_missing_array_path(tmp_vsimem):
    """An array name that doesn't exist must fail clearly with the suggestion."""
    tmpfile = tmp_vsimem / "out.gpkg"
    alg = get_mdim_get_refs_alg()
    alg["array"] = "/this_array_does_not_exist"
    alg["output-format"] = "GPKG"
    with pytest.raises(Exception, match="Cannot find array"):
        alg.ParseRunAndFinalize(["data/mdim_zarr.vrt", str(tmpfile)])


def test_gdalalg_mdim_get_refs_unknown_output_format(tmp_vsimem):
    """Unknown output driver must produce the helpful error wording."""
    alg = get_mdim_get_refs_alg()
    alg["array"] = "/var"
    with pytest.raises(RuntimeError, match="does not exist"):
        alg["output-format"] = "NOT_A_DRIVER"


def test_gdalalg_mdim_get_refs_non_vector_output_format(tmp_vsimem):
    """A raster driver passed as --of must fail (capability filter rejects)."""
    alg = get_mdim_get_refs_alg()
    alg["array"] = "/var"
    with pytest.raises(RuntimeError, match="does not expose the required"):
        alg["output-format"] = "GTiff"


def test_gdalalg_mdim_get_refs_overwrite(tmp_vsimem):
    """--overwrite permits re-running over an existing output file."""
    tmpfile = tmp_vsimem / "out.gpkg"

    alg = get_mdim_get_refs_alg()
    alg["array"] = "/var"
    alg["output-format"] = "GPKG"
    assert alg.ParseRunAndFinalize(["data/mdim_zarr.vrt", str(tmpfile)])

    # Second run without --overwrite should fail with the framework's wording.
    alg2 = get_mdim_get_refs_alg()
    alg2["array"] = "/var"
    alg2["output-format"] = "GPKG"
    with pytest.raises(Exception, match="already exists"):
        alg2.ParseRunAndFinalize(["data/mdim_zarr.vrt", str(tmpfile)])

    # Third run with --overwrite should succeed.
    alg3 = get_mdim_get_refs_alg()
    alg3["array"] = "/var"
    alg3["output-format"] = "GPKG"
    alg3["overwrite"] = True
    assert alg3.ParseRunAndFinalize(["data/mdim_zarr.vrt", str(tmpfile)])


def test_gdalalg_mdim_get_refs_no_chunked_storage(tmp_vsimem):
    """Array without natural block size declines cleanly (Stage B3 guard)."""
    # The mdim.vrt fixture should contain (or be augmented to contain) a
    # coordinate array or contiguous-storage variable that GetBlockSize
    # returns 0 for. Adjust the array path once the fixture is finalised.
    tmpfile = tmp_vsimem / "out.gpkg"
    alg = get_mdim_get_refs_alg()
    alg["array"] = "/my_variable_with_time_decreasing"
    alg["output-format"] = "GPKG"
    with pytest.raises(Exception, match="not chunk-enumerable"):
        alg.ParseRunAndFinalize(["data/mdim.vrt", str(tmpfile)])


@pytest.mark.require_driver("HDF5")
def test_gdalalg_mdim_get_refs_hdf5_partial_chunk(tmp_vsimem):
    """HDF5 with non-even chunking: trailing partial chunk must appear.

    Uses the autotest HDFEOS fixture (dummy_HDFEOS_swath_chunked.h5),
    which has dimensions [20, 30, 40] and chunks [3, 4, 6] -- all dimensions
    non-even, so the chunk grid is [7, 8, 7] = 392 chunks total.

    The trailing all-partial corner chunk (6, 7, 6) is the Phase 0 Q1
    stressor: its byte size of 64 (versus ~143 for full chunks) confirms
    ceil-division semantics. Both the offset (121797) and size (64) here
    are values predicted by the Phase 0 evidence-log oracle and verified
    against the running algorithm in earlier validation.
    """
    tmpfile = tmp_vsimem / "out.gpkg"
    alg = get_mdim_get_refs_alg()
    alg["array"] = "/HDFEOS/SWATHS/MySwath/Data Fields/MyDataField"
    alg["output-format"] = "GPKG"
    assert alg.ParseRunAndFinalize(
        ["../gdrivers/data/hdf5/dummy_HDFEOS_swath_chunked.h5", str(tmpfile)]
    )

    ds = gdal.OpenEx(str(tmpfile), gdal.OF_VECTOR)
    layer = ds.GetLayer(0)
    assert layer.GetFeatureCount() == 392  # 7 * 8 * 7 from ceil division

    # row (6, 7, 6) is a present, all-partial corner chunk with the specific offset/size.
    layer.SetAttributeFilter("dim_0 = 6 AND dim_1 = 7 AND dim_2 = 6")
    f = layer.GetNextFeature()
    assert f is not None
    assert f.GetField("present") == 1
    assert f.GetField("offset") == 121797
    assert f.GetField("size") == 64


###############################################################################
# Zarr driver tests: exercise present (one-file-per-chunk) and absent (sparse)
# branches of the three-state classification. Both require a small synthetic
# fixture built with zarr-python; deferred to a later test pass if zarr-python
# is not available in the test environment.
###############################################################################


@pytest.mark.require_driver("ZARR")
def test_gdalalg_mdim_get_refs_zarr_present(tmp_vsimem):
    """Native Zarr produces present rows with offset=0 (one file per chunk).

    Uses autotest fixture gdrivers/data/zarr/order_f_u2.zarr: 2D array [4, 4]
    with chunks [2, 3], ceil grid [2, 2], all four chunks materialised. The
    non-even chunking on dim_1 (4 / 3 = 2 with remainder) also exercises
    ceil-division through the Zarr driver, complementing the HDF5 partial-chunk
    test in case the per-driver implementations diverge.

    Verifies: every chunk reports present=1, offset=0 (Zarr one-file-per-chunk
    pattern), with path ending in the Zarr chunk-key form (e.g. ".../0.0").
    """
    tmpfile = tmp_vsimem / "out.gpkg"
    alg = get_mdim_get_refs_alg()
    alg["array"] = "/order_f_u2"
    alg["output-format"] = "GPKG"
    assert alg.ParseRunAndFinalize(
        ["../gdrivers/data/zarr/order_f_u2.zarr", str(tmpfile)]
    )

    ds = gdal.OpenEx(str(tmpfile), gdal.OF_VECTOR)
    layer = ds.GetLayer(0)
    assert layer.GetFeatureCount() == 4  # 2 * 2 from ceil([4/2, 4/3])

    # All chunks must be present, all offsets zero (Zarr's one-file-per-chunk
    # pattern), all paths must reference the chunk-key form ".../d0.d1".
    layer.SetAttributeFilter("present = 1")
    assert layer.GetFeatureCount() == 4
    layer.SetAttributeFilter("offset = 0")
    assert layer.GetFeatureCount() == 4
    layer.SetAttributeFilter(None)  # clear filter for enumeration check below

    # Enumeration cross-check: every (dim_0, dim_1) in the chunk grid must
    # appear exactly once. This implicitly tests the row-major enumeration
    # order of LinearToCoords (no chunks dropped, no chunks duplicated).
    expected_coords = [(0, 0), (0, 1), (1, 0), (1, 1)]
    for d0, d1 in expected_coords:
        layer.SetAttributeFilter(f"dim_0 = {d0} AND dim_1 = {d1}")
        assert (
            layer.GetFeatureCount() == 1
        ), f"chunk coordinate ({d0}, {d1}) missing from output"

    # Path-form check: pick one chunk and confirm the path ends with the
    # expected Zarr chunk-key. The full path includes the fixture's location;
    # we only assert on the suffix.
    layer.SetAttributeFilter("dim_0 = 1 AND dim_1 = 1")
    f = layer.GetNextFeature()
    assert f is not None
    assert f.GetField("path").endswith(
        "/1.1"
    ), f"unexpected path form: {f.GetField('path')}"


@pytest.mark.require_driver("ZARR")
def test_gdalalg_mdim_get_refs_zarr_sparse(tmp_path, tmp_vsimem):
    """Zarr with a missing chunk produces an absent row (NULL path/offset/size).

    Copies the order_f_u2.zarr fixture into tmp_path, deletes chunk file 0.1
    to create a sparse case (Zarr semantics: missing chunk = fill-value
    everywhere), and verifies that get-refs emits a present=0 row with NULL
    path/offset/size for that coordinate while the other three chunks remain
    present.

    This exercises the absent branch of the three-state classification; the
    present branch is covered by test_gdalalg_mdim_get_refs_zarr_present.
    """
    import os

    alg = get_mdim_convert_alg()
    dst = str(tmp_path / "order_f_u2_sparse.zarr")
    assert alg.ParseRunAndFinalize(["../gdrivers/data/zarr/order_f_u2.zarr", dst])

    # Remove one chunk file. Picking 0.1 (a non-corner chunk) so the test
    # exercises absence cleanly without conflating with the trailing-partial
    # case at coordinate (1, 1).
    os.remove(os.path.join(dst, "order_f_u2", "0.1"))

    tmpfile = tmp_vsimem / "out.gpkg"
    alg = get_mdim_get_refs_alg()
    alg["array"] = "/order_f_u2"
    alg["output-format"] = "GPKG"
    assert alg.ParseRunAndFinalize([dst, str(tmpfile)])

    ds = gdal.OpenEx(str(tmpfile), gdal.OF_VECTOR)
    layer = ds.GetLayer(0)
    assert layer.GetFeatureCount() == 4  # the chunk grid is unchanged

    # The deleted chunk's row must be absent with NULL byte-reference fields.
    layer.SetAttributeFilter("dim_0 = 0 AND dim_1 = 1")
    f = layer.GetNextFeature()
    assert f is not None
    assert f.GetField("present") == 0
    assert f.IsFieldNull("path")
    assert f.IsFieldNull("offset")
    assert f.IsFieldNull("size")

    # The other three chunks must still be present.
    layer.SetAttributeFilter("present = 1")
    assert layer.GetFeatureCount() == 3

    # And specifically, only chunk (0, 1) should be absent.
    layer.SetAttributeFilter("present = 0")
    assert layer.GetFeatureCount() == 1


###############################################################################
# Parquet-specific tests
###############################################################################


@pytest.mark.require_driver("Parquet")
def test_gdalalg_mdim_get_refs_parquet_boolean_subtype(tmp_vsimem):
    """OFSTBoolean subtype survives Parquet round-trip."""
    tmpfile = tmp_vsimem / "out.parquet"
    alg = get_mdim_get_refs_alg()
    alg["array"] = "/var"
    alg["output-format"] = "Parquet"
    assert alg.ParseRunAndFinalize(["data/mdim_zarr.vrt", str(tmpfile)])

    ds = gdal.OpenEx(str(tmpfile), gdal.OF_VECTOR)
    layer = ds.GetLayer(0)
    defn = layer.GetLayerDefn()
    idx = defn.GetFieldIndex("present")
    assert idx >= 0
    assert defn.GetFieldDefn(idx).GetSubType() == ogr.OFSTBoolean
