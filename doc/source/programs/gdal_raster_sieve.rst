.. _gdal_raster_sieve:

================================================================================
``gdal raster sieve``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Remove small raster polygons

.. Index:: gdal raster sieve

Synopsis
--------

.. program-output:: gdal raster sieve --help-doc

Description
-----------

:program:`gdal raster sieve` removes raster polygons smaller than a provided threshold size (in pixels)
and replaces them with the pixel value of the largest neighbour polygon.

The input dataset is read as integer data which means that floating point
values are rounded to integers. Re-scaling source data may be necessary in
some cases (e.g. 32-bit floating point data with min=0 and max=1).

This subcommand is also available as a potential step of :ref:`gdal_raster_pipeline`
(since GDAL 3.12)

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. versionadded:: 3.12

.. include:: gdal_cli_include/gdalg_raster_compatible_non_natively_streamable.rst


Program-Specific Options
------------------------

.. option:: -b, --band <BAND>

    Input band (1-based index)

.. option:: -c, --connect-diagonal-pixels

    Consider diagonal pixels (pixels at the corners) as connected.
    The default behavior is to only consider pixels that are touching the edges
    as connected, which is the same as 4-connectivity. When this option is
    selected, the algorithm will also consider pixels at the corners as connected,
    which is the same as 8-connectivity.

.. option:: --mask <MASK>

    Use the first band of the specified file as a validity mask:
    all pixels in the mask band with a value other than zero
    will be considered suitable for inclusion in polygons.

.. option:: -s, --size-threshold <SIZE-THRESHOLD>

    Minimum size of polygons to keep (default: 2)


Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/append_raster.rst

    .. include:: gdal_options/co.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/of_raster_create_copy.rst

    .. include:: gdal_options/overwrite.rst

Examples
--------

.. example::
   :title: Remove polygons smaller than 10 pixels from the band 2 of a raster.

   .. code-block:: bash

        $ gdal raster sieve -b 2 -s 10 input.tif output.tif
