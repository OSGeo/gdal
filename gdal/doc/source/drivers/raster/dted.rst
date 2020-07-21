.. _raster.dted:

================================================================================
DTED -- Military Elevation Data
================================================================================

.. shortname:: DTED

.. built_in_by_default::

GDAL supports DTED Levels 0, 1, and 2 elevation data for read access.
Elevation data is returned as 16 bit signed integer. Appropriate
projection and georeferencing information is also returned. A variety of
header fields are returned dataset level metadata.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_georeferencing::

.. supports_virtualio::

Read Issues
-----------

Read speed
~~~~~~~~~~

Elevation data in DTED files are organized per columns. This data
organization doesn't fit very well with some scanline oriented
algorithms and can cause slowdowns, especially for DTED Level 2
datasets. By defining GDAL_DTED_SINGLE_BLOCK=TRUE, a whole DTED dataset
will be considered as a single block. The first access to the file will
be slow, but further accesses will be much quicker. Only use that option
if you need to do processing on a whole file.

Georeferencing Issues
~~~~~~~~~~~~~~~~~~~~~

| The DTED specification
  (`MIL-PRF-89020B <http://earth-info.nga.mil/publications/specs/printed/89020B/89020B.pdf>`__)
  states that *horizontal datum shall be the World Geodetic System (WGS
  84)*. The vertical datum is defined as EGM96, or EPSG:5773. However,
  there are still people using old data files georeferenced in WGS 72. A
  header field indicates the horizontal datum code, so we can detect and
  handle this situation.

-  If the horizontal datum specified in the DTED file is WGS84, the DTED
   driver will report WGS 84 as SRS.
-  If the horizontal datum specified in the DTED file is WGS72, the DTED
   driver will report WGS 72 as SRS and issue a warning.
-  If the horizontal datum specified in the DTED file is neither WGS84
   nor WGS72, the DTED driver will report WGS 84 as SRS and issue a
   warning.

Configuration options
~~~~~~~~~~~~~~~~~~~~~

This paragraph lists the configuration options that can be set to alter
the default behavior of the DTED driver.

-  REPORT_COMPD_CS: (GDAL >= 2.2.2). Can be set to TRUE to avoid
   stripping the vertical CS of compound CS when reading the SRS of a
   file. Default value : FALSE



Checksum Issues
~~~~~~~~~~~~~~~

The default behavior of the DTED driver is to ignore the checksum while
reading data from the files. However, you may specify the environment
variable ``DTED_VERIFY_CHECKSUM=YES`` if you want the checksums to be
verified. In some cases, the checksum written in the DTED file is wrong
(the data producer did a wrong job). This will be reported as a warning.
If the checksum written in the DTED file and the checksum computed from
the data do not match, an error will be issued.

Creation Issues
---------------

The DTED driver does support creating new files, but the input data must
be exactly formatted as a Level 0, 1 or 2 cell. That is the size, and
bounds must be appropriate for a cell.


GeoTransform
------------

The ``DTED_APPLY_PIXEL_IS_POINT=TRUE`` environment variable can be set (GDAL >=
3.1) to apply a pixel-is-point interpretation to the data when reading
the geotransform.

See Also
--------

-  Implemented as ``gdal/frmts/dted/dteddataset.cpp``.
