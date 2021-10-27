.. _raster.l1b:

================================================================================
L1B -- NOAA Polar Orbiter Level 1b Data Set (AVHRR)
================================================================================

.. shortname:: L1B

.. built_in_by_default::

GDAL supports NOAA Polar Orbiter Level 1b Data Set format for reading.
Now it can read NOAA-9(F) -- NOAA-17(M) datasets. NOTE: only AVHRR
instrument supported now, if you want read data from other instruments,
write to me (Andrey Kiselev, dron@ak4719.spb.edu). AVHRR LAC/HRPT (1 km
resolution) and GAC (4 km resolution) should be processed correctly.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Georeference
------------

Note, that GDAL simple affine georeference model completely unsuitable
for the NOAA data. So you should not rely on it. It is recommended to
use the thin plate spline warper (tps). Automatic image rectification
can be done with ground control points (GCPs) from the input file.

NOAA stores 51 GCPs per scanline both in the LAC and GAC datasets. In
fact you may get less than 51 GCPs, especially at end of scanlines.
Another approach to rectification is manual selection of the GCPs using
external source of georeference information.

A high density of GCPs will be reported, unless the L1B_HIGH_GCP_DENSITY
configuration option is set to NO.

Precision of the GCPs determination depends from the satellite type. In
the NOAA-9 -- NOAA-14 datasets geographic coordinates of the GCPs stored
in integer values as a 128th of a degree. So we can't determine
positions more precise than 1/128=0.0078125 of degree (~28"). In NOAA-15
-- NOAA-17 datasets we have much more precise positions, they are stored
as 10000th of degree.

The GCPs will also be reported as a
:ref:`geolocation array <rfc-41>`,
with Lagrangian interpolation of the 51 GCPs per scanline to the number
of pixels per scanline width.

Image will be always returned with most northern scanline located at the
top of image. If you want determine actual direction of the satellite
moving you should look at **LOCATION** metadata record.

Data
----

| In case of NOAA-10 in channel 5 you will get repeated channel 4 data.
| AVHRR/3 instrument (NOAA-15 -- NOAA-17) is a six channel radiometer,
  but only five channels are transmitted to the ground at any given
  time. Channels 3A and 3B cannot operate simultaneously. Look at
  channel description field reported by ``gdalinfo`` to determine what
  kind of channel contained in processed file.

Metadata
--------

Several parameters, obtained from the dataset stored as metadata
records.

Metadata records:

-  **SATELLITE**: Satellite name
-  **DATA_TYPE**: Type of the data, stored in the Level 1b dataset
   (AVHRR HRPT/LAC/GAC).
-  **REVOLUTION**: Orbit number. Note that it can be 1 to 2 off the
   correct orbit number (according to documentation).
-  **SOURCE**: Receiving station name.
-  **PROCESSING_CENTER**: Name of data processing center.
-  **START**: Time of first scanline acquisition (year, day of year,
   millisecond of day).
-  **STOP**: Time of last scanline acquisition (year, day of year,
   millisecond of day).
-  **LOCATION**: AVHRR Earth location indication. Will be **Ascending**
   when satellite moves from low latitudes to high latitudes and
   **Descending** in other case.

Most metadata records can be written to a .CSV
file when the L1B_FETCH_METADATA configuration file is set to YES. By
default, the filename will be called "[l1b_dataset_name]_metadata.csv",
and located in the same directory as the L1B dataset. By defining the
L1B_METADATA_DIRECTORY configuration option, it is possible to create
that file in another directory. The documentation to interpret those
metadata is `PODUG
3.1 <http://www.ncdc.noaa.gov/oa/pod-guide/ncdc/docs/podug/html/c3/sec3-1.htm>`__
for NOAA <=14 and `KLM
8.3.1.3.3.1 <http://www.ncdc.noaa.gov/oa/pod-guide/ncdc/docs/klm/html/c8/sec83133-1.htm>`__
for NOAA >=15.

Subdatasets
-----------

NOAA <=14 datasets advertise a
L1B_SOLAR_ZENITH_ANGLES:"l1b_dataset_name" subdataset that contains a
maximum of 51 solar zenith angles for each scanline ( beginning at
sample 5 with a step of 8 samples for GAC data, beginning at sample 25
with a step of 40 samples for HRPT/LAC/FRAC data).

NOAA >=15 datasets advertise a L1B_ANGLES:"l1b_dataset_name" subdataset
that contains 3 bands (solar zenith angles, satellite zenith angles and
relative azimuth angles) with 51 values for each scanline ( beginning at
sample 5 with a step of 8 samples for GAC data, beginning at sample 25
with a step of 40 samples for HRPT/LAC/FRAC data).

NOAA >=15 datasets advertise a L1B_CLOUDS:"l1b_dataset_name" subdataset
that contains a band of same dimensions as bands of the main L1B
dataset. The values of each pixel are 0 = unknown; 1 = clear; 2 =
cloudy; 3 = partly cloudy.

Nodata mask
-----------

NOAA >=15 datasets that report in their header to have missing scan
lines will expose a per-dataset mask band (following :ref:`rfc-15`) to
indicate such scan lines.

See Also
--------

-  Implemented as ``gdal/frmts/l1b/l1bdataset.cpp``.
-  NOAA Polar Orbiter Level 1b Data Set documented in the \``POD User's
   Guide'' (TIROS-N -- NOAA-14 satellites) and in the \``NOAA KLM User's
   Guide'' (NOAA-15 -- NOAA-16 satellites). You can find this manuals at
   `NOAA Technical Documentation Introduction
   Page <http://www2.ncdc.noaa.gov/docs/intro.htm>`__
-  There are a great variety of L1B datasets, sometimes with variations
   in header locations that are not documented in the official NOAA
   documentation. In case a dataset is not recognized by the GDAL L1B
   driver, the `pytroll <http://www.pytroll.org/>`__ package might be
   able to recognize it.
-  Excellent and complete review contained in the printed book \``The
   Advanced Very High Resolution Radiometer (AVHRR)'' by Arthur P.
   Cracknell, Taylor and Francis Ltd., 1997, ISBN 0-7484-0209-8.
-  NOAA data can be downloaded from the `Comprehensive Large Array-data
   Stewardship System (CLASS) <http://www.class.noaa.gov/>`__ (former
   SAA). Actually it is only source of Level 1b datasets for me, so my
   implementation tested with that files only.
-  `NOAA spacecrafts status
   page <http://www.oso.noaa.gov/poesstatus/>`__
