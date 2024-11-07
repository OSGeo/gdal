#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test STACIT driver
# Author:   Even Rouault <even.rouault@spatialys.com>
#
###############################################################################
# Copyright (c) 2021, Even Rouault <even.rouault@spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json

import gdaltest
import pytest
import webserver

from osgeo import gdal

pytestmark = pytest.mark.require_driver("STACIT")


def test_stacit_basic():

    ds = gdal.Open("data/stacit/test.json")
    assert ds is not None
    assert ds.GetDriver().GetDescription() == "STACIT"
    assert ds.RasterCount == 1
    assert ds.RasterXSize == 40
    assert ds.RasterYSize == 20
    assert ds.GetSpatialRef().GetName() == "NAD27 / UTM zone 11N"
    assert ds.GetGeoTransform() == pytest.approx(
        [440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0], rel=1e-8
    )
    assert ds.GetRasterBand(1).GetNoDataValue() is None

    vrt = ds.GetMetadata("xml:VRT")[0]
    placement_vrt = """<SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/int16.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="20" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>"""
    assert placement_vrt in vrt

    assert ds.GetRasterBand(1).Checksum() == 9239


def test_stacit_max_items():

    ds = gdal.OpenEx("data/stacit/test.json", open_options=["MAX_ITEMS=1"])
    assert ds is not None
    assert ds.RasterXSize == 20
    assert ds.GetRasterBand(1).Checksum() == 4672


def test_stacit_multiple_assets():

    ds = gdal.Open("data/stacit/test_multiple_assets.json")
    assert ds is not None
    assert ds.RasterCount == 0
    subds = ds.GetSubDatasets()
    assert subds == [
        (
            'STACIT:"data/stacit/test_multiple_assets.json":collection=my_collection,asset=B01,crs=EPSG_26711',
            "Collection my_collection, Asset B01 of data/stacit/test_multiple_assets.json in CRS EPSG:26711",
        ),
        (
            'STACIT:"data/stacit/test_multiple_assets.json":collection=my_collection,asset=B01,crs=EPSG_26712',
            "Collection my_collection, Asset B01 of data/stacit/test_multiple_assets.json in CRS EPSG:26712",
        ),
        (
            'STACIT:"data/stacit/test_multiple_assets.json":collection=my_collection,asset=B02',
            "Collection my_collection, Asset B02 of data/stacit/test_multiple_assets.json",
        ),
        (
            'STACIT:"data/stacit/test_multiple_assets.json":collection=my_collection2,asset=B01',
            "Collection my_collection2, Asset B01 of data/stacit/test_multiple_assets.json",
        ),
    ]

    ds = gdal.Open(
        'STACIT:"data/stacit/test_multiple_assets.json":collection=my_collection,asset=B01,crs=EPSG_26711'
    )
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.GetSpatialRef().GetName() == "NAD27 / UTM zone 11N"
    assert ds.GetGeoTransform() == pytest.approx(
        [440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0], rel=1e-8
    )

    ds = gdal.Open(
        'STACIT:"data/stacit/test_multiple_assets.json":collection=my_collection,asset=B01,crs=EPSG_26712'
    )
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.GetSpatialRef().GetName() == "NAD27 / UTM zone 12N"
    assert ds.GetGeoTransform() == pytest.approx(
        [440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0], rel=1e-8
    )

    ds = gdal.Open(
        'STACIT:"data/stacit/test_multiple_assets.json":collection=my_collection,asset=B02'
    )
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.GetSpatialRef().GetName() == "NAD27 / UTM zone 11N"
    assert ds.GetGeoTransform() == pytest.approx(
        [-440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0], rel=1e-8
    )

    ds = gdal.Open(
        'STACIT:"data/stacit/test_multiple_assets.json":collection=my_collection2,asset=B01'
    )
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.GetSpatialRef().GetName() == "NAD27 / UTM zone 13N"
    assert ds.GetGeoTransform() == pytest.approx(
        [440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0], rel=1e-8
    )

    with pytest.raises(Exception):
        gdal.Open(
            'STACIT:"data/stacit/test_multiple_assets.json":collection=i_dont_exist'
        )

    with pytest.raises(Exception):
        gdal.Open('STACIT:"data/stacit/test_multiple_assets.json":asset=i_dont_exist')


@pytest.mark.require_geos
def test_stacit_overlapping_sources():

    ds = gdal.Open("data/stacit/overlapping_sources.json")
    assert ds is not None

    # Check that the source covered by another one is not listed
    vrt = ds.GetMetadata("xml:VRT")[0]
    only_one_simple_source = """
    <ColorInterp>Coastal</ColorInterp>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>
  </VRTRasterBand>"""
    # print(vrt)
    assert only_one_simple_source in vrt

    ds = gdal.OpenEx(
        "data/stacit/overlapping_sources.json",
        open_options=["OVERLAP_STRATEGY=REMOVE_IF_NO_NODATA"],
    )
    assert ds is not None
    vrt = ds.GetMetadata("xml:VRT")[0]
    assert only_one_simple_source in vrt

    ds = gdal.OpenEx(
        "data/stacit/overlapping_sources.json",
        open_options=["OVERLAP_STRATEGY=USE_MOST_RECENT"],
    )
    assert ds is not None
    vrt = ds.GetMetadata("xml:VRT")[0]
    assert only_one_simple_source in vrt

    ds = gdal.OpenEx(
        "data/stacit/overlapping_sources.json",
        open_options=["OVERLAP_STRATEGY=USE_ALL"],
    )
    assert ds is not None
    assert len(ds.GetFileList()) == 4
    vrt = ds.GetMetadata("xml:VRT")[0]


@pytest.mark.require_geos
def test_stacit_overlapping_sources_with_nodata():

    ds = gdal.Open("data/stacit/overlapping_sources_with_nodata.json")
    assert ds is not None
    assert len(ds.GetFileList()) == 3
    vrt = ds.GetMetadata("xml:VRT")[0]
    # print(vrt)
    two_sources = """<ComplexSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <NODATA>0</NODATA>
    </ComplexSource>
    <ComplexSource>
      <SourceFilename relativeToVRT="0">data/byte_nodata_0.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <NODATA>0</NODATA>
    </ComplexSource>"""
    assert two_sources in vrt

    ds = gdal.OpenEx(
        "data/stacit/overlapping_sources_with_nodata.json",
        open_options=["OVERLAP_STRATEGY=REMOVE_IF_NO_NODATA"],
    )
    assert ds is not None
    vrt = ds.GetMetadata("xml:VRT")[0]
    assert len(ds.GetFileList()) == 3
    assert two_sources in vrt

    ds = gdal.OpenEx(
        "data/stacit/overlapping_sources_with_nodata.json",
        open_options=["OVERLAP_STRATEGY=USE_MOST_RECENT"],
    )
    assert ds is not None
    assert len(ds.GetFileList()) == 2

    ds = gdal.OpenEx(
        "data/stacit/overlapping_sources_with_nodata.json",
        open_options=["OVERLAP_STRATEGY=USE_ALL"],
    )
    assert ds is not None
    vrt = ds.GetMetadata("xml:VRT")[0]
    assert len(ds.GetFileList()) == 3
    assert two_sources in vrt


# Launch a single webserver in a module-scoped fixture.
@pytest.fixture(scope="module")
def webserver_launch():

    process, port = webserver.launch(handler=webserver.DispatcherHttpHandler)

    yield process, port

    webserver.server_stop(process, port)


@pytest.fixture(scope="function")
def webserver_port(webserver_launch):

    webserver_process, webserver_port = webserver_launch

    if webserver_port == 0:
        pytest.skip()
    yield webserver_port


@pytest.mark.require_curl
def test_stacit_post_paging(tmp_vsimem, webserver_port):

    initial_doc = {
        "type": "FeatureCollection",
        "stac_version": "1.0.0-beta.2",
        "stac_extensions": [],
        "features": json.loads(open("data/stacit/test.json", "rb").read())["features"],
        "links": [
            {
                "rel": "next",
                "href": f"http://localhost:{webserver_port}/request",
                "method": "POST",
                "body": {"token": "page_2"},
                "headers": {"foo": "bar"},
            }
        ],
    }

    filename = str(tmp_vsimem / "tmp.json")
    gdal.FileFromMemBuffer(filename, json.dumps(initial_doc))

    next_page_doc = {
        "type": "FeatureCollection",
        "stac_version": "1.0.0-beta.2",
        "stac_extensions": [],
        "features": json.loads(open("data/stacit/test_page2.json", "rb").read())[
            "features"
        ],
    }

    handler = webserver.SequentialHandler()
    handler.add(
        "POST",
        "/request",
        200,
        {"Content-type": "application/json"},
        json.dumps(next_page_doc),
        expected_headers={"Content-Type": "application/json", "foo": "bar"},
        expected_body=b'{\n  "token":"page_2"\n}',
    )
    with webserver.install_http_handler(handler):
        ds = gdal.Open(filename)
    assert ds is not None
    assert ds.RasterCount == 1
    assert ds.RasterXSize == 40
    assert ds.RasterYSize == 20
    assert ds.GetSpatialRef().GetName() == "NAD27 / UTM zone 11N"
    assert ds.GetGeoTransform() == pytest.approx(
        [440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0], rel=1e-8
    )


###############################################################################
# Test force opening a STACIT file


def test_stacit_force_opening(tmp_vsimem):

    filename = str(tmp_vsimem / "test.foo")

    with open("data/stacit/test.json", "rb") as fsrc:
        with gdaltest.vsi_open(filename, "wb") as fdest:
            fdest.write(fsrc.read(1))
            fdest.write(b" " * (1000 * 1000))
            fdest.write(fsrc.read())

    with pytest.raises(Exception):
        gdal.OpenEx(filename)

    ds = gdal.OpenEx(filename, allowed_drivers=["STACIT"])
    assert ds.GetDriver().GetDescription() == "STACIT"


###############################################################################
# Test force opening a URL as STACIT


def test_stacit_force_opening_url():

    drv = gdal.IdentifyDriverEx("http://example.com", allowed_drivers=["STACIT"])
    assert drv.GetDescription() == "STACIT"


###############################################################################
# Test force opening, but provided file is still not recognized (for good reasons)


def test_stacit_force_opening_no_match():

    drv = gdal.IdentifyDriverEx("data/byte.tif", allowed_drivers=["STACIT"])
    assert drv is None


###############################################################################
# Test opening a top-level Feature


def test_stacit_single_feature(tmp_vsimem):

    j = json.loads(open("data/stacit/test.json", "rb").read())
    j = j["features"][0]

    filename = str(tmp_vsimem / "feature.json")
    with gdaltest.tempfile(filename, json.dumps(j)):
        ds = gdal.Open(filename)
        assert ds is not None
        assert ds.RasterXSize == 20
        assert ds.GetRasterBand(1).Checksum() == 4672
