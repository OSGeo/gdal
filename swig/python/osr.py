# This file was created automatically by SWIG 1.3.29.
# Don't modify this file, modify the SWIG interface instead.
# This file is compatible with both classic and new-style classes.

import _osr
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


SRS_PT_ALBERS_CONIC_EQUAL_AREA = _osr.SRS_PT_ALBERS_CONIC_EQUAL_AREA
SRS_PT_AZIMUTHAL_EQUIDISTANT = _osr.SRS_PT_AZIMUTHAL_EQUIDISTANT
SRS_PT_CASSINI_SOLDNER = _osr.SRS_PT_CASSINI_SOLDNER
SRS_PT_CYLINDRICAL_EQUAL_AREA = _osr.SRS_PT_CYLINDRICAL_EQUAL_AREA
SRS_PT_ECKERT_IV = _osr.SRS_PT_ECKERT_IV
SRS_PT_ECKERT_VI = _osr.SRS_PT_ECKERT_VI
SRS_PT_EQUIDISTANT_CONIC = _osr.SRS_PT_EQUIDISTANT_CONIC
SRS_PT_EQUIRECTANGULAR = _osr.SRS_PT_EQUIRECTANGULAR
SRS_PT_GALL_STEREOGRAPHIC = _osr.SRS_PT_GALL_STEREOGRAPHIC
SRS_PT_GNOMONIC = _osr.SRS_PT_GNOMONIC
SRS_PT_GOODE_HOMOLOSINE = _osr.SRS_PT_GOODE_HOMOLOSINE
SRS_PT_HOTINE_OBLIQUE_MERCATOR = _osr.SRS_PT_HOTINE_OBLIQUE_MERCATOR
SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN = _osr.SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN
SRS_PT_LABORDE_OBLIQUE_MERCATOR = _osr.SRS_PT_LABORDE_OBLIQUE_MERCATOR
SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP = _osr.SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP
SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP = _osr.SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP
SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM = _osr.SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM
SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA = _osr.SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA
SRS_PT_MERCATOR_1SP = _osr.SRS_PT_MERCATOR_1SP
SRS_PT_MERCATOR_2SP = _osr.SRS_PT_MERCATOR_2SP
SRS_PT_MILLER_CYLINDRICAL = _osr.SRS_PT_MILLER_CYLINDRICAL
SRS_PT_MOLLWEIDE = _osr.SRS_PT_MOLLWEIDE
SRS_PT_NEW_ZEALAND_MAP_GRID = _osr.SRS_PT_NEW_ZEALAND_MAP_GRID
SRS_PT_OBLIQUE_STEREOGRAPHIC = _osr.SRS_PT_OBLIQUE_STEREOGRAPHIC
SRS_PT_ORTHOGRAPHIC = _osr.SRS_PT_ORTHOGRAPHIC
SRS_PT_POLAR_STEREOGRAPHIC = _osr.SRS_PT_POLAR_STEREOGRAPHIC
SRS_PT_POLYCONIC = _osr.SRS_PT_POLYCONIC
SRS_PT_ROBINSON = _osr.SRS_PT_ROBINSON
SRS_PT_SINUSOIDAL = _osr.SRS_PT_SINUSOIDAL
SRS_PT_STEREOGRAPHIC = _osr.SRS_PT_STEREOGRAPHIC
SRS_PT_SWISS_OBLIQUE_CYLINDRICAL = _osr.SRS_PT_SWISS_OBLIQUE_CYLINDRICAL
SRS_PT_TRANSVERSE_MERCATOR = _osr.SRS_PT_TRANSVERSE_MERCATOR
SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED = _osr.SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED
SRS_PT_TRANSVERSE_MERCATOR_MI_22 = _osr.SRS_PT_TRANSVERSE_MERCATOR_MI_22
SRS_PT_TRANSVERSE_MERCATOR_MI_23 = _osr.SRS_PT_TRANSVERSE_MERCATOR_MI_23
SRS_PT_TRANSVERSE_MERCATOR_MI_24 = _osr.SRS_PT_TRANSVERSE_MERCATOR_MI_24
SRS_PT_TRANSVERSE_MERCATOR_MI_25 = _osr.SRS_PT_TRANSVERSE_MERCATOR_MI_25
SRS_PT_TUNISIA_MINING_GRID = _osr.SRS_PT_TUNISIA_MINING_GRID
SRS_PT_VANDERGRINTEN = _osr.SRS_PT_VANDERGRINTEN
SRS_PT_KROVAK = _osr.SRS_PT_KROVAK
SRS_PP_CENTRAL_MERIDIAN = _osr.SRS_PP_CENTRAL_MERIDIAN
SRS_PP_SCALE_FACTOR = _osr.SRS_PP_SCALE_FACTOR
SRS_PP_STANDARD_PARALLEL_1 = _osr.SRS_PP_STANDARD_PARALLEL_1
SRS_PP_STANDARD_PARALLEL_2 = _osr.SRS_PP_STANDARD_PARALLEL_2
SRS_PP_PSEUDO_STD_PARALLEL_1 = _osr.SRS_PP_PSEUDO_STD_PARALLEL_1
SRS_PP_LONGITUDE_OF_CENTER = _osr.SRS_PP_LONGITUDE_OF_CENTER
SRS_PP_LATITUDE_OF_CENTER = _osr.SRS_PP_LATITUDE_OF_CENTER
SRS_PP_LONGITUDE_OF_ORIGIN = _osr.SRS_PP_LONGITUDE_OF_ORIGIN
SRS_PP_LATITUDE_OF_ORIGIN = _osr.SRS_PP_LATITUDE_OF_ORIGIN
SRS_PP_FALSE_EASTING = _osr.SRS_PP_FALSE_EASTING
SRS_PP_FALSE_NORTHING = _osr.SRS_PP_FALSE_NORTHING
SRS_PP_AZIMUTH = _osr.SRS_PP_AZIMUTH
SRS_PP_LONGITUDE_OF_POINT_1 = _osr.SRS_PP_LONGITUDE_OF_POINT_1
SRS_PP_LATITUDE_OF_POINT_1 = _osr.SRS_PP_LATITUDE_OF_POINT_1
SRS_PP_LONGITUDE_OF_POINT_2 = _osr.SRS_PP_LONGITUDE_OF_POINT_2
SRS_PP_LATITUDE_OF_POINT_2 = _osr.SRS_PP_LATITUDE_OF_POINT_2
SRS_PP_LONGITUDE_OF_POINT_3 = _osr.SRS_PP_LONGITUDE_OF_POINT_3
SRS_PP_LATITUDE_OF_POINT_3 = _osr.SRS_PP_LATITUDE_OF_POINT_3
SRS_PP_RECTIFIED_GRID_ANGLE = _osr.SRS_PP_RECTIFIED_GRID_ANGLE
SRS_PP_LANDSAT_NUMBER = _osr.SRS_PP_LANDSAT_NUMBER
SRS_PP_PATH_NUMBER = _osr.SRS_PP_PATH_NUMBER
SRS_PP_PERSPECTIVE_POINT_HEIGHT = _osr.SRS_PP_PERSPECTIVE_POINT_HEIGHT
SRS_PP_FIPSZONE = _osr.SRS_PP_FIPSZONE
SRS_PP_ZONE = _osr.SRS_PP_ZONE
SRS_UL_METER = _osr.SRS_UL_METER
SRS_UL_FOOT = _osr.SRS_UL_FOOT
SRS_UL_FOOT_CONV = _osr.SRS_UL_FOOT_CONV
SRS_UL_US_FOOT = _osr.SRS_UL_US_FOOT
SRS_UL_US_FOOT_CONV = _osr.SRS_UL_US_FOOT_CONV
SRS_UL_NAUTICAL_MILE = _osr.SRS_UL_NAUTICAL_MILE
SRS_UL_NAUTICAL_MILE_CONV = _osr.SRS_UL_NAUTICAL_MILE_CONV
SRS_UL_LINK = _osr.SRS_UL_LINK
SRS_UL_LINK_CONV = _osr.SRS_UL_LINK_CONV
SRS_UL_CHAIN = _osr.SRS_UL_CHAIN
SRS_UL_CHAIN_CONV = _osr.SRS_UL_CHAIN_CONV
SRS_UL_ROD = _osr.SRS_UL_ROD
SRS_UL_ROD_CONV = _osr.SRS_UL_ROD_CONV
SRS_DN_NAD27 = _osr.SRS_DN_NAD27
SRS_DN_NAD83 = _osr.SRS_DN_NAD83
SRS_DN_WGS72 = _osr.SRS_DN_WGS72
SRS_DN_WGS84 = _osr.SRS_DN_WGS84
SRS_WGS84_SEMIMAJOR = _osr.SRS_WGS84_SEMIMAJOR
SRS_WGS84_INVFLATTENING = _osr.SRS_WGS84_INVFLATTENING

def GetWellKnownGeogCSAsWKT(*args):
  """GetWellKnownGeogCSAsWKT(char name, char argout) -> OGRErr"""
  return _osr.GetWellKnownGeogCSAsWKT(*args)

def GetUserInputAsWKT(*args):
  """GetUserInputAsWKT(char name, char argout) -> OGRErr"""
  return _osr.GetUserInputAsWKT(*args)
class SpatialReference(_object):
    """Proxy of C++ SpatialReference class"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, SpatialReference, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, SpatialReference, name)
    __repr__ = _swig_repr
    def __init__(self, *args, **kwargs): 
        """__init__(self, char wkt="") -> SpatialReference"""
        this = _osr.new_SpatialReference(*args, **kwargs)
        try: self.this.append(this)
        except: self.this = this
    __swig_destroy__ = _osr.delete_SpatialReference
    __del__ = lambda self : None;
    def __str__(*args):
        """__str__(self) -> char"""
        return _osr.SpatialReference___str__(*args)

    def IsSame(*args):
        """IsSame(self, SpatialReference rhs) -> int"""
        return _osr.SpatialReference_IsSame(*args)

    def IsSameGeogCS(*args):
        """IsSameGeogCS(self, SpatialReference rhs) -> int"""
        return _osr.SpatialReference_IsSameGeogCS(*args)

    def IsGeographic(*args):
        """IsGeographic(self) -> int"""
        return _osr.SpatialReference_IsGeographic(*args)

    def IsProjected(*args):
        """IsProjected(self) -> int"""
        return _osr.SpatialReference_IsProjected(*args)

    def IsLocal(*args):
        """IsLocal(self) -> int"""
        return _osr.SpatialReference_IsLocal(*args)

    def SetAuthority(*args):
        """SetAuthority(self, char pszTargetKey, char pszAuthority, int nCode) -> OGRErr"""
        return _osr.SpatialReference_SetAuthority(*args)

    def GetAttrValue(*args):
        """GetAttrValue(self, char name, int child=0) -> char"""
        return _osr.SpatialReference_GetAttrValue(*args)

    def SetAttrValue(*args):
        """SetAttrValue(self, char name, char value) -> OGRErr"""
        return _osr.SpatialReference_SetAttrValue(*args)

    def SetAngularUnits(*args):
        """SetAngularUnits(self, char name, double to_radians) -> OGRErr"""
        return _osr.SpatialReference_SetAngularUnits(*args)

    def GetAngularUnits(*args):
        """GetAngularUnits(self) -> double"""
        return _osr.SpatialReference_GetAngularUnits(*args)

    def SetLinearUnits(*args):
        """SetLinearUnits(self, char name, double to_meters) -> OGRErr"""
        return _osr.SpatialReference_SetLinearUnits(*args)

    def GetLinearUnits(*args):
        """GetLinearUnits(self) -> double"""
        return _osr.SpatialReference_GetLinearUnits(*args)

    def GetLinearUnitsName(*args):
        """GetLinearUnitsName(self) -> char"""
        return _osr.SpatialReference_GetLinearUnitsName(*args)

    def GetAuthorityCode(*args):
        """GetAuthorityCode(self, char target_key) -> char"""
        return _osr.SpatialReference_GetAuthorityCode(*args)

    def GetAuthorityName(*args):
        """GetAuthorityName(self, char target_key) -> char"""
        return _osr.SpatialReference_GetAuthorityName(*args)

    def SetUTM(*args):
        """SetUTM(self, int zone, int north=1) -> OGRErr"""
        return _osr.SpatialReference_SetUTM(*args)

    def SetStatePlane(*args):
        """SetStatePlane(self, int zone, int is_nad83=1, char unitsname="", double units=0.0) -> OGRErr"""
        return _osr.SpatialReference_SetStatePlane(*args)

    def AutoIdentifyEPSG(*args):
        """AutoIdentifyEPSG(self) -> OGRErr"""
        return _osr.SpatialReference_AutoIdentifyEPSG(*args)

    def SetProjection(*args):
        """SetProjection(self, char arg) -> OGRErr"""
        return _osr.SpatialReference_SetProjection(*args)

    def SetProjParm(*args):
        """SetProjParm(self, char name, double val) -> OGRErr"""
        return _osr.SpatialReference_SetProjParm(*args)

    def GetProjParm(*args):
        """GetProjParm(self, char name, double default_val=0.0) -> double"""
        return _osr.SpatialReference_GetProjParm(*args)

    def SetNormProjParm(*args):
        """SetNormProjParm(self, char name, double val) -> OGRErr"""
        return _osr.SpatialReference_SetNormProjParm(*args)

    def GetNormProjParm(*args):
        """GetNormProjParm(self, char name, double default_val=0.0) -> double"""
        return _osr.SpatialReference_GetNormProjParm(*args)

    def SetACEA(*args):
        """
        SetACEA(self, double dfStdP1, double dfStdP2, double dfCenterLat, 
            double dfCenterLong, double dfFalseEasting, 
            double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetACEA(*args)

    def SetAE(*args):
        """
        SetAE(self, double dfCenterLat, double dfCenterLong, double dfFalseEasting, 
            double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetAE(*args)

    def SetBonne(*args):
        """
        SetBonne(self, double dfStandardParallel, double dfCentralMeridian, 
            double dfFalseEasting, double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetBonne(*args)

    def SetCEA(*args):
        """
        SetCEA(self, double dfStdP1, double dfCentralMeridian, double dfFalseEasting, 
            double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetCEA(*args)

    def SetCS(*args):
        """
        SetCS(self, double dfCenterLat, double dfCenterLong, double dfFalseEasting, 
            double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetCS(*args)

    def SetEC(*args):
        """
        SetEC(self, double dfStdP1, double dfStdP2, double dfCenterLat, 
            double dfCenterLong, double dfFalseEasting, 
            double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetEC(*args)

    def SetEckertIV(*args):
        """SetEckertIV(self, double dfCentralMeridian, double dfFalseEasting, double dfFalseNorthing) -> OGRErr"""
        return _osr.SpatialReference_SetEckertIV(*args)

    def SetEckertVI(*args):
        """SetEckertVI(self, double dfCentralMeridian, double dfFalseEasting, double dfFalseNorthing) -> OGRErr"""
        return _osr.SpatialReference_SetEckertVI(*args)

    def SetEquirectangular(*args):
        """
        SetEquirectangular(self, double dfCenterLat, double dfCenterLong, double dfFalseEasting, 
            double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetEquirectangular(*args)

    def SetGS(*args):
        """SetGS(self, double dfCentralMeridian, double dfFalseEasting, double dfFalseNorthing) -> OGRErr"""
        return _osr.SpatialReference_SetGS(*args)

    def SetGH(*args):
        """SetGH(self, double dfCentralMeridian, double dfFalseEasting, double dfFalseNorthing) -> OGRErr"""
        return _osr.SpatialReference_SetGH(*args)

    def SetGEOS(*args):
        """
        SetGEOS(self, double dfCentralMeridian, double dfSatelliteHeight, 
            double dfFalseEasting, double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetGEOS(*args)

    def SetGnomonic(*args):
        """
        SetGnomonic(self, double dfCenterLat, double dfCenterLong, double dfFalseEasting, 
            double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetGnomonic(*args)

    def SetHOM(*args):
        """
        SetHOM(self, double dfCenterLat, double dfCenterLong, double dfAzimuth, 
            double dfRectToSkew, double dfScale, 
            double dfFalseEasting, double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetHOM(*args)

    def SetHOM2PNO(*args):
        """
        SetHOM2PNO(self, double dfCenterLat, double dfLat1, double dfLong1, 
            double dfLat2, double dfLong2, double dfScale, 
            double dfFalseEasting, double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetHOM2PNO(*args)

    def SetKrovak(*args):
        """
        SetKrovak(self, double dfCenterLat, double dfCenterLong, double dfAzimuth, 
            double dfPseudoStdParallelLat, double dfScale, 
            double dfFalseEasting, double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetKrovak(*args)

    def SetLAEA(*args):
        """
        SetLAEA(self, double dfCenterLat, double dfCenterLong, double dfFalseEasting, 
            double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetLAEA(*args)

    def SetLCC(*args):
        """
        SetLCC(self, double dfStdP1, double dfStdP2, double dfCenterLat, 
            double dfCenterLong, double dfFalseEasting, 
            double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetLCC(*args)

    def SetLCC1SP(*args):
        """
        SetLCC1SP(self, double dfCenterLat, double dfCenterLong, double dfScale, 
            double dfFalseEasting, double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetLCC1SP(*args)

    def SetLCCB(*args):
        """
        SetLCCB(self, double dfStdP1, double dfStdP2, double dfCenterLat, 
            double dfCenterLong, double dfFalseEasting, 
            double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetLCCB(*args)

    def SetMC(*args):
        """
        SetMC(self, double dfCenterLat, double dfCenterLong, double dfFalseEasting, 
            double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetMC(*args)

    def SetMercator(*args):
        """
        SetMercator(self, double dfCenterLat, double dfCenterLong, double dfScale, 
            double dfFalseEasting, double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetMercator(*args)

    def SetMollweide(*args):
        """SetMollweide(self, double dfCentralMeridian, double dfFalseEasting, double dfFalseNorthing) -> OGRErr"""
        return _osr.SpatialReference_SetMollweide(*args)

    def SetNZMG(*args):
        """
        SetNZMG(self, double dfCenterLat, double dfCenterLong, double dfFalseEasting, 
            double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetNZMG(*args)

    def SetOS(*args):
        """
        SetOS(self, double dfOriginLat, double dfCMeridian, double dfScale, 
            double dfFalseEasting, double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetOS(*args)

    def SetOrthographic(*args):
        """
        SetOrthographic(self, double dfCenterLat, double dfCenterLong, double dfFalseEasting, 
            double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetOrthographic(*args)

    def SetPolyconic(*args):
        """
        SetPolyconic(self, double dfCenterLat, double dfCenterLong, double dfFalseEasting, 
            double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetPolyconic(*args)

    def SetPS(*args):
        """
        SetPS(self, double dfCenterLat, double dfCenterLong, double dfScale, 
            double dfFalseEasting, double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetPS(*args)

    def SetRobinson(*args):
        """SetRobinson(self, double dfCenterLong, double dfFalseEasting, double dfFalseNorthing) -> OGRErr"""
        return _osr.SpatialReference_SetRobinson(*args)

    def SetSinusoidal(*args):
        """SetSinusoidal(self, double dfCenterLong, double dfFalseEasting, double dfFalseNorthing) -> OGRErr"""
        return _osr.SpatialReference_SetSinusoidal(*args)

    def SetStereographic(*args):
        """
        SetStereographic(self, double dfCenterLat, double dfCenterLong, double dfScale, 
            double dfFalseEasting, double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetStereographic(*args)

    def SetSOC(*args):
        """
        SetSOC(self, double dfLatitudeOfOrigin, double dfCentralMeridian, 
            double dfFalseEasting, double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetSOC(*args)

    def SetTM(*args):
        """
        SetTM(self, double dfCenterLat, double dfCenterLong, double dfScale, 
            double dfFalseEasting, double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetTM(*args)

    def SetTMVariant(*args):
        """
        SetTMVariant(self, char pszVariantName, double dfCenterLat, double dfCenterLong, 
            double dfScale, double dfFalseEasting, 
            double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetTMVariant(*args)

    def SetTMG(*args):
        """
        SetTMG(self, double dfCenterLat, double dfCenterLong, double dfFalseEasting, 
            double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetTMG(*args)

    def SetTMSO(*args):
        """
        SetTMSO(self, double dfCenterLat, double dfCenterLong, double dfScale, 
            double dfFalseEasting, double dfFalseNorthing) -> OGRErr
        """
        return _osr.SpatialReference_SetTMSO(*args)

    def SetVDG(*args):
        """SetVDG(self, double dfCenterLong, double dfFalseEasting, double dfFalseNorthing) -> OGRErr"""
        return _osr.SpatialReference_SetVDG(*args)

    def SetWellKnownGeogCS(*args):
        """SetWellKnownGeogCS(self, char name) -> OGRErr"""
        return _osr.SpatialReference_SetWellKnownGeogCS(*args)

    def SetFromUserInput(*args):
        """SetFromUserInput(self, char name) -> OGRErr"""
        return _osr.SpatialReference_SetFromUserInput(*args)

    def CopyGeogCSFrom(*args):
        """CopyGeogCSFrom(self, SpatialReference rhs) -> OGRErr"""
        return _osr.SpatialReference_CopyGeogCSFrom(*args)

    def SetTOWGS84(*args):
        """
        SetTOWGS84(self, double p1, double p2, double p3, double p4=0.0, double p5=0.0, 
            double p6=0.0, double p7=0.0) -> OGRErr
        """
        return _osr.SpatialReference_SetTOWGS84(*args)

    def GetTOWGS84(*args):
        """GetTOWGS84(self, double argout) -> OGRErr"""
        return _osr.SpatialReference_GetTOWGS84(*args)

    def SetGeogCS(*args):
        """
        SetGeogCS(self, char pszGeogName, char pszDatumName, char pszEllipsoidName, 
            double dfSemiMajor, double dfInvFlattening, 
            char pszPMName="Greenwich", double dfPMOffset=0.0, 
            char pszUnits="degree", double dfConvertToRadians=0.0174532925199433) -> OGRErr
        """
        return _osr.SpatialReference_SetGeogCS(*args)

    def SetProjCS(*args):
        """SetProjCS(self, char name="unnamed") -> OGRErr"""
        return _osr.SpatialReference_SetProjCS(*args)

    def ImportFromWkt(*args):
        """ImportFromWkt(self, char ppszInput) -> OGRErr"""
        return _osr.SpatialReference_ImportFromWkt(*args)

    def ImportFromProj4(*args):
        """ImportFromProj4(self, char ppszInput) -> OGRErr"""
        return _osr.SpatialReference_ImportFromProj4(*args)

    def ImportFromESRI(*args):
        """ImportFromESRI(self, char ppszInput) -> OGRErr"""
        return _osr.SpatialReference_ImportFromESRI(*args)

    def ImportFromEPSG(*args):
        """ImportFromEPSG(self, int arg) -> OGRErr"""
        return _osr.SpatialReference_ImportFromEPSG(*args)

    def ImportFromPCI(*args):
        """ImportFromPCI(self, char proj, char units="METRE", double argin=0) -> OGRErr"""
        return _osr.SpatialReference_ImportFromPCI(*args)

    def ImportFromUSGS(*args):
        """ImportFromUSGS(self, long proj_code, long zone=0, double argin=0, long datum_code=0) -> OGRErr"""
        return _osr.SpatialReference_ImportFromUSGS(*args)

    def ImportFromXML(*args):
        """ImportFromXML(self, char xmlString) -> OGRErr"""
        return _osr.SpatialReference_ImportFromXML(*args)

    def ExportToWkt(*args):
        """ExportToWkt(self, char argout) -> OGRErr"""
        return _osr.SpatialReference_ExportToWkt(*args)

    def ExportToPrettyWkt(*args):
        """ExportToPrettyWkt(self, char argout, int simplify=0) -> OGRErr"""
        return _osr.SpatialReference_ExportToPrettyWkt(*args)

    def ExportToProj4(*args):
        """ExportToProj4(self, char argout) -> OGRErr"""
        return _osr.SpatialReference_ExportToProj4(*args)

    def ExportToPCI(*args):
        """ExportToPCI(self, char proj, char units, double parms) -> OGRErr"""
        return _osr.SpatialReference_ExportToPCI(*args)

    def ExportToUSGS(*args):
        """ExportToUSGS(self, long code, long zone, double parms, long datum) -> OGRErr"""
        return _osr.SpatialReference_ExportToUSGS(*args)

    def ExportToXML(*args):
        """ExportToXML(self, char argout, char dialect="") -> OGRErr"""
        return _osr.SpatialReference_ExportToXML(*args)

    def CloneGeogCS(*args):
        """CloneGeogCS(self) -> SpatialReference"""
        return _osr.SpatialReference_CloneGeogCS(*args)

    def Validate(*args):
        """Validate(self) -> OGRErr"""
        return _osr.SpatialReference_Validate(*args)

    def StripCTParms(*args):
        """StripCTParms(self) -> OGRErr"""
        return _osr.SpatialReference_StripCTParms(*args)

    def FixupOrdering(*args):
        """FixupOrdering(self) -> OGRErr"""
        return _osr.SpatialReference_FixupOrdering(*args)

    def Fixup(*args):
        """Fixup(self) -> OGRErr"""
        return _osr.SpatialReference_Fixup(*args)

    def MorphToESRI(*args):
        """MorphToESRI(self) -> OGRErr"""
        return _osr.SpatialReference_MorphToESRI(*args)

    def MorphFromESRI(*args):
        """MorphFromESRI(self) -> OGRErr"""
        return _osr.SpatialReference_MorphFromESRI(*args)

SpatialReference_swigregister = _osr.SpatialReference_swigregister
SpatialReference_swigregister(SpatialReference)
GetProjectionMethods = _osr.GetProjectionMethods

class CoordinateTransformation(_object):
    """Proxy of C++ CoordinateTransformation class"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, CoordinateTransformation, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, CoordinateTransformation, name)
    __repr__ = _swig_repr
    def __init__(self, *args): 
        """__init__(self, SpatialReference src, SpatialReference dst) -> CoordinateTransformation"""
        this = _osr.new_CoordinateTransformation(*args)
        try: self.this.append(this)
        except: self.this = this
    __swig_destroy__ = _osr.delete_CoordinateTransformation
    __del__ = lambda self : None;
    def TransformPoint(*args):
        """
        TransformPoint(self, double inout)
        TransformPoint(self, double argout, double x, double y, double z=0.0)
        """
        return _osr.CoordinateTransformation_TransformPoint(*args)

CoordinateTransformation_swigregister = _osr.CoordinateTransformation_swigregister
CoordinateTransformation_swigregister(CoordinateTransformation)



