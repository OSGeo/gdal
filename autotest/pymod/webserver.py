#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Fake HTTP server
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2010-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

import contextlib
import time
import sys
import gdaltest
from sys import version_info

do_log = False
custom_handler = None

@contextlib.contextmanager
def install_http_handler(handler_instance):
    global custom_handler
    custom_handler = handler_instance
    try:
        yield
    finally:
        handler_instance.final_check()
        custom_handler = None

class RequestResponse:
    def __init__(self, method, path, code, headers = {}, body = None, custom_method = None):
        self.method = method
        self.path = path
        self.code = code
        self.headers = headers
        self.body = body
        self.custom_method = custom_method

class SequentialHandler:
    def __init__(self):
        self.req_count = 0
        self.req_resp = []

    def final_check(self):
        assert self.req_count == len(self.req_resp), (self.req_count, len(self.req_resp))

    def add(self, method, path, code = None, headers = {}, body = None, custom_method = None):
        self.req_resp.append( RequestResponse(method, path, code, headers, body, custom_method) )

    def process(self, method, request):
        if self.req_count < len(self.req_resp):
            req_resp = self.req_resp[self.req_count]
            if method == req_resp.method and request.path == req_resp.path:
                self.req_count += 1

                if req_resp.custom_method:
                    req_resp.custom_method(request)
                else:
                    request.send_response(req_resp.code)
                    for k in req_resp.headers:
                        request.send_header(k, req_resp.headers[k])
                    if req_resp.body:
                        request.send_header('Content-Length', len(req_resp.body))
                    elif not 'Content-Length' in req_resp.headers:
                        request.send_header('Content-Length', '0')
                    request.end_headers()
                    if req_resp.body:
                        request.wfile.write(req_resp.body.encode('ascii'))

                return

        request.send_error(500,'Unexpected %s request for %s, req_count = %d' % (method, request.path, self.req_count))

    def do_HEAD(self, request):
        self.process('HEAD', request)

    def do_GET(self, request):
        self.process('GET', request)

    def do_POST(self, request):
        self.process('POST', request)

    def do_PUT(self, request):
        self.process('PUT', request)

    def do_DELETE(self, request):
        self.process('DELETE', request)

class DispatcherHttpHandler(BaseHTTPRequestHandler):

    # protocol_version = 'HTTP/1.1'

    def log_request(self, code='-', size='-'):
        return

    def do_HEAD(self):

        if do_log:
            f = open('/tmp/log.txt', 'a')
            f.write('HEAD %s\n' % self.path)
            f.close()

        custom_handler.do_HEAD(self)

    def do_DELETE(self):

        if do_log:
            f = open('/tmp/log.txt', 'a')
            f.write('DELETE %s\n' % self.path)
            f.close()

        custom_handler.do_DELETE(self)

    def do_POST(self):

        if do_log:
            f = open('/tmp/log.txt', 'a')
            f.write('POST %s\n' % self.path)
            f.close()

        custom_handler.do_POST(self)

    def do_PUT(self):

        if do_log:
            f = open('/tmp/log.txt', 'a')
            f.write('PUT %s\n' % self.path)
            f.close()

        custom_handler.do_PUT(self)

    def do_GET(self):

        if do_log:
            f = open('/tmp/log.txt', 'a')
            f.write('GET %s\n' % self.path)
            f.close()

        custom_handler.do_GET(self)

class GDAL_Handler(BaseHTTPRequestHandler):

    def log_request(self, code='-', size='-'):
        return

    def do_HEAD(self):
        if do_log:
            f = open('/tmp/log.txt', 'a')
            f.write('HEAD %s\n' % self.path)
            f.close()

        self.send_error(404,'File Not Found: %s' % self.path)

    def do_DELETE(self):
        if do_log:
            f = open('/tmp/log.txt', 'a')
            f.write('DELETE %s\n' % self.path)
            f.close()

        self.send_error(404,'File Not Found: %s' % self.path)

    def do_POST(self):
        if do_log:
            f = open('/tmp/log.txt', 'a')
            f.write('POST %s\n' % self.path)
            f.close()

        self.send_error(404,'File Not Found: %s' % self.path)

    def do_PUT(self):
        if do_log:
            f = open('/tmp/log.txt', 'a')
            f.write('PUT %s\n' % self.path)
            f.close()

        self.send_error(404,'File Not Found: %s' % self.path)

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
                gdaltest.gdalurlopen("http://127.0.0.1:%d/shutdown" % self.port)
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
        # Explicitly destroy the object so that the socket is really closed
        del self.server

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

def launch(fork_process = None, handler = None):
    if handler is not None:
        if fork_process:
            raise Exception('fork_process = True incompatible with custom handler')
        fork_process = False
    else:
        fork_process = True

    if not fork_process or handler is not None:
        try:
            if handler is None:
                handler = GDAL_Handler
            server = GDAL_ThreadedHttpServer(handler)
            server.start_and_wait_ready()
            return (server, server.getPort())
        except:
            return (None, 0)

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

    if isinstance(process, GDAL_ThreadedHttpServer):
        process.stop()
        return

    gdaltest.gdalurlopen('http://127.0.0.1:%d/shutdown' % port)
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
