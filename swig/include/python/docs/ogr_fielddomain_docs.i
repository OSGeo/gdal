%feature("docstring") OGRFieldDomainShadow "

Python proxy of an :cpp:class:`OGRFieldDomain`.

Created using one of:

- :py:func:`CreateCodedFieldDomain`
- :py:func:`CreateGlobFieldDomain`
- :py:func:`CreateRangeFieldDomain`
";

%extend OGRFieldDomainShadow {

%feature("docstring")  GetDescription "

Get the description of the field domain.

See :cpp:func:`OGRFieldDomain::GetDescription`.

.. versionadded:: 3.3

Returns
--------
str
    the field domain description (might be empty string).
";

%feature("docstring")  GetName "

Get the name of the field domain.

See :cpp:func:`OGRFieldDomain::GetName`.

.. versionadded:: 3.3

Returns
--------
str
    the field domain name.
";

%feature("docstring")  GetDomainType "

Get the type of the field domain.

See :cpp:func:`OGRFieldDomain::GetDomainType`.

.. versionadded:: 3.3

Returns
--------
int
    the type of the field domain.

Examples
--------
>>> d = ogr.CreateCodedFieldDomain('my_code', None, ogr.OFTInteger, ogr.OFSTNone, { 1 : 'owned', 2 : 'leased' })
>>> d.GetDomainType() == ogr.OFDT_CODED
True

";

%feature("docstring")  GetEnumeration "

Get the enumeration as a mapping of codes to values.

See :cpp:func:`OGRCodedFieldDomain::GetEnumeration`.

.. versionadded:: 3.3

Returns
--------
dict

Examples
--------
>>> d = ogr.CreateCodedFieldDomain('my_domain', None, ogr.OFTInteger, ogr.OFSTNone, { 1 : 'owned', 2 : 'leased' })
>>> d.GetEnumeration()
{'1': 'owned', '2': 'leased'}

";

%feature("docstring")  GetFieldSubType "

Get the field subtype of the field domain.

See :cpp:func:`OGRFieldDomain::GetFieldSubType`.

.. versionadded:: 3.3

Returns
--------
int
    the field subtype of the field domain.
";

%feature("docstring")  GetFieldType "

Get the field type of the field domain.

See :cpp:func:`OGRFieldDomain::GetFieldType`.

.. versionadded:: 3.3

Returns
--------
int
    the field type of the field domain.
";

%feature("docstring")  GetGlob "

Get the glob expression.

See :cpp:func:`OGRGlobFieldDomain::GetGlob`.

.. versionadded:: 3.3

Returns
--------
str
    the glob expression, or ``None`` in case of error
";

%feature("docstring")  GetMergePolicy "

Get the merge policy of the field domain.

See :cpp:func:`OGRFieldDomain::GetMergePolicy`.

.. versionadded:: 3.3

Returns
--------
int
    the merge policy of the field domain (default = :py:const:`OFDMP_DEFAULT_VALUE`)
";

%feature("docstring")  GetMaxAsDouble "

Get the maximum value of a range domain.

See :cpp:func:`OGRRangeFieldDomain::GetMax()`

.. versionadded:: 3.3

Returns
--------
float
    the maximum value of the range
";

%feature("docstring")  GetMaxAsString "

Get the maximum value of a range domain.

See :cpp:func:`OGRRangeFieldDomain::GetMax()`

.. versionadded:: 3.3

Returns
--------
str
    the maximum value of the range
";

%feature("docstring")  GetMinAsDouble "

Get the minimum value of a range domain.

See :cpp:func:`OGRRangeFieldDomain::GetMin()`

.. versionadded:: 3.3

Returns
--------
float
    the minimum value of the range
";

%feature("docstring")  GetMinAsString "

Get the minimum value of a range domain.

See :cpp:func:`OGRRangeFieldDomain::GetMin()`

.. versionadded:: 3.3

Returns
--------
str
    the minimum value of the range
";

%feature("docstring")  GetSplitPolicy "

Get the split policy of the field domain.

See :cpp:func:`OGRFieldDomain::GetSplitPolicy`.

.. versionadded:: 3.3

Returns
--------
int
    the split policy of the field domain (default = :py:const:`OFDSP_DEFAULT_VALUE`)
";

%feature("docstring")  SetMergePolicy "

Set the merge policy of the field domain.

See :cpp:func:`OGRFieldDomain::SetMergePolicy`.

.. versionadded:: 3.3

Parameters
-----------
policy : int
    the merge policy code of the field domain.
";

%feature("docstring")  SetSplitPolicy "

Set the split policy of the field domain.

See :cpp:func:`OGRFieldDomain::SetSplitPolicy`.

.. versionadded:: 3.3

policy : int
    the split policy code of the field domain.
";

}

