.. _raster.fits:

================================================================================
FITS -- Flexible Image Transport System
================================================================================

.. shortname:: FITS

.. build_dependencies:: libcfitsio

FITS is a format used mainly by astronomers, but it is a relatively
simple format that supports arbitrary image types and multi-spectral
images, and so has found its way into GDAL. FITS support is implemented
in terms of the standard `CFITSIO
library <http://heasarc.gsfc.nasa.gov/docs/software/fitsio/fitsio.html>`__,
which you must have on your system in order for FITS support to be
enabled (see :ref:`notes on CFITSIO linking <notes-on-cfitsio-linking>`).
Both reading and writing of FITS files is supported.

Starting from version 3.0
georeferencing system support is implemented via the conversion of
WCS (World Coordinate System) keywords.
Only Latitude - Longitude systems (see the `FITS standard document
<https://fits.gsfc.nasa.gov/standard40/fits_standard40aa-le.pdf#subsection.8.3>`_)
have been implemented, those for which remote sensing processing is commonly used.
As 3D Datum information is missing in FITS/WCS standard, Radii and target bodies
are translated using the planetary extension proposed `here
<https://agupubs.onlinelibrary.wiley.com/doi/full/10.1029/2018EA000388>`_. 

Non-standard header keywords that are present in the FITS file will be
copied to the dataset's metadata when the file is opened, for access via
GDAL methods. Similarly, non-standard header keywords that the user
defines in the dataset's metadata will be written to the FITS file when
the GDAL handle is closed.

Note to those familiar with the CFITSIO library: The automatic rescaling
of data values, triggered by the presence of the BSCALE and BZERO header
keywords in a FITS file, is disabled in GDAL < v3.0. Those header keywords are
accessible and updatable via dataset metadata, in the same was as any
other header keywords, but they do not affect reading/writing of data
values from/to the file. Starting from version 3.0 BZERO and BSCALE keywords
are managed via standard :cpp:func:`GDALRasterBand::GetOffset` / :cpp:func:`GDALRasterBand::SetOffset`
and :cpp:func:`GDALRasterBand::GetScale` / :cpp:func:`GDALRasterBand::SetScale` GDAL functions and no more
referred as metadata.

Multiple image support
----------------------

Starting with GDAL 3.2, Multi-Extension FITS (MEF) files that contain one or
more extensions following the primary HDU are supported. When more than 2 image
HDUs are found, they are reported as subdatasets.

The connection string for a given subdataset/HDU is ``FITS:"filename.fits":hdu_number``

Examples
--------

* Listing subdatasets in a MEF .fits:

    ::

        $ gdalinfo ../autotest/gdrivers/data/fits/image_in_first_and_second_hdu.fits

        Driver: FITS/Flexible Image Transport System
        Files: ../autotest/gdrivers/data/fits/image_in_first_and_second_hdu.fits
        Size is 512, 512
        Metadata:
        EXTNAME=FIRST_IMAGE
        Subdatasets:
        SUBDATASET_1_NAME=FITS:"../autotest/gdrivers/data/fits/image_in_first_and_second_hdu.fits":1
        SUBDATASET_1_DESC=HDU 1 (1x2, 1 band), FIRST_IMAGE
        SUBDATASET_2_NAME=FITS:"../autotest/gdrivers/data/fits/image_in_first_and_second_hdu.fits":2
        SUBDATASET_2_DESC=HDU 2 (1x3, 1 band)
        Corner Coordinates:
        Upper Left  (    0.0,    0.0)
        Lower Left  (    0.0,  512.0)
        Upper Right (  512.0,    0.0)
        Lower Right (  512.0,  512.0)
        Center      (  256.0,  256.0)

* Opening a given HDU:

    ::

        $ gdalinfo FITS:"../autotest/gdrivers/data/fits/image_in_first_and_second_hdu.fits":1

        Driver: FITS/Flexible Image Transport System
        Files: none associated
        Size is 1, 2
        Metadata:
        EXTNAME=FIRST_IMAGE
        Corner Coordinates:
        Upper Left  (    0.0,    0.0)
        Lower Left  (    0.0,    2.0)
        Upper Right (    1.0,    0.0)
        Lower Right (    1.0,    2.0)
        Center      (    0.5,    1.0)
        Band 1 Block=1x1 Type=Byte, ColorInterp=Undefined


Other
-----

NOTE: Implemented as ``gdal/frmts/fits/fitsdataset.cpp``.

.. _notes-on-cfitsio-linking:

Notes on CFITSIO linking in GDAL
--------------------------------
Linux
^^^^^
From source
"""""""""""
Install CFITSIO headers from your distro (eg, cfitsio-devel on Fedora; libcfitsio-dev on Debian-Ubuntu), then compile GDAL as usual. CFITSIO will be automatically detected and linked.

From distros
""""""""""""
On Fedora/CentOS install CFITSIO then GDAL with dnf (yum): cfitsio is automatically linked.

MacOSX
^^^^^^
The last versions of the MacOSX packages are not linked against CFITSIO.
Install CFITSIO as described in the `official documentation <https://heasarc.gsfc.nasa.gov/docs/software/fitsio/fitsio_macosx.html>`__.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::
