.. _raster.palsar:

================================================================================
JAXA PALSAR Processed Products
================================================================================

.. shortname:: JAXAPALSAR

.. built_in_by_default::

This driver provides enhanced support for processed PALSAR products from
the JAXA PALSAR processor. This encompasses products acquired from the
following organizations:

-  JAXA (Japanese Aerospace eXploration Agency)
-  AADN (Alaska Satellite Facility)
-  ESA (European Space Agency)

This driver does not support products created using the Vexcel processor
(i.e. products distributed by ERSDAC and affiliated organizations).

Support is provided for the following features of PALSAR products:

-  Reading Level 1.1 and 1.5 processed products
-  Georeferencing for Level 1.5 products
-  Basic metadata (sensor information, ground pixel spacing, etc.)
-  Multi-channel data (i.e. dual-polarization or fully polarimetric
   datasets)

This is a read-only driver.

To open a PALSAR product, select the volume directory file (for example,
VOL-ALPSR000000000-P1.5_UA or VOL-ALPSR000000000-P1.1__A). The driver
will then use the information contained in the volume directory file to
find the various image files (the IMG-\* files), as well as the Leader
file. Note that the Leader file is essential for correct operation of
the driver.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

See Also
--------

-  `RESTEC Sample
   Data <http://www.alos-restec.jp/en/staticpages/index.php/service-sampledata>`__
