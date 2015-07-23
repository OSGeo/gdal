#!/usr/bin/env python
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

import sys
import shutil

sys.path.append( '../pymod' )

from osgeo import gdal

import gdaltest

###############################################################################
# Find WMTS driver

def wmts_1():

    gdaltest.wmts_drv = gdal.GetDriverByName('WMTS')

    if gdaltest.wmts_drv is not None and gdal.GetDriverByName('WMS') is None:
        print('Missing WMS driver')
        gdaltest.wmts_drv = None
    
    if gdaltest.wmts_drv is not None:

        gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')
        gdal.SetConfigOption('GDAL_DEFAULT_WMS_CACHE_PATH', '/vsimem/cache')

        return 'success'
    else:
        return 'skip'

###############################################################################
# Error: no URL and invalid GDAL_WMTS service file documents

def wmts_2():

    if gdaltest.wmts_drv is None:
        return 'skip'

    gdal.PushErrorHandler()
    ds = gdal.Open('WMTS:')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.PushErrorHandler()
    ds = gdal.Open('<GDAL_WMTS>')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.PushErrorHandler()
    ds = gdal.Open('<GDAL_WMTSxxx/>')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.PushErrorHandler()
    ds = gdal.Open('<GDAL_WMTS></GDAL_WMTS>')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Error: invalid URL

def wmts_3():

    if gdaltest.wmts_drv is None:
        return 'skip'

    gdal.PushErrorHandler()
    ds = gdal.Open('WMTS:https://non_existing')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Error: invalid URL

def wmts_4():

    if gdaltest.wmts_drv is None:
        return 'skip'

    gdal.PushErrorHandler()
    ds = gdal.Open('WMTS:/vsimem/non_existing')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Error: invalid XML in GetCapabilities response

def wmts_5():

    if gdaltest.wmts_drv is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/invalid_getcapabilities.xml', '<invalid_xml')

    gdal.PushErrorHandler()
    ds = gdal.Open('WMTS:/vsimem/invalid_getcapabilities.xml')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Error: invalid content in GetCapabilities response

def wmts_6():

    if gdaltest.wmts_drv is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/invalid_getcapabilities.xml', '<Capabilities/>')

    gdal.PushErrorHandler()
    ds = gdal.Open('WMTS:/vsimem/invalid_getcapabilities.xml')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Error: no layers

def wmts_7():

    if gdaltest.wmts_drv is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/empty_getcapabilities.xml', '<Capabilities><Contents/></Capabilities>')

    gdal.PushErrorHandler()
    ds = gdal.Open('WMTS:/vsimem/empty_getcapabilities.xml')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Error: missing TileMatrixSetLink and Style

def wmts_8():

    if gdaltest.wmts_drv is None:
        return 'skip'

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
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Error: missing TileMatrixSet

def wmts_9():

    if gdaltest.wmts_drv is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/missing_tms.xml', """<Capabilities>
    <Contents>
        <Layer>
            <Identifier/>
            <TileMatrixSetLink>
                <TileMatrixSet/>
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
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Error: Missing SupportedCRS

def wmts_10():

    if gdaltest.wmts_drv is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/missing_SupportedCRS.xml', """<Capabilities>
    <Contents>
        <Layer>
            <Identifier/>
            <TileMatrixSetLink>
                <TileMatrixSet/>
            </TileMatrixSetLink>
            <Style>
                <Identifier/>
            </Style>
            <ResourceURL format="image/png" template="/vsimem/{Style}/{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}.jpeg" resourceType="tile"/>
        </Layer>
        <TileMatrixSet>
            <Identifier/>
        </TileMatrixSet>
    </Contents>
</Capabilities>""")

    gdal.PushErrorHandler()
    ds = gdal.Open('WMTS:/vsimem/missing_SupportedCRS.xml')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Error: Cannot find TileMatrix in TileMatrixSet

def wmts_11():

    if gdaltest.wmts_drv is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/no_tilematrix.xml', """<Capabilities>
    <Contents>
        <Layer>
            <Identifier/>
            <TileMatrixSetLink>
                <TileMatrixSet/>
            </TileMatrixSetLink>
            <Style>
                <Identifier/>
            </Style>
            <ResourceURL format="image/png" template="/vsimem/{Style}/{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}.jpeg" resourceType="tile"/>
        </Layer>
        <TileMatrixSet>
            <Identifier/>
            <SupportedCRS>urn:ogc:def:crs:EPSG:6.18:3:3857</SupportedCRS>
        </TileMatrixSet>
    </Contents>
</Capabilities>""")

    gdal.PushErrorHandler()
    ds = gdal.Open('WMTS:/vsimem/no_tilematrix.xml')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Error: Missing required element in TileMatrix element

def wmts_12():

    if gdaltest.wmts_drv is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/missing_required_element_in_tilematrix.xml', """<Capabilities>
    <Contents>
        <Layer>
            <Identifier/>
            <TileMatrixSetLink>
                <TileMatrixSet/>
            </TileMatrixSetLink>
            <Style>
                <Identifier/>
            </Style>
            <ResourceURL format="image/png" template="/vsimem/{Style}/{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}.jpeg" resourceType="tile"/>
        </Layer>
        <TileMatrixSet>
            <Identifier/>
            <SupportedCRS>urn:ogc:def:crs:EPSG:6.18:3:3857</SupportedCRS>
            <TileMatrix/>
        </TileMatrixSet>
    </Contents>
</Capabilities>""")

    gdal.PushErrorHandler()
    ds = gdal.Open('WMTS:/vsimem/missing_required_element_in_tilematrix.xml')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Error: Missing ResourceURL

def wmts_12bis():

    if gdaltest.wmts_drv is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/wmts_12bis.xml', """<Capabilities>
    <Contents>
        <Layer>
            <Identifier/>
            <TileMatrixSetLink>
                <TileMatrixSet/>
            </TileMatrixSetLink>
            <Style>
                <Identifier/>
            </Style>
        </Layer>
        <TileMatrixSet>
            <Identifier/>
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
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Minimal

def wmts_13():

    if gdaltest.wmts_drv is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/minimal.xml', """<Capabilities>
    <Contents>
        <Layer>
            <Identifier/>
            <TileMatrixSetLink>
                <TileMatrixSet/>
            </TileMatrixSetLink>
            <Style>
                <Identifier/>
            </Style>
            <ResourceURL format="image/png" template="/vsimem/{TileMatrix}/{TileRow}/{TileCol}.png" resourceType="tile"/>
        </Layer>
        <TileMatrixSet>
            <Identifier/>
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
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.RasterXSize != 256:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.RasterYSize != 256:
        gdaltest.post_reason('fail')
        return 'fail'
    got_gt = ds.GetGeoTransform()
    expected_gt = (-20037508.342799999, 156543.03392811998, 0.0, 20037508.342799999, 0.0, -156543.03392811998)
    for i in range(6):
        if abs(got_gt[i] - expected_gt[i]) > 1e-8:
            gdaltest.post_reason('fail')
            print(got_gt)
            return 'fail'
    if ds.GetProjectionRef().find('3857') < 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.RasterCount != 4:
        gdaltest.post_reason('fail')
        return 'fail'
    for i in range(4):
        if ds.GetRasterBand(i+1).GetColorInterpretation() != gdal.GCI_RedBand + i:
            gdaltest.post_reason('fail')
            return 'fail'
    if ds.GetRasterBand(1).GetOverviewCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetOverview(0) is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.PushErrorHandler()
    cs = ds.GetRasterBand(1).Checksum()
    gdal.PopErrorHandler()
    if cs != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetSubDatasets() != []:
        gdaltest.post_reason('fail')
        print(ds.GetSubDatasets())
        return 'fail'
    if ds.GetRasterBand(1).GetMetadataItem('Pixel_0_0', 'LocationInfo') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetMetadataItem('foo') is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    for connection_str in [ 'WMTS:/vsimem/minimal.xml,layer=',
                            'WMTS:/vsimem/minimal.xml,style=',
                            'WMTS:/vsimem/minimal.xml,tilematrixset=',
                            'WMTS:/vsimem/minimal.xml,layer=,style=,tilematrixset=' ]:
        ds = gdal.Open(connection_str)
        if ds is None:
            gdaltest.post_reason('fail')
            print(connection_str)
            return 'fail'
        ds = None

    for connection_str in [ 'WMTS:/vsimem/minimal.xml,layer=foo',
                            'WMTS:/vsimem/minimal.xml,style=bar',
                            'WMTS:/vsimem/minimal.xml,tilematrixset=baz' ]:
        gdal.PushErrorHandler()
        ds = gdal.Open(connection_str)
        gdal.PopErrorHandler()
        if ds is not None:
            gdaltest.post_reason('fail')
            print(connection_str)
            return 'fail'
        ds = None

    ds = gdal.Open('WMTS:/vsimem/minimal.xml')
    tmp_ds = gdal.GetDriverByName('MEM').Create('',256,256,4)
    for i in range(4):
        tmp_ds.GetRasterBand(i+1).Fill((i+1)*255/4)
    tmp_ds = gdal.GetDriverByName('PNG').CreateCopy('/vsimem/0/0/0.png', tmp_ds)
    for i in range(4):
        cs = ds.GetRasterBand(i+1).Checksum()
        if cs != tmp_ds.GetRasterBand(i+1).Checksum():
            gdaltest.post_reason('fail')
            return 'fail'

    ref_data = tmp_ds.ReadRaster(0,0,256,256)
    got_data = ds.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize,256,256)
    if ref_data != got_data:
        gdaltest.post_reason('fail')
        return 'fail'

    ref_data = tmp_ds.GetRasterBand(1).ReadRaster(0,0,256,256)
    got_data = ds.GetRasterBand(1).ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize,256,256)
    if ref_data != got_data:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = None
    wmts_CleanCache()

    return 'success'

###############################################################################
# Nominal RESTful

def wmts_14():

    if gdaltest.wmts_drv is None:
        return 'skip'

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
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetSubDatasets() != [('WMTS:/vsimem/nominal.xml,layer=lyr1,tilematrixset=tms,style="style=auto"',
                                'Layer My layer1, tile matrix set tms, style "Default style"'),
                                ('WMTS:/vsimem/nominal.xml,layer=lyr1,tilematrixset=tms,style=another_style',
                                'Layer My layer1, tile matrix set tms, style "Another style"'),
                                ('WMTS:/vsimem/nominal.xml,layer=lyr1,tilematrixset=another_tms,style="style=auto"',
                                'Layer My layer1, tile matrix set another_tms, style "Default style"'),
                                ('WMTS:/vsimem/nominal.xml,layer=lyr1,tilematrixset=another_tms,style=another_style',
                                'Layer My layer1, tile matrix set another_tms, style "Another style"')]:
        gdaltest.post_reason('fail')
        print(ds.GetSubDatasets())
        return 'fail'
    if ds.RasterXSize != 67108864:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.PushErrorHandler()
    res = ds.GetRasterBand(1).GetMetadataItem('Pixel_1_2', 'LocationInfo') 
    gdal.PopErrorHandler()
    if res != '':
        gdaltest.post_reason('fail')
        print(res)
        return 'fail'
    if ds.GetMetadata() != {'ABSTRACT': 'My abstract', 'TITLE': 'My layer1'}:
        gdaltest.post_reason('fail')
        print(ds.GetMetadata())
        return 'fail'

    gdal.PushErrorHandler()
    gdaltest.wmts_drv.CreateCopy('/vsimem/gdal_nominal.xml', gdal.GetDriverByName('MEM').Create('',1,1))
    gdal.PopErrorHandler()

    gdaltest.wmts_drv.CreateCopy('/vsimem/gdal_nominal.xml', ds)
    ds = None
    
    f = gdal.VSIFOpenL('/vsimem/gdal_nominal.xml', 'rb')
    data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
    gdal.VSIFCloseL(f)
    if data != """<GDAL_WMTS>
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
  <Cache />
  <UnsafeSSL>true</UnsafeSSL>
  <ZeroBlockHttpCodes>204,404</ZeroBlockHttpCodes>
  <ZeroBlockOnServerException>true</ZeroBlockOnServerException>
</GDAL_WMTS>
""":
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    ds = gdal.Open('/vsimem/gdal_nominal.xml')
    gdal.FileFromMemBuffer('/vsimem/2011-10-04/style=auto/tms/18/0/0/2/1.txt', 'foo')
    res = ds.GetRasterBand(1).GetMetadataItem('Pixel_1_2', 'LocationInfo') 
    if res != '<LocationInfo>foo</LocationInfo>':
        gdaltest.post_reason('fail')
        print(res)
        return 'fail'
    res = ds.GetRasterBand(1).GetMetadataItem('Pixel_1_2', 'LocationInfo') 
    if res != '<LocationInfo>foo</LocationInfo>':
        gdaltest.post_reason('fail')
        print(res)
        return 'fail'

    ds = gdal.Open('<GDAL_WMTS><GetCapabilitiesUrl>/vsimem/nominal.xml</GetCapabilitiesUrl></GDAL_WMTS>')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.Open('WMTS:/vsimem/gdal_nominal.xml')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    for open_options in [ ['URL=/vsimem/nominal.xml'],
                          ['URL=/vsimem/nominal.xml', 'STYLE=style=auto', 'TILEMATRIXSET=tms'] ]:
        ds = gdal.OpenEx('WMTS:', open_options = open_options)
        if ds is None:
            gdaltest.post_reason('fail')
            return 'fail'

    for open_options in [ ['URL=/vsimem/nominal.xml', 'STYLE=x', 'TILEMATRIXSET=y'] ]:
        gdal.PushErrorHandler()
        ds = gdal.OpenEx('WMTS:', open_options = open_options)
        gdal.PopErrorHandler()
        if ds is not None:
            gdaltest.post_reason('fail')
            return 'fail'

    ds = gdal.Open('WMTS:/vsimem/nominal.xml')
    gdal.FileFromMemBuffer('/vsimem/2011-10-04/style=auto/tms/18/0/0/2/1.txt', '<?xml version="1.0" encoding="UTF-8"?><xml_content/>')
    res = ds.GetRasterBand(1).GetMetadataItem('Pixel_1_2', 'LocationInfo') 
    if res != """<LocationInfo><xml_content />
</LocationInfo>""":
        gdaltest.post_reason('fail')
        print(res)
        return 'fail'

    return 'success'

###############################################################################
# Nominal KVP

def wmts_15():

    if gdaltest.wmts_drv is None:
        return 'skip'

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
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.RasterXSize != 67108864:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.PushErrorHandler()
    res = ds.GetRasterBand(1).GetMetadataItem('Pixel_1_2', 'LocationInfo') 
    gdal.PopErrorHandler()
    if res != '':
        gdaltest.post_reason('fail')
        print(res)
        return 'fail'

    gdaltest.wmts_drv.CreateCopy('/vsimem/gdal_nominal_kvp.xml', ds)
    ds = None

    ds = gdal.Open('/vsimem/gdal_nominal_kvp.xml')
    gdal.FileFromMemBuffer('/vsimem/nominal_kvp.xml?service=WMTS&request=GetFeatureInfo&version=1.0.0&layer=lyr1&style=default_style&InfoFormat=text/plain&TileMatrixSet=tms&TileMatrix=18&TileRow=0&TileCol=0&J=2&I=1&time=2011-10-04', 'bar')
    res = ds.GetRasterBand(1).GetMetadataItem('Pixel_1_2', 'LocationInfo') 
    if res != '<LocationInfo>bar</LocationInfo>':
        gdaltest.post_reason('fail')
        print(res)
        return 'fail'

    ds = gdal.Open('WMTS:/vsimem/gdal_nominal_kvp.xml')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    tmp_ds = gdal.GetDriverByName('MEM').Create('',256,256,4)
    for i in range(4):
        tmp_ds.GetRasterBand(i+1).Fill((i+1)*255/4)
    tmp_ds = gdal.GetDriverByName('PNG').CreateCopy('/vsimem/nominal_kvp.xml?service=WMTS&request=GetTile&version=1.0.0&layer=lyr1&style=default_style&format=image/png&TileMatrixSet=tms&TileMatrix=0&TileRow=0&TileCol=0&time=2011-10-04', tmp_ds)
    for i in range(4):
        cs = ds.GetRasterBand(i+1).GetOverview(0).Checksum()
        if cs != tmp_ds.GetRasterBand(i+1).Checksum():
            gdaltest.post_reason('fail')
            return 'fail'

    ref_data = tmp_ds.ReadRaster(0,0,256,256)
    got_data = ds.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize,256,256)
    if ref_data != got_data:
        gdaltest.post_reason('fail')
        return 'fail'

    ref_data = tmp_ds.GetRasterBand(1).ReadRaster(0,0,256,256)
    got_data = ds.GetRasterBand(1).ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize,256,256)
    if ref_data != got_data:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = None
    wmts_CleanCache()

    return 'success'

###############################################################################
# AOI from layer WGS84BoundingBox

def wmts_16():

    if gdaltest.wmts_drv is None:
        return 'skip'

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
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.RasterXSize != 512:
        gdaltest.post_reason('fail')
        print(ds.RasterXSize)
        return 'fail'
    if ds.RasterYSize != 256:
        gdaltest.post_reason('fail')
        print(ds.RasterYSize)
        return 'fail'
    got_gt = ds.GetGeoTransform()
    expected_gt = (-90, 0.3515625, 0.0, 90.0, 0.0, -0.3515625)
    for i in range(6):
        if abs(got_gt[i] - expected_gt[i]) > 1e-8:
            gdaltest.post_reason('fail')
            print(got_gt)
            return 'fail'
    if ds.GetProjectionRef().find('4326') < 0 or ds.GetProjectionRef().find('AXIS') >= 0:
        gdaltest.post_reason('fail')
        print(ds.GetProjectionRef())
        return 'fail'

    return 'success'

###############################################################################
# AOI from layer BoundingBox

def wmts_17():

    if gdaltest.wmts_drv is None:
        return 'skip'

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
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.RasterXSize != 512:
        gdaltest.post_reason('fail')
        print(ds.RasterXSize)
        return 'fail'
    if ds.RasterYSize != 256:
        gdaltest.post_reason('fail')
        print(ds.RasterYSize)
        return 'fail'
    got_gt = ds.GetGeoTransform()
    expected_gt = (-90, 0.3515625, 0.0, 90.0, 0.0, -0.3515625)
    for i in range(6):
        if abs(got_gt[i] - expected_gt[i]) > 1e-8:
            gdaltest.post_reason('fail')
            print(got_gt)
            return 'fail'
    if ds.GetProjectionRef().find('4326') < 0 or ds.GetProjectionRef().find('AXIS') >= 0:
        gdaltest.post_reason('fail')
        print(ds.GetProjectionRef())
        return 'fail'

    return 'success'

###############################################################################
# AOI from TileMatrixSet BoundingBox

def wmts_18():

    if gdaltest.wmts_drv is None:
        return 'skip'

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
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.RasterXSize != 512:
        gdaltest.post_reason('fail')
        print(ds.RasterXSize)
        return 'fail'
    if ds.RasterYSize != 256:
        gdaltest.post_reason('fail')
        print(ds.RasterYSize)
        return 'fail'
    got_gt = ds.GetGeoTransform()
    expected_gt = (-90, 0.3515625, 0.0, 90.0, 0.0, -0.3515625)
    for i in range(6):
        if abs(got_gt[i] - expected_gt[i]) > 1e-8:
            gdaltest.post_reason('fail')
            print(got_gt)
            return 'fail'
    if ds.GetProjectionRef().find('4326') < 0 or ds.GetProjectionRef().find('AXIS') >= 0:
        gdaltest.post_reason('fail')
        print(ds.GetProjectionRef())
        return 'fail'

    return 'success'

###############################################################################
# AOI from TileMatrixSetLimits

def wmts_19():

    if gdaltest.wmts_drv is None:
        return 'skip'

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
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.RasterXSize != 512:
        gdaltest.post_reason('fail')
        print(ds.RasterXSize)
        return 'fail'
    if ds.RasterYSize != 256:
        gdaltest.post_reason('fail')
        print(ds.RasterYSize)
        return 'fail'
    got_gt = ds.GetGeoTransform()
    expected_gt = (-90, 0.3515625, 0.0, 90.0, 0.0, -0.3515625)
    for i in range(6):
        if abs(got_gt[i] - expected_gt[i]) > 1e-8:
            gdaltest.post_reason('fail')
            print(got_gt)
            return 'fail'
    if ds.GetProjectionRef().find('4326') < 0 or ds.GetProjectionRef().find('AXIS') >= 0:
        gdaltest.post_reason('fail')
        print(ds.GetProjectionRef())
        return 'fail'

    return 'success'

###############################################################################
# AOI from layer BoundingBox but restricted with TileMatrixSetLimits

def wmts_20():

    if gdaltest.wmts_drv is None:
        return 'skip'

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

    ds = gdal.Open('WMTS:/vsimem/wmts_20.xml')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.RasterXSize != 512:
        gdaltest.post_reason('fail')
        print(ds.RasterXSize)
        return 'fail'
    if ds.RasterYSize != 256:
        gdaltest.post_reason('fail')
        print(ds.RasterYSize)
        return 'fail'
    got_gt = ds.GetGeoTransform()
    expected_gt = (-90, 0.3515625, 0.0, 90.0, 0.0, -0.3515625)
    for i in range(6):
        if abs(got_gt[i] - expected_gt[i]) > 1e-8:
            gdaltest.post_reason('fail')
            print(got_gt)
            return 'fail'
    if ds.GetProjectionRef().find('4326') < 0 or ds.GetProjectionRef().find('AXIS') >= 0:
        gdaltest.post_reason('fail')
        print(ds.GetProjectionRef())
        return 'fail'

    return 'success'

###############################################################################
# Test ExtendBeyondDateLine

def wmts_21():

    if gdaltest.wmts_drv is None:
        return 'skip'

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
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.RasterXSize != 512:
        gdaltest.post_reason('fail')
        print(ds.RasterXSize)
        return 'fail'
    if ds.RasterYSize != 256:
        gdaltest.post_reason('fail')
        print(ds.RasterYSize)
        return 'fail'
    got_gt = ds.GetGeoTransform()
    expected_gt = (90, 0.3515625, 0.0, 0.0, 0.0, -0.3515625)
    for i in range(6):
        if abs(got_gt[i] - expected_gt[i]) > 1e-8:
            gdaltest.post_reason('fail')
            print(got_gt)
            return 'fail'
    if ds.GetProjectionRef().find('4326') < 0 or ds.GetProjectionRef().find('AXIS') >= 0:
        gdaltest.post_reason('fail')
        print(ds.GetProjectionRef())
        return 'fail'

    tmp_ds = gdal.GetDriverByName('MEM').Create('',256,256,4)
    for i in range(4):
        tmp_ds.GetRasterBand(i+1).Fill(64)
    tmp3_ds = gdal.GetDriverByName('PNG').CreateCopy('/vsimem/wmts_21/default_style/tms/GoogleCRS84Quad:2/1/3.png', tmp_ds)

    tmp_ds = gdal.GetDriverByName('MEM').Create('',256,256,4)
    for i in range(4):
        tmp_ds.GetRasterBand(i+1).Fill(128)
    tmp0_ds = gdal.GetDriverByName('PNG').CreateCopy('/vsimem/wmts_21/default_style/tms/GoogleCRS84Quad:2/1/0.png', tmp_ds)

    if ds.GetRasterBand(1).ReadRaster(0,0,256,256) != tmp3_ds.GetRasterBand(1).ReadRaster(0,0,256,256):
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetRasterBand(1).ReadRaster(256,0,256,256) != tmp0_ds.GetRasterBand(1).ReadRaster(0,0,256,256):
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

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

def wmts_cleanup():

    if gdaltest.wmts_drv is None:
        return 'skip'

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', None)
    gdal.SetConfigOption('GDAL_DEFAULT_WMS_CACHE_PATH', None)

    wmts_CleanCache()

    lst = gdal.ReadDir('/vsimem/')
    if lst:
        for f in lst:
            gdal.Unlink('/vsimem/' + f)

    try:
        shutil.rmtree('tmp/wmts_cache')
    except:
        pass

    return 'success'


gdaltest_list = [ 
    wmts_1,
    wmts_2,
    wmts_3,
    wmts_4,
    wmts_5,
    wmts_6,
    wmts_7,
    wmts_8,
    wmts_9,
    wmts_10,
    wmts_11,
    wmts_12,
    wmts_12bis,
    wmts_13,
    wmts_14,
    wmts_15,
    wmts_16,
    wmts_17,
    wmts_18,
    wmts_19,
    wmts_20,
    wmts_21,
    wmts_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'wmts' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

