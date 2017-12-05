#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsioss
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2017, Even Rouault <even dot rouault at spatialys dot com>
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

import stat
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
def visoss_init():

    gdaltest.oss_vars = {}
    for var in ('OSS_SECRET_ACCESS_KEY', 'OSS_ACCESS_KEY_ID', 'OSS_TIMESTAMP', 'OSS_HTTPS', 'OSS_VIRTUAL_HOSTING', 'OSS_ENDPOINT'):
        gdaltest.oss_vars[var] = gdal.GetConfigOption(var)
        if gdaltest.oss_vars[var] is not None:
            gdal.SetConfigOption(var, "")


    return 'success'

###############################################################################
# Error cases

def visoss_1():

    if not gdaltest.built_against_curl():
        return 'skip'

    # Missing OSS_SECRET_ACCESS_KEY
    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsioss/foo/bar')
    if f is not None or gdal.VSIGetLastErrorMsg().find('OSS_SECRET_ACCESS_KEY') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsioss_streaming/foo/bar')
    if f is not None or gdal.VSIGetLastErrorMsg().find('OSS_SECRET_ACCESS_KEY') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    gdal.SetConfigOption('OSS_SECRET_ACCESS_KEY', 'OSS_SECRET_ACCESS_KEY')

    # Missing OSS_ACCESS_KEY_ID
    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsioss/foo/bar')
    if f is not None or gdal.VSIGetLastErrorMsg().find('OSS_ACCESS_KEY_ID') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    gdal.SetConfigOption('OSS_ACCESS_KEY_ID', 'OSS_ACCESS_KEY_ID')

    # ERROR 1: The OSS Access Key Id you provided does not exist in our records.
    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsioss/foo/bar.baz')
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
        f = open_for_read('/vsioss_streaming/foo/bar.baz')
    if f is not None or gdal.VSIGetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
def visoss_start_webserver():

    gdaltest.webserver_process = None
    gdaltest.webserver_port = 0

    if not gdaltest.built_against_curl():
        return 'skip'

    (gdaltest.webserver_process, gdaltest.webserver_port) = webserver.launch(handler = webserver.DispatcherHttpHandler)
    if gdaltest.webserver_port == 0:
        return 'skip'

    gdal.SetConfigOption('OSS_SECRET_ACCESS_KEY', 'OSS_SECRET_ACCESS_KEY')
    gdal.SetConfigOption('OSS_ACCESS_KEY_ID', 'OSS_ACCESS_KEY_ID')
    gdal.SetConfigOption('CPL_OSS_TIMESTAMP', 'my_timestamp')
    gdal.SetConfigOption('OSS_HTTPS', 'NO')
    gdal.SetConfigOption('OSS_VIRTUAL_HOSTING', 'NO')
    gdal.SetConfigOption('OSS_ENDPOINT', '127.0.0.1:%d' % gdaltest.webserver_port)

    return 'success'

###############################################################################

def get_oss_fake_bucket_resource_method(request):
    request.protocol_version = 'HTTP/1.1'

    if 'Authorization' not in request.headers:
        sys.stderr.write('Bad headers: %s\n' % str(request.headers))
        request.send_response(403)
        return
    expected_authorization = 'OSS OSS_ACCESS_KEY_ID:ZFgKjvMtWUwm9CTeCYoPomhuJiE='
    if request.headers['Authorization'] != expected_authorization:
        sys.stderr.write("Bad Authorization: '%s'\n" % str(request.headers['Authorization']))
        request.send_response(403)
        return

    request.send_response(200)
    request.send_header('Content-type', 'text/plain')
    request.send_header('Content-Length', 3)
    request.send_header('Connection', 'close')
    request.end_headers()
    request.wfile.write("""foo""".encode('ascii'))

###############################################################################
# Test with a fake OSS server

def visoss_2():

    if gdaltest.webserver_port == 0:
        return 'skip'

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oss_fake_bucket/resource', custom_method = get_oss_fake_bucket_resource_method)

    with webserver.install_http_handler(handler):
        f = open_for_read('/vsioss/oss_fake_bucket/resource')
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
    handler.add('GET', '/oss_fake_bucket/resource', custom_method = get_oss_fake_bucket_resource_method)
    with webserver.install_http_handler(handler):
        f = open_for_read('/vsioss_streaming/oss_fake_bucket/resource')
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

    def method(request):
        request.protocol_version = 'HTTP/1.1'
        request.send_response(400)
        response = """<?xml version="1.0" encoding="UTF-8"?>
<Error>
  <Code>AccessDenied</Code>
  <Message>The bucket you are attempting to access must be addressed using the specified endpoint. Please send all future requests to this endpoint.</Message>
  <HostId>unused</HostId>
  <Bucket>unuset</Bucket>
  <Endpoint>localhost:%d</Endpoint>
</Error>""" % request.server.port
        response = '%x\r\n%s\r\n0\r\n\r\n' % (len(response), response)
        request.send_header('Content-type', 'application/xml')
        request.send_header('Transfer-Encoding', 'chunked')
        request.send_header('Connection', 'close')
        request.end_headers()
        request.wfile.write(response.encode('ascii'))

    handler.add('GET', '/oss_fake_bucket/redirect', custom_method = method)

    def method(request):
        request.protocol_version = 'HTTP/1.1'
        if request.headers['Host'].startswith('localhost'):
            request.send_response(200)
            request.send_header('Content-type', 'text/plain')
            request.send_header('Content-Length', 3)
            request.send_header('Connection', 'close')
            request.end_headers()
            request.wfile.write("""foo""".encode('ascii'))
        else:
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)

    handler.add('GET', '/oss_fake_bucket/redirect', custom_method = method)

    # Test region and endpoint 'redirects'
    with webserver.install_http_handler(handler):
        f = open_for_read('/vsioss/oss_fake_bucket/redirect')
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
    handler.req_count = 0
    with webserver.install_http_handler(handler):
        f = open_for_read('/vsioss_streaming/oss_fake_bucket/redirect')
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

    def method(request):
        # /vsioss_streaming/ should have remembered the change of region and endpoint
        if not request.headers['Host'].startswith('localhost'):
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)

        request.protocol_version = 'HTTP/1.1'
        request.send_response(400)
        response = 'bla'
        response = '%x\r\n%s\r\n0\r\n\r\n' % (len(response), response)
        request.send_header('Content-type', 'application/xml')
        request.send_header('Transfer-Encoding', 'chunked')
        request.send_header('Connection', 'close')
        request.end_headers()
        request.wfile.write(response.encode('ascii'))

    handler.add('GET', '/oss_fake_bucket/non_xml_error', custom_method = method)

    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            f = open_for_read('/vsioss_streaming/oss_fake_bucket/non_xml_error')
    if f is not None or gdal.VSIGetLastErrorMsg().find('bla') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'


    handler = webserver.SequentialHandler()
    response = '<?xml version="1.0" encoding="UTF-8"?><oops>'
    response = '%x\r\n%s\r\n0\r\n\r\n' % (len(response), response)
    handler.add('GET', '/oss_fake_bucket/invalid_xml_error', 400,
                { 'Content-type': 'application/xml',
                  'Transfer-Encoding': 'chunked',
                  'Connection': 'close' }, response)
    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            f = open_for_read('/vsioss_streaming/oss_fake_bucket/invalid_xml_error')
    if f is not None or gdal.VSIGetLastErrorMsg().find('<oops>') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'


    handler = webserver.SequentialHandler()
    response = '<?xml version="1.0" encoding="UTF-8"?><Error/>'
    response = '%x\r\n%s\r\n0\r\n\r\n' % (len(response), response)
    handler.add('GET', '/oss_fake_bucket/no_code_in_error', 400,
                { 'Content-type': 'application/xml',
                  'Transfer-Encoding': 'chunked',
                  'Connection': 'close' }, response)
    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            f = open_for_read('/vsioss_streaming/oss_fake_bucket/no_code_in_error')
    if f is not None or gdal.VSIGetLastErrorMsg().find('<Error/>') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'


    handler = webserver.SequentialHandler()
    response = '<?xml version="1.0" encoding="UTF-8"?><Error><Code>AuthorizationHeaderMalformed</Code></Error>'
    response = '%x\r\n%s\r\n0\r\n\r\n' % (len(response), response)
    handler.add('GET', '/oss_fake_bucket/no_region_in_AuthorizationHeaderMalformed_error', 400,
                { 'Content-type': 'application/xml',
                  'Transfer-Encoding': 'chunked',
                  'Connection': 'close' }, response)
    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            f = open_for_read('/vsioss_streaming/oss_fake_bucket/no_region_in_AuthorizationHeaderMalformed_error')
    if f is not None or gdal.VSIGetLastErrorMsg().find('<Error>') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    handler = webserver.SequentialHandler()
    response = '<?xml version="1.0" encoding="UTF-8"?><Error><Code>PermanentRedirect</Code></Error>'
    response = '%x\r\n%s\r\n0\r\n\r\n' % (len(response), response)
    handler.add('GET', '/oss_fake_bucket/no_endpoint_in_PermanentRedirect_error', 400,
                { 'Content-type': 'application/xml',
                  'Transfer-Encoding': 'chunked',
                  'Connection': 'close' }, response)
    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            f = open_for_read('/vsioss_streaming/oss_fake_bucket/no_endpoint_in_PermanentRedirect_error')
    if f is not None or gdal.VSIGetLastErrorMsg().find('<Error>') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    handler = webserver.SequentialHandler()
    response = '<?xml version="1.0" encoding="UTF-8"?><Error><Code>bla</Code></Error>'
    response = '%x\r\n%s\r\n0\r\n\r\n' % (len(response), response)
    handler.add('GET', '/oss_fake_bucket/no_message_in_error', 400,
                { 'Content-type': 'application/xml',
                  'Transfer-Encoding': 'chunked',
                  'Connection': 'close' }, response)
    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            f = open_for_read('/vsioss_streaming/oss_fake_bucket/no_message_in_error')
    if f is not None or gdal.VSIGetLastErrorMsg().find('<Error>') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
# Test ReadDir() with a fake OSS server

def visoss_3():

    if gdaltest.webserver_port == 0:
        return 'skip'

    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = 'HTTP/1.1'
        request.send_response(200)
        request.send_header('Content-type', 'application/xml')
        response = """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>a_dir/</Prefix>
                <NextMarker>bla</NextMarker>
                <Contents>
                    <Key>a_dir/resource3.bin</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>123456</Size>
                </Contents>
            </ListBucketResult>
        """
        request.send_header('Content-Length', len(response))
        request.end_headers()
        request.wfile.write(response.encode('ascii'))

    handler.add('GET', '/oss_fake_bucket2/?delimiter=%2F&prefix=a_dir%2F', custom_method = method)

    def method(request):
        request.protocol_version = 'HTTP/1.1'
        request.send_response(200)
        request.send_header('Content-type', 'application/xml')
        response = """<?xml version="1.0" encoding="UTF-8"?>
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
        """
        request.send_header('Content-Length', len(response))
        request.end_headers()
        request.wfile.write(response.encode('ascii'))

    handler.add('GET', '/oss_fake_bucket2/?delimiter=%2F&marker=bla&prefix=a_dir%2F', custom_method = method)

    with webserver.install_http_handler(handler):
        f = open_for_read('/vsioss/oss_fake_bucket2/a_dir/resource3.bin')
    if f is None:

        if gdaltest.is_travis_branch('trusty'):
            print('Skipped on trusty branch, but should be investigated')
            return 'skip'

        gdaltest.post_reason('fail')
        return 'fail'
    gdal.VSIFCloseL(f)

    with webserver.install_http_handler(webserver.SequentialHandler()):
        dir_contents = gdal.ReadDir('/vsioss/oss_fake_bucket2/a_dir')
    if dir_contents != ['resource3.bin', 'resource4.bin', 'subdir']:
        gdaltest.post_reason('fail')
        print(dir_contents)
        return 'fail'
    if gdal.VSIStatL('/vsioss/oss_fake_bucket2/a_dir/resource3.bin').size != 123456:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.VSIStatL('/vsioss/oss_fake_bucket2/a_dir/resource3.bin').mtime != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # ReadDir on something known to be a file shouldn't cause network access
    dir_contents = gdal.ReadDir('/vsioss/oss_fake_bucket2/a_dir/resource3.bin')
    if dir_contents is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test CPL_VSIL_CURL_NON_CACHED
    for config_option_value in [ '/vsioss/oss_non_cached/test.txt',
                        '/vsioss/oss_non_cached',
                        '/vsioss/oss_non_cached:/vsioss/unrelated',
                        '/vsioss/unrelated:/vsioss/oss_non_cached',
                        '/vsioss/unrelated:/vsioss/oss_non_cached:/vsioss/unrelated' ]:
      with gdaltest.config_option('CPL_VSIL_CURL_NON_CACHED', config_option_value):

        handler = webserver.SequentialHandler()
        handler.add('GET', '/oss_non_cached/test.txt', 200, {}, 'foo')

        with webserver.install_http_handler(handler):
            f = open_for_read('/vsioss/oss_non_cached/test.txt')
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

        handler = webserver.SequentialHandler()
        handler.add('GET', '/oss_non_cached/test.txt', 200, {}, 'bar2')

        with webserver.install_http_handler(handler):
            size = gdal.VSIStatL('/vsioss/oss_non_cached/test.txt').size
        if size != 4:
            gdaltest.post_reason('fail')
            print(config_option_value)
            print(size)
            return 'fail'

        handler = webserver.SequentialHandler()
        handler.add('GET', '/oss_non_cached/test.txt', 200, {}, 'foo')

        with webserver.install_http_handler(handler):
            size = gdal.VSIStatL('/vsioss/oss_non_cached/test.txt').size
            if size != 3:
                gdaltest.post_reason('fail')
                print(config_option_value)
                print(data)
                return 'fail'

        handler = webserver.SequentialHandler()
        handler.add('GET', '/oss_non_cached/test.txt', 200, {}, 'bar2')

        with webserver.install_http_handler(handler):
            f = open_for_read('/vsioss/oss_non_cached/test.txt')
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

    # Retry without option
    for config_option_value in [ None,
                                '/vsioss/oss_non_cached/bar.txt' ]:
      with gdaltest.config_option('CPL_VSIL_CURL_NON_CACHED', config_option_value):

        handler = webserver.SequentialHandler()
        if config_option_value is None:
            handler.add('GET', '/oss_non_cached/?delimiter=%2F', 200, { 'Content-type': 'application/xml' },
                        """<?xml version="1.0" encoding="UTF-8"?>
                        <ListBucketResult>
                            <Prefix>/</Prefix>
                            <Contents>
                                <Key>/test.txt</Key>
                                <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                                <Size>40</Size>
                            </Contents>
                            <Contents>
                                <Key>/test2.txt</Key>
                                <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                                <Size>40</Size>
                            </Contents>
                        </ListBucketResult>
                    """)
            handler.add('GET', '/oss_non_cached/test.txt', 200, {}, 'foo')

        with webserver.install_http_handler(handler):
            f = open_for_read('/vsioss/oss_non_cached/test.txt')
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

        handler = webserver.SequentialHandler()
        with webserver.install_http_handler(handler):
            f = open_for_read('/vsioss/oss_non_cached/test.txt')
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

    # List buckets (empty result)
    handler = webserver.SequentialHandler()
    handler.add('GET', '/', 200, { 'Content-type': 'application/xml' },
        """<?xml version="1.0" encoding="UTF-8"?>
        <ListAllMyBucketsResult>
        <Buckets>
        </Buckets>
        </ListAllMyBucketsResult>
        """)
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir('/vsioss/')
    if dir_contents != [ '.' ]:
        gdaltest.post_reason('fail')
        print(dir_contents)
        return 'fail'

    gdal.VSICurlClearCache()

    # List buckets
    handler = webserver.SequentialHandler()
    handler.add('GET', '/', 200, { 'Content-type': 'application/xml' },
        """<?xml version="1.0" encoding="UTF-8"?>
        <ListAllMyBucketsResult>
        <Buckets>
            <Bucket>
                <Name>mybucket</Name>
            </Bucket>
        </Buckets>
        </ListAllMyBucketsResult>
        """)
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir('/vsioss/')
    if dir_contents != [ 'mybucket' ]:
        gdaltest.post_reason('fail')
        print(dir_contents)
        return 'fail'

    return 'success'

###############################################################################
# Test simple PUT support with a fake OSS server

def visoss_4():

    if gdaltest.webserver_port == 0:
        return 'skip'

    with webserver.install_http_handler(webserver.SequentialHandler()):
        with gdaltest.error_handler():
            f = gdal.VSIFOpenL('/vsioss/oss_fake_bucket3', 'wb')
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oss_fake_bucket3/empty_file.bin', 200, {'Connection': 'close'}, 'foo')
    with webserver.install_http_handler(handler):
        if gdal.VSIStatL('/vsioss/oss_fake_bucket3/empty_file.bin').size != 3:
            gdaltest.post_reason('fail')
            return 'fail'

    # Empty file
    handler = webserver.SequentialHandler()

    def method(request):
        if request.headers['Content-Length'] != '0':
            sys.stderr.write('Did not get expected headers: %s\n' % str(request.headers))
            request.send_response(400)
            return

        request.send_response(200)
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add('PUT', '/oss_fake_bucket3/empty_file.bin', custom_method = method)

    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL('/vsioss/oss_fake_bucket3/empty_file.bin', 'wb')
        if f is None:
            gdaltest.post_reason('fail')
            return 'fail'
        gdal.ErrorReset()
        gdal.VSIFCloseL(f)
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oss_fake_bucket3/empty_file.bin', 200, {'Connection': 'close'}, '')
    with webserver.install_http_handler(handler):
        if gdal.VSIStatL('/vsioss/oss_fake_bucket3/empty_file.bin').size != 0:
            gdaltest.post_reason('fail')
            return 'fail'

    # Invalid seek
    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL('/vsioss/oss_fake_bucket3/empty_file.bin', 'wb')
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
    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL('/vsioss/oss_fake_bucket3/empty_file.bin', 'wb')
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
    handler = webserver.SequentialHandler()
    handler.add('PUT', '/oss_fake_bucket3/empty_file_error.bin', 403)
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL('/vsioss/oss_fake_bucket3/empty_file_error.bin', 'wb')
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
    with webserver.install_http_handler(webserver.SequentialHandler()):
        f = gdal.VSIFOpenL('/vsioss/oss_fake_bucket3/another_file.bin', 'wb')
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
        if gdal.VSIFSeekL(f, gdal.VSIFTellL(f), 0) != 0:
            gdaltest.post_reason('fail')
            return 'fail'
        if gdal.VSIFWriteL('bar', 1, 3, f) != 3:
            gdaltest.post_reason('fail')
            return 'fail'

    handler = webserver.SequentialHandler()

    def method(request):
        if request.headers['Content-Length'] != '6':
            sys.stderr.write('Did not get expected headers: %s\n' % str(request.headers))
            request.send_response(400)
            request.send_header('Content-Length', 0)
            request.end_headers()
            return

        request.wfile.write('HTTP/1.1 100 Continue\r\n\r\n'.encode('ascii'))

        content = request.rfile.read(6).decode('ascii')
        if content != 'foobar':
            sys.stderr.write('Did not get expected content: %s\n' % content)
            request.send_response(400)
            request.send_header('Content-Length', 0)
            request.end_headers()
            return

        request.send_response(200)
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add('PUT', '/oss_fake_bucket3/another_file.bin', custom_method = method)

    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        gdal.VSIFCloseL(f)
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test simple DELETE support with a fake OSS server

def visoss_5():

    if gdaltest.webserver_port == 0:
        return 'skip'

    with webserver.install_http_handler(webserver.SequentialHandler()):
        with gdaltest.error_handler():
            ret = gdal.Unlink('/vsioss/foo')
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oss_delete_bucket/delete_file', 200, {'Connection': 'close'}, 'foo')
    with webserver.install_http_handler(handler):
        if gdal.VSIStatL('/vsioss/oss_delete_bucket/delete_file').size != 3:
            gdaltest.post_reason('fail')
            return 'fail'

    with webserver.install_http_handler(webserver.SequentialHandler()):
        if gdal.VSIStatL('/vsioss/oss_delete_bucket/delete_file').size != 3:
            gdaltest.post_reason('fail')
            return 'fail'

    handler = webserver.SequentialHandler()
    handler.add('DELETE', '/oss_delete_bucket/delete_file', 204)
    with webserver.install_http_handler(handler):
        ret = gdal.Unlink('/vsioss/oss_delete_bucket/delete_file')
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'


    handler = webserver.SequentialHandler()
    handler.add('GET', '/oss_delete_bucket/delete_file', 404, {'Connection': 'close'}, 'foo')
    with webserver.install_http_handler(handler):
        if gdal.VSIStatL('/vsioss/oss_delete_bucket/delete_file') is not None:
            gdaltest.post_reason('fail')
            return 'fail'

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oss_delete_bucket/delete_file_error', 200)
    handler.add('DELETE', '/oss_delete_bucket/delete_file_error', 403)
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ret = gdal.Unlink('/vsioss/oss_delete_bucket/delete_file_error')
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test multipart upload with a fake OSS server

def visoss_6():

    if gdaltest.webserver_port == 0:
        return 'skip'

    with gdaltest.config_option('VSIOSS_CHUNK_SIZE', '1'): # 1 MB
        with webserver.install_http_handler(webserver.SequentialHandler()):
            f = gdal.VSIFOpenL('/vsioss/oss_fake_bucket4/large_file.bin', 'wb')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    size = 1024*1024+1
    big_buffer = ''.join('a' for i in range(size))

    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = 'HTTP/1.1'
        response = '<?xml version="1.0" encoding="UTF-8"?><InitiateMultipartUploadResult><UploadId>my_id</UploadId></InitiateMultipartUploadResult>'
        request.send_response(200)
        request.send_header('Content-type', 'application/xml')
        request.send_header('Content-Length', len(response))
        request.end_headers()
        request.wfile.write(response.encode('ascii'))

    handler.add('POST', '/oss_fake_bucket4/large_file.bin?uploads', custom_method = method)

    def method(request):
        if request.headers['Content-Length'] != '1048576':
            sys.stderr.write('Did not get expected headers: %s\n' % str(request.headers))
            request.send_response(400)
            request.send_header('Content-Length', 0)
            request.end_headers()
            return
        request.send_response(200)
        request.send_header('ETag', '"first_etag"')
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add('PUT', '/oss_fake_bucket4/large_file.bin?partNumber=1&uploadId=my_id', custom_method = method)

    with webserver.install_http_handler(handler):
        ret = gdal.VSIFWriteL(big_buffer, 1,size, f)
    if ret != size:
        gdaltest.post_reason('fail')
        return 'fail'
    handler = webserver.SequentialHandler()

    def method(request):
        if request.headers['Content-Length'] != '1':
            sys.stderr.write('Did not get expected headers: %s\n' % str(request.headers))
            request.send_response(400)
            return
        request.send_response(200)
        request.send_header('ETag', '"second_etag"')
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add('PUT', '/oss_fake_bucket4/large_file.bin?partNumber=2&uploadId=my_id', custom_method = method)

    def method(request):

        if request.headers['Content-Length'] != '186':
            sys.stderr.write('Did not get expected headers: %s\n' % str(request.headers))
            request.send_response(400)
            request.send_header('Content-Length', 0)
            request.end_headers()
            return

        content = request.rfile.read(186).decode('ascii')
        if content != """<CompleteMultipartUpload>
<Part>
<PartNumber>1</PartNumber><ETag>"first_etag"</ETag></Part>
<Part>
<PartNumber>2</PartNumber><ETag>"second_etag"</ETag></Part>
</CompleteMultipartUpload>
""":
            sys.stderr.write('Did not get expected content: %s\n' % content)
            request.send_response(400)
            request.send_header('Content-Length', 0)
            request.end_headers()
            return

        request.send_response(200)
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add('POST', '/oss_fake_bucket4/large_file.bin?uploadId=my_id', custom_method = method)

    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        gdal.VSIFCloseL(f)
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    handler = webserver.SequentialHandler()
    handler.add('POST', '/oss_fake_bucket4/large_file_initiate_403_error.bin?uploads', 403)
    handler.add('POST', '/oss_fake_bucket4/large_file_initiate_empty_result.bin?uploads', 200)
    handler.add('POST', '/oss_fake_bucket4/large_file_initiate_invalid_xml_result.bin?uploads', 200, {}, 'foo')
    handler.add('POST', '/oss_fake_bucket4/large_file_initiate_no_uploadId.bin?uploads', 200, {}, '<foo/>')
    with webserver.install_http_handler(handler):
      for filename in [ '/vsioss/oss_fake_bucket4/large_file_initiate_403_error.bin',
                        '/vsioss/oss_fake_bucket4/large_file_initiate_empty_result.bin',
                        '/vsioss/oss_fake_bucket4/large_file_initiate_invalid_xml_result.bin',
                        '/vsioss/oss_fake_bucket4/large_file_initiate_no_uploadId.bin' ]:
        with gdaltest.config_option('VSIOSS_CHUNK_SIZE', '1'): # 1 MB
            f = gdal.VSIFOpenL(filename, 'wb')
        if f is None:
            gdaltest.post_reason('fail')
            return 'fail'
        with gdaltest.error_handler():
            ret = gdal.VSIFWriteL(big_buffer, 1,size, f)
        if ret != 0:
            gdaltest.post_reason('fail')
            print(ret)
            return 'fail'
        gdal.ErrorReset()
        gdal.VSIFCloseL(f)
        if gdal.GetLastErrorMsg() != '':
            gdaltest.post_reason('fail')
            return 'fail'

    handler = webserver.SequentialHandler()
    handler.add('POST', '/oss_fake_bucket4/large_file_upload_part_403_error.bin?uploads', 200, {},
                '<?xml version="1.0" encoding="UTF-8"?><InitiateMultipartUploadResult><UploadId>my_id</UploadId></InitiateMultipartUploadResult>')
    handler.add('PUT', '/oss_fake_bucket4/large_file_upload_part_403_error.bin?partNumber=1&uploadId=my_id', 403)
    handler.add('DELETE', '/oss_fake_bucket4/large_file_upload_part_403_error.bin?uploadId=my_id', 204)

    handler.add('POST', '/oss_fake_bucket4/large_file_upload_part_no_etag.bin?uploads', 200, {},
                '<?xml version="1.0" encoding="UTF-8"?><InitiateMultipartUploadResult><UploadId>my_id</UploadId></InitiateMultipartUploadResult>')
    handler.add('PUT', '/oss_fake_bucket4/large_file_upload_part_no_etag.bin?partNumber=1&uploadId=my_id', 200)
    handler.add('DELETE', '/oss_fake_bucket4/large_file_upload_part_no_etag.bin?uploadId=my_id', 204)

    with webserver.install_http_handler(handler):
      for filename in [ '/vsioss/oss_fake_bucket4/large_file_upload_part_403_error.bin',
                        '/vsioss/oss_fake_bucket4/large_file_upload_part_no_etag.bin']:
        with gdaltest.config_option('VSIOSS_CHUNK_SIZE', '1'): # 1 MB
            f = gdal.VSIFOpenL(filename, 'wb')
        if f is None:
            gdaltest.post_reason('fail')
            print(filename)
            return 'fail'
        with gdaltest.error_handler():
            ret = gdal.VSIFWriteL(big_buffer, 1,size, f)
        if ret != 0:
            gdaltest.post_reason('fail')
            print(filename)
            print(ret)
            return 'fail'
        gdal.ErrorReset()
        gdal.VSIFCloseL(f)
        if gdal.GetLastErrorMsg() != '':
            gdaltest.post_reason('fail')
            print(filename)
            return 'fail'

    # Simulate failure in AbortMultipart stage
    handler = webserver.SequentialHandler()
    handler.add('POST', '/oss_fake_bucket4/large_file_abortmultipart_403_error.bin?uploads', 200, {},
                '<?xml version="1.0" encoding="UTF-8"?><InitiateMultipartUploadResult><UploadId>my_id</UploadId></InitiateMultipartUploadResult>')
    handler.add('PUT', '/oss_fake_bucket4/large_file_abortmultipart_403_error.bin?partNumber=1&uploadId=my_id', 403)
    handler.add('DELETE', '/oss_fake_bucket4/large_file_abortmultipart_403_error.bin?uploadId=my_id', 403)

    filename = '/vsioss/oss_fake_bucket4/large_file_abortmultipart_403_error.bin'
    with webserver.install_http_handler(handler):
        with gdaltest.config_option('VSIOSS_CHUNK_SIZE', '1'): # 1 MB
            f = gdal.VSIFOpenL(filename, 'wb')
        if f is None:
            gdaltest.post_reason('fail')
            print(filename)
            return 'fail'
        with gdaltest.error_handler():
            ret = gdal.VSIFWriteL(big_buffer, 1,size, f)
        if ret != 0:
            gdaltest.post_reason('fail')
            print(filename)
            print(ret)
            return 'fail'
        gdal.ErrorReset()
        with gdaltest.error_handler():
            gdal.VSIFCloseL(f)
        if gdal.GetLastErrorMsg() == '':
            gdaltest.post_reason('fail')
            print(filename)
            return 'fail'

    # Simulate failure in CompleteMultipartUpload stage
    handler = webserver.SequentialHandler()
    handler.add('POST', '/oss_fake_bucket4/large_file_completemultipart_403_error.bin?uploads', 200, {},
                '<?xml version="1.0" encoding="UTF-8"?><InitiateMultipartUploadResult><UploadId>my_id</UploadId></InitiateMultipartUploadResult>')
    handler.add('PUT', '/oss_fake_bucket4/large_file_completemultipart_403_error.bin?partNumber=1&uploadId=my_id', 200, { 'ETag': 'first_etag' }, '')
    handler.add('PUT', '/oss_fake_bucket4/large_file_completemultipart_403_error.bin?partNumber=2&uploadId=my_id', 200, { 'ETag': 'second_etag' }, '')
    handler.add('POST', '/oss_fake_bucket4/large_file_completemultipart_403_error.bin?uploadId=my_id', 403)
    #handler.add('DELETE', '/oss_fake_bucket4/large_file_completemultipart_403_error.bin?uploadId=my_id', 204)

    filename = '/vsioss/oss_fake_bucket4/large_file_completemultipart_403_error.bin'
    with webserver.install_http_handler(handler):
        with gdaltest.config_option('VSIOSS_CHUNK_SIZE', '1'): # 1 MB
            f = gdal.VSIFOpenL(filename, 'wb')
            if f is None:
                gdaltest.post_reason('fail')
                print(filename)
                return 'fail'
            ret = gdal.VSIFWriteL(big_buffer, 1,size, f)
            if ret != size:
                gdaltest.post_reason('fail')
                print(filename)
                print(ret)
                return 'fail'
            gdal.ErrorReset()
            with gdaltest.error_handler():
                gdal.VSIFCloseL(f)
            if gdal.GetLastErrorMsg() == '':
                gdaltest.post_reason('fail')
                print(filename)
                return 'fail'

    return 'success'

###############################################################################
# Test Mkdir() / Rmdir()

def visoss_7():

    if gdaltest.webserver_port == 0:
        return 'skip'

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oss_bucket_test_mkdir/dir/', 404, {'Connection':'close'})
    handler.add('PUT', '/oss_bucket_test_mkdir/dir/', 200)
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir('/vsioss/oss_bucket_test_mkdir/dir', 0)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Try creating already existing directory
    handler = webserver.SequentialHandler()
    handler.add('GET', '/oss_bucket_test_mkdir/dir/', 416)
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir('/vsioss/oss_bucket_test_mkdir/dir', 0)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    handler = webserver.SequentialHandler()
    handler.add('GET', '/oss_bucket_test_mkdir/?delimiter=%2F&max-keys=1&prefix=dir%2F', 200,
                 { 'Content-type': 'application/xml', 'Connection':'close' },
                 """<?xml version="1.0" encoding="UTF-8"?>
                    <ListBucketResult>
                        <Prefix>dir/</Prefix>
                        <Contents>
                        </Contents>
                    </ListBucketResult>
                """)
    handler.add('DELETE', '/oss_bucket_test_mkdir/dir/', 204)
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir('/vsioss/oss_bucket_test_mkdir/dir')
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Try deleting already deleted directory
    handler = webserver.SequentialHandler()
    handler.add('GET', '/oss_bucket_test_mkdir/dir/', 404)
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir('/vsioss/oss_bucket_test_mkdir/dir')
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Try deleting non-empty directory
    handler = webserver.SequentialHandler()
    handler.add('GET', '/oss_bucket_test_mkdir/dir_nonempty/', 416)
    handler.add('GET', '/oss_bucket_test_mkdir/?delimiter=%2F&max-keys=1&prefix=dir_nonempty%2F', 200,
                 { 'Content-type': 'application/xml' },
                 """<?xml version="1.0" encoding="UTF-8"?>
                    <ListBucketResult>
                        <Prefix>dir_nonempty/</Prefix>
                        <Contents>
                            <Key>dir_nonempty/test.txt</Key>
                            <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                            <Size>40</Size>
                        </Contents>
                    </ListBucketResult>
                """)
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir('/vsioss/oss_bucket_test_mkdir/dir_nonempty')
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test handling of file and directory with same name

def visoss_8():

    if gdaltest.webserver_port == 0:
        return 'skip'

    handler = webserver.SequentialHandler()
    handler.add('GET', '/visoss_8/?delimiter=%2F', 200,
                 { 'Content-type': 'application/xml' },
                 """<?xml version="1.0" encoding="UTF-8"?>
                    <ListBucketResult>
                        <Prefix></Prefix>
                        <Contents>
                            <Key>test</Key>
                            <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                            <Size>40</Size>
                        </Contents>
                        <CommonPrefixes>
                            <Prefix>test/</Prefix>
                        </CommonPrefixes>
                    </ListBucketResult>
                """)

    with webserver.install_http_handler(handler):
        listdir = gdal.ReadDir('/vsioss/visoss_8', 0)
    if listdir != [ 'test', 'test/' ]:
        gdaltest.post_reason('fail')
        print(listdir)
        return 'fail'

    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        if stat.S_ISDIR(gdal.VSIStatL('/vsioss/visoss_8/test').mode):
            gdaltest.post_reason('fail')
            return 'fail'

    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        if not stat.S_ISDIR(gdal.VSIStatL('/vsioss/visoss_8/test/').mode):
            gdaltest.post_reason('fail')
            return 'fail'

    return 'success'

###############################################################################
def visoss_stop_webserver():

    if gdaltest.webserver_port == 0:
        return 'skip'

    # Clearcache needed to close all connections, since the Python server
    # can only handle one connection at a time
    gdal.VSICurlClearCache()

    webserver.server_stop(gdaltest.webserver_process, gdaltest.webserver_port)

    return 'success'

###############################################################################
# Nominal cases (require valid credentials)

def visoss_extra_1():

    if not gdaltest.built_against_curl():
        return 'skip'

    # Either a bucket name or bucket/filename
    OSS_RESOURCE = gdal.GetConfigOption('OSS_RESOURCE')

    if gdal.GetConfigOption('OSS_SECRET_ACCESS_KEY') is None:
        print('Missing OSS_SECRET_ACCESS_KEY for running gdaltest_list_extra')
        return 'skip'
    elif gdal.GetConfigOption('OSS_ACCESS_KEY_ID') is None:
        print('Missing OSS_ACCESS_KEY_ID for running gdaltest_list_extra')
        return 'skip'
    elif OSS_RESOURCE is None:
        print('Missing OSS_RESOURCE for running gdaltest_list_extra')
        return 'skip'

    if OSS_RESOURCE.find('/') < 0:
        path = '/vsioss/' + OSS_RESOURCE
        statres = gdal.VSIStatL(path)
        if statres is None or not stat.S_ISDIR(statres.mode):
            gdaltest.post_reason('fail')
            print('%s is not a valid bucket' % path)
            return 'fail'

        readdir = gdal.ReadDir(path)
        if readdir is None:
            gdaltest.post_reason('fail')
            print('ReadDir() should not return empty list')
            return 'fail'
        for filename in readdir:
            if filename != '.':
                subpath = path + '/' + filename
                if gdal.VSIStatL(subpath) is None:
                    gdaltest.post_reason('fail')
                    print('Stat(%s) should not return an error' % subpath)
                    return 'fail'

        unique_id = 'visoss_test'
        subpath = path + '/' + unique_id
        ret = gdal.Mkdir(subpath, 0)
        if ret < 0:
            gdaltest.post_reason('fail')
            print('Mkdir(%s) should not return an error' % subpath)
            return 'fail'

        readdir = gdal.ReadDir(path)
        if unique_id not in readdir:
            gdaltest.post_reason('fail')
            print('ReadDir(%s) should contain %s' % (path, unique_id))
            print(readdir)
            return 'fail'

        ret = gdal.Mkdir(subpath, 0)
        if ret == 0:
            gdaltest.post_reason('fail')
            print('Mkdir(%s) repeated should return an error' % subpath)
            return 'fail'

        ret = gdal.Rmdir(subpath)
        if ret < 0:
            gdaltest.post_reason('fail')
            print('Rmdir(%s) should not return an error' % subpath)
            return 'fail'

        readdir = gdal.ReadDir(path)
        if unique_id in readdir:
            gdaltest.post_reason('fail')
            print('ReadDir(%s) should not contain %s' % (path, unique_id))
            print(readdir)
            return 'fail'

        ret = gdal.Rmdir(subpath)
        if ret == 0:
            gdaltest.post_reason('fail')
            print('Rmdir(%s) repeated should return an error' % subpath)
            return 'fail'

        ret = gdal.Mkdir(subpath, 0)
        if ret < 0:
            gdaltest.post_reason('fail')
            print('Mkdir(%s) should not return an error' % subpath)
            return 'fail'

        f = gdal.VSIFOpenL(subpath + '/test.txt', 'wb')
        if f is None:
            gdaltest.post_reason('fail')
            return 'fail'
        gdal.VSIFWriteL('hello', 1, 5, f)
        gdal.VSIFCloseL(f)

        ret = gdal.Rmdir(subpath)
        if ret == 0:
            gdaltest.post_reason('fail')
            print('Rmdir(%s) on non empty directory should return an error' % subpath)
            return 'fail'

        f = gdal.VSIFOpenL(subpath + '/test.txt', 'rb')
        if f is None:
            gdaltest.post_reason('fail')
            return 'fail'
        data = gdal.VSIFReadL(1, 5, f).decode('utf-8')
        if data != 'hello':
            gdaltest.post_reason('fail')
            print(data)
            return 'fail'
        gdal.VSIFCloseL(f)

        ret = gdal.Unlink(subpath + '/test.txt')
        if ret < 0:
            gdaltest.post_reason('fail')
            print('Unlink(%s) should not return an error' % (subpath + '/test.txt'))
            return 'fail'


        ret = gdal.Rmdir(subpath)
        if ret < 0:
            gdaltest.post_reason('fail')
            print('Rmdir(%s) should not return an error' % subpath)
            return 'fail'


        return 'success'

    f = open_for_read('/vsioss/' + OSS_RESOURCE)
    if f is None:
        gdaltest.post_reason('fail')
        print('cannot open %s' % ('/vsioss/' + OSS_RESOURCE))
        return 'fail'
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    if len(ret) != 1:
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'

    # Same with /vsioss_streaming/
    f = open_for_read('/vsioss_streaming/' + OSS_RESOURCE)
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
    f = open_for_read('/vsioss/not_existing_bucket/foo')
    with gdaltest.error_handler():
        gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)
    if gdal.VSIGetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    # Invalid resource
    gdal.ErrorReset()
    f = open_for_read('/vsioss_streaming/' + gdal.GetConfigOption('OSS_RESOURCE') + '/invalid_resource.baz')
    if f is not None:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
def visoss_cleanup():

    for var in gdaltest.oss_vars:
        gdal.SetConfigOption(var, gdaltest.oss_vars[var])

    return 'success'

gdaltest_list = [ visoss_init,
                  visoss_1,
                  visoss_start_webserver,
                  visoss_2,
                  visoss_3,
                  visoss_4,
                  visoss_5,
                  visoss_6,
                  visoss_7,
                  visoss_8,
                  visoss_stop_webserver,
                  visoss_cleanup ]

# gdaltest_list = [ visoss_init, visoss_start_webserver, visoss_8, visoss_stop_webserver, visoss_cleanup ]

gdaltest_list_extra = [ visoss_extra_1 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'vsioss' )

    if gdal.GetConfigOption('RUN_MANUAL_ONLY', None):
        gdaltest.run_tests( gdaltest_list_extra )
    else:
        gdaltest.run_tests( gdaltest_list + gdaltest_list_extra + [ visoss_cleanup ] )

    gdaltest.summarize()
