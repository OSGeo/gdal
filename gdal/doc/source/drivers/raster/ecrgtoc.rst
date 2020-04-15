.. _raster.ecrgtoc:

================================================================================
ECRGTOC -- ECRG Table Of Contents (TOC.xml)
================================================================================

.. shortname:: ECRGTOC

.. built_in_by_default::

This is a read-only reader for ECRG (Enhanced Compressed Raster Graphic)
products, that uses the table of content file, TOC.xml, and exposes it
as a virtual dataset whose coverage is the set of ECRG frames contained
in the table of content.

The driver will report a different subdataset for each subdataset found
in the TOC.xml file. Each subdataset consists of the frames of same
product id, disk id, and with same scale.

Result of a gdalinfo on a TOC.xml file.

::

   Subdatasets:
     SUBDATASET_1_NAME=ECRG_TOC_ENTRY:ECRG:FalconView:1_500_K:ECRG_Sample/EPF/TOC.xml
     SUBDATASET_1_DESC=Product ECRG, Disk FalconView, Scale 1:500 K

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::


See Also
--------

-  :ref:`raster.nitf`: format of the ECRG frames
-  `MIL-PRF-32283 <http://www.everyspec.com/MIL-PRF/MIL-PRF+%28030000+-+79999%29/MIL-PRF-32283_26022/>`__
   : specification of ECRG products

NOTE: Implemented as ``gdal/frmts/nitf/ecrgtocdataset.cpp``
