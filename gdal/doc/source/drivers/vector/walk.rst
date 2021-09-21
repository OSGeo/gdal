.. _vector.walk:

Walk - Walk Spatial Data
========================

.. shortname:: Walk

.. build_dependencies:: ODBC library

.. deprecated_driver:: version_targeted_for_removal: 3.5
   env_variable: GDAL_ENABLE_DEPRECATED_DRIVER_WALK

OGR optionally supports reading Walk spatial data via ODBC. Walk spatial
data is a Microsoft Access database developed by Walkinfo Technologies
mainly for land surveying, evaluation, planning, checking and data
analysis in China.

Walk .mdb are accessed by passing the file name of the .mdb file to be
accessed as the data source name. On Windows, no ODBC DSN is required.
On Linux, there are problems with DSN-less connection due to incomplete
or buggy implementation of this feature in the `MDB
Tools <http://mdbtools.sourceforge.net/>`__ package, So, it is required
to configure Data Source Name (DSN) if the MDB Tools driver is used
(check instructions below).

OGR treats all feature tables as layers. Most geometry types should be
supported (arcs and circles are translated into line segments, while
other curves are currently converted into straight lines). Coordinate
system information should be properly associated with layers. Currently
no effort is made to preserve styles and annotations.

Currently the OGR Walk driver does not take advantage of spatial indexes
for fast spatial queries.

By default, SQL statements are handled by :ref:`OGR SQL <ogr_sql_dialect>`
engine. SQL commands can also be passed directly to the ODBC database
engine when SQL dialect is not "OGRSQL". In that case, the queries will
deal with tables (such as "XXXXFeatures", where XXXX is the name of a
layer) instead of layers.

Driver capabilities
-------------------

.. supports_georeferencing::

How to use Walk driver with unixODBC and MDB Tools (on Unix and Linux)
----------------------------------------------------------------------

Refer to the similar section of the :ref:`PGeo <vector.pgeo>` driver. The
prefix to use for this driver is Walk:
