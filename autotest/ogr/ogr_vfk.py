#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR VFK driver functionality.
# Author:   Martin Landa <landa.martin gmail.com>
#
###############################################################################
# Copyright (c) 2009-2019 Martin Landa <landa.martin gmail.com>
# Copyright (c) 2010-2012, Even Rouault <even dot rouault at spatialys.com>
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
from osgeo import gdal
from osgeo import ogr
import pytest

###############################################################################
# Open file, check number of layers, get first layer,
# check number of fields and features


def test_ogr_vfk_1():

    gdaltest.vfk_drv = ogr.GetDriverByName('VFK')
    if gdaltest.vfk_drv is None:
        pytest.skip()

    gdal.SetConfigOption('OGR_VFK_DB_OVERWRITE', 'YES')

    gdaltest.vfk_ds = ogr.Open('data/vfk/bylany.vfk')
    gdal.SetConfigOption('OGR_VFK_DB_OVERWRITE', None)

    assert gdaltest.vfk_ds is not None

    assert gdaltest.vfk_ds.GetLayerCount() == 61, 'expected exactly 61 layers!'

    gdaltest.vfk_layer_par = gdaltest.vfk_ds.GetLayer(0)

    assert gdaltest.vfk_layer_par is not None, 'cannot get first layer'

    assert gdaltest.vfk_layer_par.GetName() == 'PAR', \
        'did not get expected layer name "PAR"'

    defn = gdaltest.vfk_layer_par.GetLayerDefn()
    assert defn.GetFieldCount() == 28, \
        ('did not get expected number of fields, got %d' % defn.GetFieldCount())

    fc = gdaltest.vfk_layer_par.GetFeatureCount()
    assert fc == 1, ('did not get expected feature count, got %d' % fc)

###############################################################################
# Read the first feature from layer 'PAR', check envelope


def test_ogr_vfk_2():

    if gdaltest.vfk_drv is None:
        pytest.skip()

    gdaltest.vfk_layer_par.ResetReading()

    feat = gdaltest.vfk_layer_par.GetNextFeature()

    assert feat.GetFID() == 1, 'did not get expected fid for feature 1'

    geom = feat.GetGeometryRef()
    assert geom.GetGeometryType() == ogr.wkbPolygon, \
        'did not get expected geometry type.'

    envelope = geom.GetEnvelope()
    area = (envelope[1] - envelope[0]) * (envelope[3] - envelope[2])
    exp_area = 2010.5

    assert area >= exp_area - 0.5 and area <= exp_area + 0.5, \
        ('envelope area not as expected, got %g.' % area)

###############################################################################
# Read features from layer 'SOBR', test attribute query


def test_ogr_vfk_3():

    if gdaltest.vfk_drv is None:
        pytest.skip()

    gdaltest.vfk_layer_sobr = gdaltest.vfk_ds.GetLayer(43)

    assert gdaltest.vfk_layer_sobr.GetName() == 'SOBR', \
        'did not get expected layer name "SOBR"'

    gdaltest.vfk_layer_sobr.SetAttributeFilter("CISLO_BODU = '55'")

    gdaltest.vfk_layer_sobr.ResetReading()

    feat = gdaltest.vfk_layer_sobr.GetNextFeature()
    count = 0
    while feat:
        feat = gdaltest.vfk_layer_sobr.GetNextFeature()
        count += 1

    assert count == 1, ('did not get expected number of features, got %d' % count)

###############################################################################
# Read features from layer 'SBP', test random access, check length


def test_ogr_vfk_4():

    if gdaltest.vfk_drv is None:
        pytest.skip()

    gdaltest.vfk_layer_sbp = gdaltest.vfk_ds.GetLayerByName('SBP')

    assert gdaltest.vfk_layer_sbp, 'did not get expected layer name "SBP"'

    feat = gdaltest.vfk_layer_sbp.GetFeature(5)
    length = int(feat.geometry().Length())

    assert length == 10, ('did not get expected length, got %d' % length)

###############################################################################
# Read features from layer 'HP', check geometry type


def test_ogr_vfk_5():

    if gdaltest.vfk_drv is None:
        pytest.skip()

    gdaltest.vfk_layer_hp = gdaltest.vfk_ds.GetLayerByName('HP')

    assert gdaltest.vfk_layer_hp != 'HP', 'did not get expected layer name "HP"'

    geom_type = gdaltest.vfk_layer_hp.GetGeomType()

    assert geom_type == ogr.wkbLineString, \
        ('did not get expected geometry type, got %d' % geom_type)

###############################################################################
# Re-Open file (test .db persistence)


def test_ogr_vfk_6():

    if gdaltest.vfk_drv is None:
        pytest.skip()

    gdaltest.vfk_layer_par = None
    gdaltest.vfk_layer_sobr = None
    gdaltest.vfk_ds = None
    gdaltest.vfk_ds = ogr.Open('data/vfk/bylany.vfk')

    assert gdaltest.vfk_ds is not None

    assert gdaltest.vfk_ds.GetLayerCount() == 61, 'expected exactly 61 layers!'

    gdaltest.vfk_layer_par = gdaltest.vfk_ds.GetLayer(0)

    assert gdaltest.vfk_layer_par is not None, 'cannot get first layer'

    assert gdaltest.vfk_layer_par.GetName() == 'PAR', \
        'did not get expected layer name "PAR"'

    defn = gdaltest.vfk_layer_par.GetLayerDefn()
    assert defn.GetFieldCount() == 28, \
        ('did not get expected number of fields, got %d' % defn.GetFieldCount())

    fc = gdaltest.vfk_layer_par.GetFeatureCount()
    assert fc == 1, ('did not get expected feature count, got %d' % fc)

###############################################################################
# Read PAR layer, check data types (Integer64 new in GDAL 2.2)


def test_ogr_vfk_7():

    if gdaltest.vfk_drv is None:
        pytest.skip()

    defn = gdaltest.vfk_layer_par.GetLayerDefn()

    for idx, name, ctype in ((0, "ID", ogr.OFTInteger64),
                             (1, "STAV_DAT", ogr.OFTInteger),
                             (2, "DATUM_VZNIKU", ogr.OFTString),
                             (22, "CENA_NEMOVITOSTI", ogr.OFTReal)):
        col = defn.GetFieldDefn(idx)
        assert col.GetName() == name and col.GetType() == ctype, \
            "PAR: '{}' column name/type mismatch".format(name)


###############################################################################
# Open DB file as datasource (new in GDAL 2.2)


def test_ogr_vfk_8():

    if gdaltest.vfk_drv is None:
        pytest.skip()

    # open by SQLite driver first
    vfk_ds_db = ogr.Open('data/vfk/bylany.db')
    count1 = vfk_ds_db.GetLayerCount()
    vfk_ds_db = None

    # then open by VFK driver
    os.environ['OGR_VFK_DB_READ'] = 'YES'
    vfk_ds_db = ogr.Open('data/vfk/bylany.db')
    count2 = vfk_ds_db.GetLayerCount()
    vfk_ds_db = None

    assert count1 == count2, \
        'layer count differs when opening DB by SQLite and VFK drivers'

    del os.environ['OGR_VFK_DB_READ']

###############################################################################
# Open datasource with SUPPRESS_GEOMETRY open option (new in GDAL 2.3)


def test_ogr_vfk_9():

    if gdaltest.vfk_drv is None:
        pytest.skip()

    # open with suppressing geometry
    vfk_ds = None
    vfk_ds = gdal.OpenEx('data/vfk/bylany.vfk', open_options=['SUPPRESS_GEOMETRY=YES'])

    vfk_layer_par = vfk_ds.GetLayerByName('PAR')

    assert vfk_layer_par != 'PAR', 'did not get expected layer name "PAR"'

    geom_type = vfk_layer_par.GetGeomType()
    vfk_layer_par = None
    vfk_ds = None

    assert geom_type == ogr.wkbNone, \
        ('did not get expected geometry type, got %d' % geom_type)

###############################################################################
# Open datasource with FILE_FIELD open option (new in GDAL 2.4)


def test_ogr_vfk_10():

    if gdaltest.vfk_drv is None:
        pytest.skip()

    # open with suppressing geometry
    vfk_ds = None
    vfk_ds = gdal.OpenEx('data/vfk/bylany.vfk', open_options=['FILE_FIELD=YES'])

    vfk_layer_par = vfk_ds.GetLayerByName('PAR')

    assert vfk_layer_par != 'PAR', 'did not get expected layer name "PAR"'

    vfk_layer_par.ResetReading()
    feat = vfk_layer_par.GetNextFeature()
    file_field = feat.GetField('VFK_FILENAME')
    vfk_layer_par = None
    vfk_ds = None

    assert file_field == 'bylany.vfk', 'did not get expected file field value'

###############################################################################
# Read PAR layer, check sequential feature access consistency


def test_ogr_vfk_11():
    def count_features():
        gdaltest.vfk_layer_par.ResetReading()
        count = 0
        while True:
            feat = gdaltest.vfk_layer_par.GetNextFeature()
            if not feat:
                break
            count += 1

        return count

    if gdaltest.vfk_drv is None:
        pytest.skip()

    count = gdaltest.vfk_layer_par.GetFeatureCount()
    for i in range(2):  # perform check twice, mix with random access
        if count != count_features():
            feat = gdaltest.vfk_layer_par.GetFeature(i)
            feat.DumpReadable()
            pytest.fail('did not get expected number of features')

###############################################################################
# Read SBP layer, check curved geometry


def test_ogr_vfk_12():

    if gdaltest.vfk_drv is None:
        pytest.skip()

    gdaltest.vfk_layer_sbp = gdaltest.vfk_ds.GetLayerByName('SBP')

    gdaltest.vfk_layer_sbp.SetAttributeFilter("PARAMETRY_SPOJENI = '16'")
    feat = gdaltest.vfk_layer_sbp.GetNextFeature()
    geom = feat.GetGeometryRef()

    assert geom.GetGeometryType() == ogr.wkbLineString, \
        'did not get expected geometry type.'

    assert geom.GetPointCount() == 92, \
        'did not get expected number of points.'

###############################################################################
# cleanup


def test_ogr_vfk_cleanup():

    if gdaltest.vfk_drv is None:
        pytest.skip()

    gdaltest.vfk_layer_par = None
    gdaltest.vfk_layer_hp = None
    gdaltest.vfk_layer_sobr = None
    gdaltest.vfk_ds = None

    try:
        os.remove('data/vfk/bylany.db')
    except OSError:
        pass


###############################################################################
#



