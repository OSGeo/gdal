.. _gdal_raster_contour:

================================================================================
``gdal raster contour``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Builds vector contour lines from a raster elevation model.

.. Index:: gdal raster contour

Synopsis
--------

.. program-output:: gdal raster contour --help-doc

Description
-----------

:program:`gdal raster contour` creates a vector contour from a raster elevation model (DEM).

Since GDAL 3.12, this algorithm can be part of a :ref:`gdal_pipeline`.

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. versionadded:: 3.12

.. include:: gdal_cli_include/gdalg_vector_compatible_non_natively_streamable.rst

Program-Specific Options
------------------------

.. option:: --3d

    Forces the production of 3D vectors instead of 2D. Includes elevation at every vertex.

.. option:: -b, --band <BAND>

    Picks a particular band to get the DEM from. Defaults to band 1.

.. option:: --elevation-name <ELEVATION-NAME>

    Provides a name for the attribute in which to put the elevation. If not provided no elevation attribute is attached. Ignored in polygonal contouring (-p) mode.

.. option:: -e, --exp-base <EXP-BASE>

    Generate levels on an exponential scale: base ^ k, for k an integer. Mutually exclusive with :option:`--interval`, :option:`--levels`.

.. option:: --group-transactions <GROUP-TRANSACTIONS>

    Group n features per transaction (default 100 000).

.. option:: --interval <INTERVAL>

    Elevation interval between contours. Mutually exclusive with :option:`--levels`, :option:`--exp-base`.
    The first contour will be generated at the first multiple of ``INTERVAL`` which is greater than the raster minimum value.


.. option:: --levels <LEVELS>

    List of contour levels. `MIN` and `MAX` are special values that represent the minimum and maximum values in the raster.
    When `--polygonize` is used, the specified values are used as bounds of the generated polygons.
    Mutually exclusive with :option:`--interval`, :option:`--exp-base`.

.. option:: --max-name <MAX-NAME>

    Provides a name for the attribute in which to put the maximum elevation. If not provided no maximum elevation attribute is attached. Ignored in polygonal contouring (-p) mode.

.. option:: --min-name <MIN-NAME>

    Provides a name for the attribute in which to put the minimum elevation. If not provided no minimum elevation attribute is attached. Ignored in polygonal contouring (-p) mode.

.. option:: --off, --offset <OFFSET>

    Offset from zero relative to which to interpret intervals.

.. option:: --nln, --output-layer <OUTPUT-LAYER>

    Provides a name for the output vector layer. Defaults to "contour".

.. option:: -p, --polygonize

    Create polygons instead of lines.

.. option:: --src-nodata <SRCNODATA>

    Input pixel value to treat as 'nodata'.

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/co.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/lco.rst

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/output_oo.rst

    .. include:: gdal_options/of_vector.rst

    .. include:: gdal_options/overwrite.rst

Examples
--------

.. example::
   :title: Create contour lines from a raster elevation model

    The following example creates contour lines from a raster elevation model:

    .. code-block:: bash

        gdal raster contour --interval 100 elevation.tif contour.shp

    This will create a shapefile named ``contour.shp`` with contour lines at 100 meter intervals.

.. example::
    :title: Create contour polygons from a raster elevation model with custom attributes and fixed levels

     The following example creates contour polygons from a raster elevation model with custom attributes and fixed levels:

     .. code-block:: bash

          gdal raster contour --levels MIN,100,200,MAX --polygonize --min-name MIN --max-name MAX elevation.tif contour.shp

     This will create a shapefile named ``contour.shp`` with contour polygons from the minimum raster value to 100,
     and from 100 to 200 and from 200 to the maximum value, and with attributes ``MIN``, and ``MAX``.


