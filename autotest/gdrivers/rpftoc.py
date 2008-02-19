#!/usr/bin/env python
###############################################################################
# $Id: rpftoc.py  rouault $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for RPFTOC driver.
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault @ mines-paris dot org>
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
import gdal
import array
import string

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

gdaltest_list = [
    rpftoc_1,
    rpftoc_2 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'rpftoc' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

