#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR DXF driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
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
from sys import version_info

sys.path.append( '../pymod' )

import ogrtest
import gdaltest
from osgeo import gdal, ogr

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
    if defn.GetFieldCount() != 8:
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
        
    feat.Destroy()

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

    feat.Destroy()
    
    return 'success'

###############################################################################
# Third feature: point.

def ogr_dxf_4():
    
    feat = gdaltest.dxf_layer.GetNextFeature()

    if ogrtest.check_feature_geometry( feat, 'POINT (83.5 160.0 0)' ):
        return 'fail'

    feat.Destroy()

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
        
    feat.Destroy()

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
        
    if feat.GetStyleString() != 'LABEL(f:"Arial",t:"Test",a:30,s:5g,p:7,c:#000000)':
        print(feat.GetStyleString())
        gdaltest.post_reason( 'got wrong style string' )
        return 'fail'

    feat.Destroy()

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

    feat.Destroy()
    
    return 'success'

###############################################################################
# Dimension 

def ogr_dxf_8():

    # Skip boring line.
    feat = gdaltest.dxf_layer.GetNextFeature()
    feat.Destroy()

    # Dimension lines
    feat = gdaltest.dxf_layer.GetNextFeature()
    geom = feat.GetGeometryRef()

    if geom.GetGeometryType() != ogr.wkbMultiLineString:
        gdaltest.post_reason( 'did not get expected geometry type.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'MULTILINESTRING ((63.862871944482457 149.209935992088333,24.341960668550669 111.934531038652722),(72.754404848874373 139.782768575383642,63.005199575228545 150.119275371538265),(33.233493572942614 102.507363621948002,23.484288299296757 112.843870418102654),(63.862871944482457 149.209935992088333,61.758302395831294 147.797704380063834),(63.862871944482457 149.209935992088333,62.330083975333906 147.191478127097213),(24.341960668550669 111.934531038652722,25.874748637699227 113.952988903643856),(24.341960668550669 111.934531038652722,25.874748637699227 113.952988903643856))' ):
        print geom.ExportToWkt()
        return 'fail'

    feat.Destroy()

    # Dimension text
    feat = gdaltest.dxf_layer.GetNextFeature()
    
    geom = feat.GetGeometryRef()

    if ogrtest.check_feature_geometry( feat, 'POINT (42.815907752635709 131.936242584545397)' ):
        return 'fail'

    expected_style = 'LABEL(f:"Arial",t:"54.3264",p:5,a:43.3,s:2.5g)'
    if feat.GetStyleString() != expected_style:
        gdaltest.post_reason( 'Got unexpected style string:\n%s\ninstead of:\n%s.' % (feat.GetStyleString(),expected_style) )
        return 'fail'

    feat.Destroy()
    
    return 'success'

###############################################################################
# BLOCK (inlined)

def ogr_dxf_9():

    # Skip two dimensions each with a line and text.
    for x in range(4):
        feat = gdaltest.dxf_layer.GetNextFeature()
        feat.Destroy()

    # block (merged geometries)
    feat = gdaltest.dxf_layer.GetNextFeature()
    geom = feat.GetGeometryRef()

    if geom.GetGeometryType() != ogr.wkbGeometryCollection25D:
        gdaltest.post_reason( 'did not get expected geometry type.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'GEOMETRYCOLLECTION (LINESTRING (79.069506278985116 121.003652476272777 0,79.716898725419625 118.892590150942851 0),LINESTRING (79.716898725419625 118.892590150942851 0,78.140638855839953 120.440702522851453 0),LINESTRING (78.140638855839953 120.440702522851453 0,80.139111190485622 120.328112532167196 0),LINESTRING (80.139111190485622 120.328112532167196 0,78.619146316248077 118.920737648613908 0),LINESTRING (78.619146316248077 118.920737648613908 0,79.041358781314059 120.975504978601705 0))' ):
        return 'fail'

    feat.Destroy()

    # First of two MTEXTs
    feat = gdaltest.dxf_layer.GetNextFeature()
    if feat.GetField( 'Text' ) != gdaltest.sample_text:
        gdaltest.post_reason( 'Did not get expected first mtext.' )
        return 'fail'

    expected_style = 'LABEL(f:"Arial",t:"'+gdaltest.sample_style+'",a:45,s:0.5g,p:5,c:#000000)'
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

    feat.Destroy()

    return 'success'

###############################################################################
# LWPOLYLINE in an Object Coordinate System.

def ogr_dxf_10():

    ocs_ds = ogr.Open('data/LWPOLYLINE-OCS.dxf')
    ocs_lyr = ocs_ds.GetLayer(0)
    
    # Skip boring line.
    feat = ocs_lyr.GetNextFeature()
    feat.Destroy()

    # LWPOLYLINE in OCS
    feat = ocs_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()

    if geom.GetGeometryType() != ogr.wkbLineString25D:
        print(geom.GetGeometryType())
        gdaltest.post_reason( 'did not get expected geometry type.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'LINESTRING (600325.567999998573214 3153021.253000000491738 562.760000000052969,600255.215999998385087 3151973.98600000096485 536.950000000069849,597873.927999997511506 3152247.628000000491738 602.705000000089058)' ):
        return 'fail'

    feat.Destroy()

    ocs_lyr = None
    ocs_ds.Destroy()
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

    feat.Destroy()

    # Check second point.
    feat = eo_lyr.GetNextFeature()

    if ogrtest.check_feature_geometry( feat,
                                       'POINT (672750.0 242000.0 558.974)' ):
        return 'fail'

    feat.Destroy()

    eo_lyr = None
    eo_ds.Destroy()
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
    dst_feat.Destroy()
                                  
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'POLYGON((0 0,100 0,100 100,0 0))' ) )
    lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

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

    if feat.GetGeometryRef().GetGeometryType() == ogr.wkbLineString25D:
        gdaltest.post_reason( 'not linestring 2D' )
        return 'fail'
        
    feat.Destroy()

    # Check second point.
    feat = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry( feat,
                                       'POLYGON((0 0,100 0,100 100,0 0))' ):
        print(feat.GetGeometryRef().ExportToWkt())
        return 'fail'

    if feat.GetGeometryRef().GetGeometryType() == ogr.wkbPolygon25D:
        gdaltest.post_reason( 'not keeping polygon 2D' )
        return 'fail'
        
    feat.Destroy()

    lyr = None
    ds.Destroy()
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

    # Check for specific number of points from tesselated arc(s).
    # Note that this number depends on the tesselation algorithm and
    # possibly the default global arc_stepsize variable; therefore it is
    # not guaranteed to remain constant even if the input DXF file is constant.
    # If you retain this test, you may need to update the point count if
    # changes are made to the aforementioned items. Ideally, one would test 
    # only that more points are returned than in the original polyline, and 
    # that the points lie along (or reasonably close to) said path.

    if geom.GetPointCount() != 146:
        gdaltest.post_reason( 'did not get expected number of points, got %d' % (rgeom.GetPointCount()) )
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
        
    feat.Destroy()

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
        
    feat.Destroy()

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
    dst_feat.Destroy()
                                  
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'POLYGON((0 0,100 0,100 100,0 0))' ) )
    lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

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
        
    feat.Destroy()

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
        
    feat.Destroy()

    lyr = None
    ds.Destroy()
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

    expected_style = 'LABEL(f:"Arial",t:"'+gdaltest.sample_style+'",a:45,s:0.5g,p:5,c:#000000)'
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

    if ogrtest.check_feature_geometry( feat, 'GEOMETRYCOLLECTION (LINESTRING (-0.028147497671066 1.041457413829428 0,0.619244948763444 -1.069604911500494 0),LINESTRING (0.619244948763444 -1.069604911500494 0,-0.957014920816232 0.478507460408116 0),LINESTRING (-0.957014920816232 0.478507460408116 0,1.041457413829428 0.365917469723853 0),LINESTRING (1.041457413829428 0.365917469723853 0,-0.478507460408116 -1.041457413829428 0),LINESTRING (-0.478507460408116 -1.041457413829428 0,-0.056294995342131 1.013309916158363 0))' ):
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
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'GEOMETRYCOLLECTION( LINESTRING(0 0,1 1),LINESTRING(1 0,0 1))' ) )
    dst_feat.SetField( 'BlockName', 'XMark' )
    blyr.CreateFeature( dst_feat )
    dst_feat.Destroy()
    

    # Write a block reference feature.
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'POINT(200 100)' ))
    dst_feat.SetField( 'Layer', 'abc' )
    dst_feat.SetField( 'BlockName', 'XMark' )
    lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()
                                  
    # Write a block reference feature for a non-existant block.
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'POINT(300 50)' ))
    dst_feat.SetField( 'Layer', 'abc' )
    dst_feat.SetField( 'BlockName', 'DoesNotExist' )
    lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()
                                  
    # Write a block reference feature for a template defined block
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'POINT(250 200)' ))
    dst_feat.SetField( 'Layer', 'abc' )
    dst_feat.SetField( 'BlockName', 'STAR' )
    lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()
                                  
    # Write a block reference feature with scaling and rotation
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'POINT(300 100)' ))
    dst_feat.SetField( 'BlockName', 'XMark' )
    dst_feat.SetField( 'BlockAngle', '30' )
    dst_feat.SetFieldDoubleList(lyr.GetLayerDefn().GetFieldIndex('BlockScale'), 
                                [4.0,5.0,6.0] )
    lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    ds = None

    # Reopen and check contents.

    ds = ogr.Open('tmp/dxf_17.dxf')

    lyr = ds.GetLayer(0)

    # Check first feature.
    feat = lyr.GetNextFeature()
    if feat.GetField('SubClasses') != 'AcDbEntity:AcDbBlockReference':
        gdaltest.post_reason( 'Got wrong subclasses for feature 1.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'GEOMETRYCOLLECTION (LINESTRING (200 100,201 101),LINESTRING (201 100,200 101))' ):
        print( 'Feature 1' )
        return 'fail'

    # Check second feature.
    feat = lyr.GetNextFeature()
    if feat.GetField('SubClasses') != 'AcDbEntity:AcDbPoint':
        gdaltest.post_reason( 'Got wrong subclasses for feature 2.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'POINT (300 50)' ):
        print( 'Feature 2' )
        return 'fail'

    # Check third feature.
    feat = lyr.GetNextFeature()
    if feat.GetField('SubClasses') != 'AcDbEntity:AcDbBlockReference':
        gdaltest.post_reason( 'Got wrong subclasses for feature 3.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'GEOMETRYCOLLECTION (LINESTRING (249.971852502328943 201.04145741382942 0,250.619244948763452 198.930395088499495 0),LINESTRING (250.619244948763452 198.930395088499495 0,249.042985079183779 200.47850746040811 0),LINESTRING (249.042985079183779 200.47850746040811 0,251.04145741382942 200.365917469723854 0),LINESTRING (251.04145741382942 200.365917469723854 0,249.52149253959189 198.95854258617058 0),LINESTRING (249.52149253959189 198.95854258617058 0,249.943705004657858 201.013309916158363 0))' ):
        print( 'Feature 3' )
        return 'fail'

    # Check fourth feature (scaled and rotated)
    feat = lyr.GetNextFeature()
    if feat.GetField('SubClasses') != 'AcDbEntity:AcDbBlockReference':
        gdaltest.post_reason( 'Got wrong subclasses for feature 4.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'GEOMETRYCOLLECTION (LINESTRING (300 100,300.964101615137736 106.330127018922198),LINESTRING (303.464101615137736 102.0,297.5 104.330127018922198))' ):
        print( 'Feature 4' )
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
    dst_feat.Destroy()
    
    # Write a feature with a named linetype but that isn't predefined in the header.
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt('LINESTRING(5 5,30 30)') )
    dst_feat.SetField( 'Linetype', 'DOTTED' )
    dst_feat.SetStyleString( 'PEN(c:#ffff00,w:2g,p:"0.0g 4.0g")' )
    lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()
    
    # Write a feature without a linetype name - it will be created.
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt('LINESTRING(5 5,40 30)') )
    dst_feat.SetStyleString( 'PEN(c:#ffff00,w:2g,p:"3.0g 4.0g")' )
    lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()
    
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
    dst_feat.Destroy()
                                  
    ds = None

    # Reopen and check contents.

    ds = ogr.Open('tmp/dxf_19.dxf')

    lyr = ds.GetLayer(0)

    # Check first feature.
    feat = lyr.GetNextFeature()
    if feat.GetField('SubClasses') != 'AcDbEntity:AcDbBlockReference':
        gdaltest.post_reason( 'Got wrong subclasses for feature 1.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'GEOMETRYCOLLECTION (LINESTRING (249.971852502328943 201.04145741382942 0,250.619244948763452 198.930395088499495 0),LINESTRING (250.619244948763452 198.930395088499495 0,249.042985079183779 200.47850746040811 0),LINESTRING (249.042985079183779 200.47850746040811 0,251.04145741382942 200.365917469723854 0),LINESTRING (251.04145741382942 200.365917469723854 0,249.52149253959189 198.95854258617058 0),LINESTRING (249.52149253959189 198.95854258617058 0,249.943705004657858 201.013309916158363 0))' ):
        return 'fail'

    # Cleanup
    
    lyr = None
    ds = None
    
    os.unlink( 'tmp/dxf_19.dxf' )
        
    return 'success'

###############################################################################
# cleanup

def ogr_dxf_cleanup():
    gdaltest.dxf_layer = None
    gdaltest.dxf_ds.Destroy()
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
    ogr_dxf_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_dxf' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

