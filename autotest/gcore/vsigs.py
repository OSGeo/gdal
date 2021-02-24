#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsigs
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2017 Even Rouault <even dot rouault at spatialys dot com>
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


import gdaltest
import webserver
import pytest


def open_for_read(uri):
    """
    Opens a test file for reading.
    """
    return gdal.VSIFOpenExL(uri, 'rb', 1)

###############################################################################


def test_vsigs_init():

    gdaltest.gs_vars = {}
    for var in ('GS_SECRET_ACCESS_KEY', 'GS_ACCESS_KEY_ID',
                'GOOGLE_APPLICATION_CREDENTIALS',
                'CPL_GS_TIMESTAMP', 'CPL_GS_ENDPOINT',
                'GDAL_HTTP_HEADER_FILE'):
        gdaltest.gs_vars[var] = gdal.GetConfigOption(var)
        if gdaltest.gs_vars[var] is not None:
            gdal.SetConfigOption(var, "")

    # To avoid user credentials in ~/.boto
    # to mess up our tests
    gdal.SetConfigOption('CPL_GS_CREDENTIALS_FILE', '')
    gdal.SetConfigOption('GS_OAUTH2_REFRESH_TOKEN', '')
    gdal.SetConfigOption('GS_OAUTH2_CLIENT_EMAIL', '')
    gdal.SetConfigOption('GS_OAUTH2_CLIENT_SECRET', '')
    gdal.SetConfigOption('GS_OAUTH2_CLIENT_ID', '')
    gdal.SetConfigOption('GOOGLE_APPLICATION_CREDENTIALS', '')

    with gdaltest.config_option('CPL_GCE_SKIP', 'YES'):
        assert gdal.GetSignedURL('/vsigs/foo/bar') is None

    
###############################################################################
# Error cases


def test_vsigs_1():

    if not gdaltest.built_against_curl():
        pytest.skip()

    # Invalid header filename
    gdal.ErrorReset()
    with gdaltest.config_option('GDAL_HTTP_HEADER_FILE', '/i_dont/exist.py'):
        with gdaltest.config_option('CPL_GCE_SKIP', 'YES'):
            with gdaltest.error_handler():
                f = open_for_read('/vsigs/foo/bar')
    if f is not None:
        gdal.VSIFCloseL(f)
        pytest.fail()
    last_err = gdal.GetLastErrorMsg()
    assert 'Cannot read' in last_err

    # Invalid content for header file
    with gdaltest.config_option('GDAL_HTTP_HEADER_FILE', 'vsigs.py'):
        with gdaltest.config_option('CPL_GCE_SKIP', 'YES'):
            f = open_for_read('/vsigs/foo/bar')
    if f is not None:
        gdal.VSIFCloseL(f)
        pytest.fail()

    # Missing GS_SECRET_ACCESS_KEY
    gdal.ErrorReset()
    with gdaltest.config_option('CPL_GCE_SKIP', 'YES'):
        with gdaltest.error_handler():
            f = open_for_read('/vsigs/foo/bar')
    assert f is None and gdal.VSIGetLastErrorMsg().find('GS_SECRET_ACCESS_KEY') >= 0

    gdal.ErrorReset()
    with gdaltest.config_option('CPL_GCE_SKIP', 'YES'):
        with gdaltest.error_handler():
            f = open_for_read('/vsigs_streaming/foo/bar')
    assert f is None and gdal.VSIGetLastErrorMsg().find('GS_SECRET_ACCESS_KEY') >= 0

    gdal.SetConfigOption('GS_SECRET_ACCESS_KEY', 'GS_SECRET_ACCESS_KEY')

    # Missing GS_ACCESS_KEY_ID
    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsigs/foo/bar')
    assert f is None and gdal.VSIGetLastErrorMsg().find('GS_ACCESS_KEY_ID') >= 0

    gdal.SetConfigOption('GS_ACCESS_KEY_ID', 'GS_ACCESS_KEY_ID')

    # ERROR 1: The User Id you provided does not exist in our records.
    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsigs/foo/bar.baz')
    if f is not None or gdal.VSIGetLastErrorMsg() == '':
        if f is not None:
            gdal.VSIFCloseL(f)
        if gdal.GetConfigOption('APPVEYOR') is not None:
            return
        pytest.fail(gdal.VSIGetLastErrorMsg())

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsigs_streaming/foo/bar.baz')
    assert f is None and gdal.VSIGetLastErrorMsg() != ''

###############################################################################


def test_vsigs_start_webserver():

    gdaltest.webserver_process = None
    gdaltest.webserver_port = 0

    if not gdaltest.built_against_curl():
        pytest.skip()

    (gdaltest.webserver_process, gdaltest.webserver_port) = webserver.launch(handler=webserver.DispatcherHttpHandler)
    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.SetConfigOption('CPL_GS_ENDPOINT', 'http://127.0.0.1:%d/' % gdaltest.webserver_port)
    gdal.SetConfigOption('GS_SECRET_ACCESS_KEY', 'GS_SECRET_ACCESS_KEY')
    gdal.SetConfigOption('GS_ACCESS_KEY_ID', 'GS_ACCESS_KEY_ID')

###############################################################################
# Test with a fake Google Cloud Storage server


def test_vsigs_2():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    # header file
    gdal.FileFromMemBuffer('/vsimem/my_headers.txt', 'foo: bar')

    handler = webserver.SequentialHandler()
    handler.add('GET', '/gs_fake_bucket_http_header_file/resource', 200,
                {'Content-type': 'text/plain'}, 'Y',
                expected_headers={'foo': 'bar'})
    with webserver.install_http_handler(handler):
        with gdaltest.config_option('GDAL_HTTP_HEADER_FILE', '/vsimem/my_headers.txt'):
            f = open_for_read('/vsigs/gs_fake_bucket_http_header_file/resource')
            assert f is not None
            data = gdal.VSIFReadL(1, 1, f)
            gdal.VSIFCloseL(f)
            assert len(data) == 1
    gdal.Unlink('/vsimem/my_headers.txt')

    gdal.SetConfigOption('GS_SECRET_ACCESS_KEY', 'GS_SECRET_ACCESS_KEY')
    gdal.SetConfigOption('GS_ACCESS_KEY_ID', 'GS_ACCESS_KEY_ID')
    gdal.SetConfigOption('CPL_GS_TIMESTAMP', 'my_timestamp')

    signed_url = gdal.GetSignedURL('/vsigs/gs_fake_bucket/resource',
                                   ['START_DATE=20180212T123456Z'])
    assert (signed_url in ('http://127.0.0.1:8080/gs_fake_bucket/resource?Expires=1518442496&GoogleAccessId=GS_ACCESS_KEY_ID&Signature=xTphUyMqtKA6UmAX3PEr5VL3EOg%3D',
                          'http://127.0.0.1:8081/gs_fake_bucket/resource?Expires=1518442496&GoogleAccessId=GS_ACCESS_KEY_ID&Signature=xTphUyMqtKA6UmAX3PEr5VL3EOg%3D'))

    handler = webserver.SequentialHandler()
    handler.add('GET', '/gs_fake_bucket/resource', 200,
                {'Content-type': 'text/plain'}, 'foo',
                expected_headers={'Authorization': 'GOOG1 GS_ACCESS_KEY_ID:8tndu9//BfmN+Kg4AFLdUMZMBDQ='})
    with webserver.install_http_handler(handler):
        f = open_for_read('/vsigs/gs_fake_bucket/resource')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    assert data == 'foo'

    handler = webserver.SequentialHandler()
    handler.add('GET', '/gs_fake_bucket/resource', 200,
                {'Content-type': 'text/plain'}, 'foo',
                expected_headers={'Authorization': 'GOOG1 GS_ACCESS_KEY_ID:8tndu9//BfmN+Kg4AFLdUMZMBDQ='})
    with webserver.install_http_handler(handler):
        f = open_for_read('/vsigs_streaming/gs_fake_bucket/resource')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

        assert data == 'foo'

    handler = webserver.SequentialHandler()
    handler.add('GET', '/gs_fake_bucket/resource2.bin', 206,
                {'Content-Range': 'bytes 0-0/1000000'}, 'x')
    with webserver.install_http_handler(handler):
        stat_res = gdal.VSIStatL('/vsigs/gs_fake_bucket/resource2.bin')
        if stat_res is None or stat_res.size != 1000000:
            if stat_res is not None:
                print(stat_res.size)
            else:
                print(stat_res)
            pytest.fail()

    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/gs_fake_bucket/resource2.bin', 200,
                {'Content-Length': 1000000})
    with webserver.install_http_handler(handler):
        stat_res = gdal.VSIStatL('/vsigs_streaming/gs_fake_bucket/resource2.bin')
        if stat_res is None or stat_res.size != 1000000:
            if stat_res is not None:
                print(stat_res.size)
            else:
                print(stat_res)
            pytest.fail()

    
###############################################################################
# Test ReadDir() with a fake Google Cloud Storage server


def test_vsigs_readdir():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/gs_fake_bucket2/?delimiter=%2F&prefix=a_dir%2F', 200,
                {'Content-type': 'application/xml'},
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
    handler.add('GET', '/gs_fake_bucket2/?delimiter=%2F&marker=bla&prefix=a_dir%2F', 200,
                {'Content-type': 'application/xml'},
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
            pytest.skip('Skipped on trusty branch, but should be investigated')

        pytest.fail()
    gdal.VSIFCloseL(f)

    dir_contents = gdal.ReadDir('/vsigs/gs_fake_bucket2/a_dir')
    assert dir_contents == ['resource3.bin', 'resource4.bin', 'subdir']
    assert gdal.VSIStatL('/vsigs/gs_fake_bucket2/a_dir/resource3.bin').size == 123456
    assert gdal.VSIStatL('/vsigs/gs_fake_bucket2/a_dir/resource3.bin').mtime == 1

    # ReadDir on something known to be a file shouldn't cause network access
    dir_contents = gdal.ReadDir('/vsigs/gs_fake_bucket2/a_dir/resource3.bin')
    assert dir_contents is None

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
        dir_contents = gdal.ReadDir('/vsigs/')
    assert dir_contents == ['mybucket']

###############################################################################
# Test write


def test_vsigs_write():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    f = gdal.VSIFOpenL('/vsigs/test_copy/file.bin', 'wb')
    assert f is not None

    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = 'HTTP/1.1'
        request.wfile.write('HTTP/1.1 100 Continue\r\n\r\n'.encode('ascii'))
        content = ''
        while True:
            numchars = int(request.rfile.readline().strip(), 16)
            content += request.rfile.read(numchars).decode('ascii')
            request.rfile.read(2)
            if numchars == 0:
                break
        if len(content) != 40000:
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)
            request.send_header('Content-Length', 0)
            request.end_headers()
            return
        request.send_response(200)
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add('PUT', '/test_copy/file.bin', custom_method=method)
    with webserver.install_http_handler(handler):
        ret = gdal.VSIFWriteL('x' * 35000, 1, 35000, f)
        ret += gdal.VSIFWriteL('x' * 5000, 1, 5000, f)
        if ret != 40000:
            gdal.VSIFCloseL(f)
            pytest.fail(ret)
        gdal.VSIFCloseL(f)

    # Simulate failure while transmitting
    f = gdal.VSIFOpenL('/vsigs/test_copy/file.bin', 'wb')
    assert f is not None

    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = 'HTTP/1.1'
        request.send_response(403)
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add('PUT', '/test_copy/file.bin', custom_method=method)
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ret = gdal.VSIFWriteL('x' * 35000, 1, 35000, f)
        if ret != 0:
            gdal.VSIFCloseL(f)
            pytest.fail(ret)
    gdal.VSIFCloseL(f)

    # Simulate failure at end of transfer
    f = gdal.VSIFOpenL('/vsigs/test_copy/file.bin', 'wb')
    assert f is not None

    handler = webserver.SequentialHandler()

    def method(request):
        request.protocol_version = 'HTTP/1.1'
        request.wfile.write('HTTP/1.1 100 Continue\r\n\r\n'.encode('ascii'))
        content = ''
        while True:
            numchars = int(request.rfile.readline().strip(), 16)
            content += request.rfile.read(numchars).decode('ascii')
            request.rfile.read(2)
            if numchars == 0:
                break
        request.send_response(403)
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add('PUT', '/test_copy/file.bin', custom_method=method)
    with webserver.install_http_handler(handler):
        ret = gdal.VSIFWriteL('x' * 35000, 1, 35000, f)
        if ret != 35000:
            gdal.VSIFCloseL(f)
            pytest.fail(ret)
        with gdaltest.error_handler():
            ret = gdal.VSIFCloseL(f)
        assert ret != 0

###############################################################################
# Test write with retry


def test_vsigs_write_retry():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    with gdaltest.config_options({'GDAL_HTTP_MAX_RETRY': '2',
                                  'GDAL_HTTP_RETRY_DELAY': '0.01'}):

        f = gdal.VSIFOpenL('/vsigs/test_write_retry/put_with_retry.bin', 'wb')
        assert f is not None

        handler = webserver.SequentialHandler()

        def method(request):
            request.protocol_version = 'HTTP/1.1'
            request.wfile.write('HTTP/1.1 100 Continue\r\n\r\n'.encode('ascii'))
            content = ''
            while True:
                numchars = int(request.rfile.readline().strip(), 16)
                content += request.rfile.read(numchars).decode('ascii')
                request.rfile.read(2)
                if numchars == 0:
                    break
            if len(content) != 3:
                sys.stderr.write('Bad headers: %s\n' % str(request.headers))
                request.send_response(403)
                request.send_header('Content-Length', 0)
                request.end_headers()
                return
            request.send_response(200)
            request.send_header('Content-Length', 0)
            request.end_headers()

        handler.add('PUT', '/test_write_retry/put_with_retry.bin', 502)
        handler.add('PUT', '/test_write_retry/put_with_retry.bin', custom_method=method)

        with gdaltest.error_handler():
            with webserver.install_http_handler(handler):
                assert gdal.VSIFWriteL('foo', 1, 3, f) == 3
                gdal.VSIFCloseL(f)

###############################################################################
# Test rename

def test_vsigs_fake_rename():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()
    handler = webserver.SequentialHandler()
    handler.add('GET', '/test/source.txt', 206,
                { 'Content-Length' : '3',
                  'Content-Range': 'bytes 0-2/3' }, "foo")
    handler.add('GET', '/test/target.txt', 404)
    handler.add('GET', '/test/?delimiter=%2F&max-keys=100&prefix=target.txt%2F', 200)

    def method(request):
        if request.headers['Content-Length'] != '0':
            sys.stderr.write('Did not get expected headers: %s\n' % str(request.headers))
            request.send_response(400)
            return
        if request.headers['x-goog-copy-source'] != '/test/source.txt':
            sys.stderr.write('Did not get expected headers: %s\n' % str(request.headers))
            request.send_response(400)
            return

        request.send_response(200)
        request.send_header('Content-Length', 0)
        request.end_headers()

    handler.add('PUT', '/test/target.txt', custom_method=method)
    handler.add('DELETE', '/test/source.txt', 204)

    with webserver.install_http_handler(handler):
        assert gdal.Rename( '/vsigs/test/source.txt', '/vsigs/test/target.txt') == 0

###############################################################################
# Read credentials with OAuth2 refresh_token


def test_vsigs_read_credentials_refresh_token_default_gdal_app():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.SetConfigOption('GS_SECRET_ACCESS_KEY', '')
    gdal.SetConfigOption('GS_ACCESS_KEY_ID', '')

    gdal.SetConfigOption('GOA2_AUTH_URL_TOKEN',
                         'http://localhost:%d/accounts.google.com/o/oauth2/token' % gdaltest.webserver_port)

    gdal.SetConfigOption('GS_OAUTH2_REFRESH_TOKEN', 'REFRESH_TOKEN')

    with gdaltest.error_handler():
        assert gdal.GetSignedURL('/vsigs/foo/bar') is None

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()

    def method(request):
        content = request.rfile.read(int(request.headers['Content-Length'])).decode('ascii')
        if content != 'refresh_token=REFRESH_TOKEN&client_id=265656308688.apps.googleusercontent.com&client_secret=0IbTUDOYzaL6vnIdWTuQnvLz&grant_type=refresh_token':
            sys.stderr.write('Bad POST content: %s\n' % content)
            request.send_response(403)
            return

        request.send_response(200)
        request.send_header('Content-type', 'text/plain')
        content = """{
                "access_token" : "ACCESS_TOKEN",
                "token_type" : "Bearer",
                "expires_in" : 3600,
                }"""
        request.send_header('Content-Length', len(content))
        request.end_headers()
        request.wfile.write(content.encode('ascii'))

    handler.add('POST', '/accounts.google.com/o/oauth2/token', custom_method=method)

    def method(request):
        if 'Authorization' not in request.headers:
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)
            return
        expected_authorization = 'Bearer ACCESS_TOKEN'
        if request.headers['Authorization'] != expected_authorization:
            sys.stderr.write("Bad Authorization: '%s'\n" % str(request.headers['Authorization']))
            request.send_response(403)
            return

        request.send_response(200)
        request.send_header('Content-type', 'text/plain')
        request.send_header('Content-Length', 3)
        request.end_headers()
        request.wfile.write("""foo""".encode('ascii'))

    handler.add('GET', '/gs_fake_bucket/resource', custom_method=method)
    with webserver.install_http_handler(handler):
        f = open_for_read('/vsigs/gs_fake_bucket/resource')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    assert data == 'foo'

    gdal.SetConfigOption('GOA2_AUTH_URL_TOKEN', None)
    gdal.SetConfigOption('GS_OAUTH2_REFRESH_TOKEN', '')

###############################################################################
# Read credentials with OAuth2 refresh_token


def test_vsigs_read_credentials_refresh_token_custom_app():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.SetConfigOption('GS_SECRET_ACCESS_KEY', '')
    gdal.SetConfigOption('GS_ACCESS_KEY_ID', '')

    gdal.SetConfigOption('GOA2_AUTH_URL_TOKEN',
                         'http://localhost:%d/accounts.google.com/o/oauth2/token' % gdaltest.webserver_port)

    gdal.SetConfigOption('GS_OAUTH2_REFRESH_TOKEN', 'REFRESH_TOKEN')
    gdal.SetConfigOption('GS_OAUTH2_CLIENT_ID', 'CLIENT_ID')
    gdal.SetConfigOption('GS_OAUTH2_CLIENT_SECRET', 'CLIENT_SECRET')

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()

    def method(request):
        content = request.rfile.read(int(request.headers['Content-Length'])).decode('ascii')
        if content != 'refresh_token=REFRESH_TOKEN&client_id=CLIENT_ID&client_secret=CLIENT_SECRET&grant_type=refresh_token':
            sys.stderr.write('Bad POST content: %s\n' % content)
            request.send_response(403)
            return

        request.send_response(200)
        request.send_header('Content-type', 'text/plain')
        content = """{
                "access_token" : "ACCESS_TOKEN",
                "token_type" : "Bearer",
                "expires_in" : 3600,
                }"""
        request.send_header('Content-Length', len(content))
        request.end_headers()
        request.wfile.write(content.encode('ascii'))

    handler.add('POST', '/accounts.google.com/o/oauth2/token', custom_method=method)

    def method(request):
        if 'Authorization' not in request.headers:
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)
            return
        expected_authorization = 'Bearer ACCESS_TOKEN'
        if request.headers['Authorization'] != expected_authorization:
            sys.stderr.write("Bad Authorization: '%s'\n" % str(request.headers['Authorization']))
            request.send_response(403)
            return

        request.send_response(200)
        request.send_header('Content-type', 'text/plain')
        request.send_header('Content-Length', 3)
        request.end_headers()
        request.wfile.write("""foo""".encode('ascii'))

    handler.add('GET', '/gs_fake_bucket/resource', custom_method=method)
    with webserver.install_http_handler(handler):
        f = open_for_read('/vsigs/gs_fake_bucket/resource')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    assert data == 'foo'

    gdal.SetConfigOption('GOA2_AUTH_URL_TOKEN', None)
    gdal.SetConfigOption('GS_OAUTH2_REFRESH_TOKEN', '')
    gdal.SetConfigOption('GS_OAUTH2_CLIENT_ID', '')
    gdal.SetConfigOption('GS_OAUTH2_CLIENT_SECRET', '')

###############################################################################
# Read credentials with OAuth2 service account


def test_vsigs_read_credentials_oauth2_service_account():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.SetConfigOption('GS_SECRET_ACCESS_KEY', '')
    gdal.SetConfigOption('GS_ACCESS_KEY_ID', '')

    # Generated with 'openssl genrsa -out rsa-openssl.pem 1024' and
    # 'openssl pkcs8 -nocrypt -in rsa-openssl.pem -inform PEM -topk8 -outform PEM -out rsa-openssl.pkcs8.pem'
    # DO NOT USE in production !!!!
    key = """-----BEGIN PRIVATE KEY-----
MIICeAIBADANBgkqhkiG9w0BAQEFAASCAmIwggJeAgEAAoGBAOlwJQLLDG1HeLrk
VNcFR5Qptto/rJE5emRuy0YmkVINT4uHb1be7OOo44C2Ev8QPVtNHHS2XwCY5gTm
i2RfIBLv+VDMoVQPqqE0LHb0WeqGmM5V1tHbmVnIkCcKMn3HpK30grccuBc472LQ
DVkkGqIiGu0qLAQ89JP/r0LWWySRAgMBAAECgYAWjsS00WRBByAOh1P/dz4kfidy
TabiXbiLDf3MqJtwX2Lpa8wBjAc+NKrPXEjXpv0W3ou6Z4kkqKHJpXGg4GRb4N5I
2FA+7T1lA0FCXa7dT2jvgJLgpBepJu5b//tqFqORb4A4gMZw0CiPN3sUsWsSw5Hd
DrRXwp6sarzG77kvZQJBAPgysAmmXIIp9j1hrFSkctk4GPkOzZ3bxKt2Nl4GFrb+
bpKSon6OIhP1edrxTz1SMD1k5FiAAVUrMDKSarbh5osCQQDwxq4Tvf/HiYz79JBg
Wz5D51ySkbg01dOVgFW3eaYAdB6ta/o4vpHhnbrfl6VO9oUb3QR4hcrruwnDHsw3
4mDTAkEA9FPZjbZSTOSH/cbgAXbdhE4/7zWOXj7Q7UVyob52r+/p46osAk9i5qj5
Kvnv2lrFGDrwutpP9YqNaMtP9/aLnwJBALLWf9n+GAv3qRZD0zEe1KLPKD1dqvrj
j+LNjd1Xp+tSVK7vMs4PDoAMDg+hrZF3HetSQM3cYpqxNFEPgRRJOy0CQQDQlZHI
yzpSgEiyx8O3EK1iTidvnLXbtWabvjZFfIE/0OhfBmN225MtKG3YLV2HoUvpajLq
gwE6fxOLyJDxuWRf
-----END PRIVATE KEY-----
"""

    for i in range(2):

        gdal.SetConfigOption('GO2A_AUD',
                             'http://localhost:%d/oauth2/v4/token' % gdaltest.webserver_port)
        gdal.SetConfigOption('GOA2_NOW', '123456')

        if i == 0:
            gdal.SetConfigOption('GS_OAUTH2_PRIVATE_KEY', key)
        else:
            gdal.FileFromMemBuffer('/vsimem/pkey', key)
            gdal.SetConfigOption('GS_OAUTH2_PRIVATE_KEY_FILE', '/vsimem/pkey')

        gdal.SetConfigOption('GS_OAUTH2_CLIENT_EMAIL', 'CLIENT_EMAIL')

        gdal.VSICurlClearCache()

        handler = webserver.SequentialHandler()

        def method(request):
            content = request.rfile.read(int(request.headers['Content-Length'])).decode('ascii')
            content_8080 = 'grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiAiQ0xJRU5UX0VNQUlMIiwgInNjb3BlIjogImh0dHBzOi8vd3d3Lmdvb2dsZWFwaXMuY29tL2F1dGgvZGV2c3RvcmFnZS5yZWFkX3dyaXRlIiwgImF1ZCI6ICJodHRwOi8vbG9jYWxob3N0OjgwODAvb2F1dGgyL3Y0L3Rva2VuIiwgImlhdCI6IDEyMzQ1NiwgImV4cCI6IDEyNzA1Nn0%3D.DAhqWtBgKpObxZ%2BGiXqwF%2Fa4SS%2FNWQRhLCI7DYZCuOTuf2w7dL8j4CdpiwwzQg1diIus7dyViRfzpsFmuZKAXwL%2B84iBoVVqnJJZ4TgwH49NdfMAnc4Rgm%2Bo2a2nEcMjX%2FbQ3jRY%2B9WNVl96hzULGvLrVeyego2f06wivqmvxHA%3D'
            content_8081 = 'grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiAiQ0xJRU5UX0VNQUlMIiwgInNjb3BlIjogImh0dHBzOi8vd3d3Lmdvb2dsZWFwaXMuY29tL2F1dGgvZGV2c3RvcmFnZS5yZWFkX3dyaXRlIiwgImF1ZCI6ICJodHRwOi8vbG9jYWxob3N0OjgwODEvb2F1dGgyL3Y0L3Rva2VuIiwgImlhdCI6IDEyMzQ1NiwgImV4cCI6IDEyNzA1Nn0%3D.0abOEg4%2FRApWTSeAs6YTHaNzdwOgZLm8DTMO2MKlOA%2Fiagyb4cBJxDpkD5gECPvi7qhkg7LsyFuj0a%2BK48Bsuj%2FgLHOU4MpB0dHwYnDO2UXzH%2FUPdgFCVak1P1V%2ByiDA%2B%2Ft4aDI5fD9qefKQiu3wsMDHzP71MNLzayrjqaqKKS4%3D'
            if content not in [content_8080, content_8081]:
                sys.stderr.write('Bad POST content: %s\n' % content)
                request.send_response(403)
                return

            request.send_response(200)
            request.send_header('Content-type', 'text/plain')
            content = """{
                    "access_token" : "ACCESS_TOKEN",
                    "token_type" : "Bearer",
                    "expires_in" : 3600,
                    }"""
            request.send_header('Content-Length', len(content))
            request.end_headers()
            request.wfile.write(content.encode('ascii'))

        handler.add('POST', '/oauth2/v4/token', custom_method=method)

        def method(request):
            if 'Authorization' not in request.headers:
                sys.stderr.write('Bad headers: %s\n' % str(request.headers))
                request.send_response(403)
                return
            expected_authorization = 'Bearer ACCESS_TOKEN'
            if request.headers['Authorization'] != expected_authorization:
                sys.stderr.write("Bad Authorization: '%s'\n" % str(request.headers['Authorization']))
                request.send_response(403)
                return

            request.send_response(200)
            request.send_header('Content-type', 'text/plain')
            request.send_header('Content-Length', 3)
            request.end_headers()
            request.wfile.write("""foo""".encode('ascii'))

        handler.add('GET', '/gs_fake_bucket/resource', custom_method=method)
        try:
            with webserver.install_http_handler(handler):
                f = open_for_read('/vsigs/gs_fake_bucket/resource')
                assert f is not None
                data = gdal.VSIFReadL(1, 4, f).decode('ascii')
                gdal.VSIFCloseL(f)
        except:
            if gdal.GetLastErrorMsg().find('CPLRSASHA256Sign() not implemented') >= 0:
                pytest.skip()
        finally:
            gdal.SetConfigOption('GO2A_AUD', None)
            gdal.SetConfigOption('GO2A_NOW', None)
            gdal.SetConfigOption('GS_OAUTH2_PRIVATE_KEY', '')
            gdal.SetConfigOption('GS_OAUTH2_PRIVATE_KEY_FILE', '')
            gdal.SetConfigOption('GS_OAUTH2_CLIENT_EMAIL', '')

        assert data == 'foo'

    gdal.Unlink('/vsimem/pkey')

###############################################################################
# Read credentials with OAuth2 service account through a json configuration file


def test_vsigs_read_credentials_oauth2_service_account_json_file():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.SetConfigOption('GS_SECRET_ACCESS_KEY', '')
    gdal.SetConfigOption('GS_ACCESS_KEY_ID', '')

    gdal.FileFromMemBuffer('/vsimem/service_account.json', """{
  "private_key": "-----BEGIN PRIVATE KEY-----\nMIICeAIBADANBgkqhkiG9w0BAQEFAASCAmIwggJeAgEAAoGBAOlwJQLLDG1HeLrk\nVNcFR5Qptto/rJE5emRuy0YmkVINT4uHb1be7OOo44C2Ev8QPVtNHHS2XwCY5gTm\ni2RfIBLv+VDMoVQPqqE0LHb0WeqGmM5V1tHbmVnIkCcKMn3HpK30grccuBc472LQ\nDVkkGqIiGu0qLAQ89JP/r0LWWySRAgMBAAECgYAWjsS00WRBByAOh1P/dz4kfidy\nTabiXbiLDf3MqJtwX2Lpa8wBjAc+NKrPXEjXpv0W3ou6Z4kkqKHJpXGg4GRb4N5I\n2FA+7T1lA0FCXa7dT2jvgJLgpBepJu5b//tqFqORb4A4gMZw0CiPN3sUsWsSw5Hd\nDrRXwp6sarzG77kvZQJBAPgysAmmXIIp9j1hrFSkctk4GPkOzZ3bxKt2Nl4GFrb+\nbpKSon6OIhP1edrxTz1SMD1k5FiAAVUrMDKSarbh5osCQQDwxq4Tvf/HiYz79JBg\nWz5D51ySkbg01dOVgFW3eaYAdB6ta/o4vpHhnbrfl6VO9oUb3QR4hcrruwnDHsw3\n4mDTAkEA9FPZjbZSTOSH/cbgAXbdhE4/7zWOXj7Q7UVyob52r+/p46osAk9i5qj5\nKvnv2lrFGDrwutpP9YqNaMtP9/aLnwJBALLWf9n+GAv3qRZD0zEe1KLPKD1dqvrj\nj+LNjd1Xp+tSVK7vMs4PDoAMDg+hrZF3HetSQM3cYpqxNFEPgRRJOy0CQQDQlZHI\nyzpSgEiyx8O3EK1iTidvnLXbtWabvjZFfIE/0OhfBmN225MtKG3YLV2HoUvpajLq\ngwE6fxOLyJDxuWRf\n-----END PRIVATE KEY-----\n",
  "client_email": "CLIENT_EMAIL"
                           }""")

    gdal.SetConfigOption('GOOGLE_APPLICATION_CREDENTIALS', '/vsimem/service_account.json')

    gdal.SetConfigOption('GO2A_AUD',
                         'http://localhost:%d/oauth2/v4/token' % gdaltest.webserver_port)
    gdal.SetConfigOption('GOA2_NOW', '123456')

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()

    def method(request):
        content = request.rfile.read(int(request.headers['Content-Length'])).decode('ascii')
        content_8080 = 'grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiAiQ0xJRU5UX0VNQUlMIiwgInNjb3BlIjogImh0dHBzOi8vd3d3Lmdvb2dsZWFwaXMuY29tL2F1dGgvZGV2c3RvcmFnZS5yZWFkX3dyaXRlIiwgImF1ZCI6ICJodHRwOi8vbG9jYWxob3N0OjgwODAvb2F1dGgyL3Y0L3Rva2VuIiwgImlhdCI6IDEyMzQ1NiwgImV4cCI6IDEyNzA1Nn0%3D.DAhqWtBgKpObxZ%2BGiXqwF%2Fa4SS%2FNWQRhLCI7DYZCuOTuf2w7dL8j4CdpiwwzQg1diIus7dyViRfzpsFmuZKAXwL%2B84iBoVVqnJJZ4TgwH49NdfMAnc4Rgm%2Bo2a2nEcMjX%2FbQ3jRY%2B9WNVl96hzULGvLrVeyego2f06wivqmvxHA%3D'
        content_8081 = 'grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiAiQ0xJRU5UX0VNQUlMIiwgInNjb3BlIjogImh0dHBzOi8vd3d3Lmdvb2dsZWFwaXMuY29tL2F1dGgvZGV2c3RvcmFnZS5yZWFkX3dyaXRlIiwgImF1ZCI6ICJodHRwOi8vbG9jYWxob3N0OjgwODEvb2F1dGgyL3Y0L3Rva2VuIiwgImlhdCI6IDEyMzQ1NiwgImV4cCI6IDEyNzA1Nn0%3D.0abOEg4%2FRApWTSeAs6YTHaNzdwOgZLm8DTMO2MKlOA%2Fiagyb4cBJxDpkD5gECPvi7qhkg7LsyFuj0a%2BK48Bsuj%2FgLHOU4MpB0dHwYnDO2UXzH%2FUPdgFCVak1P1V%2ByiDA%2B%2Ft4aDI5fD9qefKQiu3wsMDHzP71MNLzayrjqaqKKS4%3D'
        if content not in [content_8080, content_8081]:
            sys.stderr.write('Bad POST content: %s\n' % content)
            request.send_response(403)
            return

        request.send_response(200)
        request.send_header('Content-type', 'text/plain')
        content = """{
                "access_token" : "ACCESS_TOKEN",
                "token_type" : "Bearer",
                "expires_in" : 3600,
                }"""
        request.send_header('Content-Length', len(content))
        request.end_headers()
        request.wfile.write(content.encode('ascii'))

    handler.add('POST', '/oauth2/v4/token', custom_method=method)

    def method(request):
        if 'Authorization' not in request.headers:
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)
            return
        expected_authorization = 'Bearer ACCESS_TOKEN'
        if request.headers['Authorization'] != expected_authorization:
            sys.stderr.write("Bad Authorization: '%s'\n" % str(request.headers['Authorization']))
            request.send_response(403)
            return

        request.send_response(200)
        request.send_header('Content-type', 'text/plain')
        request.send_header('Content-Length', 3)
        request.end_headers()
        request.wfile.write("""foo""".encode('ascii'))

    handler.add('GET', '/gs_fake_bucket/resource', custom_method=method)
    try:
        with webserver.install_http_handler(handler):
            f = open_for_read('/vsigs/gs_fake_bucket/resource')
            if f is None:
                gdal.Unlink('/vsimem/service_account.json')
                pytest.fail()
            data = gdal.VSIFReadL(1, 4, f).decode('ascii')
            gdal.VSIFCloseL(f)

            signed_url = gdal.GetSignedURL('/vsigs/gs_fake_bucket/resource',
                                           ['START_DATE=20180212T123456Z'])
            if signed_url not in ('http://127.0.0.1:8080/gs_fake_bucket/resource?Expires=1518442496&GoogleAccessId=CLIENT_EMAIL&Signature=b19I62KdqV51DpWGxhxGXLGJIA8MHvSJofwOygoeQuIxkM6PmmQFvJYTNWRt9zUVTUoVC0UHVB7ee5Z35NqDC8K4i0quu1hb8Js2B4h0W6OAupvyF3nSQ5D0OJmiSbomGMq0Ehyro5cqJ%2FU%2Fd8oAaKrGKVQScKfXoFrSJBbWkNs%3D',
                                  'http://127.0.0.1:8081/gs_fake_bucket/resource?Expires=1518442496&GoogleAccessId=CLIENT_EMAIL&Signature=b19I62KdqV51DpWGxhxGXLGJIA8MHvSJofwOygoeQuIxkM6PmmQFvJYTNWRt9zUVTUoVC0UHVB7ee5Z35NqDC8K4i0quu1hb8Js2B4h0W6OAupvyF3nSQ5D0OJmiSbomGMq0Ehyro5cqJ%2FU%2Fd8oAaKrGKVQScKfXoFrSJBbWkNs%3D'):
                gdal.Unlink('/vsimem/service_account.json')
                pytest.fail(signed_url)

    except:
        if gdal.GetLastErrorMsg().find('CPLRSASHA256Sign() not implemented') >= 0:
            pytest.skip()
    finally:
        gdal.SetConfigOption('GO2A_AUD', None)
        gdal.SetConfigOption('GO2A_NOW', None)
        gdal.SetConfigOption('GOOGLE_APPLICATION_CREDENTIALS', '')

    gdal.Unlink('/vsimem/service_account.json')

    assert data == 'foo'

###############################################################################
# Read credentials from simulated ~/.boto


def test_vsigs_read_credentials_file():

    if gdaltest.webserver_port == 0:
        pytest.skip()

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
        if request.headers['Authorization'] != expected_authorization:
            sys.stderr.write("Bad Authorization: '%s'\n" % str(request.headers['Authorization']))
            request.send_response(403)
            return

        request.send_response(200)
        request.send_header('Content-type', 'text/plain')
        request.send_header('Content-Length', 3)
        request.end_headers()
        request.wfile.write("""foo""".encode('ascii'))

    handler = webserver.SequentialHandler()
    handler.add('GET', '/gs_fake_bucket/resource', custom_method=method)
    with webserver.install_http_handler(handler):
        f = open_for_read('/vsigs/gs_fake_bucket/resource')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    assert data == 'foo'

    gdal.SetConfigOption('CPL_GS_CREDENTIALS_FILE', '')
    gdal.Unlink('/vsimem/.boto')

###############################################################################
# Read credentials from simulated ~/.boto


def test_vsigs_read_credentials_file_refresh_token():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.SetConfigOption('GS_SECRET_ACCESS_KEY', '')
    gdal.SetConfigOption('GS_ACCESS_KEY_ID', '')

    gdal.SetConfigOption('CPL_GS_CREDENTIALS_FILE', '/vsimem/.boto')
    gdal.SetConfigOption('GOA2_AUTH_URL_TOKEN',
                         'http://localhost:%d/accounts.google.com/o/oauth2/token' % gdaltest.webserver_port)

    gdal.VSICurlClearCache()

    gdal.FileFromMemBuffer('/vsimem/.boto', """
[Credentials]
gs_oauth2_refresh_token = REFRESH_TOKEN
[OAuth2]
client_id = CLIENT_ID
client_secret = CLIENT_SECRET
""")

    handler = webserver.SequentialHandler()

    def method(request):
        content = request.rfile.read(int(request.headers['Content-Length'])).decode('ascii')
        if content != 'refresh_token=REFRESH_TOKEN&client_id=CLIENT_ID&client_secret=CLIENT_SECRET&grant_type=refresh_token':
            sys.stderr.write('Bad POST content: %s\n' % content)
            request.send_response(403)
            return

        request.send_response(200)
        request.send_header('Content-type', 'text/plain')
        content = """{
                "access_token" : "ACCESS_TOKEN",
                "token_type" : "Bearer",
                "expires_in" : 3600,
                }"""
        request.send_header('Content-Length', len(content))
        request.end_headers()
        request.wfile.write(content.encode('ascii'))

    handler.add('POST', '/accounts.google.com/o/oauth2/token', custom_method=method)

    def method(request):
        if 'Authorization' not in request.headers:
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)
            return
        expected_authorization = 'Bearer ACCESS_TOKEN'
        if request.headers['Authorization'] != expected_authorization:
            sys.stderr.write("Bad Authorization: '%s'\n" % str(request.headers['Authorization']))
            request.send_response(403)
            return

        request.send_response(200)
        request.send_header('Content-type', 'text/plain')
        request.send_header('Content-Length', 3)
        request.end_headers()
        request.wfile.write("""foo""".encode('ascii'))

    handler.add('GET', '/gs_fake_bucket/resource', custom_method=method)
    with webserver.install_http_handler(handler):
        f = open_for_read('/vsigs/gs_fake_bucket/resource')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    assert data == 'foo'

    gdal.SetConfigOption('CPL_GS_CREDENTIALS_FILE', '')
    gdal.SetConfigOption('GOA2_AUTH_URL_TOKEN', None)
    gdal.Unlink('/vsimem/.boto')

###############################################################################
# Read credentials from simulated GCE instance


def test_vsigs_read_credentials_gce():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    if sys.platform not in ('linux', 'linux2', 'win32'):
        pytest.skip()

    gdal.SetConfigOption('CPL_GS_CREDENTIALS_FILE', '')
    gdal.SetConfigOption('GS_SECRET_ACCESS_KEY', '')
    gdal.SetConfigOption('GS_ACCESS_KEY_ID', '')

    gdal.SetConfigOption('CPL_GCE_CREDENTIALS_URL',
                         'http://localhost:%d/computeMetadata/v1/instance/service-accounts/default/token' % gdaltest.webserver_port)
    # Disable hypervisor related check to test if we are really on EC2
    gdal.SetConfigOption('CPL_GCE_CHECK_LOCAL_FILES', 'NO')

    gdal.VSICurlClearCache()

    def method(request):
        if 'Authorization' not in request.headers:
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)
            return
        expected_authorization = 'Bearer ACCESS_TOKEN'
        if request.headers['Authorization'] != expected_authorization:
            sys.stderr.write("Bad Authorization: '%s'\n" % str(request.headers['Authorization']))
            request.send_response(403)
            return

        request.send_response(200)
        request.send_header('Content-type', 'text/plain')
        request.send_header('Content-Length', 3)
        request.end_headers()
        request.wfile.write("""foo""".encode('ascii'))

    handler = webserver.SequentialHandler()
    handler.add('GET', '/computeMetadata/v1/instance/service-accounts/default/token', 200, {},
                """{
                "access_token" : "ACCESS_TOKEN",
                "token_type" : "Bearer",
                "expires_in" : 3600,
                }""")
    handler.add('GET', '/gs_fake_bucket/resource', custom_method=method)
    with webserver.install_http_handler(handler):
        f = open_for_read('/vsigs/gs_fake_bucket/resource')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    assert data == 'foo'

    # Set a fake URL to check that credentials re-use works
    gdal.SetConfigOption('CPL_GCE_CREDENTIALS_URL', '')

    handler = webserver.SequentialHandler()
    handler.add('GET', '/gs_fake_bucket/bar', 200, {}, 'bar')
    with webserver.install_http_handler(handler):
        f = open_for_read('/vsigs/gs_fake_bucket/bar')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    assert data == 'bar'

    with gdaltest.error_handler():
        assert gdal.GetSignedURL('/vsigs/foo/bar') is None

    gdal.SetConfigOption('CPL_GCE_CREDENTIALS_URL', '')
    gdal.SetConfigOption('CPL_GCE_CHECK_LOCAL_FILES', None)

###############################################################################
# Read credentials from simulated GCE instance with expiration of the
# cached credentials


def test_vsigs_read_credentials_gce_expiration():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    if sys.platform not in ('linux', 'linux2', 'win32'):
        pytest.skip()

    gdal.SetConfigOption('CPL_GS_CREDENTIALS_FILE', '')
    gdal.SetConfigOption('GS_SECRET_ACCESS_KEY', '')
    gdal.SetConfigOption('GS_ACCESS_KEY_ID', '')

    gdal.SetConfigOption('CPL_GCE_CREDENTIALS_URL',
                         'http://localhost:%d/computeMetadata/v1/instance/service-accounts/default/token' % gdaltest.webserver_port)
    # Disable hypervisor related check to test if we are really on EC2
    gdal.SetConfigOption('CPL_GCE_CHECK_LOCAL_FILES', 'NO')

    gdal.VSICurlClearCache()

    def method(request):
        if 'Authorization' not in request.headers:
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)
            return
        expected_authorization = 'Bearer ACCESS_TOKEN'
        if request.headers['Authorization'] != expected_authorization:
            sys.stderr.write("Bad Authorization: '%s'\n" % str(request.headers['Authorization']))
            request.send_response(403)
            return

        request.send_response(200)
        request.send_header('Content-type', 'text/plain')
        request.send_header('Content-Length', 3)
        request.end_headers()
        request.wfile.write("""foo""".encode('ascii'))

    handler = webserver.SequentialHandler()
    # First time is used when trying to establish if GCE authentication is available
    handler.add('GET', '/computeMetadata/v1/instance/service-accounts/default/token', 200, {},
                """{
                "access_token" : "ACCESS_TOKEN",
                "token_type" : "Bearer",
                "expires_in" : 0,
                }""")
    # Second time is needed because f the access to th file
    handler.add('GET', '/computeMetadata/v1/instance/service-accounts/default/token', 200, {},
                """{
                "access_token" : "ACCESS_TOKEN",
                "token_type" : "Bearer",
                "expires_in" : 0,
                }""")
    handler.add('GET', '/gs_fake_bucket/resource', custom_method=method)
    with webserver.install_http_handler(handler):
        f = open_for_read('/vsigs/gs_fake_bucket/resource')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    assert data == 'foo'

    gdal.SetConfigOption('CPL_GCE_CREDENTIALS_URL', '')
    gdal.SetConfigOption('CPL_GCE_CHECK_LOCAL_FILES', None)

###############################################################################


def test_vsigs_stop_webserver():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    # Clearcache needed to close all connections, since the Python server
    # can only handle one connection at a time
    gdal.VSICurlClearCache()

    webserver.server_stop(gdaltest.webserver_process, gdaltest.webserver_port)

###############################################################################
# Nominal cases (require valid credentials)


def test_vsigs_extra_1():

    if not gdaltest.built_against_curl():
        pytest.skip()

    # if gdal.GetConfigOption('GS_SECRET_ACCESS_KEY') is None:
    #    print('Missing GS_SECRET_ACCESS_KEY')
    #    pytest.skip()
    # elif gdal.GetConfigOption('GS_ACCESS_KEY_ID') is None:
    #    print('Missing GS_ACCESS_KEY_ID')
    #    pytest.skip()

    gs_resource = gdal.GetConfigOption('GS_RESOURCE')
    if gs_resource is None:
        pytest.skip('Missing GS_RESOURCE')

    if '/' not in gs_resource:
        path = '/vsigs/' + gs_resource
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

        unique_id = 'vsigs_test'
        subpath = path + '/' + unique_id
        ret = gdal.Mkdir(subpath, 0)
        assert ret >= 0, ('Mkdir(%s) should not return an error' % subpath)

        readdir = gdal.ReadDir(path)
        assert unique_id in readdir, \
            ('ReadDir(%s) should contain %s' % (path, unique_id))

        ret = gdal.Mkdir(subpath, 0)
        assert ret != 0, ('Mkdir(%s) repeated should return an error' % subpath)

        ret = gdal.Rmdir(subpath)
        assert ret >= 0, ('Rmdir(%s) should not return an error' % subpath)

        readdir = gdal.ReadDir(path)
        assert unique_id not in readdir, \
            ('ReadDir(%s) should not contain %s' % (path, unique_id))

        ret = gdal.Rmdir(subpath)
        assert ret != 0, ('Rmdir(%s) repeated should return an error' % subpath)

        ret = gdal.Mkdir(subpath, 0)
        assert ret >= 0, ('Mkdir(%s) should not return an error' % subpath)

        f = gdal.VSIFOpenExL(subpath + '/test.txt', 'wb', 0, ['Content-Type=foo', 'Content-Encoding=bar'])
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
            ('Rmdir(%s) on non empty directory should return an error' % subpath)

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
            ('Unlink(%s) should not return an error' % (subpath + '/test2.txt'))

        ret = gdal.Rmdir(subpath)
        assert ret >= 0, ('Rmdir(%s) should not return an error' % subpath)

        return

    f = open_for_read('/vsigs/' + gs_resource)
    assert f is not None
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    assert len(ret) == 1

    # Same with /vsigs_streaming/
    f = open_for_read('/vsigs_streaming/' + gs_resource)
    assert f is not None
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    assert len(ret) == 1

    if False:  # pylint: disable=using-constant-test
        # we actually try to read at read() time and bSetError = false
        # Invalid bucket : "The specified bucket does not exist"
        gdal.ErrorReset()
        f = open_for_read('/vsigs/not_existing_bucket/foo')
        with gdaltest.error_handler():
            gdal.VSIFReadL(1, 1, f)
        gdal.VSIFCloseL(f)
        assert gdal.VSIGetLastErrorMsg() != ''

    # Invalid resource
    gdal.ErrorReset()
    f = open_for_read('/vsigs_streaming/' + gs_resource + '/invalid_resource.baz')
    assert f is None, gdal.VSIGetLastErrorMsg()

    # Test GetSignedURL()
    signed_url = gdal.GetSignedURL('/vsigs/' + gs_resource)
    f = open_for_read('/vsicurl_streaming/' + signed_url)
    assert f is not None
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    assert len(ret) == 1

###############################################################################


def test_vsigs_cleanup():

    for var in gdaltest.gs_vars:
        gdal.SetConfigOption(var, gdaltest.gs_vars[var])
