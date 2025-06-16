.. _vector.filegdb:

ESRI File Geodatabase (FileGDB)
===============================

.. shortname:: FileGDB

.. build_dependencies:: FileGDB API library

The FileGDB driver provides read and write access to vector layers of
File Geodatabases (.gdb directories) created by ArcGIS 10 and above. The
dataset name must be the directory/folder name, and it must end with the
.gdb extension.

Note : the :ref:`OpenFileGDB driver <vector.openfilegdb>` driver exists as an
alternative built-in (i.e. not depending on a third-party library) driver.

Starting with GDAL 3.11, update and creation support is delegated to the
:ref:`OpenFileGDB driver <vector.openfilegdb>` driver.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

Requirements
------------

`ESRI File Geodatabase API <https://github.com/Esri/file-geodatabase-api>`__

Curve in geometries are supported on reading with GDAL >= 2.2.

SQL support
-----------

SQL statements are run through the SQL engine of
the FileGDB SDK API. This holds for non-SELECT statements. However, due
to partial/inaccurate support for SELECT statements in current FileGDB
SDK API versions (v1.2), SELECT statements will be run by default by the
OGR SQL engine. This can be changed by specifying the *-dialect FileGDB*
option to ogrinfo or ogr2ogr.

Special SQL requests
~~~~~~~~~~~~~~~~~~~~

"GetLayerDefinition a_layer_name" and "GetLayerMetadata a_layer_name"
can be used as special SQL requests to get respectively the definition
and metadata of a FileGDB table as XML content.

Starting with GDAL 3.5, the "REPACK" special SQL request can be issued to
ask for database compaction.

Field domains
-------------

.. versionadded:: 3.3

Retrieving coded and range field domains are supported.
Writing support has been added in GDAL 3.5.

Relationships
-------------

.. versionadded:: 3.6

Relationship retrieval is supported.

Hierarchical organization
-------------------------

.. versionadded:: 3.4

The hierarchical organization of tables and feature classes as top-level
element or within a feature dataset can be explored using the methods
:cpp:func:`GDALDataset::GetRootGroup`,
:cpp:func:`GDALGroup::GetGroupNames`, :cpp:func:`GDALGroup::OpenGroup`,
:cpp:func:`GDALGroup::GetVectorLayerNames` and :cpp:func:`GDALGroup::OpenVectorLayer`

Geometry coordinate precision
-----------------------------

.. versionadded:: GDAL 3.9

The driver supports reading and writing the geometry coordinate
precision, using the XYResolution, ZResolution and MResolution members of
the :cpp:class:`OGRGeomCoordinatePrecision` settings of the
:cpp:class:`OGRGeomFieldDefn`. ``XYScale`` is computed as 1.0 / ``XYResolution``
(and similarly for the Z and M components). The tolerance setting is computed
as being one tenth of the resolution

On reading, the coordinate precision grid parameters are returned as format
specific options of :cpp:class:`OGRGeomCoordinatePrecision` with the
``FileGeodatabase`` format key, with the following option key names:
``XYScale``, ``XYTolerance``, ``XYOrigin``,
``ZScale``, ``ZTolerance``, ``ZOrigin``,
``MScale``, ``MTolerance``, ``MOrigin``. On writing, they are also honored
(they will have precedence over XYResolution, ZResolution and MResolution).

On layer creation, the XORIGIN, YORIGIN, ZORIGIN, MORIGIN, XYSCALE, ZSCALE,
ZORIGIN, XYTOLERANCE, ZTOLERANCE, MTOLERANCE layer creation options will be
used in priority over the settings of :cpp:class:`OGRGeomCoordinatePrecision`.

Limitations
-----------

-  The SDK is known to be unable to open layers with particular spatial
   reference systems. This might be the case if messages "FGDB: Error
   opening XXXXXXX. Skipping it (Invalid function arguments.)" when
   running ``ogrinfo --debug on the.gdb`` (reported as warning in GDAL
   2.0). Using the OpenFileGDB driver will generally solve that issue.

-  FGDB coordinate snapping will cause geometries to be altered during
   writing. Use the origin and scale layer creation options to control
   the snapping behavior.

-  Reading data compressed in SDC format (Smart Data Compression) is not
   support by the driver, because it is not supported by the ESRI SDK.

-  Reading data compressed in CDF format (Compressed Data Format)
   requires ESRI SDK 1.4 or later.

-  Some applications create FileGeodatabases with non-spatial tables which are
   not present in the GDB_Items metadata table. These tables cannot be opened
   by the ESRI SDK, so GDAL will automatically fallback to the OpenFileGDB
   driver to read these tables. Accordingly they will be opened with the
   limitations of the OpenFileGDB driver (for instance, they will be
   read only).

- The driver does not support 64-bit integers.

Links
-----

-  `ESRI File Geodatabase API
   Page <https://github.com/Esri/file-geodatabase-api/>`__
-  :ref:`OpenFileGDB driver <vector.openfilegdb>`, not depending on a
   third-party library/SDK
