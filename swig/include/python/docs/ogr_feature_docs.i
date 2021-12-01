%extend OGRFeatureShadow {
// File: ogrfeature_8cpp.xml
%feature("docstring")  Create "OGRFeatureH
OGR_F_Create(OGRFeatureDefnH hDefn)

Feature factory.

Note that the OGRFeature will increment the reference count of its
defining OGRFeatureDefn. Destruction of the OGRFeatureDefn before
destruction of all OGRFeatures that depend on it is likely to result
in a crash.

This function is the same as the C++ method OGRFeature::OGRFeature().

Parameters:
-----------

hDefn:  handle to the feature class (layer) definition to which the
feature will adhere.

a handle to the new feature object with null fields and no geometry,
or, starting with GDAL 2.1, NULL in case out of memory situation. ";

%feature("docstring")  Destroy "void OGR_F_Destroy(OGRFeatureH hFeat)

Destroy feature.

The feature is deleted, but within the context of the GDAL/OGR heap.
This is necessary when higher level applications use GDAL/OGR from a
DLL and they want to delete a feature created within the DLL. If the
delete is done in the calling application the memory will be freed
onto the application heap which is inappropriate.

This function is the same as the C++ method
OGRFeature::DestroyFeature().

Parameters:
-----------

hFeat:  handle to the feature to destroy. ";

%feature("docstring")  GetDefnRef "OGRFeatureDefnH
OGR_F_GetDefnRef(OGRFeatureH hFeat)

Fetch feature definition.

This function is the same as the C++ method OGRFeature::GetDefnRef().

Parameters:
-----------

hFeat:  handle to the feature to get the feature definition from.

a handle to the feature definition object on which feature depends. ";

%feature("docstring")  SetGeometryDirectly "OGRErr
OGR_F_SetGeometryDirectly(OGRFeatureH hFeat, OGRGeometryH hGeom)

Set feature geometry.

This function updates the features geometry, and operate exactly as
SetGeometry(), except that this function assumes ownership of the
passed geometry (even in case of failure of that function).

This function is the same as the C++ method
OGRFeature::SetGeometryDirectly.

This method has only an effect on the in-memory feature object. If
this object comes from a layer and the modifications must be
serialized back to the datasource, OGR_L_SetFeature() must be used
afterwards. Or if this is a new feature, OGR_L_CreateFeature() must be
used afterwards.

Parameters:
-----------

hFeat:  handle to the feature on which to apply the geometry.

hGeom:  handle to the new geometry to apply to feature.

OGRERR_NONE if successful, or OGR_UNSUPPORTED_GEOMETRY_TYPE if the
geometry type is illegal for the OGRFeatureDefn (checking not yet
implemented). ";

%feature("docstring")  SetGeometry "OGRErr
OGR_F_SetGeometry(OGRFeatureH hFeat, OGRGeometryH hGeom)

Set feature geometry.

This function updates the features geometry, and operate exactly as
SetGeometryDirectly(), except that this function does not assume
ownership of the passed geometry, but instead makes a copy of it.

This function is the same as the C++ OGRFeature::SetGeometry().

This method has only an effect on the in-memory feature object. If
this object comes from a layer and the modifications must be
serialized back to the datasource, OGR_L_SetFeature() must be used
afterwards. Or if this is a new feature, OGR_L_CreateFeature() must be
used afterwards.

Parameters:
-----------

hFeat:  handle to the feature on which new geometry is applied to.

hGeom:  handle to the new geometry to apply to feature.

OGRERR_NONE if successful, or OGR_UNSUPPORTED_GEOMETRY_TYPE if the
geometry type is illegal for the OGRFeatureDefn (checking not yet
implemented). ";

%feature("docstring")  StealGeometry "OGRGeometryH
OGR_F_StealGeometry(OGRFeatureH hFeat)

Take away ownership of geometry.

Fetch the geometry from this feature, and clear the reference to the
geometry on the feature. This is a mechanism for the application to
take over ownership of the geometry from the feature without copying.
Sort of an inverse to OGR_FSetGeometryDirectly().

After this call the OGRFeature will have a NULL geometry.

the pointer to the geometry. ";

%feature("docstring")  GetGeometryRef "OGRGeometryH
OGR_F_GetGeometryRef(OGRFeatureH hFeat)

Fetch a handle to feature geometry.

This function is essentially the same as the C++ method
OGRFeature::GetGeometryRef() (the only difference is that this C
function honours OGRGetNonLinearGeometriesEnabledFlag())

Parameters:
-----------

hFeat:  handle to the feature to get geometry from.

a handle to internal feature geometry. This object should not be
modified. ";

%feature("docstring")  GetGeomFieldRef "OGRGeometryH
OGR_F_GetGeomFieldRef(OGRFeatureH hFeat, int iField)

Fetch a handle to feature geometry.

This function is the same as the C++ method
OGRFeature::GetGeomFieldRef().

Parameters:
-----------

hFeat:  handle to the feature to get geometry from.

iField:  geometry field to get.

a handle to internal feature geometry. This object should not be
modified.

GDAL 1.11 ";

%feature("docstring")  SetGeomFieldDirectly "OGRErr
OGR_F_SetGeomFieldDirectly(OGRFeatureH hFeat, int iField, OGRGeometryH
hGeom)

Set feature geometry of a specified geometry field.

This function updates the features geometry, and operate exactly as
SetGeomField(), except that this function assumes ownership of the
passed geometry (even in case of failure of that function).

This function is the same as the C++ method
OGRFeature::SetGeomFieldDirectly.

Parameters:
-----------

hFeat:  handle to the feature on which to apply the geometry.

iField:  geometry field to set.

hGeom:  handle to the new geometry to apply to feature.

OGRERR_NONE if successful, or OGRERR_FAILURE if the index is invalid,
or OGR_UNSUPPORTED_GEOMETRY_TYPE if the geometry type is illegal for
the OGRFeatureDefn (checking not yet implemented).

GDAL 1.11 ";

%feature("docstring")  SetGeomField "OGRErr
OGR_F_SetGeomField(OGRFeatureH hFeat, int iField, OGRGeometryH hGeom)

Set feature geometry of a specified geometry field.

This function updates the features geometry, and operate exactly as
SetGeometryDirectly(), except that this function does not assume
ownership of the passed geometry, but instead makes a copy of it.

This function is the same as the C++ OGRFeature::SetGeomField().

Parameters:
-----------

hFeat:  handle to the feature on which new geometry is applied to.

iField:  geometry field to set.

hGeom:  handle to the new geometry to apply to feature.

OGRERR_NONE if successful, or OGR_UNSUPPORTED_GEOMETRY_TYPE if the
geometry type is illegal for the OGRFeatureDefn (checking not yet
implemented). ";

%feature("docstring")  Clone "OGRFeatureH OGR_F_Clone(OGRFeatureH
hFeat)

Duplicate feature.

The newly created feature is owned by the caller, and will have its
own reference to the OGRFeatureDefn.

This function is the same as the C++ method OGRFeature::Clone().

Parameters:
-----------

hFeat:  handle to the feature to clone.

a handle to the new feature, exactly matching this feature. ";

%feature("docstring")  GetFieldCount "int
OGR_F_GetFieldCount(OGRFeatureH hFeat)

Fetch number of fields on this feature This will always be the same as
the field count for the OGRFeatureDefn.

This function is the same as the C++ method
OGRFeature::GetFieldCount().

Parameters:
-----------

hFeat:  handle to the feature to get the fields count from.

count of fields. ";

%feature("docstring")  GetFieldDefnRef "OGRFieldDefnH
OGR_F_GetFieldDefnRef(OGRFeatureH hFeat, int i)

Fetch definition for this field.

This function is the same as the C++ method
OGRFeature::GetFieldDefnRef().

Parameters:
-----------

hFeat:  handle to the feature on which the field is found.

i:  the field to fetch, from 0 to GetFieldCount()-1.

a handle to the field definition (from the OGRFeatureDefn). This is an
internal reference, and should not be deleted or modified. ";

%feature("docstring")  GetFieldIndex "int
OGR_F_GetFieldIndex(OGRFeatureH hFeat, const char *pszName)

Fetch the field index given field name.

This is a cover for the OGRFeatureDefn::GetFieldIndex() method.

This function is the same as the C++ method
OGRFeature::GetFieldIndex().

Parameters:
-----------

hFeat:  handle to the feature on which the field is found.

pszName:  the name of the field to search for.

the field index, or -1 if no matching field is found. ";

%feature("docstring")  GetGeomFieldCount "int
OGR_F_GetGeomFieldCount(OGRFeatureH hFeat)

Fetch number of geometry fields on this feature This will always be
the same as the geometry field count for the OGRFeatureDefn.

This function is the same as the C++ method
OGRFeature::GetGeomFieldCount().

Parameters:
-----------

hFeat:  handle to the feature to get the geometry fields count from.

count of geometry fields.

GDAL 1.11 ";

%feature("docstring")  GetGeomFieldDefnRef "OGRGeomFieldDefnH
OGR_F_GetGeomFieldDefnRef(OGRFeatureH hFeat, int i)

Fetch definition for this geometry field.

This function is the same as the C++ method
OGRFeature::GetGeomFieldDefnRef().

Parameters:
-----------

hFeat:  handle to the feature on which the field is found.

i:  the field to fetch, from 0 to GetGeomFieldCount()-1.

a handle to the field definition (from the OGRFeatureDefn). This is an
internal reference, and should not be deleted or modified.

GDAL 1.11 ";

%feature("docstring")  GetGeomFieldIndex "int
OGR_F_GetGeomFieldIndex(OGRFeatureH hFeat, const char *pszName)

Fetch the geometry field index given geometry field name.

This is a cover for the OGRFeatureDefn::GetGeomFieldIndex() method.

This function is the same as the C++ method
OGRFeature::GetGeomFieldIndex().

Parameters:
-----------

hFeat:  handle to the feature on which the geometry field is found.

pszName:  the name of the geometry field to search for.

the geometry field index, or -1 if no matching geometry field is
found.

GDAL 1.11 ";

%feature("docstring")  IsFieldSet "int OGR_F_IsFieldSet(OGRFeatureH
hFeat, int iField)

Test if a field has ever been assigned a value or not.

This function is the same as the C++ method OGRFeature::IsFieldSet().

Parameters:
-----------

hFeat:  handle to the feature on which the field is.

iField:  the field to test.

TRUE if the field has been set, otherwise false. ";

%feature("docstring")  UnsetField "void OGR_F_UnsetField(OGRFeatureH
hFeat, int iField)

Clear a field, marking it as unset.

This function is the same as the C++ method OGRFeature::UnsetField().

Parameters:
-----------

hFeat:  handle to the feature on which the field is.

iField:  the field to unset. ";

%feature("docstring")  IsFieldNull "int OGR_F_IsFieldNull(OGRFeatureH
hFeat, int iField)

Test if a field is null.

This function is the same as the C++ method OGRFeature::IsFieldNull().

Parameters:
-----------

hFeat:  handle to the feature on which the field is.

iField:  the field to test.

TRUE if the field is null, otherwise false.

GDAL 2.2 ";

%feature("docstring")  IsFieldSetAndNotNull "int
OGR_F_IsFieldSetAndNotNull(OGRFeatureH hFeat, int iField)

Test if a field is set and not null.

This function is the same as the C++ method
OGRFeature::IsFieldSetAndNotNull().

Parameters:
-----------

hFeat:  handle to the feature on which the field is.

iField:  the field to test.

TRUE if the field is set and not null, otherwise false.

GDAL 2.2 ";

%feature("docstring")  SetFieldNull "void
OGR_F_SetFieldNull(OGRFeatureH hFeat, int iField)

Clear a field, marking it as null.

This function is the same as the C++ method
OGRFeature::SetFieldNull().

Parameters:
-----------

hFeat:  handle to the feature on which the field is.

iField:  the field to set to null.

GDAL 2.2 ";

%feature("docstring")  GetRawFieldRef "OGRField*
OGR_F_GetRawFieldRef(OGRFeatureH hFeat, int iField)

Fetch a handle to the internal field value given the index.

This function is the same as the C++ method
OGRFeature::GetRawFieldRef().

Parameters:
-----------

hFeat:  handle to the feature on which field is found.

iField:  the field to fetch, from 0 to GetFieldCount()-1.

the returned handle is to an internal data structure, and should not
be freed, or modified. ";

%feature("docstring")  GetFieldAsInteger "int
OGR_F_GetFieldAsInteger(OGRFeatureH hFeat, int iField)

Fetch field value as integer.

OFTString features will be translated using atoi(). OFTReal fields
will be cast to integer. Other field types, or errors will result in a
return value of zero.

This function is the same as the C++ method
OGRFeature::GetFieldAsInteger().

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to fetch, from 0 to GetFieldCount()-1.

the field value. ";

%feature("docstring")  GetFieldAsInteger64 "GIntBig
OGR_F_GetFieldAsInteger64(OGRFeatureH hFeat, int iField)

Fetch field value as integer 64 bit.

OFTInteger are promoted to 64 bit. OFTString features will be
translated using CPLAtoGIntBig(). OFTReal fields will be cast to
integer. Other field types, or errors will result in a return value of
zero.

This function is the same as the C++ method
OGRFeature::GetFieldAsInteger64().

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to fetch, from 0 to GetFieldCount()-1.

the field value.

GDAL 2.0 ";

%feature("docstring")  GetFieldAsDouble "double
OGR_F_GetFieldAsDouble(OGRFeatureH hFeat, int iField)

Fetch field value as a double.

OFTString features will be translated using CPLAtof(). OFTInteger
fields will be cast to double. Other field types, or errors will
result in a return value of zero.

This function is the same as the C++ method
OGRFeature::GetFieldAsDouble().

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to fetch, from 0 to GetFieldCount()-1.

the field value. ";

%feature("docstring")  OGRFeatureFormatDateTimeBuffer "static void
OGRFeatureFormatDateTimeBuffer(char *szTempBuffer, size_t nMaxSize,
int nYear, int nMonth, int nDay, int nHour, int nMinute, float
fSecond, int nTZFlag) ";

%feature("docstring")  GetFieldAsString "const char*
OGR_F_GetFieldAsString(OGRFeatureH hFeat, int iField)

Fetch field value as a string.

OFTReal and OFTInteger fields will be translated to string using
sprintf(), but not necessarily using the established formatting rules.
Other field types, or errors will result in a return value of zero.

This function is the same as the C++ method
OGRFeature::GetFieldAsString().

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to fetch, from 0 to GetFieldCount()-1.

the field value. This string is internal, and should not be modified,
or freed. Its lifetime may be very brief. ";

%feature("docstring")  GetFieldAsIntegerList "const int*
OGR_F_GetFieldAsIntegerList(OGRFeatureH hFeat, int iField, int
*pnCount)

Fetch field value as a list of integers.

Currently this function only works for OFTIntegerList fields.

This function is the same as the C++ method
OGRFeature::GetFieldAsIntegerList().

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to fetch, from 0 to GetFieldCount()-1.

pnCount:  an integer to put the list count (number of integers) into.

the field value. This list is internal, and should not be modified, or
freed. Its lifetime may be very brief. If *pnCount is zero on return
the returned pointer may be NULL or non-NULL. ";

%feature("docstring")  GetFieldAsInteger64List "const GIntBig*
OGR_F_GetFieldAsInteger64List(OGRFeatureH hFeat, int iField, int
*pnCount)

Fetch field value as a list of 64 bit integers.

Currently this function only works for OFTInteger64List fields.

This function is the same as the C++ method
OGRFeature::GetFieldAsInteger64List().

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to fetch, from 0 to GetFieldCount()-1.

pnCount:  an integer to put the list count (number of integers) into.

the field value. This list is internal, and should not be modified, or
freed. Its lifetime may be very brief. If *pnCount is zero on return
the returned pointer may be NULL or non-NULL.

GDAL 2.0 ";

%feature("docstring")  GetFieldAsDoubleList "const double*
OGR_F_GetFieldAsDoubleList(OGRFeatureH hFeat, int iField, int
*pnCount)

Fetch field value as a list of doubles.

Currently this function only works for OFTRealList fields.

This function is the same as the C++ method
OGRFeature::GetFieldAsDoubleList().

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to fetch, from 0 to GetFieldCount()-1.

pnCount:  an integer to put the list count (number of doubles) into.

the field value. This list is internal, and should not be modified, or
freed. Its lifetime may be very brief. If *pnCount is zero on return
the returned pointer may be NULL or non-NULL. ";

%feature("docstring")  GetFieldAsStringList "char**
OGR_F_GetFieldAsStringList(OGRFeatureH hFeat, int iField)

Fetch field value as a list of strings.

Currently this method only works for OFTStringList fields.

The returned list is terminated by a NULL pointer. The number of
elements can also be calculated using CSLCount().

This function is the same as the C++ method
OGRFeature::GetFieldAsStringList().

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to fetch, from 0 to GetFieldCount()-1.

the field value. This list is internal, and should not be modified, or
freed. Its lifetime may be very brief. ";

%feature("docstring")  GetFieldAsBinary "GByte*
OGR_F_GetFieldAsBinary(OGRFeatureH hFeat, int iField, int *pnBytes)

Fetch field value as binary.

This method only works for OFTBinary and OFTString fields.

This function is the same as the C++ method
OGRFeature::GetFieldAsBinary().

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to fetch, from 0 to GetFieldCount()-1.

pnBytes:  location to place count of bytes returned.

the field value. This list is internal, and should not be modified, or
freed. Its lifetime may be very brief. ";

%feature("docstring")  GetFieldAsDateTime "int
OGR_F_GetFieldAsDateTime(OGRFeatureH hFeat, int iField, int *pnYear,
int *pnMonth, int *pnDay, int *pnHour, int *pnMinute, int *pnSecond,
int *pnTZFlag)

Fetch field value as date and time.

Currently this method only works for OFTDate, OFTTime and OFTDateTime
fields.

This function is the same as the C++ method
OGRFeature::GetFieldAsDateTime().

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to fetch, from 0 to GetFieldCount()-1.

pnYear:  (including century)

pnMonth:  (1-12)

pnDay:  (1-31)

pnHour:  (0-23)

pnMinute:  (0-59)

pnSecond:  (0-59)

pnTZFlag:  (0=unknown, 1=localtime, 100=GMT, see data model for
details)

TRUE on success or FALSE on failure.

See:  Use OGR_F_GetFieldAsDateTimeEx() for second with millisecond
accuracy. ";

%feature("docstring")  GetFieldAsDateTimeEx "int
OGR_F_GetFieldAsDateTimeEx(OGRFeatureH hFeat, int iField, int *pnYear,
int *pnMonth, int *pnDay, int *pnHour, int *pnMinute, float *pfSecond,
int *pnTZFlag)

Fetch field value as date and time.

Currently this method only works for OFTDate, OFTTime and OFTDateTime
fields.

This function is the same as the C++ method
OGRFeature::GetFieldAsDateTime().

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to fetch, from 0 to GetFieldCount()-1.

pnYear:  (including century)

pnMonth:  (1-12)

pnDay:  (1-31)

pnHour:  (0-23)

pnMinute:  (0-59)

pfSecond:  (0-59 with millisecond accuracy)

pnTZFlag:  (0=unknown, 1=localtime, 100=GMT, see data model for
details)

TRUE on success or FALSE on failure.

GDAL 2.0 ";

%feature("docstring")  OGRFeatureGetIntegerValue "static int
OGRFeatureGetIntegerValue(OGRFieldDefn *poFDefn, int nValue) ";

%feature("docstring")  SetFieldInteger "void
OGR_F_SetFieldInteger(OGRFeatureH hFeat, int iField, int nValue)

Set field to integer value.

OFTInteger, OFTInteger64 and OFTReal fields will be set directly.
OFTString fields will be assigned a string representation of the
value, but not necessarily taking into account formatting constraints
on this field. Other field types may be unaffected.

This function is the same as the C++ method OGRFeature::SetField().

This method has only an effect on the in-memory feature object. If
this object comes from a layer and the modifications must be
serialized back to the datasource, OGR_L_SetFeature() must be used
afterwards. Or if this is a new feature, OGR_L_CreateFeature() must be
used afterwards.

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to fetch, from 0 to GetFieldCount()-1.

nValue:  the value to assign. ";

%feature("docstring")  SetFieldInteger64 "void
OGR_F_SetFieldInteger64(OGRFeatureH hFeat, int iField, GIntBig nValue)

Set field to 64 bit integer value.

OFTInteger, OFTInteger64 and OFTReal fields will be set directly.
OFTString fields will be assigned a string representation of the
value, but not necessarily taking into account formatting constraints
on this field. Other field types may be unaffected.

This function is the same as the C++ method OGRFeature::SetField().

This method has only an effect on the in-memory feature object. If
this object comes from a layer and the modifications must be
serialized back to the datasource, OGR_L_SetFeature() must be used
afterwards. Or if this is a new feature, OGR_L_CreateFeature() must be
used afterwards.

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to fetch, from 0 to GetFieldCount()-1.

nValue:  the value to assign.

GDAL 2.0 ";

%feature("docstring")  SetFieldDouble "void
OGR_F_SetFieldDouble(OGRFeatureH hFeat, int iField, double dfValue)

Set field to double value.

OFTInteger, OFTInteger64 and OFTReal fields will be set directly.
OFTString fields will be assigned a string representation of the
value, but not necessarily taking into account formatting constraints
on this field. Other field types may be unaffected.

This function is the same as the C++ method OGRFeature::SetField().

This method has only an effect on the in-memory feature object. If
this object comes from a layer and the modifications must be
serialized back to the datasource, OGR_L_SetFeature() must be used
afterwards. Or if this is a new feature, OGR_L_CreateFeature() must be
used afterwards.

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to fetch, from 0 to GetFieldCount()-1.

dfValue:  the value to assign. ";

%feature("docstring")  SetFieldString "void
OGR_F_SetFieldString(OGRFeatureH hFeat, int iField, const char
*pszValue)

Set field to string value.

OFTInteger fields will be set based on an atoi() conversion of the
string. OFTInteger64 fields will be set based on an CPLAtoGIntBig()
conversion of the string. OFTReal fields will be set based on an
CPLAtof() conversion of the string. Other field types may be
unaffected.

This function is the same as the C++ method OGRFeature::SetField().

This method has only an effect on the in-memory feature object. If
this object comes from a layer and the modifications must be
serialized back to the datasource, OGR_L_SetFeature() must be used
afterwards. Or if this is a new feature, OGR_L_CreateFeature() must be
used afterwards.

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to fetch, from 0 to GetFieldCount()-1.

pszValue:  the value to assign. ";

%feature("docstring")  SetFieldIntegerList "void
OGR_F_SetFieldIntegerList(OGRFeatureH hFeat, int iField, int nCount,
const int *panValues)

Set field to list of integers value.

This function currently on has an effect of OFTIntegerList,
OFTInteger64List and OFTRealList fields.

This function is the same as the C++ method OGRFeature::SetField().

This method has only an effect on the in-memory feature object. If
this object comes from a layer and the modifications must be
serialized back to the datasource, OGR_L_SetFeature() must be used
afterwards. Or if this is a new feature, OGR_L_CreateFeature() must be
used afterwards.

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to set, from 0 to GetFieldCount()-1.

nCount:  the number of values in the list being assigned.

panValues:  the values to assign. ";

%feature("docstring")  SetFieldInteger64List "void
OGR_F_SetFieldInteger64List(OGRFeatureH hFeat, int iField, int nCount,
const GIntBig *panValues)

Set field to list of 64 bit integers value.

This function currently on has an effect of OFTIntegerList,
OFTInteger64List and OFTRealList fields.

This function is the same as the C++ method OGRFeature::SetField().

This method has only an effect on the in-memory feature object. If
this object comes from a layer and the modifications must be
serialized back to the datasource, OGR_L_SetFeature() must be used
afterwards. Or if this is a new feature, OGR_L_CreateFeature() must be
used afterwards.

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to set, from 0 to GetFieldCount()-1.

nCount:  the number of values in the list being assigned.

panValues:  the values to assign.

GDAL 2.0 ";

%feature("docstring")  SetFieldDoubleList "void
OGR_F_SetFieldDoubleList(OGRFeatureH hFeat, int iField, int nCount,
const double *padfValues)

Set field to list of doubles value.

This function currently on has an effect of OFTIntegerList,
OFTInteger64List, OFTRealList fields.

This function is the same as the C++ method OGRFeature::SetField().

This method has only an effect on the in-memory feature object. If
this object comes from a layer and the modifications must be
serialized back to the datasource, OGR_L_SetFeature() must be used
afterwards. Or if this is a new feature, OGR_L_CreateFeature() must be
used afterwards.

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to set, from 0 to GetFieldCount()-1.

nCount:  the number of values in the list being assigned.

padfValues:  the values to assign. ";

%feature("docstring")  SetFieldStringList "void
OGR_F_SetFieldStringList(OGRFeatureH hFeat, int iField, CSLConstList
papszValues)

Set field to list of strings value.

This function currently on has an effect of OFTStringList fields.

This function is the same as the C++ method OGRFeature::SetField().

This method has only an effect on the in-memory feature object. If
this object comes from a layer and the modifications must be
serialized back to the datasource, OGR_L_SetFeature() must be used
afterwards. Or if this is a new feature, OGR_L_CreateFeature() must be
used afterwards.

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to set, from 0 to GetFieldCount()-1.

papszValues:  the values to assign. List of NUL-terminated string,
ending with a NULL pointer. ";

%feature("docstring")  SetFieldBinary "void
OGR_F_SetFieldBinary(OGRFeatureH hFeat, int iField, int nBytes, const
void *pabyData)

Set field to binary data.

This function currently on has an effect of OFTBinary fields.

This function is the same as the C++ method OGRFeature::SetField().

This method has only an effect on the in-memory feature object. If
this object comes from a layer and the modifications must be
serialized back to the datasource, OGR_L_SetFeature() must be used
afterwards. Or if this is a new feature, OGR_L_CreateFeature() must be
used afterwards.

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to set, from 0 to GetFieldCount()-1.

nBytes:  the number of bytes in pabyData array.

pabyData:  the data to apply. ";

%feature("docstring")  SetFieldDateTime "void
OGR_F_SetFieldDateTime(OGRFeatureH hFeat, int iField, int nYear, int
nMonth, int nDay, int nHour, int nMinute, int nSecond, int nTZFlag)

Set field to datetime.

This method currently only has an effect for OFTDate, OFTTime and
OFTDateTime fields.

This method has only an effect on the in-memory feature object. If
this object comes from a layer and the modifications must be
serialized back to the datasource, OGR_L_SetFeature() must be used
afterwards. Or if this is a new feature, OGR_L_CreateFeature() must be
used afterwards.

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to set, from 0 to GetFieldCount()-1.

nYear:  (including century)

nMonth:  (1-12)

nDay:  (1-31)

nHour:  (0-23)

nMinute:  (0-59)

nSecond:  (0-59)

nTZFlag:  (0=unknown, 1=localtime, 100=GMT, see data model for
details)

See:  Use OGR_F_SetFieldDateTimeEx() for second with millisecond
accuracy. ";

%feature("docstring")  SetFieldDateTimeEx "void
OGR_F_SetFieldDateTimeEx(OGRFeatureH hFeat, int iField, int nYear, int
nMonth, int nDay, int nHour, int nMinute, float fSecond, int nTZFlag)

Set field to datetime.

This method currently only has an effect for OFTDate, OFTTime and
OFTDateTime fields.

This method has only an effect on the in-memory feature object. If
this object comes from a layer and the modifications must be
serialized back to the datasource, OGR_L_SetFeature() must be used
afterwards. Or if this is a new feature, OGR_L_CreateFeature() must be
used afterwards.

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to set, from 0 to GetFieldCount()-1.

nYear:  (including century)

nMonth:  (1-12)

nDay:  (1-31)

nHour:  (0-23)

nMinute:  (0-59)

fSecond:  (0-59, with millisecond accuracy)

nTZFlag:  (0=unknown, 1=localtime, 100=GMT, see data model for
details)

GDAL 2.0 ";

%feature("docstring")  SetFieldRaw "void
OGR_F_SetFieldRaw(OGRFeatureH hFeat, int iField, OGRField *psValue)

Set field.

The passed value OGRField must be of exactly the same type as the
target field, or an application crash may occur. The passed value is
copied, and will not be affected. It remains the responsibility of the
caller.

This function is the same as the C++ method OGRFeature::SetField().

This method has only an effect on the in-memory feature object. If
this object comes from a layer and the modifications must be
serialized back to the datasource, OGR_L_SetFeature() must be used
afterwards. Or if this is a new feature, OGR_L_CreateFeature() must be
used afterwards.

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to fetch, from 0 to GetFieldCount()-1.

psValue:  handle on the value to assign. ";

%feature("docstring")  DumpReadable "void
OGR_F_DumpReadable(OGRFeatureH hFeat, FILE *fpOut)

Dump this feature in a human readable form.

This dumps the attributes, and geometry; however, it doesn't
definition information (other than field types and names), nor does it
report the geometry spatial reference system.

This function is the same as the C++ method
OGRFeature::DumpReadable().

Parameters:
-----------

hFeat:  handle to the feature to dump.

fpOut:  the stream to write to, such as strout. ";

%feature("docstring")  GetFID "GIntBig OGR_F_GetFID(OGRFeatureH
hFeat)

Get feature identifier.

This function is the same as the C++ method OGRFeature::GetFID().
Note: since GDAL 2.0, this method returns a GIntBig (previously a
long)

Parameters:
-----------

hFeat:  handle to the feature from which to get the feature
identifier.

feature id or OGRNullFID if none has been assigned. ";

%feature("docstring")  SetFID "OGRErr OGR_F_SetFID(OGRFeatureH hFeat,
GIntBig nFID)

Set the feature identifier.

For specific types of features this operation may fail on illegal
features ids. Generally it always succeeds. Feature ids should be
greater than or equal to zero, with the exception of OGRNullFID (-1)
indicating that the feature id is unknown.

This function is the same as the C++ method OGRFeature::SetFID().

Parameters:
-----------

hFeat:  handle to the feature to set the feature id to.

nFID:  the new feature identifier value to assign.

On success OGRERR_NONE, or on failure some other value. ";

%feature("docstring")  Equal "int OGR_F_Equal(OGRFeatureH hFeat,
OGRFeatureH hOtherFeat)

Test if two features are the same.

Two features are considered equal if the share them (handle equality)
same OGRFeatureDefn, have the same field values, and the same geometry
(as tested by OGR_G_Equal()) as well as the same feature id.

This function is the same as the C++ method OGRFeature::Equal().

Parameters:
-----------

hFeat:  handle to one of the feature.

hOtherFeat:  handle to the other feature to test this one against.

TRUE if they are equal, otherwise FALSE. ";

%feature("docstring")  SetFrom "OGRErr OGR_F_SetFrom(OGRFeatureH
hFeat, OGRFeatureH hOtherFeat, int bForgiving)

Set one feature from another.

Overwrite the contents of this feature from the geometry and
attributes of another. The hOtherFeature does not need to have the
same OGRFeatureDefn. Field values are copied by corresponding field
names. Field types do not have to exactly match. OGR_F_SetField*()
function conversion rules will be applied as needed.

This function is the same as the C++ method OGRFeature::SetFrom().

Parameters:
-----------

hFeat:  handle to the feature to set to.

hOtherFeat:  handle to the feature from which geometry, and field
values will be copied.

bForgiving:  TRUE if the operation should continue despite lacking
output fields matching some of the source fields.

OGRERR_NONE if the operation succeeds, even if some values are not
transferred, otherwise an error code. ";

%feature("docstring")  SetFromWithMap "OGRErr
OGR_F_SetFromWithMap(OGRFeatureH hFeat, OGRFeatureH hOtherFeat, int
bForgiving, const int *panMap)

Set one feature from another.

Overwrite the contents of this feature from the geometry and
attributes of another. The hOtherFeature does not need to have the
same OGRFeatureDefn. Field values are copied according to the provided
indices map. Field types do not have to exactly match.
OGR_F_SetField*() function conversion rules will be applied as needed.
This is more efficient than OGR_F_SetFrom() in that this doesn't
lookup the fields by their names. Particularly useful when the field
names don't match.

This function is the same as the C++ method OGRFeature::SetFrom().

Parameters:
-----------

hFeat:  handle to the feature to set to.

hOtherFeat:  handle to the feature from which geometry, and field
values will be copied.

panMap:  Array of the indices of the destination feature's fields
stored at the corresponding index of the source feature's fields. A
value of -1 should be used to ignore the source's field. The array
should not be NULL and be as long as the number of fields in the
source feature.

bForgiving:  TRUE if the operation should continue despite lacking
output fields matching some of the source fields.

OGRERR_NONE if the operation succeeds, even if some values are not
transferred, otherwise an error code. ";

%feature("docstring")  GetStyleString "const char*
OGR_F_GetStyleString(OGRFeatureH hFeat)

Fetch style string for this feature.

Set the OGR Feature Style Specification for details on the format of
this string, and ogr_featurestyle.h for services available to parse
it.

This function is the same as the C++ method
OGRFeature::GetStyleString().

Parameters:
-----------

hFeat:  handle to the feature to get the style from.

a reference to a representation in string format, or NULL if there
isn't one. ";

%feature("docstring")  SetStyleString "void
OGR_F_SetStyleString(OGRFeatureH hFeat, const char *pszStyle)

Set feature style string.

This method operate exactly as OGR_F_SetStyleStringDirectly() except
that it does not assume ownership of the passed string, but instead
makes a copy of it.

This function is the same as the C++ method
OGRFeature::SetStyleString().

Parameters:
-----------

hFeat:  handle to the feature to set style to.

pszStyle:  the style string to apply to this feature, cannot be NULL.
";

%feature("docstring")  SetStyleStringDirectly "void
OGR_F_SetStyleStringDirectly(OGRFeatureH hFeat, char *pszStyle)

Set feature style string.

This method operate exactly as OGR_F_SetStyleString() except that it
assumes ownership of the passed string.

This function is the same as the C++ method
OGRFeature::SetStyleStringDirectly().

Parameters:
-----------

hFeat:  handle to the feature to set style to.

pszStyle:  the style string to apply to this feature, cannot be NULL.
";

%feature("docstring")  GetStyleTable "OGRStyleTableH
OGR_F_GetStyleTable(OGRFeatureH hFeat)

Return style table. ";

%feature("docstring")  SetStyleTableDirectly "void
OGR_F_SetStyleTableDirectly(OGRFeatureH hFeat, OGRStyleTableH
hStyleTable)

Set style table and take ownership. ";

%feature("docstring")  SetStyleTable "void
OGR_F_SetStyleTable(OGRFeatureH hFeat, OGRStyleTableH hStyleTable)

Set style table. ";

%feature("docstring")  FillUnsetWithDefault "void
OGR_F_FillUnsetWithDefault(OGRFeatureH hFeat, int bNotNullableOnly,
char **papszOptions)

Fill unset fields with default values that might be defined.

This function is the same as the C++ method
OGRFeature::FillUnsetWithDefault().

Parameters:
-----------

hFeat:  handle to the feature.

bNotNullableOnly:  if we should fill only unset fields with a not-null
constraint.

papszOptions:  unused currently. Must be set to NULL.

GDAL 2.0 ";

%feature("docstring")  Validate "int OGR_F_Validate(OGRFeatureH
hFeat, int nValidateFlags, int bEmitError)

Validate that a feature meets constraints of its schema.

The scope of test is specified with the nValidateFlags parameter.

Regarding OGR_F_VAL_WIDTH, the test is done assuming the string width
must be interpreted as the number of UTF-8 characters. Some drivers
might interpret the width as the number of bytes instead. So this test
is rather conservative (if it fails, then it will fail for all
interpretations).

This function is the same as the C++ method OGRFeature::Validate().

Parameters:
-----------

hFeat:  handle to the feature to validate.

nValidateFlags:  OGR_F_VAL_ALL or combination of OGR_F_VAL_NULL,
OGR_F_VAL_GEOM_TYPE, OGR_F_VAL_WIDTH and
OGR_F_VAL_ALLOW_NULL_WHEN_DEFAULT with '|' operator

bEmitError:  TRUE if a CPLError() must be emitted when a check fails

TRUE if all enabled validation tests pass.

GDAL 2.0 ";

%feature("docstring")  GetNativeData "const char*
OGR_F_GetNativeData(OGRFeatureH hFeat)

Returns the native data for the feature.

The native data is the representation in a \"natural\" form that comes
from the driver that created this feature, or that is aimed at an
output driver. The native data may be in different format, which is
indicated by OGR_F_GetNativeMediaType().

Note that most drivers do not support storing the native data in the
feature object, and if they do, generally the NATIVE_DATA open option
must be passed at dataset opening.

The \"native data\" does not imply it is something more performant or
powerful than what can be obtained with the rest of the API, but it
may be useful in round-tripping scenarios where some characteristics
of the underlying format are not captured otherwise by the OGR
abstraction.

This function is the same as the C++ method
OGRFeature::GetNativeData().

Parameters:
-----------

hFeat:  handle to the feature.

a string with the native data, or NULL if there is none.

GDAL 2.1

See:
https://trac.osgeo.org/gdal/wiki/rfc60_improved_roundtripping_in_ogr
";

%feature("docstring")  GetNativeMediaType "const char*
OGR_F_GetNativeMediaType(OGRFeatureH hFeat)

Returns the native media type for the feature.

The native media type is the identifier for the format of the native
data. It follows the IANA RFC 2045
(seehttps://en.wikipedia.org/wiki/Media_type), e.g.
\"application/vnd.geo+json\" for JSon.

This function is the same as the C function
OGR_F_GetNativeMediaType().

Parameters:
-----------

hFeat:  handle to the feature.

a string with the native media type, or NULL if there is none.

GDAL 2.1

See:
https://trac.osgeo.org/gdal/wiki/rfc60_improved_roundtripping_in_ogr
";

%feature("docstring")  SetNativeData "void
OGR_F_SetNativeData(OGRFeatureH hFeat, const char *pszNativeData)

Sets the native data for the feature.

The native data is the representation in a \"natural\" form that comes
from the driver that created this feature, or that is aimed at an
output driver. The native data may be in different format, which is
indicated by OGR_F_GetNativeMediaType().

This function is the same as the C++ method
OGRFeature::SetNativeData().

Parameters:
-----------

hFeat:  handle to the feature.

pszNativeData:  a string with the native data, or NULL if there is
none.

GDAL 2.1

See:
https://trac.osgeo.org/gdal/wiki/rfc60_improved_roundtripping_in_ogr
";

%feature("docstring")  SetNativeMediaType "void
OGR_F_SetNativeMediaType(OGRFeatureH hFeat, const char
*pszNativeMediaType)

Sets the native media type for the feature.

The native media type is the identifier for the format of the native
data. It follows the IANA RFC 2045
(seehttps://en.wikipedia.org/wiki/Media_type), e.g.
\"application/vnd.geo+json\" for JSon.

This function is the same as the C++ method
OGRFeature::SetNativeMediaType().

Parameters:
-----------

hFeat:  handle to the feature.

pszNativeMediaType:  a string with the native media type, or NULL if
there is none.

GDAL 2.1

See:
https://trac.osgeo.org/gdal/wiki/rfc60_improved_roundtripping_in_ogr
";

%feature("docstring")  OGR_RawField_IsUnset "int
OGR_RawField_IsUnset(const OGRField *puField)

Returns whether a raw field is unset.

Note: this function is rather low-level and should be rarely used in
client code. Use instead OGR_F_IsFieldSet().

Parameters:
-----------

puField:  pointer to raw field.

GDAL 2.2 ";

%feature("docstring")  OGR_RawField_IsNull "int
OGR_RawField_IsNull(const OGRField *puField)

Returns whether a raw field is null.

Note: this function is rather low-level and should be rarely used in
client code. Use instead OGR_F_IsFieldNull().

Parameters:
-----------

puField:  pointer to raw field.

GDAL 2.2 ";

%feature("docstring")  OGR_RawField_SetUnset "void
OGR_RawField_SetUnset(OGRField *puField)

Mark a raw field as unset.

This should be called on a un-initialized field. In particular this
will not free any memory dynamically allocated.

Note: this function is rather low-level and should be rarely used in
client code. Use instead OGR_F_UnsetField().

Parameters:
-----------

puField:  pointer to raw field.

GDAL 2.2 ";

%feature("docstring")  OGR_RawField_SetNull "void
OGR_RawField_SetNull(OGRField *puField)

Mark a raw field as null.

This should be called on a un-initialized field. In particular this
will not free any memory dynamically allocated.

Note: this function is rather low-level and should be rarely used in
client code. Use instead OGR_F_SetFieldNull().

Parameters:
-----------

puField:  pointer to raw field.

GDAL 2.2 ";

}