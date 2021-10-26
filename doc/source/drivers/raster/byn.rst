.. _raster.byn:

================================================================================
BYN - Natural Resources Canada's Geoid file format (.byn)
================================================================================

.. shortname:: BYN

.. built_in_by_default:: 

Files with extension ".byn" have a binary format. The format includes
two sections which are the Header and the Data. The data are stored by
rows starting from the north. Each row is stored from the west to the
east. The data are either short (2 bytes) or standard (4 bytes)
integers. The size of the bytes is defined in the header.

The total size of the file is 80 bytes + (Row x Column x (2 or 4) bytes)
where Row is the number of rows in the grid and Column is the number of
columns in the grid. Row and Column can be calculated by these two
equations:

Row = (North Boundary - South Boundary) / (NS Spacing) + 1

Column = (East Boundary - West Boundary) / (EW Spacing) + 1

The ".byn" files may contain undefined data. Depending if the data are
stored as 2-byte or 4-byte integers, the undefined data are expressed
the following way:

4-byte data (Standard integer): 9999.0 \* Factor, the Factor is given in
the header

2 byte data (Short integer): 32767

Most of the parameters in the ".byn" header can be read by clicking the
“Information” icon in software GPS-H.

NOTE: Files with extension ".err" are also in the ".byn" format. An
".err" file usually contains the error estimates of the ".byn" file of
the same name (e.g., CGG2013n83.byn and CGG2013n83.err). The ".err" file
will have variable Data equal to 1 or 3.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Factor
------

When translating from or into BYN file to or from another formats the
scale will affect the result profoundly.

Translating to a format that supports Scale (GTIFF for example) will
maintain the data type and thescale information. The pixel values are
perfectly preserved.

Translating to a format that does not support Scale will maintain the
data type but without the Scale, meaning loss of information on every
pixels.

The solution to the problem above is to use "-unscale" and "-ot Float32"
when using gdal_translate or GDAL API. That will produce a dataset
without scale but with the correct pixel information. Ex.:

gdal_translate CGG2013an83.err test2.tif -unscale -ot Float32

NOTE: The BYN header variable **Factor** is the inverse of GDAL
**Scale**. (Scale = 1.0 / Factor).

See Also
--------

-  Implemented as ``gdal/frmts/raw/byndataset.{h,cpp}``.
-  `www.nrcan.gc.ca <https://www.nrcan.gc.ca>`__
