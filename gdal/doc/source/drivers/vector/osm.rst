.. _vector.osm:

OSM - OpenStreetMap XML and PBF
===============================

.. shortname:: OSM

.. build_dependencies:: libsqlite3 (and libexpat for OSM XML)

This driver reads OpenStreetMap files, in .osm (XML based) and .pbf
(optimized binary) formats.

The driver is available if GDAL is built with SQLite support and, for
.osm XML files, with Expat support.

The filenames must end with .osm or .pbf extension.

The driver will categorize features into 5 layers :

-  **points** : "node" features that have significant tags attached.
-  **lines** : "way" features that are recognized as non-area.
-  **multilinestrings** : "relation" features that form a
   multilinestring(type = 'multilinestring' or type = 'route').
-  **multipolygons** : "relation" features that form a multipolygon
   (type = 'multipolygon' or type = 'boundary'), and "way" features that
   are recognized as area.
-  **other_relations** : "relation" features that do not belong to the
   above 2 layers.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Configuration
-------------

In the *data* folder of the GDAL distribution, you can find a
`osmconf.ini <https://github.com/OSGeo/gdal/blob/master/gdal/data/osmconf.ini>`__
file that can be customized to fit your needs. You can also define an
alternate path with the :decl_configoption:`OSM_CONFIG_FILE` configuration option.

The customization is essentially which OSM attributes and keys should be
translated into OGR layer fields.

Fields can be computed with SQL expressions
(evaluated by SQLite engine) from other fields/tags. For example to
compute the z_order attribute.

"other_tags" field
~~~~~~~~~~~~~~~~~~

When keys are not strictly identified in the *osmconf.ini* file, the
key/value pair is appended in a "other_tags" field, with a syntax
compatible with the PostgreSQL HSTORE type. See the *COLUMN_TYPES* layer
creation option of the :ref:`PG driver <vector.pg>`.

For example :

::

   ogr2ogr -f PostgreSQL "PG:dbname=osm" test.pbf -lco COLUMN_TYPES=other_tags=hstore

"all_tags" field
~~~~~~~~~~~~~~~~

Similar to "other_tags", except that it contains both keys specifically
identified to be reported as dedicated fields, as well as other keys.

"all_tags" is disabled by default, and when enabled, it is exclusive
with "other_tags".

Internal working and performance tweaking
-----------------------------------------

The driver will use an internal SQLite database to resolve geometries.
If that database remains under 100 MB it will reside in RAM. If it grows
above, it will be written in a temporary file on disk. By default, this
file will be written in the current directory, unless you define the
:decl_configoption:`CPL_TMPDIR` configuration option. The 100 MB default threshold can be
adjusted with the :decl_configoption:`OSM_MAX_TMPFILE_SIZE` configuration option (value in
MB).

For indexation of nodes, a custom mechanism not relying on SQLite is
used by default (indexation of ways to solve relations is still relying
on SQLite). It can speed up operations significantly. However, in some
situations (non increasing node ids, or node ids not in expected range),
it might not work and the driver will output an error message suggesting
to relaunch by defining the :decl_configoption:`OSM_USE_CUSTOM_INDEXING` configuration option
to NO.

When custom indexing is used (default case), the :decl_configoption:`OSM_COMPRESS_NODES`
configuration option can be set to YES (the default is NO). This option
might be turned on to improve performances when I/O access is the
limiting factor (typically the case of rotational disk), and will be
mostly efficient for country-sized OSM extracts where compression rate
can go up to a factor of 3 or 4, and help keep the node DB to a size
that fit in the OS I/O caches. For whole planet file, the effect of this
option will be less efficient. This option consumes addionnal 60 MB of
RAM.

Interleaved reading
-------------------

Due to the nature of OSM files and how the driver works internally, the
default reading mode that works per-layer might not work correctly,
because too many features will accumulate in the layers before being
consumed by the user application.

Starting with GDAL 2.2, applications should use the
GDALDataset::GetNextFeature() API to iterate over features in the order
they are produced.

For earlier versions, for large files, applications should set the
OGR_INTERLEAVED_READING=YES configuration option to turn on a special
reading mode where the following reading pattern must be used:

::

       bool bHasLayersNonEmpty;
       do
       {
           bHasLayersNonEmpty = false;

           for( int iLayer = 0; iLayer < poDS->GetLayerCount(); iLayer++ )
           {
               OGRLayer *poLayer = poDS->GetLayer(iLayer);

               OGRFeature* poFeature;
               while( (poFeature = poLayer->GetNextFeature()) != NULL )
               {
                   bHasLayersNonEmpty = true;
                   OGRFeature::DestroyFeature(poFeature);
               }
           }
       }
       while( bHasLayersNonEmpty );

Note : the ogr2ogr application has been modified to use that
OGR_INTERLEAVED_READING mode without any particular user action.

Spatial filtering
-----------------

Due to way .osm or .pbf files are structured and the parsing of the file
is done, for efficiency reasons, a spatial filter applied on the points
layer will also affect other layers. This may result in lines or
polygons that have missing vertices.

To improve this, a possibility is using a larger spatial filter with
some buffer for the points layer, and then post-process the output to
apply the desired filter. This would not work however if a polygon has
vertices very far away from the interest area. In which case full
conversion of the file to another format, and filtering of the resulting
lines or polygons layers would be needed.

Reading .osm.bz2 files and/or online files
------------------------------------------

.osm.bz2 are not natively recognized, however you can process them (on
Unix), with the following command :

::

   bzcat my.osm.bz2 | ogr2ogr -f SQLite my.sqlite /vsistdin/

You can convert a .osm or .pbf file without downloading it :

::

   wget -O - http://www.example.com/some.pbf | ogr2ogr -f SQLite my.sqlite /vsistdin/

   or

   ogr2ogr -f SQLite my.sqlite /vsicurl_streaming/http://www.example.com/some.pbf -progress

And to combine the above steps :

::

   wget -O - http://www.example.com/some.osm.bz2 | bzcat | ogr2ogr -f SQLite my.sqlite /vsistdin/

Open options
------------

-  **CONFIG_FILE=filename**: Configuration filename.
   Defaults to {GDAL_DATA}/osmconf.ini.
-  **USE_CUSTOM_INDEXING=YES/NO**: Whether to enable custom
   indexing. Defaults to YES.
-  **COMPRESS_NODES=YES/NO**: Whether to compress nodes in
   temporary DB. Defaults to NO.
-  **MAX_TMPFILE_SIZE=int_val**: Maximum size in MB of
   in-memory temporary file. If it exceeds that value, it will go to
   disk. Defaults to 100.
-  **INTERLEAVED_READING=YES/NO**: Whether to enable
   interleaved reading. Defaults to NO.

See Also
--------

-  `OpenStreetMap home page <http://www.openstreetmap.org/>`__
-  `OSM XML Format
   description <http://wiki.openstreetmap.org/wiki/OSM_XML>`__
-  `OSM PBF Format
   description <http://wiki.openstreetmap.org/wiki/PBF_Format>`__
