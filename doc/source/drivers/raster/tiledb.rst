.. _raster.tiledb:

================================================================================
TileDB - TileDB raster
================================================================================

.. shortname:: TileDB

.. versionadded:: 3.0

.. build_dependencies:: TileDB (>= 2.7 starting with GDAL 3.7)

GDAL can read and write TileDB arrays through the TileDB library.

The driver relies on the Open Source TileDB
`library <https://github.com/TileDB-Inc/TileDB>`__ (MIT licensed).

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Open options
------------

The following open options exist:

-  .. oo:: TILEDB_CONFIG
      :choices: <filename>

      A local file with TileDB configuration
      `options <https://docs.tiledb.io/en/stable/tutorials/config.html>`__

-  .. oo:: TILEDB_TIMESTAMP
      :choices: <integer>

      Open array at this timestamp. Should be strictly greater than zero when set.

-  .. oo:: STATS
      :choices: YES, NO
      :default: NO

      Whether TileDB `performance statistics <https://docs.tiledb.com/main/how-to/performance/using-performance-statistics>`__
      should be displayed.

-  .. oo:: TILEDB_ATTRIBUTE
      :choices: <string>

      Attribute to read from each band

Creation options
----------------

The following creation options exist:

-  .. co:: TILEDB_CONFIG
      :choices: <filename>

      A local file with TileDB configuration
      `options <https://docs.tiledb.io/en/stable/tutorials/config.html>`__

-  .. co:: COMPRESSION
      :choices: NONE, GZIP, ZSTD, LZ4, RLE, BZIP2, DOUBLE-DELTA, POSITIVE-DELTA

      Compression to use. Default is NONE

-  .. co:: COMPRESSION_LEVEL
      :choices: <integer>

      Compression level

-  .. co:: BLOCKXSIZE
      :choices: <integer>
      :default: 256

      Tile width.

-  .. co:: BLOCKYSIZE
      :choices: <integer>
      :default: 256

      Tile height

-  .. co:: STATS
      :choices: YES, NO
      :default: NO

      Whether TileDB `performance statistics <https://docs.tiledb.com/main/how-to/performance/using-performance-statistics>`__
      should be displayed.

-  .. co:: TILEDB_ATTRIBUTE
      :choices: <string>

      Co-registered file to add as TileDB attributes. Only applicable for interleave types of band or pixel

-  .. co:: INTERLEAVE
      :choices: BAND, PIXEL, ATTRIBUTES
      :default: BAND

      Indexing order. Influences how multi-band rasters are stored.

      * ``BAND``: a 3D array is created with the slowest varying dimension being the band.
      * ``PIXEL``: a 3D array is created with the fastest varying dimension being the band.
      * ``ATTRIBUTES``: a 2D array is created with each band being stored in a separate TileDB attribute.

-  .. co:: TILEDB_TIMESTAMP
      :choices: <integer>

      Create array at this timestamp. Should be strictly greater than zero when set.

-  .. co:: CREATE_GROUP
      :choices: YES, NO
      :default: YES since 3.10 (NO previously)
      :since: 3.10

      Whether the dataset should be created within a TileDB group.

      When the dataset is created within a TileDB group, overviews that may be
      created are stored as TileDB arrays inside that group, next to the full
      resolution array. This makes administration of the dataset easier.

      Otherwise, the past default behavior (CREATE_GROUP=NO) is to create the dataset
      as a TileDB array. Overviews cannot be created in that mode.


Multidimensional API support
----------------------------

.. versionadded:: 3.8

The TileDB driver supports the :ref:`multidim_raster_data_model` for reading
and writing operations. It requires GDAL to be built and run against TileDB >= 2.15.

The driver supports:
- creating and reading groups and subgroups
- creating and reading multidimensional dense arrays with a numeric data type
- creating and reading numeric or string attributes in groups and arrays
- storing an indexing array of a dimension as a TileDB dimension label

The multidimensional API supports reading dense arrays created by the classic
raster API of GDAL.

The following multidimensional dataset open options are available:

-  **TILEDB_CONFIG=config**: A local file with TileDB configuration
   `options <https://docs.tiledb.io/en/stable/tutorials/config.html>`__

-  **TILEDB_TIMESTAMP=val**: inclusive ending timestamp when opening this array


The following multidimensional dataset creation options are available:

-  **TILEDB_CONFIG=config**: A local file with TileDB configuration
   `options <https://docs.tiledb.io/en/stable/tutorials/config.html>`__

-  **TILEDB_TIMESTAMP=val**: Create array at this timestamp. Should be strictly greater than zero when set.


The following array open options are available:

-  **TILEDB_TIMESTAMP=val**: inclusive ending timestamp when opening this array


The following array creation options are available:

-  **BLOCKSIZE=val1,val2,...,valN**: Block size in pixels

-  **COMPRESSION=NONE/GZIP/ZSTD/LZ4/RLE/BZIP2/DOUBLE-DELTA/POSITIVE-DELTA**:
   Compression to use. Default is NONE

-  **COMPRESSION_LEVEL=int_value**: compression level

-  **IN_MEMORY=YES/NO**: hether the array should be only in-memory. Useful to
   create an indexing variable that is serialized as a dimension label

-  **TILEDB_TIMESTAMP=val**: Create array at this timestamp. Should be strictly greater than zero when set.


Cf :source_file:`autotest/gdrivers/tiledb_multidim.py` for examples of how to
use the Python multidimensional API with the TileDB driver.

Standalone plugin compilation
-----------------------------

.. versionadded:: 3.10

While this driver may be built as part of a whole GDAL build, either in libgdal
itself, or as a plugin, it is also possible to only build this driver as a plugin,
against an already built libgdal.

The version of the GDAL sources used to build the driver must match the version
of the libgdal it is built against.

For example, from a "build_tiledb" directory under the root of the GDAL source tree:

::

    cmake -S ../frmts/tiledb -DCMAKE_PREFIX_PATH=/path/to/GDAL_installation_prefix -DTileDB_DIR:PATH=/path/to/tiledb_installation_prefix/lib/cmake/TileDB
    cmake --build .


Note that such a plugin, when used against a libgdal not aware of it, will be
systematically loaded at GDAL driver initialization time, and will not benefit from
`deferred plugin loading capabilities <rfc-96>`. For that, libgdal itself must be built with the
CMake variable GDAL_REGISTER_DRIVER_TILEDB_FOR_LATER_PLUGIN=ON set.

See Also
--------

- `TileDB home page <https://tiledb.io/>`__

- :ref:`TileDB vector <vector.tiledb>` documentation page
