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

from osgeo import gdal
import sys
import os
from sys import version_info

sys.path.append( '../pymod' )

try:
    import ctypes
except:
    pass

import gdaltest

gdal_handle_init = False
gdal_handle = None
gdal_handle_stdcall = None

###############################################################################
# Init

def testnonboundtoswig_init():

    global gdal_handle_init, gdal_handle, gdal_handle_stdcall

    if gdal_handle_init:
        if gdal_handle is None:
            return 'skip'
        return 'success'

    gdal_handle_init = True

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
# Call GDALDestroyDriverManager()

def GDALDestroyDriverManager():

    if gdal_handle is None:
        testnonboundtoswig_init()

    if gdal_handle is None:
        return 'skip'

    gdal_handle_stdcall.GDALDestroyDriverManager.argtypes = [ ]
    gdal_handle_stdcall.GDALDestroyDriverManager.restype = None

    gdal_handle_stdcall.GDALDestroyDriverManager()

    return 'success'

###############################################################################
# Call OGRCleanupAll()

def OGRCleanupAll():

    if gdal_handle is None:
        testnonboundtoswig_init()

    if gdal_handle is None:
        return 'skip'

    gdal_handle_stdcall.OGRCleanupAll.argtypes = [ ]
    gdal_handle_stdcall.OGRCleanupAll.restype = None

    gdal_handle_stdcall.OGRCleanupAll()

    return 'success'

###############################################################################
# Call OSRCleanup()

def OSRCleanup():

    if gdal_handle is None:
        testnonboundtoswig_init()

    if gdal_handle is None:
        return 'skip'

    gdal_handle.OSRCleanup.argtypes = [ ]
    gdal_handle.OSRCleanup.restype = None

    gdal_handle.OSRCleanup()

    return 'success'

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

###############################################################################
# Test VRT derived bands with callback functions implemented in Python!

def GDALTypeToCTypes(gdaltype):

    if gdaltype == gdal.GDT_Byte:
        return ctypes.c_ubyte
    elif gdaltype == gdal.GDT_Int16:
        return ctypes.c_short
    elif gdaltype == gdal.GDT_UInt16:
        return ctypes.c_ushort
    elif gdaltype == gdal.GDT_Int32:
        return ctypes.c_int
    elif gdaltype == gdal.GDT_UInt32:
        return ctypes.c_uint
    elif gdaltype == gdal.GDT_Float32:
        return ctypes.c_float
    elif gdaltype == gdal.GDT_Float64:
        return ctypes.c_double
    else:
        return None

def my_pyDerivedPixelFunc(papoSources, nSources, pData, nBufXSize, nBufYSize, eSrcType, eBufType, nPixelSpace, nLineSpace):
    if nSources != 1:
        print(nSources)
        gdaltest.post_reason('did not get expected nSources')
        return 1

    srcctype = GDALTypeToCTypes(eSrcType)
    if srcctype is None:
        print(eSrcType)
        gdaltest.post_reason('did not get expected eSrcType')
        return 1

    dstctype = GDALTypeToCTypes(eBufType)
    if dstctype is None:
        print(eBufType)
        gdaltest.post_reason('did not get expected eBufType')
        return 1

    if nPixelSpace != gdal.GetDataTypeSize(eBufType) / 8:
        print(nPixelSpace)
        gdaltest.post_reason('did not get expected nPixelSpace')
        return 1

    if (nLineSpace % nPixelSpace) != 0:
        print(nLineSpace)
        gdaltest.post_reason('did not get expected nLineSpace')
        return 1

    nLineStride = (int)(nLineSpace/nPixelSpace)

    srcValues = ctypes.cast(papoSources[0], ctypes.POINTER(srcctype))
    dstValues = ctypes.cast(pData, ctypes.POINTER(dstctype))
    for j in range(nBufYSize):
        for i in range(nBufXSize):
            dstValues[j * nLineStride + i] = srcValues[j * nBufXSize + i]

    return 0

def testnonboundtoswig_VRTDerivedBands():

    if gdal_handle is None:
        return 'skip'

    DerivedPixelFuncType = ctypes.CFUNCTYPE(ctypes.c_int, # ret CPLErr
                                            ctypes.POINTER(ctypes.c_void_p), # void **papoSources
                                            ctypes.c_int, # int nSources
                                            ctypes.c_void_p, #void *pData
                                            ctypes.c_int, #int nBufXSize
                                            ctypes.c_int, #int nBufYSize
                                            ctypes.c_int, # GDALDataType eSrcType
                                            ctypes.c_int, # GDALDataType eBufType
                                            ctypes.c_int, #int nPixelSpace
                                            ctypes.c_int ) #int nLineSpace

    my_cDerivedPixelFunc = DerivedPixelFuncType(my_pyDerivedPixelFunc)

    #CPLErr CPL_DLL CPL_STDCALL GDALAddDerivedBandPixelFunc( const char *pszName,
    #                                GDALDerivedPixelFunc pfnPixelFunc );

    gdal_handle_stdcall.GDALAddDerivedBandPixelFunc.argtypes = [ ctypes.c_char_p, DerivedPixelFuncType]
    gdal_handle_stdcall.GDALAddDerivedBandPixelFunc.restype = ctypes.c_int

    funcName = "pyDerivedPixelFunc"
    if version_info >= (3,0,0):
        funcName = bytes(funcName, 'utf-8')
    ret = gdal_handle_stdcall.GDALAddDerivedBandPixelFunc(funcName, my_cDerivedPixelFunc)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    vrt_xml = """<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
    <PixelFunctionType>pyDerivedPixelFunc</PixelFunctionType>
    <SourceTransferType>Byte</SourceTransferType>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>"""

    src_ds = gdal.Open('data/byte.tif')
    ref_cs = src_ds.GetRasterBand(1).Checksum()
    ref_data = src_ds.GetRasterBand(1).ReadRaster(0,0,20,20)
    src_ds = None

    ds = gdal.Open(vrt_xml)
    got_cs = ds.GetRasterBand(1).Checksum()
    got_data = ds.GetRasterBand(1).ReadRaster(0,0,20,20)
    ds = None

    if ref_cs != got_cs:
        gdaltest.post_reason('wrong checksum')
        print(got_cs)
        return 'fail'

    if ref_data != got_data:
        gdaltest.post_reason('wrong data')
        print(ref_data)
        print(got_data)
        return 'fail'

    return 'success'

# Empty because it is not completely reliable, and we
# can get a ctype handle to a GDAL library which is not
# the one fetched by the GDAL Python module.
gdaltest_list = []

manual_gdaltest_list = [ testnonboundtoswig_init,
                  testnonboundtoswig_GDALSimpleImageWarp,
                  testnonboundtoswig_VRTDerivedBands ]

if __name__ == '__main__':

    gdaltest.setup_run( 'testnonboundtoswig' )

    gdaltest.run_tests( manual_gdaltest_list )

    gdaltest.summarize()

