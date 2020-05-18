.. _vector.xplane:

================================================================================
X-Plane/Flightgear aeronautical data
================================================================================

.. shortname:: XPlane

.. built_in_by_default::

The X-Plane aeronautical data is supported for read access. This data is
for example used by the X-Plane and Flightgear software.

The driver is able to read the following files :

============================================== ================= ==================
Filename                                       Description       Supported versions
============================================== ================= ==================
:ref:`apt.dat <xplane_apt>`                    Airport data      850, 810
:ref:`nav.dat <xplane_nav>` (or earth_nav.dat) Navigation aids   810, 740
:ref:`fix.dat <xplane_fix>` (or earth_fix.dat) IFR intersections 600
:ref:`awy.dat <xplane_awy>` (or earth_awy.dat) Airways           640
============================================== ================= ==================

Each file will be reported as a set of layers whose data schema is given
below. The data schema is generally as close as possible to the original
schema data described in the X-Plane specification. However, please note
that meters (or kilometers) are always used to report heights,
elevations, distances (widths, lengths), etc., even if the original data
are sometimes expressed in feet or nautical miles.

Data is reported as being expressed in WGS84 datum (latitude,
longitude), although the specification is not very clear on that
subject.

The OGR_XPLANE_READ_WHOLE_FILE configuration option can be set to FALSE
when reading a big file in regards with the available RAM (especially
true for apt.dat). This option forces the driver not to cache features
in RAM, but just to fetch the features of the current layer. Of course,
this will have a negative impact on performance.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Examples
--------

Converting all the layers contained in 'apt.dat' in a set of shapefiles
:

::

   % ogr2ogr apt_shapes apt.dat

Converting all the layers contained in 'apt.dat' into a PostreSQL
database :

::

   % PG_USE_COPY=yes ogr2ogr -overwrite -f PostgreSQL PG:"dbname=apt" apt.dat

See Also
--------

-  `X-Plane data file
   definitions <http://data.x-plane.com/designers.html>`__

.. _xplane_apt:

Airport data (apt.dat)
----------------------

This file contains the description of elements defining airports,
heliports, seabases, with their runways and taxiways, ATC frequencies,
etc.

The following layers are reported :

-  `APT <#APT>`__ (Point)
-  `RunwayThreshold <#RunwayThreshold>`__ (Point)
-  `RunwayPolygon <#RunwayPolygon>`__ (Polygon)
-  `WaterRunwayThreshold <#WaterRunwayThreshold>`__ (Point)
-  `WaterRunwayPolygon <#WaterRunwayPolygon>`__ (Polygon)
-  `Stopway <#Stopway>`__ (Polygon)
-  `Helipad <#Helipad>`__ (Point)
-  `HelipadPolygon <#HelipadPolygon>`__ (Polygon)
-  `TaxiwayRectangle <#TaxiwayRectangle>`__ (Polygon)
-  `Pavement <#Pavement>`__ (Polygon)
-  `APTBoundary <#APTBoundary>`__ (Polygon)
-  `APTLinearFeature <#APTLinearFeature>`__ (Line String)
-  `StartupLocation <#StartupLocation>`__ (Point)
-  `APTLightBeacon <#APTLightBeacon>`__ (Point)
-  `APTWindsock <#APTWindsock>`__ (Point)
-  `TaxiwaySign <#TaxiwaySign>`__ (Point)
-  `VASI_PAPI_WIGWAG <#VASI_PAPI_WIGWAG>`__ (Point)
-  `ATCFreq <#ATCFreq>`__ (None)

All the layers other than APT will refer to the airport thanks to the
"apt_icao" column, that can serve as a foreign key.

APT layer
~~~~~~~~~

Main description for an airport. The position reported will be the
position of the tower view point if present, otherwise the position of
the first runway threshold found.

Fields:

-  apt_icao: String (5.0). ICAO code for the airport.
-  apt_name: String (0.0). Full name of the airport.
-  type: Integer (1.0). Airport type : 0 for regular airport, 1 for
   seaplane/floatplane base, 2 for heliport
-  elevation_m: Real (8.2). Elevation of the airport (in meters).
-  has_tower: Integer (1.0). Set to 1 if the airport has a tower view
   point.
-  hgt_tower_m: Real (8.2). Height of the tower view point if present.
-  tower_name: String (32.0). Name of the tower view point if present.

RunwayThreshold layer
~~~~~~~~~~~~~~~~~~~~~

| This layer contains the description of one threshold of a runway.
| The runway itself is fully be described by its 2 thresholds, and the
  RunwayPolygon layer.

Note : when a runway has a displaced threshold, the threshold will be
reported as 2 features : one at the non-displaced threshold position
(is_displaced=0), and another one at the displaced threshold position
(is_displaced=1).

Fields:

-  apt_icao: String (5.0). ICAO code for the airport of this runway
   threshold.
-  rwy_num: String (3.0). Code for the runway, such as 18, 02L, etc...
   Unique for each airport.
-  width_m: Real (3.0). Width in meters.
-  surface: String (0.0). Type of the surface among :

   -  Asphalt
   -  Concrete
   -  Turf/grass
   -  Dirt
   -  Gravel
   -  Dry lakebed
   -  Water
   -  Snow
   -  Transparent

-  shoulder: String (0.0). Type of the runway shoulder among :

   -  None
   -  Asphalt
   -  Concrete

-  smoothness: Real (4.2). Runway smoothness. Percentage between 0.00
   and 1.00. 0.25 is the default value.
-  centerline_lights: Integer (1.0). Set to 1 if the runway has
   centre-line lights
-  edge_lighting: String (0.0). Type of edge lighting among :

   -  None
   -  Yes (when imported from V810 records)
   -  LIRL . Low intensity runway lights (proposed for V90x)
   -  MIRL : Medium intensity runway lights
   -  HIRL : High intensity runway lights (proposed for V90x)

-  distance_remaining_signs: Integer (1.0). Set to 1 if the runway has
   'distance remaining' lights.
-  displaced_threshold_m: Real (3.0). Distance between the threshold and
   the displaced threshold.
-  is_displaced: Integer (1.0). Set to 1 if the position is the position
   of the displaced threshold.
-  stopway_length_m: Real (3.0). Length of stopway/blastpad/over-run at
   the approach end of runway in meters
-  markings: String (0.0). Runway markings for the end of the runway
   among :

   -  None
   -  Visual
   -  Non-precision approach
   -  Precision approach
   -  UK-style non-precision
   -  UK-style precision

-  approach_lighting: String (0.0). Approach lighting for the end of the
   runway among :

   -  None
   -  ALSF-I
   -  ALSF-II
   -  Calvert
   -  Calvert ISL Cat II and III
   -  SSALR
   -  SSALS (V810 records)
   -  SSALF
   -  SALS
   -  MALSR
   -  MALSF
   -  MALS
   -  ODALS
   -  RAIL

-  touchdown_lights: Integer (1.0). Set to 1 if the runway has
   touchdown-zone lights (TDZL)
-  REIL: String (0.0). Runway End Identifier Lights (REIL) among :

   -  None
   -  Omni-directional
   -  Unidirectionnal

-  length_m: Real (5.0). (Computed field). Length in meters between the
   2 thresholds at both ends of the runway. The displaced thresholds are
   not taken into account in this computation.
-  true_heading_deg: Real (6.2). (Computed field). True heading in
   degree at the approach of the end of the runway.

RunwayPolygon layer
~~~~~~~~~~~~~~~~~~~

This layer contains the rectangular shape of a runway. It is computed
from the runway threshold information. When not specified, the meaning
of the fields is the same as the `RunwayThreshold <#RunwayThreshold>`__
layer. Fields:

-  apt_icao: String (5.0)
-  rwy_num1: String (3.0). Code for first runway threshold. For example
   20L.
-  rwy_num2: String (3.0). Code for the second the runway threshold. For
   example 02R.
-  width_m: Real (3.0)
-  surface: String (0.0)
-  shoulder: String (0.0)
-  smoothness: Real (4.2)
-  centerline_lights: Integer (1.0)
-  edge_lighting: String (0.0)
-  distance_remaining_signs: Integer (1.0)
-  length_m: Real (5.0)
-  true_heading_deg: Real (6.2). True heading from the first runway to
   the second runway.

WaterRunwayThreshold (Point)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Fields:

-  apt_icao: String (5.0)
-  rwy_num: String (3.0). Code for the runway, such as 18. Unique for
   each airport.
-  width_m: Real (3.0)
-  has_buoys: Integer (1.0). Set to 1 if the runway should be marked
   with buoys bobbing in the water
-  length_m: Real (5.0). (Computed field) Length between the two ends of
   the water runway.
-  true_heading_deg: Real (6.2). (Computed field). True heading in
   degree at the approach of the end of the runway.

WaterRunwayPolygon (Polygon)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This layer contains the rectangular shape of a water runway. It is
computed from the water runway threshold information. Fields:

-  apt_icao: String (5.0)
-  rwy_num1: String (3.0)
-  rwy_num2: String (3.0)
-  width_m: Real (3.0)
-  has_buoys: Integer (1.0)
-  length_m: Real (5.0)
-  true_heading_deg: Real (6.2)

Stopway layer (Polygon)
~~~~~~~~~~~~~~~~~~~~~~~

This layer contains the rectangular shape of
a stopway/blastpad/over-run that may be found at the beginning of a
runway. It is part of the tarmac but not intended to be used for normal
operations. It is computed from the runway stopway/blastpad/over-run
length information and only present when this length is non zero. When
not specified, the meaning of the fields is the same as the
`RunwayThreshold <#RunwayThreshold>`__ layer. Fields:

-  apt_icao: String (5.0)
-  rwy_num: String (3.0).
-  width_m: Real (3.0)
-  length_m: Real (5.0) : Length of stopway/blastpad/over-run at the
   approach end of runway in meters.

Helipad (Point)
~~~~~~~~~~~~~~~

This layer contains the center of a helipad. Fields:

-  apt_icao: String (5.0)
-  helipad_name: String (5.0). Name of the helipad in the format "Hxx".
   Unique for each airport.
-  true_heading_deg: Real (6.2)
-  length_m: Real (5.0)
-  width_m: Real (3.0)
-  surface: String (0.0). See above runway `surface <#surface>`__ codes.
-  markings: String (0.0). See above runway `markings <#markings>`__
   codes.
-  shoulder: String (0.0). See above runway `shoulder <#shoulder>`__
   codes.
-  smoothness: Real (4.2). See above runway `smoothness <#smoothness>`__
   description.
-  edge_lighting: String (0.0). Helipad edge lighting among :

   -  None
   -  Yes (V810 records)
   -  Yellow
   -  White (proposed for V90x)
   -  Red (V810 records)

HelipadPolygon (Polygon)
~~~~~~~~~~~~~~~~~~~~~~~~

This layer contains the rectangular shape of a helipad. The fields are
identical to the `Helipad <#Helipad>`__ layer.

TaxiwayRectangle (Polygon) - V810 record
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This layer contains the rectangular shape of a taxiway. Fields:

-  apt_icao: String (5.0)
-  true_heading_deg: Real (6.2)
-  length_m: Real (5.0)
-  width_m: Real (3.0)
-  surface: String (0.0). See above runway `surface <#surface>`__ codes.
-  smoothness: Real (4.2). See above runway `smoothness <#smoothness>`__
   description.
-  edge_lighting: Integer (1.0). Set to 1 if the taxiway has edge
   lighting.

Pavement (Polygon)
~~~~~~~~~~~~~~~~~~

This layer contains polygonal chunks of pavement for taxiways and
aprons. The polygons may include holes.

The source file may contain Bezier curves as sides of the polygon. Due
to the lack of support for such geometry into OGR Simple Feature model,
Bezier curves are discretized into linear pieces.

Fields:

-  apt_icao: String (5.0)
-  name: String (0.0)
-  surface: String (0.0). See above runway `surface <#surface>`__ codes.
-  smoothness: Real (4.2). See above runway `smoothness <#smoothness>`__
   description.
-  texture_heading: Real (6.2). Pavement texture grain direction in true
   degrees

APTBoundary (Polygon)
~~~~~~~~~~~~~~~~~~~~~

This layer contains the boundary of the airport. There is at the maximum
one such feature per airport. The polygon may include holes. Bezier
curves are discretized into linear pieces.

Fields:

-  apt_icao: String (5.0)
-  name: String (0.0)

APTLinearFeature (Line String)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This layer contains linear features. Bezier curves are discretized into
linear pieces.

Fields:

-  apt_icao: String (5.0)
-  name: String (0.0)

StartupLocation (Point)
~~~~~~~~~~~~~~~~~~~~~~~

Define gate positions, ramp locations etc.

Fields:

-  apt_icao: String (5.0)
-  name: String (0.0)
-  true_heading_deg: Real (6.2)

APTLightBeacon (Point)
~~~~~~~~~~~~~~~~~~~~~~

Define airport light beacons.

Fields:

-  apt_icao: String (5.0)
-  name: String (0.0)
-  color: String (0.0). Color of the light beacon among :

   -  None
   -  White-green: land airport
   -  White-yellow: seaplane base
   -  Green-yellow-white: heliports
   -  White-white-green: military field

APTWindsock (Point)
~~~~~~~~~~~~~~~~~~~

Define airport windsocks.

Fields:

-  apt_icao: String (5.0)
-  name: String (0.0)
-  is_illuminated: Integer (1.0)

TaxiwaySign (Point)
~~~~~~~~~~~~~~~~~~~

Define airport taxiway signs.

Fields:

-  apt_icao: String (5.0)
-  text: String (0.0). This is somehow encoded into a specific format.
   See X-Plane `specification (pages 13 and
   14) <http://developer.x-plane.com/wp-content/uploads/2017/01/XP-APT850-Spec.pdf>`__
   for more details.
-  true_heading_deg: Real (6.2)
-  size: Integer (1.0). From 1 to 5. See X-Plane specification for more
   details.

VASI_PAPI_WIGWAG (Point)
~~~~~~~~~~~~~~~~~~~~~~~~

Define a VASI, PAPI or Wig-Wag. For PAPIs and Wig-Wags, the coordinate
is the centre of the display. For VASIs, this is the mid point between
the two VASI light units.

Fields:

-  apt_icao: String (5.0)
-  rwy_num: String (3.0). Foreign key to the rwy_num field of the
   `RunwayThreshold <#RunwayThreshold>`__ layer.
-  type: String (0.0). Type among :

   -  VASI
   -  PAPI Left
   -  PAPI Right
   -  Space Shuttle PAPI
   -  Tri-colour VASI
   -  Wig-Wag lights

-  true_heading_deg: Real (6.2)
-  visual_glide_deg: Real (4.2)

ATCFreq (None)
~~~~~~~~~~~~~~

Define an airport ATC frequency. Note that this layer has no geometry.

Fields:

-  apt_icao: String (5.0)
-  atc_type: String (4.0). Type of the frequency among (derived from the
   record type number) :

   -  ATIS : AWOS (Automatic Weather Observation System), ASOS
      (Automatic Surface Observation System) or ATIS (Automated Terminal
      Information System)
   -  CTAF : Unicom or CTAF (USA), radio (UK)
   -  CLD : Clearance delivery (CLD)
   -  GND : Ground
   -  TWR : Tower
   -  APP : Approach
   -  DEP : Departure

-  freq_name: String (0.0). Name of the ATC frequency. This is often an
   abbreviation (such as GND for "Ground").
-  freq_mhz: Real (7.3). Frequency in MHz.


.. _xplane_nav:

Navigation aids (nav.dat)
-------------------------

This file contains the description of various navigation aids beacons.

The following layers are reported :

-  `ILS <#ILS>`__ (Point)
-  `VOR <#VOR>`__ (Point)
-  `NDB <#NDB>`__ (Point)
-  `GS <#GS>`__ (Point)
-  `Marker <#Marker>`__ (Point)
-  `DME <#DME>`__ (Point)
-  `DMEILS <#DMEILS>`__ (Point)

ILS (Point)
~~~~~~~~~~~

Localizer that is part of a full ILS, or Stand-alone localizer (LOC),
also including a LDA (Landing Directional Aid) or SDF (Simplified
Directional Facility).

Fields :

-  navaid_id: String (4.0). Identification of nav-aid. \*NOT\* unique.
-  apt_icao: String (5.0). Foreign key to the apt_icao field of the
   `RunwayThreshold <#RunwayThreshold>`__ layer.
-  rwy_num: String (3.0). Foreign key to the rwy_num field of the
   `RunwayThreshold <#RunwayThreshold>`__ layer.
-  subtype: String (10.0). Sub-type among :

   -  ILS-cat-I
   -  ILS-cat-II
   -  ILS-cat-III
   -  LOC
   -  LDA
   -  SDF
   -  IGS
   -  LDA-GS

-  elevation_m: Real (8.2). Elevation of nav-aid in meters.
-  freq_mhz: Real (7.3). Frequency of nav-aid in MHz.
-  range_km: Real (7.3). Range of nav-aid in km.
-  true_heading_deg: Real (6.2). True heading of the localizer in
   degree.

VOR (Point)
~~~~~~~~~~~

Navaid of type VOR, VORTAC or VOR-DME.

Fields :

-  navaid_id: String (4.0). Identification of nav-aid. \*NOT\* unique.
-  navaid_name: String (0.0)
-  subtype: String (10.0). Among VOR, VORTAC or VOR-DME
-  elevation_m: Real (8.2)
-  freq_mhz: Real (7.3)
-  range_km: Real (7.3)
-  slaved_variation_deg: Real (6.2). Indicates the slaved variation of a
   VOR/VORTAC in degrees.

NDB (Point)
~~~~~~~~~~~

Fields :

-  navaid_id: String (4.0). Identification of nav-aid. \*NOT\* unique.
-  navaid_name: String (0.0)
-  subtype: String (10.0). Among NDB, LOM, NDB-DME.
-  elevation_m: Real (8.2)
-  freq_khz: Real (7.3). Frenquency in **kHz**
-  range_km: Real (7.3)

GS - Glideslope (Point)
~~~~~~~~~~~~~~~~~~~~~~~

Glideslope nav-aid.

Fields :

-  navaid_id: String (4.0). Identification of nav-aid. \*NOT\* unique.
-  apt_icao: String (5.0). Foreign key to the apt_icao field of the
   `RunwayThreshold <#RunwayThreshold>`__ layer.
-  rwy_num: String (3.0). Foreign key to the rwy_num field of the
   `RunwayThreshold <#RunwayThreshold>`__ layer.
-  elevation_m: Real (8.2)
-  freq_mhz: Real (7.3)
-  range_km: Real (7.3)
-  true_heading_deg: Real (6.2). True heading of the glideslope in
   degree.
-  glide_slope: Real (6.2). Glide-slope angle in degree (typically 3
   degree)

Marker - ILS marker beacons. (Point)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Nav-aids of type Outer Marker (OM), Middle Marker (MM) or Inner Marker
(IM).

Fields:

-  apt_icao: String (5.0). Foreign key to the apt_icao field of the
   `RunwayThreshold <#RunwayThreshold>`__ layer.
-  rwy_num: String (3.0). Foreign key to the rwy_num field of the
   `RunwayThreshold <#RunwayThreshold>`__ layer.
-  subtype: String (10.0). Among OM, MM or IM.
-  elevation_m: Real (8.2)
-  true_heading_deg: Real (6.2). True heading of the glideslope in
   degree.

DME (Point)
~~~~~~~~~~~

DME, including the DME element of an VORTAC, VOR-DME or NDB-DME.

Fields:

-  navaid_id: String (4.0). Identification of nav-aid. \*NOT\* unique.
-  navaid_name: String (0.0)
-  subtype: String (10.0). Among VORTAC, VOR-DME, TACAN or NDB-DME
-  elevation_m: Real (8.2)
-  freq_mhz: Real (7.3)
-  range_km: Real (7.3)
-  bias_km: Real (6.2). This bias must be subtracted from the calculated
   distance to the DME to give the desired cockpit reading

DMEILS (Point)
~~~~~~~~~~~~~~

DME element of an ILS.

Fields:

-  navaid_id: String (4.0). Identification of nav-aid. \*NOT\* unique.
-  apt_icao: String (5.0). Foreign key to the apt_icao field of the
   `RunwayThreshold <#RunwayThreshold>`__ layer.
-  rwy_num: String (3.0). Foreign key to the rwy_num field of the
   `RunwayThreshold <#RunwayThreshold>`__ layer.
-  elevation_m: Real (8.2)
-  freq_mhz: Real (7.3)
-  range_km: Real (7.3)
-  bias_km: Real (6.2). This bias must be subtracted from the calculated
   distance to the DME to give the desired cockpit reading


.. _xplane_fix:

IFR intersections (fix.dat)
---------------------------

This file contain IFR intersections (often referred to as "fixes").

The following layer is reported :

-  `FIX <#FIX>`__ (Point)

FIX (Point)
~~~~~~~~~~~

Fields:

-  fix_name: String (5.0). Intersection name. \*NOT\* unique.


.. _xplane_awy:

Airways (awy.dat)
-----------------

This file contains the description of airway segments.

The following layers are reported :

-  `AirwaySegment <#AirwaySegment>`__ (Line String)
-  `AirwayIntersection <#AirwayIntersection>`__ (Point)

AirwaySegment (Line String)
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Fields:

-  segment_name: String (0.0)
-  point1_name: String (0.0) : Name of intersection or nav-aid at the
   beginning of this segment
-  point2_name: String (0.0) : Name of intersection or nav-aid at the
   beginning of this segment
-  is_high: Integer (1.0) : Set to 1 if this is a "High" airway.
-  base_FL: Integer (3.0) : Flight level (hundreds of feet) of the base
   of the airway.
-  top_FL: Integer (3.0) : Flight level (hundreds of feet) of the top of
   the airway.

AirwayIntersection (Point)
~~~~~~~~~~~~~~~~~~~~~~~~~~

Fields:

-  name: String (0.0) : Name of intersection or nav-aid
