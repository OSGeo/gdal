/*
 * $Id$
 *
 * python specific code for ogr bindings.
 */


%feature("autodoc");

#ifndef FROM_GDAL_I
%init %{

  if ( OGRGetDriverCount() == 0 ) {
    OGRRegisterAll();
  }
  
%}
#endif

/*%{
    
#if PY_MINOR_VERSION >= 4 
#include "datetime.h" 
#define USE_PYTHONDATETIME 1
#endif
%}
*/

%include "ogr_layer_docs.i"
#ifndef FROM_GDAL_I
%include "ogr_datasource_docs.i"
%include "ogr_driver_docs.i"
#endif
%include "ogr_feature_docs.i"
%include "ogr_featuredef_docs.i"
%include "ogr_fielddef_docs.i"
%include "ogr_geometry_docs.i"

%rename (GetDriverCount) OGRGetDriverCount;
%rename (GetOpenDSCount) OGRGetOpenDSCount;
%rename (SetGenerate_DB2_V72_BYTE_ORDER) OGRSetGenerate_DB2_V72_BYTE_ORDER;
%rename (RegisterAll) OGRRegisterAll();

#ifndef FROM_GDAL_I
%include "python_exceptions.i"
%include "python_strings.i"

%extend OGRDataSourceShadow {
  %pythoncode {
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
ds[0] would return the first layer on the datasource.
ds['aname'] would return the layer named "aname".
ds[0:4] would return a list of the first four layers."""
        if isinstance(value, slice):
            output = []
            for i in xrange(value.start,value.stop,value.step):
                try:
                    output.append(self.GetLayer(i))
                except OGRError: #we're done because we're off the end
                    return output
            return output
        if isinstance(value, int):
            if value > len(self)-1:
                raise IndexError
            return self.GetLayer(value)
        elif isinstance(value, str):
            return self.GetLayer(value)
        else:
            raise TypeError('Input %s is not of String or Int type' % type(value))

    def GetLayer(self,iLayer=0):
        """Return the layer given an index or a name"""
        if isinstance(iLayer, str):
            return self.GetLayerByName(str(iLayer))
        elif isinstance(iLayer, int):
            return self.GetLayerByIndex(iLayer)
        else:
            raise TypeError("Input %s is not of String or Int type" % type(iLayer))

    def DeleteLayer(self, value):
        """Deletes the layer given an index or layer name"""
        if isinstance(value, str):
            for i in range(self.GetLayerCount()):
                name = self.GetLayer(i).GetName()
                if name == value:
                    return _ogr.DataSource_DeleteLayer(self, i)
            raise ValueError("Layer %s not found to delete" % value)
        elif isinstance(value, int):
            return _ogr.DataSource_DeleteLayer(self, value)
        else:
            raise TypeError("Input %s is not of String or Int type" % type(value))
  }
}

#endif


%extend OGRLayerShadow {
  %pythoncode %{
    def Reference(self):
      "For backwards compatibility only."
      pass
  
    def Dereference(self):
      "For backwards compatibility only."
      pass

    def __len__(self):
        """Returns the number of features in the layer"""
        return self.GetFeatureCount()

    # To avoid __len__ being called when testing boolean value
    # which can have side effects (#4758)
    def __nonzero__(self):
        return True

    # For Python 3 compat
    __bool__ = __nonzero__

    def __getitem__(self, value):
        """Support list and slice -like access to the layer.
layer[0] would return the first feature on the layer.
layer[0:4] would return a list of the first four features."""
        if isinstance(value, slice):
            import sys
            output = []
            if value.stop == sys.maxint:
                #for an unending slice, sys.maxint is used
                #We need to stop before that or GDAL will write an
                ##error to stdout
                stop = len(self) - 1
            else:
                stop = value.stop
            for i in xrange(value.start,stop,value.step):
                feature = self.GetFeature(i)
                if feature:
                    output.append(feature)
                else:
                    return output
            return output
        if isinstance(value, int):
            if value > len(self)-1:
                raise IndexError
            return self.GetFeature(value)
        else:
            raise TypeError("Input %s is not of IntType or SliceType" % type(value))

    def CreateFields(self, fields):
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

    def schema(self):
        output = []
        defn = self.GetLayerDefn()
        for n in range(defn.GetFieldCount()):
            output.append(defn.GetFieldDefn(n))
        return output
    schema = property(schema)

  %}

}

%extend OGRFeatureShadow {

  %apply ( const char *utf8_path ) { (const char* value) };
  void SetFieldString(int id, const char* value) {
    OGR_F_SetFieldString(self, id, value);
  }
  %clear (const char* value );
  
  %pythoncode %{
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

    # This makes it possible to fetch fields in the form "feature.area". 
    # This has some risk of name collisions.
    def __getattr__(self, key):
        """Returns the values of fields by the given name"""
        if key == 'this':
            return self.__dict__[key]

        idx = self.GetFieldIndex(key)
        if idx < 0:
            idx = self.GetGeomFieldIndex(key)
            if idx < 0:
                raise AttributeError(key)
            else:
                return self.GetGeomFieldRef(idx)
        else:
            return self.GetField(idx)

    # This makes it possible to set fields in the form "feature.area". 
    # This has some risk of name collisions.
    def __setattr__(self, key, value):
        """Set the values of fields by the given name"""
        if key == 'this' or key == 'thisown':
            self.__dict__[key] = value
        else:
            idx = self.GetFieldIndex(key)
            if idx != -1:
                self.SetField2(idx,value)
            else:
                idx = self.GetGeomFieldIndex(key)
                if idx != -1:
                    self.SetGeomField(idx, value)
                else:
                    self.__dict__[key] = value

    # This makes it possible to fetch fields in the form "feature['area']". 
    def __getitem__(self, key):
        """Returns the values of fields by the given name / field_index"""
        if isinstance(key, str):
            fld_index = self.GetFieldIndex(key)
        if fld_index < 0:
            if isinstance(key, str):
                fld_index = self.GetGeomFieldIndex(key)
            if fld_index < 0:
                raise ValueError("Illegal field requested in GetField()")
            else:
                return self.GetGeomFieldRef(fld_index)
        else:
            return self.GetField(fld_index)

    # This makes it possible to set fields in the form "feature['area'] = 123". 
    def __setitem__(self, key, value):
        """Returns the value of a field by field name / index"""
        if isinstance(key, str):
            fld_index = self.GetFieldIndex(key)
        if fld_index < 0:
            if isinstance(key, str):
                fld_index = self.GetGeomFieldIndex(key)
            if fld_index < 0:
                raise ValueError("Illegal field requested in SetField()")
            else:
                return self.SetGeomField( fld_index, value )
        else:
            return self.SetField2( fld_index, value )

    def GetField(self, fld_index):
        if isinstance(fld_index, str):
            fld_index = self.GetFieldIndex(fld_index)
        if (fld_index < 0) or (fld_index > self.GetFieldCount()):
            raise ValueError("Illegal field requested in GetField()")
        if not (self.IsFieldSet(fld_index)):
            return None
        fld_type = self.GetFieldType(fld_index)
        if fld_type == OFTInteger:
            return self.GetFieldAsInteger(fld_index)
        if fld_type == OFTInteger64:
            return self.GetFieldAsInteger64(fld_index)
        if fld_type == OFTReal:
            return self.GetFieldAsDouble(fld_index)
        if fld_type == OFTStringList:
            return self.GetFieldAsStringList(fld_index)
        if fld_type == OFTIntegerList:
            return self.GetFieldAsIntegerList(fld_index)
        if fld_type == OFTInteger64List:
            return self.GetFieldAsInteger64List(fld_index)
        if fld_type == OFTRealList:
            return self.GetFieldAsDoubleList(fld_index)
        ## if fld_type == OFTDateTime or fld_type == OFTDate or fld_type == OFTTime:
        #     return self.GetFieldAsDate(fld_index)
        # default to returning as a string.  Should we add more types?
        try:
            return self.GetFieldAsString(fld_index)
        except:
            # For Python3 on non-UTF8 strings
            return self.GetFieldAsBinary(fld_index)

    # With several override, SWIG cannot dispatch automatically unicode strings
    # to the right implementation, so we have to do it at hand
    def SetField(self, *args):
        """
        SetField(self, int id, char value)
        SetField(self, char name, char value)
        SetField(self, int id, int value)
        SetField(self, char name, int value)
        SetField(self, int id, double value)
        SetField(self, char name, double value)
        SetField(self, int id, int year, int month, int day, int hour, int minute, 
            int second, int tzflag)
        SetField(self, char name, int year, int month, int day, int hour, 
            int minute, int second, int tzflag)
        """

        if len(args) == 2 and type(args[1]) == type(1):
            fld_index = args[0]
            if isinstance(fld_index, str):
                fld_index = self.GetFieldIndex(fld_index)
            return _ogr.Feature_SetFieldInteger64(self, fld_index, args[1])


        if len(args) == 2 and str(type(args[1])) == "<type 'unicode'>":
            fld_index = args[0]
            if isinstance(fld_index, str):
                fld_index = self.GetFieldIndex(fld_index)
            return _ogr.Feature_SetFieldString(self, fld_index, args[1])

        return _ogr.Feature_SetField(self, *args)

    def SetField2(self, fld_index, value):
        if isinstance(fld_index, str):
            fld_index = self.GetFieldIndex(fld_index)
        if (fld_index < 0) or (fld_index > self.GetFieldCount()):
            raise ValueError("Illegal field requested in SetField2()")

        if value is None:
            self.UnsetField( fld_index )
            return

        if isinstance(value,list):
            if len(value) == 0:
                self.UnsetField( fld_index )
                return
            if isinstance(value[0],int):
                self.SetFieldInteger64List(fld_index,value)
                return
            elif isinstance(value[0],float):
                self.SetFieldDoubleList(fld_index,value)
                return
            elif isinstance(value[0],str):
                self.SetFieldStringList(fld_index,value)
                return
            else:
                raise TypeError( 'Unsupported type of list in SetField2(). Type of element is %s' % str(type(value[0])) )

        try:
            self.SetField( fld_index, value )
        except:
            self.SetField( fld_index, str(value) )
        return

    def keys(self):
        names = []
        for i in range(self.GetFieldCount()):
            fieldname = self.GetFieldDefnRef(i).GetName()
            names.append(fieldname)
        return names
    
    def items(self):
        keys = self.keys()
        output = {}
        for key in keys:
            output[key] = self.GetField(key)
        return output
    def geometry(self):
        return self.GetGeometryRef()

    def ExportToJson(self, as_object = False, options = None):
        """Exports a GeoJSON object which represents the Feature. The
           as_object parameter determines whether the returned value 
           should be a Python object instead of a string. Defaults to False.
           The options parameter is passed to Geometry.ExportToJson()"""

        try:
            import simplejson
        except ImportError:
            try:
                import json as simplejson
            except ImportError:
                raise ImportError("Unable to import simplejson or json, needed for ExportToJson.")

        geom = self.GetGeometryRef()
        if geom is not None:
            if options is None:
                options = []
            geom_json_string = geom.ExportToJson(options = options)
            geom_json_object = simplejson.loads(geom_json_string)
        else:
            geom_json_object = None

        output = {'type':'Feature',
                   'geometry': geom_json_object,
                   'properties': {}
                  } 
        
        fid = self.GetFID()
        if fid != NullFID:
            output['id'] = fid
            
        for key in self.keys():
            output['properties'][key] = self.GetField(key)
        
        if not as_object:
            output = simplejson.dumps(output)

        return output


%}

}

%extend OGRGeometryShadow {
%pythoncode %{
  def Destroy(self):
    self.__swig_destroy__(self) 
    self.__del__()
    self.thisown = 0

  def __str__(self):
    return self.ExportToWkt()
    

  def __reduce__(self):
    return (self.__class__, (), self.ExportToWkb())
 	
  def __setstate__(self, state):
      result = CreateGeometryFromWkb(state)
      self.this = result.this
        
  def __iter__(self):
      self.iter_subgeom = 0
      return self
      
  def next(self):
      if self.iter_subgeom < self.GetGeometryCount():
          subgeom = self.GetGeometryRef(self.iter_subgeom)
          self.iter_subgeom += 1
          return subgeom
      else:
          raise StopIteration
%}
}


%extend OGRFieldDefnShadow {
%pythoncode {
    width = property(GetWidth, SetWidth)
    type = property(GetType, SetType)
    precision = property(GetPrecision, SetPrecision)
    name = property(GetName, SetName)
    justify = property(GetJustify, SetJustify)
}
}

%extend OGRGeomFieldDefnShadow {
%pythoncode {
    type = property(GetType, SetType)
    name = property(GetName, SetName)
    srs = property(GetSpatialRef, SetSpatialRef)
}
}

%extend OGRFeatureDefnShadow {
%pythoncode {
  def Destroy(self):
    "Once called, self has effectively been destroyed.  Do not access. For backwards compatiblity only"
    _ogr.delete_FeatureDefn( self )
    self.thisown = 0

}
}

%extend OGRFieldDefnShadow {
%pythoncode %{
  def Destroy(self):
    "Once called, self has effectively been destroyed.  Do not access. For backwards compatiblity only"
    _ogr.delete_FieldDefn( self )
    self.thisown = 0
%}
}

%import typemaps_python.i

#ifndef FROM_GDAL_I
%include "callback.i"


%extend GDALMajorObjectShadow {
%pythoncode %{
  def GetMetadata( self, domain = '' ):
    if domain[:4] == 'xml:':
      return self.GetMetadata_List( domain )
    return self.GetMetadata_Dict( domain )
%}
}
#endif


%pythoncode %{

# Backup original dictionnary before doing anything else
_initial_dict = globals().copy()

@property
def wkb25Bit(module):
    import warnings
    warnings.warn("ogr.wkb25DBit deprecated: use ogr.GT_Flatten(), ogr.GT_HasZ() or ogr.GT_SetZ() instead", DeprecationWarning)
    return module._initial_dict['wkb25DBit']

@property
def wkb25DBit(module):
    import warnings
    warnings.warn("ogr.wkb25DBit deprecated: use ogr.GT_Flatten(), ogr.GT_HasZ() or ogr.GT_SetZ() instead", DeprecationWarning)
    return module._initial_dict['wkb25DBit']

# Inspired from http://www.dr-josiah.com/2013/12/properties-on-python-modules.html
class _Module(object):
    def __init__(self):
        self.__dict__ = globals()
        self._initial_dict = _initial_dict

        # Transfer properties from the object to the Class
        for k, v in list(self.__dict__.items()):
            if isinstance(v, property):
                setattr(self.__class__, k, v)
                #del self.__dict__[k]

        # Replace original module by our object
        import sys
        self._original_module = sys.modules[self.__name__]
        sys.modules[self.__name__] = self

# Custom help() replacement to display the help of the original module
# instead of the one of our instance object
class _MyHelper(object):

    def __init__(self, module):
        self.module = module
        self.original_help = help

        # Replace builtin help by ours
        try:
            import __builtin__ as builtins # Python 2
        except ImportError:
            import builtins # Python 3
        builtins.help = self

    def __repr__(self):
        return self.original_help.__repr__()

    def __call__(self, *args, **kwds):

        if args == (self.module,):
            import sys

            # Restore original module before calling help() otherwise
            # we don't get methods or classes mentionned
            sys.modules[self.module.__name__] = self.module._original_module

            ret = self.original_help(self.module._original_module, **kwds)

            # Reinstall our module
            sys.modules[self.module.__name__] = self.module

            return ret
        elif args == (self,):
            return self.original_help(self.original_help, **kwds)
        else:
            return self.original_help(*args, **kwds)

_MyHelper(_Module())
del _MyHelper
del _Module

%}
