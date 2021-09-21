.. _vector.geomedia:

Geomedia MDB database
=====================

.. shortname:: Geomedia

.. build_dependencies:: ODBC library

.. deprecated_driver:: version_targeted_for_removal: 3.5
   env_variable: GDAL_ENABLE_DEPRECATED_DRIVER_GEOMEDIA

OGR optionally supports reading Geomedia .mdb files via ODBC. Geomedia
is a Microsoft Access database with a set of tables defined by
Intergraph for holding geodatabase metadata, and with geometry for
features held in a BLOB column in a custom format. This drivers accesses
the database via ODBC but does not depend on any Intergraph middle-ware.

Geomedia .mdb are accessed by passing the file name of the .mdb file to
be accessed as the data source name. On Windows, no ODBC DSN is
required. On Linux, there are problems with DSN-less connection due to
incomplete or buggy implementation of this feature in the `MDB
Tools <http://mdbtools.sourceforge.net/>`__ package, So, it is required
to configure Data Source Name (DSN) if the MDB Tools driver is used
(check instructions below).

In order to facilitate compatibility with different configurations, the
GEOMEDIA_DRIVER_TEMPLATE Config Option was added to provide a way to
programmatically set the DSN programmatically with the filename as an
argument. In cases where the driver name is known, this allows for the
construction of the DSN based on that information in a manner similar to
the default (used for Windows access to the Microsoft Access Driver).

OGR treats all feature tables as layers. Most geometry types should be
supported (arcs are not yet). Coordinate system information is not
currently supported.

Currently the OGR Personal Geodatabase driver does not take advantage of
spatial indexes for fast spatial queries.

By default, SQL statements are passed directly to the MDB database
engine. It's also possible to request the driver to handle SQL commands
with :ref:`OGR SQL <ogr_sql_dialect>` engine, by passing **"OGRSQL"**
string to the ExecuteSQL() method, as name of the SQL dialect.

Driver capabilities
-------------------

.. supports_georeferencing::

How to use Geomedia driver with unixODBC and MDB Tools (on Unix and Linux)
--------------------------------------------------------------------------

The :ref:`MDB <vector.mdb>` driver is an
alternate way of reading Geomedia .mdb files without requiring unixODBC
and MDB Tools

Refer to the similar section of the :ref:`PGeo <vector.pgeo>` driver. The
prefix to use for this driver is Geomedia:

See also
--------

-  :ref:`MDB <vector.mdb>` driver page
