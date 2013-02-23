#!/usr/bin/python
# -*- coding: utf-8 -*-
#/******************************************************************************
# * $Id$
# *
# * Project:  GDAL
# * Purpose:  GDAL API_PROXY server written in Python
# * Author:   Even Rouault, <even dot rouault at mines-paris dot org>
# *
# ******************************************************************************
# * Copyright (c) 2013, Even Rouault, <even dot rouault at mines-paris dot org>
# *
# * Permission is hereby granted, free of charge, to any person obtaining a
# * copy of this software and associated documentation files (the "Software"),
# * to deal in the Software without restriction, including without limitation
# * the rights to use, copy, modify, merge, publish, distribute, sublicense,
# * and/or sell copies of the Software, and to permit persons to whom the
# * Software is furnished to do so, subject to the following conditions:
# *
# * The above copyright notice and this permission notice shall be included
# * in all copies or substantial portions of the Software.
# *
# * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# * DEALINGS IN THE SOFTWARE.
# ****************************************************************************/

# WARNING: only Python 2 compatible for now

import sys
import os
from struct import *

from osgeo import gdalconst, gdal

class GDALPythonServerRasterBand:

    def __init__(self, gdal_band):
        self.gdal_band = gdal_band
        self.XSize = gdal_band.XSize
        self.YSize = gdal_band.YSize
        self.Band = gdal_band.GetBand()
        (self.BlockXSize, self.BlockYSize) = gdal_band.GetBlockSize()
        self.DataType = gdal_band.DataType
        self.mask_band = None
        self.ovr_bands = None

    def FlushCache(self):
        return self.gdal_band.FlushCache()

    def GetColorInterpretation(self):
        return self.gdal_band.GetColorInterpretation()

    def GetNoDataValue(self):
        return self.gdal_band.GetNoDataValue()

    def GetMinimum(self):
        return self.gdal_band.GetMinimum()

    def GetMaximum(self):
        return self.gdal_band.GetMaximum()

    def GetOffset(self):
        return self.gdal_band.GetOffset()

    def GetScale(self):
        return self.gdal_band.GetScale()

    def HasArbitraryOverviews(self):
        return self.gdal_band.HasArbitraryOverviews()

    def GetOverviewCount(self):
        return self.gdal_band.GetOverviewCount()

    def GetMaskFlags(self):
        return self.gdal_band.GetMaskFlags()

    def GetMaskBand(self):
        if self.mask_band is None:
            gdal_mask_band = self.gdal_band.GetMaskBand()
            if gdal_mask_band is not None:
                self.mask_band = GDALPythonServerRasterBand(gdal_mask_band)
        return self.mask_band

    def GetOverview(self, iovr):
        if self.ovr_bands is None:
            self.ovr_bands = [None for i in range(self.GetOverviewCount())]
        if self.ovr_bands[iovr] is None:
            gdal_ovr_band = self.gdal_band.GetOverview(iovr)
            if gdal_ovr_band is not None:
                self.ovr_bands[iovr] = GDALPythonServerRasterBand(gdal_ovr_band)
        return self.ovr_bands[iovr]

    def GetMetadata(self, domain):
        return self.gdal_band.GetMetadata(domain)

    def GetMetadataItem(self, key, domain):
        return self.gdal_band.GetMetadataItem(key, domain)

    def IReadBlock(self, nXBlockOff, nYBlockOff):
        return self.gdal_band.ReadBlock(nXBlockOff, nYBlockOff)

    def IRasterIO_Read(self, nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize, nBufType):
        return self.gdal_band.ReadRaster(nXOff, nYOff, nXSize, nYSize, buf_xsize = nBufXSize, buf_ysize = nBufYSize, buf_type = nBufType)

    def GetUnitType(self):
        return self.gdal_band.GetUnitType()

    def GetStatistics(self, approx_ok, force):
        return self.gdal_band.GetStatistics(approx_ok, force)

    def ComputeRasterMinMax(self, approx_ok):
        return self.gdal_band.ComputeRasterMinMax(approx_ok)

    def GetColorTable(self):
        return self.gdal_band.GetColorTable()

    def GetHistogram(self, dfMin, dfMax, nBuckets, bIncludeOutOfRange, bApproxOK):
        return self.gdal_band.GetHistogram(dfMin, dfMax, nBuckets, include_out_of_range = bIncludeOutOfRange, approx_ok = bApproxOK)

class GDALPythonServerDataset:

    def __init__(self, filename, access = gdal.GA_ReadOnly):
        self.gdal_ds = gdal.Open(filename, access)
        if self.gdal_ds is None:
            raise Exception(gdal.GetLastErrorMsg())
        self.RasterXSize = self.gdal_ds.RasterXSize
        self.RasterYSize = self.gdal_ds.RasterYSize
        self.RasterCount = self.gdal_ds.RasterCount
        self.bands = []
        for i in range(self.RasterCount):
            gdal_band = self.gdal_ds.GetRasterBand(i+1)
            self.bands.append(GDALPythonServerRasterBand(gdal_band))

    def __del__(self):
        self.gdal_ds = None

    def GetDriver(self):
        return self.gdal_ds.GetDriver()

    def GetRasterBand(self, i):
        return self.bands[i-1]

    def GetDescription(self):
        return self.gdal_ds.GetDescription()

    def GetGeoTransform(self):
        return self.gdal_ds.GetGeoTransform()

    def GetProjectionRef(self):
        return self.gdal_ds.GetProjectionRef()

    def GetGCPCount(self):
        return self.gdal_ds.GetGCPCount()

    def GetFileList(self):
        return self.gdal_ds.GetFileList()

    def GetMetadata(self, domain):
        return self.gdal_ds.GetMetadata(domain)

    def GetMetadataItem(self, key, domain):
        return self.gdal_ds.GetMetadataItem(key, domain)

    def FlushCache(self):
        self.gdal_ds.FlushCache()
        return

    def IRasterIO_Read(self, nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize, \
                       nBufType, panBandMap, nPixelSpace, nLineSpace, nBandSpace):
        return self.gdal_ds.ReadRaster(nXOff, nYOff, nXSize, nYSize, \
                                       buf_xsize = nBufXSize, buf_ysize = nBufYSize, \
                                       buf_type = nBufType, band_list = panBandMap, \
                                       buf_pixel_space = nPixelSpace, buf_line_space = nLineSpace, buf_band_space = nBandSpace)

INSTR_GetGDALVersion = 1
INSTR_EXIT = 2
INSTR_EXIT_FAIL = 3
INSTR_SetConfigOption = 4
#INSTR_Progress = 5
INSTR_Reset = 6
INSTR_Open = 7
INSTR_Identify = 8
INSTR_Create = 9
INSTR_CreateCopy = 10
INSTR_QuietDelete = 11
#INSTR_AddBand = 12
INSTR_GetGeoTransform = 13
#INSTR_SetGeoTransform = 14
INSTR_GetProjectionRef = 15
#INSTR_SetProjection = 16
INSTR_GetGCPCount = 17
#INSTR_GetGCPProjection = 18
#INSTR_GetGCPs = 19
#INSTR_SetGCPs = 20
INSTR_GetFileList = 21
INSTR_FlushCache = 22
#INSTR_SetDescription = 23
INSTR_GetMetadata = 24
INSTR_GetMetadataItem = 25
#INSTR_SetMetadata = 26
#INSTR_SetMetadataItem = 27
INSTR_IRasterIO_Read = 28
#INSTR_IRasterIO_Write = 29
#INSTR_IBuildOverviews = 30
#INSTR_AdviseRead = 31
#INSTR_CreateMaskBand = 32
INSTR_Band_First = 33
INSTR_Band_FlushCache = 34
INSTR_Band_GetCategoryNames = 35
#INSTR_Band_SetCategoryNames = 36
#INSTR_Band_SetDescription = 37
INSTR_Band_GetMetadata = 38
INSTR_Band_GetMetadataItem = 39
INSTR_Band_SetMetadata = 40
INSTR_Band_SetMetadataItem = 41
INSTR_Band_GetColorInterpretation = 42
#INSTR_Band_SetColorInterpretation = 43
INSTR_Band_GetNoDataValue = 44
INSTR_Band_GetMinimum = 45
INSTR_Band_GetMaximum = 46
INSTR_Band_GetOffset = 47
INSTR_Band_GetScale = 48
#INSTR_Band_SetNoDataValue = 49
#INSTR_Band_SetOffset = 50
#INSTR_Band_SetScale = 51
INSTR_Band_IReadBlock = 52
#INSTR_Band_IWriteBlock = 53
INSTR_Band_IRasterIO_Read = 54
#INSTR_Band_IRasterIO_Write = 55
INSTR_Band_GetStatistics = 56
#INSTR_Band_ComputeStatistics = 57
#INSTR_Band_SetStatistics = 58
INSTR_Band_ComputeRasterMinMax = 59
INSTR_Band_GetHistogram = 60
INSTR_Band_GetDefaultHistogram = 61
#INSTR_Band_SetDefaultHistogram = 62
INSTR_Band_HasArbitraryOverviews = 63
INSTR_Band_GetOverviewCount = 64
INSTR_Band_GetOverview = 65
INSTR_Band_GetMaskBand = 66
INSTR_Band_GetMaskFlags = 67
#INSTR_Band_CreateMaskBand = 68
#INSTR_Band_Fill = 69
INSTR_Band_GetColorTable = 70
#INSTR_Band_SetColorTable = 71
INSTR_Band_GetUnitType = 72
#INSTR_Band_SetUnitType = 73
#INSTR_Band_BuildOverviews = 74
INSTR_Band_GetDefaultRAT = 75
#INSTR_Band_SetDefaultRAT = 76
#INSTR_Band_AdviseRead = 77
INSTR_Band_End = 78
#INSTR_END = 79

caps_list = [
    INSTR_GetGDALVersion,
    INSTR_EXIT,
    INSTR_EXIT_FAIL,
    INSTR_SetConfigOption,
    #INSTR_Progress,
    INSTR_Reset,
    INSTR_Open,
    INSTR_Identify,
    INSTR_Create,
    INSTR_CreateCopy,
    INSTR_QuietDelete,
    #INSTR_AddBand,
    INSTR_GetGeoTransform,
    #INSTR_SetGeoTransform,
    INSTR_GetProjectionRef,
    #INSTR_SetProjection,
    INSTR_GetGCPCount,
    #INSTR_GetGCPProjection,
    #INSTR_GetGCPs,
    #INSTR_SetGCPs,
    INSTR_GetFileList,
    INSTR_FlushCache,
    #INSTR_SetDescription,
    INSTR_GetMetadata,
    INSTR_GetMetadataItem,
    #INSTR_SetMetadata,
    #INSTR_SetMetadataItem,
    INSTR_IRasterIO_Read,
    #INSTR_IRasterIO_Write,
    #INSTR_IBuildOverviews,
    #INSTR_AdviseRead,
    #INSTR_CreateMaskBand,
    #INSTR_Band_First,
    INSTR_Band_FlushCache,
    INSTR_Band_GetCategoryNames,
    #INSTR_Band_SetCategoryNames,
    #INSTR_Band_SetDescription,
    INSTR_Band_GetMetadata,
    INSTR_Band_GetMetadataItem,
    INSTR_Band_SetMetadata,
    INSTR_Band_SetMetadataItem,
    INSTR_Band_GetColorInterpretation,
    #INSTR_Band_SetColorInterpretation,
    INSTR_Band_GetNoDataValue,
    INSTR_Band_GetMinimum,
    INSTR_Band_GetMaximum,
    INSTR_Band_GetOffset,
    INSTR_Band_GetScale,
    #INSTR_Band_SetNoDataValue,
    #INSTR_Band_SetOffset,
    #INSTR_Band_SetScale,
    INSTR_Band_IReadBlock,
    #INSTR_Band_IWriteBlock,
    INSTR_Band_IRasterIO_Read,
    #INSTR_Band_IRasterIO_Write,
    INSTR_Band_GetStatistics,
    #INSTR_Band_ComputeStatistics,
    #INSTR_Band_SetStatistics,
    INSTR_Band_ComputeRasterMinMax,
    INSTR_Band_GetHistogram,
    #INSTR_Band_GetDefaultHistogram,
    #INSTR_Band_SetDefaultHistogram,
    INSTR_Band_HasArbitraryOverviews,
    INSTR_Band_GetOverviewCount,
    INSTR_Band_GetOverview,
    INSTR_Band_GetMaskBand,
    INSTR_Band_GetMaskFlags,
    #INSTR_Band_CreateMaskBand,
    #INSTR_Band_Fill,
    INSTR_Band_GetColorTable,
    #INSTR_Band_SetColorTable,
    INSTR_Band_GetUnitType,
    #INSTR_Band_SetUnitType,
    #INSTR_Band_BuildOverviews,
    #INSTR_Band_GetDefaultRAT,
    #INSTR_Band_SetDefaultRAT,
    #INSTR_Band_AdviseRead ,
    #INSTR_Band_End,
    #INSTR_END = 79
]

CE_None = 0
CE_Failure = 3

VERBOSE = 0

def read_int():
    if sys.version_info >= (3,0,0):
        return unpack('i', sys.stdin.read(4).encode('latin1'))[0]
    else:
        return unpack('i', sys.stdin.read(4))[0]

def read_double():
    if sys.version_info >= (3,0,0):
        return unpack('d', sys.stdin.read(8).encode('latin1'))[0]
    else:
        return unpack('d', sys.stdin.read(8))[0]

def read_str():
    length = read_int()
    if length <= 0:
        return None
    str = sys.stdin.read(length)
    if len(str) > 0 and str[len(str)-1] == '\0':
        str =  str[0:len(str)-1]
    return str

def read_strlist():
    count = read_int()
    strlist = []
    for i in range(count):
        strlist.append(read_str())
    return strlist

def write_int(i):
    if i is True:
        v = pack('i', 1)
    elif i is False or i is None:
        v = pack('i', 0)
    else:
        v = pack('i', i)
    if sys.version_info >= (3,0,0):
        sys.stdout.write(v.decode('latin1'))
    else:
        sys.stdout.write(v)

def write_double(d):
    if sys.version_info >= (3,0,0):
        sys.stdout.write(pack('d', d).decode('latin1'))
    else:
        sys.stdout.write(pack('d', d))

def write_str(s):
    if s is None:
        write_int(0)
    else:
        l = len(s)
        write_int(l+1)
        sys.stdout.write(s)
        sys.stdout.write('\x00')

def write_band(band, isrv_num):
    if band is not None:
        write_int(isrv_num) # srv band count
        write_int(band.Band) # band number
        write_int(0) # access
        write_int(band.XSize) # X
        write_int(band.YSize) # Y
        write_int(band.DataType) # data type
        write_int(band.BlockXSize) # block x size
        write_int(band.BlockYSize) # block y size
        write_str('') # band description
    else:
        write_int(-1)

def write_ct(ct):
    if ct is None:
        write_int(-1)
    else:
        write_int(ct.GetPaletteInterpretation())
        nCount = ct.GetCount()
        write_int(nCount)
        for i in range(nCount):
            entry = ct.GetColorEntry(i)
            write_int(entry[0])
            write_int(entry[1])
            write_int(entry[2])
            write_int(entry[3])

def write_marker():
    sys.stdout.write('\xDE\xAD\xBE\xEF')

def write_zero_error():
    write_int(0)

def main_loop():

    server_ds = None
    server_bands = []
    gdal.SetConfigOption('GDAL_API_PROXY', 'NO')

    while 1:
        sys.stdout.flush()

        instr = read_int()
        if VERBOSE:
            sys.stderr.write('instr=%d\n' % instr)

        band = None
        if instr >= INSTR_Band_First and instr <= INSTR_Band_End:
            srv_band = read_int()
            band = server_bands[srv_band]

        if instr == INSTR_GetGDALVersion:
            if sys.version_info >= (3,0,0):
                lsb = unpack('B', sys.stdin.read(1).encode('latin1'))[0]
            else:
                lsb = unpack('B', sys.stdin.read(1))[0]
            ver = read_str()
            vmajor = read_int()
            vminor = read_int()
            protovmajor = read_int()
            protovminor = read_int()
            extra_bytes = read_int()
            if VERBOSE:
                sys.stderr.write('lsb=%d\n' % lsb)
                sys.stderr.write('ver=%s\n' % ver)
                sys.stderr.write('vmajor=%d\n' % vmajor)
                sys.stderr.write('vminor=%d\n' % vminor)
                sys.stderr.write('protovmajor=%d\n' % protovmajor)
                sys.stderr.write('protovminor=%d\n' % protovminor)
                sys.stderr.write('extra_bytes=%d\n' % extra_bytes)

            write_str('1.10')
            write_int(1) # vmajor
            write_int(10) # vminor
            write_int(1) # protovmajor
            write_int(0) # protovminor
            write_int(0) # extra bytes
            continue
        elif instr == INSTR_EXIT:
            server_ds = None
            server_bands = []
            write_marker()
            write_int(1)
            sys.exit(0)
        elif instr == INSTR_EXIT_FAIL:
            server_ds = None
            server_bands = []
            write_marker()
            write_int(1)
            sys.exit(1)
        elif instr == INSTR_SetConfigOption:
            key = read_str()
            val = read_str()
            gdal.SetConfigOption(key, val)
            if VERBOSE:
                sys.stderr.write('key=%s\n' % key)
                sys.stderr.write('val=%s\n' % val)
            continue
        elif instr == INSTR_Reset:
            #if server_ds is not None:
            #    sys.stderr.write('Reset(%s)\n' % server_ds.GetDescription())
            server_ds = None
            server_bands = []
            write_marker()
            write_int(1)
        elif instr == INSTR_Open:
            access = read_int()
            filename = read_str()
            cwd = read_str()
            if cwd is not None:
                os.chdir(cwd)
            if VERBOSE:
                sys.stderr.write('access=%d\n' % access)
                sys.stderr.write('filename=%s\n' % filename)
                sys.stderr.write('cwd=%s\n' % cwd)
            #sys.stderr.write('Open(%s)\n' % filename)
            try:
                server_ds = GDALPythonServerDataset(filename, access)
            except:
                server_ds = None

            write_marker()
            if server_ds is None:
                write_int(0) # Failure
            else:
                write_int(1) # Success
                write_int(16) # caps length
                caps = [ 0 for i in range(16)]
                for cap in caps_list:
                    caps[int(cap / 8)] = caps[int(cap / 8)] | (1 << (cap % 8))
                for i in range(16):
                    sys.stdout.write(pack('B', caps[i])) # caps
                write_str(server_ds.GetDescription())
                drv = server_ds.GetDriver()
                if drv is not None:
                    write_str(drv.GetDescription())
                    write_int(0) # End of driver metadata
                else:
                    write_str(None)
                write_int(server_ds.RasterXSize) # X
                write_int(server_ds.RasterYSize) # Y
                write_int(server_ds.RasterCount) # Band count
                write_int(1) # All bands are identical

                if server_ds.RasterCount > 0:
                    write_band(server_ds.GetRasterBand(1), len(server_bands))
                    for i in range(server_ds.RasterCount):
                        server_bands.append(server_ds.GetRasterBand(i + 1))
        elif instr == INSTR_Identify:
            filename = read_str()
            cwd = read_str()
            dr = gdal.IdentifyDriver(filename)
            write_marker()
            if dr is None:
                write_int(0)
            else:
                write_int(1)
        elif instr == INSTR_Create:
            filename = read_str()
            cwd = read_str()
            xsize = read_int()
            ysize = read_int()
            bands = read_int()
            datatype = read_int()
            options = read_strlist()
            write_marker()
            # FIXME
            write_int(0)
        elif instr == INSTR_CreateCopy:
            filename = read_str()
            src_description = read_str()
            cwd = read_str()
            strict = read_int()
            options = read_strlist()
            # FIXME
            write_int(0)
        elif instr == INSTR_QuietDelete:
            filename = read_str()
            cwd = read_str()
            write_marker()
            # FIXME
        elif instr == INSTR_GetGeoTransform:
            gt = server_ds.GetGeoTransform()
            write_marker()
            if gt is not None:
                write_int(CE_None)
                write_int(6 * 8)
                for i in range(6):
                    write_double(gt[i])
            else:
                write_int(CE_Failure)
                write_int(6 * 8)
                write_double(0)
                write_double(1)
                write_double(0)
                write_double(0)
                write_double(0)
                write_double(1)
        elif instr == INSTR_GetProjectionRef:
            write_marker()
            write_str(server_ds.GetProjectionRef())
        elif instr == INSTR_GetGCPCount:
            write_marker()
            write_int(server_ds.GetGCPCount())
        elif instr == INSTR_GetFileList:
            write_marker()
            fl = server_ds.GetFileList()
            write_int(len(fl))
            for i in range(len(fl)):
                write_str(fl[i])
        elif instr == INSTR_GetMetadata:
            domain = read_str()
            md = server_ds.GetMetadata(domain)
            write_marker()
            write_int(len(md))
            for key in md:
                write_str('%s=%s' % (key, md[key]))
        elif instr == INSTR_GetMetadataItem:
            key = read_str()
            domain = read_str()
            val = server_ds.GetMetadataItem(key, domain)
            write_marker()
            write_str(val)
        elif instr == INSTR_IRasterIO_Read:
            nXOff = read_int()
            nYOff = read_int()
            nXSize = read_int()
            nYSize = read_int()
            nBufXSize = read_int()
            nBufYSize = read_int()
            nBufType = read_int()
            nBandCount = read_int()
            panBandMap = []
            size = read_int()
            for i in range(nBandCount):
                panBandMap.append(read_int())
            nPixelSpace = read_int()
            nLineSpace = read_int()
            nBandSpace = read_int()
            val = server_ds.IRasterIO_Read(nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize, nBufType, panBandMap, nPixelSpace, nLineSpace, nBandSpace)
            write_marker()
            if val is None:
                write_int(CE_Failure)
                write_int(0)
            else:
                write_int(CE_None)
                write_int(len(val))
                sys.stdout.write(val)
        elif instr == INSTR_FlushCache:
            if server_ds is not None:
                server_ds.FlushCache()
            write_marker()
        elif instr == INSTR_Band_FlushCache:
            val = band.FlushCache()
            write_marker()
            write_int(val)
        elif instr == INSTR_Band_GetCategoryNames:
            write_marker()
            # FIXME
            write_int(-1)
        elif instr == INSTR_Band_GetMetadata:
            domain = read_str()
            md = band.GetMetadata(domain)
            write_marker()
            write_int(len(md))
            for key in md:
                write_str('%s=%s' % (key, md[key]))
        elif instr == INSTR_Band_GetMetadataItem:
            key = read_str()
            domain = read_str()
            val = band.GetMetadataItem(key, domain)
            write_marker()
            write_str(val)
        elif instr == INSTR_Band_GetColorInterpretation:
            val = band.GetColorInterpretation()
            write_marker()
            write_int(val)
        elif instr == INSTR_Band_GetNoDataValue:
            val = band.GetNoDataValue()
            write_marker()
            if val is None:
                write_int(0)
                write_double(0)
            else:
                write_int(1)
                write_double(val)
        elif instr == INSTR_Band_GetMinimum:
            val = band.GetMinimum()
            write_marker()
            if val is None:
                write_int(0)
                write_double(0)
            else:
                write_int(1)
                write_double(val)
        elif instr == INSTR_Band_GetMaximum:
            val = band.GetMaximum()
            write_marker()
            if val is None:
                write_int(0)
                write_double(0)
            else:
                write_int(1)
                write_double(val)
        elif instr == INSTR_Band_GetOffset:
            val = band.GetOffset()
            write_marker()
            if val is None:
                write_int(0)
                write_double(0)
            else:
                write_int(1)
                write_double(val)
        elif instr == INSTR_Band_GetScale:
            val = band.GetScale()
            write_marker()
            if val is None:
                write_int(0)
                write_double(1) #default value is 1
            else:
                write_int(1)
                write_double(val)
        elif instr == INSTR_Band_IReadBlock:
            nXBlockOff = read_int()
            nYBlockOff = read_int()
            val = band.IReadBlock(nXBlockOff, nYBlockOff)
            write_marker()
            if val is None:
                write_int(CE_Failure)
                l = band.BlockXSize * band.BlockYSize * (gdal.GetDataTypeSize(band.DataType) / 8)
                write_int(l)
                sys.stdout.write(''.join('\0' for i in range(l)))
            else:
                write_int(CE_None)
                write_int(len(val))
                sys.stdout.write(val)
        elif instr == INSTR_Band_IRasterIO_Read:
            nXOff = read_int()
            nYOff = read_int()
            nXSize = read_int()
            nYSize = read_int()
            nBufXSize = read_int()
            nBufYSize = read_int()
            nBufType = read_int()
            val = band.IRasterIO_Read(nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize, nBufType)
            write_marker()
            if val is None:
                write_int(CE_Failure)
                write_int(0)
            else:
                write_int(CE_None)
                write_int(len(val))
                sys.stdout.write(val)
        elif instr == INSTR_Band_GetStatistics:
            approx_ok = read_int()
            force = read_int()
            val = band.GetStatistics(approx_ok, force)
            write_marker()
            if val is None or val[3] < 0:
                write_int(CE_Failure)
            else:
                write_int(CE_None)
                write_double(val[0])
                write_double(val[1])
                write_double(val[2])
                write_double(val[3])
        elif instr == INSTR_Band_ComputeRasterMinMax:
            approx_ok = read_int()
            val = band.ComputeRasterMinMax(approx_ok)
            write_marker()
            if val is None:
                write_int(CE_Failure)
            else:
                write_int(CE_None)
                write_double(val[0])
                write_double(val[1])
        elif instr == INSTR_Band_GetHistogram:
            dfMin = read_double()
            dfMax = read_double()
            nBuckets = read_int()
            bIncludeOutOfRange = read_int()
            bApproxOK = read_int()
            val = band.GetHistogram(dfMin, dfMax, nBuckets, bIncludeOutOfRange, bApproxOK)
            write_marker()
            if val is None:
                write_int(CE_Failure)
            else:
                write_int(CE_None)
                write_int(len(val) * 4)
                for i in range(len(val)):
                    write_int(val[i])
        #elif instr == INSTR_Band_GetDefaultHistogram:
        #    bForce = read_int()
        #    write_marker()
        #    write_int(CE_Failure)
        elif instr == INSTR_Band_HasArbitraryOverviews:
            val = band.HasArbitraryOverviews()
            write_marker()
            write_int(val)
        elif instr == INSTR_Band_GetOverviewCount:
            val = band.GetOverviewCount()
            write_marker()
            write_int(val)
        elif instr == INSTR_Band_GetOverview:
            iovr = read_int()
            ovr_band = band.GetOverview(iovr)
            write_marker()
            write_band(ovr_band, len(server_bands))
            if ovr_band is not None:
                server_bands.append(ovr_band)
        elif instr == INSTR_Band_GetMaskBand:
            msk_band = band.GetMaskBand()
            write_marker()
            write_band(msk_band, len(server_bands))
            if msk_band is not None:
                server_bands.append(msk_band)
        elif instr == INSTR_Band_GetMaskFlags:
            val = band.GetMaskFlags()
            write_marker()
            write_int(val)
        elif instr == INSTR_Band_GetColorTable:
            ct = band.GetColorTable()
            write_marker()
            write_ct(ct)
        elif instr == INSTR_Band_GetUnitType:
            val = band.GetUnitType()
            write_marker()
            write_str(val)
        #elif instr == INSTR_Band_GetDefaultRAT:
        #    write_marker()
        #    # FIXME
        #    write_int(0)
        else:
            break

        write_zero_error()

main_loop()
