.. _vector.pgeo:

ESRI Personal GeoDatabase
=========================

.. shortname:: PGeo

.. build_dependencies:: ODBC library

OGR optionally supports reading ESRI Personal GeoDatabase .mdb files via
ODBC. Personal GeoDatabase is a Microsoft Access database with a set of
tables defined by ESRI for holding geodatabase metadata, and with
geometry for features held in a BLOB column in a custom format
(essentially Shapefile geometry fragments). This drivers accesses the
personal geodatabase via ODBC but does not depend on any ESRI
middle-ware.

Personal Geodatabases are accessed by passing the file name of the .mdb
file to be accessed as the data source name.

In order to facilitate compatibility with different configurations, the
PGEO_DRIVER_TEMPLATE Config Option was added to provide a way to
programmatically set the DSN programmatically with the filename as an
argument. In cases where the driver name is known, this allows for the
construction of the DSN based on that information in a manner similar to
the default (used for Windows access to the Microsoft Access Driver).

OGR treats all feature tables as layers. Most geometry types should be
supported, including 3D data. Measures information will be discarded.
Coordinate system information should be properly associated with layers.

Currently the OGR Personal Geodatabase driver does not take advantage of
spatial indexes for fast spatial queries, though that may be added in
the future.

By default, SQL statements are passed directly to the MDB database
engine. It's also possible to request the driver to handle SQL commands
with :ref:`OGR SQL <ogr_sql_dialect>` engine, by passing **"OGRSQL"**
string to the ExecuteSQL() method, as name of the SQL dialect.

Driver capabilities
-------------------

.. supports_georeferencing::

How to use PGeo driver with unixODBC and MDB Tools (on Unix and Linux)
----------------------------------------------------------------------

The :ref:`MDB <vector.mdb>` driver is an
alternate way of reading ESRI Personal GeoDatabase .mdb files without
requiring unixODBC and MDB Tools

This article gives step-by-step explanation of how to use OGR with
unixODBC package and how to access Personal Geodatabase with PGeo
driver. See also `GDAL wiki for other
details <http://trac.osgeo.org/gdal/wiki/mdbtools>`__

Prerequisites
~~~~~~~~~~~~~

#. Install `unixODBC <http://www.unixodbc.org>`__ >= 2.2.11
#. Install MDB Tools. While the official upstream of MDB Tools is abandoned and
   contains many bugs which prevent correct parsing of Personal Geodatabase geometry
   data, the maintained fork at `https://github.com/evanmiller/mdbtools <https://github.com/evanmiller/mdbtools>`__
   contains all required fixes to allow GDAL to successfully read Personal Geodatabases.

(On Ubuntu 8.04 : sudo apt-get install unixodbc libmdbodbc)

Configuration
~~~~~~~~~~~~~

There are two configuration files for unixODBC:

-  odbcinst.ini - this file contains definition of ODBC drivers
   available to all users; this file can be found in /etc directory or
   location given as --sysconfdir if you did build unixODBC yourself.
-  odbc.ini - this file contains definition of ODBC data sources (DSN
   entries) available to all users.
-  ~/.odbc.ini - this is the private file where users can put their own
   ODBC data sources.

Editing the odbc.ini files is only required if you want to setup an ODBC
Data Source Name (DSN) so that Personal Geodatabase files can be directly
accessed via DSN. This is entirely optional, as the PGeo driver will automatically
handle the required connection parameters for you if a direct .mdb file name
is used instead.

Format of configuration files is very simple:

::

   [section_name]
   entry1 = value
   entry2 = value

For more details, refer to `unixODBC
manual <http://www.unixodbc.org/doc/>`__.

1. ODBC driver configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

First, you need to configure ODBC driver to access Microsoft Access
databases with MDB Tools. Add following definition to your odbcinst.ini
file.

::

   [Microsoft Access Driver (*.mdb)]
   Description = MDB Tools ODBC drivers
   Driver     = /usr/lib/libmdbodbc.so.0
   Setup      =
   FileUsage  = 1
   CPTimeout  =
   CPReuse    =

-  [Microsoft Access Driver (\*.mdb)] - remember to use "Microsoft Access
   Driver (\*.mdb)" as the name of section because PGeo driver composes
   ODBC connection string for Personal Geodatabase using
   "DRIVER=Microsoft Access Driver (\*.mdb);" string.
-  Description - put short description of this driver definition.
-  Driver - full path of ODBC driver for MDB Tools.

2. ODBC data source configuration (optional)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In this section, I use 'sample.mdb' as a name of Personal Geodatabase,
so replace this name with your own database.

Create .odbc.ini file in your HOME directory:

::

   $ touch ~/.odbc.ini

Put following ODBC data source definition to your .odbc.ini file:

::

   [sample_pgeo]
   Description = Sample PGeo Database
   Driver      = Microsoft Access Driver (*.mdb)
   Database    = /home/mloskot/data/sample.mdb
   Host        = localhost
   Port        = 1360
   User        = mloskot
   Password    =
   Trace       = Yes
   TraceFile   = /home/mloskot/odbc.log

Step by step explanation of DSN entry:

-  [sample_pgeo] - this is name of ODBC data source (DSN). You will
   refer to your Personal Geodatabase using this name. You can use your
   own name here.
-  Description - short description of the DSN entry.
-  Driver - full name of driver defined in step 1. above.
-  Database - full path to .mdb file with your Personal Geodatabase.
-  Host, Port, User and Password entries are not used by MDB Tools
   driver.

Testing PGeo driver with ogrinfo
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Now, you can try to access PGeo data source with ogrinfo.

First, check if you have PGeo driver built in OGR:

::

   $ ogrinfo --formats
   Supported Formats:
     ESRI Shapefile
     ...
     PGeo
     ...

Now, you can access your Personal Geodatabase. If you've setup a DSN for the
Personal Geodatabase (as detailed in section 2 above), the data source should be
PGeo:<DSN> where <DSN> is the name of DSN entry you put to your .odbc.ini.

Alternatively, you can pass a .mdb filename directly to OGR to avoid manual
creation of the DSN.

::

   ogrinfo PGeo:sample_pgeo
   INFO: Open of `PGeo:sample_pgeo'
   using driver `PGeo' successful.
   1. ...

After you run the command above, you should get list of layers stored in
your geodatabase.

Now, you can try to query details of particular layer:

::

   ogrinfo PGeo:sample_pgeo <layer name>
   INFO: Open of `PGeo:sample_pgeo'
   using driver `PGeo' successful.

   Layer name: ...

Resources
---------

-  `About ESRI
   Geodatabase <http://www.esri.com/software/arcgis/geodatabase/index.html>`__
-  `evanmiller's maintained fork of MDB Tools <https://github.com/evanmiller/mdbtools>`__

See also
--------

-  :ref:`MDB <vector.mdb>` driver page
