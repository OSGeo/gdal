.. _gdal_edit:

================================================================================
gdal_edit
================================================================================

.. only:: html

    Edit in place various information of an existing GDAL dataset.

.. Index:: gdal_edit

Synopsis
--------

.. code-block::

    gdal_edit [--help-general] [-ro] [-a_srs srs_def]
            [-a_ullr ulx uly lrx lry] [-a_ulurll ulx uly urx ury llx lly]
            [-tr xres yres] [-unsetgt] [-unsetrpc] [-a_nodata value] [-unsetnodata]
            [-unsetstats] [-stats] [-approx_stats]
            [-setstats min max mean stddev]
            [-scale value] [-offset value] [-units value]
            [-colorinterp_X red|green|blue|alpha|gray|undefined]*
            [-gcp pixel line easting northing [elevation]]*
            [-unsetmd] [-oo NAME=VALUE]* [-mo "META-TAG=VALUE"]*  datasetname

Description
-----------

The :program:`gdal_edit.py` script can be used to edit in place various
information of an existing GDAL dataset (projection, geotransform,
nodata, metadata).

It works only with raster formats that support update access to existing datasets.

.. caution::

    Depending on the format, older values of the updated information might
    still be found in the file in a "ghost" state, even if no longer accessible
    through the GDAL API. This is for example the case of the :ref:`raster.gtiff`
    format (this is not a exhaustive list).

.. option:: --help-general

    Gives a brief usage message for the generic GDAL commandline options and exit.

.. option:: -ro

    Open the dataset in read-only. Might be useful for drivers refusing to use
    the dataset in update-mode. In which case, updated information will go into
    PAM :file:`.aux.xml` files.

.. option:: -a_srs <srs_def>

    Defines the target coordinate system.
    This coordinate system will be written to the dataset.
    If the empty string or None is specified, then the existing
    coordinate system will be removed (for TIFF/GeoTIFF, might not be well
    supported besides that).

.. option:: -a_ullr ulx uly lrx lry:

    Assign/override the georeferenced bounds of the dataset.

.. option:: -a_ulurll ulx uly urx ury llx lly:

    Assign/override the georeferenced bounds of the dataset from three points:
    upper-left, upper-right and lower-left. Unlike :option:`-a_ullr`, this also
    supports rotated datasets (edges not parallel to coordinate system axes).

    .. versionadded:: 3.1

.. option:: -tr <xres> <yres>

    Set target resolution. The values must be expressed in georeferenced units.
    Both must be positive values.

.. option:: -unsetgt

    Remove the georeference information.

.. option:: -unsetrpc

    Remove RPC information.

    .. versionadded:: 2.4

.. option:: -unsetstats

    Remove band statistics information.

    .. versionadded:: 2.0

.. option:: -stats

    Calculate and store band statistics.

    .. versionadded:: 2.0

.. option:: -setstatsmin max mean stddev

    Store user-defined values for band statistics (minimum, maximum,
    mean and standard deviation). If any of the values is set to None,
    the real statistics are calclulated from the file and the ones set
    to None are used from the real statistics.

    .. versionadded:: 2.4

.. option:: -approx_stats

    Calculate and store approximate band statistics.

    .. versionadded:: 2.0

.. option:: -a_nodata <value>

    Assign a specified nodata value to output bands.

.. option:: -unsetnodata

    Remove existing nodata values.

    .. versionadded:: 2.1

.. option:: -scale <value>

    Assign a specified scale value to output bands.
    If a single scale value is provided it will be set for all bands.
    Alternatively one scale value per band can be provided, in which case
    the number of scale values must match the number of bands.
    If no scale is needed, it it recommended to set the value to 1.
    Scale and Offset are generally used together. For example, scale and
    offset might be used to store elevations in a unsigned 16bit integer
    file with a precision of 0.1, and starting from -100. True values
    would be calculated as: true_value = (pixel_value * scale) + offset

    .. note:: These values can be applied using -unscale during a :program:`gdal_translate` run.

    .. versionadded:: 2.2

.. option:: -offset <value>

    Assign a specified offset value to output bands.
    If a single offset value is provided it will be set for all bands.
    Alternatively one offset value per band can be provided, in which case
    the number of offset values must match the number of bands.
    If no offset is needed, it recommended to set the value to 0.
    For more see scale.

    .. versionadded:: 2.2

.. option:: -units <value>

    Assign a unit to output band(s).

    .. versionadded:: 3.1

.. option:: -colorinterp_X red|green|blue|alpha|gray|undefined

    Change the color interpretation of band X (where X is a valid band
    number, starting at 1).

    .. versionadded:: 2.3

.. option:: -gcp pixel line easting northing [elevation]

    Add the indicated ground control point to the dataset.
    This option may be provided multiple times to provide a set of GCPs.

.. option:: -unsetmd

    Remove existing metadata (in the default metadata domain).
    Can be combined with :option:`-mo`.

    .. versionadded:: 2.0

.. option:: -mo META-TAG=VALUE

    Passes a metadata key and value to set on the output dataset if possible.
    This metadata is added to the existing metadata items, unless :option:`-unsetmd`
    is also specified.

.. option:: -oo NAME=VALUE

    Open option (format specific).

    .. versionadded:: 2.0

The :option:`-a_ullr`, :option:`-a_ulurll`, :option:`-tr` and :option:`-unsetgt` options are exclusive.

The :option:`-unsetstats` and either :option:`-stats` or :option:`-approx_stats` options are exclusive.

Example
-------

.. code-block::

    gdal_edit -mo DATUM=WGS84 -mo PROJ=GEODETIC -a_ullr 7 47 8 46 test.ecw

.. code-block::

    gdal_edit -scale 1e3 1e4 -offset 0 10 twoBand.tif
