# This file was created automatically by SWIG 1.3.29.
# Don't modify this file, modify the SWIG interface instead.
# This file is compatible with both classic and new-style classes.

import _gdalconst
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


GDT_Unknown = _gdalconst.GDT_Unknown
GDT_Byte = _gdalconst.GDT_Byte
GDT_UInt16 = _gdalconst.GDT_UInt16
GDT_Int16 = _gdalconst.GDT_Int16
GDT_UInt32 = _gdalconst.GDT_UInt32
GDT_Int32 = _gdalconst.GDT_Int32
GDT_Float32 = _gdalconst.GDT_Float32
GDT_Float64 = _gdalconst.GDT_Float64
GDT_CInt16 = _gdalconst.GDT_CInt16
GDT_CInt32 = _gdalconst.GDT_CInt32
GDT_CFloat32 = _gdalconst.GDT_CFloat32
GDT_CFloat64 = _gdalconst.GDT_CFloat64
GDT_TypeCount = _gdalconst.GDT_TypeCount
GA_ReadOnly = _gdalconst.GA_ReadOnly
GA_Update = _gdalconst.GA_Update
GF_Read = _gdalconst.GF_Read
GF_Write = _gdalconst.GF_Write
GCI_Undefined = _gdalconst.GCI_Undefined
GCI_GrayIndex = _gdalconst.GCI_GrayIndex
GCI_PaletteIndex = _gdalconst.GCI_PaletteIndex
GCI_RedBand = _gdalconst.GCI_RedBand
GCI_GreenBand = _gdalconst.GCI_GreenBand
GCI_BlueBand = _gdalconst.GCI_BlueBand
GCI_AlphaBand = _gdalconst.GCI_AlphaBand
GCI_HueBand = _gdalconst.GCI_HueBand
GCI_SaturationBand = _gdalconst.GCI_SaturationBand
GCI_LightnessBand = _gdalconst.GCI_LightnessBand
GCI_CyanBand = _gdalconst.GCI_CyanBand
GCI_MagentaBand = _gdalconst.GCI_MagentaBand
GCI_YellowBand = _gdalconst.GCI_YellowBand
GCI_BlackBand = _gdalconst.GCI_BlackBand
GRA_NearestNeighbour = _gdalconst.GRA_NearestNeighbour
GRA_Bilinear = _gdalconst.GRA_Bilinear
GRA_Cubic = _gdalconst.GRA_Cubic
GRA_CubicSpline = _gdalconst.GRA_CubicSpline
GPI_Gray = _gdalconst.GPI_Gray
GPI_RGB = _gdalconst.GPI_RGB
GPI_CMYK = _gdalconst.GPI_CMYK
GPI_HLS = _gdalconst.GPI_HLS
CXT_Element = _gdalconst.CXT_Element
CXT_Text = _gdalconst.CXT_Text
CXT_Attribute = _gdalconst.CXT_Attribute
CXT_Comment = _gdalconst.CXT_Comment
CXT_Literal = _gdalconst.CXT_Literal
CE_None = _gdalconst.CE_None
CE_Debug = _gdalconst.CE_Debug
CE_Warning = _gdalconst.CE_Warning
CE_Failure = _gdalconst.CE_Failure
CE_Fatal = _gdalconst.CE_Fatal
CPLE_None = _gdalconst.CPLE_None
CPLE_AppDefined = _gdalconst.CPLE_AppDefined
CPLE_OutOfMemory = _gdalconst.CPLE_OutOfMemory
CPLE_FileIO = _gdalconst.CPLE_FileIO
CPLE_OpenFailed = _gdalconst.CPLE_OpenFailed
CPLE_IllegalArg = _gdalconst.CPLE_IllegalArg
CPLE_NotSupported = _gdalconst.CPLE_NotSupported
CPLE_AssertionFailed = _gdalconst.CPLE_AssertionFailed
CPLE_NoWriteAccess = _gdalconst.CPLE_NoWriteAccess
CPLE_UserInterrupt = _gdalconst.CPLE_UserInterrupt
DMD_LONGNAME = _gdalconst.DMD_LONGNAME
DMD_HELPTOPIC = _gdalconst.DMD_HELPTOPIC
DMD_MIMETYPE = _gdalconst.DMD_MIMETYPE
DMD_EXTENSION = _gdalconst.DMD_EXTENSION
DMD_CREATIONOPTIONLIST = _gdalconst.DMD_CREATIONOPTIONLIST
DMD_CREATIONDATATYPES = _gdalconst.DMD_CREATIONDATATYPES
DCAP_CREATE = _gdalconst.DCAP_CREATE
DCAP_CREATECOPY = _gdalconst.DCAP_CREATECOPY
CPLES_BackslashQuotable = _gdalconst.CPLES_BackslashQuotable
CPLES_XML = _gdalconst.CPLES_XML
CPLES_URL = _gdalconst.CPLES_URL
CPLES_SQL = _gdalconst.CPLES_SQL
CPLES_CSV = _gdalconst.CPLES_CSV


