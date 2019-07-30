.. _rfc-29:

================================================================================
RFC 29: OGR Set Ignored Fields
================================================================================

Author: Martin Dobias

Contact: wonder.sk@gmail.com

Status: Adopted

Summary
-------

To improve performance when fetching features, this RFC proposes a way
how to tell OGR which fields are not going to be required in subsequent
GetFeature() / GetNextFeature() calls. Such fields will be ignored by
the driver and their value will be kept null. The RFC counts also with
the possibility to ignore feature geometry and style.

Common use cases:

1. the client renders the layer: all (or most) fields can be ignored,
   only the geometry is required

2. the client shows attribute table: all fields are required, the
   geometry can be ignored

Details
-------

A new function will be added to OGRLayer class to allow the client to
set which fields will *not* be fetched:

::

   virtual OGRErr OGRLayer::SetIgnoredFields( const char **papszFields );

and an equivalent call for C API:

::

   OGRErr CPL_DLL OGR_L_SetIgnoredFields( OGRLayerH, const char **papszFields );

The argument is a list of fields to be ignored, by name, and the special
field names "OGR_GEOMETRY" and "OGR_STYLE" will be interpreted to refer
to the geometry and style values of a feature.

Passing by field name has been chosen so that we could handle
OGR_GEOMETRY, OGR_STYLE and possibly some other special fields in the
future. Instead of specifying "desired" fields, it has been decided to
specify "ignored" fields so that we wouldn't accidentally drop things
like geometry and style just because they weren't explicitly listed in a
desired list.

Passing NULL for papszFields will clear the ignored list.

The method will return OGRERR_NONE as long as all the field names are
able to be resolved, even if the method does not support selection of
fields.

The drivers supporting this method will return TRUE to OLCIgnoreFields
("IgnoreFields") capability.

The method will be implemented at the level of OGRLayer class: it will
resolve indexes of the fields and set the following new member variables
which indicate what should be ignored. The flags will be stored within
OGRFeatureDefn and OGRFieldDefn classes and available with these getter
functions:

::

   bool OGRFieldDefn::IsIgnored();
   bool OGRFeatureDefn::IsGeometryIgnored();
   bool OGRFeatureDefn::IsStyleIgnored();

The getter member functions will be complemented by setter functions for
use by OGRLayer. Setting the "ignored" flags directly by clients will be
forbidden.

Optionally the method ``SetIgnoredFields()`` can be overridden in driver
implementation if the driver has some special needs.

Implementation in drivers
-------------------------

The implementation of drivers will require small adjustments in order to
support this RFC. Drivers not making use of this addition will simply
continue to fetch also fields/geometry/style that are not requested by
the caller.

The adjustments in driver implementation will look as follows:

::

   if (!poDefn->IsGeometryIgnored())
   {
     // fetch geometry
   }
   if (!poDefn->IsStyleIgnored())
   {
     // fetch style
   }

   for( int iField = 0; iField < poDefn->GetFieldCount(); iField++ )
   {
     if (poDefn->GetFieldDefn(iField)->IsIgnored())
       continue;

     // fetch field
   }

Compatibility
-------------

This change is fully backwards compatible: OGR will continue to fetch
geometry, style and all fields by default. Only applications using the
proposed API will experience the new behavior.

Initially, only some drivers (Shapefile and few others) will implement
this RFC. There is no need to modify all existing drivers when adopting
the RFC - drivers that do not consider the ignored fields will simply
fetch all attributes as before. To check whether a driver supports this
RFC, OLCIgnoreFields capability can be checked.

ogr2ogr command line tool will make use of this RFC in cases it receives
-select argument with a list of required fields. Other than the
specified fields will be ignored.

Voting History
--------------

-  Frank Warmerdam +1
-  Tamas Szekeres +1
-  Daniel Morissette +0
-  Howard Butler +0
-  Even Rouault +0
