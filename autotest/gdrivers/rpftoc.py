#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for RPFTOC driver.
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2010, Even Rouault <even dot rouault at spatialys.com>
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
import shutil
from osgeo import gdal


import gdaltest

###############################################################################
# Read a simple and hand-made RPFTOC dataset, made of one single CADRG frame
# whose content is fully empty.


def test_rpftoc_1():
    tst = gdaltest.GDALTest('RPFTOC', 'NITF_TOC_ENTRY:CADRG_ONC_1,000,000_2_0:data/nitf/A.TOC', 1, 53599, filename_absolute=1)
    gt = (1.9999416000000001, 0.0017833876302083334, 0.0, 36.000117500000002, 0.0, -0.0013461816406249993)
    return tst.testOpen(check_gt=gt)

###############################################################################
# Same test as rpftoc_1, but the dataset is forced to be opened in RGBA mode


def test_rpftoc_2():
    gdal.SetConfigOption('RPFTOC_FORCE_RGBA', 'YES')
    tst = gdaltest.GDALTest('RPFTOC', 'NITF_TOC_ENTRY:CADRG_ONC_1,000,000_2_0:data/nitf/A.TOC', 1, 0, filename_absolute=1)
    res = tst.testOpen()
    gdal.SetConfigOption('RPFTOC_FORCE_RGBA', 'NO')
    return res

###############################################################################
# Test reading the metadata


def test_rpftoc_3():
    ds = gdal.Open('data/nitf/A.TOC')
    md = ds.GetMetadata('SUBDATASETS')
    assert 'SUBDATASET_1_NAME' in md and md['SUBDATASET_1_NAME'] == 'NITF_TOC_ENTRY:CADRG_ONC_1,000,000_2_0:data/nitf/A.TOC', \
        'missing SUBDATASET_1_NAME metadata'

    ds = gdal.Open('NITF_TOC_ENTRY:CADRG_ONC_1,000,000_2_0:data/nitf/A.TOC')
    md = ds.GetMetadata()
    assert 'FILENAME_0' in md and md['FILENAME_0'].replace('\\', '/') == 'data/nitf/RPFTOC01.ON2'

###############################################################################
# Add an overview


def test_rpftoc_4():
    gdal.SetConfigOption('RPFTOC_FORCE_RGBA', 'YES')

    shutil.copyfile('data/nitf/A.TOC', 'tmp/A.TOC')
    shutil.copyfile('data/nitf/RPFTOC01.ON2', 'tmp/RPFTOC01.ON2')

    ds = gdal.Open('NITF_TOC_ENTRY:CADRG_ONC_1,000,000_2_0:tmp/A.TOC')
    err = ds.BuildOverviews(overviewlist=[2, 4])

    assert err == 0, 'BuildOverviews reports an error'

    assert ds.GetRasterBand(1).GetOverviewCount() == 2, \
        'Overview missing on target file.'

    ds = None
    ds = gdal.Open('NITF_TOC_ENTRY:CADRG_ONC_1,000,000_2_0:tmp/A.TOC')
    assert ds.GetRasterBand(1).GetOverviewCount() == 2, \
        'Overview missing on target file after re-open.'

    ds = None

    gdal.SetConfigOption('RPFTOC_FORCE_RGBA', 'NO')

    os.unlink('tmp/A.TOC')
    os.unlink('tmp/A.TOC.1.ovr')
    os.unlink('tmp/RPFTOC01.ON2')



