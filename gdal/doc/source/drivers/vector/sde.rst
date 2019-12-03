.. _vector.sde:

ESRI ArcSDE
===========

.. shortname:: SDE

.. build_dependencies:: ESRI SDE

OGR optionally supports reading ESRI ArcSDE database instances. ArcSDE
is a middleware spatial solution for storing spatial data in a variety
of backend relational databases. The OGR ArcSDE driver depends on being
built with the ESRI provided ArcSDE client libraries.

ArcSDE instances are accessed with a datasource name of the following
form. The server, instance, username and password fields are required.
The instance is the port number of the SDE server, which generally
defaults to 5151. If the layer parameter is specified then the SDE
driver is able to skip reading the summary metadata for each layer;
skipping this step can be a significant time savings.

**Note**: Only GDAL 1.6+ supports querying against versions and write
operations. Older versions only support querying against the base
(SDE.DEFAULT) version and no writing operations.

::

     SDE:server,instance,database,username,password[,layer]

To specify a version to query against, you \*must\* specify a layer as
well. The SDE.DEFAULT version will be used when no version name is
specified.

::

     SDE:server,instance,database,username,password,layer,[version]

You can also request to create a new version if it does not already
exist. If the child version already exists, it will be used unless the
SDE_VERSIONOVERWRITE environment variable is set to "TRUE". In that
case, the version will be deleted and recreated.

::

     SDE:server,instance,database,username,password,layer,[parentversion],[childversion]

The OGR ArcSDE driver does not support reading CAD data (treated as BLOB
attribute), annotation properties, measure values at vertices, or raster
data. The ExecuteSQL() method does **not** get passed through to the
underlying database. For now it is interpreted by the limited OGR SQL
handler. Spatial indexes are used to accelerate spatial queries.

The driver has been tested with ArcSDE 9.x, and should work with newer
versions, as well as ArcSDE 8.2 or 8.3. Both 2D and 3D geometries are
supported. Curve geometries are approximated as line strings (actually
still TODO).

ArcSDE is generally sensitive to case-specific, fully-qualified
tablenames. While you may be able to use short names for some
operations, others (notably deleting) will require a fully-qualified
name. Because of this fact, it is generally best to **always** use
fully-qualified table names.

Layer Creation Options
~~~~~~~~~~~~~~~~~~~~~~

-  **OVERWRITE**: This can be set to allow an existing layer to be
   overwritten during the layer creation process. If set, and the value
   is not "NO", the layer will first be deleted prior to creating a new
   layer of the same name as an existing layer. Set to "NO" explicitly,
   or do not include the option to treat attempts to create new layers
   which collide with existing layers of the same name as an error. Off
   by default.
-  **GEOMETRY_NAME**: By default OGR creates new layers with the
   geometry (feature) column named \`SHAPE'. If you wish to use a
   different name, it can be supplied with the GEOMETRY_NAME layer
   creation option.
-  **SDE_FID**: Can be set to override the default name of the feature
   ID column. The default is "OBJECTID".
-  **SDE_KEYWORD**: The DBTUNE keyword with which to create the layer.
   Defaults to "DEFAULTS".
-  **SDE_DESCRIPTION**: The text description of the layer. Defaults to
   "Created by GDAL/OGR 1.6" (Also used as the version description when
   creating a new child version from a parent version.)
-  **SDE_MULTIVERSION**: If this creation option is set is set to
   "FALSE", multi-versioning will be disabled for the layer at creation
   time. By default, multiversion tables are created when layers are
   created on an SDE datasource.
-  **USE_NSTRING**: If this option is set to "TRUE" then string fields
   will be created as type NSTRING. This option was added for GDAL/OGR
   1.9.0.

Environment variables
~~~~~~~~~~~~~~~~~~~~~

-  **OGR_SDE_GETLAYERTYPE**: This may be "TRUE" to determine the
   geometry type from the database. Otherwise, the SDE driver will
   always return an Unknown geometry type.
-  **OGR_SDE_SEARCHORDER**: This may be "ATTRIBUTE_FIRST" to tell ArcSDE
   to filter based on attributes \*before\* using a spatial filter or
   "SPATIAL_FIRST" to use the spatial filter. By default, it uses the
   spatial filter first.
-  **SDE_VERSIONOVERWRITE**: If set to "TRUE", the specified child
   version will be deleted before being recreated. Note that this action
   does nothing to reconcile any edits that existed on that version
   before doing so and essentially throws them away.
-  **OGR_SDE_USE_NSTRING**: If this option is set to "TRUE" then string
   fields will be created as type NSTRING. This option was added for
   GDAL/OGR 1.9.0.
-  

Examples
~~~~~~~~

See the
`ogr_sde.py <http://trac.osgeo.org/gdal/browser/trunk/autotest/ogr/ogr_sde.py>`__
test script for some example connection strings and usage of the driver.
