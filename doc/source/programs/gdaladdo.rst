.. _gdaladdo:

================================================================================
gdaladdo
================================================================================

.. only:: html

    Builds or rebuilds overview images.

.. Index:: gdaladdo

Synopsis
--------

.. code-block::

    gdaladdo [-r {nearest,average,rms,bilinear,gauss,cubic,cubicspline,lanczos,average_magphase,mode}]
            [-b band]* [-minsize val]
            [-ro] [-clean] [-oo NAME=VALUE]* [--help-general] filename [levels]

Description
-----------

The :program:`gdaladdo` utility can be used to build or rebuild overview images for
most supported file formats with one of several downsampling algorithms.

.. program:: gdaladdo

.. option:: -r {nearest (default),average,rms,gauss,cubic,cubicspline,lanczos,average_magphase,mode}

    Select a resampling algorithm.

    ``nearest`` applies a nearest neighbour (simple sampling) resampler

    ``average`` computes the average of all non-NODATA contributing pixels. Starting with GDAL 3.1, this is a weighted average taking into account properly the weight of source pixels not contributing fully to the target pixel.

    ``rms`` computes the root mean squared / quadratic mean of all non-NODATA contributing pixels (GDAL >= 3.3)

    ``bilinear`` applies a bilinear convolution kernel.

    ``gauss`` applies a Gaussian kernel before computing the overview,
    which can lead to better results than simple averaging in e.g case of sharp edges
    with high contrast or noisy patterns. The advised level values should be 2, 4, 8, ...
    so that a 3x3 resampling Gaussian kernel is selected.

    ``cubic`` applies a cubic convolution kernel.

    ``cubicspline`` applies a B-Spline convolution kernel.

    ``lanczos`` applies a Lanczos windowed sinc convolution kernel.

    ``average_magphase`` averages complex data in mag/phase space.

    ``mode`` selects the value which appears most often of all the sampled points.

.. option:: -b <band>

    Select an input band **band** for overview generation. Band numbering
    starts from 1. Multiple :option:`-b` switches may be used to select a set
    of input bands to generate overviews.

.. option:: -ro

    open the dataset in read-only mode, in order to generate external overview
    (for GeoTIFF especially).

.. option:: -clean

    remove all overviews.

.. option:: -oo NAME=VALUE

    Dataset open option (format specific)

.. option:: -minsize <val>

    Maximum width or height of the smallest overview level. Only taken into
    account if explicit levels are not specified. Defaults to 256.

    .. versionadded:: 2.3

.. option:: <filename>

    The file to build overviews for (or whose overviews must be removed).

.. option:: <levels>

    A list of integral overview levels to build. Ignored with :option:`-clean` option.

    .. versionadded:: 2.3

        levels are no longer required to build overviews.
        In which case, appropriate overview power-of-two factors will be selected
        until the smallest overview is smaller than the value of the -minsize switch.

gdaladdo will honour properly NODATA_VALUES tuples (special dataset metadata) so
that only a given RGB triplet (in case of a RGB image) will be considered as the
nodata value and not each value of the triplet independently per band.

Selecting a level value like ``2`` causes an overview level that is 1/2
the resolution (in each dimension) of the base layer to be computed.  If
the file has existing overview levels at a level selected, those levels will
be recomputed and rewritten in place.

For internal GeoTIFF overviews (or external overviews in GeoTIFF format), note
that -clean does not shrink the file. A later run of gdaladdo with overview levels
will cause the file to be expanded, rather than reusing the space of the previously
deleted overviews. If you just want to change the resampling method on a file that
already has overviews computed, you don't need to clean the existing overviews.

Some format drivers do not support overviews at all.  Many format drivers
store overviews in a secondary file with the extension .ovr that is actually
in TIFF format.  By default, the GeoTIFF driver stores overviews internally to the file
operated on (if it is writable), unless the -ro flag is specified.

Most drivers also support an alternate overview format using Erdas Imagine
format.  To trigger this use the :decl_configoption:`USE_RRD` =YES configuration option.  This will
place the overviews in an associated .aux file suitable for direct use with
Imagine or ArcGIS as well as GDAL applications.  (e.g. --config USE_RRD YES)

External overviews in GeoTIFF format
------------------------------------

External overviews created in TIFF format may be compressed using the :decl_configoption:`COMPRESS_OVERVIEW`
configuration option.  All compression methods, supported by the GeoTIFF
driver, are available here. (e.g. ``--config COMPRESS_OVERVIEW DEFLATE``).
The photometric interpretation can be set with the :decl_configoption:`PHOTOMETRIC_OVERVIEW`
=RGB/YCBCR/... configuration option,
and the interleaving with the :decl_configoption:`INTERLEAVE_OVERVIEW` =PIXEL/BAND configuration option.

For JPEG compressed external and internal overviews, the JPEG quality can be set with
``--config JPEG_QUALITY_OVERVIEW value``.

For WEBP compressed external and internal overviews, the WEBP quality level can be set with
``--config WEBP_LEVEL_OVERVIEW value``. If not set, will default to 75.

For LERC compressed external and internal overviews, the max error threshold can be set with
``--config MAX_Z_ERROR_OVERVIEW value``. If not set, will default to 0 (lossless). Added in GDAL 3.4.1

For DEFLATE or LERC_DEFLATE compressed external and internal overviews, the compression level can be set with
``--config ZLEVEL_OVERVIEW value``. If not set, will default to 6. Added in GDAL 3.4.1

For ZSTD or LERC_ZSTD compressed external and internal overviews, the compression level can be set with
``--config ZSTD_LEVEL_OVERVIEW value``. If not set, will default to 9. Added in GDAL 3.4.1

For LZW, ZSTD or DEFLATE compressed external overviews, the predictor value can be set
with ``--config PREDICTOR_OVERVIEW 1|2|3``.

To produce the smallest possible JPEG-In-TIFF overviews, you should use:

::

    --config COMPRESS_OVERVIEW JPEG --config PHOTOMETRIC_OVERVIEW YCBCR --config INTERLEAVE_OVERVIEW PIXEL

External overviews can be created in the BigTIFF format by using
the :decl_configoption:`BIGTIFF_OVERVIEW` configuration option:
``--config BIGTIFF_OVERVIEW {IF_NEEDED|IF_SAFER|YES|NO}``.

The default value is IF_SAFER starting with GDAL 2.3.0 (previously was IF_NEEDED).
The behavior of this option is exactly the same as the BIGTIFF creation option
documented in the GeoTIFF driver documentation.

- YES forces BigTIFF.
- NO forces classic TIFF.
- IF_NEEDED will only create a BigTIFF if it is clearly needed (uncompressed,
  and overviews larger than 4GB).
- IF_SAFER will create BigTIFF if the resulting file *might* exceed 4GB.

Sparse GeoTIFF overview files (that is tiles which are omitted if all their pixels are
at the nodata value, when there's one, or at 0 otherwise) can be obtained with
``--config SPARSE_OK_OVERVIEW ON``. Added in GDAL 3.4.1

See the documentation of the :ref:`raster.gtiff` driver for further explanations on all those options.

Setting blocksize in Geotiff overviews
---------------------------------------

``--config GDAL_TIFF_OVR_BLOCKSIZE <size>``

Example: ``--config GDAL_TIFF_OVR_BLOCKSIZE 256``

Default value is 128, or starting with GDAL 3.1, if creating overviews on a tiled GeoTIFF file, the tile size of the full resolution image.
Note: without this setting, the file can have the full resolution image with a blocksize different from overviews blocksize.(e.g. full resolution image at blocksize 256, overviews at blocksize 128)


Multithreading
--------------

.. versionadded:: 3.2

The :decl_configoption:`GDAL_NUM_THREADS` configuration option can be set to
``ALL_CPUS`` or a integer value to specify the number of threads to use for
overview computation.

C API
-----

Functionality of this utility can be done from C with :cpp:func:`GDALBuildOverviews`.

Examples
--------

Create overviews, embedded in the supplied TIFF file, with automatic computation
of levels (GDAL 2.3 or later)

::

    gdaladdo -r average abc.tif

Create overviews, embedded in the supplied TIFF file:

::

    gdaladdo -r average abc.tif 2 4 8 16

Create an external compressed GeoTIFF overview file from the ERDAS .IMG file:

::

    gdaladdo -ro --config COMPRESS_OVERVIEW DEFLATE erdas.img 2 4 8 16

Create an external JPEG-compressed GeoTIFF overview file from a 3-band RGB dataset
(if the dataset is a writable GeoTIFF, you also need to add the -ro option to
force the generation of external overview):

::

    gdaladdo --config COMPRESS_OVERVIEW JPEG --config PHOTOMETRIC_OVERVIEW YCBCR
             --config INTERLEAVE_OVERVIEW PIXEL rgb_dataset.ext 2 4 8 16

Create an Erdas Imagine format overviews for the indicated JPEG file:

::

    gdaladdo --config USE_RRD YES airphoto.jpg 3 9 27 81

Create overviews for a specific subdataset, like for example one of potentially many raster layers in a GeoPackage (the "filename" parameter must be driver prefix, filename and subdataset name, like e.g. shown by gdalinfo):

::

    gdaladdo GPKG:file.gpkg:layer
