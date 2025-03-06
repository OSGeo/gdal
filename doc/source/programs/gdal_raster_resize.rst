.. _gdal_raster_resize_subcommand:

================================================================================
"gdal raster resize" sub-command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Resize a raster dataset without changing the georeferenced extents.

.. Index:: gdal raster resize

Synopsis
--------

.. code-block::

    Usage: gdal raster resize [OPTIONS] <INPUT> <OUTPUT>

    Resize a raster dataset.

    Positional arguments:
      -i, --input <INPUT>                                  Input raster dataset [required]
      -o, --output <OUTPUT>                                Output raster dataset [required]

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
      --overwrite                                          Whether overwriting existing output is allowed
      --size <width>,<height>                              Target size in pixels [required]
      -r, --resampling <RESAMPLING>                        Resampling method. RESAMPLING=nearest|bilinear|cubic|cubicspline|lanczos|average|mode (default: nearest)


    Advanced Options:
      --if, --input-format <INPUT-FORMAT>                  Input formats [may be repeated]
      --oo, --open-option <KEY=VALUE>                      Open options [may be repeated]


Description
-----------

:program:`gdal raster resize` can be used to resize a raster dataset without
changing the georeferenced extents.

Standard options
++++++++++++++++

.. include:: gdal_options/of_raster_create_copy.rst

.. include:: gdal_options/co.rst

.. include:: gdal_options/overwrite.rst

.. option:: --size <width>,<height>

    Set output file size in pixels and lines. If width or height is set to 0,
    the other dimension will be guessed from the computed resolution.

.. option:: -r, --resampling <RESAMPLING>

    Resampling method to use. Available methods are:

    ``near``: nearest neighbour resampling (default, fastest algorithm, worst interpolation quality).

    ``bilinear``: bilinear resampling.

    ``cubic``: cubic resampling.

    ``cubicspline``: cubic spline resampling.

    ``lanczos``: Lanczos windowed sinc resampling.

    ``average``: average resampling, computes the weighted average of all non-NODATA contributing pixels.

Examples
--------

.. example::
   :title: Resize a dataset to 1000 columns and 500 lines using cubic resampling

   .. code-block:: bash

        $ gdal raster resize --resize=1000,500 -r cubic in.tif out.tif --overwrite
