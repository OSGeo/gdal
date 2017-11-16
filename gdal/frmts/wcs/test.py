#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test WCS client support.
# Author:   Ari Jolma <ari.jolma at gmail.com>
#
###############################################################################
# Copyright (c) 2017, Ari Jolma <ari dot jolma at gmail dotcom>
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

# Test the WCS driver using a dummy HTTP server but real responses from real servers.
# 1. test that the driver can create the same URLs that were used to get the responses.
# todo: test the cache
# test setting and unsetting options

import sys
sys.path.insert(0,'/home/ajolma/github/gdal/autotest/pymod/')
cache = 'CACHE_DIR=/home/ajolma/github/gdal/gdal/frmts/wcs/cache'

try:
    from BaseHTTPServer import HTTPServer
    from BaseHTTPServer import BaseHTTPRequestHandler
except:
    from http.server import BaseHTTPRequestHandler

import urlparse

from osgeo import gdal
    
import webserver

do_log = False

urls = {}
fname = 'responses/urls'
f = open(fname, 'rb')
content = f.read()
f.close()
i = 1
for line in content.splitlines():
    if i == 1:
        key = line
    elif i == 3:
        urls[key] = line
    i += 1
    if i == 4:
        i = 1

scaled = False
        
class WCSHTTPHandler(BaseHTTPRequestHandler):

    def log_request(self, code='-', size='-'):
        return

    def Headers(self, type):
        self.send_response(200)
        self.send_header('Content-Type', type)
        self.end_headers()

    def Respond(self, request, brand, version):
        try:
            fname = 'responses/'
            if request == 'GetCoverage' and scaled:
                suffix = '.tiff'
                fname += brand + '-' + version + '-scaled' + suffix
            elif request == 'GetCoverage':
                suffix = '.tiff'
                fname += brand + '-' + version + suffix
            else:
                suffix = '.xml'
                fname += request + '-' + brand + '-' + version + suffix
            f = open(fname, 'rb')
            content = f.read()
            f.close()
            self.Headers('application/xml')
            self.wfile.write(content)
        except IOError:
            self.send_error(404, 'File Not Found: ' + request + ' ' + brand + ' ' + version)

    def do_GET(self):
        if do_log:
            f = open('/tmp/log.txt', 'a')
            f.write('GET %s\n' % self.path)
            f.close()
        split = urlparse.urlparse(self.path)
        query = urlparse.parse_qs(split.query)
        query2 = {}
        for key in query:
            query2[key.lower()] = query[key]
        server = query2['server'][0]
        version = query2['version'][0]
        request = query2['request'][0]
        if scaled:
            tmp, have = self.path.split('SERVICE=WCS')
            sys.stdout.write('test ' + server + ' WCS ' + version + ' ')
            key = server + '-' + version
            tmp, should_be = urls[key].split('SERVICE=WCS')
            if have == should_be:
                test = 'ok'
            else:
                test = "not ok\n" + have + "\n" + should_be
            print(test)
        self.Respond(request, server, version)
        return

if len(sys.argv) > 1 and sys.argv[1] == "server":
    port = 8080
    server = HTTPServer(('', port), WCSHTTPHandler)
    try:
        print "Starting server"
        server.serve_forever()    
    except KeyboardInterrupt:
        print "Closing server"
        server.server_close()
    sys.exit(0)


(process, port) = webserver.launch(handler = WCSHTTPHandler)

url = "http://127.0.0.1:" + str(port)
first_call = True
try:
    servers = ['MapServer', 'GeoServer', 'Rasdaman', 'ArcGIS']
    versions = ['1.0.0', '1.1.0', '1.1.1', '1.1.2', '2.0.1']
    for server in servers:
        for version in versions:
            if server == 'GeoServer' and version == '1.1.2':
                continue
            if server == 'Rasdaman' and version != '2.0.1':
                continue
            if version == '1.0.0':
                coverage = {'MapServer': 'BGS_EMODNET_CentralMed-MCol',
                            'GeoServer': 'smartsea:eusm2016-EPSG2393',
                            'ArcGIS': '2'}
                server_options = {'MapServer': ['OriginNotCenter100=TRUE'],
                                  'GeoServer': [],
                                  'ArcGIS': []}
            elif version == '1.1.0':
                coverage = {'MapServer': 'BGS_EMODNET_CentralMed-MCol',
                            'GeoServer': 'smartsea:eusm2016-EPSG2393',
                            'ArcGIS': '2'}
                server_options = {'MapServer': ['OffsetsPositive', 'NrOffsets=2', 'NoGridAxisSwap'],
                                  'GeoServer': ['GridCRS', 'OuterExtents', 'BufSizeAdjust=0.5', 'NoGridAxisSwap'],
                                  'ArcGIS': ['NrOffsets=2']}
            elif version == '1.1.1':
                coverage = {'MapServer': 'BGS_EMODNET_CentralMed-MCol',
                            'GeoServer': 'smartsea:eusm2016-EPSG2393',
                            'ArcGIS': '2'}
                server_options = {'MapServer': ['OffsetsPositive', 'NrOffsets=2', 'NoGridAxisSwap'],
                                  'GeoServer': ['GridCRS', 'OuterExtents', 'BufSizeAdjust=0.5', 'NoGridAxisSwap'],
                                  'ArcGIS': ['NrOffsets=2']}
            elif version == '1.1.2':
                coverage = {'MapServer': 'BGS_EMODNET_CentralMed-MCol',
                            'ArcGIS': '2'}
                server_options = {'MapServer': ['OffsetsPositive', 'NrOffsets=2', 'NoGridAxisSwap'],
                                  'ArcGIS': ['NrOffsets=2']}
            elif version == '2.0.1':
                coverage = {'MapServer': 'BGS_EMODNET_CentralMed-MCol',
                            'GeoServer': 'smartsea__eusm2016-EPSG2393',
                            'Rasdaman': 'BlueMarbleCov',
                            'ArcGIS': 'Coverage2'}
                server_options = {'MapServer': ['GridAxisLabelSwap'],
                                  'GeoServer': ['NoGridAxisSwap', 'SubsetAxisSwap'],
                                  'Rasdaman': [],
                                  'ArcGIS': ['UseScaleFactor']}
                
            projwin = {'MapServer': [10, 45, 15, 35],
                       'GeoServer': [3200000, 6670000, 3280000, 6620000],
                       'ArcGIS': [181000, 7005000, 200000, 6980000],
                       'Rasdaman': [10, 45, 15, 35]}
            
            options = [cache]
            if first_call:
                options.append('CLEAR_CACHE')
                first_call = False

            scaled = False
            # get capabilities
            query = 'server=' + server
            query += '&version=' + version                     
            ds = gdal.OpenEx(utf8_path = "WCS:" + url + "/?" + query,
                             open_options = options)
            
            options = [cache]
            options += server_options[server]
            query += '&coverage=' + coverage[server]
            ds = gdal.OpenEx(utf8_path = "WCS:" + url + "/?" + query,
                             open_options = options)
            # delete ds
            ds = 0

            scaled = True
            options = [cache]
            ds = gdal.OpenEx(utf8_path = "WCS:" + url + "/?" + query,
                             open_options = options)
            ds = gdal.Translate('output.tif', ds, projWin = projwin[server], width = 60)
except:
    print "Unexpected error:", sys.exc_info()[0]
    webserver.server_stop(process, port)
    sys.exit(0)

webserver.server_stop(process, port)
