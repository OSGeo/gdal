#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test DWG Driver.
# Author:   Jorge Gustavo Rocha <jgr at geomaster dot pt>
#
###############################################################################
# Copyright (c) 2017, Jorge Gustavo Rocha <jgr at geomaster dot pt>
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

import pytest
import json

from osgeo import gdal
import gdaltest

pytestmark = pytest.mark.require_driver('DWG')

###############################################################################
#
# The test file was contributed by Maxime Colmant, from Mapwize
# (see https://github.com/OSGeo/gdal/pull/5013)
#
# The AutoCAD drawing format version for this file is: 
# AC1027 - DWG AutoCAD 2013/2014/2015/2016/2017 
#
# The drawing format can be checked reading the first six bytes of the DWG file
#
# od -c ogr/data/cad/Building_A_Floor_0_Mapwize.dwg | head -1
# 0000000   A   C   1   0   2   7  \0  \0  \0  \0  \0   } 003 300 001  \0
#
###############################################################################

def test_ogr_dwg_1():

    ds = gdal.OpenEx('data/cad/Building_A_Floor_0_Mapwize.dwg', allowed_drivers=['DWG'])

    assert ds is not None

    assert ds.GetLayerCount() == 1, 'expected exactly one layer.'

    layer = ds.GetLayer(0)

    assert layer.GetName() == 'entities', \
        'layer name is expected to be entities.'

    defn = layer.GetLayerDefn()

    assert defn.GetFieldCount() == 6, \
        ('did not get expected number of fields in defn. got %d'
                             % defn.GetFieldCount())

    fc = layer.GetFeatureCount()

    assert fc == 425, ('did not get expected feature count, got %d' % fc)

    layer.ResetReading()
    layer.SetAttributeFilter("layer = 'Trees'")
    tree = layer.GetNextFeature()
    geom = tree.GetGeometryRef()

    assert geom.GetGeometryName() == 'GEOMETRYCOLLECTION', \
        'expanded block geometry is expected to be GEOMETRYCOLLECTION.'

    ds = None


def test_ogr_dwg_2():

    with gdaltest.config_option('DWG_INLINE_BLOCKS', 'FALSE'):

        ds = gdal.OpenEx('data/cad/Building_A_Floor_0_Mapwize.dwg', allowed_drivers=['DWG'])

        assert ds is not None

        assert ds.GetLayerCount() == 2, 'expected two layers.'

        zero = ds.GetLayer(0)

        assert zero.GetName() == 'blocks', \
            'layer name is expected to be blocks.'

        layer = ds.GetLayer( 'entities' )
        defn = layer.GetLayerDefn()

        assert defn.GetFieldCount() == 10, \
            ('did not get expected number of fields in defn. got %d'
                                % defn.GetFieldCount())

        fc = layer.GetFeatureCount()

        assert fc == 245, ('did not get expected feature count, got %d' % fc)

        layer.ResetReading()
        layer.SetAttributeFilter("layer = 'Trees'")
        tree = layer.GetNextFeature()
        geom = tree.GetGeometryRef()

        assert geom.GetGeometryName() == 'POINT', \
            'block placement is expected to be POINT.'

        ds = None

def test_ogr_dwg_3():

    with gdaltest.config_option('DWG_INLINE_BLOCKS', 'FALSE'):

        ds = gdal.OpenEx('data/cad/Building_A_Floor_0_Mapwize.dwg', allowed_drivers=['DWG'])

        assert ds is not None

        layer = ds.GetLayer( 'entities' )
        layer.ResetReading()
        layer.SetAttributeFilter("layer = 'RoomsID'")

        dwg_occupants = set()
        for feature in layer:
            data = json.loads( feature.GetField("blockattributes") )
            dwg_occupants.add( data['OCCUPANT'] )

        occupants = {'Mederic', 'Everybody', 'Mathieu', 'Alex, Manon', 'Perrine', 'Maxime, Cyprien, Etienne, Thierry, Kevin'}

        assert occupants == dwg_occupants, \
            ('block attribute OCCUPANT for features in layer RoomsID is expected to be %s.' % str(occupants) )

        ds = None

