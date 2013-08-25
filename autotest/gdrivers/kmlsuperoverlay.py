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
# Copyright (c) 2010, Even Rouault <even dot rouault at mines dash paris dot org>
#
# Permission is hereby granted, free of charge, to any person oxyzaining a
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
import struct
from osgeo import gdal
from osgeo import osr
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
    ds = None
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
    ds = gdal.GetDriverByName('KMLSUPEROVERLAY').CreateCopy('/vsimem/kmlsuperoverlay_4.kmz', src_ds, options = ['FORMAT=PNG'])
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
    ds = None
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
# Cleanup

def  kmlsuperoverlay_cleanup():

    return 'success'


gdaltest_list = [
    kmlsuperoverlay_1,
    kmlsuperoverlay_2,
    kmlsuperoverlay_3,
    kmlsuperoverlay_4,
    kmlsuperoverlay_5,
    kmlsuperoverlay_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( ' kmlsuperoverlay' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

