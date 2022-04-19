.. _vector.shapefile:

ESRI Shapefile / DBF
====================

.. shortname:: ESRI Shapefile

.. built_in_by_default::

All varieties of ESRI Shapefiles should be available for reading, creation and
editing. The driver can also handle standalone
DBF files without associated .shp files.

Normally the OGR Shapefile driver treats a whole directory of shapefiles
as a dataset, and a single shapefile within that directory as a layer.
In this case the directory name should be used as the dataset name.
However, it is also possible to use one of the files (.shp, .shx or
.dbf) in a shapefile set as the dataset name, and then it will be
treated as a dataset with one layer.

Note that when reading a Shapefile of type SHPT_ARC, the corresponding
layer will be reported as of type wkbLineString, but depending on the
number of parts of each geometry, the actual type of the geometry for
each feature can be either OGRLineString or OGRMultiLineString. The same
applies for SHPT_POLYGON shapefiles, reported as layers of type
wkbPolygon, but depending on the number of parts of each geometry, the
actual type can be either OGRPolygon or OGRMultiPolygon.

Measures (M coordinate) are supported. A
Shapefile with measures is created if the specified geometry type is
measured or an appropriate layer creation option is used. When a
shapefile which may have measured geometries is opened, the first shape
is examined and if it uses measures, the geometry type of the layer is
set accordingly. This behavior can be changed with the ADJUST_GEOM_TYPE
open option.

MultiPatch files are read and each patch geometry is turned into a TIN
or a GEOMETRYCOLLECTION of TIN representation for fans and meshes.

If a .prj files in old Arc/Info style or new ESRI OGC WKT style is
present, it will be read and used to associate a projection with
features. Starting with GDAL 2.3, a match will be attempted with the
EPSG databases to identify the SRS of the .prj with an entry in the
catalog.

The read driver assumes that multipart polygons follow the
specification, that is to say the vertices of outer rings should be
oriented clockwise on the X/Y plane, and those of inner rings
counterclockwise. If a Shapefile is broken w.r.t. that rule, it is
possible to define the configuration option
:decl_configoption:`OGR_ORGANIZE_POLYGONS` to DEFAULT to proceed to 
a full analysis based on topological relationships of the parts of the 
polygons so that the resulting polygons are correctly defined in the 
OGC Simple Feature convention.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Encoding
--------

An attempt is made to read the code page setting in the .cpg file, or as
a fallback in the LDID/codepage setting from the .dbf file, and use it
to translate string fields to UTF-8 on read, and back when writing. LDID
"87 / 0x57" is treated as ISO-8859-1 which may not be appropriate. The
:decl_configoption:`SHAPE_ENCODING` configuration option may be used to
override the encoding interpretation of the shapefile with any encoding
supported by CPLRecode or to "" to avoid any recoding.

Starting with GDAL 3.1, the following metadata items are available in the
"SHAPEFILE" domain:

-  **LDID_VALUE**\ =integer: Raw LDID value from the DBF header. Only present
   if this value is not zero.
-  **ENCODING_FROM_LDID**\ =string: Encoding name deduced from LDID_VALUE. Only
   present if LDID_VALUE is present
-  **CPG_VALUE**\ =string: Content of the .cpg file. Only present if the file
   exists.
-  **ENCODING_FROM_CPG**\ =string: Encoding name deduced from CPG_VALUE. Only
   present if CPG_VALUE is present
-  **SOURCE_ENCODING**\ =string: Encoding used by GDAL to encode/recode strings.
   If the user has provided the :decl_configoption:`SHAPE_ENCODING` 
   configuration option or ``ENCODING`` open option have been provided 
   (included to empty value), then their value is used to fill this metadata 
   item. Otherwise it is equal to ENCODING_FROM_CPG if it is present. 
   Otherwise it is equal to ENCODING_FROM_LDID.

Spatial and attribute indexing
------------------------------

The OGR Shapefile driver supports spatial indexing and a limited form of
attribute indexing.

The spatial indexing uses the same .qix quadtree spatial index files
that are used by UMN MapServer. Spatial indexing can accelerate
spatially filtered passes through large datasets to pick out a small
area quite dramatically.

It can also use the ESRI spatial index files
(.sbn / .sbx), but writing them is not supported currently.

To create a spatial index (in .qix format), issue a SQL command of the
form

::

   CREATE SPATIAL INDEX ON tablename [DEPTH N]

where optional DEPTH specifier can be used to control number of index
tree levels generated. If DEPTH is omitted, tree depth is estimated on
basis of number of features in a shapefile and its value ranges from 1
to 12.

To delete a spatial index issue a command of the form

::

   DROP SPATIAL INDEX ON tablename

Otherwise, the `MapServer <http://mapserver.org>`__ shptree utility can
be used:

::

   shptree <shpfile> [<depth>] [<index_format>]

More information is available about this utility at the `MapServer
shptree page <http://mapserver.org/utilities/shptree.html>`__

Currently the OGR Shapefile driver only supports attribute indexes for
looking up specific values in a unique key column. To create an
attribute index for a column issue an SQL command of the form "CREATE
INDEX ON tablename USING fieldname". To drop the attribute indexes issue
a command of the form "DROP INDEX ON tablename". The attribute index
will accelerate WHERE clause searches of the form "fieldname = value".
The attribute index is actually stored as a mapinfo format index and is
not compatible with any other shapefile applications.

Creation Issues
---------------

The Shapefile driver treats a directory as a dataset, and each Shapefile
set (.shp, .shx, and .dbf) as a layer. The dataset name will be treated
as a directory name. If the directory already exists it is used and
existing files in the directory are ignored. If the directory does not
exist it will be created.

As a special case attempts to create a new dataset with the extension
.shp will result in a single file set being created instead of a
directory.

ESRI shapefiles can only store one kind of geometry per layer
(shapefile). On creation this is may be set based on the source file (if
a uniform geometry type is known from the source driver), or it may be
set directly by the user with the layer creation option SHPT (shown
below). If not set the layer creation will fail. If geometries of
incompatible types are written to the layer, the output will be
terminated with an error.

Note that this can make it very difficult to translate a mixed geometry
layer from another format into Shapefile format using ogr2ogr, since
ogr2ogr has no support for separating out geometries from a source
layer. See the
`FAQ <http://trac.osgeo.org/gdal/wiki/FAQVector#HowdoItranslateamixedgeometryfiletoshapefileformat>`__
for a solution.

Shapefile feature attributes are stored in an associated .dbf file, and
so attributes suffer a number of limitations:

-  Attribute names can only be up to 10 characters long.
   The OGR Shapefile driver tries to generate unique field
   names. Successive duplicate field names, including those created by
   truncation to 10 characters, will be truncated to 8 characters and
   appended with a serial number from 1 to 99.

   For example:

   -  a → a, a → a_1, A → A_2;
   -  abcdefghijk → abcdefghij, abcdefghijkl → abcdefgh_1

-  Only Integer, Integer64, Real, String and Date (not DateTime, just
   year/month/day) field types are supported. The various list, and
   binary field types cannot be created.

-  The field width and precision are directly used to establish storage
   size in the .dbf file. This means that strings longer than the field
   width, or numbers that don't fit into the indicated field format will
   suffer truncation.

-  Integer fields without an explicit width are treated as width 9, and
   extended to 10 or 11 if needed.

-  Integer64 fields without an explicit width are treated as width 18,
   and extended to 19 or 20 if needed.

-  Real (floating point) fields without an explicit width are treated as
   width 24 with 15 decimal places of precision.

-  String fields without an assigned width are treated as 80 characters.

Also, .dbf files are required to have at least one field. If none are
created by the application an "FID" field will be automatically created
and populated with the record number.

The OGR shapefile driver supports rewriting existing shapes in a
shapefile as well as deleting shapes. Deleted shapes are marked for
deletion in the .dbf file, and then ignored by OGR. To actually remove
them permanently (resulting in renumbering of FIDs) invoke the SQL
'REPACK <tablename>' via the datasource ExecuteSQL() method.

REPACK will also result in .shp being rewritten
if a feature geometry has been modified with SetFeature() and resulted
in a change of the size the binary encoding of the geometry in the .shp
file.

Starting with GDAL 2.2, REPACK is also done automatically at file
closing, or at FlushCache()/SyncToDisk() time, since shapefiles with
holes can cause interoperability issues with other software.

Field sizes
-----------

The driver knows to auto-extend string and
integer fields (up to the 255 bytes limit imposed by the DBF format) to
dynamically accommodate for the length of the data to be inserted.

It is also possible to force a resize of the fields to the optimal width
by issuing a SQL 'RESIZE <tablename>' via the datasource ExecuteSQL()
method. This is convenient in situations where the default column width
(80 characters for a string field) is bigger than necessary.

Spatial extent
--------------

Shapefiles store the layer spatial extent in the .SHP file. The layer
spatial extent is automatically updated when inserting a new feature in
a shapefile. However when updating an existing feature, if its previous
shape was touching the bounding box of the layer extent but the updated
shape does not touch the new extent, the computed extent will not be
correct. It is then necessary to force a recomputation by invoking the
SQL 'RECOMPUTE EXTENT ON <tablename>' via the datasource ExecuteSQL()
method. The same applies for the deletion of a shape.

Size Issues
-----------

Geometry: The Shapefile format explicitly uses 32bit offsets and so
cannot go over 8GB (it actually uses 32bit offsets to 16bit words), but
the OGR shapefile implementation has a limitation to 4GB.

Attributes: The dbf format does not have any offsets in it, so it can be
arbitrarily large.

However, for compatibility with other software implementation, it is not
recommended to use a file size over 2GB for both .SHP and .DBF files.

The 2GB_LIMIT=YES layer creation option can be used to strictly enforce that 
limit. For update mode, the :decl_configoption:`SHAPE_2GB_LIMIT` 
configuration option can be set to YES for similar effect. If nothing is set, 
a warning will be emitted when the 2GB limit is reached.

Compressed files
----------------

Starting with GDAL 3.1, the driver can also support reading, creating and
editing .shz files (ZIP files containing the .shp, .shx, .dbf and other side-car
files of a single layer) and .shp.zip files (ZIP files contains one or several
layers). Creation and editing involves the creation of temporary files.

Open options
------------

The following open options are available.

-  **ENCODING**\ =encoding_name: to override the encoding interpretation
   of the shapefile with any encoding supported by CPLRecode or to "" to
   avoid any recoding
-  **DBF_DATE_LAST_UPDATE=**\ *YYYY-MM-DD*: Modification date to write
   in DBF header with year-month-day format. If not specified, current
   date is used.
-  **ADJUST_TYPE**\ =YES/NO: Set to YES (default is NO) to read the
   whole .dbf to adjust Real->Integer/Integer64 or Integer64->Integer
   field types when possible. This can be used when field widths are
   ambiguous and that by default OGR would select the larger data type.
   For example, a numeric column with 0 decimal figures and with width
   of 10/11 character may hold Integer or Integer64, and with width
   19/20 may hold Integer64 or larger integer (hold as Real)
-  **ADJUST_GEOM_TYPE**\ =NO/FIRST_SHAPE/ALL_SHAPES. (Starting with GDAL
   2.1) Defines how layer geometry type is computed, in particular to
   distinguish shapefiles that have shapes with significant values in
   the M dimension from the ones where the M values are set to the
   nodata value. By default (FIRST_SHAPE), the driver will look at the
   first shape and if it has M values it will expose the layer as having
   a M dimension. By specifying ALL_SHAPES, the driver will iterate over
   features until a shape with a valid M value is found to decide the
   appropriate layer type.
-  **AUTO_REPACK=**\ *YES/NO*: (OGR >= 2.2) Default to YES in GDAL 2.2.
   Whether the shapefile should be automatically repacked when needed,
   at dataset closing or at FlushCache()/SyncToDisk() time.
-  **DBF_EOF_CHAR=**\ *YES/NO*: (OGR >= 2.2) Default to YES in GDAL 2.2.
   Whether the .DBF should be terminated by a 0x1A end-of-file
   character, as in the DBF spec and done by other software vendors.
   Previous GDAL versions did not write one.

Dataset creation options
------------------------

None

Layer creation options
----------------------

-  **SHPT=type**: Override the type of shapefile created. Can be one of
   NULL for a simple .dbf file with no .shp file, POINT, ARC, POLYGON or
   MULTIPOINT for 2D; POINTZ, ARCZ, POLYGONZ, MULTIPOINTZ or MULTIPATCH
   for 3D; POINTM, ARCM, POLYGONM or MULTIPOINTM for measured
   geometries; and POINTZM, ARCZM, POLYGONZM or MULTIPOINTZM for 3D
   measured geometries. The measure support was added in GDAL 2.1.
   MULTIPATCH files are supported since GDAL 2.2.
-  **ENCODING=**\ *value*: set the encoding value in the DBF file. The
   default value is "LDID/87". It is not clear what other values may be
   appropriate.
-  **RESIZE=**\ *YES/NO*: set the YES to resize fields
   to their optimal size. See above "Field sizes" section. Defaults to
   NO.
-  **2GB_LIMIT=**\ *YES/NO*: set the YES to enforce the
   2GB file size for .SHP or .DBF files. Defaults to NO.
-  **SPATIAL_INDEX=**\ *YES/NO*: set the YES to create a
   spatial index (.qix). Defaults to NO.
-  **DBF_DATE_LAST_UPDATE=**\ *YYYY-MM-DD*: Modification
   date to write in DBF header with year-month-day format. If not
   specified, current date is used. Note: behavior of past GDAL
   releases was to write 1995-07-26
-  **AUTO_REPACK=**\ *YES/NO*: (OGR >= 2.2) Default to YES in GDAL 2.2.
   Whether the shapefile should be automatically repacked when needed,
   at dataset closing or at FlushCache()/SyncToDisk() time.
-  **DBF_EOF_CHAR=**\ *YES/NO*: (OGR >= 2.2) Default to YES in GDAL 2.2.
   Whether the .DBF should be terminated by a 0x1A end-of-file
   character, as in the DBF spec and done by other software vendors.
   Previous GDAL versions did not write one.

Configuration options
---------------------

The following :ref:`configuration options <configoptions>` are 
available:

- :decl_configoption:`SHAPE_REWIND_ON_WRITE` can be set to NO to prevent the 
  shapefile writer to correct the winding order of exterior/interior rings to 
  be conformant with the one mandated by the Shapefile specification. This can 
  be useful in some situations where a MultiPolygon passed to the shapefile 
  writer is not really a compliant Single Feature polygon, but originates from 
  example from a MultiPatch object (from a Shapefile/FileGDB/PGeo datasource).

- :decl_configoption:`SHAPE_RESTORE_SHX` (GDAL >= 2.1): can be set to YES 
  (default NO) to restore broken or absent .shx file from associated .shp file 
  during opening.

- :decl_configoption:`SHAPE_2GB_LIMIT` can be set to YES to strictly enforce 
  the 2 GB file size limit when updating a shapefile. If nothing is set, a 
  warning will be emitted when the 2 GB limit is reached.

- :decl_configoption:`OGR_ORGANIZE_POLYGONS` can be set to DEFAULT to activate 
  a full analysis based on topological relationships of the parts of the 
  polygons to make sure that the ring ordering of all polygons are correct 
  according to the OGC Simple Feature convention.

- :decl_configoption:`SHAPE_ENCODING` may be used to override the encoding 
  interpretation of the shapefile with any encoding supported by CPLRecode 
  or to "" to avoid any recoding.

Examples
--------

-  A merge of two shapefiles 'file1.shp' and 'file2.shp' into a new file
   'file_merged.shp' is performed like this:

   ::

      ogr2ogr file_merged.shp file1.shp
      ogr2ogr -update -append file_merged.shp file2.shp -nln file_merged

   The second command is opening file_merged.shp in update mode, and
   trying to find existing layers and append the features being copied.

   The -nln option sets the name of the layer to be copied to.

-  Building a spatial index :

   ::

      ogrinfo file1.shp -sql "CREATE SPATIAL INDEX ON file1"

-  Resizing columns of a DBF file to their optimal size
   :

   ::

      ogrinfo file1.dbf -sql "RESIZE file1"

See Also
--------

-  `Shapelib Page <http://shapelib.maptools.org/>`__
-  `User Notes on OGR Shapefile
   Driver <http://trac.osgeo.org/gdal/wiki/UserDocs/Shapefiles>`__
