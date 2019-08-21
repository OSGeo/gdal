#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for ECRGTOC driver.
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
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
from osgeo import gdal


import gdaltest
import pytest


###############################################################################
# Basic test

def test_ecrgtoc_1():

    toc_xml = """<Table_of_Contents>
  <file_header file_status="new">
    <file_name>TOC.xml</file_name>
  </file_header>
  <product product_title="ProductTitle">
    <disc id="DiscId">
      <frame_list number_of_frames="2">
        <scale size="1:500 K">
          <frame name="000000009s0013.lf2">
            <frame_path>clfc\\2</frame_path>
            <frame_version>001</frame_version>
            <frame_chart_type>lf</frame_chart_type>
            <frame_zone>2</frame_zone>
          </frame>
          <frame name="000000009t0013.lf2">
            <frame_path>clfc\\2</frame_path>
            <frame_version>001</frame_version>
            <frame_chart_type>lf</frame_chart_type>
            <frame_zone>2</frame_zone>
          </frame>
        </scale>
      </frame_list>
    </disc>
  </product>
  <extension_list>
    <extension code="LF">
      <chart_code>LF</chart_code>
      <chart_type>1:500 K (LFC Day)</chart_type>
      <chart_scale>1:500 K</chart_scale>
      <chart_description>LFC Day</chart_description>
    </extension>
  </extension_list>
</Table_of_Contents>"""

    f = gdal.VSIFOpenL('/vsimem/TOC.xml', 'wb')
    gdal.VSIFWriteL(toc_xml, 1, len(toc_xml), f)
    gdal.VSIFCloseL(f)

    ds = gdal.Open('/vsimem/TOC.xml')
    assert ds is not None

    expected_gt = [-85.43147208121826, 0.00059486040609137061, 0.0, 33.166986564299428, 0.0, -0.00044985604606525913]
    gt = ds.GetGeoTransform()
    for i in range(6):
        if gt[i] != pytest.approx(expected_gt[i], abs=1e-10):
            gdaltest.post_reason('did not get expected geotransform')
            print(gt)

    wkt = ds.GetProjectionRef()
    assert wkt.find('WGS 84') != -1, 'did not get expected SRS'

    filelist = ds.GetFileList()
    assert len(filelist) == 3, 'did not get expected filelist'

    ds2 = gdal.GetDriverByName('NITF').Create('/vsimem/clfc/2/000000009s0013.lf2', 2304, 2304, 3,
                                              options=['ICORDS=G', 'TRE=GEOLOB=000605184000800256-85.43147208122+33.16698656430'])
    ds2.SetGeoTransform([-85.43147208122, 0.00059486040609137061, 0.0, 33.16698656430, 0.0, -0.00044985604606525913])
    ds2.SetProjection(wkt)
    ds2.GetRasterBand(1).Fill(255)
    ds2 = None

    ds2 = gdal.GetDriverByName('NITF').Create('/vsimem/clfc/2/000000009t0013.lf2', 2304, 2304, 3,
                                              options=['ICORDS=G', 'TRE=GEOLOB=000605184000800256-84.06091370558+33.16698656430'])
    ds2.SetGeoTransform([-84.06091370558, 0.00059486040609137061, 0.0, 33.16698656430, 0.0, -0.00044985604606525913])
    ds2.SetProjection(wkt)
    ds2 = None

    cs = ds.GetRasterBand(1).Checksum()

    ds = None

    assert cs == 5966, 'bad checksum'

###############################################################################
# Test overviews


def test_ecrgtoc_2():

    ds = gdal.Open('/vsimem/TOC.xml')
    ds.BuildOverviews('NEAR', [2])
    ds = None

    ds = gdal.Open('/vsimem/TOC.xml')

    filelist = ds.GetFileList()
    assert len(filelist) == 4, 'did not get expected filelist'

    ds = None

###############################################################################
# Test opening subdataset


def test_ecrgtoc_3():

    # Try different errors
    for name in ['ECRG_TOC_ENTRY:',
                 'ECRG_TOC_ENTRY:ProductTitle',
                 'ECRG_TOC_ENTRY:ProductTitle:DiscId',
                 'ECRG_TOC_ENTRY:ProductTitle:DiscId:not_existing',
                 'ECRG_TOC_ENTRY:ProductTitle:DiscId:c:/not_existing',
                 'ECRG_TOC_ENTRY:ProductTitle:DiscId:1_500_K:not_existing',
                 'ECRG_TOC_ENTRY:ProductTitle:DiscId:1_500_K:c:/not_existing',
                 'ECRG_TOC_ENTRY:ProductTitle:DiscId:1_500_K:c:/not_existing:extra',
                 'ECRG_TOC_ENTRY:ProductTitle:DiscId:inexisting_scale:/vsimem/TOC.xml',
                 'ECRG_TOC_ENTRY:ProductTitle:DiscId2:/vsimem/TOC.xml',
                 'ECRG_TOC_ENTRY:ProductTitle2:DiscId:/vsimem/TOC.xml']:

        gdal.PushErrorHandler()
        ds = gdal.Open(name)
        gdal.PopErrorHandler()
        assert ds is None, name

    # Legacy syntax
    ds = gdal.Open('ECRG_TOC_ENTRY:ProductTitle:DiscId:/vsimem/TOC.xml')
    assert ds is not None
    ds = None

    ds = gdal.Open('ECRG_TOC_ENTRY:ProductTitle:DiscId:1_500_K:/vsimem/TOC.xml')
    assert ds is not None
    ds = None

    gdal.Unlink('/vsimem/TOC.xml')
    gdal.Unlink('/vsimem/TOC.xml.1.ovr')
    gdal.Unlink('/vsimem/clfc/2/000000009s0013.lf2')
    gdal.Unlink('/vsimem/clfc/2/000000009t0013.lf2')

###############################################################################
# Test dataset with 3 subdatasets


def test_ecrgtoc_4():

    toc_xml = """<Table_of_Contents>
  <file_header file_status="new">
    <file_name>TOC.xml</file_name>
  </file_header>
  <product product_title="ProductTitle">
    <disc id="DiscId">
      <frame_list number_of_frames="2">
        <scale size="1:500 K">
          <frame name="000000009s0013.lf2">
            <frame_path>clfc\\2</frame_path>
            <frame_version>001</frame_version>
            <frame_chart_type>lf</frame_chart_type>
            <frame_zone>2</frame_zone>
          </frame>
        </scale>
        <scale size="1:1000 K">
          <frame name="000000009s0013.lf2">
            <frame_path>clfc\\2</frame_path>
            <frame_version>001</frame_version>
            <frame_chart_type>lf</frame_chart_type>
            <frame_zone>2</frame_zone>
          </frame>
        </scale>
      </frame_list>
    </disc>
    <disc id="DiscId2">
      <frame_list number_of_frames="1">
        <scale size="1:500 K">
          <frame name="000000009t0013.lf2">
            <frame_path>clfc\\2</frame_path>
            <frame_version>001</frame_version>
            <frame_chart_type>lf</frame_chart_type>
            <frame_zone>2</frame_zone>
          </frame>
        </scale>
      </frame_list>
    </disc>
  </product>
  <extension_list>
    <extension code="LF">
      <chart_code>LF</chart_code>
      <chart_type>1:500 K (LFC Day)</chart_type>
      <chart_scale>1:500 K</chart_scale>
      <chart_description>LFC Day</chart_description>
    </extension>
  </extension_list>
</Table_of_Contents>"""

    f = gdal.VSIFOpenL('/vsimem/TOC.xml', 'wb')
    gdal.VSIFWriteL(toc_xml, 1, len(toc_xml), f)
    gdal.VSIFCloseL(f)

    ds = gdal.Open('/vsimem/TOC.xml')
    assert ds is not None
    assert ds.RasterCount == 0, 'bad raster count'

    expected_gt = (-85.43147208121826, 0.00059486040609137061, 0.0, 37.241379310344833, 0.0, -0.00044985604606525913)
    gt = ds.GetGeoTransform()
    for i in range(6):
        assert gt[i] == pytest.approx(expected_gt[i], abs=1e-10), 'did not get expected geotransform'

    wkt = ds.GetProjectionRef()
    assert wkt.find('WGS 84') != -1, 'did not get expected SRS'

    filelist = ds.GetFileList()
    assert len(filelist) == 4, 'did not get expected filelist'

    subdatasets = ds.GetMetadata('SUBDATASETS')
    if len(subdatasets) != 6:
        print(filelist)
        pytest.fail('did not get expected subdatasets')

    ds = None

    ds = gdal.Open('ECRG_TOC_ENTRY:ProductTitle:DiscId:1_500_K:/vsimem/TOC.xml')
    assert ds is not None, 'did not get subdataset'
    ds = None

    ds = gdal.Open('ECRG_TOC_ENTRY:ProductTitle:DiscId:1_1000_K:/vsimem/TOC.xml')
    assert ds is not None, 'did not get subdataset'
    ds = None

    ds = gdal.Open('ECRG_TOC_ENTRY:ProductTitle:DiscId2:1_500_K:/vsimem/TOC.xml')
    assert ds is not None, 'did not get subdataset'
    ds = None

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('ECRG_TOC_ENTRY:ProductTitle:DiscId:/vsimem/TOC.xml')
    gdal.PopErrorHandler()
    assert ds is None, 'should not have got subdataset'

    gdal.Unlink('/vsimem/TOC.xml')

###############################################################################


def test_ecrgtoc_online_1():

    if not gdaltest.download_file('http://www.falconview.org/trac/FalconView/downloads/17', 'ECRG_Sample.zip'):
        pytest.skip()

    try:
        os.stat('tmp/cache/ECRG_Sample.zip')
    except OSError:
        pytest.skip()

    ds = gdal.Open('/vsizip/tmp/cache/ECRG_Sample.zip/ECRG_Sample/EPF/TOC.xml')
    assert ds is not None

    expected_gt = (-85.43147208121826, 0.00059486040609137061, 0.0, 35.239923224568145, 0.0, -0.00044985604606525913)
    gt = ds.GetGeoTransform()
    for i in range(6):
        if gt[i] != pytest.approx(expected_gt[i], abs=1e-10):
            gdaltest.post_reason('did not get expected geotransform')
            print(gt)

    wkt = ds.GetProjectionRef()
    assert wkt.find('WGS 84') != -1, 'did not get expected SRS'

    filelist = ds.GetFileList()
    assert len(filelist) == 7, 'did not get expected filelist'



