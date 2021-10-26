.. _raster.tiledb:

================================================================================
TileDB - TileDB
================================================================================

.. shortname:: TileDB

.. versionadded:: 3.0

.. build_dependencies:: TileDB

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

-  **TILEDB_CONFIG=config**: A local file with TileDB configuration
   `options <https://docs.tiledb.io/en/stable/tutorials/config.html>`__

See Also
--------

-  `TileDB home page <https://tiledb.io/>`__
