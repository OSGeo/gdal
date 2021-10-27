.. _vector.gmlas:

GMLAS - Geography Markup Language (GML) driven by application schemas
=====================================================================

.. versionadded:: 2.2

.. shortname:: GMLAS

.. build_dependencies:: Xerces

This driver can read and write XML files of arbitrary structure,
included those containing so called Complex Features, provided that they
are accompanied by one or several XML schemas that describe the
structure of their content. While this driver is generic to any XML
schema, the main target is to be able to read and write documents
referencing directly or indirectly to the GML namespace.

The driver requires Xerces-C >= 3.1.

The driver can deal with files of arbitrary size with a very modest RAM
usage, due to its working in streaming mode.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Opening syntax
--------------

The connection string is GMLAS:/path/to/the.gml. Note the GMLAS: prefix.
If this prefix it is omitted, then the GML driver is likely to be used.

It is also possible to only used "GMLAS:" as the connection string, but
in that case the schemas must be explicitly provided with the XSD open
option.

Mapping of XML structure to OGR layers and fields
-------------------------------------------------

The driver scans the XML schemas referenced by the XML/GML to build the
OGR layers and fields. It is strictly required that the schemas,
directly or indirectly used, are fully valid. The content of the XML/GML
file itself is marginally used, mostly to determine the SRS of geometry
columns.

XML elements declared at the top level of a schema will generally be
exposed as OGR layers. Their attributes and sub-elements of simple XML
types (string, integer, real, ...) will be exposed as OGR fields. For
sub-elements of complex type, different cases can happen. If the
cardinality of the sub-element is at most one and it is not referenced
by other elements, then it is "flattened" into its enclosing element.
Otherwise it will be exposed as a OGR layer, with either a link to its
"parent" layer if the sub-element is specific to its parent element, or
through a junction table if the sub-element is shared by several
parents.

By default the driver is robust to documents non strictly conforming to
the schemas. Unexpected content in the document will be silently
ignored, as well as content required by the schema and absent from the
document.

Consult the :ref:`GMLAS mapping examples <gmlas_mapping_examples>`
page for more details.

By default in the configuration, swe:DataRecord and swe:DataArray
elements from the Sensor Web Enablement (SWE) Common Data Model
namespace will receive a special processing, so they are mapped more
naturally to OGR concepts. The swe:field elements will be mapped as OGR
fields, and the swe:values element of a swe:DataArray will be parsed
into OGR features in a dedicated layer for each swe:DataArray. Note that
those conveniency exposure is for read-only purpose. When using the
write side of the driver, only the content of the general mapping
mechanisms will be used.

Metadata layers
---------------

Three special layers "_ogr_fields_metadata", "_ogr_layers_metadata",
"_ogr_layer_relationships" and "_ogr_other_metadata" add extra
information to the basic ones you can get from the OGR data model on OGR
layers and fields.

Those layers are exposed if the EXPOSE_METADATA_LAYERS open option is
set to YES (or if enabled in the configuration). They can also be
individually retrieved by specifying their name in calls to
GetLayerByName(), or on as layer names with the ogrinfo and ogr2ogr
utility.

Consult the :ref:`GMLAS metadata layers <gmlas_metadata_layers>`
page for more details.

Configuration file
------------------

A default configuration file
`gmlasconf.xml <http://github.com/OSGeo/gdal/blob/master/gdal/data/gmlasconf.xml>`__
file is provided in the data directory of the GDAL installation. Its
structure and content is documented in
`gmlasconf.xsd <http://github.com/OSGeo/gdal/blob/master/gdal/data/gmlasconf.xsd>`__
schema.

This configuration file enables the user to modify the following
settings:

-  whether remote schemas should be downloaded. Enabled by default.
-  whether the local cache of schemas is enabled. Enabled by default.
-  the path of the local cache. By default, $HOME/.gdal/gmlas_xsd_cache
-  whether validation of the document against the schemas should be
   enabled. Disabled by default.
-  whether validation error should cause dataset opening to fail.
   Disabled by default.
-  whether the metadata layers should be exposed by default. Disabled by
   default.
-  whether a 'ogr_pkid' field should always be generated. Disabled by
   default. Turning that on can be useful on layers that have a ID
   attribute whose uniqueness is not guaranteed among various documents.
   Which could cause issues when appending several documents into a
   target database table.
-  whether layers and fields that are not used in the XML document
   should be removed. Disable by default.
-  whether OGR array data types can be used. Enabled by default.
-  whether the XML definition of the GML geometry should be reported as
   a OGR string field. Disabled by default.
-  whether only XML elements that derive from gml:_Feature or
   gml:AbstractFeature should be considered in the initial pass of the
   schema building, when at least one element in the schemas derive from
   them. Enabled by default.
-  several rules to configure if and how xlink:href should be resolved.
-  a definition of XPaths of elements and attributes that must be
   ignored, so as to lighten the number of OGR layers and fields.

This file can be adapted and modified versions can be provided to the
driver with the CONFIG_FILE open option. None of the elements of the
configuration file are required. When they are absent, the default value
indicated in the schema documentation is used.

Configuration can also be provided through other open options. Note that
some open options have identical names to settings present in the
configuration file. When such open option is provided, then its value
will override the one of the configuration file (either the default one,
or the one provided through the CONFIG_FILE open option).

Geometry support
----------------

XML schemas only indicate the geometry type but do not constraint the
spatial reference systems (SRS), so it is theoretically possible to have
object instances of the same class having different SRS for the same
geometry field. This is not practical to deal with, so when geometry
fields are detected, an initial scan of the document is done to find the
first geometry of each geometry field that has an explicit srsName set.
This one will be used for the whole geometry field. In case other
geometries of the same field would have different SRS, they will be
reprojected.

By default, only the OGR geometry built from the GML geometry is exposed
in the OGR feature. It is possible to change the IncludeGeometryXML
setting of the configuration file to true so as to expose a OGR string
field with the XML definition of the GML geometry.

Performance issues with large multi-layer GML files.
----------------------------------------------------

Traditionnaly to read a OGR datasource, one iterate over layers with
GDALDataset::GetLayer(), and for each layer one iterate over features
with OGRLayer::GetNextFeature(). While this approach still works for the
GMLAS driver, it may result in very poor performance on big documents or
documents using complex schemas that are translated in many OGR layers.

It is thus recommended to use GDALDataset::GetNextFeature() to iterate
over features as soon as they appear in the .gml/.xml file. This may
return features from non-sequential layers, when the features include
nested elements.

Open options
------------

-  **XSD**\ =filename(s): to specify an explicit XSD application schema
   to use (or a list of filenames, provided they are comma separated).
   "http://" or "https://" URLs can be used. This option is not required
   when the XML/GML document has a schemaLocation attribute with valid
   links in its root element.
-  **CONFIG_FILE**\ =filename or inline XML definition: filename of a
   XML configuration file conforming to the
   `gmlasconf.xsd <https://github.com/OSGeo/gdal/blob/master/gdal/data/gmlasconf.xsd>`__
   schema. It is also possible to provide the XML content directly
   inlined provided that the very first characters are <Configuration.
-  **EXPOSE_METADATA_LAYERS**\ =YES/NO: whether the metadata layers
   "_ogr_fields_metadata", "_ogr_layers_metadata",
   "_ogr_layer_relationships" and "ogr_other_metadata" should be
   reported by default. Default is NO.
-  **VALIDATE**\ =YES/NO: whether the document should be validated
   against the schemas. Validation is done at dataset opening. Default
   is NO.
-  **FAIL_IF_VALIDATION_ERROR**\ =YES/NO: Whether a validation error
   should cause dataset opening to fail. (only used if VALIDATE=YES)
   Default is NO.
-  **REFRESH_CACHE**\ =YES/NO: Whether remote schemas and documents
   pointed by xlink:href links should be downloaded from the server even
   if already present in the local cache. If the cache is enabled, it
   will be refreshed with the newly downloaded resources. Default is NO.
-  **SWAP_COORDINATES**\ =AUTO/YES/NO: Whether the order of the x/y or
   long/lat coordinates should be swapped. In AUTO mode, the driver will
   determine if swapping must be done from the srsName. If the srsName
   is urn:ogc:def:crs:EPSG::XXXX and that the order of coordinates in
   the EPSG database for this SRS is lat,long or northing,easting, then
   the driver will swap them to the GIS friendly order (long,lat or
   easting,northing). For other forms of SRS (such as EPSG:XXXX), GIS
   friendly order is assumed and thus no swapping is done. When
   SWAP_COORDINATES is set to YES, coordinates will be always swapped
   regarding the order they appear in the GML, and when it set to NO,
   they will be kept in the same order. The default is AUTO.
-  **REMOVE_UNUSED_LAYERS**\ =YES/NO: Whether unused layers should be
   removed from the reported layers. Defaults to NO
-  **REMOVE_UNUSED_FIELDS**\ =YES/NO: Whether unused fields should be
   removed from the reported layers. Defaults to NO
-  **HANDLE_MULTIPLE_IMPORTS**\ =YES/NO: Whether multiple imports with
   the same namespace but different schema are allowed. Defaults to NO
-  **SCHEMA_FULL_CHECKING**\ =YES/NO: Whether to be pedantic with XSD
   checking or to be forgiving e.g. if the invalid part of the schema is
   not referenced in the main document. Defaults to NO

Creation support
----------------

The GMLAS driver can write XML documents in a schema-driven way by
converting a source dataset (contrary to most other drivers that have
read support that implement the CreateLayer() and CreateFeature()
interfaces). The typical workflow is to use the read side of the GMLAS
driver to produce a SQLite/Spatialite/ PostGIS database, potentially
modify the features imported and re-export this database as a new XML
document.

The driver will identify in the source dataset "top-level" layers, and
in those layers will find which features are not referenced by other
top-level layers. As the creation of the output XML is schema-driver,
the schemas need to be available. There are two possible ways:

-  either the result of the processing of the schemas was stored as the
   4 \_ogr_\* metadata tables in the source dataset by using the
   EXPOSE_METADATA_LAYERS=YES open option when converting the source
   .xml),
-  or the schemas can be specified at creation time with the INPUT_XSD
   creation option.

By default, the driver will "wrap" the features inside a WFS 2.0
wfs:FeatureCollection / wfs:member element. It is also possible to ask
the driver to create instead a custom wrapping .xsd file that declares
the ogr_gmlas:FeatureCollection / ogr_gmlas:featureMember XML elements.

Note that while the file resulting from the export should be XML valid,
there is no strong guarantee that it will validate against the
additional constraints expressed in XML schema(s). This will depend on
the content of the features (for example if converting from a GML file
that is not conformant to the schemas, the output of the driver will
generally be not validating)

If the input layers have geometries stored as GML content in a \_xml
suffixed field, then the driver will compare the OGR geometry built from
that XML content with the OGR geometry stored in the dedicated geometry
field of the feature. If both match, then the GML content stored in the
\_xml suffixed field will be used, such as to preserve particularities
of the initial GML content. Otherwise GML will be exported from the OGR
geometry.

To increase export performance on very large databases, creating
attribute indexes on the fields pointed by the 'layer_pkid_name'
attribute in '_ogr_layers_metadata' might help.

ogr2ogr behavior
~~~~~~~~~~~~~~~~~

When using ogr2ogr / GDALVectorTranslate() to convert to XML/GML from a
source database, there are restrictions to the options that can be used.
Only the following options of ogr2ogr are supported:

-  dataset creation options (see below)
-  layer names
-  spatial filter through -spat option.
-  attribute filter through -where option

The effect of spatial and attribute filtering will only apply on
top-levels layers. Sub-features selected through joins will not be
affected by those filters.

Dataset creation options
~~~~~~~~~~~~~~~~~~~~~~~~

The supported dataset creation options are:

-  **INPUT_XSD**\ =filename(s): to specify an explicit XSD application
   schema to use (or a list of filenames, provided they are comma
   separated). "http://" or "https://" URLs can be used. This option is
   not required when the source dataset has a \_ogr_other_metadata with
   schemas and locations filled.
-  **CONFIG_FILE**\ =filename or inline XML definition: filename of a
   XML configuration file conforming to the
   `gmlasconf.xsd <https://github.com/OSGeo/gdal/blob/master/gdal/data/gmlasconf.xsd>`__
   schema. It is also possible to provide the XML content directly
   inlined provided that the very first characters are <Configuration>.
-  **LAYERS**\ =layers. Comma separated list of layers to export as
   top-level features. The special value "{SPATIAL_LAYERS}" can also be
   used to specify all layers that have geometries. When LAYERS is not
   specified, the driver will identify in the source dataset "top-level"
   layers, and in those layers will find which features are not
   referenced by other top-level layers.
-  **SRSNAME_FORMAT**\ =SHORT/OGC_URN/OGC_URL (Only valid for GML 3
   output) Defaults to OGC_URL. If SHORT, then srsName will be in the
   form AUTHORITY_NAME:AUTHORITY_CODE If OGC_URN, then srsName will be
   in the form urn:ogc:def:crs:AUTHORITY_NAME::AUTHORITY_CODE If
   OGC_URL, then srsName will be in the form
   http://www.opengis.net/def/crs/AUTHORITY_NAME/0/AUTHORITY_CODE For
   OGC_URN and OGC_URL, in the case the SRS is a SRS without explicit
   AXIS order, but that the same SRS authority code imported with
   ImportFromEPSGA() should be treated as lat/long or northing/easting,
   then the function will take care of coordinate order swapping.
-  **INDENT_SIZE**\ =[0-8]. Number of spaces for each indentation level.
   Default is 2.
-  **COMMENT**\ =string. Comment to add at top of generated XML file as
   a XML comment.
-  **LINEFORMAT**\ =CRLF/LF. End-of-line sequence to use. Defaults to
   CRLF on Windows and LF on other platforms.
-  **WRAPPING**\ =WFS2_FEATURECOLLECTION/GMLAS_FEATURECOLLECTION.
   Whether to wrap features in a wfs:FeatureCollection or in a
   ogr_gmlas:FeatureCollection. Defaults to WFS2_FEATURECOLLECTION.
-  **TIMESTAMP**\ =XML date time. User-specified XML dateTime value for
   timestamp to use in wfs:FeatureCollection attribute. If not
   specified, current date time is used. Only valid for
   WRAPPING=WFS2_FEATURECOLLECTION.
-  **WFS20_SCHEMALOCATION**\ =Path or URL to wfs.xsd. Only valid for
   WRAPPING=WFS2_FEATURECOLLECTION. Default is
   "http://schemas.opengis.net/wfs/2.0/wfs.xsd"
-  **GENERATE_XSD**\ =YES/NO. Whether to generate a .xsd file that has
   the structure of the wrapping ogr_gmlas:FeatureCollection /
   ogr_gmlas:featureMember elements. Only valid for
   WRAPPING=GMLAS_FEATURECOLLECTION. Default to YES.
-  **OUTPUT_XSD_FILENAME**\ =string. Wrapping .xsd filename. If not
   specified, same basename as output file with .xsd extension. Note
   that it is possible to use this option even if GENERATE_XSD=NO, so
   that the wrapping .xsd appear in the schemaLocation attribute of the
   .xml file. Only valid for WRAPPING=GMLAS_FEATURECOLLECTION

Examples
--------

Listing content of a data file:

::

   ogrinfo -ro GMLAS:my.gml

Converting to PostGIS:

::

   ogr2ogr -f PostgreSQL PG:'host=myserver dbname=warmerda' GMLAS:my.gml -nlt CONVERT_TO_LINEAR

Converting to Spatialite and back to GML

::

   ogr2ogr -f SQLite tmp.sqlite GMLAS:in.gml -dsco SPATILIATE=YES -nlt CONVERT_TO_LINEAR -oo EXPOSE_METADATA_LAYERS=YES
   ogr2ogr -f GMLAS out.gml tmp.sqlite

See Also
--------

-  :ref:`GML <vector.gml>`: general purpose driver not requiring the
   presence of schemas, but with limited support for complex features
-  :ref:`NAS/ALKIS <vector.nas>`: specialized GML driver for cadastral
   data in Germany

Credits
-------

Initial implementation has been funded by the European Union's Earth
observation programme Copernicus, as part of the tasks delegated to the
European Environment Agency.

Development of special processing of some Sensor Web Enablement (SWE)
Common Data Model swe:DataRecord and swe:DataArray constructs has been
funded by Bureau des Recherches Géologiques et Minières (BRGM).

.. toctree::
   :maxdepth: 1
   :hidden:

   gmlas_mapping_examples
   gmlas_metadata_layers

