.. _gdal_raster_set_type:

================================================================================
``gdal raster set-type``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Modify the data type of bands of a raster dataset.

.. Index:: gdal raster set_type

Synopsis
--------

.. program-output:: gdal raster set-type --help-doc

Description
-----------

:program:`gdal raster set-type` can be used to force the output image bands to
have a specific data type. Values may be truncated or rounded if the output
data type is "smaller" than the input one.

This subcommand is also available as a potential step of :ref:`gdal_raster_pipeline`

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_raster_compatible.rst


Program-Specific Options
------------------------

.. include:: gdal_options/ot.rst

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/co.rst

    .. include:: gdal_options/of_raster_create_copy.rst

    .. include:: gdal_options/overwrite.rst


Examples
--------

.. example::
   :title: Convert to Float32 data type

   .. code-block:: bash

        $ gdal raster set-type --datatype Float32 byte.tif float32.tif --overwrite
