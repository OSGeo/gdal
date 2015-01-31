#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR EDIGEO driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
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

import os
import sys
from osgeo import ogr

sys.path.append( '../pymod' )

import gdaltest
import ogrtest

###############################################################################
def ogr_edigeo_1():

    filelist = ['E000AB01.THF',
                'EDAB01S1.VEC',
                'EDAB01SE.DIC',
                'EDAB01SE.GEN',
                'EDAB01SE.GEO',
                'EDAB01SE.QAL',
                'EDAB01SE.SCD',
                'EDAB01T1.VEC',
                'EDAB01T2.VEC',
                'EDAB01T3.VEC']
    #base_url = 'http://svn.geotools.org/trunk/modules/unsupported/edigeo/src/test/resources/org/geotools/data/edigeo/test-data/'
    base_url = 'https://raw.githubusercontent.com/geotools/geotools/master/modules/unsupported/edigeo/src/test/resources/org/geotools/data/edigeo/test-data/'

    for filename in filelist:
        if not gdaltest.download_file(base_url + filename, filename):
            return 'skip'

    try:
        for filename in filelist:
            os.stat('tmp/cache/' + filename)
    except:
        return 'skip'

    ds = ogr.Open('tmp/cache/E000AB01.THF')
    if ds.GetLayerCount() != 24:
        print(ds.GetLayerCount())
        return 'fail'

    layers = [ ('BATIMENT_id', ogr.wkbPolygon, 107),
               ('BORNE_id', ogr.wkbPoint, 5),
               ('COMMUNE_id', ogr.wkbPolygon, 1),
               ('LIEUDIT_id', ogr.wkbPolygon, 3),
               ('NUMVOIE_id', ogr.wkbPoint, 43),
               ('PARCELLE_id', ogr.wkbPolygon, 155),
               ('SECTION_id', ogr.wkbPolygon, 1),
               ('SUBDFISC_id', ogr.wkbPolygon, 1),
               ('SUBDSECT_id', ogr.wkbPolygon, 1),
               ('SYMBLIM_id', ogr.wkbPoint, 29),
               ('TLINE_id', ogr.wkbLineString, 134),
               ('TPOINT_id', ogr.wkbPoint, 1),
               ('TRONFLUV_id', ogr.wkbPolygon, 3),
               ('TRONROUTE_id', ogr.wkbPolygon, 1),
               ('TSURF_id', ogr.wkbPolygon, 3),
               ('ZONCOMMUNI_id', ogr.wkbLineString, 15),
               ('ID_S_OBJ_Z_1_2_2', ogr.wkbPoint, 248),
               ]

    for l in layers:
        lyr = ds.GetLayerByName(l[0])
        if lyr.GetLayerDefn().GetGeomType() != l[1]:
            return 'fail'
        if lyr.GetFeatureCount() != l[2]:
            print(lyr.GetFeatureCount())
            return 'fail'
        if l[1] != ogr.wkbNone:
            if lyr.GetSpatialRef().ExportToWkt().find('Lambert_Conformal_Conic_1SP') == -1:
                print(lyr.GetSpatialRef().ExportToWkt())
                return 'fail'

    lyr = ds.GetLayerByName('BORNE_id')
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, 'POINT (877171.28 72489.22)'):
        feat.DumpReadable()
        return 'fail'

    lyr = ds.GetLayerByName('BATIMENT_id')
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, 'POLYGON ((877206.16 71888.82,877193.14 71865.51,877202.95 71860.07,877215.83 71883.5,877206.16 71888.82))'):
        feat.DumpReadable()
        return 'fail'

    lyr = ds.GetLayerByName('ZONCOMMUNI_id')
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, 'LINESTRING (877929.8 71656.39,877922.38 71663.72,877911.48 71669.51,877884.23 71675.64,877783.07 71694.04,877716.31 71706.98,877707.45 71709.71,877702.0 71713.79,877696.89 71719.58,877671.69 71761.82,877607.99 71865.03,877545.32 71959.04,877499.22 72026.82)'):
        feat.DumpReadable()
        return 'fail'

    ds.Destroy()

    return 'success'


gdaltest_list = [
    ogr_edigeo_1 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_edigeo' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

