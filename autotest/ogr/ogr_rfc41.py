#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test RFC41
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
from osgeo import ogr
from osgeo import osr
from osgeo import gdal

###############################################################################
# Test OGRGeomFieldDefn class

def ogr_rfc41_1():

    gfld_defn = ogr.GeomFieldDefn()

    # Check default values
    if gfld_defn.GetName() != '':
        gdaltest.post_reason('fail')
        return 'fail'
    if gfld_defn.GetType() != ogr.wkbUnknown:
        gdaltest.post_reason('fail')
        return 'fail'
    if gfld_defn.GetSpatialRef() != None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gfld_defn.IsIgnored() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test SetName() / GetName()
    gfld_defn.SetName('foo')
    if gfld_defn.GetName() != 'foo':
        gdaltest.post_reason('fail')
        return 'fail'

    # Test SetType() / GetType()
    gfld_defn.SetType(ogr.wkbPoint)
    if gfld_defn.GetType() != ogr.wkbPoint:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test SetSpatialRef() / GetSpatialRef()
    sr = osr.SpatialReference()
    gfld_defn.SetSpatialRef(sr)
    got_sr = gfld_defn.GetSpatialRef()
    if got_sr.IsSame(sr) == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gfld_defn.SetSpatialRef(None)
    if gfld_defn.GetSpatialRef() != None:
        gdaltest.post_reason('fail')
        return 'fail'

    gfld_defn.SetSpatialRef(sr)

    # Test SetIgnored() / IsIgnored()
    gfld_defn.SetIgnored(1)
    if gfld_defn.IsIgnored() != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test setting invalid value
    old_val = gfld_defn.GetType()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    gfld_defn.SetType(-3)
    gdal.PopErrorHandler()
    if gfld_defn.GetType() != old_val:
        gdaltest.post_reason('fail')
        return 'fail'

    gfld_defn = None

    return 'success'


###############################################################################
# Test OGRFeatureDefn methods related to OGRGeomFieldDefn class

def ogr_rfc41_2():

    # Check implicit geometry field creation
    feature_defn = ogr.FeatureDefn()
    if feature_defn.GetGeomFieldCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature_defn.GetGeomType() != ogr.wkbUnknown:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test IsSame()
    if feature_defn.IsSame(feature_defn) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    other_feature_defn = ogr.FeatureDefn()
    if feature_defn.IsSame(other_feature_defn) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    other_feature_defn.GetGeomFieldDefn(0).SetSpatialRef(osr.SpatialReference())
    if feature_defn.IsSame(other_feature_defn) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feature_defn.GetGeomFieldDefn(0).SetSpatialRef(osr.SpatialReference())
    if feature_defn.IsSame(other_feature_defn) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    other_feature_defn.GetGeomFieldDefn(0).SetSpatialRef(None)
    if feature_defn.IsSame(other_feature_defn) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    feature_defn = None
    feature_defn = ogr.FeatureDefn()

    # Check changing geometry type
    feature_defn.SetGeomType(ogr.wkbPoint)
    if feature_defn.GetGeomType() != ogr.wkbPoint:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature_defn.GetGeomFieldDefn(0).GetType() != ogr.wkbPoint:
        gdaltest.post_reason('fail')
        return 'fail'

    # Check setting to wkbNone and implicitely destroying the field
    for i in range(2):
        feature_defn.SetGeomType(ogr.wkbNone)
        if feature_defn.GetGeomFieldCount() != 0:
            gdaltest.post_reason('fail')
            return 'fail'
        if feature_defn.GetGeomType() != ogr.wkbNone:
            gdaltest.post_reason('fail')
            return 'fail'

    # Recreate the field
    for t in [ ogr.wkbPoint, ogr.wkbLineString ]:
        feature_defn.SetGeomType(t)
        if feature_defn.GetGeomFieldCount() != 1:
            gdaltest.post_reason('fail')
            return 'fail'
        if feature_defn.GetGeomType() != t:
            gdaltest.post_reason('fail')
            return 'fail'
        if feature_defn.GetGeomFieldDefn(0).GetType() != t:
            gdaltest.post_reason('fail')
            return 'fail'

    # Test setting invalid value
    old_val = feature_defn.GetGeomType()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    feature_defn.SetGeomType(-3)
    gdal.PopErrorHandler()
    if feature_defn.GetGeomType() != old_val:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test SetIgnored() / IsIgnored()
    if feature_defn.IsGeometryIgnored() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature_defn.GetGeomFieldDefn(0).IsIgnored() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feature_defn.SetGeometryIgnored(1)
    if feature_defn.IsGeometryIgnored() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature_defn.GetGeomFieldDefn(0).IsIgnored() != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test wrong index values for GetGeomFieldDefn()
    for idx in [-1, 1]:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ret = feature_defn.GetGeomFieldDefn(idx)
        gdal.PopErrorHandler()
        if ret != None:
            gdaltest.post_reason('fail')
            return 'fail'

    # Test GetGeomFieldIndex()
    if feature_defn.GetGeomFieldIndex("") != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature_defn.GetGeomFieldIndex("invalid") != -1:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test AddGeomFieldDefn()
    gfld_defn = ogr.GeomFieldDefn('polygon_field', ogr.wkbPolygon)
    feature_defn.AddGeomFieldDefn(gfld_defn)
    if feature_defn.GetGeomFieldCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature_defn.GetGeomFieldIndex("polygon_field") != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature_defn.GetGeomFieldDefn(1).GetName() != 'polygon_field':
        gdaltest.post_reason('fail')
        return 'fail'

    # Test DeleteGeomFieldDefn() : error cases
    if feature_defn.DeleteGeomFieldDefn(-1) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature_defn.DeleteGeomFieldDefn(2) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature_defn.GetGeomFieldCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test DeleteGeomFieldDefn() : valid cases
    if feature_defn.DeleteGeomFieldDefn(0) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature_defn.GetGeomFieldCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature_defn.GetGeomFieldIndex("polygon_field") != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    if feature_defn.DeleteGeomFieldDefn(0) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature_defn.GetGeomFieldCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    if feature_defn.IsSame(feature_defn) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature_defn.IsSame(ogr.FeatureDefn()) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    feature_defn = None

    return 'success'

###############################################################################
# Test OGRFeature methods

def ogr_rfc41_3():

    # Test with just one geometry field
    feature_defn = ogr.FeatureDefn()
    feature = ogr.Feature(feature_defn)
    if feature.GetGeomFieldCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature.GetGeomFieldDefnRef(0).GetName() != '':
        gdaltest.post_reason('fail')
        return 'fail'
    if feature.GetGeomFieldDefnRef(0).GetType() != ogr.wkbUnknown:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature.GetGeomFieldIndex('') != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature.GetGeomFieldIndex('non_existing') != -1:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature.GetGeomFieldRef(-1) is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature.GetGeomFieldRef(0) is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature.GetGeomFieldRef(1) is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    feature_clone_without_geom = feature.Clone()
    if not feature.Equal(feature_clone_without_geom):
        gdaltest.post_reason('fail')
        return 'fail'
    if feature.SetGeomField(0, ogr.Geometry(ogr.wkbPoint)) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature.GetGeomFieldRef(0).ExportToWkt() != 'POINT EMPTY':
        gdaltest.post_reason('fail')
        return 'fail'
    if not feature.Equal(feature.Clone()):
        gdaltest.post_reason('fail')
        return 'fail'
    if feature.Equal(feature_clone_without_geom):
        gdaltest.post_reason('fail')
        return 'fail'
    feature_clone_with_other_geom = feature.Clone()
    feature_clone_with_other_geom.SetGeometry(ogr.Geometry(ogr.wkbLineString))
    if feature.Equal(feature_clone_with_other_geom):
        gdaltest.post_reason('fail')
        return 'fail'
    if feature.SetGeomFieldDirectly(-1, None) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature.SetGeomFieldDirectly(0, ogr.Geometry(ogr.wkbLineString)) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature.GetGeomFieldRef(0).ExportToWkt() != 'LINESTRING EMPTY':
        gdaltest.post_reason('fail')
        return 'fail'
    feature_clone_with_geom = feature.Clone()
    if feature.SetGeomFieldDirectly(0, None) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature.GetGeomFieldRef(0) != None:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature.Equal(feature_clone_with_geom):
        gdaltest.post_reason('fail')
        return 'fail'
    feature = None

    # Test one a feature with 0 geometry field
    feature_defn = ogr.FeatureDefn()
    feature_defn.SetGeomType(ogr.wkbNone)
    feature = ogr.Feature(feature_defn)
    if not feature.Equal(feature.Clone()):
        gdaltest.post_reason('fail')
        return 'fail'
    if feature.GetGeomFieldCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    # This used to work before RFC 41, but it no longer will
    if feature.SetGeometry(ogr.Geometry(ogr.wkbPoint)) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature.SetGeomField(0, ogr.Geometry(ogr.wkbPoint)) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature.GetGeometryRef() is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature.GetGeomFieldRef(0) is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature.SetGeometryDirectly(ogr.Geometry(ogr.wkbPoint)) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if feature.SetGeomFieldDirectly(0, ogr.Geometry(ogr.wkbPoint)) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feature = None

    # Test one a feature with several geometry fields
    feature_defn = ogr.FeatureDefn()
    feature_defn.SetGeomType(ogr.wkbNone)
    gfld_defn = ogr.GeomFieldDefn('polygon_field', ogr.wkbPolygon)
    feature_defn.AddGeomFieldDefn(gfld_defn)
    gfld_defn = ogr.GeomFieldDefn('point_field', ogr.wkbPoint)
    feature_defn.AddGeomFieldDefn(gfld_defn)
    feature = ogr.Feature(feature_defn)
    feature.SetGeomField(0, ogr.Geometry(ogr.wkbPolygon))
    feature.SetGeomField(1, ogr.Geometry(ogr.wkbPoint))
    if feature.GetGeomFieldRef(0).ExportToWkt() != 'POLYGON EMPTY':
        gdaltest.post_reason('fail')
        return 'fail'
    if feature.GetGeomFieldRef(1).ExportToWkt() != 'POINT EMPTY':
        gdaltest.post_reason('fail')
        return 'fail'
    if not feature.Equal(feature.Clone()):
        gdaltest.post_reason('fail')
        return 'fail'
    other_feature = ogr.Feature(feature_defn)
    if feature.Equal(other_feature):
        gdaltest.post_reason('fail')
        return 'fail'
    other_feature.SetFrom(feature)
    if not feature.Equal(other_feature):
        gdaltest.post_reason('fail')
        return 'fail'

    # Test that in SetFrom() where target has a single geometry field,
    # we get the first geometry of the source even if we cannot find a 
    # source geometry field with the right name.
    feature_defn_default = ogr.FeatureDefn()
    feature_default = ogr.Feature(feature_defn_default)
    feature_default.SetFrom(feature)
    if feature_default.GetGeomFieldRef(0).ExportToWkt() != 'POLYGON EMPTY':
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test OGRLayer methods

def ogr_rfc41_4():

    ds = ogr.GetDriverByName('memory').CreateDataSource('')
    if ds.TestCapability(ogr.ODsCCreateGeomFieldAfterCreateLayer) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    sr = osr.SpatialReference()
    lyr = ds.CreateLayer('test', geom_type = ogr.wkbPoint, srs = sr)
    if lyr.TestCapability(ogr.OLCCreateGeomField) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetSpatialRef().IsSame(sr) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetGeomFieldDefn(0).GetSpatialRef().IsSame(sr) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr.GetLayerDefn().GetGeomFieldDefn(0).SetName('a_name')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT (1 2)'))
    lyr.CreateFeature(feat)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    if geom.GetSpatialReference().IsSame(sr) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None
    lyr.CreateGeomField(ogr.GeomFieldDefn('another_geom_field', ogr.wkbPolygon))
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    feat.SetGeomField(1, ogr.CreateGeometryFromWkt('POLYGON ((10 10,10 11,11 11,11 10,10 10))'))
    lyr.SetFeature(feat)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    geom = feat.GetGeomFieldRef(0)
    if geom.ExportToWkt() != 'POINT (1 2)':
        gdaltest.post_reason('fail')
        return 'fail'
    geom = feat.GetGeomFieldRef('another_geom_field')
    if geom.ExportToWkt() != 'POLYGON ((10 10,10 11,11 11,11 10,10 10))':
        gdaltest.post_reason('fail')
        return 'fail'

    # Test GetExtent()
    got_extent = lyr.GetExtent(geom_field = 1)
    if got_extent != (10.0, 11.0, 10.0, 11.0):
        gdaltest.post_reason('fail')
        return 'fail'
    # Test invalid geometry field index
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    got_extent = lyr.GetExtent(geom_field = 2)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    # Test SetSpatialFilter()
    lyr.SetSpatialFilter(1, ogr.CreateGeometryFromWkt('POLYGON ((-10 10,-10 11,-11 11,-11 10,-10 10))'))
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr.SetSpatialFilter(1, ogr.CreateGeometryFromWkt('POLYGON ((10 10,10 11,11 11,11 10,10 10))'))
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr.SetSpatialFilterRect(1, 10, 10, 11, 11)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('fail')
        return 'fail'
    # Test invalid spatial filter index
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr.SetSpatialFilterRect(2, 0,0,0,0)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    lyr.SetSpatialFilter(None)
    another_lyr = ds.CopyLayer(lyr, 'dup_test')
    dup_feat = another_lyr.GetNextFeature()
    geom = dup_feat.GetGeomFieldRef('a_name')
    if geom.ExportToWkt() != 'POINT (1 2)':
        gdaltest.post_reason('fail')
        return 'fail'
    geom = dup_feat.GetGeomFieldRef('another_geom_field')
    if geom.ExportToWkt() != 'POLYGON ((10 10,10 11,11 11,11 10,10 10))':
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test Python field accessors facilities

def ogr_rfc41_5():

    feature_defn = ogr.FeatureDefn()
    field_defn = ogr.FieldDefn('strfield', ogr.OFTString)
    feature_defn.AddFieldDefn(field_defn)
    feature_defn.GetGeomFieldDefn(0).SetName('geomfield')

    f = ogr.Feature(feature_defn)

    if f['strfield'] != None:
        gdaltest.post_reason('fail')
        return 'fail'
    if f.strfield != None:
        gdaltest.post_reason('fail')
        return 'fail'

    if f['geomfield'] != None:
        gdaltest.post_reason('fail')
        return 'fail'
    if f.geomfield != None:
        gdaltest.post_reason('fail')
        return 'fail'

    try:
        a = f['inexisting_field']
        gdaltest.post_reason('fail')
        return 'fail'
    except:
        pass
    try:
        a = f.inexisting_field
        gdaltest.post_reason('fail')
        return 'fail'
    except:
        pass

    try:
        f['inexisting_field'] = 'foo'
        gdaltest.post_reason('fail')
        return 'fail'
    except:
        pass

    # This works. Default Python behaviour. Stored in a dictionnary
    f.inexisting_field = 'bar'
    if f.inexisting_field != 'bar':
        gdaltest.post_reason('fail')
        return 'fail'

    f['strfield'] = 'foo'
    if f['strfield'] != 'foo':
        gdaltest.post_reason('fail')
        return 'fail'
    if f.strfield != 'foo':
        gdaltest.post_reason('fail')
        return 'fail'

    f.strfield = 'bar'
    if f['strfield'] != 'bar':
        gdaltest.post_reason('fail')
        return 'fail'
    if f.strfield != 'bar':
        gdaltest.post_reason('fail')
        return 'fail'

    wkt = 'POINT EMPTY'
    f['geomfield'] = ogr.CreateGeometryFromWkt(wkt)
    if f['geomfield'].ExportToWkt() != wkt:
        gdaltest.post_reason('fail')
        return 'fail'
    if f.geomfield.ExportToWkt() != wkt:
        gdaltest.post_reason('fail')
        return 'fail'

    wkt2 = 'POLYGON EMPTY'
    f.geomfield = ogr.CreateGeometryFromWkt(wkt2)
    if f['geomfield'].ExportToWkt() != wkt2:
        gdaltest.post_reason('fail')
        return 'fail'
    if f.geomfield.ExportToWkt() != wkt2:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

gdaltest_list = [
    ogr_rfc41_1,
    ogr_rfc41_2,
    ogr_rfc41_3,
    ogr_rfc41_4,
    ogr_rfc41_5,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_rfc41' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
