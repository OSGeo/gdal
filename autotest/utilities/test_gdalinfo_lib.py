#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  test librarified gdalinfo
# Author:   Faza Mahamood <fazamhd at gmail dot com>
# 
###############################################################################
# Copyright (c) 2015, Faza Mahamood <fazamhd at gmail dot com>
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
# Simple test

def test_gdalinfo_lib_1():

    ds = gdal.Open('../gcore/data/byte.tif')

    ret = gdal.Info(ds)
    if ret.find('Driver: GTiff/GeoTIFF') == -1:
        gdaltest.post_reason('did not get expected string.')
        print(ret)
        return 'fail'
    
    return 'success' 

###############################################################################
# Test Json format

def test_gdalinfo_lib_2():

    ds = gdal.Open('../gcore/data/byte.tif')

    ret = gdal.Info(ds, format = 'json')
    if ret['driverShortName'] != 'GTiff':
        gdaltest.post_reason('wrong value for driverShortName.')
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test extraMDDomains()

def test_gdalinfo_lib_3():

    ds = gdal.Open('../gdrivers/data/fake_nsif.ntf')

    ret = gdal.Info(ds, format = 'json')
    if 'TRE' in ret['metadata']:
        gdaltest.post_reason( 'unexpectingly got extra MD.' )
        print(ret)
        return 'fail'

    options = gdal.InfoOptions(format = 'json', extraMDDomains = ['TRE'])
    ret = gdal.Info(ds, options = options)
    if ret['metadata']['TRE']['BLOCKA'].find('010000001000000000') == -1:
        gdaltest.post_reason( 'did not get extra MD.' )
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test allMetadata

def test_gdalinfo_lib_4():

    ds = gdal.Open('../gdrivers/data/byte_with_xmp.tif')
    
    ret = gdal.Info(ds, allMetadata = True, format = 'json')
    if 'xml:XMP' not in ret['metadata']:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test all options

def test_gdalinfo_lib_5():

    ds = gdal.Open('../gdrivers/data/byte.tif')
    
    ret = gdal.Info(ds, format = 'json', deserialize = True, computeMinMax = True, \
                    reportHistograms = True, reportProj4 = True, \
                    stats = True, approxStats = True, computeChecksum = True, \
                    showGCPs = False, showMetadata = False, showRAT = False, \
                    showColorTable = False, listMDD = True, showFileList = False)
    if 'files' in ret:
        gdaltest.post_reason('fail')
        return 'fail'
    band = ret['bands'][0]
    if not 'computedMin' in band:
        gdaltest.post_reason('fail')
        return 'fail'
    if not 'histogram' in band:
        gdaltest.post_reason('fail')
        return 'fail'
    if not 'checksum' in band:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test command line syntax + dataset as string

def test_gdalinfo_lib_6():

    ret = gdal.Info('../gcore/data/byte.tif', options = '-json')
    if ret['driverShortName'] != 'GTiff':
        gdaltest.post_reason('wrong value for driverShortName.')
        print(ret)
        return 'fail'

    return 'success'
gdaltest_list = [
    test_gdalinfo_lib_1,
    test_gdalinfo_lib_2,
    test_gdalinfo_lib_3,
    test_gdalinfo_lib_4,
    test_gdalinfo_lib_5,
    test_gdalinfo_lib_6
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdalinfo_lib' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
