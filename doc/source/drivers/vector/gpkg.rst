.. _vector.gpkg:

GPKG -- GeoPackage vector
=========================

.. shortname:: GPKG

.. build_dependencies:: libsqlite3

This driver implements support for access to spatial tables in the `OGC
GeoPackage format
standard <http://www.opengeospatial.org/standards/geopackage>`__ The
GeoPackage standard uses a SQLite database file as a generic container,
and the standard defines:

-  Expected metadata tables (``gpkg_contents``,
   ``gpkg_spatial_ref_sys``, ``gpkg_geometry_columns``)
-  Binary format encoding for geometries in spatial tables (basically a
   GPKG standard header object followed by ISO standard well-known
   binary (WKB))
-  Naming and conventions for extensions (extended feature types) and
   indexes (how to use SQLite r-tree in an interoperable manner)

This driver reads and writes SQLite files from the file system, so it
must be run by a user with read/write access to the files it is working
with.

The driver also supports reading and writing the
following non-linear geometry types: CIRCULARSTRING, COMPOUNDCURVE,
CURVEPOLYGON, MULTICURVE and MULTISURFACE

GeoPackage raster/tiles are supported. See the 
:ref:`GeoPackage raster <raster.gpkg>` documentation page.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Specification version
---------------------

Starting with GDAL 2.2, the driver is able to create GeoPackage
databases following the 1.0/1.0.1, 1.1 or 1.2 versions. For GDAL 2.2, it
will automatically adjust to the minimum version required for the
features of GeoPackage used. For GDAL 2.3 or later, it will default to
1.2. Explicit version choice can be done by specifying the VERSION
dataset creation option.

Limitations
-----------

-  GeoPackage only supports one geometry column per table.

SQL
---

The driver supports OGR attribute filters, and users are expected to
provide filters in the SQLite dialect, as they will be executed directly
against the database.

SQL SELECT statements passed to ExecuteSQL() are
also executed directly against the database. If Spatialite is used, a
recent version (4.2.0) is needed and use of explicit cast operators
AsGPB() is required to transform GeoPackage geometries to Spatialite
geometries (the reverse conversion from Spatialite geometries is
automatically done by the GPKG driver). It is also possible to use with
any Spatialite version, but in a slower way, by specifying the
"INDIRECT_SQLITE" dialect. In which case, GeoPackage geometries
automatically appear as Spatialite geometries after translation by OGR.

Starting with GDAL 2.2, the "DROP TABLE layer_name" and "ALTER TABLE
layer_name RENAME TO new_layer" statements can be used. They will update
GeoPackage system tables.

Starting with GDAL 2.2, the
"HasSpatialIndex('table_name','geom_col_name')" statement can be used
for checking if the table has spatial index on the named geometry
column.

When dropping a table, or removing records from tables, the space they
occupied is not immediately released and kept in the pool of file pages
that SQLite may reuse later. If you need to shrink the file to its
minimum size, you need to issue an explicit "VACUUM" SQL request. Note
that this will result in a full rewrite of the file.

SQL functions
~~~~~~~~~~~~~

The following SQL functions, from the GeoPackage specification, are available:

-  ST_MinX(geom *Geometry*): returns the minimum X coordinate of the
   geometry
-  ST_MinY(geom *Geometry*): returns the minimum Y coordinate of the
   geometry
-  ST_MaxX(geom *Geometry*): returns the maximum X coordinate of the
   geometry
-  ST_MaxY(geom *Geometry*): returns the maximum Y coordinate of the
   geometry
-  ST_IsEmpty(geom *Geometry*): returns 1 if the geometry is empty (but
   not null), e.g. a POINT EMPTY geometry
-  ST_GeometryType(geom *Geometry*): returns the geometry type:
   'POINT', 'LINESTRING', 'POLYGON', 'MULTIPOLYGON', 'MULTILINESTRING',
   'MULTIPOINT', 'GEOMETRYCOLLECTION'
-  ST_SRID(geom *Geometry*): returns the SRID of the geometry
-  GPKG_IsAssignable(expected_geom_type *String*, actual_geom_type
   *String*) : mainly, needed for the 'Geometry Type Triggers Extension'

The following functions, with identical syntax and semantics as in
Spatialite, are also available :

-  CreateSpatialIndex(table_name *String*, geom_column_name *String*):
   creates a spatial index (RTree) on the specified table/geometry
   column
-  DisableSpatialIndex(table_name *String*, geom_column_name *String*):
   drops an existing spatial index (RTree) on the specified
   table/geometry column
-  ST_Transform(geom *Geometry*, target_srs_id *Integer*): reproject the geometry
   to the SRS of specified srs_id. If no SRS with that given srs_id is not found
   in gpkg_spatial_ref_sys, starting with GDAL 3.2, it will be interpreted as
   a EPSG code.

Link with Spatialite
~~~~~~~~~~~~~~~~~~~~

If it has been compiled against Spatialite 4.2
or later, it is also possible to use Spatialite SQL functions. Explicit
transformation from GPKG geometry binary encoding to Spatialite geometry
binary encoding must be done.

::

   ogrinfo poly.gpkg -sql "SELECT ST_Buffer(CastAutomagic(geom),5) FROM poly"

Starting with Spatialite 4.3, CastAutomagic is no longer needed.

Transaction support
-------------------

The driver implements transactions at the database level, per :ref:`rfc-54`

Dataset open options
--------------------

The following open options are available:

-  **LIST_ALL_TABLES**\ =AUTO/YES/NO: (GDAL >=2.2) Whether all tables,
   including those not listed in gpkg_contents, should be listed.
   Defaults to AUTO. If AUTO, all tables including those not listed in
   gpkg_contents will be listed, except if the aspatial extension is
   found or a table is registered as 'attributes' in gpkg_contents. If
   YES, all tables including those not listed in gpkg_contents will be
   listed, in all cases. If NO, only tables registered as 'features',
   'attributes' or 'aspatial' will be listed.
-  **PRELUDE_STATEMENTS**\ =string (GDAL >= 3.2). SQL statement(s) to
   send on the SQLite3 connection before any other ones. In
   case of several statements, they must be separated with the
   semi-column (;) sign. This option may be useful to
   `attach another database <https://www.sqlite.org/lang_attach.html>`__
   to the current one and issue cross-database requests.

   .. note::
        The attached database must be a GeoPackage one too, so
        that its geometry blobs are properly recognized (so typically not a Spatialite one)

-  **NOLOCK**\= YES/NO (GDAL >= 3.4.2). Defaults is NO.
   Whether the database should be used without doing any file locking. Setting
   it to YES will only be honoured when opening in read-only mode and if the
   journal mode is not WAL.
   This corresponds to the nolock=1 query parameter described at
   https://www.sqlite.org/uri.html

Note: open options are typically specified with "-oo name=value" syntax
in most OGR utilities, or with the ``GDALOpenEx()`` API call.

Note: configuration option :decl_configoption:`OGR_SQLITE_JOURNAL` can
be used to set the journal mode of the GeoPackage (and thus SQLite)
file, see also https://www.sqlite.org/pragma.html#pragma_journal_mode.

Creation issues
---------------

When creating a new GeoPackage file, the driver will attempt to force
the database into a UTF-8 mode for text handling, satisfying the OGR
strict UTF-8 capability. For pre-existing files, the driver will work
with whatever it is given.

The driver updates the GeoPackage ``last_change`` timestamp when the file is
created or modified. If consistent binary output is required for
reproducibility, the timestamp can be forced to a specific value by setting the
:decl_configoption:`OGR_CURRENT_DATE` global configuration option.
When setting the option, take care to meet the specific time format
requirement of the GeoPackage standard,
e.g. `for version 1.2 <https://www.geopackage.org/spec120/#r15>`__.

Dataset creation options
~~~~~~~~~~~~~~~~~~~~~~~~

The following creation options (specific to vector, or common with
raster) are available:

-  **VERSION**\ =AUTO/1.0/1.1/1.2/1.3: (GDAL >= 2.2) Set GeoPackage version
   (for application_id and user_version fields). In AUTO mode, this will
   be equivalent to 1.2 starting with GDAL 2.3.
   1.3 is available starting with GDAL 3.3
-  **ADD_GPKG_OGR_CONTENTS**\ =YES/NO: (GDAL >= 2.2) Defines whether to
   add a gpkg_ogr_contents table to keep feature count, and associated
   triggers. Defaults to YES.
-  **DATETIME_FORMAT**\ =WITH_TZ/UTC: (GDAL >= 3.2) Defines whether to keep the
   DateTime values in the time zones as used in the data source (WITH_TZ),
   or to convert the date and time expressions to UTC (Coordinated Universal Time).
   Defaults to WITH_TZ. Pedantically, non-UTC time zones are not currently supported
   by GeoPackage v1.3 (see https://github.com/opengeospatial/geopackage/issues/530).
   When using UTC format, with a unspecified timezone, UTC will be assumed.

Other options are available for raster. See the :ref:`GeoPackage raster <raster.gpkg>`
documentation page.

Layer creation options
~~~~~~~~~~~~~~~~~~~~~~

The following layer creation options are available:

-  **GEOMETRY_NAME**: Column to use for the geometry column. Default to
   "geom". Note: This option was called GEOMETRY_COLUMN in releases before
   GDAL 2
-  **GEOMETRY_NULLABLE**: Whether the values of the
   geometry column can be NULL. Can be set to NO so that geometry is
   required. Default to "YES"
-  **FID**: Column name to use for the OGR FID (primary key in the
   SQLite database). Default to "fid"
-  **OVERWRITE**: If set to "YES" will delete any existing layers that
   have the same name as the layer being created. Default to NO
-  **SPATIAL_INDEX**: If set to "YES" will create a spatial
   index for this layer. Default to YES
-  **PRECISION**: This may be "YES" to force new fields
   created on this layer to try and represent the width of text fields
   (in terms of UTF-8 characters, not bytes), if available using
   TEXT(width) types. If "NO" then the type TEXT will be used instead.
   The default is "YES".
-  **TRUNCATE_FIELDS**: This may be "YES" to force
   truncated of field values that exceed the maximum allowed width of
   text fields, and also to "fix" the passed string if needed to make it
   a valid UTF-8 string. If "NO" then the value is not truncated nor
   modified. The default is "NO".
-  **IDENTIFIER**\ =string: Identifier of the layer, as put
   in the contents table.
-  **DESCRIPTION**\ =string: Description of the layer, as
   put in the contents table.
-  **ASPATIAL_VARIANT**\ =GPKG_ATTRIBUTES/NOT_REGISTERED:
   (GDAL >=2.2) How to register non spatial tables. Defaults to
   GPKG_ATTRIBUTES in GDAL 2.2 or later (behavior in previous version
   was equivalent to OGR_ASPATIAL). Starting with GeoPackage 1.2, non
   spatial tables are part of the specification. They are recorded with
   data_type="attributes" in the gpkg_contents table. This is only
   compatible of GDAL 2.2 or later.
   It is also possible to use the NOT_REGISTERED
   option, in which case the non spatial table is not registered at all
   in any GeoPackage system tables.
   Priorly, in OGR 2.0 and 2.1, the "aspatial" extension had been developed for
   similar purposes, so if selecting OGR_ASPATIAL, non spatial tables will be
   recorded with data_type="aspatial" and the "aspatial" extension was declared in the
   gpkg_extensions table. Starting with GDAL 3.3, OGR_ASPATIAL is no longer
   available on creation.

Configuration options
---------------------

The following :ref:`configuration options <configoptions>` are 
available:

- :decl_configoption:`OGR_SQLITE_JOURNAL` can be used to set the journal mode 
  of the GeoPackage (and thus SQLite) file, see also 
  https://www.sqlite.org/pragma.html#pragma_journal_mode.

- :decl_configoption:`OGR_SQLITE_CACHE`: see 
  :ref:`Performance hints <target_drivers_vector_gpkg_performance_hints>`.
  
- :decl_configoption:`OGR_SQLITE_SYNCHRONOUS`: see 
  :ref:`Performance hints <target_drivers_vector_gpkg_performance_hints>`.

- :decl_configoption:`OGR_SQLITE_LOAD_EXTENSIONS` =extension1,...,extensionN,ENABLE_SQL_LOAD_EXTENSION:
  (GDAL >= 3.5.0). Comma separated list of names of shared libraries containing
  extensions to load at database opening.
  If a file cannot be loaded directly, attempts are made to load with various
  operating-system specific extensions added. So
  for example, if "samplelib" cannot be loaded, then names like "samplelib.so"
  or "samplelib.dylib" or "samplelib.dll" might be tried also.
  The special value ``ENABLE_SQL_LOAD_EXTENSION`` can be used to enable the use of
  the SQL ``load_extension()`` function, which is normally disabled in standard
  builds of sqlite3.
  Loading extensions as a potential security impact if they are untrusted.

- :decl_configoption:`OGR_SQLITE_PRAGMA` can be used to specify any SQLite
  `pragma <http://www.sqlite.org/pragma.html>`__ . The syntax is
  ``OGR_SQLITE_PRAGMA = "pragma_name=pragma_value[,pragma_name2=pragma_value2]*"``.

- :decl_configoption:`OGR_CURRENT_DATE`: the driver updates the GeoPackage 
  ``last_change`` timestamp when the file is created or modified. If consistent 
  binary output is required for reproducibility, the timestamp can be forced to 
  a specific value by setting this global configuration option.
  When setting the option, take care to meet the specific time format
  requirement of the GeoPackage standard,
  e.g. `for version 1.2 <https://www.geopackage.org/spec120/#r15>`__.

- :decl_configoption:`SQLITE_USE_OGR_VFS` =YES enables extra buffering/caching 
  by the GDAL/OGR I/O layer and can speed up I/O. More information 
  :ref:`here <target_user_virtual_file_systems_file_caching>`.
  Be aware that no file locking will occur if this option is activated, so 
  concurrent edits may lead to database corruption.

Metadata
--------

GDAL uses the standardized
`gpkg_metadata <http://www.geopackage.org/spec/#_metadata_table>`__
and
`gpkg_metadata_reference <http://www.geopackage.org/spec/#_metadata_reference_table>`__
tables to read and write metadata, on the dataset and layer objects.

GDAL metadata, from the default metadata domain and possibly other
metadata domains, is serialized in a single XML document, conformant
with the format used in GDAL PAM (Persistent Auxiliary Metadata)
.aux.xml files, and registered with md_scope=dataset and
md_standard_uri=http://gdal.org in gpkg_metadata. For the dataset, this
entry is referenced in gpkg_metadata_reference with a
reference_scope=geopackage. For a layer, this entry is referenced in
gpkg_metadata_reference with a reference_scope=table and
table_name={name of the table}

Metadata not originating from GDAL can be read by the driver and will be
exposed as metadata items with keys of the form GPKG_METADATA_ITEM_XXX
and values the content of the *metadata* columns of the gpkg_metadata
table. Update of such metadata is not currently supported through GDAL
interfaces ( although it can be through direct SQL commands).

The specific DESCRIPTION and IDENTIFIER metadata item of the default
metadata domain can be used in read/write to read from/update the
corresponding columns of the gpkg_contents table.

Non-spatial tables
------------------

The core GeoPackage specification of GeoPackage 1.0 and 1.1 did not
support non-spatial tables. This was added in GeoPackage 1.2 as the
"attributes" data type.

The driver allows creating and reading non-spatial tables with the :ref:`vector.geopackage_aspatial`.

Starting with GDAL 2.2, the driver will also, by default, list non
spatial tables that are not registered through the gdal_aspatial
extension, and support the GeoPackage 1.2 "attributes" data type as
well. Starting with GDAL 2.2, non spatial tables are by default created
following the GeoPackage 1.2 "attributes" data type (can be controlled
with the ASPATIAL_VARIANT layer creation option).

Spatial views
-------------

Views can be created and recognized as valid spatial layers if a
corresponding record is inserted into the gpkg_contents and
gpkg_geometry_columns table.

Starting with GDAL 2.2, in the case of the columns in the SELECT clause
of the view acts a integer primary key, then it can be recognized by OGR
as the FID column of the view, provided it is renamed as OGC_FID.
Selecting a feature id from a source table without renaming will not be
sufficient, since due to joins this feature id could appear several
times. Thus the user must explicitly acknowledge that the column is
really a primary key.

For example:

::

   CREATE VIEW my_view AS SELECT foo.fid AS OGC_FID, foo.geom, ... FROM foo JOIN another_table ON foo.some_id = another_table.other_id
   INSERT INTO gpkg_contents (table_name, identifier, data_type, srs_id) VALUES ( 'my_view', 'my_view', 'features', 4326)
   INSERT INTO gpkg_geometry_columns (table_name, column_name, geometry_type_name, srs_id, z, m) values ('my_view', 'my_geom', 'GEOMETRY', 4326, 0, 0)

This requires GDAL to be compiled with the SQLITE_HAS_COLUMN_METADATA
option and SQLite3 with the SQLITE_ENABLE_COLUMN_METADATA option.
Starting with GDAL 2.3, this can be easily verified if the
SQLITE_HAS_COLUMN_METADATA=YES driver metadata item is declared (for
example with "ogrinfo --format GPKG").

Coordinate Reference Systems
----------------------------

Valid geographic, projected and compound CRS supported in general by GDAL are
also supported by GeoPackage and stored in the ``gpkg_spatial_ref_sys`` table.

Two special hard-coded CRS are reserved per the GeoPackage specification:

- SRID 0, for a Undefined Geographic CRS. This one is selected by default if
  creating a spatial layer without any explicit CRS

- SRID -1, for a Undefined Projected CRS. It might be selected by creating a
  layer with a CRS instantiated from the following WKT string:
  ``LOCAL_CS["Undefined cartesian SRS"]``. (GDAL >= 3.3)

Level of support of GeoPackage Extensions
-----------------------------------------

(Restricted to those that have a vector scope)

.. list-table:: Extensions
   :header-rows: 1

   * - Extension name
     - OGC adopted extension ?
     - Supported by GDAL?
   * - `Non-Linear Geometry Types <http://www.geopackage.org/guidance/extensions/nonlinear_geometry_types.html>`__
     - Yes
     - Yes, since GDAL 2.1
   * - `RTree Spatial Indexes <http://www.geopackage.org/guidance/extensions/rtree_spatial_indexes.html>`__
     - Yes
     - Yes
   * - `Metadata <http://www.geopackage.org/guidance/extensions/metadata.html>`__
     - Yes
     - Yes
   * - `Schema <http://www.geopackage.org/guidance/extensions/schema.html>`__
     - Yes
     - Yes, since GDAL 3.3 (Geopackage constraints exposed as field domains)
   * - `WKT for Coordinate Reference Systems <http://www.geopackage.org/guidance/extensions/wkt_for_crs.md>`__ (WKT v2)
     - Yes
     -  Partially, since GDAL 2.2. GDAL can read databases using this extension, but cannot interpret a SRS entry that has only a WKT v2 entry.
   * - :ref:`vector.geopackage_aspatial`
     - No
     - Yes. Deprecated in GDAL 2.2 for the *attributes* official data_type

.. _target_drivers_vector_gpkg_performance_hints:

Performance hints
-----------------

The same performance hints apply as those mentioned for the 
:ref:`SQLite driver <target_drivers_vector_sqlite_performance_hints>`.

Examples
--------

-  Simple translation of a single shapefile into GeoPackage. The table
   'abc' will be created with the features from abc.shp and attributes
   from abc.dbf. The file ``filename.gpkg`` must **not** already exist,
   as it will be created. For adding new layers into existing geopackage
   run ogr2ogr with **-update**.

   ::

      ogr2ogr -f GPKG filename.gpkg abc.shp

-  Translation of a directory of shapefiles into a GeoPackage. Each file
   will end up as a new table within the GPKG file. The file
   ``filename.gpkg`` must **not** already exist, as it will be created.

   ::

      ogr2ogr -f GPKG filename.gpkg ./path/to/dir

-  Translation of a PostGIS database into a GeoPackage. Each table in
   the database will end up as a table in the GPKG file. The file
   ``filename.gpkg`` must **not** already exist, as it will be created.

   ::

      ogr2ogr -f GPKG filename.gpkg PG:'dbname=mydatabase host=localhost'

- Perform a join between 2 GeoPackage databases:

    ::

      ogrinfo my_spatial.gpkg \
        -sql "SELECT poly.id, other.foo FROM poly JOIN other_schema.other USING (id)" \
        -oo PRELUDE_STATEMENTS="ATTACH DATABASE 'other.gpkg' AS other_schema"

See Also
--------

-  :ref:`GeoPackage raster <raster.gpkg>` documentation page
-  `Getting Started With
   GeoPackage <http://www.geopackage.org/guidance/getting-started.html>`__
-  `OGC GeoPackage format standard <http://www.geopackage.org/spec/>`__
   specification, HTML format (current/development version of the
   standard)
-  `OGC GeoPackage Encoding
   Standard <http://www.opengeospatial.org/standards/geopackage>`__ page
-  `SQLite <http://sqlite.org/>`__

.. toctree::
   :hidden:

   geopackage_aspatial
