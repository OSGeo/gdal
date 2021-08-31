#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsiswift
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2018 Even Rouault <even dot rouault at spatialys dot com>
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
import json


def open_for_read(uri):
    """
    Opens a test file for reading.
    """
    return gdal.VSIFOpenExL(uri, 'rb', 1)

###############################################################################


def test_vsiswift_init():

    gdaltest.swift_vars = {}
    for var in ('SWIFT_STORAGE_URL', 'SWIFT_AUTH_TOKEN',
                'SWIFT_AUTH_V1_URL', 'SWIFT_USER', 'SWIFT_KEY'):
        gdaltest.swift_vars[var] = gdal.GetConfigOption(var)
        if gdaltest.swift_vars[var] is not None:
            gdal.SetConfigOption(var, "")


###############################################################################
# Error cases


def test_vsiswift_real_server_errors():

    if not gdaltest.built_against_curl():
        pytest.skip()

    # Nothing set
    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsiswift/foo/bar')
    assert f is None and gdal.VSIGetLastErrorMsg().find('SWIFT_STORAGE_URL') >= 0

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsiswift_streaming/foo/bar')
    assert f is None and gdal.VSIGetLastErrorMsg().find('SWIFT_STORAGE_URL') >= 0

    gdal.SetConfigOption('SWIFT_STORAGE_URL', 'http://0.0.0.0')

    # Missing SWIFT_AUTH_TOKEN
    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsiswift/foo/bar')
    assert f is None and gdal.VSIGetLastErrorMsg().find('SWIFT_AUTH_TOKEN') >= 0

    gdal.SetConfigOption('SWIFT_AUTH_TOKEN', 'SWIFT_AUTH_TOKEN')

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsiswift/foo/bar.baz')
    if f is not None:
        if f is not None:
            gdal.VSIFCloseL(f)
        pytest.fail(gdal.VSIGetLastErrorMsg())

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsiswift_streaming/foo/bar.baz')
    assert f is None, gdal.VSIGetLastErrorMsg()

###############################################################################


def test_vsiswift_start_webserver():

    gdaltest.webserver_process = None
    gdaltest.webserver_port = 0

    if not gdaltest.built_against_curl():
        pytest.skip()

    (gdaltest.webserver_process, gdaltest.webserver_port) = webserver.launch(handler=webserver.DispatcherHttpHandler)
    if gdaltest.webserver_port == 0:
        pytest.skip()


###############################################################################
# Test authentication with SWIFT_AUTH_V1_URL + SWIFT_USER + SWIFT_KEY


def test_vsiswift_fake_auth_v1_url():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()
    gdal.SetConfigOption('SWIFT_AUTH_V1_URL', 'http://127.0.0.1:%d/auth/1.0' % gdaltest.webserver_port)
    gdal.SetConfigOption('SWIFT_USER', 'my_user')
    gdal.SetConfigOption('SWIFT_KEY', 'my_key')
    gdal.SetConfigOption('SWIFT_STORAGE_URL', '')
    gdal.SetConfigOption('SWIFT_AUTH_TOKEN', '')

    handler = webserver.SequentialHandler()

    def method(request):

        request.protocol_version = 'HTTP/1.1'
        h = request.headers
        if 'X-Auth-User' not in h or h['X-Auth-User'] != 'my_user' or \
           'X-Auth-Key' not in h or h['X-Auth-Key'] != 'my_key':
            sys.stderr.write('Bad headers: %s\n' % str(h))
            request.send_response(403)
            return
        request.send_response(200)
        request.send_header('Content-Length', 0)
        request.send_header('X-Storage-Url', 'http://127.0.0.1:%d/v1/AUTH_something' % gdaltest.webserver_port)
        request.send_header('X-Auth-Token', 'my_auth_token')
        request.send_header('Connection', 'close')
        request.end_headers()
        request.wfile.write("""foo""".encode('ascii'))

    handler.add('GET', '/auth/1.0', custom_method=method)

    def method(request):

        request.protocol_version = 'HTTP/1.1'
        h = request.headers
        if 'x-auth-token' not in h or \
           h['x-auth-token'] != 'my_auth_token':
            sys.stderr.write('Bad headers: %s\n' % str(h))
            request.send_response(403)
            return
        request.send_response(200)
        request.send_header('Content-type', 'text/plain')
        request.send_header('Content-Length', 3)
        request.send_header('Connection', 'close')
        request.end_headers()
        request.wfile.write("""foo""".encode('ascii'))

    handler.add('GET', '/v1/AUTH_something/foo/bar', custom_method=method)
    with webserver.install_http_handler(handler):
        f = open_for_read('/vsiswift/foo/bar')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    assert data == 'foo'

    # authentication is reused

    def method(request):

        request.protocol_version = 'HTTP/1.1'
        h = request.headers
        if 'x-auth-token' not in h or \
           h['x-auth-token'] != 'my_auth_token':
            sys.stderr.write('Bad headers: %s\n' % str(h))
            request.send_response(403)
            return
        request.send_response(200)
        request.send_header('Content-type', 'text/plain')
        request.send_header('Content-Length', 3)
        request.send_header('Connection', 'close')
        request.end_headers()
        request.wfile.write("""bar""".encode('ascii'))

    handler.add('GET', '/v1/AUTH_something/foo/baz', custom_method=method)

    with webserver.install_http_handler(handler):
        f = open_for_read('/vsiswift/foo/baz')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    assert data == 'bar'


###############################################################################
# Test authentication with OS_IDENTITY_API_VERSION=3 OS_AUTH_URL + OS_USERNAME + OS_PASSWORD


def test_vsiswift_fake_auth_v3_url():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()
    gdal.SetConfigOption('OS_IDENTITY_API_VERSION', '3')
    gdal.SetConfigOption('OS_AUTH_URL', 'http://127.0.0.1:%d/v3' % gdaltest.webserver_port)
    gdal.SetConfigOption('OS_USERNAME', 'my_user')
    gdal.SetConfigOption('OS_USER_DOMAIN_NAME', 'test_user_domain')
    gdal.SetConfigOption('OS_PROJECT_NAME', 'test_proj')
    gdal.SetConfigOption('OS_PROJECT_DOMAIN_NAME', 'test_project_domain')
    gdal.SetConfigOption('OS_REGION_NAME', 'Test')
    gdal.SetConfigOption('OS_PASSWORD', 'pwd')
    gdal.SetConfigOption('SWIFT_STORAGE_URL', '')
    gdal.SetConfigOption('SWIFT_AUTH_TOKEN', '')

    handler = webserver.SequentialHandler()

    def method(request):

        request.protocol_version = 'HTTP/1.1'
        h = request.headers

        if 'Content-Type' not in h or h['Content-Type'] != 'application/json':
            sys.stderr.write('Bad headers: %s\n' % str(h))
            request.send_response(403)
            return

        request_len = int(h['Content-Length'])
        request_body = request.rfile.read(request_len).decode()
        request_json = json.loads(request_body)
        methods = request_json['auth']['identity']["methods"]
        assert "password" in methods
        password = request_json['auth']['identity']['password']['user']['password']
        assert password == 'pwd'

        content = """{
             "token" : {
               "catalog" : [
                 {
                  "endpoints" : [
                     {
                        "region" : "Test",
                        "interface" : "admin",
                        "url" : "http://127.0.0.1:8080/v1/admin/AUTH_something"
                     },
                     {
                        "region" : "Test",
                        "interface" : "internal",
                        "url" : "http://127.0.0.1:8081/v1/internal/AUTH_something"
                     },
                     {
                        "region" : "Test",
                        "interface" : "public",
                        "url" : "http://127.0.0.1:%d/v1/AUTH_something"
                     }
                  ],
                  "type": "object-store",
                  "name" : "swift"
                 }
               ]
             }
          }""" % gdaltest.webserver_port
        content = content.encode('ascii')
        request.send_response(200)
        request.send_header('Content-Length', len(content))
        request.send_header('Content-Type', 'application/json')
        request.send_header('X-Subject-Token', 'my_auth_token')
        request.end_headers()
        request.wfile.write(content)

    handler.add('POST', '/v3/auth/tokens', custom_method=method)

    def method(request):

        request.protocol_version = 'HTTP/1.1'
        h = request.headers
        if 'x-auth-token' not in h or \
           h['x-auth-token'] != 'my_auth_token':
            sys.stderr.write('Bad headers: %s\n' % str(h))
            request.send_response(403)
            return
        request.send_response(200)
        request.send_header('Content-type', 'text/plain')
        request.send_header('Content-Length', 3)
        request.send_header('Connection', 'close')
        request.end_headers()
        request.wfile.write('foo'.encode('ascii'))

    handler.add('GET', '/v1/AUTH_something/foo/bar', custom_method=method)
    with webserver.install_http_handler(handler):
        f = open_for_read('/vsiswift/foo/bar')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        assert data == 'foo'
        gdal.VSIFCloseL(f)


###############################################################################
# Test authentication with OS_IDENTITY_API_VERSION=3 OS_AUTH_TYPE="v3applicationcredential"
# + OS_APPLICATION_CREDENTIAL_ID + OS_APPLICATION_CREDENTIAL_SECRET


def test_vsiswift_fake_auth_v3_application_credential_url():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()
    gdal.SetConfigOption('SWIFT_STORAGE_URL', '')
    gdal.SetConfigOption('SWIFT_AUTH_TOKEN', '')
    with gdaltest.config_options( {
            'OS_IDENTITY_API_VERSION': '3',
            'OS_AUTH_URL': 'http://127.0.0.1:%d/v3' % gdaltest.webserver_port,
            'OS_AUTH_TYPE': 'v3applicationcredential',
            'OS_APPLICATION_CREDENTIAL_ID': 'xxxyyycredential-idyyyxxx==',
            'OS_APPLICATION_CREDENTIAL_SECRET': 'xxxyyycredential-secretyyyxxx==',
            'OS_USER_DOMAIN_NAME': 'test_user_domain',
            'OS_REGION_NAME': 'Test'
        } ):

        handler = webserver.SequentialHandler()

        def method(request):

            request.protocol_version = 'HTTP/1.1'
            h = request.headers

            if 'Content-Type' not in h or h['Content-Type'] != 'application/json':
                sys.stderr.write('Bad headers: %s\n' % str(h))
                request.send_response(403)
                return

            request_len = int(h['Content-Length'])
            request_body = request.rfile.read(request_len).decode()
            request_json = json.loads(request_body)
            methods = request_json['auth']['identity']["methods"]
            assert "application_credential" in methods
            cred_id = request_json['auth']['identity']['application_credential']['id']
            cred_secret = request_json['auth']['identity']['application_credential']['secret']

            assert cred_id == 'xxxyyycredential-idyyyxxx=='
            assert cred_secret == 'xxxyyycredential-secretyyyxxx=='

            content = """{
                 "token" : {
                   "catalog" : [
                     {
                      "endpoints" : [
                         {
                            "region" : "Test",
                            "interface" : "admin",
                            "url" : "http://127.0.0.1:8080/v1/admin/AUTH_something"
                         },
                         {
                            "region" : "Test",
                            "interface" : "internal",
                            "url" : "http://127.0.0.1:8081/v1/internal/AUTH_something"
                         },
                         {
                            "region" : "Test",
                            "interface" : "public",
                            "url" : "http://127.0.0.1:%d/v1/AUTH_something"
                         }
                      ],
                      "type": "object-store",
                      "name" : "swift"
                     }
                   ]
                 }
              }""" % gdaltest.webserver_port
            content = content.encode('ascii')
            request.send_response(200)
            request.send_header('Content-Length', len(content))
            request.send_header('Content-Type', 'application/json')
            request.send_header('X-Subject-Token', 'my_auth_token')
            request.end_headers()
            request.wfile.write(content)

        handler.add('POST', '/v3/auth/tokens', custom_method=method)

        def method(request):

            request.protocol_version = 'HTTP/1.1'
            h = request.headers
            if 'x-auth-token' not in h or \
                    h['x-auth-token'] != 'my_auth_token':
                sys.stderr.write('Bad headers: %s\n' % str(h))
                request.send_response(403)
                return
            request.send_response(200)
            request.send_header('Content-type', 'text/plain')
            request.send_header('Content-Length', 3)
            request.send_header('Connection', 'close')
            request.end_headers()
            request.wfile.write('foo'.encode('ascii'))

        handler.add('GET', '/v1/AUTH_something/foo/bar', custom_method=method)
        with webserver.install_http_handler(handler):
            f = open_for_read('/vsiswift/foo/bar')
            assert f is not None
            data = gdal.VSIFReadL(1, 4, f).decode('ascii')
            assert data == 'foo'
            gdal.VSIFCloseL(f)


###############################################################################
# Test authentication with SWIFT_STORAGE_URL + SWIFT_AUTH_TOKEN


def test_vsiswift_fake_auth_storage_url_and_auth_token():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()
    gdal.SetConfigOption('SWIFT_AUTH_V1_URL', '')
    gdal.SetConfigOption('SWIFT_USER', '')
    gdal.SetConfigOption('SWIFT_KEY', '')
    gdal.SetConfigOption('SWIFT_STORAGE_URL', 'http://127.0.0.1:%d/v1/AUTH_something' % gdaltest.webserver_port)
    gdal.SetConfigOption('SWIFT_AUTH_TOKEN', 'my_auth_token')

    # Failure
    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something/foo/bar', 501)
    with webserver.install_http_handler(handler):
        f = open_for_read('/vsiswift/foo/bar')
        assert f is not None
        gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    gdal.VSICurlClearCache()

    # Success
    def method(request):

        request.protocol_version = 'HTTP/1.1'
        h = request.headers
        if 'x-auth-token' not in h or \
           h['x-auth-token'] != 'my_auth_token':
            sys.stderr.write('Bad headers: %s\n' % str(h))
            request.send_response(403)
            return
        request.send_response(200)
        request.send_header('Content-type', 'text/plain')
        request.send_header('Content-Length', 3)
        request.send_header('Connection', 'close')
        request.end_headers()
        request.wfile.write("""foo""".encode('ascii'))

    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something/foo/bar', custom_method=method)
    with webserver.install_http_handler(handler):
        f = open_for_read('/vsiswift/foo/bar')
        assert f is not None
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    assert data == 'foo'

###############################################################################
# Test VSIStatL()


def test_vsiswift_stat():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something/foo/bar', 206,
                {'Content-Range': 'bytes 0-0/1000000'}, 'x')
    with webserver.install_http_handler(handler):
        stat_res = gdal.VSIStatL('/vsiswift/foo/bar')
        if stat_res is None or stat_res.size != 1000000:
            if stat_res is not None:
                print(stat_res.size)
            else:
                print(stat_res)
            pytest.fail()

    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/v1/AUTH_something/foo/bar', 200, {'Content-Length': '1000000'})
    with webserver.install_http_handler(handler):
        stat_res = gdal.VSIStatL('/vsiswift_streaming/foo/bar')
        if stat_res is None or stat_res.size != 1000000:
            if stat_res is not None:
                print(stat_res.size)
            else:
                print(stat_res)
            pytest.fail()

    # Test stat on container
    handler = webserver.SequentialHandler()
    # GET on the container URL returns something, but we must hack this back
    # to a directory
    handler.add('GET', '/v1/AUTH_something/foo', 200, {}, "blabla")
    with webserver.install_http_handler(handler):
        stat_res = gdal.VSIStatL('/vsiswift/foo')
        assert stat_res is not None and stat.S_ISDIR(stat_res.mode)

    # No network access done
    s = gdal.VSIStatL('/vsiswift/foo',
                      gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG |gdal.VSI_STAT_CACHE_ONLY)
    assert s
    assert stat.S_ISDIR(s.mode)

    # No network access done
    assert gdal.VSIStatL('/vsiswift/i_do_not_exist',
                         gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG | gdal.VSI_STAT_CACHE_ONLY) is None

###############################################################################
# Test ReadDir()


def test_vsiswift_fake_readdir():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something/foo?delimiter=%2F&limit=1', 200,
                {'Content-type': 'application/json'},
                """[
  {
    "last_modified": "1970-01-01T00:00:01",
    "bytes": 123456,
    "name": "bar.baz"
  }
]""")

    handler.add('GET', '/v1/AUTH_something/foo?delimiter=%2F&limit=1&marker=bar.baz', 200,
                {'Content-type': 'application/json'},
                """[
  {
    "subdir": "mysubdir/"
  }
]""")

    handler.add('GET', '/v1/AUTH_something/foo?delimiter=%2F&limit=1&marker=mysubdir%2F', 200,
                {'Content-type': 'application/json'},
                """[
]""")

    with gdaltest.config_option('SWIFT_MAX_KEYS', '1'):
        with webserver.install_http_handler(handler):
            f = open_for_read('/vsiswift/foo/bar.baz')
        assert f is not None
        gdal.VSIFCloseL(f)

    dir_contents = gdal.ReadDir('/vsiswift/foo')
    assert dir_contents == ['bar.baz', 'mysubdir']
    stat_res = gdal.VSIStatL('/vsiswift/foo/bar.baz')
    assert stat_res.size == 123456
    assert stat_res.mtime == 1

    # ReadDir on something known to be a file shouldn't cause network access
    dir_contents = gdal.ReadDir('/vsiswift/foo/bar.baz')
    assert dir_contents is None

    # Test error on ReadDir()
    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something/foo?delimiter=%2F&limit=10000&prefix=error_test%2F', 500)
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir('/vsiswift/foo/error_test/')
    assert dir_contents is None

    # List containers (empty result)
    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something', 200, {'Content-type': 'application/json'},
                """[]
        """)
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir('/vsiswift/')
    assert dir_contents == ['.']

    # List containers
    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something', 200, {'Content-type': 'application/json'},
                """[ { "name": "mycontainer1", "count": 0, "bytes": 0 },
             { "name": "mycontainer2", "count": 0, "bytes": 0}
           ] """)
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir('/vsiswift/')
    assert dir_contents == ['mycontainer1', 'mycontainer2']

    # ReadDir() with a file and directory of same names
    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something', 200, {'Content-type': 'application/json'},
                """[ {
                "last_modified": "1970-01-01T00:00:01",
                "bytes": 123456,
                "name": "foo"
             },
             { "subdir": "foo/"} ] """)
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir('/vsiswift/')
    assert dir_contents == ['foo', 'foo/']

    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something/foo?delimiter=%2F&limit=10000', 200,
                {'Content-type': 'application/json'}, "[]")
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir('/vsiswift/foo/')
    assert dir_contents == ['.']

###############################################################################
# Test write


def test_vsiswift_fake_write():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    # Test creation of BlockBob
    f = gdal.VSIFOpenL('/vsiswift/test_copy/file.bin', 'wb')
    assert f is not None

    handler = webserver.SequentialHandler()

    def method(request):
        h = request.headers
        if 'x-auth-token' not in h or \
           h['x-auth-token'] != 'my_auth_token' or \
           'Transfer-Encoding' not in h or h['Transfer-Encoding'] != 'chunked':
            sys.stderr.write('Bad headers: %s\n' % str(h))
            request.send_response(403)
            return

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

    handler.add('PUT', '/v1/AUTH_something/test_copy/file.bin', custom_method=method)
    with webserver.install_http_handler(handler):
        ret = gdal.VSIFWriteL('x' * 35000, 1, 35000, f)
        ret += gdal.VSIFWriteL('x' * 5000, 1, 5000, f)
        if ret != 40000:
            gdal.VSIFCloseL(f)
            pytest.fail(ret)
        gdal.VSIFCloseL(f)


###############################################################################
# Test Unlink()


def test_vsiswift_fake_unlink():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    # Success
    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something/foo/bar', 206,
                {'Content-Range': 'bytes 0-0/1'}, 'x')
    handler.add('DELETE', '/v1/AUTH_something/foo/bar', 202, {'Connection': 'close'})
    with webserver.install_http_handler(handler):
        ret = gdal.Unlink('/vsiswift/foo/bar')
    assert ret == 0

    # Failure
    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something/foo/bar', 206,
                {'Content-Range': 'bytes 0-0/1'}, 'x')
    handler.add('DELETE', '/v1/AUTH_something/foo/bar', 400, {'Connection': 'close'})
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ret = gdal.Unlink('/vsiswift/foo/bar')
    assert ret == -1

###############################################################################
# Test Mkdir() / Rmdir()


def test_vsiswift_fake_mkdir_rmdir():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    # Invalid name
    ret = gdal.Mkdir('/vsiswift', 0)
    assert ret != 0

    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something/foo/dir/', 404, {'Connection': 'close'})
    handler.add('GET', '/v1/AUTH_something/foo?delimiter=%2F&limit=10000', 200, {'Connection': 'close'}, "[]")
    handler.add('PUT', '/v1/AUTH_something/foo/dir/', 201)
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir('/vsiswift/foo/dir', 0)
    assert ret == 0

    # Try creating already existing directory
    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something/foo/dir/', 404, {'Connection': 'close'})
    handler.add('GET', '/v1/AUTH_something/foo?delimiter=%2F&limit=10000',
                200,
                {'Connection': 'close', 'Content-type': 'application/json'},
                """[ { "subdir": "dir/" } ]""")
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir('/vsiswift/foo/dir', 0)
    assert ret != 0

    # Invalid name
    ret = gdal.Rmdir('/vsiswift')
    assert ret != 0

    gdal.VSICurlClearCache()

    # Not a directory
    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something/foo/it_is_a_file/', 404)
    handler.add('GET', '/v1/AUTH_something/foo?delimiter=%2F&limit=10000',
                200,
                {'Connection': 'close', 'Content-type': 'application/json'},
                """[ { "name": "it_is_a_file/", "bytes": 0, "last_modified": "1970-01-01T00:00:01" } ]""")
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir('/vsiswift/foo/it_is_a_file')
    assert ret != 0

    # Valid
    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something/foo/dir/', 200)
    handler.add('GET', '/v1/AUTH_something/foo?delimiter=%2F&limit=101&prefix=dir%2F',
                200,
                {'Connection': 'close', 'Content-type': 'application/json'},
                """[]
                """)
    handler.add('DELETE', '/v1/AUTH_something/foo/dir/', 204)
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir('/vsiswift/foo/dir')
    assert ret == 0

    # Try deleting already deleted directory
    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something/foo/dir/', 404)
    handler.add('GET', '/v1/AUTH_something/foo?delimiter=%2F&limit=10000', 200)
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir('/vsiswift/foo/dir')
    assert ret != 0

    gdal.VSICurlClearCache()

    # Try deleting non-empty directory
    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something/foo/dir_nonempty/', 404)
    handler.add('GET', '/v1/AUTH_something/foo?delimiter=%2F&limit=10000',
                200,
                {'Connection': 'close', 'Content-type': 'application/json'},
                """[ { "subdir": "dir_nonempty/" } ]""")
    handler.add('GET', '/v1/AUTH_something/foo?delimiter=%2F&limit=101&prefix=dir_nonempty%2F',
                200,
                {'Connection': 'close', 'Content-type': 'application/json'},
                """[ { "name": "dir_nonempty/some_file", "bytes": 0, "last_modified": "1970-01-01T00:00:01" } ]""")
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir('/vsiswift/foo/dir_nonempty')
    assert ret != 0

###############################################################################


def test_vsiswift_stop_webserver():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    # Clearcache needed to close all connections, since the Python server
    # can only handle one connection at a time
    gdal.VSICurlClearCache()

    webserver.server_stop(gdaltest.webserver_process, gdaltest.webserver_port)

###############################################################################
# Nominal cases (require valid credentials)


def test_vsiswift_extra_1():

    if not gdaltest.built_against_curl():
        pytest.skip()

    swift_resource = gdal.GetConfigOption('SWIFT_RESOURCE')
    if swift_resource is None:
        pytest.skip('Missing SWIFT_RESOURCE')

    if '/' not in swift_resource:
        path = '/vsiswift/' + swift_resource
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

        unique_id = 'vsiswift_test'
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

        f = gdal.VSIFOpenL(subpath + '/test.txt', 'wb')
        assert f is not None
        gdal.VSIFWriteL('hello', 1, 5, f)
        gdal.VSIFCloseL(f)

        ret = gdal.Rmdir(subpath)
        assert ret != 0, \
            ('Rmdir(%s) on non empty directory should return an error' % subpath)

        f = gdal.VSIFOpenL(subpath + '/test.txt', 'rb')
        assert f is not None
        data = gdal.VSIFReadL(1, 5, f).decode('utf-8')
        assert data == 'hello'
        gdal.VSIFCloseL(f)

        ret = gdal.Unlink(subpath + '/test.txt')
        assert ret >= 0, \
            ('Unlink(%s) should not return an error' % (subpath + '/test.txt'))

        ret = gdal.Rmdir(subpath)
        assert ret >= 0, ('Rmdir(%s) should not return an error' % subpath)

        return

    f = open_for_read('/vsiswift/' + swift_resource)
    assert f is not None
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    assert len(ret) == 1

    # Same with /vsiswift_streaming/
    f = open_for_read('/vsiswift_streaming/' + swift_resource)
    assert f is not None
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    assert len(ret) == 1

    # Invalid resource
    gdal.ErrorReset()
    f = open_for_read('/vsiswift_streaming/' + swift_resource + '/invalid_resource.baz')
    assert f is None, gdal.VSIGetLastErrorMsg()

###############################################################################


def test_vsiswift_cleanup():

    for var in gdaltest.swift_vars:
        gdal.SetConfigOption(var, gdaltest.swift_vars[var])
