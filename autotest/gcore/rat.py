#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test RasterAttributeTables services from Python.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import datetime

import gdaltest
import pytest

from osgeo import gdal, ogr

###############################################################################
# Create a raster attribute table.


@pytest.fixture()
def test_rat():

    rat = gdal.RasterAttributeTable()

    rat.CreateColumn("Value", gdal.GFT_Integer, gdal.GFU_MinMax)
    rat.CreateColumn("Count", gdal.GFT_Integer, gdal.GFU_PixelCount)

    rat.SetRowCount(3)
    rat.SetValueAsInt(0, 0, 10)
    rat.SetValueAsInt(0, 1, 100)
    rat.SetValueAsInt(1, 0, 11)
    rat.SetValueAsInt(1, 1, 200)
    rat.SetValueAsInt(2, 0, 12)
    rat.SetValueAsInt(2, 1, 90)

    return rat


def test_rat_1(test_rat):

    rat2 = test_rat.Clone()

    assert rat2.GetColumnCount() == 2, "wrong column count"

    assert rat2.GetRowCount() == 3, "wrong row count"

    assert rat2.GetNameOfCol(1) == "Count", "wrong column name"

    assert rat2.GetUsageOfCol(1) == gdal.GFU_PixelCount, "wrong column usage"

    assert rat2.GetTypeOfCol(1) == gdal.GFT_Integer, "wrong column type"

    assert rat2.GetRowOfValue(11.0) == 1, "wrong row for value"

    assert rat2.GetValueAsInt(1, 1) == 200, "wrong field value."


###############################################################################
# Save a RAT in a file, written to .aux.xml, read it back and check it.


@pytest.mark.require_driver("PNM")
def test_rat_2(test_rat, tmp_vsimem):

    with gdal.GetDriverByName("PNM").Create(
        tmp_vsimem / "rat_2.pnm", 100, 90, 1, gdal.GDT_UInt8
    ) as ds:
        ds.GetRasterBand(1).SetDefaultRAT(test_rat)

    with gdal.Open(tmp_vsimem / "rat_2.pnm", gdal.GA_Update) as ds:
        rat2 = ds.GetRasterBand(1).GetDefaultRAT()

        assert rat2.GetColumnCount() == 2, "wrong column count"

        assert rat2.GetRowCount() == 3, "wrong row count"

        assert rat2.GetNameOfCol(1) == "Count", "wrong column name"

        assert rat2.GetUsageOfCol(1) == gdal.GFU_PixelCount, "wrong column usage"

        assert rat2.GetTypeOfCol(1) == gdal.GFT_Integer, "wrong column type"

        assert rat2.GetRowOfValue(11.0) == 1, "wrong row for value"

        assert rat2.GetValueAsInt(1, 1) == 200, "wrong field value."

        # unset the RAT
        ds.GetRasterBand(1).SetDefaultRAT(None)

    with gdal.Open(tmp_vsimem / "rat_2.pnm") as ds:
        rat = ds.GetRasterBand(1).GetDefaultRAT()
        assert rat is None, "expected a NULL RAT."


###############################################################################
# Save an empty RAT (#5451)


def test_rat_3(tmp_vsimem):

    with gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "rat_3.tif", 1, 1) as ds:
        ds.GetRasterBand(1).SetDefaultRAT(gdal.RasterAttributeTable())


###############################################################################
# Edit an existing RAT (#3783)


def test_rat_4(tmp_vsimem):

    # Create test RAT
    with gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "rat_4.tif", 1, 1) as ds:
        rat = gdal.RasterAttributeTable()
        rat.CreateColumn("VALUE", gdal.GFT_Integer, gdal.GFU_MinMax)
        rat.CreateColumn("CLASS", gdal.GFT_String, gdal.GFU_Name)
        rat.SetValueAsInt(0, 0, 111)
        rat.SetValueAsString(0, 1, "Class1")
        ds.GetRasterBand(1).SetDefaultRAT(rat)

    # Verify
    with gdal.OpenEx(tmp_vsimem / "rat_4.tif") as ds:
        gdal_band = ds.GetRasterBand(1)
        rat = gdal_band.GetDefaultRAT()
        assert rat.GetValueAsInt(0, 0) == 111

    # Replace existing RAT
    rat = gdal.RasterAttributeTable()
    rat.CreateColumn("VALUE", gdal.GFT_Integer, gdal.GFU_MinMax)
    rat.CreateColumn("CLASS", gdal.GFT_String, gdal.GFU_Name)
    rat.SetValueAsInt(0, 0, 222)
    rat.SetValueAsString(0, 1, "Class1")
    with gdal.OpenEx(tmp_vsimem / "rat_4.tif", gdal.OF_RASTER | gdal.OF_UPDATE) as ds:
        gdal_band = ds.GetRasterBand(1)
        gdal_band.SetDefaultRAT(rat)

    # Verify
    with gdal.OpenEx(tmp_vsimem / "rat_4.tif") as ds:
        gdal_band = ds.GetRasterBand(1)
        rat = gdal_band.GetDefaultRAT()
        assert rat is not None
        assert rat.GetValueAsInt(0, 0) == 222


###############################################################################
# Read multiple rows


def test_rat_readvaluesio():

    rat = gdal.RasterAttributeTable()
    rat.SetRowCount(5)
    rat.CreateColumn("VALUE_INT", gdal.GFT_Integer, gdal.GFU_Generic)
    rat.CreateColumn("VALUE_REAL", gdal.GFT_Real, gdal.GFU_Generic)
    rat.CreateColumn("VALUE_STRING", gdal.GFT_String, gdal.GFU_Generic)
    rat.CreateColumn("VALUE_BOOL", gdal.GFT_Boolean, gdal.GFU_Generic)
    rat.CreateColumn("VALUE_DATETIME", gdal.GFT_DateTime, gdal.GFU_Generic)
    rat.CreateColumn("VALUE_WKBGEOMETRY", gdal.GFT_WKBGeometry, gdal.GFU_Generic)

    for i in range(rat.GetRowCount()):
        rat.SetValueAsInt(i, 0, i + 7)
        rat.SetValueAsDouble(i, 1, i + 7.1)
        rat.SetValueAsString(i, 2, f"x{i + 7}s")
        rat.SetValueAsBoolean(i, 3, (i % 2) != 0)

    # start index < 0
    with pytest.raises(Exception, match="out of range"):
        rat.ReadValuesIOAsInteger(0, -1, 3)

    # col index < 0
    with pytest.raises(Exception, match="out of range"):
        rat.ReadValuesIOAsInteger(-1, 0, 5)

    # col index > max
    with pytest.raises(Exception, match="out of range"):
        rat.ReadValuesIOAsInteger(6, 0, 5)

    assert rat.ReadValuesIOAsInteger(0, 0, 5) == [7, 8, 9, 10, 11]
    assert rat.ReadValuesIOAsInteger(0, 2, 3) == [9, 10, 11]
    # assert rat.ReadValuesIOAsInteger(0, 2, 5) is None

    assert rat.ReadValuesIOAsDouble(1, 0, 5) == [7.1, 8.1, 9.1, 10.1, 11.1]
    assert rat.ReadValuesIOAsDouble(1, 2, 3) == [9.1, 10.1, 11.1]

    with pytest.raises(Exception, match="out of range"):
        rat.ReadValuesIOAsDouble(1, -1, 3)

    assert rat.ReadValuesIOAsString(2, 0, 5) == ["x7s", "x8s", "x9s", "x10s", "x11s"]
    assert rat.ReadValuesIOAsString(2, 2, 3) == ["x9s", "x10s", "x11s"]

    with pytest.raises(Exception, match="out of range"):
        rat.ReadValuesIOAsString(2, -1, 3)

    assert rat.ReadValuesIOAsBoolean(3, 0, 5) == [False, True, False, True, False]

    with pytest.raises(Exception, match=r"iField \(-1\) out of range"):
        rat.ReadValuesIOAsBoolean(-1, 0, 5)
    with pytest.raises(Exception, match=r"iRow \(-1\) out of range"):
        rat.ReadValuesIOAsBoolean(0, -1, 5)


def test_rat_readasarray():

    np = pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    rat = gdal.RasterAttributeTable()
    rat.SetRowCount(5)
    rat.CreateColumn("VALUE_INT", gdal.GFT_Integer, gdal.GFU_Generic)
    rat.CreateColumn("VALUE_REAL", gdal.GFT_Real, gdal.GFU_Generic)
    rat.CreateColumn("VALUE_STRING", gdal.GFT_String, gdal.GFU_Generic)
    rat.CreateColumn("VALUE_BOOL", gdal.GFT_Boolean, gdal.GFU_Generic)
    rat.CreateColumn("VALUE_DATETIME", gdal.GFT_DateTime, gdal.GFU_Generic)
    rat.CreateColumn("VALUE_WKBGEOMETRY", gdal.GFT_WKBGeometry, gdal.GFU_Generic)

    for i in range(rat.GetRowCount()):
        rat.SetValueAsInt(i, 0, i + 7)
        rat.SetValueAsDouble(i, 1, i + 7.1)
        rat.SetValueAsString(i, 2, f"x{i + 7}s")

    # start index < 0
    with pytest.raises(Exception, match="out of range"):
        rat.ReadAsArray(0, -1, 3)

    # col index < 0
    with pytest.raises(Exception, match="out of range"):
        rat.ReadAsArray(-1, 0, 5)

    # col index > max
    with pytest.raises(Exception, match="out of range"):
        rat.ReadAsArray(6, 0, 5)

    # reading before first row row
    with pytest.raises(Exception, match="must be a positive integer"):
        rat.ReadAsArray(0, 2, -5)

    # reading beyond last row
    with pytest.raises(Exception):
        rat.ReadAsArray(0, 2, 5)

    np.testing.assert_array_equal(
        rat.ReadAsArray(0, 0, 5), np.array([7, 8, 9, 10, 11], dtype=np.int32)
    )
    np.testing.assert_array_equal(
        rat.ReadAsArray(0, 2, 3), np.array([9, 10, 11], dtype=np.int32)
    )

    np.testing.assert_array_equal(
        rat.ReadAsArray(1, 0, 5),
        np.array([7.1, 8.1, 9.1, 10.1, 11.1], dtype=np.float64),
    )
    np.testing.assert_array_equal(
        rat.ReadAsArray(1, 2, 3), np.array([9.1, 10.1, 11.1], dtype=np.float64)
    )

    with pytest.raises(Exception, match="out of range"):
        rat.ReadAsArray(1, -1, 3)

    np.testing.assert_array_equal(
        rat.ReadAsArray(2, 0, 5),
        np.array(["x7s", "x8s", "x9s", "x10s", "x11s"], dtype=np.bytes_),
    )
    np.testing.assert_array_equal(
        rat.ReadAsArray(2, 2, 3), np.array(["x9s", "x10s", "x11s"], dtype=np.bytes_)
    )

    with pytest.raises(Exception, match="out of range"):
        rat.ReadAsArray(2, -1, 3)

    # Test GFT_Boolean
    rat.WriteArray(np.array([False, True, False, True, False], dtype=np.bool_), 3, 0)
    np.testing.assert_array_equal(
        rat.ReadAsArray(3, 0, 5),
        np.array([False, True, False, True, False], dtype=np.bool_),
    )

    # Test GFT_DateTime
    dt = datetime.datetime(
        2025,
        12,
        31,
        10,
        30,
        59,
        500000,
        tzinfo=datetime.timezone(datetime.timedelta(seconds=4500)),
    )
    rat.WriteArray(np.array([dt, None], dtype=object), 4, 0)
    np.testing.assert_array_equal(
        rat.ReadAsArray(4, 0, 2),
        np.array([dt, None], dtype=object),
    )

    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.WriteArray(np.array([1.5], dtype=np.float64), 4, 0)
    with pytest.raises(Exception, match="Non-datetime object found at index 0"):
        rat.WriteArray(np.array(["foo"], dtype=object), 4, 0)

    # Test GFT_WKBGeometry
    rat.WriteArray(np.array([b"\x01\x02"], dtype=object), 5, 0)
    np.testing.assert_array_equal(
        rat.ReadAsArray(5, 0, 1),
        np.array([b"\x01\x02"], dtype=object),
    )

    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.WriteArray(np.array([1.5], dtype=np.float64), 5, 0)
    with pytest.raises(Exception, match="Non-byte object found at index 0"):
        rat.WriteArray(np.array(["foo"], dtype=object), 5, 0)


def test_rat_int():

    rat = gdal.RasterAttributeTable()
    rat.CreateColumn("col", gdal.GFT_Integer, gdal.GFU_Generic)
    rat.SetRowCount(1)

    with pytest.raises(Exception, match=r"iRow \(-1\) out of range"):
        rat.GetValueAsInt(-1, 0)

    with pytest.raises(Exception, match=r"iField \(-1\) out of range"):
        rat.GetValueAsInt(0, -1)

    with pytest.raises(Exception, match=r"iRow \(-1\) out of range"):
        rat.SetValueAsInt(-1, 0, True)

    with pytest.raises(Exception, match=r"iField \(-1\) out of range"):
        rat.SetValueAsInt(0, -1, True)

    rat.SetValueAsInt(0, 0, 12345)
    assert rat.GetValueAsInt(0, 0) == 12345
    assert rat.GetValueAsBoolean(0, 0)
    assert rat.GetValueAsDouble(0, 0) == 12345
    assert rat.GetValueAsString(0, 0) == "12345"

    rat.SetValueAsBoolean(0, 0, True)
    assert rat.GetValueAsInt(0, 0) == 1

    rat.SetValueAsDouble(0, 0, 12345.4)
    assert rat.GetValueAsInt(0, 0) == 12345

    rat.SetValueAsString(0, 0, "12345")
    assert rat.GetValueAsInt(0, 0) == 12345

    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.GetValueAsDateTime(0, 0)
    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.GetValueAsWKBGeometry(0, 0)
    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.SetValueAsDateTime(0, 0, datetime.datetime.fromtimestamp(0))
    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.SetValueAsWKBGeometry(0, 0, b"")


def test_rat_real():

    rat = gdal.RasterAttributeTable()
    rat.CreateColumn("col", gdal.GFT_Real, gdal.GFU_Generic)
    rat.SetRowCount(1)

    with pytest.raises(Exception, match=r"iRow \(-1\) out of range"):
        rat.GetValueAsDouble(-1, 0)

    with pytest.raises(Exception, match=r"iField \(-1\) out of range"):
        rat.GetValueAsDouble(0, -1)

    with pytest.raises(Exception, match=r"iRow \(-1\) out of range"):
        rat.SetValueAsDouble(-1, 0, True)

    with pytest.raises(Exception, match=r"iField \(-1\) out of range"):
        rat.SetValueAsDouble(0, -1, True)

    rat.SetValueAsDouble(0, 0, 12345.5)
    assert rat.GetValueAsInt(0, 0) == 12345
    assert rat.GetValueAsBoolean(0, 0)
    assert rat.GetValueAsDouble(0, 0) == 12345.5
    assert rat.GetValueAsString(0, 0) == "12345.5"

    rat.SetValueAsBoolean(0, 0, True)
    assert rat.GetValueAsDouble(0, 0) == 1

    rat.SetValueAsInt(0, 0, 12345)
    assert rat.GetValueAsDouble(0, 0) == 12345

    rat.SetValueAsString(0, 0, "12345.5")
    assert rat.GetValueAsDouble(0, 0) == 12345.5

    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.GetValueAsDateTime(0, 0)
    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.GetValueAsWKBGeometry(0, 0)
    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.SetValueAsDateTime(0, 0, datetime.datetime.fromtimestamp(0))
    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.SetValueAsWKBGeometry(0, 0, b"")


def test_rat_string():

    rat = gdal.RasterAttributeTable()
    rat.CreateColumn("col", gdal.GFT_String, gdal.GFU_Generic)
    rat.SetRowCount(1)

    with pytest.raises(Exception, match=r"iRow \(-1\) out of range"):
        rat.GetValueAsString(-1, 0)

    with pytest.raises(Exception, match=r"iField \(-1\) out of range"):
        rat.GetValueAsString(0, -1)

    with pytest.raises(Exception, match=r"iRow \(-1\) out of range"):
        rat.SetValueAsString(-1, 0, True)

    with pytest.raises(Exception, match=r"iField \(-1\) out of range"):
        rat.SetValueAsString(0, -1, True)

    rat.SetValueAsString(0, 0, "12345.5")
    assert rat.GetValueAsInt(0, 0) == 12345
    assert rat.GetValueAsBoolean(0, 0)
    assert rat.GetValueAsDouble(0, 0) == 12345.5
    assert rat.GetValueAsString(0, 0) == "12345.5"

    rat.SetValueAsBoolean(0, 0, True)
    assert rat.GetValueAsString(0, 0) == "true"

    rat.SetValueAsInt(0, 0, 12345)
    assert rat.GetValueAsString(0, 0) == "12345"

    rat.SetValueAsDouble(0, 0, 12345.5)
    assert rat.GetValueAsString(0, 0) == "12345.5"

    dt = datetime.datetime(
        2025,
        12,
        31,
        23,
        58,
        59,
        500000,
        tzinfo=datetime.timezone(datetime.timedelta(seconds=4500)),
    )
    rat.SetValueAsDateTime(0, 0, dt)
    assert rat.GetValueAsString(0, 0) == "2025-12-31T23:58:59.500+01:15"

    g = ogr.Geometry(ogr.wkbPoint)
    g.SetPoint_2D(0, 1, 2)
    wkb = g.ExportToWkb()
    rat.SetValueAsWKBGeometry(0, 0, wkb)
    assert rat.GetValueAsString(0, 0) == "POINT (1 2)"


def test_rat_bool():

    rat = gdal.RasterAttributeTable()
    rat.CreateColumn("col", gdal.GFT_Boolean, gdal.GFU_Generic)
    rat.SetRowCount(1)

    with pytest.raises(Exception, match=r"iRow \(-1\) out of range"):
        rat.GetValueAsBoolean(-1, 0)

    with pytest.raises(Exception, match=r"iField \(-1\) out of range"):
        rat.GetValueAsBoolean(0, -1)

    with pytest.raises(Exception, match=r"iRow \(-1\) out of range"):
        rat.SetValueAsBoolean(-1, 0, True)

    with pytest.raises(Exception, match=r"iField \(-1\) out of range"):
        rat.SetValueAsBoolean(0, -1, True)

    rat.SetValueAsBoolean(0, 0, True)
    assert rat.GetValueAsBoolean(0, 0)
    assert rat.GetValueAsInt(0, 0)
    assert rat.GetValueAsDouble(0, 0)
    assert rat.GetValueAsString(0, 0) == "true"

    rat.SetValueAsBoolean(0, 0, False)
    assert not rat.GetValueAsBoolean(0, 0)
    assert rat.GetValueAsString(0, 0) == "false"

    rat.SetValueAsInt(0, 0, 1)
    assert rat.GetValueAsBoolean(0, 0)
    rat.SetValueAsBoolean(0, 0, False)

    rat.SetValueAsDouble(0, 0, 1)
    assert rat.GetValueAsBoolean(0, 0)
    rat.SetValueAsBoolean(0, 0, False)

    rat.SetValueAsString(0, 0, "true")
    assert rat.GetValueAsBoolean(0, 0)

    rat.SetValueAsString(0, 0, "false")
    assert not rat.GetValueAsBoolean(0, 0)

    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.GetValueAsDateTime(0, 0)
    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.GetValueAsWKBGeometry(0, 0)
    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.SetValueAsDateTime(0, 0, datetime.datetime.fromtimestamp(0))
    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.SetValueAsWKBGeometry(0, 0, b"")


def test_rat_datetime():

    rat = gdal.RasterAttributeTable()
    rat.CreateColumn("col", gdal.GFT_DateTime, gdal.GFU_Generic)
    rat.SetRowCount(1)

    dt = datetime.datetime(
        2025,
        12,
        31,
        23,
        58,
        59,
        500000,
        tzinfo=datetime.timezone(datetime.timedelta(seconds=4500)),
    )

    with pytest.raises(Exception, match=r"iRow \(-1\) out of range"):
        rat.GetValueAsDateTime(-1, 0)

    with pytest.raises(Exception, match=r"iField \(-1\) out of range"):
        rat.GetValueAsDateTime(0, -1)

    with pytest.raises(Exception, match=r"iRow \(-1\) out of range"):
        rat.SetValueAsDateTime(-1, 0, dt)

    with pytest.raises(Exception, match=r"iField \(-1\) out of range"):
        rat.SetValueAsDateTime(0, -1, dt)

    rat.SetValueAsDateTime(0, 0, dt)

    assert rat.GetValueAsDateTime(0, 0) == dt

    assert rat.GetValueAsString(0, 0) == "2025-12-31T23:58:59.500+01:15"

    rat.SetValueAsDateTime(0, 0, gdal.RATDateTime())
    assert rat.GetValueAsDateTime(0, 0) is None

    rat.SetValueAsString(0, 0, "2025-12-31T23:58:59.500+01:15")
    assert rat.GetValueAsString(0, 0) == "2025-12-31T23:58:59.500+01:15"

    rat.SetValueAsDateTime(0, 0, None)
    assert rat.GetValueAsDateTime(0, 0) is None

    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.GetValueAsInt(0, 0)
    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.GetValueAsDouble(0, 0)
    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.GetValueAsBoolean(0, 0)
    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.GetValueAsWKBGeometry(0, 0)

    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.SetValueAsInt(0, 0, 0)
    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.SetValueAsDouble(0, 0, 0)
    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.SetValueAsBoolean(0, 0, False)
    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.SetValueAsWKBGeometry(0, 0, b"")


def test_rat_wkbgeometry():

    rat = gdal.RasterAttributeTable()
    rat.CreateColumn("col", gdal.GFT_WKBGeometry, gdal.GFU_Generic)
    rat.SetRowCount(1)

    with pytest.raises(Exception, match=r"iRow \(-1\) out of range"):
        rat.GetValueAsWKBGeometry(-1, 0)

    with pytest.raises(Exception, match=r"iField \(-1\) out of range"):
        rat.GetValueAsWKBGeometry(0, -1)

    with pytest.raises(Exception, match=r"iRow \(-1\) out of range"):
        rat.SetValueAsWKBGeometry(-1, 0, b"")

    with pytest.raises(Exception, match=r"iField \(-1\) out of range"):
        rat.SetValueAsWKBGeometry(0, -1, b"")

    g = ogr.Geometry(ogr.wkbPoint)
    g.SetPoint_2D(0, 1, 2)
    wkb = g.ExportToWkb()

    rat.SetValueAsWKBGeometry(0, 0, wkb)
    assert rat.GetValueAsWKBGeometry(0, 0) == wkb

    assert rat.GetValueAsString(0, 0) == "POINT (1 2)"

    rat.SetValueAsString(0, 0, "POINT (3.0 4)")
    assert rat.GetValueAsString(0, 0) == "POINT (3 4)"

    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.GetValueAsInt(0, 0)
    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.GetValueAsDouble(0, 0)
    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.GetValueAsBoolean(0, 0)
    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.GetValueAsDateTime(0, 0)

    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.SetValueAsInt(0, 0, 0)
    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.SetValueAsDouble(0, 0, 0)
    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.SetValueAsBoolean(0, 0, False)
    with pytest.raises(Exception, match="Incompatible RAT field type"):
        rat.SetValueAsDateTime(0, 0, datetime.datetime.fromtimestamp(0))


###############################################################################


def test_rat_as_json():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    rat = gdal.RasterAttributeTable()
    rat.SetRowCount(2)
    rat.CreateColumn("VALUE_INT", gdal.GFT_Integer, gdal.GFU_Generic)
    rat.CreateColumn("VALUE_REAL", gdal.GFT_Real, gdal.GFU_Generic)
    rat.CreateColumn("VALUE_STRING", gdal.GFT_String, gdal.GFU_Generic)
    rat.CreateColumn("VALUE_BOOL", gdal.GFT_Boolean, gdal.GFU_Generic)
    rat.CreateColumn("VALUE_DATETIME", gdal.GFT_DateTime, gdal.GFU_Generic)
    rat.CreateColumn("VALUE_WKBGEOMETRY", gdal.GFT_WKBGeometry, gdal.GFU_Generic)
    rat.SetValueAsInt(0, 0, 123)
    rat.SetValueAsDouble(0, 1, 45.5)
    rat.SetValueAsString(0, 2, "str")
    rat.SetValueAsBoolean(0, 3, True)
    rat.SetValueAsString(0, 4, "2025-12-31T23:58:59.500+01:15")
    rat.SetValueAsString(0, 5, "POINT (1.0 2)")
    ds.GetRasterBand(1).SetDefaultRAT(rat)

    j = gdal.Info(ds, format="json")["bands"][0]["rat"]
    assert j == {
        "tableType": "thematic",
        "fieldDefn": [
            {"index": 0, "name": "VALUE_INT", "type": 0, "usage": 0},
            {"index": 1, "name": "VALUE_REAL", "type": 1, "usage": 0},
            {"index": 2, "name": "VALUE_STRING", "type": 2, "usage": 0},
            {"index": 3, "name": "VALUE_BOOL", "type": 3, "usage": 0},
            {"index": 4, "name": "VALUE_DATETIME", "type": 4, "usage": 0},
            {"index": 5, "name": "VALUE_WKBGEOMETRY", "type": 5, "usage": 0},
        ],
        "row": [
            {
                "index": 0,
                "f": [
                    123,
                    45.5,
                    "str",
                    True,
                    "2025-12-31T23:58:59.500+01:15",
                    "POINT (1 2)",
                ],
            },
            {
                "f": [
                    0,
                    0.0,
                    "",
                    False,
                    None,
                    None,
                ],
                "index": 1,
            },
        ],
    }


###############################################################################


def test_rat_as_xml():

    ds = gdal.GetDriverByName("VRT").Create("", 1, 1)
    rat = gdal.RasterAttributeTable()
    rat.SetRowCount(2)
    rat.CreateColumn("VALUE_INT", gdal.GFT_Integer, gdal.GFU_Generic)
    rat.CreateColumn("VALUE_REAL", gdal.GFT_Real, gdal.GFU_Generic)
    rat.CreateColumn("VALUE_STRING", gdal.GFT_String, gdal.GFU_Generic)
    rat.CreateColumn("VALUE_BOOL", gdal.GFT_Boolean, gdal.GFU_Generic)
    rat.CreateColumn("VALUE_DATETIME", gdal.GFT_DateTime, gdal.GFU_Generic)
    rat.CreateColumn("VALUE_WKBGEOMETRY", gdal.GFT_WKBGeometry, gdal.GFU_Generic)
    rat.SetValueAsInt(0, 0, 123)
    rat.SetValueAsDouble(0, 1, 45.5)
    rat.SetValueAsString(0, 2, "str")
    rat.SetValueAsBoolean(0, 3, True)
    rat.SetValueAsString(0, 4, "2025-12-31T23:58:59.500+01:15")
    rat.SetValueAsString(0, 5, "POINT (1.0 2)")
    ds.GetRasterBand(1).SetDefaultRAT(rat)
    xml = ds.GetMetadata("xml:VRT")[0]
    assert xml == """<VRTDataset rasterXSize="1" rasterYSize="1">
  <VRTRasterBand dataType="Byte" band="1">
    <GDALRasterAttributeTable tableType="thematic">
      <FieldDefn index="0">
        <Name>VALUE_INT</Name>
        <Type typeAsString="Integer">0</Type>
        <Usage usageAsString="Generic">0</Usage>
      </FieldDefn>
      <FieldDefn index="1">
        <Name>VALUE_REAL</Name>
        <Type typeAsString="Real">1</Type>
        <Usage usageAsString="Generic">0</Usage>
      </FieldDefn>
      <FieldDefn index="2">
        <Name>VALUE_STRING</Name>
        <Type typeAsString="String">2</Type>
        <Usage usageAsString="Generic">0</Usage>
      </FieldDefn>
      <FieldDefn index="3">
        <Name>VALUE_BOOL</Name>
        <Type typeAsString="Boolean">3</Type>
        <Usage usageAsString="Generic">0</Usage>
      </FieldDefn>
      <FieldDefn index="4">
        <Name>VALUE_DATETIME</Name>
        <Type typeAsString="DateTime">4</Type>
        <Usage usageAsString="Generic">0</Usage>
      </FieldDefn>
      <FieldDefn index="5">
        <Name>VALUE_WKBGEOMETRY</Name>
        <Type typeAsString="WKBGeometry">5</Type>
        <Usage usageAsString="Generic">0</Usage>
      </FieldDefn>
      <Row index="0">
        <F>123</F>
        <F>45.5</F>
        <F>str</F>
        <F>true</F>
        <F>2025-12-31T23:58:59.500+01:15</F>
        <F>POINT (1 2)</F>
      </Row>
      <Row index="1">
        <F>0</F>
        <F>0</F>
        <F></F>
        <F>false</F>
        <F></F>
        <F></F>
      </Row>
    </GDALRasterAttributeTable>
  </VRTRasterBand>
</VRTDataset>
"""

    ds = gdal.Open(xml)

    rat_clone = ds.GetRasterBand(1).GetDefaultRAT().Clone()
    ds.GetRasterBand(1).SetDefaultRAT(rat_clone)

    j = gdal.Info(ds, format="json")["bands"][0]["rat"]
    assert j == {
        "tableType": "thematic",
        "fieldDefn": [
            {"index": 0, "name": "VALUE_INT", "type": 0, "usage": 0},
            {"index": 1, "name": "VALUE_REAL", "type": 1, "usage": 0},
            {"index": 2, "name": "VALUE_STRING", "type": 2, "usage": 0},
            {"index": 3, "name": "VALUE_BOOL", "type": 3, "usage": 0},
            {"index": 4, "name": "VALUE_DATETIME", "type": 4, "usage": 0},
            {"index": 5, "name": "VALUE_WKBGEOMETRY", "type": 5, "usage": 0},
        ],
        "row": [
            {
                "index": 0,
                "f": [
                    123,
                    45.5,
                    "str",
                    True,
                    "2025-12-31T23:58:59.500+01:15",
                    "POINT (1 2)",
                ],
            },
            {
                "f": [
                    0,
                    0.0,
                    "",
                    False,
                    None,
                    None,
                ],
                "index": 1,
            },
        ],
    }
