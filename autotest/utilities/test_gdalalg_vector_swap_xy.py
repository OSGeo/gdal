#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector swap-xy' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

from osgeo import gdal, ogr, osr


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["swap-xy"]


def test_gdalalg_vector_swap_xy():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    src_lyr = src_ds.CreateLayer("the_layer", srs=srs)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    src_lyr.CreateFeature(f)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    assert out_f.GetGeometryRef().ExportToIsoWkt() == "POINT (2 1)"
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
    assert out_lyr.GetExtent() == (2, 2, 1, 1)
    assert out_lyr.GetFeature(0).GetFID() == 0
    assert out_lyr.GetFeature(-1) is None
