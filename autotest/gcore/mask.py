#!/usr/bin/env python
###############################################################################
# $Id: pam_md.py 11065 2007-03-24 09:35:32Z mloskot $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test RFC 15 "mask band" default functionality (nodata/alpha/etc)
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
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
import gdal
import gdalconst

###############################################################################
# Verify the checksum and flags for "all valid" case.

def mask_1():

    try:
        x = gdalconst.GMF_ALL_VALID
        gdaltest.have_ng = 1
    except:
        gdaltest.have_ng = 0
        return 'skip'

    ds = gdal.Open('data/byte.tif')
    band = ds.GetRasterBand(1)

    if band.GetMaskFlags() != gdal.GMF_ALL_VALID:
        gdaltest.post_reason( 'Did not get expected mask.' )
        return 'fail'

    cs = band.GetMaskBand().Checksum()
    if cs != 4873:
        gdaltest.post_reason( 'Got wrong mask checksum' )
        print cs
        return 'fail'
    
    return 'success' 

###############################################################################
# Verify the checksum and flags for "nodata" case.

def mask_2():

    if gdaltest.have_ng == 0:
        return 'skip'

    ds = gdal.Open('data/byte.vrt')
    band = ds.GetRasterBand(1)

    if band.GetMaskFlags() != gdal.GMF_NODATA:
        gdaltest.post_reason( 'Did not get expected mask.' )
        return 'fail'

    cs = band.GetMaskBand().Checksum()
    if cs != 4209:
        gdaltest.post_reason( 'Got wrong mask checksum' )
        print cs
        return 'fail'
    
    return 'success' 

###############################################################################
# Verify the checksum and flags for "alpha" case.

def mask_3():

    if gdaltest.have_ng == 0:
        return 'skip'

    ds = gdal.Open('data/stefan_full_rgba.png')

    # Test first mask.
    
    band = ds.GetRasterBand(1)

    if band.GetMaskFlags() != gdal.GMF_ALPHA + gdal.GMF_PER_DATASET:
        gdaltest.post_reason( 'Did not get expected mask.' )
        return 'fail'

    cs = band.GetMaskBand().Checksum()
    if cs != 10807:
        gdaltest.post_reason( 'Got wrong mask checksum' )
        print cs
        return 'fail'

    # Verify second and third same as first.

    band_2 = ds.GetRasterBand(2)
    band_3 = ds.GetRasterBand(3)
    
    if band_2.GetMaskFlags() != band.GetMaskFlags() \
       or band_3.GetMaskFlags() != band.GetMaskFlags() \
       or str(band_2.GetMaskBand()) != str(band.GetMaskBand()) \
       or str(band_3.GetMaskBand()) != str(band.GetMaskBand()):
        gdaltest.post_reason( 'Band 2 or 3 does not seem to match first mask')
        return 'fail'

    # Verify alpha has no mask.
    band = ds.GetRasterBand(4)
    if band.GetMaskFlags() != gdal.GMF_ALL_VALID:
        gdaltest.post_reason( 'Did not get expected mask for alpha.' )
        return 'fail'

    cs = band.GetMaskBand().Checksum()
    if cs != 36074:
        gdaltest.post_reason( 'Got wrong alpha mask checksum' )
        print cs
        return 'fail'
    
    return 'success' 

###############################################################################
# Cleanup.


gdaltest_list = [
    mask_1,
    mask_2,
    mask_3 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'mask' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

