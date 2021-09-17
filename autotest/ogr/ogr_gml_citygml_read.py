#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  GML Reading Driver for Japanese FGD GML v4 testing.
# Author:   Hiroshi Miura <miurahr@linux.com>
#
###############################################################################
# Copyright (c) 2017, Hiroshi Miura <miurahr@linux.com>
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



import ogrtest
from osgeo import gdal
from osgeo import ogr
from osgeo import osr
import pytest


###############################################################################
# Test reading CityGML files
###############################################################################

_citygml_dir = 'data/gml_citygml/'

###############################################################################
# Test reading CityGML of Project PLATEAU

def test_gml_read_compound_crs_lat_long():

    # open CityGML file
    gml = ogr.Open(_citygml_dir + 'compound_crs.gml')

    # check number of layers
    assert gml.GetLayerCount() == 1, 'Wrong layer count'

    lyr = gml.GetLayer(0)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(6668)  # JGD2011
    assert sr.IsSame(lyr.GetSpatialRef(), options = ['IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES']), 'Wrong SRS'

    wkt = 'POLYHEDRALSURFACE Z (((139.812484938717 35.7092130413279 0.15,139.812489071491 35.7091641446533 0.15,139.812444202746 35.7091610722245 0.15,139.812439721473 35.7092112956502 0.15,139.812436111402 35.7092517484017 0.15,139.812481422309 35.7092546406366 0.15,139.812484938717 35.7092130413279 0.15)),((139.812484938717 35.7092130413279 0.15,139.812481422309 35.7092546406366 0.15,139.812481422309 35.7092546406366 12.08,139.812484938717 35.7092130413279 12.08,139.812484938717 35.7092130413279 0.15)),((139.812481422309 35.7092546406366 0.15,139.812436111402 35.7092517484017 0.15,139.812436111402 35.7092517484017 12.08,139.812481422309 35.7092546406366 12.08,139.812481422309 35.7092546406366 0.15)),((139.812436111402 35.7092517484017 0.15,139.812439721473 35.7092112956502 0.15,139.812439721473 35.7092112956502 12.08,139.812436111402 35.7092517484017 12.08,139.812436111402 35.7092517484017 0.15)),((139.812439721473 35.7092112956502 0.15,139.812444202746 35.7091610722245 0.15,139.812444202746 35.7091610722245 12.08,139.812439721473 35.7092112956502 12.08,139.812439721473 35.7092112956502 0.15)),((139.812444202746 35.7091610722245 0.15,139.812489071491 35.7091641446533 0.15,139.812489071491 35.7091641446533 12.08,139.812444202746 35.7091610722245 12.08,139.812444202746 35.7091610722245 0.15)),((139.812489071491 35.7091641446533 0.15,139.812484938717 35.7092130413279 0.15,139.812484938717 35.7092130413279 12.08,139.812489071491 35.7091641446533 12.08,139.812489071491 35.7091641446533 0.15)),((139.812484938717 35.7092130413279 12.08,139.812481422309 35.7092546406366 12.08,139.812436111402 35.7092517484017 12.08,139.812439721473 35.7092112956502 12.08,139.812444202746 35.7091610722245 12.08,139.812489071491 35.7091641446533 12.08,139.812484938717 35.7092130413279 12.08)))'

    # check the first feature
    feat = lyr.GetNextFeature()
    assert not ogrtest.check_feature_geometry(feat, wkt), 'Wrong geometry'
