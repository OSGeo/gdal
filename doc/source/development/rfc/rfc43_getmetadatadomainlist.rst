.. _rfc-43:

=======================================================================================
RFC 43: GDALMajorObject::GetMetadataDomainList
=======================================================================================

Author: Even Rouault

Contact: even dot rouault at spatialys.com

Summary
-------

This (mini)RFC proposes a new virtual method, GetMetadataDomainList(),
in the GDALMajorObject class (and a C API) to return the list of all
available metadata domains.

Background
----------

GDALMajorObject currently offers the GetMetadata() and GetMetadataItem()
methods that both accept a metadata domain argument. But there is no way
to auto-discover which metadata domains are valid for a given
GDALMajorObject (i.e. a dataset or raster band). This make it impossible
to have generic code that can exhaustively discover all metadata in a
dataset/raster band.

Implementation
--------------

The base implementation in GDALMajorObject just calls GetDomainList() on
the internal oMDMD member.

::

   /************************************************************************/
   /*                      GetMetadataDomainList()                         */
   /************************************************************************/

   /**
    * \brief Fetch list of metadata domains.
    *
    * The returned string list is the list of (non-empty) metadata domains.
    *
    * This method does the same thing as the C function GDALGetMetadataDomainList().
    * 
    * @return NULL or a string list. Must be freed with CSLDestroy()
    *
    * @since GDAL 1.11
    */

   char **GDALMajorObject::GetMetadataDomainList()
   {
       return CSLDuplicate(oMDMD.GetDomainList());
   }

This method is also available in the C API ( char \*\* CPL_STDCALL
GDALGetMetadataDomainList( GDALMajorObjectH hObject) ) and Swig
bindings.

Impacted drivers
----------------

Drivers that have custom implementations of GetMetadata() and/or
GetMetadataItem() will generally have to also implement
GetMetadataDomainList(), when they don't modify the oMDMD member.

To make it easy to implement the specialized GetMetadataDomainList(),
GDALMajorObject will offer a protected BuildMetadataDomainList() method
that can be used like the following :

::

   /************************************************************************/
   /*                      GetMetadataDomainList()                         */
   /************************************************************************/

   char **NITFDataset::GetMetadataDomainList()
   {
       return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                      TRUE,
                                      "NITF_METADATA", "NITF_DES", "NITF_DES_METADATA",
                                      "NITF_FILE_HEADER_TRES", "NITF_IMAGE_SEGMENT_TRES",
                                      "CGM", "TEXT", "TRE", "xml:TRE", "OVERVIEWS", NULL);
   }

The TRUE parameter means that the list of domains that follows are
potential domains, and thus BuildMetadataDomainList() will check for
each one that GetMetadata() returns a non-NULL value.

An exhaustive search in GDAL drivers has been made and all drivers that
needed to be updated to implement GetMetadataDomainList() have been
updated: ADRG, BAG, CEOS2, DIMAP, ECW, ENVISAT, ERS, GeoRaster (cannot
check myself that it compiles), GIF, GTiff, HDF4, JPEG, MBTILES, netCDF,
NITF, OGDI, PCIDSK, PDF, PNG, PostgisRaster, RasterLite, RS2, VRT, WCS,
WebP, WMS.

A few caveats :

-  For MBTiles, WMS and VRT, GetMetadataDomainList(), at the band level,
   will return "LocationInfo" as a valid metadata domain (used by the
   gdallocationinfo utility), even if GetMetadata("LocationInfo") itself
   does not return metadata : you have to call
   GetMetadataItem("Pixel_someX_someY", "LocationInfo") or
   GetMetadataItem("GeoPixel_someX_someY", "LocationInfo").
-  For CEOS2 and ENVISAT, the list of metadata domains cannot be
   established easily. GetMetadataDomainList() will return the pattern
   of accepted domain names.

Impacted utilities
------------------

The gdalinfo utility is extended to accept :

-  a "-listmdd" option that will print the metadata domains available,

::

   $ gdalinfo ../autotest/gdrivers/data/byte_with_xmp.jpg -listmdd

   Driver: JPEG/JPEG JFIF
   Files: ../autotest/gdrivers/data/byte_with_xmp.jpg
   Size is 20, 20
   Coordinate System is `'
   Metadata domains:
     xml:XMP
   Corner Coordinates:
   Upper Left  (    0.0,    0.0)
   Lower Left  (    0.0,   20.0)
   Upper Right (   20.0,    0.0)
   Lower Right (   20.0,   20.0)
   Center      (   10.0,   10.0)
   Band 1 Block=20x1 Type=Byte, ColorInterp=Gray
     Metadata domains:
       IMAGE_STRUCTURE
     Image Structure Metadata:
       COMPRESSION=JPEG

-  and "-mdd all" will display the content of all metadata domains.

::

   $ gdalinfo ../autotest/gdrivers/data/byte_with_xmp.jpg -mdd all

   Driver: JPEG/JPEG JFIF
   Files: ../autotest/gdrivers/data/byte_with_xmp.jpg
   Size is 20, 20
   Coordinate System is `'
   Metadata (xml:XMP):
   <?xpacket begin='' id='W5M0MpCehiHzreSzNTczkc9d'?>
   <x:xmpmeta xmlns:x='adobe:ns:meta/' x:xmptk='Image::ExifTool 7.89'>
   <rdf:RDF xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#'>

    <rdf:Description rdf:about=''
     xmlns:dc='http://purl.org/dc/elements/1.1/'>
     <dc:description>
      <rdf:Alt>
       <rdf:li xml:lang='x-default'>Description</rdf:li>
      </rdf:Alt>
     </dc:description>
     <dc:subject>
      <rdf:Bag>
       <rdf:li>XMP</rdf:li>
       <rdf:li>Test</rdf:li>
      </rdf:Bag>
     </dc:subject>
     <dc:title>
      </rdf:Alt>
     </dc:title>
    </rdf:Description>

    <rdf:Description rdf:about=''
     xmlns:tiff='http://ns.adobe.com/tiff/1.0/'>
     <tiff:BitsPerSample>
      <rdf:Seq>
       <rdf:li>8</rdf:li>
      </rdf:Seq>
     </tiff:BitsPerSample>
     <tiff:Compression>1</tiff:Compression>
     <tiff:ImageLength>20</tiff:ImageLength>
     <tiff:ImageWidth>20</tiff:ImageWidth>
     <tiff:PhotometricInterpretation>1</tiff:PhotometricInterpretation>
     <tiff:PlanarConfiguration>1</tiff:PlanarConfiguration>
     <tiff:SamplesPerPixel>1</tiff:SamplesPerPixel>
    </rdf:Description>
   </rdf:RDF>
   </x:xmpmeta>
                                                                                                       
                                                                                                       
                                                                                                       
                                                                                                       
                                                                                                       
                                                                                                       
                                                                                                       
                                                                                                       
                                                                                                       
                                                                                                       
                                                                                                       
                                                                                                       
                                                                                                       
                                                                                                       
                                                                                                       
                                                                                                       
                                                                                                       
                                                                                                       
                                                                                                       
                                                                                                       
                                                                                                       
                                                                                                       
                                                                                                       
                                                                                                       
   <?xpacket end='w'?>
   Corner Coordinates:
   Upper Left  (    0.0,    0.0)
   Lower Left  (    0.0,   20.0)
   Upper Right (   20.0,    0.0)
   Lower Right (   20.0,   20.0)
   Center      (   10.0,   10.0)
   Band 1 Block=20x1 Type=Byte, ColorInterp=Gray
     Image Structure Metadata:
       COMPRESSION=JPEG

Backward Compatibility
----------------------

This change has no impact on backward compatibility at the C API/ABI and
C++ API levels. But it impacts C++ ABI, so it requires a full rebuild of
all GDAL drivers.

Testing
-------

The Python autotest suite will be extended to test the new API in a few
drivers.

Ticket
------

Ticket #5275 has been opened to track the progress of this RFC.

The implementation is available in `an attachment to ticket
5275 <http://trac.osgeo.org/gdal/attachment/ticket/5275/getmetadatadomainlist.patch>`__.

Voting history
--------------

+1 from EvenR, DanielM and JukkaR
