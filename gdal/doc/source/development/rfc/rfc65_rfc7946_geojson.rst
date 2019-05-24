.. _rfc-65:

=======================================================================================
RFC 65: RFC 7946 GeoJSON
=======================================================================================

Authors: Sean Gillies

Contact: sean at mapbox.com

Status: Adopted, implemented

Implementation version: 2.2

Summary
-------

GeoJSON has been standardized by the IETF: `RFC
7946 <https://tools.ietf.org/html/rfc7946>`__. Updates to the OGR
GeoJSON driver are needed so that it may write RFC 7946 GeoJSON.

Rationale
---------

The RFC 7946 standard is backwards compatible with the legacy definition
of GeoJSON, but has a few differences (see
`https://tools.ietf.org/html/rfc7946#appendix-B <https://tools.ietf.org/html/rfc7946#appendix-B>`__).
For OGR, the most significant are: removal of "crs" (CRS84 only),
counter-clockwise winding of polygons, geometry splitting at the
antimeridian, and representation of bounding boxes at the antimeridian
and poles. Note: RFC 7946 explicitly restricts to 2D and 3D coordinates,
and forbid use of the M dimension for example. This was already the case
in the existing driver for the GeoJSON 2008 output.

Consensus on the gdal-dev list is that developers should be able to
require RFC 7946 GeoJSON by configuring layer creation with an option
and that it be an all-or-nothing switch.

Changes
-------

A layer creation option will be added for the GeoJSON driver, e.g.,
``RFC7946=TRUE``. When "on", OGR will write GeoJSON with CRS84
coordinates (reprojecting as needed) with 7 places of precision by
default, polygons wound properly, and geometries split at the
antimeridian.

Related to that work, the OGRGeometryFactory::transformWithOptions()
method has been improved to better deal with reprojection of geometries
from polar projections, and projections that span the antimeridian, to
EPSG:4326

Updated drivers
~~~~~~~~~~~~~~~

GeoJSON

SWIG bindings (Python / Java / C# / Perl) changes
-------------------------------------------------

None

Utilities
---------

Utilities will implement RFC 7946 by using the layer creation option.

Documentation
-------------

Documentation of the new layer creation option will reference RFC 7946.

Test Suite
----------

The ogr_geojson.py file tests the effect of the new option.

Compatibility Issues
--------------------

As this is a opt-in parameter, no backward compatibility issue. GeoJSON
files conformant to RFC 7646 can be read by previous GDAL/OGR versions.

Related ticket
--------------

#6705

Implementation
--------------

Implementation has been done by Even Rouault and sponsored by Mapbox.

Voting history
--------------

+1 from JukkaR, HowardB, DanielM and EvenR
