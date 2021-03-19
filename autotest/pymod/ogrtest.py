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

import contextlib
import sys
import pytest

sys.path.append('../pymod')

from osgeo import ogr
import gdaltest

geos_flag = None
sfcgal_flag = None

###############################################################################


def check_features_against_list(layer, field_name, value_list):

    field_index = layer.GetLayerDefn().GetFieldIndex(field_name)
    if field_index < 0:
        gdaltest.post_reason('did not find required field ' + field_name)
        return 0

    for i, value in enumerate(value_list):
        feat = layer.GetNextFeature()
        if feat is None:
            gdaltest.post_reason('Got only %d features, not the expected %d features.' % (i, len(value_list)))
            return 0

        if isinstance(value, type('str')):
            isok = (feat.GetFieldAsString(field_index) != value)
        else:
            isok = (feat.GetField(field_index) != value)
        if isok:
            gdaltest.post_reason('field %s feature %d did not match expected value %s, got %s.' % (field_name, i, str(value), str(feat.GetField(field_index))))
            return 0

    feat = layer.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('got more features than expected')
        return 0

    return 1

###############################################################################


def check_feature_geometry(feat, geom, max_error=0.0001):
    """ Returns 0 in case of success """
    try:
        f_geom = feat.GetGeometryRef()
    except:
        f_geom = feat

    if geom is not None and isinstance(geom, type('a')):
        geom = ogr.CreateGeometryFromWkt(geom)

    if (f_geom is not None and geom is None):
        gdaltest.post_reason('expected NULL geometry but got one.')
        return 1

    if (f_geom is None and geom is not None):
        gdaltest.post_reason('expected geometry but got NULL.')
        return 1

    if f_geom is None and geom is None:
        return 0

    if f_geom.GetGeometryName() != geom.GetGeometryName():
        gdaltest.post_reason('geometry names do not match.  "%s" ! = "%s"' %
                             (f_geom.GetGeometryName(),
                              geom.GetGeometryName()))
        return 1

    if f_geom.GetGeometryCount() != geom.GetGeometryCount():
        gdaltest.post_reason('sub-geometry counts do not match')
        return 1

    if f_geom.GetPointCount() != geom.GetPointCount():
        gdaltest.post_reason('point counts do not match')
        return 1

    # ST_Equals(a,b) <==> ST_Within(a,b) && ST_Within(b,a)
    # We can't use OGRGeometry::Equals() because it doesn't not test spatial
    # equality, but structural one
    if have_geos() and f_geom.Within(geom) and geom.Within(f_geom):
        return 0

    if f_geom.GetGeometryCount() > 0:
        count = f_geom.GetGeometryCount()
        for i in range(count):
            result = check_feature_geometry(f_geom.GetGeometryRef(i),
                                            geom.GetGeometryRef(i),
                                            max_error)
            if result != 0:
                return result
    else:
        count = f_geom.GetPointCount()

        for i in range(count):
            x_dist = abs(f_geom.GetX(i) - geom.GetX(i))
            y_dist = abs(f_geom.GetY(i) - geom.GetY(i))
            z_dist = abs(f_geom.GetZ(i) - geom.GetZ(i))
            m_dist = abs(f_geom.GetM(i) - geom.GetM(i))

            # Hack to deal with shapefile not-a-number M values that equal to -1.79769313486232e+308
            if m_dist > max_error and f_geom.GetM(i) < -1.7e308 and geom.GetM(i) < -1.7e308:
                m_dist = 0

            if max(x_dist, y_dist, z_dist, m_dist) > max_error:
                gdaltest.post_reason('Error in vertex %d, off by %g.'
                                     % (i, max(x_dist, y_dist, z_dist, m_dist)))
                # print(f_geom.GetX(i))
                # print(geom.GetX(i))
                # print(f_geom.GetY(i))
                # print(geom.GetY(i))
                # print(f_geom.GetZ(i))
                # print(geom.GetZ(i))
                return 1

    return 0

###############################################################################


def check_feature(feat, feat_ref, max_error=0.0001, excluded_fields=None):
    """ Returns 0 in case of success """

    for i in range(feat.GetGeomFieldCount()):
        ret = check_feature_geometry(feat.GetGeomFieldRef(i),
                                     feat_ref.GetGeomFieldRef(i),
                                     max_error=max_error)
        if ret != 0:
            return ret

    for i in range(feat.GetFieldCount()):
        if excluded_fields is not None:
            if feat.GetDefnRef().GetFieldDefn(i).GetName() in excluded_fields:
                continue
        if feat.GetField(i) != feat_ref.GetField(i):
            gdaltest.post_reason('Field %d, expected val %s, got val %s' %
                                 (i, str(feat_ref.GetField(i)),
                                  str(feat.GetField(i))))
            return 1

    return 0

###############################################################################


def compare_layers(lyr, lyr_ref, excluded_fields=None):

    for f_ref in lyr_ref:
        f = lyr.GetNextFeature()
        if f is None:
            f_ref.DumpReadable()
            pytest.fail()
        if check_feature(f, f_ref, excluded_fields=excluded_fields) != 0:
            f.DumpReadable()
            f_ref.DumpReadable()
            pytest.fail()
    f = lyr.GetNextFeature()
    if f is not None:
        f.DumpReadable()
        pytest.fail()

###############################################################################
# Temporarily enable exceptions


@contextlib.contextmanager
def enable_exceptions():
    if ogr.GetUseExceptions():
        try:
            yield
        finally:
            pass
        return

    ogr.UseExceptions()
    try:
        yield
    finally:
        ogr.DontUseExceptions()

###############################################################################

def get_wkt_data_series(with_z, with_m, with_gc, with_circular, with_surface):
    basic_wkts = [
        'POINT (1 1)',
        'POINT (1.1234 1.4321)',
        'POINT (1.12345678901234 1.4321)',
        'POINT (1.2 -2.1)',
        'MULTIPOINT ((10 40),(40 30),(20 20),(30 10))',
        'LINESTRING (1.2 -2.1,2.4 -4.8)',
        'MULTILINESTRING ((10 10,20 20,10 40),(40 40,30 30,40 20,30 10),(50 50,60 60,50 90))',
        'MULTILINESTRING ((1.2 -2.1,2.4 -4.8))',
        'POLYGON ((30 10,40 40,20 40,10 20,30 10))',
        'POLYGON ((35 10,45 45,15 40,10 20,35 10),(20 30,35 35,30 20,20 30))',
        'MULTIPOLYGON (((30 20,45 40,10 40,30 20)),((15 5,40 10,10 20,5 10,15 5)))',
        'MULTIPOLYGON (((40 40,20 45,45 30,40 40)),((20 35,10 30,10 10,30 5,45 20,20 35),(30 20,20 15,20 25,30 20)))',
        'MULTIPOLYGON (((30 20,45 40,10 40,30 20)))',
        'MULTIPOLYGON (((35 10,45 45,15 40,10 20,35 10),(20 30,35 35,30 20,20 30)))',
    ]
    gc_wkts = [
        'GEOMETRYCOLLECTION (POINT (4 6),LINESTRING (4 6,7 10))',
        'GEOMETRYCOLLECTION (POINT (4 6),GEOMETRYCOLLECTION (POINT (4 6),LINESTRING (4 6,7 10)))'
    ]
    circular_wkts = [
        'CIRCULARSTRING (0 0,1 1,1 0)',
        'COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,1 0),(1 0,0 1))',
        'CURVEPOLYGON (CIRCULARSTRING (0 0,4 0,4 4,0 4,0 0),(1 1,3 3,3 1,1 1))',
        'MULTICURVE ((0 0,5 5),CIRCULARSTRING (4 0,4 4,8 4))',
        'MULTISURFACE (CURVEPOLYGON (CIRCULARSTRING (0 0,4 0,4 4,0 4,0 0),(1 1,3 3,3 1,1 1)),((10 10,14 12,11 10,10 10),(11 11,11.5 11.0,11.0 11.5,11 11)))',
    ]
    surface_wkts = [
        'POLYHEDRALSURFACE Z (((0 0 0,0 0 1,0 1 1,0 1 0,0 0 0)),((0 0 0,0 1 0,1 1 0,1 0 0,0 0 0)),((0 0 0,1 0 0,1 0 1,0 0 1,0 0 0)),((1 1 0,1 1 1,1 0 1,1 0 0,1 1 0)),((0 1 0,0 1 1,1 1 1,1 1 0,0 1 0)),((0 0 1,1 0 1,1 1 1,0 1 1,0 0 1)))',
        'TRIANGLE ((0 0,0 9,9 0,0 0))',
        'TIN Z (((0 0 0,0 0 1,0 1 0,0 0 0)))',
        'TIN Z (((0 0 0,0 0 1,0 1 0,0 0 0)),((0 0 0,0 1 0,1 1 0,0 0 0)))',
    ]

    wkts = basic_wkts
    wkts_with_z = []
    wkts_with_m = []
    wkts_with_zm = []
    if with_gc:
        wkts.extend(gc_wkts)
    if with_circular:
        wkts.extend(circular_wkts)
    if with_z or with_m:
        for i, wkt in enumerate(wkts):
            if with_z:
                g = ogr.CreateGeometryFromWkt(wkt)
                g.Set3D(True)
                wkts_with_z.extend([g.ExportToIsoWkt()])
            if with_z:
                g = ogr.CreateGeometryFromWkt(wkt)
                g.SetMeasured(True)
                wkts_with_m.extend([g.ExportToIsoWkt()])
            if with_z and with_m:
                g = ogr.CreateGeometryFromWkt(wkt)
                g.Set3D(True)
                g.SetMeasured(True)
                wkts_with_zm.extend([g.ExportToIsoWkt()])
    wkts.extend(wkts_with_z)
    wkts.extend(wkts_with_m)
    wkts.extend(wkts_with_zm)
    if with_surface:
        wkts.extend(surface_wkts)
    return wkts

###############################################################################


def quick_create_layer_def(lyr, field_list):
    # Each field is a tuple of (name, type, width, precision)
    # Any of type, width and precision can be skipped.  Default type is string.

    for field in field_list:
        name = field[0]
        if len(field) > 1:
            field_type = field[1]
        else:
            field_type = ogr.OFTString

        field_defn = ogr.FieldDefn(name, field_type)

        if len(field) > 2:
            field_defn.SetWidth(int(field[2]))

        if len(field) > 3:
            field_defn.SetPrecision(int(field[3]))

        lyr.CreateField(field_defn)

###############################################################################


def quick_create_feature(layer, field_values, wkt_geometry):
    feature = ogr.Feature(feature_def=layer.GetLayerDefn())

    for i, field_value in enumerate(field_values):
        feature.SetField(i, field_value)

    if wkt_geometry is not None:
        geom = ogr.CreateGeometryFromWkt(wkt_geometry)
        if geom is None:
            raise ValueError('Failed to create geometry from: ' + wkt_geometry)
        feature.SetGeometryDirectly(geom)

    result = layer.CreateFeature(feature)

    if result != 0:
        raise ValueError('CreateFeature() failed in ogrtest.quick_create_feature()')

###############################################################################


def have_geos():
    global geos_flag

    if geos_flag is None:
        pnt1 = ogr.CreateGeometryFromWkt('POINT(10 20)')
        pnt2 = ogr.CreateGeometryFromWkt('POINT(30 20)')
        geos_flag = pnt1.Union(pnt2) is not None

    return geos_flag

###############################################################################


def have_sfcgal():
    global sfcgal_flag

    if sfcgal_flag is None:
        pnt1 = ogr.CreateGeometryFromWkt('POINT(10 20 30)')
        pnt2 = ogr.CreateGeometryFromWkt('POINT(40 50 60)')
        sfcgal_flag = pnt1.Distance3D(pnt2) >= 0

    return sfcgal_flag
