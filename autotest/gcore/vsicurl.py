#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsicurl
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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
import time
from osgeo import gdal
from osgeo import ogr
from sys import version_info

sys.path.append('../pymod')

import gdaltest
import webserver


###############################################################################
#

def vsicurl_1():
    if not gdaltest.run_slow_tests():
        return 'skip'

    if not gdaltest.built_against_curl():
        return 'skip'

    ds = ogr.Open('/vsizip/vsicurl/http://publicfiles.dep.state.fl.us/dear/BWR_GIS/2007NWFLULC/NWFWMD2007LULC.zip')
    if ds is None:
        return 'fail'

    return 'success'

###############################################################################
#


def vsicurl_2():
    if not gdaltest.run_slow_tests():
        return 'skip'

    if not gdaltest.built_against_curl():
        return 'skip'

    ds = gdal.Open('/vsizip//vsicurl/http://eros.usgs.gov/archive/nslrsda/GeoTowns/HongKong/srtm/n22e113.zip/n22e113.bil')
    if ds is None:
        return 'fail'

    return 'success'

###############################################################################
# This server doesn't support range downloading


def vsicurl_3():
    if not gdaltest.run_slow_tests():
        return 'skip'

    if not gdaltest.built_against_curl():
        return 'skip'

    ds = ogr.Open('/vsizip/vsicurl/http://www.iucnredlist.org/spatial-data/MAMMALS_TERRESTRIAL.zip')
    if ds is not None:
        return 'fail'

    return 'success'

###############################################################################
# This server doesn't support range downloading


def vsicurl_4():
    if not gdaltest.run_slow_tests():
        return 'skip'

    if not gdaltest.built_against_curl():
        return 'skip'

    ds = ogr.Open('/vsizip/vsicurl/http://lelserver.env.duke.edu:8080/LandscapeTools/export/49/Downloads/1_Habitats.zip')
    if ds is not None:
        return 'fail'

    return 'success'

###############################################################################
# Test URL unescaping when reading HTTP file list


def vsicurl_5():
    if not gdaltest.run_slow_tests():
        return 'skip'

    if not gdaltest.built_against_curl():
        return 'skip'

    ds = gdal.Open('/vsicurl/http://dds.cr.usgs.gov/srtm/SRTM_image_sample/picture%20examples/N34W119_DEM.tif')
    if ds is None:
        return 'fail'

    return 'success'

###############################################################################
# Test with FTP server that doesn't support EPSV command


def vsicurl_6():
    if not gdaltest.run_slow_tests():
        return 'skip'

    if not gdaltest.built_against_curl():
        return 'skip'

    fl = gdal.ReadDir('/vsicurl/ftp://ftp2.cits.rncan.gc.ca/pub/cantopo/250k_tif')
    if len(fl) == 0:
        return 'fail'

    return 'success'


###############################################################################
# Test Microsoft-IIS/6.0 listing

def vsicurl_7():
    if not gdaltest.run_slow_tests():
        return 'skip'

    if not gdaltest.built_against_curl():
        return 'skip'

    fl = gdal.ReadDir('/vsicurl/http://ortho.linz.govt.nz/tifs/2005_06')
    if len(fl) == 0:
        return 'fail'

    return 'success'

###############################################################################
# Test interleaved reading between 2 datasets


def vsicurl_8():
    if not gdaltest.run_slow_tests():
        return 'skip'

    if not gdaltest.built_against_curl():
        return 'skip'

    ds1 = gdal.Open('/vsigzip//vsicurl/http://dds.cr.usgs.gov/pub/data/DEM/250/notavail/C/chipicoten-w.gz')
    gdal.Open('/vsizip//vsicurl/http://edcftp.cr.usgs.gov/pub/data/landcover/files/2009/biso/gokn09b_dnbr.zip/nps-serotnbsp-9001-20090321_rd.tif')
    cs = ds1.GetRasterBand(1).Checksum()
    if cs != 61342:
        return 'fail'

    return 'success'

###############################################################################
# Test reading a file with Chinese characters, but the HTTP file listing
# returns escaped sequences instead of the Chinese characters.


def vsicurl_9():
    if not gdaltest.run_slow_tests():
        return 'skip'

    if not gdaltest.built_against_curl():
        return 'skip'

    if version_info >= (3, 0, 0):
        filename = 'xx\u4E2D\u6587.\u4E2D\u6587'
    else:
        exec("filename =  u'xx\u4E2D\u6587.\u4E2D\u6587'")
        filename = filename.encode('utf-8')

    ds = gdal.Open('/vsicurl/http://download.osgeo.org/gdal/data/gtiff/' + filename)
    if ds is None:
        return 'fail'

    return 'success'

###############################################################################
# Test reading a file with escaped Chinese characters.


def vsicurl_10():
    if not gdaltest.run_slow_tests():
        return 'skip'

    if not gdaltest.built_against_curl():
        return 'skip'

    ds = gdal.Open('/vsicurl/http://download.osgeo.org/gdal/data/gtiff/xx%E4%B8%AD%E6%96%87.%E4%B8%AD%E6%96%87')
    if ds is None:
        return 'fail'

    return 'success'

###############################################################################
# Test ReadDir() after reading a file on the same server


def vsicurl_11():
    if not gdaltest.run_slow_tests():
        return 'skip'

    if not gdaltest.built_against_curl():
        return 'skip'

    f = gdal.VSIFOpenL('/vsicurl/http://download.osgeo.org/gdal/data/bmp/Bug2236.bmp', 'rb')
    if f is None:
        return 'skip'
    gdal.VSIFSeekL(f, 1000000, 0)
    gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)

    filelist = gdal.ReadDir('/vsicurl/http://download.osgeo.org/gdal/data/gtiff')
    if filelist is None or len(filelist) == 0:
        return 'fail'

    return 'success'

###############################################################################


def vsicurl_start_webserver():

    gdaltest.webserver_process = None
    gdaltest.webserver_port = 0

    if not gdaltest.built_against_curl():
        return 'skip'

    (gdaltest.webserver_process, gdaltest.webserver_port) = webserver.launch(handler=webserver.DispatcherHttpHandler)
    if gdaltest.webserver_port == 0:
        return 'skip'

    return 'success'

###############################################################################


def vsicurl_test_redirect():

    if gdaltest.is_travis_branch('trusty'):
        print('Skipped on trusty branch, but should be investigated')
        return 'skip'

    if gdaltest.webserver_port == 0:
        return 'skip'

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
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.VSIFSeekL(f, 0, 2)
    if gdal.VSIFTellL(f) != 1000000:
        gdaltest.post_reason('fail')
        print(gdal.VSIFTellL(f))
        gdal.VSIFCloseL(f)
        return 'fail'
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
            gdaltest.post_reason('fail')
            print(content)
            gdal.VSIFCloseL(f)
            return 'fail'
        content = gdal.VSIFReadL(1, 2, f).decode('ascii')
        if content != 'xy':
            gdaltest.post_reason('fail')
            print(content)
            gdal.VSIFCloseL(f)
            return 'fail'

    gdal.VSIFCloseL(f)

    return 'success'

###############################################################################
# TODO: better testing


def vsicurl_test_clear_cache():

    gdal.VSICurlClearCache()
    gdal.VSICurlClearCache()

    return 'success'

###############################################################################


def vsicurl_test_retry():

    if gdaltest.webserver_port == 0:
        return 'skip'

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
        if data_len != 0:
            gdaltest.post_reason('fail')
            print(data_len)
            return 'fail'

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/test_retry/', 404)
    handler.add('HEAD', '/test_retry/test.txt', 200, {'Content-Length': '3'})
    handler.add('GET', '/test_retry/test.txt', 502)
    handler.add('GET', '/test_retry/test.txt', 429)
    handler.add('GET', '/test_retry/test.txt', 200, {}, 'foo')
    with webserver.install_http_handler(handler):
        f = gdal.VSIFOpenL('/vsicurl?max_retry=2&retry_delay=0.01&url=http://localhost:%d/test_retry/test.txt' % gdaltest.webserver_port, 'rb')
        if f is None:
            gdaltest.post_reason('fail')
            return 'fail'
        gdal.ErrorReset()
        with gdaltest.error_handler():
            data = gdal.VSIFReadL(1, 3, f).decode('ascii')
        error_msg = gdal.GetLastErrorMsg()
        gdal.VSIFCloseL(f)
        if data != 'foo':
            gdaltest.post_reason('fail')
            print(data)
            return 'fail'
        if error_msg.find('429') < 0:
            gdaltest.post_reason('fail')
            print(error_msg)
            return 'fail'

    return 'success'

###############################################################################


def vsicurl_test_fallback_from_head_to_get():

    if gdaltest.webserver_port == 0:
        return 'skip'

    gdal.VSICurlClearCache()

    handler = webserver.SequentialHandler()
    handler.add('HEAD', '/test_fallback_from_head_to_get', 405)
    handler.add('GET', '/test_fallback_from_head_to_get', 200, {}, 'foo')
    with webserver.install_http_handler(handler):
        statres = gdal.VSIStatL('/vsicurl/http://localhost:%d/test_fallback_from_head_to_get' % gdaltest.webserver_port)
    if statres.size != 3:
        return 'fail'

    gdal.VSICurlClearCache()

    return 'success'

###############################################################################


def vsicurl_stop_webserver():

    if gdaltest.webserver_port == 0:
        return 'skip'

    # Clearcache needed to close all connections, since the Python server
    # can only handle one connection at a time
    gdal.VSICurlClearCache()

    webserver.server_stop(gdaltest.webserver_process, gdaltest.webserver_port)

    return 'success'


gdaltest_list = [vsicurl_1,
                 # vsicurl_2,
                 # vsicurl_3,
                 vsicurl_4,
                 vsicurl_5,
                 vsicurl_6,
                 vsicurl_7,
                 # vsicurl_8,
                 vsicurl_9,
                 vsicurl_10,
                 vsicurl_11,
                 vsicurl_start_webserver,
                 vsicurl_test_redirect,
                 vsicurl_test_clear_cache,
                 vsicurl_test_retry,
                 vsicurl_test_fallback_from_head_to_get,
                 vsicurl_stop_webserver]

if __name__ == '__main__':

    if gdal.GetConfigOption('GDAL_RUN_SLOW_TESTS', '').upper() != 'NO':
        print('Enabling slow tests as GDAL_RUN_SLOW_TESTS is not defined')
        gdal.SetConfigOption('GDAL_RUN_SLOW_TESTS', 'YES')

    gdaltest.setup_run('vsicurl')

    gdaltest.run_tests(gdaltest_list)

    gdaltest.summarize()
