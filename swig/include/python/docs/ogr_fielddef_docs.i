%feature("docstring") OGRFieldDefnShadow "
Python proxy of an :cpp:class:`OGRFieldDefn`.
";

%extend OGRFieldDefnShadow {

%feature("docstring")  __init__ "

Create a new field definition.

By default, fields have no width, precision, are nullable and not
ignored.

This function is the same as the CPP method
OGRFieldDefn::OGRFieldDefn().

Parameters
-----------
pszName:
    the name of the new field definition.
eType:
    the type of the new field definition.

Returns
-------
OGRFieldDefnH:
    handle to the new field definition.
";

%feature("docstring")  GetAlternativeNameRef "

Fetch the alternative name (or \"alias\") for this field.

The alternative name is an optional attribute for a field which can
provide a more user-friendly, descriptive name of a field which is not
subject to the usual naming constraints defined by the data provider.

This is a metadata style attribute only: the alternative name cannot
be used in place of the actual field name during SQL queries or other
field name dependent API calls.

See :cpp:func:`OGRFieldDefn::GetAlternativeNameRef`.

.. versionadded:: 3.2

Returns
--------
str:
    the alternative name of the field definition.
";

%feature("docstring")  GetDefault "

Get default field value.

See :cpp:func:`OGRFieldDefn::GetDefault`.

Returns
--------
str:
    default field value or ``None``.
";

%feature("docstring")  GetDomainName "

Return the name of the field domain for this field.

By default an empty string is returned.

Field domains ( :py:class:`FieldDomain` class) are attached at the :py:class:`Dataset` level and should be retrieved with :py:meth:`Dataset.GetFieldDomain`.

See :cpp:func:`OGRFieldDefn::GetDomainName`.

.. versionadded:: 3.3

Returns
--------
str:
    the field domain name, or an empty string if there is none.
";

%feature("docstring")  GetJustify "

Get the justification for this field.

See :cpp:func:`OGRFieldDefn::GetJustify`.

Note: no driver is know to use the concept of field justification.

Returns
--------
OGRJustification:
    the justification.
";

%feature("docstring")  GetNameRef "

Fetch name of this field.

See :cpp:func:`OGRFieldDefn::GetNameRef`.

Returns
--------
str:
    the name of the field definition.
";

%feature("docstring")  GetPrecision "

Get the formatting precision for this field.

This should normally be zero for fields of types other than :py:const:`OFTReal`.

See :cpp:func:`OGRFieldDefn::GetPrecision`.

Returns
--------
int:
    the precision.
";

%feature("docstring")  GetSubType "

Fetch subtype of this field.

See :cpp:func:`OGRFieldDefn::GetSubType`.

Returns
--------
int
    field subtype code, default = :py:const:`OFSTNone`
";

%feature("docstring")  GetType "

Fetch type of this field.

See :cpp:func:`OGRFieldDefn::GetType`.

Returns
--------
int
    field type code, e.g. :py:const:`OFTInteger`
";

%feature("docstring")  GetWidth "

Get the formatting width for this field.

See :cpp:func:`OGRFieldDefn::GetWidth`.

Returns
--------
int:
    the width, zero means no specified width.
";

%feature("docstring")  IsDefaultDriverSpecific "

Returns whether the default value is driver specific.

Driver specific default values are those that are not NULL, a numeric
value, a literal value enclosed between single quote characters,
CURRENT_TIMESTAMP, CURRENT_TIME, CURRENT_DATE or datetime literal
value.

See :cpp:func:`OGRFieldDefn::IsDefaultDriverSpecific`.

Returns
--------
int:
    TRUE if the default value is driver specific.
";

%feature("docstring")  IsIgnored "

Return whether this field should be omitted when fetching features.

See :cpp:func:`OGRFieldDefn::IsIgnored`.

Returns
--------
int:
    ignore state
";

%feature("docstring")  IsNullable "

Return whether this field can receive null values.

By default, fields are nullable.

Even if this method returns FALSE (i.e not-nullable field), it doesn't
mean that :py:meth:`Feature.IsFieldSet` will necessary return TRUE, as
fields can be temporary unset and null/not-null validation is usually
done when :py:meth:`Layer.CreateFeature`/:py:meth:`Layer.SetFeature` is called.

See :cpp:func:`OGRFieldDefn::IsNullable`.

Returns
--------
int:
    TRUE if the field is authorized to be null.
";

%feature("docstring")  IsUnique "

Return whether this field has a unique constraint.

By default, fields have no unique constraint.

See :cpp:func:`OGRFieldDefn::IsUnique`.

.. versionadded:: 3.2

Returns
--------
int:
    TRUE if the field has a unique constraint.
";

%feature("docstring")  SetAlternativeName "

Reset the alternative name (or \"alias\") for this field.

The alternative name is an optional attribute for a field which can
provide a more user-friendly, descriptive name of a field which is not
subject to the usual naming constraints defined by the data provider.

This is a metadata style attribute only: the alternative name cannot
be used in place of the actual field name during SQL queries or other
field name dependent API calls.

See :cpp:func:`OGRFieldDefn::SetAlternativeName`.

.. versionadded:: 3.2

Parameters
-----------
alternativeName : str
    the new alternative name to apply.
";

%feature("docstring")  SetDefault "

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

See :cpp:func:`OGRFieldDefn::SetDefault`.

Parameters
-----------
pszValue : str
    new default field value or NULL pointer.
";

%feature("docstring")  SetDomainName "

Set the name of the field domain for this field.

Field domains ( :py:class:`FieldDomain`) are attached at the :py:class:`Dataset` level.

See :cpp:func:`OGRFieldDefn::SetDomainName`.

.. versionadded:: 3.3

Parameters
-----------
name : str
    Field domain name.
";

%feature("docstring")  SetIgnored "

Set whether this field should be omitted when fetching features.

See :cpp:func:`OGRFieldDefn::SetIgnored`.

Parameters
-----------
bignored : bool
    ignore state
";

%feature("docstring")  SetJustify "

Set the justification for this field.

Note: no driver is know to use the concept of field justification.

See :cpp:func:`OGRFieldDefn::SetJustify`.

Parameters
-----------
justify : int
    the new justification

Examples
--------
>>> f = ogr.FieldDefn('desc', ogr.OFTString)
>>> f.SetJustify(ogr.OJRight)
";

%feature("docstring")  SetName "

Reset the name of this field.

See :cpp:func:`OGRFieldDefn::SetName`.

Parameters
-----------
name : str
    the new name to apply
";

%feature("docstring")  SetNullable "

Set whether this field can receive null values.

By default, fields are nullable, so this method is generally called
with ``False`` to set a not-null constraint.

Drivers that support writing not-null constraint will advertise the
``GDAL_DCAP_NOTNULL_FIELDS`` driver metadata item.

See :cpp:func:`OGRFieldDefn::SetNullable`.

Parameters
-----------
bNullable : bool
    ``False`` if the field must have a not-null constraint.
";

%feature("docstring")  SetPrecision "

Set the formatting precision for this field in characters.

This should normally be zero for fields of types other than :py:const:`OFTReal`.

See :cpp:func:`OGRFieldDefn::SetPrecision`.

Parameters
-----------
precision : int
    the new precision.
";

%feature("docstring")  SetSubType "

Set the subtype of this field.

This should never be done to a :py:class:`FieldDefn` that is already part of
an :py:class:FeatureDefn`.

See :cpp:func:`OGRFieldDefn::SetSubType`.

Parameters
-----------
type :
    the new field subtype.

Examples
--------
>>> f = ogr.FieldDefn()
>>> f.SetType(ogr.OFTReal)
>>> f.SetSubType(ogr.OFSTJSON)
Warning 1: Type and subtype of field definition are not compatible. Resetting to OFSTNone
>>> f.SetSubType(ogr.OFSTFloat32)
";

%feature("docstring")  SetType "

Set the type of this field.

This should never be done to a :py:class:`FieldDefn` that is already part of
an :py:class:`FeatureDefn`.

See :cpp:func:`OGRFieldDefn::SetType`.

Parameters
-----------
type : int
    the new field type.

Examples
--------
>>> f = ogr.FieldDefn()
>>> f.SetType(ogr.OFTReal)
";

%feature("docstring")  SetUnique "

Set whether this field has a unique constraint.

By default, fields have no unique constraint, so this method is
generally called with TRUE to set a unique constraint.

Drivers that support writing unique constraint will advertise the
``GDAL_DCAP_UNIQUE_FIELDS`` driver metadata item.

Note that once a :py:class:`FieldDefn` has been added to a layer definition with
:py:meth:`Layer.AddFieldDefn`, its setter methods should not be called on the
object returned with ``GetLayerDefn().GetFieldDefn()``. Instead,
:py:meth:`Layer::AlterFieldDefn` should be called on a new instance of
:py:class:`FieldDefn`, for drivers that support :py:meth:`Layer.AlterFieldDefn`.

See :cpp:func:`OGRFieldDefn::SetUnique`.

.. versionadded:: 3.2

Parameters
-----------
bUnique : bool
    ``True`` if the field must have a unique constraint
";

%feature("docstring")  SetWidth "

Set the formatting width for this field in characters.

See :cpp:func:`OGRFieldDefn::SetWidth`.

Parameters
-----------
width : int
    the new width
";

}



