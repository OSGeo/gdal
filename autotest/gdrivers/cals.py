#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test CALS driver
# Author:   Even Rouault, <even dot rouault at spatialys dot com>
# 
###############################################################################
# Copyright (c) 2015, Even Rouault, <even dot rouault at spatialys dot com>
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

sys.path.append( '../pymod' )

from osgeo import gdal
import gdaltest

###############################################################################
# Source has no color table

def cals_1():

    tst = gdaltest.GDALTest( 'CALS', 'small1bit.img', 1, 9907 )

    return tst.testCreateCopy()

###############################################################################
# Source has a color table (0,0,0),(255,255,255)

def cals_2():

    # Has no color table
    tst = gdaltest.GDALTest( 'CALS', '../../gcore/data/oddsize1bit.tif', 1, 3883 )

    return tst.testCreateCopy()

###############################################################################
# Source has a color table (255,255,255),(0,0,0)

def cals_3():

    src_ds = gdal.Open('../gcore/data/oddsize1bit.tif')
    tmp_ds = gdal.GetDriverByName('CALS').CreateCopy('/vsimem/cals_2_tmp.cal', src_ds)
    tmp_ds.SetMetadataItem('TIFFTAG_XRESOLUTION', '600')
    tmp_ds.SetMetadataItem('TIFFTAG_YRESOLUTION', '600')
    out_ds = gdal.GetDriverByName('CALS').CreateCopy('/vsimem/cals_2.cal', tmp_ds)
    if gdal.VSIStatL('/vsimem/cals_2.cal.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if out_ds.GetRasterBand(1).Checksum() != 3883:
        gdaltest.post_reason('fail')
        return 'fail'
    if out_ds.GetMetadataItem('PIXEL_PATH') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if out_ds.GetMetadataItem('TIFFTAG_XRESOLUTION') != '600':
        gdaltest.post_reason('fail')
        return 'fail'
    if out_ds.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_PaletteIndex:
        gdaltest.post_reason('fail')
        return 'fail'
    tmp_ds = None
    out_ds = None
    gdal.Unlink('/vsimem/cals_2_tmp.cal')
    gdal.Unlink('/vsimem/cals_2.cal')

    return 'success'

###############################################################################
# Test CreateCopy() error conditions

def cals_4():
    
    # 0 band
    src_ds = gdal.GetDriverByName('MEM').Create('',1,1,0)
    gdal.PushErrorHandler()
    out_ds = gdal.GetDriverByName('CALS').CreateCopy('/vsimem/cals_4.cal', src_ds)
    gdal.PopErrorHandler()
    if out_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # 2 bands
    src_ds = gdal.GetDriverByName('MEM').Create('',1,1,2)
    gdal.PushErrorHandler()
    out_ds = gdal.GetDriverByName('CALS').CreateCopy('/vsimem/cals_4.cal', src_ds, strict = True)
    gdal.PopErrorHandler()
    if out_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # 1 band but not 1-bit
    src_ds = gdal.GetDriverByName('MEM').Create('',1,1,1)
    gdal.PushErrorHandler()
    out_ds = gdal.GetDriverByName('CALS').CreateCopy('/vsimem/cals_4.cal', src_ds, strict = True)
    gdal.PopErrorHandler()
    if out_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Dimension > 999999
    src_ds = gdal.GetDriverByName('MEM').Create('',1000000,1,1)
    src_ds.GetRasterBand(1).SetMetadataItem('NBITS', '1', 'IMAGE_STRUCTURE')
    gdal.PushErrorHandler()
    out_ds = gdal.GetDriverByName('CALS').CreateCopy('/vsimem/cals_4.cal', src_ds, strict = True)
    gdal.PopErrorHandler()
    if out_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Invalid output filename
    src_ds = gdal.GetDriverByName('MEM').Create('',1,1,1)
    src_ds.GetRasterBand(1).SetMetadataItem('NBITS', '1', 'IMAGE_STRUCTURE')
    gdal.PushErrorHandler()
    out_ds = gdal.GetDriverByName('CALS').CreateCopy('/not_existing_dir/cals_4.cal', src_ds)
    gdal.PopErrorHandler()
    if out_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test PIXEL_PATH & LINE_PROGRESSION metadata item

def cals_5():

    src_ds = gdal.GetDriverByName('MEM').Create('',1,1,1)
    src_ds.GetRasterBand(1).SetMetadataItem('NBITS', '1', 'IMAGE_STRUCTURE')
    src_ds.SetMetadataItem('PIXEL_PATH', '90')
    src_ds.SetMetadataItem('LINE_PROGRESSION', '270')
    out_ds = gdal.GetDriverByName('CALS').CreateCopy('/vsimem/cals_5.cal', src_ds)
    if gdal.VSIStatL('/vsimem/cals_5.cal.aux.xml') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if out_ds.GetMetadataItem('PIXEL_PATH') != '90':
        gdaltest.post_reason('fail')
        return 'fail'
    if out_ds.GetMetadataItem('LINE_PROGRESSION') != '270':
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None
    gdal.Unlink('/vsimem/cals_5.cal')

    return 'success'

###############################################################################

gdaltest_list = [
    cals_1,
    cals_2,
    cals_3,
    cals_4,
    cals_5,
]
  
if __name__ == '__main__':

    gdaltest.setup_run( 'cals' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

