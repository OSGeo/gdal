.. _raster.xpm:

================================================================================
XPM -- X11 Pixmap
================================================================================

.. shortname:: XPM

.. built_in_by_default::

GDAL includes support for reading and writing XPM (X11 Pixmap Format)
image files. These are colormapped one band images primarily used for
simple graphics purposes in X11 applications. It has been incorporated
in GDAL primarily to ease translation of GDAL images into a form usable
with the GTK toolkit.

The XPM support does not support georeferencing (not available from XPM
files) nor does it support XPM files with more than one character per
pixel. New XPM files must be colormapped or greyscale, and colortables
will be reduced to about 70 colors automatically.

NOTE: Implemented as ``gdal/frmts/xpm/xpmdataset.cpp``.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_virtualio::
