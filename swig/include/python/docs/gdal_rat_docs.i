%feature("docstring") GDALRasterAttributeTable "
Python proxy of a :cpp:class:`GDALRasterAttributeTable`.
";

%extend GDALRasterAttributeTableShadow {

%feature("docstring")  Clone "

Create a copy of the RAT.

Returns
-------
RasterAttributeTable
    A Python proxy of a :cpp:class:`GDALRasterAttributeTable`

";

%feature("docstring")  CreateColumn "

Create a new column in the RAT.

If the table already has rows, all row values for the new column will
be initialized to the default value ('', or zero).  The new column is
always created as the last column.

Parameters
----------
pszName : str
    Name of the new column
eType : int
    Data type of the new column (one of :py:const:`GFT_Integer`, :py:const:`GFT_Real`, or :py:const:`GFT_String`).
eUsage : int
    Usage of the new column (see :cpp:enum:`GDALRATFieldUsage`)

Returns
-------
int
   :py:const:`CE_None` on success or :py:const:`CE_Failure` on failure.

";

%feature("docstring")  DumpReadable "

Return an XML representation of the RAT.

Returns
-------
str

";

%feature("docstring")  GetColOfUsage "

Return the first column of a specified usage the a RAT.

See :cpp:func:`GDALRasterAttributeTable::GetColOfUsage`.

Parameters
----------
eUsage : int
    Usage code such as :py:const:`gdal.GFU_Red`.

Returns
-------
int
    Index of the column, or -1 if no such column can be found.

Examples
--------
>>> ds = gdal.Open('testrat.tif')
>>> rat = ds.GetRasterBand(1).GetDefaultRAT()
>>> rat.GetColOfUsage(gdal.GFU_Name)
2
>>> rat.GetColOfUsage(gdal.GFU_RedMin)
-1

";

%feature("docstring")  GetColumnCount "

Return the number of columns in the RAT. 

See :cpp:func:`GDALRasterAttributeTable::GetColumnCount`.

Returns
-------
int
    The number of columns in the RAT

Examples
--------
>>> with gdal.Open('testrat.tif') as ds:
...     ds.GetRasterBand(1).GetDefaultRAT().GetColumnCount()
... 
9

";

%feature("docstring")  GetLinearBinning "

Get linear binning information, if any.

See :cpp:func:`GDALRasterAttributeTable::GetLinearBinning`.

Returns
-------
list
   a three-element list indicating whether linear binning information
   is available, the minimum value associated with the smallest bin,
   and the size of each bin.

";

%feature("docstring")  GetNameOfCol "

Get the name of a specified column (0-indexed).

See :cpp:func:`GDALRasterAttributeTable::GetNameOfCol`.

Parameters
----------
iCol : int
    The index of the column (starting at 0)

Returns
-------
str
    The name of the column

Examples
--------
>>> ds = gdal.Open('testrat.tif')
>>> rat = ds.GetRasterBand(1).GetDefaultRAT()
>>> [rat.GetNameOfCol(i) for i in range(rat.GetColumnCount())]
['VALUE', 'COUNT', 'CLASS', 'Red', 'Green', 'Blue', 'OtherInt', 'OtherReal', 'OtherStr']

";

%feature("docstring")  GetRowCount "

Return the number of rows in the RAT. 

See :cpp:func:`GDALRasterAttributeTable::GetRowCount`.

Returns
-------
int
    The number of rows in the RAT

Examples
--------
>>> with gdal.Open('testrat.tif') as ds:
...     ds.GetRasterBand(1).GetDefaultRAT().GetRowCount()
... 
2

";

%feature("docstring")  GetRowOfValue "

Return the index of the row that applies to a specific value,
or -1 of no such row exists.

See :cpp:func:`GDALRasterAttributeTable::GetRowOfValue`.

Parameters
----------
dfValue : float
    Value for which a row should be found

Returns
-------
int
    Index of the row (0-based), or -1 of no row was found

Examples
--------
>>> ds = gdal.Open('testrat.tif')
>>> rat = ds.GetRasterBand(1).GetDefaultRAT()
>>> rat.GetValueAsString(rat.GetRowOfValue(2), 2)
'my class2'
>>> rat.GetValueAsString(rat.GetRowOfValue(802), 2)
''

See Also
--------
:py:meth:`SetLinearBinning`

";

%feature("docstring")  GetTableType "

Returns the type of the RAT (:py:const:`GRTT_THEMATIC` or :py:const:`GRTT_ATHEMATIC`).

See :cpp:func:`GDALRasterAttributeTable::GetTableType`.

Returns
-------
int
    table type code

";

%feature("docstring")  GetTypeOfCol "

Return the data type of a column in the RAT (one of :py:const:`GFT_Integer`, :py:const:`GFT_Real`, or :py:const:`GFT_String`).

See :cpp:func:`GDALRasterAttributeTable::GetTypeOfCol`.

Parameters
----------
iCol : int
    The index of the column (starting at 0)

Returns
-------
int
    type code for the specified column

Examples
--------
>>> ds = gdal.Open('testrat.tif')
>>> rat = ds.GetRasterBand(1).GetDefaultRAT()
>>> rat.GetTypeOfCol(2) == gdal.GFT_String
True

";

%feature("docstring")  GetUsageOfCol "

Return the usage of a column in the RAT.

See :cpp:func:`GDALRasterAttributeTable::GetUsageOfCol`.

Parameters
----------
iCol : int
    The index of the column (starting at 0)

Returns
-------
int
    Usage code for the specified column

Examples
--------
>>> ds = gdal.Open('testrat.tif')
>>> rat = ds.GetRasterBand(1).GetDefaultRAT()
>>> [rat.GetUsageOfCol(i) for i in range(rat.GetColumnCount())]
[5, 1, 2, 6, 7, 8, 0, 0, 0]
>>> [rat.GetUsageOfCol(i) == gdal.GFU_Red for i in range(rat.GetColumnCount())]
[False, False, False, True, False, False, False, False, False]

";

%feature("docstring")  GetValueAsDouble "

Get the value of a single cell in the RAT.

Parameters
----------
iRow : int
    Row index (0-based)
iCol : int
    Column index (0-based)
";

%feature("docstring")  GetValueAsInt "

Get the value of a single cell in the RAT.

Parameters
----------
iRow : int
    Row index (0-based)
iCol : int
    Column index (0-based)
";

%feature("docstring")  GetValueAsString "

Get the value of a single cell in the RAT.

Parameters
----------
iRow : int
    Row index (0-based)
iCol : int
    Column index (0-based)
";


%feature("docstring")  ReadValuesIOAsDouble "

Read a single column of a RAT into a list of floats.

Parameters
----------
iField : int
    The index of the column (starting at 0)
iStartRow : int, default = 0
    The index of the first row to read (starting at 0)
iLength : int, default = None
    The number of rows to read 

Returns
-------
list

Examples
--------
>>> ds = gdal.Open('testrat.tif')
>>> rat = ds.GetRasterBand(1).GetDefaultRAT()
>>> rat.ReadValuesIOAsDouble(3, 0, 2)
[26.0, 26.0]

See Also
--------
:py:meth:`ReadAsArray`

";

%feature("docstring")  ReadValuesIOAsInteger "

Read a single column of a RAT into a list of ints.

Parameters
----------
iField : int
    The index of the column (starting at 0)
iStartRow : int, default = 0
    The index of the first row to read (starting at 0)
iLength : int, default = None
    The number of rows to read 

Returns
-------
list

Examples
--------
>>> ds = gdal.Open('testrat.tif')
>>> rat = ds.GetRasterBand(1).GetDefaultRAT()
>>> rat.ReadValuesIOAsInteger(3, 0, 2)
[26, 26]

See Also
--------
:py:meth:`ReadAsArray`

";

%feature("docstring")  ReadValuesIOAsString "

Read a single column of a RAT into a list of strings.

Parameters
----------
iField : int
    The index of the column (starting at 0)
iStartRow : int, default = 0
    The index of the first row to read (starting at 0)
iLength : int, default = None
    The number of rows to read 

Returns
-------
list

Examples
--------
>>> ds = gdal.Open('testrat.tif')
>>> rat = ds.GetRasterBand(1).GetDefaultRAT()
>>> rat.ReadValuesIOAsString(2, 0, 2)
['my class', 'my class2']

See Also
--------
:py:meth:`ReadAsArray`

";

%feature("docstring")  RemoveStatistics "

Remove statistics information, such as a histogram, from the RAT.

See :cpp:func:`GDALRasterAttributeTable::RemoveStatistics`.

";

%feature("docstring")  SetLinearBinning "

Set linear binning information.

This can be used to provide optimized table look-ups (via
:py:meth:`GetRowOfValue`) when the rows of the table represent
uniformly sized bins. 

It is the responsibility of the user to actually define the 
appropriate bins. If the bins do not correspond to the provided
binning information, lookup values will be incorrect.

See :cpp:func:`GDALRasterAttributeTable::SetLinearBinning`.

Parameters
----------
dfRow0Min : float
    Minimum value associated with the smallest bin   
dfBinSize : float
    Size of each bin
";

%feature("docstring")  SetRowCount "

Resizes the table to include the indicated number of rows. Newly created
rows will be initialized to '' for strings and zero for numeric fields.

See :cpp:func:`GDALRasterAttributeTable::SetRowCount`.

Parameters
----------
nCount : int
   The number of rows in the resized table

";

%feature("docstring")  SetTableType "

Set the type of the RAT (thematic or athematic).

Parameters
----------
eTableType : int
   Table type (:py:const:`GRTT_THEMATIC` or :py:const:`GRTT_ATHEMATIC`)    
";

%feature("docstring")  SetValueAsDouble "

Set the value of a single cell in the RAT.

If ``iRow`` is equal to the number of rows in the table, the table
size will be increased by one. However, it is more efficient to
call :py:meth:`SetRowCount` before repeated insertions.

Parameters
----------
iRow : int
    Row index (0-based)
iCol : int
    Column index (0-based)
dfValue : float
    Cell value
";

%feature("docstring")  SetValueAsInt "

Set the value of a single cell in the RAT.

If ``iRow`` is equal to the number of rows in the table, the table
size will be increased by one. However, it is more efficient to
call :py:meth:`SetRowCount` before repeated insertions.

Parameters
----------
iRow : int
    Row index (0-based)
iCol : int
    Column index (0-based)
nValue : int
    Cell value
";

%feature("docstring")  SetValueAsString "

Set the value of a single cell in the RAT.

If ``iRow`` is equal to the number of rows in the table, the table
size will be increased by one. However, it is more efficient to
call :py:meth:`SetRowCount` before repeated insertions.

Parameters
----------
iRow : int
    Row index (0-based)
iCol : int
    Column index (0-based)
pszValue : str
    Cell value
";

}

