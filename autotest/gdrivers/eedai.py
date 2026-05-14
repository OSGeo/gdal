#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Earth Engine Data API Images driver test suite.
# Author:   Even Rouault, even dot rouault at spatialys.com
#
###############################################################################
# Copyright (c) 2017, Planet Labs
#
# SPDX-License-Identifier: MIT
###############################################################################

import json
import struct
import sys

import gdaltest
import pytest
import webserver

from osgeo import gdal

pytestmark = [
    pytest.mark.require_driver("EEDAI"),
    pytest.mark.random_order(disabled=True),
]

###############################################################################
# Find EEDAI driver


def test_eedai_1():

    gdal.SetConfigOption("CPL_CURL_ENABLE_VSIMEM", "YES")

    gdaltest.EEDA_BEARER = gdal.GetConfigOption("EEDA_BEARER")
    gdaltest.EEDA_URL = gdal.GetConfigOption("EEDA_URL")
    gdaltest.EEDA_PRIVATE_KEY = gdal.GetConfigOption("EEDA_PRIVATE_KEY")
    gdaltest.EEDA_CLIENT_EMAIL = gdal.GetConfigOption("EEDA_CLIENT_EMAIL")
    gdaltest.GOOGLE_APPLICATION_CREDENTIALS = gdal.GetConfigOption(
        "GOOGLE_APPLICATION_CREDENTIALS"
    )
    gdal.SetConfigOption("GOOGLE_APPLICATION_CREDENTIALS", "")


###############################################################################
# Nominal case


def test_eedai_2():

    gdal.FileFromMemBuffer(
        "/vsimem/ee/projects/earthengine-public/assets/image",
        json.dumps(
            {
                "type": "IMAGE",
                "properties": {
                    "foo": "bar",
                    "prop_B9": "the_prop_B9",
                    "prop_BAND_2": "the_prop_B2",
                },
                "bands": [
                    {
                        "id": "B1",
                        "dataType": {"precision": "INT", "range": {"max": 65535}},
                        "grid": {
                            "crsCode": "EPSG:32610",
                            "affineTransform": {
                                "translateX": 499980,
                                "translateY": 4200000,
                                "scaleX": 60,
                                "scaleY": -60,
                            },
                            "dimensions": {"width": 1830, "height": 1831},
                        },
                    },
                    {
                        "id": "B2",
                        "dataType": {"precision": "INT", "range": {"max": 65535}},
                        "grid": {
                            "crsCode": "EPSG:32610",
                            "affineTransform": {
                                "translateX": 499980,
                                "translateY": 4200000,
                                "scaleX": 60,
                                "scaleY": -60,
                            },
                            "dimensions": {"width": 10980, "height": 10981},
                        },
                    },
                    {
                        "id": "B9",
                        "dataType": {"precision": "INT", "range": {"max": 65535}},
                        "grid": {
                            "crsCode": "EPSG:32610",
                            "affineTransform": {
                                "translateX": 499980,
                                "translateY": 4200000,
                                "scaleX": 60,
                                "scaleY": -60,
                            },
                            "dimensions": {"width": 1830, "height": 1831},
                        },
                    },
                ],
            }
        ),
    )

    # To please the unregistering of the persistent connection
    gdal.FileFromMemBuffer("/vsimem/ee/", "")

    gdal.SetConfigOption("EEDA_BEARER", "mybearer")
    gdal.SetConfigOption("EEDA_URL", "/vsimem/ee/")
    ds = gdal.OpenEx("EEDAI:image", open_options=["PIXEL_ENCODING=NPY"])
    gdal.SetConfigOption("EEDA_URL", None)

    expected_info = {
        "metadata": {
            "": {"foo": "bar"},
            "SUBDATASETS": {
                "SUBDATASET_1_DESC": "Bands B1,B9 of image",
                "SUBDATASET_2_DESC": "Band B2 of image",
                "SUBDATASET_2_NAME": "EEDAI:image:B2",
                "SUBDATASET_1_NAME": "EEDAI:image:B1,B9",
            },
            "IMAGE_STRUCTURE": {"INTERLEAVE": "PIXEL"},
        },
        "size": [1830, 1831],
        "driverLongName": "Earth Engine Data API Image",
        "bands": [
            {
                "colorInterpretation": "Undefined",
                "block": [256, 256],
                "metadata": {},
                "band": 1,
                "description": "B1",
                "type": "UInt16",
                "overviews": [
                    {"size": [915, 915]},
                    {"size": [457, 457]},
                    {"size": [228, 228]},
                ],
            },
            {
                "colorInterpretation": "Undefined",
                "block": [256, 256],
                "metadata": {"": {"prop": "the_prop_B9"}},
                "band": 2,
                "description": "B9",
                "type": "UInt16",
                "overviews": [
                    {"size": [915, 915]},
                    {"size": [457, 457]},
                    {"size": [228, 228]},
                ],
            },
        ],
        "cornerCoordinates": {
            "center": [554880.0, 4145070.0],
            "upperRight": [609780.0, 4200000.0],
            "lowerLeft": [499980.0, 4090140.0],
            "lowerRight": [609780.0, 4090140.0],
            "upperLeft": [499980.0, 4200000.0],
        },
        "files": [],
        "description": "EEDAI:image",
        "geoTransform": [499980.0, 60.0, 0.0, 4200000.0, 0.0, -60.0],
    }

    info = gdal.Info(ds, format="json")
    for key in expected_info:
        if not (key in info and info[key] == expected_info[key]):
            if key in info:
                print("Got: " + str(info[key]))
            else:
                print("Does not exist in got info")
            print("Expected: " + str(expected_info[key]))
            print("Whole info:")
            print(json.dumps(info, indent=4))
            pytest.fail("Got difference for key %s" % key)

    assert ds.GetProjectionRef().find("32610") >= 0

    assert ds.GetRasterBand(1).GetOverview(-1) is None

    assert ds.GetRasterBand(1).GetOverview(3) is None

    npy_serialized = struct.pack(
        "B" * 8, 0x93, ord("N"), ord("U"), ord("M"), ord("P"), ord("Y"), 1, 0
    )
    descr = "{'descr': [('B1', '<u2'), ('B9', '<u2')], 'fortran_order': False, 'shape': (39, 38), }".encode(
        "ascii"
    )
    npy_serialized += struct.pack("<h", len(descr))
    npy_serialized += descr
    val = struct.pack("<h", 12345) + struct.pack("<h", 23456)
    npy_serialized += "".encode("ascii").join(val for i in range(38 * 39))

    gdal.FileFromMemBuffer(
        '/vsimem/ee/projects/earthengine-public/assets/image:getPixels&CUSTOMREQUEST=POST&POSTFIELDS={ "fileFormat": "NPY", "bandIds": [ "B1", "B9" ], "grid": { "affineTransform": { "translateX": 607500.0, "translateY": 4092480.0, "scaleX": 60.0, "scaleY": -60.0, "shearX": 0.0, "shearY": 0.0 }, "dimensions": { "width": 38, "height": 39 } } }',
        npy_serialized,
    )
    got_data = ds.GetRasterBand(1).ReadRaster(1800, 1810, 1, 1)
    got_data = struct.unpack("h", got_data)[0]
    assert got_data == 12345
    got_data = ds.GetRasterBand(2).ReadRaster(1800, 1810, 1, 1)
    got_data = struct.unpack("h", got_data)[0]
    assert got_data == 23456

    ds = None

    gdal.SetConfigOption("EEDA_URL", "/vsimem/ee/")
    sub_ds = gdal.Open("EEDAI:image:B1,B9")
    gdal.SetConfigOption("EEDA_URL", None)
    assert sub_ds.RasterCount == 2

    gdal.SetConfigOption("EEDA_URL", "/vsimem/ee/")
    sub_ds = gdal.Open("EEDAI:image:B2")
    gdal.SetConfigOption("EEDA_URL", None)
    assert sub_ds.RasterCount == 1

    got_md = sub_ds.GetRasterBand(1).GetMetadata()
    expected_md = {"prop": "the_prop_B2"}
    assert got_md == expected_md

    gdal.SetConfigOption("EEDA_BEARER", None)


###############################################################################
# Test OAuth2 with ServiceAccount


def test_eedai_3():

    gdal.SetConfigOption("EEDA_URL", "/vsimem/ee/")
    # Generated with 'openssl genrsa -out rsa-openssl.pem 1024' and
    # 'openssl pkcs8 -nocrypt -in rsa-openssl.pem -inform PEM -topk8 -outform PEM -out rsa-openssl.pkcs8.pem'
    # DO NOT USE in production !!!!
    gdal.SetConfigOption(
        "EEDA_PRIVATE_KEY",
        """-----BEGIN PRIVATE KEY-----
MIICeAIBADANBgkqhkiG9w0BAQEFAASCAmIwggJeAgEAAoGBAOlwJQLLDG1HeLrk
VNcFR5Qptto/rJE5emRuy0YmkVINT4uHb1be7OOo44C2Ev8QPVtNHHS2XwCY5gTm
i2RfIBLv+VDMoVQPqqE0LHb0WeqGmM5V1tHbmVnIkCcKMn3HpK30grccuBc472LQ
DVkkGqIiGu0qLAQ89JP/r0LWWySRAgMBAAECgYAWjsS00WRBByAOh1P/dz4kfidy
TabiXbiLDf3MqJtwX2Lpa8wBjAc+NKrPXEjXpv0W3ou6Z4kkqKHJpXGg4GRb4N5I
2FA+7T1lA0FCXa7dT2jvgJLgpBepJu5b//tqFqORb4A4gMZw0CiPN3sUsWsSw5Hd
DrRXwp6sarzG77kvZQJBAPgysAmmXIIp9j1hrFSkctk4GPkOzZ3bxKt2Nl4GFrb+
bpKSon6OIhP1edrxTz1SMD1k5FiAAVUrMDKSarbh5osCQQDwxq4Tvf/HiYz79JBg
Wz5D51ySkbg01dOVgFW3eaYAdB6ta/o4vpHhnbrfl6VO9oUb3QR4hcrruwnDHsw3
4mDTAkEA9FPZjbZSTOSH/cbgAXbdhE4/7zWOXj7Q7UVyob52r+/p46osAk9i5qj5
Kvnv2lrFGDrwutpP9YqNaMtP9/aLnwJBALLWf9n+GAv3qRZD0zEe1KLPKD1dqvrj
j+LNjd1Xp+tSVK7vMs4PDoAMDg+hrZF3HetSQM3cYpqxNFEPgRRJOy0CQQDQlZHI
yzpSgEiyx8O3EK1iTidvnLXbtWabvjZFfIE/0OhfBmN225MtKG3YLV2HoUvpajLq
gwE6fxOLyJDxuWRf
-----END PRIVATE KEY-----
""",
    )
    gdal.SetConfigOption("EEDA_CLIENT_EMAIL", "my@email.com")
    gdal.SetConfigOption("GO2A_AUD", "/vsimem/oauth2/v4/token")
    gdal.SetConfigOption("GOA2_NOW", "123456")
    gdal.FileFromMemBuffer(
        "/vsimem/oauth2/v4/token&POSTFIELDS=grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiAibXlAZW1haWwuY29tIiwgInNjb3BlIjogImh0dHBzOi8vd3d3Lmdvb2dsZWFwaXMuY29tL2F1dGgvZWFydGhlbmdpbmUucmVhZG9ubHkiLCAiYXVkIjogIi92c2ltZW0vb2F1dGgyL3Y0L3Rva2VuIiwgImlhdCI6IDEyMzQ1NiwgImV4cCI6IDEyNzA1Nn0%3D.1W564xcQESVsqZmBEIMzj4rr0RuGa4RiUPZp5H%2FNENN9V9oPSTdacw%2BMiu3pcFf9AJv8wj0ajUeRsgTmvSicAftER49xeCQYUrs6uV122FGVsxml26kMFacNsCgRad%2Fy7xCAhMPfRJsqxS2%2BB392ssBeEzTGCSI6W3AsJg64OfA%3D",
        '{ "access_token": "my_token", "token_type": "Bearer", "expires_in": 3600 }',
    )

    try:
        ds = gdal.Open("EEDAI:image")
        assert ds is not None
    except RuntimeError:
        pass
    finally:
        gdal.SetConfigOption("EEDA_URL", None)
        gdal.SetConfigOption("EEDA_PRIVATE_KEY", None)
        gdal.SetConfigOption("EEDA_CLIENT_EMAIL", None)

    if "CPLRSASHA256Sign() not implemented" in gdal.GetLastErrorMsg():
        pytest.skip("CPLRSASHA256Sign() not implemented")


###############################################################################
# Test OAuth2 with GOOGLE_APPLICATION_CREDENTIALS


@pytest.mark.parametrize("use_vsi_path", [False, True])
def test_eedai_GOOGLE_APPLICATION_CREDENTIALS(use_vsi_path):

    gdal.FileFromMemBuffer(
        "/vsimem/my.json",
        """{
  "private_key": "-----BEGIN PRIVATE KEY-----\nMIICeAIBADANBgkqhkiG9w0BAQEFAASCAmIwggJeAgEAAoGBAOlwJQLLDG1HeLrk\nVNcFR5Qptto/rJE5emRuy0YmkVINT4uHb1be7OOo44C2Ev8QPVtNHHS2XwCY5gTm\ni2RfIBLv+VDMoVQPqqE0LHb0WeqGmM5V1tHbmVnIkCcKMn3HpK30grccuBc472LQ\nDVkkGqIiGu0qLAQ89JP/r0LWWySRAgMBAAECgYAWjsS00WRBByAOh1P/dz4kfidy\nTabiXbiLDf3MqJtwX2Lpa8wBjAc+NKrPXEjXpv0W3ou6Z4kkqKHJpXGg4GRb4N5I\n2FA+7T1lA0FCXa7dT2jvgJLgpBepJu5b//tqFqORb4A4gMZw0CiPN3sUsWsSw5Hd\nDrRXwp6sarzG77kvZQJBAPgysAmmXIIp9j1hrFSkctk4GPkOzZ3bxKt2Nl4GFrb+\nbpKSon6OIhP1edrxTz1SMD1k5FiAAVUrMDKSarbh5osCQQDwxq4Tvf/HiYz79JBg\nWz5D51ySkbg01dOVgFW3eaYAdB6ta/o4vpHhnbrfl6VO9oUb3QR4hcrruwnDHsw3\n4mDTAkEA9FPZjbZSTOSH/cbgAXbdhE4/7zWOXj7Q7UVyob52r+/p46osAk9i5qj5\nKvnv2lrFGDrwutpP9YqNaMtP9/aLnwJBALLWf9n+GAv3qRZD0zEe1KLPKD1dqvrj\nj+LNjd1Xp+tSVK7vMs4PDoAMDg+hrZF3HetSQM3cYpqxNFEPgRRJOy0CQQDQlZHI\nyzpSgEiyx8O3EK1iTidvnLXbtWabvjZFfIE/0OhfBmN225MtKG3YLV2HoUvpajLq\ngwE6fxOLyJDxuWRf\n-----END PRIVATE KEY-----\n",
  "client_email": "my@email.com"
                           }""",
    )

    gdal.SetConfigOption("EEDA_URL", "/vsimem/ee/")
    if use_vsi_path:
        gdal.SetPathSpecificOption(
            "/vsigs/to_test_eeda", "GOOGLE_APPLICATION_CREDENTIALS", "/vsimem/my.json"
        )
    else:
        gdal.SetConfigOption("GOOGLE_APPLICATION_CREDENTIALS", "/vsimem/my.json")
    gdal.SetConfigOption("EEDA_PRIVATE_KEY", None)
    gdal.SetConfigOption("EEDA_CLIENT_EMAIL", None)
    gdal.SetConfigOption("GO2A_AUD", "/vsimem/oauth2/v4/token")
    gdal.SetConfigOption("GOA2_NOW", "123456")
    gdal.FileFromMemBuffer(
        "/vsimem/oauth2/v4/token&POSTFIELDS=grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiAibXlAZW1haWwuY29tIiwgInNjb3BlIjogImh0dHBzOi8vd3d3Lmdvb2dsZWFwaXMuY29tL2F1dGgvZWFydGhlbmdpbmUucmVhZG9ubHkiLCAiYXVkIjogIi92c2ltZW0vb2F1dGgyL3Y0L3Rva2VuIiwgImlhdCI6IDEyMzQ1NiwgImV4cCI6IDEyNzA1Nn0%3D.1W564xcQESVsqZmBEIMzj4rr0RuGa4RiUPZp5H%2FNENN9V9oPSTdacw%2BMiu3pcFf9AJv8wj0ajUeRsgTmvSicAftER49xeCQYUrs6uV122FGVsxml26kMFacNsCgRad%2Fy7xCAhMPfRJsqxS2%2BB392ssBeEzTGCSI6W3AsJg64OfA%3D",
        '{ "access_token": "my_token", "token_type": "Bearer", "expires_in": 3600 }',
    )

    open_options = []
    if use_vsi_path:
        open_options.append("VSI_PATH_FOR_AUTH=/vsigs/to_test_eeda")
    try:
        ds = gdal.OpenEx("EEDAI:image", open_options=open_options)
        assert ds is not None
    except RuntimeError:
        pass
    finally:
        gdal.Unlink("/vsimem/my.json")

        gdal.SetConfigOption("EEDA_URL", None)
        gdal.SetConfigOption("GOOGLE_APPLICATION_CREDENTIALS", None)
        gdal.SetConfigOption("EEDA_PRIVATE_KEY", None)
        gdal.SetConfigOption("EEDA_CLIENT_EMAIL", None)
        gdal.ClearPathSpecificOptions("/vsigs/to_test_eeda")

    if "CPLRSASHA256Sign() not implemented" in gdal.GetLastErrorMsg():
        pytest.skip("CPLRSASHA256Sign() not implemented")


###############################################################################
# Read credentials from simulated GCE instance


@pytest.mark.skipif(sys.platform not in ("linux", "win32"), reason="Incorrect platform")
def test_eedai_gce_credentials():

    gdaltest.webserver_process = None
    gdaltest.webserver_port = 0

    gdaltest.webserver_process, gdaltest.webserver_port = webserver.launch(
        handler=webserver.DispatcherHttpHandler
    )
    if gdaltest.webserver_port == 0:
        pytest.skip()

    gdal.SetConfigOption(
        "CPL_GCE_CREDENTIALS_URL",
        "http://localhost:%d/computeMetadata/v1/instance/service-accounts/default/token"
        % gdaltest.webserver_port,
    )
    # Disable hypervisor related check to test if we are really on EC2
    gdal.SetConfigOption("CPL_GCE_CHECK_LOCAL_FILES", "NO")

    gdal.VSICurlClearCache()

    def method(request):
        if "Authorization" not in request.headers:
            sys.stderr.write("Bad headers: %s\n" % str(request.headers))
            request.send_response(403)
            return
        expected_authorization = "Bearer ACCESS_TOKEN"
        if request.headers["Authorization"] != expected_authorization:
            sys.stderr.write(
                "Bad Authorization: '%s'\n" % str(request.headers["Authorization"])
            )
            request.send_response(403)
            return

        request.send_response(200)
        request.send_header("Content-type", "text/plain")
        request.send_header("Content-Length", 3)
        request.end_headers()
        request.wfile.write("""foo""".encode("ascii"))

    handler = webserver.SequentialHandler()
    handler.add(
        "GET",
        "/computeMetadata/v1/instance/service-accounts/default/token",
        200,
        {},
        """{
                "access_token" : "ACCESS_TOKEN",
                "token_type" : "Bearer",
                "expires_in" : 3600,
                }""",
    )

    with webserver.install_http_handler(handler):
        gdal.SetConfigOption("EEDA_URL", "/vsimem/ee/")
        ds = gdal.Open("EEDAI:image")
        gdal.SetConfigOption("EEDA_URL", None)

    gdal.SetConfigOption("CPL_GCE_CREDENTIALS_URL", None)
    gdal.SetConfigOption("CPL_GCE_CHECK_LOCAL_FILES", None)

    webserver.server_stop(gdaltest.webserver_process, gdaltest.webserver_port)

    assert ds is not None


###############################################################################
# Request in PNG mode


def test_eedai_4():

    gdal.FileFromMemBuffer(
        "/vsimem/ee/projects/earthengine-public/assets/image",
        json.dumps(
            {
                "type": "IMAGE",
                "bands": [
                    {
                        "id": "B1",
                        "dataType": {"precision": "INT", "range": {"max": 255}},
                        "grid": {
                            "crsCode": "EPSG:32610",
                            "affineTransform": {
                                "translateX": 499980,
                                "translateY": 4200000,
                                "scaleX": 60,
                                "scaleY": -60,
                            },
                            "dimensions": {"width": 1830, "height": 1831},
                        },
                    },
                    {
                        "id": "B2",
                        "dataType": {"precision": "INT", "range": {"max": 255}},
                        "grid": {
                            "crsCode": "EPSG:32610",
                            "affineTransform": {
                                "translateX": 499980,
                                "translateY": 4200000,
                                "scaleX": 60,
                                "scaleY": -60,
                            },
                            "dimensions": {"width": 1830, "height": 1831},
                        },
                    },
                    {
                        "id": "B3",
                        "dataType": {"precision": "INT", "range": {"max": 255}},
                        "grid": {
                            "crsCode": "EPSG:32610",
                            "affineTransform": {
                                "translateX": 499980,
                                "translateY": 4200000,
                                "scaleX": 60,
                                "scaleY": -60,
                            },
                            "dimensions": {"width": 1830, "height": 1831},
                        },
                    },
                ],
            }
        ),
    )

    gdal.SetConfigOption("EEDA_BEARER", "mybearer")
    gdal.SetConfigOption("EEDA_URL", "/vsimem/ee/")
    ds = gdal.Open("EEDAI:image")
    gdal.SetConfigOption("EEDA_URL", None)

    mem_ds = gdal.GetDriverByName("MEM").Create("", 256, 256, 3)
    mem_ds.GetRasterBand(1).Fill(127)
    mem_ds.GetRasterBand(2).Fill(128)
    mem_ds.GetRasterBand(3).Fill(129)
    gdal.GetDriverByName("PNG").CreateCopy("/vsimem/out.png", mem_ds)
    f = gdal.VSIFOpenL("/vsimem/out.png", "rb")
    png_data = gdal.VSIFReadL(1, 1000000, f)
    gdal.VSIFCloseL(f)
    gdal.Unlink("/vsimem/out.png")

    gdal.FileFromMemBuffer(
        '/vsimem/ee/projects/earthengine-public/assets/image:getPixels&CUSTOMREQUEST=POST&POSTFIELDS={ "fileFormat": "PNG", "bandIds": [ "B1", "B2", "B3" ], "grid": { "affineTransform": { "translateX": 499980.0, "translateY": 4200000.0, "scaleX": 60.0, "scaleY": -60.0, "shearX": 0.0, "shearY": 0.0 }, "dimensions": { "width": 256, "height": 256 } } }',
        png_data,
    )
    got_data = ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1)
    got_data = struct.unpack("B", got_data)[0]
    assert got_data == 127

    # Same with dataset RasterIO
    got_data = ds.ReadRaster(0, 0, 1, 1)
    got_data = struct.unpack("B" * 3, got_data)
    assert got_data == (127, 128, 129)

    # Same after flushing cache
    ds.FlushCache()
    got_data = ds.ReadRaster(0, 0, 1, 1)
    got_data = struct.unpack("B" * 3, got_data)
    assert got_data == (127, 128, 129)

    # Sub-sampled query
    gdal.FileFromMemBuffer(
        '/vsimem/ee/projects/earthengine-public/assets/image:getPixels&CUSTOMREQUEST=POST&POSTFIELDS={ "fileFormat": "PNG", "bandIds": [ "B1", "B2", "B3" ], "grid": { "affineTransform": { "translateX": 499980.0, "translateY": 4200000.0, "scaleX": 120.0, "scaleY": -120.06557377049181, "shearX": 0.0, "shearY": 0.0 }, "dimensions": { "width": 256, "height": 256 } } }',
        png_data,
    )
    got_data = ds.GetRasterBand(1).ReadRaster(0, 0, 2, 2, buf_xsize=1, buf_ysize=1)
    got_data = struct.unpack("B", got_data)[0]
    assert got_data == 127

    # Same after flushing cache with dataset RasterIO
    ds.FlushCache()
    got_data = ds.ReadRaster(0, 0, 2, 2, buf_xsize=1, buf_ysize=1)
    got_data = struct.unpack("B" * 3, got_data)
    assert got_data == (127, 128, 129)

    ds = None

    gdal.SetConfigOption("EEDA_BEARER", None)


###############################################################################
# Request in AUTO GTIFF mode


def test_eedai_geotiff():

    gdal.FileFromMemBuffer(
        "/vsimem/ee/projects/earthengine-public/assets/image",
        json.dumps(
            {
                "type": "IMAGE",
                "bands": [
                    {
                        "id": "B1",
                        "dataType": {"precision": "INT", "range": {"max": 65535}},
                        "grid": {
                            "crsCode": "EPSG:32610",
                            "affineTransform": {
                                "translateX": 499980,
                                "translateY": 4200000,
                                "scaleX": 60,
                                "scaleY": -60,
                            },
                            "dimensions": {"width": 1830, "height": 1831},
                        },
                    }
                ],
            }
        ),
    )

    gdal.SetConfigOption("EEDA_BEARER", "mybearer")
    gdal.SetConfigOption("EEDA_URL", "/vsimem/ee/")
    ds = gdal.Open("EEDAI:image")
    gdal.SetConfigOption("EEDA_URL", None)

    mem_ds = gdal.GetDriverByName("MEM").Create("", 256, 256, 1, gdal.GDT_UInt16)
    mem_ds.GetRasterBand(1).Fill(12345)
    gdal.GetDriverByName("GTiff").CreateCopy("/vsimem/out.tif", mem_ds)
    f = gdal.VSIFOpenL("/vsimem/out.tif", "rb")
    data = gdal.VSIFReadL(1, 1000000, f)
    gdal.VSIFCloseL(f)
    gdal.Unlink("/vsimem/out.tif")

    gdal.FileFromMemBuffer(
        '/vsimem/ee/projects/earthengine-public/assets/image:getPixels&CUSTOMREQUEST=POST&POSTFIELDS={ "fileFormat": "GEO_TIFF", "bandIds": [ "B1" ], "grid": { "affineTransform": { "translateX": 499980.0, "translateY": 4200000.0, "scaleX": 60.0, "scaleY": -60.0, "shearX": 0.0, "shearY": 0.0 }, "dimensions": { "width": 256, "height": 256 } } }',
        data,
    )
    got_data = ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1)
    got_data = struct.unpack("H", got_data)[0]
    assert got_data == 12345

    ds = None

    gdal.SetConfigOption("EEDA_BEARER", None)


###############################################################################
#


def test_eedai_cleanup():

    gdal.SetConfigOption("CPL_CURL_ENABLE_VSIMEM", None)
    gdal.SetConfigOption("EEDA_BEARER", gdaltest.EEDA_BEARER)
    gdal.SetConfigOption("EEDA_URL", gdaltest.EEDA_URL)
    gdal.SetConfigOption("EEDA_PRIVATE_KEY", gdaltest.EEDA_PRIVATE_KEY)
    gdal.SetConfigOption("EEDA_CLIENT_EMAIL", gdaltest.EEDA_CLIENT_EMAIL)
    gdal.SetConfigOption("GO2A_AUD", None)
    gdal.SetConfigOption("GOA2_NOW", None)
    gdal.SetConfigOption(
        "GOOGLE_APPLICATION_CREDENTIALS", gdaltest.GOOGLE_APPLICATION_CREDENTIALS
    )

    gdal.Unlink("/vsimem/ee/projects/earthengine-public/assets/image")
    with gdaltest.disable_exceptions():
        gdal.RmdirRecursive("/vsimem/ee/")


###############################################################################
#


def test_eedai_real_service():

    if gdal.GetConfigOption("GOOGLE_APPLICATION_CREDENTIALS") is None:

        if (
            gdal.GetConfigOption("EEDA_PRIVATE_KEY_FILE") is None
            and gdal.GetConfigOption("EEDA_PRIVATE_KEY") is None
        ):
            pytest.skip(
                "Missing EEDA_PRIVATE_KEY_FILE/EEDA_PRIVATE_KEY or GOOGLE_APPLICATION_CREDENTIALS"
            )

        if gdal.GetConfigOption("EEDA_CLIENT_EMAIL") is None:
            pytest.skip("Missing EEDA_CLIENT_EMAIL")

    ds = gdal.Open("EEDAI:USDA/NAIP/DOQQ/n_4010064_se_14_2_20070725")
    assert ds is not None
    res = gdal.Info(ds, format="json")
    expected = {
        "description": "EEDAI:USDA/NAIP/DOQQ/n_4010064_se_14_2_20070725",
        "driverShortName": "EEDAI",
        "driverLongName": "Earth Engine Data API Image",
        "files": [],
        "size": [3023, 3800],
        "coordinateSystem": {
            "wkt": 'PROJCRS["NAD83 / UTM zone 14N",\n    BASEGEOGCRS["NAD83",\n        DATUM["North American Datum 1983",\n            ELLIPSOID["GRS 1980",6378137,298.257222101,\n                LENGTHUNIT["metre",1]]],\n        PRIMEM["Greenwich",0,\n            ANGLEUNIT["degree",0.0174532925199433]],\n        ID["EPSG",4269]],\n    CONVERSION["UTM zone 14N",\n        METHOD["Transverse Mercator",\n            ID["EPSG",9807]],\n        PARAMETER["Latitude of natural origin",0,\n            ANGLEUNIT["degree",0.0174532925199433],\n            ID["EPSG",8801]],\n        PARAMETER["Longitude of natural origin",-99,\n            ANGLEUNIT["degree",0.0174532925199433],\n            ID["EPSG",8802]],\n        PARAMETER["Scale factor at natural origin",0.9996,\n            SCALEUNIT["unity",1],\n            ID["EPSG",8805]],\n        PARAMETER["False easting",500000,\n            LENGTHUNIT["metre",1],\n            ID["EPSG",8806]],\n        PARAMETER["False northing",0,\n            LENGTHUNIT["metre",1],\n            ID["EPSG",8807]]],\n    CS[Cartesian,2],\n        AXIS["easting",east,\n            ORDER[1],\n            LENGTHUNIT["metre",1]],\n        AXIS["northing",north,\n            ORDER[2],\n            LENGTHUNIT["metre",1]],\n    ID["EPSG",26914]]',
            "dataAxisToSRSAxisMapping": [1, 2],
        },
        "geoTransform": [408970.0, 2.0, 0.0, 4435536.0, 0.0, -2.0],
        "metadata": {"IMAGE_STRUCTURE": {"INTERLEAVE": "PIXEL"}},
        "cornerCoordinates": {
            "upperLeft": [408970.0, 4435536.0],
            "lowerLeft": [408970.0, 4427936.0],
            "lowerRight": [415016.0, 4427936.0],
            "upperRight": [415016.0, 4435536.0],
            "center": [411993.0, 4431736.0],
        },
        "wgs84Extent": {
            "type": "Polygon",
            "coordinates": [
                [
                    [-100.067433, 40.0651671],
                    [-100.0663662, 39.9967049],
                    [-99.9955511, 39.9973349],
                    [-99.9965471, 40.0657986],
                    [-100.067433, 40.0651671],
                ]
            ],
        },
        "bands": [
            {
                "band": 1,
                "block": [256, 256],
                "type": "Byte",
                "colorInterpretation": "Red",
                "description": "R",
                "overviews": [
                    {"size": [1511, 1900]},
                    {"size": [755, 950]},
                    {"size": [377, 475]},
                    {"size": [188, 237]},
                ],
                "metadata": {},
            },
            {
                "band": 2,
                "block": [256, 256],
                "type": "Byte",
                "colorInterpretation": "Green",
                "description": "G",
                "overviews": [
                    {"size": [1511, 1900]},
                    {"size": [755, 950]},
                    {"size": [377, 475]},
                    {"size": [188, 237]},
                ],
                "metadata": {},
            },
            {
                "band": 3,
                "block": [256, 256],
                "type": "Byte",
                "colorInterpretation": "Blue",
                "description": "B",
                "overviews": [
                    {"size": [1511, 1900]},
                    {"size": [755, 950]},
                    {"size": [377, 475]},
                    {"size": [188, 237]},
                ],
                "metadata": {},
            },
        ],
        "stac": {
            "proj:shape": [3023, 3800],
            "proj:wkt2": 'PROJCRS["NAD83 / UTM zone 14N",\n    BASEGEOGCRS["NAD83",\n        DATUM["North American Datum 1983",\n            ELLIPSOID["GRS 1980",6378137,298.257222101,\n                LENGTHUNIT["metre",1]]],\n        PRIMEM["Greenwich",0,\n            ANGLEUNIT["degree",0.0174532925199433]],\n        ID["EPSG",4269]],\n    CONVERSION["UTM zone 14N",\n        METHOD["Transverse Mercator",\n            ID["EPSG",9807]],\n        PARAMETER["Latitude of natural origin",0,\n            ANGLEUNIT["degree",0.0174532925199433],\n            ID["EPSG",8801]],\n        PARAMETER["Longitude of natural origin",-99,\n            ANGLEUNIT["degree",0.0174532925199433],\n            ID["EPSG",8802]],\n        PARAMETER["Scale factor at natural origin",0.9996,\n            SCALEUNIT["unity",1],\n            ID["EPSG",8805]],\n        PARAMETER["False easting",500000,\n            LENGTHUNIT["metre",1],\n            ID["EPSG",8806]],\n        PARAMETER["False northing",0,\n            LENGTHUNIT["metre",1],\n            ID["EPSG",8807]]],\n    CS[Cartesian,2],\n        AXIS["easting",east,\n            ORDER[1],\n            LENGTHUNIT["metre",1]],\n        AXIS["northing",north,\n            ORDER[2],\n            LENGTHUNIT["metre",1]],\n    ID["EPSG",26914]]',
            "proj:epsg": 26914,
            "proj:projjson": {
                "$schema": "https://proj.org/schemas/v0.7/projjson.schema.json",
                "type": "ProjectedCRS",
                "name": "NAD83 / UTM zone 14N",
                "base_crs": {
                    "name": "NAD83",
                    "datum": {
                        "type": "GeodeticReferenceFrame",
                        "name": "North American Datum 1983",
                        "ellipsoid": {
                            "name": "GRS 1980",
                            "semi_major_axis": 6378137,
                            "inverse_flattening": 298.257222101,
                        },
                    },
                    "coordinate_system": {
                        "subtype": "ellipsoidal",
                        "axis": [
                            {
                                "name": "Geodetic latitude",
                                "abbreviation": "Lat",
                                "direction": "north",
                                "unit": "degree",
                            },
                            {
                                "name": "Geodetic longitude",
                                "abbreviation": "Lon",
                                "direction": "east",
                                "unit": "degree",
                            },
                        ],
                    },
                    "id": {"authority": "EPSG", "code": 4269},
                },
                "conversion": {
                    "name": "UTM zone 14N",
                    "method": {
                        "name": "Transverse Mercator",
                        "id": {"authority": "EPSG", "code": 9807},
                    },
                    "parameters": [
                        {
                            "name": "Latitude of natural origin",
                            "value": 0,
                            "unit": "degree",
                            "id": {"authority": "EPSG", "code": 8801},
                        },
                        {
                            "name": "Longitude of natural origin",
                            "value": -99,
                            "unit": "degree",
                            "id": {"authority": "EPSG", "code": 8802},
                        },
                        {
                            "name": "Scale factor at natural origin",
                            "value": 0.9996,
                            "unit": "unity",
                            "id": {"authority": "EPSG", "code": 8805},
                        },
                        {
                            "name": "False easting",
                            "value": 500000,
                            "unit": "metre",
                            "id": {"authority": "EPSG", "code": 8806},
                        },
                        {
                            "name": "False northing",
                            "value": 0,
                            "unit": "metre",
                            "id": {"authority": "EPSG", "code": 8807},
                        },
                    ],
                },
                "coordinate_system": {
                    "subtype": "Cartesian",
                    "axis": [
                        {
                            "name": "Easting",
                            "abbreviation": "",
                            "direction": "east",
                            "unit": "metre",
                        },
                        {
                            "name": "Northing",
                            "abbreviation": "",
                            "direction": "north",
                            "unit": "metre",
                        },
                    ],
                },
                "id": {"authority": "EPSG", "code": 26914},
            },
            "proj:transform": [408970.0, 2.0, 0.0, 4435536.0, 0.0, -2.0],
            "raster:bands": [
                {"data_type": "uint8"},
                {"data_type": "uint8"},
                {"data_type": "uint8"},
            ],
            "eo:bands": [
                {"name": "b1", "description": "R"},
                {"name": "b2", "description": "G"},
                {"name": "b3", "description": "B"},
            ],
        },
    }
    assert expected == res
    assert ds.ReadRaster(0, 0, 1, 1) is not None

    ds = gdal.Open("EEDAI:MODIS/006/MYD09GA/2017_05_24")
    assert ds is not None
    res = gdal.Info(ds, format="json")
    expected = {
        "description": "EEDAI:MODIS/006/MYD09GA/2017_05_24",
        "driverShortName": "EEDAI",
        "driverLongName": "Earth Engine Data API Image",
        "files": [],
        "size": [43200, 21600],
        "coordinateSystem": {
            "wkt": 'PROJCRS["MODIS Sinusoidal",\n    BASEGEOGCRS["WGS 84",\n        DATUM["World Geodetic System 1984",\n            ELLIPSOID["WGS 84",6378137,298.257223563,\n                LENGTHUNIT["metre",1]]],\n        PRIMEM["Greenwich",0,\n            ANGLEUNIT["degree",0.0174532925199433]],\n        ID["EPSG",4326]],\n    CONVERSION["unnamed",\n        METHOD["Sinusoidal"],\n        PARAMETER["False easting",0,\n            LENGTHUNIT["m",1],\n            ID["EPSG",8806]],\n        PARAMETER["False northing",0,\n            LENGTHUNIT["m",1],\n            ID["EPSG",8807]],\n        PARAMETER["Longitude of natural origin",0,\n            ANGLEUNIT["degree",0.0174532925199433],\n            ID["EPSG",8802]],\n        PARAMETER["semi_major",6371007.181],\n        PARAMETER["semi_minor",6371007.181]],\n    CS[Cartesian,2],\n        AXIS["easting",east,\n            ORDER[1],\n            LENGTHUNIT["m",1]],\n        AXIS["northing",north,\n            ORDER[2],\n            LENGTHUNIT["m",1]],\n    ID["SR-ORG",6974]]',
            "dataAxisToSRSAxisMapping": [1, 2],
        },
        "geoTransform": [
            -20015109.354,
            926.625433056,
            0.0,
            10007554.677,
            0.0,
            -926.625433055,
        ],
        "metadata": {
            "": {"google": "max_source_file_timestamp=1495924664224"},
            "IMAGE_STRUCTURE": {"INTERLEAVE": "PIXEL"},
            "SUBDATASETS": {
                "SUBDATASET_1_NAME": "EEDAI:MODIS/006/MYD09GA/2017_05_24:num_observations_1km,state_1km,SensorZenith,SensorAzimuth,Range,SolarZenith,SolarAzimuth,gflags,orbit_pnt,granule_pnt",
                "SUBDATASET_1_DESC": "Bands num_observations_1km,state_1km,SensorZenith,SensorAzimuth,Range,SolarZenith,SolarAzimuth,gflags,orbit_pnt,granule_pnt of MODIS/006/MYD09GA/2017_05_24",
                "SUBDATASET_2_NAME": "EEDAI:MODIS/006/MYD09GA/2017_05_24:num_observations_500m,sur_refl_b01,sur_refl_b02,sur_refl_b03,sur_refl_b04,sur_refl_b05,sur_refl_b06,sur_refl_b07,QC_500m,obscov_500m,iobs_res,q_scan",
                "SUBDATASET_2_DESC": "Bands num_observations_500m,sur_refl_b01,sur_refl_b02,sur_refl_b03,sur_refl_b04,sur_refl_b05,sur_refl_b06,sur_refl_b07,QC_500m,obscov_500m,iobs_res,q_scan of MODIS/006/MYD09GA/2017_05_24",
            },
        },
        "cornerCoordinates": {
            "upperLeft": [-20015109.354, 10007554.677],
            "lowerLeft": [-20015109.354, -10007554.677],
            "lowerRight": [20015109.354, -10007554.677],
            "upperRight": [20015109.354, 10007554.677],
            "center": [9.6e-06, 6e-06],
        },
        "wgs84Extent": {"type": "Polygon", "coordinates": [[]]},
        "bands": [
            {
                "band": 1,
                "block": [256, 256],
                "type": "Int8",
                "colorInterpretation": "Undefined",
                "description": "num_observations_1km",
                "overviews": [
                    {"size": [21600, 10800]},
                    {"size": [10800, 5400]},
                    {"size": [5400, 2700]},
                    {"size": [2700, 1350]},
                    {"size": [1350, 675]},
                    {"size": [675, 337]},
                    {"size": [337, 168]},
                    {"size": [168, 84]},
                ],
                "metadata": {},
            },
            {
                "band": 2,
                "block": [256, 256],
                "type": "UInt16",
                "colorInterpretation": "Undefined",
                "description": "state_1km",
                "overviews": [
                    {"size": [21600, 10800]},
                    {"size": [10800, 5400]},
                    {"size": [5400, 2700]},
                    {"size": [2700, 1350]},
                    {"size": [1350, 675]},
                    {"size": [675, 337]},
                    {"size": [337, 168]},
                    {"size": [168, 84]},
                ],
                "metadata": {},
            },
            {
                "band": 3,
                "block": [256, 256],
                "type": "Int16",
                "colorInterpretation": "Undefined",
                "description": "SensorZenith",
                "overviews": [
                    {"size": [21600, 10800]},
                    {"size": [10800, 5400]},
                    {"size": [5400, 2700]},
                    {"size": [2700, 1350]},
                    {"size": [1350, 675]},
                    {"size": [675, 337]},
                    {"size": [337, 168]},
                    {"size": [168, 84]},
                ],
                "metadata": {},
            },
            {
                "band": 4,
                "block": [256, 256],
                "type": "Int16",
                "colorInterpretation": "Undefined",
                "description": "SensorAzimuth",
                "overviews": [
                    {"size": [21600, 10800]},
                    {"size": [10800, 5400]},
                    {"size": [5400, 2700]},
                    {"size": [2700, 1350]},
                    {"size": [1350, 675]},
                    {"size": [675, 337]},
                    {"size": [337, 168]},
                    {"size": [168, 84]},
                ],
                "metadata": {},
            },
            {
                "band": 5,
                "block": [256, 256],
                "type": "UInt16",
                "colorInterpretation": "Undefined",
                "description": "Range",
                "overviews": [
                    {"size": [21600, 10800]},
                    {"size": [10800, 5400]},
                    {"size": [5400, 2700]},
                    {"size": [2700, 1350]},
                    {"size": [1350, 675]},
                    {"size": [675, 337]},
                    {"size": [337, 168]},
                    {"size": [168, 84]},
                ],
                "metadata": {},
            },
            {
                "band": 6,
                "block": [256, 256],
                "type": "Int16",
                "colorInterpretation": "Undefined",
                "description": "SolarZenith",
                "overviews": [
                    {"size": [21600, 10800]},
                    {"size": [10800, 5400]},
                    {"size": [5400, 2700]},
                    {"size": [2700, 1350]},
                    {"size": [1350, 675]},
                    {"size": [675, 337]},
                    {"size": [337, 168]},
                    {"size": [168, 84]},
                ],
                "metadata": {},
            },
            {
                "band": 7,
                "block": [256, 256],
                "type": "Int16",
                "colorInterpretation": "Undefined",
                "description": "SolarAzimuth",
                "overviews": [
                    {"size": [21600, 10800]},
                    {"size": [10800, 5400]},
                    {"size": [5400, 2700]},
                    {"size": [2700, 1350]},
                    {"size": [1350, 675]},
                    {"size": [675, 337]},
                    {"size": [337, 168]},
                    {"size": [168, 84]},
                ],
                "metadata": {},
            },
            {
                "band": 8,
                "block": [256, 256],
                "type": "Byte",
                "colorInterpretation": "Undefined",
                "description": "gflags",
                "overviews": [
                    {"size": [21600, 10800]},
                    {"size": [10800, 5400]},
                    {"size": [5400, 2700]},
                    {"size": [2700, 1350]},
                    {"size": [1350, 675]},
                    {"size": [675, 337]},
                    {"size": [337, 168]},
                    {"size": [168, 84]},
                ],
                "metadata": {},
            },
            {
                "band": 9,
                "block": [256, 256],
                "type": "Int8",
                "colorInterpretation": "Undefined",
                "description": "orbit_pnt",
                "overviews": [
                    {"size": [21600, 10800]},
                    {"size": [10800, 5400]},
                    {"size": [5400, 2700]},
                    {"size": [2700, 1350]},
                    {"size": [1350, 675]},
                    {"size": [675, 337]},
                    {"size": [337, 168]},
                    {"size": [168, 84]},
                ],
                "metadata": {},
            },
            {
                "band": 10,
                "block": [256, 256],
                "type": "Byte",
                "colorInterpretation": "Undefined",
                "description": "granule_pnt",
                "overviews": [
                    {"size": [21600, 10800]},
                    {"size": [10800, 5400]},
                    {"size": [5400, 2700]},
                    {"size": [2700, 1350]},
                    {"size": [1350, 675]},
                    {"size": [675, 337]},
                    {"size": [337, 168]},
                    {"size": [168, 84]},
                ],
                "metadata": {},
            },
        ],
        "stac": {
            "proj:shape": [43200, 21600],
            "proj:wkt2": 'PROJCRS["MODIS Sinusoidal",\n    BASEGEOGCRS["WGS 84",\n        DATUM["World Geodetic System 1984",\n            ELLIPSOID["WGS 84",6378137,298.257223563,\n                LENGTHUNIT["metre",1]]],\n        PRIMEM["Greenwich",0,\n            ANGLEUNIT["degree",0.0174532925199433]],\n        ID["EPSG",4326]],\n    CONVERSION["unnamed",\n        METHOD["Sinusoidal"],\n        PARAMETER["False easting",0,\n            LENGTHUNIT["m",1],\n            ID["EPSG",8806]],\n        PARAMETER["False northing",0,\n            LENGTHUNIT["m",1],\n            ID["EPSG",8807]],\n        PARAMETER["Longitude of natural origin",0,\n            ANGLEUNIT["degree",0.0174532925199433],\n            ID["EPSG",8802]],\n        PARAMETER["semi_major",6371007.181],\n        PARAMETER["semi_minor",6371007.181]],\n    CS[Cartesian,2],\n        AXIS["easting",east,\n            ORDER[1],\n            LENGTHUNIT["m",1]],\n        AXIS["northing",north,\n            ORDER[2],\n            LENGTHUNIT["m",1]],\n    ID["SR-ORG",6974]]',
            "proj:epsg": None,
            "proj:projjson": {
                "$schema": "https://proj.org/schemas/v0.7/projjson.schema.json",
                "type": "ProjectedCRS",
                "name": "MODIS Sinusoidal",
                "base_crs": {
                    "name": "WGS 84",
                    "datum": {
                        "type": "GeodeticReferenceFrame",
                        "name": "World Geodetic System 1984",
                        "ellipsoid": {
                            "name": "WGS 84",
                            "semi_major_axis": 6378137,
                            "inverse_flattening": 298.257223563,
                        },
                    },
                    "coordinate_system": {
                        "subtype": "ellipsoidal",
                        "axis": [
                            {
                                "name": "Geodetic latitude",
                                "abbreviation": "Lat",
                                "direction": "north",
                                "unit": "degree",
                            },
                            {
                                "name": "Geodetic longitude",
                                "abbreviation": "Lon",
                                "direction": "east",
                                "unit": "degree",
                            },
                        ],
                    },
                    "id": {"authority": "EPSG", "code": 4326},
                },
                "conversion": {
                    "name": "unnamed",
                    "method": {"name": "Sinusoidal"},
                    "parameters": [
                        {
                            "name": "False easting",
                            "value": 0,
                            "unit": {
                                "type": "LinearUnit",
                                "name": "m",
                                "conversion_factor": 1,
                            },
                            "id": {"authority": "EPSG", "code": 8806},
                        },
                        {
                            "name": "False northing",
                            "value": 0,
                            "unit": {
                                "type": "LinearUnit",
                                "name": "m",
                                "conversion_factor": 1,
                            },
                            "id": {"authority": "EPSG", "code": 8807},
                        },
                        {
                            "name": "Longitude of natural origin",
                            "value": 0,
                            "unit": "degree",
                            "id": {"authority": "EPSG", "code": 8802},
                        },
                        {
                            "name": "semi_major",
                            "value": 6371007.181,
                            "unit": {
                                "type": "Unit",
                                "name": "",
                                "conversion_factor": 1,
                            },
                        },
                        {
                            "name": "semi_minor",
                            "value": 6371007.181,
                            "unit": {
                                "type": "Unit",
                                "name": "",
                                "conversion_factor": 1,
                            },
                        },
                    ],
                },
                "coordinate_system": {
                    "subtype": "Cartesian",
                    "axis": [
                        {
                            "name": "Easting",
                            "abbreviation": "",
                            "direction": "east",
                            "unit": {
                                "type": "LinearUnit",
                                "name": "m",
                                "conversion_factor": 1,
                            },
                        },
                        {
                            "name": "Northing",
                            "abbreviation": "",
                            "direction": "north",
                            "unit": {
                                "type": "LinearUnit",
                                "name": "m",
                                "conversion_factor": 1,
                            },
                        },
                    ],
                },
                "id": {"authority": "SR-ORG", "code": 6974},
            },
            "proj:transform": [
                -20015109.354,
                926.625433056,
                0.0,
                10007554.677,
                0.0,
                -926.625433055,
            ],
            "raster:bands": [
                {"data_type": "int8"},
                {"data_type": "uint16"},
                {"data_type": "int16"},
                {"data_type": "int16"},
                {"data_type": "uint16"},
                {"data_type": "int16"},
                {"data_type": "int16"},
                {"data_type": "uint8"},
                {"data_type": "int8"},
                {"data_type": "uint8"},
            ],
            "eo:bands": [
                {"name": "b1", "description": "num_observations_1km"},
                {"name": "b2", "description": "state_1km"},
                {"name": "b3", "description": "SensorZenith"},
                {"name": "b4", "description": "SensorAzimuth"},
                {"name": "b5", "description": "Range"},
                {"name": "b6", "description": "SolarZenith"},
                {"name": "b7", "description": "SolarAzimuth"},
                {"name": "b8", "description": "gflags"},
                {"name": "b9", "description": "orbit_pnt"},
                {"name": "b10", "description": "granule_pnt"},
            ],
        },
    }
    assert expected == res
    assert ds.ReadRaster(0, 0, 1, 1) is not None
