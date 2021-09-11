.. _vector.db2:

DB2 Spatial
===========

.. shortname:: DB2

.. build_dependencies:: ODBC library

.. deprecated_driver:: version_targeted_for_removal: 3.5
   env_variable: GDAL_ENABLE_DEPRECATED_DRIVER_DB2

This driver implements support for access to spatial tables in the IBM
DB2 for Linux, Unix and Windows (DB2 LUW) and the IBM DB2 for z/OS
relational databases using the default ODBC support incorporated into
GDAL.

The documentation for the DB2 spatial features can be found online for
`DB2 for
z/OS <http://www-01.ibm.com/support/knowledgecenter/SSEPEK_11.0.0/com.ibm.db2z11.doc.spatl/src/spatl/dasz_spatl.dita?lang=en>`__
and `DB2
LUW <http://www-01.ibm.com/support/knowledgecenter/SSEPGG_10.5.0/com.ibm.db2.luw.spatial.topics.doc/doc/db2sb03.html>`__

This driver is currently supported only in the Windows environment.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

Connecting to a database
------------------------

| To connect to a DB2 datasource, use a connection string specifying the
  database name, with additional parameters as necessary. The connection
  strings must be prefixed with '*DB2ODBC:*'.
| You can specify either a DSN that has been registered or use
  parameters that specify the host, port and protocol.

   ::

      DB2ODBC:database=dbname;DSN=datasourcename

or

   ::

      DB2ODBC:database=dbname;DRIVER={IBM DB2 ODBC DRIVER};Hostname=hostipaddr;PROTOCOL=TCPIP;port=db2port;UID=myuserid;PWD=mypw

The following custom parameters can also be used in the following
syntax:

-  **Tables=schema1.table1(geometry column1),schema2.table2(geometry
   column2)**: By using this parameter you can specify the subset of the
   layers to be used by the driver. If this parameter is not set, the
   layers are retrieved from the DB2GSE.ST_GEOMETRY_COLUMNS metadata
   view. You can omit specifying the schema and the geometry column
   portions of the syntax.

The parameter names are not case sensitive in the connection strings.

Specifying the **Database** parameter is required by the driver in order
to select the proper database.

Layers
------

By default the DB2 driver will only look for layers that are registered
in the *DB2GSE.ST_GEOMETRY_COLUMNS* metadata table.

SQL statements
--------------

The DB2 driver passes SQL statements directly to DB2 by default, rather
than evaluating them internally when using the ExecuteSQL() call on the
OGRDataSource, or the -sql command option to ogr2ogr. Attribute query
expressions are also passed directly through to DB2. It's also possible
to request the OGR DB2 driver to handle SQL commands with the :ref:`OGR
SQL <ogr_sql_dialect>` engine, by passing **"OGRSQL"** string to the
ExecuteSQL() method, as the name of the SQL dialect.

The DB2 driver in OGR supports the OGRLayer::StartTransaction(),
OGRLayer::CommitTransaction() and OGRLayer::RollbackTransaction() calls
in the normal SQL sense.

Creation Issues
---------------

This driver doesn't support creating new databases. Use the DB2 command
line or tools like IBM Data Studio to create the database. It does allow
creation of new layers within an existing database.

Layer Creation Options
~~~~~~~~~~~~~~~~~~~~~~

-  **OVERWRITE**: This may be "YES" to force an existing layer of the
   desired name to be destroyed before creating the requested layer.
-  **LAUNDER**: This may be "YES" to force new fields created on this
   layer to have their field names "laundered" into a form more
   compatible with DB2. This converts to lower case and converts some
   special characters like "-" and "#" to "_". If "NO" exact names are
   preserved. The default value is "YES". If enabled the table (layer)
   name will also be laundered.
-  **PRECISION**: This may be "YES" to force new fields created on this
   layer to try and represent the width and precision information, if
   available using numeric(width,precision) or char(width) types. If
   "NO" then the types float, int and varchar will be used instead. The
   default is "YES".
-  **DIM={2,3}**: Control the dimension of the layer. Defaults to 2.
-  **GEOM_NAME**: Set the name of geometry column in the new table. If
   omitted it defaults to *ogr_geometry*.
-  **SCHEMA**: Set name of schema for new table. The default schema is
   that of the userid used to connect to the database
-  **SRID**: Set the spatial reference id of the new table explicitly.
   The corresponding entry should already be added to the
   spatial_ref_sys metadata table. If this parameter is not set the SRID
   is derived from the authority code of source layer SRS.

Spatial Index Creation
~~~~~~~~~~~~~~~~~~~~~~

By default the DB2 driver doesn't add spatial indexes to the tables
during the layer creation. Spatial indexes should be created using the
DB2 CREATE INDEX command.

Examples
--------

Creating a layer from an OGR data source

   ::

      ogr2ogr -overwrite  DB2ODBC:database=sample;DSN=sampDSN zipcodes.shp

Connecting to a layer and dump the contents

   ::

      ogrinfo -al DB2ODBC:database=sample;DSN=sampDSN;tables=zipcodes
