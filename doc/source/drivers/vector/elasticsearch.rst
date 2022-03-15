.. _vector.elasticsearch:

Elasticsearch: Geographically Encoded Objects for Elasticsearch
===============================================================

.. shortname:: Elasticsearch

.. build_dependencies:: libcurl

| Driver is read-write starting with GDAL 2.1
| As of GDAL 2.1, Elasticsearch 1.X and, partially, 2.X versions are
  supported (5.0 known not to work). GDAL 2.2 adds supports for
  Elasticsearch 2.X and 5.X

`Elasticsearch <http://elasticsearch.org/>`__ is an Enterprise-level
search engine for a variety of data sources. It supports full-text
indexing and geospatial querying of those data in a fast and efficient
manor using a predefined REST API.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

Opening dataset name syntax
---------------------------

Starting with GDAL 2.1, the driver supports reading existing indices
from a Elasticsearch host. There are two main possible syntaxes to open
a dataset:

-  Using *ES:http://hostname:port* (where port is typically 9200)
-  Using *ES:* with the open options to specify HOST and PORT

The open options available are :

-  **HOST**\ =hostname: Server hostname. Default to localhost.
-  **PORT**\ =port. Server port. Default to 9200.
-  **USERPWD**\ =user:password. (GDAL >=2.4) Basic authentication as
   username:password.
-  **LAYER**\ =name. (GDAL >=2.4) Index name or index_mapping to use for
   restricting layer listing.
-  **BATCH_SIZE**\ =number. Number of features to retrieve per batch.
   Default is 100.
-  **FEATURE_COUNT_TO_ESTABLISH_FEATURE_DEFN**\ =number. Number of
   features to retrieve to establish feature definition. -1 = unlimited.
   Defaults to 100.
-  **SINGLE_QUERY_TIMEOUT**\ =number. (GDAL >= 3.2.1)
   Timeout in second (as floating point number) for requests such as
   GetFeatureCount() or GetExtent(). Defaults to unlimited.
-  **SINGLE_QUERY_TERMINATE_AFTER**\ =number. (GDAL >= 3.2.1)
   Maximum number of documents to collect for requests such as
   GetFeatureCount() or GetExtent(). Defaults to unlimited.
-  **FEATURE_ITERATION_TIMEOUT**\ =number. (GDAL >= 3.2.1)
   Timeout in second (as floating point number) for feature iteration,
   starting from the time of ResetReading(). Defaults to unlimited.
-  **FEATURE_ITERATION_TERMINATE_AFTER**\ =number. (GDAL >= 3.2.1)
   Maximum number of documents to collect for feature iteration.
   Defaults to unlimited.
-  **JSON_FIELD**\ =YES/NO. Whether to include a field called "_json"
   with the full document as JSON. Defaults to NO.
-  **FLATTEN_NESTED_ATTRIBUTE**\ =YES/NO. Whether to recursively explore
   nested objects and produce flatten OGR attributes. Defaults to YES.
-  **FID**\ =string. Field name, with integer values, to use as FID.
   Defaults to 'ogc_fid'
-  **FORWARD_HTTP_HEADERS_FROM_ENV**\ =string. (GDAL >= 3.1)
   Can be used to specify HTTP headers,
   typically for authentication purposes, that must be passed to Elasticsearch.
   The value of string is a comma separated list of http_header_name=env_variable_name,
   where http_header_name is the name of a HTTP header and env_variable_name
   the name of the environment variable / configuration option from which th value
   of the HTTP header should be retrieved. This is intended for a use case where
   the OGR Elasticsearch driver is invoked from a web server that stores the HTTP
   headers of incoming request into environment variables.
   The ES_FORWARD_HTTP_HEADERS_FROM_ENV configuration option can also be used.
-  **AGGREGATION**\ = string (GDAL >= 3.5). JSON-serialized definition of an
   :ref:`aggregation <vector.elasticsearch.aggregations>`.

Elasticsearch vs OGR concepts
-----------------------------

Each mapping type inside a Elasticsearch index will be considered as a
OGR layer. A Elasticsearch document is considered as a OGR feature.

Field definitions
-----------------

Fields are dynamically mapped from the input OGR data source. However,
the driver will take advantage of advanced options within Elasticsearch
as defined in a `field mapping
file <http://code.google.com/p/ogr2elasticsearch/wiki/ModifyingtheIndex>`__.

The mapping file allows you to modify the mapping according to the
`Elasticsearch field-specific
types <http://www.elasticsearch.org/guide/reference/mapping/core-types.html>`__.
There are many options to choose from, however, most of the
functionality is based on all the different things you are able to do
with text fields within Elasticsearch.

::

   ogr2ogr -progress --config ES_WRITEMAP /path/to/file/map.txt -f "Elasticsearch" http://localhost:9200 my_shapefile.shp

The Elasticsearch writer supports the following Configuration Options.
Starting with GDAL 2.1, layer creation options are also available and
should be preferred:

-  **ES_WRITEMAP**\ =/path/to/mapfile.txt. Creates a mapping file that
   can be modified by the user prior to insert in to the index. No
   feature will be written. Note that this will properly work only if
   only one single layer is created. Starting with GDAL 2.1, the
   **WRITE_MAPPING** layer creation option should rather be used.
-  **ES_META**\ =/path/to/mapfile.txt. Tells the driver to the
   user-defined field mappings. Starting with GDAL 2.1, the **MAPPING**
   layer creation option should rather be used.
-  **ES_BULK**\ =5000000. Identifies the maximum size in bytes of the
   buffer to store documents to be inserted at a time. Lower record
   counts help with memory consumption within Elasticsearch but take
   longer to insert. Starting with GDAL 2.1, the **BULK_SIZE** layer
   creation option should rather be used.
-  **ES_OVERWRITE**\ =1. Overwrites the current index by deleting an
   existing one. Starting with GDAL 2.1, the **OVERWRITE** layer
   creation option should rather be used.

Geometry types
--------------

In GDAL 2.0 and earlier, the driver was limited in the geometry it
handles: even if polygons were provided as input, they were stored as
`geo
point <http://www.elasticsearch.org/guide/en/elasticsearch/reference/current/mapping-geo-point-type.html>`__
and the "center" of the polygon is used as value of the point. Starting
with GDAL 2.1,
`geo_shape <https://www.elastic.co/guide/en/elasticsearch/reference/current/mapping-geo-shape-type.html>`__
is used to store all geometry types (except curve geometries that are
not handled by Elasticsearch and will be approximated to their linear
equivalents).

Filtering
---------

The driver will forward any spatial filter set with SetSpatialFilter()
to the server.

Starting with GDAL 2.2, SQL attribute filters set with
SetAttributeFilter() are converted to `Elasticsearch filter
syntax <https://www.elastic.co/guide/en/elasticsearch/reference/current/query-dsl-filters.html>`__.
They will be combined with the potentially defined spatial filter.

It is also possible to directly use a Elasticsearch filter by setting
the string passed to SetAttributeFilter() as a JSon serialized object,
e.g.

.. code-block:: json

   { "post_filter": { "term": { "properties.EAS_ID": 169 } } }

Note: if defining directly an Elastic Search JSon filter, the spatial
filter specified through SetSpatialFilter() will be ignored, and must
thus be included in the JSon filter if needed.

Paging
------

Features are retrieved from the server by chunks of 100. This can be
altered with the BATCH_SIZE open option.

Schema
------

When reading a Elastic Search index/type, OGR must establish the schema
of attribute and geometry fields, since OGR has a fixed schema concept.

In the general case, OGR will read the mapping definition and the first
100 documents (can be altered with the
FEATURE_COUNT_TO_ESTABLISH_FEATURE_DEFN open option) of the index/type
and build the schema that best fit to the found fields and values.

It is also possible to set the JSON_FIELD=YES open option so that a
\_json special field is added to the OGR schema. When reading Elastic
Search documents as OGR features, the full JSon version of the document
will be stored in the \_json field. This might be useful in case of
complex documents or with data types that do not translate well in OGR
data types. On creation/update of documents, if the \_json field is
present and set, its content will be used directly (other fields will be
ignored).

Feature ID
----------

Elastic Search have a special \_id field that contains the unique ID of
the document. This field is returned as an OGR field, but cannot be used
as the OGR special FeatureID field, which must be of integer type. By
default, OGR will try to read a potential 'ogc_fid' field to set the OGR
FeatureID. The name of this field to look up can be set with the FID
open option. If the field is not found, the FID returned by OGR will be
a sequential number starting at 1, but it is not guaranteed to be stable
at all.

ExecuteSQL() interface
----------------------

Starting with GDAL 2.2, SQL requests, involving a single layer, with
WHERE and ORDER BY statements will be translated as Elasticsearch
queries.

Otherwise, if specifying "ES" as the dialect of ExecuteSQL(), a JSon
string with a serialized `Elastic Search
filter <https://www.elastic.co/guide/en/elasticsearch/reference/current/query-dsl-filters.html>`__
can be passed. The search will be done on all indices and types, unless
the filter itself restricts the search. The returned layer will be a
union of the types returned by the
FEATURE_COUNT_TO_ESTABLISH_FEATURE_DEFN first documents. It will also
contain the \_index and \_type special fields to indicate the provenance
of the features.

The following filter can be used to restrict the search to the "poly"
index and its "FeatureCollection" type mapping (Elasticsearch 1.X and
2.X)

.. code-block:: json

   { "filter": {
       "indices" : {
           "no_match_filter": "none",
           "index": "poly",
           "filter": {
              "and" : [
                { "type": { "value": "FeatureCollection" } },
                { "term" : { "properties.EAS_ID" : 158.0 } }
              ]
           }
         }
       }
   }

For Elasticsearch 5.X (works also with 2.X) :

.. code-block:: json

   { "post_filter": {
       "indices" : {
           "no_match_query": "none",
           "index": "poly",
           "query": {
             "bool": {
               "must" : [
                 { "type": { "value": "FeatureCollection" } },
                 { "term" : { "properties.EAS_ID" : 158.0 } }
               ]
             }
           }
         }
       }
   }

Aggregations are not supported through the ExecuteSQL() interface, but through
the below described mechanism.

.. _vector.elasticsearch.aggregations:

Aggregations
------------

.. versionadded:: 3.5.0

The driver can support issuing aggregation requests to an index. ElasticSearch
aggregations can potentially be rather complex, so the driver currently limits
to geohash grid based spatial aggegrations, with additional fields with
statistical indicators (min, max, average, .), which can be used for example
to generate heatmaps. The specification of the aggegation is done through
the ``AGGREGATION`` open option, whose value is a JSON serialized object whose
members are:

- ``index`` (required): the name of the index to query.

- ``geometry_field`` (optional): the path to the geometry field on which to do
  `geohash grid aggregation <https://www.elastic.co/guide/en/elasticsearch/reference/current/search-aggregations-bucket-geohashgrid-aggregation.html>`__. For documents with points encoded as GeoJSON, this will
  be for example `geometry.coordinates`. When this property is not specified,
  the driver will analyze the mapping and use the geometry field definition
  found into it (provided there is a single one). Note that aggegration on
  geo_shape geometries is only supported since Elasticsearch 7 and may require
  a non-free license.

- ``geohash_grid`` (optional): a JSon object, describing a few characteristics of
  the geohash_grid, that can have the following members:

    * ``size`` (optional): maximum number of geohash buckets to return per query. The
      default is 10,000. If ``precision`` is specified and the number of results
      would exceed ``size``, then the server will trim the results, by sorting
      by decreasing number of documents matched.

    * ``precision`` (optional): string length of the geohashes used to define
      cells/buckets in the results, in the [1,12] range. A geohash of size 1
      can generate up to 32 buckets, of size 2 up to 32*32 buckets, etc.
      When it is not specified, the driver will automatically compute a value,
      taking into account the ``size`` parameter and the spatial filter, so that
      the theoretical number of buckets returned does not exceed ``size``.

- ``fields`` (optional): a JSon object, describing which additional statistical
  fields should be added, that can have the following members:

      * ``min`` (optional): array with the paths to index properties on which
        to compute the minimum during aggegation.

      * ``max`` (optional): array with the paths to index properties on which
        to compute the maximum  during aggegation.

      * ``avg`` (optional): array with the paths to index properties on which
        to compute the average during aggegation.

      * ``sum`` (optional): array with the paths to index properties on which
        to compute the sum during aggegation.

      * ``count`` (optional): array with the paths to index properties on which
        to compute the value_count during aggegation.

      * ``stats`` (optional): array with the paths to index properties on which
        to compute all the above indicators during aggegation.

  When using a GeoJSON mapping, the path to an index property is typically
  ``property.some_name``.

When specifying the AGGREGATION open option, a single read-only layer called
``aggregation`` will be returned. A spatial filter can be set on it using the
standard OGR SetSpatialFilter() API: it is applied prior to aggregation.

An example of a potential value for the AGGREGATION open option can be:

.. code-block:: json

    {
        "index": "my_points",
        "geometry_field": "geometry.coordinates",
        "geohash_grid": {
            "size": 1000,
            "precision": 3
        },
        "fields": {
            "min": [ "field_a", "field_b"],
            "stats": [ "field_c" ]
        }
    }


It will return a layer with a Point geometry field and the following fields:

- ``key`` of type String: the value of the geohash of the corresponding bucket
- ``doc_count`` of type Integer64: the number of matching documents in the bucket
- ``field_a_min`` of type Real
- ``field_b_min`` of type Real
- ``field_c_min`` of type Real
- ``field_c_max`` of type Real
- ``field_c_avg`` of type Real
- ``field_c_sum`` of type Real
- ``field_c_count`` of type Integer64

Multi-target layers
-------------------

.. versionadded:: 3.5.0

The GetLayerByName() method accepts a layer name that can be a comma-separated
list of indices, potentially combined with the '*' wildcard character. See
https://www.elastic.co/guide/en/elasticsearch/reference/current/multi-index.html.
Note that in the current implementation, the field definition will be established
from the one of the matching layers, but not all, so using this functionality will be
appropriate when the multiple matching layers share the same schema.

Getting metadata
----------------

Getting feature count is efficient.

Getting extent is efficient, only on geometry columns mapped to
Elasticsearch type geo_point. On geo_shape fields, feature retrieval of
the whole layer is done, which might be slow.

Write support
-------------

Index/type creation and deletion is possible.

Write support is only enabled when the datasource is opened in update
mode.

When inserting a new feature with CreateFeature() in non-bulk mode, and
if the command is successful, OGR will fetch the returned \_id and use
it for the SetFeature() operation.

Spatial reference system
------------------------

Geometries stored in Elastic Search are supposed to be referenced as
longitude/latitude over WGS84 datum (EPSG:4326). On creation, the driver
will automatically reproject from the layer (or geometry field) SRS to
EPSG:4326, provided that the input SRS is set and that is not already
EPSG:4326.

Layer creation options
----------------------

Starting with GDAL 2.1, the driver supports the following layer creation
options:

-  **INDEX_NAME**\ =name. Name of the index to create (or reuse). By
   default the index name is the layer name.
-  **INDEX_DEFINITION**\ =filename or JSon. (GDAL >= 2.4) Filename from
   which to read a user-defined index definition, or inlined index
   definition as serialized JSon.
-  **MAPPING_NAME=**\ =name. (Elasticsearch < 7) Name of the mapping type within the index.
   By default, the mapping name is "FeatureCollection" and the documents
   will be written as GeoJSON Feature objects. If another mapping name
   is chosen, a more "flat" structure will be used.  This option is
   ignored when converting to Elasticsearch >=7 (see `Removal of mapping types <https://www.elastic.co/guide/en/elasticsearch/reference/current/removal-of-types.html>`__).
   With Elasticsearch 7 or later, a "flat" structure is always used.
-  **MAPPING**\ =filename or JSon. Filename from which to read a
   user-defined mapping, or mapping as serialized JSon.
-  **WRITE_MAPPING**\ =filename. Creates a mapping file that can be
   modified by the user prior to insert in to the index. No feature will
   be written. This option is exclusive with MAPPING.
-  **OVERWRITE**\ =YES/NO. Whether to overwrite an existing type mapping
   with the layer name to be created. Defaults to NO.
-  **OVERWRITE_INDEX**\ =YES/NO. (GDAL >= 2.2) Whether to overwrite the
   whole index to which the layer belongs to. Defaults to NO. This
   option is stronger than OVERWRITE. OVERWRITE will only proceed if the
   type mapping corresponding to the layer is the single type mapping of
   the index. In case there are several type mappings, the whole index
   need to be destroyed (it is unsafe to destroy a mapping and the
   documents that use it, since they might be used by other mappings.
   This was possible in Elasticsearch 1.X, but no longer in later
   versions).
-  **GEOMETRY_NAME**\ =name. Name of geometry column. Defaults to
   'geometry'.
-  **GEOM_MAPPING_TYPE**\ =AUTO/GEO_POINT/GEO_SHAPE. Mapping type for
   geometry fields. Defaults to AUTO. GEO_POINT uses the
   `geo_point <https://www.elastic.co/guide/en/elasticsearch/reference/current/mapping-geo-point-type.html>`__
   mapping type. If used, the "centroid" of the geometry is used. This
   is the behavior of GDAL < 2.1. GEO_SHAPE uses the
   `geo_shape <https://www.elastic.co/guide/en/elasticsearch/reference/current/mapping-geo-shape-type.html>`__
   mapping type, compatible of all geometry types. When using AUTO, for
   geometry fields of type Point, a geo_point is used. In other cases,
   geo_shape is used.
-  **GEO_SHAPE_ENCODING**\ =GeoJSON/WKT. (GDAL >= 3.2.1)
   Encoding for geo_shape geometry fields. Defaults to GeoJSON. WKT is possible
   since Elasticsearch 6.2
-  **GEOM_PRECISION**\ ={value}{unit}'. Desired geometry precision.
   Number followed by unit. For example 1m. For a geo_point geometry
   field, this causes a compressed geometry format to be used. This
   option is without effect if MAPPING is specified.
-  **STORE_FIELDS**\ =YES/NO. Whether fields should be stored in the
   index. Setting to YES sets the `"store"
   property <https://www.elastic.co/guide/en/elasticsearch/reference/current/mapping-core-types.html>`__
   of the field mapping to "true" for all fields. Defaults to NO. (Note:
   prior to GDAL 2.1, the default behavior was to store fields) This
   option is without effect if MAPPING is specified.
-  **STORED_FIELDS**\ =List of comma separated field names that should
   be stored in the index. Those fields will have their `"store"
   property <https://www.elastic.co/guide/en/elasticsearch/reference/current/mapping-core-types.html>`__
   of the field mapping set to "true". If all fields must be stored,
   then using STORE_FIELDS=YES is a shortcut. This option is without
   effect if MAPPING is specified.
-  **NOT_ANALYZED_FIELDS**\ =List of comma separated field names that
   should not be analyzed during indexing. Those fields will have their
   `"index"
   property <https://www.elastic.co/guide/en/elasticsearch/reference/current/mapping-core-types.html>`__
   of the field mapping set to "not_analyzed" (the default in
   Elasticsearch is "analyzed"). A same field should not be specified
   both in NOT_ANALYZED_FIELDS and NOT_INDEXED_FIELDS. Starting with
   GDAL 2.2, the {ALL} value can be used to designate all fields. This
   option is without effect if MAPPING is specified.
-  **NOT_INDEXED_FIELDS**\ =List of comma separated field names that
   should not be indexed. Those fields will have their `"index"
   property <https://www.elastic.co/guide/en/elasticsearch/reference/current/mapping-core-types.html>`__
   of the field mapping set to "no" (the default in Elasticsearch is
   "analyzed"). A same field should not be specified both in
   NOT_ANALYZED_FIELDS and NOT_INDEXED_FIELDS. This option is without
   effect if MAPPING is specified.
-  **FIELDS_WITH_RAW_VALUE**\ =(GDAL > 2.2) List of comma separated
   field names (of type string) that should be created with an
   additional raw/not_analyzed sub-field, or {ALL} to designate all
   string analyzed fields. This is needed for sorting on those columns,
   and can improve performance when filtering with SQL operators. This
   option is without effect if MAPPING is specified.
-  **BULK_INSERT**\ =YES/NO. Whether to use bulk insert for feature
   creation. Defaults to YES.
-  **BULK_SIZE**\ =value. Size in bytes of the buffer for bulk upload.
   Defaults to 1000000 (1 million).
-  **FID**\ =string. Field name, with integer values, to use as FID. Can
   be set to empty to disable the writing of the FID value. Defaults to
   'ogc_fid'
-  **DOT_AS_NESTED_FIELD**\ =YES/NO. Whether to consider dot character
   in field name as sub-document. Defaults to YES.
-  **IGNORE_SOURCE_ID**\ =YES/NO. Whether to ignore \_id field in
   features passed to CreateFeature(). Defaults to NO.

Examples
--------

**Open the local store:**

::

   ogrinfo ES:

**Open a remote store:**

::

   ogrinfo ES:http://example.com:9200

**Filtering on a Elastic Search field:**

::

   ogrinfo -ro ES: my_type -where '{ "post_filter": { "term": { "properties.EAS_ID": 168 } } }'

**Using "match" query on Windows:**
On Windows the query must be between double quotes and double quotes
inside the query must be escaped.

::

   C:\GDAL_on_Windows>ogrinfo ES: my_type -where "{\"query\": { \"match\": { \"properties.NAME\": \"Helsinki\" } } }"

**Basic aggregation:**

::

   ogrinfo -ro ES: my_type -oo "AGGREGATION={\"index\":\"my_points\"}"

**Load an Elasticsearch index with a shapefile:**

::

   ogr2ogr -f "Elasticsearch" http://localhost:9200 my_shapefile.shp

**Create a Mapping File:** The mapping file allows you to modify the
mapping according to the `Elasticsearch field-specific
types <http://www.elasticsearch.org/guide/reference/mapping/core-types.html>`__.
There are many options to choose from, however, most of the
functionality is based on all the different things you are able to do
with text fields.

::

   ogr2ogr -progress --config ES_WRITEMAP /path/to/file/map.txt -f "Elasticsearch" http://localhost:9200 my_shapefile.shp

or (GDAL >= 2.1):

::

   ogr2ogr -progress -lco WRITE_MAPPING=/path/to/file/map.txt -f "Elasticsearch" http://localhost:9200 my_shapefile.shp

**Read the Mapping File:** Reads the mapping file during the
transformation

::

   ogr2ogr -progress --config ES_META /path/to/file/map.txt -f "Elasticsearch" http://localhost:9200 my_shapefile.shp

or (GDAL >= 2.1):

::

   ogr2ogr -progress -lco MAPPING=/path/to/file/map.txt -f "Elasticsearch" http://localhost:9200 my_shapefile.shp

**Bulk Uploading (for larger datasets):** Bulk loading helps when
uploading a lot of data. The integer value is the number of bytes that
are collected before being inserted. `Bulk size
considerations <https://www.elastic.co/guide/en/elasticsearch/guide/current/bulk.html#_how_big_is_too_big>`__

::

   ogr2ogr -progress --config ES_BULK 5000000 -f "Elasticsearch" http://localhost:9200 PG:"host=localhost user=postgres dbname=my_db password=password" "my_table" -nln thetable

or (GDAL >= 2.1):

::

   ogr2ogr -progress -lco BULK_SIZE=5000000 -f "Elasticsearch" http://localhost:9200 my_shapefile.shp

**Overwrite the current Index:** If specified, this will overwrite the
current index. Otherwise, the data will be appended.

::

   ogr2ogr -progress --config ES_OVERWRITE 1 -f "Elasticsearch" http://localhost:9200 PG:"host=localhost user=postgres dbname=my_db password=password" "my_table" -nln thetable

or (GDAL >= 2.1):

::

   ogr2ogr -progress -overwrite ES:http://localhost:9200 PG:"host=localhost user=postgres dbname=my_db password=password" "my_table" -nln thetable

See Also
--------

-  `Home page for Elasticsearch <http://elasticsearch.org/>`__
-  `Examples Wiki <http://code.google.com/p/ogr2elasticsearch/w/list>`__
-  `Google Group <http://groups.google.com/group/ogr2elasticsearch>`__
