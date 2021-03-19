#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsicurl
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
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

import time
from osgeo import gdal
from osgeo import ogr


import gdaltest
import webserver
import pytest


###############################################################################
#

def test_vsicurl_1():
    if not gdaltest.run_slow_tests():
        pytest.skip()

    if not gdaltest.built_against_curl():
        pytest.skip()

    ds = ogr.Open('/vsizip/vsicurl/http://publicfiles.dep.state.fl.us/dear/BWR_GIS/2007NWFLULC/NWFWMD2007LULC.zip')
    assert ds is not None

###############################################################################
#


def vsicurl_2():
    if not gdaltest.run_slow_tests():
        pytest.skip()

    if not gdaltest.built_against_curl():
        pytest.skip()

    ds = gdal.Open('/vsizip//vsicurl/http://eros.usgs.gov/archive/nslrsda/GeoTowns/HongKong/srtm/n22e113.zip/n22e113.bil')
    assert ds is not None

###############################################################################
# This server doesn't support range downloading


def vsicurl_3():
    if not gdaltest.run_slow_tests():
        pytest.skip()

    if not gdaltest.built_against_curl():
        pytest.skip()

    ds = ogr.Open('/vsizip/vsicurl/http://www.iucnredlist.org/spatial-data/MAMMALS_TERRESTRIAL.zip')
    assert ds is None

###############################################################################
# This server doesn't support range downloading


def test_vsicurl_4():
    if not gdaltest.run_slow_tests():
        pytest.skip()

    if not gdaltest.built_against_curl():
        pytest.skip()

    ds = ogr.Open('/vsizip/vsicurl/http://lelserver.env.duke.edu:8080/LandscapeTools/export/49/Downloads/1_Habitats.zip')
    assert ds is None

###############################################################################
# Test URL unescaping when reading HTTP file list


def test_vsicurl_5():
    if not gdaltest.run_slow_tests():
        pytest.skip()

    if not gdaltest.built_against_curl():
        pytest.skip()

    ds = gdal.Open('/vsicurl/http://dds.cr.usgs.gov/srtm/SRTM_image_sample/picture%20examples/N34W119_DEM.tif')
    assert ds is not None

###############################################################################
# Test with FTP server that doesn't support EPSV command


def vsicurl_6_disabled():
    if not gdaltest.run_slow_tests():
        pytest.skip()

    if not gdaltest.built_against_curl():
        pytest.skip()

    fl = gdal.ReadDir('/vsicurl/ftp://ftp2.cits.rncan.gc.ca/pub/cantopo/250k_tif')
    assert fl


###############################################################################
# Test Microsoft-IIS/6.0 listing

def test_vsicurl_7():
    if not gdaltest.run_slow_tests():
        pytest.skip()

    if not gdaltest.built_against_curl():
        pytest.skip()

    fl = gdal.ReadDir('/vsicurl/http://ortho.linz.govt.nz/tifs/2005_06')
    assert fl

###############################################################################
# Test interleaved reading between 2 datasets


def vsicurl_8():
    if not gdaltest.run_slow_tests():
        pytest.skip()

    if not gdaltest.built_against_curl():
        pytest.skip()

    ds1 = gdal.Open('/vsigzip//vsicurl/http://dds.cr.usgs.gov/pub/data/DEM/250/notavail/C/chipicoten-w.gz')
    gdal.Open('/vsizip//vsicurl/http://edcftp.cr.usgs.gov/pub/data/landcover/files/2009/biso/gokn09b_dnbr.zip/nps-serotnbsp-9001-20090321_rd.tif')
    cs = ds1.GetRasterBand(1).Checksum()
    assert cs == 61342

###############################################################################
# Test reading a file with Chinese characters, but the HTTP file listing
# returns escaped sequences instead of the Chinese characters.


def test_vsicurl_9():
    if not gdaltest.run_slow_tests():
        pytest.skip()

    if not gdaltest.built_against_curl():
        pytest.skip()

    ds = gdal.Open('/vsicurl/http://download.osgeo.org/gdal/data/gtiff/'
                   'xx\u4E2D\u6587.\u4E2D\u6587')
    assert ds is not None

###############################################################################
# Test reading a file with escaped Chinese characters.


def test_vsicurl_10():
    if not gdaltest.run_slow_tests():
        pytest.skip()

    if not gdaltest.built_against_curl():
        pytest.skip()

    ds = gdal.Open('/vsicurl/http://download.osgeo.org/gdal/data/gtiff/xx%E4%B8%AD%E6%96%87.%E4%B8%AD%E6%96%87')
    assert ds is not None

###############################################################################
# Test ReadDir() after reading a file on the same server


def test_vsicurl_11():
    if not gdaltest.run_slow_tests():
        pytest.skip()

    if not gdaltest.built_against_curl():
        pytest.skip()

    f = gdal.VSIFOpenL('/vsicurl/http://download.osgeo.org/gdal/data/bmp/Bug2236.bmp', 'rb')
    if f is None:
        pytest.skip()
    gdal.VSIFSeekL(f, 1000000, 0)
    gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    filelist = gdal.ReadDir('/vsicurl/http://download.osgeo.org/gdal/data/gtiff')
    assert filelist is not None and filelist

###############################################################################


def test_vsicurl_start_webserver():

    gdaltest.webserver_process = None
    gdaltest.webserver_port = 0

    if not gdaltest.built_against_curl():
        pytest.skip()

    (gdaltest.webserver_process, gdaltest.webserver_port) = webserver.launch(handler=webserver.DispatcherHttpHandler)
    if gdaltest.webserver_port == 0:
        pytest.skip()

    
###############################################################################


def test_vsicurl_test_redirect():

    if gdaltest.is_travis_branch('trusty'):
        pytest.skip('Skipped on trusty branch, but should be investigated')

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/test_redirect/', 404)
    # Simulate a big time difference between server and local machine
    current_time = 1500

    def method(request):
        response = 'HTTP/1.1 302\r\n'
        response += 'Server: foo\r\n'
        response += 'Date: ' + time.strftime("%a, %d %b %Y %H:%M:%S GMT", time.gmtime(current_time)) + '\r\n'
        response += 'Location: %s\r\n' % ('http://localhost:%d/foo.s3.amazonaws.com/test_redirected/test.bin?Signature=foo&Expires=%d' % (gdaltest.webserver_port, current_time + 30))
        response += '\r\n'
        request.wfile.write(response.encode('ascii'))

    handler.add('HEAD', '/test_redirect/test.bin', custom_method=method)
    handler.add('HEAD', '/foo.s3.amazonaws.com/test_redirected/test.bin?Signature=foo&Expires=%d' % (current_time + 30), 403,
                {'Server': 'foo'}, '')

    def method(request):
        if 'Range' in request.headers:
            if request.headers['Range'] == 'bytes=0-16383':
                request.protocol_version = 'HTTP/1.1'
                request.send_response(200)
                request.send_header('Content-type', 'text/plain')
                request.send_header('Content-Range', 'bytes 0-16383/1000000')
                request.send_header('Content-Length', 16384)
                request.send_header('Connection', 'close')
                request.end_headers()
                request.wfile.write(('x' * 16384).encode('ascii'))
            elif request.headers['Range'] == 'bytes=16384-49151':
                # Test expiration of the signed URL
                request.protocol_version = 'HTTP/1.1'
                request.send_response(403)
                request.send_header('Content-Length', 0)
                request.end_headers()
            else:
                request.send_response(404)
                request.send_header('Content-Length', 0)
                request.end_headers()
        else:
            # After a failed attempt on a HEAD, the client should go there
            response = 'HTTP/1.1 200\r\n'
            response += 'Server: foo\r\n'
            response += 'Date: ' + time.strftime("%a, %d %b %Y %H:%M:%S GMT", time.gmtime(current_time)) + '\r\n'
            response += 'Content-type: text/plain\r\n'
            response += 'Content-Length: 1000000\r\n'
            response += 'Connection: close\r\n'
            response += '\r\n'
            request.wfile.write(response.encode('ascii'))

    handler.add('GET', '/foo.s3.amazonaws.com/test_redirected/test.bin?Signature=foo&Expires=%d' % (current_time + 30), custom_method=method)

    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL('/vsicurl/http://localhost:%d/test_redirect/test.bin' % gdaltest.webserver_port, 'rb')
    assert f is not None

    gdal.VSIFSeekL(f, 0, 2)
    if gdal.VSIFTellL(f) != 1000000:
        gdal.VSIFCloseL(f)
        pytest.fail(gdal.VSIFTellL(f))
    gdal.VSIFSeekL(f, 0, 0)

    handler = webserver.SequentialHandler()
    handler.add('GET', '/foo.s3.amazonaws.com/test_redirected/test.bin?Signature=foo&Expires=%d' % (current_time + 30), custom_method=method)
    handler.add('GET', '/foo.s3.amazonaws.com/test_redirected/test.bin?Signature=foo&Expires=%d' % (current_time + 30), custom_method=method)

    current_time = int(time.time())

    def method(request):
        # We should go there after expiration of the first signed URL
        if 'Range' in request.headers and \
                request.headers['Range'] == 'bytes=16384-49151':
            request.protocol_version = 'HTTP/1.1'
            request.send_response(302)
            # Return a new signed URL
            request.send_header('Location', 'http://localhost:%d/foo.s3.amazonaws.com/test_redirected2/test.bin?Signature=foo&Expires=%d' % (request.server.port, current_time + 30))
            request.send_header('Content-Length', 16384)
            request.end_headers()
            request.wfile.write(('x' * 16384).encode('ascii'))

    handler.add('GET', '/test_redirect/test.bin', custom_method=method)

    def method(request):
        # Second signed URL
        if 'Range' in request.headers and \
                request.headers['Range'] == 'bytes=16384-49151':
            request.protocol_version = 'HTTP/1.1'
            request.send_response(200)
            request.send_header('Content-type', 'text/plain')
            request.send_header('Content-Range', 'bytes 16384-16384/1000000')
            request.send_header('Content-Length', 1)
            request.end_headers()
            request.wfile.write('y'.encode('ascii'))

    handler.add('GET', '/foo.s3.amazonaws.com/test_redirected2/test.bin?Signature=foo&Expires=%d' % (current_time + 30), custom_method=method)

    with webserver.install_http_handler(handler):
        content = gdal.VSIFReadL(1, 16383, f).decode('ascii')
        if len(content) != 16383 or content[0] != 'x':
            gdal.VSIFCloseL(f)
            pytest.fail(content)
        content = gdal.VSIFReadL(1, 2, f).decode('ascii')
        if content != 'xy':
            gdal.VSIFCloseL(f)
            pytest.fail(content)

    gdal.VSIFCloseL(f)

###############################################################################
# TODO: better testing


def test_vsicurl_test_clear_cache():

    gdal.VSICurlClearCache()
    gdal.VSICurlClearCache()

###############################################################################


def test_vsicurl_test_retry():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/test_retry/', 404)
    handler.add('HEAD', '/test_retry/test.txt', 200, {'Content-Length': '3'})
    handler.add('GET', '/test_retry/test.txt', 502)
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL('/vsicurl/http://localhost:%d/test_retry/test.txt' % gdaltest.webserver_port, 'rb')
        data_len = 0
        if f:
            data_len = len(gdal.VSIFReadL(1, 1, f))
            gdal.VSIFCloseL(f)
        assert data_len == 0

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/test_retry/', 404)
    handler.add('HEAD', '/test_retry/test.txt', 200, {'Content-Length': '3'})
    handler.add('GET', '/test_retry/test.txt', 502)
    handler.add('GET', '/test_retry/test.txt', 429)
    handler.add('GET', '/test_retry/test.txt', 200, {}, 'foo')
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL('/vsicurl?max_retry=2&retry_delay=0.01&url=http://localhost:%d/test_retry/test.txt' % gdaltest.webserver_port, 'rb')
        assert f is not None
        gdal.ErrorReset()
        with gdaltest.error_handler():
            data = gdal.VSIFReadL(1, 3, f).decode('ascii')
        error_msg = gdal.GetLastErrorMsg()
        gdal.VSIFCloseL(f)
        assert data == 'foo'
        assert '429' in error_msg

    
###############################################################################


def test_vsicurl_test_fallback_from_head_to_get():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/test_fallback_from_head_to_get', 405)
    handler.add('GET', '/test_fallback_from_head_to_get', 200, {}, 'foo')
    with webserver.install_http_handler(handler):
        statres = gdal.VSIStatL('/vsicurl/http://localhost:%d/test_fallback_from_head_to_get' % gdaltest.webserver_port)
    assert statres.size == 3

    gdal.VSICurlClearCache()

###############################################################################


def test_vsicurl_test_parse_html_filelist_apache():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/mydir/', 200, {}, """<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 3.2 Final//EN">
<html>
 <head>
  <title>Index of /mydir</title>
 </head>
 <body>
<h1>Index of /mydir</h1>
<table><tr><th><img src="/icons/blank.gif" alt="[ICO]"></th><th><a href="?C=N;O=D">Name</a></th><th><a href="?C=M;O=A">Last modified</a></th><th><a href="?C=S;O=A">Size</a></th><th><a href="?C=D;O=A">Description</a></th></tr><tr><th colspan="5"><hr></th></tr>
<tr><td valign="top"><img src="/icons/back.gif" alt="[DIR]"></td><td><a href="/gdal/data/">Parent Directory</a></td><td>&nbsp;</td><td align="right">  - </td><td>&nbsp;</td></tr>
<tr><td valign="top"><img src="/icons/image2.gif" alt="[IMG]"></td><td><a href="foo.tif">foo.tif</a></td><td align="right">17-May-2010 12:26  </td><td align="right"> 90K</td><td>&nbsp;</td></tr>
<tr><td valign="top"><img src="/icons/image2.gif" alt="[IMG]"></td><td><a href="foo%20with%20space.tif">foo with space.tif</a></td><td align="right">15-Jan-2007 11:02  </td><td align="right">736 </td><td>&nbsp;</td></tr>
<tr><th colspan="5"><hr></th></tr>
</table>
</body></html>""")
    with webserver.install_http_handler(handler):
        fl = gdal.ReadDir('/vsicurl/http://localhost:%d/mydir' % gdaltest.webserver_port)
    assert fl == ['foo.tif', 'foo%20with%20space.tif']

    assert gdal.VSIStatL('/vsicurl/http://localhost:%d/mydir/foo%%20with%%20space.tif' % gdaltest.webserver_port, gdal.VSI_STAT_EXISTS_FLAG) is not None

    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/mydir/i_dont_exist', 404, {})
    with webserver.install_http_handler(handler):
        assert gdal.VSIStatL('/vsicurl/http://localhost:%d/mydir/i_dont_exist' % gdaltest.webserver_port, gdal.VSI_STAT_EXISTS_FLAG) is None


###############################################################################


def test_vsicurl_no_size_in_HEAD():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/test_vsicurl_no_size_in_HEAD.bin', 200, {}, add_content_length_header=False)
    handler.add('GET', '/test_vsicurl_no_size_in_HEAD.bin', 200, {}, 'X' * 10)
    with webserver.install_http_handler(handler):
        statres = gdal.VSIStatL('/vsicurl/http://localhost:%d/test_vsicurl_no_size_in_HEAD.bin' % gdaltest.webserver_port)
    assert statres.size == 10

###############################################################################


def test_vsicurl_stop_webserver():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    # Clearcache needed to close all connections, since the Python server
    # can only handle one connection at a time
    gdal.VSICurlClearCache()

    webserver.server_stop(gdaltest.webserver_process, gdaltest.webserver_port)



