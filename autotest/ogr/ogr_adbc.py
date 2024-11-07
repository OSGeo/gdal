#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR ADBC driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal, ogr


def _has_adbc_driver_manager():
    drv = gdal.GetDriverByName("ADBC")
    return drv and drv.GetMetadataItem("HAS_ADBC_DRIVER_MANAGER")


pytestmark = [
    pytest.mark.require_driver("ADBC"),
    pytest.mark.skipif(
        not _has_adbc_driver_manager(),
        reason="ADBC driver built without AdbcDriverManager",
    ),
]

###############################################################################


def _has_sqlite_driver():
    import ctypes

    try:
        return ctypes.cdll.LoadLibrary("libadbc_driver_sqlite.so") is not None
    except Exception:
        return False


###############################################################################


def test_ogr_adbc_driver_open_option():

    if not _has_sqlite_driver():
        pytest.skip("adbc_driver_sqlite missing")

    with gdal.OpenEx(
        "ADBC:", gdal.OF_VECTOR, open_options=["ADBC_DRIVER=adbc_driver_sqlite"]
    ) as ds:
        assert ds.GetLayerCount() == 0
        with ds.ExecuteSQL("SELECT sqlite_version()") as sql_lyr:
            f = sql_lyr.GetNextFeature()
            assert f
            assert f.GetField(0).startswith("3.")


###############################################################################


def test_ogr_adbc_invalid_driver():

    with pytest.raises(Exception):
        gdal.OpenEx(
            "ADBC:", gdal.OF_VECTOR, open_options=["ADBC_DRIVER=invalid_driver"]
        )


###############################################################################


def test_ogr_adbc_invalid_dataset():

    if not _has_sqlite_driver():
        pytest.skip("adbc_driver_sqlite missing")

    with pytest.raises(Exception):
        gdal.OpenEx(
            "ADBC:/i/do/not/exist.db",
            gdal.OF_VECTOR,
            open_options=["ADBC_DRIVER=adbc_driver_sqlite"],
        )


###############################################################################


def test_ogr_adbc_sqlite3():

    if not _has_sqlite_driver():
        pytest.skip("adbc_driver_sqlite missing")

    with gdal.OpenEx(
        "data/sqlite/poly_spatialite.sqlite", gdal.OF_VECTOR, allowed_drivers=["ADBC"]
    ) as ds:
        assert ds.GetLayerCount() == 13
        assert ds.GetLayer(-1) is None
        assert ds.GetLayer(ds.GetLayerCount()) is None
        lyr = ds.GetLayer(0)
        assert lyr.TestCapability(ogr.OLCFastGetArrowStream)


###############################################################################


def test_ogr_adbc_sql_open_option():

    if not _has_sqlite_driver():
        pytest.skip("adbc_driver_sqlite missing")

    with gdal.OpenEx(
        "ADBC:data/sqlite/poly_spatialite.sqlite",
        gdal.OF_VECTOR,
        open_options=["SQL=SELECT * FROM poly"],
    ) as ds:
        assert ds.GetLayerCount() == 1
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 10


###############################################################################


def test_ogr_adbc_invalid_sql():

    if not _has_sqlite_driver():
        pytest.skip("adbc_driver_sqlite missing")

    with pytest.raises(Exception):
        gdal.OpenEx(
            "ADBC:data/sqlite/poly_spatialite.sqlite",
            gdal.OF_VECTOR,
            open_options=["SQL=SELECT * FROM"],
        )


###############################################################################


def test_ogr_adbc_generic_open_option():

    if not _has_sqlite_driver():
        pytest.skip("adbc_driver_sqlite missing")

    with gdal.OpenEx(
        "ADBC:",
        gdal.OF_VECTOR,
        open_options=[
            "ADBC_DRIVER=adbc_driver_sqlite",
            "ADBC_OPTION_uri=data/sqlite/poly_spatialite.sqlite",
        ],
    ) as ds:
        assert ds.GetLayerCount() == 13


###############################################################################


def test_ogr_adbc_execute_sql():

    if not _has_sqlite_driver():
        pytest.skip("adbc_driver_sqlite missing")

    with gdal.OpenEx(
        "data/sqlite/poly_spatialite.sqlite",
        gdal.OF_VECTOR,
        open_options=["SQL="],
        allowed_drivers=["ADBC"],
    ) as ds:
        assert ds.GetLayerCount() == 0
        with ds.ExecuteSQL("SELECT * FROM poly") as lyr:
            assert lyr.GetFeatureCount() == 10


###############################################################################


def _has_libduckdb():
    import ctypes

    try:
        return ctypes.cdll.LoadLibrary("libduckdb.so") is not None
    except Exception:
        return False


###############################################################################


def test_ogr_adbc_duckdb_parquet():

    if not _has_libduckdb():
        pytest.skip("libduckdb.so missing")

    with gdal.OpenEx(
        "data/parquet/partitioned_flat/part.0.parquet",
        gdal.OF_VECTOR,
        allowed_drivers=["ADBC"],
    ) as ds:
        assert ds.GetLayerCount() == 1
        lyr = ds.GetLayer(0)
        assert lyr.TestCapability(ogr.OLCFastFeatureCount)
        assert lyr.GetFeatureCount() == 3


###############################################################################


def test_ogr_adbc_duckdb_parquet_with_sql_open_option():

    if not _has_libduckdb():
        pytest.skip("libduckdb.so missing")

    with gdal.OpenEx(
        "data/parquet/partitioned_flat/part.0.parquet",
        gdal.OF_VECTOR,
        allowed_drivers=["ADBC"],
        open_options=["SQL=SELECT * FROM part.0 ORDER BY one DESC LIMIT 2"],
    ) as ds:
        assert ds.GetLayerCount() == 1
        lyr = ds.GetLayer(0)
        assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 0
        assert lyr.GetFeatureCount() == 2


###############################################################################


def test_ogr_adbc_duckdb_parquet_with_spatial():

    if not _has_libduckdb():
        pytest.skip("libduckdb.so missing")

    if gdaltest.is_travis_branch("ubuntu_2404"):
        # Works locally for me when replicating the Dockerfile ...
        pytest.skip("fails on ubuntu_2404 for unknown reason")

    with gdal.OpenEx(
        "data/parquet/poly.parquet",
        gdal.OF_VECTOR,
        allowed_drivers=["ADBC"],
        open_options=[
            "PRELUDE_STATEMENTS=INSTALL spatial",
            "PRELUDE_STATEMENTS=LOAD spatial",
        ],
    ) as ds:
        with ds.ExecuteSQL(
            "SELECT ST_AsText(geometry) FROM read_parquet('data/parquet/poly.parquet')"
        ) as sql_lyr:
            f = sql_lyr.GetNextFeature()
            assert f.GetField(0).startswith("POLYGON")


###############################################################################
# Run test_ogrsf on a SQLite3 database


def test_ogr_adbc_test_ogrsf_sqlite3():

    if not _has_sqlite_driver():
        pytest.skip("adbc_driver_sqlite missing")

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + " -ro ADBC:data/sqlite/first_geometry_null.db"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################
# Run test_ogrsf on a single Parquet file


def test_ogr_adbc_test_ogrsf_parquet():

    if not _has_libduckdb():
        pytest.skip("libduckdb.so missing")

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + " -ro ADBC:data/parquet/partitioned_flat/part.0.parquet"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################
# Run test_ogrsf on a partitioned Parquet dataset


def test_ogr_adbc_test_ogrsf_parquet_filename_with_glob():

    if not _has_libduckdb():
        pytest.skip("libduckdb.so missing")

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + " -ro ADBC:data/parquet/partitioned_flat/*.parquet"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################
# Run test_ogrsf on a DuckDB dataset


def test_ogr_adbc_test_ogrsf_duckdb():

    if not _has_libduckdb():
        pytest.skip("libduckdb.so missing")

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + " -ro ADBC:data/duckdb/poly.duckdb"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################


def test_ogr_adbc_layer_list():

    if not _has_sqlite_driver():
        pytest.skip("adbc_driver_sqlite missing")

    with gdal.OpenEx(
        "data/sqlite/poly_spatialite.sqlite", gdal.OF_VECTOR, allowed_drivers=["ADBC"]
    ) as ds:
        assert ds.GetLayerCount() == 13
        assert ds.GetLayerByName("table_list")
        assert ds.GetLayerCount() == 14
        # Re-issue GetLayerByName() to check it has been added to the list
        # of known layers and is no instantiated twice.
        lyr = ds.GetLayerByName("table_list")
        assert lyr
        assert ds.GetLayerCount() == 14
        assert lyr.GetFeatureCount() == 14
        found = False
        for f in lyr:
            if (
                f["catalog_name"] == "main"
                and f["table_name"] == "spatial_ref_sys"
                and f["table_type"] == "table"
            ):
                found = True
        assert found
