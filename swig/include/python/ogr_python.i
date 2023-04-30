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
  // Will be turned on for GDAL 4.0
  // UseExceptions();

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
%{
#define MODULE_NAME           "ogr"
%}

%include "python_exceptions.i"
%include "python_strings.i"

// Start: to be removed in GDAL 4.0

// Issue a FutureWarning in a number of functions and methods that will
// be impacted when exceptions are enabled by default

%pythoncode %{

hasWarnedAboutUserHasNotSpecifiedIfUsingExceptions = False

def _WarnIfUserHasNotSpecifiedIfUsingExceptions():
    from . import gdal
    if not hasattr(gdal, "hasWarnedAboutUserHasNotSpecifiedIfUsingExceptions") and not _UserHasSpecifiedIfUsingExceptions():
        gdal.hasWarnedAboutUserHasNotSpecifiedIfUsingExceptions = True
        import warnings
        warnings.warn(
            "Neither ogr.UseExceptions() nor ogr.DontUseExceptions() has been explicitly called. " +
            "In GDAL 4.0, exceptions will be enabled by default.", FutureWarning)
%}

%pythonprepend Open %{
    _WarnIfUserHasNotSpecifiedIfUsingExceptions()
%}

// End: to be removed in GDAL 4.0

%extend OGRDataSourceShadow {
  %pythoncode {
    def Destroy(self):
      "Once called, self has effectively been destroyed.  Do not access. For backwards compatibility only"
      _ogr.delete_DataSource(self)
      self.thisown = 0

    def Release(self):
      "Once called, self has effectively been destroyed.  Do not access. For backwards compatibility only"
      _ogr.delete_DataSource(self)
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
            step = value.step if value.step else 1
            for i in range(value.start, value.stop, step):
                lyr = self.GetLayer(i)
                if lyr is None:
                    return output
                output.append(lyr)
            return output
        if isinstance(value, int):
            if value > len(self) - 1:
                raise IndexError
            return self.GetLayer(value)
        elif isinstance(value, str):
            return self.GetLayer(value)
        else:
            raise TypeError('Input %s is not of String or Int type' % type(value))

    def GetLayer(self, iLayer=0):
        """Return the layer given an index or a name"""

        _WarnIfUserHasNotSpecifiedIfUsingExceptions()

        if isinstance(iLayer, str):
            return self.GetLayerByName(str(iLayer))
        elif isinstance(iLayer, int):
            return self.GetLayerByIndex(iLayer)
        else:
            raise TypeError("Input %s is not of String or Int type" % type(iLayer))
  }

%feature("shadow") DeleteLayer %{
    def DeleteLayer(self, value) -> "OGRErr":
        """
        DeleteLayer(DataSource self, value) -> OGRErr

        Delete the indicated layer from the datasource.

        For more details: :c:func:`OGR_DS_DeleteLayer`

        Parameters
        -----------
        value: str | int
            index or name of the layer to delete.

        Returns
        -------
        int:
            :py:const:`osgeo.ogr.OGRERR_NONE` on success, or :py:const:`osgeo.ogr.OGRERR_UNSUPPORTED_OPERATION` if deleting
            layers is not supported for this datasource.
        """

        if isinstance(value, str):
            for i in range(self.GetLayerCount()):
                name = self.GetLayer(i).GetName()
                if name == value:
                    return $action(self, i)
            raise ValueError("Layer %s not found to delete" % value)
        elif isinstance(value, int):
            return $action(self, value)
        else:
            raise TypeError("Input %s is not of String or Int type" % type(value))
%}

%feature("shadow") ExecuteSQL %{
def ExecuteSQL(self, statement, spatialFilter=None, dialect="", keep_ref_on_ds=False):
    """ExecuteSQL(self, statement, spatialFilter: ogr.Geometry = None, dialect: Optional[str] = "", keep_ref_on_ds=False) -> ogr.Layer

    Execute a SQL statement against the dataset

    The result of a SQL query is:
      - None (or an exception if exceptions are enabled) for statements
        that are in error
      - or None for statements that have no results set,
      - or a ogr.Layer handle representing a results set from the query.

    Note that this ogr.Layer is in addition to the layers in the data store
    and must be released with ReleaseResultSet() before the data source is closed
    (destroyed).

    Starting with GDAL 3.7, this method can also be used as a context manager,
    as a convenient way of automatically releasing the returned result layer.

    For more information on the SQL dialect supported internally by OGR
    review the OGR SQL document (:ref:`ogr_sql_sqlite_dialect`)
    Some drivers (i.e. Oracle and PostGIS) pass the SQL directly through to the
    underlying RDBMS.

    The SQLITE dialect can also be used (:ref:`sql_sqlite_dialect`)

    Parameters
    ----------
    statement:
        the SQL statement to execute (e.g "SELECT * FROM layer")
    spatialFilter:
        a geometry which represents a spatial filter. Can be None
    dialect:
        allows control of the statement dialect. If set to None or empty string,
        the OGR SQL engine will be used, except for RDBMS drivers that will
        use their dedicated SQL engine, unless OGRSQL is explicitly passed as
        the dialect. The SQLITE dialect can also be used.
    keep_ref_on_ds:
        whether the returned layer should keep a (strong) reference on
        the current dataset. Cf example 2 for a use case.

    Returns
    -------
    ogr.Layer:
        a ogr.Layer containing the results of the query, that will be
        automatically released when the context manager goes out of scope.

    Examples
    --------
    1. Use as a context manager:

    >>> with ds.ExecuteSQL("SELECT * FROM layer") as lyr:
    ...     print(lyr.GetFeatureCount())

    2. Use keep_ref_on_ds=True to return an object that keeps a reference to its dataset:

    >>> def get_sql_lyr():
    ...     return gdal.OpenEx("test.shp").ExecuteSQL("SELECT * FROM test", keep_ref_on_ds=True)
    ...
    ... with get_sql_lyr() as lyr:
    ...     print(lyr.GetFeatureCount())
    """

    sql_lyr = $action(self, statement, spatialFilter, dialect)
    if sql_lyr:
        import weakref
        sql_lyr._dataset_weak_ref = weakref.ref(self)
        if keep_ref_on_ds:
            sql_lyr._dataset_strong_ref = self
    return sql_lyr
%}


%feature("shadow") ReleaseResultSet %{
def ReleaseResultSet(self, sql_lyr):
    """ReleaseResultSet(self, sql_lyr: ogr.Layer)

    Release ogr.Layer returned by ExecuteSQL() (when not called as an execution manager)

    The sql_lyr object is invalidated after this call.

    Parameters
    ----------
    sql_lyr:
        ogr.Layer got with ExecuteSQL()
    """

    $action(self, sql_lyr)
    # Invalidates the layer
    if sql_lyr:
        sql_lyr.thisown = None
        sql_lyr.this = None
%}

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
            if value.stop == sys.maxsize:
                #for an unending slice, sys.maxsize is used
                #We need to stop before that or GDAL will write an
                ##error to stdout
                stop = len(self) - 1
            else:
                stop = value.stop
            for i in range(value.start, stop, value.step):
                feature = self.GetFeature(i)
                if feature:
                    output.append(feature)
                else:
                    return output
            return output
        if isinstance(value, int):
            if value > len(self) - 1:
                raise IndexError
            return self.GetFeature(value)
        else:
            raise TypeError("Input %s is not of IntType or SliceType" % type(value))

    def CreateFields(self, fields):
        """Create a list of fields on the Layer"""
        for i in fields:
            self.CreateField(i)

    def __enter__(self):
        """Method called when using Dataset.ExecuteSQL() as a context manager"""
        if hasattr(self, "_dataset_weak_ref"):
            self._dataset_strong_ref = self._dataset_weak_ref()
            assert self._dataset_strong_ref
            del self._dataset_weak_ref
            return self
        raise Exception("__enter__() called in unexpected situation")

    def __exit__(self, *args):
        """Method called when using Dataset.ExecuteSQL() as a context manager"""
        if hasattr(self, "_dataset_strong_ref"):
            self._dataset_strong_ref.ReleaseResultSet(self)
            del self._dataset_strong_ref

    def __iter__(self):
        self.ResetReading()
        while True:
            feature = self.GetNextFeature()
            if not feature:
                break
            yield feature

    def schema(self):
        output = []
        defn = self.GetLayerDefn()
        for n in range(defn.GetFieldCount()):
            output.append(defn.GetFieldDefn(n))
        return output
    schema = property(schema)


    def GetArrowStreamAsPyArrow(self, options = []):
        """ Return an ArrowStream as PyArrow Schema and Array objects """

        import pyarrow as pa

        class Stream:
            def __init__(self, stream):
                self.stream = stream
                self.end_of_stream = False

            def schema(self):
                """ Return the schema as a PyArrow DataType """

                schema = self.stream.GetSchema()
                if schema is None:
                    raise Exception("cannot get schema")
                return pa.DataType._import_from_c(schema._getPtr())

            schema = property(schema)

            def __enter__(self):
                return self

            def __exit__(self, type, value, tb):
                self.end_of_stream = True
                self.stream = None

            def GetNextRecordBatch(self):
                """ Return the next RecordBatch as a PyArrow StructArray, or None at end of iteration """

                array = self.stream.GetNextRecordBatch()
                if array is None:
                    return None
                return pa.Array._import_from_c(array._getPtr(), self.schema)

            def __iter__(self):
                """ Return an iterator over record batches as a PyArrow StructArray """
                if self.end_of_stream:
                    raise Exception("Stream has already been iterated over")

                while True:
                    batch = self.GetNextRecordBatch()
                    if not batch:
                        break
                    yield batch
                self.end_of_stream = True
                self.stream = None

        stream = self.GetArrowStream(options)
        if not stream:
            raise Exception("GetArrowStream() failed")
        return Stream(stream)


    def GetArrowStreamAsNumPy(self, options = []):
        """ Return an ArrowStream as NumPy Array objects.
            A specific option to this method is USE_MASKED_ARRAYS=YES/NO (default is YES).
        """

        from osgeo import gdal_array

        class Stream:
            def __init__(self, stream, use_masked_arrays):
                self.stream = stream
                self.schema = stream.GetSchema()
                self.end_of_stream = False
                self.use_masked_arrays = use_masked_arrays

            def __enter__(self):
                return self

            def __exit__(self, type, value, tb):
                self.end_of_stream = True
                self.schema = None
                self.stream = None

            def GetNextRecordBatch(self):
                """ Return the next RecordBatch as a dictionary of Numpy arrays, or None at end of iteration """

                array = self.stream.GetNextRecordBatch()
                if array is None:
                    return None

                ret = gdal_array._RecordBatchAsNumpy(array._getPtr(),
                                                     self.schema._getPtr(),
                                                     array)
                if ret is None:
                    gdal_array._RaiseException()
                    return ret
                for key, val in ret.items():
                    if isinstance(val, dict):
                        if self.use_masked_arrays:
                            import numpy.ma as ma
                            ret[key] = ma.masked_array(val["data"], val["mask"])
                        else:
                            ret[key] = val["data"]
                return ret

            def __iter__(self):
                """ Return an iterator over record batches as a dictionary of Numpy arrays """

                if self.end_of_stream:
                    raise Exception("Stream has already been iterated over")

                try:
                    while True:
                        batch = self.GetNextRecordBatch()
                        if not batch:
                            break
                        yield batch
                finally:
                    self.end_of_stream = True
                    self.stream = None

        stream = self.GetArrowStream(options)
        if not stream:
            raise Exception("GetArrowStream() failed")

        use_masked_arrays = True
        for opt in options:
            opt = opt.upper()
            if opt.startswith('USE_MASKED_ARRAYS='):
                use_masked_arrays = opt[len('USE_MASKED_ARRAYS='):] in ('YES', 'TRUE', 'ON', '1')

        return Stream(stream, use_masked_arrays)

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
      "Once called, self has effectively been destroyed.  Do not access. For backwards compatibility only"
      _ogr.delete_Feature(self)
      self.thisown = 0

    def __cmp__(self, other):
        """Compares a feature to another for equality"""
        return self.Equal(other)

    def __copy__(self):
        return self.Clone()

    def _getfieldindex(self, fieldname):
        case_insensitive_idx = -1
        fdefn = _ogr.Feature_GetDefnRef(self)
        for i in range(fdefn.GetFieldCount()):
            name = fdefn.GetFieldDefn(i).GetName()
            if name == fieldname:
                return i
            elif case_insensitive_idx < 0 and name.lower() == fieldname.lower():
                case_insensitive_idx = i
        return case_insensitive_idx

    # This makes it possible to fetch fields in the form "feature.area".
    # This has some risk of name collisions.
    def __getattr__(self, key):
        """Returns the values of fields by the given name"""
        if key == 'this':
            return self.__dict__[key]

        idx = self._getfieldindex(key)
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
            idx = self._getfieldindex(key)
            if idx != -1:
                self._SetField2(idx, value)
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
            fld_index = self._getfieldindex(key)
        else:
            fld_index = key
            if key == self.GetFieldCount():
                raise IndexError
        if fld_index < 0:
            if isinstance(key, str):
                fld_index = self.GetGeomFieldIndex(key)
            if fld_index < 0:
                raise KeyError("Illegal field requested in GetField()")
            else:
                return self.GetGeomFieldRef(fld_index)
        else:
            return self.GetField(fld_index)

    # This makes it possible to set fields in the form "feature['area'] = 123".
    def __setitem__(self, key, value):
        """Returns the value of a field by field name / index"""
        if isinstance(key, str):
            fld_index = self._getfieldindex(key)
        else:
            fld_index = key
            if key == self.GetFieldCount():
                raise IndexError
        if fld_index < 0:
            if isinstance(key, str):
                fld_index = self.GetGeomFieldIndex(key)
            if fld_index < 0:
                raise KeyError("Illegal field requested in SetField()")
            else:
                return self.SetGeomField(fld_index, value)
        else:
            return self._SetField2(fld_index, value)

    def GetField(self, fld_index):
        if isinstance(fld_index, str):
            fld_index = self._getfieldindex(fld_index)
        if (fld_index < 0) or (fld_index > self.GetFieldCount()):
            raise KeyError("Illegal field requested in GetField()")
        if not (self.IsFieldSet(fld_index)) or self.IsFieldNull(fld_index):
            return None
        fld_type = self.GetFieldType(fld_index)
        if fld_type == OFTInteger:
            if self.GetFieldDefnRef(fld_index).GetSubType() == OFSTBoolean:
                return bool(self.GetFieldAsInteger(fld_index))
            return self.GetFieldAsInteger(fld_index)
        if fld_type == OFTInteger64:
            return self.GetFieldAsInteger64(fld_index)
        if fld_type == OFTReal:
            return self.GetFieldAsDouble(fld_index)
        if fld_type == OFTStringList:
            return self.GetFieldAsStringList(fld_index)
        if fld_type == OFTIntegerList:
            ret = self.GetFieldAsIntegerList(fld_index)
            if self.GetFieldDefnRef(fld_index).GetSubType() == OFSTBoolean:
                 ret = [bool(x) for x in ret]
            return ret
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

    def _SetField2(self, fld_index, value):
        if isinstance(fld_index, str):
            fld_index = self._getfieldindex(fld_index)
        if (fld_index < 0) or (fld_index > self.GetFieldCount()):
            raise KeyError("Illegal field requested in _SetField2()")

        if value is None:
            self.SetFieldNull(fld_index)
            return

        if isinstance(value, list):
            if not value:
                self.SetFieldNull(fld_index)
                return
            if isinstance(value[0], type(1)) or isinstance(value[0], type(12345678901234)):
                self.SetFieldInteger64List(fld_index, value)
                return
            elif isinstance(value[0], float):
                self.SetFieldDoubleList(fld_index, value)
                return
            elif isinstance(value[0], str):
                self.SetFieldStringList(fld_index, value)
                return
            else:
                raise TypeError('Unsupported type of list in _SetField2(). Type of element is %s' % str(type(value[0])))

        try:
            self.SetField(fld_index, value)
        except:
            self.SetField(fld_index, str(value))
        return

    def keys(self):
        """Return the list of field names (of the layer definition)"""
        names = []
        for i in range(self.GetFieldCount()):
            fieldname = self.GetFieldDefnRef(i).GetName()
            names.append(fieldname)
        return names

    def items(self):
        """Return a dictionary with the field names as key, and their value in the feature"""
        keys = self.keys()
        output = {}
        for key in keys:
            output[key] = self.GetField(key)
        return output

    def geometry(self):
        """ Return the feature geometry

            The lifetime of the returned geometry is bound to the one of its belonging
            feature.

            For more details: :cpp:func:`OGR_F_GetGeometryRef`

            The GetGeometryRef() method is also available as an alias of geometry()

            Returns
            --------
            Geometry:
                the geometry, or None.
        """

        return self.GetGeometryRef()

    def ExportToJson(self, as_object=False, options=None):
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
            geom_json_string = geom.ExportToJson(options=options)
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
            fld_defn = self.GetFieldDefnRef(self.GetFieldIndex(key))
            if fld_defn.GetType() == _ogr.OFTInteger and fld_defn.GetSubType() == _ogr.OFSTBoolean:
                output['properties'][key] = bool(self.GetField(key))
            else:
                output['properties'][key] = self.GetField(key)

        if not as_object:
            output = simplejson.dumps(output)

        return output


%}

%feature("shadow") SetField %{
    # With several override, SWIG cannot dispatch automatically unicode strings
    # to the right implementation, so we have to do it at hand
    def SetField(self, *args) -> "OGRErr":
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

        if len(args) == 2 and args[1] is None:
            return _ogr.Feature_SetFieldNull(self, args[0])

        if len(args) == 2 and (type(args[1]) == type(1) or type(args[1]) == type(12345678901234)):
            fld_index = args[0]
            if isinstance(fld_index, str):
                fld_index = self._getfieldindex(fld_index)
            return _ogr.Feature_SetFieldInteger64(self, fld_index, args[1])


        if len(args) == 2 and isinstance(args[1], str):
            fld_index = args[0]
            if isinstance(fld_index, str):
                fld_index = self._getfieldindex(fld_index)
            return _ogr.Feature_SetFieldString(self, fld_index, args[1])

        return $action(self, *args)
%}

}

%extend OGRGeometryShadow {
%pythoncode %{
  def Destroy(self):
    self.__swig_destroy__(self)
    self.thisown = 0

  def __str__(self):
    return self.ExportToIsoWkt()

  def __copy__(self):
    return self.Clone()

  def __deepcopy__(self, memo):
    g = self.Clone()
    srs = self.GetSpatialReference()
    if srs:
        g.AssignSpatialReference(srs.Clone())
    return g

  def __reduce__(self):
    return (self.__class__, (), self.ExportToWkb())

  def __setstate__(self, state):
      result = CreateGeometryFromWkb(state)
      self.this = result.this

  def __iter__(self):
      for i in range(self.GetGeometryCount()):
          yield self.GetGeometryRef(i)

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
    "Once called, self has effectively been destroyed.  Do not access. For backwards compatibility only"
    _ogr.delete_FeatureDefn(self)
    self.thisown = 0

}
}

%extend OGRFieldDefnShadow {
%pythoncode %{
  def Destroy(self):
    "Once called, self has effectively been destroyed.  Do not access. For backwards compatibility only"
    _ogr.delete_FieldDefn(self)
    self.thisown = 0
%}
}

%import typemaps_python.i

#ifndef FROM_GDAL_I
%include "callback.i"


%extend GDALMajorObjectShadow {
%pythoncode %{
  def GetMetadata(self, domain=''):
    if domain[:4] == 'xml:':
      return self.GetMetadata_List(domain)
    return self.GetMetadata_Dict(domain)
%}
}
#endif
