.. _gdal_raster_proximity:

================================================================================
``gdal raster proximity``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Produces a raster proximity map.

.. Index:: gdal raster proximity

Synopsis
--------

.. program-output:: gdal raster proximity --help-doc

Description
-----------

:program:`gdal raster proximity` generates a raster proximity map indicating the Cartesian distance from
the center of each pixel to the center of the nearest pixel identified as a target pixel.
Target pixels are those in the source raster for which the raster pixel value is in the set of
target pixel values.

This subcommand is also available as a potential step of :ref:`gdal_raster_pipeline`

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. versionadded:: 3.12

.. include:: gdal_cli_include/gdalg_raster_compatible_non_natively_streamable.rst

Program-Specific Options
------------------------

.. option:: -b, --band <BAND>

    Input band (1-based index)

.. option:: --distance-units <pixel|geo>

    Units for the distance. If `geo` is specified, the distance will be calculated in
    georeferenced units (meters or degrees) using the pixel size of the input raster.
    Otherwise, the distance is interpreted in pixels.

.. option:: --fixed-value <FIXED-VALUE>

    Define a fixed value to be written to output pixels that are within :option:`--max-distance`
    from the target pixels, instead of the actual distance.

.. option:: --max-distance <MAX-DISTANCE>

    Maximum distance to search for a target pixel. The NoData value will be output if no target pixel is found within this distance.
    Distance is interpreted in pixels unless `--distance-units geo` is specified.

.. option:: --nodata <NODATA>

    Nodata value for the output raster. If not specified, the NoData value of the input band will be used.
    If the output band does not have a NoData value, then the value 65535 will be used for floating point
    output types and the maximum value that can be stored will be used for the integer output types.


.. option:: --target-values <TARGET-VALUES>

    A single value or a comma separated list of target pixel values in the source image
    to be considered target pixels.
    If not specified, all non-zero pixels will be considered target pixels.

Standard Options
----------------

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

   :title: Proximity map of a raster with max distance of 3 pixels

    .. code-block:: bash

        $ gdal raster proximity --max-distance 3  input.tif output.tif
