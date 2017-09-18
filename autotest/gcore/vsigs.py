#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsigs
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 20157 Even Rouault <even dot rouault at spatialys dot com>
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

def open_for_read(uri):
    """
    Opens a test file for reading.
    """
    return gdal.VSIFOpenExL(uri, 'rb', 1)

###############################################################################
def vsigs_init():

    gdaltest.gs_vars = {}
    for var in ('GS_SECRET_ACCESS_KEY', 'GS_ACCESS_KEY_ID',
                'CPL_GS_TIMESTAMP', 'CPL_GS_ENDPOINT',
                'GDAL_HTTP_HEADER_FILE'):
        gdaltest.gs_vars[var] = gdal.GetConfigOption(var)
        if gdaltest.gs_vars[var] is not None:
            gdal.SetConfigOption(var, "")

    # To avoid user credentials in ~/.boto
    # to mess up our tests
    gdal.SetConfigOption('CPL_GS_CREDENTIALS_FILE', '')

    return 'success'

###############################################################################
# Error cases

def vsigs_1():

    if not gdaltest.built_against_curl():
        return 'skip'

    # Invalid header filename
    gdal.ErrorReset()
    gdal.SetConfigOption('GDAL_HTTP_HEADER_FILE', '/i_dont/exist.py')
    f = open_for_read('/vsigs/foo/bar')
    if f is None:
        gdal.SetConfigOption('GDAL_HTTP_HEADER_FILE', None)
        gdaltest.post_reason('fail')
        return 'fail'
    with gdaltest.error_handler():
        data = gdal.VSIFReadL(1, 1, f)
    last_err = gdal.GetLastErrorMsg()
    gdal.SetConfigOption('GDAL_HTTP_HEADER_FILE', None)
    gdal.VSIFCloseL(f)
    if len(data) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if last_err.find('Cannot read') < 0:
        gdaltest.post_reason('fail')
        print(last_err)
        return 'fail'

    # Invalid content for header file 
    gdal.SetConfigOption('GDAL_HTTP_HEADER_FILE', 'vsigs.py')
    f = open_for_read('/vsigs/foo/bar')
    if f is None:
        gdal.SetConfigOption('GDAL_HTTP_HEADER_FILE', None)
        gdaltest.post_reason('fail')
        return 'fail'
    data = gdal.VSIFReadL(1, 1, f)
    gdal.SetConfigOption('GDAL_HTTP_HEADER_FILE', None)
    gdal.VSIFCloseL(f)
    if len(data) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Missing GS_SECRET_ACCESS_KEY
    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsigs/foo/bar')
    if f is not None or gdal.VSIGetLastErrorMsg().find('GS_SECRET_ACCESS_KEY') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsigs_streaming/foo/bar')
    if f is not None or gdal.VSIGetLastErrorMsg().find('GS_SECRET_ACCESS_KEY') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    gdal.SetConfigOption('GS_SECRET_ACCESS_KEY', 'GS_SECRET_ACCESS_KEY')

    # Missing GS_ACCESS_KEY_ID
    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsigs/foo/bar')
    if f is not None or gdal.VSIGetLastErrorMsg().find('GS_ACCESS_KEY_ID') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    gdal.SetConfigOption('GS_ACCESS_KEY_ID', 'GS_ACCESS_KEY_ID')

    # ERROR 1: The User Id you provided does not exist in our records.
    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsigs/foo/bar.baz')
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
        f = open_for_read('/vsigs_streaming/foo/bar.baz')
    if f is not None or gdal.VSIGetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
def vsigs_start_webserver():

    gdaltest.webserver_process = None
    gdaltest.webserver_port = 0

    if not gdaltest.built_against_curl():
        return 'skip'

    (gdaltest.webserver_process, gdaltest.webserver_port) = webserver.launch(handler = webserver.DispatcherHttpHandler)
    if gdaltest.webserver_port == 0:
        return 'skip'

    gdal.SetConfigOption('CPL_GS_ENDPOINT', 'http://127.0.0.1:%d/' % gdaltest.webserver_port)

    return 'success'

###############################################################################
# Test with a fake Google Cloud Storage server

def vsigs_2():

    if gdaltest.webserver_port == 0:
        return 'skip'

    # header file 
    gdal.FileFromMemBuffer('/vsimem/my_headers.txt', 'foo: bar')


    def method(request):
        if 'foo' not in request.headers or request.headers['foo'] != 'bar':
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)
            return

        request.send_response(200)
        request.send_header('Content-type', 'text/plain')
        request.send_header('Content-Length', 1)
        request.end_headers()
        request.wfile.write("""Y""".encode('ascii'))

    handler = webserver.SequentialHandler()
    handler.add('GET', '/gs_fake_bucket_http_header_file/resource', custom_method = method)
    with webserver.install_http_handler(handler):
        with gdaltest.config_option('GDAL_HTTP_HEADER_FILE', '/vsimem/my_headers.txt'):
            f = open_for_read('/vsigs/gs_fake_bucket_http_header_file/resource')
            if f is None:
                gdaltest.post_reason('fail')
                return 'fail'
            data = gdal.VSIFReadL(1, 1, f)
            gdal.VSIFCloseL(f)
            if len(data) != 1:
                gdaltest.post_reason('fail')
                return 'fail'
    gdal.Unlink('/vsimem/my_headers.txt')


    gdal.SetConfigOption('GS_SECRET_ACCESS_KEY', 'GS_SECRET_ACCESS_KEY')
    gdal.SetConfigOption('GS_ACCESS_KEY_ID', 'GS_ACCESS_KEY_ID')
    gdal.SetConfigOption('CPL_GS_TIMESTAMP', 'my_timestamp')


    def method(request):
        if 'Authorization' not in request.headers:
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)
            return
        expected_authorization = 'GOOG1 GS_ACCESS_KEY_ID:8tndu9//BfmN+Kg4AFLdUMZMBDQ='
        if request.headers['Authorization'] != expected_authorization :
            sys.stderr.write("Bad Authorization: '%s'\n" % str(request.headers['Authorization']))
            request.send_response(403)
            return

        request.send_response(200)
        request.send_header('Content-type', 'text/plain')
        request.send_header('Content-Length', 3)
        request.end_headers()
        request.wfile.write("""foo""".encode('ascii'))

    handler = webserver.SequentialHandler()
    handler.add('GET', '/gs_fake_bucket/resource', custom_method = method)
    with webserver.install_http_handler(handler):
        f = open_for_read('/vsigs/gs_fake_bucket/resource')
        if f is None:
            gdaltest.post_reason('fail')
            return 'fail'
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    if data != 'foo':
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    handler = webserver.SequentialHandler()
    handler.add('GET', '/gs_fake_bucket/resource', custom_method = method)
    with webserver.install_http_handler(handler):
        f = open_for_read('/vsigs_streaming/gs_fake_bucket/resource')
        if f is None:
            gdaltest.post_reason('fail')
            return 'fail'
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

        if data != 'foo':
            gdaltest.post_reason('fail')
            print(data)
            return 'fail'

    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/gs_fake_bucket/resource2.bin', 200,
                {'Content-Length': 1000000})
    with webserver.install_http_handler(handler):
        stat_res = gdal.VSIStatL('/vsigs/gs_fake_bucket/resource2.bin')
        if stat_res is None or stat_res.size != 1000000:
            gdaltest.post_reason('fail')
            if stat_res is not None:
                print(stat_res.size)
            else:
                print(stat_res)
            return 'fail'

    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/gs_fake_bucket/resource2.bin', 200,
                {'Content-Length': 1000000})
    with webserver.install_http_handler(handler):
        stat_res = gdal.VSIStatL('/vsigs_streaming/gs_fake_bucket/resource2.bin')
        if stat_res is None or stat_res.size != 1000000:
            gdaltest.post_reason('fail')
            if stat_res is not None:
                print(stat_res.size)
            else:
                print(stat_res)
            return 'fail'

    return 'success'

###############################################################################
# Test ReadDir() with a fake Google Cloud Storage server

def vsigs_3():

    if gdaltest.webserver_port == 0:
        return 'skip'

    handler = webserver.SequentialHandler()
    handler.add('GET', '/gs_fake_bucket2?delimiter=/&prefix=a_dir/', 200,
                { 'Content-type': 'application/xml' },
                """<?xml version="1.0" encoding="UTF-8"?>
                    <ListBucketResult>
                        <Prefix>a_dir/</Prefix>
                        <NextMarker>bla</NextMarker>
                        <Contents>
                            <Key>a_dir/resource3.bin</Key>
                            <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                            <Size>123456</Size>
                        </Contents>
                    </ListBucketResult>
                """)
    handler.add('GET', '/gs_fake_bucket2?delimiter=/&marker=bla&prefix=a_dir/', 200,
                { 'Content-type': 'application/xml' },
                """<?xml version="1.0" encoding="UTF-8"?>
                    <ListBucketResult>
                        <Prefix>a_dir/</Prefix>
                        <Contents>
                            <Key>a_dir/resource4.bin</Key>
                            <LastModified>2015-10-16T12:34:56.000Z</LastModified>
                            <Size>456789</Size>
                        </Contents>
                        <CommonPrefixes>
                            <Prefix>a_dir/subdir/</Prefix>
                        </CommonPrefixes>
                    </ListBucketResult>
                """)

    with webserver.install_http_handler(handler):
        f = open_for_read('/vsigs/gs_fake_bucket2/a_dir/resource3.bin')
    if f is None:

        if gdaltest.is_travis_branch('trusty'):
            print('Skipped on trusty branch, but should be investigated')
            return 'skip'

        gdaltest.post_reason('fail')
        return 'fail'
    gdal.VSIFCloseL(f)

    dir_contents = gdal.ReadDir('/vsigs/gs_fake_bucket2/a_dir')
    if dir_contents != ['resource3.bin', 'resource4.bin', 'subdir']:
        gdaltest.post_reason('fail')
        print(dir_contents)
        return 'fail'
    if gdal.VSIStatL('/vsigs/gs_fake_bucket2/a_dir/resource3.bin').size != 123456:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.VSIStatL('/vsigs/gs_fake_bucket2/a_dir/resource3.bin').mtime != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Read credentials from simulated ~/.boto

def vsigs_read_credentials_file():

    if gdaltest.webserver_port == 0:
        return 'skip'

    gdal.SetConfigOption('GS_SECRET_ACCESS_KEY', '')
    gdal.SetConfigOption('GS_ACCESS_KEY_ID', '')

    gdal.SetConfigOption('CPL_GS_CREDENTIALS_FILE', '/vsimem/.boto')

    gdal.VSICurlClearCache()

    gdal.FileFromMemBuffer('/vsimem/.boto', """
[unrelated]
gs_access_key_id = foo
gs_secret_access_key = bar
[Credentials]
gs_access_key_id = GS_ACCESS_KEY_ID
gs_secret_access_key = GS_SECRET_ACCESS_KEY
[unrelated]
gs_access_key_id = foo
gs_secret_access_key = bar
""")

    def method(request):
        if 'Authorization' not in request.headers:
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)
            return
        expected_authorization = 'GOOG1 GS_ACCESS_KEY_ID:8tndu9//BfmN+Kg4AFLdUMZMBDQ='
        if request.headers['Authorization'] != expected_authorization :
            sys.stderr.write("Bad Authorization: '%s'\n" % str(request.headers['Authorization']))
            request.send_response(403)
            return

        request.send_response(200)
        request.send_header('Content-type', 'text/plain')
        request.send_header('Content-Length', 3)
        request.end_headers()
        request.wfile.write("""foo""".encode('ascii'))

    handler = webserver.SequentialHandler()
    handler.add('GET', '/gs_fake_bucket/resource', custom_method = method)
    with webserver.install_http_handler(handler):
        f = open_for_read('/vsigs/gs_fake_bucket/resource')
        if f is None:
            gdaltest.post_reason('fail')
            return 'fail'
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    if data != 'foo':
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    gdal.SetConfigOption('CPL_GS_CREDENTIALS_FILE', '')
    gdal.Unlink('/vsimem/.boto')

    return 'success'

###############################################################################
def vsigs_stop_webserver():

    if gdaltest.webserver_port == 0:
        return 'skip'

    webserver.server_stop(gdaltest.webserver_process, gdaltest.webserver_port)

    return 'success'

###############################################################################
# Nominal cases (require valid credentials)

def vsigs_extra_1():

    if not gdaltest.built_against_curl():
        return 'skip'

    if gdal.GetConfigOption('GS_SECRET_ACCESS_KEY') is None:
        print('Missing GS_SECRET_ACCESS_KEY for running gdaltest_list_extra')
        return 'skip'
    elif gdal.GetConfigOption('GS_ACCESS_KEY_ID') is None:
        print('Missing GS_ACCESS_KEY_ID for running gdaltest_list_extra')
        return 'skip'
    elif gdal.GetConfigOption('GS_RESOURCE') is None:
        print('Missing GS_RESOURCE for running gdaltest_list_extra')
        return 'skip'

    f = open_for_read('/vsigs/' + gdal.GetConfigOption('GS_RESOURCE'))
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    if len(ret) != 1:
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'

    # Same with /vsigs_streaming/
    f = open_for_read('/vsigs_streaming/' + gdal.GetConfigOption('GS_RESOURCE'))
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
    f = open_for_read('/vsigs/not_existing_bucket/foo')
    with gdaltest.error_handler():
        gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)
    if gdal.VSIGetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    # Invalid resource
    gdal.ErrorReset()
    f = open_for_read('/vsigs_streaming/' + gdal.GetConfigOption('GS_RESOURCE') + '/invalid_resource.baz')
    if f is not None:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
def vsigs_cleanup():

    for var in gdaltest.gs_vars:
        gdal.SetConfigOption(var, gdaltest.gs_vars[var])

    return 'success'

gdaltest_list = [ vsigs_init,
                  vsigs_1,
                  vsigs_start_webserver,
                  vsigs_2,
                  vsigs_3,
                  vsigs_read_credentials_file,
                  vsigs_stop_webserver,
                  vsigs_cleanup ]

# gdaltest_list = [ vsigs_init, vsigs_start_webserver, vsigs_2, vsigs_read_credentials_file, vsigs_stop_webserver, vsigs_cleanup ]

gdaltest_list_extra = [ vsigs_extra_1, vsigs_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'vsigs' )

    gdaltest.run_tests( gdaltest_list + gdaltest_list_extra )

    gdaltest.summarize()
