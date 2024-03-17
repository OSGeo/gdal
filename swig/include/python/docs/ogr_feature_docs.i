%feature("docstring")  OGRFeatureShadow "
Python proxy of an :cpp:class:`OGRFeature`.
";

%extend OGRFeatureShadow {
%feature("docstring")  OGRFeatureShadow "

Parameters
-----------
feature_def:
    :py:class:`FeatureDefn` to which the feature will adhere.
";

%feature("docstring")  Clone "
Duplicate a Feature.
See :cpp:func:`OGRFeature::Clone`.

Returns
--------
Feature
";

%feature("docstring")  DumpReadable "

Print this feature in a human readable form.

This dumps the attributes and geometry. It doesn't include
definition information other than field types and names nor does it
report the geometry spatial reference system.

See :cpp:func:`OGRFeature::DumpReadable`.

Examples
--------
>>> with gdal.OpenEx('data/poly.shp') as ds:
...     lyr = ds.GetLayer(0)
...     feature = lyr.GetNextFeature()
...     feature.DumpReadable()
...
OGRFeature(poly):0
  AREA (Real) = 215229.266
  EAS_ID (Integer64) = 168
  PRFEDEA (String) = 35043411
  POLYGON ((479819.84375 4765180.5,479690.1875 4765259.5,479647.0 4765369.5,479730.375 4765400.5,480039.03125 4765539.5,480035.34375 4765558.5,480159.78125 4765610.5,480202.28125 4765482.0,480365.0 4765015.5,480389.6875 4764950.0,480133.96875 4764856.5,480080.28125 4764979.5,480082.96875 4765049.5,480088.8125 4765139.5,480059.90625 4765239.5,480019.71875 4765319.5,479980.21875 4765409.5,479909.875 4765370.0,479859.875 4765270.0,479819.84375 4765180.5))
";

%feature("docstring")  DumpReadableAsString "

Return feature information in a human-readable form.
Returns the text printed by :py:func:`Feature.DumpReadable`.

Returns
-------
str
";

%feature("docstring")  Equal "

Test if two features are the same.

Two features are considered equal if they reference the
same :py:class:`FeatureDefn`, have the same field values, and the same geometry
(as tested by :py:func:`Geometry.Equal`) as well as the same feature id.

See :cpp:func:`OGRFeature::Equal`.

Parameters
-----------
feature : Feature
    feature to test this one against

Returns
--------
bool
";

%feature("docstring")  FillUnsetWithDefault "

Fill unset fields with default values that might be defined.

See :cpp:func:`OGRFeature::FillUnsetWithDefault`.

Parameters
-----------
bNotNullableOnly : bool
    if we should fill only unset fields with a not-null
    constraint.
options : dict
    unused currently.
";

%feature("docstring")  GetDefnRef "

Fetch the :py:class:`FeatureDefn` associated with this Feature.

See :cpp:func:`OGRFeature::GetDefnRef()`.

Returns
--------
FeatureDefn
";

%feature("docstring")  GetFID "

Get feature identifier.
See :cpp:func:`OGRFeature::GetFID`

Returns
-------
int:
    feature id or :py:const:`NullFID` if none has been assigned.
";

// GetField documented inline

%feature("docstring")  GetFieldAsBinary "

Fetch field value as binary.

This method only works for :py:const:`OFTBinary` and :py:const:`OFTString` fields.

See :cpp:func:`OGRFeature::GetFieldAsBinary`.

Parameters
-----------
fld_index : int / str
    Field name or 0-based numeric index. For repeated
    access, use of the numeric index avoids a lookup
    step.

Returns
--------
bytearray
";

%feature("docstring")  GetFieldAsDateTime "

Fetch field value as date and time.

Currently this method only works for :py:const:`OFTDate`, :py:const:`OFTTime`
and :py:const:`OFTDateTime` fields.

See :cpp:func:`OGRFeature::GetFieldAsDateTime`.

Parameters
-----------
fld_index : int / str
    Field name or 0-based numeric index. For repeated
    access, use of the numeric index avoids a lookup
    step.

Returns
--------
list
    list containing [ year, month, day, hour, minute, second, timezone flag ]

Examples
--------
>>> from datetime import datetime
>>> from zoneinfo import ZoneInfo
>>> defn = ogr.FeatureDefn()
>>> defn.AddFieldDefn(ogr.FieldDefn('unknown', ogr.OFTDateTime))
>>> defn.AddFieldDefn(ogr.FieldDefn('local', ogr.OFTDateTime))
>>> defn.AddFieldDefn(ogr.FieldDefn('utc', ogr.OFTDateTime))
>>> feature = ogr.Feature(defn)
>>> feature['unknown'] = datetime.now()
>>> feature['local'] = datetime.now(ZoneInfo('Canada/Eastern'))
>>> feature['utc'] = datetime.now(ZoneInfo('UTC'))
>>> feature.GetFieldAsDateTime('unknown')
[2024, 3, 15, 20, 34, 52.594173431396484, 0]
>>> feature.GetFieldAsDateTime('local')
[2024, 3, 15, 20, 34, 52.59502410888672, 84]
>>> feature.GetFieldAsDateTime('utc')
[2024, 3, 16, 0, 34, 52.59580993652344, 100]

See Also
--------
:py:func:`Feature.GetFieldAsISO8601DateTime`
";

%feature("docstring")  GetFieldAsDouble "
Fetch field value as a double.

:py:const:`OFTString` features will be translated using :cpp:func:`CPLAtof`. :py:const:`OFTInteger`
fields will be cast to double. Other field types, or errors will
result in a return value of zero.

See :cpp:func:`OGRFeature::GetFieldAsDouble`.

Parameters
-----------
fld_index : int / str
    Field name or 0-based numeric index. For repeated
    access, use of the numeric index avoids a lookup
    step.

Returns
--------
float:
    the field value.
";

%feature("docstring")  GetFieldAsDoubleList "

Fetch field value as a list of doubles.

Currently this function only works for :py:const:`OFTRealList` fields.

See :cpp:func:`OGRFeature::GetFieldAsDoubleList`.

Parameters
-----------
fld_index : int / str
    Field name or 0-based numeric index. For repeated
    access, use of the numeric index avoids a lookup
    step.

Returns
-------
list

Examples
--------
>>> defn = ogr.FeatureDefn()
>>> defn.AddFieldDefn(ogr.FieldDefn('list', ogr.OFTRealList))
>>> feature = ogr.Feature(defn)
>>> feature['list'] = [1.1, 2.2, 3.3]
>>> feature.GetFieldAsDoubleList('list')
[1.1, 2.2, 3.3]
";

%feature("docstring")  GetFieldAsISO8601DateTime "

Fetch :py:const:`OFTDateTime` field value as a ISO8601 representation.

Return a string like 'YYYY-MM-DDTHH:MM:SS(.sss)?(Z|([+|-]HH:MM))?'
Milliseconds are omitted if equal to zero.
Other field types, or errors will result in a return of an empty string.

See :cpp:func:`OGRFeature::GetFieldAsISO8601DateTime`.

.. versionadded:: 3.7

Parameters
-----------
fld_index : int / str
    Field name or 0-based numeric index. For repeated
    access, use of the numeric index avoids a lookup
    step.
options : dict / str
    Not currently used.
";

%feature("docstring")  GetFieldAsInteger "

Fetch field value as a 32-bit integer.

:py:const:`OFTString` features will be translated using atoi().
:py:const:`OFTReal` fields will be cast to integer. Other field types, or
errors will result in a return value of zero.

See :cpp:func:`GetFieldAsInteger`.

Parameters
-----------
fld_index : int / str
    Field name or 0-based numeric index. For repeated
    access, use of the numeric index avoids a lookup
    step.

Returns
--------
int:
    the field value.

Examples
--------
>>> defn = ogr.FeatureDefn()
>>> defn.AddFieldDefn(ogr.FieldDefn('my_int', ogr.OFTInteger64))
>>> feature = ogr.Feature(defn)
>>> feature['my_int'] = 2**32 + 1
>>> feature.GetFieldAsInteger('my_int')
Warning 1: Integer overflow occurred when trying to return 64bit integer. Use GetFieldAsInteger64() instead
2147483647
>>> feature.GetFieldAsInteger64('my_int')
4294967297
>>> feature.GetField('my_int')
4294967297
";

%feature("docstring")  GetFieldAsInteger64 "

Fetch field value as integer 64 bit.

:py:const:`OFTInteger` are promoted to 64 bit. :py:const:`OFTString` features
will be translated using :cpp:func:`CPLAtoGIntBig`. :py:const:`OFTReal` fields
will be cast to integer. Other field types, or errors will result in a return
value of zero.

See :cpp:func:`OGRFeature::GetFieldAsInteger64`.

Parameters
-----------
fld_index : int / str
    Field name or 0-based numeric index. For repeated
    access, use of the numeric index avoids a lookup
    step.

Returns
--------
int:
    the field value.
";

%feature("docstring")  GetFieldAsInteger64List "
Fetch field value as a list of 64 bit integers.

Currently this function only works for :py:const:`OFTInteger64List` fields.

See :cpp:func:`OGRFeature::GetFieldAsInteger64List`.

Parameters
-----------
fld_index : int / str
    Field name or 0-based numeric index. For repeated
    access, use of the numeric index avoids a lookup
    step.

Returns
--------
list:
    the field value.
";

%feature("docstring")  GetFieldAsIntegerList "

Fetch field value as a list of integers.

Currently this function only works for :py:const:`OFTIntegerList` fields.

This function is the same as the C++ method
:cpp:func:`OGRFeature::GetFieldAsIntegerList`.

Parameters
-----------
fld_index : int / str
    Field name or 0-based numeric index. For repeated
    access, use of the numeric index avoids a lookup
    step.

Returns
--------
list:
    the field value.
";

%feature("docstring")  GetFieldAsString "

:py:const:`OFTReal` and :py:const:`OFTInteger` fields will be translated to string using
sprintf(), but not necessarily using the established formatting rules.
Other field types, or errors will result in a return value of zero.

See :cpp:func:`OGRFeature::GetFieldAsString`.

Parameters
-----------
fld_index : int / str
    Field name or 0-based numeric index. For repeated
    access, use of the numeric index avoids a lookup
    step.

Returns
--------
str:
    the field value.
";

%feature("docstring")  GetFieldAsStringList "

Fetch field value as a list of strings.

Currently this method only works for :py:const:`OFTStringList` fields.

See :cpp:func:`OGRFeature::GetFieldAsStringList`.

Parameters
-----------
fld_index : int / str
    Field name or 0-based numeric index. For repeated
    access, use of the numeric index avoids a lookup
    step.

Returns
--------
list:
    the field value.
";

%feature("docstring")  GetFieldCount "

Fetch number of fields on this feature This will always be the same as
the field count for the :py:class:`FeatureDefn`.

See :cpp:func:`OGRFeature::GetFieldCount`.

Returns
--------
int:
    count of fields.
";

%feature("docstring")  GetFieldDefnRef "

Fetch definition for this field.

See :cpp:func:`OGRFeature::GetFieldDefnRef`.

Parameters
-----------
fld_index : int / str
    Field name or 0-based numeric index. For repeated
    access, use of the numeric index avoids a lookup
    step.

Returns
--------
FieldDefn
    a reference to the field definition. This reference should
    not be modified.
";

%feature("docstring")  GetFieldIndex "

Fetch the field index given field name.

See :cpp:func:`OGRFeature::GetFieldIndex`.

Parameters
-----------
field_name:
    the name of the field to search for.

Returns
--------
int:
    the field index, or -1 if no matching field is found.
";

%feature("docstring")  GetFieldType "

Return the type of the given field.

Parameters
-----------
fld_index : int / str
    Field name or 0-based numeric index. For repeated
    access, use of the numeric index avoids a lookup
    step.

Returns
--------
int
    field type code (e.g., :py:const:`OFTInteger`)
";

%feature("docstring")  GetGeomFieldCount "

Fetch number of geometry fields on this feature This will always be
the same as the geometry field count for the :py:class:`FeatureDefn`.

See :cpp:func:`OGRFeature::GetGeomFieldCount`.

Returns
--------
int:
    count of geometry fields.
";

%feature("docstring")  GetGeomFieldDefnRef "

Fetch definition for this geometry field.

See :cpp:func:`OGRFeature::GetGeomFieldDefnRef`.

Parameters
-----------
fld_index : int / str
    Field name or 0-based numeric index. For repeated
    access, use of the numeric index avoids a lookup
    step.

Returns
--------
GeomFieldDefn:
    a reference to the field definition.
    Should not be deleted or modified.
";

%feature("docstring")  GetGeomFieldIndex "

Fetch the geometry field index given geometry field name.

See :cpp:func:`OGRFeature::GetGeomFieldIndex`.

Parameters
-----------
field_name:
    the name of the geometry field to search for.

Returns
--------
int:
    the geometry field index, or -1 if no matching geometry field is found.
";

%feature("docstring")  GetGeomFieldRef "

Fetch a feature :py:class:`Geometry`.

See :cpp:func:`OGRFeature::GetGeomFieldRef`.

Parameters
-----------
fld_index : int / str
    Field name or 0-based numeric index. For repeated
    access, use of the numeric index avoids a lookup
    step.

Returns
-------
Geometry

";

%feature("docstring")  GetGeometryRef "
Return the feature geometry

The lifetime of the returned geometry is bound to the one of its belonging
feature.

See :cpp:func:`OGRFeature::GetGeometryRef`

The :py:func:`Feature.geometry` method is also available as an alias of :py:func:`Feature.GetGeometryRef`.

Returns
--------
Geometry:
    the geometry, or None.
";

%feature("docstring")  GetNativeData "

Returns the native data for the feature.

The native data is the representation in a \"natural\" form that comes
from the driver that created this feature, or that is aimed at an
output driver. The native data may be in different format, which is
indicated by :py:func:`GetNativeMediaType`.

Note that most drivers do not support storing the native data in the
feature object, and if they do, generally the ``NATIVE_DATA`` open option
must be passed at dataset opening.

The \"native data\" does not imply it is something more performant or
powerful than what can be obtained with the rest of the API, but it
may be useful in round-tripping scenarios where some characteristics
of the underlying format are not captured otherwise by the OGR
abstraction.

See :cpp:func:`OGRFeature::GetNativeData` and :ref:`rfc-60`.

Returns
-------
str:
    a string with the native data, or ``None``.
";

%feature("docstring")  GetNativeMediaType "

Returns the native media type for the feature.

The native media type is the identifier for the format of the native
data. It follows the IANA RFC 2045
(seehttps://en.wikipedia.org/wiki/Media_type), e.g.
\"application/vnd.geo+json\" for JSon.

See :cpp:func:`OGRFeature::GetNativeMediaType` and :ref:`rfc-60`.

Returns
--------
str:
    a string with the native media type, or ``None``.
";

%feature("docstring")  GetStyleString "

Fetch style string for this feature.

Set the OGR Feature Style Specification for details on the format of
this string, and :source_file:`ogr/ogr_featurestyle.h` for services available to parse
it.

See :cpp:func:`OGRFeature::GetStyleString`.

Returns
--------
str or None
";

%feature("docstring")  IsFieldNull "

Test if a field is null.

See :cpp:func:OGRFeature::`IsFieldNull`.

Parameters
-----------
fld_index : int / str
    Field name or 0-based numeric index. For repeated
    access, use of the numeric index avoids a lookup
    step.

Returns
--------
bool:
    ``True`` if the field is null, otherwise ``False``
";

%feature("docstring")  IsFieldSet "

Test if a field has ever been assigned a value or not.

See :cpp:func:`OGRFeature::IsFieldSet`.

Parameters
-----------
fld_index : int / str
    Field name or 0-based numeric index. For repeated
    access, use of the numeric index avoids a lookup
    step.

Returns
--------
bool:
    ``True`` if the field has been set, otherwise ``False``.
";

%feature("docstring")  IsFieldSetAndNotNull "

Test if a field is set and not null.

See :cpp:func:`OGRFeature::IsFieldSetAndNotNull`.

Parameters
-----------
fld_index : int / str
    Field name or 0-based numeric index. For repeated
    access, use of the numeric index avoids a lookup
    step.

Returns
--------
bool:
    ``True`` if the field is set and not null, otherwise ``False``.
";

%feature("docstring")  SetFID "

Set the feature identifier.

For specific types of features this operation may fail on illegal
features ids. Generally it always succeeds. Feature ids should be
greater than or equal to zero, with the exception of :py:const:NullFID` (-1)
indicating that the feature id is unknown.

See :cpp:func:`OGRFeature::SetFID`.

Parameters
-----------
fid:
    the new feature identifier value to assign.

Returns
--------
int:
    :py:const:`OGRERR_NONE` on success, or some other value on failure.
";

// SetFieldBinary documented inline

%feature("docstring")  SetFieldDoubleList "

Set field to list of double values.

This function currently on has an effect of :py:const:`OFTIntegerList`,
:py:const:`OFTInteger64List`, :py:const:`OFTRealList` fields.

See :cpp:func:`OGRFeature::SetField`.

This method has only an effect on the in-memory feature object. If
this object comes from a layer and the modifications must be
serialized back to the datasource, :py:meth:`Layer.SetFeature` must be used
afterwards. Or if this is a new feature, :py:meth:`Layer.CreateFeature` must be
used afterwards.

Parameters
-----------
id : int
    the field to set, from 0 to :py:meth:`GetFieldCount`-1.
nList : list
    the values to assign.
";

%feature("docstring")  SetFieldInteger64List "void

Set field to list of 64 bit integer values.

This function currently on has an effect of :py:const:`OFTIntegerList`,
:py:const:`OFTInteger64List`, :py:const:`OFTRealList` fields.

See :cpp:func:`OGRFeature::SetField`.

This method has only an effect on the in-memory feature object. If
this object comes from a layer and the modifications must be
serialized back to the datasource, :py:meth:`Layer.SetFeature` must be used
afterwards. Or if this is a new feature, :py:meth:`Layer.CreateFeature` must be
used afterwards.

Parameters
-----------
id : int
    the field to set, from 0 to :py:meth:`GetFieldCount`-1.
nList : list
    the values to assign.
";

%feature("docstring")  SetFieldIntegerList "void

Set field to list of integer values.

This function currently on has an effect of :py:const:`OFTIntegerList`,
:py:const:`OFTInteger64List`, :py:const:`OFTRealList` fields.

See :cpp:func:`OGRFeature::SetField`.

This method has only an effect on the in-memory feature object. If
this object comes from a layer and the modifications must be
serialized back to the datasource, :py:meth:`Layer.SetFeature` must be used
afterwards. Or if this is a new feature, :py:meth:`Layer.CreateFeature` must be
used afterwards.

Parameters
-----------
id : int
    the field to set, from 0 to :py:meth:`GetFieldCount`-1.
nList : list
    the values to assign.
";

%feature("docstring")  SetFieldNull "

Clear a field, marking it as null.

See :cpp:func:`OGRFeature::SetFieldNull`.

Parameters
-----------
fld_index : int / str
    Field name or 0-based numeric index. For repeated
    access, use of the numeric index avoids a lookup
    step.

";

%feature("docstring")  SetFieldString "

Set field to string value.

:py:const:`OFTInteger` fields will be set based on an atoi() conversion of the
string. :py:const:`OFTInteger64` fields will be set based on an :cpp:func:`CPLAtoGIntBig`
conversion of the string. :py:const:`OFTReal` fields will be set based on an
:cpp:func:`CPLAtof` conversion of the string. Other field types may be
unaffected.

See :cpp:func:`OGRFeature::SetField`.

This method has only an effect on the in-memory feature object. If
this object comes from a layer and the modifications must be
serialized back to the datasource, :py:meth:`Layer.SetFeature` must be used
afterwards. Or if this is a new feature, :py:meth:`Layer.CreateFeature` must be
used afterwards.

Parameters
-----------
fld_index : int / str
    Field name or 0-based numeric index. For repeated
    access, use of the numeric index avoids a lookup
    step.
value:
    the value to assign.
";

%feature("docstring")  SetFieldStringList "

Set field to list of strings value.

This function currently only has an effect of :py:const:`OFTStringList` fields.

See :cpp:func:`OGRFeature::SetField`.

This method has only an effect on the in-memory feature object. If
this object comes from a layer and the modifications must be
serialized back to the datasource, :py:meth:`Layer.SetFeature` must be used
afterwards. Or if this is a new feature, :py:meth:`Layer.CreateFeature` must be
used afterwards.

Parameters
-----------
fld_index : int / str
    Field name or 0-based numeric index. For repeated
    access, use of the numeric index avoids a lookup
    step.
value:
    the value to assign.
";

%feature("docstring")  SetFrom "
Set one feature from another.

Overwrite the contents of this feature from the geometry and
attributes of another. The other feature does not need to have the
same :py:class:`FeatureDefn`. Field values are copied by corresponding field
names. Field types do not have to exactly match. OGR_F_SetField\\*()
function conversion rules will be applied as needed.

See :cpp:func:`OGRFeature::SetFrom`.

Parameters
-----------
other : Feature
    feature from which geometry and field values will be copied.
forgiving : bool, default = True
    ``True`` if the operation should continue despite lacking
    output fields matching some of the source fields.

Returns
--------
int:
    :py:const:`OGRERR_NONE` if the operation succeeds, even if some values are not
    transferred, otherwise an error code.
";

%feature("docstring")  SetFromWithMap "

Set one feature from another.

Overwrite the contents of this feature from the geometry and
attributes of another. The other feature does not need to have the
same :py:class:`FeatureDefn`. Field values are copied according to the provided
indices map. Field types do not have to exactly match.
OGR_F_SetField\\*() function conversion rules will be applied as needed.
This is more efficient than :py:meth:SetFrom` in that this doesn't
lookup the fields by their names. Particularly useful when the field
names don't match.

See :cpp:func:`OGRFeature::SetFrom`.

Parameters
-----------
other : Feature
    handle to the feature from which geometry, and field
    values will be copied.
forgiving : bool
    ``True`` if the operation should continue despite lacking
    output fields matching some of the source fields.
nList : list
    Array of the indices of the destination feature's fields
    stored at the corresponding index of the source feature's fields. A
    value of -1 should be used to ignore the source's field. The array
    should not be NULL and be as long as the number of fields in the
    source feature.

Returns
--------
OGRErr:
    :py:const:`OGRERR_NONE` if the operation succeeds, even if some values are not
    transferred, otherwise an error code.
";

%feature("docstring")  SetGeomField "

Set feature geometry of a specified geometry field.

This function updates the features geometry, and operates exactly as
:py:meth:`SetGeomFieldDirectly`, except that this function does not assume
ownership of the passed geometry, but instead makes a copy of it.

See :cpp:func:`OGRFeature::SetGeomField`.

Parameters
-----------
fld_index : int / str
    Geometry field name or 0-based numeric index. For repeated
    access, use of the numeric index avoids a lookup
    step.
geom : Geometry
    handle to the new geometry to apply to feature.

Returns
--------
int:
    :py:const:`OGRERR_NONE` if successful, or
    :py:const:`OGR_UNSUPPORTED_GEOMETRY_TYPE` if the geometry type is illegal for
    the :py:class:`FeatureDefn` (checking not yet implemented).
";

// SetGeomFieldDirectly : documented inline

%feature("docstring")  SetGeometry "

Set feature geometry.

This function updates the features geometry, and operates exactly as
:py:meth:`SetGeometryDirectly`, except that this function does not assume
ownership of the passed geometry, but instead makes a copy of it.

See :cpp:func:`OGRFeature::SetGeometry`.

This method has only an effect on the in-memory feature object. If
this object comes from a layer and the modifications must be
serialized back to the datasource, :py:meth:`Layer.SetFeature` must be used
afterwards. Or if this is a new feature, :py:meth:`Layer.CreateFeature` must be
used afterwards.

Parameters
-----------
geom : Geometry
    new geometry to apply to feature.

Returns
--------
int:
    :py:const:`OGRERR_NONE` if successful, or
    :py:const:`OGR_UNSUPPORTED_GEOMETRY_TYPE` if the geometry type is illegal for
    the :py:class:`FeatureDefn` (checking not yet implemented).
";

// SetGeometryDirectly : documented inline

%feature("docstring")  SetNativeData "

Sets the native data for the feature.

The native data is the representation in a \"natural\" form that comes
from the driver that created this feature, or that is aimed at an
output driver. The native data may be in different format, which is
indicated by :py:meth:`GetNativeMediaType`.

See :cpp:func:`OGRFeature::SetNativeData` and :ref:`rfc-60`.

Parameters
-----------
nativeData : str
    a string with the native data, or ``None``
";

%feature("docstring")  SetNativeMediaType "

Sets the native media type for the feature.

The native media type is the identifier for the format of the native
data. It follows the IANA RFC 2045
(see https://en.wikipedia.org/wiki/Media_type), e.g.
\"application/vnd.geo+json\" for JSon.

See :cpp:func:`OGRFeature::SetNativeMediaType` and :ref:`rfc-60`.

Parameters
-----------
nativeMediaType : str
    a string with the native media type, or ``None``
";

%feature("docstring")  SetStyleString "

Set feature style string.

See :cpp:func:`OGRFeature::SetStyleString`.

Parameters
-----------
the_string : str
    the style string to apply to this feature
";

%feature("docstring")  UnsetField "

Clear a field, marking it as unset.

See :cpp:func:`OGRFeature::UnsetField`.

Parameters
-----------
fld_index : int / str
    Field name or 0-based numeric index. For repeated
    access, use of the numeric index avoids a lookup
    step.
";

%feature("docstring")  Validate "

Validate that a feature meets constraints of its schema.

The scope of test is specified with the ``flags`` parameter.

Regarding :py:const:`OGR_F_VAL_WIDTH`, the test is done assuming the string
width must be interpreted as the number of UTF-8 characters. Some drivers might
interpret the width as the number of bytes instead. So this test is rather
conservative (if it fails, then it will fail for all interpretations).

See :cpp:func:`OGRFeature::Validate`.

Parameters
-----------
flags : int, default = :py:const:`F_VAL_ALL`
    One ore more of :py:const:`OGR_F_VAL_NULL`,
    :py:const:`OGR_F_VAL_GEOM_TYPE`, py:const:`OGR_F_VAL_WIDTH` and
    :py:const:`OGR_F_VAL_ALLOW_NULL_WHEN_DEFAULT` combined with
    the with ``|`` operator
bEmitError : bool, default = True
    TRUE if a CPLError() must be emitted when a check fails

Returns
-------
int:
    TRUE if all enabled validation tests pass.
";

}
