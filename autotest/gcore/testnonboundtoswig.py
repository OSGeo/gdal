#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GDAL functions not bound SWIG with ctypes
# Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2011 Even Rouault
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

import gdal
import sys
import os
from sys import version_info

sys.path.append( '../pymod' )

try:
    import ctypes
except:
    pass

import gdaltest

gdal_handle = None
gdal_handle_stdcall = None

###############################################################################
# Init

def testnonboundtoswig_init():

    global gdal_handle, gdal_handle_stdcall

    try:
        ctypes.cdll
    except:
        print('cannot find ctypes')
        return 'skip'

    static_version = gdal.VersionInfo(None)
    short_static_version = static_version[0:2]

    for name in ["libgdal.so", "libgdal.dylib", "gdal%s.dll" % short_static_version, "gdal%sdev.dll" % short_static_version]:
        try:
            gdal_handle = ctypes.cdll.LoadLibrary(name)
            try:
                gdal_handle_stdcall = ctypes.windll.LoadLibrary(name)
            except:
                gdal_handle_stdcall = gdal_handle

            gdal_handle_stdcall.GDALVersionInfo.argtypes = [ ctypes.c_char_p ]
            gdal_handle_stdcall.GDALVersionInfo.restype = ctypes.c_char_p

            dynamic_version = gdal_handle_stdcall.GDALVersionInfo(None)
            if version_info >= (3,0,0):
                dynamic_version = str(dynamic_version, 'utf-8')

            if dynamic_version != static_version:
                gdaltest.post_reason('dynamic version(%s) does not match static version (%s)' % (dynamic_version, static_version))
                gdal_handle = None
                gdal_handle_stdcall = None
                return 'skip'

            return 'success'
        except:
            pass

    print('cannot find gdal shared object')
    return 'skip'

###############################################################################
# Test GDALSimpleImageWarp

def testnonboundtoswig_GDALSimpleImageWarp():

    if gdal_handle is None:
        return 'skip'

    src_ds = gdal.Open('data/byte.tif')
    gt = src_ds.GetGeoTransform()
    wkt = src_ds.GetProjectionRef()
    src_ds = None

    gdal_handle_stdcall.GDALOpen.argtypes = [ ctypes.c_char_p, ctypes.c_int]
    gdal_handle_stdcall.GDALOpen.restype = ctypes.c_void_p

    gdal_handle_stdcall.GDALClose.argtypes = [ ctypes.c_void_p ]
    gdal_handle_stdcall.GDALClose.restype = None

    gdal_handle.GDALCreateGenImgProjTransformer2.restype = ctypes.c_void_p
    gdal_handle.GDALCreateGenImgProjTransformer2.argtypes = [ ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]

    gdal_handle_stdcall.GDALSimpleImageWarp.argtypes = [ ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
    gdal_handle_stdcall.GDALSimpleImageWarp.restype = ctypes.c_int

    gdal_handle.GDALDestroyGenImgProjTransformer.argtypes = [ ctypes.c_void_p ]
    gdal_handle.GDALDestroyGenImgProjTransformer.restype = None

    out_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/out.tif', 20, 20, 1)
    out_ds.SetGeoTransform(gt)
    out_ds.SetProjection(wkt)
    out_ds = None

    filename = 'data/byte.tif'
    if version_info >= (3,0,0):
        filename = bytes(filename, 'utf-8')

    native_in_ds = gdal_handle_stdcall.GDALOpen(filename, gdal.GA_ReadOnly)
    if native_in_ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    filename = '/vsimem/out.tif'
    if version_info >= (3,0,0):
        filename = bytes(filename, 'utf-8')

    native_out_ds = gdal_handle_stdcall.GDALOpen(filename, gdal.GA_Update)
    if native_out_ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    pTransformerArg = gdal_handle.GDALCreateGenImgProjTransformer2( native_in_ds, native_out_ds, None )
    if pTransformerArg is None:
        gdaltest.post_reason('fail')
        return 'fail'

    ret = gdal_handle_stdcall.GDALSimpleImageWarp(native_in_ds, native_out_ds, 0, None, gdal_handle_stdcall.GDALGenImgProjTransform, pTransformerArg, None, None, None)
    if ret != 1:
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'

    gdal_handle.GDALDestroyGenImgProjTransformer(pTransformerArg)

    gdal_handle_stdcall.GDALClose(native_in_ds)
    gdal_handle_stdcall.GDALClose(native_out_ds)

    ds = gdal.Open('/vsimem/out.tif')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    gdal.Unlink('/vsimem/out.tif')

    if cs != 4672:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    return 'success'


gdaltest_list = [ testnonboundtoswig_init,
                  testnonboundtoswig_GDALSimpleImageWarp ]

if __name__ == '__main__':

    gdaltest.setup_run( 'testnonboundtoswig' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

