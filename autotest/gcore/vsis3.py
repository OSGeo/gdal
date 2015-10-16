#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsis3
# Author:   Even Rouault <even dot rouault at spatialys dot com>
# 
###############################################################################
# Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
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
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest
import webserver

###############################################################################
def vsis3_init():

    gdaltest.aws_vars = {}
    for var in ('AWS_SECRET_ACCESS_KEY', 'AWS_ACCESS_KEY_ID', 'AWS_TIMESTAMP', 'AWS_HTTPS', 'AWS_VIRTUAL_HOSTING', 'AWS_S3_ENDPOINT'):
        gdaltest.aws_vars[var] = gdal.GetConfigOption(var)
        if gdaltest.aws_vars[var] is not None:
            gdal.SetConfigOption(var, "")

    return 'success'

###############################################################################
# Error cases

def vsis3_1():
    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    # Missing AWS_SECRET_ACCESS_KEY
    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = gdal.VSIFOpenL('/vsis3/foo/bar', 'rb')
    if f is not None or gdal.GetLastErrorMsg().find('AWS_SECRET_ACCESS_KEY') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'
        
    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = gdal.VSIFOpenL('/vsis3_streaming/foo/bar', 'rb')
    if f is not None or gdal.GetLastErrorMsg().find('AWS_SECRET_ACCESS_KEY') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'
        
    gdal.SetConfigOption('AWS_SECRET_ACCESS_KEY', 'AWS_SECRET_ACCESS_KEY')

    # Missing AWS_ACCESS_KEY_ID
    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = gdal.VSIFOpenL('/vsis3/foo/bar', 'rb')
    if f is not None or gdal.GetLastErrorMsg().find('AWS_ACCESS_KEY_ID') < 0:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    gdal.SetConfigOption('AWS_ACCESS_KEY_ID', 'AWS_ACCESS_KEY_ID')

    # ERROR 1: The AWS Access Key Id you provided does not exist in our records.
    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = gdal.VSIFOpenL('/vsis3/foo/bar.baz', 'rb')
    if f is not None or gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = gdal.VSIFOpenL('/vsis3_streaming/foo/bar.baz', 'rb')
    if f is not None or gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
def vsis3_start_webserver():

    gdaltest.webserver_process = None
    gdaltest.webserver_port = 0

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    (gdaltest.webserver_process, gdaltest.webserver_port) = webserver.launch()
    if gdaltest.webserver_port == 0:
        return 'skip'

    return 'success'

###############################################################################
# Test with a fake AWS server

def vsis3_2():

    if gdaltest.webserver_port == 0:
        return 'skip'
        
    gdal.SetConfigOption('AWS_SECRET_ACCESS_KEY', 'AWS_SECRET_ACCESS_KEY')
    gdal.SetConfigOption('AWS_ACCESS_KEY_ID', 'AWS_ACCESS_KEY_ID')
    gdal.SetConfigOption('AWS_TIMESTAMP', '20150101T000000Z')
    gdal.SetConfigOption('AWS_HTTPS', 'NO')
    gdal.SetConfigOption('AWS_VIRTUAL_HOSTING', 'NO')
    gdal.SetConfigOption('AWS_S3_ENDPOINT', '127.0.0.1:%d' % gdaltest.webserver_port)

    f = gdal.VSIFOpenL('/vsis3/s3_fake_bucket/resource', 'rb')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    data = gdal.VSIFReadL(1, 4, f).decode('ascii')
    gdal.VSIFCloseL(f)
    
    if data != 'foo':
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    f = gdal.VSIFOpenL('/vsis3_streaming/s3_fake_bucket/resource', 'rb')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    data = gdal.VSIFReadL(1, 4, f).decode('ascii')
    gdal.VSIFCloseL(f)
    
    if data != 'foo':
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    #old_val = gdal.GetConfigOption('GDAL_DISABLE_READDIR_ON_OPEN')
    #gdal.SetConfigOption('GDAL_DISABLE_READDIR_ON_OPEN', 'EMPTY_DIR')
    stat_res = gdal.VSIStatL('/vsis3/s3_fake_bucket/resource2.bin')
    #gdal.SetConfigOption('GDAL_DISABLE_READDIR_ON_OPEN', old_val)
    if stat_res is None or stat_res.size != 1000000:
        gdaltest.post_reason('fail')
        if stat_res is not None:
            print(stat_res.size)
        else:
            print(stat_res)
        return 'fail'

    stat_res = gdal.VSIStatL('/vsis3_streaming/s3_fake_bucket/resource2.bin')
    if stat_res is None or stat_res.size != 1000000:
        gdaltest.post_reason('fail')
        if stat_res is not None:
            print(stat_res.size)
        else:
            print(stat_res)
        return 'fail'

    # Test region and endpoint 'redirects'
    f = gdal.VSIFOpenL('/vsis3/s3_fake_bucket/redirect', 'rb')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    data = gdal.VSIFReadL(1, 4, f).decode('ascii')
    gdal.VSIFCloseL(f)
    
    if data != 'foo':
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    # Test region and endpoint 'redirects'
    f = gdal.VSIFOpenL('/vsis3_streaming/s3_fake_bucket/redirect', 'rb')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    data = gdal.VSIFReadL(1, 4, f).decode('ascii')
    gdal.VSIFCloseL(f)
    
    if data != 'foo':
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    return 'success'

###############################################################################
def vsis3_stop_webserver():

    if gdaltest.webserver_port == 0:
        return 'skip'

    webserver.server_stop(gdaltest.webserver_process, gdaltest.webserver_port)

    return 'success'

###############################################################################
# Nominal cases (require valid credentials)

def vsis3_extra_1():
    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    if gdal.GetConfigOption('AWS_SECRET_ACCESS_KEY') is None:
        print('Missing AWS_SECRET_ACCESS_KEY for running gdaltest_list_extra')
        return 'skip'
    elif gdal.GetConfigOption('AWS_ACCESS_KEY_ID') is None:
        print('Missing AWS_ACCESS_KEY_ID for running gdaltest_list_extra')
        return 'skip'
    elif gdal.GetConfigOption('S3_RESOURCE') is None:
        print('Missing S3_RESOURCE for running gdaltest_list_extra')
        return 'skip'

    f = gdal.VSIFOpenL('/vsis3/' + gdal.GetConfigOption('S3_RESOURCE'), 'rb')
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)
    
    if len(ret) != 1:
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'

    # Same with /vsis3_streaming/
    f = gdal.VSIFOpenL('/vsis3_streaming/' + gdal.GetConfigOption('S3_RESOURCE'), 'rb')
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)
    
    if len(ret) != 1:
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'

    # Invalid bucket : "The specified bucket does not exist"
    gdal.ErrorReset()
    f = gdal.VSIFOpenL('/vsis3/not_existing_bucket/foo', 'rb')
    with gdaltest.error_handler():
        gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    # Invalid resource
    gdal.ErrorReset()
    f = gdal.VSIFOpenL('/vsis3_streaming/' + gdal.GetConfigOption('S3_RESOURCE') + '/invalid_resource.baz', 'rb')
    if f is not None:
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
def vsis3_cleanup():

    for var in gdaltest.aws_vars:
        gdal.SetConfigOption(var, gdaltest.aws_vars[var])

    return 'success'

gdaltest_list = [ vsis3_init,
                  vsis3_1,
                  vsis3_start_webserver,
                  vsis3_2,
                  vsis3_stop_webserver,
                  vsis3_cleanup ]
gdaltest_list_extra = [ vsis3_extra_1, vsis3_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'vsis3' )

    gdaltest.run_tests( gdaltest_list + gdaltest_list_extra )

    gdaltest.summarize()

