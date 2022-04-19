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


@pytest.fixture()
def gs_test_config():
    # To avoid user credentials in ~/.boto
    # to mess up our tests
    options = {
        'CPL_GS_CREDENTIALS_FILE': '',
        'GS_OAUTH2_REFRESH_TOKEN': '',
        'GS_OAUTH2_CLIENT_EMAIL': '',
        'GS_OAUTH2_CLIENT_SECRET': '',
        'GS_OAUTH2_CLIENT_ID': '',
        'GOOGLE_APPLICATION_CREDENTIALS': '',
        'GS_USER_PROJECT': ''
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
        with gdaltest.config_options(
                {'CPL_GS_ENDPOINT': 'http://127.0.0.1:%d/' % webserver_port}):
            yield webserver_port
    finally:
        gdal.VSICurlClearCache()

        webserver.server_stop(webserver_process, webserver_port)

###############################################################################


def test_vsigs_init(gs_test_config):

    with gdaltest.config_options({'CPL_GCE_SKIP': 'YES',
                                  'CPL_GS_ENDPOINT': ''}):
        assert gdal.GetSignedURL('/vsigs/foo/bar') is None


###############################################################################
# Error cases


def test_vsigs_1(gs_test_config):

    if not gdaltest.built_against_curl():
        pytest.skip()

    gdal.VSICurlClearCache()

    with gdaltest.config_options({'CPL_GCE_SKIP': 'YES',
                                  'CPL_GS_ENDPOINT': ''}):
        # Invalid header filename
        gdal.ErrorReset()
        with gdaltest.config_option('GDAL_HTTP_HEADER_FILE', '/i_dont/exist.py'):
            with gdaltest.error_handler():
                f = open_for_read('/vsigs/foo/bar')
        if f is not None:
            gdal.VSIFCloseL(f)
            pytest.fail()
        last_err = gdal.GetLastErrorMsg()
        assert 'Cannot read' in last_err

        # Invalid content for header file
        with gdaltest.config_option('GDAL_HTTP_HEADER_FILE', 'vsigs.py'):
            f = open_for_read('/vsigs/foo/bar')
        if f is not None:
            gdal.VSIFCloseL(f)
            pytest.fail()

        # Missing GS_SECRET_ACCESS_KEY
        gdal.ErrorReset()
        with gdaltest.error_handler():
            f = open_for_read('/vsigs/foo/bar')
        assert f is None and gdal.VSIGetLastErrorMsg().find('GS_SECRET_ACCESS_KEY') >= 0

        gdal.ErrorReset()
        with gdaltest.error_handler():
            f = open_for_read('/vsigs_streaming/foo/bar')
        assert f is None and gdal.VSIGetLastErrorMsg().find('GS_SECRET_ACCESS_KEY') >= 0

        with gdaltest.config_option('GS_SECRET_ACCESS_KEY', 'GS_SECRET_ACCESS_KEY'):

            # Missing GS_ACCESS_KEY_ID
            gdal.ErrorReset()
            with gdaltest.error_handler():
                f = open_for_read('/vsigs/foo/bar')
            assert f is None and gdal.VSIGetLastErrorMsg().find('GS_ACCESS_KEY_ID') >= 0

            with gdaltest.config_option('GS_ACCESS_KEY_ID', 'GS_ACCESS_KEY_ID'):

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
# Test GS_NO_SIGN_REQUEST=YES


def test_vsigs_no_sign_request(gs_test_config):

    if not gdaltest.built_against_curl():
        pytest.skip()

    with gdaltest.config_options({'CPL_GS_ENDPOINT': ''}):

        object_key = 'gcp-public-data-landsat/LC08/01/044/034/LC08_L1GT_044034_20130330_20170310_01_T2/LC08_L1GT_044034_20130330_20170310_01_T2_B1.TIF'
        expected_url = 'https://storage.googleapis.com/' + object_key

        with gdaltest.config_option('GS_NO_SIGN_REQUEST', 'YES'):
            actual_url = gdal.GetActualURL('/vsigs/' + object_key)
            assert actual_url == expected_url

            actual_url = gdal.GetActualURL('/vsigs_streaming/' + object_key)
            assert actual_url == expected_url

            f = open_for_read('/vsigs/' + object_key)

        if f is None:
            if gdaltest.gdalurlopen(expected_url) is None:
                pytest.skip('cannot open URL')
            pytest.fail()
        gdal.VSIFCloseL(f)


###############################################################################
# Test with a fake Google Cloud Storage server


@pytest.mark.parametrize("use_config_options", [True, False])
def test_vsigs_2(gs_test_config, webserver_port, use_config_options):

    gdal.VSICurlClearCache()

    # header file
    gdal.FileFromMemBuffer('/vsimem/my_headers.txt', 'foo: bar')

    handler = webserver.SequentialHandler()
    handler.add('GET', '/gs_fake_bucket_http_header_file/resource', 200,
                {'Content-type': 'text/plain'}, 'Y',
                expected_headers={'foo': 'bar'})
    with webserver.install_http_handler(handler):

        with gdaltest.config_options(
            { 'GS_SECRET_ACCESS_KEY': 'GS_SECRET_ACCESS_KEY',
              'GS_ACCESS_KEY_ID': 'GS_ACCESS_KEY_ID',
              'GDAL_HTTP_HEADER_FILE': '/vsimem/my_headers.txt'}):
            f = open_for_read('/vsigs/gs_fake_bucket_http_header_file/resource')
            assert f is not None
            data = gdal.VSIFReadL(1, 1, f)
            gdal.VSIFCloseL(f)
            assert len(data) == 1
    gdal.Unlink('/vsimem/my_headers.txt')

    options = { 'GS_SECRET_ACCESS_KEY': 'GS_SECRET_ACCESS_KEY',
          'GS_ACCESS_KEY_ID': 'GS_ACCESS_KEY_ID',
          'CPL_GS_TIMESTAMP': 'my_timestamp'}
    with gdaltest.config_options(options) if use_config_options else gdaltest.credentials('/vsigs/', options):

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

        # Test GS_USER_PROJECT
        handler = webserver.SequentialHandler()
        handler.add('GET', '/gs_fake_bucket/resource_under_requester_pays', 200,
                    {'Content-type': 'text/plain'}, 'foo',
                    expected_headers={
                        'Authorization': 'GOOG1 GS_ACCESS_KEY_ID:q7i3g4lJD1c4OwiFtn/N/ePxxS0=',
                        'x-goog-user-project': 'my_project_id'})
        with webserver.install_http_handler(handler):
            with gdaltest.config_option('GS_USER_PROJECT', 'my_project_id'):
                f = open_for_read('/vsigs_streaming/gs_fake_bucket/resource_under_requester_pays')
                assert f is not None
                data = gdal.VSIFReadL(1, 4, f).decode('ascii')
                gdal.VSIFCloseL(f)

                assert data == 'foo'

###############################################################################
# Test ReadDir() with a fake Google Cloud Storage server


def test_vsigs_readdir(gs_test_config, webserver_port):

    with gdaltest.config_options(
        { 'GS_SECRET_ACCESS_KEY': 'GS_SECRET_ACCESS_KEY',
          'GS_ACCESS_KEY_ID': 'GS_ACCESS_KEY_ID' }):

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


def test_vsigs_write(gs_test_config, webserver_port):

    gdal.VSICurlClearCache()

    with gdaltest.config_options(
        { 'GS_SECRET_ACCESS_KEY': 'GS_SECRET_ACCESS_KEY',
          'GS_ACCESS_KEY_ID': 'GS_ACCESS_KEY_ID' }):

        with webserver.install_http_handler(webserver.SequentialHandler()):
            f = gdal.VSIFOpenExL('/vsigs/gs_fake_bucket3/another_file.bin', 'wb', 0,
                                 ['Content-Type=foo', 'Content-Encoding=bar', 'x-goog-storage-class=NEARLINE'])
            assert f is not None
            assert gdal.VSIFSeekL(f, gdal.VSIFTellL(f), 0) == 0
            assert gdal.VSIFSeekL(f, 0, 1) == 0
            assert gdal.VSIFSeekL(f, 0, 2) == 0
            assert gdal.VSIFWriteL('foo', 1, 3, f) == 3
            assert gdal.VSIFSeekL(f, gdal.VSIFTellL(f), 0) == 0
            assert gdal.VSIFWriteL('bar', 1, 3, f) == 3

        handler = webserver.SequentialHandler()

        def method(request):
            if request.headers['Content-Length'] != '6' or \
               request.headers['Content-Type'] != 'foo' or \
               request.headers['Content-Encoding'] != 'bar' or \
               request.headers['x-goog-storage-class'] != 'NEARLINE':
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

        handler.add('PUT', '/gs_fake_bucket3/another_file.bin', custom_method=method)

        gdal.ErrorReset()
        with webserver.install_http_handler(handler):
            gdal.VSIFCloseL(f)
        assert gdal.GetLastErrorMsg() == ''

###############################################################################
# Test rename

def test_vsigs_fake_rename(gs_test_config, webserver_port):

    gdal.VSICurlClearCache()

    with gdaltest.config_options(
        { 'GS_SECRET_ACCESS_KEY': 'GS_SECRET_ACCESS_KEY',
          'GS_ACCESS_KEY_ID': 'GS_ACCESS_KEY_ID' }):

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
# Test reading/writing ACL


def test_vsigs_acl(gs_test_config, webserver_port):

    gdal.VSICurlClearCache()

    with gdaltest.config_options(
        { 'GS_SECRET_ACCESS_KEY': 'GS_SECRET_ACCESS_KEY',
          'GS_ACCESS_KEY_ID': 'GS_ACCESS_KEY_ID' }):

        handler = webserver.SequentialHandler()
        handler.add('GET', '/test_metadata/foo.txt?acl', 200, {}, "<foo/>")
        with webserver.install_http_handler(handler):
            md = gdal.GetFileMetadata('/vsigs/test_metadata/foo.txt', 'ACL')
        assert 'XML' in md and md['XML'] == '<foo/>'

        # Error cases
        with gdaltest.error_handler():
            assert gdal.GetFileMetadata('/vsigs/test_metadata/foo.txt', 'UNSUPPORTED') == {}

        handler = webserver.SequentialHandler()
        handler.add('GET', '/test_metadata/foo.txt?acl', 400)
        with webserver.install_http_handler(handler):
            with gdaltest.error_handler():
                assert not gdal.GetFileMetadata('/vsigs/test_metadata/foo.txt', 'ACL')

        handler = webserver.SequentialHandler()
        handler.add('PUT', '/test_metadata/foo.txt?acl', 200, expected_body=b'<foo/>')
        with webserver.install_http_handler(handler):
            assert gdal.SetFileMetadata('/vsigs/test_metadata/foo.txt', {'XML': '<foo/>'}, 'ACL')

        # Error cases
        with gdaltest.error_handler():
            assert not gdal.SetFileMetadata('/vsigs/test_metadata/foo.txt', {}, 'UNSUPPORTED')
            assert not gdal.SetFileMetadata('/vsigs/test_metadata/foo.txt', {}, 'ACL')

        handler = webserver.SequentialHandler()
        handler.add('PUT', '/test_metadata/foo.txt?acl', 400)
        with webserver.install_http_handler(handler):
            with gdaltest.error_handler():
                assert not gdal.SetFileMetadata('/vsigs/test_metadata/foo.txt', {'XML': '<foo/>'}, 'ACL')

###############################################################################
# Test reading/writing HEADERS


def test_vsigs_headers(gs_test_config, webserver_port):

    gdal.VSICurlClearCache()

    with gdaltest.config_options(
        { 'GS_SECRET_ACCESS_KEY': 'GS_SECRET_ACCESS_KEY',
          'GS_ACCESS_KEY_ID': 'GS_ACCESS_KEY_ID' }):

        handler = webserver.SequentialHandler()
        handler.add('GET', '/test_metadata/foo.txt', 200, {'x-goog-meta-foo': 'bar'})
        with webserver.install_http_handler(handler):
            md = gdal.GetFileMetadata('/vsigs/test_metadata/foo.txt', 'HEADERS')
        assert 'x-goog-meta-foo' in md and md['x-goog-meta-foo'] == 'bar'

        # Write HEADERS domain
        handler = webserver.SequentialHandler()
        handler.add('PUT', '/test_metadata/foo.txt', 200, {},
                    expected_headers = {'x-goog-meta-foo': 'bar',
                                        'x-goog-metadata-directive': 'REPLACE',
                                        'x-goog-copy-source': '/test_metadata/foo.txt'})
        with webserver.install_http_handler(handler):
            assert gdal.SetFileMetadata('/vsigs/test_metadata/foo.txt', {'x-goog-meta-foo': 'bar'}, 'HEADERS')


###############################################################################
# Read credentials with OAuth2 refresh_token


def test_vsigs_read_credentials_refresh_token_default_gdal_app(gs_test_config, webserver_port):

    with gdaltest.config_options(
        { 'GOA2_AUTH_URL_TOKEN': 'http://localhost:%d/accounts.google.com/o/oauth2/token' % webserver_port,
          'GS_OAUTH2_REFRESH_TOKEN': 'REFRESH_TOKEN'}):

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

        # Test GS_USER_PROJECT
        handler = webserver.SequentialHandler()
        handler.add('GET', '/gs_fake_bucket/resource_under_requester_pays', 200,
                    {'Content-type': 'text/plain'}, 'foo',
                    expected_headers={'x-goog-user-project': 'my_project_id'})
        with webserver.install_http_handler(handler):
            with gdaltest.config_option('GS_USER_PROJECT', 'my_project_id'):
                f = open_for_read('/vsigs_streaming/gs_fake_bucket/resource_under_requester_pays')
                assert f is not None
                data = gdal.VSIFReadL(1, 4, f).decode('ascii')
                gdal.VSIFCloseL(f)

                assert data == 'foo'

###############################################################################
# Read credentials with OAuth2 refresh_token


def test_vsigs_read_credentials_refresh_token_custom_app(gs_test_config, webserver_port):

    with gdaltest.config_options(
        { 'GOA2_AUTH_URL_TOKEN': 'http://localhost:%d/accounts.google.com/o/oauth2/token' % webserver_port,
          'GS_OAUTH2_REFRESH_TOKEN': 'REFRESH_TOKEN',
          'GS_OAUTH2_CLIENT_ID': 'CLIENT_ID',
          'GS_OAUTH2_CLIENT_SECRET': 'CLIENT_SECRET' }):

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

###############################################################################
# Read credentials with OAuth2 service account


def test_vsigs_read_credentials_oauth2_service_account(gs_test_config, webserver_port):

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
    gdal.FileFromMemBuffer('/vsimem/pkey', key)

    with gdaltest.config_options(
        { 'GO2A_AUD': 'http://localhost:%d/oauth2/v4/token' % webserver_port,
          'GOA2_NOW': '123456',
          'GS_OAUTH2_CLIENT_EMAIL': 'CLIENT_EMAIL' }):

        for i in range(2):

            with gdaltest.config_options({'GS_OAUTH2_PRIVATE_KEY': key} if i == 0 else {'GS_OAUTH2_PRIVATE_KEY_FILE': '/vsimem/pkey'}):

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

                assert data == 'foo'

    gdal.Unlink('/vsimem/pkey')

###############################################################################
# Read credentials with OAuth2 service account through a json configuration file


def test_vsigs_read_credentials_oauth2_service_account_json_file(gs_test_config, webserver_port):

    gdal.FileFromMemBuffer('/vsimem/service_account.json', """{
  "private_key": "-----BEGIN PRIVATE KEY-----\nMIICeAIBADANBgkqhkiG9w0BAQEFAASCAmIwggJeAgEAAoGBAOlwJQLLDG1HeLrk\nVNcFR5Qptto/rJE5emRuy0YmkVINT4uHb1be7OOo44C2Ev8QPVtNHHS2XwCY5gTm\ni2RfIBLv+VDMoVQPqqE0LHb0WeqGmM5V1tHbmVnIkCcKMn3HpK30grccuBc472LQ\nDVkkGqIiGu0qLAQ89JP/r0LWWySRAgMBAAECgYAWjsS00WRBByAOh1P/dz4kfidy\nTabiXbiLDf3MqJtwX2Lpa8wBjAc+NKrPXEjXpv0W3ou6Z4kkqKHJpXGg4GRb4N5I\n2FA+7T1lA0FCXa7dT2jvgJLgpBepJu5b//tqFqORb4A4gMZw0CiPN3sUsWsSw5Hd\nDrRXwp6sarzG77kvZQJBAPgysAmmXIIp9j1hrFSkctk4GPkOzZ3bxKt2Nl4GFrb+\nbpKSon6OIhP1edrxTz1SMD1k5FiAAVUrMDKSarbh5osCQQDwxq4Tvf/HiYz79JBg\nWz5D51ySkbg01dOVgFW3eaYAdB6ta/o4vpHhnbrfl6VO9oUb3QR4hcrruwnDHsw3\n4mDTAkEA9FPZjbZSTOSH/cbgAXbdhE4/7zWOXj7Q7UVyob52r+/p46osAk9i5qj5\nKvnv2lrFGDrwutpP9YqNaMtP9/aLnwJBALLWf9n+GAv3qRZD0zEe1KLPKD1dqvrj\nj+LNjd1Xp+tSVK7vMs4PDoAMDg+hrZF3HetSQM3cYpqxNFEPgRRJOy0CQQDQlZHI\nyzpSgEiyx8O3EK1iTidvnLXbtWabvjZFfIE/0OhfBmN225MtKG3YLV2HoUvpajLq\ngwE6fxOLyJDxuWRf\n-----END PRIVATE KEY-----\n",
  "client_email": "CLIENT_EMAIL",
  "type": "service_account"
                           }""")

    with gdaltest.config_options(
        { 'GOOGLE_APPLICATION_CREDENTIALS': '/vsimem/service_account.json',
          'GO2A_AUD': 'http://localhost:%d/oauth2/v4/token' % webserver_port,
          'GOA2_NOW': '123456' }):

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

        gdal.Unlink('/vsimem/service_account.json')

        assert data == 'foo'

###############################################################################
# Read credentials with OAuth2 authorized user through a json configuration file


def test_vsigs_read_credentials_oauth2_authorized_user_json_file(gs_test_config, webserver_port):

    gdal.FileFromMemBuffer('/vsimem/authorized_user.json', """{
      "client_id": "CLIENT_ID",
      "client_secret": "CLIENT_SECRET",
      "refresh_token": "REFRESH_TOKEN",
      "type": "authorized_user"
    }""")

    with gdaltest.config_options(
        { 'GOOGLE_APPLICATION_CREDENTIALS': '/vsimem/authorized_user.json',
          'GOA2_AUTH_URL_TOKEN': 'http://localhost:%d/accounts.google.com/o/oauth2/token' % webserver_port }):

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

        gdal.Unlink('/vsimem/service_account.json')

###############################################################################
# Read credentials from simulated ~/.boto


def test_vsigs_read_credentials_file(gs_test_config, webserver_port):

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

    with gdaltest.config_options(
        { 'CPL_GS_TIMESTAMP': 'my_timestamp',
          'CPL_GS_CREDENTIALS_FILE': '/vsimem/.boto'}):

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

    gdal.Unlink('/vsimem/.boto')

###############################################################################
# Read credentials from simulated ~/.boto


def test_vsigs_read_credentials_file_refresh_token(gs_test_config, webserver_port):

    gdal.VSICurlClearCache()

    gdal.FileFromMemBuffer('/vsimem/.boto', """
[Credentials]
gs_oauth2_refresh_token = REFRESH_TOKEN
[OAuth2]
client_id = CLIENT_ID
client_secret = CLIENT_SECRET
""")

    with gdaltest.config_options(
            { 'CPL_GS_CREDENTIALS_FILE': '/vsimem/.boto',
              'GOA2_AUTH_URL_TOKEN':
                             'http://localhost:%d/accounts.google.com/o/oauth2/token' % webserver_port }):

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

        # Test UnlinkBatch()
        handler = webserver.SequentialHandler()
        handler.add('POST', '/batch/storage/v1', 200,
                    {'content-type': 'multipart/mixed; boundary=batch_nWfTDwb9aAhYucqUtdLRWUX93qsJaf3T'},
                    """--batch_phVs0DE8tHbyfvlYTZEeI5_snlh9XJR5
Content-Type: application/http
Content-ID: <response-1>

HTTP/1.1 204 No Content
Content-Length: 0


-batch_phVs0DE8tHbyfvlYTZEeI5_snlh9XJR5
Content-Type: application/http
Content-ID: <response-2>

HTTP/1.1 204 No Content
Content-Length: 0


--batch_phVs0DE8tHbyfvlYTZEeI5_snlh9XJR5--
""",
                    expected_body = b'--===============7330845974216740156==\r\nContent-Type: application/http\r\nContent-ID: <1>\r\n\r\n\r\nDELETE /storage/v1/b/unlink_batch/o/foo HTTP/1.1\r\n\r\n\r\n--===============7330845974216740156==\r\nContent-Type: application/http\r\nContent-ID: <2>\r\n\r\n\r\nDELETE /storage/v1/b/unlink_batch/o/bar%2Fbaz HTTP/1.1\r\n\r\n\r\n--===============7330845974216740156==--\r\n')
        handler.add('POST', '/batch/storage/v1', 200,
                    {'content-type': 'multipart/mixed; boundary=batch_nWfTDwb9aAhYucqUtdLRWUX93qsJaf3T'},
                    """--batch_phVs0DE8tHbyfvlYTZEeI5_snlh9XJR5
Content-Type: application/http
Content-ID: <response-3>

HTTP/1.1 204 No Content
Content-Length: 0


--batch_phVs0DE8tHbyfvlYTZEeI5_snlh9XJR5--
""",
                    expected_body = b'--===============7330845974216740156==\r\nContent-Type: application/http\r\nContent-ID: <3>\r\n\r\n\r\nDELETE /storage/v1/b/unlink_batch/o/baw HTTP/1.1\r\n\r\n\r\n--===============7330845974216740156==--\r\n')
        with gdaltest.config_option('CPL_VSIGS_UNLINK_BATCH_SIZE', '2'):
            with webserver.install_http_handler(handler):
                ret = gdal.UnlinkBatch(['/vsigs/unlink_batch/foo', '/vsigs/unlink_batch/bar/baz', '/vsigs/unlink_batch/baw'])
        assert ret

    gdal.Unlink('/vsimem/.boto')


###############################################################################
# Read credentials from simulated GCE instance
@pytest.mark.skipif(sys.platform not in ('linux', 'win32'), reason='Incorrect platform')
def test_vsigs_read_credentials_gce(gs_test_config, webserver_port):

    gdal.VSICurlClearCache()

    with gdaltest.config_options(
            { 'CPL_GS_CREDENTIALS_FILE': '',
              'CPL_GCE_CREDENTIALS_URL':
                             'http://localhost:%d/computeMetadata/v1/instance/service-accounts/default/token' % webserver_port,
              # Disable hypervisor related check to test if we are really on EC2
              'CPL_GCE_CHECK_LOCAL_FILES': 'NO'}):

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
        with gdaltest.config_option('CPL_GCE_CREDENTIALS_URL', ''):

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


###############################################################################
# Read credentials from simulated GCE instance with expiration of the
# cached credentials
@pytest.mark.skipif(sys.platform not in ('linux', 'win32'), reason='Incorrect platform')
def test_vsigs_read_credentials_gce_expiration(gs_test_config, webserver_port):

    gdal.VSICurlClearCache()

    with gdaltest.config_options(
            { 'CPL_GS_CREDENTIALS_FILE': '',
              'CPL_GCE_CREDENTIALS_URL':
                             'http://localhost:%d/computeMetadata/v1/instance/service-accounts/default/token' % webserver_port,
              # Disable hypervisor related check to test if we are really on EC2
              'CPL_GCE_CHECK_LOCAL_FILES': 'NO'}):

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

###############################################################################
# Nominal cases (require valid credentials)


def test_vsigs_extra_1():

    if not gdaltest.built_against_curl():
        pytest.skip()

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
        new_md = {}
        for key in md:
            new_md[key.lower()] = md[key]
        md = new_md
        assert 'content-type' in md
        assert md['content-type'] == 'foo'
        assert 'content-encoding' in md
        assert md['content-encoding'] == 'bar'

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
