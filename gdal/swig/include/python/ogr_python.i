/*
 * $Id$
 *
 * python specific code for ogr bindings.
 */


%feature("autodoc");

%init %{

  if ( OGRGetDriverCount() == 0 ) {
    OGRRegisterAll();
  }
  
%}

/*%{
    
#if PY_MINOR_VERSION >= 4 
#include "datetime.h" 
#define USE_PYTHONDATETIME 1
#endif
%}
*/

%include "ogr_layer_docs.i"
%include "ogr_datasource_docs.i"
%include "ogr_driver_docs.i"
%include "ogr_feature_docs.i"
%include "ogr_featuredef_docs.i"
%include "ogr_fielddef_docs.i"
%include "ogr_geometry_docs.i"

%rename (GetDriverCount) OGRGetDriverCount;
%rename (GetOpenDSCount) OGRGetOpenDSCount;
%rename (SetGenerate_DB2_V72_BYTE_ORDER) OGRSetGenerate_DB2_V72_BYTE_ORDER;
%rename (RegisterAll) OGRRegisterAll();

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
        import types
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
        import types
        if isinstance(iLayer, str):
            return self.GetLayerByName(str(iLayer))
        elif isinstance(iLayer, int):
            return self.GetLayerByIndex(iLayer)
        else:
            raise TypeError("Input %s is not of String or Int type" % type(iLayer))

    def DeleteLayer(self, value):
        """Deletes the layer given an index or layer name"""
        import types
        if isinstance(value, str):
            for i in range(self.GetLayerCount()):
                name = self.GetLayer(i).GetName()
                if name == value:
                    return _ogr.DataSource_DeleteLayer(self, i)
            raise ValueError("Layer %s not found to delete" % value)
        elif isinstance(value, int):
            return _ogr.DataSource_DeleteLayer(self, value)
        else:
            raise TypeError("Input %s is not of String or Int type" % type(iLayer))
  }
}

%extend OGRLayerShadow {
  %pythoncode {
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
layer[0] would return the first feature on the layer.
layer[0:4] would return a list of the first four features."""
        import types
        if isinstance(value, slice):
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

    def schema(self):
        output = []
        defn = self.GetLayerDefn()
        for n in range(defn.GetFieldCount()):
            output.append(defn.GetFieldDefn(n))
        return output
    schema = property(schema)

  }

}

%extend OGRFeatureShadow {
  %pythoncode {
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
            raise AttributeError(name)

    def GetField(self, fld_index):
        import types
        if isinstance(fld_index, str):
            fld_index = self.GetFieldIndex(fld_index)
        if (fld_index < 0) or (fld_index > self.GetFieldCount()):
            raise ValueError("Illegal field requested in GetField()")
        if not (self.IsFieldSet(fld_index)):
            return None
        fld_type = self.GetFieldType(fld_index)
        if fld_type == OFTInteger:
            return self.GetFieldAsInteger(fld_index)
        if fld_type == OFTReal:
            return self.GetFieldAsDouble(fld_index)
        ## if fld_type == OFTDateTime or fld_type == OFTDate or fld_type == OFTTime:
        #     return self.GetFieldAsDate(fld_index)
        # default to returning as a string.  Should we add more types?
        return self.GetFieldAsString(fld_index)
    
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

    def ExportToJson(self, as_object = False):
        """Exports a GeoJSON object which represents the Feature. The
           as_object parameter determines whether the returned value 
           should be a Python object instead of a string. Defaults to False."""
        output = {'type':'Feature',
                   'geometry': self.GetGeometryRef().ExportToJson(as_object=True),
                   'properties': {}
                  } 
        
        fid = self.GetFID()
        if fid:
            output['id'] = fid
            
        for key in self.keys():
            output['properties'][key] = self.GetField(key)
        
        if not as_object:
            try:
                import simplejson
            except ImportError:
                raise ImportError("Unable to import simplejson, needed for ExportToJson.")
            output = simplejson.dumps(output)
        
        return output


}

}

%extend OGRGeometryShadow {
%pythoncode {
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
}
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

%extend OGRFeatureDefnShadow {
%pythoncode {
  def Destroy(self):
    "Once called, self has effectively been destroyed.  Do not access. For backwards compatiblity only"
    _ogr.delete_FeatureDefn( self )
    self.thisown = 0

}
}

%extend OGRFieldDefnShadow {
%pythoncode {
  def Destroy(self):
    "Once called, self has effectively been destroyed.  Do not access. For backwards compatiblity only"
    _ogr.delete_FieldDefn( self )
    self.thisown = 0
}
}

%import typemaps_python.i
