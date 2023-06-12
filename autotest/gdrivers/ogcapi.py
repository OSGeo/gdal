#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  OGCAPI driver testing.
# Author:   Alessandro Pasotti <elpaso at itopen dot it>
#
###############################################################################
# Copyright (c) 2023, Alessandro Pasotti <elpaso at itopen dot it>
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

import os
from http.server import BaseHTTPRequestHandler

import gdaltest
import ogrtest
import pytest
import requests
import webserver

from osgeo import gdal, ogr

# Set RECORD to TRUE to recreate test data from the https://demo.pygeoapi.io/stable server
RECORD = False
BASE_TEST_DATA_PATH = os.path.join(os.path.dirname(__file__), "data", "ogcapi")


def sanitize_url(url):
    chars = "&#/?="
    text = url
    for c in chars:
        text = text.replace(c, "_")
    return text.replace("_fakeogcapi", "request")


class OGCAPIHTTPHandler(BaseHTTPRequestHandler):
    def log_request(self, code="-", size="-"):
        pass

    def do_GET(self):

        try:

            request_data_path = os.path.join(
                BASE_TEST_DATA_PATH, sanitize_url(self.path) + ".http_data"
            )

            if RECORD:

                with open(request_data_path, "wb+") as fd:
                    response = requests.get(
                        "https://demo.pygeoapi.io/stable"
                        + self.path.replace("/fakeogcapi", ""),
                        stream=True,
                    )
                    content = response.content.replace(
                        b"https://demo.pygeoapi.io/stable",
                        (
                            "http://"
                            + self.address_string()
                            + ":"
                            + str(self.server.server_port)
                            + "/fakeogcapi"
                        ).encode("utf8"),
                    )
                    data = b"HTTP/2 200 OK\r\n"
                    for k, v in response.headers.items():
                        if k == "Content-Length":
                            data += (
                                k.encode("utf8")
                                + b": "
                                + str(len(content)).encode("utf8")
                                + b"\r\n"
                            )
                        else:
                            data += (
                                k.encode("utf8") + b": " + v.encode("utf8") + b"\r\n"
                            )
                    data += b"\r\n"
                    data += content
                    fd.write(data)
                    self.wfile.write(data)

            elif self.path.find("/fakeogcapi") != -1:

                with open(request_data_path, "rb+") as fd:
                    self.wfile.write(fd.read())

            return

        except IOError:
            pass

        self.send_error(404, "File Not Found: %s" % self.path)


###############################################################################
# Test underlying OGR drivers
#


pytestmark = pytest.mark.require_driver("OGCAPI")

###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


@pytest.fixture(autouse=True, scope="module")
def ogr_ogcapi_init():
    gdaltest.ogcapi_drv = ogr.GetDriverByName("OGCAPI")


def test_ogr_ogcapi_fake_ogcapi_server():
    if gdaltest.ogcapi_drv is None:
        pytest.skip()

    (process, port) = webserver.launch(handler=OGCAPIHTTPHandler)
    if port == 0:
        pytest.skip()

    ds = gdal.OpenEx("OGCAPI:http://127.0.0.1:%d/fakeogcapi" % port)
    if ds is None:
        webserver.server_stop(process, port)
        pytest.fail("did not manage to open OGCAPI datastore")

    sub_ds_uri = [
        v[0] for v in ds.GetSubDatasets() if v[1] == "Collection Large Lakes"
    ][0]
    del ds

    ds = gdal.OpenEx(sub_ds_uri)
    assert ds is not None

    lyr = ds.GetLayerByName("lakes")

    if lyr.GetName() != "lakes":
        print(lyr.GetName())
        webserver.server_stop(process, port)
        pytest.fail("did not get expected layer name")

    feat = lyr.GetNextFeature()
    fdef = feat.GetDefnRef()
    assert fdef.GetFieldDefn(0).GetName() == "id"
    assert fdef.GetFieldDefn(1).GetName() == "scalerank"
    assert fdef.GetFieldDefn(2).GetName() == "name"

    if (
        feat.GetField("name") != "Lake Baikal"
        or ogrtest.check_feature_geometry(
            feat,
            "POLYGON ((106.579985793079 52.7999815944455,106.539988234485 52.9399988877404,107.080006951935 53.18001007752,107.299993524202 53.3799978704895,107.599975213656 53.5199893255682,108.039948358189 53.8599685736165,108.37997928267 54.2599958359878,109.052703078245 55.0275975612513,109.193469679808 55.5356027288966,109.506990594523 55.7309138047437,109.929807163535 55.7129562445223,109.700002069133 54.9800035671105,109.660004510539 54.719993598034,109.479963820434 54.3399909531757,109.319973586059 53.8199968532387,109.220031366006 53.619983222053,108.999993117308 53.7800251328609,108.600017531368 53.4399942083804,108.800005324338 53.3799978704895,108.760007765744 53.2000088568169,108.459974399857 53.1400125189261,108.1799914897 52.7999815944455,107.799963006626 52.579995022179,107.319992303499 52.4200047878034,106.640033807402 52.3200108913186,106.100015089952 52.039976304729,105.740037062607 51.7599933945716,105.240015903751 51.5200080430081,104.819989862083 51.4600117051173,104.300021600362 51.5000092637112,103.760002882912 51.600003160196,103.620011427833 51.7399946152746,103.859996779396 51.8599872910564,104.399963820414 51.8599872910564,105.059975213646 52.0000045843512,105.480001255314 52.2800133327247,105.98002241417 52.5199986842882,106.260005324328 52.6199925807729,106.579985793079 52.7999815944455))",
            max_error=0.00001,
        )
        != 0
    ):
        feat.DumpReadable()
        webserver.server_stop(process, port)
        pytest.fail("did not get expected feature")

    del lyr
    del ds
    webserver.server_stop(process, port)
