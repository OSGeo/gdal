.. _vector.geojson:

GeoJSON
=======

.. shortname:: GeoJSON

.. built_in_by_default::

This driver implements read/write support for access to features encoded
in `GeoJSON <http://geojson.org/>`__ format. GeoJSON is a dialect based
on the `JavaScript Object Notation (JSON) <http://json.org/>`__. JSON is
a lightweight plain text format for data interchange and GeoJSON is
nothing other than its specialization for geographic content.

GeoJSON is supported as an output format of a number of services:
`GeoServer <http://docs.geoserver.org/2.6.x/en/user/services/wfs/outputformats.html>`__,
`CartoWeb <http://exportgge.sourceforge.net/kml/>`__, etc.

The OGR GeoJSON driver translates GeoJSON encoded data to objects of the
`OGR Simple Features model <ogr_arch.html>`__: Datasource, Layer,
Feature, Geometry. The implementation is based on `GeoJSON
Specification, v1.0 <http://geojson.org/geojson-spec.html>`__.

Starting with GDAL 2.1.0, the GeoJSON driver supports updating existing
GeoJSON files. In that case, the default value for the NATIVE_DATA open
option will be YES.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Datasource
----------

The OGR GeoJSON driver accepts three types of sources of data:

-  Uniform Resource Locator (`URL <http://en.wikipedia.org/wiki/URL>`__)
   - a Web address to perform
   `HTTP <http://en.wikipedia.org/wiki/HTTP>`__ request
-  Plain text file with GeoJSON data - identified from the file
   extension .geojson or .json
-  Text passed directly and encoded in GeoJSON

Starting with GDAL 2.3, the URL/filename/text might be prefixed with
GeoJSON: to avoid any ambiguity with other drivers.

Layer
-----

A GeoJSON datasource is translated to single OGRLayer object with
pre-defined name *OGRGeoJSON*:

::

   ogrinfo -ro http://featureserver/data/.geojson OGRGeoJSON

It is also valid to assume that OGRDataSource::GetLayerCount() for
GeoJSON datasource always returns 1.

Starting with GDAL 2.2, the layer name is built with the following
logic:

#. If a "name" member is found at the FeatureCollection level, it is
   used.
#. Otherwise if the filename is regular (ie not a URL with query
   parameters), then the filename without extension and path is used as
   the layer name.
#. Otherwise OGRGeoJSON is used.

Accessing Web Service as a datasource (i.e. FeatureServer), each request
will produce new layer. This behavior conforms to stateless nature of
HTTP transaction and is similar to how Web browsers operate: single
request == single page.

If a top-level member of GeoJSON data is of any other type than
*FeatureCollection*, the driver will produce a layer with only one
feature. Otherwise, a layer will consists of a set of features.

If the NATIVE_DATA open option is set to YES, members at the level of
the FeatureCollection will be stored as a serialized JSon object in the
NATIVE_DATA item of the NATIVE_DATA metadata domain of the layer object
(and "application/vnd.geo+json" in the NATIVE_MEDIA_TYPE of the
NATIVE_DATA metadata domain).

Feature
-------

The OGR GeoJSON driver maps each object of following types to new
*OGRFeature* object: Point, LineString, Polygon, GeometryCollection,
Feature.

According to the *GeoJSON Specification*, only the *Feature* object must
have a member with name *properties*. Each and every member of
*properties* is translated to OGR object of type of OGRField and added
to corresponding OGRFeature object.

The *GeoJSON Specification* does not require all *Feature* objects in a
collection to have the same schema of properties. If *Feature* objects
in a set defined by *FeatureCollection* object have different schema of
properties, then resulting schema of fields in OGRFeatureDefn is
generated as `union <http://en.wikipedia.org/wiki/Union_(set_theory)>`__
of all *Feature* properties.

Schema detection will recognized fields of type String, Integer, Real,
StringList, IntegerList and RealList, Integer(Boolean), Date, Time and DateTime.

It is possible to tell the driver to not to process attributes by
setting environment variable **ATTRIBUTES_SKIP=YES**. Default behavior
is to preserve all attributes (as an union, see previous paragraph),
what is equal to setting **ATTRIBUTES_SKIP=NO**.

If the NATIVE_DATA open option is set to YES, the Feature JSon object
will be stored as a serialized JSon object in the NativeData property of
the OGRFeature object (and "application/vnd.geo+json" in the
NativeMediaType property). On write, if a OGRFeature to be written has
its NativeMediaType property set to "application/vnd.geo+json" and its
NativeData property set to a string that is a serialized JSon object,
then extra members of this object (i.e. not the "property" dictionary,
nor the first 3 dimensions of geometry coordinates) will be used to
enhance the created JSon object from the OGRFeature. See :ref:`rfc-60`
for more details.

Geometry
--------

Similarly to the issue with mixed-properties features, the *GeoJSON
Specification* draft does not require all *Feature* objects in a
collection must have geometry of the same type. Fortunately, OGR objects
model does allow to have geometries of different types in single layer -
a heterogeneous layer. By default, the GeoJSON driver preserves type of
geometries.

However, sometimes there is a need to generate a homogeneous layer from
a set of heterogeneous features. For this purpose, it is possible to
tell the driver to wrap all geometries with OGRGeometryCollection type
as a common denominator. This behavior may be controlled by setting
the :decl_configoption:`GEOMETRY_AS_COLLECTION` configuration option to YES
(default is NO).

configuration options
---------------------

-  :decl_configoption:`GEOMETRY_AS_COLLECTION` - used to control translation of
   geometries: YES - wrap geometries with OGRGeometryCollection type
-  :decl_configoption:`ATTRIBUTES_SKIP` - controls translation of attributes:
   YES - skip all attributes
-  :decl_configoption:`OGR_GEOJSON_MAX_OBJ_SIZE` - (GDAL >= 3.0.2) size in
   MBytes of the maximum accepted single feature, default value is 200MB

Open options
------------

-  **FLATTEN_NESTED_ATTRIBUTES** = YES/NO: Whether to recursively
   explore nested objects and produce flatten OGR attributes. Defaults
   to NO.
-  **NESTED_ATTRIBUTE_SEPARATOR** = character: Separator between
   components of nested attributes. Defaults to '_'
-  **FEATURE_SERVER_PAGING** = YES/NO: Whether to automatically scroll
   through results with a ArcGIS Feature Service endpoint.
-  **NATIVE_DATA** = YES/NO: (GDAL >= 2.1) Whether to store the native
   JSon representation at FeatureCollection and Feature level. Defaults
   to NO. This option can be used to improve round-tripping from GeoJSON
   to GeoJSON by preserving some extra JSon objects that would otherwise
   be ignored by the OGR abstraction. Note that ogr2ogr by default
   enable this option, unless you specify its -noNativeData switch.
-  **ARRAY_AS_STRING** = YES/NO: (GDAL >= 2.1) Whether to expose JSon
   arrays of strings, integers or reals as a OGR String. Default is NO.
   Can also be set with the :decl_configoption:`OGR_GEOJSON_ARRAY_AS_STRING`
   configuration option.
-  **DATE_AS_STRING** = YES/NO: (GDAL >= 3.0.3) Whether to expose
   date/time/date-time content using dedicated OGR date/time/date-time types
   or as a OGR String. Default is NO (that is date/time/date-time are
   detected as such).
   Can also be set with the :decl_configoption:`OGR_GEOJSON_DATE_AS_STRING`
   configuration option.

To explain FLATTEN_NESTED_ATTRIBUTES, consider the following GeoJSON
fragment:

::

   {
     "type": "FeatureCollection",
     "features":
     [
       {
         "type": "Feature",
         "geometry": {
           "type": "Point",
           "coordinates": [ 2, 49 ]
         },
         "properties": {
           "a_property": "foo",
           "some_object": {
             "a_property": 1,
             "another_property": 2
           }
         }
       }
     ]
   }

"ogrinfo test.json -al -oo FLATTEN_NESTED_ATTRIBUTES=yes" reports:

::

   OGRFeature(OGRGeoJSON):0
     a_property (String) = foo
     some_object_a_property (Integer) = 1
     some_object_another_property (Integer) = 2
     POINT (2 49)

Layer creation options
----------------------

-  **WRITE_BBOX** = YES/NO: Set to YES to write a bbox
   property with the bounding box of the geometries at the feature and
   feature collection level. Defaults to NO.
-  **COORDINATE_PRECISION** = int_number: Maximum number
   of figures after decimal separator to write in coordinates. Default
   to 15 for GeoJSON 2008, and 7 for RFC 7946. "Smart" truncation will
   occur to remove trailing zeros.
-  **SIGNIFICANT_FIGURES** = int_number: (OGR >= 2.1) Maximum number of
   significant figures when writing floating-point numbers. Default to
   17. If explicitly specified, and COORDINATE_PRECISION is not, this
   will also apply to coordinates.
-  **NATIVE_DATA**\ =string. (OGR >= 2.1) Serialized JSon object that
   contains extra properties to store at FeatureCollection level.
-  **NATIVE_MEDIA_TYPE**\ =string. (OGR >= 2.1) Format of NATIVE_DATA.
   Must be "application/vnd.geo+json", otherwise NATIVE_DATA will be
   ignored.
-  **RFC7946**\ =YES/NO. (OGR >= 2.2) Whether to use `RFC
   7946 <https://tools.ietf.org/html/rfc7946>`__ standard. Otherwise
   `GeoJSON 2008 <http://geojson.org/geojson-spec.html>`__ initial
   version will be used. Default is NO (thus GeoJSON 2008)
-  **WRITE_NAME**\ =YES/NO. (OGR >= 2.2) Whether to write a "name"
   property at feature collection level with layer name. Defaults to
   YES.
-  **DESCRIPTION**\ =string. (OGR >= 2.2) (Long) description to write in
   a "description" property at feature collection level. On reading,
   this will be reported in the DESCRIPTION metadata item of the layer.
-  **ID_FIELD**\ =string. (OGR >= 2.3) Name of the source field that
   must be written as the 'id' member of Feature objects.
-  **ID_TYPE**\ =AUTO/String/Integer. (OGR >= 2.3) Type of the 'id'
   member of Feature objects.
-  **ID_GENERATE**\ =YES/NO. (OGR >= 3.1) Auto-generate feature ids
-  **WRITE_NON_FINITE_VALUES**\ =YES/NO. (OGR >= 2.4) Whether to write
   NaN / Infinity values. Such values are not allowed in strict JSon
   mode, but some JSon parsers (libjson-c >= 0.12 for exampl) can
   understand them as they are allowed by ECMAScript. Defaults to NO

VSI Virtual File System API support
-----------------------------------

The driver supports reading and writing to files managed by VSI Virtual
File System API, which includes "regular" files, as well as files in the
/vsizip/ (read-write), /vsigzip/ (read-write), /vsicurl/ (read-only)
domains.

Writing to /dev/stdout or /vsistdout/ is also supported.

Round-tripping of extra JSon members
------------------------------------

See :ref:`rfc-60` for more details.

Starting with GDAL 2.1, extra JSon members at the FeatureCollection,
Feature or geometry levels that are not normally reflected in the OGR
abstraction, such as the ones called "extra_XXXXX_member" in the below
snippet, are by default preserved when executing ogr2ogr with GeoJSON
both at the source and destination. This also applies to extra values in
position tuples of geometries, beyond the 3rd dimension (100, 101 in the
below example), if the transformation preserves the geometry structure
(for example, reprojection is allowed, but not change in the number of
coordinates).

::

   {
     "type": "FeatureCollection",
     "extra_fc_member": "foo",
     "features":
     [
       {
         "type": "Feature",
         "extra_feat_member": "bar",
         "geometry": {
           "type": "Point",
           "extra_geom_member": "baz",
           "coordinates": [ 2, 49, 3, 100, 101 ]
         },
         "properties": {
           "a_property": "foo",
         }
       }
     ]
   }

This behavior can be turned off by specifying the **-noNativeData**
switch of the ogr2ogr utility.

RFC 7946 write support
----------------------

By default, the driver will write GeoJSON files following GeoJSON 2008
specification. When specifying the RFC7946=YES creation option, the RFC
7946 standard will be used instead.

The differences between the 2 versions are mentioned in `Appendix B of
RFC 7946 <https://tools.ietf.org/html/rfc7946#appendix-B>`__ and
recalled here for what matters to the driver:

-  Coordinates must be geographic over the WGS 84 ellipsoid,
   hence if the spatial reference system specified at layer creation
   time is not EPSG:4326, on-the-fly reprojection will be done by the
   driver.
-  Polygons will be written such as to follow the right-hand rule for
   orientation (counterclockwise external rings, clockwise internal
   rings).
-  The values of a "bbox" array are "[west, south, east, north]", not
   "[minx, miny, maxx, maxy]"
-  Some extension member names (see previous section about
   round/tripping) are forbidden in the FeatureCollection, Feature and
   Geometry objects.
-  The default coordinate precision is 7 decimal digits after decimal
   separator.

Examples
--------

How to dump content of .geojson file:

::

   ogrinfo -ro point.geojson

How to query features from remote service with filtering by attribute:

::

   ogrinfo -ro http://featureserver/cities/.geojson OGRGeoJSON -where "name=Warsaw"

How to translate number of features queried from FeatureServer to ESRI
Shapefile:

::

   ogr2ogr -f "ESRI Shapefile" cities.shp http://featureserver/cities/.geojson OGRGeoJSON

How to translate a ESRI Shapefile into a RFC 7946 GeoJSON file:

::

   ogr2ogr -f GeoJSON cities.json cities.shp -lco RFC7946=YES

See Also
--------

-  `GeoJSON <http://geojson.org/>`__ - encoding geographic content in
   JSON
-  `RFC 7946 <https://tools.ietf.org/html/rfc7946>`__ standard.
-  `GeoJSON 2008 <http://geojson.org/geojson-spec.html>`__ specification
   (obsoleted by RFC 7946).
-  `JSON <http://json.org/>`__ - JavaScript Object Notation
-  :ref:`GeoJSON sequence driver <vector.geojsonseq>`
-  :ref:`ESRI JSon / FeatureService driver <vector.esrijson>`
-  :ref:`TopoJSON driver <vector.topojson>`
