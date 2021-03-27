#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test mask bands in VRT driver
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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
from osgeo import gdal


import gdaltest
import pytest

###############################################################################
# Test with a global dataset mask band


def test_vrtmask_1():

    vrt_string = """<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <ColorInterp>Gray</ColorInterp>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>
  </VRTRasterBand>
  <MaskBand>
      <VRTRasterBand dataType="Byte">
        <SimpleSource>
          <SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>
          <SourceBand>1</SourceBand> <!-- here we use band 1 of the sourcefilename as a mask band -->
          <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
          <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
          <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
        </SimpleSource>
      </VRTRasterBand>
  </MaskBand>
</VRTDataset>"""

    ds = gdal.Open(vrt_string)
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_PER_DATASET, \
        'did not get expected mask flags'

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    assert cs == 4672, 'did not get expected mask band checksum'

    ds = None


###############################################################################
# Test with a per band mask band

def test_vrtmask_2():

    vrt_string = """<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <ColorInterp>Gray</ColorInterp>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>
    <MaskBand>
        <VRTRasterBand dataType="Byte">
            <SimpleSource>
                <SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>
                <SourceBand>mask,1</SourceBand> <!-- note the mask,1 meaning the mask band of the band 1 of the sourcefilename -->
                <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
                <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
                <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
             </SimpleSource>
        </VRTRasterBand>
    </MaskBand>
  </VRTRasterBand>
</VRTDataset>"""

    ds = gdal.Open(vrt_string)
    assert ds.GetRasterBand(1).GetMaskFlags() == 0, 'did not get expected mask flags'

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    assert cs == 4873, 'did not get expected mask band checksum'

    ds = None

###############################################################################
# Translate a RGB dataset with a mask into a VRT


def test_vrtmask_3():

    gtiff_drv = gdal.GetDriverByName('GTiff')
    md = gtiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    src_ds = gdal.Open('../gcore/data/ycbcr_with_mask.tif')
    ds = gdal.GetDriverByName('VRT').CreateCopy('tmp/vrtmask_3.vrt', src_ds)
    ds = None
    expected_msk_cs = src_ds.GetRasterBand(1).GetMaskBand().Checksum()
    src_ds = None

    ds = gdal.Open('tmp/vrtmask_3.vrt', gdal.GA_Update)
    ds.GetRasterBand(1).SetDescription('foo')
    ds = None

    ds = gdal.Open('tmp/vrtmask_3.vrt')
    msk_cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    ds = None

    os.remove('tmp/vrtmask_3.vrt')

    assert msk_cs == expected_msk_cs, 'did not get expected mask band checksum'

###############################################################################
# Same with gdalbuildvrt


def test_vrtmask_4():

    gtiff_drv = gdal.GetDriverByName('GTiff')
    md = gtiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    src_ds = gdal.Open('../gcore/data/ycbcr_with_mask.tif')
    gdal.BuildVRT('tmp/vrtmask_4.vrt', [src_ds])
    expected_msk_cs = src_ds.GetRasterBand(1).GetMaskBand().Checksum()
    src_ds = None

    ds = gdal.Open('tmp/vrtmask_4.vrt')
    msk_cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    ds = None

    os.remove('tmp/vrtmask_4.vrt')

    assert msk_cs == expected_msk_cs, 'did not get expected mask band checksum'

###############################################################################
# Same with gdal_translate


def test_vrtmask_5():

    gtiff_drv = gdal.GetDriverByName('GTiff')
    md = gtiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    gdal.Translate('tmp/vrtmask_5.vrt', '../gcore/data/ycbcr_with_mask.tif', options='-of VRT -outsize 100% 100%')

    src_ds = gdal.Open('../gcore/data/ycbcr_with_mask.tif')
    expected_msk_cs = src_ds.GetRasterBand(1).GetMaskBand().Checksum()
    src_ds = None

    ds = gdal.Open('tmp/vrtmask_5.vrt')
    msk_cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    ds = None

    os.remove('tmp/vrtmask_5.vrt')

    assert msk_cs == expected_msk_cs, 'did not get expected mask band checksum'

###############################################################################
# Same with gdal_translate with explicit -b and -mask arguments


def test_vrtmask_6():

    gtiff_drv = gdal.GetDriverByName('GTiff')
    md = gtiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    gdal.Translate('tmp/vrtmask_6.vrt', '../gcore/data/ycbcr_with_mask.tif',
                   options='-of VRT -b 1 -b 2 -b 3 -mask mask,1')

    src_ds = gdal.Open('../gcore/data/ycbcr_with_mask.tif')
    expected_msk_cs = src_ds.GetRasterBand(1).GetMaskBand().Checksum()
    src_ds = None

    ds = gdal.Open('tmp/vrtmask_6.vrt')
    msk_cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    ds = None

    os.remove('tmp/vrtmask_6.vrt')

    assert msk_cs == expected_msk_cs, 'did not get expected mask band checksum'

###############################################################################
# gdal_translate with RGBmask -> RGBA and then RGBA->RGBmask


def test_vrtmask_7():

    gtiff_drv = gdal.GetDriverByName('GTiff')
    md = gtiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()
    try:
        os.remove('tmp/vrtmask_7_rgba.tif.msk')
    except OSError:
        pass

    gdal.Translate('tmp/vrtmask_7_rgba.tif',
                   '../gcore/data/ycbcr_with_mask.tif',
                   options='-b 1 -b 2 -b 3 -b mask')

    src_ds = gdal.Open('../gcore/data/ycbcr_with_mask.tif')
    expected_msk_cs = src_ds.GetRasterBand(1).GetMaskBand().Checksum()
    src_ds = None

    ds = gdal.Open('tmp/vrtmask_7_rgba.tif')
    alpha_cs = ds.GetRasterBand(4).Checksum()
    ds = None

    gdal.Translate('tmp/vrtmask_7_rgbmask.vrt', 'tmp/vrtmask_7_rgba.tif',
                   options='-of VRT -b 1 -b 2 -b 3 -mask 4')

    ds = gdal.Open('tmp/vrtmask_7_rgbmask.vrt')
    msk_cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    ds = None

    os.remove('tmp/vrtmask_7_rgba.tif')
    with pytest.raises(OSError):
        os.remove('tmp/vrtmask_7_rgba.tif.msk')
        pytest.fail('did not expect tmp/vrtmask_7_rgba.tif.msk')

    os.remove('tmp/vrtmask_7_rgbmask.vrt')

    assert alpha_cs == expected_msk_cs, 'did not get expected alpha band checksum'

    assert msk_cs == expected_msk_cs, 'did not get expected mask band checksum'

###############################################################################
# gdal_translate with RGBmask -> RGB


def test_vrtmask_8():

    gtiff_drv = gdal.GetDriverByName('GTiff')
    md = gtiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        pytest.skip()

    gdal.Translate('tmp/vrtmask_8.vrt', '../gcore/data/ycbcr_with_mask.tif',
                   options='-of VRT -mask none')

    ds = gdal.Open('tmp/vrtmask_8.vrt')
    flags = ds.GetRasterBand(1).GetMaskFlags()
    ds = None

    os.remove('tmp/vrtmask_8.vrt')

    assert flags == gdal.GMF_ALL_VALID

###############################################################################
# gdal_translate with RGBA -> RGB


def test_vrtmask_9():
    import test_cli_utilities
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    src_ds = gdal.GetDriverByName('GTiff').Create('tmp/vrtmask_9_src.tif', 10, 10, 4)
    del src_ds

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path() + ' tmp/vrtmask_9_src.tif tmp/vrtmask_9_dst.tif -b 1 -b 2 -b 3')

    ds = gdal.Open('tmp/vrtmask_9_dst.tif')
    flags = ds.GetRasterBand(1).GetMaskFlags()
    ds = None

    os.remove('tmp/vrtmask_9_src.tif')
    os.remove('tmp/vrtmask_9_dst.tif')
    assert err == '', 'unexpected output on standard err'

    assert flags == gdal.GMF_ALL_VALID

###############################################################################
# Test fix for #5120 (VRTSourcedRasterBand::AddMaskBandSource() ignores specified window)


def test_vrtmask_10():

    gdal.Translate('tmp/vrtmask_10_ref.tif', '../gcore/data/stefan_full_rgba.tif', options='-srcwin 40 40 100 100')
    gdal.Translate('tmp/vrtmask_10.vrt', '../gcore/data/stefan_full_rgba.tif', options='-of VRT -b 1 -b 2 -b 3 -mask 4 -srcwin 30 30 120 120')
    gdal.Translate('tmp/vrtmask_10_2.vrt', 'tmp/vrtmask_10.vrt', options='-of VRT -srcwin 5 5 110 110')
    gdal.Translate('tmp/vrtmask_10_3.tif', 'tmp/vrtmask_10_2.vrt', options='-b 1 -b 2 -b 3 -b mask -srcwin 5 5 100 100')

    ds = gdal.Open('tmp/vrtmask_10_ref.tif')
    cs_ref = ds.GetRasterBand(4).Checksum()
    ds = None
    ds = gdal.Open('tmp/vrtmask_10_3.tif')
    cs_got = ds.GetRasterBand(4).Checksum()
    ds = None

    os.remove('tmp/vrtmask_10_ref.tif')
    os.remove('tmp/vrtmask_10.vrt')
    os.remove('tmp/vrtmask_10_2.vrt')
    os.remove('tmp/vrtmask_10_3.tif')

    assert cs_ref == cs_got

###############################################################################


def test_vrtmask_11():

    # Cannot create mask band at raster band level when a dataset mask band already exists
    ds = gdal.Translate('', 'data/byte.tif', format='VRT')
    ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    with gdaltest.error_handler():
        ret = ds.GetRasterBand(1).CreateMaskBand(0)
    assert ret != 0, 'expected an error, but got success'

    # This VRT dataset has already a mask band
    ds = gdal.Translate('', 'data/byte.tif', format='VRT')
    ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    with gdaltest.error_handler():
        ret = ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    assert ret != 0, 'expected an error, but got success'

    # This VRT band has already a mask band
    ds = gdal.Translate('', 'data/byte.tif', format='VRT')
    ds.GetRasterBand(1).CreateMaskBand(0)
    with gdaltest.error_handler():
        ret = ds.GetRasterBand(1).CreateMaskBand(0)
    assert ret != 0, 'expected an error, but got success'

    ds = gdal.Translate('', 'data/byte.tif', format='VRT')
    ret = ds.GetRasterBand(1).CreateMaskBand(gdal.GMF_PER_DATASET)
    assert ret == 0

###############################################################################
# Cleanup.


def test_vrtmask_cleanup():
    pass


