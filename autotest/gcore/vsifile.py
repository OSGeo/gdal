#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test VSI file primitives
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
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
import sys
import time

import gdaltest
import pytest
from lxml import etree

from osgeo import gdal


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################
# Generic test


def vsifile_generic(filename, options=[]):

    start_time = time.time()

    fp = gdal.VSIFOpenExL(filename, "wb+", False, options)
    assert fp is not None

    assert gdal.VSIFWriteL("0123456789", 1, 10, fp) == 10

    assert gdal.VSIFFlushL(fp) == 0

    assert gdal.VSIFTruncateL(fp, 20) == 0

    assert gdal.VSIFTellL(fp) == 10

    assert gdal.VSIFTruncateL(fp, 5) == 0

    assert gdal.VSIFTellL(fp) == 10

    assert gdal.VSIFSeekL(fp, 0, 2) == 0

    assert gdal.VSIFTellL(fp) == 5

    gdal.VSIFWriteL("XX", 1, 2, fp)
    gdal.VSIFCloseL(fp)

    statBuf = gdal.VSIStatL(
        filename,
        gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG,
    )
    assert statBuf.size == 7
    assert start_time == pytest.approx(statBuf.mtime, abs=2)

    fp = gdal.VSIFOpenExL(filename, "rb", False, options)
    assert gdal.VSIFReadL(1, 0, fp) is None
    assert gdal.VSIFReadL(0, 1, fp) is None
    buf = gdal.VSIFReadL(1, 7, fp)
    assert gdal.VSIFWriteL("a", 1, 1, fp) == 0
    assert gdal.VSIFTruncateL(fp, 0) != 0
    gdal.VSIFCloseL(fp)

    assert buf.decode("ascii") == "01234XX"

    # Test append mode on existing file
    fp = gdal.VSIFOpenExL(filename, "ab", False, options)
    gdal.VSIFWriteL("XX", 1, 2, fp)
    gdal.VSIFCloseL(fp)

    statBuf = gdal.VSIStatL(
        filename,
        gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG,
    )
    assert statBuf.size == 9

    assert gdal.Unlink(filename) == 0

    statBuf = gdal.VSIStatL(filename, gdal.VSI_STAT_EXISTS_FLAG)
    assert statBuf is None

    # Test append mode on non existing file
    fp = gdal.VSIFOpenExL(filename, "ab", False, options)
    gdal.VSIFWriteL("XX", 1, 2, fp)
    gdal.VSIFCloseL(fp)

    statBuf = gdal.VSIStatL(
        filename,
        gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG,
    )
    assert statBuf.size == 2

    assert gdal.Unlink(filename) == 0


###############################################################################
# Test /vsimem


def test_vsifile_1():
    vsifile_generic("/vsimem/vsifile_1.bin")


###############################################################################
# Test regular file system


def test_vsifile_2():
    vsifile_generic("tmp/vsifile_2.bin")


###############################################################################
# Test Windows specific WRITE_THROUGH=YES


@pytest.mark.skipif(sys.platform != "win32", reason="Windows specific test")
def test_vsifile_WRITE_THROUGH():
    vsifile_generic("tmp/vsifile_WRITE_THROUGH.bin", ["WRITE_THROUGH=YES"])


###############################################################################
# Test ftruncate >= 32 bit


def test_vsifile_3():

    if not gdaltest.filesystem_supports_sparse_files("tmp"):
        pytest.skip()

    filename = "tmp/vsifile_3"

    fp = gdal.VSIFOpenL(filename, "wb+")
    gdal.VSIFTruncateL(fp, 10 * 1024 * 1024 * 1024)
    gdal.VSIFSeekL(fp, 0, 2)
    pos = gdal.VSIFTellL(fp)
    if pos != 10 * 1024 * 1024 * 1024:
        gdal.VSIFCloseL(fp)
        gdal.Unlink(filename)
        pytest.fail(pos)
    gdal.VSIFSeekL(fp, 0, 0)
    gdal.VSIFSeekL(fp, pos, 0)
    pos = gdal.VSIFTellL(fp)
    if pos != 10 * 1024 * 1024 * 1024:
        gdal.VSIFCloseL(fp)
        gdal.Unlink(filename)
        pytest.fail(pos)

    gdal.VSIFCloseL(fp)

    statBuf = gdal.VSIStatL(
        filename,
        gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG,
    )
    gdal.Unlink(filename)

    assert statBuf.size == 10 * 1024 * 1024 * 1024


###############################################################################
# Test fix for #4583 (short reads)


def test_vsifile_4():

    fp = gdal.VSIFOpenL("vsifile.py", "rb")
    data = gdal.VSIFReadL(1000000, 1, fp)
    # print(len(data))
    gdal.VSIFSeekL(fp, 0, 0)
    data = gdal.VSIFReadL(1, 1000000, fp)
    assert data
    gdal.VSIFCloseL(fp)


###############################################################################
# Test vsicache


@pytest.mark.parametrize("cache_size", ("0", "65536", None))
def test_vsifile_5(cache_size):

    fp = gdal.VSIFOpenL("tmp/vsifile_5.bin", "wb")
    ref_data = "".join(["%08X" % i for i in range(5 * 32768)])
    gdal.VSIFWriteL(ref_data, 1, len(ref_data), fp)
    gdal.VSIFCloseL(fp)

    with gdal.config_options({"VSI_CACHE": "YES", "VSI_CACHE_SIZE": cache_size}):
        fp = gdal.VSIFOpenL("tmp/vsifile_5.bin", "rb")

        gdal.VSIFSeekL(fp, 50000, 0)
        if gdal.VSIFTellL(fp) != 50000:
            pytest.fail()

        gdal.VSIFSeekL(fp, 50000, 1)
        if gdal.VSIFTellL(fp) != 100000:
            pytest.fail()

        gdal.VSIFSeekL(fp, 0, 2)
        if gdal.VSIFTellL(fp) != 5 * 32768 * 8:
            pytest.fail()
        gdal.VSIFReadL(1, 1, fp)

        gdal.VSIFSeekL(fp, 0, 0)
        data = gdal.VSIFReadL(1, 3 * 32768, fp)
        if data.decode("ascii") != ref_data[0 : 3 * 32768]:
            pytest.fail()

        gdal.VSIFSeekL(fp, 16384, 0)
        data = gdal.VSIFReadL(1, 5 * 32768, fp)
        if data.decode("ascii") != ref_data[16384 : 16384 + 5 * 32768]:
            pytest.fail()

        data = gdal.VSIFReadL(1, 50 * 32768, fp)
        if data[0:1130496].decode("ascii") != ref_data[16384 + 5 * 32768 :]:
            pytest.fail()

        gdal.VSIFCloseL(fp)

    gdal.Unlink("tmp/vsifile_5.bin")


###############################################################################
# Test vsicache an read errors (https://github.com/qgis/QGIS/issues/45293)


def test_vsifile_vsicache_read_error():

    tmpfilename = "tmp/test_vsifile_vsicache_read_error.bin"
    f = gdal.VSIFOpenL(tmpfilename, "wb")
    assert f
    try:
        gdal.VSIFTruncateL(f, 1000 * 1000)

        with gdaltest.config_option("VSI_CACHE", "YES"):
            f2 = gdal.VSIFOpenL(tmpfilename, "rb")
        assert f2
        try:
            gdal.VSIFSeekL(f2, 500 * 1000, 0)

            # Truncate the file to simulate a read error
            gdal.VSIFTruncateL(f, 0)

            assert len(gdal.VSIFReadL(1, 5000 * 1000, f2)) == 0

            # Extend the file again
            gdal.VSIFTruncateL(f, 1000 * 1000)

            # Read again
            # Note: reading after truncating / extending seems to not play
            # very well with FILE* and depends on the libc implementation,
            # in particular musl seems to behave differently from glibc and BSDs
            gdal.VSIFSeekL(f2, 500 * 1000, 0)
            # just test we don't crash
            gdal.VSIFReadL(1, 50 * 1000, f2)
            # assert len(gdal.VSIFReadL(1, 50 * 1000, f2)) == 50 * 1000

            # Truncate the file to simulate a read error
            gdal.VSIFTruncateL(f, 10)

            CHUNK_SIZE = 32768

            gdal.VSIFSeekL(f2, 0, 0)
            assert len(gdal.VSIFReadL(1, CHUNK_SIZE, f2)) == 10

            gdal.VSIFSeekL(f2, 100, 0)
            assert len(gdal.VSIFReadL(1, CHUNK_SIZE, f2)) == 0

        finally:
            gdal.VSIFCloseL(f2)
    finally:
        gdal.VSIFCloseL(f)
        gdal.Unlink(tmpfilename)


###############################################################################
# Test vsicache above 2 GB


def test_vsifile_6():

    if not gdaltest.filesystem_supports_sparse_files("tmp"):
        pytest.skip()

    offset = 4 * 1024 * 1024 * 1024

    ref_data = "abcd".encode("ascii")
    fp = gdal.VSIFOpenL("tmp/vsifile_6.bin", "wb")
    gdal.VSIFSeekL(fp, offset, 0)
    gdal.VSIFWriteL(ref_data, 1, len(ref_data), fp)
    gdal.VSIFCloseL(fp)

    # Sanity check without VSI_CACHE
    fp = gdal.VSIFOpenL("tmp/vsifile_6.bin", "rb")
    gdal.VSIFSeekL(fp, offset, 0)
    got_data = gdal.VSIFReadL(1, len(ref_data), fp)
    gdal.VSIFCloseL(fp)

    assert ref_data == got_data

    # Real test now
    with gdal.config_option("VSI_CACHE", "YES"):
        fp = gdal.VSIFOpenL("tmp/vsifile_6.bin", "rb")
    gdal.VSIFSeekL(fp, offset, 0)
    got_data = gdal.VSIFReadL(1, len(ref_data), fp)
    gdal.VSIFCloseL(fp)

    assert ref_data == got_data

    gdal.Unlink("tmp/vsifile_6.bin")


###############################################################################
# Test limit cases on /vsimem


def test_vsifile_7():

    if gdal.GetConfigOption("SKIP_MEM_INTENSIVE_TEST") is not None:
        pytest.skip()

    # Test extending file beyond reasonable limits in write mode
    fp = gdal.VSIFOpenL("/vsimem/vsifile_7.bin", "wb")
    assert gdal.VSIFSeekL(fp, 0x7FFFFFFFFFFFFFFF, 0) == 0
    assert gdal.VSIStatL("/vsimem/vsifile_7.bin").size == 0
    with gdal.quiet_errors():
        ret = gdal.VSIFWriteL("a", 1, 1, fp)
    assert ret == 0
    assert gdal.VSIStatL("/vsimem/vsifile_7.bin").size == 0
    gdal.VSIFCloseL(fp)

    # Test seeking  beyond file size in read-only mode
    fp = gdal.VSIFOpenL("/vsimem/vsifile_7.bin", "rb")
    assert gdal.VSIFSeekL(fp, 0x7FFFFFFFFFFFFFFF, 0) == 0
    assert gdal.VSIFEofL(fp) == 0
    assert gdal.VSIFTellL(fp) == 0x7FFFFFFFFFFFFFFF
    assert not gdal.VSIFReadL(1, 1, fp)
    assert gdal.VSIFEofL(fp) == 1
    gdal.VSIFCloseL(fp)

    gdal.Unlink("/vsimem/vsifile_7.bin")


###############################################################################
# Test renaming directory in /vsimem


def test_vsifile_8():

    # octal 0666 = decimal 438
    gdal.Mkdir("/vsimem/mydir", 438)
    fp = gdal.VSIFOpenL("/vsimem/mydir/a", "wb")
    gdal.VSIFCloseL(fp)
    gdal.Rename("/vsimem/mydir", "/vsimem/newdir".encode("ascii").decode("ascii"))
    assert gdal.VSIStatL("/vsimem/newdir") is not None
    assert gdal.VSIStatL("/vsimem/newdir/a") is not None
    gdal.Unlink("/vsimem/newdir/a")
    gdal.Rmdir("/vsimem/newdir")


###############################################################################
# Test implicit directory creation in /vsimem


def test_vsifile_implicit_dir_creation_1(tmp_vsimem):

    assert gdal.VSIStatL(str(tmp_vsimem)) is not None
    assert gdal.VSIStatL(str(tmp_vsimem)).IsDirectory()

    fpath = str(tmp_vsimem / "subdir1" / "subdir2" / "subdir3" / "myfile.txt")
    fp = gdal.VSIFOpenL(str(fpath), "wb")
    assert fp is not None
    gdal.VSIFCloseL(fp)

    assert gdal.VSIStatL(str(tmp_vsimem / "subdir1")).IsDirectory()
    assert gdal.VSIStatL(str(tmp_vsimem / "subdir1" / "subdir2")).IsDirectory()
    assert gdal.VSIStatL(
        str(tmp_vsimem / "subdir1" / "subdir2" / "subdir3")
    ).IsDirectory()


def test_vsifile_implicit_dir_creation_2(tmp_vsimem):

    fpath = str(tmp_vsimem / "afile")
    fp = gdal.VSIFOpenL(str(fpath), "wb")
    assert fp is not None
    gdal.VSIFCloseL(fp)

    fpath = str(tmp_vsimem / "afile" / "anotherfile")
    fp = gdal.VSIFOpenL(str(fpath), "wb")
    assert fp is None


###############################################################################
# Test ReadDir()


def test_vsifile_9():

    lst = gdal.ReadDir(".")
    assert len(lst) >= 4
    # Test truncation
    lst_truncated = gdal.ReadDir(".", int(len(lst) / 2))
    assert len(lst_truncated) > int(len(lst) / 2)

    gdal.Mkdir("/vsimem/mydir", 438)
    for i in range(10):
        fp = gdal.VSIFOpenL("/vsimem/mydir/%d" % i, "wb")
        gdal.VSIFCloseL(fp)

    lst = gdal.ReadDir("/vsimem/mydir")
    assert len(lst) >= 4
    # Test truncation
    lst_truncated = gdal.ReadDir("/vsimem/mydir", int(len(lst) / 2))
    assert len(lst_truncated) > int(len(lst) / 2)

    for i in range(10):
        gdal.Unlink("/vsimem/mydir/%d" % i)
    gdal.Rmdir("/vsimem/mydir")


###############################################################################
# Test fuzzer friendly archive


def test_vsifile_10():

    gdal.FileFromMemBuffer(
        "/vsimem/vsifile_10.tar",
        """FUZZER_FRIENDLY_ARCHIVE
***NEWFILE***:test.txt
abc***NEWFILE***:huge.txt
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
0123456789012345678901234567890123456789012345678901234567890123456789012345678X
***NEWFILE***:small.txt
a""",
    )
    contents = gdal.ReadDir("/vsitar//vsimem/vsifile_10.tar")
    if contents is None:
        gdal.Unlink("/vsimem/vsifile_10.tar")
        pytest.skip()
    assert contents == ["test.txt", "huge.txt", "small.txt"]
    assert gdal.VSIStatL("/vsitar//vsimem/vsifile_10.tar/test.txt").size == 3
    assert gdal.VSIStatL("/vsitar//vsimem/vsifile_10.tar/huge.txt").size == 3888
    assert gdal.VSIStatL("/vsitar//vsimem/vsifile_10.tar/small.txt").size == 1

    gdal.FileFromMemBuffer(
        "/vsimem/vsifile_10.tar",
        """FUZZER_FRIENDLY_ARCHIVE
***NEWFILE***:x
abc""",
    )
    contents = gdal.ReadDir("/vsitar//vsimem/vsifile_10.tar")
    assert contents == ["x"]

    gdal.FileFromMemBuffer(
        "/vsimem/vsifile_10.tar",
        """FUZZER_FRIENDLY_ARCHIVE
***NEWFILE***:x
abc***NEWFILE***:""",
    )
    contents = gdal.ReadDir("/vsitar//vsimem/vsifile_10.tar")
    assert contents == ["x"]

    gdal.Unlink("/vsimem/vsifile_10.tar")


###############################################################################
# Test generic Truncate implementation for file extension


def test_vsifile_11():
    f = gdal.VSIFOpenL("/vsimem/vsifile_11", "wb")
    gdal.VSIFCloseL(f)

    f = gdal.VSIFOpenL("/vsisubfile/0_,/vsimem/vsifile_11", "wb")
    gdal.VSIFWriteL("0123456789", 1, 10, f)
    assert gdal.VSIFTruncateL(f, 10 + 4096 + 2) == 0
    assert gdal.VSIFTellL(f) == 10
    assert gdal.VSIFTruncateL(f, 0) == -1
    gdal.VSIFCloseL(f)

    f = gdal.VSIFOpenL("/vsimem/vsifile_11", "rb")
    data = gdal.VSIFReadL(1, 10 + 4096 + 2, f)
    gdal.VSIFCloseL(f)
    import struct

    data = struct.unpack("B" * len(data), data)
    assert (
        data[0] == 48
        and data[9] == 57
        and data[10] == 0
        and data[10 + 4096 + 2 - 1] == 0
    )

    gdal.Unlink("/vsimem/vsifile_11")


###############################################################################
# Test regular file system sparse file support


def test_vsifile_12():

    target_dir = "tmp"

    if gdal.VSISupportsSparseFiles(target_dir) == 0:
        pytest.skip()

    # Minimum value to make it work on NTFS
    block_size = 65536
    f = gdal.VSIFOpenL(target_dir + "/vsifile_12", "wb")
    gdal.VSIFWriteL("a", 1, 1, f)
    assert gdal.VSIFTruncateL(f, block_size * 2) == 0
    ret = gdal.VSIFGetRangeStatusL(f, 0, 1)
    # We could get unknown on nfs
    if ret == gdal.VSI_RANGE_STATUS_UNKNOWN:
        print("Range status unknown")
    else:
        assert ret == gdal.VSI_RANGE_STATUS_DATA
        ret = gdal.VSIFGetRangeStatusL(f, block_size * 2 - 1, 1)
        assert ret == gdal.VSI_RANGE_STATUS_HOLE
    gdal.VSIFCloseL(f)

    gdal.Unlink(target_dir + "/vsifile_12")


###############################################################################
# Test reading filename with prefixes without terminating slash


def test_vsifile_13():

    gdal.VSIFOpenL("/vsigzip", "rb")
    gdal.VSIFOpenL("/vsizip", "rb")
    gdal.VSIFOpenL("/vsitar", "rb")
    gdal.VSIFOpenL("/vsimem", "rb")
    gdal.VSIFOpenL("/vsisparse", "rb")
    gdal.VSIFOpenL("/vsisubfile", "rb")
    gdal.VSIFOpenL("/vsicurl", "rb")
    gdal.VSIFOpenL("/vsis3", "rb")
    gdal.VSIFOpenL("/vsicurl_streaming", "rb")
    gdal.VSIFOpenL("/vsis3_streaming", "rb")
    gdal.VSIFOpenL("/vsistdin", "rb")

    fp = gdal.VSIFOpenL("/vsistdout", "wb")
    if fp is not None:
        gdal.VSIFCloseL(fp)

    gdal.VSIStatL("/vsigzip")
    gdal.VSIStatL("/vsizip")
    gdal.VSIStatL("/vsitar")
    gdal.VSIStatL("/vsimem")
    gdal.VSIStatL("/vsisparse")
    gdal.VSIStatL("/vsisubfile")
    gdal.VSIStatL("/vsicurl")
    gdal.VSIStatL("/vsis3")
    gdal.VSIStatL("/vsicurl_streaming")
    gdal.VSIStatL("/vsis3_streaming")
    gdal.VSIStatL("/vsistdin")
    gdal.VSIStatL("/vsistdout")


###############################################################################
# Check performance issue (https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=1673)


def test_vsifile_14():

    with gdal.quiet_errors():
        gdal.VSIFOpenL(
            "/vsitar//vsitar//vsitar//vsitar//vsitar//vsitar//vsitar//vsitar/a.tgzb.tgzc.tgzd.tgze.tgzf.tgz.h.tgz.i.tgz",
            "rb",
        )


###############################################################################
# Test issue with Eof() not detecting end of corrupted gzip stream (#6944)


def test_vsifile_15():

    fp = gdal.VSIFOpenL("/vsigzip/data/corrupted_z_buf_error.gz", "rb")
    assert fp is not None
    file_len = 0
    while not gdal.VSIFEofL(fp):
        with gdal.quiet_errors():
            file_len += len(gdal.VSIFReadL(1, 4, fp))
    assert file_len == 6469

    with gdal.quiet_errors():
        file_len += len(gdal.VSIFReadL(1, 4, fp))
    assert file_len == 6469

    with gdal.quiet_errors():
        assert gdal.VSIFSeekL(fp, 0, 2) != 0

    assert gdal.VSIFSeekL(fp, 0, 0) == 0

    len_read = len(gdal.VSIFReadL(1, file_len, fp))
    assert len_read == file_len

    gdal.VSIFCloseL(fp)


###############################################################################
# Test failed gdal.Rename() with exceptions enabled


def test_vsifile_16():

    with gdal.ExceptionMgr(useExceptions=True):
        with pytest.raises(Exception):
            gdal.Rename("/tmp/i_do_not_exist_vsifile_16.tif", "/tmp/me_neither.tif")


###############################################################################
# Test gdal.GetActualURL() on a non-network based filesystem


def test_vsifile_17():

    assert gdal.GetActualURL("foo") is None

    assert gdal.GetSignedURL("foo") is None


###############################################################################
# Test gdal.GetFileSystemsPrefixes()


def test_vsifile_18():

    prefixes = gdal.GetFileSystemsPrefixes()
    assert "/vsimem/" in prefixes


###############################################################################
# Test gdal.GetFileSystemOptions()


def test_vsifile_19():

    for prefix in gdal.GetFileSystemsPrefixes():
        options = gdal.GetFileSystemOptions(prefix)
        # Check that the options is XML correct
        if options is not None:
            try:
                etree.fromstring(options)
            except Exception:
                assert False, (prefix, options)


###############################################################################
# Test gdal.VSIFReadL with None fp


def test_vsifile_20():

    try:
        gdal.VSIFReadL(1, 1, None)
    except ValueError:
        return

    pytest.fail()


###############################################################################
# Test gdal.VSIGetMemFileBuffer_unsafe() and gdal.VSIFWriteL() reading buffers


def test_vsifile_21():

    filename = "/vsimem/read.tif"
    filename_write = "/vsimem/write.tif"
    data = "This is some data"

    vsifile = gdal.VSIFOpenL(filename, "wb")
    assert gdal.VSIFWriteL(data, 1, len(data), vsifile) == len(data)
    gdal.VSIFCloseL(vsifile)

    vsifile = gdal.VSIFOpenL(filename, "rb")
    gdal.VSIFSeekL(vsifile, 0, 2)
    vsilen = gdal.VSIFTellL(vsifile)
    gdal.VSIFSeekL(vsifile, 0, 0)
    data_read = gdal.VSIFReadL(1, vsilen, vsifile)
    data_mem = gdal.VSIGetMemFileBuffer_unsafe(filename)
    assert data_read == data_mem[:]
    gdal.VSIFCloseL(vsifile)
    vsifile_write = gdal.VSIFOpenL(filename_write, "wb")
    assert gdal.VSIFWriteL(data_mem, 1, len(data_mem), vsifile_write) == len(data_mem)
    gdal.VSIFCloseL(vsifile_write)
    gdal.Unlink(filename)
    gdal.Unlink(filename_write)
    with gdal.quiet_errors():
        data3 = gdal.VSIGetMemFileBuffer_unsafe(filename)
        assert data3 == None


def test_vsifile_22():
    # VSIOpenL doesn't set errorno
    gdal.VSIErrorReset()
    assert gdal.VSIGetLastErrorNo() == 0, (
        "Expected Err=0 after VSIErrorReset(), got %d" % gdal.VSIGetLastErrorNo()
    )

    fp = gdal.VSIFOpenL("tmp/not-existing", "r")
    assert fp is None, "Expected None from VSIFOpenL"
    assert gdal.VSIGetLastErrorNo() == 0, (
        "Expected Err=0 from VSIFOpenL, got %d" % gdal.VSIGetLastErrorNo()
    )

    # VSIOpenExL does
    fp = gdal.VSIFOpenExL("tmp/not-existing", "r", 1)
    assert fp is None, "Expected None from VSIFOpenExL"
    assert gdal.VSIGetLastErrorNo() == 1, (
        "Expected Err=1 from VSIFOpenExL, got %d" % gdal.VSIGetLastErrorNo()
    )
    assert len(gdal.VSIGetLastErrorMsg()) != 0, "Expected a VSI error message"
    gdal.VSIErrorReset()
    assert gdal.VSIGetLastErrorNo() == 0, (
        "Expected Err=0 after VSIErrorReset(), got %d" % gdal.VSIGetLastErrorNo()
    )


###############################################################################
# Test bugfix for https://github.com/OSGeo/gdal/issues/675


def test_vsitar_bug_675():

    content = gdal.ReadDir("/vsitar/data/tar_with_star_base256_fields.tar")
    assert len(content) == 1


###############################################################################
# Test multithreaded compression


def test_vsigzip_multi_thread():

    with gdaltest.config_options(
        {"GDAL_NUM_THREADS": "ALL_CPUS", "CPL_VSIL_DEFLATE_CHUNK_SIZE": "32K"}
    ):
        f = gdal.VSIFOpenL("/vsigzip//vsimem/vsigzip_multi_thread.gz", "wb")
        for i in range(100000):
            gdal.VSIFWriteL("hello", 1, 5, f)
        gdal.VSIFCloseL(f)

    f = gdal.VSIFOpenL("/vsigzip//vsimem/vsigzip_multi_thread.gz", "rb")
    data = gdal.VSIFReadL(100000, 5, f).decode("ascii")
    gdal.VSIFCloseL(f)

    gdal.Unlink("/vsimem/vsigzip_multi_thread.gz")

    if data != "hello" * 100000:
        for i in range(10000):
            if data[i * 5 : i * 5 + 5] != "hello":
                print(i * 5, data[i * 5 : i * 5 + 5], data[i * 5 - 5 : i * 5 + 5 - 5])
                break

        pytest.fail()


###############################################################################
# Test vsisync()


def test_vsisync():

    with gdal.quiet_errors():
        assert not gdal.Sync("/i_do/not/exist", "/vsimem/")

    with gdal.quiet_errors():
        assert not gdal.Sync("vsifile.py", "/i_do/not/exist")

    # Test copying a file
    for i in range(2):
        assert gdal.Sync("vsifile.py", "/vsimem/")
        assert (
            gdal.VSIStatL("/vsimem/vsifile.py").size == gdal.VSIStatL("vsifile.py").size
        )
    gdal.Unlink("/vsimem/vsifile.py")

    # Test copying the content of a directory
    gdal.Mkdir("/vsimem/test_sync", 0)
    gdal.FileFromMemBuffer("/vsimem/test_sync/foo.txt", "bar")
    gdal.Mkdir("/vsimem/test_sync/subdir", 0)
    gdal.FileFromMemBuffer("/vsimem/test_sync/subdir/bar.txt", "baz")

    if sys.platform == "linux":
        with gdal.quiet_errors():
            # even root cannot write into /proc
            assert not gdal.Sync("/vsimem/test_sync/", "/proc/i_do_not/exist")

    assert gdal.Sync("/vsimem/test_sync/", "/vsimem/out")
    assert gdal.ReadDir("/vsimem/out") == ["foo.txt", "subdir"]
    assert gdal.ReadDir("/vsimem/out/subdir") == ["bar.txt"]
    # Again
    assert gdal.Sync("/vsimem/test_sync/", "/vsimem/out")

    gdal.RmdirRecursive("/vsimem/out")

    # Test copying a directory
    pct_values = []

    def my_progress(pct, message, user_data):
        pct_values.append(pct)

    assert gdal.Sync("/vsimem/test_sync", "/vsimem/out", callback=my_progress)

    assert pct_values == [0.5, 1.0]

    assert gdal.ReadDir("/vsimem/out") == ["test_sync"]
    assert gdal.ReadDir("/vsimem/out/test_sync") == ["foo.txt", "subdir"]

    gdal.RmdirRecursive("/vsimem/test_sync")
    gdal.RmdirRecursive("/vsimem/out")


###############################################################################
# Test gdal.OpenDir()


@pytest.mark.parametrize("basepath", ["/vsimem/", "tmp/"])
def test_vsifile_opendir(basepath):

    # Non existing dir
    d = gdal.OpenDir(basepath + "/i_dont_exist")
    assert not d

    gdal.RmdirRecursive(basepath + "/vsifile_opendir")

    gdal.Mkdir(basepath + "/vsifile_opendir", 0o755)

    # Empty dir
    d = gdal.OpenDir(basepath + "/vsifile_opendir")
    assert d

    entry = gdal.GetNextDirEntry(d)
    assert not entry

    gdal.CloseDir(d)

    f = gdal.VSIFOpenL(basepath + "/vsifile_opendir/test", "wb")
    assert f
    gdal.VSIFWriteL("foo", 1, 3, f)
    gdal.VSIFCloseL(f)

    gdal.Mkdir(basepath + "/vsifile_opendir/subdir", 0o755)
    gdal.Mkdir(basepath + "/vsifile_opendir/subdir/subdir2", 0o755)

    f = gdal.VSIFOpenL(basepath + "/vsifile_opendir/subdir/subdir2/test2", "wb")
    assert f
    gdal.VSIFWriteL("bar", 1, 3, f)
    gdal.VSIFCloseL(f)

    # Unlimited depth
    d = gdal.OpenDir(basepath + "/vsifile_opendir")

    entries_found = []
    for i in range(4):
        entry = gdal.GetNextDirEntry(d)
        assert entry
        name = entry.name.replace("\\", "/")
        if name == "test":
            entries_found.append(name)
            assert (entry.mode & 32768) != 0
            assert entry.modeKnown
            assert entry.size == 3
            assert entry.sizeKnown
            assert entry.mtime != 0
            assert entry.mtimeKnown
            assert not entry.extra
        elif name == "subdir":
            entries_found.append(name)
            assert (entry.mode & 16384) != 0
        elif name == "subdir/subdir2":
            entries_found.append(name)
            assert (entry.mode & 16384) != 0
        elif name == "subdir/subdir2/test2":
            entries_found.append(name)
            assert (entry.mode & 32768) != 0
        else:
            assert False, entry.name
    assert len(entries_found) == 4, entries_found

    entry = gdal.GetNextDirEntry(d)
    assert not entry
    gdal.CloseDir(d)

    # Unlimited depth, do not require stating (only honoured on Unix)
    d = gdal.OpenDir(basepath + "/vsifile_opendir", -1, ["NAME_AND_TYPE_ONLY=YES"])

    entries_found = []
    for i in range(4):
        entry = gdal.GetNextDirEntry(d)
        assert entry
        name = entry.name.replace("\\", "/")
        if name == "test":
            entries_found.append(name)
            assert (entry.mode & 32768) != 0
            if os.name == "posix" and basepath == "tmp/":
                assert entry.size == 0
        elif name == "subdir":
            entries_found.append(name)
            assert (entry.mode & 16384) != 0
        elif name == "subdir/subdir2":
            entries_found.append(name)
            assert (entry.mode & 16384) != 0
        elif name == "subdir/subdir2/test2":
            entries_found.append(name)
            assert (entry.mode & 32768) != 0
            if os.name == "posix" and basepath == "tmp/":
                assert entry.size == 0
        else:
            assert False, entry.name
    assert len(entries_found) == 4, entries_found

    entry = gdal.GetNextDirEntry(d)
    assert not entry
    gdal.CloseDir(d)

    # Only top level
    d = gdal.OpenDir(basepath + "/vsifile_opendir", 0)
    entries_found = set()
    for i in range(2):
        entry = gdal.GetNextDirEntry(d)
        assert entry
        entries_found.add(entry.name)
    assert entries_found == set(["test", "subdir"])

    entry = gdal.GetNextDirEntry(d)
    assert not entry
    gdal.CloseDir(d)

    # Depth 1
    files = set(
        [
            l_entry.name.replace("\\", "/")
            for l_entry in gdal.listdir(basepath + "/vsifile_opendir", 1)
        ]
    )
    assert files == set(["test", "subdir", "subdir/subdir2"])

    # Prefix filtering
    d = gdal.OpenDir(basepath + "/vsifile_opendir", -1, ["PREFIX=t"])
    entry = gdal.GetNextDirEntry(d)
    assert entry.name == "test"
    entry = gdal.GetNextDirEntry(d)
    assert not entry
    gdal.CloseDir(d)

    d = gdal.OpenDir(basepath + "/vsifile_opendir", -1, ["PREFIX=testtoolong"])
    entry = gdal.GetNextDirEntry(d)
    assert not entry
    gdal.CloseDir(d)

    d = gdal.OpenDir(basepath + "/vsifile_opendir", -1, ["PREFIX=subd"])
    entry = gdal.GetNextDirEntry(d)
    assert entry.name == "subdir"
    entry = gdal.GetNextDirEntry(d)
    sep = "\\" if "\\" in entry.name else "/"
    assert entry.name.replace("\\", "/") == "subdir/subdir2"
    entry = gdal.GetNextDirEntry(d)
    assert entry.name.replace("\\", "/") == "subdir/subdir2/test2"
    entry = gdal.GetNextDirEntry(d)
    assert not entry
    gdal.CloseDir(d)

    d = gdal.OpenDir(basepath + "/vsifile_opendir", -1, ["PREFIX=subdir" + sep + "sub"])
    entry = gdal.GetNextDirEntry(d)
    assert entry.name.replace("\\", "/") == "subdir/subdir2"
    entry = gdal.GetNextDirEntry(d)
    assert entry.name.replace("\\", "/") == "subdir/subdir2/test2"
    entry = gdal.GetNextDirEntry(d)
    assert not entry
    gdal.CloseDir(d)

    # Cleanup
    gdal.RmdirRecursive(basepath + "/vsifile_opendir")


###############################################################################
# Test bugfix for https://github.com/OSGeo/gdal/issues/1559


def test_vsitar_verylongfilename_posix():

    f = gdal.VSIFOpenL(
        "/vsitar/data/verylongfilename.tar/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb/ccccccccccccccccccccccccccccccccccc/ddddddddddddddddddddddddddddddddddddddd/eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee/fffffffffffffffffffffffffffffffffffffffffffffff/foo",
        "rb",
    )
    assert f
    data = gdal.VSIFReadL(1, 3, f).decode("ascii")
    gdal.VSIFCloseL(f)
    assert data == "bar"


###############################################################################
# Test bugfix for https://github.com/OSGeo/gdal/issues/4625


def test_vsitar_longfilename_ustar():

    assert (
        gdal.VSIStatL(
            "/vsitar/data/longfilename_ustar.tar/zzzzzzzzzzzzzzzzzzzzzzzzzzzzzz/bbbbbbbbbbbbbbbbbbbbbbb/ccccccccccccccccccccccccc/dddddddddd/e/byte.tif"
        )
        is not None
    )


def test_unlink_batch():

    gdal.FileFromMemBuffer("/vsimem/foo", "foo")
    gdal.FileFromMemBuffer("/vsimem/bar", "bar")
    assert gdal.UnlinkBatch(["/vsimem/foo", "/vsimem/bar"])
    assert not gdal.VSIStatL("/vsimem/foo")
    assert not gdal.VSIStatL("/vsimem/bar")

    assert not gdal.UnlinkBatch([])

    gdal.FileFromMemBuffer("/vsimem/foo", "foo")
    open("tmp/bar", "wt").write("bar")
    with gdal.quiet_errors():
        assert not gdal.UnlinkBatch(["/vsimem/foo", "tmp/bar"])
    gdal.Unlink("/vsimem/foo")
    gdal.Unlink("tmp/bar")


###############################################################################
# Test gdal.RmdirRecursive()


def test_vsifile_rmdirrecursive():

    gdal.Mkdir("tmp/rmdirrecursive", 493)
    gdal.Mkdir("tmp/rmdirrecursive/subdir", 493)
    open("tmp/rmdirrecursive/foo.bin", "wb").close()
    open("tmp/rmdirrecursive/subdir/bar.bin", "wb").close()
    assert gdal.RmdirRecursive("tmp/rmdirrecursive") == 0
    assert not os.path.exists("tmp/rmdirrecursive")


###############################################################################


def test_vsifile_vsizip_error():

    for i in range(128):
        filename = "/vsimem/tmp||maxlength=%d.zip" % i
        with gdal.quiet_errors():
            f = gdal.VSIFOpenL("/vsizip/%s/out.bin" % filename, "wb")
            if f is not None:
                assert gdal.VSIFCloseL(f) < 0
        gdal.Unlink(filename)


###############################################################################
# Test bugfix for https://github.com/OSGeo/gdal/issues/5225


def test_vsifile_vsitar_gz_with_tar_multiple_of_65536_bytes():

    f = gdal.VSIFOpenL("/vsitar/data/tar_of_65536_bytes.tar.gz/zero.bin", "rb")
    assert f is not None
    read_bytes = gdal.VSIFReadL(1, 65024, f)
    gdal.VSIFCloseL(f)
    assert read_bytes == b"\x00" * 65024
    gdal.Unlink("data/tar_of_65536_bytes.tar.gz.properties")


###############################################################################
# Test bugfix for https://github.com/OSGeo/gdal/issues/5468


def test_vsifile_vsizip_stored():

    f = gdal.VSIFOpenL("/vsizip/data/stored.zip/foo.txt", "rb")
    assert f
    assert gdal.VSIFReadL(1, 5, f) == b"foo\n"
    assert gdal.VSIFEofL(f)
    gdal.VSIFCloseL(f)


###############################################################################
# Test that VSIFTruncateL() zeroize beyond the truncated area


def test_vsifile_vsimem_truncate_zeroize():

    filename = "/vsimem/test.bin"
    f = gdal.VSIFOpenL(filename, "wb+")
    data = b"\xFF" * 10000
    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFTruncateL(f, 0)
    gdal.VSIFSeekL(f, 10000, 0)
    gdal.VSIFWriteL(b"\x00", 1, 1, f)
    gdal.VSIFSeekL(f, 0, 0)
    assert gdal.VSIFReadL(1, 1, f) == b"\x00"
    gdal.VSIFCloseL(f)


###############################################################################
# Test VSICopyFile()


def test_vsifile_copyfile():

    # Most simple invocation
    dstfilename = "/vsimem/test_vsifile_copyfile.tif"
    assert gdal.CopyFile("data/byte.tif", dstfilename) == 0
    assert gdal.VSIStatL(dstfilename).size == gdal.VSIStatL("data/byte.tif").size

    # Test srcfilename passed to None
    srcfilename = "/vsimem/test.bin"
    f = gdal.VSIFOpenL(srcfilename, "wb+")
    gdal.VSIFTruncateL(f, 1000 * 1000)
    assert gdal.CopyFile(None, dstfilename, f) == 0
    gdal.VSIFCloseL(f)
    gdal.Unlink(srcfilename)
    assert gdal.VSIStatL(dstfilename).size == 1000 * 1000

    # Test progress callback
    srcfilename = "/vsimem/test.bin"
    f = gdal.VSIFOpenL(srcfilename, "wb+")
    gdal.VSIFTruncateL(f, 1000 * 1000)
    gdal.VSIFCloseL(f)

    def progress(pct, msg, user_data):
        user_data.append(pct)
        return 1

    tab = []
    assert (
        gdal.CopyFile(srcfilename, dstfilename, callback=progress, callback_data=tab)
        == 0
    )
    assert tab[-1] == 1.0
    gdal.Unlink(srcfilename)
    assert gdal.VSIStatL(dstfilename).size == 1000 * 1000

    # Test progress callback in error situation
    srcfilename = "/vsimem/test.bin"
    f = gdal.VSIFOpenL(srcfilename, "wb+")
    gdal.VSIFTruncateL(f, 1000 * 1000)
    gdal.VSIFCloseL(f)

    def progress(pct, msg, user_data):
        if pct > 0.5:
            return 0
        user_data.append(pct)
        return 1

    tab = []
    assert (
        gdal.CopyFile(srcfilename, dstfilename, callback=progress, callback_data=tab)
        != 0
    )
    assert tab[-1] != 1.0
    gdal.Unlink(srcfilename)
    assert gdal.VSIStatL(dstfilename).size != 1000 * 1000

    gdal.Unlink(dstfilename)


def test_vsimem_illegal_filename():
    assert gdal.FileFromMemBuffer("/vsimem/\\\\", "foo") == -1


###############################################################################
# Test operations with Windows special filenames (prefix with "\\?\")


@pytest.mark.skipif(sys.platform != "win32", reason="Windows specific test")
def test_vsifile_win32_special_filenames(tmp_path):

    # Try prefix filenames
    tmp_path_str = str(tmp_path)
    if "/" not in tmp_path_str:
        prefix_path = "\\\\?\\" + tmp_path_str
        assert gdal.VSIStatL(prefix_path) is not None

        assert gdal.MkdirRecursive(prefix_path + "\\foo\\bar", 0o755) == 0
        assert gdal.VSIStatL(prefix_path + "\\foo\\bar") is not None

        assert gdal.Mkdir(prefix_path + "\\foo\\baz", 0o755) == 0

        f = gdal.VSIFOpenL(prefix_path + "\\foo\\file.bin", "wb")
        assert f
        gdal.VSIFCloseL(f)

        assert set(gdal.ReadDir(prefix_path)) == set([".", "..", "foo"])
        assert set(gdal.ReadDirRecursive(prefix_path)) == set(
            ["foo\\", "foo\\file.bin", "foo\\bar\\", "foo\\baz\\"]
        )

        assert gdal.Sync(prefix_path + "\\foo\\", prefix_path + "\\foo2")
        assert set(gdal.ReadDirRecursive(prefix_path + "\\foo2")) == set(
            ["file.bin", "bar\\", "baz\\"]
        )

        assert gdal.Rmdir(prefix_path + "\\foo\\bar") == 0
        assert gdal.RmdirRecursive(prefix_path + "\\foo") == 0
        assert gdal.VSIStatL(prefix_path + "\\foo") is None


###############################################################################
# Test operations with Windows network path


@pytest.mark.skipif(sys.platform != "win32", reason="Windows specific test")
@pytest.mark.skipif(
    gdaltest.is_travis_branch("mingw64"), reason="does not work with mingw64"
)
def test_vsifile_win32_network_path():

    # Try the code path that converts network paths "\\foo\bar" to prefixed ones
    # "\\?\foo\bar"
    drive_letter = os.getcwd()[0]
    dirname = f"\\\\localhost\\{drive_letter}$"
    assert gdal.VSIStatL(dirname) is not None


###############################################################################
# Test operations with VSI_CACHE=YES and past EOF reads


@pytest.mark.require_driver("GTiff")
def test_vsifile_eof_cache_read(tmp_path):
    """Test issue GH #9658"""

    tmp_filename = str(tmp_path / "vsifile_eof_cache_read.bin")
    f = gdal.VSIFOpenL(tmp_filename, "wb")
    gdal.VSIFWriteL(b"x" * 100000, 100000, 1, f)
    gdal.VSIFCloseL(f)
    with gdal.config_option("VSI_CACHE", "YES"):
        f = gdal.VSIFOpenL(tmp_filename, "rb")
        gdal.VSIFSeekL(f, 60000, 0)
        data = gdal.VSIFReadL(1, 75000, f)  # reads past end of file
        gdal.VSIFCloseL(f)
        assert data == b"x" * 40000


def test_vsifile_use_closed_file(tmp_path):

    f = gdal.VSIFOpenL(tmp_path / "file.txt", "wb")
    assert gdal.VSIFWriteL("0123456789", 1, 10, f) == 10
    gdal.VSIFCloseL(f)

    with pytest.raises(ValueError, match="closed file"):
        gdal.VSIFCloseL(f)

    with pytest.raises(ValueError, match="closed file"):
        gdal.VSIFEofL(f)

    with pytest.raises(ValueError, match="closed file"):
        gdal.VSIFSeekL(f, 0, 0)

    with pytest.raises(ValueError, match="closed file"):
        gdal.VSIFTellL(f)

    with pytest.raises(ValueError, match="closed file"):
        gdal.VSIFTruncateL(f, 0)

    with pytest.raises(ValueError, match="closed file"):
        gdal.VSIFWriteL("0123456789", 1, 10, f)
