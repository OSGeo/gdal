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

    gdal_contour [-b <band>] [-a <attribute_name>] [-amin <attribute_name>] [-amax <attribute_name>]
                 [-3d] [-inodata]
                 [-snodata n] [-i <interval>]
                 [-f <formatname>] [[-dsco NAME=VALUE] ...] [[-lco NAME=VALUE] ...]
                 [-off <offset>] [-fl <level> <level>...] [-e <exp_base>]
                 [-nln <outlayername>] [-q] [-p]
                 <src_filename> <dst_filename>

Description
-----------

The :program:`gdal_contour` generates a vector contour file from the input
raster elevation model (DEM).

The contour line-strings are oriented consistently and the high side will
be on the right, i.e. a line string goes clockwise around a top.

.. program:: gdal_contour

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

.. option:: -dsco <NAME=VALUE>

    Dataset creation option (format specific)

.. option:: -lco <NAME=VALUE>

    Layer creation option (format specific)

.. option:: -i <interval>

    Elevation interval between contours.

.. option:: -off <offset>

    Offset from zero relative to which to interpret intervals.

.. option:: -fl <level>

    Name one or more "fixed levels" to extract.

.. option:: -e <base>

    Generate levels on an exponential scale: `base ^ k`, for `k` an integer.

    .. versionadded:: 2.4.0

.. option:: -nln <name>

    Provide a name for the output vector layer. Defaults to "contour".

.. option:: -p

    Generate contour polygons rather than contour lines.

    .. versionadded:: 2.4.0

.. option:: -q

    Be quiet.

C API
-----

Functionality of this utility can be done from C with :cpp:func:`GDALContourGenerate`.

Example
-------

This would create 10-meter contours from the DEM data in :file:`dem.tif` and
produce a shapefile in :file:`contour.shp|shx|dbf` with the contour elevations
in the ``elev`` attribute.

.. code-block::

    gdal_contour -a elev dem.tif contour.shp -i 10.0
