#!/usr/bin/env python
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

import sys

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import gdal
from osgeo import ogr
from osgeo import osr


###############################################################################
# Test reading Japanese FGD GML (v4) files
###############################################################################

_fgd_dir = 'data/gml_jpfgd/'


###############################################################################
# Test reading Japanese FGD GML (v4) ElevPt file

def ogr_gml_fgd_1():

    gdaltest.have_gml_fgd_reader = 0

    ### open FGD GML file
    try:
        ds = ogr.Open(_fgd_dir + 'ElevPt.xml')
    except:
        ds = None

    if ds is None:
        if gdal.GetLastErrorMsg().find('Xerces') != -1:
            return 'skip'
        else:
            gdaltest.post_reason( 'failed to open test file.' )
            return 'fail'

    # we have gml reader for fgd
    gdaltest.have_gml_fgd_reader = 1

    # check number of layers
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('Wrong layer count')
        return 'fail'

    lyr = ds.GetLayer(0)

    # check the SRS
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(6668)   # JGD2011
    if not sr.IsSame(lyr.GetSpatialRef()):
        gdaltest.post_reason('Wrong SRS')
        return 'fail'

    # check the first feature
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, 'POINT (34.123456789 133.123456789)'):
        gdaltest.post_reason('Wrong geometry')
        return 'fail'

    if feat.GetField('devDate') != '2015-01-07':
        gdaltest.post_reason('Wrong attribute value')
        return 'fail'

    return 'success'


###############################################################################
# List test cases

gdaltest_list = [
    ogr_gml_fgd_1
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_gml_fgd_read' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
