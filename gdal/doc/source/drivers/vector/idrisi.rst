.. _vector.idrisi:

Idrisi Vector (.VCT)
====================

.. shortname:: IDRISI

.. built_in_by_default::

This driver reads Idrisi vector files with .vct extension. The driver
recognized point, lines and polygons geometries.

For geographical referencing identification, the .vdc file contains
information that points to a file that holds the geographic reference
details. Those files uses extension REF and resides in the same folder
as the RST image or more likely in the Idrisi installation folders.

Therefore the presence or absence of the Idrisi software in the running
operation system will determine the way that this driver will work. By
setting the environment variable IDRISIDIR pointing to the Idrisi main
installation folder will enable GDAL to find more detailed information
about geographical reference and projection in the REF files.

Note that the driver recognizes the name convention used in Idrisi for
UTM and State Plane geographic reference so it doesn't need to access
the REF files. That is the case for RDC file that specify "utm-30n" or
"spc87ma1" in the "ref. system" field. Note that exporting to RST in any
other geographical reference system will generate a suggested REF
content in the comment section of the RDC file.

The driver can retrieve attributes from .ADC / .AVL ASCII files.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::
