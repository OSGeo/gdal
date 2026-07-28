#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test RFC 30 (UTF filename handling) support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2010 Frank Warmerdam
# Copyright (c) 2010-2011, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import urllib.parse

import gdaltest

from osgeo import gdal

###############################################################################
# Try opening a file with a Chinese name using the Python UTF-8 string.


def test_rfc30_1():

    filename = "xx\u4e2d\u6587.\u4e2d\u6587"
    filename_escaped = urllib.parse.quote(filename)

    gdaltest.download_or_skip(
        "http://download.osgeo.org/gdal/data/gtiff/" + filename_escaped, filename
    )

    filename = "tmp/cache/" + filename

    ds = gdal.Open(filename)

    file_list = ds.GetFileList()

    assert ds is not None, "failed to open utf filename."

    ds = None

    ds = gdal.Open(file_list[0])

    assert ds is not None, "failed to open utf filename (2)."


###############################################################################
# Try creating, then renaming a utf-8 named file.


def test_rfc30_2():

    filename = "tmp/yy\u4e2d\u6587.\u4e2d\u6587"
    fd = gdal.VSIFOpenL(filename, "w")
    assert fd is not None, "failed to create utf-8 named file."

    gdal.VSIFWriteL("abc", 3, 1, fd)
    gdal.VSIFCloseL(fd)

    # rename

    new_filename = "tmp/yy\u4e2d\u6587.\u4e2d\u6587"
    filename_for_rename = filename

    assert gdal.Rename(filename_for_rename, new_filename) == 0, "utf-8 rename failed."

    fd = gdal.VSIFOpenL(new_filename, "r")
    assert fd is not None, "reopen failed with utf8"

    data = gdal.VSIFReadL(3, 1, fd)
    gdal.VSIFCloseL(fd)

    assert data == b"abc"

    gdal.Unlink(new_filename)

    fd = gdal.VSIFOpenL(new_filename, "r")
    assert fd is None, "did unlink fail on utf8 filename?"
