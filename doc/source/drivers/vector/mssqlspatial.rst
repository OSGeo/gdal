.. _vector.mssqlspatial:

MSSQLSpatial - Microsoft SQL Server Spatial Database
====================================================

.. shortname:: MSSQLSpatial

.. build_dependencies:: ODBC library

This driver implements support for access to spatial tables in Microsoft
SQL Server 2008+ which contains the geometry and geography data types to
represent the geometry columns.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

Connecting to a database
------------------------

| To connect to a MSSQL datasource, use a connection string specifying
  the database name, with additional parameters as necessary. The
  connection strings must be prefixed with '*MSSQL:*'.

   ::

      MSSQL:server=.\MSSQLSERVER2008;database=dbname;trusted_connection=yes

In addition to the standard parameters of the `ODBC driver connection
string <http://msdn.microsoft.com/en-us/library/ms130822.aspx>`__ format
the following custom parameters can also be used in the following
syntax:

-  **Tables=schema1.table1(geometry column1),schema2.table2(geometry
   column2)**: By using this parameter you can specify the subset of the
   layers to be used by the driver. If this parameter is not set, the
   layers are retrieved from the geometry_columns metadata table. You
   can omit specifying the schema and the geometry column portions of
   the syntax.
-  **GeometryFormat=native|wkb|wkt|wkbzm**: The desired format in which
   the geometries should be retrieved from the server. The default value
   is 'native' in this case the native SqlGeometry and SqlGeography
   serialization format is used. When using the 'wkb' or 'wkt' setting
   the geometry representation is converted to 'Well Known Binary' and
   'Well Known Text' at the server. This conversion requires a
   significant overhead at the server and makes the feature access
   slower than using the native format. The wkbzm format can only be
   used with SQL Server 2012.

The parameter names are not case sensitive in the connection strings.

Specifying the **Database** parameter is required by the driver in order
to select the proper database.

The connection may contain the optional **Driver** parameter if a custom
SQL server driver should be loaded (like FreeTDS). The default is **{SQL
Server}**

Layers
------

If the user defines the environment variable
*MSSQLSPATIAL_LIST_ALL_TABLES=YES* (and does not specify Tables= in the
connection string), all regular user tables will be treated as layers.
This option is useful if you want tables with with no spatial data

By default the MSSQL driver will only look for layers that are
registered in the *geometry_columns* metadata table.
If the user defines the environment variable
*MSSQLSPATIAL_USE_GEOMETRY_COLUMNS=NO* then the driver will look for all
user spatial tables found in the system catalog

SQL statements
--------------

The MS SQL Spatial driver passes SQL statements directly to MS SQL by
default, rather than evaluating them internally when using the
ExecuteSQL() call on the OGRDataSource, or the -sql command option to
ogr2ogr. Attribute query expressions are also passed directly through to
MSSQL. It's also possible to request the OGR MSSQL driver to handle SQL
commands with the :ref:`OGR SQL <ogr_sql_dialect>` engine, by passing
**"OGRSQL"** string to the ExecuteSQL() method, as the name of the SQL
dialect.

The MSSQL driver in OGR supports the OGRLayer::StartTransaction(),
OGRLayer::CommitTransaction() and OGRLayer::RollbackTransaction() calls
in the normal SQL sense.

Creation Issues
---------------

This driver doesn't support creating new databases, you might want to
use the *Microsoft SQL Server Client Tools* for this purpose, but it
does allow creation of new layers within an existing database.

Layer Creation Options
~~~~~~~~~~~~~~~~~~~~~~

-  **GEOM_TYPE**: The GEOM_TYPE layer creation option can be set to one
   of "geometry" or "geography". If this option is not specified the
   default value is "geometry". So as to create the geometry column with
   "geography" type, this parameter should be set "geography". In this
   case the layer must have a valid spatial reference of one of the
   geography coordinate systems defined in the
   **sys.spatial_reference_systems** SQL Server metadata table.
   Projected coordinate systems are not supported in this case.
-  **OVERWRITE**: This may be "YES" to force an existing layer of the
   desired name to be destroyed before creating the requested layer.
-  **LAUNDER**: This may be "YES" to force new fields created on this
   layer to have their field names "laundered" into a form more
   compatible with MSSQL. This converts to lower case and converts some
   special characters like "-" and "#" to "_". If "NO" exact names are
   preserved. The default value is "YES". If enabled the table (layer)
   name will also be laundered.
-  **PRECISION**: This may be "YES" to force new fields created on this
   layer to try and represent the width and precision information, if
   available using numeric(width,precision) or char(width) types. If
   "NO" then the types float, int and varchar will be used instead. The
   default is "YES".
-  **DIM={2,3}**: Control the dimension of the layer. Defaults to 3.
-  **GEOMETRY_NAME**: Set the name of geometry column in the new table.
   If omitted it defaults to *ogr_geometry*.
-  **SCHEMA**: Set name of schema for new table. If this parameter is
   not supported the default schema "*dbo"* is used.
-  **SRID**: Set the spatial reference id of the new table explicitly.
   The corresponding entry should already be added to the
   spatial_ref_sys metadata table. If this parameter is not set the SRID
   is derived from the authority code of source layer SRS.
-  **SPATIAL_INDEX**: Boolean flag (YES/NO) to
   enable/disable the automatic creation of a spatial index on the newly
   created layers (enabled by default).
-  **UPLOAD_GEOM_FORMAT**: Specify the geometry format
   (wkb or wkt) when creating or modifying features. The default is wkb.
-  **FID**: Name of the FID column to create. Defaults
   to ogr_fid.
-  **FID64**: Specifies whether to create the FID
   column with bigint type to handle 64bit wide ids. Default = NO
-  **GEOMETRY_NULLABLE**: Specifies whether the values
   of the geometry column can be NULL. Default = YES
-  **EXTRACT_SCHEMA_FROM_LAYER_NAME**: (From GDAL 2.3.0) Can be set to
   NO to avoid considering the dot character as the separator between
   the schema and the table name. Defaults to YES.

Configuration options
---------------------

The following :ref:`configuration options <configoptions>` are 
available:

-  :decl_configoption:`MSSQLSPATIAL_USE_BCP`: (From GDAL 2.1.0) Enable bulk insert when
   adding features. This option requires to to compile GDAL against a
   bulk copy enabled ODBC driver like SQL Server Native Client 11.0. To
   specify a BCP supported driver in the connection string, use the
   driver parameter, like DRIVER={SQL Server Native Client 11.0}. If
   GDAL is compiled against SQL Server Native Client 10.0 or 11.0 the
   driver is selected automatically not requiring to specify that in the
   connection string. If GDAL is compiled against SQL Server Native
   Client 10.0 or 11.0 the default setting of this parameter is TRUE,
   otherwise the parameter is ignored by the driver.
-  :decl_configoption:`MSSQLSPATIAL_BCP_SIZE`: (From GDAL 2.1.0) Specifies the bulk
   insert batch size. The larger value makes the insert faster, but
   consumes more memory. Default = 1000.
-  :decl_configoption:`MSSQLSPATIAL_OGR_FID`: Override FID column name. Default =
   ogr_fid.
-  :decl_configoption:`MSSQLSPATIAL_ALWAYS_OUTPUT_FID`: Always retrieve the FID value of
   the recently created feature (even if it is not a true IDENTITY
   column). Default = "NO".
-  :decl_configoption:`MSSQLSPATIAL_SHOW_FID_COLUMN`: Force to display the FID columns as
   a feature attribute. Default = "NO".
-  :decl_configoption:`MSSQLSPATIAL_USE_GEOMETRY_COLUMNS`: Use/create geometry_columns
   metadata table in the database. Default = "YES".
-  :decl_configoption:`MSSQLSPATIAL_LIST_ALL_TABLES`: Use mssql catalog to list available
   layers. Default = "NO".
-  :decl_configoption:`MSSQLSPATIAL_USE_GEOMETRY_VALIDATION`: (From GDAL 3.0) Let the
   driver detect the geometries which would trigger run time errors at
   MSSQL server. The driver tries to correct these geometries before
   submitting that to the server. Default = "YES".

Transaction support
-------------------

The driver implements transactions at the dataset level, per :ref:`rfc-54`

Examples
--------

Creating a layer from an OGR data source

   ::

      ogr2ogr -overwrite -f MSSQLSpatial "MSSQL:server=.\MSSQLSERVER2008;database=geodb;trusted_connection=yes" "rivers.tab"

      ogr2ogr -overwrite -f MSSQLSpatial "MSSQL:server=127.0.0.1;database=TestDB;UID=SA;PWD=DummyPassw0rd" "rivers.gpkg"
      
Connecting to a layer and dump the contents

   ::

      ogrinfo -al "MSSQL:server=.\MSSQLSERVER2008;database=geodb;tables=rivers;trusted_connection=yes"
      
      ogrinfo -al "MSSQL:server=127.0.0.1;database=TestDB;driver=ODBC Driver 17 for SQL Server;UID=SA;PWD=DummyPassw0rd"

Connecting with username/password

   ::
   
      ogrinfo -al   MSSQL:server=.\MSSQLSERVER2008;database=geodb;trusted_connection=no;UID=user;PWD=pwd
