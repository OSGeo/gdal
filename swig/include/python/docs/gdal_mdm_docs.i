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
data_type : :py:class:`ExtendedDataType`
    Attribute data type
options : dict or list, optional
    dict or list of driver specific ``NAME=VALUE`` option strings.

Returns
-------

Attribute
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
direction : str
    Dimension direction (might be empty, and ignored by drivers)
size : int
    Number of values indexed by this dimension. Should be > 0
options : dict or list, optional
    dict or list of driver specific ``NAME=VALUE`` option strings.

Returns
-------

Dimension
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
data_type : :py:class:`ExtendedDataType`
    Array data type
options : dict or list, optional
    dict or list of driver specific ``NAME=VALUE`` option strings.

Returns
-------

MDArray
    the new :py:class:`MDArray` or ``None`` on failure.

Examples
--------
>>> drv = gdal.GetDriverByName('MEM')
>>> mem_ds = drv.CreateMultiDimensional('myds')
>>> rg = mem_ds.GetRootGroup()
>>> dimX = rg.CreateDimension('X', None, None, 3)
>>> ar = rg.CreateMDArray('ar', [dimX], gdal.ExtendedDataType.Create(gdal.GDT_Byte))

";


%feature("docstring") GuessGeoTransform "

Returns whether 2 specified dimensions form a geotransform

See :cpp:func:`GDALMDArray::GuessGeoTransform`.


Parameters
----------
nDimX : int
    Index of the X axis
nDimY : int
    Index of the Y axis
bPixelIsPoint : bool
    Whether the geotransform should be returned with the pixel-is-point (pixel-center) convention (bPixelIsPoint = True), or with the pixel-is-area (top left corner convention) (bPixelIsPoint = False)

Returns
-------
tuple
    A tuple of 6 geotransform coefficients if successful, or ``None`` on failure

Examples
--------
>>> import array
>>> drv = gdal.GetDriverByName('MEM')
>>> mem_ds = drv.CreateMultiDimensional('myds')
>>> rg = mem_ds.GetRootGroup()
>>> dimX = rg.CreateDimension('X', None, None, 3)
>>> dimY = rg.CreateDimension('Y', None, None, 2)
>>> varY = rg.CreateMDArray(dimY.GetName(), [dimY], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
>>> _ = varY.Write(array.array('d', [90 - 0.9 - i for i in range(dimY.GetSize())]))
>>> _ = dimY.SetIndexingVariable(varY)
>>> varX = rg.CreateMDArray(dimX.GetName(), [dimX], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
>>> _ = varX.Write(array.array('d', [-180 + 0.9 + i for i in range(dimX.GetSize())]))
>>> _ = dimX.SetIndexingVariable(varX)
>>> ar = rg.CreateMDArray('ar', [dimY, dimX], gdal.ExtendedDataType.Create(gdal.GDT_Byte))
>>> gt0 = ar.GuessGeoTransform(1, 0, False)  #(-179.6, 1.0, 0.0, 89.6, 0.0, -1.0)
>>> gt1 = ar.GuessGeoTransform(0, 1, False)  #(89.6, -1.0, 0.0, -179.6, 0.0, 1.0)
>>> gt2 = ar.GuessGeoTransform(1, 0, True)   #(-179.1, 1.0, 0.0, 89.1, 0.0, -1.0)

";
