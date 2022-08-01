# -*- coding: utf-8 -*-
###############################################################################
#
# Purpose:  Generate GPKG sample file
# Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2014-2017, Even Rouault <even.rouault at spatialys.com>
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

import sys
from osgeo import gdal
from osgeo import ogr
from osgeo import osr

if int(gdal.VersionInfo('VERSION_NUM')) < 2020000:
    print('Requires GDAL >= 2.2(dev)')
    sys.exit(1)

sr4326 = osr.SpatialReference()
sr4326.SetFromUserInput('WGS84')

sr32631 = osr.SpatialReference()
sr32631.ImportFromEPSG(32631)

byte_src_ds = gdal.OpenEx('https://raw.githubusercontent.com/OSGeo/gdal/master/autotest/gcore/data/byte.tif', allowed_drivers=['GTIFF', 'HTTP'])
elev_src_ds = gdal.OpenEx('https://raw.githubusercontent.com/OSGeo/gdal/master/autotest/gdrivers/data/n43.dt0', allowed_drivers=['DTED', 'HTTP'])

gdal.SetConfigOption('CREATE_METADATA_TABLES', 'NO')

for (out_filename, options) in [
        ('gdal_sample_v1.2_no_extensions.gpkg', {}),
    ('gdal_sample_v1.2_no_extensions_with_gpkg_ogr_contents.gpkg', {'gpkg_ogr_contents': True}),
    ('gdal_sample_v1.2_spatial_index_extension.gpkg', {'spi': True}),
    ('gdal_sample_v1.2_spi_nonlinear_webp_elevation.gpkg', {'spi': True, 'nonlinear': True, 'webp': True, 'elevation': True})
]:

    dataset_options = ['VERSION=1.2']
    if not('gpkg_ogr_contents' in options and options['gpkg_ogr_contents']):
        dataset_options += ['ADD_GPKG_OGR_CONTENTS=NO']

    layer_options = []
    if not('spi' in options and options['spi']):
        layer_options += ['SPATIAL_INDEX=NO']

    ds = ogr.GetDriverByName('GPKG').CreateDataSource(out_filename, options=dataset_options)

    lyr = ds.CreateLayer('attribute_table', geom_type=ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn('intfield', ogr.OFTInteger))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat['intfield'] = 1
    lyr.CreateFeature(feat)

    lyr = ds.CreateLayer('point2d', geom_type=ogr.wkbPoint, options=layer_options)
    lyr.CreateField(ogr.FieldDefn('intfield', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('strfield', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('realfield', ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn('datetimefield', ogr.OFTDateTime))
    lyr.CreateField(ogr.FieldDefn('datefield', ogr.OFTDate))
    lyr.CreateField(ogr.FieldDefn('binaryfield', ogr.OFTBinary))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat['intfield'] = 1
    feat['strfield'] = 'foo'
    feat['realfield'] = 1.23456
    feat['datetimefield'] = '2014/06/07 14:20:00'
    feat['datefield'] = '2014/06/07'
    feat.SetFieldBinaryFromHexString('binaryfield', '007FFF')
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 2)'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())

    # Null geometry
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)

    lyr = ds.CreateLayer('linestring2d', geom_type=ogr.wkbLineString, srs=sr4326, options=layer_options)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(1 2,3 4)'))
    lyr.CreateFeature(feat)

    # Null geometry
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)

    lyr = ds.CreateLayer('polygon2d', geom_type=ogr.wkbPolygon, srs=sr32631, options=layer_options)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((0 0,0 10,10 10,10 0,0 0),(1 1,1 9,9 9,9 1,1 1))'))
    lyr.CreateFeature(feat)

    # Null geometry
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)

    lyr = ds.CreateLayer('multipoint2d', geom_type=ogr.wkbMultiPoint, options=layer_options)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOINT(0 1,2 3)'))
    lyr.CreateFeature(feat)

    # Null geometry
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)

    lyr = ds.CreateLayer('multilinestring2d', geom_type=ogr.wkbMultiLineString, options=layer_options)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTILINESTRING((0 1,2 3),(4 5,6 7))'))
    lyr.CreateFeature(feat)

    # Null geometry
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)

    lyr = ds.CreateLayer('multipolygon2d', geom_type=ogr.wkbMultiPolygon, options=layer_options)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(1 1,1 9,9 9,9 1,1 1)),((-9 0,-9 10,-1 10,-1 0,-9 0)))'))
    lyr.CreateFeature(feat)

    # Null geometry
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)

    lyr = ds.CreateLayer('geomcollection2d', geom_type=ogr.wkbGeometryCollection, options=layer_options)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(POINT(0 1),LINESTRING(2 3,4 5),POLYGON((0 0,0 10,10 10,10 0,0 0),(1 1,1 9,9 9,9 1,1 1)),MULTIPOINT(0 1,2 3),MULTILINESTRING((0 1,2 3),(4 5,6 7)),MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(1 1,1 9,9 9,9 1,1 1)),((-9 0,-9 10,-1 10,-1 0,-9 0))))'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOINT(0 1,2 3)'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTILINESTRING((0 1,2 3),(4 5,6 7))'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(1 1,1 9,9 9,9 1,1 1)),((-9 0,-9 10,-1 10,-1 0,-9 0)))'))
    lyr.CreateFeature(feat)

    # Null geometry
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)

    lyr = ds.CreateLayer('geometry2d', geom_type=ogr.wkbUnknown, options=layer_options)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 2)'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(1 2,3 4)'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((0 0,0 10,10 10,10 0,0 0),(1 1,1 9,9 9,9 1,1 1))'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOINT(0 1,2 3)'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTILINESTRING((0 1,2 3),(4 5,6 7))'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(1 1,1 9,9 9,9 1,1 1)),((-9 0,-9 10,-1 10,-1 0,-9 0)))'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(POINT(0 1),LINESTRING(2 3,4 5),POLYGON((0 0,0 10,10 10,10 0,0 0),(1 1,1 9,9 9,9 1,1 1)),MULTIPOINT(0 1,2 3),MULTILINESTRING((0 1,2 3),(4 5,6 7)),MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(1 1,1 9,9 9,9 1,1 1)),((-9 0,-9 10,-1 10,-1 0,-9 0))))'))
    lyr.CreateFeature(feat)

    # Null geometry
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)

    lyr = ds.CreateLayer('point3d', geom_type=ogr.wkbPoint25D, options=layer_options)
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 2 3)'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())

    # Null geometry
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)

    lyr = ds.CreateLayer('linestring3d', geom_type=ogr.wkbLineString25D, options=layer_options)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(1 2 3,4 5 6)'))
    lyr.CreateFeature(feat)

    # Null geometry
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)

    lyr = ds.CreateLayer('polygon3d', geom_type=ogr.wkbPolygon25D, options=layer_options)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((0 0 100,0 10 100,10 10 100,10 0 100,0 0 100),(1 1 100,1 9 100,9 9 100,9 1 100,1 1 100))'))
    lyr.CreateFeature(feat)

    # Null geometry
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)

    lyr = ds.CreateLayer('multipoint3d', geom_type=ogr.wkbMultiPoint25D, options=layer_options)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOINT(0 1 2,3 4 5)'))
    lyr.CreateFeature(feat)

    # Null geometry
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)

    lyr = ds.CreateLayer('multilinestring3d', geom_type=ogr.wkbMultiLineString25D, options=layer_options)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTILINESTRING((0 1 2,3 4 5),(6 7 8,9 10 11))'))
    lyr.CreateFeature(feat)

    # Null geometry
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)

    lyr = ds.CreateLayer('multipolygon3d', geom_type=ogr.wkbMultiPolygon25D, options=layer_options)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOLYGON(((0 0 100,0 10 100,10 10 100,10 0 100,0 0 100),(1 1 100,1 9 100,9 9 100,9 1 100,1 1 100)),((-9 0 50,-9 10 50,-1 10 50,-1 0 50,-9 0 50)))'))
    lyr.CreateFeature(feat)

    # Null geometry
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)

    lyr = ds.CreateLayer('geomcollection3d', geom_type=ogr.wkbGeometryCollection25D, options=layer_options)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(POINT(1 2 3),LINESTRING(1 2 3,4 5 6),POLYGON((0 0 100,0 10 100,10 10 100,10 0 100,0 0 100),(1 1 100,1 9 100,9 9 100,9 1 100,1 1 100)),MULTIPOINT(0 1 2,3 4 5),MULTILINESTRING((0 1 2,3 4 5),(6 7 8,9 10 11)),MULTIPOLYGON(((0 0 100,0 10 100,10 10 100,10 0 100,0 0 100),(1 1 100,1 9 100,9 9 100,9 1 100,1 1 100)),((-9 0 50,-9 10 50,-1 10 50,-1 0 50,-9 0 50))))'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOINT(0 1 2,3 4 5)'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTILINESTRING((0 1 2,3 4 5),(6 7 8,9 10 11))'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOLYGON(((0 0 100,0 10 100,10 10 100,10 0 100,0 0 100),(1 1 100,1 9 100,9 9 100,9 1 100,1 1 100)),((-9 0 50,-9 10 50,-1 10 50,-1 0 50,-9 0 50)))'))
    lyr.CreateFeature(feat)

    # Null geometry
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)

    lyr = ds.CreateLayer('geometry3d', geom_type=ogr.wkbUnknown | ogr.wkb25DBit, options=layer_options)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 2 3)'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(1 2 3,4 5 6)'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((0 0 100,0 10 100,10 10 100,10 0 100,0 0 100),(1 1 100,1 9 100,9 9 100,9 1 100,1 1 100))'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOINT(0 1 2,3 4 5)'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTILINESTRING((0 1 2,3 4 5),(6 7 8,9 10 11))'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOLYGON(((0 0 100,0 10 100,10 10 100,10 0 100,0 0 100),(1 1 100,1 9 100,9 9 100,9 1 100,1 1 100)),((-9 0 50,-9 10 50,-1 10 50,-1 0 50,-9 0 50)))'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(POINT(1 2 3),LINESTRING(1 2 3,4 5 6),POLYGON((0 0 100,0 10 100,10 10 100,10 0 100,0 0 100),(1 1 100,1 9 100,9 9 100,9 1 100,1 1 100)),MULTIPOINT(0 1 2,3 4 5),MULTILINESTRING((0 1 2,3 4 5),(6 7 8,9 10 11)),MULTIPOLYGON(((0 0 100,0 10 100,10 10 100,10 0 100,0 0 100),(1 1 100,1 9 100,9 9 100,9 1 100,1 1 100)),((-9 0 50,-9 10 50,-1 10 50,-1 0 50,-9 0 50))))'))
    lyr.CreateFeature(feat)

    # Null geometry
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)

    if 'nonlinear' in options and options['nonlinear']:
        lyr = ds.CreateLayer('circularstring', geom_type=ogr.wkbCircularString, options=layer_options)
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeometry(ogr.CreateGeometryFromWkt('CIRCULARSTRING(0 0,1 1,2 0)'))
        lyr.CreateFeature(feat)

        lyr = ds.CreateLayer('compoundcurve', geom_type=ogr.wkbCompoundCurve, options=layer_options)
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeometry(ogr.CreateGeometryFromWkt('COMPOUNDCURVE(CIRCULARSTRING(0 0,1 1,2 0),(2 0,3 0))'))
        lyr.CreateFeature(feat)

        lyr = ds.CreateLayer('curvepolygon', geom_type=ogr.wkbCurvePolygon, options=layer_options)
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeometry(ogr.CreateGeometryFromWkt('CURVEPOLYGON(COMPOUNDCURVE(CIRCULARSTRING(0 0,1 1,2 0),(2 0,3 0,3 -1,0 -1,0 0)))'))
        lyr.CreateFeature(feat)

        lyr = ds.CreateLayer('multicurve', geom_type=ogr.wkbMultiCurve, options=layer_options)
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTICURVE(CIRCULARSTRING(0 0,1 1,2 0))'))
        lyr.CreateFeature(feat)

        lyr = ds.CreateLayer('multisurface', geom_type=ogr.wkbMultiSurface, options=layer_options)
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTISURFACE(CURVEPOLYGON(COMPOUNDCURVE(CIRCULARSTRING(0 0,1 1,2 0),(2 0,3 0,3 -1,0 -1,0 0))))'))
        lyr.CreateFeature(feat)

        lyr = ds.CreateLayer('curve', geom_type=ogr.wkbCurve, options=layer_options)
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(0 0,1 1,2 0)'))
        lyr.CreateFeature(feat)
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeometry(ogr.CreateGeometryFromWkt('CIRCULARSTRING(0 0,1 1,2 0)'))
        lyr.CreateFeature(feat)
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeometry(ogr.CreateGeometryFromWkt('COMPOUNDCURVE(CIRCULARSTRING(0 0,1 1,2 0),(2 0,3 0))'))
        lyr.CreateFeature(feat)

        lyr = ds.CreateLayer('surface', geom_type=ogr.wkbSurface, options=layer_options)
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((0 0,0 1,1 1,0 0))'))
        lyr.CreateFeature(feat)
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeometry(ogr.CreateGeometryFromWkt('CURVEPOLYGON(COMPOUNDCURVE(CIRCULARSTRING(0 0,1 1,2 0),(2 0,3 0,3 -1,0 -1,0 0)))'))
        lyr.CreateFeature(feat)

    ds = None

    raster_options = ['APPEND_SUBDATASET=YES']

    gdal.GetDriverByName('GPKG').CreateCopy(out_filename, byte_src_ds,
                                            options=raster_options + ['TILE_FORMAT=PNG', 'RASTER_TABLE=byte_png'])
    gdal.GetDriverByName('GPKG').CreateCopy(out_filename, byte_src_ds,
                                            options=raster_options + ['TILE_FORMAT=JPEG', 'RASTER_TABLE=byte_jpeg'])

    if 'webp' in options and options['webp']:
        gdal.GetDriverByName('GPKG').CreateCopy(out_filename, byte_src_ds,
                                                options=raster_options + ['TILE_FORMAT=WEBP', 'RASTER_TABLE=byte_webp'])

    if 'elevation' in options and options['elevation']:
        gdal.Translate(out_filename, elev_src_ds,
                       format='GPKG',
                       outputType=gdal.GDT_Float32,
                       creationOptions=raster_options + ['RASTER_TABLE=elev_tiff'])
        gdal.GetDriverByName('GPKG').CreateCopy(out_filename, elev_src_ds,
                                                options=raster_options + ['RASTER_TABLE=elev_png'])
