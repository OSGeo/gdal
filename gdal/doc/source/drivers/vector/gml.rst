.. _vector.gml:

GML - Geography Markup Language
===============================

.. shortname:: GML

.. build_dependencies:: (read support needs Xerces or libexpat)

OGR has limited support for GML reading and writing. Update of existing
files is not supported.

Supported GML flavors :

======================================= =================================
Read                                    Write
======================================= =================================
GML2 and GML3 that can                  GML 2.1.2 or GML 3 SF-0
be translated into simple feature model (GML 3.1.1 Compliance level SF-0)
======================================= =================================

Starting with GDAL 2.2, another driver, :ref:`GMLAS <vector.gmlas>`, for
GML driven by application schemas, is also available. Both GML and GMLAS
drivers have their use cases.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Parsers
-------

The reading part of the driver only works if OGR is built with Xerces
linked in. When Xerces is unavailable, read
support also works if OGR is built with Expat linked in. XML validation
is disabled by default. GML writing is always supported, even without
Xerces or Expat.

Note: if both Xerces and Expat are available at
build time, the GML driver will preferentially select at runtime the
Expat parser for cases where it is possible (GML file in a compatible
encoding), and default back to Xerces parser in other cases. However,
the choice of the parser can be overridden by specifying the
**GML_PARSER** configuration option to **EXPAT** or **XERCES**.

CRS support
-----------

The GML driver has coordinate system support. This is
only reported when all the geometries of a layer have a srsName
attribute, whose value is the same for all geometries. For srsName such
as "urn:ogc:def:crs:EPSG:" (or "http://www.opengis.net/def/crs/EPSG/0/"
starting with GDAL 2.1.2), for geographic coordinate systems (as
returned by WFS 1.1.0 for example), the axis order should be (latitude,
longitude) as required by the standards, but this is unusual and can
cause issues with applications unaware of axis order. So by default, the
driver will swap the coordinates so that they are in the (longitude,
latitude) order and report a SRS without axis order specified. It is
possible to get the original (latitude, longitude) order and SRS with
axis order by setting the configuration option
**GML_INVERT_AXIS_ORDER_IF_LAT_LONG** to **NO**.

There also situations where the srsName is of the form "EPSG:XXXX"
(whereas "urn:ogc:def:crs:EPSG::XXXX" would have been more explicit on
the intent) and the coordinates in the file are in (latitude, longitude)
order. By default, OGR will not consider the EPSG axis order and will
report the coordinates in (latitude,longitude) order. However, if you
set the configuration option **GML_CONSIDER_EPSG_AS_URN** to **YES**,
the rules explained in the previous paragraph will be applied.

The above also applied for projected coordinate systems
whose EPSG preferred axis order is (northing, easting).

Starting with GDAL 2.1.2, the SWAP_COORDINATES open option (or
GML_SWAP_COORDINATES configuration option) can be set to AUTO/YES/NO. It
controls whether the order of the x/y or long/lat coordinates should be
swapped. In AUTO mode, the driver will determine if swapping must be
done from the srsName and value of other options like
CONSIDER_EPSG_AS_URN and INVERT_AXIS_ORDER_IF_LAT_LONG. When
SWAP_COORDINATES is set to YES, coordinates will be always swapped
regarding the order they appear in the GML, and when it set to NO, they
will be kept in the same order. The default is AUTO.

Schema
------

In contrast to most GML readers, the OGR GML reader does not require the
presence of an XML Schema definition of the feature classes (file with
.xsd extension) to be able to read the GML file. If the .xsd file is
absent or OGR is not able to parse it, the driver attempts to
automatically discover the feature classes and their associated
properties by scanning the file and looking for "known" gml objects in
the gml namespace to determine the organization. While this approach is
error prone, it has the advantage of working for GML files even if the
associated schema (.xsd) file has been lost.

It is possible to specify an explicit filename
for the XSD schema to use, by using
"a_filename.gml,xsd=another_filename.xsd" as a connection string.
The XSD can also be specified as the value of the
XSD open option.

The first time a GML file is opened, if the associated .xsd is absent or
could not been parsed correctly, it is completely scanned in order to
determine the set of featuretypes, the attributes associated with each
and other dataset level information. This information is stored in a
.gfs file with the same basename as the target gml file. Subsequent
accesses to the same GML file will use the .gfs file to predefine
dataset level information accelerating access. To a limited extent the
.gfs file can be manually edited to alter how the GML file will be
parsed. Be warned that the .gfs file will be ignored if the associated
.gml file has a newer timestamp.

When prescanning the GML file to determine the list of feature types,
and fields, the contents of fields are scanned to try and determine the
type of the field. In some applications it is easier if all fields are
just treated as string fields. This can be accomplished by setting the
configuration option **GML_FIELDTYPES** to the value **ALWAYS_STRING**.

The **GML_ATTRIBUTES_TO_OGR_FIELDS**
configuration option can be set to **YES** so that attributes of GML
elements are also taken into account to create OGR fields.

Configuration options can be set via the CPLSetConfigOption() function
or as environment variables.

You can use **GML_GFS_TEMPLATE** configuration option (or **GFS_TEMPLATE**
open option) set to a **path_to_template.gfs** in order
to unconditionally use a predefined GFS file. This option is
really useful when you are planning to import many distinct GML
files in subsequent steps [**-append**] and you absolutely want to
preserve a fully consistent data layout for the whole GML set.
Please, pay attention not to use the **-lco LAUNDER=yes** setting
when using **GML_GFS_TEMPLATE**; this should break the correct
recognition of attribute names between subsequent GML import runs.

Particular GML application schemas
----------------------------------

Feature attributes in nested GML elements (non-flat attribute hierarchy) that
can be found in some GML profiles, such as UK Ordnance Survey MasterMap, are
detected. IntegerList, RealList and StringList field types
when a GML element has several occurrences are also supported.

A specialized GML driver - the :ref:`NAS <vector.nas>`
driver - is available to read German AAA GML Exchange Format
(NAS/ALKIS).

The GML driver has partial support for reading AIXM or
CityGML files.

The GML driver supports reading :

-  `Finnish National Land Survey GML files (a.k.a MTK GML) for
   topographic
   data. <http://xml.nls.fi/XML/Schema/Maastotietojarjestelma/MTK/201202/Maastotiedot.xsd>`__
-  `Finnish National Land Survey GML files for cadastral
   data <http://xml.nls.fi/XML/Schema/sovellus/ktjkii/modules/kiinteistotietojen_kyselypalvelu_WFS/Asiakasdokumentaatio/ktjkiiwfs/2010/02/>`__.
-  `Cadastral data in Inspire GML
   schemas <http://inspire.ec.europa.eu/schemas/cp/3.0/CadastralParcels.xsd>`__.
-  `Czech RUIAN Exchange Format
   (VFR) <http://www.cuzk.cz/Uvod/Produkty-a-sluzby/RUIAN/2-Poskytovani-udaju-RUIAN-ISUI-VDP/Vymenny-format-RUIAN/Vymenny-format-RUIAN-%28VFR%29.aspx>`__.

The GML driver supports reading responses to CSW GetRecords queries.

Since OGR 2.2, the GML driver supports reading Japanese FGD GML v4
files.

Geometry reading
----------------

When reading a feature, the driver will by default only take into
account the last recognized GML geometry found (in case they are
multiples) in the XML subtree describing the feature.

But, if the .xsd schema is understood by the XSD
parser and declares several geometry fields, or the .gfs file declares
several geometry fields, multiple geometry fields will be reported by
the GML driver according to :ref:`rfc-41`.

In case of multiple geometry occurrences, if a
geometry is in a <geometry> element, this will be the one selected. This
will make default behavior consistent with Inspire objects.

The user can change the .gfs file to select the
appropriate geometry by specifying its path with the
<GeometryElementPath> element. See the description of the .gfs syntax
below.

GML geometries including TopoCurve, TopoSurface, MultiCurve are also supported.
The TopoCurve type GML geometry can be
interpreted as either of two types of geometries. The Edge elements in
it contain curves and their corresponding nodes. By default only the
curves, the main geometries, are reported as OGRMultiLineString. To
retrieve the nodes, as OGRMultiPoint, the configuration option
**GML_GET_SECONDARY_GEOM** should be set to the value **YES**. When this
is set only the secondary geometries are reported.

Arc, ArcString, ArcByBulge, ArcByCenterPoint,
Circle and CircleByCenterPoints will be returned as circular string OGR
geometries. If they are included in other GML elements such as
CurveComposite, MultiCurve, Surface, corresponding non-linear OGR
geometries will be returned as well. When reading GML3 application
schemas, declarations of geometry fields such as CurvePropertyType,
SurfacePropertyType, MultiCurvePropertyType or MultiSurfacePropertyType
will be also interpreted as being potential non-linear geometries, and
corresponding OGR geometry type will be used for the layer geometry
type.

gml:xlink resolving
-------------------

gml:xlink resolving is supported. When the resolver finds
an element containing the tag xlink:href, it tries to find the
corresponding element with the gml:id in the same gml file, other gml
file in the file system or on the web using cURL. Set the configuration
option **GML_SKIP_RESOLVE_ELEMS** to **NONE** to enable resolution.

By default the resolved file will be saved in the same directory as the
original file with the extension ".resolved.gml", if it doesn't exist
already. This behavior can be changed using the configuration option
**GML_SAVE_RESOLVED_TO**. Set it to **SAME** to overwrite the original
file. Set it to a **filename ending with .gml** to save it to that
location. Any other values are ignored. If the resolver cannot write to
the file for any reason, it will try to save it to a temporary file
generated using CPLGenerateTempFilename("ResolvedGML"); if it cannot,
resolution fails.

Note that the resolution algorithm is not optimized for large files. For
files with more than a couple of thousand xlink:href tags, the process
can go beyond a few minutes. A rough progress is displayed through
CPLDebug() for every 256 links. It can be seen by setting the
environment variable CPL_DEBUG. The resolution time can be reduced if
you know any elements that will not be needed. Mention a comma separated
list of names of such elements with the configuration option
**GML_SKIP_RESOLVE_ELEMS**. Set it to **ALL** to skip resolving
altogether (default action). Set it to **NONE** to resolve all the
xlinks.

An alternative resolution method is available.
This alternative method will be activated using the configuration option
**GML_SKIP_RESOLVE_ELEMS HUGE**. In this case any gml:xlink will be
resolved using a temporary SQLite DB so to identify any corresponding
gml:id relation. At the end of this SQL-based process, a resolved file
will be generated exactly as in the **NONE** case but without their
limits. The main advantages in using an external (temporary) DBMS so to
resolve gml:xlink and gml:id relations are the following:

-  no memory size constraints. The **NONE** method stores the whole GML
   node-tree in-memory; and this practically means that no GML file
   bigger than 1 GB can be processed at all using a 32-bit platform, due
   to memory allocation limits. Using a file-system based DBMS avoids at
   all this issue.
-  by far better efficiency, most notably when huge GML files containing
   many thousands (or even millions) of xlink:href / gml:id relational
   pairs.
-  using the **GML_SKIP_RESOLVE_ELEMS HUGE** method realistically allows
   to successfully resolve some really huge GML file (3GB+) containing
   many millions xlink:href / gml:id in a reasonable time (about an hour
   or so on).
-  The **GML_SKIP_RESOLVE_ELEMS HUGE** method supports the following
   further configuration option:

TopoSurface interpretation rules [polygons and internal holes]
--------------------------------------------------------------

The GML driver is able to recognize two
different interpretation rules for TopoSurface when a polygon contains
any internal hole:

-  the previously supported interpretation rule assumed that:

   -  each TopoSurface may be represented as a collection of many Faces
   -  *positive* Faces [i.e. declaring **orientation="+"**] are assumed
      to represent the Exterior Ring of some Polygon.
   -  *negative* Faces [i.e. declaring **orientation="-"**] are assumed
      to represent an Interior Ring (aka *hole*) belonging to the latest
      declared Exterior Ring.
   -  ordering any Edge used to represent each Ring is important: each
      Edge is expected to be exactly adjacent to the next one.

-  the new interpretation rule now assumes that:

   -  each TopoSurface may be represented as a collection of many Faces
   -  the declared **orientation** for any Face has nothing to deal with
      Exterior/Interior Rings
   -  each Face is now intended to represent a complete Polygon,
      eventually including any possible Interior Ring (*holes*)
   -  the relative ordering of any Edge composing the same Face is
      completely not relevant

The newest interpretation seems to fully match GML 3 standard
recommendations; so this latest is now assumed to be the default
interpretation supported by OGR.

**NOTE** : Using the newest interpretation requires GDAL/OGR to be built
against the GEOS library.

Using the **GML_FACE_HOLE_NEGATIVE** configuration option you can anyway
select the actual interpretation to be applied when parsing GML 3
Topologies:

-  setting **GML_FACE_HOLE_NEGATIVE NO** (*default* option) will
   activate the newest interpretation rule
-  but explicitly setting **GML_FACE_HOLE_NEGATIVE YES** still enables
   to activate the old interpretation rule

Encoding issues
---------------

Expat library supports reading the following built-in encodings :

-  US-ASCII
-  UTF-8
-  UTF-16
-  ISO-8859-1
-  Windows-1252

The content returned by OGR will be encoded in UTF-8, after the
conversion from the encoding mentioned in the file header is.

If the GML file is not encoded in one of the previous encodings and the
only parser available is Expat, it will not be parsed by the GML driver.
You may convert it into one of the supported encodings with the *iconv*
utility for example and change accordingly the *encoding* parameter
value in the XML header.

When writing a GML file, the driver expects UTF-8 content to be passed
in.

Note: The .xsd schema files are parsed with an integrated XML parser
which does not currently understand XML encodings specified in the XML
header. It expects encoding to be always UTF-8. If attribute names in
the schema file contains non-ascii characters, it is better to use
*iconv* utility and convert the .xsd file into UTF-8 encoding first.

Feature id (fid / gml:id)
-------------------------

The driver exposes the content of the gml:id
attribute as a string field called *gml_id*, when reading GML WFS
documents. When creating a GML3 document, if a field is called *gml_id*,
its content will also be used to write the content of the gml:id
attribute of the created feature.

The driver autodetects the presence of a fid
(GML2) (resp. gml:id (GML3)) attribute at the beginning of the file,
and, if found, exposes it by default as a *fid* (resp. *gml_id*) field.
The autodetection can be overridden by specifying the **GML_EXPOSE_FID**
or **GML_EXPOSE_GML_ID** configuration option to **YES** or **NO**.

When creating a GML2 document, if a field is
called *fid*, its content will also be used to write the content of the
fid attribute of the created feature.

Performance issues with large multi-layer GML files.
----------------------------------------------------

There is only one GML parser per GML datasource shared among the various
layers. By default, the GML driver will restart reading from the
beginning of the file, each time a layer is accessed for the first time,
which can lead to poor performance with large GML files.

The **GML_READ_MODE** configuration option can
be set to **SEQUENTIAL_LAYERS** if all features belonging to the same
layer are written sequentially in the file. The reader will then avoid
unnecessary resets when layers are read completely one after the other.
To get the best performance, the layers must be read in the order they
appear in the file.

If no .xsd and .gfs files are found, the parser will detect the layout
of layers when building the .gfs file. If the layers are found to be
sequential, a *<SequentialLayers>true</SequentialLayers>* element will
be written in the .gfs file, so that the GML_READ_MODE will be
automatically initialized to SEQUENTIAL_LAYERS if not explicitly set by
the user.

The GML_READ_MODE configuration option can be
set to INTERLEAVED_LAYERS to be able to read a GML file whose features
from different layers are interleaved. In the case, the semantics of the
GetNextFeature() will be slightly altered, in a way where a NULL return
does not necessarily mean that all features from the current layer have
been read, but it could also mean that there is still a feature to read,
but that belongs to another layer. In that case, the file should be read
with code similar to the following one :

::

       int nLayerCount = poDS->GetLayerCount();
       int bFoundFeature;
       do
       {
           bFoundFeature = FALSE;
           for( int iLayer = 0; iLayer < nLayerCount; iLayer++ )
           {
               OGRLayer   *poLayer = poDS->GetLayer(iLayer);
               OGRFeature *poFeature;
               while((poFeature = poLayer->GetNextFeature()) != NULL)
               {
                   bFoundFeature = TRUE;
                   poFeature->DumpReadable(stdout, NULL);
                   OGRFeature::DestroyFeature(poFeature);
               }
           }
       } while (bInterleaved && bFoundFeature);

Open options
------------

-  **XSD=filename**: to specify an explicit filename for
   the XSD application schema to use.
-  **WRITE_GFS=AUTO/YES/NO**: (GDAL >= 3.2) whether to write a .gfs file.
   In AUTO mode, the .gfs file is only written if there is no recognized .xsd
   file, no existing .gfs file and for non-network file systems. This option
   can be set to YES for force .gfs file writing in situations where AUTO would
   not attempt to do it. Or it can be set to NO to disable .gfs file writing.
-  **GFS_TEMPLATE=filename**: to unconditionally use a predefined GFS file.
   This option is really useful when you are planning to import many distinct GML
   files in subsequent steps [**-append**] and you absolutely want to
   preserve a fully consistent data layout for the whole GML set.
   Please, pay attention not to use the **-lco LAUNDER=yes** setting
   when this option; this should break the correct
   recognition of attribute names between subsequent GML import runs.
-  **FORCE_SRS_DETECTION=YES/NO**: Force a full scan to
   detect the SRS of layers. This option may be needed in the case where
   the .gml file is accompanied with a .xsd. Normally in that situation,
   OGR would not detect the SRS, because this requires to do a full scan
   of the file. Defaults to NO
-  **EMPTY_AS_NULL=YES/NO**: By default
   (EMPTY_AS_NULL=YES), fields with empty content will be reported as
   being NULL, instead of being an empty string. This is the historic
   behavior. However this will prevent such fields to be declared as
   not-nullable if the application schema declared them as mandatory. So
   this option can be set to NO to have both empty strings being report
   as such, and mandatory fields being reported as not nullable.
-  **GML_ATTRIBUTES_TO_OGR_FIELDS=YES/NO**: Whether GML
   attributes should be reported as OGR fields. Note that this option
   has only an effect the first time a GML file is opened (before the
   .gfs file is created), and if it has no valid associated .xsd.
   Defaults to NO.
-  **INVERT_AXIS_ORDER_IF_LAT_LONG=YES/NO**: Whether to
   present SRS and coordinate ordering in traditional GIS order.
   Defaults to YES.
-  **CONSIDER_EPSG_AS_URN=YES/NO/AUTO**: Whether to
   consider srsName like EPSG:XXXX as respecting EPSG axis order.
   Defaults to AUTO.
-  **SWAP_COORDINATES**\ =AUTO/YES/NO: (GDAL >= 2.1.2) Whether the order
   of the x/y or long/lat coordinates should be swapped. In AUTO mode,
   the driver will determine if swapping must be done from the srsName
   and value of other options like CONSIDER_EPSG_AS_URN and
   INVERT_AXIS_ORDER_IF_LAT_LONG. When SWAP_COORDINATES is set to YES,
   coordinates will be always swapped regarding the order they appear in
   the GML, and when it set to NO, they will be kept in the same order.
   The default is AUTO.
-  **READ_MODE=AUTO/STANDARD/SEQUENTIAL_LAYERS/INTERLEAVED_LAYERS**:
   Read mode. Defaults to AUTO.
-  **EXPOSE_GML_ID=YES/NO/AUTO**: Whether to make feature
   gml:id as a gml_id attribute. Defaults to AUTO.
-  **EXPOSE_FID=YES/NO/AUTO**: Whether to make feature fid
   as a fid attribute. Defaults to AUTO.
-  **DOWNLOAD_SCHEMA=YES/NO**: Whether to download the
   remote application schema if needed (only for WFS currently).
   Defaults to YES.
-  **REGISTRY=filename**: Filename of the registry with
   application schemas. Defaults to {GDAL_DATA}/gml_registry.xml.

Creation Issues
---------------

On export all layers are written to a single GML file all in a single
feature collection. Each layer's name is used as the element name for
objects from that layer. Geometries are always written as the
ogr:geometryProperty element on the feature.

The GML writer supports the following dataset creation options:

-  **XSISCHEMAURI**: If provided, this URI will be inserted as the
   schema location. Note that the schema file isn't actually accessed by
   OGR, so it is up to the user to ensure it will match the schema of
   the OGR produced GML data file.
-  **XSISCHEMA**: This can be EXTERNAL, INTERNAL or OFF and defaults to
   EXTERNAL. This writes a GML application schema file to a
   corresponding .xsd file (with the same basename). If INTERNAL is used
   the schema is written within the GML file, but this is experimental
   and almost certainly not valid XML. OFF disables schema generation
   (and is implicit if XSISCHEMAURI is used).
-  **PREFIX** Defaults to 'ogr'. This is the prefix for
   the application target namespace.
-  **STRIP_PREFIX** Defaults to FALSE. Can be set to TRUE
   to avoid writing the prefix of the application target namespace in
   the GML file.
-  **TARGET_NAMESPACE** Defaults to
   'http://ogr.maptools.org/'. This is the application target namespace.
-  **FORMAT**: This can be set to :

   -  *GML3* in order to write GML files that follow GML 3.1.1 SF-0
      profile.
   -  *GML3Deegree* in order to produce a GML 3.1.1 .XSD
      schema, with a few variations with respect to what is recommended
      by GML3 SF-0 profile, but that will be better accepted by some
      software (such as Deegree 3).
   -  *GML3.2*\ in order to write GML files that follow
      GML 3.2.1 SF-0 profile.

   If not specified, GML2 will be used.
   Non-linear geometries can be written. This is
   only compatible with selecting on of that above GML3 format variant.
   Otherwise, such geometries will be approximating into their closest
   matching linear geometry.
   Note: fields of type StringList, RealList or
   IntegerList can be written. This will cause to advertise the SF-1
   profile in the .XSD schema (such types are not supported by SF-0).
-  **GML_FEATURE_COLLECTION**\ =YES/NO (OGR >= 2.3) Whether to use the
   gml:FeatureCollection, instead of creating a dedicated container
   element in the target namespace. Only valid for FORMAT=GML3/GML3.2.
   Note that gml:FeatureCollection has been deprecated in GML 3.2, and
   is not allowed by the OGC 06-049r1 "Geography Markup Language (GML)
   simple features profile" (for GML 3.1.1) and OGC 10-100r3 "Geography
   Markup Language (GML) simple features profile (with Corrigendum)"
   (for GML 3.2) specifications.
-  **GML3_LONGSRS**\ =YES/NO. (only valid when
   FORMAT=GML3/GML3Degree/GML3.2) Deprecated by SRSNAME_FORMAT in GDAL
   2.2. Default to YES. If YES, SRS with EPSG authority will be written
   with the "urn:ogc:def:crs:EPSG::" prefix. In the case the SRS is a
   SRS without explicit AXIS order, but that the same SRS authority code
   imported with ImportFromEPSGA() should be treated as lat/long or
   northing/easting, then the function will take care of coordinate
   order swapping. If set to NO, SRS with EPSG authority will be written
   with the "EPSG:" prefix, even if they are in lat/long order.
-  **SRSNAME_FORMAT**\ =SHORT/OGC_URN/OGC_URL (Only valid for
   FORMAT=GML3/GML3Degree/GML3.2, GDAL >= 2.2). Defaults to OGC_URN. If
   SHORT, then srsName will be in the form AUTHORITY_NAME:AUTHORITY_CODE
   If OGC_URN, then srsName will be in the form
   urn:ogc:def:crs:AUTHORITY_NAME::AUTHORITY_CODE If OGC_URL, then
   srsName will be in the form
   http://www.opengis.net/def/crs/AUTHORITY_NAME/0/AUTHORITY_CODE For
   OGC_URN and OGC_URL, in the case the SRS is a SRS without explicit
   AXIS order, but that the same SRS authority code imported with
   ImportFromEPSGA() should be treated as lat/long or northing/easting,
   then the function will take care of coordinate order swapping.
-  **SRSDIMENSION_LOC**\ =POSLIST/GEOMETRY/GEOMETRY,POSLIST. (Only valid
   for FORMAT=GML3/GML3Degree/GML3.2) Default to POSLIST.
   For 2.5D geometries, define the location where to attach the
   srsDimension attribute. There are diverging implementations. Some put
   in on the <gml:posList> element, other on the top geometry element.
-  **WRITE_FEATURE_BOUNDED_BY**\ =YES/NO. (only valid when
   FORMAT=GML3/GML3Degree/GML3.2) Default to YES. If set to NO, the
   <gml:boundedBy> element will not be written for each feature.
-  **SPACE_INDENTATION**\ =YES/NO. Default to YES. If
   YES, the output will be indented with spaces for more readability,
   but at the expense of file size.
-  **GML_ID**\ =string. (Only valid for GML 3.2) Value of
   feature collection gml:id. Default value is "aFeatureCollection".
-  **NAME**\ =string. Content of GML name element. Can also be set as
   the NAME metadata item on the dataset.
-  **DESCRIPTION**\ =string. Content of GML description element. Can
   also be set as the DESCRIPTION metadata item on the dataset.

VSI Virtual File System API support
-----------------------------------

The driver supports reading and writing to files managed by VSI Virtual
File System API, which include "regular" files, as well as files in the
/vsizip/ (read-write) , /vsigzip/ (read-write) , /vsicurl/ (read-only)
domains.

Writing to /dev/stdout or /vsistdout/ is also supported. Note that in
that case, only the content of the GML file will be written to the
standard output (and not the .xsd). The <boundedBy> element will not be
written. This is also the case if writing in /vsigzip/

Syntax of .gfs file by example
------------------------------

Let's consider the following test.gml file :

.. code-block:: XML

   <?xml version="1.0" encoding="UTF-8"?>
   <gml:FeatureCollection xmlns:gml="http://www.opengis.net/gml">
     <gml:featureMember>
       <LAYER>
         <attrib1>attrib1_value</attrib1>
         <attrib2container>
           <attrib2>attrib2_value</attrib2>
         </attrib2container>
         <location1container>
           <location1>
               <gml:Point><gml:coordinates>3,50</gml:coordinates></gml:Point>
           </location1>
         </location1container>
         <location2>
           <gml:Point><gml:coordinates>2,49</gml:coordinates></gml:Point>
         </location2>
       </LAYER>
     </gml:featureMember>
   </gml:FeatureCollection>

and the following associated .gfs file.

.. code-block:: XML

   <GMLFeatureClassList>
     <GMLFeatureClass>
       <Name>LAYER</Name>
       <ElementPath>LAYER</ElementPath>
       <GeometryElementPath>location1container|location1</GeometryElementPath>
       <PropertyDefn>
         <Name>attrib1</Name>
         <ElementPath>attrib1</ElementPath>
         <Type>String</Type>
         <Width>13</Width>
       </PropertyDefn>
       <PropertyDefn>
         <Name>attrib2</Name>
         <ElementPath>attrib2container|attrib2</ElementPath>
         <Type>String</Type>
         <Width>13</Width>
       </PropertyDefn>
     </GMLFeatureClass>
   </GMLFeatureClassList>

Note the presence of the '|' character in the <ElementPath> and
<GeometryElementPath> elements to specify the wished field/geometry
element that is a nested XML element. Nested field elements are supported,
as well as specifying <GeometryElementPath> If
GeometryElementPath is not specified, the GML driver will use the last
recognized geometry element.

The <GeometryType> element can be specified to force the geometry type.
Accepted values are : 0 (any geometry type), 1 (point), 2 (linestring),
3 (polygon), 4 (multipoint), 5 (multilinestring), 6 (multipolygon), 7
(geometrycollection).

The <GeometryElementPath> and <GeometryType> can
be specified as many times as there are geometry fields in the GML file.
Another possibility is to define a <GeomPropertyDefn>element as many
times as necessary:

.. code-block:: XML

   <GMLFeatureClassList>
     <GMLFeatureClass>
       <Name>LAYER</Name>
       <ElementPath>LAYER</ElementPath>
       <GeomPropertyDefn>
           <Name>geometry</Name> <!-- OGR geometry name -->
           <ElementPath>geometry</ElementPath> <!-- XML element name possibly with '|' to specify the path -->
           <Type>MultiPolygon</Type>
       </GeomPropertyDefn>
       <GeomPropertyDefn>
           <Name>referencePoint</Name>
           <ElementPath>referencePoint</ElementPath>
           <Type>Point</Type>
       </GeomPropertyDefn>
     </GMLFeatureClass>
   </GMLFeatureClassList>

The output of *ogrinfo test.gml -ro -al* is:

::

   Layer name: LAYER
   Geometry: Unknown (any)
   Feature Count: 1
   Extent: (3.000000, 50.000000) - (3.000000, 50.000000)
   Layer SRS WKT:
   (unknown)
   Geometry Column = location1container|location1
   attrib1: String (13.0)
   attrib2: String (13.0)
   OGRFeature(LAYER):0
     attrib1 (String) = attrib1_value
     attrib2 (String) = attrib2_value
     POINT (3 50)

Advanced .gfs syntax
--------------------

Specifying ElementPath to find objects embedded into top level objects
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Let's consider the following test.gml file :

.. code-block:: XML

   <?xml version="1.0" encoding="utf-8"?>
   <gml:FeatureCollection xmlns:xlink="http://www.w3.org/1999/xlink"
                          xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                          gml:id="foo" xmlns:gml="http://www.opengis.net/gml/3.2">
     <gml:featureMember>
       <TopLevelObject gml:id="TopLevelObject.1">
         <content>
           <Object gml:id="Object.1">
             <geometry>
               <gml:Polygon gml:id="Object.1.Geometry" srsName="urn:ogc:def:crs:EPSG::4326">
                 <gml:exterior>
                   <gml:LinearRing>
                     <gml:posList srsDimension="2">48 2 49 2 49 3 48 3 48 2</gml:posList>
                   </gml:LinearRing>
                 </gml:exterior>
               </gml:Polygon>
             </geometry>
             <foo>bar</foo>
           </Object>
         </content>
         <content>
           <Object gml:id="Object.2">
             <geometry>
               <gml:Polygon gml:id="Object.2.Geometry" srsName="urn:ogc:def:crs:EPSG::4326">
                 <gml:exterior>
                   <gml:LinearRing>
                     <gml:posList srsDimension="2">-48 2 -49 2 -49 3 -48 3 -48 2</gml:posList>
                   </gml:LinearRing>
                 </gml:exterior>
               </gml:Polygon>
             </geometry>
             <foo>baz</foo>
           </Object>
         </content>
       </TopLevelObject>
     </gml:featureMember>
   </gml:FeatureCollection>

By default, only the TopLevelObject object would be reported and it
would only use the second geometry. This is not the desired behavior in
that instance. You can edit the generated .gfs and modify it like the
following in order to specify a full path to the element (top level XML
element being omitted) :

.. code-block:: XML

   <GMLFeatureClassList>
     <GMLFeatureClass>
       <Name>Object</Name>
       <ElementPath>featureMember|TopLevelObject|content|Object</ElementPath>
       <GeometryType>3</GeometryType>
       <PropertyDefn>
         <Name>foo</Name>
         <ElementPath>foo</ElementPath>
         <Type>String</Type>
       </PropertyDefn>
     </GMLFeatureClass>
   </GMLFeatureClassList>

Getting XML attributes as OGR fields
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The element@attribute syntax can be used in the <ElementPath> to specify
that the value of attribute 'attribute' of element 'element' must be
fetched.

Let's consider the following test.gml file :

.. code-block:: XML

   <?xml version="1.0" encoding="UTF-8"?>
   <gml:FeatureCollection xmlns:gml="http://www.opengis.net/gml">
     <gml:featureMember>
       <LAYER>
         <length unit="m">5</length>
       </LAYER>
     </gml:featureMember>
   </gml:FeatureCollection>

and the following associated .gfs file.

.. code-block:: XML

   <GMLFeatureClassList>
     <GMLFeatureClass>
       <Name>LAYER</Name>
       <ElementPath>LAYER</ElementPath>
       <GeometryType>100</GeometryType> <!-- no geometry -->
       <PropertyDefn>
         <Name>length</Name>
         <ElementPath>length</ElementPath>
         <Type>Real</Type>
       </PropertyDefn>
       <PropertyDefn>
         <Name>length_unit</Name>
         <ElementPath>length@unit</ElementPath>
         <Type>String</Type>
       </PropertyDefn>
     </GMLFeatureClass>
   </GMLFeatureClassList>

The output of *ogrinfo test.gml -ro -al* is:

::

   Layer name: LAYER
   Geometry: None
   Feature Count: 1
   Layer SRS WKT:
   (unknown)
   gml_id: String (0.0)
   length: Real (0.0)
   length_unit: String (0.0)
   OGRFeature(LAYER):0
     gml_id (String) = (null)
     length (Real) = 5
     length_unit (String) = m

Using conditions on XML attributes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A <Condition> element can be specified as a child element of a
<PropertyDefn>. The content of the Condition follows a minimalistic
XPath syntax. It must be of the form @attrname[=|!=]'attrvalue' [and|or
other_cond]*. Note that 'and' and 'or' operators cannot be mixed (their
precedence is not taken into account).

Several <PropertyDefn> can be defined with the same <ElementPath>, but
with <Condition> that must be mutually exclusive.

Let's consider the following testcondition.gml file :

.. code-block:: XML

   <?xml version="1.0" encoding="utf-8" ?>
   <ogr:FeatureCollection
        xmlns:ogr="http://ogr.maptools.org/"
        xmlns:gml="http://www.opengis.net/gml">
     <gml:featureMember>
       <ogr:testcondition fid="testcondition.0">
         <ogr:name lang="en">English name</ogr:name>
         <ogr:name lang="fr">Nom francais</ogr:name>
         <ogr:name lang="de">Deutsche name</ogr:name>
       </ogr:testcondition>
     </gml:featureMember>
   </ogr:FeatureCollection>

and the following associated .gfs file.

.. code-block:: XML

   <GMLFeatureClassList>
     <GMLFeatureClass>
       <Name>testcondition</Name>
       <ElementPath>testcondition</ElementPath>
       <GeometryType>100</GeometryType>
       <PropertyDefn>
         <Name>name_en</Name>
         <ElementPath>name</ElementPath>
         <Condition>@lang='en'</Condition>
         <Type>String</Type>
       </PropertyDefn>
       <PropertyDefn>
         <Name>name_fr</Name>
         <ElementPath>name</ElementPath>
         <Condition>@lang='fr'</Condition>
         <Type>String</Type>
       </PropertyDefn>
       <PropertyDefn>
         <Name>name_others_lang</Name>
         <ElementPath>name@lang</ElementPath>
         <Condition>@lang!='en' and @lang!='fr'</Condition>
         <Type>StringList</Type>
       </PropertyDefn>
       <PropertyDefn>
         <Name>name_others</Name>
         <ElementPath>name</ElementPath>
         <Condition>@lang!='en' and @lang!='fr'</Condition>
         <Type>StringList</Type>
       </PropertyDefn>
     </GMLFeatureClass>
   </GMLFeatureClassList>

The output of *ogrinfo testcondition.gml -ro -al* is:

::

   Layer name: testcondition
   Geometry: None
   Feature Count: 1
   Layer SRS WKT:
   (unknown)
   fid: String (0.0)
   name_en: String (0.0)
   name_fr: String (0.0)
   name_others_lang: StringList (0.0)
   name_others: StringList (0.0)
   OGRFeature(testcondition):0
     fid (String) = testcondition.0
     name_en (String) = English name
     name_fr (String) = Nom francais
     name_others_lang (StringList) = (1:de)
     name_others (StringList) = (1:Deutsche name)

Registry for GML application schemas
------------------------------------

The "data" directory of the GDAL installation contains a
"gml_registry.xml" file that links feature types of GML application
schemas to .xsd or .gfs files that contain their definition. This is
used in case no valid .gfs or .xsd file is found next to the GML file.

An alternate location for the registry file can be defined by setting
its full pathname to the GML_REGISTRY configuration option.

An example of such a file is :

.. code-block:: XML

   <gml_registry>
       <!-- Finnish National Land Survey cadastral data -->
       <namespace prefix="ktjkiiwfs" uri="http://xml.nls.fi/ktjkiiwfs/2010/02" useGlobalSRSName="true">
           <featureType elementName="KiinteistorajanSijaintitiedot"
                    schemaLocation="http://xml.nls.fi/XML/Schema/sovellus/ktjkii/modules/kiinteistotietojen_kyselypalvelu_WFS/Asiakasdokumentaatio/ktjkiiwfs/2010/02/KiinteistorajanSijaintitiedot.xsd"/>
           <featureType elementName="PalstanTunnuspisteenSijaintitiedot"
                    schemaLocation="http://xml.nls.fi/XML/Schema/sovellus/ktjkii/modules/kiinteistotietojen_kyselypalvelu_WFS/Asiakasdokumentaatio/ktjkiiwfs/2010/02/palstanTunnuspisteenSijaintitiedot.xsd"/>
           <featureType elementName="RekisteriyksikonTietoja"
                    schemaLocation="http://xml.nls.fi/XML/Schema/sovellus/ktjkii/modules/kiinteistotietojen_kyselypalvelu_WFS/Asiakasdokumentaatio/ktjkiiwfs/2010/02/RekisteriyksikonTietoja.xsd"/>
           <featureType elementName="PalstanTietoja"
                    schemaLocation="http://xml.nls.fi/XML/Schema/sovellus/ktjkii/modules/kiinteistotietojen_kyselypalvelu_WFS/Asiakasdokumentaatio/ktjkiiwfs/2010/02/PalstanTietoja.xsd"/>
       </namespace>

       <!-- Inspire CadastralParcels schema -->
       <namespace prefix="cp" uri="urn:x-inspire:specification:gmlas:CadastralParcels:3.0" useGlobalSRSName="true">
           <featureType elementName="BasicPropertyUnit"
                        gfsSchemaLocation="inspire_cp_BasicPropertyUnit.gfs"/>
           <featureType elementName="CadastralBoundary"
                        gfsSchemaLocation="inspire_cp_CadastralBoundary.gfs"/>
           <featureType elementName="CadastralParcel"
                        gfsSchemaLocation="inspire_cp_CadastralParcel.gfs"/>
           <featureType elementName="CadastralZoning"
                        gfsSchemaLocation="inspire_cp_CadastralZoning.gfs"/>
       </namespace>

       <!-- Czech RUIAN (VFR) schema (v1) -->
       <namespace prefix="vf"
                  uri="urn:cz:isvs:ruian:schemas:VymennyFormatTypy:v1 ../ruian/xsd/vymenny_format/VymennyFormatTypy.xsd"
                  useGlobalSRSName="true">
           <featureType elementName="TypSouboru"
                        elementValue="OB"
                        gfsSchemaLocation="ruian_vf_ob_v1.gfs"/>
           <featureType elementName="TypSouboru"
                        elementValue="ST"
                        gfsSchemaLocation="ruian_vf_st_v1.gfs"/>
       </namespace>
   </gml_registry>

XML schema definition (.xsd) files are pointed by the schemaLocation
attribute, whereas OGR .gfs files are pointed by the gfsSchemaLocation
attribute. In both cases, the filename can be a URL (http://, https://),
an absolute filename, or a relative filename (relative to the location
of gml_registry.xml).

The schema is used if and only if the namespace prefix and URI are found
in the first bytes of the GML file (e.g.
*xmlns:ktjkiiwfs="http://xml.nls.fi/ktjkiiwfs/2010/02"*), and that the
feature type is also detected in the first bytes of the GML file (e.g.
*ktjkiiwfs:KiinteistorajanSijaintitiedot*). If the element value is
defined than the schema is used only if the feature type together with
the value is found in the first bytes of the GML file (e.g.
*vf:TypSouboru>OB_UKSH*).

Building junction tables
------------------------

The
`ogr_build_junction_table.py <https://github.com/OSGeo/gdal/blob/master/gdal/swig/python/gdal-utils/osgeo_utils/samples/ogr_build_junction_table.py>`__
script can be used to build a `junction
table <http://en.wikipedia.org/wiki/Junction_table>`__ from OGR layers
that contain "XXXX_href" fields. Let's considering the following output
of a GML file with links to other features :

::

   OGRFeature(myFeature):1
     gml_id (String) = myFeature.1
     [...]
     otherFeature_href (StringList) = (2:#otherFeature.10,#otherFeature.20)

   OGRFeature(myFeature):2
     gml_id (String) = myFeature.2
     [...]
     otherFeature_href (StringList) = (2:#otherFeature.30,#otherFeature.10)

After running

::

   ogr2ogr -f PG PG:dbname=mydb my.gml

to import it into PostGIS and

::

   python ogr_build_junction_table.py PG:dbname=mydb

, a *myfeature_otherfeature* table will be created and will contain the
following content :

================ ===================
myfeature_gml_id otherfeature_gml_id
================ ===================
myFeature.1      otherFeature.10
myFeature.1      otherFeature.20
myFeature.2      otherFeature.30
myFeature.2      otherFeature.10
================ ===================

Reading datasets resulting from a WFS 2.0 join queries
------------------------------------------------------

The GML driver can read datasets resulting from a WFS 2.0 join queries.

Such datasets typically look like:

.. code-block:: XML


   <wfs:FeatureCollection xmlns:xs="http://www.w3.org/2001/XMLSchema"
       xmlns:app="http://app.com"
       xmlns:wfs="http://www.opengis.net/wfs/2.0"
       xmlns:gml="http://www.opengis.net/gml/3.2"
       xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
       numberMatched="unknown" numberReturned="2" timeStamp="2015-01-01T00:00:00.000Z"
       xsi:schemaLocation="http://www.opengis.net/gml/3.2 http://schemas.opengis.net/gml/3.2.1/gml.xsd
                           http://www.opengis.net/wfs/2.0 http://schemas.opengis.net/wfs/2.0/wfs.xsd">
     <wfs:member>
       <wfs:Tuple>
         <wfs:member>
           <app:table1 gml:id="table1-1">
             <app:foo>1</app:foo>
           </app:table1>
         </wfs:member>
         <wfs:member>
           <app:table2 gml:id="table2-1">
             <app:bar>2</app:bar>
             <app:baz>foo</app:baz>
             <app:geometry><gml:Point gml:id="table2-2.geom.0"><gml:pos>2 49</gml:pos></gml:Point></app:geometry>
           </app:table2>
         </wfs:member>
       </wfs:Tuple>
     </wfs:member>
     <wfs:member>
       <wfs:Tuple>
         <wfs:member>
           <app:table1 gml:id="table1-2">
             <app:bar>2</app:bar>
             <app:geometry><gml:Point gml:id="table1-1.geom.0"><gml:pos>3 50</gml:pos></gml:Point></app:geometry>
           </app:table1>
         </wfs:member>
         <wfs:member>
           <app:table2 gml:id="table2-2">
             <app:bar>2</app:bar>
             <app:baz>bar</app:baz>
             <app:geometry><gml:Point gml:id="table2-2.geom.0"><gml:pos>2 50</gml:pos></gml:Point></app:geometry>
           </app:table2>
         </wfs:member>
       </wfs:Tuple>
     </wfs:member>
   </wfs:FeatureCollection>

OGR will group together the attributes from the layers participating to
the join and will prefix them with the layer name. So the above example
will be read as the following:

::

   OGRFeature(join_table1_table2):0
     table1.gml_id (String) = table1-1
     table1.foo (Integer) = 1
     table1.bar (Integer) = (null)
     table2.gml_id (String) = table2-1
     table2.bar (Integer) = 2
     table2.baz (String) = foo
     table2.geometry = POINT (2 49)

   OGRFeature(join_table1_table2):1
     table1.gml_id (String) = table1-2
     table1.foo (Integer) = (null)
     table1.bar (Integer) = 2
     table2.gml_id (String) = table2-2
     table2.bar (Integer) = 2
     table2.baz (String) = bar
     table1.geometry = POINT (3 50)
     table2.geometry = POINT (2 50)

Examples
--------

The ogr2ogr utility can be used to dump the results of a Oracle query to
GML:

::

   ogr2ogr -f GML output.gml OCI:usr/pwd@db my_feature -where "id = 0"

The ogr2ogr utility can be used to dump the results of a PostGIS query
to GML:

::

   ogr2ogr -f GML output.gml PG:'host=myserver dbname=warmerda' -sql "SELECT pop_1994 from canada where province_name = 'Alberta'"

See Also
--------

-  `GML Specifications <http://www.opengeospatial.org/standards/gml>`__
-  `GML 3.1.1 simple features profile - OGC(R)
   06-049r1 <http://portal.opengeospatial.org/files/?artifact_id=15201>`__
-  `Geography Markup Language (GML) simple features profile (with
   Corrigendum) (GML 3.2.1) - OGC(R)
   10-100r3 <https://portal.opengeospatial.org/files/?artifact_id=42729>`__
-  `Xerces <http://xml.apache.org/xerces2-j/index.html>`__
-  :ref:`GMLAS - Geography Markup Language (GML) driven by application
   schemas <vector.gmlas>`
-  :ref:`NAS/ALKIS : specialized GML driver for cadastral data in
   Germany <vector.nas>`

Credits
-------

-  Implementation for **GML_SKIP_RESOLVE_ELEMS HUGE** was contributed by
   A.Furieri, with funding from Regione Toscana
-  Support for cadastral data in Finnish National Land Survey GML and
   Inspire GML was funded by The Information Centre of the Ministry of
   Agriculture and Forestry (Tike)
