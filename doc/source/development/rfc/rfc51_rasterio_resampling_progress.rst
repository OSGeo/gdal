.. _rfc-51:

=======================================================================================
RFC 51: RasterIO() improvements : resampling and progress callback
=======================================================================================

Author: Even Rouault

Contact: even dot rouault at spatialys dot com

Status: Adopted, implemented in GDAL 2.0

Summary
-------

This RFC aims at extending the RasterIO() API to allow specifying a
resampling algorithm when doing requests involving subsampling or
oversampling. A progress callback can also be specified to be notified
of progression and allow the user to interrupt the operation.

Core changes
------------

Addition of GDALRasterIOExtraArg structure
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A new structure GDALRasterIOExtraArg is added to contain the new
options.

::

   /** Structure to pass extra arguments to RasterIO() method
     * @since GDAL 2.0
     */
   typedef struct
   {
       /*! Version of structure (to allow future extensions of the structure) */ 
       int                    nVersion;

       /*! Resampling algorithm */ 
       GDALRIOResampleAlg     eResampleAlg;

       /*! Progress callback */ 
       GDALProgressFunc       pfnProgress;
       /*! Progress callback user data */ 
       void                  *pProgressData;

       /*! Indicate if dfXOff, dfYOff, dfXSize and dfYSize are set.
           Mostly reserved from the VRT driver to communicate a more precise
           source window. Must be such that dfXOff - nXOff < 1.0 and
           dfYOff - nYOff < 1.0 and nXSize - dfXSize < 1.0 and nYSize - dfYSize < 1.0 */
       int                    bFloatingPointWindowValidity;
       /*! Pixel offset to the top left corner. Only valid if bFloatingPointWindowValidity = TRUE */
       double                 dfXOff;
       /*! Line offset to the top left corner. Only valid if bFloatingPointWindowValidity = TRUE */
       double                 dfYOff;
       /*! Width in pixels of the area of interest. Only valid if bFloatingPointWindowValidity = TRUE */
       double                 dfXSize;
       /*! Height in pixels of the area of interest. Only valid if bFloatingPointWindowValidity = TRUE */
       double                 dfYSize;
   } GDALRasterIOExtraArg;

   #define RASTERIO_EXTRA_ARG_CURRENT_VERSION  1

   /** Macro to initialize an instance of GDALRasterIOExtraArg structure.
     * @since GDAL 2.0
     */
   #define INIT_RASTERIO_EXTRA_ARG(s)  \
       do { (s).nVersion = RASTERIO_EXTRA_ARG_CURRENT_VERSION; \
            (s).eResampleAlg = GRIORA_NearestNeighbour; \
            (s).pfnProgress = NULL; \
            (s).pProgressData = NULL; \
            (s).bFloatingPointWindowValidity = FALSE; } while(0)

There are several reasons to prefer a structure rather than new
parameters to the RasterIO() methods :

-  code readability (GDALDataset::IRasterIO() has already 14
   parameters...)
-  allow future extensions without changing the prototype in all drivers
-  to a lesser extent, efficiency: it is common for RasterIO() calls to
   be chained between generic/specific and/or dataset/rasterband
   implementations. Passing just the pointer is more efficient.

The structure is versioned. In the future if further options are added,
the new members will be added at the end of the structure and the
version number will be incremented. Code in GDAL core&drivers can check
the version number to determine which options are available.

Addition of GDALRIOResampleAlg structure
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following resampling algorithms are available :

::

   /** RasterIO() resampling method.
     * @since GDAL 2.0
     */
   typedef enum
   {
       /*! Nearest neighbour */                            GRIORA_NearestNeighbour = 0,
       /*! Bilinear (2x2 kernel) */                        GRIORA_Bilinear = 1,
       /*! Cubic Convolution Approximation (4x4 kernel) */ GRIORA_Cubic = 2,
       /*! Cubic B-Spline Approximation (4x4 kernel) */    GRIORA_CubicSpline = 3,
       /*! Lanczos windowed sinc interpolation (6x6 kernel) */ GRIORA_Lanczos = 4,
       /*! Average */                                      GRIORA_Average = 5,
       /*! Mode (selects the value which appears most often of all the sampled points) */
                                                           GRIORA_Mode = 6,
       /*! Gauss blurring */                               GRIORA_Gauss = 7
   } GDALRIOResampleAlg;

Those new resampling methods can be used by the
GDALRasterBand::IRasterIO() default implementation when the size of the
buffer (nBufXSize x nBufYSize) is different from the size of the area of
interest (nXSize x nYSize). The code heavily relies on the algorithms
used for overview computation, with adjustments to be also able to deal
with oversampling. Bilinear, CubicSpline and Lanczos are now available
in overview computation as well, and rely on the generic infrastructure
for convolution computation introduced lately for improved cubic
overviews. Some algorithms are not available on raster bands with color
palette. A warning will be emitted if an attempt of doing so is done,
and nearest neighbour will be used as a fallback.

The GDAL_RASTERIO_RESAMPLING configuration option can be set as an
alternate way of specifying the resampling algorithm. Mainly useful for
tests with applications that do not yet use the new API.

Currently, the new resampling methods are only available for GF_Read
operations. The use case for GF_Write operations isn't obvious, but
could be added without API changes if needed.

C++ changes
~~~~~~~~~~~

GDALDataset and GDALRasterBand (non virtual) RasterIO() and (virtual)
IRasterIO() methods have a new final argument psExtraArg of type
GDALRasterIOExtraArg*. This extra argument defaults to NULL for code
using GDAL, but is required for all in-tree code, so as to avoid that
in-tree code forgets to forwards psExtraArg it might have returned from
a caller.

GDALDataset::RasterIO() and GDALRasterBand::RasterIO() can accept a NULL
pointer for that argument in which case they will instantiate a default
GDALRasterIOExtraArg structure to be passed to IRasterIO(). Any other
code that calls IRasterIO() directly (a few IReadBlock()
implementations) should make sure of doing so, so that IRasterIO() can
assume that its psExtraArg is not NULL.

As a provision to be able to deal with very large requests with buffers
larger than several gigabytes, the nPixelSpace, nLineSpace and
nBandSpace parameters have been promoted from the int datatype to the
new GSpacing datatype, which is an alias of a signed 64 bit integer.

GDALRasterBand::IRasterIO() and GDALDataset::BlockBasedRasterIO() now
use the progress callback when available.

C API changes
~~~~~~~~~~~~~

Only additions :

::

   CPLErr CPL_DLL CPL_STDCALL GDALDatasetRasterIOEx( 
       GDALDatasetH hDS, GDALRWFlag eRWFlag,
       int nDSXOff, int nDSYOff, int nDSXSize, int nDSYSize,
       void * pBuffer, int nBXSize, int nBYSize, GDALDataType eBDataType,
       int nBandCount, int *panBandCount, 
       GSpacing nPixelSpace, GSpacing nLineSpace, GSpacing nBandSpace,
       GDALRasterIOExtraArg* psExtraArg);

   CPLErr CPL_DLL CPL_STDCALL 
   GDALRasterIOEx( GDALRasterBandH hRBand, GDALRWFlag eRWFlag,
                   int nDSXOff, int nDSYOff, int nDSXSize, int nDSYSize,
                   void * pBuffer, int nBXSize, int nBYSize,GDALDataType eBDataType,
                   GSpacing nPixelSpace, GSpacing nLineSpace,
                   GDALRasterIOExtraArg* psExtraArg );

Those are the same as the existing functions with a final
GDALRasterIOExtraArg\* psExtraArg argument, and the spacing parameters
promoted to GSpacing.

Changes in drivers
------------------

-  All in-tree drivers that implemented or used RasterIO have been
   edited to accept the GDALRasterIOExtraArg\* psExtraArg parameter, and
   forward it when needed. Those who had a custom RasterIO()
   implementation now use the progress callback when available.
-  VRT: the and elements can accept a 'resampling' attribute. The VRT
   driver will also set the dfXOff, dfYOff, dfXSize and dfYSize fields
   of GDALRasterIOExtraArg\* to have source sub-pixel accuracy, so that
   GDALRasterBand::IRasterIO() leads to consistent results when
   operating on a small area of interest or the whole raster. If that
   was not done, chunking done in GDALDatasetCopyWholeRaster() or other
   algorithms could lead to repeated lines due to integer rounding
   issues.

Changes in utilities
--------------------

-  gdal_translate: accept a -r parameter to specify the resampling
   algorithm. Defaults to NEAR. Can be set to bilinear, cubic,
   cubicspline, lanczos, average or mode. (Under the hood, this sets the
   new resampling property at the VRT source level.)
-  gdaladdo: -r parameter now accepts bilinear, cubicspline and lanczos
   as additional algorithms to the existing ones.

Changes in SWIG bindings
------------------------

-  For Python and Perl bindings: Band.ReadRaster(), Dataset.ReadRaster()
   now accept optional resample_alg, callback and callback_data
   arguments. (untested for Perl, but the existing tests pass)
-  For Python bindings, Band.ReadAsArray() and Dataset.ReadAsArray() now
   accept optional resample_alg, callback and callback_data arguments.

Compatibility
-------------

-  C API/ABI preserved.

-  C++ users of the GDALRasterBand::RasterIO() and
   GDALDataset::RasterIO() API do not need to change their code, since
   the new GDALRasterIOExtraArg\* psExtraArg argument is optional for
   out-of-tree code.

-  Out-of-tree drivers that implement IRasterIO() must be changed to
   accept the new GDALRasterIOExtraArg\* psExtraArg argument. Note:
   failing to do so will be undetected at compile time (due to how C++
   virtual method overloading work).

Both issues will be mentioned in MIGRATION_GUIDE.TXT

Documentation
-------------

All new methods are documented.

Testing
-------

The various aspects of this RFC are tested in the Python bindings:

-  use of the new options of Band.ReadRaster(), Dataset.ReadRaster(),
   Band.ReadAsArray() and Dataset.ReadAsArray().
-  resampling algorithms in subsampling and oversampling RasterIO()
   requests.
-  "-r" option of gdal_translate

Implementation
--------------

Implementation will be done by Even Rouault
(`Spatialys <http://spatialys.com>`__), and sponsored by `R3
GIS <http://r3-gis.com>`__.

The proposed implementation lies in the "rasterio" branch of the
`https://github.com/rouault/gdal2/tree/rasterio <https://github.com/rouault/gdal2/tree/rasterio>`__
repository.

The list of changes :
`https://github.com/rouault/gdal2/compare/rasterio <https://github.com/rouault/gdal2/compare/rasterio>`__

Voting history
--------------

+1 from FrankW, JukkaR, HowardB, DanielM, TamasS and EvenR
