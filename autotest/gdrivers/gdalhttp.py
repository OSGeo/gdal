#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test HTTP Driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2012, Even Rouault <even dot rouault at mines-paris dot org>
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
from osgeo import gdal
from osgeo import ogr

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Verify we have the driver.

def http_1():

    gdaltest.dods_drv = None

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    try:
        gdaltest.dods_drv = gdal.GetDriverByName( 'DODS' )
        if gdaltest.dods_drv is not None:
            gdaltest.dods_drv.Deregister()
    except:
        gdaltest.dods_drv = None

    tst = gdaltest.GDALTest( 'PNG','http://gdal.org/gdalicon.png',
                             1, 7617, filename_absolute = 1 )
    ret = tst.testOpen()
    if ret == 'fail':
        conn = gdaltest.gdalurlopen('http://gdal.org/gdalicon.png')
        if conn is None:
            print('cannot open URL')
            return 'skip'
        conn.close()

    return ret

###############################################################################
# Verify /vsicurl (subversion file listing)

def http_2():

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'GTiff','/vsicurl/http://svn.osgeo.org/gdal/trunk/autotest/gcore/data/byte.tif',
                             1, 4672, filename_absolute = 1 )
    ret = tst.testOpen()
    if ret == 'fail':
        conn = gdaltest.gdalurlopen('http://svn.osgeo.org/gdal/trunk/autotest/gcore/data/byte.tif')
        if conn is None:
            print('cannot open URL')
            return 'skip'
        conn.close()

    return ret

###############################################################################
# Verify /vsicurl (apache file listing)

def http_3():

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    gdal.SetConfigOption('GDAL_HTTP_TIMEOUT', '5')
    ds = gdal.Open('/vsicurl/http://download.osgeo.org/gdal/data/ehdr/elggll.bil')
    gdal.SetConfigOption('GDAL_HTTP_TIMEOUT', None)
    if ds is None:
        conn = gdaltest.gdalurlopen('http://download.osgeo.org/gdal/data/ehdr/elggll.bil')
        if conn is None:
            print('cannot open URL')
            return 'skip'
        conn.close()
        return 'fail'

    return 'success'

###############################################################################
# Verify /vsicurl (ftp)

def http_4_old():

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    ds = gdal.Open('/vsicurl/ftp://ftp2.cits.rncan.gc.ca/pub/cantopo/250k_tif/MCR2010_01.tif')
    if ds is None:

        # Workaround unexplained failure on Tamas test machine. The test works fine with his
        # builds on other machines...
        # This heuristics might be fragile !
        if "GDAL_DATA" in os.environ and os.environ["GDAL_DATA"].find("E:\\builds\\..\\sdk\\") == 0:
            return 'skip'

        conn = gdaltest.gdalurlopen('ftp://ftp2.cits.rncan.gc.ca/pub/cantopo/250k_tif/MCR2010_01.tif')
        if conn is None:
            print('cannot open URL')
            return 'skip'
        conn.close()
        return 'fail'

    filelist = ds.GetFileList()
    if filelist[0] != '/vsicurl/ftp://ftp2.cits.rncan.gc.ca/pub/cantopo/250k_tif/MCR2010_01.tif':
        print(filelist)
        return 'fail'

    return 'success'

###############################################################################
# Verify /vsicurl (ftp)

def http_4():

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    ds = gdal.Open('/vsicurl/ftp://download.osgeo.org/gdal/data/gtiff/utm.tif')
    if ds is None:
        conn = gdaltest.gdalurlopen('ftp://download.osgeo.org/gdal/data/gtiff/utm.tif')
        if conn is None:
            print('cannot open URL')
            return 'skip'
        try:
            conn.read()
        except:
            print('cannot read')
            return 'skip'
        conn.close()
        if sys.platform == 'darwin' and gdal.GetConfigOption('TRAVIS', None) is not None:
            print("Fails on MacOSX Travis sometimes. Not sure why.")
            return 'skip'
        gdaltest.post_reason('fail')
        return 'fail'

    filelist = ds.GetFileList()
    if '/vsicurl/ftp://download.osgeo.org/gdal/data/gtiff/utm.tif' not in filelist:
        print(filelist)
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test HTTP driver with non VSIL driver

def http_5():

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    ds = gdal.Open('http://svn.osgeo.org/gdal/trunk/autotest/gdrivers/data/s4103.blx')
    if ds is None:
        conn = gdaltest.gdalurlopen('http://svn.osgeo.org/gdal/trunk/autotest/gdrivers/data/s4103.blx')
        if conn is None:
            print('cannot open URL')
            return 'skip'
        try:
            conn.read()
        except:
            print('cannot read')
            return 'skip'
        conn.close()
        gdaltest.post_reason('fail')
        return 'fail'
    filename = ds.GetDescription()
    ds = None

    try:
        os.stat(filename)
        gdaltest.post_reason('file %s should have been removed' % filename)
        return 'fail'
    except:
        pass

    return 'success'

###############################################################################
# Test HTTP driver with OGR driver

def http_6():

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    ds = ogr.Open('http://svn.osgeo.org/gdal/trunk/autotest/ogr/data/test.jml')
    if ds is None:
        conn = gdaltest.gdalurlopen('http://svn.osgeo.org/gdal/trunk/autotest/ogr/data/test.jml')
        if conn is None:
            print('cannot open URL')
            return 'skip'
        try:
            conn.read()
        except:
            print('cannot read')
            return 'skip'
        conn.close()
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    return 'success'

###############################################################################
#

def http_cleanup():
    if gdaltest.dods_drv is not None:
        gdaltest.dods_drv.Register()
    gdaltest.dods_drv = None

    return 'success'

gdaltest_list = [ http_1,
                  http_2,
                  http_3,
                  #http_4_old,
                  http_4,
                  http_5,
                  http_6,
                  http_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'http' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

