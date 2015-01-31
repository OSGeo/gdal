#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test mask bands in VRT driver 
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Test with a global dataset mask band

def vrtmask_1():

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
    if ds.GetRasterBand(1).GetMaskFlags() != gdal.GMF_PER_DATASET:
        gdaltest.post_reason('did not get expected mask flags')
        print(ds.GetRasterBand(1).GetMaskFlags())
        return 'fail'

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    if cs != 4672:
        gdaltest.post_reason('did not get expected mask band checksum')
        print(cs)
        return 'fail'

    ds = None

    return 'success'


###############################################################################
# Test with a per band mask band

def vrtmask_2():

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
    if ds.GetRasterBand(1).GetMaskFlags() != 0:
        gdaltest.post_reason('did not get expected mask flags')
        print(ds.GetRasterBand(1).GetMaskFlags())
        return 'fail'

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    if cs != 4873:
        gdaltest.post_reason('did not get expected mask band checksum')
        print(cs)
        return 'fail'

    ds = None

    return 'success'
    
###############################################################################
# Translate a RGB dataset with a mask into a VRT

def vrtmask_3():

    gtiff_drv = gdal.GetDriverByName('GTiff')
    md = gtiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

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

    if msk_cs != expected_msk_cs:
        gdaltest.post_reason('did not get expected mask band checksum')
        print(msk_cs)
        print(expected_msk_cs)
        return 'fail'

    return 'success'

###############################################################################
# Same with gdalbuildvrt

def vrtmask_4():
    import test_cli_utilities
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        return 'skip'

    gtiff_drv = gdal.GetDriverByName('GTiff')
    md = gtiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' tmp/vrtmask_4.vrt ../gcore/data/ycbcr_with_mask.tif')

    src_ds = gdal.Open('../gcore/data/ycbcr_with_mask.tif')
    expected_msk_cs = src_ds.GetRasterBand(1).GetMaskBand().Checksum()
    src_ds = None

    ds = gdal.Open('tmp/vrtmask_4.vrt')
    msk_cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    ds = None

    os.remove('tmp/vrtmask_4.vrt')

    if msk_cs != expected_msk_cs:
        gdaltest.post_reason('did not get expected mask band checksum')
        print(msk_cs)
        print(expected_msk_cs)
        return 'fail'

    return 'success'

###############################################################################
# Same with gdal_translate

def vrtmask_5():
    import test_cli_utilities
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gtiff_drv = gdal.GetDriverByName('GTiff')
    md = gtiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -of VRT ../gcore/data/ycbcr_with_mask.tif tmp/vrtmask_5.vrt -outsize 100% 100%')

    src_ds = gdal.Open('../gcore/data/ycbcr_with_mask.tif')
    expected_msk_cs = src_ds.GetRasterBand(1).GetMaskBand().Checksum()
    src_ds = None

    ds = gdal.Open('tmp/vrtmask_5.vrt')
    msk_cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    ds = None

    os.remove('tmp/vrtmask_5.vrt')

    if msk_cs != expected_msk_cs:
        gdaltest.post_reason('did not get expected mask band checksum')
        print(msk_cs)
        print(expected_msk_cs)
        return 'fail'

    return 'success'

###############################################################################
# Same with gdal_translate with explicit -b and -mask arguments

def vrtmask_6():
    import test_cli_utilities
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gtiff_drv = gdal.GetDriverByName('GTiff')
    md = gtiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -of VRT ../gcore/data/ycbcr_with_mask.tif tmp/vrtmask_6.vrt -b 1 -b 2 -b 3 -mask mask,1')

    src_ds = gdal.Open('../gcore/data/ycbcr_with_mask.tif')
    expected_msk_cs = src_ds.GetRasterBand(1).GetMaskBand().Checksum()
    src_ds = None

    ds = gdal.Open('tmp/vrtmask_6.vrt')
    msk_cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    ds = None

    os.remove('tmp/vrtmask_6.vrt')

    if msk_cs != expected_msk_cs:
        gdaltest.post_reason('did not get expected mask band checksum')
        print(msk_cs)
        print(expected_msk_cs)
        return 'fail'

    return 'success'

###############################################################################
# gdal_translate with RGBmask -> RGBA and then RGBA->RGBmask

def vrtmask_7():
    import test_cli_utilities
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gtiff_drv = gdal.GetDriverByName('GTiff')
    md = gtiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'
    try:
        os.remove('tmp/vrtmask_7_rgba.tif.msk')
    except:
        pass

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' ../gcore/data/ycbcr_with_mask.tif tmp/vrtmask_7_rgba.tif -b 1 -b 2 -b 3 -b mask')

    src_ds = gdal.Open('../gcore/data/ycbcr_with_mask.tif')
    expected_msk_cs = src_ds.GetRasterBand(1).GetMaskBand().Checksum()
    src_ds = None

    ds = gdal.Open('tmp/vrtmask_7_rgba.tif')
    alpha_cs = ds.GetRasterBand(4).Checksum()
    ds = None

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -of VRT tmp/vrtmask_7_rgba.tif tmp/vrtmask_7_rgbmask.vrt -b 1 -b 2 -b 3 -mask 4')
    
    ds = gdal.Open('tmp/vrtmask_7_rgbmask.vrt')
    msk_cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    ds = None

    os.remove('tmp/vrtmask_7_rgba.tif')
    try:
        os.remove('tmp/vrtmask_7_rgba.tif.msk')
        gdaltest.post_reason('did not expect tmp/vrtmask_7_rgba.tif.msk')
        return 'fail'
    except:
        pass
    os.remove('tmp/vrtmask_7_rgbmask.vrt')

    if alpha_cs != expected_msk_cs:
        gdaltest.post_reason('did not get expected alpha band checksum')
        print(alpha_cs)
        print(expected_msk_cs)
        return 'fail'

    if msk_cs!= expected_msk_cs:
        gdaltest.post_reason('did not get expected mask band checksum')
        print(msk_cs)
        print(expected_msk_cs)
        return 'fail'
        
    return 'success'

###############################################################################
# gdal_translate with RGBmask -> RGB

def vrtmask_8():
    import test_cli_utilities
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gtiff_drv = gdal.GetDriverByName('GTiff')
    md = gtiff_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -of VRT ../gcore/data/ycbcr_with_mask.tif tmp/vrtmask_8.vrt -mask none')

    ds = gdal.Open('tmp/vrtmask_8.vrt')
    flags = ds.GetRasterBand(1).GetMaskFlags()
    ds = None

    os.remove('tmp/vrtmask_8.vrt')
    
    if flags != gdal.GMF_ALL_VALID:
        print(flags)
        return 'fail'
        
    return 'success'
    
###############################################################################
# gdal_translate with RGBA -> RGB

def vrtmask_9():
    import test_cli_utilities
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'
        
    src_ds = gdal.GetDriverByName('GTiff').Create('tmp/vrtmask_9_src.tif', 10, 10, 4)
    del src_ds
    
    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path() + ' tmp/vrtmask_9_src.tif tmp/vrtmask_9_dst.tif -b 1 -b 2 -b 3')

    ds = gdal.Open('tmp/vrtmask_9_dst.tif')
    flags = ds.GetRasterBand(1).GetMaskFlags()
    ds = None

    os.remove('tmp/vrtmask_9_src.tif')
    os.remove('tmp/vrtmask_9_dst.tif')
    if err != '':
        gdaltest.post_reason('unexpected output on standard err')
        print(err)
        return 'fail'

    if flags != gdal.GMF_ALL_VALID:
        print(flags)
        return 'fail'
        
    return 'success'
    
###############################################################################
# Test fix for #5120 (VRTSourcedRasterBand::AddMaskBandSource() ignores specified window)

def vrtmask_10():
    import test_cli_utilities
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path() + ' ../gcore/data/stefan_full_rgba.tif tmp/vrtmask_10_ref.tif -srcwin 40 40 100 100')
    gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path() + ' ../gcore/data/stefan_full_rgba.tif tmp/vrtmask_10.vrt -of vrt -b 1 -b 2 -b 3 -mask 4 -srcwin 30 30 120 120')
    gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path() + ' tmp/vrtmask_10.vrt tmp/vrtmask_10_2.vrt  -of vrt -srcwin 5 5 110 110')
    gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_translate_path() + ' tmp/vrtmask_10_2.vrt tmp/vrtmask_10_3.tif -b 1 -b 2 -b 3 -b mask -srcwin 5 5 100 100')

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

    if cs_ref != cs_got:
        return 'fail'

    return 'success'

###############################################################################
# Cleanup.

def vrtmask_cleanup():
    return 'success'

gdaltest_list = [
    vrtmask_1,
    vrtmask_2,
    vrtmask_3,
    vrtmask_4,
    vrtmask_5,
    vrtmask_6,
    vrtmask_7,
    vrtmask_8,
    vrtmask_9,
    vrtmask_10,
    vrtmask_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'vrtmask' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

