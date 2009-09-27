#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for Rasterlite driver.
# Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2009, Even Rouault, <even dot rouault at mines dash paris dot org>
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
import ogr

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Get the rasterlite driver

def rasterlite_1():

    try:
        gdaltest.rasterlite_drv = gdal.GetDriverByName( 'RASTERLITE' )
    except:
        gdaltest.rasterlite_drv = None
        return 'skip'

    return 'success'

###############################################################################
# Test opening a rasterlite DB without overviews

def rasterlite_2():

    if gdaltest.rasterlite_drv is None:
        return 'skip'
    
    # Test if SQLite3 supports rtrees
    try:
        os.remove('tmp/testrtree.sqlite')
    except:
        pass
    ds2 = ogr.GetDriverByName('SQLite').CreateDataSource('tmp/testrtree.sqlite')
    gdal.ErrorReset()
    ds2.ExecuteSQL('CREATE VIRTUAL TABLE testrtree USING rtree(id,minX,maxX,minY,maxY)')
    ds2.Destroy()
    try:
        os.remove('tmp/testrtree.sqlite')
    except:
        pass
    if gdal.GetLastErrorMsg().find('rtree') != -1:
        gdaltest.rasterlite_drv = None
        gdaltest.post_reason('Please upgrade your sqlite3 library to be able to read Rasterlite DBs (needs rtree support)!')
        return 'skip'
        
    gdal.ErrorReset()
    ds = gdal.Open('data/rasterlite.sqlite')
    if ds is None:
        if gdal.GetLastErrorMsg().find('unsupported file format') != -1:
            gdaltest.rasterlite_drv = None
            gdaltest.post_reason('Please upgrade your sqlite3 library to be able to read Rasterlite DBs!')
            return 'skip'
        else:
            return 'fail'
    
            
    if ds.RasterCount != 3:
        gdaltest.post_reason('expected 3 bands')
        return 'fail'

    if ds.GetRasterBand(1).GetOverviewCount() != 0:
        gdaltest.post_reason('did not expect overview')
        return 'fail'
        
    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 11746
    if cs != expected_cs:
        gdaltest.post_reason('for band 1, cs = %d, different from expected_cs = %d' % (cs, expected_cs))
        return 'fail'

    cs = ds.GetRasterBand(2).Checksum()
    expected_cs = 19843
    if cs != expected_cs:
        gdaltest.post_reason('for band 2, cs = %d, different from expected_cs = %d' % (cs, expected_cs))
        return 'fail'

    cs = ds.GetRasterBand(3).Checksum()
    expected_cs = 48911
    if cs != expected_cs:
        gdaltest.post_reason('for band 3, cs = %d, different from expected_cs = %d' % (cs, expected_cs))
        return 'fail'
        
    if ds.GetProjectionRef().find('WGS_1984') == -1:
        gdaltest.post_reason('projection_ref = %s' % ds.GetProjectionRef())
        return 'fail'
        
    gt = ds.GetGeoTransform()
    expected_gt = ( -180.0, 360. / ds.RasterXSize, 0.0, 90.0, 0.0, -180. / ds.RasterYSize)
    for i in range(6):
        if abs(gt[i] - expected_gt[i]) > 1e-15:
            print gt
            print expected_gt
            return 'fail'
        
    ds = None

    return 'success'

###############################################################################
# Test opening a rasterlite DB with overviews

def rasterlite_3():

    if gdaltest.rasterlite_drv is None:
        return 'skip'
        
    ds = gdal.Open('RASTERLITE:data/rasterlite_pyramids.sqlite,table=test')
    
    if ds.RasterCount != 3:
        gdaltest.post_reason('expected 3 bands')
        return 'fail'
        
    if ds.GetRasterBand(1).GetOverviewCount() != 1:
        gdaltest.post_reason('expected 1 overview')
        return 'fail'

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs = 59551
    if cs != expected_cs:
        gdaltest.post_reason('for overview of band 1, cs = %d, different from expected_cs = %d' % (cs, expected_cs))
        return 'fail'

    cs = ds.GetRasterBand(2).GetOverview(0).Checksum()
    expected_cs = 59603
    if cs != expected_cs:
        gdaltest.post_reason('for overview of band 2, cs = %d, different from expected_cs = %d' % (cs, expected_cs))
        return 'fail'

    cs = ds.GetRasterBand(3).GetOverview(0).Checksum()
    expected_cs = 42173
    if cs != expected_cs:
        gdaltest.post_reason('for overview of band 3, cs = %d, different from expected_cs = %d' % (cs, expected_cs))
        return 'fail'
        
    ds = None
        
    return 'success'
    
###############################################################################
# Test opening a rasterlite DB with color table and user-defined spatial extent

def rasterlite_4():

    if gdaltest.rasterlite_drv is None:
        return 'skip'
        
    ds = gdal.Open('RASTERLITE:data/rasterlite_pct.sqlite,minx=0,miny=0,maxx=180,maxy=90')
    
    if ds.RasterCount != 1:
        gdaltest.post_reason('expected 1 band')
        return 'fail'
        
    if ds.RasterXSize != 169 or ds.RasterYSize != 85:
        print ds.RasterXSize
        print ds.RasterYSize
        return 'fail'
        
    ct = ds.GetRasterBand(1).GetRasterColorTable()
    if ct is None:
        gdaltest.post_reason('did not get color table')
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 36473
    if cs != expected_cs:
        gdaltest.post_reason('for band 1, cs = %d, different from expected_cs = %d' % (cs, expected_cs))
        return 'fail'
        
    ds = None
        
    return 'success'
    
###############################################################################
# Test opening a rasterlite DB with color table and do color table expansion

def rasterlite_5():

    if gdaltest.rasterlite_drv is None:
        return 'skip'
        
    ds = gdal.Open('RASTERLITE:data/rasterlite_pct.sqlite,bands=3')
    
    if ds.RasterCount != 3:
        gdaltest.post_reason('expected 3 bands')
        return 'fail'
        
    ct = ds.GetRasterBand(1).GetRasterColorTable()
    if ct is not None:
        gdaltest.post_reason('did not expect color table')
        return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 506
    if cs != expected_cs:
        gdaltest.post_reason('for band 1, cs = %d, different from expected_cs = %d' % (cs, expected_cs))
        return 'fail'

    cs = ds.GetRasterBand(2).Checksum()
    expected_cs = 3842
    if cs != expected_cs:
        gdaltest.post_reason('for band 2, cs = %d, different from expected_cs = %d' % (cs, expected_cs))
        return 'fail'

    cs = ds.GetRasterBand(3).Checksum()
    expected_cs = 59282
    if cs != expected_cs:
        gdaltest.post_reason('for band 3, cs = %d, different from expected_cs = %d' % (cs, expected_cs))
        return 'fail'

    ds = None
        
    return 'success'
    
gdaltest_list = [
    rasterlite_1,
    rasterlite_2,
    rasterlite_3,
    rasterlite_4,
    rasterlite_5 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'rasterlite' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

