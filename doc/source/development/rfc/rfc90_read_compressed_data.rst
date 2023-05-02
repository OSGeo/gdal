.. _rfc-90:

=============================================================
RFC 90: Direct access to compressed raster data
=============================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault at spatialys.com
Started:       2023-Jan-03
Status:        Adopted, implemented
Target:        GDAL 3.7
============== =============================================

Summary
-------

The document proposes 2 new methods to directly obtain the content of a window
of interest of a raster dataset in its native compressed format. Those methods
can be optionally implemented and used by drivers to perform:

- extraction of a compressed tile as a standalone file from a container format
  (GeoTIFF, GeoPackage, etc.)

- creation of mosaics from a set of tiles in individual files

- lossless conversion between container formats using the same
  compression method

Motivation
----------

When converting data between formats, calls to RasterIO(GF_Read, ...) followed
by RasterIO(GF_Write, ...) must be currently done. For compressed formats, this
causes decompression and recompression of data.

In some particular cases, we could avoid decompressing and recompressing when the
underlying compression method is the same (or compatible), and when requesting
data aligned on the boundaries of the encoded data (typically whole tiles).
This would save execution time and would avoid extra quality loss due to
recompression cycles for lossy compression methods.

This RFC offers the framework to potentially address the following use cases
(non exhaustive list):

- extraction of a JPEG (resp. WEBP, JPEGXL)-compressed whole tile from a GeoTIFF
  file to a standalone JPEG (resp. WEBP, JPEGXL) file.
  Typical scenario of tile servers using a GeoTIFF file as a tile backend.

- similarly as above, but with GeoPackage and JPEG/PNG/WEBP compression methods.

- extraction of a subset of whole tiles of a JPEG/WEBP/JPEGXL-compressed tiled
  GeoTIFF to a tiled GeoTIFF using the same compression method.

- similarly as above, but with GeoPackage and JPEG/PNG/WEBP compression methods.

- lossless conversion of a mosaic of JPEG, WEBP, etc. non-overlapping and
  contiguous tiles, each in a separate file, assembled as a VRT mosaic, to a
  tiled GeoTIFF using the same compression method, with the tile dimension being
  the one of each source file.

- lossless conversion of a JPEG-compressed (resp. WEBP-compressed) tiled GeoTIFF
  to a JPEG-compressed (resp. WEBP-compressed) GeoPackage file and vice-versa.
  Or a subset of the file, provided that the window of interest is aligned on
  tiles boundaries.

- lossless conversion of JPEG to JPEGXL
  (currently implemented in master in a rather ad-hoc way in the JPEGXL driver)

- lossless conversion of JPEGXL that has JPEG reconstruction box back to JPEG
  (currently implemented in master in a rather ad-hoc way in the JPEGXL and
  JPEGXL drivers)

- lossless conversion of JPEG-compressed tiled GeoTIFF to a JPEGXL-compressed
  tiled GeoTIFF (and the reverse if JPEG reconstruction box is included).
  Or a subset of the file, provided that the window of interest is aligned on
  tiles boundaries.

- extraction of a subset of a JPEG-compressed tiled GeoTIFF to a standalone JPEG
  file, with a window of interest not necessarily aligned on tile boundaries, but
  just on the JPEG MCU (Minimum Code Unit), which is equal to 8 pixels in the
  general case, and 16 for YCbCr data. And when intersecting several tiles,
  provided that they share the same quality settings (which is usually the case!)
  Or lossless retiling of JPEG-compressed tiled GeoTIFF (e.g 256x256 -> 512x512)

.. note::

    The proposed candidate implementation only covers a subset of those use cases
    (some of them would require significant implementation effort), but the
    proposed API addition makes them possible pending further developments.


Technical details
-----------------

The following 2 new methods are added at the :cpp:class:`GDALDataset` level:

GetCompressionFormats()
+++++++++++++++++++++++

.. code-block:: c++

    /** Return the compression formats that can be natively obtained for the
     * window of interest and requested bands.
     *
     * For example, a tiled dataset may be able to return data in a compressed
     * format if the window of interest matches exactly a tile. For some formats,
     * drivers may also be able to merge several tiles together (not currently
     * implemented though).
     *
     * Each format string is a pseudo MIME type, whose first part can be passed
     * as the pszFormat argument of ReadCompressedData(), with additional
     * parameters specified as key=value with a semi-colon separator.
     *
     * The amount and types of optional parameters passed after the MIME type is
     * format dependent, and driver dependent (some drivers might not be able to
     * return those extra information without doing a rather costly processing).
     *
     * For example, a driver might return "JPEG;frame_type=SOF0_baseline;"
     * "bit_depth=8;num_components=3;subsampling=4:2:0;colorspace=YCbCr", and
     * consequently "JPEG" can be passed as the pszFormat argument of
     * ReadCompressedData(). For JPEG, implementations can use the
     * GDALGetCompressionFormatForJPEG() helper method to generate a string like
     * above from a JPEG codestream.
     *
     * Several values might be returned. For example,
     * the JPEGXL driver will return "JXL", but also potentially "JPEG"
     * if the JPEGXL codestream includes a JPEG reconstruction box.
     *
     * In the general case this method will return an empty list.
     *
     * @param nXOff The pixel offset to the top left corner of the region
     * of the band to be accessed.  This would be zero to start from the left side.
     *
     * @param nYOff The line offset to the top left corner of the region
     * of the band to be accessed.  This would be zero to start from the top.
     *
     * @param nXSize The width of the region of the band to be accessed in pixels.
     *
     * @param nYSize The height of the region of the band to be accessed in lines.
     *
     * @param nBandCount the number of bands being requested.
     *
     * @param panBandList the list of nBandCount band numbers.
     * Note band numbers are 1 based. This may be NULL to select the first
     * nBandCount bands.
     *
     * @return a list of compatible formats (which may be empty)
     *
     * @since GDAL 3.7
     */
    CPLStringList
    GDALDataset::GetCompressionFormats(int nXOff, int nYOff,
                                       int nXSize, int nYSize,
                                       int nBandCount,
                                       const int *panBandList);

For example, to check if native compression format(s) are available on the
whole image:


.. code-block:: c++

  const CPLStringList aosFormats =
     poDataset->GetCompressionFormats(0, 0,
                                      poDataset->GetRasterXSize(),
                                      poDataset->GetRasterYSize(),
                                      poDataset->GetRasterCount(),
                                      nullptr);
  for( const char* pszFormat: aosFormats )
  {
     // Remove optional parameters and just print out the MIME type.
     const CPLStringList aosTokens(CSLTokenizeString2(pszFormat, ";", 0));
     printf("Found format %s\n, aosTokens[0]);
  }


ReadCompressedData()
++++++++++++++++++++

.. code-block:: c++

    /** Return the compressed content that can be natively obtained for the
     * window of interest and requested bands.
     *
     * For example, a tiled dataset may be able to return data in compressed format
     * if the window of interest matches exactly a tile. For some formats, drivers
     * may also be able to merge several tiles together (not currently
     * implemented though).
     *
     * The implementation should make sure that the content returned forms a valid
     * standalone file. For example, for the GeoTIFF implementation of this method,
     * when extracting a JPEG tile, the method will automatically add the content
     * of the JPEG Huffman and/or quantization tables that might be stored in the
     * TIFF JpegTables tag, and not in tile data itself.
     *
     * In the general case this method will return CE_Failure.
     *
     * @param pszFormat Requested compression format (e.g. "JPEG",
     * "WEBP", "JXL"). This is the MIME type of one of the values
     * returned by GetCompressionFormats(). The format string is designed to
     * potentially include at a later point key=value optional parameters separated
     * by a semi-colon character. At time of writing, none are implemented.
     * ReadCompressedData() implementations should verify optional parameters and
     * return CE_Failure if they cannot support one of them.
     *
     * @param nXOff The pixel offset to the top left corner of the region
     * of the band to be accessed.  This would be zero to start from the left side.
     *
     * @param nYOff The line offset to the top left corner of the region
     * of the band to be accessed.  This would be zero to start from the top.
     *
     * @param nXSize The width of the region of the band to be accessed in pixels.
     *
     * @param nYSize The height of the region of the band to be accessed in lines.
     *
     * @param nBandCount the number of bands being requested.
     *
     * @param panBandList the list of nBandCount band numbers.
     * Note band numbers are 1 based. This may be NULL to select the first
     * nBandCount bands.
     *
     * @param ppBuffer Pointer to a buffer to store the compressed data or nullptr.
     * If ppBuffer is not nullptr, then pnBufferSize should also not be nullptr.
     * If ppBuffer is not nullptr, and *ppBuffer is not nullptr, then the provided
     * buffer will be filled with the compressed data, provided that pnBufferSize
     * and *pnBufferSize are not nullptr, and *pnBufferSize, indicating the size
     * of *ppBuffer, is sufficiently large to hold the data.
     * If ppBuffer is not nullptr, but *ppBuffer is nullptr, then the method will
     * allocate *ppBuffer using VSIMalloc(), and thus the caller is responsible to
     * free it with VSIFree().
     * If ppBuffer is nullptr, then the compressed data itself will not be returned,
     * but *pnBufferSize will be updated with an upper bound of the size that would
     * be necessary to hold it (if pnBufferSize != nullptr).
     *
     * @param pnBufferSize Output buffer size, or nullptr.
     * If ppBuffer != nullptr && *ppBuffer != nullptr, then pnBufferSize should
     * be != nullptr and *pnBufferSize contain the size of *ppBuffer. If the
     * method is successful, *pnBufferSize will be updated with the actual size
     * used.
     *
     * @param ppszDetailedFormat Pointer to an output string, or nullptr.
     * If ppszDetailedFormat is not nullptr, then, on success, the method will
     * allocate a new string in *ppszDetailedFormat (to be freed with VSIFree())
     * *ppszDetailedFormat might contain strings like
     * "JPEG;frame_type=SOF0_baseline;bit_depth=8;num_components=3;"
     * "subsampling=4:2:0;colorspace=YCbCr" or simply the MIME type.
     * The string will contain at least as much information as what
     * GetCompressionFormats() returns, and potentially more when
     * ppBuffer != nullptr.
     *
     * @return CE_None in case of success, CE_Failure otherwise.
     * @since GDAL 3.7
     */
    CPLErr GDALDataset::ReadCompressedData(
        const char *pszFormat, int nXOff,
        int nYOff, int nXSize, int nYSize,
        int nBandCount, const int *panBandList,
        void **ppBuffer, size_t *pnBufferSize,
        char **ppszDetailedFormat);


For example, to request JPEG content on the whole image and let GDAL deal
with the buffer allocation.

.. code-block:: c++

  void* pBuffer = nullptr;
  size_t nBufferSize = 0;
  CPLErr eErr =
     poDataset->ReadCompressedData("JPEG",
                                   0, 0,
                                   poDataset->GetRasterXSize(),
                                   poDataset->GetRasterYSize(),
                                   poDataset->GetRasterCount(),
                                   nullptr, // panBandList
                                   &pBuffer,
                                   &nBufferSize,
                                   nullptr // ppszDetailedFormat
                                  );
  if (eErr == CE_None)
  {
      CPLAssert(pBuffer != nullptr);
      CPLAssert(nBufferSize > 0);
      VSILFILE* fp = VSIFOpenL("my.jpeg", "wb");
      if (fp)
      {
          VSIFWriteL(pBuffer, nBufferSize, 1, fp);
          VSIFCloseL(fp);
      }
      VSIFree(pBuffer);
  }


Or to manage the buffer allocation on your side:

.. code-block:: c++

  size_t nUpperBoundBufferSize = 0;
  CPLErr eErr =
     poDataset->ReadCompressedData("JPEG",
                                   0, 0,
                                   poDataset->GetRasterXSize(),
                                   poDataset->GetRasterYSize(),
                                   poDataset->GetRasterCount(),
                                   nullptr, // panBandList
                                   nullptr, // ppBuffer,
                                   &nUpperBoundBufferSize,
                                   nullptr // ppszDetailedFormat
                                  );
  if (eErr == CE_None)
  {
      std::vector<uint8_t> myBuffer;
      myBuffer.resize(nUpperBoundBufferSize);
      void* pBuffer = myBuffer.data();
      size_t nActualSize = nUpperBoundBufferSize;
      char* pszDetailedFormat = nullptr;
      // We also request detailed format, but we could have passed it to
      // nullptr as well.
      eErr =
        poDataset->ReadCompressedData("JPEG",
                                      0, 0,
                                      poDataset->GetRasterXSize(),
                                      poDataset->GetRasterYSize(),
                                      poDataset->GetRasterCount(),
                                      nullptr, // panBandList
                                      &pBuffer,
                                      &nActualSize,
                                      &pszDetailedFormat);
      if (eErr == CE_None)
      {
         CPLAssert(pBuffer == myBuffer.data()); // pointed value not modified
         CPLAssert(nActualSize <= nUpperBoundBufferSize);
         myBuffer.resize(nActualSize);
         // do something useful
         VSIFree(pszDetailedFormat);
      }
  }


LOSSLESS_COPY creation option
+++++++++++++++++++++++++++++

Those methods are typically used by a GDALDriver::CreateCopy() implementation
to short-circuit the nominal logic of acquiring pixels from the source and
compressing them and use instead the compressed data if available in the desired
target compression format.

Drivers that implement such short-circuit should expose a LOSSLESS_COPY creation
option, whose default value is AUTO, to mean that use of source compressed data
should be done in priority, and fallback to the regular code path otherwise.
Users might set it to YES to require the use of lossless copy, and, when it is
not possible to use it, the driver should error out.
Users might also set it to NO to ask for the regular code path to be taken.
Setting it to NO should be uncommon. This is a provision in case the
optimized code path would have bugs, or if for any other reason, the regular
code path must be taken (if the source compressed data was not fully conformant
for example).

Miscellaneous
+++++++++++++

A helper ``bool GDALDataset::IsAllBands(int nBandCount, const int *panBandList) const``
method is also added to check if (nBandCount, panBandList) requests all the
bands of the dataset.


Intended use
------------

This RFC does *not* deprecate the traditional RasterIO() usage by any means.
Its main intended users are (some) CreateCopy() implementations.

Clearly, the use of ReadCompressedData() is an advanced one, which often
requires a good understanding of some low-level characteristics of the
compression methods to be used properly (e.g. not all formulations of JPEG
codestreams are usable as a JPEG-in-TIFF or supported by libjxl in JPEG lossless
transcoding, which requires to examine the output of the new helper
GDALGetCompressionFormatForJPEG() function).

C API
-----

The 2 above C++ methods are available in the C API as
``GDALDatasetGetCompressionFormats()`` and ``GDALDatasetReadCompressedData()``.
The return of GDALDatasetGetCompressionFormats() should be freed with
:cpp:func:`CSLDestroy`.

Backward compatibility
----------------------

No backward incompatibility. Only API addition.

For driver using ReadCompressedData() in their CreateCopy() implementation,
generated files might be changed, with more frequent lossless conversions.

SWIG Bindings
-------------

The new functions will *not* be exposed to bindings currently.

Testing
-------

The scenarios covered by the below proposed implementation will be tested
by C++ unit tests (unit testing of GetCompressionFormats() and ReadCompressedData()
implementations), and in Python autotest suite for end-to-end tests (e.g lossless
JPEG -> JPEGXL -> JPEG)

Issues / pull requests
----------------------

https://github.com/OSGeo/gdal/compare/master...rouault:gdal:ReadCompressedData?expand=1 contains
a candidate implementation with the following capabilities:

- core empty implementation of GetCompressionFormats() and ReadCompressedData()

- implement ReadCompressedData() in the GeoTIFF driver for JPEG/WEBP/JPEGXL compression
  (limited to extracting a single tile for now)

- implement ReadCompressedData() in the VRT driver (limited to a single source
  for now, which forwards the call to the source)

- implement ReadCompressedData() in the JPEG driver.

- use ReadCompressedData() in the JPEG driver (with pszFormat equal to "JPEG")
  in its CreateCopy() implementation, and expose the LOSSLESS_COPY creation
  option

- implement ReadCompressedData() in the JPEGXL driver, returning both "JXL" of course,
  but also "JPEG" if the JPEGXL file includes a JPEG reconstruction box.

- use ReadCompressedData() in the JPEGXL driver, with pszFormat equal to "JPEG"
  or "JXL", in its CreateCopy() implementation, and expose the LOSSLESS_COPY
  creation option

- implement ReadCompressedData() in the WEBP driver.

- use ReadCompressedData() in the WEBP driver (with pszFormat equal to "WEBP")
  in its CreateCopy() implementation, and expose the LOSSLESS_COPY creation
  option

Given the above, the following scenarios are for example covered:

- gdal_translate -srcwin of a tile of a JPEG (resp. JPEGXL, WEBP)-compressed tiled
  GeoTIFF to JPEG (resp. JPEGXL, WEBP).
  (involves the GTiff and VRT drivers as producers, the JPEG/JPEGXL/WEBP drivers as
  consumers)

- gdal_translate of a JPEGXL file with JPEG reconstruction box to JPEG
  (involves the JPEGXL driver as producer, the JPEG driver as consumer). And
  the reverse operation: lossless conversion of JPEG to JPEGXL with a JPEG
  reconstruction box.

Voting history
--------------

+1 from PSC members JukkaR, FrankW, SeanG, MateuzL and EvenR
