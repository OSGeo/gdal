.. _gdal_pansharpen:

================================================================================
gdal_pansharpen
================================================================================

.. only:: html

    Perform a pansharpen operation.

    (Since GDAL 2.1)

.. Index:: gdal_pansharpen

Synopsis
--------

.. code-block::

    gdal_pansharpen [--help] [--help-general]
                    <pan_dataset>
                    <spectral_dataset>[,band=<num>] [<spectral_dataset>[,band=<num>]]...
                    <out_dataset>
                    [-of <format>] [-b <band>]... [-w <weight_val>]...
                    [-r {nearest|bilinear|cubic|cubicspline|lanczos|average}]
                    [-threads {ALL_CPUS|<number>}] [-bitdepth <val>] [-nodata <val>]
                    [-spat_adjust {union|intersection|none|nonewithoutwarning}]
                    [-co <NAME>=<VALUE>]... [-q]

Description
-----------

:program:`gdal_pansharpen` performs a pan-sharpening operation. It
can create a "classic" output dataset (such as GeoTIFF), or a VRT
dataset describing the pan-sharpening operation.

More details can be found in the :ref:`gdal_vrttut_pansharpen` section.

.. note::

    gdal_pansharpen is a Python utility, and is only available if GDAL Python bindings are available.

.. include:: options/help_and_help_general.rst

.. option:: -of <format>

    Select the output format. Starting with GDAL 2.3, if not specified,
    the format is guessed from the extension (previously was ``GTiff``). Use
    the short format name. ``VRT`` can also be used.

.. option:: -b <band>

    Select band *band* from the input spectral bands for output. Bands
    are numbered from 1 in the order spectral bands are specified.
    Multiple **-b** switches may be used. When no -b switch is used, all
    input spectral bands are set for output.

.. option:: -w <weight_val>

    Specify a weight for the computation of the pseudo panchromatic
    value. There must be as many -w switches as input spectral bands.

.. option:: -r {nearest|bilinear|cubic|cubicspline|lanczos|average}

    Select a resampling algorithm. ``cubic`` is the default.

.. option:: -threads {ALL_CPUS|<number>}

    Specify number of threads to use to do the resampling and
    pan-sharpening itself. Can be an integer number or ``ALL_CPUS``.

.. option:: -bitdepth <val>

    Specify the bit depth of the panchromatic and spectral bands (e.g.
    12). If not specified, the NBITS metadata item from the panchromatic
    band will be used if it exists.

.. option:: -nodata <val>

    Specify nodata value for bands. Used for the resampling and
    pan-sharpening computation itself. If not set, deduced from the
    input bands, provided they have a consistent setting.

.. option:: -spat_adjust {union|intersection|none|nonewithoutwarning}

    Select behavior when bands have not the same extent. See
    *SpatialExtentAdjustment* documentation in :ref:`gdal_vrttut_pansharpen`
    ``union`` is the default.

.. include:: options/co.rst

.. option:: -q

    Suppress progress monitor and other non-error output.

.. option:: <pan_dataset>

    Dataset with panchromatic band (first band will be used).

.. option:: <spectral_dataset>[,band=<num>]

    Dataset with one or several spectral bands. If the band option is
    not specified, all bands of the datasets are taken into account.
    Otherwise, only the specified (num)th band. The same dataset can be
    repeated several times.

.. option:: <out_dataset>

    Output dataset

Bands should be in the same projection.

Example
-------

With spectral bands in a single dataset :

.. code-block::

    gdal_pansharpen panchro.tif multispectral.tif pansharpened_out.tif

With a few spectral bands from a single dataset, reordered :

.. code-block::

    gdal_pansharpen panchro.tif multispectral.tif,band=3 multispectral.tif,band=2 multispectral.tif,band=1 pansharpened_out.tif

With spectral bands in several datasets :

.. code-block::

    gdal_pansharpen panchro.tif band1.tif band2.tif band3.tif pansharpened_out.tif

Specify weights:

.. code-block::

    gdal_pansharpen -w 0.7 -w 0.2 -w 0.1 panchro.tif multispectral.tif pansharpened_out.tif

Specify RGB bands from a RGBNir multispectral dataset while computing
the pseudo panchromatic intensity on the 4 RGBNir bands:

.. code-block::

    gdal_pansharpen -b 1 -b 2 -b 3 panchro.tif rgbnir.tif pansharpened_out.tif
