.. _gdal_raster_unscale:

================================================================================
``gdal raster unscale``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Convert scaled values of a raster dataset into unscaled values.

.. Index:: gdal raster unscale

Synopsis
--------

.. program-output:: gdal raster unscale --help-doc

Description
-----------

:program:`gdal raster unscale` applies the scale/offset metadata for the bands
to convert scaled values to unscaled values.

If the input band data type is Byte, Int8, UInt16, Int16, UInt32, Int32, Int64,
UInt64, Float16 or Float32, the default output data type will be Float32.
If the input band data type is Float64, it will be kept as the default output data type.
If the input band data type is CInt16, CFloat16 or CFloat32, the default output data type will be Float32.
If the input band data type is CFloat64, it will be kept as the default output data type.

The unscaled value is computed from the scaled raw value with the following
formula:

.. math::
    {unscaled\_value} = {scaled\_value} * {scale} + {offset}

If one of the input bands has no scale/offset metadata, its values are kept
unmodified.

This command is the reverse operation of :ref:`gdal_raster_scale`.

This subcommand is also available as a potential step of :ref:`gdal_raster_pipeline`

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_raster_compatible.rst


Standard Options
----------------

.. collapse:: details

    .. include:: gdal_options/append_raster.rst

    .. include:: gdal_options/co.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/ot.rst

    .. include:: gdal_options/of_raster_create_copy.rst

    .. include:: gdal_options/overwrite.rst

Examples
--------

.. example::
   :title: Unscale a scaled raster to a Float32 one

   .. code-block:: bash

        $ gdal raster unscale scaled_byte.tif unscaled_float32.tif --overwrite
