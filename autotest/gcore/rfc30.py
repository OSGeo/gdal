#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test RFC 30 (UTF filename handling) support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2010 Frank Warmerdam
# Copyright (c) 2010-2011, Even Rouault <even dot rouault at spatialys.com>
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

import urllib.parse

from osgeo import gdal

import gdaltest
import pytest

###############################################################################
# Try opening a file with a Chinese name using the Python UTF-8 string.


def test_rfc30_1():

    filename = 'xx\u4E2D\u6587.\u4E2D\u6587'
    filename_escaped = urllib.parse.quote(filename)

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/gtiff/' + filename_escaped, filename):
        pytest.skip()

    filename = 'tmp/cache/' + filename

    ds = gdal.Open(filename)

    file_list = ds.GetFileList()

    assert ds is not None, 'failed to open utf filename.'

    ds = None

    ds = gdal.Open(file_list[0])

    assert ds is not None, 'failed to open utf filename (2).'

###############################################################################
# Try creating, then renaming a utf-8 named file.


def test_rfc30_2():

    filename = 'tmp/yy\u4E2D\u6587.\u4E2D\u6587'
    fd = gdal.VSIFOpenL(filename, 'w')
    assert fd is not None, 'failed to create utf-8 named file.'

    gdal.VSIFWriteL('abc', 3, 1, fd)
    gdal.VSIFCloseL(fd)

    # rename

    new_filename = 'tmp/yy\u4E2D\u6587.\u4E2D\u6587'
    filename_for_rename = filename

    assert gdal.Rename(filename_for_rename, new_filename) == 0, 'utf-8 rename failed.'

    fd = gdal.VSIFOpenL(new_filename, 'r')
    assert fd is not None, 'reopen failed with utf8'

    data = gdal.VSIFReadL(3, 1, fd)
    gdal.VSIFCloseL(fd)

    assert data == b'abc'

    gdal.Unlink(new_filename)

    fd = gdal.VSIFOpenL(new_filename, 'r')
    assert fd is None, 'did unlink fail on utf8 filename?'
