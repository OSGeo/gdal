#!/usr/bin/env pytest
###############################################################################
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

import shutil

import gdaltest
import pytest

from osgeo import gdal, ogr


def _has_adbc_driver_manager():
    drv = gdal.GetDriverByName("ADBC")
    return drv and drv.GetMetadataItem("HAS_ADBC_DRIVER_MANAGER")


pytestmark = pytest.mark.require_driver("ADBC")

###############################################################################


def _has_sqlite_driver():
    import ctypes

    try:
        return ctypes.cdll.LoadLibrary("libadbc_driver_sqlite.so") is not None
    except Exception:
        return False


###############################################################################


def _has_duckdb_driver():
    import ctypes

    for libname in ["libduckdb.so", "libduckdb.dylib", "duckdb.dll"]:
        try:
            if ctypes.cdll.LoadLibrary(libname):
                return True
        except Exception:
            pass
    return False


###############################################################################


@pytest.mark.skipif(
    not _has_adbc_driver_manager(),
    reason="ADBC driver built without AdbcDriverManager",
)
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


@pytest.mark.skipif(
    not _has_sqlite_driver(),
    reason="adbc_driver_sqlite missing",
)
def test_ogr_adbc_invalid_dataset():

    with pytest.raises(Exception):
        gdal.OpenEx(
            "ADBC:/i/do/not/exist.db",
            gdal.OF_VECTOR,
            open_options=["ADBC_DRIVER=adbc_driver_sqlite"],
        )


###############################################################################


@pytest.mark.skipif(
    not _has_adbc_driver_manager(),
    reason="ADBC driver built without AdbcDriverManager",
)
@pytest.mark.skipif(
    not _has_sqlite_driver(),
    reason="adbc_driver_sqlite missing",
)
def test_ogr_adbc_sqlite3():

    with gdal.OpenEx(
        "data/sqlite/poly_spatialite.sqlite", gdal.OF_VECTOR, allowed_drivers=["ADBC"]
    ) as ds:
        assert ds.GetLayerCount() == 13
        assert ds.GetLayer(-1) is None
        assert ds.GetLayer(ds.GetLayerCount()) is None
        lyr = ds.GetLayer(0)
        assert lyr.TestCapability(ogr.OLCFastGetArrowStream)


###############################################################################


@pytest.mark.skipif(
    not _has_adbc_driver_manager(),
    reason="ADBC driver built without AdbcDriverManager",
)
@pytest.mark.skipif(
    not _has_sqlite_driver(),
    reason="adbc_driver_sqlite missing",
)
@pytest.mark.require_driver("GPKG")
def test_ogr_adbc_create_empty_gpkg_and_open(tmp_path):

    filename = tmp_path / "out.gpkg"
    ogr.GetDriverByName("GPKG").CreateDataSource(filename)
    with pytest.raises(Exception):
        ogr.Open(filename)

    with gdal.OpenEx(filename, allowed_drivers=["ADBC"]) as ds:
        assert ds.GetLayerCount() == 6

    with ogr.Open("ADBC:" + str(filename)) as ds:
        assert ds.GetLayerCount() == 6


###############################################################################


@pytest.mark.skipif(
    not _has_adbc_driver_manager(),
    reason="ADBC driver built without AdbcDriverManager",
)
@pytest.mark.skipif(
    not _has_sqlite_driver(),
    reason="adbc_driver_sqlite missing",
)
def test_ogr_adbc_sql_open_option():

    with gdal.OpenEx(
        "ADBC:data/sqlite/poly_spatialite.sqlite",
        gdal.OF_VECTOR,
        open_options=["SQL=SELECT * FROM poly"],
    ) as ds:
        assert ds.GetLayerCount() == 1
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 10


###############################################################################


@pytest.mark.skipif(
    not _has_sqlite_driver(),
    reason="adbc_driver_sqlite missing",
)
def test_ogr_adbc_invalid_sql():

    with pytest.raises(Exception):
        gdal.OpenEx(
            "ADBC:data/sqlite/poly_spatialite.sqlite",
            gdal.OF_VECTOR,
            open_options=["SQL=SELECT * FROM"],
        )


###############################################################################


@pytest.mark.skipif(
    not _has_adbc_driver_manager(),
    reason="ADBC driver built without AdbcDriverManager",
)
@pytest.mark.skipif(
    not _has_sqlite_driver(),
    reason="adbc_driver_sqlite missing",
)
def test_ogr_adbc_generic_open_option():

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


@pytest.mark.skipif(
    not _has_adbc_driver_manager(),
    reason="ADBC driver built without AdbcDriverManager",
)
@pytest.mark.skipif(
    not _has_sqlite_driver(),
    reason="adbc_driver_sqlite missing",
)
def test_ogr_adbc_execute_sql():

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


@pytest.mark.skipif(
    not _has_duckdb_driver(),
    reason="duckdb driver missing",
)
def test_ogr_adbc_duckdb_parquet():

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


@pytest.mark.skipif(
    not _has_duckdb_driver(),
    reason="duckdb driver missing",
)
def test_ogr_adbc_duckdb_parquet_with_sql_open_option():

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


@pytest.mark.skipif(
    not _has_duckdb_driver(),
    reason="duckdb driver missing",
)
@pytest.mark.parametrize("OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL", ["ON", "OFF"])
def test_ogr_adbc_duckdb_parquet_with_spatial(OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL):

    with gdal.config_option(
        "OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL", OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL
    ):
        with gdal.OpenEx(
            "data/parquet/poly.parquet",
            gdal.OF_VECTOR,
            allowed_drivers=["ADBC"],
            open_options=(
                [
                    "PRELUDE_STATEMENTS=INSTALL spatial",
                ]
                if OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL == "ON"
                else []
            ),
        ) as ds:
            lyr = ds.GetLayer(0)
            assert lyr.GetGeomType() == ogr.wkbPolygon
            assert lyr.TestCapability(ogr.OLCFastGetExtent)
            assert lyr.TestCapability(ogr.OLCFastSpatialFilter)
            minx, maxx, miny, maxy = lyr.GetExtent()
            assert (minx, maxx, miny, maxy) == (
                478315.53125,
                481645.3125,
                4762880.5,
                4765610.5,
            )
            assert lyr.GetExtent3D() == (
                478315.53125,
                481645.3125,
                4762880.5,
                4765610.5,
                float("inf"),
                float("-inf"),
            )
            assert lyr.GetSpatialRef().GetAuthorityCode(None) == "27700"
            f = lyr.GetNextFeature()
            assert f.GetGeometryRef().ExportToWkt().startswith("POLYGON ((")

            assert lyr.GetFeatureCount() == 10
            lyr.SetAttributeFilter("false")

            assert lyr.GetFeatureCount() == 0
            lyr.SetAttributeFilter("true")

            lyr.SetAttributeFilter(None)
            assert lyr.GetFeatureCount() == 10
            lyr.SetSpatialFilterRect(minx, miny, maxx, maxy)
            assert lyr.GetFeatureCount() == 10
            lyr.SetSpatialFilterRect(minx, miny, minx, maxy)
            assert lyr.GetFeatureCount() < 10
            lyr.SetSpatialFilterRect(maxx, miny, maxx, maxy)
            assert lyr.GetFeatureCount() < 10
            lyr.SetSpatialFilterRect(minx, miny, maxx, miny)
            assert lyr.GetFeatureCount() < 10
            lyr.SetSpatialFilterRect(minx, maxy, maxx, maxy)
            assert lyr.GetFeatureCount() < 10

            lyr.SetAttributeFilter("true")
            lyr.SetSpatialFilter(None)
            assert lyr.GetFeatureCount() == 10
            lyr.SetSpatialFilterRect(minx, miny, maxx, maxy)
            assert lyr.GetFeatureCount() == 10

            lyr.SetAttributeFilter("false")
            lyr.SetSpatialFilterRect(minx, miny, maxx, maxy)
            assert lyr.GetFeatureCount() == 0


###############################################################################


@pytest.mark.skipif(
    not _has_duckdb_driver(),
    reason="duckdb driver missing",
)
@pytest.mark.parametrize("OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL", ["ON", "OFF"])
def test_ogr_adbc_duckdb_parquet_with_spatial_and_SQL_open_optoin(
    OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL,
):

    with gdal.config_option(
        "OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL", OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL
    ):
        open_options = ["SQL=SELECT * FROM 'data/parquet/poly.parquet' LIMIT 1"]
        if OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL == "ON":
            open_options += ["PRELUDE_STATEMENTS=INSTALL spatial"]
        with gdal.OpenEx("ADBC:", gdal.OF_VECTOR, open_options=open_options) as ds:
            lyr = ds.GetLayer(0)
            assert lyr.GetGeomType() == ogr.wkbPolygon
            assert lyr.GetFeatureCount() == 1


###############################################################################


@pytest.mark.skipif(
    not _has_duckdb_driver(),
    reason="duckdb driver missing",
)
@pytest.mark.parametrize("OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL", ["ON", "OFF"])
def test_ogr_adbc_duckdb_with_spatial_index(OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL):

    with gdal.config_option(
        "OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL", OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL
    ):
        ds = ogr.Open("data/duckdb/poly_with_spatial_index.duckdb")
        lyr = ds.GetLayer(0)
        with ds.ExecuteSQL(
            "SELECT 1 FROM duckdb_extensions() WHERE extension_name='spatial' AND loaded = true"
        ) as sql_lyr:
            spatial_loaded = sql_lyr.GetNextFeature() is not None
        assert lyr.TestCapability(ogr.OLCFastSpatialFilter) == spatial_loaded


###############################################################################


@pytest.mark.skipif(
    not _has_duckdb_driver(),
    reason="duckdb driver missing",
)
@pytest.mark.skipif(
    not _has_adbc_driver_manager(),
    reason="ADBC driver built without AdbcDriverManager",
)
def test_ogr_adbc_duckdb_sql(tmp_path):

    tmp_filename = str(tmp_path / "test.parquet")
    shutil.copy("data/parquet/poly.parquet", tmp_filename)
    ds = gdal.OpenEx(
        "ADBC:",
        open_options=[
            "ADBC_DRIVER=libduckdb",
            f"SQL=SELECT * FROM read_parquet('{tmp_path}/*', hive_partitioning=1)",
        ],
    )
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef() is not None


###############################################################################


@pytest.mark.skipif(
    not _has_duckdb_driver(),
    reason="duckdb driver missing",
)
@pytest.mark.skipif(
    not _has_adbc_driver_manager(),
    reason="ADBC driver built without AdbcDriverManager",
)
def test_ogr_adbc_duckdb_not_existing_file(tmp_path):

    with pytest.raises(Exception, match="/i/do_not/exist does not exist"):
        gdal.OpenEx(
            "ADBC:/i/do_not/exist",
            open_options=["ADBC_DRIVER=libduckdb"],
        )


###############################################################################


@pytest.mark.skipif(
    not _has_duckdb_driver(),
    reason="duckdb driver missing",
)
@pytest.mark.skipif(
    not _has_adbc_driver_manager(),
    reason="ADBC driver built without AdbcDriverManager",
)
def test_ogr_adbc_duckdb_memory(tmp_path):

    with gdal.OpenEx(
        "ADBC::memory:",
        open_options=["ADBC_DRIVER=libduckdb", "SQL=SELECT 1"],
    ) as ds:
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f.GetField(0) == 1


###############################################################################
# Run test_ogrsf on a SQLite3 database


@pytest.mark.skipif(
    not _has_sqlite_driver(),
    reason="adbc_driver_sqlite driver missing",
)
@pytest.mark.skipif(
    not _has_adbc_driver_manager(),
    reason="ADBC driver built without AdbcDriverManager",
)
def test_ogr_adbc_test_ogrsf_sqlite3():
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


@pytest.mark.skipif(
    not _has_duckdb_driver(),
    reason="duckdb driver missing",
)
def test_ogr_adbc_test_ogrsf_parquet():

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


@pytest.mark.skipif(
    not _has_duckdb_driver(),
    reason="duckdb driver missing",
)
def test_ogr_adbc_test_ogrsf_parquet_filename_with_glob():

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
# Run test_ogrsf on a GeoParquet file


@pytest.mark.skipif(
    not _has_duckdb_driver(),
    reason="duckdb driver missing",
)
@pytest.mark.parametrize("OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL", ["ON", "OFF"])
def test_ogr_adbc_test_ogrsf_geoparquet(OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + f" -ro ADBC:data/parquet/poly.parquet --config OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL={OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL}"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################
# Test DATETIME_AS_STRING=YES GetArrowStream() option


@pytest.mark.skipif(
    not _has_duckdb_driver(),
    reason="duckdb driver missing",
)
def test_ogr_adbc_arrow_stream_numpy_datetime_as_string(tmp_vsimem):
    gdaltest.importorskip_gdal_array()
    pytest.importorskip("numpy")

    with gdal.OpenEx(
        "data/parquet/test.parquet", gdal.OF_VECTOR, allowed_drivers=["ADBC"]
    ) as ds:
        lyr = ds.GetLayer(0)
        stream = lyr.GetArrowStreamAsNumPy(
            options=["USE_MASKED_ARRAYS=NO", "DATETIME_AS_STRING=YES"]
        )
        batches = [batch for batch in stream]
        batch = batches[0]
        # Should be "2019-01-01T14:00:00.500-02:15" but DuckDB returns in UTC
        # On my machine, for some reason it returns without the Z, whereas on
        # the ubuntu_2404 it returns with the Z... despite both using libduckdb 1.1.3
        # at time of writing...
        assert batch["timestamp_ms_gmt_minus_0215"][0] in (
            b"2019-01-01T16:15:00.500",
            b"2019-01-01T16:15:00.500Z",
        )


###############################################################################
# Run test_ogrsf on a DuckDB dataset


@pytest.mark.skipif(
    not _has_duckdb_driver(),
    reason="duckdb driver missing",
)
@pytest.mark.parametrize("OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL", ["ON", "OFF"])
def test_ogr_adbc_test_ogrsf_duckdb(OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + f" -ro ADBC:data/duckdb/poly.duckdb --config OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL={OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL}"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################
# Run test_ogrsf on a DuckDB dataset


@pytest.mark.skipif(
    not _has_duckdb_driver(),
    reason="duckdb driver missing",
)
@pytest.mark.parametrize("OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL", ["ON", "OFF"])
def test_ogr_adbc_test_ogrsf_duckdb_with_spatial_index(
    OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL,
):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + f" -ro ADBC:data/duckdb/poly_with_spatial_index.duckdb --config OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL={OGR_ADBC_AUTO_LOAD_DUCKDB_SPATIAL}"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################


@pytest.mark.skipif(
    not _has_sqlite_driver(),
    reason="adbc_driver_sqlite missing",
)
@pytest.mark.skipif(
    not _has_adbc_driver_manager(),
    reason="ADBC driver built without AdbcDriverManager",
)
def test_ogr_adbc_layer_list():

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
