.. _gdal_raster_pixel_info:

================================================================================
``gdal raster pixel-info``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Return information on a pixel of a raster dataset

.. Index:: gdal raster pixel-info

Synopsis
--------

.. program-output:: gdal raster pixel-info --help-doc

Description
-----------

:program:`gdal raster pixel-info` provide a mechanism to query information about
a pixel given its location in one of a variety of coordinate systems.

It supports outputting either as GeoJSON or CSV.

The following items will be reported (when known):

- Input coordinates
- Input coordinates converted to column, line
- Pixel value per selected band(s), with unscaled value
- For VRT files, which file(s) contribute to the pixel value.

The following options are available:

Program-Specific Options
------------------------

.. option:: -b <band>

    Selects a band to query. Multiple bands can be listed. By default all
    bands are queried.

.. option:: -f, --of, --format, --output-format geojson|csv

    Which output format to use. Default is GeoJSON.

.. option:: --ovr, --overview <index>

    Query the (overview_level)th overview (overview_level=0 is the 1st overview),
    instead of the base band. Note that the x,y location (if the coordinate system is
    pixel/line) must still be given with respect to the base band.

.. option:: -p, --pos, --position <column,line> or <X,Y>

    Required.
    This can be specified either as an option, a positional value after the
    dataset name, or when called from :program:`gdal`, as (space separated)
    values provided on the standard input.

    By default, when :option:`--position-crs` is not specified, or set to ``pixel``,
    this is a column, line tuple (possibly with fractional part).
    If :option:`--position-crs` is set to ``dataset``, this is a georeferenced
    coordinate expressed in the CRS of the dataset.
    If :option:`--position-crs` is specified to a CRS definition, this is a
    georeferenced coordinate expressed in this CRS.

    X means always longitude or easting, Y means always latitude or northing.

    Several x,y tuples may be specified.

.. option:: --position-crs pixel|dataset|<crs-def>

    CRS of position, or one of two following special values:

    - ``pixel`` means that the position is set as column, line (default)

    - ``dataset`` means that the position is a georeferenced
      coordinate expressed in the CRS of the dataset.

.. option:: --promote-pixel-value-to-z

    .. versionadded:: 3.13

    Whether to set the pixel value as Z component of GeoJSON geometry.
    Only applies if a single band is selected, and for GeoJSON output format.

.. option:: -r, --resampling nearest|bilinear|cubic|cubicspline

    Select a sampling algorithm. The default is ``nearest``.

    The available methods are:

    - ``nearest`` applies a nearest neighbour.

    - ``bilinear`` applies a bilinear convolution kernel.

    - ``cubic`` applies a cubic convolution kernel.

    - ``cubicspline`` applies a B-Spline convolution kernel.

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/if.rst

Examples
--------

.. example::
   :title: Reporting on pixel column=5, line=10 on the file :file:`byte.tif`

   .. command-output:: gdal raster pixel-info byte.tif 5 10
      :cwd: ../../../autotest/gcore/data

.. example::
   :title: Reporting on point at UTM 11N coordinates easting=441320 and northing=3750720 on the file :file:`byte.tif`

   .. command-output:: gdal raster pixel-info --position-crs=dataset byte.tif 441320 3750720
      :cwd: ../../../autotest/gcore/data

.. example::
   :title: Reporting on point at WGS84 coordinates longitude=-117.6355 and latitude=33.8970 on the file :file:`byte.tif`, with CSV output format

   .. command-output:: gdal raster pixel-info --of=csv --position-crs=WGS84 byte.tif -117.6355 33.8970
      :cwd: ../../../autotest/gcore/data

.. example::
   :title: Reporting on point at WGS84 coordinates provided on the standard input with longitude, latitude order.

   .. command-output:: echo -117.6355 33.8970 | gdal raster pixel-info --of=csv --position-crs=WGS84 byte.tif
      :cwd: ../../../autotest/gcore/data
