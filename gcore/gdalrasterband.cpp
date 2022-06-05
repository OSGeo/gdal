/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Base class for format specific band class implementation.  This
 *           base class provides default implementation for many methods.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2016, Even Rouault <even dot rouault at spatialys dot com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_port.h"
#include "gdal_priv.h"

#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_virtualmem.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_rat.h"
#include "gdal_priv_templates.hpp"

CPL_CVSID("$Id$")

/************************************************************************/
/*                           GDALRasterBand()                           */
/************************************************************************/

/*! Constructor. Applications should never create GDALRasterBands directly. */

GDALRasterBand::GDALRasterBand() :
    GDALRasterBand(CPLTestBool( CPLGetConfigOption( "GDAL_FORCE_CACHING", "NO") ) )
{
}

/** Constructor. Applications should never create GDALRasterBands directly.
 * @param bForceCachedIOIn Whether cached IO should be forced.
 */
GDALRasterBand::GDALRasterBand(int bForceCachedIOIn):
    bForceCachedIO(bForceCachedIOIn)

{
}

/************************************************************************/
/*                          ~GDALRasterBand()                           */
/************************************************************************/

/*! Destructor. Applications should never destroy GDALRasterBands directly,
    instead destroy the GDALDataset. */

GDALRasterBand::~GDALRasterBand()

{
    if( poDS && poDS->bSuppressOnClose )
    {
        if( poBandBlockCache )
            poBandBlockCache->DisableDirtyBlockWriting();
    }
    GDALRasterBand::FlushCache(true);

    delete poBandBlockCache;

    if( static_cast<GIntBig>(nBlockReads) > static_cast<GIntBig>(nBlocksPerRow) * nBlocksPerColumn
        && nBand == 1 && poDS != nullptr )
    {
        CPLDebug( "GDAL", "%d block reads on %d block band 1 of %s.",
                  nBlockReads, nBlocksPerRow * nBlocksPerColumn,
                  poDS->GetDescription() );
    }

    InvalidateMaskBand();
    nBand = -nBand;
}

/************************************************************************/
/*                              RasterIO()                              */
/************************************************************************/

/**
 * \fn GDALRasterBand::IRasterIO( GDALRWFlag eRWFlag,
 *                                int nXOff, int nYOff, int nXSize, int nYSize,
 *                                void * pData, int nBufXSize, int nBufYSize,
 *                                GDALDataType eBufType,
 *                                GSpacing nPixelSpace,
 *                                GSpacing nLineSpace,
 *                                GDALRasterIOExtraArg* psExtraArg )
 * \brief Read/write a region of image data for this band.
 *
 * This method allows reading a region of a GDALRasterBand into a buffer,
 * or writing data from a buffer into a region of a GDALRasterBand. It
 * automatically takes care of data type translation if the data type
 * (eBufType) of the buffer is different than that of the GDALRasterBand.
 * The method also takes care of image decimation / replication if the
 * buffer size (nBufXSize x nBufYSize) is different than the size of the
 * region being accessed (nXSize x nYSize).
 *
 * The nPixelSpace and nLineSpace parameters allow reading into or
 * writing from unusually organized buffers. This is primarily used
 * for buffers containing more than one bands raster data in interleaved
 * format.
 *
 * Some formats may efficiently implement decimation into a buffer by
 * reading from lower resolution overview images.
 *
 * For highest performance full resolution data access, read and write
 * on "block boundaries" as returned by GetBlockSize(), or use the
 * ReadBlock() and WriteBlock() methods.
 *
 * This method is the same as the C GDALRasterIO() or GDALRasterIOEx() functions.
 *
 * @param eRWFlag Either GF_Read to read a region of data, or GF_Write to
 * write a region of data.
 *
 * @param nXOff The pixel offset to the top left corner of the region
 * of the band to be accessed. This would be zero to start from the left side.
 *
 * @param nYOff The line offset to the top left corner of the region
 * of the band to be accessed. This would be zero to start from the top.
 *
 * @param nXSize The width of the region of the band to be accessed in pixels.
 *
 * @param nYSize The height of the region of the band to be accessed in lines.
 *
 * @param pData The buffer into which the data should be read, or from which
 * it should be written. This buffer must contain at least nBufXSize *
 * nBufYSize words of type eBufType. It is organized in left to right,
 * top to bottom pixel order. Spacing is controlled by the nPixelSpace,
 * and nLineSpace parameters.
 *
 * @param nBufXSize the width of the buffer image into which the desired region is
 * to be read, or from which it is to be written.
 *
 * @param nBufYSize the height of the buffer image into which the desired region is
 * to be read, or from which it is to be written.
 *
 * @param eBufType the type of the pixel values in the pData data buffer. The
 * pixel values will automatically be translated to/from the GDALRasterBand
 * data type as needed.
 *
 * @param nPixelSpace The byte offset from the start of one pixel value in
 * pData to the start of the next pixel value within a scanline. If defaulted
 * (0) the size of the datatype eBufType is used.
 *
 * @param nLineSpace The byte offset from the start of one scanline in
 * pData to the start of the next. If defaulted (0) the size of the datatype
 * eBufType * nBufXSize is used.
 *
 * @param psExtraArg (new in GDAL 2.0) pointer to a GDALRasterIOExtraArg structure with additional
 * arguments to specify resampling and progress callback, or NULL for default
 * behavior. The GDAL_RASTERIO_RESAMPLING configuration option can also be defined
 * to override the default resampling to one of BILINEAR, CUBIC, CUBICSPLINE,
 * LANCZOS, AVERAGE or MODE.
 *
 * @return CE_Failure if the access fails, otherwise CE_None.
 */

/**
 * \brief Read/write a region of image data for this band.
 *
 * This method allows reading a region of a GDALRasterBand into a buffer,
 * or writing data from a buffer into a region of a GDALRasterBand. It
 * automatically takes care of data type translation if the data type
 * (eBufType) of the buffer is different than that of the GDALRasterBand.
 * The method also takes care of image decimation / replication if the
 * buffer size (nBufXSize x nBufYSize) is different than the size of the
 * region being accessed (nXSize x nYSize).
 *
 * The nPixelSpace and nLineSpace parameters allow reading into or
 * writing from unusually organized buffers. This is primarily used
 * for buffers containing more than one bands raster data in interleaved
 * format.
 *
 * Some formats may efficiently implement decimation into a buffer by
 * reading from lower resolution overview images.
 *
 * For highest performance full resolution data access, read and write
 * on "block boundaries" as returned by GetBlockSize(), or use the
 * ReadBlock() and WriteBlock() methods.
 *
 * This method is the same as the C GDALRasterIO() or GDALRasterIOEx() functions.
 *
 * @param eRWFlag Either GF_Read to read a region of data, or GF_Write to
 * write a region of data.
 *
 * @param nXOff The pixel offset to the top left corner of the region
 * of the band to be accessed. This would be zero to start from the left side.
 *
 * @param nYOff The line offset to the top left corner of the region
 * of the band to be accessed. This would be zero to start from the top.
 *
 * @param nXSize The width of the region of the band to be accessed in pixels.
 *
 * @param nYSize The height of the region of the band to be accessed in lines.
 *
 * @param[in,out] pData The buffer into which the data should be read, or from which
 * it should be written. This buffer must contain at least nBufXSize *
 * nBufYSize words of type eBufType. It is organized in left to right,
 * top to bottom pixel order. Spacing is controlled by the nPixelSpace,
 * and nLineSpace parameters.
 *
 * @param nBufXSize the width of the buffer image into which the desired region is
 * to be read, or from which it is to be written.
 *
 * @param nBufYSize the height of the buffer image into which the desired region is
 * to be read, or from which it is to be written.
 *
 * @param eBufType the type of the pixel values in the pData data buffer. The
 * pixel values will automatically be translated to/from the GDALRasterBand
 * data type as needed.
 *
 * @param nPixelSpace The byte offset from the start of one pixel value in
 * pData to the start of the next pixel value within a scanline. If defaulted
 * (0) the size of the datatype eBufType is used.
 *
 * @param nLineSpace The byte offset from the start of one scanline in
 * pData to the start of the next. If defaulted (0) the size of the datatype
 * eBufType * nBufXSize is used.
 *
 * @param[in] psExtraArg (new in GDAL 2.0) pointer to a GDALRasterIOExtraArg structure with additional
 * arguments to specify resampling and progress callback, or NULL for default
 * behavior. The GDAL_RASTERIO_RESAMPLING configuration option can also be defined
 * to override the default resampling to one of BILINEAR, CUBIC, CUBICSPLINE,
 * LANCZOS, AVERAGE or MODE.
 *
 * @return CE_Failure if the access fails, otherwise CE_None.
 */

CPLErr GDALRasterBand::RasterIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 GSpacing nPixelSpace,
                                 GSpacing nLineSpace,
                                 GDALRasterIOExtraArg* psExtraArg )

{
    GDALRasterIOExtraArg sExtraArg;
    if( psExtraArg == nullptr )
    {
        INIT_RASTERIO_EXTRA_ARG(sExtraArg);
        psExtraArg = &sExtraArg;
    }
    else if( psExtraArg->nVersion != RASTERIO_EXTRA_ARG_CURRENT_VERSION )
    {
        ReportError( CE_Failure, CPLE_AppDefined,
                     "Unhandled version of GDALRasterIOExtraArg" );
        return CE_Failure;
    }

    GDALRasterIOExtraArgSetResampleAlg(psExtraArg, nXSize, nYSize,
                                       nBufXSize, nBufYSize);

    if( nullptr == pData )
    {
        ReportError( CE_Failure, CPLE_AppDefined,
                  "The buffer into which the data should be read is null" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Some size values are "noop".  Lets just return to avoid         */
/*      stressing lower level functions.                                */
/* -------------------------------------------------------------------- */
    if( nXSize < 1 || nYSize < 1 || nBufXSize < 1 || nBufYSize < 1 )
    {
        CPLDebug( "GDAL",
                  "RasterIO() skipped for odd window or buffer size.\n"
                  "  Window = (%d,%d)x%dx%d\n"
                  "  Buffer = %dx%d\n",
                  nXOff, nYOff, nXSize, nYSize,
                  nBufXSize, nBufYSize );

        return CE_None;
    }

    if( eRWFlag == GF_Write )
    {
        if( eFlushBlockErr != CE_None )
        {
            ReportError(eFlushBlockErr, CPLE_AppDefined,
                        "An error occurred while writing a dirty block "
                        "from GDALRasterBand::RasterIO");
            CPLErr eErr = eFlushBlockErr;
            eFlushBlockErr = CE_None;
            return eErr;
        }
        if( eAccess != GA_Update )
        {
            ReportError( CE_Failure, CPLE_AppDefined,
                        "Write operation not permitted on dataset opened "
                        "in read-only mode" );
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      If pixel and line spacing are defaulted assign reasonable      */
/*      value assuming a packed buffer.                                 */
/* -------------------------------------------------------------------- */
    if( nPixelSpace == 0 )
    {
        nPixelSpace = GDALGetDataTypeSizeBytes( eBufType );
    }

    if( nLineSpace == 0 )
    {
        nLineSpace = nPixelSpace * nBufXSize;
    }

/* -------------------------------------------------------------------- */
/*      Do some validation of parameters.                               */
/* -------------------------------------------------------------------- */
    if( nXOff < 0 || nXOff > INT_MAX - nXSize || nXOff + nXSize > nRasterXSize
        || nYOff < 0 || nYOff > INT_MAX - nYSize || nYOff + nYSize > nRasterYSize )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                  "Access window out of range in RasterIO().  Requested\n"
                  "(%d,%d) of size %dx%d on raster of %dx%d.",
                  nXOff, nYOff, nXSize, nYSize, nRasterXSize, nRasterYSize );
        return CE_Failure;
    }

    if( eRWFlag != GF_Read && eRWFlag != GF_Write )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                  "eRWFlag = %d, only GF_Read (0) and GF_Write (1) are legal.",
                  eRWFlag );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Call the format specific function.                              */
/* -------------------------------------------------------------------- */

    const bool bCallLeaveReadWrite = CPL_TO_BOOL(EnterReadWrite(eRWFlag));

    CPLErr eErr;
    if( bForceCachedIO )
        eErr = GDALRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                         pData, nBufXSize, nBufYSize, eBufType,
                                         nPixelSpace, nLineSpace, psExtraArg );
    else
        eErr = IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                          pData, nBufXSize, nBufYSize, eBufType,
                          nPixelSpace, nLineSpace, psExtraArg ) ;

    if( bCallLeaveReadWrite) LeaveReadWrite();

    return eErr;
}

/************************************************************************/
/*                            GDALRasterIO()                            */
/************************************************************************/

/**
 * \brief Read/write a region of image data for this band.
 *
 * Use GDALRasterIOEx() if 64 bit spacings or extra arguments (resampling
 * resolution, progress callback, etc. are needed)
 *
 * @see GDALRasterBand::RasterIO()
 */

CPLErr CPL_STDCALL
GDALRasterIO( GDALRasterBandH hBand, GDALRWFlag eRWFlag,
              int nXOff, int nYOff, int nXSize, int nYSize,
              void * pData, int nBufXSize, int nBufYSize,
              GDALDataType eBufType,
              int nPixelSpace, int nLineSpace )

{
    VALIDATE_POINTER1( hBand, "GDALRasterIO", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);

    return( poBand->RasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                              pData, nBufXSize, nBufYSize, eBufType,
                              nPixelSpace, nLineSpace, nullptr) );
}

/************************************************************************/
/*                            GDALRasterIOEx()                          */
/************************************************************************/

/**
 * \brief Read/write a region of image data for this band.
 *
 * @see GDALRasterBand::RasterIO()
 * @since GDAL 2.0
 */

CPLErr CPL_STDCALL
GDALRasterIOEx( GDALRasterBandH hBand, GDALRWFlag eRWFlag,
              int nXOff, int nYOff, int nXSize, int nYSize,
              void * pData, int nBufXSize, int nBufYSize,
              GDALDataType eBufType,
              GSpacing nPixelSpace, GSpacing nLineSpace,
              GDALRasterIOExtraArg* psExtraArg )

{
    VALIDATE_POINTER1( hBand, "GDALRasterIOEx", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);

    return( poBand->RasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                              pData, nBufXSize, nBufYSize, eBufType,
                              nPixelSpace, nLineSpace, psExtraArg) );
}
/************************************************************************/
/*                             ReadBlock()                              */
/************************************************************************/

/**
 * \brief Read a block of image data efficiently.
 *
 * This method accesses a "natural" block from the raster band without
 * resampling, or data type conversion.  For a more generalized, but
 * potentially less efficient access use RasterIO().
 *
 * This method is the same as the C GDALReadBlock() function.
 *
 * See the GetLockedBlockRef() method for a way of accessing internally cached
 * block oriented data without an extra copy into an application buffer.
 *
 * The following code would efficiently compute a histogram of eight bit
 * raster data.  Note that the final block may be partial ... data beyond
 * the edge of the underlying raster band in these edge blocks is of an
 * undetermined value.
 *
\code{.cpp}
 CPLErr GetHistogram( GDALRasterBand *poBand, GUIntBig *panHistogram )

 {
     memset( panHistogram, 0, sizeof(GUIntBig) * 256 );

     CPLAssert( poBand->GetRasterDataType() == GDT_Byte );

     int nXBlockSize, nYBlockSize;

     poBand->GetBlockSize( &nXBlockSize, &nYBlockSize );
     int nXBlocks = (poBand->GetXSize() + nXBlockSize - 1) / nXBlockSize;
     int nYBlocks = (poBand->GetYSize() + nYBlockSize - 1) / nYBlockSize;

     GByte *pabyData = (GByte *) CPLMalloc(nXBlockSize * nYBlockSize);

     for( int iYBlock = 0; iYBlock < nYBlocks; iYBlock++ )
     {
         for( int iXBlock = 0; iXBlock < nXBlocks; iXBlock++ )
         {
             int        nXValid, nYValid;

             poBand->ReadBlock( iXBlock, iYBlock, pabyData );

             // Compute the portion of the block that is valid
             // for partial edge blocks.
             poBand->GetActualBlockSize(iXBlock, iYBlock, &nXValid, &nYValid)

             // Collect the histogram counts.
             for( int iY = 0; iY < nYValid; iY++ )
             {
                 for( int iX = 0; iX < nXValid; iX++ )
                 {
                     panHistogram[pabyData[iX + iY * nXBlockSize]] += 1;
                 }
             }
         }
     }
 }
\endcode
 *
 * @param nXBlockOff the horizontal block offset, with zero indicating
 * the left most block, 1 the next block and so forth.
 *
 * @param nYBlockOff the vertical block offset, with zero indicating
 * the top most block, 1 the next block and so forth.
 *
 * @param pImage the buffer into which the data will be read.  The buffer
 * must be large enough to hold GetBlockXSize()*GetBlockYSize() words
 * of type GetRasterDataType().
 *
 * @return CE_None on success or CE_Failure on an error.
 */

CPLErr GDALRasterBand::ReadBlock( int nXBlockOff, int nYBlockOff,
                                   void * pImage )

{
/* -------------------------------------------------------------------- */
/*      Validate arguments.                                             */
/* -------------------------------------------------------------------- */
    CPLAssert( pImage != nullptr );

    if( !InitBlockInfo() )
        return CE_Failure;

    if( nXBlockOff < 0 || nXBlockOff >= nBlocksPerRow )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nXBlockOff value (%d) in "
                        "GDALRasterBand::ReadBlock()\n",
                  nXBlockOff );

        return( CE_Failure );
    }

    if( nYBlockOff < 0 || nYBlockOff >= nBlocksPerColumn )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nYBlockOff value (%d) in "
                        "GDALRasterBand::ReadBlock()\n",
                  nYBlockOff );

        return( CE_Failure );
    }

/* -------------------------------------------------------------------- */
/*      Invoke underlying implementation method.                        */
/* -------------------------------------------------------------------- */

    int bCallLeaveReadWrite = EnterReadWrite(GF_Read);
    CPLErr eErr = IReadBlock( nXBlockOff, nYBlockOff, pImage );
    if( bCallLeaveReadWrite) LeaveReadWrite();
    return eErr;
}

/************************************************************************/
/*                           GDALReadBlock()                            */
/************************************************************************/

/**
 * \brief Read a block of image data efficiently.
 *
 * @see GDALRasterBand::ReadBlock()
 */

CPLErr CPL_STDCALL GDALReadBlock( GDALRasterBandH hBand, int nXOff, int nYOff,
                      void * pData )

{
    VALIDATE_POINTER1( hBand, "GDALReadBlock", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return( poBand->ReadBlock( nXOff, nYOff, pData ) );
}

/************************************************************************/
/*                            IReadBlock()                             */
/************************************************************************/

/** \fn GDALRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff, void *pData )
 * \brief Read a block of data.
 *
 * Default internal implementation ... to be overridden by
 * subclasses that support reading.
 * @param nBlockXOff Block X Offset
 * @param nBlockYOff Block Y Offset
 * @param pData Pixel buffer into which to place read data.
 * @return CE_None on success or CE_Failure on an error.
 */

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

/**
 * \fn GDALRasterBand::IWriteBlock(int, int, void*)
 * Write a block of data.
 *
 * Default internal implementation ... to be overridden by
 * subclasses that support writing.
 * @param nBlockXOff Block X Offset
 * @param nBlockYOff Block Y Offset
 * @param pData Pixel buffer to write
 * @return CE_None on success or CE_Failure on an error.
 */

/**/
/**/

CPLErr GDALRasterBand::IWriteBlock( int /*nBlockXOff*/,
                                    int /*nBlockYOff*/,
                                    void * /*pData*/ )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError( CE_Failure, CPLE_NotSupported,
                  "WriteBlock() not supported for this dataset." );

    return( CE_Failure );
}

/************************************************************************/
/*                             WriteBlock()                             */
/************************************************************************/

/**
 * \brief Write a block of image data efficiently.
 *
 * This method accesses a "natural" block from the raster band without
 * resampling, or data type conversion.  For a more generalized, but
 * potentially less efficient access use RasterIO().
 *
 * This method is the same as the C GDALWriteBlock() function.
 *
 * See ReadBlock() for an example of block oriented data access.
 *
 * @param nXBlockOff the horizontal block offset, with zero indicating
 * the left most block, 1 the next block and so forth.
 *
 * @param nYBlockOff the vertical block offset, with zero indicating
 * the left most block, 1 the next block and so forth.
 *
 * @param pImage the buffer from which the data will be written.  The buffer
 * must be large enough to hold GetBlockXSize()*GetBlockYSize() words
 * of type GetRasterDataType().
 *
 * @return CE_None on success or CE_Failure on an error.
 */

CPLErr GDALRasterBand::WriteBlock( int nXBlockOff, int nYBlockOff,
                                   void * pImage )

{
/* -------------------------------------------------------------------- */
/*      Validate arguments.                                             */
/* -------------------------------------------------------------------- */
    CPLAssert( pImage != nullptr );

    if( !InitBlockInfo() )
        return CE_Failure;

    if( nXBlockOff < 0 || nXBlockOff >= nBlocksPerRow )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nXBlockOff value (%d) in "
                        "GDALRasterBand::WriteBlock()\n",
                  nXBlockOff );

        return( CE_Failure );
    }

    if( nYBlockOff < 0 || nYBlockOff >= nBlocksPerColumn )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nYBlockOff value (%d) in "
                        "GDALRasterBand::WriteBlock()\n",
                  nYBlockOff );

        return( CE_Failure );
    }

    if( eAccess == GA_ReadOnly )
    {
        ReportError( CE_Failure, CPLE_NoWriteAccess,
                  "Attempt to write to read only dataset in"
                  "GDALRasterBand::WriteBlock().\n" );

        return( CE_Failure );
    }

    if( eFlushBlockErr != CE_None )
    {
        ReportError(eFlushBlockErr, CPLE_AppDefined,
                    "An error occurred while writing a dirty block "
                    "from GDALRasterBand::WriteBlock");
        CPLErr eErr = eFlushBlockErr;
        eFlushBlockErr = CE_None;
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Invoke underlying implementation method.                        */
/* -------------------------------------------------------------------- */

    const bool bCallLeaveReadWrite = CPL_TO_BOOL(EnterReadWrite(GF_Write));
    CPLErr eErr = IWriteBlock( nXBlockOff, nYBlockOff, pImage );
    if( bCallLeaveReadWrite ) LeaveReadWrite();

    return eErr;
}

/************************************************************************/
/*                           GDALWriteBlock()                           */
/************************************************************************/

/**
 * \brief Write a block of image data efficiently.
 *
 * @see GDALRasterBand::WriteBlock()
 */

CPLErr CPL_STDCALL GDALWriteBlock( GDALRasterBandH hBand, int nXOff, int nYOff,
                       void * pData )

{
    VALIDATE_POINTER1( hBand, "GDALWriteBlock", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle( hBand );
    return( poBand->WriteBlock( nXOff, nYOff, pData ) );
}

/************************************************************************/
/*                         GetActualBlockSize()                         */
/************************************************************************/
/**
* \brief Fetch the actual block size for a given block offset.
*
* Handles partial blocks at the edges of the raster and returns the true
* number of pixels
*
* @param nXBlockOff the horizontal block offset for which to calculate the number of
* valid pixels, with zero indicating the left most block, 1 the next block and so forth.
*
* @param nYBlockOff the vertical block offset, with zero indicating
* the left most block, 1 the next block and so forth.
*
* @param pnXValid pointer to an integer in which the number of valid pixels in the x
* direction will be stored
*
* @param pnYValid pointer to an integer in which the number of valid pixels in the y
* direction will be stored
*
* @return CE_None if the input parameter are valid, CE_Failure otherwise
*
* @since GDAL 2.2
*/
CPLErr GDALRasterBand::GetActualBlockSize(int nXBlockOff, int nYBlockOff,
                                          int *pnXValid, int *pnYValid)
{
    if( nXBlockOff < 0 || nBlockXSize == 0 ||
        nXBlockOff >= nRasterXSize / nBlockXSize + ((nRasterXSize % nBlockXSize) ? 1 : 0) ||
        nYBlockOff < 0 || nBlockYSize == 0 ||
        nYBlockOff >= nRasterYSize / nBlockYSize + ((nRasterYSize % nBlockYSize) ? 1 : 0) )
    {
        return CE_Failure;
    }

    int nXPixelOff = nXBlockOff * nBlockXSize;
    int nYPixelOff = nYBlockOff * nBlockYSize;

    *pnXValid = nBlockXSize;
    *pnYValid = nBlockYSize;

    if( nXPixelOff + nBlockXSize >= nRasterXSize)
    {
        *pnXValid = nRasterXSize - nXPixelOff;
    }

    if( nYPixelOff + nBlockYSize >= nRasterYSize)
    {
        *pnYValid = nRasterYSize - nYPixelOff;
    }

    return CE_None;
}

/************************************************************************/
/*                           GDALGetActualBlockSize()                   */
/************************************************************************/

/**
 * \brief Retrieve the actual block size for a given block offset.
 *
 * @see GDALRasterBand::GetActualBlockSize()
 */

CPLErr CPL_STDCALL GDALGetActualBlockSize( GDALRasterBandH hBand,
                                           int nXBlockOff,
                                           int nYBlockOff,
                                           int *pnXValid,
                                           int *pnYValid )

{
    VALIDATE_POINTER1( hBand, "GDALGetActualBlockSize", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle( hBand );
    return( poBand->GetActualBlockSize( nXBlockOff, nYBlockOff, pnXValid, pnYValid ) );
}

/************************************************************************/
/*                         GetRasterDataType()                          */
/************************************************************************/

/**
 * \brief Fetch the pixel data type for this band.
 *
 * This method is the same as the C function GDALGetRasterDataType().
 *
 * @return the data type of pixels for this band.
 */

GDALDataType GDALRasterBand::GetRasterDataType()

{
    return eDataType;
}

/************************************************************************/
/*                       GDALGetRasterDataType()                        */
/************************************************************************/

/**
 * \brief Fetch the pixel data type for this band.
 *
 * @see GDALRasterBand::GetRasterDataType()
 */

GDALDataType CPL_STDCALL GDALGetRasterDataType( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterDataType", GDT_Unknown );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->GetRasterDataType();
}

/************************************************************************/
/*                            GetBlockSize()                            */
/************************************************************************/

/**
 * \brief Fetch the "natural" block size of this band.
 *
 * GDAL contains a concept of the natural block size of rasters so that
 * applications can organized data access efficiently for some file formats.
 * The natural block size is the block size that is most efficient for
 * accessing the format.  For many formats this is simple a whole scanline
 * in which case *pnXSize is set to GetXSize(), and *pnYSize is set to 1.
 *
 * However, for tiled images this will typically be the tile size.
 *
 * Note that the X and Y block sizes don't have to divide the image size
 * evenly, meaning that right and bottom edge blocks may be incomplete.
 * See ReadBlock() for an example of code dealing with these issues.
 *
 * This method is the same as the C function GDALGetBlockSize().
 *
 * @param pnXSize integer to put the X block size into or NULL.
 *
 * @param pnYSize integer to put the Y block size into or NULL.
 */

void GDALRasterBand::GetBlockSize( int * pnXSize, int *pnYSize )

{
    if( nBlockXSize <= 0 || nBlockYSize <= 0 )
    {
        ReportError( CE_Failure, CPLE_AppDefined, "Invalid block dimension : %d * %d",
                 nBlockXSize, nBlockYSize );
        if( pnXSize != nullptr )
            *pnXSize = 0;
        if( pnYSize != nullptr )
            *pnYSize = 0;
    }
    else
    {
        if( pnXSize != nullptr )
            *pnXSize = nBlockXSize;
        if( pnYSize != nullptr )
            *pnYSize = nBlockYSize;
    }
}

/************************************************************************/
/*                          GDALGetBlockSize()                          */
/************************************************************************/

/**
 * \brief Fetch the "natural" block size of this band.
 *
 * @see GDALRasterBand::GetBlockSize()
 */

void CPL_STDCALL
GDALGetBlockSize( GDALRasterBandH hBand, int * pnXSize, int * pnYSize )

{
    VALIDATE_POINTER0( hBand, "GDALGetBlockSize" );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    poBand->GetBlockSize( pnXSize, pnYSize );
}

/************************************************************************/
/*                           InitBlockInfo()                            */
/************************************************************************/

//! @cond Doxygen_Suppress
int GDALRasterBand::InitBlockInfo()

{
    if( poBandBlockCache != nullptr )
        return poBandBlockCache->IsInitOK();

    /* Do some validation of raster and block dimensions in case the driver */
    /* would have neglected to do it itself */
    if( nBlockXSize <= 0 || nBlockYSize <= 0 )
    {
        ReportError( CE_Failure, CPLE_AppDefined, "Invalid block dimension : %d * %d",
                  nBlockXSize, nBlockYSize );
        return FALSE;
    }

    if( nRasterXSize <= 0 || nRasterYSize <= 0 )
    {
        ReportError( CE_Failure, CPLE_AppDefined, "Invalid raster dimension : %d * %d",
                  nRasterXSize, nRasterYSize );
        return FALSE;
    }

    const int nDataTypeSize = GDALGetDataTypeSizeBytes(eDataType);
    if( nDataTypeSize == 0 )
    {
        ReportError( CE_Failure, CPLE_AppDefined, "Invalid data type" );
        return FALSE;
    }

#if SIZEOF_VOIDP == 4
    if (nBlockXSize >= 10000 || nBlockYSize >= 10000)
    {
        /* As 10000 * 10000 * 16 < INT_MAX, we don't need to do the multiplication in other cases */
        if( nBlockXSize > INT_MAX / nDataTypeSize ||
            nBlockYSize > INT_MAX / (nDataTypeSize * nBlockXSize) )
        {
            ReportError( CE_Failure, CPLE_NotSupported, "Too big block : %d * %d for 32-bit build",
                        nBlockXSize, nBlockYSize );
            return FALSE;
        }
    }
#endif

    nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, nBlockXSize);
    nBlocksPerColumn = DIV_ROUND_UP(nRasterYSize, nBlockYSize);

    const char* pszBlockStrategy = CPLGetConfigOption("GDAL_BAND_BLOCK_CACHE", nullptr);
    bool bUseArray = true;
    if( pszBlockStrategy == nullptr )
    {
        if( poDS == nullptr ||
            (poDS->nOpenFlags & GDAL_OF_BLOCK_ACCESS_MASK) ==
                                            GDAL_OF_DEFAULT_BLOCK_ACCESS )
        {
            GUIntBig nBlockCount = static_cast<GIntBig>(nBlocksPerRow) * nBlocksPerColumn;
            if( poDS != nullptr )
                nBlockCount *= poDS->GetRasterCount();
            bUseArray = ( nBlockCount  < 1024 * 1024  );
        }
        else if( (poDS->nOpenFlags & GDAL_OF_BLOCK_ACCESS_MASK) ==
                                            GDAL_OF_HASHSET_BLOCK_ACCESS )
        {
            bUseArray = false;
        }
    }
    else if( EQUAL(pszBlockStrategy, "HASHSET") )
        bUseArray = false;
    if( bUseArray )
        poBandBlockCache = GDALArrayBandBlockCacheCreate(this);
    else
    {
        if( nBand == 1)
            CPLDebug("GDAL", "Use hashset band block cache");
        poBandBlockCache = GDALHashSetBandBlockCacheCreate(this);
    }
    if( poBandBlockCache == nullptr )
        return FALSE;
    return poBandBlockCache->Init();
}
//! @endcond

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

/**
 * \brief Flush raster data cache.
 *
 * This call will recover memory used to cache data blocks for this raster
 * band, and ensure that new requests are referred to the underlying driver.
 *
 * This method is the same as the C function GDALFlushRasterCache().
 *
 * @param bAtClosing Whether this is called from a GDALDataset destructor
 * @return CE_None on success.
 */

CPLErr GDALRasterBand::FlushCache(bool bAtClosing)

{
    if( bAtClosing && poDS && poDS->bSuppressOnClose && poBandBlockCache )
        poBandBlockCache->DisableDirtyBlockWriting();

    CPLErr eGlobalErr = eFlushBlockErr;

    if (eFlushBlockErr != CE_None)
    {
        ReportError(
            eFlushBlockErr, CPLE_AppDefined,
            "An error occurred while writing a dirty block from FlushCache");
        eFlushBlockErr = CE_None;
    }

    if (poBandBlockCache == nullptr || !poBandBlockCache->IsInitOK())
        return eGlobalErr;

    return poBandBlockCache->FlushCache();
}

/************************************************************************/
/*                        GDALFlushRasterCache()                        */
/************************************************************************/

/**
 * \brief Flush raster data cache.
 *
 * @see GDALRasterBand::FlushCache()
 */

CPLErr CPL_STDCALL GDALFlushRasterCache( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALFlushRasterCache", CE_Failure );

    return GDALRasterBand::FromHandle(hBand)->FlushCache(false);
}

/************************************************************************/
/*                        UnreferenceBlock()                            */
/*                                                                      */
/*      Unreference the block from our array of blocks                  */
/*      This method should only be called by                            */
/*      GDALRasterBlock::Internalize() and FlushCacheBlock() (and under */
/*      the block cache mutex)                                          */
/************************************************************************/

CPLErr GDALRasterBand::UnreferenceBlock( GDALRasterBlock* poBlock )
{
#ifdef notdef
    if( poBandBlockCache == nullptr || !poBandBlockCache->IsInitOK() )
    {
        if( poBandBlockCache == nullptr )
            printf("poBandBlockCache == NULL\n");/*ok*/
        else
            printf("!poBandBlockCache->IsInitOK()\n");/*ok*/
        printf("caller = %s\n", pszCaller);/*ok*/
        printf("GDALRasterBand: %p\n", this);/*ok*/
        printf("GDALRasterBand: nBand=%d\n", nBand);/*ok*/
        printf("nRasterXSize = %d\n", nRasterXSize);/*ok*/
        printf("nRasterYSize = %d\n", nRasterYSize);/*ok*/
        printf("nBlockXSize = %d\n", nBlockXSize);/*ok*/
        printf("nBlockYSize = %d\n", nBlockYSize);/*ok*/
        poBlock->DumpBlock();
        if( GetDataset() != nullptr )
            printf("Dataset: %s\n", GetDataset()->GetDescription());/*ok*/
        GDALRasterBlock::Verify();
        abort();
    }
#endif
    CPLAssert(poBandBlockCache && poBandBlockCache->IsInitOK());
    return poBandBlockCache->UnreferenceBlock( poBlock );
}

/************************************************************************/
/*                        AddBlockToFreeList()                          */
/*                                                                      */
/*      When GDALRasterBlock::Internalize() or FlushCacheBlock() are    */
/*      finished with a block about to be free'd, they pass it to that  */
/*      method.                                                         */
/************************************************************************/

//! @cond Doxygen_Suppress
void GDALRasterBand::AddBlockToFreeList( GDALRasterBlock * poBlock )
{
    CPLAssert(poBandBlockCache && poBandBlockCache->IsInitOK());
    return poBandBlockCache->AddBlockToFreeList( poBlock );
}
//! @endcond

/************************************************************************/
/*                             FlushBlock()                             */
/************************************************************************/

/** Flush a block out of the block cache.
 * @param nXBlockOff block x offset
 * @param nYBlockOff blocky offset
 * @param bWriteDirtyBlock whether the block should be written to disk if dirty.
 * @return CE_None in case of success, an error code otherwise.
 */
CPLErr GDALRasterBand::FlushBlock( int nXBlockOff, int nYBlockOff,
                                   int bWriteDirtyBlock )

{
    if( poBandBlockCache == nullptr || !poBandBlockCache->IsInitOK() )
        return( CE_Failure );

/* -------------------------------------------------------------------- */
/*      Validate the request                                            */
/* -------------------------------------------------------------------- */
    if( nXBlockOff < 0 || nXBlockOff >= nBlocksPerRow )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                    "Illegal nBlockXOff value (%d) in "
                    "GDALRasterBand::FlushBlock()\n",
                    nXBlockOff );

        return( CE_Failure );
    }

    if( nYBlockOff < 0 || nYBlockOff >= nBlocksPerColumn )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                    "Illegal nBlockYOff value (%d) in "
                    "GDALRasterBand::FlushBlock()\n",
                    nYBlockOff );

        return( CE_Failure );
    }

    return poBandBlockCache->FlushBlock( nXBlockOff, nYBlockOff, bWriteDirtyBlock );
}

/************************************************************************/
/*                        TryGetLockedBlockRef()                        */
/************************************************************************/

/**
 * \brief Try fetching block ref.
 *
 * This method will returned the requested block (locked) if it is already
 * in the block cache for the layer.  If not, nullptr is returned.
 *
 * If a non-NULL value is returned, then a lock for the block will have been
 * acquired on behalf of the caller.  It is absolutely imperative that the
 * caller release this lock (with GDALRasterBlock::DropLock()) or else
 * severe problems may result.
 *
 * @param nXBlockOff the horizontal block offset, with zero indicating
 * the left most block, 1 the next block and so forth.
 *
 * @param nYBlockOff the vertical block offset, with zero indicating
 * the top most block, 1 the next block and so forth.
 *
 * @return NULL if block not available, or locked block pointer.
 */

GDALRasterBlock *GDALRasterBand::TryGetLockedBlockRef( int nXBlockOff,
                                                       int nYBlockOff )

{
    if( poBandBlockCache == nullptr || !poBandBlockCache->IsInitOK() )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Validate the request                                            */
/* -------------------------------------------------------------------- */
    if( nXBlockOff < 0 || nXBlockOff >= nBlocksPerRow )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                    "Illegal nBlockXOff value (%d) in "
                    "GDALRasterBand::TryGetLockedBlockRef()\n",
                    nXBlockOff );

        return( nullptr );
    }

    if( nYBlockOff < 0 || nYBlockOff >= nBlocksPerColumn )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                    "Illegal nBlockYOff value (%d) in "
                    "GDALRasterBand::TryGetLockedBlockRef()\n",
                    nYBlockOff );

        return( nullptr );
    }

    return poBandBlockCache->TryGetLockedBlockRef(nXBlockOff, nYBlockOff);
}

/************************************************************************/
/*                         GetLockedBlockRef()                          */
/************************************************************************/

/**
 * \brief Fetch a pointer to an internally cached raster block.
 *
 * This method will returned the requested block (locked) if it is already
 * in the block cache for the layer.  If not, the block will be read from
 * the driver, and placed in the layer block cached, then returned.  If an
 * error occurs reading the block from the driver, a NULL value will be
 * returned.
 *
 * If a non-NULL value is returned, then a lock for the block will have been
 * acquired on behalf of the caller.  It is absolutely imperative that the
 * caller release this lock (with GDALRasterBlock::DropLock()) or else
 * severe problems may result.
 *
 * Note that calling GetLockedBlockRef() on a previously uncached band will
 * enable caching.
 *
 * @param nXBlockOff the horizontal block offset, with zero indicating
 * the left most block, 1 the next block and so forth.
 *
 * @param nYBlockOff the vertical block offset, with zero indicating
 * the top most block, 1 the next block and so forth.
 *
 * @param bJustInitialize If TRUE the block will be allocated and initialized,
 * but not actually read from the source.  This is useful when it will just
 * be completely set and written back.
 *
 * @return pointer to the block object, or NULL on failure.
 */

GDALRasterBlock * GDALRasterBand::GetLockedBlockRef( int nXBlockOff,
                                                     int nYBlockOff,
                                                     int bJustInitialize )

{
/* -------------------------------------------------------------------- */
/*      Try and fetch from cache.                                       */
/* -------------------------------------------------------------------- */
    GDALRasterBlock *poBlock = TryGetLockedBlockRef( nXBlockOff, nYBlockOff );

/* -------------------------------------------------------------------- */
/*      If we didn't find it in our memory cache, instantiate a         */
/*      block (potentially load from disk) and "adopt" it into the      */
/*      cache.                                                          */
/* -------------------------------------------------------------------- */
    if( poBlock == nullptr )
    {
        if( !InitBlockInfo() )
            return( nullptr );

    /* -------------------------------------------------------------------- */
    /*      Validate the request                                            */
    /* -------------------------------------------------------------------- */
        if( nXBlockOff < 0 || nXBlockOff >= nBlocksPerRow )
        {
            ReportError( CE_Failure, CPLE_IllegalArg,
                      "Illegal nBlockXOff value (%d) in "
                      "GDALRasterBand::GetLockedBlockRef()\n",
                      nXBlockOff );

            return( nullptr );
        }

        if( nYBlockOff < 0 || nYBlockOff >= nBlocksPerColumn )
        {
            ReportError( CE_Failure, CPLE_IllegalArg,
                      "Illegal nBlockYOff value (%d) in "
                      "GDALRasterBand::GetLockedBlockRef()\n",
                      nYBlockOff );

            return( nullptr );
        }

        poBlock = poBandBlockCache->CreateBlock( nXBlockOff, nYBlockOff );
        if( poBlock == nullptr )
            return nullptr;

        poBlock->AddLock();

        /* We need to temporarily drop the read-write lock in the following */
        /*scenario. Imagine 2 threads T1 and T2 that respectively write dataset */
        /* D1 and D2. T1 will take the mutex on D1 and T2 on D2. Now when the */
        /* block cache fills, T1 might need to flush dirty blocks of D2 in the */
        /* below Internalize(), which will cause GDALRasterBlock::Write() to be */
        /* called and attempt at taking the lock on T2 (already taken). Similarly */
        /* for T2 with D1, hence a deadlock situation (#6163) */
        /* But this may open the door to other problems... */
        if( poDS )
            poDS->TemporarilyDropReadWriteLock();
        /* allocate data space */
        CPLErr eErr = poBlock->Internalize();
        if( poDS )
            poDS->ReacquireReadWriteLock();
        if( eErr != CE_None )
        {
            poBlock->DropLock();
            delete poBlock;
            return nullptr;
        }

        if ( poBandBlockCache->AdoptBlock(poBlock) != CE_None )
        {
            poBlock->DropLock();
            delete poBlock;
            return nullptr;
        }

        if( !bJustInitialize )
        {
            const GUInt32 nErrorCounter = CPLGetErrorCounter();
            int bCallLeaveReadWrite = EnterReadWrite(GF_Read);
            eErr = IReadBlock(nXBlockOff,nYBlockOff,poBlock->GetDataRef());
            if( bCallLeaveReadWrite) LeaveReadWrite();
            if( eErr != CE_None )
            {
                poBlock->DropLock();
                FlushBlock( nXBlockOff, nYBlockOff );
                ReportError( CE_Failure, CPLE_AppDefined,
                    "IReadBlock failed at X offset %d, Y offset %d%s",
                    nXBlockOff, nYBlockOff,
                    (nErrorCounter != CPLGetErrorCounter()) ?
                        CPLSPrintf(": %s", CPLGetLastErrorMsg()) : "");
                return nullptr;
            }

            nBlockReads++;
            if( static_cast<GIntBig>(nBlockReads) ==
                static_cast<GIntBig>(nBlocksPerRow) * nBlocksPerColumn + 1
                && nBand == 1 && poDS != nullptr )
            {
                CPLDebug( "GDAL", "Potential thrashing on band %d of %s.",
                          nBand, poDS->GetDescription() );
            }
        }
    }

    return poBlock;
}

/************************************************************************/
/*                               Fill()                                 */
/************************************************************************/

/**
 * \brief Fill this band with a constant value.
 *
 * GDAL makes no guarantees
 * about what values pixels in newly created files are set to, so this
 * method can be used to clear a band to a specified "default" value.
 * The fill value is passed in as a double but this will be converted
 * to the underlying type before writing to the file. An optional
 * second argument allows the imaginary component of a complex
 * constant value to be specified.
 *
 * This method is the same as the C function GDALFillRaster().
 *
 * @param dfRealValue Real component of fill value
 * @param dfImaginaryValue Imaginary component of fill value, defaults to zero
 *
 * @return CE_Failure if the write fails, otherwise CE_None
 */
CPLErr GDALRasterBand::Fill( double dfRealValue, double dfImaginaryValue ) {

    // General approach is to construct a source block of the file's
    // native type containing the appropriate value and then copy this
    // to each block in the image via the RasterBlock cache. Using
    // the cache means we avoid file I/O if it is not necessary, at the
    // expense of some extra memcpy's (since we write to the
    // RasterBlock cache, which is then at some point written to the
    // underlying file, rather than simply directly to the underlying
    // file.)

    // Check we can write to the file.
    if( eAccess == GA_ReadOnly ) {
        ReportError( CE_Failure, CPLE_NoWriteAccess,
                     "Attempt to write to read only dataset in "
                     "GDALRasterBand::Fill()." );
        return CE_Failure;
    }

    // Make sure block parameters are set.
    if( !InitBlockInfo() )
        return CE_Failure;

    // Allocate the source block.
    auto blockSize = static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize;
    int elementSize = GDALGetDataTypeSizeBytes(eDataType);
    auto blockByteSize = blockSize * elementSize;
    unsigned char* srcBlock =
        static_cast<unsigned char*>( VSIMalloc(blockByteSize) );
    if (srcBlock == nullptr) {
        ReportError( CE_Failure, CPLE_OutOfMemory,
                     "GDALRasterBand::Fill(): Out of memory "
                     "allocating " CPL_FRMT_GUIB " bytes.\n",
                     static_cast<GUIntBig>(blockByteSize) );
        return CE_Failure;
    }

    // Initialize the source block.
    double complexSrc[2] = { dfRealValue, dfImaginaryValue };
    GDALCopyWords64(complexSrc, GDT_CFloat64, 0,
                  srcBlock, eDataType, elementSize, blockSize);

    const bool bCallLeaveReadWrite = CPL_TO_BOOL(EnterReadWrite(GF_Write));

    // Write block to block cache
    for (int j = 0; j < nBlocksPerColumn; ++j) {
        for (int i = 0; i < nBlocksPerRow; ++i) {
            GDALRasterBlock* destBlock = GetLockedBlockRef(i, j, TRUE);
            if (destBlock == nullptr)
            {
                ReportError( CE_Failure, CPLE_OutOfMemory,
                             "GDALRasterBand::Fill(): Error "
                             "while retrieving cache block.");
                VSIFree(srcBlock);
                return CE_Failure;
            }
            memcpy(destBlock->GetDataRef(), srcBlock, blockByteSize);
            destBlock->MarkDirty();
            destBlock->DropLock();
        }
    }

    if( bCallLeaveReadWrite ) LeaveReadWrite();

    // Free up the source block
    VSIFree(srcBlock);

    return CE_None;
}

/************************************************************************/
/*                         GDALFillRaster()                             */
/************************************************************************/

/**
 * \brief Fill this band with a constant value.
 *
 * @see GDALRasterBand::Fill()
 */
CPLErr CPL_STDCALL GDALFillRaster(
    GDALRasterBandH hBand, double dfRealValue, double dfImaginaryValue )
{
    VALIDATE_POINTER1( hBand, "GDALFillRaster", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->Fill(dfRealValue, dfImaginaryValue);
}

/************************************************************************/
/*                             GetAccess()                              */
/************************************************************************/

/**
 * \brief Find out if we have update permission for this band.
 *
 * This method is the same as the C function GDALGetRasterAccess().
 *
 * @return Either GA_Update or GA_ReadOnly.
 */

GDALAccess GDALRasterBand::GetAccess()

{
    return eAccess;
}

/************************************************************************/
/*                        GDALGetRasterAccess()                         */
/************************************************************************/

/**
 * \brief Find out if we have update permission for this band.
 *
 * @see GDALRasterBand::GetAccess()
 */

GDALAccess CPL_STDCALL GDALGetRasterAccess( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterAccess", GA_ReadOnly );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->GetAccess();
}

/************************************************************************/
/*                          GetCategoryNames()                          */
/************************************************************************/

/**
 * \brief Fetch the list of category names for this raster.
 *
 * The return list is a "StringList" in the sense of the CPL functions.
 * That is a NULL terminated array of strings.  Raster values without
 * associated names will have an empty string in the returned list.  The
 * first entry in the list is for raster values of zero, and so on.
 *
 * The returned stringlist should not be altered or freed by the application.
 * It may change on the next GDAL call, so please copy it if it is needed
 * for any period of time.
 *
 * This method is the same as the C function GDALGetRasterCategoryNames().
 *
 * @return list of names, or NULL if none.
 */

char **GDALRasterBand::GetCategoryNames()

{
    return nullptr;
}

/************************************************************************/
/*                     GDALGetRasterCategoryNames()                     */
/************************************************************************/

/**
 * \brief Fetch the list of category names for this raster.
 *
 * @see GDALRasterBand::GetCategoryNames()
 */

char ** CPL_STDCALL GDALGetRasterCategoryNames( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterCategoryNames", nullptr );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->GetCategoryNames();
}

/************************************************************************/
/*                          SetCategoryNames()                          */
/************************************************************************/

/**
 * \fn GDALRasterBand::SetCategoryNames(char**)
 * \brief Set the category names for this band.
 *
 * See the GetCategoryNames() method for more on the interpretation of
 * category names.
 *
 * This method is the same as the C function GDALSetRasterCategoryNames().
 *
 * @param papszNames the NULL terminated StringList of category names.  May
 * be NULL to just clear the existing list.
 *
 * @return CE_None on success of CE_Failure on failure.  If unsupported
 * by the driver CE_Failure is returned, but no error message is reported.
 */

/**/
/**/

CPLErr GDALRasterBand::SetCategoryNames( char ** /*papszNames*/ )
{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError( CE_Failure, CPLE_NotSupported,
                     "SetCategoryNames() not supported for this dataset." );

    return CE_Failure;
}

/************************************************************************/
/*                        GDALSetCategoryNames()                        */
/************************************************************************/

/**
 * \brief Set the category names for this band.
 *
 * @see GDALRasterBand::SetCategoryNames()
 */

CPLErr CPL_STDCALL
GDALSetRasterCategoryNames( GDALRasterBandH hBand, CSLConstList papszNames )

{
    VALIDATE_POINTER1( hBand, "GDALSetRasterCategoryNames", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->SetCategoryNames( const_cast<char**>(papszNames) );
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

/**
 * \brief Fetch the no data value for this band.
 *
 * If there is no out of data value, an out of range value will generally
 * be returned.  The no data value for a band is generally a special marker
 * value used to mark pixels that are not valid data.  Such pixels should
 * generally not be displayed, nor contribute to analysis operations.
 *
 * The no data value returned is 'raw', meaning that it has no offset and
 * scale applied.
 *
 * For rasters of type GDT_Int64 or GDT_UInt64, using this method might be
 * lossy if the nodata value cannot exactly been represented by a double.
 * Use GetNoDataValueAsInt64() or GetNoDataValueAsUInt64() instead.
 *
 * This method is the same as the C function GDALGetRasterNoDataValue().
 *
 * @param pbSuccess pointer to a boolean to use to indicate if a value
 * is actually associated with this layer.  May be NULL (default).
 *
 * @return the nodata value for this band.
 */

double GDALRasterBand::GetNoDataValue( int *pbSuccess )

{
    if( pbSuccess != nullptr )
        *pbSuccess = FALSE;

    return -1e10;
}

/************************************************************************/
/*                      GDALGetRasterNoDataValue()                      */
/************************************************************************/

/**
 * \brief Fetch the no data value for this band.
 *
 * @see GDALRasterBand::GetNoDataValue()
 */

double CPL_STDCALL
GDALGetRasterNoDataValue( GDALRasterBandH hBand, int *pbSuccess )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterNoDataValue", 0 );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->GetNoDataValue( pbSuccess );
}

/************************************************************************/
/*                       GetNoDataValueAsInt64()                        */
/************************************************************************/

/**
 * \brief Fetch the no data value for this band.
 *
 * This method should ONLY be called on rasters whose data type is GDT_Int64.
 *
 * If there is no out of data value, an out of range value will generally
 * be returned.  The no data value for a band is generally a special marker
 * value used to mark pixels that are not valid data.  Such pixels should
 * generally not be displayed, nor contribute to analysis operations.
 *
 * The no data value returned is 'raw', meaning that it has no offset and
 * scale applied.
 *
 * This method is the same as the C function GDALGetRasterNoDataValueAsInt64().
 *
 * @param pbSuccess pointer to a boolean to use to indicate if a value
 * is actually associated with this layer.  May be NULL (default).
 *
 * @return the nodata value for this band.
 *
 * @since GDAL 3.5
 */

int64_t GDALRasterBand::GetNoDataValueAsInt64( int *pbSuccess )

{
    if( pbSuccess != nullptr )
        *pbSuccess = FALSE;

    return std::numeric_limits<int64_t>::min();
}

/************************************************************************/
/*                   GDALGetRasterNoDataValueAsInt64()                  */
/************************************************************************/

/**
 * \brief Fetch the no data value for this band.
 *
 * This function should ONLY be called on rasters whose data type is GDT_Int64.
 *
 * @see GDALRasterBand::GetNoDataValueAsInt64()
 *
 * @since GDAL 3.5
 */

int64_t CPL_STDCALL
GDALGetRasterNoDataValueAsInt64( GDALRasterBandH hBand, int *pbSuccess )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterNoDataValueAsInt64",
                       std::numeric_limits<int64_t>::min());

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->GetNoDataValueAsInt64( pbSuccess );
}

/************************************************************************/
/*                       GetNoDataValueAsUInt64()                        */
/************************************************************************/

/**
 * \brief Fetch the no data value for this band.
 *
 * This method should ONLY be called on rasters whose data type is GDT_UInt64.
 *
 * If there is no out of data value, an out of range value will generally
 * be returned.  The no data value for a band is generally a special marker
 * value used to mark pixels that are not valid data.  Such pixels should
 * generally not be displayed, nor contribute to analysis operations.
 *
 * The no data value returned is 'raw', meaning that it has no offset and
 * scale applied.
 *
 * This method is the same as the C function GDALGetRasterNoDataValueAsUInt64().
 *
 * @param pbSuccess pointer to a boolean to use to indicate if a value
 * is actually associated with this layer.  May be NULL (default).
 *
 * @return the nodata value for this band.
 *
 * @since GDAL 3.5
 */

uint64_t GDALRasterBand::GetNoDataValueAsUInt64( int *pbSuccess )

{
    if( pbSuccess != nullptr )
        *pbSuccess = FALSE;

    return std::numeric_limits<uint64_t>::max();
}

/************************************************************************/
/*                   GDALGetRasterNoDataValueAsUInt64()                  */
/************************************************************************/

/**
 * \brief Fetch the no data value for this band.
 *
 * This function should ONLY be called on rasters whose data type is GDT_UInt64.
 *
 * @see GDALRasterBand::GetNoDataValueAsUInt64()
 *
 * @since GDAL 3.5
 */

uint64_t CPL_STDCALL
GDALGetRasterNoDataValueAsUInt64( GDALRasterBandH hBand, int *pbSuccess )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterNoDataValueAsUInt64",
                       std::numeric_limits<uint64_t>::max());

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->GetNoDataValueAsUInt64( pbSuccess );
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

/**
 * \fn GDALRasterBand::SetNoDataValue(double)
 * \brief Set the no data value for this band.
 *
 * Depending on drivers, changing the no data value may or may not have an
 * effect on the pixel values of a raster that has just been created. It is
 * thus advised to explicitly called Fill() if the intent is to initialize
 * the raster to the nodata value.
 * In any case, changing an existing no data value, when one already exists and
 * the dataset exists or has been initialized, has no effect on the pixel whose
 * value matched the previous nodata value.
 *
 * For rasters of type GDT_Int64 or GDT_UInt64, whose nodata value cannot always
 * be represented by a double, use SetNoDataValueAsInt64() or
 * SetNoDataValueAsUInt64() instead.
 *
 * To clear the nodata value, use DeleteNoDataValue().
 *
 * This method is the same as the C function GDALSetRasterNoDataValue().
 *
 * @param dfNoData the value to set.
 *
 * @return CE_None on success, or CE_Failure on failure.  If unsupported
 * by the driver, CE_Failure is returned by no error message will have
 * been emitted.
 */

/**/
/**/

CPLErr GDALRasterBand::SetNoDataValue( double /*dfNoData*/ )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError( CE_Failure, CPLE_NotSupported,
                     "SetNoDataValue() not supported for this dataset." );

    return CE_Failure;
}

/************************************************************************/
/*                         GDALSetRasterNoDataValue()                   */
/************************************************************************/

/**
 * \brief Set the no data value for this band.
 *
 * Depending on drivers, changing the no data value may or may not have an
 * effect on the pixel values of a raster that has just been created. It is
 * thus advised to explicitly called Fill() if the intent is to initialize
 * the raster to the nodata value.
 * In any case, changing an existing no data value, when one already exists and
 * the dataset exists or has been initialized, has no effect on the pixel whose
 * value matched the previous nodata value.
 *
 * For rasters of type GDT_Int64 or GDT_UInt64, whose nodata value cannot always
 * be represented by a double, use GDALSetRasterNoDataValueAsInt64() or
 * GDALSetRasterNoDataValueAsUInt64() instead.
 *
 * @see GDALRasterBand::SetNoDataValue()
 */

CPLErr CPL_STDCALL
GDALSetRasterNoDataValue( GDALRasterBandH hBand, double dfValue )

{
    VALIDATE_POINTER1( hBand, "GDALSetRasterNoDataValue", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->SetNoDataValue( dfValue );
}

/************************************************************************/
/*                       SetNoDataValueAsInt64()                        */
/************************************************************************/

/**
 * \brief Set the no data value for this band.
 *
 * This method should ONLY be called on rasters whose data type is GDT_Int64.
 *
 * Depending on drivers, changing the no data value may or may not have an
 * effect on the pixel values of a raster that has just been created. It is
 * thus advised to explicitly called Fill() if the intent is to initialize
 * the raster to the nodata value.
 * In ay case, changing an existing no data value, when one already exists and
 * the dataset exists or has been initialized, has no effect on the pixel whose
 * value matched the previous nodata value.
 *
 * To clear the nodata value, use DeleteNoDataValue().
 *
 * This method is the same as the C function GDALSetRasterNoDataValueAsInt64().
 *
 * @param nNoDataValue the value to set.
 *
 * @return CE_None on success, or CE_Failure on failure.  If unsupported
 * by the driver, CE_Failure is returned by no error message will have
 * been emitted.
 *
 * @since GDAL 3.5
 */

CPLErr GDALRasterBand::SetNoDataValueAsInt64( CPL_UNUSED int64_t nNoDataValue )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError( CE_Failure, CPLE_NotSupported,
                     "SetNoDataValueAsInt64() not supported for this dataset." );

    return CE_Failure;
}

/************************************************************************/
/*                 GDALSetRasterNoDataValueAsInt64()                    */
/************************************************************************/

/**
 * \brief Set the no data value for this band.
 *
 * This function should ONLY be called on rasters whose data type is GDT_Int64.
 *
 * Depending on drivers, changing the no data value may or may not have an
 * effect on the pixel values of a raster that has just been created. It is
 * thus advised to explicitly called Fill() if the intent is to initialize
 * the raster to the nodata value.
 * In ay case, changing an existing no data value, when one already exists and
 * the dataset exists or has been initialized, has no effect on the pixel whose
 * value matched the previous nodata value.
 *
 * @see GDALRasterBand::SetNoDataValueAsInt64()
 *
 * @since GDAL 3.5
 */

CPLErr CPL_STDCALL
GDALSetRasterNoDataValueAsInt64( GDALRasterBandH hBand, int64_t nValue )

{
    VALIDATE_POINTER1( hBand, "GDALSetRasterNoDataValueAsInt64", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->SetNoDataValueAsInt64( nValue );
}

/************************************************************************/
/*                       SetNoDataValueAsUInt64()                       */
/************************************************************************/

/**
 * \brief Set the no data value for this band.
 *
 * This method should ONLY be called on rasters whose data type is GDT_UInt64.
 *
 * Depending on drivers, changing the no data value may or may not have an
 * effect on the pixel values of a raster that has just been created. It is
 * thus advised to explicitly called Fill() if the intent is to initialize
 * the raster to the nodata value.
 * In ay case, changing an existing no data value, when one already exists and
 * the dataset exists or has been initialized, has no effect on the pixel whose
 * value matched the previous nodata value.
 *
 * To clear the nodata value, use DeleteNoDataValue().
 *
 * This method is the same as the C function GDALSetRasterNoDataValueAsUInt64().
 *
 * @param nNoDataValue the value to set.
 *
 * @return CE_None on success, or CE_Failure on failure.  If unsupported
 * by the driver, CE_Failure is returned by no error message will have
 * been emitted.
 *
 * @since GDAL 3.5
 */

CPLErr GDALRasterBand::SetNoDataValueAsUInt64( CPL_UNUSED uint64_t nNoDataValue )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError( CE_Failure, CPLE_NotSupported,
                     "SetNoDataValueAsUInt64() not supported for this dataset." );

    return CE_Failure;
}

/************************************************************************/
/*                 GDALSetRasterNoDataValueAsUInt64()                    */
/************************************************************************/

/**
 * \brief Set the no data value for this band.
 *
 * This function should ONLY be called on rasters whose data type is GDT_UInt64.
 *
 * Depending on drivers, changing the no data value may or may not have an
 * effect on the pixel values of a raster that has just been created. It is
 * thus advised to explicitly called Fill() if the intent is to initialize
 * the raster to the nodata value.
 * In ay case, changing an existing no data value, when one already exists and
 * the dataset exists or has been initialized, has no effect on the pixel whose
 * value matched the previous nodata value.
 *
 * @see GDALRasterBand::SetNoDataValueAsUInt64()
 *
 * @since GDAL 3.5
 */

CPLErr CPL_STDCALL
GDALSetRasterNoDataValueAsUInt64( GDALRasterBandH hBand, uint64_t nValue )

{
    VALIDATE_POINTER1( hBand, "GDALSetRasterNoDataValueAsUInt64", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->SetNoDataValueAsUInt64( nValue );
}

/************************************************************************/
/*                        DeleteNoDataValue()                           */
/************************************************************************/

/**
 * \brief Remove the no data value for this band.
 *
 * This method is the same as the C function GDALDeleteRasterNoDataValue().
 *
 * @return CE_None on success, or CE_Failure on failure.  If unsupported
 * by the driver, CE_Failure is returned by no error message will have
 * been emitted.
 *
 * @since GDAL 2.1
 */

CPLErr GDALRasterBand::DeleteNoDataValue()

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError( CE_Failure, CPLE_NotSupported,
                     "DeleteNoDataValue() not supported for this dataset." );

    return CE_Failure;
}

/************************************************************************/
/*                       GDALDeleteRasterNoDataValue()                  */
/************************************************************************/

/**
 * \brief Remove the no data value for this band.
 *
 * @see GDALRasterBand::DeleteNoDataValue()
 *
 * @since GDAL 2.1
 */

CPLErr CPL_STDCALL
GDALDeleteRasterNoDataValue( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALDeleteRasterNoDataValue", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->DeleteNoDataValue();
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

/**
 * \brief Fetch the maximum value for this band.
 *
 * For file formats that don't know this intrinsically, the maximum supported
 * value for the data type will generally be returned.
 *
 * This method is the same as the C function GDALGetRasterMaximum().
 *
 * @param pbSuccess pointer to a boolean to use to indicate if the
 * returned value is a tight maximum or not.  May be NULL (default).
 *
 * @return the maximum raster value (excluding no data pixels)
 */

double GDALRasterBand::GetMaximum( int *pbSuccess )

{
    const char *pszValue = nullptr;

    if( (pszValue = GetMetadataItem("STATISTICS_MAXIMUM")) != nullptr )
    {
        if( pbSuccess != nullptr )
            *pbSuccess = TRUE;

        return CPLAtofM(pszValue);
    }

    if( pbSuccess != nullptr )
        *pbSuccess = FALSE;

    switch( eDataType )
    {
      case GDT_Byte:
      {
        const char* pszPixelType =
            GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
        if (pszPixelType != nullptr && EQUAL(pszPixelType, "SIGNEDBYTE"))
            return 127;

        return 255;
      }

      case GDT_UInt16:
        return 65535;

      case GDT_Int16:
      case GDT_CInt16:
        return 32767;

      case GDT_Int32:
      case GDT_CInt32:
        return 2147483647.0;

      case GDT_UInt32:
        return 4294967295.0;

      case GDT_Float32:
      case GDT_CFloat32:
        return 4294967295.0;  // Not actually accurate.

      case GDT_Float64:
      case GDT_CFloat64:
        return 4294967295.0;  // Not actually accurate.

      default:
        return 4294967295.0;  // Not actually accurate.
    }
}

/************************************************************************/
/*                        GDALGetRasterMaximum()                        */
/************************************************************************/

/**
 * \brief Fetch the maximum value for this band.
 *
 * @see GDALRasterBand::GetMaximum()
 */

double CPL_STDCALL
GDALGetRasterMaximum( GDALRasterBandH hBand, int *pbSuccess )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterMaximum", 0 );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->GetMaximum( pbSuccess );
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

/**
 * \brief Fetch the minimum value for this band.
 *
 * For file formats that don't know this intrinsically, the minimum supported
 * value for the data type will generally be returned.
 *
 * This method is the same as the C function GDALGetRasterMinimum().
 *
 * @param pbSuccess pointer to a boolean to use to indicate if the
 * returned value is a tight minimum or not.  May be NULL (default).
 *
 * @return the minimum raster value (excluding no data pixels)
 */

double GDALRasterBand::GetMinimum( int *pbSuccess )

{
    const char *pszValue = nullptr;

    if( (pszValue = GetMetadataItem("STATISTICS_MINIMUM")) != nullptr )
    {
        if( pbSuccess != nullptr )
            *pbSuccess = TRUE;

        return CPLAtofM(pszValue);
    }

    if( pbSuccess != nullptr )
        *pbSuccess = FALSE;

    switch( eDataType )
    {
      case GDT_Byte:
      {
        const char* pszPixelType =
            GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
        if (pszPixelType != nullptr && EQUAL(pszPixelType, "SIGNEDBYTE"))
            return -128;

        return 0;
      }

      case GDT_UInt16:
        return 0;

      case GDT_Int16:
        return -32768;

      case GDT_Int32:
        return -2147483648.0;

      case GDT_UInt32:
        return 0;

      case GDT_Float32:
        return -4294967295.0;  // Not actually accurate.

      case GDT_Float64:
        return -4294967295.0;  // Not actually accurate.

      default:
        return -4294967295.0;  // Not actually accurate.
    }
}

/************************************************************************/
/*                        GDALGetRasterMinimum()                        */
/************************************************************************/

/**
 * \brief Fetch the minimum value for this band.
 *
 * @see GDALRasterBand::GetMinimum()
 */

double CPL_STDCALL
GDALGetRasterMinimum( GDALRasterBandH hBand, int *pbSuccess )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterMinimum", 0 );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->GetMinimum( pbSuccess );
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

/**
 * \brief How should this band be interpreted as color?
 *
 * GCI_Undefined is returned when the format doesn't know anything
 * about the color interpretation.
 *
 * This method is the same as the C function
 * GDALGetRasterColorInterpretation().
 *
 * @return color interpretation value for band.
 */

GDALColorInterp GDALRasterBand::GetColorInterpretation()

{
    return GCI_Undefined;
}

/************************************************************************/
/*                  GDALGetRasterColorInterpretation()                  */
/************************************************************************/

/**
 * \brief How should this band be interpreted as color?
 *
 * @see GDALRasterBand::GetColorInterpretation()
 */

GDALColorInterp CPL_STDCALL
GDALGetRasterColorInterpretation( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterColorInterpretation",
                       GCI_Undefined );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->GetColorInterpretation();
}

/************************************************************************/
/*                       SetColorInterpretation()                       */
/************************************************************************/

/**
 * \fn GDALRasterBand::SetColorInterpretation(GDALColorInterp)
 * \brief Set color interpretation of a band.
 *
 * This method is the same as the C function GDALSetRasterColorInterpretation().
 *
 * @param eColorInterp the new color interpretation to apply to this band.
 *
 * @return CE_None on success or CE_Failure if method is unsupported by format.
 */

/**/
/**/

CPLErr GDALRasterBand::SetColorInterpretation(
    GDALColorInterp /*eColorInterp*/ )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError( CE_Failure, CPLE_NotSupported,
                  "SetColorInterpretation() not supported for this dataset." );
    return CE_Failure;
}

/************************************************************************/
/*                  GDALSetRasterColorInterpretation()                  */
/************************************************************************/

/**
 * \brief Set color interpretation of a band.
 *
 * @see GDALRasterBand::SetColorInterpretation()
 */

CPLErr CPL_STDCALL
GDALSetRasterColorInterpretation( GDALRasterBandH hBand,
                                  GDALColorInterp eColorInterp )

{
    VALIDATE_POINTER1( hBand, "GDALSetRasterColorInterpretation", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->SetColorInterpretation(eColorInterp);
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

/**
 * \brief Fetch the color table associated with band.
 *
 * If there is no associated color table, the return result is NULL.  The
 * returned color table remains owned by the GDALRasterBand, and can't
 * be depended on for long, nor should it ever be modified by the caller.
 *
 * This method is the same as the C function GDALGetRasterColorTable().
 *
 * @return internal color table, or NULL.
 */

GDALColorTable *GDALRasterBand::GetColorTable()

{
    return nullptr;
}

/************************************************************************/
/*                      GDALGetRasterColorTable()                       */
/************************************************************************/

/**
 * \brief Fetch the color table associated with band.
 *
 * @see GDALRasterBand::GetColorTable()
 */

GDALColorTableH CPL_STDCALL GDALGetRasterColorTable( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterColorTable", nullptr );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return GDALColorTable::ToHandle(poBand->GetColorTable());
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

/**
 * \fn GDALRasterBand::SetColorTable(GDALColorTable*)
 * \brief Set the raster color table.
 *
 * The driver will make a copy of all desired data in the colortable.  It
 * remains owned by the caller after the call.
 *
 * This method is the same as the C function GDALSetRasterColorTable().
 *
 * @param poCT the color table to apply.  This may be NULL to clear the color
 * table (where supported).
 *
 * @return CE_None on success, or CE_Failure on failure.  If the action is
 * unsupported by the driver, a value of CE_Failure is returned, but no
 * error is issued.
 */

/**/
/**/

CPLErr GDALRasterBand::SetColorTable( GDALColorTable * /*poCT*/ )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError( CE_Failure, CPLE_NotSupported,
                  "SetColorTable() not supported for this dataset." );
    return CE_Failure;
}

/************************************************************************/
/*                      GDALSetRasterColorTable()                       */
/************************************************************************/

/**
 * \brief Set the raster color table.
 *
 * @see GDALRasterBand::SetColorTable()
 */

CPLErr CPL_STDCALL
GDALSetRasterColorTable( GDALRasterBandH hBand, GDALColorTableH hCT )

{
    VALIDATE_POINTER1( hBand, "GDALSetRasterColorTable", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->SetColorTable( GDALColorTable::FromHandle(hCT) );
}

/************************************************************************/
/*                       HasArbitraryOverviews()                        */
/************************************************************************/

/**
 * \brief Check for arbitrary overviews.
 *
 * This returns TRUE if the underlying datastore can compute arbitrary
 * overviews efficiently, such as is the case with OGDI over a network.
 * Datastores with arbitrary overviews don't generally have any fixed
 * overviews, but the RasterIO() method can be used in downsampling mode
 * to get overview data efficiently.
 *
 * This method is the same as the C function GDALHasArbitraryOverviews(),
 *
 * @return TRUE if arbitrary overviews available (efficiently), otherwise
 * FALSE.
 */

int GDALRasterBand::HasArbitraryOverviews()

{
    return FALSE;
}

/************************************************************************/
/*                     GDALHasArbitraryOverviews()                      */
/************************************************************************/

/**
 * \brief Check for arbitrary overviews.
 *
 * @see GDALRasterBand::HasArbitraryOverviews()
 */

int CPL_STDCALL GDALHasArbitraryOverviews( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALHasArbitraryOverviews", 0 );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->HasArbitraryOverviews();
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

/**
 * \brief Return the number of overview layers available.
 *
 * This method is the same as the C function GDALGetOverviewCount().
 *
 * @return overview count, zero if none.
 */

int GDALRasterBand::GetOverviewCount()

{
    if( poDS != nullptr && poDS->oOvManager.IsInitialized() && poDS->AreOverviewsEnabled() )
        return poDS->oOvManager.GetOverviewCount( nBand );

    return 0;
}

/************************************************************************/
/*                        GDALGetOverviewCount()                        */
/************************************************************************/

/**
 * \brief Return the number of overview layers available.
 *
 * @see GDALRasterBand::GetOverviewCount()
 */

int CPL_STDCALL GDALGetOverviewCount( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetOverviewCount", 0 );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->GetOverviewCount();
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

/**
 * \brief Fetch overview raster band object.
 *
 * This method is the same as the C function GDALGetOverview().
 *
 * @param i overview index between 0 and GetOverviewCount()-1.
 *
 * @return overview GDALRasterBand.
 */

GDALRasterBand * GDALRasterBand::GetOverview( int i )

{
    if( poDS != nullptr && poDS->oOvManager.IsInitialized() && poDS->AreOverviewsEnabled() )
        return poDS->oOvManager.GetOverview( nBand, i );

    return nullptr;
}

/************************************************************************/
/*                          GDALGetOverview()                           */
/************************************************************************/

/**
 * \brief Fetch overview raster band object.
 *
 * @see GDALRasterBand::GetOverview()
 */

GDALRasterBandH CPL_STDCALL GDALGetOverview( GDALRasterBandH hBand, int i )

{
    VALIDATE_POINTER1( hBand, "GDALGetOverview", nullptr );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return GDALRasterBand::ToHandle(poBand->GetOverview(i));
}

/************************************************************************/
/*                      GetRasterSampleOverview()                       */
/************************************************************************/

/**
 * \brief Fetch best sampling overview.
 *
 * Returns the most reduced overview of the given band that still satisfies
 * the desired number of samples.  This function can be used with zero
 * as the number of desired samples to fetch the most reduced overview.
 * The same band as was passed in will be returned if it has not overviews,
 * or if none of the overviews have enough samples.
 *
 * This method is the same as the C functions GDALGetRasterSampleOverview()
 * and GDALGetRasterSampleOverviewEx().
 *
 * @param nDesiredSamples the returned band will have at least this many
 * pixels.
 *
 * @return optimal overview or the band itself.
 */

GDALRasterBand *GDALRasterBand::GetRasterSampleOverview(
    GUIntBig nDesiredSamples )

{
    GDALRasterBand *poBestBand = this;

    double dfBestSamples = GetXSize() * static_cast<double>(GetYSize());

    for( int iOverview = 0; iOverview < GetOverviewCount(); iOverview++ )
    {
        GDALRasterBand  *poOBand = GetOverview( iOverview );

        if (poOBand == nullptr)
            continue;

        const double dfOSamples =
            poOBand->GetXSize() * static_cast<double>(poOBand->GetYSize());

        if( dfOSamples < dfBestSamples && dfOSamples > nDesiredSamples )
        {
            dfBestSamples = dfOSamples;
            poBestBand = poOBand;
        }
    }

    return poBestBand;
}

/************************************************************************/
/*                    GDALGetRasterSampleOverview()                     */
/************************************************************************/

/**
 * \brief Fetch best sampling overview.
 *
 * Use GDALGetRasterSampleOverviewEx() to be able to specify more than 2
 * billion samples.
 *
 * @see GDALRasterBand::GetRasterSampleOverview()
 * @see GDALGetRasterSampleOverviewEx()
 */

GDALRasterBandH CPL_STDCALL
GDALGetRasterSampleOverview( GDALRasterBandH hBand, int nDesiredSamples )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterSampleOverview", nullptr );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return GDALRasterBand::ToHandle(
        poBand->GetRasterSampleOverview(
            nDesiredSamples < 0 ? 0 : static_cast<GUIntBig>(nDesiredSamples) ));
}

/************************************************************************/
/*                    GDALGetRasterSampleOverviewEx()                   */
/************************************************************************/

/**
 * \brief Fetch best sampling overview.
 *
 * @see GDALRasterBand::GetRasterSampleOverview()
 * @since GDAL 2.0
 */

GDALRasterBandH CPL_STDCALL
GDALGetRasterSampleOverviewEx( GDALRasterBandH hBand, GUIntBig nDesiredSamples )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterSampleOverviewEx", nullptr );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return GDALRasterBand::ToHandle(
        poBand->GetRasterSampleOverview( nDesiredSamples ));
}

/************************************************************************/
/*                           BuildOverviews()                           */
/************************************************************************/

/**
 * \fn GDALRasterBand::BuildOverviews(const char*, int, int*, GDALProgressFunc, void*)
 * \brief Build raster overview(s)
 *
 * If the operation is unsupported for the indicated dataset, then
 * CE_Failure is returned, and CPLGetLastErrorNo() will return
 * CPLE_NotSupported.
 *
 * WARNING:  It is not possible to build overviews for a single band in
 * TIFF format, and thus this method does not work for TIFF format, or any
 * formats that use the default overview building in TIFF format.  Instead
 * it is necessary to build overviews on the dataset as a whole using
 * GDALDataset::BuildOverviews().  That makes this method pretty useless
 * from a practical point of view.
 *
 * @param pszResampling one of "NEAREST", "GAUSS", "CUBIC", "AVERAGE", "MODE",
 * "AVERAGE_MAGPHASE" "RMS" or "NONE" controlling the downsampling method applied.
 * @param nOverviews number of overviews to build.
 * @param panOverviewList the list of overview decimation factors to build.
 * @param pfnProgress a function to call to report progress, or NULL.
 * @param pProgressData application data to pass to the progress function.
 *
 * @return CE_None on success or CE_Failure if the operation doesn't work.
 */

/**/
/**/

CPLErr GDALRasterBand::BuildOverviews( const char* /*pszResampling*/,
                                       int /*nOverviews*/,
                                       int* /*panOverviewList*/,
                                       GDALProgressFunc /*pfnProgress*/,
                                       void * /*pProgressData*/ )

{
    ReportError( CE_Failure, CPLE_NotSupported,
                 "BuildOverviews() not supported for this dataset." );

    return( CE_Failure );
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

/**
 * \brief Fetch the raster value offset.
 *
 * This value (in combination with the GetScale() value) can be used to
 * transform raw pixel values into the units returned by GetUnitType().
 * For example this might be used to store elevations in GUInt16 bands
 * with a precision of 0.1, and starting from -100.
 *
 * Units value = (raw value * scale) + offset
 *
 * Note that applying scale and offset is of the responsibility of the user,
 * and is not done by methods such as RasterIO() or ReadBlock().
 *
 * For file formats that don't know this intrinsically a value of zero
 * is returned.
 *
 * This method is the same as the C function GDALGetRasterOffset().
 *
 * @param pbSuccess pointer to a boolean to use to indicate if the
 * returned value is meaningful or not.  May be NULL (default).
 *
 * @return the raster offset.
 */

double GDALRasterBand::GetOffset( int *pbSuccess )

{
    if( pbSuccess != nullptr )
        *pbSuccess = FALSE;

    return 0.0;
}

/************************************************************************/
/*                        GDALGetRasterOffset()                         */
/************************************************************************/

/**
 * \brief Fetch the raster value offset.
 *
 * @see GDALRasterBand::GetOffset()
 */

double CPL_STDCALL GDALGetRasterOffset( GDALRasterBandH hBand, int *pbSuccess )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterOffset", 0 );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->GetOffset( pbSuccess );
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/

/**
 * \fn GDALRasterBand::SetOffset(double)
 * \brief Set scaling offset.
 *
 * Very few formats implement this method.   When not implemented it will
 * issue a CPLE_NotSupported error and return CE_Failure.
 *
 * This method is the same as the C function GDALSetRasterOffset().
 *
 * @param dfNewOffset the new offset.
 *
 * @return CE_None or success or CE_Failure on failure.
 */

/**/
/**/

CPLErr GDALRasterBand::SetOffset( double /*dfNewOffset*/ )
{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError( CE_Failure, CPLE_NotSupported,
                     "SetOffset() not supported on this raster band." );

    return CE_Failure;
}

/************************************************************************/
/*                        GDALSetRasterOffset()                         */
/************************************************************************/

/**
 * \brief Set scaling offset.
 *
 * @see GDALRasterBand::SetOffset()
 */

CPLErr CPL_STDCALL
GDALSetRasterOffset( GDALRasterBandH hBand, double dfNewOffset )

{
    VALIDATE_POINTER1( hBand, "GDALSetRasterOffset", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->SetOffset( dfNewOffset );
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

/**
 * \brief Fetch the raster value scale.
 *
 * This value (in combination with the GetOffset() value) can be used to
 * transform raw pixel values into the units returned by GetUnitType().
 * For example this might be used to store elevations in GUInt16 bands
 * with a precision of 0.1, and starting from -100.
 *
 * Units value = (raw value * scale) + offset
 *
 * Note that applying scale and offset is of the responsibility of the user,
 * and is not done by methods such as RasterIO() or ReadBlock().
 *
 * For file formats that don't know this intrinsically a value of one
 * is returned.
 *
 * This method is the same as the C function GDALGetRasterScale().
 *
 * @param pbSuccess pointer to a boolean to use to indicate if the
 * returned value is meaningful or not.  May be NULL (default).
 *
 * @return the raster scale.
 */

double GDALRasterBand::GetScale( int *pbSuccess )

{
    if( pbSuccess != nullptr )
        *pbSuccess = FALSE;

    return 1.0;
}

/************************************************************************/
/*                         GDALGetRasterScale()                         */
/************************************************************************/

/**
 * \brief Fetch the raster value scale.
 *
 * @see GDALRasterBand::GetScale()
 */

double CPL_STDCALL GDALGetRasterScale( GDALRasterBandH hBand, int *pbSuccess )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterScale", 0 );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->GetScale( pbSuccess );
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

/**
 * \fn GDALRasterBand::SetScale(double)
 * \brief Set scaling ratio.
 *
 * Very few formats implement this method.   When not implemented it will
 * issue a CPLE_NotSupported error and return CE_Failure.
 *
 * This method is the same as the C function GDALSetRasterScale().
 *
 * @param dfNewScale the new scale.
 *
 * @return CE_None or success or CE_Failure on failure.
 */

/**/
/**/

CPLErr GDALRasterBand::SetScale( double /*dfNewScale*/ )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError( CE_Failure, CPLE_NotSupported,
                     "SetScale() not supported on this raster band." );

    return CE_Failure;
}

/************************************************************************/
/*                        GDALSetRasterScale()                          */
/************************************************************************/

/**
 * \brief Set scaling ratio.
 *
 * @see GDALRasterBand::SetScale()
 */

CPLErr CPL_STDCALL
GDALSetRasterScale( GDALRasterBandH hBand, double dfNewOffset )

{
    VALIDATE_POINTER1( hBand, "GDALSetRasterScale", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->SetScale( dfNewOffset );
}

/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/

/**
 * \brief Return raster unit type.
 *
 * Return a name for the units of this raster's values.  For instance, it
 * might be "m" for an elevation model in meters, or "ft" for feet.  If no
 * units are available, a value of "" will be returned.  The returned string
 * should not be modified, nor freed by the calling application.
 *
 * This method is the same as the C function GDALGetRasterUnitType().
 *
 * @return unit name string.
 */

const char *GDALRasterBand::GetUnitType()

{
    return "";
}

/************************************************************************/
/*                       GDALGetRasterUnitType()                        */
/************************************************************************/

/**
 * \brief Return raster unit type.
 *
 * @see GDALRasterBand::GetUnitType()
 */

const char * CPL_STDCALL GDALGetRasterUnitType( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterUnitType", nullptr );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->GetUnitType();
}

/************************************************************************/
/*                            SetUnitType()                             */
/************************************************************************/

/**
 * \fn GDALRasterBand::SetUnitType(const char*)
 * \brief Set unit type.
 *
 * Set the unit type for a raster band.  Values should be one of
 * "" (the default indicating it is unknown), "m" indicating meters,
 * or "ft" indicating feet, though other nonstandard values are allowed.
 *
 * This method is the same as the C function GDALSetRasterUnitType().
 *
 * @param pszNewValue the new unit type value.
 *
 * @return CE_None on success or CE_Failure if not successful, or
 * unsupported.
 */

/**/
/**/

CPLErr GDALRasterBand::SetUnitType( const char * /*pszNewValue*/ )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError( CE_Failure, CPLE_NotSupported,
                     "SetUnitType() not supported on this raster band." );
    return CE_Failure;
}

/************************************************************************/
/*                       GDALSetRasterUnitType()                        */
/************************************************************************/

/**
 * \brief Set unit type.
 *
 * @see GDALRasterBand::SetUnitType()
 *
 * @since GDAL 1.8.0
 */

CPLErr CPL_STDCALL GDALSetRasterUnitType( GDALRasterBandH hBand, const char *pszNewValue )

{
    VALIDATE_POINTER1( hBand, "GDALSetRasterUnitType", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->SetUnitType(pszNewValue);
}

/************************************************************************/
/*                              GetXSize()                              */
/************************************************************************/

/**
 * \brief Fetch XSize of raster.
 *
 * This method is the same as the C function GDALGetRasterBandXSize().
 *
 * @return the width in pixels of this band.
 */

int GDALRasterBand::GetXSize()

{
    return nRasterXSize;
}

/************************************************************************/
/*                       GDALGetRasterBandXSize()                       */
/************************************************************************/

/**
 * \brief Fetch XSize of raster.
 *
 * @see GDALRasterBand::GetXSize()
 */

int CPL_STDCALL GDALGetRasterBandXSize( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterBandXSize", 0 );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->GetXSize();
}

/************************************************************************/
/*                              GetYSize()                              */
/************************************************************************/

/**
 * \brief Fetch YSize of raster.
 *
 * This method is the same as the C function GDALGetRasterBandYSize().
 *
 * @return the height in pixels of this band.
 */

int GDALRasterBand::GetYSize()

{
    return nRasterYSize;
}

/************************************************************************/
/*                       GDALGetRasterBandYSize()                       */
/************************************************************************/

/**
 * \brief Fetch YSize of raster.
 *
 * @see GDALRasterBand::GetYSize()
 */

int CPL_STDCALL GDALGetRasterBandYSize( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterBandYSize", 0 );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->GetYSize();
}

/************************************************************************/
/*                              GetBand()                               */
/************************************************************************/

/**
 * \brief Fetch the band number.
 *
 * This method returns the band that this GDALRasterBand objects represents
 * within its dataset.  This method may return a value of 0 to indicate
 * GDALRasterBand objects without an apparently relationship to a dataset,
 * such as GDALRasterBands serving as overviews.
 *
 * This method is the same as the C function GDALGetBandNumber().
 *
 * @return band number (1+) or 0 if the band number isn't known.
 */

int GDALRasterBand::GetBand()

{
    return nBand;
}

/************************************************************************/
/*                         GDALGetBandNumber()                          */
/************************************************************************/

/**
 * \brief Fetch the band number.
 *
 * @see GDALRasterBand::GetBand()
 */

int CPL_STDCALL GDALGetBandNumber( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetBandNumber", 0 );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->GetBand();
}

/************************************************************************/
/*                             GetDataset()                             */
/************************************************************************/

/**
 * \brief Fetch the owning dataset handle.
 *
 * Note that some GDALRasterBands are not considered to be a part of a dataset,
 * such as overviews or other "freestanding" bands.
 *
 * This method is the same as the C function GDALGetBandDataset().
 *
 * @return the pointer to the GDALDataset to which this band belongs, or
 * NULL if this cannot be determined.
 */

GDALDataset *GDALRasterBand::GetDataset()

{
    return poDS;
}

/************************************************************************/
/*                         GDALGetBandDataset()                         */
/************************************************************************/

/**
 * \brief Fetch the owning dataset handle.
 *
 * @see GDALRasterBand::GetDataset()
 */

GDALDatasetH CPL_STDCALL GDALGetBandDataset( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetBandDataset", nullptr );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return GDALDataset::ToHandle(poBand->GetDataset());
}

/************************************************************************/
/*                        ComputeFloatNoDataValue()                     */
/************************************************************************/

static inline void ComputeFloatNoDataValue( GDALDataType eDataType,
                                            double dfNoDataValue,
                                            int& bGotNoDataValue,
                                            float& fNoDataValue,
                                            bool& bGotFloatNoDataValue )
{
    if( eDataType == GDT_Float32 && bGotNoDataValue )
    {
        dfNoDataValue = GDALAdjustNoDataCloseToFloatMax(dfNoDataValue);
        if (GDALIsValueInRange<float>(dfNoDataValue) )
        {
            fNoDataValue = static_cast<float>(dfNoDataValue);
            bGotFloatNoDataValue = true;
            bGotNoDataValue = false;
        }
    }
}

/************************************************************************/
/*                            GetHistogram()                            */
/************************************************************************/

/**
 * \brief Compute raster histogram.
 *
 * Note that the bucket size is (dfMax-dfMin) / nBuckets.
 *
 * For example to compute a simple 256 entry histogram of eight bit data,
 * the following would be suitable.  The unusual bounds are to ensure that
 * bucket boundaries don't fall right on integer values causing possible errors
 * due to rounding after scaling.
\code{.cpp}
    GUIntBig anHistogram[256];

    poBand->GetHistogram( -0.5, 255.5, 256, anHistogram, FALSE, FALSE,
                          GDALDummyProgress, nullptr );
\endcode
 *
 * Note that setting bApproxOK will generally result in a subsampling of the
 * file, and will utilize overviews if available.  It should generally
 * produce a representative histogram for the data that is suitable for use
 * in generating histogram based luts for instance.  Generally bApproxOK is
 * much faster than an exactly computed histogram.
 *
 * This method is the same as the C functions GDALGetRasterHistogram() and
 * GDALGetRasterHistogramEx().
 *
 * @param dfMin the lower bound of the histogram.
 * @param dfMax the upper bound of the histogram.
 * @param nBuckets the number of buckets in panHistogram.
 * @param panHistogram array into which the histogram totals are placed.
 * @param bIncludeOutOfRange if TRUE values below the histogram range will
 * mapped into panHistogram[0], and values above will be mapped into
 * panHistogram[nBuckets-1] otherwise out of range values are discarded.
 * @param bApproxOK TRUE if an approximate, or incomplete histogram OK.
 * @param pfnProgress function to report progress to completion.
 * @param pProgressData application data to pass to pfnProgress.
 *
 * @return CE_None on success, or CE_Failure if something goes wrong.
 */

CPLErr GDALRasterBand::GetHistogram( double dfMin, double dfMax,
                                     int nBuckets, GUIntBig *panHistogram,
                                     int bIncludeOutOfRange, int bApproxOK,
                                     GDALProgressFunc pfnProgress,
                                     void *pProgressData )

{
    CPLAssert( nullptr != panHistogram );

    if( pfnProgress == nullptr )
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      If we have overviews, use them for the histogram.               */
/* -------------------------------------------------------------------- */
    if( bApproxOK && GetOverviewCount() > 0 && !HasArbitraryOverviews() )
    {
        // FIXME: should we use the most reduced overview here or use some
        // minimum number of samples like GDALRasterBand::ComputeStatistics()
        // does?
        GDALRasterBand *poBestOverview = GetRasterSampleOverview( 0 );

        if( poBestOverview != this )
        {
            return poBestOverview->GetHistogram( dfMin, dfMax, nBuckets,
                                                 panHistogram,
                                                 bIncludeOutOfRange, bApproxOK,
                                                 pfnProgress, pProgressData );
        }
    }

/* -------------------------------------------------------------------- */
/*      Read actual data and build histogram.                           */
/* -------------------------------------------------------------------- */
    if( !pfnProgress( 0.0, "Compute Histogram", pProgressData ) )
    {
        ReportError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);

    const double dfScale = (dfMax > dfMin) ? nBuckets / (dfMax - dfMin) : 0.0;
    memset( panHistogram, 0, sizeof(GUIntBig) * nBuckets );

    int bGotNoDataValue = FALSE;
    const double dfNoDataValue = GetNoDataValue( &bGotNoDataValue );
    bGotNoDataValue = bGotNoDataValue && !CPLIsNan(dfNoDataValue);
    // Not advertized. May be removed at any time. Just as a provision if the
    // old behavior made sense sometimes.
    bGotNoDataValue = bGotNoDataValue &&
        !CPLTestBool(CPLGetConfigOption("GDAL_NODATA_IN_HISTOGRAM", "NO"));
    bool bGotFloatNoDataValue = false;
    float fNoDataValue = 0.0f;
    ComputeFloatNoDataValue( eDataType, dfNoDataValue, bGotNoDataValue,
                            fNoDataValue, bGotFloatNoDataValue );

    const char* pszPixelType = GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
    const bool bSignedByte =
        pszPixelType != nullptr && EQUAL(pszPixelType, "SIGNEDBYTE");

    if ( bApproxOK && HasArbitraryOverviews() )
    {
/* -------------------------------------------------------------------- */
/*      Figure out how much the image should be reduced to get an       */
/*      approximate value.                                              */
/* -------------------------------------------------------------------- */
        const double dfReduction = sqrt(
            static_cast<double>(nRasterXSize) * nRasterYSize /
            GDALSTAT_APPROX_NUMSAMPLES );

        int nXReduced = nRasterXSize;
        int nYReduced = nRasterYSize;
        if ( dfReduction > 1.0 )
        {
            nXReduced = static_cast<int>( nRasterXSize / dfReduction );
            nYReduced = static_cast<int>( nRasterYSize / dfReduction );

            // Catch the case of huge resizing ratios here
            if ( nXReduced == 0 )
                nXReduced = 1;
            if ( nYReduced == 0 )
                nYReduced = 1;
        }

        void *pData =
            CPLMalloc(
                GDALGetDataTypeSizeBytes(eDataType) * nXReduced * nYReduced );

        const CPLErr eErr =
            IRasterIO(
                GF_Read, 0, 0, nRasterXSize, nRasterYSize, pData,
                nXReduced, nYReduced, eDataType, 0, 0, &sExtraArg );
        if ( eErr != CE_None )
        {
            CPLFree(pData);
            return eErr;
        }

        // This isn't the fastest way to do this, but is easier for now.
        for( int iY = 0; iY < nYReduced; iY++ )
        {
            for( int iX = 0; iX < nXReduced; iX++ )
            {
                const int iOffset = iX + iY * nXReduced;
                double dfValue = 0.0;

                switch( eDataType )
                {
                  case GDT_Byte:
                  {
                    if( bSignedByte )
                        dfValue = static_cast<signed char *>(pData)[iOffset];
                    else
                        dfValue = static_cast<GByte *>(pData)[iOffset];
                    break;
                  }
                  case GDT_UInt16:
                    dfValue = static_cast<GUInt16 *>(pData)[iOffset];
                    break;
                  case GDT_Int16:
                    dfValue = static_cast<GInt16 *>(pData)[iOffset];
                    break;
                  case GDT_UInt32:
                    dfValue = static_cast<GUInt32 *>(pData)[iOffset];
                    break;
                  case GDT_Int32:
                    dfValue = static_cast<GInt32 *>(pData)[iOffset];
                    break;
                  case GDT_Float32:
                  {
                    const float fValue = static_cast<float *>(pData)[iOffset];
                    if( CPLIsNan(fValue) ||
                        (bGotFloatNoDataValue && ARE_REAL_EQUAL(fValue, fNoDataValue)) )
                        continue;
                    dfValue = fValue;
                    break;
                  }
                  case GDT_Float64:
                    dfValue = static_cast<double *>(pData)[iOffset];
                    if( CPLIsNan(dfValue) )
                        continue;
                    break;
                  case GDT_CInt16:
                    {
                        const double dfReal =
                            static_cast<GInt16 *>(pData)[iOffset*2];
                        const double dfImag =
                            static_cast<GInt16 *>(pData)[iOffset*2+1];
                        if ( CPLIsNan(dfReal) || CPLIsNan(dfImag) )
                            continue;
                        dfValue = sqrt( dfReal * dfReal + dfImag * dfImag );
                    }
                    break;
                  case GDT_CInt32:
                    {
                        const double dfReal =
                            static_cast<GInt32 *>(pData)[iOffset*2];
                        const double dfImag =
                            static_cast<GInt32 *>(pData)[iOffset*2+1];
                        if ( CPLIsNan(dfReal) || CPLIsNan(dfImag) )
                            continue;
                        dfValue = sqrt( dfReal * dfReal + dfImag * dfImag );
                    }
                    break;
                  case GDT_CFloat32:
                    {
                        const double dfReal =
                            static_cast<float *>(pData)[iOffset*2];
                        const double dfImag =
                            static_cast<float *>(pData)[iOffset*2+1];
                        if ( CPLIsNan(dfReal) || CPLIsNan(dfImag) )
                            continue;
                        dfValue = sqrt( dfReal * dfReal + dfImag * dfImag );
                    }
                    break;
                  case GDT_CFloat64:
                    {
                        const double dfReal =
                            static_cast<double *>(pData)[iOffset*2];
                        const double dfImag =
                            static_cast<double *>(pData)[iOffset*2+1];
                        if ( CPLIsNan(dfReal) || CPLIsNan(dfImag) )
                            continue;
                        dfValue = sqrt( dfReal * dfReal + dfImag * dfImag );
                    }
                    break;
                  default:
                    CPLAssert( false );
                }

                if( eDataType != GDT_Float32 &&
                    bGotNoDataValue && ARE_REAL_EQUAL(dfValue, dfNoDataValue) )
                    continue;

                const int nIndex =
                    static_cast<int>(floor((dfValue - dfMin) * dfScale));

                if( nIndex < 0 )
                {
                    if( bIncludeOutOfRange )
                        panHistogram[0]++;
                }
                else if( nIndex >= nBuckets )
                {
                    if( bIncludeOutOfRange )
                        ++panHistogram[nBuckets-1];
                }
                else
                {
                    ++panHistogram[nIndex];
                }
            }
        }

        CPLFree( pData );
    }
    else  // No arbitrary overviews.
    {
        if( !InitBlockInfo() )
            return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Figure out the ratio of blocks we will read to get an           */
/*      approximate value.                                              */
/* -------------------------------------------------------------------- */

        int nSampleRate = 1;
        if ( bApproxOK )
        {
            nSampleRate = static_cast<int>(
                std::max(1.0,
                         sqrt(static_cast<double>(nBlocksPerRow) *
                              nBlocksPerColumn)));
            // We want to avoid probing only the first column of blocks for
            // a square shaped raster, because it is not unlikely that it may
            // be padding only (#6378).
            if( nSampleRate == nBlocksPerRow && nBlocksPerRow > 1 )
              nSampleRate += 1;
        }

/* -------------------------------------------------------------------- */
/*      Read the blocks, and add to histogram.                          */
/* -------------------------------------------------------------------- */
        for( int iSampleBlock = 0;
             iSampleBlock < nBlocksPerRow * nBlocksPerColumn;
             iSampleBlock += nSampleRate )
        {
            if( !pfnProgress(
                    iSampleBlock /
                        (static_cast<double>(nBlocksPerRow) * nBlocksPerColumn),
                    "Compute Histogram", pProgressData ) )
                return CE_Failure;

            const int iYBlock = iSampleBlock / nBlocksPerRow;
            const int iXBlock = iSampleBlock - nBlocksPerRow * iYBlock;

            GDALRasterBlock *poBlock = GetLockedBlockRef( iXBlock, iYBlock );
            if( poBlock == nullptr )
                return CE_Failure;

            void *pData = poBlock->GetDataRef();

            int nXCheck = 0, nYCheck = 0;
            GetActualBlockSize(iXBlock, iYBlock, &nXCheck, &nYCheck);

            // this is a special case for a common situation.
            if( eDataType == GDT_Byte && !bSignedByte
                && dfScale == 1.0 && (dfMin >= -0.5 && dfMin <= 0.5)
                && nYCheck == nBlockYSize && nXCheck == nBlockXSize
                && nBuckets == 256 )
            {
                const GPtrDiff_t nPixels = static_cast<GPtrDiff_t>(nXCheck) * nYCheck;
                GByte *pabyData = static_cast<GByte *>(pData);

                for( GPtrDiff_t i = 0; i < nPixels; i++ )
                    if( ! (bGotNoDataValue &&
                           (pabyData[i] == static_cast<GByte>(dfNoDataValue))))
                    {
                        panHistogram[pabyData[i]]++;
                    }

                poBlock->DropLock();
                continue;  // To next sample block.
            }

            // This isn't the fastest way to do this, but is easier for now.
            for( int iY = 0; iY < nYCheck; iY++ )
            {
                for( int iX = 0; iX < nXCheck; iX++ )
                {
                    const GPtrDiff_t iOffset = iX + static_cast<GPtrDiff_t>(iY) * nBlockXSize;
                    double dfValue = 0.0;

                    switch( eDataType )
                    {
                      case GDT_Byte:
                      {
                        if( bSignedByte )
                            dfValue =
                                static_cast<signed char *>(pData)[iOffset];
                        else
                            dfValue = static_cast<GByte *>(pData)[iOffset];
                        break;
                      }
                      case GDT_UInt16:
                        dfValue = static_cast<GUInt16 *>(pData)[iOffset];
                        break;
                      case GDT_Int16:
                        dfValue = static_cast<GInt16 *>(pData)[iOffset];
                        break;
                      case GDT_UInt32:
                        dfValue = static_cast<GUInt32 *>(pData)[iOffset];
                        break;
                      case GDT_Int32:
                        dfValue = static_cast<GInt32 *>(pData)[iOffset];
                        break;
                      case GDT_Float32:
                      {
                        const float fValue = static_cast<float *>(pData)[iOffset];
                        if( CPLIsNan(fValue) ||
                            (bGotFloatNoDataValue && ARE_REAL_EQUAL(fValue, fNoDataValue)) )
                            continue;
                        dfValue = fValue;
                        break;
                      }
                      case GDT_Float64:
                        dfValue = static_cast<double *>(pData)[iOffset];
                        if( CPLIsNan(dfValue) )
                            continue;
                        break;
                      case GDT_CInt16:
                        {
                            double  dfReal =
                                static_cast<GInt16 *>(pData)[iOffset*2];
                            double  dfImag =
                                static_cast<GInt16 *>(pData)[iOffset*2+1];
                            dfValue = sqrt( dfReal * dfReal + dfImag * dfImag );
                        }
                        break;
                      case GDT_CInt32:
                        {
                            double  dfReal =
                                static_cast<GInt32 *>(pData)[iOffset*2];
                            double  dfImag =
                                static_cast<GInt32 *>(pData)[iOffset*2+1];
                            dfValue = sqrt( dfReal * dfReal + dfImag * dfImag );
                        }
                        break;
                      case GDT_CFloat32:
                        {
                            double  dfReal =
                                static_cast<float *>(pData)[iOffset*2];
                            double  dfImag =
                                static_cast<float *>(pData)[iOffset*2+1];
                            if ( CPLIsNan(dfReal) || CPLIsNan(dfImag) )
                                continue;
                            dfValue = sqrt( dfReal * dfReal + dfImag * dfImag );
                        }
                        break;
                      case GDT_CFloat64:
                        {
                            double  dfReal =
                                static_cast<double *>(pData)[iOffset*2];
                            double  dfImag =
                                static_cast<double *>(pData)[iOffset*2+1];
                            if ( CPLIsNan(dfReal) || CPLIsNan(dfImag) )
                                continue;
                            dfValue = sqrt( dfReal * dfReal + dfImag * dfImag );
                        }
                        break;
                      default:
                        CPLAssert( false );
                        return CE_Failure;
                    }

                    if( eDataType != GDT_Float32 && bGotNoDataValue &&
                        ARE_REAL_EQUAL(dfValue, dfNoDataValue) )
                        continue;

                    const int nIndex =
                        static_cast<int>(floor((dfValue - dfMin) * dfScale));

                    if( nIndex < 0 )
                    {
                        if( bIncludeOutOfRange )
                            ++panHistogram[0];
                    }
                    else if( nIndex >= nBuckets )
                    {
                        if( bIncludeOutOfRange )
                            ++panHistogram[nBuckets-1];
                    }
                    else
                    {
                        panHistogram[nIndex]++;
                    }
                }
            }

            poBlock->DropLock();
        }
    }

    pfnProgress( 1.0, "Compute Histogram", pProgressData );

    return CE_None;
}

/************************************************************************/
/*                       GDALGetRasterHistogram()                       */
/************************************************************************/

/**
 * \brief Compute raster histogram.
 *
 * Use GDALGetRasterHistogramEx() instead to get correct counts for values
 * exceeding 2 billion.
 *
 * @see GDALRasterBand::GetHistogram()
 * @see GDALGetRasterHistogramEx()
 */

CPLErr CPL_STDCALL
GDALGetRasterHistogram( GDALRasterBandH hBand,
                        double dfMin, double dfMax,
                        int nBuckets, int *panHistogram,
                        int bIncludeOutOfRange, int bApproxOK,
                        GDALProgressFunc pfnProgress,
                        void *pProgressData )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterHistogram", CE_Failure );
    VALIDATE_POINTER1( panHistogram, "GDALGetRasterHistogram", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);

    GUIntBig* panHistogramTemp = static_cast<GUIntBig *>(
        VSIMalloc2(sizeof(GUIntBig), nBuckets) );
    if( panHistogramTemp == nullptr )
    {
        poBand->ReportError(
            CE_Failure, CPLE_OutOfMemory,
            "Out of memory in GDALGetRasterHistogram()." );
        return CE_Failure;
    }

    CPLErr eErr = poBand->GetHistogram(
        dfMin, dfMax, nBuckets, panHistogramTemp,
        bIncludeOutOfRange, bApproxOK,
        pfnProgress, pProgressData );

    if( eErr == CE_None )
    {
        for(int i=0;i<nBuckets;i++)
        {
            if( panHistogramTemp[i] > INT_MAX )
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Count for bucket %d, which is " CPL_FRMT_GUIB
                    " exceeds maximum 32 bit value",
                    i, panHistogramTemp[i] );
                panHistogram[i] = INT_MAX;
            }
            else
            {
                panHistogram[i] = static_cast<int>(panHistogramTemp[i]);
            }
        }
    }

    CPLFree(panHistogramTemp);

    return eErr;
}

/************************************************************************/
/*                      GDALGetRasterHistogramEx()                      */
/************************************************************************/

/**
 * \brief Compute raster histogram.
 *
 * @see GDALRasterBand::GetHistogram()
 *
 * @since GDAL 2.0
 */

CPLErr CPL_STDCALL
GDALGetRasterHistogramEx( GDALRasterBandH hBand,
                          double dfMin, double dfMax,
                          int nBuckets, GUIntBig *panHistogram,
                          int bIncludeOutOfRange, int bApproxOK,
                          GDALProgressFunc pfnProgress,
                          void *pProgressData )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterHistogramEx", CE_Failure );
    VALIDATE_POINTER1( panHistogram, "GDALGetRasterHistogramEx", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);

    return poBand->GetHistogram( dfMin, dfMax, nBuckets, panHistogram,
                                 bIncludeOutOfRange, bApproxOK,
                                 pfnProgress, pProgressData );
}

/************************************************************************/
/*                        GetDefaultHistogram()                         */
/************************************************************************/

/**
 * \brief Fetch default raster histogram.
 *
 * The default method in GDALRasterBand will compute a default histogram. This
 * method is overridden by derived classes (such as GDALPamRasterBand,
 * VRTDataset, HFADataset...) that may be able to fetch efficiently an already
 * stored histogram.
 *
 * This method is the same as the C functions GDALGetDefaultHistogram() and
 * GDALGetDefaultHistogramEx().
 *
 * @param pdfMin pointer to double value that will contain the lower bound of
 * the histogram.
 * @param pdfMax pointer to double value that will contain the upper bound of
 * the histogram.
 * @param pnBuckets pointer to int value that will contain the number of buckets
 * in *ppanHistogram.
 * @param ppanHistogram pointer to array into which the histogram totals are
 * placed. To be freed with VSIFree
 * @param bForce TRUE to force the computation. If FALSE and no default
 * histogram is available, the method will return CE_Warning
 * @param pfnProgress function to report progress to completion.
 * @param pProgressData application data to pass to pfnProgress.
 *
 * @return CE_None on success, CE_Failure if something goes wrong, or
 * CE_Warning if no default histogram is available.
 */

CPLErr
    GDALRasterBand::GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                         int *pnBuckets,
                                         GUIntBig **ppanHistogram,
                                         int bForce,
                                         GDALProgressFunc pfnProgress,
                                         void *pProgressData )

{
    CPLAssert( nullptr != pnBuckets );
    CPLAssert( nullptr != ppanHistogram );
    CPLAssert( nullptr != pdfMin );
    CPLAssert( nullptr != pdfMax );

    *pnBuckets = 0;
    *ppanHistogram = nullptr;

    if( !bForce )
        return CE_Warning;

    const int nBuckets = 256;

    const char* pszPixelType = GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
    const int bSignedByte =
        pszPixelType != nullptr && EQUAL(pszPixelType, "SIGNEDBYTE");

    if( GetRasterDataType() == GDT_Byte && !bSignedByte)
    {
        *pdfMin = -0.5;
        *pdfMax = 255.5;
    }
    else
    {

        const CPLErr eErr =
            GetStatistics( TRUE, TRUE, pdfMin, pdfMax, nullptr, nullptr );
        const double dfHalfBucket = (*pdfMax - *pdfMin) / (2 * (nBuckets - 1));
        *pdfMin -= dfHalfBucket;
        *pdfMax += dfHalfBucket;

        if( eErr != CE_None )
            return eErr;
    }

    *ppanHistogram = static_cast<GUIntBig *>(
        VSICalloc(sizeof(GUIntBig), nBuckets) );
    if( *ppanHistogram == nullptr )
    {
        ReportError( CE_Failure, CPLE_OutOfMemory,
                  "Out of memory in InitBlockInfo()." );
        return CE_Failure;
    }

    *pnBuckets = nBuckets;
    CPLErr eErr = GetHistogram( *pdfMin, *pdfMax, *pnBuckets, *ppanHistogram,
                         TRUE, FALSE, pfnProgress, pProgressData );
    if( eErr != CE_None )
    {
        *pnBuckets = 0;
    }
    return eErr;
}

/************************************************************************/
/*                      GDALGetDefaultHistogram()                       */
/************************************************************************/

/**
  * \brief Fetch default raster histogram.
  *
  * Use GDALGetRasterHistogramEx() instead to get correct counts for values
  * exceeding 2 billion.
  *
  * @see GDALRasterBand::GDALGetDefaultHistogram()
  * @see GDALGetRasterHistogramEx()
  */

CPLErr CPL_STDCALL GDALGetDefaultHistogram(
    GDALRasterBandH hBand,
    double *pdfMin, double *pdfMax,
    int *pnBuckets, int **ppanHistogram,
    int bForce,
    GDALProgressFunc pfnProgress,
    void *pProgressData )

{
    VALIDATE_POINTER1( hBand, "GDALGetDefaultHistogram", CE_Failure );
    VALIDATE_POINTER1( pdfMin, "GDALGetDefaultHistogram", CE_Failure );
    VALIDATE_POINTER1( pdfMax, "GDALGetDefaultHistogram", CE_Failure );
    VALIDATE_POINTER1( pnBuckets, "GDALGetDefaultHistogram", CE_Failure );
    VALIDATE_POINTER1( ppanHistogram, "GDALGetDefaultHistogram", CE_Failure );

    GDALRasterBand * const poBand = GDALRasterBand::FromHandle(hBand);
    GUIntBig* panHistogramTemp = nullptr;
    CPLErr eErr = poBand->GetDefaultHistogram( pdfMin, pdfMax,
        pnBuckets, &panHistogramTemp, bForce, pfnProgress, pProgressData );
    if( eErr == CE_None )
    {
        const int nBuckets = *pnBuckets;
        *ppanHistogram = static_cast<int *>(VSIMalloc2(sizeof(int), nBuckets));
        if( *ppanHistogram == nullptr )
        {
            poBand->ReportError(
                CE_Failure, CPLE_OutOfMemory,
                "Out of memory in GDALGetDefaultHistogram()." );
            VSIFree(panHistogramTemp);
            return CE_Failure;
        }

        for( int i = 0; i < nBuckets; ++i )
        {
            if( panHistogramTemp[i] > INT_MAX )
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Count for bucket %d, which is " CPL_FRMT_GUIB
                    " exceeds maximum 32 bit value",
                    i, panHistogramTemp[i] );
                (*ppanHistogram)[i] = INT_MAX;
            }
            else
            {
                (*ppanHistogram)[i] = static_cast<int>(panHistogramTemp[i]);
            }
        }

        CPLFree(panHistogramTemp);
    }
    else
    {
        *ppanHistogram = nullptr;
    }

    return eErr;
}

/************************************************************************/
/*                      GDALGetDefaultHistogramEx()                     */
/************************************************************************/

/**
  * \brief Fetch default raster histogram.
  *
  * @see GDALRasterBand::GetDefaultHistogram()
  *
  * @since GDAL 2.0
  */

CPLErr CPL_STDCALL GDALGetDefaultHistogramEx(
    GDALRasterBandH hBand,
    double *pdfMin, double *pdfMax,
    int *pnBuckets, GUIntBig **ppanHistogram,
    int bForce,
    GDALProgressFunc pfnProgress,
    void *pProgressData )

{
    VALIDATE_POINTER1( hBand, "GDALGetDefaultHistogram", CE_Failure );
    VALIDATE_POINTER1( pdfMin, "GDALGetDefaultHistogram", CE_Failure );
    VALIDATE_POINTER1( pdfMax, "GDALGetDefaultHistogram", CE_Failure );
    VALIDATE_POINTER1( pnBuckets, "GDALGetDefaultHistogram", CE_Failure );
    VALIDATE_POINTER1( ppanHistogram, "GDALGetDefaultHistogram", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->GetDefaultHistogram( pdfMin, pdfMax,
        pnBuckets, ppanHistogram, bForce, pfnProgress, pProgressData );
}
/************************************************************************/
/*                             AdviseRead()                             */
/************************************************************************/

/**
 * \fn GDALRasterBand::AdviseRead(int,int,int,int,int,int,GDALDataType,char**)
 * \brief Advise driver of upcoming read requests.
 *
 * Some GDAL drivers operate more efficiently if they know in advance what
 * set of upcoming read requests will be made.  The AdviseRead() method allows
 * an application to notify the driver of the region of interest,
 * and at what resolution the region will be read.
 *
 * Many drivers just ignore the AdviseRead() call, but it can dramatically
 * accelerate access via some drivers.
 *
 * Depending on call paths, drivers might receive several calls to
 * AdviseRead() with the same parameters.
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
 * @param nBufXSize the width of the buffer image into which the desired region
 * is to be read, or from which it is to be written.
 *
 * @param nBufYSize the height of the buffer image into which the desired
 * region is to be read, or from which it is to be written.
 *
 * @param eBufType the type of the pixel values in the pData data buffer.  The
 * pixel values will automatically be translated to/from the GDALRasterBand
 * data type as needed.
 *
 * @param papszOptions a list of name=value strings with special control
 * options.  Normally this is NULL.
 *
 * @return CE_Failure if the request is invalid and CE_None if it works or
 * is ignored.
 */

/**/
/**/

CPLErr GDALRasterBand::AdviseRead(
    int /*nXOff*/,
    int /*nYOff*/,
    int /*nXSize*/,
    int /*nYSize*/,
    int /*nBufXSize*/,
    int /*nBufYSize*/,
    GDALDataType /*eBufType*/,
    char ** /*papszOptions*/ )
{
    return CE_None;
}

/************************************************************************/
/*                        GDALRasterAdviseRead()                        */
/************************************************************************/

/**
 * \brief Advise driver of upcoming read requests.
 *
 * @see GDALRasterBand::AdviseRead()
 */

CPLErr CPL_STDCALL
GDALRasterAdviseRead( GDALRasterBandH hBand,
                      int nXOff, int nYOff, int nXSize, int nYSize,
                      int nBufXSize, int nBufYSize,
                      GDALDataType eDT, CSLConstList papszOptions )

{
    VALIDATE_POINTER1( hBand, "GDALRasterAdviseRead", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->AdviseRead( nXOff, nYOff, nXSize, nYSize,
        nBufXSize, nBufYSize, eDT, const_cast<char**>(papszOptions) );
}

/************************************************************************/
/*                           GetStatistics()                            */
/************************************************************************/

/**
 * \brief Fetch image statistics.
 *
 * Returns the minimum, maximum, mean and standard deviation of all
 * pixel values in this band.  If approximate statistics are sufficient,
 * the bApproxOK flag can be set to true in which case overviews, or a
 * subset of image tiles may be used in computing the statistics.
 *
 * If bForce is FALSE results will only be returned if it can be done
 * quickly (i.e. without scanning the data).  If bForce is FALSE and
 * results cannot be returned efficiently, the method will return CE_Warning
 * but no warning will have been issued.   This is a non-standard use of
 * the CE_Warning return value to indicate "nothing done".
 *
 * Note that file formats using PAM (Persistent Auxiliary Metadata) services
 * will generally cache statistics in the .pam file allowing fast fetch
 * after the first request.
 *
 * This method is the same as the C function GDALGetRasterStatistics().
 *
 * @param bApproxOK If TRUE statistics may be computed based on overviews
 * or a subset of all tiles.
 *
 * @param bForce If FALSE statistics will only be returned if it can
 * be done without rescanning the image.
 *
 * @param pdfMin Location into which to load image minimum (may be NULL).
 *
 * @param pdfMax Location into which to load image maximum (may be NULL).-
 *
 * @param pdfMean Location into which to load image mean (may be NULL).
 *
 * @param pdfStdDev Location into which to load image standard deviation
 * (may be NULL).
 *
 * @return CE_None on success, CE_Warning if no values returned,
 * CE_Failure if an error occurs.
 */

CPLErr GDALRasterBand::GetStatistics( int bApproxOK, int bForce,
                                      double *pdfMin, double *pdfMax,
                                      double *pdfMean, double *pdfStdDev )

{
/* -------------------------------------------------------------------- */
/*      Do we already have metadata items for the requested values?     */
/* -------------------------------------------------------------------- */
    if( (pdfMin == nullptr || GetMetadataItem("STATISTICS_MINIMUM") != nullptr)
     && (pdfMax == nullptr || GetMetadataItem("STATISTICS_MAXIMUM") != nullptr)
     && (pdfMean == nullptr || GetMetadataItem("STATISTICS_MEAN") != nullptr)
     && (pdfStdDev == nullptr || GetMetadataItem("STATISTICS_STDDEV") != nullptr) )
    {
        if( !(GetMetadataItem("STATISTICS_APPROXIMATE") && !bApproxOK) )
        {
            if( pdfMin != nullptr )
                *pdfMin = CPLAtofM(GetMetadataItem("STATISTICS_MINIMUM"));
            if( pdfMax != nullptr )
                *pdfMax = CPLAtofM(GetMetadataItem("STATISTICS_MAXIMUM"));
            if( pdfMean != nullptr )
                *pdfMean = CPLAtofM(GetMetadataItem("STATISTICS_MEAN"));
            if( pdfStdDev != nullptr )
                *pdfStdDev = CPLAtofM(GetMetadataItem("STATISTICS_STDDEV"));

            return CE_None;
        }
    }

/* -------------------------------------------------------------------- */
/*      Does the driver already know the min/max?                       */
/* -------------------------------------------------------------------- */
    if( bApproxOK && pdfMean == nullptr && pdfStdDev == nullptr )
    {
        int bSuccessMin = FALSE;
        int bSuccessMax = FALSE;

        const double dfMin = GetMinimum( &bSuccessMin );
        const double dfMax = GetMaximum( &bSuccessMax );

        if( bSuccessMin && bSuccessMax )
        {
            if( pdfMin != nullptr )
                *pdfMin = dfMin;
            if( pdfMax != nullptr )
                *pdfMax = dfMax;
            return CE_None;
        }
    }

/* -------------------------------------------------------------------- */
/*      Either return without results, or force computation.            */
/* -------------------------------------------------------------------- */
    if( !bForce )
        return CE_Warning;
    else
        return ComputeStatistics( bApproxOK,
                                  pdfMin, pdfMax, pdfMean, pdfStdDev,
                                  GDALDummyProgress, nullptr );
}

/************************************************************************/
/*                      GDALGetRasterStatistics()                       */
/************************************************************************/

/**
 * \brief Fetch image statistics.
 *
 * @see GDALRasterBand::GetStatistics()
 */

CPLErr CPL_STDCALL GDALGetRasterStatistics(
    GDALRasterBandH hBand, int bApproxOK, int bForce,
    double *pdfMin, double *pdfMax, double *pdfMean, double *pdfStdDev )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterStatistics", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->GetStatistics(
        bApproxOK, bForce, pdfMin, pdfMax, pdfMean, pdfStdDev );
}

#ifdef CPL_HAS_GINT64

/************************************************************************/
/*                         GDALUInt128                                  */
/************************************************************************/

#ifdef HAVE_UINT128_T
class GDALUInt128
{
        __uint128_t val;

        explicit GDALUInt128(__uint128_t valIn) : val(valIn) {}

    public:
        static GDALUInt128 Mul(GUIntBig first, GUIntBig second)
        {
            // Evaluates to just a single mul on x86_64
            return GDALUInt128(static_cast<__uint128_t>(first) * second);
        }

        GDALUInt128 operator- (const GDALUInt128& other) const
        {
            return GDALUInt128(val - other.val);
        }

        operator double() const
        {
            return static_cast<double>(val);
        }
};
#else

#if defined(_MSC_VER) && defined(_M_X64)
#include <intrin.h>
#endif

class GDALUInt128
{
        GUIntBig low, high;

        GDALUInt128(GUIntBig lowIn, GUIntBig highIn):
                                        low(lowIn), high(highIn) {}

    public:
        static GDALUInt128 Mul(GUIntBig first, GUIntBig second)
        {
#if defined(_MSC_VER) && defined(_M_X64)
            GUIntBig highRes;
            GUIntBig lowRes = _umul128(first, second, &highRes);
            return GDALUInt128(lowRes, highRes);
#else
            const GUInt32 firstLow = static_cast<GUInt32>(first);
            const GUInt32 firstHigh = static_cast<GUInt32>(first >> 32);
            const GUInt32 secondLow = static_cast<GUInt32>(second);
            const GUInt32 secondHigh = static_cast<GUInt32>(second >> 32);
            GUIntBig highRes = 0;
            const GUIntBig firstLowSecondHigh =
                    static_cast<GUIntBig>(firstLow) * secondHigh;
            const GUIntBig firstHighSecondLow =
                    static_cast<GUIntBig>(firstHigh) * secondLow;
            const GUIntBig middleTerm = firstLowSecondHigh + firstHighSecondLow;
            if( middleTerm < firstLowSecondHigh ) // check for overflow
                highRes += static_cast<GUIntBig>(1) << 32;
            const GUIntBig firstLowSecondLow =
                    static_cast<GUIntBig>(firstLow) * secondLow;
            GUIntBig lowRes = firstLowSecondLow + (middleTerm << 32);
            if( lowRes < firstLowSecondLow ) // check for overflow
                highRes ++;
            highRes += (middleTerm >> 32) +
                            static_cast<GUIntBig>(firstHigh) * secondHigh;
            return GDALUInt128(lowRes, highRes);
#endif
        }

        GDALUInt128 operator- (const GDALUInt128& other) const
        {
            GUIntBig highRes = high - other.high;
            GUIntBig lowRes = low - other.low;
            if (lowRes > low) // check for underflow
                --highRes;
            return GDALUInt128(lowRes, highRes);
        }

        operator double() const
        {
            const double twoPow64 = 18446744073709551616.0;
            return high * twoPow64 + low;
        }
};
#endif

/************************************************************************/
/*                    ComputeStatisticsInternal()                       */
/************************************************************************/

// Just to make coverity scan happy w.r.t overflow_before_widen, but otherwise not needed.
#define static_cast_for_coverity_scan static_cast

// The rationale for below optimizations is detailed in statistics.txt

// Use with T = GByte or GUInt16 only !
template<class T, bool COMPUTE_OTHER_STATS> struct ComputeStatisticsInternalGeneric
{
    static void f( int nXCheck,
                                       int nBlockXSize,
                                       int nYCheck,
                                       const T* pData,
                                       bool bHasNoData,
                                       GUInt32 nNoDataValue,
                                       GUInt32& nMin,
                                       GUInt32& nMax,
                                       GUIntBig& nSum,
                                       GUIntBig& nSumSquare,
                                       GUIntBig& nSampleCount,
                                       GUIntBig& nValidCount )
{
    static_assert( std::is_same<T, GByte>::value ||
                   std::is_same<T, GUInt16>::value, "bad type for T" );
    if( bHasNoData )
    {
        // General case
        for( int iY = 0; iY < nYCheck; iY++ )
        {
            for( int iX = 0; iX < nXCheck; iX++ )
            {
                const GPtrDiff_t iOffset = iX + static_cast<GPtrDiff_t>(iY) * nBlockXSize;
                const GUInt32 nValue = pData[iOffset];
                if( nValue == nNoDataValue )
                    continue;
                if( nValue < nMin )
                    nMin = nValue;
                if( nValue > nMax )
                    nMax = nValue;
                if( COMPUTE_OTHER_STATS )
                {
                    nValidCount ++;
                    nSum += nValue;
                    nSumSquare += static_cast_for_coverity_scan<GUIntBig>(nValue) * nValue;
                }
            }
        }
        if( COMPUTE_OTHER_STATS )
        {
            nSampleCount += static_cast<GUIntBig>(nXCheck) * nYCheck;
        }
    }
    else if( nMin == std::numeric_limits<T>::min() &&
             nMax == std::numeric_limits<T>::max() )
    {
        if( !COMPUTE_OTHER_STATS )
        {
            return;
        }
        // Optimization when there is no nodata and we know we have already
        // reached the min and max
        for( int iY = 0; iY < nYCheck; iY++ )
        {
            int iX;
            for( iX = 0; iX + 3 < nXCheck; iX+=4 )
            {
                const GPtrDiff_t iOffset = iX + static_cast<GPtrDiff_t>(iY) * nBlockXSize;
                const GUIntBig nValue = pData[iOffset];
                const GUIntBig nValue2 = pData[iOffset+1];
                const GUIntBig nValue3 = pData[iOffset+2];
                const GUIntBig nValue4 = pData[iOffset+3];
                nSum += nValue;
                nSumSquare += nValue * nValue;
                nSum += nValue2;
                nSumSquare += nValue2 * nValue2;
                nSum += nValue3;
                nSumSquare += nValue3 * nValue3;
                nSum += nValue4;
                nSumSquare += nValue4 * nValue4;
            }
            for( ; iX < nXCheck; ++iX )
            {
                const GPtrDiff_t iOffset = iX + static_cast<GPtrDiff_t>(iY) * nBlockXSize;
                const GUIntBig nValue = pData[iOffset];
                nSum += nValue;
                nSumSquare += nValue * nValue;
            }
        }
        nSampleCount += static_cast<GUIntBig>(nXCheck) * nYCheck;
        nValidCount += static_cast<GUIntBig>(nXCheck) * nYCheck;
    }
    else
    {
        for( int iY = 0; iY < nYCheck; iY++ )
        {
            int iX;
            for( iX = 0; iX + 1 < nXCheck; iX+=2 )
            {
                const GPtrDiff_t iOffset = iX + static_cast<GPtrDiff_t>(iY) * nBlockXSize;
                const GUInt32 nValue = pData[iOffset];
                const GUInt32 nValue2 = pData[iOffset+1];
                if( nValue < nValue2 )
                {
                    if( nValue < nMin )
                        nMin = nValue;
                    if( nValue2 > nMax )
                        nMax = nValue2;
                }
                else
                {
                    if( nValue2 < nMin )
                        nMin = nValue2;
                    if( nValue > nMax )
                        nMax = nValue;
                }
                if( COMPUTE_OTHER_STATS )
                {
                    nSum += nValue;
                    nSumSquare += static_cast_for_coverity_scan<GUIntBig>(nValue) * nValue;
                    nSum += nValue2;
                    nSumSquare += static_cast_for_coverity_scan<GUIntBig>(nValue2) * nValue2;
                }
            }
            if( iX < nXCheck )
            {
                const GPtrDiff_t iOffset = iX + static_cast<GPtrDiff_t>(iY) * nBlockXSize;
                const GUInt32 nValue = pData[iOffset];
                if( nValue < nMin )
                    nMin = nValue;
                if( nValue > nMax )
                    nMax = nValue;
                if( COMPUTE_OTHER_STATS )
                {
                    nSum += nValue;
                    nSumSquare += static_cast_for_coverity_scan<GUIntBig>(nValue) * nValue;
                }
            }
        }
        if( COMPUTE_OTHER_STATS )
        {
            nSampleCount += static_cast<GUIntBig>(nXCheck) * nYCheck;
            nValidCount += static_cast<GUIntBig>(nXCheck) * nYCheck;
        }
    }
}
};

// Specialization for Byte that is mostly 32 bit friendly as it avoids
// using 64bit accumulators in internal loops. This also slightly helps in
// 64bit mode.
template<bool COMPUTE_OTHER_STATS> struct ComputeStatisticsInternalGeneric<GByte, COMPUTE_OTHER_STATS>
{
static void f( int nXCheck,
                                       int nBlockXSize,
                                       int nYCheck,
                                       const GByte* pData,
                                       bool bHasNoData,
                                       GUInt32 nNoDataValue,
                                       GUInt32& nMin,
                                       GUInt32& nMax,
                                       GUIntBig& nSum,
                                       GUIntBig& nSumSquare,
                                       GUIntBig& nSampleCount,
                                       GUIntBig& nValidCount )
{
    int nOuterLoops = nXCheck / 65536;
    if( nXCheck % 65536 )
        nOuterLoops ++;

    if( bHasNoData )
    {
        // General case
        for( int iY = 0; iY < nYCheck; iY++ )
        {
            int iX = 0;
            for( int k=0; k< nOuterLoops; k++ )
            {
                int iMax = iX + 65536;
                if (iMax > nXCheck )
                    iMax = nXCheck;
                GUInt32 nSum32bit = 0;
                GUInt32 nSumSquare32bit = 0;
                GUInt32 nValidCount32bit = 0;
                GUInt32 nSampleCount32bit = 0;
                for( ; iX < iMax; iX++)
                {
                    const GPtrDiff_t iOffset = iX + static_cast<GPtrDiff_t>(iY) * nBlockXSize;
                    const GUInt32 nValue = pData[iOffset];

                    nSampleCount32bit ++;
                    if( nValue == nNoDataValue )
                        continue;
                    if( nValue < nMin )
                        nMin = nValue;
                    if( nValue > nMax )
                        nMax = nValue;
                    if( COMPUTE_OTHER_STATS )
                    {
                        nValidCount32bit ++;
                        nSum32bit += nValue;
                        nSumSquare32bit += nValue * nValue;
                    }
                }
                if( COMPUTE_OTHER_STATS )
                {
                    nSampleCount += nSampleCount32bit;
                    nValidCount += nValidCount32bit;
                    nSum += nSum32bit;
                    nSumSquare += nSumSquare32bit;
                }
            }
        }
    }
    else if( nMin == 0 &&
             nMax == 255 )
    {
        if( !COMPUTE_OTHER_STATS )
            return;

        // Optimization when there is no nodata and we know we have already
        // reached the min and max
        for( int iY = 0; iY < nYCheck; iY++ )
        {
            int iX = 0;
            for( int k=0; k< nOuterLoops; k++ )
            {
                int iMax = iX + 65536;
                if (iMax > nXCheck )
                    iMax = nXCheck;
                GUInt32 nSum32bit = 0;
                GUInt32 nSumSquare32bit = 0;
                for( ; iX + 3 < iMax; iX+=4 )
                {
                    const GPtrDiff_t iOffset = iX + static_cast<GPtrDiff_t>(iY) * nBlockXSize;
                    const GUInt32 nValue = pData[iOffset];
                    const GUInt32 nValue2 = pData[iOffset+1];
                    const GUInt32 nValue3 = pData[iOffset+2];
                    const GUInt32 nValue4 = pData[iOffset+3];
                    nSum32bit += nValue;
                    nSumSquare32bit += nValue * nValue;
                    nSum32bit += nValue2;
                    nSumSquare32bit += nValue2 * nValue2;
                    nSum32bit += nValue3;
                    nSumSquare32bit += nValue3 * nValue3;
                    nSum32bit += nValue4;
                    nSumSquare32bit += nValue4 * nValue4;
                }
                nSum += nSum32bit;
                nSumSquare += nSumSquare32bit;
            }
            for( ; iX < nXCheck; ++iX )
            {
                const GPtrDiff_t iOffset = iX + static_cast<GPtrDiff_t>(iY) * nBlockXSize;
                const GUIntBig nValue = pData[iOffset];
                nSum += nValue;
                nSumSquare += nValue * nValue;
            }
        }
        nSampleCount += static_cast<GUIntBig>(nXCheck) * nYCheck;
        nValidCount += static_cast<GUIntBig>(nXCheck) * nYCheck;
    }
    else
    {
        for( int iY = 0; iY < nYCheck; iY++ )
        {
            int iX = 0;
            for( int k=0; k< nOuterLoops; k++ )
            {
                int iMax = iX + 65536;
                if (iMax > nXCheck )
                    iMax = nXCheck;
                GUInt32 nSum32bit = 0;
                GUInt32 nSumSquare32bit = 0;
                for( ; iX + 1 < iMax; iX+=2 )
                {
                    const GPtrDiff_t iOffset = iX + static_cast<GPtrDiff_t>(iY) * nBlockXSize;
                    const GUInt32 nValue = pData[iOffset];
                    const GUInt32 nValue2 = pData[iOffset+1];
                    if( nValue < nValue2 )
                    {
                        if( nValue < nMin )
                            nMin = nValue;
                        if( nValue2 > nMax )
                            nMax = nValue2;
                    }
                    else
                    {
                        if( nValue2 < nMin )
                            nMin = nValue2;
                        if( nValue > nMax )
                            nMax = nValue;
                    }
                    if( COMPUTE_OTHER_STATS )
                    {
                        nSum32bit += nValue;
                        nSumSquare32bit += nValue * nValue;
                        nSum32bit += nValue2;
                        nSumSquare32bit += nValue2 * nValue2;
                    }
                }
                if( COMPUTE_OTHER_STATS )
                {
                    nSum += nSum32bit;
                    nSumSquare += nSumSquare32bit;
                }
            }
            if( iX < nXCheck )
            {
                const GPtrDiff_t iOffset = iX + static_cast<GPtrDiff_t>(iY) * nBlockXSize;
                const GUInt32 nValue = pData[iOffset];
                if( nValue < nMin )
                    nMin = nValue;
                if( nValue > nMax )
                    nMax = nValue;
                if( COMPUTE_OTHER_STATS )
                {
                    nSum += nValue;
                    nSumSquare += static_cast_for_coverity_scan<GUIntBig>(nValue) * nValue;
                }
            }
        }
        if( COMPUTE_OTHER_STATS )
        {
            nSampleCount += static_cast<GUIntBig>(nXCheck) * nYCheck;
            nValidCount += static_cast<GUIntBig>(nXCheck) * nYCheck;
        }
    }
}
};

template<class T, bool COMPUTE_OTHER_STATS> struct ComputeStatisticsInternal
{
    static void f( int nXCheck,
                                           int nBlockXSize,
                                           int nYCheck,
                                           const T* pData,
                                           bool bHasNoData,
                                           GUInt32 nNoDataValue,
                                           GUInt32& nMin,
                                           GUInt32& nMax,
                                           GUIntBig& nSum,
                                           GUIntBig& nSumSquare,
                                           GUIntBig& nSampleCount,
                                           GUIntBig& nValidCount )
    {
        ComputeStatisticsInternalGeneric<T, COMPUTE_OTHER_STATS>::f(
                                          nXCheck, nBlockXSize, nYCheck,
                                          pData,
                                          bHasNoData, nNoDataValue,
                                          nMin, nMax, nSum, nSumSquare,
                                          nSampleCount, nValidCount );
    }
};

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(_MSC_VER))

#include "gdal_avx2_emulation.hpp"

#define ZERO256                      GDALmm256_setzero_si256()

template<bool COMPUTE_MIN, bool COMPUTE_MAX, bool COMPUTE_OTHER_STATS>
static void ComputeStatisticsByteNoNodata( GPtrDiff_t nBlockPixels,
                                           // assumed to be aligned on 256 bits
                                           const GByte* pData,
                                           GUInt32& nMin,
                                           GUInt32& nMax,
                                           GUIntBig& nSum,
                                           GUIntBig& nSumSquare,
                                           GUIntBig& nSampleCount,
                                           GUIntBig& nValidCount )
{
    // 32-byte alignment may not be enforced by linker, so do it at hand
    GByte aby32ByteUnaligned[32+32+32+(COMPUTE_OTHER_STATS ? 32+32 : 0)];
    GByte* paby32ByteAligned = aby32ByteUnaligned +
                                (32 - (reinterpret_cast<GUIntptr_t>(aby32ByteUnaligned) % 32));
    GByte* pabyMin = paby32ByteAligned;
    GByte* pabyMax = paby32ByteAligned + 32;
    GUInt32* panSum = COMPUTE_OTHER_STATS ? reinterpret_cast<GUInt32*>(paby32ByteAligned + 32*2): nullptr;
    GUInt32* panSumSquare = COMPUTE_OTHER_STATS ? reinterpret_cast<GUInt32*>(paby32ByteAligned + 32*3): nullptr;

    CPLAssert( (reinterpret_cast<uintptr_t>(pData) % 32) == 0 );

    GPtrDiff_t i = 0;
    // Make sure that sumSquare can fit on uint32
    // * 8 since we can hold 8 sums per vector register
    const int nMaxIterationsPerInnerLoop = 8 *
            ((std::numeric_limits<GUInt32>::max() / (255 * 255)) & ~31);
    GPtrDiff_t nOuterLoops = nBlockPixels / nMaxIterationsPerInnerLoop;
    if( (nBlockPixels % nMaxIterationsPerInnerLoop) != 0 )
        nOuterLoops ++;

    GDALm256i ymm_min = GDALmm256_load_si256(reinterpret_cast<const GDALm256i*>(pData + i));
    GDALm256i ymm_max = ymm_min;
    const auto ymm_mask_8bits = GDALmm256_set1_epi16(0xFF);

    for( GPtrDiff_t k=0; k< nOuterLoops; k++ )
    {
        const auto iMax = std::min(nBlockPixels, i + nMaxIterationsPerInnerLoop);

        // holds 4 uint32 sums in [0], [2], [4] and [6]
        GDALm256i ymm_sum = ZERO256;
        GDALm256i ymm_sumsquare = ZERO256; // holds 8 uint32 sums
        for( ;i+31<iMax; i+=32 )
        {
            const GDALm256i ymm = GDALmm256_load_si256(reinterpret_cast<const GDALm256i*>(pData + i));
            if( COMPUTE_MIN )
            {
                ymm_min = GDALmm256_min_epu8 (ymm_min, ymm);
            }
            if( COMPUTE_MAX )
            {
                ymm_max = GDALmm256_max_epu8 (ymm_max, ymm);
            }

            if( COMPUTE_OTHER_STATS )
            {
                // Extract even-8bit values
                const GDALm256i ymm_even = GDALmm256_and_si256(ymm, ymm_mask_8bits);
                // Compute square of those 16 values as 32 bit result
                // and add adjacent pairs
                const GDALm256i ymm_even_square =
                                            GDALmm256_madd_epi16(ymm_even, ymm_even);
                // Add to the sumsquare accumulator
                ymm_sumsquare = GDALmm256_add_epi32(ymm_sumsquare, ymm_even_square);

                // Extract odd-8bit values
                const GDALm256i ymm_odd = GDALmm256_srli_epi16(ymm, 8);
                const GDALm256i ymm_odd_square =
                                        GDALmm256_madd_epi16(ymm_odd, ymm_odd);
                ymm_sumsquare = GDALmm256_add_epi32(ymm_sumsquare, ymm_odd_square);

                // Now compute the sums
                ymm_sum = GDALmm256_add_epi32(ymm_sum,
                                           GDALmm256_sad_epu8(ymm, ZERO256));
            }
        }

        if( COMPUTE_OTHER_STATS )
        {
            GDALmm256_store_si256(reinterpret_cast<GDALm256i*>(panSum), ymm_sum);
            GDALmm256_store_si256(reinterpret_cast<GDALm256i*>(panSumSquare), ymm_sumsquare);

            nSum += panSum[0] + panSum[2] + panSum[4] + panSum[6];
            nSumSquare += static_cast<GUIntBig>(panSumSquare[0]) +
                          panSumSquare[1] + panSumSquare[2] + panSumSquare[3] +
                          panSumSquare[4] + panSumSquare[5] + panSumSquare[6] +
                          panSumSquare[7];
        }
    }

    if( COMPUTE_MIN )
    {
        GDALmm256_store_si256(reinterpret_cast<GDALm256i*>(pabyMin), ymm_min);
    }
    if( COMPUTE_MAX )
    {
        GDALmm256_store_si256(reinterpret_cast<GDALm256i*>(pabyMax), ymm_max);
    }
    if( COMPUTE_MIN || COMPUTE_MAX )
    {
        for(int j=0;j<32;j++)
        {
            if( COMPUTE_MIN )
            {
                if( pabyMin[j] < nMin ) nMin = pabyMin[j];
            }
            if( COMPUTE_MAX )
            {
                if( pabyMax[j] > nMax ) nMax = pabyMax[j];
            }
        }
    }

    for( ; i<nBlockPixels; i++)
    {
        const GUInt32 nValue = pData[i];
        if( COMPUTE_MIN )
        {
            if( nValue < nMin )
                nMin = nValue;
        }
        if( COMPUTE_MAX )
        {
            if( nValue > nMax )
                nMax = nValue;
        }
        if( COMPUTE_OTHER_STATS )
        {
            nSum += nValue;
            nSumSquare += static_cast_for_coverity_scan<GUIntBig>(nValue) * nValue;
        }
    }

    if( COMPUTE_OTHER_STATS )
    {
        nSampleCount += static_cast<GUIntBig>(nBlockPixels);
        nValidCount += static_cast<GUIntBig>(nBlockPixels);
    }
}


// SSE2/AVX2 optimization for GByte case
// In pure SSE2, this relies on gdal_avx2_emulation.hpp. There is no
// penaly in using the emulation, because, given the mm256 intrinsics used here,
// there are strictly equivalent to 2 parallel SSE2 streams.
template<bool COMPUTE_OTHER_STATS> struct ComputeStatisticsInternal<GByte, COMPUTE_OTHER_STATS>
{
    static void f( int nXCheck,
                                       int nBlockXSize,
                                       int nYCheck,
                                       // assumed to be aligned on 256 bits
                                       const GByte* pData,
                                       bool bHasNoData,
                                       GUInt32 nNoDataValue,
                                       GUInt32& nMin,
                                       GUInt32& nMax,
                                       GUIntBig& nSum,
                                       GUIntBig& nSumSquare,
                                       GUIntBig& nSampleCount,
                                       GUIntBig& nValidCount )
{
    const auto nBlockPixels = static_cast<GPtrDiff_t>(nXCheck) * nYCheck;
    if( bHasNoData && nXCheck == nBlockXSize && nBlockPixels >= 32 &&
        nMin <= nMax )
    {
        // 32-byte alignment may not be enforced by linker, so do it at hand
        GByte aby32ByteUnaligned[32+32+32+32+32];
        GByte* paby32ByteAligned = aby32ByteUnaligned +
                                    (32 - (reinterpret_cast<GUIntptr_t>(aby32ByteUnaligned) % 32));
        GByte* pabyMin = paby32ByteAligned;
        GByte* pabyMax = paby32ByteAligned + 32;
        GUInt32* panSum = reinterpret_cast<GUInt32*>(paby32ByteAligned + 32*2);
        GUInt32* panSumSquare = reinterpret_cast<GUInt32*>(paby32ByteAligned + 32*3);

        CPLAssert( (reinterpret_cast<uintptr_t>(pData) % 32) == 0 );

        GPtrDiff_t i = 0;
        // Make sure that sumSquare can fit on uint32
        // * 8 since we can hold 8 sums per vector register
        const int nMaxIterationsPerInnerLoop = 8 *
                ((std::numeric_limits<GUInt32>::max() / (255 * 255)) & ~31);
        auto nOuterLoops = nBlockPixels / nMaxIterationsPerInnerLoop;
        if( (nBlockPixels % nMaxIterationsPerInnerLoop) != 0 )
            nOuterLoops ++;

        const GDALm256i ymm_nodata = GDALmm256_set1_epi8(
                                        static_cast<GByte>(nNoDataValue) );
        // any non noData value in [min,max] would do.
        const GDALm256i ymm_neutral = GDALmm256_set1_epi8(
                                        static_cast<GByte>(nMin) );
        GDALm256i ymm_min = ymm_neutral;
        GDALm256i ymm_max = ymm_neutral;
        const auto ymm_mask_8bits = GDALmm256_set1_epi16(0xFF);

        const GUInt32 nMinThreshold =
                        (bHasNoData && nNoDataValue == 0) ? 1 : 0;
        const GUInt32 nMaxThreshold =
                        (bHasNoData && nNoDataValue == 255) ? 254 : 255;
        const bool bComputeMinMax = nMin > nMinThreshold || nMax < nMaxThreshold;

        for( GPtrDiff_t k=0; k< nOuterLoops; k++ )
        {
            const auto iMax = std::min(nBlockPixels, i + nMaxIterationsPerInnerLoop);

            // holds 4 uint32 sums in [0], [2], [4] and [6]
            GDALm256i ymm_sum = ZERO256;
            // holds 8 uint32 sums
            GDALm256i ymm_sumsquare = ZERO256;
            // holds 4 uint32 sums in [0], [2], [4] and [6]
            GDALm256i ymm_count_nodata_mul_255 = ZERO256;
            const auto iInit = i;
            for( ;i+31<iMax; i+=32 )
            {
                const GDALm256i ymm = GDALmm256_load_si256(reinterpret_cast<const GDALm256i*>(pData + i));

                // Check which values are nodata
                const GDALm256i ymm_eq_nodata =
                                        GDALmm256_cmpeq_epi8( ymm, ymm_nodata );
                if( COMPUTE_OTHER_STATS )
                {
                    // Count how many values are nodata (due to cmpeq putting 255
                    // when condition is met, this will actually be 255 times
                    // the number of nodata value, spread in 4 64 bits words).
                    // We can use add_epi32 as the counter will not overflow uint32
                    ymm_count_nodata_mul_255 = GDALmm256_add_epi32 (
                                        ymm_count_nodata_mul_255,
                                        GDALmm256_sad_epu8(ymm_eq_nodata, ZERO256) );
                }
                // Replace all nodata values by zero for the purpose of sum
                // and sumquare.
                const GDALm256i ymm_nodata_by_zero =
                                GDALmm256_andnot_si256(ymm_eq_nodata, ymm);
                if( bComputeMinMax )
                {
                    // Replace all nodata values by a neutral value for the
                    // purpose of min and max.
                    const GDALm256i ymm_nodata_by_neutral = GDALmm256_or_si256(
                                GDALmm256_and_si256(ymm_eq_nodata, ymm_neutral),
                                ymm_nodata_by_zero);

                    ymm_min = GDALmm256_min_epu8 (ymm_min,
                                                  ymm_nodata_by_neutral);
                    ymm_max = GDALmm256_max_epu8 (ymm_max,
                                                  ymm_nodata_by_neutral);
                }

                if( COMPUTE_OTHER_STATS )
                {
                    // Extract even-8bit values
                    const GDALm256i ymm_even =
                        GDALmm256_and_si256(ymm_nodata_by_zero, ymm_mask_8bits);
                    // Compute square of those 16 values as 32 bit result
                    // and add adjacent pairs
                    const GDALm256i ymm_even_square =
                                                GDALmm256_madd_epi16(ymm_even, ymm_even);
                    // Add to the sumsquare accumulator
                    ymm_sumsquare = GDALmm256_add_epi32(ymm_sumsquare, ymm_even_square);

                    // Extract odd-8bit values
                    const GDALm256i ymm_odd =
                        GDALmm256_srli_epi16(ymm_nodata_by_zero, 8);
                    const GDALm256i ymm_odd_square =
                                            GDALmm256_madd_epi16(ymm_odd, ymm_odd);
                    ymm_sumsquare = GDALmm256_add_epi32(ymm_sumsquare, ymm_odd_square);

                    // Now compute the sums
                    ymm_sum = GDALmm256_add_epi32(ymm_sum,
                                    GDALmm256_sad_epu8(ymm_nodata_by_zero, ZERO256));
                }
            }

            if( COMPUTE_OTHER_STATS )
            {
                GUInt32* panCoutNoDataMul255 = panSum;
                GDALmm256_store_si256(reinterpret_cast<GDALm256i*>(panCoutNoDataMul255),
                                   ymm_count_nodata_mul_255);

                nSampleCount += (i - iInit);

                nValidCount += (i - iInit) -
                            (panCoutNoDataMul255[0] + panCoutNoDataMul255[2] +
                             panCoutNoDataMul255[4] + panCoutNoDataMul255[6]) / 255;

                GDALmm256_store_si256(reinterpret_cast<GDALm256i*>(panSum), ymm_sum);
                GDALmm256_store_si256(reinterpret_cast<GDALm256i*>(panSumSquare), ymm_sumsquare);
                nSum += panSum[0] + panSum[2] + panSum[4] + panSum[6];
                nSumSquare += static_cast<GUIntBig>(panSumSquare[0]) +
                              panSumSquare[1] + panSumSquare[2] + panSumSquare[3] +
                              panSumSquare[4] + panSumSquare[5] + panSumSquare[6] +
                              panSumSquare[7];
            }
        }

        if( bComputeMinMax )
        {
            GDALmm256_store_si256(reinterpret_cast<GDALm256i*>(pabyMin), ymm_min);
            GDALmm256_store_si256(reinterpret_cast<GDALm256i*>(pabyMax), ymm_max);
            for(int j=0;j<32;j++)
            {
                if( pabyMin[j] < nMin ) nMin = pabyMin[j];
                if( pabyMax[j] > nMax ) nMax = pabyMax[j];
            }
        }

        if( COMPUTE_OTHER_STATS )
        {
            nSampleCount += nBlockPixels - i;
        }
        for( ; i<nBlockPixels; i++)
        {
            const GUInt32 nValue = pData[i];
            if( nValue == nNoDataValue )
                continue;
            if( nValue < nMin )
                nMin = nValue;
            if( nValue > nMax )
                nMax = nValue;
            if( COMPUTE_OTHER_STATS )
            {
                nValidCount ++;
                nSum += nValue;
                nSumSquare += static_cast_for_coverity_scan<GUIntBig>(nValue) * nValue;
            }
        }
    }
    else if( !bHasNoData && nXCheck == nBlockXSize && nBlockPixels >= 32 )
    {
        if( nMin > 0 )
        {
            if( nMax < 255 )
            {
                ComputeStatisticsByteNoNodata<true, true, COMPUTE_OTHER_STATS>(
                    nBlockPixels, pData,
                    nMin, nMax,
                    nSum, nSumSquare, nSampleCount, nValidCount );
            }
            else
            {
                ComputeStatisticsByteNoNodata<true, false, COMPUTE_OTHER_STATS>(
                    nBlockPixels, pData,
                    nMin, nMax,
                    nSum, nSumSquare, nSampleCount, nValidCount );
            }
        }
        else
        {
            if( nMax < 255 )
            {
                ComputeStatisticsByteNoNodata<false, true, COMPUTE_OTHER_STATS>(
                    nBlockPixels, pData,
                    nMin, nMax,
                    nSum, nSumSquare, nSampleCount, nValidCount );
            }
            else
            {
                ComputeStatisticsByteNoNodata<false, false, COMPUTE_OTHER_STATS>(
                    nBlockPixels, pData,
                    nMin, nMax,
                    nSum, nSumSquare, nSampleCount, nValidCount );
            }
        }
    }
    else if ( !COMPUTE_OTHER_STATS && !bHasNoData && nXCheck >= 32 &&
              (nBlockXSize % 32) == 0 )
    {
        for( int iY = 0; iY < nYCheck; iY++ )
        {
            ComputeStatisticsByteNoNodata<true, true, COMPUTE_OTHER_STATS>(
                    nXCheck, pData + static_cast<size_t>(iY) * nBlockXSize,
                    nMin, nMax,
                    nSum, nSumSquare, nSampleCount, nValidCount );
        }
    }
    else
    {
        ComputeStatisticsInternalGeneric<GByte, COMPUTE_OTHER_STATS>::f(
                                          nXCheck, nBlockXSize, nYCheck,
                                          pData,
                                          bHasNoData, nNoDataValue,
                                          nMin, nMax, nSum, nSumSquare,
                                          nSampleCount, nValidCount );
    }
}
};

CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW
static void UnshiftSumSquare( GUIntBig& nSumSquare,
                              GUIntBig  nSumThis,
                              GUIntBig  i )
{
    nSumSquare += 32768 * (2 * nSumThis - i * 32768);
}

// AVX2/SSE2 optimization for GUInt16 case
template<bool COMPUTE_OTHER_STATS> struct ComputeStatisticsInternal<GUInt16, COMPUTE_OTHER_STATS>
{
    static void f( int nXCheck,
                                       int nBlockXSize,
                                       int nYCheck,
                                       // assumed to be aligned on 128 bits
                                       const GUInt16* pData,
                                       bool bHasNoData,
                                       GUInt32 nNoDataValue,
                                       GUInt32& nMin,
                                       GUInt32& nMax,
                                       GUIntBig& nSum,
                                       GUIntBig& nSumSquare,
                                       GUIntBig& nSampleCount,
                                       GUIntBig& nValidCount )
{
    const auto nBlockPixels = static_cast<GPtrDiff_t>(nXCheck) * nYCheck;
    if( !bHasNoData && nXCheck == nBlockXSize && nBlockPixels >= 16 )
    {
        CPLAssert( (reinterpret_cast<uintptr_t>(pData) % 16) == 0 );

        GPtrDiff_t i = 0;
        // In SSE2, min_epu16 and max_epu16 do not exist, so shift from
        // UInt16 to SInt16 to be able to use min_epi16 and max_epi16.
        // Furthermore the shift is also needed to use madd_epi16
        const GDALm256i ymm_m32768 = GDALmm256_set1_epi16(-32768);
        GDALm256i ymm_min = GDALmm256_load_si256(reinterpret_cast<const GDALm256i*>(pData + i));
        ymm_min = GDALmm256_add_epi16(ymm_min, ymm_m32768);
        GDALm256i ymm_max = ymm_min;
        GDALm256i ymm_sumsquare = ZERO256; // holds 4 uint64 sums

        // Make sure that sum can fit on uint32
        // * 8 since we can hold 8 sums per vector register
        const int nMaxIterationsPerInnerLoop = 8 *
                ((std::numeric_limits<GUInt32>::max() / 65535) & ~15);
        GPtrDiff_t nOuterLoops = nBlockPixels / nMaxIterationsPerInnerLoop;
        if( (nBlockPixels % nMaxIterationsPerInnerLoop) != 0 )
            nOuterLoops ++;

        const bool bComputeMinMax = nMin > 0 || nMax < 65535;
        const auto ymm_mask_16bits = GDALmm256_set1_epi32(0xFFFF);
        const auto ymm_mask_32bits = GDALmm256_set1_epi64x(0xFFFFFFFF);

        GUIntBig nSumThis = 0;
        for( int k=0; k< nOuterLoops; k++ )
        {
            const auto iMax = std::min(nBlockPixels, i + nMaxIterationsPerInnerLoop);

            GDALm256i ymm_sum = ZERO256; // holds 8 uint32 sums
            for( ;i+15<iMax ; i+=16 )
            {
                const GDALm256i ymm = GDALmm256_load_si256(reinterpret_cast<const GDALm256i*>(pData + i));
                const GDALm256i ymm_shifted = GDALmm256_add_epi16(ymm, ymm_m32768);
                if( bComputeMinMax )
                {
                    ymm_min = GDALmm256_min_epi16 (ymm_min, ymm_shifted);
                    ymm_max = GDALmm256_max_epi16 (ymm_max, ymm_shifted);
                }

                if( COMPUTE_OTHER_STATS )
                {
                    // Note: the int32 range can overflow for (0-32768)^2 +
                    // (0-32768)^2 = 0x80000000, but as we know the result is
                    // positive, this is OK as we interpret is a uint32.
                    const GDALm256i ymm_square = GDALmm256_madd_epi16(ymm_shifted,
                                                                      ymm_shifted);
                    ymm_sumsquare = GDALmm256_add_epi64(ymm_sumsquare,
                                        GDALmm256_and_si256(ymm_square, ymm_mask_32bits));
                    ymm_sumsquare = GDALmm256_add_epi64(ymm_sumsquare,
                                        GDALmm256_srli_epi64(ymm_square, 32));

                    // Now compute the sums
                    ymm_sum = GDALmm256_add_epi32(ymm_sum,
                                      GDALmm256_and_si256(ymm, ymm_mask_16bits));
                    ymm_sum = GDALmm256_add_epi32(ymm_sum,
                                      GDALmm256_srli_epi32(ymm, 16));
                }
            }

            if( COMPUTE_OTHER_STATS )
            {
                GUInt32 anSum[8];
                GDALmm256_storeu_si256(reinterpret_cast<GDALm256i*>(anSum), ymm_sum);
                nSumThis += static_cast<GUIntBig>(anSum[0]) + anSum[1] +
                        anSum[2] + anSum[3] + anSum[4] + anSum[5] +
                        anSum[6] + anSum[7];
            }
        }

        if( bComputeMinMax )
        {
            GUInt16 anMin[16];
            GUInt16 anMax[16];

            // Unshift the result
            ymm_min = GDALmm256_sub_epi16(ymm_min, ymm_m32768);
            ymm_max = GDALmm256_sub_epi16(ymm_max, ymm_m32768);
            GDALmm256_storeu_si256(reinterpret_cast<GDALm256i*>(anMin), ymm_min);
            GDALmm256_storeu_si256(reinterpret_cast<GDALm256i*>(anMax), ymm_max);
            for(int j=0;j<16;j++)
            {
                if( anMin[j] < nMin ) nMin = anMin[j];
                if( anMax[j] > nMax ) nMax = anMax[j];
            }
        }

        if ( COMPUTE_OTHER_STATS )
        {
            GUIntBig anSumSquare[4];
            GDALmm256_storeu_si256(reinterpret_cast<GDALm256i*>(anSumSquare), ymm_sumsquare);
            nSumSquare += anSumSquare[0] +
                            anSumSquare[1] + anSumSquare[2] + anSumSquare[3];

            // Unshift the sum of squares
            UnshiftSumSquare(nSumSquare, nSumThis, static_cast<GUIntBig>(i));

            nSum += nSumThis;

            for( ; i<nBlockPixels; i++)
            {
                const GUInt32 nValue = pData[i];
                if( nValue < nMin )
                    nMin = nValue;
                if( nValue > nMax )
                    nMax = nValue;
                nSum += nValue;
                nSumSquare += static_cast_for_coverity_scan<GUIntBig>(nValue) * nValue;
            }

            nSampleCount += static_cast<GUIntBig>(nXCheck) * nYCheck;
            nValidCount += static_cast<GUIntBig>(nXCheck) * nYCheck;
        }
    }
    else
    {
        ComputeStatisticsInternalGeneric<GUInt16, COMPUTE_OTHER_STATS>::f(
                                          nXCheck, nBlockXSize, nYCheck,
                                          pData,
                                          bHasNoData, nNoDataValue,
                                          nMin, nMax, nSum, nSumSquare,
                                          nSampleCount, nValidCount );
    }
}
};

#endif // (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(_MSC_VER))

#endif // CPL_HAS_GINT64


/************************************************************************/
/*                          GetPixelValue()                             */
/************************************************************************/

static
inline double GetPixelValue( GDALDataType eDataType,
                             bool bSignedByte,
                             const void* pData,
                             GPtrDiff_t iOffset,
                             bool bGotNoDataValue,
                             double dfNoDataValue,
                             bool bGotFloatNoDataValue,
                             float fNoDataValue,
                             bool& bValid )
{
    bValid = true;
    double dfValue;
    switch( eDataType )
    {
        case GDT_Byte:
        {
            if( bSignedByte )
                dfValue = static_cast<const signed char *>(pData)[iOffset];
            else
                dfValue = static_cast<const GByte *>(pData)[iOffset];
            break;
        }
        case GDT_UInt16:
            dfValue = static_cast<const GUInt16 *>(pData)[iOffset];
            break;
        case GDT_Int16:
            dfValue = static_cast<const GInt16 *>(pData)[iOffset];
            break;
        case GDT_UInt32:
            dfValue = static_cast<const GUInt32 *>(pData)[iOffset];
            break;
        case GDT_Int32:
            dfValue = static_cast<const GInt32 *>(pData)[iOffset];
            break;
        case GDT_UInt64:
            dfValue = static_cast<double>(static_cast<const std::uint64_t *>(pData)[iOffset]);
            break;
        case GDT_Int64:
            dfValue = static_cast<double>(static_cast<const std::int64_t *>(pData)[iOffset]);
            break;
        case GDT_Float32:
        {
            const float fValue = static_cast<const float *>(pData)[iOffset];
            if( CPLIsNan(fValue) ||
                (bGotFloatNoDataValue && ARE_REAL_EQUAL(fValue, fNoDataValue)) )
            {
                bValid = false;
                return 0.0;
            }
            dfValue = fValue;
            return dfValue;
        }
        case GDT_Float64:
            dfValue = static_cast<const double *>(pData)[iOffset];
            if( CPLIsNan(dfValue) )
            {
                bValid = false;
                return 0.0;
            }
            break;
        case GDT_CInt16:
            dfValue = static_cast<const GInt16 *>(pData)[iOffset*2];
            break;
        case GDT_CInt32:
            dfValue = static_cast<const GInt32 *>(pData)[iOffset*2];
            break;
        case GDT_CFloat32:
            dfValue = static_cast<const float *>(pData)[iOffset*2];
            if( CPLIsNan(dfValue) )
            {
                bValid = false;
                return 0.0;
            }
            break;
        case GDT_CFloat64:
            dfValue = static_cast<const double *>(pData)[iOffset*2];
            if( CPLIsNan(dfValue) )
            {
                bValid = false;
                return 0.0;
            }
            break;
        default:
#ifndef CSA_BUILD
            dfValue = 0.0;
#endif
            CPLAssert( false );
    }

    if( bGotNoDataValue && ARE_REAL_EQUAL(dfValue, dfNoDataValue) )
    {
        bValid = false;
        return 0.0;
    }
    return dfValue;
}

/************************************************************************/
/*                         SetValidPercent()                            */
/************************************************************************/

/**
 * \brief Set percentage of valid (not nodata) pixels.
 *
 * Stores the percentage of valid pixels in the metadata item
 * STATISTICS_VALID_PERCENT
 *
 * @param nSampleCount Number of sampled pixels.
 *
 * @param nValidCount Number of valid pixels.
 */

void GDALRasterBand::SetValidPercent(GUIntBig nSampleCount, GUIntBig nValidCount)
{
    if( nValidCount == 0 )
    {
        SetMetadataItem( "STATISTICS_VALID_PERCENT", "0" );
    }
    else if( nValidCount == nSampleCount )
    {
        SetMetadataItem( "STATISTICS_VALID_PERCENT", "100" );
    }
    else /* nValidCount < nSampleCount */
    {
        char szValue[128] = { 0 };

        /* percentage is only an indicator: limit precision */
        CPLsnprintf( szValue, sizeof(szValue), "%.4g",
                 100. * static_cast<double>(nValidCount) / nSampleCount );

        if (EQUAL(szValue, "100"))
        {
            /* don't set 100 percent valid
             * because some of the sampled pixels were nodata */
            SetMetadataItem( "STATISTICS_VALID_PERCENT", "99.999" );
        }
        else
        {
            SetMetadataItem( "STATISTICS_VALID_PERCENT", szValue );
        }
    }
}

/************************************************************************/
/*                         ComputeStatistics()                          */
/************************************************************************/

/**
 * \brief Compute image statistics.
 *
 * Returns the minimum, maximum, mean and standard deviation of all
 * pixel values in this band.  If approximate statistics are sufficient,
 * the bApproxOK flag can be set to true in which case overviews, or a
 * subset of image tiles may be used in computing the statistics.
 *
 * Once computed, the statistics will generally be "set" back on the
 * raster band using SetStatistics().
 *
 * Cached statistics can be cleared with GDALDataset::ClearStatistics().
 *
 * This method is the same as the C function GDALComputeRasterStatistics().
 *
 * @param bApproxOK If TRUE statistics may be computed based on overviews
 * or a subset of all tiles.
 *
 * @param pdfMin Location into which to load image minimum (may be NULL).
 *
 * @param pdfMax Location into which to load image maximum (may be NULL).-
 *
 * @param pdfMean Location into which to load image mean (may be NULL).
 *
 * @param pdfStdDev Location into which to load image standard deviation
 * (may be NULL).
 *
 * @param pfnProgress a function to call to report progress, or NULL.
 *
 * @param pProgressData application data to pass to the progress function.
 *
 * @return CE_None on success, or CE_Failure if an error occurs or processing
 * is terminated by the user.
 */

CPLErr
GDALRasterBand::ComputeStatistics( int bApproxOK,
                                   double *pdfMin, double *pdfMax,
                                   double *pdfMean, double *pdfStdDev,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData )

{
    if( pfnProgress == nullptr )
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      If we have overview bands, use them for statistics.             */
/* -------------------------------------------------------------------- */
    if( bApproxOK && GetOverviewCount() > 0 && !HasArbitraryOverviews() )
    {
        GDALRasterBand *poBand
            = GetRasterSampleOverview( GDALSTAT_APPROX_NUMSAMPLES );

        if( poBand != this )
        {
            CPLErr eErr = poBand->ComputeStatistics( FALSE,
                                              pdfMin, pdfMax,
                                              pdfMean, pdfStdDev,
                                              pfnProgress, pProgressData );
            if( eErr == CE_None )
            {
                if( pdfMin && pdfMax && pdfMean && pdfStdDev )
                {
                    SetMetadataItem( "STATISTICS_APPROXIMATE", "YES" );
                    SetStatistics( *pdfMin,*pdfMax, *pdfMean, *pdfStdDev );
                }

                /* transfer metadata from overview band to this */
                const char *pszPercentValid = poBand->GetMetadataItem("STATISTICS_VALID_PERCENT");

                if ( pszPercentValid != nullptr )
                {
                    SetMetadataItem( "STATISTICS_VALID_PERCENT", pszPercentValid );
                }
            }
            return eErr;
        }
    }

    if( !pfnProgress( 0.0, "Compute Statistics", pProgressData ) )
    {
        ReportError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Read actual data and compute statistics.                        */
/* -------------------------------------------------------------------- */
    // Using Welford algorithm:
    // http://en.wikipedia.org/wiki/Algorithms_for_calculating_variance
    // to compute standard deviation in a more numerically robust way than
    // the difference of the sum of square values with the square of the sum.
    // dfMean and dfM2 are updated at each sample.
    // dfM2 is the sum of square of differences to the current mean.
    double dfMin = std::numeric_limits<double>::max();
    double dfMax = -std::numeric_limits<double>::max();
    double dfMean = 0.0;
    double dfM2 = 0.0;

    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);

    int bGotNoDataValue = FALSE;
    const double dfNoDataValue = GetNoDataValue( &bGotNoDataValue );
    bGotNoDataValue = bGotNoDataValue && !CPLIsNan(dfNoDataValue);
    bool bGotFloatNoDataValue = false;
    float fNoDataValue = 0.0f;
    ComputeFloatNoDataValue( eDataType, dfNoDataValue, bGotNoDataValue,
                            fNoDataValue, bGotFloatNoDataValue );

    const char* pszPixelType =
        GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
    const bool bSignedByte =
        pszPixelType != nullptr && EQUAL(pszPixelType, "SIGNEDBYTE");

    GUIntBig nSampleCount = 0;
    GUIntBig nValidCount = 0;

    if ( bApproxOK && HasArbitraryOverviews() )
    {
/* -------------------------------------------------------------------- */
/*      Figure out how much the image should be reduced to get an       */
/*      approximate value.                                              */
/* -------------------------------------------------------------------- */
        double  dfReduction = sqrt(
            static_cast<double>(nRasterXSize) * nRasterYSize /
            GDALSTAT_APPROX_NUMSAMPLES );

        int nXReduced = nRasterXSize;
        int nYReduced = nRasterYSize;
        if ( dfReduction > 1.0 )
        {
            nXReduced = static_cast<int>( nRasterXSize / dfReduction );
            nYReduced = static_cast<int>( nRasterYSize / dfReduction );

            // Catch the case of huge resizing ratios here
            if ( nXReduced == 0 )
                nXReduced = 1;
            if ( nYReduced == 0 )
                nYReduced = 1;
        }

        void *pData =
            CPLMalloc(
                GDALGetDataTypeSizeBytes(eDataType) * nXReduced * nYReduced );

        const CPLErr eErr = IRasterIO(
            GF_Read, 0, 0, nRasterXSize, nRasterYSize, pData,
            nXReduced, nYReduced, eDataType, 0, 0, &sExtraArg );
        if ( eErr != CE_None )
        {
            CPLFree(pData);
            return eErr;
        }

        /* this isn't the fastest way to do this, but is easier for now */
        for( int iY = 0; iY < nYReduced; iY++ )
        {
            for( int iX = 0; iX < nXReduced; iX++ )
            {
                const int iOffset = iX + iY * nXReduced;
                bool bValid = true;
                double dfValue = GetPixelValue( eDataType,
                                                bSignedByte,
                                                pData,
                                                iOffset,
                                                CPL_TO_BOOL(bGotNoDataValue),
                                                dfNoDataValue,
                                                bGotFloatNoDataValue,
                                                fNoDataValue,
                                                bValid );
                if( !bValid )
                    continue;

                dfMin = std::min(dfMin, dfValue);
                dfMax = std::max(dfMax, dfValue);

                nValidCount++;
                const double dfDelta = dfValue - dfMean;
                dfMean += dfDelta / nValidCount;
                dfM2 += dfDelta * (dfValue - dfMean);
            }
        }

        nSampleCount = static_cast<GUIntBig>(nXReduced) * nYReduced;

        CPLFree( pData );
    }

    else  // No arbitrary overviews.
    {
        if( !InitBlockInfo() )
            return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Figure out the ratio of blocks we will read to get an           */
/*      approximate value.                                              */
/* -------------------------------------------------------------------- */
        int nSampleRate = 1;
        if ( bApproxOK )
        {
            nSampleRate = static_cast<int>(
                std::max(1.0,
                         sqrt(static_cast<double>(nBlocksPerRow) *
                              nBlocksPerColumn)));
            // We want to avoid probing only the first column of blocks for
            // a square shaped raster, because it is not unlikely that it may
            // be padding only (#6378)
            if( nSampleRate == nBlocksPerRow && nBlocksPerRow > 1 )
              nSampleRate += 1;
        }
        if( nSampleRate == 1 )
            bApproxOK = false;

#ifdef CPL_HAS_GINT64
        // Particular case for GDT_Byte that only use integral types for all
        // intermediate computations. Only possible if the number of pixels
        // explored is lower than GUINTBIG_MAX / (255*255), so that nSumSquare
        // can fit on a uint64. Should be 99.99999% of cases.
        // For GUInt16, this limits to raster of 4 giga pixels
        if( (eDataType == GDT_Byte && !bSignedByte &&
             static_cast<GUIntBig>(nBlocksPerRow)*nBlocksPerColumn/nSampleRate <
                GUINTBIG_MAX / (255U * 255U) /
                        (static_cast<GUInt64>(nBlockXSize) * static_cast<GUInt64>(nBlockYSize))) ||
            (eDataType == GDT_UInt16 &&
             static_cast<GUIntBig>(nBlocksPerRow)*nBlocksPerColumn/nSampleRate <
                GUINTBIG_MAX / (65535U * 65535U) /
                        (static_cast<GUInt64>(nBlockXSize) * static_cast<GUInt64>(nBlockYSize))) )
        {
            const GUInt32 nMaxValueType = (eDataType == GDT_Byte) ? 255 : 65535;
            GUInt32 nMin = nMaxValueType;
            GUInt32 nMax = 0;
            GUIntBig nSum = 0;
            GUIntBig nSumSquare = 0;
            // If no valid nodata, map to invalid value (256 for Byte)
            const GUInt32 nNoDataValue =
                (bGotNoDataValue && dfNoDataValue >= 0 &&
                 dfNoDataValue <= nMaxValueType &&
                 fabs(dfNoDataValue -
                      static_cast<GUInt32>(dfNoDataValue + 1e-10)) < 1e-10 ) ?
                            static_cast<GUInt32>(dfNoDataValue + 1e-10) :
                            nMaxValueType+1;

            for( int iSampleBlock = 0;
                iSampleBlock < nBlocksPerRow * nBlocksPerColumn;
                iSampleBlock += nSampleRate )
            {
                const int iYBlock = iSampleBlock / nBlocksPerRow;
                const int iXBlock = iSampleBlock - nBlocksPerRow * iYBlock;

                GDALRasterBlock * const poBlock =
                    GetLockedBlockRef( iXBlock, iYBlock );
                if( poBlock == nullptr )
                    return CE_Failure;

                void* const pData = poBlock->GetDataRef();

                int nXCheck = 0, nYCheck = 0;
                GetActualBlockSize(iXBlock, iYBlock, &nXCheck, &nYCheck);

                if( eDataType == GDT_Byte )
                {
                    ComputeStatisticsInternal<GByte, /* COMPUTE_OTHER_STATS = */ true>::f(
                                               nXCheck,
                                               nBlockXSize,
                                               nYCheck,
                                               static_cast<const GByte*>(pData),
                                               nNoDataValue <= nMaxValueType,
                                               nNoDataValue,
                                               nMin, nMax, nSum,
                                               nSumSquare,
                                               nSampleCount,
                                               nValidCount );
                }
                else
                {
                    ComputeStatisticsInternal<GUInt16, /* COMPUTE_OTHER_STATS = */ true>::f(
                                               nXCheck,
                                               nBlockXSize,
                                               nYCheck,
                                               static_cast<const GUInt16*>(pData),
                                               nNoDataValue <= nMaxValueType,
                                               nNoDataValue,
                                               nMin, nMax, nSum,
                                               nSumSquare,
                                               nSampleCount,
                                               nValidCount );
                }

                poBlock->DropLock();

                if ( !pfnProgress( iSampleBlock
                        / static_cast<double>(nBlocksPerRow*nBlocksPerColumn),
                        "Compute Statistics", pProgressData) )
                {
                    ReportError( CE_Failure, CPLE_UserInterrupt,
                                 "User terminated" );
                    return CE_Failure;
                }
            }

            if( !pfnProgress( 1.0, "Compute Statistics", pProgressData ) )
            {
                ReportError( CE_Failure, CPLE_UserInterrupt,
                             "User terminated" );
                return CE_Failure;
            }

/* -------------------------------------------------------------------- */
/*      Save computed information.                                      */
/* -------------------------------------------------------------------- */
            if( nValidCount )
                dfMean = static_cast<double>(nSum) / nValidCount;

            // To avoid potential precision issues when doing the difference,
            // we need to do that computation on 128 bit rather than casting
            // to double
            const GDALUInt128 nTmpForStdDev(
                    GDALUInt128::Mul(nSumSquare,nValidCount) -
                    GDALUInt128::Mul(nSum,nSum));
            const double dfStdDev =
                nValidCount > 0 ?
                    sqrt(static_cast<double>(nTmpForStdDev)) / nValidCount :
                    0.0;

            if( nValidCount > 0 )
            {
                if( bApproxOK )
                {
                    SetMetadataItem( "STATISTICS_APPROXIMATE", "YES" );
                }
                else if( GetMetadataItem( "STATISTICS_APPROXIMATE" ) )
                {
                    SetMetadataItem( "STATISTICS_APPROXIMATE",  nullptr );
                }
                SetStatistics( nMin, nMax, dfMean, dfStdDev );
            }

            SetValidPercent( nSampleCount, nValidCount );

/* -------------------------------------------------------------------- */
/*      Record results.                                                 */
/* -------------------------------------------------------------------- */
            if( pdfMin != nullptr )
                *pdfMin = nValidCount ? nMin : 0;
            if( pdfMax != nullptr )
                *pdfMax = nValidCount ? nMax : 0;

            if( pdfMean != nullptr )
                *pdfMean = dfMean;

            if( pdfStdDev != nullptr )
                *pdfStdDev = dfStdDev;

            if( nValidCount > 0 )
                return CE_None;

            ReportError(
                CE_Failure, CPLE_AppDefined,
                "Failed to compute statistics, no valid pixels found in sampling." );
            return CE_Failure;
        }
#endif

        for( int iSampleBlock = 0;
             iSampleBlock < nBlocksPerRow * nBlocksPerColumn;
             iSampleBlock += nSampleRate )
        {
            const int iYBlock = iSampleBlock / nBlocksPerRow;
            const int iXBlock = iSampleBlock - nBlocksPerRow * iYBlock;

            GDALRasterBlock * const poBlock = GetLockedBlockRef( iXBlock, iYBlock );
            if( poBlock == nullptr )
                return CE_Failure;

            void* const pData = poBlock->GetDataRef();

            int nXCheck = 0, nYCheck = 0;
            GetActualBlockSize(iXBlock, iYBlock, &nXCheck, &nYCheck);

            // This isn't the fastest way to do this, but is easier for now.
            for( int iY = 0; iY < nYCheck; iY++ )
            {
                for( int iX = 0; iX < nXCheck; iX++ )
                {
                    const GPtrDiff_t iOffset = iX + static_cast<GPtrDiff_t>(iY) * nBlockXSize;
                    bool bValid = true;
                    double dfValue = GetPixelValue( eDataType,
                                                    bSignedByte,
                                                    pData,
                                                    iOffset,
                                                    CPL_TO_BOOL(bGotNoDataValue),
                                                    dfNoDataValue,
                                                    bGotFloatNoDataValue,
                                                    fNoDataValue,
                                                    bValid );

                    if( !bValid )
                        continue;

                    dfMin = std::min(dfMin, dfValue);
                    dfMax = std::max(dfMax, dfValue);

                    nValidCount++;
                    const double dfDelta = dfValue - dfMean;
                    dfMean += dfDelta / nValidCount;
                    dfM2 += dfDelta * (dfValue - dfMean);
                }
            }

            nSampleCount += static_cast<GUIntBig>(nXCheck) * nYCheck;

            poBlock->DropLock();

            if ( !pfnProgress(
                     iSampleBlock
                         / static_cast<double>(nBlocksPerRow*nBlocksPerColumn),
                     "Compute Statistics", pProgressData) )
            {
                ReportError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
                return CE_Failure;
            }
        }
    }

    if( !pfnProgress( 1.0, "Compute Statistics", pProgressData ) )
    {
        ReportError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Save computed information.                                      */
/* -------------------------------------------------------------------- */
    const double dfStdDev =
        nValidCount > 0 ? sqrt(dfM2 / nValidCount) : 0.0;

    if( nValidCount > 0 )
    {
        if( bApproxOK )
        {
            SetMetadataItem( "STATISTICS_APPROXIMATE", "YES" );
        }
        else if( GetMetadataItem( "STATISTICS_APPROXIMATE" ) )
        {
            SetMetadataItem( "STATISTICS_APPROXIMATE",  nullptr );
        }
        SetStatistics( dfMin, dfMax, dfMean, dfStdDev );
    }
    else
    {
        dfMin = 0.0;
        dfMax = 0.0;
    }

    SetValidPercent( nSampleCount, nValidCount );

/* -------------------------------------------------------------------- */
/*      Record results.                                                 */
/* -------------------------------------------------------------------- */
    if( pdfMin != nullptr )
        *pdfMin = dfMin;
    if( pdfMax != nullptr )
        *pdfMax = dfMax;

    if( pdfMean != nullptr )
        *pdfMean = dfMean;

    if( pdfStdDev != nullptr )
        *pdfStdDev = dfStdDev;

    if( nValidCount > 0 )
        return CE_None;

    ReportError(
        CE_Failure, CPLE_AppDefined,
        "Failed to compute statistics, no valid pixels found in sampling." );
    return CE_Failure;
}

/************************************************************************/
/*                    GDALComputeRasterStatistics()                     */
/************************************************************************/

/**
  * \brief Compute image statistics.
  *
  * @see GDALRasterBand::ComputeStatistics()
  */

CPLErr CPL_STDCALL GDALComputeRasterStatistics(
        GDALRasterBandH hBand, int bApproxOK,
        double *pdfMin, double *pdfMax, double *pdfMean, double *pdfStdDev,
        GDALProgressFunc pfnProgress, void *pProgressData )

{
    VALIDATE_POINTER1( hBand, "GDALComputeRasterStatistics", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);

    return poBand->ComputeStatistics(
        bApproxOK, pdfMin, pdfMax, pdfMean, pdfStdDev,
        pfnProgress, pProgressData );
}

/************************************************************************/
/*                           SetStatistics()                            */
/************************************************************************/

/**
 * \brief Set statistics on band.
 *
 * This method can be used to store min/max/mean/standard deviation
 * statistics on a raster band.
 *
 * The default implementation stores them as metadata, and will only work
 * on formats that can save arbitrary metadata.  This method cannot detect
 * whether metadata will be properly saved and so may return CE_None even
 * if the statistics will never be saved.
 *
 * This method is the same as the C function GDALSetRasterStatistics().
 *
 * @param dfMin minimum pixel value.
 *
 * @param dfMax maximum pixel value.
 *
 * @param dfMean mean (average) of all pixel values.
 *
 * @param dfStdDev Standard deviation of all pixel values.
 *
 * @return CE_None on success or CE_Failure on failure.
 */

CPLErr GDALRasterBand::SetStatistics( double dfMin, double dfMax,
                                      double dfMean, double dfStdDev )

{
    char szValue[128] = { 0 };

    CPLsnprintf( szValue, sizeof(szValue), "%.14g", dfMin );
    SetMetadataItem( "STATISTICS_MINIMUM", szValue );

    CPLsnprintf( szValue, sizeof(szValue), "%.14g", dfMax );
    SetMetadataItem( "STATISTICS_MAXIMUM", szValue );

    CPLsnprintf( szValue, sizeof(szValue), "%.14g", dfMean );
    SetMetadataItem( "STATISTICS_MEAN", szValue );

    CPLsnprintf( szValue, sizeof(szValue), "%.14g", dfStdDev );
    SetMetadataItem( "STATISTICS_STDDEV", szValue );

    return CE_None;
}

/************************************************************************/
/*                      GDALSetRasterStatistics()                       */
/************************************************************************/

/**
 * \brief Set statistics on band.
 *
 * @see GDALRasterBand::SetStatistics()
 */

CPLErr CPL_STDCALL GDALSetRasterStatistics(
        GDALRasterBandH hBand,
        double dfMin, double dfMax, double dfMean, double dfStdDev )

{
    VALIDATE_POINTER1( hBand, "GDALSetRasterStatistics", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->SetStatistics( dfMin, dfMax, dfMean, dfStdDev );
}

/************************************************************************/
/*                        ComputeRasterMinMax()                         */
/************************************************************************/

template<class T, bool HAS_NODATA> static void ComputeMinMax(const T* buffer,
                                                             size_t nElts,
                                                             T nodataValue,
                                                             T* pMin,
                                                             T* pMax)
{
    T min0 = *pMin;
    T max0 = *pMax;
    T min1 = *pMin;
    T max1 = *pMax;
    size_t i;
    for(i = 0; i + 1 < nElts; i+=2 )
    {
        if( !HAS_NODATA || buffer[i] != nodataValue )
        {
            min0 = std::min(min0, buffer[i]);
            max0 = std::max(max0, buffer[i]);
        }
        if( !HAS_NODATA || buffer[i+1] != nodataValue )
        {
            min1 = std::min(min1, buffer[i+1]);
            max1 = std::max(max1, buffer[i+1]);
        }
    }
    T min = std::min(min0, min1);
    T max = std::max(max0, max1);
    if( i < nElts )
    {
        if( !HAS_NODATA || buffer[i] != nodataValue )
        {
            min = std::min(min, buffer[i]);
            max = std::max(max, buffer[i]);
        }
    }
    *pMin = min;
    *pMax = max;
}

template<GDALDataType eDataType, bool bSignedByte>
static void ComputeMinMaxGeneric(const void* pData,
                                 int nXCheck,
                                 int nYCheck,
                                 int nBlockXSize,
                                 bool bGotNoDataValue,
                                 double dfNoDataValue,
                                 bool bGotFloatNoDataValue,
                                 float fNoDataValue,
                                 double &dfMin, double& dfMax)
{
    double dfLocalMin = dfMin;
    double dfLocalMax = dfMax;

    for( int iY = 0; iY < nYCheck; iY++ )
    {
        for( int iX = 0; iX < nXCheck; iX++ )
        {
            const GPtrDiff_t iOffset = iX + static_cast<GPtrDiff_t>(iY) * nBlockXSize;
            bool bValid = true;
            double dfValue = GetPixelValue( eDataType,
                                            bSignedByte,
                                            pData,
                                            iOffset,
                                            bGotNoDataValue,
                                            dfNoDataValue,
                                            bGotFloatNoDataValue,
                                            fNoDataValue,
                                            bValid );
            if( !bValid )
                continue;

            dfLocalMin = std::min(dfLocalMin, dfValue);
            dfLocalMax = std::max(dfLocalMax, dfValue);
        }
    }

    dfMin = dfLocalMin;
    dfMax = dfLocalMax;
}

static void ComputeMinMaxGeneric(const void* pData,
                                 GDALDataType eDataType,
                                 bool bSignedByte,
                                 int nXCheck,
                                 int nYCheck,
                                 int nBlockXSize,
                                 bool bGotNoDataValue,
                                 double dfNoDataValue,
                                 bool bGotFloatNoDataValue,
                                 float fNoDataValue,
                                 double &dfMin, double& dfMax)
{
    switch( eDataType)
    {
        case GDT_Unknown:
            CPLAssert(false);
            break;
        case GDT_Byte:
            CPLAssert(bSignedByte);
            (void)bSignedByte;
            ComputeMinMaxGeneric<GDT_Byte, true>(
                pData, nXCheck, nYCheck, nBlockXSize,
                bGotNoDataValue, dfNoDataValue, false, 0, dfMin, dfMax);
            break;
        case GDT_UInt16:
        case GDT_Int16:
            CPLAssert(false);
            break;
        case GDT_UInt32:
            ComputeMinMaxGeneric<GDT_UInt32, false>(
                pData, nXCheck, nYCheck, nBlockXSize,
                bGotNoDataValue, dfNoDataValue, false, 0, dfMin, dfMax);
            break;
        case GDT_Int32:
            ComputeMinMaxGeneric<GDT_Int32, false>(
                pData, nXCheck, nYCheck, nBlockXSize,
                bGotNoDataValue, dfNoDataValue, false, 0, dfMin, dfMax);
            break;
        case GDT_UInt64:
            ComputeMinMaxGeneric<GDT_UInt64, false>(
                pData, nXCheck, nYCheck, nBlockXSize,
                bGotNoDataValue, dfNoDataValue, false, 0, dfMin, dfMax);
            break;
        case GDT_Int64:
            ComputeMinMaxGeneric<GDT_Int64, false>(
                pData, nXCheck, nYCheck, nBlockXSize,
                bGotNoDataValue, dfNoDataValue, false, 0, dfMin, dfMax);
            break;
        case GDT_Float32:
            ComputeMinMaxGeneric<GDT_Float32, false>(
                pData, nXCheck, nYCheck, nBlockXSize,
                false, 0, bGotFloatNoDataValue, fNoDataValue, dfMin, dfMax);
            break;
        case GDT_Float64:
            ComputeMinMaxGeneric<GDT_Float64, false>(
                pData, nXCheck, nYCheck, nBlockXSize,
                bGotNoDataValue, dfNoDataValue, false, 0, dfMin, dfMax);
            break;
        case GDT_CInt16:
            ComputeMinMaxGeneric<GDT_CInt16, false>(
                pData, nXCheck, nYCheck, nBlockXSize,
                bGotNoDataValue, dfNoDataValue, false, 0, dfMin, dfMax);
            break;
        case GDT_CInt32:
            ComputeMinMaxGeneric<GDT_CInt32, false>(
                pData, nXCheck, nYCheck, nBlockXSize,
                bGotNoDataValue, dfNoDataValue, false, 0, dfMin, dfMax);
            break;
        case GDT_CFloat32:
            ComputeMinMaxGeneric<GDT_CFloat32, false>(
                pData, nXCheck, nYCheck, nBlockXSize,
                bGotNoDataValue, dfNoDataValue, false, 0, dfMin, dfMax);
            break;
        case GDT_CFloat64:
            ComputeMinMaxGeneric<GDT_CFloat64, false>(
                pData, nXCheck, nYCheck, nBlockXSize,
                bGotNoDataValue, dfNoDataValue, false, 0, dfMin, dfMax);
            break;
        case GDT_TypeCount:
            CPLAssert(false);
            break;
    }
}

static bool
ComputeMinMaxGenericIterBlocks(GDALRasterBand* poBand,
                               GDALDataType eDataType,
                               bool bSignedByte,
                                           int nTotalBlocks,
                                           int nSampleRate,
                                           int nBlocksPerRow,
                                           bool bGotNoDataValue,
                                           double dfNoDataValue,
                                           bool bGotFloatNoDataValue,
                                           float fNoDataValue,
                                           double& dfMin,
                                           double& dfMax)

{
    int nBlockXSize, nBlockYSize;
    poBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
    for( int iSampleBlock = 0;
         iSampleBlock < nTotalBlocks;
         iSampleBlock += nSampleRate )
    {
        const int iYBlock = iSampleBlock / nBlocksPerRow;
        const int iXBlock = iSampleBlock - nBlocksPerRow * iYBlock;

        GDALRasterBlock *poBlock = poBand->GetLockedBlockRef( iXBlock, iYBlock );
        if( poBlock == nullptr )
            return false;

        void * const pData = poBlock->GetDataRef();

        int nXCheck = 0, nYCheck = 0;
        poBand->GetActualBlockSize(iXBlock, iYBlock, &nXCheck, &nYCheck);

        ComputeMinMaxGeneric(pData, eDataType, bSignedByte,
                             nXCheck, nYCheck, nBlockXSize,
                             CPL_TO_BOOL(bGotNoDataValue),
                             dfNoDataValue,
                             bGotFloatNoDataValue,
                             fNoDataValue,
                             dfMin, dfMax);

        poBlock->DropLock();
    }
    return true;
}

/**
 * \brief Compute the min/max values for a band.
 *
 * If approximate is OK, then the band's GetMinimum()/GetMaximum() will
 * be trusted.  If it doesn't work, a subsample of blocks will be read to
 * get an approximate min/max.  If the band has a nodata value it will
 * be excluded from the minimum and maximum.
 *
 * If bApprox is FALSE, then all pixels will be read and used to compute
 * an exact range.
 *
 * This method is the same as the C function GDALComputeRasterMinMax().
 *
 * @param bApproxOK TRUE if an approximate (faster) answer is OK, otherwise
 * FALSE.
 * @param adfMinMax the array in which the minimum (adfMinMax[0]) and the
 * maximum (adfMinMax[1]) are returned.
 *
 * @return CE_None on success or CE_Failure on failure.
 */

CPLErr GDALRasterBand::ComputeRasterMinMax( int bApproxOK,
                                            double* adfMinMax )
{
/* -------------------------------------------------------------------- */
/*      Does the driver already know the min/max?                       */
/* -------------------------------------------------------------------- */
    if( bApproxOK )
    {
        int bSuccessMin = FALSE;
        int bSuccessMax = FALSE;

        double dfMin = GetMinimum( &bSuccessMin );
        double dfMax = GetMaximum( &bSuccessMax );

        if( bSuccessMin && bSuccessMax )
        {
            adfMinMax[0] = dfMin;
            adfMinMax[1] = dfMax;
            return CE_None;
        }
    }

/* -------------------------------------------------------------------- */
/*      If we have overview bands, use them for min/max.                */
/* -------------------------------------------------------------------- */
    // cppcheck-suppress knownConditionTrueFalse
    if ( bApproxOK && GetOverviewCount() > 0 && !HasArbitraryOverviews() )
    {
        GDALRasterBand *poBand =
          GetRasterSampleOverview( GDALSTAT_APPROX_NUMSAMPLES );

        if ( poBand != this )
            return poBand->ComputeRasterMinMax( FALSE, adfMinMax );
    }

/* -------------------------------------------------------------------- */
/*      Read actual data and compute minimum and maximum.               */
/* -------------------------------------------------------------------- */
    int bGotNoDataValue = FALSE;
    const double dfNoDataValue = GetNoDataValue( &bGotNoDataValue );
    bGotNoDataValue = bGotNoDataValue && !CPLIsNan(dfNoDataValue);
    bool bGotFloatNoDataValue = false;
    float fNoDataValue = 0.0f;
    ComputeFloatNoDataValue( eDataType, dfNoDataValue, bGotNoDataValue,
                            fNoDataValue, bGotFloatNoDataValue );

    const char* pszPixelType = GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
    const bool bSignedByte =
        pszPixelType != nullptr && EQUAL(pszPixelType, "SIGNEDBYTE");

    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);

    GUInt32 nMin = (eDataType == GDT_Byte) ? 255 : 65535; // used for GByte & GUInt16 cases
    GUInt32 nMax = 0;// used for GByte & GUInt16 cases
    GInt16 nMinInt16 = std::numeric_limits<GInt16>::max(); // used for GInt16 case
    GInt16 nMaxInt16 = std::numeric_limits<GInt16>::lowest(); // used for GInt16 case
    double dfMin = std::numeric_limits<double>::max(); // used for generic code path
    double dfMax = -std::numeric_limits<double>::max(); // used for generic code path
    const bool bUseOptimizedPath = ( eDataType == GDT_Byte && !bSignedByte ) ||
                                     eDataType == GDT_Int16 ||
                                     eDataType == GDT_UInt16;

    const auto ComputeMinMaxForBlock = [
        this, bSignedByte,
        bGotNoDataValue, dfNoDataValue,
        &nMin, &nMax, &nMinInt16, &nMaxInt16]
        (const void* pData, int nXCheck, int nBufferWidth, int nYCheck)
    {
        if( eDataType == GDT_Byte && !bSignedByte )
        {
            const bool bHasNoData =
                bGotNoDataValue &&
                GDALIsValueInRange<GByte>(dfNoDataValue) &&
                static_cast<GByte>(dfNoDataValue) == dfNoDataValue;
            const GUInt32 nNoDataValue = bHasNoData ? static_cast<GByte>(dfNoDataValue) : 0;
            GUIntBig nSum, nSumSquare, nSampleCount, nValidCount; // unused
            ComputeStatisticsInternal<GByte, /* COMPUTE_OTHER_STATS = */ false>::f(
                nXCheck, nBufferWidth, nYCheck,
                static_cast<const GByte*>(pData),
                bHasNoData,
                nNoDataValue,
                nMin,
                nMax,
                nSum, nSumSquare, nSampleCount, nValidCount);
        }
        else if( eDataType == GDT_UInt16 )
        {
            const bool bHasNoData =
                bGotNoDataValue &&
                GDALIsValueInRange<GUInt16>(dfNoDataValue) &&
                static_cast<GUInt16>(dfNoDataValue) == dfNoDataValue;
            const GUInt32 nNoDataValue = bHasNoData ? static_cast<GUInt16>(dfNoDataValue) : 0;
            GUIntBig nSum, nSumSquare, nSampleCount, nValidCount; // unused
            ComputeStatisticsInternal<GUInt16, /* COMPUTE_OTHER_STATS = */ false>::f(
                nXCheck, nBufferWidth, nYCheck,
                static_cast<const GUInt16*>(pData),
                bHasNoData,
                nNoDataValue,
                nMin,
                nMax,
                nSum, nSumSquare, nSampleCount, nValidCount);
        }
        else if( eDataType == GDT_Int16 )
        {
            const bool bHasNoData =
                bGotNoDataValue &&
                GDALIsValueInRange<int16_t>(dfNoDataValue) &&
                static_cast<int16_t>(dfNoDataValue) == dfNoDataValue;
            if( bHasNoData )
            {
                const int16_t nNoDataValue = static_cast<int16_t>(dfNoDataValue);
                for( int iY = 0; iY < nYCheck; iY++ )
                {
                    ComputeMinMax<int16_t, true>(
                        static_cast<const int16_t*>(pData) + static_cast<size_t>(iY) * nBufferWidth,
                        nXCheck,
                        nNoDataValue,
                        &nMinInt16,
                        &nMaxInt16);
                }
            }
            else
            {
                for( int iY = 0; iY < nYCheck; iY++ )
                {
                    ComputeMinMax<int16_t, false>(
                        static_cast<const int16_t*>(pData) + static_cast<size_t>(iY) * nBufferWidth,
                        nXCheck,
                        0,
                        &nMinInt16,
                        &nMaxInt16);
                }
            }
        }
    };

    if ( bApproxOK && HasArbitraryOverviews() )
    {
/* -------------------------------------------------------------------- */
/*      Figure out how much the image should be reduced to get an       */
/*      approximate value.                                              */
/* -------------------------------------------------------------------- */
        double  dfReduction = sqrt(
            static_cast<double>(nRasterXSize) * nRasterYSize /
            GDALSTAT_APPROX_NUMSAMPLES );

        int nXReduced = nRasterXSize;
        int nYReduced = nRasterYSize;
        if ( dfReduction > 1.0 )
        {
            nXReduced = static_cast<int>( nRasterXSize / dfReduction );
            nYReduced = static_cast<int>( nRasterYSize / dfReduction );

            // Catch the case of huge resizing ratios here
            if ( nXReduced == 0 )
                nXReduced = 1;
            if ( nYReduced == 0 )
                nYReduced = 1;
        }

        void * const pData =
            CPLMalloc(
                GDALGetDataTypeSizeBytes(eDataType) * nXReduced * nYReduced );

        const CPLErr eErr = IRasterIO(
            GF_Read, 0, 0, nRasterXSize, nRasterYSize, pData,
            nXReduced, nYReduced, eDataType, 0, 0, &sExtraArg );
        if ( eErr != CE_None )
        {
            CPLFree(pData);
            return eErr;
        }

        if( bUseOptimizedPath )
        {
            ComputeMinMaxForBlock(pData, nXReduced, nXReduced, nYReduced);
        }
        else
        {
            ComputeMinMaxGeneric(pData, eDataType, bSignedByte,
                                 nXReduced, nYReduced, nXReduced,
                                 CPL_TO_BOOL(bGotNoDataValue),
                                 dfNoDataValue,
                                 bGotFloatNoDataValue,
                                 fNoDataValue,
                                 dfMin, dfMax);
        }


        CPLFree( pData );
    }

    else  // No arbitrary overviews
    {
        if( !InitBlockInfo() )
            return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Figure out the ratio of blocks we will read to get an           */
/*      approximate value.                                              */
/* -------------------------------------------------------------------- */
        int nSampleRate = 1;

        if ( bApproxOK )
        {
            nSampleRate = static_cast<int>(
                std::max(1.0,
                         sqrt(static_cast<double>(nBlocksPerRow) *
                              nBlocksPerColumn)));
            // We want to avoid probing only the first column of blocks for
            // a square shaped raster, because it is not unlikely that it may
            // be padding only (#6378).
            if( nSampleRate == nBlocksPerRow && nBlocksPerRow > 1 )
              nSampleRate += 1;
        }

        if( bUseOptimizedPath )
        {
            for( int iSampleBlock = 0;
                 iSampleBlock < nBlocksPerRow * nBlocksPerColumn;
                 iSampleBlock += nSampleRate )
            {
                const int iYBlock = iSampleBlock / nBlocksPerRow;
                const int iXBlock = iSampleBlock - nBlocksPerRow * iYBlock;

                GDALRasterBlock *poBlock = GetLockedBlockRef( iXBlock, iYBlock );
                if( poBlock == nullptr )
                    return CE_Failure;

                void * const pData = poBlock->GetDataRef();

                int nXCheck = 0, nYCheck = 0;
                GetActualBlockSize(iXBlock, iYBlock, &nXCheck, &nYCheck);

                ComputeMinMaxForBlock(pData, nXCheck, nBlockXSize, nYCheck);

                poBlock->DropLock();

                if( eDataType == GDT_Byte && !bSignedByte && nMin == 0 && nMax == 255 )
                    break;
            }
        }
        else
        {
            const int nTotalBlocks = nBlocksPerRow * nBlocksPerColumn;
            if( !ComputeMinMaxGenericIterBlocks(this, eDataType,
                                                bSignedByte,
                                                nTotalBlocks,
                                                nSampleRate,
                                                nBlocksPerRow,
                                                CPL_TO_BOOL(bGotNoDataValue),
                                                dfNoDataValue,
                                                bGotFloatNoDataValue,
                                                fNoDataValue,
                                                dfMin,
                                                dfMax) )
            {
                return CE_Failure;
            }
        }
    }

    if( (eDataType == GDT_Byte && !bSignedByte) || eDataType == GDT_UInt16 )
    {
        dfMin = nMin;
        dfMax = nMax;
    }
    else if( eDataType == GDT_Int16 )
    {
        dfMin = nMinInt16;
        dfMax = nMaxInt16;
    }

    if( dfMin > dfMax )
    {
        adfMinMax[0] = 0;
        adfMinMax[1] = 0;
        ReportError(
            CE_Failure, CPLE_AppDefined,
            "Failed to compute min/max, no valid pixels found in sampling." );
        return CE_Failure;
    }

    adfMinMax[0] = dfMin;
    adfMinMax[1] = dfMax;

    return CE_None;
}

/************************************************************************/
/*                      GDALComputeRasterMinMax()                       */
/************************************************************************/

/**
 * \brief Compute the min/max values for a band.
 *
 * @see GDALRasterBand::ComputeRasterMinMax()
 */

void CPL_STDCALL
GDALComputeRasterMinMax( GDALRasterBandH hBand, int bApproxOK,
                         double adfMinMax[2] )

{
    VALIDATE_POINTER0( hBand, "GDALComputeRasterMinMax" );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    poBand->ComputeRasterMinMax( bApproxOK, adfMinMax );
}

/************************************************************************/
/*                        SetDefaultHistogram()                         */
/************************************************************************/

/* FIXME : add proper documentation */
/**
 * \brief Set default histogram.
 *
 * This method is the same as the C function GDALSetDefaultHistogram() and
 * GDALSetDefaultHistogramEx()
 */
CPLErr GDALRasterBand::SetDefaultHistogram( double /* dfMin */,
                                            double /* dfMax */,
                                            int /* nBuckets */,
                                            GUIntBig * /* panHistogram */)

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError( CE_Failure, CPLE_NotSupported,
                     "SetDefaultHistogram() not implemented for this format." );

    return CE_Failure;
}

/************************************************************************/
/*                      GDALSetDefaultHistogram()                       */
/************************************************************************/

/**
 * \brief Set default histogram.
 *
 * Use GDALSetRasterHistogramEx() instead to be able to set counts exceeding
 * 2 billion.
 *
 * @see GDALRasterBand::SetDefaultHistogram()
 * @see GDALSetRasterHistogramEx()
 */

CPLErr CPL_STDCALL GDALSetDefaultHistogram( GDALRasterBandH hBand,
                                            double dfMin, double dfMax,
                                            int nBuckets, int *panHistogram )

{
    VALIDATE_POINTER1( hBand, "GDALSetDefaultHistogram", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);

    GUIntBig* panHistogramTemp = static_cast<GUIntBig *>(
        VSIMalloc2(sizeof(GUIntBig), nBuckets) );
    if( panHistogramTemp == nullptr )
    {
        poBand->ReportError(
            CE_Failure, CPLE_OutOfMemory,
            "Out of memory in GDALSetDefaultHistogram()." );
        return CE_Failure;
    }

    for( int i = 0; i < nBuckets; ++i )
    {
        panHistogramTemp[i] = static_cast<GUIntBig>(panHistogram[i]);
    }

    const CPLErr eErr =
        poBand->SetDefaultHistogram( dfMin, dfMax, nBuckets, panHistogramTemp );

    CPLFree(panHistogramTemp);

    return eErr;
}

/************************************************************************/
/*                     GDALSetDefaultHistogramEx()                      */
/************************************************************************/

/**
 * \brief Set default histogram.
 *
 * @see GDALRasterBand::SetDefaultHistogram()
 *
 * @since GDAL 2.0
 */

CPLErr CPL_STDCALL GDALSetDefaultHistogramEx( GDALRasterBandH hBand,
                                              double dfMin, double dfMax,
                                              int nBuckets,
                                              GUIntBig *panHistogram )

{
    VALIDATE_POINTER1( hBand, "GDALSetDefaultHistogramEx", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->SetDefaultHistogram( dfMin, dfMax, nBuckets, panHistogram );
}

/************************************************************************/
/*                           GetDefaultRAT()                            */
/************************************************************************/

/**
 * \brief Fetch default Raster Attribute Table.
 *
 * A RAT will be returned if there is a default one associated with the
 * band, otherwise NULL is returned.  The returned RAT is owned by the
 * band and should not be deleted by the application.
 *
 * This method is the same as the C function GDALGetDefaultRAT().
 *
 * @return NULL, or a pointer to an internal RAT owned by the band.
 */

GDALRasterAttributeTable *GDALRasterBand::GetDefaultRAT()

{
    return nullptr;
}

/************************************************************************/
/*                         GDALGetDefaultRAT()                          */
/************************************************************************/

/**
 * \brief Fetch default Raster Attribute Table.
 *
 * @see GDALRasterBand::GetDefaultRAT()
 */

GDALRasterAttributeTableH CPL_STDCALL GDALGetDefaultRAT( GDALRasterBandH hBand)

{
    VALIDATE_POINTER1( hBand, "GDALGetDefaultRAT", nullptr );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return GDALRasterAttributeTable::ToHandle(poBand->GetDefaultRAT());
}

/************************************************************************/
/*                           SetDefaultRAT()                            */
/************************************************************************/

/**
 * \fn GDALRasterBand::SetDefaultRAT(const GDALRasterAttributeTable*)
 * \brief Set default Raster Attribute Table.
 *
 * Associates a default RAT with the band.  If not implemented for the
 * format a CPLE_NotSupported error will be issued.  If successful a copy
 * of the RAT is made, the original remains owned by the caller.
 *
 * This method is the same as the C function GDALSetDefaultRAT().
 *
 * @param poRAT the RAT to assign to the band.
 *
 * @return CE_None on success or CE_Failure if unsupported or otherwise
 * failing.
 */

/**/
/**/

CPLErr GDALRasterBand::SetDefaultRAT(
    const GDALRasterAttributeTable * /* poRAT */ )
{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        ReportError( CE_Failure, CPLE_NotSupported,
                     "SetDefaultRAT() not implemented for this format." );
        CPLPopErrorHandler();
    }
    return CE_Failure;
}

/************************************************************************/
/*                         GDALSetDefaultRAT()                          */
/************************************************************************/

/**
 * \brief Set default Raster Attribute Table.
 *
 * @see GDALRasterBand::GDALSetDefaultRAT()
 */

CPLErr CPL_STDCALL GDALSetDefaultRAT( GDALRasterBandH hBand,
                                      GDALRasterAttributeTableH hRAT )

{
    VALIDATE_POINTER1( hBand, "GDALSetDefaultRAT", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);

    return poBand->SetDefaultRAT(
        GDALRasterAttributeTable::FromHandle(hRAT) );
}

/************************************************************************/
/*                            GetMaskBand()                             */
/************************************************************************/

/**
 * \brief Return the mask band associated with the band.
 *
 * The GDALRasterBand class includes a default implementation of GetMaskBand() that
 * returns one of four default implementations :
 * <ul>
 * <li>If a corresponding .msk file exists it will be used for the mask band.</li>
 * <li>If the dataset has a NODATA_VALUES metadata item, an instance of the
 *     new GDALNoDataValuesMaskBand class will be returned.
 *     GetMaskFlags() will return GMF_NODATA | GMF_PER_DATASET. @since GDAL 1.6.0</li>
 * <li>If the band has a nodata value set, an instance of the new
 *     GDALNodataMaskRasterBand class will be returned.
 *     GetMaskFlags() will return GMF_NODATA.</li>
 * <li>If there is no nodata value, but the dataset has an alpha band that seems
 *     to apply to this band (specific rules yet to be determined) and that is
 *     of type GDT_Byte then that alpha band will be returned, and the flags
 *     GMF_PER_DATASET and GMF_ALPHA will be returned in the flags.</li>
 * <li>If neither of the above apply, an instance of the new GDALAllValidRasterBand
 *     class will be returned that has 255 values for all pixels.
 *     The null flags will return GMF_ALL_VALID.</li>
 * </ul>
 *
 * Note that the GetMaskBand() should always return a GDALRasterBand mask, even
 * if it is only an all 255 mask with the flags indicating GMF_ALL_VALID.
 *
 * For an external .msk file to be recognized by GDAL, it must be a valid GDAL
 * dataset, with the same name as the main dataset and suffixed with .msk,
 * with either one band (in the GMF_PER_DATASET case), or as many bands as the
 * main dataset.
 * It must have INTERNAL_MASK_FLAGS_xx metadata items set at the dataset
 * level, where xx matches the band number of a band of the main dataset. The
 * value of those items is a combination of the flags GMF_ALL_VALID,
 * GMF_PER_DATASET, GMF_ALPHA and GMF_NODATA. If a metadata item is missing for
 * a band, then the other rules explained above will be used to generate a
 * on-the-fly mask band.
 * \see CreateMaskBand() for the characteristics of .msk files created by GDAL.
 *
 * This method is the same as the C function GDALGetMaskBand().
 *
 * @return a valid mask band.
 *
 * @since GDAL 1.5.0
 *
 * @see https://gdal.org/development/rfc/rfc15_nodatabitmask.html
 *
 */
GDALRasterBand *GDALRasterBand::GetMaskBand()

{
    if( poMask != nullptr )
        return poMask;

/* -------------------------------------------------------------------- */
/*      Check for a mask in a .msk file.                                */
/* -------------------------------------------------------------------- */
    if( poDS != nullptr && poDS->oOvManager.HaveMaskFile() )
    {
        poMask = poDS->oOvManager.GetMaskBand( nBand );
        if( poMask != nullptr )
        {
            nMaskFlags = poDS->oOvManager.GetMaskFlags( nBand );
            return poMask;
        }
    }

/* -------------------------------------------------------------------- */
/*      Check for NODATA_VALUES metadata.                               */
/* -------------------------------------------------------------------- */
    if( poDS != nullptr )
    {
        const char* pszNoDataValues = poDS->GetMetadataItem("NODATA_VALUES");
        if( pszNoDataValues != nullptr )
        {
            char** papszNoDataValues
                = CSLTokenizeStringComplex(pszNoDataValues, " ", FALSE, FALSE);

            // Make sure we have as many values as bands.
            if (CSLCount(papszNoDataValues) == poDS->GetRasterCount()
                && poDS->GetRasterCount() != 0)
            {
                // Make sure that all bands have the same data type
                // This is clearly not a fundamental condition, just a
                // condition to make implementation easier.
                GDALDataType eDT = GDT_Unknown;
                int i = 0;  // Used after for.
                for( ; i < poDS->GetRasterCount(); ++i )
                {
                    if( i == 0 )
                        eDT = poDS->GetRasterBand(1)->GetRasterDataType();
                    else if (eDT != poDS->GetRasterBand(i + 1)->GetRasterDataType())
                    {
                        break;
                    }
                }
                if( i == poDS->GetRasterCount() )
                {
                    nMaskFlags = GMF_NODATA | GMF_PER_DATASET;
                    try
                    {
                        poMask = new GDALNoDataValuesMaskBand ( poDS );
                    }
                    catch( const std::bad_alloc& )
                    {
                        CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
                        poMask = nullptr;
                    }
                    bOwnMask = true;
                    CSLDestroy(papszNoDataValues);
                    return poMask;
                }
                else
                {
                    ReportError( CE_Warning, CPLE_AppDefined,
                                 "All bands should have the same type in "
                                 "order the NODATA_VALUES metadata item "
                                 "to be used as a mask." );
                }
            }
            else
            {
                ReportError(
                    CE_Warning, CPLE_AppDefined,
                    "NODATA_VALUES metadata item doesn't have the same number "
                    "of values as the number of bands.  "
                    "Ignoring it for mask.");
            }

            CSLDestroy(papszNoDataValues);
        }
    }

/* -------------------------------------------------------------------- */
/*      Check for nodata case.                                          */
/* -------------------------------------------------------------------- */
    int bHaveNoDataRaw = FALSE;
    bool bHaveNoData = false;
    if( eDataType == GDT_Int64 )
    {
        CPL_IGNORE_RET_VAL(GetNoDataValueAsInt64(&bHaveNoDataRaw));
        bHaveNoData = CPL_TO_BOOL(bHaveNoDataRaw);
    }
    else if( eDataType == GDT_UInt64 )
    {
        CPL_IGNORE_RET_VAL(GetNoDataValueAsUInt64(&bHaveNoDataRaw));
        bHaveNoData = CPL_TO_BOOL(bHaveNoDataRaw);
    }
    else
    {
        const double dfNoDataValue = GetNoDataValue( &bHaveNoDataRaw );
        if( bHaveNoDataRaw &&
            GDALNoDataMaskBand::IsNoDataInRange(dfNoDataValue, eDataType) )
        {
            bHaveNoData = true;
        }
    }
    if( bHaveNoData )
    {
        nMaskFlags = GMF_NODATA;
        try
        {
            poMask = new GDALNoDataMaskBand( this );
        }
        catch( const std::bad_alloc& )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
            poMask = nullptr;
        }
        bOwnMask = true;
        return poMask;
    }

/* -------------------------------------------------------------------- */
/*      Check for alpha case.                                           */
/* -------------------------------------------------------------------- */
    if( poDS != nullptr
        && poDS->GetRasterCount() == 2
        && this == poDS->GetRasterBand(1)
        && poDS->GetRasterBand(2)->GetColorInterpretation() == GCI_AlphaBand )
    {
        if( poDS->GetRasterBand(2)->GetRasterDataType() == GDT_Byte )
        {
            nMaskFlags = GMF_ALPHA | GMF_PER_DATASET;
            poMask = poDS->GetRasterBand(2);
            return poMask;
        }
        else if( poDS->GetRasterBand(2)->GetRasterDataType() == GDT_UInt16 )
        {
            nMaskFlags = GMF_ALPHA | GMF_PER_DATASET;
            try
            {
                poMask = new GDALRescaledAlphaBand( poDS->GetRasterBand(2) );
            }
            catch( const std::bad_alloc& )
            {
                CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
                poMask = nullptr;
            }
            bOwnMask = true;
            return poMask;
        }
    }

    if( poDS != nullptr
        && poDS->GetRasterCount() == 4
        && (this == poDS->GetRasterBand(1)
            || this == poDS->GetRasterBand(2)
            || this == poDS->GetRasterBand(3))
        && poDS->GetRasterBand(4)->GetColorInterpretation() == GCI_AlphaBand )
    {
        if( poDS->GetRasterBand(4)->GetRasterDataType() == GDT_Byte )
        {
            nMaskFlags = GMF_ALPHA | GMF_PER_DATASET;
            poMask = poDS->GetRasterBand(4);
            return poMask;
        }
        else if( poDS->GetRasterBand(4)->GetRasterDataType() == GDT_UInt16 )
        {
            nMaskFlags = GMF_ALPHA | GMF_PER_DATASET;
            try
            {
                poMask = new GDALRescaledAlphaBand( poDS->GetRasterBand(4) );
            }
            catch( const std::bad_alloc& )
            {
                CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
                poMask = nullptr;
            }
            bOwnMask = true;
            return poMask;
        }
    }

/* -------------------------------------------------------------------- */
/*      Fallback to all valid case.                                     */
/* -------------------------------------------------------------------- */
    nMaskFlags = GMF_ALL_VALID;
    try
    {
        poMask = new GDALAllValidMaskBand( this );
    }
    catch( const std::bad_alloc& )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
        poMask = nullptr;
    }
    bOwnMask = true;

    return poMask;
}

/************************************************************************/
/*                          GDALGetMaskBand()                           */
/************************************************************************/

/**
 * \brief Return the mask band associated with the band.
 *
 * @see GDALRasterBand::GetMaskBand()
 */

GDALRasterBandH CPL_STDCALL GDALGetMaskBand( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetMaskBand", nullptr );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->GetMaskBand();
}

/************************************************************************/
/*                            GetMaskFlags()                            */
/************************************************************************/

/**
 * \brief Return the status flags of the mask band associated with the band.
 *
 * The GetMaskFlags() method returns an bitwise OR-ed set of status flags with
 * the following available definitions that may be extended in the future:
 * <ul>
 * <li>GMF_ALL_VALID(0x01): There are no invalid pixels, all mask values will be 255.
 *     When used this will normally be the only flag set.</li>
 * <li>GMF_PER_DATASET(0x02): The mask band is shared between all bands on the dataset.</li>
 * <li>GMF_ALPHA(0x04): The mask band is actually an alpha band and may have values
 *     other than 0 and 255.</li>
 * <li>GMF_NODATA(0x08): Indicates the mask is actually being generated from nodata values.
 *     (mutually exclusive of GMF_ALPHA)</li>
 * </ul>
 *
 * The GDALRasterBand class includes a default implementation of GetMaskBand() that
 * returns one of four default implementations :
 * <ul>
 * <li>If a corresponding .msk file exists it will be used for the mask band.</li>
 * <li>If the dataset has a NODATA_VALUES metadata item, an instance of the
 *     new GDALNoDataValuesMaskBand class will be returned.
 *     GetMaskFlags() will return GMF_NODATA | GMF_PER_DATASET. @since GDAL 1.6.0</li>
 * <li>If the band has a nodata value set, an instance of the new
 *     GDALNodataMaskRasterBand class will be returned.
 *     GetMaskFlags() will return GMF_NODATA.</li>
 * <li>If there is no nodata value, but the dataset has an alpha band that seems
 *     to apply to this band (specific rules yet to be determined) and that is
 *     of type GDT_Byte then that alpha band will be returned, and the flags
 *     GMF_PER_DATASET and GMF_ALPHA will be returned in the flags.</li>
 * <li>If neither of the above apply, an instance of the new GDALAllValidRasterBand
 *     class will be returned that has 255 values for all pixels.
 *     The null flags will return GMF_ALL_VALID.</li>
 * </ul>
 *
 * For an external .msk file to be recognized by GDAL, it must be a valid GDAL
 * dataset, with the same name as the main dataset and suffixed with .msk,
 * with either one band (in the GMF_PER_DATASET case), or as many bands as the
 * main dataset.
 * It must have INTERNAL_MASK_FLAGS_xx metadata items set at the dataset
 * level, where xx matches the band number of a band of the main dataset. The
 * value of those items is a combination of the flags GMF_ALL_VALID,
 * GMF_PER_DATASET, GMF_ALPHA and GMF_NODATA. If a metadata item is missing for
 * a band, then the other rules explained above will be used to generate a
 * on-the-fly mask band.
 * \see CreateMaskBand() for the characteristics of .msk files created by GDAL.
 *
 * This method is the same as the C function GDALGetMaskFlags().
 *
 * @since GDAL 1.5.0
 *
 * @return a valid mask band.
 *
 * @see https://gdal.org/development/rfc/rfc15_nodatabitmask.html
 *
 */
int GDALRasterBand::GetMaskFlags()

{
    // If we don't have a band yet, force this now so that the masks value
    // will be initialized.

    if( poMask == nullptr )
        GetMaskBand();

    return nMaskFlags;
}

/************************************************************************/
/*                          GDALGetMaskFlags()                          */
/************************************************************************/

/**
 * \brief Return the status flags of the mask band associated with the band.
 *
 * @see GDALRasterBand::GetMaskFlags()
 */

int CPL_STDCALL GDALGetMaskFlags( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetMaskFlags", GMF_ALL_VALID );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->GetMaskFlags();
}

/************************************************************************/
/*                         InvalidateMaskBand()                         */
/************************************************************************/

//! @cond Doxygen_Suppress
void GDALRasterBand::InvalidateMaskBand()
{
    if (bOwnMask)
        delete poMask;
    bOwnMask = false;
    nMaskFlags = 0;
    poMask = nullptr;
}
//! @endcond

/************************************************************************/
/*                           CreateMaskBand()                           */
/************************************************************************/

/**
 * \brief Adds a mask band to the current band
 *
 * The default implementation of the CreateMaskBand() method is implemented
 * based on similar rules to the .ovr handling implemented using the
 * GDALDefaultOverviews object. A TIFF file with the extension .msk will
 * be created with the same basename as the original file, and it will have
 * as many bands as the original image (or just one for GMF_PER_DATASET).
 * The mask images will be deflate compressed tiled images with the same
 * block size as the original image if possible.
 * It will have INTERNAL_MASK_FLAGS_xx metadata items set at the dataset
 * level, where xx matches the band number of a band of the main dataset. The
 * value of those items will be the one of the nFlagsIn parameter.
 *
 * Note that if you got a mask band with a previous call to GetMaskBand(),
 * it might be invalidated by CreateMaskBand(). So you have to call GetMaskBand()
 * again.
 *
 * This method is the same as the C function GDALCreateMaskBand().
 *
 * @since GDAL 1.5.0
 *
 * @param nFlagsIn 0 or combination of GMF_PER_DATASET / GMF_ALPHA.
 *
 * @return CE_None on success or CE_Failure on an error.
 *
 * @see https://gdal.org/development/rfc/rfc15_nodatabitmask.html
 * @see GDALDataset::CreateMaskBand()
 *
 */

CPLErr GDALRasterBand::CreateMaskBand( int nFlagsIn )

{
    if( poDS != nullptr && poDS->oOvManager.IsInitialized() )
    {
        const CPLErr eErr = poDS->oOvManager.CreateMaskBand( nFlagsIn, nBand );
        if (eErr != CE_None)
            return eErr;

        InvalidateMaskBand();

        return CE_None;
    }

    ReportError( CE_Failure, CPLE_NotSupported,
                 "CreateMaskBand() not supported for this band." );

    return CE_Failure;
}

/************************************************************************/
/*                         GDALCreateMaskBand()                         */
/************************************************************************/

/**
 * \brief Adds a mask band to the current band
 *
 * @see GDALRasterBand::CreateMaskBand()
 */

CPLErr CPL_STDCALL GDALCreateMaskBand( GDALRasterBandH hBand, int nFlags )

{
    VALIDATE_POINTER1( hBand, "GDALCreateMaskBand", CE_Failure );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->CreateMaskBand( nFlags );
}

/************************************************************************/
/*                            IsMaskBand()                              */
/************************************************************************/

/**
 * \brief Returns whether a band is a mask band.
 *
 * Mask band must be understood in the broad term: it can be a per-dataset
 * mask band, an alpha band, or an implicit mask band.
 * Typically the return of GetMaskBand()->IsMaskBand() should be true.
 *
 * This method is the same as the C function GDALIsMaskBand().
 *
 * @return true if the band is a mask band.
 *
 * @see GDALDataset::CreateMaskBand()
 *
 * @since GDAL 3.5.0
 *
 */

bool GDALRasterBand::IsMaskBand() const
{
    // The GeoTIFF driver, among others, override this method to
    // also handle external .msk bands.
    return const_cast<GDALRasterBand*>(this)->GetColorInterpretation() == GCI_AlphaBand;
}

/************************************************************************/
/*                            GDALIsMaskBand()                          */
/************************************************************************/

/**
 * \brief Returns whether a band is a mask band.
 *
 * Mask band must be understood in the broad term: it can be a per-dataset
 * mask band, an alpha band, or an implicit mask band.
 * Typically the return of GetMaskBand()->IsMaskBand() should be true.
 *
 * This function is the same as the C++ method GDALRasterBand::IsMaskBand()
 *
 * @return true if the band is a mask band.
 *
 * @see GDALRasterBand::IsMaskBand()
 *
 * @since GDAL 3.5.0
 *
 */

bool GDALIsMaskBand( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALIsMaskBand", false );

    const GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);
    return poBand->IsMaskBand();
}

/************************************************************************/
/*                         GetMaskValueRange()                          */
/************************************************************************/

/**
 * \brief Returns the range of values that a mask band can take.
 *
 * @return the range of values that a mask band can take.
 *
 * @since GDAL 3.5.0
 *
 */

GDALMaskValueRange GDALRasterBand::GetMaskValueRange() const
{
    return GMVR_UNKNOWN;
}


/************************************************************************/
/*                    GetIndexColorTranslationTo()                      */
/************************************************************************/

/**
 * \brief Compute translation table for color tables.
 *
 * When the raster band has a palette index, it may be useful to compute
 * the "translation" of this palette to the palette of another band.
 * The translation tries to do exact matching first, and then approximate
 * matching if no exact matching is possible.
 * This method returns a table such that table[i] = j where i is an index
 * of the 'this' rasterband and j the corresponding index for the reference
 * rasterband.
 *
 * This method is thought as internal to GDAL and is used for drivers
 * like RPFTOC.
 *
 * The implementation only supports 1-byte palette rasterbands.
 *
 * @param poReferenceBand the raster band
 * @param pTranslationTable an already allocated translation table (at least 256 bytes),
 *                          or NULL to let the method allocate it
 * @param pApproximateMatching a pointer to a flag that is set if the matching
 *                              is approximate. May be NULL.
 *
 * @return a translation table if the two bands are palette index and that they do
 *         not match or NULL in other cases.
 *         The table must be freed with CPLFree if NULL was passed for pTranslationTable.
 */

unsigned char* GDALRasterBand::GetIndexColorTranslationTo(
    GDALRasterBand* poReferenceBand,
    unsigned char* pTranslationTable,
    int* pApproximateMatching )
{
    if (poReferenceBand == nullptr)
        return nullptr;

    // cppcheck-suppress knownConditionTrueFalse
    if (poReferenceBand->GetColorInterpretation() == GCI_PaletteIndex &&
        // cppcheck-suppress knownConditionTrueFalse
        GetColorInterpretation() == GCI_PaletteIndex &&
        poReferenceBand->GetRasterDataType() == GDT_Byte &&
        GetRasterDataType() == GDT_Byte)
    {
        const GDALColorTable* srcColorTable = GetColorTable();
        GDALColorTable* destColorTable = poReferenceBand->GetColorTable();
        if (srcColorTable != nullptr && destColorTable != nullptr)
        {
            const int nEntries = srcColorTable->GetColorEntryCount();
            const int nRefEntries = destColorTable->GetColorEntryCount();

            int bHasNoDataValueSrc = FALSE;
            double dfNoDataValueSrc = GetNoDataValue(&bHasNoDataValueSrc);
            if( !(bHasNoDataValueSrc &&
                  dfNoDataValueSrc >= 0 && dfNoDataValueSrc <= 255 &&
                  dfNoDataValueSrc == static_cast<int>(dfNoDataValueSrc)) )
                bHasNoDataValueSrc = FALSE;
            const int noDataValueSrc =
                bHasNoDataValueSrc ? static_cast<int>(dfNoDataValueSrc) : 0;

            int bHasNoDataValueRef = FALSE;
            const double dfNoDataValueRef =
                poReferenceBand->GetNoDataValue(&bHasNoDataValueRef);
            if( !(bHasNoDataValueRef &&
                  dfNoDataValueRef >= 0 && dfNoDataValueRef <= 255 &&
                  dfNoDataValueRef == static_cast<int>(dfNoDataValueRef)) )
                bHasNoDataValueRef = FALSE;
            const int noDataValueRef =
                bHasNoDataValueRef ? static_cast<int>(dfNoDataValueRef) : 0;

            bool samePalette = false;

            if (pApproximateMatching)
                *pApproximateMatching = FALSE;

            if (nEntries == nRefEntries && bHasNoDataValueSrc == bHasNoDataValueRef &&
                (bHasNoDataValueSrc == FALSE || noDataValueSrc == noDataValueRef))
            {
                samePalette = true;
                for( int i = 0; i < nEntries; ++i )
                {
                    if (noDataValueSrc == i)
                        continue;
                    const GDALColorEntry* entry = srcColorTable->GetColorEntry(i);
                    const GDALColorEntry* entryRef = destColorTable->GetColorEntry(i);
                    if (entry->c1 != entryRef->c1 ||
                        entry->c2 != entryRef->c2 ||
                        entry->c3 != entryRef->c3)
                    {
                        samePalette = false;
                    }
                }
            }

            if( !samePalette )
            {
                if (pTranslationTable == nullptr)
                {
                    pTranslationTable = static_cast<unsigned char *>(
                        VSI_CALLOC_VERBOSE(1, std::max(256, nEntries)) );
                    if( pTranslationTable == nullptr )
                        return nullptr;
                }

                // Trying to remap the product palette on the subdataset
                // palette.
                for( int i = 0; i < nEntries; ++i )
                {
                    if( bHasNoDataValueSrc && bHasNoDataValueRef &&
                        noDataValueSrc == i )
                        continue;
                    const GDALColorEntry* entry =
                        srcColorTable->GetColorEntry(i);
                    bool bMatchFound = false;
                    for( int j = 0; j < nRefEntries; ++j )
                    {
                        if (bHasNoDataValueRef && noDataValueRef == j)
                            continue;
                        const GDALColorEntry* entryRef =
                            destColorTable->GetColorEntry(j);
                        if( entry->c1 == entryRef->c1 &&
                            entry->c2 == entryRef->c2 &&
                            entry->c3 == entryRef->c3 )
                        {
                            pTranslationTable[i] =
                                static_cast<unsigned char>(j);
                            bMatchFound = true;
                            break;
                        }
                    }
                    if( !bMatchFound )
                    {
                        // No exact match. Looking for closest color now.
                        int best_j = 0;
                        int best_distance = 0;
                        if( pApproximateMatching )
                            *pApproximateMatching = TRUE;
                        for( int j = 0; j < nRefEntries; ++j )
                        {
                            const GDALColorEntry* entryRef =
                                destColorTable->GetColorEntry(j);
                            int distance =
                                (entry->c1 - entryRef->c1) *
                                    (entry->c1 - entryRef->c1) +
                                (entry->c2 - entryRef->c2) *
                                    (entry->c2 - entryRef->c2) +
                                (entry->c3 - entryRef->c3) *
                                    (entry->c3 - entryRef->c3);
                            if( j == 0 || distance < best_distance )
                            {
                                best_j = j;
                                best_distance = distance;
                            }
                        }
                        pTranslationTable[i] =
                            static_cast<unsigned char>(best_j);
                    }
                }
                if( bHasNoDataValueRef && bHasNoDataValueSrc )
                    pTranslationTable[noDataValueSrc] =
                        static_cast<unsigned char>( noDataValueRef );

                return pTranslationTable;
            }
        }
    }
    return nullptr;
}

/************************************************************************/
/*                         SetFlushBlockErr()                           */
/************************************************************************/

/**
 * \brief Store that an error occurred while writing a dirty block.
 *
 * This function stores the fact that an error occurred while writing a dirty
 * block from GDALRasterBlock::FlushCacheBlock(). Indeed when dirty blocks are
 * flushed when the block cache get full, it is not convenient/possible to
 * report that a dirty block could not be written correctly. This function
 * remembers the error and re-issue it from GDALRasterBand::FlushCache(),
 * GDALRasterBand::WriteBlock() and GDALRasterBand::RasterIO(), which are
 * places where the user can easily match the error with the relevant dataset.
 */

void GDALRasterBand::SetFlushBlockErr( CPLErr eErr )
{
    eFlushBlockErr = eErr;
}

/************************************************************************/
/*                         IncDirtyBlocks()                             */
/************************************************************************/

/**
 * \brief Increment/decrement the number of dirty blocks
 */

void GDALRasterBand::IncDirtyBlocks( int nInc )
{
    if( poBandBlockCache )
        poBandBlockCache->IncDirtyBlocks(nInc);
}

/************************************************************************/
/*                            ReportError()                             */
/************************************************************************/

#ifndef DOXYGEN_XML
/**
 * \brief Emits an error related to a raster band.
 *
 * This function is a wrapper for regular CPLError(). The only difference
 * with CPLError() is that it prepends the error message with the dataset
 * name and the band number.
 *
 * @param eErrClass one of CE_Warning, CE_Failure or CE_Fatal.
 * @param err_no the error number (CPLE_*) from cpl_error.h.
 * @param fmt a printf() style format string.  Any additional arguments
 * will be treated as arguments to fill in this format in a manner
 * similar to printf().
 *
 * @since GDAL 1.9.0
 */

void GDALRasterBand::ReportError( CPLErr eErrClass, CPLErrorNum err_no,
                                  const char *fmt, ... )
{
    va_list args;

    va_start(args, fmt);

    char szNewFmt[256] = { '\0' };
    const char* pszDSName = poDS ? poDS->GetDescription() : "";
    if( strlen(fmt) + strlen(pszDSName) + 20 >= sizeof(szNewFmt) - 1 )
        pszDSName = CPLGetFilename(pszDSName);
    if( pszDSName[0] != '\0' && strchr(pszDSName, '%') == nullptr &&
        strlen(fmt) + strlen(pszDSName) + 20 < sizeof(szNewFmt) - 1 )
    {
        snprintf(szNewFmt, sizeof(szNewFmt), "%s, band %d: %s",
                 pszDSName, GetBand(), fmt);
        CPLErrorV( eErrClass, err_no, szNewFmt, args );
    }
    else
    {
        CPLErrorV( eErrClass, err_no, fmt, args );
    }
    va_end(args);
}
#endif

/************************************************************************/
/*                           GetVirtualMemAuto()                        */
/************************************************************************/

/** \brief Create a CPLVirtualMem object from a GDAL raster band object.
 *
 * Only supported on Linux and Unix systems with mmap() for now.
 *
 * This method allows creating a virtual memory object for a GDALRasterBand,
 * that exposes the whole image data as a virtual array.
 *
 * The default implementation relies on GDALRasterBandGetVirtualMem(), but specialized
 * implementation, such as for raw files, may also directly use mechanisms of the
 * operating system to create a view of the underlying file into virtual memory
 * ( CPLVirtualMemFileMapNew() )
 *
 * At the time of writing, the GeoTIFF driver and "raw" drivers (EHdr, ...) offer
 * a specialized implementation with direct file mapping, provided that some
 * requirements are met :
 *   - for all drivers, the dataset must be backed by a "real" file in the file
 *     system, and the byte ordering of multi-byte datatypes (Int16, etc.)
 *     must match the native ordering of the CPU.
 *   - in addition, for the GeoTIFF driver, the GeoTIFF file must be uncompressed, scanline
 *     oriented (i.e. not tiled). Strips must be organized in the file in sequential
 *     order, and be equally spaced (which is generally the case). Only power-of-two
 *     bit depths are supported (8 for GDT_Bye, 16 for GDT_Int16/GDT_UInt16,
 *     32 for GDT_Float32 and 64 for GDT_Float64)
 *
 * The pointer returned remains valid until CPLVirtualMemFree() is called.
 * CPLVirtualMemFree() must be called before the raster band object is destroyed.
 *
 * If p is such a pointer and base_type the type matching GDALGetRasterDataType(),
 * the element of image coordinates (x, y) can be accessed with
 * *(base_type*) ((GByte*)p + x * *pnPixelSpace + y * *pnLineSpace)
 *
 * This method is the same as the C GDALGetVirtualMemAuto() function.
 *
 * @param eRWFlag Either GF_Read to read the band, or GF_Write to
 * read/write the band.
 *
 * @param pnPixelSpace Output parameter giving the byte offset from the start of one pixel value in
 * the buffer to the start of the next pixel value within a scanline.
 *
 * @param pnLineSpace Output parameter giving the byte offset from the start of one scanline in
 * the buffer to the start of the next.
 *
 * @param papszOptions NULL terminated list of options.
 *                     If a specialized implementation exists, defining USE_DEFAULT_IMPLEMENTATION=YES
 *                     will cause the default implementation to be used.
 *                     On the contrary, starting with GDAL 2.2, defining USE_DEFAULT_IMPLEMENTATION=NO
 *                     will prevent the default implementation from being used (thus only allowing
 *                     efficient implementations to be used).
 *                     When requiring or falling back to the default implementation, the following
 *                     options are available : CACHE_SIZE (in bytes, defaults to 40 MB),
 *                     PAGE_SIZE_HINT (in bytes),
 *                     SINGLE_THREAD ("FALSE" / "TRUE", defaults to FALSE)
 *
 * @return a virtual memory object that must be unreferenced by CPLVirtualMemFree(),
 *         or NULL in case of failure.
 *
 * @since GDAL 1.11
 */

CPLVirtualMem  *GDALRasterBand::GetVirtualMemAuto( GDALRWFlag eRWFlag,
                                                   int *pnPixelSpace,
                                                   GIntBig *pnLineSpace,
                                                   char **papszOptions )
{
    const char* pszImpl = CSLFetchNameValueDef(
            papszOptions, "USE_DEFAULT_IMPLEMENTATION", "AUTO");
    if( EQUAL(pszImpl, "NO") || EQUAL(pszImpl, "OFF") ||
        EQUAL(pszImpl, "0") || EQUAL(pszImpl, "FALSE") )
    {
        return nullptr;
    }

    const int nPixelSpace = GDALGetDataTypeSizeBytes(eDataType);
    const GIntBig nLineSpace = static_cast<GIntBig>(nRasterXSize) * nPixelSpace;
    if( pnPixelSpace )
        *pnPixelSpace = nPixelSpace;
    if( pnLineSpace )
        *pnLineSpace = nLineSpace;
    const size_t nCacheSize = atoi(
        CSLFetchNameValueDef(papszOptions, "CACHE_SIZE", "40000000"));
    const size_t nPageSizeHint = atoi(
        CSLFetchNameValueDef(papszOptions, "PAGE_SIZE_HINT", "0") );
    const bool bSingleThreadUsage =
        CPLTestBool( CSLFetchNameValueDef( papszOptions,
                                           "SINGLE_THREAD", "FALSE" ) );
    return GDALRasterBandGetVirtualMem( GDALRasterBand::ToHandle(this),
                                        eRWFlag,
                                        0, 0, nRasterXSize, nRasterYSize,
                                        nRasterXSize, nRasterYSize,
                                        eDataType,
                                        nPixelSpace, nLineSpace,
                                        nCacheSize,
                                        nPageSizeHint,
                                        bSingleThreadUsage,
                                        papszOptions );
}

/************************************************************************/
/*                         GDALGetVirtualMemAuto()                      */
/************************************************************************/

/**
 * \brief Create a CPLVirtualMem object from a GDAL raster band object.
 *
 * @see GDALRasterBand::GetVirtualMemAuto()
 */

CPLVirtualMem * GDALGetVirtualMemAuto( GDALRasterBandH hBand,
                                       GDALRWFlag eRWFlag,
                                       int *pnPixelSpace,
                                       GIntBig *pnLineSpace,
                                       CSLConstList papszOptions )
{
    VALIDATE_POINTER1( hBand, "GDALGetVirtualMemAuto", nullptr );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);

    return poBand->GetVirtualMemAuto( eRWFlag, pnPixelSpace,
                                      pnLineSpace,
                                      const_cast<char**>(papszOptions) );
}

/************************************************************************/
/*                        GDALGetDataCoverageStatus()                   */
/************************************************************************/

/**
 * \brief Get the coverage status of a sub-window of the raster.
 *
 * Returns whether a sub-window of the raster contains only data, only empty
 * blocks or a mix of both. This function can be used to determine quickly
 * if it is worth issuing RasterIO / ReadBlock requests in datasets that may
 * be sparse.
 *
 * Empty blocks are blocks that are generally not physically present in the
 * file, and when read through GDAL, contain only pixels whose value is the
 * nodata value when it is set, or whose value is 0 when the nodata value is
 * not set.
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
 * @see GDALRasterBand::GetDataCoverageStatus()
 *
 * @param hBand raster band
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

int CPL_STDCALL GDALGetDataCoverageStatus( GDALRasterBandH hBand,
                               int nXOff, int nYOff,
                               int nXSize,
                               int nYSize,
                               int nMaskFlagStop,
                               double* pdfDataPct)
{
    VALIDATE_POINTER1( hBand, "GDALGetDataCoverageStatus",
                       GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED );

    GDALRasterBand *poBand = GDALRasterBand::FromHandle(hBand);

    return poBand->GetDataCoverageStatus( nXOff, nYOff, nXSize, nYSize,
                                          nMaskFlagStop, pdfDataPct );
}

/************************************************************************/
/*                          GetDataCoverageStatus()                     */
/************************************************************************/

/**
 * \fn GDALRasterBand::IGetDataCoverageStatus( int nXOff,
 *                                           int nYOff,
 *                                           int nXSize,
 *                                           int nYSize,
 *                                           int nMaskFlagStop,
 *                                           double* pdfDataPct)
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

int  GDALRasterBand::GetDataCoverageStatus( int nXOff,
                                            int nYOff,
                                            int nXSize,
                                            int nYSize,
                                            int nMaskFlagStop,
                                            double* pdfDataPct)
{
    if( nXOff < 0 || nYOff < 0 ||
        nXSize > INT_MAX - nXOff ||
        nYSize > INT_MAX - nYOff ||
        nXOff + nXSize > nRasterXSize ||
        nYOff + nYSize > nRasterYSize )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Bad window");
        if( pdfDataPct )
            *pdfDataPct = 0.0;
        return GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED |
               GDAL_DATA_COVERAGE_STATUS_EMPTY;
    }
    return IGetDataCoverageStatus(nXOff, nYOff, nXSize, nYSize,
                                  nMaskFlagStop, pdfDataPct);
}

/************************************************************************/
/*                         IGetDataCoverageStatus()                     */
/************************************************************************/

int  GDALRasterBand::IGetDataCoverageStatus( int /*nXOff*/,
                                             int /*nYOff*/,
                                             int /*nXSize*/,
                                             int /*nYSize*/,
                                             int /*nMaskFlagStop*/,
                                             double* pdfDataPct)
{
    if( pdfDataPct != nullptr )
        *pdfDataPct = 100.0;
    return GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED |
           GDAL_DATA_COVERAGE_STATUS_DATA;
}

//! @cond Doxygen_Suppress
/************************************************************************/
/*                          EnterReadWrite()                            */
/************************************************************************/

int GDALRasterBand::EnterReadWrite( GDALRWFlag eRWFlag )
{
    if( poDS != nullptr )
        return poDS->EnterReadWrite(eRWFlag);
    return FALSE;
}

/************************************************************************/
/*                         LeaveReadWrite()                             */
/************************************************************************/

void GDALRasterBand::LeaveReadWrite()
{
    if( poDS != nullptr )
        poDS->LeaveReadWrite();
}

/************************************************************************/
/*                           InitRWLock()                               */
/************************************************************************/

void GDALRasterBand::InitRWLock()
{
    if( poDS != nullptr )
        poDS->InitRWLock();
}

//! @endcond

/**
 * \fn GDALRasterBand::SetMetadata( char ** papszMetadata, const char * pszDomain)
 * \brief Set metadata.
 *
 * CAUTION: depending on the format, older values of the updated information might
 * still be found in the file in a "ghost" state, even if no longer accessible
 * through the GDAL API. This is for example the case of the GTiff format (this is
 * not a exhaustive list)
 *
 * The C function GDALSetMetadata() does the same thing as this method.
 *
 * @param papszMetadata the metadata in name=value string list format to
 * apply.
 * @param pszDomain the domain of interest.  Use "" or NULL for the default
 * domain.
 * @return CE_None on success, CE_Failure on failure and CE_Warning if the
 * metadata has been accepted, but is likely not maintained persistently
 * by the underlying object between sessions.
 */

/**
 * \fn GDALRasterBand::SetMetadataItem( const char * pszName, const char * pszValue, const char * pszDomain)
 * \brief Set single metadata item.
 *
 * CAUTION: depending on the format, older values of the updated information might
 * still be found in the file in a "ghost" state, even if no longer accessible
 * through the GDAL API. This is for example the case of the GTiff format (this is
 * not a exhaustive list)
 *
 * The C function GDALSetMetadataItem() does the same thing as this method.
 *
 * @param pszName the key for the metadata item to fetch.
 * @param pszValue the value to assign to the key.
 * @param pszDomain the domain to set within, use NULL for the default domain.
 *
 * @return CE_None on success, or an error code on failure.
 */

/************************************************************************/
/*                     GDALMDArrayFromRasterBand                        */
/************************************************************************/

class GDALMDArrayFromRasterBand final: public GDALMDArray
{
    CPL_DISALLOW_COPY_ASSIGN(GDALMDArrayFromRasterBand)

    GDALDataset* m_poDS;
    GDALRasterBand* m_poBand;
    GDALExtendedDataType m_dt;
    std::vector<std::shared_ptr<GDALDimension>> m_dims{};
    std::string m_osUnit;
    std::vector<GByte> m_pabyNoData{};
    std::shared_ptr<GDALMDArray> m_varX{};
    std::shared_ptr<GDALMDArray> m_varY{};
    std::string m_osFilename{};

    bool ReadWrite(GDALRWFlag eRWFlag,
                    const GUInt64* arrayStartIdx,
                    const size_t* count,
                    const GInt64* arrayStep,
                    const GPtrDiff_t* bufferStride,
                    const GDALExtendedDataType& bufferDataType,
                    void* pBuffer) const;

protected:
    GDALMDArrayFromRasterBand(GDALDataset* poDS,
                              GDALRasterBand* poBand):
        GDALAbstractMDArray( std::string(),
                             std::string(poDS->GetDescription()) +
                                CPLSPrintf(" band %d", poBand->GetBand()) ),
        GDALMDArray( std::string(),
                     std::string(poDS->GetDescription()) +
                        CPLSPrintf(" band %d", poBand->GetBand()) ),
        m_poDS(poDS),
        m_poBand(poBand),
        m_dt(GDALExtendedDataType::Create(poBand->GetRasterDataType())),
        m_osUnit( poBand->GetUnitType() ),
        m_osFilename(poDS->GetDescription())
    {
        m_poDS->Reference();

        int bHasNoData = false;
        if( m_poBand->GetRasterDataType() == GDT_Int64 )
        {
            const auto nNoData = m_poBand->GetNoDataValueAsInt64(&bHasNoData);
            if( bHasNoData )
            {
                m_pabyNoData.resize(m_dt.GetSize());
                GDALCopyWords(&nNoData, GDT_Int64, 0,
                              &m_pabyNoData[0], m_dt.GetNumericDataType(), 0,
                              1);
            }
        }
        else if( m_poBand->GetRasterDataType() == GDT_UInt64 )
        {
            const auto nNoData = m_poBand->GetNoDataValueAsUInt64(&bHasNoData);
            if( bHasNoData )
            {
                m_pabyNoData.resize(m_dt.GetSize());
                GDALCopyWords(&nNoData, GDT_UInt64, 0,
                              &m_pabyNoData[0], m_dt.GetNumericDataType(), 0,
                              1);
            }
        }
        else
        {
            const auto dfNoData = m_poBand->GetNoDataValue(&bHasNoData);
            if( bHasNoData )
            {
                m_pabyNoData.resize(m_dt.GetSize());
                GDALCopyWords(&dfNoData, GDT_Float64, 0,
                              &m_pabyNoData[0], m_dt.GetNumericDataType(), 0,
                              1);
            }
        }

        const int nXSize = poBand->GetXSize();
        const int nYSize = poBand->GetYSize();

        auto poSRS = m_poDS->GetSpatialRef();
        std::string osTypeY;
        std::string osTypeX;
        std::string osDirectionY;
        std::string osDirectionX;
        if( poSRS && poSRS->GetAxesCount() == 2 )
        {
            const auto mapping = poSRS->GetDataAxisToSRSAxisMapping();
            OGRAxisOrientation eOrientation1 = OAO_Other;
            poSRS->GetAxis(nullptr, 0,  &eOrientation1 );
            OGRAxisOrientation eOrientation2 = OAO_Other;
            poSRS->GetAxis(nullptr, 1,  &eOrientation2 );
            if( eOrientation1 == OAO_East && eOrientation2 == OAO_North )
            {
                if( mapping == std::vector<int>{1,2} )
                {
                    osTypeY = GDAL_DIM_TYPE_HORIZONTAL_Y;
                    osDirectionY = "NORTH";
                    osTypeX = GDAL_DIM_TYPE_HORIZONTAL_X;
                    osDirectionX = "EAST";
                }
            }
            else if( eOrientation1 == OAO_North && eOrientation2 == OAO_East )
            {
                if( mapping == std::vector<int>{2,1} )
                {
                    osTypeY = GDAL_DIM_TYPE_HORIZONTAL_Y;
                    osDirectionY = "NORTH";
                    osTypeX = GDAL_DIM_TYPE_HORIZONTAL_X;
                    osDirectionX = "EAST";
                }
            }
        }

        m_dims = {
            std::make_shared<GDALDimensionWeakIndexingVar>(
                "/", "Y", osTypeY, osDirectionY, nYSize),
            std::make_shared<GDALDimensionWeakIndexingVar>(
                "/", "X", osTypeX, osDirectionX, nXSize)
        };

        double adfGeoTransform[6];
        if( m_poDS->GetGeoTransform(adfGeoTransform) == CE_None &&
            adfGeoTransform[2] == 0 && adfGeoTransform[4] == 0 )
        {
            m_varX = std::make_shared<GDALMDArrayRegularlySpaced>(
                "/", "X", m_dims[1],
                adfGeoTransform[0],
                adfGeoTransform[1], 0.5);
            m_dims[1]->SetIndexingVariable(m_varX);

            m_varY = std::make_shared<GDALMDArrayRegularlySpaced>(
                "/", "Y", m_dims[0],
                adfGeoTransform[3],
                adfGeoTransform[5], 0.5);
            m_dims[0]->SetIndexingVariable(m_varY);
        }
    }

    bool IRead(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const override
    {
        return ReadWrite(GF_Read, arrayStartIdx, count, arrayStep, bufferStride,
                         bufferDataType, pDstBuffer);
    }

    bool IWrite(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      const void* pSrcBuffer) override
    {
        return ReadWrite(GF_Write, arrayStartIdx, count, arrayStep, bufferStride,
                         bufferDataType, const_cast<void*>(pSrcBuffer));
    }

public:
    ~GDALMDArrayFromRasterBand()
    {
        m_poDS->ReleaseRef();
    }

    static std::shared_ptr<GDALMDArray> Create(GDALDataset* poDS,
                                               GDALRasterBand* poBand)
    {
        auto array(std::shared_ptr<GDALMDArrayFromRasterBand>(
            new GDALMDArrayFromRasterBand(poDS, poBand)));
        array->SetSelf(array);
        return array;
    }

    bool IsWritable() const override { return m_poDS->GetAccess() == GA_Update; }

    const std::string& GetFilename() const override { return m_osFilename; }

    const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override { return m_dims; }

    const GDALExtendedDataType &GetDataType() const override { return m_dt; }

    const std::string& GetUnit() const override { return m_osUnit; }

    const void* GetRawNoDataValue() const override
    { return m_pabyNoData.empty() ? nullptr: m_pabyNoData.data(); }

    double GetOffset(bool* pbHasOffset, GDALDataType* peStorageType) const override
    {
        int bHasOffset = false;
        double dfRes = m_poBand->GetOffset(&bHasOffset);
        if( pbHasOffset )
            *pbHasOffset = CPL_TO_BOOL(bHasOffset);
        if( peStorageType )
            *peStorageType = GDT_Unknown;
        return dfRes;
    }

    double GetScale(bool* pbHasScale, GDALDataType* peStorageType) const override
    {
        int bHasScale = false;
        double dfRes = m_poBand->GetScale(&bHasScale);
        if( pbHasScale )
            *pbHasScale = CPL_TO_BOOL(bHasScale);
        if( peStorageType )
            *peStorageType = GDT_Unknown;
        return dfRes;
    }

    std::shared_ptr<OGRSpatialReference> GetSpatialRef() const override
    {
        auto poSrcSRS = m_poDS->GetSpatialRef();
        if( !poSrcSRS )
            return nullptr;
        auto poSRS = std::shared_ptr<OGRSpatialReference>(poSrcSRS->Clone());

        auto axisMapping = poSRS->GetDataAxisToSRSAxisMapping();
        constexpr int iYDim = 0;
        constexpr int iXDim = 1;
        for( auto& m: axisMapping )
        {
            if( m == 1 )
                m = iXDim + 1;
            else if( m == 2 )
                m = iYDim + 1;
            else
                m = 0;
        }
        poSRS->SetDataAxisToSRSAxisMapping(axisMapping);
        return poSRS;
    }

    std::vector<GUInt64> GetBlockSize() const override
    {
        int nBlockXSize = 0;
        int nBlockYSize = 0;
        m_poBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
        return std::vector<GUInt64>{ static_cast<GUInt64>(nBlockYSize),
                                     static_cast<GUInt64>(nBlockXSize) };
    }

    class MDIAsAttribute: public GDALAttribute
    {
        std::vector<std::shared_ptr<GDALDimension>> m_dims{};
        const GDALExtendedDataType m_dt = GDALExtendedDataType::CreateString();
        std::string m_osValue;

    public:
        MDIAsAttribute(const std::string& name, const std::string& value):
            GDALAbstractMDArray(std::string(), name),
            GDALAttribute(std::string(), name),
            m_osValue(value)
        {
        }

        const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override { return m_dims; }

        const GDALExtendedDataType &GetDataType() const override { return m_dt; }

        bool IRead(const GUInt64*, const size_t*,
                   const GInt64*, const GPtrDiff_t*,
                   const GDALExtendedDataType& bufferDataType,
                   void* pDstBuffer) const override
        {
            const char* pszStr = m_osValue.c_str();
            GDALExtendedDataType::CopyValue(&pszStr, m_dt,
                                            pDstBuffer, bufferDataType);
            return true;
        }
    };

    std::vector<std::shared_ptr<GDALAttribute>> GetAttributes(CSLConstList) const override
    {
        std::vector<std::shared_ptr<GDALAttribute>> res;
        auto papszMD = m_poBand->GetMetadata();
        for( auto iter = papszMD; iter && iter[0]; ++iter )
        {
            char* pszKey = nullptr;
            const char* pszValue = CPLParseNameValue(*iter, &pszKey);
            if( pszKey && pszValue )
            {
                res.emplace_back(std::make_shared<MDIAsAttribute>(pszKey, pszValue));
            }
            CPLFree(pszKey);
        }
        return res;
    }
};

/************************************************************************/
/*                            ReadWrite()                               */
/************************************************************************/

bool GDALMDArrayFromRasterBand::ReadWrite(GDALRWFlag eRWFlag,
                                          const GUInt64* arrayStartIdx,
                                          const size_t* count,
                                          const GInt64* arrayStep,
                                          const GPtrDiff_t* bufferStride,
                                          const GDALExtendedDataType& bufferDataType,
                                          void* pBuffer) const
{
    constexpr size_t iDimX = 1;
    constexpr size_t iDimY = 0;
    return GDALMDRasterIOFromBand(m_poBand, eRWFlag,
                                  iDimX,
                                  iDimY,
                                  arrayStartIdx,
                                  count,
                                  arrayStep,
                                  bufferStride,
                                  bufferDataType,
                                  pBuffer);
}

/************************************************************************/
/*                       GDALMDRasterIOFromBand()                       */
/************************************************************************/

bool GDALMDRasterIOFromBand(GDALRasterBand* poBand,
                            GDALRWFlag eRWFlag,
                            size_t iDimX,
                            size_t iDimY,
                            const GUInt64* arrayStartIdx,
                            const size_t* count,
                            const GInt64* arrayStep,
                            const GPtrDiff_t* bufferStride,
                            const GDALExtendedDataType& bufferDataType,
                            void* pBuffer)
{
    const auto eDT(bufferDataType.GetNumericDataType());
    const auto nDTSize(GDALGetDataTypeSizeBytes(eDT));
    const int nX = arrayStep[iDimX] > 0  ?
        static_cast<int>(arrayStartIdx[iDimX]) :
        static_cast<int>(arrayStartIdx[iDimX] - (count[iDimX]-1) * -arrayStep[iDimX]);
    const int nY = arrayStep[iDimY] > 0  ?
        static_cast<int>(arrayStartIdx[iDimY]) :
        static_cast<int>(arrayStartIdx[iDimY] - (count[iDimY]-1) * -arrayStep[iDimY]);
    const int nSizeX = static_cast<int>(count[iDimX] * ABS(arrayStep[iDimX]));
    const int nSizeY = static_cast<int>(count[iDimY] * ABS(arrayStep[iDimY]));
    GByte* pabyBuffer = static_cast<GByte*>(pBuffer);
    int nStrideXSign = 1;
    if( arrayStep[iDimX] < 0 )
    {
        pabyBuffer += (count[iDimX]-1) * bufferStride[iDimX] * nDTSize;
        nStrideXSign = -1;
    }
    int nStrideYSign = 1;
    if( arrayStep[iDimY] < 0 )
    {
        pabyBuffer += (count[iDimY]-1) * bufferStride[iDimY] * nDTSize;
        nStrideYSign = -1;
    }

    return poBand->RasterIO(eRWFlag,
            nX, nY, nSizeX, nSizeY,
            pabyBuffer,
            static_cast<int>(count[iDimX]),
            static_cast<int>(count[iDimY]),
            eDT,
            static_cast<GSpacing>(nStrideXSign * bufferStride[iDimX] * nDTSize),
            static_cast<GSpacing>(nStrideYSign * bufferStride[iDimY] * nDTSize),
            nullptr) == CE_None;
}

/************************************************************************/
/*                            AsMDArray()                               */
/************************************************************************/

/** Return a view of this raster band as a 2D multidimensional GDALMDArray.
 *
 * The band must be linked to a GDALDataset. If this dataset is not already
 * marked as shared, it will be, so that the returned array holds a reference
 * to it.
 *
 * If the dataset has a geotransform attached, the X and Y dimensions of the
 * returned array will have an associated indexing variable.
 *
 * This is the same as the C function GDALRasterBandAsMDArray().
 *
 * The "reverse" method is GDALMDArray::AsClassicDataset().
 *
 * @return a new array, or nullptr.
 *
 * @since GDAL 3.1
 */
std::shared_ptr<GDALMDArray> GDALRasterBand::AsMDArray() const
{
    if( !poDS )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Band not attached to a dataset");
        return nullptr;
    }
    if( !poDS->GetShared() )
    {
        poDS->MarkAsShared();
    }
    return GDALMDArrayFromRasterBand::Create(poDS,
                                             const_cast<GDALRasterBand*>(this));
}
