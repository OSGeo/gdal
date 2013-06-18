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
# Copyright (c) 2013, Even Rouault <even dot rouault at mines dash paris dot org>
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
import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Test linear scaling

def vrtmisc_1():
    import test_cli_utilities
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -of VRT data/byte.tif tmp/vrtmisc_1.vrt -scale 74 255 0 255')

    ds = gdal.Open('tmp/vrtmisc_1.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    os.remove('tmp/vrtmisc_1.vrt')

    if cs != 4323:
        gdaltest.post_reason('did not get expected checksum')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Test power scaling

def vrtmisc_2():
    import test_cli_utilities
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -of VRT data/byte.tif tmp/vrtmisc_2.vrt -scale 74 255 0 255 -exponent 2.2')

    ds = gdal.Open('tmp/vrtmisc_2.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    os.remove('tmp/vrtmisc_2.vrt')

    if cs != 4159:
        gdaltest.post_reason('did not get expected checksum')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Test power scaling (not <SrcMin> <SrcMax> in VRT file)

def vrtmisc_3():
    import test_cli_utilities
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

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
# Cleanup.

def vrtmisc_cleanup():
    return 'success'

gdaltest_list = [
    vrtmisc_1,
    vrtmisc_2,
    vrtmisc_3,
    vrtmisc_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'vrtmisc' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

