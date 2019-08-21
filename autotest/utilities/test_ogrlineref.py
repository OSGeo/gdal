#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  ogrlineref testing
# Author:   Dmitry Baryshnikov. polimax@mail.ru
#
###############################################################################
# Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
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

import sys
import os
import pytest

sys.path.append('../ogr')

from osgeo import ogr
import gdaltest
import ogrtest
import test_cli_utilities

###############################################################################
# create test


def test_ogrlineref_1():
    if not ogrtest.have_geos() or test_cli_utilities.get_ogrlineref_path() is None:
        pytest.skip()

    if os.path.exists('tmp/parts.shp'):
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/parts.shp')

    _, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrlineref_path() + ' -create -l data/path.shp -p data/mstones.shp -pm pos -o tmp/parts.shp -s 1000')
    assert err is None or err == '', ('got error/warning: "%s"' % err)

    ds = ogr.Open('tmp/parts.shp')
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 9

###############################################################################
# get_pos test


def test_ogrlineref_2():
    if not ogrtest.have_geos() or test_cli_utilities.get_ogrlineref_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrlineref_path() + ' -get_pos -r tmp/parts.shp -x -1.4345 -y 51.9497 -quiet').strip()

    expected = '15977.724709'
    assert ret == expected, ('"%s" != %s' % (ret.strip(), expected))

###############################################################################
# get_coord test


def test_ogrlineref_3():
    if not ogrtest.have_geos() or test_cli_utilities.get_ogrlineref_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrlineref_path() + ' -get_coord -r tmp/parts.shp -m 15977.724709 -quiet').strip()

    expected = '-1.435097,51.950080,0.000000'
    assert ret == expected, ('%s != %s' % (ret.strip(), expected))

###############################################################################
# get_subline test


def test_ogrlineref_4():
    if not ogrtest.have_geos() or test_cli_utilities.get_ogrlineref_path() is None:
        pytest.skip()

    if os.path.exists('tmp/subline.shp'):
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/subline.shp')

    gdaltest.runexternal(test_cli_utilities.get_ogrlineref_path() + ' -get_subline -r tmp/parts.shp -mb 13300 -me 17400 -o tmp/subline.shp')

    ds = ogr.Open('tmp/subline.shp')
    assert ds is not None, 'ds is None'

    feature_count = ds.GetLayer(0).GetFeatureCount()
    assert feature_count == 1, ('feature count %d != 1' % feature_count)
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/subline.shp')

###############################################################################
# test kml


def test_ogrlineref_5():
    if not ogrtest.have_geos() or test_cli_utilities.get_ogrlineref_path() is None:
        pytest.skip()

    if os.path.exists('tmp/parts.kml'):
        ogr.GetDriverByName('KML').DeleteDataSource('tmp/parts.kml')

    gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrlineref_path() + ' -create -f "KML" -l data/path.shp -p data/mstones.shp -pm pos -o tmp/parts.kml -s 222')
    if os.path.exists('tmp/parts.kml'):
        return

    pytest.fail()


def test_ogrlineref_cleanup():
    if not ogrtest.have_geos() or test_cli_utilities.get_ogrlineref_path() is None:
        pytest.skip()

    if os.path.exists('tmp/parts.shp'):
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/parts.shp')
    if os.path.exists('tmp/parts.kml'):
        ogr.GetDriverByName('KML').DeleteDataSource('tmp/parts.kml')

    


