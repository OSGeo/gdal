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

import os.path
import sys
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest
import webserver

def open_for_read(uri):
    """
    Opens a test file for reading.
    """
    return gdal.VSIFOpenExL(uri, 'rb', 1)

###############################################################################
def vsis3_init():

    gdaltest.aws_vars = {}
    for var in ('AWS_SECRET_ACCESS_KEY', 'AWS_ACCESS_KEY_ID', 'AWS_TIMESTAMP', 'AWS_HTTPS', 'AWS_VIRTUAL_HOSTING', 'AWS_S3_ENDPOINT', 'AWS_REQUEST_PAYER', 'AWS_DEFAULT_REGION', 'AWS_DEFAULT_PROFILE'):
        gdaltest.aws_vars[var] = gdal.GetConfigOption(var)
        if gdaltest.aws_vars[var] is not None:
            gdal.SetConfigOption(var, "")

    # To avoid user AWS credentials in ~/.aws/credentials and ~/.aws/config
    # to mess up our tests
    gdal.SetConfigOption('CPL_AWS_CREDENTIALS_FILE', '')
    gdal.SetConfigOption('AWS_CONFIG_FILE', '')
    gdal.SetConfigOption('CPL_AWS_EC2_CREDENTIALS_URL', '')

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

        # RETODO: Bind to swig, change test

    # Missing AWS_SECRET_ACCESS_KEY
    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsis3/foo/bar')
    if f is not None or gdal.VSIGetLastErrorMsg().find('AWS_SECRET_ACCESS_KEY') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsis3_streaming/foo/bar')
    if f is not None or gdal.VSIGetLastErrorMsg().find('AWS_SECRET_ACCESS_KEY') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    gdal.SetConfigOption('AWS_SECRET_ACCESS_KEY', 'AWS_SECRET_ACCESS_KEY')

    # Missing AWS_ACCESS_KEY_ID
    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsis3/foo/bar')
    if f is not None or gdal.VSIGetLastErrorMsg().find('AWS_ACCESS_KEY_ID') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    gdal.SetConfigOption('AWS_ACCESS_KEY_ID', 'AWS_ACCESS_KEY_ID')

    # ERROR 1: The AWS Access Key Id you provided does not exist in our records.
    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsis3/foo/bar.baz')
    if f is not None or gdal.VSIGetLastErrorMsg() == '':
        if f is not None:
            gdal.VSIFCloseL(f)
        if gdal.GetConfigOption('APPVEYOR') is not None:
            return 'success'
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsis3_streaming/foo/bar.baz')
    if f is not None or gdal.VSIGetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
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

    f = open_for_read('/vsis3/s3_fake_bucket/resource')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    data = gdal.VSIFReadL(1, 4, f).decode('ascii')
    gdal.VSIFCloseL(f)

    if data != 'foo':
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    f = open_for_read('/vsis3_streaming/s3_fake_bucket/resource')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    data = gdal.VSIFReadL(1, 4, f).decode('ascii')
    gdal.VSIFCloseL(f)

    if data != 'foo':
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    # Test with temporary credentials
    gdal.SetConfigOption('AWS_SESSION_TOKEN', 'AWS_SESSION_TOKEN')
    f = open_for_read('/vsis3/s3_fake_bucket_with_session_token/resource')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    data = gdal.VSIFReadL(1, 4, f).decode('ascii')
    gdal.VSIFCloseL(f)
    gdal.SetConfigOption('AWS_SESSION_TOKEN', None)

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
    f = open_for_read('/vsis3/s3_fake_bucket/redirect')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    data = gdal.VSIFReadL(1, 4, f).decode('ascii')
    gdal.VSIFCloseL(f)

    if data != 'foo':

        if gdaltest.is_travis_branch('trusty'):
            print('Skipped on trusty branch, but should be investigated')
            return 'skip'

        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    # Test region and endpoint 'redirects'
    f = open_for_read('/vsis3_streaming/s3_fake_bucket/redirect')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    data = gdal.VSIFReadL(1, 4, f).decode('ascii')
    gdal.VSIFCloseL(f)

    if data != 'foo':
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsis3_streaming/s3_fake_bucket/non_xml_error')
    if f is not None or gdal.VSIGetLastErrorMsg().find('bla') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsis3_streaming/s3_fake_bucket/invalid_xml_error')
    if f is not None or gdal.VSIGetLastErrorMsg().find('<oops>') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsis3_streaming/s3_fake_bucket/no_code_in_error')
    if f is not None or gdal.VSIGetLastErrorMsg().find('<Error/>') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsis3_streaming/s3_fake_bucket/no_region_in_AuthorizationHeaderMalformed_error')
    if f is not None or gdal.VSIGetLastErrorMsg().find('<Error>') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsis3_streaming/s3_fake_bucket/no_endpoint_in_PermanentRedirect_error')
    if f is not None or gdal.VSIGetLastErrorMsg().find('<Error>') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsis3_streaming/s3_fake_bucket/no_message_in_error')
    if f is not None or gdal.VSIGetLastErrorMsg().find('<Error>') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    # Test with requester pays
    gdal.SetConfigOption('AWS_REQUEST_PAYER', 'requester')
    f = open_for_read('/vsis3/s3_fake_bucket_with_requester_pays/resource')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    data = gdal.VSIFReadL(1, 3, f).decode('ascii')
    gdal.VSIFCloseL(f)
    gdal.SetConfigOption('AWS_REQUEST_PAYER', None)
    if data != 'foo':
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test ReadDir() with a fake AWS server

def vsis3_3():

    if gdaltest.webserver_port == 0:
        return 'skip'
    f = open_for_read('/vsis3/s3_fake_bucket2/a_dir/resource3.bin')
    if f is None:

        if gdaltest.is_travis_branch('trusty'):
            print('Skipped on trusty branch, but should be investigated')
            return 'skip'

        gdaltest.post_reason('fail')
        return 'fail'
    gdal.VSIFCloseL(f)
    dir_contents = gdal.ReadDir('/vsis3/s3_fake_bucket2/a_dir')
    if dir_contents != ['resource3.bin', 'resource4.bin', 'subdir']:
        gdaltest.post_reason('fail')
        print(dir_contents)
        return 'fail'
    if gdal.VSIStatL('/vsis3/s3_fake_bucket2/a_dir/resource3.bin').size != 123456:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.VSIStatL('/vsis3/s3_fake_bucket2/a_dir/resource3.bin').mtime != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test CPL_VSIL_CURL_NON_CACHED
    for config_option_value in [ '/vsis3/s3_non_cached/test.txt',
                        '/vsis3/s3_non_cached',
                        '/vsis3/s3_non_cached:/vsis3/unrelated',
                        '/vsis3/unrelated:/vsis3/s3_non_cached',
                        '/vsis3/unrelated:/vsis3/s3_non_cached:/vsis3/unrelated' ]:
        gdal.SetConfigOption('CPL_VSIL_CURL_NON_CACHED', config_option_value)

        # So that the server knows we want it to serve initial content
        gdaltest.gdalurlopen('http://127.0.0.1:%d/s3_non_cached_test_use_content_1' % gdaltest.webserver_port)

        f = open_for_read('/vsis3/s3_non_cached/test.txt')
        if f is None:
            gdaltest.post_reason('fail')
            print(config_option_value)
            return 'fail'
        data = gdal.VSIFReadL(1, 3, f).decode('ascii')
        gdal.VSIFCloseL(f)
        if data != 'foo':
            gdaltest.post_reason('fail')
            print(config_option_value)
            print(data)
            return 'fail'

        # So that the server knows we want it to serve other content
        gdaltest.gdalurlopen('http://127.0.0.1:%d/s3_non_cached_test_use_content_2' % gdaltest.webserver_port)

        size = gdal.VSIStatL('/vsis3/s3_non_cached/test.txt').size
        if size != 4:
            gdaltest.post_reason('fail')
            print(config_option_value)
            print(data)
            return 'fail'

        # So that the server knows we want it to serve initial content
        gdaltest.gdalurlopen('http://127.0.0.1:%d/s3_non_cached_test_use_content_1' % gdaltest.webserver_port)

        size = gdal.VSIStatL('/vsis3/s3_non_cached/test.txt').size
        if size != 3:
            gdaltest.post_reason('fail')
            print(config_option_value)
            print(data)
            return 'fail'

        # So that the server knows we want it to serve other content
        gdaltest.gdalurlopen('http://127.0.0.1:%d/s3_non_cached_test_use_content_2' % gdaltest.webserver_port)

        f = open_for_read('/vsis3/s3_non_cached/test.txt')
        if f is None:
            gdaltest.post_reason('fail')
            print(config_option_value)
            return 'fail'
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)
        if data != 'bar2':
            gdaltest.post_reason('fail')
            print(config_option_value)
            print(data)
            return 'fail'

        gdal.SetConfigOption('CPL_VSIL_CURL_NON_CACHED', None)

    # Retry without option
    for config_option_value in [ None,
                                '/vsis3/s3_non_cached/bar.txt' ]:
        gdal.SetConfigOption('CPL_VSIL_CURL_NON_CACHED', config_option_value)

        # So that the server knows we want it to serve initial content
        gdaltest.gdalurlopen('http://127.0.0.1:%d/s3_non_cached_test_use_content_1' % gdaltest.webserver_port)

        f = open_for_read('/vsis3/s3_non_cached/test.txt')
        if f is None:
            gdaltest.post_reason('fail')
            print(config_option_value)
            return 'fail'
        data = gdal.VSIFReadL(1, 3, f).decode('ascii')
        gdal.VSIFCloseL(f)
        if data != 'foo':
            gdaltest.post_reason('fail')
            print(config_option_value)
            print(data)
            return 'fail'

        # So that the server knows we want it to serve other content
        gdaltest.gdalurlopen('http://127.0.0.1:%d/s3_non_cached_test_use_content_2' % gdaltest.webserver_port)

        f = open_for_read('/vsis3/s3_non_cached/test.txt')
        if f is None:
            gdaltest.post_reason('fail')
            print(config_option_value)
            return 'fail'
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)
        # We should still get foo because of caching
        if data != 'foo':
            gdaltest.post_reason('fail')
            print(config_option_value)
            print(data)
            return 'fail'

    gdal.SetConfigOption('CPL_VSIL_CURL_NON_CACHED', None)

    return 'success'

###############################################################################
# Test simple PUT support with a fake AWS server

def vsis3_4():

    if gdaltest.webserver_port == 0:
        return 'skip'

    with gdaltest.error_handler():
        f = gdal.VSIFOpenL('/vsis3/s3_fake_bucket3', 'wb')
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    if gdal.VSIStatL('/vsis3/s3_fake_bucket3/empty_file.bin').size != 3:
        gdaltest.post_reason('fail')
        return 'fail'

    # Empty file
    f = gdal.VSIFOpenL('/vsis3/s3_fake_bucket3/empty_file.bin', 'wb')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.ErrorReset()
    gdal.VSIFCloseL(f)
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    if gdal.VSIStatL('/vsis3/s3_fake_bucket3/empty_file.bin').size != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Invalid seek
    f = gdal.VSIFOpenL('/vsis3/s3_fake_bucket3/empty_file.bin', 'wb')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    with gdaltest.error_handler():
        ret = gdal.VSIFSeekL(f, 1, 0)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.VSIFCloseL(f)

    # Invalid read
    f = gdal.VSIFOpenL('/vsis3/s3_fake_bucket3/empty_file.bin', 'wb')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    with gdaltest.error_handler():
        ret = gdal.VSIFReadL(1, 1, f)
    if len(ret) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.VSIFCloseL(f)

    # Error case
    f = gdal.VSIFOpenL('/vsis3/s3_fake_bucket3/empty_file_error.bin', 'wb')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.ErrorReset()
    with gdaltest.error_handler():
        gdal.VSIFCloseL(f)
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    # Nominal case
    f = gdal.VSIFOpenL('/vsis3/s3_fake_bucket3/another_file.bin', 'wb')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.VSIFSeekL(f, gdal.VSIFTellL(f), 0) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.VSIFSeekL(f, 0, 1) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.VSIFSeekL(f, 0, 2) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.VSIFWriteL('foo', 1, 3, f) != 3:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.VSIFWriteL('bar', 1, 3, f) != 3:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.ErrorReset()
    gdal.VSIFCloseL(f)
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    # Redirect case
    f = gdal.VSIFOpenL('/vsis3/s3_fake_bucket3/redirect', 'wb')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.VSIFWriteL('foobar', 1, 6, f) != 6:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.ErrorReset()
    gdal.VSIFCloseL(f)
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test simple DELETE support with a fake AWS server

def vsis3_5():

    if gdaltest.webserver_port == 0:
        return 'skip'

    with gdaltest.error_handler():
        ret = gdal.Unlink('/vsis3/foo')
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    if gdal.VSIStatL('/vsis3/s3_delete_bucket/delete_file').size != 3:
        gdaltest.post_reason('fail')
        return 'fail'

    if gdal.VSIStatL('/vsis3/s3_delete_bucket/delete_file').size != 3:
        gdaltest.post_reason('fail')
        return 'fail'

    ret = gdal.Unlink('/vsis3/s3_delete_bucket/delete_file')
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    if gdal.VSIStatL('/vsis3/s3_delete_bucket/delete_file') is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        ret = gdal.Unlink('/vsis3/s3_delete_bucket/delete_file_error')
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ret = gdal.Unlink('/vsis3/s3_delete_bucket/redirect')
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test multipart upload with a fake AWS server

def vsis3_6():

    if gdaltest.webserver_port == 0:
        return 'skip'

    gdal.SetConfigOption('VSIS3_CHUNK_SIZE', '1') # 1 MB
    f = gdal.VSIFOpenL('/vsis3/s3_fake_bucket4/large_file.bin', 'wb')
    gdal.SetConfigOption('VSIS3_CHUNK_SIZE', None)
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    size = 1024*1024+1
    ret = gdal.VSIFWriteL(''.join('a' for i in range(size)), 1,size, f)
    if ret != size:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.ErrorReset()
    gdal.VSIFCloseL(f)
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    for filename in [ '/vsis3/s3_fake_bucket4/large_file_initiate_403_error.bin',
                      '/vsis3/s3_fake_bucket4/large_file_initiate_empty_result.bin',
                      '/vsis3/s3_fake_bucket4/large_file_initiate_invalid_xml_result.bin',
                      '/vsis3/s3_fake_bucket4/large_file_initiate_no_uploadId.bin' ]:
        gdal.SetConfigOption('VSIS3_CHUNK_SIZE', '1') # 1 MB
        f = gdal.VSIFOpenL(filename, 'wb')
        gdal.SetConfigOption('VSIS3_CHUNK_SIZE', None)
        if f is None:
            gdaltest.post_reason('fail')
            return 'fail'
        size = 1024*1024+1
        with gdaltest.error_handler():
            ret = gdal.VSIFWriteL(''.join('a' for i in range(size)), 1,size, f)
        if ret != 0:
            gdaltest.post_reason('fail')
            print(ret)
            return 'fail'
        gdal.ErrorReset()
        gdal.VSIFCloseL(f)
        if gdal.GetLastErrorMsg() != '':
            gdaltest.post_reason('fail')
            return 'fail'

    for filename in [ '/vsis3/s3_fake_bucket4/large_file_upload_part_403_error.bin',
                      '/vsis3/s3_fake_bucket4/large_file_upload_part_no_etag.bin']:
        gdal.SetConfigOption('VSIS3_CHUNK_SIZE', '1') # 1 MB
        f = gdal.VSIFOpenL(filename, 'wb')
        gdal.SetConfigOption('VSIS3_CHUNK_SIZE', None)
        if f is None:
            gdaltest.post_reason('fail')
            return 'fail'
        size = 1024*1024+1
        with gdaltest.error_handler():
            ret = gdal.VSIFWriteL(''.join('a' for i in range(size)), 1,size, f)
        if ret != 0:
            gdaltest.post_reason('fail')
            print(ret)
            return 'fail'
        gdal.ErrorReset()
        gdal.VSIFCloseL(f)
        if gdal.GetLastErrorMsg() != '':
            gdaltest.post_reason('fail')
            return 'fail'

    return 'success'

###############################################################################
# Read credentials from simulated ~/.aws/credentials

def vsis3_read_credentials_file():

    if gdaltest.webserver_port == 0:
        return 'skip'

    gdal.SetConfigOption('AWS_SECRET_ACCESS_KEY', '')
    gdal.SetConfigOption('AWS_ACCESS_KEY_ID', '')

    gdal.SetConfigOption('CPL_AWS_CREDENTIALS_FILE', '/vsimem/aws_credentials')

    gdal.VSICurlClearCache()

    gdal.FileFromMemBuffer('/vsimem/aws_credentials', """
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
[default]
aws_access_key_id = AWS_ACCESS_KEY_ID
aws_secret_access_key = AWS_SECRET_ACCESS_KEY
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
""")

    f = open_for_read('/vsis3/s3_fake_bucket/resource')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    data = gdal.VSIFReadL(1, 4, f).decode('ascii')
    gdal.VSIFCloseL(f)

    if data != 'foo':
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    gdal.SetConfigOption('CPL_AWS_CREDENTIALS_FILE', '')
    gdal.Unlink('/vsimem/aws_credentials')

    return 'success'

###############################################################################
# Read credentials from simulated  ~/.aws/config

def vsis3_read_config_file():

    if gdaltest.webserver_port == 0:
        return 'skip'

    gdal.SetConfigOption('AWS_SECRET_ACCESS_KEY', '')
    gdal.SetConfigOption('AWS_ACCESS_KEY_ID', '')

    gdal.SetConfigOption('AWS_CONFIG_FILE', '/vsimem/aws_config')

    gdal.VSICurlClearCache()

    gdal.FileFromMemBuffer('/vsimem/aws_config', """
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
[default]
aws_access_key_id = AWS_ACCESS_KEY_ID
aws_secret_access_key = AWS_SECRET_ACCESS_KEY
region = us-east-1
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
""")

    f = open_for_read('/vsis3/s3_fake_bucket/resource')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    data = gdal.VSIFReadL(1, 4, f).decode('ascii')
    gdal.VSIFCloseL(f)

    if data != 'foo':
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    gdal.SetConfigOption('AWS_CONFIG_FILE', '')
    gdal.Unlink('/vsimem/aws_config')

    return 'success'

###############################################################################
# Read credentials from simulated ~/.aws/credentials and ~/.aws/config

def vsis3_read_credentials_config_file():

    if gdaltest.webserver_port == 0:
        return 'skip'

    gdal.SetConfigOption('AWS_SECRET_ACCESS_KEY', '')
    gdal.SetConfigOption('AWS_ACCESS_KEY_ID', '')

    gdal.SetConfigOption('CPL_AWS_CREDENTIALS_FILE', '/vsimem/aws_credentials')
    gdal.SetConfigOption('AWS_CONFIG_FILE', '/vsimem/aws_config')

    gdal.VSICurlClearCache()

    gdal.FileFromMemBuffer('/vsimem/aws_credentials', """
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
[default]
aws_access_key_id = AWS_ACCESS_KEY_ID
aws_secret_access_key = AWS_SECRET_ACCESS_KEY
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
""")

    gdal.FileFromMemBuffer('/vsimem/aws_config', """
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
[default]
aws_access_key_id = AWS_ACCESS_KEY_ID
aws_secret_access_key = AWS_SECRET_ACCESS_KEY
region = us-east-1
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
""")

    f = open_for_read('/vsis3/s3_fake_bucket/resource')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    data = gdal.VSIFReadL(1, 4, f).decode('ascii')
    gdal.VSIFCloseL(f)

    if data != 'foo':
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    gdal.SetConfigOption('CPL_AWS_CREDENTIALS_FILE', '')
    gdal.Unlink('/vsimem/aws_credentials')
    gdal.SetConfigOption('AWS_CONFIG_FILE', '')
    gdal.Unlink('/vsimem/aws_config')

    return 'success'

###############################################################################
# Read credentials from simulated ~/.aws/credentials and ~/.aws/config with
# a non default profile

def vsis3_read_credentials_config_file_non_default():

    if gdaltest.webserver_port == 0:
        return 'skip'

    gdal.SetConfigOption('AWS_SECRET_ACCESS_KEY', '')
    gdal.SetConfigOption('AWS_ACCESS_KEY_ID', '')

    gdal.SetConfigOption('CPL_AWS_CREDENTIALS_FILE', '/vsimem/aws_credentials')
    gdal.SetConfigOption('AWS_CONFIG_FILE', '/vsimem/aws_config')
    gdal.SetConfigOption('AWS_DEFAULT_PROFILE', 'myprofile')

    gdal.VSICurlClearCache()

    gdal.FileFromMemBuffer('/vsimem/aws_credentials', """
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
[myprofile]
aws_access_key_id = AWS_ACCESS_KEY_ID
aws_secret_access_key = AWS_SECRET_ACCESS_KEY
[default]
aws_access_key_id = foo
aws_secret_access_key = bar
""")

    gdal.FileFromMemBuffer('/vsimem/aws_config', """
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
[profile myprofile]
region = us-east-1
[default]
aws_access_key_id = foo
aws_secret_access_key = bar
""")

    f = open_for_read('/vsis3/s3_fake_bucket/resource')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    data = gdal.VSIFReadL(1, 4, f).decode('ascii')
    gdal.VSIFCloseL(f)

    if data != 'foo':
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    gdal.SetConfigOption('CPL_AWS_CREDENTIALS_FILE', '')
    gdal.Unlink('/vsimem/aws_credentials')
    gdal.SetConfigOption('AWS_CONFIG_FILE', '')
    gdal.Unlink('/vsimem/aws_config')
    gdal.SetConfigOption('AWS_DEFAULT_PROFILE', '')

    return 'success'

###############################################################################
# Read credentials from simulated ~/.aws/credentials and ~/.aws/config

def vsis3_read_credentials_config_file_inconsistent():

    if gdaltest.webserver_port == 0:
        return 'skip'

    gdal.SetConfigOption('AWS_SECRET_ACCESS_KEY', '')
    gdal.SetConfigOption('AWS_ACCESS_KEY_ID', '')

    gdal.SetConfigOption('CPL_AWS_CREDENTIALS_FILE', '/vsimem/aws_credentials')
    gdal.SetConfigOption('AWS_CONFIG_FILE', '/vsimem/aws_config')

    gdal.VSICurlClearCache()

    gdal.FileFromMemBuffer('/vsimem/aws_credentials', """
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
[default]
aws_access_key_id = AWS_ACCESS_KEY_ID
aws_secret_access_key = AWS_SECRET_ACCESS_KEY
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
""")

    gdal.FileFromMemBuffer('/vsimem/aws_config', """
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
[default]
aws_access_key_id = AWS_ACCESS_KEY_ID_inconsistent
aws_secret_access_key = AWS_SECRET_ACCESS_KEY_inconsistent
region = us-east-1
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
""")

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsis3/s3_fake_bucket/resource')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorMsg() == '':
        # Expected 'aws_access_key_id defined in both /vsimem/aws_credentials and /vsimem/aws_config'
        gdaltest.post_reason('fail')
        return 'fail'
    data = gdal.VSIFReadL(1, 4, f).decode('ascii')
    gdal.VSIFCloseL(f)

    if data != 'foo':
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    gdal.SetConfigOption('CPL_AWS_CREDENTIALS_FILE', '')
    gdal.Unlink('/vsimem/aws_credentials')
    gdal.SetConfigOption('AWS_CONFIG_FILE', '')
    gdal.Unlink('/vsimem/aws_config')

    return 'success'

###############################################################################
# Read credentials from simulated EC2 instance

def vsis3_read_credentials_ec2():

    if gdaltest.webserver_port == 0:
        return 'skip'

    if sys.platform not in ('linux', 'linux2', 'win32'):
        return 'skip'

    gdal.SetConfigOption('CPL_AWS_EC2_CREDENTIALS_URL',
                         'http://localhost:%d/latest/meta-data/iam/security-credentials/' % gdaltest.webserver_port)
    # Disable hypervisor related check to test if we are really on EC2
    gdal.SetConfigOption('CPL_AWS_CHECK_HYPERVISOR_UUID', 'NO')

    gdal.VSICurlClearCache()

    f = open_for_read('/vsis3/s3_fake_bucket/resource')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    data = gdal.VSIFReadL(1, 4, f).decode('ascii')
    gdal.VSIFCloseL(f)

    if data != 'foo':
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    # Set a fake URL to check that credentials re-use works
    gdal.SetConfigOption('CPL_AWS_EC2_CREDENTIALS_URL', '')

    f = open_for_read('/vsis3/s3_fake_bucket/bar')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    data = gdal.VSIFReadL(1, 4, f).decode('ascii')
    gdal.VSIFCloseL(f)

    if data != 'bar':
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    gdal.SetConfigOption('CPL_AWS_EC2_CREDENTIALS_URL','')
    gdal.SetConfigOption('CPL_AWS_CHECK_HYPERVISOR_UUID', None)

    return 'success'

###############################################################################
# Read credentials from simulated EC2 instance with expiration of the
# cached credentials

def vsis3_read_credentials_ec2_expiration():

    if gdaltest.webserver_port == 0:
        return 'skip'

    if sys.platform not in ('linux', 'linux2', 'win32'):
        return 'skip'

    gdal.SetConfigOption('CPL_AWS_EC2_CREDENTIALS_URL',
                         'http://localhost:%d/latest/meta-data/iam/security-credentials/expire_in_past/' % gdaltest.webserver_port)
    # Disable hypervisor related check to test if we are really on EC2
    gdal.SetConfigOption('CPL_AWS_CHECK_HYPERVISOR_UUID', 'NO')

    gdal.VSICurlClearCache()

    f = open_for_read('/vsis3/s3_fake_bucket/resource')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    data = gdal.VSIFReadL(1, 4, f).decode('ascii')
    gdal.VSIFCloseL(f)

    if data != 'foo':
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    # Set a fake URL to demonstrate we try to re-fetch credentials
    gdal.SetConfigOption('CPL_AWS_EC2_CREDENTIALS_URL', '')

    with gdaltest.error_handler():
        f = open_for_read('/vsis3/s3_fake_bucket/bar')
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.SetConfigOption('CPL_AWS_EC2_CREDENTIALS_URL','')
    gdal.SetConfigOption('CPL_AWS_CHECK_HYPERVISOR_UUID', None)

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

    credentials_filename = gdal.GetConfigOption('HOME',
        gdal.GetConfigOption('USERPROFILE', '')) + '/.aws/credentials'
    if not os.path.exists(credentials_filename):
        if gdal.GetConfigOption('AWS_SECRET_ACCESS_KEY') is None:
            print('Missing AWS_SECRET_ACCESS_KEY for running gdaltest_list_extra')
            return 'skip'
        elif gdal.GetConfigOption('AWS_ACCESS_KEY_ID') is None:
            print('Missing AWS_ACCESS_KEY_ID for running gdaltest_list_extra')
            return 'skip'
    elif gdal.GetConfigOption('S3_RESOURCE') is None:
        print('Missing S3_RESOURCE for running gdaltest_list_extra')
        return 'skip'

    f = open_for_read('/vsis3/' + gdal.GetConfigOption('S3_RESOURCE'))
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    if len(ret) != 1:
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'

    # Same with /vsis3_streaming/
    f = open_for_read('/vsis3_streaming/' + gdal.GetConfigOption('S3_RESOURCE'))
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    if len(ret) != 1:
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'

    # Invalid bucket : "The specified bucket does not exist"
    gdal.ErrorReset()
    f = open_for_read('/vsis3/not_existing_bucket/foo')
    with gdaltest.error_handler():
        gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)
    if gdal.VSIGetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    # Invalid resource
    gdal.ErrorReset()
    f = open_for_read('/vsis3_streaming/' + gdal.GetConfigOption('S3_RESOURCE') + '/invalid_resource.baz')
    if f is not None:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
def vsis3_cleanup():

    for var in gdaltest.aws_vars:
        gdal.SetConfigOption(var, gdaltest.aws_vars[var])

    gdal.SetConfigOption('CPL_AWS_CREDENTIALS_FILE', None)
    gdal.SetConfigOption('AWS_CONFIG_FILE', None)
    gdal.SetConfigOption('CPL_AWS_EC2_CREDENTIALS_URL', None)

    return 'success'

gdaltest_list = [ vsis3_init,
                  vsis3_1,
                  vsis3_start_webserver,
                  vsis3_2,
                  vsis3_3,
                  vsis3_4,
                  vsis3_5,
                  vsis3_6,
                  vsis3_read_credentials_file,
                  vsis3_read_config_file,
                  vsis3_read_credentials_config_file,
                  vsis3_read_credentials_config_file_non_default,
                  vsis3_read_credentials_config_file_inconsistent,
                  vsis3_read_credentials_ec2,
                  vsis3_read_credentials_ec2_expiration,
                  vsis3_stop_webserver,
                  vsis3_cleanup ]
gdaltest_list_extra = [ vsis3_extra_1, vsis3_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'vsis3' )

    gdaltest.run_tests( gdaltest_list + gdaltest_list_extra )

    gdaltest.summarize()
