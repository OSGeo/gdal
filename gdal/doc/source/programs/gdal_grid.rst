.. _gdal_grid:

================================================================================
gdal_grid
================================================================================

.. only:: html

    Creates regular grid from the scattered data.

.. Index:: gdal_grid

Synopsis
--------

.. code-block::

    gdal_grid [-ot {Byte/Int16/UInt16/UInt32/Int32/Float32/Float64/
              CInt16/CInt32/CFloat32/CFloat64}]
              [-of format] [-co "NAME=VALUE"]
              [-zfield field_name] [-z_increase increase_value] [-z_multiply multiply_value]
              [-a_srs srs_def] [-spat xmin ymin xmax ymax]
              [-clipsrc <xmin ymin xmax ymax>|WKT|datasource|spat_extent]
              [-clipsrcsql sql_statement] [-clipsrclayer layer]
              [-clipsrcwhere expression]
              [-l layername]* [-where expression] [-sql select_statement]
              [-txe xmin xmax] [-tye ymin ymax] [-tr xres yres] [-outsize xsize ysize]
              [-a algorithm[:parameter1=value1]*] [-q]
              <src_datasource> <dst_filename>

Description
-----------

This program creates regular grid (raster) from the scattered data read from
the OGR datasource. Input data will be interpolated to fill grid nodes with
values, you can choose from various interpolation methods.

It is possible to set the ``GDAL_NUM_THREADS``
configuration option to parallelize the processing. The value to specify is
the number of worker threads, or ``ALL_CPUS`` to use all the cores/CPUs of the
computer.

.. program:: gdal_grid

.. include:: options/ot.rst

.. include:: options/of.rst

.. option:: -txe <xmin> <xmax>

    Set georeferenced X extents of output file to be created.

.. option:: -tye <ymin> <ymax>

    Set georeferenced Y extents of output file to be created.

.. option:: -tr <xres> <yres>

    Set output file resolution (in target georeferenced units).
    Note that :option:`-tr` just works in combination with a valid input from :option:`-txe` and :option:`-tye`

    .. versionadded:: 3.2

.. option:: -outsize <xsize ysize>

    Set the size of the output file in pixels and lines.
    Note that :option:`-outsize` cannot be used with :option:`-tr`

.. option:: -a_srs <srs_def>

    Override the projection for the
    output file.  The <i>srs_def> may be any of the usual GDAL/OGR forms,
    complete WKT, PROJ.4, EPSG:n or a file containing the WKT.
    No reprojection is done.

.. option:: -zfield <field_name>

    Identifies an attribute field
    on the features to be used to get a Z value from. This value overrides Z value
    read from feature geometry record (naturally, if you have a Z value in
    geometry, otherwise you have no choice and should specify a field name
    containing Z value).

.. option:: -z_increase <increase_value>

    Addition to the attribute field
    on the features to be used to get a Z value from. The addition should be the same
    unit as Z value. The result value will be Z value + Z increase value. The default value is 0.

.. option:: -z_multiply <multiply_value>

    This is multiplication
    ratio for Z field. This can be used for shift from e.g. foot to meters or from
    elevation to deep. The result value will be (Z value + Z increase value) * Z multiply value.
    The default value is 1.

.. option:: -a <[algorithm[:parameter1=value1][:parameter2=value2]...]>

    Set the interpolation algorithm or data metric name and (optionally)
    its parameters. See `Interpolation algorithms`_ and `Data metrics`_
    sections for further discussion of available options.

.. option:: -spat <xmin> <ymin> <xmax> <ymax>

    Adds a spatial filter
    to select only features contained within the bounding box described by
    (xmin, ymin) - (xmax, ymax).

.. option:: -clipsrc [xmin ymin xmax ymax]|WKT|datasource|spat_extent

    Adds a spatial filter to select only features contained within the
    specified bounding box (expressed in source SRS), WKT geometry (POLYGON or
    MULTIPOLYGON), from a datasource or to the spatial extent of the :option:`-spat`
    option if you use the ``spat_extent`` keyword. When specifying a
    datasource, you will generally want to use it in combination of the
    :option:`-clipsrclayer`, :option:`-clipsrcwhere` or :option:`-clipsrcsql`
    options.

.. option:: -clipsrcsql <sql_statement>

    Select desired geometries using an SQL query instead.

.. option:: -clipsrclayer <layername>

    Select the named layer from the source clip datasource.

.. option:: -clipsrcwhere <expression>

    Restrict desired geometries based on attribute query.

.. option:: -l <layername>

    Indicates the layer(s) from the
    datasource that will be used for input features.  May be specified multiple
    times, but at least one layer name or a :option:`-sql` option must be
    specified.

.. option:: -where <expression>

    An optional SQL WHERE style query expression to be applied to select features
    to process from the input layer(s).

.. option:: -sql <select_statement>

    An SQL statement to be evaluated against the datasource to produce a
    virtual layer of features to be processed.

.. include:: options/co.rst

.. option:: -q

    Suppress progress monitor and other non-error output.

.. option:: <src_datasource>

    Any OGR supported readable datasource.

.. option:: <dst_filename>

    The GDAL supported output file.


Interpolation algorithms
------------------------

There are number of interpolation algorithms to choose from.

More details about them can also be found in :ref:`gdal_grid_tut`

.. _gdal_grid_invdist:

invdist
+++++++

Inverse distance to a power. This is default algorithm. It has following
parameters:

- ``power``: Weighting power (default 2.0).
- ``smoothing``: Smoothing parameter (default 0.0).
- ``radius1``: The first radius (X axis if rotation angle is 0)
  of search ellipse. Set this parameter to zero to use whole point array.
  Default is 0.0.
- ``radius2``: The second radius (Y axis if rotation angle is 0)
  of search ellipse. Set this parameter to zero to use whole point array.
  Default is 0.0.
- ``angle``: Angle of search ellipse rotation in degrees
  (counter clockwise, default 0.0).
- ``max_points``: Maximum number of data points to use. Do not
  search for more points than this number. This is only used if search ellipse
  is set (both radii are non-zero). Zero means that all found points should
  be used. Default is 0.
- ``min_points``: Minimum number of data points to use. If less
  amount of points found the grid node considered empty and will be filled with
  NODATA marker. This is only used if search ellipse is set (both radii are
  non-zero). Default is 0.
- ``nodata``: NODATA marker to fill empty points (default
  0.0).

invdistnn
+++++++++

.. versionadded:: 2.1

Inverse distance to a power with nearest neighbor searching, ideal when
max_points is used. It has following parameters:

- ``power``: Weighting power (default 2.0).
- ``smoothing``: Smoothing parameter (default 0.0).
- ``radius``: The radius of the search circle, which should be
  non-zero. Default is 1.0.
- ``max_points``: Maximum number of data points to use. Do not
  search for more points than this number. Found points will be ranked from
  nearest to furthest distance when weighting. Default is 12.
- ``min_points``: Minimum number of data points to use. If less
  amount of points found the grid node is considered empty and will be filled
  with NODATA marker. Default is 0.
- ``nodata``: NODATA marker to fill empty points (default
  0.0).


.. _gdal_grid_average:

average
+++++++

Moving average algorithm. It has following parameters:

- ``radius1``: The first radius (X axis if rotation angle is 0)
  of search ellipse. Set this parameter to zero to use whole point array.
  Default is 0.0.
- ``radius2``: The second radius (Y axis if rotation angle is 0)
  of search ellipse. Set this parameter to zero to use whole point array.
  Default is 0.0.
- ``angle``: Angle of search ellipse rotation in degrees
  (counter clockwise, default 0.0).
- ``min_points``: Minimum number of data points to use. If less
  amount of points found the grid node considered empty and will be filled with
  NODATA marker. Default is 0.
- ``nodata``: NODATA marker to fill empty points (default
  0.0).

Note, that it is essential to set search ellipse for moving average method. It
is a window that will be averaged when computing grid nodes values.

.. _gdal_grid_nearest:

nearest
+++++++

Nearest neighbor algorithm. It has following parameters:

- ``radius1``: The first radius (X axis if rotation angle is 0)
  of search ellipse. Set this parameter to zero to use whole point array.
  Default is 0.0.
- ``radius2``: The second radius (Y axis if rotation angle is 0)
  of search ellipse. Set this parameter to zero to use whole point array.
  Default is 0.0.
- ``angle``: Angle of search ellipse rotation in degrees
  (counter clockwise, default 0.0).
- ``nodata``: NODATA marker to fill empty points (default
  0.0).

linear
++++++

.. versionadded:: 2.1

Linear interpolation algorithm.

The Linear method performs linear interpolation by computing a Delaunay
triangulation of the point cloud, finding in which triangle of the triangulation
the point is, and by doing linear interpolation from its barycentric coordinates
within the triangle.
If the point is not in any triangle, depending on the radius, the
algorithm will use the value of the nearest point or the nodata value.

It has following parameters:

- ``radius``: In case the point to be interpolated does not fit
  into a triangle of the Delaunay triangulation, use that maximum distance to search a nearest
  neighbour, or use nodata otherwise. If set to -1, the search distance is infinite.
  If set to 0, nodata value will be always used. Default is -1.
- ``nodata``: NODATA marker to fill empty points (default
  0.0).

Data metrics
------------

Besides the interpolation functionality \ref gdal_grid can be used to compute
some data metrics using the specified window and output grid geometry. These
metrics are:

- ``minimum``: Minimum value found in grid node search ellipse.

- ``maximum``: Maximum value found in grid node search ellipse.

- ``range``: A difference between the minimum and maximum values
  found in grid node search ellipse.

- ``count``:  A number of data points found in grid node search ellipse.

- ``average_distance``: An average distance between the grid
  node (center of the search ellipse) and all of the data points found in grid
  node search ellipse.

- ``average_distance_pts``: An average distance between the data
  points found in grid node search ellipse. The distance between each pair of
  points within ellipse is calculated and average of all distances is set as a
  grid node value.

All the metrics have the same set of options:

- ``radius1``: The first radius (X axis if rotation angle is 0)
  of search ellipse. Set this parameter to zero to use whole point array.
  Default is 0.0.
- ``radius2``: The second radius (Y axis if rotation angle is 0)
  of search ellipse. Set this parameter to zero to use whole point array.
  Default is 0.0.
- ``angle``: Angle of search ellipse rotation in degrees
  (counter clockwise, default 0.0).
- ``min_points``: Minimum number of data points to use. If less
  amount of points found the grid node considered empty and will be filled with
  NODATA marker. This is only used if search ellipse is set (both radii are
  non-zero). Default is 0.
- ``nodata``: NODATA marker to fill empty points (default
  0.0).

Reading comma separated values
------------------------------

Often you have a text file with a list of comma separated XYZ values to work
with (so called CSV file). You can easily use that kind of data source in \ref
gdal_grid. All you need is create a virtual dataset header (VRT) for you CSV
file and use it as input datasource for \ref gdal_grid. You can find details
on VRT format at :ref:`vector.vrt` description page.

Here is a small example. Let we have a CSV file called <i>dem.csv>
containing

::

    Easting,Northing,Elevation
    86943.4,891957,139.13
    87124.3,892075,135.01
    86962.4,892321,182.04
    87077.6,891995,135.01
    ...

For above data we will create <i>dem.vrt> header with the following
content:

.. code-block:: xml

    <OGRVRTDataSource>
        <OGRVRTLayer name="dem">
            <SrcDataSource>dem.csv</SrcDataSource>
            <GeometryType>wkbPoint</GeometryType>
            <GeometryField encoding="PointFromColumns" x="Easting" y="Northing" z="Elevation"/>
        </OGRVRTLayer>
    </OGRVRTDataSource>

This description specifies so called 2.5D geometry with three coordinates X, Y
and Z. Z value will be used for interpolation. Now you can use <i>dem.vrt>
with all OGR programs (start with \ref ogrinfo to test that everything works
fine). The datasource will contain single layer called <i>"dem"> filled
with point features constructed from values in CSV file. Using this technique
you can handle CSV files with more than three columns, switch columns, etc.

If your CSV file does not contain column headers then it can be handled in the
following way:

.. code-block:: xml

    <GeometryField encoding="PointFromColumns" x="field_1" y="field_2" z="field_3"/>

The :ref:`vector.csv` description page contains
details on CSV format supported by GDAL/OGR.

C API
-----

This utility is also callable from C with :cpp:func:`GDALGrid`.

Examples
--------

The following would create raster TIFF file from VRT datasource described in
`Reading comma separated values`_ section using the inverse distance to a power method.
Values to interpolate will be read from Z value of geometry record.

::

    gdal_grid -a invdist:power=2.0:smoothing=1.0 -txe 85000 89000 -tye 894000 890000 -outsize 400 400 -of GTiff -ot Float64 -l dem dem.vrt dem.tiff

The next command does the same thing as the previous one, but reads values to
interpolate from the attribute field specified with <b>-zfield</b> option
instead of geometry record. So in this case X and Y coordinates are being
taken from geometry and Z is being taken from the <i>"Elevation"> field.
The GDAL_NUM_THREADS is also set to parallelize the computation.

::

    gdal_grid -zfield "Elevation" -a invdist:power=2.0:smoothing=1.0 -txe 85000 89000 -tye 894000 890000 -outsize 400 400 -of GTiff -ot Float64 -l dem dem.vrt dem.tiff --config GDAL_NUM_THREADS ALL_CPUS

