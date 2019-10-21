.. _vector.vrt:

VRT -- Virtual Format
=====================

.. shortname:: VRT

OGR Virtual Format is a driver that transforms features read from other
drivers based on criteria specified in an XML control file. It is
primarily used to derive spatial layers from flat tables with spatial
information in attribute columns. It can also be used to associate
coordinate system information with a datasource, merge layers from
different datasources into a single data source, or even just to provide
an anchor file for access to non-file oriented datasources.

The virtual files are currently normally prepared by hand.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Creation Issues
---------------

The CreateFeature(), SetFeature() and DeleteFeature()
operations are supported on a layer of a VRT dataset, if the following
conditions are met :

-  the VRT dataset is opened in update mode
-  the underlying source layer supports those operations
-  the *SrcLayer* element is used (as opposed to the *SrcSQL* element)
-  the FID of the VRT features is the same as the FID of the source
   features, that is to say, the *FID* element is not specified

Virtual File Format
-------------------

The root element of the XML control file is **OGRVRTDataSource**. It has
an **OGRVRTLayer** (or **OGRVRTWarpedLayer** or **OGRVRTUnionLayer**
starting with GDAL 1.10.0) child for each layer in the virtual
datasource, and a **Metadata** element.

A `XML schema of the OGR VRT
format <https://github.com/OSGeo/gdal/blob/master/gdal/data/ogrvrt.xsd>`__ is
available. Starting with GDAL 1.11, when GDAL is configured with libXML2
support, that schema will be used to validate the VRT documents.
Non-conformities will be reported only as warnings. That validation can
be disabled by setting the GDAL_XML_VALIDATION configuration option to
NO.

**Metadata** (optional): (GDAL >= 2.0) This element contains a list of
metadata name/value pairs associated with the dataset as a whole. It has
<MDI> (metadata item) subelements which have a "key" attribute and the
value as the data of the element. The Metadata element can be repeated
multiple times, in which case it must be accompanied with a "domain"
attribute to indicate the name of the metadata domain.

A **OGRVRTLayer** element should have a **name** attribute with the
layer name, and may have the following subelements:

**SrcDataSource** (mandatory): The value is the name of the datasource
that this layer will be derived from. The element may optionally have a
**relativeToVRT** attribute which defaults to "0", but if "1" indicates
that the source datasource should be interpreted as relative to the
virtual file. This can be any OGR supported dataset, including ODBC,
CSV, etc. The element may also have a **shared** attribute to control
whether the datasource should be opened in shared mode. Defaults to OFF
for SrcLayer use and ON for SrcSQL use.

| 

**OpenOptions** (optional): (GDAL >= 2.0) This element may list a number
of open options as child elements of the form <OOI
key="key_name">value_name</OOI>

| 

**Metadata** (optional): (GDAL >= 2.0) This element contains a list of
metadata name/value pairs associated with the layer as a whole. It has
<MDI> (metadata item) subelements which have a "key" attribute and the
value as the data of the element. The Metadata element can be repeated
multiple times, in which case it must be accompanied with a "domain"
attribute to indicate the name of the metadata domain.

| 

**SrcLayer** (optional): The value is the name of the layer on the
source data source from which this virtual layer should be derived. If
this element isn't provided, then the SrcSQL element must be provided.

| 

**SrcSQL** (optional): An SQL statement to execute to generate the
desired layer result. This should be provided instead of the SrcLayer
for statement derived results. Some limitations may apply for SQL
derived layers. Starting with OGR 1.10, an optional **dialect**
attribute can be specified on the SrcSQL element to specify which SQL
"dialect" should be used : possible values are currently
`OGRSQL <ogr_sql.html>`__ or `SQLITE <ogr_sql_sqlite.html>`__. If
*dialect* is not specified, the default dialect of the datasource will
be used.

| 

| **FID** (optional): Name of the source attribute column from which the
  FID of features should be derived. If not provided, the FID of the
  source features will be used directly.

-  Logic for GDAL >= 2.4: Different situations are possible:

   -  ::

         <FID>source_field_name</FID>

      A FID column will be reported as source_field_name with the
      content of source field source_field_name.

   -  ::

         <FID name="dest_field_name">source_field_name</FID>

      A FID column will be reported as dest_field_name with the content
      of source field source_field_name. dest_field_name can potentially
      be set to the empty string.

   -  ::

         <FID />

      No FID column is reported. The FID value of VRT features is the
      FID value of the source features.

   -  ::

         <FID name="dest_field_name"/>

      A FID column will be reported as dest_field_name with the content
      of the implicit source FID column. The FID value of VRT features
      is the FID value of the source features.

-  Logic for GDAL < 2.4: The layer will report the FID column name only
   if it is also reported as a regular field. Starting with GDAL 2.0, a
   "name" attribute can be specified on the FID element so that the FID
   column name is always reported.

| 

**Style** (optional): Name of the attribute column from which the style
of features should be derived. If not provided, the style of the source
features will be used directly.

| 

**GeometryType** (optional): The geometry type to be assigned to the
layer. If not provided it will be taken from the source layer. The value
should be one of "wkbNone", "wkbUnknown", "wkbPoint", "wkbLineString",
"wkbPolygon", "wkbMultiPoint", "wkbMultiLineString", "wkbMultiPolygon",
or "wkbGeometryCollection". Optionally "25D" may be appended to mark it
as including Z coordinates. Defaults to "wkbUnknown" indicating that any
geometry type is allowed.

| 

**LayerSRS** (optional): The value of this element is the spatial
reference to use for the layer. If not provided, it is inherited from
the source layer. The value may be WKT or any other input that is
accepted by the OGRSpatialReference::SetUserInput() method. If the value
is NULL, then no SRS will be used for the layer.

| 

| **GeometryField** (optional): This element is used to define how the
  geometry for features should be derived.
| If not provided the geometry of the source feature is copied directly.
| The type of geometry encoding is indicated with the **encoding**
  attribute which may have the value "WKT", "WKB" or "PointFromColumns".
| If the encoding is "WKT" or "WKB" then the **field** attribute will
  have the name of the field containing the WKT or WKB geometry.
| If the encoding is "PointFromColumns" then the **x**, **y**, **z** and
  **m** attributes will have the names of the columns to be used for the
  X, Y, Z and M coordinates. The **z** and **m** attributes are optional
  (m only supported since OGR 2.1.1).
| The optional **reportSrcColumn** attribute can be used to specify
  whether the source geometry fields (the fields set in the **field**,
  **x**, **y**, **z**, **m** attributes) should be reported as fields of
  the VRT layer. It defaults to TRUE. If set to FALSE, the source
  geometry fields will only be used to build the geometry of the
  features of the VRT layer.

Starting with OGR 1.11, the GeometryField element can be repeated as
many times as necessary to create multiple geometry fields. It accepts a
**name** attribute (recommended) that will be used to define the VRT
geometry field name. When **encoding** is not specified, the **field**
attribute will be used to determine the corresponding geometry field
name in the source layer. If neither **encoding** nor **field** are
specified, it is assumed that the name of source geometry field is the
value of the **name** attribute.

Starting with GDAL 2.0, the optional **nullable** attribute can be used
to specify whether the geometry field is nullable. It defaults to
"true".

When several geometry fields are used, the following child elements of
**GeometryField** can be defined to explicitly set the geometry type,
SRS, source region, or extent.

-  **GeometryType** (optional) : same syntax as OGRVRTLayer-level
   **GeometryType**.
-  **SRS** (optional) : same syntax as OGRVRTLayer-level **LayerSRS**
   (note SRS vs LayerSRS)
-  **SrcRegion** (optional) : same syntax as OGRVRTLayer-level
   **SrcRegion**
-  **ExtentXMin**, **ExtentYMin**, **ExtentXMax** and **ExtentXMax**
   (optional) : same syntax as OGRVRTLayer-level elements of same name

If no **GeometryField** element is specified, all the geometry fields of
the source layer will be exposed by the VRT layer. In order not to
expose any geometry field of the source layer, you need to specify
OGRVRTLayer-level **GeometryType** element to wkbNone.

| 

**SrcRegion** (optional, from GDAL 1.7.0) : This element is used to
define an initial spatial filter for the source features. This spatial
filter will be combined with any spatial filter explicitly set on the
VRT layer with the SetSpatialFilter() method. The value of the element
must be a valid WKT string defining a polygon. An optional **clip**
attribute can be set to "TRUE" to clip the geometries to the source
region, otherwise the source geometries are not modified.

| 

**Field** (optional, from GDAL 1.7.0): One or more attribute fields may
be defined with Field elements. If no Field elements are defined, the
fields of the source layer/sql will be defined on the VRT layer. The
Field may have the following attributes:

-  **name** (required): the name of the field.
-  **type**: the field type, one of "Integer", "IntegerList", "Real",
   "RealList", "String", "StringList", "Binary", "Date", "Time", or
   "DateTime". Defaults to "String".
-  **subtype**: (GDAL >= 2.0) the field subtype, one of "None",
   "Boolean", "Int16", "Float32". Defaults to "None".
-  **width**: the field width. Defaults to unknown.
-  **precision**: the field width. Defaults to zero.
-  **src**: the name of the source field to be copied to this one.
   Defaults to the value of "name".
-  **nullable** (GDAL >= 2.0) can be used to specify whether the field
   is nullable. It defaults to "true".

| 

**FeatureCount** (optional, from GDAL 1.10.0) : This element is used to
define the feature count of the layer (when no spatial or attribute
filter is set). This can be useful on static data, when getting the
feature count from the source layer is slow.

| 

**ExtentXMin**, **ExtentYMin**, **ExtentXMax** and **ExtentXMax**
(optional, from GDAL 1.10.0) : Those elements are used to define the
extent of the layer. This can be useful on static data, when getting the
extent from the source layer is slow.

| 

A **OGRVRTWarpedLayer** element (GDAL >= 1.10.0) is used to do
on-the-fly reprojection of a source layer. It may have the following
subelements:

-  **OGRVRTLayer**, **OGRVRTWarpedLayer** or **OGRVRTUnionLayer**
   (mandatory): the source layer to reproject.
-  **SrcSRS** (optional): The value of this element is the spatial
   reference to use for the layer before reprojection. If not specified,
   it is deduced from the source layer.
-  **TargetSRS** (mandatory): The value of this element is the spatial
   reference to use for the layer after reprojection.
-  **ExtentXMin**, **ExtentYMin**, **ExtentXMax** and **ExtentXMax**
   (optional, from GDAL 1.10.0) : Those elements are used to define the
   extent of the layer. This can be useful on static data, when getting
   the extent from the source layer is slow.
-  **WarpedGeomFieldName** (optional, from GDAL 1.11) : The value of
   this element is the geometry field name of the source layer to wrap.
   If not specified, the first geometry field will be used. If there are
   several geometry fields, only the one matching WarpedGeomFieldName
   will be warped; the other ones will be untouched.

A **OGRVRTUnionLayer** element (GDAL >= 1.10.0) is used to concatenate
the content of source layers. It should have a **name** and may have the
following subelements:

-  **OGRVRTLayer**, **OGRVRTWarpedLayer** or **OGRVRTUnionLayer**
   (mandatory and may be repeated): a source layer to add in the union.
-  **PreserveSrcFID** (optional) : may be ON or OFF. If set to ON, the
   FID from the source layer will be used, otherwise a counter will be
   used. Defaults to OFF.
-  **SourceLayerFieldName** (optional) : if specified, an additional
   field (named with the value of SourceLayerFieldName) will be added in
   the layer field definition. For each feature, the value of this field
   will be set with the name of the layer from which the feature comes
   from.
-  **GeometryType** (optional) : see above for the syntax. If not
   specified, the geometry type will be deduced from the geometry type
   of all source layers.
-  **LayerSRS** (optional) : see above for the syntax. If not specified,
   the SRS will be the SRS of the first source layer.
-  **FieldStrategy** (optional, exclusive with **Field** or
   **GeometryField**) : may be **FirstLayer** to use the fields from the
   first layer found, **Union** to use a super-set of all the fields
   from all source layers, or **Intersection** to use a sub-set of all
   the common fields from all source layers. Defaults to **Union**.
-  **Field** (optional, exclusive with **FieldStrategy**) : see above
   for the syntax. Note: the src attribute is not supported in the
   context of a OGRVRTUnionLayer element (field names are assumed to be
   identical).
-  **GeometryField** (optional, exclusive with **FieldStrategy**, GDAL
   >= 1.11) : the **name** attribute and the following sub-elements
   **GeometryType**, **SRS** and **Extent[X|Y][Min|Max]** are available.
-  **FeatureCount** (optional) : see above for the syntax
-  **ExtentXMin**, **ExtentYMin**, **ExtentXMax** and **ExtentXMax**
   (optional) : see above for the syntax

Example: ODBC Point Layer
-------------------------

In the following example (disease.ovf) the worms table from the ODBC
database DISEASE is used to form a spatial layer. The virtual file uses
the "x" and "y" columns to get the spatial location. It also marks the
layer as a point layer, and as being in the WGS84 coordinate system.

::

   <OGRVRTDataSource>

       <OGRVRTLayer name="worms">
           <SrcDataSource>ODBC:DISEASE,worms</SrcDataSource>
       <SrcLayer>worms</SrcLayer>
       <GeometryType>wkbPoint</GeometryType>
           <LayerSRS>WGS84</LayerSRS>
       <GeometryField encoding="PointFromColumns" x="x" y="y"/>
       </OGRVRTLayer>

   </OGRVRTDataSource>

Example: Renaming attributes
----------------------------

It can be useful in some circumstances to be able to rename the field
names from a source layer to other names. This is particularly true when
you want to transcode to a format whose schema is fixed, such as GPX
(<name>, <desc>, etc.). This can be accomplished using SQL this way:

::

   <OGRVRTDataSource>
       <OGRVRTLayer name="remapped_layer">
           <SrcDataSource>your_source.shp</SrcDataSource>
           <SrcSQL>SELECT src_field_1 AS name, src_field_2 AS desc FROM your_source_layer_name</SrcSQL>
       </OGRVRTLayer>
   </OGRVRTDataSource>

This can also be accomplished (from GDAL 1.7.0) using explicit field
definitions:

::

   <OGRVRTDataSource>
       <OGRVRTLayer name="remapped_layer">
           <SrcDataSource>your_source.shp</SrcDataSource>
           <SrcLayer>your_source</SrcSQL>
           <Field name="name" src="src_field_1" />
           <Field name="desc" src="src_field_2" type="String" width="45" />
       </OGRVRTLayer>
   </OGRVRTDataSource>

Example: Transparent spatial filtering (GDAL >= 1.7.0)
------------------------------------------------------

The following example will only return features from the source layer
that intersect the (0,40)-(10,50) region. Furthermore, returned
geometries will be clipped to fit into that region.

::

   <OGRVRTDataSource>
       <OGRVRTLayer name="source">
           <SrcDataSource>source.shp</SrcDataSource>
           <SrcRegion clip="true">POLYGON((0 40,10 40,10 50,0 50,0 40))</SrcRegion>
       </OGRVRTLayer>
   </OGRVRTDataSource>

Example: Reprojected layer (GDAL >= 1.10.0)
-------------------------------------------

The following example will return the source.shp layer reprojected to
EPSG:4326.

::

   <OGRVRTDataSource>
       <OGRVRTWarpedLayer>
           <OGRVRTLayer name="source">
               <SrcDataSource>source.shp</SrcDataSource>
           </OGRVRTLayer>
           <TargetSRS>EPSG:4326</TargetSRS>
       </OGRVRTWarpedLayer>
   </OGRVRTDataSource>

Example: Union layer (GDAL >= 1.10.0)
-------------------------------------

The following example will return a layer that is the concatenation of
source1.shp and source2.shp.

::

   <OGRVRTDataSource>
       <OGRVRTUnionLayer name="unionLayer">
           <OGRVRTLayer name="source1">
               <SrcDataSource>source1.shp</SrcDataSource>
           </OGRVRTLayer>
           <OGRVRTLayer name="source2">
               <SrcDataSource>source2.shp</SrcDataSource>
           </OGRVRTLayer>
       </OGRVRTUnionLayer>
   </OGRVRTDataSource>

Example: SQLite/Spatialite SQL dialect (GDAL >=1.10.0)
------------------------------------------------------

The following example will return four different layers which are
generated in a fly from the same polygon shapefile. The first one is the
shapefile layer as it stands. The second layer gives simplified polygons
by applying SpatiaLite function "Simplify" with parameter tolerance=10.
In the third layer the original geometries are replaced by their convex
hulls. In the fourth layer SpatiaLite function PointOnSurface is used
for replacing the original geometries by points which are inside the
corresponding source polygons. Note that for using the last three layers
of this VRT file GDAL must be compiled with SQLite and SpatiaLite.

::

   <OGRVRTDataSource>
       <OGRVRTLayer name="polygons">
           <SrcDataSource>polygons.shp</SrcDataSource>
       </OGRVRTLayer>
       <OGRVRTLayer name="polygons_as_simplified">
           <SrcDataSource>polygons.shp</SrcDataSource>
           <SrcSQL dialect="sqlite">SELECT Simplify(geometry,10) from polygons</SrcSQL>
       </OGRVRTLayer>
       <OGRVRTLayer name="polygons_as_hulls">
           <SrcDataSource>polygons.shp</SrcDataSource>
           <SrcSQL dialect="sqlite">SELECT ConvexHull(geometry) from polygons</SrcSQL>
       </OGRVRTLayer>
       <OGRVRTLayer name="polygons_as_points">
           <SrcDataSource>polygons.shp</SrcDataSource>
           <SrcSQL dialect="sqlite">SELECT PointOnSurface(geometry) from polygons</SrcSQL>
       </OGRVRTLayer>
   </OGRVRTDataSource>

Example: Multiple geometry fields (GDAL >= 1.11)
------------------------------------------------

The following example will expose all the attribute and geometry fields
of the source layer:

::

   <OGRVRTDataSource>
       <OGRVRTLayer name="test">
           <SrcDataSource>PG:dbname=testdb</SrcDataSource>
       </OGRVRTLayer>
   </OGRVRTDataSource>

To expose only part (or all!) of the fields:

::

   <OGRVRTDataSource>
       <OGRVRTLayer name="other_test">
           <SrcDataSource>PG:dbname=testdb</SrcDataSource>
           <SrcLayer>test</SrcLayer>
           <GeometryField name="pg_geom_field_1" />
           <GeometryField name="vrt_geom_field_2" field="pg_geom_field_2">
               <GeometryType>wkbPolygon</GeometryType>
               <SRS>EPSG:4326</SRS>
               <ExtentXMin>-180</ExtentXMin>
               <ExtentYMin>-90</ExtentYMin>
               <ExtentXMax>180</ExtentXMax>
               <ExtentYMax>90</ExtentYMax>
           </GeometryField>
           <Field name="vrt_field_1" src="src_field_1" />
       </OGRVRTLayer>w
   </OGRVRTDataSource>

To reproject the 'pg_geom_field_2' geometry field to EPSG:4326:

::

   <OGRVRTDataSource>
       <OGRVRTWarpedLayer>
           <OGRVRTLayer name="other_test">
               <SrcDataSource>PG:dbname=testdb</SrcDataSource>
           </OGRVRTLayer>
           <WarpedGeomFieldName>pg_geom_field_2</WarpedGeomFieldName>
           <TargetSRS>EPSG:32631</TargetSRS>
       </OGRVRTWarpedLayer>
   </OGRVRTDataSource>

To make the union of several multi-geometry layers and keep only a few
of them:

::

   <OGRVRTDataSource>
       <OGRVRTUnionLayer name="unionLayer">
           <OGRVRTLayer name="source1">
               <SrcDataSource>PG:dbname=testdb</SrcDataSource>
           </OGRVRTLayer>
           <OGRVRTLayer name="source2">
               <SrcDataSource>PG:dbname=testdb</SrcDataSource>
           </OGRVRTLayer>
           <GeometryField name="pg_geom_field_2">
               <GeometryType>wkbPolygon</GeometryType>
               <SRS>EPSG:4326</SRS>
               <ExtentXMin>-180</ExtentXMin>
               <ExtentYMin>-90</ExtentYMin>
               <ExtentXMax>180</ExtentXMax>
               <ExtentYMax>90</ExtentYMax>
           </GeometryField>
           <GeometryField name="pg_geom_field_3" />
           <Field name="src_field_1" />
       </OGRVRTUnionLayer>
   </OGRVRTDataSource>

Other Notes
-----------

-  When the *GeometryField* is "WKT" spatial filtering is applied after
   extracting all rows from the source datasource. Essentially that
   means there is no fast spatial filtering on WKT derived geometries.
-  When the *GeometryField* is "PointFromColumns", and a *SrcLayer* (as
   opposed to *SrcSQL*) is used, and a spatial filter is in effect on
   the virtual layer then the spatial filter will be internally
   translated into an attribute filter on the X and Y columns in the
   *SrcLayer*. In cases where fast spatial filtering is important it can
   be helpful to index the X and Y columns in the source datastore, if
   that is possible. For instance if the source is an RDBMS. You can
   turn off that feature by setting the *useSpatialSubquery* attribute
   of the GeometryField element to FALSE.
