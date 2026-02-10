#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Selafin driver testing.
# Author:   François Hissel <francois.hissel@gmail.com>
#
###############################################################################
# Copyright (c) 2014, François Hissel <francois.hissel@gmail.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import math

import gdaltest
import pytest

from osgeo import ogr, osr

pytestmark = pytest.mark.require_driver("Selafin")


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################
# Create Selafin datasource


@pytest.fixture()
def selafin_dsn(tmp_path):

    selafin_drv = ogr.GetDriverByName("Selafin")

    with selafin_drv.CreateDataSource(tmp_path / "tmp.slf") as ds:

        ###############################################################################
        # Add a few points to the datasource

        ref = osr.SpatialReference()
        ref.ImportFromEPSG(4326)
        layer = ds.CreateLayer("name", ref, geom_type=ogr.wkbPoint)
        assert layer is not None, "unable to create layer"
        layer.CreateField(ogr.FieldDefn("value", ogr.OFTReal))
        dfn = layer.GetLayerDefn()
        for i in range(5):
            for j in range(5):
                pt = ogr.Geometry(type=ogr.wkbPoint)
                pt.AddPoint_2D(float(i), float(j))
                feat = ogr.Feature(dfn)
                feat.SetGeometry(pt)
                feat.SetField(0, (float)(i * 5 + j))
                assert layer.CreateFeature(feat) == 0, "unable to create node feature"
        # do some checks
        assert (
            layer.GetFeatureCount() == 25
        ), "wrong number of features after point layer creation"

    return tmp_path / "tmp.slf"


###############################################################################
# Add a set of elements to the datasource


def test_ogr_selafin_create_elements(selafin_dsn):

    selafin_ds = ogr.Open(selafin_dsn, 1)
    layerCount = selafin_ds.GetLayerCount()
    assert layerCount >= 2, "elements layer not created with nodes layer"
    for i in range(layerCount):
        name = selafin_ds.GetLayer(i).GetName()
        if "_e" in name:
            j = i
        if "_p" in name:
            k = i
    layere = selafin_ds.GetLayer(j)
    assert layere.GetDataset().GetDescription() == selafin_ds.GetDescription()
    dfn = layere.GetLayerDefn()
    for i in range(4):
        for j in range(4):
            pol = ogr.Geometry(type=ogr.wkbPolygon)
            poll = ogr.Geometry(type=ogr.wkbLinearRing)
            poll.AddPoint_2D(float(i), float(j))
            poll.AddPoint_2D(float(i), float(j + 1))
            poll.AddPoint_2D(float(i + 1), float(j + 1))
            poll.AddPoint_2D(float(i + 1), float(j))
            poll.AddPoint_2D(float(i), float(j))
            pol.AddGeometry(poll)
            feat = ogr.Feature(dfn)
            feat.SetGeometry(pol)
            assert layere.CreateFeature(feat) == 0, "unable to create element feature"
    pol = ogr.Geometry(type=ogr.wkbPolygon)
    poll = ogr.Geometry(type=ogr.wkbLinearRing)
    poll.AddPoint_2D(4.0, 4.0)
    poll.AddPoint_2D(4.0, 5.0)
    poll.AddPoint_2D(5.0, 5.0)
    poll.AddPoint_2D(5.0, 4.0)
    poll.AddPoint_2D(4.0, 4.0)
    pol.AddGeometry(poll)
    feat = ogr.Feature(dfn)
    feat.SetGeometry(pol)
    assert layere.CreateFeature(feat) == 0, "unable to create element feature"
    # do some checks
    assert (
        selafin_ds.GetLayer(k).GetFeatureCount() == 28
    ), "wrong number of point features after elements layer creation"
    assert (
        math.fabs(layere.GetFeature(5).GetFieldAsDouble(0) - 9) <= 0.01
    ), "wrong value of attribute in element layer"
    assert (
        math.fabs(layere.GetFeature(10).GetFieldAsDouble(0) - 15) <= 0.01
    ), "wrong value of attribute in element layer"


###############################################################################
# Add a field and set its values for point features


def test_ogr_selafin_set_field(selafin_dsn):

    selafin_ds = ogr.Open(selafin_dsn, 1)
    layerCount = selafin_ds.GetLayerCount()
    assert layerCount >= 2, "elements layer not created with nodes layer"
    for i in range(layerCount):
        name = selafin_ds.GetLayer(i).GetName()
        if "_e" in name:
            j = i
        if "_p" in name:
            k = i
    layern = selafin_ds.GetLayer(k)
    selafin_ds.GetLayer(j)
    layern.CreateField(ogr.FieldDefn("reverse", ogr.OFTReal))
    layern.AlterFieldDefn(0, ogr.FieldDefn("new", ogr.OFTReal), ogr.ALTER_NAME_FLAG)
    layern.ReorderFields([1, 0])
    layern.GetLayerDefn()
    for i in range(layern.GetFeatureCount()):
        feat = layern.GetFeature(i)
        val = feat.GetFieldAsDouble(1)
        feat.SetField(0, (float)(val * 10))
        layern.SetFeature(feat)
    # do some checks
    assert (
        math.fabs(layern.GetFeature(11).GetFieldAsDouble(0) - 110) <= 0.01
    ), "wrong value of attribute in point layer"
