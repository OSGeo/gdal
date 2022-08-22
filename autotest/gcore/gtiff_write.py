#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for GTiff driver.
# Author:   Andrey Kiselev, dron@remotesensing.org
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

init_list = [
    ("byte.tif", 4672),
    ("int16.tif", 4672),
    ("uint16.tif", 4672),
    ("int32.tif", 4672),
    ("uint32.tif", 4672),
    ("float32.tif", 4672),
    ("float64.tif", 4672),
    ("cint16.tif", 5028),
    ("cint32.tif", 5028),
    ("cfloat32.tif", 5028),
    ("cfloat64.tif", 5028),
]


# Some tests we don't need to do for each type.
@pytest.mark.parametrize(
    "testfunction",
    [
        "testSetGeoTransform",
        "testSetProjection",
        "testSetMetadata",
    ],
)
@pytest.mark.require_driver("GTiff")
def test_gtiff_set(testfunction):
    ut = gdaltest.GDALTest("GTiff", "byte.tif", 1, 4672)
    getattr(ut, testfunction)()


# Others we do for each pixel type.
@pytest.mark.parametrize(
    "filename,checksum",
    init_list,
    ids=[tup[0].split(".")[0] for tup in init_list],
)
@pytest.mark.parametrize(
    "testfunction",
    [
        "testCreateCopy",
        "testCreate",
        "testSetNoDataValue",
    ],
)
@pytest.mark.require_driver("GTiff")
def test_gtiff_create(filename, checksum, testfunction):
    ut = gdaltest.GDALTest("GTiff", filename, 1, checksum)
    getattr(ut, testfunction)()
