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

###############################################################################
# Test OGRSQL with geometries

def ogr_rfc41_6():

    ds = ogr.GetDriverByName('memory').CreateDataSource('')
    sr = osr.SpatialReference()
    lyr = ds.CreateLayer('poly', geom_type = ogr.wkbPolygon, srs = sr)
    lyr.GetLayerDefn().GetGeomFieldDefn(0).SetName('geomfield')
    lyr.CreateField(ogr.FieldDefn('intfield', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('wkt', ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('intfield', 1)
    feat.SetField('wkt', 'POINT (0 0)')
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POLYGON EMPTY'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    feat = None

    # Test implicit geometry column (since poly has one single geometry column)
    # then explicit geometry column
    for sql in [ 'SELECT intfield FROM poly',
                 'SELECT * FROM poly',
                 'SELECT intfield, geomfield FROM poly',
                 'SELECT geomfield, intfield FROM poly' ]:
        sql_lyr = ds.ExecuteSQL(sql)
        if sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() != ogr.wkbPolygon:
            gdaltest.post_reason('fail')
            return 'fail'
        if sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetSpatialRef() is None:
            gdaltest.post_reason('fail')
            return 'fail'
        feat = sql_lyr.GetNextFeature()
        if feat.GetField('intfield') != 1:
            gdaltest.post_reason('fail')
            return 'fail'
        if feat.GetGeomFieldRef('geomfield') is None:
            gdaltest.post_reason('fail')
            return 'fail'
        feat = sql_lyr.GetNextFeature()
        if feat.GetGeomFieldRef('geomfield') is not None:
            gdaltest.post_reason('fail')
            return 'fail'
        feat = None
        ds.ReleaseResultSet(sql_lyr)

    # Test CAST(geometry_field AS GEOMETRY)
    sql_lyr = ds.ExecuteSQL('SELECT CAST(geomfield AS GEOMETRY) AS mygeom FROM poly WHERE CAST(geomfield AS GEOMETRY) IS NOT NULL')
    if sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() != ogr.wkbUnknown:
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetSpatialRef() is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeomFieldRef('mygeom') is None:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test CAST(xxx AS GEOMETRY(POLYGON))
    sql_lyr = ds.ExecuteSQL('SELECT CAST(geomfield AS GEOMETRY(POLYGON)) AS mygeom FROM poly WHERE CAST(geomfield AS GEOMETRY(POLYGON)) IS NOT NULL')
    if sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() != ogr.wkbPolygon:
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetSpatialRef() is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeomFieldRef('mygeom') is None:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test CAST(xxx AS GEOMETRY(POLYGON,4326))
    sql_lyr = ds.ExecuteSQL('SELECT CAST(geomfield AS GEOMETRY(POLYGON,4326)) AS mygeom FROM poly WHERE CAST(geomfield AS GEOMETRY(POLYGON,4326)) IS NOT NULL')
    if sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() != ogr.wkbPolygon:
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetSpatialRef().ExportToWkt().find('4326') < 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeomFieldRef('mygeom') is None:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test CAST(a_multipolygon AS GEOMETRY(POLYGON))
    sql_lyr = ds.ExecuteSQL("SELECT CAST('MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)))' AS GEOMETRY(POLYGON)) AS mygeom FROM poly")
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeomFieldRef('mygeom').ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))':
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test CAST(a_polygon AS GEOMETRY(MULTIPOLYGON))
    sql_lyr = ds.ExecuteSQL("SELECT CAST('POLYGON ((0 0,0 1,1 1,1 0,0 0))' AS GEOMETRY(MULTIPOLYGON)) AS mygeom FROM poly")
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeomFieldRef('mygeom').ExportToWkt() != 'MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)))':
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test CAST(a_multilinestring AS GEOMETRY(LINESTRING))
    sql_lyr = ds.ExecuteSQL("SELECT CAST('MULTILINESTRING ((0 0,0 1,1 1,1 0,0 0))' AS GEOMETRY(LINESTRING)) AS mygeom FROM poly")
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeomFieldRef('mygeom').ExportToWkt() != 'LINESTRING (0 0,0 1,1 1,1 0,0 0)':
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test CAST(a_linestring AS GEOMETRY(MULTILINESTRING))
    sql_lyr = ds.ExecuteSQL("SELECT CAST('LINESTRING (0 0,0 1,1 1,1 0,0 0)' AS GEOMETRY(MULTILINESTRING)) AS mygeom FROM poly")
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeomFieldRef('mygeom').ExportToWkt() != 'MULTILINESTRING ((0 0,0 1,1 1,1 0,0 0))':
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test expression with cast CHARACTER <--> GEOMETRY
    sql_lyr = ds.ExecuteSQL('SELECT CAST(CAST(geomfield AS CHARACTER) AS GEOMETRY) AS mygeom, intfield FROM poly')
    if sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() != ogr.wkbUnknown:
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetSpatialRef() is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = sql_lyr.GetNextFeature()
    if feat.GetField('intfield') != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if feat.GetGeomFieldRef('mygeom') is None:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test CAST(NULL AS GEOMETRY)
    sql_lyr = ds.ExecuteSQL('SELECT CAST(NULL AS GEOMETRY) FROM poly')
    if sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() != ogr.wkbUnknown:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeomFieldRef('') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test CAST(stringfield AS GEOMETRY)
    sql_lyr = ds.ExecuteSQL('SELECT CAST(wkt AS GEOMETRY) FROM poly')
    if sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() != ogr.wkbUnknown:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeomFieldRef('wkt').ExportToWkt() != 'POINT (0 0)':
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test COUNT(geometry)
    sql_lyr = ds.ExecuteSQL('SELECT COUNT(geomfield) FROM poly')
    feat = sql_lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if feat.GetField(0) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    wrong_sql_list = [
        ( 'SELECT DISTINCT geomfield FROM poly', 'SELECT DISTINCT on a geometry not supported' ),
        ( 'SELECT COUNT(DISTINCT geomfield) FROM poly', 'SELECT COUNT DISTINCT on a geometry not supported' ),
        ( 'SELECT MAX(geomfield) FROM poly', 'Use of field function MAX() on geometry field' ),
        ( 'SELECT CAST(5 AS GEOMETRY) FROM poly', 'Cannot cast integer to geometry'),
        ( 'SELECT CAST(geomfield AS integer) FROM poly', 'Cannot cast geometry to integer' ),
        ( 'SELECT CAST(geomfield AS GEOMETRY(2)) FROM poly', 'First argument of CAST operator should be an geometry type identifier'),
        ( 'SELECT CAST(geomfield AS GEOMETRY(UNSUPPORTED_TYPE)) FROM poly', 'SQL Expression Parsing Error: syntax error' ),
        ( 'SELECT CAST(geomfield AS GEOMETRY(UNSUPPORTED_TYPE,5)) FROM poly', 'SQL Expression Parsing Error: syntax error' ),
    ]

    for (sql, error_msg) in wrong_sql_list:
        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        sql_lyr = ds.ExecuteSQL(sql)
        gdal.PopErrorHandler()
        if gdal.GetLastErrorMsg().find(error_msg) != 0:
            gdaltest.post_reason('fail')
            print('For %s, expected error %s, got %s' % (sql, error_msg, gdal.GetLastErrorMsg()))
            return 'fail'
        if sql_lyr is not None:
            gdaltest.post_reason('fail')
            return 'fail'

    # Test invalid expressions with geometry
    for sql in [ "SELECT geomfield + 'a' FROM poly",
                 "SELECT geomfield * 'a' FROM poly",
                 "SELECT geomfield + 'a' FROM poly",
                 "SELECT geomfield - 'a' FROM poly",
                 "SELECT geomfield % 'a' FROM poly",
                 "SELECT CONCAT(geomfield, 'a') FROM poly",
                 "SELECT SUBSTR(geomfield, 0, 1) FROM poly",
                 "SELECT * FROM poly WHERE geomfield = CAST('POINT EMPTY' AS GEOMETRY)",
                 "SELECT * FROM poly WHERE geomfield LIKE 'a'",
                 "SELECT * FROM poly WHERE geomfield IN( 'a' )"]:
        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        sql_lyr = ds.ExecuteSQL(sql)
        gdal.PopErrorHandler()
        if gdal.GetLastErrorMsg().find('Cannot use geometry field in this operation') != 0:
            gdaltest.post_reason('fail')
            print(gdal.GetLastErrorMsg())
            return 'fail'
        if sql_lyr is not None:
            gdaltest.post_reason('fail')
            return 'fail'

    # Test expression with geometry in WHERE
    sql_lyr = ds.ExecuteSQL('SELECT * FROM poly WHERE geomfield IS NOT NULL')
    feat = sql_lyr.GetNextFeature()
    if feat.GetField('intfield') != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = sql_lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None
    ds.ReleaseResultSet(sql_lyr)
    
    sql_lyr = ds.ExecuteSQL('SELECT * FROM poly WHERE geomfield IS NULL')
    feat = sql_lyr.GetNextFeature()
    if feat.IsFieldSet(0):
        gdaltest.post_reason('fail')
        return 'fail'
    feat = sql_lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL("SELECT * FROM poly WHERE CAST(geomfield AS CHARACTER) = 'POLYGON EMPTY'")
    feat = sql_lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = sql_lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL('SELECT count(*) FROM poly WHERE geomfield IS NULL')
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None
    ds.ReleaseResultSet(sql_lyr)
    
    sql_lyr = ds.ExecuteSQL('SELECT count(*) FROM poly WHERE geomfield IS NOT NULL')
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test spatial filter
    feat = lyr.GetFeature(0)
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(1 2)'))
    lyr.SetFeature(feat)
    feat = None

    lyr.DeleteFeature(1)

    sql_lyr = ds.ExecuteSQL("SELECT * FROM poly")
    sql_lyr.SetSpatialFilterRect(0, 0, 0, 0)
    feat = sql_lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None

    sql_lyr.SetSpatialFilterRect(0, 1,2,1,2)
    feat = sql_lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None

    # Test invalid spatial filter index
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr.SetSpatialFilterRect(2, 0,0,0,0)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    # Test invalid geometry field index
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    got_extent = sql_lyr.GetExtent(geom_field = 2)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    ds.ReleaseResultSet(sql_lyr)

    # Test querying several geometry fields
    sql_lyr = ds.ExecuteSQL('SELECT geomfield as geom1, geomfield as geom2 FROM poly')
    feat = sql_lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if feat.GetGeomFieldRef('geom1') is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if feat.GetGeomFieldRef('geom2') is None:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test querying a layer with several geometry fields
    lyr.CreateGeomField(ogr.GeomFieldDefn('secondarygeom', ogr.wkbPoint))
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    feat.SetGeomField('secondarygeom', ogr.CreateGeometryFromWkt('POINT (10 100)'))
    lyr.SetFeature(feat)
    feat = None

    for sql in [ 'SELECT * FROM poly',
                 'SELECT geomfield, secondarygeom FROM poly',
                 'SELECT secondarygeom, geomfield FROM poly' ]:
        sql_lyr = ds.ExecuteSQL(sql)
        feat = sql_lyr.GetNextFeature()
        if feat.GetGeomFieldRef('geomfield').ExportToWkt() != 'POINT (1 2)':
            gdaltest.post_reason('fail')
            return 'fail'
        if feat.GetGeomFieldRef('secondarygeom').ExportToWkt() != 'POINT (10 100)':
            gdaltest.post_reason('fail')
            return 'fail'
        feat = None
        ds.ReleaseResultSet(sql_lyr)

    # Check that we don't get an implicit geometry field
    sql_lyr = ds.ExecuteSQL('SELECT intfield FROM poly')
    if sql_lyr.GetLayerDefn().GetGeomFieldCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    # Check GetExtent() and SetSpatialFilter()
    sql_lyr = ds.ExecuteSQL('SELECT * FROM poly')
    if sql_lyr.GetExtent(geom_field = 0) != (1.0, 1.0, 2.0, 2.0):
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetExtent(geom_field = 1) != (10.0, 10.0, 100.0, 100.0):
        gdaltest.post_reason('fail')
        return 'fail'
    sql_lyr.SetSpatialFilterRect(0,0.5,1.5,1.5,2.5)
    if sql_lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    sql_lyr.SetSpatialFilterRect(0,0,0,0.5,0.5)
    if sql_lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    sql_lyr.SetSpatialFilterRect(1,9,99,11,101)
    if sql_lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    sql_lyr.SetSpatialFilterRect(1,0,0,0.5,0.5)
    if sql_lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    ds = None

    return 'success'

###############################################################################
# Test crazy OGRSQL

def ogr_rfc41_7():

    ds = ogr.Open('data')
    sql = "select eas_id, \"\" as geom1, cast(null as geometry) as geom2, " + \
          "'a', cast('POINT(3 4)' as geometry) as geom3, fid, \"\" as geom4, "+\
          "'c', p.eas_id, cast(area as integer) as area_int, \"\", area from " +\
          "poly join \"data\".poly p on poly.eas_id = p.eas_id"
    sql_lyr = ds.ExecuteSQL(sql)
    feat = sql_lyr.GetNextFeature()
    if feat.eas_id != 168 or \
       feat.FIELD_2 != 'a' or \
       feat.fid != 0 or \
       feat.FIELD_4 != 'c' or \
       feat['p.eas_id'] != 168 or \
       feat.area_int != 215229 or \
       abs(feat.area - 215229.266) > 1e-5 or \
       feat.geom1.GetGeometryType() != ogr.wkbPolygon or \
       feat.geom2 != None or \
       feat.geom3.GetGeometryType() != ogr.wkbPoint or \
       feat.geom4.GetGeometryType() != ogr.wkbPolygon or \
       feat[''].GetGeometryType() != ogr.wkbPolygon:
        feat.DumpReadable()
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    ds = None

    return 'success'

###############################################################################
# Test SQLite dialect

def ogr_rfc41_8():

    import ogr_sql_sqlite
    if not ogr_sql_sqlite.ogr_sql_sqlite_available():
        return 'skip'

    ds = ogr.GetDriverByName('memory').CreateDataSource('')
    lyr = ds.CreateLayer('mytable', geom_type = ogr.wkbPolygon)
    lyr.GetLayerDefn().GetGeomFieldDefn(0).SetName('geomfield')
    gfld_defn = ogr.GeomFieldDefn('geomfield2', ogr.wkbPoint25D)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    gfld_defn.SetSpatialRef(sr)
    lyr.CreateGeomField(gfld_defn)

    # Check that we get the geometry columns, even with no features
    sql_lyr = ds.ExecuteSQL('SELECT * FROM mytable', dialect = 'SQLite')
    if sql_lyr.GetLayerDefn().GetGeomFieldCount() != 2:
        print(sql_lyr.GetLayerDefn().GetGeomFieldCount())
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() != ogr.wkbPolygon:
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetSpatialRef() is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetLayerDefn().GetGeomFieldDefn(1).GetType() != ogr.wkbPoint25D:
        gdaltest.post_reason('fail')
        return 'fail'
    srs = sql_lyr.GetLayerDefn().GetGeomFieldDefn(1).GetSpatialRef()
    if srs.GetAuthorityCode(None) != '4326':
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    # Test INSERT INTO request
    ds.ExecuteSQL("INSERT INTO mytable (geomfield, geomfield2) VALUES (" +
                  "GeomFromText('POLYGON ((0 0,0 1,1 1,1 0,0 0))'), " +
                  "GeomFromText('POINT Z(0 1 2)') )", dialect = 'SQLite')

    # Check output
    sql_lyr = ds.ExecuteSQL('SELECT geomfield2, geomfield FROM mytable', dialect = 'SQLite')
    feat = sql_lyr.GetNextFeature()
    geom = feat.GetGeomFieldRef('geomfield')
    if geom.ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))':
        feat.DumpReadable()
        gdaltest.post_reason('fail')
        return 'fail'
    geom = feat.GetGeomFieldRef('geomfield2')
    if geom.ExportToWkt() != 'POINT (0 1 2)':
        feat.DumpReadable()
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test UPDATE
    ds.ExecuteSQL("UPDATE mytable SET geomfield2 = " +
                  "GeomFromText('POINT Z(3 4 5)')", dialect = 'SQLite')

    # Check output
    sql_lyr = ds.ExecuteSQL('SELECT geomfield2, geomfield FROM mytable', dialect = 'SQLite')
    feat = sql_lyr.GetNextFeature()
    geom = feat.GetGeomFieldRef('geomfield')
    if geom.ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))':
        feat.DumpReadable()
        gdaltest.post_reason('fail')
        return 'fail'
    geom = feat.GetGeomFieldRef('geomfield2')
    if geom.ExportToWkt() != 'POINT (3 4 5)':
        feat.DumpReadable()
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    return 'success'

gdaltest_list = [
    ogr_rfc41_1,
    ogr_rfc41_2,
    ogr_rfc41_3,
    ogr_rfc41_4,
    ogr_rfc41_5,
    ogr_rfc41_6,
    ogr_rfc41_7,
    ogr_rfc41_8,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_rfc41' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
