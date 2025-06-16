.. _gdal_vector_grid:

================================================================================
``gdal vector grid``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Create a regular grid from scattered points

.. Index:: gdal vector grid

Synopsis
--------

.. program-output:: gdal vector grid --help-doc

Description
-----------

This program creates a regular grid (raster) from the scattered data read from
a vector dataset. Input data will be interpolated to fill grid nodes with
values, you can choose from various interpolation methods.

It is possible to set the :config:`GDAL_NUM_THREADS`
configuration option to parallelize the processing. The value to specify is
the number of worker threads, or ``ALL_CPUS`` to use all the cores/CPUs of the
computer.

Since GDAL 3.12, this algorithm can be part of a :ref:`gdal_pipeline`.

Options common to all algorithms
--------------------------------

Standard options
++++++++++++++++

.. include:: gdal_options/of_vector.rst

.. include:: gdal_options/co_vector.rst

.. include:: gdal_options/overwrite.rst

.. option:: -l, --layer, --layer-name <LAYER-NAME>

    Indicates the layer(s) from the datasource that will be used for input features.
    May be specified multiple times

.. option:: --sql <SQL>|@<filename>

    An SQL statement to be evaluated against the datasource to produce a virtual layer of features to be burned in.
    The @filename syntax can be used to indicate that the content is in the pointed filename.

.. option:: --extent <xmin>,<ymin>,<xmax>,<ymax>

    Set georeferenced extents. The values must be expressed in georeferenced units.
    If not specified, the extent of the output file will be the extent of the vector layers.

.. option:: --resolution <xres>,<yres>

    Set target resolution. The values must be expressed in georeferenced units.
    Both must be positive values. Note that `--resolution` cannot be used with `--size`.

.. option:: --size <xsize>,<ysize>

    Set output file size in pixels and lines. Note that `--size` cannot be used with `--resolution`.

.. option:: --ot, --datatype, --output-data-type <OUTPUT-DATA-TYPE>

    Force the output bands to be of the indicated data type.
    Defaults to ``Float64``.

.. option:: --crs <CRS>

    Override the projection for the output file. If not specified, the projection of the input vector file will be used if available. When using this option, no reprojection of features from the CRS of the input vector to the specified CRS of the output raster, so use only this option to correct an invalid source CRS. The ``<CRS>`` may be any of the usual GDAL/OGR forms, complete WKT, PROJ.4, EPSG:n or a file containing the WKT.

.. option:: --bbox <xmin>,<ymin>,<xmax>,<ymax>

    Select only points contained within the specified bounding box.

.. option:: --zfield <field_name>

    Identifies an attribute field
    on the features to be used to get a Z value from. This value overrides the Z value
    read from the feature geometry record (naturally, if you have a Z value in
    the geometry, otherwise you have no choice and should specify a field name
    containing a Z value).

.. option:: --zincrease <increase_value>

    Addition to the attribute field
    on the features to be used to get a Z value from. The addition should be the same
    unit as the Z value. The result value will be Z value + Z increase value. The default value is 0.

.. option:: --zmultiply <multiply_value>

    This is multiplication
    ratio for the Z field. This can be used for a shift from e.g. feet to meters or from
    elevation to depth. The result value will be (Z value + Z increase value) * Z multiply value.
    The default value is 1.

Advanced options
++++++++++++++++

.. include:: gdal_options/oo.rst

.. include:: gdal_options/if.rst


"invdist" algorithm
-------------------

Interpolation using an inverse distance to a power.

When :option:`--min-points-per-quadrant` or :option:`--max-points-per-quadrant`
is specified, the actual algorithm used is "invdistnn".

Options
+++++++

.. option:: --power <val>

    Weighting power (default 2.0).

.. option:: --smoothing <val>

    Smoothing parameter (default 0.0).

.. option:: --radius <val>

    Set first and second radius (mutually exclusive with :option:`--radius1` and :option:`--radius2`.
    By default, uses the whole point array.

.. option:: --radius1 <val>

    The first radius (X axis if rotation angle is 0) of the search ellipse.
    By default, uses the whole point array.

.. option:: --radius2 <val>

    The second radius (Y axis if rotation angle is 0) of the search ellipse.
    By default, uses the whole point array.

.. option:: --angle <val>

    Angle of search ellipse rotation in degrees (counter clockwise, default is 0).

.. option:: --max-points <val>

    Maximum number of data points to use. Do not search for more points than this number.
    This may only used if the search ellipse is set (both radii are non-zero).
    By default, no limitation.

.. option:: --min-points <val>

    Minimum number of data points to use. If less points in the search ellipse
    than the specified value are found, the grid node considered empty and will
    be filled with the nodata value.
    This may only used if the search ellipse is set (both radii are non-zero).
    By default, no limitation.

.. option:: --max-points-per-quadrant <val>

    Maximum number of data points to use per quadrant.
    By default, no limitation.
    When specified, the algorithm will only take into account up to max_points_per_quadrant
    points for each of the right-top, left-top, right-bottom and right-top quadrant
    relative to the point being interpolated.

.. option:: --min-points-per-quadrant <val>

    Minimum number of data points to use per quadrant.
    By default, no limitation.
    When specified, the algorithm will collect at least min_points_per_quadrant
    points for each of the right-top, left-top, right-bottom and right-top quadrant
    relative to the point being interpolated.

.. option:: --nodata <val>

    Nodata value to fill empty points (default is 0).


"invdistnn" algorithm
---------------------

Interpolation using an inverse distance to a power with nearest neighbor
searching, ideal when :option:`--max-points` is used.

When :option:`--min-points-per-quadrant` or :option:`--max-points-per-quadrant` is specified, the
search will start with the closest point to the point being interpolated
from the first quadrant, then the closest point to the point being interpolated
from the second quadrant, etc. up to the 4th quadrant, and will continue with
the next closest point in the first quadrant, etc. until :option:`--max-points` and/or
:option:`--max-points-per-quadrant` thresholds are reached.

Required option
+++++++++++++++

.. option:: --radius <val>

    Set search radius (mutually exclusive with :option:`--radius1` and :option:`--radius2`.
    Required.

Options
+++++++

.. option:: --power <val>

    Weighting power (default 2.0).

.. option:: --smoothing <val>

    Smoothing parameter (default 0.0).

.. option:: --max-points <val>

    Maximum number of data points to use. Do not search for more points than this number.
    This may only used if the search ellipse is set (both radii are non-zero).
    By default, no limitation.

.. option:: --min-points <val>

    Minimum number of data points to use. If less points in the search ellipse
    than the specified value are found, the grid node considered empty and will
    be filled with the nodata value.
    This may only used if the search ellipse is set (both radii are non-zero).
    By default, no limitation.

.. option:: --max-points-per-quadrant <val>

    Maximum number of data points to use per quadrant.
    By default, no limitation.
    When specified, the algorithm will only take into account up to max_points_per_quadrant
    points for each of the right-top, left-top, right-bottom and right-top quadrant
    relative to the point being interpolated.

.. option:: --min-points-per-quadrant <val>

    Minimum number of data points to use per quadrant.
    By default, no limitation.
    When specified, the algorithm will collect at least min_points_per_quadrant
    points for each of the right-top, left-top, right-bottom and right-top quadrant
    relative to the point being interpolated.

.. option:: --nodata <val>

    Nodata value to fill empty points (default is 0).


"average" algorithm
-------------------

Interpolation using a moving average algorithm.

When :option:`--min-points-per-quadrant` or :option:`--max-points-per-quadrant` is specified, the
search will start with the closest point to the point being interpolated
from the first quadrant, then the closest point to the point being interpolated
from the second quadrant, etc. up to the 4th quadrant, and will continue with
the next closest point in the first quadrant, etc. until :option:`--max-points` and/or
:option:`--max-points-per-quadrant` thresholds are reached.

Options
+++++++

.. option:: --radius <val>

    Set first and second radius (mutually exclusive with :option:`--radius1` and :option:`--radius2`.
    By default, uses the whole point array.

.. option:: --radius1 <val>

    The first radius (X axis if rotation angle is 0) of the search ellipse.
    By default, uses the whole point array.

.. option:: --radius2 <val>

    The second radius (Y axis if rotation angle is 0) of the search ellipse.
    By default, uses the whole point array.

.. option:: --angle <val>

    Angle of search ellipse rotation in degrees (counter clockwise, default is 0).

.. option:: --max-points <val>

    Maximum number of data points to use. Do not search for more points than this number.
    This may only used if the search ellipse is set (both radii are non-zero).
    By default, no limitation.

.. option:: --min-points <val>

    Minimum number of data points to use. If less points in the search ellipse
    than the specified value are found, the grid node considered empty and will
    be filled with the nodata value.
    This may only used if the search ellipse is set (both radii are non-zero).
    By default, no limitation.

.. option:: --max-points-per-quadrant <val>

    Maximum number of data points to use per quadrant.
    By default, no limitation.
    When specified, the algorithm will only take into account up to max_points_per_quadrant
    points for each of the right-top, left-top, right-bottom and right-top quadrant
    relative to the point being interpolated.

.. option:: --min-points-per-quadrant <val>

    Minimum number of data points to use per quadrant.
    By default, no limitation.
    When specified, the algorithm will collect at least min_points_per_quadrant
    points for each of the right-top, left-top, right-bottom and right-top quadrant
    relative to the point being interpolated.

.. option:: --nodata <val>

    Nodata value to fill empty points (default is 0).


"nearest" algorithm
-------------------

Interpolation using nearest neighbor algorithm.

Options
+++++++

.. option:: --radius <val>

    Set first and second radius (mutually exclusive with :option:`--radius1` and :option:`--radius2`.
    By default, uses the whole point array.

.. option:: --radius1 <val>

    The first radius (X axis if rotation angle is 0) of the search ellipse.
    By default, uses the whole point array.

.. option:: --radius2 <val>

    The second radius (Y axis if rotation angle is 0) of the search ellipse.
    By default, uses the whole point array.

.. option:: --angle <val>

    Angle of search ellipse rotation in degrees (counter clockwise, default is 0).

.. option:: --nodata <val>

    Nodata value to fill empty points (default is 0).


"linear" algorithm
------------------

Linear interpolation by computing a Delaunay triangulation of the point cloud,
finding in which triangle of the triangulation the point is, and by doing
linear interpolation from its barycentric coordinates
within the triangle.
If the point is not in any triangle, depending on the radius, the
algorithm will use the value of the nearest point or the nodata value.

Options
+++++++

.. option:: --radius <val>

    In case the point to be interpolated does not fit into a triangle of the
    Delaunay triangulation, use that maximum distance to search a nearest
    neighbour, or use nodata otherwise.
    If unset, the search distance is infinite.
    If set to 0, nodata value will be always used.

.. option:: --nodata <val>

    Nodata value to fill empty points (default is 0).


Data metrics algorithms
-----------------------

Besides the interpolation functionality :program:`gdal vector grid` can be used to compute
some data metrics using the specified window and output grid geometry. These
metrics are:

- ``minimum``: Minimum value found in grid node search ellipse.

- ``maximum``: Maximum value found in grid node search ellipse.

- ``range``: A difference between the minimum and maximum values
  found in grid node search ellipse.

- ``count``:  A number of data points found in grid node search ellipse.

- ``average-distance``: An average distance between the grid
  node (center of the search ellipse) and all of the data points found in grid
  node search ellipse.

- ``average-distance-points``: An average distance between the data
  points found in grid node search ellipse. The distance between each pair of
  points within ellipse is calculated and average of all distances is set as a
  grid node value.

All the metrics have the same set of options:

.. option:: --radius <val>

    Set first and second radius (mutually exclusive with :option:`--radius1` and :option:`--radius2`.
    By default, uses the whole point array.

.. option:: --radius1 <val>

    The first radius (X axis if rotation angle is 0) of the search ellipse.
    By default, uses the whole point array.

.. option:: --radius2 <val>

    The second radius (Y axis if rotation angle is 0) of the search ellipse.
    By default, uses the whole point array.

.. option:: --angle <val>

    Angle of search ellipse rotation in degrees (counter clockwise, default is 0).

.. option:: --min-points <val>

    Minimum number of data points to use. If less points in the search ellipse
    than the specified value are found, the grid node considered empty and will
    be filled with the nodata value.
    This may only used if the search ellipse is set (both radii are non-zero).
    By default, no limitation.

.. option:: --max-points-per-quadrant <val>

    Maximum number of data points to use per quadrant.
    By default, no limitation.
    When specified, the algorithm will only take into account up to max_points_per_quadrant
    points for each of the right-top, left-top, right-bottom and right-top quadrant
    relative to the point being interpolated.

.. option:: --min-points-per-quadrant <val>

    Minimum number of data points to use per quadrant.
    By default, no limitation.
    If that number is not reached, the point being interpolated will be set with
    the nodata value.
    When specified, the algorithm will collect at least min_points_per_quadrant
    points for each of the right-top, left-top, right-bottom and right-top quadrant
    relative to the point being interpolated.

.. option:: --nodata <val>

    Nodata value to fill empty points (default is 0).


When :option:`--min-points-per-quadrant` or :option:`--max-points-per-quadrant` is specified, the
search will start with the closest point to the point being interpolated
from the first quadrant, then the closest point to the point being interpolated
from the second quadrant, etc. up to the 4th quadrant, and will continue with
the next closest point in the first quadrant, etc. until :option:`--max-points` and/or
:option:`--max-points-per-quadrant` thresholds are reached.

Reading comma separated values
------------------------------

Often you have a text file with a list of comma separated XYZ values to work
with (so called CSV file). You can easily use that kind of data source in
:program:`gdal vector grid`. All you need is to create a virtual dataset header (VRT) for your CSV
file and use it as an input datasource for :program:`gdal vector grid`. You can find details
on the VRT format on the :ref:`vector.vrt` description page.

Here is a small example. Let's say we have a CSV file called *dem.csv*
containing

::

    Easting,Northing,Elevation
    86943.4,891957,139.13
    87124.3,892075,135.01
    86962.4,892321,182.04
    87077.6,891995,135.01
    ...

For the above data we will create a *dem.vrt* header with the following
content:

.. code-block:: xml

    <OGRVRTDataSource>
        <OGRVRTLayer name="dem">
            <SrcDataSource>dem.csv</SrcDataSource>
            <GeometryType>wkbPoint</GeometryType>
            <GeometryField encoding="PointFromColumns" x="Easting" y="Northing" z="Elevation"/>
        </OGRVRTLayer>
    </OGRVRTDataSource>

This description specifies so called 2.5D geometry with  three  coordinates
X,  Y and Z. The Z value will be used for interpolation. Now you can
use *dem.vrt* with all OGR programs (start  with  :ref:`ogrinfo`  to  test  that
everything works fine). The datasource will contain a single layer called
*"dem"*  filled  with point features constructed from values in the CSV file.
Using this technique you can handle CSV  files  with  more  than  three
columns, switch columns, etc. OK, now the final step:

.. code-block::

    gdal vector grid invdist dem.vrt demv.tif

Or, if we do not wish to use a VRT file:

.. code-block::

    gdal vector grid invdist -l dem -oo X_POSSIBLE_NAMES=Easting \
    -oo Y_POSSIBLE_NAMES=Northing --zfield=Elevation dem.csv dem.tif

If your CSV file does not contain column headers then it can be handled
in the VRT file in the following way:

.. code-block:: xml

    <GeometryField encoding="PointFromColumns" x="field_1" y="field_2" z="field_3"/>

The :ref:`vector.csv` description page contains details on CSV format supported by GDAL.

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. versionadded:: 3.12

.. include:: gdal_cli_include/gdalg_raster_compatible_non_natively_streamable.rst


Examples
--------

.. example::
   :title: Create a raster from a VRT datasource using inverse distance to a power

   .. code-block:: bash

       gdal vector grid invdist --power=2.0 --smoothing=1.0 --extent=85000,894000,89000,890000 \
           --size=400,400 -l dem dem.vrt dem.tif

   This example creates a raster TIFF file from the VRT datasource described in
   `Reading comma separated values`_ section using the inverse distance to a power method.
   Values to interpolate will be read from Z value of geometry record.

.. example::
   :title: Read values to interpolate from an attribute field

   .. code-block:: bash

       gdal vector grid invdist --zfield=Elevation --config GDAL_NUM_THREADS ALL_CPUS \
           --power=2.0 --smoothing=1.0 --extent=85000,894000,89000,890000 \
           --size=400,400 -l dem dem.vrt dem.tif

   This command does the same thing as the previous one, but reads values to
   interpolate from the attribute field specified with :option:`--zfield` option
   instead of geometry record. So in this case X and Y coordinates are being
   taken from geometry and Z is being taken from the *"Elevation"* field.
   The :config:`GDAL_NUM_THREADS` is also set to parallelize the computation.
