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

Creation options
----------------

Various creation and open options exists, among them :

-  .. co:: TILEDB_CONFIG
      :choices: <filename>

      A local file with TileDB configuration
      `options <https://docs.tiledb.io/en/stable/tutorials/config.html>`__

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

-  **TILEDB_TIMESTAMP=val**: inclusive ending timestamp when opening this array


The following array open options are available:

-  **TILEDB_TIMESTAMP=val**: inclusive ending timestamp when opening this array


The following array creation options are available:

-  **BLOCKSIZE=val1,val2,...,valN**: Block size in pixels

-  **COMPRESSION=NONE/GZIP/ZSTD/LZ4/RLE/BZIP2/DOUBLE-DELTA/POSITIVE-DELTA**:
   Compression to use. Default is NONE

-  **COMPRESSION_LEVEL=int_value**: compression level

-  **IN_MEMORY=YES/NO**: hether the array should be only in-memory. Useful to
   create an indexing variable that is serialized as a dimension label

-  **TILEDB_TIMESTAMP=val**: inclusive ending timestamp when opening this array


Cf :source_file:`autotest/gdrivers/tiledb_multidim.py` for examples of how to
use the Python multidimensional API with the TileDB driver.

See Also
--------

- `TileDB home page <https://tiledb.io/>`__

- :ref:`TileDB vector <vector.tiledb>` documentation page
