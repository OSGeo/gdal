%feature("docstring")  CreateMDArray "

Create a multidimensional array within a group.

See :cpp:func:`GDALGroup::CreateMDArray`.

Parameters
----------
osName : str
    name
aoDimensions : list
    List of dimensions, ordered from the slowest varying
    dimension first to the fastest varying dimension last.
    Might be empty for a scalar array (if supported by driver)
oDataType: GDALExtendedDataType
papszOptions : CSLConstList

Returns
-------

GDALMDArray - the new array

Examples
--------
>>> from osgeo import gdal
>>> drv = gdal.GetDriverByName('MEM')
>>> mem_ds = drv.CreateMultiDimensional('myds')
>>> rg = mem_ds.GetRootGroup()
>>> dimX = rg.CreateDimension('X', None, None, 3)
>>> ar = rg.CreateMDArray('ar', [dimX], gdal.ExtendedDataType.Create(gdal.GDT_Byte))

";

