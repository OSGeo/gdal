.. _vector.hana:

SAP HANA
====================

.. shortname:: HANA

.. build_dependencies:: odbc-cpp-wrapper

This driver implements read and write access for spatial data stored in
an `SAP HANA <https://www.sap.com/products/hana.html>`__ database.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

Connecting to a database
------------------------

| To connect to an SAP HANA database, use a connection string
  specifying the database name, with additional parameters as necessary.
  The HANA: prefix is used to mark the name as a HANA connection string.

   ::

      HANA:"DRIVER=HDBODBC;DATABASE=HAN;HOST=localhost;PORT=30015;USER=mylogin;PASSWORD=mypassword;SCHEMA=MYSCHEMA"

   In this syntax each parameter setting is in the form keyword = value.
   Spaces around the equal sign are optional. To write an empty value, or a
   value containing spaces, surround it with single quotes, e.g.,
   keyword = 'a value'. Single quotes and backslashes within the value must
   be escaped with a backslash, i.e., \' and \\.


SQL statements
--------------

The HANA driver passes SQL statements directly to HANA by
default, rather than evaluating them internally when using the
ExecuteSQL() call on the OGRDataSource, or the -sql command option to
ogr2ogr. Attribute query expressions are also passed directly through to
HANA. It's also possible to request the OGR HANA driver to handle
SQL commands with the :ref:`OGR SQL <ogr_sql_dialect>` engine, by
passing **"OGRSQL"** string to the ExecuteSQL() method, as the name of
the SQL dialect.

The HANA driver in OGR supports the OGRDataSource::StartTransaction(),
OGRDataSource::CommitTransaction() and OGRDataSource::RollbackTransaction()
calls in the normal SQL sense.

Creation Issues
---------------

The HANA driver does not support creation of new schemas, but it
does allow creation of new layers (tables) within an existing schema.

Dataset Open options
~~~~~~~~~~~~~~~~~~~~

-  .. oo:: DSN

      Data source name.

-  .. oo:: DRIVER

      Name or a path to a driver. For example,
      :oo:`DRIVER=HDBODBC` (Windows) or :oo:`DRIVER=/usr/sap/hdbclient/libodbcHDB.so`
      (Linux/MacOS).

-  .. oo:: HOST

      Server host name.

-  .. oo:: PORT

      Port number.

-  .. oo:: USER

      User name.

-  .. oo:: PASSWORD

      User password.

-  .. oo:: DATABASE

      Database name.

-  .. oo:: USER_STORE_KEY
      :since: 3.8

      Key defined in the SAP HANA user store (hdbuserstore).

-  .. oo:: SCHEMA

      Specifies schema used for tables listed in :oo:`TABLES`
      option.

-  .. oo:: TABLES

      Restricted set of tables to list (comma separated).

-  .. oo:: ENCRYPT
      :choices: YES, NO
      :default: NO

      Enables or disables TLS/SSL encryption.

-  .. oo:: SSL_CRYPTO_PROVIDER
      :choices: commoncrypto, sapcrypto, openssl

      Cryptographic library provider used for
      SSL communication.

-  .. oo:: SSL_KEY_STORE

      Path to the keystore file that contains the
      server's private key.

-  .. oo:: SSL_TRUST_STORE

      Path to trust store file that contains the
      server's public certificate(s) (OpenSSL only).

-  .. oo:: SSL_VALIDATE_CERTIFICATE
      :choices: YES, NO
      :default: YES

      If set to true, the server's
      certificate is validated.

-  .. oo:: SSL_HOST_NAME_IN_CERTIFICATE

      Host name used to verify server's
      identity validated.

-  .. oo:: CONNECTION_TIMEOUT
      :choices: <milliseconds>
      :default: 0

      Connection timeout measured in
      milliseconds. The default value is 0 (disabled).

-  .. oo:: PACKET_SIZE
      :choices: <bytes>
      :default: 1 MB

      Sets the maximum size of a request packet sent
      from the client to the server, in bytes. The minimum is 1 MB.

-  .. oo:: SPLIT_BATCH_COMMANDS
      :choices: YES, NO
      :default: YES

      Allows split and parallel execution of
      batch commands on partitioned tables.

-  .. oo:: DETECT_GEOMETRY_TYPE
      :choices: YES, NO
      :default: YES

      Specifies whether to detect the type of
      geometry columns. Note, the detection may take a significant amount of time
      for large tables.

Dataset Creation Options
~~~~~~~~~~~~~~~~~~~~~~~~

None

Layer Creation Options
~~~~~~~~~~~~~~~~~~~~~~

-  .. lco:: OVERWRITE
      :choices: YES, NO
      :default: NO

      This may be "YES" to force an existing layer of the
      desired name to be destroyed before creating the requested layer.

-  .. lco:: LAUNDER
      :choices: YES, NO
      :default: YES

      This may be "YES" to force new fields created on this
      layer to have their field names "laundered" into a form more
      compatible with HANA. This converts to upper case and converts
      some special characters like "-" and "#" to "_". If "NO" exact names
      are preserved. If enabled the table
      (layer) name will also be laundered.

-  .. lco:: PRECISION
      :choices: YES, NO
      :default: YES

      This may be "YES" to force new fields created on this
      layer to try and represent the width and precision information, if
      available using DECIMAL(width,precision) or CHAR(width) types. If
      "NO" then the types REAL, INTEGER and VARCHAR will be used instead.

-  .. lco:: DEFAULT_STRING_SIZE
      :default: 256

      Specifies default string column size.

-  .. lco:: GEOMETRY_NAME
      :default: GEOMETRY

      Specifies the name of the geometry column in new table.

-  .. lco:: GEOMETRY_NULLABLE
      :choices: YES, NO
      :default: YES

      Specifies whether the values of the geometry
      column can be NULL or not.

-  .. lco:: SRID

      Specifies the SRID of the layer.

-  .. lco:: FID
      :default: OGR_FID

      Specifies the name of the FID column to create.

-  .. lco:: FID64
      :choices: YES, NO
      :default: NO

      Specifies whether to create the FID column with BIGINT
      type to handle 64bit wide ids.

-  .. lco:: COLUMN_TYPES

      Specifies a comma-separated list of strings in
      the format field_name=hana_field_type that define column types.

-  .. lco:: BATCH_SIZE
      :choices: <bytes>
      :default: 4194304 (4MB)

      Specifies the number of bytes to be written per one
      batch.

Multitenant Database Containers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In order to connect to a tenant database, you need to specify a port number
assigned exactly to a desired instance. This port number can be determined
by executing the following query from the tenant database.

   ::

      SELECT SQL_PORT FROM SYS.M_SERVICES WHERE ((SERVICE_NAME='indexserver' and COORDINATOR_TYPE= 'MASTER') or (SERVICE_NAME='xsengine'))

For more details, see **Section 2.9 Connections for Multitenant Database Containers**
in `SAP HANA Multitenant Database Containers <https://help.sap.com/doc/0987e3b51fb74e5a8631385fe4599c97/2.0.00/en-us/sap_hana_multitenant_database_containers_en.pdf>`__.


Examples
--------

-  This example shows how to list HANA layers on a specified host using
   :ref:`ogrinfo` command.

   ::

      ogrinfo -ro HANA:"DRIVER=HDBODBC;DATABASE=HAN;HOST=localhost;PORT=30015;USER=mylogin;PASSWORD=mypassword;SCHEMA=MYSCHEMA"

   or

   ::

      ogrinfo -ro HANA:"DSN=MYHANADB;USER=mylogin;PASSWORD=mypassword;SCHEMA=MYSCHEMA"

   or

   ::

      ogrinfo -ro HANA:"DRIVER=HDBODBC;USER_STORE_KEY=mykey;SCHEMA=MYSCHEMA"

-  This example shows how to print summary information about a given layer,
   i.e. 'planet_osm_line', using :ref:`ogrinfo`.

   ::

      ogrinfo -ro HANA:"DRIVER=HDBODBC;DATABASE=HAN;HOST=localhost;PORT=30015;USER=mylogin;PASSWORD=mypassword;SCHEMA=MYSCHEMA" -so "planet_osm_line"

      Layer name: planet_osm_line
      Geometry: Line String
      Feature Count: 81013
      Extent: (732496.086304, 6950959.464783) - (1018694.144531, 7204272.976379)
      Layer SRS WKT:
      PROJCS["WGS 84 / Pseudo-Mercator",
          GEOGCS["WGS 84",
              DATUM["WGS_1984",
                  SPHEROID["WGS 84",6378137,298.257223563, AHORITY["EPSG","7030"]],
                  AUTHORITY["EPSG","6326"]],
                  PRIMEM["Greenwich",0, AUTHORITY["EPSG","8901"]],
                  UNIT["degree",0.0174532925199433, AUTHORITY["EPSG","9122"]],
                  AUTHORITY["EPSG","4326"]],
              PROJECTION["Mercator_1SP"],
              PARAMETER["central_meridian",0],
              PARAMETER["scale_factor",1],
              PARAMETER["false_easting",0],
              PARAMETER["false_northing",0],
              UNIT["metre",1,AUTHORITY["EPSG","9001"]],
              AXIS["X",EAST],
              AXIS["Y",NORTH],
              AUTHORITY["EPSG","3857"]]
      Geometry Column = way
      osm_id: Integer64 (0.0)
      access: String (4000.0)
      addr:housename: String (4000.0)
      addr:housenumber: String (4000.0)
      addr:interpolation: String (4000.0)
      admin_level: String (4000.0)
      aerialway: String (4000.0)
      aeroway: String (4000.0)

-  This example shows how to export data from the 'points' table to a shapefile called 'points_output.shp'.

   ::

      ogr2ogr -f "ESRI Shapefile" "D:\\points_output.shp" HANA:"DRIVER=HDBODBC;DATABASE=HAN;HOST=localhost;PORT=30015;USER=mylogin;PASSWORD=mypassword;SCHEMA=GIS;TABLES=points"

-  This example shows how to create and populate a table with data taken from a shapefile.

   ::

      ogr2ogr -f HANA HANA:"DRIVER=HDBODBC;DATABASE=HAN;HOST=localhost;PORT=30015;USER=mylogin;PASSWORD=mypassword;SCHEMA=MYSCHEMA" myshapefile.shp


For developers
--------------

To compile the SAP HANA driver, `odbc-cpp-wrapper <https://github.com/SAP/odbc-cpp-wrapper/>`__ library needs to be linked or installed.
For more details, see comments in nmake.opt or configure.ac files to build the driver for Windows or Linux/MacOS correspondingly.

See Also
--------

-  `SAP HANA Home Page <https://www.sap.com/products/hana.html>`__
-  `SAP HANA Spatial Reference <https://help.sap.com/viewer/cbbbfc20871e4559abfd45a78ad58c02/2.0.03/en-US/e1c934157bd14021a3b43b5822b2cbe9.html>`__
-  `SAP HANA ODBC Connection Properties <https://help.sap.com/viewer/0eec0d68141541d1b07893a39944924e/2.0.02/en-US/7cab593774474f2f8db335710b2f5c50.html>`__
