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

%feature("docstring") GetDriverByName "

Get a vector driver. Like :py:func:`gdal.GetDriverByName`, but
only returns drivers that handle vector data.

Parameters
----------
name : str
    name of the driver to fetch

Returns
-------
gdal.Driver

Examples
--------
>>> ogr.GetDriverByName('ESRI Shapefile').GetDescription()
'ESRI Shapefile'

>>> ogr.GetDriverByName('GTiff')
>>>
"

%feature("docstring") GetDriverCount "

Returns the number of registered drivers that handle vector data.

Returns
-------
int
"

%feature("docstring") Open "

Open a vector file as a :py:class:`gdal.Dataset`.
Equivalent to calling :py:func:`gdal.OpenEx` with the
:py:const:`gdal.OF_VECTOR` flag.

Parameters
----------
utf8_path : str
    name of the file to open

Returns
-------
gdal.Dataset, or ``None`` on failure

Examples
--------
>>> from osgeo import ogr
>>> ogr.GetDriverByName('ESRI Shapefile').GetDescription()
'ESRI Shapefile'
>>> ogr.GetDriverByName('GTiff')
>>>
";


%feature("docstring") OpenShared "

Open a vector file as a :py:class:`gdal.Dataset`. If the file has already been
opened in the current thread, return a reference to the already-opened
:py:class:`gdal.Dataset`. Equivalent to calling :py:func:`gdal.OpenEx` with the
:py:const:`gdal.OF_VECTOR` and :py:const:`gdal.OF_SHARED` flags.

Parameters
----------
utf8_path : str
    name of the file to open

Returns
-------
gdal.Dataset, or ``None`` on failure

";

