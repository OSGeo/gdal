.. _raster.tab:

================================================================================
TAB -- MapInfo TAB File
================================================================================

A TAB file that georefences a raster image is a plain ASCII text file.
An example of the format is:

.. code-block::

    !table
    !version 300
    !charset WindowsLatin1

    Definition Table
      File "TabFileName.gif"
      Type "RASTER"
      (290000,90000)  (0,4000) Label "Pt 1",
      (300000,90000)  (4000,4000) Label "Pt 2",
      (300000,100000) (4000,0) Label "Pt 3",
      (290000,100000) (0,0) Label "Pt 4"
      CoordSys Earth Projection 8, 79, "m", -2, 49, 0.9996012717, 400000, -100000
      Units "m"
      RasterStyle 4 1
      RasterStyle 7 16777215
      RasterStyle 8 127
    begin_metadata
      "\OptionalCustomKey" = "Some metadata value"
    end_metadata

The file starts with a format header.  This is followed by the definition table
which consists of at least the following lines in order:

   #. The path to the raster image file (relative to the TAB file).
   #. The type of the imag, this must be RASTER for a raster TAB file.
   #. Ground control point (real world coordinates to pixel coordinates) of bottom-left.
   #. Ground control point (real world coordinates to pixel coordinates) of bottom-right.
   #. Ground control point (real world coordinates to pixel coordinates) of top-right.
   #. Ground control point (real world coordinates to pixel coordinates) of top-left.
   #. Coordinate system line defining the Transverse Mercator projection
   #. Units usually degrees or m
   #. Subsequent lines may be present in the TAB file but are ignored by GDAL.

It is also possible to only define three ground control points instead for example:

.. code-block::

    !table
    !version 300
    !charset WindowsLatin1

    Definition Table
      File "TabFileName.png"
      Type "RASTER"
      (220000,520000) (0,0) Label "Pt 1",
      (420000,520000) (2000,0) Label "Pt 2",
      (420000,260000) (2000,2600) Label "Pt 3"
      CoordSys Earth Projection 8, 79, "m", -2, 49, 0.9996012717, 400000, -100000
      Units "m"

The coordinate system line has the form:

.. code-block::

    CoordSys Earth Projection <type (integer)>, <reference datum (integer)>, <unit name>,
        <origin longitude>, <origin longitude>, <scale/distortion factor>,
        <false easting>, <false northing>

Typically the coordinate system will be chosen from a list rather than constructed manually.
You can construct a TAB file simply by using your favorite text editor.

A MapInfo TAB file usually has suffix ``.tab``.
