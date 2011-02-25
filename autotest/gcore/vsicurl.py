#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsicurl
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at mines dash paris dot org>
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
#

def vsicurl_1():

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    ds = ogr.Open('/vsizip/vsicurl/http://publicfiles.dep.state.fl.us/dear/BWR_GIS/2007NWFLULC/NWFWMD2007LULC.zip')
    if ds is None:
        return 'fail'

    return 'success'

###############################################################################
#

def vsicurl_2():

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    ds = gdal.Open('/vsizip//vsicurl/http://eros.usgs.gov/archive/nslrsda/GeoTowns/HongKong/srtm/n22e113.zip/n22e113.bil')
    if ds is None:
        return 'fail'

    return 'success'

###############################################################################
# This server doesn't support range downloading

def vsicurl_3():

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    ds = ogr.Open('/vsizip/vsicurl/http://www.iucnredlist.org/spatial-data/MAMMALS_TERRESTRIAL.zip')
    if ds is not None:
        return 'fail'

    return 'success'

###############################################################################
# This server doesn't support range downloading

def vsicurl_4():

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    ds = ogr.Open('/vsizip/vsicurl/http://lelserver.env.duke.edu:8080/LandscapeTools/export/49/Downloads/1_Habitats.zip')
    if ds is not None:
        return 'fail'

    return 'success'

###############################################################################
# Test URL unescaping when reading HTTP file list

def vsicurl_5():

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    ds = gdal.Open('/vsicurl/http://dds.cr.usgs.gov/srtm/SRTM_image_sample/picture%20examples/N34W119_DEM.tif')
    if ds is None:
        return 'fail'

    return 'success'

###############################################################################
# Test with FTP server that doesn't support EPSV command 

def vsicurl_6():

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    fl = gdal.ReadDir('/vsicurl/ftp://ftp2.cits.rncan.gc.ca/pub/cantopo/250k_tif')
    if len(fl) == 0:
        return 'fail'

    return 'success'


###############################################################################
# Test Microsoft-IIS/6.0 listing

def vsicurl_7():

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    fl = gdal.ReadDir('/vsicurl/http://ortho.linz.govt.nz/tifs/2005_06')
    if len(fl) == 0:
        return 'fail'

    return 'success'

###############################################################################
# Test interleaved reading between 2 datasets

def vsicurl_8():

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    ds1 = gdal.Open('/vsigzip//vsicurl/http://dds.cr.usgs.gov/pub/data/DEM/250/notavail/C/chipicoten-w.gz')
    ds2 = gdal.Open('/vsizip//vsicurl/http://edcftp.cr.usgs.gov/pub/data/landcover/files/2009/biso/gokn09b_dnbr.zip/nps-serotnbsp-9001-20090321_rd.tif')
    cs = ds1.GetRasterBand(1).Checksum()
    if cs != 61342:
        return 'fail'

    return 'success'

# Not run by run_all.py
my_gdaltest_list = [ vsicurl_1,
                     vsicurl_2,
                     vsicurl_3,
                     vsicurl_4,
                     vsicurl_5,
                     vsicurl_6,
                     vsicurl_7,
                     vsicurl_8 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'vsicurl' )

    gdaltest.run_tests( my_gdaltest_list )

    gdaltest.summarize()

