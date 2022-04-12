.. _raster.nsidcbin:

================================================================================
NSIDCbin -- National Snow and Ice Data Centre Sea Ice Concentrations
================================================================================

.. shortname:: NSIDCbin

.. built_in_by_default::


Supported by GDAL for read access. This format is a raw binary format for the
Nimbus-7 SMMR and DMSP SSM/I-SSMIS Passive Microwave Data sea ice
concentrations. There are daily and monthly maps in the north and south
hemispheres supported by this driver.

Support includes an affine georeferencing transform, and projection - these are
both 25000m resolution polar stereographic grids centred on the north and south
pole respectively. Metadata from the file including julian day and year are
recorded.

This driver is implemented based on the NSIDC documentation in the User Guide at
<https://nsidc.org/data/nsidc-0051>.

Band values are Byte, sea ice concentration (fractional coverage scaled by 250).

The dataset band implements GetScale() which will convert the values from 0,255
to 0.0,102.0 by multiplying by 0.4. Unscaled values above 250 have
specific meanings, 251 is Circular mask used in the Arctic, 252 is Unused, 253
is Coastlines, 254 is Superimposed land mask, 255 is Missing data.

NOTE: Implemented as ``gdal/frmts/raw/nsidcdbinataset.cpp``.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Example
--------

For example, we want to read monthly data from September 2019:

::

   $ gdalinfo nt_201809_f17_v1.1_s.bin
Driver: NSIDCbin/NSIDCbin
Files: nt_201809_f17_v1.1_s.bin
Size is 316, 332
Coordinate System is:
PROJCRS["WGS 84 / NSIDC Sea Ice Polar Stereographic South",
    BASEGEOGCRS["WGS 84",
        DATUM["World Geodetic System 1984",
            ELLIPSOID["WGS 84",6378137,298.257223563,
                LENGTHUNIT["metre",1]]],
        PRIMEM["Greenwich",0,
            ANGLEUNIT["degree",0.0174532925199433]],
        ID["EPSG",4326]],
    CONVERSION["unnamed",
        METHOD["Polar Stereographic (variant B)",
            ID["EPSG",9829]],
        PARAMETER["Latitude of standard parallel",-70,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8832]],
        PARAMETER["Longitude of origin",0,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8833]],
        PARAMETER["False easting",0,
            LENGTHUNIT["metre",1],
            ID["EPSG",8806]],
        PARAMETER["False northing",0,
            LENGTHUNIT["metre",1],
            ID["EPSG",8807]]],
    CS[Cartesian,2],
        AXIS["easting",north,
            ORDER[1],
            LENGTHUNIT["metre",1]],
        AXIS["northing",north,
            ORDER[2],
            LENGTHUNIT["metre",1]],
    ID["EPSG",3976]]
Data axis to CRS axis mapping: 1,2
Origin = (-3950000.000000000000000,4350000.000000000000000)
Pixel Size = (25000.000000000000000,-25000.000000000000000)
Metadata:
  DATA_DESCRIPTORS=17 cn
  DATA_INFORMATION=ANTARCTIC  SSMISONSSMIGRID CON Coast253Pole251Land254      02/11/2019
  FILENAME=nt_201809_f17_v01_s
  IMAGE_TITLE=ANTARCTIC SSMISS TOTAL ICE CONCENTRATION       DMSP  F17             09/2018
  INSTRUMENT=SSMIS
  JULIAN_DAY=244
  YEAR=2018
Corner Coordinates:
Upper Left  (-3950000.000, 4350000.000) ( 42d14'27.21"W, 39d13'47.79"S)
Lower Left  (-3950000.000,-3950000.000) (135d 0' 0.00"W, 41d26'45.74"S)
Upper Right ( 3950000.000, 4350000.000) ( 42d14'27.21"E, 39d13'47.79"S)
Lower Right ( 3950000.000,-3950000.000) (135d 0' 0.00"E, 41d26'45.74"S)
Center      (       0.000,  200000.000) (  0d 0' 0.01"E, 88d 9'14.03"S)
Band 1 Block=316x1 Type=Byte, ColorInterp=Undefined

