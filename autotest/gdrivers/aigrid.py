#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for AIGRID driver.
# Author:   Swapnil Hajare <dreamil@gmail.com>
# 
###############################################################################
# Copyright (c) 2006, Swapnil Hajare <dreamil@gmail.com>
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
# Read test of simple byte reference data.

def aigrid_1():

    tst = gdaltest.GDALTest( 'AIG', 'abc3x1', 1, 3 )
    return tst.testOpen()

###############################################################################
# Verify some auxilary data. 

def aigrid_2():

    ds = gdal.Open( 'data/abc3x1/prj.adf' )

    gt = ds.GetGeoTransform()

    if gt[0] != -0.5 or gt[1] != 1.0 or gt[2] != 0.0 \
       or gt[3] != 0.5 or gt[4] != 0.0 or gt[5] != -1.0:
        gdaltest.post_reason( 'Aigrid geotransform wrong.' )
        return 'fail'

    prj = ds.GetProjection()
    if prj.find('PROJCS["UTM Zone 55, Southern Hemisphere",GEOGCS["GDA94",DATUM["Geocentric_Datum_of_Australia_1994"') == -1:
        gdaltest.post_reason( 'Projection does not match expected:\n%s' % prj )
        return 'fail'

    band1 = ds.GetRasterBand(1)
    if band1.GetNoDataValue() != 255:
        gdaltest.post_reason( 'Grid NODATA value wrong or missing.' )
        return 'fail'

    if band1.DataType != gdal.GDT_Byte:
        gdaltest.post_reason( 'Data type is not Byte!' )
        return 'fail'

    return 'success'

###############################################################################
# Verify the colormap, and nodata setting for test file. 

def aigrid_3():

    ds = gdal.Open( 'data/abc3x1' )
    cm = ds.GetRasterBand(1).GetRasterColorTable()
    if cm.GetCount() != 256 \
       or cm.GetColorEntry(0) != (95, 113, 150, 255)\
       or cm.GetColorEntry(1) != (95, 57, 29, 255):
        gdaltest.post_reason( 'Wrong colormap entries' )
        return 'fail'

    cm = None

    if ds.GetRasterBand(1).GetNoDataValue() != 255.0:
        gdaltest.post_reason( 'Wrong nodata value.' )
        return 'fail'

    return 'success'
###############################################################################
# Read test of simple byte reference data with data directory name in all uppercase

def aigrid_4():

    tst = gdaltest.GDALTest( 'AIG', 'ABC3X1UC', 1, 3 )
    return tst.testOpen()
###############################################################################
# Verify the colormap, and nodata setting for test file with names of coverage directory and all files in it in all uppercase. Additionally also test for case where clr file resides in parent directory of coverage.

def aigrid_5():

    ds = gdal.Open( 'data/ABC3X1UC' )
    cm = ds.GetRasterBand(1).GetRasterColorTable()
    if cm.GetCount() != 256 \
       or cm.GetColorEntry(0) != (95, 113, 150, 255)\
       or cm.GetColorEntry(1) != (95, 57, 29, 255):
        gdaltest.post_reason( 'Wrong colormap entries' )
        return 'fail'

    cm = None

    if ds.GetRasterBand(1).GetNoDataValue() != 255.0:
        gdaltest.post_reason( 'Wrong nodata value.' )
        return 'fail'

    return 'success'    

###############################################################################
# Test on real dataset downloaded from http://download.osgeo.org/gdal/data/aig/nzdem

def aigrid_online_1():

    list_files = [ 'info/arc.dir',
                   'info/arc0000.dat',
                   'info/arc0000.nit',
                   'info/arc0001.dat',
                   'info/arc0001.nit',
                   'info/arc0002.dat',
                   'info/arc0002.nit',
                   'info/arc0002r.001',
                   'nzdem500/dblbnd.adf',
                   'nzdem500/hdr.adf',
                   'nzdem500/log',
                   'nzdem500/sta.adf',
                   'nzdem500/vat.adf',
                   'nzdem500/w001001.adf',
                   'nzdem500/w001001x.adf' ]

    try:
        os.mkdir('tmp/cache/nzdem')
        os.mkdir('tmp/cache/nzdem/info')
        os.mkdir('tmp/cache/nzdem/nzdem500')
    except:
        pass

    for filename in list_files:
        if not gdaltest.download_file('http://download.osgeo.org/gdal/data/aig/nzdem/' + filename , 'nzdem/' + filename):
            return 'skip'

    tst = gdaltest.GDALTest( 'AIG', 'tmp/cache/nzdem/nzdem500/hdr.adf', 1, 45334, filename_absolute = 1 )
    ret = tst.testOpen()
    if ret != 'success':
        return ret

    ds = gdal.Open('tmp/cache/nzdem/nzdem500/hdr.adf')

    try:
        rat = ds.GetRasterBand(1).GetDefaultRAT()
    except:
        print('Skipping RAT checking... OG Python bindings have no RAT API')
        return 'sucess'

    if rat is None:
        gdaltest.post_reason( 'No RAT found' )
        return 'fail'

    if rat.GetRowCount() != 2642:
        gdaltest.post_reason( 'Wrong row count in RAT' )
        return 'fail'

    if rat.GetColumnCount() != 2:
        gdaltest.post_reason( 'Wrong column count in RAT' )
        return 'fail'

    if rat.GetNameOfCol(0) != 'VALUE':
        gdaltest.post_reason( 'Wrong name of col 0' )
        return 'fail'

    if rat.GetTypeOfCol(0) != gdal.GFT_Integer:
        gdaltest.post_reason( 'Wrong type of col 0' )
        return 'fail'

    if rat.GetUsageOfCol(0) != gdal.GFU_MinMax:
        gdaltest.post_reason( 'Wrong usage of col 0' )
        return 'fail'

    if rat.GetNameOfCol(1) != 'COUNT':
        gdaltest.post_reason( 'Wrong name of col 1' )
        return 'fail'

    if rat.GetTypeOfCol(1) != gdal.GFT_Integer:
        gdaltest.post_reason( 'Wrong type of col 1' )
        return 'fail'

    if rat.GetUsageOfCol(1) != gdal.GFU_PixelCount:
        gdaltest.post_reason( 'Wrong usage of col 1' )
        return 'fail'

    if rat.GetValueAsInt(2641, 0) != 3627:
        gdaltest.post_reason( 'Wrong value in RAT' )
        return 'fail'

    if ds.GetRasterBand(1).GetMinimum() != 0.0:
        gdaltest.post_reason( 'Wrong minimum' )
        return 'fail'

    if ds.GetRasterBand(1).GetMaximum() != 3627.0:
        gdaltest.post_reason( 'Wrong maximum' )
        return 'fail'

    return 'success'

###############################################################################

gdaltest_list = [
    aigrid_1,
    aigrid_2,
    aigrid_3,
    aigrid_4,
    aigrid_5,
    aigrid_online_1 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'aigrid' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

