#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GDAL functions not bound SWIG with ctypes
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011-2012, Even Rouault <even dot rouault at spatialys.com>
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
import pytest


try:
    import ctypes
except ImportError:
    pass

import gdaltest

gdal_handle_init = False
gdal_handle = None
gdal_handle_stdcall = None

###############################################################################
# find_libgdal()


def find_libgdal():
    return gdaltest.find_lib('gdal')

###############################################################################
# Init


@pytest.fixture(scope='module', autouse=True)
def setup():
    global gdal_handle_init, gdal_handle, gdal_handle_stdcall

    if gdal_handle_init:
        if gdal_handle is None:
            pytest.skip()
        return gdal_handle

    gdal_handle_init = True

    try:
        ctypes.cdll
    except ImportError:
        pytest.skip('cannot find ctypes')

    name = find_libgdal()
    if name is None:
        pytest.skip()

    print('Found libgdal we are running against : %s' % name)

    static_version = gdal.VersionInfo(None)
    # short_static_version = static_version[0:2]

    try:
        gdal_handle = ctypes.cdll.LoadLibrary(name)
        try:
            gdal_handle_stdcall = ctypes.windll.LoadLibrary(name)
        except:
            gdal_handle_stdcall = gdal_handle

        gdal_handle_stdcall.GDALVersionInfo.argtypes = [ctypes.c_char_p]
        gdal_handle_stdcall.GDALVersionInfo.restype = ctypes.c_char_p

        dynamic_version = gdal_handle_stdcall.GDALVersionInfo(None)
        dynamic_version = dynamic_version.decode('utf-8')

        if dynamic_version != static_version:
            gdal_handle = None
            gdal_handle_stdcall = None
            pytest.skip('dynamic version(%s) does not match static version (%s)' % (dynamic_version, static_version))

        return gdal_handle
    except Exception:
        print('cannot find gdal shared object')
        pytest.skip()

###############################################################################
# Call GDALDestroyDriverManager()


def GDALDestroyDriverManager():
    if gdal_handle_stdcall:
        gdal_handle_stdcall.GDALDestroyDriverManager.argtypes = []
        gdal_handle_stdcall.GDALDestroyDriverManager.restype = None

        gdal_handle_stdcall.GDALDestroyDriverManager()

###############################################################################
# Call OGRCleanupAll()


def OGRCleanupAll():
    if gdal_handle_stdcall:
        gdal_handle_stdcall.OGRCleanupAll.argtypes = []
        gdal_handle_stdcall.OGRCleanupAll.restype = None

        gdal_handle_stdcall.OGRCleanupAll()

###############################################################################
# Call OSRCleanup()


def OSRCleanup():
    if gdal_handle:
        gdal_handle.OSRCleanup.argtypes = []
        gdal_handle.OSRCleanup.restype = None

        gdal_handle.OSRCleanup()

###############################################################################
# Test GDALSimpleImageWarp


def test_testnonboundtoswig_GDALSimpleImageWarp():

    src_ds = gdal.Open('data/byte.tif')
    gt = src_ds.GetGeoTransform()
    wkt = src_ds.GetProjectionRef()
    src_ds = None

    gdal_handle_stdcall.GDALOpen.argtypes = [ctypes.c_char_p, ctypes.c_int]
    gdal_handle_stdcall.GDALOpen.restype = ctypes.c_void_p

    gdal_handle_stdcall.GDALClose.argtypes = [ctypes.c_void_p]
    gdal_handle_stdcall.GDALClose.restype = None

    gdal_handle.GDALCreateGenImgProjTransformer2.restype = ctypes.c_void_p
    gdal_handle.GDALCreateGenImgProjTransformer2.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]

    gdal_handle_stdcall.GDALSimpleImageWarp.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
    gdal_handle_stdcall.GDALSimpleImageWarp.restype = ctypes.c_int

    gdal_handle.GDALDestroyGenImgProjTransformer.argtypes = [ctypes.c_void_p]
    gdal_handle.GDALDestroyGenImgProjTransformer.restype = None

    out_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/out.tif', 20, 20, 1)
    out_ds.SetGeoTransform(gt)
    out_ds.SetProjection(wkt)
    out_ds = None

    filename = b'data/byte.tif'
    native_in_ds = gdal_handle_stdcall.GDALOpen(filename, gdal.GA_ReadOnly)
    assert native_in_ds is not None

    filename = b'/vsimem/out.tif'
    native_out_ds = gdal_handle_stdcall.GDALOpen(filename, gdal.GA_Update)
    assert native_out_ds is not None

    pTransformerArg = gdal_handle.GDALCreateGenImgProjTransformer2(native_in_ds, native_out_ds, None)
    assert pTransformerArg is not None

    ret = gdal_handle_stdcall.GDALSimpleImageWarp(native_in_ds, native_out_ds, 0, None, gdal_handle_stdcall.GDALGenImgProjTransform, pTransformerArg, None, None, None)
    assert ret == 1

    gdal_handle.GDALDestroyGenImgProjTransformer(pTransformerArg)

    gdal_handle_stdcall.GDALClose(native_in_ds)
    gdal_handle_stdcall.GDALClose(native_out_ds)

    ds = gdal.Open('/vsimem/out.tif')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    gdal.Unlink('/vsimem/out.tif')

    assert cs == 4672

###############################################################################
# Test VRT derived bands with callback functions implemented in Python!


def GDALTypeToCTypes(gdaltype):

    if gdaltype == gdal.GDT_Byte:
        return ctypes.c_ubyte
    if gdaltype == gdal.GDT_Int16:
        return ctypes.c_short
    if gdaltype == gdal.GDT_UInt16:
        return ctypes.c_ushort
    if gdaltype == gdal.GDT_Int32:
        return ctypes.c_int
    if gdaltype == gdal.GDT_UInt32:
        return ctypes.c_uint
    if gdaltype == gdal.GDT_Float32:
        return ctypes.c_float
    if gdaltype == gdal.GDT_Float64:
        return ctypes.c_double
    return None


def my_pyDerivedPixelFunc(papoSources, nSources, pData, nBufXSize, nBufYSize, eSrcType, eBufType, nPixelSpace, nLineSpace):
    if nSources != 1:
        print(nSources)
        print('did not get expected nSources')
        return 1

    srcctype = GDALTypeToCTypes(eSrcType)
    if srcctype is None:
        print(eSrcType)
        print('did not get expected eSrcType')
        return 1

    dstctype = GDALTypeToCTypes(eBufType)
    if dstctype is None:
        print(eBufType)
        print('did not get expected eBufType')
        return 1

    if nPixelSpace != gdal.GetDataTypeSize(eBufType) / 8:
        print(nPixelSpace)
        print('did not get expected nPixelSpace')
        return 1

    if (nLineSpace % nPixelSpace) != 0:
        print(nLineSpace)
        print('did not get expected nLineSpace')
        return 1

    nLineStride = (int)(nLineSpace / nPixelSpace)

    srcValues = ctypes.cast(papoSources[0], ctypes.POINTER(srcctype))
    dstValues = ctypes.cast(pData, ctypes.POINTER(dstctype))
    for j in range(nBufYSize):
        for i in range(nBufXSize):
            dstValues[j * nLineStride + i] = srcValues[j * nBufXSize + i]

    return 0


def test_testnonboundtoswig_VRTDerivedBands():

    DerivedPixelFuncType = ctypes.CFUNCTYPE(ctypes.c_int,  # ret CPLErr
                                            ctypes.POINTER(ctypes.c_void_p),  # void **papoSources
                                            ctypes.c_int,  # int nSources
                                            ctypes.c_void_p,  # void *pData
                                            ctypes.c_int,  # int nBufXSize
                                            ctypes.c_int,  # int nBufYSize
                                            ctypes.c_int,  # GDALDataType eSrcType
                                            ctypes.c_int,  # GDALDataType eBufType
                                            ctypes.c_int,  # int nPixelSpace
                                            ctypes.c_int)  # int nLineSpace

    my_cDerivedPixelFunc = DerivedPixelFuncType(my_pyDerivedPixelFunc)

    # CPLErr CPL_DLL CPL_STDCALL GDALAddDerivedBandPixelFunc( const char *pszName,
    #                                GDALDerivedPixelFunc pfnPixelFunc );

    gdal_handle_stdcall.GDALAddDerivedBandPixelFunc.argtypes = [ctypes.c_char_p, DerivedPixelFuncType]
    gdal_handle_stdcall.GDALAddDerivedBandPixelFunc.restype = ctypes.c_int

    funcName = b"pyDerivedPixelFunc"
    ret = gdal_handle_stdcall.GDALAddDerivedBandPixelFunc(funcName, my_cDerivedPixelFunc)
    assert ret == 0

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
    ref_data = src_ds.GetRasterBand(1).ReadRaster(0, 0, 20, 20)
    src_ds = None

    ds = gdal.Open(vrt_xml)
    got_cs = ds.GetRasterBand(1).Checksum()
    got_data = ds.GetRasterBand(1).ReadRaster(0, 0, 20, 20)
    ds = None

    assert ref_cs == got_cs, 'wrong checksum'

    assert ref_data == got_data
