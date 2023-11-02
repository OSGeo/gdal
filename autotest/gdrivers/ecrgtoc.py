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

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("ECRGTOC")

###############################################################################
# Basic test


@pytest.fixture()
def toc_ds(tmp_vsimem):

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

    f = gdal.VSIFOpenL(tmp_vsimem / "TOC.xml", "wb")
    gdal.VSIFWriteL(toc_xml, 1, len(toc_xml), f)
    gdal.VSIFCloseL(f)

    ds = gdal.Open(tmp_vsimem / "TOC.xml")

    assert ds is not None

    ds2 = gdal.GetDriverByName("NITF").Create(
        tmp_vsimem / "clfc/2/000000009s0013.lf2",
        2304,
        2304,
        3,
        options=[
            "ICORDS=G",
            "TRE=GEOLOB=000605184000800256-85.43147208122+33.16698656430",
        ],
    )
    ds2.SetGeoTransform(
        [
            -85.43147208122,
            0.00059486040609137061,
            0.0,
            33.16698656430,
            0.0,
            -0.00044985604606525913,
        ]
    )
    ds2.SetProjection(ds.GetProjectionRef())
    ds2.GetRasterBand(1).Fill(255)
    ds2 = None

    ds2 = gdal.GetDriverByName("NITF").Create(
        tmp_vsimem / "clfc/2/000000009t0013.lf2",
        2304,
        2304,
        3,
        options=[
            "ICORDS=G",
            "BLOCKXSIZE=128",
            "BLOCKYSIZE=256",
            "TRE=GEOLOB=000605184000800256-84.06091370558+33.16698656430",
        ],
    )
    ds2.SetGeoTransform(
        [
            -84.06091370558,
            0.00059486040609137061,
            0.0,
            33.16698656430,
            0.0,
            -0.00044985604606525913,
        ]
    )

    ds2.SetProjection(ds.GetProjectionRef())
    ds2 = None

    return ds


def test_ecrgtoc_1(toc_ds):

    ds = toc_ds

    expected_gt = [
        -85.43147208121826,
        0.00059486040609137061,
        0.0,
        33.166986564299428,
        0.0,
        -0.00044985604606525913,
    ]
    gt = ds.GetGeoTransform()
    gdaltest.check_geotransform(gt, expected_gt, 1e-10)

    wkt = ds.GetProjectionRef()
    assert wkt.find("WGS 84") != -1, "did not get expected SRS"

    filelist = ds.GetFileList()
    assert len(filelist) == 3, "did not get expected filelist"

    cs = ds.GetRasterBand(1).Checksum()

    ds = None

    assert cs == 5966, "bad checksum"


###############################################################################
# Test in GDAL_FORCE_CACHING=YES mode


def test_ecrgtoc_force_caching(toc_ds):

    with gdaltest.config_option("GDAL_FORCE_CACHING", "YES"):
        ds = gdal.Open(toc_ds.GetDescription())

    cs = ds.GetRasterBand(1).Checksum()

    assert cs == 5966, "bad checksum"


###############################################################################
# Test overviews


def test_ecrgtoc_2(toc_ds):

    toc_ds.BuildOverviews("NEAR", [2])

    toc_ds = gdaltest.reopen(toc_ds)

    filelist = toc_ds.GetFileList()
    assert len(filelist) == 4, "did not get expected filelist"


###############################################################################
# Test opening subdataset


def test_ecrgtoc_3(toc_ds):

    # Try different errors
    for name in [
        "ECRG_TOC_ENTRY:",
        "ECRG_TOC_ENTRY:ProductTitle",
        "ECRG_TOC_ENTRY:ProductTitle:DiscId",
        "ECRG_TOC_ENTRY:ProductTitle:DiscId:not_existing",
        "ECRG_TOC_ENTRY:ProductTitle:DiscId:c:/not_existing",
        "ECRG_TOC_ENTRY:ProductTitle:DiscId:1_500_K:not_existing",
        "ECRG_TOC_ENTRY:ProductTitle:DiscId:1_500_K:c:/not_existing",
        "ECRG_TOC_ENTRY:ProductTitle:DiscId:1_500_K:c:/not_existing:extra",
        f"ECRG_TOC_ENTRY:ProductTitle:DiscId:inexisting_scale:{toc_ds.GetDescription()}",
        f"ECRG_TOC_ENTRY:ProductTitle:DiscId2:{toc_ds.GetDescription()}",
        f"ECRG_TOC_ENTRY:ProductTitle2:DiscId:{toc_ds.GetDescription()}",
    ]:

        with pytest.raises(Exception):
            gdal.Open(name)

    # Legacy syntax
    ds = gdal.Open(f"ECRG_TOC_ENTRY:ProductTitle:DiscId:{toc_ds.GetDescription()}")
    assert ds is not None
    ds = None

    ds = gdal.Open(
        f"ECRG_TOC_ENTRY:ProductTitle:DiscId:1_500_K:{toc_ds.GetDescription()}"
    )
    assert ds is not None
    ds = None


###############################################################################
# Test dataset with 3 subdatasets


def test_ecrgtoc_4(tmp_vsimem):

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

    f = gdal.VSIFOpenL(tmp_vsimem / "TOC.xml", "wb")
    gdal.VSIFWriteL(toc_xml, 1, len(toc_xml), f)
    gdal.VSIFCloseL(f)

    ds = gdal.Open(tmp_vsimem / "TOC.xml")
    assert ds is not None
    assert ds.RasterCount == 0, "bad raster count"

    expected_gt = (
        -85.43147208121826,
        0.00059486040609137061,
        0.0,
        37.241379310344833,
        0.0,
        -0.00044985604606525913,
    )
    gt = ds.GetGeoTransform()
    gdaltest.check_geotransform(gt, expected_gt, 1e-10)

    wkt = ds.GetProjectionRef()
    assert wkt.find("WGS 84") != -1, "did not get expected SRS"

    filelist = ds.GetFileList()
    assert len(filelist) == 4, "did not get expected filelist"

    subdatasets = ds.GetMetadata("SUBDATASETS")
    if len(subdatasets) != 6:
        print(filelist)
        pytest.fail("did not get expected subdatasets")

    ds = None

    ds = gdal.Open(f"ECRG_TOC_ENTRY:ProductTitle:DiscId:1_500_K:{tmp_vsimem}/TOC.xml")
    assert ds is not None, "did not get subdataset"
    ds = None

    ds = gdal.Open(f"ECRG_TOC_ENTRY:ProductTitle:DiscId:1_1000_K:{tmp_vsimem}/TOC.xml")
    assert ds is not None, "did not get subdataset"
    ds = None

    ds = gdal.Open(f"ECRG_TOC_ENTRY:ProductTitle:DiscId2:1_500_K:{tmp_vsimem}/TOC.xml")
    assert ds is not None, "did not get subdataset"
    ds = None

    with pytest.raises(Exception):
        gdal.Open("ECRG_TOC_ENTRY:ProductTitle:DiscId:{tmp_vsimem}/TOC.xml")


###############################################################################


@pytest.mark.skipif(
    not os.path.exists("tmp/cache/ECRG_Sample.zip"),
    reason="Test data no longer available",
)
def test_ecrgtoc_online_1():

    gdaltest.download_or_skip(
        "http://www.falconview.org/trac/FalconView/downloads/17", "ECRG_Sample.zip"
    )

    try:
        os.stat("tmp/cache/ECRG_Sample.zip")
    except OSError:
        pytest.skip()

    ds = gdal.Open("/vsizip/tmp/cache/ECRG_Sample.zip/ECRG_Sample/EPF/TOC.xml")
    assert ds is not None

    expected_gt = (
        -85.43147208121826,
        0.00059486040609137061,
        0.0,
        35.239923224568145,
        0.0,
        -0.00044985604606525913,
    )
    gt = ds.GetGeoTransform()
    gdaltest.check_geotransform(gt, expected_gt, 1e-10)

    wkt = ds.GetProjectionRef()
    assert wkt.find("WGS 84") != -1, "did not get expected SRS"

    filelist = ds.GetFileList()
    assert len(filelist) == 7, "did not get expected filelist"
