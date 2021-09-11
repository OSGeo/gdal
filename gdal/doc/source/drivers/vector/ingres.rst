.. _vector.ingres:

INGRES
======

.. shortname:: INGRES

.. build_dependencies:: INGRESS

.. deprecated_driver:: version_targeted_for_removal: 3.5
   env_variable: GDAL_ENABLE_DEPRECATED_DRIVER_INGRES

This driver implements read and write access for spatial data in
`INGRES <http://www.actian.com/products/ingres/geospatial/>`__ database
tables.

When opening a database, its name should be specified in the form
"@driver=ingres,[host=*host*,
instance=\ *instance*],dbname=\ *[vnode::]dbname* [,options]". where the
options can include comma separated items like
"host=*ip_address*","instance=*instance*", "username=*userid*",
"password=*password*", "effuser=*database_user*",
"dbpwd=*database_passwd*", "timeout=*timeout*", "tables=table1/table2".

**The driver and dbname values are required, while the rest are
optional.** If username and password are not provided an attempt is made
to authenticate as the current OS user.

If the host and instance options are both specified, the username and
password \*must\* be supplied as it creates a temporary `dynamic vnode
connection <http://docs.actian.com/ingres/10.0/command-reference-guide/1207-dynamic-vnode-specificationconnect-to-remote-node>`__.
The default protocol is TCP/IP. If any other protocol is expected to be
used, a pre-built vnode is preferable. If vnode and these two options
are passed at the same time, an error will occur.

The option effuser and dbpwd are mapped to the real user name and
password needs to be authorized in dbms, compared to the username and
password which are used for OS level authorization.

Examples:

::

     @driver=ingres,host=192.168.0.1, instance=II, dbname=test,userid=warmerda,password=test,effuser=frank, dbpwd=123, tables=usa/canada

     @driver=ingres,host=192.168.0.1, instance=II, dbname=test,userid=warmerda,password=test,tables=usa/canada

     @driver=ingres,dbname=test,userid=warmerda,password=test,tables=usa/canada

     @driver=ingres,dbname=test,userid=warmerda,password=test,tables=usa/canada

     @driver=ingres,dbname=test,userid=warmerda,password=test,tables=usa/canada

     @driver=ingres,dbname=server::mapping

     @driver=ingres,dbname=mapping

If the tables list is not provided, an attempt is made to enumerate all
non-system tables as layers, otherwise only the listed tables are
represented as layers. This option is primarily useful when a database
has a lot of tables, and scanning all their schemas would take a
significant amount of time.

If an integer field exists in a table that is named "ogr_fid" it will be
used as the FID, otherwise FIDs will be assigned sequentially. This can
result in different FIDs being assigned to a given record/feature
depending on the spatial and attribute query filters in effect at a
given time.

By default, SQL statements are passed directly to the INGRES database
engine. It's also possible to request the driver to handle SQL commands
with :ref:`OGR SQL <ogr_sql_dialect>` engine, by passing **"OGRSQL"**
string to the ExecuteSQL() method, as name of the SQL dialect.

The INGRES driver supports OGC SFSQL 1.1 compliant spatial types and
functions, including types: POINT, LINESTRING, POLYGON, MULTI\*
versions, and GEOMETRYCOLLECTION.

Driver capabilities
-------------------

.. supports_create::

Caveats
-------

-  No fast spatial index is used when reading, so spatial filters are
   implemented by reading and parsing all records, and then discarding
   those that do not satisfy the spatial filter.

Creation Issues
---------------

The INGRES driver does not support creation of new datasets (a database
within INGRES), but it does allow creation of new layers (tables) within
an existing database instance.

-  The INGRES driver makes no allowances for character encodings at this
   time.
-  The INGRES driver is not transactional at this time.

Layer Creation Options
~~~~~~~~~~~~~~~~~~~~~~

-  **OVERWRITE**: This may be "YES" to force an existing layer of the
   desired name to be destroyed before creating the requested layer.
-  **LAUNDER**: This may be "YES" to force new fields created on this
   layer to have their field names "laundered" into a form more
   compatible with MySQL. This converts to lower case and converts some
   special characters like "-" and "#" to "_". If "NO" exact names are
   preserved. The default value is "YES".
-  **PRECISION**: This may be "TRUE" to attempt to preserve field widths
   and precisions for the creation and reading of MySQL layers. The
   default value is "TRUE".
-  **GEOMETRY_NAME**: This option specifies the name of the geometry
   column. The default value is "SHAPE".
-  **INGRES_FID**: This option specifies the name of the FID column. The
   default value is "OGR_FID"
-  **GEOMETRY_TYPE**: Specifies the object type for the geometry column.
   It may be one of POINT, LSEG, LINE, LONG LINE, POLYGON, or LONG
   POLYGON. By default POINT, LONG LINE or LONG POLYGON are used
   depending on the layer type.

Older Versions
--------------

The INGRES GDAL driver also includes support for old INGRES spatial
types, but these are not enabled by default. It enable these, the input
*configure* script needs to include pointers to libraries used by the
older version:

::

   INGRES_LIB="-L$II_SYSTEM/ingres/lib \
            $II_SYSTEM/ingres/lib/iiclsadt.o \
            $II_SYSTEM/ingres/lib/iiuseradt.o \
            -liiapi.1 -lcompat.1 -lq.1 -lframe.1"
