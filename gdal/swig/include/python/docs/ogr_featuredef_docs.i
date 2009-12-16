%extend OGRFeatureDefnShadow {
// File: ogrfeaturedefn_8cpp.xml
%feature("docstring")  CPL_CVSID "CPL_CVSID(\"$Id: ogrfeaturedefn.cpp
17587 2009-08-27 17:56:01Z warmerdam $\") ";

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

Starting with GDAL 1.7.0, this method will also issue an error if the
index is not valid.

Parameters:
-----------

hDefn:  handle to the feature definition to get the field definition
from.

iField:  the field to fetch, between 0 and GetFieldCount()-1.

an handle to an internal field definition object or NULL if invalid
index. This object should not be modified or freed by the application.
";

%feature("docstring")  AddFieldDefn "void
OGR_FD_AddFieldDefn(OGRFeatureDefnH hDefn, OGRFieldDefnH hNewField)

Add a new field definition to the passed feature definition.

This function should only be called while there are no OGRFeature
objects in existance based on this OGRFeatureDefn. The OGRFieldDefn
passed in is copied, and remains the responsibility of the caller.

This function is the same as the C++ method
OGRFeatureDefn::AddFieldDefn.

Parameters:
-----------

hDefn:  handle to the feature definition to add the field definition
to.

hNewField:  handle to the new field definition. ";

%feature("docstring")  GetGeomType "OGRwkbGeometryType
OGR_FD_GetGeomType(OGRFeatureDefnH hDefn)

Fetch the geometry base type of the passed feature definition.

This function is the same as the C++ method
OGRFeatureDefn::GetGeomType().

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

hDefn:  hanlde to the feature definition on witch OGRFeature are based
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

}