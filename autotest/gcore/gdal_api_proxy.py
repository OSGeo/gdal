#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GDAL API PROXY mechanism
# Author:    Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
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

import sys
import subprocess
import time
from osgeo import gdal
from osgeo import osr

import pytest

###############################################################################
# Test forked gdalserver


@pytest.fixture
def gdalserver_path():
    import test_cli_utilities
    gdalserver_path = test_cli_utilities.get_cli_utility_path('gdalserver')
    if gdalserver_path is None:
        gdalserver_path = 'gdalserver'
    return gdalserver_path


def test_gdal_api_proxy_1(gdalserver_path):
    subprocess.check_call([
        sys.executable, 'gdal_api_proxy.py', gdalserver_path, '-2',
    ])

###############################################################################
# Test connection to TCP server


def test_gdal_api_proxy_2(gdalserver_path):

    if sys.version_info < (2, 6, 0):
        pytest.skip()

    subprocess.check_call([
        sys.executable, 'gdal_api_proxy.py', gdalserver_path, '-2',
    ])

###############################################################################
# Test connection to Unix socket server


def test_gdal_api_proxy_3(gdalserver_path):

    if sys.version_info < (2, 6, 0):
        pytest.skip()

    if sys.platform == 'win32':
        pytest.skip()

    if sys.platform == 'darwin':
        pytest.skip("Fails on MacOSX ('ERROR 1: posix_spawnp() failed'. Not sure why.")

    subprocess.check_call([
        sys.executable, 'gdal_api_proxy.py', gdalserver_path, '-3',
    ])

###############################################################################
# Test -nofork mode


def test_gdal_api_proxy_4(gdalserver_path):

    if sys.version_info < (2, 6, 0):
        pytest.skip()

    if sys.platform == 'win32':
        pytest.skip()

    if sys.platform == 'darwin':
        pytest.skip("Fails on MacOSX ('ERROR 1: posix_spawnp() failed'. Not sure why.")

    subprocess.check_call([
        sys.executable, 'gdal_api_proxy.py', gdalserver_path, '-4',
    ])

###############################################################################
#


def _gdal_api_proxy_sub():

    src_ds = gdal.Open('data/byte.tif')
    src_cs = src_ds.GetRasterBand(1).Checksum()
    src_gt = src_ds.GetGeoTransform()
    src_prj = src_ds.GetProjectionRef()
    src_data = src_ds.ReadRaster(0, 0, 20, 20)
    src_md = src_ds.GetMetadata()
    src_ds = None

    drv = gdal.IdentifyDriver('data/byte.tif')
    assert drv.GetDescription() == 'API_PROXY'

    ds = gdal.GetDriverByName('GTiff').Create('tmp/byte.tif', 1, 1, 3)
    ds = None

    src_ds = gdal.Open('data/byte.tif')
    ds = gdal.GetDriverByName('GTiff').CreateCopy('tmp/byte.tif', src_ds, options=['TILED=YES'])
    got_cs = ds.GetRasterBand(1).Checksum()
    assert src_cs == got_cs
    ds = None

    ds = gdal.Open('tmp/byte.tif', gdal.GA_Update)

    ds.SetGeoTransform([1, 2, 3, 4, 5, 6])
    got_gt = ds.GetGeoTransform()
    assert src_gt != got_gt

    ds.SetGeoTransform(src_gt)
    got_gt = ds.GetGeoTransform()
    assert src_gt == got_gt

    assert ds.GetGCPCount() == 0

    assert ds.GetGCPProjection() == '', ds.GetGCPProjection()

    assert not ds.GetGCPs()

    gcps = [gdal.GCP(0, 1, 2, 3, 4)]
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    wkt = sr.ExportToWkt()
    assert ds.SetGCPs(gcps, wkt) == 0

    got_gcps = ds.GetGCPs()
    assert len(got_gcps) == 1

    assert (got_gcps[0].GCPLine == gcps[0].GCPLine and  \
       got_gcps[0].GCPPixel == gcps[0].GCPPixel and  \
       got_gcps[0].GCPX == gcps[0].GCPX and \
       got_gcps[0].GCPY == gcps[0].GCPY)

    assert ds.GetGCPProjection() == wkt

    ds.SetGCPs([], "")

    assert not ds.GetGCPs()

    ds.SetProjection('')
    got_prj = ds.GetProjectionRef()
    assert src_prj != got_prj

    ds.SetProjection(src_prj)
    got_prj = ds.GetProjectionRef()
    assert src_prj == got_prj

    ds.GetRasterBand(1).Fill(0)
    got_cs = ds.GetRasterBand(1).Checksum()
    assert got_cs == 0

    ds.GetRasterBand(1).WriteRaster(0, 0, 20, 20, src_data)
    got_cs = ds.GetRasterBand(1).Checksum()
    assert src_cs == got_cs

    ds.GetRasterBand(1).Fill(0)
    got_cs = ds.GetRasterBand(1).Checksum()
    assert got_cs == 0

    ds.WriteRaster(0, 0, 20, 20, src_data)
    got_cs = ds.GetRasterBand(1).Checksum()
    assert src_cs == got_cs

    # Not bound to SWIG
    # ds.AdviseRead(0,0,20,20,20,20)

    got_data = ds.ReadRaster(0, 0, 20, 20)
    assert src_data == got_data

    got_data = ds.GetRasterBand(1).ReadRaster(0, 0, 20, 20)
    assert src_data == got_data

    got_data_weird_spacing = ds.ReadRaster(0, 0, 20, 20, buf_pixel_space=1, buf_line_space=32)
    assert len(got_data_weird_spacing) == 32 * (20 - 1) + 20

    assert got_data[20:20 + 20] == got_data_weird_spacing[32:32 + 20]

    got_data_weird_spacing = ds.GetRasterBand(1).ReadRaster(0, 0, 20, 20, buf_pixel_space=1, buf_line_space=32)
    assert len(got_data_weird_spacing) == 32 * (20 - 1) + 20

    assert got_data[20:20 + 20] == got_data_weird_spacing[32:32 + 20]

    got_block = ds.GetRasterBand(1).ReadBlock(0, 0)
    assert len(got_block) == 256 * 256

    assert got_data[20:20 + 20] == got_block[256:256 + 20]

    ds.FlushCache()
    ds.GetRasterBand(1).FlushCache()

    got_data = ds.GetRasterBand(1).ReadRaster(0, 0, 20, 20)
    assert src_data == got_data

    assert len(ds.GetFileList()) == 1

    assert ds.AddBand(gdal.GDT_Byte) != 0

    got_md = ds.GetMetadata()
    assert src_md == got_md

    assert ds.GetMetadataItem('AREA_OR_POINT') == 'Area'

    assert ds.GetMetadataItem('foo') is None

    ds.SetMetadataItem('foo', 'bar')
    assert ds.GetMetadataItem('foo') == 'bar'

    ds.SetMetadata({'foo': 'baz'}, 'OTHER')
    assert ds.GetMetadataItem('foo', 'OTHER') == 'baz'

    ds.GetRasterBand(1).SetMetadata({'foo': 'baw'}, 'OTHER')
    assert ds.GetRasterBand(1).GetMetadataItem('foo', 'OTHER') == 'baw'

    assert ds.GetMetadataItem('INTERLEAVE', 'IMAGE_STRUCTURE') == 'BAND'

    assert not ds.GetRasterBand(1).GetMetadata()

    assert ds.GetRasterBand(1).GetMetadataItem('foo') is None

    ds.GetRasterBand(1).SetMetadataItem('foo', 'baz')
    assert ds.GetRasterBand(1).GetMetadataItem('foo') == 'baz'

    ds.GetRasterBand(1).SetMetadata({'foo': 'baw'})
    assert ds.GetRasterBand(1).GetMetadataItem('foo') == 'baw'

    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_GrayIndex

    ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_Undefined)

    ct = ds.GetRasterBand(1).GetColorTable()
    assert ct is None

    ct = gdal.ColorTable()
    ct.SetColorEntry(0, (1, 2, 3))
    assert ds.GetRasterBand(1).SetColorTable(ct) == 0

    ct = ds.GetRasterBand(1).GetColorTable()
    assert ct is not None
    assert ct.GetColorEntry(0) == (1, 2, 3, 255)

    ct = ds.GetRasterBand(1).GetColorTable()
    assert ct is not None

    assert ds.GetRasterBand(1).SetColorTable(None) == 0

    ct = ds.GetRasterBand(1).GetColorTable()
    assert ct is None

    rat = ds.GetRasterBand(1).GetDefaultRAT()
    assert rat is None

    assert ds.GetRasterBand(1).SetDefaultRAT(None) == 0

    ref_rat = gdal.RasterAttributeTable()
    assert ds.GetRasterBand(1).SetDefaultRAT(ref_rat) == 0

    rat = ds.GetRasterBand(1).GetDefaultRAT()
    assert rat is None

    assert ds.GetRasterBand(1).SetDefaultRAT(None) == 0

    rat = ds.GetRasterBand(1).GetDefaultRAT()
    assert rat is None

    assert ds.GetRasterBand(1).GetMinimum() is None

    got_stats = ds.GetRasterBand(1).GetStatistics(0, 0)
    assert got_stats[3] < 0.0

    got_stats = ds.GetRasterBand(1).GetStatistics(1, 1)
    assert got_stats[0] == 74.0

    assert ds.GetRasterBand(1).GetMinimum() == 74.0

    assert ds.GetRasterBand(1).GetMaximum() == 255.0

    ds.GetRasterBand(1).SetStatistics(1, 2, 3, 4)
    got_stats = ds.GetRasterBand(1).GetStatistics(1, 1)
    assert got_stats == [1, 2, 3, 4]

    ds.GetRasterBand(1).ComputeStatistics(0)
    got_stats = ds.GetRasterBand(1).GetStatistics(1, 1)
    assert got_stats[0] == 74.0

    minmax = ds.GetRasterBand(1).ComputeRasterMinMax()
    assert minmax == (74.0, 255.0)

    assert ds.GetRasterBand(1).GetOffset() is None

    assert ds.GetRasterBand(1).GetScale() is None

    ds.GetRasterBand(1).SetOffset(10.0)
    assert ds.GetRasterBand(1).GetOffset() == 10.0

    ds.GetRasterBand(1).SetScale(2.0)
    assert ds.GetRasterBand(1).GetScale() == 2.0

    ds.BuildOverviews('NEAR', [2])
    assert ds.GetRasterBand(1).GetOverviewCount() == 1

    assert ds.GetRasterBand(1).GetOverview(-1) is None

    assert ds.GetRasterBand(1).GetOverview(0) is not None

    assert ds.GetRasterBand(1).GetOverview(0) is not None

    got_hist = ds.GetRasterBand(1).GetHistogram()
    assert len(got_hist) == 256

    (minval, maxval, nitems, got_hist2) = ds.GetRasterBand(1).GetDefaultHistogram()
    assert minval == -0.5
    assert maxval == 255.5
    assert nitems == 256
    assert got_hist == got_hist2

    ds.GetRasterBand(1).SetDefaultHistogram(1, 2, [3])
    (minval, maxval, nitems, got_hist3) = ds.GetRasterBand(1).GetDefaultHistogram()
    assert minval == 1
    assert maxval == 2
    assert nitems == 1
    assert got_hist3[0] == 3

    got_nodatavalue = ds.GetRasterBand(1).GetNoDataValue()
    assert got_nodatavalue is None

    ds.GetRasterBand(1).SetNoDataValue(123)
    got_nodatavalue = ds.GetRasterBand(1).GetNoDataValue()
    assert got_nodatavalue == 123

    assert ds.GetRasterBand(1).GetMaskFlags() == 8

    assert ds.GetRasterBand(1).GetMaskBand() is not None

    ret = ds.GetRasterBand(1).DeleteNoDataValue()
    assert ret == 0
    got_nodatavalue = ds.GetRasterBand(1).GetNoDataValue()
    assert got_nodatavalue is None

    ds.CreateMaskBand(0)

    assert ds.GetRasterBand(1).GetMaskFlags() == 2

    assert ds.GetRasterBand(1).GetMaskBand() is not None

    ds.GetRasterBand(1).CreateMaskBand(0)

    assert ds.GetRasterBand(1).HasArbitraryOverviews() == 0

    ds.GetRasterBand(1).SetUnitType('foo')
    assert ds.GetRasterBand(1).GetUnitType() == 'foo'

    assert ds.GetRasterBand(1).GetCategoryNames() is None

    ds.GetRasterBand(1).SetCategoryNames(['foo'])
    assert ds.GetRasterBand(1).GetCategoryNames() == ['foo']

    ds.GetRasterBand(1).SetDescription('bar')

    ds = None

    gdal.GetDriverByName('GTiff').Delete('tmp/byte.tif')

###############################################################################
#


def _gdal_api_proxy_sub_clean():
    if gdaltest.api_proxy_server_p is not None:
        try:
            gdaltest.api_proxy_server_p.terminate()
        except Exception:
            pass
        gdaltest.api_proxy_server_p.wait()
    gdal.Unlink('tmp/gdalapiproxysocket')


if __name__ == '__main__':
    sys.path.insert(0, '../pymod')
    import gdaltest

    if len(sys.argv) >= 3 and sys.argv[2] == '-1':

        gdal.SetConfigOption('GDAL_API_PROXY', 'YES')
        if sys.platform == 'win32':
            gdalserver_path = sys.argv[1]  # noqa
            gdal.SetConfigOption('GDAL_API_PROXY_SERVER', gdalserver_path)

        gdaltest.api_proxy_server_p = None
        gdaltest_list = [_gdal_api_proxy_sub]

    elif len(sys.argv) >= 3 and sys.argv[2] == '-2':

        gdalserver_path = sys.argv[1]

        p = None
        for port in [8080, 8081, 8082]:
            p = subprocess.Popen([gdalserver_path, '-tcpserver', '%d' % port])
            time.sleep(1)
            if p.poll() is None:
                break
            try:
                p.terminate()
            except (AttributeError, OSError):
                pass
            p.wait()
            p = None

        if p is not None:
            gdal.SetConfigOption('GDAL_API_PROXY', 'YES')
            gdal.SetConfigOption('GDAL_API_PROXY_SERVER', 'localhost:%d' % port)
            print('port = %d' % port)
            gdaltest.api_proxy_server_p = p
            gdaltest_list = [_gdal_api_proxy_sub, _gdal_api_proxy_sub_clean]
        else:
            gdaltest_list = []

    elif len(sys.argv) >= 3 and sys.argv[2] == '-3':

        gdalserver_path = sys.argv[1]

        p = subprocess.Popen([gdalserver_path, '-unixserver', 'tmp/gdalapiproxysocket'])
        time.sleep(1)
        if p.poll() is None:
            gdal.SetConfigOption('GDAL_API_PROXY', 'YES')
            gdal.SetConfigOption('GDAL_API_PROXY_SERVER', 'tmp/gdalapiproxysocket')
            gdaltest.api_proxy_server_p = p
            gdaltest_list = [_gdal_api_proxy_sub, _gdal_api_proxy_sub_clean]
        else:
            try:
                p.terminate()
            except (AttributeError, OSError):
                pass
            p.wait()
            gdaltest_list = []

    elif len(sys.argv) >= 3 and sys.argv[2] == '-4':

        gdalserver_path = sys.argv[1]

        p = subprocess.Popen([gdalserver_path, '-nofork', '-unixserver', 'tmp/gdalapiproxysocket'])
        time.sleep(1)
        if p.poll() is None:
            gdal.SetConfigOption('GDAL_API_PROXY', 'YES')
            gdal.SetConfigOption('GDAL_API_PROXY_SERVER', 'tmp/gdalapiproxysocket')
            gdaltest.api_proxy_server_p = p
            gdaltest_list = [_gdal_api_proxy_sub, _gdal_api_proxy_sub_clean]
        else:
            try:
                p.terminate()
            except (AttributeError, OSError):
                pass
            p.wait()
            gdaltest_list = []

    for func in gdaltest_list:
        func()
