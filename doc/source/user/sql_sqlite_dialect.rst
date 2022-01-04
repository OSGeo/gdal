.. _sql_sqlite_dialect:

================================================================================
SQL SQLite dialect
================================================================================

.. highlight:: sql

The SQLite "dialect" can be used as an alternate SQL dialect to the
:ref:`ogr_sql_dialect`.
This assumes that GDAL/OGR is built with support for SQLite, and preferably
with `Spatialite <https://www.gaia-gis.it/fossil/libspatialite/index>`_ support too to benefit from spatial functions.

The SQLite dialect may be used with any OGR datasource, like the OGR SQL dialect. It
is available through the GDALDataset::ExecuteSQL() method by specifying the pszDialect to
"SQLITE". For the :ref:`ogrinfo` or :ref:`ogr2ogr`
utility, you must specify the "-dialect SQLITE" option.

This is mainly aimed to execute SELECT statements, but, for datasources that support
update, INSERT/UPDATE/DELETE statements can also be run. GDAL is internally using
`the Virtual Table Mechanism of SQLite <https://sqlite.org/vtab.html>`_
and therefore operations like ALTER TABLE are not supported. For executing ALTER TABLE
or DROP TABLE use :ref:`ogr_sql_dialect`

If the datasource is SQLite database (GeoPackage, SpatiaLite) then SQLite dialect
acts as native SQL dialect and Virtual Table Mechanism is not used. It is possible to
force GDAL to use Virtual Tables even in this case by specifying
"-dialect INDIRECT_SQLITE". This should be used only when necessary, since going through
the virtual table mechanism might affect performance.

The syntax of the SQL statements is fully the one of the SQLite SQL engine. You can
refer to the following pages:

- `SELECT <http://www.sqlite.org/lang_select.html>`_
- `INSERT <http://www.sqlite.org/lang_insert.html>`_
- `UPDATE <http://www.sqlite.org/lang_update.html>`_
- `DELETE <http://www.sqlite.org/lang_delete.html>`_

SELECT statement
----------------

The SELECT statement is used to fetch layer features (analogous to table
rows in an RDBMS) with the result of the query represented as a temporary layer
of features. The layers of the datasource are analogous to tables in an
RDBMS and feature attributes are analogous to column values. The simplest
form of OGR SQLITE SELECT statement looks like this:

.. code-block::

    SELECT * FROM polylayer

More complex statements can of course be used, including WHERE, JOIN, USING, GROUP BY,
ORDER BY, sub SELECT, ...

The table names that can be used are the layer names available in the datasource on
which the ExecuteSQL() method is called.

Similarly to OGRSQL, it is also possible to refer to layers of other datasources with
the following syntax : "other_datasource_name"."layer_name".

.. code-block::

    SELECT p.*, NAME FROM poly p JOIN "idlink.dbf"."idlink" il USING (eas_id)

If the master datasource is SQLite database (GeoPackage, SpatiaLite) it is necessary to
use indirect SQLite dialect. Otherwise additional datasources are never opened but tables to
be used in joins are searched from the master database.

.. code-block:: shell

    ogrinfo jointest.gpkg -dialect INDIRECT_SQLITE -sql "SELECT a.ID,b.ID FROM jointest a JOIN \"jointest2.shp\".\"jointest2\" b ON a.ID=b.ID"

The column names that can be used in the result column list, in WHERE, JOIN, ... clauses
are the field names of the layers. Expressions, SQLite functions, spatial functions, etc...
can also be used.


The conditions on fields expressed in WHERE clauses, or in JOINs are
translated, as far as possible, as attribute filters that are applied on the
underlying OGR layers. Joins can be very expensive operations if the secondary table is not
indexed on the key field being used.

Delimited identifiers
+++++++++++++++++++++

If names of layers or attributes are reserved keywords in SQL like 'FROM' or they
begin with a number or underscore they must be handled as "delimited identifiers" and
enclosed between double quotation marks in queries. Double quotes can be used even when
they are not strictly needed.

.. code-block::

    SELECT "p"."geometry", "p"."FROM", "p"."3D" FROM "poly" p

When SQL statements are used in the command shell and the statement itself is put
between double quotes, the internal double quotes must be escaped with \\

.. code-block:: shell

    ogrinfo p.shp -sql "SELECT geometry \"FROM\", \"3D\" FROM p"

Geometry field
++++++++++++++

The ``GEOMETRY`` special field represents the geometry of the feature
returned by OGRFeature::GetGeometryRef(). It can be explicitly specified
in the result column list of a SELECT, and is automatically selected if the
* wildcard is used.

For OGR layers that have a non-empty geometry column name (generally for RDBMS datasources),
as returned by OGRLayer::GetGeometryColumn(), the name of the geometry special field
in the SQL statement will be the name of the geometry column of the underlying OGR layer.
If the name of the geometry column in the source layer is empty, like with shapefiles etc.,
the name to use in the SQL statement is always "geometry".

.. code-block::

    SELECT EAS_ID, GEOMETRY FROM poly

returns:

::

    OGRFeature(SELECT):0
    EAS_ID (Real) = 168
    POLYGON ((479819.84375 4765180.5,479690.1875 4765259.5,[...],479819.84375 4765180.5))

.. code-block::

    SELECT * FROM poly

returns:

::

    OGRFeature(SELECT):0
    AREA (Real) = 215229.266
    EAS_ID (Real) = 168
    PRFEDEA (String) = 35043411
    POLYGON ((479819.84375 4765180.5,479690.1875 4765259.5,[...],479819.84375 4765180.5))

Feature id
++++++++++

The feature id is a special property of a feature and not treated
as an attribute of the feature.  In some cases it is convenient to be able to
utilize the feature id in queries and result sets as a regular field.  To do
so use the name ``rowid``. The field wildcard expansions will not include
the feature id, but it may be explicitly included using a syntax like:

.. code-block::

    SELECT rowid, * FROM nation

It is of course possible to rename it:

.. code-block::

    SELECT rowid AS fid, * FROM nation

OGR_STYLE special field
+++++++++++++++++++++++

The ``OGR_STYLE`` special field represents the style string of the feature
returned by OGRFeature::GetStyleString(). By using this field and the
``LIKE`` operator the result of the query can be filtered by the style.
For example we can select the annotation features as:

.. code-block::

    SELECT * FROM nation WHERE OGR_STYLE LIKE 'LABEL%'

Spatialite SQL functions
++++++++++++++++++++++++

When GDAL/OGR is build with support for the `Spatialite <https://www.gaia-gis.it/fossil/libspatialite/index>`_ library,
a lot of `extra SQL functions <http://www.gaia-gis.it/gaia-sins/spatialite-sql-latest.html>`_,
in particular spatial functions, can be used in results column fields, WHERE clauses, etc....

.. code-block::

    SELECT EAS_ID, ST_Area(GEOMETRY) AS area FROM poly WHERE
        ST_Intersects(GEOMETRY, BuildCircleMbr(479750.6875,4764702.0,100))

returns:

::

    OGRFeature(SELECT):0
    EAS_ID (Real) = 169
    area (Real) = 101429.9765625

    OGRFeature(SELECT):1
    EAS_ID (Real) = 165
    area (Real) = 596610.3359375

    OGRFeature(SELECT):2
    EAS_ID (Real) = 170
    area (Real) = 5268.8125

OGR datasource SQL functions
++++++++++++++++++++++++++++

The ``ogr_datasource_load_layers(datasource_name[, update_mode[, prefix]])``
function can be used to automatically load all the layers of a datasource as
:ref:`VirtualOGR tables <vector.sqlite>`.

::

    sqlite> SELECT load_extension('libgdal.so');

    sqlite> SELECT load_extension('libspatialite.so');

    sqlite> SELECT ogr_datasource_load_layers('poly.shp');
    1
    sqlite> SELECT * FROM sqlite_master;
    table|poly|poly|0|CREATE VIRTUAL TABLE "poly" USING VirtualOGR('poly.shp', 0, 'poly')

OGR layer SQL functions
+++++++++++++++++++++++

The following SQL functions are available and operate on a layer name :
``ogr_layer_Extent()``, ``ogr_layer_SRID()``,
``ogr_layer_GeometryType()`` and ``ogr_layer_FeatureCount()``

.. code-block::

    SELECT ogr_layer_Extent('poly'), ogr_layer_SRID('poly') AS srid,
        ogr_layer_GeometryType('poly') AS geomtype, ogr_layer_FeatureCount('poly') AS count

::

    OGRFeature(SELECT):0
    srid (Integer) = 40004
    geomtype (String) = POLYGON
    count (Integer) = 10
    POLYGON ((478315.53125 4762880.5,481645.3125 4762880.5,481645.3125 4765610.5,478315.53125 4765610.5,478315.53125 4762880.5))

OGR compression functions
+++++++++++++++++++++++++

``ogr_deflate(text_or_blob[, compression_level])`` returns a binary blob
compressed with the ZLib deflate algorithm. See :cpp:func:`CPLZLibDeflate`

``ogr_inflate(compressed_blob)`` returns the decompressed binary blob,
from a blob compressed with the ZLib deflate algorithm.
If the decompressed binary is a string, use
CAST(ogr_inflate(compressed_blob) AS VARCHAR). See CPLZLibInflate().

Other functions
+++++++++++++++

The ``hstore_get_value()`` function can be used to extract
a value associate to a key from a HSTORE string, formatted like "key=>value,other_key=>other_value,..."

.. code-block::

    SELECT hstore_get_value('a => b, "key with space"=> "value with space"', 'key with space') --> 'value with space'

OGR geocoding functions
+++++++++++++++++++++++

The following SQL functions are available : ``ogr_geocode(...)`` and ``ogr_geocode_reverse(...)``.

``ogr_geocode(name_to_geocode [, field_to_return [, option1 [, option2, ...]]])`` where
name_to_geocode is a literal or a column name that must be geocoded. field_to_return if specified can be "geometry" for
the geometry (default), or a field name of the layer returned by :cpp:func:`OGRGeocode`. The special field  "raw" can also be used
to return the raw response (XML string) of the geocoding service.
option1, option2, etc.. must be of the key=value format, and are options understood
by :cpp:func:`OGRGeocodeCreateSession` or OGRGeocode().

This function internally uses the OGRGeocode() API. Refer to it for more details.

.. code-block::

    SELECT ST_Centroid(ogr_geocode('Paris'))

returns:

::

    OGRFeature(SELECT):0
    POINT (2.342878767069653 48.85661793020374)

.. code-block:: shell

    ogrinfo cities.csv -dialect sqlite -sql "SELECT *, ogr_geocode(city, 'country') AS country, ST_Centroid(ogr_geocode(city)) FROM cities"

returns:


.. highlight:: none

::

    OGRFeature(SELECT):0
    id (Real) = 1
    city (String) = Paris
    country (String) = France métropolitaine
    POINT (2.342878767069653 48.85661793020374)

    OGRFeature(SELECT):1
    id (Real) = 2
    city (String) = London
    country (String) = United Kingdom
    POINT (-0.109369427546499 51.500506667319407)

    OGRFeature(SELECT):2
    id (Real) = 3
    city (String) = Rennes
    country (String) = France métropolitaine
    POINT (-1.68185153381778 48.111663929761093)

    OGRFeature(SELECT):3
    id (Real) = 4
    city (String) = Strasbourg
    country (String) = France métropolitaine
    POINT (7.767762859150757 48.571233274141846)

    OGRFeature(SELECT):4
    id (Real) = 5
    city (String) = New York
    country (String) = United States of America
    POINT (-73.938140243499049 40.663799577449979)

    OGRFeature(SELECT):5
    id (Real) = 6
    city (String) = Berlin
    country (String) = Deutschland
    POINT (13.402306623451983 52.501470321410636)

    OGRFeature(SELECT):6
    id (Real) = 7
    city (String) = Beijing
    POINT (116.391195 39.9064702)

    OGRFeature(SELECT):7
    id (Real) = 8
    city (String) = Brasilia
    country (String) = Brasil
    POINT (-52.830435216371839 -10.828214867369699)

    OGRFeature(SELECT):8
    id (Real) = 9
    city (String) = Moscow
    country (String) = Российская Федерация
    POINT (37.367988106866868 55.556208255649558)

.. highlight:: sql

``ogr_geocode_reverse(longitude, latitude, field_to_return [, option1 [, option2, ...]])`` where
longitude, latitude is the coordinate to query. field_to_return must be a field name of the layer
returned by OGRGeocodeReverse() (for example 'display_name'). The special field  "raw" can also be used
to return the raw response (XML string) of the geocoding service.
option1, option2, etc.. must be of the key=value format, and are options understood
by OGRGeocodeCreateSession() or OGRGeocodeReverse().

``ogr_geocode_reverse(geometry, field_to_return [, option1 [, option2, ...]])`` is also accepted
as an alternate syntax where geometry is a (Spatialite) point geometry.

This function internally uses the :cpp:func:`OGRGeocodeReverse` API. Refer to it for more details.

Spatialite spatial index
++++++++++++++++++++++++

Spatialite spatial index mechanism can be triggered by making sure a spatial index
virtual table is mentioned in the SQL (of the form idx_layername_geometrycolumn), or
by using the more recent SpatialIndex from the VirtualSpatialIndex extension. In which
case, a in-memory RTree will be built to be used to speed up the spatial queries.

For example, a spatial intersection between 2 layers, by using a spatial index on one
of the layers to limit the number of actual geometry intersection computations :

.. code-block::

    SELECT city_name, region_name FROM cities, regions WHERE
        ST_Area(ST_Intersection(cities.geometry, regions.geometry)) > 0 AND
        regions.rowid IN (
            SELECT pkid FROM idx_regions_geometry WHERE
                xmax >= MbrMinX(cities.geometry) AND xmin <= MbrMaxX(cities.geometry) AND
                ymax >= MbrMinY(cities.geometry) AND ymin <= MbrMaxY(cities.geometry))

or more elegantly :

.. code-block::

    SELECT city_name, region_name FROM cities, regions WHERE
        ST_Area(ST_Intersection(cities.geometry, regions.geometry)) > 0 AND
        regions.rowid IN (
            SELECT rowid FROM SpatialIndex WHERE
                f_table_name = 'regions' AND search_frame = cities.geometry)
