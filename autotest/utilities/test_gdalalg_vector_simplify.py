#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector simplify' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_geos


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["simplify"]


def test_gdalalg_vector_simplify():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    src_lyr = src_ds.CreateLayer("the_layer", srs=srs)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(0 0,1 1,2 0)"))
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    src_lyr.CreateFeature(f)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["tolerance"] = 2

    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    assert out_f.GetGeometryRef().ExportToWkt() == "LINESTRING (0 0,2 0)"
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
    assert out_lyr.GetExtent() == (0, 2, 0, 1)
    assert out_lyr.GetFeature(0).GetFID() == 0
    assert out_lyr.GetFeature(-1) is None


@pytest.mark.parametrize("tol", (-1, float("nan")))
def test_gdalalg_vector_simplify_error(tol):

    alg = get_alg()
    with pytest.raises(
        Exception, match="Value of argument 'tolerance' is .*, but should be >= 0"
    ):
        alg["tolerance"] = tol
