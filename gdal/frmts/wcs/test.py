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
import numbers
import collections
import re
import urlparse

try:
    from BaseHTTPServer import HTTPServer
    from BaseHTTPServer import BaseHTTPRequestHandler
except:
    from http.server import BaseHTTPRequestHandler

sys.path.insert(0,'../../../autotest/pymod/')

import webserver

from osgeo import gdal


do_log = False
cache = 'CACHE=wcs_cache'

def read_urls():
    retval = {}
    fname = 'responses/urls'
    f = open(fname, 'rb')
    content = f.read()
    f.close()
    i = 1
    for line in content.splitlines():
        items = line.split()
        retval[items[0]] = {}
        retval[items[0]][items[1]] = items[2]
    return retval

urls = read_urls()

class WCSHTTPHandler(BaseHTTPRequestHandler):

    def log_request(self, code='-', size='-'):
        return

    def Headers(self, type):
        self.send_response(200)
        self.send_header('Content-Type', type)
        self.end_headers()

    def Respond(self, request, brand, version, test):
        try:
            fname = 'responses/'
            if request == 'GetCoverage' and test == "scaled":
                suffix = '.tiff'
                fname += brand + '-' + version + '-scaled' + suffix
            elif request == 'GetCoverage':
                suffix = '.tiff'
                fname += brand + '-' + version + suffix
            else:
                suffix = '.xml'
                fname += request + '-' + brand + '-' + version + suffix
            #print 'test '+test+' return '+fname
            f = open(fname, 'rb')
            content = f.read()
            f.close()
            self.Headers('application/xml')
            self.wfile.write(content)
        except IOError:
            self.send_error(404, 'File Not Found: ' + request + ' ' + brand + ' ' + version)

    def do_GET(self):
        #print self.path
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
        test = ''
        if 'test' in query2:
            test = query2['test'][0]
        if test == "scaled":
            tmp, got = self.path.split('SERVICE=WCS')
            got = re.sub('\&test=.*', '', got)
            key = server + '-' + version
            tmp, should_be = urls[key][test].split('SERVICE=WCS')
            if got == should_be:
                ok = 'ok'
            else:
                ok = "not ok\ngot:  " + got + "\nhave: " + should_be
            print('test ' + server + ' WCS ' + version + ' '+ok)
        self.Respond(request, server, version, test)
        return

port = 8080

if len(sys.argv) > 1 and sys.argv[1] == "server":
    server = HTTPServer(('', port), WCSHTTPHandler)
    try:
        print "Starting server"
        server.serve_forever()
    except KeyboardInterrupt:
        print "Closing server"
        server.server_close()
    sys.exit(0)

url = "http://127.0.0.1:" + str(port)
first_call = True
size = 60

def test():
    try:
        setup = setupFct()
        servers = []
        for server in setup:
            servers.append(server)
        for server in sorted(servers):
            #if server != "MapServer":
            #    continue
            #print "** SERVER: "+server
            for i, v in enumerate(setup[server]['Versions']):
                #if v != 110:
                #    continue
                version = str(int(v / 100)) + '.' + str(int(v % 100 / 10)) + '.' + str((v % 10))
                if not server + '-' + version in urls:
                    print "Error: " + server + '-' + version + " not in urls"
                    continue
                options = [cache]
                global first_call
                if first_call:
                    options.append('CLEAR_CACHE')
                    first_call = False
                # get capabilities
                query = 'version=' + version
                options.append('GetCapabilitiesExtra=server=' + server)
                ds = gdal.OpenEx(utf8_path = "WCS:" + url + "/?" + query,
                                 open_options = options)

                coverage = setup[server]['Coverage']
                if isinstance(coverage, list):
                    coverage = coverage[i]
                if isinstance(coverage, numbers.Number):
                    coverage = str(coverage)
                query += '&coverage=' + coverage

                options = [cache]
                if isinstance(setup[server]['Options'], list):
                    oo = setup[server]['Options'][i]
                else:
                    oo = setup[server]['Options']
                oo = oo.split();
                for o in oo:
                    if o != '-oo':
                        options.append(o)
                options.append('DescribeCoverageExtra=server=' + server)
                options.append('GetCoverageExtra=test=none&server=' + server)
                
                ds = gdal.OpenEx(utf8_path = "WCS:" + url + "/?" + query,
                                 open_options = options)

                # delete ds
                ds = 0
                options = [cache]
                options.append('GetCoverageExtra=test=scaled&server=' + server)
                options.append('INTERLEAVE=PIXEL')
                ds = gdal.OpenEx(utf8_path = "WCS:" + url + "/?" + query,
                                 open_options = options)
                projwin = setup[server]['Projwin'].replace('-projwin ', '').split()
                for i, c in enumerate(projwin):
                    projwin[i] = int(c)
                #print projwin
                ds = gdal.Translate('output.tif', ds, projWin = projwin, width = size)
    except:
        print "Unexpected error:", sys.exc_info()[0]
        webserver.server_stop(process, port)
        sys.exit(0)

def setupFct():
    return {
        'SimpleGeoServer' : {
            'URL' : 'https://msp.smartsea.fmi.fi/geoserver/wcs',
            'Options' : [
                "",
                "-oo OuterExtents",
                "-oo OuterExtents",
                ""
                ],
            'Projwin' : "-projwin 145300 6737500 209680 6688700",
            'Outsize' : "-outsize $size 0",
            'Coverage' : [
                'smartsea:eusm2016', 'smartsea:eusm2016',
                'smartsea:eusm2016', 'smartsea__eusm2016'],
            'Versions' : [100, 110, 111, 201],
        },
        'GeoServer2' : {
            'URL' : 'https://msp.smartsea.fmi.fi/geoserver/wcs',
            'Options' : [
                "",
                "-oo OuterExtents -oo NoGridAxisSwap",
                "-oo OuterExtents -oo NoGridAxisSwap",
                "-oo NoGridAxisSwap -oo SubsetAxisSwap"
                ],
            'Projwin' : "-projwin 145300 6737500 209680 6688700",
            'Outsize' : "-outsize $size 0",
            'Coverage' : ['smartsea:south', 'smartsea:south', 'smartsea:south', 'smartsea__south'],
            'Versions' : [100, 110, 111, 201],
            'Range' : ['GREEN_BAND', 'BLUE_BAND']
        },
        'GeoServer' : {
            'URL' : 'https://msp.smartsea.fmi.fi/geoserver/wcs',
            'Options' : [
                "",
                "-oo OuterExtents -oo BufSizeAdjust=0.5 -oo NoGridAxisSwap",
                "-oo OuterExtents -oo BufSizeAdjust=0.5 -oo NoGridAxisSwap",
                "-oo NoGridAxisSwap -oo SubsetAxisSwap",
                ],
            'Projwin' : "-projwin 3200000 6670000 3280000 6620000",
            'Outsize' : "-outsize $size 0",
            'Coverage' : [
                'smartsea:eusm2016-EPSG2393', 'smartsea:eusm2016-EPSG2393',
                'smartsea:eusm2016-EPSG2393', 'smartsea__eusm2016-EPSG2393'],
            'Versions' : [100, 110, 111, 201]
        },
        'MapServer' : {
            'URL' : 'http://194.66.252.155/cgi-bin/BGS_EMODnet_bathymetry/ows',
            'Options' : [
                "-oo INTERLEAVE=PIXEL -oo OriginAtBoundary -oo BandIdentifier=none",
                "-oo INTERLEAVE=PIXEL -oo OffsetsPositive -oo NrOffsets=2 -oo NoGridAxisSwap -oo BandIdentifier=none",
                "-oo INTERLEAVE=PIXEL -oo OffsetsPositive -oo NrOffsets=2 -oo NoGridAxisSwap -oo BandIdentifier=none",
                "-oo INTERLEAVE=PIXEL -oo OffsetsPositive -oo NrOffsets=2 -oo NoGridAxisSwap -oo BandIdentifier=none",
                "-oo OriginAtBoundary",
                ],
            'Projwin' : "-projwin 10 45 15 35",
            'Outsize' : "-outsize $size 0",
            'Coverage' : 'BGS_EMODNET_CentralMed-MCol',
            'Versions' : [100, 110, 111, 112, 201]
        },
        'Rasdaman' : {
            'URL' : 'http://ows.rasdaman.org/rasdaman/ows',
            'Options' : "",
            'Projwin' : "-projwin 10 45 15 35",
            'Outsize' : "-outsize $size 0",
            'Coverage' : 'BlueMarbleCov',
            'Versions' : [201]
        },
        'Rasdaman2' : {
            'URL' : 'http://ows.rasdaman.org/rasdaman/ows',
            'Options' : '-oo subset=unix("2008-01-05T01:58:30.000Z")',
            'Projwin' : "-projwin 100000 5400000 150000 5100000",
            'Outsize' : "-outsize $size 0",
            'Coverage' : 'test_irr_cube_2',
            'Versions' : [201],
            'Dimension' : "unix(\"2008-01-05T01:58:30.000Z\")"
        },
        'ArcGIS' : {
            'URL' : 'http://paikkatieto.ymparisto.fi/arcgis/services/Testit/Velmu_wcs_testi/MapServer/WCSServer',
            'Options' : [
                "",
                "-oo NrOffsets=2",
                "-oo NrOffsets=2",
                "-oo NrOffsets=2",
                "-oo UseScaleFactor"
                ],
            'Projwin' : "-projwin 181000 7005000 200000 6980000",
            'Outsize' : "-outsize $size 0",
            'Coverage' : [2, 2, 2, 2, 'Coverage2'],
            'Versions' : [100, 110, 111, 112, 201]
        }
    }

(process, port) = webserver.launch(handler = WCSHTTPHandler)
test()
webserver.server_stop(process, port)
