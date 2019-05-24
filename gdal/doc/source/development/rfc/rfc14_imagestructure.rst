.. _rfc-14:

================================================================================
RFC 14: Image Structure Metadata
================================================================================

Author: Frank Warmerdam

Contact: warmerdam@pobox.com

Status: Adopted

Summary
-------

This RFC attempts to formalize the semantics of the "IMAGE_STRUCTURE"
domain of metadata. This metadata domain is used to hold structural
information about image organization that would not normally be carried
with an image when translated into another format. The IMAGE_STRUCTURE
metadata may occur on the GDALDataset or on individual bands, and most
items are meaningful in both contexts. When items like NBITS are found
on the dataset it is assumed they apply to all bands of that dataset.

IMAGE_STRUCTURE items
---------------------

COMPRESSION:: The compression type used for this dataset or band. There
is no fixed catalog of compression type names, but where a given format
includes a COMPRESSION creation option, the same list of values should
be used here as there.

NBITS:: The actual number of bits used for this band, or the bands of
this dataset. Normally only present when the number of bits is
non-standard for the datatype, such as when a 1 bit TIFF is represented
through GDAL as GDT_Byte.

INTERLEAVE:: This only applies on datasets, and the value should be one
of PIXEL, LINE or BAND. It can be used as a data access hint.

PIXELTYPE:: This may appear on a GDT_Byte band (or the corresponding
dataset) and have the value SIGNEDBYTE to indicate the unsigned byte
values between 128 and 255 should be interpreted as being values between
-128 and -1 for applications that recognise the SIGNEDBYTE type.

Compatibility Issues
--------------------

This RFC has two changes from existing practise that may cause
compatibility issues:

1. Traditionally the NBITS metadata appeared in the default metadata
   domain on datasets, instead of in the IMAGE_STRUCTURE domain.
2. Traditionally the COMPRESSION metadata appeared only on the dataset,
   never one the band.

I am only aware of one application previously making systematic use of
these items, and it will be updated to reflect the new usage as GDAL
1.5.0 is adopted.

Development
-----------

Beyond adopting the definition for the semantics of the IMAGE_STRUCTURE
metadata, the following development steps will be taken:

1. The PNG, GTiff, NITF and EHdr drivers will be updated to place NBITS
   in the IMAGE_STRUCTURE metadata domain.
2. The HFA driver will be updated to return NBITS metadata.
3. The HFA, GTiff, JP2KAK, ECW, JPEG, and PNG drivers will be updated to
   return INTERLEAVE metadata.
4. The HFA and GTiff drivers will be updated to return PIXELTYPE
   metadata.

The development will be done by Frank Warmerdam in trunk in time for
GDAL/OGR 1.5.0 release. Changes to other drivers that these definitions
might be useful for while be done as time permits by interested
developers - not necessarily in time for GDAL/OGR 1.5.0.

Notes
-----

-  The gdalinfo utility already reports IMAGE_STRUCTURE metadata when it
   is available.
-  The GTiff, and HFA drivers CreateCopy() methods check the source for
   NBITS, and PIXELTYPE metadata to create specialized output files
   types.
-  The GTiff, HFA and default CreateCopy() implementations have been
   reworked to use the new GDALDatasetCopyWholeRaster() function which
   uses the INTERLEAVE metadata as a clue whether to do interleaved
   copies if the source dataset is interleaved.
