.. _gdal_contour:

================================================================================
gdal_contour
================================================================================

.. only:: html

    Builds vector contour lines from a raster elevation model.

.. Index:: gdal_contour

Synopsis
--------

.. code-block::

    gdal_contour [--help] [--help-general]
                 [-b <band>] [-a <attribute_name>] [-amin <attribute_name>] [-amax <attribute_name>]
                 [-3d] [-inodata] [-snodata <n>] [-f <formatname>] [-i <interval>]
                 [-dsco <NAME>=<VALUE>]... [-lco <NAME>=<VALUE>]...
                 [-off <offset>] [-fl <level> <level>...] [-e <exp_base>]
                 [-nln <outlayername>] [-q] [-p] [-gt <n>|unlimited]
                 <src_filename> <dst_filename>

Description
-----------

The :program:`gdal_contour` generates a vector contour file from the input
raster elevation model (DEM).

The contour line-strings are oriented consistently and the high side will
be on the right, i.e. a line string goes clockwise around a top.

.. program:: gdal_contour

.. include:: options/help_and_help_general.rst

.. option:: -b <band>

    Picks a particular band to get the DEM from. Defaults to band 1.

.. option:: -a <name>

    Provides a name for the attribute in which to put the elevation.
    If not provided no elevation attribute is attached.
    Ignored in polygonal contouring (:option:`-p`) mode.

.. option:: -amin <name>

    Provides a name for the attribute in which to put the minimum elevation
    of contour polygon. If not provided no minimum elevation attribute
    is attached. Ignored in default line contouring mode.

    .. versionadded:: 2.4.0

.. option:: -amax <name>

    Provides a name for the attribute in which to put the maximum elevation of
    contour polygon. If not provided no maximum elevation attribute is attached.
    Ignored in default line contouring mode.

    .. versionadded:: 2.4.0

.. option:: -3d

    Force production of 3D vectors instead of 2D.
    Includes elevation at every vertex.

.. option:: -inodata

    Ignore any nodata value implied in the dataset - treat all values as valid.

.. option:: -snodata <value>

    Input pixel value to treat as "nodata".

.. option:: -f <format>

    Create output in a particular format.

    .. versionadded:: 2.3.0

        If not specified, the format is guessed from the extension (previously was ESRI Shapefile).

.. option:: -dsco <NAME>=<VALUE>

    Dataset creation option (format specific)

.. option:: -lco <NAME>=<VALUE>

    Layer creation option (format specific)

.. option:: -i <interval>

    Elevation interval between contours.
    Must specify either :option:`-i` or :option:`-fl` or :option:`-e`.

.. option:: -off <offset>

    Offset from zero relative to which to interpret intervals.

    For example, `-i 100` requests contours at ...-100, 0, 100...
    Further adding `-off 25` makes that request instead ...-75, 25, 125...

.. option:: -fl <level>

    Name one or more "fixed levels" to extract, in ascending order separated by spaces.

.. option:: -e <base>

    Generate levels on an exponential scale: `base ^ k`, for `k` an integer.
    Must specify either -i or -fl or -e.

    .. versionadded:: 2.4.0

.. option:: -nln <name>

    Provide a name for the output vector layer. Defaults to "contour".

.. option:: -p

    Generate contour polygons rather than contour lines.

    When this mode is selected the polygons are created for values between
    each level specified by :option:`-i` or :option:`-fl` or :option:`-e`,
    in case :option:`-fl` is used alone at least two fixed levels must be specified.

    The minimum and maximum values from the raster are not automatically added to
    the fixed levels list but the special values ``MIN`` and ``MAX`` (case insensitive)
    can be used to include them.


    .. versionadded:: 2.4.0

.. option:: -gt <n>

    Group n features per transaction (default 100 000). Increase the value for
    better performance when writing into DBMS drivers that have transaction
    support. ``n`` can be set to unlimited to load the data into a single
    transaction. If set to 0, no explicit transaction is done.

    .. versionadded:: 3.10

.. option:: -q

    Be quiet: do not print progress indicators.

C API
-----

Functionality of this utility can be done from C with :cpp:func:`GDALContourGenerate`.

Examples
--------

.. example::

    :title: Creating contours from a DEM

    .. code-block::

        gdal_contour -a elev dem.tif contour.shp -i 10.0

    This would create 10-meter contours from the DEM data in :file:`dem.tif` and
    produce a shapefile in :file:`contour.shp|shx|dbf` with the contour elevations
    in the ``elev`` attribute.

.. example::

    :title: Creating polygonal contours from a DEM

    .. code-block:: bash

        $ cat test.asc
        ncols        2
        nrows        2
        xllcorner    0
        yllcorner    0
        cellsize     1
        4 15
        25 36

        $ gdal_contour test.asc -f GeoJSON /vsistdout/ -i 10 -p -amin min -amax max

    This would create 10-meter polygonal contours from the DEM data in :file:`test.asc`
    and produce a GeoJSON output with the contour min and max elevations in the ``min``
    and ``max`` attributes, including the minimum and maximum values from the raster.

    .. code-block:: bash

        {
            "type": "FeatureCollection",
            "name": "contour",
            "features": [
            { "type": "Feature", "properties": { "ID": 0, "min": 4.0, "max": 10.0 }, "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 0.5, 1.214285714285714 ], [ 1.045454545454545, 1.5 ], [ 1.045454545454545, 2.0 ], [ 1.0, 2.0 ], [ 0.5, 2.0 ], [ 0.0, 2.0 ], [ 0.0, 1.5 ], [ 0.0, 1.214285714285714 ], [ 0.5, 1.214285714285714 ] ] ] ] } },
            { "type": "Feature", "properties": { "ID": 1, "min": 10.0, "max": 20.0 }, "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 1.5, 1.261904761904762 ], [ 2.0, 1.261904761904762 ], [ 2.0, 1.5 ], [ 2.0, 2.0 ], [ 1.5, 2.0 ], [ 1.045454545454545, 2.0 ], [ 1.045454545454545, 1.5 ], [ 0.5, 1.214285714285714 ], [ 0.0, 1.214285714285714 ], [ 0.0, 1.0 ], [ 0.0, 0.738095238095238 ], [ 0.5, 0.738095238095238 ], [ 1.5, 1.261904761904762 ] ] ] ] } },
            { "type": "Feature", "properties": { "ID": 2, "min": 20.0, "max": 30.0 }, "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 0.954545454545455, 0.0 ], [ 0.954545454545455, 0.5 ], [ 1.5, 0.785714285714286 ], [ 2.0, 0.785714285714286 ], [ 2.0, 1.0 ], [ 2.0, 1.261904761904762 ], [ 1.5, 1.261904761904762 ], [ 0.5, 0.738095238095238 ], [ 0.0, 0.738095238095238 ], [ 0.0, 0.5 ], [ 0.0, 0.0 ], [ 0.5, 0.0 ], [ 0.954545454545455, 0.0 ] ] ] ] } },
            { "type": "Feature", "properties": { "ID": 3, "min": 30.0, "max": 36.0 }, "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 1.499999909090926, 0.0 ], [ 1.0, 0.0 ], [ 0.954545454545455, 0.0 ], [ 0.954545454545455, 0.5 ], [ 1.5, 0.785714285714286 ], [ 2.0, 0.785714285714286 ], [ 2.0, 0.500000047619043 ], [ 1.5, 0.500000047619043 ], [ 1.499999909090926, 0.5 ], [ 1.499999909090926, 0.0 ] ] ] ] } }
            ]
        }

.. example::

    :title: Creating contours from a DEM with fixed levels

    .. code-block:: bash

        $ cat test.asc
        ncols        2
        nrows        2
        xllcorner    0
        yllcorner    0
        cellsize     1
        4 15
        25 36

        $ gdal_contour test.asc -f GeoJSON /vsistdout/ -fl 10 20 -p -amin min -amax max

    This would create a single polygonal contour between 10 and 20 meters from the DEM data in :file:`test.asc`
    and produce a GeoJSON output with the contour min and max elevations in the ``min`` and ``max`` attributes.


    If the minimum and maximum values from the raster are desired, the special values `MIN`` and `MAX``
    (case insensitive) can be used:

    .. code-block:: bash

        $ cat test.asc
        ncols        2
        nrows        2
        xllcorner    0
        yllcorner    0
        cellsize     1
        4 15
        25 36

        $ gdal_contour test.asc -f GeoJSON /vsistdout/ -fl MIN 10 20 MAX -p -amin min -amax max

    This would create three polygonal contours from the DEM data in :file:`test.asc` and produce a GeoJSON output
    with the contour min and max elevations in the ``min`` and ``max`` attributes, the values of these fields will
    be: (4.0, 10.0), (10, 20.0) and (20.0, 36.0).

.. example::

    :title: Creating contours from a DEM specifying an interval and fixed levels at the same time

    .. code-block:: bash

        $ cat test.asc
        ncols        2
        nrows        2
        xllcorner    0
        yllcorner    0
        cellsize     1
        4 15
        25 36

        $ gdal_contour test.asc -f GeoJSON /vsistdout/ -i 10 -fl 15 -p -amin min -amax max

    Creates contours at regular 10 meter intervals and adds extra contour for a fixed 15 m level.
    Finally turns areas between the contours into polygons  with the contour min and max elevations
    in the ``min`` and ``max`` attributes, the values of these fields will be:
    (4.0, 10.0), (10, 15.0), (15, 20.0), (20.0, 30.0)  and (30.0, 36.0).


