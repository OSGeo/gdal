#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  WMTS driver test suite.
# Author:   Even Rouault, even dot rouault at spatialys.com
#
###############################################################################
# Copyright (c) 2015, Even Rouault <even dot rouault at spatialys.com>
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

import shutil


from osgeo import gdal

import gdaltest
import pytest
import struct

###############################################################################
# Find WMTS driver


def test_wmts_1():

    gdaltest.wmts_drv = gdal.GetDriverByName('WMTS')

    if gdaltest.wmts_drv is not None and gdal.GetDriverByName('WMS') is None:
        print('Missing WMS driver')
        gdaltest.wmts_drv = None

    if gdaltest.wmts_drv is not None:

        gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')
        gdal.SetConfigOption('GDAL_DEFAULT_WMS_CACHE_PATH', '/vsimem/cache')

        return
    pytest.skip()

###############################################################################
# Error: no URL and invalid GDAL_WMTS service file documents


def test_wmts_2():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    gdal.PushErrorHandler()
    ds = gdal.Open('WMTS:')
    gdal.PopErrorHandler()
    assert ds is None

    gdal.PushErrorHandler()
    ds = gdal.Open('<GDAL_WMTS>')
    gdal.PopErrorHandler()
    assert ds is None

    gdal.PushErrorHandler()
    ds = gdal.Open('<GDAL_WMTSxxx/>')
    gdal.PopErrorHandler()
    assert ds is None

    gdal.PushErrorHandler()
    ds = gdal.Open('<GDAL_WMTS></GDAL_WMTS>')
    gdal.PopErrorHandler()
    assert ds is None

###############################################################################
# Error: invalid URL


def test_wmts_3():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    gdal.PushErrorHandler()
    ds = gdal.Open('WMTS:https://non_existing')
    gdal.PopErrorHandler()
    assert ds is None

###############################################################################
# Error: invalid URL


def test_wmts_4():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    gdal.PushErrorHandler()
    ds = gdal.Open('WMTS:/vsimem/non_existing')
    gdal.PopErrorHandler()
    assert ds is None

###############################################################################
# Error: invalid XML in GetCapabilities response


def test_wmts_5():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/invalid_getcapabilities.xml', '<invalid_xml')

    gdal.PushErrorHandler()
    ds = gdal.Open('WMTS:/vsimem/invalid_getcapabilities.xml')
    gdal.PopErrorHandler()
    assert ds is None

###############################################################################
# Error: invalid content in GetCapabilities response


def test_wmts_6():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/invalid_getcapabilities.xml', '<Capabilities/>')

    gdal.PushErrorHandler()
    ds = gdal.Open('WMTS:/vsimem/invalid_getcapabilities.xml')
    gdal.PopErrorHandler()
    assert ds is None

###############################################################################
# Error: no layers


def test_wmts_7():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/empty_getcapabilities.xml', '<Capabilities><Contents/></Capabilities>')

    gdal.PushErrorHandler()
    ds = gdal.Open('WMTS:/vsimem/empty_getcapabilities.xml')
    gdal.PopErrorHandler()
    assert ds is None

###############################################################################
# Error: missing TileMatrixSetLink and Style


def test_wmts_8():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/missing.xml', """<Capabilities>
    <Contents>
        <Layer>
            <Identifier/>
        </Layer>
    </Contents>
</Capabilities>""")

    gdal.PushErrorHandler()
    ds = gdal.Open('WMTS:/vsimem/missing.xml')
    gdal.PopErrorHandler()
    assert ds is None

###############################################################################
# Error: missing TileMatrixSet


def test_wmts_9():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/missing_tms.xml', """<Capabilities>
    <Contents>
        <Layer>
            <Identifier/>
            <TileMatrixSetLink>
                <TileMatrixSet>tms</TileMatrixSet>
            </TileMatrixSetLink>
            <Style>
                <Identifier/>
            </Style>
            <ResourceURL format="image/png" template="/vsimem/{Style}/{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}.jpeg" resourceType="tile"/>
        </Layer>
    </Contents>
</Capabilities>""")

    gdal.PushErrorHandler()
    ds = gdal.Open('WMTS:/vsimem/missing_tms.xml')
    gdal.PopErrorHandler()
    assert ds is None

###############################################################################
# Error: Missing SupportedCRS


def test_wmts_10():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/missing_SupportedCRS.xml', """<Capabilities>
    <Contents>
        <Layer>
            <Identifier/>
            <TileMatrixSetLink>
                <TileMatrixSet>tms</TileMatrixSet>
            </TileMatrixSetLink>
            <Style>
                <Identifier/>
            </Style>
            <ResourceURL format="image/png" template="/vsimem/{Style}/{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}.jpeg" resourceType="tile"/>
        </Layer>
        <TileMatrixSet>
            <Identifier>tms</Identifier>
        </TileMatrixSet>
    </Contents>
</Capabilities>""")

    gdal.PushErrorHandler()
    ds = gdal.Open('WMTS:/vsimem/missing_SupportedCRS.xml')
    gdal.PopErrorHandler()
    assert ds is None

###############################################################################
# Error: Cannot find TileMatrix in TileMatrixSet


def test_wmts_11():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/no_tilematrix.xml', """<Capabilities>
    <Contents>
        <Layer>
            <Identifier/>
            <TileMatrixSetLink>
                <TileMatrixSet>tms</TileMatrixSet>
            </TileMatrixSetLink>
            <Style>
                <Identifier/>
            </Style>
            <ResourceURL format="image/png" template="/vsimem/{Style}/{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}.jpeg" resourceType="tile"/>
        </Layer>
        <TileMatrixSet>
            <Identifier>tms</Identifier>
            <SupportedCRS>urn:ogc:def:crs:EPSG:6.18:3:3857</SupportedCRS>
        </TileMatrixSet>
    </Contents>
</Capabilities>""")

    gdal.PushErrorHandler()
    ds = gdal.Open('WMTS:/vsimem/no_tilematrix.xml')
    gdal.PopErrorHandler()
    assert ds is None

###############################################################################
# Error: Missing required element in TileMatrix element


def test_wmts_12():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/missing_required_element_in_tilematrix.xml', """<Capabilities>
    <Contents>
        <Layer>
            <Identifier/>
            <TileMatrixSetLink>
                <TileMatrixSet>tms</TileMatrixSet>
            </TileMatrixSetLink>
            <Style>
                <Identifier/>
            </Style>
            <ResourceURL format="image/png" template="/vsimem/{Style}/{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}.jpeg" resourceType="tile"/>
        </Layer>
        <TileMatrixSet>
            <Identifier>tms</Identifier>
            <SupportedCRS>urn:ogc:def:crs:EPSG:6.18:3:3857</SupportedCRS>
            <TileMatrix/>
        </TileMatrixSet>
    </Contents>
</Capabilities>""")

    gdal.PushErrorHandler()
    ds = gdal.Open('WMTS:/vsimem/missing_required_element_in_tilematrix.xml')
    gdal.PopErrorHandler()
    assert ds is None

###############################################################################
# Error: Missing ResourceURL


def test_wmts_12bis():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/wmts_12bis.xml', """<Capabilities>
    <Contents>
        <Layer>
            <Identifier/>
            <TileMatrixSetLink>
                <TileMatrixSet>tms</TileMatrixSet>
            </TileMatrixSetLink>
            <Style>
                <Identifier/>
            </Style>
        </Layer>
        <TileMatrixSet>
            <Identifier>tms</Identifier>
            <SupportedCRS>urn:ogc:def:crs:EPSG:6.18:3:3857</SupportedCRS>
            <TileMatrix>
                <Identifier>0</Identifier>
                <ScaleDenominator>559082264.029</ScaleDenominator>
                <TopLeftCorner>-20037508.3428 20037508.3428</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>1</MatrixWidth>
                <MatrixHeight>1</MatrixHeight>
            </TileMatrix>
        </TileMatrixSet>
    </Contents>
</Capabilities>""")

    gdal.PushErrorHandler()
    ds = gdal.Open('WMTS:/vsimem/wmts_12bis.xml')
    gdal.PopErrorHandler()
    assert ds is None

###############################################################################
# Error: TileMatrixSetLink points to non-existing TileMatrix


def test_wmts_tilematrixsetlink_to_non_existing_tilematrix():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    xml = """<Capabilities>
    <Contents>
        <Layer>
            <Identifier/>
            <TileMatrixSetLink>
                <TileMatrixSet>unknown</TileMatrixSet>
            </TileMatrixSetLink>
            <Style>
                <Identifier/>
            </Style>
            <ResourceURL format="image/png" template="/vsimem/{TileMatrix}/{TileRow}/{TileCol}.png" resourceType="tile"/>
        </Layer>
        <TileMatrixSet>
            <Identifier>tms</Identifier>
            <SupportedCRS>urn:ogc:def:crs:EPSG:6.18:3:3857</SupportedCRS>
            <TileMatrix>
                <Identifier>0</Identifier>
                <ScaleDenominator>559082264.029</ScaleDenominator>
                <TopLeftCorner>-20037508.3428 20037508.3428</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>1</MatrixWidth>
                <MatrixHeight>1</MatrixHeight>
            </TileMatrix>
        </TileMatrixSet>
    </Contents>
</Capabilities>"""
    with gdaltest.tempfile('/vsimem/test_wmts.xml', xml), gdaltest.error_handler():
        ds = gdal.Open('WMTS:/vsimem/test_wmts.xml')
    assert ds is None

###############################################################################
# Minimal


def test_wmts_13():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/minimal.xml', """<Capabilities>
    <Contents>
        <Layer>
            <Identifier/>
            <TileMatrixSetLink>
                <TileMatrixSet>tms</TileMatrixSet>
            </TileMatrixSetLink>
            <Style>
                <Identifier/>
            </Style>
            <ResourceURL format="image/png" template="/vsimem/{TileMatrix}/{TileRow}/{TileCol}.png" resourceType="tile"/>
        </Layer>
        <TileMatrixSet>
            <Identifier>tms</Identifier>
            <SupportedCRS>urn:ogc:def:crs:EPSG:6.18:3:3857</SupportedCRS>
            <TileMatrix>
                <Identifier>0</Identifier>
                <ScaleDenominator>559082264.029</ScaleDenominator>
                <TopLeftCorner>-20037508.3428 20037508.3428</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>1</MatrixWidth>
                <MatrixHeight>1</MatrixHeight>
            </TileMatrix>
        </TileMatrixSet>
    </Contents>
</Capabilities>""")

    ds = gdal.Open('WMTS:/vsimem/minimal.xml')
    assert ds is not None
    assert ds.RasterXSize == 256
    assert ds.RasterYSize == 256
    got_gt = ds.GetGeoTransform()
    expected_gt = (-20037508.342799999, 156543.03392811998, 0.0, 20037508.342799999, 0.0, -156543.03392811998)
    for i in range(6):
        assert got_gt[i] == pytest.approx(expected_gt[i], abs=1e-8)
    assert ds.GetProjectionRef().find('3857') >= 0
    assert ds.RasterCount == 4
    for i in range(4):
        assert ds.GetRasterBand(i + 1).GetColorInterpretation() == gdal.GCI_RedBand + i
    assert ds.GetRasterBand(1).GetOverviewCount() == 0
    assert ds.GetRasterBand(1).GetOverview(0) is None
    gdal.PushErrorHandler()
    cs = ds.GetRasterBand(1).Checksum()
    gdal.PopErrorHandler()
    assert cs == 0
    assert ds.GetSubDatasets() == []
    assert ds.GetRasterBand(1).GetMetadataItem('Pixel_0_0', 'LocationInfo') is None
    assert ds.GetRasterBand(1).GetMetadataItem('foo') is None

    for connection_str in ['WMTS:/vsimem/minimal.xml,layer=',
                           'WMTS:/vsimem/minimal.xml,style=',
                           'WMTS:/vsimem/minimal.xml,tilematrixset=',
                           'WMTS:/vsimem/minimal.xml,tilematrixset=tms',
                           'WMTS:/vsimem/minimal.xml,tilematrix=',
                           'WMTS:/vsimem/minimal.xml,zoom_level=',
                           'WMTS:/vsimem/minimal.xml,layer=,style=,tilematrixset=']:
        ds = gdal.Open(connection_str)
        assert ds is not None, connection_str
        ds = None

    for connection_str in ['WMTS:/vsimem/minimal.xml,layer=foo',
                           'WMTS:/vsimem/minimal.xml,style=bar',
                           'WMTS:/vsimem/minimal.xml,tilematrixset=baz',
                           'WMTS:/vsimem/minimal.xml,tilematrix=baw',
                           'WMTS:/vsimem/minimal.xml,zoom_level=30']:
        gdal.PushErrorHandler()
        ds = gdal.Open(connection_str)
        gdal.PopErrorHandler()
        assert ds is None, connection_str
        ds = None

    ds = gdal.Open('WMTS:/vsimem/minimal.xml')
    tmp_ds = gdal.GetDriverByName('MEM').Create('', 256, 256, 4)
    for i in range(4):
        tmp_ds.GetRasterBand(i + 1).Fill((i + 1) * 255 / 4)
    tmp_ds = gdal.GetDriverByName('PNG').CreateCopy('/vsimem/0/0/0.png', tmp_ds)
    for i in range(4):
        cs = ds.GetRasterBand(i + 1).Checksum()
        assert cs == tmp_ds.GetRasterBand(i + 1).Checksum()

    ref_data = tmp_ds.ReadRaster(0, 0, 256, 256)
    got_data = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize, 256, 256)
    assert ref_data == got_data

    ref_data = tmp_ds.GetRasterBand(1).ReadRaster(0, 0, 256, 256)
    got_data = ds.GetRasterBand(1).ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize, 256, 256)
    assert ref_data == got_data

    ds = None
    wmts_CleanCache()

###############################################################################
# Nominal RESTful


def test_wmts_14():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/nominal.xml', """<Capabilities>
    <Contents>
        <Layer>
            <Identifier>lyr1</Identifier>
            <Title>My layer1</Title>
            <Abstract>My abstract</Abstract>
            <ows:WGS84BoundingBox>
                <ows:LowerCorner>-180 -85.0511287798065</ows:LowerCorner>
                <ows:UpperCorner>180 85.0511287798065</ows:UpperCorner>
            </ows:WGS84BoundingBox>
            <Dimension>
                <ows:Identifier>time</ows:Identifier>
                <UOM>ISO8601</UOM>
                <Default>2011-10-04</Default>
                <Current>false</Current>
                <Value>2002-06-01/2011-10-04/P1D</Value>
           </Dimension>
            <TileMatrixSetLink>
                <TileMatrixSet>tms</TileMatrixSet>
            </TileMatrixSetLink>
            <TileMatrixSetLink>
                <TileMatrixSet>another_tms</TileMatrixSet>
            </TileMatrixSetLink>
            <Style isDefault="true">
                <Identifier>style=auto</Identifier>
                <Title>Default style</Title>
            </Style>
            <Style>
                <Identifier>another_style</Identifier>
                <Title>Another style</Title>
            </Style>
            <ResourceURL format="image/png"
    template="/vsimem/{time}/{Style}/{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}.png" resourceType="tile"/>
            <ResourceURL format="text/plain"
    template="/vsimem/{time}/{Style}/{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}/{J}/{I}.txt" resourceType="FeatureInfo"/>
        </Layer>
        <TileMatrixSet>
            <Identifier>tms</Identifier>
            <SupportedCRS>urn:ogc:def:crs:EPSG:6.18:3:3857</SupportedCRS>
            <TileMatrix>
                <Identifier>tm_0</Identifier>
                <ScaleDenominator>559082264.029</ScaleDenominator>
                <TopLeftCorner>-20037508.3428 20037508.3428</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>1</MatrixWidth>
                <MatrixHeight>1</MatrixHeight>
            </TileMatrix>
            <TileMatrix>
                <ows:Identifier>tm_18</ows:Identifier>
                <ScaleDenominator>2132.72958385</ScaleDenominator>
                <TopLeftCorner>-20037508.3428 20037508.3428</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>262144</MatrixWidth>
                <MatrixHeight>262144</MatrixHeight>
            </TileMatrix>
            <TileMatrix>
                <ows:Identifier>24</ows:Identifier>
                <ScaleDenominator>33.3238997477</ScaleDenominator>
                <TopLeftCorner>-20037508.3428 20037508.3428</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>16777216</MatrixWidth>
                <MatrixHeight>16777216</MatrixHeight>
            </TileMatrix>
        </TileMatrixSet>
        <TileMatrixSet>
            <Identifier>another_tms</Identifier>
            <ows:Identifier>GoogleCRS84Quad</ows:Identifier>
            <ows:SupportedCRS>urn:ogc:def:crs:EPSG::4326</ows:SupportedCRS>
            <TileMatrix>
                <ows:Identifier>GoogleCRS84Quad:0</ows:Identifier>
                <ScaleDenominator>5.590822640287178E8</ScaleDenominator>
                <TopLeftCorner>90.0 -180.0</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>1</MatrixWidth>
                <MatrixHeight>1</MatrixHeight>
            </TileMatrix>
        </TileMatrixSet>
    </Contents>
    <ServiceMetadataURL xlink:href="/vsimem/nominal.xml"/>
</Capabilities>""")

    ds = gdal.Open('WMTS:/vsimem/nominal.xml')
    assert ds is not None
    assert (ds.GetSubDatasets() == [('WMTS:/vsimem/nominal.xml,layer=lyr1,tilematrixset=tms,style="style=auto"',
                                'Layer My layer1, tile matrix set tms, style "Default style"'),
                               ('WMTS:/vsimem/nominal.xml,layer=lyr1,tilematrixset=tms,style=another_style',
                                'Layer My layer1, tile matrix set tms, style "Another style"'),
                               ('WMTS:/vsimem/nominal.xml,layer=lyr1,tilematrixset=another_tms,style="style=auto"',
                                'Layer My layer1, tile matrix set another_tms, style "Default style"'),
                               ('WMTS:/vsimem/nominal.xml,layer=lyr1,tilematrixset=another_tms,style=another_style',
                                'Layer My layer1, tile matrix set another_tms, style "Another style"')])
    assert ds.RasterXSize == 67108864
    gdal.PushErrorHandler()
    res = ds.GetRasterBand(1).GetMetadataItem('Pixel_1_2', 'LocationInfo')
    gdal.PopErrorHandler()
    assert res == ''
    assert ds.GetMetadata() == {'ABSTRACT': 'My abstract', 'TITLE': 'My layer1'}

    gdal.PushErrorHandler()
    gdaltest.wmts_drv.CreateCopy('/vsimem/gdal_nominal.xml', gdal.GetDriverByName('MEM').Create('', 1, 1))
    gdal.PopErrorHandler()

    gdaltest.wmts_drv.CreateCopy('/vsimem/gdal_nominal.xml', ds)
    ds = None

    f = gdal.VSIFOpenL('/vsimem/gdal_nominal.xml', 'rb')
    data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
    gdal.VSIFCloseL(f)
    assert data == """<GDAL_WMTS>
  <GetCapabilitiesUrl>/vsimem/nominal.xml</GetCapabilitiesUrl>
  <Layer>lyr1</Layer>
  <Style>style=auto</Style>
  <TileMatrixSet>tms</TileMatrixSet>
  <DataWindow>
    <UpperLeftX>-20037508.3428</UpperLeftX>
    <UpperLeftY>20037508.3428</UpperLeftY>
    <LowerRightX>20037508.34278254</LowerRightX>
    <LowerRightY>-20037508.34278254</LowerRightY>
  </DataWindow>
  <BandsCount>4</BandsCount>
  <DataType>Byte</DataType>
  <Cache />
  <UnsafeSSL>true</UnsafeSSL>
  <ZeroBlockHttpCodes>204,404</ZeroBlockHttpCodes>
  <ZeroBlockOnServerException>true</ZeroBlockOnServerException>
</GDAL_WMTS>
"""

    ds = gdal.Open('/vsimem/gdal_nominal.xml')
    gdal.FileFromMemBuffer('/vsimem/2011-10-04/style=auto/tms/tm_18/0/0/2/1.txt', 'foo')
    res = ds.GetRasterBand(1).GetMetadataItem('Pixel_1_2', 'LocationInfo')
    assert res == '<LocationInfo>foo</LocationInfo>'
    res = ds.GetRasterBand(1).GetMetadataItem('Pixel_1_2', 'LocationInfo')
    assert res == '<LocationInfo>foo</LocationInfo>'

    ds = gdal.Open('<GDAL_WMTS><GetCapabilitiesUrl>/vsimem/nominal.xml</GetCapabilitiesUrl></GDAL_WMTS>')
    assert ds is not None

    ds = gdal.Open('WMTS:/vsimem/gdal_nominal.xml')
    assert ds is not None

    for open_options in [['URL=/vsimem/nominal.xml'],
                         ['URL=/vsimem/nominal.xml', 'STYLE=style=auto', 'TILEMATRIXSET=tms']]:
        ds = gdal.OpenEx('WMTS:', open_options=open_options)
        assert ds is not None

    for open_options in [['URL=/vsimem/nominal.xml', 'STYLE=x', 'TILEMATRIXSET=y'],
                         ['URL=/vsimem/nominal.xml', 'STYLE=style=auto', 'TILEMATRIX=30'],
                         ['URL=/vsimem/nominal.xml', 'STYLE=style=auto', 'ZOOM_LEVEL=30']]:
        gdal.PushErrorHandler()
        ds = gdal.OpenEx('WMTS:', open_options=open_options)
        gdal.PopErrorHandler()
        assert ds is None

    ds = gdal.Open('WMTS:/vsimem/nominal.xml')
    gdal.FileFromMemBuffer('/vsimem/2011-10-04/style=auto/tms/tm_18/0/0/2/1.txt', '<?xml version="1.0" encoding="UTF-8"?><xml_content/>')
    res = ds.GetRasterBand(1).GetMetadataItem('Pixel_1_2', 'LocationInfo')
    assert res == """<LocationInfo><xml_content />
</LocationInfo>"""

    ds = gdal.Open('WMTS:/vsimem/gdal_nominal.xml,tilematrix=tm_0')
    assert ds is not None
    assert ds.RasterXSize == 256

    ds = gdal.OpenEx('WMTS:/vsimem/gdal_nominal.xml', open_options=['tilematrix=tm_0'])
    assert ds is not None
    assert ds.RasterXSize == 256

    ds = gdal.Open('WMTS:/vsimem/gdal_nominal.xml,zoom_level=0')
    assert ds is not None
    assert ds.RasterXSize == 256

    ds = gdal.OpenEx('WMTS:/vsimem/gdal_nominal.xml', open_options=['zoom_level=0'])
    assert ds is not None
    assert ds.RasterXSize == 256

    gdal.FileFromMemBuffer('/vsimem/gdal_nominal.xml', """<GDAL_WMTS>
  <GetCapabilitiesUrl>/vsimem/nominal.xml</GetCapabilitiesUrl>
  <Layer>lyr1</Layer>
  <Style>style=auto</Style>
  <TileMatrixSet>tms</TileMatrixSet>
  <TileMatrix>tm_0</TileMatrix>
  <DataWindow>
    <UpperLeftX>-20037508.3428</UpperLeftX>
    <UpperLeftY>20037508.3428</UpperLeftY>
    <LowerRightX>20037508.34278254</LowerRightX>
    <LowerRightY>-20037508.34278254</LowerRightY>
  </DataWindow>
  <BandsCount>4</BandsCount>
  <Cache />
  <UnsafeSSL>true</UnsafeSSL>
  <ZeroBlockHttpCodes>204,404</ZeroBlockHttpCodes>
  <ZeroBlockOnServerException>true</ZeroBlockOnServerException>
</GDAL_WMTS>""")
    ds = gdal.Open('WMTS:/vsimem/gdal_nominal.xml')
    assert ds is not None
    assert ds.RasterXSize == 256

    gdal.FileFromMemBuffer('/vsimem/gdal_nominal.xml', """<GDAL_WMTS>
  <GetCapabilitiesUrl>/vsimem/nominal.xml</GetCapabilitiesUrl>
  <Layer>lyr1</Layer>
  <Style>style=auto</Style>
  <TileMatrixSet>tms</TileMatrixSet>
  <ZoomLevel>0</ZoomLevel>
  <DataWindow>
    <UpperLeftX>-20037508.3428</UpperLeftX>
    <UpperLeftY>20037508.3428</UpperLeftY>
    <LowerRightX>20037508.34278254</LowerRightX>
    <LowerRightY>-20037508.34278254</LowerRightY>
  </DataWindow>
  <BandsCount>4</BandsCount>
  <Cache />
  <UnsafeSSL>true</UnsafeSSL>
  <ZeroBlockHttpCodes>204,404</ZeroBlockHttpCodes>
  <ZeroBlockOnServerException>true</ZeroBlockOnServerException>
</GDAL_WMTS>""")
    ds = gdal.Open('WMTS:/vsimem/gdal_nominal.xml')
    assert ds is not None
    assert ds.RasterXSize == 256

###############################################################################
# Nominal KVP


def test_wmts_15():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/nominal_kvp.xml?service=WMTS&request=GetCapabilities', """<Capabilities xmlns="http://www.opengis.net/wmts/1.0">
    <ows:OperationsMetadata>
    <ows:Operation name="GetCapabilities">
      <ows:DCP>
        <ows:HTTP>
          <ows:Get xlink:href="/vsimem/nominal_kvp.xml?">
            <ows:Constraint name="GetEncoding">
              <ows:AllowedValues>
                <ows:Value>KVP</ows:Value>
              </ows:AllowedValues>
            </ows:Constraint>
          </ows:Get>
        </ows:HTTP>
      </ows:DCP>
    </ows:Operation>
    <ows:Operation name="GetTile">
      <ows:DCP>
        <ows:HTTP>
          <ows:Get xlink:href="/vsimem/nominal_kvp.xml?">
          </ows:Get>
        </ows:HTTP>
      </ows:DCP>
    </ows:Operation>
    <ows:Operation name="GetFeatureInfo">
      <ows:DCP>
        <ows:HTTP>
          <ows:Get xlink:href="/vsimem/nominal_kvp.xml?">
          </ows:Get>
        </ows:HTTP>
      </ows:DCP>
    </ows:Operation>
  </ows:OperationsMetadata>
    <Contents>
        <Layer>
            <Identifier>lyr1</Identifier>
            <Title>My layer1</Title>
            <ows:BoundingBox crs="urn:ogc:def:crs:EPSG:6.18:3:3857">
                <ows:LowerCorner>-20037508.3428 -20037508.3428</ows:LowerCorner>
                <ows:UpperCorner>20037508.3428 20037508.3428</ows:UpperCorner>
            </ows:BoundingBox>
            <Dimension>
                <ows:Identifier>time</ows:Identifier>
                <UOM>ISO8601</UOM>
                <Default>2011-10-04</Default>
                <Current>false</Current>
                <Value>2002-06-01/2011-10-04/P1D</Value>
           </Dimension>
            <TileMatrixSetLink>
                <TileMatrixSet>tms</TileMatrixSet>
            </TileMatrixSetLink>
            <Style isDefault="true">
                <Identifier>default_style</Identifier>
                <Title>Default style</Title>
            </Style>
            <Format>image/jpeg</Format>
            <Format>image/png</Format>
            <InfoFormat>text/plain</InfoFormat>
        </Layer>
        <TileMatrixSet>
            <Identifier>tms</Identifier>
            <ows:BoundingBox crs="urn:ogc:def:crs:EPSG:6.18:3:3857">
                <ows:LowerCorner>-20037508.3428 -20037508.3428</ows:LowerCorner>
                <ows:UpperCorner>20037508.3428 20037508.3428</ows:UpperCorner>
            </ows:BoundingBox>
            <SupportedCRS>urn:ogc:def:crs:EPSG:6.18:3:3857</SupportedCRS>
            <TileMatrix>
                <Identifier>0</Identifier>
                <ScaleDenominator>559082264.029</ScaleDenominator>
                <TopLeftCorner>-20037508.3428 20037508.3428</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>1</MatrixWidth>
                <MatrixHeight>1</MatrixHeight>
            </TileMatrix>
            <TileMatrix>
                <ows:Identifier>18</ows:Identifier>
                <ScaleDenominator>2132.72958385</ScaleDenominator>
                <TopLeftCorner>-20037508.3428 20037508.3428</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>262144</MatrixWidth>
                <MatrixHeight>262144</MatrixHeight>
            </TileMatrix>
            <TileMatrix>
                <ows:Identifier>24</ows:Identifier>
                <ScaleDenominator>33.3238997477</ScaleDenominator>
                <TopLeftCorner>-20037508.3428 20037508.3428</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>16777216</MatrixWidth>
                <MatrixHeight>16777216</MatrixHeight>
            </TileMatrix>
        </TileMatrixSet>
    </Contents>
</Capabilities>""")

    ds = gdal.Open('/vsimem/nominal_kvp.xml?service=WMTS&request=GetCapabilities')
    assert ds is not None
    assert ds.RasterXSize == 67108864
    gdal.PushErrorHandler()
    res = ds.GetRasterBand(1).GetMetadataItem('Pixel_1_2', 'LocationInfo')
    gdal.PopErrorHandler()
    assert res == ''

    gdaltest.wmts_drv.CreateCopy('/vsimem/gdal_nominal_kvp.xml', ds)
    ds = None

    ds = gdal.Open('/vsimem/gdal_nominal_kvp.xml')
    gdal.FileFromMemBuffer('/vsimem/nominal_kvp.xml?service=WMTS&request=GetFeatureInfo&version=1.0.0&layer=lyr1&style=default_style&InfoFormat=text/plain&TileMatrixSet=tms&TileMatrix=18&TileRow=0&TileCol=0&J=2&I=1&time=2011-10-04', 'bar')
    res = ds.GetRasterBand(1).GetMetadataItem('Pixel_1_2', 'LocationInfo')
    assert res == '<LocationInfo>bar</LocationInfo>'

    ds = gdal.Open('WMTS:/vsimem/gdal_nominal_kvp.xml')
    assert ds is not None
    tmp_ds = gdal.GetDriverByName('MEM').Create('', 256, 256, 4)
    for i in range(4):
        tmp_ds.GetRasterBand(i + 1).Fill((i + 1) * 255 / 4)
    tmp_ds = gdal.GetDriverByName('PNG').CreateCopy('/vsimem/nominal_kvp.xml?service=WMTS&request=GetTile&version=1.0.0&layer=lyr1&style=default_style&format=image/png&TileMatrixSet=tms&TileMatrix=0&TileRow=0&TileCol=0&time=2011-10-04', tmp_ds)
    for i in range(4):
        cs = ds.GetRasterBand(i + 1).GetOverview(0).Checksum()
        assert cs == tmp_ds.GetRasterBand(i + 1).Checksum()

    ref_data = tmp_ds.ReadRaster(0, 0, 256, 256)
    got_data = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize, 256, 256)
    assert ref_data == got_data

    ref_data = tmp_ds.GetRasterBand(1).ReadRaster(0, 0, 256, 256)
    got_data = ds.GetRasterBand(1).ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize, 256, 256)
    assert ref_data == got_data

    ds = None
    wmts_CleanCache()

###############################################################################
# AOI from layer WGS84BoundingBox


def test_wmts_16():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/wmts_16.xml', """<Capabilities>
    <Contents>
        <Layer>
            <Identifier>lyr1</Identifier>
            <Title>My layer1</Title>
            <ows:WGS84BoundingBox>
                <ows:LowerCorner>-90 0</ows:LowerCorner>
                <ows:UpperCorner>90 90</ows:UpperCorner>
            </ows:WGS84BoundingBox>
            <TileMatrixSetLink>
                <TileMatrixSet>tms</TileMatrixSet>
            </TileMatrixSetLink>
            <Style isDefault="true">
                <Identifier>default_style</Identifier>
                <Title>Default style</Title>
            </Style>
            <ResourceURL format="image/png"
    template="/vsimem/{Style}/{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}.png" resourceType="tile"/>
        </Layer>
        <TileMatrixSet>
            <Identifier>tms</Identifier>
            <ows:Identifier>GoogleCRS84Quad</ows:Identifier>
            <ows:SupportedCRS>urn:ogc:def:crs:EPSG::4326</ows:SupportedCRS>
            <TileMatrix>
                <ows:Identifier>GoogleCRS84Quad:0</ows:Identifier>
                <ScaleDenominator>5.590822640287178E8</ScaleDenominator>
                <TopLeftCorner>90.0 -180.0</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>1</MatrixWidth>
                <MatrixHeight>1</MatrixHeight>
            </TileMatrix>
            <TileMatrix>
                <ows:Identifier>GoogleCRS84Quad:1</ows:Identifier>
                <ScaleDenominator>2.795411320143589E8</ScaleDenominator>
                <TopLeftCorner>90.0 -180.0</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>2</MatrixWidth>
                <MatrixHeight>1</MatrixHeight>
            </TileMatrix>
            <TileMatrix>
                <ows:Identifier>GoogleCRS84Quad:2</ows:Identifier>
                <ScaleDenominator>1.397705660071794E8</ScaleDenominator>
                <TopLeftCorner>90.0 -180.0</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>4</MatrixWidth>
                <MatrixHeight>2</MatrixHeight>
            </TileMatrix>
        </TileMatrixSet>
    </Contents>
    <ServiceMetadataURL xlink:href="/vsimem/wmts_16.xml"/>
</Capabilities>""")

    ds = gdal.Open('WMTS:/vsimem/wmts_16.xml')
    assert ds is not None
    assert ds.RasterXSize == 512
    assert ds.RasterYSize == 256
    got_gt = ds.GetGeoTransform()
    expected_gt = (-90, 0.3515625, 0.0, 90.0, 0.0, -0.3515625)
    for i in range(6):
        assert got_gt[i] == pytest.approx(expected_gt[i], abs=1e-8)
    assert ds.GetProjectionRef().find('4326') >= 0
    assert ds.GetSpatialRef().GetDataAxisToSRSAxisMapping() == [2, 1]

###############################################################################
# AOI from layer BoundingBox


def test_wmts_17():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/wmts_17.xml', """<Capabilities>
    <Contents>
        <Layer>
            <Identifier>lyr1</Identifier>
            <Title>My layer1</Title>
            <ows:BoundingBox crs="urn:ogc:def:crs:EPSG::4326">
                <ows:LowerCorner>0 -90</ows:LowerCorner>
                <ows:UpperCorner>90 90</ows:UpperCorner>
            </ows:BoundingBox>
            <TileMatrixSetLink>
                <TileMatrixSet>tms</TileMatrixSet>
            </TileMatrixSetLink>
            <Style isDefault="true">
                <Identifier>default_style</Identifier>
                <Title>Default style</Title>
            </Style>
            <ResourceURL format="image/png"
    template="/vsimem/{Style}/{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}.png" resourceType="tile"/>
        </Layer>
        <TileMatrixSet>
            <Identifier>tms</Identifier>
            <ows:Identifier>GoogleCRS84Quad</ows:Identifier>
            <ows:SupportedCRS>urn:ogc:def:crs:EPSG::4326</ows:SupportedCRS>
            <TileMatrix>
                <ows:Identifier>GoogleCRS84Quad:0</ows:Identifier>
                <ScaleDenominator>5.590822640287178E8</ScaleDenominator>
                <TopLeftCorner>90.0 -180.0</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>1</MatrixWidth>
                <MatrixHeight>1</MatrixHeight>
            </TileMatrix>
            <TileMatrix>
                <ows:Identifier>GoogleCRS84Quad:1</ows:Identifier>
                <ScaleDenominator>2.795411320143589E8</ScaleDenominator>
                <TopLeftCorner>90.0 -180.0</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>2</MatrixWidth>
                <MatrixHeight>1</MatrixHeight>
            </TileMatrix>
            <TileMatrix>
                <ows:Identifier>GoogleCRS84Quad:2</ows:Identifier>
                <ScaleDenominator>1.397705660071794E8</ScaleDenominator>
                <TopLeftCorner>90.0 -180.0</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>4</MatrixWidth>
                <MatrixHeight>2</MatrixHeight>
            </TileMatrix>
        </TileMatrixSet>
    </Contents>
    <ServiceMetadataURL xlink:href="/vsimem/wmts_17.xml"/>
</Capabilities>""")

    ds = gdal.Open('WMTS:/vsimem/wmts_17.xml')
    assert ds is not None
    assert ds.RasterXSize == 512
    assert ds.RasterYSize == 256
    got_gt = ds.GetGeoTransform()
    expected_gt = (-90, 0.3515625, 0.0, 90.0, 0.0, -0.3515625)
    for i in range(6):
        assert got_gt[i] == pytest.approx(expected_gt[i], abs=1e-8)
    assert ds.GetProjectionRef().find('4326') >= 0

###############################################################################
# AOI from TileMatrixSet BoundingBox


def test_wmts_18():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/wmts_18.xml', """<Capabilities>
    <Contents>
        <Layer>
            <Identifier>lyr1</Identifier>
            <Title>My layer1</Title>
            <TileMatrixSetLink>
                <TileMatrixSet>tms</TileMatrixSet>
            </TileMatrixSetLink>
            <Style isDefault="true">
                <Identifier>default_style</Identifier>
                <Title>Default style</Title>
            </Style>
            <ResourceURL format="image/png"
    template="/vsimem/{Style}/{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}.png" resourceType="tile"/>
        </Layer>
        <TileMatrixSet>
            <Identifier>tms</Identifier>
            <ows:Identifier>GoogleCRS84Quad</ows:Identifier>
            <ows:SupportedCRS>urn:ogc:def:crs:EPSG::4326</ows:SupportedCRS>
            <ows:BoundingBox crs="urn:ogc:def:crs:EPSG::4326">
                <ows:LowerCorner>0 -90</ows:LowerCorner>
                <ows:UpperCorner>90 90</ows:UpperCorner>
            </ows:BoundingBox>
            <TileMatrix>
                <ows:Identifier>GoogleCRS84Quad:0</ows:Identifier>
                <ScaleDenominator>5.590822640287178E8</ScaleDenominator>
                <TopLeftCorner>90.0 -180.0</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>1</MatrixWidth>
                <MatrixHeight>1</MatrixHeight>
            </TileMatrix>
            <TileMatrix>
                <ows:Identifier>GoogleCRS84Quad:1</ows:Identifier>
                <ScaleDenominator>2.795411320143589E8</ScaleDenominator>
                <TopLeftCorner>90.0 -180.0</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>2</MatrixWidth>
                <MatrixHeight>1</MatrixHeight>
            </TileMatrix>
            <TileMatrix>
                <ows:Identifier>GoogleCRS84Quad:2</ows:Identifier>
                <ScaleDenominator>1.397705660071794E8</ScaleDenominator>
                <TopLeftCorner>90.0 -180.0</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>4</MatrixWidth>
                <MatrixHeight>2</MatrixHeight>
            </TileMatrix>
        </TileMatrixSet>
    </Contents>
    <ServiceMetadataURL xlink:href="/vsimem/wmts_18.xml"/>
</Capabilities>""")

    ds = gdal.Open('WMTS:/vsimem/wmts_18.xml')
    assert ds is not None
    assert ds.RasterXSize == 512
    assert ds.RasterYSize == 256
    got_gt = ds.GetGeoTransform()
    expected_gt = (-90, 0.3515625, 0.0, 90.0, 0.0, -0.3515625)
    for i in range(6):
        assert got_gt[i] == pytest.approx(expected_gt[i], abs=1e-8)
    assert ds.GetProjectionRef().find('4326') >= 0

###############################################################################
# AOI from TileMatrixSetLimits


def test_wmts_19():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/wmts_19.xml', """<Capabilities>
    <Contents>
        <Layer>
            <Identifier>lyr1</Identifier>
            <Title>My layer1</Title>
            <TileMatrixSetLink>
                <TileMatrixSet>tms</TileMatrixSet>
                <TileMatrixSetLimits>
                    <TileMatrixLimits>
                        <TileMatrix>GoogleCRS84Quad:2</TileMatrix>
                        <MinTileRow>0</MinTileRow>
                        <MaxTileRow>0</MaxTileRow>
                        <MinTileCol>1</MinTileCol>
                        <MaxTileCol>2</MaxTileCol>
                    </TileMatrixLimits>
                </TileMatrixSetLimits>
            </TileMatrixSetLink>
            <Style isDefault="true">
                <Identifier>default_style</Identifier>
                <Title>Default style</Title>
            </Style>
            <ResourceURL format="image/png"
    template="/vsimem/{Style}/{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}.png" resourceType="tile"/>
        </Layer>
        <TileMatrixSet>
            <Identifier>tms</Identifier>
            <ows:Identifier>GoogleCRS84Quad</ows:Identifier>
            <ows:SupportedCRS>urn:ogc:def:crs:EPSG::4326</ows:SupportedCRS>
            <TileMatrix>
                <ows:Identifier>GoogleCRS84Quad:0</ows:Identifier>
                <ScaleDenominator>5.590822640287178E8</ScaleDenominator>
                <TopLeftCorner>90.0 -180.0</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>1</MatrixWidth>
                <MatrixHeight>1</MatrixHeight>
            </TileMatrix>
            <TileMatrix>
                <ows:Identifier>GoogleCRS84Quad:1</ows:Identifier>
                <ScaleDenominator>2.795411320143589E8</ScaleDenominator>
                <TopLeftCorner>90.0 -180.0</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>2</MatrixWidth>
                <MatrixHeight>1</MatrixHeight>
            </TileMatrix>
            <TileMatrix>
                <ows:Identifier>GoogleCRS84Quad:2</ows:Identifier>
                <ScaleDenominator>1.397705660071794E8</ScaleDenominator>
                <TopLeftCorner>90.0 -180.0</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>4</MatrixWidth>
                <MatrixHeight>2</MatrixHeight>
            </TileMatrix>
        </TileMatrixSet>
    </Contents>
    <ServiceMetadataURL xlink:href="/vsimem/wmts_19.xml"/>
</Capabilities>""")

    ds = gdal.Open('WMTS:/vsimem/wmts_19.xml')
    assert ds is not None
    assert ds.RasterXSize == 512
    assert ds.RasterYSize == 256
    got_gt = ds.GetGeoTransform()
    expected_gt = (-90, 0.3515625, 0.0, 90.0, 0.0, -0.3515625)
    for i in range(6):
        assert got_gt[i] == pytest.approx(expected_gt[i], abs=1e-8)
    assert ds.GetProjectionRef().find('4326') >= 0

###############################################################################
# AOI from layer BoundingBox but restricted with TileMatrixSetLimits


def test_wmts_20():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/wmts_20.xml', """<Capabilities>
    <Contents>
        <Layer>
            <ows:BoundingBox crs="urn:ogc:def:crs:EPSG::4326">
                <ows:LowerCorner>-90 -180</ows:LowerCorner>
                <ows:UpperCorner>90 180</ows:UpperCorner>
            </ows:BoundingBox>
            <Identifier>lyr1</Identifier>
            <Title>My layer1</Title>
            <TileMatrixSetLink>
                <TileMatrixSet>tms</TileMatrixSet>
                <TileMatrixSetLimits>
                    <TileMatrixLimits>
                        <TileMatrix>GoogleCRS84Quad:2</TileMatrix>
                        <MinTileRow>0</MinTileRow>
                        <MaxTileRow>0</MaxTileRow>
                        <MinTileCol>1</MinTileCol>
                        <MaxTileCol>2</MaxTileCol>
                    </TileMatrixLimits>
                </TileMatrixSetLimits>
            </TileMatrixSetLink>
            <Style isDefault="true">
                <Identifier>default_style</Identifier>
                <Title>Default style</Title>
            </Style>
            <ResourceURL format="image/png"
    template="/vsimem/{Style}/{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}.png" resourceType="tile"/>
        </Layer>
        <TileMatrixSet>
            <Identifier>tms</Identifier>
            <ows:Identifier>GoogleCRS84Quad</ows:Identifier>
            <ows:SupportedCRS>urn:ogc:def:crs:EPSG::4326</ows:SupportedCRS>
            <TileMatrix>
                <ows:Identifier>GoogleCRS84Quad:0</ows:Identifier>
                <ScaleDenominator>5.590822640287178E8</ScaleDenominator>
                <TopLeftCorner>90.0 -180.0</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>1</MatrixWidth>
                <MatrixHeight>1</MatrixHeight>
            </TileMatrix>
            <TileMatrix>
                <ows:Identifier>GoogleCRS84Quad:1</ows:Identifier>
                <ScaleDenominator>2.795411320143589E8</ScaleDenominator>
                <TopLeftCorner>90.0 -180.0</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>2</MatrixWidth>
                <MatrixHeight>1</MatrixHeight>
            </TileMatrix>
            <TileMatrix>
                <ows:Identifier>GoogleCRS84Quad:2</ows:Identifier>
                <ScaleDenominator>1.397705660071794E8</ScaleDenominator>
                <TopLeftCorner>90.0 -180.0</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>4</MatrixWidth>
                <MatrixHeight>2</MatrixHeight>
            </TileMatrix>
        </TileMatrixSet>
    </Contents>
    <ServiceMetadataURL xlink:href="/vsimem/wmts_20.xml"/>
</Capabilities>""")

    ds = gdal.OpenEx('WMTS:/vsimem/wmts_20.xml',
                     open_options = ["CLIP_EXTENT_WITH_MOST_PRECISE_TILE_MATRIX_LIMITS=YES"])
    assert ds is not None
    assert ds.RasterXSize == 512
    assert ds.RasterYSize == 256
    got_gt = ds.GetGeoTransform()
    expected_gt = (-90, 0.3515625, 0.0, 90.0, 0.0, -0.3515625)
    for i in range(6):
        assert got_gt[i] == pytest.approx(expected_gt[i], abs=1e-8)
    assert ds.GetProjectionRef().find('4326') >= 0

###############################################################################
# Test ExtendBeyondDateLine


def test_wmts_21():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/wmts_21.xml', """<Capabilities>
    <Contents>
        <Layer>
            <ows:BoundingBox crs="urn:ogc:def:crs:EPSG::4326">
                <ows:LowerCorner>-90 -180</ows:LowerCorner>
                <ows:UpperCorner>0 180</ows:UpperCorner>
            </ows:BoundingBox>
            <!-- completely made-up case and not really representative... -->
            <ows:BoundingBox crs="urn:ogc:def:crs:OGC:2:84">
                <ows:LowerCorner>90 -90</ows:LowerCorner>
                <ows:UpperCorner>-90 0</ows:UpperCorner>
            </ows:BoundingBox>
            <Identifier>lyr1</Identifier>
            <Title>My layer1</Title>
            <Style isDefault="true">
                <Identifier>default_style</Identifier>
                <Title>Default style</Title>
            </Style>
            <TileMatrixSetLink>
                <TileMatrixSet>tms</TileMatrixSet>
            </TileMatrixSetLink>
            <ResourceURL format="image/png"
    template="/vsimem/wmts_21/{Style}/{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}.png" resourceType="tile"/>
        </Layer>
        <TileMatrixSet>
            <Identifier>tms</Identifier>
            <ows:Identifier>GoogleCRS84Quad</ows:Identifier>
            <ows:SupportedCRS>urn:ogc:def:crs:EPSG::4326</ows:SupportedCRS>
            <TileMatrix>
                <ows:Identifier>GoogleCRS84Quad:0</ows:Identifier>
                <ScaleDenominator>5.590822640287178E8</ScaleDenominator>
                <TopLeftCorner>90.0 -180.0</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>1</MatrixWidth>
                <MatrixHeight>1</MatrixHeight>
            </TileMatrix>
            <TileMatrix>
                <ows:Identifier>GoogleCRS84Quad:1</ows:Identifier>
                <ScaleDenominator>2.795411320143589E8</ScaleDenominator>
                <TopLeftCorner>90.0 -180.0</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>2</MatrixWidth>
                <MatrixHeight>1</MatrixHeight>
            </TileMatrix>
            <TileMatrix>
                <ows:Identifier>GoogleCRS84Quad:2</ows:Identifier>
                <ScaleDenominator>1.397705660071794E8</ScaleDenominator>
                <TopLeftCorner>90.0 -180.0</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>4</MatrixWidth>
                <MatrixHeight>2</MatrixHeight>
            </TileMatrix>
        </TileMatrixSet>
    </Contents>
    <ServiceMetadataURL xlink:href="/vsimem/wmts_21.xml"/>
</Capabilities>""")

    ds = gdal.Open('WMTS:/vsimem/wmts_21.xml,extendbeyonddateline=yes')
    assert ds is not None
    assert ds.RasterXSize == 512
    assert ds.RasterYSize == 256
    got_gt = ds.GetGeoTransform()
    expected_gt = (90, 0.3515625, 0.0, 0.0, 0.0, -0.3515625)
    for i in range(6):
        assert got_gt[i] == pytest.approx(expected_gt[i], abs=1e-8)
    assert ds.GetProjectionRef().find('4326') >= 0

    tmp_ds = gdal.GetDriverByName('MEM').Create('', 256, 256, 4)
    for i in range(4):
        tmp_ds.GetRasterBand(i + 1).Fill(64)
    tmp3_ds = gdal.GetDriverByName('PNG').CreateCopy('/vsimem/wmts_21/default_style/tms/GoogleCRS84Quad:2/1/3.png', tmp_ds)

    tmp_ds = gdal.GetDriverByName('MEM').Create('', 256, 256, 4)
    for i in range(4):
        tmp_ds.GetRasterBand(i + 1).Fill(128)
    tmp0_ds = gdal.GetDriverByName('PNG').CreateCopy('/vsimem/wmts_21/default_style/tms/GoogleCRS84Quad:2/1/0.png', tmp_ds)

    assert ds.GetRasterBand(1).ReadRaster(0, 0, 256, 256) == tmp3_ds.GetRasterBand(1).ReadRaster(0, 0, 256, 256)

    assert ds.GetRasterBand(1).ReadRaster(256, 0, 256, 256) == tmp0_ds.GetRasterBand(1).ReadRaster(0, 0, 256, 256)

###############################################################################
# Test when WGS84BoundingBox is a densified reprojection of the tile matrix bbox


def test_wmts_22():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/wmts_22.xml', """<Capabilities>
    <Contents>
        <Layer>
            <ows:WGS84BoundingBox>
                <ows:LowerCorner>-6.38153862706 55.6179644952</ows:LowerCorner>
                <ows:UpperCorner>60.3815386271 75.5825702342</ows:UpperCorner>
            </ows:WGS84BoundingBox>
            <Identifier>lyr1</Identifier>
            <Title>My layer1</Title>
            <Style isDefault="true">
                <Identifier>default_style</Identifier>
                <Title>Default style</Title>
            </Style>
            <TileMatrixSetLink>
                <TileMatrixSet>tms</TileMatrixSet>
            </TileMatrixSetLink>
            <ResourceURL format="image/png"
    template="/vsimem/{Style}/{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}.png" resourceType="tile"/>
        </Layer>
        <TileMatrixSet>
            <Identifier>tms</Identifier>
            <ows:Identifier>tms</ows:Identifier>
            <ows:SupportedCRS>urn:ogc:def:crs:EPSG::3067</ows:SupportedCRS>
            <TileMatrix>
                <ows:Identifier>13</ows:Identifier>
                <ScaleDenominator>3571.42857143</ScaleDenominator>
                <TopLeftCorner>-548576.0 8388608.0</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>8192</MatrixWidth>
                <MatrixHeight>8192</MatrixHeight>
            </TileMatrix>
        </TileMatrixSet>
    </Contents>
    <ServiceMetadataURL xlink:href="/vsimem/wmts_22.xml"/>
</Capabilities>""")

    ds = gdal.Open('WMTS:/vsimem/wmts_22.xml')
    assert ds is not None
    assert ds.RasterXSize == 2097152
    assert ds.RasterYSize == 2097152
    got_gt = ds.GetGeoTransform()
    expected_gt = (-548576.0, 1.0000000000004, 0.0, 8388608.0, 0.0, -1.0000000000004)
    for i in range(6):
        assert got_gt[i] == pytest.approx(expected_gt[i], abs=1e-8)
    assert ds.GetProjectionRef().find('3067') >= 0
###############################################################################
#


def wmts_23(imagetype, expected_cs):

    if gdaltest.wmts_drv is None:
        pytest.skip()

    inputXml = '/vsimem/' + imagetype + '.xml'
    serviceUrl = '/vsimem/wmts_23/' + imagetype
    gdal.FileFromMemBuffer(inputXml, """<Capabilities>
    <Contents>
        <Layer>
            <Identifier/>
            <TileMatrixSetLink>
                <TileMatrixSet>tms</TileMatrixSet>
            </TileMatrixSetLink>
            <Style>
                <Identifier/>
            </Style>
            <ResourceURL format="image/png" template=" """ + serviceUrl + """/{TileMatrix}/{TileRow}/{TileCol}.png" resourceType="tile"/>
        </Layer>
        <TileMatrixSet>
            <Identifier>tms</Identifier>
            <SupportedCRS>urn:ogc:def:crs:EPSG:6.18:3:3857</SupportedCRS>
            <TileMatrix>
                <Identifier>0</Identifier>
                <ScaleDenominator>559082264.029</ScaleDenominator>
                <TopLeftCorner>-20037508.3428 20037508.3428</TopLeftCorner>
                <TileWidth>128</TileWidth>
                <TileHeight>128</TileHeight>
                <MatrixWidth>1</MatrixWidth>
                <MatrixHeight>1</MatrixHeight>
            </TileMatrix>
        </TileMatrixSet>
    </Contents>
</Capabilities>""")

    tmp_ds = gdal.Open('data/wms/' + imagetype + '.png')
    assert tmp_ds is not None, 'fail - cannot open tmp_ds'

    tile0_ds = gdal.GetDriverByName('PNG').CreateCopy(serviceUrl + '/0/0/0.png', tmp_ds)
    assert tile0_ds is not None, 'fail - cannot create tile0'

    ds = gdal.Open('WMTS:' + inputXml)
    assert ds is not None

    assert ds.RasterXSize == 128
    assert ds.RasterYSize == 128

    for i in range(4):
        cs = ds.GetRasterBand(i + 1).Checksum()
        assert cs == expected_cs[i]



def test_wmts_23_gray():
    return wmts_23('gray', [60137, 60137, 60137, 4428])


def test_wmts_23_grayalpha():
    return wmts_23('gray+alpha', [39910, 39910, 39910, 63180])


def test_wmts_23_pal():
    return wmts_23('pal', [62950, 59100, 63864, 453])


def test_wmts_23_rgb():
    return wmts_23('rgb', [1020, 3665, 6180, 4428])


def test_wmts_23_rgba():
    return wmts_23('rgba', [65530, 51449, 1361, 59291])


def test_wmts_invalid_global_to_tm_reprojection():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    inputXml = '/vsimem/wmts_invalid_global_to_tm_reprojection.xml'
    gdal.FileFromMemBuffer(inputXml, """<?xml version="1.0"?>
<Capabilities xmlns="http://www.opengis.net/wmts/1.0"
xmlns:ows="http://www.opengis.net/ows/1.1"
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" version="1.0.0">
  <Contents>
    <Layer>
      <ows:Title>foo</ows:Title>
      <ows:Abstract></ows:Abstract>
      <ows:WGS84BoundingBox>
        <ows:LowerCorner>-180.0 -85.0511287798</ows:LowerCorner>
        <ows:UpperCorner>180.0 85.0511287798</ows:UpperCorner>
      </ows:WGS84BoundingBox>
      <ows:Identifier>foo</ows:Identifier>
      <Style>
        <ows:Identifier>default</ows:Identifier>
      </Style>
      <Format>image/png</Format>
      <TileMatrixSetLink>
        <TileMatrixSet>NZTM</TileMatrixSet>
      </TileMatrixSetLink>
      <ResourceURL
          format="image/png"
          resourceType="tile"
          template="https://example.com/{TileMatrixSet}/{TileMatrix}/{TileCol}/{TileRow}.png"
      />
    </Layer>
    <TileMatrixSet>
      <ows:Identifier>NZTM</ows:Identifier>
      <ows:SupportedCRS>urn:ogc:def:crs:EPSG::2193</ows:SupportedCRS>
      <TileMatrix>
        <ows:Identifier>0</ows:Identifier>
        <ScaleDenominator>32000000.0</ScaleDenominator>
        <TopLeftCorner>10000000.0 -1000000.0</TopLeftCorner>
        <TileWidth>256</TileWidth>
        <TileHeight>256</TileHeight>
        <MatrixWidth>2</MatrixWidth>
        <MatrixHeight>4</MatrixHeight>
      </TileMatrix>
    </TileMatrixSet>
  </Contents>
</Capabilities>""")

    ds = gdal.Open('WMTS:' + inputXml)
    assert ds.RasterXSize == 512 and ds.RasterYSize == 1024
    ds = None

    gdal.Unlink(inputXml)

###############################################################################
#


def test_wmts_check_no_overflow_zoom_level():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    inputXml = '/vsimem/wmts_check_no_overflow_zoom_level.xml'
    gdal.FileFromMemBuffer(inputXml, """<?xml version="1.0"?>
<Capabilities xmlns="http://www.opengis.net/wmts/1.0"
xmlns:ows="http://www.opengis.net/ows/1.1"
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" version="1.0.0">
  <Contents>
    <Layer>
      <ows:Title>foo</ows:Title>
      <ows:Abstract></ows:Abstract>
      <ows:WGS84BoundingBox crs="urn:ogc:def:crs:OGC:2:84">
        <ows:LowerCorner>-179.99999000000003 -85.00000000000003</ows:LowerCorner>
        <ows:UpperCorner>179.99999000000003 85.0</ows:UpperCorner>
      </ows:WGS84BoundingBox>
      <ows:Identifier>foo</ows:Identifier>
      <Style>
        <ows:Identifier>default</ows:Identifier>
      </Style>
      <Format>image/png</Format>
      <TileMatrixSetLink>
        <TileMatrixSet>default</TileMatrixSet>
      </TileMatrixSetLink>
      <ResourceURL
          format="image/png"
          resourceType="tile"
          template="https://example.com/{TileMatrixSet}/{TileMatrix}/{TileCol}/{TileRow}.png"
      />
    </Layer>
    <TileMatrixSet>
    <ows:Identifier>default</ows:Identifier>
    <ows:SupportedCRS>urn:ogc:def:crs:EPSG::3857</ows:SupportedCRS>
    <TileMatrix>
    <ows:Identifier>0</ows:Identifier>
    <ScaleDenominator>5.590822640285016E8</ScaleDenominator>
    <TopLeftCorner>-2.0037508342787E7 2.0037508342787E7</TopLeftCorner>
    <TileWidth>256</TileWidth>
    <TileHeight>256</TileHeight>
    <MatrixWidth>1</MatrixWidth>
    <MatrixHeight>1</MatrixHeight>
    </TileMatrix>
    <TileMatrix>
    <ows:Identifier>1</ows:Identifier>
    <ScaleDenominator>2.7954113201425034E8</ScaleDenominator>
    <TopLeftCorner>-2.0037508342787E7 2.0037508342787E7</TopLeftCorner>
    <TileWidth>256</TileWidth>
    <TileHeight>256</TileHeight>
    <MatrixWidth>2</MatrixWidth>
    <MatrixHeight>2</MatrixHeight>
    </TileMatrix>
    <TileMatrix>
    <ows:Identifier>2</ows:Identifier>
    <ScaleDenominator>1.3977056600712562E8</ScaleDenominator>
    <TopLeftCorner>-2.0037508342787E7 2.0037508342787E7</TopLeftCorner>
    <TileWidth>256</TileWidth>
    <TileHeight>256</TileHeight>
    <MatrixWidth>4</MatrixWidth>
    <MatrixHeight>4</MatrixHeight>
    </TileMatrix>
    <TileMatrix>
    <ows:Identifier>3</ows:Identifier>
    <ScaleDenominator>6.988528300356235E7</ScaleDenominator>
    <TopLeftCorner>-2.0037508342787E7 2.0037508342787E7</TopLeftCorner>
    <TileWidth>256</TileWidth>
    <TileHeight>256</TileHeight>
    <MatrixWidth>8</MatrixWidth>
    <MatrixHeight>8</MatrixHeight>
    </TileMatrix>
    <TileMatrix>
    <ows:Identifier>4</ows:Identifier>
    <ScaleDenominator>3.494264150178117E7</ScaleDenominator>
    <TopLeftCorner>-2.0037508342787E7 2.0037508342787E7</TopLeftCorner>
    <TileWidth>256</TileWidth>
    <TileHeight>256</TileHeight>
    <MatrixWidth>16</MatrixWidth>
    <MatrixHeight>16</MatrixHeight>
    </TileMatrix>
    <TileMatrix>
    <ows:Identifier>5</ows:Identifier>
    <ScaleDenominator>1.7471320750890587E7</ScaleDenominator>
    <TopLeftCorner>-2.0037508342787E7 2.0037508342787E7</TopLeftCorner>
    <TileWidth>256</TileWidth>
    <TileHeight>256</TileHeight>
    <MatrixWidth>32</MatrixWidth>
    <MatrixHeight>32</MatrixHeight>
    </TileMatrix>
    <TileMatrix>
    <ows:Identifier>6</ows:Identifier>
    <ScaleDenominator>8735660.375445293</ScaleDenominator>
    <TopLeftCorner>-2.0037508342787E7 2.0037508342787E7</TopLeftCorner>
    <TileWidth>256</TileWidth>
    <TileHeight>256</TileHeight>
    <MatrixWidth>64</MatrixWidth>
    <MatrixHeight>64</MatrixHeight>
    </TileMatrix>
    <TileMatrix>
    <ows:Identifier>7</ows:Identifier>
    <ScaleDenominator>4367830.187722647</ScaleDenominator>
    <TopLeftCorner>-2.0037508342787E7 2.0037508342787E7</TopLeftCorner>
    <TileWidth>256</TileWidth>
    <TileHeight>256</TileHeight>
    <MatrixWidth>128</MatrixWidth>
    <MatrixHeight>128</MatrixHeight>
    </TileMatrix>
    <TileMatrix>
    <ows:Identifier>8</ows:Identifier>
    <ScaleDenominator>2183915.0938617955</ScaleDenominator>
    <TopLeftCorner>-2.0037508342787E7 2.0037508342787E7</TopLeftCorner>
    <TileWidth>256</TileWidth>
    <TileHeight>256</TileHeight>
    <MatrixWidth>256</MatrixWidth>
    <MatrixHeight>256</MatrixHeight>
    </TileMatrix>
    <TileMatrix>
    <ows:Identifier>9</ows:Identifier>
    <ScaleDenominator>1091957.5469304253</ScaleDenominator>
    <TopLeftCorner>-2.0037508342787E7 2.0037508342787E7</TopLeftCorner>
    <TileWidth>256</TileWidth>
    <TileHeight>256</TileHeight>
    <MatrixWidth>512</MatrixWidth>
    <MatrixHeight>512</MatrixHeight>
    </TileMatrix>
    <TileMatrix>
    <ows:Identifier>10</ows:Identifier>
    <ScaleDenominator>545978.7734656851</ScaleDenominator>
    <TopLeftCorner>-2.0037508342787E7 2.0037508342787E7</TopLeftCorner>
    <TileWidth>256</TileWidth>
    <TileHeight>256</TileHeight>
    <MatrixWidth>1024</MatrixWidth>
    <MatrixHeight>1023</MatrixHeight>
    </TileMatrix>
    <TileMatrix>
    <ows:Identifier>11</ows:Identifier>
    <ScaleDenominator>272989.38673237007</ScaleDenominator>
    <TopLeftCorner>-2.0037508342787E7 2.0037508342787E7</TopLeftCorner>
    <TileWidth>256</TileWidth>
    <TileHeight>256</TileHeight>
    <MatrixWidth>2048</MatrixWidth>
    <MatrixHeight>2045</MatrixHeight>
    </TileMatrix>
    <TileMatrix>
    <ows:Identifier>12</ows:Identifier>
    <ScaleDenominator>136494.69336618503</ScaleDenominator>
    <TopLeftCorner>-2.0037508342787E7 2.0037508342787E7</TopLeftCorner>
    <TileWidth>256</TileWidth>
    <TileHeight>256</TileHeight>
    <MatrixWidth>4096</MatrixWidth>
    <MatrixHeight>4090</MatrixHeight>
    </TileMatrix>
    <TileMatrix>
    <ows:Identifier>13</ows:Identifier>
    <ScaleDenominator>68247.34668309252</ScaleDenominator>
    <TopLeftCorner>-2.0037508342787E7 2.0037508342787E7</TopLeftCorner>
    <TileWidth>256</TileWidth>
    <TileHeight>256</TileHeight>
    <MatrixWidth>8192</MatrixWidth>
    <MatrixHeight>8179</MatrixHeight>
    </TileMatrix>
    <TileMatrix>
    <ows:Identifier>14</ows:Identifier>
    <ScaleDenominator>34123.67334154626</ScaleDenominator>
    <TopLeftCorner>-2.0037508342787E7 2.0037508342787E7</TopLeftCorner>
    <TileWidth>256</TileWidth>
    <TileHeight>256</TileHeight>
    <MatrixWidth>16384</MatrixWidth>
    <MatrixHeight>16358</MatrixHeight>
    </TileMatrix>
    <TileMatrix>
    <ows:Identifier>15</ows:Identifier>
    <ScaleDenominator>17061.836671245605</ScaleDenominator>
    <TopLeftCorner>-2.0037508342787E7 2.0037508342787E7</TopLeftCorner>
    <TileWidth>256</TileWidth>
    <TileHeight>256</TileHeight>
    <MatrixWidth>32768</MatrixWidth>
    <MatrixHeight>32715</MatrixHeight>
    </TileMatrix>
    <TileMatrix>
    <ows:Identifier>16</ows:Identifier>
    <ScaleDenominator>8530.918335622802</ScaleDenominator>
    <TopLeftCorner>-2.0037508342787E7 2.0037508342787E7</TopLeftCorner>
    <TileWidth>256</TileWidth>
    <TileHeight>256</TileHeight>
    <MatrixWidth>65536</MatrixWidth>
    <MatrixHeight>65429</MatrixHeight>
    </TileMatrix>
    <TileMatrix>
    <ows:Identifier>17</ows:Identifier>
    <ScaleDenominator>4265.459167338929</ScaleDenominator>
    <TopLeftCorner>-2.0037508342787E7 2.0037508342787E7</TopLeftCorner>
    <TileWidth>256</TileWidth>
    <TileHeight>256</TileHeight>
    <MatrixWidth>131072</MatrixWidth>
    <MatrixHeight>130858</MatrixHeight>
    </TileMatrix>
    <TileMatrix>
    <ows:Identifier>18</ows:Identifier>
    <ScaleDenominator>2132.729584141936</ScaleDenominator>
    <TopLeftCorner>-2.0037508342787E7 2.0037508342787E7</TopLeftCorner>
    <TileWidth>256</TileWidth>
    <TileHeight>256</TileHeight>
    <MatrixWidth>262144</MatrixWidth>
    <MatrixHeight>261715</MatrixHeight>
    </TileMatrix>
    <TileMatrix>
    <ows:Identifier>19</ows:Identifier>
    <ScaleDenominator>1066.3647915984968</ScaleDenominator>
    <TopLeftCorner>-2.0037508342787E7 2.0037508342787E7</TopLeftCorner>
    <TileWidth>256</TileWidth>
    <TileHeight>256</TileHeight>
    <MatrixWidth>524288</MatrixWidth>
    <MatrixHeight>523430</MatrixHeight>
    </TileMatrix>
    <TileMatrix>
    <ows:Identifier>20</ows:Identifier>
    <ScaleDenominator>533.1823957992484</ScaleDenominator>
    <TopLeftCorner>-2.0037508342787E7 2.0037508342787E7</TopLeftCorner>
    <TileWidth>256</TileWidth>
    <TileHeight>256</TileHeight>
    <MatrixWidth>1048576</MatrixWidth>
    <MatrixHeight>1046859</MatrixHeight>
    </TileMatrix>
    <TileMatrix>
    <ows:Identifier>21</ows:Identifier>
    <ScaleDenominator>266.5911978996242</ScaleDenominator>
    <TopLeftCorner>-2.0037508342787E7 2.0037508342787E7</TopLeftCorner>
    <TileWidth>256</TileWidth>
    <TileHeight>256</TileHeight>
    <MatrixWidth>2097152</MatrixWidth>
    <MatrixHeight>2093718</MatrixHeight>
    </TileMatrix>
    <TileMatrix>
    <ows:Identifier>22</ows:Identifier>
    <ScaleDenominator>133.2955989498121</ScaleDenominator>
    <TopLeftCorner>-2.0037508342787E7 2.0037508342787E7</TopLeftCorner>
    <TileWidth>256</TileWidth>
    <TileHeight>256</TileHeight>
    <MatrixWidth>4194304</MatrixWidth>
    <MatrixHeight>4187435</MatrixHeight>
    </TileMatrix>
    <TileMatrix>
    <ows:Identifier>23</ows:Identifier>
    <ScaleDenominator>66.64779947490605</ScaleDenominator>
    <TopLeftCorner>-2.0037508342787E7 2.0037508342787E7</TopLeftCorner>
    <TileWidth>256</TileWidth>
    <TileHeight>256</TileHeight>
    <MatrixWidth>8388608</MatrixWidth>
    <MatrixHeight>8374869</MatrixHeight>
    </TileMatrix>
    </TileMatrixSet>
  </Contents>
</Capabilities>""")

    ds = gdal.Open(inputXml)
    assert ds.RasterXSize == 1073741766 and ds.RasterYSize == 1070224430
    count_levels = 1 + ds.GetRasterBand(1).GetOverviewCount()
    assert count_levels == 23
    ds = None

    gdal.Unlink(inputXml)

###############################################################################
# Test when local wmts tiles are missing


def test_wmts_24():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/wmts_missing_local_tiles.xml', """<Capabilities>
    <Contents>
        <Layer>
            <ows:WGS84BoundingBox>
                <ows:LowerCorner>-6.38153862706 55.6179644952</ows:LowerCorner>
                <ows:UpperCorner>60.3815386271 75.5825702342</ows:UpperCorner>
            </ows:WGS84BoundingBox>
            <Identifier>lyr1</Identifier>
            <Title>My layer1</Title>
            <Style isDefault="true">
                <Identifier>default_style</Identifier>
                <Title>Default style</Title>
            </Style>
            <TileMatrixSetLink>
                <TileMatrixSet>tms</TileMatrixSet>
            </TileMatrixSetLink>
            <ResourceURL format="image/png"
    template="file:///invalid_path/{Style}/{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}.png" resourceType="tile"/>
        </Layer>
        <TileMatrixSet>
            <Identifier>tms</Identifier>
            <ows:Identifier>tms</ows:Identifier>
            <ows:SupportedCRS>urn:ogc:def:crs:EPSG::3067</ows:SupportedCRS>
            <TileMatrix>
                <ows:Identifier>13</ows:Identifier>
                <ScaleDenominator>3571.42857143</ScaleDenominator>
                <TopLeftCorner>-548576.0 8388608.0</TopLeftCorner>
                <TileWidth>256</TileWidth>
                <TileHeight>256</TileHeight>
                <MatrixWidth>8192</MatrixWidth>
                <MatrixHeight>8192</MatrixHeight>
            </TileMatrix>
        </TileMatrixSet>
    </Contents>
    <ServiceMetadataURL xlink:href="/vsimem/wmts_missing_local_tiles.xml"/>
</Capabilities>""")

    ds = gdal.Open('WMTS:/vsimem/wmts_missing_local_tiles.xml')
#   Read some data from the image
    band = ds.GetRasterBand(1)
    assert band is not None
    structval=band.ReadRaster(0,0,1,1,buf_type=gdal.GDT_UInt16)
    assert structval is not None
    data = struct.unpack('h' , structval)
#   Expect a null value for the pixel data
    assert data[0] == 0

###############################################################################
#


def wmts_CleanCache():
    hexstr = '012346789abcdef'
    for i in range(len(hexstr)):
        for j in range(len(hexstr)):
            lst = gdal.ReadDir('/vsimem/cache/%s/%s' % (i, j))
            if lst is not None:
                for f in lst:
                    gdal.Unlink('/vsimem/cache/%s/%s/%s' % (i, j, f))

###############################################################################
#


def test_wmts_cleanup():

    if gdaltest.wmts_drv is None:
        pytest.skip()

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', None)
    gdal.SetConfigOption('GDAL_DEFAULT_WMS_CACHE_PATH', None)

    wmts_CleanCache()

    lst = gdal.ReadDir('/vsimem/')
    if lst:
        for f in lst:
            gdal.Unlink('/vsimem/' + f)

    try:
        shutil.rmtree('tmp/wmts_cache')
    except OSError:
        pass




