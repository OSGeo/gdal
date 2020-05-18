.. _vector.pg_advanced:

PostgreSQL / PostGIS - Advanced Driver Information
==================================================

The information collected in that page deal with advanced topics, not
found in the :ref:`OGR PostgreSQL driver Information <vector.pg>` page.

Connection options related to schemas and tables
------------------------------------------------

The database opening should be significantly
faster than in previous versions, so using tables= or schemas= options
will not bring further noticeable speed-ups.

The set of tables to be scanned can be
overridden by specifying
*tables=[schema.]table[(geom_column_name)][,[schema2.]table2[(geom_column_name2)],...]*
within the connection string. If the parameter is found, the driver
skips enumeration of the tables as described in the next paragraph.

It is possible to restrict the schemas that
will be scanned while establishing the list of tables. This can be done
by specifying *schemas=schema_name[,schema_name2]* within the connection
string. This can also be a way of speeding up the connection to a
PostgreSQL database if there are a lot of schemas. Note that if only one
schema is listed, it will also be made automatically the active schema
(and the schema name will not prefix the layer name). Otherwise, the
active schema is still 'public', unless otherwise specified by the
*active_schema=* option.

The active schema ('public' being the default)
can be overridden by specifying *active_schema=schema_name* within the
connection string. The active schema is the schema where tables are
created or looked for when their name is not explicitly prefixed by a
schema name. Note that this does not restrict the tables that will be
listed (see *schemas=* option above). When getting the list of tables,
the name of the tables within that active schema will not be prefixed by
the schema name. For example, if you have a table 'foo' within the
public schema, and a table 'foo' within the 'bar_schema' schema, and
that you specify active_schema=bar_schema, 2 layers will be listed :
'foo' (implicitly within 'bar_schema') and 'public.foo'.

Multiple geometry columns
-------------------------

The PostgreSQL driver supports accessing
tables with multiple PostGIS geometry columns.

OGR supports reading, updating, creating tables with multiple
PostGIS geometry columns (following :ref:`rfc-41`)
For such a table, a single OGR layer will be reported with as many
geometry fields as there are geometry columns in the table.

For backward compatibility, it is also possible to query a layer with
GetLayerByName() with a name formatted like 'foo(bar)' where 'foo' is a
table and 'bar' a geometry column.

Layers
------

Even when PostGIS is enabled, if the user
defines the environment variable

::

   PG_LIST_ALL_TABLES=YES

(and does not specify tables=), all regular user tables and named views
will be treated as layers. However, tables with multiple geometry column
will only be reported once in that mode. So this variable is mainly
useful when PostGIS is enabled to find out tables with no spatial data,
or views without an entry in *geometry_columns* table.

In any case, all user tables can be queried explicitly with
GetLayerByName()

Regular (non-spatial) tables can be accessed, and will return features
with attributes, but not geometry. If the table has a "wkb_geometry"
field, it will be treated as a spatial table. The type of the field is
inspected to determine how to read it. It can be a PostGIS **geometry**
field, which is assumed to come back in OGC WKT, or type BYTEA or OID in
which case it is used as a source of OGC WKB geometry.

Tables inherited from spatial tables are
supported.

If there is an "ogc_fid" field, it will be used to set the feature id of
the features, and not treated as a regular field.

The layer name may be of the form "schema.table". The schema must exist,
and the user needs to have write permissions for the target and the
public schema.

If the user defines the environment variable

::

   PG_SKIP_VIEWS=YES

(and does not specify tables=), only the regular user tables will be
treated as layers. The default action is to include the views. This
variable is particularly useful when you want to copy the data into
another format while avoiding the redundant data from the views.

Named views
-----------

When PostGIS is enabled for the accessed database, named views are
supported, provided that there is an entry in the *geometry_columns*
tables. But, note that the AddGeometryColumn() SQL function doesn't
accept adding an entry for a view (only for regular tables). So, that
must usually be done by hand with a SQL statement like :

::

   "INSERT INTO geometry_columns VALUES ( '', 'public', 'name_of_my_view', 'name_of_geometry_column', 2, 4326, 'POINT');"

It is also possible to use named views without
inserting a row in the geometry_columns table. For that, you need to
explicitly specify the name of the view in the "tables=" option of the
connection string. See above. The drawback is that OGR will not be able
to report a valid SRS and figure out the right geometry type.

Retrieving FID of newly inserted feature
----------------------------------------

The FID of
a feature (i.e. usually the value of the OGC_FID column for the feature)
inserted into a table with CreateFeature(), in non-copy mode, will be
retrieved from the database and can be obtained with GetFID(). One
side-effect of this new behavior is that you must be careful if you
re-use the same feature object in a loop that makes insertions. After
the first iteration, the FID will be set to a non-null value, so at the
second iteration, CreateFeature() will try to insert the new feature
with the FID of the previous feature, which will fail as you cannot
insert 2 features with same FID. So in that case you must explicitly
reset the FID before calling CreateFeature(), or use a fresh feature
object.

Snippet example in Python :

::

       feat = ogr.Feature(lyr.GetLayerDefn())
       for i in range(100):
           feat.SetFID(-1)  # Reset FID to null value
           lyr.CreateFeature(feat)
           print('The feature has been assigned FID %d' % feat.GetFID())

or :

::

       for i in range(100):
           feat = ogr.Feature(lyr.GetLayerDefn())
           lyr.CreateFeature(feat)
           print('The feature has been assigned FID %d' % feat.GetFID())

Old GDAL behavior can be obtained by setting the configuration
option :decl_configoption:`OGR_PG_RETRIEVE_FID` to FALSE.

Issues with transactions
------------------------

Efficient sequential reading in PostgreSQL requires to be done within a
transaction (technically this is a CURSOR WITHOUT HOLD). So the PG
driver will implicitly open such a transaction if none is currently
opened as soon as a feature is retrieved. This transaction will be
released if ResetReading() is called (provided that no other layer is
still being read).

If within such an implicit transaction, an explicit dataset level
StartTransaction() is issued, the PG driver will use a SAVEPOINT to
emulate properly the transaction behavior while making the active
cursor on the read layer still opened.

If an explicit transaction is opened with dataset level
StartTransaction() before reading a layer, this transaction will be used
for the cursor that iterates over the layer. When explicitly committing
or rolling back the transaction, the cursor will become invalid, and
ResetReading() should be issued again to restart reading from the
beginning.

As calling SetAttributeFilter() or SetSpatialFilter() implies an
implicit ResetReading(), they have the same effect as ResetReading().
That is to say, while an implicit transaction is in progress, the
transaction will be committed (if no other layer is being read), and a
new one will be started again at the next GetNextFeature() call. On the
contrary, if they are called within an explicit transaction, the
transaction is maintained.

With the above rules, the below examples show the SQL instructions that
are run when using the OGR API in different scenarios.

::


   lyr1->GetNextFeature()             BEGIN (implicit)
                                      DECLARE cur1 CURSOR FOR SELECT * FROM lyr1
                                      FETCH 1 IN cur1

   lyr1->SetAttributeFilter('xxx')
        --> lyr1->ResetReading()      CLOSE cur1
                                      COMMIT (implicit)

   lyr1->GetNextFeature()             BEGIN (implicit)
                                      DECLARE cur1 CURSOR  FOR SELECT * FROM lyr1 WHERE xxx
                                      FETCH 1 IN cur1

   lyr2->GetNextFeature()             DECLARE cur2 CURSOR  FOR SELECT * FROM lyr2
                                      FETCH 1 IN cur2

   lyr1->GetNextFeature()             FETCH 1 IN cur1

   lyr2->GetNextFeature()             FETCH 1 IN cur2

   lyr1->CreateFeature(f)             INSERT INTO cur1 ...

   lyr1->SetAttributeFilter('xxx')
        --> lyr1->ResetReading()      CLOSE cur1
                                      COMMIT (implicit)

   lyr1->GetNextFeature()             DECLARE cur1 CURSOR  FOR SELECT * FROM lyr1 WHERE xxx
                                      FETCH 1 IN cur1

   lyr1->ResetReading()               CLOSE cur1

   lyr2->ResetReading()               CLOSE cur2
                                      COMMIT (implicit)

   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

   ds->StartTransaction()             BEGIN

   lyr1->GetNextFeature()             DECLARE cur1 CURSOR FOR SELECT * FROM lyr1
                                      FETCH 1 IN cur1

   lyr2->GetNextFeature()             DECLARE cur2 CURSOR FOR SELECT * FROM lyr2
                                      FETCH 1 IN cur2

   lyr1->CreateFeature(f)             INSERT INTO cur1 ...

   lyr1->SetAttributeFilter('xxx')
        --> lyr1->ResetReading()      CLOSE cur1
                                      COMMIT (implicit)

   lyr1->GetNextFeature()             DECLARE cur1 CURSOR  FOR SELECT * FROM lyr1 WHERE xxx
                                      FETCH 1 IN cur1

   lyr1->ResetReading()               CLOSE cur1

   lyr2->ResetReading()               CLOSE cur2

   ds->CommitTransaction()            COMMIT

   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

   ds->StartTransaction()             BEGIN

   lyr1->GetNextFeature()             DECLARE cur1 CURSOR FOR SELECT * FROM lyr1
                                      FETCH 1 IN cur1

   lyr1->CreateFeature(f)             INSERT INTO cur1 ...

   ds->CommitTransaction()            CLOSE cur1 (implicit)
                                      COMMIT

   lyr1->GetNextFeature()             FETCH 1 IN cur1      ==> Error since the cursor was closed with the commit. Explicit ResetReading() required before

   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

   lyr1->GetNextFeature()             BEGIN (implicit)
                                      DECLARE cur1 CURSOR FOR SELECT * FROM lyr1
                                      FETCH 1 IN cur1

   ds->StartTransaction()             SAVEPOINT savepoint

   lyr1->CreateFeature(f)             INSERT INTO cur1 ...

   ds->CommitTransaction()            RELEASE SAVEPOINT savepoint

   lyr1->ResetReading()               CLOSE cur1
                                      COMMIT (implicit)

Note: in reality, the PG drivers fetches 500 features at once. The FETCH
1 is for clarity of the explanation.

Advanced Examples
-----------------

-  This example shows using ogrinfo to list only the layers specified by
   the *tables=* options.

   ::

      ogrinfo -ro PG:'dbname=warmerda tables=table1,table2'

-  This example shows using ogrinfo to query a table 'foo' with multiple
   geometry columns ('geom1' and 'geom2').

   ::

      ogrinfo -ro -al PG:dbname=warmerda 'foo(geom2)'

-  This example show how to list only the layers inside the schema
   apt200810 and apt200812. The layer names will be prefixed by the name
   of the schema they belong to.

   ::

      ogrinfo -ro PG:'dbname=warmerda schemas=apt200810,apt200812'

-  This example shows using ogrinfo to list only the layers inside the
   schema named apt200810. Note that the layer names will not be
   prefixed by apt200810 as only one schema is listed.

   ::

      ogrinfo -ro PG:'dbname=warmerda schemas=apt200810'

-  This example shows how to convert a set of shapefiles inside the
   apt200810 directory into an existing Postgres schema apt200810. In
   that example, we could have use the schemas= option instead.

   ::

      ogr2ogr -f PostgreSQL "PG:dbname=warmerda active_schema=apt200810" apt200810

-  This example shows how to convert all the tables inside the schema
   apt200810 as a set of shapefiles inside the apt200810 directory. Note
   that the layer names will not be prefixed by apt200810 as only one
   schema is listed 

   ::

      ogr2ogr apt200810 PG:'dbname=warmerda schemas=apt200810'

-  This example shows how to overwrite an existing table in an existing
   schema. Note the use of -nln to specify the qualified layer name.

   ::

      ogr2ogr -overwrite -f PostgreSQL "PG:dbname=warmerda" mytable.shp mytable -nln myschema.mytable

   Note that using -lco SCHEMA=mytable instead of -nln would not have
   worked in that case (see
   `#2821 <http://trac.osgeo.org/gdal/ticket/2821>`__ for more details).

   If you need to overwrite many tables located in a schema at once, the
   -nln option is not the more appropriate, so it might be more
   convenient to use the active_schema connection string.
   The following example will overwrite, if necessary, all
   the PostgreSQL tables corresponding to a set of shapefiles inside the
   apt200810 directory :

   ::

      ogr2ogr -overwrite -f PostgreSQL "PG:dbname=warmerda active_schema=apt200810" apt200810

See Also
--------

-  :ref:`OGR PostgreSQL driver Information <vector.pg>`
