#******************************************************************************
#  $Id$
# 
#  Name:     gdalconst.py
#  Project:  GDAL Python Interface
#  Purpose:  GDAL Shadow Class Implementations
#  Author:   Frank Warmerdam, warmerda@home.com
# 
#******************************************************************************
#  Copyright (c) 2000, Frank Warmerdam
# 
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
# 
#  The above copyright notice and this permission notice shall be included
#  in all copies or substantial portions of the Software.
# 
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
#  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#  DEALINGS IN THE SOFTWARE.
#******************************************************************************
# 
# $Log$
# Revision 1.4  2000/03/10 13:55:56  warmerda
# added lots of methods
#

import _gdal
from gdalconst import *

def GetDataTypeSize(type):
    return _gdal.GDALGetDataTypeSize(type)

def GetDataTypeName(type):
    return _gdal.GDALGetDataTypeName(type)

def GetColorInterpretationName(type):
    return _gdal.GDALGetColorInterpretationName(type)

def GetPaletteInterpretationName(type):
    return _gdal.GDALGetPaletteInterpretationName(type)

def Open(file,access=GA_ReadOnly):
    _gdal.GDALAllRegister()
    _obj = _gdal.GDALOpen(file,access)
    if _obj == "NULL":
        return None;
    else:
        _gdal.GDALDereferenceDataset( _obj )
        return Dataset(_obj)

class Dataset:

    def __init__(self, _obj):
        self._o = _obj
        _gdal.GDALReferenceDataset( _obj )
        self.RasterXSize = _gdal.GDALGetRasterXSize(self._o)
        self.RasterYSize = _gdal.GDALGetRasterYSize(self._o)
        self.RasterCount = _gdal.GDALGetRasterCount(self._o)

        self._band = []
        for i in range(self.RasterCount):
            self._band.append(Band(_gdal.GDALGetRasterBand(self._o,i+1)))

    def __del__(self):
        if self._o:
            if _gdal.GDALDereferenceDataset(self._o) <= 0:
                _gdal.GDALClose(self._o)

    def GetRasterBand(self, i):
        if i > 0 & i <= self.RasterCount:
            return self._band[i-1]
        else:
            return None

    def GetGeoTransform(self):
        return _gdal.GDALGetGeoTransform(self._o)

    def SetGeoTransform(self,transform):
        _gdal.GDALSetGeoTransform(self._o,transform)

    def SetProjection(self,projection):
        return _gdal.GDALSetProjection(self._o,projection)

    def GetProjection(self):
        return _gdal.GDALGetProjectionRef(self._o)

    def GetProjectionRef(self):
        return _gdal.GDALGetProjectionRef(self._o)

class Band:            
    def __init__(self, _obj):
        self._o = _obj
        DataType = _gdal.GDALGetRasterDataType(self._o)
        XSize = _gdal.GDALGetRasterBandXSize(self._o)
        YSize = _gdal.GDALGetRasterBandYSize(self._o)
        
    def ReadRaster(self, xoff, yoff, xsize, ysize,
                   buf_xsize = None, buf_ysize = None, buf_type = None):

        if buf_xsize is None:
            buf_xsize = self.xsize;
        if buf_ysize is None:
            buf_ysize = self.ysize;
        if buf_type is None:
            buf_type = self.DataType;
            
        return _gdal.GDALReadRaster(self._o, xoff, yoff, xsize, ysize,
                                    buf_xsize, buf_ysize,buf_type)
    
    def WriteRaster(self, xoff, yoff, xsize, ysize,
                    buf_string,
                    buf_xsize = None, buf_ysize = None, buf_type = None ):

        if buf_xsize is None:
            buf_xsize = xsize;
        if buf_ysize is None:
            buf_ysize = ysize;
        if buf_type is None:
            buf_type = DataType;

        if len(buf_string) < buf_xsize * buf_ysize \
           * (_gdal.GDALGetDataTypeSize(buf_type) / 8):
            raise ValueError, "raster buffer to small in WriteRaster"
        else:    
            return _gdal.GDALWriteRaster(self._o, xoff, yoff, xsize, ysize,
                                   buf_string, buf_xsize, buf_ysize,buf_type)
    
    def GetRasterColorInterpretation(self):
        return _gdal.GDALGetRasterColorInterpretation(self._o)

    def GetRasterColorTable(self):
        _ct = _gdal.GDALGetRasterColorTable( self._o )
        if _ct is None:
            return None
        else:
            return gdal.GDALColorTable( _ct )

    def FlushCache(self):
        return _gdal.GDALFlushRasterCache(self._o)

    def GetHistogram(self, min=-0.5, max=255.5, buckets=256,
                     include_out_of_range=0, approx_ok = 0 ):
        return _gdal.GDALGetRasterHistogram(self._o, min, max, buckets,
                                            include_out_of_range, approx_ok)
