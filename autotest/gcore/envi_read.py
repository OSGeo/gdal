#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic read support for a all datatypes from an ENVI file.
# Author:   Andrey Kiselev, dron@remotesensing.org
#
# See also: gdrivers/envi.py for a driver focused on coordinate system support.
#
###############################################################################
# Copyright (c) 2003, Andrey Kiselev <dron@remotesensing.org>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################


import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("ENVI")

###############################################################################
# Test GDAL_READDIR_LIMIT_ON_OPEN


def test_envi_1():

    with gdaltest.config_option("GDAL_READDIR_LIMIT_ON_OPEN", "1"):
        ds = gdal.Open("data/utmsmall.raw")
    filelist = ds.GetFileList()

    assert len(filelist) == 2, "did not get expected file list."


###############################################################################
# When imported build a list of units based on the files available.


init_list = [
    ("byte.raw", 4672),
    ("int16.raw", 4672),
    ("uint16.raw", 4672),
    ("int32.raw", 4672),
    ("uint32.raw", 4672),
    ("float32.raw", 4672),
    ("float64.raw", 4672),
    # ('cfloat32.raw', 5028),
    # ('cfloat64.raw', 5028),
]


@pytest.mark.parametrize(
    "filename,checksum",
    init_list,
    ids=[tup[0].split(".")[0] for tup in init_list],
)
def test_envi_open(filename, checksum):
    ut = gdaltest.GDALTest("ENVI", filename, 1, checksum)
    ut.testOpen()
