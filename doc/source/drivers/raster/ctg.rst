.. _raster.ctg:

================================================================================
CTG -- USGS LULC Composite Theme Grid
================================================================================

.. shortname:: CTG

.. built_in_by_default::

This driver can read USGS Land Use and Land Cover (LULC) grids encoded
in the Character Composite Theme Grid (CTG) format. Each file is
reported as a 6-band dataset of type Int32. The meaning of each band is
the following one :

#. Land Use and Land Cover Code
#. Political units Code
#. Census county subdivisions and SMSA tracts Code
#. Hydrologic units Code
#. Federal land ownership Code
#. State land ownership Code

Those files are typically named grid_cell.gz, grid_cell1.gz or
grid_cell2.gz on the USGS site.

-  `Land Use and Land Cover Digital Data (Data Users Guide
   4) <http://edc2.usgs.gov/geodata/LULC/LULCDataUsersGuide.pdf>`__ -
   PDF version from USGS
-  `Land Use and Land Cover Digital Data (Data Users Guide
   4) <http://www.vterrain.org/Culture/LULC/Data_Users_Guide_4.html>`__
   - HTML version converted by Ben Discoe
-  `USGS LULC data at 250K and
   100K <http://edcftp.cr.usgs.gov/pub/data/LULC>`__

NOTE: Implemented as ``gdal/frmts/ctg/ctgdataset.cpp``.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

