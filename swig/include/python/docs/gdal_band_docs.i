%feature("docstring") GDALRasterBandShadow "

Python proxy of a :cpp:class:`GDALRasterBand`.

";

%extend GDALRasterBandShadow {

%feature("docstring")  Checksum "

Computes a checksum from a region of a RasterBand.
See :cpp:func:`GDALChecksumImage`.

Parameters
----------
xoff : int, default=0
   The pixel offset to left side of the region of the band to
   be read. This would be zero to start from the left side.
yoff : int, default=0
   The line offset to top side of the region of the band to
   be read. This would be zero to start from the top side.
xsize : int, optional
     The number of pixels to read in the x direction. By default,
     equal to the number of columns in the raster.
ysize : int, optional
     The number of rows to read in the y direction. By default,
     equal to the number of bands in the raster.

Returns
-------
int
    checksum value, or -1 in case of error

";

%feature("docstring")  ComputeBandStats "

Computes the mean and standard deviation of values in this Band.
See :cpp:func:`GDALComputeBandStats`.

Parameters
----------
samplestep : int, default=1
    Step between scanlines used to compute statistics.

Returns
-------
tuple
    tuple of length 2 with value of mean and standard deviation

See Also
--------
:py:meth:`ComputeRasterMinMax`
:py:meth:`ComputeStatistics`
:py:meth:`GetMaximum`
:py:meth:`GetMinimum`
:py:meth:`GetStatistics`
:py:meth:`SetStatistics`
";

%feature("docstring")  CreateMaskBand "

Add a mask band to the current band.
See :cpp:func:`GDALRasterBand::CreateMaskBand`.

Parameters
----------
nFlags : int

Returns
-------
int:
   :py:const:`CE_None` on success or :py:const:`CE_Failure` on failure.

";

%feature("docstring")  DeleteNoDataValue "

Remove the nodata value for this band.

Returns
-------
int:
   :py:const:`CE_None` on success or :py:const:`CE_Failure` on failure.

";

%feature("docstring")  Fill "

Fill this band with a constant value.
See :cpp:func:`GDALRasterBand::Fill`.

Parameters
----------
real_fill : float
    real component of the fill value
imag_fill : float, default = 0.0
    imaginary component of the fill value

Returns
-------
int:
   :py:const:`CE_None` on success or :py:const:`CE_Failure` on failure.

";

%feature("docstring")  FlushCache "

Flush raster data cache.
See :cpp:func:`GDALRasterBand::FlushCache`.
";

%feature("docstring")  GetActualBlockSize "

Fetch the actual block size for a given block offset.
See :cpp:func:`GDALRasterBand::GetActualBlockSize`.

Parameters
----------
nXBlockOff : int
    the horizontal block offset for which to calculate the
    number of valid pixels, with zero indicating the left most block, 1 the next
    block and so forth.
nYBlockOff : int
    the vertical block offset, with zero indicating
    the top most block, 1 the next block and so forth.

Returns
-------
tuple
    tuple with the x and y dimensions of the block
";

%feature("docstring")  GetBand "

Return the index of this band.
See :cpp:func:`GDALRasterBand::GetBand`.

Returns
-------
int
    the (1-based) index of this band
";

%feature("docstring")  GetBlockSize "

Fetch the natural block size of this band.
See :cpp:func:`GDALRasterBand::GetBlockSize`.

Returns
-------
list
    list with the x and y dimensions of a block
";

%feature("docstring")  GetCategoryNames "

Fetch the list of category names for this raster.
See :cpp:func:`GDALRasterBand::GetCategoryNames`.

Returns
-------
list
    A list of category names, or ``None``
";

%feature("docstring")  GetColorInterpretation "

Get the :cpp:enum:`GDALColorInterp` value for this band.
See :cpp:func:`GDALRasterBand::GetColorInterpretation`.

Returns
-------
int
";

%feature("docstring")  GetColorTable "

Get the color table associated with this band.
See :cpp:func:`GDALRasterBand::GetColorTable`.

Returns
-------
ColorTable or ``None``
";

%feature("docstring")  GetDataCoverageStatus "

Determine whether a sub-window of the Band contains only data, only empty blocks, or a mix of both.
See :cpp:func:`GDALRasterBand::GetDataCoverageStatus`.

Parameters
----------
nXOff : int
nYOff : int
nXSize : int
nYSize : int
nMaskFlagStop : int, default=0

Returns
-------
list
    First value represents a bitwise-or value of the following constants
    - :py:const:`gdalconst.GDAL_DATA_COVERAGE_STATUS_DATA`
    - :py:const:`gdalconst.GDAL_DATA_COVERAGE_STATUS_EMPTY`
    - :py:const:`gdalconst.GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED`
    Second value represents the approximate percentage in [0, 100] of pixels in the window that have valid values

Examples
--------
>>> import numpy as np
>>> # Create a raster with four blocks
>>> ds = gdal.GetDriverByName('GTiff').Create('test.tif', 64, 64, options = {'SPARSE_OK':True, 'TILED':True, 'BLOCKXSIZE':32, 'BLOCKYSIZE':32})
>>> band = ds.GetRasterBand(1)
>>> # Write some data to upper-left block
>>> band.WriteArray(np.array([[1, 2], [3, 4]]))
0
>>> # Check status of upper-left block
>>> flags, pct = band.GetDataCoverageStatus(0, 0, 32, 32)
>>> flags == gdal.GDAL_DATA_COVERAGE_STATUS_DATA
True
>>> pct
100.0
>>> # Check status of upper-right block
>>> flags, pct = band.GetDataCoverageStatus(32, 0, 32, 32)
>>> flags == gdal.GDAL_DATA_COVERAGE_STATUS_EMPTY
True
>>> pct
0.0
>>> # Check status of window touching all four blocks
>>> flags, pct = band.GetDataCoverageStatus(16, 16, 32, 32)
>>> flags == gdal.GDAL_DATA_COVERAGE_STATUS_DATA | gdal.GDAL_DATA_COVERAGE_STATUS_EMPTY
True
>>> pct
25.0
";

%feature("docstring")  GetDataset "

Fetch the :py:class:`Dataset` associated with this Band.
See :cpp:func:`GDALRasterBand::GetDataset`.
";

%feature("docstring")  GetDefaultHistogram "

Fetch the default histogram for this band.
See :cpp:func:`GDALRasterBand::GetDefaultHistogram`.

Returns
-------
list
    List with the following four elements:
    - lower bound of histogram
    - upper bound of histogram
    - number of buckets in histogram
    - tuple with counts for each bucket
";

%feature("docstring")  GetHistogram "

Compute raster histogram.
See :cpp:func:`GDALRasterBand::GetHistogram`.

Parameters
----------
min : float, default=-0.05
    the lower bound of the histogram
max : float, default=255.5
    the upper bound of the histogram
buckets : int, default=256
    the number of buckets int he histogram
include_out_of_range : bool, default=False
    if ``True``, add out-of-range values into the first and last buckets
approx_ok : bool, default=True
    if ``True``, compute an approximate histogram by using subsampling or overviews
callback : function, optional
             A progress callback function
callback_data: optional
             Optional data to be passed to callback function

Returns
-------
list
    list with length equal to ``buckets``. If ``approx_ok`` is ``False``, each
    the value of each list item will equal the number of pixels in that bucket.

Examples
--------
>>> import numpy as np
>>> ds = gdal.GetDriverByName('MEM').Create('', 10, 10, eType=gdal.GDT_Float32)
>>> ds.WriteArray(np.random.normal(size=100).reshape(10, 10))
0
>>> ds.GetRasterBand(1).GetHistogram(min=-3.5, max=3.5, buckets=13, approx_ok=False)
[0, 0, 3, 9, 13, 12, 25, 22, 9, 6, 0, 1, 0]  # random
";

%feature("docstring")  GetMaskBand "

Return the mask band associated with this band.
See :cpp:func:`GDALRasterBand::GetMaskBand`.

Returns
-------
Band

";

%feature("docstring")  GetMaskFlags "

Return the status flags of the mask band.
See :cpp:func:`GDALRasterBand::GetMaskFlags`.

Returns
-------
int

Examples
--------
>>> import numpy as np
>>> ds = gdal.GetDriverByName('MEM').Create('', 10, 10)
>>> band = ds.GetRasterBand(1)
>>> band.GetMaskFlags() == gdal.GMF_ALL_VALID
True
>>> band.SetNoDataValue(22)
0
>>> band.WriteArray(np.array([[22]]))
0
>>> band.GetMaskBand().ReadAsArray(win_xsize=2,win_ysize=2)
array([[  0, 255],
       [255, 255]], dtype=uint8)
>>> band.GetMaskFlags() == gdal.GMF_NODATA
True

";

%feature("docstring")  GetMaximum "

Fetch a previously stored maximum value for this band.
See :cpp:func:`GDALRasterBand::GetMaximum`.

Returns
-------
float
    The stored maximum value, or ``None`` if no value
    has been stored.

";

%feature("docstring")  GetMinimum "

Fetch a previously stored maximum value for this band.
See :cpp:func:`GDALRasterBand::GetMinimum`.

Returns
-------
float
    The stored minimum value, or ``None`` if no value
    has been stored.

";

%feature("docstring")  GetNoDataValueAsInt64 "

Fetch the nodata value for this band.
See :cpp:func:`GDALRasterBand::GetNoDataValueAsInt64`.

Returns
-------
int
    The nodata value, or ``None`` if it has not been set or
    the data type of this band is not :py:const:`gdal.GDT_Int64`.

";

%feature("docstring")  GetNoDataValueAsUInt64 "

Fetch the nodata value for this band.
See :cpp:func:`GDALRasterBand::GetNoDataValueAsUInt64`.

Returns
-------
int
    The nodata value, or ``None`` if it has not been set or
    the data type of this band is not :py:const:`gdal.GDT_UInt64`.

";

%feature("docstring")  GetOffset "

Fetch the raster value offset.
See :cpp:func:`GDALRasterBand::GetOffset`.

Returns
-------
double
    The offset value, or ``0.0``.

";

%feature("docstring")  GetOverview "

Fetch a raster overview.
See :cpp:func:`GDALRasterBand::GetOverview`.

Parameters
----------
i : int
    Overview index between 0 and ``GetOverviewCount() - 1``.

Returns
-------
Band

";

%feature("docstring")  GetOverviewCount "

Return the number of overview layers available.
See :cpp:func:`GDALRasterBand::GetOverviewCount`.

Returns
-------
int

";

%feature("docstring")  GetRasterCategoryNames "

Fetch the list of category names for this band.
See :cpp:func:`GDALRasterBand::GetCategoryNames`.

Returns
-------
list
    The list of names, or ``None`` if no names exist.

";

%feature("docstring")  GetRasterColorInterpretation  "

Return the color interpretation code for this band.
See :cpp:func:`GDALRasterBand::GetColorInterpretation`.

Returns
-------
int
    The color interpretation code (default :py:const:`gdal.GCI_Undefined`)

";

%feature("docstring")  GetRasterColorTable "

Fetch the color table associated with this band.
See :cpp:func:`GDALRasterBand::GetColorTable`.

Returns
-------
ColorTable
    The :py:class:`ColorTable`, or ``None`` if it has not been defined.
";

%feature("docstring")  GetScale "

Fetch the band scale value.
See :cpp:func:`GDALRasterBand::GetScale`.

Returns
-------
double
    The scale value, or ``1.0``.
";

%feature("docstring")  GetStatistics "

Return the minimum, maximum, mean, and standard deviation of all pixel values
in this band.
See :cpp:func:`GDALRasterBand::GetStatistics`

Parameters
----------
approx_ok : bool
    If ``True``, allow overviews or a subset of image tiles to be used in
    computing the statistics.
force : bool
    If ``False``, only return a result if it can be obtained without scanning
    the image, i.e. from pre-existing metadata.

Returns
-------
list
   a list with the min, max, mean, and standard deviation of values
   in the Band.

See Also
--------
:py:meth:`ComputeBandStats`
:py:meth:`ComputeRasterMinMax`
:py:meth:`GetMaximum`
:py:meth:`GetMinimum`
:py:meth:`GetStatistics`
";

%feature("docstring") GetUnitType "

Return a name for the units of this raster's values.
See :cpp:func:`GDALRasterBand::GetUnitType`.

Returns
-------
str

Examples
--------
>>> ds = gdal.GetDriverByName('MEM').Create('', 10, 10)
>>> ds.GetRasterBand(1).SetUnitType('ft')
0
>>> ds.GetRasterBand(1).GetUnitType()
'ft'
";

%feature("docstring")  HasArbitraryOverviews "

Check for arbitrary overviews.
See :cpp:func:`GDALRasterBand::HasArbitraryOverviews`.

Returns
-------
bool
";

%feature("docstring")  IsMaskBand "

Returns whether the band is a mask band.
See :cpp:func:`GDALRasterBand::IsMaskBand`.

Returns
-------
bool
";

%feature("docstring")  SetCategoryNames "

Set the category names for this band.
See :cpp:func:`GDALRasterBand::SetCategoryNames`.

Parameters
----------
papszCategoryNames : list

Returns
-------
int:
   :py:const:`CE_None` on success or :py:const:`CE_Failure` on failure.
";

%feature("docstring")  SetColorInterpretation "

Set color interpretation of the band
See :cpp:func:`GDALRasterBand::SetColorInterpretation`.

Parameters
----------
val : int
    A color interpretation code such as :py:const:`gdal.GCI_RedBand`

Returns
-------
int:
   :py:const:`CE_None` on success or :py:const:`CE_Failure` on failure.
";

%feature("docstring")  SetColorTable "

Set the raster color table.
See :cpp:func:`GDALRasterBand::SetColorTable`.

Parameters
----------
arg : ColorTable

Returns
-------
int:
   :py:const:`CE_None` on success or :py:const:`CE_Failure` on failure.
";

%feature("docstring")  SetDefaultHistogram "

Set default histogram.
See :cpp:func:`GDALRasterBand::SetDefaultHistogram`.

Parameters
----------
min : float
    minimum value
max : float
    maximum value
buckets_in : list
    list of pixel counts for each bucket

Returns
-------
int:
   :py:const:`CE_None` on success or :py:const:`CE_Failure` on failure.

See Also
--------
:py:meth:`SetHistogram`
";

%feature("docstring")  SetOffset "

Set scaling offset.
See :cpp:func:`GDALRasterBand::SetOffset`.

Parameters
----------
val : float

Returns
-------
int:
   :py:const:`CE_None` on success or :py:const:`CE_Failure` on failure.

See Also
--------
:py:meth:`SetScale`
";


%feature("docstring")  SetRasterColorTable "
Deprecated. Alternate name for :py:meth:`SetColorTable`.
";

%feature("docstring")  SetRasterColorInterpretation "
Deprecated.  Alternate name for :py:meth:`SetColorInterpretation`.
";

%feature("docstring")  SetRasterCategoryNames "
Deprecated.  Alternate name for :py:meth:`SetCategoryNames`.
";

%feature("docstring")  SetScale "
Set scaling ratio.
See :cpp:func:`GDALRasterBand::SetScale`.

Parameters
----------
val : float

Returns
-------
int:
   :py:const:`CE_None` on success or :py:const:`CE_Failure` on failure.

See Also
--------
:py:meth:`SetOffset`
";

%feature("docstring")  SetStatistics "

Set statistics on band.
See :cpp:func:`GDALRasterBand::SetStatistics`.

Parameters
----------
min : float
max : float
mean : float
stdev : float

Returns
-------
int:
   :py:const:`CE_None` on apparent success or :py:const:`CE_Failure` on
   failure.  This method cannot detect whether metadata will be properly saved and
   so may return :py:const:`gdal.`CE_None` even if the statistics will never be
   saved.

See Also
--------
:py:meth:`ComputeBandStats`
:py:meth:`ComputeRasterMinMax`
:py:meth:`ComputeStatistics`
:py:meth:`GetMaximum`
:py:meth:`GetMinimum`
:py:meth:`GetStatistics`
";

%feature("docstring")  SetUnitType "

Set unit type.
See :cpp:func:`GDALRasterBand::SetUnitType`.

Parameters
----------
val : str

Returns
-------
int:
   :py:const:`CE_None` on success or :py:const:`CE_Failure` on failure.
";

}
