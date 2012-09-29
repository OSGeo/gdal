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

    if ogrtest.check_feature_geometry( feat, 'MULTILINESTRING ((63.862871944482457 149.209935992088333,24.341960668550669 111.934531038652722),(72.754404848874373 139.782768575383642,62.744609795879391 150.395563330366286),(33.233493572942614 102.507363621948002,23.2236985199476 113.120158376930675),(63.862871944482457 149.209935992088333,59.187727781045531 147.04077688455709),(63.862871944482457 149.209935992088333,61.424252078251662 144.669522208001183),(24.341960668550669 111.934531038652722,26.78058053478146 116.474944822739886),(24.341960668550669 111.934531038652722,29.017104831987599 114.103690146183979))' ):
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
    ogr_dxf_20,
    ogr_dxf_21,
    ogr_dxf_22,
    ogr_dxf_23,
    ogr_dxf_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_dxf' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

