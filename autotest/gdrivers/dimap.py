#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test SPOT DIMAP driver.
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
import shutil
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Open and verify a the GCPs and metadata.

def dimap_1():
    
    shutil.copy('data/METADATA.DIM', 'tmp')
    shutil.copy('data/IMAGERY.TIF', 'tmp')
    shutil.copy('data/rgbsmall.tif', 'tmp')

    ds = gdal.Open( 'tmp/METADATA.DIM' )

    if ds.RasterCount != 1 \
       or ds.RasterXSize != 6000 \
       or ds.RasterYSize != 6000:
        gdaltest.post_reason ( 'wrong size or bands' )
        return 'fail'
        
    if ds.GetRasterBand(1).Checksum(0,0,100,100) != 21586:
        gdaltest.post_reason ( 'wrong checksum' )
        return 'fail'

    md = ds.GetMetadata()
    if md['PROCESSING_LEVEL'] != '1A':
        gdaltest.post_reason( 'metadata wrong.' )
        return 'fail'
    
    md = ds.GetMetadata()
    if md['SPECTRAL_PHYSICAL_BIAS'] != '0.000000':
        gdaltest.post_reason( 'metadata wrong.' )
        return 'fail'
    
    gcp_srs = ds.GetGCPProjection()
    if gcp_srs[:6] != 'GEOGCS' \
       or gcp_srs.find('WGS') == -1 \
       or gcp_srs.find('84') == -1:
        gdaltest.post_reason('GCP Projection not retained.')
        print(gcp_srs)
        return 'fail'

    gcps = ds.GetGCPs()
    if len(gcps) != 4 \
       or gcps[0].GCPPixel != 0.5 \
       or gcps[0].GCPLine  != 0.5 \
       or abs(gcps[0].GCPX - 4.3641728) > 0.0000002 \
       or abs(gcps[0].GCPY - 44.2082255) > 0.0000002 \
       or abs(gcps[0].GCPZ - 0) > 0.0000002:
        gdaltest.post_reason( 'GCPs wrong.' )
        print(len(gcps))
        print(gcps[0])
        return 'fail'

    ds = None
    os.unlink('tmp/METADATA.DIM')
    os.unlink('tmp/IMAGERY.TIF')
    os.unlink('tmp/rgbsmall.tif')

    return 'success'

gdaltest_list = [
    dimap_1 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'dimap' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

