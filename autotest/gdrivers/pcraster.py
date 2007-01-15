#!/usr/bin/env python
###############################################################################
# $Id: pcraster.py,v 1.2 2006/10/27 04:27:12 fwarmerdam Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test PCRaster driver support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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
# 
#  $Log: pcraster.py,v $
#  Revision 1.2  2006/10/27 04:27:12  fwarmerdam
#  fixed license text
#
#  Revision 1.1  2004/11/11 03:06:54  fwarmerdam
#  New
#
#

import os
import sys
import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Perform simple read test.

def pcraster_1():

    gdaltest.pcraster_drv = gdal.GetDriverByName( 'PCRaster' )

    if gdaltest.pcraster_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'PNG', 'ldd.map', 1, 4528 )
    return tst.testOpen()

###############################################################################
# Verify some auxilary data. 

def pcraster_2():

    if gdaltest.pcraster_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/ldd.map' )

    gt = ds.GetGeoTransform()

    if gt[0] != 182140.0 or gt[1] != 10 or gt[2] != 0 \
       or gt[3] != 327880.0 or gt[4] != 0 or gt[5] != -10:
        gdal.post_reason( 'PCRaster geotransform wrong.' )
        return 'fail'

    band1 = ds.GetRasterBand(1)
    if band1.GetNoDataValue() != 255:
        gdal.post_reason( 'PCRaster NODATA value wrong or missing.' )
        return 'fail'

    return 'success'

    
gdaltest_list = [
    pcraster_1,
    pcraster_2 ]


if __name__ == '__main__':

    gdaltest.setup_run( 'pcraster' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

