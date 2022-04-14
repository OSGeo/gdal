.. _vector.csw:

CSW - OGC CSW (Catalog Service for the Web)
===========================================

.. shortname:: CSW

.. build_dependencies:: libcurl

This driver can connect to a OGC CSW service. It supports CSW 2.0.2
protocol. GDAL/OGR must be built with Curl support in order to the CSW
driver to be compiled. And the GML driver should be set-up for read
support (thus requiring GDAL/OGR to be built with Xerces or Expat
support).

It retrieves records with Dublin Core metadata.

Driver capabilities
-------------------

.. supports_georeferencing::

Dataset name syntax
-------------------

The minimal syntax to open a CSW datasource is : *CSW:* and the URL open
option, or *CSW:http://path/to/CSW/endpoint*

Filtering
---------

The driver will forward any spatial filter set with SetSpatialFilter()
to the server. It also makes its best effort to do the same for
attribute filters set with SetAttributeFilter() when possible (turning
OGR SQL language into OGC filter description).

The *anytext* field can be queried to do a search in any text field.
Note that we always return it as null content however in OGR side, to
avoid duplicating information.

Issues
------

Some servers do not respect EPSG axis order, in particular latitude,
longitude order for WGS 84 geodetic coordinates, so it might be needed
to specify the :decl_configoption:`GML_INVERT_AXIS_ORDER_IF_LAT_LONG=NO` 
configuration option in those cases.

Open options
------------

-  **URL**: URL to the CSW server endpoint (if not specified in the
   connection string already)
-  **ELEMENTSETNAME**\ =brief/summary/full: Level of details of
   properties. Defaults to *full*.
-  **FULL_EXTENT_RECORDS_AS_NON_SPATIAL**\ =YES/NO: Whether records with
   (-180,-90,180,90) extent should be considered non-spatial. Defaults
   to NO.
-  **OUTPUT_SCHEMA**\ =URL : Value of outputSchema parameter, in the
   restricted set supported by the serve. Special value *gmd* can be
   used as a shortcut for http://www.isotc211.org/2005/gmd, *csw* for
   http://www.opengis.net/cat/csw/2.0.2. When this open option is set, a
   *raw_xml* field will be filled with the XML content of each record.
   Other metadata fields will remain empty.
-  **MAX_RECORDS**\ =value : Maximum number of records to retrieve in a
   single time. Defaults to 500. Servers might have a lower accepted
   value.

Configuration options
---------------------

The following :ref:`configuration options <configoptions>` are 
available:

-  :decl_configoption:`GML_INVERT_AXIS_ORDER_IF_LAT_LONG=NO`: Some servers 
   do not respect EPSG axis order, in particular latitude,
   longitude order for WGS 84 geodetic coordinates, so it might be needed
   to specify the  configuration option in those cases.

Examples
--------

Listing all the records of a CSW server:

::

   ogrinfo -ro -al -noextent CSW:http://catalog.data.gov/csw

Listing all the records of a CSW server with spatial and an attribute
filter on a give field:

::

   ogrinfo -ro -al -noextent CSW:http://catalog.data.gov/csw -spat 2 49 2 49 -where "subject LIKE '%mineralogy%'"

Listing all the records of a CSW server that matches a text on any text
field:

::

   ogrinfo -ro -al -q CSW:http://catalog.data.gov/csw -spat 2 49 2 49 -where "anytext LIKE '%France%'"

Listing all the records of a CSW server as ISO 19115/19119:

::

   ogrinfo -ro -al -q CSW:http://catalog.data.gov/csw -oo OUTPUT_SCHEMA=gmd

See Also
--------

-  `OGC CSW Standard <http://www.opengeospatial.org/standards/cat>`__
-  :ref:`GML driver documentation <vector.gml>`
