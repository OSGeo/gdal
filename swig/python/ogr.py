# This file was created automatically by SWIG 1.3.29.
# Don't modify this file, modify the SWIG interface instead.
# This file is compatible with both classic and new-style classes.

import _ogr
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


wkb25Bit = _ogr.wkb25Bit
wkbUnknown = _ogr.wkbUnknown
wkbPoint = _ogr.wkbPoint
wkbLineString = _ogr.wkbLineString
wkbPolygon = _ogr.wkbPolygon
wkbMultiPoint = _ogr.wkbMultiPoint
wkbMultiLineString = _ogr.wkbMultiLineString
wkbMultiPolygon = _ogr.wkbMultiPolygon
wkbGeometryCollection = _ogr.wkbGeometryCollection
wkbNone = _ogr.wkbNone
wkbLinearRing = _ogr.wkbLinearRing
wkbPoint25D = _ogr.wkbPoint25D
wkbLineString25D = _ogr.wkbLineString25D
wkbPolygon25D = _ogr.wkbPolygon25D
wkbMultiPoint25D = _ogr.wkbMultiPoint25D
wkbMultiLineString25D = _ogr.wkbMultiLineString25D
wkbMultiPolygon25D = _ogr.wkbMultiPolygon25D
wkbGeometryCollection25D = _ogr.wkbGeometryCollection25D
OFTInteger = _ogr.OFTInteger
OFTIntegerList = _ogr.OFTIntegerList
OFTReal = _ogr.OFTReal
OFTRealList = _ogr.OFTRealList
OFTString = _ogr.OFTString
OFTStringList = _ogr.OFTStringList
OFTWideString = _ogr.OFTWideString
OFTWideStringList = _ogr.OFTWideStringList
OFTBinary = _ogr.OFTBinary
OFTDate = _ogr.OFTDate
OFTTime = _ogr.OFTTime
OFTDateTime = _ogr.OFTDateTime
OJUndefined = _ogr.OJUndefined
OJLeft = _ogr.OJLeft
OJRight = _ogr.OJRight
wkbXDR = _ogr.wkbXDR
wkbNDR = _ogr.wkbNDR
OLCRandomRead = _ogr.OLCRandomRead
OLCSequentialWrite = _ogr.OLCSequentialWrite
OLCRandomWrite = _ogr.OLCRandomWrite
OLCFastSpatialFilter = _ogr.OLCFastSpatialFilter
OLCFastFeatureCount = _ogr.OLCFastFeatureCount
OLCFastGetExtent = _ogr.OLCFastGetExtent
OLCCreateField = _ogr.OLCCreateField
OLCTransactions = _ogr.OLCTransactions
OLCDeleteFeature = _ogr.OLCDeleteFeature
OLCFastSetNextByIndex = _ogr.OLCFastSetNextByIndex
ODsCCreateLayer = _ogr.ODsCCreateLayer
ODsCDeleteLayer = _ogr.ODsCDeleteLayer
ODrCCreateDataSource = _ogr.ODrCCreateDataSource
ODrCDeleteDataSource = _ogr.ODrCDeleteDataSource
import osr
class Driver(_object):
    """Proxy of C++ Driver class"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, Driver, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, Driver, name)
    def __init__(self): raise AttributeError, "No constructor defined"
    __repr__ = _swig_repr
    __swig_getmethods__["name"] = _ogr.Driver_name_get
    if _newclass:name = property(_ogr.Driver_name_get)
    def CreateDataSource(*args, **kwargs):
        """CreateDataSource(self, char name, char options=0) -> DataSource"""
        return _ogr.Driver_CreateDataSource(*args, **kwargs)

    def CopyDataSource(*args, **kwargs):
        """CopyDataSource(self, DataSource copy_ds, char name, char options=0) -> DataSource"""
        return _ogr.Driver_CopyDataSource(*args, **kwargs)

    def Open(*args, **kwargs):
        """Open(self, char name, int update=0) -> DataSource"""
        return _ogr.Driver_Open(*args, **kwargs)

    def DeleteDataSource(*args):
        """DeleteDataSource(self, char name) -> int"""
        return _ogr.Driver_DeleteDataSource(*args)

    def TestCapability(*args):
        """TestCapability(self, char cap) -> bool"""
        return _ogr.Driver_TestCapability(*args)

    def GetName(*args):
        """GetName(self) -> char"""
        return _ogr.Driver_GetName(*args)

Driver_swigregister = _ogr.Driver_swigregister
Driver_swigregister(Driver)

class DataSource(_object):
    """Proxy of C++ DataSource class"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, DataSource, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, DataSource, name)
    def __init__(self): raise AttributeError, "No constructor defined"
    __repr__ = _swig_repr
    __swig_getmethods__["name"] = _ogr.DataSource_name_get
    if _newclass:name = property(_ogr.DataSource_name_get)
    __swig_destroy__ = _ogr.delete_DataSource
    __del__ = lambda self : None;
    def GetRefCount(*args):
        """GetRefCount(self) -> int"""
        return _ogr.DataSource_GetRefCount(*args)

    def GetSummaryRefCount(*args):
        """GetSummaryRefCount(self) -> int"""
        return _ogr.DataSource_GetSummaryRefCount(*args)

    def GetLayerCount(*args):
        """GetLayerCount(self) -> int"""
        return _ogr.DataSource_GetLayerCount(*args)

    def GetDriver(*args):
        """GetDriver(self) -> Driver"""
        return _ogr.DataSource_GetDriver(*args)

    def GetName(*args):
        """GetName(self) -> char"""
        return _ogr.DataSource_GetName(*args)

    def DeleteLayer(*args):
        """DeleteLayer(self, int index) -> OGRErr"""
        return _ogr.DataSource_DeleteLayer(*args)

    def CreateLayer(*args, **kwargs):
        """
        CreateLayer(self, char name, SpatialReference reference=None, OGRwkbGeometryType geom_type=wkbUnknown, 
            char options=0) -> Layer
        """
        return _ogr.DataSource_CreateLayer(*args, **kwargs)

    def CopyLayer(*args, **kwargs):
        """CopyLayer(self, Layer src_layer, char new_name, char options=0) -> Layer"""
        return _ogr.DataSource_CopyLayer(*args, **kwargs)

    def GetLayerByIndex(*args, **kwargs):
        """GetLayerByIndex(self, int index=0) -> Layer"""
        return _ogr.DataSource_GetLayerByIndex(*args, **kwargs)

    def GetLayerByName(*args):
        """GetLayerByName(self, char layer_name) -> Layer"""
        return _ogr.DataSource_GetLayerByName(*args)

    def TestCapability(*args):
        """TestCapability(self, char cap) -> bool"""
        return _ogr.DataSource_TestCapability(*args)

    def ExecuteSQL(*args, **kwargs):
        """ExecuteSQL(self, char statement, Geometry geom=None, char dialect="") -> Layer"""
        return _ogr.DataSource_ExecuteSQL(*args, **kwargs)

    def ReleaseResultSet(*args):
        """ReleaseResultSet(self, Layer layer)"""
        return _ogr.DataSource_ReleaseResultSet(*args)

    def Destroy(self):
      "Once called, self has effectively been destroyed.  Do not access. For backwards compatiblity only"
      _ogr.delete_DataSource( self )
      self.thisown = 0

    def Release(self):
      "Once called, self has effectively been destroyed.  Do not access. For backwards compatiblity only"
      _ogr.delete_DataSource( self )
      self.thisown = 0

    def Reference(self):
      "For backwards compatibility only."
      return self.Reference()

    def Dereference(self):
      "For backwards compatibility only."
      self.Dereference()

    def __len__(self):
        """Returns the number of layers on the datasource"""
        return self.GetLayerCount()

    def __getitem__(self, value):
        """Support dictionary, list, and slice -like access to the datasource.
    ] would return the first layer on the datasource.
    aname'] would return the layer named "aname".
    :4] would return a list of the first four layers."""
        import types
        if isinstance(value, types.SliceType):
            output = []
            for i in xrange(value.start,value.stop,step=value.step):
                try:
                    output.append(self.GetLayer(i))
                except OGRError: #we're done because we're off the end
                    return output
            return output
        if isinstance(value, types.IntType):
            if value > len(self)-1:
                raise IndexError
            return self.GetLayer(value)
        elif isinstance(value,types.StringType):
            return self.GetLayer(value)
        else:
            raise TypeError, 'Input %s is not of String or Int type' % type(value)

    def GetLayer(self,iLayer=0):
        """Return the layer given an index or a name"""
        import types
        if isinstance(iLayer, types.StringType):
            return self.GetLayerByName(iLayer)
        elif isinstance(iLayer, types.IntType):
            return self.GetLayerByIndex(iLayer)
        else:
            raise TypeError, "Input %s is not of String or Int type" % type(iLayer)

DataSource_swigregister = _ogr.DataSource_swigregister
DataSource_swigregister(DataSource)

class Layer(_object):
    """Proxy of C++ Layer class"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, Layer, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, Layer, name)
    def __init__(self): raise AttributeError, "No constructor defined"
    __repr__ = _swig_repr
    def GetRefCount(*args):
        """GetRefCount(self) -> int"""
        return _ogr.Layer_GetRefCount(*args)

    def SetSpatialFilter(*args):
        """SetSpatialFilter(self, Geometry filter)"""
        return _ogr.Layer_SetSpatialFilter(*args)

    def SetSpatialFilterRect(*args):
        """SetSpatialFilterRect(self, double minx, double miny, double maxx, double maxy)"""
        return _ogr.Layer_SetSpatialFilterRect(*args)

    def GetSpatialFilter(*args):
        """GetSpatialFilter(self) -> Geometry"""
        return _ogr.Layer_GetSpatialFilter(*args)

    def SetAttributeFilter(*args):
        """SetAttributeFilter(self, char filter_string) -> OGRErr"""
        return _ogr.Layer_SetAttributeFilter(*args)

    def ResetReading(*args):
        """ResetReading(self)"""
        return _ogr.Layer_ResetReading(*args)

    def GetName(*args):
        """GetName(self) -> char"""
        return _ogr.Layer_GetName(*args)

    def GetFeature(*args):
        """GetFeature(self, long fid) -> Feature"""
        return _ogr.Layer_GetFeature(*args)

    def GetNextFeature(*args):
        """GetNextFeature(self) -> Feature"""
        return _ogr.Layer_GetNextFeature(*args)

    def SetNextByIndex(*args):
        """SetNextByIndex(self, long new_index) -> OGRErr"""
        return _ogr.Layer_SetNextByIndex(*args)

    def SetFeature(*args):
        """SetFeature(self, Feature feature) -> OGRErr"""
        return _ogr.Layer_SetFeature(*args)

    def CreateFeature(*args):
        """CreateFeature(self, Feature feature) -> OGRErr"""
        return _ogr.Layer_CreateFeature(*args)

    def DeleteFeature(*args):
        """DeleteFeature(self, long fid) -> OGRErr"""
        return _ogr.Layer_DeleteFeature(*args)

    def SyncToDisk(*args):
        """SyncToDisk(self) -> OGRErr"""
        return _ogr.Layer_SyncToDisk(*args)

    def GetLayerDefn(*args):
        """GetLayerDefn(self) -> FeatureDefn"""
        return _ogr.Layer_GetLayerDefn(*args)

    def GetFeatureCount(*args, **kwargs):
        """GetFeatureCount(self, int force=1) -> int"""
        return _ogr.Layer_GetFeatureCount(*args, **kwargs)

    def GetExtent(*args, **kwargs):
        """GetExtent(self, double argout, int force=1) -> OGRErr"""
        return _ogr.Layer_GetExtent(*args, **kwargs)

    def TestCapability(*args):
        """TestCapability(self, char cap) -> bool"""
        return _ogr.Layer_TestCapability(*args)

    def CreateField(*args, **kwargs):
        """CreateField(self, FieldDefn field_def, int approx_ok=1) -> OGRErr"""
        return _ogr.Layer_CreateField(*args, **kwargs)

    def StartTransaction(*args):
        """StartTransaction(self) -> OGRErr"""
        return _ogr.Layer_StartTransaction(*args)

    def CommitTransaction(*args):
        """CommitTransaction(self) -> OGRErr"""
        return _ogr.Layer_CommitTransaction(*args)

    def RollbackTransaction(*args):
        """RollbackTransaction(self) -> OGRErr"""
        return _ogr.Layer_RollbackTransaction(*args)

    def GetSpatialRef(*args):
        """GetSpatialRef(self) -> SpatialReference"""
        return _ogr.Layer_GetSpatialRef(*args)

    def GetFeatureRead(*args):
        """GetFeatureRead(self) -> GIntBig"""
        return _ogr.Layer_GetFeatureRead(*args)

    def Reference(self):
      "For backwards compatibility only."
      pass

    def Dereference(self):
      "For backwards compatibility only."
      pass

    def __len__(self):
        """Returns the number of features in the layer"""
        return self.GetFeatureCount()

    def __getitem__(self, value):
        """Support list and slice -like access to the layer.
    r[0] would return the first feature on the layer.
    r[0:4] would return a list of the first four features."""
        if isinstance(value, types.SliceType):
            output = []
            if value.stop == sys.maxint:
                
                
                
                stop = len(self) - 1
            else:
                stop = value.stop
            for i in xrange(value.start,stop,step=value.step):
                feature = self.GetFeature(i)
                if feature:
                    output.append(feature)
                else:
                    return output
            return output
        if isinstance(value, types.IntType):
            if value > len(self)-1:
                raise IndexError
            return self.GetFeature(value)
        else:
            raise TypeError,"Input %s is not of IntType or SliceType" % type(value)

    def CreateFields(fields):
        """Create a list of fields on the Layer"""
        for i in fields:
            self.CreateField(i)

    def __iter__(self):
        return self

    def next(self):
        feature = self.GetNextFeature()
        if not feature:
            raise StopIteration
        else:
            return feature

Layer_swigregister = _ogr.Layer_swigregister
Layer_swigregister(Layer)

class Feature(_object):
    """Proxy of C++ Feature class"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, Feature, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, Feature, name)
    __repr__ = _swig_repr
    __swig_destroy__ = _ogr.delete_Feature
    __del__ = lambda self : None;
    def __init__(self, *args, **kwargs): 
        """__init__(self, FeatureDefn feature_def=0) -> Feature"""
        this = _ogr.new_Feature(*args, **kwargs)
        try: self.this.append(this)
        except: self.this = this
    def GetDefnRef(*args):
        """GetDefnRef(self) -> FeatureDefn"""
        return _ogr.Feature_GetDefnRef(*args)

    def SetGeometry(*args):
        """SetGeometry(self, Geometry geom) -> OGRErr"""
        return _ogr.Feature_SetGeometry(*args)

    def SetGeometryDirectly(*args):
        """SetGeometryDirectly(self, Geometry geom) -> OGRErr"""
        return _ogr.Feature_SetGeometryDirectly(*args)

    def GetGeometryRef(*args):
        """GetGeometryRef(self) -> Geometry"""
        return _ogr.Feature_GetGeometryRef(*args)

    def Clone(*args):
        """Clone(self) -> Feature"""
        return _ogr.Feature_Clone(*args)

    def Equal(*args):
        """Equal(self, Feature feature) -> bool"""
        return _ogr.Feature_Equal(*args)

    def GetFieldCount(*args):
        """GetFieldCount(self) -> int"""
        return _ogr.Feature_GetFieldCount(*args)

    def GetFieldDefnRef(*args):
        """
        GetFieldDefnRef(self, int id) -> FieldDefn
        GetFieldDefnRef(self, char name) -> FieldDefn
        """
        return _ogr.Feature_GetFieldDefnRef(*args)

    def GetFieldAsString(*args):
        """
        GetFieldAsString(self, int id) -> char
        GetFieldAsString(self, char name) -> char
        """
        return _ogr.Feature_GetFieldAsString(*args)

    def GetFieldAsInteger(*args):
        """
        GetFieldAsInteger(self, int id) -> int
        GetFieldAsInteger(self, char name) -> int
        """
        return _ogr.Feature_GetFieldAsInteger(*args)

    def GetFieldAsDouble(*args):
        """
        GetFieldAsDouble(self, int id) -> double
        GetFieldAsDouble(self, char name) -> double
        """
        return _ogr.Feature_GetFieldAsDouble(*args)

    def IsFieldSet(*args):
        """
        IsFieldSet(self, int id) -> bool
        IsFieldSet(self, char name) -> bool
        """
        return _ogr.Feature_IsFieldSet(*args)

    def GetFieldIndex(*args):
        """GetFieldIndex(self, char name) -> int"""
        return _ogr.Feature_GetFieldIndex(*args)

    def GetFID(*args):
        """GetFID(self) -> int"""
        return _ogr.Feature_GetFID(*args)

    def SetFID(*args):
        """SetFID(self, int fid) -> OGRErr"""
        return _ogr.Feature_SetFID(*args)

    def DumpReadable(*args):
        """DumpReadable(self)"""
        return _ogr.Feature_DumpReadable(*args)

    def UnsetField(*args):
        """
        UnsetField(self, int id)
        UnsetField(self, char name)
        """
        return _ogr.Feature_UnsetField(*args)

    def SetField(*args):
        """
        SetField(self, int id, char value)
        SetField(self, char name, char value)
        """
        return _ogr.Feature_SetField(*args)

    def SetFrom(*args, **kwargs):
        """SetFrom(self, Feature other, int forgiving=1) -> OGRErr"""
        return _ogr.Feature_SetFrom(*args, **kwargs)

    def GetStyleString(*args):
        """GetStyleString(self) -> char"""
        return _ogr.Feature_GetStyleString(*args)

    def SetStyleString(*args):
        """SetStyleString(self, char the_string)"""
        return _ogr.Feature_SetStyleString(*args)

    def GetFieldType(*args):
        """
        GetFieldType(self, int id) -> OGRFieldType
        GetFieldType(self, char name) -> OGRFieldType
        """
        return _ogr.Feature_GetFieldType(*args)

    def Reference(self):
      pass

    def Dereference(self):
      pass

    def Destroy(self):
      "Once called, self has effectively been destroyed.  Do not access. For backwards compatiblity only"
      _ogr.delete_Feature( self )
      self.thisown = 0

    def __cmp__(self, other):
        """Compares a feature to another for equality"""
        return self.Equal(other)

    def __copy__(self):
        return self.Clone()

    def __getattr__(self, name):
        """Returns the values of fields by the given name"""
        try:
            return self.GetField(name)
        except:
            raise AttributeError, name

    def GetField(self, fld_index):
        import types
        if isinstance(fld_index, types.StringType):
            fld_index = self.GetFieldIndex(fld_index)
        if (fld_index < 0) or (fld_index > self.GetFieldCount()):
            raise ValueError, "Illegal field requested in GetField()"
        if not (self.IsFieldSet(fld_index)):
            return None
        fld_type = self.GetFieldType(fld_index)
        if fld_type == OFTInteger:
            return self.GetFieldAsInteger(fld_index)
        if fld_type == OFTReal:
            return self.GetFieldAsDouble(fld_index)
        
        return self.GetFieldAsString(fld_index)
        

Feature_swigregister = _ogr.Feature_swigregister
Feature_swigregister(Feature)

class FeatureDefn(_object):
    """Proxy of C++ FeatureDefn class"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, FeatureDefn, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, FeatureDefn, name)
    __repr__ = _swig_repr
    __swig_destroy__ = _ogr.delete_FeatureDefn
    __del__ = lambda self : None;
    def __init__(self, *args, **kwargs): 
        """__init__(self, char name=None) -> FeatureDefn"""
        this = _ogr.new_FeatureDefn(*args, **kwargs)
        try: self.this.append(this)
        except: self.this = this
    def GetName(*args):
        """GetName(self) -> char"""
        return _ogr.FeatureDefn_GetName(*args)

    def GetFieldCount(*args):
        """GetFieldCount(self) -> int"""
        return _ogr.FeatureDefn_GetFieldCount(*args)

    def GetFieldDefn(*args):
        """GetFieldDefn(self, int i) -> FieldDefn"""
        return _ogr.FeatureDefn_GetFieldDefn(*args)

    def GetFieldIndex(*args):
        """GetFieldIndex(self, char name) -> int"""
        return _ogr.FeatureDefn_GetFieldIndex(*args)

    def AddFieldDefn(*args):
        """AddFieldDefn(self, FieldDefn defn)"""
        return _ogr.FeatureDefn_AddFieldDefn(*args)

    def GetGeomType(*args):
        """GetGeomType(self) -> OGRwkbGeometryType"""
        return _ogr.FeatureDefn_GetGeomType(*args)

    def SetGeomType(*args):
        """SetGeomType(self, OGRwkbGeometryType geom_type)"""
        return _ogr.FeatureDefn_SetGeomType(*args)

    def GetReferenceCount(*args):
        """GetReferenceCount(self) -> int"""
        return _ogr.FeatureDefn_GetReferenceCount(*args)

    def Destroy(self):
      "Once called, self has effectively been destroyed.  Do not access. For backwards compatiblity only"
      self.__del__()
      self.thisown = 0

FeatureDefn_swigregister = _ogr.FeatureDefn_swigregister
FeatureDefn_swigregister(FeatureDefn)

class FieldDefn(_object):
    """Proxy of C++ FieldDefn class"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, FieldDefn, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, FieldDefn, name)
    __repr__ = _swig_repr
    __swig_destroy__ = _ogr.delete_FieldDefn
    __del__ = lambda self : None;
    def __init__(self, *args, **kwargs): 
        """__init__(self, char name="unnamed", OGRFieldType field_type=OFTString) -> FieldDefn"""
        this = _ogr.new_FieldDefn(*args, **kwargs)
        try: self.this.append(this)
        except: self.this = this
    def GetName(*args):
        """GetName(self) -> char"""
        return _ogr.FieldDefn_GetName(*args)

    def GetNameRef(*args):
        """GetNameRef(self) -> char"""
        return _ogr.FieldDefn_GetNameRef(*args)

    def SetName(*args):
        """SetName(self, char name)"""
        return _ogr.FieldDefn_SetName(*args)

    def GetType(*args):
        """GetType(self) -> OGRFieldType"""
        return _ogr.FieldDefn_GetType(*args)

    def SetType(*args):
        """SetType(self, OGRFieldType type)"""
        return _ogr.FieldDefn_SetType(*args)

    def GetJustify(*args):
        """GetJustify(self) -> OGRJustification"""
        return _ogr.FieldDefn_GetJustify(*args)

    def SetJustify(*args):
        """SetJustify(self, OGRJustification justify)"""
        return _ogr.FieldDefn_SetJustify(*args)

    def GetWidth(*args):
        """GetWidth(self) -> int"""
        return _ogr.FieldDefn_GetWidth(*args)

    def SetWidth(*args):
        """SetWidth(self, int width)"""
        return _ogr.FieldDefn_SetWidth(*args)

    def GetPrecision(*args):
        """GetPrecision(self) -> int"""
        return _ogr.FieldDefn_GetPrecision(*args)

    def SetPrecision(*args):
        """SetPrecision(self, int precision)"""
        return _ogr.FieldDefn_SetPrecision(*args)

    def GetFieldTypeName(*args):
        """GetFieldTypeName(self, OGRFieldType type) -> char"""
        return _ogr.FieldDefn_GetFieldTypeName(*args)

    width = property(GetWidth, SetWidth)
    type = property(GetType, SetType)
    precision = property(GetPrecision, SetPrecision)
    name = property(GetName, SetName)
    justify = property(GetJustify, SetJustify)

    def Destroy(self):
      "Once called, self has effectively been destroyed.  Do not access. For backwards compatiblity only"
      self.__del__()
      self.thisown = 0

FieldDefn_swigregister = _ogr.FieldDefn_swigregister
FieldDefn_swigregister(FieldDefn)


def CreateGeometryFromWkb(*args, **kwargs):
  """CreateGeometryFromWkb(int len, SpatialReference reference=None) -> Geometry"""
  return _ogr.CreateGeometryFromWkb(*args, **kwargs)

def CreateGeometryFromWkt(*args, **kwargs):
  """CreateGeometryFromWkt(char val, SpatialReference reference=None) -> Geometry"""
  return _ogr.CreateGeometryFromWkt(*args, **kwargs)

def CreateGeometryFromGML(*args):
  """CreateGeometryFromGML(char input_string) -> Geometry"""
  return _ogr.CreateGeometryFromGML(*args)
class Geometry(_object):
    """Proxy of C++ Geometry class"""
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, Geometry, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, Geometry, name)
    __repr__ = _swig_repr
    __swig_destroy__ = _ogr.delete_Geometry
    __del__ = lambda self : None;
    def __init__(self, *args, **kwargs): 
        """
        __init__(self, OGRwkbGeometryType type=wkbUnknown, char wkt=0, int wkb=0, 
            char wkb_buf=0, char gml=0) -> Geometry
        """
        this = _ogr.new_Geometry(*args, **kwargs)
        try: self.this.append(this)
        except: self.this = this
    def ExportToWkt(*args):
        """ExportToWkt(self, char argout) -> OGRErr"""
        return _ogr.Geometry_ExportToWkt(*args)

    def ExportToWkb(*args, **kwargs):
        """ExportToWkb(self, int nLen, OGRwkbByteOrder byte_order=wkbXDR) -> OGRErr"""
        return _ogr.Geometry_ExportToWkb(*args, **kwargs)

    def ExportToGML(*args):
        """ExportToGML(self) -> char"""
        return _ogr.Geometry_ExportToGML(*args)

    def AddPoint(*args, **kwargs):
        """AddPoint(self, double x, double y, double z=0)"""
        return _ogr.Geometry_AddPoint(*args, **kwargs)

    def AddGeometryDirectly(*args):
        """AddGeometryDirectly(self, Geometry other) -> OGRErr"""
        return _ogr.Geometry_AddGeometryDirectly(*args)

    def AddGeometry(*args):
        """AddGeometry(self, Geometry other) -> OGRErr"""
        return _ogr.Geometry_AddGeometry(*args)

    def Clone(*args):
        """Clone(self) -> Geometry"""
        return _ogr.Geometry_Clone(*args)

    def GetGeometryType(*args):
        """GetGeometryType(self) -> OGRwkbGeometryType"""
        return _ogr.Geometry_GetGeometryType(*args)

    def GetGeometryName(*args):
        """GetGeometryName(self) -> char"""
        return _ogr.Geometry_GetGeometryName(*args)

    def GetArea(*args):
        """GetArea(self) -> double"""
        return _ogr.Geometry_GetArea(*args)

    def GetPointCount(*args):
        """GetPointCount(self) -> int"""
        return _ogr.Geometry_GetPointCount(*args)

    def GetX(*args, **kwargs):
        """GetX(self, int point=0) -> double"""
        return _ogr.Geometry_GetX(*args, **kwargs)

    def GetY(*args, **kwargs):
        """GetY(self, int point=0) -> double"""
        return _ogr.Geometry_GetY(*args, **kwargs)

    def GetZ(*args, **kwargs):
        """GetZ(self, int point=0) -> double"""
        return _ogr.Geometry_GetZ(*args, **kwargs)

    def GetGeometryCount(*args):
        """GetGeometryCount(self) -> int"""
        return _ogr.Geometry_GetGeometryCount(*args)

    def SetPoint(*args, **kwargs):
        """SetPoint(self, int point, double x, double y, double z=0)"""
        return _ogr.Geometry_SetPoint(*args, **kwargs)

    def GetGeometryRef(*args):
        """GetGeometryRef(self, int geom) -> Geometry"""
        return _ogr.Geometry_GetGeometryRef(*args)

    def GetBoundary(*args):
        """GetBoundary(self) -> Geometry"""
        return _ogr.Geometry_GetBoundary(*args)

    def ConvexHull(*args):
        """ConvexHull(self) -> Geometry"""
        return _ogr.Geometry_ConvexHull(*args)

    def Buffer(*args, **kwargs):
        """Buffer(self, double distance, int quadsecs=30) -> Geometry"""
        return _ogr.Geometry_Buffer(*args, **kwargs)

    def Intersection(*args):
        """Intersection(self, Geometry other) -> Geometry"""
        return _ogr.Geometry_Intersection(*args)

    def Union(*args):
        """Union(self, Geometry other) -> Geometry"""
        return _ogr.Geometry_Union(*args)

    def Difference(*args):
        """Difference(self, Geometry other) -> Geometry"""
        return _ogr.Geometry_Difference(*args)

    def SymmetricDifference(*args):
        """SymmetricDifference(self, Geometry other) -> Geometry"""
        return _ogr.Geometry_SymmetricDifference(*args)

    def Distance(*args):
        """Distance(self, Geometry other) -> double"""
        return _ogr.Geometry_Distance(*args)

    def Empty(*args):
        """Empty(self)"""
        return _ogr.Geometry_Empty(*args)

    def Intersect(*args):
        """Intersect(self, Geometry other) -> bool"""
        return _ogr.Geometry_Intersect(*args)

    def Equal(*args):
        """Equal(self, Geometry other) -> bool"""
        return _ogr.Geometry_Equal(*args)

    def Disjoint(*args):
        """Disjoint(self, Geometry other) -> bool"""
        return _ogr.Geometry_Disjoint(*args)

    def Touches(*args):
        """Touches(self, Geometry other) -> bool"""
        return _ogr.Geometry_Touches(*args)

    def Crosses(*args):
        """Crosses(self, Geometry other) -> bool"""
        return _ogr.Geometry_Crosses(*args)

    def Within(*args):
        """Within(self, Geometry other) -> bool"""
        return _ogr.Geometry_Within(*args)

    def Contains(*args):
        """Contains(self, Geometry other) -> bool"""
        return _ogr.Geometry_Contains(*args)

    def Overlaps(*args):
        """Overlaps(self, Geometry other) -> bool"""
        return _ogr.Geometry_Overlaps(*args)

    def TransformTo(*args):
        """TransformTo(self, SpatialReference reference) -> OGRErr"""
        return _ogr.Geometry_TransformTo(*args)

    def Transform(*args):
        """Transform(self, CoordinateTransformation trans) -> OGRErr"""
        return _ogr.Geometry_Transform(*args)

    def GetSpatialReference(*args):
        """GetSpatialReference(self) -> SpatialReference"""
        return _ogr.Geometry_GetSpatialReference(*args)

    def AssignSpatialReference(*args):
        """AssignSpatialReference(self, SpatialReference reference)"""
        return _ogr.Geometry_AssignSpatialReference(*args)

    def CloseRings(*args):
        """CloseRings(self)"""
        return _ogr.Geometry_CloseRings(*args)

    def FlattenTo2D(*args):
        """FlattenTo2D(self)"""
        return _ogr.Geometry_FlattenTo2D(*args)

    def GetEnvelope(*args):
        """GetEnvelope(self, double argout)"""
        return _ogr.Geometry_GetEnvelope(*args)

    def Centroid(*args):
        """Centroid(self) -> Geometry"""
        return _ogr.Geometry_Centroid(*args)

    def WkbSize(*args):
        """WkbSize(self) -> int"""
        return _ogr.Geometry_WkbSize(*args)

    def GetCoordinateDimension(*args):
        """GetCoordinateDimension(self) -> int"""
        return _ogr.Geometry_GetCoordinateDimension(*args)

    def SetCoordinateDimension(*args):
        """SetCoordinateDimension(self, int dimension)"""
        return _ogr.Geometry_SetCoordinateDimension(*args)

    def GetDimension(*args):
        """GetDimension(self) -> int"""
        return _ogr.Geometry_GetDimension(*args)

    def Destroy(self):
      self.__del__()
      self.thisown = 0

    def __str__(self):
      return self.ExportToWkt()

Geometry_swigregister = _ogr.Geometry_swigregister
Geometry_swigregister(Geometry)


def GetDriverCount(*args):
  """GetDriverCount() -> int"""
  return _ogr.GetDriverCount(*args)

def GetOpenDSCount(*args):
  """GetOpenDSCount() -> int"""
  return _ogr.GetOpenDSCount(*args)

def SetGenerate_DB2_V72_BYTE_ORDER(*args):
  """SetGenerate_DB2_V72_BYTE_ORDER(int bGenerate_DB2_V72_BYTE_ORDER) -> OGRErr"""
  return _ogr.SetGenerate_DB2_V72_BYTE_ORDER(*args)

def RegisterAll(*args):
  """RegisterAll()"""
  return _ogr.RegisterAll(*args)

def GetOpenDS(*args):
  """GetOpenDS(int ds_number) -> DataSource"""
  return _ogr.GetOpenDS(*args)

def Open(*args, **kwargs):
  """Open(char filename, int update=0) -> DataSource"""
  return _ogr.Open(*args, **kwargs)

def OpenShared(*args, **kwargs):
  """OpenShared(char filename, int update=0) -> DataSource"""
  return _ogr.OpenShared(*args, **kwargs)

def GetDriverByName(*args):
  """GetDriverByName(char name) -> Driver"""
  return _ogr.GetDriverByName(*args)

def GetDriver(*args):
  """GetDriver(int driver_number) -> Driver"""
  return _ogr.GetDriver(*args)


