.. _gdal_raster_scale:

================================================================================
``gdal raster scale``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Scale the values of the bands of a raster dataset

.. Index:: gdal raster scale

Synopsis
--------

.. program-output:: gdal raster scale --help-doc

Description
-----------

:program:`gdal raster scale` can be used to rescale the input pixels values
from the range :option:`--src-min` to :option:`--src-max` to the range
:option:`--dst-min` to :option:`--dst-max`.
It is also often necessary to reset the output datatype with the :option:`--ot` switch.
If omitted the output range is from the minimum value to the maximum value allowed
for integer data types (for example from 0 to 255 for Byte output) or from 0 to 1
for floating-point data types.
If omitted the input range is automatically computed from the source dataset.
This may be a slow operation on a large source dataset, and if using it multiple times
for several gdal_translate invocation, it might be beneficial to call
``gdal raster edit --stats {source_dataset}`` priorly to precompute statistics, for
formats that support serializing statistics computations (GeoTIFF, VRT...)
Source values are clipped to the range defined by ``srcmin`` and ``srcmax``,
unless :option:`--no-clip` is set.

By default, the scaling is applied to all bands. It is possible to restrict
it to a single band with :option:`--band` and leave values of other bands unmodified.

This command is the reverse operation of :ref:`gdal_raster_unscale`.

This subcommand is also available as a potential step of :ref:`gdal_raster_pipeline`

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_raster_compatible.rst

Program-Specific Options
------------------------

.. option:: -b, --band <BAND>

    Index (starting at 1) of the band to which the scaling must be only applied.

.. option:: --dst-max <DSTMAX>

    Maximum value of the output range. This option must be used together with :option:`--dst-min`.

.. option:: --dst-min <DSTMIN>

    Minimum value of the output range. This option must be used together with :option:`--dst-max`.

.. option:: --exponent <EXPONENT>

    Apply non-linear scaling with a power function. ``exp_val`` is the exponent
    of the power function (must be positive). This option must be used with the
    :option:`--src-min` / :option:`--src-max` / :option:`--dst-min` / :option:`--dst-max` options.

    The scaled value ``Dst`` is calculated from the source value ``Src`` with the following
    formula:

    .. math::
        {Dst} = \left( {Dst}_{max} - {Dst}_{min} \right) \times \operatorname{max} \left( 0, \operatorname{min} \left( 1, \left( \frac{{Src} - {Src}_{min}}{{Src}_{max}-{Src}_{min}} \right)^{exp\_val} \right) \right) + {Dst}_{min}

    .. note::

        :ref:`gdal_raster_unscale` assumes linear scaling, and
        this cannot unscale values back to the original ones.

.. option:: --no-clip

    Disable clipping input values to the source range. Note that using this option
    with non-linear scaling with a non-integer exponent will cause input values lower
    than the minimum value of the source range to be mapped to not-a-number.

.. option:: --src-max <SRCMAX>

    Maximum value of the source range. If not specified, it will be calculated from the source dataset.
    This option must be used together with :option:`--src-min`.

.. option:: --src-min <SRCMIN>

    Minimum value of the source range. If not specified, it will be calculated from the input dataset.
    This option must be used together with :option:`--src-max`.

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/append_raster.rst

    .. include:: gdal_options/co.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/of_raster_create_copy.rst

    .. include:: gdal_options/ot.rst

    .. include:: gdal_options/overwrite.rst


Examples
--------

.. example::
   :title: Rescale linearly values of a UInt16 dataset from [0,4095] to a Byte dataset [0,255]

   .. code-block:: bash

        $ gdal raster scale --datatype Byte --src-min 0 --src-max 4095 uint16.tif byte.tif --overwrite
