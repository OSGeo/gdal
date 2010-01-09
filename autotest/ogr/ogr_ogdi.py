#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR OGDI driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at mines dash paris dot org>
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
import ogr
import osr

sys.path.append( '../pymod' )

import gdaltest
import ogrtest

###############################################################################
def ogr_ogdi_1():

    # Skip for Win32 for the moment.
    if os.name == 'nt':
        return 'skip'

    try:
        drv = ogr.GetDriverByName('OGDI')
    except:
        drv = None
   
    if drv is None:
        return 'skip'

    if not gdaltest.download_file('http://freefr.dl.sourceforge.net/project/ogdi/OGDI_Test_Suite/3.1/ogdits-3.1.0.tar.gz', 'ogdits-3.1.0.tar.gz'):
        return 'skip'

    try:
        os.stat('tmp/cache/ogdits-3.1')
    except:
        try:
            wd = os.getcwd()
            os.chdir( 'tmp/cache' )
            os.system('tar xvzf ogdits-3.1.0.tar.gz')
            os.chdir(wd)
            try:
                os.stat('tmp/cache/ogdits-3.1')
            except:
                return 'skip'
        except:
            return 'skip'

    url_name ='gltp:/vrf/' + os.getcwd()+ '/tmp/cache/ogdits-3.1/data/vpf/vm2alv2/texash'

    ds = ogr.Open(url_name)
    if ds is None:
        gdaltest.post_reason('cannot open ' + url_name)
        return 'fail'
    if ds.GetLayerCount() != 57:
        print(ds.GetLayerCount())
        gdaltest.post_reason('did not get expected layer count')
        return 'fail'

    layers = [ ('libref@libref(*)_line', ogr.wkbLineString, 15),
               ('libreft@libref(*)_text', ogr.wkbPoint, 4),
               ('markersp@bnd(*)_point', ogr.wkbPoint, 40),
               ('polbnda@bnd(*)_area', ogr.wkbPolygon, 6)]

    for l in layers:
        lyr = ds.GetLayerByName(l[0])
        if lyr.GetLayerDefn().GetGeomType() != l[1]:
            return 'fail'
        if lyr.GetFeatureCount() != l[2]:
            print(lyr.GetFeatureCount())
            return 'fail'
        #if l[1] != ogr.wkbNone:
        #    if lyr.GetSpatialRef().ExportToWkt().find('WGS 84') == -1:
        #        return 'fail'

    lyr = ds.GetLayerByName('libref@libref(*)_line')
    feat = lyr.GetNextFeature()

    wkt = 'LINESTRING (-97.570159912109375 31.242000579833984,-97.569938659667969 31.242116928100586,-97.562828063964844 31.245765686035156,-97.558868408203125 31.247797012329102,-97.555778503417969 31.249361038208008,-97.55413818359375 31.250171661376953)'
    ref_geom = ogr.CreateGeometryFromWkt(wkt)

    if ogrtest.check_feature_geometry(feat, ref_geom) != 0:
        print(feat.GetGeometryRef().ExportToWkt())
        return 'fail'

    ds.Destroy()

    return 'success'

gdaltest_list = [
    ogr_ogdi_1 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_ogdi' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

