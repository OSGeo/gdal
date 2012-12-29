#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Fake HTTP server
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at mines dash paris dot org>
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


try:
    from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer
except:
    from http.server import BaseHTTPRequestHandler, HTTPServer
from threading import Thread

import time
import sys
import gdaltest
from sys import version_info

do_log = False

class GDAL_Handler(BaseHTTPRequestHandler):

    def log_request(self, code='-', size='-'):
        return

    def do_DELETE(self):
        if do_log:
            f = open('/tmp/log.txt', 'a')
            f.write('DELETE %s\n' % self.path)
            f.close()

        if self.path.find('/fakeelasticsearch') != -1:
            self.send_response(200)
            self.end_headers()

        return

    def do_POST(self):
        if do_log:
            f = open('/tmp/log.txt', 'a')
            f.write('POST %s\n' % self.path)
            f.close()

        if self.path.find('/fakeelasticsearch') != -1:
            self.send_response(200)
            self.end_headers()

        return

    def do_GET(self):

        try:
            if do_log:
                f = open('/tmp/log.txt', 'a')
                f.write('GET %s\n' % self.path)
                f.close()

            if self.path == '/shutdown':
                self.send_response(200)
                self.send_header('Content-type', 'text/html')
                self.end_headers()
                #sys.stderr.write('stop requested\n')
                self.server.stop_requested = True
                return

            if self.path == '/index.html':
                self.send_response(200)
                self.send_header('Content-type', 'text/html')
                self.end_headers()
                return


            # Below is for ElasticSearch
            if self.path.find('/fakeelasticsearch') != -1:
                if self.path == '/fakeelasticsearch/_status':
                    self.send_response(200)
                    self.end_headers()
                    self.elastic_search = True
                    return
                else:
                    self.send_error(404,'File Not Found: %s' % self.path)
                    return

            # Below is for geocoding
            elif self.path.find('/geocoding') != -1:
                if self.path == '/geocoding?q=Paris&addressdetails=1&limit=1&email=foo%40bar':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<?xml version="1.0" encoding="UTF-8"?>
<searchresults>
  <place lat="48.8566177374844" lon="2.34288146739775" display_name="Paris, Ile-de-France, France metropolitaine">
    <county>Paris</county>
    <state>Ile-de-France</state>
    <country>France metropolitaine</country>
    <country_code>fr</country_code>
  </place>
</searchresults>""")
                    return
                elif self.path == '/geocoding?q=NonExistingPlace&addressdetails=1&limit=1&email=foo%40bar':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<?xml version="1.0" encoding="UTF-8"?><searchresults></searchresults>""")
                    return
                else:
                    self.send_error(404,'File Not Found: %s' % self.path)
                    return

            elif self.path.find('/yahoogeocoding') != -1:
                if self.path == '/yahoogeocoding?q=Paris':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<?xml version="1.0" encoding="UTF-8" standalone="yes"?><ResultSet xmlns:ns1="http://www.yahooapis.com/v1/base.rng" version="2.0" xml:lang="en-US"><Error>0</Error><ErrorMessage>No error</ErrorMessage><Locale>en-US</Locale><Found>1</Found><Quality>40</Quality><Result><quality>40</quality><latitude>48.85693</latitude><longitude>2.3412</longitude><offsetlat>48.85693</offsetlat><offsetlon>2.3412</offsetlon><radius>9200</radius><name></name><line1></line1><line2>Paris</line2><line3></line3><line4>France</line4><house></house><street></street><xstreet></xstreet><unittype></unittype><unit></unit><postal></postal><neighborhood></neighborhood><city>Paris</city><county>Paris</county><state>Ile-de-France</state><country>France</country><countrycode>FR</countrycode><statecode></statecode><countycode>75</countycode><uzip>75001</uzip><hash></hash><woeid>615702</woeid><woetype>7</woetype></Result></ResultSet>
<!-- nws03.maps.bf1.yahoo.com uncompressed/chunked Sat Dec 29 04:59:06 PST 2012 -->
<!-- wws09.geotech.bf1.yahoo.com uncompressed/chunked Sat Dec 29 04:59:06 PST 2012 -->""")
                    return
                elif self.path == '/yahoogeocoding?q=NonExistingPlace':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<?xml version="1.0" encoding="UTF-8" standalone="yes"?><ResultSet xmlns:ns1="http://www.yahooapis.com/v1/base.rng" version="2.0" xml:lang="en-US"><Error>7</Error><ErrorMessage>No result</ErrorMessage><Locale>en-US</Locale><Found>0</Found><Quality>0</Quality></ResultSet>
<!-- nws08.maps.bf1.yahoo.com uncompressed/chunked Sat Dec 29 05:00:45 PST 2012 -->
<!-- wws08.geotech.bf1.yahoo.com uncompressed/chunked Sat Dec 29 05:00:45 PST 2012 -->""")
                    return

                else:
                    self.send_error(404,'File Not Found: %s' % self.path)
                    return

            elif self.path.find('/geonamesgeocoding') != -1:
                if self.path == '/geonamesgeocoding?q=Paris&username=demo':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<geonames style="MEDIUM">
<totalResultsCount>2356</totalResultsCount>
<geoname>
<toponymName>Paris</toponymName>
<name>Paris</name>
<lat>48.85341</lat>
<lng>2.3488</lng>
<geonameId>2988507</geonameId>
<countryCode>FR</countryCode>
<countryName>France</countryName>
<fcl>P</fcl>
<fcode>PPLC</fcode>
</geoname>
</geonames>""")
                    return
                elif self.path == '/geonamesgeocoding?q=NonExistingPlace&username=demo':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<geonames style="MEDIUM">
<totalResultsCount>0</totalResultsCount>
</geonames>""")
                    return

                else:
                    self.send_error(404,'File Not Found: %s' % self.path)
                    return

            # Below is for reverse geocoding
            elif self.path.find('/reversegeocoding') != -1:
                if self.path == '/reversegeocoding?lon=2.0000000000000000&lat=49.0000000000000000&email=foo%40bar' or \
                   self.path == '/reversegeocoding?lon=2.0000000000000000&lat=49.0000000000000000&zoom=12&email=foo%40bar':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<?xml version="1.0" encoding="UTF-8"?>
<reversegeocode>
  <result place_id="46754274" osm_type="way" osm_id="38621743" ref="Chemin du Cordon" lat="49.0002726061675" lon="1.99514157818059">Chemin du Cordon, Forêt de l'Hautil, Triel-sur-Seine, Saint-Germain-en-Laye, Yvelines, Île-de-France, 78510, France métropolitaine</result>
  <addressparts>
    <road>Chemin du Cordon</road>
    <forest>Forêt de l'Hautil</forest>
    <city>Triel-sur-Seine</city>
    <county>Saint-Germain-en-Laye</county>
    <state>Île-de-France</state>
    <postcode>78510</postcode>
    <country>France métropolitaine</country>
    <country_code>fr</country_code>
  </addressparts>
</reversegeocode>
""")
                    return
                else:
                    self.send_error(404,'File Not Found: %s' % self.path)
                    return

            elif self.path.find('/yahooreversegeocoding') != -1:
                if self.path == '/yahooreversegeocoding?q=49.0000000000000000,2.0000000000000000&gflags=R':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<?xml version="1.0" encoding="UTF-8" standalone="yes"?><ResultSet xmlns:ns1="http://www.yahooapis.com/v1/base.rng" version="2.0" xml:lang="en-US"><Error>0</Error><ErrorMessage>No error</ErrorMessage><Locale>en-US</Locale><Found>1</Found><Quality>99</Quality><Result><quality>72</quality><latitude>49.001</latitude><longitude>1.999864</longitude><offsetlat>49.001</offsetlat><offsetlon>1.999864</offsetlon><radius>400</radius><name>49.0000000000000000,2.0000000000000000</name><line1>Chemin de Menucourt</line1><line2>78510 Triel-sur-Seine</line2><line3></line3><line4>France</line4><house></house><street>Chemin de Menucourt</street><xstreet></xstreet><unittype></unittype><unit></unit><postal>78510</postal><neighborhood></neighborhood><city>Triel-sur-Seine</city><county>Yvelines</county><state>Ile-de-France</state><country>France</country><countrycode>FR</countrycode><statecode></statecode><countycode>78</countycode><uzip>78510</uzip><hash></hash><woeid>12727518</woeid><woetype>11</woetype></Result></ResultSet>
<!-- nws02.maps.bf1.yahoo.com uncompressed/chunked Sat Dec 29 05:03:31 PST 2012 -->
<!-- wws05.geotech.bf1.yahoo.com uncompressed/chunked Sat Dec 29 05:03:31 PST 2012 -->""")
                    return
                else:
                    self.send_error(404,'File Not Found: %s' % self.path)
                    return

            elif self.path.find('/geonamesreversegeocoding') != -1:
                if self.path == '/geonamesreversegeocoding?lat=49.0000000000000000&lng=2.0000000000000000&username=demo':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<geonames>
<geoname>
<toponymName>Paris Basin</toponymName>
<name>Paris Basin</name>
<lat>49</lat>
<lng>2</lng>
<geonameId>2988503</geonameId>
<countryCode>FR</countryCode>
<countryName>France</countryName>
<fcl>T</fcl>
<fcode>DPR</fcode>
<distance>0</distance>
</geoname>
</geonames>""")
                    return
                else:
                    self.send_error(404,'File Not Found: %s' % self.path)
                    return

            # Below is for WFS
            elif self.path.find('/fakewfs') != -1:

                if self.path == '/fakewfs?SERVICE=WFS&REQUEST=GetCapabilities' or \
                self.path == '/fakewfs?SERVICE=WFS&REQUEST=GetCapabilities&ACCEPTVERSIONS=1.1.0,1.0.0':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    f = open('data/get_capabilities.xml', 'rb')
                    content = f.read()
                    f.close()
                    self.wfile.write(content)
                    return

                if self.path == '/fakewfs?SERVICE=WFS&VERSION=1.1.0&REQUEST=DescribeFeatureType&TYPENAME=rijkswegen':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    f = open('data/describe_feature_type.xml', 'rb')
                    content = f.read()
                    f.close()
                    self.wfile.write(content)
                    return

                if self.path == '/fakewfs?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=rijkswegen':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    f = open('data/get_feature.xml', 'rb')
                    content = f.read()
                    f.close()
                    self.wfile.write(content)
                    return

            return
        except IOError:
            pass

        self.send_error(404,'File Not Found: %s' % self.path)


class GDAL_HttpServer(HTTPServer):

    def __init__ (self, server_address, handlerClass):
        HTTPServer.__init__(self, server_address, handlerClass)
        self.running = False
        self.stop_requested = False

    def is_running(self):
        return self.running

    def stop_server(self):
        if self.running:
            if version_info >= (2,6,0):
                self.shutdown()
            else:
                handle = gdaltest.gdalurlopen("http://127.0.0.1:%d/shutdown" % self.port)
        self.running = False

    def serve_until_stop_server(self):
        self.running = True
        if version_info >= (2,6,0):
            self.serve_forever(0.25)
        else:
            while self.running and not self.stop_requested:
                self.handle_request()
        self.running = False
        self.stop_requested = False

class GDAL_ThreadedHttpServer(Thread):

    def __init__ (self, handlerClass = None):
        Thread.__init__(self)
        ok = False
        self.server = 0
        if handlerClass is None:
            handlerClass = GDAL_Handler
        for port in range(8080,8100):
            try:
                self.server = GDAL_HttpServer(('', port), handlerClass)
                self.server.port = port
                ok = True
                break
            except:
                pass
        if not ok:
            raise Exception('could not start server')

    def getPort(self):
        return self.server.port

    def run(self):
        try:
            self.server.serve_until_stop_server()
        except KeyboardInterrupt:
            print('^C received, shutting down server')
            self.server.socket.close()

    def start_and_wait_ready(self):
        if self.server.running:
            raise Exception('server already started')
        self.start()
        while not self.server.running:
            time.sleep(1)

    def stop(self):
        self.server.stop_server()

    def run_server(self, timeout):
        if not self.server.running:
            raise Exception('server not started')
        count = 0
        while (timeout <= 0 or count < timeout) and self.server.running and not self.server.stop_requested:
            #print(count)
            #print(self.server.is_running())
            time.sleep(0.5)
            count = count + 0.5
        self.stop()

def launch():
    python_exe = sys.executable
    if sys.platform == 'win32':
        python_exe = python_exe.replace('\\', '/')

    (process, process_stdout) = gdaltest.spawn_async(python_exe + ' ../pymod/webserver.py')
    if process is None:
        return (None, 0)

    line = process_stdout.readline()
    line = line.decode('ascii')
    process_stdout.close()
    if line.find('port=') == -1:
        return (None, 0)

    port = int(line[5:])
    if port != 0:
        print('HTTP Server started on port %d' % port)

    return (process, port)

def server_stop(process, port):
    handle = gdaltest.gdalurlopen('http://127.0.0.1:%d/shutdown' % port)
    gdaltest.wait_process(process)

def main():
    try:
        server = GDAL_ThreadedHttpServer(GDAL_Handler)
        server.start_and_wait_ready()
        print('port=%d' % server.getPort())
        sys.stdout.flush()
    except:
        print('port=0')
        sys.stdout.flush()
        sys.exit(0)

    server.run_server(10)

if __name__ == '__main__':
    main()
