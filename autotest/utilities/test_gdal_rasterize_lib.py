#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_rasterize testing
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
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

sys.path.append( '../pymod' )

from osgeo import gdal, ogr, osr
import gdaltest

###############################################################################
# Simple polygon rasterization (adapted from alg/rasterize.py).

def test_gdal_rasterize_lib_1():


    # Setup working spatial reference
    #sr_wkt = 'LOCAL_CS["arbitrary"]'
    #sr = osr.SpatialReference( sr_wkt )
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    sr_wkt = sr.ExportToWkt()

    # Create a raster to rasterize into.

    target_ds = gdal.GetDriverByName('MEM').Create( '', 100, 100, 3,
                                                    gdal.GDT_Byte )
    target_ds.SetGeoTransform( (1000,1,0,1100,0,-1) )
    target_ds.SetProjection( sr_wkt )

    # Create a layer to rasterize from.

    vector_ds = \
              gdal.GetDriverByName('Memory').Create( '', 0, 0, 0 )
    rast_lyr = vector_ds.CreateLayer( 'rast1', srs=sr )

    rast_lyr.GetLayerDefn()
    field_defn = ogr.FieldDefn('foo')
    rast_lyr.CreateField(field_defn)

    # Add a polygon.

    wkt_geom = 'POLYGON((1020 1030,1020 1045,1050 1045,1050 1030,1020 1030))'

    feat = ogr.Feature( rast_lyr.GetLayerDefn() )
    feat.SetGeometryDirectly( ogr.Geometry(wkt = wkt_geom) )

    rast_lyr.CreateFeature( feat )

    # Add feature without geometry to test fix for #3310
    feat = ogr.Feature( rast_lyr.GetLayerDefn() )
    rast_lyr.CreateFeature( feat )

    # Add a linestring.

    wkt_geom = 'LINESTRING(1000 1000, 1100 1050)'

    feat = ogr.Feature( rast_lyr.GetLayerDefn() )
    feat.SetGeometryDirectly( ogr.Geometry(wkt = wkt_geom) )

    rast_lyr.CreateFeature( feat )

    ret = gdal.Rasterize(target_ds, vector_ds, bands = [3,2,1], burnValues = [200,220,240], layers = 'rast1')
    if ret != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # Check results.
    expected = 6452
    checksum = target_ds.GetRasterBand(2).Checksum()
    if checksum != expected:
        print(checksum)
        gdaltest.post_reason( 'Did not get expected image checksum' )

        return 'fail'

    target_ds = None

    return 'success'

###############################################################################
# Test creating an output file

def test_gdal_rasterize_lib_3():

    import test_cli_utilities
    if test_cli_utilities.get_gdal_contour_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_contour_path() + ' ../gdrivers/data/n43.dt0 tmp/n43dt0.shp -i 10 -3d')

    with gdaltest.error_handler():
        ds = gdal.Rasterize('/vsimem/bogus.tif', 'tmp/n43dt0.shp')
    if ds is not None:
        gdaltest.post_reason('did not expected success')
        return 'fail'

    ds = gdal.Rasterize('', 'tmp/n43dt0.shp', format = 'MEM', outputType = gdal.GDT_Byte, useZ = True, layers = ['n43dt0'], width = 121, height = 121, noData = 0)

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource( 'tmp/n43dt0.shp' )

    ds_ref = gdal.Open('../gdrivers/data/n43.dt0')

    if ds.GetRasterBand(1).GetNoDataValue() != 0.0:
        gdaltest.post_reason('did not get expected nodata value')
        return 'fail'

    if ds.RasterXSize != 121 or ds.RasterYSize != 121:
        gdaltest.post_reason('did not get expected dimensions')
        return 'fail'

    gt_ref = ds_ref.GetGeoTransform()
    gt = ds.GetGeoTransform()
    for i in range(6):
        if (abs(gt[i]-gt_ref[i])>1e-6):
            gdaltest.post_reason('did not get expected geotransform')
            print(gt)
            print(gt_ref)
            return 'fail'

    wkt = ds.GetProjectionRef()
    if wkt.find("WGS_1984") == -1:
        gdaltest.post_reason('did not get expected SRS')
        print(wkt)
        return 'fail'

    return 'success'

###############################################################################
# Rasterization without georeferencing

def test_gdal_rasterize_lib_100():

    target_ds = gdal.GetDriverByName('MEM').Create( '', 100, 100 )

    # Create a layer to rasterize from.

    vector_ds = \
              gdal.GetDriverByName('Memory').Create( '', 0, 0, 0 )
    rast_lyr = vector_ds.CreateLayer( 'rast1' )

    wkt_geom = 'POLYGON((20 20,20 80,80 80,80 20,20 20))'

    feat = ogr.Feature( rast_lyr.GetLayerDefn() )
    feat.SetGeometryDirectly( ogr.Geometry(wkt = wkt_geom) )

    rast_lyr.CreateFeature( feat )

    ret = gdal.Rasterize(target_ds, vector_ds, burnValues = [255])
    if ret != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # Check results.
    expected = 44190
    checksum = target_ds.GetRasterBand(1).Checksum()
    if checksum != expected:
        print(checksum)
        gdaltest.post_reason( 'Did not get expected image checksum' )

        return 'fail'

    target_ds = None

    return 'success'

###############################################################################
# Rasterization on empty geometry

def test_gdal_rasterize_lib_101():

    target_ds = gdal.GetDriverByName('MEM').Create( '', 100, 100 )

    # Create a layer to rasterize from.

    vector_ds = \
              gdal.GetDriverByName('Memory').Create( '', 0, 0, 0 )
    rast_lyr = vector_ds.CreateLayer( 'rast1' )

    # polygon with empty exterior ring
    geom = ogr.CreateGeometryFromJson('{ "type": "Polygon", "coordinates": [ [ ] ] }')

    feat = ogr.Feature( rast_lyr.GetLayerDefn() )
    feat.SetGeometryDirectly( geom )

    rast_lyr.CreateFeature( feat )

    ret = gdal.Rasterize(target_ds, vector_ds, burnValues = [255])
    if ret != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # Check results.
    checksum = target_ds.GetRasterBand(1).Checksum()
    if checksum != 0:
        print(checksum)
        gdaltest.post_reason( 'Did not get expected image checksum' )

        return 'fail'

    target_ds = None

    return 'success'

###############################################################################
# Rasterization on raster with RPC

def test_gdal_rasterize_lib_102():
    
    target_ds = gdal.GetDriverByName('MEM').Create( '', 353, 226 )
    target_ds.GetRasterBand(1).Fill(255)
    md = {
        'HEIGHT_OFF' : '430',
        'HEIGHT_SCALE' : '501',
        'LAT_OFF' : '-0.0734',
        'LAT_SCALE' : '0.2883',
        'LINE_DEN_COEFF' : '1 0.0002790015 0.001434672 1.481312e-07 5.866139e-06 1.878347e-07 -7.1677e-08 -1.099383e-05 1.968371e-06 -5.50509e-06 0 -1.227539e-08 0 0 2.40682e-07 -1.144941e-08 0 -1.884406e-08 0 0',
        'LINE_NUM_COEFF' : '0.002744972 -0.382552 -1.279674 -0.0001147828 0.001140472 1.262068e-07 -1.69402e-07 -0.005830625 -0.001964747 0 -2.006924e-07 3.066144e-07 3.005069e-06 2.103552e-06 -1.981401e-06 -1.636042e-06 7.045145e-06 -5.699422e-08 1.169591e-07 0',
        'LINE_OFF' : '112.98500331785',
        'LINE_SCALE' : '113.01499668215',
        'LONG_OFF' : '-4.498',
        'LONG_SCALE' : '0.5511',
        'SAMP_DEN_COEFF' : '1 0.001297913 0.0005878427 -0.0004554233 -7.353773e-05 7.928584e-06 -1.826261e-06 9.516839e-05 5.332457e-07 -4.236274e-05 -1.89316e-08 -1.520878e-06 -8.941367e-07 -7.770314e-07 1.413225e-06 9.681702e-08 -4.724849e-08 -2.244317e-07 1.0665e-08 4.212225e-08',
        'SAMP_NUM_COEFF' : '0.01819195 1.091934 -0.1976373 0.003166136 0.002648549 0.0003527143 -6.27017e-05 -0.01889831 -0.0005888535 1.729232e-07 3.037208e-06 0.000174218 -3.129558e-05 -4.602708e-05 2.724941e-05 -9.314161e-07 8.328574e-06 -1.240182e-05 -4.652876e-07 -1.322223e-07',
        'SAMP_OFF' : '176.619681301916',
        'SAMP_SCALE' : '177.068486184099'
    }
    target_ds.SetMetadata(md, "RPC")

    vector_ds = \
              gdal.GetDriverByName('Memory').Create( '', 0, 0, 0 )
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    rast_lyr = vector_ds.CreateLayer( '', srs = sr )

    # polygon with empty exterior ring
    geom = ogr.CreateGeometryFromWkt("""POLYGON ((-3.967081661665 0.0003251483162,-4.976841813513 0.0003251483162,-4.904140485134 0.2151010973779,-4.904124933286 0.2151433982916,-4.904107210626 0.2151848366149,-4.904087364764 0.2152253010321,-4.904065449011 0.2152646828438,-4.904041522241 0.2153028762585,-4.904015648726 0.2153397786774,-4.903987897971 0.2153752909697,-4.903958344523 0.2154093177387,-4.903927067771 0.2154417675784,-4.903894151734 0.2154725533188,-4.903859684834 0.2155015922603,-4.90382375966 0.2155288063955,-4.903786472717 0.2155541226193,-4.903747924169 0.2155774729247,-4.90370821757 0.2155987945858,-4.903667459582 0.2156180303263,-4.903625759694 0.2156351284732,-4.903583229924 0.2156500430959,-4.90353998452 0.2156627341291,-4.903496139651 0.2156731674811,-4.9034518131 0.2156813151247,-4.903407123938 0.2156871551729,-4.903362192216 0.2156906719376,-4.903317138633 0.2156918559717,-4.903272084216 0.2156907040946,-4.903227149995 0.2156872194006,-4.903182456677 0.2156814112505,-3.945022212098 0.0658193544758,-3.94497805444 0.0658112751384,-3.944934372574 0.0658009277184,-3.944891282916 0.0657883397923,-3.944848900303 0.0657735449078,-3.944807337687 0.0657565824944,-3.944766705836 0.0657374977579,-3.944727113036 0.0657163415606,-3.944688664805 0.0656931702851,-3.94465146361 0.0656680456844,-3.944615608594 0.0656410347174,-3.944581195314 0.0656122093701,-3.944548315483 0.065581646464,-3.944517056729 0.0655494274513,-3.944487502358 0.065515638198,-3.944459731134 0.0654803687547,-3.944433817071 0.0654437131168,-3.944409829229 0.0654057689742,-3.94438783154 0.0653666374506,-3.944367882627 0.0653264228341,-3.944350035657 0.0652852322995,-3.944334338192 0.0652431756223,-3.944320832068 0.0652003648864,-3.944309553279 0.0651569141855,-3.944300531884 0.0651129393185,-3.944293791926 0.0650685574815,-3.944289351367 0.0650238869551,-3.944287222041 0.0649790467894,-3.944287409623 0.0649341564864,-3.944289913614 0.0648893356818,-3.94429472734 0.0648447038262,-3.944301837972 0.0648003798665,-3.94431122656 0.064756481929,-3.944322868082 0.0647131270048,-3.944336731513 0.0646704306378,-3.967081661665 0.0003251483162))""")

    feat = ogr.Feature( rast_lyr.GetLayerDefn() )
    feat.SetGeometryDirectly( geom )

    rast_lyr.CreateFeature( feat )

    ret = gdal.Rasterize(target_ds, vector_ds, burnValues = [0])
    if ret != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # Check results.
    checksum = target_ds.GetRasterBand(1).Checksum()
    if checksum != 1604:
        print(checksum)
        gdaltest.post_reason( 'Did not get expected image checksum' )

        return 'fail'


    # Re-try with transformer options
    target_ds.GetRasterBand(1).Fill(255)
    ret = gdal.Rasterize(target_ds, vector_ds, burnValues = [0],
                         transformerOptions = ['RPC_HEIGHT=1000'])
    if ret != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # Check results.
    checksum = target_ds.GetRasterBand(1).Checksum()
    if checksum != 2003:
        print(checksum)
        gdaltest.post_reason( 'Did not get expected image checksum' )

        return 'fail'
    target_ds = None

    return 'success'

###############################################################################
# Simple rasterization with all values of the optim option

def test_gdal_rasterize_lib_4():


    # Setup working spatial reference
    #sr_wkt = 'LOCAL_CS["arbitrary"]'
    #sr = osr.SpatialReference( sr_wkt )
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    sr_wkt = sr.ExportToWkt()

    # Create a raster to rasterize into.
    for optim in [ 'RASTER', 'VECTOR', 'AUTO' ]:
        target_ds = gdal.GetDriverByName('MEM').Create( '', 100, 100, 3,
                                                        gdal.GDT_Byte )
        target_ds.SetGeoTransform( (1000,1,0,1100,0,-1) )
        target_ds.SetProjection( sr_wkt )

        # Create a layer to rasterize from.

        vector_ds = \
                gdal.GetDriverByName('Memory').Create( '', 0, 0, 0 )
        rast_lyr = vector_ds.CreateLayer( 'rast1', srs=sr )

        rast_lyr.GetLayerDefn()
        field_defn = ogr.FieldDefn('foo')
        rast_lyr.CreateField(field_defn)

        # Add a polygon.

        wkt_geom = 'POLYGON((1020 1030,1020 1045,1050 1045,1050 1030,1020 1030))'

        feat = ogr.Feature( rast_lyr.GetLayerDefn() )
        feat.SetGeometryDirectly( ogr.Geometry(wkt = wkt_geom) )

        rast_lyr.CreateFeature( feat )

        # Add feature without geometry to test fix for #3310
        feat = ogr.Feature( rast_lyr.GetLayerDefn() )
        rast_lyr.CreateFeature( feat )

        # Add a linestring.

        wkt_geom = 'LINESTRING(1000 1000, 1100 1050)'

        feat = ogr.Feature( rast_lyr.GetLayerDefn() )
        feat.SetGeometryDirectly( ogr.Geometry(wkt = wkt_geom) )

        rast_lyr.CreateFeature( feat )

        ret = gdal.Rasterize(target_ds, vector_ds, bands = [3,2,1], burnValues = [200,220,240], layers = 'rast1', optim = optim)
        if ret != 1:
            gdaltest.post_reason('fail')
            return 'fail'

        # Check results.
        expected = 6452
        checksum = target_ds.GetRasterBand(2).Checksum()
        if checksum != expected:
            print(checksum, optim)
            gdaltest.post_reason( 'Did not get expected image checksum' )

            return 'fail'

        target_ds = None

    return 'success'

gdaltest_list = [
    test_gdal_rasterize_lib_1,
    test_gdal_rasterize_lib_3,
    test_gdal_rasterize_lib_100,
    test_gdal_rasterize_lib_101,
    test_gdal_rasterize_lib_102,
    test_gdal_rasterize_lib_4
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdal_rasterize_lib' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()





