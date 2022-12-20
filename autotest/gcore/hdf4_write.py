#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for HDF4 driver.
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
    ("../../gdrivers/data/gtiff/int8.tif", 1046),
    ("int16.tif", 4672),
    ("uint16.tif", 4672),
    ("int32.tif", 4672),
    ("uint32.tif", 4672),
    ("float32.tif", 4672),
    ("float64.tif", 4672),
    ("utmsmall.tif", 50054),
]


@pytest.mark.parametrize("rank", [2, 3], ids=lambda x: "rank%d" % x)
@pytest.mark.parametrize(
    "filename,checksum", init_list, ids=[arg[0].split(".")[0] for arg in init_list]
)
@pytest.mark.parametrize(
    "testfunction",
    [
        "testCreateCopy",
        "testCreate",
        "testSetGeoTransform",
        "testSetProjection",
        "testSetMetadata",
        "testSetNoDataValue",
        "testSetDescription",
    ],
)
@pytest.mark.require_driver("HDF4Image")
def test_hdf4_write(filename, checksum, testfunction, rank):
    ut = gdaltest.GDALTest(
        "HDF4Image", filename, 1, checksum, options=["RANK=%d" % rank]
    )
    getattr(ut, testfunction)()
