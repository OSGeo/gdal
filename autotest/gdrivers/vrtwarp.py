#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test VRTWarpedDataset support.
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

import os
import sys
import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Verify reading from simple existing warp definition.

def vrtwarp_1():

    tst = gdaltest.GDALTest( 'VRT', 'rgb_warp.vrt', 2, 21504 )
    return tst.testOpen()

###############################################################################
# Create a new VRT warp in the temp directory.

def vrtwarp_2():

    try:
        os.remove( 'tmp/warp.vrt' )
    except:
        pass
    
    gcp_ds = gdal.OpenShared( 'data/rgb_gcp.vrt', gdal.GA_ReadOnly )

    gdaltest.vrtwarp_ds = gdal.AutoCreateWarpedVRT( gcp_ds )

    gcp_ds = None
    
    checksum = gdaltest.vrtwarp_ds.GetRasterBand(2).Checksum()
    expected = 21504
    if checksum != expected:
        gdaltest.post_reason( 'Got checksum of %d instead of expected %d.' \
                              % (checksum, expected) )
        return 'fail'
        
    return 'success'

###############################################################################
# Force the VRT warp file to be written to disk and close it.  Reopen, and
# verify checksum.

def vrtwarp_3():

    gdaltest.vrtwarp_ds.SetDescription( 'tmp/warp.vrt' )
    gdaltest.vrtwarp_ds = None

    gdaltest.vrtwarp_ds = gdal.Open( 'tmp/warp.vrt', gdal.GA_ReadOnly )

    checksum = gdaltest.vrtwarp_ds.GetRasterBand(2).Checksum()
    expected = 21504

    gdaltest.vrtwarp_ds = None
    gdal.GetDriverByName('VRT').Delete( 'tmp/warp.vrt' )

    if checksum != expected:
        gdaltest.post_reason( 'Got checksum of %d instead of expected %d.' \
                              % (checksum, expected) )
        return 'fail'

    return 'success'

gdaltest_list = [
    vrtwarp_1,
    vrtwarp_2,
    vrtwarp_3 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'vrtwarp' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

