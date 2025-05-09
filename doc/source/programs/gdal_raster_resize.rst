.. _gdal_raster_resize:

================================================================================
``gdal raster resize``
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

This subcommand is also available as a potential step of :ref:`gdal_raster_pipeline`

Standard options
++++++++++++++++

.. include:: gdal_options/of_raster_create_copy.rst

.. include:: gdal_options/co.rst

.. include:: gdal_options/overwrite.rst

.. option:: --size <width[%]>,<height[%]>

    Set output raster width and height, expressed in pixels,
    or percentage if using the ``%`` suffix.
    If the width or the height is set to 0, the other dimension will be guessed
    from the computed resolution.

.. option:: -r, --resampling <RESAMPLING>

    Resampling method to use. Available methods are:

    ``near``: nearest neighbour resampling (default, fastest algorithm, worst interpolation quality).

    ``bilinear``: bilinear resampling.

    ``cubic``: cubic resampling.

    ``cubicspline``: cubic spline resampling.

    ``lanczos``: Lanczos windowed sinc resampling.

    ``average``: average resampling, computes the weighted average of all non-NODATA contributing pixels.


.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_raster_compatible.rst

Examples
--------

.. example::
   :title: Resize a dataset to 1000 columns and 500 lines using cubic resampling

   .. code-block:: bash

        $ gdal raster resize --size=1000,500 -r cubic in.tif out.tif --overwrite

.. example::
   :title: Resize a dataset to half size using cubic resampling

   .. code-block:: bash

        $ gdal raster resize --size=50%,50% -r cubic in.tif out.tif --overwrite
