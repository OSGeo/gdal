#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic GDAL open
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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
import subprocess
import sys

import gdaltest
import pytest

from osgeo import gdal


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


# Nothing exciting here. Just trying to open non existing files,
# or empty names, or files that are not valid datasets...


def matches_non_existing_error_msg(msg):
    m1 = (
        "does not exist in the file system, and is not recognized as a supported dataset name."
        in msg
    )
    m2 = "No such file or directory" in msg
    m3 = "Permission denied" in msg
    return m1 or m2 or m3


def test_basic_test_1():
    with gdal.quiet_errors():
        ds = gdal.Open("non_existing_ds", gdal.GA_ReadOnly)
    if ds is None and matches_non_existing_error_msg(gdal.GetLastErrorMsg()):
        return
    pytest.fail("did not get expected error message, got %s" % gdal.GetLastErrorMsg())


def test_basic_test_invalid_open_flag():
    with pytest.raises(Exception, match="invalid value for GDALAccess"):
        gdal.Open("data/byte.tif", "invalid")

    assert gdal.OF_RASTER not in (gdal.GA_ReadOnly, gdal.GA_Update)
    with pytest.raises(Exception, match="invalid value for GDALAccess"):
        gdal.Open("data/byte.tif", gdal.OF_RASTER)


@pytest.mark.skipif(sys.platform != "linux", reason="Incorrect platform")
def test_basic_test_strace_non_existing_file():

    python_exe = sys.executable
    cmd = 'strace -f %s -c "from osgeo import gdal; ' % python_exe + (
        "gdal.DontUseExceptions(); gdal.OpenEx('non_existing_ds', gdal.OF_RASTER)" ' " '
    )
    try:
        (_, err) = gdaltest.runexternal_out_and_err(cmd, encoding="UTF-8")
    except Exception as e:
        # strace not available
        pytest.skip(str(e))

    interesting_lines = []
    for line in err.split("\n"):
        if "non_existing_ds" in line:
            interesting_lines += [line]
    # Only 3 calls on the file are legit: open(), stat() and readlink()
    assert len(interesting_lines) <= 3, "too many system calls accessing file"


def test_basic_test_2():
    with gdal.quiet_errors():
        ds = gdal.Open("non_existing_ds", gdal.GA_Update)
    if ds is None and matches_non_existing_error_msg(gdal.GetLastErrorMsg()):
        return
    pytest.fail("did not get expected error message, got %s" % gdal.GetLastErrorMsg())


def test_basic_test_3():
    with gdal.quiet_errors():
        ds = gdal.Open("", gdal.GA_ReadOnly)
    if ds is None and matches_non_existing_error_msg(gdal.GetLastErrorMsg()):
        return
    pytest.fail("did not get expected error message, got %s" % gdal.GetLastErrorMsg())


def test_basic_test_4():
    with gdal.quiet_errors():
        ds = gdal.Open("", gdal.GA_Update)
    if ds is None and matches_non_existing_error_msg(gdal.GetLastErrorMsg()):
        return
    pytest.fail("did not get expected error message, got %s" % gdal.GetLastErrorMsg())


def test_basic_test_5():
    with gdal.quiet_errors():
        ds = gdal.Open("data/doctype.xml", gdal.GA_ReadOnly)
    last_error = gdal.GetLastErrorMsg()
    expected = "`data/doctype.xml' not recognized as being in a supported file format"
    assert ds is not None or expected in last_error


def test_basic_test_5bis():
    with pytest.raises(RuntimeError, match="not a string"):
        gdal.Open(12345)


###############################################################################
# Issue several AllRegister() to check that GDAL drivers are good citizens


def test_basic_test_6():
    gdal.AllRegister()
    gdal.AllRegister()
    gdal.AllRegister()


###############################################################################
# Test fix for #3077 (check that errors are cleared when using UseExceptions())


def basic_test_7_internal():

    with pytest.raises(Exception):
        gdal.Open("non_existing_ds", gdal.GA_ReadOnly)

    # Special case: we should still be able to get the error message
    # until we call a new GDAL function
    assert matches_non_existing_error_msg(gdal.GetLastErrorMsg()), (
        "did not get expected error message, got %s" % gdal.GetLastErrorMsg()
    )

    # Special case: we should still be able to get the error message
    # until we call a new GDAL function
    assert matches_non_existing_error_msg(gdal.GetLastErrorMsg()), (
        "did not get expected error message, got %s" % gdal.GetLastErrorMsg()
    )

    assert gdal.GetLastErrorType() != 0, "did not get expected error type"

    # Should issue an implicit CPLErrorReset()
    gdal.GetCacheMax()

    assert gdal.GetLastErrorType() == 0, "got unexpected error type"


###############################################################################
# Test gdal.VersionInfo('RELEASE_DATE') and gdal.VersionInfo('LICENSE')


def test_basic_test_8():

    ret = gdal.VersionInfo("RELEASE_DATE")
    assert len(ret) == 8

    python_exe = sys.executable
    if sys.platform == "win32":
        python_exe = python_exe.replace("\\", "/")

    license_text = gdal.VersionInfo("LICENSE")
    assert (
        license_text.startswith("GDAL/OGR is released under the MIT license")
        or "GDAL/OGR Licensing" in license_text
    )

    # Use a subprocess to avoid the cached license text
    env = os.environ.copy()
    env["GDAL_DATA"] = "tmp"
    with open("tmp/LICENSE.TXT", "wt") as f:
        f.write("fake_license")
    license_text = subprocess.check_output(
        [sys.executable, "basic_test_subprocess.py"], env=env
    ).decode("utf-8")
    os.unlink("tmp/LICENSE.TXT")
    assert license_text.startswith("fake_license")


###############################################################################
# Test gdal.PushErrorHandler() with a Python error handler


def my_python_error_handler(eErrClass, err_no, msg):
    gdaltest.eErrClass = eErrClass
    gdaltest.err_no = err_no
    gdaltest.msg = msg


def test_basic_test_9():

    gdaltest.eErrClass = 0
    gdaltest.err_no = 0
    gdaltest.msg = ""
    gdal.PushErrorHandler(my_python_error_handler)
    gdal.Error(1, 2, "test")
    gdal.PopErrorHandler()

    assert gdaltest.eErrClass == 1

    assert gdaltest.err_no == 2

    assert gdaltest.msg == "test"


###############################################################################
# Test gdal.PushErrorHandler() with a Python error handler as a method (#5186)


class my_python_error_handler_class(object):
    def __init__(self):
        self.eErrClass = None
        self.err_no = None
        self.msg = None

    def handler(self, eErrClass, err_no, msg):
        self.eErrClass = eErrClass
        self.err_no = err_no
        self.msg = msg


def test_basic_test_10():

    # Check that reference counting works OK
    gdal.PushErrorHandler(my_python_error_handler_class().handler)
    gdal.Error(1, 2, "test")
    gdal.PopErrorHandler()

    error_handler = my_python_error_handler_class()
    gdal.PushErrorHandler(error_handler.handler)
    gdal.Error(1, 2, "test")
    gdal.PopErrorHandler()

    assert error_handler.eErrClass == 1

    assert error_handler.err_no == 2

    assert error_handler.msg == "test"


###############################################################################
# Test gdal.OpenEx()


def test_basic_test_11():

    ds = gdal.OpenEx("data/byte.tif")
    assert ds is not None

    ds = gdal.OpenEx("data/byte.tif", gdal.OF_RASTER)
    assert ds is not None

    ds = gdal.OpenEx("data/byte.tif", gdal.OF_VECTOR)
    assert ds is None

    ds = gdal.OpenEx("data/byte.tif", gdal.OF_RASTER | gdal.OF_VECTOR)
    assert ds is not None

    ds = gdal.OpenEx("data/byte.tif", gdal.OF_ALL)
    assert ds is not None

    ds = gdal.OpenEx("data/byte.tif", gdal.OF_UPDATE)
    assert ds is not None

    ds = gdal.OpenEx(
        "data/byte.tif",
        gdal.OF_RASTER | gdal.OF_VECTOR | gdal.OF_UPDATE | gdal.OF_VERBOSE_ERROR,
    )
    assert ds is not None

    ds = gdal.OpenEx("data/byte.tif", allowed_drivers=[])
    assert ds is not None

    ds = gdal.OpenEx("data/byte.tif", allowed_drivers=["GTiff"])
    assert ds is not None

    ds = gdal.OpenEx("data/byte.tif", allowed_drivers=["PNG"])
    assert ds is None

    with gdal.quiet_errors():
        ds = gdal.OpenEx("data/byte.tif", open_options=["FOO"])
    assert ds is not None

    ar_ds = [gdal.OpenEx("data/byte.tif", gdal.OF_SHARED) for _ in range(1024)]
    assert ar_ds[1023] is not None
    ar_ds = None

    ds = gdal.OpenEx("../ogr/data/poly.shp", gdal.OF_RASTER)
    assert ds is None

    ds = gdal.OpenEx("../ogr/data/poly.shp", gdal.OF_VECTOR)
    assert ds is not None
    assert ds.GetLayerCount() == 1
    assert ds.GetLayer(0) is not None
    ds.GetLayer(0).GetMetadata()

    ds = gdal.OpenEx("../ogr/data/poly.shp", allowed_drivers=["ESRI Shapefile"])
    assert ds is not None

    ds = gdal.OpenEx("../ogr/data/poly.shp", gdal.OF_RASTER | gdal.OF_VECTOR)
    assert ds is not None

    ds = gdal.OpenEx("non existing")
    assert ds is None and gdal.GetLastErrorMsg() == ""

    with gdal.quiet_errors():
        ds = gdal.OpenEx("non existing", gdal.OF_VERBOSE_ERROR)
    assert ds is None and gdal.GetLastErrorMsg() != ""

    with gdal.ExceptionMgr(useExceptions=True):
        assert gdal.GetUseExceptions()
        with pytest.raises(Exception):
            gdal.OpenEx("non existing")

    try:
        with gdal.ExceptionMgr(useExceptions=True):
            try:
                gdal.OpenEx("non existing")
            except Exception:
                pass
    except Exception:
        pytest.fails("Exception thrown whereas it should not have")

    with gdal.ExceptionMgr(useExceptions=True):
        try:
            gdal.OpenEx("non existing")
        except Exception:
            pass
        gdal.Open("data/byte.tif")


###############################################################################
# Test GDAL layer API


def test_basic_test_12():

    ds = gdal.GetDriverByName("MEMORY").Create("bar", 0, 0, 0)
    assert ds.GetDescription() == "bar"
    lyr = ds.CreateLayer("foo")
    assert lyr is not None
    assert lyr.GetDescription() == "foo"
    from osgeo import ogr

    assert lyr.TestCapability(ogr.OLCCreateField) == 1
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayerByName("foo")
    assert lyr is not None
    lyr = ds.GetLayerByIndex(0)
    assert lyr is not None
    lyr = ds.GetLayer(0)
    assert lyr is not None
    assert not ds.IsLayerPrivate(0)
    sql_lyr = ds.ExecuteSQL("SELECT * FROM foo")
    assert sql_lyr is not None
    ds.ReleaseResultSet(sql_lyr)
    new_lyr = ds.CopyLayer(lyr, "bar")
    assert new_lyr is not None
    assert ds.DeleteLayer(0) == 0
    assert ds.DeleteLayer("bar") == 0
    ds.SetStyleTable(ds.GetStyleTable())
    ds = None


###############################################################################
# Test correct sorting of StringList / metadata (#5540, #5557)


def test_basic_test_13():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    for i in range(3):
        if i == 0:
            ds.SetMetadataItem("ScaleBounds", "True")
            ds.SetMetadataItem("ScaleBounds.MinScale", "0")
            ds.SetMetadataItem("ScaleBounds.MaxScale", "2000000")
        elif i == 1:
            ds.SetMetadataItem("ScaleBounds.MaxScale", "2000000")
            ds.SetMetadataItem("ScaleBounds.MinScale", "0")
            ds.SetMetadataItem("ScaleBounds", "True")
        else:
            ds.SetMetadataItem("ScaleBounds.MinScale", "0")
            ds.SetMetadataItem("ScaleBounds", "True")
            ds.SetMetadataItem("ScaleBounds.MaxScale", "2000000")

        assert ds.GetMetadataItem("scalebounds") == "True"
        assert ds.GetMetadataItem("ScaleBounds") == "True"
        assert ds.GetMetadataItem("SCALEBOUNDS") == "True"
        assert ds.GetMetadataItem("ScaleBounds.MinScale") == "0"
        assert ds.GetMetadataItem("ScaleBounds.MaxScale") == "2000000"
    ds = None

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    for i in range(200):
        ds.SetMetadataItem("FILENAME_%d" % i, "%d" % i)
    for i in range(200):
        assert ds.GetMetadataItem("FILENAME_%d" % i) == "%d" % i


###############################################################################
# Test SetMetadata()


def test_basic_test_14():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)

    ds.SetMetadata("foo")
    assert ds.GetMetadata_List() == ["foo"]

    with pytest.raises(Exception):
        ds.SetMetadata(5)

    ds.SetMetadata(["foo=bar"])
    assert ds.GetMetadata_List() == ["foo=bar"]

    with gdal.quiet_errors():
        with pytest.raises(Exception):
            ds.SetMetadata([5])

    ds.SetMetadata({"foo": "baz"})
    assert ds.GetMetadata_List() == ["foo=baz"]

    ds.SetMetadata({"foo": b"baz"})
    assert ds.GetMetadata_List() == ["foo=baz"]

    with gdal.quiet_errors():
        with pytest.raises(Exception):
            ds.SetMetadata({"foo": b"zero_byte_in_string\0"})

    ds.SetMetadata({"foo": 5})
    assert ds.GetMetadata_List() == ["foo=5"]

    ds.SetMetadata({5: "baz"})
    assert ds.GetMetadata_List() == ["5=baz"]

    ds.SetMetadata({5: 6})
    assert ds.GetMetadata_List() == ["5=6"]

    val = "\u00e9ven"

    ds.SetMetadata({"bar": val})
    assert ds.GetMetadata()["bar"] == val

    ds.SetMetadata({val: "baz"})
    assert ds.GetMetadata()[val] == "baz"

    ds.SetMetadata({val: 5})
    assert ds.GetMetadata_List() == ["\u00e9ven=5"]

    ds.SetMetadata({5: val})
    assert ds.GetMetadata_List() == ["5=\u00e9ven"]

    class ClassWithoutStrRepr:
        def __init__(self):
            pass

        def __str__(self):
            raise Exception("no string representation")

    with pytest.raises(Exception):
        ds.SetMetadata({"a": ClassWithoutStrRepr()})

    with pytest.raises(Exception):
        ds.SetMetadata({ClassWithoutStrRepr(): "a"})

    ds.SetMetadata([b"foo=\xE8\x03"])
    assert ds.GetMetadata_List() == [b"foo=\xe8\x03"]


###############################################################################
# Test errors with progress callback


def basic_test_15_cbk_no_argument():
    return None


def basic_test_15_cbk_no_ret(a, b, c):
    # pylint: disable=unused-argument
    return None


def basic_test_15_cbk_bad_ret(a, b, c):
    # pylint: disable=unused-argument
    return "ok"


def test_basic_test_15():
    mem_driver = gdal.GetDriverByName("MEM")

    with pytest.raises(Exception):
        with gdal.quiet_errors():
            gdal.GetDriverByName("MEM").CreateCopy(
                "", gdal.GetDriverByName("MEM").Create("", 1, 1), callback="foo"
            )

    with gdal.quiet_errors():
        ds = mem_driver.CreateCopy(
            "", mem_driver.Create("", 1, 1), callback=basic_test_15_cbk_no_argument
        )
    assert ds is None

    with gdal.quiet_errors():
        ds = mem_driver.CreateCopy(
            "", mem_driver.Create("", 1, 1), callback=basic_test_15_cbk_no_ret
        )
    assert ds is not None

    with gdal.quiet_errors():
        ds = mem_driver.CreateCopy(
            "", mem_driver.Create("", 1, 1), callback=basic_test_15_cbk_bad_ret
        )
    assert ds is None


###############################################################################
# Test unrecognized and recognized open options prefixed by @


def test_basic_test_16():

    gdal.ErrorReset()
    gdal.OpenEx("data/byte.tif", open_options=["@UNRECOGNIZED=FOO"])
    assert gdal.GetLastErrorMsg() == ""

    gdal.ErrorReset()
    gdal.Translate("/vsimem/temp.tif", "data/byte.tif", options="-co BLOCKYSIZE=10")
    with gdal.quiet_errors():
        gdal.OpenEx(
            "/vsimem/temp.tif", gdal.OF_UPDATE, open_options=["@NUM_THREADS=INVALID"]
        )
    gdal.Unlink("/vsimem/temp.tif")
    assert "Invalid value for NUM_THREADS: INVALID" in gdal.GetLastErrorMsg()


def test_basic_dict_open_options():

    ds1 = gdal.Open("data/byte.tif")

    ds2 = gdal.OpenEx("data/byte.tif", open_options={"GEOREF_SOURCES": "TABFILE"})

    assert ds1.GetGeoTransform() != ds2.GetGeoTransform()


@pytest.mark.parametrize(
    "create_tfw", (True, False, "TRUE", "FALSE", "YES", "NO", "ON", "OFF")
)
def test_basic_dict_create_options(tmp_vsimem, create_tfw):

    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "test_basic_dict_create_options.tif",
        1,
        1,
        options={"TFW": create_tfw},
    ) as ds:
        gt = (0.0, 5.0, 0.0, 5.0, 0.0, -5.0)
        ds.SetGeoTransform(gt)

    if create_tfw in (True, "TRUE", "YES", "ON"):
        assert (
            gdal.VSIStatL(tmp_vsimem / "test_basic_dict_create_options.tfw") is not None
        )
    else:
        assert gdal.VSIStatL(tmp_vsimem / "test_basic_dict_create_options.tfw") is None


@pytest.mark.parametrize("create_tfw", (True, False))
def test_basic_dict_create_copy_options(tmp_vsimem, create_tfw):

    src_ds = gdal.Open("data/byte.tif")

    with gdal.GetDriverByName("GTiff").CreateCopy(
        tmp_vsimem / "test_basic_dict_create_copy_options.tif",
        src_ds,
        options={"TFW": create_tfw},
    ) as ds:
        gt = (0.0, 5.0, 0.0, 5.0, 0.0, -5.0)
        ds.SetGeoTransform(gt)

    if create_tfw:
        assert (
            gdal.VSIStatL(tmp_vsimem / "test_basic_dict_create_copy_options.tfw")
            is not None
        )
    else:
        assert (
            gdal.VSIStatL(tmp_vsimem / "test_basic_dict_create_copy_options.tfw")
            is None
        )


def test_gdal_getspatialref():

    ds = gdal.Open("data/byte.tif")
    assert ds.GetSpatialRef() is not None

    ds = gdal.Open("data/minfloat.tif")
    assert ds.GetSpatialRef() is None


def test_gdal_setspatialref():

    ds = gdal.Open("data/byte.tif")
    sr = ds.GetSpatialRef()
    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    assert ds.SetSpatialRef(sr) == gdal.CE_None
    sr_got = ds.GetSpatialRef()
    assert sr_got
    assert sr_got.IsSame(sr)


def test_gdal_getgcpspatialref():

    ds = gdal.Open("data/byte.tif")
    assert ds.GetGCPSpatialRef() is None

    ds = gdal.Open("data/byte_gcp.tif")
    assert ds.GetGCPSpatialRef() is not None


def test_gdal_setgcpspatialref():

    ds = gdal.Open("data/byte.tif")
    sr = ds.GetSpatialRef()
    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    gcp = gdal.GCP()
    gcp.GCPPixel = 0
    gcp.GCPLine = 0
    gcp.GCPX = 440720.000
    gcp.GCPY = 3751320.000
    ds.SetGCPs([gcp], sr)
    sr_got = ds.GetGCPSpatialRef()
    assert sr_got
    assert sr_got.IsSame(sr)


def test_gdal_getdatatypename():

    assert gdal.GetDataTypeName(gdal.GDT_Byte) == "Byte"
    with pytest.raises(Exception):
        gdal.GetDataTypeName(-1)
    with pytest.raises(Exception):
        gdal.GetDataTypeName(100)
    with pytest.raises(Exception):
        gdal.GetDataTypeName("invalid")


def test_gdal_EscapeString():

    assert gdal.EscapeString("", scheme=gdal.CPLES_XML) == ""

    assert gdal.EscapeString(b"", scheme=gdal.CPLES_XML) == b""

    assert gdal.EscapeString("&", gdal.CPLES_XML) == "&amp;"

    assert gdal.EscapeString("<", gdal.CPLES_XML) == "&lt;"

    assert gdal.EscapeString(">", gdal.CPLES_XML) == "&gt;"

    assert gdal.EscapeString('"', gdal.CPLES_XML) == "&quot;"

    assert gdal.EscapeString(b"\xEF\xBB\xBF", gdal.CPLES_XML) == b"&#xFEFF;"

    assert gdal.EscapeString("\t", gdal.CPLES_XML) == "\t"

    assert gdal.EscapeString("\n", gdal.CPLES_XML) == "\n"

    assert gdal.EscapeString(b"\x01a", gdal.CPLES_XML) == b"a"

    assert gdal.EscapeString("", gdal.CPLES_XML_BUT_QUOTES) == ""

    assert gdal.EscapeString("&", gdal.CPLES_XML_BUT_QUOTES) == "&amp;"

    assert gdal.EscapeString("<", gdal.CPLES_XML_BUT_QUOTES) == "&lt;"

    assert gdal.EscapeString(">", gdal.CPLES_XML_BUT_QUOTES) == "&gt;"

    assert gdal.EscapeString('"', gdal.CPLES_XML_BUT_QUOTES) == '"'

    assert gdal.EscapeString(b"\xEF\xBB\xBF", gdal.CPLES_XML_BUT_QUOTES) == b"&#xFEFF;"

    assert gdal.EscapeString("\t", gdal.CPLES_XML_BUT_QUOTES) == "\t"

    assert gdal.EscapeString("\n", gdal.CPLES_XML_BUT_QUOTES) == "\n"

    assert gdal.EscapeString(b"\x01a", gdal.CPLES_XML_BUT_QUOTES) == b"a"

    assert gdal.EscapeString("", gdal.CPLES_BackslashQuotable) == ""

    assert gdal.EscapeString("a", gdal.CPLES_BackslashQuotable) == "a"

    assert gdal.EscapeString(b"\x00x", gdal.CPLES_BackslashQuotable) == b"\\0x"

    assert gdal.EscapeString(b"\x01", gdal.CPLES_BackslashQuotable) == b"\x01"

    assert gdal.EscapeString("\\", gdal.CPLES_BackslashQuotable) == "\\\\"

    assert gdal.EscapeString("\n", gdal.CPLES_BackslashQuotable) == "\\n"

    assert gdal.EscapeString('"', gdal.CPLES_BackslashQuotable) == '\\"'

    assert gdal.EscapeString("", gdal.CPLES_URL) == ""

    assert (
        gdal.EscapeString("aZAZ09$-_.+!*'(), ", gdal.CPLES_URL)
        == "aZAZ09$-_.+!*'(),%20"
    )

    assert gdal.EscapeString("", gdal.CPLES_SQL) == ""

    assert gdal.EscapeString("a", gdal.CPLES_SQL) == "a"

    assert gdal.EscapeString("a'a", gdal.CPLES_SQL) == "a''a"

    assert gdal.EscapeString("", gdal.CPLES_CSV) == ""

    assert gdal.EscapeString("a'b", gdal.CPLES_CSV) == "a'b"

    assert gdal.EscapeString('a"b', gdal.CPLES_CSV) == '"a""b"'

    assert gdal.EscapeString("a,b", gdal.CPLES_CSV) == '"a,b"'

    assert gdal.EscapeString("a,b", gdal.CPLES_CSV) == '"a,b"'

    assert gdal.EscapeString("a\tb", gdal.CPLES_CSV) == '"a\tb"'

    assert gdal.EscapeString("a\nb", gdal.CPLES_CSV) == '"a\nb"'

    assert gdal.EscapeString("a\rb", gdal.CPLES_CSV) == '"a\rb"'


def test_gdal_EscapeString_errors():

    if sys.maxsize > 2**32:
        pytest.skip("Test not available on 64 bit")

    try:
        # Allocation will be < 4 GB, but will fail being > 2 GB
        assert gdal.EscapeString(b'"' * (((1 << 32) - 1) // 6), gdal.CPLES_XML) is None

        # Allocation will be > 4 GB
        assert (
            gdal.EscapeString(b'"' * (((1 << 32) - 1) // 6 + 1), gdal.CPLES_XML) is None
        )
    except MemoryError:
        print("Got MemoryError")


def test_gdal_DataTypeUnion():

    assert gdal.DataTypeUnion(gdal.GDT_Byte, gdal.GDT_UInt16) == gdal.GDT_UInt16


def test_exceptionmanager():
    currentExceptionsFlag = gdal.GetUseExceptions()
    usingExceptions = currentExceptionsFlag == 1

    # Run in context with opposite state
    with gdal.ExceptionMgr(useExceptions=(not usingExceptions)):
        assert gdal.GetUseExceptions() != currentExceptionsFlag

    # Check we are back to original state
    assert gdal.GetUseExceptions() == currentExceptionsFlag


def test_quiet_errors():
    with gdal.ExceptionMgr(useExceptions=False), gdal.quiet_errors():
        gdal.Error(gdal.CE_Failure, gdal.CPLE_AppDefined, "you will never see me")


def test_basic_test_UseExceptions():

    python_exe = sys.executable
    cmd = '%s -c "from osgeo import gdal;' % python_exe + (
        "gdal.UseExceptions();" "gdal.Open('non_existing.tif');" ' " '
    )
    try:
        (_, err) = gdaltest.runexternal_out_and_err(cmd, encoding="UTF-8")
    except Exception as e:
        pytest.skip("got exception %s" % str(e))
    assert "RuntimeError: " in err
    assert "FutureWarning: Neither gdal.UseExceptions()" not in err
    assert "FutureWarning: Neither ogr.UseExceptions()" not in err


def test_basic_test_UseExceptions_ogr_open():

    python_exe = sys.executable
    cmd = '%s -c "from osgeo import gdal, ogr;' % python_exe + (
        "gdal.UseExceptions();" "ogr.Open('non_existing.tif');" ' " '
    )
    try:
        (_, err) = gdaltest.runexternal_out_and_err(cmd, encoding="UTF-8")
    except Exception as e:
        pytest.skip("got exception %s" % str(e))
    assert "RuntimeError: " in err
    assert "FutureWarning: Neither gdal.UseExceptions()" not in err
    assert "FutureWarning: Neither ogr.UseExceptions()" not in err


def test_basic_test_DontUseExceptions():

    python_exe = sys.executable
    cmd = '%s -c "from osgeo import gdal;' % python_exe + (
        "gdal.DontUseExceptions();" "gdal.Open('non_existing.tif');" ' " '
    )
    try:
        (_, err) = gdaltest.runexternal_out_and_err(cmd, encoding="UTF-8")
    except Exception as e:
        pytest.skip("got exception %s" % str(e))
    assert "ERROR " in err
    assert "FutureWarning: Neither gdal.UseExceptions()" not in err
    assert "FutureWarning: Neither ogr.UseExceptions()" not in err


def test_create_context_manager(tmp_path):
    fname = tmp_path / "out.tif"

    drv = gdal.GetDriverByName("GTiff")
    with drv.Create(fname, xsize=10, ysize=10, bands=1, eType=gdal.GDT_Float32) as ds:
        ds.GetRasterBand(1).Fill(100)

    # Make sure we don't crash when accessing ds after it has been closed
    with pytest.raises(Exception):
        ds.GetRasterBand(1).ReadRaster()

    ds_in = gdal.Open(fname)
    assert ds_in.GetRasterBand(1).Checksum() != 0


def test_dataset_use_after_close_1():
    ds = gdal.Open("data/byte.tif")
    assert ds is not None

    ds.Close()

    with pytest.raises(Exception):
        ds.GetRasterBand(1)


def test_dataset_use_after_close_2():
    ds = gdal.Open("data/byte.tif")
    assert ds is not None

    ds2 = ds

    ds2.Close()

    with pytest.raises(Exception):
        ds.GetRasterBand(1)

    with pytest.raises(Exception):
        ds2.GetRasterBand(1)


def test_band_use_after_dataset_close_1():
    ds = gdal.Open("data/byte.tif")
    band = ds.GetRasterBand(1)
    del ds

    # Make sure "del ds" has invalidated "band" so we don't crash here
    with pytest.raises(Exception):
        band.Checksum()


def test_band_use_after_dataset_close_2():
    with gdal.Open("data/byte.tif") as ds:
        band = ds.GetRasterBand(1)

    # Make sure ds.__exit__() has invalidated "band" so we don't crash here
    with pytest.raises(Exception):
        band.Checksum()


def test_layer_use_after_dataset_close_1():
    with gdal.OpenEx("../ogr/data/poly.shp") as ds:
        lyr = ds.GetLayer(0)

    # Make sure ds.__exit__() has invalidated "lyr" so we don't crash here
    with pytest.raises(Exception):
        lyr.GetFeatureCount()


def test_layer_use_after_dataset_close_2():
    ds = gdal.OpenEx("../ogr/data/poly.shp")
    lyr = ds.GetLayerByName("poly")

    del ds
    # Make sure ds.__del__() has invalidated "lyr" so we don't crash here
    with pytest.raises(Exception):
        lyr.GetFeatureCount()


def test_mask_band_use_after_dataset_close():
    with gdal.Open("data/byte.tif") as ds:
        m1 = ds.GetRasterBand(1).GetMaskBand()
        m2 = m1.GetMaskBand()

    # Make sure ds.__exit__() invalidation has propagated to mask bands
    with pytest.raises(Exception):
        m1.Checksum()

    with pytest.raises(Exception):
        m2.Checksum()


def test_ovr_band_use_after_dataset_close():
    with gdal.Open("data/byte_with_ovr.tif") as ds:
        ovr = ds.GetRasterBand(1).GetOverview(1)

    # Make sure ds.__exit__() invalidation has propagated to overviews

    with pytest.raises(Exception):
        ovr.Checksum()


@pytest.mark.slow()
def test_checksum_more_than_2billion_pixels():

    filename = "/vsimem/test_checksum_more_than_2billion_pixels.tif"
    ds = gdal.GetDriverByName("GTiff").Create(
        filename,
        50000,
        50000,
        options=["SPARSE_OK=YES"],
    )
    ds.GetRasterBand(1).SetNoDataValue(1)
    assert ds.GetRasterBand(1).Checksum() == 63744
    ds = None
    gdal.Unlink(filename)


def test_tmp_vsimem(tmp_vsimem):
    assert isinstance(tmp_vsimem, os.PathLike)

    assert gdal.VSIStatL(tmp_vsimem) is not None


def test_band_iter():

    ds = gdal.Open("data/rgba.tif")

    assert len(ds) == 4

    bands = []

    for band in ds:
        bands.append(band)

    assert len(bands) == 4


def test_band_getitem():

    ds = gdal.Open("data/rgba.tif")

    assert ds[2].this == ds.GetRasterBand(2).this

    with pytest.raises(IndexError):
        ds[0]

    with pytest.raises(IndexError):
        ds[5]
