.. _raster.fast:

================================================================================
FAST -- EOSAT FAST Format
================================================================================

.. shortname:: FAST

.. built_in_by_default::

Supported reading from FAST-L7A format (Landsat TM data) and EOSAT Fast
Format Rev. C (IRS-1C/1D data). If you want to read other datasets in
this format (SPOT), write to me (Andrey Kiselev, dron@ak4719.spb.edu).
You should share data samples with me.

Datasets in FAST format represented by several files: one or more
administrative headers and one or more files with actual image data in
raw format. Administrative files contains different information about
scene parameters including filenames of images. You can read files with
administrative headers with any text viewer/editor, it is just plain
ASCII text.

This driver wants administrative file for input. Filenames of images
will be extracted and data will be imported, every file will be
interpreted as band.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Data
----

FAST-L7A
~~~~~~~~

FAST-L7A consists form several files: big ones with image data and three
small files with administrative information. You should give to driver
one of the administrative files:

-  L7fppprrr_rrrYYYYMMDD_HPN.FST: panchromatic band header file with 1
   band
-  L7fppprrr_rrrYYYYMMDD_HRF.FST: VNIR/ SWIR bands header file with 6
   bands
-  L7fppprrr_rrrYYYYMMDD_HTM.FST: thermal bands header file with 2 bands

All raw images corresponded to their administrative files will be
imported as GDAL bands.

From the \`\` `Level 1 Product Output Files Data Format Control
Book <http://ltpwww.gsfc.nasa.gov/IAS/pdfs/DFCB_V5_B2_R4.pdf>`__'':

``The file naming convention for the FAST-L7A product files is  L7fppprrr_rrrYYYYMMDD_AAA.FST  where  L7 = Landsat 7 mission  f = ETM+ format (1 or 2) (data not pertaining to a specific format defaults to 1)  ppp = starting path of the product  rrr_rrr = starting and ending rows of the product  YYYYMMDD = acquisition date of the image  AAA = file type: HPN = panchromatic band header file HRF = VNIR/ SWIR bands header file HTM = thermal bands header file B10 = band 1 B20 = band 2 B30 = band 3 B40 = band 4 B50 = band 5 B61 = band 6L B62 = band 6H B70 = band 7 B80 = band 8  FST = FAST file extension``

So you should give to driver one of the
``L7fppprrr_rrrYYYYMMDD_HPN.FST``, ``L7fppprrr_rrrYYYYMMDD_HRF.FST`` or
``L7fppprrr_rrrYYYYMMDD_HTM.FST`` files.

IRS-1C/1D
~~~~~~~~~

Fast Format REV. C does not contain band filenames in administrative
header. So we should guess band filenames, because different data
distributors name their files differently. Several naming schemes
hardcoded in GDAL's FAST driver. These are:

``<header>.<ext> <header>.1.<ext> <header>.2.<ext> ...``

or

``<header>.<ext> band1.<ext> band2.<ext> ...``

or

``<header>.<ext> band1.dat band2.dat ...``

or

``<header>.<ext> imagery1.<ext> imagery2.<ext> ...``

or

``<header>.<ext> imagery1.dat imagery2.dat ...``

in lower or upper case. Header file could be named arbitrarily. This
should cover majority of distributors fantasy in naming files. But if
you out of luck and your datasets named differently you should rename
them manually before importing data with GDAL.

GDAL also supports the logic for naming band files for datasets produced
by Euromap GmbH for IRS-1C/IRS-1D PAN, LISS3 and WIFS sensors. Their
filename logic is explained in the `Euromap Naming
Conventions <http://www.euromap.de/download/em_names.pdf>`__ document.

Georeference
------------

All USGS projections should be supported (namely UTM, LCC, PS, PC, TM,
OM, SOM). Contact me if you have troubles with proper projection
extraction.

Metadata
--------

Calibration coefficients for each band reported as metadata items.

-  **ACQUISITION_DATE**: First scene acquisition date in yyyyddmm
   format.
-  **SATELLITE**: First scene satellite name.
-  **SENSOR**: First scene sensor name.
-  **BIASn**: Bias value for the channel **n**.
-  **GAINn**: Gain value for the channel **n**.

See Also
--------

Implemented as ``gdal/frmts/raw/fastdataset.cpp``.

Landsat FAST L7A format description available from
http://ltpwww.gsfc.nasa.gov/IAS/htmls/l7_review.html (see `ESDIS Level 1
Product Generation System (LPGS) Output Files DFCB, Vol. 5, Book
2 <http://ltpwww.gsfc.nasa.gov/IAS/pdfs/DFCB_V5_B2_R4.pdf>`__)

EOSAT Fast Format REV. C description available from
http://www.euromap.de/docs/doc_001.html
