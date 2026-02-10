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

The following items will be reported (when known):

- Input coordinates
- Input coordinates converted to column, line
- Pixel value per selected band(s), with unscaled value
- For VRT files, which file(s) contribute to the pixel value (only for GeoJSON output)

There are 3 possible ways of providing input location(s):

- from input stream, with a "X Y" pair per line, potentially followed by other text.
- though the :option:`--position` argument.
- through a GDAL vector dataset through the :option:`--position-dataset` argument.

There are 2 possibilities of outputting pixel information:

- either on the output stream as CSV or GeoJSON (default to GeoJSON)
- or as a GDAL vector dataset whose name is specified with the :option:`--output` argument.

Since GDAL 3.13, this algorithm can be part of a :ref:`gdal_pipeline`. In that
case, only reading coordinates from a GDAL vector dataset and outputting results
to one is supported.

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. versionadded:: 3.13

.. include:: gdal_cli_include/gdalg_vector_compatible_non_natively_streamable.rst

The following options are available:

Program-Specific Options
------------------------

.. option:: -b <band>

    Selects a band to query. Multiple bands can be listed. By default all
    bands are queried.

.. option:: -f, --of, --format, --output-format <FORMAT>

    Which vector output format to use.

    When :option:`--output` is not specified, ``csv`` or ``geojson`` are the
    only two supported choices, with ``geojson`` being the default.
    Otherwise, any GDAL-supported vector format with write capabilities is
    supported.

.. option:: --position-dataset <COORDINATE-DATASET>

    .. versionadded:: 3.13

    GDAL-compatible vector dataset from which input coordinates are read.
    If the input feature has a point geometry, its location is used to query
    the raster. For other geometry types, the centroid is used. To read all
    pixel values for non-point geometries, use :ref:`gdal_raster_zonal_stats`.
    By default, all attribute fields are copied to the output.

    If the :option:`--position-crs` argument is specified, the coordinates of
    the point are interpreted according to the value of that argument.

    Otherwise, if the input vector layer has a CRS, coordinates are assumed to
    be expressed in it.

    Otherwise, coordinates are assumed to be in the CRS raster dataset if it has
    a CRS.

    Otherwise, coordinates are assumed to be expressed in the column,line raster space.

.. option:: -l, --layer, --input-layer <INPUT-LAYER>

    .. versionadded:: 3.13

    Input layer name of the dataset specified with :option:`--position-dataset` to use.

.. option:: -o, --output <OUTPUT>

    .. versionadded:: 3.13

    Output vector dataset (created by algorithm)

.. option:: --include-field <FIELD>

    .. versionadded:: 3.13

    Name of the field(s) from :option:`--position-dataset` to include into the output.
    By default, all fields are included (which corresponds to special value ``ALL``).
    Special value ``NONE`` can be used to mean that no field from the position
    dataset must be included in the output.

.. option:: --ovr, --overview <index>

    Query the (overview_level)th overview (overview_level=0 is the 1st overview),
    instead of the base band. Note that the x,y location (if the coordinate system is
    pixel/line) must still be given with respect to the base band.

.. option:: -p, --pos, --position <column,line> or <X,Y>

    Required, if :option:`--position-dataset` is not specified and no content
    is provided through the input stream.

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

    .. include:: gdal_options/co.rst

    .. include:: gdal_options/lco.rst

    .. include:: gdal_options/overwrite.rst

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

.. example::
   :title: Reading coordinates to extract from an input GeoPackage file and writing the output to a GeoPackage file

   .. code-block:: bash

       gdal raster pixel-info --position-dataset input.gpkg --input byte.tif --output output.gpkg

.. example::
   :title: Getting pixel values from a on-the-fly resized raster dataset from coordinates in :file:`input.gpkg`.

   .. code-block:: bash

       gdal pipeline read byte.tif ! resize --size 50%,50% -r cubic ! pixel-info input.gpkg ! write output.gpkg

.. example::
   :title: Getting pixel values from coordinates in a piped vector dataset, using the ``_`` placeholder dataset name

   .. code-block:: bash

       gdal pipeline read input.gml ! swap-xy ! pixel-info --input byte.tif --position-dataset _ ! write output.gpkg
