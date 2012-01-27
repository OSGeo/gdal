#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test shapefile spatial index mechanism (.qix files). This can serve
#           as a test for the functionnality of shapelib's shptree.c
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
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
import random

sys.path.append( '../pymod' )

import gdaltest
from osgeo import ogr

###############################################################################
#

def check_qix_non_overlapping_geoms(lyr):

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
        feat = lyr.GetNextFeature()
        got_geom = feat.GetGeometryRef()
        if got_geom.Equals(geom) == 0:
            gdaltest.post_reason('expected %s. got %s' % (geom.ExportToWkt(), got_geom.ExportToWkt()))
            return 'fail'

    # Get all geoms in a single gulp. We do not use exactly the extent bounds, because
    # there is an optimization in the shapefile driver to skip the spatial index in that
    # case. That trick can only work with non point geometries of course
    lyr.SetSpatialFilterRect(extents[0]+0.001, extents[2]+0.001, extents[1]-0.001, extents[3]-0.001)
    lyr.ResetReading()
    fc = lyr.GetFeatureCount()
    if fc != fc_ref:
        gdaltest.post_reason('expected %d. got %d' % (fc_ref, fc))
        return 'fail'

    return 'success'

###############################################################################
def build_rectangle_from_point(x, y, radius = 0.1):
    return ogr.CreateGeometryFromWkt('POLYGON((%f %f,%f %f,%f %f,%f %f,%f %f))' % \
        (x-radius,y-radius,x-radius,y+radius,x+radius,y+radius,x+radius,y-radius,x-radius,y-radius))

###############################################################################
# Test geoms on a 10x10 grid

def ogr_shape_qix_1():

    shape_drv = ogr.GetDriverByName('ESRI Shapefile')
    ds = shape_drv.CreateDataSource('/vsimem/ogr_shape_qix.shp')
    lyr = ds.CreateLayer("ogr_shape_qix")

    for x in range(10):
        for y in range(10):
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetGeometry(build_rectangle_from_point(x,y))
            lyr.CreateFeature(feat)
            feat = None

    ds.ExecuteSQL('CREATE SPATIAL INDEX ON ogr_shape_qix')

    ds = None

    ds = ogr.Open('/vsimem/ogr_shape_qix.shp')
    lyr = ds.GetLayer(0)
    ret = check_qix_non_overlapping_geoms(lyr)

    shape_drv.DeleteDataSource('/vsimem/ogr_shape_qix.shp')

    return ret

###############################################################################
# Test geoms on a 100x100 grid

def ogr_shape_qix_2():

    shape_drv = ogr.GetDriverByName('ESRI Shapefile')
    ds = shape_drv.CreateDataSource('/vsimem/ogr_shape_qix.shp')
    lyr = ds.CreateLayer("ogr_shape_qix")

    for x in range(100):
        for y in range(100):
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetGeometry(build_rectangle_from_point(x,y))
            lyr.CreateFeature(feat)
            feat = None

    ds.ExecuteSQL('CREATE SPATIAL INDEX ON ogr_shape_qix')

    ds = None

    ds = ogr.Open('/vsimem/ogr_shape_qix.shp')
    lyr = ds.GetLayer(0)
    ret = check_qix_non_overlapping_geoms(lyr)

    shape_drv.DeleteDataSource('/vsimem/ogr_shape_qix.shp')

    return ret

###############################################################################
# Test 2 separated regions of 10x10 geoms

def ogr_shape_qix_3():

    shape_drv = ogr.GetDriverByName('ESRI Shapefile')
    ds = shape_drv.CreateDataSource('/vsimem/ogr_shape_qix.shp')
    lyr = ds.CreateLayer("ogr_shape_qix")

    for x in range(10):
        for y in range(10):
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetGeometry(build_rectangle_from_point(x,y))
            lyr.CreateFeature(feat)
            feat = None

    for x in range(10):
        for y in range(10):
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetGeometry(build_rectangle_from_point(x+1000,y))
            lyr.CreateFeature(feat)
            feat = None

    ds.ExecuteSQL('CREATE SPATIAL INDEX ON ogr_shape_qix')

    ds = None

    ds = ogr.Open('/vsimem/ogr_shape_qix.shp')
    lyr = ds.GetLayer(0)
    ret = check_qix_non_overlapping_geoms(lyr)

    shape_drv.DeleteDataSource('/vsimem/ogr_shape_qix.shp')

    return ret

###############################################################################
#

def check_qix_random_geoms(lyr):

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
    # case. That trick can only work with non point geometries of course
    lyr.SetSpatialFilterRect(extents[0]+0.001, extents[2]+0.001, extents[1]-0.001, extents[3]-0.001)
    lyr.ResetReading()
    fc = lyr.GetFeatureCount()
    if fc != fc_ref:
        gdaltest.post_reason('expected %d. got %d' % (fc_ref, fc))
        return 'fail'

    return 'success'

###############################################################################
def build_rectangle(x1,y1,x2,y2):
    return ogr.CreateGeometryFromWkt('POLYGON((%f %f,%f %f,%f %f,%f %f,%f %f))' % \
        (x1,y1,x1,y2,x2,y2,x2,y1,x1,y1))

###############################################################################
# Test random geometries

def ogr_shape_qix_4():

    shape_drv = ogr.GetDriverByName('ESRI Shapefile')
    ds = shape_drv.CreateDataSource('/vsimem/ogr_shape_qix.shp')
    lyr = ds.CreateLayer("ogr_shape_qix")

    # The 1000,200,10 figures are such that there are
    # a bit of overlapping between the geometries
    for x in range(1000):
        feat = ogr.Feature(lyr.GetLayerDefn())
        x1 = random.randint(0,200)
        y1 = random.randint(0,200)
        x2 = x1 + random.randint(1,10)
        y2 = y1 + random.randint(1,10)
        feat.SetGeometry(build_rectangle(x1,y1,x2,y2))
        lyr.CreateFeature(feat)
        feat = None


    # And add statistically non overlapping features
    for x in range(1000):
        feat = ogr.Feature(lyr.GetLayerDefn())
        x1 = random.randint(0,10000)
        y1 = random.randint(0,10000)
        x2 = x1 + random.randint(1,10)
        y2 = y1 + random.randint(1,10)
        feat.SetGeometry(build_rectangle(x1,y1,x2,y2))
        lyr.CreateFeature(feat)
        feat = None

    ds.ExecuteSQL('CREATE SPATIAL INDEX ON ogr_shape_qix')

    ret = check_qix_random_geoms(lyr)

    shape_drv.DeleteDataSource('/vsimem/ogr_shape_qix.shp')

    return ret
    
gdaltest_list = [
    ogr_shape_qix_1,
    ogr_shape_qix_2,
    ogr_shape_qix_3,
    ogr_shape_qix_4,
]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_shape_qix' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

