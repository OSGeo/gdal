import sys
sys.path.insert(0,'../build/lib.darwin-8.8.1-i386-2.3/')



import numpy
import _gdal_array

import gdalconst
import gdal
codes = {   gdalconst.GDT_Byte      :   numpy.uint8,
            gdalconst.GDT_UInt16    :   numpy.uint16,
            gdalconst.GDT_Int16     :   numpy.int16,
            gdalconst.GDT_UInt32    :   numpy.uint32,
            gdalconst.GDT_Int32     :   numpy.int32,
            gdalconst.GDT_Float32   :   numpy.float32,
            gdalconst.GDT_Float64   :   numpy.float64,
            gdalconst.GDT_CInt16    :   numpy.complex,
            gdalconst.GDT_CInt32    :   numpy.complex,
            gdalconst.GDT_CFloat32  :   numpy.complex,
            gdalconst.GDT_CFloat64  :   numpy.complex64
        }

def GetArrayFilename( array ):
    return _gdal_array.GetArrayFilename(array)
    
def OpenArray( array, prototype_ds = None ):
    ds = gdal.Open( GetArrayFilename(array) )
    if ds is not None and prototype_ds is not None:
        if type(prototype_ds).__name__ == 'str':
            prototype_ds = gdal.Open( prototype_ds )
        if prototype_ds is not None:
            CopyDatasetInfo( prototype_ds, ds )
            
    return ds
    
    
def flip_code(code):
    if isinstance(code, type):
        for key, value in codes.items():
            if value == code:
                return key
        return None
    else:
        try:
            return codes[code]
        except KeyError:
            return None

def NumericTypeCodeToGDALTypeCode(numeric_type):
    if not isinstance(numeric_type, type):
        raise TypeError, "Input must be a type"
    return flip_code(numeric_type)

def GDALTypeCodeToNumericTypeCode(gdal_code):
    return flip_code(gdal_code)
    
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
            datatype = gdalconst.GDT_Float32
    
    typecode = GDALTypeCodeToNumericTypeCode( datatype )
    if typecode == None:
        datatype = gdalconst.GDT_Float32
        typecode = numpy.float32

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
        datatype = gdalconst.GDT_Float32
        typecode = numpy.float32
    else:
        datatype = NumericTypeCodeToGDALTypeCode( typecode )

    if not buf_obj:
        buf_obj = numpy.zeros( shape, typecode )

    return gdal.ReadRaster( band._o, xoff, yoff, win_xsize, win_ysize,
                                 buf_xsize, buf_ysize, datatype, buf_obj )

def BandWriteArray( band, array, xoff=0, yoff=0 ):
    """Pure python implementation of writing a chunk of a GDAL file
    from a numpy array.  Used by the gdal.Band.WriteAsArray method."""

    xsize = array.shape[1]
    ysize = array.shape[0]

    if xsize + xoff > band.XSize or ysize + yoff > band.YSize:
        raise ValueError, "array larger than output file, or offset off edge"

    datatype = NumericTypeCodeToGDALTypeCode( array.typecode() )
    if not datatype:
        raise ValueError, "array does not have corresponding GDAL data type"

    result = band.WriteRaster( xoff, yoff, xsize, ysize,
                               array.tostring(), xsize, ysize, datatype )

    return result

    
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
        

