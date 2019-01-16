#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic read support for a all datatypes from a BMP file.
# Author:   Andrey Kiselev, dron@remotesensing.org
#
###############################################################################
# Copyright (c) 2003, Andrey Kiselev <dron@remotesensing.org>
# Copyright (c) 2008-2009, Even Rouault <even dot rouault at mines-paris dot org>
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


init_list = [
    ('1bit.bmp', 200),
    ('4bit_pal.bmp', 2587),
    ('8bit_pal.bmp', 4672),
    ('byte_rle8.bmp', 4672)]


@pytest.mark.parametrize(
    'filename,checksum',
    init_list,
    ids=[tup[0].split('.')[0] for tup in init_list],
)
@pytest.mark.require_driver('BMP')
def test_bmp_open(filename, checksum):
    ut = gdaltest.GDALTest('BMP', filename, 1, checksum)
    ut.testOpen()


def test_bmp_online_1():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/bmp/8bit_pal_rle.bmp', '8bit_pal_rle.bmp'):
        pytest.skip()

    tst = gdaltest.GDALTest('BMP', 'tmp/cache/8bit_pal_rle.bmp', 1, 17270, filename_absolute=1)

    return tst.testOpen()


def test_bmp_online_2():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/bmp/24bit.bmp', '24bit.bmp'):
        pytest.skip()

    tst = gdaltest.GDALTest('BMP', 'tmp/cache/24bit.bmp', 1, 7158, filename_absolute=1)
    if tst == 'success':
        tst = gdaltest.GDALTest('BMP', 'tmp/cache/24bit.bmp', 3, 27670, filename_absolute=1)

    return tst.testOpen()



