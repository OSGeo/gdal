#******************************************************************************
#  $Id$
# 
#  Name:     gdalconst.py
#  Project:  GDAL Python Interface
#  Purpose:  GDAL/numpy integration.
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
# Revision 1.4  2001/03/12 19:59:56  warmerda
# added numeric to gdal type translation
#
# Revision 1.3  2000/11/17 17:16:33  warmerda
# improved error reporting
#
# Revision 1.2  2000/07/25 13:06:43  warmerda
# Allow optional specification of window in LoadFile().
#
# Revision 1.1  2000/07/19 19:42:54  warmerda
# New
#

import gdal
import _gdal
from gdalconst import *
from Numeric import *

def OpenArray( array ):
    return gdal.Open( GetArrayFilename(array) )

def GetArrayFilename( array ):
    _gdal.GDALRegister_NUMPY()
    return _gdal.NumPyArrayToGDALFilename( array )

def LoadFile( filename, xoff=0, yoff=0, xsize=None, ysize=None ):
    ds = gdal.Open( filename )
    if ds is None:
        raise ValueError, "Can't open "+filename+"\n\n"+gdal.GetLastErrorMsg()

    return DatasetReadAsArray( ds, xoff, yoff, xsize, ysize )

def SaveArray( src_array, filename, format = "GTiff" ):
    driver = gdal.GetDriverByName( format )
    if driver is None:
        raise ValueError, "Can't find driver "+format

    return driver.CreateCopy( filename, OpenArray(src_array) )

def DatasetReadAsArray( ds, xoff=0, yoff=0, xsize=None, ysize=None ):

    if xsize is None:
        xsize = ds.RasterXSize
    if ysize is None:
        ysize = ds.RasterYSize

    if ds.RasterCount == 1:
        return BandReadAsArray( ds.GetRasterBand(1), xoff, yoff, xsize, ysize)

    datatype = ds.GetRasterBand(1).DataType
    for band_index in range(2,ds.RasterCount+1):
        if datatype != ds.GetRasterBand(band_index).DataType:
            datatype = GDT_Float32
    
    typecode = GDALTypeCodeToNumericTypeCode( datatype )
    if typecode == None:
        datatype = GDT_Float32
        typecode = Float32

    array_list = []
    for band_index in range(1,ds.RasterCount+1):
        band_array = BandReadAsArray( ds.GetRasterBand(band_index),
                                      xoff, yoff, xsize, ysize)
        array_list.append( reshape( band_array, [1,ysize,xsize] ) )

    return concatenate( array_list )
            
def BandReadAsArray( band, xoff, yoff, xsize, ysize ):
    """Pure python implementation of reading a chunk of a GDAL file
    into a numpy array.  Used by the gdal.Band.ReadaAsArray method."""

    if xsize is None:
        xsize = self.XSize
    if ysize is None:
        ysize = self.YSize
            
    shape = [ysize, xsize]
    datatype = band.DataType
    typecode = GDALTypeCodeToNumericTypeCode( datatype )
    if typecode == None:
        datatype = GDT_Float32
        typecode = Float32

    raw_data = band.ReadRaster( xoff, yoff, xsize, ysize,
                                buf_type = datatype );

    m = fromstring(raw_data,typecode)
    m = reshape( m, shape )

    return m

def GDALTypeCodeToNumericTypeCode( gdal_code ):
    from Numeric import *
    
    if gdal_code == GDT_Byte:
        return UnsignedInt8
    elif gdal_code == GDT_UInt16:
        return None
    elif gdal_code == GDT_Int16:
        return Int16
    elif gdal_code == GDT_UInt32:
        return None
    elif gdal_code == GDT_Int32:
        return Int32
    elif gdal_code == GDT_Float32:
        return Float32
    elif gdal_code == GDT_Float64:
        return Float64
    elif gdal_code == GDT_CInt16:
        return None
    elif gdal_code == GDT_CInt32:
        return None
    elif gdal_code == GDT_CFloat32:
        return Complex32
    elif gdal_code == GDT_CFloat64:
        return Complex64
    else:
        return None

def NumericTypeCodeToGDALTypeCode( numeric_code ):
    from Numeric import *

    if numeric_code == UnsignedInt8:
        return GDT_Byte
    elif numeric_code == Int16:
        return GDT_Int16
    elif numeric_code == Int32:
        return GDT_Int32
    elif numeric_code == Float32:
        return GDT_Float32
    elif numeric_code == Float64:
        return GDT_Float64
    elif numeric_code == Complex32:
        return GDT_CFloat32
    elif numeric_code == Complex64:
        return GDT_CFloat64
    else:
        return None
    

