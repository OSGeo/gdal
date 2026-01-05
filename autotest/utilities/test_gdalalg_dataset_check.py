#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal dataset check' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal


@pytest.mark.parametrize(
    "driver, filename",
    [
        (
            "GTiff",
            "../gcore/data/gtiff/rgbsmall_NONE_tiled_separate.tif",
        ),  # INTERLEAVE=BAND
        ("GTiff", "../gcore/data/gtiff/rgbsmall_NONE_tiled.tif"),  # INTERLEAVE=PIXEL
        ("GTiff", "../gcore/data/twoimages.tif"),  # subdatasets
        ("Zarr", "../gdrivers/data/zarr/v3/test.zr3"),  # multidim API
        (
            "GPKG",
            "../ogr/data/gpkg/poly_golden.gpkg",
        ),  # vector, compatible with fast Arrow
        (
            "MapInfo File",
            "../ogr/data/mitab/small.mif",
        ),  # vector, not compatible with fast Arrow
        (
            "OSM",
            "../ogr/data/osm/test.pbf",
        ),
        # Raster and vector
        (
            "GPKG",
            "../gdrivers/data/gpkg/raster_and_vector.gpkg",
        ),
    ],
)
def test_gdalalg_dataset_check(tmp_vsimem, driver, filename):

    if gdal.GetDriverByName(driver) is None:
        pytest.skip(f"Skipped because driver {driver} not available")
    assert gdal.VSIStatL(filename) is not None

    with gdal.alg.dataset.check(input=filename) as alg:
        assert alg.Output() == 0

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    with gdal.alg.dataset.check(input=filename, progress=my_progress) as alg:
        assert alg.Output() == 0
        assert tab_pct[0] == 1.0

    if driver not in ("MapInfo File", "OSM"):
        tab_pct = [0]

        def my_progress(pct, msg, user_data):
            assert pct >= tab_pct[0]
            tab_pct[0] = pct
            return pct < 0.5

        with pytest.raises(Exception, match="Interrupted by user"):
            gdal.alg.dataset.check(input=filename, progress=my_progress)

    if driver == "Zarr":
        tmp_file = tmp_vsimem / "tmp.zarr"
        gdal.alg.vsi.copy(
            source=filename + "/", destination=str(tmp_file) + "/", recursive=True
        )
        assert gdal.VSIStatL(tmp_file / "marvin/android/0.0")
        with gdal.VSIFile(tmp_file / "marvin/android/0.0", "wb") as f:
            f.write(b"")
    else:
        tmp_file = str(tmp_vsimem) + "/tmp." + filename[filename.rfind(".") + 1 :]
        with gdal.VSIFile(tmp_file, "wb") as f:
            length = gdal.VSIStatL(filename).size
            if driver == "GPKG":
                data = bytearray(open(filename, "rb").read(length))
                # Beginning of a GeoPackage geometry blob
                if "raster_and_vector.gpkg" in filename:
                    data[0x19032] = 0
                else:
                    data[0x13D5E] = 0
                f.write(data)
            else:
                length = length - 100
                f.write(open(filename, "rb").read(length))

    assert gdal.OpenEx(tmp_file)

    with gdal.quiet_errors(), gdaltest.disable_exceptions():
        with pytest.raises(Exception, match=r"Algorithm.Run\(\) failed"):
            gdal.alg.dataset.check(input=tmp_file)


@pytest.mark.require_driver("Zarr")
def test_gdalalg_dataset_check_mdim_string():

    assert (
        gdal.alg.dataset.check(input="../gdrivers/data/zarr/unicode_le.zarr").Output()
        == 0
    )
