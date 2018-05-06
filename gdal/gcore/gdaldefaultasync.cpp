/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALDefaultAsyncReader and the
 *           GDALAsyncReader base class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Frank Warmerdam
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

#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "gdal.h"

CPL_CVSID("$Id$")

CPL_C_START
GDALAsyncReader *
GDALGetDefaultAsyncReader( GDALDataset* poDS,
                           int nXOff, int nYOff, int nXSize, int nYSize,
                           void *pBuf, int nBufXSize, int nBufYSize,
                           GDALDataType eBufType,
                           int nBandCount, int* panBandMap,
                           int nPixelSpace, int nLineSpace,
                           int nBandSpace, char **papszOptions );
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*                         GDALAsyncReader                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          GDALAsyncReader()                           */
/************************************************************************/

GDALAsyncReader::GDALAsyncReader() :
    poDS(nullptr),
    nXOff(0),
    nYOff(0),
    nXSize(0),
    nYSize(0),
    pBuf(nullptr),
    nBufXSize(0),
    nBufYSize(0),
    eBufType(GDT_Unknown),
    nBandCount(0),
    panBandMap(nullptr),
    nPixelSpace(0),
    nLineSpace(0),
    nBandSpace(0)
{
}

/************************************************************************/
/*                         ~GDALAsyncReader()                           */
/************************************************************************/
GDALAsyncReader::~GDALAsyncReader() = default;

/************************************************************************/
/*                        GetNextUpdatedRegion()                        */
/************************************************************************/

/**
 * \fn GDALAsyncStatusType GDALAsyncReader::GetNextUpdatedRegion( double dfTimeout, int* pnBufXOff, int* pnBufYOff, int* pnBufXSize, int* pnBufYSize) = 0;
 *
 * \brief Get async IO update
 *
 * Provide an opportunity for an asynchronous IO request to update the
 * image buffer and return an indication of the area of the buffer that
 * has been updated.
 *
 * The dfTimeout parameter can be used to wait for additional data to
 * become available.  The timeout does not limit the amount
 * of time this method may spend actually processing available data.
 *
 * The following return status are possible.
 * - GARIO_PENDING: No imagery was altered in the buffer, but there is still
 * activity pending, and the application should continue to call
 * GetNextUpdatedRegion() as time permits.
 * - GARIO_UPDATE: Some of the imagery has been updated, but there is still
 * activity pending.
 * - GARIO_ERROR: Something has gone wrong. The asynchronous request should
 * be ended.
 * - GARIO_COMPLETE: An update has occurred and there is no more pending work
 * on this request. The request should be ended and the buffer used.
 *
 * @param dfTimeout the number of seconds to wait for additional updates.  Use
 * -1 to wait indefinitely, or zero to not wait at all if there is no data
 * available.
 * @param pnBufXOff location to return the X offset of the area of the
 * request buffer that has been updated.
 * @param pnBufYOff location to return the Y offset of the area of the
 * request buffer that has been updated.
 * @param pnBufXSize location to return the X size of the area of the
 * request buffer that has been updated.
 * @param pnBufYSize location to return the Y size of the area of the
 * request buffer that has been updated.
 *
 * @return GARIO_ status, details described above.
 */

/************************************************************************/
/*                     GDALARGetNextUpdatedRegion()                     */
/************************************************************************/

/**
 * \brief Get async IO update
 *
 * Provide an opportunity for an asynchronous IO request to update the
 * image buffer and return an indication of the area of the buffer that
 * has been updated.
 *
 * The dfTimeout parameter can be used to wait for additional data to
 * become available.  The timeout does not limit the amount
 * of time this method may spend actually processing available data.
 *
 * The following return status are possible.
 * - GARIO_PENDING: No imagery was altered in the buffer, but there is still
 * activity pending, and the application should continue to call
 * GetNextUpdatedRegion() as time permits.
 * - GARIO_UPDATE: Some of the imagery has been updated, but there is still
 * activity pending.
 * - GARIO_ERROR: Something has gone wrong. The asynchronous request should
 * be ended.
 * - GARIO_COMPLETE: An update has occurred and there is no more pending work
 * on this request. The request should be ended and the buffer used.
 *
 * This is the same as GDALAsyncReader::GetNextUpdatedRegion()
 *
 * @param hARIO handle to the async reader.
 * @param dfTimeout the number of seconds to wait for additional updates.  Use
 * -1 to wait indefinitely, or zero to not wait at all if there is no data
 * available.
 * @param pnBufXOff location to return the X offset of the area of the
 * request buffer that has been updated.
 * @param pnBufYOff location to return the Y offset of the area of the
 * request buffer that has been updated.
 * @param pnBufXSize location to return the X size of the area of the
 * request buffer that has been updated.
 * @param pnBufYSize location to return the Y size of the area of the
 * request buffer that has been updated.
 *
 * @return GARIO_ status, details described above.
 */

GDALAsyncStatusType CPL_STDCALL
GDALARGetNextUpdatedRegion(GDALAsyncReaderH hARIO, double dfTimeout,
                           int* pnBufXOff, int* pnBufYOff,
                           int* pnBufXSize, int* pnBufYSize)
{
    VALIDATE_POINTER1(hARIO, "GDALARGetNextUpdatedRegion", GARIO_ERROR);
    return static_cast<GDALAsyncReader *>(hARIO)->GetNextUpdatedRegion(
        dfTimeout, pnBufXOff, pnBufYOff, pnBufXSize, pnBufYSize);
}

/************************************************************************/
/*                             LockBuffer()                             */
/************************************************************************/

/**
 * \fn GDALAsyncReader::LockBuffer(double)
 * \brief Lock image buffer.
 *
 * Locks the image buffer passed into GDALDataset::BeginAsyncReader().
 * This is useful to ensure the image buffer is not being modified while
 * it is being used by the application.  UnlockBuffer() should be used
 * to release this lock when it is no longer needed.
 *
 * @param dfTimeout the time in seconds to wait attempting to lock the buffer.
 * -1.0 to wait indefinitely and 0 to not wait at all if it can't be
 * acquired immediately.  Default is -1.0 (infinite wait).
 *
 * @return TRUE if successful, or FALSE on an error.
 */

/**/
/**/

int GDALAsyncReader::LockBuffer( double /* dfTimeout */ )
{
    return TRUE;
}

/************************************************************************/
/*                          GDALARLockBuffer()                          */
/************************************************************************/

/**
 * \brief Lock image buffer.
 *
 * Locks the image buffer passed into GDALDataset::BeginAsyncReader().
 * This is useful to ensure the image buffer is not being modified while
 * it is being used by the application.  UnlockBuffer() should be used
 * to release this lock when it is no longer needed.
 *
 * This is the same as GDALAsyncReader::LockBuffer()
 *
 * @param hARIO handle to async reader.
 * @param dfTimeout the time in seconds to wait attempting to lock the buffer.
 * -1.0 to wait indefinitely and 0 to not wait at all if it can't be
 * acquired immediately.  Default is -1.0 (infinite wait).
 *
 * @return TRUE if successful, or FALSE on an error.
 */

int CPL_STDCALL GDALARLockBuffer(GDALAsyncReaderH hARIO, double dfTimeout )
{
    VALIDATE_POINTER1(hARIO, "GDALARLockBuffer",FALSE);
    return static_cast<GDALAsyncReader *>(hARIO)->LockBuffer( dfTimeout );
}

/************************************************************************/
/*                            UnlockBuffer()                            */
/************************************************************************/

/**
 * \brief Unlock image buffer.
 *
 * Releases a lock on the image buffer previously taken with LockBuffer().
 */

void GDALAsyncReader::UnlockBuffer()

{
}

/************************************************************************/
/*                          GDALARUnlockBuffer()                        */
/************************************************************************/

/**
 * \brief Unlock image buffer.
 *
 * Releases a lock on the image buffer previously taken with LockBuffer().
 *
 * This is the same as GDALAsyncReader::UnlockBuffer()
 *
 * @param hARIO handle to async reader.
 */

void CPL_STDCALL GDALARUnlockBuffer(GDALAsyncReaderH hARIO)
{
    VALIDATE_POINTER0(hARIO, "GDALARUnlockBuffer");
    static_cast<GDALAsyncReader *>(hARIO)->UnlockBuffer();
}

/************************************************************************/
/* ==================================================================== */
/*                     GDALDefaultAsyncReader                           */
/* ==================================================================== */
/************************************************************************/

class GDALDefaultAsyncReader : public GDALAsyncReader
{
  private:
    char **papszOptions = nullptr;

    CPL_DISALLOW_COPY_ASSIGN(GDALDefaultAsyncReader)

  public:
    GDALDefaultAsyncReader(GDALDataset* poDS,
                             int nXOff, int nYOff,
                             int nXSize, int nYSize,
                             void *pBuf,
                             int nBufXSize, int nBufYSize,
                             GDALDataType eBufType,
                             int nBandCount, int* panBandMap,
                             int nPixelSpace, int nLineSpace,
                             int nBandSpace, char **papszOptions);
    ~GDALDefaultAsyncReader() override;

    GDALAsyncStatusType GetNextUpdatedRegion(double dfTimeout,
                                             int* pnBufXOff,
                                             int* pnBufYOff,
                                             int* pnBufXSize,
                                             int* pnBufYSize) override;
};

/************************************************************************/
/*                     GDALGetDefaultAsyncReader()                      */
/************************************************************************/

GDALAsyncReader *
GDALGetDefaultAsyncReader( GDALDataset* poDS,
                             int nXOff, int nYOff,
                             int nXSize, int nYSize,
                             void *pBuf,
                             int nBufXSize, int nBufYSize,
                             GDALDataType eBufType,
                             int nBandCount, int* panBandMap,
                             int nPixelSpace, int nLineSpace,
                             int nBandSpace, char **papszOptions)

{
    return new GDALDefaultAsyncReader( poDS,
                                         nXOff, nYOff, nXSize, nYSize,
                                         pBuf, nBufXSize, nBufYSize, eBufType,
                                         nBandCount, panBandMap,
                                         nPixelSpace, nLineSpace, nBandSpace,
                                         papszOptions );
}

/************************************************************************/
/*                       GDALDefaultAsyncReader()                       */
/************************************************************************/

GDALDefaultAsyncReader::
GDALDefaultAsyncReader( GDALDataset* poDSIn,
                          int nXOffIn, int nYOffIn,
                          int nXSizeIn, int nYSizeIn,
                          void *pBufIn,
                          int nBufXSizeIn, int nBufYSizeIn,
                          GDALDataType eBufTypeIn,
                          int nBandCountIn, int* panBandMapIn,
                          int nPixelSpaceIn, int nLineSpaceIn,
                          int nBandSpaceIn, char **papszOptionsIn)

{
    poDS = poDSIn;
    nXOff = nXOffIn;
    nYOff = nYOffIn;
    nXSize = nXSizeIn;
    nYSize = nYSizeIn;
    pBuf = pBufIn;
    nBufXSize = nBufXSizeIn;
    nBufYSize = nBufYSizeIn;
    eBufType = eBufTypeIn;
    nBandCount = nBandCountIn;
    panBandMap = static_cast<int*>(CPLMalloc(sizeof(int)*nBandCountIn));

    if( panBandMapIn != nullptr )
        memcpy( panBandMap, panBandMapIn, sizeof(int)*nBandCount );
    else
    {
        for( int i = 0; i < nBandCount; i++ )
            panBandMap[i] = i+1;
    }

    nPixelSpace = nPixelSpaceIn;
    nLineSpace = nLineSpaceIn;
    nBandSpace = nBandSpaceIn;

    papszOptions = CSLDuplicate(papszOptionsIn);
}

/************************************************************************/
/*                      ~GDALDefaultAsyncReader()                       */
/************************************************************************/

GDALDefaultAsyncReader::~GDALDefaultAsyncReader()

{
    CPLFree( panBandMap );
    CSLDestroy( papszOptions );
}

/************************************************************************/
/*                        GetNextUpdatedRegion()                        */
/************************************************************************/

GDALAsyncStatusType
GDALDefaultAsyncReader::GetNextUpdatedRegion( double /*dfTimeout*/,
                                              int* pnBufXOff,
                                              int* pnBufYOff,
                                              int* pnBufXSize,
                                              int* pnBufYSize )
{
    CPLErr eErr;

    eErr = poDS->RasterIO( GF_Read, nXOff, nYOff, nXSize, nYSize,
                           pBuf, nBufXSize, nBufYSize, eBufType,
                           nBandCount, panBandMap,
                           nPixelSpace, nLineSpace, nBandSpace,
                           nullptr );

    *pnBufXOff = 0;
    *pnBufYOff = 0;
    *pnBufXSize = nBufXSize;
    *pnBufYSize = nBufYSize;

    if( eErr == CE_None )
        return GARIO_COMPLETE;
    else
        return GARIO_ERROR;
}
