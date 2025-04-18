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
data_type: :py:class:`osgeo.gdal.ExtendedDataType`
options: dict/list
    an optional dict or list of driver specific ``NAME=VALUE`` option strings.

Returns
-------

MDArray:
    the new :py:class:`osgeo.gdal.MDArray` or ``None`` on failure.

Examples
--------
>>> drv = gdal.GetDriverByName('MEM')
>>> mem_ds = drv.CreateMultiDimensional('myds')
>>> rg = mem_ds.GetRootGroup()
>>> dimX = rg.CreateDimension('X', None, None, 3)
>>> ar = rg.CreateMDArray('ar', [dimX], gdal.ExtendedDataType.Create(gdal.GDT_Byte))

";

