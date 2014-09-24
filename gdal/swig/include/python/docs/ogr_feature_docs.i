%extend OGRFeatureShadow {
// File: ogrfeature_8cpp.xml
%feature("docstring")  CPL_CVSID "CPL_CVSID(\"$Id: ogrfeature.cpp
23414 2011-11-23 05:50:05Z warmerdam $\") ";

%feature("docstring")  Create "OGRFeatureH
OGR_F_Create(OGRFeatureDefnH hDefn)

Feature factory.

Note that the OGRFeature will increment the reference count of it's
defining OGRFeatureDefn. Destruction of the OGRFeatureDefn before
destruction of all OGRFeatures that depend on it is likely to result
in a crash.

This function is the same as the C++ method OGRFeature::OGRFeature().

Parameters:
-----------

hDefn:  handle to the feature class (layer) definition to which the
feature will adhere.

an handle to the new feature object with null fields and no geometry.
";

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

an handle to the feature definition object on which feature depends.
";

%feature("docstring")  SetGeometryDirectly "OGRErr
OGR_F_SetGeometryDirectly(OGRFeatureH hFeat, OGRGeometryH hGeom)

Set feature geometry.

This function updates the features geometry, and operate exactly as
SetGeometry(), except that this function assumes ownership of the
passed geometry.

This function is the same as the C++ method
OGRFeature::SetGeometryDirectly.

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
take over ownship of the geometry from the feature without copying.
Sort of an inverse to OGR_FSetGeometryDirectly().

After this call the OGRFeature will have a NULL geometry.

the pointer to the geometry. ";

%feature("docstring")  GetGeometryRef "OGRGeometryH
OGR_F_GetGeometryRef(OGRFeatureH hFeat)

Fetch an handle to feature geometry.

This function is the same as the C++ method
OGRFeature::GetGeometryRef().

Parameters:
-----------

hFeat:  handle to the feature to get geometry from.

an handle to internal feature geometry. This object should not be
modified. ";

%feature("docstring")  Clone "OGRFeatureH OGR_F_Clone(OGRFeatureH
hFeat)

Duplicate feature.

The newly created feature is owned by the caller, and will have it's
own reference to the OGRFeatureDefn.

This function is the same as the C++ method OGRFeature::Clone().

Parameters:
-----------

hFeat:  handle to the feature to clone.

an handle to the new feature, exactly matching this feature. ";

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

an handle to the field definition (from the OGRFeatureDefn). This is
an internal reference, and should not be deleted or modified. ";

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

%feature("docstring")  GetRawFieldRef "OGRField*
OGR_F_GetRawFieldRef(OGRFeatureH hFeat, int iField)

Fetch an handle to the internal field value given the index.

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

%feature("docstring")  GetFieldAsDouble "double
OGR_F_GetFieldAsDouble(OGRFeatureH hFeat, int iField)

Fetch field value as a double.

OFTString features will be translated using atof(). OFTInteger fields
will be cast to double. Other field types, or errors will result in a
return value of zero.

This function is the same as the C++ method
OGRFeature::GetFieldAsDouble().

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to fetch, from 0 to GetFieldCount()-1.

the field value. ";

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

Currently this method only works for OFTBinary fields.

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

TRUE on success or FALSE on failure. ";

%feature("docstring")  SetFieldInteger "void
OGR_F_SetFieldInteger(OGRFeatureH hFeat, int iField, int nValue)

Set field to integer value.

OFTInteger and OFTReal fields will be set directly. OFTString fields
will be assigned a string representation of the value, but not
necessarily taking into account formatting constraints on this field.
Other field types may be unaffected.

This function is the same as the C++ method OGRFeature::SetField().

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to fetch, from 0 to GetFieldCount()-1.

nValue:  the value to assign. ";

%feature("docstring")  SetFieldDouble "void
OGR_F_SetFieldDouble(OGRFeatureH hFeat, int iField, double dfValue)

Set field to double value.

OFTInteger and OFTReal fields will be set directly. OFTString fields
will be assigned a string representation of the value, but not
necessarily taking into account formatting constraints on this field.
Other field types may be unaffected.

This function is the same as the C++ method OGRFeature::SetField().

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
string. OFTReal fields will be set based on an atof() conversion of
the string. Other field types may be unaffected.

This function is the same as the C++ method OGRFeature::SetField().

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to fetch, from 0 to GetFieldCount()-1.

pszValue:  the value to assign. ";

%feature("docstring")  SetFieldIntegerList "void
OGR_F_SetFieldIntegerList(OGRFeatureH hFeat, int iField, int nCount,
int *panValues)

Set field to list of integers value.

This function currently on has an effect of OFTIntegerList fields.

This function is the same as the C++ method OGRFeature::SetField().

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to set, from 0 to GetFieldCount()-1.

nCount:  the number of values in the list being assigned.

panValues:  the values to assign. ";

%feature("docstring")  SetFieldDoubleList "void
OGR_F_SetFieldDoubleList(OGRFeatureH hFeat, int iField, int nCount,
double *padfValues)

Set field to list of doubles value.

This function currently on has an effect of OFTRealList fields.

This function is the same as the C++ method OGRFeature::SetField().

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to set, from 0 to GetFieldCount()-1.

nCount:  the number of values in the list being assigned.

padfValues:  the values to assign. ";

%feature("docstring")  SetFieldStringList "void
OGR_F_SetFieldStringList(OGRFeatureH hFeat, int iField, char
**papszValues)

Set field to list of strings value.

This function currently on has an effect of OFTStringList fields.

This function is the same as the C++ method OGRFeature::SetField().

Parameters:
-----------

hFeat:  handle to the feature that owned the field.

iField:  the field to set, from 0 to GetFieldCount()-1.

papszValues:  the values to assign. ";

%feature("docstring")  SetFieldBinary "void
OGR_F_SetFieldBinary(OGRFeatureH hFeat, int iField, int nBytes, GByte
*pabyData)

Set field to binary data.

This function currently on has an effect of OFTBinary fields.

This function is the same as the C++ method OGRFeature::SetField().

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
details) ";

%feature("docstring")  SetFieldRaw "void
OGR_F_SetFieldRaw(OGRFeatureH hFeat, int iField, OGRField *psValue)

Set field.

The passed value OGRField must be of exactly the same type as the
target field, or an application crash may occur. The passed value is
copied, and will not be affected. It remains the responsibility of the
caller.

This function is the same as the C++ method OGRFeature::SetField().

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

%feature("docstring")  GetFID "long OGR_F_GetFID(OGRFeatureH hFeat)

Get feature identifier.

This function is the same as the C++ method OGRFeature::GetFID().

Parameters:
-----------

hFeat:  handle to the feature from which to get the feature
identifier.

feature id or OGRNullFID if none has been assigned. ";

%feature("docstring")  SetFID "OGRErr OGR_F_SetFID(OGRFeatureH hFeat,
long nFID)

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
bForgiving, int *panMap)

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

Set feature style string. This method operate exactly as
OGR_F_SetStyleStringDirectly() except that it does not assume
ownership of the passed string, but instead makes a copy of it.

This function is the same as the C++ method
OGRFeature::SetStyleString().

Parameters:
-----------

hFeat:  handle to the feature to set style to.

pszStyle:  the style string to apply to this feature, cannot be NULL.
";

%feature("docstring")  SetStyleStringDirectly "void
OGR_F_SetStyleStringDirectly(OGRFeatureH hFeat, char *pszStyle)

Set feature style string. This method operate exactly as
OGR_F_SetStyleString() except that it assumes ownership of the passed
string.

This function is the same as the C++ method
OGRFeature::SetStyleStringDirectly().

Parameters:
-----------

hFeat:  handle to the feature to set style to.

pszStyle:  the style string to apply to this feature, cannot be NULL.
";

%feature("docstring")  GetStyleTable "OGRStyleTableH
OGR_F_GetStyleTable(OGRFeatureH hFeat) ";

%feature("docstring")  SetStyleTableDirectly "void
OGR_F_SetStyleTableDirectly(OGRFeatureH hFeat, OGRStyleTableH
hStyleTable) ";

%feature("docstring")  SetStyleTable "void
OGR_F_SetStyleTable(OGRFeatureH hFeat, OGRStyleTableH hStyleTable) ";

}