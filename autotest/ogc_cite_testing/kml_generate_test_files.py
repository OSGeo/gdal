#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Generate test files that can be used to exercise the points tested
#           by the OGC KML 2.2 – Abstract Test Suite
#           (http://portal.opengeospatial.org/files/?artifact_id=27811)
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
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

from osgeo import gdal
from osgeo import ogr

###############################################################################
# Generate a .kml/.kmz file with OGR LIBKML driver covering most requirements
#
# Following steps are covered :
# ATC 1: Root element
# ATC 2: XML Schema constraints
# ATC 3: Geometry coordinates
# ATC 4: TimeSpan
# ATC 5: TimeStamp
# ATC 6: Style reference
# ATC 7: Shared style definition
# ATC 8: Region – LatLonAltBox
# ATC 9: Link elements
# ATC 10: Link referent
# ATC 12: Geometry - extrude
# ATC 13: Geometry - tessellate --> test currently broken.
# ATC 14: Point
# ATC 15: LineString
# ATC 16: LinearRing - control points
# ATC 17: Polygon boundary
# ATC 18: Icon - href
# ATC 19: ViewVolume - minimal content
# ATC 20: NetworkLinkControl - minRefreshPeriod
# ATC 21: Empty object
# ATC 24: PhoneNumber
# ATC 25: Schema
# ATC 26: Schema - SimpleField
# ATC 27: ExtendedData - SchemaData
# ATC 28: ExtendedData - Data
# ATC 29: Alias
# ATC 30: atom:author
# ATC 31: atom:link
# ATC 32: Orientation - minimal content
# ATC 34: Model
# ATC 35: PhotoOverlay - minimal content
# ATC 36: Pair
# ATC 37: ItemIcon
# ATC 38: LookAt
# ATC 39: Lod
# ATC 40: Link
# ATC 41: Region
# ATC 42: PolyStyle
# ATC 43: Coordinates - altitudeMode
# ATC 44: Scale - minimal content
# ATC 45: KML - minimal content
# ATC 46: ViewFormat
# ATC 47: httpQuery
# ATC 48: LinearRing in Polygon
# ATC 49: Data
# ATC 50: ResourceMap - Alias
# ATC 51: Link refresh values
# ATC 52: PhotoOverlay
# ATC 54: Camera
# ATC 55: Location
# ATC 56: Overlay
# ATC 57: ScreenOverlay
# ATC 58: BalloonStyle
# ATC 59: ExtendedData
# ATC 60: Folder
# ATC 61: IconStyle
# ATC 62: ImagePyramid
# ATC 63: LabelStyle
# ATC 64: ListStyle
# ATC 65: Style
# ATC 66: MultiGeometry
# ATC 67: Placemark
# ATC 68: StyleMap
# ATC 69: Polygon - rings
# ATC 70: LinearRing - Simple
# ATC 71: BalloonStyle - color
# ATC 72: Metadata
# ATC 73: Scale - full content
# ATC 74: Lod - fade extents
# ATC 75: Orientation - full content
# ATC 76: Snippet
# ATC 77: NetworkLink-Url


def generate_libkml(filename):

    gdal.Unlink(filename)

    content = """eiffel_tower_normal:SYMBOL(id:"http://upload.wikimedia.org/wikipedia/commons/thumb/7/78/Eiffel_Tower_from_north_Avenue_de_New_York%2C_Aug_2010.jpg/220px-Eiffel_Tower_from_north_Avenue_de_New_York%2C_Aug_2010.jpg");LABEL(c:#FF0000FF)
eiffel_tower_highlight:SYMBOL(id:"http://upload.wikimedia.org/wikipedia/commons/thumb/7/78/Eiffel_Tower_from_north_Avenue_de_New_York%2C_Aug_2010.jpg/220px-Eiffel_Tower_from_north_Avenue_de_New_York%2C_Aug_2010.jpg");LABEL(c:#0000FFFF)"""
    gdal.FileFromMemBuffer("/vsimem/style.txt", content)
    style_table = ogr.StyleTable()
    style_table.LoadStyleTable("/vsimem/style.txt")
    gdal.Unlink("/vsimem/style.txt")

    ds_options = ['author_name=Even Rouault',
                  'author_uri=http://gdal.org',
                  'author_email=foo@bar.com',
                  'link=http://gdal.org',
                  'phonenumber=tel:12345678',
                  'NLC_MINREFRESHPERIOD=3600',
                  'NLC_MAXSESSIONLENGTH=-1',
                  'NLC_COOKIE=cookie',
                  'NLC_MESSAGE=message',
                  'NLC_LINKNAME=linkname',
                  'NLC_LINKDESCRIPTION=linkdescription',
                  'NLC_LINKSNIPPET=linksnippet',
                  'NLC_EXPIRES=2014-12-31T23:59:59Z',
                  'LISTSTYLE_ICON_HREF=http://www.gdal.org/gdalicon.png',
                  'eiffel_tower_normal_balloonstyle_bgcolor=#FFFF00']
    ds = ogr.GetDriverByName('LIBKML').CreateDataSource(filename, options=ds_options)

    ds.SetStyleTable(style_table)

    lyr_options = ['LOOKAT_LONGITUDE=2.2945', 'LOOKAT_LATITUDE=48.85825', 'LOOKAT_RANGE=300',
                   'LOOKAT_ALTITUDE=30', 'LOOKAT_HEADING=0', 'LOOKAT_TILT=70', 'LOOKAT_ALTITUDEMODE=relativeToGround',
                   'ADD_REGION=YES', 'REGION_MIN_LOD_PIXELS=128', 'REGION_MAX_LOD_PIXELS=10000000',
                   'REGION_MIN_FADE_EXTENT=1', 'REGION_MAX_FADE_EXTENT=2', 'SO_HREF=http://www.gdal.org/gdalicon.png',
                   'LISTSTYLE_ICON_HREF=http://www.gdal.org/gdalicon.png']
    lyr = ds.CreateLayer('test', options=lyr_options)
    lyr.CreateField(ogr.FieldDefn('name', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('description', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('nom_francais', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('int_value', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('double_value', ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn('timestamp', ogr.OFTDateTime))
    lyr.CreateField(ogr.FieldDefn('begin', ogr.OFTDateTime))
    lyr.CreateField(ogr.FieldDefn('end', ogr.OFTDateTime))
    lyr.CreateField(ogr.FieldDefn('snippet', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('altitudeMode', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('extrude', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('tessellate', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('model', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("scale_x", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("scale_y", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("scale_z", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("heading", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("tilt", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("roll", ogr.OFTReal))

    lyr.CreateField(ogr.FieldDefn("networklink", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("networklink_refreshvisibility", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("networklink_flytoview", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("networklink_refreshMode", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("networklink_refreshInterval", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("networklink_viewRefreshMode", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("networklink_viewRefreshTime", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("networklink_viewBoundScale", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("networklink_viewFormat", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("networklink_httpQuery", ogr.OFTString))

    lyr.CreateField(ogr.FieldDefn("camera_longitude", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("camera_latitude", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("camera_altitude", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("camera_altitudemode", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("photooverlay", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("leftfov", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("rightfov", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("bottomfov", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("topfov", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("near", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("photooverlay_shape", ogr.OFTString))

    lyr.CreateField(ogr.FieldDefn("imagepyramid_tilesize", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("imagepyramid_maxwidth", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("imagepyramid_maxheight", ogr.OFTInteger))

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('name', 'Eiffel tower')
    feat.SetField('description', 'Famous Paris attraction. Built by Gustave Eiffel in 1889.')
    feat.SetField('nom_francais', 'Tour Eiffel')
    feat.SetField('int_value', 12)
    feat.SetField('double_value', 34.56)
    feat.SetField('snippet', 'Very cool snippet')
    feat.SetField('begin', '1889/05/06')
    feat.SetField('end', '9999/12/31')
    feat.SetStyleString('@eiffel_tower')
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2.2945 48.85825)'))
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('name', 'Avenue Gustave Eiffel')
    feat.SetField('timestamp', '2014/02/22')
    feat.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(2.29420 48.85746,2.29540 48.85833)'))
    feat.SetStyleString('PEN(c:#00FF00)')
    feat.SetField('tessellate', 1)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('name', 'Ecole Militaire')
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((2.30383 48.85162 15,2.30460 48.85220 15,2.30581 48.85152 15,2.30507 48.85083 15,2.30383 48.85162 15))'))
    feat.SetField('altitudeMode', 'relativeToGround')
    feat.SetField('extrude', 1)
    feat.SetStyleString('BRUSH(fc:#0000FF)')
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('name', 'Champ de Mars')
    feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOLYGON(((2.29413 48.85703,2.29606 48.85847,2.29837 48.85679,2.29676 48.85543,2.29413 48.85703)),((2.29656 48.85504,2.29929 48.85674,2.30359 48.85364,2.30164 48.85226,2.29656 48.85504)))'))
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2.2945 48.85825 10)'))
    feat.SetField("tilt", 75)
    feat.SetField("roll", 10)
    feat.SetField("heading", -70)
    feat.SetField("scale_x", 2)
    feat.SetField("scale_y", 3)
    feat.SetField("scale_z", 4)
    feat.SetField("altitudeMode", "relativeToGround")
    feat.SetField("model", "http://even.rouault.free.fr/kml/gdal_2.1/dummy.dae")
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("name", "a network link")
    feat.SetField("networklink", "http://developers.google.com/kml/documentation/Point.kml")
    feat.SetField("networklink_refreshVisibility", 1)
    feat.SetField("networklink_flyToView", 1)
    feat.SetField("networklink_refreshInterval", 60)
    feat.SetField("networklink_httpQuery", "[clientVersion]")
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("networklink", "http://developers.google.com/kml/documentation/Point.kml")
    feat.SetField("networklink_viewRefreshTime", 30)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("networklink", "http://developers.google.com/kml/documentation/Point.kml")
    feat.SetField("networklink_refreshMode", 'onExpire')
    feat.SetField("networklink_viewRefreshMode", 'onRegion')
    feat.SetField("networklink_viewBoundScale", 0.5)
    feat.SetField("networklink_viewFormat", 'BBOX=[bboxWest],[bboxSouth],[bboxEast],[bboxNorth]')
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("photooverlay", "http://even.rouault.free.fr/kml/gdalicon_$[level]_$[x]_$[y].png")
    feat.SetField("imagepyramid_tilesize", 256)
    feat.SetField("imagepyramid_maxwidth", 512)
    feat.SetField("imagepyramid_maxheight", 512)
    feat.SetField("camera_longitude", 2.2946)
    feat.SetField("camera_latitude", 48.8583)
    feat.SetField("camera_altitude", 20)
    feat.SetField("camera_altitudemode", "relativeToGround")
    feat.SetField("leftfov", -60)
    feat.SetField("rightfov", 60)
    feat.SetField("bottomfov", -60)
    feat.SetField("topfov", 60)
    feat.SetField("near", 100)
    feat.SetField("heading", 0)
    feat.SetField("tilt", 90)
    feat.SetField("roll", 0)
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2.2945 48.85825)'))
    lyr.CreateFeature(feat)

    # feat = ogr.Feature(lyr.GetLayerDefn())
    # feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT EMPTY'))
    # lyr.CreateFeature(feat)

    # feat = ogr.Feature(lyr.GetLayerDefn())
    # # feat.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING EMPTY'))
    # lyr.CreateFeature(feat)

    # feat = ogr.Feature(lyr.GetLayerDefn())
    # feat.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON EMPTY'))
    # lyr.CreateFeature(feat)

    # feat = ogr.Feature(lyr.GetLayerDefn())
    # feat.SetGeometry(ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION (POINT EMPTY,POINT(1 2))'))
    # lyr.CreateFeature(feat)

    lyr_options = ['CAMERA_LONGITUDE=2.2945', 'CAMERA_LATITUDE=48.85825', 'CAMERA_ALTITUDE=30',
                   'CAMERA_HEADING=120', 'CAMERA_TILT=70', 'CAMERA_ROLL=10', 'CAMERA_ALTITUDEMODE=relativeToGround',
                   'FOLDER=YES', 'NAME=layer_name', 'DESCRIPTION=description', 'OPEN=1', 'VISIBILITY=1', 'SNIPPET=snippet']
    ds.CreateLayer('test2', options=lyr_options)

    gdal.SetConfigOption('LIBKML_USE_SIMPLEFIELD', 'NO')
    lyr = ds.CreateLayer('test_data')
    gdal.SetConfigOption('LIBKML_USE_SIMPLEFIELD', None)
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("foo", "bar")
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2.2945 48.85825)'))
    lyr.CreateFeature(feat)

    ds = None

###############################################################################
# Generate a .kml file with OGR LIBKML driver covering the Update KML features
#
# Following steps are covered :
# ATC 22: Update - targetHref
# ATC 23: Identification of update target


def generate_libkml_update(filename):
    gdal.Unlink(filename)

    ds = ogr.GetDriverByName('LIBKML').CreateDataSource(filename,
                                                        options=['UPDATE_TARGETHREF=http://even.rouault.free.fr/kml/gdal_2.1/test_ogrlibkml.kml'])
    lyr = ds.CreateLayer('test')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(100)
    lyr.CreateFeature(feat)
    feat.SetFID(1)
    lyr.SetFeature(feat)
    lyr.DeleteFeature(1)
    ds = None

###############################################################################
# Generate a .kml file with GDAL KMLSuperOverlay driver covering the GroundOverlay KML features
#
# Following steps are covered :
# ATC 11: LatLonBox
# ATC 33: GroundOverlay
# ATC 53: GroundOverlay - minimal content


def generate_kmlsuperoverlay(filename):

    src_ds = gdal.GetDriverByName('MEM').Create('', 512, 256, 3)
    src_ds.SetGeoTransform([-180, 360. / 512, 0, 90, 0, -180. / 256])
    ds = gdal.GetDriverByName('KMLSuperOverlay').CreateCopy(filename, src_ds)
    del ds
    src_ds = None


if __name__ == '__main__':

    generate_libkml('test_ogrlibkml.kml')
    generate_libkml('test_ogrlibkml.kmz')
    generate_libkml_update('test_ogrlibkml_update.kml')
    generate_kmlsuperoverlay('test_superoverlay.kmz')
