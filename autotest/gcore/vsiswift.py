#!/usr/bin/env python
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

sys.path.append('../pymod')

import gdaltest
import webserver


def open_for_read(uri):
    """
    Opens a test file for reading.
    """
    return gdal.VSIFOpenExL(uri, 'rb', 1)

###############################################################################


def vsiswift_init():

    gdaltest.az_vars = {}
    for var in ('SWIFT_STORAGE_URL', 'SWIFT_AUTH_TOKEN',
                'SWIFT_AUTH_V1_URL', 'SWIFT_USER', 'SWIFT_KEY'):
        gdaltest.az_vars[var] = gdal.GetConfigOption(var)
        if gdaltest.az_vars[var] is not None:
            gdal.SetConfigOption(var, "")

    return 'success'

###############################################################################
# Error cases


def vsiswift_real_server_errors():

    if not gdaltest.built_against_curl():
        return 'skip'

    # Nothing set
    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsiswift/foo/bar')
    if f is not None or gdal.VSIGetLastErrorMsg().find('SWIFT_STORAGE_URL') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsiswift_streaming/foo/bar')
    if f is not None or gdal.VSIGetLastErrorMsg().find('SWIFT_STORAGE_URL') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    gdal.SetConfigOption('SWIFT_STORAGE_URL', 'http://0.0.0.0')

    # Missing SWIFT_AUTH_TOKEN
    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsiswift/foo/bar')
    if f is not None or gdal.VSIGetLastErrorMsg().find('SWIFT_AUTH_TOKEN') < 0:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    gdal.SetConfigOption('SWIFT_AUTH_TOKEN', 'SWIFT_AUTH_TOKEN')

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsiswift/foo/bar.baz')
    if f is not None:
        if f is not None:
            gdal.VSIFCloseL(f)
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = open_for_read('/vsiswift_streaming/foo/bar.baz')
    if f is not None:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################


def vsiswift_start_webserver():

    gdaltest.webserver_process = None
    gdaltest.webserver_port = 0

    if not gdaltest.built_against_curl():
        return 'skip'

    (gdaltest.webserver_process, gdaltest.webserver_port) = webserver.launch(handler=webserver.DispatcherHttpHandler)
    if gdaltest.webserver_port == 0:
        return 'skip'

    return 'success'

###############################################################################
# Test authentication with SWIFT_AUTH_V1_URL + SWIFT_USER + SWIFT_KEY


def vsiswift_fake_auth_v1_url():

    if gdaltest.webserver_port == 0:
        return 'skip'

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
        if f is None:
            gdaltest.post_reason('fail')
            return 'fail'
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    if data != 'foo':
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

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
        if f is None:
            gdaltest.post_reason('fail')
            return 'fail'
        data = gdal.VSIFReadL(1, 4, f).decode('ascii')
        gdal.VSIFCloseL(f)

    if data != 'bar':
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    return 'success'

###############################################################################
# Test authentication with SWIFT_STORAGE_URL + SWIFT_AUTH_TOKEN


def vsiswift_fake_auth_storage_url_and_auth_token():

    if gdaltest.webserver_port == 0:
        return 'skip'

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
        if f is None:
            gdaltest.post_reason('fail')
            return 'fail'
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
# Test VSIStatL()


def vsiswift_stat():

    if gdaltest.webserver_port == 0:
        return 'skip'

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something/foo/bar', 206,
                {'Content-Range': 'bytes 0-0/1000000'}, 'x')
    with webserver.install_http_handler(handler):
        stat_res = gdal.VSIStatL('/vsiswift/foo/bar')
        if stat_res is None or stat_res.size != 1000000:
            gdaltest.post_reason('fail')
            if stat_res is not None:
                print(stat_res.size)
            else:
                print(stat_res)
            return 'fail'

    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/v1/AUTH_something/foo/bar', 200, {'Content-Length': '1000000'})
    with webserver.install_http_handler(handler):
        stat_res = gdal.VSIStatL('/vsiswift_streaming/foo/bar')
        if stat_res is None or stat_res.size != 1000000:
            gdaltest.post_reason('fail')
            if stat_res is not None:
                print(stat_res.size)
            else:
                print(stat_res)
            return 'fail'

    # Test stat on container
    handler = webserver.SequentialHandler()
    # GET on the container URL returns something, but we must hack this back
    # to a directory
    handler.add('GET', '/v1/AUTH_something/foo', 200, {}, "blabla")
    with webserver.install_http_handler(handler):
        stat_res = gdal.VSIStatL('/vsiswift/foo')
        if stat_res is None or not stat.S_ISDIR(stat_res.mode):
            gdaltest.post_reason('fail')
            return 'fail'

    return 'success'

###############################################################################
# Test ReadDir()


def vsiswift_fake_readdir():

    if gdaltest.webserver_port == 0:
        return 'skip'

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
        if f is None:
            gdaltest.post_reason('fail')
            return 'fail'
        gdal.VSIFCloseL(f)

    dir_contents = gdal.ReadDir('/vsiswift/foo')
    if dir_contents != ['bar.baz', 'mysubdir']:
        gdaltest.post_reason('fail')
        print(dir_contents)
        return 'fail'
    stat_res = gdal.VSIStatL('/vsiswift/foo/bar.baz')
    if stat_res.size != 123456:
        gdaltest.post_reason('fail')
        print(stat_res.size)
        return 'fail'
    if stat_res.mtime != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # ReadDir on something known to be a file shouldn't cause network access
    dir_contents = gdal.ReadDir('/vsiswift/foo/bar.baz')
    if dir_contents is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test error on ReadDir()
    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something/foo?delimiter=%2F&limit=10000&prefix=error_test%2F', 500)
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir('/vsiswift/foo/error_test/')
    if dir_contents is not None:
        gdaltest.post_reason('fail')
        print(dir_contents)
        return 'fail'

    # List containers (empty result)
    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something', 200, {'Content-type': 'application/json'},
                """[]
        """)
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir('/vsiswift/')
    if dir_contents != ['.']:
        gdaltest.post_reason('fail')
        print(dir_contents)
        return 'fail'

    # List containers
    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something', 200, {'Content-type': 'application/json'},
                """[ { "name": "mycontainer1", "count": 0, "bytes": 0 },
             { "name": "mycontainer2", "count": 0, "bytes": 0}
           ] """)
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir('/vsiswift/')
    if dir_contents != ['mycontainer1', 'mycontainer2']:
        gdaltest.post_reason('fail')
        print(dir_contents)
        return 'fail'

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
    if dir_contents != ['foo', 'foo/']:
        gdaltest.post_reason('fail')
        print(dir_contents)
        return 'fail'

    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something/foo?delimiter=%2F&limit=10000', 200,
                {'Content-type': 'application/json'}, "[]")
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir('/vsiswift/foo/')
    if dir_contents != ['.']:
        gdaltest.post_reason('fail')
        print(dir_contents)
        return 'fail'

    return 'success'

###############################################################################
# Test write


def vsiswift_fake_write():

    if gdaltest.webserver_port == 0:
        return 'skip'

    gdal.VSICurlClearCache()

    # Test creation of BlockBob
    f = gdal.VSIFOpenL('/vsiswift/test_copy/file.bin', 'wb')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'

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
            gdaltest.post_reason('fail')
            print(ret)
            gdal.VSIFCloseL(f)
            return 'fail'
        gdal.VSIFCloseL(f)

    return 'success'

###############################################################################
# Test Unlink()


def vsiswift_fake_unlink():

    if gdaltest.webserver_port == 0:
        return 'skip'

    gdal.VSICurlClearCache()

    # Success
    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something/foo/bar', 206,
                {'Content-Range': 'bytes 0-0/1'}, 'x')
    handler.add('DELETE', '/v1/AUTH_something/foo/bar', 202, {'Connection': 'close'})
    with webserver.install_http_handler(handler):
        ret = gdal.Unlink('/vsiswift/foo/bar')
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Failure
    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something/foo/bar', 206,
                {'Content-Range': 'bytes 0-0/1'}, 'x')
    handler.add('DELETE', '/v1/AUTH_something/foo/bar', 400, {'Connection': 'close'})
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ret = gdal.Unlink('/vsiswift/foo/bar')
    if ret != -1:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test Mkdir() / Rmdir()


def vsiswift_fake_mkdir_rmdir():

    if gdaltest.webserver_port == 0:
        return 'skip'

    gdal.VSICurlClearCache()

    # Invalid name
    ret = gdal.Mkdir('/vsiswift', 0)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something/foo/dir/', 404, {'Connection': 'close'})
    handler.add('GET', '/v1/AUTH_something/foo?delimiter=%2F&limit=10000', 200, {'Connection': 'close'}, "[]")
    handler.add('PUT', '/v1/AUTH_something/foo/dir/', 201)
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir('/vsiswift/foo/dir', 0)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Try creating already existing directory
    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something/foo/dir/', 404, {'Connection': 'close'})
    handler.add('GET', '/v1/AUTH_something/foo?delimiter=%2F&limit=10000',
                200,
                {'Connection': 'close', 'Content-type': 'application/json'},
                """[ { "subdir": "dir/" } ]""")
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir('/vsiswift/foo/dir', 0)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Invalid name
    ret = gdal.Rmdir('/vsiswift')
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

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
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Valid
    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something/foo/dir/', 200)
    handler.add('GET', '/v1/AUTH_something/foo?delimiter=%2F&limit=2&prefix=dir%2F',
                200,
                {'Connection': 'close', 'Content-type': 'application/json'},
                """[]
                """)
    handler.add('DELETE', '/v1/AUTH_something/foo/dir/', 204)
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir('/vsiswift/foo/dir')
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Try deleting already deleted directory
    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something/foo/dir/', 404)
    handler.add('GET', '/v1/AUTH_something/foo?delimiter=%2F&limit=10000', 200)
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir('/vsiswift/foo/dir')
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.VSICurlClearCache()

    # Try deleting non-empty directory
    handler = webserver.SequentialHandler()
    handler.add('GET', '/v1/AUTH_something/foo/dir_nonempty/', 404)
    handler.add('GET', '/v1/AUTH_something/foo?delimiter=%2F&limit=10000',
                200,
                {'Connection': 'close', 'Content-type': 'application/json'},
                """[ { "subdir": "dir_nonempty/" } ]""")
    handler.add('GET', '/v1/AUTH_something/foo?delimiter=%2F&limit=2&prefix=dir_nonempty%2F',
                200,
                {'Connection': 'close', 'Content-type': 'application/json'},
                """[ { "name": "dir_nonempty/some_file", "bytes": 0, "last_modified": "1970-01-01T00:00:01" } ]""")
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir('/vsiswift/foo/dir_nonempty')
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################


def vsiswift_stop_webserver():

    if gdaltest.webserver_port == 0:
        return 'skip'

    # Clearcache needed to close all connections, since the Python server
    # can only handle one connection at a time
    gdal.VSICurlClearCache()

    webserver.server_stop(gdaltest.webserver_process, gdaltest.webserver_port)

    return 'success'

###############################################################################
# Nominal cases (require valid credentials)


def vsiswift_extra_1():

    if not gdaltest.built_against_curl():
        return 'skip'

    swift_resource = gdal.GetConfigOption('SWIFT_RESOURCE')
    if swift_resource is None:
        print('Missing SWIFT_RESOURCE for running gdaltest_list_extra')
        return 'skip'

    if swift_resource.find('/') < 0:
        path = '/vsiswift/' + swift_resource
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

        unique_id = 'vsiswift_test'
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

    f = open_for_read('/vsiswift/' + swift_resource)
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    if len(ret) != 1:
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'

    # Same with /vsiswift_streaming/
    f = open_for_read('/vsiswift_streaming/' + swift_resource)
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    if len(ret) != 1:
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'

    # Invalid resource
    gdal.ErrorReset()
    f = open_for_read('/vsiswift_streaming/' + swift_resource + '/invalid_resource.baz')
    if f is not None:
        gdaltest.post_reason('fail')
        print(gdal.VSIGetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################


def vsiswift_cleanup():

    for var in gdaltest.az_vars:
        gdal.SetConfigOption(var, gdaltest.az_vars[var])

    return 'success'


gdaltest_list = [vsiswift_init,
                 vsiswift_real_server_errors,
                 vsiswift_start_webserver,
                 vsiswift_fake_auth_v1_url,
                 vsiswift_fake_auth_storage_url_and_auth_token,
                 vsiswift_stat,
                 vsiswift_fake_readdir,
                 vsiswift_fake_write,
                 vsiswift_fake_unlink,
                 vsiswift_fake_mkdir_rmdir,
                 vsiswift_stop_webserver,
                 vsiswift_cleanup]

# gdaltest_list = [ vsiswift_init, vsiswift_start_webserver, vsiswift_fake_mkdir_rmdir, vsiswift_stop_webserver, vsiswift_cleanup ]

gdaltest_list_extra = [vsiswift_extra_1]

if __name__ == '__main__':

    gdaltest.setup_run('vsiswift')

    if gdal.GetConfigOption('RUN_MANUAL_ONLY', None):
        gdaltest.run_tests(gdaltest_list_extra)
    else:
        gdaltest.run_tests(gdaltest_list + gdaltest_list_extra + [vsiswift_cleanup])

    gdaltest.summarize()
