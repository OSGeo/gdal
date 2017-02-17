#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test write functionality for KMLSUPEROVERLAY driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2010-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
from osgeo import gdal
import shutil

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Test CreateCopy() to a KMZ file

def kmlsuperoverlay_1():

    tst = gdaltest.GDALTest( 'KMLSUPEROVERLAY', 'small_world.tif', 1, 30111, options = [ 'FORMAT=PNG'] )

    return tst.testCreateCopy( new_filename = '/vsimem/kmlout.kmz' )

###############################################################################
# Test CreateCopy() to a KML file

def kmlsuperoverlay_2():

    tst = gdaltest.GDALTest( 'KMLSUPEROVERLAY', 'small_world.tif', 1, 30111, options = [ 'FORMAT=PNG'] )

    return tst.testCreateCopy( new_filename = '/vsimem/kmlout.kml' )

###############################################################################
# Test CreateCopy() to a KML file

def kmlsuperoverlay_3():

    src_ds = gdal.Open('data/utm.tif')
    ds = gdal.GetDriverByName('KMLSUPEROVERLAY').CreateCopy('tmp/tmp.kml', src_ds)
    del ds
    src_ds = None

    filelist = [ 'tmp/0/0/0.jpg',
                 'tmp/0/0/0.kml',
                 'tmp/1/0/0.jpg',
                 'tmp/1/0/0.kml',
                 'tmp/1/0/1.jpg',
                 'tmp/1/0/1.kml',
                 'tmp/1/1/0.jpg',
                 'tmp/1/1/0.kml',
                 'tmp/1/1/1.jpg',
                 'tmp/1/1/1.kml',
                 'tmp/tmp.kml' ]
    for filename in filelist:
        try:
            os.remove(filename)
        except:
            gdaltest.post_reason("Missing file: %s" % filename)
            return 'fail'

    shutil.rmtree('tmp/0')
    shutil.rmtree('tmp/1')

    return 'success'

###############################################################################
# Test overviews

def kmlsuperoverlay_4():

    vrt_xml = """<VRTDataset rasterXSize="800" rasterYSize="400">
  <SRS>GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4326"]]</SRS>
  <GeoTransform> -1.8000000000000000e+02,  4.5000000000000001e-01,  0.0000000000000000e+00,  9.0000000000000000e+01,  0.0000000000000000e+00, -4.5000000000000001e-01</GeoTransform>
  <Metadata>
    <MDI key="AREA_OR_POINT">Area</MDI>
  </Metadata>
  <Metadata domain="IMAGE_STRUCTURE">
    <MDI key="INTERLEAVE">BAND</MDI>
  </Metadata>
  <VRTRasterBand dataType="Byte" band="1">
    <Metadata />
    <ColorInterp>Red</ColorInterp>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/small_world.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="400" RasterYSize="200" DataType="Byte" BlockXSize="400" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="400" ySize="200" />
      <DstRect xOff="0" yOff="0" xSize="800" ySize="400" />
    </SimpleSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="2">
    <Metadata />
    <ColorInterp>Green</ColorInterp>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/small_world.tif</SourceFilename>
      <SourceBand>2</SourceBand>
      <SourceProperties RasterXSize="400" RasterYSize="200" DataType="Byte" BlockXSize="400" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="400" ySize="200" />
      <DstRect xOff="0" yOff="0" xSize="800" ySize="400" />
    </SimpleSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="3">
    <Metadata />
    <ColorInterp>Blue</ColorInterp>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/small_world.tif</SourceFilename>
      <SourceBand>3</SourceBand>
      <SourceProperties RasterXSize="400" RasterYSize="200" DataType="Byte" BlockXSize="400" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="400" ySize="200" />
      <DstRect xOff="0" yOff="0" xSize="800" ySize="400" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>"""

    gdal.FileFromMemBuffer("/vsimem/src.vrt", vrt_xml)

    src_ds = gdal.Open("/vsimem/src.vrt")
    ds = gdal.GetDriverByName('KMLSUPEROVERLAY').CreateCopy('/vsimem/kmlsuperoverlay_4.kmz', src_ds, options = ['FORMAT=PNG', 'NAME=myname', 'DESCRIPTION=mydescription', 'ALTITUDE=10', 'ALTITUDEMODE=absolute'])
    if ds.GetMetadataItem('NAME') != 'myname':
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetMetadataItem('DESCRIPTION') != 'mydescription':
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetOverviewCount() != 1:
        gdaltest.post_reason('fail')
        ds = None
        src_ds = None
        gdal.Unlink("/vsimem/src.vrt")
        gdal.Unlink("/vsimem/kmlsuperoverlay_4.kmz")
        return 'fail'
    if ds.GetRasterBand(1).GetOverview(0).Checksum() != 30111:
        gdaltest.post_reason('fail')
        ds = None
        src_ds = None
        gdal.Unlink("/vsimem/src.vrt")
        gdal.Unlink("/vsimem/kmlsuperoverlay_4.kmz")
        return 'fail'
    if ds.GetRasterBand(1).Checksum() != src_ds.GetRasterBand(1).Checksum():
        gdaltest.post_reason('fail')
        ds = None
        src_ds = None
        gdal.Unlink("/vsimem/src.vrt")
        gdal.Unlink("/vsimem/kmlsuperoverlay_4.kmz")
        return 'fail'

    # Test fix for #6311
    vrt_ds = gdal.GetDriverByName('VRT').CreateCopy('', ds)
    got_data = vrt_ds.ReadRaster(0,0,800,400,200,100)
    ref_data = ds.ReadRaster(0,0,800,400,200,100)
    vrt_ds = None
    if got_data != ref_data:
        gdaltest.post_reason('fail')
        ds = None
        src_ds = None
        gdal.Unlink("/vsimem/src.vrt")
        gdal.Unlink("/vsimem/kmlsuperoverlay_4.kmz")
        return 'fail'

    ds = None
    src_ds = None

    gdal.Unlink("/vsimem/src.vrt")
    gdal.Unlink("/vsimem/kmlsuperoverlay_4.kmz")

    return 'success'

###############################################################################
# Test that a raster which crosses the anti-meridian will be able to be displayed correctly (#4528)

def kmlsuperoverlay_5():

    try:
        from xml.etree import ElementTree
    except:
        return 'skip'

    src_ds = gdal.Open("""<VRTDataset rasterXSize="512" rasterYSize="512">
  <SRS>PROJCS["WGS 84 / Mercator 41",
    GEOGCS["WGS 84",
        DATUM["WGS_1984",
            SPHEROID["WGS 84",6378137,298.257223563,
                AUTHORITY["EPSG","7030"]],
            AUTHORITY["EPSG","6326"]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433],
        AUTHORITY["EPSG","4326"]],
    PROJECTION["Mercator_1SP"],
    PARAMETER["central_meridian",100],
    PARAMETER["scale_factor",1],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AUTHORITY["EPSG","3994"]]
</SRS>
  <GeoTransform>  8.8060000000000000e+06,  3.4960937500000000e+02,  0.0000000000000000e+00, -3.6200000000000000e+06,  0.0000000000000000e+00, -3.5937500000000000e+02</GeoTransform>
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="1">data/utm.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="512" ySize="512" />
      <DstRect xOff="0" yOff="0" xSize="512" ySize="512" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")
    ds = gdal.GetDriverByName('KMLSUPEROVERLAY').CreateCopy('tmp/tmp.kml', src_ds, options=['FIX_ANTIMERIDIAN=YES'])
    del ds
    src_ds = None

    files = [
        'tmp/tmp.kml',
        'tmp/0/0/0.kml',
        'tmp/1/0/0.kml',
        'tmp/1/0/1.kml',
        'tmp/1/1/0.kml',
        'tmp/1/1/1.kml',
    ]

    for file in files:
        res = ElementTree.parse(file)
        for tag in res.findall('.//{http://earth.google.com/kml/2.1}LatLonAltBox'):
            east = tag.find('{http://earth.google.com/kml/2.1}east').text
            west = tag.find('{http://earth.google.com/kml/2.1}west').text

            if float(east) < float(west):
                gdaltest.post_reason('East is less than west in LatLonAltBox %s, (%s < %s)' % (file, east, west))
                return 'fail'

    shutil.rmtree('tmp/0')
    shutil.rmtree('tmp/1')
    os.remove('tmp/tmp.kml')

    return 'success'

###############################################################################
# Test raster KML with alternate structure (such as http://opentopo.sdsc.edu/files/Haiti/NGA_Haiti_LiDAR2.kmz))

def kmlsuperoverlay_6():

    ds = gdal.Open('data/kmlimage.kmz')
    if ds.GetProjectionRef().find('WGS_1984') < 0:
        gdaltest.post_reason('failure')
        return 'fail'
    got_gt = ds.GetGeoTransform()
    ref_gt = [ 1.2554125761846773, 1.6640895429971981e-05, 0.0, 43.452120815728101, 0.0, -1.0762348187666334e-05 ]
    for i in range(6):
        if abs(got_gt[i] - ref_gt[i]) > 1e-6:
            gdaltest.post_reason('failure')
            print(got_gt)
            return 'fail'
    for i in range(4):
        cs = ds.GetRasterBand(i+1).Checksum()
        if cs != 47673:
            print(cs)
            gdaltest.post_reason('failure')
            return 'fail'
        if ds.GetRasterBand(i+1).GetRasterColorInterpretation() != gdal.GCI_RedBand + i:
            gdaltest.post_reason('failure')
            return 'fail'
    if ds.GetRasterBand(1).GetOverviewCount() != 1:
        gdaltest.post_reason('failure')
        return 'fail'
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    if cs != 61070:
        print(cs)
        gdaltest.post_reason('failure')
        return 'fail'

    return 'success'

###############################################################################
# Test raster KML with single Overlay (such as https://trac.osgeo.org/gdal/ticket/6712)

def kmlsuperoverlay_7():

    ds = gdal.Open('data/small_world.kml')
    if ds.GetProjectionRef().find('WGS_1984') < 0:
        gdaltest.post_reason('failure')
        return 'fail'
    got_gt = ds.GetGeoTransform()
    ref_gt = [ -180.0, 0.9, 0.0, 90.0, 0.0, -0.9 ]
    for i in range(6):
        if abs(got_gt[i] - ref_gt[i]) > 1e-6:
            gdaltest.post_reason('failure')
            print(got_gt)
            return 'fail'

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 30111:
        print(cs)
        gdaltest.post_reason('failure')
        return 'fail'
    if ds.GetRasterBand(1).GetRasterColorInterpretation() != gdal.GCI_RedBand:
        gdaltest.post_reason('failure')
        return 'fail'

    return 'success'

###############################################################################
# Test that a raster with lots of blank space doesn't have unnecessary child
# KML/PNG files in transparent areas

def kmlsuperoverlay_8():

    # a large raster with actual data on each end and blank space in between
    src_ds = gdal.Open("""<VRTDataset rasterXSize="2048" rasterYSize="512">
  <SRS>GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9108"]],AUTHORITY["EPSG","4326"]]</SRS>
  <GeoTransform>  0,  0.01,  0,  0,  0, 0.01</GeoTransform>
  <VRTRasterBand dataType="Byte" band="1">
    <ColorInterp>Gray</ColorInterp>
    <SimpleSource>
      <SourceFilename relativeToVRT="1">data/utm.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="512" RasterYSize="512" DataType="Byte" BlockXSize="512" BlockYSize="16" />
      <SrcRect xOff="0" yOff="0" xSize="512" ySize="512" />
      <DstRect xOff="0" yOff="0" xSize="512" ySize="512" />
    </SimpleSource>
    <SimpleSource>
      <SourceFilename relativeToVRT="1">data/utm.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="512" RasterYSize="512" DataType="Byte" BlockXSize="512" BlockYSize="16" />
      <SrcRect xOff="0" yOff="0" xSize="512" ySize="512" />
      <DstRect xOff="1536" yOff="0" xSize="512" ySize="512" />
    </SimpleSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="2">
    <ColorInterp>Gray</ColorInterp>
    <SimpleSource>
      <SourceFilename relativeToVRT="1">data/utm.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="512" RasterYSize="512" DataType="Byte" BlockXSize="512" BlockYSize="16" />
      <SrcRect xOff="0" yOff="0" xSize="512" ySize="512" />
      <DstRect xOff="0" yOff="0" xSize="512" ySize="512" />
    </SimpleSource>
    <SimpleSource>
      <SourceFilename relativeToVRT="1">data/utm.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="512" RasterYSize="512" DataType="Byte" BlockXSize="512" BlockYSize="16" />
      <SrcRect xOff="0" yOff="0" xSize="512" ySize="512" />
      <DstRect xOff="1536" yOff="0" xSize="512" ySize="512" />
    </SimpleSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="3">
    <ColorInterp>Gray</ColorInterp>
    <SimpleSource>
      <SourceFilename relativeToVRT="1">data/utm.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="512" RasterYSize="512" DataType="Byte" BlockXSize="512" BlockYSize="16" />
      <SrcRect xOff="0" yOff="0" xSize="512" ySize="512" />
      <DstRect xOff="0" yOff="0" xSize="512" ySize="512" />
    </SimpleSource>
    <SimpleSource>
      <SourceFilename relativeToVRT="1">data/utm.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="512" RasterYSize="512" DataType="Byte" BlockXSize="512" BlockYSize="16" />
      <SrcRect xOff="0" yOff="0" xSize="512" ySize="512" />
      <DstRect xOff="1536" yOff="0" xSize="512" ySize="512" />
    </SimpleSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="4">
    <ColorInterp>Alpha</ColorInterp>
    <ComplexSource>
      <SourceFilename relativeToVRT="1">data/utm.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="512" RasterYSize="512" DataType="Byte" BlockXSize="512" BlockYSize="16" />
      <SrcRect xOff="0" yOff="0" xSize="512" ySize="512" />
      <DstRect xOff="0" yOff="0" xSize="512" ySize="512" />
      <ScaleOffset>255</ScaleOffset>
      <ScaleRatio>0</ScaleRatio>
    </ComplexSource>
    <ComplexSource>
      <SourceFilename relativeToVRT="1">data/utm.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="512" RasterYSize="512" DataType="Byte" BlockXSize="512" BlockYSize="16" />
      <SrcRect xOff="0" yOff="0" xSize="512" ySize="512" />
      <DstRect xOff="1536" yOff="0" xSize="512" ySize="512" />
      <ScaleOffset>255</ScaleOffset>
      <ScaleRatio>0</ScaleRatio>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>""")
    ds = gdal.GetDriverByName('KMLSUPEROVERLAY').CreateCopy('tmp/tmp.kml', src_ds, options=['FORMAT=AUTO'])
    del ds
    src_ds = None

    if set(os.listdir('tmp/0/0')) != set(('0.kml', '0.png')):
      gdaltest.post_reason('failure')
      return 'fail'
    if set(os.listdir('tmp/3/1')) != set(('0.jpg', '0.kml', '1.jpg', '1.kml', '2.jpg', '2.kml', '3.jpg', '3.kml',
        '4.jpg', '4.kml', '5.jpg', '5.kml', '6.jpg', '6.kml', '7.jpg', '7.kml',)):
      gdaltest.post_reason('failure')
      return 'fail'
    if set(os.listdir('tmp/3/2')) != set():
      # dir should be empty - 3/2 is entirely transparent so we skip generating files.
      gdaltest.post_reason('failure')
      return 'fail'

    shutil.rmtree('tmp/0')
    shutil.rmtree('tmp/1')
    shutil.rmtree('tmp/2')
    shutil.rmtree('tmp/3')
    os.remove('tmp/tmp.kml')

    return 'success'

###############################################################################
# Cleanup

def  kmlsuperoverlay_cleanup():

    return 'success'


gdaltest_list = [
    kmlsuperoverlay_1,
    kmlsuperoverlay_2,
    kmlsuperoverlay_3,
    kmlsuperoverlay_4,
    kmlsuperoverlay_5,
    kmlsuperoverlay_6,
    kmlsuperoverlay_7,
    kmlsuperoverlay_8,
    kmlsuperoverlay_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( ' kmlsuperoverlay' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

