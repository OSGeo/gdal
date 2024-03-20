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
bool
    True if the two points are within Line of Sight.
";
