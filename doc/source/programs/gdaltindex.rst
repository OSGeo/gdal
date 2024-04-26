.. _gdaltindex:

================================================================================
gdaltindex
================================================================================

.. only:: html

    Creates an OGR-supported dataset as a raster tileindex.

.. Index:: gdaltindex

Synopsis
--------

.. code-block::

    gdaltindex [--help] [--help-general]
            [-overwrite] [-recursive] [-filename_filter <val>]...
            [-min_pixel_size <val>] [-max_pixel_size <val>]
            [-f <format>] [-tileindex <field_name>] [-write_absolute_path]
            [-skip_different_projection] [-t_srs <target_srs>]
            [-src_srs_name <field_name>] [-src_srs_format {AUTO|WKT|EPSG|PROJ}]
            [-lyr_name <name>] [-lco <NAME>=<VALUE>]...
            [-gti_filename <name>]
            [-tr <xres> <yres>] [-te <xmin> <ymin> <xmax> <ymax>]
            [-ot <datatype>] [-bandcount <val>] [-nodata <val>[,<val>...]]
            [-colorinterp <val>[,<val>...]] [-mask]
            [-mo <KEY>=<VALUE>]...
            [-fetch_md <gdal_md_name> <fld_name> <fld_type>]...
            <index_file> <file_or_dir> [<file_or_dir>]...

Description
-----------

This program creates an OGR-supported dataset with a record for each input raster file,
an attribute containing the filename, and a polygon geometry outlining the
raster.  This output is suitable for use with `MapServer <http://mapserver.org/>`__ as a raster
tileindex, or as input for the :ref:`GTI <raster.gti>` driver.

.. program:: gdaltindex

.. include:: options/help_and_help_general.rst

.. option:: -overwrite

    .. versionadded:: 3.9

    Overwrite the tile index if it already exists.

.. option:: -recursive

    .. versionadded:: 3.9

    Whether directories specified in <file_or_dir> should be explored recursively.

.. option:: -filename_filter <val>

    .. versionadded:: 3.9

    Pattern that the filenames contained in directories pointed by <file_or_dir>
    should follow.
    '*' is a wildcard character that matches any number of any characters
    including none. '?' is a wildcard character that matches a single character.
    Comparisons are done in a case insensitive way.
    Several filters may be specified.

    For example :``-filename_filter "*.tif" -filename_filter "*.tiff"``

.. option:: -min_pixel_size <val>

    .. versionadded:: 3.9

    Minimum pixel size in term of geospatial extent per pixel (resolution) that
    a raster should have to be selected. The pixel size
    is evaluated after reprojection of its extent to the target SRS defined
    by :option:`-t_srs`.

.. option:: -max_pixel_size <val>

    .. versionadded:: 3.9

    Maximum pixel size in term of geospatial extent per pixel (resolution) that
    a raster should have to be selected. The pixel size
    is evaluated after reprojection of its extent to the target SRS defined
    by :option:`-t_srs`.

.. option:: -f <format>

    The OGR format of the output tile index file. Starting with
    GDAL 2.3, if not specified, the format is guessed from the extension (previously
    was ESRI Shapefile).

.. option:: -tileindex <field_name>

    The output field name to hold the file path/location to the indexed
    rasters. The default tile index field name is ``location``.

.. option:: -write_absolute_path

    The absolute path to the raster files is stored in the tile index file.
    By default the raster filenames will be put in the file exactly as they
    are specified on the command line.

.. option:: -skip_different_projection

    Only files with same projection as files already inserted in the tileindex
    will be inserted (unless :option:`-t_srs` is specified). Default does not
    check projection and accepts all inputs.

.. option:: -t_srs <target_srs>

    Geometries of input files will be transformed to the desired target
    coordinate reference system.
    Default creates simple rectangular polygons in the same coordinate reference
    system as the input rasters.

.. option:: -src_srs_name <field_name>

    The name of the field to store the SRS of each tile. This field name can be
    used as the value of the TILESRS keyword in MapServer

.. option:: -src_srs_format {AUTO|WKT|EPSG|PROJ}

    The format in which the SRS of each tile must be written. Types can be
    ``AUTO``, ``WKT``, ``EPSG``, ``PROJ``.
    This option should be used together with :option:`-src_srs_format`.

.. option:: -lyr_name <name>

    Layer name to create/append to in the output tile index file.

.. option:: -lco <NAME>=<VALUE>

    .. versionadded:: 3.9

    Layer creation option (format specific)

.. option:: <index_file>

    The name of the output file to create/append to. The default dataset will
    be created if it doesn't already exist, otherwise it will append to the
    existing dataset.

.. option:: <file_or_dir>

    The input GDAL raster files, can be multiple files separated by spaces.
    Wildcards may also be used. Stores the file locations in the same style as
    specified here, unless :option:`-write_absolute_path` option is also used.

    Starting with GDAL 3.9, this can also be a directory name. :option:`-recursive`
    can also be used to recurse down to sub-directories.

    It is also possible to use the generic option ``--optfile filelist.txt``
    to specify a list of source files.


Options specific to use by the GDAL GTI driver
------------------------------------------------

gdaltindex can be used to generate a tile index suitable for use by the
:ref:`GTI <raster.gti>` driver. There are two possibilities:

- either use directly a vector tile index generated by gdaltindex as the input
  of the GTI driver

- or generate a small XML .gti wrapper file, for easier use with non-file-based
  formats such as databases, or for vector formats that do not support setting
  layer metadata items.

Formats that support layer metadata are for example GeoPackage (``-f GPKG``),
FlatGeoBuf (``-f FlatGeoBuf``) or PostGIS (``-f PG``)

Setting :option:`-tr` and :option:`-ot` is recommended to avoid the GTI
driver to have to deduce them by opening the first tile in the index. If the tiles
have nodata or mask band,  :option:`-nodata` and :option:`-mask` should also
be set.

In a GTI context, the extent of all tiles referenced in the tile index must
be expressed in a single SRS. Consequently, if input tiles may have different
SRS, either :option:`-t_srs` or :option:`-skip_different_projection` should be
specified.


.. option:: -gti_filename <name>

    .. versionadded:: 3.9

    Filename of the XML Virtual Tile Index file to generate, that can be used
    as an input for the GDAL GTI / Virtual Raster Tile Index driver.
    This can be useful when writing the tile index in a vector format that
    does not support writing layer metadata items.

.. option:: -tr <xres> <yres>

    .. versionadded:: 3.9

    Target resolution in SRS unit per pixel.

    Written in the XML Virtual Tile Index if :option:`-gti_filename`
    is specified, or as ``RESX`` and ``RESY`` layer metadata items for formats that
    support layer metadata.

.. option:: -te <xmin> <ymin> <xmax> <ymax>

    .. versionadded:: 3.9

    Target extent in SRS unit.

    Written in the XML Virtual Tile Index if :option:`-gti_filename`
    is specified, or as ``MINX``, ``MINY``, ``MAXX`` and ``MAXY`` layer metadata
    items for formats that support layer metadata.

.. option:: -ot <datatype>

    .. versionadded:: 3.9

    Data type of the tiles of the tile index: ``Byte``, ``Int8``, ``UInt16``,
    ``Int16``, ``UInt32``, ``Int32``, ``UInt64``, ``Int64``, ``Float32``, ``Float64``, ``CInt16``,
    ``CInt32``, ``CFloat32`` or ``CFloat64``

    Written in the XML Virtual Tile Index if :option:`-gti_filename`
    is specified, or as ``DATA_TYPE`` layer metadata item for formats that
    support layer metadata.

.. option:: -bandcount <val>

    .. versionadded:: 3.9

    Number of bands of the tiles of the tile index.

    Written in the XML Virtual Tile Index if :option:`-gti_filename`
    is specified, or as ``BAND_COUNT`` layer metadata item for formats that
    support layer metadata.

    A mix of tiles with N and N+1 bands is allowed, provided that the color
    interpretation of the (N+1)th band is alpha. The N+1 value must be written
    as the band count in that situation.

    If :option:`-nodata` or :option:`-colorinterp` are specified and have multiple
    values, the band count is also inferred from that number.

.. option:: -nodata <val>[,<val>...]

    .. versionadded:: 3.9

    Nodata value of the tiles of the tile index.

    Written in the XML Virtual Tile Index if :option:`-gti_filename`
    is specified, or as ``NODATA`` layer metadata item for formats that
    support layer metadata.

.. option:: -colorinterp <val>[,<val>...]

    .. versionadded:: 3.9

    Color interpretation of of the tiles of the tile index:
    ``red``, ``green``, ``blue``, ``alpha``, ``gray``, ``undefined``.

    Written in the XML Virtual Tile Index if :option:`-gti_filename`
    is specified, or as ``COLOR_INTERPRETATION`` layer metadata item for formats that
    support layer metadata.

.. option:: -mask

    .. versionadded:: 3.9

    Whether tiles in the tile index have a mask band.

    Written in the XML Virtual Tile Index if :option:`-gti_filename`
    is specified, or as ``MASK_BAND`` layer metadata item for formats that
    support layer metadata.

.. option:: -mo <KEY>=<VALUE>

    .. versionadded:: 3.9

    Write an arbitrary layer metadata item, for formats that support layer
    metadata.
    This option may be repeated.

    .. note:: This option cannot be used together :option:`-gti_filename`

.. option:: -fetch_md <gdal_md_name> <fld_name> <fld_type>

    .. versionadded:: 3.9

    Fetch a metadata item from the raster tile and write it as a field in the
    tile index.

    <gdal_md_name> should be the name of the raster metadata item.
    ``{PIXEL_SIZE}`` may be used as a special name to indicate the pixel size.

    <fld_name> should be the name of the field to create in the tile index.

    <fld_type> should be the name of the type to create.
    One of ``String``, ``Integer``, ``Integer64``, ``Real``, ``Date``, ``DateTime``

    This option may be repeated.

    For example: ``-fetch_md TIFFTAG_DATETIME creation_date DateTime``

Examples
--------

- Produce a shapefile (``doq_index.shp``) with a record for every
  image that the utility found in the ``doq`` folder. Each record holds
  information that points to the location of the image and also a bounding rectangle
  shape showing the bounds of the image:

::

    gdaltindex doq_index.shp doq/*.tif

- Perform the same command as before, but now we create a GeoPackage instead of a Shapefile.

::

    gdaltindex -f GPKG doq_index.gpkg doq/*.tif

- The :option:`-t_srs` option can also be used to transform all input rasters
  into the same output projection:

::

    gdaltindex -t_srs EPSG:4326 -src_srs_name src_srs tile_index_mixed_srs.shp *.tif

- Make a tile index from files listed in a text file, with metadata suitable
  for use by the GDAL GTI / Virtual Raster Tile Index driver.

::

    gdaltindex tile_index.gti.gpkg -datatype Byte -tr 60 60 -colorinterp Red,Green,Blue --optfile my_list.txt

C API
-----

This utility is also callable from C with :cpp:func:`GDALTileIndex`.

See also
--------

:ref:`raster_common_options` for other command-line options, and in particular the
:ref:`--optfile <raster_common_options_optfile>` switch that can be used to specify a list of input datasets.
