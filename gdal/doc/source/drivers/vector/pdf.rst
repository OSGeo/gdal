.. _vector.pdf:

================================================================================
PDF -- Geospatial PDF
================================================================================

.. shortname:: PDF

.. build_dependencies:: none for write support, Poppler/PoDoFo/PDFium for read support

Refer to the :ref:`PDF raster <raster.pdf>` documentation page for common
documentation of the raster and vector sides of the driver.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Vector support
--------------

This driver can read and write geospatial PDF
with vector features. Vector read support requires linking to one of the
above mentioned dependent libraries, but write support does not. The
driver can read vector features encoded according to PDF's logical
structure facilities (as described by "§10.6 - Logical Structure" of PDF
spec), or retrieve only vector geometries for other vector PDF files.

If there is no such logical structure, the driver will not try to
interpret the vector content of the PDF, unless you defined the
:decl_configoption:`OGR_PDF_READ_NON_STRUCTURED` configuration option to YES.

Feature style support
---------------------

For write support, the driver has partial support for the style
information attached to features, encoded according to the
:ref:`ogr_feature_style`.

The following tools are recognized:

-  For points, LABEL and SYMBOL.
-  For lines, PEN.
-  For polygons, PEN and BRUSH.

The supported attributes for each tool are summed up in the following
table:

.. list-table::
   :header-rows: 1
   :widths: 10 60 30

   * - Tool
     - Supported attributes
     - Example
   * - PEN
     - color (c); width (w); dash pattern (p)
     - PEN(c:#FF0000,w:5px)
   * - BRUSH
     - foreground color (fc)
     - BRUSH(fc:#0000FF)
   * - LABEL
     - | GDAL >= 2.3.0: text (t), limited to ASCII strings; font name (f), see
       | note below; font size (s); bold (bo); italic (it); text color (c); x and
       | y offsets (dx, dy); angle (a); anchor point (p), values 1 through 9;
       | stretch (w)
       | GDAL <= 2.2.x: text (t), limited to ASCII strings; font size (s); text
       | color (c); x and y offsets (dx, dy); angle (a)
     - LABEL(c:#000000,t:"Hello World!",s:5g)
   * - SYMBOL
     - id (ogr-sym-0 to ogr-sym-9, and filenames for raster symbols); color (c); size (s)
     - | SYMBOL(c:#00FF00,id:"ogr- sym-3",s:10)
       | SYMBOL(c:#00000080,id:"a_symbol.png")

Alpha values are supported for colors to control the opacity. If not
specified, for BRUSH, it is set at 50% opaque.

For SYMBOL with a bitmap name, only the alpha value of the color
specified with 'c' is taken into account.

A font name starting with "Times" or containing the string "Serif" (case
sensitive) will be treated as Times. A font name starting with "Courier"
or containing the string "Mono" (case sensitive) will be treated as
Courier. All other font names will be treated as Helvetica.

See Also
--------

-  :ref:`PDF raster <raster.pdf>` documentation page
-  :ref:`ogr_feature_style`
