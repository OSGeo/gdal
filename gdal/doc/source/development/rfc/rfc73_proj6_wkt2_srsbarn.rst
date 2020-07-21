.. _rfc-73:

=======================================================================================================
RFC 73: Integration of PROJ6 for WKT2, late binding capabilities, time-support and unified CRS database
=======================================================================================================

============== ============================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2019-Jan-08
Last modified: 2019-May-02
Status:        Implemented in GDAL 3.0
============== ============================

Summary
-------

The document describe work related to integration of PROJ 6 with GDAL,
which adds different capabilities: support for CRS WKT 2 version, "late
binding" capabilities for coordinate transformations between CRS,
support of time-dimension for coordinate operations and the use of a
unified CRS database.

Motivation
----------

The motivations are those exposed in
`https://gdalbarn.com/#why <https://gdalbarn.com/#why>`__ , which are
copied here

Coordinate systems in GDAL, PROJ, and libgeotiff are missing modern
capabilities and need a thorough refactoring:

-  The dreaded ad hoc CSV databases in PROJ_LIB and GDAL_DATA are
   frustrating for users, pose challenges for developers, and impede
   interoperability of definitions.
-  GDAL and PROJ are missing OGC WKT2 support.
-  PROJ 5.0+ no longer requires datum transformation pivots through
   WGS84, which can introduce errors of up to 2m, but the rest of the
   tools do not take advantage of it.

CSV database
~~~~~~~~~~~~

The use of a SQLite-based database for EPSG and other definitions will
allow the projects to add more capability (area-aware validation),
transition the custom peculiar data structures of the projects to
something more universally consumable, and promote definition
interoperability between many coordinate system handling software tools.

WKT2
~~~~

`OGC WKT2 <http://docs.opengeospatial.org/is/12-063r5/12-063r5.html>`__
fixes longstanding interoperability coordinate system definition
discrepancies. WKT2 contains tools for describing time-dependent
coordinate reference systems. PROJ 5+ is now capable of time-dependent
transformations, but GDAL and other tools do not yet support them.

Several countries are updating their geodetic infrastructure to include
time-dependent coordinate systems. For example, Australia and the United
States are adapting time-dependent coordinate systems in 2020 and 2022,
respectively. The familiar NAD83 and NAVD88 in North America being
replaced by NATRF2022 and NAPGD2022, and the industry WILL have to adapt
to these challenges sooner or later.

WGS84 Pivot
~~~~~~~~~~~

PROJ previously required datum transformation that pivoted through WGS84
via a 7-parameter transform. This pivot is a practical solution, but it
can introduce error of about two meters, and many legacy datums cannot
be defined in terms of WGS84. PROJ 5+ now provides the tools to support
late-binding through its `transformation pipeline
framework <https://proj4.org/usage/transformation.html#geodetic-transformation>`__,
but GDAL and the rest of the tools cannot use it yet. Higher accuracy
transformations avoid stepping through WGS84 and eliminates extra
transformation steps with side-car data from a local geodetic authority.

Related work in other libraries
-------------------------------

This RFC is the last step in the "gdalbarn" work. Previous steps have
consisted in implementing the related changes in PROJ master per `PROJ
RFC 2 <https://proj4.org/community/rfc/rfc-2.html>`__ and in libgeotiff
master per `libgeotiff pull request
2 <https://github.com/OSGeo/libgeotiff/pull/2>`__.

Proposal
--------

Third-party library requirements
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

GDAL master (future 3.0) will require PROJ master (future PROJ 6.0) and
libgeotiff master (future libgeotiff 1.5 or 2.0) for build and
execution.

Regarding PROJ, no internal copy of PROJ will be embedded in GDAL
master. It is not doable of supporting older versions of PROJ, as the
OGRSpatialReference class has been largely rewritten to take advantage
of functionality that has been completely moved from GDAL to PROJ: PROJ
string import and export, WKT string import and export, EPSG database
exploitation. To be able to use more easily GDAL master and PROJ master
in complex setups where some GDAL dependencies use a libproj provided by
the system, and where mixing naively PROJ master and this older libproj
would result in runtime crashes, PROJ master can be built with
CFLAGS/CXXFLAGS=-DPROJ_RENAME_SYMBOLS to alias its public symbols, and
GDAL will be able to use this custom build. Note that this is not
intended to be used in a long term, since proper packaging solutions
will eventually use PROJ 6 to rebuild all its reverse dependencies. It
should be noted also that PROJ is required at configure / nmake time,
that is the dynamic loading at runtime through dlopen() / LoadLibrary()
is no longer available.

Regarding libgeotiff, the internal copy in frmts/gtiff/libgeotiff has
been refreshed with the content of upstream libgeotiff master.

All continuous integration systems (Travis-CI and AppVeyor) have been
updated to build PROJ master as part of the GDAL build.

OGRSpatialReference rewrite
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The OGRSpatialReference class is central in GDAL/OGR for all coordinate
reference systems (CRS) manipulations. Up to GDAL 2.4, this class
contained mostly a OGR_SRSNode root node of a WKT 1 representation, and
all getters and setters manipulated this tree representation. As part of
this work, the main object contained internally by OGRSpatialReference
is now a PROJ PJ object, and methods call PROJ C API getters and setters
on this PJ object. This enables to be, mostly (*), representation
independent.

WKT1, WKT2, ESRI WKT, PROJ strings import and export is now delegated to
PROJ. The same holds for import of CRS from the EPSG database, that now
relies on proj.db SQLite database. Consequently all the data/\*.csv files
that contained CRS related information have been removed from GDAL. It
should be noted that "morphing" from ESRI WKT is now done automatically
when importing WKT.

While general semantics of methods like IsSame() or FindMatches() remain
the same, underneath implementations are substantially different, which
can lead to different results than previous GDAL versions in some cases.
In the FindMatches() case, identification of CRS to EPSG entries is
generally improved due to enhanced query capabilities in the database.

(*) The "mostly" precision is here since it was not practical to do this
rewrite in every place. So for some methods, an internal WKT1 export is
still done. This is the case for methods that take a path to a SRS node
(like "GEOGCS|UNIT") as an argument, or some methods like
SetProjection(), GetProjParm(), that expect a OGC WKT1 specific name.
Those are thought to be used mostly be drivers. Changing them to be EPSG
names would impact a number of drivers, some of them little tested
regarding SRS support, and which furthermore mostly support WKT1
representation only.

OGRCoordinateTransformation changes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Since GDAL 2.3 and initial PROJ 5 support, when transforming between two
CRS we still relied on the PROJ.4 string export of the source and target
CRS to create a coordinate operation pipeline. So this limited to
"early-binding" operations, that is using the WGS84 pivot through
towgs84 or nadgrids PROJ keywords. Now PROJ new capabilities to find
appropriate coordinate operations between two CRS is used, offering
"late-binding" capabilities to take into account other pivots than WGS84
or area of uses.

OGRCreateCoordinateOperation() now takes an extra optional arguments to
define options.

One of those options is to define an area of interest that will be taken
into account when searching candidate operations. If several operations
match, the "best" (according to PROJ sorting criterion) will be
selected. Note: it will systematically be used even if later calls to
Transform() use coordinates outside of the initial area of interest.

Another option is the ability to specify the coordinate operation to
apply, so as an override of what GDAL / PROJ would have automatically
computed, either as a PROJ string (generally a +proj=pipeline), or a WKT
coordinate operation/concatenated operation. Users can typically select
a specific coordinate operation by using the new PROJ projinfo utility
that can return the candidate operations from a source_crs / target_crs
tuple.

When no option is specified, GDAL will use PROJ to list all candidate
coordinate operations. For each call to Transform(), it will compute the
average coordinate of the input coordinates and use it to determine the
best coordinate operation from the candidate ones.

The Transform() method now takes an extra argument to contain the
coordinate epoch (generally as a decimal year value) for coordinate
operations that are time-dependent. Related, the transform options of
the GDALTransform mechanism typically used by gdalwarp now accepts a
COORDINATE_EPOCH for the same purpose.

Use of OGRSpatialReference in GDAL
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Currently GDAL datasets accept and return a WKT 1 string to describe the
SRS. To be more independent of the actual encoding, and for example
allowing a GeoPackage raster dataset to be able to use WKT 2, it is
desirable to be able to attach a SRS that is not dependent of the
representation (WKT 1 or WKT 2), hence using a OGRSpatialReference
object instead of a const char\* string.

The following new methods are added in GDALDataset:

-  virtual const OGRSpatialReference\* GetSpatialRef() const;
-  virtual CPLErr SetSpatialRef(const OGRSpatialReference*);
-  virtual const OGRSpatialReference\* GetGCPSpatialRef() const;
-  virtual CPLErr SetGCPs(int nGCPCount, const GDAL_GCP *pasGCPList,
   const OGRSpatialReference*);

To ease the transition, the following non virtual methods are added in
GDALDataset:

-  const OGRSpatialReference\* GetSpatialRefFromOldGetProjectionRef()
   const;
-  CPLErr OldSetProjectionFromSetSpatialRef(const OGRSpatialReference\*
   poSRS);
-  const OGRSpatialReference\* GetGCPSpatialRefFromOldGetGCPProjection()
   const;
-  CPLErr OldSetGCPsFromNew( int nGCPCount, const GDAL_GCP \*pasGCPList,
   const OGRSpatialReference \* poGCP_SRS );

and the previous GetProjectionRef(), SetProjection(), GetGCPProjection()
and SetGCPs() are available as projected virtual methods, prefixed by an
underscore

This way to convert an existing driver, it is a matter of renaming its
GetProjectionRef() method as \_GetProjectionRef(), and adding:

::

   const OGRSpatialReference* GetSpatialRef() const override {
       return GetSpatialRefFromOldGetProjectionRef();
   }

Default WKT version
~~~~~~~~~~~~~~~~~~~

OGRSpatialReference::exportToWkt() without options will report WKT 1
(with explicit AXIS nodes. See below "Axis order issues" paragraph) for
CRS compatibles of this representation, and otherwise use WKT2:2018
(typically for Geographic 3D CRS).

An enhanced version of exportToWkt() accepts options to specify the
exact WKT version used, if multi-line or single-line output must be
used, etc.

Alternatively the OSR_WKT_FORMAT configuration option can be used to
modify the WKT version used by exportToWk() (when no explicit version is
passed in the options of exportToWkt())

The gdalinfo, ogrinfo and gdalsrsinfo utililies will default to
outputting WKT2:2018

Axis order issues
~~~~~~~~~~~~~~~~~

This is a recurring pain point. This RFC proposes a new approach
(without pretending to solving it completely) to what was initially done
per `RFC 20: OGRSpatialReference Axis Support <./rfc20_srs_axes>`__. The
issue is that CRS official definitions use axis orders that do not
conform to the way raster or vector data is traditionally encoded in GIS
applications. The typical example is the Geographic "WGS 84" definition
from EPSG, EPSG:4326, which uses latitude as the first axis and
longitude as the second axis. RFC 20 decided that by default the AXIS
definition would be stripped off from the WKT when the axis order from
the authority did not match the GIS friendly one (and use a custom EPSGA
authority to have WKT with official AXIS elements)

This was technically possible since the WKT 1 grammar makes the AXIS
element definition. However removal of the AXIS definitions was a
potential source of confusion as it was unclear which axis order was
actually used. Furthermore, in WKT2, the AXIS element is compulsory, and
the internal PROJ representation requires also a coordinate system to be
defined. So there would have been two unsatisfactory options:

-  return patched versions of the official definition with the GIS
   friendly order, while still using the official authority code.
   Practical since we keep the link with the source code, but a lie
   since we modify it. Users would not know whether they must trust the
   encoded order, or the official order from the authority.
-  return patched versions of the official definition with the GIS
   friendly order, but without the official authority code. This would
   be compliant, but we would lose the link with the authority code.

The solution put forward in this RFC is to add a "data axis to SRS axis
mapping" concept, which is a bit similar to what is done in WCS
DescribeCoverage response to explain how the SRS axis map to the grid
axis of a coverage

Extract from
`https://docs.geoserver.org/stable/en/user/extensions/wcs20eo/index.html <https://docs.geoserver.org/stable/en/user/extensions/wcs20eo/index.html>`__
for a coverage that uses EPSG:4326

::

         <gml:coverageFunction>
           <gml:GridFunction>
             <gml:sequenceRule axisOrder="+2 +1">Linear</gml:sequenceRule>
             <gml:startPoint>0 0</gml:startPoint>
           </gml:GridFunction>
         </gml:coverageFunction>

A similar mapping is added to define how the 'x' and 'y' components in
the geotransform matrix or in a OGRGeometry map to the axis defined by
the CRS definition.

Such mapping is given by a new method in OGRSpatialReference

::

   const std::vector<int>& GetDataAxisToSRSAxisMapping() const

To explain its semantics, imagine that it return 2,-1,3. That is
interpreted as:

-  2: the first axis of the CRS maps to the second axis of the data
-  -1: the second axis of the CRS maps to the first axis of the data,
   with values negated
-  3: the third axis of the CRS maps to the third axis of the data

This is similar to the PROJ axisswap operation:
`https://proj4.org/operations/conversions/axisswap.html <https://proj4.org/operations/conversions/axisswap.html>`__

By default, on a newly create OGRSpatialReference object,
GetDataAxisToSRSAxisMapping() returns the identity 1,2[,3], that is,
conform to the axis order defined by the authority.

As all GDAL and a vast majority of OGR drivers depend on using the "GIS
axis mapping", a method SetAxisMappingStrategy(
OAMS_TRADITIONAL_GIS_ORDER or OAMS_AUTHORITY_COMPLIANT or OAMS_CUSTOM )
is added to make their job of specifying the axis mapping easier;

OAMS_TRADITIONAL_GIS_ORDER means:

-  for geographic 2D CRS,

   -  for Latitude NORTH, Longitude EAST (such as EPSG:4326),
      GetDataAxisToSRSAxisMapping() returns {2,1}, meaning that the data
      order is longitude, latitude
   -  for Longitude EAST, Latitude NORTH (such as OGC:CRS84), returns
      {1,2}

-  for projected CRS,

   -  for EAST, NORTH (ie most projected CRS), return {1,2}
   -  for NORTH, EAST, return {2,1}
   -  for North Pole CRS, with East/SOUTH, North/SOUTH, such as
      EPSG:5041 ("WGS 84 / UPS North (E,N)"), would return {1,2}
   -  for North Pole CRS, with northing/SOUTH, easting/SOUTH, such as
      EPSG:32661 ("WGS 84 / UPS North (N,E)"), would return {2,1}
   -  similarly for South Pole CRS
   -  for all other cases, return {1,2}

OGRCreateCoordinateTransformation() now honors the data axis to srs axis
mapping.

Note: contrary to what I indicated in a previous email, gdaltransform
behavior is unchanged, since internally the GDALTransform mechanism
forces the GIS friendly order.

Raster datasets are modified to call
SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER) on the
OGRSpatialReference\* they return, and assumes it in SetSpatialRef()
(assumed and unchecked for now)

Vector layers mostly all call
SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER) on the
OGRSpatialReference\* returned by GetSpatialRef(). In the case of the
GML driver, if the user defines the INVERT_AXIS_ORDER_IF_LAT_LONG open
option, axis swapping is not done (as previously) and the
AUTHORITY_COMPLIANT strategy is used. ICreateLayer() when receiving a
OGRSpatialReference\* may decide (and most will do it) to change the
axis mapping strategy. That is: if it receives a OGRSpatialReference
with AUTHORITY_COMPLIANT order, it may decide to switch to
TRADITIONAL_GIS_ORDER and GetSpatialRef()::GetDataAxisToSRSAxisMapping()
will reflect that. ogr2ogr is modified to do the geometry axis swapping
in that case.

Related to that change, WKT 1 export now always return the AXIS element,
and EPSG:xxxx thus behaves identically to EPSGA:xxxx

So a summary view of this approach is that in the formal SRS definition,
we no longer do derogations regarding axis order, but we add an
additional interface to describe how we actually make our match match
with the SRS definition.

Driver changes
~~~~~~~~~~~~~~

Raster drivers that returned / accepted a SRS as a WKT string through
the GetProjectionRef(), SetProjection(), GetGCPProjection() and
SetGCPs() methods have been upgraded to use the new virtual methods, in
most cases by using the compatibility layer.

The GDALPamDataset (PAM .aux.xml files) and the GDAL VRT drivers have
been fully upgraded to support the new interfaces, and
serialize/deserialize the data axis to SRS axis mapping values.

The GeoPackage driver now fully supports the official "gpkg_crs_wkt"
extension used to store WKT 2 string definitions in the
gpkg_spatial_ref_sys table. The driver attempts at not using the
extension when SRS can be encoded as WKT1 strings, and will
automatically add the "definition_12_063" column to an existing
gpkg_spatial_ref_sys table if a SRS requiring WKT2 (typically a
Geographic 3D CRS) is inserted.

Changes in utilities
~~~~~~~~~~~~~~~~~~~~

-  gdalinfo and ogrinfo reports the data axis to CRS axis mapping
   whenever a CRS is reported. They will also output WKT2_2018 by
   default, unless "-wkt_format wkt1" is specified.

::

   Driver: GTiff/GeoTIFF
   Files: out.tif
   Size is 20, 20
   Coordinate System is:
   GEOGCRS["WGS 84",
       DATUM["World Geodetic System 1984",
           ELLIPSOID["WGS 84",6378137,298.257223563,
               LENGTHUNIT["metre",1]]],
       PRIMEM["Greenwich",0,
           ANGLEUNIT["degree",0.0174532925199433]],
       CS[ellipsoidal,2],
           AXIS["geodetic latitude (Lat)",north,
               ORDER[1],
               ANGLEUNIT["degree",0.0174532925199433]],
           AXIS["geodetic longitude (Lon)",east,
               ORDER[2],
               ANGLEUNIT["degree",0.0174532925199433]],
       USAGE[
           SCOPE["unknown"],
           AREA["World"],
           BBOX[-90,-180,90,180]],
       ID["EPSG",4326]]
   Data axis to CRS axis mapping: 2,1 <-- here
   Origin = (2.000000000000000,49.000000000000000)
   Pixel Size = (0.100000000000000,-0.100000000000000)

-  gdalwarp, ogr2ogr and gdaltransform have gained a -ct switch that can
   be used by advanced users to specify a coordinate operation, either
   as a PROJ string (generally a +proj=pipeline), or a WKT coordinate
   operation/concatenated operation, as explained in the above
   "OGRCoordinateTransformation changes" paragraph. Note: the pipeline
   must take into account the axis order of the CRS, even if the
   underlying raster/vector drivers use the "GIS friendly" order. For
   example "+proj=pipeline +step +proj=axisswap +order=2,1 +step
   +proj=unitconvert +xy_in=deg +xy_out=rad +step +proj=utm +zone=31
   +ellps=WGS84" when transforming from EPSG:4326 to EPSG:32631.

-  gdalsrsinfo is enhanced to be able to specify the 2 new supported WKT
   variants: WKT2_2015 and WKT2_2018. It will default to outputting
   WKT2_2018

SWIG binding changes
~~~~~~~~~~~~~~~~~~~~

The enhanced ExportToWkt() and OGRCoordinateTransformation methods are
available through SWIG bindings. May require additional typemaps for
non-Python languages (particularly for the support of 4D X,Y,Z,time
coordinates)

Backward compatibility
----------------------

This work is intended to be *mostly* backward compatible, yet inevitable
differences will be found. For example the WKT 1 and PROJ string export
has been completely rewritten in PROJ, and so while being hopefully
equivalent to what GDAL 2.4 or earlier generated, this is not strictly
identical: number of significant digits, order of PROJ parameters,
rounding, etc etc...

MIGRATION_GUIDE.TXT has been updated to reflect some differences:

-  OSRImportFromEPSG() takes into account official axis order.
-  removal of OPTGetProjectionMethods(), OPTGetParameterList() and
   OPTGetParameterInfo() No equivalent.
-  removal of OSRFixup() and OSRFixupOrdering(): no longer needed since
   objects constructed are always valid
-  removal of OSRStripCTParms(). Use OSRExportToWktEx() instead with the
   FORMAT=SQSQL option
-  exportToWkt() outputs AXIS nodes
-  OSRIsSame(): now takes into account data axis to CRS axis mapping,
   unless IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES is set as an option
   to OSRIsSameEx()
-  ogr_srs_api.h: SRS_WKT_WGS84 macro is no longer declared by default
   since WKT without AXIS is too ambiguous. Preferred remediation: use
   SRS_WKT_WGS84_LAT_LONG. Or #define USE_DEPRECATED_SRS_WKT_WGS84
   before including ogr_srs_api.h

Out-of-tree raster drivers will be impacted by the introduction of the
new virtual methods GetSpatialRef(), SetSpatialRef(), GetGCPSpatialRef()
and SetGCPs(..., const OGRSpatialReference\* poSRS), and the removal of
their older equivalents using WKT strings instead of a
OGRSpatialReference\* instance.

Documentation
-------------

New methods have been documented, and documentation of existing methods
has been changed when appropriate during the development. That said, a
more thorough pass will be needed. The tutorials will also have to be
updated.

Testing
-------

The autotest suite has been adapted in a number of places since the
expected results have changed for a number of reasons (AXIS node
exported in WKT, differences in WKT and PROJ string generation). New
tests have been added for the new capabilities.

It should be noted that autotest not necessarily checks everything, and
issues have been discovered and fixed through manual testing. The
introduction of the "data axis to CRS axis mapping" concept is also
quite error prone, as it requires setting the OAMS_TRADITIONAL_GIS_ORDER
strategy in a lot of different places.

So users and developers are kindly invited to thoroughly test GDAL once
this work has landed in master.

Implementation
--------------

Done by Even Rouault, `Spatialys <http://www.spatialys.com>`__.
Available per `PR 1185 <https://github.com/OSGeo/gdal/pull/1185>`__
Funded through `gdalbarn <https://gdalbarn.com/>`__ sponsoring.

While it is provided as a multiple commit for """easier""" review, it
will be probably squashed in a single commit for inclusion in master, as
intermediate steps are not all buildable, due to PROJ symbol renames
having occurred during the development, which would break bisectability.

Voting history
--------------

Adopted with +1 from PSC members HowardB, JukkaR, DanielM and EvenR

Modifications
-------------

2019-May-02: change mentions of GDAL 2.5 to GDAL 3.0
