.. _vector.grass:

GRASS Vector Format
===================

.. shortname:: GRASS

.. build_dependencies:: libgrass

GRASS driver can read GRASS (version 6.0 and higher) vector maps. Each
GRASS vector map is represented as one datasource. A GRASS vector map
may have 0, 1 or more layers.

GRASS points are represented as wkbPoint, lines and boundaries as
wkbLineString and areas as wkbPolygon. wkbMulti\* and
wkbGeometryCollection are not used. More feature types can be mixed in
one layer. If a layer contains only features of one type, it is set
appropriately and can be retrieved by OGRLayer::GetLayerDefn();

If a geometry has more categories of the same layer attached, its
represented as more features (one for each category).

Both 2D and 3D maps are supported.

Driver capabilities
-------------------

.. supports_georeferencing::

Datasource name
---------------

Datasource name is full path to 'head' file in GRASS vector directory.
Using names of GRASS environment variables it can be expressed:

::

      $GISDBASE/$LOCATION_NAME/$MAPSET/vector/mymap/head

where 'mymap' is name of a vector map. For example:

::

      /home/cimrman/grass_data/jizerky/jara/vector/liptakov/head

Layer names
-----------

Usually layer numbers are used as layer names. Layer number 0 is used
for all features without any category. It is possible to optionally give
names to GRASS layers linked to database however currently this is not
supported by grass modules. A layer name can be added in 'dbln' vector
file as '/name' after layer number, for example to original record:

::

   1 rivers cat $GISDBASE/$LOCATION_NAME/$MAPSET/dbf/ dbf

it is possible to assign name 'rivers'

::

   1/rivers rivers cat $GISDBASE/$LOCATION_NAME/$MAPSET/dbf/ dbf

the layer 1 will be listed is layer 'rivers'.

Attribute filter
----------------

If a layer has attributes stored in a database, the query is passed to
the underlying database driver. That means, that SQL conditions which
can be used depend on the driver and database to which the layer is
linked. For example, DBF driver has currently very limited set of SQL
expressions and PostgreSQL offers very rich set of SQL expressions.

If a layer has no attributes linked and it has only categories, OGR
internal SQL engine is used to evaluate the expression. Category is an
integer number attached to geometry, it is sort of ID, but it is not FID
as more features in one layer can have the same category.

Evaluation is done once when the attribute filter is set.

Spatial filter
--------------

Bounding boxes of features stored in topology structure are used to
evaluate if a features matches current spatial filter.

Evaluation is done once when the spatial filter is set.

GISBASE
-------

GISBASE is full path to the directory where GRASS is installed. By
default, GRASS driver is using the path given to gdal configure script.
A different directory can be forced by setting GISBASE environment
variable. GISBASE is used to find GRASS database drivers.

Missing topology
----------------

GRASS driver can read GRASS vector files if topology is available (AKA
level 2). If an error is reported, telling that the topology is not
available, it is necessary to build topology within GRASS using v.build
module.

Random access
-------------

If random access (GetFeature instead of GetNextFeature) is used on layer
with attributes, the reading of features can be quite slow. It is
because the driver has to query attributes by category for each feature
(to avoid using a lot of memory) and random access to database is
usually slow. This can be improved on GRASS side optimizing/writing file
based (DBF, SQLite) drivers.

Known problem
-------------

Because of bug in GRASS library, it is impossible to start/stop database
drivers in FIFO order and FILO order must be used. The GRASS driver for
OGR is written with this limit in mind and drivers are always closed if
not used and if a driver remains opened kill() is used to terminate it.
It can happen however in rare cases, that the driver will try to stop
database driver which is not the last opened and an application hangs.
This can happen if sequential read (GetNextFeature) of a layer is not
finished (reading is stopped before last available feature is reached),
features from another layer are read and then the reading of the first
layer is finished, because in that case kill() is not used.

See Also
--------

-  `GRASS GIS home page <http://grass.osgeo.org>`__

--------------

Development of this driver was financially supported by Faunalia
(`www.faunalia.it <http://www.faunalia.it/>`__).
