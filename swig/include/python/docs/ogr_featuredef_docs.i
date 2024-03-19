%feature("docstring")  OGRFeatureDefnShadow "

Python proxy of an :cpp:class:`OGRFeatureDefn`.

";

%extend OGRFeatureDefnShadow {

%feature("docstring")  OGRFeatureDefnShadow "

Create a new feature definition object to hold the field definitions.

Parameters
----------
name_null_ok : str, optional
    Name for the :py:class:`FeatureDefn`.
";


%feature("docstring")  AddFieldDefn "

Add a new field definition.

To add a new field definition to a layer definition, do not use this
function directly, but use :py:meth:`Layer.CreateField` instead.

This function should only be called while there are no :py:class:`Feature`
objects in existence based on this :py:class:`FeatureDefn`. The
:py:class:`FieldDefn` passed in is copied.

See :cpp:func:`OGRFeatureDefn::AddFieldDefn`.

Parameters
-----------
defn : FieldDefn
    the new field definition.
";

%feature("docstring")  AddGeomFieldDefn "

Add a new geometry field definition.

To add a new field definition to a layer definition, do not use this
function directly, but use :py:meth:`Layer.CreateGeomField` instead.

This function should only be called while there are no :py:class:`Feature`
objects in existence based on this :py:class:`FeatureDefn`. The
:py:class:`GeomFieldDefn` passed in is copied.

See :cpp:Func:`OGRFeatureDefn::AddGeomFieldDefn`.

Parameters
-----------
defn : GeomFieldDefn
    new geometry field definition.
";

%feature("docstring")  DeleteGeomFieldDefn "

Delete an existing geometry field definition.

To delete an existing geometry field definition from a layer
definition, do not use this function directly, but use
:py:meth:`Layer.DeleteGeomField` instead ( not implemented yet).

This function should only be called while there are no :py:class:`Feature`
objects in existence based on this :py:class:`FeatureDefn`.

See :cpp:func:`OGRFeatureDefn::DeleteGeomFieldDefn`.

Parameters
-----------
idx : int
    the index of the geometry field definition.

Returns
--------
int:
    :py:const:`OGRERR_NONE` in case of success.
";

%feature("docstring")  GetFieldCount "

Fetch number of fields on the passed feature definition.

See :cpp:func:`OGRFeatureDefn::GetFieldCount`.

Returns
--------
int:
    count of fields.
";

%feature("docstring")  GetFieldDefn "

Fetch field definition of the passed feature definition.

See :cpp:func:`OGRFeatureDefn::GetFieldDefn`.

Parameters
-----------
i : int / str
    Field name or 0-based numeric index. For repeated
    access, use of the numeric index avoids a lookup
    step.

Returns
--------
FieldDefn:
    internal field definition object or ``None`` if the field does not
    exist. This object should not be modified by the application.
";

%feature("docstring")  GetFieldIndex "

Find field by name.

The field index of the first field matching the passed field name
(case insensitively) is returned.

See :cpp:func:`OGRFeatureDefn::GetFieldIndex`.

Parameters
-----------
field_name : str
    the field name to search for.

Returns
--------
int:
    the field index, or -1 if no match found.
";

%feature("docstring")  GetGeomFieldCount "

Fetch number of geometry fields on the passed feature definition.

See :cpp:func:`OGRFeatureDefn::GetGeomFieldCount`.

Returns
--------
int:
    count of geometry fields.
";

%feature("docstring")  GetGeomFieldDefn "

Fetch geometry field definition of the passed feature definition.

See :cpp:func:`OGRFeatureDefn::GetGeomFieldDefn`.

Parameters
-----------
i : int
    the geometry field to fetch, between 0 and GetGeomFieldCount() - 1.

Returns
--------
GeomFieldDefn:
    an internal field definition object or ``None`` if invalid
    index. This object should not be modified by the application.
";

%feature("docstring")  GetGeomFieldIndex "

Find geometry field by name.

The geometry field index of the first geometry field matching the
passed field name (case insensitively) is returned.

See :cpp:func:`OGRFeatureDefn::GetGeomFieldIndex`.

Parameters
-----------
field_name : str
    the geometry field name to search for.

Returns
--------
int:
    the geometry field index, or -1 if no match found.
";

%feature("docstring")  GetGeomType "

Fetch the geometry base type of the passed feature definition.

This is equivalent to ``GetGeomFieldDefn(0).GetType()``.

See :cpp:func:`OGRFeatureDefn::GetGeomType`.

Returns
--------
int :
    the base type for all geometry related to this definition.
";

%feature("docstring")  GetName "

Get name of the :py:class:`FeatureDefn`.

See :cpp:func:`OGRFeatureDefn::GetName`.

Returns
--------
str:
    the name
";

%feature("docstring")  GetReferenceCount "

Fetch current reference count.

See :cpp:func:`OGRFeatureDefn::GetReferenceCount`.

Returns
--------
int:
    the current reference count.
";

%feature("docstring")  IsGeometryIgnored "

Determine whether the geometry can be omitted when fetching features.

Equivalent to ``GetGeomFieldDefn(0).IsIgnored()``.

See :cpp:func:`OGRFeatureDefn::IsGeometryIgnored`.

Returns
--------
int:
    ignore state
";

%feature("docstring")  IsSame "

Test if the feature definition is identical to the other one.

Parameters
-----------
other_defn : FeatureDefn
    other feature definition to compare to.

Returns
--------
int:
    1 if the feature definition is identical to the other one.
";

%feature("docstring")  IsStyleIgnored "

Determine whether the style can be omitted when fetching features.

See :cpp:func:`OGRFeatureDefn::IsStyleIgnored`.

Returns
--------
int:
    ignore state
";

%feature("docstring")  SetGeomType "

Assign the base geometry type for the passed layer (the same as the
feature definition).

This is equivalent to ``GetGeomFieldDefn(0).SetType()``.

All geometry objects using this type must be of the defined type or a
derived type. The default upon creation is :py:const:`wkbUnknown` which allows for
any geometry type. The geometry type should generally not be changed
after any :py:class:`Feature` objects have been created against this definition.

See :cpp:func:`OGRFeatureDefn::SetGeomType`.

Parameters
-----------
geom_type : int
    the new type to assign.
";

%feature("docstring")  SetGeometryIgnored "

Set whether the geometry can be omitted when fetching features.

This is equivalent to ``GetGeomFieldDefn(0).SetIgnored()``.

See :cpp:func:`OGRFeatureDefn::SetGeometryIgnored`.

Parameters
-----------
bignored : bool
    ignore state
";

%feature("docstring")  SetStyleIgnored "

Set whether the style can be omitted when fetching features.

See :cpp:func:`OGRFeatureDefn::SetStyleIgnored`.

Parameters
-----------
bignored : bool
    ignore state
";

}
