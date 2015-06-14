/******************************************************************************
 * $Id: ecwdataset.cpp 21486 2011-01-13 17:38:17Z warmerdam $
 *
 * Project:  GDAL 
 * Purpose:  ECWAsyncReader implementation
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdal_ecw.h"

CPL_CVSID("$Id: ecwdataset.cpp 21486 2011-01-13 17:38:17Z warmerdam $");

#if defined(FRMT_ecw) && (ECWSDK_VERSION >= 40)

/************************************************************************/
/*                          BeginAsyncReader()                          */
/************************************************************************/

GDALAsyncReader* 
ECWDataset::BeginAsyncReader( int nXOff, int nYOff, int nXSize, int nYSize, 
                              void *pBuf, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType,
                              int nBandCount, int* panBandMap,
                              int nPixelSpace, int nLineSpace, int nBandSpace,
                              char **papszOptions)

{
    int   i;

/* -------------------------------------------------------------------- */
/*      Provide default packing if needed.                              */
/* -------------------------------------------------------------------- */
    if( nPixelSpace == 0 )
        nPixelSpace = GDALGetDataTypeSize(eBufType) / 8;
    if( nLineSpace == 0 )
        nLineSpace = nPixelSpace * nBufXSize;
    if( nBandSpace == 0 )
        nBandSpace = nLineSpace * nBufYSize;
    
/* -------------------------------------------------------------------- */
/*      Do a bit of validation.                                         */
/* -------------------------------------------------------------------- */
    if( nXSize < 1 || nYSize < 1 || nBufXSize < 1 || nBufYSize < 1 )
    {
        CPLDebug( "GDAL", 
                  "BeginAsyncReader() skipped for odd window or buffer size.\n"
                  "  Window = (%d,%d)x%dx%d\n"
                  "  Buffer = %dx%d\n",
                  nXOff, nYOff, nXSize, nYSize, 
                  nBufXSize, nBufYSize );
        return NULL;
    }

    if( nXOff < 0 || nXOff > INT_MAX - nXSize || nXOff + nXSize > nRasterXSize
        || nYOff < 0 || nYOff > INT_MAX - nYSize || nYOff + nYSize > nRasterYSize )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                  "Access window out of range in RasterIO().  Requested\n"
                  "(%d,%d) of size %dx%d on raster of %dx%d.",
                  nXOff, nYOff, nXSize, nYSize, nRasterXSize, nRasterYSize );
        return NULL;
    }

    if( nBandCount <= 0 || nBandCount > nBands )
    {
        ReportError( CE_Failure, CPLE_IllegalArg, "Invalid band count" );
        return NULL;
    }

    if( panBandMap != NULL )
    {
        for( i = 0; i < nBandCount; i++ )
        {
            if( panBandMap[i] < 1 || panBandMap[i] > nBands )
            {
                ReportError( CE_Failure, CPLE_IllegalArg,
                      "panBandMap[%d] = %d, this band does not exist on dataset.",
                      i, panBandMap[i] );
                return NULL;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Create the corresponding async reader.                          */
/* -------------------------------------------------------------------- */
    ECWAsyncReader *poReader = new ECWAsyncReader();

    poReader->poDS = this;

    poReader->nXOff = nXOff;
    poReader->nYOff = nYOff;
    poReader->nXSize = nXSize;
    poReader->nYSize = nYSize;

    poReader->pBuf = pBuf;
    poReader->nBufXSize = nBufXSize;
    poReader->nBufYSize = nBufYSize;
    poReader->eBufType = eBufType;
    poReader->nBandCount = nBandCount;
    poReader->panBandMap = (int *) CPLCalloc(sizeof(int),nBandCount);
    if( panBandMap != NULL )
    {
        memcpy( poReader->panBandMap, panBandMap, sizeof(int) * nBandCount );
    }
    else
    {
        for( i = 0; i < nBandCount; i++ )
            poReader->panBandMap[i] = i + 1;
    }

    poReader->nPixelSpace = nPixelSpace;
    poReader->nLineSpace = nLineSpace;
    poReader->nBandSpace = nBandSpace;

/* -------------------------------------------------------------------- */
/*      Create a new view for this request.                             */
/* -------------------------------------------------------------------- */
    poReader->poFileView = OpenFileView( GetDescription(), true, 
                                         poReader->bUsingCustomStream );

    if( poReader->poFileView == NULL )
    {
        delete poReader;
        return NULL;
    }

    poReader->poFileView->SetClientData( poReader );
    poReader->poFileView->SetRefreshCallback( ECWAsyncReader::RefreshCB );

/* -------------------------------------------------------------------- */
/*      Issue a corresponding SetView command.                          */
/* -------------------------------------------------------------------- */
    std::vector<UINT32> anBandIndices;
    NCSError     eNCSErr;
    CNCSError    oErr;
    
    for( i = 0; i < nBandCount; i++ )
        anBandIndices.push_back( panBandMap[i] - 1 );

    oErr = poReader->poFileView->SetView( nBandCount, &(anBandIndices[0]),
                                          nXOff, nYOff, 
                                          nXOff + nXSize - 1, 
                                          nYOff + nYSize - 1,
                                          nBufXSize, nBufYSize );
    eNCSErr = oErr.GetErrorNumber();
    
    if( eNCSErr != NCS_SUCCESS )
    {
        delete poReader;
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", NCSGetErrorText(eNCSErr) );
        
        return NULL;
    }

    return poReader;
}

/************************************************************************/
/*                           EndAsyncReader()                           */
/************************************************************************/
void ECWDataset::EndAsyncReader(GDALAsyncReader *poReader)

{
    delete poReader;
}

/************************************************************************/
/*                           ECWAsyncReader()                           */
/************************************************************************/

ECWAsyncReader::ECWAsyncReader()

{
    hMutex = CPLCreateMutex();
    CPLReleaseMutex( hMutex );

    poFileView = NULL;
    bUpdateReady = FALSE;
    bComplete = FALSE;
    panBandMap = NULL;
}

/************************************************************************/
/*                          ~ECWAsyncReader()                           */
/************************************************************************/

ECWAsyncReader::~ECWAsyncReader()

{
    {
        CPLMutexHolderD( &hMutex );

        // cancel? 

        delete poFileView;
        // we should also consider cleaning up the io stream if needed.
    }

    CPLFree(panBandMap);
    panBandMap = NULL;

    CPLDestroyMutex( hMutex );
    hMutex = NULL;
}

/************************************************************************/
/*                             RefreshCB()                              */
/*                                                                      */
/*      This static method is called by the ECW SDK to notify us        */
/*      that there is new data ready to refresh from.  We just mark     */
/*      the async reader as ready for an update.  We fetch the data     */
/*      and push into into the buffer the application uses.  We lock    */
/*      this async reader's mutex for this whole operation to avoid     */
/*      a conflict with the main application.                           */
/************************************************************************/

NCSEcwReadStatus ECWAsyncReader::RefreshCB( NCSFileView *pFileView )

{
    NCSFileViewSetInfo *psVSI = NULL;

    NCScbmGetViewInfo( pFileView, &psVSI );
    if( psVSI != NULL )
    {
        CPLDebug( "ECW", "RefreshCB(): BlockCounts=%d/%d/%d/%d", 
                  psVSI->nBlocksAvailableAtSetView,
                  psVSI->nBlocksAvailable,
                  psVSI->nMissedBlocksDuringRead,
                  psVSI->nBlocksInView );
    }

/* -------------------------------------------------------------------- */
/*      Identify the reader we are responding on behalf of.             */
/* -------------------------------------------------------------------- */
    CNCSJP2FileView *poFileView = (CNCSJP2FileView *) pFileView;
    ECWAsyncReader *poReader = (ECWAsyncReader *)poFileView->GetClientData();
    
/* -------------------------------------------------------------------- */
/*      Acquire the async reader mutex.  Currently we make no           */
/*      arrangements for failure to acquire it.                         */
/* -------------------------------------------------------------------- */
    {
        CPLMutexHolderD( &(poReader->hMutex) );

/* -------------------------------------------------------------------- */
/*      Mark the buffer as updated unless we are already complete.      */
/*      It seems the Update callback keeps getting called even when     */
/*      no new data has arrived after completion so we don't want to    */
/*      trigger new work elsewhere in that case.                        */
/*                                                                      */
/*      Also record whether we are now complete.                        */
/* -------------------------------------------------------------------- */
        if( !poReader->bComplete )
            poReader->bUpdateReady = TRUE;

        if( psVSI->nBlocksAvailable == psVSI->nBlocksInView )
            poReader->bComplete = TRUE;
    }

    /* Call CPLCleanupTLS explicitly since this thread isn't managed */
    /* by CPL. This will free the resources taken by the above CPLDebug */
    if( poReader->bComplete )
        CPLCleanupTLS();

    return NCSECW_READ_OK;
}

/************************************************************************/
/*                            ReadToBuffer()                            */
/************************************************************************/
NCSEcwReadStatus ECWAsyncReader::ReadToBuffer()
{
/* -------------------------------------------------------------------- */
/*      Setup working scanline, and the pointers into it.               */
/*                                                                      */
/*      Should we try and optimize some cases that we could read        */
/*      directly into the application buffer?  Perhaps in the           */
/*      future.                                                         */
/* -------------------------------------------------------------------- */
    ECWDataset *poECWDS = (ECWDataset *) poDS;
    int i;
    int nDataTypeSize = (GDALGetDataTypeSize(poECWDS->eRasterDataType) / 8);
    GByte *pabyBILScanline = (GByte *) 
        CPLMalloc(nBufXSize * nDataTypeSize * nBandCount);
    GByte **papabyBIL = (GByte**)CPLMalloc(nBandCount*sizeof(void*));

    for( i = 0; i < nBandCount; i++ )
        papabyBIL[i] = pabyBILScanline 
            + i * nBufXSize * nDataTypeSize;

/* -------------------------------------------------------------------- */
/*      Read back the imagery into the buffer.                          */
/* -------------------------------------------------------------------- */ 
    for( int iScanline = 0; iScanline < nBufYSize; iScanline++ )
    {
        NCSEcwReadStatus  eRStatus;

        eRStatus = 
            poFileView->ReadLineBIL( poECWDS->eNCSRequestDataType, 
                                               (UINT16) nBandCount,
                                               (void **) papabyBIL );
        if( eRStatus != NCSECW_READ_OK )
        {
            CPLFree( papabyBIL );
            CPLFree( pabyBILScanline );
            CPLError( CE_Failure, CPLE_AppDefined,
                      "NCScbmReadViewLineBIL failed." );
            return eRStatus;
        }

        for( i = 0; i < nBandCount; i++ )
        {
            GDALCopyWords( 
                pabyBILScanline + i * nDataTypeSize * nBufXSize,
                poECWDS->eRasterDataType, nDataTypeSize, 
                ((GByte *) pBuf) 
                + nLineSpace * iScanline 
                + nBandSpace * i, 
                eBufType, 
                nPixelSpace, 
                nBufXSize );
        }
    }

    CPLFree( pabyBILScanline );
    CPLFree( papabyBIL );

    return NCSECW_READ_OK;
}

/************************************************************************/
/*                        GetNextUpdatedRegion()                        */
/************************************************************************/

GDALAsyncStatusType 
ECWAsyncReader::GetNextUpdatedRegion( double dfTimeout,
                                      int* pnXBufOff, int* pnYBufOff,
                                      int* pnXBufSize, int* pnYBufSize )

{
    CPLDebug( "ECW", "GetNextUpdatedRegion()" );

/* -------------------------------------------------------------------- */
/*      We always mark the whole raster as updated since the ECW SDK    */
/*      does not have a concept of partial update notifications.        */
/* -------------------------------------------------------------------- */
    *pnXBufOff = 0;
    *pnYBufOff = 0;
    *pnXBufSize = nBufXSize;
    *pnYBufSize = nBufYSize;

    if( bComplete && !bUpdateReady )
    {
        CPLDebug( "ECW", "return GARIO_COMPLETE" );
        return GARIO_COMPLETE;
    }
        
/* -------------------------------------------------------------------- */
/*      Wait till our timeout, or until we are notified there is        */
/*      data ready.  We are trusting the CPLSleep() to be pretty        */
/*      accurate instead of keeping track of time elapsed ourselves     */
/*      - this is not necessarily a good approach.                      */
/* -------------------------------------------------------------------- */
    if( dfTimeout < 0.0 )
        dfTimeout = 100000.0;
    
    while( !bUpdateReady && dfTimeout > 0.0 )
    {
        CPLSleep( MIN(0.1, dfTimeout) );
        dfTimeout -= 0.1;
        CPLDebug( "ECW", "wait..." );
    }

    if( !bUpdateReady )
    {
        CPLDebug( "ECW", "return GARIO_PENDING" );
        return GARIO_PENDING;
    }

    bUpdateReady = FALSE;

/* -------------------------------------------------------------------- */
/*      Acquire Mutex                                                   */
/* -------------------------------------------------------------------- */
    if( !CPLAcquireMutex( hMutex, dfTimeout ) )
    {
        CPLDebug( "ECW", "return GARIO_PENDING" );
        return GARIO_PENDING;
    }

/* -------------------------------------------------------------------- */
/*      Actually decode the imagery into our buffer.                    */
/* -------------------------------------------------------------------- */
    NCSEcwReadStatus  eRStatus = ReadToBuffer();

    if( eRStatus != NCSECW_READ_OK )
    {
        CPLReleaseMutex( hMutex );
        return GARIO_ERROR;
    }

/* -------------------------------------------------------------------- */
/*      Return indication of complete or just buffer updateded.         */
/* -------------------------------------------------------------------- */

    if( bComplete && !bUpdateReady )
    {
        CPLReleaseMutex( hMutex );
        CPLDebug( "ECW", "return GARIO_COMPLETE" );
        return GARIO_COMPLETE;
    }
    else
    {
        CPLReleaseMutex( hMutex );
        CPLDebug( "ECW", "return GARIO_UPDATE" );
        return GARIO_UPDATE;
    }
}

#endif /* def FRMT_ecw */
