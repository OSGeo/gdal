.. _wktproblems:

================================================================================
OGC WKT Coordinate System Issues
================================================================================

This document is intended to discuss some issues that arise in
attempting to use OpenGIS Well Known Text descriptions of coordinate
systems. It discusses various vendor implementations and issues between
the original `"Simple Features" specification (ie. SF-SQL
99-049) <http://portal.opengeospatial.org/files/?artifact_id=829>`__ and
the newer `Coordinate Transformation Services (CT) specification
(01-009) <http://portal.opengeospatial.org/files/?artifact_id=999>`__
which defines an extended form of WKT.

WKT Implementations
-------------------

At this time I am aware of at least the following software packages that
use some form of WKT internally, or for interchange of coordinate system
descriptions:

-  Oracle Spatial (WKT is used internally in MDSYS.WKT, loosely SFSQL
   based)
-  ESRI - The Arc8 system's projection engine uses a roughly simple
   features compatible description for projections. I believe ESRI
   provided the WKT definition for the simple features spec.
-  Cadcorp - Has the ability to read and write CT 1.0 style WKT. Cadcorp
   wrote the CT spec.
-  OGR/GDAL - reads/writes WKT as its internal coordinate system
   description format. Attempts to support old and new forms as well as
   forms from ESRI.
-  FME - Includes WKT read/write capabilities built on OGR.
-  MapGuide - Uses WKT in the SDP data access API. Roughly SF compliant.
-  PostGIS - Keeps WKT in the spatial_ref_sys table, but it is up to
   clients to translate to PROJ.4 format for actual use. I believe the
   spatial_ref_sys table is populated using OGR generated translations.

Projection Parameters
---------------------

The various specs do not list a set of projections, and the parameters
associated with them. This leads to various selection of parameter names
(and sometimes projection names) from different vendors. I have
attempted to maintain a list of WKT bindings for different projections
as part of my `GeoTIFF Projections
List <https://web.archive.org/web/20130728081442/http://www.remotesensing.org/geotiff/proj_list/>`__
registry. Please try to adhere to the projection names and parameters
listed there. That list also tries to relate the projections to the
GeoTIFF, EPSG and PROJ.4 formulations where possible.

The one case where it isn't followed by a vendor that I am aware of
ESRIs definition of Lambert Conformal Conic. In EPSG there is a 1SP and
a 2SP form of this. ESRI merges them, and just have different parameters
depending on the type.

One other issue is that the CT specification does explicitly list
parameters for the Transverse Mercator, LCC 1SP and LCC 2SP projections;
however, it lists standard_parallel1 and standard_parallel2 as
parameters for LCC 2SP which conflicts with the existing usage of
standard_parallel_1 and standard_parallel_2 and conflicts with examples
in the same CT spec. My position is that the table in section 10.x of
the CT spec is in error and that the widely used form is correct. Note
that the table in the CT spec conflicts with other examples in the same
spec.

A third issue is the formulation for Albers. While I have used
longitude_of_center and latitude_of_center ESRI uses Central_meridian
and latitude_of_origin.

ESRI:

::

   PROJECTION["Albers"],
   PARAMETER["False_Easting",1000000.0],
   PARAMETER["False_Northing",0.0],
   PARAMETER["Central_Meridian",-126.0],
   PARAMETER["Standard_Parallel_1",50.0],
   PARAMETER["Standard_Parallel_2",58.5],
   PARAMETER["Latitude_Of_Origin",45.0],

OGR:

::

   PROJECTION["Albers"],
   PARAMETER["standard_parallel_1",50],
   PARAMETER["standard_parallel_2",58.5],
   PARAMETER["longitude_of_center",-126],
   PARAMETER["latitude_of_center",45],
   PARAMETER["false_easting",1000000],
   PARAMETER["false_northing",0],

Datum Names
-----------

In Simple Features style WKT, the name associated with a datum is the
only way to identify the datum. In CT WKT the datum can also have a
TOWGS84 parameter indicating its relationship to WGS84, and an AUTHORITY
parameter relating it to EPSG or some other authority space. However, in
SF WKT the name itself is the only key.

By convention OGR and Cadcorp have translated the datum names in a
particular way from the EPSG database in order to produce comparible
names. The rule is to convert all non alphanumeric characters to
underscores, then to strip any leading, trailing or repeating
underscores. This produces well behaved datum names like
"Nouvelle_Triangulation_Francaise".

However, other vendors have done different things. ESRI seems to follow
a similar convention but prefixes all datum names with "D\_" as well,
giving names like "D_WGS_1972". Also they have lots of other differences
for reasons that are not clear. For instance for what Cadcorp and OGR
call "Nouvelle_Triangulation_Francaise", they call it "D_NTF". Oracle
appears to use the raw names without cleanup. So for NTF they use "NTF
(Paris meridian)".

The short result of this is that it is almost impossible to recognise
and compare datums between different Simple Features implementations,
though I have had some success in translating ESRI datum names to match
Cadcorp/OGR conventions, with some special casing.

Parameter Ordering
------------------

It is worthwhile keeping in mind that the BNF grammars for WKT in the SF
specs, and the CT spec imply specific orders for most items. For
instance the BNF for the PROJCS item in the CT spec is

::

   <projected cs> =
     PROJCS["<name>", <geographic cs>, <projection>, {<parameter>,}* <linear unit> {,<twin axes>}{,<authority>}]

This clearly states that the PROJECTION keyword follows the GEOGCS,
followed by the UNIT, AXIS and AUTHORITY items. Providing them out of
order is technically a violation of the spec. On the other hand, WKT
consumers are encouraged to be flexible on ordering.

Units of PARAMETERs
-------------------

The linear PARAMETER values in a PROJCS must be in terms of the linear
units for that PROJCS. I think the only linear units are the false
easting and northing type values. Thus, in common cases like a state
plane zone in feet, the false easting and northing will also be in feet.

The angular PARAMETER values in a PROJCS must be in terms of the angular
units of the GEOGCS. If the GEOGCS is in gradians, for instance, then
all the projection angles must also be in gradians!

Units of PRIMEM
---------------

What units should the prime meridian appear in?

-  The CT 1.0 specification (7.3.14 PRIMEM) says *"The units of the must
   be inferred from the context. If the PRIMEM clause occurs inside a
   GEOGCS, then the longitude units will match those of the geographic
   coordinate system."* Note: for a geocentric coordinate system, it
   says *"If the PRIMEM clause occurs inside a GEOCCS, then the units
   will be in degrees"*.
-  The SF-SQL spec (99-049) does not attempt to address the issue of
   units of the prime meridian.
-  Existing ESRI EPSG translation to WKT uses degrees for prime
   meridian, even when the GEOGCS is in gradians as shown in their
   translation of EPSG 4807:

   ::

      GEOGCS["GCS_NTF_Paris",
        DATUM["D_NTF",
          SPHEROID["Clarke_1880_IGN",6378249.2,293.46602]],
        PRIMEM["Paris",2.337229166666667],
        UNIT["Grad",0.015707963267948967]]

-  OGR implements the same interpretation as ESRI for its
   OGRSpatialReference class: the PRIMEM longitude is always in degrees.
   See `GDAL Ticket #4524 <https://trac.osgeo.org/gdal/ticket/4524>`__

   ::

      GEOGCS["NTF (Paris)",
          DATUM["Nouvelle_Triangulation_Francaise_Paris",
              SPHEROID["Clarke 1880 (IGN)",6378249.2,293.4660212936269,
                  AUTHORITY["EPSG","7011"]],
              TOWGS84[-168,-60,320,0,0,0,0],
              AUTHORITY["EPSG","6807"]],
          PRIMEM["Paris",2.33722917,
              AUTHORITY["EPSG","8903"]],
          UNIT["grad",0.01570796326794897,
              AUTHORITY["EPSG","9105"]],
          AUTHORITY["EPSG","4807"]]

-  Cadcorp implements according to the CT 1.0 specification as shown in
   their translation of EPSG 4807:

   ::

      GEOGCS["NTF (Paris)",
        DATUM["Nouvelle_Triangulation_Francaise",
          SPHEROID["Clarke 1880 (IGN)",6378249.2,293.466021293627,
            AUTHORITY["EPSG",7011]],
          TOWGS84[-168,-60,320,0,0,0,0],
          AUTHORITY["EPSG",6275]],
        PRIMEM["Paris",2.5969213,
          AUTHORITY["EPSG",8903]],
        UNIT["grad",0.015707963267949,
          AUTHORITY["EPSG",9105]],
        AXIS["Lat",NORTH],
        AXIS["Long",EAST],
        AUTHORITY["EPSG",4807]]

-  Oracle Spatial 8.1.7 uses the following definition for what I assume
   is supposed to be EPSG 4807. Interestingly it does not bother with
   using gradians, and it appears that the prime meridian is expressed
   in radians with very low precision!

   ::

      GEOGCS [ "Longitude / Latitude (NTF with Paris prime meridian)",
        DATUM ["NTF (Paris meridian)",
          SPHEROID ["Clarke 1880 (IGN)", 6378249.200000, 293.466021]],
        PRIMEM [ "", 0.000649 ],
        UNIT ["Decimal Degree", 0.01745329251994330]]

Sign of TOWGS84 Rotations
-------------------------

Discussion
~~~~~~~~~~

In EPSG there are two methods of defining the 7 parameter bursa wolf
parameters, 9606 (position vector 7-parameter) and 9607 (coordinate
frame rotation). The only difference is that the sign of the rotation
coefficients is reversed between them.

I (Frank Warmerdam) had somehow convinced myself that the TOWGS84 values
in WKT were supposed to be done using the sense in 9606 (position vector
7-parameter) and that if I read a 9607 I would need to switch the
rotation signs before putting it into a TOWGS84 chunk in WKT.

However, I see in the WKT dump you (Martin from Cadcorp) sent me you are
using the 9607 sense. For instance, this item appears to use 9607 values
directly without switching the sign.

::

    GEOGCS["DHDN",
       DATUM["Deutsche_Hauptdreiecksnetz",
         SPHEROID["Bessel 1841",6377397.155,299.1528128,AUTHORITY["EPSG","7004"]],
         TOWGS84[582,105,414,-1.04,-0.35,3.08,8.3],
         AUTHORITY["EPSG","6314"]],
       PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],
       UNIT["DMSH",0.0174532925199433,AUTHORITY["EPSG","9108"]],
       AXIS["Lat",NORTH],AXIS["Long",EAST],AUTHORITY["EPSG","4314"]]

I read over the TOWGS84[] clause in the 1.0 CT spec, and it just talks
about them being the Bursa Wolf transformation parameters (on page 22,
7.3.18). I also scanned through to 12.3.15.2 and 12.3.27 and they are
nonspecific as to the handedness of the TOWGS84 rotations.

I am seeking a clarification of whether TOWGS84 matches EPSG 9606 or
EPSG 9607. Furthermore, I would like to see any future rev of the spec
clarify this, referencing the EPSG method definitions.

Martin wrote back that he was uncertain on the correct signage and that
the Adam had programmed the Cadcorp implementation empirically,
according to what seemed to work for the test data available.

I am prepared to adhere to the Cadorp sign usage (as per EPSG 9607) if
this can be clarified in the specification.

Current state of OGR implementation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OGR imports from/exports to WKT assumes EPSG 9606 convention (position
vector 7-parameter), as `proj.4
does <http://proj4.org/parameters.html#towgs84-datum-transformation-to-wgs84>`__.

When importing from EPSG parameters expressed with EPSG 9607, it does
the appropriate conversion (negating the sign of the rotation terms).

Longitudes Relative to PRIMEM?
------------------------------

Another related question is whether longtiudinal projection parameters
(ie. central meridian) are relative to the GEOGCS prime meridian or
relative to greenwich. While the simplest approach is to treat all
longitudes as relative to Greenwich, I somehow convinced myself at one
point that the longitudes were intended to be relative to the prime
meridian. However, a review of 7.3.11 (describing PARAMETER) in the CT
1.0 spec provides no support for this opinion, and an inspection of EPSG
25700 in Cadcorp also suggests that the central meridian is relative to
greenwich, not the prime meridian.

::

   PROJCS["Makassar (Jakarta) / NEIEZ",
       GEOGCS["Makassar (Jakarta)",
           DATUM["Makassar",
               SPHEROID["Bessel 1841",6377397.155,299.1528128,
                   AUTHORITY["EPSG","7004"]],
               TOWGS84[0,0,0,0,0,0,0],
               AUTHORITY["EPSG","6257"]],
           PRIMEM["Jakarta",106.807719444444,
               AUTHORITY["EPSG","8908"]],
           UNIT["DMSH",0.0174532925199433,
               AUTHORITY["EPSG","9108"]],
           AXIS["Lat","NORTH"],
           AXIS["Long","EAST"],
           AUTHORITY["EPSG","4804"]],
       PROJECTION["Mercator_1SP",
           AUTHORITY["EPSG","9804"]],
       PARAMETER["latitude_of_origin",0],
       PARAMETER["central_meridian",110],
       PARAMETER["scale_factor",0.997],
       PARAMETER["false_easting",3900000],
       PARAMETER["false_northing",900000],
       UNIT["metre",1,
           AUTHORITY["EPSG","9001"]],
       AXIS["X","EAST"],
       AXIS["Y","NORTH"],
       AUTHORITY["EPSG","25700"]]

Based on this, I am proceeding on the assumption that while parameters
are in the units of the GEOGCS they are not relative the GEOGCS prime
meridian.

Numerical Precision in WKT
--------------------------

The specification does not address the precision to which values in WKT
should be stored. Some implementations, such as Oracles apparently, use
rather limited precision for parameters such as Scale Factor making it
difficult to compare coordinate system descriptions or even to get
comparable numerical results.

The best practice is to preserve the original precision as specified in
the source database, such as EPSG where possible. Given that many
systems do not track precision, at least it is advisable to produce
values with the equivalent of the C "%.16g" format, maintaining 16
digits of precision, capturing most of the precision of a double
precision IEEE floating point value.

Other Notes
-----------

#. ESRI seems to use Equidistant_Cylindrical for what I know as
   Equirectangular.

--------------

History

-  2018: Even Rouault: make it clear that OGR implements EPSG 9606
   convention for TOWGS84.
-  2018: Even Rouault: remove mention about CT 1.0 specification (7.3.14
   PRIMEM) having an error, and explicitly mentions that OGR uses
   degrees for PRIMEM longitude.
-  2018: Even Rouault: add hyperlinks
-  2007 or before: Originally written by `Frank
   Warmerdam <https://web.archive.org/web/20130728081442/http://pobox.com/~warmerdam>`__.
