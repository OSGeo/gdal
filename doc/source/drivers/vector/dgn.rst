.. _vector.dgn:

Microstation DGN
================

.. shortname:: DGN

.. built_in_by_default::

Microstation DGN files from Microstation versions predating version 8.0
are supported for reading (a :ref:`DGNv8 driver <vector.dgnv8>`, using
Teigha libraries, is available to read and write DGN v8 files). The
entire file is represented as one layer (named "elements").

DGN files are considered to have no georeferencing information through
OGR. Features will all have the following generic attributes:

-  Type: The integer type code as listed below in supported elements.
-  Level: The DGN level number (0-63).
-  GraphicGroup: The graphic group number.
-  ColorIndex: The color index from the dgn palette.
-  Weight: The drawing weight (thickness) for the element.
-  Style: The style value for the element.
-  EntityNum and MSLink: The Entity ID and MSLINK values in database linkage.
-  ULink: User data linkage (multiple user data linkages may exist for each element).

DGN files do not contain spatial indexes; however, the DGN driver does
take advantage of the extents information at the beginning of each
element to minimize processing of elements outside the current spatial
filter window when in effect.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Supported Elements
------------------

The following element types are supported:

-  Line (3): Line geometry.
-  Line String (4): Multi segment line geometry.
-  Shape (6): Polygon geometry.
-  Curve (11): Approximated as a line geometry.
-  B-Spline (21): Treated (inaccurately) as a line geometry.
-  Arc (16): Approximated as a line geometry.
-  Ellipse (15): Approximated as a line geometry.
-  Text (17): Treated as a point geometry.

Generally speaking any concept of complex objects, and cells as
associated components is lost. Each component of a complex object or
cell is treated as a independent feature.

MSLINK
------

A DGN element can have a correspondence to a row in a database table,
known as database linkage or database attribute. The EntityNum 
refers to the database table. The MSLink is the key to find the
row in that table.

User data linkage
-----------------

A DGN element may have multiple user data linkages. Each linkage has 
a user id, application id and a number of words of data. The user 
data linkage output reports the data for each different application id
found as raw hexadecimal words (16bits). The application id is the 
second word of the raw data.

Is up to the user how to decode the user raw data, depending on the 
application id.

Styling Information
-------------------

Some drawing information about features can be extracted from the
ColorIndex, Weight and Style generic attributes; however, for all
features an OGR style string has been prepared with the values encoded
in ready-to-use form for applications supporting OGR style strings.

The various kinds of linear geometries will carry style information
indicating the color, thickness and line style (i.e. dotted, solid,
etc).

Polygons (Shape elements) will carry styling information for the edge as
well as a fill color if provided. Fill patterns are not supported.

Text elements will contain the text, angle, color and size information
(expressed in ground units) in the style string.

Creation Issues
---------------

2D DGN files may be written with OGR with significant limitations:

-  Output features have the usual fixed DGN attributes. Attempts to
   create any other fields will fail.
-  Virtual no effort is currently made to translate OGR feature style
   strings back into DGN representation information.
-  POINT geometries that are not text (Text is NULL, and the feature
   style string is not a LABEL) will be translated as a degenerate (0
   length) line element.
-  Polygon, and multipolygon objects will be translated to simple
   polygons with all rings other than the first discarded.
-  Polygons and line strings with too many vertices will be split into a
   group of elements prefixed with a Complex Shape Header or Complex
   Chain Header element as appropriate.
-  A seed file must be provided (or if not provided,
   $PREFIX/share/gdal/seed_2d.dgn will be used). Many aspects of the
   resulting DGN file are determined by the seed file, and cannot be
   affected via OGR, such as initial view window.
-  The various collection geometries other than MultiPolygon are
   completely discarded at this time.
-  Geometries which fall outside the "design plane" of the seed file
   will be discarded, or corrupted in unpredictable ways.
-  DGN files can only have one layer. Attempts to create more than one
   layer in a DGN file will fail.

Dataset creation options
------------------------

-  **3D=**\ *YES* or *NO*: Determine whether 2D (seed_2d.dgn) or 3D
   (seed_3d.dgn) seed file should be used. This option is ignored if the
   SEED option is provided.
-  **SEED=**\ *filename*: Override the seed file to use.
-  **COPY_WHOLE_SEED_FILE=**\ *YES/NO*: Indicate whether the whole seed
   file should be copied. If not, only the first three elements (and
   potentially the color table) will be copied. Default is NO.
-  **COPY_SEED_FILE_COLOR_TABLE=**\ *YES/NO*: Indicates whether the
   color table should be copied from the seed file. By default this is
   NO.
-  **MASTER_UNIT_NAME=**\ *name*: Override the master unit name from the
   seed file with the provided one or two character unit name.
-  **SUB_UNIT_NAME=**\ *name*: Override the sub unit name from the seed
   file with the provided one or two character unit name.
-  **SUB_UNITS_PER_MASTER_UNIT=**\ *count*: Override the number of
   subunits per master unit. By default the seed file value is used.
-  **UOR_PER_SUB_UNIT=**\ *count*: Override the number of UORs (Units of
   Resolution) per sub unit. By default the seed file value is used.
-  **ORIGIN**\ =\ *x,y,z*: Override the origin of the design plane. By
   default the origin from the seed file is used.

--------------

-  `Dgnlib Page <http://dgnlib.maptools.org/>`__
-  :ref:`ogr_feature_style`
-  :ref:`DGNv8 driver <vector.dgnv8>` (using Teigha libraries)
