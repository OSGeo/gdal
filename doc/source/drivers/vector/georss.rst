.. _vector.georss:

GeoRSS : Geographically Encoded Objects for RSS feeds
=====================================================

.. shortname:: GeoRSS

.. build_dependencies:: (read support needs libexpat)

GeoRSS is a way of encoding location in RSS or Atom feeds.

OGR has support for GeoRSS reading and writing. Read support is only
available if GDAL is built with *expat* library support

The driver supports RSS documents in RSS 2.0 or Atom 1.0 format.

It also supports the `3 ways of encoding
location <http://georss.org/model>`__ : GeoRSS simple, GeoRSS GML and
W3C Geo (the later being deprecated).

The driver can read and write documents without location information as
well.

The default datum for GeoRSS document is the WGS84 datum (EPSG:4326).
Although that GeoRSS locations are encoded in latitude-longitude order
in the XML file, all coordinates reported or expected by the driver are
in longitude-latitude order. The longitude/latitude order used by OGR is
meant for compatibility with most of the rest of OGR drivers and
utilities. For locations encoded in GML, the driver will support the
srsName attribute for describing other SRS.

Simple and GML encoding support the notion of a *box* as a geometry.
This will be decoded as a rectangle (Polygon geometry) in OGR Simple
Feature model.

A single layer is returned while reading a RSS document. Features are
retrieved from the content of <item> (RSS document) or <entry> (Atom
document) elements.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

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

| If your GeoRSS file is not encoded in one of the previous encodings,
  it will not be parsed by the GeoRSS driver. You may convert it into
  one of the supported encoding with the *iconv* utility for example and
  change accordingly the *encoding* parameter value in the XML header.

When writing a GeoRSS file, the driver expects UTF-8 content to be
passed in.

Field definitions
-----------------

While reading a GeoRSS document, the driver will first make a full scan
of the document to get the field definitions.

The driver will return elements found in the base schema of RSS channel
or Atom feeds. It will also return extension elements, that are allowed
in namespaces.

Attributes of first level elements will be exposed as fields.

Complex content (elements inside first level elements) will be returned
as an XML blob.

When a same element is repeated, a number will be appended at the end of
the attribute name for the repetitions. This is useful for the
<category> element in RSS and Atom documents for example.

The following content :

::

       <item>
           <title>My tile</title>
           <link>http://www.mylink.org</link>
           <description>Cool description !</description>
           <pubDate>Wed, 11 Jul 2007 15:39:21 GMT</pubDate>
           <guid>http://www.mylink.org/2007/07/11</guid>
           <category>Computer Science</category>
           <category>Open Source Software</category>
           <georss:point>49 2</georss:point>
           <myns:name type="my_type">My Name</myns:name>
           <myns:complexcontent>
               <myns:subelement>Subelement</myns:subelement>
           </myns:complexcontent>
       </item>

will be interpreted in the OGR SF model as :

::

     title (String) = My title
     link (String) = http://www.mylink.org
     description (String) = Cool description !
     pubDate (DateTime) = 2007/07/11 15:39:21+00
     guid (String) = http://www.mylink.org/2007/07/11
     category (String) = Computer Science
     category2 (String) = Open Source Software
     myns_name (String) = My Name
     myns_name_type (String) = my_type
     myns_complexcontent (String) = <myns:subelement>Subelement</myns:subelement>
     POINT (2 49)

Creation Issues
---------------

On export, all layers are written to a single file. Update of existing
files is not supported.

If the output file already exits, the writing will not occur. You have
to delete the existing file first.

A layer that is created cannot be immediately read without closing and
reopening the file. That is to say that a dataset is read-only or
write-only in the same session.

Supported geometries :

-  Features of type wkbPoint/wkbPoint25D.
-  Features of type wkbLineString/wkbLineString25D.
-  Features of type wkbPolygon/wkbPolygon25D.

Other type of geometries are not supported and will be silently ignored.

Dataset creation options
------------------------

-  **FORMAT**\ =RSS|ATOM: whether the document must be in RSS 2.0 or
   Atom 1.0 format. Default value : RSS
-  **GEOM_DIALECT**\ =SIMPLE|GML|W3C_GEO (RSS or ATOM document): the
   encoding of location information. Default value : SIMPLE
   W3C_GEO only supports point geometries.
   SIMPLE or W3C_GEO only support geometries in geographic WGS84
   coordinates.
-  **USE_EXTENSIONS**\ =YES|NO. Default value : NO. If defined to YES,
   extension fields (that is to say fields not in the base schema of RSS
   or Atom documents) will be written. If the field name not found in
   the base schema matches the foo_bar pattern, foo will be considered
   as the namespace of the element, and a <foo:bar> element will be
   written. Otherwise, elements will be written in the <ogr:> namespace.
-  **WRITE_HEADER_AND_FOOTER**\ =YES|NO. Default value : YES. If defined
   to NO, only <entry> or <item> elements will be written. The user will
   have to provide the appropriate header and footer of the document.
   Following options are not relevant in that case.
-  **HEADER** (RSS or Atom document): XML content that will be put
   between the <channel> element and the first <item> element for a RSS
   document, or between the xml tag and the first <entry> element for an
   Atom document. If it is specified, it will overload the following
   options.
-  **TITLE** (RSS or Atom document): value put inside the <title>
   element in the header. If not provided, a dummy value will be used as
   that element is compulsory.
-  **DESCRIPTION** (RSS document): value put inside the <description>
   element in the header. If not provided, a dummy value will be used as
   that element is compulsory.
-  **LINK** (RSS document): value put inside the <link> element in the
   header. If not provided, a dummy value will be used as that element
   is compulsory.
-  **UPDATED** (Atom document): value put inside the <updated> element
   in the header. Should be formatted as a XML datetime. If not
   provided, a dummy value will be used as that element is compulsory.
-  **AUTHOR_NAME** (Atom document): value put inside the <author><name>
   element in the header. If not provided, a dummy value will be used as
   that element is compulsory.
-  **ID** (Atom document): value put inside the <id> element in the
   header. If not provided, a dummy value will be used as that element
   is compulsory.

When translating from a source dataset, it may be necessary to rename
the field names from the source dataset to the expected RSS or ATOM
attribute names, such as <title>, <description>, etc... This can be done
with a :ref:`OGR VRT <vector.vrt>` dataset, or by using the "-sql" option
of the ogr2ogr utility (see :ref:`rfc-21`)

VSI Virtual File System API support
-----------------------------------

The driver supports reading and writing to files managed by VSI Virtual
File System API, which include "regular" files, as well as files in the
/vsizip/ (read-write) , /vsigzip/ (read-write) , /vsicurl/ (read-only)
domains.

Writing to /dev/stdout or /vsistdout/ is also supported.

Example
-------

The ogrinfo utility can be used to dump the content of a GeoRSS datafile
:

::

   ogrinfo -ro -al input.xml

| 

The ogr2ogr utility can be used to do GeoRSS to GeoRSS translation. For
example, to translate a Atom document into a RSS document

::

   ogr2ogr -f GeoRSS output.xml input.xml "select link_href as link, title, content as description, author_name as author, id as guid from georss"

| 
| Note : in this example we map equivalent fields, from the source name
  to the expected name of the destination format.

| 

The following Python script shows how to read the content of a online
GeoRSS feed

::

       #!/usr/bin/python
       import gdal
       import ogr
       import urllib2

       url = 'http://earthquake.usgs.gov/eqcenter/catalogs/eqs7day-M5.xml'
       content = None
       try:
           handle = urllib2.urlopen(url)
           content = handle.read()
       except urllib2.HTTPError, e:
           print 'HTTP service for %s is down (HTTP Error: %d)' % (url, e.code)
       except:
           print 'HTTP service for %s is down.' %(url)

       # Create in-memory file from the downloaded content
       gdal.FileFromMemBuffer('/vsimem/temp', content)

       ds = ogr.Open('/vsimem/temp')
       lyr = ds.GetLayer(0)
       feat = lyr.GetNextFeature()
       while feat is not None:
           print feat.GetFieldAsString('title') + ' ' + feat.GetGeometryRef().ExportToWkt()
           feat.Destroy()
           feat = lyr.GetNextFeature()

       ds.Destroy()

       # Free memory associated with the in-memory file
       gdal.Unlink('/vsimem/temp')

See Also
--------

-  `Home page for GeoRSS format <http://georss.org/>`__
-  `Wikipedia page for GeoRSS
   format <http://en.wikipedia.org/wiki/GeoRSS>`__
-  `Wikipedia page for RSS format <http://en.wikipedia.org/wiki/RSS>`__
-  `RSS 2.0 specification <http://www.rssboard.org/rss-specification>`__
-  `Wikipedia page for Atom
   format <http://en.wikipedia.org/wiki/Atom_(standard)>`__
-  `Atom 1.0 specification <http://www.ietf.org/rfc/rfc4287.txt>`__
