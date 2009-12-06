#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test gdal.ComputeMedianCutPCT() and gdal.DitherRGB2PCT()
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
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

try:
    from osgeo import gdal, gdalconst
except:
    import gdal
    import gdalconst

###############################################################################
# Test

def dither_1():

    drv = gdal.GetDriverByName( 'GTiff' )

    src_ds = gdal.Open('../gdrivers/data/rgbsmall.tif')
    r_band = src_ds.GetRasterBand(1)
    g_band = src_ds.GetRasterBand(2)
    b_band = src_ds.GetRasterBand(3)

    dst_ds = drv.Create('tmp/rgbsmall.tif', src_ds.RasterXSize, src_ds.RasterYSize, 1, gdal.GDT_Byte )
    dst_band = dst_ds.GetRasterBand(1)

    ct = gdal.ColorTable()

    nColors = 8

    gdal.ComputeMedianCutPCT(r_band, g_band, b_band, nColors, ct)

    dst_band.SetRasterColorTable( ct )

    gdal.DitherRGB2PCT( r_band, g_band, b_band, dst_band, ct)

    cs_expected = 8803
    cs = dst_band.Checksum()
    dst_band = None
    dst_ds = None

    if ct.GetCount() != nColors:
        gdaltest.post_reason( 'color table size wrong' )
        return 'fail'

    ref_ct = [ (36,48,32,255), (92,120,20,255), (88,96,20,255), (92,132,56,255),
               (0,0,0,255), (96,152,24,255), (60,112,32,255), (164,164,108,255) ]

    for i in range(nColors):
        ct_data = ct.GetColorEntry( i )
        ref_data = ref_ct[i]

        for j in range(4):

            if ct_data[j] != ref_data[j]:
                gdaltest.post_reason( 'color table mismatch' )
                for k in range(nColors):
                    print(ct.GetColorEntry( k ))
                    print(ref_ct[k])
                return 'fail'

    if cs == cs_expected \
       or gdal.GetConfigOption( 'CPL_DEBUG', 'OFF' ) != 'ON':
        drv.Delete( 'tmp/rgbsmall.tif' )

    if cs != cs_expected:
        print('Got: ', cs)
        gdaltest.post_reason( 'got wrong checksum' )
        return 'fail'
    else:
        return 'success' 

gdaltest_list = [
    dither_1
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'dither' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

