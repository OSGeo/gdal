%feature("docstring") GDALDriverShadow "
Python proxy of a :cpp:class:`GDALDriver`.
";

%extend GDALDriverShadow {

%feature("docstring") CopyFiles "
Copy all the files associated with a :py:class:`Dataset`.

Parameters
----------
newName : str
    new path for the dataset
oldName : str
    old path for the dataset

Returns
-------
int:
   :py:const:`CE_None` on success or :py:const:`CE_Failure` on failure.
";

%feature("docstring") Create "

Create a new :py:class:`Dataset` with this driver.
See :cpp:func:`GDALDriver::Create`.

Parameters
----------
utf8_path : str
   Path of the dataset to create.
xsize : int
   Width of created raster in pixels. Set to zero for vector datasets.
ysize : int
   Height of created raster in pixels. Set to zero for vector datasets.
bands : int, default = 1
    Number of bands. Set to zero for vector datasets.
eType : int, default = :py:const:`GDT_Byte`
    Raster data type. Set to :py:const:`GDT_Unknown` for vector datasets.
options : list/dict
    List of driver-specific options

Returns
-------
Dataset

Examples
--------
>>> with gdal.GetDriverByName('GTiff').Create('test.tif', 12, 4, 2, gdal.GDT_Float32, {'COMPRESS': 'DEFLATE'}) as ds:
...     print(gdal.Info(ds))
...
Driver: GTiff/GeoTIFF
Files: test.tif
Size is 12, 4
Image Structure Metadata:
  INTERLEAVE=PIXEL
Corner Coordinates:
Upper Left  (    0.0,    0.0)
Lower Left  (    0.0,    4.0)
Upper Right (   12.0,    0.0)
Lower Right (   12.0,    4.0)
Center      (    6.0,    2.0)
Band 1 Block=12x4 Type=Float32, ColorInterp=Gray
Band 2 Block=12x4 Type=Float32, ColorInterp=Undefined

>>> with gdal.GetDriverByName('ESRI Shapefile').Create('test.shp', 0, 0, 0, gdal.GDT_Unknown) as ds:
...     print(gdal.VectorInfo(ds))
...
INFO: Open of `test.shp'
      using driver `ESRI Shapefile' successful.
";

%feature("docstring") CreateCopy "

Create a copy of a :py:class:`Dataset`.
See :cpp:func:`GDALDriver::CreateCopy`.

Parameters
----------
utf8_path : str
   Path of the dataset to create.
src : Dataset
   The Dataset being duplicated.
strict : bool, default=1
   Indicates whether the copy must be strictly equivalent or if
   it may be adapted as needed for the output format.
options : list/dict
   List of driver-specific options
callback : function, optional
   A progress callback function
callback_data: optional
   Optional data to be passed to callback function

Returns
-------
Dataset
";

%feature("docstring") CreateMultiDimensional "

Create a new multidimensional dataset.
See :cpp:func:`GDALDriver::CreateMultiDimensional`.

Parameters
----------
utf8_path : str
   Path of the dataset to create.
root_group_options : dict/list
   Driver-specific options regarding the creation of the
   root group.
options : list/dict
   List of driver-specific options regarding the creation
   of the Dataset.

Returns
-------
Dataset

Examples
--------
>>> with gdal.GetDriverByName('netCDF').CreateMultiDimensional('test.nc') as ds:
...     gdal.MultiDimInfo(ds)
...
{'type': 'group', 'driver': 'netCDF', 'name': '/', 'attributes': {'Conventions': 'CF-1.6'}, 'structural_info': {'NC_FORMAT': 'NETCDF4'}}

";

%feature("docstring") Delete "
Delete a :py:class:`Dataset`.
See :cpp:func:`GDALDriver::Delete`.

Parameters
----------
utf8_path : str
   Path of the dataset to delete.

Returns
-------
int:
   :py:const:`CE_None` on success or :py:const:`CE_Failure` on failure.
";

%feature("docstring") Deregister "
Deregister the driver.
See :cpp:func:`GDALDriverManager::DeregisterDriver`.
";

%feature("docstring") HelpTopic "
The URL for driver documentation, relative to the GDAL documentation directory.
See :cpp:func:`GDALGetDriverHelpTopic`.
";

%feature("docstring") LongName "
The long name of the driver.
See :cpp:func:`GDALGetDriverLongName`.
";

%feature("docstring") Register "
Register the driver for use.
See :cpp:func:`GDALDriverManager::RegisterDriver`.
";

%feature("docstring") Rename "
Rename a :py:class:`Dataset`.
See :cpp:func:`GDALDriver::Rename`.

Parameters
----------
newName : str
    new path for the dataset
oldName : str
    old path for the dataset

Returns
-------
int:
   :py:const:`CE_None` on success or :py:const:`CE_Failure` on failure.
";

%feature("docstring") ShortName "
The short name of a :py:class:`Driver` that can be passed to
:py:func:`GetDriverByName`.
See :cpp:func:`GDALGetDriverShortName`.
";

};
