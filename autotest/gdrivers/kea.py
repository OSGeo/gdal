#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test KEA driver
# Author:   Even Rouault, <even dot rouault at spatialys dot com>
# 
###############################################################################
# Copyright (c) 2014, Even Rouault <even dot rouault at spatialys dot com>
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
def kea_init():
    try:
        gdaltest.kea_driver = gdal.GetDriverByName('KEA')
    except:
        gdaltest.kea_driver = None

    return 'success'

###############################################################################
# Test copying a reference sample with CreateCopy()

def kea_1():
    if gdaltest.kea_driver is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'KEA', 'byte.tif', 1, 4672, options = ['IMAGEBLOCKSIZE=15', 'THEMATIC=YES'] )
    return tst.testCreateCopy( check_srs = True, check_gt = 1 )

###############################################################################
# Test CreateCopy() for various data types

def kea_2():
    if gdaltest.kea_driver is None:
        return 'skip'

    src_files = [ 'byte.tif',
                  'int16.tif',
                  '../../gcore/data/uint16.tif',
                  '../../gcore/data/int32.tif',
                  '../../gcore/data/uint32.tif',
                  '../../gcore/data/float32.tif',
                  '../../gcore/data/float64.tif' ]

    for src_file in src_files:
        tst = gdaltest.GDALTest( 'KEA', src_file, 1, 4672 )
        ret = tst.testCreateCopy( check_minmax = 1 )
        if ret != 'success':
            return ret
            
    return 'success'

###############################################################################
# Test Create() for various data types

def kea_3():
    if gdaltest.kea_driver is None:
        return 'skip'

    src_files = [ 'byte.tif',
                  'int16.tif',
                  '../../gcore/data/uint16.tif',
                  '../../gcore/data/int32.tif',
                  '../../gcore/data/uint32.tif',
                  '../../gcore/data/float32.tif',
                  '../../gcore/data/float64.tif' ]

    for src_file in src_files:
        tst = gdaltest.GDALTest( 'KEA', src_file, 1, 4672 )
        ret = tst.testCreate( out_bands = 1, check_minmax = 1 )
        if ret != 'success':
            return ret
            
    return 'success'

###############################################################################
# Test Create()/CreateCopy() error cases or limit cases

def kea_4():
    if gdaltest.kea_driver is None:
        return 'skip'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdaltest.kea_driver.Create("/non_existing_path", 1, 1)
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    src_ds = gdaltest.kea_driver.Create('tmp/src.kea', 1, 1, 0)
    if src_ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = gdaltest.kea_driver.CreateCopy("tmp/out.kea", src_ds)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.RasterCount != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    src_ds = None
    ds = None
    
    # Test updating a read-only file
    ds = gdaltest.kea_driver.Create('tmp/out.kea', 1, 1)
    ds.GetRasterBand(1).Fill(255)
    ds = None
    ds = gdal.Open('tmp/out.kea')

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = ds.SetProjection('a')
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = ds.SetGeoTransform([1,2,3,4,5,6])
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Disabled for now since some of them cause memory leaks or
    # crash in the HDF5 library finalizer
    if False:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ret = ds.SetMetadataItem('foo', 'bar')
        gdal.PopErrorHandler()
        if ret == 0:
            gdaltest.post_reason('fail')
            return 'fail'

        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ret = ds.SetMetadata({'foo': 'bar'})
        gdal.PopErrorHandler()
        if ret == 0:
            gdaltest.post_reason('fail')
            return 'fail'

        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ret = ds.GetRasterBand(1).SetMetadataItem('foo', 'bar')
        gdal.PopErrorHandler()
        if ret == 0:
            gdaltest.post_reason('fail')
            return 'fail'

        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ret = ds.GetRasterBand(1).SetMetadata({'foo': 'bar'})
        gdal.PopErrorHandler()
        if ret == 0:
            gdaltest.post_reason('fail')
            return 'fail'

        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ret = ds.SetGCPs([], "")
        gdal.PopErrorHandler()
        if ret == 0:
            gdaltest.post_reason('fail')
            return 'fail'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = ds.AddBand(gdal.GDT_Byte)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.GetRasterBand(1).WriteRaster(0,0,1,1,'\0')
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds.FlushCache()
    gdal.PopErrorHandler()
    if ds.GetRasterBand(1).Checksum() != 3:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = None
    
    gdaltest.kea_driver.Delete('tmp/src.kea')
    gdaltest.kea_driver.Delete('tmp/out.kea')

    return 'success'

###############################################################################
# Test Create() creation options

def kea_5():
    if gdaltest.kea_driver is None:
        return 'skip'

    options = [ 'IMAGEBLOCKSIZE=15', 'ATTBLOCKSIZE=100', 'MDC_NELMTS=10',
                'RDCC_NELMTS=256', 'RDCC_NBYTES=500000', 'RDCC_W0=0.5',
                'SIEVE_BUF=32768', 'META_BLOCKSIZE=1024', 'DEFLATE=9', 'THEMATIC=YES' ]
    ds = gdaltest.kea_driver.Create("tmp/out.kea", 100, 100, 3, options = options)
    ds = None
    ds = gdal.Open('tmp/out.kea')
    if ds.GetRasterBand(1).GetBlockSize() != [15,15]:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetBlockSize())
        return 'failure'
    if ds.GetRasterBand(1).GetMetadataItem('LAYER_TYPE') != 'thematic':
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetMetadata())
        return 'failure'
    if ds.GetRasterBand(1).Checksum() != 0:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).Checksum())
        return 'failure'
    if ds.GetGeoTransform() != (0,1,0,0,0,-1):
        gdaltest.post_reason('fail')
        print(ds.GetGeoTransform())
        return 'failure'
    if ds.GetProjectionRef() != '':
        gdaltest.post_reason('fail')
        print(ds.GetProjectionRef())
        return 'failure'
    ds = None
    gdaltest.kea_driver.Delete('tmp/out.kea')

    return 'success'

###############################################################################
# Test metadata

def kea_6():
    if gdaltest.kea_driver is None:
        return 'skip'

    ds = gdaltest.kea_driver.Create("tmp/out.kea", 1, 1, 5)
    ds.SetMetadata( { 'foo':'bar' } )
    ds.SetMetadataItem( 'bar', 'baw' )
    ds.GetRasterBand(1).SetMetadata( { 'bar':'baz' } )
    ds.GetRasterBand(1).SetDescription('desc')
    ds.GetRasterBand(2).SetMetadata( { 'LAYER_TYPE' : 'any_string_that_is_not_athematic_is_thematic' } )
    ds.GetRasterBand(3).SetMetadata( { 'LAYER_TYPE' : 'athematic' } )
    ds.GetRasterBand(4).SetMetadataItem( 'LAYER_TYPE', 'thematic' )
    ds.GetRasterBand(5).SetMetadataItem( 'LAYER_TYPE', 'athematic' )
    if ds.SetMetadata( { 'foo':'bar' }, 'other_domain' ) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.SetMetadataItem( 'foo', 'bar', 'other_domain' ) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).SetMetadata( { 'foo':'bar' }, 'other_domain' ) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).SetMetadataItem( 'foo', 'bar', 'other_domain' ) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    
    ds = gdal.Open('tmp/out.kea')
    if ds.GetMetadata('other_domain') != {}:
        gdaltest.post_reason('fail')
        print(ds.GetMetadata('other_domain'))
        return 'fail'
    if ds.GetMetadataItem('item', 'other_domain') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetMetadata('other_domain') != {}:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetMetadataItem('item', 'other_domain') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    md = ds.GetMetadata()
    if md['foo'] != 'bar':
        gdaltest.post_reason('fail')
        print(md)
        return 'failure'
    if ds.GetMetadataItem('foo') != 'bar':
        gdaltest.post_reason('fail')
        print(ds.GetMetadataItem('foo'))
        return 'failure'
    if ds.GetMetadataItem('bar') != 'baw':
        gdaltest.post_reason('fail')
        print(ds.GetMetadataItem('bar'))
        return 'failure'
    if ds.GetRasterBand(1).GetDescription() != 'desc':
        gdaltest.post_reason('fail')
        return 'failure'
    md = ds.GetRasterBand(1).GetMetadata()
    if md['bar'] != 'baz':
        gdaltest.post_reason('fail')
        print(md)
        return 'failure'
    if ds.GetRasterBand(1).GetMetadataItem('bar') != 'baz':
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetMetadataItem('bar'))
        return 'failure'
    if ds.GetRasterBand(2).GetMetadataItem('LAYER_TYPE') != 'thematic':
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(2).GetMetadataItem('LAYER_TYPE'))
        return 'failure'
    if ds.GetRasterBand(3).GetMetadataItem('LAYER_TYPE') != 'athematic':
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(3).GetMetadataItem('LAYER_TYPE'))
        return 'failure'
    if ds.GetRasterBand(4).GetMetadataItem('LAYER_TYPE') != 'thematic':
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(4).GetMetadataItem('LAYER_TYPE'))
        return 'failure'
    if ds.GetRasterBand(5).GetMetadataItem('LAYER_TYPE') != 'athematic':
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(5).GetMetadataItem('LAYER_TYPE'))
        return 'failure'
    out2_ds = gdaltest.kea_driver.CreateCopy('tmp/out2.kea', ds)
    ds = None

    if out2_ds.GetMetadataItem('foo') != 'bar':
        gdaltest.post_reason('fail')
        print(out2_ds.GetMetadataItem('foo'))
        return 'failure'
    if out2_ds.GetRasterBand(1).GetMetadataItem('bar') != 'baz':
        gdaltest.post_reason('fail')
        print(out2_ds.GetRasterBand(1).GetMetadataItem('bar'))
        return 'failure'

    out2_ds = None

    gdaltest.kea_driver.Delete('tmp/out.kea')
    gdaltest.kea_driver.Delete('tmp/out2.kea')

    return 'success'

###############################################################################
# Test georef

def kea_7():
    if gdaltest.kea_driver is None:
        return 'skip'

    # Geotransform
    ds = gdaltest.kea_driver.Create("tmp/out.kea", 1, 1)
    if ds.GetGCPCount() != 0:
        gdaltest.post_reason('fail')
        return 'failure'
    if ds.SetGeoTransform([1,2,3,4,5,6]) != 0:
        gdaltest.post_reason('fail')
        return 'failure'
    if ds.SetProjection('foo') != 0:
        gdaltest.post_reason('fail')
        return 'failure'
    ds = None
    
    ds = gdal.Open('tmp/out.kea')
    out2_ds = gdaltest.kea_driver.CreateCopy('tmp/out2.kea', ds)
    ds = None
    if out2_ds.GetGCPCount() != 0:
        gdaltest.post_reason('fail')
        return 'failure'
    if out2_ds.GetGeoTransform() != (1,2,3,4,5,6):
        gdaltest.post_reason('fail')
        print(out2_ds.GetGeoTransform())
        return 'failure'
    if out2_ds.GetProjectionRef() != 'foo':
        gdaltest.post_reason('fail')
        print(out2_ds.GetProjectionRef())
        return 'failure'
    out2_ds = None

    gdaltest.kea_driver.Delete('tmp/out.kea')
    gdaltest.kea_driver.Delete('tmp/out2.kea')

    # GCP
    ds = gdaltest.kea_driver.Create("tmp/out.kea", 1, 1)
    gcp1 = gdal.GCP(0,1,2,3,4)
    gcp1.Id = "id"
    gcp1.Info = "info"
    gcp2 = gdal.GCP(0,1,2,3,4)
    gcps = [ gcp1, gcp2 ]
    ds.SetGCPs(gcps, "foo")
    ds = None
    
    ds = gdal.Open('tmp/out.kea')
    out2_ds = gdaltest.kea_driver.CreateCopy('tmp/out2.kea', ds)
    ds = None

    if out2_ds.GetGCPCount() != 2:
        gdaltest.post_reason('fail')
        return 'failure'
    if out2_ds.GetGCPProjection() != 'foo':
        gdaltest.post_reason('fail')
        return 'failure'
    got_gcps = out2_ds.GetGCPs()
    for i in range(2):
        if got_gcps[i].GCPX != gcps[i].GCPX or got_gcps[i].GCPY != gcps[i].GCPY or \
           got_gcps[i].GCPZ != gcps[i].GCPZ or got_gcps[i].GCPPixel != gcps[i].GCPPixel or \
           got_gcps[i].GCPLine != gcps[i].GCPLine or got_gcps[i].Id != gcps[i].Id or \
           got_gcps[i].Info != gcps[i].Info:
            print(i)
            print(got_gcps[i])
            gdaltest.post_reason('fail')
            return 'failure'
    out2_ds = None

    gdaltest.kea_driver.Delete('tmp/out.kea')
    gdaltest.kea_driver.Delete('tmp/out2.kea')

    return 'success'

###############################################################################
# Test colortable

def kea_8():
    if gdaltest.kea_driver is None:
        return 'skip'

    for i in range(2):
        ds = gdaltest.kea_driver.Create("tmp/out.kea", 1, 1)
        if ds.GetRasterBand(1).GetColorTable() is not None:
            gdaltest.post_reason('fail')
            return 'fail'
        if ds.GetRasterBand(1).SetColorTable( None ) == 0: # not allowed by the driver
            gdaltest.post_reason('fail')
            return 'fail'
        ct = gdal.ColorTable()
        ct.SetColorEntry( 0, (0,255,0,255) )
        ct.SetColorEntry( 1, (255,0,255,255) )
        ct.SetColorEntry( 2, (0,0,255,255) )
        if ds.GetRasterBand(1).SetColorTable( ct ) != 0:
            gdaltest.post_reason('fail')
            return 'fail'
        if i == 1:
            # And again
            if ds.GetRasterBand(1).SetColorTable( ct ) != 0:
                gdaltest.post_reason('fail')
                return 'fail'
        ds = None
        
        ds = gdal.Open('tmp/out.kea')
        out2_ds = gdaltest.kea_driver.CreateCopy('tmp/out2.kea', ds)
        ds = None
        got_ct = out2_ds.GetRasterBand(1).GetColorTable()
        if got_ct.GetCount() != 3:
            gdaltest.post_reason( 'Got wrong color table entry count.' )
            return 'fail'
        if got_ct.GetColorEntry(1) != (255,0,255,255):
            gdaltest.post_reason( 'Got wrong color table entry.' )
            return 'fail'

        out2_ds = None

        gdaltest.kea_driver.Delete('tmp/out.kea')
        gdaltest.kea_driver.Delete('tmp/out2.kea')

    return 'success'

###############################################################################
# Test color interpretation

def kea_9():
    if gdaltest.kea_driver is None:
        return 'skip'

    ds = gdaltest.kea_driver.Create("tmp/out.kea", 1, 1, gdal.GCI_YCbCr_CrBand - gdal.GCI_GrayIndex + 1)
    if ds.GetRasterBand(1).GetColorInterpretation() != gdal.GCI_GrayIndex:
        gdaltest.post_reason('fail')
        return 'fail'
    for i in range(gdal.GCI_GrayIndex, gdal.GCI_YCbCr_CrBand + 1):
        ds.GetRasterBand(i).SetColorInterpretation(i)
    ds = None
    
    ds = gdal.Open('tmp/out.kea')
    out2_ds = gdaltest.kea_driver.CreateCopy('tmp/out2.kea', ds)
    ds = None
    for i in range(gdal.GCI_GrayIndex, gdal.GCI_YCbCr_CrBand + 1):
        if out2_ds.GetRasterBand(i).GetColorInterpretation() != i:
            gdaltest.post_reason( 'Got wrong color interpreation.' )
            print(i)
            print(out2_ds.GetRasterBand(i).GetColorInterpretation())
            return 'fail'

    out2_ds = None

    gdaltest.kea_driver.Delete('tmp/out.kea')
    gdaltest.kea_driver.Delete('tmp/out2.kea')

    return 'success'

###############################################################################
# Test nodata

def kea_10():
    if gdaltest.kea_driver is None:
        return 'skip'

    for (dt,nd,expected_nd) in [ (gdal.GDT_Byte,0,0),
                                 (gdal.GDT_Byte,1.1,1.0),
                                 (gdal.GDT_Byte,255,255),
                                 (gdal.GDT_Byte,-1,None),
                                 (gdal.GDT_Byte,256,None),
                                 (gdal.GDT_UInt16,0,0),
                                 (gdal.GDT_UInt16,65535,65535),
                                 (gdal.GDT_UInt16,-1,None),
                                 (gdal.GDT_UInt16,65536,None),
                                 (gdal.GDT_Int16,-32768,-32768),
                                 (gdal.GDT_Int16,32767,32767),
                                 (gdal.GDT_Int16,-32769,None),
                                 (gdal.GDT_Int16,32768,None),
                                 (gdal.GDT_UInt32,0,0),
                                 (gdal.GDT_UInt32,0xFFFFFFFF,0xFFFFFFFF),
                                 (gdal.GDT_UInt32,-1,None),
                                 (gdal.GDT_UInt32,0xFFFFFFFF+1,None),
                                 (gdal.GDT_Int32,-2147483648,-2147483648),
                                 (gdal.GDT_Int32,2147483647,2147483647),
                                 (gdal.GDT_Int32,-2147483649,None),
                                 (gdal.GDT_Int32,2147483648,None),
                                 (gdal.GDT_Float32,0.5,0.5),
                                 ]:
        ds = gdaltest.kea_driver.Create("tmp/out.kea", 1, 1, 1, dt)
        if ds.GetRasterBand(1).GetNoDataValue() is not None:
            gdaltest.post_reason('fail')
            return 'fail'
        ds.GetRasterBand(1).SetNoDataValue(nd)
        if ds.GetRasterBand(1).GetNoDataValue() != expected_nd:
            gdaltest.post_reason( 'Got wrong nodata.' )
            print(dt)
            print(ds.GetRasterBand(1).GetNoDataValue())
            return 'fail'
        ds = None

        ds = gdal.Open('tmp/out.kea')
        out2_ds = gdaltest.kea_driver.CreateCopy('tmp/out2.kea', ds)
        ds = None
        if out2_ds.GetRasterBand(1).GetNoDataValue() != expected_nd:
            gdaltest.post_reason( 'Got wrong nodata.' )
            print(dt)
            print(out2_ds.GetRasterBand(1).GetNoDataValue())
            return 'fail'

        out2_ds = None

        gdaltest.kea_driver.Delete('tmp/out.kea')
        gdaltest.kea_driver.Delete('tmp/out2.kea')

    return 'success'

###############################################################################
# Test AddBand

def kea_11():
    if gdaltest.kea_driver is None:
        return 'skip'
    
    ds = gdaltest.kea_driver.Create("tmp/out.kea", 1, 1, 1, gdal.GDT_Byte)
    ds = None
    
    ds = gdal.Open('tmp/out.kea', gdal.GA_Update)
    if ds.AddBand(gdal.GDT_Byte) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.AddBand(gdal.GDT_Int16, options = ['DEFLATE=9']) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    
    ds = gdal.Open('tmp/out.kea')
    if ds.RasterCount != 3:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(2).DataType != gdal.GDT_Byte:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(3).DataType != gdal.GDT_Int16:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    
    gdaltest.kea_driver.Delete('tmp/out.kea')

    return 'success'

###############################################################################
# Test RAT

def kea_12():
    if gdaltest.kea_driver is None:
        return 'skip'
    
    ds = gdaltest.kea_driver.Create("tmp/out.kea", 1, 1, 1, gdal.GDT_Byte)
    if ds.GetRasterBand(1).GetDefaultRAT().GetColumnCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).SetDefaultRAT( None ) == 0: # not allowed by the driver
        gdaltest.post_reason('fail')
        return 'fail'
    rat = ds.GetRasterBand(1).GetDefaultRAT()
    rat.CreateColumn('col_real_generic', gdal.GFT_Real, gdal.GFU_Generic)
    if ds.GetRasterBand(1).SetDefaultRAT( rat ) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    rat = ds.GetRasterBand(1).GetDefaultRAT()
    rat.CreateColumn('col_integer_pixelcount', gdal.GFT_Real, gdal.GFU_PixelCount)
    rat.CreateColumn('col_string_name', gdal.GFT_String, gdal.GFU_Name)
    rat.CreateColumn('col_integer_red', gdal.GFT_Integer, gdal.GFU_Red)
    rat.CreateColumn('col_integer_green', gdal.GFT_Integer, gdal.GFU_Green)
    rat.CreateColumn('col_integer_blue', gdal.GFT_Integer, gdal.GFU_Blue)
    rat.CreateColumn('col_integer_alpha', gdal.GFT_Integer, gdal.GFU_Alpha)
    rat.SetRowCount(1)

    rat.SetValueAsString(0,0,"1.23")
    rat.SetValueAsInt(0,0,1)
    rat.SetValueAsDouble(0,0,1.23)

    rat.SetValueAsInt(0,2,0)
    rat.SetValueAsDouble(0,2,0)
    rat.SetValueAsString(0,2,'foo')

    rat.SetValueAsString(0,3,"123")
    rat.SetValueAsDouble(0,3,123)
    rat.SetValueAsInt(0,3,123)

    cloned_rat = rat.Clone()
    if ds.GetRasterBand(1).SetDefaultRAT( rat ) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = gdal.Open('tmp/out.kea')
    out2_ds = gdaltest.kea_driver.CreateCopy('tmp/out2.kea', ds)
    rat = out2_ds.GetRasterBand(1).GetDefaultRAT()
    
    for i in range(7):
        if rat.GetColOfUsage(rat.GetUsageOfCol(i)) != i:
            gdaltest.post_reason('fail')
            print(i)
            print(rat.GetColOfUsage(rat.GetUsageOfCol(i)))
            return 'fail'

    if cloned_rat.GetNameOfCol(0) != 'col_real_generic':
        gdaltest.post_reason('fail')
        return 'fail'
    if cloned_rat.GetTypeOfCol(0) != gdal.GFT_Real:
        gdaltest.post_reason('fail')
        return 'fail'
    if cloned_rat.GetUsageOfCol(0) != gdal.GFU_Generic:
        gdaltest.post_reason('fail')
        return 'fail'
    if cloned_rat.GetUsageOfCol(1) != gdal.GFU_PixelCount:
        gdaltest.post_reason('fail')
        return 'fail'
    if cloned_rat.GetTypeOfCol(2) != gdal.GFT_String:
        gdaltest.post_reason('fail')
        return 'fail'
    if cloned_rat.GetTypeOfCol(3) != gdal.GFT_Integer:
        gdaltest.post_reason('fail')
        return 'fail'

    if rat.GetColumnCount() != cloned_rat.GetColumnCount():
        gdaltest.post_reason('fail')
        return 'fail'
    if rat.GetRowCount() != cloned_rat.GetRowCount():
        gdaltest.post_reason('fail')
        return 'fail'
    for i in range(rat.GetColumnCount()):
        if rat.GetNameOfCol(i) != cloned_rat.GetNameOfCol(i):
            gdaltest.post_reason('fail')
            return 'fail'
        if rat.GetTypeOfCol(i) != cloned_rat.GetTypeOfCol(i):
            gdaltest.post_reason('fail')
            return 'fail'
        if rat.GetUsageOfCol(i) != cloned_rat.GetUsageOfCol(i):
            gdaltest.post_reason('fail')
            print(i)
            print(rat.GetUsageOfCol(i))
            print(cloned_rat.GetUsageOfCol(i))
            return 'fail'

    gdal.PushErrorHandler('CPLQuietErrorHandler')

    rat.GetNameOfCol(-1)
    rat.GetTypeOfCol(-1)
    rat.GetUsageOfCol(-1)

    rat.GetNameOfCol(rat.GetColumnCount())
    rat.GetTypeOfCol(rat.GetColumnCount())
    rat.GetUsageOfCol(rat.GetColumnCount())

    rat.GetValueAsDouble( -1, 0 )
    rat.GetValueAsInt( -1, 0 ) 
    rat.GetValueAsString( -1, 0 )
    
    rat.GetValueAsDouble( rat.GetColumnCount(), 0 )
    rat.GetValueAsInt( rat.GetColumnCount(), 0 ) 
    rat.GetValueAsString( rat.GetColumnCount(), 0 ) 

    rat.GetValueAsDouble( 0, -1 )
    rat.GetValueAsInt( 0, -1 ) 
    rat.GetValueAsString( 0, -1 ) 

    rat.GetValueAsDouble( 0, rat.GetRowCount() )
    rat.GetValueAsInt( 0, rat.GetRowCount() ) 
    rat.GetValueAsString( 0, rat.GetRowCount() ) 

    gdal.PopErrorHandler()

    if rat.GetValueAsDouble( 0, 0 ) != 1.23:
        gdaltest.post_reason('fail')
        return 'fail'
    if rat.GetValueAsInt( 0, 0 ) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if rat.GetValueAsString( 0, 0 ) != '1.23':
        gdaltest.post_reason('fail')
        print(rat.GetValueAsString( 0, 0 ))
        return 'fail'

    if rat.GetValueAsInt( 0, 3 ) != 123:
        gdaltest.post_reason('fail')
        return 'fail'
    if rat.GetValueAsDouble( 0, 3 ) != 123:
        gdaltest.post_reason('fail')
        return 'fail'
    if rat.GetValueAsString( 0, 3 ) != '123':
        gdaltest.post_reason('fail')
        return 'fail'

    if rat.GetValueAsString( 0, 2 ) != 'foo':
        gdaltest.post_reason('fail')
        return 'fail'
    if rat.GetValueAsInt( 0, 2 ) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if rat.GetValueAsDouble( 0, 2 ) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = None
    out2_ds = None

    gdaltest.kea_driver.Delete('tmp/out.kea')
    gdaltest.kea_driver.Delete('tmp/out2.kea')

    return 'success'

###############################################################################
# Test overviews

def kea_13():
    if gdaltest.kea_driver is None:
        return 'skip'
    
    src_ds = gdal.Open('data/byte.tif')
    ds = gdaltest.kea_driver.CreateCopy("tmp/out.kea", src_ds)
    src_ds = None
    ds.BuildOverviews('NEAR', [2])
    ds = None
    ds = gdal.Open('tmp/out.kea')
    out2_ds = gdaltest.kea_driver.CreateCopy('tmp/out2.kea', ds) # yes CreateCopy() of KEA copies overviews
    if out2_ds.GetRasterBand(1).GetOverviewCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if out2_ds.GetRasterBand(1).GetOverview(0).Checksum() != 1087:
        gdaltest.post_reason('fail')
        return 'fail'
    if out2_ds.GetRasterBand(1).GetOverview(0).GetDefaultRAT() is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if out2_ds.GetRasterBand(1).GetOverview(0).SetDefaultRAT(None) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if out2_ds.GetRasterBand(1).GetOverview(-1) is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if out2_ds.GetRasterBand(1).GetOverview(1) is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    out2_ds = None
    ds = None

    gdaltest.kea_driver.Delete('tmp/out.kea')
    gdaltest.kea_driver.Delete('tmp/out2.kea')

    return 'success'

###############################################################################
# Test mask bands

def kea_14():
    if gdaltest.kea_driver is None:
        return 'skip'
    
    ds = gdaltest.kea_driver.Create("tmp/out.kea", 1, 1, 1, gdal.GDT_Byte)
    if ds.GetRasterBand(1).GetMaskFlags() != gdal.GMF_ALL_VALID:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetMaskBand().Checksum() != 3:
        print(ds.GetRasterBand(1).GetMaskBand().Checksum())
        gdaltest.post_reason('fail')
        return 'fail'
    ds.GetRasterBand(1).CreateMaskBand(0)
    if ds.GetRasterBand(1).GetMaskFlags() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetMaskBand().Checksum() != 3:
        print(ds.GetRasterBand(1).GetMaskBand().Checksum())
        gdaltest.post_reason('fail')
        return 'fail'
    ds.GetRasterBand(1).GetMaskBand().Fill(0)
    if ds.GetRasterBand(1).GetMaskBand().Checksum() != 0:
        print(ds.GetRasterBand(1).GetMaskBand().Checksum())
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = gdal.Open('tmp/out.kea')
    out2_ds = gdaltest.kea_driver.CreateCopy('tmp/out2.kea', ds) # yes CreateCopy() of KEA copies overviews
    if out2_ds.GetRasterBand(1).GetMaskFlags() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if out2_ds.GetRasterBand(1).GetMaskBand().Checksum() != 0:
        print(out2_ds.GetRasterBand(1).GetMaskBand().Checksum())
        gdaltest.post_reason('fail')
        return 'fail'
    out2_ds = None
    ds = None

    gdaltest.kea_driver.Delete('tmp/out.kea')
    gdaltest.kea_driver.Delete('tmp/out2.kea')

    return 'success'

gdaltest_list = [
    kea_init,
    kea_1,
    kea_2,
    kea_3,
    kea_4,
    kea_5,
    kea_6,
    kea_7,
    kea_8,
    kea_9,
    kea_10,
    kea_11,
    kea_12,
    kea_13,
    kea_14
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'kea' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

