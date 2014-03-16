#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for ECRGTOC driver.
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest


###############################################################################
# Basic test

def ecrgtoc_1():

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
    if ds is None:
        return 'fail'

    expected_gt = [-85.43147208121826, 0.00059486040609137061, 0.0, 33.166986564299428, 0.0, -0.00044985604606525913]
    gt = ds.GetGeoTransform()
    for i in range(6):
        if abs(gt[i] - expected_gt[i]) > 1e-10:
            gdaltest.post_reason('did not get expected geotransform')
            print(gt)

    wkt = ds.GetProjectionRef()
    if wkt.find('WGS 84') == -1:
        gdaltest.post_reason('did not get expected SRS')
        print(wkt)
        return 'fail'

    filelist = ds.GetFileList()
    if len(filelist) != 3:
        gdaltest.post_reason('did not get expected filelist')
        print(filelist)
        return 'fail'

    ds2 = gdal.GetDriverByName('NITF').Create('/vsimem/clfc/2/000000009s0013.lf2', 2304, 2304, 3, \
        options = ['ICORDS=G', 'TRE=GEOLOB=000605184000800256-85.43147208122+33.16698656430'])
    ds2.SetGeoTransform([-85.43147208122, 0.00059486040609137061, 0.0, 33.16698656430, 0.0, -0.00044985604606525913])
    ds2.SetProjection(wkt)
    ds2.GetRasterBand(1).Fill(255)
    ds2 = None

    ds2 = gdal.GetDriverByName('NITF').Create('/vsimem/clfc/2/000000009t0013.lf2', 2304, 2304, 3, \
        options = ['ICORDS=G', 'TRE=GEOLOB=000605184000800256-84.06091370558+33.16698656430'])
    ds2.SetGeoTransform([-84.06091370558, 0.00059486040609137061, 0.0, 33.16698656430, 0.0, -0.00044985604606525913])
    ds2.SetProjection(wkt)
    ds2 = None

    cs = ds.GetRasterBand(1).Checksum()

    ds = None

    if cs != 5966:
        gdaltest.post_reason('bad checksum')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Test overviews

def ecrgtoc_2():

    ds = gdal.Open('/vsimem/TOC.xml')
    ds.BuildOverviews('NEAR', [2])
    ds = None

    ds = gdal.Open('/vsimem/TOC.xml')

    filelist = ds.GetFileList()
    if len(filelist) != 4:
        gdaltest.post_reason('did not get expected filelist')
        print(filelist)
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test opening subdataset

def ecrgtoc_3():

    ds = gdal.Open('ECRG_TOC_ENTRY:ProductTitle:DiscId:/vsimem/TOC.xml')
    if ds is None:
        return 'fail'
    ds = None

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('ECRG_TOC_ENTRY:ProductTitle:DiscId2:/vsimem/TOC.xml')
    gdal.PopErrorHandler()
    if ds is not None:
        return 'fail'

    gdal.Unlink('/vsimem/TOC.xml')
    gdal.Unlink('/vsimem/TOC.xml.1.ovr')
    gdal.Unlink('/vsimem/clfc/2/000000009s0013.lf2')
    gdal.Unlink('/vsimem/clfc/2/000000009t0013.lf2')

    return 'success'

###############################################################################
# Test dataset with 2 subdatasets

def ecrgtoc_4():

    toc_xml = """<Table_of_Contents>
  <file_header file_status="new">
    <file_name>TOC.xml</file_name>
  </file_header>
  <product product_title="ProductTitle">
    <disc id="DiscId">
      <frame_list number_of_frames="1">
        <scale size="1:500 K">
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
    if ds is None:
        return 'fail'
    if ds.RasterCount != 0:
        gdaltest.post_reason('bad raster count')
        return 'fail'

    expected_gt = (-85.43147208121826, 0.00059486040609137061, 0.0, 33.166986564299428, 0.0, -0.00044985604606525913)
    gt = ds.GetGeoTransform()
    for i in range(6):
        if abs(gt[i] - expected_gt[i]) > 1e-10:
            gdaltest.post_reason('did not get expected geotransform')
            print(gt)

    wkt = ds.GetProjectionRef()
    if wkt.find('WGS 84') == -1:
        gdaltest.post_reason('did not get expected SRS')
        print(wkt)
        return 'fail'

    filelist = ds.GetFileList()
    if len(filelist) != 3:
        gdaltest.post_reason('did not get expected filelist')
        print(filelist)
        return 'fail'

    subdatasets = ds.GetMetadata('SUBDATASETS')
    if len(subdatasets) != 4:
        gdaltest.post_reason('did not get expected subdatasets')
        print(filelist)
        return 'fail'

    ds = None

    gdal.Unlink('/vsimem/TOC.xml')

    return 'success'

###############################################################################
def ecrgtoc_online_1():

    if not gdaltest.download_file('http://www.falconview.org/trac/FalconView/downloads/17', 'ECRG_Sample.zip'):
        return 'skip'

    try:
        os.stat('tmp/cache/ECRG_Sample.zip')
    except:
        return 'skip'

    ds = gdal.Open('/vsizip/tmp/cache/ECRG_Sample.zip/ECRG_Sample/EPF/TOC.xml')
    if ds is None:
        return 'fail'

    expected_gt = (-85.43147208121826, 0.00059486040609137061, 0.0, 35.239923224568145, 0.0, -0.00044985604606525913)
    gt = ds.GetGeoTransform()
    for i in range(6):
        if abs(gt[i] - expected_gt[i]) > 1e-10:
            gdaltest.post_reason('did not get expected geotransform')
            print(gt)

    wkt = ds.GetProjectionRef()
    if wkt.find('WGS 84') == -1:
        gdaltest.post_reason('did not get expected SRS')
        print(wkt)
        return 'fail'

    filelist = ds.GetFileList()
    if len(filelist) != 7:
        gdaltest.post_reason('did not get expected filelist')
        print(filelist)
        return 'fail'

    return 'success'

gdaltest_list = [
    ecrgtoc_1,
    ecrgtoc_2,
    ecrgtoc_3,
    ecrgtoc_4,
    ecrgtoc_online_1 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ecrgtoc' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

