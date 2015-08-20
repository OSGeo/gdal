#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test RFC 30 (UTF filename handling) support.
# Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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
import sys
from sys import version_info

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
## Try ogr.Open(), Driver.CreateDataSource(), Driver.DeleteDataSource()

def ogr_rfc30_1_internal(filename, layer_name):

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource(filename)
    lyr = ds.CreateLayer('foo')
    ds = None

    ds = ogr.Open(filename)
    if ds is None:
        gdaltest.post_reason('cannot reopen datasource')
        return 'fail'
    lyr = ds.GetLayerByName(layer_name)
    if lyr is None:
        gdaltest.post_reason('cannot find layer')
        return 'fail'
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource(filename)

    return 'success'

def ogr_rfc30_1():

    if version_info >= (3,0,0):
        filename =  '/vsimem/\u00e9.shp'
        layer_name = '\u00e9'
    else:
        # First try with Unicode string
        exec("filename =  u'/vsimem/\u00e9.shp'")
        exec("layer_name = u'\u00e9'.encode( 'utf-8' )") # FIXME? we should perhaps accept Unicode strings for layernames as well

    return ogr_rfc30_1_internal(filename, layer_name)

def ogr_rfc30_1_bis():

    if version_info >= (3,0,0):
        return 'skip'

    filename = None
    layer_name = None
    # Test that it also works with a regular string (non Unicode) with utf8 content on python 2.X
    exec("filename =  u'/vsimem/\u00e9.shp'.encode( 'utf-8' )")
    exec("layer_name = u'\u00e9'.encode( 'utf-8' )") # FIXME? we should perhaps accept Unicode strings for layernames as well

    return ogr_rfc30_1_internal(filename, layer_name)

gdaltest_list = [ ogr_rfc30_1,
                  ogr_rfc30_1_bis
                  ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_rfc30' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

