.. _gdal_raster_select:

================================================================================
``gdal raster select``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Select a subset of bands from a raster dataset.

.. Index:: gdal raster select

Synopsis
--------

.. program-output:: gdal raster select --help-doc

Description
-----------

:program:`gdal raster select` can be used to select and re-order a subset of
raster bands from a raster dataset.

This subcommand is also available as a potential step of :ref:`gdal_raster_pipeline`

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_raster_compatible.rst

Program-Specific Options
------------------------

.. option:: --band <BAND>

    Select one or several input bands. Bands are numbered from 1.
    Multiple values may be selected to select a set of input bands
    to write to the output file, or to reorder bands. The special value ``mask``
    means the mask band of the first band of the input dataset. The mask band
    of any band can be specified with ``mask:<band>``. A mask band selected
    with ``--band`` will become an ordinary band in the output dataset.

.. option:: --mask <BAND>

    Select one input band to create output dataset mask band.. Bands are numbered from 1.
    The special value ``mask`` means the mask band of the first band of the input dataset. The mask band
    of any band can be specified with ``mask:<band>``.
    <BAND> can be set to ``none`` to avoid copying the global
    mask of the input dataset if it exists. Otherwise it is copied by default,
    unless the mask is an alpha channel, or if it is explicitly selected
    to be a regular band of the output dataset (``--band mask``)

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
   :title: Reorder a 3-band dataset with bands ordered Blue, Green, Red to Red, Green, Blue

   .. code-block:: bash

        $ gdal raster select --band 3,2,1 bgr.tif rgb.tif --overwrite

.. example::
   :title: Convert a RGBA dataset to a YCbCR JPEG compressed GeoTIFF

   .. code-block:: bash

        $ gdal raster select --band 1,2,3 --mask 4 --co COMPRESS=JPEG,PHOTOMETRIC=YCBCR rgba.tif rgb_mask.tif
