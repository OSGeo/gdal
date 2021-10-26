.. _raster.Idrisi:

================================================================================
RST -- Idrisi Raster Format
================================================================================

.. shortname:: RST

.. built_in_by_default::

This format is basically a raw one. There is just one band per files,
except in the RGB24 data type where the Red, Green and Blue bands are
store interleaved by pixels in the order Blue, Green and Red. The others
data type are unsigned 8 bits integer with values from 0 to 255 or
signed 16 bits integer with values from -32.768 to 32.767 or 32 bits
single precision floating point.32 bits. The description of the file is
stored in a accompanying text file, extension RDC.

The RDC image description file doesn't include color table, or detailed
geographic referencing information. The color table if present can be
obtained by another accompanying file using the same base name as the
RST file and SMP as extension.

For geographical referencing identification, the RDC file contains
information that points to a file that holds the geographic reference
details. Those files uses extension REF and  resides in the same folder
as the RST image or more likely in the Idrisi installation folders.

Therefore the presence or absence of the Idrisi software in the running
operation system will determine the way that this driver will work. By
setting the environment variable IDRISIDIR pointing to the Idrisi main
installation folder will enable GDAL to find more detailed information
about geographical reference and projection in the REF files.

Note that the RST driver recognizes the name convention used in Idrisi
for UTM and State Plane geographic reference so it doesn't need to
access the REF files. That is the case for RDC file that specify
"utm-30n" or "spc87ma1" in the "ref. system" field. Note that exporting
to RST in any other geographical reference system will generate a
suggested REF content in the comment section of the RDC file.

-  ".rst" the raw image file
-  ".rdc" the description file
-  ".smp" the color table file
-  ".ref" the geographical reference file

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

See Also
--------

-  Implemented as ``gdal/frmts/idrisi/IdrisiDataset.cpp``.
-  `www.idrisi.com <http://www.idrisi.com>`__
