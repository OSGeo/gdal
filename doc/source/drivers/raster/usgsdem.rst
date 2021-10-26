.. _raster.usgsdem:

================================================================================
USGSDEM -- USGS ASCII DEM (and CDED)
================================================================================

.. shortname:: USGSDEM

.. built_in_by_default::

GDAL includes support for reading USGS ASCII DEM files. This is the
traditional format used by USGS before being replaced by SDTS, and is
the format used for CDED DEM data products from the Canada. Most popular
variations on USGS DEM files should be supported, including correct
recognition of coordinate system, and georeferenced positioning.

The 7.5 minute (UTM grid) USGS DEM files will generally have regions of
missing data around the edges, and these are properly marked with a
nodata value. Elevation values in USGS DEM files may be in meters or
feet, and this will be indicated by the return value of
GDALRasterBand::GetUnitType() (either "m" or "ft").

Note that USGS DEM files are represented as one big tile. This may cause
cache thrashing problems if the GDAL tile cache size is small. It will
also result in a substantial delay when the first pixel is read as the
whole file will be ingested.

Some of the code for implementing usgsdemdataset.cpp was derived from
VTP code by Ben Discoe. See the `Virtual
Terrain <http://www.vterrain.org/>`__ project for more information on
VTP.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Creation Issues
---------------

GDAL supports export of geographic (and UTM) USGS DEM and CDED data
files, including the ability to generate CDED 2.0 50K products to
Canadian federal government specifications.

Input data must already be sampled in a geographic or UTM coordinate
system. By default the entire area of the input file will be output, but
for CDED50K products the output file will be sampled at the production
specified resolution and on product tile boundaries.

If the input file has appropriate coordinate system information set,
export to specific product formats can take input in different
coordinate systems (i.e. from Albers projection to NAD83 geographic for
CDED50K production).

Creation Options:

-  **PRODUCT=DEFAULT/CDED50K**: When CDED50K is specified, the output
   file will be forced to adhere to CDED 50K product specifications. The
   output will always be 1201x1201 and generally a 15 minute by 15
   minute tile (though wider in longitude in far north areas).
-  **TOPLEFT=long,lat**: For CDED50K products, this is used to specify
   the top left corner of the tile to be generated. It should be on a 15
   minute boundary and can be given in decimal degrees or degrees and
   minutes (eg. TOPLEFT=117d15w,52d30n).
-  **RESAMPLE=Nearest/Bilinear/Cubic/CubicSpline**: Set the resampling
   kernel used for resampling the data to the target grid. Only has an
   effect when particular products like CDED50K are being produced.
   Defaults to Bilinear.
-  **DEMLevelCode=integer** DEM Level (1, 2 or 3 if set). Defaults to 1.
-  **DataSpecVersion=integer** :Data and Specification version/revision
   (eg. 1020)
-  **PRODUCER=text**: Up to 60 characters to be put into the producer
   field of the generated file .
-  **OriginCode=text**: Up to 4 characters to be put into the origin
   code field of the generated file (YT for Yukon).
-  **ProcessCode=code**: One character to be put into the process code
   field of the generated file (8=ANUDEM, 9=FME, A=TopoGrid).
-  **TEMPLATE=filename**: For any output file, a template file can be
   specified. A number of fields (including the Data Producer) will be
   copied from the template file if provided, and are otherwise left
   blank.
-  **ZRESOLUTION=float**: DEM's store elevation information as positive
   integers, and these integers are scaled using the "z resolution." By
   default, this resolution is written as 1.0. However, you may specify
   a different resolution here, if you would like your integers to be
   scaled into floating point numbers.
-  **NTS=name**: NTS Mapsheet name, used to derive TOPLEFT. Only has an
   effect when particular products like CDED50K are being produced.
-  **INTERNALNAME=name**: Dataset name written into file header. Only
   has an effect when particular products like CDED50K are being
   produced.

Example: The following would generate a single CDED50K tile, extracting
from the larger DEM coverage yk_3arcsec for a tile with the top left
corner -117w,60n. The file yk_template.dem is used to set some product
fields including the Producer of Data, Process Code and Origin Code
fields.

::

   gdal_translate -of USGSDEM -co PRODUCT=CDED50K -co TEMPLATE=yk_template.dem \
                  -co TOPLEFT=-117w,60n yk_3arcsec 031a01_e.dem

--------------

NOTE: Implemented as ``gdal/frmts/usgsdem/usgsdemdataset.cpp``.

The USGS DEM reading code in GDAL was derived from the importer in the
`VTP <http://www.vterrain.org/>`__ software. The export capability was
developed with the financial support of the Yukon Department of
Environment.
