.. _raster.sar_ceos:

================================================================================
SAR_CEOS -- CEOS SAR Image
================================================================================

.. shortname:: SAR_CEOS

.. built_in_by_default::

This is a read-only reader for CEOS SAR image files. To use, select the
main imagery file.

This driver works with most Radarsat, JERS-1 and ERS data products, including
single look complex products; however, it is unlikely to work for
non-Radar CEOS products. The simpler `CEOS <#CEOS>`__ driver is often
appropriate for these.

This driver will attempt to read 15 lat/long GCPS by sampling the
per-scanline CEOS superstructure information. In the case of products from the
Alaska Satellite Facility, it will obtain corner coordinates from either the
map projection record in the case of ScanSAR products, or the facility data
record for non-ScanSAR products. It also captures various pieces of metadata
from various header files, including:

::

     CEOS_LOGICAL_VOLUME_ID=EERS-1-SAR-MLD
     CEOS_FACILITY=CDPF-RSAT
     CEOS_PROCESSING_FACILITY=APP
     CEOS_PROCESSING_AGENCY=CCRS
     CEOS_PROCESSING_COUNTRY=CANADA
     CEOS_SOFTWARE_ID=APP 1.62
     CEOS_ACQUISITION_TIME=19911029162818919
     CEOS_SENSOR_CLOCK_ANGLE=  90.000
     CEOS_ELLIPSOID=IUGG_75
     CEOS_SEMI_MAJOR=    6378.1400000
     CEOS_SEMI_MINOR=    6356.7550000

The SAR_CEOS driver also includes some support for SIR-C and PALSAR
polarimetric data. The SIR-C format contains an image in compressed
scattering matrix form, described
`here <http://southport.jpl.nasa.gov/software/dcomp/dcomp.html>`__. GDAL
decompresses the data as it is read in. The PALSAR format contains bands
that correspond almost exactly to elements of the 3x3 Hermitian
covariance matrix- see the
`ERSDAC-VX-CEOS-004A.pdf <http://www.ersdac.or.jp/palsar/palsar_E.html>`__
document for a complete description (pixel storage is described on page
193). GDAL converts these to complex floating point covariance matrix
bands as they are read in. The convention used to represent the
covariance matrix in terms of the scattering matrix elements HH, HV
(=VH), and VV is indicated below. Note that the non-diagonal elements of
the matrix are complex values, while the diagonal values are real
(though represented as complex bands).

-  Band 1: Covariance_11 (Float32) = HH*conj(HH)
-  Band 2: Covariance_12 (CFloat32) = sqrt(2)*HH*conj(HV)
-  Band 3: Covariance_13 (CFloat32) = HH*conj(VV)
-  Band 4: Covariance_22 (Float32) = 2*HV*conj(HV)
-  Band 5: Covariance_23 (CFloat32) = sqrt(2)*HV*conj(VV)
-  Band 6: Covariance_33 (Float32) = VV*conj(VV)

The identities of the bands are also reflected in the metadata.

NOTE: Implemented as ``gdal/frmts/ceos2/sar_ceosdataset.cpp``.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

