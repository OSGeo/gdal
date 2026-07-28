#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Misc tests of VRT driver
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import shutil
import struct
import sys
from pathlib import Path

import gdaltest
import pytest

from osgeo import gdal, osr

pytestmark = pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)

###############################################################################
# Test linear scaling


def test_vrtmisc_1():

    ds = gdal.Translate("", "data/byte.tif", options="-of MEM -scale 74 255 0 255")
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    assert cs == 4323, "did not get expected checksum"


###############################################################################
# Test power scaling


def test_vrtmisc_2():

    ds = gdal.Translate(
        "", "data/byte.tif", options="-of MEM -scale 74 255 0 255 -exponent 2.2"
    )
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    assert cs == 4159, "did not get expected checksum"


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

    assert cs == 4159, "did not get expected checksum"


###############################################################################
# Test multi-band linear scaling with a single -scale occurrence.


def test_vrtmisc_4():

    # -scale specified once applies to all bands
    ds = gdal.Translate(
        "", "data/byte.tif", options="-of MEM -scale 74 255 0 255 -b 1 -b 1"
    )
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    ds = None

    assert cs1 == 4323, "did not get expected checksum"
    assert cs2 == 4323, "did not get expected checksum"


###############################################################################
# Test multi-band linear scaling with -scale_XX syntax


def test_vrtmisc_5():

    # -scale_2 applies to band 2 only
    ds = gdal.Translate(
        "", "data/byte.tif", options="-of MEM -scale_2 74 255 0 255 -b 1 -b 1"
    )
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    ds = None

    assert cs1 == 4672, "did not get expected checksum"
    assert cs2 == 4323, "did not get expected checksum"


###############################################################################
# Test multi-band linear scaling with repeated -scale syntax


def test_vrtmisc_6():

    # -scale repeated as many times as output band number
    ds = gdal.Translate(
        "",
        "data/byte.tif",
        options="-of MEM -scale 0 255 0 255 -scale 74 255 0 255 -b 1 -b 1",
    )
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    ds = None

    assert cs1 == 4672, "did not get expected checksum"
    assert cs2 == 4323, "did not get expected checksum"


###############################################################################
# Test multi-band power scaling with a single -scale and -exponent occurrence.


def test_vrtmisc_7():

    # -scale and -exponent, specified once, apply to all bands
    ds = gdal.Translate(
        "",
        "data/byte.tif",
        options="-of MEM -scale 74 255 0 255 -exponent 2.2 -b 1 -b 1",
    )
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    ds = None

    assert cs1 == 4159, "did not get expected checksum"
    assert cs2 == 4159, "did not get expected checksum"


###############################################################################
# Test multi-band power scaling with -scale_XX and -exponent_XX syntax


def test_vrtmisc_8():

    # -scale_2 and -exponent_2 apply to band 2 only
    ds = gdal.Translate(
        "",
        "data/byte.tif",
        options="-of MEM -scale_2 74 255 0 255 -exponent_2 2.2 -b 1 -b 1",
    )
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    ds = None

    assert cs1 == 4672, "did not get expected checksum"
    assert cs2 == 4159, "did not get expected checksum"


###############################################################################
# Test multi-band linear scaling with repeated -scale and -exponent syntax


def test_vrtmisc_9():

    # -scale and -exponent repeated as many times as output band number
    ds = gdal.Translate(
        "",
        "data/byte.tif",
        options="-of MEM -scale 0 255 0 255 -scale 74 255 0 255 -exponent 1 -exponent 2.2 -b 1 -b 1",
    )
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    ds = None

    assert cs1 == 4672, "did not get expected checksum"
    assert cs2 == 4159, "did not get expected checksum"


###############################################################################
# Test metadata serialization (#5944)


def test_vrtmisc_10(tmp_vsimem):

    gdal.FileFromMemBuffer(
        tmp_vsimem / "vrtmisc_10.vrt",
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
""",
    )

    ds = gdal.Open(tmp_vsimem / "vrtmisc_10.vrt", gdal.GA_Update)
    # to trigger a flush
    ds.SetMetadata(ds.GetMetadata())
    ds = None

    ds = gdal.Open(tmp_vsimem / "vrtmisc_10.vrt", gdal.GA_Update)
    assert ds.GetMetadata() == {"foo": "bar"}
    assert ds.GetMetadata("some_domain") == {"bar": "baz"}
    assert ds.GetMetadata_List("xml:a_xml_domain")[0] == "<some_xml />\n"
    # Empty default domain
    ds.SetMetadata({})
    ds = None

    ds = gdal.Open(tmp_vsimem / "vrtmisc_10.vrt")
    assert ds.GetMetadata() == {}
    assert ds.GetMetadata("some_domain") == {"bar": "baz"}
    assert ds.GetMetadata_List("xml:a_xml_domain")[0] == "<some_xml />\n"
    ds = None


###############################################################################
# Test relativeToVRT is preserved during re-serialization (#5985)


def test_vrtmisc_11():

    f = open("tmp/vrtmisc_11.vrt", "wt")
    f.write("""<VRTDataset rasterXSize="1" rasterYSize="1">
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

    data = open("tmp/vrtmisc_11.vrt", "rt").read()

    gdal.Unlink("tmp/vrtmisc_11.vrt")

    assert '<SourceFilename relativeToVRT="1">../data/byte.tif</SourceFilename>' in data


###############################################################################
# Test set/delete nodata


def test_vrtmisc_12(tmp_vsimem):

    gdal.FileFromMemBuffer(
        tmp_vsimem / "vrtmisc_12.vrt",
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
""",
    )

    ds = gdal.Open(tmp_vsimem / "vrtmisc_12.vrt", gdal.GA_Update)
    ds.GetRasterBand(1).SetNoDataValue(123)
    ds = None

    ds = gdal.Open(tmp_vsimem / "vrtmisc_12.vrt", gdal.GA_Update)
    assert ds.GetRasterBand(1).GetNoDataValue() == 123
    assert ds.GetRasterBand(1).DeleteNoDataValue() == 0
    ds = None

    ds = gdal.Open(tmp_vsimem / "vrtmisc_12.vrt")
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    ds = None


###############################################################################
# Test CreateCopy() preserve NBITS


def test_vrtmisc_13():

    ds = gdal.Open("data/oddsize1bit.tif")
    out_ds = gdal.GetDriverByName("VRT").CreateCopy("", ds)
    assert out_ds.GetRasterBand(1).GetMetadataItem("NBITS", "IMAGE_STRUCTURE") == "1"


###############################################################################
# Test SrcRect/DstRect are serialized as integers


@pytest.mark.parametrize("xSize,ySize", [(123456789, 1), (1, 123456789)])
def test_vrtmisc_14(tmp_vsimem, xSize, ySize):

    src_ds = gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "vrtmisc_14_src.tif",
        xSize,
        ySize,
        options=["SPARSE_OK=YES", "TILED=YES"],
    )
    gdal.GetDriverByName("VRT").CreateCopy(tmp_vsimem / "vrtmisc_14.vrt", src_ds)
    src_ds = None
    fp = gdal.VSIFOpenL(tmp_vsimem / "vrtmisc_14.vrt", "rb")
    content = gdal.VSIFReadL(1, 10000, fp).decode("latin1")
    gdal.VSIFCloseL(fp)

    assert f'<SrcRect xOff="0" yOff="0" xSize="{xSize}" ySize="{ySize}"' in content
    assert f'<DstRect xOff="0" yOff="0" xSize="{xSize}" ySize="{ySize}"' in content


###############################################################################
# Test rounding to closest int for coordinates


def test_vrtmisc_16(tmp_vsimem):

    gdal.BuildVRT(
        tmp_vsimem / "vrtmisc_16.vrt",
        ["data/vrtmisc16_tile1.tif", "data/vrtmisc16_tile2.tif"],
    )
    fp = gdal.VSIFOpenL(tmp_vsimem / "vrtmisc_16.vrt", "rb")
    content = gdal.VSIFReadL(1, 100000, fp).decode("latin1")
    gdal.VSIFCloseL(fp)

    assert '<SrcRect xOff="0" yOff="0" xSize="952" ySize="1189"' in content
    assert '<DstRect xOff="0" yOff="0" xSize="952" ySize="1189"' in content
    assert '<SrcRect xOff="0" yOff="0" xSize="494" ySize="893"' in content
    assert '<DstRect xOff="1680" yOff="5922" xSize="494" ySize="893"' in content

    gdal.GetDriverByName("GTiff").CreateCopy(
        tmp_vsimem / "vrtmisc_16.tif", gdal.Open(tmp_vsimem / "vrtmisc_16.vrt")
    )
    ds = gdal.Open(tmp_vsimem / "vrtmisc_16.tif")
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 206


def test_vrtmisc_16b(tmp_vsimem):

    gdal.FileFromMemBuffer(
        tmp_vsimem / "vrtmisc_16.vrt",
        """<VRTDataset rasterXSize="2174" rasterYSize="6815">
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
</VRTDataset>""",
    )
    gdal.GetDriverByName("GTiff").CreateCopy(
        tmp_vsimem / "vrtmisc_16.tif", gdal.Open(tmp_vsimem / "vrtmisc_16.vrt")
    )
    ds = gdal.Open(tmp_vsimem / "vrtmisc_16.tif")
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 206


###############################################################################
# Check that the serialized xml:VRT doesn't include itself (#6767)


def test_vrtmisc_17(tmp_vsimem):

    ds = gdal.Open("data/byte.tif")
    vrt_ds = gdal.GetDriverByName("VRT").CreateCopy(tmp_vsimem / "vrtmisc_17.vrt", ds)
    xml_vrt = vrt_ds.GetMetadata("xml:VRT")[0]
    vrt_ds = None

    assert "xml:VRT" not in xml_vrt


###############################################################################
# Check GetMetadata('xml:VRT') behaviour on a in-memory VRT copied from a VRT


def test_vrtmisc_18():

    ds = gdal.Open("data/byte.vrt")
    vrt_ds = gdal.GetDriverByName("VRT").CreateCopy("", ds)
    xml_vrt = vrt_ds.GetMetadata("xml:VRT")[0]
    assert gdal.GetLastErrorMsg() == ""
    vrt_ds = None

    assert (
        '<SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>' in xml_vrt
        or '<SourceFilename relativeToVRT="1">data\\byte.tif</SourceFilename>'
        in xml_vrt
    )


###############################################################################
# Check RAT support


def test_vrtmisc_rat(tmp_vsimem):

    ds = gdal.Translate(tmp_vsimem / "vrtmisc_rat.tif", "data/byte.tif", format="MEM")
    rat = gdal.RasterAttributeTable()
    rat.CreateColumn("Ints", gdal.GFT_Integer, gdal.GFU_Generic)
    ds.GetRasterBand(1).SetDefaultRAT(rat)

    vrt_ds = gdal.GetDriverByName("VRT").CreateCopy(tmp_vsimem / "vrtmisc_rat.vrt", ds)

    xml_vrt = vrt_ds.GetMetadata("xml:VRT")[0]
    assert gdal.GetLastErrorMsg() == ""
    vrt_ds = None

    assert '<GDALRasterAttributeTable tableType="thematic">' in xml_vrt

    vrt_ds = gdal.Translate(
        tmp_vsimem / "vrtmisc_rat.vrt", ds, format="VRT", srcWin=[0, 0, 1, 1]
    )

    xml_vrt = vrt_ds.GetMetadata("xml:VRT")[0]
    assert gdal.GetLastErrorMsg() == ""
    vrt_ds = None

    assert '<GDALRasterAttributeTable tableType="thematic">' in xml_vrt

    ds = None

    vrt_ds = gdal.Open(tmp_vsimem / "vrtmisc_rat.vrt", gdal.GA_Update)
    rat = vrt_ds.GetRasterBand(1).GetDefaultRAT()
    assert rat is not None and rat.GetColumnCount() == 1
    vrt_ds.GetRasterBand(1).SetDefaultRAT(None)
    assert vrt_ds.GetRasterBand(1).GetDefaultRAT() is None
    vrt_ds = None

    ds = None


###############################################################################
# Check ColorTable support


def test_vrtmisc_colortable():

    ds = gdal.Translate("", "data/byte.tif", format="VRT")
    ct = gdal.ColorTable()
    ct.SetColorEntry(0, (255, 255, 255, 255))
    ds.GetRasterBand(1).SetColorTable(ct)
    assert ds.GetRasterBand(1).GetColorTable().GetCount() == 1
    ds.GetRasterBand(1).SetColorTable(None)
    assert ds.GetRasterBand(1).GetColorTable() is None


###############################################################################
# Check histogram support


def test_vrtmisc_histogram(tmp_vsimem):

    tmpfile = tmp_vsimem / "vrtmisc_histogram.vrt"
    ds = gdal.Translate(tmpfile, "data/byte.tif", format="VRT")
    ds.GetRasterBand(1).SetDefaultHistogram(1, 2, [3000000000, 4])
    ds = None

    ds = gdal.Open(tmpfile)
    hist = ds.GetRasterBand(1).GetDefaultHistogram(force=0)
    ds = None

    assert hist == (1.0, 2.0, 2, [3000000000, 4])


###############################################################################
# write SRS with unusual data axis to SRS axis mapping


def test_vrtmisc_write_srs(tmp_vsimem):

    tmpfile = tmp_vsimem / "test_vrtmisc_write_srs.vrt"
    ds = gdal.Translate(tmpfile, "data/byte.tif", format="VRT")
    sr = osr.SpatialReference()
    sr.SetAxisMappingStrategy(osr.OAMS_AUTHORITY_COMPLIANT)
    sr.ImportFromEPSG(4326)
    ds.SetSpatialRef(sr)
    assert ds.FlushCache() == gdal.CE_None
    ds = None

    ds = gdal.Open(tmpfile)
    assert ds.GetSpatialRef().GetDataAxisToSRSAxisMapping() == [1, 2]
    ds = None


###############################################################################
# complex scenario involving masks and implicit overviews


def test_vrtmisc_mask_implicit_overviews(tmp_vsimem):

    ds = gdal.Translate(
        tmp_vsimem / "cog.tif",
        "data/stefan_full_rgba.tif",
        options="-outsize 2048 0 -b 1 -b 2 -b 3 -mask 4",
    )
    ds.BuildOverviews("NEAR", [2, 4])
    ds = None
    gdal.Translate(tmp_vsimem / "cog.vrt", tmp_vsimem / "cog.tif")
    ds = gdal.Open(tmp_vsimem / "cog.vrt")
    assert ds.GetRasterBand(1).GetOverview(0).GetMaskFlags() == gdal.GMF_PER_DATASET
    assert ds.GetRasterBand(1).GetMaskBand().IsMaskBand()
    assert ds.GetRasterBand(1).GetOverview(0).GetMaskBand().IsMaskBand()

    ds = None
    gdal.Translate(
        tmp_vsimem / "out.tif", tmp_vsimem / "cog.vrt", options="-b mask -outsize 10% 0"
    )
    gdal.GetDriverByName("GTiff").Delete(tmp_vsimem / "cog.tif")

    ds = gdal.Open(tmp_vsimem / "out.tif")
    histo = ds.GetRasterBand(1).GetDefaultHistogram()[3]
    # Check that there are only 0 and 255 in the histogram
    assert histo[0] + histo[255] == ds.RasterXSize * ds.RasterYSize, histo
    assert ds.GetRasterBand(1).Checksum() == 46885
    ds = None


###############################################################################
# Test setting block size


def test_vrtmisc_blocksize(tmp_vsimem):
    filename = tmp_vsimem / "test_vrtmisc_blocksize.vrt"
    vrt_ds = gdal.GetDriverByName("VRT").Create(filename, 50, 50, 0)
    options = ["subClass=VRTSourcedRasterBand", "blockXSize=32", "blockYSize=48"]
    vrt_ds.AddBand(gdal.GDT_UInt8, options)
    vrt_ds = None

    vrt_ds = gdal.Open(filename)
    blockxsize, blockysize = vrt_ds.GetRasterBand(1).GetBlockSize()
    assert blockxsize == 32
    assert blockysize == 48
    vrt_ds = None


###############################################################################
# Test setting block size through creation options


def test_vrtmisc_blocksize_creation_options(tmp_vsimem):
    filename = tmp_vsimem / "test_vrtmisc_blocksize_creation_options.vrt"
    vrt_ds = gdal.GetDriverByName("VRT").Create(
        filename, 50, 50, 1, options=["BLOCKXSIZE=32", "BLOCKYSIZE=48"]
    )
    vrt_ds = None

    vrt_ds = gdal.Open(filename)
    blockxsize, blockysize = vrt_ds.GetRasterBand(1).GetBlockSize()
    assert blockxsize == 32
    assert blockysize == 48
    vrt_ds = None


###############################################################################
# Test setting block size through creation options


def test_vrtmisc_blocksize_gdal_translate_direct(tmp_vsimem):
    filename = tmp_vsimem / "test_vrtmisc_blocksize_gdal_translate_direct.vrt"
    vrt_ds = gdal.Translate(
        filename, "data/byte.tif", creationOptions=["BLOCKXSIZE=32", "BLOCKYSIZE=48"]
    )
    vrt_ds = None

    vrt_ds = gdal.Open(filename)
    blockxsize, blockysize = vrt_ds.GetRasterBand(1).GetBlockSize()
    assert blockxsize == 32
    assert blockysize == 48
    vrt_ds = None


###############################################################################
# Test setting block size through creation options


def test_vrtmisc_blocksize_gdalbuildvrt(tmp_vsimem):
    filename = tmp_vsimem / "test_vrtmisc_blocksize_gdalbuildvrt.vrt"
    vrt_ds = gdal.BuildVRT(
        filename, ["data/byte.tif"], creationOptions=["BLOCKXSIZE=32", "BLOCKYSIZE=48"]
    )
    vrt_ds = None

    vrt_ds = gdal.Open(filename)
    blockxsize, blockysize = vrt_ds.GetRasterBand(1).GetBlockSize()
    assert blockxsize == 32
    assert blockysize == 48
    vrt_ds = None


###############################################################################
# Test setting block size through creation options


def test_vrtmisc_blocksize_gdal_translate_indirect(tmp_vsimem):
    filename = tmp_vsimem / "test_vrtmisc_blocksize_gdal_translate_indirect.vrt"
    vrt_ds = gdal.Translate(
        filename,
        "data/byte.tif",
        metadataOptions=["FOO=BAR"],
        creationOptions=["BLOCKXSIZE=32", "BLOCKYSIZE=48"],
    )
    vrt_ds = None

    vrt_ds = gdal.Open(filename)
    blockxsize, blockysize = vrt_ds.GetRasterBand(1).GetBlockSize()
    assert blockxsize == 32
    assert blockysize == 48
    vrt_ds = None


###############################################################################
# Test replicating source block size if we subset with an offset multiple
# of the source block size


def test_vrtmisc_blocksize_gdal_translate_implicit(tmp_vsimem):

    src_filename = tmp_vsimem / "src.tif"
    gdal.GetDriverByName("GTiff").Create(
        src_filename, 128, 512, options=["TILED=YES", "BLOCKXSIZE=48", "BLOCKYSIZE=256"]
    )
    filename = tmp_vsimem / "test_vrtmisc_blocksize_gdal_translate_implicit.vrt"
    vrt_ds = gdal.Translate(filename, src_filename, srcWin=[48 * 2, 256, 100, 256])
    vrt_ds = None

    vrt_ds = gdal.Open(filename)
    blockxsize, blockysize = vrt_ds.GetRasterBand(1).GetBlockSize()
    assert blockxsize == 48
    assert blockysize == 256
    vrt_ds = None


###############################################################################
# Test support for coordinate epoch


def test_vrtmisc_coordinate_epoch(tmp_vsimem):

    filename = tmp_vsimem / "temp.vrt"
    gdal.Translate(filename, "data/byte.tif", options="-a_coord_epoch 2021.3")
    ds = gdal.Open(filename)
    srs = ds.GetSpatialRef()
    assert srs.GetCoordinateEpoch() == 2021.3
    ds = None


###############################################################################
# Test the relativeToVRT attribute of SourceFilename


def test_vrtmisc_sourcefilename_all_relatives(tmp_path):

    shutil.copy("data/byte.tif", tmp_path)

    src_ds = gdal.Open(tmp_path / "byte.tif")
    ds = gdal.GetDriverByName("VRT").CreateCopy("", src_ds)
    ds.SetDescription(f"{tmp_path}/byte.vrt")
    ds = None
    src_ds = None
    assert (
        '<SourceFilename relativeToVRT="1">byte.tif<'
        in open(tmp_path / "byte.vrt", "rt").read()
    )


###############################################################################
# Test the relativeToVRT attribute of SourceFilename


def test_vrtmisc_sourcefilename_source_relative_dest_absolute():

    shutil.copy("data/byte.tif", "tmp")

    try:
        src_ds = gdal.Open(os.path.join("tmp", "byte.tif"))
        ds = gdal.GetDriverByName("VRT").CreateCopy("", src_ds)
        path = os.path.join(os.getcwd(), "tmp", "byte.vrt")
        if sys.platform == "win32":
            path = path.replace("/", "\\")
        ds.SetDescription(path)
        ds = None
        src_ds = None
        assert (
            '<SourceFilename relativeToVRT="1">byte.tif<'
            in open("tmp/byte.vrt", "rt").read()
        )
    finally:
        gdal.Unlink("tmp/byte.tif")
        gdal.Unlink("tmp/byte.vrt")


###############################################################################
# Test the relativeToVRT attribute of SourceFilename


def test_vrtmisc_sourcefilename_source_absolute_dest_absolute():

    shutil.copy("data/byte.tif", "tmp")

    try:
        src_ds = gdal.Open(os.path.join(os.getcwd(), "tmp", "byte.tif"))
        ds = gdal.GetDriverByName("VRT").CreateCopy("", src_ds)
        ds.SetDescription(os.path.join(os.getcwd(), "tmp", "byte.vrt"))
        ds = None
        src_ds = None
        assert (
            '<SourceFilename relativeToVRT="1">byte.tif<'
            in open("tmp/byte.vrt", "rt").read()
        )
    finally:
        gdal.Unlink("tmp/byte.tif")
        gdal.Unlink("tmp/byte.vrt")


###############################################################################
# Test the relativeToVRT attribute of SourceFilename


def test_vrtmisc_sourcefilename_source_absolute_dest_relative():

    shutil.copy("data/byte.tif", "tmp")

    try:
        path = os.path.join(os.getcwd(), "tmp", "byte.tif")
        if sys.platform == "win32":
            path = path.replace("/", "\\")
        src_ds = gdal.Open(path)
        ds = gdal.GetDriverByName("VRT").CreateCopy("", src_ds)
        ds.SetDescription(os.path.join("tmp", "byte.vrt"))
        ds = None
        src_ds = None
        assert (
            '<SourceFilename relativeToVRT="1">byte.tif<'
            in open("tmp/byte.vrt", "rt").read()
        )
    finally:
        gdal.Unlink("tmp/byte.tif")
        gdal.Unlink("tmp/byte.vrt")


###############################################################################
# Test Int64 nodata


def test_vrtmisc_nodata_int64(tmp_vsimem):

    filename = tmp_vsimem / "temp.vrt"
    ds = gdal.Translate(
        filename, "data/byte.tif", format="VRT", outputType=gdal.GDT_Int64
    )
    val = -(1 << 63)
    assert ds.GetRasterBand(1).SetNoDataValue(val) == gdal.CE_None
    assert ds.GetRasterBand(1).GetNoDataValue() == val
    ds = None

    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetNoDataValue() == val
    ds = None


###############################################################################
# Test UInt64 nodata


def test_vrtmisc_nodata_uint64(tmp_vsimem):

    filename = tmp_vsimem / "temp.vrt"
    ds = gdal.Translate(
        filename, "data/byte.tif", format="VRT", outputType=gdal.GDT_UInt64
    )
    val = (1 << 64) - 1
    assert ds.GetRasterBand(1).SetNoDataValue(val) == gdal.CE_None
    assert ds.GetRasterBand(1).GetNoDataValue() == val
    ds = None

    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetNoDataValue() == val
    ds = None


###############################################################################
# Check IsMaskBand() on an alpha band


def test_vrtmisc_alpha_ismaskband():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 2)
    src_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)
    ds = gdal.GetDriverByName("VRT").CreateCopy("", src_ds)
    assert not ds.GetRasterBand(1).IsMaskBand()
    assert ds.GetRasterBand(2).IsMaskBand()


###############################################################################
# Check that serialization of a ComplexSource with NODATA (generally) doesn't
# require opening the source band.


def test_vrtmisc_serialize_complexsource_with_NODATA(tmp_vsimem):

    tmpfilename = tmp_vsimem / "test_vrtmisc_serialize_complexsource_with_NODATA.vrt"
    gdal.FileFromMemBuffer(
        tmpfilename,
        """<VRTDataset rasterXSize="1" rasterYSize="1">
  <VRTRasterBand dataType="Byte" band="1">
    <NoDataValue>0</NoDataValue>
    <ComplexSource>
      <SourceFilename relativeToVRT="1">i_do_not_exist.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="1" ySize="1" />
      <DstRect xOff="0" yOff="0" xSize="1" ySize="1" />
      <NODATA>1</NODATA>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>""",
    )

    ds = gdal.Open(tmpfilename)
    ds.GetRasterBand(1).SetDescription("foo")
    gdal.ErrorReset()
    ds = None
    assert gdal.GetLastErrorMsg() == ""

    fp = gdal.VSIFOpenL(tmpfilename, "rb")
    content = gdal.VSIFReadL(1, 10000, fp).decode("latin1")
    gdal.VSIFCloseL(fp)
    gdal.Unlink(tmpfilename)

    # print(content)
    assert "<NODATA>1</NODATA>" in content


###############################################################################
# Test bugfix for https://github.com/OSGeo/gdal/issues/7486


def test_vrtmisc_nodata_float32(tmp_vsimem):

    tif_filename = tmp_vsimem / "test_vrtmisc_nodata_float32.tif"
    ds = gdal.GetDriverByName("GTiff").Create(tif_filename, 1, 1, 1, gdal.GDT_Float32)
    nodata = -0.1
    ds.GetRasterBand(1).SetNoDataValue(nodata)
    ds.GetRasterBand(1).Fill(nodata)
    ds = None

    # When re-opening the TIF file, the -0.1 double value will be exposed
    # with the float32 precision (~ -0.10000000149011612)
    vrt_filename = tmp_vsimem / "test_vrtmisc_nodata_float32.vrt"
    ds = gdal.Translate(vrt_filename, tif_filename)
    nodata_vrt = ds.GetRasterBand(1).GetNoDataValue()
    assert nodata_vrt == struct.unpack("f", struct.pack("f", nodata))[0]
    ds = None

    # Check that this is still the case after above serialization to .vrt
    # and re-opening. That is check that we serialize the rounded value with
    # full double precision (%.17g)
    ds = gdal.Open(vrt_filename)
    nodata_vrt = ds.GetRasterBand(1).GetNoDataValue()
    assert nodata_vrt == struct.unpack("f", struct.pack("f", nodata))[0]
    ds = None


###############################################################################
def test_vrt_write_copy_mdd(tmp_vsimem):

    src_filename = tmp_vsimem / "test_vrt_write_copy_mdd.tif"
    src_ds = gdal.GetDriverByName("GTiff").Create(src_filename, 1, 1)
    src_ds.SetMetadataItem("FOO", "BAR")
    src_ds.SetMetadataItem("BAR", "BAZ", "OTHER_DOMAIN")
    src_ds.SetMetadataItem("should_not", "be_copied", "IMAGE_STRUCTURE")

    filename = tmp_vsimem / "test_vrt_write_copy_mdd.vrt"

    gdal.GetDriverByName("VRT").CreateCopy(filename, src_ds)
    ds = gdal.Open(filename)
    assert set(ds.GetMetadataDomainList()) == set(
        ["", "IMAGE_STRUCTURE", "DERIVED_SUBDATASETS"]
    )
    assert ds.GetMetadata_Dict() == {"FOO": "BAR"}
    assert ds.GetMetadata_Dict("OTHER_DOMAIN") == {}
    assert ds.GetMetadata_Dict("IMAGE_STRUCTURE") == {"INTERLEAVE": "BAND"}
    ds = None

    gdal.GetDriverByName("VRT").CreateCopy(
        filename, src_ds, options=["COPY_SRC_MDD=NO"]
    )
    ds = gdal.Open(filename)
    assert ds.GetMetadata_Dict() == {}
    assert ds.GetMetadata_Dict("OTHER_DOMAIN") == {}
    ds = None

    gdal.GetDriverByName("VRT").CreateCopy(
        filename, src_ds, options=["COPY_SRC_MDD=YES"]
    )
    ds = gdal.Open(filename)
    assert set(ds.GetMetadataDomainList()) == set(
        ["", "IMAGE_STRUCTURE", "DERIVED_SUBDATASETS", "OTHER_DOMAIN"]
    )
    assert ds.GetMetadata_Dict() == {"FOO": "BAR"}
    assert ds.GetMetadata_Dict("OTHER_DOMAIN") == {"BAR": "BAZ"}
    assert ds.GetMetadata_Dict("IMAGE_STRUCTURE") == {"INTERLEAVE": "BAND"}
    ds = None

    gdal.GetDriverByName("VRT").CreateCopy(
        filename, src_ds, options=["SRC_MDD=OTHER_DOMAIN"]
    )
    ds = gdal.Open(filename)
    assert ds.GetMetadata_Dict() == {}
    assert ds.GetMetadata_Dict("OTHER_DOMAIN") == {"BAR": "BAZ"}
    ds = None

    gdal.GetDriverByName("VRT").CreateCopy(
        filename, src_ds, options=["SRC_MDD=", "SRC_MDD=OTHER_DOMAIN"]
    )
    ds = gdal.Open(filename)
    assert ds.GetMetadata_Dict() == {"FOO": "BAR"}
    assert ds.GetMetadata_Dict("OTHER_DOMAIN") == {"BAR": "BAZ"}
    ds = None

    src_ds = None


###############################################################################


@pytest.mark.require_driver("netCDF")
@pytest.mark.skipif(
    os.environ.get("BUILD_NAME", "") == "s390x",
    reason="Fails on that platform",
)
def test_vrt_read_netcdf(tmp_path):
    """Test subdataset info API used by VRT driver to calculate relative path"""

    nc_path = os.path.join(tmp_path, "alldatatypes.nc")
    vrt_path = os.path.join(tmp_path, "test_vrt_read_netcdf.vrt")
    shutil.copyfile(
        Path(__file__).parent.parent / "gdrivers/data/netcdf/alldatatypes.nc",
        nc_path,
    )
    buffer = """<VRTDataset rasterXSize="1" rasterYSize="1">
  <VRTRasterBand dataType="Byte" band="1">
    <NoDataValue>0</NoDataValue>
    <ComplexSource>
      <SourceFilename relativeToVRT="1">NETCDF:"alldatatypes.nc":ubyte_var</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="1" ySize="1" />
      <DstRect xOff="0" yOff="0" xSize="1" ySize="1" />
      <NODATA>1</NODATA>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>"""
    with open(vrt_path, "w+") as f:
        f.write(buffer)

    ds = gdal.Open(vrt_path)
    assert ds is not None
    assert ds.GetRasterBand(1).Checksum() == 3


###############################################################################


def test_vrtmisc_add_band_gdt_unknown():
    vrt_ds = gdal.GetDriverByName("VRT").Create("", 1, 1, 0)
    options = ["subClass=VRTSourcedRasterBand", "blockXSize=32", "blockYSize=48"]
    with pytest.raises(Exception, match="Illegal GDT_Unknown/GDT_TypeCount argument"):
        vrt_ds.AddBand(gdal.GDT_Unknown, options)


###############################################################################


def test_vrtmisc_virtual_overview_mask_band_as_regular_band(tmp_vsimem):

    src_filename = tmp_vsimem / "src.tif"
    with gdal.GetDriverByName("GTiff").Create(src_filename, 256, 256, 3) as src_ds:
        src_ds.CreateMaskBand(gdal.GMF_PER_DATASET)
        src_ds.BuildOverviews("NEAR", [2])

    vrt_ds = gdal.Translate("", src_filename, options="-of VRT -b 1 -b 2 -b 3 -b mask")
    assert vrt_ds.GetRasterBand(1).GetOverviewCount() == 1
    assert vrt_ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_AlphaBand
