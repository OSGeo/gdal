.. _gdal_raster_overview_refresh:

================================================================================
``gdal raster overview refresh``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Refresh overviews

.. Index:: gdal raster overview refresh

Synopsis
--------

.. program-output:: gdal raster overview refresh --help-doc

Description
-----------

:program:`gdal raster overview refresh` can be used to refresh existing overviews
of a dataset.
By default all overviews are refreshed, but it is also possible to restrict
the refreshed overviews by level and/or extent.

Program-Specific Options
------------------------

.. option:: --bbox <xmin>,<ymin>,<xmax>,ymax>

    This option performs a partial refresh of existing overviews, in the region
    of interest specified by georeferenced coordinates, in CRS units.

    'x' is longitude values for geographic CRS and easting for projected CRS.
    'y' is latitude values for geographic CRS and northing for projected CRS.

.. option:: --dataset <DATASET>

    Dataset name, to be updated in-place by default (unless :option:`--external` is specified). Required.

.. option:: --external

    Refresh external ``.ovr`` overviews.

.. option:: --levels <level1,level2,...>

    A list of overview levels to build. Each overview level must be an integer
    value greater or equal to 2.

    If not specified all existing overviews are refreshed.

.. option:: --like <filename1>[,<filenameN>]...

    This option performs a partial refresh of existing overviews, in the region
    of interest specified by one or several filenames (names separated by comma).
    Note that the filenames are only used to determine the regions of interest
    to refresh. The reference source pixels are the one of the main dataset.
    By default all existing overview levels will be refreshed, unless explicit
    levels are specified. See :example:`refresh-tiff`.


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

.. option:: --use-source-timestamp

    This option performs a partial refresh of existing overviews of a
    :ref:`raster.vrt` or :ref:`raster.gti` file with an external overview.
    It checks the modification timestamp of all the sources of the VRT
    and regenerate the overview for areas corresponding to sources whose
    timestamp is more recent than the external overview of the VRT.
    By default all existing overview levels will be refreshed, unless explicit
    levels are specified. See :example:`refresh-vrt`.

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/oo.rst

Examples
--------

.. example::
   :title: Refresh external overviews of a VRT file using timestamp of source files
   :id: refresh-vrt

   This is needed when for sources have been modified after the .vrt.ovr generation:

   .. code-block:: bash

       gdal raster mosaic tile1.tif tile2.tif my.vrt               # create VRT
       gdal raster overview add --external -r cubic my.vrt         # initial overview generation
       touch tile1.tif                                             # simulate update of one of the source tiles
       gdal raster overview refresh --external -r cubic \
                                 --use-source-timestamp my.vrt     # refresh overviews


.. example::
   :title: Refresh (internal) overviews of a TIFF file
   :id: refresh-tiff

   .. code-block:: bash

       gdal raster mosaic tile1.tif tile2.tif mosaic.tif       # create mosaic
       gdal raster overview add -r cubic mosaic.tif            # initial overview generation
       gdalwarp tile1_modif.tif mosaic.tif                     # update mosaic
       gdal raster overview refresh --like=tile1.tif my.tif    # refresh overviews
