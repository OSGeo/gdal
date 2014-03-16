#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic OGR functionality against test shapefiles.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
# 
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################

import os
import sys

sys.path.append( '../pymod' )

import gdaltest
from osgeo import ogr

###############################################################################

def ogr_basic_1():

    gdaltest.ds = ogr.Open( 'data/poly.shp' )

    if gdaltest.ds is not None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test Feature counting.

def ogr_basic_2():

    gdaltest.lyr = gdaltest.ds.GetLayerByName( 'poly' )

    if gdaltest.lyr.GetName() != 'poly':
        return 'fail'
    if gdaltest.lyr.GetGeomType() != ogr.wkbPolygon:
        return 'fail'

    if gdaltest.lyr.GetLayerDefn().GetName() != 'poly':
        return 'fail'
    if gdaltest.lyr.GetLayerDefn().GetGeomType() != ogr.wkbPolygon:
        return 'fail'

    count = gdaltest.lyr.GetFeatureCount()
    if count != 10:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting 10' % count )
        return 'fail'

    # Now actually iterate through counting the features and ensure they agree.
    gdaltest.lyr.ResetReading()

    count2 = 0
    feat = gdaltest.lyr.GetNextFeature()
    while feat is not None:
        count2 = count2 + 1
        feat.Destroy()
        feat = gdaltest.lyr.GetNextFeature()
        
    if count2 != 10:
        gdaltest.post_reason( 'Got wrong count with GetNextFeature() - %d, expecting 10' % count2 )
        return 'fail'

    return 'success'

###############################################################################
# Test Spatial Query.

def ogr_basic_3():

    minx = 479405
    miny = 4762826
    maxx = 480732
    maxy = 4763590

    ###########################################################################
    # Create query geometry.

    ring = ogr.Geometry( type = ogr.wkbLinearRing )
    ring.AddPoint( minx, miny )
    ring.AddPoint( maxx, miny )
    ring.AddPoint( maxx, maxy )
    ring.AddPoint( minx, maxy )
    ring.AddPoint( minx, miny )

    poly = ogr.Geometry( type = ogr.wkbPolygon )
    poly.AddGeometryDirectly( ring )

    gdaltest.lyr.SetSpatialFilter( poly )
    gdaltest.lyr.ResetReading()

    poly.Destroy()

    count = gdaltest.lyr.GetFeatureCount()
    if count != 1:
        gdaltest.post_reason( 'Got wrong feature count with spatial filter, expected 1, got %d' % count )
        return 'fail'

    feat1 = gdaltest.lyr.GetNextFeature()
    feat2 = gdaltest.lyr.GetNextFeature()

    if feat1 is None or feat2 is not None:
        gdaltest.post_reason( 'Got too few or too many features with spatial filter.' )
        return 'fail'

    feat1.Destroy()

    gdaltest.lyr.SetSpatialFilter( None )
    count = gdaltest.lyr.GetFeatureCount()
    if count != 10:
        gdaltest.post_reason( 'Clearing spatial query may not have worked properly, getting\n%d features instead of expected 10 features.' % count )
        return 'fail'

    return 'success'

###############################################################################
# Test GetDriver().

def ogr_basic_4():
    driver = gdaltest.ds.GetDriver()
    if driver is None:
        gdaltest.post_reason( 'GetDriver() returns None' )
        return 'fail'

    if driver.GetName() != 'ESRI Shapefile':
        gdaltest.post_reason( 'Got wrong driver name: ' + driver.GetName() )
        return 'fail'

    return 'success'

###############################################################################
# Test attribute query on special field fid - per bug 1468.

def ogr_basic_5():

    gdaltest.lyr.SetAttributeFilter( 'FID = 3' )
    gdaltest.lyr.ResetReading()
    
    feat1 = gdaltest.lyr.GetNextFeature()
    feat2 = gdaltest.lyr.GetNextFeature()

    gdaltest.lyr.SetAttributeFilter( None )
    
    if feat1 is None or feat2 is not None:
        gdaltest.post_reason( 'unexpected result count.' )
        return 'fail'

    if feat1.GetFID() != 3:
        gdaltest.post_reason( 'got wrong feature.' )
        return 'fail'

    feat1.Destroy()

    return 'success'


###############################################################################
# Test opening a dataset with an empty string and a non existing dataset
def ogr_basic_6():

    # Put inside try/except for OG python bindings
    try:
        if ogr.Open( '' ) is not None:
            return 'fail'
    except:
        pass

    try:
        if ogr.Open( 'non_existing' ) is not None:
            return 'fail'
    except:
        pass

    return 'success'

###############################################################################
# Test ogr.Feature.Equal()

def ogr_basic_7():

    feat_defn = ogr.FeatureDefn()
    feat = ogr.Feature(feat_defn)
    if not feat.Equal(feat):
        return 'fail'
        
    try:
        feat.SetFieldIntegerList
    except:
        return 'skip'

    feat_clone = feat.Clone()
    if not feat.Equal(feat_clone):
        return 'fail'

    # We MUST delete now as we are changing the feature defn afterwards !
    # Crash guaranteed otherwise
    feat.Destroy()
    feat_clone.Destroy()

    field_defn = ogr.FieldDefn('field1', ogr.OFTInteger)
    feat_defn.AddFieldDefn(field_defn)
    field_defn = ogr.FieldDefn('field2', ogr.OFTReal)
    feat_defn.AddFieldDefn(field_defn)
    field_defn = ogr.FieldDefn('field3', ogr.OFTString)
    feat_defn.AddFieldDefn(field_defn)
    field_defn = ogr.FieldDefn('field4', ogr.OFTIntegerList)
    feat_defn.AddFieldDefn(field_defn)
    field_defn = ogr.FieldDefn('field5', ogr.OFTRealList)
    feat_defn.AddFieldDefn(field_defn)
    field_defn = ogr.FieldDefn('field6', ogr.OFTStringList)
    feat_defn.AddFieldDefn(field_defn)
    field_defn = ogr.FieldDefn('field7', ogr.OFTDate)
    feat_defn.AddFieldDefn(field_defn)
    field_defn = ogr.FieldDefn('field8', ogr.OFTTime)
    feat_defn.AddFieldDefn(field_defn)
    field_defn = ogr.FieldDefn('field9', ogr.OFTDateTime)
    feat_defn.AddFieldDefn(field_defn)
    field_defn = ogr.FieldDefn('field10', ogr.OFTBinary)
    feat_defn.AddFieldDefn(field_defn)

    feat = ogr.Feature(feat_defn)
    feat.SetFID(100)
    feat.SetField(0, 1)
    feat.SetField(1, 1.2)
    feat.SetField(2, "A")
    feat.SetFieldIntegerList(3, [1, 2])
    feat.SetFieldDoubleList(4, [1.2, 3.4])
    feat.SetFieldStringList(5, ["A", "B"])
    feat.SetField(6, 2010, 1, 8, 22, 48, 15, 4)
    feat.SetField(7, 2010, 1, 8, 22, 48, 15, 4)
    feat.SetField(8, 2010, 1, 8, 22, 48, 15, 4)
    feat.SetFieldBinaryFromHexString(9, '012345678ABCDEF')

    feat_clone = feat.Clone()
    if not feat.Equal(feat_clone):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        return 'fail'

    feat_almost_clone = feat.Clone()
    geom = ogr.CreateGeometryFromWkt('POINT(0 1)')
    feat_almost_clone.SetGeometry(geom)
    if feat.Equal(feat_almost_clone):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        return 'fail'

    feat_almost_clone = feat.Clone()
    geom = ogr.CreateGeometryFromWkt('POINT(0 1)')
    feat.SetGeometry(geom)
    if feat.Equal(feat_almost_clone):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        return 'fail'

    feat_clone = feat.Clone()
    if not feat.Equal(feat_clone):
        feat.DumpReadable()
        feat_clone.DumpReadable()
        return 'fail'

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetFID(99)
    if feat.Equal(feat_almost_clone):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        return 'fail'

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetField(0, 2)
    if feat.Equal(feat_almost_clone):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        return 'fail'

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetField(1, 2.2)
    if feat.Equal(feat_almost_clone):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        return 'fail'

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetField(2, "B")
    if feat.Equal(feat_almost_clone):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        return 'fail'

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetFieldIntegerList(3, [1, 2, 3])
    if feat.Equal(feat_almost_clone):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        return 'fail'

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetFieldIntegerList(3, [1, 3])
    if feat.Equal(feat_almost_clone):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        return 'fail'

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetFieldDoubleList(4, [1.2, 3.4, 5.6])
    if feat.Equal(feat_almost_clone):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        return 'fail'

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetFieldDoubleList(4, [1.2, 3.5])
    if feat.Equal(feat_almost_clone):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        return 'fail'

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetFieldStringList(5, ["A", "B", "C"])
    if feat.Equal(feat_almost_clone):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        return 'fail'

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetFieldStringList(5, ["A", "D"])
    if feat.Equal(feat_almost_clone):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        return 'fail'

    for num_field in [6, 7, 8]:
        for i in range(7):
            feat_almost_clone = feat.Clone()
            feat_almost_clone.SetField(num_field, 2010+(i==0), 1+(i==1), 
                                       8+(i==2), 22+(i==3), 48+(i==4),
                                       15+(i==5), 4+(i==6))
            if feat.Equal(feat_almost_clone):
                feat.DumpReadable()
                feat_almost_clone.DumpReadable()
                return 'fail'

    feat_almost_clone = feat.Clone()
    feat_almost_clone.SetFieldBinaryFromHexString(9, '00')
    if feat.Equal(feat_almost_clone):
        feat.DumpReadable()
        feat_almost_clone.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Issue several RegisterAll() to check that OGR drivers are good citizens

def ogr_basic_8():

    ogr.RegisterAll()
    ogr.RegisterAll()
    ogr.RegisterAll()

    return 'success'

###############################################################################
# Test ogr.GeometryTypeToName (#4871)

def ogr_basic_9():

    geom_type_tuples = [ [ ogr.wkbUnknown, "Unknown (any)" ],
                         [ ogr.wkbPoint, "Point" ],
                         [ ogr.wkbLineString, "Line String"],
                         [ ogr.wkbPolygon, "Polygon"],
                         [ ogr.wkbMultiPoint, "Multi Point"],
                         [ ogr.wkbMultiLineString, "Multi Line String"],
                         [ ogr.wkbMultiPolygon, "Multi Polygon"],
                         [ ogr.wkbGeometryCollection, "Geometry Collection"],
                         [ ogr.wkbNone, "None"],
                         [ ogr.wkbUnknown | ogr.wkb25DBit, "3D Unknown (any)" ],
                         [ ogr.wkbPoint25D, "3D Point" ],
                         [ ogr.wkbLineString25D, "3D Line String"],
                         [ ogr.wkbPolygon25D, "3D Polygon"],
                         [ ogr.wkbMultiPoint25D, "3D Multi Point"],
                         [ ogr.wkbMultiLineString25D, "3D Multi Line String"],
                         [ ogr.wkbMultiPolygon25D, "3D Multi Polygon"],
                         [ ogr.wkbGeometryCollection25D, "3D Geometry Collection"],
                         [ 123456, "Unrecognised: 123456" ]
                       ]

    for geom_type_tuple in geom_type_tuples:
        if ogr.GeometryTypeToName(geom_type_tuple[0]) != geom_type_tuple[1]:
            gdaltest.post_reason('fail')
            print('Got %s, expected %s' % (ogr.GeometryTypeToName(geom_type_tuple[0]), geom_type_tuple[1]))
            return 'fail'

    return 'success'

###############################################################################
# cleanup

def ogr_basic_cleanup():
    gdaltest.lyr = None
    gdaltest.ds.Destroy()
    gdaltest.ds = None

    return 'success'

gdaltest_list = [ 
    ogr_basic_1,
    ogr_basic_2,
    ogr_basic_3,
    ogr_basic_4,
    ogr_basic_5,
    ogr_basic_6,
    ogr_basic_7,
    ogr_basic_8,
    ogr_basic_9,
    ogr_basic_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_basic_test' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

