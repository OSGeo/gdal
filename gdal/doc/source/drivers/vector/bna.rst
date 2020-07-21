.. _vector.bna:

================================================================================
BNA - Atlas BNA
================================================================================

.. shortname:: BNA

.. built_in_by_default::

The BNA format is an ASCII exchange format for 2D vector data supported by many
software packages. It only contains geometry and a few identifiers per record.
Attributes must be stored into external files. It does not support any
coordinate system information.

OGR has support for BNA reading and writing.

The OGR driver supports reading and writing of all the BNA feature types:

- points
- polygons
- lines
- ellipses/circles

As the BNA format is lacking from a formal specification, there can be various
forms of BNA data files. The OGR driver does it best to parse BNA datasets and
supports single line or multi line record formats, records with 2, 3 or 4
identifiers, etc etc. If you have a BNA data file that cannot be parsed
correctly by the BNA driver, please report on the GDAL track system.

To be recognized as BNA, the file extension must be ".bna". When reading a BNA
file, the driver will scan it entirely to find out which layers are available.
If the file name is foo.bna, the layers will be named foo_points, foo_polygons,
foo_lines and foo_ellipses.

The BNA driver support reading of polygons with holes or lakes. It determines
what is a hole or a lake only from geometrical analysis (inclusion,
non-intersection tests) and ignores completely the notion of polygon winding
(whether the polygon edges are described clockwise or counter-clockwise). GDAL
must be built with GEOS enabled to make geometry test work. Polygons are
exposed as multipolygons in the OGR Simple Feature model.

Ellipses and circles are discretized as polygons with 360 points.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Creation Issues
---------------

On export all layers are written to a single BNA file. Update of existing files
is not currently supported.

If the output file already exits, the writing will not occur. You have to
delete the existing file first.

The BNA writer supports the following creation options (dataset options):

-  **LINEFORMAT**: By default when creating new .BNA files they are
   created with the line termination conventions of the local platform
   (CR/LF on win32 or LF on all other systems). This may be overridden
   through use of the LINEFORMAT layer creation option which may have a
   value of **CRLF** (DOS format) or **LF** (Unix format).
-  **MULTILINE**: By default, BNA files are created in the multi-line
   format (for each record, the first line contains the identifiers and
   the type/number of coordinates to follow. The following lines
   contains a pair of coordinates). This may be overridden through use
   of MULTILINE=\ **NO**.
-  **NB_IDS**: BNA records may contain from 2 to 4 identifiers per
   record. Some software packages only support a precise number of
   identifiers. You can override the default value (2) by a precise
   value : **2**, **3** or **4**, or **NB_SOURCE_FIELDS**.
   NB_SOURCE_FIELDS means that the output file will contains the same
   number of identifiers as the features written in (clamped between 2
   and 4).
-  **ELLIPSES_AS_ELLIPSES**: the BNA writer will try to recognize
   ellipses and circles when writing a polygon. This will only work if
   the feature has previously been read from a BNA file. As some
   software packages do not support ellipses/circles in BNA data file,
   it may be useful to tell the writer by specifying
   ELLIPSES_AS_ELLIPSES=\ **NO** not to export them as such, but keep
   them as polygons.
-  **NB_PAIRS_PER_LINE**: this option may be used to limit the number of
   coordinate pairs per line in multiline format.
-  **COORDINATE_PRECISION**: this option may be used to set the number
   of decimal for coordinates. Default value is 10.

VSI Virtual File System API support
-----------------------------------

The driver supports reading and writing to files managed by VSI Virtual
File System API, which include "regular" files, as well as files in the
/vsizip/ (read-write) , /vsigzip/ (read-write) , /vsicurl/ (read-only)
domains.

Writing to /dev/stdout or /vsistdout/ is also supported.

Example
-------

The ogrinfo utility can be used to dump the content of a BNA datafile :

.. code-block::

   ogrinfo -ro -al a_bna_file.bna

The ogr2ogr utility can be used to do BNA to BNA translation :

.. code-block::

   ogr2ogr -f BNA -dsco "NB_IDS=2" -dsco "ELLIPSES_AS_ELLIPSES=NO" output.bna input.bna

See Also
--------

-  `Description of the BNA file format <http://www.softwright.com/faq/support/boundary_file_bna_format.html>`__
-  `Another description of the BNA file format <http://64.145.236.125/forum/topic.asp?topic_id=1930&forum_id=1&Topic_Title=how+to+edit+*.bna+files%3F&forum_title=Surfer+Support&M=False>`__
-  `Archive of Census Related Products (ACRP) <http://sedac.ciesin.org/plue/cenguide.html>`__ : downloadable
   BNA datasets of boundary files based on TIGER 1992 files containing
   U.S. census geographies
