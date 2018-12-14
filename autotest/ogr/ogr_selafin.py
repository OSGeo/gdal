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
import math


import gdaltest
from osgeo import ogr
from osgeo import osr
import pytest

###############################################################################
# Create wasp datasource


def test_ogr_selafin_create_ds():

    gdaltest.selafin_ds = None
    try:
        os.remove('tmp/tmp.slf')
    except OSError:
        pass
    selafin_drv = ogr.GetDriverByName('Selafin')

    gdaltest.selafin_ds = selafin_drv.CreateDataSource('tmp/tmp.slf')

    if gdaltest.selafin_ds is not None:
        return
    pytest.fail()

###############################################################################
# Add a few points to the datasource


def test_ogr_selafin_create_nodes():
    test_ogr_selafin_create_ds()
    ref = osr.SpatialReference()
    ref.ImportFromEPSG(4326)
    layer = gdaltest.selafin_ds.CreateLayer('name', ref, geom_type=ogr.wkbPoint)
    assert layer is not None, 'unable to create layer'
    layer.CreateField(ogr.FieldDefn('value', ogr.OFTReal))
    dfn = layer.GetLayerDefn()
    for i in range(5):
        for j in range(5):
            pt = ogr.Geometry(type=ogr.wkbPoint)
            pt.AddPoint_2D(float(i), float(j))
            feat = ogr.Feature(dfn)
            feat.SetGeometry(pt)
            feat.SetField(0, (float)(i * 5 + j))
            assert layer.CreateFeature(feat) == 0, 'unable to create node feature'
    # do some checks
    assert layer.GetFeatureCount() == 25, \
        'wrong number of features after point layer creation'
    # return
    del gdaltest.selafin_ds
    del layer

###############################################################################
# Add a set of elements to the datasource


def test_ogr_selafin_create_elements():

    gdaltest.selafin_ds = ogr.Open('tmp/tmp.slf', 1)
    if gdaltest.selafin_ds is None:
        pytest.skip()
    layerCount = gdaltest.selafin_ds.GetLayerCount()
    assert layerCount >= 2, 'elements layer not created with nodes layer'
    for i in range(layerCount):
        name = gdaltest.selafin_ds.GetLayer(i).GetName()
        if '_e' in name:
            j = i
        if '_p' in name:
            k = i
    layere = gdaltest.selafin_ds.GetLayer(j)
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
            assert layere.CreateFeature(feat) == 0, 'unable to create element feature'
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
    assert layere.CreateFeature(feat) == 0, 'unable to create element feature'
    # do some checks
    assert gdaltest.selafin_ds.GetLayer(k).GetFeatureCount() == 28, \
        'wrong number of point features after elements layer creation'
    assert math.fabs(layere.GetFeature(5).GetFieldAsDouble(0) - 9) <= 0.01, \
        'wrong value of attribute in element layer'
    assert math.fabs(layere.GetFeature(10).GetFieldAsDouble(0) - 15) <= 0.01, \
        'wrong value of attribute in element layer'
    # return
    del gdaltest.selafin_ds

###############################################################################
# Add a field and set its values for point features


def test_ogr_selafin_set_field():

    gdaltest.selafin_ds = ogr.Open('tmp/tmp.slf', 1)
    if gdaltest.selafin_ds is None:
        pytest.skip()
    layerCount = gdaltest.selafin_ds.GetLayerCount()
    assert layerCount >= 2, 'elements layer not created with nodes layer'
    for i in range(layerCount):
        name = gdaltest.selafin_ds.GetLayer(i).GetName()
        if '_e' in name:
            j = i
        if '_p' in name:
            k = i
    layern = gdaltest.selafin_ds.GetLayer(k)
    gdaltest.selafin_ds.GetLayer(j)
    layern.CreateField(ogr.FieldDefn('reverse', ogr.OFTReal))
    layern.AlterFieldDefn(0, ogr.FieldDefn('new', ogr.OFTReal), ogr.ALTER_NAME_FLAG)
    layern.ReorderFields([1, 0])
    layern.GetLayerDefn()
    for i in range(28):
        feat = layern.GetFeature(i)
        val = feat.GetFieldAsDouble(1)
        feat.SetField(0, (float)(val * 10))
        layern.SetFeature(feat)
    # do some checks
    assert math.fabs(layern.GetFeature(11).GetFieldAsDouble(0) - 110) <= 0.01, \
        'wrong value of attribute in point layer'
    # return
    del gdaltest.selafin_ds


###############################################################################
# Cleanup

def test_ogr_selafin_cleanup():

    selafin_drv = ogr.GetDriverByName('Selafin')
    selafin_drv.DeleteDataSource('tmp/tmp.slf')



