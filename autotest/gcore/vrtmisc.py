#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Misc tests of VRT driver
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Test linear scaling

def vrtmisc_1():

    ds = gdal.Translate('', 'data/byte.tif', options = '-of MEM -scale 74 255 0 255')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    if cs != 4323:
        gdaltest.post_reason('did not get expected checksum')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Test power scaling

def vrtmisc_2():

    ds = gdal.Translate('', 'data/byte.tif', options = '-of MEM -scale 74 255 0 255 -exponent 2.2')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    if cs != 4159:
        gdaltest.post_reason('did not get expected checksum')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Test power scaling (not <SrcMin> <SrcMax> in VRT file)

def vrtmisc_3():

    ds = gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <ComplexSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <Exponent>2.2</Exponent>
      <DstMin>0</DstMin>
      <DstMax>255</DstMax>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>""")
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    if cs != 4159:
        gdaltest.post_reason('did not get expected checksum')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Test multi-band linear scaling with a single -scale occurrence.

def vrtmisc_4():

    # -scale specified once applies to all bands
    ds = gdal.Translate('', 'data/byte.tif', options = '-of MEM -scale 74 255 0 255 -b 1 -b 1')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    ds = None

    if cs1 != 4323:
        gdaltest.post_reason('did not get expected checksum')
        print(cs1)
        return 'fail'
    if cs2 != 4323:
        gdaltest.post_reason('did not get expected checksum')
        print(cs2)
        return 'fail'

    return 'success'

###############################################################################
# Test multi-band linear scaling with -scale_XX syntax

def vrtmisc_5():

    # -scale_2 applies to band 2 only
    ds = gdal.Translate('', 'data/byte.tif', options = '-of MEM -scale_2 74 255 0 255 -b 1 -b 1')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    ds = None

    if cs1 != 4672:
        gdaltest.post_reason('did not get expected checksum')
        print(cs1)
        return 'fail'
    if cs2 != 4323:
        gdaltest.post_reason('did not get expected checksum')
        print(cs2)
        return 'fail'

    return 'success'

###############################################################################
# Test multi-band linear scaling with repeated -scale syntax

def vrtmisc_6():

    # -scale repeated as many times as output band number
    ds = gdal.Translate('', 'data/byte.tif', options = '-of MEM -scale 0 255 0 255 -scale 74 255 0 255 -b 1 -b 1')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    ds = None

    if cs1 != 4672:
        gdaltest.post_reason('did not get expected checksum')
        print(cs1)
        return 'fail'
    if cs2 != 4323:
        gdaltest.post_reason('did not get expected checksum')
        print(cs2)
        return 'fail'

    return 'success'

###############################################################################
# Test multi-band power scaling with a single -scale and -exponent occurrence.

def vrtmisc_7():

    # -scale and -exponent, specified once, apply to all bands
    ds = gdal.Translate('', 'data/byte.tif', options = '-of MEM -scale 74 255 0 255 -exponent 2.2 -b 1 -b 1')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    ds = None

    if cs1 != 4159:
        gdaltest.post_reason('did not get expected checksum')
        print(cs1)
        return 'fail'
    if cs2 != 4159:
        gdaltest.post_reason('did not get expected checksum')
        print(cs2)
        return 'fail'

    return 'success'

###############################################################################
# Test multi-band power scaling with -scale_XX and -exponent_XX syntax

def vrtmisc_8():

    # -scale_2 and -exponent_2 apply to band 2 only
    ds = gdal.Translate('', 'data/byte.tif', options = '-of MEM -scale_2 74 255 0 255 -exponent_2 2.2 -b 1 -b 1')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    ds = None

    if cs1 != 4672:
        gdaltest.post_reason('did not get expected checksum')
        print(cs1)
        return 'fail'
    if cs2 != 4159:
        gdaltest.post_reason('did not get expected checksum')
        print(cs2)
        return 'fail'

    return 'success'

###############################################################################
# Test multi-band linear scaling with repeated -scale and -exponent syntax

def vrtmisc_9():

    # -scale and -exponent repeated as many times as output band number
    ds = gdal.Translate('', 'data/byte.tif', options = '-of MEM -scale 0 255 0 255 -scale 74 255 0 255 -exponent 1 -exponent 2.2 -b 1 -b 1')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    ds = None

    if cs1 != 4672:
        gdaltest.post_reason('did not get expected checksum')
        print(cs1)
        return 'fail'
    if cs2 != 4159:
        gdaltest.post_reason('did not get expected checksum')
        print(cs2)
        return 'fail'

    return 'success'

###############################################################################
# Test metadata serialization (#5944)

def vrtmisc_10():

    gdal.FileFromMemBuffer("/vsimem/vrtmisc_10.vrt",
"""<VRTDataset rasterXSize="1" rasterYSize="1">
  <Metadata>
      <MDI key="foo">bar</MDI>
  </Metadata>
  <Metadata domain="some_domain">
    <MDI key="bar">baz</MDI>
  </Metadata>
  <Metadata domain="xml:a_xml_domain" format="xml">
    <some_xml />
  </Metadata>
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="0">foo.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="1" RasterYSize="1" DataType="Byte" BlockXSize="1" BlockYSize="1" />
      <SrcRect xOff="0" yOff="0" xSize="1" ySize="1" />
      <DstRect xOff="0" yOff="0" xSize="1" ySize="1" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>
""")

    ds = gdal.Open("/vsimem/vrtmisc_10.vrt", gdal.GA_Update)
    # to trigger a flush
    ds.SetMetadata(ds.GetMetadata())
    ds = None

    ds = gdal.Open("/vsimem/vrtmisc_10.vrt", gdal.GA_Update)
    if ds.GetMetadata() != { 'foo': 'bar' }:
        gdaltest.post_reason('fail')
        print(ds.GetMetadata())
        return 'fail'
    if ds.GetMetadata('some_domain') != { 'bar' : 'baz' }:
        gdaltest.post_reason('fail')
        print(ds.GetMetadata('some_domain'))
        return 'fail'
    if ds.GetMetadata_List('xml:a_xml_domain')[0] != '<some_xml />\n':
        gdaltest.post_reason('fail')
        print(ds.GetMetadata_List('xml:a_xml_domain'))
        return 'fail'
    # Empty default domain
    ds.SetMetadata({})
    ds = None

    ds = gdal.Open("/vsimem/vrtmisc_10.vrt")
    if ds.GetMetadata() != {}:
        gdaltest.post_reason('fail')
        print(ds.GetMetadata())
        return 'fail'
    if ds.GetMetadata('some_domain') != { 'bar' : 'baz' }:
        gdaltest.post_reason('fail')
        print(ds.GetMetadata('some_domain'))
        return 'fail'
    if ds.GetMetadata_List('xml:a_xml_domain')[0] != '<some_xml />\n':
        gdaltest.post_reason('fail')
        print(ds.GetMetadata_List('xml:a_xml_domain'))
        return 'fail'
    ds = None

    gdal.Unlink("/vsimem/vrtmisc_10.vrt")

    return "success"

###############################################################################
# Test relativeToVRT is preserved during re-serialization (#5985)

def vrtmisc_11():

    f = open('tmp/vrtmisc_11.vrt', 'wt')
    f.write(
"""<VRTDataset rasterXSize="1" rasterYSize="1">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="1">../data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="1" RasterYSize="1" DataType="Byte" BlockXSize="1" BlockYSize="1" />
      <SrcRect xOff="0" yOff="0" xSize="1" ySize="1" />
      <DstRect xOff="0" yOff="0" xSize="1" ySize="1" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>
""")
    f.close()

    ds = gdal.Open("tmp/vrtmisc_11.vrt", gdal.GA_Update)
    # to trigger a flush
    ds.SetMetadata(ds.GetMetadata())
    ds = None

    data = open('tmp/vrtmisc_11.vrt', 'rt').read()

    gdal.Unlink("tmp/vrtmisc_11.vrt")

    if data.find('<SourceFilename relativeToVRT="1">../data/byte.tif</SourceFilename>') < 0:
        gdaltest.post_reason('fail')
        return 'fail'

    return "success"

###############################################################################
# Test set/delete nodata

def vrtmisc_12():

    gdal.FileFromMemBuffer("/vsimem/vrtmisc_12.vrt",
"""<VRTDataset rasterXSize="1" rasterYSize="1">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="0">foo.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="1" RasterYSize="1" DataType="Byte" BlockXSize="1" BlockYSize="1" />
      <SrcRect xOff="0" yOff="0" xSize="1" ySize="1" />
      <DstRect xOff="0" yOff="0" xSize="1" ySize="1" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>
""")

    ds = gdal.Open("/vsimem/vrtmisc_12.vrt", gdal.GA_Update)
    ds.GetRasterBand(1).SetNoDataValue(123)
    ds = None

    ds = gdal.Open("/vsimem/vrtmisc_12.vrt", gdal.GA_Update)
    if ds.GetRasterBand(1).GetNoDataValue() != 123:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).DeleteNoDataValue() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = gdal.Open("/vsimem/vrtmisc_12.vrt")
    if ds.GetRasterBand(1).GetNoDataValue() is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink("/vsimem/vrtmisc_12.vrt")

    return "success"

###############################################################################
# Test CreateCopy() preserve NBITS

def vrtmisc_13():

    ds = gdal.Open('data/oddsize1bit.tif')
    out_ds = gdal.GetDriverByName('VRT').CreateCopy('', ds)
    if out_ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') != '1':
        gdaltest.post_reason('fail')
        return 'fail'

    return "success"

###############################################################################
# Test SrcRect/DstRect are serialized as integers

def vrtmisc_14():

    src_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/vrtmisc_14_src.tif', 123456789, 1, options = [ 'SPARSE_OK=YES', 'TILED=YES' ] )
    gdal.GetDriverByName('VRT').CreateCopy('/vsimem/vrtmisc_14.vrt', src_ds)
    src_ds = None
    fp = gdal.VSIFOpenL('/vsimem/vrtmisc_14.vrt', 'rb')
    content = gdal.VSIFReadL(1, 10000, fp).decode('latin1')
    gdal.VSIFCloseL(fp)

    gdal.Unlink("/vsimem/vrtmisc_14_src.tif")
    gdal.Unlink("/vsimem/vrtmisc_14.vrt")

    if content.find('<SrcRect xOff="0" yOff="0" xSize="123456789" ySize="1"') < 0 or \
       content.find('<DstRect xOff="0" yOff="0" xSize="123456789" ySize="1"') < 0:
        gdaltest.post_reason('fail')
        print(content)
        return 'fail'

    src_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/vrtmisc_14_src.tif', 1, 123456789, options = [ 'SPARSE_OK=YES', 'TILED=YES' ] )
    gdal.GetDriverByName('VRT').CreateCopy('/vsimem/vrtmisc_14.vrt', src_ds)
    src_ds = None
    fp = gdal.VSIFOpenL('/vsimem/vrtmisc_14.vrt', 'rb')
    content = gdal.VSIFReadL(1, 10000, fp).decode('latin1')
    gdal.VSIFCloseL(fp)

    gdal.Unlink("/vsimem/vrtmisc_14_src.tif")
    gdal.Unlink("/vsimem/vrtmisc_14.vrt")

    if content.find('<SrcRect xOff="0" yOff="0" xSize="1" ySize="123456789"') < 0 or \
       content.find('<DstRect xOff="0" yOff="0" xSize="1" ySize="123456789"') < 0:
        gdaltest.post_reason('fail')
        print(content)
        return 'fail'

    return "success"

###############################################################################
# Test CreateCopy() preserve SIGNEDBYTE

def vrtmisc_15():

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/vrtmisc_15.tif', 1, 1, options = ['PIXELTYPE=SIGNEDBYTE'])
    out_ds = gdal.GetDriverByName('VRT').CreateCopy('', ds)
    if out_ds.GetRasterBand(1).GetMetadataItem('PIXELTYPE', 'IMAGE_STRUCTURE') != 'SIGNEDBYTE':
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    gdal.Unlink('/vsimem/vrtmisc_15.tif')

    return "success"

###############################################################################
# Test rounding to closest int for coordinates

def vrtmisc_16():

    gdal.BuildVRT('/vsimem/vrtmisc_16.vrt', [ 'data/vrtmisc16_tile1.tif', 'data/vrtmisc16_tile2.tif' ])
    fp = gdal.VSIFOpenL('/vsimem/vrtmisc_16.vrt', 'rb')
    content = gdal.VSIFReadL(1, 100000, fp).decode('latin1')
    gdal.VSIFCloseL(fp)

    if content.find('<SrcRect xOff="0" yOff="0" xSize="952" ySize="1189"') < 0:
        gdaltest.post_reason('fail')
        print(content)
        return 'fail'
    if content.find('<DstRect xOff="0" yOff="0" xSize="952" ySize="1189"') < 0:
        gdaltest.post_reason('fail')
        print(content)
        return 'fail'
    if content.find('<SrcRect xOff="0" yOff="0" xSize="494" ySize="893"') < 0:
        gdaltest.post_reason('fail')
        print(content)
        return 'fail'
    if content.find('<DstRect xOff="1680" yOff="5922" xSize="494" ySize="893"') < 0:
        gdaltest.post_reason('fail')
        print(content)
        return 'fail'

    gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/vrtmisc_16.tif', gdal.Open('/vsimem/vrtmisc_16.vrt'))
    ds = gdal.Open('/vsimem/vrtmisc_16.tif')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 206:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'
    gdal.Unlink('/vsimem/vrtmisc_16.tif')
    gdal.Unlink('/vsimem/vrtmisc_16.vrt')

    gdal.FileFromMemBuffer('/vsimem/vrtmisc_16.vrt', """<VRTDataset rasterXSize="2174" rasterYSize="6815">
  <VRTRasterBand dataType="Byte" band="1">
    <NoDataValue>0</NoDataValue>
    <ComplexSource>
      <SourceFilename relativeToVRT="0">data/vrtmisc16_tile1.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="952" RasterYSize="1189" DataType="Byte" BlockXSize="952" BlockYSize="8" />
      <SrcRect xOff="0" yOff="0" xSize="952" ySize="1189" />
      <DstRect xOff="0" yOff="0" xSize="951.999999999543" ySize="1189.0000000031" />
      <NODATA>0</NODATA>
    </ComplexSource>
    <ComplexSource>
      <SourceFilename relativeToVRT="0">data/vrtmisc16_tile2.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="494" RasterYSize="893" DataType="Byte" BlockXSize="494" BlockYSize="16" />
      <SrcRect xOff="0" yOff="0" xSize="494" ySize="893" />
      <DstRect xOff="1680.00000000001" yOff="5921.99999999876" xSize="494.000000000237" ySize="892.99999999767" />
      <NODATA>0</NODATA>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>""")
    gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/vrtmisc_16.tif', gdal.Open('/vsimem/vrtmisc_16.vrt'))
    ds = gdal.Open('/vsimem/vrtmisc_16.tif')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 206:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'
    gdal.Unlink('/vsimem/vrtmisc_16.tif')
    gdal.Unlink('/vsimem/vrtmisc_16.vrt')

    return "success"

###############################################################################
# Check that the serialized xml:VRT doesn't include itself (#6767)

def vrtmisc_17():

    ds = gdal.Open('data/byte.tif')
    vrt_ds = gdal.GetDriverByName('VRT').CreateCopy('/vsimem/vrtmisc_17.vrt', ds)
    xml_vrt = vrt_ds.GetMetadata('xml:VRT')[0]
    vrt_ds = None

    gdal.Unlink('/vsimem/vrtmisc_17.vrt')

    if xml_vrt.find('xml:VRT') >= 0:
        return 'fail'

    return "success"

###############################################################################
# Check GetMetadata('xml:VRT') behaviour on a in-memory VRT copied from a VRT

def vrtmisc_18():

    ds = gdal.Open('data/byte.vrt')
    vrt_ds = gdal.GetDriverByName('VRT').CreateCopy('', ds)
    xml_vrt = vrt_ds.GetMetadata('xml:VRT')[0]
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'
    vrt_ds = None

    if xml_vrt.find('<SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>') < 0 and \
       xml_vrt.find('<SourceFilename relativeToVRT="1">data\\byte.tif</SourceFilename>') < 0:
        gdaltest.post_reason('fail')
        print(xml_vrt)
        return 'fail'

    return "success"

###############################################################################
# Cleanup.

def vrtmisc_cleanup():
    return 'success'

gdaltest_list = [
    vrtmisc_1,
    vrtmisc_2,
    vrtmisc_3,
    vrtmisc_4,
    vrtmisc_5,
    vrtmisc_6,
    vrtmisc_7,
    vrtmisc_8,
    vrtmisc_9,
    vrtmisc_10,
    vrtmisc_11,
    vrtmisc_12,
    vrtmisc_13,
    vrtmisc_14,
    vrtmisc_15,
    vrtmisc_16,
    vrtmisc_17,
    vrtmisc_18,
    vrtmisc_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'vrtmisc' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
