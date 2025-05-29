%feature("docstring")  CreateAttribute "

Create an attribute within a :py:class:`MDArray` or :py:class:`Group`.

See :cpp:func:`GDALIHasAttribute::CreateAttribute`.

Parameters
----------
name : str
    name
dimensions : list
    List of dimensions, ordered from the slowest varying
    dimension first to the fastest varying dimension last.
    Might be empty for a scalar array (if supported by driver)
data_type: :py:class:`ExtendedDataType`
    Attribute data type
options: dict/list
    an optional dict or list of driver specific ``NAME=VALUE`` option strings.

Returns
-------

Attribute:
    the new :py:class:`Attribute` or ``None`` on failure.

Examples
--------

>>> drv = gdal.GetDriverByName('MEM')
>>> mem_ds = drv.CreateMultiDimensional('myds')
>>> rg = mem_ds.GetRootGroup()
>>> dim = rg.CreateDimension('dim', None, None, 2)
>>> ar = rg.CreateMDArray('ar_double', [dim], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
>>> numeric_attr = ar.CreateAttribute('numeric_attr', [], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
>>> string_attr = ar.CreateAttribute('string_attr', [], gdal.ExtendedDataType.CreateString())

";

%feature("docstring")  CreateDimension "

Create a dimension within a :py:class:`Group`.

See :cpp:func:`GDALGroup::CreateDimension`.

Parameters
----------
name : str
    Dimension name
dim_type : str
    Dimension type (might be empty, and ignored by drivers)
direction: str
    Dimension direction (might be empty, and ignored by drivers)
size : int
    Number of values indexed by this dimension. Should be > 0
options: dict/list
    an optional dict or list of driver specific ``NAME=VALUE`` option strings.

Returns
-------

Dimension:
    the new :py:class:`Dimension` or ``None`` on failure.

Examples
--------

>>> drv = gdal.GetDriverByName('MEM')
>>> mem_ds = drv.CreateMultiDimensional('myds')
>>> rg = mem_ds.GetRootGroup()
>>> dim_band = rg.CreateDimension('band', None, None, 3)
>>> dim_x = rg.CreateDimension('X', None, None, 2)
>>> dim_x.GetFullName()
'/X'
>>> lat = rg.CreateDimension('latitude', gdal.DIM_TYPE_HORIZONTAL_X, None, 2)
>>> lat.GetType()
'HORIZONTAL_X'
";

%feature("docstring")  CreateMDArray "

Create a multidimensional array within a group.

It is recommended that the GDALDimension objects passed in ``dimensions``
belong to this group, either by retrieving them with :py:meth:`GetDimensions`
or creating a new one with :py:meth:`CreateDimension`.

See :cpp:func:`GDALGroup::CreateMDArray`.

Parameters
----------
name : str
    name
dimensions : list
    List of dimensions, ordered from the slowest varying
    dimension first to the fastest varying dimension last.
    Might be empty for a scalar array (if supported by driver)
data_type: :py:class:`ExtendedDataType`
    Array data type
options: dict/list
    an optional dict or list of driver specific ``NAME=VALUE`` option strings.

Returns
-------

MDArray:
    the new :py:class:`MDArray` or ``None`` on failure.

Examples
--------
>>> drv = gdal.GetDriverByName('MEM')
>>> mem_ds = drv.CreateMultiDimensional('myds')
>>> rg = mem_ds.GetRootGroup()
>>> dimX = rg.CreateDimension('X', None, None, 3)
>>> ar = rg.CreateMDArray('ar', [dimX], gdal.ExtendedDataType.Create(gdal.GDT_Byte))

";

