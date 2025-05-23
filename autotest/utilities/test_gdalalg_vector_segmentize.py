#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector segmentize' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal, ogr, osr


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["segmentize"]


def test_gdalalg_vector_segmentize():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    src_lyr = src_ds.CreateLayer("the_layer", srs=srs)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(0 0,0 1)"))
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    src_lyr.CreateFeature(f)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["max-length"] = 0.3

    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    assert (
        out_f.GetGeometryRef().ExportToWkt()
        == "LINESTRING (0 0,0.0 0.25,0.0 0.5,0.0 0.75,0 1)"
    )
    assert (
        out_f.GetGeometryRef().GetSpatialReference().GetAuthorityCode(None) == "32631"
    )
    out_f = out_lyr.GetNextFeature()
    assert out_f.GetGeometryRef() is None
    assert out_lyr.GetFeatureCount() == 2
    out_lyr.SetAttributeFilter("0 = 1")
    assert out_lyr.GetFeatureCount() == 0
    out_lyr.SetAttributeFilter(None)
    assert out_lyr.TestCapability(ogr.OLCFastFeatureCount) == 1
    assert out_lyr.TestCapability(ogr.OLCRandomWrite) == 0
    assert out_lyr.GetExtent() == (0, 0, 0, 1)
    assert out_lyr.GetFeature(0).GetFID() == 0
    assert out_lyr.GetFeature(-1) is None


def test_gdalalg_vector_segmentize_error():

    alg = get_alg()
    with pytest.raises(
        Exception, match="Value of argument 'max-length' is 0, but should be > 0"
    ):
        alg["max-length"] = 0
