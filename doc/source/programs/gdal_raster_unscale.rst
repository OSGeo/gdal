.. _gdal_raster_unscale_subcommand:

================================================================================
"gdal raster unscale" sub-command
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

This command is the reverse operation of :ref:`gdal_raster_scale_subcommand`.

Standard options
++++++++++++++++

.. include:: gdal_options/of_raster_create_copy.rst

.. include:: gdal_options/co.rst

.. include:: gdal_options/overwrite.rst

.. option:: --ot, --datatype, --output-data-type <OUTPUT-DATA-TYPE>

  Override output data type among ``Byte``, ``Int8``, ``UInt16``, ``Int16``, ``UInt32``,
  ``Int32``, ``UInt64``, ``Int64``, ``CInt16``, ``CInt32``, ``Float32``,
  ``Float64``, ``CFloat32``, ``CFloat64``.

Examples
--------

.. example::
   :title: Unscale a scaled raster to a Float32 one

   .. code-block:: bash

        $ gdal raster unscale scaled_byte.tif unscaled_float32.tif --overwrite
