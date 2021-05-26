#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test write functionality for KMLSUPEROVERLAY driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys.com>
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
import shutil
from osgeo import gdal


import gdaltest
import pytest

###############################################################################
# Test CreateCopy() to a KMZ file


def test_kmlsuperoverlay_1():

    tst = gdaltest.GDALTest('KMLSUPEROVERLAY', 'small_world.tif', 1, 30111, options=['FORMAT=PNG'])

    return tst.testCreateCopy(new_filename='/vsimem/kmlout.kmz')

###############################################################################
# Test CreateCopy() to a KML file


def test_kmlsuperoverlay_2():

    tst = gdaltest.GDALTest('KMLSUPEROVERLAY', 'small_world.tif', 1, 30111, options=['FORMAT=PNG'])

    return tst.testCreateCopy(new_filename='/vsimem/kmlout.kml')

###############################################################################
# Test CreateCopy() to a KML file


def test_kmlsuperoverlay_3():

    src_ds = gdal.Open('data/utm.tif')
    ds = gdal.GetDriverByName('KMLSUPEROVERLAY').CreateCopy('tmp/tmp.kml', src_ds)
    del ds
    src_ds = None

    f = gdal.VSIFOpenL('tmp/tmp.kml', 'rb')
    if f:
        data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
        gdal.VSIFCloseL(f)
    assert '<north>33.903' in data, data
    assert '<south>33.625' in data, data
    assert '<east>-117.309' in data, data
    assert '<west>-117.639' in data, data

    filelist = ['tmp/0/0/0.jpg',
                'tmp/0/0/0.kml',
                'tmp/1/0/0.jpg',
                'tmp/1/0/0.kml',
                'tmp/1/0/1.jpg',
                'tmp/1/0/1.kml',
                'tmp/1/1/0.jpg',
                'tmp/1/1/0.kml',
                'tmp/1/1/1.jpg',
                'tmp/1/1/1.kml',
                'tmp/tmp.kml']
    for filename in filelist:
        try:
            os.remove(filename)
        except OSError:
            pytest.fail("Missing file: %s" % filename)

    shutil.rmtree('tmp/0')
    shutil.rmtree('tmp/1')

###############################################################################
# Test overviews


def test_kmlsuperoverlay_4():

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
    ds = gdal.GetDriverByName('KMLSUPEROVERLAY').CreateCopy('/vsimem/kmlsuperoverlay_4.kmz', src_ds, options=['FORMAT=PNG', 'NAME=myname', 'DESCRIPTION=mydescription', 'ALTITUDE=10', 'ALTITUDEMODE=absolute'])
    assert ds.GetMetadataItem('NAME') == 'myname'
    assert ds.GetMetadataItem('DESCRIPTION') == 'mydescription'
    if ds.GetRasterBand(1).GetOverviewCount() != 1:
        ds = None
        src_ds = None
        gdal.Unlink("/vsimem/src.vrt")
        gdal.Unlink("/vsimem/kmlsuperoverlay_4.kmz")
        pytest.fail()
    if ds.GetRasterBand(1).GetOverview(0).Checksum() != 30111:
        ds = None
        src_ds = None
        gdal.Unlink("/vsimem/src.vrt")
        gdal.Unlink("/vsimem/kmlsuperoverlay_4.kmz")
        pytest.fail()
    if ds.GetRasterBand(1).Checksum() != src_ds.GetRasterBand(1).Checksum():
        ds = None
        src_ds = None
        gdal.Unlink("/vsimem/src.vrt")
        gdal.Unlink("/vsimem/kmlsuperoverlay_4.kmz")
        pytest.fail()

    # Test fix for #6311
    vrt_ds = gdal.GetDriverByName('VRT').CreateCopy('', ds)
    got_data = vrt_ds.ReadRaster(0, 0, 800, 400, 200, 100)
    ref_data = ds.ReadRaster(0, 0, 800, 400, 200, 100)
    vrt_ds = None
    if got_data != ref_data:
        ds = None
        src_ds = None
        gdal.Unlink("/vsimem/src.vrt")
        gdal.Unlink("/vsimem/kmlsuperoverlay_4.kmz")
        pytest.fail()

    ds = None
    src_ds = None

    gdal.Unlink("/vsimem/src.vrt")
    gdal.Unlink("/vsimem/kmlsuperoverlay_4.kmz")

###############################################################################
# Test that a raster which crosses the anti-meridian will be able to be displayed correctly (#4528)


def test_kmlsuperoverlay_5():

    from xml.etree import ElementTree

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

    for f in files:
        res = ElementTree.parse(f)
        for tag in res.findall('.//{http://earth.google.com/kml/2.1}LatLonAltBox'):
            east = tag.find('{http://earth.google.com/kml/2.1}east').text
            west = tag.find('{http://earth.google.com/kml/2.1}west').text

            assert float(east) >= float(west), \
                ('East is less than west in LatLonAltBox %s, (%s < %s)' % (f, east, west))

    shutil.rmtree('tmp/0')
    shutil.rmtree('tmp/1')
    os.remove('tmp/tmp.kml')

###############################################################################
# Test raster KML with alternate structure (such as http://opentopo.sdsc.edu/files/Haiti/NGA_Haiti_LiDAR2.kmz))


def test_kmlsuperoverlay_6():

    ds = gdal.Open('data/kml/kmlimage.kmz')
    assert ds.GetProjectionRef().find('WGS_1984') >= 0
    got_gt = ds.GetGeoTransform()
    ref_gt = [1.2554125761846773, 1.6640895429971981e-05, 0.0, 43.452120815728101, 0.0, -1.0762348187666334e-05]
    for i in range(6):
        assert got_gt[i] == pytest.approx(ref_gt[i], abs=1e-6)
    for i in range(4):
        cs = ds.GetRasterBand(i + 1).Checksum()
        assert cs == 47673
        assert ds.GetRasterBand(i + 1).GetRasterColorInterpretation() == gdal.GCI_RedBand + i
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 61070

###############################################################################
# Test raster KML with single Overlay (such as https://trac.osgeo.org/gdal/ticket/6712)


def test_kmlsuperoverlay_7():

    ds = gdal.Open('data/kml/small_world.kml')
    assert ds.GetProjectionRef().find('WGS_1984') >= 0
    got_gt = ds.GetGeoTransform()
    ref_gt = [-180.0, 0.9, 0.0, 90.0, 0.0, -0.9]
    for i in range(6):
        assert got_gt[i] == pytest.approx(ref_gt[i], abs=1e-6)

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 30111
    assert ds.GetRasterBand(1).GetRasterColorInterpretation() == gdal.GCI_RedBand

###############################################################################
# Test raster KML with single Overlay (such as https://issues.qgis.org/issues/20173)


def test_kmlsuperoverlay_single_overlay_document_folder_pct():

    ds = gdal.Open('data/kml/small_world_in_document_folder_pct.kml')
    assert ds.GetProjectionRef().find('WGS_1984') >= 0
    got_gt = ds.GetGeoTransform()
    ref_gt = [-180.0, 0.9, 0.0, 90.0, 0.0, -0.9]
    for i in range(6):
        assert got_gt[i] == pytest.approx(ref_gt[i], abs=1e-6)

    assert ds.GetRasterBand(1).GetRasterColorInterpretation() == gdal.GCI_PaletteIndex
    assert ds.GetRasterBand(1).GetColorTable()

###############################################################################
# Test that a raster with lots of blank space doesn't have unnecessary child
# KML/PNG files in transparent areas


def test_kmlsuperoverlay_8():

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

    assert set(os.listdir('tmp/0/0')) == set(('0.kml', '0.png'))
    assert (set(os.listdir('tmp/3/1')) == set(('0.jpg', '0.kml', '1.jpg', '1.kml', '2.jpg', '2.kml', '3.jpg', '3.kml',
                                          '4.jpg', '4.kml', '5.jpg', '5.kml', '6.jpg', '6.kml', '7.jpg', '7.kml',)))
    assert set(os.listdir('tmp/3/2')) == set()

    shutil.rmtree('tmp/0')
    shutil.rmtree('tmp/1')
    shutil.rmtree('tmp/2')
    shutil.rmtree('tmp/3')
    os.remove('tmp/tmp.kml')

###############################################################################
# Cleanup


def test_kmlsuperoverlay_cleanup():

    gdal.Unlink('/vsimem/0/0/0.png')
    gdal.Unlink('/vsimem/0/0/0.kml')
    gdal.Unlink('/vsimem/0/0')
    gdal.Unlink('/vsimem/0')
    gdal.Unlink('/vsimem/kmlout.kml')
    gdal.Unlink('/vsimem/kmlout.kmz')



