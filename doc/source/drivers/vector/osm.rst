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

In the *data* folder of the GDAL distribution, you can find an
:source_file:`ogr/ogrsf_frmts/osm/data/osmconf.ini`
file that can be customized to fit your needs. You can also define an
alternate path with the :config:`OSM_CONFIG_FILE` configuration option.

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
The ``hstore_get_value()`` function can be used with the :ref:`OGRSQL <ogr_sql_dialect>`
or :ref:`SQLite <sql_sqlite_dialect>` SQL dialects to extract the value of a
given key.

For example :

::

   ogr2ogr -f PostgreSQL "PG:dbname=osm" test.pbf \
   -lco COLUMN_TYPES=other_tags=hstore

Starting with GDAL 3.7, it is possible to ask the format of this field to
be JSON encoded, by using the TAGS_FORMAT=JSON open option, or the
``tags_format=json`` setting in the *osmconf.ini* file.

"all_tags" field
~~~~~~~~~~~~~~~~

Similar to "other_tags", except that it contains both keys specifically
identified to be reported as dedicated fields, as well as other keys.

"all_tags" is disabled by default, and when enabled, it is exclusive
with "other_tags".


Configuration options
---------------------

|about-config-options|
The following configuration options are available:

-  .. config:: OSM_CONFIG_FILE
      :choices: <filename>

      Path to an OSM configuration file. See `Configuration`_.

-  .. config:: OSM_MAX_TMPFILE_SIZE
      :choices: <MB>
      :default: 100

      Size of the internal SQLite database used to resolve geometries.
      If that database remains under the size specified by this option,
      it will reside in RAM. If it grows
      above, it will be written in a temporary file on disk. By default, this
      file will be written in the current directory, unless you define the
      :config:`CPL_TMPDIR` configuration option.

-  .. config:: OSM_USE_CUSTOM_INDEXING
      :choices: YES, NO
      :default: YES

      For indexation of nodes, a custom mechanism not relying on SQLite is
      used by default (indexation of ways to solve relations is still relying
      on SQLite). It can speed up operations significantly. However, in some
      situations (non increasing node ids, or node ids not in expected range),
      it might not work and the driver will output an error message suggesting
      to relaunch by setting this configuration option to NO.

-  .. config:: OSM_COMPRESS_NODES
      :choices: YES, NO
      :default: NO

      When custom indexing is used (:config:`OSM_USE_CUSTOM_INDEXING=YES`, default case),
      the :config:`OSM_COMPRESS_NODES`
      configuration option can be set to YES. This option
      might be turned on to improve performance when I/O access is the
      limiting factor (typically the case of rotational disk), and will be
      mostly efficient for country-sized OSM extracts where compression rate
      can go up to a factor of 3 or 4, and help keep the node DB to a size
      that fit in the OS I/O caches. For whole planet file, the effect of this
      option will be less efficient. This option consumes additional 60 MB of
      RAM.

-  .. config:: OGR_INTERLEAVED_READING

      See `Interleaved reading`_.


Interleaved reading
-------------------

Due to the nature of OSM files and how the driver works internally, the
default reading mode that works per-layer might not work correctly,
because too many features will accumulate in the layers before being
consumed by the user application.

Starting with GDAL 2.2, applications should use the
``GDALDataset::GetNextFeature()`` API to iterate over features in the order
they are produced.

For earlier versions, for large files, applications should set the
:config:`OGR_INTERLEAVED_READING=YES` configuration option to turn on a special
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
:config:`OGR_INTERLEAVED_READING` mode without any
particular user action.

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

   wget -O - http://www.example.com/some.pbf | 
   ogr2ogr -f SQLite my.sqlite /vsistdin/

   or

   ogr2ogr -f SQLite my.sqlite \
   /vsicurl_streaming/http://www.example.com/some.pbf -progress

And to combine the above steps :

::

   wget -O - http://www.example.com/some.osm.bz2 | 
   bzcat | ogr2ogr -f SQLite my.sqlite /vsistdin/

Open options
------------

|about-open-options|
The following open options are supported:

-  .. oo:: CONFIG_FILE
      :choices: <filename>
      :default: {GDAL_DATA}/osmconf.ini

      Configuration filename.

-  .. oo:: USE_CUSTOM_INDEXING
      :choices: YES, NO
      :default: YES

      Whether to enable custom indexing.

-  .. oo:: COMPRESS_NODES
      :choices: YES, NO
      :default: NO

      Whether to compress nodes in temporary DB.

-  .. oo:: MAX_TMPFILE_SIZE
      :choices: <MBytes>
      :default: 100

      Maximum size in MB of
      in-memory temporary file. If it exceeds that value, it will go to
      disk.

-  .. oo:: INTERLEAVED_READING
      :choices: YES, NO
      :default: NO

      Whether to enable interleaved reading.

-  .. oo:: TAGS_FORMAT
      :choices: HSTORE, JSON
      :default: HSTORE
      :since: 3.7

      Format for all_tags/other_tags fields.

See Also
--------

-  `OpenStreetMap home page <http://www.openstreetmap.org/>`__
-  `OSM XML Format
   description <http://wiki.openstreetmap.org/wiki/OSM_XML>`__
-  `OSM PBF Format
   description <http://wiki.openstreetmap.org/wiki/PBF_Format>`__
