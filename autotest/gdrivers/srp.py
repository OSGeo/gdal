#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for SRP driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
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
from osgeo import osr

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Read USRP dataset with PCB=0

def srp_1(filename = 'USRP_PCB0/FKUSRP01.IMG'):

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32600 + 17)

    tst = gdaltest.GDALTest( 'SRP', filename, 1, 24576 )
    ret = tst.testOpen( check_prj = srs.ExportToWkt(), check_gt = (500000.0, 5.0, 0.0, 5000000.0, 0.0, -5.0) )
    if ret != 'success':
        return ret

    ds = gdal.Open('data/' + filename)
    if ds.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_PaletteIndex:
        gdaltest.post_reason('fail')
        return 'fail'

    ct = ds.GetRasterBand(1).GetColorTable()
    if ct.GetCount() != 4:
        gdaltest.post_reason('fail')
        return 'fail'

    if ct.GetColorEntry(0) != (0,0,0,255):
        gdaltest.post_reason('fail')
        print(ct.GetColorEntry(0))
        return 'fail'

    if ct.GetColorEntry(1) != (255,0,0,255):
        gdaltest.post_reason('fail')
        print(ct.GetColorEntry(1))
        return 'fail'

    return 'success'

###############################################################################
# Read USRP dataset with PCB=4

def srp_2():
    return srp_1('USRP_PCB4/FKUSRP01.IMG')

###############################################################################
# Read USRP dataset with PCB=8

def srp_3():
    return srp_1('USRP_PCB8/FKUSRP01.IMG')

###############################################################################

gdaltest_list = [
    srp_1,
    srp_2,
    srp_3,
 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'srp' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

