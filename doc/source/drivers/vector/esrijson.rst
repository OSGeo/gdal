.. _vector.esrijson:

ESRIJSON / FeatureService driver
================================

.. shortname:: ESRIJSON

.. built_in_by_default::

(Note: prior to GDAL 2.3, the functionality of this driver was available
in the GeoJSON driver. They are now distinct drivers)

This driver can read the JSON output of Feature Service requests
following the `GeoServices REST
Specification <http://www.esri.com/industries/landing-pages/geoservices/geoservices.html>`__,
like implemented by `ArcGIS Server REST
API <http://help.arcgis.com/en/arcgisserver/10.0/apis/rest/index.html>`__.
The driver can scroll through such result sets
that are spread over multiple pages (for ArcGIS servers >= 10.3). This
is automatically enabled if URL does not contain an explicit
*resultOffset* parameter. If it contains this parameter and scrolling is
still desired, the FEATURE_SERVER_PAGING open option must be set to YES.
The page size can be explicitly set with the *resultRecordCount*
parameter (but is subject to a server limit). If it is not set, OGR will
set it to the maximum value allowed by the server.

Note: for paged requests to work properly, it is generally necessary to
add a sort clause on a field, typically the OBJECTID with a
"&orderByFields=OBJECTID+ASC" parameter in the URL, so that the server
returns the results in a reliable way.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Datasource
----------

The driver accepts three types of sources of data:

-  Uniform Resource Locator (`URL <http://en.wikipedia.org/wiki/URL>`__)
   - a Web address to perform
   `HTTP <http://en.wikipedia.org/wiki/HTTP>`__ request.
-  Plain text file with ESRIJSON data - identified from the file
   extension .json
-  Text passed directly and encoded in ESRI JSON

Starting with GDAL 2.3, the URL/filename/text might be prefixed with
ESRIJSON: to avoid any ambiguity with other drivers.

Open options
------------

-  **FEATURE_SERVER_PAGING** = YES/NO: Whether to automatically scroll
   through results with a ArcGIS Feature Service endpoint. Has only effect
   for ArcGIS servers >= 10.3 and layers with supportsPagination=true capability.

Example
-------

Read the result of a FeatureService request against a GeoServices REST
server (note that this server does not support paging):

::

   ogrinfo -ro -al "http://sampleserver3.arcgisonline.com/ArcGIS/rest/services/Hydrography/Watershed173811/FeatureServer/0/query?where=objectid+%3D+objectid&outfields=*&f=json"

See Also
--------

-  :ref:`GeoJSON driver <vector.geojson>`
-  `GeoServices REST
   Specification <http://www.esri.com/industries/landing-pages/geoservices/geoservices.html>`__
