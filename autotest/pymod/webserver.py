#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Fake HTTP server
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2010-2012, Even Rouault <even dot rouault at spatialys.com>
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

import contextlib
from http.server import BaseHTTPRequestHandler, HTTPServer
import subprocess
import sys
from threading import Thread
import time

import gdaltest

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


class RequestResponse(object):
    def __init__(self, method, path, code, headers=None, body=None, custom_method=None, expected_headers=None, expected_body=None, add_content_length_header=True, unexpected_headers=[]):
        self.method = method
        self.path = path
        self.code = code
        self.headers = {} if headers is None else headers
        self.body = body
        self.custom_method = custom_method
        self.expected_headers = {} if expected_headers is None else expected_headers
        self.expected_body = expected_body
        self.add_content_length_header = add_content_length_header
        self.unexpected_headers = unexpected_headers

    def __repr__(self):
        return (
            f"RequestResponse({self.method}, {self.path}, {self.code}, headers={self.headers}, "
            f"body={self.body}, custom_method={self.custom_method}, "
            f"expected_headers={self.expected_headers}, expected_body={self.expected_body}, "
            f"add_content_length_header={self.add_content_length_header}, "
            f"unexpected_headers={self.unexpected_headers}"
        )


class FileHandler(object):
    def __init__(self, _dict):
        self.dict = _dict

    def final_check(self):
        pass

    def do_HEAD(self, request):
        if request.path not in self.dict:
            request.send_response(404)
            request.end_headers()
        else:
            request.send_response(200)
            request.send_header('Content-Length', len(self.dict[request.path]))
            request.end_headers()

    def do_GET(self, request):
        if request.path not in self.dict:
            request.send_response(404)
            request.end_headers()
        else:
            filedata = self.dict[request.path]
            start = 0
            end = len(filedata)
            if 'Range' in request.headers:
                import re
                res = re.search(r'bytes=(\d+)\-(\d+)', request.headers['Range'])
                if res:
                    res = res.groups()
                    start = int(res[0])
                    end = int(res[1]) + 1
                    if end > len(filedata):
                        end = len(filedata)
            request.send_response(200)
            if 'Range' in request.headers:
                request.send_header('Content-Range', '%d-%d' % (start, end - 1))
            request.send_header('Content-Length', len(filedata))
            request.end_headers()
            request.wfile.write(filedata[start:end])


class SequentialHandler(object):
    def __init__(self):
        self.req_count = 0
        self.req_resp = []
        self.req_resp_map = {}

    def final_check(self):
        assert self.req_count == len(self.req_resp), (self.req_count, len(self.req_resp))
        assert not self.req_resp_map

    def add(self, method, path, code=None, headers=None, body=None, custom_method=None, expected_headers=None, expected_body=None, add_content_length_header=True, unexpected_headers=[]):
        hdrs = {} if headers is None else headers
        expected_hdrs = {} if expected_headers is None else expected_headers
        assert not self.req_resp_map
        self.req_resp.append(RequestResponse(method, path, code, hdrs, body, custom_method, expected_hdrs, expected_body, add_content_length_header, unexpected_headers))

    def add_unordered(self, method, path, code=None, headers=None, body=None, custom_method=None, expected_headers=None, expected_body=None, add_content_length_header=True, unexpected_headers=[]):
        hdrs = {} if headers is None else headers
        expected_hdrs = {} if expected_headers is None else expected_headers
        self.req_resp_map[(method, path)] = RequestResponse(method, path, code, hdrs, body, custom_method, expected_hdrs, expected_body, add_content_length_header, unexpected_headers)

    @staticmethod
    def _process_req_resp(req_resp, request):
        if req_resp.custom_method:
            req_resp.custom_method(request)
        else:

            if req_resp.expected_headers:
                for k in req_resp.expected_headers:
                    if k not in request.headers or request.headers[k] != req_resp.expected_headers[k]:
                        sys.stderr.write('Did not get expected headers: %s\n' % str(request.headers))
                        request.send_response(400)
                        request.send_header('Content-Length', 0)
                        request.end_headers()
                        return

            for k in req_resp.unexpected_headers:
                if k in request.headers:
                    sys.stderr.write('Did not expect header: %s\n' % k)
                    request.send_response(400)
                    request.send_header('Content-Length', 0)
                    request.end_headers()
                    return

            if req_resp.expected_body:
                content = request.rfile.read(int(request.headers['Content-Length']))
                if content != req_resp.expected_body:
                    sys.stderr.write('Did not get expected content: %s\n' % content)
                    request.send_response(400)
                    request.send_header('Content-Length', 0)
                    request.end_headers()
                    return

            request.send_response(req_resp.code)
            for k in req_resp.headers:
                request.send_header(k, req_resp.headers[k])
            if req_resp.add_content_length_header:
                if req_resp.body:
                    request.send_header('Content-Length', len(req_resp.body))
                elif 'Content-Length' not in req_resp.headers:
                    request.send_header('Content-Length', '0')
            request.end_headers()
            if req_resp.body:
                try:
                    request.wfile.write(req_resp.body)
                except:
                    request.wfile.write(req_resp.body.encode('ascii'))

    def process(self, method, request):
        if self.req_count < len(self.req_resp):
            req_resp = self.req_resp[self.req_count]
            if method == req_resp.method and request.path == req_resp.path:
                self.req_count += 1
                SequentialHandler._process_req_resp(req_resp, request)
                return
        else:
            if (method, request.path) in self.req_resp_map:
                req_resp = self.req_resp_map[(method, request.path)]
                del self.req_resp_map[(method, request.path)]
                SequentialHandler._process_req_resp(req_resp, request)
                return

        request.send_error(500, 'Unexpected %s request for %s, req_count = %d' % (method, request.path, self.req_count))

    def do_HEAD(self, request):
        self.process('HEAD', request)

    def do_GET(self, request):
        self.process('GET', request)

    def do_PATCH(self, request):
        self.process('PATCH', request)

    def do_POST(self, request):
        self.process('POST', request)

    def do_PUT(self, request):
        self.process('PUT', request)

    def do_DELETE(self, request):
        self.process('DELETE', request)


class DispatcherHttpHandler(BaseHTTPRequestHandler):

    # protocol_version = 'HTTP/1.1'

    def log_request(self, code='-', size='-'):
        # pylint: disable=unused-argument
        pass

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

    def do_PATCH(self):
        if do_log:
            f = open('/tmp/log.txt', 'a')
            f.write('PATCH %s\n' % self.path)
            f.close()

        custom_handler.do_PATCH(self)

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
    # pylint: disable=unused-argument
    def log_request(self, code='-', size='-'):
        pass

    def do_HEAD(self):
        if do_log:
            f = open('/tmp/log.txt', 'a')
            f.write('HEAD %s\n' % self.path)
            f.close()

        self.send_error(404, 'File Not Found: %s' % self.path)

    def do_DELETE(self):
        if do_log:
            f = open('/tmp/log.txt', 'a')
            f.write('DELETE %s\n' % self.path)
            f.close()

        self.send_error(404, 'File Not Found: %s' % self.path)

    def do_PATCH(self):
        if do_log:
            f = open('/tmp/log.txt', 'a')
            f.write('PATCH %s\n' % self.path)
            f.close()

        self.send_error(404, 'File Not Found: %s' % self.path)

    def do_POST(self):
        if do_log:
            f = open('/tmp/log.txt', 'a')
            f.write('POST %s\n' % self.path)
            f.close()

        self.send_error(404, 'File Not Found: %s' % self.path)

    def do_PUT(self):
        if do_log:
            f = open('/tmp/log.txt', 'a')
            f.write('PUT %s\n' % self.path)
            f.close()

        self.send_error(404, 'File Not Found: %s' % self.path)

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
                # sys.stderr.write('stop requested\n')
                self.server.stop_requested = True
                return

            return
        except IOError:
            pass

        self.send_error(404, 'File Not Found: %s' % self.path)


class GDAL_HttpServer(HTTPServer):

    def __init__(self, server_address, handlerClass):
        HTTPServer.__init__(self, server_address, handlerClass)
        self.running = False
        self.stop_requested = False

    def is_running(self):
        return self.running

    def stop_server(self):
        if self.running:
            self.shutdown()
        self.running = False

    def serve_until_stop_server(self):
        self.running = True
        self.serve_forever(0.25)
        self.running = False
        self.stop_requested = False


class GDAL_ThreadedHttpServer(Thread):

    def __init__(self, handlerClass=None):
        Thread.__init__(self)
        ok = False
        self.server = 0
        if handlerClass is None:
            handlerClass = GDAL_Handler
        for port in range(8080, 8100):
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
            # print(count)
            # print(self.server.is_running())
            time.sleep(0.5)
            count = count + 0.5
        self.stop()


def launch(fork_process=None, handler=None):
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

    process = subprocess.Popen(
        [python_exe, '../pymod/webserver.py'],
        stdout=subprocess.PIPE)
    if process is None:
        return (None, 0)

    line = process.stdout.readline()
    line = line.decode('ascii')
    process.stdout.close()
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
    process.wait()


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
