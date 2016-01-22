%extend OGRFieldDefnShadow {
// File: ogrfielddefn_8cpp.xml
%feature("docstring")  CPL_CVSID "CPL_CVSID(\"$Id: ogrfielddefn.cpp
21018 2010-10-30 11:30:51Z rouault $\") ";

%feature("docstring")  Create "OGRFieldDefnH OGR_Fld_Create(const
char *pszName, OGRFieldType eType)

Create a new field definition.

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

Set the type of this field. This should never be done to an
OGRFieldDefn that is already part of an OGRFeatureDefn.

This function is the same as the CPP method OGRFieldDefn::SetType().

Parameters:
-----------

hDefn:  handle to the field definition to set type to.

eType:  the new field type. ";

%feature("docstring")  OGR_GetFieldTypeName "const char*
OGR_GetFieldTypeName(OGRFieldType eType)

Fetch human readable name for a field type.

This function is the same as the CPP method
OGRFieldDefn::GetFieldTypeName().

Parameters:
-----------

eType:  the field type to get name for.

the name. ";

%feature("docstring")  GetJustify "OGRJustification
OGR_Fld_GetJustify(OGRFieldDefnH hDefn)

Get the justification for this field.

This function is the same as the CPP method
OGRFieldDefn::GetJustify().

Parameters:
-----------

hDefn:  handle to the field definition to get justification from.

the justification. ";

%feature("docstring")  SetJustify "void
OGR_Fld_SetJustify(OGRFieldDefnH hDefn, OGRJustification eJustify)

Set the justification for this field.

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

Get the formatting precision for this field. This should normally be
zero for fields of types other than OFTReal.

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

This method is the same as the C function OGRFieldDefn::SetIgnored().

Parameters:
-----------

hDefn:  handle to the field definition

ignore:  ignore state ";

}