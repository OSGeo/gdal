.. _raster.openfilegdb:

ESRI File Geodatabase raster (OpenFileGDB)
==========================================

.. versionadded:: 3.7

.. shortname:: OpenFileGDB

.. built_in_by_default::

The OpenFileGDB driver provides read access to raster layers of File
Geodatabases (.gdb directories). The dataset name must be the directory/folder
name, and it must end with the .gdb extension.

It can also read directly zipped .gdb directories (with .gdb.zip
extension), provided they contain a .gdb directory at their first level.

The driver supports:

- reading CRS information
- reading geotransform
- exposing overviews
- exposing nodata mask band or nodata value
- uncompressed, LZ77, JPEG and JPEG2000 compression methods.
- exposing value attribute tables as GDAL Raster attribute tables.

Support for FileGDB created by ArcGIS v10 has been added in GDAL 3.7
Support for FileGDB created by ArcGIS v9 has been added in GDAL 3.8

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Connection string
-----------------

When a File Geodatabase contains several raster layers, the raster layer that
must be opened is specified with the following connection string

::

    OpenFileGDB:"/path/to/my.gdb":name_of_raster_layer

When opening a dataset with pointing only to the .gdb directory, and if it
contains several raster layers, a list of subdatasets is returned by the driver

Open options
-------------

-  .. oo:: NODATA_OR_MASK
      :choices: AUTO, MASK, NONE, numeric nodata value

      Control nodata handling.

      - In AUTO mode, the driver will expose a dataset nodata mask band, unless the
        band data type is Float32 or Float64, in which case a nodata value is used.

      - In MASK mode, the driver will expose a dataset nodata mask band for all
        data types.

      - In NONE mode, the driver will not expose a nodata mask band or a
        nodata value.

      - When specifying a numeric nodata value (``nan`` accepted for Float32 or
        Float64), it is used as the band nodata value. The nodata value should be
        selected outside the range of valid values (but within the range of the
        data type).

Metadata
--------

The ``xml:documentation`` and ``xml:definition`` metadata domains contain the
XML content from the ``GDB_Items`` table related to the raster layer.

Examples
--------

-  List raster layers from a FileGDB

   ::

      gdalinfo /path/to/my.gdb

-  Open a given subdataset:

   ::

      gdalinfo OpenFileGDB:"/path/to/my.gdb":name_of_raster_layer


Links
-----

-  :ref:`OpenFileGDB vector <vector.openfilegdb>` documentation page


Credits
-------

Thanks to Richard Barnes and his ArcRescue tool for the deciphering of
the band_types field which indicates the compression method and the data type.
