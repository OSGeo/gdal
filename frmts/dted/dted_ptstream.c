/******************************************************************************
 * $Id$
 *
 * Project:  DTED Translator
 * Purpose:  DTED Point Stream Writer.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
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

#include "dted_api.h"

CPL_CVSID("$Id$")

typedef struct {
    char     *pszFilename;
    DTEDInfo *psInfo;

    GInt16 **papanProfiles;

    int    nLLLong;
    int    nLLLat;
} DTEDCachedFile;

typedef struct {
    int nLevel;
    char *pszPath;

    double  dfPixelSize;

    int nOpenFiles;
    DTEDCachedFile *pasCF;

    int nLastFile;

    char *apszMetadata[DTEDMD_MAX+1];
} DTEDPtStream;

/************************************************************************/
/*                         DTEDCreatePtStream()                         */
/************************************************************************/

void *DTEDCreatePtStream( const char *pszPath, int nLevel )

{
    DTEDPtStream *psStream;
    int          i;
    VSIStatBuf   sStat;

/* -------------------------------------------------------------------- */
/*      Does the target directory already exist?  If not try to         */
/*      create it.                                                      */
/* -------------------------------------------------------------------- */
    if( CPLStat( pszPath, &sStat ) != 0 )
    {
        if( VSIMkdir( pszPath, 0755 ) != 0 )
        {
#ifndef AVOID_CPL
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Unable to find, or create directory `%s'.",
                      pszPath );
#endif
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Create the stream and initialize it.                            */
/* -------------------------------------------------------------------- */

    psStream = (DTEDPtStream *) CPLCalloc( sizeof(DTEDPtStream), 1 );
    psStream->nLevel = nLevel;
    psStream->pszPath = CPLStrdup( pszPath );
    psStream->nOpenFiles = 0;
    psStream->pasCF = NULL;
    psStream->nLastFile = -1;

    for( i = 0; i < DTEDMD_MAX+1; i++ )
        psStream->apszMetadata[i] = NULL;

    if( nLevel == 0 )
        psStream->dfPixelSize = 1.0 / 120.0;
    else if( nLevel == 1 )
        psStream->dfPixelSize = 1.0 / 1200.0;
    else /* if( nLevel == 2 ) */
        psStream->dfPixelSize = 1.0 / 3600.0;

    return (void *) psStream;
}

/************************************************************************/
/*                        DTEDPtStreamNewTile()                         */
/*                                                                      */
/*      Create a new DTED file file, add it to our list, and make it    */
/*      "current".                                                      */
/************************************************************************/

static int DTEDPtStreamNewTile( DTEDPtStream *psStream,
                                int nCrLong, int nCrLat )

{
    DTEDInfo        *psInfo;
    char            szFile[128];
    char            chNSHemi, chEWHemi;
    char            *pszFullFilename;
    const char      *pszError;

    /* work out filename */
    if( nCrLat < 0 )
        chNSHemi = 's';
    else
        chNSHemi = 'n';

    if( nCrLong < 0 )
        chEWHemi = 'w';
    else
        chEWHemi = 'e';

    snprintf( szFile, sizeof(szFile), "%c%03d%c%03d.dt%d",
             chEWHemi, ABS(nCrLong), chNSHemi, ABS(nCrLat),
             psStream->nLevel );

    pszFullFilename =
        CPLStrdup(CPLFormFilename( psStream->pszPath, szFile, NULL ));

    /* create the dted file */
    pszError = DTEDCreate( pszFullFilename, psStream->nLevel,
                           nCrLat, nCrLong );
    if( pszError != NULL )
    {
#ifndef AVOID_CPL
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to create DTED file `%s'.\n%s",
                  pszFullFilename, pszError );
#endif
        return FALSE;
    }

    psInfo = DTEDOpen( pszFullFilename, "rb+", FALSE );

    if( psInfo == NULL )
    {
        CPLFree( pszFullFilename );
        return FALSE;
    }

    /* add cached file to stream */
    psStream->nOpenFiles++;
    psStream->pasCF =
        CPLRealloc(psStream->pasCF,
                   sizeof(DTEDCachedFile)*psStream->nOpenFiles);

    psStream->pasCF[psStream->nOpenFiles-1].psInfo = psInfo;
    psStream->pasCF[psStream->nOpenFiles-1].papanProfiles =
        CPLCalloc(sizeof(GInt16*),psInfo->nXSize);
    psStream->pasCF[psStream->nOpenFiles-1].pszFilename = pszFullFilename;
    psStream->pasCF[psStream->nOpenFiles-1].nLLLat = nCrLat;
    psStream->pasCF[psStream->nOpenFiles-1].nLLLong = nCrLong;

    psStream->nLastFile = psStream->nOpenFiles-1;

    return TRUE;
}

/************************************************************************/
/*                           DTEDWritePtLL()                            */
/************************************************************************/

static int DTEDWritePtLL( CPL_UNUSED DTEDPtStream *psStream,
                          DTEDCachedFile *psCF,
                          double dfLong,
                          double dfLat,
                          double dfElev )
{
/* -------------------------------------------------------------------- */
/*      Determine what profile this belongs in, and initialize the      */
/*      profile if it doesn't already exist.                            */
/* -------------------------------------------------------------------- */
    DTEDInfo *psInfo = psCF->psInfo;
    int iProfile, i, iRow;

    iProfile = (int) ((dfLong - psInfo->dfULCornerX) / psInfo->dfPixelSizeX);
    iProfile = MAX(0,MIN(psInfo->nXSize-1,iProfile));

    if( psCF->papanProfiles[iProfile] == NULL )
    {
        psCF->papanProfiles[iProfile] =
            CPLMalloc(sizeof(GInt16) * psInfo->nYSize);

        for( i = 0; i < psInfo->nYSize; i++ )
            psCF->papanProfiles[iProfile][i] = DTED_NODATA_VALUE;
    }

/* -------------------------------------------------------------------- */
/*      Establish where we fit in the profile.                          */
/* -------------------------------------------------------------------- */
    iRow = (int) ((psInfo->dfULCornerY-dfLat) / psInfo->dfPixelSizeY);
    iRow = MAX(0,MIN(psInfo->nYSize-1,iRow));

    psCF->papanProfiles[iProfile][iRow] = (GInt16) floor(dfElev+0.5);

    return TRUE;
}

/************************************************************************/
/*                            DTEDWritePt()                             */
/*                                                                      */
/*      Write a single point out, creating a new file if necessary      */
/*      to hold it.                                                     */
/************************************************************************/

int DTEDWritePt( void *hStream, double dfLong, double dfLat, double dfElev )

{
    DTEDPtStream *psStream = (DTEDPtStream *) hStream;
    int          i;
    DTEDInfo     *psInfo;
    int          bOnBoundary = FALSE;

/* -------------------------------------------------------------------- */
/*      Determine if we are in a boundary region ... that is in the     */
/*      area of the edge "pixel" that is shared with adjacent           */
/*      tiles.                                                          */
/* -------------------------------------------------------------------- */
    if( (floor(dfLong - 0.5*psStream->dfPixelSize)
         != floor(dfLong + 0.5*psStream->dfPixelSize))
        || (floor(dfLat - 0.5*psStream->dfPixelSize)
            != floor(dfLat + 0.5*psStream->dfPixelSize)) )
    {
        bOnBoundary = TRUE;
        psStream->nLastFile = -1;
    }

/* ==================================================================== */
/*      Handle case where the tile is not on a boundary.  We only       */
/*      need one output tile.                                           */
/* ==================================================================== */
/* -------------------------------------------------------------------- */
/*      Is the last file used still applicable?                         */
/* -------------------------------------------------------------------- */
    if( !bOnBoundary )
    {
        if( psStream->nLastFile != -1 )
        {
            psInfo = psStream->pasCF[psStream->nLastFile].psInfo;

            if( dfLat > psInfo->dfULCornerY
                || dfLat < psInfo->dfULCornerY - 1.0 - psInfo->dfPixelSizeY
                || dfLong < psInfo->dfULCornerX
                || dfLong > psInfo->dfULCornerX + 1.0 + psInfo->dfPixelSizeX )
                psStream->nLastFile = -1;
        }

/* -------------------------------------------------------------------- */
/*      Search for the file to write to.                                */
/* -------------------------------------------------------------------- */
        for( i = 0; i < psStream->nOpenFiles && psStream->nLastFile == -1; i++ )
        {
            psInfo = psStream->pasCF[i].psInfo;

            if( !(dfLat > psInfo->dfULCornerY
                  || dfLat < psInfo->dfULCornerY - 1.0 - psInfo->dfPixelSizeY
                  || dfLong < psInfo->dfULCornerX
                  || dfLong > psInfo->dfULCornerX + 1.0 + psInfo->dfPixelSizeX) )
            {
                psStream->nLastFile = i;
            }
        }

/* -------------------------------------------------------------------- */
/*      If none found, create a new file.                               */
/* -------------------------------------------------------------------- */
        if( psStream->nLastFile == -1 )
        {
            int nCrLong, nCrLat;

            nCrLong = (int) floor(dfLong);
            nCrLat = (int) floor(dfLat);

            if( !DTEDPtStreamNewTile( psStream, nCrLong, nCrLat ) )
                return FALSE;
        }

/* -------------------------------------------------------------------- */
/*      Write data out to selected tile.                                */
/* -------------------------------------------------------------------- */
        return DTEDWritePtLL( psStream, psStream->pasCF + psStream->nLastFile,
                              dfLong, dfLat, dfElev );
    }

/* ==================================================================== */
/*      Handle case where we are on a boundary.  We may be writing      */
/*      the value to as many as four tiles.                             */
/* ==================================================================== */
    else
    {
        int nLatMin, nLatMax, nLongMin, nLongMax;
        int nCrLong, nCrLat;

        nLongMin = (int) floor( dfLong - 0.5*psStream->dfPixelSize );
        nLongMax = (int) floor( dfLong + 0.5*psStream->dfPixelSize );
        nLatMin = (int) floor( dfLat - 0.5*psStream->dfPixelSize );
        nLatMax = (int) floor( dfLat + 0.5*psStream->dfPixelSize );

        for( nCrLong = nLongMin; nCrLong <= nLongMax; nCrLong++ )
        {
            for( nCrLat = nLatMin; nCrLat <= nLatMax; nCrLat++ )
            {
                psStream->nLastFile = -1;

/* -------------------------------------------------------------------- */
/*      Find this tile in our existing list.                            */
/* -------------------------------------------------------------------- */
                for( i = 0; i < psStream->nOpenFiles; i++ )
                {
                    if( psStream->pasCF[i].nLLLong == nCrLong
                        && psStream->pasCF[i].nLLLat == nCrLat )
                    {
                        psStream->nLastFile = i;
                        break;
                    }
                }

/* -------------------------------------------------------------------- */
/*      Create the tile if not found.                                   */
/* -------------------------------------------------------------------- */
                if( psStream->nLastFile == -1 )
                {
                    if( !DTEDPtStreamNewTile( psStream, nCrLong, nCrLat ) )
                        return FALSE;
                }

/* -------------------------------------------------------------------- */
/*      Write to the tile.                                              */
/* -------------------------------------------------------------------- */
                if( !DTEDWritePtLL( psStream,
                                    psStream->pasCF + psStream->nLastFile,
                                    dfLong, dfLat, dfElev ) )
                    return FALSE;
            }
        }
    }

    return TRUE;
}

/************************************************************************/
/*                         DTEDClosePtStream()                          */
/************************************************************************/

void DTEDClosePtStream( void *hStream )

{
    DTEDPtStream *psStream = (DTEDPtStream *) hStream;
    int           iFile, iMD;

/* -------------------------------------------------------------------- */
/*      Flush all DTED files.                                           */
/* -------------------------------------------------------------------- */
    for( iFile = 0; iFile < psStream->nOpenFiles; iFile++ )
    {
        int             iProfile;
        DTEDCachedFile  *psCF = psStream->pasCF + iFile;

        for( iProfile = 0; iProfile < psCF->psInfo->nXSize; iProfile++ )
        {
            if( psCF->papanProfiles[iProfile] != NULL )
            {
                DTEDWriteProfile( psCF->psInfo, iProfile,
                                  psCF->papanProfiles[iProfile] );
                CPLFree( psCF->papanProfiles[iProfile] );
            }
        }

        CPLFree( psCF->papanProfiles );

        for( iMD = 0; iMD <= DTEDMD_MAX; iMD++ )
        {
            if( psStream->apszMetadata[iMD] != NULL )
                DTEDSetMetadata( psCF->psInfo, (DTEDMetaDataCode)iMD,
                                 psStream->apszMetadata[iMD] );
        }

        DTEDClose( psCF->psInfo );
    }

/* -------------------------------------------------------------------- */
/*      Final cleanup.                                                  */
/* -------------------------------------------------------------------- */

    for( iMD = 0; iMD < DTEDMD_MAX+1; iMD++ )
        CPLFree( psStream->apszMetadata[iMD] );

    CPLFree( psStream->pasCF );
    CPLFree( psStream->pszPath );
    CPLFree( psStream );
}

/************************************************************************/
/*                           DTEDFillPixel()                            */
/************************************************************************/
static
void DTEDFillPixel( DTEDInfo *psInfo, GInt16 **papanProfiles,
                    GInt16 **papanDstProfiles, int iX, int iY,
                    int nPixelSearchDist, float *pafKernel )

{
    int nKernelWidth = 2 * nPixelSearchDist + 1;
    int nXMin, nXMax, nYMin, nYMax;
    double dfCoefSum = 0.0, dfValueSum = 0.0;
    int iXS, iYS;

    nXMin = MAX(0,iX - nPixelSearchDist);
    nXMax = MIN(psInfo->nXSize-1,iX + nPixelSearchDist);
    nYMin = MAX(0,iY - nPixelSearchDist);
    nYMax = MIN(psInfo->nYSize-1,iY + nPixelSearchDist);

    for( iXS = nXMin; iXS <= nXMax; iXS++ )
    {
        GInt16  *panThisProfile = papanProfiles[iXS];

        if( panThisProfile == NULL )
            continue;

        for( iYS = nYMin; iYS <= nYMax; iYS++ )
        {
            if( panThisProfile[iYS] != DTED_NODATA_VALUE )
            {
                int     iXK, iYK;
                float   fKernelCoef;

                iXK = iXS - iX + nPixelSearchDist;
                iYK = iYS - iY + nPixelSearchDist;

                fKernelCoef = pafKernel[iXK + iYK * nKernelWidth];
                dfCoefSum += fKernelCoef;
                dfValueSum += fKernelCoef * panThisProfile[iYS];
            }
        }
    }

    if( dfCoefSum == 0.0 )
        papanDstProfiles[iX][iY] = DTED_NODATA_VALUE;
    else
        papanDstProfiles[iX][iY] =
            (GInt16) floor(dfValueSum / dfCoefSum + 0.5);
}

/************************************************************************/
/*                          DTEDFillPtStream()                          */
/*                                                                      */
/*      Apply simple inverse distance interpolator to all no-data       */
/*      pixels based on available values within the indicated search    */
/*      distance (rectangular).                                         */
/************************************************************************/

void DTEDFillPtStream( void *hStream, int nPixelSearchDist )

{
    DTEDPtStream *psStream = (DTEDPtStream *) hStream;
    int           iFile, nKernelWidth;
    float         *pafKernel;
    int           iX, iY;

/* -------------------------------------------------------------------- */
/*      Setup inverse distance weighting kernel.                        */
/* -------------------------------------------------------------------- */
    nKernelWidth = 2 * nPixelSearchDist + 1;
    pafKernel = (float *) CPLMalloc(nKernelWidth*nKernelWidth*sizeof(float));

    for( iX = 0; iX < nKernelWidth; iX++ )
    {
        for( iY = 0; iY < nKernelWidth; iY++ )
        {
            pafKernel[iX + iY * nKernelWidth] = (float) (1.0 /
                sqrt( (nPixelSearchDist-iX) * (nPixelSearchDist-iX)
                      + (nPixelSearchDist-iY) * (nPixelSearchDist-iY) ));
        }
    }

/* ==================================================================== */
/*      Process each cached file.                                       */
/* ==================================================================== */
    for( iFile = 0; iFile < psStream->nOpenFiles; iFile++ )
    {
        DTEDInfo        *psInfo = psStream->pasCF[iFile].psInfo;
        GInt16          **papanProfiles = psStream->pasCF[iFile].papanProfiles;
        GInt16          **papanDstProfiles;

        papanDstProfiles = (GInt16 **)
            CPLCalloc(sizeof(GInt16*),psInfo->nXSize);

/* -------------------------------------------------------------------- */
/*      Setup output image.                                             */
/* -------------------------------------------------------------------- */
        for( iX = 0; iX < psInfo->nXSize; iX++ )
        {
            papanDstProfiles[iX] = (GInt16 *)
                CPLMalloc(sizeof(GInt16) * psInfo->nYSize);
        }

/* -------------------------------------------------------------------- */
/*      Interpolate all missing values, and copy over available values. */
/* -------------------------------------------------------------------- */
        for( iX = 0; iX < psInfo->nXSize; iX++ )
        {
            for( iY = 0; iY < psInfo->nYSize; iY++ )
            {
                if( papanProfiles[iX] == NULL
                    || papanProfiles[iX][iY] == DTED_NODATA_VALUE )
                {
                    DTEDFillPixel( psInfo, papanProfiles, papanDstProfiles,
                                   iX, iY, nPixelSearchDist, pafKernel );
                }
                else
                {
                    papanDstProfiles[iX][iY] = papanProfiles[iX][iY];
                }
            }
        }
/* -------------------------------------------------------------------- */
/*      Push new values back into cache.                                */
/* -------------------------------------------------------------------- */
        for( iX = 0; iX < psInfo->nXSize; iX++ )
        {
            CPLFree( papanProfiles[iX] );
            papanProfiles[iX] = papanDstProfiles[iX];
        }

        CPLFree( papanDstProfiles );
    }

    CPLFree( pafKernel );
}

/************************************************************************/
/*                      DTEDPtStreamSetMetadata()                       */
/************************************************************************/

void DTEDPtStreamSetMetadata( void *hStream, DTEDMetaDataCode eCode,
                              const char *pszValue )

{
    DTEDPtStream *psStream = (DTEDPtStream *) hStream;

    if( (int)eCode >= 0 && eCode < DTEDMD_MAX+1 )
    {
        CPLFree( psStream->apszMetadata[eCode] );
        psStream->apszMetadata[eCode] = CPLStrdup( pszValue );
    }
}

/************************************************************************/
/*                   DTEDPtStreamTrimEdgeOnlyTiles()                    */
/*                                                                      */
/*      Erase all tiles that only have boundary values set.             */
/************************************************************************/

void DTEDPtStreamTrimEdgeOnlyTiles( void *hStream )

{
    DTEDPtStream *psStream = (DTEDPtStream *) hStream;
    int iFile;

    for( iFile = psStream->nOpenFiles-1; iFile >= 0; iFile-- )
    {
        DTEDInfo        *psInfo = psStream->pasCF[iFile].psInfo;
        GInt16          **papanProfiles = psStream->pasCF[iFile].papanProfiles;
        int             iProfile, iPixel, bGotNonEdgeData = FALSE;

        for( iProfile = 1; iProfile < psInfo->nXSize-1; iProfile++ )
        {
            if( papanProfiles[iProfile] == NULL )
                continue;

            for( iPixel = 1; iPixel < psInfo->nYSize-1; iPixel++ )
            {
                if( papanProfiles[iProfile][iPixel] != DTED_NODATA_VALUE )
                {
                    bGotNonEdgeData = TRUE;
                    break;
                }
            }
        }

        if( bGotNonEdgeData )
            continue;

        /* Remove this tile */

        for( iProfile = 0; iProfile < psInfo->nXSize; iProfile++ )
        {
            if( papanProfiles[iProfile] != NULL )
                CPLFree( papanProfiles[iProfile] );
        }
        CPLFree( papanProfiles );

        DTEDClose( psInfo );

        VSIUnlink( psStream->pasCF[iFile].pszFilename );
        CPLFree( psStream->pasCF[iFile].pszFilename );

        memmove( psStream->pasCF + iFile,
                 psStream->pasCF + iFile + 1,
                 sizeof(DTEDCachedFile) * (psStream->nOpenFiles-iFile-1) );
        psStream->nOpenFiles--;
    }
}
