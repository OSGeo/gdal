.. _vector.vfk:

VFK - Czech Cadastral Exchange Data Format
==========================================

.. shortname:: VFK

.. build_dependencies:: libsqlite3

This driver reads VFK files, i.e. data in the *Czech cadastral exchange
data format*. The VFK file is recognized as an datasource with zero or
more layers.

The driver is compiled only if GDAL is *built with SQLite support*.

Points are represented as wkbPoints, lines and boundaries as
wkbLineStrings and areas as wkbPolygons. wkbMulti\* features are not
used. Feature types cannot be mixed in one layer.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Open options
------------

Starting with GDAL 2.3, the following open options can be specified
(typically with the ``-oo name=value`` parameters of ogrinfo or ogr2ogr):

-  **SUPPRESS_GEOMETRY**\ =YES/NO (defaults to NO). Setting it to YES
   will skip resolving geometry. All layers will be recognized with no
   geometry type. Mostly useful when user is interested at attributes
   only. Note that suppressing geometry can cause significant
   performance gain when reading input VFK data by the driver.
-  **FILE_FIELD**\ =YES/NO (defaults to NO). Setting it to YES will
   append new field *VFK_FILENAME* containing name of source VFK file to
   all layers.

Configuration options
~~~~~~~~~~~~~~~~~~~~~

(set with ``--config key value`` on GDAL command line utilities)

The driver uses SQLite as a backend database
when reading VFK data. By default, SQLite database is created in a
directory of input VFK file (with file extension '.db').
The user can define DB name with **OGR_VFK_DB_NAME** configuration
option. If **OGR_VFK_DB_OVERWRITE=YES** configuration option is given,
the driver overwrites existing SQLite database and stores data read from
input VFK file into newly created DB. If
**OGR_VFK_DB_DELETE=YES** configuration option is given, the driver
deletes backend SQLite database when closing the datasource.

Resolved geometries are stored also in backend
SQLite database. It means that geometries are resolved only once when
building SQLite database from VFK data. Geometries are stored in WKB
format. Note that GDAL doesn't need to be built with SpatiaLite support.
Geometries are not stored in DB when **OGR_VFK_DB_SPATIAL=NO**
configuration option is given. In this case geometries are resolved when
reading data from DB on the fly.

Internal working and performance tweaking
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If backend SQLite database already exists then the driver reads features
directly from the database and not from input VFK file given as an input
datasource. This causes significant performance gain when reading
features by the driver.

The driver reads by default all data blocks from VFK
file when building backend SQLite database. When configuration option
**OGR_VFK_DB_READ_ALL_BLOCKS=NO** is given, the driver reads only data
blocks which are requested by the user. This can be useful when the user
want to process only part of VFK data.

Datasource name
---------------

Datasource name is a full path to the VFK file.

The driver supports reading files managed by VSI Virtual File System
API, which include "regular" files, as well as files in the /vsizip/,
/vsigzip/, and /vsicurl/ read-only domains.

Since GDAL 2.2 also a full path to the backend SQLite database can be
used as an datasource. By default, such datasource is read by SQLite
driver. If configuration option **OGR_VFK_DB_READ=YES** is given, such
datasource is open by VFK driver instead.

Layer names
-----------

VFK data blocks are used as layer names.

Filters
-------

Attribute filter
~~~~~~~~~~~~~~~~

An internal SQL engine is used to evaluate the expression. Evaluation is
done once when the attribute filter is set.

Spatial filter
~~~~~~~~~~~~~~

Bounding boxes of features stored in topology structure are used to
evaluate if a features matches current spatial filter. Evaluation is
done once when the spatial filter is set.

References
----------

-  `OGR VFK Driver Implementation
   Issues <http://geo.fsv.cvut.cz/~landa/publications/2010/gis-ostrava-2010/paper/landa-ogr-vfk.pdf>`__
-  `Open Source Tools for VFK
   format <http://freegis.fsv.cvut.cz/gwiki/VFK>`__ (in Czech)
-  `Czech cadastral exchange data format
   documentation <http://www.cuzk.cz/Dokument.aspx?PRARESKOD=998&MENUID=0&AKCE=DOC:10-VF_ISKNTEXT>`__
   (in Czech)
