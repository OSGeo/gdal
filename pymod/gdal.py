#******************************************************************************
#  $Id$
# 
#  Name:     gdal.py
#  Project:  GDAL Python Interface
#  Purpose:  GDAL Shadow Class Implementations
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
# Revision 1.53  2003/12/02 16:39:05  warmerda
# added GDALGetColorEntry support
#
# Revision 1.52  2003/09/01 15:03:34  dron
# Added DecToDMS() wrapper.
#
# Revision 1.51  2003/07/18 04:52:52  warmerda
# remote GetMetadata() on RasterBand
#
# Revision 1.50  2003/06/10 09:25:29  dron
# Handle the case of empty string in MajorObject::GetMetadata().
#
# Revision 1.49  2003/05/28 19:46:53  warmerda
# added TermProgress
#
# Revision 1.48  2003/05/28 16:20:17  warmerda
# return default transform if GetGeoTransform() fails
#
# Revision 1.47  2003/05/20 14:30:37  warmerda
# fixed GetRasterBand logic
#
# Revision 1.46  2003/04/03 19:27:55  warmerda
# added nullable string support, fixed ogr.Layer.SetAttributeFilter()
#
# Revision 1.45  2003/03/25 19:48:04  warmerda
# Don't throw an exception from GetGeoTransform() on failure, just return
# the default transform.
#
# Revision 1.44  2003/03/25 05:58:37  warmerda
# add better pointer and stringlist support
#
# Revision 1.43  2003/03/18 06:05:12  warmerda
# Added GDALDataset::FlushCache()
#
# Revision 1.42  2003/03/07 16:27:03  warmerda
# some NULL fixes
#
# Revision 1.41  2003/03/02 17:18:47  warmerda
# Updated default args for CPLError.
#
# Revision 1.40  2003/03/02 17:11:27  warmerda
# added error handling support
#
# Revision 1.39  2003/01/20 22:19:28  warmerda
# added buffer size option in ReadAsArray
#
# Revision 1.38  2003/01/18 22:22:12  gwalter
# Lengthened strings in GCP serialize function to avoid truncation.
#
# Revision 1.37  2002/12/06 16:59:58  gwalter
# Added serialize() methods to GCP and ColorTable classes.
#
# Revision 1.36  2002/12/04 19:15:21  warmerda
# fixed gdal.RasterBand.GetRasterColorTable() method
#
# Revision 1.35  2002/11/18 19:25:43  warmerda
# provide access to overviews
#
# Revision 1.34  2002/11/05 12:54:45  dron
# Added GetMetadata()/SetMetadata to the Driver interface
#
# Revision 1.33  2002/09/11 14:30:06  warmerda
# added GDALMajorObject.SetDescription()
#
# Revision 1.32  2002/06/27 15:41:49  warmerda
# added minixml read/write stuff
#
# Revision 1.31  2002/05/28 18:52:23  warmerda
# added GDALOpenShared
#
# Revision 1.30  2002/05/10 02:58:58  warmerda
# added GDALGCPsToGeoTransform
#
# Revision 1.29  2002/01/18 05:46:52  warmerda
# added support for writing arrays to a GDAL band
#

import _gdal
from gdalconst import *
from _gdal import ptrcreate, ptrfree, ptrvalue, ptrset, ptrcast, ptradd, ptrmap

def ToNULLableString(x):
    if x is None or x == 'NULL':
        return 'NULL'
    else:
        l = len(x)
        p = ptrcreate( 'char', '', l+1 )
        for i in range(l):
            ptrset( p, x[i], i )
        ptrset( p, chr(0), l )
        
        return ptrcast(p,'NULLableString')

def FreeNULLableString(x):
    if x is not 'NULL':
        ptrfree( x )

def Debug(msg_class, message):
    _gdal.CPLDebug( msg_class, message )

def Error(err_class = CE_Failure, err_code = CPLE_AppDefined, msg = 'error' ):
    _gdal.CPLError( err_class, err_code, msg )

def ErrorReset():
    _gdal.CPLErrorReset()

def GetLastErrorNo():
    return _gdal.CPLGetLastErrorNo()
    
def GetLastErrorMsg():
    return _gdal.CPLGetLastErrorMsg()

def PushErrorHandler( handler = "CPLQuietErrorHandler" ):
    _gdal.CPLPushErrorHandler( handler )

def PopErrorHandler():
    _gdal.CPLPopErrorHandler()

def ParseXMLString( text ):
    return _gdal.CPLParseXMLString( text )
    
def SerializeXMLTree( tree ):
    return _gdal.CPLSerializeXMLTree( tree )
    
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

def TermProgress( ratio, msg = '', ptr = None ):
    return _gdal.GDALTermProgress( ratio, msg, 'NULL' )

def Open(file,access=GA_ReadOnly):
    _gdal.GDALAllRegister()
    _obj = _gdal.GDALOpen(file,access)
    if _obj is None or _obj == "NULL" :
        return None;
    else:
        _gdal.GDALDereferenceDataset( _obj )
        return Dataset(_obj)

def OpenShared(file,access=GA_ReadOnly):
    _gdal.GDALAllRegister()
    _obj = _gdal.GDALOpenShared(file,access)
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
    if src_ds is None or src_ds == 'NULL':
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

def DecToDMS(angle, axis, precision = 2):
    return _gdal.GDALDecToDMS(angle, axis, precision)

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

    def serialize(self,with_Z=0):
        base = [CXT_Element,'GCP']
        base.append([CXT_Attribute,'Id',[CXT_Text,self.Id]])
        pixval = '%0.15E' % self.GCPPixel       
        lineval = '%0.15E' % self.GCPLine
        xval = '%0.15E' % self.GCPX
        yval = '%0.15E' % self.GCPY
        zval = '%0.15E' % self.GCPZ
        base.append([CXT_Attribute,'Pixel',[CXT_Text,pixval]])
        base.append([CXT_Attribute,'Line',[CXT_Text,lineval]])
        base.append([CXT_Attribute,'X',[CXT_Text,xval]])
        base.append([CXT_Attribute,'Y',[CXT_Text,yval]])
        if with_Z:
            base.append([CXT_Attribute,'Z',[CXT_Text,yval]])        
        return base

def GCPsToGeoTransform( gcp_list, approx_ok = 1 ):
        tuple_list = []
        for gcp in gcp_list:
            tuple_list.append( (gcp.Id, gcp.Info, gcp.GCPPixel, gcp.GCPLine,
                                gcp.GCPX, gcp.GCPY, gcp.GCPZ) )

        return _gdal.GDALGCPsToGeoTransform( tuple_list, approx_ok )

class MajorObject:

    def GetMetadata( self, domain = '' ):
        md = _gdal.GDALGetMetadata( self._o, domain )
	if md is None:
	    return {}
        md_dict = _gdal.StringListToDict( md )
        return md_dict
        
    def SetMetadata(self, metadata, domain = ''):
        md_c = _gdal.DictToStringList( metadata )
        result = _gdal.GDALSetMetadata(self._o, md_c, domain)
        _gdal.CSLDestroy(md_c)
        return result

    def GetDescription(self):
        return _gdal.GDALGetDescription( self._o )
    
    def SetDescription(self, description ):
        _gdal.GDALSetDescription( self._o, description )
    
class Driver(MajorObject):
    
    def __init__(self, _obj):
        self._o = _obj
        self.ShortName = _gdal.GDALGetDriverShortName(self._o)
        self.LongName = _gdal.GDALGetDriverLongName(self._o)
        self.HelpTopic = _gdal.GDALGetDriverHelpTopic(self._o)

    def Create(self, filename, xsize, ysize, bands=1, datatype=GDT_Byte,
               options = []):
        target_ds = _gdal.GDALCreate( self._o, filename, xsize, ysize,
                                      bands, datatype, options )

        if target_ds is None or target_ds == 'NULL':
            return None
        else:
            _gdal.GDALDereferenceDataset( target_ds )
            return Dataset(target_ds)
    
    def CreateCopy(self, filename, source_ds, strict=1, options=[],
                   callback = None, callback_data = None ):
        target_ds = _gdal.GDALCreateCopy( self._o, filename, source_ds._o,
                                          strict, options,
                                          callback, callback_data )
        if target_ds is None or target_ds == 'NULL':
            return None
        else:
            _gdal.GDALDereferenceDataset( target_ds )
            return Dataset(target_ds)

    def Delete(self, filename):
        return _gdal.GDALDeleteDataset( self._o, filename )

class Dataset(MajorObject):

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

    def GetRasterBand(self, i):
        if i > 0 and i <= self.RasterCount:
            return self._band[i-1]
        else:
            return None

    def GetGeoTransform(self):
        c_transform = _gdal.ptrcreate('double',0,6)
        if _gdal.GDALGetGeoTransform(self._o, c_transform) == 0:
            transform = ( _gdal.ptrvalue(c_transform,0),
                      _gdal.ptrvalue(c_transform,1),
                      _gdal.ptrvalue(c_transform,2),
                      _gdal.ptrvalue(c_transform,3),
                      _gdal.ptrvalue(c_transform,4),
                      _gdal.ptrvalue(c_transform,5) )
        else:
            transform = (0,1,0,0,0,1)
  
        _gdal.ptrfree( c_transform )

        return transform

    def SetGeoTransform(self,transform):
        c_transform = _gdal.ptrcreate('double',0,6)
        for i in range(6):
            _gdal.ptrset( c_transform, transform[i], i );
        
        err = _gdal.GDALSetGeoTransform(self._o,c_transform)
        _gdal.ptrfree( c_transform )

        return err

    def SetProjection(self,projection):
        return _gdal.GDALSetProjection(self._o,projection)

    def GetProjection(self):
        return _gdal.GDALGetProjectionRef(self._o)

    def GetProjectionRef(self):
        return _gdal.GDALGetProjectionRef(self._o)

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

    def FlushCache(self):
        _gdal.GDALFlushCache( self._o )
    
class Band(MajorObject):
    def __init__(self, _obj):
        self._o = _obj
        self.DataType = _gdal.GDALGetRasterDataType(self._o)
        self.XSize = _gdal.GDALGetRasterBandXSize(self._o)
        self.YSize = _gdal.GDALGetRasterBandYSize(self._o)
        
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

    def ReadAsArray(self, xoff=0, yoff=0, win_xsize=None, win_ysize=None,
                    buf_xsize=None,buf_ysize=None ):
        import gdalnumeric

        return gdalnumeric.BandReadAsArray( self, xoff, yoff,
                                            win_xsize, win_ysize,
                                            buf_xsize, buf_ysize )
    
    def WriteArray(self, array, xoff=0, yoff=0):
        import gdalnumeric

        return gdalnumeric.BandWriteArray( self, array, xoff, yoff )
    
    def GetRasterColorInterpretation(self):
        return _gdal.GDALGetRasterColorInterpretation(self._o)

    def GetRasterColorTable(self):
        _ct = _gdal.GDALGetRasterColorTable( self._o )
        if _ct is None or _ct == 'NULL':
            return None
        else:
            return ColorTable( _ct )

    def SetRasterColorTable(self, ct):
        return _gdal.GDALSetRasterColorTable( self._o, ct._o )
    
    def FlushCache(self):
        return _gdal.GDALFlushRasterCache(self._o)

    def GetHistogram(self, min=-0.5, max=255.5, buckets=256,
                     include_out_of_range=0, approx_ok = 0 ):
        return _gdal.GDALGetRasterHistogram(self._o, min, max, buckets,
                                            include_out_of_range, approx_ok)

    def ComputeRasterMinMax(self, approx_ok = 0):
        c_minmax = ptrcreate('double',0,2)
        _gdal.GDALComputeRasterMinMax(self._o, approx_ok, c_minmax )
        result = ( ptrvalue(c_minmax,0), ptrvalue(c_minmax,1) )
        ptrfree( c_minmax )
        return result
    
    def GetNoDataValue(self):
        c_success_flag = ptrcreate('int',0,1)
        result = _gdal.GDALGetRasterNoDataValue(self._o,c_success_flag)
        success_flag = ptrvalue(c_success_flag,0)
        ptrfree(c_success_flag)
        if success_flag:
            return result
        else:
            return None

    def SetNoDataValue(self,value):
        return _gdal.GDALSetRasterNoDataValue(self._o,value)

    def GetOverviewCount(self):
        return _gdal.GDALGetOverviewCount(self._o)

    def GetOverview(self, ov_index ):
        _o = _gdal.GDALGetOverview( self._o, ov_index )
        if _o is None or _o == 'NULL':
            return None
        else:
            return Band( _obj = _o )

    def Checksum( self, xoff=0, yoff=0, xsize=None, ysize=None ):
        if xsize is None:
            xsize = self.XSize

        if ysize is None:
            ysize = self.YSize

        return _gdal.GDALChecksumImage( self._o, xoff, yoff, xsize, ysize )

class ColorTable:
    def __init__(self, _obj = None, mode = GPI_RGB ):
        if _obj is None:
            self.own_o = 1
            self._o = _gdal.GDALCreateColorTable( mode )
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
        if entry is None or entry == 'NULL':
            return (0,0,0,0)
        else:
            entry = ptrcast(entry,'short_p')
            return (ptrvalue(entry, 0),
                    ptrvalue(entry, 1),
                    ptrvalue(entry, 2),
                    ptrvalue(entry, 3) )
        
    def GetColorEntryAsRGB( self, i ):
        entry = ptrcreate( 'short', 0, 4 )
        _gdal.GDALGetColorEntryAsRGB( self._o, i,
                                      ptrcast(entry,'GDALColorEntry_p') )
        res = (ptrvalue(entry, 0),
               ptrvalue(entry, 1),
               ptrvalue(entry, 2),
               ptrvalue(entry, 3) )
        ptrfree( entry )
        return res
        
    def SetColorEntry( self, i, color ):
        entry = ptrcreate( 'short', 0, 4 )
        for j in range(4):
            if j >= len(color):
                ptrset( entry, 255, j )
            else:
                ptrset( entry, int(color[j]), j )
        res = _gdal.GDALSetColorEntry( self._o, i,
                                       ptrcast(entry,'GDALColorEntry_p') )
        ptrfree( entry )
        return res
    
    def __str__(self):
        str = ''
        count = self.GetCount()
        for i in range(count):
            entry = self.GetColorEntry(i)
            str = str + ('%d: (%3d,%3d,%3d,%3d)\n' \
                         % (i,entry[0],entry[1],entry[2],entry[3]))

        return str

    def serialize(self):
        base=[CXT_Element,'ColorTable']
        for i in range(self.GetCount()):
            centry=self.GetColorEntry(i)
            ebase=[CXT_Element,'Entry']
            ebase.append([CXT_Attribute,'c1',[CXT_Text,str(centry[0])]])
            ebase.append([CXT_Attribute,'c2',[CXT_Text,str(centry[1])]])
            ebase.append([CXT_Attribute,'c3',[CXT_Text,str(centry[2])]])
            ebase.append([CXT_Attribute,'c4',[CXT_Text,str(centry[3])]])
            base.append(ebase)

        return base

