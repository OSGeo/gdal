#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GDAL API PROXY mechanism
# Author:    Even Rouault, <even dot rouault at mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2013, Even Rouault, <even dot rouault at mines-paris dot org>
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
#
def gdal_api_proxy_start():

    import test_py_scripts
    ret = test_py_scripts.run_py_script_as_external_script('.', 'gdal_api_proxy', ' -api_proxy', display_live_on_parent_stdout = True)

    if ret.find('Failed:    0') == -1:
        return 'fail'

    return 'success'

###############################################################################
#
def gdal_api_proxy_1():

    src_ds = gdal.Open('data/byte.tif')
    src_cs = src_ds.GetRasterBand(1).Checksum()
    src_gt = src_ds.GetGeoTransform()
    src_prj = src_ds.GetProjectionRef()
    src_data = src_ds.ReadRaster(0, 0, 20, 20)
    src_md = src_ds.GetMetadata()
    src_ds = None

    gdal.SetConfigOption('GDAL_API_PROXY', 'YES')

    drv = gdal.IdentifyDriver('data/byte.tif')
    if drv.GetDescription() != 'API_PROXY':
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.GetDriverByName('GTiff').Create('tmp/byte.tif', 1, 1, 3)
    ds = None

    src_ds = gdal.Open('data/byte.tif')
    ds = gdal.GetDriverByName('GTiff').CreateCopy('tmp/byte.tif', src_ds, options = ['TILED=YES'])
    got_cs = ds.GetRasterBand(1).Checksum()
    if src_cs != got_cs:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = gdal.Open('tmp/byte.tif', gdal.GA_Update)

    ds.SetGeoTransform([1,2,3,4,5,6])
    got_gt = ds.GetGeoTransform()
    if src_gt == got_gt:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.SetGeoTransform(src_gt)
    got_gt = ds.GetGeoTransform()
    if src_gt != got_gt:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetGCPCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetGCPProjection() != '':
        print(ds.GetGCPProjection())
        gdaltest.post_reason('fail')
        return 'fail'

    if len(ds.GetGCPs()) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.SetProjection('')
    got_prj = ds.GetProjectionRef()
    if src_prj == got_prj:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.SetProjection(src_prj)
    got_prj = ds.GetProjectionRef()
    if src_prj != got_prj:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.GetRasterBand(1).Fill(0)
    got_cs = ds.GetRasterBand(1).Checksum()
    if 0 != got_cs:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.GetRasterBand(1).WriteRaster(0, 0, 20, 20, src_data)
    got_cs = ds.GetRasterBand(1).Checksum()
    if src_cs != got_cs:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.GetRasterBand(1).Fill(0)
    got_cs = ds.GetRasterBand(1).Checksum()
    if 0 != got_cs:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.WriteRaster(0, 0, 20, 20, src_data)
    got_cs = ds.GetRasterBand(1).Checksum()
    if src_cs != got_cs:
        gdaltest.post_reason('fail')
        return 'fail'

    got_data = ds.ReadRaster(0, 0, 20, 20)
    if src_data != got_data:
        gdaltest.post_reason('fail')
        return 'fail'

    got_data = ds.GetRasterBand(1).ReadRaster(0, 0, 20, 20)
    if src_data != got_data:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.FlushCache()
    ds.GetRasterBand(1).FlushCache()

    if len(ds.GetFileList()) != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.AddBand(gdal.GDT_Byte) == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    got_md = ds.GetMetadata()
    if src_md != got_md:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetMetadataItem('AREA_OR_POINT') != 'Area':
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetMetadataItem('foo') is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.SetMetadataItem('foo', 'bar')
    if ds.GetMetadataItem('foo') != 'bar':
        gdaltest.post_reason('fail')
        return 'fail'

    ds.GetRasterBand(1).SetMetadata({'foo' : 'baw'}, 'OTHER')
    if ds.GetRasterBand(1).GetMetadataItem('foo', 'OTHER') != 'baw':
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetMetadataItem('INTERLEAVE', 'IMAGE_STRUCTURE') != 'BAND':
        gdaltest.post_reason('fail')
        return 'fail'

    if len(ds.GetRasterBand(1).GetMetadata()) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetRasterBand(1).GetMetadataItem('foo') is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.GetRasterBand(1).SetMetadataItem('foo', 'baz')
    if ds.GetRasterBand(1).GetMetadataItem('foo') != 'baz':
        gdaltest.post_reason('fail')
        return 'fail'

    ds.GetRasterBand(1).SetMetadata({'foo' : 'baw'})
    if ds.GetRasterBand(1).GetMetadataItem('foo') != 'baw':
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_GrayIndex:
        gdaltest.post_reason('fail')
        return 'fail'

    ct = ds.GetRasterBand(1).GetColorTable()
    if ct is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ct = gdal.ColorTable()
    ct.SetColorEntry( 0, (1, 2, 3) )
    if ds.GetRasterBand(1).SetColorTable(ct) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ct = ds.GetRasterBand(1).GetColorTable()
    if ct is  None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ct.GetColorEntry(0) != (1, 2, 3, 255):
        gdaltest.post_reason('fail')
        return 'fail'

    rat = ds.GetRasterBand(1).GetDefaultRAT()
    if rat is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetRasterBand(1).GetMinimum() is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    got_stats = ds.GetRasterBand(1).GetStatistics(0,0)
    if got_stats[3] >= 0.0:
        gdaltest.post_reason('fail')
        return 'fail'

    got_stats = ds.GetRasterBand(1).GetStatistics(1,1)
    if got_stats[0] != 74.0:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetRasterBand(1).GetMinimum() != 74.0:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetRasterBand(1).GetMaximum() != 255.0:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.GetRasterBand(1).SetStatistics(1,2,3,4)
    got_stats = ds.GetRasterBand(1).GetStatistics(1,1)
    if got_stats != [1,2,3,4]:
        print(got_stats)
        gdaltest.post_reason('fail')
        return 'fail'

    ds.GetRasterBand(1).ComputeStatistics(0)
    got_stats = ds.GetRasterBand(1).GetStatistics(1,1)
    if got_stats[0] != 74.0:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetRasterBand(1).GetOffset() != 0.0:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetRasterBand(1).GetScale() != 1.0:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.GetRasterBand(1).SetOffset(10.0)
    if ds.GetRasterBand(1).GetOffset() != 10.0:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.GetRasterBand(1).SetScale(2.0)
    if ds.GetRasterBand(1).GetScale() != 2.0:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.BuildOverviews('NEAR', [2])
    if ds.GetRasterBand(1).GetOverviewCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetRasterBand(1).GetOverview(-1) is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetRasterBand(1).GetOverview(0) is None:
        gdaltest.post_reason('fail')
        return 'fail'

    got_hist = ds.GetRasterBand(1).GetHistogram()
    if len(got_hist) != 256:
        gdaltest.post_reason('fail')
        return 'fail'

    (minval, maxval, nitems, got_hist2) = ds.GetRasterBand(1).GetDefaultHistogram()
    if minval != -0.5:
        gdaltest.post_reason('fail')
        return 'fail'
    if maxval != 255.5:
        gdaltest.post_reason('fail')
        return 'fail'
    if nitems != 256:
        gdaltest.post_reason('fail')
        return 'fail'
    if got_hist != got_hist2:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.GetRasterBand(1).SetDefaultHistogram(1,2,[3])
    (minval, maxval, nitems, got_hist3) = ds.GetRasterBand(1).GetDefaultHistogram()
    if minval != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if maxval != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    if nitems != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if got_hist3[0] != 3:
        gdaltest.post_reason('fail')
        return 'fail'

    got_nodatavalue = ds.GetRasterBand(1).GetNoDataValue()
    if got_nodatavalue is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.GetRasterBand(1).SetNoDataValue(123)
    got_nodatavalue = ds.GetRasterBand(1).GetNoDataValue()
    if got_nodatavalue != 123:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetRasterBand(1).GetMaskFlags() != 8:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetRasterBand(1).GetMaskBand() is None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.CreateMaskBand(0)

    if ds.GetRasterBand(1).GetMaskFlags() != 2:
        print(ds.GetRasterBand(1).GetMaskFlags())
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetRasterBand(1).GetMaskBand() is None:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetRasterBand(1).HasArbitraryOverviews() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.GetRasterBand(1).SetUnitType('foo')
    if ds.GetRasterBand(1).GetUnitType() != 'foo':
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetRasterBand(1).GetCategoryNames() is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.GetRasterBand(1).SetCategoryNames(['foo'])
    if ds.GetRasterBand(1).GetCategoryNames() != ['foo']:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.GetRasterBand(1).SetDescription('bar')

    ds = None

    gdal.Unlink('tmp/byte.tif')

    return 'success'

gdaltest_list = [ gdal_api_proxy_start ]

if __name__ == '__main__':

    if len(sys.argv) >= 2 and sys.argv[1] == '-api_proxy':
        gdaltest_list = [ gdal_api_proxy_1 ]

    gdaltest.setup_run( 'gdal_api_proxy' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

