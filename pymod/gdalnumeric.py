#******************************************************************************
#  $Id$
# 
#  Name:     gdalconst.py
#  Project:  GDAL Python Interface
#  Purpose:  GDAL/numpy integration.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
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
# Revision 1.17  2006/03/21 21:54:00  fwarmerdam
# fixup headers
#
# Revision 1.16  2005/01/24 20:15:24  fwarmerdam
# added UnsignedInt16 hack for backward compatibility
#
# Revision 1.15  2004/11/20 02:02:14  gwalter
# Add option to specify offsets in
# CopyDatasetInfo (for geocoding info).
#
# Revision 1.14  2004/03/23 21:11:27  warmerda
# some Numeric distributions dont have NumericInt, but NumericInteger instead
#
# Revision 1.13  2004/03/21 17:21:52  dron
# Handle unsigned integer types in type code conversion routines.
#
# Revision 1.12  2004/02/08 09:13:56  aamici
# optimize Band.ReadAsArray performance avoiding memory copy.
#
# Revision 1.11  2003/01/29 19:55:31  warmerda
# Added prototype to SaveArray() call
#
# Revision 1.10  2003/01/20 22:19:28  warmerda
# added buffer size option in ReadAsArray
#
# Revision 1.9  2002/10/07 06:46:55  warmerda
# removed extra import for python 2.2 compatibility
#
# Revision 1.8  2002/09/09 14:40:45  warmerda
# removed extra import
#
# Revision 1.7  2002/02/13 17:17:22  warmerda
# improve unknown type handling
#
# Revision 1.6  2002/01/18 05:46:53  warmerda
# added support for writing arrays to a GDAL band
#
# Revision 1.5  2001/10/19 15:45:34  warmerda
# added CopyDatasetInfo
#
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

UnsignedInteger = 'u'
UnsignedInt = 'u'
try:
    UnsignedInt16
except:
    UnsignedInt16 = 'b'
    
def OpenArray( array, prototype_ds = None ):
    ds = gdal.Open( GetArrayFilename(array) )
    if ds is not None and prototype_ds is not None:
        if type(prototype_ds).__name__ == 'str':
            prototype_ds = gdal.Open( prototype_ds )
        if prototype_ds is not None:
            CopyDatasetInfo( prototype_ds, ds )
            
    return ds

def GetArrayFilename( array ):
    _gdal.GDALRegister_NUMPY()
    return _gdal.NumPyArrayToGDALFilename( array )

def LoadFile( filename, xoff=0, yoff=0, xsize=None, ysize=None ):
    ds = gdal.Open( filename )
    if ds is None:
        raise ValueError, "Can't open "+filename+"\n\n"+gdal.GetLastErrorMsg()

    return DatasetReadAsArray( ds, xoff, yoff, xsize, ysize )

def SaveArray( src_array, filename, format = "GTiff", prototype = None ):
    driver = gdal.GetDriverByName( format )
    if driver is None:
        raise ValueError, "Can't find driver "+format

    return driver.CreateCopy( filename, OpenArray(src_array,prototype) )

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
            
def BandReadAsArray( band, xoff, yoff, win_xsize, win_ysize,
                     buf_xsize=None, buf_ysize=None, buf_obj=None ):
    """Pure python implementation of reading a chunk of a GDAL file
    into a numpy array.  Used by the gdal.Band.ReadaAsArray method."""

    if win_xsize is None:
        win_xsize = band.XSize
    if win_ysize is None:
        win_ysize = band.YSize

    if buf_xsize is None:
        buf_xsize = win_xsize
    if buf_ysize is None:
        buf_ysize = win_ysize

    shape = [buf_ysize, buf_xsize]
    datatype = band.DataType
    typecode = GDALTypeCodeToNumericTypeCode( datatype )
    if typecode == None:
        datatype = GDT_Float32
        typecode = Float32
    else:
        datatype = NumericTypeCodeToGDALTypeCode( typecode )

    if buf_obj is None:
        buf_obj = zeros( shape, typecode )

    return _gdal.GDALReadRaster( band._o, xoff, yoff, win_xsize, win_ysize,
                                 buf_xsize, buf_ysize, datatype, buf_obj )

def BandWriteArray( band, array, xoff=0, yoff=0 ):
    """Pure python implementation of writing a chunk of a GDAL file
    from a numpy array.  Used by the gdal.Band.WriteAsArray method."""

    xsize = array.shape[1]
    ysize = array.shape[0]

    if xsize + xoff > band.XSize or ysize + yoff > band.YSize:
        raise ValueError, "array larger than output file, or offset off edge"

    datatype = NumericTypeCodeToGDALTypeCode( array.typecode() )
    if datatype == None:
        raise ValueError, "array does not have corresponding GDAL data type"

    result = band.WriteRaster( xoff, yoff, xsize, ysize,
                               array.tostring(), xsize, ysize, datatype )

    return result

def GDALTypeCodeToNumericTypeCode( gdal_code ):
    if gdal_code == GDT_Byte:
        return UnsignedInt8
    elif gdal_code == GDT_UInt16:
        return UnsignedInt16
    elif gdal_code == GDT_Int16:
        return Int16
    elif gdal_code == GDT_UInt32:
        return UnsignedInt32
    elif gdal_code == GDT_Int32:
        return Int32
    elif gdal_code == GDT_Float32:
        return Float32
    elif gdal_code == GDT_Float64:
        return Float64
    elif gdal_code == GDT_CInt16:
        return Complex32
    elif gdal_code == GDT_CInt32:
        return Complex32
    elif gdal_code == GDT_CFloat32:
        return Complex32
    elif gdal_code == GDT_CFloat64:
        return Complex64
    else:
        return None

def NumericTypeCodeToGDALTypeCode( numeric_code ):
    if numeric_code == UnsignedInt8:
        return GDT_Byte
    elif numeric_code == Int16:
        return GDT_Int16
    elif numeric_code == UnsignedInt16:
        return GDT_UInt16
    elif numeric_code == Int32:
        return GDT_Int32
    elif numeric_code == UnsignedInt32:
        return GDT_UInt32
    elif numeric_code == Int:
        return GDT_Int32
    elif numeric_code == UnsignedInteger:
        return GDT_UInt32
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
    
def CopyDatasetInfo( src, dst, xoff=0, yoff=0 ):
    """
    Copy georeferencing information and metadata from one dataset to another.
    src: input dataset
    dst: output dataset - It can be a ROI - 
    xoff, yoff:  dst's offset with respect to src in pixel/line.  
    
    Notes: Destination dataset must have update access.  Certain formats
           do not support creation of geotransforms and/or gcps.

    """

    dst.SetMetadata( src.GetMetadata() )
                    


    #Check for geo transform
    gt = src.GetGeoTransform()
    if gt != (0,1,0,0,0,1):
        dst.SetProjection( src.GetProjectionRef() )
        
        if (xoff == 0) and (yoff == 0):
            dst.SetGeoTransform( gt  )
        else:
            ngt = [gt[0],gt[1],gt[2],gt[3],gt[4],gt[5]]
            ngt[0] = gt[0] + xoff*gt[1] + yoff*gt[2];
            ngt[3] = gt[3] + xoff*gt[4] + yoff*gt[5];
            dst.SetGeoTransform( ( ngt[0], ngt[1], ngt[2], ngt[3], ngt[4], ngt[5] ) )
            
    #Check for GCPs
    elif src.GetGCPCount() > 0:
        
        if (xoff == 0) and (yoff == 0):
            dst.SetGCPs( src.GetGCPs(), src.GetGCPProjection() )
        else:
            gcps = src.GetGCPs()
            #Shift gcps
            new_gcps = []
            for gcp in gcps:
                ngcp = gdal.GCP()
                ngcp.GCPX = gcp.GCPX 
                ngcp.GCPY = gcp.GCPY
                ngcp.GCPZ = gcp.GCPZ
                ngcp.GCPPixel = gcp.GCPPixel - xoff
                ngcp.GCPLine = gcp.GCPLine - yoff
                ngcp.Info = gcp.Info
                ngcp.Id = gcp.Id
                new_gcps.append(ngcp)

            try:
                dst.SetGCPs( new_gcps , src.GetGCPProjection() )
            except:
                print "Failed to set GCPs"
                return

    return
        

