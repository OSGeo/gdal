.. _raster.grass:

================================================================================
GRASS Raster Format
================================================================================

.. shortname:: GRASS

.. build_dependencies:: libgrass

GDAL optionally supports reading of existing GRASS GIS raster maps or
imagery groups, but not writing or export. The support for GRASS raster
format is determined when the library is configured, and requires
libgrass to be pre-installed (see Notes below).

GRASS raster maps/imagery groups can be selected in several ways.

#. The full path to the ``cellhd`` file can be specified. This is not a
   relative path, or at least it must contain all the path components
   within the GRASS database including the database root itself. The
   following example opens the raster map "elevation" within the GRASS
   mapset "PERMANENT" of the GRASS location "myloc" in the GRASS
   database located at ``/data/grassdb``.

   For example:

   ::

      gdalinfo /data/grassdb/myloc/PERMANENT/cellhd/elevation

#. The full path to the directory containing information about an
   imagery group (or the REF file within it) can be specified to refer
   to the whole group as a single dataset. The following examples do the
   same thing.

   For example:

   ::

      gdalinfo /data/grassdb/imagery/raw/group/testmff/REF
      gdalinfo /data/grassdb/imagery/raw/group/testmff

#. If there is a correct ``.grassrc7/rc`` (GRASS 7) setup file in the
   user's home directory then raster maps or imagery groups may be opened
   just by the cell or group name.
   This only works for raster maps or imagery groups in the
   current GRASS location and mapset as defined in the GRASS setup file.

The following features are supported by the GDAL/GRASS link.

-  Up to 256 entries from raster colormaps are read (0-255).
-  Compressed and uncompressed integer (CELL), floating point (FCELL)
   and double precision (DCELL) raster maps are all supported. Integer
   raster maps are classified with a band type of "Byte" if the 1-byte
   per pixel format is used, or "UInt16" if the two byte per pixel
   format is used. Otherwise integer raster maps are treated as
   "UInt32".
-  Georeferencing information is properly read from GRASS format.
-  An attempt is made to translate coordinate systems, but some
   conversions may be flawed, in particular in handling of datums and
   units.

Driver capabilities
-------------------

.. supports_georeferencing::

Notes on driver variations
--------------------------

The driver is able to use the GRASS GIS shared libraries directly
instead of using libgrass (not recommended due to potentially circular
dependencies). Currently both versions of the driver are available and
can be configured using ``--with-libgrass`` for the libgrass variant or
``--with-grass=<dir>`` for the GRASS GIS library based version. The
GRASS driver version currently does not support coordinate system
access, though it is hoped that will be corrected at some point.

See Also
--------

-  `GRASS GIS home page <https://grass.osgeo.org>`__
-  `libgrass page <https://web.archive.org/web/20130730111701/http://home.gdal.org/projects/grass/>`__
