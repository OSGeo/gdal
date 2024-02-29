// gdal.AllRegister
%feature("docstring") GDALAllRegister "

Register all known configured GDAL drivers.
Automatically called when the :py:mod:`gdal` module is loaded.
Does not need to be called in user code unless a driver was
deregistered and needs to be re-registered.
See :cpp:func:`GDALAllRegister`.

See Also
--------
:py:func:`Driver.Register`
";

// gdal.GetCacheMax
%feature("docstring") wrapper_GDALGetCacheMax "

Get the maximum size of the block cache.
See :cpp:func:`GDALGetCacheMax`.

Returns
-------
int
    maximum cache size in bytes
";

// gdal.GetCacheUsed
%feature("docstring") wrapper_GDALGetCacheUsed "

Get the number of bytes in used by the block cache.
See :cpp:func:`GDALGetCacheUsed`.

Returns
-------
int
    cache size in bytes
";

// gdal.GetConfigOption
%feature("docstring") wrapper_CPLGetConfigOption "

Return the value of a configuration option.
See :cpp:func:`CPLGetConfigOption`.

Parameters
----------
pszKey : str
    name of the configuration option
pszDefault : str, optional
    default value to return if the option has not been set

Returns
-------
str

See Also
--------
:py:func:`GetConfigOptions`
:py:func:`GetThreadLocalConfigOption`
";

// gdal.GetConfigOptions
%feature("docstring") wrapper_GetConfigOptions "

Return a dictionary of currently set configuration options.
See :cpp:func:`CPLGetConfigOptions`.

Returns
-------
dict

Examples
--------
>>> with gdal.config_options({'A': '3', 'B': '4'}):
...     gdal.SetConfigOption('C', '5')
...     gdal.GetConfigOptions()
...
{'C': '5', 'A': '3', 'B': '4'}

See Also
--------
:py:func:`GetConfigOption`
:py:func:`GetGlobalConfigOptions`
";

// gdal.GetDataTypeByName
%feature("docstring") GDALGetDataTypeByName "

Return the data type for a given name.

Parameters
----------
pszDataTypeName : str
    data type name

Returns
-------
int
    data type code

Examples
--------
>>> gdal.GetDataTypeByName('Int16') == gdal.GDT_Int16
True

"

// gdal.GetDataTypeName
%feature("docstring") GDALGetDataTypeName "

Return the name of the data type.

Parameters
----------
eDataType : int
    data type code

Returns
-------
str

Examples
--------
>>> gdal.GetDataTypeName(gdal.GDT_Int16)
'Int16'
>>> gdal.GetDataTypeName(gdal.GDT_Float64)
'Float64'
";

// gdal.GetDataTypeSize
%feature("docstring") GDALGetDataTypeSize "

Return the size of the data type in bits.

Parameters
----------
eDataType : int
    data type code

Returns
-------
int

Examples
--------
>>> gdal.GetDataTypeSize(gdal.GDT_Byte)
8
>>> gdal.GetDataTypeSize(gdal.GDT_Int32)
32
";

// gdal.GetDriverCount
%feature("docstring") GetDriverCount "

Return the number of registered drivers.
See :cpp:func:`GDALGetDriverCount`.

Examples
--------
>>> gdal.GetDriverCount()
227
>>> gdal.GetDriverByName('ESRI Shapefile').Deregister()
>>> gdal.GetDriverCount()
226

";

// gdal.GetGlobalConfigOption
%feature("docstring") wrapper_CPLGetGlobalConfigOption "

Return the value of a global (not thread-local) configuration option.
See :cpp:func:`CPLGetGlobalConfigOption`.

Parameters
----------
pszKey : str
    name of the configuration option
pszDefault : str, optional
    default value to return if the option has not been set

Returns
-------
str
";

// gdal.GetGlobalConfigOptions
%feature("docstring") wrapper_GetGlobalConfigOptions "

Return a dictionary of currently set configuration options,
excluding thread-local configuration options.

Returns
-------
dict

See Also
--------
:py:func:`GetConfigOption`
:py:func:`GetConfigOptions`
";

// gdal.GetNumCPUs
%feature("docstring") CPLGetNumCPUs "

Return the number of processors detected by GDAL.

Returns
-------
int
";

// gdal.GetThreadLocalConfigOption
%feature("docstring") wrapper_CPLGetThreadLocalConfigOption "

Return the value of a thread-local configuration option.
See :cpp:func:`CPLGetThreadLocalConfigOption`.

Parameters
----------
pszKey : str
    name of the configuration option
pszDefault : str, optional
    default value to return if the option has not been set

Returns
-------
str

";

// gdal.Open
%feature("docstring") Open "

Opens a raster file as a :py:class:`Dataset` using default options.
See :cpp:func:`GDALOpen`.
For more control over how the file is opened, use :py:func:`OpenEx`.

Parameters
----------
utf8_path : str
    name of the file to open
eAccess : int, default = :py:const:`gdal.GA_ReadOnly`

Returns
-------
Dataset, or ``None`` on failure

See Also
--------
:py:func:`OpenEx`
:py:func:`OpenShared`

";

// gdal.OpenEx
%feature("docstring") OpenEx "

Open a raster or vector file as a :py:class:`Dataset`.
See :cpp:func:`GDALOpenEx`.

Parameters
----------
utf8_path : str
    name of the file to open
flags : int
        Flags controlling how the Dataset is opened. Multiple ``gdal.OF_XXX`` flags
        may be combined using the ``|`` operator. See :cpp:func:`GDALOpenEx`.
allowed_drivers : list, optional
        A list of the names of drivers that may attempt to open the dataset.
open_options : dict/list, optional
        A dict or list of name=value driver-specific opening options.
sibling_files: list, optional
        A list of filenames that are auxiliary to the main filename

Returns
-------
Dataset, or ``None`` on failure.

See Also
--------
:py:func:`Open`
:py:func:`OpenShared`

";

// gdal.OpenShared
%feature("docstring") OpenShared "

Open a raster file as a :py:class:`Dataset`. If the file has already been
opened in the current thread, return a reference to the already-opened
:py:class:`Dataset`.  See :cpp:func:`GDALOpenShared`.

Parameters
----------
utf8_path : str
    name of the file to open
eAccess : int, default = :py:const:`gdal.GA_ReadOnly`

Returns
-------
Dataset, or ``None`` on failure

See Also
--------
:py:func:`Open`
:py:func:`OpenEx`

";

// gdal.SetCacheMax
%feature("docstring") wrapper_GDALSetCacheMax "

Set the maximum size of the block cache.
See :cpp:func:`GDALSetCacheMax`.

Parameters
----------
nBytes: int
    Cache size in bytes

See Also
--------
:config:`GDAL_CACHEMAX`
";

// gdal.SetConfigOption
%feature("docstring") CPLSetConfigOption "

Set the value of a configuration option for all threads.
See :cpp:func:`CPLSetConfigOption`.

Parameters
----------
pszKey : str
    name of the configuration option
pszValue : str
    value of the configuration option

See Also
--------
:py:func:`SetThreadLocalConfigOption`
:py:func:`config_option`
:py:func:`config_options`

";

// gdal.SetThreadLocalConfigOption
%feature("docstring") CPLSetThreadLocalConfigOption "

Set the value of a configuration option for the current thread.
See :cpp:func:`CPLSetThreadLocalConfigOption`.

Parameters
----------
pszKey : str
    name of the configuration option
pszValue : str
    value of the configuration option

See Also
--------
:py:func:`SetConfigOption`
:py:func:`config_option`
:py:func:`config_options`
";
