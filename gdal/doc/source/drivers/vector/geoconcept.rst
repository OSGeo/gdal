.. _vector.geoconcept:

GeoConcept text export
======================

.. shortname:: Geoconcept

.. built_in_by_default::

GeoConcept text export files should be available for writing and
reading.

The OGR GeoConcept driver treats a single GeoConcept file within a
directory as a dataset comprising layers. GeoConcept files extensions
are ``.txt`` or ``.gxt``.

Currently the GeoConcept driver only supports multi-polygons, lines and
points.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::
    
GeoConcept Text File Format (gxt)
---------------------------------

GeoConcept is a GIS developed by the Company GeoConcept SA.

It's an object oriented GIS, where the features are named « objects »,
and feature types are named « type/subtype » (class allowing
inheritance).

Among its import/export formats, it proposes a simple text format named
gxt. A gxt file may contain objects from several type/subtype.

GeoConcept text export files should be available for writing and
reading.

The OGR GeoConcept driver treats a single GeoConcept file within a
directory as a dataset comprising layers. GeoConcept files extensions
are ``.txt`` or ``.gxt``.

Currently the GeoConcept driver only supports multi-polygons, lines and
points.

Creation Issues
---------------

The GeoConcept driver treats a GeoConcept file (``.txt`` or ``.gxt``) as
a dataset.

GeoConcept files can store multiple kinds of geometry (one by layer),
even if a GeoConcept layer can only have one kind of geometry.

Note this makes it very difficult to translate a mixed geometry layer
from another format into GeoConcept format using ogr2ogr, since ogr2ogr
has no support for separating out geometries from a source layer.

GeoConcept sub-type is treated as OGR feature. The name of a layer is
therefore the concatenation of the GeoConcept type name, ``'.'`` and
GeoConcept sub-type name.

GeoConcept type definition (``.gct`` files) are used for creation only.

GeoConcept feature fields definition are stored in an associated
``.gct`` file, and so fields suffer a number of limitations (FIXME) :

-  Attribute names are not limited in length.
-  Only Integer, Real and String field types are supported. The various
   list, and other field types cannot be created for the moment (they
   exist in the GeoConcept model, but are not yet supported by the
   GeoConcept driver).

The OGR GeoConcept driver does not support deleting features.

Dataset Creation Options
~~~~~~~~~~~~~~~~~~~~~~~~

**EXTENSION=TXT|GXT** : indicates the GeoConcept export file extension.
``TXT`` was used by earlier releases of GeoConcept. ``GXT`` is currently
used.

**CONFIG=path to the GCT** : the GCT file describe the GeoConcept types
definitions : In this file, every line must start with ``//#`` followed
by a keyword. Lines starting with ``//`` are comments.

It is important to note that a GeoConcept export file can hold different
types and associated sub-types.

-  configuration section : the GCT file starts with
   ``//#SECTION CONFIG`` and ends with ``//#ENDSECTION CONFIG``. All the
   configuration is enclosed within these marks.
-  map section : purely for documentation at the time of writing this
   document. This section starts with ``//#SECTION MAP`` and ends with
   ``//#ENDSECTION MAP``.
-  type section : this section defines a class of features. A type has a
   name (keyword ``Name``) and an ID (keyword ``ID``). A type holds
   sub-types and fields. This section starts with ``//#SECTION TYPE``
   and ends with ``//#ENDSECTION TYPE``.

   -  sub-type section : this sub-section defines a kind og features
      within a class. A sub-type has a name (keyword ``Name``), an ID
      (keyword ``ID``), a type of geometry (keyword ``Kind``) and a
      dimension. The following types of geometry are supported : POINT,
      LINE, POLYGON. The current release of this driver does not support
      the TEXT geometry. The dimension can be 2D, 3DM or 3D. A sub-type
      holds fields. This section starts with ``//#SECTION SUBTYPE`` and
      ends with ``//#ENDSECTION SUBTYPE``.

      -  fields section : defines user fields. A field has a name
         (keyword ``Name``), an ID (keyword ``ID``), a type (keyword
         ``Kind``). The following types of fields are supported : INT,
         REAL, MEMO, CHOICE, DATE, TIME, LENGTH, AREA. This section
         starts with ``//#SECTION FIELD`` and ends with
         ``//#ENDSECTION FIELD``.

   -  field section : defines type fields. See above.

-  field section : defines general fields. Out of these, the following
   rules apply :

   -  private field names start with a '@' : the private fields are
      ``Identifier``, ``Class``, ``Subclass``, ``Name``, ``NbFields``,
      ``X``, ``Y``, ``XP``, ``YP``, ``Graphics``, ``Angle``.
   -  some private field are mandatory (they must appear in the
      configuration) : ``Identifier``, ``Class``, ``Subclass``,
      ``Name``, ``X``, ``Y``.
   -  If the sub-type is linear (LINE), then the following fields must
      be declared ``XP``, ``YP``.
   -  If the sub-type is linear or polygonal (LINE, POLY), then
      ``Graphics`` must be declared.
   -  If the sub-type is ponctual or textual (POINT, TEXT), the
      ``Angle`` may be declared.

   When this option is not used, the driver manage types and sub-types
   name based on either the layer name or on the use of ``-nln`` option.

Layer Creation Options
~~~~~~~~~~~~~~~~~~~~~~

**FEATURETYPE=TYPE.SUBTYPE** : defines the feature to be created. The
``TYPE`` corresponds to one of the ``Name`` found in the GCT file for a
type section. The ``SUBTYPE`` corresponds to one of the ``Name`` found
in the GCT file for a sub-type section within the previous type section.

At the present moment, coordinates are written with 2 decimals for
Cartesian spatial reference systems (including height) or with 9
decimals for geographical spatial reference systems.

Examples
~~~~~~~~

Example of a .gct file :
^^^^^^^^^^^^^^^^^^^^^^^^

::

   //#SECTION CONFIG
   //#SECTION MAP
   //# Name=SCAN1000-TILES-LAMB93
   //# Unit=m
   //# Precision=1000
   //#ENDSECTION MAP
   //#SECTION TYPE
   //# Name=TILE
   //# ID=10
   //#SECTION SUBTYPE
   //# Name=TILE
   //# ID=100
   //# Kind=POLYGON
   //# 3D=2D
   //#SECTION FIELD
   //# Name=IDSEL
   //# ID=101
   //# Kind=TEXT
   //#ENDSECTION FIELD
   //#SECTION FIELD
   //# Name=NOM
   //# ID=102
   //# Kind=TEXT
   //#ENDSECTION FIELD
   //#SECTION FIELD
   //# Name=WITHDATA
   //# ID=103
   //# Kind=INT
   //#ENDSECTION FIELD
   //#ENDSECTION SUBTYPE
   //#ENDSECTION TYPE
   //#SECTION FIELD
   //# Name=@Identifier
   //# ID=-1
   //# Kind=INT
   //#ENDSECTION FIELD
   //#SECTION FIELD
   //# Name=@Class
   //# ID=-2
   //# Kind=CHOICE
   //#ENDSECTION FIELD
   //#SECTION FIELD
   //# Name=@Subclass
   //# ID=-3
   //# Kind=CHOICE
   //#ENDSECTION FIELD
   //#SECTION FIELD
   //# Name=@Name
   //# ID=-4
   //# Kind=TEXT
   //#ENDSECTION FIELD
   //#SECTION FIELD
   //# Name=@X
   //# ID=-5
   //# Kind=REAL
   //#ENDSECTION FIELD
   //#SECTION FIELD
   //# Name=@Y
   //# ID=-6
   //# Kind=REAL
   //#ENDSECTION FIELD
   //#SECTION FIELD
   //# Name=@Graphics
   //# ID=-7
   //# Kind=REAL
   //#ENDSECTION FIELD
   //#ENDSECTION CONFIG

Example of a GeoConcept text export :
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

::

   //$DELIMITER "    "
   //$QUOTED-TEXT "no"
   //$CHARSET ANSI
   //$UNIT Distance=m
   //$FORMAT 2
   //$SYSCOORD {Type: 2001}
   //$FIELDS Class=TILE;Subclass=TILE;Kind=4;Fields=Private#Identifier    Private#Class    Private#Subclass    Private#Name    Private#NbFields    IDSEL    NOM    WITHDATA    Private#X    Private#Y    Private#Graphics
   -1    TILE    TILE    TILE    3    000-2007-0050-7130-LAMB93    0    50000.00     7130000.00    4    600000.00     7130000.00    600000.00     6580000.00    50000.00     6580000.00    50000.00     7130000.00
   -1    TILE    TILE    TILE    3    000-2007-0595-7130-LAMB93    0    595000.00    7130000.00    4    1145000.00    7130000.00    1145000.00    6580000.00    595000.00    6580000.00    595000.00    7130000.00
   -1    TILE    TILE    TILE    3    000-2007-0595-6585-LAMB93    0    595000.00    6585000.00    4    1145000.00    6585000.00    1145000.00    6035000.00    595000.00    6035000.00    595000.00    6585000.00
   -1    TILE    TILE    TILE    3    000-2007-1145-6250-LAMB93    0    1145000.00   6250000.00    4    1265000.00    6250000.00    1265000.00    6030000.00    1145000.00   6030000.00    1145000.00   6250000.00
   -1    TILE    TILE    TILE    3    000-2007-0050-6585-LAMB93    0    50000.00     6585000.00    4    600000.00     6585000.00    600000.00     6035000.00    50000.00     6035000.00    50000.00     6585000.00

Example of use :
^^^^^^^^^^^^^^^^

| Creating a GeoConcept export file :

::

   ogr2ogr -f "Geoconcept" -a_srs "+init=IGNF:LAMB93" -dsco EXTENSION=txt -dsco CONFIG=tile_schema.gct tile.gxt tile.shp -lco FEATURETYPE=TILE.TILE

| Appending new features to an existing GeoConcept export file :

::

   ogr2ogr -f "Geoconcept" -update -append tile.gxt tile.shp -nln TILE.TILE

| Translating a GeoConcept export file layer into MapInfo file :

::

   ogr2ogr -f "MapInfo File" -dsco FORMAT=MIF tile.mif tile.gxt TILE.TILE

See Also
~~~~~~~~

-  `GeoConcept web site <http://www.geoconcept.com/>`__
