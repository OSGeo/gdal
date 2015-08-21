#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test ISCE format driver.
# Author:   Matthieu Volat <matthieu.volat@ujf-grenoble.fr>
#
###############################################################################
# Copyright (c) 2014, Matthieu Volat <matthieu.volat@ujf-grenoble.fr>
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

import sys
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Perform simple read test.
def isce_1():

    tst = gdaltest.GDALTest( 'isce', 'isce.slc', 1, 350 )

    return tst.testOpen( )

###############################################################################
# Test reading of metadata from the ISCE metadata domain 

def isce_2():

    ds = gdal.Open( 'data/isce.slc' )
    val = ds.GetMetadataItem( 'IMAGE_TYPE', 'ISCE' )
    if val != 'slc':
        return 'fail'

    return 'success'

###############################################################################
# Verify this can be exported losslessly.

def isce_3():

    tst = gdaltest.GDALTest( 'isce', 'isce.slc', 1, 350 )
    return tst.testCreateCopy( check_gt = 0, new_filename = 'isce.tst.slc' )

###############################################################################
# Verify VSIF*L capacity

def isce_4():

    tst = gdaltest.GDALTest( 'isce', 'isce.slc', 1, 350 )
    return tst.testCreateCopy( check_gt = 0, new_filename = 'isce.tst.slc', vsimem = 1 )

gdaltest_list = [
    isce_1,
    isce_2,
    isce_3,
    isce_4,
    ]
  


if __name__ == '__main__':

    gdaltest.setup_run( 'isce' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

