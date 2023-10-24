#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test RFC35 for several drivers
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011-2014, Even Rouault <even dot rouault at spatialys.com>
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


import os

import gdaltest
import pytest

from osgeo import gdal, ogr


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


driver_extensions = {
    "ESRI Shapefile": "dbf",
    "MapInfo File": "tab",
    "SQLite": "sqlite",
    "Memory": None,
}


@pytest.fixture(autouse=True, params=driver_extensions.keys())
def driver_name(request):
    return request.param


###############################################################################
#


def CheckFileSize(src_filename, tmpdir):

    import test_py_scripts

    src_ext = os.path.splitext(src_filename)[1]

    for driver_name, ext in driver_extensions.items():
        if ext is not None and src_ext.endswith(ext):
            driver = driver_name

    dst_filename = os.path.join(tmpdir, "checkfilesize." + src_ext)

    script_path = test_py_scripts.get_py_script("ogr2ogr")
    if script_path is None:
        pytest.skip()

    test_py_scripts.run_py_script(
        script_path, "ogr2ogr", f'-f "{driver}" {dst_filename} {src_filename}'
    )

    statBufSrc = gdal.VSIStatL(
        src_filename,
        gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG,
    )
    statBufDst = gdal.VSIStatL(
        dst_filename,
        gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG,
    )

    assert statBufSrc.size == statBufDst.size, (
        "src_size = %d, dst_size = %d",
        statBufSrc.size,
        statBufDst.size,
    )


###############################################################################
# Initiate the test file


@pytest.fixture()
def rfc35_test_input(driver_name, tmp_path):

    ext = driver_extensions[driver_name]
    fname = str(tmp_path / f"rfc35_test.{ext}")

    drv = ogr.GetDriverByName(driver_name)

    if drv is None:
        pytest.skip(f"Driver {driver_name} not available")

    ds = drv.CreateDataSource(fname)
    lyr = ds.CreateLayer("rfc35_test")

    lyr.ReorderFields([])

    fd = ogr.FieldDefn("foo5", ogr.OFTString)
    fd.SetWidth(5)
    lyr.CreateField(fd)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, "foo0")
    lyr.CreateFeature(feat)
    feat = None

    fd = ogr.FieldDefn("bar10", ogr.OFTString)
    fd.SetWidth(10)
    lyr.CreateField(fd)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, "foo1")
    feat.SetField(1, "bar1")
    lyr.CreateFeature(feat)
    feat = None

    fd = ogr.FieldDefn("baz15", ogr.OFTString)
    fd.SetWidth(15)
    lyr.CreateField(fd)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, "foo2")
    feat.SetField(1, "bar2_01234")
    feat.SetField(2, "baz2_0123456789")
    lyr.CreateFeature(feat)
    feat = None

    fd = ogr.FieldDefn("baw20", ogr.OFTString)
    fd.SetWidth(20)
    lyr.CreateField(fd)

    if driver_name == "Memory":
        return ds
    else:
        return fname


def Truncate(val, lyr_defn, fieldname):
    if val is None:
        return val

    return val[0 : lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex(fieldname)).GetWidth()]


def Identity(val, *args):
    return val


def CheckFeatures(
    ds, lyr, field1="foo5", field2="bar10", field3="baz15", field4="baw20"
):

    driver_name = ds.GetDriver().GetName()

    expected_values = [
        ["foo0", None, None, None],
        ["foo1", "bar1", None, None],
        ["foo2", "bar2_01234", "baz2_0123456789", None],
        ["foo3", "bar3_01234", "baz3_0123456789", "baw3_012345678901234"],
    ]

    if driver_name == "MapInfo File":
        for i in range(len(expected_values)):
            expected_values[i] = [
                x if x is not None else "" for x in expected_values[i]
            ]

    truncate_fn = Identity if driver_name in ("SQLite", "Memory") else Truncate

    lyr_defn = lyr.GetLayerDefn()

    lyr.ResetReading()

    for i, feat in enumerate(lyr):
        if field1 is not None:
            assert feat.GetField(field1) == truncate_fn(
                expected_values[i][0], lyr_defn, field1
            )
        if field2 is not None:
            assert feat.GetField(field2) == truncate_fn(
                expected_values[i][1], lyr_defn, field2
            )
        if field3 is not None:
            assert feat.GetField(field3) == truncate_fn(
                expected_values[i][2], lyr_defn, field3
            )
        if field4 is not None:
            assert feat.GetField(field4) == truncate_fn(
                expected_values[i][3], lyr_defn, field4
            )


def CheckColumnOrder(lyr, expected_order):

    __tracebackhide__ = True

    lyr_defn = lyr.GetLayerDefn()
    for i, exp_order in enumerate(expected_order):
        assert lyr_defn.GetFieldDefn(i).GetName() == exp_order


def Check(ds, lyr, expected_order):

    CheckColumnOrder(lyr, expected_order)

    CheckFeatures(ds, lyr)

    if ds.GetDriver().GetName() == "Memory":
        return

    ds = ogr.Open(ds.GetDescription(), update=1)
    lyr_reopen = ds.GetLayer(0)

    CheckColumnOrder(lyr_reopen, expected_order)

    CheckFeatures(ds, lyr_reopen)


###############################################################################
# Test ReorderField()


def test_ogr_rfc35_2(rfc35_test_input, driver_name):

    if driver_name == "Memory":
        ds = rfc35_test_input
    else:
        ds = ogr.Open(rfc35_test_input, update=1)

    lyr = ds.GetLayer(0)

    assert lyr.TestCapability(ogr.OLCReorderFields) == 1

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, "foo3")
    feat.SetField(1, "bar3_01234")
    feat.SetField(2, "baz3_0123456789")
    feat.SetField(3, "baw3_012345678901234")
    lyr.CreateFeature(feat)
    feat = None

    assert lyr.ReorderField(1, 3) == 0
    Check(ds, lyr, ["foo5", "baz15", "baw20", "bar10"])

    lyr.ReorderField(3, 1)
    Check(ds, lyr, ["foo5", "bar10", "baz15", "baw20"])

    lyr.ReorderField(0, 2)
    Check(ds, lyr, ["bar10", "baz15", "foo5", "baw20"])

    lyr.ReorderField(2, 0)
    Check(ds, lyr, ["foo5", "bar10", "baz15", "baw20"])

    lyr.ReorderField(0, 1)
    Check(ds, lyr, ["bar10", "foo5", "baz15", "baw20"])

    lyr.ReorderField(1, 0)
    Check(ds, lyr, ["foo5", "bar10", "baz15", "baw20"])

    lyr.ReorderFields([3, 2, 1, 0])
    Check(ds, lyr, ["baw20", "baz15", "bar10", "foo5"])

    lyr.ReorderFields([3, 2, 1, 0])
    Check(ds, lyr, ["foo5", "bar10", "baz15", "baw20"])

    with gdal.quiet_errors():
        ret = lyr.ReorderFields([0, 0, 0, 0])
    assert ret != 0

    if driver_name == "Memory":
        return

    ds = None

    ds = ogr.Open(rfc35_test_input, update=1)
    lyr = ds.GetLayer(0)

    CheckColumnOrder(lyr, ["foo5", "bar10", "baz15", "baw20"])

    CheckFeatures(ds, lyr)


###############################################################################
# Test AlterFieldDefn() for change of name and width


def test_ogr_rfc35_3(rfc35_test_input, driver_name):

    if driver_name == "Memory":
        ds = rfc35_test_input
    else:
        ds = ogr.Open(rfc35_test_input, update=1)

    lyr = ds.GetLayer(0)

    fd = ogr.FieldDefn("baz25", ogr.OFTString)
    fd.SetWidth(25)

    lyr_defn = lyr.GetLayerDefn()

    with gdal.quiet_errors():
        ret = lyr.AlterFieldDefn(-1, fd, ogr.ALTER_ALL_FLAG)
    assert ret != 0

    with gdal.quiet_errors():
        ret = lyr.AlterFieldDefn(lyr_defn.GetFieldCount(), fd, ogr.ALTER_ALL_FLAG)
    assert ret != 0

    lyr.AlterFieldDefn(lyr_defn.GetFieldIndex("baz15"), fd, ogr.ALTER_ALL_FLAG)

    CheckFeatures(ds, lyr, field3="baz25")

    fd = ogr.FieldDefn("baz5", ogr.OFTString)
    fd.SetWidth(5)

    lyr_defn = lyr.GetLayerDefn()
    lyr.AlterFieldDefn(lyr_defn.GetFieldIndex("baz25"), fd, ogr.ALTER_ALL_FLAG)

    CheckFeatures(ds, lyr, field3="baz5")

    if driver_name not in ("SQLite", "Memory"):
        ds = None
        ds = ogr.Open(rfc35_test_input, update=1)

    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    fld_defn = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("baz5"))
    assert fld_defn.GetWidth() == 5

    CheckFeatures(ds, lyr, field3="baz5")

    # Change only name
    if driver_name == "SQLite":
        fd = ogr.FieldDefn("baz5_2", ogr.OFTString)
        fd.SetWidth(5)
        lyr.AlterFieldDefn(lyr_defn.GetFieldIndex("baz5"), fd, ogr.ALTER_ALL_FLAG)
        assert fld_defn.GetWidth() == 5


###############################################################################
# Test AlterFieldDefn() for change of type


def test_ogr_rfc35_4(rfc35_test_input, driver_name, tmp_path):

    if driver_name == "ESRI Shapefile":
        int_resizing_supported = True
    else:
        int_resizing_supported = False

    if driver_name == "Memory":
        ds = rfc35_test_input
    else:
        ds = ogr.Open(rfc35_test_input, update=1)

    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    assert lyr.TestCapability(ogr.OLCAlterFieldDefn) == 1

    fd = ogr.FieldDefn("intfield", ogr.OFTInteger)
    lyr.CreateField(fd)

    lyr.ReorderField(lyr_defn.GetFieldIndex("intfield"), 0)

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    feat.SetField("intfield", 12345)
    lyr.SetFeature(feat)
    feat = None

    fd.SetWidth(10)
    ret = lyr.AlterFieldDefn(lyr_defn.GetFieldIndex("intfield"), fd, ogr.ALTER_ALL_FLAG)

    if driver_name == "MapInfo File":
        assert ret != 0

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat.GetField("intfield") == 12345
    feat = None

    CheckFeatures(ds, lyr)

    if int_resizing_supported:
        fd.SetWidth(5)
        lyr.AlterFieldDefn(lyr_defn.GetFieldIndex("intfield"), fd, ogr.ALTER_ALL_FLAG)

        lyr.ResetReading()
        feat = lyr.GetNextFeature()
        assert feat.GetField("intfield") == 12345
        feat = None

        CheckFeatures(ds, lyr)

        ds = ogr.Open(rfc35_test_input, update=1)
        lyr = ds.GetLayer(0)
        lyr_defn = lyr.GetLayerDefn()

        fd.SetWidth(4)
        lyr.AlterFieldDefn(lyr_defn.GetFieldIndex("intfield"), fd, ogr.ALTER_ALL_FLAG)

        lyr.ResetReading()
        feat = lyr.GetNextFeature()
        assert feat.GetField("intfield") == 1234
        feat = None

        CheckFeatures(ds, lyr)

        ds = None

        # Check that the file size has decreased after column shrinking
        CheckFileSize(rfc35_test_input, tmp_path)

    if driver_name != "Memory":
        ds = None
        ds = ogr.Open(rfc35_test_input, update=1)
        lyr = ds.GetLayer(0)
        lyr_defn = lyr.GetLayerDefn()

    fd = ogr.FieldDefn("oldintfld", ogr.OFTString)
    fd.SetWidth(15)
    lyr.AlterFieldDefn(lyr_defn.GetFieldIndex("intfield"), fd, ogr.ALTER_ALL_FLAG)

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat.GetField("oldintfld") == "1234" if int_resizing_supported else "12345"
    feat = None

    CheckFeatures(ds, lyr)

    if driver_name != "Memory":
        ds = None
        ds = ogr.Open(rfc35_test_input, update=1)
        lyr = ds.GetLayer(0)
        lyr_defn = lyr.GetLayerDefn()

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat.GetField("oldintfld") == "1234" if int_resizing_supported else "12345"
    feat = None

    CheckFeatures(ds, lyr)

    lyr.DeleteField(lyr_defn.GetFieldIndex("oldintfld"))

    fd = ogr.FieldDefn("intfield", ogr.OFTInteger)
    fd.SetWidth(10)
    assert lyr.CreateField(fd) == 0

    assert lyr.ReorderField(lyr_defn.GetFieldIndex("intfield"), 0) == 0

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    feat.SetField("intfield", 98765)
    assert lyr.SetFeature(feat) == 0
    feat = None

    fd = ogr.FieldDefn("oldintfld", ogr.OFTString)
    fd.SetWidth(6)
    lyr.AlterFieldDefn(lyr_defn.GetFieldIndex("intfield"), fd, ogr.ALTER_ALL_FLAG)

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat.GetField("oldintfld") == "98765"
    feat = None

    CheckFeatures(ds, lyr)

    if driver_name == "Memory":
        return

    ds = None

    ds = ogr.Open(rfc35_test_input, update=1)
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat.GetField("oldintfld") == "98765"
    feat = None

    CheckFeatures(ds, lyr)


###############################################################################
# Test DeleteField()


def test_ogr_rfc35_5(rfc35_test_input, driver_name, tmp_path):

    if driver_name == "Memory":
        ds = rfc35_test_input
    else:
        ds = ogr.Open(rfc35_test_input, update=1)

    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    assert lyr.TestCapability(ogr.OLCDeleteField) == 1

    with gdal.quiet_errors():
        ret = lyr.DeleteField(-1)
    assert ret != 0

    with gdal.quiet_errors():
        ret = lyr.DeleteField(lyr.GetLayerDefn().GetFieldCount())
    assert ret != 0

    CheckFeatures(ds, lyr)

    assert lyr.DeleteField(lyr_defn.GetFieldIndex("baw20")) == 0

    if driver_name not in ("Memory", "SQLite"):
        ds = None
        # Check that the file size has decreased after column removing
        CheckFileSize(rfc35_test_input, tmp_path)

        ds = ogr.Open(rfc35_test_input, update=1)
        lyr = ds.GetLayer(0)
        lyr_defn = lyr.GetLayerDefn()

    CheckFeatures(ds, lyr, field4=None)

    assert lyr.DeleteField(lyr_defn.GetFieldIndex("baz15")) == 0

    CheckFeatures(ds, lyr, field3=None, field4=None)

    assert lyr.DeleteField(lyr_defn.GetFieldIndex("foo5")) == 0

    CheckFeatures(ds, lyr, field1=None, field3=None, field4=None)

    if driver_name != "MapInfo File":
        # MapInfo does not allow removing the last field
        assert lyr.DeleteField(lyr_defn.GetFieldIndex("bar10")) == 0

        CheckFeatures(ds, lyr, field1=None, field2=None, field3=None, field4=None)

    if driver_name == "Memory":
        return

    ds = None

    ds = ogr.Open(rfc35_test_input, update=1)
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    CheckFeatures(ds, lyr, field1=None, field2=None, field3=None, field4=None)
