#!/usr/bin/env pytest
###############################################################################
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
# SPDX-License-Identifier: MIT
###############################################################################


import pytest

from osgeo import gdal

###############################################################################
# Create a color table and verify its contents


def test_colortable_1():

    test_ct_data = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255, 0)]

    test_ct = gdal.ColorTable()
    for i, color in enumerate(test_ct_data):
        test_ct.SetColorEntry(i, color)

    for i in range(len(test_ct_data)):
        g_data = test_ct.GetColorEntry(i)
        o_data = test_ct_data[i]

        for j in range(4):
            if len(o_data) <= j:
                o_v = 255
            else:
                o_v = o_data[j]

            assert g_data[j] == o_v, "color table mismatch"


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
