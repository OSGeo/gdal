#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  SNAP_TIFF driver testing.
# Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even.rouault at spatialys.com>
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

import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("SNAP_TIFF")


def test_snap_tiff():
    ds = gdal.Open(
        "/vsizip/vsizip/data/snap_tiff/S1A_IW_GRDH_1SDV_20171009T141532_20171009T141557_018737_01F9E2_E974_tnr_empty_truncated.tif.zip.zip"
    )
    assert ds.GetDriver().GetDescription() == "SNAP_TIFF"
    assert ds.RasterXSize == 25548
    assert ds.RasterYSize == 16716
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Float32
    assert ds.GetGCPCount() == 4
    assert ds.GetGCPSpatialRef().GetAuthorityCode(None) == "4326"
    gcps = ds.GetGCPs()
    assert len(gcps) == 4
    assert gcps[0].GCPPixel == 0.5
    assert gcps[0].GCPLine == 0.5
    assert gcps[0].GCPX == -121.18662152623274
    assert gcps[0].GCPY == 39.655540466308594
    assert gcps[3].GCPPixel == 25547.5
    assert gcps[3].GCPLine == 16715.5
    assert gcps[3].GCPX == -124.43485147116212
    assert gcps[3].GCPY == 38.550738598352105
    assert ds.GetRasterBand(1).GetNoDataValue() == 0
    assert ds.GetRasterBand(1).GetDescription() == "Intensity_VV"
    assert ds.GetRasterBand(1).GetUnitType() == "intensity"
    assert ds.GetRasterBand(1).GetScale() == 1
    assert ds.GetRasterBand(1).GetOffset() == 0
    assert ds.GetMetadataDomainList() == [
        "",
        "DERIVED_SUBDATASETS",
        "GEOLOCATION",
        "SUBDATASETS",
        "xml:DIMAP",
    ]
    assert ds.GetMetadata() == {
        "IMAGE_DESCRIPTION": "S1A_IW_GRDH_1SDV_20171009T141532_20171009T141557_018737_01F9E2_E974_tnr"
    }
    assert (
        ds.GetMetadataItem("IMAGE_DESCRIPTION")
        == "S1A_IW_GRDH_1SDV_20171009T141532_20171009T141557_018737_01F9E2_E974_tnr"
    )
    assert ds.GetMetadata("GEOLOCATION") == {
        "LINE_OFFSET": "0",
        "LINE_STEP": "16.025886864813039",
        "PIXEL_OFFSET": "0",
        "PIXEL_STEP": "16.02697616060226",
        "SRS": 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS '
        '84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]',
        "X_BAND": "1",
        "X_DATASET": 'SNAP_TIFF:"/vsizip/vsizip/data/snap_tiff/S1A_IW_GRDH_1SDV_20171009T141532_20171009T141557_018737_01F9E2_E974_tnr_empty_truncated.tif.zip.zip":GEOLOCATION',
        "Y_BAND": "2",
        "Y_DATASET": 'SNAP_TIFF:"/vsizip/vsizip/data/snap_tiff/S1A_IW_GRDH_1SDV_20171009T141532_20171009T141557_018737_01F9E2_E974_tnr_empty_truncated.tif.zip.zip":GEOLOCATION',
    }
    assert ds.GetMetadataItem("LINE_OFFSET", "GEOLOCATION") == "0"
    assert ds.GetMetadata("SUBDATASETS") == {
        "SUBDATASET_1_DESC": "Main content of "
        "/vsizip/vsizip/data/snap_tiff/S1A_IW_GRDH_1SDV_20171009T141532_20171009T141557_018737_01F9E2_E974_tnr_empty_truncated.tif.zip.zip",
        "SUBDATASET_1_NAME": 'SNAP_TIFF:"/vsizip/vsizip/data/snap_tiff/S1A_IW_GRDH_1SDV_20171009T141532_20171009T141557_018737_01F9E2_E974_tnr_empty_truncated.tif.zip.zip":MAIN',
        "SUBDATASET_2_DESC": "Geolocation array of "
        "/vsizip/vsizip/data/snap_tiff/S1A_IW_GRDH_1SDV_20171009T141532_20171009T141557_018737_01F9E2_E974_tnr_empty_truncated.tif.zip.zip",
        "SUBDATASET_2_NAME": 'SNAP_TIFF:"/vsizip/vsizip/data/snap_tiff/S1A_IW_GRDH_1SDV_20171009T141532_20171009T141557_018737_01F9E2_E974_tnr_empty_truncated.tif.zip.zip":GEOLOCATION',
    }
    assert ds.GetMetadata("xml:DIMAP")[0].startswith("<?xml")

    subds = gdal.Open(ds.GetMetadataItem("SUBDATASET_1_NAME", "SUBDATASETS"))
    assert subds.RasterXSize == ds.RasterXSize

    subds = gdal.Open(ds.GetMetadataItem("SUBDATASET_2_NAME", "SUBDATASETS"))
    assert subds.RasterXSize == 1595
    assert subds.RasterYSize == 1044
    assert subds.RasterCount == 2
    assert subds.GetRasterBand(1).DataType == gdal.GDT_Float64
    assert subds.GetMetadata("SUBDATASETS") == {}
    subds.GetRasterBand(1).SetNoDataValue(0)
    assert subds.GetRasterBand(1).ComputeRasterMinMax() == (
        -124.43485147116212,
        -121.18662152623274,
    )
    subds.GetRasterBand(2).SetNoDataValue(0)
    assert subds.GetRasterBand(2).ComputeRasterMinMax() == (
        38.15253672014443,
        40.05228536834884,
    )
    with gdal.quiet_errors():
        subds.Close()

    with pytest.raises(Exception):
        gdal.Open("SNAP_TIFF:")

    with pytest.raises(Exception):
        gdal.Open("SNAP_TIFF:data/byte.tif:MAIN")

    with pytest.raises(Exception):
        gdal.Open("SNAP_TIFF:data/non_existing.tif:MAIN")

    with pytest.raises(Exception):
        gdal.Open(
            "SNAP_TIFF:/vsizip/vsizip/data/snap_tiff/S1A_IW_GRDH_1SDV_20171009T141532_20171009T141557_018737_01F9E2_E974_tnr_empty_truncated.tif.zip.zip:INVALID"
        )
