#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  LOS/LAS Testing.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
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


import gdaltest
import pytest

###############################################################################


def test_loslas_online_1():

    if not gdaltest.download_file('http://www.ngs.noaa.gov/PC_PROD/NADCON/NADCON.zip', 'NADCON.zip'):
        pytest.skip()

    try:
        os.stat('tmp/cache/NADCON.zip')
    except OSError:
        pytest.skip()

    try:
        gdaltest.unzip('tmp/cache/NADCON', 'tmp/cache/NADCON.zip')
        os.stat('tmp/cache/NADCON/nadcon.jar')
        gdaltest.unzip('tmp/cache/NADCON', 'tmp/cache/NADCON/nadcon.jar')
        os.stat('tmp/cache/NADCON/grids/wyhpgn.los')
    except OSError:
        pytest.skip()

    tst = gdaltest.GDALTest('LOSLAS', 'tmp/cache/NADCON/grids/wyhpgn.los', 1, 0, filename_absolute=1)
    gt = (-111.625, 0.25, 0.0, 45.625, 0.0, -0.25)
    stats = (-0.0080000003799796, 0.031125999987125001, 0.0093017323318172005, 0.0075646520354096004)
    return tst.testOpen(check_gt=gt, check_stat=stats, check_prj='WGS84')




