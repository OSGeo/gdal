#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  FGDB driver testing.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at mines dash paris dot org>
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
import shutil

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
import ogr
import osr

###############################################################################
# Test if driver is available

def ogr_fgdb_init():

    ogrtest.fgdb_drv = None

    try:
        ogrtest.fgdb_drv = ogr.GetDriverByName('FileGDB')
    except:
        pass

    if ogrtest.fgdb_drv is None:
        return 'skip'

    return 'success'

###############################################################################
# Write and read back

def ogr_fgdb_1():
    if ogrtest.fgdb_drv is None:
        return 'skip'

    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")

    ds = ogrtest.fgdb_drv.CreateDataSource("tmp/test.gdb")

    datalist = [ [ "point", ogr.wkbPoint, "POINT(1 2)", "POINT (1.0 2.0)" ],
                 [ "multipoint", ogr.wkbMultiPoint, "MULTIPOINT(1 2,3 4)", "MULTIPOINT (1.0 2.0,3.0 4.0)" ],
                 [ "linestring", ogr.wkbLineString, "LINESTRING(1 2,3 4)", "MULTILINESTRING ((1.0 2.0,3.0 4.0))" ],
                 [ "multilinestring", ogr.wkbMultiLineString, "MULTILINESTRING ((1.0 2.0,3.0 4.0))", "MULTILINESTRING ((1.0 2.0,3.0 4.0))" ],
                 [ "polygon", ogr.wkbPolygon, "POLYGON((0 0,0 1,1 1,1 0,0 0))", "MULTIPOLYGON (((0.0 0.0,0.0 1.0,1.0 1.0,1.0 0.0,0.0 0.0)))" ],
                 [ "multipolygon", ogr.wkbMultiPolygon, "MULTIPOLYGON (((0.0 0.0,0.0 1.0,1.0 1.0,1.0 0.0,0.0 0.0)))", "MULTIPOLYGON (((0.0 0.0,0.0 1.0,1.0 1.0,1.0 0.0,0.0 0.0)))" ],
                 #[ "point25D", ogr.wkbPoint25D, "POINT(1 2 3)", "POINT (1.0 2.0 3.0)" ],
                 #[ "multipoint25D", ogr.wkbMultiPoint25D, "MULTIPOINT(1 2 -10,3 4 -20)", "MULTIPOINT (1.0 2.0 -10.0,3.0 4.0 -20.0)" ],
                 #[ "linestring25D", ogr.wkbLineString25D, "LINESTRING(1 2 -10,3 4 -20)", "MULTILINESTRING ((1.0 2.0 -10.0,3.0 4.0 -20.0))" ],
               ]

    for data in datalist:
        lyr = ds.CreateLayer(data[0], geom_type = data[1], srs = srs)
        lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
        lyr.CreateField(ogr.FieldDefn("int", ogr.OFTInteger))
        lyr.CreateField(ogr.FieldDefn("real", ogr.OFTReal))
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeometry(ogr.CreateGeometryFromWkt(data[2]))
        feat.SetField("str", "foo_\xc3\xa9")
        feat.SetField("int", 123)
        feat.SetField("real", 4.56)
        lyr.CreateFeature(feat)

    for data in datalist:
        lyr = ds.GetLayerByName(data[0])
        feat = lyr.GetNextFeature()
        if feat.GetGeometryRef().ExportToWkt() != data[3]:
            feat.DumpReadable()
            return 'fail'

    ds = None

    return 'success'

###############################################################################
# Cleanup

def ogr_fgdb_cleanup():
    if ogrtest.fgdb_drv is None:
        return 'skip'

    shutil.rmtree("tmp/test.gdb")

    return 'success'

gdaltest_list = [ 
    ogr_fgdb_init,
    ogr_fgdb_1,
    ogr_fgdb_cleanup,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_fgdb' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()



