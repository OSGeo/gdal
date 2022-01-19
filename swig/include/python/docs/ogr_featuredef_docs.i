%extend OGRFeatureDefnShadow {
// File: ogrfeaturedefn_8cpp.xml
%feature("docstring")  Create "OGRFeatureDefnH OGR_FD_Create(const
char *pszName)

Create a new feature definition object to hold the field definitions.

The OGRFeatureDefn maintains a reference count, but this starts at
zero, and should normally be incremented by the owner.

This function is the same as the C++ method
OGRFeatureDefn::OGRFeatureDefn().

Parameters:
-----------

pszName:  the name to be assigned to this layer/class. It does not
need to be unique.

handle to the newly created feature definition. ";

%feature("docstring")  Destroy "void OGR_FD_Destroy(OGRFeatureDefnH
hDefn)

Destroy a feature definition object and release all memory associated
with it.

This function is the same as the C++ method
OGRFeatureDefn::~OGRFeatureDefn().

Parameters:
-----------

hDefn:  handle to the feature definition to be destroyed. ";

%feature("docstring")  Release "void OGR_FD_Release(OGRFeatureDefnH
hDefn)

Drop a reference, and destroy if unreferenced.

This function is the same as the C++ method OGRFeatureDefn::Release().

Parameters:
-----------

hDefn:  handle to the feature definition to be released. ";

%feature("docstring")  GetName "const char*
OGR_FD_GetName(OGRFeatureDefnH hDefn)

Get name of the OGRFeatureDefn passed as an argument.

This function is the same as the C++ method OGRFeatureDefn::GetName().

Parameters:
-----------

hDefn:  handle to the feature definition to get the name from.

the name. This name is internal and should not be modified, or freed.
";

%feature("docstring")  GetFieldCount "int
OGR_FD_GetFieldCount(OGRFeatureDefnH hDefn)

Fetch number of fields on the passed feature definition.

This function is the same as the C++ OGRFeatureDefn::GetFieldCount().

Parameters:
-----------

hDefn:  handle to the feature definition to get the fields count from.

count of fields. ";

%feature("docstring")  GetFieldDefn "OGRFieldDefnH
OGR_FD_GetFieldDefn(OGRFeatureDefnH hDefn, int iField)

Fetch field definition of the passed feature definition.

This function is the same as the C++ method
OGRFeatureDefn::GetFieldDefn().

Parameters:
-----------

hDefn:  handle to the feature definition to get the field definition
from.

iField:  the field to fetch, between 0 and GetFieldCount()-1.

a handle to an internal field definition object or NULL if invalid
index. This object should not be modified or freed by the application.
";

%feature("docstring")  AddFieldDefn "void
OGR_FD_AddFieldDefn(OGRFeatureDefnH hDefn, OGRFieldDefnH hNewField)

Add a new field definition to the passed feature definition.

To add a new field definition to a layer definition, do not use this
function directly, but use OGR_L_CreateField() instead.

This function should only be called while there are no OGRFeature
objects in existence based on this OGRFeatureDefn. The OGRFieldDefn
passed in is copied, and remains the responsibility of the caller.

This function is the same as the C++ method
OGRFeatureDefn::AddFieldDefn().

Parameters:
-----------

hDefn:  handle to the feature definition to add the field definition
to.

hNewField:  handle to the new field definition. ";

%feature("docstring")  DeleteFieldDefn "OGRErr
OGR_FD_DeleteFieldDefn(OGRFeatureDefnH hDefn, int iField)

Delete an existing field definition.

To delete an existing field definition from a layer definition, do not
use this function directly, but use OGR_L_DeleteField() instead.

This method should only be called while there are no OGRFeature
objects in existence based on this OGRFeatureDefn.

This method is the same as the C++ method
OGRFeatureDefn::DeleteFieldDefn().

Parameters:
-----------

hDefn:  handle to the feature definition.

iField:  the index of the field definition.

OGRERR_NONE in case of success.

OGR 1.9.0 ";

%feature("docstring")  ReorderFieldDefns "OGRErr
OGR_FD_ReorderFieldDefns(OGRFeatureDefnH hDefn, const int *panMap)

Reorder the field definitions in the array of the feature definition.

To reorder the field definitions in a layer definition, do not use
this function directly, but use OGR_L_ReorderFields() instead.

This method should only be called while there are no OGRFeature
objects in existence based on this OGRFeatureDefn.

This method is the same as the C++ method
OGRFeatureDefn::ReorderFieldDefns().

Parameters:
-----------

hDefn:  handle to the feature definition.

panMap:  an array of GetFieldCount() elements which is a permutation
of [0, GetFieldCount()-1]. panMap is such that, for each field
definition at position i after reordering, its position before
reordering was panMap[i].

OGRERR_NONE in case of success.

OGR 2.1.0 ";

%feature("docstring")  GetGeomFieldCount "int
OGR_FD_GetGeomFieldCount(OGRFeatureDefnH hDefn)

Fetch number of geometry fields on the passed feature definition.

This function is the same as the C++
OGRFeatureDefn::GetGeomFieldCount().

Parameters:
-----------

hDefn:  handle to the feature definition to get the fields count from.

count of geometry fields.

GDAL 1.11 ";

%feature("docstring")  GetGeomFieldDefn "OGRGeomFieldDefnH
OGR_FD_GetGeomFieldDefn(OGRFeatureDefnH hDefn, int iGeomField)

Fetch geometry field definition of the passed feature definition.

This function is the same as the C++ method
OGRFeatureDefn::GetGeomFieldDefn().

Parameters:
-----------

hDefn:  handle to the feature definition to get the field definition
from.

iGeomField:  the geometry field to fetch, between 0 and
GetGeomFieldCount() - 1.

a handle to an internal field definition object or NULL if invalid
index. This object should not be modified or freed by the application.

GDAL 1.11 ";

%feature("docstring")  AddGeomFieldDefn "void
OGR_FD_AddGeomFieldDefn(OGRFeatureDefnH hDefn, OGRGeomFieldDefnH
hNewGeomField)

Add a new field definition to the passed feature definition.

To add a new field definition to a layer definition, do not use this
function directly, but use OGR_L_CreateGeomField() instead.

This function should only be called while there are no OGRFeature
objects in existence based on this OGRFeatureDefn. The
OGRGeomFieldDefn passed in is copied, and remains the responsibility
of the caller.

This function is the same as the C++ method
OGRFeatureDefn::AddGeomFieldDefn().

Parameters:
-----------

hDefn:  handle to the feature definition to add the geometry field
definition to.

hNewGeomField:  handle to the new field definition.

GDAL 1.11 ";

%feature("docstring")  DeleteGeomFieldDefn "OGRErr
OGR_FD_DeleteGeomFieldDefn(OGRFeatureDefnH hDefn, int iGeomField)

Delete an existing geometry field definition.

To delete an existing geometry field definition from a layer
definition, do not use this function directly, but use
OGR_L_DeleteGeomField() instead ( not implemented yet).

This method should only be called while there are no OGRFeature
objects in existence based on this OGRFeatureDefn.

This method is the same as the C++ method
OGRFeatureDefn::DeleteGeomFieldDefn().

Parameters:
-----------

hDefn:  handle to the feature definition.

iGeomField:  the index of the geometry field definition.

OGRERR_NONE in case of success.

GDAL 1.11 ";

%feature("docstring")  GetGeomFieldIndex "int
OGR_FD_GetGeomFieldIndex(OGRFeatureDefnH hDefn, const char
*pszGeomFieldName)

Find geometry field by name.

The geometry field index of the first geometry field matching the
passed field name (case insensitively) is returned.

This function is the same as the C++ method
OGRFeatureDefn::GetGeomFieldIndex.

Parameters:
-----------

hDefn:  handle to the feature definition to get field index from.

pszGeomFieldName:  the geometry field name to search for.

the geometry field index, or -1 if no match found. ";

%feature("docstring")  GetGeomType "OGRwkbGeometryType
OGR_FD_GetGeomType(OGRFeatureDefnH hDefn)

Fetch the geometry base type of the passed feature definition.

This function is the same as the C++ method
OGRFeatureDefn::GetGeomType().

Starting with GDAL 1.11, this method returns
GetGeomFieldDefn(0)->GetType().

Parameters:
-----------

hDefn:  handle to the feature definition to get the geometry type
from.

the base type for all geometry related to this definition. ";

%feature("docstring")  SetGeomType "void
OGR_FD_SetGeomType(OGRFeatureDefnH hDefn, OGRwkbGeometryType eType)

Assign the base geometry type for the passed layer (the same as the
feature definition).

All geometry objects using this type must be of the defined type or a
derived type. The default upon creation is wkbUnknown which allows for
any geometry type. The geometry type should generally not be changed
after any OGRFeatures have been created against this definition.

This function is the same as the C++ method
OGRFeatureDefn::SetGeomType().

Starting with GDAL 1.11, this method calls
GetGeomFieldDefn(0)->SetType().

Parameters:
-----------

hDefn:  handle to the layer or feature definition to set the geometry
type to.

eType:  the new type to assign. ";

%feature("docstring")  Reference "int
OGR_FD_Reference(OGRFeatureDefnH hDefn)

Increments the reference count by one.

The reference count is used keep track of the number of OGRFeature
objects referencing this definition.

This function is the same as the C++ method
OGRFeatureDefn::Reference().

Parameters:
-----------

hDefn:  handle to the feature definition on witch OGRFeature are based
on.

the updated reference count. ";

%feature("docstring")  Dereference "int
OGR_FD_Dereference(OGRFeatureDefnH hDefn)

Decrements the reference count by one.

This function is the same as the C++ method
OGRFeatureDefn::Dereference().

Parameters:
-----------

hDefn:  handle to the feature definition on witch OGRFeature are based
on.

the updated reference count. ";

%feature("docstring")  GetReferenceCount "int
OGR_FD_GetReferenceCount(OGRFeatureDefnH hDefn)

Fetch current reference count.

This function is the same as the C++ method
OGRFeatureDefn::GetReferenceCount().

Parameters:
-----------

hDefn:  handle to the feature definition on witch OGRFeature are based
on.

the current reference count. ";

%feature("docstring")  GetFieldIndex "int
OGR_FD_GetFieldIndex(OGRFeatureDefnH hDefn, const char *pszFieldName)

Find field by name.

The field index of the first field matching the passed field name
(case insensitively) is returned.

This function is the same as the C++ method
OGRFeatureDefn::GetFieldIndex.

Parameters:
-----------

hDefn:  handle to the feature definition to get field index from.

pszFieldName:  the field name to search for.

the field index, or -1 if no match found. ";

%feature("docstring")  IsGeometryIgnored "int
OGR_FD_IsGeometryIgnored(OGRFeatureDefnH hDefn)

Determine whether the geometry can be omitted when fetching features.

This function is the same as the C++ method
OGRFeatureDefn::IsGeometryIgnored().

Starting with GDAL 1.11, this method returns
GetGeomFieldDefn(0)->IsIgnored().

Parameters:
-----------

hDefn:  handle to the feature definition on witch OGRFeature are based
on.

ignore state ";

%feature("docstring")  SetGeometryIgnored "void
OGR_FD_SetGeometryIgnored(OGRFeatureDefnH hDefn, int bIgnore)

Set whether the geometry can be omitted when fetching features.

This function is the same as the C++ method
OGRFeatureDefn::SetGeometryIgnored().

Starting with GDAL 1.11, this method calls
GetGeomFieldDefn(0)->SetIgnored().

Parameters:
-----------

hDefn:  handle to the feature definition on witch OGRFeature are based
on.

bIgnore:  ignore state ";

%feature("docstring")  IsStyleIgnored "int
OGR_FD_IsStyleIgnored(OGRFeatureDefnH hDefn)

Determine whether the style can be omitted when fetching features.

This function is the same as the C++ method
OGRFeatureDefn::IsStyleIgnored().

Parameters:
-----------

hDefn:  handle to the feature definition on which OGRFeature are based
on.

ignore state ";

%feature("docstring")  SetStyleIgnored "void
OGR_FD_SetStyleIgnored(OGRFeatureDefnH hDefn, int bIgnore)

Set whether the style can be omitted when fetching features.

This function is the same as the C++ method
OGRFeatureDefn::SetStyleIgnored().

Parameters:
-----------

hDefn:  handle to the feature definition on witch OGRFeature are based
on.

bIgnore:  ignore state ";

%feature("docstring")  IsSame "int OGR_FD_IsSame(OGRFeatureDefnH
hFDefn, OGRFeatureDefnH hOtherFDefn)

Test if the feature definition is identical to the other one.

Parameters:
-----------

hFDefn:  handle to the feature definition on witch OGRFeature are
based on.

hOtherFDefn:  handle to the other feature definition to compare to.

TRUE if the feature definition is identical to the other one.

OGR 1.11 ";

}