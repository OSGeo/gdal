#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  CartoDB driver testing.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2013, Even Rouault <even dot rouault at mines dash paris dot org>
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
import string

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import gdal
from osgeo import ogr

###############################################################################
# Test if driver is available

def ogr_cartodb_init():

    ogrtest.cartodb_drv = None

    try:
        ogrtest.cartodb_drv = ogr.GetDriverByName('CartoDB')
    except:
        pass

    if ogrtest.cartodb_drv is None:
        return 'skip'

    ogrtest.cartodb_test_server = 'https://gdalautotest2.cartodb.com'
   
    if gdaltest.gdalurlopen(ogrtest.cartodb_test_server) is None:
        print('cannot open %s' % ogrtest.cartodb_test_server)
        ogrtest.cartodb_drv = None
        return 'skip'

    return 'success'

###############################################################################
#  Run test_ogrsf

def ogr_cartodb_test_ogrsf():
    if ogrtest.cartodb_drv is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro "CARTODB:gdalautotest2 tables=tm_world_borders_simpl_0_3"')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

gdaltest_list = [ 
    ogr_cartodb_init,
    ogr_cartodb_test_ogrsf
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_cartodb' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
