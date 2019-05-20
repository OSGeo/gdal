.. _raster.fits:

FITS -- Flexible Image Transport System
=======================================

.. shortname:: FITS

FITS is a format used mainly by astronomers, but it is a relatively
simple format that supports arbitrary image types and multi-spectral
images, and so has found its way into GDAL. FITS support is implemented
in terms of the standard `CFITSIO
library <http://heasarc.gsfc.nasa.gov/docs/software/fitsio/fitsio.html>`__,
which you must have on your system in order for FITS support to be
enabled. Both reading and writing of FITS files is supported. At the
current time, no support for a georeferencing system is implemented, but
WCS (World Coordinate System) support is possible in the future.

Non-standard header keywords that are present in the FITS file will be
copied to the dataset's metadata when the file is opened, for access via
GDAL methods. Similarly, non-standard header keywords that the user
defines in the dataset's metadata will be written to the FITS file when
the GDAL handle is closed.

Note to those familiar with the CFITSIO library: The automatic rescaling
of data values, triggered by the presence of the BSCALE and BZERO header
keywords in a FITS file, is disabled in GDAL. Those header keywords are
accessible and updatable via dataset metadata, in the same was as any
other header keywords, but they do not affect reading/writing of data
values from/to the file.

NOTE: Implemented as ``gdal/frmts/fits/fitsdataset.cpp``.


Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::
