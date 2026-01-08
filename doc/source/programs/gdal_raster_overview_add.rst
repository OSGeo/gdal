.. _gdal_raster_overview_add:

================================================================================
``gdal raster overview add``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Add overviews to a raster dataset

.. Index:: gdal raster overview add

Synopsis
--------

.. program-output:: gdal raster overview add --help-doc

Description
-----------

:program:`gdal raster overview add` can be used to build or rebuild overview images for
most supported file formats with one of several downsampling algorithms.

Starting with GDAL 3.12, :program:`gdal raster overview add` can be used as a
step of a pipeline. In that case virtual overviews are generated at the output
of the step, and can potentially be materialized in the final write step, when
writing a GeoTIFF file with the COPY_SRC_OVERVIEWS creation option, or when writing
to a COG (Cloud Optimized GeoTIFF) file.

Program-Specific Options
------------------------

.. option:: --co <NAME>=<VALUE>

    .. versionadded:: 3.12

    Overview creation options. May be repeated.

    Many formats have one or more optional creation options that can be
    used to control particulars about the created overviews. Options available
    can be obtained by looking at the ``OverviewCreationOptionList`` returned
    by ``gdal --format <FORMAT-NAME>``.

    Most formats will support external overviews in a GeoTIFF file in a
    side-car file of extension ``.ovr``. You can consult the
    :ref:`overview creation options for GeoTIFF <raster.gtiff-overview-creation-options>`.

.. option:: --dataset <DATASET>

    Dataset name, to be updated in-place by default (unless :option:`--external` is specified).
    Required for standalone execution, implicit when run as a pipeline step

.. option:: --external

    Create external ``.ovr`` overviews as GeoTIFF files.
    Not available when run as a pipeline step.

.. option:: --levels <level1,level2,...>

    A list of overview levels to build. Each overview level must be an integer
    value greater or equal to 2.

    When explicit levels are not specified,

    -  If there are already existing overviews, the corresponding levels will be
       used to refresh them if no explicit levels are specified.

    - Otherwise, appropriate overview power-of-two factors will be selected
      until the smallest overview is smaller than the value of the
      :option:`--min-size` switch.

    Mutually exclusive with :option:`--overview-src`

.. option:: --min-size <val>

    Maximum width or height of the smallest overview level. Only taken into
    account if explicit levels are not specified. Defaults to 256.

.. option::  --overview-src <INPUT>

    .. versionadded:: 3.12

    Add specified input raster datasets as overviews of the target dataset.
    Source overviews may come from any GDAL supported format, provided they
    have the same number of bands and geospatial extent than the target
    dataset.

    That mode is currently only implemented when the target dataset is in
    GeoTIFF format, or when using :option:`--external`.

    Mutually exclusive with :option:`--levels`

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

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/co.rst

    .. include:: gdal_options/oo.rst

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

       gdal raster overview add --external --levels=2,4,8,16 --co COMPRESS=YES erdas.img

.. example::
   :title: Create an external JPEG-compressed GeoTIFF overview file from a 3-band RGB dataset

   If the dataset is a writable GeoTIFF, you also need to add the :option:`--external` option to
   force the generation of external overview.

   .. code-block:: bash

       gdal raster overview add --co OVERVIEW=JPEG --co PHOTOMETRIC=YCBCR \
                                --co INTERLEAVE=PIXEL rgb_dataset.ext 2 4 8 16

.. example::
   :title: Create overviews for a specific subdataset

   For example, one of potentially many raster layers in a GeoPackage (the "filename" parameter must be driver prefix, filename and subdataset name, like e.g. shown by gdalinfo):

   .. code-block:: bash

       gdal raster overview add GPKG:file.gpkg:layer

.. example::
   :title: Add 3 existing datasets at scale 1:25K, 1:50K and 1:100K as overviews of :file:`my.tif`.

   .. code-block:: bash

       gdal raster overview add --overview-src ovr_25k.tif --overview-src ovr_50k.tif --overview-src ovr_100k.tif --dataset my.tif

.. example::
   :title: Create a COG file with non power-of-two overview levels.

   .. code-block:: bash

       gdal pipeline read input.tif ! reproject --dst-crs=EPSG:4326 ! add overview --levels 16,64,128 ! write output.tif --format=COG
