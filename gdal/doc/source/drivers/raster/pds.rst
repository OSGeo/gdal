.. _raster.pds:

================================================================================
PDS -- Planetary Data System v3
================================================================================

.. shortname:: PDS

PDS is a format used primarily by NASA to store and distribute solar,
lunar and planetary imagery data. GDAL provides read-only access to PDS
formatted imagery data.

PDS files often have the extension .img, sometimes with an associated
.lbl (label) file. When a .lbl file exists it should be used as the
dataset name rather than the .img file.

In addition to support for most PDS imagery configurations, this driver
also reads georeferencing and coordinate system information as well as
selected other header metadata.

Implementation of this driver was supported by the United States
Geological Survey.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Georeferencing
--------------

Due to ambiguities in the PDS specification, the georeferencing of some
products is subtly or grossly incorrect. There are configuration
variables which may be set for these products to correct the
interpretation of the georeferencing. Some details are available in
`ticket #5941 <http://trac.osgeo.org/gdal/ticket/5941>`__ and `ticket
#3940 <http://trac.osgeo.org/gdal/ticket/3940>`__. Due to corrections
made in `ticket #5941 <http://trac.osgeo.org/gdal/ticket/5941>`__ it is
recommended to GDAL 1.11.4 or higher.

As a test (for GDAL versions 1.11.4 and higher), download both the label
and image for the lunar `LOLA
DEM <http://pds-geosciences.wustl.edu/missions/lro/lola.htm>`__ (digital
elevation file) `LOLA PDS
label <http://pds-geosciences.wustl.edu/lro/lro-l-lola-3-rdr-v1/lrolol_1xxx/data/lola_gdr/cylindrical/img/ldem_4.lbl>`__
and `LOLA PDS v3
image <http://pds-geosciences.wustl.edu/lro/lro-l-lola-3-rdr-v1/lrolol_1xxx/data/lola_gdr/cylindrical/img/ldem_4.img>`__.
Using gdalinfo, the reported centered should be perfectly at 0.0, 0.0
meters in Cartesian space without any configuration options.

$ gdalinfo ldem_4.lbl

Example conversion to GeoTiff:

$ gdal_translate ldem_4.lbl out_LOLA.tif

Example conversion and applying offset and multiplier values as defined
in some PDS labels:

$ gdal_translate -ot Float32 -unscale ldem_4.lbl out_LOLA_32bit.tif

--------------

To show an example to correct an offset issue we can use the `MOLA
DEM <http://pds-geosciences.wustl.edu/missions/mgs/megdr.html>`__ from
the PDS. Download both the `MOLA PDS
label <http://pds-geosciences.wustl.edu/mgs/mgs-m-mola-5-megdr-l3-v1/mgsl_300x/meg004/megt90n000cb.lbl>`__
and `MOLA PDS v3
image <http://pds-geosciences.wustl.edu/mgs/mgs-m-mola-5-megdr-l3-v1/mgsl_300x/meg004/megt90n000cb.img>`__.
The MOLA labels currently contain a one pixel offset. To read this file
in correctly using GDAL (versions 1.11.4 and higher) set these options.

$ gdalinfo --config PDS_SampleProjOffset_Shift -0.5 --config
PDS_LineProjOffset_Shift -0.5 megt90n000cb.lbl

Again with these optional parameters, the center should be perfectly
0.0, 0.0 meters in Cartesian space.

Example conversion for MOLA:

$ gdal_translate --config PDS_SampleProjOffset_Shift -0.5 --config
PDS_LineProjOffset_Shift -0.5 megt90n000cb.lbl out_MOLA_4ppd.tif

Example conversion and applying offset and multiplier values as defined
in some PDS labels:

$ gdal_translate -ot Float32 -unscale --config
PDS_SampleProjOffset_Shift -0.5 --config PDS_LineProjOffset_Shift -0.5
megt90n000cb.lbl out_MOLA_4ppd_32bit.tif

--------------

PDS is part of a family of related formats including ISIS2 and ISIS3.


See Also
--------

-  Implemented as ``gdal/frmts/pds/pdsdataset.cpp``.
-  `NASA Planetary Data System <http://pds.nasa.gov/>`__
-  :ref:`raster.isis2` driver.
-  :ref:`raster.isis3` driver.
