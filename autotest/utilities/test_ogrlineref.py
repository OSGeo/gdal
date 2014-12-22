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
import shutil

sys.path.append( '../pymod' )
sys.path.append( '../ogr' )

from osgeo import gdal
from osgeo import ogr
from osgeo import osr
import gdaltest
import ogrtest
import test_cli_utilities

###############################################################################
# create test

def test_ogrlineref_1():
    if ogrtest.have_geos() is 0 or test_cli_utilities.get_ogrlineref_path() is None:
        return 'skip'

    try:
        os.stat('tmp/parts.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/parts.shp')
    except:
        pass

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrlineref_path() + ' -create -l data/path.shp -p data/mstones.shp -pm pos -o tmp/parts.shp -s 1000')
    if not (err is None or err == '') :
        gdaltest.post_reason('got error/warning')
        print(err)
        return 'fail'

    ds = ogr.Open('tmp/parts.shp')
    if ds is None or ds.GetLayer(0).GetFeatureCount() != 9:
        return 'fail'

    ds.Destroy()
    
#    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/parts.shp')

    return 'success'

###############################################################################
# get_pos test

def test_ogrlineref_2():
    if ogrtest.have_geos() is 0 or test_cli_utilities.get_ogrlineref_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrlineref_path() + ' -get_pos -r tmp/parts.shp -x -1.4345 -y 51.9497 -quiet')

    if ret.strip() != "15977.724709":
        return 'fail'

    return 'success'

###############################################################################
# get_coord test

def test_ogrlineref_3():
    if ogrtest.have_geos() is 0 or test_cli_utilities.get_ogrlineref_path() is None:
        return 'skip'
 
    ret = gdaltest.runexternal(test_cli_utilities.get_ogrlineref_path() + ' -get_coord -r tmp/parts.shp -m 15977.724709 -quiet')

    if ret.strip() != "-1.435097,51.950080,0.000000":
        return 'fail'
        
    return 'success'

###############################################################################
# get_subline test

def test_ogrlineref_4():
    if ogrtest.have_geos() is 0 or test_cli_utilities.get_ogrlineref_path() is None:
        return 'skip'

    try:
        os.stat('tmp/subline.shp')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/subline.shp')
    except:
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogrlineref_path() + ' -get_subline -r tmp/parts.shp -mb 13300 -me 17400 -o tmp/subline.shp')
    
    ds = ogr.Open('tmp/subline.shp')
    if ds is None or ds.GetLayer(0).GetFeatureCount() != 1:
        return 'fail'
    ds.Destroy()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/subline.shp')

    return 'success'

def test_ogrlineref_cleanup():
    if ogrtest.have_geos() is 0 or test_cli_utilities.get_ogrlineref_path() is None:
        return 'skip'
    try:
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/parts.shp')
    except:
        pass
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

