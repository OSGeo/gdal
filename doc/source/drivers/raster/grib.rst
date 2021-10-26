.. _raster.grib:

================================================================================
GRIB -- WMO General Regularly-distributed Information in Binary form
================================================================================

.. shortname:: GRIB

.. built_in_by_default::

GDAL supports GRIB1 (reading) and GRIB2 (reading and writing) format
raster data, with support for common coordinate system, georeferencing
and other metadata. GRIB format is commonly used for distribution of
Meteorological information, and is propagated by the World
Meteorological Organization.

The GDAL GRIB driver is based on a modified version of the degrib
application which is written primarily by Arthur Taylor of NOAA NWS NDFD
(MDL). The degrib application (and the GDAL GRIB driver) are built on
the g2clib grib decoding library written primarily by John Huddleston of
NOAA NWS NCEP.

GRIB2 files without projection on lon/lat grids have the peculiarity
of using longitudes in the [0,360] range and wrapping around the
antimeridian as opposed to the usual wrapping around the prime meridian
of other raster datasets. Starting with GDAL 3.4.0, when reading such
files, a transparent conversion of the longitudes to [-180,180] will be
performed and the data will be rewrapped around the prime meridian -
the split&swap mode. This behavior can be disabled by setting the
:decl_configoption:`GRIB_ADJUST_LONGITUDE_RANGE` configuration option to `NO`.

There are several encoding schemes for raster data in GRIB format. Most
common ones should be supported including PNG encoding. JPEG2000 encoded
GRIB files will generally be supported if GDAL is also built with
JPEG2000 support via one of the GDAL JPEG2000 drivers.

GRIB files may a be represented in GDAL as having many bands, with some
sets of bands representing a time sequence. GRIB bands are represented
as Float64 (double precision floating point) regardless of the actual
values. GRIB metadata is captured as per-band metadata and used to set
band descriptions, similar to this:

::

     Description = 100000[Pa] ISBL="Isobaric surface"
       GRIB_UNIT=[gpm]
       GRIB_COMMENT=Geopotential height [gpm]
       GRIB_ELEMENT=HGT
       GRIB_SHORT_NAME=100000-ISBL
       GRIB_REF_TIME=  1201100400 sec UTC
       GRIB_VALID_TIME=  1201104000 sec UTC
       GRIB_FORECAST_SECONDS=3600 sec

GRIB2 files may also include an extract of other metadata, such as the
`identification
section <http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_sect1.shtml>`__,
`product
definition <http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_sect4.shtml>`__
template number (GRIB_PDS_PDTN, octet 8-9), and the product definition
template values (GRIB_PDS_TEMPLATE_NUMBERS, octet 10+) as metadata like
this:

::

       GRIB_DISCIPLINE=0(Meteorological)
       GRIB_IDS=CENTER=7(US-NCEP) SUBCENTER=0 MASTER_TABLE=8 LOCAL_TABLE=1 SIGNF_REF_TIME=1(Start_of_Forecast) REF_TIME=2017-10-20T06:00:00Z PROD_STATUS=0(Operational) TYPE=1(Forecast)
       GRIB_PDS_PDTN=32
       GRIB_PDS_TEMPLATE_NUMBERS=5 7 2 0 0 0 0 0 1 0 0 0 0 1 0 31 1 29 67 140 2 0 0 238 217
       GRIB_PDS_TEMPLATE_ASSEMBLED_VALUES=5 7 2 0 0 0 0 1 0 1 31 285 17292 2 61145

GRIB_DISCIPLINE was added in GDAL 2.3.0 and is the
`Discipline <http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_table0-0.shtml>`__
field of the Section 0 of the message.

GRIB_IDS was added in GDAL 2.3.0 and is the `identification
section <http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_table0-0.shtml>`__
/ Section 1 of the message.

GRIB_PDS_TEMPLATE_ASSEMBLED_VALUES was added in GDAL 2.3.0, and use
template definitions to assemble several bytes that make a template item
into a 16 or 32 bit signed/unsigned integers, whereas
GRIB_PDS_TEMPLATE_NUMBERS expose raw bytes

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Configuration options
---------------------

This paragraph lists the configuration options that can be set to alter
the default behavior of the GRIB driver.

-  GRIB_NORMALIZE_UNITS=YES/NO : Default to YES. Can be
   set to NO to avoid gdal to normalize units to metric. By default
   (GRIB_NORMALIZE_UNITS=YES), temperatures are reported in degree
   Celsius (°C). With GRIB_NORMALIZE_UNITS=NO, they are reported in
   degree Kelvin (°K).

Open options
------------

-  **USE_IDX=YES/NO**: (From GDAL 3.4) Enable automatic reading
   of external wgrib2 external index files when available. GDAL
   will look for a `<GRIB>.idx` in the same place as the dataset.
   These files when combined with careful usage of the API or the
   CLI tools allow a GRIBv2 file to be opened without reading all
   the bands. In particular, this allows an orders of magnitude
   faster extraction of select bands from large GRIBv2 files on
   remote storage (like NOMADS on AWS S3).
   In order to avoid unncessary I/O only the text
   description of the bands should be accessed as accessing the
   metadata will require loading of the band header.
   gdal_translate is supported but gdalinfo is not.
   Default is YES.

GRIB2 write support
-------------------

GRIB2 write support is available since GDAL 2.3.0, through the
CreateCopy() / gdal_translate interface.

Each band of the input dataset is translated as a GRIB2 message, and all
of them are concatenated in a single file, conforming to the usual
practice.

The input dataset must be georeferenced, and the supported projections
are: Geographic Longitude/Latitude, Mercator 1SP/2SP, Transverse
Mercator, Polar Stereographic, Lambert Conformal Conic 1SP/2SP, Albers
Conic Equal Area and Lambert Azimuthal Equal Area.

A number of creation options are available as detailed in below
sections. Those creation options are valid for all bands. But it is
possible to override those global settings in a per-band way, by
defining creation options that use the same key and are prefixed by
BAND_X\_ where X is the band number between 1 and the total number of
bands. For example BAND_1_PDS_PDTN

Product identification and definition
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Users are strongly advised to provide necessary information to
appropriately fill the `Section 0 /
"Indicator" <http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_sect0.shtml>`__,
`Section 1 / "Identification
section" <http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_sect1.shtml>`__
and `Section 4 / "Product definition
section" <http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_sect4.shtml>`__
with the following creation options. Otherwise, GDAL will fill with
default values, but readers might have trouble exploiting GRIB2 datasets
generating with those defaults.

-  **DISCIPLINE**\ =integer: sets the Discipline field of Section 0.
   Valid values are given by `Table
   0.0 <http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_table0-0.shtml>`__:

   -  0: Meteorological Products. Default value
   -  1: Hydrological Products
   -  2: Land Surface Products
   -  3, 4: Space Products
   -  10: Oceanographic Product

-  **IDS**\ =string. String with different elements to fill the fields
   of the Section 1 / Identification section. The value of that string
   will typically be retrieved from the GRIB_IDS metadata item of an
   existing GRIB product. For example "IDS=CENTER=7(US-NCEP) SUBCENTER=0
   MASTER_TABLE=8 SIGNF_REF_TIME=1(Start_of_Forecast)
   REF_TIME=2017-10-20T06:00:00Z PROD_STATUS=0(Operational)
   TYPE=1(Forecast)". More formally, the format of the string is a list
   of KEY=VALUE items, with space separator. The accepted keys are
   CENTER, SUBCENTER, MASTER_TABLE, SIGNF_REF_TIME, REF_TIME,
   PROD_STATUS and TYPE. Only the numerical part of the value is taken
   into account (the precision between parenthesis will be ignored). It
   is possible to use both this IDS creation option and a specific
   IDS_xxx creation option that will override the potential
   corresponding xxx key of IDS. For example with the previous example,
   if both "IDS=CENTER=7(US-NCEP)..." and "IDS_CENTER=8" are define, the
   actual value used with be 8.
-  **IDS_CENTER**\ =integer. Identification of originating/generating
   center, according to `Table
   0 <http://www.nco.ncep.noaa.gov/pmb/docs/on388/table0.html>`__.
   Defaults to 255/Missing
-  **IDS_SUBCENTER**\ =integer. Identification of originating/generating
   center, according to `Table
   C <http://www.nco.ncep.noaa.gov/pmb/docs/on388/tablec.html>`__.
   Defaults to 65535/Missing
-  **IDS_MASTER_TABLE**\ =integer. GRIB master tables version number,
   according to `Table
   1.0 <http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_table1-0.shtml>`__.
   Defaults to 2
-  **IDS_SIGNF_REF_TIME**\ =integer. Significance of reference time,
   according to `Table
   1.2 <http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_table1-2.shtml>`__.
   Defaults to 0/Analysis
-  **IDS_REF_TIME**\ =datetime as YYYY-MM-DD[THH:MM:SSZ]. Reference
   time. Defaults to 1970-01-01T00:00:00Z
-  **IDS_PROD_STATUS**\ =integer. Production status of processed data,
   according to `Table
   1.3 <http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_table1-3.shtml>`__.
   Defaults to 255/Missing
-  **IDS_TYPE**\ =integer. Type of processed data, according to `Table
   1.4 <http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_table1-4.shtml>`__.
   Defaults to 255/Missing
-  **PDS_PDTN**\ =integer. Product definition template number, according
   to `Table
   4.0 <http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_table4-0.shtml>`__.
   Defaults to 0/Analysis or forecast at a horizontal level or in a
   horizontal layer at a point in time. If this default template number
   is used, and none of PDS_TEMPLATE_NUMBERS or
   PDS_TEMPLATE_ASSEMBLED_VALUES is specified, then a default template
   definition is also used, with most fields set to Missing.
-  **PDS_TEMPLATE_NUMBERS**\ =string. Product definition template raw
   numbers. This is a list of byte values (between 0 and 255 each),
   space separated. The number of values and their semantics depends on
   the template number specified by PDS_PDTN, and you have to consult
   the template structures pointed by `Table
   4.0 <http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_table4-0.shtml>`__.
   It might be easier to use the GRIB_PDS_TEMPLATE_NUMBERS reported by
   existing GRIB2 products as the value for this item. If the template
   structure is known by the reading side of the driver, an effort to
   validate the number of template numbers against the template
   structure is made (with warnings if more elements than needed are
   specified, and error if less are specified). It is also possible to
   define a template that is not or partially implemented by the reading
   side of the driver.
-  **PDS_TEMPLATE_ASSEMBLED_VALUES**\ =string. Product definition
   template assembled values. This is a list of values (with the range
   of signed/unsigned 1, 2 or 4-byte wide integers, depending on the
   item), space separated. The number of values and their semantics
   depends on the template number specified by PDS_PDTN, and you have to
   consult the template structures pointed by `Table
   4.0 <http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_table4-0.shtml>`__.
   It might be easier to use the GRIB_PDS_TEMPLATE_ASSEMBLED_VALUES
   reported by existing GRIB2 products as the value for this item.
   PDS_TEMPLATE_NUMBERS and PDS_TEMPLATE_ASSEMBLED_VALUES are exclusive.
   To use this creation option, the template structure must be known by
   the reading side of the driver.

Data encoding
~~~~~~~~~~~~~

In GRIB2, a number of data encoding schemes exist (see `Section 5 /
"Data representation
section" <http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_sect5.shtml>`__).
By default, GDAL will select an appropriate data encoding that will
preserve the range of input data. with the **DATA_ENCODING**, **NBITS**,
**DECIMAL_SCALE_FACTOR**, **JPEG200_DRIVER**, **COMPRESSION_RATIO** and
**SPATIAL_DIFFERENCING_ORDER** creation options.

Users can override those defaults with the following creation options
are:

-  **DATA_ENCODING**\ =AUTO / SIMPLE_PACKING / COMPLEX_PACKING /
   IEEE_FLOATING_POINT / PNG / JPEG2000: Choice of the `Data
   representation template number. Defaults to
   AUTO. <http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_table5-0.shtml>`__

   -  In AUTO mode, COMPLEX_PACKING is selected if input band has a
      nodata value. Otherwise if input band datatype is Float32 or
      Float64, IEEE_FLOATING_POINT is selected. Otherwise SIMPLE_PACKING
      is selected.
   -  `SIMPLE_PACKING <http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_temp5-0.shtml>`__:
      use integer representation internally, with offset and decimal
      and/or binary scaling. So can be used for any datatype.
   -  COMPLEX_PACKING: evolution of SIMPLE_PACKING with nodata handling.
      By default, a `non-spatial differencing encoding is
      used <http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_temp5-2.shtml>`__,
      but if SPATIAL_DIFFERENCING_ORDER=1 or 2, `complex packing with
      spatial
      differencing <http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_temp5-3.shtml>`__
      is used
   -  `IEEE_FLOATING_POINT <http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_temp5-4.shtml>`__:
      store values as IEEE-754 single or double precision numbers.
   -  `PNG <http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_temp5-41.shtml>`__:
      uses the same preparation steps as SIMPLE_PACKING but with PNG
      encoding of the integer values.
   -  `JPEG2000 <http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_temp5-40.shtml>`__:
      uses the same preparation steps as SIMPLE_PACKING but with
      JPEG2000 encoding of the integer values.

-  **NBITS**\ =integer between 1 to 31. Bit width for each sample value.
   Might be only loosely honored by some DATA_ENCODING. If not
   specified, the bit width is computed automatically from the range of
   input values for integral data types, or default to 8 for
   Float32/Float64.
-  **DECIMAL_SCALE_FACTOR**\ =integer_value. Input values are multiplied
   by 10^DECIMAL_SCALE_FACTOR before integer encoding (and automatically
   divised by this value at decoding, so this only affect precision).
   For example, if the type of the data is a temperature, with floating
   point data type, DECIMAL_SCALE_FACTOR=1 can be used to specify that
   the data has a precision of 1/10 of degree. The default is 0 (no
   premultiplication)
-  **SPATIAL_DIFFERENCING_ORDER**\ =0/1/2. Only used for
   COMPLEX_PACKING. Defines the order of the spatial differencing. 0
   means that the values are encoded independently, 1 means that the
   difference of consecutive values is encoded and 2 means that the
   difference of the difference of consecutive values is encoded.
   Defaults to 0
-  **COMPRESSION_RATIO**\ =integer_value between 1 and 100. Defaults to
   1 for lossless JPEG2000 encoding. Only used for JPEG2000 encoding. If
   a value greater than 1 is specified, lossy JPEG2000 compression is
   used. The value indicates the desired compression factor with
   respected to uncompressed data. For example a value of 10 means that
   the desired JPEG2000 codestream should be 10 times smaller than the
   corresponding uncompressed file (with NBITS bits per pixel).
-  **JPEG2000_DRIVER**\ =JP2KAK/JP2OPENJPEG/JPEG2000/JP2ECW (possible
   values depend on the actually available JPEG2000 driver in the GDAL
   build). To specify which JPEG2000 driver should be used. If not
   specified, drivers are searched in the order given in the
   enumeration.

Data units
~~~~~~~~~~

Internally GRIB stores values in the units of the international system
(ie Metric system). So temperatures must be stored as Kelvin degrees.
But on the reading side of the driver, fields with temperatures are
exposed in Celsius degrees (unless the GRIB_NORMALIZE_UNITS
configuration option is set to NO). For consistency, the writing side of
the driver also assumed that temperature (detected if the first value of
a product definition template, ie the *Parameter category* is
0=Temperature) values in the input dataset will be in Celsius degrees,
and will automatically offset them to Kelvin degrees. It is possible to
control that behavior by setting the **INPUT_UNIT** creation option to
C (for Celsius) or K (for Kelvin). The default is C.

GRIB2 to GRIB2 conversions
~~~~~~~~~~~~~~~~~~~~~~~~~~

If GRIB2 to GRIB2 translation is done with gdal_translate (or
CreateCopy()), the GRIB_DISCIPLINE, GRIB_IDS, GRIB_PDS_PDTN and
GRIB_PDS_TEMPLATE_NUMBERS metadata items of the bands of the source
dataset are used by default (unless creation options override them).

DECIMAL_SCALE_FACTOR and NBITS will also be attempted to be retrieved
from the GRIB special metadata domain.

Examples
~~~~~~~~

::

   gdal_translate in.tif out.grb2 -of GRIB \
       -co "IDS=CENTER=8(US-NWSTG) SIGNF_REF_TIME=1(Start_of_Forecast) REF_TIME=2008-02-21T17:00:00Z PROD_STATUS=0(Operational) TYPE=1(Forecast)" \
       -co "PDS_PDTN=8" \
       -co "PDS_TEMPLATE_ASSEMBLED_VALUES=0 5 2 0 0 255 255 1 43 1 0 0 255 -1 -2147483647 2008 2 23 12 0 0 1 0 3 255 1 12 1 0"

See Also:
---------

-  `NOAA NWS NDFD "degrib" GRIB2
   Decoder <https://www.weather.gov/mdl/degrib_archive>`__
-  `NOAA NWS NCEP g2clib grib decoding
   library <http://www.nco.ncep.noaa.gov/pmb/codes/GRIB2/>`__
-  `WMO GRIB1 Format
   Documents <http://www.wmo.int/pages/prog/www/WDM/Guides/Guide-binary-2.html>`__
-  `NCEP WMO GRIB2
   Documentation <http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/>`__

Credits
-------

Support for GRIB2 write capabilities has been funded by Meteorological
Service of Canada.
