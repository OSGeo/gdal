.. _raster.airsar:

================================================================================
AIRSAR -- AIRSAR Polarimetric Format
================================================================================

.. shortname:: AIRSAR

.. built_in_by_default::

Most variants of the AIRSAR Polarimetric Format produced by the AIRSAR
Integrated Processor are supported for reading by GDAL. AIRSAR products
normally include various associated data files, but only the imagery
data themselves is supported. Normally these are named
*mission*\ \_l.dat (L-Band) or *mission*\ \_c.dat (C-Band).

AIRSAR format contains a polarimetric image in compressed stokes matrix
form. Internally GDAL decompresses the data into a stokes matrix, and
then converts that form into a covariance matrix. The returned six bands
are the six values needed to define the 3x3 Hermitian covariance matrix.
The convention used to represent the covariance matrix in terms of the
scattering matrix elements HH, HV (=VH), and VV is indicated below. Note
that the non-diagonal elements of the matrix are complex values, while
the diagonal values are real (though represented as complex bands).

-  Band 1: Covariance_11 (Float32) = HH*conj(HH)
-  Band 2: Covariance_12 (CFloat32) = sqrt(2)*HH*conj(HV)
-  Band 3: Covariance_13 (CFloat32) = HH*conj(VV)
-  Band 4: Covariance_22 (Float32) = 2*HV*conj(HV)
-  Band 5: Covariance_23 (CFloat32) = sqrt(2)*HV*conj(VV)
-  Band 6: Covariance_33 (Float32) = VV*conj(VV)

The identities of the bands are also reflected in metadata and in the
band descriptions.

The AIRSAR product format includes (potentially) several headers of
information. This information is captured and represented as metadata on
the file as a whole. Information items from the main header are prefixed
with "MH\_", items from the parameter header are prefixed with "PH\_" and
information from the calibration header are prefixed with "CH\_". The
metadata item names are derived automatically from the names of the
fields within the header itself.

No effort is made to read files associated with the AIRSAR product such
as *mission*\ \_l.mocomp, *mission*\ \_meta.airsar or
*mission*\ \_meta.podaac.

Driver capabilities
-------------------

.. supports_virtualio::

See Also
--------

-  `AIRSAR Data
   Format <http://airsar.jpl.nasa.gov/documents/dataformat.htm>`__
