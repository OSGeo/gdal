#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal driver gti create' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_driver("GTI")


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["driver"]["gti"]["create"]


def test_gdalalg_driver_gti_create_xml_filename(tmp_vsimem):

    xml_filename = tmp_vsimem / "out.xml"

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "my_layer"
    alg["xml-filename"] = xml_filename
    alg["fetch-metadata"] = "AREA_OR_POINT,area_or_point,String"
    assert alg.Run()

    ds = alg["output"].GetDataset()
    lyr = ds.GetLayerByName("my_layer")
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "26711"
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "location"
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetName() == "area_or_point"
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTString
    f = lyr.GetNextFeature()
    assert f["location"] == "../gcore/data/byte.tif"
    assert f["area_or_point"] == "Area"
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "POLYGON ((440720 3751320,441920 3751320,441920 3750120,440720 3750120,440720 3751320))"
    )
    assert lyr.GetMetadata_Dict() == {}

    with gdal.VSIFile(xml_filename, "rb") as f:
        assert (
            f.read()
            == b"<GDALTileIndexDataset>\n  <IndexDataset></IndexDataset>\n  <IndexLayer>my_layer</IndexLayer>\n  <LocationField>location</LocationField>\n</GDALTileIndexDataset>\n"
        )


def test_gdalalg_driver_gti_create():

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "my_layer"
    alg["resolution"] = [10, 11]
    alg["datatype"] = "UInt16"
    alg["bbox"] = [1, 2, 3, 4]
    alg["band-count"] = 2
    alg["nodata"] = [5, 6]
    alg["color-interpretation"] = ["red", "green"]
    alg["mask"] = True
    assert alg.Run()

    ds = alg["output"].GetDataset()
    lyr = ds.GetLayerByName("my_layer")
    assert lyr.GetMetadata_Dict() == {
        "BAND_COUNT": "2",
        "COLOR_INTERPRETATION": "red,green",
        "DATA_TYPE": "UInt16",
        "LOCATION_FIELD": "location",
        "MASK_BAND": "YES",
        "MAXX": "3",
        "MAXY": "4",
        "MINX": "1",
        "MINY": "2",
        "NODATA": "5,6",
        "RESX": "10",
        "RESY": "11",
    }


def test_gdalalg_driver_gti_create_wrong_nodata():

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "my_layer"
    alg["band-count"] = 3
    alg["nodata"] = [5, 6]
    with pytest.raises(
        Exception, match="2 nodata values whereas one or 3 were expected"
    ):
        alg.Run()


def test_gdalalg_driver_gti_create_wrong_color_interpretation():

    alg = get_alg()
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["output-layer"] = "my_layer"
    alg["band-count"] = 3
    alg["color-interpretation"] = ["red", "green"]
    with pytest.raises(
        Exception, match="2 color interpretations whereas one or 3 were expected"
    ):
        alg.Run()


def test_gdalalg_driver_gti_create_wrong_fetch_metadata():

    alg = get_alg()
    with pytest.raises(
        Exception,
        match="'foo' is not of the form <gdal-metadata-name>,<field-name>,<field-type>",
    ):
        alg["fetch-metadata"] = "foo"

    alg = get_alg()
    with pytest.raises(
        Exception,
        match="'foo,bar,baz' has an invalid field type 'baz'. It should be one of 'String', 'Integer', 'Integer64', 'Real', 'Date', 'DateTime'",
    ):
        alg["fetch-metadata"] = "foo,bar,baz"
