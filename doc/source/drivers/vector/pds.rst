.. _vector.pds:

PDS - Planetary Data Systems TABLE
==================================

.. shortname:: PDS

.. built_in_by_default::

This driver reads TABLE objects from PDS datasets. Note there is a GDAL
PDS driver to read the raster IMAGE objects from PDS datasets.

The driver must be provided with the product label file (even when the
actual data is placed in a separate file).

If the label file contains a *TABLE* object, it will be read as the only
layer of the dataset. If no *TABLE* object is found, the driver will
look for all objects containing the TABLE string and read each one in a
layer.

ASCII and BINARY tables are supported. The driver can retrieve the field
descriptions from inline COLUMN objects or from a separate file pointed
by ^STRUCTURE.

If the table has a LONGITUDE and LATITUDE columns of type REAL and with
UNIT=DEGREE, they will be used to return POINT geometries.

Driver capabilities
-------------------

.. supports_virtualio::

See Also
--------

-  `Description of PDS
   format <https://pds.jpl.nasa.gov/tools/standards-reference.shtml>`__
   (see Annex A.29 from StdRef_20090227_v3.8.pdf)
