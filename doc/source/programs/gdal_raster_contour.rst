.. _gdal_raster_contour_subcommand:

================================================================================
"gdal raster contour" sub-command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Builds vector contour lines from a raster elevation model.

.. Index:: gdal raster contour

Synopsis
--------

.. code-block::

    Usage: gdal raster contour [OPTIONS] <INPUT> <OUTPUT>

    Creates a vector contour from a raster elevation model (DEM).

    Positional arguments:
    -i, --input <INPUT>                                  Input raster dataset [required]
    -o, --output <OUTPUT>                                Output raster dataset (created by algorithm) [required]

    Common Options:
    -h, --help                                           Display help message and exit
    --version                                            Display GDAL version and exit
    --json-usage                                         Display usage as JSON document and exit
    --drivers                                            Display driver list as JSON document and exit
    --config <KEY>=<VALUE>                               Configuration option [may be repeated]
    --progress                                           Display progress bar

    Options:
    -f, --of, --format, --output-format <OUTPUT-FORMAT>  Output format
    --co, --creation-option <KEY>=<VALUE>                Creation option [may be repeated]
    -b, --band <BAND>                                    Specify input band number (default: 1)
    -l, --nln, --layer <LAYER>                           Layer name
    --elevation-name <ELEVATION-NAME>                    Name of the elevation field
    --min-name <MIN-NAME>                                Name of the minimum elevation field
    --max-name <MAX-NAME>                                Name of the maximum elevation field
    --3d                                                 Force production of 3D vectors instead of 2D
    --srcnodata <SRCNODATA>                              Input pixel value to treat as 'nodata'
    --interval <INTERVAL>                                Elevation interval between contours
                                                        Mutually exclusive with --levels, --exp-base
    --levels <LEVELS>                                    List of contour levels [may be repeated]
                                                        Mutually exclusive with --interval, --exp-base
    -e, --exp-base <EXP-BASE>                            Base for exponential contour level generation
                                                        Mutually exclusive with --interval, --levels
    --off, --offset <OFFSET>                             Offset to apply to contour levels
    -p, --polygonize                                     Create polygons instead of lines
    --group-transactions <GROUP-TRANSACTIONS>            Group n features per transaction (default 100 000)
    --overwrite                                          Whether overwriting existing output is allowed

    Advanced Options:
    --oo, --open-option <KEY=VALUE>                      Open options [may be repeated]
    --if, --input-format <INPUT-FORMAT>                  Input formats [may be repeated]

Description
-----------

:program:`gdal raster contour` creates a vector contour from a raster elevation model (DEM).

The following options are available:

Standard options
++++++++++++++++


.. include:: gdal_options/of_vector.rst

.. include:: gdal_options/co.rst

.. include:: gdal_options/overwrite.rst

.. option:: -b, --band <BAND>

    Picks a particular band to get the DEM from. Defaults to band 1.

.. option:: -l, --nln, --layer <LAYER>

    Provides a name for the output vector layer. Defaults to "contour".

.. option:: -elevation-name <ELEVATION-NAME>

    Provides a name for the attribute in which to put the elevation. If not provided no elevation attribute is attached. Ignored in polygonal contouring (-p) mode.

.. option:: --min-name <MIN-NAME>

    Provides a name for the attribute in which to put the minimum elevation. If not provided no minimum elevation attribute is attached. Ignored in polygonal contouring (-p) mode.

.. option:: --max-name <MAX-NAME>

    Provides a name for the attribute in which to put the maximum elevation. If not provided no maximum elevation attribute is attached. Ignored in polygonal contouring (-p) mode.

.. option:: --3d

    Forces the production of 3D vectors instead of 2D. Includes elevation at every vertex.

.. option:: --srcnodata <SRCNODATA>

    Input pixel value to treat as 'nodata'.

.. option:: --interval <INTERVAL>

    Elevation interval between contours. Mutually exclusive with :option:`--levels`, :option:`--exp-base`.

.. option:: --levels <LEVELS>

    List of contour levels. `MIN` and `MAX` are special values that represent the minimum and maximum values in the raster.
    When `--polygonize` is used, the specified values are used as bounds of the generated polygons.
    Mutually exclusive with :option:`--interval`, :option:`--exp-base`.

.. option:: -e, --exp-base <EXP-BASE>

    Generate levels on an exponential scale: base ^ k, for k an integer. Mutually exclusive with :option:`--interval`, :option:`--levels`.

.. option:: --off, --offset <OFFSET>

    Offset from zero relative to which to interpret intervals.

.. option:: -p, --polygonize

    Create polygons instead of lines.

.. option:: --group-transactions <GROUP-TRANSACTIONS>

    Group n features per transaction (default 100 000).

Advanced options
++++++++++++++++

.. include:: gdal_options/oo.rst

.. include:: gdal_options/if.rst



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


