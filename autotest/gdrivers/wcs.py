#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test WCS client support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
import sys
import numbers
import re
import shutil
import urlparse
try:
    from BaseHTTPServer import BaseHTTPRequestHandler
except ImportError:
    from http.server import BaseHTTPRequestHandler

from osgeo import gdal

sys.path.append('../pymod')

import webserver
import gdaltest

###############################################################################
# Verify we have the driver.


def wcs_1():

    # Disable wcs tests till we have a more reliable test server.
    gdaltest.wcs_drv = None

    try:
        gdaltest.wcs_drv = gdal.GetDriverByName('WCS')
    except:
        gdaltest.wcs_drv = None

    # NOTE - mloskot:
    # This is a dirty hack checking if remote WCS service is online.
    # Nothing genuine but helps to keep the buildbot waterfall green.
    srv = 'http://demo.opengeo.org/geoserver/wcs?'
    if gdaltest.gdalurlopen(srv) is None:
        gdaltest.wcs_drv = None

    gdaltest.wcs_ds = None
    if gdaltest.wcs_drv is None:
        return 'skip'
    else:
        return 'success'

###############################################################################
# Open the GeoServer WCS service.


def wcs_2():

    if gdaltest.wcs_drv is None:
        return 'skip'

    # first, copy to tmp directory.
    open('tmp/geoserver.wcs', 'w').write(open('data/geoserver.wcs').read())

    gdaltest.wcs_ds = None
    gdaltest.wcs_ds = gdal.Open('tmp/geoserver.wcs')

    if gdaltest.wcs_ds is not None:
        return 'success'
    else:
        gdaltest.post_reason('open failed.')
        return 'fail'

###############################################################################
# Check various things about the configuration.


def wcs_3():

    if gdaltest.wcs_drv is None or gdaltest.wcs_ds is None:
        return 'skip'

    if gdaltest.wcs_ds.RasterXSize != 983 \
       or gdaltest.wcs_ds.RasterYSize != 598 \
       or gdaltest.wcs_ds.RasterCount != 3:
        gdaltest.post_reason('wrong size or bands')
        print(gdaltest.wcs_ds.RasterXSize)
        print(gdaltest.wcs_ds.RasterYSize)
        print(gdaltest.wcs_ds.RasterCount)
        return 'fail'

    wkt = gdaltest.wcs_ds.GetProjectionRef()
    if wkt[:14] != 'GEOGCS["WGS 84':
        gdaltest.post_reason('Got wrong SRS: ' + wkt)
        return 'fail'

    gt = gdaltest.wcs_ds.GetGeoTransform()
    expected_gt = (-130.85167999999999, 0.070036907426246159, 0.0, 54.114100000000001, 0.0, -0.055867725752508368)
    for i in range(6):
        if abs(gt[i] - expected_gt[i]) > 0.00001:
            gdaltest.post_reason('wrong geotransform')
            print(gt)
            return 'fail'

    if gdaltest.wcs_ds.GetRasterBand(1).GetOverviewCount() < 1:
        gdaltest.post_reason('no overviews!')
        return 'fail'

    if gdaltest.wcs_ds.GetRasterBand(1).DataType != gdal.GDT_Byte:
        gdaltest.post_reason('wrong band data type')
        return 'fail'

    return 'success'

###############################################################################
# Check checksum


def wcs_4():

    if gdaltest.wcs_drv is None or gdaltest.wcs_ds is None:
        return 'skip'

    cs = gdaltest.wcs_ds.GetRasterBand(1).Checksum()
    if cs != 58765:
        gdaltest.post_reason('Wrong checksum: ' + str(cs))
        return 'fail'

    return 'success'

###############################################################################
# Open the service using XML as filename.


def wcs_5():

    if gdaltest.wcs_drv is None:
        return 'skip'

    fn = """<WCS_GDAL>
  <ServiceURL>http://demo.opengeo.org/geoserver/wcs?</ServiceURL>
  <CoverageName>Img_Sample</CoverageName>
</WCS_GDAL>
"""

    ds = gdal.Open(fn)

    if ds is None:
        gdaltest.post_reason('open failed.')
        return 'fail'

    if ds.RasterXSize != 983 \
       or ds.RasterYSize != 598 \
       or ds.RasterCount != 3:
        gdaltest.post_reason('wrong size or bands')
        print(ds.RasterXSize)
        print(ds.RasterYSize)
        print(ds.RasterCount)
        return 'fail'

    ds = None

    return 'success'
###############################################################################
# Open the srtm plus service.


def old_wcs_2():

    if gdaltest.wcs_drv is None:
        return 'skip'

    # first, copy to tmp directory.
    open('tmp/srtmplus.wcs', 'w').write(open('data/srtmplus.wcs').read())

    gdaltest.wcs_ds = None
    gdaltest.wcs_ds = gdal.Open('tmp/srtmplus.wcs')

    if gdaltest.wcs_ds is not None:
        return 'success'
    else:
        gdaltest.post_reason('open failed.')
        return 'fail'

###############################################################################
# Check various things about the configuration.


def old_wcs_3():

    if gdaltest.wcs_drv is None or gdaltest.wcs_ds is None:
        return 'skip'

    if gdaltest.wcs_ds.RasterXSize != 43200 \
       or gdaltest.wcs_ds.RasterYSize != 21600 \
       or gdaltest.wcs_ds.RasterCount != 1:
        gdaltest.post_reason('wrong size or bands')
        return 'fail'

    wkt = gdaltest.wcs_ds.GetProjectionRef()
    if wkt[:12] != 'GEOGCS["NAD8':
        gdaltest.post_reason('Got wrong SRS: ' + wkt)
        return 'fail'

    gt = gdaltest.wcs_ds.GetGeoTransform()
    if abs(gt[0] - -180.0041667) > 0.00001 \
       or abs(gt[3] - 90.004167) > 0.00001 \
       or abs(gt[1] - 0.00833333) > 0.00001 \
       or abs(gt[2] - 0) > 0.00001 \
       or abs(gt[5] - -0.00833333) > 0.00001 \
       or abs(gt[4] - 0) > 0.00001:
        gdaltest.post_reason('wrong geotransform')
        print(gt)
        return 'fail'

    if gdaltest.wcs_ds.GetRasterBand(1).GetOverviewCount() < 1:
        gdaltest.post_reason('no overviews!')
        return 'fail'

    if gdaltest.wcs_ds.GetRasterBand(1).DataType < gdal.GDT_Int16:
        gdaltest.post_reason('wrong band data type')
        return 'fail'

    return 'success'

###############################################################################
# Check checksum for a small region.


def old_wcs_4():

    if gdaltest.wcs_drv is None or gdaltest.wcs_ds is None:
        return 'skip'

    cs = gdaltest.wcs_ds.GetRasterBand(1).Checksum(0, 0, 100, 100)
    if cs != 10469:
        gdaltest.post_reason('Wrong checksum: ' + str(cs))
        return 'fail'

    return 'success'

###############################################################################
# Open the srtm plus service using XML as filename.


def old_wcs_5():

    if gdaltest.wcs_drv is None:
        return 'skip'

    fn = '<WCS_GDAL><ServiceURL>http://geodata.telascience.org/cgi-bin/mapserv_dem?</ServiceURL><CoverageName>srtmplus_raw</CoverageName><Timeout>75</Timeout></WCS_GDAL>'

    ds = gdal.Open(fn)

    if ds is None:
        gdaltest.post_reason('open failed.')
        return 'fail'

    if ds.RasterXSize != 43200 \
       or ds.RasterYSize != 21600 \
       or ds.RasterCount != 1:
        gdaltest.post_reason('wrong size or bands')
        return 'fail'

    ds = None

    return 'success'

###############################################################################

# utilities


def read_urls():
    retval = {}
    fname = 'data/wcs/urls'
    f = open(fname, 'rb')
    content = f.read()
    f.close()
    for line in content.splitlines():
        items = line.split()
        if items[1].endswith('2'):
            items[1] = items[1][:-1]
        if not items[0] in retval:
            retval[items[0]] = {}
        retval[items[0]][items[1]] = items[2]
    return retval


do_log = False
wcs_6_ok = True


class WCSHTTPHandler(BaseHTTPRequestHandler):

    def log_request(self, code='-', size='-'):
        return

    def Headers(self, type):
        self.send_response(200)
        self.send_header('Content-Type', type)
        self.end_headers()

    def Respond(self, request, brand, version, test):
        try:
            fname = 'data/wcs/'
            if request == 'GetCapabilities':
                # *2 and Simple* are different coverages from same server
                brand = brand.replace('2', '')
                brand = brand.replace('Simple', '')
            if request == 'GetCoverage' and test == "scaled":
                suffix = '.tiff'
                self.Headers('image/tiff')
                fname += brand + '-' + version + '-scaled' + suffix
            elif request == 'GetCoverage' and test == "non_scaled":
                suffix = '.tiff'
                self.Headers('image/tiff')
                fname += brand + '-' + version + '-non_scaled' + suffix
            elif request == 'GetCoverage':
                suffix = '.tiff'
                self.Headers('image/tiff')
                fname += brand + '-' + version + suffix
            else:
                suffix = '.xml'
                self.Headers('application/xml')
                fname += request + '-' + brand + '-' + version + suffix
            f = open(fname, 'rb')
            content = f.read()
            f.close()
            self.wfile.write(content)
        except IOError:
            self.send_error(404, 'File Not Found: ' + request + ' ' + brand + ' ' + version)
            global wcs_6_ok
            wcs_6_ok = False

    def do_GET(self):
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
        key = server + '-' + version
        if key in urls and test in urls[key]:
            tmp, got = self.path.split('SERVICE=WCS')
            got = re.sub('\&test=.*', '', got)
            tmp, have = urls[key][test].split('SERVICE=WCS')
            have += '&server=' + server
            if got == have:
                ok = 'ok'
            else:
                ok = "not ok\ngot:  " + got + "\nhave: " + have
                global wcs_6_ok
                wcs_6_ok = False
            print('test ' + server + ' ' + test + ' WCS ' + version + ' ' + ok)
        self.Respond(request, server, version, test)
        return


def setupFct():
    return {
        'SimpleGeoServer': {
            'URL': 'https://msp.smartsea.fmi.fi/geoserver/wcs',
            'Options': [
                "",
                "-oo OuterExtents",
                "-oo OuterExtents",
                ""
            ],
            'Projwin': "-projwin 145300 6737500 209680 6688700",
            'Outsize': "-outsize $size 0",
            'Coverage': [
                'smartsea:eusm2016', 'smartsea:eusm2016',
                'smartsea:eusm2016', 'smartsea__eusm2016'],
            'Versions': [100, 110, 111, 201],
        },
        'GeoServer2': {
            'URL': 'https://msp.smartsea.fmi.fi/geoserver/wcs',
            'Options': [
                "",
                "-oo OuterExtents -oo NoGridAxisSwap",
                "-oo OuterExtents -oo NoGridAxisSwap",
                "-oo NoGridAxisSwap -oo SubsetAxisSwap"
            ],
            'Projwin': "-projwin 145300 6737500 209680 6688700",
            'Outsize': "-outsize $size 0",
            'Coverage': ['smartsea:south', 'smartsea:south', 'smartsea:south', 'smartsea__south'],
            'Versions': [100, 110, 111, 201],
            'Range': ['GREEN_BAND', 'BLUE_BAND']
        },
        'GeoServer': {
            'URL': 'https://msp.smartsea.fmi.fi/geoserver/wcs',
            'Options': [
                "",
                "-oo OuterExtents -oo BufSizeAdjust=0.5 -oo NoGridAxisSwap",
                "-oo OuterExtents -oo BufSizeAdjust=0.5 -oo NoGridAxisSwap",
                "-oo NoGridAxisSwap -oo SubsetAxisSwap",
            ],
            'Projwin': "-projwin 3200000 6670000 3280000 6620000",
            'Outsize': "-outsize $size 0",
            'Coverage': [
                'smartsea:eusm2016-EPSG2393', 'smartsea:eusm2016-EPSG2393',
                'smartsea:eusm2016-EPSG2393', 'smartsea__eusm2016-EPSG2393'],
            'Versions': [100, 110, 111, 201]
        },
        'MapServer': {
            'URL': 'http://194.66.252.155/cgi-bin/BGS_EMODnet_bathymetry/ows',
            'Options': [
                "-oo INTERLEAVE=PIXEL -oo OriginAtBoundary -oo BandIdentifier=none",
                "-oo INTERLEAVE=PIXEL -oo OffsetsPositive -oo NrOffsets=2 -oo NoGridAxisSwap -oo BandIdentifier=none",
                "-oo INTERLEAVE=PIXEL -oo OffsetsPositive -oo NrOffsets=2 -oo NoGridAxisSwap -oo BandIdentifier=none",
                "-oo INTERLEAVE=PIXEL -oo OffsetsPositive -oo NrOffsets=2 -oo NoGridAxisSwap -oo BandIdentifier=none",
                "-oo OriginAtBoundary",
            ],
            'Projwin': "-projwin 10 45 15 35",
            'Outsize': "-outsize $size 0",
            'Coverage': 'BGS_EMODNET_CentralMed-MCol',
            'Versions': [100, 110, 111, 112, 201]
        },
        'Rasdaman': {
            'URL': 'http://ows.rasdaman.org/rasdaman/ows',
            'Options': "",
            'Projwin': "-projwin 10 45 15 35",
            'Outsize': "-outsize $size 0",
            'Coverage': 'BlueMarbleCov',
            'Versions': [201]
        },
        'Rasdaman2': {
            'URL': 'http://ows.rasdaman.org/rasdaman/ows',
            'Options': '-oo subset=unix("2008-01-05T01:58:30.000Z")',
            'Projwin': "-projwin 100000 5400000 150000 5100000",
            'Outsize': "-outsize $size 0",
            'Coverage': 'test_irr_cube_2',
            'Versions': [201],
            'Dimension': "unix(\"2008-01-05T01:58:30.000Z\")"
        },
        'ArcGIS': {
            'URL': 'http://paikkatieto.ymparisto.fi/arcgis/services/Testit/Velmu_wcs_testi/MapServer/WCSServer',
            'Options': [
                "",
                "-oo NrOffsets=2",
                "-oo NrOffsets=2",
                "-oo NrOffsets=2",
                "-oo UseScaleFactor"
            ],
            'Projwin': "-projwin 181000 7005000 200000 6980000",
            'Outsize': "-outsize $size 0",
            'Coverage': [2, 2, 2, 2, 'Coverage2'],
            'Versions': [100, 110, 111, 112, 201]
        }
    }

###############################################################################


def wcs_6():
    driver = gdal.GetDriverByName('WCS')
    if driver is None:
        return 'skip'
    # Generating various URLs from the driver and comparing them to ones
    # that have worked.
    first_call = True
    size = 60
    cache = 'CACHE=wcs_cache'
    global urls
    urls = read_urls()
    (process, port) = webserver.launch(handler=WCSHTTPHandler)
    url = "http://127.0.0.1:" + str(port)
    setup = setupFct()
    servers = []
    for server in setup:
        servers.append(server)
    for server in sorted(servers):
        for i, v in enumerate(setup[server]['Versions']):
            version = str(int(v / 100)) + '.' + str(int(v % 100 / 10)) + '.' + str((v % 10))
            if not server + '-' + version in urls:
                print("Error: " + server + '-' + version + " not in urls")
                global wcs_6_ok
                wcs_6_ok = False
                continue
            options = [cache]
            if first_call:
                options.append('CLEAR_CACHE')
                first_call = False
            query = 'server=' + server + '&version=' + version
            ds = gdal.OpenEx(utf8_path="WCS:" + url + "/?" + query,
                             open_options=options)

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
            oo = oo.split()
            for o in oo:
                if o != '-oo':
                    options.append(o)
            options.append('GetCoverageExtra=test=none')
            ds = gdal.OpenEx(utf8_path="WCS:" + url + "/?" + query,
                             open_options=options)
            ds = 0
            options = [cache]
            options.append('GetCoverageExtra=test=scaled')
            options.append('INTERLEAVE=PIXEL')
            ds = gdal.OpenEx(utf8_path="WCS:" + url + "/?" + query,
                             open_options=options)
            if not ds:
                print("OpenEx failed: WCS:" + url + "/?" + query)
                wcs_6_ok = False
                break
            projwin = setup[server]['Projwin'].replace('-projwin ', '').split()
            for i, c in enumerate(projwin):
                projwin[i] = int(c)
            options = [cache]
            tmpfile = "tmp/" + server + version + ".tiff"
            gdal.Translate(tmpfile, ds, projWin=projwin, width=size, options=options)
            os.remove(tmpfile)

            if os.path.isfile('data/wcs/' + server + '-' + version + '-non_scaled.tiff'):
                options = [cache]
                options.append('GetCoverageExtra=test=non_scaled')
                options.append('INTERLEAVE=PIXEL')
                ds = gdal.OpenEx(utf8_path="WCS:" + url + "/?" + query,
                                 open_options=options)
                if not ds:
                    print("OpenEx failed: WCS:" + url + "/?" + query)
                    wcs_6_ok = False
                    break
                options = [cache]
                gdal.Translate(tmpfile, ds, srcWin=[0, 0, 2, 2], options=options)
                os.remove(tmpfile)
            else:
                print(server + ' ' + version + ' non_scaled skipped (no response file)')
    webserver.server_stop(process, port)
    if wcs_6_ok:
        return 'success'
    else:
        return 'fail'

###############################################################################


def wcs_cleanup():

    gdaltest.wcs_drv = None
    gdaltest.wcs_ds = None

    try:
        os.remove('tmp/geoserver.wcs')
    except OSError:
        pass

    try:
        shutil.rmtree('wcs_cache')
    except OSError:
        pass

    return 'success'


gdaltest_list = [
    wcs_1,
    # wcs_2, #FIXME: re-enable after adapting test
    wcs_3,
    wcs_4,
    # wcs_5, #FIXME: re-enable after adapting test
    wcs_6,
    wcs_cleanup]


if __name__ == '__main__':

    gdaltest.setup_run('wcs')

    gdaltest.run_tests(gdaltest_list)

    gdaltest.summarize()
