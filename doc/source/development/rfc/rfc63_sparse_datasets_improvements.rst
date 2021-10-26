.. _rfc-63:

=======================================================================================
RFC 63 : Sparse datasets improvements
=======================================================================================

Author: Even Rouault

Contact: even.rouault at spatialys.com

Status: Adopted, implemented

Version: 2.2

Summary
-------

This RFC covers an improvement to manage sparse datasets, that is to say
datasets that contain substantial empty regions.

Approach
--------

There are use cases where one needs to read or generate a dataset that
covers a large spatial extent, but in which significant parts are not
covered by data. There is no way in the GDAL API to quickly know which
areas are covered or not by data, hence requiring to process all pixels,
which is rather inefficient. Whereas some formats like GeoTIFF, VRT or
GeoPackage can potentially give such an information without processing
pixels.

It is thus proposed to add a new method GetDataCoverageStatus() in the
GDALRasterBand class, that takes as input a window of interest and
returns whether it is made of data, empty blocks or a mix of them.

This method will be used by the GDALDatasetCopyWholeRaster() method
(used by CreateCopy() / gdal_translate) to avoid processing sparse
regions when the output driver instructs it to do so.

C++ API
-------

In GDALRasterBand class, a new virtual method is added :

::

    virtual int IGetDataCoverageStatus( int nXOff, int nYOff,
                                        int nXSize, int nYSize,
                                        int nMaskFlagStop,
                                        double* pdfDataPct);


   /**
    * \brief Get the coverage status of a sub-window of the raster.
    *
    * Returns whether a sub-window of the raster contains only data, only empty
    * blocks or a mix of both. This function can be used to determine quickly
    * if it is worth issuing RasterIO / ReadBlock requests in datasets that may
    * be sparse.
    *
    * Empty blocks are blocks that contain only pixels whose value is the nodata
    * value when it is set, or whose value is 0 when the nodata value is not set.
    *
    * The query is done in an efficient way without reading the actual pixel
    * values. If not possible, or not implemented at all by the driver,
    * GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED | GDAL_DATA_COVERAGE_STATUS_DATA will
    * be returned.
    *
    * The values that can be returned by the function are the following,
    * potentially combined with the binary or operator :
    * <ul>
    * <li>GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED : the driver does not implement
    * GetDataCoverageStatus(). This flag should be returned together with
    * GDAL_DATA_COVERAGE_STATUS_DATA.</li>
    * <li>GDAL_DATA_COVERAGE_STATUS_DATA: There is (potentially) data in the queried
    * window.</li>
    * <li>GDAL_DATA_COVERAGE_STATUS_EMPTY: There is nodata in the queried window.
    * This is typically identified by the concept of missing block in formats that
    * supports it.
    * </li>
    * </ul>
    *
    * Note that GDAL_DATA_COVERAGE_STATUS_DATA might have false positives and
    * should be interpreted more as hint of potential presence of data. For example
    * if a GeoTIFF file is created with blocks filled with zeroes (or set to the
    * nodata value), instead of using the missing block mechanism,
    * GDAL_DATA_COVERAGE_STATUS_DATA will be returned. On the contrary,
    * GDAL_DATA_COVERAGE_STATUS_EMPTY should have no false positives.
    *
    * The nMaskFlagStop should be generally set to 0. It can be set to a
    * binary-or'ed mask of the above mentioned values to enable a quick exiting of
    * the function as soon as the computed mask matches the nMaskFlagStop. For
    * example, you can issue a request on the whole raster with nMaskFlagStop =
    * GDAL_DATA_COVERAGE_STATUS_EMPTY. As soon as one missing block is encountered,
    * the function will exit, so that you can potentially refine the requested area
    * to find which particular region(s) have missing blocks.
    *
    * @see GDALGetDataCoverageStatus()
    *
    * @param nXOff The pixel offset to the top left corner of the region
    * of the band to be queried. This would be zero to start from the left side.
    *
    * @param nYOff The line offset to the top left corner of the region
    * of the band to be queried. This would be zero to start from the top.
    *
    * @param nXSize The width of the region of the band to be queried in pixels.
    *
    * @param nYSize The height of the region of the band to be queried in lines.
    *
    * @param nMaskFlagStop 0, or a binary-or'ed mask of possible values
    * GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED,
    * GDAL_DATA_COVERAGE_STATUS_DATA and GDAL_DATA_COVERAGE_STATUS_EMPTY. As soon
    * as the computation of the coverage matches the mask, the computation will be
    * stopped. *pdfDataPct will not be valid in that case.
    *
    * @param pdfDataPct Optional output parameter whose pointed value will be set
    * to the (approximate) percentage in [0,100] of pixels in the queried
    * sub-window that have valid values. The implementation might not always be
    * able to compute it, in which case it will be set to a negative value.
    *
    * @return a binary-or'ed combination of possible values
    * GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED,
    * GDAL_DATA_COVERAGE_STATUS_DATA and GDAL_DATA_COVERAGE_STATUS_EMPTY
    *
    * @note Added in GDAL 2.2
    */

This method has a dumb default implementation that returns
GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED \|
GDAL_DATA_COVERAGE_STATUS_DATA

The public API is made of :

::


   /** Flag returned by GDALGetDataCoverageStatus() when the driver does not
    * implement GetDataCoverageStatus(). This flag should be returned together
    * with GDAL_DATA_COVERAGE_STATUS_DATA */
   #define GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED 0x01

   /** Flag returned by GDALGetDataCoverageStatus() when there is (potentially)
    * data in the queried window. Can be combined with the binary or operator
    * with GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED or
    * GDAL_DATA_COVERAGE_STATUS_EMPTY */
   #define GDAL_DATA_COVERAGE_STATUS_DATA          0x02

   /** Flag returned by GDALGetDataCoverageStatus() when there is nodata in the
    * queried window. This is typically identified by the concept of missing block
    * in formats that supports it.
    * Can be combined with the binary or operator with
    * GDAL_DATA_COVERAGE_STATUS_DATA */
   #define GDAL_DATA_COVERAGE_STATUS_EMPTY         0x04


   C++ :

   int  GDALRasterBand::GetDataCoverageStatus( int nXOff,
                                               int nYOff,
                                               int nXSize,
                                               int nYSize,
                                               int nMaskFlagStop,
                                               double* pdfDataPct)

   C :
   int GDALGetDataCoverageStatus( GDALRasterBandH hBand,
                                  int nXOff, int nYOff,
                                  int nXSize,
                                  int nYSize,
                                  int nMaskFlagStop,
                                  double* pdfDataPct);

GDALRasterBand::GetDataCoverageStatus() does basic checks on the
validity of the window before calling IGetDataCoverageStatus()

Changes
-------

GDALDatasetCopyWholeRaster() and GDALRasterBandCopyWholeRaster() accepts
a SKIP_HOLES option that can be set to YES by the output driver to cause
GetDataCoverageStatus() to be called on each chunk of the source dataset
to determine if contains only holes or not.

Drivers
-------

This RFC upgrades the GeoTIFF and VRT drivers to implement the
IGetDataCoverageStatus() method.

The GeoTIFF driver has also receive a number of prior enhancements,
related to that topic, for example to accept the SPARSE_OK=YES creation
option in CreateCopy() mode (or the SPARSE_OK open option in update
mode).

Extract of the documentation of the driver:

::

   GDAL makes a special interpretation of a TIFF tile or strip whose offset
   and byte count are set to 0, that is to say a tile or strip that has no corresponding
   allocated physical storage. On reading, such tiles or strips are considered to
   be implicitly set to 0 or to the nodata value when it is defined. On writing, it
   is possible to enable generating such files through the Create() interface by setting
   the SPARSE_OK creation option to YES. Then, blocks that are never written
   through the IWriteBlock()/IRasterIO() interfaces will have their offset and
   byte count set to 0. This is particularly useful to save disk space and time when
   the file must be initialized empty before being passed to a further processing
   stage that will fill it.
   To avoid ambiguities with another sparse mechanism discussed in the next paragraphs,
   we will call such files with implicit tiles/strips "TIFF sparse files". They will
   be likely *not* interoperable with TIFF readers that are not GDAL based and
   would consider such files with implicit tiles/strips as defective.

   Starting with GDAL 2.2, this mechanism is extended to the CreateCopy() and
   Open() interfaces (for update mode) as well. If the SPARSE_OK creation option
   (or the SPARSE_OK open option for Open()) is set to YES, even an attempt to
   write a all 0/nodata block will be detected so that the tile/strip is not
   allocated (if it was already allocated, then its content will be replaced by
   the 0/nodata content).

   Starting with GDAL 2.2, in the case where SPARSE_OK is *not* defined (or set
   to its default value FALSE), for uncompressed files whose nodata value is not
   set, or set to 0, in Create() and CreateCopy() mode, the driver will delay the
   allocation of 0-blocks until file closing, so as to be able to write them at
   the very end of the file, and in a way compatible of the filesystem sparse file
   mechanisms (to be distinguished from the TIFF sparse file extension discussed
   earlier). That is that all the empty blocks will be seen as properly allocated
   from the TIFF point of view (corresponding strips/tiles will have valid offsets
   and byte counts), but will have no corresponding physical storage. Provided that
   the filesystem supports such sparse files, which is the case for most Linux
   popular filesystems (ext2/3/4, xfs, btfs, ...) or NTFS on Windows. If the file
   system does not support sparse files, physical storage will be
   allocated and filled with zeros.

Bindings
--------

The Python bindings has a mapping of GDALGetDataCoverageStatus(). Other
bindings could be updated (need to figure out how to return both a
status flag and a percentage)

Utilities
---------

No direct changes in utilities.

Results
-------

With this new capability, a VRT of size 200 000 x 200 000 pixels that
contains 2 regions of 20x20 pixels each can be gdal_translated as a
sparse tiled GeoTIFF in 2 seconds. The resulting GeoTIFF can be itself
translated into another sparse tiled GeoTIFF in the same time.

Future work
-----------

Future work using the new capability could be done in overview building
or warping. Other drivers could also benefit from that new capability:
GeoPackage, ERDAS Imagine, ...

Documentation
-------------

The new method is documented.

Test Suite
----------

Tests of the VRT and GeoTIFF drivers are enhanced to test their
IGetDataCoverageStatus() implementation.

Compatibility Issues
--------------------

C++ ABI change. No functional incompatibility foreseen.

Implementation
--------------

The implementation will be done by Even Rouault.

The proposed implementation is in
`https://github.com/rouault/gdal2/tree/sparse_datasets <https://github.com/rouault/gdal2/tree/sparse_datasets>`__

Changes can be seen with
`https://github.com/OSGeo/gdal/compare/trunk...rouault:sparse_datasets?expand=1 <https://github.com/OSGeo/gdal/compare/trunk...rouault:sparse_datasets?expand=1>`__

Voting history
--------------

+1 from EvenR and DanielM
