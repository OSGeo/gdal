#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for RPFTOC driver.
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2008-2010, Even Rouault <even dot rouault at mines-paris dot org>
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
import sys
from osgeo import gdal
import shutil

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Read a simple and hand-made RPFTOC dataset, made of one single CADRG frame
# whose content is fully empty.

def rpftoc_1():
    tst = gdaltest.GDALTest( 'RPFTOC', 'NITF_TOC_ENTRY:CADRG_ONC_1,000,000_2_0:data/A.TOC', 1, 53599, filename_absolute = 1 )
    gt = (1.9999416000000001, 0.0017833876302083334, 0.0, 36.000117500000002, 0.0, -0.0013461816406249993)
    return tst.testOpen(check_gt = gt)

###############################################################################
# Same test as rpftoc_1, but the dataset is forced to be opened in RGBA mode

def rpftoc_2():
    gdal.SetConfigOption( 'RPFTOC_FORCE_RGBA', 'YES' )
    tst = gdaltest.GDALTest( 'RPFTOC', 'NITF_TOC_ENTRY:CADRG_ONC_1,000,000_2_0:data/A.TOC', 1, 0, filename_absolute = 1 )
    res = tst.testOpen()
    gdal.SetConfigOption( 'RPFTOC_FORCE_RGBA', 'NO' )
    return res

###############################################################################
# Test reading the metadata

def rpftoc_3():
    ds = gdal.Open('data/A.TOC')
    md = ds.GetMetadata('SUBDATASETS')
    if 'SUBDATASET_1_NAME' not in md or md['SUBDATASET_1_NAME'] != 'NITF_TOC_ENTRY:CADRG_ONC_1,000,000_2_0:data/A.TOC':
        gdaltest.post_reason( 'missing SUBDATASET_1_NAME metadata' )
        return 'fail'

    ds = gdal.Open('NITF_TOC_ENTRY:CADRG_ONC_1,000,000_2_0:data/A.TOC')
    md = ds.GetMetadata()
    if 'FILENAME_0' not in md or (md['FILENAME_0'] != 'data/RPFTOC01.ON2' and md['FILENAME_0'] != 'data\\RPFTOC01.ON2'):
        gdaltest.post_reason( 'missing SUBDATASET_1_NAME metadata' )
        return 'fail'

    return 'success'

###############################################################################
# Add an overview

def rpftoc_4():
    gdal.SetConfigOption( 'RPFTOC_FORCE_RGBA', 'YES' )

    shutil.copyfile( 'data/A.TOC', 'tmp/A.TOC' )
    shutil.copyfile( 'data/RPFTOC01.ON2', 'tmp/RPFTOC01.ON2' )

    ds = gdal.Open('NITF_TOC_ENTRY:CADRG_ONC_1,000,000_2_0:tmp/A.TOC')
    err = ds.BuildOverviews( overviewlist = [2, 4] )

    if err != 0:
        gdaltest.post_reason('BuildOverviews reports an error' )
        return 'fail'

    if ds.GetRasterBand(1).GetOverviewCount() != 2:
        gdaltest.post_reason('Overview missing on target file.')
        return 'fail'

    ds = None
    ds = gdal.Open('NITF_TOC_ENTRY:CADRG_ONC_1,000,000_2_0:tmp/A.TOC')
    if ds.GetRasterBand(1).GetOverviewCount() != 2:
        gdaltest.post_reason('Overview missing on target file after re-open.')
        return 'fail'

    ds = None

    gdal.SetConfigOption( 'RPFTOC_FORCE_RGBA', 'NO' )

    os.unlink('tmp/A.TOC')
    os.unlink('tmp/A.TOC.1.ovr')
    os.unlink('tmp/RPFTOC01.ON2')

    return 'success'

gdaltest_list = [
    rpftoc_1,
    rpftoc_2,
    rpftoc_3,
    rpftoc_4 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'rpftoc' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

