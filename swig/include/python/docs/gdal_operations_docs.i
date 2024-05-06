// gdal.FillNodata
%feature("docstring") FillNodata "

Fill selected raster regions by interpolation from the edges.

This algorithm will interpolate values for all designated
nodata pixels (marked by zeros in ``maskBand``). For each pixel
a four direction conic search is done to find values to interpolate
from (using inverse distance weighting by default). Once all values are
interpolated, zero or more smoothing iterations (3x3 average
filters on interpolated pixels) are applied to smooth out
artifacts.

This algorithm is generally suitable for interpolating missing
regions of fairly continuously varying rasters (such as elevation
models for instance). It is also suitable for filling small holes
and cracks in more irregularly varying images (like airphotos). It
is generally not so great for interpolating a raster from sparse
point data. See :py:func:`Grid` for that case.

See :cpp:func:`GDALFillNodata`.

Parameters
----------
targetBand : Band
    Band containing values to fill. Will be modified in-place.
maskBand : Band
    Mask band with a value of 0 indicating values that should be filled.
    If not specified, the mask band associated with ``targetBand`` will be used.
maxSearchDist : float
    the maximum number of pixels to search in all directions to find values to interpolate from.
smoothingIterations : int
    the number of 3x3 smoothing filter passes to run (0 or more)
options : dict/list, optional
    A dict or list of name=value options. Available options are
    described in :cpp:func:`GDALFillNodata`.
callback : function, optional
   A progress callback function
callback_data: optional
   Optional data to be passed to callback function

Returns
-------
int
   :py:const:`CE_None` on success or :py:const:`CE_Failure` on failure.

Examples
--------
>>> import numpy as np
>>> data = np.array([[1, 2], [9, 9], [9, 9], [3, 4]], dtype=np.float32)
>>> ds = gdal.GetDriverByName('MEM').Create('', 2, 4, eType=gdal.GDT_Float32)
>>> ds.WriteArray(data)
0
>>> mask = data != 9  # replace pixels with value = 9
>>> mask_ds = gdal.GetDriverByName('MEM').Create('', 2, 4, eType=gdal.GDT_Byte)
>>> mask_ds.WriteArray(mask)
0
>>> gdal.FillNodata(ds.GetRasterBand(1), mask_ds.GetRasterBand(1), 5, 0)
0
>>> ds.ReadAsArray()
array([[1.       , 2.       ],
       [2.1485982, 2.6666667],
       [2.721169 , 3.3333333],
       [3.       , 4.       ]], dtype=float32)
>>> gdal.FillNodata(ds.GetRasterBand(1), mask_ds.GetRasterBand(1), 5, 0, {'INTERPOLATION':'NEAREST'})
0
>>> ds.ReadAsArray()
array([[1., 2.],
       [1., 2.],
       [3., 4.],
       [3., 4.]], dtype=float32)

";

// gdal.IsLineOfSightVisible
%feature("docstring") IsLineOfSightVisible "

Check Line of Sight between two points.
Both input coordinates must be within the raster coordinate bounds.

 See :cpp:func:`GDALIsLineOfSightVisible`.

.. versionadded:: 3.9

Parameters
----------
band : gdal.RasterBand
    The band to read the DEM data from. This must NOT be null.
xA : int
    The X location (raster column) of the first point to check on the raster.
yA : int
    The Y location (raster row) of the first point to check on the raster.
zA : float
    The Z location (height) of the first point to check.
xB : int
    The X location (raster column) of the second point to check on the raster.
yB : int
    The Y location (raster row) of the second point to check on the raster.
zB : float
    The Z location (height) of the second point to check.
options : dict/list, optional
    A dict or list of name=value of options for the line of sight algorithm (currently ignored).

Returns
-------
collections.namedtuple(is_visible: bool, col_intersection: int, row_intersection: int)
    is_visible is True if the two points are within Line of Sight.
    col_intersection is the raster column index where the LOS line intersects with terrain (will be set in the future, currently set to -1).
    row_intersection is the raster row index where the LOS line intersects with terrain (will be set in the future, currently set to -1).
";
