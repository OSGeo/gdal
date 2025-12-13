.. _gdal_raster_pansharpen:

================================================================================
``gdal raster pansharpen``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Perform a pansharpen operation.

.. Index:: gdal raster pansharpen

Synopsis
--------

.. program-output:: gdal raster pansharpen --help-doc

Description
-----------

:program:`gdal raster pansharpen` performs a pan-sharpening operation. It
can create a "classic" output dataset (such as GeoTIFF), or a VRT
dataset describing the pan-sharpening operation.

It takes as input a one-band panchromatic dataset and several spectral bands,
generally at a lower resolution than the panchromatic band. The output is
a multi-band dataset (with as many bands as input panchromatic bands), enhanced
at the resolution of the panchromatic band.

All bands should be in the same coordinate reference system.

More details can be found in the :ref:`gdal_vrttut_pansharpen` section.

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_raster_compatible.rst

Program-Specific Options
------------------------

.. option:: --bit-depth <val>

    Specify the bit depth of the panchromatic and spectral bands (e.g.
    12). If not specified, the NBITS metadata item from the panchromatic
    band will be used if it exists.

.. option:: -i, --panchromatic, --input <INPUT>

    Dataset with panchromatic band. [required]

.. option:: -j, --num-threads <value>

    Specify number of threads to use to do the resampling and
    pan-sharpening itself. Can be an integer number or ``ALL_CPUS`` (the default)

.. option:: --nodata <val>

    Specify nodata value for bands. Used for the resampling and
    pan-sharpening computation itself. If not set, deduced from the
    input bands, provided they have a consistent setting.

.. option:: -r, --resampling nearest|bilinear|cubic|cubicspline|lanczos|average

    Select a resampling algorithm. ``cubic`` is the default.

.. option:: --spatial-extent-adjustment union|intersection|none|none-without-warning

    Select behavior when bands have not the same extent. See
    *SpatialExtentAdjustment* documentation in :ref:`gdal_vrttut_pansharpen`
    ``union`` is the default.
.. option:: --spectral <spectral_dataset>[,band=<num>]

    Dataset with one or several spectral bands. [required]

    If the band option is not specified, all bands of the dataset are taken
    into account.
    Otherwise, only the specified (num)th band. The same dataset can be
    repeated several times.

.. option::  --weights <WEIGHTS>

    Specify a weight for the computation of the pseudo panchromatic
    value. There must be as many values as input spectral bands.

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
   :title: With spectral bands in a single dataset

   .. code-block::

       gdal raster pansharpen panchro.tif rgb.tif pansharpened_out.tif

.. example::
   :title: With a few spectral bands from a single dataset, reordered

   .. code-block::

       gdal raster pansharpen panchro.tif bgr.tif,band=3 bgr.tif,band=2 bgr.tif,band=1 pansharpened_out.tif

.. example::
   :title: With spectral bands in several datasets

   .. code-block::

       gdal raster pansharpen panchro.tif red.tif green.tif blue.tif pansharpened_out.tif

.. example::
   :title: Specifying weights

   .. code-block::

       gdal raster pansharpen -w 0.7,0.2,0.1 panchro.tif multispectral.tif pansharpened_out.tif

.. example::
   :title: Select RGB bands from a RGBNir multispectral dataset while computing the pseudo panchromatic intensity on the 4 RGBNir bands

   .. code-block::

       gdal raster pipeline read panchro.tif ! pansharpen rgbnir.tif ! select 1,2,3 ! write pansharpened_out.tif
