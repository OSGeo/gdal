#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test TPS algorithm.
# Author:   Even Rouault <even dot rouault at mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2014, Even Rouault <even dot rouault at mines-paris dot org>
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

sys.path.append( '../pymod' )

import gdaltest
from osgeo import gdal, osr

###############################################################################
# Test error case (#5586)

def tps_1():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.Create('foo',2,2)
    gcp_list = [
        gdal.GCP(0, 0, 0,  0,  0),
        gdal.GCP(0, 50, 0,  0, 50),
        gdal.GCP(50, 0, 0, 50,  0),
        gdal.GCP(50, 50, 0, 50, 50),
        gdal.GCP(0*25, 0*25, 0, 25, 25),
        ]
    ds.SetGCPs(gcp_list, osr.GetUserInputAsWKT('WGS84'))
    utm_wkt = osr.GetUserInputAsWKT('+proj=utm +zone=11 +datum=WGS84')

    transformer = gdal.Transformer(ds, None,
                                          ['DST_SRS='+utm_wkt,
                                           'METHOD=GCP_TPS'] )
    if gdal.GetLastErrorType() == 0:
        return 'fail'

    return 'success' 


gdaltest_list = [
    tps_1,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'tps' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

