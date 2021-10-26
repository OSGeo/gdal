.. _vector.openfilegdb:

ESRI File Geodatabase (OpenFileGDB)
===================================

.. shortname:: OpenFileGDB

.. built_in_by_default::

The OpenFileGDB driver provides read access to vector layers of File
Geodatabases (.gdb directories) created by ArcGIS 9 and above. The
dataset name must be the directory/folder name, and it must end with the
.gdb extension.

It can also read directly zipped .gdb directories (with .gdb.zip
extension), provided they contain a .gdb directory at their first level.

A specific .gdbtable file (including "system" tables) can also be opened
directly.

Curve in geometries are supported with GDAL >= 2.2.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Spatial filtering
-----------------

Since GDAL 3.2, the driver can use the native .spx spatial indices for
spatial filtering.

In earlier versions, it uses the minimum bounding rectangle included
at the beginning of the geometry blobs to speed up spatial filtering. By
default, it also builds on the fly a in-memory spatial index during
the first sequential read of a layer. Following spatial filtering
operations on that layer will then benefit from that spatial index. The
building of this in-memory spatial index can be disabled by setting the
OPENFILEGDB_IN_MEMORY_SPI configuration option to NO.

SQL support
-----------

SQL statements are run through the OGR SQL engine. When attribute
indexes (.atx files) exist, the driver will use them to speed up WHERE
clauses or SetAttributeFilter() calls.

Special SQL requests
~~~~~~~~~~~~~~~~~~~~

"GetLayerDefinition a_layer_name" and "GetLayerMetadata a_layer_name"
can be used as special SQL requests to get respectively the definition
and metadata of a FileGDB table as XML content (only available in
Geodatabases created with ArcGIS 10 or above)

Dataset open options
--------------------

-  **LIST_ALL_TABLES**\ =YES/NO: This may be "YES" to force all tables,
   including system and internal tables (such as the GDB_* tables) to be listed (since GDAL 3.4)

Field domains
-------------

.. versionadded:: 3.3

Coded and range field domains are supported.

Hiearchical organization
------------------------

.. versionadded:: 3.4

The hiearchical organization of tables and feature classes as top-level
element or within a feature dataset can be explored using the methods
:cpp:func:`GDALDataset::GetRootGroup`,
:cpp:func:`GDALGroup::GetGroupNames`, :cpp:func:`GDALGroup::OpenGroup`,
:cpp:func:`GDALGroup::GetVectorLayerNames` and :cpp:func:`GDALGroup::OpenVectorLayer`

Comparison with the FileGDB driver
----------------------------------

(Comparison done with a FileGDB driver using FileGDB API SDK 1.4)

Advantages of the OpenFileGDB driver:

-  Can read ArcGIS 9.X Geodatabases, and not only 10 or above.
-  Can open layers with any spatial reference system.
-  Thread-safe (i.e. datasources can be processed in parallel).
-  Uses the VSI Virtual File API, enabling the user to read a
   Geodatabase in a ZIP file or stored on a HTTP server.
-  Faster on databases with a big number of fields.
-  Does not depend on a third-party library.
-  Robust against corrupted Geodatabase files.

Drawbacks of the OpenFileGDB driver:

-  Read-only.
-  Cannot read data from compressed data in CDF format (Compressed Data
   Format).

Examples
--------

-  Read layer from FileGDB and load into PostGIS:

   ::

      ogr2ogr -overwrite -f "PostgreSQL" PG:"host=myhost user=myuser dbname=mydb password=mypass" "C:\somefolder\BigFileGDB.gdb" "MyFeatureClass"

-  Get detailed info for FileGDB:

   ::

      ogrinfo -al "C:\somefolder\MyGDB.gdb"

-  Get detailed info for a zipped FileGDB:

   ::

      ogrinfo -al "C:\somefolder\MyGDB.gdb.zip"

Links
-----

-  :ref:`FileGDB driver <vector.filegdb>`, relying on the FileGDB API SDK
-  Reverse-engineered specification of the `FileGDB
   format <https://github.com/rouault/dump_gdbtable/wiki/FGDB-Spec>`__
