#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR SDTS driver functionality.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2008-2009, Even Rouault <even dot rouault at mines-paris dot org>
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
from osgeo import ogr

###############################################################################
# Test reading

def ogr_sdts_1():

    gdaltest.sdts_ds = ogr.Open( 'data/D3607551_rd0s_1_sdts_truncated/TR01CATD.DDF' )

    if gdaltest.sdts_ds is None:
        return 'fail'

    layers = [ ( 'ARDF' , 164, ogr.wkbNone, [ ('ENTITY_LABEL', '1700005') ] ),
               ( 'ARDM' , 21,  ogr.wkbNone, [ ('ROUTE_NUMBER', 'SR 1200') ] ),
               ( 'AHDR' , 1,   ogr.wkbNone, [ ('BANNER', 'USGS-NMD  DLG DATA - CHARACTER FORMAT - 09-29-87 VERSION                ') ] ),
               ( 'NP01' , 4,   ogr.wkbPoint, [ ('RCID', '1') ] ),
               ( 'NA01' , 34,  ogr.wkbPoint, [ ('RCID', '2') ] ),
               ( 'NO01' , 88,  ogr.wkbPoint, [ ('RCID', '1') ] ), 
               ( 'LE01' , 27,  ogr.wkbLineString, [ ('RCID', '1') ]  ),
               ( 'PC01' , 35,  ogr.wkbPolygon, [ ('RCID', '1') ] )
             ]

    for layer in layers:
        lyr = gdaltest.sdts_ds.GetLayerByName( layer[0] )
        if lyr is None:
            gdaltest.post_reason( 'could not get layer %s' % (layer[0]) )
            return 'fail'
        if lyr.GetFeatureCount() != layer[1] :
            gdaltest.post_reason( 'wrong number of features for layer %s : %d. %d were expected ' % (layer[0], lyr.GetFeatureCount(), layer[1]) )
            return 'fail'
        if lyr.GetLayerDefn().GetGeomType() != layer[2]:
            return 'fail'
        feat_read = lyr.GetNextFeature()
        for item in layer[3]:
            if feat_read.GetFieldAsString(item[0]) != item[1]:
                print(layer[0])
                print('"%s"' % (item[1]))
                print('"%s"' % (feat_read.GetField(item[0])))
                return 'fail'

    gdaltest.sdts_ds = None

    return 'success'


###############################################################################
# 

gdaltest_list = [ 
    ogr_sdts_1 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_sdts' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
