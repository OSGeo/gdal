/******************************************************************************
 * $Id: gdaldataset.cpp 16796 2009-04-17 23:35:04Z normanb $
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

#include "gdal_priv.h"

CPL_CVSID("$Id: gdaldataset.cpp 16796 2009-04-17 23:35:04Z normanb $");

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

GDALAsyncReader::GDALAsyncReader()
{
}

/************************************************************************/
/*                         ~GDALAsyncReader()                           */
/************************************************************************/
GDALAsyncReader::~GDALAsyncReader()
{
}

/************************************************************************/
/*                        GetNextUpdatedRegion()                        */
/************************************************************************/

/**
 * \fn GDALAsyncStatusType GDALAsyncReader::GetNextUpdatedRegion( double dfTimeout, int* pnBufXOff, int* pnBufYOff, int* pnBufXSize, int* pnBufXSize) = 0;
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
 * - GARIO_COMPLETE: An update has occured and there is no more pending work 
 * on this request. The request should be ended and the buffer used. 
 *
 * @param dfTimeout the number of seconds to wait for additional updates.  Use 
 * -1 to wait indefinately, or zero to not wait at all if there is no data
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

GDALAsyncStatusType CPL_STDCALL 
GDALARGetNextUpdatedRegion(GDALAsyncReaderH hARIO, double timeout,
                           int* pnxbufoff, int* pnybufoff, 
                           int* pnxbufsize, int* pnybufsize)
{
    VALIDATE_POINTER1(hARIO, "GDALARGetNextUpdatedRegion", GARIO_ERROR);
    return ((GDALAsyncReader *)hARIO)->GetNextUpdatedRegion(
        timeout, pnxbufoff, pnybufoff, pnxbufsize, pnybufsize);
}

/************************************************************************/
/*                             LockBuffer()                             */
/************************************************************************/

/**
 * \brief Lock image buffer.
 *
 * Locks the image buffer passed into GDALDataset::BeginAsyncReader(). 
 * This is useful to ensure the image buffer is not being modified while
 * it is being used by the application.  UnlockBuffer() should be used
 * to release this lock when it is no longer needed.
 *
 * @param dfTimeout the time in seconds to wait attempting to lock the buffer.
 * -1.0 to wait indefinately and 0 to not wait at all if it can't be
 * acquired immediately.  Default is -1.0 (infinite wait).
 *
 * @return TRUE if successful, or FALSE on an error.
 */

int GDALAsyncReader::LockBuffer( CPL_UNUSED double dfTimeout )
{
    return TRUE;
}


/************************************************************************/
/*                          GDALARLockBuffer()                          */
/************************************************************************/
int CPL_STDCALL GDALARLockBuffer(GDALAsyncReaderH hARIO, double dfTimeout )
{
    VALIDATE_POINTER1(hARIO, "GDALARLockBuffer",FALSE);
    return ((GDALAsyncReader *)hARIO)->LockBuffer( dfTimeout );
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
/*                          GDALARUnlockBuffer()                          */
/************************************************************************/
void CPL_STDCALL GDALARUnlockBuffer(GDALAsyncReaderH hARIO)
{
    VALIDATE_POINTER0(hARIO, "GDALARUnlockBuffer");
    ((GDALAsyncReader *)hARIO)->UnlockBuffer();
}

/************************************************************************/
/* ==================================================================== */
/*                     GDALDefaultAsyncReader                           */
/* ==================================================================== */
/************************************************************************/

class GDALDefaultAsyncReader : public GDALAsyncReader
{
  private:
    char **         papszOptions;

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
    ~GDALDefaultAsyncReader();

    virtual GDALAsyncStatusType GetNextUpdatedRegion(double dfTimeout,
                                                     int* pnBufXOff,
                                                     int* pnBufYOff,
                                                     int* pnBufXSize,
                                                     int* pnBufYSize);
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
GDALDefaultAsyncReader( GDALDataset* poDS,
                          int nXOff, int nYOff,
                          int nXSize, int nYSize,
                          void *pBuf,
                          int nBufXSize, int nBufYSize,
                          GDALDataType eBufType,
                          int nBandCount, int* panBandMap,
                          int nPixelSpace, int nLineSpace,
                          int nBandSpace, char **papszOptions) 

{
    this->poDS = poDS;
    this->nXOff = nXOff;
    this->nYOff = nYOff;
    this->nXSize = nXSize;
    this->nYSize = nYSize;
    this->pBuf = pBuf;
    this->nBufXSize = nBufXSize;
    this->nBufYSize = nBufYSize;
    this->eBufType = eBufType;
    this->nBandCount = nBandCount;
    this->panBandMap = (int *) CPLMalloc(sizeof(int)*nBandCount);

    if( panBandMap != NULL )
        memcpy( this->panBandMap, panBandMap, sizeof(int)*nBandCount );
    else
    {
        for( int i = 0; i < nBandCount; i++ )
            this->panBandMap[i] = i+1;
    }
    
    this->nPixelSpace = nPixelSpace;
    this->nLineSpace = nLineSpace;
    this->nBandSpace = nBandSpace;

    this->papszOptions = CSLDuplicate(papszOptions);
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
GDALDefaultAsyncReader::GetNextUpdatedRegion(CPL_UNUSED double dfTimeout,
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
                           NULL );

    *pnBufXOff = 0;
    *pnBufYOff = 0;
    *pnBufXSize = nBufXSize;
    *pnBufYSize = nBufYSize;

    if( eErr == CE_None )
        return GARIO_COMPLETE;
    else
        return GARIO_ERROR;
}
