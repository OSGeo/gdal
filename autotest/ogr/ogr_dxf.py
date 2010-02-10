#!/usr/bin/env python
###############################################################################
# $Id: ogr_sdts.py 14973 2008-07-19 13:06:14Z rouault $
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

sys.path.append( '../pymod' )

import ogrtest
import gdaltest
import ogr

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
    if fc != 13:
        gdaltest.post_reason( 'did not get expected feature count, got %d' % fc)
        return 'fail'

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

    feat.Destroy()

    return 'success'

###############################################################################
# Fourth feature: MTEXT

def ogr_dxf_6():
    
    feat = gdaltest.dxf_layer.GetNextFeature()

    if ogrtest.check_feature_geometry( feat, 'POINT (84 126 0)' ):
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
# LWPOLYLINE in an Object Coordinate System.

def ogr_dxf_9():

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

def ogr_dxf_10():

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
    ogr_dxf_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_dxf' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

