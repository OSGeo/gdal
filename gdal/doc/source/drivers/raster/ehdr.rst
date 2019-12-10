.. _raster.ehdr:

================================================================================
EHdr -- ESRI .hdr Labelled
================================================================================

.. shortname:: EHdr

.. built_in_by_default::

GDAL supports reading and writing the ESRI .hdr labeling format, often
referred to as ESRI BIL format. Eight, sixteen and thirty-two bit
integer raster data types are supported as well as 32 bit floating
point. Coordinate systems (from a .prj file), and georeferencing are
supported. Unrecognized options in the .hdr file are ignored. To open a
dataset select the file with the image file (often with the extension
.bil). If present .clr color table files are read, but not written. If
present, image.rep file will be read to extract the projection system of
SpatioCarte Defense 1.0 raster products.

This driver does not always do well differentiating between floating
point and integer data. The GDAL extension to the .hdr format to
differentiate is to add a field named PIXELTYPE with values of either
FLOAT, SIGNEDINT or UNSIGNEDINT. In combination with the NBITS field it
is possible to described all variations of pixel types.

eg.

::

     ncols 1375
     nrows 649
     cellsize 0.050401
     xllcorner -130.128639
     yllcorner 20.166799
     nodata_value 9999.000000
     nbits 32
     pixeltype float
     byteorder msbfirst

This driver may be sufficient to read GTOPO30 data.

NOTE: Implemented as ``gdal/frmts/raw/ehdrdataset.cpp``.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

See Also
--------

-  `ESRI whitepaper: + Extendable Image Formats for ArcView GIS 3.1 and
   3.2 <http://downloads.esri.com/support/whitepapers/other_/eximgav.pdf>`__
   (BIL, see p. 5)
-  `GTOPO30 - Global Topographic
   Data <http://edcdaac.usgs.gov/gtopo30/gtopo30.html>`__
-  `GTOPO30
   Documentation <http://edcdaac.usgs.gov/gtopo30/README.html>`__
-  `SpatioCarte Defense 1.0
   specification <http://eden.ign.fr/download/pub/doc/emabgi/spdf10.pdf/download>`__
   (in French)
-  `SRTMHGT Driver <#SRTMHGT>`__
