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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.7  2003/05/30 16:17:21  warmerda
 * fix warnings with casting and unused parameters
 *
 * Revision 1.6  2002/03/05 14:26:01  warmerda
 * expanded tabs
 *
 * Revision 1.5  2002/02/15 18:26:44  warmerda
 * create output directory if necessary
 *
 * Revision 1.4  2002/01/28 19:11:34  warmerda
 * avoid warning
 *
 * Revision 1.3  2002/01/28 18:18:48  warmerda
 * Added DTEDPtStreamSetMetadata
 *
 * Revision 1.2  2001/11/23 16:43:34  warmerda
 * rough interpolate implementation
 *
 * Revision 1.1  2001/11/21 19:51:34  warmerda
 * New
 *
 */

#include "dted_api.h"

typedef struct {
    DTEDInfo *psInfo;

    GInt16 **papanProfiles;
} DTEDCachedFile;

typedef struct {
    int nLevel;
    char *pszPath;

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

    
    
    return (void *) psStream;
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
    int          i, iProfile, iRow;
    DTEDInfo     *psInfo;

/* -------------------------------------------------------------------- */
/*      Is the last file used still applicable?                         */
/* -------------------------------------------------------------------- */
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
        DTEDInfo        *psInfo;
        char            szFile[128];
        char            chNSHemi, chEWHemi;
        int             nCrLong, nCrLat;
        char            *pszFullFilename;
        const char      *pszError;

        nCrLong = (int) floor(dfLong);
        nCrLat = (int) floor(dfLat);

        /* work out filename */
        if( nCrLat < 0 )
            chNSHemi = 's';
        else
            chNSHemi = 'n';

        if( nCrLong < 0 )
            chEWHemi = 'w';
        else
            chEWHemi = 'e';

        sprintf( szFile, "%c%03d%c%03d.dt%d", 
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

        CPLFree( pszFullFilename );

        if( psInfo == NULL )
            return FALSE;

        /* add cached file to stream */
        psStream->nOpenFiles++;
        psStream->pasCF = 
            CPLRealloc(psStream->pasCF, 
                       sizeof(DTEDCachedFile)*psStream->nOpenFiles);

        psStream->pasCF[psStream->nOpenFiles-1].psInfo = psInfo;
        psStream->pasCF[psStream->nOpenFiles-1].papanProfiles =
            CPLCalloc(sizeof(GInt16*),psInfo->nXSize);

        psStream->nLastFile = psStream->nOpenFiles-1;
    }

/* -------------------------------------------------------------------- */
/*      Determine what profile this belongs in, and initialize the      */
/*      profile if it doesn't already exist.                            */
/* -------------------------------------------------------------------- */
    psInfo = psStream->pasCF[psStream->nLastFile].psInfo;

    iProfile = (int) ((dfLong - psInfo->dfULCornerX) / psInfo->dfPixelSizeX);
    iProfile = MAX(0,MIN(psInfo->nXSize-1,iProfile));

    if( psStream->pasCF[psStream->nLastFile].papanProfiles[iProfile] == NULL )
    {
        psStream->pasCF[psStream->nLastFile].papanProfiles[iProfile] = 
            CPLMalloc(sizeof(GInt16) * psInfo->nYSize);

        for( i = 0; i < psInfo->nYSize; i++ )
            psStream->pasCF[psStream->nLastFile].papanProfiles[iProfile][i] =
                DTED_NODATA_VALUE;
    }

/* -------------------------------------------------------------------- */
/*      Establish where we fit in the profile.                          */
/* -------------------------------------------------------------------- */
    iRow = (int) ((psInfo->dfULCornerY-dfLat) / psInfo->dfPixelSizeY);
    iRow = MAX(0,MIN(psInfo->nYSize-1,iRow));

    psStream->pasCF[psStream->nLastFile].papanProfiles[iProfile][iRow] = 
        (GInt16) dfElev;
    
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

        for( iMD = 0; iMD < DTEDMD_MAX+1; iMD++ )
        {
            if( psStream->apszMetadata[iMD] != NULL )
                DTEDSetMetadata( psCF->psInfo, iMD, 
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

    if( eCode >= 0 && eCode < DTEDMD_MAX+1 )
    {
        CPLFree( psStream->apszMetadata[eCode] );
        psStream->apszMetadata[eCode] = CPLStrdup( pszValue );
    }
}

