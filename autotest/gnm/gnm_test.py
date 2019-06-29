#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic GNMGdalNetwork class functionality.
# Authors:  Mikhail Gusev (gusevmihs at gmail dot com)
#           Dmitry Baryshnikov, polimax@mail.ru
#
###############################################################################
# Copyright (c) 2014, Mikhail Gusev
# Copyright (c) 2014-2015, NextGIS <info@nextgis.com>
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
import shutil



from osgeo import gdal
from osgeo import gnm
import pytest

###############################################################################
# Create file base network


def test_gnm_filenetwork_create():

    try:
        shutil.rmtree('tmp/test_gnm')
    except OSError:
        pass

    ogrtest.drv = None
    ogrtest.have_gnm = 0

    ogrtest.drv = gdal.GetDriverByName('GNMFile')

    if ogrtest.drv is None:
        pytest.skip()

    ds = ogrtest.drv.Create('tmp/', 0, 0, 0, gdal.GDT_Unknown, options=['net_name=test_gnm', 'net_description=Test file based GNM', 'net_srs=EPSG:4326'])
    # cast to GNM
    dn = gnm.CastToNetwork(ds)
    assert dn is not None
    assert dn.GetVersion() == 100, 'GNM: Check GNM version failed'
    assert dn.GetName() == 'test_gnm', 'GNM: Check GNM name failed'
    assert dn.GetDescription() == 'Test file based GNM', \
        'GNM: Check GNM description failed'

    dn = None
    ogrtest.have_gnm = 1

###############################################################################
# Open file base network


def test_gnm_filenetwork_open():

    if not ogrtest.have_gnm:
        pytest.skip()

    ds = gdal.OpenEx('tmp/test_gnm')
    # cast to GNM
    dn = gnm.CastToNetwork(ds)
    assert dn is not None
    assert dn.GetVersion() == 100, 'GNM: Check GNM version failed'
    assert dn.GetName() == 'test_gnm', 'GNM: Check GNM name failed'
    assert dn.GetDescription() == 'Test file based GNM', \
        'GNM: Check GNM description failed'

    dn = None

###############################################################################
# Import layers into file base network


def test_gnm_import():

    if not ogrtest.have_gnm:
        pytest.skip()

    ds = gdal.OpenEx('tmp/test_gnm')

    # pipes
    dspipes = gdal.OpenEx('data/pipes.shp', gdal.OF_VECTOR)
    lyrpipes = dspipes.GetLayerByIndex(0)
    new_lyr = ds.CopyLayer(lyrpipes, 'pipes')
    assert new_lyr is not None, 'failed to import pipes'
    dspipes = None
    new_lyr = None

    # wells
    dswells = gdal.OpenEx('data/wells.shp', gdal.OF_VECTOR)
    lyrwells = dswells.GetLayerByIndex(0)
    new_lyr = ds.CopyLayer(lyrwells, 'wells')
    assert new_lyr is not None, 'failed to import wells'
    dswells = None
    new_lyr = None

    assert ds.GetLayerCount() == 2, 'expected 2 layers'

    ds = None

###############################################################################
# autoconnect


def test_gnm_autoconnect():

    if not ogrtest.have_gnm:
        pytest.skip()

    ds = gdal.OpenEx('tmp/test_gnm')
    dgn = gnm.CastToGenericNetwork(ds)
    assert dgn is not None, 'cast to GNMGenericNetwork failed'

    ret = dgn.ConnectPointsByLines(['pipes', 'wells'], 0.000001, 1, 1, gnm.GNM_EDGE_DIR_BOTH)
    assert ret == 0, 'failed to connect'

    dgn = None

###############################################################################
# Dijkstra shortest path


def test_gnm_graph_dijkstra():

    if not ogrtest.have_gnm:
        pytest.skip()

    ds = gdal.OpenEx('tmp/test_gnm')
    dn = gnm.CastToNetwork(ds)
    assert dn is not None, 'cast to GNMNetwork failed'

    lyr = dn.GetPath(61, 50, gnm.GATDijkstraShortestPath)
    assert lyr is not None, 'failed to get path'

    if lyr.GetFeatureCount() == 0:
        dn.ReleaseResultSet(lyr)
        pytest.fail('failed to get path')

    dn.ReleaseResultSet(lyr)
    dn = None


import ogrtest
###############################################################################
# KShortest Paths


def test_gnm_graph_kshortest():

    if not ogrtest.have_gnm:
        pytest.skip()

    ds = gdal.OpenEx('tmp/test_gnm')
    dn = gnm.CastToNetwork(ds)
    assert dn is not None, 'cast to GNMNetwork failed'

    lyr = dn.GetPath(61, 50, gnm.GATKShortestPath, options=['num_paths=3'])
    assert lyr is not None, 'failed to get path'

    if lyr.GetFeatureCount() < 20:
        dn.ReleaseResultSet(lyr)
        pytest.fail('failed to get path')

    dn.ReleaseResultSet(lyr)
    dn = None

###############################################################################
# ConnectedComponents


def test_gnm_graph_connectedcomponents():

    if not ogrtest.have_gnm:
        pytest.skip()

    ds = gdal.OpenEx('tmp/test_gnm')
    dn = gnm.CastToNetwork(ds)
    assert dn is not None, 'cast to GNMNetwork failed'

    lyr = dn.GetPath(61, 50, gnm.GATConnectedComponents)
    assert lyr is not None, 'failed to get path'

    if lyr.GetFeatureCount() == 0:
        dn.ReleaseResultSet(lyr)
        pytest.fail('failed to get path')

    dn.ReleaseResultSet(lyr)
    dn = None

###############################################################################
# Network deleting


def test_gnm_delete():

    if not ogrtest.have_gnm:
        pytest.skip()

    gdal.GetDriverByName('GNMFile').Delete('tmp/test_gnm')

    assert not os.path.exists('tmp/test_gnm')


