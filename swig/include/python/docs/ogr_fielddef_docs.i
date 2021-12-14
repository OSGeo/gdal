%extend OGRFieldDefnShadow {
// File: ogrfielddefn_8cpp.xml
%feature("docstring")  Create "OGRFieldDefnH OGR_Fld_Create(const
char *pszName, OGRFieldType eType)

Create a new field definition.

By default, fields have no width, precision, are nullable and not
ignored.

This function is the same as the CPP method
OGRFieldDefn::OGRFieldDefn().

Parameters:
-----------

pszName:  the name of the new field definition.

eType:  the type of the new field definition.

handle to the new field definition. ";

%feature("docstring")  Destroy "void OGR_Fld_Destroy(OGRFieldDefnH
hDefn)

Destroy a field definition.

Parameters:
-----------

hDefn:  handle to the field definition to destroy. ";

%feature("docstring")  SetName "void OGR_Fld_SetName(OGRFieldDefnH
hDefn, const char *pszName)

Reset the name of this field.

This function is the same as the CPP method OGRFieldDefn::SetName().

Parameters:
-----------

hDefn:  handle to the field definition to apply the new name to.

pszName:  the new name to apply. ";

%feature("docstring")  GetNameRef "const char*
OGR_Fld_GetNameRef(OGRFieldDefnH hDefn)

Fetch name of this field.

This function is the same as the CPP method
OGRFieldDefn::GetNameRef().

Parameters:
-----------

hDefn:  handle to the field definition.

the name of the field definition. ";

%feature("docstring")  SetAlternativeName "void
OGR_Fld_SetAlternativeName(OGRFieldDefnH hDefn, const char
*pszAlternativeName)

Reset the alternative name (or \"alias\") for this field.

The alternative name is an optional attribute for a field which can
provide a more user-friendly, descriptive name of a field which is not
subject to the usual naming constraints defined by the data provider.

This is a metadata style attribute only: the alternative name cannot
be used in place of the actual field name during SQL queries or other
field name dependent API calls.

This function is the same as the CPP method
OGRFieldDefn::SetAlternativeName().

Parameters:
-----------

hDefn:  handle to the field definition to apply the new alternative
name to.

pszAlternativeName:  the new alternative name to apply.

GDAL 3.2 ";

%feature("docstring")  GetAlternativeNameRef "const char*
OGR_Fld_GetAlternativeNameRef(OGRFieldDefnH hDefn)

Fetch the alternative name (or \"alias\") for this field.

The alternative name is an optional attribute for a field which can
provide a more user-friendly, descriptive name of a field which is not
subject to the usual naming constraints defined by the data provider.

This is a metadata style attribute only: the alternative name cannot
be used in place of the actual field name during SQL queries or other
field name dependent API calls.

This function is the same as the CPP method
OGRFieldDefn::GetAlternativeNameRef().

Parameters:
-----------

hDefn:  handle to the field definition.

the alternative name of the field definition.

GDAL 3.2 ";

%feature("docstring")  GetType "OGRFieldType
OGR_Fld_GetType(OGRFieldDefnH hDefn)

Fetch type of this field.

This function is the same as the CPP method OGRFieldDefn::GetType().

Parameters:
-----------

hDefn:  handle to the field definition to get type from.

field type. ";

%feature("docstring")  SetType "void OGR_Fld_SetType(OGRFieldDefnH
hDefn, OGRFieldType eType)

Set the type of this field.

This should never be done to an OGRFieldDefn that is already part of
an OGRFeatureDefn.

This function is the same as the CPP method OGRFieldDefn::SetType().

Parameters:
-----------

hDefn:  handle to the field definition to set type to.

eType:  the new field type. ";

%feature("docstring")  GetSubType "OGRFieldSubType
OGR_Fld_GetSubType(OGRFieldDefnH hDefn)

Fetch subtype of this field.

This function is the same as the CPP method
OGRFieldDefn::GetSubType().

Parameters:
-----------

hDefn:  handle to the field definition to get subtype from.

field subtype.

GDAL 2.0 ";

%feature("docstring")  SetSubType "void
OGR_Fld_SetSubType(OGRFieldDefnH hDefn, OGRFieldSubType eSubType)

Set the subtype of this field.

This should never be done to an OGRFieldDefn that is already part of
an OGRFeatureDefn.

This function is the same as the CPP method
OGRFieldDefn::SetSubType().

Parameters:
-----------

hDefn:  handle to the field definition to set type to.

eSubType:  the new field subtype.

GDAL 2.0 ";

%feature("docstring")  SetDefault "void
OGR_Fld_SetDefault(OGRFieldDefnH hDefn, const char *pszDefault)

Set default field value.

The default field value is taken into account by drivers (generally
those with a SQL interface) that support it at field creation time.
OGR will generally not automatically set the default field value to
null fields by itself when calling OGRFeature::CreateFeature() /
OGRFeature::SetFeature(), but will let the low-level layers to do the
job. So retrieving the feature from the layer is recommended.

The accepted values are NULL, a numeric value, a literal value
enclosed between single quote characters (and inner single quote
characters escaped by repetition of the single quote character),
CURRENT_TIMESTAMP, CURRENT_TIME, CURRENT_DATE or a driver specific
expression (that might be ignored by other drivers). For a datetime
literal value, format should be 'YYYY/MM/DD HH:MM:SS[.sss]'
(considered as UTC time).

Drivers that support writing DEFAULT clauses will advertise the
GDAL_DCAP_DEFAULT_FIELDS driver metadata item.

This function is the same as the C++ method
OGRFieldDefn::SetDefault().

Parameters:
-----------

hDefn:  handle to the field definition.

pszDefault:  new default field value or NULL pointer.

GDAL 2.0 ";

%feature("docstring")  GetDefault "const char*
OGR_Fld_GetDefault(OGRFieldDefnH hDefn)

Get default field value.

This function is the same as the C++ method
OGRFieldDefn::GetDefault().

Parameters:
-----------

hDefn:  handle to the field definition.

default field value or NULL.

GDAL 2.0 ";

%feature("docstring")  IsDefaultDriverSpecific "int
OGR_Fld_IsDefaultDriverSpecific(OGRFieldDefnH hDefn)

Returns whether the default value is driver specific.

Driver specific default values are those that are not NULL, a numeric
value, a literal value enclosed between single quote characters,
CURRENT_TIMESTAMP, CURRENT_TIME, CURRENT_DATE or datetime literal
value.

This function is the same as the C++ method
OGRFieldDefn::IsDefaultDriverSpecific().

Parameters:
-----------

hDefn:  handle to the field definition

TRUE if the default value is driver specific.

GDAL 2.0 ";

%feature("docstring")  OGR_GetFieldTypeName "const char*
OGR_GetFieldTypeName(OGRFieldType eType)

Fetch human readable name for a field type.

This function is the same as the CPP method
OGRFieldDefn::GetFieldTypeName().

Parameters:
-----------

eType:  the field type to get name for.

the name. ";

%feature("docstring")  OGR_GetFieldSubTypeName "const char*
OGR_GetFieldSubTypeName(OGRFieldSubType eSubType)

Fetch human readable name for a field subtype.

This function is the same as the CPP method
OGRFieldDefn::GetFieldSubTypeName().

Parameters:
-----------

eSubType:  the field subtype to get name for.

the name.

GDAL 2.0 ";

%feature("docstring")  OGR_AreTypeSubTypeCompatible "int
OGR_AreTypeSubTypeCompatible(OGRFieldType eType, OGRFieldSubType
eSubType)

Return if type and subtype are compatible.

Parameters:
-----------

eType:  the field type.

eSubType:  the field subtype.

TRUE if type and subtype are compatible

GDAL 2.0 ";

%feature("docstring")  GetJustify "OGRJustification
OGR_Fld_GetJustify(OGRFieldDefnH hDefn)

Get the justification for this field.

This function is the same as the CPP method
OGRFieldDefn::GetJustify().

Note: no driver is know to use the concept of field justification.

Parameters:
-----------

hDefn:  handle to the field definition to get justification from.

the justification. ";

%feature("docstring")  SetJustify "void
OGR_Fld_SetJustify(OGRFieldDefnH hDefn, OGRJustification eJustify)

Set the justification for this field.

Note: no driver is know to use the concept of field justification.

This function is the same as the CPP method
OGRFieldDefn::SetJustify().

Parameters:
-----------

hDefn:  handle to the field definition to set justification to.

eJustify:  the new justification. ";

%feature("docstring")  GetWidth "int OGR_Fld_GetWidth(OGRFieldDefnH
hDefn)

Get the formatting width for this field.

This function is the same as the CPP method OGRFieldDefn::GetWidth().

Parameters:
-----------

hDefn:  handle to the field definition to get width from.

the width, zero means no specified width. ";

%feature("docstring")  SetWidth "void OGR_Fld_SetWidth(OGRFieldDefnH
hDefn, int nNewWidth)

Set the formatting width for this field in characters.

This function is the same as the CPP method OGRFieldDefn::SetWidth().

Parameters:
-----------

hDefn:  handle to the field definition to set width to.

nNewWidth:  the new width. ";

%feature("docstring")  GetPrecision "int
OGR_Fld_GetPrecision(OGRFieldDefnH hDefn)

Get the formatting precision for this field.

This should normally be zero for fields of types other than OFTReal.

This function is the same as the CPP method
OGRFieldDefn::GetPrecision().

Parameters:
-----------

hDefn:  handle to the field definition to get precision from.

the precision. ";

%feature("docstring")  SetPrecision "void
OGR_Fld_SetPrecision(OGRFieldDefnH hDefn, int nPrecision)

Set the formatting precision for this field in characters.

This should normally be zero for fields of types other than OFTReal.

This function is the same as the CPP method
OGRFieldDefn::SetPrecision().

Parameters:
-----------

hDefn:  handle to the field definition to set precision to.

nPrecision:  the new precision. ";

%feature("docstring")  Set "void OGR_Fld_Set(OGRFieldDefnH hDefn,
const char *pszNameIn, OGRFieldType eTypeIn, int nWidthIn, int
nPrecisionIn, OGRJustification eJustifyIn)

Set defining parameters for a field in one call.

This function is the same as the CPP method OGRFieldDefn::Set().

Parameters:
-----------

hDefn:  handle to the field definition to set to.

pszNameIn:  the new name to assign.

eTypeIn:  the new type (one of the OFT values like OFTInteger).

nWidthIn:  the preferred formatting width. Defaults to zero indicating
undefined.

nPrecisionIn:  number of decimals places for formatting, defaults to
zero indicating undefined.

eJustifyIn:  the formatting justification (OJLeft or OJRight),
defaults to OJUndefined. ";

%feature("docstring")  IsIgnored "int OGR_Fld_IsIgnored(OGRFieldDefnH
hDefn)

Return whether this field should be omitted when fetching features.

This method is the same as the C++ method OGRFieldDefn::IsIgnored().

Parameters:
-----------

hDefn:  handle to the field definition

ignore state ";

%feature("docstring")  SetIgnored "void
OGR_Fld_SetIgnored(OGRFieldDefnH hDefn, int ignore)

Set whether this field should be omitted when fetching features.

This method is the same as the C++ method OGRFieldDefn::SetIgnored().

Parameters:
-----------

hDefn:  handle to the field definition

ignore:  ignore state ";

%feature("docstring")  IsNullable "int
OGR_Fld_IsNullable(OGRFieldDefnH hDefn)

Return whether this field can receive null values.

By default, fields are nullable.

Even if this method returns FALSE (i.e not-nullable field), it doesn't
mean that OGRFeature::IsFieldSet() will necessary return TRUE, as
fields can be temporary unset and null/not-null validation is usually
done when OGRLayer::CreateFeature()/SetFeature() is called.

This method is the same as the C++ method OGRFieldDefn::IsNullable().

Parameters:
-----------

hDefn:  handle to the field definition

TRUE if the field is authorized to be null.

GDAL 2.0 ";

%feature("docstring")  SetNullable "void
OGR_Fld_SetNullable(OGRFieldDefnH hDefn, int bNullableIn)

Set whether this field can receive null values.

By default, fields are nullable, so this method is generally called
with FALSE to set a not-null constraint.

Drivers that support writing not-null constraint will advertise the
GDAL_DCAP_NOTNULL_FIELDS driver metadata item.

This method is the same as the C++ method OGRFieldDefn::SetNullable().

Parameters:
-----------

hDefn:  handle to the field definition

bNullableIn:  FALSE if the field must have a not-null constraint.

GDAL 2.0 ";

%feature("docstring")  IsUnique "int OGR_Fld_IsUnique(OGRFieldDefnH
hDefn)

Return whether this field has a unique constraint.

By default, fields have no unique constraint.

This method is the same as the C++ method OGRFieldDefn::IsUnique().

Parameters:
-----------

hDefn:  handle to the field definition

TRUE if the field has a unique constraint.

GDAL 3.2 ";

%feature("docstring")  SetUnique "void
OGR_Fld_SetUnique(OGRFieldDefnH hDefn, int bUniqueIn)

Set whether this field has a unique constraint.

By default, fields have no unique constraint, so this method is
generally called with TRUE to set a unique constraint.

Drivers that support writing unique constraint will advertise the
GDAL_DCAP_UNIQUE_FIELDS driver metadata item. field can receive null
values.

This method is the same as the C++ method OGRFieldDefn::SetUnique().

Parameters:
-----------

hDefn:  handle to the field definition

bUniqueIn:  TRUE if the field must have a unique constraint.

GDAL 3.2 ";

%feature("docstring")  GetDomainName "const char*
OGR_Fld_GetDomainName(OGRFieldDefnH hDefn)

Return the name of the field domain for this field.

By default, none (empty string) is returned.

Field domains ( OGRFieldDomain class) are attached at the GDALDataset
level and should be retrieved with GDALDatasetGetFieldDomain().

This method is the same as the C++ method
OGRFieldDefn::GetDomainName().

Parameters:
-----------

hDefn:  handle to the field definition

the field domain name, or an empty string if there is none.

GDAL 3.3 ";

%feature("docstring")  SetDomainName "void
OGR_Fld_SetDomainName(OGRFieldDefnH hDefn, const char *pszFieldName)

Set the name of the field domain for this field.

Field domains ( OGRFieldDomain) are attached at the GDALDataset level.

This method is the same as the C++ method
OGRFieldDefn::SetDomainName().

Parameters:
-----------

hDefn:  handle to the field definition

pszFieldName:  Field domain name.

GDAL 3.3 ";

%feature("docstring")  OGRUpdateFieldType "void
OGRUpdateFieldType(OGRFieldDefn *poFDefn, OGRFieldType eNewType,
OGRFieldSubType eNewSubType)

Update the type of a field definition by \"merging\" its existing type
with a new type.

The update is done such as broadening the type. For example a
OFTInteger updated with OFTInteger64 will be promoted to OFTInteger64.

Parameters:
-----------

poFDefn:  the field definition whose type must be updated.

eNewType:  the new field type to merge into the existing type.

eNewSubType:  the new field subtype to merge into the existing
subtype.

GDAL 2.1 ";

%feature("docstring")  OGR_FldDomain_Destroy "void
OGR_FldDomain_Destroy(OGRFieldDomainH hFieldDomain)

Destroy a field domain.

This is the same as the C++ method OGRFieldDomain::~OGRFieldDomain()

Parameters:
-----------

hFieldDomain:  the field domain.

GDAL 3.3 ";

%feature("docstring")  OGR_CodedFldDomain_Create "OGRFieldDomainH
OGR_CodedFldDomain_Create(const char *pszName, const char
*pszDescription, OGRFieldType eFieldType, OGRFieldSubType
eFieldSubType, const OGRCodedValue *enumeration)

Creates a new coded field domain.

This is the same as the C++ method
OGRCodedFieldDomain::OGRCodedFieldDomain() (except that the C function
copies the enumeration, whereas the C++ method moves it)

Parameters:
-----------

pszName:  Domain name. Should not be NULL.

pszDescription:  Domain description (can be NULL)

eFieldType:  Field type. Generally numeric. Potentially OFTDateTime

eFieldSubType:  Field subtype.

enumeration:  Enumeration as (code, value) pairs. Should not be NULL.
The end of the enumeration is marked by a code set to NULL. The
enumeration will be copied. Each code should appear only once, but it
is the responsibility of the user to check it.

a new handle that should be freed with OGR_FldDomain_Destroy(), or
NULL in case of error.

GDAL 3.3 ";

%feature("docstring")  GetUnsetField "static OGRField GetUnsetField()
";

%feature("docstring")  OGR_RangeFldDomain_Create "OGRFieldDomainH
OGR_RangeFldDomain_Create(const char *pszName, const char
*pszDescription, OGRFieldType eFieldType, OGRFieldSubType
eFieldSubType, const OGRField *psMin, bool bMinIsInclusive, const
OGRField *psMax, bool bMaxIsInclusive)

Creates a new range field domain.

This is the same as the C++ method
OGRRangeFieldDomain::OGRRangeFieldDomain().

Parameters:
-----------

pszName:  Domain name. Should not be NULL.

pszDescription:  Domain description (can be NULL)

eFieldType:  Field type. Among OFTInteger, OFTInteger64, OFTReal and
OFTDateTime.

eFieldSubType:  Field subtype.

psMin:  Minimum value (can be NULL). The member in the union that is
read is consistent with eFieldType

bMinIsInclusive:  Whether the minimum value is included in the range.

psMax:  Maximum value (can be NULL). The member in the union that is
read is consistent with eFieldType

bMaxIsInclusive:  Whether the maximum value is included in the range.

a new handle that should be freed with OGR_FldDomain_Destroy()

GDAL 3.3 ";

%feature("docstring")  OGR_GlobFldDomain_Create "OGRFieldDomainH
OGR_GlobFldDomain_Create(const char *pszName, const char
*pszDescription, OGRFieldType eFieldType, OGRFieldSubType
eFieldSubType, const char *pszGlob)

Creates a new blob field domain.

This is the same as the C++ method
OGRGlobFieldDomain::OGRGlobFieldDomain()

Parameters:
-----------

pszName:  Domain name. Should not be NULL.

pszDescription:  Domain description (can be NULL)

eFieldType:  Field type.

eFieldSubType:  Field subtype.

pszGlob:  Glob expression. Should not be NULL.

a new handle that should be freed with OGR_FldDomain_Destroy()

GDAL 3.3 ";

%feature("docstring")  OGR_FldDomain_GetName "const char*
OGR_FldDomain_GetName(OGRFieldDomainH hFieldDomain)

Get the name of the field domain.

This is the same as the C++ method OGRFieldDomain::GetName()

Parameters:
-----------

hFieldDomain:  Field domain handle.

the field domain name.

GDAL 3.3 ";

%feature("docstring")  OGR_FldDomain_GetDescription "const char*
OGR_FldDomain_GetDescription(OGRFieldDomainH hFieldDomain)

Get the description of the field domain.

This is the same as the C++ method OGRFieldDomain::GetDescription()

Parameters:
-----------

hFieldDomain:  Field domain handle.

the field domain description (might be empty string).

GDAL 3.3 ";

%feature("docstring")  OGR_FldDomain_GetDomainType "OGRFieldDomainType OGR_FldDomain_GetDomainType(OGRFieldDomainH
hFieldDomain)

Get the type of the field domain.

This is the same as the C++ method OGRFieldDomain::GetDomainType()

Parameters:
-----------

hFieldDomain:  Field domain handle.

the type of the field domain.

GDAL 3.3 ";

%feature("docstring")  OGR_FldDomain_GetFieldType "OGRFieldType
OGR_FldDomain_GetFieldType(OGRFieldDomainH hFieldDomain)

Get the field type of the field domain.

This is the same as the C++ method OGRFieldDomain::GetFieldType()

Parameters:
-----------

hFieldDomain:  Field domain handle.

the field type of the field domain.

GDAL 3.3 ";

%feature("docstring")  OGR_FldDomain_GetFieldSubType "OGRFieldSubType
OGR_FldDomain_GetFieldSubType(OGRFieldDomainH hFieldDomain)

Get the field subtype of the field domain.

This is the same as OGRFieldDomain::GetFieldSubType()

Parameters:
-----------

hFieldDomain:  Field domain handle.

the field subtype of the field domain.

GDAL 3.3 ";

%feature("docstring")  OGR_FldDomain_GetSplitPolicy "OGRFieldDomainSplitPolicy OGR_FldDomain_GetSplitPolicy(OGRFieldDomainH
hFieldDomain)

Get the split policy of the field domain.

This is the same as the C++ method OGRFieldDomain::GetSplitPolicy()

Parameters:
-----------

hFieldDomain:  Field domain handle.

the split policy of the field domain.

GDAL 3.3 ";

%feature("docstring")  OGR_FldDomain_SetSplitPolicy "void
OGR_FldDomain_SetSplitPolicy(OGRFieldDomainH hFieldDomain,
OGRFieldDomainSplitPolicy policy)

Set the split policy of the field domain.

This is the same as the C++ method OGRFieldDomain::SetSplitPolicy()

Parameters:
-----------

hFieldDomain:  Field domain handle.

policy:  the split policy of the field domain.

GDAL 3.3 ";

%feature("docstring")  OGR_FldDomain_GetMergePolicy "OGRFieldDomainMergePolicy OGR_FldDomain_GetMergePolicy(OGRFieldDomainH
hFieldDomain)

Get the split policy of the field domain.

This is the same as the C++ method OGRFieldDomain::GetMergePolicy()

Parameters:
-----------

hFieldDomain:  Field domain handle.

the split policy of the field domain.

GDAL 3.3 ";

%feature("docstring")  OGR_FldDomain_SetMergePolicy "void
OGR_FldDomain_SetMergePolicy(OGRFieldDomainH hFieldDomain,
OGRFieldDomainMergePolicy policy)

Set the split policy of the field domain.

This is the same as the C++ method OGRFieldDomain::SetMergePolicy()

Parameters:
-----------

hFieldDomain:  Field domain handle.

policy:  the split policy of the field domain.

GDAL 3.3 ";

%feature("docstring")  OGR_CodedFldDomain_GetEnumeration "const
OGRCodedValue* OGR_CodedFldDomain_GetEnumeration(OGRFieldDomainH
hFieldDomain)

Get the enumeration as (code, value) pairs.

The end of the enumeration is signaled by code == NULL

This is the same as the C++ method
OGRCodedFieldDomain::GetEnumeration()

Parameters:
-----------

hFieldDomain:  Field domain handle.

the (code, value) pairs, or nullptr in case of error.

GDAL 3.3 ";

%feature("docstring")  OGR_RangeFldDomain_GetMin "const OGRField*
OGR_RangeFldDomain_GetMin(OGRFieldDomainH hFieldDomain, bool
*pbIsInclusiveOut)

Get the minimum value.

Which member in the returned OGRField enum must be read depends on the
field type.

If no minimum value is set, the OGR_RawField_IsUnset() will return
true when called on the result.

This is the same as the C++ method OGRRangeFieldDomain::GetMin()

Parameters:
-----------

hFieldDomain:  Field domain handle.

pbIsInclusiveOut:  set to true if the minimum is included in the
range.

the minimum value.

GDAL 3.3 ";

%feature("docstring")  OGR_RangeFldDomain_GetMax "const OGRField*
OGR_RangeFldDomain_GetMax(OGRFieldDomainH hFieldDomain, bool
*pbIsInclusiveOut)

Get the maximum value.

Which member in the returned OGRField enum must be read depends on the
field type.

If no maximum value is set, the OGR_RawField_IsUnset() will return
true when called on the result.

This is the same as the C++ method OGRRangeFieldDomain::GetMax()

Parameters:
-----------

hFieldDomain:  Field domain handle.

pbIsInclusiveOut:  set to true if the maximum is included in the
range.

the maximum value.

GDAL 3.3 ";

%feature("docstring")  OGR_GlobFldDomain_GetGlob "const char*
OGR_GlobFldDomain_GetGlob(OGRFieldDomainH hFieldDomain)

Get the glob expression.

This is the same as the C++ method OGRGlobFieldDomain::GetGlob()

Parameters:
-----------

hFieldDomain:  Field domain handle.

the glob expression, or nullptr in case of error

GDAL 3.3 ";

}