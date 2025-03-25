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

.. program-output:: gdal raster resize --help-doc

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
