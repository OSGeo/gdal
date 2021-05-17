.. _gdalbuildvrt:

================================================================================
gdalbuildvrt
================================================================================

.. only:: html

    Builds a VRT from a list of datasets.

.. Index:: gdalbuildvrt

Synopsis
--------

.. code-block::

    gdalbuildvrt [-tileindex field_name]
                [-resolution {highest|lowest|average|user}]
                [-te xmin ymin xmax ymax] [-tr xres yres] [-tap]
                [-separate] [-b band]* [-sd subdataset]
                [-allow_projection_difference] [-q]
                [-addalpha] [-hidenodata]
                [-srcnodata "value [value...]"] [-vrtnodata "value [value...]"]
                [-ignore_srcmaskband]
                [-a_srs srs_def]
                [-r {nearest,bilinear,cubic,cubicspline,lanczos,average,mode}]
                [-oo NAME=VALUE]*
                [-input_file_list my_list.txt] [-overwrite] output.vrt [gdalfile]*

Description
-----------

This program builds a VRT (Virtual Dataset) that is a mosaic of the list of
input GDAL datasets. The list of input GDAL datasets can be specified at the end
of the command line, or put in a text file (one filename per line) for very long lists,
or it can be a MapServer tileindex (see \ref gdaltindex utility). In the later case, all
entries in the tile index will be added to the VRT.

With -separate, each files goes into a separate band in the VRT dataset. Otherwise,
the files are considered as tiles of a larger mosaic and the VRT file has as many bands as one
of the input files.

If one GDAL dataset is made of several subdatasets and has 0 raster bands,
all the subdatasets will be added to the VRT rather than the dataset itself.

gdalbuildvrt does some amount of checks to assure that all files that will be put
in the resulting VRT have similar characteristics : number of bands, projection, color
interpretation... If not, files that do not match the common characteristics will be skipped.
(This is only true in the default mode, and not when using the -separate option)

If there is some amount of spatial overlapping between files, the order of files
appearing in the list of source matter: files that are listed at the end are the ones
from which the content will be fetched. Note that nodata will be taken into account
to potentially fetch data from less priority datasets, but currently, alpha channel
is not taken into account to do alpha compositing (so a source with alpha=0
appearing on top of another source will override is content). This might be
changed in later versions.

.. program:: gdalbuildvrt

.. option:: -tileindex

    Use the specified value as the tile index field, instead of the default
    value which is 'location'.

.. option:: -resolution {highest|lowest|average|user}

    In case the resolution of all input files is not the same, the -resolution flag
    enables the user to control the way the output resolution is computed.

    `highest` will pick the smallest values of pixel dimensions within the set of source rasters.

    `lowest` will pick the largest values of pixel dimensions within the set of source rasters.

    `average` is the default and will compute an average of pixel dimensions within the set of source rasters.

    `user` must be used in combination with the :option:`-tr` option to specify the target resolution.

.. option:: -tr <res> <yres>

    Set target resolution. The values must be expressed in georeferenced units.
    Both must be positive values. Specifying those values is of course incompatible with
    highest|lowest|average values for :option:`-resolution` option.


.. option:: -tap

    (target aligned pixels) align
    the coordinates of the extent of the output file to the values of the :option:`-tr`,
    such that the aligned extent includes the minimum extent.

.. option:: -te xmin ymin xmax ymax

    Set georeferenced extents of VRT file. The values must be expressed in georeferenced units.
    If not specified, the extent of the VRT is the minimum bounding box of the set of source rasters.

.. option:: -addalpha

    Adds an alpha mask band to the VRT when the source raster have none. Mainly useful for RGB sources (or grey-level sources).
    The alpha band is filled on-the-fly with the value 0 in areas without any source raster, and with value
    255 in areas with source raster. The effect is that a RGBA viewer will render
    the areas without source rasters as transparent and areas with source rasters as opaque.
    This option is not compatible with :option:`-separate`.

.. option:: -hidenodata

    Even if any band contains nodata value, giving this option makes the VRT band
    not report the NoData. Useful when you want to control the background color of
    the dataset. By using along with the -addalpha option, you can prepare a
    dataset which doesn't report nodata value but is transparent in areas with no
    data.

.. option:: -srcnodata <value> [<value>...]

    Set nodata values for input bands (different values can be supplied for each band). If
    more than one value is supplied all values should be quoted to keep them
    together as a single operating system argument. If the option is not specified, the
    intrinsic nodata settings on the source datasets will be used (if they exist). The value set by this option
    is written in the NODATA element of each ComplexSource element. Use a value of
    `None` to ignore intrinsic nodata settings on the source datasets.

.. option:: -ignore_srcmaskband

    .. versionadded:: 3.3

    Starting with GDAL 3.3, if a source has a mask band (internal/external mask
    band, or alpha band), a <ComplexSource> element is created by default with
    a <UseMaskBand>true</UseMaskBand> child element, to instruct the VRT driver
    to use the mask band of the source to mask pixels being composited. This is
    a generalization of the NODATA element.
    When specifying the -ignore_srcmaskband option, the mask band of sources will
    not be taken into account, and in case of overlapping between sources, the
    last one will override previous ones in areas of overlap.

.. option:: -b <band>

    Select an input <band> to be processed. Bands are numbered from 1.
    If input bands not set all bands will be added to vrt.
    Multiple :option:`-b` switches may be used to select a set of input bands.

.. option:: -sd< <subdataset>

    If the input
    dataset contains several subdatasets use a subdataset with the specified
    number (starting from 1). This is an alternative of giving the full subdataset
    name as an input.

.. option:: -vrtnodata <value> [<value>...]

    Set nodata values at the VRT band level (different values can be supplied for each band).  If more
    than one value is supplied all values should be quoted to keep them together
    as a single operating system argument.  If the option is not specified,
    intrinsic nodata settings on the first dataset will be used (if they exist). The value set by this option
    is written in the NoDataValue element of each VRTRasterBand element. Use a value of
    `None` to ignore intrinsic nodata settings on the source datasets.

.. option:: -separate

    Place each input file into a separate band. In that case, only the first
    band of each dataset will be placed into a new band. Contrary to the default mode, it is not
    required that all bands have the same datatype.

.. option:: -allow_projection_difference

    When this option is specified, the utility will accept to make a VRT even if the input datasets have
    not the same projection. Note: this does not mean that they will be reprojected. Their projection will
    just be ignored.

.. option:: -a_srs <srs_def>

    Override the projection for the output file.  The <srs_def> may be any of the usual GDAL/OGR forms,
    complete WKT, PROJ.4, EPSG:n or a file containing the WKT. No reprojection is done.

.. option:: -r {nearest (default),bilinear,cubic,cubicspline,lanczos,average,mode}

    Select a resampling algorithm.

.. option:: -oo NAME=VALUE

    Dataset open option (format specific)

    .. versionadded:: 2.2

.. option:: -input_file_list <mylist.txt>

    To specify a text file with an input filename on each line

.. option:: -q

    To disable the progress bar on the console

.. option:: -overwrite

    Overwrite the VRT if it already exists.

Examples
--------

- Make a virtual mosaic from all TIFF files contained in a directory :

::

    gdalbuildvrt doq_index.vrt doq/*.tif

- Make a virtual mosaic from files whose name is specified in a text file :

::

    gdalbuildvrt -input_file_list my_list.txt doq_index.vrt


- Make a RGB virtual mosaic from 3 single-band input files :

::

    gdalbuildvrt -separate rgb.vrt red.tif green.tif blue.tif

- Make a virtual mosaic with blue background colour (RGB: 0 0 255) :

::

    gdalbuildvrt -hidenodata -vrtnodata "0 0 255" doq_index.vrt doq/*.tif
