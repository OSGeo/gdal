.. _raster.nsidcbin:

================================================================================
NSIDCbin -- National Snow and Ice Data Centre Sea Ice Concentrations
================================================================================

.. shortname:: NSIDCbin

.. built_in_by_default::

.. versionadded:: 3.7


Supported by GDAL for read access. This format is a raw binary format for the
Nimbus-7 SMMR and DMSP SSM/I-SSMIS Passive Microwave Data sea ice
concentrations. There are daily and monthly maps in the north and south
hemispheres supported by this driver.

Support includes an affine georeferencing transform, and projection - these are
both 25000m resolution polar stereographic grids centred on the north and south
pole respectively. Metadata from the file including julian day and year are
recorded.

This driver is implemented based on the NSIDC documentation in the `User Guide <https://nsidc.org/data/nsidc-0051>`__.

Band values are Byte, sea ice concentration (fractional coverage scaled by 250).

The dataset band implements GetScale() which will convert the values from 0,255
to 0.0,102.0 by multiplying by 0.4. Unscaled values above 250 have
specific meanings, 251 is Circular mask used in the Arctic, 252 is Unused, 253
is Coastlines, 254 is Superimposed land mask, 255 is Missing data.

NOTE: Implemented as :source_file:`frmts/raw/nsidcbindataset.cpp`.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Example
--------

For example, we want to read monthly data from September 2019, with data from this source (requires authentication).

<https://n5eil01u.ecs.nsidc.org/PM/NSIDC-0051.001/2018.09.01/nt_201809_f17_v1.1_s.bin>


::


   gdalinfo nt_201809_f17_v1.1_s.bin
