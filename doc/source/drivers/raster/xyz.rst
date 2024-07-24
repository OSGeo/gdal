.. _raster.xyz:

================================================================================
XYZ -- ASCII Gridded XYZ
================================================================================

.. shortname:: XYZ

.. built_in_by_default::

GDAL supports reading and writing ASCII **gridded** XYZ raster datasets
(i.e. ungridded XYZ, LIDAR XYZ etc. must be opened by other means. See
the documentation of the :ref:`gdal_grid` utility).

Those datasets are ASCII files with (at least) 3 columns, each line
containing the X and Y coordinates of the center of the cell and the
value of the cell. (Note the XYZ driver only uses the first band of
the dataset. I.e., columns beyond the third are ignored.)

The spacing between each cell must be constant.

The following data organization are supported :

* Cells with same Y coordinates must be placed on consecutive
  lines. For a same Y coordinate value, the lines in the dataset must be
  organized by increasing X values. The value of the Y coordinate can
  increase or decrease however.

* or, starting with GDAL 3.2.1, cells with same X coordinates must be placed
  on consecutive lines. For a same X coordinate value, the columns must be
  organized by increasing or decreasing Y values. For that organization, no
  missing value is supported, and the whole dataset will be ingested into
  memory (thus the driver will limit to 100 million points).

The supported column separators are space, comma, semicolon and tabulations.

The driver tries to autodetect an header line and will look for 'x',
'lon' or 'east' names to detect the index of the X column, 'y', 'lat' or
'north' for the Y column and 'z', 'alt' or 'height' for the Z column. If
no header is present or one of the column could not be identified in the
header, the X, Y and Z columns (in that order) are assumed to be the
first 3 columns of each line. The open option :oo:`COLUMN_ORDER` overrides
these assumptions (except on 'AUTO').

The opening of a big dataset can be slow as the driver must scan the
whole file to determine the dataset size and spatial resolution. The
driver will autodetect the data type among Byte, Int16, Int32 or
Float32.

Open options
------------

|about-open-options|
This driver supports the following open options:

-  .. oo:: COLUMN_ORDER
     :choices: AUTO, XYZ, YXZ
     :since: 3.10
     :default: AUTO

     Specifies the order of the columns. It overrides the header.

Creation options
----------------

|about-creation-options|
This driver supports the following creation options:

-  .. co:: COLUMN_SEPARATOR
      :choices: <string>

      String used to
      separate the values of the X,Y and Z columns. Defaults to one space
      character

-  .. co:: ADD_HEADER_LINE
      :choices: YES, NO
      :default: NO

      Whether an header line must be written
      (content is X <col_sep> Y <col_sep> Z).

-  .. co:: SIGNIFICANT_DIGITS
      :choices: <integer>
      :default: 18

      Specifies the number of significant digits to output (%g format)

-  .. co:: DECIMAL_PRECISION
      :choices: <integer>

      Specifies the number
      of decimal places to output when writing floating-point numbers (%f
      format; alternative to :co:`SIGNIFICANT_DIGITS`).

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_georeferencing::

.. supports_virtualio::

See also
--------

-  Documentation of :ref:`gdal_grid`
