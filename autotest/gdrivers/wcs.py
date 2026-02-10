#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test WCS client support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import re
import shutil
import urllib.parse
from http.server import BaseHTTPRequestHandler

import gdaltest
import pytest
import webserver

from osgeo import gdal

pytestmark = pytest.mark.require_driver("WCS")

###############################################################################
# Open the GeoServer WCS service.


@pytest.fixture(scope="module")
def geoserver_ds(tmp_path_factory):

    wcs_fname = str(tmp_path_factory.mktemp("tmp") / "geoserver.wcs")

    # first, copy to tmp directory.
    shutil.copy("data/geoserver.wcs", wcs_fname)

    try:
        with gdal.config_option("GDAL_HTTP_CONNECTTIMEOUT", "3"):
            return gdal.Open(wcs_fname)
    except RuntimeError:
        import xml.etree.ElementTree as ET

        tree = ET.parse(wcs_fname)

        srv = next(tree.iter("ServiceURL")).text

        pytest.skip(f"Could not connect to {srv}")


###############################################################################
# Check various things about the configuration.


def test_wcs_3(geoserver_ds):

    assert (
        geoserver_ds.RasterXSize == 983
        and geoserver_ds.RasterYSize == 598
        and geoserver_ds.RasterCount == 3
    ), "wrong size or bands"

    wkt = geoserver_ds.GetProjectionRef()
    assert wkt[:14] == 'GEOGCS["WGS 84', "Got wrong SRS: " + wkt

    gt = geoserver_ds.GetGeoTransform()
    expected_gt = (
        -130.85167999999999,
        0.070036907426246159,
        0.0,
        54.114100000000001,
        0.0,
        -0.055867725752508368,
    )
    gdaltest.check_geotransform(gt, expected_gt)

    assert (
        geoserver_ds.GetRasterBand(1).DataType == gdal.GDT_UInt8
    ), "wrong band data type"


###############################################################################
# Check checksum


def test_wcs_4(geoserver_ds):

    cs = geoserver_ds.GetRasterBand(1).Checksum()
    assert cs == 58765, "Wrong checksum: " + str(cs)


###############################################################################
# Open the service using XML as filename.


def wcs_5():

    if gdaltest.wcs_drv is None:
        pytest.skip()

    fn = """<WCS_GDAL>
  <ServiceURL>http://demo.opengeo.org/geoserver/wcs?</ServiceURL>
  <CoverageName>Img_Sample</CoverageName>
</WCS_GDAL>
"""

    ds = gdal.Open(fn)

    assert ds is not None, "open failed."

    assert (
        ds.RasterXSize == 983 and ds.RasterYSize == 598 and ds.RasterCount == 3
    ), "wrong size or bands"

    ds = None


###############################################################################
# Open the srtm plus service.


def old_wcs_2():

    if gdaltest.wcs_drv is None:
        pytest.skip()

    # first, copy to tmp directory.
    open("tmp/srtmplus.wcs", "w").write(open("data/srtmplus.wcs").read())

    gdaltest.wcs_ds = None
    gdaltest.wcs_ds = gdal.Open("tmp/srtmplus.wcs")

    if gdaltest.wcs_ds is not None:
        return
    pytest.fail("open failed.")


###############################################################################
# Check various things about the configuration.


def old_wcs_3():

    if gdaltest.wcs_drv is None or gdaltest.wcs_ds is None:
        pytest.skip()

    assert (
        gdaltest.wcs_ds.RasterXSize == 43200
        and gdaltest.wcs_ds.RasterYSize == 21600
        and gdaltest.wcs_ds.RasterCount == 1
    ), "wrong size or bands"

    wkt = gdaltest.wcs_ds.GetProjectionRef()
    assert wkt[:12] == 'GEOGCS["NAD8', "Got wrong SRS: " + wkt

    gt = gdaltest.wcs_ds.GetGeoTransform()
    assert (
        gt[0] == pytest.approx(-180.0041667, abs=0.00001)
        and gt[3] == pytest.approx(90.004167, abs=0.00001)
        and gt[1] == pytest.approx(0.00833333, abs=0.00001)
        and gt[2] == pytest.approx(0, abs=0.00001)
        and gt[5] == pytest.approx(-0.00833333, abs=0.00001)
        and gt[4] == pytest.approx(0, abs=0.00001)
    ), "wrong geotransform"

    assert gdaltest.wcs_ds.GetRasterBand(1).GetOverviewCount() >= 1, "no overviews!"

    assert (
        gdaltest.wcs_ds.GetRasterBand(1).DataType >= gdal.GDT_Int16
    ), "wrong band data type"


###############################################################################
# Check checksum for a small region.


def old_wcs_4():

    if gdaltest.wcs_drv is None or gdaltest.wcs_ds is None:
        pytest.skip()

    cs = gdaltest.wcs_ds.GetRasterBand(1).Checksum(0, 0, 100, 100)
    assert cs == 10469, "Wrong checksum: " + str(cs)


###############################################################################
# Open the srtm plus service using XML as filename.


def old_wcs_5():

    if gdaltest.wcs_drv is None:
        pytest.skip()

    fn = "<WCS_GDAL><ServiceURL>http://geodata.telascience.org/cgi-bin/mapserv_dem?</ServiceURL><CoverageName>srtmplus_raw</CoverageName><Timeout>75</Timeout></WCS_GDAL>"

    ds = gdal.Open(fn)

    assert ds is not None, "open failed."

    assert (
        ds.RasterXSize == 43200 and ds.RasterYSize == 21600 and ds.RasterCount == 1
    ), "wrong size or bands"

    ds = None


###############################################################################

# utilities


def read_urls():
    retval = {}
    fname = "data/wcs/urls"
    f = open(fname, "rb")
    text = f.read().decode("utf-8")
    f.close()
    for line in text.splitlines():
        items = line.split()
        if items[1].endswith("2"):
            items[1] = items[1][:-1]
        if not items[0] in retval:
            retval[items[0]] = {}
        retval[items[0]][items[1]] = items[2]
    return retval


do_log = False


def compare_urls(a, b):
    """Compare two WCS URLs taking into account that some items are floats"""
    a_list = a.split("&")
    b_list = b.split("&")
    n = len(a_list)
    if n != len(b_list):
        return False
    for i in range(n):
        if not re.match("SUBSET=", a_list[i]):
            if a_list[i] != b_list[i]:
                print(a_list[i], "!=", b_list[i])
                return False
            continue
        x = a_list[i]
        y = b_list[i]
        for c in ("SUBSET=[a-zA-Z]+", "%28", "%29"):
            x = re.sub(c, "", x)
            y = re.sub(c, "", y)
        x_list = x.split(",")
        y_list = y.split(",")
        m = len(x_list)
        if m != len(y_list):
            return False
        for j in range(m):
            try:
                c1 = float(x_list[j])
                c2 = float(y_list[j])
            except Exception as e:
                print(repr(e))
                return False
            if c1 == c2:
                continue
            if abs((c1 - c2) / c1) > 0.001:
                return False
    return True


class WCSHTTPHandler(BaseHTTPRequestHandler):
    def __init__(self, *args):
        self.urls = read_urls()
        super().__init__(*args)

    def log_request(self, code=",", size=","):
        # pylint: disable=unused-argument
        pass

    def Headers(self, typ):
        self.send_response(200)
        self.send_header("Content-Type", typ)
        self.end_headers()

    def Respond(self, request, brand, version, test):
        fname = "data/wcs/"
        if request == "GetCapabilities":
            # *2 and Simple* are different coverages from same server
            brand = brand.replace("2", "")
            brand = brand.replace("Simple", "")
        if request == "GetCoverage" and test == "scaled":
            suffix = ".tiff"
            self.Headers("image/tiff")
            fname += brand + "-" + version + "-scaled" + suffix
        elif request == "GetCoverage" and test == "non_scaled":
            suffix = ".tiff"
            self.Headers("image/tiff")
            fname += brand + "-" + version + "-non_scaled" + suffix
        elif request == "GetCoverage":
            suffix = ".tiff"
            self.Headers("image/tiff")
            fname += brand + "-" + version + suffix
        else:
            suffix = ".xml"
            self.Headers("application/xml")
            fname += request + "-" + brand + "-" + version + suffix

        try:
            with open(fname, "rb") as f:
                content = f.read()
            self.wfile.write(content)
        except IOError:
            self.send_error(
                404, "File Not Found: " + request + " " + brand + " " + version
            )

    def do_GET(self):
        if do_log:
            f = open("/tmp/log.txt", "a")
            f.write("GET %s\n" % self.path)
            f.close()
        split = urllib.parse.urlparse(self.path)
        query = urllib.parse.parse_qs(split.query)
        query2 = {}
        for key in query:
            query2[key.lower()] = query[key]
        server = query2["server"][0]
        version = query2["version"][0]
        request = query2["request"][0]
        test = ""
        if "test" in query2:
            test = query2["test"][0]

        key = server + "-" + version
        if key in self.urls and test in self.urls[key]:
            _, got = self.path.split("SERVICE=WCS")
            got = re.sub(r"\&test=.*", "", got)
            _, have = self.urls[key][test].split("SERVICE=WCS")
            have += "&server=" + server

            assert compare_urls(got, have)

        self.Respond(request, server, version, test)


def setupFct():
    return {
        "SimpleGeoServer": {
            "URL": "https://msp.smartsea.fmi.fi/geoserver/wcs",
            "Options": ["", "-oo OuterExtents", "-oo OuterExtents", ""],
            "Projwin": "-projwin 145300 6737500 209680 6688700",
            "Outsize": "-outsize $size 0",
            "Coverage": [
                "smartsea:eusm2016",
                "smartsea:eusm2016",
                "smartsea:eusm2016",
                "smartsea__eusm2016",
            ],
            "Versions": [100, 110, 111, 201],
        },
        "GeoServer2": {
            "URL": "https://msp.smartsea.fmi.fi/geoserver/wcs",
            "Options": [
                "",
                "-oo OuterExtents -oo NoGridAxisSwap",
                "-oo OuterExtents -oo NoGridAxisSwap",
                "-oo NoGridAxisSwap -oo SubsetAxisSwap",
            ],
            "Projwin": "-projwin 145300 6737500 209680 6688700",
            "Outsize": "-outsize $size 0",
            "Coverage": [
                "smartsea:south",
                "smartsea:south",
                "smartsea:south",
                "smartsea__south",
            ],
            "Versions": [100, 110, 111, 201],
            "Range": ["GREEN_BAND", "BLUE_BAND"],
        },
        "GeoServer": {
            "URL": "https://msp.smartsea.fmi.fi/geoserver/wcs",
            "Options": [
                "",
                "-oo OuterExtents -oo BufSizeAdjust=0.5 -oo NoGridAxisSwap",
                "-oo OuterExtents -oo BufSizeAdjust=0.5 -oo NoGridAxisSwap",
                "-oo NoGridAxisSwap -oo SubsetAxisSwap",
            ],
            "Projwin": "-projwin 3200000 6670000 3280000 6620000",
            "Outsize": "-outsize $size 0",
            "Coverage": [
                "smartsea:eusm2016-EPSG2393",
                "smartsea:eusm2016-EPSG2393",
                "smartsea:eusm2016-EPSG2393",
                "smartsea__eusm2016-EPSG2393",
            ],
            "Versions": [100, 110, 111, 201],
        },
        "MapServer": {
            "URL": "http://194.66.252.155/cgi-bin/BGS_EMODnet_bathymetry/ows",
            "Options": [
                "-oo INTERLEAVE=PIXEL -oo OriginAtBoundary -oo BandIdentifier=none",
                "-oo INTERLEAVE=PIXEL -oo OffsetsPositive -oo NrOffsets=2 -oo NoGridAxisSwap -oo BandIdentifier=none",
                "-oo INTERLEAVE=PIXEL -oo OffsetsPositive -oo NrOffsets=2 -oo NoGridAxisSwap -oo BandIdentifier=none",
                "-oo INTERLEAVE=PIXEL -oo OffsetsPositive -oo NrOffsets=2 -oo NoGridAxisSwap -oo BandIdentifier=none",
                "-oo OriginAtBoundary",
            ],
            "Projwin": "-projwin 10 45 15 35",
            "Outsize": "-outsize $size 0",
            "Coverage": "BGS_EMODNET_CentralMed-MCol",
            "Versions": [100, 110, 111, 112, 201],
        },
        "Rasdaman": {
            "URL": "http://ows.rasdaman.org/rasdaman/ows",
            "Options": "",
            "Projwin": "-projwin 10 45 15 35",
            "Outsize": "-outsize $size 0",
            "Coverage": "BlueMarbleCov",
            "Versions": [201],
        },
        "Rasdaman2": {
            "URL": "http://ows.rasdaman.org/rasdaman/ows",
            "Options": '-oo subset=unix("2008-01-05T01:58:30.000Z")',
            "Projwin": "-projwin 100000 5400000 150000 5100000",
            "Outsize": "-outsize $size 0",
            "Coverage": "test_irr_cube_2",
            "Versions": [201],
            "Dimension": 'unix("2008-01-05T01:58:30.000Z")',
        },
        "ArcGIS": {
            "URL": "http://paikkatieto.ymparisto.fi/arcgis/services/Testit/Velmu_wcs_testi/MapServer/WCSServer",
            "Options": [
                "",
                "-oo NrOffsets=2",
                "-oo NrOffsets=2",
                "-oo NrOffsets=2",
                "-oo UseScaleFactor",
            ],
            "Projwin": "-projwin 181000 7005000 200000 6980000",
            "Outsize": "-outsize $size 0",
            "Coverage": [2, 2, 2, 2, "Coverage2"],
            "Versions": [100, 110, 111, 112, 201],
        },
    }


###############################################################################


@pytest.fixture(scope="module")
def wcs_server():
    process, port = webserver.launch(handler=WCSHTTPHandler)

    url = "http://127.0.0.1:" + str(port)

    yield url

    webserver.server_stop(process, port)


@gdaltest.disable_exceptions()
@pytest.mark.parametrize(
    "server,version",
    [
        (server, version)
        for server, x in setupFct().items()
        for version in x["Versions"]
    ],
)
def test_wcs_6(wcs_server, tmp_path, server, version):

    cache_dir = str(tmp_path / "wcs_cache")

    # Generating various URLs from the driver and comparing them to ones
    # that have worked.
    size = 60
    cache = f"CACHE={cache_dir}"
    urls = read_urls()
    url = wcs_server
    setup = setupFct()

    i = setup[server]["Versions"].index(version)

    version = (
        str(int(version / 100))
        + "."
        + str(int(version % 100 / 10))
        + "."
        + str((version % 10))
    )
    if not server + "-" + version in urls:
        pytest.fail("Error: " + server + "-" + version + " not in urls")

    options = [cache]

    # 1. Open without requesting a specific coverage
    query = "server=" + server + "&version=" + version
    ds = gdal.OpenEx(utf8_path="WCS:" + url + "/?" + query, open_options=options)
    assert ds is not None, f"OpenEx failed: WCS:{url}/?{query}"

    # 2. Open and request a specific coverage
    coverage = setup[server]["Coverage"]
    if isinstance(coverage, list):
        coverage = coverage[i]
    query += f"&coverage={coverage}"

    options = [cache]
    if isinstance(setup[server]["Options"], list):
        oo = setup[server]["Options"][i]
    else:
        oo = setup[server]["Options"]
    oo = oo.split()
    for o in oo:
        if o != "-oo":
            options.append(o)
    options.append("GetCoverageExtra=test=none")
    gdal.OpenEx(utf8_path="WCS:" + url + "/?" + query, open_options=options)
    assert ds is not None, f"OpenEx failed: WCS:{url}/?{query}"
    ds = None

    # 3. Open and request a specific coverage, scaled
    options = [cache]
    options.append("GetCoverageExtra=test=scaled")
    options.append("INTERLEAVE=PIXEL")
    ds = gdal.OpenEx(utf8_path="WCS:" + url + "/?" + query, open_options=options)

    assert ds is not None, f"OpenEx failed: WCS:{url}/?{query}"

    projwin = [
        int(x) for x in setup[server]["Projwin"].replace("-projwin ", "").split()
    ]

    tmpfile = tmp_path / f"{server}{version}.tiff"
    # use resampleAlg = "bilinear" to prevent snapping projWin to pixel boundaries
    gdal.Translate(
        str(tmpfile), ds, projWin=projwin, width=size, resampleAlg="bilinear"
    )

    assert tmpfile.exists()

    # 4. Open and request a specific coverage, non-scaled
    if os.path.isfile("data/wcs/" + server + "-" + version + "-non_scaled.tiff"):
        tmpfile_non_scaled = tmp_path / f"{server}{version}-non_scaled.tiff"

        options = [cache]
        options.append("GetCoverageExtra=test=non_scaled")
        options.append("INTERLEAVE=PIXEL")
        ds = gdal.OpenEx(utf8_path="WCS:" + url + "/?" + query, open_options=options)

        assert ds is not None, f"OpenEx failed: WCS:{url}/?{query}"

        gdal.Translate(str(tmpfile_non_scaled), ds, srcWin=[0, 0, 2, 2])

        assert tmpfile_non_scaled.exists()
    else:
        print(server + " " + version + " non_scaled skipped (no response file)")


###############################################################################

# todo tests:

# test that nothing is put into cache if request fails
# parsing Capabilities and DescribeCoverage: test data in metadata and service files?

###############################################################################
