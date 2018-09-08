#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsiwebhdfs
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


def vsiwebhdfs_init():

    gdaltest.webhdfs_vars = {}
    for var in ('WEBHDFS_USERNAME', 'WEBHDFS_DELEGATION'):
        gdaltest.webhdfs_vars[var] = gdal.GetConfigOption(var)
        if gdaltest.webhdfs_vars[var] is not None:
            gdal.SetConfigOption(var, "")

    return 'success'

###############################################################################


def vsiwebhdfs_start_webserver():

    gdaltest.webserver_process = None
    gdaltest.webserver_port = 0

    if not gdaltest.built_against_curl():
        return 'skip'

    (gdaltest.webserver_process, gdaltest.webserver_port) = webserver.launch(
        handler=webserver.DispatcherHttpHandler)
    if gdaltest.webserver_port == 0:
        return 'skip'

    gdaltest.webhdfs_base_connection = '/vsiwebhdfs/http://localhost:' + \
        str(gdaltest.webserver_port) + '/webhdfs/v1'
    gdaltest.webhdfs_redirected_url = 'http://non_existing_host:' + \
        str(gdaltest.webserver_port) + '/redirected'

    return 'success'

###############################################################################
# Test VSIFOpenL()


def vsiwebhdfs_open():

    if gdaltest.webserver_port == 0:
        return 'skip'

    gdal.VSICurlClearCache()

    # Download without redirect (not nominal)
    handler = webserver.SequentialHandler()
    handler.add('GET', '/webhdfs/v1/foo/bar?op=OPEN&offset=9999990784&length=16384', 200,
                {}, '0123456789data')
    with webserver.install_http_handler(handler):
        f = open_for_read(gdaltest.webhdfs_base_connection + '/foo/bar')
        if f is None:
            gdaltest.post_reason('fail')
            return 'fail'
        gdal.VSIFSeekL(f, 9999990784 + 10, 0)
        if gdal.VSIFReadL(1, 4, f).decode('ascii') != 'data':
            gdaltest.post_reason('fail')
            return 'fail'
        gdal.VSIFCloseL(f)

    # Download with redirect (nominal) and permissions

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/webhdfs/v1/foo/bar?op=OPEN&offset=0&length=16384&user.name=root&delegation=token', 307,
                {'Location': gdaltest.webhdfs_redirected_url + '/webhdfs/v1/foo/bar?op=OPEN&offset=0&length=16384'})
    handler.add('GET', '/redirected/webhdfs/v1/foo/bar?op=OPEN&offset=0&length=16384', 200,
                {}, 'yeah')
    with gdaltest.config_options({'WEBHDFS_USERNAME': 'root',
                                  'WEBHDFS_DELEGATION': 'token',
                                  'WEBHDFS_DATANODE_HOST': 'localhost'}):
        with webserver.install_http_handler(handler):
            f = open_for_read(gdaltest.webhdfs_base_connection + '/foo/bar')
            if f is None:
                gdaltest.post_reason('fail')
                return 'fail'
            if gdal.VSIFReadL(1, 4, f).decode('ascii') != 'yeah':
                gdaltest.post_reason('fail')
                return 'fail'
            gdal.VSIFCloseL(f)

    # Test error

    gdal.VSICurlClearCache()

    f = open_for_read(gdaltest.webhdfs_base_connection + '/foo/bar')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'

    handler = webserver.SequentialHandler()
    handler.add('GET', '/webhdfs/v1/foo/bar?op=OPEN&offset=0&length=16384', 404)
    with webserver.install_http_handler(handler):
        if len(gdal.VSIFReadL(1, 4, f)) != 0:
            gdaltest.post_reason('fail')
            return 'fail'

    # Retry: shouldn't not cause network access
    if len(gdal.VSIFReadL(1, 4, f)) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.VSIFCloseL(f)

    return 'success'

###############################################################################
# Test VSIStatL()


def vsiwebhdfs_stat():

    if gdaltest.webserver_port == 0:
        return 'skip'

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/webhdfs/v1/foo/bar?op=GETFILESTATUS', 200,
                {}, '{"FileStatus":{"type":"FILE","length":1000000}}')
    with webserver.install_http_handler(handler):
        stat_res = gdal.VSIStatL(gdaltest.webhdfs_base_connection + '/foo/bar')
    if stat_res is None or stat_res.size != 1000000:
        gdaltest.post_reason('fail')
        if stat_res is not None:
            print(stat_res.size)
        else:
            print(stat_res)
        return 'fail'

    # Test caching
    stat_res = gdal.VSIStatL(gdaltest.webhdfs_base_connection + '/foo/bar')
    if stat_res.size != 1000000:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test missing file
    handler = webserver.SequentialHandler()
    handler.add('GET', '/webhdfs/v1/unexisting?op=GETFILESTATUS', 404, {},
                '{"RemoteException":{"exception":"FileNotFoundException","javaClassName":"java.io.FileNotFoundException","message":"File does not exist: /unexisting"}}')
    with webserver.install_http_handler(handler):
        stat_res = gdal.VSIStatL(
            gdaltest.webhdfs_base_connection + '/unexisting')
    if stat_res is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test ReadDir()


def vsiwebhdfs_readdir():

    if gdaltest.webserver_port == 0:
        return 'skip'

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/webhdfs/v1/foo/?op=LISTSTATUS', 200,
                {}, '{"FileStatuses":{"FileStatus":[{"type":"FILE","modificationTime":1000,"pathSuffix":"bar.baz","length":123456},{"type":"DIRECTORY","pathSuffix":"mysubdir","length":0}]}}')
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir(gdaltest.webhdfs_base_connection + '/foo')
    if dir_contents != ['bar.baz', 'mysubdir']:
        gdaltest.post_reason('fail')
        print(dir_contents)
        return 'fail'
    stat_res = gdal.VSIStatL(gdaltest.webhdfs_base_connection + '/foo/bar.baz')
    if stat_res.size != 123456:
        gdaltest.post_reason('fail')
        print(stat_res.size)
        return 'fail'
    if stat_res.mtime != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # ReadDir on something known to be a file shouldn't cause network access
    dir_contents = gdal.ReadDir(
        gdaltest.webhdfs_base_connection + '/foo/bar.baz')
    if dir_contents is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test error on ReadDir()
    handler = webserver.SequentialHandler()
    handler.add('GET', '/webhdfs/v1foo/error_test/?op=LISTSTATUS', 404)
    with webserver.install_http_handler(handler):
        dir_contents = gdal.ReadDir(
            gdaltest.webhdfs_base_connection + 'foo/error_test/')
    if dir_contents is not None:
        gdaltest.post_reason('fail')
        print(dir_contents)
        return 'fail'

    return 'success'

###############################################################################
# Test write


def vsiwebhdfs_write():

    if gdaltest.webserver_port == 0:
        return 'skip'

    gdal.VSICurlClearCache()

    # Zero length file
    handler = webserver.SequentialHandler()
    with webserver.install_http_handler(handler):
        # Missing required config options
        with gdaltest.error_handler():
            f = gdal.VSIFOpenL(
                gdaltest.webhdfs_base_connection + '/foo/bar', 'wb')
        if f is not None:
            gdaltest.post_reason('fail')
            return 'fail'

    handler = webserver.SequentialHandler()
    handler.add('PUT', '/webhdfs/v1/foo/bar?op=CREATE&overwrite=true&user.name=root', 307,
                {'Location': gdaltest.webhdfs_redirected_url + '/webhdfs/v1/foo/bar?op=CREATE&overwrite=true&user.name=root'}, )
    handler.add(
        'PUT', '/redirected/webhdfs/v1/foo/bar?op=CREATE&overwrite=true&user.name=root', 201)

    with gdaltest.config_options({'WEBHDFS_USERNAME': 'root', 'WEBHDFS_DATANODE_HOST': 'localhost'}):
        with webserver.install_http_handler(handler):
            f = gdal.VSIFOpenL(
                gdaltest.webhdfs_base_connection + '/foo/bar', 'wb')
            if f is None:
                gdaltest.post_reason('fail')
                return 'fail'
    if gdal.VSIFCloseL(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Non-empty file

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add('PUT', '/webhdfs/v1/foo/bar?op=CREATE&overwrite=true&user.name=root', 307,
                {'Location': gdaltest.webhdfs_redirected_url + '/webhdfs/v1/foo/bar?op=CREATE&overwrite=true&user.name=root'}, )
    handler.add(
        'PUT', '/redirected/webhdfs/v1/foo/bar?op=CREATE&overwrite=true&user.name=root', 201)

    with gdaltest.config_options({'WEBHDFS_USERNAME': 'root', 'WEBHDFS_DATANODE_HOST': 'localhost'}):
        with webserver.install_http_handler(handler):
            f = gdal.VSIFOpenL(
                gdaltest.webhdfs_base_connection + '/foo/bar', 'wb')
            if f is None:
                gdaltest.post_reason('fail')
                return 'fail'

    if gdal.VSIFWriteL('foobar', 1, 6, f) != 6:
        gdaltest.post_reason('fail')
        return 'fail'

    handler = webserver.SequentialHandler()

    def method(request):
        h = request.headers
        if 'Content-Length' in h and h['Content-Length'] != 0:
            sys.stderr.write('Bad headers: %s\n' % str(request.headers))
            request.send_response(403)
            request.send_header('Content-Length', 0)
            request.end_headers()

        request.protocol_version = 'HTTP/1.1'
        request.send_response(307)
        request.send_header('Location', gdaltest.webhdfs_redirected_url +
                            '/webhdfs/v1/foo/bar?op=APPEND&user.name=root')
        request.end_headers()

    handler.add('POST', '/webhdfs/v1/foo/bar?op=APPEND&user.name=root',
                307, custom_method=method)
    handler.add('POST', '/redirected/webhdfs/v1/foo/bar?op=APPEND&user.name=root',
                200, expected_body='foobar'.encode('ascii'))

    with webserver.install_http_handler(handler):
        if gdal.VSIFCloseL(f) != 0:
            gdaltest.post_reason('fail')
            return 'fail'

    # Errors during file creation

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add(
        'PUT', '/webhdfs/v1/foo/bar?op=CREATE&overwrite=true&user.name=root', 404)
    with gdaltest.config_options({'WEBHDFS_USERNAME': 'root', 'WEBHDFS_DATANODE_HOST': 'localhost'}):
        with webserver.install_http_handler(handler):
            with gdaltest.error_handler():
                f = gdal.VSIFOpenL(
                    gdaltest.webhdfs_base_connection + '/foo/bar', 'wb')
                if f is not None:
                    gdaltest.post_reason('fail')
                    return 'fail'

    handler = webserver.SequentialHandler()
    handler.add('PUT', '/webhdfs/v1/foo/bar?op=CREATE&overwrite=true&user.name=root', 307,
                {'Location': gdaltest.webhdfs_redirected_url + '/webhdfs/v1/foo/bar?op=CREATE&overwrite=true&user.name=root'}, )
    with gdaltest.config_options({'WEBHDFS_USERNAME': 'root'}):
        with webserver.install_http_handler(handler):
            with gdaltest.error_handler():
                f = gdal.VSIFOpenL(
                    gdaltest.webhdfs_base_connection + '/foo/bar', 'wb')
                if f is not None:
                    gdaltest.post_reason('fail')
                    return 'fail'

    # Errors during POST

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add('PUT', '/webhdfs/v1/foo/bar?op=CREATE&overwrite=true&user.name=root', 307,
                {'Location': gdaltest.webhdfs_redirected_url + '/webhdfs/v1/foo/bar?op=CREATE&overwrite=true&user.name=root'}, )
    handler.add(
        'PUT', '/redirected/webhdfs/v1/foo/bar?op=CREATE&overwrite=true&user.name=root', 201)

    with gdaltest.config_options({'WEBHDFS_USERNAME': 'root', 'WEBHDFS_DATANODE_HOST': 'localhost'}):
        with webserver.install_http_handler(handler):
            f = gdal.VSIFOpenL(
                gdaltest.webhdfs_base_connection + '/foo/bar', 'wb')
            if f is None:
                gdaltest.post_reason('fail')
                return 'fail'

    if gdal.VSIFWriteL('foobar', 1, 6, f) != 6:
        gdaltest.post_reason('fail')
        return 'fail'

    handler = webserver.SequentialHandler()
    handler.add('POST', '/webhdfs/v1/foo/bar?op=APPEND&user.name=root', 307,
                {'Location': gdaltest.webhdfs_redirected_url + '/webhdfs/v1/foo/bar?op=APPEND&user.name=root'})
    handler.add(
        'POST', '/redirected/webhdfs/v1/foo/bar?op=APPEND&user.name=root', 400)

    with gdaltest.error_handler():
        with webserver.install_http_handler(handler):
            if gdal.VSIFCloseL(f) == 0:
                gdaltest.post_reason('fail')
                return 'fail'

    return 'success'

###############################################################################
# Test Unlink()


def vsiwebhdfs_unlink():

    if gdaltest.webserver_port == 0:
        return 'skip'

    gdal.VSICurlClearCache()

    # Success
    handler = webserver.SequentialHandler()
    handler.add('DELETE', '/webhdfs/v1/foo/bar?op=DELETE', 200,
                {}, '{"boolean":true}')
    with webserver.install_http_handler(handler):
        ret = gdal.Unlink(gdaltest.webhdfs_base_connection + '/foo/bar')
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.VSICurlClearCache()


    # With permissions

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add('DELETE', '/webhdfs/v1/foo/bar?op=DELETE&user.name=root&delegation=token', 200,
                {}, '{"boolean":true}')
    with gdaltest.config_options({'WEBHDFS_USERNAME': 'root',
                                  'WEBHDFS_DELEGATION': 'token'}):
        with webserver.install_http_handler(handler):
            ret = gdal.Unlink(gdaltest.webhdfs_base_connection + '/foo/bar')
        if ret != 0:
            gdaltest.post_reason('fail')
            return 'fail'

    # Failure
    handler = webserver.SequentialHandler()
    handler.add('DELETE', '/webhdfs/v1/foo/bar?op=DELETE', 200,
                {}, '{"boolean":false}')
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ret = gdal.Unlink(gdaltest.webhdfs_base_connection + '/foo/bar')
    if ret != -1:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.VSICurlClearCache()

    # Failure
    handler = webserver.SequentialHandler()
    handler.add('DELETE', '/webhdfs/v1/foo/bar?op=DELETE', 404,
                {})
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ret = gdal.Unlink(gdaltest.webhdfs_base_connection + '/foo/bar')
    if ret != -1:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test Mkdir() / Rmdir()


def vsiwebhdfs_mkdir_rmdir():

    if gdaltest.webserver_port == 0:
        return 'skip'

    gdal.VSICurlClearCache()

    # Invalid name
    ret = gdal.Mkdir('/vsiwebhdfs', 0)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Valid
    handler = webserver.SequentialHandler()
    handler.add('PUT', '/webhdfs/v1/foo/dir?op=MKDIRS', 200,
                {}, '{"boolean":true}')
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir(gdaltest.webhdfs_base_connection + '/foo/dir', 0)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Valid with all options
    handler = webserver.SequentialHandler()
    handler.add('PUT', '/webhdfs/v1/foo/dir?op=MKDIRS&user.name=root&delegation=token&permission=755', 200,
                {}, '{"boolean":true}')
    with gdaltest.config_options({'WEBHDFS_USERNAME': 'root', 'WEBHDFS_DELEGATION': 'token'}):
        with webserver.install_http_handler(handler):
            ret = gdal.Mkdir(gdaltest.webhdfs_base_connection +
                             '/foo/dir/', 493)  # 0755
        if ret != 0:
            gdaltest.post_reason('fail')
            return 'fail'

    # Error
    handler = webserver.SequentialHandler()
    handler.add('PUT', '/webhdfs/v1/foo/dir_error?op=MKDIRS', 404)
    with webserver.install_http_handler(handler):
        ret = gdal.Mkdir(gdaltest.webhdfs_base_connection +
                         '/foo/dir_error', 0)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Root name is invalid
    ret = gdal.Mkdir(gdaltest.webhdfs_base_connection + '/', 0)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Invalid name
    ret = gdal.Rmdir('/vsiwebhdfs')
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.VSICurlClearCache()

    # Valid
    handler = webserver.SequentialHandler()
    handler.add('DELETE', '/webhdfs/v1/foo/dir?op=DELETE', 200,
                {}, '{"boolean":true}')
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir(gdaltest.webhdfs_base_connection + '/foo/dir')
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Error
    handler = webserver.SequentialHandler()
    handler.add('DELETE', '/webhdfs/v1/foo/dir_error?op=DELETE', 404)
    with webserver.install_http_handler(handler):
        ret = gdal.Rmdir(gdaltest.webhdfs_base_connection + '/foo/dir_error')
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################


def vsiwebhdfs_stop_webserver():

    if gdaltest.webserver_port == 0:
        return 'skip'

    # Clearcache needed to close all connections, since the Python server
    # can only handle one connection at a time
    gdal.VSICurlClearCache()

    webserver.server_stop(gdaltest.webserver_process, gdaltest.webserver_port)

    return 'success'

###############################################################################
# Nominal cases (require valid credentials)


def vsiwebhdfs_extra_1():

    if not gdaltest.built_against_curl():
        return 'skip'

    webhdfs_url = gdal.GetConfigOption('WEBHDFS_URL')
    if webhdfs_url is None:
        print('Missing WEBHDFS_URL for running gdaltest_list_extra')
        return 'skip'

    if webhdfs_url.endswith('/webhdfs/v1') or webhdfs_url.endswith('/webhdfs/v1/'):
        path = '/vsiwebhdfs/' + webhdfs_url
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

        unique_id = 'vsiwebhdfs_test'
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

        #ret = gdal.Mkdir(subpath, 0)
        # if ret == 0:
        #    gdaltest.post_reason('fail')
        #    print('Mkdir(%s) repeated should return an error' % subpath)
        #    return 'fail'

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
            print('Unlink(%s) should not return an error' %
                  (subpath + '/test.txt'))
            return 'fail'

        ret = gdal.Rmdir(subpath)
        if ret < 0:
            gdaltest.post_reason('fail')
            print('Rmdir(%s) should not return an error' % subpath)
            return 'fail'

        return 'success'

    f = open_for_read('/vsiwebhdfs/' + webhdfs_url)
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ret = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    if len(ret) != 1:
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'

    return 'success'

###############################################################################


def vsiwebhdfs_cleanup():

    for var in gdaltest.webhdfs_vars:
        gdal.SetConfigOption(var, gdaltest.webhdfs_vars[var])

    return 'success'


gdaltest_list = [vsiwebhdfs_init,
                 vsiwebhdfs_start_webserver,
                 vsiwebhdfs_open,
                 vsiwebhdfs_stat,
                 vsiwebhdfs_readdir,
                 vsiwebhdfs_write,
                 vsiwebhdfs_unlink,
                 vsiwebhdfs_mkdir_rmdir,
                 vsiwebhdfs_stop_webserver,
                 vsiwebhdfs_cleanup]

gdaltest_list_extra = [vsiwebhdfs_extra_1]

if __name__ == '__main__':

    gdaltest.setup_run('vsiwebhdfs')

    if gdal.GetConfigOption('RUN_MANUAL_ONLY', None):
        gdaltest.run_tests(gdaltest_list_extra)
    else:
        gdaltest.run_tests(
            gdaltest_list + gdaltest_list_extra + [vsiwebhdfs_cleanup])

    sys.exit(gdaltest.summarize())
