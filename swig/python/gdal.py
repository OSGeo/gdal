# This file was created automatically by SWIG 1.3.29.
# Don't modify this file, modify the SWIG interface instead.
# This file is compatible with both classic and new-style classes.

import _gdal
import new
new_instancemethod = new.instancemethod
def _swig_setattr_nondynamic(self,class_type,name,value,static=1):
    if (name == "thisown"): return self.this.own(value)
    if (name == "this"):
        if type(value).__name__ == 'PySwigObject':
            self.__dict__[name] = value
            return
    method = class_type.__swig_setmethods__.get(name,None)
    if method: return method(self,value)
    if (not static) or hasattr(self,name):
        self.__dict__[name] = value
    else:
        raise AttributeError("You cannot add attributes to %s" % self)

def _swig_setattr(self,class_type,name,value):
    return _swig_setattr_nondynamic(self,class_type,name,value,0)

def _swig_getattr(self,class_type,name):
    if (name == "thisown"): return self.this.own()
    method = class_type.__swig_getmethods__.get(name,None)
    if method: return method(self)
    raise AttributeError,name

def _swig_repr(self):
    try: strthis = "proxy of " + self.this.__repr__()
    except: strthis = ""
    return "<%s.%s; %s >" % (self.__class__.__module__, self.__class__.__name__, strthis,)

import types
try:
    _object = types.ObjectType
    _newclass = 1
except AttributeError:
    class _object : pass
    _newclass = 0
del types


from gdalconst import *


def UseExceptions(*args):
  """UseExceptions()"""
  return _gdal.UseExceptions(*args)

def DontUseExceptions(*args):
  """DontUseExceptions()"""
  return _gdal.DontUseExceptions(*args)

def Debug(*args):
  """Debug(char msg_class, char message)"""
  return _gdal.Debug(*args)

def Error(*args):
  """Error(CPLErr msg_class=CE_Failure, int err_code=0, char msg="error")"""
  return _gdal.Error(*args)

def PopErrorHandler(*args):
  """PopErrorHandler()"""
  return _gdal.PopErrorHandler(*args)

def ErrorReset(*args):
  """ErrorReset()"""
  return _gdal.ErrorReset(*args)

def GetLastErrorNo(*args):
  """GetLastErrorNo() -> int"""
  return _gdal.GetLastErrorNo(*args)

def GetLastErrorType(*args):
  """GetLastErrorType() -> CPLErr"""
  return _gdal.GetLastErrorType(*args)

def GetLastErrorMsg(*args):
  """GetLastErrorMsg() -> char"""
  return _gdal.GetLastErrorMsg(*args)

def PushFinderLocation(*args):
  """PushFinderLocation(char ?)"""
  return _gdal.PushFinderLocation(*args)

def PopFinderLocation(*args):
  """PopFinderLocation()"""
  return _gdal.PopFinderLocation(*args)

def FinderClean(*args):
  """FinderClean()"""
  return _gdal.FinderClean(*args)

def FindFile(*args):
  """FindFile(char ?, char ?) -> char"""
  return _gdal.FindFile(*args)

def SetConfigOption(*args):
  """SetConfigOption(char ?, char ?)"""
  return _gdal.SetConfigOption(*args)

def GetConfigOption(*args):
  """GetConfigOption(char ?, char ?) -> char"""
  return _gdal.GetConfigOption(*args)

def CPLBinaryToHex(*args):
  """CPLBinaryToHex(int nBytes, GByte pabyData) -> char"""
  return _gdal.CPLBinaryToHex(*args)

def CPLHexToBinary(*args):
  """CPLHexToBinary(char pszHex, int pnBytes) -> GByte"""
  return _gdal.CPLHexToBinary(*args)
class MajorObject(_object):
    """Proxy of C++ MajorObject class"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, MajorObject, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, MajorObject, name)
    def __init__(self): raise AttributeError, "No constructor defined"
    __repr__ = _swig_repr
    def GetDescription(*args):
        """GetDescription(self) -> char"""
        return _gdal.MajorObject_GetDescription(*args)

    def SetDescription(*args):
        """SetDescription(self, char pszNewDesc)"""
        return _gdal.MajorObject_SetDescription(*args)

    def GetMetadata_Dict(*args):
        """GetMetadata_Dict(self, char pszDomain="") -> char"""
        return _gdal.MajorObject_GetMetadata_Dict(*args)

    def GetMetadata_List(*args):
        """GetMetadata_List(self, char pszDomain="") -> char"""
        return _gdal.MajorObject_GetMetadata_List(*args)

    def SetMetadata(*args):
        """
        SetMetadata(self, char papszMetadata, char pszDomain="") -> CPLErr
        SetMetadata(self, char pszMetadataString, char pszDomain="") -> CPLErr
        """
        return _gdal.MajorObject_SetMetadata(*args)

    def GetMetadata( self, domain = '' ):
      if domain[:4] == 'xml:':
        return self.GetMetadata_List( domain )
      return self.GetMetadata_Dict( domain )

MajorObject_swigregister = _gdal.MajorObject_swigregister
MajorObject_swigregister(MajorObject)

def PushErrorHandler(*args):
  """
    PushErrorHandler(char pszCallbackName="CPLQuietErrorHandler") -> CPLErr
    PushErrorHandler(CPLErrorHandler ?)
    """
  return _gdal.PushErrorHandler(*args)

class Driver(MajorObject):
    """Proxy of C++ Driver class"""
    __swig_setmethods__ = {}
    for _s in [MajorObject]: __swig_setmethods__.update(_s.__swig_setmethods__)
    __setattr__ = lambda self, name, value: _swig_setattr(self, Driver, name, value)
    __swig_getmethods__ = {}
    for _s in [MajorObject]: __swig_getmethods__.update(_s.__swig_getmethods__)
    __getattr__ = lambda self, name: _swig_getattr(self, Driver, name)
    def __init__(self): raise AttributeError, "No constructor defined"
    __repr__ = _swig_repr
    __swig_getmethods__["ShortName"] = _gdal.Driver_ShortName_get
    if _newclass:ShortName = property(_gdal.Driver_ShortName_get)
    __swig_getmethods__["LongName"] = _gdal.Driver_LongName_get
    if _newclass:LongName = property(_gdal.Driver_LongName_get)
    __swig_getmethods__["HelpTopic"] = _gdal.Driver_HelpTopic_get
    if _newclass:HelpTopic = property(_gdal.Driver_HelpTopic_get)
    def Create(*args, **kwargs):
        """
        Create(self, char name, int xsize, int ysize, int bands=1, GDALDataType eType=GDT_Byte, 
            char options=0) -> Dataset
        """
        return _gdal.Driver_Create(*args, **kwargs)

    def CreateCopy(*args, **kwargs):
        """CreateCopy(self, char name, Dataset src, int strict=1, char options=0) -> Dataset"""
        return _gdal.Driver_CreateCopy(*args, **kwargs)

    def Delete(*args):
        """Delete(self, char name) -> int"""
        return _gdal.Driver_Delete(*args)

Driver_swigregister = _gdal.Driver_swigregister
Driver_swigregister(Driver)

class GCP(_object):
    """Proxy of C++ GCP class"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, GCP, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, GCP, name)
    __repr__ = _swig_repr
    __swig_setmethods__["GCPX"] = _gdal.GCP_GCPX_set
    __swig_getmethods__["GCPX"] = _gdal.GCP_GCPX_get
    if _newclass:GCPX = property(_gdal.GCP_GCPX_get, _gdal.GCP_GCPX_set)
    __swig_setmethods__["GCPY"] = _gdal.GCP_GCPY_set
    __swig_getmethods__["GCPY"] = _gdal.GCP_GCPY_get
    if _newclass:GCPY = property(_gdal.GCP_GCPY_get, _gdal.GCP_GCPY_set)
    __swig_setmethods__["GCPZ"] = _gdal.GCP_GCPZ_set
    __swig_getmethods__["GCPZ"] = _gdal.GCP_GCPZ_get
    if _newclass:GCPZ = property(_gdal.GCP_GCPZ_get, _gdal.GCP_GCPZ_set)
    __swig_setmethods__["GCPPixel"] = _gdal.GCP_GCPPixel_set
    __swig_getmethods__["GCPPixel"] = _gdal.GCP_GCPPixel_get
    if _newclass:GCPPixel = property(_gdal.GCP_GCPPixel_get, _gdal.GCP_GCPPixel_set)
    __swig_setmethods__["GCPLine"] = _gdal.GCP_GCPLine_set
    __swig_getmethods__["GCPLine"] = _gdal.GCP_GCPLine_get
    if _newclass:GCPLine = property(_gdal.GCP_GCPLine_get, _gdal.GCP_GCPLine_set)
    __swig_setmethods__["Info"] = _gdal.GCP_Info_set
    __swig_getmethods__["Info"] = _gdal.GCP_Info_get
    if _newclass:Info = property(_gdal.GCP_Info_get, _gdal.GCP_Info_set)
    __swig_setmethods__["Id"] = _gdal.GCP_Id_set
    __swig_getmethods__["Id"] = _gdal.GCP_Id_get
    if _newclass:Id = property(_gdal.GCP_Id_get, _gdal.GCP_Id_set)
    def __init__(self, *args): 
        """
        __init__(self, double x=0.0, double y=0.0, double z=0.0, double pixel=0.0, 
            double line=0.0, char info="", char id="") -> GCP
        """
        this = _gdal.new_GCP(*args)
        try: self.this.append(this)
        except: self.this = this
    __swig_destroy__ = _gdal.delete_GCP
    __del__ = lambda self : None;
    def __str__(self):
      str = '%s (%.2fP,%.2fL) -> (%.7fE,%.7fN,%.2f) %s '\
            % (self.Id, self.GCPPixel, self.GCPLine,
               self.GCPX, self.GCPY, self.GCPZ, self.Info )
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
              base.append([CXT_Attribute,'Z',[CXT_Text,zval]])        
          return base

GCP_swigregister = _gdal.GCP_swigregister
GCP_swigregister(GCP)


def GDAL_GCP_GCPX_get(*args):
  """GDAL_GCP_GCPX_get(GCP h) -> double"""
  return _gdal.GDAL_GCP_GCPX_get(*args)

def GDAL_GCP_GCPX_set(*args):
  """GDAL_GCP_GCPX_set(GCP h, double val)"""
  return _gdal.GDAL_GCP_GCPX_set(*args)

def GDAL_GCP_GCPY_get(*args):
  """GDAL_GCP_GCPY_get(GCP h) -> double"""
  return _gdal.GDAL_GCP_GCPY_get(*args)

def GDAL_GCP_GCPY_set(*args):
  """GDAL_GCP_GCPY_set(GCP h, double val)"""
  return _gdal.GDAL_GCP_GCPY_set(*args)

def GDAL_GCP_GCPZ_get(*args):
  """GDAL_GCP_GCPZ_get(GCP h) -> double"""
  return _gdal.GDAL_GCP_GCPZ_get(*args)

def GDAL_GCP_GCPZ_set(*args):
  """GDAL_GCP_GCPZ_set(GCP h, double val)"""
  return _gdal.GDAL_GCP_GCPZ_set(*args)

def GDAL_GCP_GCPPixel_get(*args):
  """GDAL_GCP_GCPPixel_get(GCP h) -> double"""
  return _gdal.GDAL_GCP_GCPPixel_get(*args)

def GDAL_GCP_GCPPixel_set(*args):
  """GDAL_GCP_GCPPixel_set(GCP h, double val)"""
  return _gdal.GDAL_GCP_GCPPixel_set(*args)

def GDAL_GCP_GCPLine_get(*args):
  """GDAL_GCP_GCPLine_get(GCP h) -> double"""
  return _gdal.GDAL_GCP_GCPLine_get(*args)

def GDAL_GCP_GCPLine_set(*args):
  """GDAL_GCP_GCPLine_set(GCP h, double val)"""
  return _gdal.GDAL_GCP_GCPLine_set(*args)

def GDAL_GCP_Info_get(*args):
  """GDAL_GCP_Info_get(GCP h) -> char"""
  return _gdal.GDAL_GCP_Info_get(*args)

def GDAL_GCP_Info_set(*args):
  """GDAL_GCP_Info_set(GCP h, char val)"""
  return _gdal.GDAL_GCP_Info_set(*args)

def GDAL_GCP_Id_get(*args):
  """GDAL_GCP_Id_get(GCP h) -> char"""
  return _gdal.GDAL_GCP_Id_get(*args)

def GDAL_GCP_Id_set(*args):
  """GDAL_GCP_Id_set(GCP h, char val)"""
  return _gdal.GDAL_GCP_Id_set(*args)

def GDAL_GCP_get_GCPX(*args):
  """GDAL_GCP_get_GCPX(GCP h) -> double"""
  return _gdal.GDAL_GCP_get_GCPX(*args)

def GDAL_GCP_set_GCPX(*args):
  """GDAL_GCP_set_GCPX(GCP h, double val)"""
  return _gdal.GDAL_GCP_set_GCPX(*args)

def GDAL_GCP_get_GCPY(*args):
  """GDAL_GCP_get_GCPY(GCP h) -> double"""
  return _gdal.GDAL_GCP_get_GCPY(*args)

def GDAL_GCP_set_GCPY(*args):
  """GDAL_GCP_set_GCPY(GCP h, double val)"""
  return _gdal.GDAL_GCP_set_GCPY(*args)

def GDAL_GCP_get_GCPZ(*args):
  """GDAL_GCP_get_GCPZ(GCP h) -> double"""
  return _gdal.GDAL_GCP_get_GCPZ(*args)

def GDAL_GCP_set_GCPZ(*args):
  """GDAL_GCP_set_GCPZ(GCP h, double val)"""
  return _gdal.GDAL_GCP_set_GCPZ(*args)

def GDAL_GCP_get_GCPPixel(*args):
  """GDAL_GCP_get_GCPPixel(GCP h) -> double"""
  return _gdal.GDAL_GCP_get_GCPPixel(*args)

def GDAL_GCP_set_GCPPixel(*args):
  """GDAL_GCP_set_GCPPixel(GCP h, double val)"""
  return _gdal.GDAL_GCP_set_GCPPixel(*args)

def GDAL_GCP_get_GCPLine(*args):
  """GDAL_GCP_get_GCPLine(GCP h) -> double"""
  return _gdal.GDAL_GCP_get_GCPLine(*args)

def GDAL_GCP_set_GCPLine(*args):
  """GDAL_GCP_set_GCPLine(GCP h, double val)"""
  return _gdal.GDAL_GCP_set_GCPLine(*args)

def GDAL_GCP_get_Info(*args):
  """GDAL_GCP_get_Info(GCP h) -> char"""
  return _gdal.GDAL_GCP_get_Info(*args)

def GDAL_GCP_set_Info(*args):
  """GDAL_GCP_set_Info(GCP h, char val)"""
  return _gdal.GDAL_GCP_set_Info(*args)

def GDAL_GCP_get_Id(*args):
  """GDAL_GCP_get_Id(GCP h) -> char"""
  return _gdal.GDAL_GCP_get_Id(*args)

def GDAL_GCP_set_Id(*args):
  """GDAL_GCP_set_Id(GCP h, char val)"""
  return _gdal.GDAL_GCP_set_Id(*args)

def GCPsToGeoTransform(*args):
  """GCPsToGeoTransform(int nGCPs, double argout, int bApproxOK=1) -> FALSE_IS_ERR"""
  return _gdal.GCPsToGeoTransform(*args)
class Dataset(MajorObject):
    """Proxy of C++ Dataset class"""
    __swig_setmethods__ = {}
    for _s in [MajorObject]: __swig_setmethods__.update(_s.__swig_setmethods__)
    __setattr__ = lambda self, name, value: _swig_setattr(self, Dataset, name, value)
    __swig_getmethods__ = {}
    for _s in [MajorObject]: __swig_getmethods__.update(_s.__swig_getmethods__)
    __getattr__ = lambda self, name: _swig_getattr(self, Dataset, name)
    def __init__(self): raise AttributeError, "No constructor defined"
    __repr__ = _swig_repr
    __swig_getmethods__["RasterXSize"] = _gdal.Dataset_RasterXSize_get
    if _newclass:RasterXSize = property(_gdal.Dataset_RasterXSize_get)
    __swig_getmethods__["RasterYSize"] = _gdal.Dataset_RasterYSize_get
    if _newclass:RasterYSize = property(_gdal.Dataset_RasterYSize_get)
    __swig_getmethods__["RasterCount"] = _gdal.Dataset_RasterCount_get
    if _newclass:RasterCount = property(_gdal.Dataset_RasterCount_get)
    __swig_destroy__ = _gdal.delete_Dataset
    __del__ = lambda self : None;
    def GetDriver(*args):
        """GetDriver(self) -> Driver"""
        return _gdal.Dataset_GetDriver(*args)

    def GetRasterBand(*args):
        """GetRasterBand(self, int nBand) -> Band"""
        return _gdal.Dataset_GetRasterBand(*args)

    def GetProjection(*args):
        """GetProjection(self) -> char"""
        return _gdal.Dataset_GetProjection(*args)

    def GetProjectionRef(*args):
        """GetProjectionRef(self) -> char"""
        return _gdal.Dataset_GetProjectionRef(*args)

    def SetProjection(*args):
        """SetProjection(self, char prj) -> CPLErr"""
        return _gdal.Dataset_SetProjection(*args)

    def GetGeoTransform(*args):
        """GetGeoTransform(self, double argout)"""
        return _gdal.Dataset_GetGeoTransform(*args)

    def SetGeoTransform(*args):
        """SetGeoTransform(self, double argin) -> CPLErr"""
        return _gdal.Dataset_SetGeoTransform(*args)

    def BuildOverviews(*args, **kwargs):
        """BuildOverviews(self, char resampling="NEAREST", int overviewlist=0) -> int"""
        return _gdal.Dataset_BuildOverviews(*args, **kwargs)

    def GetGCPCount(*args):
        """GetGCPCount(self) -> int"""
        return _gdal.Dataset_GetGCPCount(*args)

    def GetGCPProjection(*args):
        """GetGCPProjection(self) -> char"""
        return _gdal.Dataset_GetGCPProjection(*args)

    def GetGCPs(*args):
        """GetGCPs(self, int nGCPs)"""
        return _gdal.Dataset_GetGCPs(*args)

    def SetGCPs(*args):
        """SetGCPs(self, int nGCPs, char pszGCPProjection) -> CPLErr"""
        return _gdal.Dataset_SetGCPs(*args)

    def FlushCache(*args):
        """FlushCache(self)"""
        return _gdal.Dataset_FlushCache(*args)

    def AddBand(*args, **kwargs):
        """AddBand(self, GDALDataType datatype=GDT_Byte, char options=0) -> CPLErr"""
        return _gdal.Dataset_AddBand(*args, **kwargs)

    def WriteRaster(*args, **kwargs):
        """
        WriteRaster(self, int xoff, int yoff, int xsize, int ysize, int buf_len, 
            int buf_xsize=0, int buf_ysize=0, GDALDataType buf_type=0, 
            int band_list=0) -> CPLErr
        """
        return _gdal.Dataset_WriteRaster(*args, **kwargs)

    def ReadAsArray(self, xoff=0, yoff=0, win_xsize=None, win_ysize=None,
                  buf_xsize=None, buf_ysize=None, buf_obj=None):
        import gdalnumeric
        return gdalnumeric.DatasetReadAsArray( self, xoff, yoff, xsize, ysize )


Dataset_swigregister = _gdal.Dataset_swigregister
Dataset_swigregister(Dataset)

class Band(MajorObject):
    """Proxy of C++ Band class"""
    __swig_setmethods__ = {}
    for _s in [MajorObject]: __swig_setmethods__.update(_s.__swig_setmethods__)
    __setattr__ = lambda self, name, value: _swig_setattr(self, Band, name, value)
    __swig_getmethods__ = {}
    for _s in [MajorObject]: __swig_getmethods__.update(_s.__swig_getmethods__)
    __getattr__ = lambda self, name: _swig_getattr(self, Band, name)
    def __init__(self): raise AttributeError, "No constructor defined"
    __repr__ = _swig_repr
    __swig_getmethods__["XSize"] = _gdal.Band_XSize_get
    if _newclass:XSize = property(_gdal.Band_XSize_get)
    __swig_getmethods__["YSize"] = _gdal.Band_YSize_get
    if _newclass:YSize = property(_gdal.Band_YSize_get)
    __swig_getmethods__["DataType"] = _gdal.Band_DataType_get
    if _newclass:DataType = property(_gdal.Band_DataType_get)
    def GetBlockSize(*args):
        """GetBlockSize(self, int pnBlockXSize, int pnBlockYSize)"""
        return _gdal.Band_GetBlockSize(*args)

    def GetRasterColorInterpretation(*args):
        """GetRasterColorInterpretation(self) -> GDALColorInterp"""
        return _gdal.Band_GetRasterColorInterpretation(*args)

    def SetRasterColorInterpretation(*args):
        """SetRasterColorInterpretation(self, GDALColorInterp val) -> CPLErr"""
        return _gdal.Band_SetRasterColorInterpretation(*args)

    def GetNoDataValue(*args):
        """GetNoDataValue(self, double val)"""
        return _gdal.Band_GetNoDataValue(*args)

    def SetNoDataValue(*args):
        """SetNoDataValue(self, double d) -> CPLErr"""
        return _gdal.Band_SetNoDataValue(*args)

    def GetMinimum(*args):
        """GetMinimum(self, double val)"""
        return _gdal.Band_GetMinimum(*args)

    def GetMaximum(*args):
        """GetMaximum(self, double val)"""
        return _gdal.Band_GetMaximum(*args)

    def GetOffset(*args):
        """GetOffset(self, double val)"""
        return _gdal.Band_GetOffset(*args)

    def GetScale(*args):
        """GetScale(self, double val)"""
        return _gdal.Band_GetScale(*args)

    def GetStatistics(*args):
        """
        GetStatistics(self, int approx_ok, int force, double min, double max, double mean, 
            double stddev) -> CPLErr
        """
        return _gdal.Band_GetStatistics(*args)

    def SetStatistics(*args):
        """SetStatistics(self, double min, double max, double mean, double stddev) -> CPLErr"""
        return _gdal.Band_SetStatistics(*args)

    def GetOverviewCount(*args):
        """GetOverviewCount(self) -> int"""
        return _gdal.Band_GetOverviewCount(*args)

    def GetOverview(*args):
        """GetOverview(self, int i) -> Band"""
        return _gdal.Band_GetOverview(*args)

    def Checksum(*args, **kwargs):
        """Checksum(self, int xoff=0, int yoff=0, int xsize=0, int ysize=0) -> int"""
        return _gdal.Band_Checksum(*args, **kwargs)

    def ComputeRasterMinMax(*args):
        """ComputeRasterMinMax(self, double argout, int approx_ok=0)"""
        return _gdal.Band_ComputeRasterMinMax(*args)

    def ComputeBandStats(*args):
        """ComputeBandStats(self, double argout, int samplestep=1)"""
        return _gdal.Band_ComputeBandStats(*args)

    def Fill(*args):
        """Fill(self, double real_fill, double imag_fill=0.0) -> CPLErr"""
        return _gdal.Band_Fill(*args)

    def ReadRaster(*args, **kwargs):
        """
        ReadRaster(self, int xoff, int yoff, int xsize, int ysize, int buf_len, 
            int buf_xsize=0, int buf_ysize=0, int buf_type=0) -> CPLErr
        """
        return _gdal.Band_ReadRaster(*args, **kwargs)

    def WriteRaster(*args, **kwargs):
        """
        WriteRaster(self, int xoff, int yoff, int xsize, int ysize, int buf_len, 
            int buf_xsize=0, int buf_ysize=0, int buf_type=0) -> CPLErr
        """
        return _gdal.Band_WriteRaster(*args, **kwargs)

    def FlushCache(*args):
        """FlushCache(self)"""
        return _gdal.Band_FlushCache(*args)

    def GetRasterColorTable(*args):
        """GetRasterColorTable(self) -> ColorTable"""
        return _gdal.Band_GetRasterColorTable(*args)

    def SetRasterColorTable(*args):
        """SetRasterColorTable(self, ColorTable arg) -> int"""
        return _gdal.Band_SetRasterColorTable(*args)

    def ReadAsArray(self, xoff=0, yoff=0, win_xsize=None, win_ysize=None,
                    buf_xsize=None, buf_ysize=None, buf_obj=None):
        import gdalnumeric

        return gdalnumeric.BandReadAsArray( self, xoff, yoff,
                                            win_xsize, win_ysize,
                                            buf_xsize, buf_ysize, buf_obj )
      
    def WriteArray(self, array, xoff=0, yoff=0):
        import gdalnumeric

        return gdalnumeric.BandWriteArray( self, array, xoff, yoff )


Band_swigregister = _gdal.Band_swigregister
Band_swigregister(Band)

class ColorTable(_object):
    """Proxy of C++ ColorTable class"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, ColorTable, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, ColorTable, name)
    __repr__ = _swig_repr
    def __init__(self, *args): 
        """__init__(self, GDALPaletteInterp ?=GPI_RGB) -> ColorTable"""
        this = _gdal.new_ColorTable(*args)
        try: self.this.append(this)
        except: self.this = this
    __swig_destroy__ = _gdal.delete_ColorTable
    __del__ = lambda self : None;
    def Clone(*args):
        """Clone(self) -> ColorTable"""
        return _gdal.ColorTable_Clone(*args)

    def GetPaletteInterpretation(*args):
        """GetPaletteInterpretation(self) -> GDALPaletteInterp"""
        return _gdal.ColorTable_GetPaletteInterpretation(*args)

    def GetCount(*args):
        """GetCount(self) -> int"""
        return _gdal.ColorTable_GetCount(*args)

    def GetColorEntry(*args):
        """GetColorEntry(self, int ?) -> GDALColorEntry"""
        return _gdal.ColorTable_GetColorEntry(*args)

    def GetColorEntryAsRGB(*args):
        """GetColorEntryAsRGB(self, int ?, GDALColorEntry ?) -> int"""
        return _gdal.ColorTable_GetColorEntryAsRGB(*args)

    def SetColorEntry(*args):
        """SetColorEntry(self, int ?, GDALColorEntry ?)"""
        return _gdal.ColorTable_SetColorEntry(*args)

ColorTable_swigregister = _gdal.ColorTable_swigregister
ColorTable_swigregister(ColorTable)


def AllRegister(*args):
  """AllRegister()"""
  return _gdal.AllRegister(*args)

def GetCacheMax(*args):
  """GetCacheMax() -> int"""
  return _gdal.GetCacheMax(*args)

def SetCacheMax(*args):
  """SetCacheMax(int nBytes)"""
  return _gdal.SetCacheMax(*args)

def GetCacheUsed(*args):
  """GetCacheUsed() -> int"""
  return _gdal.GetCacheUsed(*args)

def GetDataTypeSize(*args):
  """GetDataTypeSize(GDALDataType ?) -> int"""
  return _gdal.GetDataTypeSize(*args)

def DataTypeIsComplex(*args):
  """DataTypeIsComplex(GDALDataType ?) -> int"""
  return _gdal.DataTypeIsComplex(*args)

def GetDataTypeName(*args):
  """GetDataTypeName(GDALDataType ?) -> char"""
  return _gdal.GetDataTypeName(*args)

def GetDataTypeByName(*args):
  """GetDataTypeByName(char ?) -> GDALDataType"""
  return _gdal.GetDataTypeByName(*args)

def GetColorInterpretationName(*args):
  """GetColorInterpretationName(GDALColorInterp ?) -> char"""
  return _gdal.GetColorInterpretationName(*args)

def GetPaletteInterpretationName(*args):
  """GetPaletteInterpretationName(GDALPaletteInterp ?) -> char"""
  return _gdal.GetPaletteInterpretationName(*args)

def DecToDMS(*args):
  """DecToDMS(double ?, char ?, int ?=2) -> char"""
  return _gdal.DecToDMS(*args)

def PackedDMSToDec(*args):
  """PackedDMSToDec(double ?) -> double"""
  return _gdal.PackedDMSToDec(*args)

def DecToPackedDMS(*args):
  """DecToPackedDMS(double ?) -> double"""
  return _gdal.DecToPackedDMS(*args)

def ParseXMLString(*args):
  """ParseXMLString(char ?) -> CPLXMLNode"""
  return _gdal.ParseXMLString(*args)

def SerializeXMLTree(*args):
  """SerializeXMLTree(CPLXMLNode xmlnode) -> char"""
  return _gdal.SerializeXMLTree(*args)

def GetDriverCount(*args):
  """GetDriverCount() -> int"""
  return _gdal.GetDriverCount(*args)

def GetDriverByName(*args):
  """GetDriverByName(char name) -> Driver"""
  return _gdal.GetDriverByName(*args)

def GetDriver(*args):
  """GetDriver(int i) -> Driver"""
  return _gdal.GetDriver(*args)

def Open(*args):
  """Open(char name, GDALAccess eAccess=GA_ReadOnly) -> Dataset"""
  return _gdal.Open(*args)

def OpenShared(*args):
  """OpenShared(char name, GDALAccess eAccess=GA_ReadOnly) -> Dataset"""
  return _gdal.OpenShared(*args)

def AutoCreateWarpedVRT(*args):
  """
    AutoCreateWarpedVRT(Dataset src_ds, char src_wkt=0, char dst_wkt=0, GDALResampleAlg eResampleAlg=GRA_NearestNeighbour, 
        double maxerror=0.0) -> Dataset
    """
  return _gdal.AutoCreateWarpedVRT(*args)

def GeneralCmdLineProcessor(*args):
  """GeneralCmdLineProcessor(char papszArgv, int nOptions=0) -> char"""
  return _gdal.GeneralCmdLineProcessor(*args)


