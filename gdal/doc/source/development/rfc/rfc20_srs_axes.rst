.. _rfc-20:

================================================================================
RFC 20: OGRSpatialReference Axis Support
================================================================================

Author: Frank Warmerdam

Contact: warmerdam@pobox.com

Status: Adopted

Summary
-------

The OGRSpatialReference and OGRCoordinateTransformation classes assume
that all coordinate systems use (easting, northing) coordinate order (or
in geographic terms (longitude, latitude)). In practice some coordinate
systems use alternate axis orientations (such as the Krovak projection),
and some standards (GML, WMS 1.3, WCS 1.1) require honouring the EPSG
declaration that all it's geographic coordinates have (latitude,
longitude) coordinate ordering.

This RFC attempts to extend the OGRSpatialReference, and
OGRCoordinateTransformation classes to support alternate axis
orientations, and to update selected drivers (GML, WMS, WCS, GMLJP2) to
properly support axis ordering.

WKT Axis Representation
-----------------------

The OGC WKT SRS format (per OGC 01-???) already indicates a way of
defining coordinate system axes as shown in this example:

::

   GEOGCS["WGS 84",
       DATUM["WGS_1984",
           SPHEROID["WGS 84",6378137,298.257223563,
               AUTHORITY["EPSG","7030"]],
           TOWGS84[0,0,0,0,0,0,0],
           AUTHORITY["EPSG","6326"]],
       PRIMEM["Greenwich",0,
           AUTHORITY["EPSG","8901"]],
       UNIT["degree",0.0174532925199433,
           AUTHORITY["EPSG","9108"]],
       AXIS["Lat",NORTH],
       AXIS["Long",EAST],
       AUTHORITY["EPSG","4326"]]

There is one AXIS definition per axis with order relating to position
within a tuple. The first argument is the user name for the axis and
exact values are not specified. The second argument is a direction and
may be one of NORTH, SOUTH, EAST or WEST.

Dilemma
-------

The core challenge of this RFC is adding support for axes orders,
including honouring EPSG desired axis order for geographic coordinate
systems where appropriate without breaking existing files and code that
make extensive use of EPSG coordinate systems but override axis
orientations and assume they should be treated as long, lat regardless
of what EPSG says.

In particular, we come up with appropriate policies and mechanisms to
decide when a file in a geographic coordinate system like EPSG:4326 is
to be treated as lat/long and when it should be long/lat. Because of the
extensive existing practice it behooves us to err on the side of past
practice, and require "opting in" to honouring EPSG axis ordering.

The Hack
--------

The main mechanism by I propose to work around the dilemma is to
differentiate between geographic coordinate systems with the AXIS values
set and those without. In particular, a WKT coordinate system with the
EPSG authority code (ie. 4326) set, but no axis declarations will be
assumed to be long, lat even though that is contrary to the definition
from EPSG of 4326. Only in cases where we really *know* we want to
honour EPSG's axis order will we actually populate the axis declarations
indicating lat, long.

The hope is that this will let us continue to (mis)use EPSG:4326
definitions without necessary honouring the EPSG axis ordering except in
specific circumstances.

OGRSpatialReference
-------------------

New Enumeration
~~~~~~~~~~~~~~~

::


   typedef enum { 
     OAO_Unknown = 0,
     OAO_North = 1,
     OAO_South = 2,
     OAO_East = 3,
     OAO_West = 4
   } OGRAxisOrientation;

New methods
~~~~~~~~~~~

::

       const char *GetAxis( const char *pszTargetKey, int iAxis, 
                            OGRAxisOrientation *peOrientation );

Fetch information about one axis (iAxis is zero based).

::

       OGRErr      SetAxes( const char *pszTargetKey, 
                            const char *pszXAxisName, OGRAxisOrientation eXAxisOrientation,
                            const char *pszYAxisName, OGRAxisOrientation eYAxisOrientation,
                            const char *pszZAxisName=NULL, OGRAxisOrientation eZAxisOrientation = OAO_Unknown );

Defines the X and Y axes for a given target key (PROJCS or GEOGCS).

::

       int         EPSGTreatsAsLatLong();

Returns true based on the EPSG code if EPSG would like this coordinate
system to be treated as lat/long. This is useful in contexts like WMS
1.3 where EPSG:4326 needs to be interpreted as lat/long due to the
standard.

::

       OGRErr      importFromEPSGA( int );

This works like importFromEPSG() but will assign the EPSG defined AXIS
definition.

Note that OGRSpatialReference::StripNodes( "AXIS" ); can be used to
strip axis definitions where they are not desired.

importFromURN
~~~~~~~~~~~~~

Modify importFromURN() to set AXIS values properly for EPSG and OGC
geographic coordinate systems. So urn:...:EPSG: will be assumed to
really honour EPSG conventions.

SetWellKnownGeogCS()
~~~~~~~~~~~~~~~~~~~~

This method appears to be the only code

-  Modify SetWellKnownGeogCS() to *not* set AXIS values, and strip AXIS
   values out of any other hardcoded WKT definitions.

importFromEPSG()
~~~~~~~~~~~~~~~~

-  importFromEPSG() will continue to *not* set AXIS values for GEOGCS
   coordinate systems.
-  importFromEPSG() will now set axis values for projected coordinate
   systems (at least in cases like Krovak where it is a non-default axis
   orientation).
-  importFromEPSG() will be implemented by calling importFromEPSGA() and
   stripping off axis definitions from the geographic portion of the
   returned definition.

SetFromUserInput()
~~~~~~~~~~~~~~~~~~

-  This method will have one new option which is a value prefixed by
   EPSGA: will be passed to importFromEPSGA() (similarly to EPSG:n being
   passed to importFromEPSG()).

OGRCoordinateTransformation
---------------------------

If AXIS values are set on source and/or destination coordinate system,
the OGRCoordinateTransformation code will take care of converting into
normal easting/northing before calling PROJ.

The CPL config option "GDAL_IGNORE_AXIS_ORIENTATION" may also be set to
"TRUE" to disable OGRCoordinateTransformation's checking, and
application of axis orientation changes. Effectively this is a backdoor
to disable the core effects of the RFC.

Drivers Affected
----------------

-  GMLJP2 (classes in gcore/gdalgmlcoverage.cpp and
   gcore/gdaljp2metadata.cpp).
-  WCS (based on interpretation of urns).
-  WMS (maybe? actually, I suspect we don't actually get the coordinate
   system from the capabalities)
-  OGR GML (maybe? only GML3 affected?)
-  BSB, SAR_CEOS, ENVISAT, HDF4, JDEM, L1B, LAN, SRTMHGT: Like
   SetWellKnownGeogCS() these all include lat/long AXIS specifications
   in their hardcoded WGS84 coordinate systems. These need to be removed
   so they will default to being interpreted as long/lat.

Versions
--------

Work will be in trunk for GDAL/OGR 1.6.0 with the following exceptions
which will be address in 1.5.x:

-  Existing use of AXIS specifier will for geographic coordinate systems
   will be stripped from SetWellKnownGeogCS() and the various drivers
   with hard coded WKT strings.
-  Some sort of hack will need to be introduced into the GMLJP2 (and
   possibly WCS) code to flip EPSG authority lat/long values (details to
   be worked out).

Implementation
--------------

Implementation would be done by Frank Warmerdam. Some aspects (such as
properly capturing axis ordering for projected coordinate systems) might
not be implemented immediately.

Compatibility Issues
--------------------

The greatest concern is that any existing WKT coordinate systems with
LAT/LONG axis ordering (in VRT files, or .aux.xml files for instance)
will be interpreted differently by GDAL/OGR 1.6.0 than they were by
1.5.0. This could easily occur if files in formats like BSB, or HDF4
were copied to a format using WKT coordinate systems (such as JPEG with
a .aux.xml file). To partially mitigate this I am proposing that AXIS
definitions be removed from GDAL 1.5.1.

Supporting Information
----------------------

-  OSGeo Wiki Summary:
   `http://wiki.osgeo.org/index.php/Axis_Order_Confusion <http://wiki.osgeo.org/index.php/Axis_Order_Confusion>`__
