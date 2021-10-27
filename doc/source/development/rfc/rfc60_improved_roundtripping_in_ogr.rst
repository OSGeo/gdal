.. _rfc-60:

=======================================================================================
RFC 60 : Improved round-tripping in OGR
=======================================================================================

Author: Even Rouault

Contact: even.rouault at spatialys.com

Status: Adopted, implemented

Implementation version: 2.1

Summary
-------

This RFC defines how to improve better round-tripping in conversion of
vector formats, in particular for GeoJSON extensions.

Rationale
---------

Some formats have concepts that are not well modeled by the OGR
abstraction, but that are desirable to be preserved in transformation
scenarios involving reprojection, spatial/attribute filtering, clipping,
etc... where the target format is the source format.

Various extensions exist above the core GeoJSON specification: at the
FeatureCollection, Feature or Geometry levels.

See
`https://github.com/mapbox/carmen/blob/master/carmen-geojson.md <https://github.com/mapbox/carmen/blob/master/carmen-geojson.md>`__,

::

   {
       "type": "FeatureCollection",
       "query": ["austin"],
       "features": [
           {
               "type": "Feature",
               "id": "place.4201",
               "text": "Austin",
               "place_name": "Austin, Texas, United States",
               "bbox": [-97.9383829999999, 30.098659, -97.5614889999999, 30.516863],
               "center": [-97.7559964, 30.3071816],
               "geometry": {
                   "type": "Point",
                   "coordinates": [-97.7559964, 30.3071816]
               },
               "properties": {
                   "title": "Austin",
                   "type": "city",
                   "score": 600000790107194.8
               },
               "context": [
                   {
                       "id": "province.293",
                       "text": "Texas"
                   },
                   {
                       "id": "country.51",
                       "text": "United States"
                   }
               ]
           },
           ...
       ]
   }

`https://github.com/geocoders/geocodejson-spec/blob/master/draft/README.md <https://github.com/geocoders/geocodejson-spec/blob/master/draft/README.md>`__:

::

   {

     // REQUIRED. GeocodeJSON result is a FeatureCollection.
     "type": "FeatureCollection",

     // REQUIRED. Namespace.
     "geocoding": {

       // REQUIRED. A semver.org compliant version number. Describes the version of
       // the GeocodeJSON spec that is implemented by this instance.
       "version": "0.1.0",

       // OPTIONAL. Default: null. The licence of the data. In case of multiple sources,
       // and then multiple licences, can be an object with one key by source.
       "licence": "ODbL",

       // OPTIONAL. Default: null. The attribution of the data. In case of multiple sources,
       // and then multiple attributions, can be an object with one key by source.
       "attribution": "OpenStreetMap Contributors",

       // OPTIONAL. Default: null. The query that has been issued to trigger the
       // search.
       "query": "24 allée de Bercy 75012 Paris",

     },

     // REQUIRED. As per GeoJSON spec.
     "features": [
       // OPTIONAL. An array of feature objects. See below.
     ]
   }

or
`https://github.com/geojson/draft-geojson/issues/80#issuecomment-138037554 <https://github.com/geojson/draft-geojson/issues/80#issuecomment-138037554>`__
for a few examples.

::

   { "type" : "GeometryCollection",
     "geometries" : [
       { "type" : "LineString",
         "extensions" : [ "time", "atemp", "hr", "cad" ],
         "coordinates" : [
           [
             -122.45671039447188,
             37.786870915442705,
             0.4000000059604645, 
             "2014-11-06T19:16:06.000Z", 
             31.0, 
             99, 
             0
           ], 

Changes
-------

OGRFeature
~~~~~~~~~~

Two new members will be added to the OGRFeature class, m_pszNativeData
(string) and m_pszNativeMediaType (string). m_pszNativeData will contain
the representation (or part of the representation) of the original
feature, and m_pszNativeMediaType the `media
type <https://en.wikipedia.org/wiki/Media_type>`__

The following methods will be added to OGRFeature class:

::

   public:
       const char *GetNativeData() const;
       const char *GetNativeMediaType() const;
       void        SetNativeData( const char* pszNativeData );
       void        SetNativeMediaType( const char* pszNativeMediaType );

Thus, in the GeoJSON case, nativeData would contain the full
serialization of a GeoJSON Feature. m_pszNativeMediaType would be set to
"application/vnd.geo+json" The writer side of the GeoJSON driver would
start from the nativeData if present (and if nativeMediaType =
"application/vnd.geo+json", replace its properties member with the
content of the OGR fields and patch its geometry to include additional
JSON objects.

The OGRFeature::Clone() and ::SetFrom() methods will propagate
nativeData and nativeMediaType.

OGRLayer
~~~~~~~~

A dedicated metadata domain "NATIVE_DATA" in which there would be a
"NATIVE_DATA" and "NATIVE_MEDIA_TYPE" items would be used. In the
GeoJSON case, this would contain JSON members at the FeatureCollection
level (excluding the features array of course).

Driver open options and layer creation options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Drivers that support nativeData on read should expose a NATIVE_DATA
boolean open option, and disable it by default so as not to impact
performance. ogr2ogr will by default turn this option on.

Drivers that support nativeData on write at the layer level should
expose a NATIVE_DATA string and NATIVE_MEDIA_TYPE string layer creation
options, so that ogr2ogr can fill them with the content of the
NATIVE_DATA metadata domain of the source layer(s).

C API
-----

The following functions will be added:

::

   const char CPL_DLL *OGR_F_GetNativeData(OGRFeatureH);
   void OGR_F_SetNativeData(OGRFeatureH, const char*);
   const char CPL_DLL *OGR_F_GetNativeMediaType(OGRFeatureH);
   void OGR_F_SetNativeMediaType(OGRFeatureH, const char*);

SQL result layers
-----------------

Both OGR SQL and SQLite SQL dialect implementations have been modified
to propagate the content of the NATIVE_DATA metadata domain of the
source layer (the one of the FROM table) to the target layer, and
NativeData and NativeMediaType from source features are copied into
target features.

SWIG bindings (Python / Java / C# / Perl) changes
-------------------------------------------------

The new functions will mapped to SWIG as GetNativeData(),
SetNativeData(), GetNativeMediaType() and SetNativeMediaType()

Drivers
-------

The GeoJSON driver will be modified to implement this RFC, in read and
write, and thus will\* :

-  declare a NATIVE_DATA open option to enable storing layer and feature
   native data.
-  and NATIVE_DATA & NATIVE_MEDIA_TYPE layer creation options so as to
   be able to write native data at FeatureCollection levels
-  use OGRFeature nativeData on write.

The effect of this is that ogr2ogr will be able to preserve the members
marked between ``***`` in the below snippet:

::

   {
     "type": "FeatureCollection",
     ***"extra_fc_member": "foo",***
     "features":
     [
       {
         "type": "Feature",
         ***"extra_feat_member": "bar",***
         "geometry": {
           "type": "Point",
           ***extra_geom_member": "baz",***
           "coordinates": [ 2, 49, 3, ***100, 101*** ]
         },
         "properties": {
           "a_property": "foo",
         }
       }
     ]
   }

Other drivers like ElasticSearch and MongoDB drivers, that use a \_json
OGR field for round-tripping could potentially be upgraded to benefit
from the mechanism of this RFC.

Utilities
---------

ogr2ogr will be modified to automatically copy nativeData at layer and
feature level. A -noNativeData flag will be added to avoid doing so,
when this is not desirable.

By default, ogr2ogr will open datasources with the NATIVE_DATA=YES open
option so that drivers that can store nativeData do so. And if the
output datasource supports the NATIVE_DATA and NATIVE_MEDIA_TYPE layer
creation options, it will feel them with the content of the source layer
NATIVE_DATA metadata domain.

Documentation
-------------

All new methods/functions are documented.

Test Suite
----------

The GeoJSON and ogr2ogr related tests will be extended

Compatibility Issues
--------------------

Nothing severe expected. Potentially existing scripts might need to add
-noNativeData to get previous behavior.

Related ticket
--------------

`https://trac.osgeo.org/gdal/ticket/5310 <https://trac.osgeo.org/gdal/ticket/5310>`__

Implementation
--------------

The implementation will be done by Even Rouault (Spatialys) and be
sponsored by Mapbox.

The proposed implementation lies in the "rfc60_native_data" branch of
the
​\ `https://github.com/rouault/gdal2/tree/rfc60_native_data <https://github.com/rouault/gdal2/tree/rfc60_native_data>`__,
in pull request
`https://github.com/OSGeo/gdal/pull/75 <https://github.com/OSGeo/gdal/pull/75>`__

Voting history
--------------

+1 from HowardB, KurtS, TamasS, JukkaR and EvenR
