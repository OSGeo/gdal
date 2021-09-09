#!/usr/bin/env pytest
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

import json
import os.path
import stat
import sys
from osgeo import gdal


import gdaltest
import webserver
import pytest


def open_for_read(uri):
    """
    Opens a test file for reading.
    """
    return gdal.VSIFOpenExL(uri, 'rb', 1)


@pytest.fixture()
def aws_test_config():
    options = {
        # To avoid user AWS credentials in ~/.aws/credentials
        # and ~/.aws/config to mess up our tests
        'CPL_AWS_CREDENTIALS_FILE': '',
        'AWS_CONFIG_FILE': '',
        'CPL_AWS_EC2_API_ROOT_URL': '',
        'AWS_NO_SIGN_REQUEST': 'NO',
        'AWS_SECRET_ACCESS_KEY': 'AWS_SECRET_ACCESS_KEY',
        'AWS_ACCESS_KEY_ID': 'AWS_ACCESS_KEY_ID',
        'AWS_TIMESTAMP': '20150101T000000Z',
        'AWS_HTTPS': 'NO',
        'AWS_VIRTUAL_HOSTING': 'NO',
        'AWS_REQUEST_PAYER': '',
        'AWS_DEFAULT_REGION': 'us-east-1',
        'AWS_DEFAULT_PROFILE': 'default',
        'AWS_PROFILE': 'default'
    }

    with gdaltest.config_options(options):
        yield


@pytest.fixture(scope="module")
def webserver_port():
    if not gdaltest.built_against_curl():
        pytest.skip()

    webserver_process, webserver_port = webserver.launch(
        handler=webserver.DispatcherHttpHandler
    )
    try:
        if webserver_port == 0:
            pytest.skip()
        with gdaltest.config_option(
                'AWS_S3_ENDPOINT', f'127.0.0.1:{webserver_port}',
        ):
            yield webserver_port
    finally:
        gdal.VSICurlClearCache()

        webserver.server_stop(webserver_process, webserver_port)


###############################################################################


def test_vsis3_init(aws_test_config):
    options = {
        'AWS_SECRET_ACCESS_KEY': '',
        'AWS_ACCESS_KEY_ID': '',
    }
    with gdaltest.config_options(options):
        assert gdal.GetSignedURL('/vsis3/foo/bar') is None

###############################################################################
# Test AWS_NO_SIGN_REQUEST=YES


def test_vsis3_no_sign_request(aws_test_config):

    if not gdaltest.built_against_curl():
        pytest.skip()

    options = {
        'AWS_S3_ENDPOINT': 's3.amazonaws.com',
        'AWS_NO_SIGN_REQUEST': 'YES',
        'AWS_HTTPS': 'YES',
        'AWS_VIRTUAL_HOSTING': 'TRUE'
    }

    with gdaltest.config_options(options):
        actual_url = gdal.GetActualURL(
            '/vsis3/landsat-pds/L8/001/002/'
            'LC80010022016230LGN00/LC80010022016230LGN00_B1.TIF'
        )
        assert actual_url == (
            'https://landsat-pds.s3.amazonaws.com/L8/001/002/'
            'LC80010022016230LGN00/LC80010022016230LGN00_B1.TIF'
        )

        actual_url = gdal.GetActualURL(
            '/vsis3_streaming/landsat-pds/L8/001/002/'
            'LC80010022016230LGN00/LC80010022016230LGN00_B1.TIF'
        )
        assert actual_url == (
            'https://landsat-pds.s3.amazonaws.com/L8/001/002/'
            'LC80010022016230LGN00/LC80010022016230LGN00_B1.TIF'
        )

        f = open_for_read(
            '/vsis3/landsat-pds/L8/001/002/'
            'LC80010022016230LGN00/LC80010022016230LGN00_B1.TIF'
        )
        if f is None:
            if gdaltest.gdalurlopen(
                'https://landsat-pds.s3.amazonaws.com/L8/001/002/'
                'LC80010022016230LGN00/LC80010022016230LGN00_B1.TIF'
            ) is None:
                pytest.skip('cannot open URL')
            pytest.fail()
        gdal.VSIFCloseL(f)


###############################################################################
# Test Sync() and multithreaded download


def test_vsis3_sync_multithreaded_download(aws_test_config):

    if not gdaltest.built_against_curl():
        pytest.skip()

    def cbk(pct, _, tab):
        assert pct >= tab[0]
        tab[0] = pct
        return True

    tab = [-1]
    options = {
        'AWS_S3_ENDPOINT': 's3.amazonaws.com',
        'AWS_NO_SIGN_REQUEST': 'YES',
        'AWS_VIRTUAL_HOSTING': 'FALSE'
    }
    # Use a public bucket with /test_dummy/foo and /test_dummy/bar files
    with gdaltest.config_options(options):
        assert gdal.Sync('/vsis3/cdn.proj.org/test_dummy',
                         '/vsimem/test_vsis3_no_sign_request_sync',
                         options=['NUM_THREADS=2'],
                         callback=cbk, callback_data=tab)
    assert tab[0] == 1.0
    assert gdal.VSIStatL(
        '/vsimem/test_vsis3_no_sign_request_sync/test_dummy/foo'
    ).size == 4
    assert gdal.VSIStatL(
        '/vsimem/test_vsis3_no_sign_request_sync/test_dummy/bar'
    ).size == 4
    gdal.RmdirRecursive('/vsimem/test_vsis3_no_sign_request_sync')


###############################################################################
# Test Sync() and multithreaded download and CHUNK_SIZE


def test_vsis3_sync_multithreaded_download_chunk_size(aws_test_config):

    if not gdaltest.built_against_curl():
        pytest.skip()

    def cbk(pct, _, tab):
        assert pct >= tab[0]
        tab[0] = pct
        return True

    tab = [-1]
    options = {
        'AWS_S3_ENDPOINT': 's3.amazonaws.com',
        'AWS_NO_SIGN_REQUEST': 'YES',
        'AWS_VIRTUAL_HOSTING': 'FALSE'
    }
    # Use a public bucket with /test_dummy/foo and /test_dummy/bar files
    with gdaltest.config_options(options):
        assert gdal.Sync('/vsis3/cdn.proj.org/test_dummy',
                         '/vsimem/test_vsis3_no_sign_request_sync',
                         options=['NUM_THREADS=2', 'CHUNK_SIZE=3'],
                         callback=cbk, callback_data=tab)
    assert tab[0] == 1.0
    assert gdal.VSIStatL(
        '/vsimem/test_vsis3_no_sign_request_sync/test_dummy/foo'
    ).size == 4
    assert gdal.VSIStatL(
        '/vsimem/test_vsis3_no_sign_request_sync/test_dummy/bar'
    ).size == 4

    gdal.RmdirRecursive('/vsimem/test_vsis3_no_sign_request_sync')

###############################################################################
# Error cases


def test_vsis3_1(aws_test_config):

    if not gdaltest.built_against_curl():
        pytest.skip()

    # Missing AWS_SECRET_ACCESS_KEY
    with gdaltest.config_options({
            'AWS_SECRET_ACCESS_KEY': ''
    }):
        gdal.ErrorReset()

        with gdaltest.error_handler():
            f = open_for_read('/vsis3/foo/bar')
        assert f is None
        assert gdal.VSIGetLastErrorMsg().find('AWS_SECRET_ACCESS_KEY') >= 0

        gdal.ErrorReset()
        with gdaltest.error_handler():
            f = open_for_read('/vsis3_streaming/foo/bar')
        assert f is None
        assert gdal.VSIGetLastErrorMsg().find('AWS_SECRET_ACCESS_KEY') >= 0

    with gdaltest.config_options({
            'AWS_SECRET_ACCESS_KEY': 'AWS_SECRET_ACCESS_KEY',
            'AWS_ACCESS_KEY_ID': ''
    }):
        # Missing AWS_ACCESS_KEY_ID
        gdal.ErrorReset()
        with gdaltest.error_handler():
            f = open_for_read('/vsis3/foo/bar')
        assert f is None
        assert gdal.VSIGetLastErrorMsg().find('AWS_ACCESS_KEY_ID') >= 0

    with gdaltest.config_options({
            'AWS_SECRET_ACCESS_KEY': 'AWS_SECRET_ACCESS_KEY',
            'AWS_ACCESS_KEY_ID': 'AWS_ACCESS_KEY_ID'
    }):
        # ERROR 1: The AWS Access Key Id you provided does not exist in our
        # records.
        gdal.ErrorReset()
        with gdaltest.error_handler():
            f = open_for_read('/vsis3/foo/bar.baz')
        if f is not None or gdal.VSIGetLastErrorMsg() == '':
            if f is not None:
                gdal.VSIFCloseL(f)
            if gdal.GetConfigOption('APPVEYOR') is not None:
                return
            pytest.fail(gdal.VSIGetLastErrorMsg())

        gdal.ErrorReset()
        with gdaltest.error_handler():
            f = open_for_read('/vsis3_streaming/foo/bar.baz')
        assert f is None and gdal.VSIGetLastErrorMsg() != ''

###############################################################################


def get_s3_fake_bucket_resource_method(request):
    request.protocol_version = 'HTTP/1.1'

    if 'Authorization' not in request.headers:
        sys.stderr.write('Bad headers: %s\n' % str(request.headers))
        request.send_response(403)
        return
    expected_authorization_8080 = (
        'AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1/'
        's3/aws4_request,SignedHeaders=host;x-amz-content-sha256;'
        'x-amz-date,Signature='
        '38901846b865b12ac492bc005bb394ca8d60c098b68db57c084fac686a932f9e'
    )
    expected_authorization_8081 = (
        'AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1/'
        's3/aws4_request,SignedHeaders=host;x-amz-content-sha256;'
        'x-amz-date,Signature='
        '9f623b7ffce76188a456c70fb4813eb31969e88d130d6b4d801b3accbf050d6c'
    )
    actual_authorization = request.headers['Authorization']
    if (actual_authorization not in
            (expected_authorization_8080, expected_authorization_8081)):
        sys.stderr.write(
            "Bad Authorization: '%s'\n" % str(actual_authorization)
        )
        request.send_response(403)
        return

    request.send_response(200)
    request.send_header('Content-type', 'text/plain')
    request.send_header('Content-Length', 3)
    request.send_header('Connection', 'close')
    request.end_headers()
    request.wfile.write("""foo""".encode('ascii'))

###############################################################################
# Test with a fake AWS server


def test_vsis3_2(aws_test_config, webserver_port):
    signed_url = gdal.GetSignedURL('/vsis3/s3_fake_bucket/resource')
    expected_url_8080 = (
        'http://127.0.0.1:8080/s3_fake_bucket/resource'
        '?X-Amz-Algorithm=AWS4-HMAC-SHA256'
        '&X-Amz-Credential='
        'AWS_ACCESS_KEY_ID%2F20150101%2Fus-east-1%2Fs3%2Faws4_request'
        '&X-Amz-Date=20150101T000000Z&X-Amz-Expires=3600'
        '&X-Amz-Signature='
        'dca239dd95f72ff8c37c15c840afc54cd19bdb07f7aaee2223108b5b0ad35da8'
        '&X-Amz-SignedHeaders=host'
    )
    expected_url_8081 = (
        'http://127.0.0.1:8081/s3_fake_bucket/resource'
        '?X-Amz-Algorithm=AWS4-HMAC-SHA256'
        '&X-Amz-Credential='
        'AWS_ACCESS_KEY_ID%2F20150101%2Fus-east-1%2Fs3%2Faws4_request'
        '&X-Amz-Date=20150101T000000Z&X-Amz-Expires=3600'
        '&X-Amz-Signature='
        'ef5216bc5971863414c69f6ca095276c0d62c0da97fa4f6ab80c30bd7fc146ac'
        '&X-Amz-SignedHeaders=host'
    )
    assert signed_url in (expected_url_8080, expected_url_8081)

    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/s3_fake_bucket/resource',
        custom_method=get_s3_fake_bucket_resource_method
    )

    with webserver.install_http_handler(handler):
        f = open_for_read('/vsis3/s3_fake_bucket/resource')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

        assert data == 'foo'

    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/s3_fake_bucket/resource',
        custom_method=get_s3_fake_bucket_resource_method
    )
    with webserver.install_http_handler(handler):
        f = open_for_read('/vsis3_streaming/s3_fake_bucket/resource')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    assert data == 'foo'

    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = 'HTTP/1.1'

        if 'Authorization' not in request.headers:
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)
            return
        expected_authorization_8080 = (
            'AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1'
            '/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;'
            'x-amz-date;x-amz-security-token,'
            'Signature='
            '464a21835038b4f4d292b6463b8a005b9aaa980513aa8c42fc170abb733dce85'
        )
        expected_authorization_8081 = (
            'AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1'
            '/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;'
            'x-amz-date;x-amz-security-token,'
            'Signature='
            'b10e91575186342f9f2acfc91c4c2c9938c4a9e8cdcbc043d09d59d9641ad7fb'
        )
        actual_authorization = request.headers['Authorization']
        if (actual_authorization not in
                (expected_authorization_8080, expected_authorization_8081)):
            sys.stderr.write(
                "Bad Authorization: '%s'\n" % str(actual_authorization)
            )
            request.send_response(403)
            return

        request.send_response(200)
        request.send_header('Content-type', 'text/plain')
        request.send_header('Content-Length', 3)
        request.end_headers()
        request.wfile.write("""foo""".encode('ascii'))

    handler.add(
        'GET',
        '/s3_fake_bucket_with_session_token/resource',
        custom_method=method
    )

    # Test with temporary credentials
    with gdaltest.config_option('AWS_SESSION_TOKEN', 'AWS_SESSION_TOKEN'):
        with webserver.install_http_handler(handler):
            f = open_for_read(
                '/vsis3/s3_fake_bucket_with_session_token/resource'
            )
            assert f is not None
            data = gdal.VSIFReadL(1, 4, f).decode('ascii')
            gdal.VSIFCloseL(f)

    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = 'HTTP/1.1'
        if 'Range' in request.headers:
            if request.headers['Range'] != 'bytes=0-16383':
                sys.stderr.write(
                    "Bad Range: '%s'\n" % str(request.headers['Range'])
                )
                request.send_response(403)
                return
            request.send_response(206)
            request.send_header('Content-type', 'text/plain')
            request.send_header('Content-Range', 'bytes 0-16383/1000000')
            request.send_header('Content-Length', 16384)
            request.send_header('Connection', 'close')
            request.end_headers()
            request.wfile.write(('a' * 16384).encode('ascii'))
        else:
            request.send_response(200)
            request.send_header('Content-type', 'text/plain')
            request.send_header('Content-Length', 1000000)
            request.send_header('Connection', 'close')
            request.end_headers()
            request.wfile.write(('a' * 1000000).encode('ascii'))

    handler.add('GET', '/s3_fake_bucket/resource2.bin', custom_method=method)

    with webserver.install_http_handler(handler):
        # old_val = gdal.GetConfigOption('GDAL_DISABLE_READDIR_ON_OPEN')
        # gdal.SetConfigOption('GDAL_DISABLE_READDIR_ON_OPEN', 'EMPTY_DIR')
        stat_res = gdal.VSIStatL('/vsis3/s3_fake_bucket/resource2.bin')
        # gdal.SetConfigOption('GDAL_DISABLE_READDIR_ON_OPEN', old_val)
        if stat_res is None or stat_res.size != 1000000:
            if stat_res is not None:
                print(stat_res.size)
            else:
                print(stat_res)
            pytest.fail()

    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/s3_fake_bucket/resource2.bin', 200,
                {'Content-type': 'text/plain',
                 'Content-Length': 1000000,
                 'Connection': 'close'})
    with webserver.install_http_handler(handler):
        stat_res = gdal.VSIStatL(
            '/vsis3_streaming/s3_fake_bucket/resource2.bin'
        )
    if stat_res is None or stat_res.size != 1000000:
        if stat_res is not None:
            print(stat_res.size)
        else:
            print(stat_res)
        pytest.fail()

    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = 'HTTP/1.1'
        if request.headers['Authorization'].find('us-east-1') >= 0:
            request.send_response(400)
            response = '''<?xml version="1.0" encoding="UTF-8"?>
            <Error>
            <Message>bla</Message>
            <Code>AuthorizationHeaderMalformed</Code>
            <Region>us-west-2</Region>
            </Error>'
            '''
            response = '%x\r\n%s\r\n0\r\n\r\n' % (len(response), response)
            request.send_header('Content-type', 'application/xml')
            request.send_header('Transfer-Encoding', 'chunked')
            request.send_header('Connection', 'close')
            request.end_headers()
            request.wfile.write(response.encode('ascii'))
        else:
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)

    handler.add('GET', '/s3_fake_bucket/redirect', custom_method=method)

    def method(request):
        request.protocol_version = 'HTTP/1.1'
        includes_us_west_2 = request.headers['Authorization'].find(
            'us-west-2'
        ) >= 0
        host_is_127_0_0_1 = request.headers['Host'].startswith('127.0.0.1')

        if includes_us_west_2 and host_is_127_0_0_1:
            request.send_response(301)
            response = '''<?xml version="1.0" encoding="UTF-8"?>
            <Error>
            <Message>bla</Message>
            <Code>PermanentRedirect</Code>
            <Endpoint>localhost:%d</Endpoint>
            </Error>''' % request.server.port
            response = '%x\r\n%s\r\n0\r\n\r\n' % (len(response), response)
            request.send_header('Content-type', 'application/xml')
            request.send_header('Transfer-Encoding', 'chunked')
            request.send_header('Connection', 'close')
            request.end_headers()
            request.wfile.write(response.encode('ascii'))
        else:
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)

    handler.add('GET', '/s3_fake_bucket/redirect', custom_method=method)

    def method(request):
        request.protocol_version = 'HTTP/1.1'
        includes_us_west_2 = request.headers['Authorization'].find(
            'us-west-2'
        ) >= 0
        host_is_localhost = request.headers['Host'].startswith('localhost')

        if includes_us_west_2 and host_is_localhost:
            request.send_response(200)
            request.send_header('Content-type', 'text/plain')
            request.send_header('Content-Length', 3)
            request.send_header('Connection', 'close')
            request.end_headers()
            request.wfile.write("""foo""".encode('ascii'))
        else:
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)

    handler.add('GET', '/s3_fake_bucket/redirect', custom_method=method)

    # Test region and endpoint 'redirects'
    with webserver.install_http_handler(handler):
        f = open_for_read('/vsis3/s3_fake_bucket/redirect')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    if data != 'foo':

        if gdaltest.is_travis_branch('trusty'):
            pytest.skip('Skipped on trusty branch, but should be investigated')

        pytest.fail(data)

    # Test region and endpoint 'redirects'
    gdal.VSICurlClearCache()

    handler.req_count = 0
    with webserver.install_http_handler(handler):
        f = open_for_read('/vsis3_streaming/s3_fake_bucket/redirect')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    assert data == 'foo'

    handler = webserver.SequentialHandler()

    def method(request):
        # /vsis3_streaming/ should have remembered the change of region and
        # endpoint
        if request.headers['Authorization'].find('us-west-2') < 0 or \
                not request.headers['Host'].startswith('localhost'):
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

    handler.add('GET', '/s3_fake_bucket/non_xml_error', custom_method=method)

    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            f = open_for_read('/vsis3_streaming/s3_fake_bucket/non_xml_error')
    assert f is None and gdal.VSIGetLastErrorMsg().find('bla') >= 0

    handler = webserver.SequentialHandler()
    response = '<?xml version="1.0" encoding="UTF-8"?><oops>'
    response = '%x\r\n%s\r\n0\r\n\r\n' % (len(response), response)
    handler.add('GET', '/s3_fake_bucket/invalid_xml_error', 400,
                {'Content-type': 'application/xml',
                 'Transfer-Encoding': 'chunked',
                 'Connection': 'close'}, response)
    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            f = open_for_read(
                '/vsis3_streaming/s3_fake_bucket/invalid_xml_error'
            )
    assert f is None and gdal.VSIGetLastErrorMsg().find('<oops>') >= 0

    handler = webserver.SequentialHandler()
    response = '<?xml version="1.0" encoding="UTF-8"?><Error/>'
    response = '%x\r\n%s\r\n0\r\n\r\n' % (len(response), response)
    handler.add('GET', '/s3_fake_bucket/no_code_in_error', 400,
                {'Content-type': 'application/xml',
                 'Transfer-Encoding': 'chunked',
                 'Connection': 'close'}, response)
    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            f = open_for_read(
                '/vsis3_streaming/s3_fake_bucket/no_code_in_error'
            )
    assert f is None and gdal.VSIGetLastErrorMsg().find('<Error/>') >= 0

    handler = webserver.SequentialHandler()
    response = '''<?xml version="1.0" encoding="UTF-8"?>
    <Error>
    <Code>AuthorizationHeaderMalformed</Code>
    </Error>'''
    response = '%x\r\n%s\r\n0\r\n\r\n' % (len(response), response)
    handler.add(
        'GET',
        '/s3_fake_bucket/no_region_in_AuthorizationHeaderMalformed_error',
        400,
        {
            'Content-type': 'application/xml',
            'Transfer-Encoding': 'chunked',
            'Connection': 'close'
        },
        response
    )
    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            f = open_for_read(
                '/vsis3_streaming/s3_fake_bucket'
                '/no_region_in_AuthorizationHeaderMalformed_error'
            )
    assert f is None and gdal.VSIGetLastErrorMsg().find('<Error>') >= 0

    handler = webserver.SequentialHandler()
    response = '''<?xml version="1.0" encoding="UTF-8"?>
    <Error>
    <Code>PermanentRedirect</Code>
    </Error>'''
    response = '%x\r\n%s\r\n0\r\n\r\n' % (len(response), response)
    handler.add(
        'GET',
        '/s3_fake_bucket/no_endpoint_in_PermanentRedirect_error',
        400,
        {
            'Content-type': 'application/xml',
            'Transfer-Encoding': 'chunked',
            'Connection': 'close'
        },
        response
    )
    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            f = open_for_read(
                '/vsis3_streaming/s3_fake_bucket'
                '/no_endpoint_in_PermanentRedirect_error'
            )
    assert f is None and gdal.VSIGetLastErrorMsg().find('<Error>') >= 0

    handler = webserver.SequentialHandler()
    response = '''<?xml version="1.0" encoding="UTF-8"?>
    <Error>
    <Code>bla</Code>
    </Error>'''
    response = '%x\r\n%s\r\n0\r\n\r\n' % (len(response), response)
    handler.add('GET', '/s3_fake_bucket/no_message_in_error', 400,
                {'Content-type': 'application/xml',
                 'Transfer-Encoding': 'chunked',
                 'Connection': 'close'}, response)
    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            f = open_for_read(
                '/vsis3_streaming/s3_fake_bucket/no_message_in_error'
            )
    assert f is None and gdal.VSIGetLastErrorMsg().find('<Error>') >= 0

    # Test with requester pays
    handler = webserver.SequentialHandler()

    def method(request):
        if 'x-amz-request-payer' not in request.headers:
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)
            return
        expected_authorization_8080 = (
            'AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1/'
            's3/aws4_request,SignedHeaders=host;x-amz-content-sha256;'
            'x-amz-date;x-amz-request-payer,'
            'Signature='
            'cf713a394e1b629ac0e468d60d3d4a12f5236fd72d21b6005c758b0dfc7049cd'
        )
        expected_authorization_8081 = (
            'AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1/'
            's3/aws4_request,SignedHeaders=host;x-amz-content-sha256;'
            'x-amz-date;x-amz-request-payer,'
            'Signature='
            '4756166679008a1a40cd6ff91dbbef670a71c11bf8e3c998dd7385577c3ac4d9'
        )
        actual_authorization = request.headers['Authorization']
        if (actual_authorization not in
                (expected_authorization_8080, expected_authorization_8081)):
            sys.stderr.write(
                "Bad Authorization: '%s'\n" % str(actual_authorization)
            )
            request.send_response(403)
            return
        if request.headers['x-amz-request-payer'] != 'requester':
            sys.stderr.write(
                "Bad x-amz-request-payer: '%s'\n"
                % str(request.headers['x-amz-request-payer'])
            )
            request.send_response(403)
            return

        request.send_response(200)
        request.send_header('Content-type', 'text/plain')
        request.send_header('Content-Length', 3)
        request.send_header('Connection', 'close')
        request.end_headers()
        request.wfile.write("""foo""".encode('ascii'))

    handler.add(
        'GET',
        '/s3_fake_bucket_with_requester_pays/resource',
        custom_method=method
    )

    with gdaltest.config_option('AWS_REQUEST_PAYER', 'requester'):
        with webserver.install_http_handler(handler):
            with gdaltest.error_handler():
                f = open_for_read(
                    '/vsis3/s3_fake_bucket_with_requester_pays/resource'
                )
                assert f is not None
                data = gdal.VSIFReadL(1, 3, f).decode('ascii')
                gdal.VSIFCloseL(f)

    assert data == 'foo'

    # Test temporary redirect
    handler = webserver.SequentialHandler()

    class HandlerClass(object):
        def __init__(self, response_value):
            self.old_authorization = None
            self.response_value = response_value

        def method_req_1(self, request):
            if request.headers['Host'].find('127.0.0.1') < 0:
                sys.stderr.write('Bad headers: %s\n' % str(request.headers))
                request.send_response(403)
                return
            self.old_authorization = request.headers['Authorization']
            request.protocol_version = 'HTTP/1.1'
            request.send_response(307)
            response = '''<?xml version="1.0" encoding="UTF-8"?>
            <Error>
            <Message>bla</Message>
            <Code>TemporaryRedirect</Code>
            <Endpoint>localhost:%d</Endpoint>
            </Error>''' % request.server.port
            response = '%x\r\n%s\r\n0\r\n\r\n' % (len(response), response)
            request.send_header('Content-type', 'application/xml')
            request.send_header('Transfer-Encoding', 'chunked')
            request.end_headers()
            request.wfile.write(response.encode('ascii'))

        def method_req_2(self, request):
            if request.headers['Host'].find('localhost') < 0:
                sys.stderr.write('Bad headers: %s\n' % str(request.headers))
                request.send_response(403)
                return
            if self.old_authorization == request.headers['Authorization']:
                sys.stderr.write(
                    'Should have get a different Authorization. '
                    'Bad headers: %s\n' % str(request.headers)
                )
                request.send_response(403)
                return
            request.protocol_version = 'HTTP/1.1'
            request.send_response(200)
            response = self.response_value
            request.send_header('Content-Length', len(response))
            request.end_headers()
            request.wfile.write(response.encode('ascii'))

    h = HandlerClass('foo')
    handler.add(
        'GET',
        '/s3_test_temporary_redirect_read/resource',
        custom_method=h.method_req_1
    )
    handler.add(
        'GET',
        '/s3_test_temporary_redirect_read/resource',
        custom_method=h.method_req_2
    )

    with webserver.install_http_handler(handler):
        f = open_for_read('/vsis3/s3_test_temporary_redirect_read/resource')
        assert f is not None
        data = gdal.VSIFReadL(1, 3, f).decode('ascii')
        gdal.VSIFCloseL(f)

    assert data == 'foo'

    # Retry on the same bucket and check that the redirection was indeed
    # temporary
    handler = webserver.SequentialHandler()

    h = HandlerClass('bar')
    handler.add(
        'GET',
        '/s3_test_temporary_redirect_read/resource2',
        custom_method=h.method_req_1
    )
    handler.add(
        'GET',
        '/s3_test_temporary_redirect_read/resource2',
        custom_method=h.method_req_2
    )

    with webserver.install_http_handler(handler):
        f = open_for_read('/vsis3/s3_test_temporary_redirect_read/resource2')
        assert f is not None
        data = gdal.VSIFReadL(1, 3, f).decode('ascii')
        gdal.VSIFCloseL(f)

    assert data == 'bar'

###############################################################################
# Test re-opening after changing configuration option (#2294)


def test_vsis3_open_after_config_option_change(
        aws_test_config,
        webserver_port):
    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/test_vsis3_change_config_options/?delimiter=%2F', 403)
    handler.add('GET', '/test_vsis3_change_config_options/test.bin', 403)
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            f = open_for_read(
                '/vsis3/test_vsis3_change_config_options/test.bin'
            )
        assert f is None

    # Does not attempt any network access since we didn't change significant
    # parameters
    f = open_for_read('/vsis3/test_vsis3_change_config_options/test.bin')
    assert f is None

    with gdaltest.config_option('AWS_ACCESS_KEY_ID', 'another_key_id'):
        handler = webserver.SequentialHandler()
        handler.add(
            'GET',
            '/test_vsis3_change_config_options/?delimiter=%2F',
            200,
            {'Content-type': 'application/xml'},
            """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix></Prefix>
                <Contents>
                    <Key>test.bin</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>123456</Size>
                </Contents>
            </ListBucketResult>
            """
        )
        with webserver.install_http_handler(handler):
            f = open_for_read(
                '/vsis3/test_vsis3_change_config_options/test.bin'
            )
            assert f is not None
            gdal.VSIFCloseL(f)


###############################################################################
# Test ReadDir() with a fake AWS server


def test_vsis3_readdir(aws_test_config, webserver_port):
    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = 'HTTP/1.1'
        if request.headers['Authorization'].find('us-east-1') >= 0:
            request.send_response(400)
            response = '''<?xml version="1.0" encoding="UTF-8"?>
            <Error>
            <Message>bla</Message>
            <Code>AuthorizationHeaderMalformed</Code>
            <Region>us-west-2</Region>
            </Error>'''
            response = '%x\r\n%s\r\n0\r\n\r\n' % (len(response), response)
            request.send_header('Content-type', 'application/xml')
            request.send_header('Transfer-Encoding', 'chunked')
            request.end_headers()
            request.wfile.write(response.encode('ascii'))
        elif request.headers['Authorization'].find('us-west-2') >= 0:
            if request.headers['Host'].startswith('127.0.0.1'):
                request.send_response(301)
                response = '''<?xml version="1.0" encoding="UTF-8"?>
                <Error>
                <Message>bla</Message>
                <Code>PermanentRedirect</Code>
                <Endpoint>localhost:%d</Endpoint>
                </Error>''' % request.server.port
                response = '%x\r\n%s\r\n0\r\n\r\n' % (len(response), response)
                request.send_header('Content-type', 'application/xml')
                request.send_header('Transfer-Encoding', 'chunked')
                request.end_headers()
                request.wfile.write(response.encode('ascii'))
            elif request.headers['Host'].startswith('localhost'):
                request.send_response(200)
                request.send_header('Content-type', 'application/xml')
                response = """<?xml version="1.0" encoding="UTF-8"?>
                <ListBucketResult>
                    <Prefix>a_dir with_space/</Prefix>
                    <NextMarker>bla</NextMarker>
                    <Contents>
                        <Key>a_dir with_space/resource3 with_space.bin</Key>
                        <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                        <Size>123456</Size>
                    </Contents>
                </ListBucketResult>
                """
                request.send_header('Content-Length', len(response))
                request.end_headers()
                request.wfile.write(response.encode('ascii'))
            else:
                sys.stderr.write('Bad headers: %s\n' % str(request.headers))
                request.send_response(403)
        else:
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)

    handler.add(
        'GET',
        '/s3_fake_bucket2/?delimiter=%2F&prefix=a_dir%20with_space%2F',
        custom_method=method
    )
    handler.add(
        'GET',
        '/s3_fake_bucket2/?delimiter=%2F&prefix=a_dir%20with_space%2F',
        custom_method=method
    )
    handler.add(
        'GET',
        '/s3_fake_bucket2/?delimiter=%2F&prefix=a_dir%20with_space%2F',
        custom_method=method
    )

    def method(request):
        # /vsis3/ should have remembered the change of region and endpoint
        if request.headers['Authorization'].find('us-west-2') < 0 or \
                not request.headers['Host'].startswith('localhost'):
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)

        request.protocol_version = 'HTTP/1.1'
        request.send_response(200)
        request.send_header('Content-type', 'application/xml')
        response = """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>a_dir with_space/</Prefix>
                <Contents>
                    <Key>a_dir with_space/resource4.bin</Key>
                    <LastModified>2015-10-16T12:34:56.000Z</LastModified>
                    <Size>456789</Size>
                </Contents>
                <Contents>
                    <Key>a_dir with_space/i_am_a_glacier_file</Key>
                    <LastModified>2015-10-16T12:34:56.000Z</LastModified>
                    <Size>456789</Size>
                     <StorageClass>GLACIER</StorageClass>
                </Contents>
                <CommonPrefixes>
                    <Prefix>a_dir with_space/subdir/</Prefix>
                </CommonPrefixes>
            </ListBucketResult>
        """
        request.send_header('Content-Length', len(response))
        request.end_headers()
        request.wfile.write(response.encode('ascii'))

    handler.add(
        'GET',
        (
            '/s3_fake_bucket2/'
            '?delimiter=%2F&marker=bla&prefix=a_dir%20with_space%2F'
        ),
        custom_method=method
    )

    with webserver.install_http_handler(handler):
        f = open_for_read(
            '/vsis3/s3_fake_bucket2/a_dir with_space/resource3 with_space.bin'
        )
    if f is None:

        if gdaltest.is_travis_branch('trusty'):
            pytest.skip('Skipped on trusty branch, but should be investigated')

        pytest.fail()
    gdal.VSIFCloseL(f)

    with webserver.install_http_handler(webserver.SequentialHandler()):
        dir_contents = gdal.ReadDir('/vsis3/s3_fake_bucket2/a_dir with_space')
    assert dir_contents == [
        'resource3 with_space.bin',
        'resource4.bin',
        'subdir'
    ]

    assert gdal.VSIStatL(
        '/vsis3/s3_fake_bucket2/a_dir with_space/resource3 with_space.bin'
    ).size == 123456
    assert gdal.VSIStatL(
        '/vsis3/s3_fake_bucket2/a_dir with_space/resource3 with_space.bin'
    ).mtime == 1

    # Same as above: cached
    dir_contents = gdal.ReadDir('/vsis3/s3_fake_bucket2/a_dir with_space')
    assert dir_contents == [
        'resource3 with_space.bin',
        'resource4.bin',
        'subdir'
    ]

    # ReadDir on something known to be a file shouldn't cause network access
    dir_contents = gdal.ReadDir(
        '/vsis3/s3_fake_bucket2/a_dir with_space/resource3 with_space.bin'
    )
    assert dir_contents is None

    # Test unrelated partial clear of the cache
    gdal.VSICurlPartialClearCache('/vsis3/s3_fake_bucket_unrelated')

    assert gdal.VSIStatL(
        '/vsis3/s3_fake_bucket2/a_dir with_space/resource3 with_space.bin'
    ).size == 123456

    dir_contents = gdal.ReadDir('/vsis3/s3_fake_bucket2/a_dir with_space')
    assert dir_contents == [
        'resource3 with_space.bin',
        'resource4.bin',
        'subdir'
    ]

    # Test partial clear of the cache
    gdal.VSICurlPartialClearCache('/vsis3/s3_fake_bucket2/a_dir with_space')

    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/s3_fake_bucket2/a_dir%20with_space/resource3%20with_space.bin',
        400
    )
    handler.add(
        'GET',
        (
            '/s3_fake_bucket2/?delimiter=%2F&max-keys=100'
            '&prefix=a_dir%20with_space%2Fresource3%20with_space.bin%2F'
        ),
        400
    )
    with webserver.install_http_handler(handler):
        gdal.VSIStatL(
            '/vsis3/s3_fake_bucket2/a_dir with_space/resource3 with_space.bin'
        )

    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/s3_fake_bucket2/?delimiter=%2F&prefix=a_dir%2F',
        200,
        {'Content-type': 'application/xml'},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>a_dir/</Prefix>
                <Contents>
                    <Key>a_dir/test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>40</Size>
                </Contents>
            </ListBucketResult>
        """
    )
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir('/vsis3/s3_fake_bucket2/a_dir')
    assert dir_contents == ['test.txt']

    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/s3_fake_bucket2/?delimiter=%2F&prefix=a_dir%2F',
        200,
        {},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>a_dir/</Prefix>
                <Contents>
                    <Key>a_dir/resource4.bin</Key>
                    <LastModified>2015-10-16T12:34:56.000Z</LastModified>
                    <Size>456789</Size>
                </Contents>
                <Contents>
                    <Key>a_dir/i_am_a_glacier_file</Key>
                    <LastModified>2015-10-16T12:34:56.000Z</LastModified>
                    <Size>456789</Size>
                     <StorageClass>GLACIER</StorageClass>
                </Contents>
                <CommonPrefixes>
                    <Prefix>a_dir/subdir/</Prefix>
                </CommonPrefixes>
            </ListBucketResult>
        """
    )
    with gdaltest.config_option('CPL_VSIL_CURL_IGNORE_GLACIER_STORAGE', 'NO'):
        with webserver.install_http_handler(handler):
            dir_contents = gdal.ReadDir('/vsis3/s3_fake_bucket2/a_dir')
    assert dir_contents == ['resource4.bin', 'i_am_a_glacier_file', 'subdir']

    # Test CPL_VSIL_CURL_NON_CACHED
    for config_option_value in [
        '/vsis3/s3_non_cached/test.txt',
        '/vsis3/s3_non_cached',
        '/vsis3/s3_non_cached:/vsis3/unrelated',
        '/vsis3/unrelated:/vsis3/s3_non_cached',
        '/vsis3/unrelated:/vsis3/s3_non_cached:/vsis3/unrelated'
    ]:
        with gdaltest.config_option(
                'CPL_VSIL_CURL_NON_CACHED',
                config_option_value
        ):

            handler = webserver.SequentialHandler()
            handler.add('GET', '/s3_non_cached/test.txt', 200, {}, 'foo')

            with webserver.install_http_handler(handler):
                f = open_for_read('/vsis3/s3_non_cached/test.txt')
                assert f is not None, config_option_value
                data = gdal.VSIFReadL(1, 3, f).decode('ascii')
                gdal.VSIFCloseL(f)
                assert data == 'foo', config_option_value

            handler = webserver.SequentialHandler()
            handler.add('GET', '/s3_non_cached/test.txt', 200, {}, 'bar2')

            with webserver.install_http_handler(handler):
                size = gdal.VSIStatL('/vsis3/s3_non_cached/test.txt').size
            assert size == 4, config_option_value

            handler = webserver.SequentialHandler()
            handler.add('GET', '/s3_non_cached/test.txt', 200, {}, 'foo')

            with webserver.install_http_handler(handler):
                size = gdal.VSIStatL('/vsis3/s3_non_cached/test.txt').size
                if size != 3:
                    print(config_option_value)
                    pytest.fail(data)

            handler = webserver.SequentialHandler()
            handler.add('GET', '/s3_non_cached/test.txt', 200, {}, 'bar2')

            with webserver.install_http_handler(handler):
                f = open_for_read('/vsis3/s3_non_cached/test.txt')
                assert f is not None, config_option_value
                data = gdal.VSIFReadL(1, 4, f).decode('ascii')
                gdal.VSIFCloseL(f)
                assert data == 'bar2', config_option_value

    # Retry without option
    for config_option_value in [
            None,
            '/vsis3/s3_non_cached/bar.txt'
    ]:
        with gdaltest.config_option(
            'CPL_VSIL_CURL_NON_CACHED',
            config_option_value
        ):

            handler = webserver.SequentialHandler()
            if config_option_value is None:
                handler.add(
                    'GET',
                    '/s3_non_cached/?delimiter=%2F',
                    200,
                    {'Content-type': 'application/xml'},
                    """<?xml version="1.0" encoding="UTF-8"?>
                        <ListBucketResult>
                            <Prefix></Prefix>
                            <Contents>
                                <Key>test.txt</Key>
                                <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                                <Size>40</Size>
                            </Contents>
                            <Contents>
                                <Key>test2.txt</Key>
                                <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                                <Size>40</Size>
                            </Contents>
                        </ListBucketResult>
                    """
                )
                handler.add('GET', '/s3_non_cached/test.txt', 200, {}, 'foo')

            with webserver.install_http_handler(handler):
                f = open_for_read('/vsis3/s3_non_cached/test.txt')
                assert f is not None, config_option_value
                data = gdal.VSIFReadL(1, 3, f).decode('ascii')
                gdal.VSIFCloseL(f)
                assert data == 'foo', config_option_value

            handler = webserver.SequentialHandler()
            with webserver.install_http_handler(handler):
                f = open_for_read('/vsis3/s3_non_cached/test.txt')
                assert f is not None, config_option_value
                data = gdal.VSIFReadL(1, 4, f).decode('ascii')
                gdal.VSIFCloseL(f)
                # We should still get foo because of caching
                assert data == 'foo', config_option_value

    # List buckets (empty result)
    handler = webserver.SequentialHandler()
    handler.add('GET', '/', 200, {'Content-type': 'application/xml'},
                """<?xml version="1.0" encoding="UTF-8"?>
        <ListAllMyBucketsResult>
        <Buckets>
        </Buckets>
        </ListAllMyBucketsResult>
        """)
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir('/vsis3/')
    assert dir_contents == ['.']

    gdal.VSICurlClearCache()

    # List buckets
    handler = webserver.SequentialHandler()
    handler.add('GET', '/', 200, {'Content-type': 'application/xml'},
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
        dir_contents = gdal.ReadDir('/vsis3/')
    assert dir_contents == ['mybucket']

    # Test temporary redirect
    handler = webserver.SequentialHandler()

    class HandlerClass(object):
        def __init__(self, response_value):
            self.old_authorization = None
            self.response_value = response_value

        def method_req_1(self, request):
            if request.headers['Host'].find('127.0.0.1') < 0:
                sys.stderr.write('Bad headers: %s\n' % str(request.headers))
                request.send_response(403)
                return
            self.old_authorization = request.headers['Authorization']
            request.protocol_version = 'HTTP/1.1'
            request.send_response(307)
            response = '''<?xml version="1.0" encoding="UTF-8"?>
            <Error>
            <Message>bla</Message>
            <Code>TemporaryRedirect</Code>
            <Endpoint>localhost:%d</Endpoint>
            </Error>''' % request.server.port
            response = '%x\r\n%s\r\n0\r\n\r\n' % (len(response), response)
            request.send_header('Content-type', 'application/xml')
            request.send_header('Transfer-Encoding', 'chunked')
            request.end_headers()
            request.wfile.write(response.encode('ascii'))

        def method_req_2(self, request):
            if request.headers['Host'].find('localhost') < 0:
                sys.stderr.write('Bad headers: %s\n' % str(request.headers))
                request.send_response(403)
                return
            if self.old_authorization == request.headers['Authorization']:
                sys.stderr.write(
                    'Should have get a different Authorization. '
                    'Bad headers: %s\n' % str(request.headers)
                )
                request.send_response(403)
                return
            request.protocol_version = 'HTTP/1.1'
            request.send_response(200)
            request.send_header('Content-type', 'application/xml')
            response = self.response_value
            request.send_header('Content-Length', len(response))
            request.end_headers()
            request.wfile.write(response.encode('ascii'))

    h = HandlerClass("""<?xml version="1.0" encoding="UTF-8"?>
                <ListBucketResult>
                    <Prefix></Prefix>
                    <CommonPrefixes>
                        <Prefix>test</Prefix>
                    </CommonPrefixes>
                </ListBucketResult>
            """)
    handler.add(
        'GET',
        '/s3_test_temporary_redirect_read_dir/?delimiter=%2F',
        custom_method=h.method_req_1
    )
    handler.add(
        'GET',
        '/s3_test_temporary_redirect_read_dir/?delimiter=%2F',
        custom_method=h.method_req_2
    )

    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir(
            '/vsis3/s3_test_temporary_redirect_read_dir'
        )
    assert dir_contents == ['test']

    # Retry on the same bucket and check that the redirection was indeed
    # temporary
    handler = webserver.SequentialHandler()

    h = HandlerClass("""<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>test/</Prefix>
                <CommonPrefixes>
                    <Prefix>test/test2</Prefix>
                </CommonPrefixes>
            </ListBucketResult>
        """)
    handler.add(
        'GET',
        '/s3_test_temporary_redirect_read_dir/?delimiter=%2F&prefix=test%2F',
        custom_method=h.method_req_1
    )
    handler.add(
        'GET',
        '/s3_test_temporary_redirect_read_dir/?delimiter=%2F&prefix=test%2F',
        custom_method=h.method_req_2
    )

    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir(
            '/vsis3/s3_test_temporary_redirect_read_dir/test'
        )
    assert dir_contents == ['test2']

###############################################################################
# Test OpenDir() with a fake AWS server


def test_vsis3_opendir(aws_test_config, webserver_port):
    # Unlimited depth
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/vsis3_opendir/',
        200,
        {'Content-type': 'application/xml'},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix/>
                <Marker/>
                <Contents>
                    <Key>test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>40</Size>
                </Contents>
                <Contents>
                    <Key>subdir/</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>0</Size>
                </Contents>
                <Contents>
                    <Key>subdir/test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>5</Size>
                </Contents>
            </ListBucketResult>
        """
    )
    with webserver.install_http_handler(handler):
        d = gdal.OpenDir('/vsis3/vsis3_opendir')
    assert d is not None

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == 'test.txt'
    assert entry.size == 40
    assert entry.mode == 32768
    assert entry.mtime == 1

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == 'subdir'
    assert entry.mode == 16384

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == 'subdir/test.txt'

    entry = gdal.GetNextDirEntry(d)
    assert entry is None

    gdal.CloseDir(d)

    # Depth = 0
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/vsis3_opendir/?delimiter=%2F',
        200,
        {'Content-type': 'application/xml'},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix/>
                <Marker/>
                <Contents>
                    <Key>test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>40</Size>
                </Contents>
                <CommonPrefixes>
                    <Prefix>subdir/</Prefix>
                </CommonPrefixes>
            </ListBucketResult>
        """
    )
    with webserver.install_http_handler(handler):
        d = gdal.OpenDir('/vsis3/vsis3_opendir', 0)
    assert d is not None

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == 'test.txt'
    assert entry.size == 40
    assert entry.mode == 32768
    assert entry.mtime == 1

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == 'subdir'
    assert entry.mode == 16384

    entry = gdal.GetNextDirEntry(d)
    assert entry is None

    gdal.CloseDir(d)

    # Depth = 1
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/vsis3_opendir/?delimiter=%2F',
        200,
        {'Content-type': 'application/xml'},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix/>
                <Marker/>
                <Contents>
                    <Key>test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>40</Size>
                </Contents>
                <CommonPrefixes>
                    <Prefix>subdir/</Prefix>
                </CommonPrefixes>
            </ListBucketResult>
        """
    )
    with webserver.install_http_handler(handler):
        d = gdal.OpenDir('/vsis3/vsis3_opendir', 1)
        assert d is not None

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == 'test.txt'
    assert entry.size == 40
    assert entry.mode == 32768
    assert entry.mtime == 1

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == 'subdir'
    assert entry.mode == 16384

    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/vsis3_opendir/?delimiter=%2F&prefix=subdir%2F',
        200,
        {'Content-type': 'application/xml'},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>subdir/</Prefix>
                <Marker/>
                <Contents>
                    <Key>subdir/test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>5</Size>
                </Contents>
            </ListBucketResult>
        """
    )
    with webserver.install_http_handler(handler):
        entry = gdal.GetNextDirEntry(d)
        assert entry.name == 'subdir/test.txt'

    entry = gdal.GetNextDirEntry(d)
    assert entry is None

    gdal.CloseDir(d)

    # Prefix filtering on root of bucket
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/vsis3_opendir/?prefix=my_prefix',
        200,
        {'Content-type': 'application/xml'},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>my_prefix</Prefix>
                <Marker/>
                <Contents>
                    <Key>my_prefix_test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>40</Size>
                </Contents>
            </ListBucketResult>
        """
    )
    with webserver.install_http_handler(handler):
        d = gdal.OpenDir('/vsis3/vsis3_opendir', -1, ['PREFIX=my_prefix'])
        assert d is not None

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == 'my_prefix_test.txt'
    assert entry.size == 40
    assert entry.mode == 32768
    assert entry.mtime == 1

    entry = gdal.GetNextDirEntry(d)
    assert entry is None

    gdal.CloseDir(d)

    # Prefix filtering on subdir
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/vsis3_opendir/?prefix=some_dir%2Fmy_prefix',
        200,
        {'Content-type': 'application/xml'},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>some_dir/my_prefix</Prefix>
                <Marker/>
                <Contents>
                    <Key>some_dir/my_prefix_test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>40</Size>
                </Contents>
            </ListBucketResult>
        """
    )
    with webserver.install_http_handler(handler):
        d = gdal.OpenDir(
            '/vsis3/vsis3_opendir/some_dir',
            -1,
            ['PREFIX=my_prefix']
        )
        assert d is not None

    entry = gdal.GetNextDirEntry(d)
    assert entry.name == 'my_prefix_test.txt'
    assert entry.size == 40
    assert entry.mode == 32768
    assert entry.mtime == 1

    entry = gdal.GetNextDirEntry(d)
    assert entry is None

    gdal.CloseDir(d)

    # No network access done
    s = gdal.VSIStatL(
        '/vsis3/vsis3_opendir/some_dir/my_prefix_test.txt',
        (
            gdal.VSI_STAT_EXISTS_FLAG
            | gdal.VSI_STAT_NATURE_FLAG
            | gdal.VSI_STAT_SIZE_FLAG
            | gdal.VSI_STAT_CACHE_ONLY
        )
    )
    assert s
    assert (s.mode & 32768) != 0
    assert s.size == 40
    assert s.mtime == 1

    # No network access done
    assert gdal.VSIStatL(
        '/vsis3/vsis3_opendir/some_dir/i_do_not_exist.txt',
        (
            gdal.VSI_STAT_EXISTS_FLAG
            | gdal.VSI_STAT_NATURE_FLAG
            | gdal.VSI_STAT_SIZE_FLAG
            | gdal.VSI_STAT_CACHE_ONLY
        )
    ) is None

###############################################################################
# Test simple PUT support with a fake AWS server


def test_vsis3_4(aws_test_config, webserver_port):
    with webserver.install_http_handler(webserver.SequentialHandler()):
        with gdaltest.error_handler():
            f = gdal.VSIFOpenL('/vsis3/s3_fake_bucket3', 'wb')
    assert f is None

    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/s3_fake_bucket3/empty_file.bin',
        200,
        {'Connection': 'close'},
        'foo'
    )
    with webserver.install_http_handler(handler):
        assert gdal.VSIStatL('/vsis3/s3_fake_bucket3/empty_file.bin').size == 3

    # Empty file
    handler = webserver.SequentialHandler()

    def method(request):
        length_is_nonzero = request.headers['Content-Length'] != '0'
        type_not_foo = request.headers['Content-Type'] != 'foo'
        encoding_not_bar = request.headers['Content-Encoding'] != 'bar'
        if length_is_nonzero or type_not_foo or encoding_not_bar:
            sys.stderr.write(
                'Did not get expected headers: %s\n' % str(request.headers)
            )
            request.send_response(400)
            return

        request.send_response(200)
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add('PUT', '/s3_fake_bucket3/empty_file.bin', custom_method=method)

    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenExL(
            '/vsis3/s3_fake_bucket3/empty_file.bin',
            'wb',
            0,
            ['Content-Type=foo', 'Content-Encoding=bar']
        )
        assert f is not None
        gdal.ErrorReset()
        gdal.VSIFCloseL(f)
    assert gdal.GetLastErrorMsg() == ''

    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/s3_fake_bucket3/empty_file.bin',
        200,
        {'Connection': 'close'},
        ''
    )
    with webserver.install_http_handler(handler):
        assert gdal.VSIStatL('/vsis3/s3_fake_bucket3/empty_file.bin').size == 0

    # Invalid seek
    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL('/vsis3/s3_fake_bucket3/empty_file.bin', 'wb')
        assert f is not None
        with gdaltest.error_handler():
            ret = gdal.VSIFSeekL(f, 1, 0)
        assert ret != 0
        gdal.VSIFCloseL(f)

    # Invalid read
    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL('/vsis3/s3_fake_bucket3/empty_file.bin', 'wb')
        assert f is not None
        with gdaltest.error_handler():
            ret = gdal.VSIFReadL(1, 1, f)
        assert not ret
        gdal.VSIFCloseL(f)

    # Error case
    handler = webserver.SequentialHandler()
    handler.add('PUT', '/s3_fake_bucket3/empty_file_error.bin', 403)
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL('/vsis3/s3_fake_bucket3/empty_file_error.bin', 'wb')
        assert f is not None
        gdal.ErrorReset()
        with gdaltest.error_handler():
            gdal.VSIFCloseL(f)
    assert gdal.GetLastErrorMsg() != ''

    # Nominal case

    gdal.NetworkStatsReset()
    with gdaltest.config_option('CPL_VSIL_NETWORK_STATS_ENABLED', 'YES'):
        with webserver.install_http_handler(webserver.SequentialHandler()):
            f = gdal.VSIFOpenL('/vsis3/s3_fake_bucket3/another_file.bin', 'wb')
            assert f is not None
            assert gdal.VSIFSeekL(f, gdal.VSIFTellL(f), 0) == 0
            assert gdal.VSIFSeekL(f, 0, 1) == 0
            assert gdal.VSIFSeekL(f, 0, 2) == 0
            assert gdal.VSIFWriteL('foo', 1, 3, f) == 3
            assert gdal.VSIFSeekL(f, gdal.VSIFTellL(f), 0) == 0
            assert gdal.VSIFWriteL('bar', 1, 3, f) == 3

        handler = webserver.SequentialHandler()

        def method(request):
            if request.headers['Content-Length'] != '6':
                sys.stderr.write(
                    'Did not get expected headers: %s\n' % str(request.headers)
                )
                request.send_response(400)
                request.send_header('Content-Length', 0)
                request.end_headers()
                return

            request.wfile.write(
                'HTTP/1.1 100 Continue\r\n\r\n'.encode('ascii')
            )

            content = request.rfile.read(6).decode('ascii')
            if content != 'foobar':
                sys.stderr.write(
                    'Did not get expected content: %s\n' % content
                )
                request.send_response(400)
                request.send_header('Content-Length', 0)
                request.end_headers()
                return

            request.send_response(200)
            request.send_header('Content-Length', 0)
            request.end_headers()

        handler.add(
            'PUT',
            '/s3_fake_bucket3/another_file.bin',
            custom_method=method
        )

        gdal.ErrorReset()
        with webserver.install_http_handler(handler):
            gdal.VSIFCloseL(f)
        assert gdal.GetLastErrorMsg() == ''

    j = json.loads(gdal.NetworkStatsGetAsSerializedJSON())
    assert j == {
        "methods": {
            "PUT": {
                "count": 1,
                "uploaded_bytes": 6
            }
        },
        "handlers": {
            "vsis3": {
                "files": {
                    "/vsis3/s3_fake_bucket3/another_file.bin": {
                        "methods": {
                            "PUT": {
                                "count": 1,
                                "uploaded_bytes": 6
                            }
                        },
                        "actions": {
                            "Write": {
                                "methods": {
                                    "PUT": {
                                        "count": 1,
                                        "uploaded_bytes": 6
                                    }
                                }
                            }
                        }
                    }
                },
                "methods": {
                    "PUT": {
                        "count": 1,
                        "uploaded_bytes": 6
                    }
                }
            }
        }
    }

    gdal.NetworkStatsReset()

    # Redirect case
    with webserver.install_http_handler(webserver.SequentialHandler()):
        f = gdal.VSIFOpenL('/vsis3/s3_fake_bucket3/redirect.tif', 'wb')
        assert f is not None
        assert gdal.VSIFWriteL('foobar', 1, 6, f) == 6

    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = 'HTTP/1.1'
        if request.headers['Authorization'].find('us-east-1') >= 0:
            request.send_response(400)
            response = '''<?xml version="1.0" encoding="UTF-8"?>
            <Error>
            <Message>bla</Message>
            <Code>AuthorizationHeaderMalformed</Code>
            <Region>us-west-2</Region>
            </Error>'''
            response = '%x\r\n%s\r\n0\r\n\r\n' % (len(response), response)
            request.send_header('Content-type', 'application/xml')
            request.send_header('Transfer-Encoding', 'chunked')
            request.end_headers()
            request.wfile.write(response.encode('ascii'))
        elif request.headers['Authorization'].find('us-west-2') >= 0:
            if (request.headers['Content-Length'] != '6'
                    or request.headers['Content-Type'] != 'image/tiff'):
                sys.stderr.write(
                    'Did not get expected headers: %s\n' % str(request.headers)
                )
                request.send_response(400)
                request.send_header('Content-Length', 0)
                request.end_headers()
                return
            request.wfile.write(
                'HTTP/1.1 100 Continue\r\n\r\n'.encode('ascii')
            )
            content = request.rfile.read(6).decode('ascii')
            if content != 'foobar':
                sys.stderr.write(
                    'Did not get expected content: %s\n' % content
                )
                request.send_response(400)
                request.send_header('Content-Length', 0)
                request.end_headers()
                return
            request.send_response(200)
            request.send_header('Content-Length', 0)
            request.end_headers()
        else:
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)
            request.send_header('Content-Length', 0)
            request.end_headers()

    handler.add('PUT', '/s3_fake_bucket3/redirect.tif', custom_method=method)
    handler.add('PUT', '/s3_fake_bucket3/redirect.tif', custom_method=method)

    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        gdal.VSIFCloseL(f)
    assert gdal.GetLastErrorMsg() == ''

###############################################################################
# Test simple PUT support with retry logic


def test_vsis3_write_single_put_retry(aws_test_config, webserver_port):
    with gdaltest.config_options({'GDAL_HTTP_MAX_RETRY': '2',
                                  'GDAL_HTTP_RETRY_DELAY': '0.01'}):

        with webserver.install_http_handler(webserver.SequentialHandler()):
            f = gdal.VSIFOpenL(
                '/vsis3/s3_fake_bucket3/put_with_retry.bin',
                'wb'
            )
            assert f is not None
            assert gdal.VSIFWriteL('foo', 1, 3, f) == 3

        handler = webserver.SequentialHandler()

        def method(request):
            if request.headers['Content-Length'] != '3':
                sys.stderr.write(
                    'Did not get expected headers: %s\n' % str(request.headers)
                )
                request.send_response(400)
                request.send_header('Content-Length', 0)
                request.end_headers()
                return

            request.wfile.write(
                'HTTP/1.1 100 Continue\r\n\r\n'.encode('ascii')
            )

            content = request.rfile.read(3).decode('ascii')
            if content != 'foo':
                sys.stderr.write(
                    'Did not get expected content: %s\n' % content
                )
                request.send_response(400)
                request.send_header('Content-Length', 0)
                request.end_headers()
                return

            request.send_response(200)
            request.send_header('Content-Length', 0)
            request.end_headers()

        handler.add(
            'PUT',
            '/s3_fake_bucket3/put_with_retry.bin',
            502
        )
        handler.add(
            'PUT',
            '/s3_fake_bucket3/put_with_retry.bin',
            custom_method=method
        )

        with gdaltest.error_handler():
            with webserver.install_http_handler(handler):
                gdal.VSIFCloseL(f)


###############################################################################
# Test simple DELETE support with a fake AWS server


def test_vsis3_5(aws_test_config, webserver_port):
    with webserver.install_http_handler(webserver.SequentialHandler()):
        with gdaltest.error_handler():
            ret = gdal.Unlink('/vsis3/foo')
    assert ret != 0

    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/s3_delete_bucket/delete_file',
        200,
        {'Connection': 'close'},
        'foo'
    )
    with webserver.install_http_handler(handler):
        assert gdal.VSIStatL('/vsis3/s3_delete_bucket/delete_file').size == 3

    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        assert gdal.VSIStatL('/vsis3/s3_delete_bucket/delete_file').size == 3

    handler = webserver.SequentialHandler()
    handler.add('DELETE', '/s3_delete_bucket/delete_file', 204)
    with webserver.install_http_handler(handler):
        ret = gdal.Unlink('/vsis3/s3_delete_bucket/delete_file')
    assert ret == 0

    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/s3_delete_bucket/delete_file',
        404,
        {'Connection': 'close'}
    )
    handler.add(
        'GET',
        '/s3_delete_bucket/?delimiter=%2F&max-keys=100&prefix=delete_file%2F',
        404,
        {'Connection': 'close'}
    )
    with webserver.install_http_handler(handler):
        assert gdal.VSIStatL('/vsis3/s3_delete_bucket/delete_file') is None

    handler = webserver.SequentialHandler()
    handler.add('GET', '/s3_delete_bucket/delete_file_error', 200)
    handler.add('DELETE', '/s3_delete_bucket/delete_file_error', 403)
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ret = gdal.Unlink('/vsis3/s3_delete_bucket/delete_file_error')
    assert ret != 0

    handler = webserver.SequentialHandler()

    handler.add('GET', '/s3_delete_bucket/redirect', 200)

    def method(request):
        request.protocol_version = 'HTTP/1.1'
        if request.headers['Authorization'].find('us-east-1') >= 0:
            request.send_response(400)
            response = '''<?xml version="1.0" encoding="UTF-8"?>
            <Error>
            <Message>bla</Message>
            <Code>AuthorizationHeaderMalformed</Code>
            <Region>us-west-2</Region>
            </Error>'''
            response = '%x\r\n%s\r\n0\r\n\r\n' % (len(response), response)
            request.send_header('Content-type', 'application/xml')
            request.send_header('Transfer-Encoding', 'chunked')
            request.end_headers()
            request.wfile.write(response.encode('ascii'))
        elif request.headers['Authorization'].find('us-west-2') >= 0:
            request.send_response(204)
            request.send_header('Content-Length', 0)
            request.end_headers()
        else:
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)
            request.send_header('Content-Length', 0)
            request.end_headers()

    handler.add('DELETE', '/s3_delete_bucket/redirect', custom_method=method)
    handler.add('DELETE', '/s3_delete_bucket/redirect', custom_method=method)

    with webserver.install_http_handler(handler):
        ret = gdal.Unlink('/vsis3/s3_delete_bucket/redirect')
    assert ret == 0

###############################################################################
# Test DeleteObjects with a fake AWS server


def test_vsis3_unlink_batch(aws_test_config, webserver_port):
    def method(request):
        if request.headers['Content-MD5'] != 'Ze0X4LdlTwCsT+WpNxD9FA==':
            sys.stderr.write(
                'Did not get expected headers: %s\n' % str(request.headers)
            )
            request.send_response(403)
            return

        content = request.rfile.read(
            int(request.headers['Content-Length'])
        ).decode('ascii')
        if content != """<?xml version="1.0" encoding="UTF-8"?>
<Delete xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
  <Object>
    <Key>foo</Key>
  </Object>
  <Object>
    <Key>bar/baz</Key>
  </Object>
</Delete>
""":
            sys.stderr.write('Did not get expected content: %s\n' % content)
            request.send_response(403)
            return

        request.protocol_version = 'HTTP/1.1'
        request.send_response(200)
        response = """
        <DeleteResult xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
        <Deleted>
        <Key>foo</Key>
        </Deleted>
        <Deleted>
        <Key>bar/baz</Key>
        </Deleted>
        </DeleteResult>"""
        request.send_header('Content-Length', len(response))
        request.send_header('Connection', 'close')
        request.end_headers()
        request.wfile.write(response.encode('ascii'))

    handler = webserver.SequentialHandler()
    handler.add(
        'POST',
        '/unlink_batch/?delete',
        custom_method=method
    )
    handler.add(
        'POST',
        '/unlink_batch/?delete',
        200,
        {},
        """
        <DeleteResult xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
        <Deleted>
        <Key>baw</Key>
        </Deleted>
        </DeleteResult>"""
    )

    with gdaltest.config_option('CPL_VSIS3_UNLINK_BATCH_SIZE', '2'):
        with webserver.install_http_handler(handler):
            ret = gdal.UnlinkBatch([
                '/vsis3/unlink_batch/foo',
                '/vsis3/unlink_batch/bar/baz',
                '/vsis3/unlink_batch/baw'
            ])
    assert ret

    handler = webserver.SequentialHandler()
    handler.add(
        'POST',
        '/unlink_batch/?delete',
        200,
        {},
        """
        <DeleteResult xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
        <Failed>
        <Key>foo</Key>
        </Failed>
        </DeleteResult>"""
    )

    with webserver.install_http_handler(handler):
        ret = gdal.UnlinkBatch(['/vsis3/unlink_batch/foo'])
    assert not ret

###############################################################################
# Test RmdirRecursive() with a fake AWS server


def test_vsis3_rmdir_recursive(aws_test_config, webserver_port):
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/test_rmdir_recursive/?prefix=somedir%2F',
        200,
        {'Content-type': 'application/xml'},
        """<?xml version="1.0" encoding="UTF-8"?>
        <ListBucketResult>
            <Prefix>somedir/</Prefix>
            <Marker/>
            <Contents>
                <Key>somedir/test.txt</Key>
                <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                <Size>40</Size>
            </Contents>
            <Contents>
                <Key>somedir/subdir/</Key>
                <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                <Size>0</Size>
            </Contents>
            <Contents>
                <Key>somedir/subdir/test.txt</Key>
                <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                <Size>5</Size>
            </Contents>
        </ListBucketResult>
        """
    )

    def method(request):
        content = request.rfile.read(
            int(request.headers['Content-Length'])
        ).decode('ascii')
        if content != """<?xml version="1.0" encoding="UTF-8"?>
<Delete xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
  <Object>
    <Key>somedir/test.txt</Key>
  </Object>
  <Object>
    <Key>somedir/subdir/</Key>
  </Object>
</Delete>
""":
            sys.stderr.write('Did not get expected content: %s\n' % content)
            request.send_response(403)
            return

        request.protocol_version = 'HTTP/1.1'
        request.send_response(200)
        response = """
        <DeleteResult xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
        <Deleted>
        <Key>somedir/test.txt</Key>
        </Deleted>
        <Deleted>
        <Key>somedir/subdir/</Key>
        </Deleted>
        </DeleteResult>"""
        request.send_header('Content-Length', len(response))
        request.send_header('Connection', 'close')
        request.end_headers()
        request.wfile.write(response.encode('ascii'))

    handler.add('POST', '/test_rmdir_recursive/?delete', custom_method=method)

    def method(request):
        content = request.rfile.read(
            int(request.headers['Content-Length'])
        ).decode('ascii')
        if content != """<?xml version="1.0" encoding="UTF-8"?>
<Delete xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
  <Object>
    <Key>somedir/subdir/test.txt</Key>
  </Object>
  <Object>
    <Key>somedir/</Key>
  </Object>
</Delete>
""":
            sys.stderr.write('Did not get expected content: %s\n' % content)
            request.send_response(403)
            return

        request.protocol_version = 'HTTP/1.1'
        request.send_response(200)
        response = """
        <DeleteResult xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
        <Deleted>
        <Key>somedir/subdir/test.txt</Key>
        </Deleted>
        <Deleted>
        <Key>somedir/</Key>
        </Deleted>
        </DeleteResult>"""
        request.send_header('Content-Length', len(response))
        request.send_header('Connection', 'close')
        request.end_headers()
        request.wfile.write(response.encode('ascii'))

    handler.add('POST', '/test_rmdir_recursive/?delete', custom_method=method)

    with gdaltest.config_option('CPL_VSIS3_UNLINK_BATCH_SIZE', '2'):
        with webserver.install_http_handler(handler):
            assert gdal.RmdirRecursive(
                '/vsis3/test_rmdir_recursive/somedir'
            ) == 0


###############################################################################
# Test multipart upload with a fake AWS server


def test_vsis3_6(aws_test_config, webserver_port):
    with gdaltest.config_option('VSIS3_CHUNK_SIZE', '1'):  # 1 MB
        with webserver.install_http_handler(webserver.SequentialHandler()):
            f = gdal.VSIFOpenL('/vsis3/s3_fake_bucket4/large_file.tif', 'wb')
    assert f is not None
    size = 1024 * 1024 + 1
    big_buffer = 'a' * size

    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = 'HTTP/1.1'
        if request.headers['Authorization'].find('us-east-1') >= 0:
            request.send_response(400)
            response = '''<?xml version="1.0" encoding="UTF-8"?>
            <Error>
            <Message>bla</Message>
            <Code>AuthorizationHeaderMalformed</Code>
            <Region>us-west-2</Region>
            </Error>'''
            response = '%x\r\n%s\r\n0\r\n\r\n' % (len(response), response)
            request.send_header('Content-type', 'application/xml')
            request.send_header('Transfer-Encoding', 'chunked')
            request.end_headers()
            request.wfile.write(response.encode('ascii'))
        elif request.headers['Authorization'].find('us-west-2') >= 0:
            if request.headers['Content-Type'] != 'image/tiff':
                sys.stderr.write(
                    'Did not get expected headers: %s\n' % str(request.headers)
                )
                request.send_response(400)
                request.send_header('Content-Length', 0)
                request.end_headers()
                return
            response = '''<?xml version="1.0" encoding="UTF-8"?>
            <InitiateMultipartUploadResult>
            <UploadId>my_id</UploadId>
            </InitiateMultipartUploadResult>'''
            request.send_response(200)
            request.send_header('Content-type', 'application/xml')
            request.send_header('Content-Length', len(response))
            request.end_headers()
            request.wfile.write(response.encode('ascii'))
        else:
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)
            request.send_header('Content-Length', 0)
            request.end_headers()

    handler.add(
        'POST',
        '/s3_fake_bucket4/large_file.tif?uploads',
        custom_method=method
    )
    handler.add(
        'POST',
        '/s3_fake_bucket4/large_file.tif?uploads',
        custom_method=method
    )

    def method(request):
        if request.headers['Content-Length'] != '1048576':
            sys.stderr.write(
                'Did not get expected headers: %s\n' % str(request.headers)
            )
            request.send_response(400)
            request.send_header('Content-Length', 0)
            request.end_headers()
            return
        request.send_response(200)
        request.send_header('ETag', '"first_etag"')
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add(
        'PUT',
        '/s3_fake_bucket4/large_file.tif?partNumber=1&uploadId=my_id',
        custom_method=method
    )

    with webserver.install_http_handler(handler):
        ret = gdal.VSIFWriteL(big_buffer, 1, size, f)
    assert ret == size
    handler = webserver.SequentialHandler()

    def method(request):
        if request.headers['Content-Length'] != '1':
            sys.stderr.write(
                'Did not get expected headers: %s\n' % str(request.headers)
            )
            request.send_response(400)
            return
        request.send_response(200)
        request.send_header('ETag', '"second_etag"')
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add(
        'PUT',
        '/s3_fake_bucket4/large_file.tif?partNumber=2&uploadId=my_id',
        custom_method=method
    )

    def method(request):

        if request.headers['Content-Length'] != '186':
            sys.stderr.write(
                'Did not get expected headers: %s\n' % str(request.headers)
            )
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

    handler.add(
        'POST',
        '/s3_fake_bucket4/large_file.tif?uploadId=my_id',
        custom_method=method
    )

    gdal.ErrorReset()
    with webserver.install_http_handler(handler):
        gdal.VSIFCloseL(f)
    assert gdal.GetLastErrorMsg() == ''

    handler = webserver.SequentialHandler()
    handler.add(
        'POST',
        '/s3_fake_bucket4/large_file_initiate_403_error.bin?uploads',
        403
    )
    handler.add(
        'POST',
        '/s3_fake_bucket4/large_file_initiate_empty_result.bin?uploads',
        200
    )
    handler.add(
        'POST',
        '/s3_fake_bucket4/large_file_initiate_invalid_xml_result.bin?uploads',
        200,
        {},
        'foo'
    )
    handler.add(
        'POST',
        '/s3_fake_bucket4/large_file_initiate_no_uploadId.bin?uploads',
        200,
        {},
        '<foo/>'
    )
    filenames = [
        '/vsis3/s3_fake_bucket4/large_file_initiate_403_error.bin',
        '/vsis3/s3_fake_bucket4/large_file_initiate_empty_result.bin',
        '/vsis3/s3_fake_bucket4/large_file_initiate_invalid_xml_result.bin',
        '/vsis3/s3_fake_bucket4/large_file_initiate_no_uploadId.bin'
    ]
    with webserver.install_http_handler(handler):
        for filename in filenames:
            with gdaltest.config_option('VSIS3_CHUNK_SIZE', '1'):  # 1 MB
                f = gdal.VSIFOpenL(filename, 'wb')
            assert f is not None
            with gdaltest.error_handler():
                ret = gdal.VSIFWriteL(big_buffer, 1, size, f)
            assert ret == 0
            gdal.ErrorReset()
            gdal.VSIFCloseL(f)
            assert gdal.GetLastErrorMsg() == ''

    handler = webserver.SequentialHandler()
    handler.add(
        'POST',
        '/s3_fake_bucket4/large_file_upload_part_403_error.bin?uploads',
        200,
        {},
        '''xml version="1.0" encoding="UTF-8"?>
        <InitiateMultipartUploadResult>
        <UploadId>my_id</UploadId>
        </InitiateMultipartUploadResult>'''
    )
    handler.add(
        'PUT',
        (
            '/s3_fake_bucket4/large_file_upload_part_403_error.bin'
            '?partNumber=1&uploadId=my_id'
        ),
        403
    )
    handler.add(
        'DELETE',
        (
            '/s3_fake_bucket4/large_file_upload_part_403_error.bin'
            '?uploadId=my_id'
        ),
        204
    )

    handler.add(
        'POST',
        '/s3_fake_bucket4/large_file_upload_part_no_etag.bin?uploads',
        200,
        {},
        '''<?xml version="1.0" encoding="UTF-8"?>
        <InitiateMultipartUploadResult>
        <UploadId>my_id</UploadId>
        </InitiateMultipartUploadResult>'''
    )
    handler.add(
        'PUT',
        (
            '/s3_fake_bucket4/large_file_upload_part_no_etag.bin'
            '?partNumber=1&uploadId=my_id'
        ),
        200
    )
    handler.add(
        'DELETE',
        '/s3_fake_bucket4/large_file_upload_part_no_etag.bin?uploadId=my_id',
        204
    )

    filenames = [
        '/vsis3/s3_fake_bucket4/large_file_upload_part_403_error.bin',
        '/vsis3/s3_fake_bucket4/large_file_upload_part_no_etag.bin'
    ]
    with webserver.install_http_handler(handler):
        for filename in filenames:
            with gdaltest.config_option('VSIS3_CHUNK_SIZE', '1'):  # 1 MB
                f = gdal.VSIFOpenL(filename, 'wb')
            assert f is not None, filename
            with gdaltest.error_handler():
                ret = gdal.VSIFWriteL(big_buffer, 1, size, f)
            assert ret == 0, filename
            gdal.ErrorReset()
            gdal.VSIFCloseL(f)
            assert gdal.GetLastErrorMsg() == '', filename

    # Simulate failure in AbortMultipart stage
    handler = webserver.SequentialHandler()
    handler.add(
        'POST',
        '/s3_fake_bucket4/large_file_abortmultipart_403_error.bin?uploads',
        200,
        {},
        '''<?xml version="1.0" encoding="UTF-8"?>
        <InitiateMultipartUploadResult>
        <UploadId>my_id</UploadId>
        </InitiateMultipartUploadResult>'''
    )
    handler.add(
        'PUT',
        (
            '/s3_fake_bucket4/large_file_abortmultipart_403_error.bin'
            '?partNumber=1&uploadId=my_id'
        ),
        403
    )
    handler.add(
        'DELETE',
        (
            '/s3_fake_bucket4/large_file_abortmultipart_403_error.bin'
            '?uploadId=my_id'
        ),
        403
    )

    filename = '/vsis3/s3_fake_bucket4/large_file_abortmultipart_403_error.bin'
    with webserver.install_http_handler(handler):
        with gdaltest.config_option('VSIS3_CHUNK_SIZE', '1'):  # 1 MB
            f = gdal.VSIFOpenL(filename, 'wb')
        assert f is not None, filename
        with gdaltest.error_handler():
            ret = gdal.VSIFWriteL(big_buffer, 1, size, f)
        assert ret == 0, filename
        gdal.ErrorReset()
        with gdaltest.error_handler():
            gdal.VSIFCloseL(f)
        assert gdal.GetLastErrorMsg() != '', filename

    # Simulate failure in CompleteMultipartUpload stage
    handler = webserver.SequentialHandler()
    handler.add(
        'POST',
        '/s3_fake_bucket4/large_file_completemultipart_403_error.bin?uploads',
        200,
        {},
        '''<?xml version="1.0" encoding="UTF-8"?>
        <InitiateMultipartUploadResult>
        <UploadId>my_id</UploadId>
        </InitiateMultipartUploadResult>'''
    )
    handler.add(
        'PUT',
        (
            '/s3_fake_bucket4/large_file_completemultipart_403_error.bin'
            '?partNumber=1&uploadId=my_id'
        ),
        200,
        {'ETag': 'first_etag'},
        ''
    )
    handler.add(
        'PUT',
        (
            '/s3_fake_bucket4/large_file_completemultipart_403_error.bin'
            '?partNumber=2&uploadId=my_id'
        ),
        200,
        {'ETag': 'second_etag'},
        ''
    )
    handler.add(
        'POST',
        (
            '/s3_fake_bucket4/large_file_completemultipart_403_error.bin'
            '?uploadId=my_id'
        ),
        403
    )

    filename = (
        '/vsis3/s3_fake_bucket4/large_file_completemultipart_403_error.bin'
    )
    with webserver.install_http_handler(handler):
        with gdaltest.config_option('VSIS3_CHUNK_SIZE', '1'):  # 1 MB
            f = gdal.VSIFOpenL(filename, 'wb')
            assert f is not None, filename
            ret = gdal.VSIFWriteL(big_buffer, 1, size, f)
            assert ret == size, filename
            gdal.ErrorReset()
            with gdaltest.error_handler():
                gdal.VSIFCloseL(f)
            assert gdal.GetLastErrorMsg() != '', filename


###############################################################################
# Test multipart upload with retry logic


def test_vsis3_write_multipart_retry(aws_test_config, webserver_port):

    with gdaltest.config_options({'GDAL_HTTP_MAX_RETRY': '2',
                                  'GDAL_HTTP_RETRY_DELAY': '0.01'}):

        with gdaltest.config_option('VSIS3_CHUNK_SIZE', '1'):  # 1 MB
            with webserver.install_http_handler(webserver.SequentialHandler()):
                f = gdal.VSIFOpenL(
                    '/vsis3/s3_fake_bucket4/large_file.tif',
                    'wb'
                )
        assert f is not None
        size = 1024 * 1024 + 1
        big_buffer = 'a' * size

        handler = webserver.SequentialHandler()

        response = '''<?xml version="1.0" encoding="UTF-8"?>
        <InitiateMultipartUploadResult>
        <UploadId>my_id</UploadId>
        </InitiateMultipartUploadResult>'''
        handler.add(
            'POST',
            '/s3_fake_bucket4/large_file.tif?uploads',
            502
        )
        handler.add(
            'POST',
            '/s3_fake_bucket4/large_file.tif?uploads',
            200,
            {
                'Content-type': 'application/xml',
                'Content-Length': len(response),
                'Connection': 'close'
            },
            response
        )

        handler.add(
            'PUT',
            '/s3_fake_bucket4/large_file.tif?partNumber=1&uploadId=my_id',
            502
        )
        handler.add(
            'PUT',
            '/s3_fake_bucket4/large_file.tif?partNumber=1&uploadId=my_id',
            200,
            {
                'Content-Length': '0',
                'ETag': '"first_etag"',
                'Connection': 'close'
            },
            {}
        )

        with gdaltest.error_handler():
            with webserver.install_http_handler(handler):
                ret = gdal.VSIFWriteL(big_buffer, 1, size, f)
        assert ret == size
        handler = webserver.SequentialHandler()

        handler.add(
            'PUT',
            '/s3_fake_bucket4/large_file.tif?partNumber=2&uploadId=my_id',
            200,
            {
                'Content-Length': '0',
                'ETag': '"second_etag"',
                'Connection': 'close'
            },
            {}
        )

        handler.add(
            'POST',
            '/s3_fake_bucket4/large_file.tif?uploadId=my_id',
            502
        )
        handler.add(
            'POST',
            '/s3_fake_bucket4/large_file.tif?uploadId=my_id',
            200,
            {
                'Content-Length': '0',
                'Connection': 'close'
            },
            {}
        )

        with gdaltest.error_handler():
            with webserver.install_http_handler(handler):
                gdal.VSIFCloseL(f)


###############################################################################
# Test abort pending multipart uploads


def test_vsis3_abort_pending_uploads(aws_test_config, webserver_port):

    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/my_bucket/?max-uploads=1&uploads',
        200,
        {},
        """<?xml version="1.0"?>
        <ListMultipartUploadsResult>
            <NextKeyMarker>next_key_marker</NextKeyMarker>
            <NextUploadIdMarker>next_upload_id_marker</NextUploadIdMarker>
            <IsTruncated>true</IsTruncated>
            <Upload>
                <Key>my_key</Key>
                <UploadId>my_upload_id</UploadId>
            </Upload>
        </ListMultipartUploadsResult>
        """
    )
    handler.add(
        'GET',
        (
            '/my_bucket/?key-marker=next_key_marker&max-uploads=1'
            '&upload-id-marker=next_upload_id_marker&uploads'
        ),
        200,
        {},
        """<?xml version="1.0"?>
        <ListMultipartUploadsResult>
            <IsTruncated>false</IsTruncated>
            <Upload>
                <Key>my_key2</Key>
                <UploadId>my_upload_id2</UploadId>
            </Upload>
        </ListMultipartUploadsResult>
        """
    )
    handler.add(
        'DELETE',
        "/my_bucket/my_key?uploadId=my_upload_id",
        204
    )
    handler.add(
        'DELETE',
        "/my_bucket/my_key2?uploadId=my_upload_id2",
        204
    )
    with webserver.install_http_handler(handler):
        with gdaltest.config_option('CPL_VSIS3_LIST_UPLOADS_MAX', '1'):
            assert gdal.AbortPendingUploads('/vsis3/my_bucket')


###############################################################################
# Test Mkdir() / Rmdir()


def test_vsis3_7(aws_test_config, webserver_port):

    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/s3_bucket_test_mkdir/dir/',
        404,
        {'Connection': 'close'}
    )
    handler.add(
        'GET',
        '/s3_bucket_test_mkdir/?delimiter=%2F&max-keys=100&prefix=dir%2F',
        404,
        {'Connection': 'close'}
    )
    handler.add(
        'PUT',
        '/s3_bucket_test_mkdir/dir/',
        200
    )
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir('/vsis3/s3_bucket_test_mkdir/dir', 0)
    assert ret == 0

    assert stat.S_ISDIR(gdal.VSIStatL('/vsis3/s3_bucket_test_mkdir/dir').mode)

    dir_content = gdal.ReadDir('/vsis3/s3_bucket_test_mkdir/dir')
    assert dir_content == ['.']

    # Try creating already existing directory
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/s3_bucket_test_mkdir/dir/',
        416,
        {'Connection': 'close'}
    )
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir('/vsis3/s3_bucket_test_mkdir/dir', 0)
    assert ret != 0

    handler = webserver.SequentialHandler()
    handler.add(
        'DELETE',
        '/s3_bucket_test_mkdir/dir/',
        204
    )
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir('/vsis3/s3_bucket_test_mkdir/dir')
    assert ret == 0

    # Try deleting already deleted directory
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/s3_bucket_test_mkdir/dir/',
        404
    )
    handler.add(
        'GET',
        '/s3_bucket_test_mkdir/?delimiter=%2F&max-keys=100&prefix=dir%2F',
        404,
        {'Connection': 'close'}
    )
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir('/vsis3/s3_bucket_test_mkdir/dir')
    assert ret != 0

    # Try deleting non-empty directory
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/s3_bucket_test_mkdir/dir_nonempty/',
        416
    )
    handler.add(
        'GET',
        (
            '/s3_bucket_test_mkdir/?delimiter=%2F&max-keys=100'
            '&prefix=dir_nonempty%2F'
        ),
        200,
        {'Content-type': 'application/xml'},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>dir_nonempty/</Prefix>
                <Contents>
                    <Key>dir_nonempty/test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>40</Size>
                </Contents>
            </ListBucketResult>
        """
    )
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir('/vsis3/s3_bucket_test_mkdir/dir_nonempty')
    assert ret != 0

    # Try stat'ing a directory not ending with slash
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/s3_bucket_test_dir_stat/test_dir_stat',
        400
    )
    handler.add(
        'GET',
        (
            '/s3_bucket_test_dir_stat/?delimiter=%2F&max-keys=100'
            '&prefix=test_dir_stat%2F'
        ),
        200,
        {'Content-type': 'application/xml'},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>test_dir_stat/</Prefix>
                <Contents>
                    <Key>test_dir_stat/test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>40</Size>
                </Contents>
            </ListBucketResult>
        """
    )
    with webserver.install_http_handler(handler):
        assert stat.S_ISDIR(
            gdal.VSIStatL('/vsis3/s3_bucket_test_dir_stat/test_dir_stat').mode
        )

    # Try ReadDi'ing a directory not ending with slash
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/s3_bucket_test_readdir/?delimiter=%2F&prefix=test_dirread%2F',
        200,
        {'Content-type': 'application/xml'},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>test_dirread/</Prefix>
                <Contents>
                    <Key>test_dirread/test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>40</Size>
                </Contents>
            </ListBucketResult>
        """
    )
    with webserver.install_http_handler(handler):
        assert gdal.ReadDir(
            '/vsis3/s3_bucket_test_readdir/test_dirread'
        ) is not None

    # Try stat'ing a directory ending with slash
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/s3_bucket_test_dir_stat_2/test_dir_stat/',
        400
    )
    handler.add(
        'GET',
        (
            '/s3_bucket_test_dir_stat_2/?delimiter=%2F&max-keys=100'
            '&prefix=test_dir_stat%2F'
        ),
        200,
        {'Content-type': 'application/xml'},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>test_dir_stat/</Prefix>
                <Contents>
                    <Key>test_dir_stat/test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>40</Size>
                </Contents>
            </ListBucketResult>
        """
    )
    with webserver.install_http_handler(handler):
        assert stat.S_ISDIR(
            gdal.VSIStatL(
                '/vsis3/s3_bucket_test_dir_stat_2/test_dir_stat/'
            ).mode
        )

    # Try ReadDi'ing a directory ending with slash
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/s3_bucket_test_readdir2/?delimiter=%2F&prefix=test_dirread%2F',
        200,
        {'Content-type': 'application/xml'},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>test_dirread/</Prefix>
                <Contents>
                    <Key>test_dirread/test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>40</Size>
                </Contents>
            </ListBucketResult>
        """
    )
    with webserver.install_http_handler(handler):
        assert gdal.ReadDir(
            '/vsis3/s3_bucket_test_readdir2/test_dirread'
        ) is not None


###############################################################################
# Test handling of file and directory with same name


def test_vsis3_8(aws_test_config, webserver_port):

    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/vsis3_8/?delimiter=%2F',
        200,
        {'Content-type': 'application/xml'},
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
        """
    )

    with webserver.install_http_handler(handler):
        listdir = gdal.ReadDir('/vsis3/vsis3_8', 0)
    assert listdir == ['test', 'test/']

    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        assert not stat.S_ISDIR(gdal.VSIStatL('/vsis3/vsis3_8/test').mode)

    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        assert stat.S_ISDIR(gdal.VSIStatL('/vsis3/vsis3_8/test/').mode)


###############################################################################
# Test vsisync() with SYNC_STRATEGY=ETAG


def test_vsis3_sync_etag(aws_test_config, webserver_port):

    gdal.VSICurlClearCache()

    options = ['SYNC_STRATEGY=ETAG']

    with gdaltest.error_handler():
        handler = webserver.SequentialHandler()
        with webserver.install_http_handler(handler):
            assert not gdal.Sync('/i_do/not/exist', '/vsis3/', options=options)

    with gdaltest.error_handler():
        handler = webserver.SequentialHandler()
        handler.add(
            'GET',
            '/do_not/exist',
            404
        )
        handler.add(
            'GET',
            '/do_not/?delimiter=%2F&max-keys=100&prefix=exist%2F',
            404
        )
        handler.add(
            'PUT',
            '/do_not/exist',
            404
        )
        with webserver.install_http_handler(handler):
            assert not gdal.Sync(
                'vsifile.py',
                '/vsis3/do_not/exist',
                options=options
            )

    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/out/',
        200
    )
    handler.add(
        'GET',
        '/out/testsync.txt',
        404
    )
    handler.add(
        'GET',
        '/out/?delimiter=%2F&max-keys=100&prefix=testsync.txt%2F',
        404
    )

    def method(request):
        if request.headers['Content-Length'] != '3':
            sys.stderr.write(
                'Did not get expected headers: %s\n' % str(request.headers)
            )
            request.send_response(400)
            request.send_header('Content-Length', 0)
            request.end_headers()
            return

        request.wfile.write('HTTP/1.1 100 Continue\r\n\r\n'.encode('ascii'))

        content = request.rfile.read(3).decode('ascii')
        if content != 'foo':
            sys.stderr.write('Did not get expected content: %s\n' % content)
            request.send_response(400)
            request.send_header('Content-Length', 0)
            request.end_headers()
            return

        request.send_response(200)
        request.send_header('Content-Length', 0)
        request.send_header('ETag', '"acbd18db4cc2f85cedef654fccc4a4d8"')
        request.end_headers()

    handler.add('PUT', '/out/testsync.txt', custom_method=method)

    gdal.FileFromMemBuffer('/vsimem/testsync.txt', 'foo')

    def cbk(pct, _, tab):
        assert pct > tab[0]
        tab[0] = pct
        return True

    tab = [0]
    with webserver.install_http_handler(handler):
        assert gdal.Sync('/vsimem/testsync.txt', '/vsis3/out', options=options,
                         callback=cbk, callback_data=tab)
    assert tab[0] == 1.0

    # Re-try with cached ETag. Should generate no network access
    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        assert gdal.Sync(
            '/vsimem/testsync.txt',
            '/vsis3/out',
            options=options
        )
        assert gdal.Sync(
            '/vsimem/testsync.txt',
            '/vsis3/out/testsync.txt',
            options=options
        )

    gdal.VSICurlClearCache()

    # Other direction: S3 to /vsimem
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/out/testsync.txt',
        206,
        {
            'Content-Length': '3',
            'Content-Range': 'bytes 0-2/3',
            'ETag': '"acbd18db4cc2f85cedef654fccc4a4d8"'
        },
        "foo"
    )
    with webserver.install_http_handler(handler):
        assert gdal.Sync(
            '/vsis3/out/testsync.txt',
            '/vsimem/',
            options=options
        )

    # Shouldn't do any copy, but hard to verify
    with webserver.install_http_handler(webserver.SequentialHandler()):
        assert gdal.Sync(
            '/vsis3/out/testsync.txt',
            '/vsimem/',
            options=options
        )
        assert gdal.Sync(
            '/vsis3/out/testsync.txt',
            '/vsimem/testsync.txt',
            options=options
        )

    # Modify target file, and redo synchronization
    gdal.FileFromMemBuffer('/vsimem/testsync.txt', 'bar')

    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/out/testsync.txt',
        200,
        {
            'Content-Length': '3',
            'ETag': '"acbd18db4cc2f85cedef654fccc4a4d8"'
        },
        "foo"
    )
    with webserver.install_http_handler(handler):
        assert gdal.Sync(
            '/vsis3/out/testsync.txt',
            '/vsimem/',
            options=options
        )

    f = gdal.VSIFOpenL('/vsimem/testsync.txt', 'rb')
    data = gdal.VSIFReadL(1, 3, f).decode('ascii')
    gdal.VSIFCloseL(f)
    assert data == 'foo'

    # /vsimem to S3, but after cleaning the cache
    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/out/',
        200
    )
    handler.add(
        'GET',
        '/out/testsync.txt',
        206,
        {
            'Content-Length': '3',
            'Content-Range': 'bytes 0-2/3',
            'ETag': '"acbd18db4cc2f85cedef654fccc4a4d8"'
        },
        "foo"
    )
    with webserver.install_http_handler(handler):
        assert gdal.Sync('/vsimem/testsync.txt', '/vsis3/out', options=options)

    gdal.Unlink('/vsimem/testsync.txt')

    # Directory copying
    gdal.VSICurlClearCache()

    gdal.Mkdir('/vsimem/subdir', 0)
    gdal.FileFromMemBuffer('/vsimem/subdir/testsync.txt', 'foo')
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/out/',
        200,
        {},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix/>
                <Marker/>
                <IsTruncated>false</IsTruncated>
                <Contents>
                    <Key>testsync.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>3</Size>
                    <ETag>"acbd18db4cc2f85cedef654fccc4a4d8"</ETag>
                </Contents>
            </ListBucketResult>
        """
    )
    with webserver.install_http_handler(handler):
        assert gdal.Sync('/vsimem/subdir/', '/vsis3/out', options=options)
    gdal.RmdirRecursive('/vsimem/subdir')

###############################################################################
# Test vsisync() with SYNC_STRATEGY=TIMESTAMP


def test_vsis3_sync_timestamp(aws_test_config, webserver_port):

    options = ['SYNC_STRATEGY=TIMESTAMP']

    gdal.FileFromMemBuffer('/vsimem/testsync.txt', 'foo')

    # S3 to local: S3 file is older -> download
    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/out/testsync.txt',
        206,
        {
            'Content-Length': '3',
            'Content-Range': 'bytes 0-2/3',
            'Last-Modified': 'Mon, 01 Jan 1970 00:00:01 GMT'
        },
        "foo"
    )
    handler.add(
        'GET',
        '/out/testsync.txt',
        200,
        {
            'Content-Length': '3',
            'Last-Modified': 'Mon, 01 Jan 1970 00:00:01 GMT'
        },
        "foo"
    )
    with webserver.install_http_handler(handler):
        assert gdal.Sync(
            '/vsis3/out/testsync.txt',
            '/vsimem/',
            options=options
        )

    # S3 to local: S3 file is newer -> do nothing
    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/out/testsync.txt',
        206,
        {
            'Content-Length': '3',
            'Content-Range': 'bytes 0-2/3',
            # TODO: Will this test fail post-2037?
            'Last-Modified': 'Mon, 01 Jan 2037 00:00:01 GMT'
        },
        "foo"
    )
    with webserver.install_http_handler(handler):
        assert gdal.Sync(
            '/vsis3/out/testsync.txt',
            '/vsimem/',
            options=options
        )

    # Local to S3: S3 file is older -> upload
    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/out/testsync.txt',
        206,
        {
            'Content-Length': '3',
            'Content-Range': 'bytes 0-2/3',
            'Last-Modified': 'Mon, 01 Jan 1970 00:00:01 GMT'
        },
        "foo"
    )
    handler.add('PUT', '/out/testsync.txt', 200)
    with webserver.install_http_handler(handler):
        assert gdal.Sync(
            '/vsimem/testsync.txt',
            '/vsis3/out/testsync.txt',
            options=options
        )

    # Local to S3: S3 file is newer -> do nothing
    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/out/testsync.txt',
        206,
        {
            'Content-Length': '3',
            'Content-Range': 'bytes 0-2/3',
            'Last-Modified': 'Mon, 01 Jan 2037 00:00:01 GMT'
        },
        "foo"
    )
    with webserver.install_http_handler(handler):
        assert gdal.Sync(
            '/vsimem/testsync.txt',
            '/vsis3/out/testsync.txt',
            options=options
        )

    gdal.Unlink('/vsimem/testsync.txt')

###############################################################################
# Test vsisync() with SYNC_STRATEGY=OVERWRITE


def test_vsis3_sync_overwrite(aws_test_config, webserver_port):

    options = ['SYNC_STRATEGY=OVERWRITE']

    gdal.FileFromMemBuffer('/vsimem/testsync.txt', 'foo')

    # S3 to local: S3 file is newer
    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/out/testsync.txt',
        206,
        {
            'Content-Length': '3',
            'Content-Range': 'bytes 0-2/3',
            'Last-Modified': 'Mon, 01 Jan 2037 00:00:01 GMT'
        },
        "foo"
    )
    handler.add(
        'GET',
        '/out/testsync.txt',
        200,
        {
            'Content-Length': '3',
            'Last-Modified': 'Mon, 01 Jan 2037 00:00:01 GMT'
        },
        "foo"
    )
    with webserver.install_http_handler(handler):
        assert gdal.Sync(
            '/vsis3/out/testsync.txt',
            '/vsimem/',
            options=options
        )

    # Local to S3: S3 file is newer
    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/out/testsync.txt',
        206,
        {
            'Content-Length': '3',
            'Content-Range': 'bytes 0-2/3',
            'Last-Modified': 'Mon, 01 Jan 2037 00:00:01 GMT'
        },
        "foo"
    )
    handler.add('PUT', '/out/testsync.txt', 200)
    with webserver.install_http_handler(handler):
        assert gdal.Sync(
            '/vsimem/testsync.txt',
            '/vsis3/out/testsync.txt',
            options=options
        )

    gdal.Unlink('/vsimem/testsync.txt')

###############################################################################
# Test vsisync() with source and target in /vsis3


def test_vsis3_sync_source_target_in_vsis3(
        aws_test_config,
        webserver_port
):

    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/in/testsync.txt',
        200,
        {
            'Content-Length': '3',
            'Content-Range': 'bytes 0-2/3',
            'Last-Modified': 'Mon, 01 Jan 1970 00:00:01 GMT'
        },
        "foo"
    )
    handler.add(
        'GET',
        '/out/',
        200
    )
    handler.add(
        'GET',
        '/out/testsync.txt',
        200,
        {
            'Content-Length': '3',
            'Last-Modified':
            'Mon, 01 Jan 1970 00:00:01 GMT'
        },
        "foo"
    )

    def method(request):
        if request.headers['Content-Length'] != '0':
            sys.stderr.write(
                'Did not get expected headers: %s\n' % str(request.headers)
            )
            request.send_response(400)
            return
        if request.headers['x-amz-copy-source'] != '/in/testsync.txt':
            sys.stderr.write(
                'Did not get expected headers: %s\n' % str(request.headers)
            )
            request.send_response(400)
            return

        request.send_response(200)
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add('PUT', '/out/testsync.txt', custom_method=method)

    with webserver.install_http_handler(handler):
        assert gdal.Sync('/vsis3/in/testsync.txt', '/vsis3/out/')

###############################################################################
# Test rename


def test_vsis3_fake_rename(aws_test_config, webserver_port):

    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/test/source.txt',
        206,
        {
            'Content-Length': '3',
            'Content-Range': 'bytes 0-2/3'
        },
        "foo"
    )
    handler.add(
        'GET',
        '/test/target.txt',
        404
    )
    handler.add(
        'GET',
        '/test/?delimiter=%2F&max-keys=100&prefix=target.txt%2F',
        200
    )

    def method(request):
        if request.headers['Content-Length'] != '0':
            sys.stderr.write(
                'Did not get expected headers: %s\n' % str(request.headers)
            )
            request.send_response(400)
            return
        if request.headers['x-amz-copy-source'] != '/test/source.txt':
            sys.stderr.write(
                'Did not get expected headers: %s\n' % str(request.headers)
            )
            request.send_response(400)
            return

        request.send_response(200)
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add('PUT', '/test/target.txt', custom_method=method)
    handler.add('DELETE', '/test/source.txt', 204)

    with webserver.install_http_handler(handler):
        assert gdal.Rename(
            '/vsis3/test/source.txt',
            '/vsis3/test/target.txt'
        ) == 0

###############################################################################
# Test rename


def test_vsis3_fake_rename_dir(aws_test_config, webserver_port):

    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/test/source_dir',
        404
    )
    handler.add(
        'GET',
        '/test/?delimiter=%2F&max-keys=100&prefix=source_dir%2F',
        200,
        {'Content-type': 'application/xml'},
        """<?xml version="1.0" encoding="UTF-8"?>
            <ListBucketResult>
                <Prefix>source_dir/</Prefix>
                <Contents>
                    <Key>source_dir/test.txt</Key>
                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                    <Size>3</Size>
                </Contents>
            </ListBucketResult>
        """
    )
    handler.add(
        'GET',
        '/test/target_dir/',
        404
    )
    handler.add(
        'GET',
        '/test/?delimiter=%2F&max-keys=100&prefix=target_dir%2F',
        404
    )

    def method(request):
        if request.headers['Content-Length'] != '0':
            sys.stderr.write(
                'Did not get expected headers: %s\n' % str(request.headers)
            )
            request.send_response(400)
            request.send_header('Content-Length', 0)
            request.end_headers()
            return

        request.wfile.write('HTTP/1.1 100 Continue\r\n\r\n'.encode('ascii'))
        request.send_response(200)
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add('PUT', '/test/target_dir/', custom_method=method)

    def method(request):
        if request.headers['Content-Length'] != '0':
            sys.stderr.write(
                'Did not get expected headers: %s\n' % str(request.headers)
            )
            request.send_response(400)
            return
        if request.headers['x-amz-copy-source'] != '/test/source_dir/test.txt':
            sys.stderr.write(
                'Did not get expected headers: %s\n' % str(request.headers)
            )
            request.send_response(400)
            return

        request.send_response(200)
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add(
        'PUT',
        '/test/target_dir/test.txt',
        custom_method=method
    )

    handler.add(
        'DELETE',
        '/test/source_dir/test.txt',
        204
    )

    handler.add(
        'GET',
        '/test/source_dir/',
        404
    )
    handler.add(
        'GET',
        '/test/?delimiter=%2F&max-keys=100&prefix=source_dir%2F',
        404
    )

    with webserver.install_http_handler(handler):
        assert gdal.Rename(
            '/vsis3/test/source_dir',
            '/vsis3/test/target_dir'
        ) == 0

###############################################################################
# Test rename onto existing dir is not allowed


def test_vsis3_fake_rename_on_existing_dir(
        aws_test_config,
        webserver_port
):

    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/test/source.txt',
        206,
        {
            'Content-Length': '3',
            'Content-Range': 'bytes 0-2/3'
        },
        "foo"
    )
    handler.add(
        'GET',
        '/test_target_dir/',
        200
    )

    with webserver.install_http_handler(handler):
        assert gdal.Rename(
            '/vsis3/test/source.txt',
            '/vsis3/test_target_dir'
        ) == -1


###############################################################################
# Test Sync() and multithreaded download and CHUNK_SIZE


def test_vsis3_fake_sync_multithreaded_upload_chunk_size(
        aws_test_config,
        webserver_port
):

    gdal.VSICurlClearCache()

    def cbk(pct, _, tab):
        assert pct >= tab[0]
        tab[0] = pct
        return True

    gdal.Mkdir('/vsimem/test', 0)
    gdal.FileFromMemBuffer('/vsimem/test/foo', 'foo\n')

    tab = [-1]
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/test_bucket/?prefix=test%2F',
        200
    )
    handler.add(
        'GET',
        '/test_bucket/test',
        404
    )
    handler.add(
        'GET',
        '/test_bucket/?delimiter=%2F&max-keys=100&prefix=test%2F',
        200
    )
    handler.add(
        'GET',
        '/test_bucket/',
        200
    )
    handler.add(
        'GET',
        '/test_bucket/test/',
        404
    )
    handler.add(
        'PUT',
        '/test_bucket/test/',
        200
    )

    def method(request):
        request.protocol_version = 'HTTP/1.1'
        response = '''<?xml version="1.0" encoding="UTF-8"?>
        <InitiateMultipartUploadResult>
        <UploadId>my_id</UploadId>
        </InitiateMultipartUploadResult>'''
        request.send_response(200)
        request.send_header('Content-type', 'application/xml')
        request.send_header('Content-Length', len(response))
        request.end_headers()
        request.wfile.write(response.encode('ascii'))

    handler.add('POST', '/test_bucket/test/foo?uploads', custom_method=method)

    def method(request):
        if request.headers['Content-Length'] != '3':
            sys.stderr.write(
                'Did not get expected headers: %s\n' % str(request.headers)
            )
            request.send_response(400)
            request.send_header('Content-Length', 0)
            request.end_headers()
            return
        request.send_response(200)
        request.send_header('ETag', '"first_etag"')
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add(
        'PUT',
        '/test_bucket/test/foo?partNumber=1&uploadId=my_id',
        custom_method=method
    )

    def method(request):
        if request.headers['Content-Length'] != '1':
            sys.stderr.write(
                'Did not get expected headers: %s\n' % str(request.headers)
            )
            request.send_response(400)
            request.send_header('Content-Length', 0)
            request.end_headers()
            return
        request.send_response(200)
        request.send_header('ETag', '"second_etag"')
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add(
        'PUT',
        '/test_bucket/test/foo?partNumber=2&uploadId=my_id',
        custom_method=method
    )

    def method(request):
        if request.headers['Content-Length'] != '186':
            sys.stderr.write(
                'Did not get expected headers: %s\n' % str(request.headers)
            )
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

    handler.add(
        'POST',
        '/test_bucket/test/foo?uploadId=my_id',
        custom_method=method
    )

    with gdaltest.config_option('VSIS3_SIMULATE_THREADING', 'YES'):
        with webserver.install_http_handler(handler):
            assert gdal.Sync('/vsimem/test',
                             '/vsis3/test_bucket',
                             options=['NUM_THREADS=1', 'CHUNK_SIZE=3'],
                             callback=cbk, callback_data=tab)
    assert tab[0] == 1.0

    gdal.RmdirRecursive('/vsimem/test')


def test_vsis3_fake_sync_multithreaded_upload_chunk_size_failure(
        aws_test_config,
        webserver_port
):

    gdal.VSICurlClearCache()

    gdal.Mkdir('/vsimem/test', 0)
    gdal.FileFromMemBuffer('/vsimem/test/foo', 'foo\n')

    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/test_bucket/?prefix=test%2F',
        200
    )
    handler.add(
        'GET',
        '/test_bucket/test',
        404
    )
    handler.add(
        'GET',
        '/test_bucket/?delimiter=%2F&max-keys=100&prefix=test%2F',
        200
    )
    handler.add(
        'GET',
        '/test_bucket/',
        200
    )
    handler.add(
        'GET',
        '/test_bucket/test/',
        404
    )
    handler.add(
        'PUT',
        '/test_bucket/test/',
        200
    )
    handler.add(
        'POST',
        '/test_bucket/test/foo?uploads',
        200,
        {'Content-type': 'application:/xml'},
        b'''<?xml version="1.0" encoding="UTF-8"?>
        <InitiateMultipartUploadResult>
        <UploadId>my_id</UploadId>
        </InitiateMultipartUploadResult>'''
    )
    handler.add(
        'PUT',
        '/test_bucket/test/foo?partNumber=1&uploadId=my_id',
        200,
        {'ETag': '"first_etag"'},
        expected_headers={'Content-Length': '3'}
    )
    handler.add(
        'DELETE',
        '/test_bucket/test/foo?uploadId=my_id',
        204
    )

    with gdaltest.config_options({'VSIS3_SIMULATE_THREADING': 'YES',
                                  'VSIS3_SYNC_MULTITHREADING': 'NO'}):
        with webserver.install_http_handler(handler):
            with gdaltest.error_handler():
                assert not gdal.Sync('/vsimem/test',
                                     '/vsis3/test_bucket',
                                     options=['NUM_THREADS=1', 'CHUNK_SIZE=3'])

    gdal.RmdirRecursive('/vsimem/test')

###############################################################################
# Test reading/writing metadata


def test_vsis3_metadata(aws_test_config, webserver_port):

    gdal.VSICurlClearCache()

    # Read HEADERS domain
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/test_metadata/foo.txt',
        200,
        {'foo': 'bar'}
    )
    with webserver.install_http_handler(handler):
        md = gdal.GetFileMetadata('/vsis3/test_metadata/foo.txt', 'HEADERS')
    assert 'foo' in md and md['foo'] == 'bar'

    # Read TAGS domain
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/test_metadata/foo.txt?tagging',
        200,
        {},
        """<Tagging>
        <TagSet>
        <Tag>
        <Key>foo</Key>
        <Value>bar</Value>
        </Tag>
        </TagSet>
        </Tagging>"""
    )
    with webserver.install_http_handler(handler):
        md = gdal.GetFileMetadata('/vsis3/test_metadata/foo.txt', 'TAGS')
    assert 'foo' in md and md['foo'] == 'bar'

    # Write HEADERS domain
    handler = webserver.SequentialHandler()
    handler.add(
        'PUT',
        '/test_metadata/foo.txt',
        200,
        {},
        expected_headers={
            'foo': 'bar',
            'x-amz-metadata-directive': 'REPLACE',
            'x-amz-copy-source': '/test_metadata/foo.txt'
        }
    )
    with webserver.install_http_handler(handler):
        assert gdal.SetFileMetadata(
            '/vsis3/test_metadata/foo.txt',
            {'foo': 'bar'},
            'HEADERS'
        )

    # Write TAGS domain
    handler = webserver.SequentialHandler()
    handler.add(
        'PUT',
        '/test_metadata/foo.txt?tagging',
        200,
        expected_body=b"""<?xml version="1.0" encoding="UTF-8"?>
<Tagging xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
  <TagSet>
    <Tag>
      <Key>foo</Key>
      <Value>bar</Value>
    </Tag>
  </TagSet>
</Tagging>
""")
    with webserver.install_http_handler(handler):
        assert gdal.SetFileMetadata(
            '/vsis3/test_metadata/foo.txt',
            {'foo': 'bar'},
            'TAGS'
        )

    # Write TAGS domain (wiping tags)
    handler = webserver.SequentialHandler()
    handler.add(
        'DELETE',
        '/test_metadata/foo.txt?tagging',
        204
    )
    with webserver.install_http_handler(handler):
        assert gdal.SetFileMetadata(
            '/vsis3/test_metadata/foo.txt',
            {},
            'TAGS'
        )

    # Error case
    with gdaltest.error_handler():
        assert gdal.GetFileMetadata(
            '/vsis3/test_metadata/foo.txt',
            'UNSUPPORTED'
        ) == {}

    # Error case
    with gdaltest.error_handler():
        assert not gdal.SetFileMetadata(
            '/vsis3/test_metadata/foo.txt',
            {},
            'UNSUPPORTED'
        )

###############################################################################
# Test that we take into account directory listing to avoid useless
# requests


def test_vsis3_no_useless_requests(aws_test_config, webserver_port):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/no_useless_requests/?delimiter=%2F',
        200,
        {'Content-type': 'application/xml'},
        """<?xml version="1.0" encoding="UTF-8"?>
        <ListBucketResult>
            <Prefix></Prefix>
            <Contents>
            </Contents>
        </ListBucketResult>
        """
    )
    with webserver.install_http_handler(handler):
        assert gdal.VSIFOpenL(
            '/vsis3/no_useless_requests/foo.txt', 'rb'
        ) is None
        assert gdal.VSIFOpenL(
            '/vsis3/no_useless_requests/bar.txt', 'rb'
        ) is None
        assert gdal.VSIStatL(
            '/vsis3/no_useless_requests/baz.txt'
        ) is None

###############################################################################
# Test w+ access


def test_vsis3_random_write(aws_test_config, webserver_port):

    gdal.VSICurlClearCache()

    with gdaltest.error_handler():
        assert gdal.VSIFOpenL('/vsis3/random_write/test.bin', 'w+b') is None

    with gdaltest.config_option(
            'CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE',
            'YES'
    ):
        f = gdal.VSIFOpenL('/vsis3/random_write/test.bin', 'w+b')
    assert f
    assert gdal.VSIFWriteL('foo', 3, 1, f) == 1
    assert gdal.VSIFSeekL(f, 0, 0) == 0
    assert gdal.VSIFReadL(3, 1, f).decode('ascii') == 'foo'
    assert gdal.VSIFEofL(f) == 0
    assert gdal.VSIFTellL(f) == 3

    handler = webserver.SequentialHandler()
    handler.add('PUT', '/random_write/test.bin', 200, {}, expected_body=b'foo')
    with webserver.install_http_handler(handler):
        assert gdal.VSIFCloseL(f) == 0

###############################################################################
# Test w+ access


def test_vsis3_random_write_failure_1(aws_test_config, webserver_port):

    gdal.VSICurlClearCache()

    with gdaltest.config_option(
            'CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE',
            'YES'
    ):
        f = gdal.VSIFOpenL('/vsis3/random_write/test.bin', 'w+b')
    assert f

    handler = webserver.SequentialHandler()
    handler.add('PUT', '/random_write/test.bin', 400, {})
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            assert gdal.VSIFCloseL(f) != 0


###############################################################################
# Test w+ access


def test_vsis3_random_write_failure_2(aws_test_config, webserver_port):

    gdal.VSICurlClearCache()

    with gdaltest.config_option(
            'CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE',
            'YES'
    ):
        with gdaltest.config_option('VSIS3_CHUNK_SIZE_BYTES', '1'):
            f = gdal.VSIFOpenL('/vsis3/random_write/test.bin', 'w+b')
    assert f
    assert gdal.VSIFWriteL('foo', 3, 1, f) == 1

    handler = webserver.SequentialHandler()
    handler.add('POST', '/random_write/test.bin?uploads', 400, {})
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            assert gdal.VSIFCloseL(f) != 0

###############################################################################
# Test w+ access


def test_vsis3_random_write_gtiff_create_copy(
        aws_test_config,
        webserver_port
):

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/random_write/test.tif',
        404,
        {}
    )
    handler.add(
        'GET',
        '/random_write/?delimiter=%2F&max-keys=100&prefix=test.tif%2F',
        404,
        {}
    )
    handler.add(
        'GET',
        '/random_write/?delimiter=%2F',
        404,
        {}
    )

    src_ds = gdal.Open('data/byte.tif')

    with gdaltest.config_option(
            'CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE',
            'YES'
    ):
        with webserver.install_http_handler(handler):
            ds = gdal.GetDriverByName('GTiff').CreateCopy(
                '/vsis3/random_write/test.tif',
                src_ds
            )
    assert ds is not None

    handler = webserver.SequentialHandler()
    handler.add('PUT', '/random_write/test.tif', 200, {})
    with webserver.install_http_handler(handler):
        ds = None

###############################################################################
# Read credentials from simulated ~/.aws/credentials


def test_vsis3_read_credentials_file(aws_test_config, webserver_port):

    options = {
        'AWS_SECRET_ACCESS_KEY': '',
        'AWS_ACCESS_KEY_ID': '',
        'CPL_AWS_CREDENTIALS_FILE': '/vsimem/aws_credentials'
    }

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

    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/s3_fake_bucket/resource',
        custom_method=get_s3_fake_bucket_resource_method
    )
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options):
            f = open_for_read('/vsis3/s3_fake_bucket/resource')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    assert data == 'foo'

    gdal.Unlink('/vsimem/aws_credentials')

###############################################################################
# Read credentials from simulated  ~/.aws/config


def test_vsis3_read_config_file(aws_test_config, webserver_port):
    options = {
        'AWS_SECRET_ACCESS_KEY': '',
        'AWS_ACCESS_KEY_ID': '',
        'AWS_CONFIG_FILE': '/vsimem/aws_config'
    }

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

    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/s3_fake_bucket/resource',
        custom_method=get_s3_fake_bucket_resource_method
    )
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options):
            f = open_for_read('/vsis3/s3_fake_bucket/resource')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    assert data == 'foo'

    gdal.Unlink('/vsimem/aws_config')

###############################################################################
# Read credentials from simulated ~/.aws/credentials and ~/.aws/config


def test_vsis3_read_credentials_config_file(
        aws_test_config,
        webserver_port
):
    options = {
        'AWS_SECRET_ACCESS_KEY': '',
        'AWS_ACCESS_KEY_ID': '',
        'CPL_AWS_CREDENTIALS_FILE': '/vsimem/aws_credentials',
        'AWS_CONFIG_FILE': '/vsimem/aws_config'
    }

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

    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/s3_fake_bucket/resource',
        custom_method=get_s3_fake_bucket_resource_method
    )
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options):
            f = open_for_read('/vsis3/s3_fake_bucket/resource')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    assert data == 'foo'

    gdal.Unlink('/vsimem/aws_credentials')
    gdal.Unlink('/vsimem/aws_config')

###############################################################################
# Read credentials from simulated ~/.aws/credentials and ~/.aws/config with
# a non default profile


def test_vsis3_read_credentials_config_file_non_default_profile(
        aws_test_config,
        webserver_port,
        tmpdir
):
    options = {
        'AWS_SECRET_ACCESS_KEY': '',
        'AWS_ACCESS_KEY_ID': '',
        'CPL_AWS_CREDENTIALS_FILE': None,
        'AWS_CONFIG_FILE': None,
        'AWS_PROFILE': 'myprofile',
        # The test fails if AWS_DEFAULT_PROFILE is set externally to the test
        # runner unless it is set to 'myprofile' here.
        'AWS_DEFAULT_PROFILE': 'myprofile'
    }

    os_aws = tmpdir.mkdir(".aws")

    gdal.VSICurlClearCache()

    os_aws.join('credentials').write("""
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

    os_aws.join('config').write("""
[unrelated]
aws_access_key_id = foo
aws_secret_access_key = bar
[profile myprofile]
region = us-east-1
[default]
aws_access_key_id = foo
aws_secret_access_key = bar
""")

    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/s3_fake_bucket/resource',
        custom_method=get_s3_fake_bucket_resource_method
    )
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options):
            with gdaltest.config_option(
                'USERPROFILE' if sys.platform == 'win32' else 'HOME',
                str(tmpdir)
            ):
                f = open_for_read('/vsis3/s3_fake_bucket/resource')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    assert data == 'foo'

###############################################################################
# Read credentials from simulated ~/.aws/credentials and ~/.aws/config


def test_vsis3_read_credentials_config_file_inconsistent(
        aws_test_config,
        webserver_port
):

    options = {
        'AWS_SECRET_ACCESS_KEY': '',
        'AWS_ACCESS_KEY_ID': '',
        'CPL_AWS_CREDENTIALS_FILE': '/vsimem/aws_credentials',
        'AWS_CONFIG_FILE': '/vsimem/aws_config'
    }

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
    handler = webserver.SequentialHandler()
    handler.add(
        'GET',
        '/s3_fake_bucket/resource',
        custom_method=get_s3_fake_bucket_resource_method
    )
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options):
            with gdaltest.error_handler():
                f = open_for_read('/vsis3/s3_fake_bucket/resource')
        assert f is not None
        assert gdal.GetLastErrorMsg() != ''
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    assert data == 'foo'

    gdal.Unlink('/vsimem/aws_credentials')
    gdal.Unlink('/vsimem/aws_config')


###############################################################################
# Read credentials from simulated EC2 instance
@pytest.mark.skipif(
    sys.platform not in ('linux', 'win32'),
    reason='Incorrect platform'
)
def test_vsis3_read_credentials_ec2_imdsv2(
        aws_test_config,
        webserver_port
):
    options = {
        'CPL_AWS_CREDENTIALS_FILE': '',
        'AWS_CONFIG_FILE': '',
        'AWS_SECRET_ACCESS_KEY': '',
        'AWS_ACCESS_KEY_ID': '',
        # Disable hypervisor related check to test if we are really on EC2
        'CPL_AWS_AUTODETECT_EC2': 'NO'
    }

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        'PUT',
        '/latest/api/token',
        200,
        {},
        'mytoken',
        expected_headers={'X-aws-ec2-metadata-token-ttl-seconds': '10'}
    )
    handler.add(
        'GET',
        '/latest/meta-data/iam/security-credentials/',
        200,
        {},
        'myprofile',
        expected_headers={'X-aws-ec2-metadata-token': 'mytoken'}
    )
    handler.add(
        'GET',
        '/latest/meta-data/iam/security-credentials/myprofile',
        200,
        {},
        """{
        "AccessKeyId": "AWS_ACCESS_KEY_ID",
        "SecretAccessKey": "AWS_SECRET_ACCESS_KEY",
        "Expiration": "3000-01-01T00:00:00Z"
        }""",
        expected_headers={'X-aws-ec2-metadata-token': 'mytoken'}
    )

    handler.add(
        'GET',
        '/s3_fake_bucket/resource',
        custom_method=get_s3_fake_bucket_resource_method
    )
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options):
            with gdaltest.config_option(
                    'CPL_AWS_EC2_API_ROOT_URL',
                    'http://localhost:%d' % webserver_port
            ):
                f = open_for_read('/vsis3/s3_fake_bucket/resource')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    assert data == 'foo'

    handler = webserver.SequentialHandler()
    handler.add('GET', '/s3_fake_bucket/bar', 200, {}, 'bar')
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options):
            # Set a fake URL to check that credentials re-use works
            with gdaltest.config_option('CPL_AWS_EC2_API_ROOT_URL', ''):
                f = open_for_read('/vsis3/s3_fake_bucket/bar')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    assert data == 'bar'


###############################################################################
# Read credentials from simulated EC2 instance that only supports IMDSv1
@pytest.mark.skipif(
    sys.platform not in ('linux', 'win32'),
    reason='Incorrect platform'
)
def test_vsis3_read_credentials_ec2_imdsv1(
        aws_test_config,
        webserver_port
):
    options = {
        'CPL_AWS_CREDENTIALS_FILE': '',
        'AWS_CONFIG_FILE': '',
        'AWS_SECRET_ACCESS_KEY': '',
        'AWS_ACCESS_KEY_ID': '',
        'CPL_AWS_EC2_API_ROOT_URL': 'http://localhost:%d' % webserver_port,
        # Disable hypervisor related check to test if we are really on EC2
        'CPL_AWS_AUTODETECT_EC2': 'NO'
    }

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        'PUT',
        '/latest/api/token',
        403,
        {},
        expected_headers={'X-aws-ec2-metadata-token-ttl-seconds': '10'}
    )
    handler.add(
        'GET',
        '/latest/meta-data/iam/security-credentials/',
        200,
        {},
        'myprofile',
        unexpected_headers=['X-aws-ec2-metadata-token']
    )
    handler.add(
        'GET',
        '/latest/meta-data/iam/security-credentials/myprofile',
        200,
        {},
        """{
        "AccessKeyId": "AWS_ACCESS_KEY_ID",
        "SecretAccessKey": "AWS_SECRET_ACCESS_KEY",
        "Expiration": "3000-01-01T00:00:00Z"
        }""",
        unexpected_headers=['X-aws-ec2-metadata-token']
    )

    handler.add(
        'GET',
        '/s3_fake_bucket/resource',
        custom_method=get_s3_fake_bucket_resource_method
    )
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options):
            f = open_for_read('/vsis3/s3_fake_bucket/resource')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    assert data == 'foo'


###############################################################################
# Read credentials from simulated EC2 instance with expiration of the
# cached credentials
@pytest.mark.skipif(
    sys.platform not in ('linux', 'win32'),
    reason='Incorrect platform'
)
def test_vsis3_read_credentials_ec2_expiration(
        aws_test_config,
        webserver_port
):

    options = {
        'CPL_AWS_CREDENTIALS_FILE': '',
        'AWS_CONFIG_FILE': '',
        'AWS_SECRET_ACCESS_KEY': '',
        'AWS_ACCESS_KEY_ID': '',
        # Disable hypervisor related check to test if we are really on EC2
        'CPL_AWS_AUTODETECT_EC2': 'NO'
    }

    valid_url = 'http://localhost:%d' % webserver_port
    invalid_url = 'http://localhost:%d/invalid' % webserver_port

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        'PUT',
        '/latest/api/token',
        200,
        {},
        'mytoken',
        expected_headers={'X-aws-ec2-metadata-token-ttl-seconds': '10'}
    )
    handler.add(
        'GET',
        '/latest/meta-data/iam/security-credentials/',
        200,
        {},
        'myprofile',
        expected_headers={'X-aws-ec2-metadata-token': 'mytoken'}
    )
    handler.add(
        'GET',
        '/latest/meta-data/iam/security-credentials/myprofile',
        200,
        {},
        """{
        "AccessKeyId": "AWS_ACCESS_KEY_ID",
        "SecretAccessKey": "AWS_SECRET_ACCESS_KEY",
        "Expiration": "1970-01-01T00:00:00Z"
        }""",
        expected_headers={'X-aws-ec2-metadata-token': 'mytoken'}
    )
    handler.add(
        'PUT',
        '/latest/api/token',
        200,
        {},
        'mytoken2',
        expected_headers={'X-aws-ec2-metadata-token-ttl-seconds': '10'}
    )
    handler.add(
        'GET',
        '/latest/meta-data/iam/security-credentials/myprofile',
        200,
        {},
        """{
        "AccessKeyId": "AWS_ACCESS_KEY_ID",
        "SecretAccessKey": "AWS_SECRET_ACCESS_KEY",
        "Expiration": "1970-01-01T00:00:00Z"
        }""",
        expected_headers={'X-aws-ec2-metadata-token': 'mytoken2'}
    )
    handler.add(
        'GET',
        '/s3_fake_bucket/resource',
        custom_method=get_s3_fake_bucket_resource_method
    )
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options):
            with gdaltest.config_option('CPL_AWS_EC2_API_ROOT_URL', valid_url):
                f = open_for_read('/vsis3/s3_fake_bucket/resource')
                assert f is not None
                data = gdal.VSIFReadL(1, 4, f).decode('ascii')
                gdal.VSIFCloseL(f)

    assert data == 'foo'

    handler = webserver.SequentialHandler()
    handler.add(
        'PUT',
        '/invalid/latest/api/token',
        404
    )
    handler.add(
        'GET',
        '/invalid/latest/meta-data/iam/security-credentials/myprofile',
        404
    )
    with webserver.install_http_handler(handler):
        with gdaltest.config_options(options):
            # Set a fake URL to demonstrate we try to re-fetch credentials
            with gdaltest.config_option(
                    'CPL_AWS_EC2_API_ROOT_URL',
                    invalid_url
            ):
                with gdaltest.error_handler():
                    f = open_for_read('/vsis3/s3_fake_bucket/bar')
        assert f is None

###############################################################################
# Nominal cases (require valid credentials)


def test_vsis3_extra_1(aws_test_config):

    if not gdaltest.built_against_curl():
        pytest.skip()

    credentials_filename = gdal.GetConfigOption(
        'HOME',
        gdal.GetConfigOption('USERPROFILE', '')
    ) + '/.aws/credentials'

    # Either a bucket name or bucket/filename
    s3_resource = gdal.GetConfigOption('S3_RESOURCE')

    if not os.path.exists(credentials_filename):
        if gdal.GetConfigOption('AWS_SECRET_ACCESS_KEY') is None:
            pytest.skip('Missing AWS_SECRET_ACCESS_KEY')
        elif gdal.GetConfigOption('AWS_ACCESS_KEY_ID') is None:
            pytest.skip('Missing AWS_ACCESS_KEY_ID')

    if s3_resource is None:
        pytest.skip('Missing S3_RESOURCE')

    if '/' not in s3_resource:
        path = '/vsis3/' + s3_resource
        statres = gdal.VSIStatL(path)
        assert statres is not None and stat.S_ISDIR(statres.mode), \
            ('%s is not a valid bucket' % path)

        readdir = gdal.ReadDir(path)
        assert readdir is not None, 'ReadDir() should not return empty list'
        for filename in readdir:
            if filename != '.':
                subpath = path + '/' + filename
                assert gdal.VSIStatL(subpath) is not None, \
                    ('Stat(%s) should not return an error' % subpath)

        unique_id = 'vsis3_test'
        subpath = path + '/' + unique_id
        ret = gdal.Mkdir(subpath, 0)
        assert ret >= 0, \
            ('Mkdir(%s) should not return an error' % subpath)

        readdir = gdal.ReadDir(path)
        assert unique_id in readdir, \
            ('ReadDir(%s) should contain %s' % (path, unique_id))

        ret = gdal.Mkdir(subpath, 0)
        assert ret != 0, \
            ('Mkdir(%s) repeated should return an error' % subpath)

        ret = gdal.Rmdir(subpath)
        assert ret >= 0, \
            ('Rmdir(%s) should not return an error' % subpath)

        readdir = gdal.ReadDir(path)
        assert unique_id not in readdir, \
            ('ReadDir(%s) should not contain %s' % (path, unique_id))

        ret = gdal.Rmdir(subpath)
        assert ret != 0, \
            ('Rmdir(%s) repeated should return an error' % subpath)

        ret = gdal.Mkdir(subpath, 0)
        assert ret >= 0, \
            ('Mkdir(%s) should not return an error' % subpath)

        f = gdal.VSIFOpenExL(
            subpath + '/test.txt',
            'wb',
            0,
            ['Content-Type=foo', 'Content-Encoding=bar']
        )
        assert f is not None
        gdal.VSIFWriteL('hello', 1, 5, f)
        gdal.VSIFCloseL(f)

        md = gdal.GetFileMetadata(subpath + '/test.txt', 'HEADERS')
        assert 'Content-Type' in md
        assert md['Content-Type'] == 'foo'
        assert 'Content-Encoding' in md
        assert md['Content-Encoding'] == 'bar'

        ret = gdal.Rmdir(subpath)
        assert ret != 0, \
            (
                'Rmdir(%s) on non empty directory should return an error'
                % subpath
            )

        f = gdal.VSIFOpenL(subpath + '/test.txt', 'rb')
        assert f is not None
        data = gdal.VSIFReadL(1, 5, f).decode('utf-8')
        assert data == 'hello'
        gdal.VSIFCloseL(f)

        assert gdal.Rename(subpath + '/test.txt', subpath + '/test2.txt') == 0

        f = gdal.VSIFOpenL(subpath + '/test2.txt', 'rb')
        assert f is not None
        data = gdal.VSIFReadL(1, 5, f).decode('utf-8')
        assert data == 'hello'
        gdal.VSIFCloseL(f)

        ret = gdal.Unlink(subpath + '/test2.txt')
        assert ret >= 0, \
            (
                'Unlink(%s) should not return an error'
                % (subpath + '/test2.txt')
            )

        ret = gdal.Rmdir(subpath)
        assert ret >= 0, ('Rmdir(%s) should not return an error' % subpath)

        return

    f = open_for_read('/vsis3/' + s3_resource)
    assert f is not None, ('cannot open %s' % ('/vsis3/' + s3_resource))
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    assert len(ret) == 1

    # Same with /vsis3_streaming/
    f = open_for_read('/vsis3_streaming/' + s3_resource)
    assert f is not None
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    assert len(ret) == 1

    if False:  # pylint: disable=using-constant-test
        # we actually try to read at read() time and bSetError = false
        # Invalid bucket : "The specified bucket does not exist"
        gdal.ErrorReset()
        f = open_for_read('/vsis3/not_existing_bucket/foo')
        with gdaltest.error_handler():
            gdal.VSIFReadL(1, 1, f)
        gdal.VSIFCloseL(f)
        assert gdal.VSIGetLastErrorMsg() != ''

    # Invalid resource
    gdal.ErrorReset()
    f = open_for_read(
        '/vsis3_streaming/'
        + gdal.GetConfigOption('S3_RESOURCE')
        + '/invalid_resource.baz'
    )
    assert f is None, gdal.VSIGetLastErrorMsg()

    # Test GetSignedURL()
    signed_url = gdal.GetSignedURL('/vsis3/' + s3_resource)
    f = open_for_read('/vsicurl_streaming/' + signed_url)
    assert f is not None
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    assert len(ret) == 1
