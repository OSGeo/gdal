#******************************************************************************
#  $Id$
# 
#  Name:     gdal.py
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
# Revision 1.30  2002/05/10 02:58:58  warmerda
# added GDALGCPsToGeoTransform
#
# Revision 1.29  2002/01/18 05:46:52  warmerda
# added support for writing arrays to a GDAL band
#
# Revision 1.28  2001/10/19 15:43:52  warmerda
# added SetGCPs, and SetMetadata support
#
# Revision 1.27  2001/10/01 13:24:17  warmerda
# Fixed last fix.
#
# Revision 1.26  2001/09/30 04:42:13  warmerda
# _gdal.GDALOpen() returns None, not "NULL".
#
# Revision 1.25  2001/08/23 03:37:59  warmerda
# added GetCacheUsed() method
#
# Revision 1.24  2001/06/26 02:22:39  warmerda
# added metadata domain support, and GetSubDatasets
#
# Revision 1.23  2001/05/07 14:50:44  warmerda
# added python access to GDALComputeRasterMinMax
#
# Revision 1.22  2001/01/23 15:49:00  warmerda
# added RGBFile2PCTFile
#
# Revision 1.21  2001/01/22 22:34:06  warmerda
# added median cut, and dithering algorithms
#
# Revision 1.20  2000/12/14 17:38:49  warmerda
# added GDALDriver.Delete
#
# Revision 1.19  2000/10/30 21:25:41  warmerda
# added access to CPL error functions
#
# Revision 1.18  2000/10/30 14:12:49  warmerda
# Fixed bug in GetRasterColorTable().
#
# Revision 1.17  2000/10/06 15:31:34  warmerda
# added nodata support
#
# Revision 1.16  2000/07/27 21:34:08  warmerda
# added description, and driver access
#
# Revision 1.15  2000/07/25 17:45:03  warmerda
# added access to CPLDebug
#
# Revision 1.14  2000/07/19 19:43:29  warmerda
# updated for numpy support
#
# Revision 1.13  2000/06/27 16:48:57  warmerda
# added progress func support
#
# Revision 1.12  2000/06/26 21:11:10  warmerda
# Added default rules for overviews
#
# Revision 1.11  2000/06/26 19:54:52  warmerda
# improve defaulting rules for overviewlist
#
# Revision 1.10  2000/06/26 18:46:31  warmerda
# Added Dataset.BuildOverviews
#
# Revision 1.9  2000/06/26 17:58:15  warmerda
# added driver, createcopy support
#
# Revision 1.8  2000/06/13 18:14:19  warmerda
# added control of the gdal raster cache
#
# Revision 1.7  2000/05/15 14:17:57  warmerda
# fixed metadata handling
#
# Revision 1.6  2000/04/03 19:42:07  warmerda
# fixed up handling of band properties
#
# Revision 1.5  2000/03/31 14:25:43  warmerda
# added metadata and gcp support
#
# Revision 1.4  2000/03/10 13:55:56  warmerda
# added lots of methods
#

import _gdal
from gdalconst import *

def Debug(msg_class, message):
    _gdal.CPLDebug( msg_class, message )

def ErrorReset():
    _gdal.CPLErrorReset()

def GetLastErrorNo():
    return _gdal.CPLGetLastErrorNo()
    
def GetLastErrorMsg():
    return _gdal.CPLGetLastErrorMsg()
    
def GetCacheMax():
    return _gdal.GDALGetCacheMax()

def SetCacheMax( new_max ):
    _gdal.GDALSetCacheMax( new_max )
    
def GetCacheUsed():
    return _gdal.GDALGetCacheUsed()
    
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
    if _obj is None or _obj == "NULL" :
        return None;
    else:
        _gdal.GDALDereferenceDataset( _obj )
        return Dataset(_obj)

def ComputeMedianCutPCT( red, green, blue, color_count, ct,
                         callback = None, callback_data = None ):
    return _gdal.GDALComputeMedianCutPCT( red._o, green._o, blue._o,
                                          color_count, ct._o,
                                          callback, callback_data )

def DitherRGB2PCT( red, green, blue, target, ct,
                   callback = None, callback_data = None ):
    return _gdal.GDALDitherRGB2PCT( red._o, green._o, blue._o, target._o,
                                    ct._o, callback, callback_data )

def RGBFile2PCTFile( src_filename, dst_filename ):
    src_ds = Open(src_filename)
    if src_ds is None:
        return 1

    ct = ColorTable()
    err = ComputeMedianCutPCT( src_ds.GetRasterBand(1),
                               src_ds.GetRasterBand(2),
                               src_ds.GetRasterBand(3),
                               256, ct )
    if err <> 0:
        return err

    gtiff_driver = GetDriverByName('GTiff')
    if gtiff_driver is None:
        return 1

    dst_ds = gtiff_driver.Create( dst_filename,
                                  src_ds.RasterXSize, src_ds.RasterYSize )
    dst_ds.GetRasterBand(1).SetRasterColorTable( ct )

    err = DitherRGB2PCT( src_ds.GetRasterBand(1),
                         src_ds.GetRasterBand(2),
                         src_ds.GetRasterBand(3),
                         dst_ds.GetRasterBand(1),
                         ct )
    dst_ds = None
    src_ds = None

    return 0

def GetDriverList():
    list = []
    _gdal.GDALAllRegister()
    driver_count = _gdal.GDALGetDriverCount()
    for iDriver in range(driver_count):
        list.append( Driver(_gdal.GDALGetDriver( iDriver )) )
    return list

def GetDriverByName(name):
    _gdal.GDALAllRegister()
    driver_count = _gdal.GDALGetDriverCount()
    for iDriver in range(driver_count):
        driver_o = _gdal.GDALGetDriver( iDriver )
        if _gdal.GDALGetDriverShortName(driver_o) == name:
            return Driver(driver_o)
    return None
    
class GCP:
    def __init__(self):
        self.GCPX = 0.0
        self.GCPY = 0.0
        self.GCPZ = 0.0
        self.GCPPixel = 0.0
        self.GCPLine = 0.0
        self.Info = ''
        self.Id = ''

    def __str__(self):
        str = '%s (%.2fP,%.2fL) -> (%.7fE,%.7fN,%.2f) %s' \
              % (self.Id, self.GCPPixel, self.GCPLine,
                 self.GCPX, self.GCPY, self.GCPZ, self.Info)
        return str

def GCPsToGeoTransform( gcp_list, approx_ok = 1 ):
        tuple_list = []
        for gcp in gcp_list:
            tuple_list.append( (gcp.Id, gcp.Info, gcp.GCPPixel, gcp.GCPLine,
                                gcp.GCPX, gcp.GCPY, gcp.GCPZ) )

        return _gdal.GDALGCPsToGeoTransform( tuple_list, approx_ok )
        
class Driver:
    
    def __init__(self, _obj):
        self._o = _obj
        self.ShortName = _gdal.GDALGetDriverShortName(self._o)
        self.LongName = _gdal.GDALGetDriverLongName(self._o)
        self.HelpTopic = _gdal.GDALGetDriverHelpTopic(self._o)

    def Create(self, filename, xsize, ysize, bands=1, datatype=GDT_Byte,
               options = []):
        target_ds = _gdal.GDALCreate( self._o, filename, xsize, ysize,
                                      bands, datatype, options )

        if target_ds is None:
            return None
        else:
            _gdal.GDALDereferenceDataset( target_ds )
            return Dataset(target_ds)
    
    def CreateCopy(self, filename, source_ds, strict=1, options=[],
                   callback = None, callback_data = None ):
        target_ds = _gdal.GDALCreateCopy( self._o, filename, source_ds._o,
                                          strict, options,
                                          callback, callback_data )
        if target_ds is None:
            return None
        else:
            _gdal.GDALDereferenceDataset( target_ds )
            return Dataset(target_ds)

    def Delete(self, filename):
        return _gdal.GDALDeleteDataset( self._o, filename )
        
        
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

    def GetDriver(self):
        return Driver(_obj= _gdal.GDALGetDatasetDriver(self._o))

    def GetDescription(self):
        return _gdal.GDALGetDescription( self._o )
    
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

    def GetMetadata(self, domain = None):
        if domain is None:
            return _gdal.GDALGetMetadata(self._o)
        else:
            return _gdal.GDALGetMetadata(self._o, domain)

    def SetMetadata(self, metadata, domain = None):
        if domain is None:
            return _gdal.GDALSetMetadata(self._o, metadata)
        else:
            return _gdal.GDALSetMetadata(self._o, metadatadomain)

    def GetSubDatasets(self):
        sd_list = []
        
        sd = self.GetMetadata('SUBDATASETS')
        if sd is None:
            return sd_list

        i = 1
        while sd.has_key('SUBDATASET_'+str(i)+'_NAME'):
            sd_list.append( ( sd['SUBDATASET_'+str(i)+'_NAME'],
                              sd['SUBDATASET_'+str(i)+'_DESC'] ) )
            i = i + 1
        return sd_list

    def GetGCPCount(self):
        return _gdal.GDALGetGCPCount(self._o)

    def GetGCPProjection(self):
        return _gdal.GDALGetGCPProjection(self._o)

    def GetGCPs(self):
        gcp_tuple_list = _gdal.GDALGetGCPs(self._o)

        gcp_list = []
        for gcp_tuple in gcp_tuple_list:
            gcp = GCP()
            gcp.Id = gcp_tuple[0]
            gcp.Info = gcp_tuple[1]
            gcp.GCPPixel = gcp_tuple[2]
            gcp.GCPLine = gcp_tuple[3]
            gcp.GCPX = gcp_tuple[4]
            gcp.GCPY = gcp_tuple[5]
            gcp.GCPZ = gcp_tuple[6]
            gcp_list.append(gcp)

        return gcp_list

    def SetGCPs(self, gcp_list, projection = '' ):
        tuple_list = []
        for gcp in gcp_list:
            tuple_list.append( (gcp.Id, gcp.Info, gcp.GCPPixel, gcp.GCPLine,
                                gcp.GCPX, gcp.GCPY, gcp.GCPZ) )

        _gdal.GDALSetGCPs( self._o, tuple_list, projection )

    def BuildOverviews(self, resampling="NEAREST", overviewlist = None,
                       callback = None, callback_data = None):
        if overviewlist is None:
            if self.RasterXSize > 4096:
                overviewlist = [2,4,8,16,32,64]
            elif self.RasterXSize > 2048:
                overviewlist = [2,4,8,16,32]
            elif self.RasterXSize > 1024:
                overviewlist = [2,4,8,16]
            else:
                overviewlist = [2,4,8]
                
        return _gdal.GDALBuildOverviews(self._o, resampling, overviewlist, [],
                                        callback, callback_data )

    def ReadAsArray(self, xoff=0, yoff=0, xsize=None, ysize=None):
        import gdalnumeric
        return gdalnumeric.DatasetReadAsArray( self, xoff, yoff, xsize, ysize )
    
class Band:            
    def __init__(self, _obj):
        self._o = _obj
        self.DataType = _gdal.GDALGetRasterDataType(self._o)
        self.XSize = _gdal.GDALGetRasterBandXSize(self._o)
        self.YSize = _gdal.GDALGetRasterBandYSize(self._o)
        
    def GetDescription(self):
        return _gdal.GDALGetDescription( self._o )
    
    def ReadRaster(self, xoff, yoff, xsize, ysize,
                   buf_xsize = None, buf_ysize = None, buf_type = None):

        if buf_xsize is None:
            buf_xsize = xsize;
        if buf_ysize is None:
            buf_ysize = ysize;
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
            buf_type = self.DataType;

        if len(buf_string) < buf_xsize * buf_ysize \
           * (_gdal.GDALGetDataTypeSize(buf_type) / 8):
            raise ValueError, "raster buffer too small in WriteRaster"
        else:    
            return _gdal.GDALWriteRaster(self._o, xoff, yoff, xsize, ysize,
                                   buf_string, buf_xsize, buf_ysize,buf_type)

    def ReadAsArray(self, xoff=0, yoff=0, xsize=None, ysize=None):
        import gdalnumeric

        return gdalnumeric.BandReadAsArray( self, xoff, yoff, xsize, ysize )
    
    def WriteArray(self, array, xoff=0, yoff=0):
        import gdalnumeric

        return gdalnumeric.BandWriteArray( self, array, xoff, yoff )
    
    def GetRasterColorInterpretation(self):
        return _gdal.GDALGetRasterColorInterpretation(self._o)

    def GetRasterColorTable(self):
        _ct = _gdal.GDALGetRasterColorTable( self._o )
        if _ct is None:
            return None
        else:
            return _gdal.GDALColorTable( _ct )

    def SetRasterColorTable(self, ct):
        return _gdal.GDALSetRasterColorTable( self._o, ct._o )
    
    def FlushCache(self):
        return _gdal.GDALFlushRasterCache(self._o)

    def GetHistogram(self, min=-0.5, max=255.5, buckets=256,
                     include_out_of_range=0, approx_ok = 0 ):
        return _gdal.GDALGetRasterHistogram(self._o, min, max, buckets,
                                            include_out_of_range, approx_ok)

    def ComputeRasterMinMax(self, approx_ok = 0):
        return _gdal.GDALComputeRasterMinMax(self._o, approx_ok )
    
    def GetMetadata(self):
        return _gdal.GDALGetMetadata(self._o)

    def GetNoDataValue(self):
        return _gdal.GDALGetRasterNoDataValue(self._o)

    def SetNoDataValue(self,value):
        return _gdal.GDALSetRasterNoDataValue(self._o,value)


class ColorTable:
    def __init__(self, _obj = None):
        if _obj is None:
            self.own_o = 1
            self._o = _gdal.GDALCreateColorTable( GPI_RGB )
        else:
            self.own_o = 0
            self._o = _obj

    def __del__(self):
        if self.own_o:
            _gdal.GDALDestroyColorTable( self._o )

    def Clone(self):
        new_ct = ColorTable( _gdal.GDALCloneColorTable( self._o ) )
        new_ct.own_o = 1
        return new_ct

    def GetPaletteInterpretation( self ):
        return _gdal.GDALGetPaletteInterpretation( self._o )

    def GetCount( self ):
        return _gdal.GDALGetColorEntryCount( self._o )

    def GetColorEntry( self, i ):
        entry = _gdal.GDALGetColorEntry( self._o, i )
        if entry is None:
            return (0,0,0,0)
        else:
            return (_gdal.GDALColorEntry_c1_get( entry ),
                    _gdal.GDALColorEntry_c2_get( entry ),
                    _gdal.GDALColorEntry_c3_get( entry ),
                    _gdal.GDALColorEntry_c4_get( entry ))
