#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test ESRI shapefile spatial index mechanism (.sbn files). This can serve
#           as a test for the functionality of shapelib's sbnsearch.c
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os

import gdaltest
import pytest

from osgeo import ogr

###############################################################################
#


def search_all_features(lyr):

    geoms = []
    lyr.SetSpatialFilter(None)
    extents = lyr.GetExtent()
    fc_ref = lyr.GetFeatureCount()

    feat = lyr.GetNextFeature()
    while feat is not None:
        geom = feat.GetGeometryRef()
        geoms.append(geom.Clone())
        feat = lyr.GetNextFeature()

    # Test getting each geom 1 by 1
    for geom in geoms:
        bbox = geom.GetEnvelope()
        lyr.SetSpatialFilterRect(bbox[0], bbox[2], bbox[1], bbox[3])
        lyr.ResetReading()
        found_geom = False
        feat = lyr.GetNextFeature()
        while feat is not None and found_geom is False:
            got_geom = feat.GetGeometryRef()
            if got_geom.Equals(geom) == 1:
                found_geom = True
            else:
                feat = lyr.GetNextFeature()
        assert found_geom, "did not find geometry for %s" % (geom.ExportToWkt())

    # Get all geoms in a single gulp. We do not use exactly the extent bounds, because
    # there is an optimization in the shapefile driver to skip the spatial index in that
    # case.
    eps = 0.0001
    lyr.SetSpatialFilterRect(
        extents[0] + eps, extents[2] + eps, extents[1] - eps, extents[3] - eps
    )
    lyr.ResetReading()
    fc = lyr.GetFeatureCount()

    # For point layers, we need a special case since there may be points on the border
    # of the extent
    if lyr.GetGeomType() == ogr.wkbPoint:
        lyr.SetSpatialFilterRect(
            extents[0], extents[2] + eps, extents[0] + eps, extents[3] - eps
        )
        lyr.ResetReading()
        fc = fc + lyr.GetFeatureCount()

        lyr.SetSpatialFilterRect(
            extents[1] - eps, extents[2] + eps, extents[1], extents[3] - eps
        )
        lyr.ResetReading()
        fc = fc + lyr.GetFeatureCount()

        lyr.SetSpatialFilterRect(extents[0], extents[2], extents[1], extents[2] + eps)
        lyr.ResetReading()
        fc = fc + lyr.GetFeatureCount()

        lyr.SetSpatialFilterRect(extents[0], extents[3] - eps, extents[1], extents[3])
        lyr.ResetReading()
        fc = fc + lyr.GetFeatureCount()

    assert fc == fc_ref, "layer %s: expected %d. got %d" % (lyr.GetName(), fc_ref, fc)


###############################################################################
# Test


def test_ogr_shape_sbn_1():

    gdaltest.download_or_skip(
        "http://pubs.usgs.gov/sim/3194/contents/Cochiti_shapefiles.zip",
        "Cochiti_shapefiles.zip",
    )

    try:
        os.stat("tmp/cache/CochitiDamShapeFiles/CochitiBoundary.shp")
    except OSError:
        try:
            gdaltest.unzip("tmp/cache", "tmp/cache/Cochiti_shapefiles.zip")
            try:
                os.stat("tmp/cache/CochitiDamShapeFiles/CochitiBoundary.shp")
            except OSError:
                pytest.skip()
        except OSError:
            pytest.skip()

    ds = ogr.Open("tmp/cache/CochitiDamShapeFiles")
    for i in range(ds.GetLayerCount()):
        lyr = ds.GetLayer(i)
        search_all_features(lyr)


###############################################################################
# Test


def test_ogr_shape_sbn_2():

    ds = ogr.Open("data/shp/CoHI_GCS12.shp")
    lyr = ds.GetLayer(0)
    return search_all_features(lyr)


###############################################################################
# Test bugfix for https://github.com/OSGeo/gdal/issues/9430


@pytest.mark.require_curl()
def test_ogr_shape_sbn_out_of_order_bin_start():

    srv = "https://github.com/OSGeo/gdal-test-datasets/raw/master/shapefile/65sv5l285i_GLWD_level2/README.TXT"
    if gdaltest.gdalurlopen(srv, timeout=5) is None:
        pytest.skip(reason=f"{srv} is down")

    ds = ogr.Open(
        "/vsizip//vsicurl/https://github.com/OSGeo/gdal-test-datasets/raw/master/shapefile/65sv5l285i_GLWD_level2/65sv5l285i_GLWD_level2_sozip.zip"
    )
    lyr = ds.GetLayer(0)
    lyr.SetSpatialFilterRect(5, 5, 6, 6)
    assert lyr.GetFeatureCount() == 13
