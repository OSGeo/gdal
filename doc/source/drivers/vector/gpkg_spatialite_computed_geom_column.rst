.. _vector.gpkg_spatialite_computed_geom_column:

GeoPackage Spatialite computed geometry column extension
========================================================

GeoPackage 1.3.1 Extension

Extension follows template from Annex E of the OGC `GeoPackage 1.3.1 Specification`_.

Extension Title
---------------

Spatialite computed geometry column

Introduction
------------

Support for geometry column computed from the result of a SQL function
of the Spatialite library.

Extension Author
----------------

`GDAL - Geospatial Data Abstraction Library`_, author_name `gdal`.

Extension Name or Template
--------------------------

The extension name is ``gdal_spatialite_computed_geom_column``.

Each view using the extension should register its geometry column with
the following template:

.. code-block:: sql

    INSERT INTO gpkg_extensions
    (table_name, column_name, extension_name, definition, scope)
    VALUES
    (
        '{view_name}',
        '{geometry_column_name}',
        'gdal_spatialite_computed_geom_column',
        'https://gdal.org/drivers/vector/gpkg_spatialite_computed_geom_column.html',
        'read-write'
    );

Extension Type
--------------

Extension of Existing Requirement in Clause 2.1 "Features".

Applicability
-------------

This extension applies to any view specified in the
``gpkg_contents`` table with a lowercase `data_type` column value of "features"
which defines its geometry column as the result of a SQL function
of the Spatialite library.

Scope
-----

Read-write

Requirements
------------

GeoPackage
++++++++++

A view using this extension should have its geometry column defined
as the result of a Spatialite SQL function returning a geometry, and wrapped
with the ``AsGPB`` function to return a GeoPackage geometry blob.

An example of such a view is:

.. code-block:: sql

    CREATE VIEW my_view AS SELECT foo.fid AS OGC_FID, AsGBP(ST_Multi(foo.geom)) FROM foo


The view name and geometry column name should be registered in the ``gpkg_extensions``
table, as shown in the above Extension Template.

GeoPackage SQLite Configuration
+++++++++++++++++++++++++++++++

None

GeoPackage SQLite Extension
+++++++++++++++++++++++++++

This extension assumes that the `Spatialite SQL functions`_ are available at
runtime.


.. _`GeoPackage 1.3.1 Specification`: http://www.geopackage.org/spec131
.. _`GDAL - Geospatial Data Abstraction Library`: http://gdal.org
.. _`Spatialite SQL functions`: https://www.gaia-gis.it/gaia-sins/spatialite-sql-5.0.1.html
