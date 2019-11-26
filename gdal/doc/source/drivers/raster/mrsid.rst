.. _raster.mrsid:

================================================================================
MrSID -- Multi-resolution Seamless Image Database
================================================================================

.. shortname:: MrSID

.. build_dependencies:: MrSID SDK

MrSID is a wavelet-based image compression technology which can utilize
both lossy and lossless encoding. This technology was acquired in its
original form from Los Alamos National Laboratories (LANL), where it was
developed under the aegis of the U.S. government for storing
fingerprints for the FBI. Now it is developed and distributed by
Extensis.

This driver supports reading of MrSID image files using Extensis'
decoding software development kit (DSDK). **This DSDK is not free
software, you should contact Extensis to obtain it (see link at end of
this page).** If you are using GCC, please, ensure that you have the
same compiler as was used for DSDK compilation. It is C++ library, so
you may get incompatibilities in C++ name mangling between different GCC
versions (2.95.x and 3.x).

Latest versions of the DSDK also support decoding JPEG2000 file format,
so this driver can be used for JPEG2000 too.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Metadata
--------

MrSID metadata transparently translated into GDAL metadata strings.
Files in MrSID format contain a set of standard metadata tags such as:
IMAGE__WIDTH (contains the width of the image), IMAGE__HEIGHT (contains
the height of the image), IMAGE__XY_ORIGIN (contains the x and y
coordinates of the origin), IMAGE__INPUT_NAME (contains the name or
names of the files used to create the MrSID image) etc. GDAL's metadata
keys cannot contain characters \`:' and \`=', but standard MrSID tags
always contain double colons in tag names. These characters replaced in
GDAL with \`_' during translation. So if you are using other software to
work with MrSID be ready that names of metadata keys will be shown
differently in GDAL.

XMP metadata can be extracted from JPEG2000
files, and will be stored as XML raw content in the xml:XMP metadata
domain.

Georeference
------------

MrSID images may contain georeference and coordinate system information
in form of GeoTIFF GeoKeys, translated in metadata records. All those
GeoKeys properly extracted and used by the driver. Unfortunately, there
is one caveat: old MrSID encoders has a bug which resulted in wrong
GeoKeys, stored in MrSID files. This bug was fixed in MrSID software
version 1.5, but if you have older encoders or files, created with older
encoders, you cannot use georeference information from them.

See Also:
---------

-  Implemented as ``gdal/frmts/mrsid/mrsiddataset.cpp``.
-  `Extensis web site <http://www.extensis.com/support/developers>`__
