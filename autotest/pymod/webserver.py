#!/usr/bin/env python
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

class GDAL_Handler(BaseHTTPRequestHandler):

    def log_request(self, code='-', size='-'):
        return

    def do_GET(self):

        try:
            #print(self.path)

            if self.path == '/shutdown':
                self.send_response(200)
                self.send_header('Content-type', 'text/html')
                self.end_headers()
                #print('stop requested')
                self.server.stopped = True
                return

            if self.path == '/index.html':
                self.send_response(200)
                self.send_header('Content-type', 'text/html')
                self.end_headers()
                return

            if self.path == '/fakewfs?SERVICE=WFS&REQUEST=GetCapabilities':
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
        self.stopped = False

    def is_running(self):
        return not self.stopped

    def stop_server(self, stop_url):
        self.stopped = True
        from sys import version_info
        if version_info < (3,0,0):
            import urllib2
            handle = urllib2.urlopen(stop_url)
        else:
            import urllib.request
            handle = urllib.request.urlopen(stop_url)

    def serve_until_stop_server(self):
        while not self.stopped:
            self.handle_request()

        self.stopped = True

class GDAL_ThreadedHttpServer(Thread):

    def __init__ (self, handlerClass = None):
        Thread.__init__(self)
        ok = False
        if handlerClass is None:
            handlerClass = GDAL_Handler
        for i in range(8080,8100):
            try:
                self.port = i
                self.server = GDAL_HttpServer(('', self.port), handlerClass)
                ok = True
                break
            except:
                pass
        if not ok:
            raise Exception('could not start server')
        self.running = False

    def getPort(self):
        return self.port

    def run(self):
        try:
            print('started httpserver...')
            self.running = True
            self.server.serve_until_stop_server()
        except KeyboardInterrupt:
            print('^C received, shutting down server')
            self.server.socket.close()
        self.running = False

    def wait_ready(self):
        while not self.running:
            import time
            time.sleep(1)

    def stop(self):
        if self.running:
            self.server.stop_server("http://127.0.0.1:%d/shutdown" % self.port)

    def run_server(self, timeout):
        self.start()
        self.wait_ready()
        count = 0
        while (timeout <= 0 or count < timeout) and self.server.is_running():
            #print(count)
            #print(self.server.is_running())
            time.sleep(1)
            count = count + 1

def launch():
    (process, process_stdout) = gdaltest.spawn_async('python ../pymod/webserver.py')
    if process is None:
        (process, process_stdout) = gdaltest.spawn_async('python3 ../pymod/webserver.py')
        if process is None:
            return 'skip'

    line = process_stdout.readline()
    line = line.decode('ascii')
    if line.find('port=') == -1:
        return 'skip'

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
        print('port=%d\n' % server.getPort())
        sys.stdout.flush()
        server.run_server(60)
    except:
        print('port=0\n')
        sys.stdout.flush()

if __name__ == '__main__':
    main()
