.. _vector.oci:

Oracle Spatial
==============

.. shortname:: OCI

.. build_dependencies:: OCI library

This driver supports reading and writing data in Oracle Spatial (8.1.7
or later) Object-Relational format. The Oracle Spatial driver is not
normally built into OGR, but may be built in on platforms where the
Oracle client libraries are available.

When opening a database, its name should be specified in the form
"OCI:userid/password@database_instance:table,table". The list of tables
is optional. The database_instance portion may be omitted when accessing
the default local database instance.

If the list of tables is not provided, then all tables appearing in
ALL_SDO_GEOM_METADATA will be treated by OGR as layers with the table
names as the layer names. Non-spatial tables or spatial tables not
listed in the ALL_SDO_GEOM_METADATA table are not accessible unless
explicitly listed in the datasource name. Even in databases where all
desired layers are in the ALL_SDO_GEOM_METADATA table, it may be
desirable to list only the tables to be used as this can substantially
reduce initialization time in databases with many tables.

If the table has an integer column called OGR_FID it will be used as the
feature id by OGR (and it will not appear as a regular attribute). When
loading data into Oracle Spatial OGR will always create the OGR_FID
field.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

SQL Issues
----------

By default, the Oracle driver passes SQL statements directly to Oracle
rather than evaluating them internally when using the ExecuteSQL() call
on the OGRDataSource, or the -sql command option to ogr2ogr. Attribute
query expressions are also passed through to Oracle.

As well two special commands are supported via the ExecuteSQL()
interface. These are "**DELLAYER:<table_name>**" to delete a layer, and
"**VALLAYER:<table_name>**" to apply the SDO_GEOM.VALIDATE_GEOMETRY()
check to a layer. Internally these pseudo-commands are translated into
more complex SQL commands for Oracle.

It is also possible to request the driver to handle SQL commands with
:ref:`OGR SQL <ogr_sql_dialect>` engine, by passing **"OGRSQL"**
string to the ExecuteSQL() method, as name of the SQL dialect.

Caveats
-------

-  The type recognition logic is currently somewhat impoverished. No
   effort is made to preserve real width information for integer and
   real fields.
-  Various types such as objects, and BLOBs in Oracle will be completely
   ignored by OGR.
-  Currently the OGR transaction semantics are not properly mapped onto
   transaction semantics in Oracle.
-  If an attribute called OGR_FID exists in the schema for tables being
   read, it will be used as the FID. Random (FID based) reads on tables
   without an identified (and indexed) FID field can be very slow. To
   force use of a particular field name the OCI_FID configuration
   variable (i.e. environment variable) can be set to the target field
   name.
-  Curved geometry types are converted to linestrings or linear rings in
   six degree segments when reading. The driver has no support for
   writing curved geometries.
-  There is no support for point cloud (SDO_PC), TIN (SDO_TIN) and
   annotation text data types in Oracle Spatial.
-  It might be necessary to define the environment variable NLS_LANG to
   "American_America.UTF8" to avoid issues with floating point numbers
   being truncated to integer on non-English environments.
-  For developers: when running the driver under the memory error
   detection tool Valgrind, specifying the database_instance, typically
   to localhost, or with the TWO_TASK environment variable seems to be
   compulsory, otherwise "TNS:permission denied" errors will be
   reported)

Creation Issues
---------------

The Oracle Spatial driver does not support creation of new datasets
(database instances), but it does allow creation of new layers within an
existing database.

Upon closing the OGRDataSource newly created layers will have a spatial
index automatically built. At this point the USER_SDO_GEOM_METADATA
table will also be updated with bounds for the table based on the
features that have actually been written. One consequence of this is
that once a layer has been loaded it is generally not possible to load
additional features outside the original extents without manually
modifying the DIMINFO information in USER_SDO_GEOM_METADATA and
rebuilding the spatial index.

Layer Creation Options
~~~~~~~~~~~~~~~~~~~~~~

-  **OVERWRITE**: This may be "YES" to force an existing layer (=table)
   of the same desired name to be destroyed before creating the
   requested layer. The default value is "NO"
-  **TRUNCATE**: This may be "YES" to force the existing table to be
   reused, but to first truncate all records in the table, preserving
   indexes or dependencies. The default value is "NO".
-  **LAUNDER**: This may be "YES" to force new fields created on this
   layer to have their field names "laundered" into a form more
   compatible with Oracle. This converts to upper case and converts some
   special characters like "-" and "#" to "_". The default value is
   "NO".
-  **PRECISION**: This may be "YES" to force new fields created on this
   layer to try and represent the width and precision information, if
   available using NUMBER(width,precision) or VARCHAR2(width) types. If
   "NO" then the types NUMBER, INTEGER and VARCHAR2 will be used
   instead. The default is "YES".
-  **DIM**: This may be set to 2 or 3 to force the dimension of the
   created layer. Prior to GDAL 2.2, 3 is used by default. Starting with
   GDAL 2.2, the dimension of the layer geometry type is used by
   default.
-  **SPATIAL_INDEX**: This may be set to FALSE to disable creation of a
   spatial index when a layer load is complete. By default an index is
   created if any of the layer features have valid geometries. The
   default is "YES". Note: option was called INDEX in releases before
   GDAL 2
-  **INDEX_PARAMETERS**: This may be set to pass creation parameters
   when the spatial index is created. For instance setting
   INDEX_PARAMETERS to SDO_RTR_PCTFREE=0 would cause the rtree index to
   be created without any empty space. By default no parameters are
   passed causing a default R-Tree spatial index to be created.
-  **ADD_LAYER_GTYPE**\ =YES/NO: This may be
   set to NO to disable the constraints on the geometry type in the
   spatial index, through the layer_gtype keyword in the PARAMETERS
   clause of the CREATE INDEX. Layers of type MultiPoint,
   MultiLineString or MultiPolygon will also accept single geometry type
   (Point, LineString, Polygon). Defaults to YES.
-  **DIMINFO_X**: This may be set to xmin,xmax,xres values to control
   the X dimension info written into the USER_SDO_GEOM_METADATA table.
   By default extents are collected from the actual data written.
-  **DIMINFO_Y**: This may be set to ymin,ymax,yres values to control
   the Y dimension info written into the USER_SDO_GEOM_METADATA table.
   By default extents are collected from the actual data written.
-  **DIMINFO_Z**: This may be set to zmin,zmax,zres values to control
   the Z dimension info written into the USER_SDO_GEOM_METADATA table.
   By default fixed values of -100000,100000,0.002 are used for layers
   with a third dimension.
-  **SRID**: By default this driver will attempt to find an existing row
   in the MDSYS.CS_SRS table with a well known text coordinate system
   exactly matching the one for this dataset. If one is not found, a new
   row will be added to this table. The SRID creation option allows the
   user to force use of an existing Oracle SRID item even it if does not
   exactly match the WKT the driver expects.
-  **MULTI_LOAD**: If enabled new features will be created in groups of
   100 per SQL INSERT command, instead of each feature being a separate
   INSERT command. Having this enabled is the fastest way to load data
   quickly. Multi-load mode is enabled by default, and may be forced off
   for existing layers or for new layers by setting to NO. The number of
   rows in each group is defined by MULTI_LOAD_COUNT. To load one row at
   a time, set MULTI_LOAD to NO.
-  **MULTI_LOAD_COUNT**: Define the number of features on each ARRAY
   INSERT command, instead of the default 100 item defined by
   MULTI_LOAD. Since each array insert will commit a transaction, this
   options shouldn't be combined with ogr2ogr "-gt N". Use "-gt
   unlimited" preferably when using MULTI_LOAD_COUNT. The default is
   100. If neither MULTI_LOAD nor MULTI_LOAD_COUNT are specified, then
   the loading happens in groups of 100 rows.
-  **FIRST_ID**: Define the first numeric value of the id column on the
   first rows. It's also work as a open option when used to append or
   update an existing dataset.
-  **NO_LOGGING**: Define that the table and the geometry will be create
   with nologging attributes.
-  **LOADER_FILE**: If this option is set, all feature information will
   be written to a file suitable for use with SQL*Loader instead of
   inserted directly in the database. The layer itself is still created
   in the database immediately. The SQL*Loader support is experimental,
   and generally MULTI_LOAD enabled mode should be used instead when
   trying for optimal load performance.
-  **GEOMETRY_NAME**: By default OGR creates new tables with the
   geometry column named ORA_GEOMETRY. If you wish to use a different
   name, it can be supplied with the GEOMETRY_NAME layer creation
   option.

Layer Open Options
~~~~~~~~~~~~~~~~~~

-  **FIRST_ID**: See Layer Create Options comments on FIRST_ID.
-  **MULTI_LOAD**: See Layer Create Options comments on MULTI_LOAD.
-  **MULTI_LOAD_COUNT**: See Layer Create Options comments on
   MULTI_LOAD_COUNT.
-  **WORKSPACE**: Define what user workspace to use.

Example
~~~~~~~

Simple translation of a shapefile into Oracle. The table 'ABC' will be
created with the features from abc.shp and attributes from abc.dbf.

::

   % ogr2ogr -f OCI OCI:warmerda/password@gdal800.dreadfest.com abc.shp

This second example loads a political boundaries layer from VPF (via the
:ref:`OGDI driver <vector.ogdi>`), and renames the layer from the cryptic
OGDI layer name to something more sensible. If an existing table of the
desired name exists it is overwritten.

::

   % ogr2ogr  -f OCI OCI:warmerda/password \
           gltp:/vrf/usr4/mpp1/v0eur/vmaplv0/eurnasia \
           -lco OVERWRITE=yes -nln polbndl_bnd 'polbndl@bnd(*)_line'

This example shows using ogrinfo to evaluate an SQL query statement
within Oracle. More sophisticated Oracle Spatial specific queries may
also be used via the -sql commandline switch to ogrinfo.

::

   ogrinfo -ro OCI:warmerda/password -sql "SELECT pop_1994 from canada where province_name = 'Alberta'"

Credits
~~~~~~~

I would like to thank `SRC, LLC <http://www.extendthereach.com/>`__ for
its financial support of the development of this driver.
