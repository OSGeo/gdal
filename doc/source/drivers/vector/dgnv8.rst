.. _vector.dgnv8:

Microstation DGN v8
===================

.. versionadded:: 2.2

.. shortname:: DGNv8

.. build_dependencies:: Open Design Alliance Teigha library

Microstation DGN files from Microstation version 8.0 are supported for
reading and writing. Each model of the file is represented by a OGR
layer.

This driver requires to be built against the (non open source) Open
Design Alliance Teigha library.

DGN files are considered to have no georeferencing information through
OGR. Features will all have the following generic attributes:

-  Type: The integer type code as listed below in supported elements.
-  Level: The DGN level number.
-  GraphicGroup: The graphic group number.
-  ColorIndex: The color index from the dgn palette.
-  Weight: The drawing weight (thickness) for the element.
-  Style: The style value for the element.
-  ULink: User data linkage (multiple user data linkages may exist for each element).

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Supported Elements
------------------

The following element types are supported in reading:

-  Cell Header (2): used for polygons with holes
-  Line (3): Line (2 points) geometry.
-  Line String (4): Multi segment line geometry.
-  Shape (6): Polygon geometry.
-  TextNode (7): Container of Text elements.
-  Curve (11): Approximated as a line geometry.
-  ComplexString (12): Treated as line string or compound curve.
-  ComplexShape (14): Treated as polygon or curve polygon.
-  Ellipse (15): Approximated as a line geometry or a circular string.
-  Arc (16): Approximated as a line geometry or a circular string.
-  Text (17): Treated as a point geometry.
-  B-Spline (21): Treated as a line geometry.
-  PointString (22): Treated as multi point.
-  Shared cell reference (35): Treated as point.

Generally speaking any concept of complex objects, and cells as
associated components is lost. Each component of a complex object or
cell is treated as a independent feature.

User data linkage
-----------------

A DGN element may have multiple user data linkages. Each linkage has 
a user id, application id and a number of words of data. The user 
data linkage output reports the data for each different application id.

For unknown application ids, the raw data is reported as hexadecimal 
words (16bit). Is up to the user how to decode the user data, depending 
on the application id.

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

Metadata
--------

The various metadata items that can be set in the DGN header with the
dataset creation options (see below) can be retrieved in the "DGN"
metadata domain.

Creation Issues
---------------

DGN files may be written with OGR with limitations:

-  Output features have the usual fixed DGN attributes. Attempts to
   create any other fields will fail.
-  Translation from OGR feature style strings back into DGN
   representation information is limited to a few properties of LABEL
   (text, font name, size, angle, color), PEN (color) and BRUSH (fill
   color) tools.
-  POINT geometries that are not text (Text is NULL, and the feature
   style string is not a LABEL) will be translated as a degenerate (0
   length) line element.
-  Geometries which fall outside the "design plane" of the seed file
   will be discarded, or corrupted in unpredictable ways.

Dataset creation options
------------------------

-  **SEED=**\ *filename*: Specify the seed file to use.
-  **COPY_SEED_FILE_COLOR_TABLE=**\ *YES/NO*: Indicates whether the
   color table should be copied from the seed file. Only taken into
   account if SEED is specified. By default this is NO.
-  **COPY_SEED_FILE_MODEL=**\ *YES/NO*: Indicates whether the existing
   models (without their graphic contents) should be copied from the
   seed file. This holds as well for the view groups and named views to
   which they are linked to. Only taken into account if SEED is
   specified. By default this is YES.
-  **COPY_SEED_FILE_MODEL_CONTROL_ELEMENTS=**\ *YES/NO*: Indicates
   whether the existing control elements of models should be copied from
   the seed file. Only taken into account if COPY_SEED_FILE_MODEL=YES.
   By default this is YES.
-  **APPLICATION=**\ *string*: Set Application field in header. If not
   specified, derived from seed file when set. Otherwise mentions the
   version of GDAL and the Teigha library used.
-  **TITLE=**\ *string*: Set Title field in header. If not specified,
   from the seed file.
-  **SUBJECT=**\ *string*: Set Subject field in header. If not
   specified, from the seed file.
-  **AUTHOR=**\ *string*: Set Author field in header. If not specified,
   from the seed file.
-  **KEYWORDS=**\ *string*: Set Keywords field in header. If not
   specified, from the seed file.
-  **TEMPLATE=**\ *string*: Set Template field in header. If not
   specified, from the seed file.
-  **COMMENTS=**\ *string*: Set Comments field in header. If not
   specified, from the seed file.
-  **LAST_SAVED_BY=**\ *string*: Set LastSavedBy field in header. If not
   specified, from the seed file.
-  **REVISION_NUMBER=**\ *string*: Set RevisionNumber field in header.
   If not specified, from the seed file.
-  **CATEGORY=**\ *string*: Set Category field in header. If not
   specified, from the seed file.
-  **MANAGER=**\ *string*: Set Manager field in header. If not
   specified, from the seed file.
-  **COMPANY=**\ *string*: Set Company field in header. If not
   specified, from the seed file.

Layer creation options
----------------------

-  **DESCRIPTION=**\ *string*: Description associated with the layer. If
   not specified, from the seed file.
-  **DIM=**\ *2/3*: Dimension (ie 2D vs 3D) of the layer. By default, 3,
   unless the model is reused from the seed file.


Building
--------

See :ref:`ODA platform support <vector.oda>` for building GDAL with ODA support.

.. toctree::
   :hidden:

   oda

--------------

-  :ref:`DGN (v7) driver <vector.dgn>`
-  :ref:`ogr_feature_style`
