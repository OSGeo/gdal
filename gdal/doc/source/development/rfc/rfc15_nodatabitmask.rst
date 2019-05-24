.. _rfc-15:

================================================================================
RFC 15: Band Masks
================================================================================

Author: Frank Warmerdam

Contact: warmerdam@pobox.com

Status: Adopted

Summary
-------

Some file formats support a concept of a bitmask to identify pixels that
are not valid data. This can be particularly valuable with byte image
formats where a nodata pixel value can not be used because all pixel
values have a valid meaning. This RFC tries to formalize a way of
recognising and accessing such null masks through the GDAL API, while
moving to a uniform means of representing other kinds of masking (nodata
values, and alpha bands).

The basic approach is to treat such masks as raster bands, but not
regular raster bands on the datasource. Instead they are freestanding
raster bands in a manner similar to the overview raster band objects.
The masks are represented as GDT_Byte bands with a value of zero
indicating nodata and non-zero values indicating valid data. Normally
the value 255 will be used for valid data pixels.

API
---

GDALRasterBand is extended with the following methods:

::

       virtual GDALRasterBand *GetMaskBand();
       virtual int             GetMaskFlags();
       virtual CPLErr          CreateMaskBand( int nFlags );

GDALDataset is extended with the following method:

::

       virtual CPLErr          CreateMaskBand( nFlags );

Note that the GetMaskBand() should always return a GDALRasterBand mask,
even if it is only an all 255 mask with the flags indicating
GMF_ALL_VALID.

The GetMaskFlags() method returns an bitwise OR-ed set of status flags
with the following available definitions that may be extended in the
future:

-  GMF_ALL_VALID(0x01): There are no invalid pixels, all mask values
   will be 255. When used this will normally be the only flag set.
-  GMF_PER_DATASET(0x02): The mask band is shared between all bands on
   the dataset.
-  GMF_ALPHA(0x04): The mask band is actually an alpha band and may have
   values other than 0 and 255.
-  GMF_NODATA(0x08): Indicates the mask is actually being generated from
   nodata values. (mutually exclusive of GMF_ALPHA)

The CreateMaskBand() method will attempt to create a mask band
associated with the band on which it is invoked, issuing an error if it
is not supported. Currently the only flag that is meaningful to pass in
when creating a mask band is GMF_PER_DATASET. The rest are used to
represent special system provided mask bands. GMF_PER_DATASET is assumed
when CreateMaskBand() is called on a dataset.

Default GetMaskBand() / GetMaskFlags() Implementation
-----------------------------------------------------

The GDALRasterBand class will include a default implementation of
GetMaskBand() that returns one of three default implementations.

-  If a corresponding .msk file exists it will be used for the mask
   band.
-  If the band has a nodata value set, an instance of the new
   GDALNodataMaskRasterBand class will be returned. GetMaskFlags() will
   return GMF_NODATA.
-  If there is no nodata value, but the dataset has an alpha band that
   seems to apply to this band (specific rules yet to be determined) and
   that is of type GDT_Byte then that alpha band will be returned, and
   the flags GMF_PER_DATASET and GMF_ALPHA will be returned in the
   flags.
-  If neither of the above apply, an instance of the new
   GDALAllValidRasterBand class will be returned that has 255 values for
   all pixels. The null flags will return GMF_ALL_VALID.

The GDALRasterBand will include a protected poMask instance variable and
a bOwnMask flag. The first call to the default GetMaskBand() will result
in creation of the GDALNodataMaskRasterBand, GDALAllValidMaskRasterBand
and their assignment to poMask with bOwnMask set TRUE. If an alpha band
is identified for use, it will be assigned to poMask and bOwnMask set to
FALSE. The GDALRasterBand class will take care of deleting the poMask if
set and bOwnMask is true in the destructor. Derived band classes may
safely use the poMask and bOwnMask flag similarly as long as the
semantics are maintained.

For an external .msk file to be recognized by GDAL, it must be a valid
GDAL dataset, with the same name as the main dataset and suffixed with
.msk, with either one band (in the GMF_PER_DATASET case), or as many
bands as the main dataset. It must have INTERNAL_MASK_FLAGS_xx metadata
items set at the dataset level, where xx matches the band number of a
band of the main dataset. The value of those items is a combination of
the flags GMF_ALL_VALID, GMF_PER_DATASET, GMF_ALPHA and GMF_NODATA. If a
metadata item is missing for a band, then the other rules explained
above will be used to generate a on-the-fly mask band.

Default CreateMaskBand()
------------------------

The default implementation of the CreateMaskBand() method will be
implemented based on similar rules to the .ovr handling implemented
using the GDALDefaultOverviews object. A TIFF file with the extension
.msk will be created with the same basename as the original file, and it
will have as many bands as the original image (or just one for
GMF_PER_DATASET). The mask images will be deflate compressed tiled
images with the same block size as the original image if possible.

The default implementation of GetFileList() will also be modified to
know about the .msk files.

CreateCopy()
------------

The GDALDriver::DefaultCreateCopy(), and GDALPamDataset::CloneInfo()
methods will be updated to copy mask information if it seems necessary
and is possible. Note that NODATA, ALL_VALID and ALPHA type masks are
not copied since they are just derived information.

Alpha Bands
-----------

When a dataset has a normal GDT_Byte alpha (transparency) band that
applies, it should be returned as the null mask, but the GetMaskFlags()
method should include GMF_ALPHA. For processing purposes any value other
than 0 should be treated as valid data, though some algorithms will
treat values between 1 and 254 as partially transparent.

Drivers Updated
---------------

These drivers will be updated:

-  JPEG Driver: support the "zlib compressed mask appended to the file"
   approach used by a few data providers.
-  GRASS Driver: updated to support handling null values as masks.

Possibly updated:

-  HDF4 Driver: This driver might possibly be updated to return real
   mask if we can figure out a way.
-  SDE Driver: This driver might be updated if Howard has sufficient
   time and enthusiasm.

Utilities
---------

The gdalwarp utility and the gdal warper algorithm will be updated to
use null masks on input. The warper algorithm already uses essentially
this model internally. For now gdalwarp output (nodata or alpha band)
will remain unchanged, though at some point in the future support may be
added for explicitly generating null masks, but for most purposes
producing an alpha band is producing a null mask.

Implementation Plan
-------------------

This change will be implemented by Frank Warmerdam in trunk in time for
the 1.5.0 release.

SWIG Implications
-----------------

The GetMaskBand(), GetMaskFlags() and CreateMaskBand() methods (and
corresponding defines) will need to be added. The mask should work like
a normal raster band for swig purposes so minimal special work should be
required.

Testing
-------

The gdalautotest will be extended with the following:

-  gcore/mask.py: test default mask implementation for nodata, alpha and
   all valid cases.
-  gdriver/jpeg.py: extend with a test for "appended bitmask" case -
   creation and reading.

Interactive testing will be done for gdalwarp.
