.. _gdal_raster_overview_add_subcommand:

================================================================================
"gdal raster overview add" sub-command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Add overviews to a raster dataset

.. Index:: gdal raster overview add

Synopsis
--------

.. code-block::

    Usage: gdal raster overview add [OPTIONS] <DATASET>

    Adding overviews.

    Positional arguments:
      --dataset <DATASET>              Dataset (in-place updated, unless --external) [required]

    Common Options:
      -h, --help                       Display help message and exit
      --version                        Display GDAL version and exit
      --json-usage                     Display usage as JSON document and exit
      --drivers                        Display driver list as JSON document and exit
      --config <KEY>=<VALUE>           Configuration option [may be repeated]
      --progress                       Display progress bar

    Options:
      --external                       Add external overviews
      -r, --resampling <RESAMPLING>    Resampling method. RESAMPLING=nearest|average|cubic|cubicspline|lanczos|bilinear|gauss|average_magphase|rms|mode
      --levels <LEVELS>                Levels / decimation factors [may be repeated]
      --min-size <MIN-SIZE>            Maximum width or height of the smallest overview level.

    Advanced Options:
      --oo, --open-option <KEY=VALUE>  Open options [may be repeated]

Description
-----------

:program:`gdal raster overview add` can be used to build or rebuild overview images for
most supported file formats with one of several downsampling algorithms.

.. option:: --dataset <DATASET>

    Dataset name, to be in-place updated by default (unless :option:`--external` is specified). Required.

.. option:: --external

    Create external ``.ovr`` overviews as GeoTIFF files.

.. option:: --resampling {nearest|average|cubic|cubicspline|lanczos|bilinear|gauss|average_magphase|rms|mode}

    Select a resampling algorithm. The default is ``nearest``, which is generally not
    appropriate if sub-pixel accuracy is desired.

    When refreshing existing TIFF overviews, the previously
    used method, as noted in the RESAMPLING metadata item of the overview, will
    be used if :option:`-r` is not specified.

    The available methods are:

    ``nearest`` applies a nearest neighbour (simple sampling) resampler.

    ``average`` computes the average of all non-NODATA contributing pixels. This is a weighted average taking into account properly the weight of source pixels not contributing fully to the target pixel.

    ``bilinear`` applies a bilinear convolution kernel.

    ``cubic`` applies a cubic convolution kernel.

    ``cubicspline`` applies a B-Spline convolution kernel.

    ``lanczos`` applies a Lanczos windowed sinc convolution kernel.

    ``gauss`` applies a Gaussian kernel before computing the overview,
    which can lead to better results than simple averaging in e.g case of sharp edges
    with high contrast or noisy patterns. The advised level values should be 2, 4, 8, ...
    so that a 3x3 resampling Gaussian kernel is selected.

    ``average_magphase`` averages complex data in mag/phase space.

    ``rms`` computes the root mean squared / quadratic mean of all non-NODATA contributing pixels

    ``mode`` selects the value which appears most often of all the sampled points.

.. option:: --levels <level1,level2,...>

    A list of overview levels to build. Each overview level must be an integer
    value greater or equal to 2.

    When explicit levels are not specified,

    -  If there are already existing overviews, the corresponding levels will be
       used to refresh them if no explicit levels are specified.

    - Otherwise, appropriate overview power-of-two factors will be selected
      until the smallest overview is smaller than the value of the
      :option:`--min-size` switch.

.. option:: --min-size <val>

    Maximum width or height of the smallest overview level. Only taken into
    account if explicit levels are not specified. Defaults to 256.


Examples
--------

.. example::
   :title: Create overviews, embedded in the supplied TIFF file, with automatic computation of levels

   .. code-block:: bash

       gdal raster overview add -r average abc.tif

.. example::
   :title: Create overviews, embedded in the supplied TIFF file

   .. code-block:: bash

       gdal raster overview add -r average --levels=2,4,8,16 abc.tif

.. example::
   :title: Create an external compressed GeoTIFF overview file from the ERDAS .IMG file

   .. code-block:: bash

       gdal raster overview add --external --levels=2,4,8,16 --config COMPRESS_OVERVIEW=DEFLATE erdas.img

.. example::
   :title: Create an external JPEG-compressed GeoTIFF overview file from a 3-band RGB dataset

   If the dataset is a writable GeoTIFF, you also need to add the :option:`--external` option to
   force the generation of external overview.

   .. code-block:: bash

       gdal raster overview add --config COMPRESS_OVERVIEW=JPEG --config PHOTOMETRIC_OVERVIEW=YCBCR
                --config INTERLEAVE_OVERVIEW=PIXEL rgb_dataset.ext 2 4 8 16

.. example::
   :title: Create overviews for a specific subdataset

   For example, one of potentially many raster layers in a GeoPackage (the "filename" parameter must be driver prefix, filename and subdataset name, like e.g. shown by gdalinfo):

   .. code-block:: bash

       gdal raster overview add GPKG:file.gpkg:layer
