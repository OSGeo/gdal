.. _raster.ers:

================================================================================
ERS -- ERMapper .ERS
================================================================================

.. shortname:: ERS

.. built_in_by_default::

GDAL supports reading and writing raster files with .ers header files,
with some limitations. The .ers ascii format is used by ERMapper for
labeling raw data files, as well as for providing extended metadata, and
georeferencing for some other file formats. The .ERS format, or variants
are also used to hold descriptions of ERMapper algorithms, but these are
not supported by GDAL.

The PROJ, DATUM and UNITS values found in the
ERS header are reported in the ERS metadata domain.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Creation Issues
---------------

Creation Options:

-  .. co:: PIXELTYPE

      By setting this to SIGNEDBYTE, a new Byte file
      can be forced to be written as signed byte.
      Starting with GDAL 3.7, this option is deprecated and Int8 should rather
      be used.

-  .. co:: PROJ

      Name of the ERS projection string to
      use. Common examples are NUTM11, or GEODETIC. If defined, this will
      override the value computed by SetProjection() or SetGCPs().

-  .. co:: DATUM

      Name of the ERS datum string to use.
      Common examples are WGS84 or NAD83. If defined, this will override
      the value computed by SetProjection() or SetGCPs().

-  .. co:: UNITS
      :choices: METERS, FEET
      :default: METERS

      Name of the ERS projection units to use : METERS (default) or FEET (us-foot). If defined, this will
      override the value computed by SetProjection() or SetGCPs().

See Also
--------

-  Implemented as :source_file:`frmts/ers/ersdataset.cpp`.
