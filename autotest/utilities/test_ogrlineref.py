#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  ogrlineref testing
# Author:   Dmitry Baryshnikov. polimax@mail.ru
# 
###############################################################################
# Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

sys.path.append( '../pymod' )
sys.path.append( '../ogr' )

from osgeo import ogr
import gdaltest
import ogrtest
import test_cli_utilities

###############################################################################
# create test

def test_ogrlineref_1():
    if not ogrtest.have_geos() or test_cli_utilities.get_ogrlineref_path() is None:
        return 'skip'

    if os.path.exists('tmp/parts.shp'):
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/parts.shp')

    ret, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrlineref_path() + ' -create -l data/path.shp -p data/mstones.shp -pm pos -o tmp/parts.shp -s 1000')
    if err is not None and err != '':
        gdaltest.post_reason('got error/warning: "%s"' % err)
        return 'fail'

    ds = ogr.Open('tmp/parts.shp')
    if ds is None or ds.GetLayer(0).GetFeatureCount() != 9:
        return 'fail'

    return 'success'

###############################################################################
# get_pos test

def test_ogrlineref_2():
    if not ogrtest.have_geos() or test_cli_utilities.get_ogrlineref_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrlineref_path() + ' -get_pos -r tmp/parts.shp -x -1.4345 -y 51.9497 -quiet').strip()

    expected = '15977.724709'
    if ret != expected:
        gdaltest.post_reason('"%s" != %s' % (ret.strip(), expected))
        return 'fail'

    return 'success'

###############################################################################
# get_coord test

def test_ogrlineref_3():
    if not ogrtest.have_geos() or test_cli_utilities.get_ogrlineref_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrlineref_path() + ' -get_coord -r tmp/parts.shp -m 15977.724709 -quiet').strip()

    expected = '-1.435097,51.950080,0.000000'
    if ret != expected:
        gdaltest.post_reason('%s != %s' % (ret.strip(), expected))
        return 'fail'

    return 'success'

###############################################################################
# get_subline test

def test_ogrlineref_4():
    if not ogrtest.have_geos() or test_cli_utilities.get_ogrlineref_path() is None:
        return 'skip'

    if os.path.exists('tmp/subline.shp'):
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/subline.shp')

    gdaltest.runexternal(test_cli_utilities.get_ogrlineref_path() + ' -get_subline -r tmp/parts.shp -mb 13300 -me 17400 -o tmp/subline.shp')

    ds = ogr.Open('tmp/subline.shp')
    if ds is None:
        gdaltest.post_reason('ds is None')
        return 'fail'

    feature_count = ds.GetLayer(0).GetFeatureCount()
    if feature_count != 1:
        gdaltest.post_reason('feature count %d != 1' % feature_count)
        return 'fail'
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/subline.shp')

    return 'success'

def test_ogrlineref_cleanup():
    if not ogrtest.have_geos() or test_cli_utilities.get_ogrlineref_path() is None:
        return 'skip'

    if os.path.exists('tmp/parts.shp'):
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/parts.shp')

    return 'success'

gdaltest_list = [
    test_ogrlineref_1,
    test_ogrlineref_2,
    test_ogrlineref_3,
    test_ogrlineref_4,
    test_ogrlineref_cleanup
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'test_ogrlineref' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
