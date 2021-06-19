#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Misc tests of VRT driver
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
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

import gdaltest
from osgeo import gdal
from osgeo import osr


###############################################################################
# Test linear scaling


def test_vrtmisc_1():

    ds = gdal.Translate('', 'data/byte.tif', options='-of MEM -scale 74 255 0 255')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    assert cs == 4323, 'did not get expected checksum'

###############################################################################
# Test power scaling


def test_vrtmisc_2():

    ds = gdal.Translate('', 'data/byte.tif', options='-of MEM -scale 74 255 0 255 -exponent 2.2')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    assert cs == 4159, 'did not get expected checksum'

###############################################################################
# Test power scaling (not <SrcMin> <SrcMax> in VRT file)


def test_vrtmisc_3():

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

    assert cs == 4159, 'did not get expected checksum'

###############################################################################
# Test multi-band linear scaling with a single -scale occurrence.


def test_vrtmisc_4():

    # -scale specified once applies to all bands
    ds = gdal.Translate('', 'data/byte.tif', options='-of MEM -scale 74 255 0 255 -b 1 -b 1')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    ds = None

    assert cs1 == 4323, 'did not get expected checksum'
    assert cs2 == 4323, 'did not get expected checksum'

###############################################################################
# Test multi-band linear scaling with -scale_XX syntax


def test_vrtmisc_5():

    # -scale_2 applies to band 2 only
    ds = gdal.Translate('', 'data/byte.tif', options='-of MEM -scale_2 74 255 0 255 -b 1 -b 1')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    ds = None

    assert cs1 == 4672, 'did not get expected checksum'
    assert cs2 == 4323, 'did not get expected checksum'

###############################################################################
# Test multi-band linear scaling with repeated -scale syntax


def test_vrtmisc_6():

    # -scale repeated as many times as output band number
    ds = gdal.Translate('', 'data/byte.tif', options='-of MEM -scale 0 255 0 255 -scale 74 255 0 255 -b 1 -b 1')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    ds = None

    assert cs1 == 4672, 'did not get expected checksum'
    assert cs2 == 4323, 'did not get expected checksum'

###############################################################################
# Test multi-band power scaling with a single -scale and -exponent occurrence.


def test_vrtmisc_7():

    # -scale and -exponent, specified once, apply to all bands
    ds = gdal.Translate('', 'data/byte.tif', options='-of MEM -scale 74 255 0 255 -exponent 2.2 -b 1 -b 1')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    ds = None

    assert cs1 == 4159, 'did not get expected checksum'
    assert cs2 == 4159, 'did not get expected checksum'

###############################################################################
# Test multi-band power scaling with -scale_XX and -exponent_XX syntax


def test_vrtmisc_8():

    # -scale_2 and -exponent_2 apply to band 2 only
    ds = gdal.Translate('', 'data/byte.tif', options='-of MEM -scale_2 74 255 0 255 -exponent_2 2.2 -b 1 -b 1')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    ds = None

    assert cs1 == 4672, 'did not get expected checksum'
    assert cs2 == 4159, 'did not get expected checksum'

###############################################################################
# Test multi-band linear scaling with repeated -scale and -exponent syntax


def test_vrtmisc_9():

    # -scale and -exponent repeated as many times as output band number
    ds = gdal.Translate('', 'data/byte.tif', options='-of MEM -scale 0 255 0 255 -scale 74 255 0 255 -exponent 1 -exponent 2.2 -b 1 -b 1')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    ds = None

    assert cs1 == 4672, 'did not get expected checksum'
    assert cs2 == 4159, 'did not get expected checksum'

###############################################################################
# Test metadata serialization (#5944)


def test_vrtmisc_10():

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
    assert ds.GetMetadata() == {'foo': 'bar'}
    assert ds.GetMetadata('some_domain') == {'bar': 'baz'}
    assert ds.GetMetadata_List('xml:a_xml_domain')[0] == '<some_xml />\n'
    # Empty default domain
    ds.SetMetadata({})
    ds = None

    ds = gdal.Open("/vsimem/vrtmisc_10.vrt")
    assert ds.GetMetadata() == {}
    assert ds.GetMetadata('some_domain') == {'bar': 'baz'}
    assert ds.GetMetadata_List('xml:a_xml_domain')[0] == '<some_xml />\n'
    ds = None

    gdal.Unlink("/vsimem/vrtmisc_10.vrt")

###############################################################################
# Test relativeToVRT is preserved during re-serialization (#5985)


def test_vrtmisc_11():

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

    assert '<SourceFilename relativeToVRT="1">../data/byte.tif</SourceFilename>' in data

###############################################################################
# Test set/delete nodata


def test_vrtmisc_12():

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
    assert ds.GetRasterBand(1).GetNoDataValue() == 123
    assert ds.GetRasterBand(1).DeleteNoDataValue() == 0
    ds = None

    ds = gdal.Open("/vsimem/vrtmisc_12.vrt")
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    ds = None

    gdal.Unlink("/vsimem/vrtmisc_12.vrt")

###############################################################################
# Test CreateCopy() preserve NBITS


def test_vrtmisc_13():

    ds = gdal.Open('data/oddsize1bit.tif')
    out_ds = gdal.GetDriverByName('VRT').CreateCopy('', ds)
    assert out_ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') == '1'

###############################################################################
# Test SrcRect/DstRect are serialized as integers


def test_vrtmisc_14():

    src_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/vrtmisc_14_src.tif', 123456789, 1, options=['SPARSE_OK=YES', 'TILED=YES'])
    gdal.GetDriverByName('VRT').CreateCopy('/vsimem/vrtmisc_14.vrt', src_ds)
    src_ds = None
    fp = gdal.VSIFOpenL('/vsimem/vrtmisc_14.vrt', 'rb')
    content = gdal.VSIFReadL(1, 10000, fp).decode('latin1')
    gdal.VSIFCloseL(fp)

    gdal.Unlink("/vsimem/vrtmisc_14_src.tif")
    gdal.Unlink("/vsimem/vrtmisc_14.vrt")

    assert ('<SrcRect xOff="0" yOff="0" xSize="123456789" ySize="1"' in content and \
       '<DstRect xOff="0" yOff="0" xSize="123456789" ySize="1"' in content)

    src_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/vrtmisc_14_src.tif', 1, 123456789, options=['SPARSE_OK=YES', 'TILED=YES'])
    gdal.GetDriverByName('VRT').CreateCopy('/vsimem/vrtmisc_14.vrt', src_ds)
    src_ds = None
    fp = gdal.VSIFOpenL('/vsimem/vrtmisc_14.vrt', 'rb')
    content = gdal.VSIFReadL(1, 10000, fp).decode('latin1')
    gdal.VSIFCloseL(fp)

    gdal.Unlink("/vsimem/vrtmisc_14_src.tif")
    gdal.Unlink("/vsimem/vrtmisc_14.vrt")

    assert ('<SrcRect xOff="0" yOff="0" xSize="1" ySize="123456789"' in content and \
       '<DstRect xOff="0" yOff="0" xSize="1" ySize="123456789"' in content)

###############################################################################
# Test CreateCopy() preserve SIGNEDBYTE


def test_vrtmisc_15():

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/vrtmisc_15.tif', 1, 1, options=['PIXELTYPE=SIGNEDBYTE'])
    out_ds = gdal.GetDriverByName('VRT').CreateCopy('', ds)
    assert out_ds.GetRasterBand(1).GetMetadataItem('PIXELTYPE', 'IMAGE_STRUCTURE') == 'SIGNEDBYTE'
    ds = None
    gdal.Unlink('/vsimem/vrtmisc_15.tif')

###############################################################################
# Test rounding to closest int for coordinates


def test_vrtmisc_16():

    gdal.BuildVRT('/vsimem/vrtmisc_16.vrt', ['data/vrtmisc16_tile1.tif', 'data/vrtmisc16_tile2.tif'])
    fp = gdal.VSIFOpenL('/vsimem/vrtmisc_16.vrt', 'rb')
    content = gdal.VSIFReadL(1, 100000, fp).decode('latin1')
    gdal.VSIFCloseL(fp)

    assert '<SrcRect xOff="0" yOff="0" xSize="952" ySize="1189"' in content
    assert '<DstRect xOff="0" yOff="0" xSize="952" ySize="1189"' in content
    assert '<SrcRect xOff="0" yOff="0" xSize="494" ySize="893"' in content
    assert '<DstRect xOff="1680" yOff="5922" xSize="494" ySize="893"' in content

    gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/vrtmisc_16.tif', gdal.Open('/vsimem/vrtmisc_16.vrt'))
    ds = gdal.Open('/vsimem/vrtmisc_16.tif')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 206
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
    assert cs == 206
    gdal.Unlink('/vsimem/vrtmisc_16.tif')
    gdal.Unlink('/vsimem/vrtmisc_16.vrt')

###############################################################################
# Check that the serialized xml:VRT doesn't include itself (#6767)


def test_vrtmisc_17():

    ds = gdal.Open('data/byte.tif')
    vrt_ds = gdal.GetDriverByName('VRT').CreateCopy('/vsimem/vrtmisc_17.vrt', ds)
    xml_vrt = vrt_ds.GetMetadata('xml:VRT')[0]
    vrt_ds = None

    gdal.Unlink('/vsimem/vrtmisc_17.vrt')

    assert 'xml:VRT' not in xml_vrt

###############################################################################
# Check GetMetadata('xml:VRT') behaviour on a in-memory VRT copied from a VRT


def test_vrtmisc_18():

    ds = gdal.Open('data/byte.vrt')
    vrt_ds = gdal.GetDriverByName('VRT').CreateCopy('', ds)
    xml_vrt = vrt_ds.GetMetadata('xml:VRT')[0]
    assert gdal.GetLastErrorMsg() == ''
    vrt_ds = None

    assert ('<SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>' in xml_vrt or \
       '<SourceFilename relativeToVRT="1">data\\byte.tif</SourceFilename>' in xml_vrt)

###############################################################################
# Check RAT support


def test_vrtmisc_rat():

    ds = gdal.Translate('/vsimem/vrtmisc_rat.tif', 'data/byte.tif', format='MEM')
    rat = gdal.RasterAttributeTable()
    rat.CreateColumn("Ints", gdal.GFT_Integer, gdal.GFU_Generic)
    ds.GetRasterBand(1).SetDefaultRAT(rat)

    vrt_ds = gdal.GetDriverByName('VRT').CreateCopy('/vsimem/vrtmisc_rat.vrt', ds)

    xml_vrt = vrt_ds.GetMetadata('xml:VRT')[0]
    assert gdal.GetLastErrorMsg() == ''
    vrt_ds = None

    assert '<GDALRasterAttributeTable tableType="thematic">' in xml_vrt

    vrt_ds = gdal.Translate('/vsimem/vrtmisc_rat.vrt', ds, format='VRT', srcWin=[0, 0, 1, 1])

    xml_vrt = vrt_ds.GetMetadata('xml:VRT')[0]
    assert gdal.GetLastErrorMsg() == ''
    vrt_ds = None

    assert '<GDALRasterAttributeTable tableType="thematic">' in xml_vrt

    ds = None

    vrt_ds = gdal.Open('/vsimem/vrtmisc_rat.vrt', gdal.GA_Update)
    rat = vrt_ds.GetRasterBand(1).GetDefaultRAT()
    assert rat is not None and rat.GetColumnCount() == 1
    vrt_ds.GetRasterBand(1).SetDefaultRAT(None)
    assert vrt_ds.GetRasterBand(1).GetDefaultRAT() is None
    vrt_ds = None

    ds = None

    gdal.Unlink('/vsimem/vrtmisc_rat.vrt')
    gdal.Unlink('/vsimem/vrtmisc_rat.tif')

###############################################################################
# Check ColorTable support


def test_vrtmisc_colortable():

    ds = gdal.Translate('', 'data/byte.tif', format='VRT')
    ct = gdal.ColorTable()
    ct.SetColorEntry(0, (255, 255, 255, 255))
    ds.GetRasterBand(1).SetColorTable(ct)
    assert ds.GetRasterBand(1).GetColorTable().GetCount() == 1
    ds.GetRasterBand(1).SetColorTable(None)
    assert ds.GetRasterBand(1).GetColorTable() is None

###############################################################################
# Check histogram support


def test_vrtmisc_histogram():

    tmpfile = '/vsimem/vrtmisc_histogram.vrt'
    ds = gdal.Translate(tmpfile, 'data/byte.tif', format='VRT')
    ds.GetRasterBand(1).SetDefaultHistogram(1, 2, [3000000000, 4])
    ds = None

    ds = gdal.Open(tmpfile)
    hist = ds.GetRasterBand(1).GetDefaultHistogram(force=0)
    ds = None

    assert hist == (1.0, 2.0, 2, [3000000000, 4])

    gdal.Unlink(tmpfile)

###############################################################################
# write SRS with unusual data axis to SRS axis mapping


def test_vrtmisc_write_srs():

    tmpfile = '/vsimem/test_vrtmisc_write_srs.vrt'
    ds = gdal.Translate(tmpfile, 'data/byte.tif', format='VRT')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    ds.SetSpatialRef(sr)
    ds = None

    ds = gdal.Open(tmpfile)
    assert ds.GetSpatialRef().GetDataAxisToSRSAxisMapping() == [1,2]
    ds = None

    gdal.Unlink(tmpfile)

###############################################################################
# complex scenario involving masks and implicit overviews

def test_vrtmisc_mask_implicit_overviews():

    with gdaltest.config_option('GDAL_TIFF_INTERNAL_MASK', 'YES'):
        ds = gdal.Translate('/vsimem/cog.tif', 'data/stefan_full_rgba.tif', options = '-outsize 2048 0 -b 1 -b 2 -b 3 -mask 4')
        ds.BuildOverviews('NEAR', [2, 4])
        ds = None
    gdal.Translate('/vsimem/cog.vrt', '/vsimem/cog.tif')
    ds = gdal.Open('/vsimem/cog.vrt')
    assert ds.GetRasterBand(1).GetOverview(0).GetMaskFlags() == gdal.GMF_PER_DATASET
    ds = None
    gdal.Translate('/vsimem/out.tif', '/vsimem/cog.vrt', options = '-b mask -outsize 10% 0')
    gdal.GetDriverByName('GTiff').Delete('/vsimem/cog.tif')
    gdal.Unlink('/vsimem/cog.vrt')
    ds = gdal.Open('/vsimem/out.tif')
    histo = ds.GetRasterBand(1).GetDefaultHistogram()[3]
    # Check that there are only 0 and 255 in the histogram
    assert histo[0] + histo[255] == ds.RasterXSize * ds.RasterYSize, histo
    assert ds.GetRasterBand(1).Checksum() == 46885
    ds = None
    gdal.Unlink('/vsimem/out.tif')


###############################################################################
# Test setting block size


def test_vrtmisc_blocksize():
    filename = '/vsimem/test_vrtmisc_blocksize.vrt'
    vrt_ds = gdal.GetDriverByName('VRT').Create(filename, 50, 50, 0)
    options = [
        'subClass=VRTSourcedRasterBand',
        'blockXSize=32',
        'blockYSize=48'
    ]
    vrt_ds.AddBand(gdal.GDT_Byte, options)
    vrt_ds = None

    vrt_ds = gdal.Open(filename)
    blockxsize, blockysize = vrt_ds.GetRasterBand(1).GetBlockSize()
    assert blockxsize == 32
    assert blockysize == 48
    vrt_ds = None

    gdal.Unlink(filename)


###############################################################################
# Test support for coordinate epoch


def test_vrtmisc_coordinate_epoch():

    filename = '/vsimem/temp.vrt'
    gdal.Translate(filename, 'data/byte.tif', options='-a_coord_epoch 2021.3')
    ds = gdal.Open(filename)
    srs = ds.GetSpatialRef()
    assert srs.GetCoordinateEpoch() == 2021.3
    ds = None

    gdal.Unlink(filename)
