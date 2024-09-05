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
-  ST_Area(geom *Geometry*): compute the area in square units of the geometry SRS.
-  ST_Area(geom *Geometry*, use_ellipsoid *boolean*): (GDAL >= 3.9): compute
   the area in square meters, considering the geometry on the ellipsoid
   (use_ellipsoid must be set to true/1).
-  ST_Length(geom *Geometry*): (GDAL >= 3.10): compute the area in units of the geometry SRS.
-  ST_Length(geom *Geometry*, use_ellipsoid *boolean*): (GDAL >= 3.10): compute
   the area in meters, considering the geometry on the ellipsoid
   (use_ellipsoid must be set to true/1).
-  SetSRID(geom *Geometry*, srs_id *Integer*): overrides the geometry' SRS ID,
   without reprojection.
-  ST_Transform(geom *Geometry*, target_srs_id *Integer*): reproject the geometry
   to the SRS of specified srs_id. If no SRS with that given srs_id is not found
   in gpkg_spatial_ref_sys, starting with GDAL 3.2, it will be interpreted as
   a EPSG code.
-  ST_EnvIntersects(geom *Geometry*, minx *Double*, miny *Double*, maxx *Double*, maxy *Double*):
   (GDAL >= 3.7) Returns 1 if the minimum bounding box of geom intersects the
   bounding box defined by (minx,miny)-(maxx,maxy), or 0 otherwise.
-  ST_EnvIntersects(geom1 *Geometry*, geom2 *Geometry*):
   (GDAL >= 3.7) Returns 1 if the minimum bounding box of geom1 intersects the
   one of geom2, or 0 otherwise. (Note: this function, as all others, does not
   automatically uses spatial indices)

The raster SQL functions mentioned at :ref:`raster.gpkg.raster`
are also available.

Link with Spatialite
~~~~~~~~~~~~~~~~~~~~

If it has been compiled against Spatialite 4.2
or later, it is also possible to use Spatialite SQL functions. Explicit
transformation from GPKG geometry binary encoding to Spatialite geometry
binary encoding must be done.

::

   ogrinfo poly.gpkg -sql "SELECT ST_Buffer(CastAutomagic(geom),5) FROM poly"

Starting with Spatialite 4.3, CastAutomagic is no longer needed.

Note that due to the loose typing mechanism of SQLite, if a geometry expression
returns a NULL value for the first row, this will generally cause OGR not to
recognize the column as a geometry column. It might be then useful to sort
the results by making sure that non-null geometries are returned first:

::

   ogrinfo poly.gpkg -sql "SELECT * FROM (SELECT ST_Buffer(geom,5) AS geom, * FROM poly) ORDER BY geom IS NULL ASC"


Transaction support
-------------------

The driver implements transactions at the database level, per :ref:`rfc-54`

Relationships
-------------

.. versionadded:: 3.6

Many-to-many relationship retrieval is supported, respecting the OGC GeoPackage Related Tables Extension.
One-to-many relationships will also be reported for tables which utilize FOREIGN KEY constraints.

Relationship creation, deletion and updating is supported since GDAL 3.7. Relationships can
only be updated to change their base or related table fields, or the relationship related
table type. It is not permissible to change the base or related table itself, or the mapping
table details. If this is desired then a new relationship should be created instead.

Note that when a many-to-many relationship is created in a GeoPackage, GDAL will always
insert the mapping table into the gpkg_contents table. Formally this is not required
by the Related Tables Extension (instead, the table should only be listed in gpkgext_relations),
however failing to list the mapping table in gpkg_contents prevents it from being usable
in some other applications (e.g. ESRI software).

Dataset open options
--------------------

|about-open-options|
The following open options are available:

-  .. oo:: LIST_ALL_TABLES
      :choices: AUTO, YES, NO
      :default: AUTO
      :since: 2.2

      Whether all tables,
      including those not listed in gpkg_contents, should be listed.
      If AUTO, all tables including those not listed in
      gpkg_contents will be listed, except if the aspatial extension is
      found or a table is registered as 'attributes' in gpkg_contents. If
      YES, all tables including those not listed in gpkg_contents will be
      listed, in all cases. If NO, only tables registered as 'features',
      'attributes' or 'aspatial' will be listed.

-  .. oo:: PRELUDE_STATEMENTS
      :since: 3.2

      SQL statement(s) to
      send on the SQLite3 connection before any other ones. In
      case of several statements, they must be separated with the
      semi-column (;) sign. This option may be useful to
      `attach another database <https://www.sqlite.org/lang_attach.html>`__
      to the current one and issue cross-database requests.

      .. note::
           The attached database must be a GeoPackage one too, so
           that its geometry blobs are properly recognized (so typically not a Spatialite one)

-  .. oo:: NOLOCK
      :choices: YES, NO
      :default: NO
      :since: 3.4.2

      Whether the database should be used without doing any file locking. Setting
      it to YES will only be honoured when opening in read-only mode and if the
      journal mode is not WAL.
      This corresponds to the nolock=1 query parameter described at
      https://www.sqlite.org/uri.html

-  .. oo:: IMMUTABLE
      :choices: YES, NO
      :since: 3.5.3

      Whether the database should be opened by assuming that the file cannot be
      modified by another process. This will skip any checks for change detection.
      This can be useful for WAL enabled files on read-only storage.
      GDAL will automatically try to turn it on when not being able to open
      in read-only mode a WAL enabled file.
      This corresponds to the immutable=1 query parameter described at
      https://www.sqlite.org/uri.html

Note: open options are typically specified with "-oo name=value" syntax
in most OGR utilities, or with the ``GDALOpenEx()`` API call.

Note: configuration option :config:`OGR_SQLITE_JOURNAL` can
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
:config:`OGR_CURRENT_DATE` global configuration option.
When setting the option, take care to meet the specific time format
requirement of the GeoPackage standard,
e.g. `for version 1.2 <https://www.geopackage.org/spec120/#r15>`__.

Dataset creation options
~~~~~~~~~~~~~~~~~~~~~~~~

|about-dataset-creation-options|
The following creation options (specific to vector, or common with
raster) are available:

-  .. dsco:: VERSION
      :choices: AUTO, 1.0, 1.1, 1.2, 1.3, 1.4
      :Since: 2.2

      Set GeoPackage version
      (for application_id and user_version fields). In AUTO mode, this will
      be equivalent to 1.2 starting with GDAL 2.3.
      1.3 is available starting with GDAL 3.3
      1.4 is available starting with GDAL 3.7.1

-  .. dsco:: ADD_GPKG_OGR_CONTENTS
      :choices: YES, NO
      :default: YES
      :since: 2.2

      Defines whether to
      add a gpkg_ogr_contents table to keep feature count, and associated
      triggers.

-  .. dsco:: DATETIME_FORMAT
      :choices: WITH_TZ, UTC
      :default: WITH_TZ
      :since: 3.2

      Defines whether to keep the
      DateTime values in the time zones as used in the data source (WITH_TZ),
      or to convert the date and time expressions to UTC (Coordinated Universal Time).
      Pedantically, non-UTC time zones are not currently supported
      by GeoPackage v1.3 (see https://github.com/opengeospatial/geopackage/issues/530).
      When using UTC format, with a unspecified timezone, UTC will be assumed.

-  .. dsco:: CRS_WKT_EXTENSION
      :choices: YES, NO
      :default: NO
      :since: 3.8

      Defines whether to add the ``definition_12_063`` column to the
      ``gpkg_spatial_ref_sys`` system table, according to
      http://www.geopackage.org/spec/#extension_crs_wkt . The default is NO,
      unless the tile gridded coverage extension is used.
      With VERSION >= 1.4, a ``epoch`` column is also added.
      WKT strings in ``definition_12_063`` will follow the
      `WKT2:2015 standard <https://docs.ogc.org/is/12-063r5/12-063r5.html>`__
      when possible, but may use the
      `WKT2:2019 standard <https://docs.ogc.org/is/18-010r7/18-010r7.html>`__
      for specific cases (dynamic CRS with coordinate epoch).
      This option generally does not need to be specified, as the driver will
      automatically update the ``gpkg_spatial_ref_sys`` table when needed, but
      it may be useful to create GeoPackage datasets matching the exceptions of
      other software or profiles (such as the DGIWG-GPKG profile).

-  .. co:: METADATA_TABLES
      :choices: YES, NO
      :since: 3.8

      Defines whether to add the metadata system tables.
      By default, they are created on demand.
      If NO is specified, they are not created, even if metadata is set.
      If YES is specified, they are always created.


Other options are available for raster. See the :ref:`GeoPackage raster <raster.gpkg>`
documentation page.

Layer creation options
~~~~~~~~~~~~~~~~~~~~~~

|about-layer-creation-options|
The following layer creation options are available:

-  .. lco:: LAUNDER
      :choices: YES, NO
      :default: NO
      :since: 3.9

      Whether layer and field names will be laundered. Laundering makes sure
      that the recommendation of https://www.geopackage.org/guidance/getting-started.html
      is followed: an identifier should start with a lowercase character and
      only use lowercase characters, numbers 0-9, and underscores (_). UTF-8
      accented characters in the `Latin-1 Supplement <https://en.wikipedia.org/wiki/Latin-1_Supplement>`__
      and `Latin Extented-A <https://en.wikipedia.org/wiki/Latin_Extended-A>`__
      sets are replaced when possible with the closest ASCII letter.
      Characters that do not match the recommendation are replaced with underscore.
      Consequently this option is not appropriate for non-Latin languages.

-  .. lco:: GEOMETRY_NAME
      :default: geom

      Column to use for the geometry column. Default to
      "geom". Note: This option was called GEOMETRY_COLUMN in releases before
      GDAL 2

-  .. lco:: GEOMETRY_NULLABLE
      :choices: YES, NO
      :default: YES

      Whether the values of the
      geometry column can be NULL. Can be set to NO so that geometry is
      required.

-  .. lco:: SRID
      :choices: <integer>
      :since: 3.9

      Forced ``srs_id`` of the entry in the ``gpkg_spatial_ref_sys`` table to point to.
      This may be -1 ("Undefined Cartesian SRS"), 0 ("Undefined geographic SRS"),
      99999 ("Undefined SRS"), a valid EPSG CRS code or an existing entry of the
      ``gpkg_spatial_ref_sys`` table. If pointing to a non-existing entry, only a warning
      will be emitted.

-  .. lco:: DISCARD_COORD_LSB
      :choices: YES, NO
      :default: NO
      :since: 3.9

      Whether the geometry coordinate precision should be used to set to zero non-significant least-significant bits of geometries. Helps when further compression is used. See :ref:`ogr_gpkg_geometry_coordinate_precision` for more details.

-  .. lco:: UNDO_DISCARD_COORD_LSB_ON_READING
      :choices: YES, NO
      :default: NO
      :since: 3.9

      Whether to ask GDAL to take into coordinate precision to undo the effects of DISCARD_COORD_LSB. See :ref:`ogr_gpkg_geometry_coordinate_precision` for more details.

-  .. lco:: FID
      :default: fid

      Column name to use for the OGR FID (primary key in the
      SQLite database).

-  .. lco:: OVERWRITE
      :choices: YES, NO
      :default: NO

      If set to "YES" will delete any existing layers that
      have the same name as the layer being created.

-  .. lco:: SPATIAL_INDEX
      :choices: YES, NO
      :default: YES

      If set to "YES" will create a spatial
      index for this layer.

-  .. lco:: PRECISION
      :choices: YES, NO
      :default: YES

      This may be "YES" to force new fields
      created on this layer to try and represent the width of text fields
      (in terms of UTF-8 characters, not bytes), if available using
      TEXT(width) types. If "NO" then the type TEXT will be used instead.

-  .. lco:: TRUNCATE_FIELDS
      :choices: YES, NO
      :default: NO

      This may be "YES" to force
      truncated of field values that exceed the maximum allowed width of
      text fields, and also to "fix" the passed string if needed to make it
      a valid UTF-8 string. If "NO" then the value is not truncated nor
      modified.

-  .. lco:: IDENTIFIER

      Identifier of the layer, as put
      in the contents table.

-  .. lco:: DESCRIPTION

      Description of the layer, as
      put in the contents table.

-  .. lco:: ASPATIAL_VARIANT
      :choices: GPKG_ATTRIBUTES, NOT_REGISTERED
      :default: GPKG_ATTRIBUTES
      :since: 2.2

      How to register non spatial tables. Defaults to
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

-  .. lco:: DATETIME_PRECISION
      :choices: AUTO, MILLISECOND, SECOND, MINUTE
      :default: AUTO
      :since: 3.8

      Determines the level of detail for datetime fields.
      Starting with GeoPackage 1.4, three variants of datetime formats are supported:
      truncated at minute (``MINUTE``), truncated at second (``SECOND``) or
      including milliseconds (``MILLISECOND``).
      In ``AUTO`` mode and GeoPackage 1.4, milliseconds are included only if non-zero.
      Selecting modes ``MINUTE`` or ``SECOND`` will raise a warning with GeoPackage < 1.4.


Configuration options
---------------------

|about-config-options|
The following configuration options are available:

- :copy-config:`OGR_SQLITE_JOURNAL`

- :copy-config:`OGR_SQLITE_CACHE`

- :copy-config:`OGR_SQLITE_SYNCHRONOUS`

- :copy-config:`OGR_SQLITE_LOAD_EXTENSIONS`

- :copy-config:`OGR_SQLITE_PRAGMA`

- .. config:: OGR_CURRENT_DATE

     the driver updates the GeoPackage
     ``last_change`` timestamp when the file is created or modified. If consistent
     binary output is required for reproducibility, the timestamp can be forced to
     a specific value by setting this global configuration option.
     When setting the option, take care to meet the specific time format
     requirement of the GeoPackage standard,
     e.g. `for version 1.2 <https://www.geopackage.org/spec120/#r15>`__.

- :copy-config:`SQLITE_USE_OGR_VFS`

- .. config:: OGR_GPKG_NUM_THREADS
     :since: 3.8.3

     Can be set to an integer or ``ALL_CPUS``.
     This is the number of threads used when reading tables through the
     ArrowArray interface, when no filter is applied and when features have
     consecutive feature ID numbering.
     The default is the minimum of 4 and the number of CPUs.
     Note that setting this value too high is not recommended: a value of 4 is
     close to the optimal.


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
with the :lco:`ASPATIAL_VARIANT` layer creation option).

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

.. code-block:: sql

   CREATE VIEW my_view AS SELECT foo.fid AS OGC_FID, foo.geom FROM foo JOIN another_table ON foo.some_id = another_table.other_id;
   INSERT INTO gpkg_contents (table_name, identifier, data_type, srs_id) VALUES ( 'my_view', 'my_view', 'features', 4326);
   INSERT INTO gpkg_geometry_columns (table_name, column_name, geometry_type_name, srs_id, z, m) values ('my_view', 'my_geom', 'GEOMETRY', 4326, 0, 0);

This requires GDAL to be compiled with the SQLITE_HAS_COLUMN_METADATA
option and SQLite3 with the SQLITE_ENABLE_COLUMN_METADATA option.
Starting with GDAL 2.3, this can be easily verified if the
SQLITE_HAS_COLUMN_METADATA=YES driver metadata item is declared (for
example with "ogrinfo --format GPKG").

Starting with GDAL 3.7.1, it is possible to define a geometry column as the
result of a Spatialite spatial function. Note however that this is an extension
likely to be non-interoperable with other software that does not activate Spatialite
for the SQLite3 database connection. Such geometry column should be registered
into the ``gpkg_extensions`` using the ``gdal_spatialite_computed_geom_column``
extension name (cf :ref:`vector.gpkg_spatialite_computed_geom_column`), like below:

.. code-block:: sql

   CREATE VIEW my_view AS SELECT foo.fid AS OGC_FID, AsGBP(ST_Multi(foo.geom)) FROM foo;
   INSERT INTO gpkg_contents (table_name, identifier, data_type, srs_id) VALUES (
       'my_view', 'my_view', 'features', 4326);
   INSERT INTO gpkg_geometry_columns (table_name, column_name, geometry_type_name, srs_id, z, m) VALUES (
       'my_view', 'my_geom', 'MULTIPOLYGON', 4326, 0, 0);
   INSERT INTO gpkg_extensions (table_name, column_name, extension_name, definition, scope) VALUES (
       'my_view', 'my_geom', 'gdal_spatialite_computed_geom_column',
       'https://gdal.org/drivers/vector/gpkg_spatialite_computed_geom_column.html', 'read-write');


Coordinate Reference Systems
----------------------------

Valid geographic, projected and compound CRS supported in general by GDAL are
also supported by GeoPackage and stored in the ``gpkg_spatial_ref_sys`` table.

Two special hard-coded CRS are reserved per the GeoPackage specification:

- srs_id=0, for a Undefined Geographic CRS. For GDAL 3.8 or earlier, this one is
  selected by default if creating a spatial layer without any explicit CRS

- srs_id=-1, for a Undefined Projected CRS. It might be selected by creating a
  layer with a CRS instantiated from the following WKT string:
  ``LOCAL_CS["Undefined Cartesian SRS"]``. (GDAL >= 3.3)

Starting with GDAL 3.9, a layer without any explicit CRS is mapped from/to a
custom entry of srs_id=99999 with the following properties:

- ``srs_name``: ``Undefined SRS``
- ``organization``: ``GDAL``
- ``organization_coordsys_id``: 99999
- ``definition``: ``LOCAL_CS["Undefined SRS",LOCAL_DATUM["unknown",32767],UNIT["unknown",0],AXIS["Easting",EAST],AXIS["Northing",NORTH]]``
- ``definition_12_063`` (when the CRS WKT extension is used): ``ENGCRS["Undefined SRS",EDATUM["unknown"],CS[Cartesian,2],AXIS["easting",east,ORDER[1],LENGTHUNIT["unknown",0]],AXIS["northing",north,ORDER[2],LENGTHUNIT["unknown",0]]]``
- ``description``: ``Custom undefined coordinate reference system``

Note that the use of a LOCAL_CS / EngineeringCRS is mostly to provide a valid
CRS definition to comply with the requirements of the GeoPackage specification
and to be compatible of other applications (or GDAL 3.8 or earlier), but the
semantics of that entry is intended to be "undefined SRS of any kind".

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
   * - :ref:`vector.gpkg_spatialite_computed_geom_column`
     - No
     - Yes, starting with GDAL 3.7.1
   * - `OGC GeoPackage Related Tables Extension <http://www.geopackage.org/spec/related-tables/>`__
     - Yes
     - Yes, starting with GDAL 3.6

Compressed files
----------------

Starting with GDAL 3.7, the driver can also support reading and creating
.gpkg.zip files containing one .gpkg file.

On large files, good read performance can only be achieved if the file inside
the .zip is not compressed or compressed using the `SOZip <https://sozip.org>`__
optimization.

Update of an existing file is not supported.

Creation involves the creation of a temporary file. Sufficiently large files
will be automatically compressed using the SOZip optimization.

.. _ogr_gpkg_geometry_coordinate_precision:

Geometry coordinate precision
-----------------------------

.. versionadded:: GDAL 3.9

The GeoPackage driver supports reading and writing the geometry coordinate
precision, using the :cpp:class:`OGRGeomCoordinatePrecision` settings of the
:cpp:class:`OGRGeomFieldDefn`. By default, the geometry coordinate precision
is only noted in metadata, and does not cause geometries that are written to
be modified to comply with this precision.

Several settings may be combined to apply further processing:

* if the :config:`OGR_APPLY_GEOM_SET_PRECISION` configuration option is set to
  ``YES``, the :cpp:func:`OGRGeometry::SetPrecision` method will be applied
  when calling the CreateFeature() and SetFeature() method of the driver, to
  round X and Y coordinates to the specified precision, and fix potential
  geometry invalidities resulting from the rounding.

* if the ``DISCARD_COORD_LSB`` layer creation option is set to YES, the
  less-significant bits of the WKB geometry encoding which are not relevant for
  the requested precision are set to zero. This can improve further lossless
  compression stages, for example when putting a GeoPackage in an archive.
  Note however that when reading back such geometries and displaying them
  to the maximum precision, they will not "exactly" match the original
  :cpp:class:`OGRGeomCoordinatePrecision` settings. However, they will round
  back to it.
  The value of the ``DISCARD_COORD_LSB`` layer creation option is written in
  the dataset metadata, and will be re-used for later edition sessions.

* if the ``UNDO_DISCARD_COORD_LSB_ON_READING`` layer creation option is set to
  YES (only makes sense if the ``DISCARD_COORD_LSB`` layer creation option is
  also set to YES), when *reading* back geometries from a dataset, the
  :cpp:func:`OGRGeometry::roundCoordinates` method will be applied so that
  the geometry coordinates exactly match the original specified coordinate
  precision. That option will only be honored by GDAL 3.9 or later.


Implementation details: the coordinate precision is stored in a record in each
of the ``gpkg_metadata`` and ``gpkg_metadata_reference`` table, with the
following additional constraints on top of the ones imposed by the GeoPackage
specification:

- gpkg_metadata.md_standard_uri = 'http://gdal.org'
- gpkg_metadata.mime_type = 'text/xml'
- gpkg_metadata.metadata = '<CoordinatePrecision xy_resolution="{xy_resolution}" z_resolution="{z_resolution}" m_resolution="{m_resolution}" discard_coord_lsb={true or false} undo_discard_coord_lsb_on_reading={true or false} />'
- gpkg_metadata_reference.reference_scope = 'column'
- gpkg_metadata_reference.table_name = '{table_name}'
- gpkg_metadata_reference.column_name = '{geometry_column_name}'

Note that the xy_resolution, z_resolution or m_resolution attributes of the
XML CoordinatePrecision element are optional. Their numeric value is expressed
in the units of the SRS for xy_resolution and z_resolution.

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

-  Update of an existing GeoPackage file – e.g. a GeoPackage template –
   by adding features to it from another GeoPackage file containing
   features according to the same or a backwards compatible database
   schema.

   ::

      ogr2ogr -append output.gpkg input.gpkg

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
   gpkg_spatialite_computed_geom_column
