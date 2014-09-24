#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  GRASS Testing.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2009, Even Rouault <even dot rouault at mines-paris dot org>
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
import sys
from osgeo import ogr

sys.path.append( '../pymod' )

import gdaltest


###############################################################################
# Test if GRASS driver is present

def ogr_grass_1():

    try:
        gdaltest.ogr_grass_drv = ogr.GetDriverByName( 'GRASS' )
    except:
        gdaltest.ogr_grass_drv = None
        return 'skip'

    return 'success'

###############################################################################
# Read 'point' datasource

def ogr_grass_2():

    if gdaltest.ogr_grass_drv is None:
        return 'skip'

    ds = ogr.Open('./data/PERMANENT/vector/point/head')
    if ds is None:
        gdaltest.post_reason('Cannot open datasource')
        return 'fail'

    lyr = ds.GetLayerByName('1')
    if lyr is None:
        gdaltest.post_reason('Cannot find layer')
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetGeometryRef().ExportToWkt() != 'POINT (0 0)':
        print(feat.GetGeometryRef().ExportToWkt())
        return 'fail'

    if feat.GetFieldAsString('name') != 'my point':
        print(feat.GetFieldAsString('name'))
        return 'fail'

    ds = None

    return 'success'


gdaltest_list = [
    ogr_grass_1,
    ogr_grass_2
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'GRASS' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

