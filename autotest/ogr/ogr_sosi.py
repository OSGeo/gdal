#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR SOSI driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
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

from osgeo import ogr
from osgeo import gdal

import gdaltest
import pytest

###############################################################################


def test_ogr_sosi_1():

    if ogr.GetDriverByName('SOSI') is None:
        pytest.skip()

    if not gdaltest.download_file('http://trac.osgeo.org/gdal/raw-attachment/ticket/3638/20BygnAnlegg.SOS', '20BygnAnlegg.SOS'):
        pytest.skip()

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/cache/20BygnAnlegg.SOS')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################
# test using no appendFieldsMap


def test_ogr_sosi_2():

    if ogr.GetDriverByName('SOSI') is None:
        pytest.skip()

        ds = gdal.OpenEx('data/sosi/test_duplicate_fields.sos', open_options=[])
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 17
        lyr = ds.GetLayer(1)
        assert lyr.GetFeatureCount() == 1
        f = lyr.GetNextFeature()
        assert f['REINBEITEBRUKERID'] == 'YD'


###############################################################################
# test using simple open_options appendFieldsMap

def test_ogr_sosi_3():

    if ogr.GetDriverByName('SOSI') is None:
        pytest.skip()

        ds = gdal.OpenEx('data/sosi/test_duplicate_fields.sos',
                         open_options=['appendFieldsMap=BEITEBRUKERID&OPPHAV'])
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 17
        lyr = ds.GetLayer(1)
        assert lyr.GetFeatureCount() == 1
        f = lyr.GetNextFeature()
        assert f['REINBEITEBRUKERID'] == 'YD,YG'


###############################################################################
# test using simple open_options appendFieldsMap with semicolumns

def test_ogr_sosi_4():

    if ogr.GetDriverByName('SOSI') is None:
        pytest.skip()

        ds = gdal.OpenEx('data/sosi/test_duplicate_fields.sos',
                         open_options=['appendFieldsMap=BEITEBRUKERID:;&OPPHAV:;'])
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 17
        lyr = ds.GetLayer(1)
        assert lyr.GetFeatureCount() == 1
        f = lyr.GetNextFeature()
        assert f['REINBEITEBRUKERID'] == 'YD;YG'
