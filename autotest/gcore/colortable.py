#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test functioning of the GDALColorTable.  Mostly this tests
#           the python binding.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
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



import gdaltest
from osgeo import gdal
import pytest

###############################################################################
# Create a color table.


def test_colortable_1():

    gdaltest.test_ct_data = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255, 0)]

    gdaltest.test_ct = gdal.ColorTable()
    for i in range(len(gdaltest.test_ct_data)):
        gdaltest.test_ct.SetColorEntry(i, gdaltest.test_ct_data[i])

    
###############################################################################
# verify contents.


def test_colortable_2():

    for i in range(len(gdaltest.test_ct_data)):
        g_data = gdaltest.test_ct.GetColorEntry(i)
        o_data = gdaltest.test_ct_data[i]

        for j in range(4):
            if len(o_data) <= j:
                o_v = 255
            else:
                o_v = o_data[j]

            assert g_data[j] == o_v, 'color table mismatch'

    
###############################################################################
# Test CreateColorRamp()


def test_colortable_3():

    ct = gdal.ColorTable()
    try:
        ct.CreateColorRamp
    except AttributeError:
        pytest.skip()

    ct.CreateColorRamp(0, (255, 0, 0), 255, (0, 0, 255))

    assert ct.GetColorEntry(0) == (255, 0, 0, 255)

    assert ct.GetColorEntry(255) == (0, 0, 255, 255)

###############################################################################
# Cleanup.


def test_colortable_cleanup():
    gdaltest.test_ct = None



