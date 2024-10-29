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
# SPDX-License-Identifier: MIT
###############################################################################

import contextlib
import os
import subprocess
import sys
import time
from http.server import BaseHTTPRequestHandler, HTTPServer
from threading import Thread

import gdaltest

from osgeo import gdal

do_log = False
custom_handler = None


@contextlib.contextmanager
def install_http_handler(handler_instance):
    global custom_handler
    custom_handler = handler_instance
    try:
        yield
        handler_instance.final_check()
    finally:
        custom_handler = None


class RequestResponse(object):
    def __init__(
        self,
        method,
        path,
        code,
        headers=None,
        body=None,
        custom_method=None,
        expected_headers=None,
        expected_body=None,
        add_content_length_header=True,
        unexpected_headers=[],
        silence_server_exception=False,
    ):
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
        self.silence_server_exception = silence_server_exception

    def __repr__(self):
        return (
            f"RequestResponse({self.method}, {self.path}, {self.code}, headers={self.headers}, "
            f"body={self.body}, custom_method={self.custom_method}, "
            f"expected_headers={self.expected_headers}, expected_body={self.expected_body}, "
            f"add_content_length_header={self.add_content_length_header}, "
            f"unexpected_headers={self.unexpected_headers}"
        )


class FileHandler(object):
    """
    Handler that serves files from a dictionary and/or a fallback VSI location.
    """

    def __init__(self, _dict, content_type=None):
        self.dict = {"GET": _dict, "PUT": {}, "POST": {}, "DELETE": {}}
        self.content_type = content_type
        self.fallback = None

    def final_check(self):
        pass

    def handle_get(self, path, contents):
        self.add_file(path, contents, verb="GET")

    def handle_put(self, path, contents):
        self.add_file(path, contents, verb="PUT")

    def handle_delete(self, path, contents):
        self.add_file(path, contents, verb="DELETE")

    def handle_post(self, path, contents, post_body=None):
        self.add_file(path, contents, verb="POST", post_body=post_body)

    def add_file(self, path, contents, verb="GET", post_body=None):
        if type(contents) is str:
            contents = contents.encode()

        if type(post_body) is str:
            post_body = post_body.encode()

        if verb == "POST":
            if path in self.dict["POST"]:
                self.dict["POST"][path][post_body] = contents
            else:
                self.dict["POST"][path] = {post_body: contents}
        else:
            self.dict[verb][path] = contents

    def set_fallback(self, path):
        self.fallback = path

    def lookup(self, path, verb, post_body=None):

        if path in self.dict[verb]:
            if verb == "POST":
                return self.dict["POST"][path].get(post_body, None)
            else:
                return self.dict[verb][path]

        if verb == "GET" and self.fallback:
            stat = gdal.VSIStatL(f"{self.fallback}/{path}")
            if stat is not None:
                f = gdal.VSIFOpenL(f"{self.fallback}/{path}", "rb")
                content = gdal.VSIFReadL(1, stat.size, f)
                gdal.VSIFCloseL(f)

                return content

    def send_response(self, request, filedata):
        if filedata is None:
            request.send_response(404)
            request.end_headers()
        else:
            start = 0
            end = len(filedata)
            if "Range" in request.headers:
                import re

                res = re.search(r"bytes=(\d+)\-(\d+)", request.headers["Range"])
                if res:
                    res = res.groups()
                    start = int(res[0])
                    end = int(res[1]) + 1
                    if end > len(filedata):
                        end = len(filedata)
            try:
                data_slice = filedata[start:end]
                request.send_response(200)
                if "Range" in request.headers:
                    request.send_header("Content-Range", "%d-%d" % (start, end - 1))
                request.send_header("Content-Length", len(filedata))
                if self.content_type:
                    request.send_header("Content-Type", self.content_type)
                request.end_headers()
                request.wfile.write(data_slice)
            except Exception as ex:
                request.send_response(500)
                request.end_headers()
                request.wfile.write(str(ex).encode("utf8"))

    def do_HEAD(self, request):
        filedata = self.lookup(request.path, "GET")

        if filedata is None:
            request.send_response(404)
            request.end_headers()
        else:
            request.send_response(200)
            request.send_header("Content-Length", len(filedata))
            request.end_headers()

    def do_GET(self, request):
        filedata = self.lookup(request.path, "GET")
        self.send_response(request, filedata)

    def do_PUT(self, request):
        filedata = self.lookup(request.path, "PUT")
        self.send_response(request, filedata)

    def do_POST(self, request):
        content = request.rfile.read(int(request.headers["Content-Length"]))
        filedata = self.lookup(request.path, "POST", post_body=content)
        self.send_response(request, filedata)

    def do_DELETE(self, request):
        filedata = self.lookup(request.path, "DELETE")
        self.send_response(request, filedata)


class BaseMockedHttpHandler(object):
    @staticmethod
    def _process_req_resp(req_resp, request):
        if req_resp.custom_method:
            if req_resp.silence_server_exception:
                try:
                    req_resp.custom_method(request)
                except Exception:
                    pass
            else:
                req_resp.custom_method(request)
        else:

            if req_resp.expected_headers:
                for k in req_resp.expected_headers:
                    if (
                        k not in request.headers
                        or request.headers[k] != req_resp.expected_headers[k]
                    ):
                        sys.stderr.write(
                            "Did not get expected headers: %s\n" % str(request.headers)
                        )
                        request.send_response(400)
                        request.send_header("Content-Length", 0)
                        request.end_headers()
                        return

            for k in req_resp.unexpected_headers:
                if k in request.headers:
                    sys.stderr.write("Did not expect header: %s\n" % k)
                    request.send_response(400)
                    request.send_header("Content-Length", 0)
                    request.end_headers()
                    return

            if req_resp.expected_body:
                content = request.rfile.read(int(request.headers["Content-Length"]))
                if content != req_resp.expected_body:
                    sys.stderr.write("Did not get expected content: %s\n" % content)
                    request.send_response(400)
                    request.send_header("Content-Length", 0)
                    request.end_headers()
                    return

            request.send_response(req_resp.code)
            for k in req_resp.headers:
                request.send_header(k, req_resp.headers[k])
            if (
                req_resp.add_content_length_header
                and "Content-Length" not in req_resp.headers
            ):
                if req_resp.body:
                    request.send_header("Content-Length", len(req_resp.body))
                else:
                    request.send_header("Content-Length", "0")
            request.end_headers()
            if req_resp.body:
                try:
                    request.wfile.write(req_resp.body)
                except Exception:
                    request.wfile.write(req_resp.body.encode("ascii"))

    def do_HEAD(self, request):
        self.process("HEAD", request)

    def do_GET(self, request):
        self.process("GET", request)

    def do_PATCH(self, request):
        self.process("PATCH", request)

    def do_POST(self, request):
        self.process("POST", request)

    def do_PUT(self, request):
        self.process("PUT", request)

    def do_DELETE(self, request):
        self.process("DELETE", request)


class SequentialHandler(BaseMockedHttpHandler):
    def __init__(self):
        self.req_count = 0
        self.req_resp = []

    def final_check(self):
        assert self.req_count == len(self.req_resp), (
            self.req_count,
            len(self.req_resp),
        )

    def add(
        self,
        method,
        path,
        code=None,
        headers=None,
        body=None,
        custom_method=None,
        expected_headers=None,
        expected_body=None,
        add_content_length_header=True,
        unexpected_headers=[],
        silence_server_exception=False,
    ):
        hdrs = {} if headers is None else headers
        expected_hdrs = {} if expected_headers is None else expected_headers
        self.req_resp.append(
            RequestResponse(
                method,
                path,
                code,
                hdrs,
                body,
                custom_method,
                expected_hdrs,
                expected_body,
                add_content_length_header,
                unexpected_headers,
                silence_server_exception,
            )
        )

    def process(self, method, request):
        if self.req_count < len(self.req_resp):
            req_resp = self.req_resp[self.req_count]
            if method == req_resp.method and request.path == req_resp.path:
                self.req_count += 1
                SequentialHandler._process_req_resp(req_resp, request)
                return

        request.send_error(
            500,
            "Unexpected %s request for %s, req_count = %d"
            % (method, request.path, self.req_count),
        )


class NonSequentialMockedHttpHandler(BaseMockedHttpHandler):
    def __init__(self):
        self.req_resp_map = {}

    def final_check(self):
        assert not self.req_resp_map

    def add(
        self,
        method,
        path,
        code=None,
        headers=None,
        body=None,
        custom_method=None,
        expected_headers=None,
        expected_body=None,
        add_content_length_header=True,
        unexpected_headers=[],
        silence_server_exception=False,
    ):
        hdrs = {} if headers is None else headers
        expected_hdrs = {} if expected_headers is None else expected_headers
        self.req_resp_map[(method, path)] = RequestResponse(
            method,
            path,
            code,
            hdrs,
            body,
            custom_method,
            expected_hdrs,
            expected_body,
            add_content_length_header,
            unexpected_headers,
            silence_server_exception,
        )

    def process(self, method, request):

        if (method, request.path) in self.req_resp_map:
            req_resp = self.req_resp_map[(method, request.path)]
            del self.req_resp_map[(method, request.path)]
            SequentialHandler._process_req_resp(req_resp, request)
            return

        request.send_error(
            500,
            "Unexpected %s request for %s, req_count = %d"
            % (method, request.path, len(self.req_resp_map)),
        )


class DispatcherHttpHandler(BaseHTTPRequestHandler):

    # protocol_version = 'HTTP/1.1'

    def log_request(self, code="-", size="-"):
        # pylint: disable=unused-argument
        pass

    def do_HEAD(self):

        if do_log:
            f = open("/tmp/log.txt", "a")
            f.write("HEAD %s\n" % self.path)
            f.close()

        custom_handler.do_HEAD(self)

    def do_DELETE(self):

        if do_log:
            f = open("/tmp/log.txt", "a")
            f.write("DELETE %s\n" % self.path)
            f.close()

        custom_handler.do_DELETE(self)

    def do_PATCH(self):
        if do_log:
            f = open("/tmp/log.txt", "a")
            f.write("PATCH %s\n" % self.path)
            f.close()

        custom_handler.do_PATCH(self)

    def do_POST(self):

        if do_log:
            f = open("/tmp/log.txt", "a")
            f.write("POST %s\n" % self.path)
            f.close()

        custom_handler.do_POST(self)

    def do_PUT(self):

        if do_log:
            f = open("/tmp/log.txt", "a")
            f.write("PUT %s\n" % self.path)
            f.close()

        custom_handler.do_PUT(self)

    def do_GET(self):

        if do_log:
            f = open("/tmp/log.txt", "a")
            f.write("GET %s\n" % self.path)
            f.close()

        custom_handler.do_GET(self)


class GDAL_Handler(BaseHTTPRequestHandler):
    # pylint: disable=unused-argument
    def log_request(self, code="-", size="-"):
        pass

    def do_HEAD(self):
        if do_log:
            f = open("/tmp/log.txt", "a")
            f.write("HEAD %s\n" % self.path)
            f.close()

        self.send_error(404, "File Not Found: %s" % self.path)

    def do_DELETE(self):
        if do_log:
            f = open("/tmp/log.txt", "a")
            f.write("DELETE %s\n" % self.path)
            f.close()

        self.send_error(404, "File Not Found: %s" % self.path)

    def do_PATCH(self):
        if do_log:
            f = open("/tmp/log.txt", "a")
            f.write("PATCH %s\n" % self.path)
            f.close()

        self.send_error(404, "File Not Found: %s" % self.path)

    def do_POST(self):
        if do_log:
            f = open("/tmp/log.txt", "a")
            f.write("POST %s\n" % self.path)
            f.close()

        self.send_error(404, "File Not Found: %s" % self.path)

    def do_PUT(self):
        if do_log:
            f = open("/tmp/log.txt", "a")
            f.write("PUT %s\n" % self.path)
            f.close()

        self.send_error(404, "File Not Found: %s" % self.path)

    def do_GET(self):

        try:
            if do_log:
                f = open("/tmp/log.txt", "a")
                f.write("GET %s\n" % self.path)
                f.close()

            if self.path == "/shutdown":
                self.send_response(200)
                self.send_header("Content-type", "text/html")
                self.end_headers()
                # sys.stderr.write('stop requested\n')
                self.server.stop_requested = True
                return

            return
        except IOError:
            pass

        self.send_error(404, "File Not Found: %s" % self.path)


class GDAL_HttpServer(HTTPServer):
    def __init__(self, server_address, handlerClass):
        HTTPServer.__init__(self, server_address, handlerClass)
        self.running = False
        self.stop_requested = False

    def server_bind(self):
        # From https://bugs.python.org/issue41135
        # Needed on Windows so that we don't start as server on a port already
        # occupied
        import socket

        if hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
            HTTPServer.allow_reuse_address = 0
        HTTPServer.server_bind(self)

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
        for port in range(int(os.environ.get("GDAL_TEST_HTTP_PORT", "8080")), 8100):
            try:
                self.server = GDAL_HttpServer(("", port), handlerClass)
                self.server.port = port
                ok = True
                break
            except Exception:
                pass
        if not ok:
            raise Exception("could not start server")

    def getPort(self):
        return self.server.port

    def run(self):
        try:
            self.server.serve_until_stop_server()
        except KeyboardInterrupt:
            print("^C received, shutting down server")
            self.server.socket.close()

    def start_and_wait_ready(self):
        if self.server.running:
            raise Exception("server already started")
        self.start()
        while not self.server.running:
            time.sleep(1)

    def stop(self):
        self.server.stop_server()
        # Explicitly destroy the object so that the socket is really closed
        del self.server

    def run_server(self, timeout):
        if not self.server.running:
            raise Exception("server not started")
        count = 0
        while (
            (timeout <= 0 or count < timeout)
            and self.server.running
            and not self.server.stop_requested
        ):
            # print(count)
            # print(self.server.is_running())
            time.sleep(0.5)
            count = count + 0.5
        self.stop()


def launch(fork_process=None, handler=None):
    if handler is not None:
        if fork_process:
            raise Exception("fork_process = True incompatible with custom handler")
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
        except Exception:
            return (None, 0)

    python_exe = sys.executable
    if sys.platform == "win32":
        python_exe = python_exe.replace("\\", "/")

    process = subprocess.Popen(
        [python_exe, "../pymod/webserver.py"], stdout=subprocess.PIPE
    )
    if process is None:
        return (None, 0)

    line = process.stdout.readline()
    line = line.decode("ascii")
    process.stdout.close()
    if line.find("port=") == -1:
        return (None, 0)

    port = int(line[5:])
    if port != 0:
        print("HTTP Server started on port %d" % port)

    return (process, port)


def server_stop(process, port):

    if isinstance(process, GDAL_ThreadedHttpServer):
        process.stop()
        return

    gdaltest.gdalurlopen("http://127.0.0.1:%d/shutdown" % port)
    process.wait()


def main():
    try:
        server = GDAL_ThreadedHttpServer(GDAL_Handler)
        server.start_and_wait_ready()
        print("port=%d" % server.getPort())
        sys.stdout.flush()
    except Exception:
        print("port=0")
        sys.stdout.flush()
        sys.exit(0)

    server.run_server(10)


if __name__ == "__main__":
    main()
