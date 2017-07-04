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

import time
import sys
import gdaltest
from sys import version_info

do_log = False

s3_no_cached_test_changed_content = False
test_retry_attempt = 0

TIME_SKEW = 30 * 60

class GDAL_Handler(BaseHTTPRequestHandler):

    def log_request(self, code='-', size='-'):
        return

    def do_HEAD(self):
        if do_log:
            f = open('/tmp/log.txt', 'a')
            f.write('HEAD %s\n' % self.path)
            f.close()

        if self.path == '/s3_fake_bucket/resource2.bin':
            self.send_response(200)
            self.send_header('Content-type', 'text/plain')
            self.send_header('Content-Length', 1000000)
            self.end_headers()
            return

        if self.path == '/gs_fake_bucket/resource2.bin':
            self.send_response(200)
            self.send_header('Content-type', 'text/plain')
            self.send_header('Content-Length', 1000000)
            self.end_headers()
            return

        # Simulate a redirect to a S3 signed URL
        if self.path == '/test_redirect/test.bin':
            import time
            # Simulate a big time difference between server and local machine
            current_time = 1500
            response = 'HTTP/1.1 302 FOUND\r\n'
            response += 'Server: foo\r\n'
            response += 'Date: ' + time.strftime("%a, %d %b %Y %H:%M:%S GMT", time.gmtime(current_time)) + '\r\n'
            response += 'Location: http://localhost:%d/foo.s3.amazonaws.com/test_redirected/test.bin?Signature=foo&Expires=%d\r\n' % (self.server.port, current_time + 30)
            response += '\r\n'
            self.wfile.write(response.encode('ascii'))
            return

        # Simulate that we don't accept HEAD on signed URLs. The client should retry with a GET
        if self.path.startswith('/foo.s3.amazonaws.com/test_redirected/test.bin?Signature=foo&Expires='):
            import time
            # Simulate a big time difference between server and local machine
            current_time = 1500
            response = 'HTTP/1.1 403\r\n'
            response += 'Server: foo\r\n'
            response += 'Date: ' + time.strftime("%a, %d %b %Y %H:%M:%S GMT", time.gmtime(current_time)) + '\r\n'
            response += '\r\n'
            self.wfile.write(response.encode('ascii'))
            return

        if self.path == '/test_retry/test.txt':
            self.send_response(200)
            self.send_header('Content-type', 'text/plain')
            self.send_header('Content-Length', 3)
            self.end_headers()
            return

        if self.path == '/test_retry_reset_counter':
            global test_retry_attempt
            test_retry_attempt = 0
            self.send_response(200)
            self.end_headers()
            return

        self.send_error(404,'File Not Found: %s' % self.path)

    def do_DELETE(self):
        if do_log:
            f = open('/tmp/log.txt', 'a')
            f.write('DELETE %s\n' % self.path)
            f.close()

        if self.path == '/s3_delete_bucket/delete_file':
            self.send_response(204)
            self.end_headers()
            return

        if self.path == '/s3_delete_bucket/delete_file_error':
            self.send_response(403)
            self.end_headers()
            return

        if self.path == '/s3_delete_bucket/redirect':
            self.protocol_version = 'HTTP/1.1'
            if self.headers['Authorization'].find('us-east-1') >= 0:
                self.send_response(400)
                response = '<?xml version="1.0" encoding="UTF-8"?><Error><Message>bla</Message><Code>AuthorizationHeaderMalformed</Code><Region>us-west-2</Region></Error>'
                response = '%x\r\n%s' % (len(response), response)
                self.send_header('Content-type', 'application/xml')
                self.send_header('Transfer-Encoding', 'chunked')
                self.end_headers()
                self.wfile.write(response.encode('ascii'))
            elif self.headers['Authorization'].find('us-west-2') >= 0:
                self.send_response(204)
                self.end_headers()
            else:
                sys.stderr.write('Bad headers: %s\n' % str(self.headers))
                self.send_response(403)

            return

        if self.path == '/s3_fake_bucket4/large_file_upload_part_403_error.bin?uploadId=my_id' or \
           self.path == '/s3_fake_bucket4/large_file_upload_part_no_etag.bin?uploadId=my_id':
            self.send_response(204)
            self.end_headers()
            return

        self.send_error(404,'File Not Found: %s' % self.path)

    def do_POST(self):
        if do_log:
            f = open('/tmp/log.txt', 'a')
            f.write('POST %s\n' % self.path)
            f.close()

        if self.path == '/s3_fake_bucket4/large_file.bin?uploads' or \
           self.path == '/s3_fake_bucket4/large_file_upload_part_403_error.bin?uploads' or \
           self.path == '/s3_fake_bucket4/large_file_upload_part_no_etag.bin?uploads':

            self.protocol_version = 'HTTP/1.1'
            if self.headers['Authorization'].find('us-east-1') >= 0:
                self.send_response(400)
                response = '<?xml version="1.0" encoding="UTF-8"?><Error><Message>bla</Message><Code>AuthorizationHeaderMalformed</Code><Region>us-west-2</Region></Error>'
                response = '%x\r\n%s' % (len(response), response)
                self.send_header('Content-type', 'application/xml')
                self.send_header('Transfer-Encoding', 'chunked')
                self.end_headers()
                self.wfile.write(response.encode('ascii'))
            elif self.headers['Authorization'].find('us-west-2') >= 0:
                response = '<?xml version="1.0" encoding="UTF-8"?><InitiateMultipartUploadResult><UploadId>my_id</UploadId></InitiateMultipartUploadResult>'
                self.send_response(200)
                self.send_header('Content-type', 'application/xml')
                self.send_header('Content-Length', len(response))
                self.end_headers()
                self.wfile.write(response.encode('ascii'))
            else:
                sys.stderr.write('Bad headers: %s\n' % str(self.headers))
                self.send_response(403)

            return

        if self.path == '/s3_fake_bucket4/large_file.bin?uploadId=my_id':

            if self.headers['Content-Length'] != '186':
                sys.stderr.write('Did not get expected headers: %s\n' % str(self.headers))
                self.send_response(400)
                return

            content = self.rfile.read(186).decode('ascii')
            if content != """<CompleteMultipartUpload>
<Part>
<PartNumber>1</PartNumber><ETag>"first_etag"</ETag></Part>
<Part>
<PartNumber>2</PartNumber><ETag>"second_etag"</ETag></Part>
</CompleteMultipartUpload>
""":
                sys.stderr.write('Did not get expected content: %s\n' % content)
                self.send_response(400)
                return

            self.send_response(200)
            self.end_headers()
            return

        if self.path == '/s3_fake_bucket4/large_file_initiate_403_error.bin?uploads':
            self.send_response(403)
            self.end_headers()
            return

        if self.path == '/s3_fake_bucket4/large_file_initiate_empty_result.bin?uploads':
            self.send_response(200)
            self.end_headers()
            return

        if self.path == '/s3_fake_bucket4/large_file_initiate_invalid_xml_result.bin?uploads':
            self.send_response(200)
            self.end_headers()
            self.wfile.write('foo'.encode('ascii'))
            return

        if self.path == '/s3_fake_bucket4/large_file_initiate_no_uploadId.bin?uploads':
            self.send_response(200)
            self.end_headers()
            self.wfile.write('<foo/>'.encode('ascii'))
            return

        self.send_error(404,'File Not Found: %s' % self.path)

    def do_PUT(self):
        if do_log:
            f = open('/tmp/log.txt', 'a')
            f.write('PUT %s\n' % self.path)
            f.close()

        if self.path == '/s3_fake_bucket3/empty_file.bin':

            if self.headers['Content-Length'] != '0':
                sys.stderr.write('Did not get expected headers: %s\n' % str(self.headers))
                self.send_response(400)
                return

            self.send_response(200)
            self.end_headers()
            return

        if self.path == '/s3_fake_bucket3/empty_file_error.bin':
            self.send_response(403)
            self.end_headers()
            return

        if self.path == '/s3_fake_bucket3/another_file.bin':

            if self.headers['Content-Length'] != '6':
                sys.stderr.write('Did not get expected headers: %s\n' % str(self.headers))
                self.send_response(400)
                return

            content = self.rfile.read(6).decode('ascii')
            if content != 'foobar':
                sys.stderr.write('Did not get expected content: %s\n' % content)
                self.send_response(400)
                return

            self.send_response(200)
            self.end_headers()
            return

        if self.path == '/s3_fake_bucket3/redirect':
            self.protocol_version = 'HTTP/1.1'
            if self.headers['Authorization'].find('us-east-1') >= 0:
                self.send_response(400)
                response = '<?xml version="1.0" encoding="UTF-8"?><Error><Message>bla</Message><Code>AuthorizationHeaderMalformed</Code><Region>us-west-2</Region></Error>'
                response = '%x\r\n%s' % (len(response), response)
                self.send_header('Content-type', 'application/xml')
                self.send_header('Transfer-Encoding', 'chunked')
                self.end_headers()
                self.wfile.write(response.encode('ascii'))
            elif self.headers['Authorization'].find('us-west-2') >= 0:
                if self.headers['Content-Length'] != '6':
                    sys.stderr.write('Did not get expected headers: %s\n' % str(self.headers))
                    self.send_response(400)
                    return
                self.send_response(200)
                self.end_headers()
            else:
                sys.stderr.write('Bad headers: %s\n' % str(self.headers))
                self.send_response(403)

            return

        if self.path == '/s3_fake_bucket4/large_file.bin?partNumber=1&uploadId=my_id':
            if self.headers['Content-Length'] != '1048576':
                sys.stderr.write('Did not get expected headers: %s\n' % str(self.headers))
                self.send_response(400)
                return
            self.send_response(200)
            self.send_header('ETag', '"first_etag"')
            self.end_headers()
            return

        if self.path == '/s3_fake_bucket4/large_file.bin?partNumber=2&uploadId=my_id':
            if self.headers['Content-Length'] != '1':
                sys.stderr.write('Did not get expected headers: %s\n' % str(self.headers))
                self.send_response(400)
                return
            self.send_response(200)
            self.send_header('ETag', '"second_etag"')
            self.end_headers()
            return

        if self.path == '/s3_fake_bucket4/large_file_upload_part_403_error.bin?partNumber=1&uploadId=my_id':
            self.send_response(403)
            self.end_headers()
            return

        if self.path == '/s3_fake_bucket4/large_file_upload_part_no_etag.bin?partNumber=1&uploadId=my_id':
            self.send_response(200)
            self.end_headers()
            return

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

            if self.path.startswith('/vsimem/'):
                from osgeo import gdal
                f = gdal.VSIFOpenL(self.path, "rb")
                if f is None:
                    self.send_response(404)
                    self.end_headers()
                else:
                    gdal.VSIFSeekL(f, 0, 2)
                    size = gdal.VSIFTellL(f)
                    gdal.VSIFSeekL(f, 0, 0)
                    content = gdal.VSIFReadL(1, size, f)
                    gdal.VSIFCloseL(f)
                    self.protocol_version = 'HTTP/1.0'
                    self.send_response(200)
                    self.end_headers()
                    self.wfile.write(content)
                return

            # First signed URL
            if self.path.startswith('/foo.s3.amazonaws.com/test_redirected/test.bin?Signature=foo&Expires='):
                if 'Range' in self.headers:
                    if self.headers['Range'] == 'bytes=0-16383':
                        self.protocol_version = 'HTTP/1.1'
                        self.send_response(200)
                        self.send_header('Content-type', 'text/plain')
                        self.send_header('Content-Range', 'bytes 0-16383/1000000')
                        self.end_headers()
                        self.wfile.write(''.join(['x' for i in range(16384)]).encode('ascii'))
                    elif self.headers['Range'] == 'bytes=16384-49151':
                        # Test expiration of the signed URL
                        self.protocol_version = 'HTTP/1.1'
                        self.send_response(403)
                        self.end_headers()
                    else:
                        self.send_response(404)
                        self.end_headers()
                else:
                    # After a failed attempt on a HEAD, the client should go there
                    import time
                    # Simulate a big time difference between server and local machine
                    current_time = 1500
                    response = 'HTTP/1.1 200\r\n'
                    response += 'Server: foo\r\n'
                    response += 'Date: ' + time.strftime("%a, %d %b %Y %H:%M:%S GMT", time.gmtime(current_time)) + '\r\n'
                    response += 'Content-type: text/plain\r\n'
                    response += 'Content-Length: 1000000\r\n'
                    response += '\r\n'
                    self.wfile.write(response.encode('ascii'))
                return

            # Second signed URL
            if self.path.startswith('/foo.s3.amazonaws.com/test_redirected2/test.bin?Signature=foo&Expires=') and 'Range' in self.headers and \
               self.headers['Range'] == 'bytes=16384-49151':
                self.protocol_version = 'HTTP/1.1'
                self.send_response(200)
                self.send_header('Content-type', 'text/plain')
                self.send_header('Content-Range', 'bytes 16384-16384/1000000')
                self.end_headers()
                self.wfile.write('y'.encode('ascii'))
                return

            # We should go there after expiration of the first signed URL
            if self.path == '/test_redirect/test.bin' and 'Range' in self.headers and \
               self.headers['Range'] == 'bytes=16384-49151':
                self.protocol_version = 'HTTP/1.1'
                self.send_response(302)
                # Return a new signed URL
                import time
                current_time = int(time.time())
                self.send_header('Location', 'http://localhost:%d/foo.s3.amazonaws.com/test_redirected2/test.bin?Signature=foo&Expires=%d' % (self.server.port, current_time + 30))
                self.end_headers()
                self.wfile.write(''.join(['x' for i in range(16384)]).encode('ascii'))
                return

            if self.path == '/s3_delete_bucket/delete_file' and getattr(self.server, 'has_requested_s3_delete_bucket_delete_file', None) is None:
                self.server.has_requested_s3_delete_bucket_delete_file = True
                self.protocol_version = 'HTTP/1.1'
                self.send_response(200)
                self.send_header('Content-type', 'text/plain')
                self.send_header('Content-Length', 3)
                self.end_headers()
                self.wfile.write("""foo""".encode('ascii'))
                return

            if self.path == '/s3_fake_bucket3/empty_file.bin':
                self.protocol_version = 'HTTP/1.1'
                self.send_response(200)
                if getattr(self.server, 'has_requested_s3_fake_bucket3_empty_file_bin', None) is None:
                    self.server.has_requested_s3_fake_bucket3_empty_file_bin = True
                    self.send_header('Content-type', 'text/plain')
                    self.send_header('Content-Length', 3)
                    self.end_headers()
                    self.wfile.write("""foo""".encode('ascii'))
                else:
                    self.send_header('Content-type', 'text/plain')
                    self.send_header('Content-Length', 0)
                    self.end_headers()
                return

            if self.path == '/s3_fake_bucket/resource':
                self.protocol_version = 'HTTP/1.1'

                if 'Authorization' not in self.headers:
                    sys.stderr.write('Bad headers: %s\n' % str(self.headers))
                    self.send_response(403)
                    return
                expected_authorization_8080 = 'AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date,Signature=38901846b865b12ac492bc005bb394ca8d60c098b68db57c084fac686a932f9e'
                expected_authorization_8081 = 'AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date,Signature=9f623b7ffce76188a456c70fb4813eb31969e88d130d6b4d801b3accbf050d6c'
                if self.headers['Authorization'] != expected_authorization_8080 and self.headers['Authorization'] != expected_authorization_8081:
                    sys.stderr.write("Bad Authorization: '%s'\n" % str(self.headers['Authorization']))
                    self.send_response(403)
                    return

                self.send_response(200)
                self.send_header('Content-type', 'text/plain')
                self.send_header('Content-Length', 3)
                self.end_headers()
                self.wfile.write("""foo""".encode('ascii'))
                return

            if self.path == '/s3_fake_bucket/bar':
                self.protocol_version = 'HTTP/1.1'
                self.send_response(200)
                self.send_header('Content-type', 'text/plain')
                self.send_header('Content-Length', 3)
                self.end_headers()
                self.wfile.write("""bar""".encode('ascii'))
                return

            if self.path == '/s3_fake_bucket_with_session_token/resource':
                self.protocol_version = 'HTTP/1.1'

                if 'Authorization' not in self.headers:
                    sys.stderr.write('Bad headers: %s\n' % str(self.headers))
                    self.send_response(403)
                    return
                expected_authorization_8080 = 'AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date;x-amz-security-token,Signature=464a21835038b4f4d292b6463b8a005b9aaa980513aa8c42fc170abb733dce85'
                expected_authorization_8081 = 'AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date;x-amz-security-token,Signature=b10e91575186342f9f2acfc91c4c2c9938c4a9e8cdcbc043d09d59d9641ad7fb'
                if self.headers['Authorization'] != expected_authorization_8080 and self.headers['Authorization'] != expected_authorization_8081:
                    sys.stderr.write("Bad Authorization: '%s'\n" % str(self.headers['Authorization']))
                    self.send_response(403)
                    return

                self.send_response(200)
                self.send_header('Content-type', 'text/plain')
                self.send_header('Content-Length', 3)
                self.end_headers()
                self.wfile.write("""foo""".encode('ascii'))
                return

            if self.path == '/s3_fake_bucket_with_requester_pays/resource':
                self.protocol_version = 'HTTP/1.1'

                if 'x-amz-request-payer' not in self.headers:
                    sys.stderr.write('Bad headers: %s\n' % str(self.headers))
                    self.send_response(403)
                    return
                expected_authorization_8080 = 'AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date;x-amz-request-payer,Signature=cf713a394e1b629ac0e468d60d3d4a12f5236fd72d21b6005c758b0dfc7049cd'
                expected_authorization_8081 = 'AWS4-HMAC-SHA256 Credential=AWS_ACCESS_KEY_ID/20150101/us-east-1/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date;x-amz-request-payer,Signature=4756166679008a1a40cd6ff91dbbef670a71c11bf8e3c998dd7385577c3ac4d9'
                if self.headers['Authorization'] != expected_authorization_8080 and self.headers['Authorization'] != expected_authorization_8081:
                    sys.stderr.write("Bad Authorization: '%s'\n" % str(self.headers['Authorization']))
                    self.send_response(403)
                    return
                if self.headers['x-amz-request-payer'] != 'requester':
                    sys.stderr.write("Bad x-amz-request-payer: '%s'\n" % str(self.headers['x-amz-request-payer']))
                    self.send_response(403)
                    return

                self.send_response(200)
                self.send_header('Content-type', 'text/plain')
                self.send_header('Content-Length', 3)
                self.end_headers()
                self.wfile.write("""foo""".encode('ascii'))
                return


            if self.path == '/s3_fake_bucket/resource2.bin':
                self.protocol_version = 'HTTP/1.1'
                if 'Range' in self.headers:
                    if self.headers['Range'] != 'bytes=0-4095':
                        sys.stderr.write("Bad Range: '%s'\n" % str(self.headers['Range']))
                        self.send_response(403)
                        return
                    self.send_response(206)
                    self.send_header('Content-type', 'text/plain')
                    self.send_header('Content-Range', 'bytes 0-4095/1000000')
                    self.send_header('Content-Length', 4096)
                    self.end_headers()
                    self.wfile.write(''.join('a' for i in range(4096)).encode('ascii'))
                else:
                    self.send_response(200)
                    self.send_header('Content-type', 'text/plain')
                    self.send_header('Content-Length', 1000000)
                    self.end_headers()
                    self.wfile.write(''.join('a' for i in range(1000000)).encode('ascii'))
                return

            if self.path == '/s3_fake_bucket/redirect':
                self.protocol_version = 'HTTP/1.1'
                if self.headers['Authorization'].find('us-east-1') >= 0:
                    self.send_response(400)
                    response = '<?xml version="1.0" encoding="UTF-8"?><Error><Message>bla</Message><Code>AuthorizationHeaderMalformed</Code><Region>us-west-2</Region></Error>'
                    response = '%x\r\n%s' % (len(response), response)
                    self.send_header('Content-type', 'application/xml')
                    self.send_header('Transfer-Encoding', 'chunked')
                    self.end_headers()
                    self.wfile.write(response.encode('ascii'))
                elif self.headers['Authorization'].find('us-west-2') >= 0:
                    if self.headers['Host'].startswith('127.0.0.1'):
                        self.send_response(301)
                        response = '<?xml version="1.0" encoding="UTF-8"?><Error><Message>bla</Message><Code>PermanentRedirect</Code><Endpoint>localhost:%d</Endpoint></Error>' % self.server.port
                        response = '%x\r\n%s' % (len(response), response)
                        self.send_header('Content-type', 'application/xml')
                        self.send_header('Transfer-Encoding', 'chunked')
                        self.end_headers()
                        self.wfile.write(response.encode('ascii'))
                    elif self.headers['Host'].startswith('localhost'):
                        self.send_response(200)
                        self.send_header('Content-type', 'text/plain')
                        self.send_header('Content-Length', 3)
                        self.end_headers()
                        self.wfile.write("""foo""".encode('ascii'))
                    else:
                        sys.stderr.write('Bad headers: %s\n' % str(self.headers))
                        self.send_response(403)
                else:
                    sys.stderr.write('Bad headers: %s\n' % str(self.headers))
                    self.send_response(403)

                return

            if self.path == '/s3_fake_bucket/non_xml_error':

                # /vsis3_streaming/ should have remembered the change of region and endpoint
                if self.headers['Authorization'].find('us-west-2') < 0 or \
                   not self.headers['Host'].startswith('localhost'):
                    sys.stderr.write('Bad headers: %s\n' % str(self.headers))
                    self.send_response(403)

                self.protocol_version = 'HTTP/1.1'
                self.send_response(400)
                response = 'bla'
                response = '%x\r\n%s' % (len(response), response)
                self.send_header('Content-type', 'application/xml')
                self.send_header('Transfer-Encoding', 'chunked')
                self.end_headers()
                self.wfile.write(response.encode('ascii'))
                return

            if self.path == '/s3_fake_bucket/invalid_xml_error':
                self.protocol_version = 'HTTP/1.1'
                self.send_response(400)
                response = '<?xml version="1.0" encoding="UTF-8"?><oops>'
                response = '%x\r\n%s' % (len(response), response)
                self.send_header('Content-type', 'application/xml')
                self.send_header('Transfer-Encoding', 'chunked')
                self.end_headers()
                self.wfile.write(response.encode('ascii'))
                return

            if self.path == '/s3_fake_bucket/no_code_in_error':
                self.protocol_version = 'HTTP/1.1'
                self.send_response(400)
                response = '<?xml version="1.0" encoding="UTF-8"?><Error/>'
                response = '%x\r\n%s' % (len(response), response)
                self.send_header('Content-type', 'application/xml')
                self.send_header('Transfer-Encoding', 'chunked')
                self.end_headers()
                self.wfile.write(response.encode('ascii'))
                return

            if self.path == '/s3_fake_bucket/no_region_in_AuthorizationHeaderMalformed_error':
                self.protocol_version = 'HTTP/1.1'
                self.send_response(400)
                response = '<?xml version="1.0" encoding="UTF-8"?><Error><Code>AuthorizationHeaderMalformed</Code></Error>'
                response = '%x\r\n%s' % (len(response), response)
                self.send_header('Content-type', 'application/xml')
                self.send_header('Transfer-Encoding', 'chunked')
                self.end_headers()
                self.wfile.write(response.encode('ascii'))
                return

            if self.path == '/s3_fake_bucket/no_endpoint_in_PermanentRedirect_error':
                self.protocol_version = 'HTTP/1.1'
                self.send_response(400)
                response = '<?xml version="1.0" encoding="UTF-8"?><Error><Code>PermanentRedirect</Code></Error>'
                response = '%x\r\n%s' % (len(response), response)
                self.send_header('Content-type', 'application/xml')
                self.send_header('Transfer-Encoding', 'chunked')
                self.end_headers()
                self.wfile.write(response.encode('ascii'))
                return

            if self.path == '/s3_fake_bucket/no_message_in_error':
                self.protocol_version = 'HTTP/1.1'
                self.send_response(400)
                response = '<?xml version="1.0" encoding="UTF-8"?><Error><Code>bla</Code></Error>'
                response = '%x\r\n%s' % (len(response), response)
                self.send_header('Content-type', 'application/xml')
                self.send_header('Transfer-Encoding', 'chunked')
                self.end_headers()
                self.wfile.write(response.encode('ascii'))
                return

            if self.path == '/s3_fake_bucket2/?delimiter=/&prefix=a_dir/':
                self.protocol_version = 'HTTP/1.1'
                if self.headers['Authorization'].find('us-east-1') >= 0:
                    self.send_response(400)
                    response = '<?xml version="1.0" encoding="UTF-8"?><Error><Message>bla</Message><Code>AuthorizationHeaderMalformed</Code><Region>us-west-2</Region></Error>'
                    response = '%x\r\n%s' % (len(response), response)
                    self.send_header('Content-type', 'application/xml')
                    self.send_header('Transfer-Encoding', 'chunked')
                    self.end_headers()
                    self.wfile.write(response.encode('ascii'))
                elif self.headers['Authorization'].find('us-west-2') >= 0:
                    if self.headers['Host'].startswith('127.0.0.1'):
                        self.send_response(301)
                        response = '<?xml version="1.0" encoding="UTF-8"?><Error><Message>bla</Message><Code>PermanentRedirect</Code><Endpoint>localhost:%d</Endpoint></Error>' % self.server.port
                        response = '%x\r\n%s' % (len(response), response)
                        self.send_header('Content-type', 'application/xml')
                        self.send_header('Transfer-Encoding', 'chunked')
                        self.end_headers()
                        self.wfile.write(response.encode('ascii'))
                    elif self.headers['Host'].startswith('localhost'):
                        self.send_response(200)
                        self.send_header('Content-type', 'application/xml')
                        response = """<?xml version="1.0" encoding="UTF-8"?>
                            <ListBucketResult>
                                <Prefix>a_dir/</Prefix>
                                <NextMarker>bla</NextMarker>
                                <Contents>
                                    <Key>a_dir/resource3.bin</Key>
                                    <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                                    <Size>123456</Size>
                                </Contents>
                            </ListBucketResult>
                        """
                        self.send_header('Content-Length', len(response))
                        self.end_headers()
                        self.wfile.write(response.encode('ascii'))
                    else:
                        sys.stderr.write('Bad headers: %s\n' % str(self.headers))
                        self.send_response(403)
                else:
                    sys.stderr.write('Bad headers: %s\n' % str(self.headers))
                    self.send_response(403)
                return

            if self.path == '/s3_fake_bucket2/?delimiter=/&marker=bla&prefix=a_dir/':

                # /vsis3/ should have remembered the change of region and endpoint
                if self.headers['Authorization'].find('us-west-2') < 0 or \
                   not self.headers['Host'].startswith('localhost'):
                    sys.stderr.write('Bad headers: %s\n' % str(self.headers))
                    self.send_response(403)

                self.protocol_version = 'HTTP/1.1'
                self.send_response(200)
                self.send_header('Content-type', 'application/xml')
                response = """<?xml version="1.0" encoding="UTF-8"?>
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
                """
                self.send_header('Content-Length', len(response))
                self.end_headers()
                self.wfile.write(response.encode('ascii'))
                return


            if self.path == '/s3_non_cached/test.txt':
                global s3_no_cached_test_changed_content
                if s3_no_cached_test_changed_content:
                    content = """bar2""".encode('ascii')
                else:
                    content = """foo""".encode('ascii')
                self.protocol_version = 'HTTP/1.1'
                self.send_response(200)
                self.send_header('Content-type', 'text/plain')
                self.send_header('Content-Length', len(content))
                self.end_headers()
                self.wfile.write(content)
                return

            if self.path == '/s3_non_cached/?delimiter=/':
                self.protocol_version = 'HTTP/1.1'
                self.send_response(200)
                self.send_header('Content-type', 'application/xml')
                if s3_no_cached_test_changed_content:
                    response = """<?xml version="1.0" encoding="UTF-8"?>
                        <ListBucketResult>
                            <Prefix>/</Prefix>
                            <Contents>
                                <Key>/test.txt</Key>
                                <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                                <Size>40</Size>
                            </Contents>
                            <Contents>
                                <Key>/test2.txt</Key>
                                <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                                <Size>40</Size>
                            </Contents>
                        </ListBucketResult>
                    """
                else:
                    response = """<?xml version="1.0" encoding="UTF-8"?>
                        <ListBucketResult>
                            <Prefix>/</Prefix>
                            <Contents>
                                <Key>/test.txt</Key>
                                <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                                <Size>30</Size>
                            </Contents>
                        </ListBucketResult>
                    """

                self.send_header('Content-Length', len(response))
                self.end_headers()
                self.wfile.write(response.encode('ascii'))
                return


            if self.path == '/s3_non_cached_test_use_content_1':
                s3_no_cached_test_changed_content = False
                self.protocol_version = 'HTTP/1.1'
                self.send_response(200)
                self.send_header('Content-type', 'text/plain')
                self.send_header('Content-Length', 2)
                self.end_headers()
                self.wfile.write("""ok""".encode('ascii'))
                return

            if self.path == '/s3_non_cached_test_use_content_2':
                s3_no_cached_test_changed_content = True
                self.protocol_version = 'HTTP/1.1'
                self.send_response(200)
                self.send_header('Content-type', 'text/plain')
                self.send_header('Content-Length', 2)
                self.end_headers()
                self.wfile.write("""ok""".encode('ascii'))
                return

            if self.path == '/latest/meta-data/iam/security-credentials/' or \
               self.path == '/latest/meta-data/iam/security-credentials/expire_in_past/':
                self.protocol_version = 'HTTP/1.1'
                self.send_response(200)
                self.send_header('Content-type', 'text/plain')
                response = 'myprofile'
                self.send_header('Content-Length', len(response))
                self.end_headers()
                self.wfile.write(response.encode('ascii'))
                return

            if self.path == '/latest/meta-data/iam/security-credentials/myprofile':
                self.protocol_version = 'HTTP/1.1'
                self.send_response(200)
                self.send_header('Content-type', 'application/json')
                response = """{
                    "AccessKeyId": "AWS_ACCESS_KEY_ID",
                    "SecretAccessKey": "AWS_SECRET_ACCESS_KEY",
                    "Expiration": "3000-01-01T00:00:00Z"
                }"""
                self.send_header('Content-Length', len(response))
                self.end_headers()
                self.wfile.write(response.encode('ascii'))
                return

            if self.path == '/latest/meta-data/iam/security-credentials/expire_in_past/myprofile':
                self.protocol_version = 'HTTP/1.1'
                self.send_response(200)
                self.send_header('Content-type', 'application/json')
                response = """{
                    "AccessKeyId": "AWS_ACCESS_KEY_ID",
                    "SecretAccessKey": "AWS_SECRET_ACCESS_KEY",
                    "Expiration": "1970-01-01T00:00:00Z"
                }"""
                self.send_header('Content-Length', len(response))
                self.end_headers()
                self.wfile.write(response.encode('ascii'))
                return


            if self.path == '/gs_fake_bucket_http_header_file/resource':
                self.protocol_version = 'HTTP/1.1'

                if 'foo' not in self.headers or self.headers['foo'] != 'bar':
                    sys.stderr.write('Bad headers: %s\n' % str(self.headers))
                    self.send_response(403)
                    return

                self.send_response(200)
                self.send_header('Content-type', 'text/plain')
                self.send_header('Content-Length', 1)
                self.end_headers()
                self.wfile.write("""Y""".encode('ascii'))
                return

            if self.path == '/gs_fake_bucket/resource':
                self.protocol_version = 'HTTP/1.1'

                if 'Authorization' not in self.headers:
                    sys.stderr.write('Bad headers: %s\n' % str(self.headers))
                    self.send_response(403)
                    return
                expected_authorization = 'GOOG1 GS_ACCESS_KEY_ID:8tndu9//BfmN+Kg4AFLdUMZMBDQ='
                if self.headers['Authorization'] != expected_authorization :
                    sys.stderr.write("Bad Authorization: '%s'\n" % str(self.headers['Authorization']))
                    self.send_response(403)
                    return

                self.send_response(200)
                self.send_header('Content-type', 'text/plain')
                self.send_header('Content-Length', 3)
                self.end_headers()
                self.wfile.write("""foo""".encode('ascii'))
                return

            if self.path == '/gs_fake_bucket2?delimiter=/&prefix=a_dir/':
                self.protocol_version = 'HTTP/1.1'
                self.send_response(200)
                self.send_header('Content-type', 'application/xml')
                response = """<?xml version="1.0" encoding="UTF-8"?>
                    <ListBucketResult>
                        <Prefix>a_dir/</Prefix>
                        <NextMarker>bla</NextMarker>
                        <Contents>
                            <Key>a_dir/resource3.bin</Key>
                            <LastModified>1970-01-01T00:00:01.000Z</LastModified>
                            <Size>123456</Size>
                        </Contents>
                    </ListBucketResult>
                """
                self.send_header('Content-Length', len(response))
                self.end_headers()
                self.wfile.write(response.encode('ascii'))
                return

            if self.path == '/gs_fake_bucket2?delimiter=/&marker=bla&prefix=a_dir/':

                self.protocol_version = 'HTTP/1.1'
                self.send_response(200)
                self.send_header('Content-type', 'application/xml')
                response = """<?xml version="1.0" encoding="UTF-8"?>
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
                """
                self.send_header('Content-Length', len(response))
                self.end_headers()
                self.wfile.write(response.encode('ascii'))
                return


            global test_retry_attempt
            if self.path == '/test_retry/test.txt':
                if test_retry_attempt == 0:
                    self.protocol_version = 'HTTP/1.1'
                    self.send_response(502)
                    self.end_headers()
                else:
                    content = """foo""".encode('ascii')
                    self.protocol_version = 'HTTP/1.1'
                    self.send_response(200)
                    self.send_header('Content-type', 'text/plain')
                    self.send_header('Content-Length', len(content))
                    self.end_headers()
                    self.wfile.write(content)
                test_retry_attempt += 1
                return

            if self.path == '/test_retry_reset_counter':
                test_retry_attempt = 0
                self.send_response(200)
                self.end_headers()
                return


            if self.path == '/index.html':
                self.send_response(200)
                self.send_header('Content-type', 'text/html')
                self.end_headers()
                return

            # Below is for geocoding
            if self.path.find('/geocoding') != -1:
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
</searchresults>""".encode('ascii'))
                    return
                elif self.path == '/geocoding?q=NonExistingPlace&addressdetails=1&limit=1&email=foo%40bar':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<?xml version="1.0" encoding="UTF-8"?><searchresults></searchresults>""".encode('ascii'))
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
<!-- wws09.geotech.bf1.yahoo.com uncompressed/chunked Sat Dec 29 04:59:06 PST 2012 -->""".encode('ascii'))
                    return
                elif self.path == '/yahoogeocoding?q=NonExistingPlace':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<?xml version="1.0" encoding="UTF-8" standalone="yes"?><ResultSet xmlns:ns1="http://www.yahooapis.com/v1/base.rng" version="2.0" xml:lang="en-US"><Error>7</Error><ErrorMessage>No result</ErrorMessage><Locale>en-US</Locale><Found>0</Found><Quality>0</Quality></ResultSet>
<!-- nws08.maps.bf1.yahoo.com uncompressed/chunked Sat Dec 29 05:00:45 PST 2012 -->
<!-- wws08.geotech.bf1.yahoo.com uncompressed/chunked Sat Dec 29 05:00:45 PST 2012 -->""".encode('ascii'))
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
</geonames>""".encode('ascii'))
                    return
                elif self.path == '/geonamesgeocoding?q=NonExistingPlace&username=demo':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<geonames style="MEDIUM">
<totalResultsCount>0</totalResultsCount>
</geonames>""".encode('ascii'))
                    return

                else:
                    self.send_error(404,'File Not Found: %s' % self.path)
                    return

            elif self.path.find('/binggeocoding') != -1:
                if self.path == '/binggeocoding?q=Paris&key=fakekey':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<Response>
  <ResourceSets>
    <ResourceSet>
      <EstimatedTotal>1</EstimatedTotal>
      <Resources>
        <Location>
          <Name>Paris, Paris, France</Name>
          <Point>
            <Latitude>48</Latitude>
            <Longitude>2</Longitude>
          </Point>
          <BoundingBox>
            <SouthLatitude>48</SouthLatitude>
            <WestLongitude>2</WestLongitude>
            <NorthLatitude>48</NorthLatitude>
            <EastLongitude>2</EastLongitude>
          </BoundingBox>
          <Address>
            <AdminDistrict>IdF</AdminDistrict>
            <AdminDistrict2>Paris</AdminDistrict2>
            <CountryRegion>France</CountryRegion>
            <FormattedAddress>Paris, Paris, France</FormattedAddress>
            <Locality>Paris</Locality>
          </Address>
          <GeocodePoint>
            <Latitude>48</Latitude>
            <Longitude>2</Longitude>
            <CalculationMethod>Random</CalculationMethod>
            <UsageType>Display</UsageType>
          </GeocodePoint>
        </Location>
      </Resources>
    </ResourceSet>
  </ResourceSets>
</Response>""".encode('ascii'))
                    return
                elif self.path == '/binggeocoding?q=NonExistingPlace&key=fakekey':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<Response>
  <ResourceSets>
    <ResourceSet>
      <EstimatedTotal>0</EstimatedTotal>
      <Resources/>
    </ResourceSet>
  </ResourceSets>
</Response>""".encode('ascii'))
                    return

                else:
                    self.send_error(404,'File Not Found: %s' % self.path)
                    return

            # Below is for reverse geocoding
            elif self.path.find('/reversegeocoding') != -1:
                if self.path == '/reversegeocoding?lon=2.00000000&lat=49.00000000&email=foo%40bar' or \
                   self.path == '/reversegeocoding?lon=2.00000000&lat=49.00000000&zoom=12&email=foo%40bar':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<?xml version="1.0" encoding="UTF-8"?>
<reversegeocode>
  <result place_id="46754274" osm_type="way" osm_id="38621743" ref="Chemin du Cordon" lat="49.0002726061675" lon="1.99514157818059">Chemin du Cordon, Foret de l'Hautil, Triel-sur-Seine, Saint-Germain-en-Laye, Yvelines, Ile-de-France, 78510, France metropolitaine</result>
  <addressparts>
    <road>Chemin du Cordon</road>
    <forest>Foret de l'Hautil</forest>
    <city>Triel-sur-Seine</city>
    <county>Saint-Germain-en-Laye</county>
    <state>Ile-de-France</state>
    <postcode>78510</postcode>
    <country>France metropolitaine</country>
    <country_code>fr</country_code>
  </addressparts>
</reversegeocode>""".encode('ascii'))
                    return
                else:
                    self.send_error(404,'File Not Found: %s' % self.path)
                    return

            elif self.path.find('/yahooreversegeocoding') != -1:
                if self.path == '/yahooreversegeocoding?q=49.00000000,2.00000000&gflags=R':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<?xml version="1.0" encoding="UTF-8" standalone="yes"?><ResultSet xmlns:ns1="http://www.yahooapis.com/v1/base.rng" version="2.0" xml:lang="en-US"><Error>0</Error><ErrorMessage>No error</ErrorMessage><Locale>en-US</Locale><Found>1</Found><Quality>99</Quality><Result><quality>72</quality><latitude>49.001</latitude><longitude>1.999864</longitude><offsetlat>49.001</offsetlat><offsetlon>1.999864</offsetlon><radius>400</radius><name>49.00000000,2.00000000</name><line1>Chemin de Menucourt</line1><line2>78510 Triel-sur-Seine</line2><line3></line3><line4>France</line4><house></house><street>Chemin de Menucourt</street><xstreet></xstreet><unittype></unittype><unit></unit><postal>78510</postal><neighborhood></neighborhood><city>Triel-sur-Seine</city><county>Yvelines</county><state>Ile-de-France</state><country>France</country><countrycode>FR</countrycode><statecode></statecode><countycode>78</countycode><uzip>78510</uzip><hash></hash><woeid>12727518</woeid><woetype>11</woetype></Result></ResultSet>
<!-- nws02.maps.bf1.yahoo.com uncompressed/chunked Sat Dec 29 05:03:31 PST 2012 -->
<!-- wws05.geotech.bf1.yahoo.com uncompressed/chunked Sat Dec 29 05:03:31 PST 2012 -->""".encode('ascii'))
                    return
                else:
                    self.send_error(404,'File Not Found: %s' % self.path)
                    return

            elif self.path.find('/geonamesreversegeocoding') != -1:
                if self.path == '/geonamesreversegeocoding?lat=49.00000000&lng=2.00000000&username=demo':
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
</geonames>""".encode('ascii'))
                    return
                else:
                    self.send_error(404,'File Not Found: %s' % self.path)
                    return

            elif self.path.find('/bingreversegeocoding') != -1:
                if self.path == '/bingreversegeocoding?49.00000000,2.00000000&key=fakekey':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<Response>
  <ResourceSets>
    <ResourceSet>
      <EstimatedTotal>1</EstimatedTotal>
      <Resources>
        <Location>
          <Name>Paris, Paris, France</Name>
          <Point>
            <Latitude>48</Latitude>
            <Longitude>2</Longitude>
          </Point>
          <BoundingBox>
            <SouthLatitude>48</SouthLatitude>
            <WestLongitude>2</WestLongitude>
            <NorthLatitude>48</NorthLatitude>
            <EastLongitude>2</EastLongitude>
          </BoundingBox>
          <Address>
            <AdminDistrict>IdF</AdminDistrict>
            <AdminDistrict2>Paris</AdminDistrict2>
            <CountryRegion>France</CountryRegion>
            <FormattedAddress>Paris, Paris, France</FormattedAddress>
            <Locality>Paris</Locality>
          </Address>
          <GeocodePoint>
            <Latitude>48</Latitude>
            <Longitude>2</Longitude>
            <CalculationMethod>Random</CalculationMethod>
            <UsageType>Display</UsageType>
          </GeocodePoint>
        </Location>
      </Resources>
    </ResourceSet>
  </ResourceSets>
</Response>""".encode('ascii'))
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

def launch(fork_process = True):
    if not fork_process:
        try:
            server = GDAL_ThreadedHttpServer(GDAL_Handler)
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
