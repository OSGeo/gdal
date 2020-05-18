.. _vector.geopackage_aspatial:

GeoPackage aspatial extension
=============================

GeoPackage 1.0 Extension

Extension follows template from Annex I of the OGC `GeoPackage 1.0 Specification`_.

Extension Title
---------------

Aspatial Support

Introduction
^^^^^^^^^^^^

Support for aspatial data (i.e. SQLite tables/views without a geometry column),
potentially with associated metadata.

This was used in GDAL 2.0 and GDAL 2.1, before the introduction of the
'attributes' data_type of GeoPackage v1.2. Starting with GDAL 2.2, 'attributes'
will be used by default, so this extension is now legacy.

Extension Author
^^^^^^^^^^^^^^^^

`GDAL - Geospatial Data Abstraction Library`_, author_name `gdal`.

Extension Name or Template
^^^^^^^^^^^^^^^^^^^^^^^^^^

SQL

.. code-block::

    INSERT INTO gpkg_extensions
    (table_name, column_name, extension_name, definition, scope)
    VALUES
    (
        NULL,
        NULL,
        'gdal_aspatial',
        'http://gdal.org/geopackage_aspatial.html',
        'read-write'
    );

Extension Type
^^^^^^^^^^^^^^

Extension of Existing Requirement in Clause 2.

Applicability
^^^^^^^^^^^^^

This extension applies to any aspatial user data table or view specified in the
``gpkg_contents`` table with a lowercase `data_type` column value of "aspatial".

Scope
^^^^^

Read-write

Requirements
^^^^^^^^^^^^

GeoPackage
""""""""""

Contents Table - Aspatial

The `gpkg_contents` table SHALL contain a row with a lowercase `data_type`
column value of "aspatial" for each aspatial user data table or view.

User Data Tables

The second component of the SQL schema for aspatial tables in an Extended
GeoPackage described in clause 'Contents Table - Aspatial' above are user
tables or views that contain aspatial user data.

An Extended GeoPackage with aspatial support is not required to contain any
user data tables. User data tables MAY be empty.

An Extended GeoPackage with aspatial support MAY contain tables or views. Every
such aspatial table or view MAY have a column with column type INTEGER and
PRIMARY KEY AUTOINCREMENT column constraints per EXAMPLE.


.. list-table::
   :header-rows: 1

   * - Column Name
     - Type
     - Description
     - Null
     - Default
     - Key
   * - `id`
     - INTEGER
     - Autoincrement primary key
     - no
     -
     - PK
   * - `text_attribute`
     - TEXT
     - Text attribute of row
     - yes
     -
     -
   * - `real_attribute`
     - REAL
     - Real attribute of row
     - yes
     -
     -
   * - `boolean_attribute`
     - BOOLEAN
     - Boolean attribute of row
     - yes
     -
     -
   * - `raster_or_photo`
     - BLOB
     - Photograph
     - yes
     -
     -

An integer primary key of an aspatial table or view allows features to be
linked to row level metadata records in the `gpkg_metadata` table by
`SQLite ROWID`_ values in the `gpkg_metadata_reference` table as described
in clause 2.4.3 Metadata Reference Table.

An aspatial table or view SHALL NOT have a geometry column.

Columns in aspatial tables or views SHALL be defined using only the data types
specified in Table 1 in Clause 1.1.1.1.3.

GeoPackage SQLite Configuration
"""""""""""""""""""""""""""""""

None

GeoPackage SQLite Extension
"""""""""""""""""""""""""""

None

.. _`GeoPackage 1.0 Specification`: http://www.geopackage.org/
.. _`GDAL - Geospatial Data Abstraction Library`: http://gdal.org
.. _`SQLite ROWID`: http://www.sqlite.org/lang_createtable.html#rowid
