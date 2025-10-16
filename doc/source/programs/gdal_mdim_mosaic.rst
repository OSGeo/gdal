.. _gdal_mdim_mosaic:

================================================================================
``gdal mdim mosaic``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Build a mosaic, either virtual (VRT) or materialized, from multidimensional datasets.

.. Index:: gdal mdim mosaic

Synopsis
--------

.. program-output:: gdal mdim mosaic --help-doc

Description
-----------

:program:`gdal mdim mosaic` builds a mosaic of multidimensional array(s),
from a list of input GDAL multidimensional datasets,
that can be either a virtual mosaic in the
:ref:`Multidimensional VRT (Virtual Dataset) <vrt_multidimensional>` format,
or in a more conventional multidimensional format such as :ref:`raster.netcdf` or :ref:`raster.zarr`.

Wildcards '*', '?' or '['] of :cpp:func:`VSIGlob` can be used for input dataset
names, even on files located on network file systems such as /vsis3/, /vsigs/, /vsiaz/, etc.
Alternatively if a input dataset name is prefixed by the `@` character, it will
be assumed to contain the list of actual dataset names (one per line) to use
as input datasets.

To create a mosaic from an array, certain structural constraints apply to all
contributing arrays from the input datasets:

- All contributing arrays must have the same array name and path (e.g. "/measurements/pressure")
- They must have the same data type and nodata value
- They must share the same number of dimensions, in the same order, and with the same names.
- each dimension of the array must have an associated one-dimensional numeric indexing variable.

A typical use case is when a multidimensional array is sliced into several files,
and it is desired to reconstruct the full array.

The program can support mosaicing along several axes.

There might be "holes" among the source datasets along the mosaiced dimensions,
and possibly overlaps (but only when regularly spacing along a mosaiced dimension).
Holes are filled with the nodata value, if defined in the source arrays,
or zero-initialized otherwise. In case of overlap, the order of the input list
is used to determine priority. Files that are listed at the end are the ones
from which the content will be fetched.

Different typical use cases that can be addressed by this program are:

- a 3D array is sliced along a single dimension (for example a temporal one)
  into several files with each slice having a sample along that dimension. In
  that use case, the timestamps of the slices do not need to be regularly spaced.

- a 2D array is chunked/tiled along X and Y dimensions, with a regular spacing
  along those X and Y dimensions, and where the coordinate values are aligned on
  the same grid among chunks.

By default, a target array is created for each array with two or more dimensions.
This behavior can be restricted to specific arrays using the :option:`--array` option.

The following options are available:

Standard options
++++++++++++++++

.. option:: -i, --input <INPUT>

   Input multidimensional dataset names. Required.

   Wildcards '*', '?' or '['] of :cpp:func:`VSIGlob` can be used for input dataset
   names, even on files located on network file systems such as /vsis3/, /vsigs/, /vsiaz/, etc.
   Alternatively if a input dataset name is prefixed by the `@` character, it will
   be assumed to contain the list of actual dataset names (one per line) to use
   as input datasets.

.. option::  -o, --output <OUTPUT>

   Output multidimensional dataset name. Required.

.. option:: -f, --of, --format, --output-format <OUTPUT-FORMAT>

    Select the output format. This can be a format that supports multidimensional
    output (such as :ref:`raster.netcdf`, :ref:`vrt_multidimensional`).
    When this option is not specified, the format is guessed when possible from
    the extension of the destination filename.

.. option:: --co, --creation-option <KEY>=<VALUE>

    Many formats have one or more optional creation options that can be
    used to control particulars about the file created.

    The creation options available vary by format driver, and some
    simple formats have no creation options at all. A list of options
    supported for a format can be listed with the
    :ref:`--format <raster_common_options_format>`
    command line option but the documentation for the format is the
    definitive source of information on driver creation options.
    See :ref:`raster_drivers` format
    specific documentation for legal creation options for each format.

    Array-level creation options may be passed by prefixing them with ``ARRAY:``.
    See :cpp:func:`GDALGroup::CopyFrom` for further details regarding such options.

.. include:: gdal_options/overwrite.rst

.. option:: --array <ARRAYS>

    Name or full path of one or several arrays to mosaic. If not specified,
    a target array is created for each array with two or more dimensions.

Advanced options
++++++++++++++++

.. include:: gdal_options/oo.rst

.. include:: gdal_options/if.rst

Examples
--------

.. example::
   :title: Mosaic together several all netCDF files starting with ``slice`` that are slices of a 3D array

   .. code-block:: bash

      gdal mdim mosaic slice*.nc out.vrt
