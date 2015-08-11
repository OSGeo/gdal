#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_rasterize testing
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
import os

sys.path.append( '../pymod' )
sys.path.append( '../gcore' )

from osgeo import gdal
import gdaltest
from osgeo import ogr
from osgeo import osr
import test_cli_utilities

###############################################################################
# Simple polygon rasterization (adapted from alg/rasterize.py).

def test_gdal_rasterize_1():

    if test_cli_utilities.get_gdal_rasterize_path() is None:
        return 'skip'

    # Setup working spatial reference
    #sr_wkt = 'LOCAL_CS["arbitrary"]'
    #sr = osr.SpatialReference( sr_wkt )
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    sr_wkt = sr.ExportToWkt()
    
    # Create a raster to rasterize into.

    target_ds = gdal.GetDriverByName('GTiff').Create( 'tmp/rast1.tif', 100, 100, 3,
                                                    gdal.GDT_Byte )
    target_ds.SetGeoTransform( (1000,1,0,1100,0,-1) )
    target_ds.SetProjection( sr_wkt )

    # Close TIF file
    target_ds = None
    
    # Create a layer to rasterize from.

    rast_ogr_ds = \
              ogr.GetDriverByName('MapInfo File').CreateDataSource( 'tmp/rast1.tab' )
    rast_lyr = rast_ogr_ds.CreateLayer( 'rast1', srs=sr )

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

    # Close file
    rast_ogr_ds.Destroy()
    

    # Run the algorithm.
    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_rasterize_path() + ' -b 3 -b 2 -b 1 -burn 200 -burn 220 -burn 240 -l rast1 tmp/rast1.tab tmp/rast1.tif')
    if not (err is None or err == '') :
        gdaltest.post_reason('got error/warning')
        print(err)
        return 'fail'

    # Check results.

    target_ds = gdal.Open('tmp/rast1.tif')
    expected = 6452
    checksum = target_ds.GetRasterBand(2).Checksum()
    if checksum != expected:
        print(checksum)
        gdaltest.post_reason( 'Did not get expected image checksum' )

        return 'fail'
    
    target_ds = None

    return 'success'

###############################################################################
# Test rasterization with ALL_TOUCHED (adapted from alg/rasterize.py).

def test_gdal_rasterize_2():

    if test_cli_utilities.get_gdal_rasterize_path() is None:
        return 'skip'

    # Create a raster to rasterize into.

    target_ds = gdal.GetDriverByName('GTiff').Create( 'tmp/rast2.tif', 12, 12, 3,
                                                    gdal.GDT_Byte )
    target_ds.SetGeoTransform( (0,1,0,12,0,-1) )

    # Close TIF file
    target_ds = None
    
    # Run the algorithm.
    gdaltest.runexternal(test_cli_utilities.get_gdal_rasterize_path() + ' -at -b 3 -b 2 -b 1 -burn 200 -burn 220 -burn 240 -l cutline ../alg/data/cutline.csv tmp/rast2.tif')

    # Check results.

    target_ds = gdal.Open('tmp/rast2.tif')
    expected = 121
    checksum = target_ds.GetRasterBand(2).Checksum()
    if checksum != expected:
        print(checksum)
        gdaltest.post_reason( 'Did not get expected image checksum' )

        return 'fail'
    
    target_ds = None

    return 'success'

###############################################################################
# Test creating an output file

def test_gdal_rasterize_3():

    if test_cli_utilities.get_gdal_contour_path() is None:
        return 'skip'

    if test_cli_utilities.get_gdal_rasterize_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_contour_path() + ' ../gdrivers/data/n43.dt0 tmp/n43dt0.shp -i 10 -3d')

    gdaltest.runexternal(test_cli_utilities.get_gdal_rasterize_path() + ' -3d tmp/n43dt0.shp tmp/n43dt0.tif -l n43dt0 -ts 121 121 -a_nodata 0 -q')

    ds_ref = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.Open('tmp/n43dt0.tif')

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
# Same but with -tr argument

def test_gdal_rasterize_4():

    if test_cli_utilities.get_gdal_contour_path() is None:
        return 'skip'

    if test_cli_utilities.get_gdal_rasterize_path() is None:
        return 'skip'

    gdal.GetDriverByName('GTiff').Delete( 'tmp/n43dt0.tif' )

    gdaltest.runexternal(test_cli_utilities.get_gdal_rasterize_path() + ' -3d tmp/n43dt0.shp tmp/n43dt0.tif -l n43dt0 -tr 0.008333333333333  0.008333333333333 -a_nodata 0 -a_srs EPSG:4326')

    ds_ref = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.Open('tmp/n43dt0.tif')

    if ds.GetRasterBand(1).GetNoDataValue() != 0.0:
        gdaltest.post_reason('did not get expected nodata value')
        return 'fail'

    # Allow output to grow by 1/2 cell, as per #6058
    if ds.RasterXSize != 122 or ds.RasterYSize != 122:
        gdaltest.post_reason('did not get expected dimensions')
        return 'fail'

    gt_ref = ds_ref.GetGeoTransform()
    gt = ds.GetGeoTransform()
    if abs(gt[1] - gt_ref[1]) > 1e-6 or abs(gt[5] - gt_ref[5]) > 1e-6:
        gdaltest.post_reason('did not get expected geotransform(dx/dy)')
        print(gt)
        print(gt_ref)
        return 'fail'

    # Allow output to grow by 1/2 cell, as per #6058
    if abs(gt[0] + (gt[1] / 2) - gt_ref[0]) > 1e-6 or \
       abs(gt[3] + (gt[5] / 2) - gt_ref[3]) > 1e-6 :
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
# Test point rasterization (#3774)

def test_gdal_rasterize_5():

    if test_cli_utilities.get_gdal_rasterize_path() is None:
        return 'skip'

    f = open('tmp/test_gdal_rasterize_5.csv', 'wb')
    f.write("""x,y,Value
0.5,0.5,1
0.5,2.5,2
2.5,2.5,3
2.5,0.5,4
1.5,1.5,5""".encode('ascii'))
    f.close()

    f = open('tmp/test_gdal_rasterize_5.vrt', 'wb')
    f.write("""<OGRVRTDataSource>
    <OGRVRTLayer name="test">
        <SrcDataSource relativetoVRT="1">test_gdal_rasterize_5.csv</SrcDataSource>
        <SrcLayer>test_gdal_rasterize_5</SrcLayer>
        <GeometryType>wkbPoint</GeometryType>
        <GeometryField encoding="PointFromColumns" x="x" y="y"/>
    </OGRVRTLayer>
</OGRVRTDataSource>""".encode('ascii'))
    f.close()

    gdaltest.runexternal(test_cli_utilities.get_gdal_rasterize_path() + ' -l test tmp/test_gdal_rasterize_5.vrt tmp/test_gdal_rasterize_5.tif -a Value -tr 1 1 -ot Byte')

    ds = gdal.Open('tmp/test_gdal_rasterize_5.tif')
    if ds.RasterXSize != 3 or ds.RasterYSize != 3:
        gdaltest.post_reason('did not get expected dimensions')
        print(ds.RasterXSize)
        print(ds.RasterYSize)
        return 'fail'

    gt_ref = [0,1,0,3,0,-1]
    gt = ds.GetGeoTransform()
    for i in range(6):
        if (abs(gt[i]-gt_ref[i])>1e-6):
            gdaltest.post_reason('did not get expected geotransform')
            print(gt)
            print(gt_ref)
            return 'fail'

    data = ds.GetRasterBand(1).ReadRaster(0, 0, 3, 3)
    if data.decode('iso-8859-1') != '\x02\x00\x03\x00\x05\x00\x01\x00\x04':
        gdaltest.post_reason('did not get expected values')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test on the fly reprojection of input data

def test_gdal_rasterize_6():

    if test_cli_utilities.get_gdal_rasterize_path() is None:
        return 'skip'

    f = open('tmp/test_gdal_rasterize_6.csv', 'wb')
    f.write("""WKT,Value
"POLYGON((2 49,2 50,3 50,3 49,2 49))",255
""".encode('ascii'))
    f.close()
    
    f = open('tmp/test_gdal_rasterize_6.prj', 'wb')
    f.write("""EPSG:4326""".encode('ascii'))
    f.close()
    
    ds = gdal.GetDriverByName('GTiff').Create('tmp/test_gdal_rasterize_6.tif', 100, 100)
    ds.SetGeoTransform([200000,(400000-200000)/100,0,6500000,0,-(6500000-6200000)/100])
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(3857)
    ds.SetProjection(sr.ExportToWkt())
    ds = None

    gdaltest.runexternal(test_cli_utilities.get_gdal_rasterize_path() + ' -l test_gdal_rasterize_6 tmp/test_gdal_rasterize_6.csv tmp/test_gdal_rasterize_6.tif -a Value')

    ds = gdal.Open('tmp/test_gdal_rasterize_6.tif')
    if ds.GetRasterBand(1).Checksum() != 39190:
        gdaltest.post_reason('did not get expected checksum')
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test SQLITE dialect in SQL

def test_gdal_rasterize_7():

    if test_cli_utilities.get_gdal_rasterize_path() is None:
        return 'skip'

    drv = ogr.GetDriverByName('SQLite')
    if drv is None:
        return 'skip'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = drv.CreateDataSource('/vsimem/foo.db', options = ['SPATIALITE=YES'])
    if ds is None:
        return 'skip'
    ds = None
    gdal.Unlink('/vsimem/foo.db')
    gdal.PopErrorHandler()

    f = open('tmp/test_gdal_rasterize_7.csv', 'wb')
    x = (0, 0, 50, 50, 25)
    y = (0, 50, 0, 50, 25)
    f.write('WKT,Value\n'.encode('ascii'))
    for i in range(len(x)):
        r = 'POINT(%d %d),1\n' % (x[i], y[i])
        f.write(r.encode('ascii'))

    f.close()

    cmds = '''tmp/test_gdal_rasterize_7.csv
              tmp/test_gdal_rasterize_7.tif
              -init 0 -burn 1
              -sql "SELECT ST_Buffer(GEOMETRY, 2) FROM test_gdal_rasterize_7"
              -dialect sqlite -tr 1 1 -te -1 -1 51 51'''

    gdaltest.runexternal(test_cli_utilities.get_gdal_rasterize_path() + ' ' + cmds)

    ds = gdal.Open('tmp/test_gdal_rasterize_7.tif')
    data = ds.GetRasterBand(1).ReadAsArray()
    if data.sum() <= 5:
        gdaltest.post_reason('Only rasterized 5 pixels or less.')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Make sure we create output that encompasses all the input points on a point
# layer, #6058.

def test_gdal_rasterize_8():

    if test_cli_utilities.get_gdal_rasterize_path() is None:
        return 'skip'

    f = open('tmp/test_gdal_rasterize_8.csv', 'wb')
    f.write('WKT,Value\n'.encode('ascii'))
    f.write('"LINESTRING (0 0, 5 5, 10 0, 10 10)",1'.encode('ascii'))
    f.close()

    cmds = '''tmp/test_gdal_rasterize_8.csv tmp/test_gdal_rasterize_8.tif -init 0 -burn 1 -tr 1 1'''

    gdaltest.runexternal(test_cli_utilities.get_gdal_rasterize_path() + ' ' + cmds)

    ds = gdal.Open('tmp/test_gdal_rasterize_8.tif')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 21:
        gdaltest.post_reason('Did not rasterize line data properly')
        print(cs)
        return 'fail'

    ds = None

    return 'success'


###########################################
def test_gdal_rasterize_cleanup():

    if test_cli_utilities.get_gdal_rasterize_path() is None:
        return 'skip'

    gdal.GetDriverByName('GTiff').Delete( 'tmp/rast1.tif' )
    ogr.GetDriverByName('MapInfo File').DeleteDataSource( 'tmp/rast1.tab' )

    gdal.GetDriverByName('GTiff').Delete( 'tmp/rast2.tif' )

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource( 'tmp/n43dt0.shp' )
    gdal.GetDriverByName('GTiff').Delete( 'tmp/n43dt0.tif' )

    gdal.GetDriverByName('GTiff').Delete( 'tmp/test_gdal_rasterize_5.tif' )
    os.unlink('tmp/test_gdal_rasterize_5.csv')
    os.unlink('tmp/test_gdal_rasterize_5.vrt')

    gdal.GetDriverByName('GTiff').Delete( 'tmp/test_gdal_rasterize_6.tif' )
    os.unlink('tmp/test_gdal_rasterize_6.csv')
    os.unlink('tmp/test_gdal_rasterize_6.prj')

    gdal.GetDriverByName('GTiff').Delete( 'tmp/test_gdal_rasterize_7.tif' )
    if os.path.exists('tmp/test_gdal_rasterize_7.csv'):
        os.unlink('tmp/test_gdal_rasterize_7.csv')

    gdal.GetDriverByName('GTiff').Delete( 'tmp/test_gdal_rasterize_8.tif' )
    os.unlink('tmp/test_gdal_rasterize_8.csv')

    return 'success'

gdaltest_list = [
    test_gdal_rasterize_1,
    test_gdal_rasterize_2,
    test_gdal_rasterize_3,
    test_gdal_rasterize_4,
    test_gdal_rasterize_5,
    test_gdal_rasterize_6,
    test_gdal_rasterize_7,
    test_gdal_rasterize_8,
    test_gdal_rasterize_cleanup
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdal_rasterize' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()





