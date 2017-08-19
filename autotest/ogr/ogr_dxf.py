#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR DXF driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
from sys import version_info

sys.path.append( '../pymod' )

import ogrtest
import gdaltest
from osgeo import gdal
from osgeo import ogr

###############################################################################
# Check some general things to see if they meet expectations.

def ogr_dxf_1():

    gdaltest.dxf_ds = ogr.Open( 'data/assorted.dxf' )

    if gdaltest.dxf_ds is None:
        return 'fail'

    if gdaltest.dxf_ds.GetLayerCount() != 1:
        gdaltest.post_reason( 'expected exactly one layer!' )
        return 'fail'

    gdaltest.dxf_layer = gdaltest.dxf_ds.GetLayer(0)

    if gdaltest.dxf_layer.GetName() != 'entities':
        gdaltest.post_reason( 'did not get expected layer name.' )
        return 'fail'

    defn = gdaltest.dxf_layer.GetLayerDefn()
    if defn.GetFieldCount() != 6:
        gdaltest.post_reason( 'did not get expected number of fields.' )
        return 'fail'

    fc = gdaltest.dxf_layer.GetFeatureCount()
    if fc != 16:
        gdaltest.post_reason( 'did not get expected feature count, got %d' % fc)
        return 'fail'

    # Setup the utf-8 string.
    if version_info >= (3,0,0):
        gdaltest.sample_text =  'Text Sample1\u00BF\u03BB\n"abc"'
        gdaltest.sample_style = 'Text Sample1\u00BF\u03BB\n\\"abc\\"'
    else:
        exec("gdaltest.sample_text =  u'Text Sample1\u00BF\u03BB'")
        gdaltest.sample_text += chr(10)

        gdaltest.sample_style = gdaltest.sample_text + '\\"abc\\"'
        gdaltest.sample_style = gdaltest.sample_style.encode('utf-8')

        gdaltest.sample_text += '"abc"'
        gdaltest.sample_text = gdaltest.sample_text.encode('utf-8')

    return 'success'

###############################################################################
# Read the first feature, an ellipse and see if it generally meets expectations.

def ogr_dxf_2():

    gdaltest.dxf_layer.ResetReading()

    feat = gdaltest.dxf_layer.GetNextFeature()

    if feat.Layer != '0':
        gdaltest.post_reason( 'did not get expected layer for feature 0' )
        return 'fail'

    if feat.GetFID() != 0:
        gdaltest.post_reason( 'did not get expected fid for feature 0' )
        return 'fail'

    if feat.SubClasses != 'AcDbEntity:AcDbEllipse':
        gdaltest.post_reason( 'did not get expected SubClasses on feature 0.' )
        return 'fail'

    if feat.LineType != 'ByLayer':
        gdaltest.post_reason( 'Did not get expected LineType' )
        return 'fail'

    if feat.EntityHandle != '43':
        gdaltest.post_reason( 'did not get expected EntityHandle' )
        return 'fail'

    if feat.GetStyleString() != 'PEN(c:#000000)':
        print( '%s' % feat.GetStyleString())
        gdaltest.post_reason( 'did not get expected style string on feat 0.' )
        return 'fail'

    geom = feat.GetGeometryRef()
    if geom.GetGeometryType() != ogr.wkbLineString25D:
        gdaltest.post_reason( 'did not get expected geometry type.' )
        return 'fail'

    envelope = geom.GetEnvelope()
    area = (envelope[1] - envelope[0]) * (envelope[3] - envelope[2])
    exp_area = 1596.12

    if area < exp_area - 0.5 or area > exp_area + 0.5:
        gdaltest.post_reason( 'envelope area not as expected, got %g.' % area )
        return 'fail'

    if abs(geom.GetX(0)-73.25) > 0.001 or abs(geom.GetY(0)-139.75) > 0.001:
        gdaltest.post_reason( 'first point (%g,%g) not expected location.' \
                              % (geom.GetX(0),geom.GetY(0)) )
        return 'fail'

    return 'success'

###############################################################################
# Second feature should be a partial ellipse.

def ogr_dxf_3():

    feat = gdaltest.dxf_layer.GetNextFeature()

    geom = feat.GetGeometryRef()

    envelope = geom.GetEnvelope()
    area = (envelope[1] - envelope[0]) * (envelope[3] - envelope[2])
    exp_area = 311.864

    if area < exp_area - 0.5 or area > exp_area + 0.5:
        gdaltest.post_reason( 'envelope area not as expected, got %g.' % area )
        return 'fail'

    if abs(geom.GetX(0)-61.133) > 0.01 or abs(geom.GetY(0)-103.592) > 0.01:
        gdaltest.post_reason( 'first point (%g,%g) not expected location.' \
                              % (geom.GetX(0),geom.GetY(0)) )
        return 'fail'

    return 'success'

###############################################################################
# Third feature: point.

def ogr_dxf_4():

    feat = gdaltest.dxf_layer.GetNextFeature()

    if ogrtest.check_feature_geometry( feat, 'POINT (83.5 160.0 0)' ):
        return 'fail'

    return 'success'

###############################################################################
# Fourth feature: LINE

def ogr_dxf_5():

    feat = gdaltest.dxf_layer.GetNextFeature()

    if ogrtest.check_feature_geometry( feat, 'LINESTRING (97.0 159.5 0,108.5 132.25 0)' ):
        return 'fail'

    if feat.GetGeometryRef().GetGeometryType() == ogr.wkbLineString:
        gdaltest.post_reason( 'not keeping 3D linestring as 3D' )
        return 'fail'

    return 'success'

###############################################################################
# Fourth feature: MTEXT

def ogr_dxf_6():

    feat = gdaltest.dxf_layer.GetNextFeature()

    if ogrtest.check_feature_geometry( feat, 'POINT (84 126)' ):
        return 'fail'

    if feat.GetGeometryRef().GetGeometryType() == ogr.wkbPoint25D:
        gdaltest.post_reason( 'not keeping 2D text as 2D' )
        return 'fail'

    if feat.GetStyleString() != 'LABEL(f:"normallatin1",t:"Test",a:30,s:5g,p:7,c:#000000)':
        print(feat.GetStyleString())
        gdaltest.post_reason( 'got wrong style string' )
        return 'fail'

    return 'success'

###############################################################################
# Partial CIRCLE

def ogr_dxf_7():

    feat = gdaltest.dxf_layer.GetNextFeature()

    geom = feat.GetGeometryRef()

    envelope = geom.GetEnvelope()
    area = (envelope[1] - envelope[0]) * (envelope[3] - envelope[2])
    exp_area = 445.748

    if area < exp_area - 0.5 or area > exp_area + 0.5:
        print(envelope)
        gdaltest.post_reason( 'envelope area not as expected, got %g.' % area )
        return 'fail'

    if abs(geom.GetX(0)-115.258) > 0.01 or abs(geom.GetY(0)-107.791) > 0.01:
        gdaltest.post_reason( 'first point (%g,%g) not expected location.' \
                              % (geom.GetX(0),geom.GetY(0)) )
        return 'fail'

    return 'success'

###############################################################################
# Dimension

def ogr_dxf_8():

    # Skip boring line.
    feat = gdaltest.dxf_layer.GetNextFeature()

    # Dimension lines
    feat = gdaltest.dxf_layer.GetNextFeature()
    geom = feat.GetGeometryRef()

    if geom.GetGeometryType() != ogr.wkbMultiLineString:
        gdaltest.post_reason( 'did not get expected geometry type.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'MULTILINESTRING ((63.862871944482457 149.209935992088333,24.341960668550669 111.934531038652722),(72.754404848874373 139.782768575383642,62.744609795879391 150.395563330366286),(33.233493572942614 102.507363621948002,23.2236985199476 113.120158376930675),(63.862871944482457 149.209935992088333,59.187727781045531 147.04077688455709),(63.862871944482457 149.209935992088333,61.424252078251662 144.669522208001183),(24.341960668550669 111.934531038652722,26.78058053478146 116.474944822739886),(24.341960668550669 111.934531038652722,29.017104831987599 114.103690146183979))' ):
        return 'fail'

    # Dimension text
    feat = gdaltest.dxf_layer.GetNextFeature()

    geom = feat.GetGeometryRef()

    if ogrtest.check_feature_geometry( feat, 'POINT (42.815907752635709 131.936242584545397)' ):
        return 'fail'

    expected_style = 'LABEL(f:"Arial",t:"54.3264",p:5,a:43.3,s:2.5g)'
    if feat.GetStyleString() != expected_style:
        gdaltest.post_reason( 'Got unexpected style string:\n%s\ninstead of:\n%s.' % (feat.GetStyleString(),expected_style) )
        return 'fail'

    return 'success'

###############################################################################
# BLOCK (inlined)

def ogr_dxf_9():

    # Skip two dimensions each with a line and text.
    for x in range(4):
        feat = gdaltest.dxf_layer.GetNextFeature()

    # block (merged geometries)
    feat = gdaltest.dxf_layer.GetNextFeature()
    geom = feat.GetGeometryRef()

    if geom.GetGeometryType() != ogr.wkbMultiLineString25D:
        gdaltest.post_reason( 'did not get expected geometry type.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'MULTILINESTRING ((79.069506278985116 121.003652476272777 0,79.716898725419625 118.892590150942851 0),(79.716898725419625 118.892590150942851 0,78.140638855839953 120.440702522851453 0),(78.140638855839953 120.440702522851453 0,80.139111190485622 120.328112532167196 0),(80.139111190485622 120.328112532167196 0,78.619146316248077 118.920737648613908 0),(78.619146316248077 118.920737648613908 0,79.041358781314059 120.975504978601705 0))' ):
        return 'fail'

    # First of two MTEXTs
    feat = gdaltest.dxf_layer.GetNextFeature()
    if feat.GetField( 'Text' ) != gdaltest.sample_text:
        gdaltest.post_reason( 'Did not get expected first mtext.' )
        return 'fail'

    expected_style = 'LABEL(f:"normallatin1",t:"'+gdaltest.sample_style+'",a:45,s:0.5g,p:5,c:#000000)'
    if feat.GetStyleString() != expected_style:
        gdaltest.post_reason( 'Got unexpected style string:\n%s\ninstead of:\n%s.' % (feat.GetStyleString(),expected_style) )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'POINT (77.602201427662891 120.775897075866169 0)' ):
        return 'fail'

    # Second of two MTEXTs
    feat = gdaltest.dxf_layer.GetNextFeature()
    if feat.GetField( 'Text' ) != 'Second':
        gdaltest.post_reason( 'Did not get expected second mtext.' )
        return 'fail'

    if feat.GetField( 'SubClasses' ) != 'AcDbEntity:AcDbMText':
        gdaltest.post_reason( 'Did not get expected subclasses.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'POINT (79.977331629005178 119.698291706738644 0)' ):
        return 'fail'

    return 'success'

###############################################################################
# LWPOLYLINE in an Object Coordinate System.

def ogr_dxf_10():

    ocs_ds = ogr.Open('data/LWPOLYLINE-OCS.dxf')
    ocs_lyr = ocs_ds.GetLayer(0)

    # Skip boring line.
    feat = ocs_lyr.GetNextFeature()

    # LWPOLYLINE in OCS
    feat = ocs_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()

    if geom.GetGeometryType() != ogr.wkbLineString25D:
        print(geom.GetGeometryType())
        gdaltest.post_reason( 'did not get expected geometry type.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'LINESTRING (600325.567999998573214 3153021.253000000491738 562.760000000052969,600255.215999998385087 3151973.98600000096485 536.950000000069849,597873.927999997511506 3152247.628000000491738 602.705000000089058)' ):
        return 'fail'

    ocs_lyr = None
    ocs_ds = None

    return 'success'

###############################################################################
# Test reading from an entities-only dxf file (#3412)

def ogr_dxf_11():

    eo_ds = ogr.Open('data/entities_only.dxf')
    eo_lyr = eo_ds.GetLayer(0)

    # Check first point.
    feat = eo_lyr.GetNextFeature()

    if ogrtest.check_feature_geometry( feat,
                                       'POINT (672500.0 242000.0 539.986)' ):
        return 'fail'

    # Check second point.
    feat = eo_lyr.GetNextFeature()

    if ogrtest.check_feature_geometry( feat,
                                       'POINT (672750.0 242000.0 558.974)' ):
        return 'fail'

    eo_lyr = None
    eo_ds = None

    return 'success'

###############################################################################
# Write a simple file with a polygon and a line, and read back.

def ogr_dxf_12():

    ds = ogr.GetDriverByName('DXF').CreateDataSource('tmp/dxf_11.dxf' )

    lyr = ds.CreateLayer( 'entities' )

    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'LINESTRING(10 12, 60 65)' ) )
    lyr.CreateFeature( dst_feat )
    dst_feat = None

    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'POLYGON((0 0,100 0,100 100,0 0))' ) )
    lyr.CreateFeature( dst_feat )
    dst_feat = None

    # Test 25D linestring with constant Z (#5210)
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'LINESTRING(1 2 10,3 4 10)' ) )
    lyr.CreateFeature( dst_feat )
    dst_feat = None

    # Test 25D linestring with different Z (#5210)
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'LINESTRING(1 2 -10,3 4 10)' ) )
    lyr.CreateFeature( dst_feat )
    dst_feat = None

    lyr = None
    ds = None

    # Read back.
    ds = ogr.Open('tmp/dxf_11.dxf')
    lyr = ds.GetLayer(0)

    # Check first feature
    feat = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry( feat,
                                       'LINESTRING(10 12, 60 65)' ):
        print(feat.GetGeometryRef().ExportToWkt())
        return 'fail'

    if feat.GetGeometryRef().GetGeometryType() != ogr.wkbLineString:
        gdaltest.post_reason( 'not linestring 2D' )
        return 'fail'
    feat = None

    # Check second feature
    feat = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry( feat,
                                       'POLYGON((0 0,100 0,100 100,0 0))' ):
        print(feat.GetGeometryRef().ExportToWkt())
        return 'fail'

    if feat.GetGeometryRef().GetGeometryType() != ogr.wkbPolygon:
        gdaltest.post_reason( 'not keeping polygon 2D' )
        return 'fail'
    feat = None

    # Check third feature
    feat = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry( feat,
                                       'LINESTRING(1 2 10,3 4 10)' ):
        print(feat.GetGeometryRef().ExportToWkt())
        return 'fail'
    feat = None

    # Check fourth feature
    feat = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry( feat,
                                       'LINESTRING(1 2 -10,3 4 10)' ):
        print(feat.GetGeometryRef().ExportToWkt())
        return 'fail'
    feat = None

    lyr = None
    ds = None
    ds = None

    os.unlink( 'tmp/dxf_11.dxf' )

    return 'success'


###############################################################################
# Check smoothed polyline.

def ogr_dxf_13():

    ds = ogr.Open( 'data/polyline_smooth.dxf' )

    layer = ds.GetLayer(0)

    feat = layer.GetNextFeature()

    if feat.Layer != '1':
        gdaltest.post_reason( 'did not get expected layer for feature 0' )
        return 'fail'

    geom = feat.GetGeometryRef()
    if geom.GetGeometryType() != ogr.wkbLineString25D:
        gdaltest.post_reason( 'did not get expected geometry type.' )
        return 'fail'

    envelope = geom.GetEnvelope()
    area = (envelope[1] - envelope[0]) * (envelope[3] - envelope[2])
    exp_area = 1350.43

    if area < exp_area - 0.5 or area > exp_area + 0.5:
        gdaltest.post_reason( 'envelope area not as expected, got %g.' % area )
        return 'fail'

    # Check for specific number of points from tessellated arc(s).
    # Note that this number depends on the tessellation algorithm and
    # possibly the default global arc_stepsize variable; therefore it is
    # not guaranteed to remain constant even if the input DXF file is constant.
    # If you retain this test, you may need to update the point count if
    # changes are made to the aforementioned items. Ideally, one would test
    # only that more points are returned than in the original polyline, and
    # that the points lie along (or reasonably close to) said path.

    if geom.GetPointCount() != 146:
        gdaltest.post_reason( 'did not get expected number of points, got %d' % (geom.GetPointCount()) )
        return 'fail'

    if abs(geom.GetX(0)-251297.8179) > 0.001 \
       or abs(geom.GetY(0)-412226.8286) > 0.001:
        gdaltest.post_reason( 'first point (%g,%g) not expected location.' \
                              % (geom.GetX(0),geom.GetY(0)) )
        return 'fail'

    # Other possible tests:
    # Polylines with no explicit Z coordinates (e.g., no attribute 38 for
    # LWPOLYLINE and no attribute 30 for POLYLINE) should always return
    # geometry type ogr.wkbPolygon. Otherwise, ogr.wkbPolygon25D should be
    # returned even if the Z coordinate values are zero.
    # If the arc_stepsize global is used, one could test that returned adjacent
    # points do not slope-diverge greater than that value.

    ds = None

    return 'success'


###############################################################################
# Check smooth LWPOLYLINE entity.

def ogr_dxf_14():

    # This test is identical to the previous one except the
    # newer lwpolyline entity is used. See the comments in the
    # previous test regarding caveats, etc.

    ds = ogr.Open( 'data/lwpolyline_smooth.dxf' )

    layer = ds.GetLayer(0)

    feat = layer.GetNextFeature()

    if feat.Layer != '1':
        gdaltest.post_reason( 'did not get expected layer for feature 0' )
        return 'fail'

    geom = feat.GetGeometryRef()
    if geom.GetGeometryType() != ogr.wkbLineString:
        gdaltest.post_reason( 'did not get expected geometry type.' )
        return 'fail'

    envelope = geom.GetEnvelope()
    area = (envelope[1] - envelope[0]) * (envelope[3] - envelope[2])
    exp_area = 1350.43

    if area < exp_area - 0.5 or area > exp_area + 0.5:
        gdaltest.post_reason( 'envelope area not as expected, got %g.' % area )
        return 'fail'

    if geom.GetPointCount() != 146:
        gdaltest.post_reason( 'did not get expected number of points, got %d' % (geom.GetPointCount()) )
        return 'fail'

    if abs(geom.GetX(0)-251297.8179) > 0.001 \
       or abs(geom.GetY(0)-412226.8286) > 0.001:
        gdaltest.post_reason( 'first point (%g,%g) not expected location.' \
                              % (geom.GetX(0),geom.GetY(0)) )
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Write a file with dynamic layer creation and confirm that the
# dynamically created layer 'abc' matches the definition of the default
# layer '0'.

def ogr_dxf_15():

    ds = ogr.GetDriverByName('DXF').CreateDataSource('tmp/dxf_14.dxf',
                                                     ['FIRST_ENTITY=80'] )

    lyr = ds.CreateLayer( 'entities' )

    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'LINESTRING(10 12, 60 65)' ) )
    dst_feat.SetField( 'Layer', 'abc' )
    lyr.CreateFeature( dst_feat )

    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'POLYGON((0 0,100 0,100 100,0 0))' ) )
    lyr.CreateFeature( dst_feat )

    lyr = None
    ds = None

    # Read back.
    ds = ogr.Open('tmp/dxf_14.dxf')
    lyr = ds.GetLayer(0)

    # Check first feature
    feat = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry( feat,
                                       'LINESTRING(10 12, 60 65)' ):
        print(feat.GetGeometryRef().ExportToWkt())
        return 'fail'

    if feat.GetGeometryRef().GetGeometryType() == ogr.wkbLineString25D:
        gdaltest.post_reason( 'not linestring 2D' )
        return 'fail'

    if feat.GetField('Layer') != 'abc':
        gdaltest.post_reason( 'Did not get expected layer, abc.' )
        return 'fail'

    # Check second point.
    feat = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry( feat,
                                       'POLYGON((0 0,100 0,100 100,0 0))' ):
        print(feat.GetGeometryRef().ExportToWkt())
        return 'fail'

    if feat.GetGeometryRef().GetGeometryType() == ogr.wkbPolygon25D:
        gdaltest.post_reason( 'not keeping polygon 2D' )
        return 'fail'

    if feat.GetField('Layer') != '0':
        print(feat.GetField('Layer'))
        gdaltest.post_reason( 'Did not get expected layer, 0.' )
        return 'fail'

    lyr = None
    ds = None

    # Check the DXF file itself to try and ensure that the layer
    # is defined essentially as we expect.  We assume the only thing
    # that will be different is the layer name is 'abc' instead of '0'
    # and the entity id.

    outdxf = open('tmp/dxf_14.dxf').read()
    start_1 = outdxf.find('  0\nLAYER')
    start_2 = outdxf.find('  0\nLAYER',start_1+10)

    txt_1 = outdxf[start_1:start_2]
    txt_2 = outdxf[start_2:start_2+len(txt_1)+2]

    abc_off = txt_2.find('abc\n')

    if txt_2[16:abc_off] + '0' + txt_2[abc_off+3:] != txt_1[16:]:
        print(txt_2[abc_off] + '0' + txt_2[abc_off+3:])
        print(txt_1)
        gdaltest.post_reason( 'Layer abc does not seem to match layer 0.' )
        return 'fail'

    # Check that $HANDSEED was set as expected.
    start_seed = outdxf.find('$HANDSEED')
    handseed = outdxf[start_seed+10+4:start_seed+10+4+8]
    if handseed != '00000053':
        gdaltest.post_reason( 'Did not get expected HANDSEED, got %s.' % handseed)
        return 'fail'

    os.unlink( 'tmp/dxf_14.dxf' )

    return 'success'


###############################################################################
# Test reading without DXF blocks inlined.

def ogr_dxf_16():

    gdal.SetConfigOption( 'DXF_INLINE_BLOCKS', 'FALSE' )

    dxf_ds = ogr.Open( 'data/assorted.dxf' )

    if dxf_ds is None:
        return 'fail'

    if dxf_ds.GetLayerCount() != 2:
        gdaltest.post_reason( 'expected exactly two layers!' )
        return 'fail'

    dxf_layer = dxf_ds.GetLayer(1)

    if dxf_layer.GetName() != 'entities':
        gdaltest.post_reason( 'did not get expected layer name.' )
        return 'fail'

    # read through till we encounter the block reference.
    feat = dxf_layer.GetNextFeature()
    while feat.GetField('EntityHandle') != '55':
        feat = dxf_layer.GetNextFeature()

    # check contents.
    if feat.GetField('BlockName') != 'STAR':
        gdaltest.post_reason( 'Did not get blockname!' )
        return 'fail'

    if feat.GetField('BlockAngle') != 0.0:
        gdaltest.post_reason( 'Did not get expected angle.' )
        return 'fail'

    if feat.GetField('BlockScale') != [1.0,1.0,1.0]:
        print(feat.GetField('BlockScale'))
        gdaltest.post_reason( 'Did not get expected BlockScale' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'POINT (79.097653776656188 119.962195062443342 0)' ):
        return 'fail'

    feat = None

    # Now we need to check the blocks layer and ensure it is as expected.

    dxf_layer = dxf_ds.GetLayer(0)

    if dxf_layer.GetName() != 'blocks':
        gdaltest.post_reason( 'did not get expected layer name.' )
        return 'fail'

    # First MTEXT
    feat = dxf_layer.GetNextFeature()
    if feat.GetField( 'Text' ) != gdaltest.sample_text:
        gdaltest.post_reason( 'Did not get expected first mtext.' )
        return 'fail'

    expected_style = 'LABEL(f:"normallatin1",t:"'+gdaltest.sample_style+'",a:45,s:0.5g,p:5,c:#000000)'
    if feat.GetStyleString() != expected_style:
        gdaltest.post_reason( 'Got unexpected style string:\n%s\ninstead of:\n%s.' % (feat.GetStyleString(),expected_style) )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'POINT (-1.495452348993292 0.813702013422821 0)' ):
        return 'fail'

    # Second MTEXT
    feat = dxf_layer.GetNextFeature()
    if feat.GetField( 'Text' ) != 'Second':
        gdaltest.post_reason( 'Did not get expected second mtext.' )
        return 'fail'

    if feat.GetField( 'SubClasses' ) != 'AcDbEntity:AcDbMText':
        gdaltest.post_reason( 'Did not get expected subclasses.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'POINT (0.879677852348995 -0.263903355704699 0)' ):
        return 'fail'

    # STAR geometry
    feat = dxf_layer.GetNextFeature()

    if feat.GetField('BlockName') != 'STAR':
        gdaltest.post_reason( 'Did not get expected block name.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'MULTILINESTRING ((-0.028147497671066 1.041457413829428 0,0.619244948763444 -1.069604911500494 0),(0.619244948763444 -1.069604911500494 0,-0.957014920816232 0.478507460408116 0),(-0.957014920816232 0.478507460408116 0,1.041457413829428 0.365917469723853 0),(1.041457413829428 0.365917469723853 0,-0.478507460408116 -1.041457413829428 0),(-0.478507460408116 -1.041457413829428 0,-0.056294995342131 1.013309916158363 0))' ):
        return 'fail'

    feat = None

    # cleanup

    gdal.SetConfigOption( 'DXF_INLINE_BLOCKS', 'TRUE' )

    return 'success'

###############################################################################
# Write a file with blocks defined from a source blocks layer.

def ogr_dxf_17():

    ds = ogr.GetDriverByName('DXF').CreateDataSource('tmp/dxf_17.dxf',
                                                     ['HEADER=data/header_extended.dxf'])

    blyr = ds.CreateLayer( 'blocks' )
    lyr = ds.CreateLayer( 'entities' )

    dst_feat = ogr.Feature( feature_def = blyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt(
        'GEOMETRYCOLLECTION( LINESTRING(0 0,1 1),LINESTRING(1 0,0 1))' ) )
    dst_feat.SetField( 'BlockName', 'XMark' )
    blyr.CreateFeature( dst_feat )

    # Block with 2 polygons
    dst_feat = ogr.Feature( feature_def = blyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt(
        'GEOMETRYCOLLECTION( POLYGON((10 10,10 20,20 20,20 10,10 10)),POLYGON((10 -10,10 -20,20 -20,20 -10,10 -10)))' ) )
    dst_feat.SetField( 'BlockName', 'Block2' )
    blyr.CreateFeature( dst_feat )

    # Block with point and line
    dst_feat = ogr.Feature( feature_def = blyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt(
        'GEOMETRYCOLLECTION( POINT(1 2),LINESTRING(0 0,1 1))' ) )
    dst_feat.SetField( 'BlockName', 'Block3' )
    blyr.CreateFeature( dst_feat )

    # Write a block reference feature.
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'POINT(200 100)' ))
    dst_feat.SetField( 'Layer', 'abc' )
    dst_feat.SetField( 'BlockName', 'XMark' )
    lyr.CreateFeature( dst_feat )

    # Write a block reference feature for a non-existent block.
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'POINT(300 50)' ))
    dst_feat.SetField( 'Layer', 'abc' )
    dst_feat.SetField( 'BlockName', 'DoesNotExist' )
    lyr.CreateFeature( dst_feat )

    # Write a block reference feature for a template defined block
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'POINT(250 200)' ))
    dst_feat.SetField( 'Layer', 'abc' )
    dst_feat.SetField( 'BlockName', 'STAR' )
    lyr.CreateFeature( dst_feat )

    # Write a block reference feature with scaling and rotation
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'POINT(300 100)' ))
    dst_feat.SetField( 'BlockName', 'XMark' )
    dst_feat.SetField( 'BlockAngle', '30' )
    dst_feat.SetFieldDoubleList(lyr.GetLayerDefn().GetFieldIndex('BlockScale'),
                                [4.0,5.0,6.0] )
    lyr.CreateFeature( dst_feat )

    # Write a Block2 reference feature.
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'POINT(350 100)' ))
    dst_feat.SetField( 'Layer', 'abc' )
    dst_feat.SetField( 'BlockName', 'Block2' )
    lyr.CreateFeature( dst_feat )

    # Write a Block3 reference feature.
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'POINT(400 100)' ))
    dst_feat.SetField( 'Layer', 'abc' )
    dst_feat.SetField( 'BlockName', 'Block3' )
    lyr.CreateFeature( dst_feat )

    ds = None

    # Reopen and check contents.

    ds = ogr.Open('tmp/dxf_17.dxf')

    lyr = ds.GetLayer(0)

    # Check first feature.
    feat = lyr.GetNextFeature()
    if feat.GetField('SubClasses') != 'AcDbEntity:AcDbBlockReference':
        gdaltest.post_reason( 'Got wrong subclasses for feature 1.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'MULTILINESTRING ((200 100,201 101),(201 100,200 101))' ):
        print( 'Feature 1' )
        return 'fail'

    # Check 2nd feature.
    feat = lyr.GetNextFeature()
    if feat.GetField('SubClasses') != 'AcDbEntity:AcDbPoint':
        gdaltest.post_reason( 'Got wrong subclasses for feature 2.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'POINT (300 50)' ):
        print( 'Feature 2' )
        return 'fail'

    # Check 3rd feature.
    feat = lyr.GetNextFeature()
    if feat.GetField('SubClasses') != 'AcDbEntity:AcDbBlockReference':
        gdaltest.post_reason( 'Got wrong subclasses for feature 3.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'MULTILINESTRING ((249.971852502328943 201.04145741382942 0,250.619244948763452 198.930395088499495 0),(250.619244948763452 198.930395088499495 0,249.042985079183779 200.47850746040811 0),(249.042985079183779 200.47850746040811 0,251.04145741382942 200.365917469723854 0),(251.04145741382942 200.365917469723854 0,249.52149253959189 198.95854258617058 0),(249.52149253959189 198.95854258617058 0,249.943705004657858 201.013309916158363 0))' ):
        print( 'Feature 3' )
        return 'fail'

    # Check 4th feature (scaled and rotated)
    feat = lyr.GetNextFeature()
    if feat.GetField('SubClasses') != 'AcDbEntity:AcDbBlockReference':
        gdaltest.post_reason( 'Got wrong subclasses for feature 4.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'MULTILINESTRING ((300 100,300.964101615137736 106.330127018922198), (303.464101615137736 102.0,297.5 104.330127018922198))' ):
        print( 'Feature 4' )
        return 'fail'

    # Check 5th feature
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'MULTIPOLYGON (((360 110,360 120,370 120,370 110,360 110)),((360 90,360 80,370 80,370 90,360 90)))' ):
        print( 'Feature 5' )
        return 'fail'

    # Check 6th feature
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'GEOMETRYCOLLECTION (POINT (401 102),LINESTRING (400 100,401 101))' ):
        print( 'Feature 5' )
        return 'fail'

    # Cleanup

    lyr = None
    ds = None

    os.unlink( 'tmp/dxf_17.dxf' )

    return 'success'

###############################################################################
# Write a file with line patterns, and make sure corresponding Linetypes are
# created.

def ogr_dxf_18():

    ds = ogr.GetDriverByName('DXF').CreateDataSource('tmp/dxf_18.dxf',
                                                     ['HEADER=data/header_extended.dxf'])

    lyr = ds.CreateLayer( 'entities' )

    # Write a feature with a predefined LTYPE in the header.
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt('LINESTRING(0 0,25 25)') )
    dst_feat.SetField( 'Linetype', 'DASHED' )
    dst_feat.SetStyleString( 'PEN(c:#ffff00,w:2g,p:"12.0g 6.0g")' )
    lyr.CreateFeature( dst_feat )

    # Write a feature with a named linetype but that isn't predefined in the header.
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt('LINESTRING(5 5,30 30)') )
    dst_feat.SetField( 'Linetype', 'DOTTED' )
    dst_feat.SetStyleString( 'PEN(c:#ffff00,w:2g,p:"0.0g 4.0g")' )
    lyr.CreateFeature( dst_feat )

    # Write a feature without a linetype name - it will be created.
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt('LINESTRING(5 5,40 30)') )
    dst_feat.SetStyleString( 'PEN(c:#ffff00,w:2g,p:"3.0g 4.0g")' )
    lyr.CreateFeature( dst_feat )

    ds = None

    # Reopen and check contents.

    ds = ogr.Open('tmp/dxf_18.dxf')

    lyr = ds.GetLayer(0)

    # Check first feature.
    feat = lyr.GetNextFeature()
    if feat.GetField('Linetype') != 'DASHED':
        gdaltest.post_reason( 'Got wrong linetype. (1)' )
        return 'fail'

    if feat.GetStyleString() != 'PEN(c:#ffff00,w:2g,p:"12.6999999999999993g 6.3499999999999996g")':
        print(feat.GetStyleString())
        gdaltest.post_reason( "got wrong style string (1)" )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'LINESTRING (0 0,25 25)' ):
        return 'fail'

    # Check second feature.
    feat = lyr.GetNextFeature()
    if feat.GetField('Linetype') != 'DOTTED':
        gdaltest.post_reason( 'Got wrong linetype. (2)' )
        return 'fail'

    if feat.GetStyleString() != 'PEN(c:#ffff00,w:2g,p:"0.0g 4.0g")':
        print(feat.GetStyleString())
        gdaltest.post_reason( "got wrong style string (2)" )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'LINESTRING (5 5,30 30)' ):
        return 'fail'

    # Check third feature.
    feat = lyr.GetNextFeature()
    if feat.GetField('Linetype') != 'AutoLineType-1':
        gdaltest.post_reason( 'Got wrong linetype. (3)' )
        return 'fail'

    if feat.GetStyleString() != 'PEN(c:#ffff00,w:2g,p:"3.0g 4.0g")':
        print(feat.GetStyleString())
        gdaltest.post_reason( "got wrong style string (3)" )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'LINESTRING (5 5,40 30)' ):
        return 'fail'

    # Cleanup

    lyr = None
    ds = None

    os.unlink( 'tmp/dxf_18.dxf' )

    return 'success'

###############################################################################
# Test writing a file using references to blocks defined entirely in the
# template - no blocks layer transferred.

def ogr_dxf_19():

    ds = ogr.GetDriverByName('DXF').CreateDataSource('tmp/dxf_19.dxf',
                                                     ['HEADER=data/header_extended.dxf'])

    lyr = ds.CreateLayer( 'entities' )

    # Write a block reference feature for a template defined block
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'POINT(250 200)' ))
    dst_feat.SetField( 'Layer', 'abc' )
    dst_feat.SetField( 'BlockName', 'STAR' )
    lyr.CreateFeature( dst_feat )

    ds = None

    # Reopen and check contents.

    ds = ogr.Open('tmp/dxf_19.dxf')

    lyr = ds.GetLayer(0)

    # Check first feature.
    feat = lyr.GetNextFeature()
    if feat.GetField('SubClasses') != 'AcDbEntity:AcDbBlockReference':
        gdaltest.post_reason( 'Got wrong subclasses for feature 1.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'MULTILINESTRING ((249.971852502328943 201.04145741382942 0,250.619244948763452 198.930395088499495 0),(250.619244948763452 198.930395088499495 0,249.042985079183779 200.47850746040811 0),(249.042985079183779 200.47850746040811 0,251.04145741382942 200.365917469723854 0),(251.04145741382942 200.365917469723854 0,249.52149253959189 198.95854258617058 0),(249.52149253959189 198.95854258617058 0,249.943705004657858 201.013309916158363 0))' ):
        return 'fail'

    # Cleanup

    lyr = None
    ds = None

    os.unlink( 'tmp/dxf_19.dxf' )

    return 'success'

###############################################################################
# SPLINE

def ogr_dxf_20():

    ds = ogr.Open('data/spline_qcad.dxf')
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry( feat, 'LINESTRING (10.75 62.75,20.637752769146068 63.434832501489716,29.283239084385464 63.396838394381845,36.766943814562865 62.711565975596599,43.169351828522906 61.454563542054103,48.570947995110252 59.70137939067456,53.05221718316956 57.527561818378146,56.693644261545501 55.008659122085049,59.575714099082703 52.220219598715438,61.778911564625851 49.237791545189509,63.383721527019588 46.136923258427423,64.470628855108572 42.993163035349369,65.120118417737459 39.882059172875508,65.412419131869868 36.878358785215056,65.417809785093752 34.025663008687722,65.193643595004147 31.327113252708507,64.796409941597645 28.783146935042897,64.282598204870823 26.394201473456341,63.708697764820236 24.16071428571431,63.131198001442392 22.083122789582241,62.606588294733939 20.161864402825621,62.191358024691354 18.397376543209894,61.941996571311265 16.790096628500525,61.914993314590184 15.340462076462975,62.166837634524704 14.0489103048627,62.754018911111373 12.915878731465167,63.723652286703427 11.940700981548817,65.053571428571416 11.114552964042769,66.690557841792398 10.424954275262921,68.581246558980226 9.859407264767562,70.672272612748785 9.405414282114966,72.910271035711943 9.050477676863418,75.241876860483572 8.782099798571203,77.613725119677511 8.587782996796603,79.97245084590763 8.4550296210979,82.264689071787842 8.371342021033378,84.437074829931987 8.324222546161321,86.436243152953921 8.301173546040012,88.208926721776336 8.289771106365336,89.722559658784164 8.293223374005688,90.990763736417563 8.349615688917151,92.033410218878885 8.501752503862612,92.870370370370395 8.792438271604945,93.521515455094473 9.264477444907039,94.006716737253413 9.960674476531764,94.345845481049565 10.923833819242011,94.558772950685281 12.196759925800654,94.665370410362868 13.82225724897058,94.685509124284636 15.843130241514663,94.639060356652948 18.302183356195791,94.545895371670113 21.242221045776841,94.421471763308503 24.702030018356666,94.215205541358216 28.660279617432039,93.825673773330607 33.049360720184715,93.15014577259474 37.800473760933045,92.085890852519697 42.844819173995376,90.530178326474584 48.113597393690064,88.380277507828495 53.538008854335445,85.533457709950525 59.049253990249873,81.886988246209697 64.578533235751706,77.338138429975174 70.057047025159264,71.784177574615995 75.415995792790937,65.122374993501282 80.586579972965055,57.25 85.5)' ):
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# CIRCLE

def ogr_dxf_21():

    ds = ogr.Open('data/circle.dxf')
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry( feat, 'LINESTRING (5 2 3,4.990256201039297 1.720974105023499 3,4.961072274966281 1.443307596159738 3,4.912590402935223 1.168353236728963 3,4.845046783753276 0.897450576732003 3,4.758770483143634 0.631919426697325 3,4.654181830570403 0.373053427696799 3,4.531790371435708 0.122113748856437 3,4.392192384625703 -0.11967705693282 3,4.23606797749979 -0.351141009169893 3,4.064177772475912 -0.571150438746157 3,3.877359201354605 -0.778633481835989 3,3.676522425435433 -0.972579301909577 3,3.462645901302633 -1.152043014426888 3,3.236771613882987 -1.316150290220167 3,3.0 -1.464101615137754 3,2.75348458715631 -1.595176185196668 3,2.498426373663648 -1.70873541826715 3,2.23606797749979 -1.804226065180614 3,1.967687582398672 -1.881182905103986 3,1.694592710667722 -1.939231012048832 3,1.418113853070614 -1.978087581473093 3,1.139597986810004 -1.997563308076383 3,0.860402013189997 -1.997563308076383 3,0.581886146929387 -1.978087581473094 3,0.305407289332279 -1.939231012048832 3,0.032312417601329 -1.881182905103986 3,-0.236067977499789 -1.804226065180615 3,-0.498426373663648 -1.70873541826715 3,-0.75348458715631 -1.595176185196668 3,-1.0 -1.464101615137755 3,-1.236771613882987 -1.316150290220167 3,-1.462645901302633 -1.152043014426888 3,-1.676522425435433 -0.972579301909577 3,-1.877359201354605 -0.778633481835989 3,-2.064177772475912 -0.571150438746158 3,-2.236067977499789 -0.351141009169893 3,-2.392192384625704 -0.11967705693282 3,-2.531790371435707 0.122113748856436 3,-2.654181830570403 0.373053427696798 3,-2.758770483143633 0.631919426697324 3,-2.845046783753275 0.897450576732001 3,-2.912590402935223 1.168353236728963 3,-2.961072274966281 1.443307596159737 3,-2.990256201039297 1.720974105023498 3,-3.0 2.0 3,-2.990256201039297 2.279025894976499 3,-2.961072274966281 2.556692403840262 3,-2.912590402935223 2.831646763271036 3,-2.845046783753276 3.102549423267996 3,-2.758770483143634 3.368080573302675 3,-2.654181830570404 3.626946572303199 3,-2.531790371435708 3.877886251143563 3,-2.392192384625704 4.119677056932819 3,-2.23606797749979 4.351141009169892 3,-2.064177772475912 4.571150438746157 3,-1.877359201354604 4.778633481835989 3,-1.676522425435434 4.972579301909576 3,-1.462645901302632 5.152043014426889 3,-1.236771613882989 5.316150290220166 3,-1.0 5.464101615137753 3,-0.753484587156311 5.595176185196667 3,-0.498426373663649 5.70873541826715 3,-0.23606797749979 5.804226065180615 3,0.032312417601329 5.881182905103985 3,0.305407289332279 5.939231012048833 3,0.581886146929387 5.978087581473094 3,0.860402013189993 5.997563308076383 3,1.139597986810005 5.997563308076383 3,1.418113853070612 5.978087581473094 3,1.69459271066772 5.939231012048833 3,1.96768758239867 5.881182905103986 3,2.236067977499789 5.804226065180615 3,2.498426373663648 5.70873541826715 3,2.75348458715631 5.595176185196668 3,3.0 5.464101615137754 3,3.236771613882985 5.316150290220168 3,3.462645901302634 5.152043014426887 3,3.676522425435431 4.972579301909578 3,3.877359201354603 4.778633481835991 3,4.064177772475912 4.571150438746159 3,4.23606797749979 4.351141009169893 3,4.392192384625702 4.119677056932823 3,4.531790371435708 3.877886251143563 3,4.654181830570404 3.626946572303201 3,4.758770483143634 3.368080573302675 3,4.845046783753275 3.102549423267999 3,4.912590402935223 2.831646763271039 3,4.961072274966281 2.556692403840263 3,4.990256201039298 2.279025894976499 3,5.0 2.0 3)' ):
        return 'fail'
    ds = None

    return 'success'


###############################################################################
# TEXT

def ogr_dxf_22():

    # Read TEXT feature
    ds = ogr.Open('data/text.dxf')
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('Text') != 'test_text':
        gdaltest.post_reason('bad attribute')
        return 'fail'
    style = feat.GetStyleString()
    if style != 'LABEL(f:"Arial",t:"test_text",a:45,s:10g,c:#ff0000)':
        gdaltest.post_reason('bad style')
        print(style)
        return 'fail'
    if ogrtest.check_feature_geometry( feat, 'POINT(1 2 3)' ):
        gdaltest.post_reason('bad geometry')
        return 'fail'

    # Write text feature
    out_ds = ogr.GetDriverByName('DXF').CreateDataSource('/vsimem/ogr_dxf_22.dxf')
    out_lyr = out_ds.CreateLayer( 'entities' )
    out_feat = ogr.Feature(out_lyr.GetLayerDefn())
    out_feat.SetStyleString(style)
    out_feat.SetGeometry(feat.GetGeometryRef())
    out_lyr.CreateFeature(out_feat)
    out_feat = None
    out_lyr = None
    out_ds = None

    ds = None

    # Check written file
    ds = ogr.Open('/vsimem/ogr_dxf_22.dxf')
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('Text') != 'test_text':
        gdaltest.post_reason('bad attribute')
        return 'fail'
    style = feat.GetStyleString()
    if style != 'LABEL(f:"Arial",t:"test_text",a:45,s:10g,c:#ff0000)':
        gdaltest.post_reason('bad style')
        print(style)
        return 'fail'
    if ogrtest.check_feature_geometry( feat, 'POINT(1 2 3)' ):
        gdaltest.post_reason('bad geometry')
        return 'fail'

    ds = None

    gdal.Unlink('/vsimem/ogr_dxf_22.dxf')

    return 'success'


###############################################################################
# POLYGON with hole

def ogr_dxf_23():

    # Write polygon
    out_ds = ogr.GetDriverByName('DXF').CreateDataSource('/vsimem/ogr_dxf_23.dxf')
    out_lyr = out_ds.CreateLayer( 'entities' )
    out_feat = ogr.Feature(out_lyr.GetLayerDefn())
    out_feat.SetStyleString('BRUSH(fc:#ff0000)')
    wkt = 'POLYGON ((0 0,0 10,10 10,10 0,0 0),(1 1,1 9,9 9,9 1,1 1))'
    out_feat.SetGeometry(ogr.CreateGeometryFromWkt(wkt))
    out_lyr.CreateFeature(out_feat)
    out_feat = None
    out_lyr = None
    out_ds = None

    ds = None

    # Check written file
    ds = ogr.Open('/vsimem/ogr_dxf_23.dxf')
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    style = feat.GetStyleString()
    if style != 'BRUSH(fc:#ff0000)':
        gdaltest.post_reason('bad style')
        print(style)
        return 'fail'
    if ogrtest.check_feature_geometry( feat, wkt ):
        gdaltest.post_reason('bad geometry')
        return 'fail'

    ds = None

    gdal.Unlink('/vsimem/ogr_dxf_23.dxf')

    return 'success'

###############################################################################
# HATCH

def ogr_dxf_24():

    ds = ogr.Open('data/hatch.dxf')
    lyr = ds.GetLayer(0)

    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    feat = lyr.GetNextFeature()
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    if ogrtest.check_feature_geometry( feat, 'POLYGON ((2 1,1.646446609406726 0.853553390593274,1.5 0.5,1.646446609406726 0.146446609406726,2 0,2.0 0.0,2.146446609406726 -0.353553390593274,2.5 -0.5,2.853553390593274 -0.353553390593274,3.0 -0.0,3 0,3.353553390593274 0.146446609406726,3.5 0.5,3.353553390593274 0.853553390593273,3 1,2.853553390593274 1.353553390593274,2.5 1.5,2.146446609406726 1.353553390593274,2 1))' ):
        return 'fail'

    gdal.SetConfigOption('OGR_ARC_STEPSIZE', '45')
    feat = lyr.GetNextFeature()
    gdal.SetConfigOption('OGR_ARC_STEPSIZE', None)
    if ogrtest.check_feature_geometry( feat, 'POLYGON ((0.0 0.0 0,-0.353553390593274 0.146446609406726 0,-0.5 0.5 0,-0.353553390593274 0.853553390593274 0,-0.0 1.0 0,0.0 1.0 0,0.146446609406726 1.353553390593274 0,0.5 1.5 0,0.853553390593274 1.353553390593274 0,1.0 1.0 0,1.0 1.0 0,1.353553390593274 0.853553390593274 0,1.5 0.5 0,1.353553390593274 0.146446609406727 0,1.0 0.0 0,1 0 0,0.853553390593274 -0.353553390593274 0,0.5 -0.5 0,0.146446609406726 -0.353553390593274 0,0.0 -0.0 0,0.0 0.0 0))' ):
        return 'fail'

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON ((-1 -1,-1 0,0 0,-1 -1))' ):
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# 3DFACE

def ogr_dxf_25():

    ds = ogr.Open('data/3dface.dxf')
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON ((10 20 30,11 21 31,12 22 32,10 20 30))' ):
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON ((10 20 30,11 21 31,12 22 32,13 23 33,10 20 30))' ):
        feat.DumpReadable()
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# SOLID (#5380)

def ogr_dxf_26():

    ds = ogr.Open('data/solid.dxf')
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON ((2.716846 2.762514,2.393674 1.647962,4.391042 1.06881,4.714214 2.183362,2.716846 2.762514))' ):
        feat.DumpReadable()
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test reading a DXF file without .dxf extensions (#5994)

def ogr_dxf_27():

    gdal.FileFromMemBuffer('/vsimem/a_dxf_without_extension', open('data/solid.dxf').read())

    ds = ogr.Open('/vsimem/a_dxf_without_extension')
    if ds is None:
        return 'fail'

    gdal.Unlink('/vsimem/a_dxf_without_extension')

    return 'success'

###############################################################################
# Test reading a ELLIPSE with Z extrusion axis value of -1.0 (#5075)

def ogr_dxf_28():

    ds = ogr.Open('data/ellipse_z_extrusion_minus_1.dxf')
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (247.379588068074 525.677518653024 0,247.560245171261 525.685592896308 0,247.739941456101 525.705876573267 0,247.917852718752 525.738276649788 0,248.09316294264 525.782644518081 0,248.265068041245 525.838776678293 0,248.432779546163 525.90641567189 0,248.595528223532 525.985251262527 0,248.752567602242 526.074921858996 0,248.903177397731 526.175016173715 0,249.046666815684 526.285075109164 0,249.182377720457 526.404593863601 0,249.309687653722 526.533024246411 0,249.428012689458 526.669777192466 0,249.536810112221 526.814225463957 0,249.635580906384 526.965706527318 0,249.723872044951 527.123525592032 0)' ):
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (290.988651614349 531.01336644407 0,290.900681473157 531.171364661134 0,290.823607338001 531.334954880971 0,290.757782720911 531.503386772611 0,290.703509536322 531.675887798031 0,290.661036716299 531.85166675552 0,290.630559068775 532.029917408641 0,290.612216384031 532.209822184155 0,290.606092793529 532.390555921946 0,290.612216384031 532.571289659737 0,290.630559068775 532.751194435252 0,290.661036716299 532.929445088373 0,290.703509536321 533.105224045862 0,290.75778272091 533.277725071282 0,290.823607338 533.446156962922 0,290.900681473156 533.60974718276 0,290.988651614348 533.767745399824 0)' ):
        feat.DumpReadable()
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# SPLINE with weights

def ogr_dxf_29():

    ds = ogr.Open('data/spline_weight.dxf')
    lyr = ds.GetLayer(0)

    # spline 227, no weight
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (2 2, 2.10256409645081 2.15371131896973, 2.20512819290161 2.3066132068634, 2.307692527771 2.4578971862793, 2.41025638580322 2.6067533493042, 2.51282024383545 2.75237274169922, 2.61538457870483 2.89394640922546, 2.71794891357422 3.03066444396973, 2.82051301002502 3.16171884536743, 2.92307710647583 3.28629970550537, 3.02564096450806 3.40359783172607, 3.12820529937744 3.51280379295349, 3.23076939582825 3.61310863494873, 3.33333325386047 3.70370388031006, 3.43589782714844 3.78377938270569, 3.53846144676208 3.85252642631531, 3.64102602005005 3.90913581848145, 3.74358987808228 3.95279765129089, 3.84615445137024 3.98270392417908, 3.94871830940247 3.99804472923279, 4.05128240585327 3.99804425239563, 4.15384674072266 3.9827036857605, 4.25641107559204 3.95279765129089, 4.35897541046143 3.90913534164429, 4.46153926849365 3.85252571105957, 4.56410360336304 3.78377866744995, 4.66666793823242 3.70370292663574, 4.76923179626465 3.61310815811157, 4.87179613113403 3.51280236244202, 4.97436046600342 3.40359592437744, 5.07692432403564 3.2862982749939, 5.17948865890503 3.16171741485596, 5.28205299377441 3.03066277503967, 5.38461685180664 2.89394426345825, 5.48718070983887 2.75237035751343, 5.58974552154541 2.60675096511841, 5.69230937957764 2.45789456367493, 5.79487323760986 2.30661058425903, 5.89743757247925 2.15370845794678, 6 2)' ):
        feat.DumpReadable()
        return 'fail'

    # spline 261, weight(3) = 2.0
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (2 2, 2.10976576805115 2.16451454162598, 2.23113083839417 2.34563326835632, 2.35994720458984 2.53639197349548, 2.49239826202393 2.73041176795959, 2.62522411346436 2.92225241661072, 2.75582838058472 3.10756635665894, 2.88229131698608 3.2831084728241, 3.00331926345825 3.44663000106812, 3.11815142631531 3.59672045707703, 3.2264621257782 3.73262500762939, 3.32825922966003 3.85407567024231, 3.42379808425903 3.96113181114197, 3.51351404190063 4.05405473709106, 3.59796214103699 4.13319540023804, 3.67778849601746 4.19891929626465, 3.75369834899902 4.25152969360352, 3.82645153999329 4.29121112823486, 3.89685702323914 4.31797361373901, 3.96579027175903 4.33159732818604, 4.03421020507812 4.33159732818604, 4.1031436920166 4.31797361373901, 4.17354917526245 4.29121017456055, 4.24630165100098 4.2515287399292, 4.32221221923828 4.19891929626465, 4.40203857421875 4.13319492340088, 4.48648738861084 4.05405378341675, 4.57620286941528 3.96113133430481, 4.67174291610718 3.85407471656799, 4.77353954315186 3.73262333869934, 4.88184928894043 3.59671831130981, 4.99668216705322 3.44662809371948, 5.11771011352539 3.28310608863831, 5.24417400360107 3.10756373405457, 5.37477827072144 2.92224931716919, 5.50760412216187 2.73040890693665, 5.64005517959595 2.5363883972168, 5.76887130737305 2.34562969207764, 5.89023685455322 2.16451120376587, 6 2)' ):
        feat.DumpReadable()
        return 'fail'

    # spline 262, weight(3) = 0.5
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (2 2, 2.09894275665283 2.14827871322632, 2.19183802604675 2.28667020797729, 2.28029608726501 2.41674375534058, 2.36573505401611 2.53972935676575, 2.4494321346283 2.65657186508179, 2.53256177902222 2.76796913146973, 2.61621570587158 2.87439489364624, 2.70142364501953 2.97611308097839, 2.78915071487427 3.07318329811096, 2.88029623031616 3.16546249389648, 2.97567653656006 3.25260472297668, 3.07599782943726 3.33406257629395, 3.18181824684143 3.40909075737, 3.29349946975708 3.4767644405365, 3.4111499786377 3.53600478172302, 3.53456783294678 3.58562660217285, 3.66318273544312 3.62440752983093, 3.79601716995239 3.6511766910553, 3.93166828155518 3.66492199897766, 4.06833267211914 3.66492199897766, 4.20398426055908 3.65117692947388, 4.33681774139404 3.62440729141235, 4.4654335975647 3.58562660217285, 4.58885097503662 3.53600406646729, 4.70650196075439 3.47676372528076, 4.81818294525146 3.40909028053284, 4.92400360107422 3.33406162261963, 5.02432489395142 3.25260376930237, 5.11970520019531 3.16546106338501, 5.21085071563721 3.07318210601807, 5.29857730865479 2.9761118888855, 5.38378524780273 2.87439346313477, 5.46744012832642 2.76796770095825, 5.55056858062744 2.65656995773315, 5.63426637649536 2.53972721099854, 5.71970558166504 2.41674160957336, 5.8081636428833 2.2866678237915, 5.9010591506958 2.14827609062195, 6 2)' ):
        feat.DumpReadable()
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# SPLINE closed

def ogr_dxf_30():

    ds = ogr.Open('data/spline_closed.dxf')
    lyr = ds.GetLayer(0)

    # spline 24b, closed
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (14 2, 13.9043273925781 2.11115527153015, 13.82310962677 2.23728489875793, 13.7564849853516 2.3759388923645, 13.7045974731445 2.52466750144958, 13.6675891876221 2.68102264404297, 13.6455984115601 2.84255337715149, 13.6387672424316 3.00681042671204, 13.6472396850586 3.17134499549866, 13.6711559295654 3.33370733261108, 13.7106552124023 3.49144792556763, 13.7658815383911 3.64211750030518, 13.8369770050049 3.78326630592346, 13.9240808486938 3.9124448299408, 14.0273275375366 4.0272102355957, 14.1460762023926 4.125901222229, 14.2781581878662 4.20836925506592, 14.4212408065796 4.27464532852173, 14.5729866027832 4.32475566864014, 14.7310619354248 4.35873079299927, 14.8931264877319 4.37659645080566, 15.056845664978 4.37838315963745, 15.2198839187622 4.36411762237549, 15.3799018859863 4.33382892608643, 15.5345678329468 4.2875452041626, 15.681544303894 4.22529458999634, 15.8184909820557 4.14710569381714, 15.9430751800537 4.05300617218018, 16.0530071258545 3.94307255744934, 16.1471080780029 3.81848883628845, 16.2252960205078 3.68154096603394, 16.2875461578369 3.53456616401672, 16.3338298797607 3.37990093231201, 16.3641166687012 3.219881772995, 16.3783836364746 3.05684399604797, 16.3765964508057 2.89312481880188, 16.3587284088135 2.73106050491333, 16.3247547149658 2.57298684120178, 16.2746448516846 2.42124080657959, 16.2083702087402 2.27815842628479, 16.1259021759033 2.146075963974, 16.0272102355957 2.02732920646667, 15.9124450683594 1.92408156394958, 15.7832660675049 1.83697760105133, 15.6421175003052 1.7658828496933, 15.4914503097534 1.71065592765808, 15.3337097167969 1.67115533351898, 15.1713457107544 1.6472395658493, 15.0068111419678 1.63876724243164, 14.8425559997559 1.64559710025787, 14.681022644043 1.66758728027344, 14.5246696472168 1.70459699630737, 14.375940322876 1.75648427009583, 14.2372856140137 1.82310783863068, 14.1111574172974 1.90432631969452, 14 2)' ):
        feat.DumpReadable()
        return 'fail'

    # spline 24c, closed, recalculate knots
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (14 2, 13.9043273925781 2.11115527153015, 13.82310962677 2.23728489875793, 13.7564849853516 2.3759388923645, 13.7045974731445 2.52466750144958, 13.6675891876221 2.68102264404297, 13.6455984115601 2.84255337715149, 13.6387672424316 3.00681042671204, 13.6472396850586 3.17134499549866, 13.6711559295654 3.33370733261108, 13.7106552124023 3.49144792556763, 13.7658815383911 3.64211750030518, 13.8369770050049 3.78326630592346, 13.9240808486938 3.9124448299408, 14.0273275375366 4.0272102355957, 14.1460762023926 4.125901222229, 14.2781581878662 4.20836925506592, 14.4212408065796 4.27464532852173, 14.5729866027832 4.32475566864014, 14.7310619354248 4.35873079299927, 14.8931264877319 4.37659645080566, 15.056845664978 4.37838315963745, 15.2198839187622 4.36411762237549, 15.3799018859863 4.33382892608643, 15.5345678329468 4.2875452041626, 15.681544303894 4.22529458999634, 15.8184909820557 4.14710569381714, 15.9430751800537 4.05300617218018, 16.0530071258545 3.94307255744934, 16.1471080780029 3.81848883628845, 16.2252960205078 3.68154096603394, 16.2875461578369 3.53456616401672, 16.3338298797607 3.37990093231201, 16.3641166687012 3.219881772995, 16.3783836364746 3.05684399604797, 16.3765964508057 2.89312481880188, 16.3587284088135 2.73106050491333, 16.3247547149658 2.57298684120178, 16.2746448516846 2.42124080657959, 16.2083702087402 2.27815842628479, 16.1259021759033 2.146075963974, 16.0272102355957 2.02732920646667, 15.9124450683594 1.92408156394958, 15.7832660675049 1.83697760105133, 15.6421175003052 1.7658828496933, 15.4914503097534 1.71065592765808, 15.3337097167969 1.67115533351898, 15.1713457107544 1.6472395658493, 15.0068111419678 1.63876724243164, 14.8425559997559 1.64559710025787, 14.681022644043 1.66758728027344, 14.5246696472168 1.70459699630737, 14.375940322876 1.75648427009583, 14.2372856140137 1.82310783863068, 14.1111574172974 1.90432631969452, 14 2)' ):
        feat.DumpReadable()
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# OCS2WCS transformations 1

def ogr_dxf_31():

    ds = ogr.Open('data/ocs2wcs1.dxf')
    lyr = ds.GetLayer(0)

# INFO: Open of `ocs2wcs1.dxf' using driver `DXF' successful.

# OGRFeature(entities):0
#   EntityHandle (String) = 1EF
#   POINT Z (4 4 0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POINT Z (4 4 0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):1
#   EntityHandle (String) = 1F0
#   LINESTRING Z (0 0 0,1 1 0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (0 0 0,1 1 0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):2
#   EntityHandle (String) = 1F1
#   LINESTRING (1 1,2 1,1 2,1 1)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (1 1,2 1,1 2,1 1)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):3
#   EntityHandle (String) = 1F2
#   LINESTRING Z (1 1 0,1 2 0,2 2 0,1 1 0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (1 1 0,1 2 0,2 2 0,1 1 0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):4
#   EntityHandle (String) = 1F7
#   LINESTRING Z (6 4 0,5.99512810051965 3.86048705251175 0,5.98053613748314 3.72165379807987 0,5.95629520146761 3.58417661836448 0,5.92252339187664 3.448725288366 0,5.87938524157182 3.31595971334866 0,5.8270909152852 3.1865267138484 0,5.76589518571785 3.06105687442822 0,5.69609619231285 2.94016147153359 0,5.61803398874989 2.82442949541505 0,5.53208888623796 2.71442478062692 0,5.4386796006773 2.61068325908201 0,5.33826121271772 2.51371034904521 0,5.23132295065132 2.42397849278656 0,5.11838580694149 2.34192485488992 0,5.0 2.26794919243112 0,4.87674229357815 2.20241190740167 0,4.74921318683182 2.14563229086643 0,4.61803398874989 2.09788696740969 0,4.48384379119934 2.05940854744801 0,4.34729635533386 2.03038449397558 0,4.20905692653531 2.01095620926345 0,4.069798993405 2.00121834596181 0,3.930201006595 2.00121834596181 0,3.79094307346469 2.01095620926345 0,3.65270364466614 2.03038449397558 0,3.51615620880066 2.05940854744801 0,3.38196601125011 2.09788696740969 0,3.25078681316818 2.14563229086643 0,3.12325770642185 2.20241190740167 0,3.0 2.26794919243112 0,2.88161419305851 2.34192485488992 0,2.76867704934868 2.42397849278656 0,2.66173878728228 2.51371034904521 0,2.5613203993227 2.61068325908201 0,2.46791111376204 2.71442478062692 0,2.38196601125011 2.82442949541505 0,2.30390380768715 2.94016147153359 0,2.23410481428215 3.06105687442822 0,2.1729090847148 3.1865267138484 0,2.12061475842818 3.31595971334866 0,2.07747660812336 3.448725288366 0,2.04370479853239 3.58417661836448 0,2.01946386251686 3.72165379807987 0,2.00487189948035 3.86048705251175 0,2.0 4.0 0,2.00487189948035 4.13951294748825 0,2.01946386251686 4.27834620192013 0,2.04370479853239 4.41582338163552 0,2.07747660812336 4.551274711634 0,2.12061475842818 4.68404028665134 0,2.1729090847148 4.8134732861516 0,2.23410481428215 4.93894312557178 0,2.30390380768715 5.05983852846641 0,2.38196601125011 5.17557050458495 0,2.46791111376204 5.28557521937308 0,2.5613203993227 5.38931674091799 0,2.66173878728228 5.48628965095479 0,2.76867704934868 5.57602150721344 0,2.88161419305851 5.65807514511008 0,3.0 5.73205080756888 0,3.12325770642184 5.79758809259833 0,3.25078681316818 5.85436770913357 0,3.38196601125011 5.90211303259031 0,3.51615620880066 5.94059145255199 0,3.65270364466614 5.96961550602442 0,3.79094307346469 5.98904379073655 0,3.930201006595 5.99878165403819 0,4.069798993405 5.99878165403819 0,4.20905692653531 5.98904379073655 0,4.34729635533386 5.96961550602442 0,4.48384379119933 5.94059145255199 0,4.61803398874989 5.90211303259031 0,4.74921318683182 5.85436770913357 0,4.87674229357815 5.79758809259833 0,5.0 5.73205080756888 0,5.11838580694149 5.65807514511008 0,5.23132295065132 5.57602150721344 0,5.33826121271772 5.48628965095479 0,5.4386796006773 5.389316740918 0,5.53208888623796 5.28557521937308 0,5.61803398874989 5.17557050458495 0,5.69609619231285 5.05983852846641 0,5.76589518571785 4.93894312557178 0,5.8270909152852 4.8134732861516 0,5.87938524157182 4.68404028665134 0,5.92252339187664 4.551274711634 0,5.95629520146761 4.41582338163552 0,5.98053613748314 4.27834620192013 0,5.99512810051965 4.13951294748825 0,6.0 4.0 0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (6 4 0,5.99512810051965 3.86048705251175 0,5.98053613748314 3.72165379807987 0,5.95629520146761 3.58417661836448 0,5.92252339187664 3.448725288366 0,5.87938524157182 3.31595971334866 0,5.8270909152852 3.1865267138484 0,5.76589518571785 3.06105687442822 0,5.69609619231285 2.94016147153359 0,5.61803398874989 2.82442949541505 0,5.53208888623796 2.71442478062692 0,5.4386796006773 2.61068325908201 0,5.33826121271772 2.51371034904521 0,5.23132295065132 2.42397849278656 0,5.11838580694149 2.34192485488992 0,5.0 2.26794919243112 0,4.87674229357815 2.20241190740167 0,4.74921318683182 2.14563229086643 0,4.61803398874989 2.09788696740969 0,4.48384379119934 2.05940854744801 0,4.34729635533386 2.03038449397558 0,4.20905692653531 2.01095620926345 0,4.069798993405 2.00121834596181 0,3.930201006595 2.00121834596181 0,3.79094307346469 2.01095620926345 0,3.65270364466614 2.03038449397558 0,3.51615620880066 2.05940854744801 0,3.38196601125011 2.09788696740969 0,3.25078681316818 2.14563229086643 0,3.12325770642185 2.20241190740167 0,3.0 2.26794919243112 0,2.88161419305851 2.34192485488992 0,2.76867704934868 2.42397849278656 0,2.66173878728228 2.51371034904521 0,2.5613203993227 2.61068325908201 0,2.46791111376204 2.71442478062692 0,2.38196601125011 2.82442949541505 0,2.30390380768715 2.94016147153359 0,2.23410481428215 3.06105687442822 0,2.1729090847148 3.1865267138484 0,2.12061475842818 3.31595971334866 0,2.07747660812336 3.448725288366 0,2.04370479853239 3.58417661836448 0,2.01946386251686 3.72165379807987 0,2.00487189948035 3.86048705251175 0,2.0 4.0 0,2.00487189948035 4.13951294748825 0,2.01946386251686 4.27834620192013 0,2.04370479853239 4.41582338163552 0,2.07747660812336 4.551274711634 0,2.12061475842818 4.68404028665134 0,2.1729090847148 4.8134732861516 0,2.23410481428215 4.93894312557178 0,2.30390380768715 5.05983852846641 0,2.38196601125011 5.17557050458495 0,2.46791111376204 5.28557521937308 0,2.5613203993227 5.38931674091799 0,2.66173878728228 5.48628965095479 0,2.76867704934868 5.57602150721344 0,2.88161419305851 5.65807514511008 0,3.0 5.73205080756888 0,3.12325770642184 5.79758809259833 0,3.25078681316818 5.85436770913357 0,3.38196601125011 5.90211303259031 0,3.51615620880066 5.94059145255199 0,3.65270364466614 5.96961550602442 0,3.79094307346469 5.98904379073655 0,3.930201006595 5.99878165403819 0,4.069798993405 5.99878165403819 0,4.20905692653531 5.98904379073655 0,4.34729635533386 5.96961550602442 0,4.48384379119933 5.94059145255199 0,4.61803398874989 5.90211303259031 0,4.74921318683182 5.85436770913357 0,4.87674229357815 5.79758809259833 0,5.0 5.73205080756888 0,5.11838580694149 5.65807514511008 0,5.23132295065132 5.57602150721344 0,5.33826121271772 5.48628965095479 0,5.4386796006773 5.389316740918 0,5.53208888623796 5.28557521937308 0,5.61803398874989 5.17557050458495 0,5.69609619231285 5.05983852846641 0,5.76589518571785 4.93894312557178 0,5.8270909152852 4.8134732861516 0,5.87938524157182 4.68404028665134 0,5.92252339187664 4.551274711634 0,5.95629520146761 4.41582338163552 0,5.98053613748314 4.27834620192013 0,5.99512810051965 4.13951294748825 0,6.0 4.0 0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):5
#   EntityHandle (String) = 1F8
#   LINESTRING Z (2 4 0,2.00487189948035 4.06975647374412 0,2.01946386251686 4.13917310096007 0,2.04370479853239 4.20791169081776 0,2.07747660812336 4.275637355817 0,2.12061475842818 4.34202014332567 0,2.1729090847148 4.4067366430758 0,2.23410481428215 4.46947156278589 0,2.30390380768715 4.52991926423321 0,2.38196601125011 4.58778525229247 0,2.46791111376204 4.64278760968654 0,2.5613203993227 4.694658370459 0,2.66173878728228 4.74314482547739 0,2.76867704934868 4.78801075360672 0,2.88161419305851 4.82903757255504 0,3.0 4.86602540378444 0,3.12325770642185 4.89879404629917 0,3.25078681316818 4.92718385456679 0,3.38196601125011 4.95105651629515 0,3.51615620880067 4.970295726276 0,3.65270364466614 4.98480775301221 0,3.79094307346469 4.99452189536827 0,3.930201006595 4.9993908270191 0,4.069798993405 4.9993908270191 0,4.20905692653531 4.99452189536827 0,4.34729635533386 4.98480775301221 0,4.48384379119934 4.970295726276 0,4.61803398874989 4.95105651629515 0,4.74921318683182 4.92718385456679 0,4.87674229357816 4.89879404629917 0,5.0 4.86602540378444 0,5.11838580694149 4.82903757255504 0,5.23132295065132 4.78801075360672 0,5.33826121271772 4.74314482547739 0,5.4386796006773 4.694658370459 0,5.53208888623796 4.64278760968654 0,5.61803398874989 4.58778525229247 0,5.69609619231285 4.5299192642332 0,5.76589518571785 4.46947156278589 0,5.8270909152852 4.4067366430758 0,5.87938524157182 4.34202014332567 0,5.92252339187664 4.275637355817 0,5.95629520146761 4.20791169081776 0,5.98053613748314 4.13917310096006 0,5.99512810051965 4.06975647374412 0,6.0 4.0 0,5.99512810051965 3.93024352625587 0,5.98053613748314 3.86082689903993 0,5.95629520146761 3.79208830918224 0,5.92252339187664 3.724362644183 0,5.87938524157182 3.65797985667433 0,5.8270909152852 3.5932633569242 0,5.76589518571785 3.53052843721411 0,5.69609619231285 3.4700807357668 0,5.61803398874989 3.41221474770753 0,5.53208888623796 3.35721239031346 0,5.4386796006773 3.305341629541 0,5.33826121271772 3.25685517452261 0,5.23132295065132 3.21198924639328 0,5.11838580694149 3.17096242744496 0,5.0 3.13397459621556 0,4.87674229357815 3.10120595370083 0,4.74921318683182 3.07281614543321 0,4.61803398874989 3.04894348370485 0,4.48384379119934 3.029704273724 0,4.34729635533386 3.01519224698779 0,4.20905692653531 3.00547810463173 0,4.069798993405 3.0006091729809 0,3.930201006595 3.0006091729809 0,3.79094307346469 3.00547810463173 0,3.65270364466614 3.01519224698779 0,3.51615620880066 3.029704273724 0,3.38196601125011 3.04894348370485 0,3.25078681316818 3.07281614543321 0,3.12325770642185 3.10120595370083 0,3.0 3.13397459621556 0,2.88161419305851 3.17096242744496 0,2.76867704934868 3.21198924639328 0,2.66173878728228 3.25685517452261 0,2.5613203993227 3.305341629541 0,2.46791111376204 3.35721239031346 0,2.38196601125011 3.41221474770753 0,2.30390380768715 3.4700807357668 0,2.23410481428215 3.53052843721411 0,2.1729090847148 3.5932633569242 0,2.12061475842818 3.65797985667433 0,2.07747660812336 3.724362644183 0,2.04370479853239 3.79208830918224 0,2.01946386251686 3.86082689903993 0,2.00487189948035 3.93024352625587 0,2 4 0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (2 4 0,2.00487189948035 4.06975647374412 0,2.01946386251686 4.13917310096007 0,2.04370479853239 4.20791169081776 0,2.07747660812336 4.275637355817 0,2.12061475842818 4.34202014332567 0,2.1729090847148 4.4067366430758 0,2.23410481428215 4.46947156278589 0,2.30390380768715 4.52991926423321 0,2.38196601125011 4.58778525229247 0,2.46791111376204 4.64278760968654 0,2.5613203993227 4.694658370459 0,2.66173878728228 4.74314482547739 0,2.76867704934868 4.78801075360672 0,2.88161419305851 4.82903757255504 0,3.0 4.86602540378444 0,3.12325770642185 4.89879404629917 0,3.25078681316818 4.92718385456679 0,3.38196601125011 4.95105651629515 0,3.51615620880067 4.970295726276 0,3.65270364466614 4.98480775301221 0,3.79094307346469 4.99452189536827 0,3.930201006595 4.9993908270191 0,4.069798993405 4.9993908270191 0,4.20905692653531 4.99452189536827 0,4.34729635533386 4.98480775301221 0,4.48384379119934 4.970295726276 0,4.61803398874989 4.95105651629515 0,4.74921318683182 4.92718385456679 0,4.87674229357816 4.89879404629917 0,5.0 4.86602540378444 0,5.11838580694149 4.82903757255504 0,5.23132295065132 4.78801075360672 0,5.33826121271772 4.74314482547739 0,5.4386796006773 4.694658370459 0,5.53208888623796 4.64278760968654 0,5.61803398874989 4.58778525229247 0,5.69609619231285 4.5299192642332 0,5.76589518571785 4.46947156278589 0,5.8270909152852 4.4067366430758 0,5.87938524157182 4.34202014332567 0,5.92252339187664 4.275637355817 0,5.95629520146761 4.20791169081776 0,5.98053613748314 4.13917310096006 0,5.99512810051965 4.06975647374412 0,6.0 4.0 0,5.99512810051965 3.93024352625587 0,5.98053613748314 3.86082689903993 0,5.95629520146761 3.79208830918224 0,5.92252339187664 3.724362644183 0,5.87938524157182 3.65797985667433 0,5.8270909152852 3.5932633569242 0,5.76589518571785 3.53052843721411 0,5.69609619231285 3.4700807357668 0,5.61803398874989 3.41221474770753 0,5.53208888623796 3.35721239031346 0,5.4386796006773 3.305341629541 0,5.33826121271772 3.25685517452261 0,5.23132295065132 3.21198924639328 0,5.11838580694149 3.17096242744496 0,5.0 3.13397459621556 0,4.87674229357815 3.10120595370083 0,4.74921318683182 3.07281614543321 0,4.61803398874989 3.04894348370485 0,4.48384379119934 3.029704273724 0,4.34729635533386 3.01519224698779 0,4.20905692653531 3.00547810463173 0,4.069798993405 3.0006091729809 0,3.930201006595 3.0006091729809 0,3.79094307346469 3.00547810463173 0,3.65270364466614 3.01519224698779 0,3.51615620880066 3.029704273724 0,3.38196601125011 3.04894348370485 0,3.25078681316818 3.07281614543321 0,3.12325770642185 3.10120595370083 0,3.0 3.13397459621556 0,2.88161419305851 3.17096242744496 0,2.76867704934868 3.21198924639328 0,2.66173878728228 3.25685517452261 0,2.5613203993227 3.305341629541 0,2.46791111376204 3.35721239031346 0,2.38196601125011 3.41221474770753 0,2.30390380768715 3.4700807357668 0,2.23410481428215 3.53052843721411 0,2.1729090847148 3.5932633569242 0,2.12061475842818 3.65797985667433 0,2.07747660812336 3.724362644183 0,2.04370479853239 3.79208830918224 0,2.01946386251686 3.86082689903993 0,2.00487189948035 3.93024352625587 0,2 4 0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):6
#   EntityHandle (String) = 1F9
#   LINESTRING Z (2.0 2.0 0,1.96657794502105 2.03582232791524 0,1.93571660708646 2.07387296203834 0,1.90756413746468 2.11396923855471 0,1.88225568337755 2.15591867344963 0,1.85991273921989 2.19951988653655 0,1.84064256332004 2.24456356819194 0,1.8245376630414 2.29083348415575 0,1.81167535069652 2.33810751357387 0,1.80211737240583 2.38615871529951 0,1.79590961168258 2.43475641733454 0,1.79308186916688 2.48366732418105 0,1.79364771956639 2.53265663678705 0,1.79760444649032 2.58148917971011 0,1.80493305548955 2.62993053008785 0,1.81559836524041 2.67774814299566 0,1.8295491764342 2.72471246778926 0,1.84671851756181 2.7705980500731 0,1.86702396641357 2.81518461400453 0,1.89036804575079 2.85825811973811 0,1.91663869124976 2.89961179093366 0,1.94570978947168 2.93904710739563 0,1.97744178327594 2.97637475807832 0,2.01168234177068 3.01141554988232 0,2.04826709158413 3.04400126787917 0,2.08702040594658 3.07397548283483 0,2.12775624779472 3.10119430215541 0,2.17027906285109 3.12552706065018 0,2.2143847183914 3.14685694779575 0,2.25986148319297 3.16508156849045 0,2.30649104396024 3.18011343460661 0,2.35404955334774 3.1918803849814 0,2.40230870454951 3.20032593182975 0,2.45103682729644 3.2054095319166 0,2.5 3.20710678118655 0,2.54896317270356 3.2054095319166 0,2.59769129545049 3.20032593182975 0,2.64595044665226 3.1918803849814 0,2.69350895603976 3.18011343460661 0,2.74013851680703 3.16508156849045 0,2.7856152816086 3.14685694779575 0,2.8297209371489 3.12552706065018 0,2.87224375220528 3.10119430215541 0,2.91297959405342 3.07397548283483 0,2.95173290841587 3.04400126787917 0,2.98831765822932 3.01141554988232 0,3.02255821672406 2.97637475807832 0,3.05429021052832 2.93904710739563 0,3.08336130875024 2.89961179093367 0,3.10963195424921 2.85825811973811 0,3.13297603358643 2.81518461400453 0,3.15328148243819 2.7705980500731 0,3.1704508235658 2.72471246778926 0,3.18440163475959 2.67774814299567 0,3.19506694451045 2.62993053008786 0,3.20239555350968 2.58148917971011 0,3.20635228043361 2.53265663678705 0,3.20691813083312 2.48366732418105 0,3.20409038831742 2.43475641733454 0,3.19788262759417 2.38615871529951 0,3.18832464930348 2.33810751357387 0,3.1754623369586 2.29083348415575 0,3.15935743667996 2.24456356819194 0,3.14008726078011 2.19951988653655 0,3.11774431662245 2.15591867344963 0,3.09243586253532 2.11396923855472 0,3.06428339291354 2.07387296203834 0,3.03342205497895 2.03582232791524 0,3 2 0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (2.0 2.0 0,1.96657794502105 2.03582232791524 0,1.93571660708646 2.07387296203834 0,1.90756413746468 2.11396923855471 0,1.88225568337755 2.15591867344963 0,1.85991273921989 2.19951988653655 0,1.84064256332004 2.24456356819194 0,1.8245376630414 2.29083348415575 0,1.81167535069652 2.33810751357387 0,1.80211737240583 2.38615871529951 0,1.79590961168258 2.43475641733454 0,1.79308186916688 2.48366732418105 0,1.79364771956639 2.53265663678705 0,1.79760444649032 2.58148917971011 0,1.80493305548955 2.62993053008785 0,1.81559836524041 2.67774814299566 0,1.8295491764342 2.72471246778926 0,1.84671851756181 2.7705980500731 0,1.86702396641357 2.81518461400453 0,1.89036804575079 2.85825811973811 0,1.91663869124976 2.89961179093366 0,1.94570978947168 2.93904710739563 0,1.97744178327594 2.97637475807832 0,2.01168234177068 3.01141554988232 0,2.04826709158413 3.04400126787917 0,2.08702040594658 3.07397548283483 0,2.12775624779472 3.10119430215541 0,2.17027906285109 3.12552706065018 0,2.2143847183914 3.14685694779575 0,2.25986148319297 3.16508156849045 0,2.30649104396024 3.18011343460661 0,2.35404955334774 3.1918803849814 0,2.40230870454951 3.20032593182975 0,2.45103682729644 3.2054095319166 0,2.5 3.20710678118655 0,2.54896317270356 3.2054095319166 0,2.59769129545049 3.20032593182975 0,2.64595044665226 3.1918803849814 0,2.69350895603976 3.18011343460661 0,2.74013851680703 3.16508156849045 0,2.7856152816086 3.14685694779575 0,2.8297209371489 3.12552706065018 0,2.87224375220528 3.10119430215541 0,2.91297959405342 3.07397548283483 0,2.95173290841587 3.04400126787917 0,2.98831765822932 3.01141554988232 0,3.02255821672406 2.97637475807832 0,3.05429021052832 2.93904710739563 0,3.08336130875024 2.89961179093367 0,3.10963195424921 2.85825811973811 0,3.13297603358643 2.81518461400453 0,3.15328148243819 2.7705980500731 0,3.1704508235658 2.72471246778926 0,3.18440163475959 2.67774814299567 0,3.19506694451045 2.62993053008786 0,3.20239555350968 2.58148917971011 0,3.20635228043361 2.53265663678705 0,3.20691813083312 2.48366732418105 0,3.20409038831742 2.43475641733454 0,3.19788262759417 2.38615871529951 0,3.18832464930348 2.33810751357387 0,3.1754623369586 2.29083348415575 0,3.15935743667996 2.24456356819194 0,3.14008726078011 2.19951988653655 0,3.11774431662245 2.15591867344963 0,3.09243586253532 2.11396923855472 0,3.06428339291354 2.07387296203834 0,3.03342205497895 2.03582232791524 0,3 2 0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):7
#   EntityHandle (String) = 1FA
#   POLYGON Z ((1 2 0,1 3 0,2 3 0,2 2 0,1 2 0))
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON Z ((1 2 0,1 3 0,2 3 0,2 2 0,1 2 0))'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):8
#   EntityHandle (String) = 1FB
#   POLYGON ((3 4,4 4,4 3,3 3,3 4))
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON ((3 4,4 4,4 3,3 3,3 4))'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):9
#   EntityHandle (String) = 1FD
#   POLYGON ((8 8,9 8,9 9,8 9,8 8))
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON ((8 8,9 8,9 9,8 9,8 8))'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):10
#   EntityHandle (String) = 200
#   LINESTRING (2 2,2.15384615384615 2.15384615384615,2.30769230769231 2.30769230769231,2.46153846153846 2.46153846153846,2.61538461538461 2.61538461538461,2.76923076923077 2.76923076923077,2.92307692307692 2.92307692307692,3.07692307692308 3.07692307692308,3.23076923076923 3.23076923076923,3.38461538461538 3.38461538461538,3.53846153846154 3.53846153846154,3.69230769230769 3.69230769230769,3.84615384615385 3.84615384615385,4 4,4.15384615384615 4.15384615384615,4.30769230769231 4.30769230769231,4.46153846153846 4.46153846153846,4.61538461538462 4.61538461538462,4.76923076923077 4.76923076923077,4.92307692307692 4.92307692307692,5.07692307692308 5.07692307692308,5.23076923076923 5.23076923076923,5.38461538461538 5.38461538461538,5.53846153846154 5.53846153846154,5.69230769230769 5.69230769230769,5.84615384615385 5.84615384615385,6.0 6.0,6.15384615384615 6.15384615384615,6.30769230769231 6.30769230769231,6.46153846153846 6.46153846153846,6.61538461538462 6.61538461538462,6.76923076923077 6.76923076923077,6.92307692307692 6.92307692307692,7.07692307692308 7.07692307692308,7.23076923076923 7.23076923076923,7.38461538461539 7.38461538461539,7.53846153846154 7.53846153846154,7.69230769230769 7.69230769230769,7.84615384615385 7.84615384615385,8 8)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (2 2,2.15384615384615 2.15384615384615,2.30769230769231 2.30769230769231,2.46153846153846 2.46153846153846,2.61538461538461 2.61538461538461,2.76923076923077 2.76923076923077,2.92307692307692 2.92307692307692,3.07692307692308 3.07692307692308,3.23076923076923 3.23076923076923,3.38461538461538 3.38461538461538,3.53846153846154 3.53846153846154,3.69230769230769 3.69230769230769,3.84615384615385 3.84615384615385,4 4,4.15384615384615 4.15384615384615,4.30769230769231 4.30769230769231,4.46153846153846 4.46153846153846,4.61538461538462 4.61538461538462,4.76923076923077 4.76923076923077,4.92307692307692 4.92307692307692,5.07692307692308 5.07692307692308,5.23076923076923 5.23076923076923,5.38461538461538 5.38461538461538,5.53846153846154 5.53846153846154,5.69230769230769 5.69230769230769,5.84615384615385 5.84615384615385,6.0 6.0,6.15384615384615 6.15384615384615,6.30769230769231 6.30769230769231,6.46153846153846 6.46153846153846,6.61538461538462 6.61538461538462,6.76923076923077 6.76923076923077,6.92307692307692 6.92307692307692,7.07692307692308 7.07692307692308,7.23076923076923 7.23076923076923,7.38461538461539 7.38461538461539,7.53846153846154 7.53846153846154,7.69230769230769 7.69230769230769,7.84615384615385 7.84615384615385,8 8)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):11
#   EntityHandle (String) = 201
#   LINESTRING (8 1,7.62837370825536 0.987348067229724,7.25775889681215 0.975707614760869,6.88916704597178 0.966090122894857,6.52360963603567 0.959507071933107,6.16209814730525 0.956969942177043,5.80564406008193 0.959490213928084,5.45525885466714 0.968079367487651,5.11195401136229 0.983748883157167,4.77674101046882 1.00751024123805,4.45063133228814 1.04037492203173,4.13463645712167 1.08335440583961,3.82976786527082 1.13746017296313,3.53703703703704 1.2037037037037,3.25745545272173 1.28309647836275,2.99203459262631 1.37664997724169,2.74178593705221 1.48537568064195,2.50772096630085 1.61028506886495,2.29085116067365 1.75238962221211,2.09218800047203 1.91270082098484,1.91270082098485 2.09218800047202,1.75238962221211 2.29085116067364,1.61028506886495 2.50772096630085,1.48537568064195 2.74178593705221,1.37664997724169 2.99203459262631,1.28309647836275 3.25745545272172,1.2037037037037 3.53703703703703,1.13746017296313 3.82976786527082,1.08335440583961 4.13463645712166,1.04037492203173 4.45063133228814,1.00751024123805 4.77674101046882,0.983748883157167 5.11195401136229,0.968079367487652 5.45525885466714,0.959490213928084 5.80564406008193,0.956969942177043 6.16209814730525,0.959507071933108 6.52360963603567,0.966090122894857 6.88916704597178,0.975707614760869 7.25775889681216,0.987348067229724 7.62837370825537,1 8)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (8 1,7.62837370825536 0.987348067229724,7.25775889681215 0.975707614760869,6.88916704597178 0.966090122894857,6.52360963603567 0.959507071933107,6.16209814730525 0.956969942177043,5.80564406008193 0.959490213928084,5.45525885466714 0.968079367487651,5.11195401136229 0.983748883157167,4.77674101046882 1.00751024123805,4.45063133228814 1.04037492203173,4.13463645712167 1.08335440583961,3.82976786527082 1.13746017296313,3.53703703703704 1.2037037037037,3.25745545272173 1.28309647836275,2.99203459262631 1.37664997724169,2.74178593705221 1.48537568064195,2.50772096630085 1.61028506886495,2.29085116067365 1.75238962221211,2.09218800047203 1.91270082098484,1.91270082098485 2.09218800047202,1.75238962221211 2.29085116067364,1.61028506886495 2.50772096630085,1.48537568064195 2.74178593705221,1.37664997724169 2.99203459262631,1.28309647836275 3.25745545272172,1.2037037037037 3.53703703703703,1.13746017296313 3.82976786527082,1.08335440583961 4.13463645712166,1.04037492203173 4.45063133228814,1.00751024123805 4.77674101046882,0.983748883157167 5.11195401136229,0.968079367487652 5.45525885466714,0.959490213928084 5.80564406008193,0.956969942177043 6.16209814730525,0.959507071933108 6.52360963603567,0.966090122894857 6.88916704597178,0.975707614760869 7.25775889681216,0.987348067229724 7.62837370825537,1 8)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):12
#   EntityHandle (String) = 202
#   POINT Z (7 7 0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POINT Z (7 7 0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):13
#   EntityHandle (String) = 203
#   POINT Z (-4 4 -5e-16)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POINT Z (-4 4 -5e-16)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):14
#   EntityHandle (String) = 204
#   LINESTRING Z (0 0 0,-1 1 -1e-16)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (0 0 0,-1 1 -1e-16)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):15
#   EntityHandle (String) = 205
#   LINESTRING (-1 1,-2 1,-1 2,-1 1)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (-1 1,-2 1,-1 2,-1 1)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):16
#   EntityHandle (String) = 206
#   LINESTRING Z (-1 1 -1e-16,-1 2 -1e-16,-2 2 -2e-16,-1 1 -1e-16)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (-1 1 -1e-16,-1 2 -1e-16,-2 2 -2e-16,-1 1 -1e-16)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):17
#   EntityHandle (String) = 20B
#   LINESTRING Z (-6 4 -6e-16,-5.99512810051965 3.86048705251175 -5.99512810051965e-16,-5.98053613748314 3.72165379807987 -5.98053613748314e-16,-5.95629520146761 3.58417661836448 -5.95629520146761e-16,-5.92252339187664 3.448725288366 -5.92252339187664e-16,-5.87938524157182 3.31595971334866 -5.87938524157182e-16,-5.8270909152852 3.1865267138484 -5.8270909152852e-16,-5.76589518571785 3.06105687442822 -5.76589518571785e-16,-5.69609619231285 2.94016147153359 -5.69609619231285e-16,-5.61803398874989 2.82442949541505 -5.61803398874989e-16,-5.53208888623796 2.71442478062692 -5.53208888623796e-16,-5.4386796006773 2.61068325908201 -5.4386796006773e-16,-5.33826121271772 2.51371034904521 -5.33826121271772e-16,-5.23132295065132 2.42397849278656 -5.23132295065132e-16,-5.11838580694149 2.34192485488992 -5.11838580694149e-16,-5.0 2.26794919243112 -5e-16,-4.87674229357815 2.20241190740167 -4.87674229357815e-16,-4.74921318683182 2.14563229086643 -4.74921318683182e-16,-4.61803398874989 2.09788696740969 -4.61803398874989e-16,-4.48384379119934 2.05940854744801 -4.48384379119934e-16,-4.34729635533386 2.03038449397558 -4.34729635533386e-16,-4.20905692653531 2.01095620926345 -4.20905692653531e-16,-4.069798993405 2.00121834596181 -4.069798993405e-16,-3.930201006595 2.00121834596181 -3.930201006595e-16,-3.79094307346469 2.01095620926345 -3.79094307346469e-16,-3.65270364466614 2.03038449397558 -3.65270364466614e-16,-3.51615620880066 2.05940854744801 -3.51615620880066e-16,-3.38196601125011 2.09788696740969 -3.3819660112501e-16,-3.25078681316818 2.14563229086643 -3.25078681316818e-16,-3.12325770642185 2.20241190740167 -3.12325770642185e-16,-3.0 2.26794919243112 -3e-16,-2.88161419305851 2.34192485488992 -2.88161419305851e-16,-2.76867704934868 2.42397849278656 -2.76867704934868e-16,-2.66173878728228 2.51371034904521 -2.66173878728228e-16,-2.5613203993227 2.61068325908201 -2.5613203993227e-16,-2.46791111376204 2.71442478062692 -2.46791111376204e-16,-2.38196601125011 2.82442949541505 -2.3819660112501e-16,-2.30390380768715 2.94016147153359 -2.30390380768715e-16,-2.23410481428215 3.06105687442822 -2.23410481428215e-16,-2.1729090847148 3.1865267138484 -2.1729090847148e-16,-2.12061475842818 3.31595971334866 -2.12061475842818e-16,-2.07747660812336 3.448725288366 -2.07747660812336e-16,-2.04370479853239 3.58417661836448 -2.04370479853239e-16,-2.01946386251686 3.72165379807987 -2.01946386251686e-16,-2.00487189948035 3.86048705251175 -2.00487189948035e-16,-2.0 4.0 -2e-16,-2.00487189948035 4.13951294748825 -2.00487189948035e-16,-2.01946386251686 4.27834620192013 -2.01946386251686e-16,-2.04370479853239 4.41582338163552 -2.04370479853239e-16,-2.07747660812336 4.551274711634 -2.07747660812336e-16,
# -2.12061475842818 4.68404028665134 -2.12061475842818e-16,-2.1729090847148 4.8134732861516 -2.1729090847148e-16,-2.23410481428215 4.93894312557178 -2.23410481428215e-16,-2.30390380768715 5.05983852846641 -2.30390380768715e-16,-2.38196601125011 5.17557050458495 -2.3819660112501e-16,-2.46791111376204 5.28557521937308 -2.46791111376204e-16,-2.5613203993227 5.38931674091799 -2.5613203993227e-16,-2.66173878728228 5.48628965095479 -2.66173878728228e-16,-2.76867704934868 5.57602150721344 -2.76867704934868e-16,-2.88161419305851 5.65807514511008 -2.88161419305851e-16,-3.0 5.73205080756888 -3e-16,-3.12325770642184 5.79758809259833 -3.12325770642184e-16,-3.25078681316818 5.85436770913357 -3.25078681316817e-16,-3.38196601125011 5.90211303259031 -3.3819660112501e-16,-3.51615620880066 5.94059145255199 -3.51615620880066e-16,-3.65270364466614 5.96961550602442 -3.65270364466614e-16,-3.79094307346469 5.98904379073655 -3.79094307346469e-16,-3.930201006595 5.99878165403819 -3.930201006595e-16,-4.069798993405 5.99878165403819 -4.069798993405e-16,-4.20905692653531 5.98904379073655 -4.20905692653531e-16,-4.34729635533386 5.96961550602442 -4.34729635533386e-16,-4.48384379119933 5.94059145255199 -4.48384379119933e-16,-4.61803398874989 5.90211303259031 -4.61803398874989e-16,-4.74921318683182 5.85436770913357 -4.74921318683182e-16,-4.87674229357815 5.79758809259833 -4.87674229357815e-16,-5.0 5.73205080756888 -5e-16,-5.11838580694149 5.65807514511008 -5.11838580694149e-16,-5.23132295065132 5.57602150721344 -5.23132295065132e-16,-5.33826121271772 5.48628965095479 -5.33826121271772e-16,-5.4386796006773 5.389316740918 -5.4386796006773e-16,-5.53208888623796 5.28557521937308 -5.53208888623796e-16,-5.61803398874989 5.17557050458495 -5.61803398874989e-16,-5.69609619231285 5.05983852846641 -5.69609619231285e-16,-5.76589518571785 4.93894312557178 -5.76589518571785e-16,-5.8270909152852 4.8134732861516 -5.8270909152852e-16,-5.87938524157182 4.68404028665134 -5.87938524157182e-16,-5.92252339187664 4.551274711634 -5.92252339187664e-16,-5.95629520146761 4.41582338163552 -5.95629520146761e-16,-5.98053613748314 4.27834620192013 -5.98053613748314e-16,-5.99512810051965 4.13951294748825 -5.99512810051965e-16,-6.0 4.0 -6e-16)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (-6 4 -6e-16,-5.99512810051965 3.86048705251175 -5.99512810051965e-16,-5.98053613748314 3.72165379807987 -5.98053613748314e-16,-5.95629520146761 3.58417661836448 -5.95629520146761e-16,-5.92252339187664 3.448725288366 -5.92252339187664e-16,-5.87938524157182 3.31595971334866 -5.87938524157182e-16,-5.8270909152852 3.1865267138484 -5.8270909152852e-16,-5.76589518571785 3.06105687442822 -5.76589518571785e-16,-5.69609619231285 2.94016147153359 -5.69609619231285e-16,-5.61803398874989 2.82442949541505 -5.61803398874989e-16,-5.53208888623796 2.71442478062692 -5.53208888623796e-16,-5.4386796006773 2.61068325908201 -5.4386796006773e-16,-5.33826121271772 2.51371034904521 -5.33826121271772e-16,-5.23132295065132 2.42397849278656 -5.23132295065132e-16,-5.11838580694149 2.34192485488992 -5.11838580694149e-16,-5.0 2.26794919243112 -5e-16,-4.87674229357815 2.20241190740167 -4.87674229357815e-16,-4.74921318683182 2.14563229086643 -4.74921318683182e-16,-4.61803398874989 2.09788696740969 -4.61803398874989e-16,-4.48384379119934 2.05940854744801 -4.48384379119934e-16,-4.34729635533386 2.03038449397558 -4.34729635533386e-16,-4.20905692653531 2.01095620926345 -4.20905692653531e-16,-4.069798993405 2.00121834596181 -4.069798993405e-16,-3.930201006595 2.00121834596181 -3.930201006595e-16,-3.79094307346469 2.01095620926345 -3.79094307346469e-16,-3.65270364466614 2.03038449397558 -3.65270364466614e-16,-3.51615620880066 2.05940854744801 -3.51615620880066e-16,-3.38196601125011 2.09788696740969 -3.3819660112501e-16,-3.25078681316818 2.14563229086643 -3.25078681316818e-16,-3.12325770642185 2.20241190740167 -3.12325770642185e-16,-3.0 2.26794919243112 -3e-16,-2.88161419305851 2.34192485488992 -2.88161419305851e-16,-2.76867704934868 2.42397849278656 -2.76867704934868e-16,-2.66173878728228 2.51371034904521 -2.66173878728228e-16,-2.5613203993227 2.61068325908201 -2.5613203993227e-16,-2.46791111376204 2.71442478062692 -2.46791111376204e-16,-2.38196601125011 2.82442949541505 -2.3819660112501e-16,-2.30390380768715 2.94016147153359 -2.30390380768715e-16,-2.23410481428215 3.06105687442822 -2.23410481428215e-16,-2.1729090847148 3.1865267138484 -2.1729090847148e-16,-2.12061475842818 3.31595971334866 -2.12061475842818e-16,-2.07747660812336 3.448725288366 -2.07747660812336e-16,-2.04370479853239 3.58417661836448 -2.04370479853239e-16,-2.01946386251686 3.72165379807987 -2.01946386251686e-16,-2.00487189948035 3.86048705251175 -2.00487189948035e-16,-2.0 4.0 -2e-16,-2.00487189948035 4.13951294748825 -2.00487189948035e-16,-2.01946386251686 4.27834620192013 -2.01946386251686e-16,-2.04370479853239 4.41582338163552 -2.04370479853239e-16,' + \
        '-2.07747660812336 4.551274711634 -2.07747660812336e-16,-2.12061475842818 4.68404028665134 -2.12061475842818e-16,-2.1729090847148 4.8134732861516 -2.1729090847148e-16,-2.23410481428215 4.93894312557178 -2.23410481428215e-16,-2.30390380768715 5.05983852846641 -2.30390380768715e-16,-2.38196601125011 5.17557050458495 -2.3819660112501e-16,-2.46791111376204 5.28557521937308 -2.46791111376204e-16,-2.5613203993227 5.38931674091799 -2.5613203993227e-16,-2.66173878728228 5.48628965095479 -2.66173878728228e-16,-2.76867704934868 5.57602150721344 -2.76867704934868e-16,-2.88161419305851 5.65807514511008 -2.88161419305851e-16,-3.0 5.73205080756888 -3e-16,-3.12325770642184 5.79758809259833 -3.12325770642184e-16,-3.25078681316818 5.85436770913357 -3.25078681316817e-16,-3.38196601125011 5.90211303259031 -3.3819660112501e-16,-3.51615620880066 5.94059145255199 -3.51615620880066e-16,-3.65270364466614 5.96961550602442 -3.65270364466614e-16,-3.79094307346469 5.98904379073655 -3.79094307346469e-16,-3.930201006595 5.99878165403819 -3.930201006595e-16,-4.069798993405 5.99878165403819 -4.069798993405e-16,-4.20905692653531 5.98904379073655 -4.20905692653531e-16,-4.34729635533386 5.96961550602442 -4.34729635533386e-16,-4.48384379119933 5.94059145255199 -4.48384379119933e-16,-4.61803398874989 5.90211303259031 -4.61803398874989e-16,-4.74921318683182 5.85436770913357 -4.74921318683182e-16,-4.87674229357815 5.79758809259833 -4.87674229357815e-16,-5.0 5.73205080756888 -5e-16,-5.11838580694149 5.65807514511008 -5.11838580694149e-16,-5.23132295065132 5.57602150721344 -5.23132295065132e-16,-5.33826121271772 5.48628965095479 -5.33826121271772e-16,-5.4386796006773 5.389316740918 -5.4386796006773e-16,-5.53208888623796 5.28557521937308 -5.53208888623796e-16,-5.61803398874989 5.17557050458495 -5.61803398874989e-16,-5.69609619231285 5.05983852846641 -5.69609619231285e-16,-5.76589518571785 4.93894312557178 -5.76589518571785e-16,-5.8270909152852 4.8134732861516 -5.8270909152852e-16,-5.87938524157182 4.68404028665134 -5.87938524157182e-16,-5.92252339187664 4.551274711634 -5.92252339187664e-16,-5.95629520146761 4.41582338163552 -5.95629520146761e-16,-5.98053613748314 4.27834620192013 -5.98053613748314e-16,-5.99512810051965 4.13951294748825 -5.99512810051965e-16,-6.0 4.0 -6e-16)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):18
#   EntityHandle (String) = 20C
#   LINESTRING Z (-2 4 -3e-16,-2.00487189948035 4.06975647374412 -3.00487189948035e-16,-2.01946386251686 4.13917310096007 -3.01946386251686e-16,-2.04370479853239 4.20791169081776 -3.04370479853239e-16,-2.07747660812336 4.275637355817 -3.07747660812336e-16,-2.12061475842818 4.34202014332567 -3.12061475842818e-16,-2.1729090847148 4.4067366430758 -3.1729090847148e-16,-2.23410481428215 4.46947156278589 -3.23410481428215e-16,-2.30390380768715 4.52991926423321 -3.30390380768715e-16,-2.38196601125011 4.58778525229247 -3.38196601125011e-16,-2.46791111376204 4.64278760968654 -3.46791111376204e-16,-2.5613203993227 4.694658370459 -3.5613203993227e-16,-2.66173878728228 4.74314482547739 -3.66173878728228e-16,-2.76867704934868 4.78801075360672 -3.76867704934868e-16,-2.88161419305851 4.82903757255504 -3.88161419305851e-16,-3.0 4.86602540378444 -4e-16,-3.12325770642185 4.89879404629917 -4.12325770642185e-16,-3.25078681316818 4.92718385456679 -4.25078681316818e-16,-3.38196601125011 4.95105651629515 -4.38196601125011e-16,-3.51615620880067 4.970295726276 -4.51615620880067e-16,-3.65270364466614 4.98480775301221 -4.65270364466614e-16,-3.79094307346469 4.99452189536827 -4.79094307346469e-16,-3.930201006595 4.9993908270191 -4.930201006595e-16,-4.069798993405 4.9993908270191 -5.069798993405e-16,-4.20905692653531 4.99452189536827 -5.20905692653531e-16,-4.34729635533386 4.98480775301221 -5.34729635533386e-16,-4.48384379119934 4.970295726276 -5.48384379119934e-16,-4.61803398874989 4.95105651629515 -5.6180339887499e-16,-4.74921318683182 4.92718385456679 -5.74921318683183e-16,-4.87674229357816 4.89879404629917 -5.87674229357816e-16,-5.0 4.86602540378444 -6e-16,-5.11838580694149 4.82903757255504 -6.1183858069415e-16,-5.23132295065132 4.78801075360672 -6.23132295065132e-16,-5.33826121271772 4.74314482547739 -6.33826121271772e-16,-5.4386796006773 4.694658370459 -6.4386796006773e-16,-5.53208888623796 4.64278760968654 -6.53208888623796e-16,-5.61803398874989 4.58778525229247 -6.61803398874989e-16,-5.69609619231285 4.5299192642332 -6.69609619231285e-16,-5.76589518571785 4.46947156278589 -6.76589518571785e-16,-5.8270909152852 4.4067366430758 -6.8270909152852e-16,-5.87938524157182 4.34202014332567 -6.87938524157182e-16,-5.92252339187664 4.275637355817 -6.92252339187664e-16,-5.95629520146761 4.20791169081776 -6.95629520146761e-16,-5.98053613748314 4.13917310096006 -6.98053613748314e-16,-5.99512810051965 4.06975647374412 -6.99512810051965e-16,-6.0 4.0 -7e-16,-5.99512810051965 3.93024352625587 -6.99512810051965e-16,-5.98053613748314 3.86082689903993 -6.98053613748314e-16,-5.95629520146761 3.79208830918224 -6.95629520146761e-16,
# -5.92252339187664 3.724362644183 -6.92252339187664e-16,-5.87938524157182 3.65797985667433 -6.87938524157182e-16,-5.8270909152852 3.5932633569242 -6.8270909152852e-16,-5.76589518571785 3.53052843721411 -6.76589518571785e-16,-5.69609619231285 3.4700807357668 -6.69609619231285e-16,-5.61803398874989 3.41221474770753 -6.61803398874989e-16,-5.53208888623796 3.35721239031346 -6.53208888623796e-16,-5.4386796006773 3.305341629541 -6.4386796006773e-16,-5.33826121271772 3.25685517452261 -6.33826121271772e-16,-5.23132295065132 3.21198924639328 -6.23132295065132e-16,-5.11838580694149 3.17096242744496 -6.11838580694149e-16,-5.0 3.13397459621556 -6e-16,-4.87674229357815 3.10120595370083 -5.87674229357816e-16,-4.74921318683182 3.07281614543321 -5.74921318683182e-16,-4.61803398874989 3.04894348370485 -5.6180339887499e-16,-4.48384379119934 3.029704273724 -5.48384379119934e-16,-4.34729635533386 3.01519224698779 -5.34729635533386e-16,-4.20905692653531 3.00547810463173 -5.20905692653531e-16,-4.069798993405 3.0006091729809 -5.069798993405e-16,-3.930201006595 3.0006091729809 -4.930201006595e-16,-3.79094307346469 3.00547810463173 -4.79094307346469e-16,-3.65270364466614 3.01519224698779 -4.65270364466614e-16,-3.51615620880066 3.029704273724 -4.51615620880066e-16,-3.38196601125011 3.04894348370485 -4.38196601125011e-16,-3.25078681316818 3.07281614543321 -4.25078681316818e-16,-3.12325770642185 3.10120595370083 -4.12325770642185e-16,-3.0 3.13397459621556 -4e-16,-2.88161419305851 3.17096242744496 -3.88161419305851e-16,-2.76867704934868 3.21198924639328 -3.76867704934868e-16,-2.66173878728228 3.25685517452261 -3.66173878728228e-16,-2.5613203993227 3.305341629541 -3.5613203993227e-16,-2.46791111376204 3.35721239031346 -3.46791111376204e-16,-2.38196601125011 3.41221474770753 -3.38196601125011e-16,-2.30390380768715 3.4700807357668 -3.30390380768715e-16,-2.23410481428215 3.53052843721411 -3.23410481428215e-16,-2.1729090847148 3.5932633569242 -3.1729090847148e-16,-2.12061475842818 3.65797985667433 -3.12061475842818e-16,-2.07747660812336 3.724362644183 -3.07747660812336e-16,-2.04370479853239 3.79208830918224 -3.04370479853239e-16,-2.01946386251686 3.86082689903993 -3.01946386251686e-16,-2.00487189948035 3.93024352625587 -3.00487189948035e-16,-2 4 -3e-16)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (-2 4 -3e-16,-2.00487189948035 4.06975647374412 -3.00487189948035e-16,-2.01946386251686 4.13917310096007 -3.01946386251686e-16,-2.04370479853239 4.20791169081776 -3.04370479853239e-16,-2.07747660812336 4.275637355817 -3.07747660812336e-16,-2.12061475842818 4.34202014332567 -3.12061475842818e-16,-2.1729090847148 4.4067366430758 -3.1729090847148e-16,-2.23410481428215 4.46947156278589 -3.23410481428215e-16,-2.30390380768715 4.52991926423321 -3.30390380768715e-16,-2.38196601125011 4.58778525229247 -3.38196601125011e-16,-2.46791111376204 4.64278760968654 -3.46791111376204e-16,-2.5613203993227 4.694658370459 -3.5613203993227e-16,-2.66173878728228 4.74314482547739 -3.66173878728228e-16,-2.76867704934868 4.78801075360672 -3.76867704934868e-16,-2.88161419305851 4.82903757255504 -3.88161419305851e-16,-3.0 4.86602540378444 -4e-16,-3.12325770642185 4.89879404629917 -4.12325770642185e-16,-3.25078681316818 4.92718385456679 -4.25078681316818e-16,-3.38196601125011 4.95105651629515 -4.38196601125011e-16,-3.51615620880067 4.970295726276 -4.51615620880067e-16,-3.65270364466614 4.98480775301221 -4.65270364466614e-16,-3.79094307346469 4.99452189536827 -4.79094307346469e-16,-3.930201006595 4.9993908270191 -4.930201006595e-16,-4.069798993405 4.9993908270191 -5.069798993405e-16,-4.20905692653531 4.99452189536827 -5.20905692653531e-16,-4.34729635533386 4.98480775301221 -5.34729635533386e-16,-4.48384379119934 4.970295726276 -5.48384379119934e-16,-4.61803398874989 4.95105651629515 -5.6180339887499e-16,-4.74921318683182 4.92718385456679 -5.74921318683183e-16,-4.87674229357816 4.89879404629917 -5.87674229357816e-16,-5.0 4.86602540378444 -6e-16,-5.11838580694149 4.82903757255504 -6.1183858069415e-16,-5.23132295065132 4.78801075360672 -6.23132295065132e-16,-5.33826121271772 4.74314482547739 -6.33826121271772e-16,-5.4386796006773 4.694658370459 -6.4386796006773e-16,-5.53208888623796 4.64278760968654 -6.53208888623796e-16,-5.61803398874989 4.58778525229247 -6.61803398874989e-16,-5.69609619231285 4.5299192642332 -6.69609619231285e-16,-5.76589518571785 4.46947156278589 -6.76589518571785e-16,-5.8270909152852 4.4067366430758 -6.8270909152852e-16,-5.87938524157182 4.34202014332567 -6.87938524157182e-16,-5.92252339187664 4.275637355817 -6.92252339187664e-16,-5.95629520146761 4.20791169081776 -6.95629520146761e-16,-5.98053613748314 4.13917310096006 -6.98053613748314e-16,-5.99512810051965 4.06975647374412 -6.99512810051965e-16,-6.0 4.0 -7e-16,-5.99512810051965 3.93024352625587 -6.99512810051965e-16,-5.98053613748314 3.86082689903993 -6.98053613748314e-16,-5.95629520146761 3.79208830918224 -6.95629520146761e-16,' + \
        '-5.92252339187664 3.724362644183 -6.92252339187664e-16,-5.87938524157182 3.65797985667433 -6.87938524157182e-16,-5.8270909152852 3.5932633569242 -6.8270909152852e-16,-5.76589518571785 3.53052843721411 -6.76589518571785e-16,-5.69609619231285 3.4700807357668 -6.69609619231285e-16,-5.61803398874989 3.41221474770753 -6.61803398874989e-16,-5.53208888623796 3.35721239031346 -6.53208888623796e-16,-5.4386796006773 3.305341629541 -6.4386796006773e-16,-5.33826121271772 3.25685517452261 -6.33826121271772e-16,-5.23132295065132 3.21198924639328 -6.23132295065132e-16,-5.11838580694149 3.17096242744496 -6.11838580694149e-16,-5.0 3.13397459621556 -6e-16,-4.87674229357815 3.10120595370083 -5.87674229357816e-16,-4.74921318683182 3.07281614543321 -5.74921318683182e-16,-4.61803398874989 3.04894348370485 -5.6180339887499e-16,-4.48384379119934 3.029704273724 -5.48384379119934e-16,-4.34729635533386 3.01519224698779 -5.34729635533386e-16,-4.20905692653531 3.00547810463173 -5.20905692653531e-16,-4.069798993405 3.0006091729809 -5.069798993405e-16,-3.930201006595 3.0006091729809 -4.930201006595e-16,-3.79094307346469 3.00547810463173 -4.79094307346469e-16,-3.65270364466614 3.01519224698779 -4.65270364466614e-16,-3.51615620880066 3.029704273724 -4.51615620880066e-16,-3.38196601125011 3.04894348370485 -4.38196601125011e-16,-3.25078681316818 3.07281614543321 -4.25078681316818e-16,-3.12325770642185 3.10120595370083 -4.12325770642185e-16,-3.0 3.13397459621556 -4e-16,-2.88161419305851 3.17096242744496 -3.88161419305851e-16,-2.76867704934868 3.21198924639328 -3.76867704934868e-16,-2.66173878728228 3.25685517452261 -3.66173878728228e-16,-2.5613203993227 3.305341629541 -3.5613203993227e-16,-2.46791111376204 3.35721239031346 -3.46791111376204e-16,-2.38196601125011 3.41221474770753 -3.38196601125011e-16,-2.30390380768715 3.4700807357668 -3.30390380768715e-16,-2.23410481428215 3.53052843721411 -3.23410481428215e-16,-2.1729090847148 3.5932633569242 -3.1729090847148e-16,-2.12061475842818 3.65797985667433 -3.12061475842818e-16,-2.07747660812336 3.724362644183 -3.07747660812336e-16,-2.04370479853239 3.79208830918224 -3.04370479853239e-16,-2.01946386251686 3.86082689903993 -3.01946386251686e-16,-2.00487189948035 3.93024352625587 -3.00487189948035e-16,-2 4 -3e-16)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):19
#   EntityHandle (String) = 20D
#   LINESTRING Z (-2.0 2.0 -2e-16,-1.96657794502105 2.03582232791524 -1.96657794502105e-16,-1.93571660708646 2.07387296203834 -1.93571660708646e-16,-1.90756413746468 2.11396923855471 -1.90756413746468e-16,-1.88225568337755 2.15591867344963 -1.88225568337755e-16,-1.85991273921989 2.19951988653655 -1.85991273921989e-16,-1.84064256332004 2.24456356819194 -1.84064256332004e-16,-1.8245376630414 2.29083348415575 -1.8245376630414e-16,-1.81167535069652 2.33810751357387 -1.81167535069652e-16,-1.80211737240583 2.38615871529951 -1.80211737240583e-16,-1.79590961168258 2.43475641733454 -1.79590961168258e-16,-1.79308186916688 2.48366732418105 -1.79308186916688e-16,-1.79364771956639 2.53265663678705 -1.79364771956639e-16,-1.79760444649032 2.58148917971011 -1.79760444649032e-16,-1.80493305548955 2.62993053008785 -1.80493305548955e-16,-1.81559836524041 2.67774814299566 -1.81559836524041e-16,-1.8295491764342 2.72471246778926 -1.8295491764342e-16,-1.84671851756181 2.7705980500731 -1.84671851756181e-16,-1.86702396641357 2.81518461400453 -1.86702396641357e-16,-1.89036804575079 2.85825811973811 -1.89036804575079e-16,-1.91663869124976 2.89961179093366 -1.91663869124976e-16,-1.94570978947168 2.93904710739563 -1.94570978947168e-16,-1.97744178327594 2.97637475807832 -1.97744178327594e-16,-2.01168234177068 3.01141554988232 -2.01168234177068e-16,-2.04826709158413 3.04400126787917 -2.04826709158413e-16,-2.08702040594658 3.07397548283483 -2.08702040594658e-16,-2.12775624779472 3.10119430215541 -2.12775624779472e-16,-2.17027906285109 3.12552706065018 -2.17027906285109e-16,-2.2143847183914 3.14685694779575 -2.2143847183914e-16,-2.25986148319297 3.16508156849045 -2.25986148319297e-16,-2.30649104396024 3.18011343460661 -2.30649104396024e-16,-2.35404955334774 3.1918803849814 -2.35404955334774e-16,-2.40230870454951 3.20032593182975 -2.40230870454951e-16,-2.45103682729644 3.2054095319166 -2.45103682729644e-16,-2.5 3.20710678118655 -2.5e-16,-2.54896317270356 3.2054095319166 -2.54896317270356e-16,-2.59769129545049 3.20032593182975 -2.59769129545049e-16,-2.64595044665226 3.1918803849814 -2.64595044665226e-16,-2.69350895603976 3.18011343460661 -2.69350895603976e-16,-2.74013851680703 3.16508156849045 -2.74013851680703e-16,-2.7856152816086 3.14685694779575 -2.7856152816086e-16,-2.8297209371489 3.12552706065018 -2.8297209371489e-16,-2.87224375220528 3.10119430215541 -2.87224375220528e-16,-2.91297959405342 3.07397548283483 -2.91297959405342e-16,-2.95173290841587 3.04400126787917 -2.95173290841587e-16,-2.98831765822932 3.01141554988232 -2.98831765822932e-16,-3.02255821672406 2.97637475807832 -3.02255821672406e-16,-3.05429021052832 2.93904710739563 -3.05429021052832e-16,-3.08336130875024 2.89961179093367 -3.08336130875024e-16,-3.10963195424921 2.85825811973811 -3.10963195424921e-16,-3.13297603358643 2.81518461400453 -3.13297603358643e-16,-3.15328148243819 2.7705980500731 -3.15328148243819e-16,-3.1704508235658 2.72471246778926 -3.1704508235658e-16,-3.18440163475959 2.67774814299567 -3.18440163475959e-16,-3.19506694451045 2.62993053008786 -3.19506694451045e-16,-3.20239555350968 2.58148917971011 -3.20239555350968e-16,-3.20635228043361 2.53265663678705 -3.20635228043361e-16,-3.20691813083312 2.48366732418105 -3.20691813083312e-16,-3.20409038831742 2.43475641733454 -3.20409038831742e-16,-3.19788262759417 2.38615871529951 -3.19788262759417e-16,-3.18832464930348 2.33810751357387 -3.18832464930349e-16,-3.1754623369586 2.29083348415575 -3.1754623369586e-16,-3.15935743667996 2.24456356819194 -3.15935743667996e-16,-3.14008726078011 2.19951988653655 -3.14008726078011e-16,-3.11774431662245 2.15591867344963 -3.11774431662245e-16,-3.09243586253532 2.11396923855472 -3.09243586253532e-16,-3.06428339291354 2.07387296203834 -3.06428339291354e-16,-3.03342205497895 2.03582232791524 -3.03342205497895e-16,-3 2 -3e-16)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (-2.0 2.0 -2e-16,-1.96657794502105 2.03582232791524 -1.96657794502105e-16,-1.93571660708646 2.07387296203834 -1.93571660708646e-16,-1.90756413746468 2.11396923855471 -1.90756413746468e-16,-1.88225568337755 2.15591867344963 -1.88225568337755e-16,-1.85991273921989 2.19951988653655 -1.85991273921989e-16,-1.84064256332004 2.24456356819194 -1.84064256332004e-16,-1.8245376630414 2.29083348415575 -1.8245376630414e-16,-1.81167535069652 2.33810751357387 -1.81167535069652e-16,-1.80211737240583 2.38615871529951 -1.80211737240583e-16,-1.79590961168258 2.43475641733454 -1.79590961168258e-16,-1.79308186916688 2.48366732418105 -1.79308186916688e-16,-1.79364771956639 2.53265663678705 -1.79364771956639e-16,-1.79760444649032 2.58148917971011 -1.79760444649032e-16,-1.80493305548955 2.62993053008785 -1.80493305548955e-16,-1.81559836524041 2.67774814299566 -1.81559836524041e-16,-1.8295491764342 2.72471246778926 -1.8295491764342e-16,-1.84671851756181 2.7705980500731 -1.84671851756181e-16,-1.86702396641357 2.81518461400453 -1.86702396641357e-16,-1.89036804575079 2.85825811973811 -1.89036804575079e-16,-1.91663869124976 2.89961179093366 -1.91663869124976e-16,-1.94570978947168 2.93904710739563 -1.94570978947168e-16,-1.97744178327594 2.97637475807832 -1.97744178327594e-16,-2.01168234177068 3.01141554988232 -2.01168234177068e-16,-2.04826709158413 3.04400126787917 -2.04826709158413e-16,-2.08702040594658 3.07397548283483 -2.08702040594658e-16,-2.12775624779472 3.10119430215541 -2.12775624779472e-16,-2.17027906285109 3.12552706065018 -2.17027906285109e-16,-2.2143847183914 3.14685694779575 -2.2143847183914e-16,-2.25986148319297 3.16508156849045 -2.25986148319297e-16,-2.30649104396024 3.18011343460661 -2.30649104396024e-16,-2.35404955334774 3.1918803849814 -2.35404955334774e-16,-2.40230870454951 3.20032593182975 -2.40230870454951e-16,-2.45103682729644 3.2054095319166 -2.45103682729644e-16,' + \
        '-2.5 3.20710678118655 -2.5e-16,-2.54896317270356 3.2054095319166 -2.54896317270356e-16,-2.59769129545049 3.20032593182975 -2.59769129545049e-16,-2.64595044665226 3.1918803849814 -2.64595044665226e-16,-2.69350895603976 3.18011343460661 -2.69350895603976e-16,-2.74013851680703 3.16508156849045 -2.74013851680703e-16,-2.7856152816086 3.14685694779575 -2.7856152816086e-16,-2.8297209371489 3.12552706065018 -2.8297209371489e-16,-2.87224375220528 3.10119430215541 -2.87224375220528e-16,-2.91297959405342 3.07397548283483 -2.91297959405342e-16,-2.95173290841587 3.04400126787917 -2.95173290841587e-16,-2.98831765822932 3.01141554988232 -2.98831765822932e-16,-3.02255821672406 2.97637475807832 -3.02255821672406e-16,-3.05429021052832 2.93904710739563 -3.05429021052832e-16,-3.08336130875024 2.89961179093367 -3.08336130875024e-16,-3.10963195424921 2.85825811973811 -3.10963195424921e-16,-3.13297603358643 2.81518461400453 -3.13297603358643e-16,-3.15328148243819 2.7705980500731 -3.15328148243819e-16,-3.1704508235658 2.72471246778926 -3.1704508235658e-16,-3.18440163475959 2.67774814299567 -3.18440163475959e-16,-3.19506694451045 2.62993053008786 -3.19506694451045e-16,-3.20239555350968 2.58148917971011 -3.20239555350968e-16,-3.20635228043361 2.53265663678705 -3.20635228043361e-16,-3.20691813083312 2.48366732418105 -3.20691813083312e-16,-3.20409038831742 2.43475641733454 -3.20409038831742e-16,-3.19788262759417 2.38615871529951 -3.19788262759417e-16,-3.18832464930348 2.33810751357387 -3.18832464930349e-16,-3.1754623369586 2.29083348415575 -3.1754623369586e-16,-3.15935743667996 2.24456356819194 -3.15935743667996e-16,-3.14008726078011 2.19951988653655 -3.14008726078011e-16,-3.11774431662245 2.15591867344963 -3.11774431662245e-16,-3.09243586253532 2.11396923855472 -3.09243586253532e-16,-3.06428339291354 2.07387296203834 -3.06428339291354e-16,-3.03342205497895 2.03582232791524 -3.03342205497895e-16,-3 2 -3e-16)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):20
#   EntityHandle (String) = 20E
#   POLYGON Z ((-1 2 -1e-16,-1 3 -1e-16,-2 3 -2e-16,-2 2 -2e-16,-1 2 -1e-16))
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON Z ((-1 2 -1e-16,-1 3 -1e-16,-2 3 -2e-16,-2 2 -2e-16,-1 2 -1e-16))'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):21
#   EntityHandle (String) = 20F
#   POLYGON ((-3 4,-4 4,-4 3,-3 3,-3 4))
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON ((-3 4,-4 4,-4 3,-3 3,-3 4))'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):22
#   EntityHandle (String) = 211
#   POLYGON ((-8 8,-9 8,-9 9,-8 9,-8 8))
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON ((-8 8,-9 8,-9 9,-8 9,-8 8))'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):23
#   EntityHandle (String) = 212
#   LINESTRING (-2 2,-2.15384615384615 2.15384615384615,-2.30769230769231 2.30769230769231,-2.46153846153846 2.46153846153846,-2.61538461538461 2.61538461538461,-2.76923076923077 2.76923076923077,-2.92307692307692 2.92307692307692,-3.07692307692308 3.07692307692308,-3.23076923076923 3.23076923076923,-3.38461538461538 3.38461538461538,-3.53846153846154 3.53846153846154,-3.69230769230769 3.69230769230769,-3.84615384615385 3.84615384615385,-4 4,-4.15384615384615 4.15384615384615,-4.30769230769231 4.30769230769231,-4.46153846153846 4.46153846153846,-4.61538461538462 4.61538461538462,-4.76923076923077 4.76923076923077,-4.92307692307692 4.92307692307692,-5.07692307692308 5.07692307692308,-5.23076923076923 5.23076923076923,-5.38461538461538 5.38461538461538,-5.53846153846154 5.53846153846154,-5.69230769230769 5.69230769230769,-5.84615384615385 5.84615384615385,-6.0 6.0,-6.15384615384615 6.15384615384615,-6.30769230769231 6.30769230769231,-6.46153846153846 6.46153846153846,-6.61538461538462 6.61538461538462,-6.76923076923077 6.76923076923077,-6.92307692307692 6.92307692307692,-7.07692307692308 7.07692307692308,-7.23076923076923 7.23076923076923,-7.38461538461539 7.38461538461539,-7.53846153846154 7.53846153846154,-7.69230769230769 7.69230769230769,-7.84615384615385 7.84615384615385,-8 8)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (-2 2,-2.15384615384615 2.15384615384615,-2.30769230769231 2.30769230769231,-2.46153846153846 2.46153846153846,-2.61538461538461 2.61538461538461,-2.76923076923077 2.76923076923077,-2.92307692307692 2.92307692307692,-3.07692307692308 3.07692307692308,-3.23076923076923 3.23076923076923,-3.38461538461538 3.38461538461538,-3.53846153846154 3.53846153846154,-3.69230769230769 3.69230769230769,-3.84615384615385 3.84615384615385,-4 4,-4.15384615384615 4.15384615384615,-4.30769230769231 4.30769230769231,-4.46153846153846 4.46153846153846,-4.61538461538462 4.61538461538462,-4.76923076923077 4.76923076923077,-4.92307692307692 4.92307692307692,-5.07692307692308 5.07692307692308,-5.23076923076923 5.23076923076923,-5.38461538461538 5.38461538461538,-5.53846153846154 5.53846153846154,-5.69230769230769 5.69230769230769,-5.84615384615385 5.84615384615385,-6.0 6.0,-6.15384615384615 6.15384615384615,-6.30769230769231 6.30769230769231,-6.46153846153846 6.46153846153846,-6.61538461538462 6.61538461538462,-6.76923076923077 6.76923076923077,-6.92307692307692 6.92307692307692,-7.07692307692308 7.07692307692308,-7.23076923076923 7.23076923076923,-7.38461538461539 7.38461538461539,-7.53846153846154 7.53846153846154,-7.69230769230769 7.69230769230769,-7.84615384615385 7.84615384615385,-8 8)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):24
#   EntityHandle (String) = 213
#   LINESTRING (-8 1,-7.62837370825536 0.987348067229724,-7.25775889681215 0.975707614760869,-6.88916704597178 0.966090122894857,-6.52360963603567 0.959507071933107,-6.16209814730525 0.956969942177043,-5.80564406008193 0.959490213928084,-5.45525885466714 0.968079367487651,-5.11195401136229 0.983748883157167,-4.77674101046882 1.00751024123805,-4.45063133228814 1.04037492203173,-4.13463645712167 1.08335440583961,-3.82976786527082 1.13746017296313,-3.53703703703704 1.2037037037037,-3.25745545272173 1.28309647836275,-2.99203459262631 1.37664997724169,-2.74178593705221 1.48537568064195,-2.50772096630085 1.61028506886495,-2.29085116067365 1.75238962221211,-2.09218800047203 1.91270082098484,-1.91270082098485 2.09218800047202,-1.75238962221211 2.29085116067364,-1.61028506886495 2.50772096630085,-1.48537568064195 2.74178593705221,-1.37664997724169 2.99203459262631,-1.28309647836275 3.25745545272172,-1.2037037037037 3.53703703703703,-1.13746017296313 3.82976786527082,-1.08335440583961 4.13463645712166,-1.04037492203173 4.45063133228814,-1.00751024123805 4.77674101046882,-0.983748883157167 5.11195401136229,-0.968079367487652 5.45525885466714,-0.959490213928084 5.80564406008193,-0.956969942177043 6.16209814730525,-0.959507071933108 6.52360963603567,-0.966090122894857 6.88916704597178,-0.975707614760869 7.25775889681216,-0.987348067229724 7.62837370825537,-1 8)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (-8 1,-7.62837370825536 0.987348067229724,-7.25775889681215 0.975707614760869,-6.88916704597178 0.966090122894857,-6.52360963603567 0.959507071933107,-6.16209814730525 0.956969942177043,-5.80564406008193 0.959490213928084,-5.45525885466714 0.968079367487651,-5.11195401136229 0.983748883157167,-4.77674101046882 1.00751024123805,-4.45063133228814 1.04037492203173,-4.13463645712167 1.08335440583961,-3.82976786527082 1.13746017296313,-3.53703703703704 1.2037037037037,-3.25745545272173 1.28309647836275,-2.99203459262631 1.37664997724169,-2.74178593705221 1.48537568064195,-2.50772096630085 1.61028506886495,-2.29085116067365 1.75238962221211,-2.09218800047203 1.91270082098484,-1.91270082098485 2.09218800047202,-1.75238962221211 2.29085116067364,-1.61028506886495 2.50772096630085,-1.48537568064195 2.74178593705221,-1.37664997724169 2.99203459262631,-1.28309647836275 3.25745545272172,-1.2037037037037 3.53703703703703,-1.13746017296313 3.82976786527082,-1.08335440583961 4.13463645712166,-1.04037492203173 4.45063133228814,-1.00751024123805 4.77674101046882,-0.983748883157167 5.11195401136229,-0.968079367487652 5.45525885466714,-0.959490213928084 5.80564406008193,-0.956969942177043 6.16209814730525,-0.959507071933108 6.52360963603567,-0.966090122894857 6.88916704597178,-0.975707614760869 7.25775889681216,-0.987348067229724 7.62837370825537,-1 8)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):25
#   EntityHandle (String) = 214
#   POINT Z (-7 7 -7e-16)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POINT Z (-7 7 -7e-16)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):26
#   EntityHandle (String) = 215
#   POINT Z (-4 -4 -1e-15)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POINT Z (-4 -4 -1e-15)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):27
#   EntityHandle (String) = 216
#   LINESTRING Z (0 0 -2e-16,-1 -1 -5e-16)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (0 0 -2e-16,-1 -1 -5e-16)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):28
#   EntityHandle (String) = 217
#   LINESTRING (-1 -1,-2 -1,-1 -2,-1 -1)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (-1 -1,-2 -1,-1 -2,-1 -1)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):29
#   EntityHandle (String) = 218
#   LINESTRING Z (-1 -1 -2e-16,-1 -2 -4e-16,-2 -2 -5e-16,-1 -1 -2e-16)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (-1 -1 -2e-16,-1 -2 -4e-16,-2 -2 -5e-16,-1 -1 -2e-16)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):30
#   EntityHandle (String) = 21D
#   LINESTRING Z (-2 -4 0,-2.00487189948035 -4.13951294748825 0,-2.01946386251686 -4.27834620192013 0,-2.04370479853239 -4.41582338163552 0,-2.07747660812336 -4.551274711634 0,-2.12061475842818 -4.68404028665134 0,-2.1729090847148 -4.8134732861516 0,-2.23410481428215 -4.93894312557178 0,-2.30390380768715 -5.05983852846641 0,-2.38196601125011 -5.17557050458495 0,-2.46791111376204 -5.28557521937308 0,-2.5613203993227 -5.38931674091799 0,-2.66173878728228 -5.48628965095479 0,-2.76867704934868 -5.57602150721344 0,-2.88161419305851 -5.65807514511008 0,-3.0 -5.73205080756888 0,-3.12325770642185 -5.79758809259833 0,-3.25078681316818 -5.85436770913357 0,-3.38196601125011 -5.90211303259031 0,-3.51615620880066 -5.94059145255199 0,-3.65270364466614 -5.96961550602442 0,-3.79094307346469 -5.98904379073655 0,-3.930201006595 -5.99878165403819 0,-4.069798993405 -5.99878165403819 0,-4.20905692653531 -5.98904379073655 0,-4.34729635533386 -5.96961550602442 0,-4.48384379119934 -5.94059145255199 0,-4.61803398874989 -5.90211303259031 0,-4.74921318683182 -5.85436770913357 0,-4.87674229357815 -5.79758809259833 0,-5.0 -5.73205080756888 0,-5.11838580694149 -5.65807514511008 0,-5.23132295065132 -5.57602150721344 0,-5.33826121271772 -5.48628965095479 0,-5.4386796006773 -5.38931674091799 0,-5.53208888623796 -5.28557521937308 0,-5.61803398874989 -5.17557050458495 0,-5.69609619231285 -5.05983852846641 0,-5.76589518571785 -4.93894312557178 0,-5.8270909152852 -4.8134732861516 0,-5.87938524157182 -4.68404028665134 0,-5.92252339187664 -4.551274711634 0,-5.95629520146761 -4.41582338163552 0,-5.98053613748314 -4.27834620192013 0,-5.99512810051965 -4.13951294748825 0,-6 -4 0,-5.99512810051965 -3.86048705251175 0,-5.98053613748314 -3.72165379807987 0,-5.95629520146761 -3.58417661836448 0,-5.92252339187664 -3.448725288366 0,-5.87938524157182 -3.31595971334866 0,-5.8270909152852 -3.1865267138484 0,-5.76589518571785 -3.06105687442822 0,-5.69609619231285 -2.94016147153359 0,-5.61803398874989 -2.82442949541505 0,-5.53208888623796 -2.71442478062692 0,-5.4386796006773 -2.61068325908201 0,-5.33826121271772 -2.51371034904521 0,-5.23132295065132 -2.42397849278656 0,-5.11838580694149 -2.34192485488992 0,-5.0 -2.26794919243112 0,-4.87674229357816 -2.20241190740167 0,-4.74921318683182 -2.14563229086643 0,-4.61803398874989 -2.09788696740969 0,-4.48384379119934 -2.05940854744801 0,-4.34729635533386 -2.03038449397558 0,-4.20905692653531 -2.01095620926345 0,-4.069798993405 -2.00121834596181 0,-3.930201006595 -2.00121834596181 0,-3.79094307346469 -2.01095620926345 0,-3.65270364466614 -2.03038449397558 0,-3.51615620880067 -2.05940854744801 0,-3.38196601125011 -2.09788696740969 0,-3.25078681316818 -2.14563229086643 0,-3.12325770642185 -2.20241190740167 0,-3.0 -2.26794919243112 0,-2.88161419305851 -2.34192485488992 0,-2.76867704934868 -2.42397849278656 0,-2.66173878728228 -2.51371034904521 0,-2.5613203993227 -2.610683259082 0,-2.46791111376204 -2.71442478062692 0,-2.38196601125011 -2.82442949541505 0,-2.30390380768715 -2.94016147153359 0,-2.23410481428215 -3.06105687442822 0,-2.1729090847148 -3.1865267138484 0,-2.12061475842818 -3.31595971334866 0,-2.07747660812336 -3.448725288366 0,-2.04370479853239 -3.58417661836448 0,-2.01946386251686 -3.72165379807987 0,-2.00487189948035 -3.86048705251175 0,-2.0 -4.0 0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (-2 -4 0,-2.00487189948035 -4.13951294748825 0,-2.01946386251686 -4.27834620192013 0,-2.04370479853239 -4.41582338163552 0,-2.07747660812336 -4.551274711634 0,-2.12061475842818 -4.68404028665134 0,-2.1729090847148 -4.8134732861516 0,-2.23410481428215 -4.93894312557178 0,-2.30390380768715 -5.05983852846641 0,-2.38196601125011 -5.17557050458495 0,-2.46791111376204 -5.28557521937308 0,-2.5613203993227 -5.38931674091799 0,-2.66173878728228 -5.48628965095479 0,-2.76867704934868 -5.57602150721344 0,-2.88161419305851 -5.65807514511008 0,-3.0 -5.73205080756888 0,-3.12325770642185 -5.79758809259833 0,-3.25078681316818 -5.85436770913357 0,-3.38196601125011 -5.90211303259031 0,-3.51615620880066 -5.94059145255199 0,-3.65270364466614 -5.96961550602442 0,-3.79094307346469 -5.98904379073655 0,-3.930201006595 -5.99878165403819 0,-4.069798993405 -5.99878165403819 0,-4.20905692653531 -5.98904379073655 0,-4.34729635533386 -5.96961550602442 0,-4.48384379119934 -5.94059145255199 0,-4.61803398874989 -5.90211303259031 0,-4.74921318683182 -5.85436770913357 0,-4.87674229357815 -5.79758809259833 0,-5.0 -5.73205080756888 0,-5.11838580694149 -5.65807514511008 0,-5.23132295065132 -5.57602150721344 0,-5.33826121271772 -5.48628965095479 0,-5.4386796006773 -5.38931674091799 0,-5.53208888623796 -5.28557521937308 0,-5.61803398874989 -5.17557050458495 0,-5.69609619231285 -5.05983852846641 0,-5.76589518571785 -4.93894312557178 0,-5.8270909152852 -4.8134732861516 0,-5.87938524157182 -4.68404028665134 0,-5.92252339187664 -4.551274711634 0,-5.95629520146761 -4.41582338163552 0,-5.98053613748314 -4.27834620192013 0,-5.99512810051965 -4.13951294748825 0,-6 -4 0,-5.99512810051965 -3.86048705251175 0,-5.98053613748314 -3.72165379807987 0,-5.95629520146761 -3.58417661836448 0,-5.92252339187664 -3.448725288366 0,-5.87938524157182 -3.31595971334866 0,-5.8270909152852 -3.1865267138484 0,-5.76589518571785 -3.06105687442822 0,-5.69609619231285 -2.94016147153359 0,-5.61803398874989 -2.82442949541505 0,-5.53208888623796 -2.71442478062692 0,-5.4386796006773 -2.61068325908201 0,-5.33826121271772 -2.51371034904521 0,-5.23132295065132 -2.42397849278656 0,-5.11838580694149 -2.34192485488992 0,-5.0 -2.26794919243112 0,-4.87674229357816 -2.20241190740167 0,-4.74921318683182 -2.14563229086643 0,-4.61803398874989 -2.09788696740969 0,-4.48384379119934 -2.05940854744801 0,-4.34729635533386 -2.03038449397558 0,-4.20905692653531 -2.01095620926345 0,-4.069798993405 -2.00121834596181 0,-3.930201006595 -2.00121834596181 0,-3.79094307346469 -2.01095620926345 0,-3.65270364466614 -2.03038449397558 0,-3.51615620880067 -2.05940854744801 0,-3.38196601125011 -2.09788696740969 0,-3.25078681316818 -2.14563229086643 0,-3.12325770642185 -2.20241190740167 0,-3.0 -2.26794919243112 0,-2.88161419305851 -2.34192485488992 0,-2.76867704934868 -2.42397849278656 0,-2.66173878728228 -2.51371034904521 0,-2.5613203993227 -2.610683259082 0,-2.46791111376204 -2.71442478062692 0,-2.38196601125011 -2.82442949541505 0,-2.30390380768715 -2.94016147153359 0,-2.23410481428215 -3.06105687442822 0,-2.1729090847148 -3.1865267138484 0,-2.12061475842818 -3.31595971334866 0,-2.07747660812336 -3.448725288366 0,-2.04370479853239 -3.58417661836448 0,-2.01946386251686 -3.72165379807987 0,-2.00487189948035 -3.86048705251175 0,-2.0 -4.0 0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):31
#   EntityHandle (String) = 21E
#   LINESTRING Z (-2 -4 -8e-16,-2.00487189948035 -4.06975647374412 -8.07462837322448e-16,-2.01946386251686 -4.13917310096007 -8.15863696347693e-16,-2.04370479853239 -4.20791169081776 -8.25161648935015e-16,-2.07747660812336 -4.275637355817 -8.35311396394036e-16,-2.12061475842818 -4.34202014332567 -8.46263490175385e-16,-2.1729090847148 -4.4067366430758 -8.5796457277906e-16,-2.23410481428215 -4.46947156278589 -8.70357637706804e-16,-2.30390380768715 -4.52991926423321 -8.83382307192036e-16,-2.38196601125011 -4.58778525229247 -8.96975126354258e-16,-2.46791111376204 -4.64278760968654 -9.11069872344859e-16,-2.5613203993227 -4.694658370459 -9.2559787697817e-16,-2.66173878728228 -4.74314482547739 -9.40488361275968e-16,-2.76867704934868 -4.78801075360672 -9.5566878029554e-16,-2.88161419305851 -4.82903757255504 -9.71065176561355e-16,-3.0 -4.86602540378444 -9.86602540378444e-16,-3.12325770642185 -4.89879404629917 -1.0022051752721e-15,-3.25078681316818 -4.92718385456679 -1.0177970667735e-15,-3.38196601125011 -4.95105651629515 -1.03330225275453e-15,-3.51615620880067 -4.970295726276 -1.04864519350767e-15,-3.65270364466614 -4.98480775301221 -1.06375113976783e-15,-3.79094307346469 -4.99452189536827 -1.0785464968833e-15,-3.930201006595 -4.9993908270191 -1.09295918336141e-15,-4.069798993405 -4.9993908270191 -1.10691898204241e-15,-4.20905692653531 -4.99452189536827 -1.12035788219036e-15,-4.34729635533386 -4.98480775301221 -1.13321041083461e-15,-4.48384379119934 -4.970295726276 -1.14541395174753e-15,-4.61803398874989 -4.95105651629515 -1.1569090505045e-15,-4.74921318683182 -4.92718385456679 -1.16763970413986e-15,-4.87674229357816 -4.89879404629917 -1.17755363398773e-15,-5.0 -4.86602540378444 -1.18660254037844e-15,-5.11838580694149 -4.82903757255504 -1.19474233794965e-15,-5.23132295065132 -4.78801075360672 -1.2019333704258e-15,-5.33826121271772 -4.74314482547739 -1.20814060381951e-15,-5.4386796006773 -4.694658370459 -1.21333379711363e-15,-5.53208888623796 -4.64278760968654 -1.21748764959245e-15,-5.61803398874989 -4.58778525229247 -1.22058192410424e-15,-5.69609619231285 -4.5299192642332 -1.22260154565461e-15,-5.76589518571785 -4.46947156278589 -1.22353667485037e-15,-5.8270909152852 -4.4067366430758 -1.2233827558361e-15,-5.87938524157182 -4.34202014332567 -1.22214053848975e-15,-5.92252339187664 -4.275637355817 -1.21981607476936e-15,-5.95629520146761 -4.20791169081776 -1.21642068922854e-15,-5.98053613748314 -4.13917310096007 -1.21197092384432e-15,-5.99512810051965 -4.06975647374412 -1.20648845742638e-15,-6 -4 -1.2e-15,-5.99512810051965 -3.93024352625587 -1.19253716267755e-15,
# -5.98053613748314 -3.86082689903993 -1.18413630365231e-15,-5.95629520146761 -3.79208830918224 -1.17483835106499e-15,-5.92252339187664 -3.724362644183 -1.16468860360596e-15,-5.87938524157182 -3.65797985667433 -1.15373650982461e-15,-5.8270909152852 -3.5932633569242 -1.14203542722094e-15,-5.76589518571785 -3.53052843721411 -1.1296423622932e-15,-5.69609619231285 -3.4700807357668 -1.11661769280796e-15,-5.61803398874989 -3.41221474770753 -1.10302487364574e-15,-5.53208888623796 -3.35721239031346 -1.08893012765514e-15,-5.4386796006773 -3.305341629541 -1.07440212302183e-15,-5.33826121271772 -3.25685517452261 -1.05951163872403e-15,-5.23132295065132 -3.21198924639328 -1.04433121970446e-15,-5.11838580694149 -3.17096242744496 -1.02893482343865e-15,-5.0 -3.13397459621556 -1.01339745962156e-15,-4.87674229357815 -3.10120595370083 -9.97794824727899e-16,-4.74921318683182 -3.07281614543321 -9.82202933226504e-16,-4.61803398874989 -3.04894348370485 -9.66697747245474e-16,-4.48384379119934 -3.029704273724 -9.51354806492334e-16,-4.34729635533386 -3.01519224698779 -9.36248860232165e-16,-4.20905692653531 -3.00547810463173 -9.21453503116703e-16,-4.069798993405 -3.0006091729809 -9.07040816638591e-16,-3.930201006595 -3.0006091729809 -8.9308101795759e-16,-3.79094307346469 -3.00547810463173 -8.79642117809642e-16,-3.65270364466614 -3.01519224698779 -8.66789589165393e-16,-3.51615620880066 -3.029704273724 -8.54586048252467e-16,-3.38196601125011 -3.04894348370485 -8.43090949495495e-16,-3.25078681316818 -3.07281614543321 -8.32360295860139e-16,-3.12325770642185 -3.10120595370083 -8.22446366012268e-16,-3.0 -3.13397459621556 -8.13397459621556e-16,-2.88161419305851 -3.17096242744496 -8.05257662050347e-16,-2.76867704934868 -3.21198924639328 -7.98066629574196e-16,-2.66173878728228 -3.25685517452261 -7.91859396180489e-16,-2.5613203993227 -3.305341629541 -7.8666620288637e-16,-2.46791111376204 -3.35721239031346 -7.82512350407551e-16,-2.38196601125011 -3.41221474770753 -7.79418075895763e-16,-2.30390380768715 -3.4700807357668 -7.77398454345394e-16,-2.23410481428215 -3.53052843721411 -7.76463325149626e-16,-2.1729090847148 -3.5932633569242 -7.766172441639e-16,-2.12061475842818 -3.65797985667433 -7.77859461510252e-16,-2.07747660812336 -3.724362644183 -7.80183925230636e-16,-2.04370479853239 -3.79208830918224 -7.83579310771463e-16,-2.01946386251686 -3.86082689903993 -7.88029076155679e-16,-2.00487189948035 -3.93024352625587 -7.93511542573623e-16,-2 -4 -8e-16)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (-2 -4 -8e-16,-2.00487189948035 -4.06975647374412 -8.07462837322448e-16,-2.01946386251686 -4.13917310096007 -8.15863696347693e-16,-2.04370479853239 -4.20791169081776 -8.25161648935015e-16,-2.07747660812336 -4.275637355817 -8.35311396394036e-16,-2.12061475842818 -4.34202014332567 -8.46263490175385e-16,-2.1729090847148 -4.4067366430758 -8.5796457277906e-16,-2.23410481428215 -4.46947156278589 -8.70357637706804e-16,-2.30390380768715 -4.52991926423321 -8.83382307192036e-16,-2.38196601125011 -4.58778525229247 -8.96975126354258e-16,-2.46791111376204 -4.64278760968654 -9.11069872344859e-16,-2.5613203993227 -4.694658370459 -9.2559787697817e-16,-2.66173878728228 -4.74314482547739 -9.40488361275968e-16,-2.76867704934868 -4.78801075360672 -9.5566878029554e-16,-2.88161419305851 -4.82903757255504 -9.71065176561355e-16,-3.0 -4.86602540378444 -9.86602540378444e-16,-3.12325770642185 -4.89879404629917 -1.0022051752721e-15,-3.25078681316818 -4.92718385456679 -1.0177970667735e-15,-3.38196601125011 -4.95105651629515 -1.03330225275453e-15,-3.51615620880067 -4.970295726276 -1.04864519350767e-15,-3.65270364466614 -4.98480775301221 -1.06375113976783e-15,-3.79094307346469 -4.99452189536827 -1.0785464968833e-15,-3.930201006595 -4.9993908270191 -1.09295918336141e-15,-4.069798993405 -4.9993908270191 -1.10691898204241e-15,-4.20905692653531 -4.99452189536827 -1.12035788219036e-15,-4.34729635533386 -4.98480775301221 -1.13321041083461e-15,-4.48384379119934 -4.970295726276 -1.14541395174753e-15,-4.61803398874989 -4.95105651629515 -1.1569090505045e-15,-4.74921318683182 -4.92718385456679 -1.16763970413986e-15,-4.87674229357816 -4.89879404629917 -1.17755363398773e-15,-5.0 -4.86602540378444 -1.18660254037844e-15,-5.11838580694149 -4.82903757255504 -1.19474233794965e-15,-5.23132295065132 -4.78801075360672 -1.2019333704258e-15,-5.33826121271772 -4.74314482547739 -1.20814060381951e-15,-5.4386796006773 -4.694658370459 -1.21333379711363e-15,-5.53208888623796 -4.64278760968654 -1.21748764959245e-15,-5.61803398874989 -4.58778525229247 -1.22058192410424e-15,-5.69609619231285 -4.5299192642332 -1.22260154565461e-15,-5.76589518571785 -4.46947156278589 -1.22353667485037e-15,-5.8270909152852 -4.4067366430758 -1.2233827558361e-15,-5.87938524157182 -4.34202014332567 -1.22214053848975e-15,-5.92252339187664 -4.275637355817 -1.21981607476936e-15,-5.95629520146761 -4.20791169081776 -1.21642068922854e-15,-5.98053613748314 -4.13917310096007 -1.21197092384432e-15,-5.99512810051965 -4.06975647374412 -1.20648845742638e-15,-6 -4 -1.2e-15,-5.99512810051965 -3.93024352625587 -1.19253716267755e-15,' + \
        '-5.98053613748314 -3.86082689903993 -1.18413630365231e-15,-5.95629520146761 -3.79208830918224 -1.17483835106499e-15,-5.92252339187664 -3.724362644183 -1.16468860360596e-15,-5.87938524157182 -3.65797985667433 -1.15373650982461e-15,-5.8270909152852 -3.5932633569242 -1.14203542722094e-15,-5.76589518571785 -3.53052843721411 -1.1296423622932e-15,-5.69609619231285 -3.4700807357668 -1.11661769280796e-15,-5.61803398874989 -3.41221474770753 -1.10302487364574e-15,-5.53208888623796 -3.35721239031346 -1.08893012765514e-15,-5.4386796006773 -3.305341629541 -1.07440212302183e-15,-5.33826121271772 -3.25685517452261 -1.05951163872403e-15,-5.23132295065132 -3.21198924639328 -1.04433121970446e-15,-5.11838580694149 -3.17096242744496 -1.02893482343865e-15,-5.0 -3.13397459621556 -1.01339745962156e-15,-4.87674229357815 -3.10120595370083 -9.97794824727899e-16,-4.74921318683182 -3.07281614543321 -9.82202933226504e-16,-4.61803398874989 -3.04894348370485 -9.66697747245474e-16,-4.48384379119934 -3.029704273724 -9.51354806492334e-16,-4.34729635533386 -3.01519224698779 -9.36248860232165e-16,-4.20905692653531 -3.00547810463173 -9.21453503116703e-16,-4.069798993405 -3.0006091729809 -9.07040816638591e-16,-3.930201006595 -3.0006091729809 -8.9308101795759e-16,-3.79094307346469 -3.00547810463173 -8.79642117809642e-16,-3.65270364466614 -3.01519224698779 -8.66789589165393e-16,-3.51615620880066 -3.029704273724 -8.54586048252467e-16,-3.38196601125011 -3.04894348370485 -8.43090949495495e-16,-3.25078681316818 -3.07281614543321 -8.32360295860139e-16,-3.12325770642185 -3.10120595370083 -8.22446366012268e-16,-3.0 -3.13397459621556 -8.13397459621556e-16,-2.88161419305851 -3.17096242744496 -8.05257662050347e-16,-2.76867704934868 -3.21198924639328 -7.98066629574196e-16,-2.66173878728228 -3.25685517452261 -7.91859396180489e-16,-2.5613203993227 -3.305341629541 -7.8666620288637e-16,-2.46791111376204 -3.35721239031346 -7.82512350407551e-16,-2.38196601125011 -3.41221474770753 -7.79418075895763e-16,-2.30390380768715 -3.4700807357668 -7.77398454345394e-16,-2.23410481428215 -3.53052843721411 -7.76463325149626e-16,-2.1729090847148 -3.5932633569242 -7.766172441639e-16,-2.12061475842818 -3.65797985667433 -7.77859461510252e-16,-2.07747660812336 -3.724362644183 -7.80183925230636e-16,-2.04370479853239 -3.79208830918224 -7.83579310771463e-16,-2.01946386251686 -3.86082689903993 -7.88029076155679e-16,-2.00487189948035 -3.93024352625587 -7.93511542573623e-16,-2 -4 -8e-16)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):32
#   EntityHandle (String) = 21F
#   LINESTRING Z (-2 -2 0,-1.96657794502105 -2.03582232791524 0,-1.93571660708646 -2.07387296203834 0,-1.90756413746468 -2.11396923855472 0,-1.88225568337755 -2.15591867344963 0,-1.85991273921989 -2.19951988653655 0,-1.84064256332004 -2.24456356819194 0,-1.8245376630414 -2.29083348415575 0,-1.81167535069652 -2.33810751357387 0,-1.80211737240583 -2.38615871529951 0,-1.79590961168258 -2.43475641733454 0,-1.79308186916688 -2.48366732418105 0,-1.79364771956639 -2.53265663678705 0,-1.79760444649032 -2.58148917971011 0,-1.80493305548955 -2.62993053008786 0,-1.81559836524041 -2.67774814299567 0,-1.8295491764342 -2.72471246778926 0,-1.84671851756181 -2.7705980500731 0,-1.86702396641357 -2.81518461400453 0,-1.89036804575079 -2.85825811973811 0,-1.91663869124976 -2.89961179093367 0,-1.94570978947168 -2.93904710739563 0,-1.97744178327594 -2.97637475807832 0,-2.01168234177068 -3.01141554988232 0,-2.04826709158413 -3.04400126787917 0,-2.08702040594658 -3.07397548283483 0,-2.12775624779472 -3.10119430215541 0,-2.1702790628511 -3.12552706065018 0,-2.2143847183914 -3.14685694779575 0,-2.25986148319297 -3.16508156849045 0,-2.30649104396024 -3.18011343460661 0,-2.35404955334774 -3.1918803849814 0,-2.40230870454951 -3.20032593182975 0,-2.45103682729644 -3.2054095319166 0,-2.5 -3.20710678118655 0,-2.54896317270356 -3.2054095319166 0,-2.59769129545049 -3.20032593182975 0,-2.64595044665226 -3.1918803849814 0,-2.69350895603976 -3.18011343460661 0,-2.74013851680703 -3.16508156849045 0,-2.7856152816086 -3.14685694779575 0,-2.8297209371489 -3.12552706065018 0,-2.87224375220528 -3.10119430215541 0,-2.91297959405342 -3.07397548283483 0,-2.95173290841587 -3.04400126787917 0,-2.98831765822932 -3.01141554988232 0,-3.02255821672406 -2.97637475807832 0,-3.05429021052832 -2.93904710739563 0,-3.08336130875024 -2.89961179093367 0,-3.10963195424921 -2.85825811973811 0,-3.13297603358643 -2.81518461400453 0,-3.15328148243819 -2.7705980500731 0,-3.1704508235658 -2.72471246778926 0,-3.18440163475959 -2.67774814299567 0,-3.19506694451045 -2.62993053008786 0,-3.20239555350968 -2.58148917971011 0,-3.20635228043361 -2.53265663678705 0,-3.20691813083312 -2.48366732418105 0,-3.20409038831742 -2.43475641733454 0,-3.19788262759417 -2.38615871529951 0,-3.18832464930348 -2.33810751357387 0,-3.1754623369586 -2.29083348415575 0,-3.15935743667996 -2.24456356819194 0,-3.14008726078011 -2.19951988653655 0,-3.11774431662245 -2.15591867344963 0,-3.09243586253532 -2.11396923855472 0,-3.06428339291354 -2.07387296203834 0,-3.03342205497895 -2.03582232791524 0,-3 -2 0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (-2 -2 0,-1.96657794502105 -2.03582232791524 0,-1.93571660708646 -2.07387296203834 0,-1.90756413746468 -2.11396923855472 0,-1.88225568337755 -2.15591867344963 0,-1.85991273921989 -2.19951988653655 0,-1.84064256332004 -2.24456356819194 0,-1.8245376630414 -2.29083348415575 0,-1.81167535069652 -2.33810751357387 0,-1.80211737240583 -2.38615871529951 0,-1.79590961168258 -2.43475641733454 0,-1.79308186916688 -2.48366732418105 0,-1.79364771956639 -2.53265663678705 0,-1.79760444649032 -2.58148917971011 0,-1.80493305548955 -2.62993053008786 0,-1.81559836524041 -2.67774814299567 0,-1.8295491764342 -2.72471246778926 0,-1.84671851756181 -2.7705980500731 0,-1.86702396641357 -2.81518461400453 0,-1.89036804575079 -2.85825811973811 0,-1.91663869124976 -2.89961179093367 0,-1.94570978947168 -2.93904710739563 0,-1.97744178327594 -2.97637475807832 0,-2.01168234177068 -3.01141554988232 0,-2.04826709158413 -3.04400126787917 0,-2.08702040594658 -3.07397548283483 0,-2.12775624779472 -3.10119430215541 0,-2.1702790628511 -3.12552706065018 0,-2.2143847183914 -3.14685694779575 0,-2.25986148319297 -3.16508156849045 0,-2.30649104396024 -3.18011343460661 0,-2.35404955334774 -3.1918803849814 0,-2.40230870454951 -3.20032593182975 0,-2.45103682729644 -3.2054095319166 0,-2.5 -3.20710678118655 0,-2.54896317270356 -3.2054095319166 0,-2.59769129545049 -3.20032593182975 0,-2.64595044665226 -3.1918803849814 0,-2.69350895603976 -3.18011343460661 0,-2.74013851680703 -3.16508156849045 0,-2.7856152816086 -3.14685694779575 0,-2.8297209371489 -3.12552706065018 0,-2.87224375220528 -3.10119430215541 0,-2.91297959405342 -3.07397548283483 0,-2.95173290841587 -3.04400126787917 0,-2.98831765822932 -3.01141554988232 0,-3.02255821672406 -2.97637475807832 0,-3.05429021052832 -2.93904710739563 0,-3.08336130875024 -2.89961179093367 0,-3.10963195424921 -2.85825811973811 0,-3.13297603358643 -2.81518461400453 0,-3.15328148243819 -2.7705980500731 0,-3.1704508235658 -2.72471246778926 0,-3.18440163475959 -2.67774814299567 0,-3.19506694451045 -2.62993053008786 0,-3.20239555350968 -2.58148917971011 0,-3.20635228043361 -2.53265663678705 0,-3.20691813083312 -2.48366732418105 0,-3.20409038831742 -2.43475641733454 0,-3.19788262759417 -2.38615871529951 0,-3.18832464930348 -2.33810751357387 0,-3.1754623369586 -2.29083348415575 0,-3.15935743667996 -2.24456356819194 0,-3.14008726078011 -2.19951988653655 0,-3.11774431662245 -2.15591867344963 0,-3.09243586253532 -2.11396923855472 0,-3.06428339291354 -2.07387296203834 0,-3.03342205497895 -2.03582232791524 0,-3 -2 0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):33
#   EntityHandle (String) = 220
#   POLYGON Z ((-1 -2 -4e-16,-1 -3 -5e-16,-2 -3 -6e-16,-2 -2 -5e-16,-1 -2 -4e-16))
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON Z ((-1 -2 -4e-16,-1 -3 -5e-16,-2 -3 -6e-16,-2 -2 -5e-16,-1 -2 -4e-16))'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):34
#   EntityHandle (String) = 221
#   POLYGON ((-3 -4,-4 -4,-4 -3,-3 -3,-3 -4))
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON ((-3 -4,-4 -4,-4 -3,-3 -3,-3 -4))'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):35
#   EntityHandle (String) = 223
#   POLYGON ((-8 -8,-9 -8,-9 -9,-8 -9,-8 -8))
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON ((-8 -8,-9 -8,-9 -9,-8 -9,-8 -8))'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):36
#   EntityHandle (String) = 224
#   LINESTRING (-2 -2,-2.15384615384615 -2.15384615384615,-2.30769230769231 -2.30769230769231,-2.46153846153846 -2.46153846153846,-2.61538461538461 -2.61538461538461,-2.76923076923077 -2.76923076923077,-2.92307692307692 -2.92307692307692,-3.07692307692308 -3.07692307692308,-3.23076923076923 -3.23076923076923,-3.38461538461538 -3.38461538461538,-3.53846153846154 -3.53846153846154,-3.69230769230769 -3.69230769230769,-3.84615384615385 -3.84615384615385,-4 -4,-4.15384615384615 -4.15384615384615,-4.30769230769231 -4.30769230769231,-4.46153846153846 -4.46153846153846,-4.61538461538462 -4.61538461538462,-4.76923076923077 -4.76923076923077,-4.92307692307692 -4.92307692307692,-5.07692307692308 -5.07692307692308,-5.23076923076923 -5.23076923076923,-5.38461538461538 -5.38461538461538,-5.53846153846154 -5.53846153846154,-5.69230769230769 -5.69230769230769,-5.84615384615385 -5.84615384615385,-6.0 -6.0,-6.15384615384615 -6.15384615384615,-6.30769230769231 -6.30769230769231,-6.46153846153846 -6.46153846153846,-6.61538461538462 -6.61538461538462,-6.76923076923077 -6.76923076923077,-6.92307692307692 -6.92307692307692,-7.07692307692308 -7.07692307692308,-7.23076923076923 -7.23076923076923,-7.38461538461539 -7.38461538461539,-7.53846153846154 -7.53846153846154,-7.69230769230769 -7.69230769230769,-7.84615384615385 -7.84615384615385,-8 -8)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (-2 -2,-2.15384615384615 -2.15384615384615,-2.30769230769231 -2.30769230769231,-2.46153846153846 -2.46153846153846,-2.61538461538461 -2.61538461538461,-2.76923076923077 -2.76923076923077,-2.92307692307692 -2.92307692307692,-3.07692307692308 -3.07692307692308,-3.23076923076923 -3.23076923076923,-3.38461538461538 -3.38461538461538,-3.53846153846154 -3.53846153846154,-3.69230769230769 -3.69230769230769,-3.84615384615385 -3.84615384615385,-4 -4,-4.15384615384615 -4.15384615384615,-4.30769230769231 -4.30769230769231,-4.46153846153846 -4.46153846153846,-4.61538461538462 -4.61538461538462,-4.76923076923077 -4.76923076923077,-4.92307692307692 -4.92307692307692,-5.07692307692308 -5.07692307692308,-5.23076923076923 -5.23076923076923,-5.38461538461538 -5.38461538461538,-5.53846153846154 -5.53846153846154,-5.69230769230769 -5.69230769230769,-5.84615384615385 -5.84615384615385,-6.0 -6.0,-6.15384615384615 -6.15384615384615,-6.30769230769231 -6.30769230769231,-6.46153846153846 -6.46153846153846,-6.61538461538462 -6.61538461538462,-6.76923076923077 -6.76923076923077,-6.92307692307692 -6.92307692307692,-7.07692307692308 -7.07692307692308,-7.23076923076923 -7.23076923076923,-7.38461538461539 -7.38461538461539,-7.53846153846154 -7.53846153846154,-7.69230769230769 -7.69230769230769,-7.84615384615385 -7.84615384615385,-8 -8)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):37
#   EntityHandle (String) = 225
#   LINESTRING (-8 -1,-7.62837370825536 -0.987348067229724,-7.25775889681215 -0.975707614760869,-6.88916704597178 -0.966090122894857,-6.52360963603567 -0.959507071933107,-6.16209814730525 -0.956969942177043,-5.80564406008193 -0.959490213928084,-5.45525885466714 -0.968079367487651,-5.11195401136229 -0.983748883157167,-4.77674101046882 -1.00751024123805,-4.45063133228814 -1.04037492203173,-4.13463645712167 -1.08335440583961,-3.82976786527082 -1.13746017296313,-3.53703703703704 -1.2037037037037,-3.25745545272173 -1.28309647836275,-2.99203459262631 -1.37664997724169,-2.74178593705221 -1.48537568064195,-2.50772096630085 -1.61028506886495,-2.29085116067365 -1.75238962221211,-2.09218800047203 -1.91270082098484,-1.91270082098485 -2.09218800047202,-1.75238962221211 -2.29085116067364,-1.61028506886495 -2.50772096630085,-1.48537568064195 -2.74178593705221,-1.37664997724169 -2.99203459262631,-1.28309647836275 -3.25745545272172,-1.2037037037037 -3.53703703703703,-1.13746017296313 -3.82976786527082,-1.08335440583961 -4.13463645712166,-1.04037492203173 -4.45063133228814,-1.00751024123805 -4.77674101046882,-0.983748883157167 -5.11195401136229,-0.968079367487652 -5.45525885466714,-0.959490213928084 -5.80564406008193,-0.956969942177043 -6.16209814730525,-0.959507071933108 -6.52360963603567,-0.966090122894857 -6.88916704597178,-0.975707614760869 -7.25775889681216,-0.987348067229724 -7.62837370825537,-1 -8)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (-8 -1,-7.62837370825536 -0.987348067229724,-7.25775889681215 -0.975707614760869,-6.88916704597178 -0.966090122894857,-6.52360963603567 -0.959507071933107,-6.16209814730525 -0.956969942177043,-5.80564406008193 -0.959490213928084,-5.45525885466714 -0.968079367487651,-5.11195401136229 -0.983748883157167,-4.77674101046882 -1.00751024123805,-4.45063133228814 -1.04037492203173,-4.13463645712167 -1.08335440583961,-3.82976786527082 -1.13746017296313,-3.53703703703704 -1.2037037037037,-3.25745545272173 -1.28309647836275,-2.99203459262631 -1.37664997724169,-2.74178593705221 -1.48537568064195,-2.50772096630085 -1.61028506886495,-2.29085116067365 -1.75238962221211,-2.09218800047203 -1.91270082098484,-1.91270082098485 -2.09218800047202,-1.75238962221211 -2.29085116067364,-1.61028506886495 -2.50772096630085,-1.48537568064195 -2.74178593705221,-1.37664997724169 -2.99203459262631,-1.28309647836275 -3.25745545272172,-1.2037037037037 -3.53703703703703,-1.13746017296313 -3.82976786527082,-1.08335440583961 -4.13463645712166,-1.04037492203173 -4.45063133228814,-1.00751024123805 -4.77674101046882,-0.983748883157167 -5.11195401136229,-0.968079367487652 -5.45525885466714,-0.959490213928084 -5.80564406008193,-0.956969942177043 -6.16209814730525,-0.959507071933108 -6.52360963603567,-0.966090122894857 -6.88916704597178,-0.975707614760869 -7.25775889681216,-0.987348067229724 -7.62837370825537,-1 -8)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):38
#   EntityHandle (String) = 226
#   POINT Z (-7 -7 0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POINT Z (-7 -7 0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):39
#   EntityHandle (String) = 227
#   POINT Z (4 -4 -5e-16)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POINT Z (4 -4 -5e-16)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):40
#   EntityHandle (String) = 228
#   LINESTRING Z (0 0 0,1 -1 -1e-16)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (0 0 0,1 -1 -1e-16)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):41
#   EntityHandle (String) = 229
#   LINESTRING (1 -1,2 -1,1 -2,1 -1)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (1 -1,2 -1,1 -2,1 -1)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):42
#   EntityHandle (String) = 22A
#   LINESTRING Z (1 -1 -1e-16,1 -2 -2e-16,2 -2 -2e-16,1 -1 -1e-16)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (1 -1 -1e-16,1 -2 -2e-16,2 -2 -2e-16,1 -1 -1e-16)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):43
#   EntityHandle (String) = 22F
#   LINESTRING Z (2 -4 -4e-16,2.00487189948035 -4.13951294748825 -4.13951294748825e-16,2.01946386251686 -4.27834620192013 -4.27834620192013e-16,2.04370479853239 -4.41582338163552 -4.41582338163552e-16,2.07747660812336 -4.551274711634 -4.551274711634e-16,2.12061475842818 -4.68404028665134 -4.68404028665134e-16,2.1729090847148 -4.8134732861516 -4.8134732861516e-16,2.23410481428215 -4.93894312557178 -4.93894312557178e-16,2.30390380768715 -5.05983852846641 -5.05983852846641e-16,2.38196601125011 -5.17557050458495 -5.17557050458495e-16,2.46791111376204 -5.28557521937308 -5.28557521937308e-16,2.5613203993227 -5.38931674091799 -5.38931674091799e-16,2.66173878728228 -5.48628965095479 -5.48628965095479e-16,2.76867704934868 -5.57602150721344 -5.57602150721344e-16,2.88161419305851 -5.65807514511008 -5.65807514511008e-16,3.0 -5.73205080756888 -5.73205080756888e-16,3.12325770642185 -5.79758809259833 -5.79758809259833e-16,3.25078681316818 -5.85436770913357 -5.85436770913357e-16,3.38196601125011 -5.90211303259031 -5.90211303259031e-16,3.51615620880066 -5.94059145255199 -5.94059145255199e-16,3.65270364466614 -5.96961550602442 -5.96961550602442e-16,3.79094307346469 -5.98904379073655 -5.98904379073655e-16,3.930201006595 -5.99878165403819 -5.99878165403819e-16,4.069798993405 -5.99878165403819 -5.99878165403819e-16,4.20905692653531 -5.98904379073655 -5.98904379073655e-16,4.34729635533386 -5.96961550602442 -5.96961550602442e-16,4.48384379119934 -5.94059145255199 -5.94059145255199e-16,4.61803398874989 -5.90211303259031 -5.90211303259031e-16,4.74921318683182 -5.85436770913357 -5.85436770913357e-16,4.87674229357815 -5.79758809259833 -5.79758809259833e-16,5.0 -5.73205080756888 -5.73205080756888e-16,5.11838580694149 -5.65807514511008 -5.65807514511008e-16,5.23132295065132 -5.57602150721344 -5.57602150721344e-16,5.33826121271772 -5.48628965095479 -5.48628965095479e-16,5.4386796006773 -5.38931674091799 -5.38931674091799e-16,5.53208888623796 -5.28557521937308 -5.28557521937308e-16,5.61803398874989 -5.17557050458495 -5.17557050458495e-16,5.69609619231285 -5.05983852846641 -5.05983852846641e-16,5.76589518571785 -4.93894312557178 -4.93894312557178e-16,5.8270909152852 -4.8134732861516 -4.8134732861516e-16,5.87938524157182 -4.68404028665134 -4.68404028665134e-16,5.92252339187664 -4.551274711634 -4.551274711634e-16,5.95629520146761 -4.41582338163552 -4.41582338163552e-16,5.98053613748314 -4.27834620192013 -4.27834620192013e-16,5.99512810051965 -4.13951294748825 -4.13951294748825e-16,6 -4 -4e-16,5.99512810051965 -3.86048705251175 -3.86048705251175e-16,5.98053613748314 -3.72165379807987 -3.72165379807987e-16,5.95629520146761 -3.58417661836448 -3.58417661836448e-16,5.92252339187664 -3.448725288366 -3.448725288366e-16,
#5.87938524157182 -3.31595971334866 -3.31595971334866e-16,5.8270909152852 -3.1865267138484 -3.1865267138484e-16,5.76589518571785 -3.06105687442822 -3.06105687442822e-16,5.69609619231285 -2.94016147153359 -2.94016147153359e-16,5.61803398874989 -2.82442949541505 -2.82442949541505e-16,5.53208888623796 -2.71442478062692 -2.71442478062692e-16,5.4386796006773 -2.61068325908201 -2.61068325908201e-16,5.33826121271772 -2.51371034904521 -2.51371034904521e-16,5.23132295065132 -2.42397849278656 -2.42397849278656e-16,5.11838580694149 -2.34192485488992 -2.34192485488992e-16,5.0 -2.26794919243112 -2.26794919243112e-16,4.87674229357816 -2.20241190740167 -2.20241190740167e-16,4.74921318683182 -2.14563229086643 -2.14563229086643e-16,4.61803398874989 -2.09788696740969 -2.09788696740969e-16,4.48384379119934 -2.05940854744801 -2.05940854744801e-16,4.34729635533386 -2.03038449397558 -2.03038449397558e-16,4.20905692653531 -2.01095620926345 -2.01095620926345e-16,4.069798993405 -2.00121834596181 -2.00121834596181e-16,3.930201006595 -2.00121834596181 -2.00121834596181e-16,3.79094307346469 -2.01095620926345 -2.01095620926345e-16,3.65270364466614 -2.03038449397558 -2.03038449397558e-16,3.51615620880067 -2.05940854744801 -2.05940854744801e-16,3.38196601125011 -2.09788696740969 -2.09788696740969e-16,3.25078681316818 -2.14563229086643 -2.14563229086643e-16,3.12325770642185 -2.20241190740167 -2.20241190740167e-16,3.0 -2.26794919243112 -2.26794919243112e-16,2.88161419305851 -2.34192485488992 -2.34192485488992e-16,2.76867704934868 -2.42397849278656 -2.42397849278656e-16,2.66173878728228 -2.51371034904521 -2.51371034904521e-16,2.5613203993227 -2.610683259082 -2.610683259082e-16,2.46791111376204 -2.71442478062692 -2.71442478062692e-16,2.38196601125011 -2.82442949541505 -2.82442949541505e-16,2.30390380768715 -2.94016147153359 -2.94016147153359e-16,2.23410481428215 -3.06105687442822 -3.06105687442822e-16,2.1729090847148 -3.1865267138484 -3.1865267138484e-16,2.12061475842818 -3.31595971334866 -3.31595971334866e-16,2.07747660812336 -3.448725288366 -3.448725288366e-16,2.04370479853239 -3.58417661836448 -3.58417661836448e-16,2.01946386251686 -3.72165379807987 -3.72165379807987e-16,2.00487189948035 -3.86048705251175 -3.86048705251175e-16,2.0 -4.0 -4e-16)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (2 -4 -4e-16,2.00487189948035 -4.13951294748825 -4.13951294748825e-16,2.01946386251686 -4.27834620192013 -4.27834620192013e-16,2.04370479853239 -4.41582338163552 -4.41582338163552e-16,2.07747660812336 -4.551274711634 -4.551274711634e-16,2.12061475842818 -4.68404028665134 -4.68404028665134e-16,2.1729090847148 -4.8134732861516 -4.8134732861516e-16,2.23410481428215 -4.93894312557178 -4.93894312557178e-16,2.30390380768715 -5.05983852846641 -5.05983852846641e-16,2.38196601125011 -5.17557050458495 -5.17557050458495e-16,2.46791111376204 -5.28557521937308 -5.28557521937308e-16,2.5613203993227 -5.38931674091799 -5.38931674091799e-16,2.66173878728228 -5.48628965095479 -5.48628965095479e-16,2.76867704934868 -5.57602150721344 -5.57602150721344e-16,2.88161419305851 -5.65807514511008 -5.65807514511008e-16,3.0 -5.73205080756888 -5.73205080756888e-16,3.12325770642185 -5.79758809259833 -5.79758809259833e-16,3.25078681316818 -5.85436770913357 -5.85436770913357e-16,3.38196601125011 -5.90211303259031 -5.90211303259031e-16,3.51615620880066 -5.94059145255199 -5.94059145255199e-16,3.65270364466614 -5.96961550602442 -5.96961550602442e-16,3.79094307346469 -5.98904379073655 -5.98904379073655e-16,3.930201006595 -5.99878165403819 -5.99878165403819e-16,4.069798993405 -5.99878165403819 -5.99878165403819e-16,4.20905692653531 -5.98904379073655 -5.98904379073655e-16,4.34729635533386 -5.96961550602442 -5.96961550602442e-16,4.48384379119934 -5.94059145255199 -5.94059145255199e-16,4.61803398874989 -5.90211303259031 -5.90211303259031e-16,4.74921318683182 -5.85436770913357 -5.85436770913357e-16,4.87674229357815 -5.79758809259833 -5.79758809259833e-16,5.0 -5.73205080756888 -5.73205080756888e-16,5.11838580694149 -5.65807514511008 -5.65807514511008e-16,5.23132295065132 -5.57602150721344 -5.57602150721344e-16,5.33826121271772 -5.48628965095479 -5.48628965095479e-16,5.4386796006773 -5.38931674091799 -5.38931674091799e-16,5.53208888623796 -5.28557521937308 -5.28557521937308e-16,5.61803398874989 -5.17557050458495 -5.17557050458495e-16,5.69609619231285 -5.05983852846641 -5.05983852846641e-16,5.76589518571785 -4.93894312557178 -4.93894312557178e-16,5.8270909152852 -4.8134732861516 -4.8134732861516e-16,5.87938524157182 -4.68404028665134 -4.68404028665134e-16,5.92252339187664 -4.551274711634 -4.551274711634e-16,5.95629520146761 -4.41582338163552 -4.41582338163552e-16,5.98053613748314 -4.27834620192013 -4.27834620192013e-16,5.99512810051965 -4.13951294748825 -4.13951294748825e-16,6 -4 -4e-16,5.99512810051965 -3.86048705251175 -3.86048705251175e-16,5.98053613748314 -3.72165379807987 -3.72165379807987e-16,5.95629520146761 -3.58417661836448 -3.58417661836448e-16,5.92252339187664 -3.448725288366 -3.448725288366e-16,5.87938524157182 -3.31595971334866 -3.31595971334866e-16,5.8270909152852 -3.1865267138484 -3.1865267138484e-16,5.76589518571785 -3.06105687442822 -3.06105687442822e-16,5.69609619231285 -2.94016147153359 -2.94016147153359e-16,5.61803398874989 -2.82442949541505 -2.82442949541505e-16,5.53208888623796 -2.71442478062692 -2.71442478062692e-16,5.4386796006773 -2.61068325908201 -2.61068325908201e-16,5.33826121271772 -2.51371034904521 -2.51371034904521e-16,' + \
        '5.23132295065132 -2.42397849278656 -2.42397849278656e-16,5.11838580694149 -2.34192485488992 -2.34192485488992e-16,5.0 -2.26794919243112 -2.26794919243112e-16,4.87674229357816 -2.20241190740167 -2.20241190740167e-16,4.74921318683182 -2.14563229086643 -2.14563229086643e-16,4.61803398874989 -2.09788696740969 -2.09788696740969e-16,4.48384379119934 -2.05940854744801 -2.05940854744801e-16,4.34729635533386 -2.03038449397558 -2.03038449397558e-16,4.20905692653531 -2.01095620926345 -2.01095620926345e-16,4.069798993405 -2.00121834596181 -2.00121834596181e-16,3.930201006595 -2.00121834596181 -2.00121834596181e-16,3.79094307346469 -2.01095620926345 -2.01095620926345e-16,3.65270364466614 -2.03038449397558 -2.03038449397558e-16,3.51615620880067 -2.05940854744801 -2.05940854744801e-16,3.38196601125011 -2.09788696740969 -2.09788696740969e-16,3.25078681316818 -2.14563229086643 -2.14563229086643e-16,3.12325770642185 -2.20241190740167 -2.20241190740167e-16,3.0 -2.26794919243112 -2.26794919243112e-16,2.88161419305851 -2.34192485488992 -2.34192485488992e-16,2.76867704934868 -2.42397849278656 -2.42397849278656e-16,2.66173878728228 -2.51371034904521 -2.51371034904521e-16,2.5613203993227 -2.610683259082 -2.610683259082e-16,2.46791111376204 -2.71442478062692 -2.71442478062692e-16,2.38196601125011 -2.82442949541505 -2.82442949541505e-16,2.30390380768715 -2.94016147153359 -2.94016147153359e-16,2.23410481428215 -3.06105687442822 -3.06105687442822e-16,2.1729090847148 -3.1865267138484 -3.1865267138484e-16,2.12061475842818 -3.31595971334866 -3.31595971334866e-16,2.07747660812336 -3.448725288366 -3.448725288366e-16,2.04370479853239 -3.58417661836448 -3.58417661836448e-16,2.01946386251686 -3.72165379807987 -3.72165379807987e-16,2.00487189948035 -3.86048705251175 -3.86048705251175e-16,2.0 -4.0 -4e-16)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):44
#   EntityHandle (String) = 230
#   LINESTRING Z (2 -4 -5e-16,2.00487189948035 -4.06975647374412 -5.06975647374413e-16,2.01946386251686 -4.13917310096007 -5.13917310096007e-16,2.04370479853239 -4.20791169081776 -5.20791169081776e-16,2.07747660812336 -4.275637355817 -5.275637355817e-16,2.12061475842818 -4.34202014332567 -5.34202014332567e-16,2.1729090847148 -4.4067366430758 -5.4067366430758e-16,2.23410481428215 -4.46947156278589 -5.46947156278589e-16,2.30390380768715 -4.52991926423321 -5.52991926423321e-16,2.38196601125011 -4.58778525229247 -5.58778525229247e-16,2.46791111376204 -4.64278760968654 -5.64278760968654e-16,2.5613203993227 -4.694658370459 -5.694658370459e-16,2.66173878728228 -4.74314482547739 -5.7431448254774e-16,2.76867704934868 -4.78801075360672 -5.78801075360672e-16,2.88161419305851 -4.82903757255504 -5.82903757255504e-16,3.0 -4.86602540378444 -5.86602540378444e-16,3.12325770642185 -4.89879404629917 -5.89879404629917e-16,3.25078681316818 -4.92718385456679 -5.92718385456679e-16,3.38196601125011 -4.95105651629515 -5.95105651629515e-16,3.51615620880067 -4.970295726276 -5.970295726276e-16,3.65270364466614 -4.98480775301221 -5.98480775301221e-16,3.79094307346469 -4.99452189536827 -5.99452189536827e-16,3.930201006595 -4.9993908270191 -5.9993908270191e-16,4.069798993405 -4.9993908270191 -5.9993908270191e-16,4.20905692653531 -4.99452189536827 -5.99452189536827e-16,4.34729635533386 -4.98480775301221 -5.98480775301221e-16,4.48384379119934 -4.970295726276 -5.970295726276e-16,4.61803398874989 -4.95105651629515 -5.95105651629515e-16,4.74921318683182 -4.92718385456679 -5.92718385456679e-16,4.87674229357816 -4.89879404629917 -5.89879404629917e-16,5.0 -4.86602540378444 -5.86602540378444e-16,5.11838580694149 -4.82903757255504 -5.82903757255504e-16,5.23132295065132 -4.78801075360672 -5.78801075360672e-16,5.33826121271772 -4.74314482547739 -5.74314482547739e-16,5.4386796006773 -4.694658370459 -5.694658370459e-16,5.53208888623796 -4.64278760968654 -5.64278760968654e-16,5.61803398874989 -4.58778525229247 -5.58778525229247e-16,5.69609619231285 -4.5299192642332 -5.52991926423321e-16,5.76589518571785 -4.46947156278589 -5.46947156278589e-16,5.8270909152852 -4.4067366430758 -5.4067366430758e-16,5.87938524157182 -4.34202014332567 -5.34202014332567e-16,5.92252339187664 -4.275637355817 -5.275637355817e-16,5.95629520146761 -4.20791169081776 -5.20791169081776e-16,5.98053613748314 -4.13917310096007 -5.13917310096007e-16,5.99512810051965 -4.06975647374412 -5.06975647374413e-16,6 -4 -5e-16,5.99512810051965 -3.93024352625587 -4.93024352625588e-16,5.98053613748314 -3.86082689903993 -4.86082689903993e-16,5.95629520146761 -3.79208830918224 -4.79208830918224e-16,5.92252339187664 -3.724362644183 -4.724362644183e-16,5.87938524157182 -3.65797985667433 -4.65797985667433e-16,5.8270909152852 -3.5932633569242 -4.5932633569242e-16,5.76589518571785 -3.53052843721411 -4.53052843721411e-16,
#5.69609619231285 -3.4700807357668 -4.4700807357668e-16,5.61803398874989 -3.41221474770753 -4.41221474770753e-16,5.53208888623796 -3.35721239031346 -4.35721239031346e-16,5.4386796006773 -3.305341629541 -4.305341629541e-16,5.33826121271772 -3.25685517452261 -4.25685517452261e-16,5.23132295065132 -3.21198924639328 -4.21198924639328e-16,5.11838580694149 -3.17096242744496 -4.17096242744496e-16,5.0 -3.13397459621556 -4.13397459621556e-16,4.87674229357815 -3.10120595370083 -4.10120595370083e-16,4.74921318683182 -3.07281614543321 -4.07281614543321e-16,4.61803398874989 -3.04894348370485 -4.04894348370485e-16,4.48384379119934 -3.029704273724 -4.029704273724e-16,4.34729635533386 -3.01519224698779 -4.01519224698779e-16,4.20905692653531 -3.00547810463173 -4.00547810463173e-16,4.069798993405 -3.0006091729809 -4.0006091729809e-16,3.930201006595 -3.0006091729809 -4.0006091729809e-16,3.79094307346469 -3.00547810463173 -4.00547810463173e-16,3.65270364466614 -3.01519224698779 -4.01519224698779e-16,3.51615620880066 -3.029704273724 -4.029704273724e-16,3.38196601125011 -3.04894348370485 -4.04894348370485e-16,3.25078681316818 -3.07281614543321 -4.07281614543321e-16,3.12325770642185 -3.10120595370083 -4.10120595370083e-16,3.0 -3.13397459621556 -4.13397459621556e-16,2.88161419305851 -3.17096242744496 -4.17096242744496e-16,2.76867704934868 -3.21198924639328 -4.21198924639328e-16,2.66173878728228 -3.25685517452261 -4.25685517452261e-16,2.5613203993227 -3.305341629541 -4.305341629541e-16,2.46791111376204 -3.35721239031346 -4.35721239031346e-16,2.38196601125011 -3.41221474770753 -4.41221474770753e-16,2.30390380768715 -3.4700807357668 -4.4700807357668e-16,2.23410481428215 -3.53052843721411 -4.53052843721411e-16,2.1729090847148 -3.5932633569242 -4.5932633569242e-16,2.12061475842818 -3.65797985667433 -4.65797985667433e-16,2.07747660812336 -3.724362644183 -4.724362644183e-16,2.04370479853239 -3.79208830918224 -4.79208830918224e-16,2.01946386251686 -3.86082689903993 -4.86082689903993e-16,2.00487189948035 -3.93024352625587 -4.93024352625588e-16,2 -4 -5e-16)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (2 -4 -5e-16,2.00487189948035 -4.06975647374412 -5.06975647374413e-16,2.01946386251686 -4.13917310096007 -5.13917310096007e-16,2.04370479853239 -4.20791169081776 -5.20791169081776e-16,2.07747660812336 -4.275637355817 -5.275637355817e-16,2.12061475842818 -4.34202014332567 -5.34202014332567e-16,2.1729090847148 -4.4067366430758 -5.4067366430758e-16,2.23410481428215 -4.46947156278589 -5.46947156278589e-16,2.30390380768715 -4.52991926423321 -5.52991926423321e-16,2.38196601125011 -4.58778525229247 -5.58778525229247e-16,2.46791111376204 -4.64278760968654 -5.64278760968654e-16,2.5613203993227 -4.694658370459 -5.694658370459e-16,2.66173878728228 -4.74314482547739 -5.7431448254774e-16,2.76867704934868 -4.78801075360672 -5.78801075360672e-16,2.88161419305851 -4.82903757255504 -5.82903757255504e-16,3.0 -4.86602540378444 -5.86602540378444e-16,3.12325770642185 -4.89879404629917 -5.89879404629917e-16,3.25078681316818 -4.92718385456679 -5.92718385456679e-16,3.38196601125011 -4.95105651629515 -5.95105651629515e-16,3.51615620880067 -4.970295726276 -5.970295726276e-16,3.65270364466614 -4.98480775301221 -5.98480775301221e-16,3.79094307346469 -4.99452189536827 -5.99452189536827e-16,3.930201006595 -4.9993908270191 -5.9993908270191e-16,4.069798993405 -4.9993908270191 -5.9993908270191e-16,4.20905692653531 -4.99452189536827 -5.99452189536827e-16,4.34729635533386 -4.98480775301221 -5.98480775301221e-16,4.48384379119934 -4.970295726276 -5.970295726276e-16,4.61803398874989 -4.95105651629515 -5.95105651629515e-16,4.74921318683182 -4.92718385456679 -5.92718385456679e-16,4.87674229357816 -4.89879404629917 -5.89879404629917e-16,5.0 -4.86602540378444 -5.86602540378444e-16,5.11838580694149 -4.82903757255504 -5.82903757255504e-16,5.23132295065132 -4.78801075360672 -5.78801075360672e-16,5.33826121271772 -4.74314482547739 -5.74314482547739e-16,5.4386796006773 -4.694658370459 -5.694658370459e-16,5.53208888623796 -4.64278760968654 -5.64278760968654e-16,5.61803398874989 -4.58778525229247 -5.58778525229247e-16,5.69609619231285 -4.5299192642332 -5.52991926423321e-16,5.76589518571785 -4.46947156278589 -5.46947156278589e-16,5.8270909152852 -4.4067366430758 -5.4067366430758e-16,5.87938524157182 -4.34202014332567 -5.34202014332567e-16,5.92252339187664 -4.275637355817 -5.275637355817e-16,5.95629520146761 -4.20791169081776 -5.20791169081776e-16,5.98053613748314 -4.13917310096007 -5.13917310096007e-16,5.99512810051965 -4.06975647374412 -5.06975647374413e-16,6 -4 -5e-16,5.99512810051965 -3.93024352625587 -4.93024352625588e-16,5.98053613748314 -3.86082689903993 -4.86082689903993e-16,5.95629520146761 -3.79208830918224 -4.79208830918224e-16,5.92252339187664 -3.724362644183 -4.724362644183e-16,5.87938524157182 -3.65797985667433 -4.65797985667433e-16,5.8270909152852 -3.5932633569242 -4.5932633569242e-16,5.76589518571785 -3.53052843721411 -4.53052843721411e-16,5.69609619231285 -3.4700807357668 -4.4700807357668e-16,5.61803398874989 -3.41221474770753 -4.41221474770753e-16,' + \
        '5.53208888623796 -3.35721239031346 -4.35721239031346e-16,5.4386796006773 -3.305341629541 -4.305341629541e-16,5.33826121271772 -3.25685517452261 -4.25685517452261e-16,5.23132295065132 -3.21198924639328 -4.21198924639328e-16,5.11838580694149 -3.17096242744496 -4.17096242744496e-16,5.0 -3.13397459621556 -4.13397459621556e-16,4.87674229357815 -3.10120595370083 -4.10120595370083e-16,4.74921318683182 -3.07281614543321 -4.07281614543321e-16,4.61803398874989 -3.04894348370485 -4.04894348370485e-16,4.48384379119934 -3.029704273724 -4.029704273724e-16,4.34729635533386 -3.01519224698779 -4.01519224698779e-16,4.20905692653531 -3.00547810463173 -4.00547810463173e-16,4.069798993405 -3.0006091729809 -4.0006091729809e-16,3.930201006595 -3.0006091729809 -4.0006091729809e-16,3.79094307346469 -3.00547810463173 -4.00547810463173e-16,3.65270364466614 -3.01519224698779 -4.01519224698779e-16,3.51615620880066 -3.029704273724 -4.029704273724e-16,3.38196601125011 -3.04894348370485 -4.04894348370485e-16,3.25078681316818 -3.07281614543321 -4.07281614543321e-16,3.12325770642185 -3.10120595370083 -4.10120595370083e-16,3.0 -3.13397459621556 -4.13397459621556e-16,2.88161419305851 -3.17096242744496 -4.17096242744496e-16,2.76867704934868 -3.21198924639328 -4.21198924639328e-16,2.66173878728228 -3.25685517452261 -4.25685517452261e-16,2.5613203993227 -3.305341629541 -4.305341629541e-16,2.46791111376204 -3.35721239031346 -4.35721239031346e-16,2.38196601125011 -3.41221474770753 -4.41221474770753e-16,2.30390380768715 -3.4700807357668 -4.4700807357668e-16,2.23410481428215 -3.53052843721411 -4.53052843721411e-16,2.1729090847148 -3.5932633569242 -4.5932633569242e-16,2.12061475842818 -3.65797985667433 -4.65797985667433e-16,2.07747660812336 -3.724362644183 -4.724362644183e-16,2.04370479853239 -3.79208830918224 -4.79208830918224e-16,2.01946386251686 -3.86082689903993 -4.86082689903993e-16,2.00487189948035 -3.93024352625587 -4.93024352625588e-16,2 -4 -5e-16)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):45
#   EntityHandle (String) = 231
#   LINESTRING Z (2 -2 -2e-16,1.96657794502105 -2.03582232791524 -2.03582232791524e-16,1.93571660708646 -2.07387296203834 -2.07387296203834e-16,1.90756413746468 -2.11396923855472 -2.11396923855472e-16,1.88225568337755 -2.15591867344963 -2.15591867344963e-16,1.85991273921989 -2.19951988653655 -2.19951988653655e-16,1.84064256332004 -2.24456356819194 -2.24456356819194e-16,1.8245376630414 -2.29083348415575 -2.29083348415575e-16,1.81167535069652 -2.33810751357387 -2.33810751357387e-16,1.80211737240583 -2.38615871529951 -2.38615871529951e-16,1.79590961168258 -2.43475641733454 -2.43475641733454e-16,1.79308186916688 -2.48366732418105 -2.48366732418105e-16,1.79364771956639 -2.53265663678705 -2.53265663678705e-16,1.79760444649032 -2.58148917971011 -2.58148917971011e-16,1.80493305548955 -2.62993053008786 -2.62993053008786e-16,1.81559836524041 -2.67774814299567 -2.67774814299567e-16,1.8295491764342 -2.72471246778926 -2.72471246778926e-16,1.84671851756181 -2.7705980500731 -2.7705980500731e-16,1.86702396641357 -2.81518461400453 -2.81518461400453e-16,1.89036804575079 -2.85825811973811 -2.85825811973811e-16,1.91663869124976 -2.89961179093367 -2.89961179093367e-16,1.94570978947168 -2.93904710739563 -2.93904710739563e-16,1.97744178327594 -2.97637475807832 -2.97637475807832e-16,2.01168234177068 -3.01141554988232 -3.01141554988232e-16,2.04826709158413 -3.04400126787917 -3.04400126787917e-16,2.08702040594658 -3.07397548283483 -3.07397548283483e-16,2.12775624779472 -3.10119430215541 -3.10119430215541e-16,2.1702790628511 -3.12552706065018 -3.12552706065018e-16,2.2143847183914 -3.14685694779575 -3.14685694779575e-16,2.25986148319297 -3.16508156849045 -3.16508156849045e-16,2.30649104396024 -3.18011343460661 -3.18011343460661e-16,2.35404955334774 -3.1918803849814 -3.1918803849814e-16,2.40230870454951 -3.20032593182975 -3.20032593182975e-16,2.45103682729644 -3.2054095319166 -3.2054095319166e-16,2.5 -3.20710678118655 -3.20710678118655e-16,2.54896317270356 -3.2054095319166 -3.2054095319166e-16,2.59769129545049 -3.20032593182975 -3.20032593182975e-16,2.64595044665226 -3.1918803849814 -3.1918803849814e-16,2.69350895603976 -3.18011343460661 -3.18011343460661e-16,2.74013851680703 -3.16508156849045 -3.16508156849045e-16,2.7856152816086 -3.14685694779575 -3.14685694779575e-16,2.8297209371489 -3.12552706065018 -3.12552706065018e-16,2.87224375220528 -3.10119430215541 -3.10119430215541e-16,2.91297959405342 -3.07397548283483 -3.07397548283483e-16,2.95173290841587 -3.04400126787917 -3.04400126787917e-16,2.98831765822932 -3.01141554988232 -3.01141554988232e-16,3.02255821672406 -2.97637475807832 -2.97637475807832e-16,3.05429021052832 -2.93904710739563 -2.93904710739563e-16,3.08336130875024 -2.89961179093367 -2.89961179093367e-16,3.10963195424921 -2.85825811973811 -2.85825811973811e-16,3.13297603358643 -2.81518461400453 -2.81518461400453e-16,3.15328148243819 -2.7705980500731 -2.7705980500731e-16,3.1704508235658 -2.72471246778926 -2.72471246778926e-16,3.18440163475959 -2.67774814299567 -2.67774814299567e-16,3.19506694451045 -2.62993053008786 -2.62993053008786e-16,3.20239555350968 -2.58148917971011 -2.58148917971011e-16,3.20635228043361 -2.53265663678705 -2.53265663678705e-16,3.20691813083312 -2.48366732418105 -2.48366732418105e-16,3.20409038831742 -2.43475641733454 -2.43475641733454e-16,3.19788262759417 -2.38615871529951 -2.38615871529951e-16,3.18832464930348 -2.33810751357387 -2.33810751357387e-16,3.1754623369586 -2.29083348415575 -2.29083348415575e-16,3.15935743667996 -2.24456356819194 -2.24456356819194e-16,3.14008726078011 -2.19951988653655 -2.19951988653655e-16,3.11774431662245 -2.15591867344963 -2.15591867344963e-16,3.09243586253532 -2.11396923855472 -2.11396923855471e-16,3.06428339291354 -2.07387296203834 -2.07387296203834e-16,3.03342205497895 -2.03582232791524 -2.03582232791524e-16,3 -2 -2e-16)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (2 -2 -2e-16,1.96657794502105 -2.03582232791524 -2.03582232791524e-16,1.93571660708646 -2.07387296203834 -2.07387296203834e-16,1.90756413746468 -2.11396923855472 -2.11396923855472e-16,1.88225568337755 -2.15591867344963 -2.15591867344963e-16,1.85991273921989 -2.19951988653655 -2.19951988653655e-16,1.84064256332004 -2.24456356819194 -2.24456356819194e-16,1.8245376630414 -2.29083348415575 -2.29083348415575e-16,1.81167535069652 -2.33810751357387 -2.33810751357387e-16,1.80211737240583 -2.38615871529951 -2.38615871529951e-16,1.79590961168258 -2.43475641733454 -2.43475641733454e-16,1.79308186916688 -2.48366732418105 -2.48366732418105e-16,1.79364771956639 -2.53265663678705 -2.53265663678705e-16,1.79760444649032 -2.58148917971011 -2.58148917971011e-16,1.80493305548955 -2.62993053008786 -2.62993053008786e-16,1.81559836524041 -2.67774814299567 -2.67774814299567e-16,1.8295491764342 -2.72471246778926 -2.72471246778926e-16,1.84671851756181 -2.7705980500731 -2.7705980500731e-16,1.86702396641357 -2.81518461400453 -2.81518461400453e-16,1.89036804575079 -2.85825811973811 -2.85825811973811e-16,1.91663869124976 -2.89961179093367 -2.89961179093367e-16,1.94570978947168 -2.93904710739563 -2.93904710739563e-16,1.97744178327594 -2.97637475807832 -2.97637475807832e-16,2.01168234177068 -3.01141554988232 -3.01141554988232e-16,2.04826709158413 -3.04400126787917 -3.04400126787917e-16,2.08702040594658 -3.07397548283483 -3.07397548283483e-16,2.12775624779472 -3.10119430215541 -3.10119430215541e-16,2.1702790628511 -3.12552706065018 -3.12552706065018e-16,2.2143847183914 -3.14685694779575 -3.14685694779575e-16,2.25986148319297 -3.16508156849045 -3.16508156849045e-16,2.30649104396024 -3.18011343460661 -3.18011343460661e-16,2.35404955334774 -3.1918803849814 -3.1918803849814e-16,2.40230870454951 -3.20032593182975 -3.20032593182975e-16,2.45103682729644 -3.2054095319166 -3.2054095319166e-16,2.5 -3.20710678118655 -3.20710678118655e-16,2.54896317270356 -3.2054095319166 -3.2054095319166e-16,2.59769129545049 -3.20032593182975 -3.20032593182975e-16,2.64595044665226 -3.1918803849814 -3.1918803849814e-16,2.69350895603976 -3.18011343460661 -3.18011343460661e-16,2.74013851680703 -3.16508156849045 -3.16508156849045e-16,2.7856152816086 -3.14685694779575 -3.14685694779575e-16,2.8297209371489 -3.12552706065018 -3.12552706065018e-16,2.87224375220528 -3.10119430215541 -3.10119430215541e-16,2.91297959405342 -3.07397548283483 -3.07397548283483e-16,2.95173290841587 -3.04400126787917 -3.04400126787917e-16,2.98831765822932 -3.01141554988232 -3.01141554988232e-16,3.02255821672406 -2.97637475807832 -2.97637475807832e-16,3.05429021052832 -2.93904710739563 -2.93904710739563e-16,3.08336130875024 -2.89961179093367 -2.89961179093367e-16,3.10963195424921 -2.85825811973811 -2.85825811973811e-16,3.13297603358643 -2.81518461400453 -2.81518461400453e-16,3.15328148243819 -2.7705980500731 -2.7705980500731e-16,3.1704508235658 -2.72471246778926 -2.72471246778926e-16,3.18440163475959 -2.67774814299567 -2.67774814299567e-16,3.19506694451045 -2.62993053008786 -2.62993053008786e-16,3.20239555350968 -2.58148917971011 -2.58148917971011e-16,3.20635228043361 -2.53265663678705 -2.53265663678705e-16,3.20691813083312 -2.48366732418105 -2.48366732418105e-16,3.20409038831742 -2.43475641733454 -2.43475641733454e-16,3.19788262759417 -2.38615871529951 -2.38615871529951e-16,3.18832464930348 -2.33810751357387 -2.33810751357387e-16,3.1754623369586 -2.29083348415575 -2.29083348415575e-16,3.15935743667996 -2.24456356819194 -2.24456356819194e-16,3.14008726078011 -2.19951988653655 -2.19951988653655e-16,3.11774431662245 -2.15591867344963 -2.15591867344963e-16,3.09243586253532 -2.11396923855472 -2.11396923855471e-16,3.06428339291354 -2.07387296203834 -2.07387296203834e-16,3.03342205497895 -2.03582232791524 -2.03582232791524e-16,3 -2 -2e-16)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):46
#   EntityHandle (String) = 232
#   POLYGON Z ((1 -2 -2e-16,1 -3 -4e-16,2 -3 -4e-16,2 -2 -2e-16,1 -2 -2e-16))
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON Z ((1 -2 -2e-16,1 -3 -4e-16,2 -3 -4e-16,2 -2 -2e-16,1 -2 -2e-16))'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):47
#   EntityHandle (String) = 233
#   POLYGON ((3 -4,4 -4,4 -3,3 -3,3 -4))
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON ((3 -4,4 -4,4 -3,3 -3,3 -4))'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):48
#   EntityHandle (String) = 235
#   POLYGON ((8 -8,9 -8,9 -9,8 -9,8 -8))
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON ((8 -8,9 -8,9 -9,8 -9,8 -8))'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):49
#   EntityHandle (String) = 236
#   LINESTRING (2 -2,2.15384615384615 -2.15384615384615,2.30769230769231 -2.30769230769231,2.46153846153846 -2.46153846153846,2.61538461538461 -2.61538461538461,2.76923076923077 -2.76923076923077,2.92307692307692 -2.92307692307692,3.07692307692308 -3.07692307692308,3.23076923076923 -3.23076923076923,3.38461538461538 -3.38461538461538,3.53846153846154 -3.53846153846154,3.69230769230769 -3.69230769230769,3.84615384615385 -3.84615384615385,4 -4,4.15384615384615 -4.15384615384615,4.30769230769231 -4.30769230769231,4.46153846153846 -4.46153846153846,4.61538461538462 -4.61538461538462,4.76923076923077 -4.76923076923077,4.92307692307692 -4.92307692307692,5.07692307692308 -5.07692307692308,5.23076923076923 -5.23076923076923,5.38461538461538 -5.38461538461538,5.53846153846154 -5.53846153846154,5.69230769230769 -5.69230769230769,5.84615384615385 -5.84615384615385,6.0 -6.0,6.15384615384615 -6.15384615384615,6.30769230769231 -6.30769230769231,6.46153846153846 -6.46153846153846,6.61538461538462 -6.61538461538462,6.76923076923077 -6.76923076923077,6.92307692307692 -6.92307692307692,7.07692307692308 -7.07692307692308,7.23076923076923 -7.23076923076923,7.38461538461539 -7.38461538461539,7.53846153846154 -7.53846153846154,7.69230769230769 -7.69230769230769,7.84615384615385 -7.84615384615385,8 -8)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (2 -2,2.15384615384615 -2.15384615384615,2.30769230769231 -2.30769230769231,2.46153846153846 -2.46153846153846,2.61538461538461 -2.61538461538461,2.76923076923077 -2.76923076923077,2.92307692307692 -2.92307692307692,3.07692307692308 -3.07692307692308,3.23076923076923 -3.23076923076923,3.38461538461538 -3.38461538461538,3.53846153846154 -3.53846153846154,3.69230769230769 -3.69230769230769,3.84615384615385 -3.84615384615385,4 -4,4.15384615384615 -4.15384615384615,4.30769230769231 -4.30769230769231,4.46153846153846 -4.46153846153846,4.61538461538462 -4.61538461538462,4.76923076923077 -4.76923076923077,4.92307692307692 -4.92307692307692,5.07692307692308 -5.07692307692308,5.23076923076923 -5.23076923076923,5.38461538461538 -5.38461538461538,5.53846153846154 -5.53846153846154,5.69230769230769 -5.69230769230769,5.84615384615385 -5.84615384615385,6.0 -6.0,6.15384615384615 -6.15384615384615,6.30769230769231 -6.30769230769231,6.46153846153846 -6.46153846153846,6.61538461538462 -6.61538461538462,6.76923076923077 -6.76923076923077,6.92307692307692 -6.92307692307692,7.07692307692308 -7.07692307692308,7.23076923076923 -7.23076923076923,7.38461538461539 -7.38461538461539,7.53846153846154 -7.53846153846154,7.69230769230769 -7.69230769230769,7.84615384615385 -7.84615384615385,8 -8)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):50
#   EntityHandle (String) = 237
#   LINESTRING (8 -1,7.62837370825536 -0.987348067229724,7.25775889681215 -0.975707614760869,6.88916704597178 -0.966090122894857,6.52360963603567 -0.959507071933107,6.16209814730525 -0.956969942177043,5.80564406008193 -0.959490213928084,5.45525885466714 -0.968079367487651,5.11195401136229 -0.983748883157167,4.77674101046882 -1.00751024123805,4.45063133228814 -1.04037492203173,4.13463645712167 -1.08335440583961,3.82976786527082 -1.13746017296313,3.53703703703704 -1.2037037037037,3.25745545272173 -1.28309647836275,2.99203459262631 -1.37664997724169,2.74178593705221 -1.48537568064195,2.50772096630085 -1.61028506886495,2.29085116067365 -1.75238962221211,2.09218800047203 -1.91270082098484,1.91270082098485 -2.09218800047202,1.75238962221211 -2.29085116067364,1.61028506886495 -2.50772096630085,1.48537568064195 -2.74178593705221,1.37664997724169 -2.99203459262631,1.28309647836275 -3.25745545272172,1.2037037037037 -3.53703703703703,1.13746017296313 -3.82976786527082,1.08335440583961 -4.13463645712166,1.04037492203173 -4.45063133228814,1.00751024123805 -4.77674101046882,0.983748883157167 -5.11195401136229,0.968079367487652 -5.45525885466714,0.959490213928084 -5.80564406008193,0.956969942177043 -6.16209814730525,0.959507071933108 -6.52360963603567,0.966090122894857 -6.88916704597178,0.975707614760869 -7.25775889681216,0.987348067229724 -7.62837370825537,1 -8)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (8 -1,7.62837370825536 -0.987348067229724,7.25775889681215 -0.975707614760869,6.88916704597178 -0.966090122894857,6.52360963603567 -0.959507071933107,6.16209814730525 -0.956969942177043,5.80564406008193 -0.959490213928084,5.45525885466714 -0.968079367487651,5.11195401136229 -0.983748883157167,4.77674101046882 -1.00751024123805,4.45063133228814 -1.04037492203173,4.13463645712167 -1.08335440583961,3.82976786527082 -1.13746017296313,3.53703703703704 -1.2037037037037,3.25745545272173 -1.28309647836275,2.99203459262631 -1.37664997724169,2.74178593705221 -1.48537568064195,2.50772096630085 -1.61028506886495,2.29085116067365 -1.75238962221211,2.09218800047203 -1.91270082098484,1.91270082098485 -2.09218800047202,1.75238962221211 -2.29085116067364,1.61028506886495 -2.50772096630085,1.48537568064195 -2.74178593705221,1.37664997724169 -2.99203459262631,1.28309647836275 -3.25745545272172,1.2037037037037 -3.53703703703703,1.13746017296313 -3.82976786527082,1.08335440583961 -4.13463645712166,1.04037492203173 -4.45063133228814,1.00751024123805 -4.77674101046882,0.983748883157167 -5.11195401136229,0.968079367487652 -5.45525885466714,0.959490213928084 -5.80564406008193,0.956969942177043 -6.16209814730525,0.959507071933108 -6.52360963603567,0.966090122894857 -6.88916704597178,0.975707614760869 -7.25775889681216,0.987348067229724 -7.62837370825537,1 -8)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):51
#   EntityHandle (String) = 238
#   POINT Z (7 -7 -7e-16)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POINT Z (7 -7 -7e-16)'):
        feat.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# OCS2WCS transformations 2

def ogr_dxf_32():

    ds = ogr.Open('data/ocs2wcs2.dxf')
    lyr = ds.GetLayer(0)

# INFO: Open of `ocs2wcs2.dxf' using driver `DXF' successful.

# OGRFeature(entities):0
#   EntityHandle (String) = 1B1
#   POINT Z (4 4 0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POINT Z (4 4 0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):1
#   EntityHandle (String) = 1B2
#   LINESTRING Z (0 0 0,1 1 0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (0 0 0,1 1 0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):2
#   EntityHandle (String) = 1B3
#   LINESTRING (1 1,2 1,1 2,1 1)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (1 1,2 1,1 2,1 1)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):3
#   EntityHandle (String) = 1B4
#   LINESTRING Z (1 1 0,1 2 0,2 2 0,1 1 0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (1 1 0,1 2 0,2 2 0,1 1 0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):4
#   EntityHandle (String) = 1B9
#   LINESTRING Z (6 4 0,5.99512810051965 3.86048705251175 0,5.98053613748314 3.72165379807987 0,5.95629520146761 3.58417661836448 0,5.92252339187664 3.448725288366 0,5.87938524157182 3.31595971334866 0,5.8270909152852 3.1865267138484 0,5.76589518571785 3.06105687442822 0,5.69609619231285 2.94016147153359 0,5.61803398874989 2.82442949541505 0,5.53208888623796 2.71442478062692 0,5.4386796006773 2.61068325908201 0,5.33826121271772 2.51371034904521 0,5.23132295065132 2.42397849278656 0,5.11838580694149 2.34192485488992 0,5.0 2.26794919243112 0,4.87674229357815 2.20241190740167 0,4.74921318683182 2.14563229086643 0,4.61803398874989 2.09788696740969 0,4.48384379119934 2.05940854744801 0,4.34729635533386 2.03038449397558 0,4.20905692653531 2.01095620926345 0,4.069798993405 2.00121834596181 0,3.930201006595 2.00121834596181 0,3.79094307346469 2.01095620926345 0,3.65270364466614 2.03038449397558 0,3.51615620880066 2.05940854744801 0,3.38196601125011 2.09788696740969 0,3.25078681316818 2.14563229086643 0,3.12325770642185 2.20241190740167 0,3.0 2.26794919243112 0,2.88161419305851 2.34192485488992 0,2.76867704934868 2.42397849278656 0,2.66173878728228 2.51371034904521 0,2.5613203993227 2.61068325908201 0,2.46791111376204 2.71442478062692 0,2.38196601125011 2.82442949541505 0,2.30390380768715 2.94016147153359 0,2.23410481428215 3.06105687442822 0,2.1729090847148 3.1865267138484 0,2.12061475842818 3.31595971334866 0,2.07747660812336 3.448725288366 0,2.04370479853239 3.58417661836448 0,2.01946386251686 3.72165379807987 0,2.00487189948035 3.86048705251175 0,2.0 4.0 0,2.00487189948035 4.13951294748825 0,2.01946386251686 4.27834620192013 0,2.04370479853239 4.41582338163552 0,2.07747660812336 4.551274711634 0,2.12061475842818 4.68404028665134 0,2.1729090847148 4.8134732861516 0,2.23410481428215 4.93894312557178 0,2.30390380768715 5.05983852846641 0,2.38196601125011 5.17557050458495 0,2.46791111376204 5.28557521937308 0,2.5613203993227 5.38931674091799 0,2.66173878728228 5.48628965095479 0,2.76867704934868 5.57602150721344 0,2.88161419305851 5.65807514511008 0,3.0 5.73205080756888 0,3.12325770642184 5.79758809259833 0,3.25078681316818 5.85436770913357 0,3.38196601125011 5.90211303259031 0,3.51615620880066 5.94059145255199 0,3.65270364466614 5.96961550602442 0,3.79094307346469 5.98904379073655 0,3.930201006595 5.99878165403819 0,4.069798993405 5.99878165403819 0,4.20905692653531 5.98904379073655 0,4.34729635533386 5.96961550602442 0,4.48384379119933 5.94059145255199 0,4.61803398874989 5.90211303259031 0,4.74921318683182 5.85436770913357 0,4.87674229357815 5.79758809259833 0,5.0 5.73205080756888 0,5.11838580694149 5.65807514511008 0,5.23132295065132 5.57602150721344 0,5.33826121271772 5.48628965095479 0,5.4386796006773 5.389316740918 0,5.53208888623796 5.28557521937308 0,5.61803398874989 5.17557050458495 0,5.69609619231285 5.05983852846641 0,5.76589518571785 4.93894312557178 0,5.8270909152852 4.8134732861516 0,5.87938524157182 4.68404028665134 0,5.92252339187664 4.551274711634 0,5.95629520146761 4.41582338163552 0,5.98053613748314 4.27834620192013 0,5.99512810051965 4.13951294748825 0,6.0 4.0 0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (6 4 0,5.99512810051965 3.86048705251175 0,5.98053613748314 3.72165379807987 0,5.95629520146761 3.58417661836448 0,5.92252339187664 3.448725288366 0,5.87938524157182 3.31595971334866 0,5.8270909152852 3.1865267138484 0,5.76589518571785 3.06105687442822 0,5.69609619231285 2.94016147153359 0,5.61803398874989 2.82442949541505 0,5.53208888623796 2.71442478062692 0,5.4386796006773 2.61068325908201 0,5.33826121271772 2.51371034904521 0,5.23132295065132 2.42397849278656 0,5.11838580694149 2.34192485488992 0,5.0 2.26794919243112 0,4.87674229357815 2.20241190740167 0,4.74921318683182 2.14563229086643 0,4.61803398874989 2.09788696740969 0,4.48384379119934 2.05940854744801 0,4.34729635533386 2.03038449397558 0,4.20905692653531 2.01095620926345 0,4.069798993405 2.00121834596181 0,3.930201006595 2.00121834596181 0,3.79094307346469 2.01095620926345 0,3.65270364466614 2.03038449397558 0,3.51615620880066 2.05940854744801 0,3.38196601125011 2.09788696740969 0,3.25078681316818 2.14563229086643 0,3.12325770642185 2.20241190740167 0,3.0 2.26794919243112 0,2.88161419305851 2.34192485488992 0,2.76867704934868 2.42397849278656 0,2.66173878728228 2.51371034904521 0,2.5613203993227 2.61068325908201 0,2.46791111376204 2.71442478062692 0,2.38196601125011 2.82442949541505 0,2.30390380768715 2.94016147153359 0,2.23410481428215 3.06105687442822 0,2.1729090847148 3.1865267138484 0,2.12061475842818 3.31595971334866 0,2.07747660812336 3.448725288366 0,2.04370479853239 3.58417661836448 0,2.01946386251686 3.72165379807987 0,2.00487189948035 3.86048705251175 0,2.0 4.0 0,2.00487189948035 4.13951294748825 0,2.01946386251686 4.27834620192013 0,2.04370479853239 4.41582338163552 0,2.07747660812336 4.551274711634 0,2.12061475842818 4.68404028665134 0,2.1729090847148 4.8134732861516 0,2.23410481428215 4.93894312557178 0,2.30390380768715 5.05983852846641 0,2.38196601125011 5.17557050458495 0,2.46791111376204 5.28557521937308 0,2.5613203993227 5.38931674091799 0,2.66173878728228 5.48628965095479 0,2.76867704934868 5.57602150721344 0,2.88161419305851 5.65807514511008 0,3.0 5.73205080756888 0,3.12325770642184 5.79758809259833 0,3.25078681316818 5.85436770913357 0,3.38196601125011 5.90211303259031 0,3.51615620880066 5.94059145255199 0,3.65270364466614 5.96961550602442 0,3.79094307346469 5.98904379073655 0,3.930201006595 5.99878165403819 0,4.069798993405 5.99878165403819 0,4.20905692653531 5.98904379073655 0,4.34729635533386 5.96961550602442 0,4.48384379119933 5.94059145255199 0,4.61803398874989 5.90211303259031 0,4.74921318683182 5.85436770913357 0,4.87674229357815 5.79758809259833 0,5.0 5.73205080756888 0,5.11838580694149 5.65807514511008 0,5.23132295065132 5.57602150721344 0,5.33826121271772 5.48628965095479 0,5.4386796006773 5.389316740918 0,5.53208888623796 5.28557521937308 0,5.61803398874989 5.17557050458495 0,5.69609619231285 5.05983852846641 0,5.76589518571785 4.93894312557178 0,5.8270909152852 4.8134732861516 0,5.87938524157182 4.68404028665134 0,5.92252339187664 4.551274711634 0,5.95629520146761 4.41582338163552 0,5.98053613748314 4.27834620192013 0,5.99512810051965 4.13951294748825 0,6.0 4.0 0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):5
#   EntityHandle (String) = 1BA
#   LINESTRING Z (2 4 0,4 4 0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (2 4 0,4 4 0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):6
#   EntityHandle (String) = 1BB
#   LINESTRING Z (4 4 0,6 4 0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (4 4 0,6 4 0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):7
#   EntityHandle (String) = 1BC
#   LINESTRING Z (4 3 0,4 4 0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (4 3 0,4 4 0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):8
#   EntityHandle (String) = 1BD
#   LINESTRING Z (4 4 0,4 5 0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (4 4 0,4 5 0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):9
#   EntityHandle (String) = 1BE
#   LINESTRING Z (2 4 0,2.00487189948035 4.06975647374412 0,2.01946386251686 4.13917310096007 0,2.04370479853239 4.20791169081776 0,2.07747660812336 4.275637355817 0,2.12061475842818 4.34202014332567 0,2.1729090847148 4.4067366430758 0,2.23410481428215 4.46947156278589 0,2.30390380768715 4.52991926423321 0,2.38196601125011 4.58778525229247 0,2.46791111376204 4.64278760968654 0,2.5613203993227 4.694658370459 0,2.66173878728228 4.74314482547739 0,2.76867704934868 4.78801075360672 0,2.88161419305851 4.82903757255504 0,3.0 4.86602540378444 0,3.12325770642185 4.89879404629917 0,3.25078681316818 4.92718385456679 0,3.38196601125011 4.95105651629515 0,3.51615620880067 4.970295726276 0,3.65270364466614 4.98480775301221 0,3.79094307346469 4.99452189536827 0,3.930201006595 4.9993908270191 0,4.069798993405 4.9993908270191 0,4.20905692653531 4.99452189536827 0,4.34729635533386 4.98480775301221 0,4.48384379119934 4.970295726276 0,4.61803398874989 4.95105651629515 0,4.74921318683182 4.92718385456679 0,4.87674229357816 4.89879404629917 0,5.0 4.86602540378444 0,5.11838580694149 4.82903757255504 0,5.23132295065132 4.78801075360672 0,5.33826121271772 4.74314482547739 0,5.4386796006773 4.694658370459 0,5.53208888623796 4.64278760968654 0,5.61803398874989 4.58778525229247 0,5.69609619231285 4.5299192642332 0,5.76589518571785 4.46947156278589 0,5.8270909152852 4.4067366430758 0,5.87938524157182 4.34202014332567 0,5.92252339187664 4.275637355817 0,5.95629520146761 4.20791169081776 0,5.98053613748314 4.13917310096006 0,5.99512810051965 4.06975647374412 0,6.0 4.0 0,5.99512810051965 3.93024352625587 0,5.98053613748314 3.86082689903993 0,5.95629520146761 3.79208830918224 0,5.92252339187664 3.724362644183 0,5.87938524157182 3.65797985667433 0,5.8270909152852 3.5932633569242 0,5.76589518571785 3.53052843721411 0,5.69609619231285 3.4700807357668 0,5.61803398874989 3.41221474770753 0,5.53208888623796 3.35721239031346 0,5.4386796006773 3.305341629541 0,5.33826121271772 3.25685517452261 0,5.23132295065132 3.21198924639328 0,5.11838580694149 3.17096242744496 0,5.0 3.13397459621556 0,4.87674229357815 3.10120595370083 0,4.74921318683182 3.07281614543321 0,4.61803398874989 3.04894348370485 0,4.48384379119934 3.029704273724 0,4.34729635533386 3.01519224698779 0,4.20905692653531 3.00547810463173 0,4.069798993405 3.0006091729809 0,3.930201006595 3.0006091729809 0,3.79094307346469 3.00547810463173 0,3.65270364466614 3.01519224698779 0,3.51615620880066 3.029704273724 0,3.38196601125011 3.04894348370485 0,3.25078681316818 3.07281614543321 0,3.12325770642185 3.10120595370083 0,3.0 3.13397459621556 0,2.88161419305851 3.17096242744496 0,2.76867704934868 3.21198924639328 0,2.66173878728228 3.25685517452261 0,2.5613203993227 3.305341629541 0,2.46791111376204 3.35721239031346 0,2.38196601125011 3.41221474770753 0,2.30390380768715 3.4700807357668 0,2.23410481428215 3.53052843721411 0,2.1729090847148 3.5932633569242 0,2.12061475842818 3.65797985667433 0,2.07747660812336 3.724362644183 0,2.04370479853239 3.79208830918224 0,2.01946386251686 3.86082689903993 0,2.00487189948035 3.93024352625587 0,2 4 0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (2 4 0,2.00487189948035 4.06975647374412 0,2.01946386251686 4.13917310096007 0,2.04370479853239 4.20791169081776 0,2.07747660812336 4.275637355817 0,2.12061475842818 4.34202014332567 0,2.1729090847148 4.4067366430758 0,2.23410481428215 4.46947156278589 0,2.30390380768715 4.52991926423321 0,2.38196601125011 4.58778525229247 0,2.46791111376204 4.64278760968654 0,2.5613203993227 4.694658370459 0,2.66173878728228 4.74314482547739 0,2.76867704934868 4.78801075360672 0,2.88161419305851 4.82903757255504 0,3.0 4.86602540378444 0,3.12325770642185 4.89879404629917 0,3.25078681316818 4.92718385456679 0,3.38196601125011 4.95105651629515 0,3.51615620880067 4.970295726276 0,3.65270364466614 4.98480775301221 0,3.79094307346469 4.99452189536827 0,3.930201006595 4.9993908270191 0,4.069798993405 4.9993908270191 0,4.20905692653531 4.99452189536827 0,4.34729635533386 4.98480775301221 0,4.48384379119934 4.970295726276 0,4.61803398874989 4.95105651629515 0,4.74921318683182 4.92718385456679 0,4.87674229357816 4.89879404629917 0,5.0 4.86602540378444 0,5.11838580694149 4.82903757255504 0,5.23132295065132 4.78801075360672 0,5.33826121271772 4.74314482547739 0,5.4386796006773 4.694658370459 0,5.53208888623796 4.64278760968654 0,5.61803398874989 4.58778525229247 0,5.69609619231285 4.5299192642332 0,5.76589518571785 4.46947156278589 0,5.8270909152852 4.4067366430758 0,5.87938524157182 4.34202014332567 0,5.92252339187664 4.275637355817 0,5.95629520146761 4.20791169081776 0,5.98053613748314 4.13917310096006 0,5.99512810051965 4.06975647374412 0,6.0 4.0 0,5.99512810051965 3.93024352625587 0,5.98053613748314 3.86082689903993 0,5.95629520146761 3.79208830918224 0,5.92252339187664 3.724362644183 0,5.87938524157182 3.65797985667433 0,5.8270909152852 3.5932633569242 0,5.76589518571785 3.53052843721411 0,5.69609619231285 3.4700807357668 0,5.61803398874989 3.41221474770753 0,5.53208888623796 3.35721239031346 0,5.4386796006773 3.305341629541 0,5.33826121271772 3.25685517452261 0,5.23132295065132 3.21198924639328 0,5.11838580694149 3.17096242744496 0,5.0 3.13397459621556 0,4.87674229357815 3.10120595370083 0,4.74921318683182 3.07281614543321 0,4.61803398874989 3.04894348370485 0,4.48384379119934 3.029704273724 0,4.34729635533386 3.01519224698779 0,4.20905692653531 3.00547810463173 0,4.069798993405 3.0006091729809 0,3.930201006595 3.0006091729809 0,3.79094307346469 3.00547810463173 0,3.65270364466614 3.01519224698779 0,3.51615620880066 3.029704273724 0,3.38196601125011 3.04894348370485 0,3.25078681316818 3.07281614543321 0,3.12325770642185 3.10120595370083 0,3.0 3.13397459621556 0,2.88161419305851 3.17096242744496 0,2.76867704934868 3.21198924639328 0,2.66173878728228 3.25685517452261 0,2.5613203993227 3.305341629541 0,2.46791111376204 3.35721239031346 0,2.38196601125011 3.41221474770753 0,2.30390380768715 3.4700807357668 0,2.23410481428215 3.53052843721411 0,2.1729090847148 3.5932633569242 0,2.12061475842818 3.65797985667433 0,2.07747660812336 3.724362644183 0,2.04370479853239 3.79208830918224 0,2.01946386251686 3.86082689903993 0,2.00487189948035 3.93024352625587 0,2 4 0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):10
#   EntityHandle (String) = 1BF
#   LINESTRING Z (2.0 2.0 0,1.96657794502105 2.03582232791524 0,1.93571660708646 2.07387296203834 0,1.90756413746468 2.11396923855471 0,1.88225568337755 2.15591867344963 0,1.85991273921989 2.19951988653655 0,1.84064256332004 2.24456356819194 0,1.8245376630414 2.29083348415575 0,1.81167535069652 2.33810751357387 0,1.80211737240583 2.38615871529951 0,1.79590961168258 2.43475641733454 0,1.79308186916688 2.48366732418105 0,1.79364771956639 2.53265663678705 0,1.79760444649032 2.58148917971011 0,1.80493305548955 2.62993053008785 0,1.81559836524041 2.67774814299566 0,1.8295491764342 2.72471246778926 0,1.84671851756181 2.7705980500731 0,1.86702396641357 2.81518461400453 0,1.89036804575079 2.85825811973811 0,1.91663869124976 2.89961179093366 0,1.94570978947168 2.93904710739563 0,1.97744178327594 2.97637475807832 0,2.01168234177068 3.01141554988232 0,2.04826709158413 3.04400126787917 0,2.08702040594658 3.07397548283483 0,2.12775624779472 3.10119430215541 0,2.17027906285109 3.12552706065018 0,2.2143847183914 3.14685694779575 0,2.25986148319297 3.16508156849045 0,2.30649104396024 3.18011343460661 0,2.35404955334774 3.1918803849814 0,2.40230870454951 3.20032593182975 0,2.45103682729644 3.2054095319166 0,2.5 3.20710678118655 0,2.54896317270356 3.2054095319166 0,2.59769129545049 3.20032593182975 0,2.64595044665226 3.1918803849814 0,2.69350895603976 3.18011343460661 0,2.74013851680703 3.16508156849045 0,2.7856152816086 3.14685694779575 0,2.8297209371489 3.12552706065018 0,2.87224375220528 3.10119430215541 0,2.91297959405342 3.07397548283483 0,2.95173290841587 3.04400126787917 0,2.98831765822932 3.01141554988232 0,3.02255821672406 2.97637475807832 0,3.05429021052832 2.93904710739563 0,3.08336130875024 2.89961179093367 0,3.10963195424921 2.85825811973811 0,3.13297603358643 2.81518461400453 0,3.15328148243819 2.7705980500731 0,3.1704508235658 2.72471246778926 0,3.18440163475959 2.67774814299567 0,3.19506694451045 2.62993053008786 0,3.20239555350968 2.58148917971011 0,3.20635228043361 2.53265663678705 0,3.20691813083312 2.48366732418105 0,3.20409038831742 2.43475641733454 0,3.19788262759417 2.38615871529951 0,3.18832464930348 2.33810751357387 0,3.1754623369586 2.29083348415575 0,3.15935743667996 2.24456356819194 0,3.14008726078011 2.19951988653655 0,3.11774431662245 2.15591867344963 0,3.09243586253532 2.11396923855472 0,3.06428339291354 2.07387296203834 0,3.03342205497895 2.03582232791524 0,3 2 0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (2.0 2.0 0,1.96657794502105 2.03582232791524 0,1.93571660708646 2.07387296203834 0,1.90756413746468 2.11396923855471 0,1.88225568337755 2.15591867344963 0,1.85991273921989 2.19951988653655 0,1.84064256332004 2.24456356819194 0,1.8245376630414 2.29083348415575 0,1.81167535069652 2.33810751357387 0,1.80211737240583 2.38615871529951 0,1.79590961168258 2.43475641733454 0,1.79308186916688 2.48366732418105 0,1.79364771956639 2.53265663678705 0,1.79760444649032 2.58148917971011 0,1.80493305548955 2.62993053008785 0,1.81559836524041 2.67774814299566 0,1.8295491764342 2.72471246778926 0,1.84671851756181 2.7705980500731 0,1.86702396641357 2.81518461400453 0,1.89036804575079 2.85825811973811 0,1.91663869124976 2.89961179093366 0,1.94570978947168 2.93904710739563 0,1.97744178327594 2.97637475807832 0,2.01168234177068 3.01141554988232 0,2.04826709158413 3.04400126787917 0,2.08702040594658 3.07397548283483 0,2.12775624779472 3.10119430215541 0,2.17027906285109 3.12552706065018 0,2.2143847183914 3.14685694779575 0,2.25986148319297 3.16508156849045 0,2.30649104396024 3.18011343460661 0,2.35404955334774 3.1918803849814 0,2.40230870454951 3.20032593182975 0,2.45103682729644 3.2054095319166 0,2.5 3.20710678118655 0,2.54896317270356 3.2054095319166 0,2.59769129545049 3.20032593182975 0,2.64595044665226 3.1918803849814 0,2.69350895603976 3.18011343460661 0,2.74013851680703 3.16508156849045 0,2.7856152816086 3.14685694779575 0,2.8297209371489 3.12552706065018 0,2.87224375220528 3.10119430215541 0,2.91297959405342 3.07397548283483 0,2.95173290841587 3.04400126787917 0,2.98831765822932 3.01141554988232 0,3.02255821672406 2.97637475807832 0,3.05429021052832 2.93904710739563 0,3.08336130875024 2.89961179093367 0,3.10963195424921 2.85825811973811 0,3.13297603358643 2.81518461400453 0,3.15328148243819 2.7705980500731 0,3.1704508235658 2.72471246778926 0,3.18440163475959 2.67774814299567 0,3.19506694451045 2.62993053008786 0,3.20239555350968 2.58148917971011 0,3.20635228043361 2.53265663678705 0,3.20691813083312 2.48366732418105 0,3.20409038831742 2.43475641733454 0,3.19788262759417 2.38615871529951 0,3.18832464930348 2.33810751357387 0,3.1754623369586 2.29083348415575 0,3.15935743667996 2.24456356819194 0,3.14008726078011 2.19951988653655 0,3.11774431662245 2.15591867344963 0,3.09243586253532 2.11396923855472 0,3.06428339291354 2.07387296203834 0,3.03342205497895 2.03582232791524 0,3 2 0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):11
#   EntityHandle (String) = 1C0
#   POLYGON Z ((1 2 0,1 3 0,2 3 0,2 2 0,1 2 0))
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON Z ((1 2 0,1 3 0,2 3 0,2 2 0,1 2 0))'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):12
#   EntityHandle (String) = 1C1
#   POLYGON ((3 4,4 4,4 3,3 3,3 4))
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON ((3 4,4 4,4 3,3 3,3 4))'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):13
#   EntityHandle (String) = 1C3
#   POLYGON ((8 8,9 8,9 9,8 9,8 8))
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON ((8 8,9 8,9 9,8 9,8 8))'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):14
#   EntityHandle (String) = 1C6
#   LINESTRING (2 2,2.15384615384615 2.15384615384615,2.30769230769231 2.30769230769231,2.46153846153846 2.46153846153846,2.61538461538461 2.61538461538461,2.76923076923077 2.76923076923077,2.92307692307692 2.92307692307692,3.07692307692308 3.07692307692308,3.23076923076923 3.23076923076923,3.38461538461538 3.38461538461538,3.53846153846154 3.53846153846154,3.69230769230769 3.69230769230769,3.84615384615385 3.84615384615385,4 4,4.15384615384615 4.15384615384615,4.30769230769231 4.30769230769231,4.46153846153846 4.46153846153846,4.61538461538462 4.61538461538462,4.76923076923077 4.76923076923077,4.92307692307692 4.92307692307692,5.07692307692308 5.07692307692308,5.23076923076923 5.23076923076923,5.38461538461538 5.38461538461538,5.53846153846154 5.53846153846154,5.69230769230769 5.69230769230769,5.84615384615385 5.84615384615385,6.0 6.0,6.15384615384615 6.15384615384615,6.30769230769231 6.30769230769231,6.46153846153846 6.46153846153846,6.61538461538462 6.61538461538462,6.76923076923077 6.76923076923077,6.92307692307692 6.92307692307692,7.07692307692308 7.07692307692308,7.23076923076923 7.23076923076923,7.38461538461539 7.38461538461539,7.53846153846154 7.53846153846154,7.69230769230769 7.69230769230769,7.84615384615385 7.84615384615385,8 8)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (2 2,2.15384615384615 2.15384615384615,2.30769230769231 2.30769230769231,2.46153846153846 2.46153846153846,2.61538461538461 2.61538461538461,2.76923076923077 2.76923076923077,2.92307692307692 2.92307692307692,3.07692307692308 3.07692307692308,3.23076923076923 3.23076923076923,3.38461538461538 3.38461538461538,3.53846153846154 3.53846153846154,3.69230769230769 3.69230769230769,3.84615384615385 3.84615384615385,4 4,4.15384615384615 4.15384615384615,4.30769230769231 4.30769230769231,4.46153846153846 4.46153846153846,4.61538461538462 4.61538461538462,4.76923076923077 4.76923076923077,4.92307692307692 4.92307692307692,5.07692307692308 5.07692307692308,5.23076923076923 5.23076923076923,5.38461538461538 5.38461538461538,5.53846153846154 5.53846153846154,5.69230769230769 5.69230769230769,5.84615384615385 5.84615384615385,6.0 6.0,6.15384615384615 6.15384615384615,6.30769230769231 6.30769230769231,6.46153846153846 6.46153846153846,6.61538461538462 6.61538461538462,6.76923076923077 6.76923076923077,6.92307692307692 6.92307692307692,7.07692307692308 7.07692307692308,7.23076923076923 7.23076923076923,7.38461538461539 7.38461538461539,7.53846153846154 7.53846153846154,7.69230769230769 7.69230769230769,7.84615384615385 7.84615384615385,8 8)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):15
#   EntityHandle (String) = 1C7
#   LINESTRING (8 1,7.62837370825536 0.987348067229724,7.25775889681215 0.975707614760869,6.88916704597178 0.966090122894857,6.52360963603567 0.959507071933107,6.16209814730525 0.956969942177043,5.80564406008193 0.959490213928084,5.45525885466714 0.968079367487651,5.11195401136229 0.983748883157167,4.77674101046882 1.00751024123805,4.45063133228814 1.04037492203173,4.13463645712167 1.08335440583961,3.82976786527082 1.13746017296313,3.53703703703704 1.2037037037037,3.25745545272173 1.28309647836275,2.99203459262631 1.37664997724169,2.74178593705221 1.48537568064195,2.50772096630085 1.61028506886495,2.29085116067365 1.75238962221211,2.09218800047203 1.91270082098484,1.91270082098485 2.09218800047202,1.75238962221211 2.29085116067364,1.61028506886495 2.50772096630085,1.48537568064195 2.74178593705221,1.37664997724169 2.99203459262631,1.28309647836275 3.25745545272172,1.2037037037037 3.53703703703703,1.13746017296313 3.82976786527082,1.08335440583961 4.13463645712166,1.04037492203173 4.45063133228814,1.00751024123805 4.77674101046882,0.983748883157167 5.11195401136229,0.968079367487652 5.45525885466714,0.959490213928084 5.80564406008193,0.956969942177043 6.16209814730525,0.959507071933108 6.52360963603567,0.966090122894857 6.88916704597178,0.975707614760869 7.25775889681216,0.987348067229724 7.62837370825537,1 8)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (8 1,7.62837370825536 0.987348067229724,7.25775889681215 0.975707614760869,6.88916704597178 0.966090122894857,6.52360963603567 0.959507071933107,6.16209814730525 0.956969942177043,5.80564406008193 0.959490213928084,5.45525885466714 0.968079367487651,5.11195401136229 0.983748883157167,4.77674101046882 1.00751024123805,4.45063133228814 1.04037492203173,4.13463645712167 1.08335440583961,3.82976786527082 1.13746017296313,3.53703703703704 1.2037037037037,3.25745545272173 1.28309647836275,2.99203459262631 1.37664997724169,2.74178593705221 1.48537568064195,2.50772096630085 1.61028506886495,2.29085116067365 1.75238962221211,2.09218800047203 1.91270082098484,1.91270082098485 2.09218800047202,1.75238962221211 2.29085116067364,1.61028506886495 2.50772096630085,1.48537568064195 2.74178593705221,1.37664997724169 2.99203459262631,1.28309647836275 3.25745545272172,1.2037037037037 3.53703703703703,1.13746017296313 3.82976786527082,1.08335440583961 4.13463645712166,1.04037492203173 4.45063133228814,1.00751024123805 4.77674101046882,0.983748883157167 5.11195401136229,0.968079367487652 5.45525885466714,0.959490213928084 5.80564406008193,0.956969942177043 6.16209814730525,0.959507071933108 6.52360963603567,0.966090122894857 6.88916704597178,0.975707614760869 7.25775889681216,0.987348067229724 7.62837370825537,1 8)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):16
#   EntityHandle (String) = 1C8
#   POINT Z (7 7 0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POINT Z (7 7 0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):17
#   EntityHandle (String) = 1C9
#   POINT Z (-2.0 4.0 -3.46410161513775)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POINT Z (-2.0 4.0 -3.46410161513775)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):18
#   EntityHandle (String) = 1CA
#   LINESTRING Z (0 0 0,-0.5 1.0 -0.866025403784439)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (0 0 0,-0.5 1.0 -0.866025403784439)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):19
#   EntityHandle (String) = 1CB
#   LINESTRING (-0.5 1.0,-1.0 1.0,-0.5 2.0,-0.5 1.0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (-0.5 1.0,-1.0 1.0,-0.5 2.0,-0.5 1.0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):20
#   EntityHandle (String) = 1CC
#   LINESTRING Z (-0.5 1.0 -0.866025403784439,-0.5 2.0 -0.866025403784439,-1.0 2.0 -1.73205080756888,-0.5 1.0 -0.866025403784439)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (-0.5 1.0 -0.866025403784439,-0.5 2.0 -0.866025403784439,-1.0 2.0 -1.73205080756888,-0.5 1.0 -0.866025403784439)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):21
#   EntityHandle (String) = 1D1
#   LINESTRING Z (-2.0 6.0 -3.46410161513776,-2.06975647374412 5.99512810051965 -3.58492337181942,-2.13917310096006 5.98053613748314 -3.7051564970475,-2.20791169081776 5.95629520146761 -3.82421522712167,-2.275637355817 5.92252339187664 -3.94151951987674,-2.34202014332567 5.87938524157182 -4.0564978805898,-2.4067366430758 5.8270909152852 -4.16859014624505,-2.46947156278589 5.76589518571785 -4.27725021459168,-2.5299192642332 5.69609619231285 -4.38194870469918,-2.58778525229247 5.61803398874989 -4.48217553604801,-2.64278760968654 5.53208888623796 -4.57744241359059,-2.694658370459 5.4386796006773 -4.66728520667574,-2.74314482547739 5.33826121271772 -4.75126621024651,-2.78801075360672 5.23132295065132 -4.82897627729524,-2.82903757255504 5.11838580694149 -4.90003681218666,-2.86602540378444 5.0 -4.96410161513776,-2.89879404629917 4.87674229357815 -5.02085856886833,-2.92718385456679 4.74921318683182 -5.07003115920498,-2.95105651629515 4.61803398874989 -5.11137982223042,-2.970295726276 4.48384379119934 -5.14470311141473,-2.98480775301221 4.34729635533386 -5.16983867904264,-2.99452189536827 4.20905692653531 -5.1866640671553,-2.99939082701909 4.069798993405 -5.19509730415311,-2.99939082701909 3.930201006595 -5.19509730415311,-2.99452189536827 3.79094307346469 -5.1866640671553,-2.98480775301221 3.65270364466614 -5.16983867904264,-2.970295726276 3.51615620880066 -5.14470311141473,-2.95105651629515 3.38196601125011 -5.11137982223042,-2.92718385456679 3.25078681316818 -5.07003115920498,-2.89879404629917 3.12325770642185 -5.02085856886833,-2.86602540378444 3.0 -4.96410161513776,-2.82903757255504 2.88161419305851 -4.90003681218666,-2.78801075360672 2.76867704934868 -4.82897627729524,-2.74314482547739 2.66173878728228 -4.75126621024651,-2.694658370459 2.5613203993227 -4.66728520667574,-2.64278760968654 2.46791111376204 -4.5774424135906,-2.58778525229247 2.38196601125011 -4.48217553604801,-2.5299192642332 2.30390380768715 -4.38194870469918,-2.46947156278589 2.23410481428215 -4.27725021459168,-2.4067366430758 2.1729090847148 -4.16859014624505,-2.34202014332567 2.12061475842818 -4.0564978805898,-2.275637355817 2.07747660812336 -3.94151951987674,-2.20791169081776 2.04370479853239 -3.82421522712167,-2.13917310096006 2.01946386251686 -3.7051564970475,-2.06975647374412 2.00487189948035 -3.58492337181943,-2.0 2.0 -3.46410161513776,-1.93024352625587 2.00487189948035 -3.34327985845609,-1.86082689903993 2.01946386251686 -3.22304673322801,-1.79208830918224 2.04370479853239 -3.10398800315384,-1.724362644183 2.07747660812336 -2.98668371039877,-1.65797985667433 2.12061475842818 -2.87170534968571,-1.5932633569242 2.1729090847148 -2.75961308403046,-1.53052843721411 2.23410481428215 -2.65095301568383,
# -1.47008073576679 2.30390380768715 -2.54625452557633,-1.41221474770753 2.38196601125011 -2.4460276942275,-1.35721239031346 2.46791111376204 -2.35076081668492,-1.305341629541 2.5613203993227 -2.26091802359977,-1.25685517452261 2.66173878728228 -2.176937020029,-1.21198924639328 2.76867704934868 -2.09922695298027,-1.17096242744496 2.88161419305851 -2.02816641808885,-1.13397459621556 3.0 -1.96410161513776,-1.10120595370083 3.12325770642184 -1.90734466140718,-1.07281614543321 3.25078681316818 -1.85817207107053,-1.04894348370485 3.38196601125011 -1.81682340804509,-1.029704273724 3.51615620880066 -1.78350011886079,-1.01519224698779 3.65270364466614 -1.75836455123287,-1.00547810463173 3.79094307346469 -1.74153916312021,-1.0006091729809 3.930201006595 -1.7331059261224,-1.0006091729809 4.069798993405 -1.7331059261224,-1.00547810463173 4.20905692653531 -1.74153916312021,-1.01519224698779 4.34729635533386 -1.75836455123287,-1.029704273724 4.48384379119933 -1.78350011886078,-1.04894348370485 4.61803398874989 -1.81682340804509,-1.07281614543321 4.74921318683182 -1.85817207107053,-1.10120595370083 4.87674229357815 -1.90734466140718,-1.13397459621556 5.0 -1.96410161513776,-1.17096242744496 5.11838580694149 -2.02816641808885,-1.21198924639328 5.23132295065132 -2.09922695298027,-1.25685517452261 5.33826121271772 -2.176937020029,-1.305341629541 5.4386796006773 -2.26091802359977,-1.35721239031346 5.53208888623796 -2.35076081668492,-1.41221474770753 5.61803398874989 -2.4460276942275,-1.47008073576679 5.69609619231285 -2.54625452557633,-1.53052843721411 5.76589518571785 -2.65095301568383,-1.5932633569242 5.8270909152852 -2.75961308403046,-1.65797985667433 5.87938524157182 -2.87170534968571,-1.724362644183 5.92252339187664 -2.98668371039877,-1.79208830918224 5.95629520146761 -3.10398800315384,-1.86082689903993 5.98053613748314 -3.22304673322801,-1.93024352625587 5.99512810051965 -3.34327985845609,-2.0 6.0 -3.46410161513775)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (-2.0 6.0 -3.46410161513776,-2.06975647374412 5.99512810051965 -3.58492337181942,-2.13917310096006 5.98053613748314 -3.7051564970475,-2.20791169081776 5.95629520146761 -3.82421522712167,-2.275637355817 5.92252339187664 -3.94151951987674,-2.34202014332567 5.87938524157182 -4.0564978805898,-2.4067366430758 5.8270909152852 -4.16859014624505,-2.46947156278589 5.76589518571785 -4.27725021459168,-2.5299192642332 5.69609619231285 -4.38194870469918,-2.58778525229247 5.61803398874989 -4.48217553604801,-2.64278760968654 5.53208888623796 -4.57744241359059,-2.694658370459 5.4386796006773 -4.66728520667574,-2.74314482547739 5.33826121271772 -4.75126621024651,-2.78801075360672 5.23132295065132 -4.82897627729524,-2.82903757255504 5.11838580694149 -4.90003681218666,-2.86602540378444 5.0 -4.96410161513776,-2.89879404629917 4.87674229357815 -5.02085856886833,-2.92718385456679 4.74921318683182 -5.07003115920498,-2.95105651629515 4.61803398874989 -5.11137982223042,-2.970295726276 4.48384379119934 -5.14470311141473,-2.98480775301221 4.34729635533386 -5.16983867904264,-2.99452189536827 4.20905692653531 -5.1866640671553,-2.99939082701909 4.069798993405 -5.19509730415311,-2.99939082701909 3.930201006595 -5.19509730415311,-2.99452189536827 3.79094307346469 -5.1866640671553,-2.98480775301221 3.65270364466614 -5.16983867904264,-2.970295726276 3.51615620880066 -5.14470311141473,-2.95105651629515 3.38196601125011 -5.11137982223042,-2.92718385456679 3.25078681316818 -5.07003115920498,-2.89879404629917 3.12325770642185 -5.02085856886833,-2.86602540378444 3.0 -4.96410161513776,-2.82903757255504 2.88161419305851 -4.90003681218666,-2.78801075360672 2.76867704934868 -4.82897627729524,-2.74314482547739 2.66173878728228 -4.75126621024651,-2.694658370459 2.5613203993227 -4.66728520667574,-2.64278760968654 2.46791111376204 -4.5774424135906,-2.58778525229247 2.38196601125011 -4.48217553604801,-2.5299192642332 2.30390380768715 -4.38194870469918,-2.46947156278589 2.23410481428215 -4.27725021459168,-2.4067366430758 2.1729090847148 -4.16859014624505,-2.34202014332567 2.12061475842818 -4.0564978805898,-2.275637355817 2.07747660812336 -3.94151951987674,-2.20791169081776 2.04370479853239 -3.82421522712167,-2.13917310096006 2.01946386251686 -3.7051564970475,-2.06975647374412 2.00487189948035 -3.58492337181943,-2.0 2.0 -3.46410161513776,-1.93024352625587 2.00487189948035 -3.34327985845609,-1.86082689903993 2.01946386251686 -3.22304673322801,-1.79208830918224 2.04370479853239 -3.10398800315384,-1.724362644183 2.07747660812336 -2.98668371039877,' + \
        '-1.65797985667433 2.12061475842818 -2.87170534968571,-1.5932633569242 2.1729090847148 -2.75961308403046,-1.53052843721411 2.23410481428215 -2.65095301568383,-1.47008073576679 2.30390380768715 -2.54625452557633,-1.41221474770753 2.38196601125011 -2.4460276942275,-1.35721239031346 2.46791111376204 -2.35076081668492,-1.305341629541 2.5613203993227 -2.26091802359977,-1.25685517452261 2.66173878728228 -2.176937020029,-1.21198924639328 2.76867704934868 -2.09922695298027,-1.17096242744496 2.88161419305851 -2.02816641808885,-1.13397459621556 3.0 -1.96410161513776,-1.10120595370083 3.12325770642184 -1.90734466140718,-1.07281614543321 3.25078681316818 -1.85817207107053,-1.04894348370485 3.38196601125011 -1.81682340804509,-1.029704273724 3.51615620880066 -1.78350011886079,-1.01519224698779 3.65270364466614 -1.75836455123287,-1.00547810463173 3.79094307346469 -1.74153916312021,-1.0006091729809 3.930201006595 -1.7331059261224,-1.0006091729809 4.069798993405 -1.7331059261224,-1.00547810463173 4.20905692653531 -1.74153916312021,-1.01519224698779 4.34729635533386 -1.75836455123287,-1.029704273724 4.48384379119933 -1.78350011886078,-1.04894348370485 4.61803398874989 -1.81682340804509,-1.07281614543321 4.74921318683182 -1.85817207107053,-1.10120595370083 4.87674229357815 -1.90734466140718,-1.13397459621556 5.0 -1.96410161513776,-1.17096242744496 5.11838580694149 -2.02816641808885,-1.21198924639328 5.23132295065132 -2.09922695298027,-1.25685517452261 5.33826121271772 -2.176937020029,-1.305341629541 5.4386796006773 -2.26091802359977,-1.35721239031346 5.53208888623796 -2.35076081668492,-1.41221474770753 5.61803398874989 -2.4460276942275,-1.47008073576679 5.69609619231285 -2.54625452557633,-1.53052843721411 5.76589518571785 -2.65095301568383,-1.5932633569242 5.8270909152852 -2.75961308403046,-1.65797985667433 5.87938524157182 -2.87170534968571,-1.724362644183 5.92252339187664 -2.98668371039877,-1.79208830918224 5.95629520146761 -3.10398800315384,-1.86082689903993 5.98053613748314 -3.22304673322801,-1.93024352625587 5.99512810051965 -3.34327985845609,-2.0 6.0 -3.46410161513775)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):22
#   EntityHandle (String) = 1D2
#   LINESTRING Z (-1.0 4.0 -1.73205080756888,-2.0 4.0 -3.46410161513775)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (-1.0 4.0 -1.73205080756888,-2.0 4.0 -3.46410161513775)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):23
#   EntityHandle (String) = 1D3
#   LINESTRING Z (-2.0 4.0 -3.46410161513775,-3.0 4.0 -5.19615242270663)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (-2.0 4.0 -3.46410161513775,-3.0 4.0 -5.19615242270663)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):24
#   EntityHandle (String) = 1D4
#   LINESTRING Z (-2.0 3.0 -3.46410161513775,-2.0 4.0 -3.46410161513775)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (-2.0 3.0 -3.46410161513775,-2.0 4.0 -3.46410161513775)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):25
#   EntityHandle (String) = 1D5
#   LINESTRING Z (-2.0 4.0 -3.46410161513775,-2.0 5.0 -3.46410161513775)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (-2.0 4.0 -3.46410161513775,-2.0 5.0 -3.46410161513775)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):26
#   EntityHandle (String) = 1D6
#   LINESTRING Z (-1.0 4.0 -1.73205080756888,-1.00243594974018 4.06975647374412 -1.73626999628355,-1.00973193125843 4.13917310096007 -1.74890700696425,-1.02185239926619 4.20791169081776 -1.76990027336521,-1.03873830406168 4.275637355817 -1.79914751840276,-1.06030737921409 4.34202014332567 -1.83650625243901,-1.0864545423574 4.4067366430758 -1.88179446747701,-1.11705240714107 4.46947156278589 -1.93479152388545,-1.15195190384357 4.52991926423321 -1.99523922533277,-1.19098300562505 4.58778525229247 -2.06284307669368,-1.23395555688102 4.64278760968654 -2.13727371879988,-1.28066019966135 4.694658370459 -2.21816853304476,-1.33086939364114 4.74314482547739 -2.30513340802484,-1.38433852467434 4.78801075360672 -2.3977446596109,-1.44080709652925 4.82903757255504 -2.49555109509446,-1.5 4.86602540378444 -2.59807621135332,-1.56162885321092 4.89879404629917 -2.70482051632684,-1.62539340658409 4.92718385456679 -2.8152639624911,-1.69098300562505 4.95105651629515 -2.92886848047812,-1.75807810440033 4.970295726276 -3.04508060049576,-1.82635182233307 4.98480775301221 -3.16333414877688,-1.89547153673235 4.99452189536827 -3.28305300592108,-1.9651005032975 4.99939082701909 -3.40365391369044,-2.0348994967025 4.99939082701909 -3.52454931658507,-2.10452846326765 4.99452189536827 -3.64515022435443,-2.17364817766693 4.98480775301221 -3.76486908149863,-2.24192189559967 4.970295726276 -3.88312262977975,-2.30901699437495 4.95105651629515 -3.99933474979739,-2.37460659341591 4.92718385456679 -4.11293926778441,-2.43837114678908 4.89879404629917 -4.22338271394867,-2.5 4.86602540378444 -4.33012701892219,-2.55919290347075 4.82903757255504 -4.43265213518105,-2.61566147532566 4.78801075360672 -4.53045857066461,-2.66913060635886 4.74314482547739 -4.62306982225067,-2.71933980033865 4.694658370459 -4.71003469723075,-2.76604444311898 4.64278760968654 -4.79092951147563,-2.80901699437495 4.58778525229247 -4.86536015358183,-2.84804809615642 4.5299192642332 -4.93296400494274,-2.88294759285893 4.46947156278589 -4.99341170639005,-2.9135454576426 4.4067366430758 -5.0464087627985,-2.93969262078591 4.34202014332567 -5.0916969778365,-2.96126169593832 4.275637355817 -5.12905571187275,-2.9781476007338 4.20791169081776 -5.1583029569103,-2.99026806874157 4.13917310096007 -5.17929622331126,-2.99756405025982 4.06975647374412 -5.19193323399196,-3.0 4.0 -5.19615242270663,-2.99756405025982 3.93024352625587 -5.19193323399196,-2.99026806874157 3.86082689903993 -5.17929622331126,-2.9781476007338 3.79208830918224 -5.1583029569103,-2.96126169593832 3.724362644183 -5.12905571187275,
#-2.93969262078591 3.65797985667433 -5.0916969778365,-2.9135454576426 3.5932633569242 -5.0464087627985,-2.88294759285893 3.53052843721411 -4.99341170639005,-2.84804809615642 3.4700807357668 -4.93296400494274,-2.80901699437495 3.41221474770753 -4.86536015358183,-2.76604444311898 3.35721239031346 -4.79092951147563,-2.71933980033865 3.305341629541 -4.71003469723075,-2.66913060635886 3.25685517452261 -4.62306982225067,-2.61566147532566 3.21198924639328 -4.53045857066461,-2.55919290347075 3.17096242744496 -4.43265213518105,-2.5 3.13397459621556 -4.33012701892219,-2.43837114678908 3.10120595370083 -4.22338271394867,-2.37460659341591 3.07281614543321 -4.11293926778441,-2.30901699437495 3.04894348370485 -3.99933474979739,-2.24192189559967 3.029704273724 -3.88312262977975,-2.17364817766693 3.01519224698779 -3.76486908149863,-2.10452846326765 3.00547810463173 -3.64515022435443,-2.0348994967025 3.0006091729809 -3.52454931658507,-1.9651005032975 3.0006091729809 -3.40365391369044,-1.89547153673235 3.00547810463173 -3.28305300592108,-1.82635182233307 3.01519224698779 -3.16333414877688,-1.75807810440033 3.029704273724 -3.04508060049576,-1.69098300562505 3.04894348370485 -2.92886848047812,-1.62539340658409 3.07281614543321 -2.8152639624911,-1.56162885321092 3.10120595370083 -2.70482051632684,-1.5 3.13397459621556 -2.59807621135332,-1.44080709652925 3.17096242744496 -2.49555109509446,-1.38433852467434 3.21198924639328 -2.3977446596109,-1.33086939364114 3.25685517452261 -2.30513340802484,-1.28066019966135 3.305341629541 -2.21816853304476,-1.23395555688102 3.35721239031346 -2.13727371879988,-1.19098300562505 3.41221474770753 -2.06284307669368,-1.15195190384357 3.4700807357668 -1.99523922533277,-1.11705240714107 3.53052843721411 -1.93479152388545,-1.0864545423574 3.5932633569242 -1.88179446747701,-1.06030737921409 3.65797985667433 -1.83650625243901,-1.03873830406168 3.724362644183 -1.79914751840276,-1.02185239926619 3.79208830918224 -1.76990027336521,-1.00973193125843 3.86082689903993 -1.74890700696425,-1.00243594974018 3.93024352625587 -1.73626999628355,-1.0 4.0 -1.73205080756888)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (-1.0 4.0 -1.73205080756888,-1.00243594974018 4.06975647374412 -1.73626999628355,-1.00973193125843 4.13917310096007 -1.74890700696425,-1.02185239926619 4.20791169081776 -1.76990027336521,-1.03873830406168 4.275637355817 -1.79914751840276,-1.06030737921409 4.34202014332567 -1.83650625243901,-1.0864545423574 4.4067366430758 -1.88179446747701,-1.11705240714107 4.46947156278589 -1.93479152388545,-1.15195190384357 4.52991926423321 -1.99523922533277,-1.19098300562505 4.58778525229247 -2.06284307669368,-1.23395555688102 4.64278760968654 -2.13727371879988,-1.28066019966135 4.694658370459 -2.21816853304476,-1.33086939364114 4.74314482547739 -2.30513340802484,-1.38433852467434 4.78801075360672 -2.3977446596109,-1.44080709652925 4.82903757255504 -2.49555109509446,-1.5 4.86602540378444 -2.59807621135332,-1.56162885321092 4.89879404629917 -2.70482051632684,-1.62539340658409 4.92718385456679 -2.8152639624911,-1.69098300562505 4.95105651629515 -2.92886848047812,-1.75807810440033 4.970295726276 -3.04508060049576,-1.82635182233307 4.98480775301221 -3.16333414877688,-1.89547153673235 4.99452189536827 -3.28305300592108,-1.9651005032975 4.99939082701909 -3.40365391369044,-2.0348994967025 4.99939082701909 -3.52454931658507,-2.10452846326765 4.99452189536827 -3.64515022435443,-2.17364817766693 4.98480775301221 -3.76486908149863,-2.24192189559967 4.970295726276 -3.88312262977975,-2.30901699437495 4.95105651629515 -3.99933474979739,-2.37460659341591 4.92718385456679 -4.11293926778441,-2.43837114678908 4.89879404629917 -4.22338271394867,-2.5 4.86602540378444 -4.33012701892219,-2.55919290347075 4.82903757255504 -4.43265213518105,-2.61566147532566 4.78801075360672 -4.53045857066461,-2.66913060635886 4.74314482547739 -4.62306982225067,-2.71933980033865 4.694658370459 -4.71003469723075,-2.76604444311898 4.64278760968654 -4.79092951147563,-2.80901699437495 4.58778525229247 -4.86536015358183,-2.84804809615642 4.5299192642332 -4.93296400494274,-2.88294759285893 4.46947156278589 -4.99341170639005,-2.9135454576426 4.4067366430758 -5.0464087627985,-2.93969262078591 4.34202014332567 -5.0916969778365,-2.96126169593832 4.275637355817 -5.12905571187275,-2.9781476007338 4.20791169081776 -5.1583029569103,-2.99026806874157 4.13917310096007 -5.17929622331126,-2.99756405025982 4.06975647374412 -5.19193323399196,-3.0 4.0 -5.19615242270663,-2.99756405025982 3.93024352625587 -5.19193323399196,-2.99026806874157 3.86082689903993 -5.17929622331126,-2.9781476007338 3.79208830918224 -5.1583029569103,-2.96126169593832 3.724362644183 -5.12905571187275,' + \
        '-2.93969262078591 3.65797985667433 -5.0916969778365,-2.9135454576426 3.5932633569242 -5.0464087627985,-2.88294759285893 3.53052843721411 -4.99341170639005,-2.84804809615642 3.4700807357668 -4.93296400494274,-2.80901699437495 3.41221474770753 -4.86536015358183,-2.76604444311898 3.35721239031346 -4.79092951147563,-2.71933980033865 3.305341629541 -4.71003469723075,-2.66913060635886 3.25685517452261 -4.62306982225067,-2.61566147532566 3.21198924639328 -4.53045857066461,-2.55919290347075 3.17096242744496 -4.43265213518105,-2.5 3.13397459621556 -4.33012701892219,-2.43837114678908 3.10120595370083 -4.22338271394867,-2.37460659341591 3.07281614543321 -4.11293926778441,-2.30901699437495 3.04894348370485 -3.99933474979739,-2.24192189559967 3.029704273724 -3.88312262977975,-2.17364817766693 3.01519224698779 -3.76486908149863,-2.10452846326765 3.00547810463173 -3.64515022435443,-2.0348994967025 3.0006091729809 -3.52454931658507,-1.9651005032975 3.0006091729809 -3.40365391369044,-1.89547153673235 3.00547810463173 -3.28305300592108,-1.82635182233307 3.01519224698779 -3.16333414877688,-1.75807810440033 3.029704273724 -3.04508060049576,-1.69098300562505 3.04894348370485 -2.92886848047812,-1.62539340658409 3.07281614543321 -2.8152639624911,-1.56162885321092 3.10120595370083 -2.70482051632684,-1.5 3.13397459621556 -2.59807621135332,-1.44080709652925 3.17096242744496 -2.49555109509446,-1.38433852467434 3.21198924639328 -2.3977446596109,-1.33086939364114 3.25685517452261 -2.30513340802484,-1.28066019966135 3.305341629541 -2.21816853304476,-1.23395555688102 3.35721239031346 -2.13727371879988,-1.19098300562505 3.41221474770753 -2.06284307669368,-1.15195190384357 3.4700807357668 -1.99523922533277,-1.11705240714107 3.53052843721411 -1.93479152388545,-1.0864545423574 3.5932633569242 -1.88179446747701,-1.06030737921409 3.65797985667433 -1.83650625243901,-1.03873830406168 3.724362644183 -1.79914751840276,-1.02185239926619 3.79208830918224 -1.76990027336521,-1.00973193125843 3.86082689903993 -1.74890700696425,-1.00243594974018 3.93024352625587 -1.73626999628355,-1.0 4.0 -1.73205080756888)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):27
#   EntityHandle (String) = 1D7
#   LINESTRING Z (-1.0 2.0 -1.73205080756888,-0.983288972510522 2.03582232791524 -1.70310645891042,-0.967858303543227 2.07387296203834 -1.67637975626429,-0.953782068732337 2.11396923855472 -1.65199900239256,-0.941127841688774 2.15591867344963 -1.6300812382226,-0.929956369609942 2.19951988653655 -1.61073168098672,-0.92032128166002 2.24456356819194 -1.59404321912206,-0.912268831520699 2.29083348415575 -1.58009596635534,-0.905837675348257 2.33810751357387 -1.56895687711326,-0.901058686202915 2.38615871529951 -1.56067942510471,-0.89795480584129 2.43475641733454 -1.55530334661776,-0.896540934583439 2.48366732418105 -1.5528544497638,-0.896823859783196 2.53265663678705 -1.55334449058452,-0.898802223245158 2.58148917971011 -1.55677111661648,-0.902466527744776 2.62993053008786 -1.56311787818422,-0.907799182620207 2.67774814299567 -1.5723543073677,-0.9147745882171 2.72471246778926 -1.58443606426492,-0.923359258780906 2.7705980500731 -1.59930514984767,-0.933511983206783 2.81518461400453 -1.61689018438853,-0.945184022875394 2.85825811973811 -1.63710675012253,-0.958319345624882 2.89961179093367 -1.65985779649846,-0.972854894735838 2.93904710739563 -1.68503410607454,-0.988720891637972 2.97637475807832 -1.71251481882177,-1.00584117088534 3.01141554988232 -1.74216801231798,-1.02413354579207 3.04400126787917 -1.77385133504753,-1.04351020297329 3.07397548283483 -1.80741268976625,-1.06387812389736 3.10119430215541 -1.84269096365129,-1.08513953142555 3.12552706065018 -1.87951680173053,-1.1071923591957 3.14685694779575 -1.917713419879,-1.12993074159648 3.16508156849045 -1.95709745347909,-1.15324552198012 3.18011343460661 -1.99747983767086,-1.17702477667387 3.1918803849814 -2.03866671496655,-1.20115435227475 3.20032593182975 -2.08046036587236,-1.22551841364822 3.2054095319166 -2.12266015804993,-1.25 3.20710678118655 -2.1650635094611,-1.27448158635178 3.2054095319166 -2.20746686087227,-1.29884564772525 3.20032593182975 -2.24966665304983,-1.32297522332613 3.1918803849814 -2.29146030395564,-1.34675447801988 3.18011343460661 -2.33264718125133,-1.37006925840352 3.16508156849045 -2.3730295654431,-1.3928076408043 3.14685694779575 -2.41241359904319,-1.41486046857445 3.12552706065018 -2.45061021719166,-1.43612187610264 3.10119430215541 -2.48743605527091,-1.45648979702671 3.07397548283483 -2.52271432915594,-1.47586645420793 3.04400126787917 -2.55627568387467,-1.49415882911466 3.01141554988232 -2.58795900660421,-1.51127910836203 2.97637475807832 -2.61761220010042,-1.52714510526416 2.93904710739563 -2.64509291284765,-1.54168065437512 2.89961179093367 -2.67026922242374,-1.55481597712461 2.85825811973811 -2.69302026879967,-1.56648801679322 2.81518461400453 -2.71323683453366,-1.57664074121909 2.7705980500731 -2.73082186907453,-1.5852254117829 2.72471246778926 -2.74569095465728,-1.59220081737979 2.67774814299567 -2.7577727115545,-1.59753347225522 2.62993053008785 -2.76700914073797,-1.60119777675484 2.58148917971011 -2.77335590230571,-1.6031761402168 2.53265663678705 -2.77678252833767,-1.60345906541656 2.48366732418105 -2.77727256915839,-1.60204519415871 2.43475641733454 -2.77482367230443,-1.59894131379708 2.38615871529951 -2.76944759381748,-1.59416232465174 2.33810751357387 -2.76117014180893,-1.5877311684793 2.29083348415575 -2.75003105256685,-1.57967871833998 2.24456356819194 -2.73608379980013,-1.57004363039006 2.19951988653655 -2.71939533793547,-1.55887215831123 2.15591867344963 -2.7000457806996,-1.54621793126766 2.11396923855472 -2.67812801652963,-1.53214169645677 2.07387296203834 -2.6537472626579,-1.51671102748948 2.03582232791524 -2.62702056001177,-1.5 2.0 -2.59807621135332)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (-1.0 2.0 -1.73205080756888,-0.983288972510522 2.03582232791524 -1.70310645891042,-0.967858303543227 2.07387296203834 -1.67637975626429,-0.953782068732337 2.11396923855472 -1.65199900239256,-0.941127841688774 2.15591867344963 -1.6300812382226,-0.929956369609942 2.19951988653655 -1.61073168098672,-0.92032128166002 2.24456356819194 -1.59404321912206,-0.912268831520699 2.29083348415575 -1.58009596635534,-0.905837675348257 2.33810751357387 -1.56895687711326,-0.901058686202915 2.38615871529951 -1.56067942510471,-0.89795480584129 2.43475641733454 -1.55530334661776,-0.896540934583439 2.48366732418105 -1.5528544497638,-0.896823859783196 2.53265663678705 -1.55334449058452,-0.898802223245158 2.58148917971011 -1.55677111661648,-0.902466527744776 2.62993053008786 -1.56311787818422,-0.907799182620207 2.67774814299567 -1.5723543073677,-0.9147745882171 2.72471246778926 -1.58443606426492,-0.923359258780906 2.7705980500731 -1.59930514984767,-0.933511983206783 2.81518461400453 -1.61689018438853,-0.945184022875394 2.85825811973811 -1.63710675012253,-0.958319345624882 2.89961179093367 -1.65985779649846,-0.972854894735838 2.93904710739563 -1.68503410607454,-0.988720891637972 2.97637475807832 -1.71251481882177,-1.00584117088534 3.01141554988232 -1.74216801231798,-1.02413354579207 3.04400126787917 -1.77385133504753,-1.04351020297329 3.07397548283483 -1.80741268976625,-1.06387812389736 3.10119430215541 -1.84269096365129,-1.08513953142555 3.12552706065018 -1.87951680173053,-1.1071923591957 3.14685694779575 -1.917713419879,-1.12993074159648 3.16508156849045 -1.95709745347909,-1.15324552198012 3.18011343460661 -1.99747983767086,-1.17702477667387 3.1918803849814 -2.03866671496655,-1.20115435227475 3.20032593182975 -2.08046036587236,-1.22551841364822 3.2054095319166 -2.12266015804993,-1.25 3.20710678118655 -2.1650635094611,-1.27448158635178 3.2054095319166 -2.20746686087227,-1.29884564772525 3.20032593182975 -2.24966665304983,-1.32297522332613 3.1918803849814 -2.29146030395564,-1.34675447801988 3.18011343460661 -2.33264718125133,-1.37006925840352 3.16508156849045 -2.3730295654431,-1.3928076408043 3.14685694779575 -2.41241359904319,-1.41486046857445 3.12552706065018 -2.45061021719166,-1.43612187610264 3.10119430215541 -2.48743605527091,-1.45648979702671 3.07397548283483 -2.52271432915594,-1.47586645420793 3.04400126787917 -2.55627568387467,-1.49415882911466 3.01141554988232 -2.58795900660421,-1.51127910836203 2.97637475807832 -2.61761220010042,-1.52714510526416 2.93904710739563 -2.64509291284765,-1.54168065437512 2.89961179093367 -2.67026922242374,-1.55481597712461 2.85825811973811 -2.69302026879967,-1.56648801679322 2.81518461400453 -2.71323683453366,-1.57664074121909 2.7705980500731 -2.73082186907453,-1.5852254117829 2.72471246778926 -2.74569095465728,-1.59220081737979 2.67774814299567 -2.7577727115545,-1.59753347225522 2.62993053008785 -2.76700914073797,-1.60119777675484 2.58148917971011 -2.77335590230571,-1.6031761402168 2.53265663678705 -2.77678252833767,-1.60345906541656 2.48366732418105 -2.77727256915839,-1.60204519415871 2.43475641733454 -2.77482367230443,-1.59894131379708 2.38615871529951 -2.76944759381748,-1.59416232465174 2.33810751357387 -2.76117014180893,-1.5877311684793 2.29083348415575 -2.75003105256685,-1.57967871833998 2.24456356819194 -2.73608379980013,-1.57004363039006 2.19951988653655 -2.71939533793547,-1.55887215831123 2.15591867344963 -2.7000457806996,-1.54621793126766 2.11396923855472 -2.67812801652963,-1.53214169645677 2.07387296203834 -2.6537472626579,-1.51671102748948 2.03582232791524 -2.62702056001177,-1.5 2.0 -2.59807621135332)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):28
#   EntityHandle (String) = 1D8
#   POLYGON Z ((-0.5 2.0 -0.866025403784439,-0.5 3.0 -0.866025403784439,-1.0 3.0 -1.73205080756888,-1.0 2.0 -1.73205080756888,-0.5 2.0 -0.866025403784439))
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON Z ((-0.5 2.0 -0.866025403784439,-0.5 3.0 -0.866025403784439,-1.0 3.0 -1.73205080756888,-1.0 2.0 -1.73205080756888,-0.5 2.0 -0.866025403784439))'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):29
#   EntityHandle (String) = 1D9
#   POLYGON ((-1.5 4.0,-2.0 4.0,-2.0 3.0,-1.5 3.0,-1.5 4.0))
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON ((-1.5 4.0,-2.0 4.0,-2.0 3.0,-1.5 3.0,-1.5 4.0))'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):30
#   EntityHandle (String) = 1DB
#   POLYGON ((-4.0 8.0,-4.5 8.0,-4.5 9.0,-4.0 9.0,-4.0 8.0))
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON ((-4.0 8.0,-4.5 8.0,-4.5 9.0,-4.0 9.0,-4.0 8.0))'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):31
#   EntityHandle (String) = 1DC
#   LINESTRING (-1.0 2.0,-1.07692307692308 2.15384615384615,-1.15384615384615 2.30769230769231,-1.23076923076923 2.46153846153846,-1.30769230769231 2.61538461538461,-1.38461538461538 2.76923076923077,-1.46153846153846 2.92307692307692,-1.53846153846154 3.07692307692308,-1.61538461538461 3.23076923076923,-1.69230769230769 3.38461538461538,-1.76923076923077 3.53846153846154,-1.84615384615384 3.69230769230769,-1.92307692307692 3.84615384615385,-2.0 4.0,-2.07692307692307 4.15384615384615,-2.15384615384615 4.30769230769231,-2.23076923076923 4.46153846153846,-2.30769230769231 4.61538461538462,-2.38461538461538 4.76923076923077,-2.46153846153846 4.92307692307692,-2.53846153846154 5.07692307692308,-2.61538461538461 5.23076923076923,-2.69230769230769 5.38461538461538,-2.76923076923077 5.53846153846154,-2.84615384615384 5.69230769230769,-2.92307692307692 5.84615384615385,-3.0 6.0,-3.07692307692308 6.15384615384615,-3.15384615384615 6.30769230769231,-3.23076923076923 6.46153846153846,-3.30769230769231 6.61538461538462,-3.38461538461538 6.76923076923077,-3.46153846153846 6.92307692307692,-3.53846153846154 7.07692307692308,-3.61538461538461 7.23076923076923,-3.69230769230769 7.38461538461539,-3.76923076923077 7.53846153846154,-3.84615384615384 7.69230769230769,-3.92307692307692 7.84615384615385,-4.0 8.0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (-1.0 2.0,-1.07692307692308 2.15384615384615,-1.15384615384615 2.30769230769231,-1.23076923076923 2.46153846153846,-1.30769230769231 2.61538461538461,-1.38461538461538 2.76923076923077,-1.46153846153846 2.92307692307692,-1.53846153846154 3.07692307692308,-1.61538461538461 3.23076923076923,-1.69230769230769 3.38461538461538,-1.76923076923077 3.53846153846154,-1.84615384615384 3.69230769230769,-1.92307692307692 3.84615384615385,-2.0 4.0,-2.07692307692307 4.15384615384615,-2.15384615384615 4.30769230769231,-2.23076923076923 4.46153846153846,-2.30769230769231 4.61538461538462,-2.38461538461538 4.76923076923077,-2.46153846153846 4.92307692307692,-2.53846153846154 5.07692307692308,-2.61538461538461 5.23076923076923,-2.69230769230769 5.38461538461538,-2.76923076923077 5.53846153846154,-2.84615384615384 5.69230769230769,-2.92307692307692 5.84615384615385,-3.0 6.0,-3.07692307692308 6.15384615384615,-3.15384615384615 6.30769230769231,-3.23076923076923 6.46153846153846,-3.30769230769231 6.61538461538462,-3.38461538461538 6.76923076923077,-3.46153846153846 6.92307692307692,-3.53846153846154 7.07692307692308,-3.61538461538461 7.23076923076923,-3.69230769230769 7.38461538461539,-3.76923076923077 7.53846153846154,-3.84615384615384 7.69230769230769,-3.92307692307692 7.84615384615385,-4.0 8.0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):32
#   EntityHandle (String) = 1DD
#   LINESTRING (-4.0 1.0,-3.81418685412768 0.987348067229724,-3.62887944840607 0.975707614760869,-3.44458352298589 0.966090122894857,-3.26180481801783 0.959507071933107,-3.08104907365262 0.956969942177043,-2.90282203004096 0.959490213928084,-2.72762942733357 0.968079367487651,-2.55597700568115 0.983748883157167,-2.38837050523441 1.00751024123805,-2.22531566614407 1.04037492203173,-2.06731822856083 1.08335440583961,-1.91488393263541 1.13746017296313,-1.76851851851852 1.2037037037037,-1.62872772636086 1.28309647836275,-1.49601729631315 1.37664997724169,-1.3708929685261 1.48537568064195,-1.25386048315042 1.61028506886495,-1.14542558033682 1.75238962221211,-1.04609400023601 1.91270082098484,-0.956350410492422 2.09218800047202,-0.876194811106054 2.29085116067364,-0.805142534432475 2.50772096630085,-0.742687840320977 2.74178593705221,-0.688324988620847 2.99203459262631,-0.641548239181376 3.25745545272172,-0.601851851851852 3.53703703703703,-0.568730086481566 3.82976786527082,-0.541677202919806 4.13463645712166,-0.520187461015863 4.45063133228814,-0.503755120619026 4.77674101046882,-0.491874441578583 5.11195401136229,-0.484039683743826 5.45525885466714,-0.479745106964042 5.80564406008193,-0.478484971088521 6.16209814730525,-0.479753535966554 6.52360963603567,-0.483045061447428 6.88916704597178,-0.487853807380435 7.25775889681216,-0.493674033614862 7.62837370825537,-0.5 8.0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (-4.0 1.0,-3.81418685412768 0.987348067229724,-3.62887944840607 0.975707614760869,-3.44458352298589 0.966090122894857,-3.26180481801783 0.959507071933107,-3.08104907365262 0.956969942177043,-2.90282203004096 0.959490213928084,-2.72762942733357 0.968079367487651,-2.55597700568115 0.983748883157167,-2.38837050523441 1.00751024123805,-2.22531566614407 1.04037492203173,-2.06731822856083 1.08335440583961,-1.91488393263541 1.13746017296313,-1.76851851851852 1.2037037037037,-1.62872772636086 1.28309647836275,-1.49601729631315 1.37664997724169,-1.3708929685261 1.48537568064195,-1.25386048315042 1.61028506886495,-1.14542558033682 1.75238962221211,-1.04609400023601 1.91270082098484,-0.956350410492422 2.09218800047202,-0.876194811106054 2.29085116067364,-0.805142534432475 2.50772096630085,-0.742687840320977 2.74178593705221,-0.688324988620847 2.99203459262631,-0.641548239181376 3.25745545272172,-0.601851851851852 3.53703703703703,-0.568730086481566 3.82976786527082,-0.541677202919806 4.13463645712166,-0.520187461015863 4.45063133228814,-0.503755120619026 4.77674101046882,-0.491874441578583 5.11195401136229,-0.484039683743826 5.45525885466714,-0.479745106964042 5.80564406008193,-0.478484971088521 6.16209814730525,-0.479753535966554 6.52360963603567,-0.483045061447428 6.88916704597178,-0.487853807380435 7.25775889681216,-0.493674033614862 7.62837370825537,-0.5 8.0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):33
#   EntityHandle (String) = 1DE
#   POINT Z (-3.5 7.0 -6.06217782649107)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POINT Z (-3.5 7.0 -6.06217782649107)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):34
#   EntityHandle (String) = 1DF
#   POINT Z (1.0 -2.0 -5.19615242270663)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POINT Z (1.0 -2.0 -5.19615242270663)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):35
#   EntityHandle (String) = 1E0
#   LINESTRING Z (0 0 0,0.25 -0.5 -1.29903810567666)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (0 0 0,0.25 -0.5 -1.29903810567666)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):36
#   EntityHandle (String) = 1E1
#   LINESTRING (0.25 -0.5,-0.25 -0.5,1.0 -1.0,0.25 -0.5)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (0.25 -0.5,-0.25 -0.5,1.0 -1.0,0.25 -0.5)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):37
#   EntityHandle (String) = 1E2
#   LINESTRING Z (0.25 -0.5 -1.29903810567666,1 -1 -1.73205080756888,0.5 -1.0 -2.59807621135332,0.25 -0.5 -1.29903810567666)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (0.25 -0.5 -1.29903810567666,1 -1 -1.73205080756888,0.5 -1.0 -2.59807621135332,0.25 -0.5 -1.29903810567666)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):38
#   EntityHandle (String) = 1E7
#   LINESTRING Z (2.78885438199983 -2.89442719099992 -5.19615242270663,2.76889880091653 -2.9234444547489 -5.33123525325721,2.74032532268425 -2.94796278993866 -5.46565997383582,2.70327315441675 -2.96786274570473 -5.59877168071614,2.65792281055334 -2.98304737146295 -5.72992186704258,2.60449523341074 -2.99344268924297 -5.85847158229053,2.54325071677425 -2.99899805410152 -5.98379454516908,2.47448763777266 -2.99968640085941 -6.1052801948005,2.39854100321483 -2.99550437596044 -6.22233666531148,2.31578081747018 -2.98647235380955 -6.33439366934417,2.22661027984464 -2.97263433751074 -6.44090527643885,2.13146382023413 -2.95405774448845 -6.54135257275227,2.03080498262578 -2.93083307803656 -6.63524618915372,1.92512416675824 -2.90307348639549 -6.72212868538223,1.81493623894341 -2.87091421150532 -6.80157677864958,1.70077802368955 -2.8345119301207 -6.87320340583148,1.58320568834623 -2.79404399049737 -6.93665960920016,1.46279203351292 -2.74970754836936 -6.99163623651142,1.34012370241203 -2.70171860642604 -7.03786544716322,1.21579832282211 -2.65031096196872 -7.07512201708822,1.09042159549536 -2.59573506787371 -7.10322443602275,0.964604343244512 -2.5382568124111 -7.12203579180661,0.838959535075416 -2.47815622386382 -7.13146443740534,0.714099299863701 -2.41572610625796 -7.13146443740534,0.590631944124412 -2.35127061285105 -7.12203579180661,0.469158988403815 -2.28510376432794 -7.10322443602275,0.350272236731764 -2.21754791892355 -7.07512201708822,0.234550893411949 -2.148932201926 -7.03786544716322,0.122558741196756 -2.07959090221128 -6.99163623651142,0.014841394594365 -2.00986184362144 -6.93665960920016,-0.088076358310285 -1.94008473912078 -6.87320340583148,-0.185693112570302 -1.87059953574847 -6.80157677864958,-0.277533289171331 -1.8017447584307 -6.72212868538223,-0.363149452004714 -1.73385586072131 -6.63524618915372,-0.442124487731234 -1.66726359050577 -6.54135257275227,-0.514073637915374 -1.60229237863074 -6.44090527643885,-0.57864637352974 -1.53925875830959 -6.33439366934417,-0.635528102697248 -1.47846982300441 -6.22233666531148,-0.684441703351125 -1.42022173029752 -6.1052801948005,-0.725148873345763 -1.36479825904151 -5.98379454516908,-0.757451291440819 -1.31246942681719 -5.85847158229053,-0.781191583502361 -1.2634901744351 -5.72992186704258,-0.796254089213832 -1.21809912388944 -5.59877168071614,-0.802565425561482 -1.1765174158158 -5.46565997383582,-0.80009484434904 -1.13894763211612 -5.33123525325721,-0.788854381999831 -1.10557280900008 -5.19615242270663,-0.768898800916532 -1.0765555452511 -5.06106959215606,-0.740325322684253 -1.05203721006134 -4.92664487157745,-0.703273154416746 -1.03213725429527 -4.79353316469713,
# -0.65792281055334 -1.01695262853705 -4.66238297837069,-0.604495233410736 -1.00655731075703 -4.53383326312274,-0.543250716774252 -1.00100194589848 -4.40851030024419,-0.474487637772662 -1.00031359914059 -4.28702465061277,-0.398541003214825 -1.00449562403956 -4.16996818010179,-0.315780817470174 -1.01352764619045 -4.0579111760691,-0.226610279844633 -1.02736566248926 -3.95139956897441,-0.131463820234124 -1.04594225551155 -3.85095227266099,-0.03080498262578 -1.06916692196344 -3.75705865625955,0.074875833241763 -1.09692651360451 -3.67017616003104,0.185063761056595 -1.12908578849468 -3.59072806676368,0.299221976310455 -1.1654880698793 -3.51910143958179,0.416794311653771 -1.20595600950263 -3.45564523621311,0.537207966487078 -1.25029245163064 -3.40066860890185,0.659876297587969 -1.29828139357396 -3.35443939825005,0.784201677177895 -1.34968903803128 -3.31718282832505,0.909578404504639 -1.40426493212629 -3.28908040939052,1.03539565675549 -1.4617431875889 -3.27026905360665,1.16104046492459 -1.52184377613619 -3.26084040800793,1.2859007001363 -1.58427389374204 -3.26084040800793,1.40936805587559 -1.64872938714895 -3.27026905360665,1.53084101159619 -1.71489623567206 -3.28908040939052,1.64972776326824 -1.78245208107646 -3.31718282832505,1.76544910658805 -1.851067798074 -3.35443939825005,1.87744125880325 -1.92040909778872 -3.40066860890185,1.98515860540564 -1.99013815637856 -3.45564523621311,2.08807635831029 -2.05991526087922 -3.51910143958179,2.1856931125703 -2.12940046425153 -3.59072806676368,2.27753328917133 -2.1982552415693 -3.67017616003104,2.36314945200472 -2.26614413927869 -3.75705865625955,2.44212448773124 -2.33273640949423 -3.85095227266099,2.51407363791538 -2.39770762136926 -3.95139956897441,2.57864637352974 -2.46074124169041 -4.0579111760691,2.63552810269725 -2.52153017699559 -4.16996818010179,2.68444170335113 -2.57977826970249 -4.28702465061277,2.72514887334576 -2.63520174095849 -4.40851030024419,2.75745129144082 -2.68753057318281 -4.53383326312274,2.78119158350236 -2.7365098255649 -4.66238297837069,2.79625408921383 -2.78190087611056 -4.79353316469713,2.80256542556148 -2.82348258418421 -4.92664487157744,2.80009484434904 -2.86105236788389 -5.06106959215606,2.78885438199983 -2.89442719099992 -5.19615242270663)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (2.78885438199983 -2.89442719099992 -5.19615242270663,2.76889880091653 -2.9234444547489 -5.33123525325721,2.74032532268425 -2.94796278993866 -5.46565997383582,2.70327315441675 -2.96786274570473 -5.59877168071614,2.65792281055334 -2.98304737146295 -5.72992186704258,2.60449523341074 -2.99344268924297 -5.85847158229053,2.54325071677425 -2.99899805410152 -5.98379454516908,2.47448763777266 -2.99968640085941 -6.1052801948005,2.39854100321483 -2.99550437596044 -6.22233666531148,2.31578081747018 -2.98647235380955 -6.33439366934417,2.22661027984464 -2.97263433751074 -6.44090527643885,2.13146382023413 -2.95405774448845 -6.54135257275227,2.03080498262578 -2.93083307803656 -6.63524618915372,1.92512416675824 -2.90307348639549 -6.72212868538223,1.81493623894341 -2.87091421150532 -6.80157677864958,1.70077802368955 -2.8345119301207 -6.87320340583148,1.58320568834623 -2.79404399049737 -6.93665960920016,1.46279203351292 -2.74970754836936 -6.99163623651142,1.34012370241203 -2.70171860642604 -7.03786544716322,1.21579832282211 -2.65031096196872 -7.07512201708822,1.09042159549536 -2.59573506787371 -7.10322443602275,0.964604343244512 -2.5382568124111 -7.12203579180661,0.838959535075416 -2.47815622386382 -7.13146443740534,0.714099299863701 -2.41572610625796 -7.13146443740534,0.590631944124412 -2.35127061285105 -7.12203579180661,0.469158988403815 -2.28510376432794 -7.10322443602275,0.350272236731764 -2.21754791892355 -7.07512201708822,0.234550893411949 -2.148932201926 -7.03786544716322,0.122558741196756 -2.07959090221128 -6.99163623651142,0.014841394594365 -2.00986184362144 -6.93665960920016,-0.088076358310285 -1.94008473912078 -6.87320340583148,-0.185693112570302 -1.87059953574847 -6.80157677864958,-0.277533289171331 -1.8017447584307 -6.72212868538223,-0.363149452004714 -1.73385586072131 -6.63524618915372,-0.442124487731234 -1.66726359050577 -6.54135257275227,-0.514073637915374 -1.60229237863074 -6.44090527643885,-0.57864637352974 -1.53925875830959 -6.33439366934417,-0.635528102697248 -1.47846982300441 -6.22233666531148,-0.684441703351125 -1.42022173029752 -6.1052801948005,-0.725148873345763 -1.36479825904151 -5.98379454516908,-0.757451291440819 -1.31246942681719 -5.85847158229053,-0.781191583502361 -1.2634901744351 -5.72992186704258,-0.796254089213832 -1.21809912388944 -5.59877168071614,-0.802565425561482 -1.1765174158158 -5.46565997383582,-0.80009484434904 -1.13894763211612 -5.33123525325721,-0.788854381999831 -1.10557280900008 -5.19615242270663,-0.768898800916532 -1.0765555452511 -5.06106959215606,-0.740325322684253 -1.05203721006134 -4.92664487157745,-0.703273154416746 -1.03213725429527 -4.79353316469713,-0.65792281055334 -1.01695262853705 -4.66238297837069,-0.604495233410736 -1.00655731075703 -4.53383326312274,-0.543250716774252 -1.00100194589848 -4.40851030024419,-0.474487637772662 -1.00031359914059 -4.28702465061277,-0.398541003214825 -1.00449562403956 -4.16996818010179,-0.315780817470174 -1.01352764619045 -4.0579111760691,' +\
        '-0.226610279844633 -1.02736566248926 -3.95139956897441,-0.131463820234124 -1.04594225551155 -3.85095227266099,-0.03080498262578 -1.06916692196344 -3.75705865625955,0.074875833241763 -1.09692651360451 -3.67017616003104,0.185063761056595 -1.12908578849468 -3.59072806676368,0.299221976310455 -1.1654880698793 -3.51910143958179,0.416794311653771 -1.20595600950263 -3.45564523621311,0.537207966487078 -1.25029245163064 -3.40066860890185,0.659876297587969 -1.29828139357396 -3.35443939825005,0.784201677177895 -1.34968903803128 -3.31718282832505,0.909578404504639 -1.40426493212629 -3.28908040939052,1.03539565675549 -1.4617431875889 -3.27026905360665,1.16104046492459 -1.52184377613619 -3.26084040800793,1.2859007001363 -1.58427389374204 -3.26084040800793,1.40936805587559 -1.64872938714895 -3.27026905360665,1.53084101159619 -1.71489623567206 -3.28908040939052,1.64972776326824 -1.78245208107646 -3.31718282832505,1.76544910658805 -1.851067798074 -3.35443939825005,1.87744125880325 -1.92040909778872 -3.40066860890185,1.98515860540564 -1.99013815637856 -3.45564523621311,2.08807635831029 -2.05991526087922 -3.51910143958179,2.1856931125703 -2.12940046425153 -3.59072806676368,2.27753328917133 -2.1982552415693 -3.67017616003104,2.36314945200472 -2.26614413927869 -3.75705865625955,2.44212448773124 -2.33273640949423 -3.85095227266099,2.51407363791538 -2.39770762136926 -3.95139956897441,2.57864637352974 -2.46074124169041 -4.0579111760691,2.63552810269725 -2.52153017699559 -4.16996818010179,2.68444170335113 -2.57977826970249 -4.28702465061277,2.72514887334576 -2.63520174095849 -4.40851030024419,2.75745129144082 -2.68753057318281 -4.53383326312274,2.78119158350236 -2.7365098255649 -4.66238297837069,2.79625408921383 -2.78190087611056 -4.79353316469713,2.80256542556148 -2.82348258418421 -4.92664487157744,2.80009484434904 -2.86105236788389 -5.06106959215606,2.78885438199983 -2.89442719099992 -5.19615242270663)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):39
#   EntityHandle (String) = 1E8
#   LINESTRING Z (2.0 -2.0 -3.46410161513775,1.0 -2.0 -5.19615242270663)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (2.0 -2.0 -3.46410161513775,1.0 -2.0 -5.19615242270663)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):40
#   EntityHandle (String) = 1E9
#   LINESTRING Z (1.0 -2.0 -5.19615242270663,0.0 -2.0 -6.92820323027551)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (1.0 -2.0 -5.19615242270663,0.0 -2.0 -6.92820323027551)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):41
#   EntityHandle (String) = 1EA
#   LINESTRING Z (0.25 -1.5 -4.76313972081441,1.0 -2.0 -5.19615242270663)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (0.25 -1.5 -4.76313972081441,1.0 -2.0 -5.19615242270663)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):42
#   EntityHandle (String) = 1EB
#   LINESTRING Z (1.0 -2.0 -5.19615242270663,1.75 -2.5 -5.62916512459885)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (1.0 -2.0 -5.19615242270663,1.75 -2.5 -5.62916512459885)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):43
#   EntityHandle (String) = 1EC
#   LINESTRING Z (2.0 -2.0 -3.46410161513775,2.04988140556792 -2.03487823687206 -3.49852624302284,2.09464789446162 -2.06958655048003 -3.54122153501056,2.13408136884713 -2.10395584540888 -3.59197948393006,2.16798971280107 -2.1378186779085 -3.65055280215638,2.19620772828016 -2.17101007166283 -3.7166561263709,2.21859793994945 -2.2033683215379 -3.78996740782271,2.23505126494835 -2.23473578139294 -3.87012948131781,2.24548754433133 -2.2649596321166 -3.956751805292,2.2498559335943 -2.29389262614624 -4.04941236449012,2.24813515038388 -2.32139380484327 -4.14765972598196,2.2403335781829 -2.3473291852295 -4.25101523849813,2.2264892254669 -2.3715724127387 -4.35897536437091,2.2066695405307 -2.39400537680336 -4.47101413271915,2.18097108288703 -2.41451878627752 -4.58658570192556,2.14951905283833 -2.43301270189222 -4.70512701892219,2.11246668151345 -2.44939702314958 -4.82606056232836,2.069994484341 -2.46359192728339 -4.94879715607678,2.02230938159631 -2.47552825814758 -5.07273883982016,1.96964369030667 -2.485147863138 -5.19728178213388,1.91225399242609 -2.4924038765061 -5.32181922232198,1.85041988479386 -2.49726094768414 -5.44574442649435,1.78444261696682 -2.49969541350955 -5.56845364351315,1.71464362356182 -2.49969541350955 -5.68934904640778,1.64136295825855 -2.49726094768414 -5.80784164492769,1.56495763709223 -2.4924038765061 -5.92335415504372,1.48579989910733 -2.485147863138 -6.03532381141787,1.40427539284642 -2.47552825814758 -6.14320510913943,1.32078129750918 -2.46359192728339 -6.24647246137009,1.2357243879353 -2.44939702314958 -6.34462275995019,1.14951905283833 -2.43301270189222 -6.43717782649107,1.06258527594554 -2.41451878627752 -6.52368674201215,0.975346589879385 -2.39400537680336 -6.60372804377285,0.888228012749189 -2.3715724127387 -6.67691177859673,0.801653977505599 -2.3473291852295 -6.74288140268412,0.716046264145928 -2.32139380484327 -6.80131551865772,0.631821944844409 -2.29389262614623 -6.85192944137827,0.549391352018479 -2.2649596321166 -6.89447658490197,0.469156079230493 -2.23473578139294 -6.92874966382241,0.391507024664251 -2.2033683215379 -6.9545817031442,0.316822486708345 -2.17101007166283 -6.97184685176839,0.245466320924432 -2.1378186779085 -6.98046099562637,0.177786167379516 -2.10395584540888 -6.98038216747516,0.114111756978481 -2.06958655048003 -6.97161075135758,0.054753305048271 -2.03487823687206 -6.95418948073126,0.0 -2.0 -6.92820323027551,-0.049881405567917 -1.96512176312794 -6.89377860239042,-0.094647894461618 -1.93041344951997 -6.8510833104027,-0.134081368847123 -1.89604415459112 -6.8003253614832,-0.167989712801067 -1.8621813220915 -6.74175204325688,-0.196207728280158 -1.82898992833716 -6.67564871904237,-0.218597939949449 -1.7966316784621 -6.60233743759055,-0.235051264948343 -1.76526421860705 -6.52217536409545,
#-0.245487544331328 -1.7350403678834 -6.43555304012126,-0.2498559335943 -1.70610737385376 -6.34289248092314,-0.248135150383881 -1.67860619515673 -6.2446451194313,-0.240333578182897 -1.6526708147705 -6.14128960691513,-0.226489225466902 -1.6284275872613 -6.03332948104236,-0.206669540530698 -1.60599462319664 -5.92129071269411,-0.180971082887026 -1.58548121372248 -5.8057191434877,-0.149519052838327 -1.56698729810778 -5.68717782649107,-0.112466681513451 -1.55060297685042 -5.5662442830849,-0.069994484341001 -1.5364080727166 -5.44350768933648,-0.022309381596311 -1.52447174185242 -5.3195660055931,0.030356309693336 -1.514852136862 -5.19502306327939,0.087746007573915 -1.50759612349389 -5.07048562309128,0.149580115206143 -1.50273905231586 -4.94656041891892,0.215557383033179 -1.50030458649045 -4.82385120190011,0.28535637643818 -1.50030458649045 -4.70295579900548,0.35863704174145 -1.50273905231586 -4.58446320048557,0.435042362907775 -1.50759612349389 -4.46895069036954,0.514200100892672 -1.514852136862 -4.35698103399539,0.595724607153583 -1.52447174185242 -4.24909973627383,0.679218702490823 -1.5364080727166 -4.14583238404317,0.764275612064703 -1.55060297685042 -4.04768208546307,0.850480947161672 -1.56698729810778 -3.95512701892219,0.937414724054466 -1.58548121372248 -3.86861810340111,1.02465341012062 -1.60599462319664 -3.78857680164041,1.11177198725081 -1.6284275872613 -3.71539306681653,1.1983460224944 -1.6526708147705 -3.64942344272914,1.28395373585407 -1.67860619515673 -3.59098932675554,1.36817805515559 -1.70610737385376 -3.54037540403499,1.45060864798152 -1.7350403678834 -3.49782826051129,1.53084392076951 -1.76526421860705 -3.46355518159085,1.60849297533575 -1.7966316784621 -3.43772314226906,1.68317751329166 -1.82898992833716 -3.42045799364487,1.75453367907557 -1.8621813220915 -3.41184384978689,1.82221383262049 -1.89604415459112 -3.4119226779381,1.88588824302152 -1.93041344951997 -3.42069409405568,1.94524669495173 -1.96512176312794 -3.438115364682,2.0 -2.0 -3.46410161513775)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (2.0 -2.0 -3.46410161513775,2.04988140556792 -2.03487823687206 -3.49852624302284,2.09464789446162 -2.06958655048003 -3.54122153501056,2.13408136884713 -2.10395584540888 -3.59197948393006,2.16798971280107 -2.1378186779085 -3.65055280215638,2.19620772828016 -2.17101007166283 -3.7166561263709,2.21859793994945 -2.2033683215379 -3.78996740782271,2.23505126494835 -2.23473578139294 -3.87012948131781,2.24548754433133 -2.2649596321166 -3.956751805292,2.2498559335943 -2.29389262614624 -4.04941236449012,2.24813515038388 -2.32139380484327 -4.14765972598196,2.2403335781829 -2.3473291852295 -4.25101523849813,2.2264892254669 -2.3715724127387 -4.35897536437091,2.2066695405307 -2.39400537680336 -4.47101413271915,2.18097108288703 -2.41451878627752 -4.58658570192556,2.14951905283833 -2.43301270189222 -4.70512701892219,2.11246668151345 -2.44939702314958 -4.82606056232836,2.069994484341 -2.46359192728339 -4.94879715607678,2.02230938159631 -2.47552825814758 -5.07273883982016,1.96964369030667 -2.485147863138 -5.19728178213388,1.91225399242609 -2.4924038765061 -5.32181922232198,1.85041988479386 -2.49726094768414 -5.44574442649435,1.78444261696682 -2.49969541350955 -5.56845364351315,1.71464362356182 -2.49969541350955 -5.68934904640778,1.64136295825855 -2.49726094768414 -5.80784164492769,1.56495763709223 -2.4924038765061 -5.92335415504372,1.48579989910733 -2.485147863138 -6.03532381141787,1.40427539284642 -2.47552825814758 -6.14320510913943,1.32078129750918 -2.46359192728339 -6.24647246137009,1.2357243879353 -2.44939702314958 -6.34462275995019,1.14951905283833 -2.43301270189222 -6.43717782649107,1.06258527594554 -2.41451878627752 -6.52368674201215,0.975346589879385 -2.39400537680336 -6.60372804377285,0.888228012749189 -2.3715724127387 -6.67691177859673,0.801653977505599 -2.3473291852295 -6.74288140268412,0.716046264145928 -2.32139380484327 -6.80131551865772,0.631821944844409 -2.29389262614623 -6.85192944137827,0.549391352018479 -2.2649596321166 -6.89447658490197,0.469156079230493 -2.23473578139294 -6.92874966382241,0.391507024664251 -2.2033683215379 -6.9545817031442,0.316822486708345 -2.17101007166283 -6.97184685176839,0.245466320924432 -2.1378186779085 -6.98046099562637,0.177786167379516 -2.10395584540888 -6.98038216747516,0.114111756978481 -2.06958655048003 -6.97161075135758,0.054753305048271 -2.03487823687206 -6.95418948073126,0.0 -2.0 -6.92820323027551,-0.049881405567917 -1.96512176312794 -6.89377860239042,-0.094647894461618 -1.93041344951997 -6.8510833104027,-0.134081368847123 -1.89604415459112 -6.8003253614832,-0.167989712801067 -1.8621813220915 -6.74175204325688,-0.196207728280158 -1.82898992833716 -6.67564871904237,-0.218597939949449 -1.7966316784621 -6.60233743759055,' + \
        '-0.235051264948343 -1.76526421860705 -6.52217536409545,-0.245487544331328 -1.7350403678834 -6.43555304012126,-0.2498559335943 -1.70610737385376 -6.34289248092314,-0.248135150383881 -1.67860619515673 -6.2446451194313,-0.240333578182897 -1.6526708147705 -6.14128960691513,-0.226489225466902 -1.6284275872613 -6.03332948104236,-0.206669540530698 -1.60599462319664 -5.92129071269411,-0.180971082887026 -1.58548121372248 -5.8057191434877,-0.149519052838327 -1.56698729810778 -5.68717782649107,-0.112466681513451 -1.55060297685042 -5.5662442830849,-0.069994484341001 -1.5364080727166 -5.44350768933648,-0.022309381596311 -1.52447174185242 -5.3195660055931,0.030356309693336 -1.514852136862 -5.19502306327939,0.087746007573915 -1.50759612349389 -5.07048562309128,0.149580115206143 -1.50273905231586 -4.94656041891892,0.215557383033179 -1.50030458649045 -4.82385120190011,0.28535637643818 -1.50030458649045 -4.70295579900548,0.35863704174145 -1.50273905231586 -4.58446320048557,0.435042362907775 -1.50759612349389 -4.46895069036954,0.514200100892672 -1.514852136862 -4.35698103399539,0.595724607153583 -1.52447174185242 -4.24909973627383,0.679218702490823 -1.5364080727166 -4.14583238404317,0.764275612064703 -1.55060297685042 -4.04768208546307,0.850480947161672 -1.56698729810778 -3.95512701892219,0.937414724054466 -1.58548121372248 -3.86861810340111,1.02465341012062 -1.60599462319664 -3.78857680164041,1.11177198725081 -1.6284275872613 -3.71539306681653,1.1983460224944 -1.6526708147705 -3.64942344272914,1.28395373585407 -1.67860619515673 -3.59098932675554,1.36817805515559 -1.70610737385376 -3.54037540403499,1.45060864798152 -1.7350403678834 -3.49782826051129,1.53084392076951 -1.76526421860705 -3.46355518159085,1.60849297533575 -1.7966316784621 -3.43772314226906,1.68317751329166 -1.82898992833716 -3.42045799364487,1.75453367907557 -1.8621813220915 -3.41184384978689,1.82221383262049 -1.89604415459112 -3.4119226779381,1.88588824302152 -1.93041344951997 -3.42069409405568,1.94524669495173 -1.96512176312794 -3.438115364682,2.0 -2.0 -3.46410161513775)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):44
#   EntityHandle (String) = 1ED
#   LINESTRING Z (0.5 -1.0 -2.59807621135332,0.543577773425908 -1.01791116395762 -2.58464338569351,0.587546417985528 -1.03693648101917 -2.57439309093773,0.631694860183701 -1.05698461927736 -2.56737453409618,0.675811163398452 -1.07795933672482 -2.56362140807291,0.719683545292469 -1.09975994326827 -2.56315172992158,0.763101394483934 -1.12228178409597 -2.5659677543537,0.805856281596113 -1.14541674207787 -2.57205596291479,0.847742959832148 -1.16905375678694 -2.58138712888039,0.888560350271717 -1.19307935764976 -2.59391645756022,0.928112507159618 -1.21737820866727 -2.60958380133721,0.96620955855235 -1.24183366209053 -2.62831394840886,1.00266861780709 -1.26632831839353 -2.65001698384495,1.03731466153743 -1.29074458985506 -2.67458872122829,1.06998136982112 -1.31496526504393 -2.7019112028064,1.10051192462654 -1.33887407149783 -2.73185326575312,1.12875976262485 -1.36235623389463 -2.76427117182176,1.15458927877392 -1.38529902503655 -2.79900929736714,1.17787647729662 -1.40759230700227 -2.83590088042404,1.19850956692819 -1.42912905986906 -2.8747688212557,1.21638949757537 -1.44980589546683 -2.91542653252918,1.23143043581088 -1.46952355369781 -2.95767883503644,1.24356017692077 -1.48818737903916 -3.00132289466106,1.2527204915264 -1.50570777494116 -3.04614919609277,1.25886740511731 -1.52200063393958 -3.09194254861523,1.26197140915283 -1.53698774141742 -3.138483119139,1.2620176027192 -1.55059715107771 -3.18554748752036,1.25900576406208 -1.56276353032509 -3.23290971909991,1.25295035165112 -1.57342847389788 -3.28034244931235,1.24388043477135 -1.58254078424523 -3.32761797516041,1.23183955397484 -1.59005671730331 -3.37450934831362,1.21688551206218 -1.5959401924907 -3.42079146458413,1.19909009659756 -1.60016296591487 -3.46624214454969,1.17853873528923 -1.6027047659583 -3.51064320013621,1.15533008588991 -1.60355339059327 -3.55378148203954,1.12957556258567 -1.6027047659583 -3.59544990295855,1.10139880114707 -1.60016296591487 -3.63544843172717,1.07093506540992 -1.5959401924907 -3.67358505357321,1.03833059793508 -1.59005671730331 -3.70967669189409,1.00374191796432 -1.58254078424523 -3.74355008712442,0.967335070042516 -1.57342847389788 -3.77504262847653,0.929284826913179 -1.56276353032509 -3.80400313456104,0.889773850513921 -1.55059715107771 -3.83029257913998,0.848991815099414 -1.53698774141741 -3.85378475852869,0.807134496701444 -1.52200063393958 -3.87436689744237,0.764402833297081 -1.50570777494116 -3.891940190379,0.72100196019671 -1.48818737903916 -3.90642027593972,0.677140225282558 -1.46952355369781 -3.91773764180954,0.633028188825132 -1.44980589546683 -3.92583795845446,0.588877612678977 -1.42912905986906 -3.93068233993284,0.544900443710183 -1.40759230700227 -3.93224753056917,0.501307796335731 -1.38529902503655 -3.93052601659399,0.458308939059047 -1.36235623389463 -3.92552606221412,0.416110289866958 -1.33887407149783 -3.91727166993992,0.374914425310669 -1.31496526504393 -3.90580246536015,0.334919108027743 -1.29074458985506 -3.89117350691752,0.296316337373485 -1.26632831839353 -3.87345502159809,0.259291427719229 -1.24183366209053 -3.85273206780345,0.224022118842197 -1.21737820866727 -3.82910412702388,0.190677722677547 -1.19307935764975 -3.80268462627299,0.159418310528662 -1.16905375678694 -3.77360039357605,0.13039394463751 -1.14541674207787 -3.7419910491263,0.103743957803975 -1.12228178409597 -3.70800833503176,0.079596284512354 -1.09975994326827 -3.67181538687033,0.058066846776 -1.07795933672482 -3.63358595054991,0.039258997648375 -1.05698461927736 -3.59350354823325,0.023263025071983 -1.03693648101917 -3.55176059733134,0.010155718446952 -1.01791116395762 -3.50855748679486,0.0 -1.0 -3.46410161513776)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (0.5 -1.0 -2.59807621135332,0.543577773425908 -1.01791116395762 -2.58464338569351,0.587546417985528 -1.03693648101917 -2.57439309093773,0.631694860183701 -1.05698461927736 -2.56737453409618,0.675811163398452 -1.07795933672482 -2.56362140807291,0.719683545292469 -1.09975994326827 -2.56315172992158,0.763101394483934 -1.12228178409597 -2.5659677543537,0.805856281596113 -1.14541674207787 -2.57205596291479,0.847742959832148 -1.16905375678694 -2.58138712888039,0.888560350271717 -1.19307935764976 -2.59391645756022,0.928112507159618 -1.21737820866727 -2.60958380133721,0.96620955855235 -1.24183366209053 -2.62831394840886,1.00266861780709 -1.26632831839353 -2.65001698384495,1.03731466153743 -1.29074458985506 -2.67458872122829,1.06998136982112 -1.31496526504393 -2.7019112028064,1.10051192462654 -1.33887407149783 -2.73185326575312,1.12875976262485 -1.36235623389463 -2.76427117182176,1.15458927877392 -1.38529902503655 -2.79900929736714,1.17787647729662 -1.40759230700227 -2.83590088042404,1.19850956692819 -1.42912905986906 -2.8747688212557,1.21638949757537 -1.44980589546683 -2.91542653252918,1.23143043581088 -1.46952355369781 -2.95767883503644,1.24356017692077 -1.48818737903916 -3.00132289466106,1.2527204915264 -1.50570777494116 -3.04614919609277,1.25886740511731 -1.52200063393958 -3.09194254861523,1.26197140915283 -1.53698774141742 -3.138483119139,1.2620176027192 -1.55059715107771 -3.18554748752036,1.25900576406208 -1.56276353032509 -3.23290971909991,1.25295035165112 -1.57342847389788 -3.28034244931235,1.24388043477135 -1.58254078424523 -3.32761797516041,1.23183955397484 -1.59005671730331 -3.37450934831362,1.21688551206218 -1.5959401924907 -3.42079146458413,1.19909009659756 -1.60016296591487 -3.46624214454969,1.17853873528923 -1.6027047659583 -3.51064320013621,1.15533008588991 -1.60355339059327 -3.55378148203954,1.12957556258567 -1.6027047659583 -3.59544990295855,1.10139880114707 -1.60016296591487 -3.63544843172717,1.07093506540992 -1.5959401924907 -3.67358505357321,1.03833059793508 -1.59005671730331 -3.70967669189409,1.00374191796432 -1.58254078424523 -3.74355008712442,0.967335070042516 -1.57342847389788 -3.77504262847653,0.929284826913179 -1.56276353032509 -3.80400313456104,0.889773850513921 -1.55059715107771 -3.83029257913998,0.848991815099414 -1.53698774141741 -3.85378475852869,0.807134496701444 -1.52200063393958 -3.87436689744237,0.764402833297081 -1.50570777494116 -3.891940190379,0.72100196019671 -1.48818737903916 -3.90642027593972,0.677140225282558 -1.46952355369781 -3.91773764180954,0.633028188825132 -1.44980589546683 -3.92583795845446,0.588877612678977 -1.42912905986906 -3.93068233993284,0.544900443710183 -1.40759230700227 -3.93224753056917,0.501307796335731 -1.38529902503655 -3.93052601659399,0.458308939059047 -1.36235623389463 -3.92552606221412,0.416110289866958 -1.33887407149783 -3.91727166993992,0.374914425310669 -1.31496526504393 -3.90580246536015,0.334919108027743 -1.29074458985506 -3.89117350691752,0.296316337373485 -1.26632831839353 -3.87345502159809,0.259291427719229 -1.24183366209053 -3.85273206780345,0.224022118842197 -1.21737820866727 -3.82910412702388,0.190677722677547 -1.19307935764975 -3.80268462627299,0.159418310528662 -1.16905375678694 -3.77360039357605,0.13039394463751 -1.14541674207787 -3.7419910491263,0.103743957803975 -1.12228178409597 -3.70800833503176,0.079596284512354 -1.09975994326827 -3.67181538687033,0.058066846776 -1.07795933672482 -3.63358595054991,0.039258997648375 -1.05698461927736 -3.59350354823325,0.023263025071983 -1.03693648101917 -3.55176059733134,0.010155718446952 -1.01791116395762 -3.50855748679486,0.0 -1.0 -3.46410161513776)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):45
#   EntityHandle (String) = 1EE
#   POLYGON Z ((1 -1 -1.73205080756888,1.75 -1.5 -2.1650635094611,1.25 -1.5 -3.03108891324553,0.5 -1.0 -2.59807621135332,1 -1 -1.73205080756888))
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON Z ((1 -1 -1.73205080756888,1.75 -1.5 -2.1650635094611,1.25 -1.5 -3.03108891324553,0.5 -1.0 -2.59807621135332,1 -1 -1.73205080756888))'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):46
#   EntityHandle (String) = 1EF
#   POLYGON ((1.5 -2.0,0.75 -1.5,0.25 -1.5,1.0 -2.0,1.5 -2.0))
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON ((1.5 -2.0,0.75 -1.5,0.25 -1.5,1.0 -2.0,1.5 -2.0))'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):47
#   EntityHandle (String) = 1F1
#   POLYGON ((2.0 -4.0,1.5 -4.0,2.25 -4.5,2.75 -4.5,2.0 -4.0))
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON ((2.0 -4.0,1.5 -4.0,2.25 -4.5,2.75 -4.5,2.0 -4.0))'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):48
#   EntityHandle (String) = 1F2
#   LINESTRING (0.5 -1.0,0.53846153846154 -1.07692307692308,0.576923076923078 -1.15384615384615,0.615384615384617 -1.23076923076923,0.653846153846155 -1.30769230769231,0.692307692307694 -1.38461538461538,0.730769230769232 -1.46153846153846,0.769230769230771 -1.53846153846154,0.807692307692309 -1.61538461538462,0.846153846153848 -1.69230769230769,0.884615384615386 -1.76923076923077,0.923076923076924 -1.84615384615385,0.961538461538463 -1.92307692307692,1.0 -2.0,1.03846153846154 -2.07692307692308,1.07692307692308 -2.15384615384615,1.11538461538462 -2.23076923076923,1.15384615384616 -2.30769230769231,1.19230769230769 -2.38461538461538,1.23076923076923 -2.46153846153846,1.26923076923077 -2.53846153846154,1.30769230769231 -2.61538461538461,1.34615384615385 -2.69230769230769,1.38461538461539 -2.76923076923077,1.42307692307693 -2.84615384615385,1.46153846153846 -2.92307692307692,1.5 -3.0,1.53846153846154 -3.07692307692308,1.57692307692308 -3.15384615384615,1.61538461538462 -3.23076923076923,1.65384615384616 -3.30769230769231,1.6923076923077 -3.38461538461539,1.73076923076923 -3.46153846153846,1.76923076923077 -3.53846153846154,1.80769230769231 -3.61538461538461,1.84615384615385 -3.69230769230769,1.88461538461539 -3.76923076923077,1.92307692307693 -3.84615384615385,1.96153846153847 -3.92307692307692,2.0 -4.0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (0.5 -1.0,0.53846153846154 -1.07692307692308,0.576923076923078 -1.15384615384615,0.615384615384617 -1.23076923076923,0.653846153846155 -1.30769230769231,0.692307692307694 -1.38461538461538,0.730769230769232 -1.46153846153846,0.769230769230771 -1.53846153846154,0.807692307692309 -1.61538461538462,0.846153846153848 -1.69230769230769,0.884615384615386 -1.76923076923077,0.923076923076924 -1.84615384615385,0.961538461538463 -1.92307692307692,1.0 -2.0,1.03846153846154 -2.07692307692308,1.07692307692308 -2.15384615384615,1.11538461538462 -2.23076923076923,1.15384615384616 -2.30769230769231,1.19230769230769 -2.38461538461538,1.23076923076923 -2.46153846153846,1.26923076923077 -2.53846153846154,1.30769230769231 -2.61538461538461,1.34615384615385 -2.69230769230769,1.38461538461539 -2.76923076923077,1.42307692307693 -2.84615384615385,1.46153846153846 -2.92307692307692,1.5 -3.0,1.53846153846154 -3.07692307692308,1.57692307692308 -3.15384615384615,1.61538461538462 -3.23076923076923,1.65384615384616 -3.30769230769231,1.6923076923077 -3.38461538461539,1.73076923076923 -3.46153846153846,1.76923076923077 -3.53846153846154,1.80769230769231 -3.61538461538461,1.84615384615385 -3.69230769230769,1.88461538461539 -3.76923076923077,1.92307692307693 -3.84615384615385,1.96153846153847 -3.92307692307692,2.0 -4.0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):49
#   EntityHandle (String) = 1F3
#   LINESTRING (-3.25 -0.5,-3.07367580370539 -0.493674033614862,-2.89709873733542 -0.487853807380434,-2.72001593081474 -0.483045061447428,-2.542174514068 -0.479753535966554,-2.36332161701984 -0.478484971088521,-2.1832043695949 -0.479745106964042,-2.00156990171783 -0.484039683743825,-1.81816534331327 -0.491874441578583,-1.63273782430587 -0.503755120619026,-1.44503447462027 -0.520187461015863,-1.25480242418112 -0.541677202919806,-1.06178880291306 -0.568730086481565,-0.865740740740739 -0.601851851851852,-0.666405367588798 -0.641548239181375,-0.463529813381883 -0.688324988620846,-0.256861208044639 -0.742687840320976,-0.04614668150171 -0.805142534432475,0.168866636322258 -0.876194811106053,0.388431615502622 -0.956350410492422,0.612790589861596 -1.04609400023601,0.841943559399181 -1.14542558033682,1.07564819029316 -1.25386048315042,1.31365161246818 -1.3708929685261,1.55570095584888 -1.49601729631315,1.80154335035992 -1.62872772636086,2.05092592592592 -1.76851851851852,2.30359581247155 -1.91488393263541,2.55930013992144 -2.06731822856083,2.81778603820024 -2.22531566614407,3.07880063723259 -2.38837050523441,3.34209106694314 -2.55597700568115,3.60740445725653 -2.72762942733357,3.87448793809741 -2.90282203004097,4.14308863939042 -3.08104907365262,4.4129536910602 -3.26180481801784,4.68383022303141 -3.44458352298589,4.95546536522868 -3.62887944840608,5.22760624757667 -3.81418685412768,5.5 -4.0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (-3.25 -0.5,-3.07367580370539 -0.493674033614862,-2.89709873733542 -0.487853807380434,-2.72001593081474 -0.483045061447428,-2.542174514068 -0.479753535966554,-2.36332161701984 -0.478484971088521,-2.1832043695949 -0.479745106964042,-2.00156990171783 -0.484039683743825,-1.81816534331327 -0.491874441578583,-1.63273782430587 -0.503755120619026,-1.44503447462027 -0.520187461015863,-1.25480242418112 -0.541677202919806,-1.06178880291306 -0.568730086481565,-0.865740740740739 -0.601851851851852,-0.666405367588798 -0.641548239181375,-0.463529813381883 -0.688324988620846,-0.256861208044639 -0.742687840320976,-0.04614668150171 -0.805142534432475,0.168866636322258 -0.876194811106053,0.388431615502622 -0.956350410492422,0.612790589861596 -1.04609400023601,0.841943559399181 -1.14542558033682,1.07564819029316 -1.25386048315042,1.31365161246818 -1.3708929685261,1.55570095584888 -1.49601729631315,1.80154335035992 -1.62872772636086,2.05092592592592 -1.76851851851852,2.30359581247155 -1.91488393263541,2.55930013992144 -2.06731822856083,2.81778603820024 -2.22531566614407,3.07880063723259 -2.38837050523441,3.34209106694314 -2.55597700568115,3.60740445725653 -2.72762942733357,3.87448793809741 -2.90282203004097,4.14308863939042 -3.08104907365262,4.4129536910602 -3.26180481801784,4.68383022303141 -3.44458352298589,4.95546536522868 -3.62887944840608,5.22760624757667 -3.81418685412768,5.5 -4.0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):50
#   EntityHandle (String) = 1F4
#   POINT Z (1.75 -3.5 -9.09326673973661)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POINT Z (1.75 -3.5 -9.09326673973661)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):51
#   EntityHandle (String) = 1F5
#   POINT Z (5.5 1.0 -0.866025403784439)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POINT Z (5.5 1.0 -0.866025403784439)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):52
#   EntityHandle (String) = 1F6
#   LINESTRING Z (0 0 0,1.375 0.25 -0.21650635094611)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (0 0 0,1.375 0.25 -0.21650635094611)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):53
#   EntityHandle (String) = 1F7
#   LINESTRING (1.375 0.25,2.0 1.0,2.125 -0.25,1.375 0.25)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (1.375 0.25,2.0 1.0,2.125 -0.25,1.375 0.25)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):54
#   EntityHandle (String) = 1F8
#   LINESTRING Z (1.375 0.25 -0.21650635094611,2.125 -0.25 -0.649519052838329,2.75 0.5 -0.43301270189222,1.375 0.25 -0.21650635094611)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (1.375 0.25 -0.21650635094611,2.125 -0.25 -0.649519052838329,2.75 0.5 -0.43301270189222,1.375 0.25 -0.21650635094611)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):55
#   EntityHandle (String) = 1FD
#   LINESTRING Z (3.71114561800017 0.105572809000085 -0.866025403784439,3.77009625337411 -0.001434563330173 -0.933566819059727,3.83747480591229 -0.103563047131737 -1.00077917934903,3.91295301407961 -0.200315082697444 -1.06733503278919,3.99616315563294 -0.2912193038355 -1.13291012595241,4.08669983912687 -0.37583283431818 -1.19718498357639,4.18412197894014 -0.453743445530294 -1.25984646501566,4.28795494420042 -0.524571564805566 -1.32058928983137,4.39769287113821 -0.587972124666501 -1.37911752508686,4.51280112760429 -0.643636243958462 -1.43514602710321,4.6327189177438 -0.691292732687594 -1.48840183065055,4.75686201413727 -0.730709413231224 -1.53862547880726,4.88462560409789 -0.761694251483893 -1.58557228700798,5.01538723625813 -0.784096292428219 -1.62901353512224,5.14850985309022 -0.797806395572568 -1.66873758175591,5.28334489458639 -0.802757766672552 -1.70455089534686,5.41923545797803 -0.798926283145887 -1.7362789970312,5.55551949809999 -0.786330611595171 -1.76376731068683,5.691533052808 -0.76503211686609 -1.78688191601273,5.82661347773556 -0.735134563084044 -1.80551020097523,5.96010267463067 -0.69678360812577 -1.8195614104425,6.09135029754434 -0.650166093988798 -1.82896708833443,6.2197169212507 -0.595509136516029 -1.83368141113379,6.34457715646241 -0.533079018910172 -1.83368141113379,6.46532269666444 -0.463179894428748 -1.82896708833443,6.58136528172222 -0.386152304579996 -1.8195614104425,6.69213956382591 -0.302371520038872 -1.80551020097523,6.79710586180808 -0.212245712366048 -1.78688191601273,6.89575279041615 -0.116213965437088 -1.76376731068683,6.9875997517299 -0.014744136269953 -1.7362789970312,7.07219927658622 0.091669424327363 -1.70455089534686,7.14913920460393 0.202508280184287 -1.66873758175591,7.2180446921877 0.317232435536567 -1.62901353512224,7.27858003872839 0.435282965831356 -1.58557228700798,7.33045032210263 0.556084740751457 -1.53862547880726,7.37340283550381 0.679049226192411 -1.48840183065055,7.4072283186042 0.803577351541497 -1.43514602710321,7.43176197705028 0.929062428289537 -1.37911752508686,7.44688428532421 1.05489310575633 -1.32058928983137,7.45252156906016 1.18045634952971 -1.25984646501566,7.44864636397843 1.3051404281076 -1.19718498357639,7.43527754968864 1.42833789319235 -1.13291012595241,7.41248025771019 1.54944853911785 -1.06733503278919,7.38036555415802 1.66788232699113 -1.00077917934903,7.33908989863968 1.78306225930261 -0.933566819059728,7.28885438199984 1.89442719099992 -0.86602540378444,
# 7.2299037466259 2.00143456333018 -0.798483988509152,7.16252519408772 2.10356304713174 -0.731271628219845,7.08704698592039 2.20031508269745 -0.664715774779688,7.00383684436707 2.2912193038355 -0.599140681616468,6.91330016087314 2.37583283431818 -0.534865823992492,6.81587802105987 2.4537434455303 -0.472204342553219,6.71204505579959 2.52457156480557 -0.411461517737508,6.6023071288618 2.5879721246665 -0.352933282482017,6.48719887239572 2.64363624395846 -0.296904780465671,6.36728108225621 2.6912927326876 -0.24364897691833,6.24313798586274 2.73070941323123 -0.19342532876162,6.11537439590212 2.7616942514839 -0.146478520560899,5.98461276374187 2.78409629242822 -0.103037272446642,5.85149014690979 2.79780639557257 -0.0633132258129652,5.71665510541362 2.80275776667256 -0.0274999122220189,5.58076454202198 2.79892628314589 0.00422818946232298,5.44448050190002 2.78633061159517 0.0317165031179519,5.30846694719201 2.76503211686609 0.0548311084438533,5.17338652226444 2.73513456308405 0.0734593934063514,5.03989732536933 2.69678360812577 0.0875106028736174,4.90864970245567 2.6501660939888 0.0969162807655497,4.78028307874931 2.59550913651603 0.101630603564914,4.6554228435376 2.53307901891017 0.101630603564914,4.53467730333557 2.46317989442875 0.0969162807655497,4.41863471827779 2.38615230458 0.0875106028736175,4.3078604361741 2.30237152003888 0.0734593934063515,4.20289413819193 2.21224571236605 0.0548311084438534,4.10424720958385 2.11621396543709 0.031716503117952,4.01240024827011 2.01474413626996 0.00422818946232319,3.92780072341379 1.90833057567264 -0.0274999122220187,3.85086079539608 1.79749171981572 -0.0633132258129645,3.7819553078123 1.68276756446344 -0.103037272446642,3.72141996127162 1.56471703416865 -0.146478520560898,3.66954967789738 1.44391525924855 -0.19342532876162,3.6265971644962 1.32095077380759 -0.243648976918329,3.5927716813958 1.19642264845851 -0.296904780465671,3.56823802294973 1.07093757171047 -0.352933282482016,3.5531157146758 0.945106894243672 -0.411461517737508,3.54747843093985 0.819543650470287 -0.472204342553219,3.55135363602158 0.694859571892403 -0.534865823992493,3.56472245031136 0.571662106807651 -0.599140681616467,3.58751974228981 0.450551460882157 -0.664715774779687,3.61963444584198 0.332117673008871 -0.731271628219845,3.66091010136033 0.216937740697387 -0.798483988509152,3.71114561800017 0.105572809000085 -0.866025403784439)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (3.71114561800017 0.105572809000085 -0.866025403784439,3.77009625337411 -0.001434563330173 -0.933566819059727,3.83747480591229 -0.103563047131737 -1.00077917934903,3.91295301407961 -0.200315082697444 -1.06733503278919,3.99616315563294 -0.2912193038355 -1.13291012595241,4.08669983912687 -0.37583283431818 -1.19718498357639,4.18412197894014 -0.453743445530294 -1.25984646501566,4.28795494420042 -0.524571564805566 -1.32058928983137,4.39769287113821 -0.587972124666501 -1.37911752508686,4.51280112760429 -0.643636243958462 -1.43514602710321,4.6327189177438 -0.691292732687594 -1.48840183065055,4.75686201413727 -0.730709413231224 -1.53862547880726,4.88462560409789 -0.761694251483893 -1.58557228700798,5.01538723625813 -0.784096292428219 -1.62901353512224,5.14850985309022 -0.797806395572568 -1.66873758175591,5.28334489458639 -0.802757766672552 -1.70455089534686,5.41923545797803 -0.798926283145887 -1.7362789970312,5.55551949809999 -0.786330611595171 -1.76376731068683,5.691533052808 -0.76503211686609 -1.78688191601273,5.82661347773556 -0.735134563084044 -1.80551020097523,5.96010267463067 -0.69678360812577 -1.8195614104425,6.09135029754434 -0.650166093988798 -1.82896708833443,6.2197169212507 -0.595509136516029 -1.83368141113379,6.34457715646241 -0.533079018910172 -1.83368141113379,6.46532269666444 -0.463179894428748 -1.82896708833443,6.58136528172222 -0.386152304579996 -1.8195614104425,6.69213956382591 -0.302371520038872 -1.80551020097523,6.79710586180808 -0.212245712366048 -1.78688191601273,6.89575279041615 -0.116213965437088 -1.76376731068683,6.9875997517299 -0.014744136269953 -1.7362789970312,7.07219927658622 0.091669424327363 -1.70455089534686,7.14913920460393 0.202508280184287 -1.66873758175591,7.2180446921877 0.317232435536567 -1.62901353512224,7.27858003872839 0.435282965831356 -1.58557228700798,7.33045032210263 0.556084740751457 -1.53862547880726,7.37340283550381 0.679049226192411 -1.48840183065055,7.4072283186042 0.803577351541497 -1.43514602710321,7.43176197705028 0.929062428289537 -1.37911752508686,7.44688428532421 1.05489310575633 -1.32058928983137,7.45252156906016 1.18045634952971 -1.25984646501566,7.44864636397843 1.3051404281076 -1.19718498357639,7.43527754968864 1.42833789319235 -1.13291012595241,7.41248025771019 1.54944853911785 -1.06733503278919,7.38036555415802 1.66788232699113 -1.00077917934903,7.33908989863968 1.78306225930261 -0.933566819059728,7.28885438199984 1.89442719099992 -0.86602540378444,7.2299037466259 2.00143456333018 -0.798483988509152,7.16252519408772 2.10356304713174 -0.731271628219845,7.08704698592039 2.20031508269745 -0.664715774779688,' + \
        '7.00383684436707 2.2912193038355 -0.599140681616468,6.91330016087314 2.37583283431818 -0.534865823992492,6.81587802105987 2.4537434455303 -0.472204342553219,6.71204505579959 2.52457156480557 -0.411461517737508,6.6023071288618 2.5879721246665 -0.352933282482017,6.48719887239572 2.64363624395846 -0.296904780465671,6.36728108225621 2.6912927326876 -0.24364897691833,6.24313798586274 2.73070941323123 -0.19342532876162,6.11537439590212 2.7616942514839 -0.146478520560899,5.98461276374187 2.78409629242822 -0.103037272446642,5.85149014690979 2.79780639557257 -0.0633132258129652,5.71665510541362 2.80275776667256 -0.0274999122220189,5.58076454202198 2.79892628314589 0.00422818946232298,5.44448050190002 2.78633061159517 0.0317165031179519,5.30846694719201 2.76503211686609 0.0548311084438533,5.17338652226444 2.73513456308405 0.0734593934063514,5.03989732536933 2.69678360812577 0.0875106028736174,4.90864970245567 2.6501660939888 0.0969162807655497,4.78028307874931 2.59550913651603 0.101630603564914,4.6554228435376 2.53307901891017 0.101630603564914,4.53467730333557 2.46317989442875 0.0969162807655497,4.41863471827779 2.38615230458 0.0875106028736175,4.3078604361741 2.30237152003888 0.0734593934063515,4.20289413819193 2.21224571236605 0.0548311084438534,4.10424720958385 2.11621396543709 0.031716503117952,4.01240024827011 2.01474413626996 0.00422818946232319,3.92780072341379 1.90833057567264 -0.0274999122220187,3.85086079539608 1.79749171981572 -0.0633132258129645,3.7819553078123 1.68276756446344 -0.103037272446642,3.72141996127162 1.56471703416865 -0.146478520560898,3.66954967789738 1.44391525924855 -0.19342532876162,3.6265971644962 1.32095077380759 -0.243648976918329,3.5927716813958 1.19642264845851 -0.296904780465671,3.56823802294973 1.07093757171047 -0.352933282482016,3.5531157146758 0.945106894243672 -0.411461517737508,3.54747843093985 0.819543650470287 -0.472204342553219,3.55135363602158 0.694859571892403 -0.534865823992493,3.56472245031136 0.571662106807651 -0.599140681616467,3.58751974228981 0.450551460882157 -0.664715774779687,3.61963444584198 0.332117673008871 -0.731271628219845,3.66091010136033 0.216937740697387 -0.798483988509152,3.71114561800017 0.105572809000085 -0.866025403784439)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):56
#   EntityHandle (String) = 1FE
#   LINESTRING Z (4.25 -0.5 -1.29903810567666,5.5 1.0 -0.866025403784439)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (4.25 -0.5 -1.29903810567666,5.5 1.0 -0.866025403784439)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):57
#   EntityHandle (String) = 1FF
#   LINESTRING Z (5.5 1.0 -0.866025403784439,6.75 2.5 -0.43301270189222)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (5.5 1.0 -0.866025403784439,6.75 2.5 -0.43301270189222)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):58
#   EntityHandle (String) = 200
#   LINESTRING Z (4.75 1.5 -0.43301270189222,5.5 1.0 -0.866025403784439)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (4.75 1.5 -0.43301270189222,5.5 1.0 -0.866025403784439)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):59
#   EntityHandle (String) = 201
#   LINESTRING Z (5.5 1.0 -0.866025403784439,6.25 0.5 -1.29903810567666)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (5.5 1.0 -0.866025403784439,6.25 0.5 -1.29903810567666)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):60
#   EntityHandle (String) = 202
#   LINESTRING Z (4.25 -0.5 -1.29903810567666,4.30536229248331 -0.531224312261799 -1.32818874766841,4.36654473979309 -0.554988653592388 -1.35508777630525,4.43324926719606 -0.571177246509588 -1.37960414222355,4.50515089693985 -0.579711221815978 -1.40161840415293,4.58189933151187 -0.580549002841697 -1.42102331082214,4.6631206602536 -0.573686508001801 -1.43772432347645,4.74841918101576 -0.559157170681335 -1.451640076461,4.83737932797937 -0.53703177635124 -1.46270277362604,4.92956769625067 -0.507418117708657 -1.47085851862302,5.02453515336618 -0.470460469521736 -1.47606757748212,5.12181902742094 -0.426338885737475 -1.47830457219218,5.22094536115948 -0.375268322276984 -1.47755860433986,5.32143122104797 -0.317497589791849 -1.47383330820552,5.42278705007785 -0.253308141483641 -1.46714683305749,5.52451905283833 -0.183012701892219 -1.45753175473055,5.62613160123803 -0.106953743333199 -1.44503491691981,5.7271296491552 -0.025501817407262 -1.42971720296291,5.82702114425268 0.060946250290002 -1.41165323922251,5.92531942520741 0.151969293462501 -1.39093103151418,6.02154559267549 0.247123856993501 -1.36765153635088,6.11523084244164 0.345946357414384 -1.34192816909299,6.2059187493862 0.4479553414367 -1.31388625140011,6.29316749114245 0.552653831544204 -1.28366240067645,6.37655200061077 0.659531747217344 -1.25140386448466,6.45566603684282 0.768068389994292 -1.21726780317044,6.53012416420658 0.877734980261504 -1.18142052419318,6.59956363019005 0.987997233414845 -1.1440366718927,6.66364613269498 1.09831796284048 -1.10529837663958,6.72205946821072 1.20815969703403 -1.06539436751435,6.77451905283833 1.31698729810778 -1.02451905283833,6.82076930875472 1.4242705689286 -0.982871573035841,6.86058490936212 1.52948683618513 -0.940654830442097,6.89377187705662 1.63212349679959 -0.898074500783399,6.92016852826756 1.73168051527848 -0.855338031145687,6.93964626116363 1.8276728598352 -0.81265362931318,6.95211018218804 1.91963286541618 -0.770229249400985,6.95749956837044 2.00711251211804 -0.72827157872355,6.95578816316308 2.08968560789544 -0.686985030834846,6.9469843043601 2.166949864926 -0.646570749646076,6.93113088347664 2.23852885951603 -0.607225629472764,6.90830513678565 2.30407386599898 -0.569141355785437,6.87861826903058 2.36326555569183 -0.53250347133728,6.84221491164701 2.41581555263232 -0.497490472218499,6.79927241813288 2.46146783851767 -0.464272938241304,6.75 2.5 -0.43301270189222,6.69463770751669 2.5312243122618 -0.40386205990047,6.63345526020692 2.55498865359239 -0.376963031263626,
# 6.56675073280394 2.57117724650959 -0.352446665345325,6.49484910306015 2.57971122181598 -0.330432403415945,6.41810066848813 2.5805490028417 -0.311027496746741,6.3368793397464 2.5736865080018 -0.294326484092429,6.25158081898424 2.55915717068134 -0.280410731107883,6.16262067202063 2.53703177635124 -0.269348033942836,6.07043230374933 2.50741811770866 -0.261192288945857,5.97546484663382 2.47046046952174 -0.255983230086761,5.87818097257907 2.42633888573748 -0.253746235376694,5.77905463884053 2.37526832227698 -0.254492203229023,5.67856877895203 2.31749758979185 -0.258217499363356,5.57721294992215 2.25330814148364 -0.264903974511391,5.47548094716167 2.18301270189222 -0.27451905283833,5.37386839876197 2.1069537433332 -0.287015890649068,5.2728703508448 2.02550181740726 -0.302333604605968,5.17297885574732 1.93905374971 -0.320397568346365,5.07468057479259 1.8480307065375 -0.341119776054697,4.97845440732451 1.7528761430065 -0.364399271218,4.88476915755836 1.65405364258562 -0.390122638475884,4.79408125061381 1.5520446585633 -0.418164556168773,4.70683250885755 1.4473461684558 -0.44838840689243,4.62344799938923 1.34046825278266 -0.48064694308422,4.54433396315718 1.23193161000571 -0.514783004398435,4.46987583579342 1.1222650197385 -0.550630283375696,4.40043636980995 1.01200276658516 -0.588014135676182,4.33635386730502 0.901682037159526 -0.626752430929296,4.27794053178928 0.791840302965968 -0.666656440054525,4.22548094716167 0.68301270189222 -0.707531754730549,4.17923069124529 0.575729431071401 -0.749179234533037,4.13941509063789 0.470513163814874 -0.791395977126782,4.10622812294338 0.36787650320041 -0.833976306785479,4.07983147173244 0.268319484721523 -0.876712776423191,4.06035373883638 0.172327140164803 -0.919397178255699,4.04788981781196 0.080367134583816 -0.961821558167894,4.04250043162957 -0.007112512118036 -1.00377922884533,4.04421183683692 -0.089685607895445 -1.04506577673403,4.0530156956399 -0.166949864926001 -1.0854800579228,4.06886911652336 -0.238528859516028 -1.12482517809611,4.09169486321435 -0.304073865998978 -1.16290945178344,4.12138173096942 -0.363265555691829 -1.1995473362316,4.15778508835299 -0.415815552632322 -1.23456033535038,4.20072758186713 -0.461467838517673 -1.26777786932757,4.25 -0.5 -1.29903810567666)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (4.25 -0.5 -1.29903810567666,4.30536229248331 -0.531224312261799 -1.32818874766841,4.36654473979309 -0.554988653592388 -1.35508777630525,4.43324926719606 -0.571177246509588 -1.37960414222355,4.50515089693985 -0.579711221815978 -1.40161840415293,4.58189933151187 -0.580549002841697 -1.42102331082214,4.6631206602536 -0.573686508001801 -1.43772432347645,4.74841918101576 -0.559157170681335 -1.451640076461,4.83737932797937 -0.53703177635124 -1.46270277362604,4.92956769625067 -0.507418117708657 -1.47085851862302,5.02453515336618 -0.470460469521736 -1.47606757748212,5.12181902742094 -0.426338885737475 -1.47830457219218,5.22094536115948 -0.375268322276984 -1.47755860433986,5.32143122104797 -0.317497589791849 -1.47383330820552,5.42278705007785 -0.253308141483641 -1.46714683305749,5.52451905283833 -0.183012701892219 -1.45753175473055,5.62613160123803 -0.106953743333199 -1.44503491691981,5.7271296491552 -0.025501817407262 -1.42971720296291,5.82702114425268 0.060946250290002 -1.41165323922251,5.92531942520741 0.151969293462501 -1.39093103151418,6.02154559267549 0.247123856993501 -1.36765153635088,6.11523084244164 0.345946357414384 -1.34192816909299,6.2059187493862 0.4479553414367 -1.31388625140011,6.29316749114245 0.552653831544204 -1.28366240067645,6.37655200061077 0.659531747217344 -1.25140386448466,6.45566603684282 0.768068389994292 -1.21726780317044,6.53012416420658 0.877734980261504 -1.18142052419318,6.59956363019005 0.987997233414845 -1.1440366718927,6.66364613269498 1.09831796284048 -1.10529837663958,6.72205946821072 1.20815969703403 -1.06539436751435,6.77451905283833 1.31698729810778 -1.02451905283833,6.82076930875472 1.4242705689286 -0.982871573035841,6.86058490936212 1.52948683618513 -0.940654830442097,6.89377187705662 1.63212349679959 -0.898074500783399,6.92016852826756 1.73168051527848 -0.855338031145687,6.93964626116363 1.8276728598352 -0.81265362931318,6.95211018218804 1.91963286541618 -0.770229249400985,6.95749956837044 2.00711251211804 -0.72827157872355,6.95578816316308 2.08968560789544 -0.686985030834846,6.9469843043601 2.166949864926 -0.646570749646076,6.93113088347664 2.23852885951603 -0.607225629472764,6.90830513678565 2.30407386599898 -0.569141355785437,6.87861826903058 2.36326555569183 -0.53250347133728,6.84221491164701 2.41581555263232 -0.497490472218499,6.79927241813288 2.46146783851767 -0.464272938241304,6.75 2.5 -0.43301270189222,6.69463770751669 2.5312243122618 -0.40386205990047,' + \
        '6.63345526020692 2.55498865359239 -0.376963031263626,6.56675073280394 2.57117724650959 -0.352446665345325,6.49484910306015 2.57971122181598 -0.330432403415945,6.41810066848813 2.5805490028417 -0.311027496746741,6.3368793397464 2.5736865080018 -0.294326484092429,6.25158081898424 2.55915717068134 -0.280410731107883,6.16262067202063 2.53703177635124 -0.269348033942836,6.07043230374933 2.50741811770866 -0.261192288945857,5.97546484663382 2.47046046952174 -0.255983230086761,5.87818097257907 2.42633888573748 -0.253746235376694,5.77905463884053 2.37526832227698 -0.254492203229023,5.67856877895203 2.31749758979185 -0.258217499363356,5.57721294992215 2.25330814148364 -0.264903974511391,5.47548094716167 2.18301270189222 -0.27451905283833,5.37386839876197 2.1069537433332 -0.287015890649068,5.2728703508448 2.02550181740726 -0.302333604605968,5.17297885574732 1.93905374971 -0.320397568346365,5.07468057479259 1.8480307065375 -0.341119776054697,4.97845440732451 1.7528761430065 -0.364399271218,4.88476915755836 1.65405364258562 -0.390122638475884,4.79408125061381 1.5520446585633 -0.418164556168773,4.70683250885755 1.4473461684558 -0.44838840689243,4.62344799938923 1.34046825278266 -0.48064694308422,4.54433396315718 1.23193161000571 -0.514783004398435,4.46987583579342 1.1222650197385 -0.550630283375696,4.40043636980995 1.01200276658516 -0.588014135676182,4.33635386730502 0.901682037159526 -0.626752430929296,4.27794053178928 0.791840302965968 -0.666656440054525,4.22548094716167 0.68301270189222 -0.707531754730549,4.17923069124529 0.575729431071401 -0.749179234533037,4.13941509063789 0.470513163814874 -0.791395977126782,4.10622812294338 0.36787650320041 -0.833976306785479,4.07983147173244 0.268319484721523 -0.876712776423191,4.06035373883638 0.172327140164803 -0.919397178255699,4.04788981781196 0.080367134583816 -0.961821558167894,4.04250043162957 -0.007112512118036 -1.00377922884533,4.04421183683692 -0.089685607895445 -1.04506577673403,4.0530156956399 -0.166949864926001 -1.0854800579228,4.06886911652336 -0.238528859516028 -1.12482517809611,4.09169486321435 -0.304073865998978 -1.16290945178344,4.12138173096942 -0.363265555691829 -1.1995473362316,4.15778508835299 -0.415815552632322 -1.23456033535038,4.20072758186713 -0.461467838517673 -1.26777786932757,4.25 -0.5 -1.29903810567666)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):61
#   EntityHandle (String) = 203
#   LINESTRING Z (2.75 0.5 -0.43301270189222,2.75597796157458 0.457022294808167 -0.455760312055479,2.76522760095779 0.414850974295674 -0.478918395607368,2.77770451483146 0.37368848382115 -0.502375781105475,2.79334880719819 0.333732425808347 -0.526019860294666,2.81208537691484 0.295174611146642 -0.549737128688177,2.83382427821898 0.258200138394064 -0.573413730451116,2.85846115251769 0.222986505203176 -0.596936004970611,2.88587772936573 0.189702756235451 -0.620191032488805,2.91594239422828 0.15850867165462 -0.643067176179329,2.94851082030252 0.129554000094665 -0.66545461806501,2.98342666136509 0.102977739784634 -0.687245886204105,3.02052230231928 0.078907471281269 -0.708336370614292,3.05961966383903 0.057458745012683 -0.728624825457685,3.10053105724686 0.038734526573237 -0.748013855076124,3.14306008552201 0.022824702432479 -0.766410381543502,3.18700258611332 0.009805648431019 -0.783726091490617,3.23214761103096 -0.00026013686519 -0.79987786005755,3.27827843951188 -0.007324332192091 -0.814788149938374,3.32517361839783 -0.011353025555964 -0.828385383602543,3.37260802523135 -0.012326877029509 -0.84060428690611,3.42035394896652 -0.010241211594056 -0.851386202443257,3.46818218310621 -0.0051060415822 -0.860679371133851,3.51586312601842 0.003053981386852 -0.868439180695292,3.56316788314946 0.014199684748517 -0.874628379805819,3.60986936584274 0.028277563042523 -0.879217256931186,3.65574338148826 0.045220034768334 -0.882183782956251,3.70056970976957 0.064945766813234 -0.883513716936746,3.74413315984144 0.087360064895672 -0.883200674463593,3.78622460336345 0.112355328149502 -0.881246158311542,3.82664197843012 0.139811565666877 -0.87765955122504,3.86519125957839 0.169596972520106 -0.872458070875934,3.90168738921576 0.201568562497258 -0.865666687209244,3.93595516599773 0.235572854514029 -0.857318002573801,3.96783008588991 0.271446609406727 -0.847452095213172,3.99715913187718 0.30901761356937 -0.836116326868217,4.02380150852887 0.348105505672996 -0.823365115414876,4.04762931789371 0.3885226424985 -0.809259673628662,4.06852817347981 0.430074999726512 -0.793867715329923,4.08639774937224 0.472563103360048 -0.777263130320539,4.10115226185219 0.515782987308575 -0.759525629672545,4.1127208812057 0.559527172536592 -0.740740363071464,4.12104807174486 0.603585663076254 -0.720997510051345,4.12609385840951 0.647746954122649 -0.700391847083764,4.1278340186693 0.691799047372318 -0.679022292599034,4.12626019880507 0.735530468730831 -0.656991432123733,4.12137995401128 0.778731283503886 -0.634405025814187,4.11321671212692 0.821194104198432 -0.61137150074998,4.10180966116915 0.862715086095846 -0.588001430424789,4.08721356120934 0.903094905817856 -0.564407003933257,4.06949848149492 0.942139718187561 -0.540701487402092,4.04874946407869 0.979662086792094 -0.516998680250836,4.02506611557057 1.01548188377972 -0.493412368892526,3.99856212897149 1.04942715457186 -0.470055780496802,3.96936473788492 1.08133494333891 -0.447041039437686,3.93761410572614 1.11105207527721 -0.424478629035376,3.9034626528613 1.13843589193168 -0.402476861176005,3.86707432490649 1.16335493603432 -0.381141356355458,3.8286238056993 1.1856895825708 -0.360574536643342,3.78829567872099 1.20533261304587 -0.340875134001137,3.74628354099508 1.22218973019068 -0.322137716314888,3.70278907371594 1.23618001064108 -0.304452233417734,3.65802107406893 1.247236293414 -0.287903585281599,3.61219445288998 1.25530550231681 -0.27257121445099,3.56552920297626 1.26034890074202 -0.258528724675415,3.51824934300062 1.26234227762414 -0.245843527571206,3.47058184209972 1.26127606366599 -0.234576519008966,3.42275553029828 1.2571553772766 -0.224781786780142,3.375 1.25 -0.21650635094611)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING Z (2.75 0.5 -0.43301270189222,2.75597796157458 0.457022294808167 -0.455760312055479,2.76522760095779 0.414850974295674 -0.478918395607368,2.77770451483146 0.37368848382115 -0.502375781105475,2.79334880719819 0.333732425808347 -0.526019860294666,2.81208537691484 0.295174611146642 -0.549737128688177,2.83382427821898 0.258200138394064 -0.573413730451116,2.85846115251769 0.222986505203176 -0.596936004970611,2.88587772936573 0.189702756235451 -0.620191032488805,2.91594239422828 0.15850867165462 -0.643067176179329,2.94851082030252 0.129554000094665 -0.66545461806501,2.98342666136509 0.102977739784634 -0.687245886204105,3.02052230231928 0.078907471281269 -0.708336370614292,3.05961966383903 0.057458745012683 -0.728624825457685,3.10053105724686 0.038734526573237 -0.748013855076124,3.14306008552201 0.022824702432479 -0.766410381543502,3.18700258611332 0.009805648431019 -0.783726091490617,3.23214761103096 -0.00026013686519 -0.79987786005755,3.27827843951188 -0.007324332192091 -0.814788149938374,3.32517361839783 -0.011353025555964 -0.828385383602543,3.37260802523135 -0.012326877029509 -0.84060428690611,3.42035394896652 -0.010241211594056 -0.851386202443257,3.46818218310621 -0.0051060415822 -0.860679371133851,3.51586312601842 0.003053981386852 -0.868439180695292,3.56316788314946 0.014199684748517 -0.874628379805819,3.60986936584274 0.028277563042523 -0.879217256931186,3.65574338148826 0.045220034768334 -0.882183782956251,3.70056970976957 0.064945766813234 -0.883513716936746,3.74413315984144 0.087360064895672 -0.883200674463593,3.78622460336345 0.112355328149502 -0.881246158311542,3.82664197843012 0.139811565666877 -0.87765955122504,3.86519125957839 0.169596972520106 -0.872458070875934,3.90168738921576 0.201568562497258 -0.865666687209244,3.93595516599773 0.235572854514029 -0.857318002573801,3.96783008588991 0.271446609406727 -0.847452095213172,3.99715913187718 0.30901761356937 -0.836116326868217,4.02380150852887 0.348105505672996 -0.823365115414876,4.04762931789371 0.3885226424985 -0.809259673628662,4.06852817347981 0.430074999726512 -0.793867715329923,4.08639774937224 0.472563103360048 -0.777263130320539,4.10115226185219 0.515782987308575 -0.759525629672545,4.1127208812057 0.559527172536592 -0.740740363071464,4.12104807174486 0.603585663076254 -0.720997510051345,4.12609385840951 0.647746954122649 -0.700391847083764,4.1278340186693 0.691799047372318 -0.679022292599034,4.12626019880507 0.735530468730831 -0.656991432123733,4.12137995401128 0.778731283503886 -0.634405025814187,4.11321671212692 0.821194104198432 -0.61137150074998,4.10180966116915 0.862715086095846 -0.588001430424789,4.08721356120934 0.903094905817856 -0.564407003933257,4.06949848149492 0.942139718187561 -0.540701487402092,4.04874946407869 0.979662086792094 -0.516998680250836,4.02506611557057 1.01548188377972 -0.493412368892526,3.99856212897149 1.04942715457186 -0.470055780496802,3.96936473788492 1.08133494333891 -0.447041039437686,3.93761410572614 1.11105207527721 -0.424478629035376,3.9034626528613 1.13843589193168 -0.402476861176005,3.86707432490649 1.16335493603432 -0.381141356355458,3.8286238056993 1.1856895825708 -0.360574536643342,3.78829567872099 1.20533261304587 -0.340875134001137,3.74628354099508 1.22218973019068 -0.322137716314888,3.70278907371594 1.23618001064108 -0.304452233417734,3.65802107406893 1.247236293414 -0.287903585281599,3.61219445288998 1.25530550231681 -0.27257121445099,3.56552920297626 1.26034890074202 -0.258528724675415,3.51824934300062 1.26234227762414 -0.245843527571206,3.47058184209972 1.26127606366599 -0.234576519008966,3.42275553029828 1.2571553772766 -0.224781786780142,3.375 1.25 -0.21650635094611)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):62
#   EntityHandle (String) = 204
#   POLYGON Z ((2.125 -0.25 -0.649519052838329,2.875 -0.75 -1.08253175473055,3.5 0.0 -0.866025403784439,2.75 0.5 -0.43301270189222,2.125 -0.25 -0.649519052838329))
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON Z ((2.125 -0.25 -0.649519052838329,2.875 -0.75 -1.08253175473055,3.5 0.0 -0.866025403784439,2.75 0.5 -0.43301270189222,2.125 -0.25 -0.649519052838329))'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):63
#   EntityHandle (String) = 205
#   POLYGON ((4.875 0.25,5.5 1.0,4.75 1.5,4.125 0.75,4.875 0.25))
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON ((4.875 0.25,5.5 1.0,4.75 1.5,4.125 0.75,4.875 0.25))'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):64
#   EntityHandle (String) = 207
#   POLYGON ((11 2,11.625 2.75,12.375 2.25,11.75 1.5,11 2))
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON ((11 2,11.625 2.75,12.375 2.25,11.75 1.5,11 2))'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):65
#   EntityHandle (String) = 208
#   LINESTRING (2.75 0.5,2.96153846153846 0.538461538461539,3.17307692307692 0.576923076923077,3.38461538461539 0.615384615384616,3.59615384615385 0.653846153846154,3.80769230769231 0.692307692307693,4.01923076923077 0.730769230769231,4.23076923076923 0.76923076923077,4.44230769230769 0.807692307692308,4.65384615384616 0.846153846153847,4.86538461538462 0.884615384615385,5.07692307692308 0.923076923076924,5.28846153846154 0.961538461538462,5.5 1.0,5.71153846153846 1.03846153846154,5.92307692307693 1.07692307692308,6.13461538461539 1.11538461538462,6.34615384615385 1.15384615384615,6.55769230769231 1.19230769230769,6.76923076923077 1.23076923076923,6.98076923076923 1.26923076923077,7.19230769230769 1.30769230769231,7.40384615384616 1.34615384615385,7.61538461538462 1.38461538461539,7.82692307692308 1.42307692307692,8.03846153846154 1.46153846153846,8.25 1.5,8.46153846153846 1.53846153846154,8.67307692307693 1.57692307692308,8.88461538461539 1.61538461538462,9.09615384615385 1.65384615384615,9.30769230769231 1.69230769230769,9.51923076923077 1.73076923076923,9.73076923076923 1.76923076923077,9.94230769230769 1.80769230769231,10.1538461538462 1.84615384615385,10.3653846153846 1.88461538461539,10.5769230769231 1.92307692307692,10.7884615384615 1.96153846153846,11.0 2.0)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (2.75 0.5,2.96153846153846 0.538461538461539,3.17307692307692 0.576923076923077,3.38461538461539 0.615384615384616,3.59615384615385 0.653846153846154,3.80769230769231 0.692307692307693,4.01923076923077 0.730769230769231,4.23076923076923 0.76923076923077,4.44230769230769 0.807692307692308,4.65384615384616 0.846153846153847,4.86538461538462 0.884615384615385,5.07692307692308 0.923076923076924,5.28846153846154 0.961538461538462,5.5 1.0,5.71153846153846 1.03846153846154,5.92307692307693 1.07692307692308,6.13461538461539 1.11538461538462,6.34615384615385 1.15384615384615,6.55769230769231 1.19230769230769,6.76923076923077 1.23076923076923,6.98076923076923 1.26923076923077,7.19230769230769 1.30769230769231,7.40384615384616 1.34615384615385,7.61538461538462 1.38461538461539,7.82692307692308 1.42307692307692,8.03846153846154 1.46153846153846,8.25 1.5,8.46153846153846 1.53846153846154,8.67307692307693 1.57692307692308,8.88461538461539 1.61538461538462,9.09615384615385 1.65384615384615,9.30769230769231 1.69230769230769,9.51923076923077 1.73076923076923,9.73076923076923 1.76923076923077,9.94230769230769 1.80769230769231,10.1538461538462 1.84615384615385,10.3653846153846 1.88461538461539,10.5769230769231 1.92307692307692,10.7884615384615 1.96153846153846,11.0 2.0)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):66
#   EntityHandle (String) = 209
#   LINESTRING (5.75 5.5,5.5082446180819 5.22760624757667,5.26788002157825 4.95546536522868,5.03029699590351 4.68383022303141,4.79688632647213 4.4129536910602,4.56903879869856 4.14308863939042,4.34814519799727 3.87448793809741,4.1355963097827 3.60740445725653,3.93278291946931 3.34209106694314,3.74109581247155 3.07880063723259,3.56192577420388 2.81778603820024,3.39666359008075 2.55930013992144,3.24670004551661 2.30359581247155,3.11342592592593 2.05092592592593,2.99823201672314 1.80154335035992,2.90250910332271 1.55570095584889,2.8276479711391 1.31365161246818,2.77503940558674 1.07564819029316,2.74607419208011 0.84194355939918,2.74214311603365 0.612790589861594,2.76457901346955 0.38843161550262,2.8133818843878 0.168866636322256,2.88721889276623 -0.046146681501713,2.98469925319038 -0.256861208044643,3.10443218024579 -0.463529813381887,3.24502688851802 -0.666405367588803,3.4050925925926 -0.865740740740744,3.58323850705508 -1.06178880291307,3.77807384649101 -1.25480242418113,3.98820782548594 -1.44503447462028,4.21224965862541 -1.63273782430588,4.44880856049496 -1.81816534331328,4.69649374568015 -2.00156990171784,4.95391442876651 -2.18320436959491,5.2196798243396 -2.36332161701985,5.49239914698496 -2.54217451406801,5.77068161128813 -2.72001593081475,6.05313643183467 -2.89709873733543,6.33837282321012 -3.0736758037054,6.625 -3.25)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (5.75 5.5,5.5082446180819 5.22760624757667,5.26788002157825 4.95546536522868,5.03029699590351 4.68383022303141,4.79688632647213 4.4129536910602,4.56903879869856 4.14308863939042,4.34814519799727 3.87448793809741,4.1355963097827 3.60740445725653,3.93278291946931 3.34209106694314,3.74109581247155 3.07880063723259,3.56192577420388 2.81778603820024,3.39666359008075 2.55930013992144,3.24670004551661 2.30359581247155,3.11342592592593 2.05092592592593,2.99823201672314 1.80154335035992,2.90250910332271 1.55570095584889,2.8276479711391 1.31365161246818,2.77503940558674 1.07564819029316,2.74607419208011 0.84194355939918,2.74214311603365 0.612790589861594,2.76457901346955 0.38843161550262,2.8133818843878 0.168866636322256,2.88721889276623 -0.046146681501713,2.98469925319038 -0.256861208044643,3.10443218024579 -0.463529813381887,3.24502688851802 -0.666405367588803,3.4050925925926 -0.865740740740744,3.58323850705508 -1.06178880291307,3.77807384649101 -1.25480242418113,3.98820782548594 -1.44503447462028,4.21224965862541 -1.63273782430588,4.44880856049496 -1.81816534331328,4.69649374568015 -2.00156990171784,4.95391442876651 -2.18320436959491,5.2196798243396 -2.36332161701985,5.49239914698496 -2.54217451406801,5.77068161128813 -2.72001593081475,6.05313643183467 -2.89709873733543,6.33837282321012 -3.0736758037054,6.625 -3.25)'):
        feat.DumpReadable()
        return 'fail'

# OGRFeature(entities):67
#   EntityHandle (String) = 20A
#   POINT Z (9.625 1.75 -1.51554445662277)
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POINT Z (9.625 1.75 -1.51554445662277)'):
        feat.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Polyface Mesh tests

def ogr_dxf_33():
    ds = ogr.Open('data/polyface.dxf')
    layer = ds.GetLayer(0)
    feat = layer.GetNextFeature()
    if feat.Layer != '0':
        return 'fail #1'

    geom = feat.GetGeometryRef()
    if geom.GetGeometryType() != ogr.wkbPolyhedralSurfaceZ:
        gdaltest.post_reason( 'did not get expected geometry type; got %s instead of wkbPolyhedralSurface', geom.GetGeometryType() )
        return 'fail #2'

    wkt_string = geom.ExportToIsoWkt()
    wkt_string_expected = 'POLYHEDRALSURFACE Z (((0 0 0,1 0 0,1 1 0,0 1 0,0 0 0)),((0 0 0,1 0 0,1 0 1,0 0 1,0 0 0)),((1 0 0,1 1 0,1 1 1,1 0 1,1 0 0)),((1 1 0,1 1 1,0 1 1,0 1 0,1 1 0)),((0 0 0,0 1 0,0 1 1,0 0 1,0 0 0)),((0 0 1,1 0 1,1 1 1,0 1 1,0 0 1)))'
    if wkt_string != wkt_string_expected:
        gdaltest.post_reason( 'did not get expected WKT of extracted geometry')
        return 'fail'

    faces = geom.GetGeometryCount()
    if faces != 6:
        gdaltest.post_reason( 'did not get expected number of faces, got %d instead of %d', faces, 6)
        return 'fail'

    return 'success'

###############################################################################
# Writing Triangle geometry and checking if it is written properly

def ogr_dxf_34():
    ds = ogr.GetDriverByName('DXF').CreateDataSource('tmp/triangle_test.dxf' )
    lyr = ds.CreateLayer( 'entities' )
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'TRIANGLE ((0 0,0 1,1 0,0 0))' ) )

    lyr.CreateFeature( dst_feat )
    dst_feat = None

    lyr = None
    ds = None

    # Read back.
    ds = ogr.Open('tmp/triangle_test.dxf')
    lyr = ds.GetLayer(0)

    # Check first feature
    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    expected_wkt = 'POLYGON ((0 0,0 1,1 0,0 0))'
    received_wkt = geom.ExportToWkt()

    if expected_wkt != received_wkt:
        gdaltest.post_reason( 'did not get expected geometry back')
        return 'fail'
    ds = None

    gdal.Unlink('tmp/triangle_test.dxf' )

    return 'success'

###############################################################################
# Test reading hatch with elliptical harts

def ogr_dxf_35():

    ds = ogr.Open('data/elliptical-arc-hatch-min.dxf')
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    expected_wkt = "POLYGON Z ((10.0 5.0 0,10.0121275732481 0.823574944937595 0," + \
    "10.0484514617793 -3.3325901498166 0," + \
    "10.1087954573461 -7.44833360561541 0," + \
    "10.1928668294578 -11.5036898303666 0," + \
    "10.3002577454253 -15.478986172205 0," + \
    "10.4304472487686 -19.3549383521031 0," + \
    "10.5828037863926 -23.1127440124738 0," + \
    "10.7565882722693 -26.7341739279578 0," + \
    "10.950957672766 -30.2016604359299 0," + \
    "11.164969096226 -33.4983826577451 0," + \
    "11.3975843669637 -36.6083480973141 0," + \
    "11.6476750614854 -39.5164702211696 0," + \
    "11.9140279825044 -42.2086416436832 0," + \
    "12.1953510441969 -44.6718025624057 0," + \
    "12.4902795401481 -46.8940041115515 0," + \
    "12.797382763583 -48.8644663262969 0," + \
    "13.1151709477668 -50.5736304367052 0," + \
    "13.442102492907 -52.0132052375995 0," + \
    "13.7765914444993 -53.1762073094422 0," + \
    "14.1170151868394 -54.0569948951078 0," + \
    "14.4617223143788 -54.6512952682117 0," + \
    "14.8090406427424 -54.9562254602315 0," + \
    "15.1572853205429 -54.9703062458714 0," + \
    "15.5047670026452 -54.6934693188278 0," + \
    "15.8498000452284 -54.1270576231449 0," + \
    "16.1907106828936 -53.2738188385539 0," + \
    "16.5258451481492 -52.137892051398 0," + \
    "16.853577693885 -50.7247876758036 0," + \
    "17.1723184799194 -49.0413607224995 0," + \
    "17.4805212853603 -47.0957775449594 0," + \
    "17.7766910093686 -44.8974762241817 0," + \
    "18.0593909239355 -42.457120784282 0," + \
    "18.3272496434925 -39.7865494609998 0," + \
    "18.578967777543 -36.8987172740701 0," + \
    "18.8133242340436 -33.8076331820431 0," + \
    "19.0291821429573 -30.5282921244156 0," + \
    "19.2254943712436 -27.0766022807403 0," + \
    "19.4013086025311 -23.4693078995808 0," + \
    "19.5557719568327 -19.7239080716712 0," + \
    "19.6881351278911 -15.8585718413141 0," + \
    "19.7977560180852 -11.8920500678142 0," + \
    "19.8841028532649 -7.84358446451107 0," + \
    "19.9467567624029 -3.73281425666327 0," + \
    "19.9854138095503 0.420319089008591 0," + \
    "19.9998864682387 4.59566860096071 0," + \
    "19.9901045311767 8.77297953637629 0," + \
    "19.9561154508277 12.9319876375154 0," + \
    "19.8980841092162 17.0525174342237 0," + \
    "19.8162920180808 21.1145801157289 0," + \
    "19.7111359532519 25.0984704969476 0," + \
    "19.5831260298811 28.9848626089156 0," + \
    "19.4328832278572 32.7549034496293 0," + \
    "19.2611363794148 36.390304440511 0," + \
    "19.0687186335478 39.8734301448397 0," + \
    "18.856563414379 43.1873838177704 0," + \
    "18.6256998930927 46.3160893729396 0," + \
    "18.3772479953948 49.2443693680303 0," + \
    "18.1124129687203 51.9580186309899 0," + \
    "17.8324795355432 54.4438731697361 0," + \
    "17.5388056611497 56.6898740310677 0," + \
    "17.2328159661089 58.6851257990001 0," + \
    "16.9159948153966 60.4199494487478 0," + \
    "16.5898791176976 61.8859292999569 0," + \
    "16.2560508698165 63.075953841417 0," + \
    "15.9161294823645 63.9842502292107 0," + \
    "15.5717639239516 64.6064122909483 0," + \
    "15.2246247219914 64.9394219002396 0," + \
    "14.8763958589235 64.9816636177129 0," + \
    "14.5287666031655 64.7329325275591 0," + \
    "14.1834233144223 64.1944352315841 0," + \
    "13.8420412631059 63.3687839959484 0," + \
    "13.5062765035495 62.2599840789869 0," + \
    "13.1777578404393 60.8734143015838 0," + \
    "12.8580789274355 59.2158009543548 0," + \
    "12.5487905363114 57.2951851682141 0," + \
    "12.2513930341143 55.1208839066128 0," + \
    "11.9673291048407 52.7034447686764 0," + \
    "11.6979767509346 50.0545948224921 0," + \
    "11.4446426085565 47.1871837167582 0," + \
    "11.2085556090535 44.1151213467616 0," + \
    "10.9908610173775 40.8533103770683 0," + \
    "10.792614876371 37.4175739482608 0," + \
    "10.6147788838724 33.8245789184198 0," + \
    "10.4582157274918 30.0917550117071 0," + \
    "10.323684899688 26.2372102662631 0," + \
    "10.2118390134483 22.2796431915832 0," + \
    "10.1232206364439 18.2382520615003 0," + \
    "10.0582596590181 14.1326417827963 0," + \
    "10.0172712087756 9.98272879122447 0," + \
    "10.0172712087756 9.9827287912244 0," + \
    "13.3027626235603 8.26630944469236 0," + \
    "10.0 5.0 0," + \
    "10.0 5.0 0))"""
    if ogrtest.check_feature_geometry(feat, expected_wkt) != 0:
        return 'fail'

    return 'success'

###############################################################################
# Test reading files with only INSERT content (#7006)

def ogr_dxf_36():

    gdal.SetConfigOption('DXF_MERGE_BLOCK_GEOMETRIES', 'FALSE')
    ds = ogr.Open('data/insert_only.dxf')
    gdal.SetConfigOption('DXF_MERGE_BLOCK_GEOMETRIES', None)
    lyr = ds.GetLayer(0)
    if lyr.GetFeatureCount() != 5:
        return 'fail'

    return 'success'

###############################################################################
# Create a blocks layer only

def ogr_dxf_37():

    ds = ogr.GetDriverByName('DXF').CreateDataSource('/vsimem/ogr_dxf_37.dxf')

    lyr = ds.CreateLayer( 'blocks' )

    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'POINT (1 2)' ) )
    lyr.CreateFeature( dst_feat )
    dst_feat = None

    lyr = None
    ds = None

    # Read back.
    gdal.SetConfigOption('DXF_INLINE_BLOCKS', 'FALSE')
    ds = ogr.Open('/vsimem/ogr_dxf_37.dxf')
    gdal.SetConfigOption('DXF_INLINE_BLOCKS', None)
    lyr = ds.GetLayerByName('blocks')

    # Check first feature
    feat = lyr.GetNextFeature()
    if feat is None:
        return 'fail'
    ds = None

    gdal.Unlink( '/vsimem/ogr_dxf_37.dxf')

    return 'success'


###############################################################################
# cleanup

def ogr_dxf_cleanup():
    gdaltest.dxf_layer = None
    gdaltest.dxf_ds = None

    return 'success'

###############################################################################
#

gdaltest_list = [
    ogr_dxf_1,
    ogr_dxf_2,
    ogr_dxf_3,
    ogr_dxf_4,
    ogr_dxf_5,
    ogr_dxf_6,
    ogr_dxf_7,
    ogr_dxf_8,
    ogr_dxf_9,
    ogr_dxf_10,
    ogr_dxf_11,
    ogr_dxf_12,
    ogr_dxf_13,
    ogr_dxf_14,
    ogr_dxf_15,
    ogr_dxf_16,
    ogr_dxf_17,
    ogr_dxf_18,
    ogr_dxf_19,
    ogr_dxf_20,
    ogr_dxf_21,
    ogr_dxf_22,
    ogr_dxf_23,
    ogr_dxf_24,
    ogr_dxf_25,
    ogr_dxf_26,
    ogr_dxf_27,
    ogr_dxf_28,
    ogr_dxf_29,
    ogr_dxf_30,
    ogr_dxf_31,
    ogr_dxf_32,
    ogr_dxf_33,
    ogr_dxf_34,
    ogr_dxf_35,
    ogr_dxf_36,
    ogr_dxf_37,
    ogr_dxf_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_dxf' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
