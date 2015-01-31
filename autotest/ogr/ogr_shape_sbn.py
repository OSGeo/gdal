#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test ESRI shapefile spatial index mechanism (.sbn files). This can serve
#           as a test for the functionnality of shapelib's sbnsearch.c
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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

import gdaltest
from osgeo import ogr

###############################################################################
#

def search_all_features(lyr):

    geoms = []
    lyr.SetSpatialFilter(None)
    extents = lyr.GetExtent()
    fc_ref = lyr.GetFeatureCount()

    feat = lyr.GetNextFeature()
    while feat is not None:
        geom = feat.GetGeometryRef()
        geoms.append(geom.Clone())
        feat = lyr.GetNextFeature()

    # Test getting each geom 1 by 1
    for geom in geoms:
        bbox = geom.GetEnvelope()
        lyr.SetSpatialFilterRect(bbox[0], bbox[2], bbox[1], bbox[3])
        lyr.ResetReading()
        found_geom = False
        feat = lyr.GetNextFeature()
        while feat is not None and found_geom is False:
            got_geom = feat.GetGeometryRef()
            if got_geom.Equals(geom) == 1:
                found_geom = True
            else:
                feat = lyr.GetNextFeature()
        if not found_geom:
            gdaltest.post_reason('did not find geometry for %s' % (geom.ExportToWkt()))
            return 'fail'

    # Get all geoms in a single gulp. We do not use exactly the extent bounds, because
    # there is an optimization in the shapefile driver to skip the spatial index in that
    # case.
    eps = 0.0001
    lyr.SetSpatialFilterRect(extents[0]+eps, extents[2]+eps, extents[1]-eps, extents[3]-eps)
    lyr.ResetReading()
    fc = lyr.GetFeatureCount()

    # For point layers, we need a special case since there may be points on the border
    # of the extent
    if lyr.GetGeomType() == ogr.wkbPoint:
        lyr.SetSpatialFilterRect(extents[0], extents[2]+eps, extents[0]+eps, extents[3]-eps)
        lyr.ResetReading()
        fc = fc + lyr.GetFeatureCount()

        lyr.SetSpatialFilterRect(extents[1]-eps, extents[2]+eps, extents[1], extents[3]-eps)
        lyr.ResetReading()
        fc = fc + lyr.GetFeatureCount()

        lyr.SetSpatialFilterRect(extents[0], extents[2], extents[1], extents[2]+eps)
        lyr.ResetReading()
        fc = fc + lyr.GetFeatureCount()

        lyr.SetSpatialFilterRect(extents[0], extents[3]-eps, extents[1], extents[3])
        lyr.ResetReading()
        fc = fc + lyr.GetFeatureCount()

    if fc != fc_ref:
        gdaltest.post_reason('layer %s: expected %d. got %d' % (lyr.GetName(), fc_ref, fc))
        return 'fail'

    return 'success'

###############################################################################
# Test

def ogr_shape_sbn_1():

    if not gdaltest.download_file('http://pubs.usgs.gov/sim/3194/contents/Cochiti_shapefiles.zip', 'Cochiti_shapefiles.zip' ):
        return 'skip'

    try:
        os.stat('tmp/cache/CochitiDamShapeFiles/CochitiBoundary.shp')
    except:
        try:
            gdaltest.unzip( 'tmp/cache', 'tmp/cache/Cochiti_shapefiles.zip')
            try:
                os.stat('tmp/cache/CochitiDamShapeFiles/CochitiBoundary.shp')
            except:
                return 'skip'
        except:
            return 'skip'

    ds = ogr.Open('tmp/cache/CochitiDamShapeFiles')
    for i in range(ds.GetLayerCount()):
        lyr = ds.GetLayer(i)
        ret = search_all_features(lyr)
        if ret != 'success':
            return ret

    return 'success'

gdaltest_list = [
    ogr_shape_sbn_1,
]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_shape_sbn' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

