.. _raster.lcp:

================================================================================
LCP -- FARSITE v.4 LCP Format
================================================================================

.. shortname:: LCP

.. built_in_by_default::

FARSITE v. 4 landscape file (LCP) is a multi-band raster format used by
wildland fire behavior and fire effect simulation models such as
FARSITE, FLAMMAP, and FBAT (`www.fire.org <http://www.fire.org>`__). The
bands of an LCP file store data describing terrain, tree canopy, and
surface fuel. The `LANDFIRE Data Distribrution
Site <https://landfire.cr.usgs.gov/viewer/>`__ distributes data in LCP
format, and programs such as FARSITE and
`LFDAT <http://www.landfire.gov/datatool.php>`__ can create LCP files
from a set of input rasters.

An LCP file (.lcp) is basically a raw format with a 7,316-byte header
described below. The data type for all bands is 16-bit signed integer.
Bands are interleaved by pixel. Five bands are required: elevation,
slope, aspect, fuel model, and tree canopy cover. Crown fuel bands
(canopy height, canopy base height, canopy bulk density), and surface
fuel bands (duff, coarse woody debris) are optional.

The LCP driver reads the linear unit, cell size, and extent, but the LCP
file does not specify the projection. UTM projections are typical, but
other projections are possible.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_georeferencing::

.. supports_virtualio::

Metadata
--------

The GDAL LCP driver reports dataset- and band-level metadata:

Dataset
~~~~~~~

   | LATITUDE: Latitude of the dataset, negative for southern hemisphere
   | LINEAR_UNIT: Feet or meters
   | DESCRIPTION: LCP file description

Band
~~~~

   | <band>_UNIT or <band>_OPTION: units or options code for the band
   | <band>_UNIT_NAME or <band>_OPTION_DESC: descriptive name of
     units/options
   | <band>_MIN: minimum value
   | <band>_MAX: maximum value
   | <band>_NUM_CLASSES: number of classes, -1 if > 100
   | <band>_VALUES: comma-delimited list of class values (fuel model
     band only)
   | <band>_FILE: original input raster file name for the band

**Note:** The LCP driver derives from the RawDataset helper class
declared in gdal/frmts/raw. It should be implemented as
gdal/frmts/raw/lcpdataset.cpp.

Creation Options
----------------

The LCP driver supports CreateCopy() and metadata values can be set via
creation options. Below is a list of options with default values listed
first.

**ELEVATION_UNIT=[METERS/FEET]**: Vertical unit for elevation band.

**SLOPE_UNIT=[DEGREES/PERCENT]**

**ASPECT_UNIT=[AZIMUTH_DEGREES/GRASS_CATEGORIES/GRASS_DEGREES]**

**FUEL_MODEL_OPTION=[NO_CUSTOM_AND_NO_FILE/CUSTOM_AND_NO_FILE/
NO_CUSTOM_AND_FILE/CUSTOM_AND_FILE]**: Specify whether or not custom
fuel models are used, and if a custom fuel model file is present.

**CANOPY_COV_UNIT=[PERCENT/CATEGORIES]**

**CANOPY_HT_UNIT=[METERS_X_10/FEET/METERS/FEET_X_10]**

**CBH_UNIT=[METERS_X_10/METERS/FEET/FEET_X_10]**

**CBD_UNIT=[KG_PER_CUBIC_METER_X_100/POUND_PER_CUBIC_FOOT/
KG_PER_CUBIC_METER/POUND_PER_CUBIC_FOOT_X_1000/TONS_PER_ACRE_X_100]**

**DUFF_UNIT=[MG_PER_HECTARE_X_10/TONS_PER_ACRE_X_10]**

**CALCULATE_STATS=[YES/NO]**: Calculate and write the min/max for each
band and write the appropriate flags and values in the header. This is
mostly a legacy feature used for creating legends.

**CLASSIFY_DATA=[YES/NO]**: Classify the data into 100 unique values or
less and write and write the appropriate flags and values in the header.
This is mostly a legacy feature used for creating legends.

**LINEAR_UNIT=[SET_FROM_SRS/METER/FOOT/KILOMETER]**: Set the linear
unit, overriding (if it can be calculated) the value in the associated
spatial reference. If no spatial reference is available, it defaults to
METER.

**LATITUDE=[-90-90]**: Override the latitude from the spatial reference.
If no spatial reference is available, this should be set, otherwise
creation will fail.

**DESCRIPTION=[...]**: A short description(less than 512 characters) of
the dataset

Creation options that are units of linear measure are fairly lenient.
METERS=METER and FOOT=FEET for the most part.

**Note:** CreateCopy does not scale or change any data. By setting the
units for various bands, it is assumed that the values are in the
specified units.

**LCP header format:**

============== ================ ========== ================ =================================================================================================================================================================================================
**Start byte** **No. of bytes** **Format** **Name**         **Description**
0              4                long       crown fuels      20 if no crown fuels, 21 if crown fuels exist (crown fuels = canopy height, canopy base height, canopy bulk density)
4              4                long       ground fuels     20 if no ground fuels, 21 if ground fuels exist (ground fuels = duff loading, coarse woody)
8              4                long       latitude         latitude (negative for southern hemisphere)
12             8                double     loeast           offset to preserve coordinate precision (legacy from 16-bit OS days)
20             8                double     hieast           offset to preserve coordinate precision (legacy from 16-bit OS days)
28             8                double     lonorth          offset to preserve coordinate precision (legacy from 16-bit OS days)
36             8                double     hinorth          offset to preserve coordinate precision (legacy from 16-bit OS days)
44             4                long       loelev           minimum elevation
48             4                long       hielev           maximum elevation
52             4                long       numelev          number of elevation classes, -1 if > 100
56             400              long       elevation values list of elevation values as longs
456            4                long       loslope          minimum slope
460            4                long       hislope          maximum slope
464            4                long       numslope         number of slope classes, -1 if > 100
468            400              long       slope values     list of slope values as longs
868            4                long       loaspect         minimum aspect
872            4                long       hiaspect         maximum aspect
876            4                long       numaspects       number of aspect classes, -1 if > 100
880            400              long       aspect values    list of aspect values as longs
1280           4                long       lofuel           minimum fuel model value
1284           4                long       hifuel           maximum fuel model value
1288           4                long       numfuel          number of fuel models -1 if > 100
1292           400              long       fuel values      list of fuel model values as longs
1692           4                long       locover          minimum canopy cover
1696           4                long       hicover          maximum canopy cover
1700           4                long       numcover         number of canopy cover classes, -1 if > 100
1704           400              long       cover values     list of canopy cover values as longs
2104           4                long       loheight         minimum canopy height
2108           4                long       hiheight         maximum canopy height
2112           4                long       numheight        number of canopy height classes, -1 if > 100
2116           400              long       height values    list of canopy height values as longs
2516           4                long       lobase           minimum canopy base height
2520           4                long       hibase           maximum canopy base height
2524           4                long       numbase          number of canopy base height classes, -1 if > 100
2528           400              long       base values      list of canopy base height values as longs
2928           4                long       lodensity        minimum canopy bulk density
2932           4                long       hidensity        maximum canopy bulk density
2936           4                long       numdensity       number of canopy bulk density classes, -1 if >100
2940           400              long       density values   list of canopy bulk density values as longs
3340           4                long       loduff           minimum duff
3344           4                long       hiduff           maximum duff
3348           4                long       numduff          number of duff classes, -1 if > 100
3352           400              long       duff values      list of duff values as longs
3752           4                long       lowoody          minimum coarse woody
3756           4                long       hiwoody          maximum coarse woody
3760           4                long       numwoodies       number of coarse woody classes, -1 if > 100
3764           400              long       woody values     list of coarse woody values as longs
4164           4                long       numeast          number of raster columns
4168           4                long       numnorth         number of raster rows
4172           8                double     EastUtm          max X
4180           8                double     WestUtm          min X
4188           8                double     NorthUtm         max Y
4196           8                double     SouthUtm         min Y
4204           4                long       GridUnits        linear unit: 0 = meters, 1 = feet, 2 = kilometers
4208           8                double     XResol           cell size width in GridUnits
4216           8                double     YResol           cell size height in GridUnits
4224           2                short      EUnits           elevation units: 0 = meters, 1 = feet
4226           2                short      SUnits           slope units: 0 = degrees, 1 = percent
4228           2                short      AUnits           aspect units: 0 = Grass categories, 1 = Grass degrees, 2 = azimuth degrees
4230           2                short      FOptions         fuel model options: 0 = no custom models AND no conversion file, 1 = custom models BUT no conversion file, 2 = no custom models BUT conversion file, 3 = custom models AND conversion file needed
4232           2                short      CUnits           canopy cover units: 0 = categories (0-4), 1 = percent
4234           2                short      HUnits           canopy height units: 1 = meters, 2 = feet, 3 = m x 10, 4 = ft x 10
4236           2                short      BUnits           canopy base height units: 1 = meters, 2 = feet, 3 = m x 10, 4 = ft x 10
4238           2                short      PUnits           canopy bulk density units: 1 = kg/m^3, 2 = lb/ft^3, 3 = kg/m^3 x 100, 4 = lb/ft^3 x 1000
4240           2                short      DUnits           duff units: 1 = Mg/ha x 10, 2 = t/ac x 10
4242           2                short      WOptions         coarse woody options (1 if coarse woody band is present)
4244           256              char[]     ElevFile         elevation file name
4500           256              char[]     SlopeFile        slope file name
4756           256              char[]     AspectFile       aspect file name
5012           256              char[]     FuelFile         fuel model file name
5268           256              char[]     CoverFile        canopy cover file name
5524           256              char[]     HeightFile       canopy height file name
5780           256              char[]     BaseFile         canopy base file name
6036           256              char[]     DensityFile      canopy bulk density file name
6292           256              char[]     DuffFile         duff file name
6548           256              char[]     WoodyFile        coarse woody file name
6804           512              char[]     Description      LCP file description
============== ================ ========== ================ =================================================================================================================================================================================================

*Chris Toney, 2009-02-14*
