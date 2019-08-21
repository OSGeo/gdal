#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for Microsoft Bitmap (.bmp)
#           BMP driver.
# Author:   Andrey Kiselev, dron@remotesensing.org
#
###############################################################################
# Copyright (c) 2003, Andrey Kiselev <dron@remotesensing.org>
# Copyright (c) 2008-2009, Even Rouault <even dot rouault at spatialys.com>
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


import pytest

import gdaltest

###############################################################################
# Test creating an in memory copy.


def test_bmp_vsimem():

    tst = gdaltest.GDALTest('BMP', 'byte.tif', 1, 4672)

    return tst.testCreateCopy(vsimem=1)


###############################################################################
# When imported build a list of units based on the files available.



init_list = [
    ('byte.tif', 4672),
    ('utmsmall.tif', 50054),
    ('8bit_pal.bmp', 4672), ]


@pytest.mark.parametrize(
    'filename,checksum',
    init_list,
    ids=[tup[0].split('.')[0] for tup in init_list],
)
@pytest.mark.parametrize(
    'testfunction', [
        'testCreateCopy',
        'testCreate',
    ]
)
@pytest.mark.require_driver('BMP')
def test_bmp_create(filename, checksum, testfunction):
    ut = gdaltest.GDALTest('BMP', filename, 1, checksum)
    getattr(ut, testfunction)()


