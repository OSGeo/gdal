%feature("docstring")  CreateCodedFieldDomain "

Creates a new coded field domain.

See :cpp:func:`OGRCodedFieldDomain::OGRCodedFieldDomain`.

.. versionadded:: 3.3

Parameters
-----------
name : str
    Domain name. Should not be ``None``.
description : str, optional
    Domain description (can be ``None``)
type : int
    Field type.
subtype : int
    Field subtype.
enumeration : dict
    Enumeration as a dictionary of (code : value) pairs. Should not be ``None``.

Returns
--------
FieldDomain
";

%feature("docstring")  CreateGlobFieldDomain "

Creates a new glob field domain.

See :cpp:func:`OGRGlobFieldDomain::OGRGlobFieldDomain`

.. versionadded:: 3.3

Parameters
-----------
name : str
    Domain name. Should not be ``None``.
description : str, optional
    Domain description (can be ``None``)
type : int
    Field type.
subtype : int
    Field subtype.
glob : str
    Glob expression. Should not be ``None``.

Returns
--------
FieldDomain
";

%feature("docstring")  CreateRangeFieldDomain "
Creates a new range field domain.

See :cpp:func:`OGRRangeFieldDomain::OGRRangeFieldDomain`.

.. versionadded:: 3.3

Parameters
-----------
name : str
    Domain name. Should not be ``None``.
description : str, optional
    Domain description (can be ``None``)
type : int
    Field type. Generally numeric. Potentially :py:const:`OFTDateTime`.
subtype : int
    Field subtype.
min : float, optional
    Minimum value (can be ``None``).
minIsInclusive : bool
    Whether the minimum value is included in the range.
max : float, optional
    Maximum value (can be ``None``).
maxIsInclusive : bool
    Whether the maximum value is included in the range.

Returns
--------
FieldDomain
";

// GetFieldSubTypeName
%feature("docstring")  OGR_GetFieldSubTypeName "

Fetch human readable name for a field subtype.

See :cpp:func:`OGRFieldDefn::GetFieldSubTypeName`.

Parameters
-----------
type : int
    the field subtype to get name for.

Returns
--------
str
    the name.

Examples
--------
>>> ogr.GetFieldSubTypeName(1)
'Boolean'

>>> ogr.GetFieldSubTypeName(ogr.OFSTInt16)
'Int16'

";

// GetFieldTypeName
%feature("docstring")  OGR_GetFieldTypeName "
Fetch human readable name for a field type.

See :cpp:func:`OGRFieldDefn::GetFieldTypeName`.

Parameters
-----------
type : int
    the field type code to get name for

Returns
--------
str
    the name

Examples
--------
>>> ogr.GetFieldTypeName(0)
'Integer'

>>> ogr.GetFieldTypeName(ogr.OFTReal)
'Real'
";
