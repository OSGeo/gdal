###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Support functions for OGR tests.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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

try:
    from osgeo import ogr
except ImportError:
    import ogr as ogr
import gdaltest

geos_flag = None


###############################################################################
def check_features_against_list( layer, field_name, value_list ):

    field_index = layer.GetLayerDefn().GetFieldIndex( field_name )
    if field_index < 0:
        gdaltest.post_reason( 'did not find required field ' + field_name )
        return 0
    
    for i in range(len(value_list)):
        feat = layer.GetNextFeature()
        if feat is None:
            
            gdaltest.post_reason( 'Got only %d features, not the expected %d features.' % (i, len(value_list)) )
            return 0

        if isinstance(value_list[i],type('str')):
            isok = (feat.GetFieldAsString( field_index ) != value_list[i])
        else:
            isok = (feat.GetField( field_index ) != value_list[i])
        if isok:
            gdaltest.post_reason( 'field %s feature %d did not match expected value %s, got %s.' % (field_name, i, str(value_list[i]), str(feat.GetField(field_index)) ) )
            feat.Destroy()
            return 0

        feat.Destroy()

    feat = layer.GetNextFeature()
    if feat is not None:
        feat.Destroy()
        gdaltest.post_reason( 'got more features than expected' )
        return 0

    return 1

###############################################################################
def check_feature_geometry( feat, geom, max_error = 0.0001 ):
    try:
        f_geom = feat.GetGeometryRef()
    except:
        f_geom = feat

    if isinstance(geom,type('a')):
        geom = ogr.CreateGeometryFromWkt( geom )
    else:
        geom = geom.Clone()

    if (f_geom is not None and geom is None):
        gdaltest.post_reason( 'expected NULL geometry but got one.' )
        return 1

    if (f_geom is None and geom is not None):
        gdaltest.post_reason( 'expected geometry but got NULL.' )
        return 1

    if f_geom.GetGeometryName() != geom.GetGeometryName():
        gdaltest.post_reason( 'geometry names do not match' )
        return 1

    if f_geom.GetGeometryCount() != geom.GetGeometryCount():
        gdaltest.post_reason( 'sub-geometry counts do not match' )
        return 1

    if f_geom.GetPointCount() != geom.GetPointCount():
        gdaltest.post_reason( 'point counts do not match' )
        return 1

    if f_geom.GetGeometryCount() > 0:
        count = f_geom.GetGeometryCount()
        for i in range(count):
            result = check_feature_geometry( f_geom.GetGeometryRef(i),
                                             geom.GetGeometryRef(i),
                                             max_error )
            if result != 0:
                return result
            
    else:
        count = f_geom.GetPointCount()
        
        for i in range(count):
            x_dist = abs(f_geom.GetX(i) - geom.GetX(i))
            y_dist = abs(f_geom.GetY(i) - geom.GetY(i))
            z_dist = abs(f_geom.GetZ(i) - geom.GetZ(i))

            if max(x_dist,y_dist,z_dist) > max_error:
                gdaltest.post_reason( 'Error in vertex %d, off by %g.' \
                                      % (i, max(x_dist,y_dist,z_dist)) )
                return 1

    geom.Destroy()
    return 0

###############################################################################
def quick_create_layer_def( lyr, field_list):
    # Each field is a tuple of (name, type, width, precision)
    # Any of type, width and precision can be skipped.  Default type is string.

    for field in field_list:
        name = field[0]
        if len(field) > 1:
            type = field[1]
        else:
            type = ogr.OFTString

        field_defn = ogr.FieldDefn( name, type )
        
        if len(field) > 2:
            field_defn.SetWidth( int(field[2]) )

        if len(field) > 3:
            field_defn.SetPrecision( int(field[3]) )

        lyr.CreateField( field_defn )

        field_defn.Destroy()

###############################################################################
def quick_create_feature( layer, field_values, wkt_geometry ):
    feature = ogr.Feature( feature_def = layer.GetLayerDefn() )

    for i in range(len(field_values)):
        feature.SetField( i, field_values[i] )

    if wkt_geometry is not None:
        geom = ogr.CreateGeometryFromWkt( wkt_geometry )
        if geom is None:
            raise ValueError('Failed to create geometry from: ' + wkt_geometry)
        feature.SetGeometryDirectly( geom )

    result = layer.CreateFeature( feature )

    feature.Destroy()
    
    if result != 0:
        raise ValueError('CreateFeature() failed in ogrtest.quick_create_feature()')

###############################################################################
def have_geos():
    global geos_flag
    
    if geos_flag is None:
        pnt1 = ogr.CreateGeometryFromWkt( 'POINT(10 20)' )
        pnt2 = ogr.CreateGeometryFromWkt( 'POINT(30 20)' )

        try:
            result = pnt1.Union( pnt2 )
        except:
            result = None

        pnt1.Destroy()
        pnt2.Destroy()
        
        if result is None:
            geos_flag = 0
        else:
            geos_flag = 1

    return geos_flag
