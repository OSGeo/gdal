.. _gdal_translate:

================================================================================
gdal_translate
================================================================================

.. only:: html

    Converts raster data between different formats.

.. Index:: gdal_translate

Synopsis
--------

.. program-output:: gdal_translate --help-doc

Description
-----------

The :program:`gdal_translate` utility can be used to convert raster data between
different formats, potentially performing some operations like subsetting,
resampling, and rescaling pixels in the process.

.. tip:: Equivalent in new "gdal" command line interface:

    * :ref:`gdal_raster_convert` for format translation.
    * :ref:`gdal_raster_clip` for spatial subsetting.
    * :ref:`gdal_raster_nodata_to_alpha` to add an alpha channel from nodata values.
    * :ref:`gdal_raster_resize` for image resizing.
    * :ref:`gdal_raster_scale` to scale pixel values.
    * :ref:`gdal_raster_set_type` to change the band data type.


.. program:: gdal_translate

.. include:: options/help_and_help_general.rst

.. include:: options/ot.rst

.. option:: -strict

    Enable strict mode. In this mode, GDAL will fail instead of silently
    performing operations that may lead to loss of information, such as
    data type conversions that cannot be exactly preserved.

    The exact behavior of this option is driver-dependent. Most raster
    drivers use it to enforce strict preservation of the input data type
    and will report an error if the requested operation cannot be performed
    without data loss. See :example:`strict`.

.. include:: options/if.rst

.. include:: options/of.rst

.. option:: -b <band>

    Select an input band **band** for output. Bands are numbered from 1.
    Multiple :option:`-b` switches may be used to select a set of input bands
    to write to the output file, or to reorder bands. **band** can also be set
    to "mask,1" (or just "mask") to mean the mask band of the first band of the
    input dataset.

.. option:: -mask <band>

    Select an input band **band** to create output dataset mask band. Bands are
    numbered from 1. **band** can be set to "none" to avoid copying the global
    mask of the input dataset if it exists. Otherwise it is copied by default
    ("auto"), unless the mask is an alpha channel, or if it is explicitly used
    to be a regular band of the output dataset ("-b mask"). **band** can also
    be set to "mask,1" (or just "mask") to mean the mask band of the 1st band
    of the input dataset.

.. option:: -expand gray|rgb|rgba

    To expose a dataset with 1 band with a color table as a dataset with
    3 (RGB) or 4 (RGBA) bands. Useful for output drivers such as JPEG,
    JPEG2000, MrSID, ECW that don't support color indexed datasets. The 'gray'
    value enables to expand a dataset with a color table that only contains
    gray levels to a gray indexed dataset.

.. option:: -outsize <xsize>[%]|0 <ysize>[%]|0

    Set the size of the output file.  Outsize is in pixels and lines unless '%'
    is attached in which case it is as a fraction of the input image size.
    If one of the 2 values is set to 0, its value will be determined from the
    other one, while maintaining the aspect ratio of the source dataset.

.. option:: -tr <xres> <yres>

    Set target resolution. The values must be expressed in georeferenced units.
    Both must be positive values. This is mutually exclusive with
    :option:`-outsize`, :option:`-a_ullr`, and :option:`-a_gt`.

.. option:: -ovr {<level>|AUTO|AUTO-<n>|NONE}

    .. versionadded:: 3.6

    To specify which overview level of source file must be used. The default choice,
    AUTO, will select the overview level whose resolution is the closest to the
    target resolution. Specify an integer value (0-based, i.e. 0=1st overview level)
    to select a particular level. Specify AUTO-n where n is an integer greater or
    equal to 1, to select an overview level below the AUTO one. Or specify NONE to
    force the base resolution to be used (can be useful if overviews have been
    generated with a low quality resampling method, and a higher quality resampling method
    is specified with :option:`-r`.)

    When :option:`-ovr` is specified as an integer value,
    and neither :option:`-outsize` nor :option:`-tr` is specified, the size of
    the overview will be used as the output size.

    When :option:`-ovr` is specified, values of :option:`-srcwin`
    should be expressed as pixel offset and size of the full resolution source dataset.
    Similarly when using :option:`-outsize` with percentage values, they refer to the size
    of the full resolution source dataset.

.. option:: -r {nearest|bilinear|cubic|cubicspline|lanczos|average|rms|mode}

    Select a resampling algorithm.

    ``nearest`` (default) applies a nearest neighbour (simple sampling) resampler

    ``average`` computes the average of all non-NODATA contributing pixels. Starting with GDAL 3.1, this is a weighted average taking into account properly the weight of source pixels not contributing fully to the target pixel.

    ``rms`` computes the root mean squared / quadratic mean of all non-NODATA contributing pixels (GDAL >= 3.3)

    ``bilinear`` applies a bilinear convolution kernel.

    ``cubic`` applies a cubic convolution kernel.

    ``cubicspline`` applies a B-Spline convolution kernel.

    ``lanczos`` applies a Lanczos windowed sinc convolution kernel.

    ``mode`` selects the value which appears most often of all the sampled points.

.. option:: -scale [<src_min> <src_max> [<dst_min> <dst_max>]]

    Rescale the input pixels values from the range **src_min** to **src_max**
    to the range **dst_min** to **dst_max**.
    If omitted the output range is from the minimum value to the maximum value allowed
    for integer data types (for example from 0 to 255 for Byte output) or from 0 to 1
    for floating-point data types.
    If omitted the input range is automatically computed from the
    source dataset, in its whole (not just the window of interest potentially
    specified with :option:`-srcwin` or :option:`-projwin`). This may be a
    slow operation on a large source dataset, and if using it multiple times
    for several gdal_translate invocation, it might be beneficial to call
    ``gdalinfo -stats {source_dataset}`` priorly to precompute statistics, for
    formats that support serializing statistics computations (GeoTIFF, VRT...)
    Note that the values specified after :option:`-scale` are only used to compute a scale and
    offset to apply to the input raster values. In particular, ``src_min`` and
    ``src_max`` are not used to clip input values unless :option:`-exponent`
    is also specified.
    Instead of being clipped, source values that are outside the range of ``src_min`` and ``src_max`` will be scaled to values outside the range of ``dst_min`` and ``dst_max``.
    If clipping without exponential scaling is desired,
    ``-exponent 1`` can be used.
    :option:`-scale` can be repeated several times (if specified only once,
    it also applies to all bands of the output dataset), so as to specify per
    band parameters. It is also possible to use the ``-scale_bn`` syntax where bn
    is a band number (e.g. ``-scale_2`` for the 2nd band of the output dataset)
    to specify the parameters of one or several specific bands.

.. option:: -exponent <exp_val>

    Apply non-linear scaling with a power function. ``exp_val`` is the exponent
    of the power function (must be positive). This option must be used with the
    :option:`-scale` option. If specified only once, :option:`-exponent` applies
    to all bands of
    the output image. It can be repeated several times so as to specify per
    band parameters. It is also possible to use the ``-exponent_bn`` syntax where
    bn is a band number (e.g. ``-exponent_2`` for the 2nd band of the output
    dataset) to specify the parameters of one or several specific bands.

    The scaled value ``Dst`` is calculated from the source value ``Src`` with the following
    formula:

    .. math::
        {Dst} = \left( {Dst}_{max} - {Dst}_{min} \right) \times \operatorname{max} \left( 0, \operatorname{min} \left( 1, \left( \frac{{Src} - {Src}_{min}}{{Src}_{max}-{Src}_{min}} \right)^{exp\_val} \right) \right) + {Dst}_{min}


.. option:: -unscale

    Apply the scale/offset metadata for the bands to convert linearly-scaled values to
    unscaled values.  It is also often necessary to reset the output datatype
    with the :option:`-ot` switch.
    The unscaled value is computed from the scaled raw value with the following
    formula:

    .. math::
        {unscaled\_value} = {scaled\_value} * {scale} + {offset}

.. option:: -srcwin <xoff> <yoff> <xsize> <ysize>

    Selects a subwindow from the source image for copying based on pixel/line
    location. Pixel/line offsets (``xoff`` and ``yoff``) are measured from the
    left and top of the image.
    If the subwindow extends beyond the bounds of the source dataset,
    output pixels will be written with a value of zero, unless a NoData value is
    defined either in the source dataset or by :option:`-a_nodata`.
    Alternatively, :program:`gdal_translate` can issue an error in this case
    if so directed by options :option:`-epo` or :option:`-eco`.

.. option:: -projwin <ulx> <uly> <lrx> <lry>

    Selects a subwindow from the source image for copying
    (like :option:`-srcwin`) but with the corners given in georeferenced
    coordinates (by default expressed in the SRS of the dataset. Can be
    changed with :option:`-projwin_srs`). If the subwindow extends beyond
    the bounds of the source dataset, output pixels will be written with a value
    of zero, unless a NoData value is defined either in the source dataset or
    by :option:`-a_nodata`. Alternatively, :program:`gdal_translate` can issue
    an error in this case if so directed by options :option:`-epo` or :option:`-eco`.

    .. note::

        Beginning with GDAL 3.11, the extent described by :option:`-projwin` will
        be transformed into the dataset SRS and used to select a subwindow.
        Before GDAL 3.11, only the two corner points were transformed into the
        dataset SRS, and these two transformed points were used to define an extent
        in the dataset SRS. Depending on the SRS involved, the subwindow selected in
        GDAL 3.11 may be substantially larger than in previous versions.

    .. note::

        When using nearest-neighbor resampling, the window specified by
        :option:`-projwin` is expanded (rounded, for GDAL < 3.11) if necessary
        to match input pixel boundaries. For other resampling algorithms, the window
        is not modified.

.. option:: -projwin_srs <srs_def>

    Specifies the SRS in which to interpret the coordinates given with
    :option:`-projwin`. The <srs_def> may be any of the usual GDAL/OGR forms,
    complete WKT, PROJ.4, EPSG:n or a file containing the WKT.

    .. warning::
        This does not cause reprojection of the dataset to the specified SRS.

.. option:: -epo

    (Error when Partially Outside) If this option is set, :option:`-srcwin` or
    :option:`-projwin` values that falls partially outside the source raster
    extent will be considered as an error. The default behavior is to accept
    such requests.

.. option:: -eco

    (Error when Completely Outside) Same as :option:`-epo`, except that the
    criterion for erroring out is when the request falls completely outside
    the source raster extent.

.. option:: -a_srs <srs_def>

    Override the projection for the output file. Can be used with
    :option:`-a_ullr` or :option:`-a_gt` to specify the extent in this projection.

    .. include:: options/srs_def.rst

    .. note:: No reprojection is done.

.. option:: -a_coord_epoch <epoch>

    .. versionadded:: 3.4

    Assign a coordinate epoch, linked with the output SRS. Useful when the
    output SRS is a dynamic CRS.

.. option:: -a_scale <value>

    Set band scaling value. No modification of pixel values is done.
    Note that the :option:`-unscale` does not take into account :option:`-a_scale`.
    You may for example specify ``-scale 0 1 <offset> <offset+scale>`` to
    apply a (offset, scale) tuple, for the equivalent of the 2 steps:
    ``gdal_translate input.tif tmp.vrt -a_scale scale -a_offset offset`` followed by
    ``gdal_translate tmp.vrt output.tif -unscale``

.. option:: -a_offset <value>

    Set band offset value. No modification of pixel values is done.
    Note that the :option:`-unscale` does not take into account :option:`-a_offset`.
    You may for example specify ``-scale 0 1 <offset> <offset+scale>`` to
    apply a (offset, scale) tuple, for the equivalent of the 2 steps:
    ``gdal_translate input.tif tmp.vrt -a_scale scale -a_offset offset`` followed by
    ``gdal_translate tmp.vrt output.tif -unscale``

.. option:: -a_ullr <ulx> <uly> <lrx> <lry>

    Assign/override the georeferenced bounds of the output file.  This assigns
    georeferenced bounds to the output file, ignoring what would have been
    derived from the source file. So this does not cause reprojection to the
    specified SRS.
    This is mutually exclusive with :option:`-a_gt`

.. option:: -a_gt <gt(0)> <gt(1)> <gt(2)> <gt(3)> <gt(4)> <gt(5)>

    Assign/override the geotransform of the output file.
    This assigns the geotransform to the output file, ignoring what would have been
    derived from the source file. So this does not cause reprojection to the
    specified SRS. See :ref:`geotransforms_tut`.
    This is mutually exclusive with :option:`-a_ullr`

    .. versionadded:: 3.8

.. option:: -a_nodata <value>

    Assign a specified nodata value to output bands. It can
    be set to ``none`` to avoid setting a nodata value to the output file if
    one exists for the source file. Note that, if the input dataset has a
    nodata value, this does not cause pixel values that are equal to that nodata
    value to be changed to the value specified with this option.

.. option:: -colorinterp_X <red|green|blue|alpha|gray|undefined|pan|coastal|rededge|nir|swir|mwir|lwir|...>

    Override the color interpretation of band X (where X is a valid band number,
    starting at 1)

.. option:: -colorinterp {red|green|blue|alpha|gray|undefined|pan|coastal|rededge|nir|swir|mwir|lwir|...},...

    Override the color interpretation of all specified bands. For
    example -colorinterp red,green,blue,alpha for a 4 band output dataset.

.. option:: -mo <META-TAG>=<VALUE>

    Passes a metadata key and value to set on the output dataset if possible.

.. option:: -dmo DOMAIN:META-TAG=VALUE

    Passes a metadata key and value in specified domain to set on the output dataset if possible.

    .. versionadded:: 3.9

.. option:: -co <NAME>=<VALUE>

    .. WARNING: if modifying the 2 below paragraphs, please edit options/co.rst too

    Many formats have one or more optional creation options that can be
    used to control particulars about the file created. For instance,
    the GeoTIFF driver supports creation options to control compression,
    and whether the file should be tiled.

    The creation options available vary by format driver, and some
    simple formats have no creation options at all. A list of options
    supported for a format can be listed with the
    :ref:`--format <raster_common_options_format>`
    command line option but the documentation for the format is the
    definitive source of information on driver creation options.
    See :ref:`raster_drivers` format
    specific documentation for legal creation options for each format.

    In addition to the driver-specific creation options, gdal_translate
    (and :cpp:func:`GDALTranslate` and :cpp:func:`GDALCreateCopy`) recognize
    the following options:

    - .. co:: APPEND_SUBDATASET
         :choices: YES, NO
         :default: NO

      Can be specified to YES to avoid prior destruction of existing dataset,
      for drivers that support adding several subdatasets (e.g. GTIFF, NITF)

    - .. co:: COPY_SRC_MDD
         :choices: AUTO, YES, NO
         :default: AUTO
         :since: 3.8

      Defines if metadata domains of the source dataset should be copied to the
      destination dataset.
      In the default AUTO mode, only "safe" domains will be copied, which
      include the default metadata domain (some drivers may include other
      domains such as IMD, RPC, GEOLOCATION).
      When setting YES, all domains will be copied (but a few reserved ones like
      IMAGE_STRUCTURE or DERIVED_SUBDATASETS).
      Currently only recognized by the GTiff, COG, VRT, PNG and JPEG drivers.

      When setting NO, no source metadata will be copied.

    - .. co:: SRC_MDD
         :choices: <domain_name>
         :since: 3.8

      Defines which source metadata domain should be copied.
      This option restricts the list of source metadata domains to be copied
      (it implies COPY_SRC_MDD=YES if it is not set). This option may be specified
      as many times as they are source domains. The default metadata domain is the
      empty string "" ("_DEFAULT_") may also be used when empty string is not practical).
      Currently only recognized by the GTiff, COG, VRT, PNG and JPEG drivers.

.. option:: -nogcp

    Do not copy the GCPs in the source dataset to the output dataset.

.. option:: -gcp <pixel> <line> <easting> <northing> [<elevation>]

    Add the indicated ground control point to the output dataset.  This option
    may be provided multiple times to provide a set of GCPs.

.. option:: -q

    Suppress progress monitor and other non-error output.

.. option:: -sds

    Copy all subdatasets of this file to individual output files.  Use with
    formats like HDF that have subdatasets.

.. option:: -stats

    Force (re)computation of statistics.

.. option:: -norat

    Do not copy source RAT into destination dataset.

.. option:: -noxmp

    Do not copy the XMP metadata in the source dataset to the output dataset when driver is able to copy it.

    .. versionadded:: 3.2

.. option:: -oo <NAME>=<VALUE>

    Dataset open option (format specific)

.. option:: <src_dataset>

    The source dataset name. It can be either file name, URL of data source or
    subdataset name for multi-dataset files.

.. option:: <dst_dataset>

    The destination file name.


Nodata / source validity mask handling during resampling
--------------------------------------------------------

Masked values, either identified through a nodata value metadata set on the
source band, a mask band, an alpha band will not be used during resampling
(when using :option:`-outsize` or :option:`-tr`).

.. include:: nodata_handling_gdaladdo_gdal_translate.rst

C API
-----

This utility is also callable from C with :cpp:func:`GDALTranslate`.

Examples
--------

.. example::
   :title: Creating a tiled GeoTIFF

   .. code-block:: bash

      gdal_translate -of GTiff -co "TILED=YES" utm.tif utm_tiled.tif


.. example::
   :title: Creating a JPEG-compressed TIFF with internal mask from a RGBA dataset

   .. code-block:: bash

      gdal_translate rgba.tif withmask.tif -b 1 -b 2 -b 3 -mask 4 -co COMPRESS=JPEG \
        -co PHOTOMETRIC=YCBCR --config GDAL_TIFF_INTERNAL_MASK YES


.. example::
   :title: Creating a RGBA dataset from a RGB dataset with a mask

   .. code-block:: bash

       gdal_translate withmask.tif rgba.tif -b 1 -b 2 -b 3 -b mask


.. example::
   :title: Subsetting using :option:`-projwin` and :option:`-outsize`

   .. code-block:: bash

      gdal_translate -projwin -20037500 10037500 0 0 -outsize 100 100 frmt_wms_googlemaps_tms.xml junk.png


.. example::
   :title: Use of strict mode with unsupported data type
   :id: strict

   .. code-block:: console

       $ gdal_create test.tif -bands 3 -ot Int16 -outsize 1 1
       $ gdal_translate -strict test.tif test.webp
       ERROR 6: WEBP driver doesn't support data type Int16.
       Only UInt8 bands supported.
