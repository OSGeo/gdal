/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  Implements Geolocation array based transformer.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam <warmerdam@pobox.com>
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
#include "gdal_alg.h"

#ifdef SHAPE_DEBUG
#include "/u/pkg/shapelib/shapefil.h"

SHPHandle hSHP = NULL;
DBFHandle hDBF = NULL;
#endif

CPL_CVSID("$Id$");

CPL_C_START
CPLXMLNode *GDALSerializeGeoLocTransformer( void *pTransformArg );
void *GDALDeserializeGeoLocTransformer( CPLXMLNode *psTree );
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*			   GDALGeoLocTransformer                        */
/* ==================================================================== */
/************************************************************************/

typedef struct {

    GDALTransformerInfo sTI;

    int         bReversed;

    // Map from target georef coordinates back to geolocation array
    // pixel line coordinates.  Built only if needed.

    int         nBackMapWidth;
    int         nBackMapHeight;
    double      adfBackMapGeoTransform[6]; // maps georef to pixel/line.
    float       *pafBackMapX;
    float       *pafBackMapY;

    // geolocation bands.
    
    GDALDatasetH     hDS_X;
    GDALRasterBandH  hBand_X;
    GDALDatasetH     hDS_Y;
    GDALRasterBandH  hBand_Y;

    // Located geolocation data. 
    int              nGeoLocXSize;
    int              nGeoLocYSize;
    double           *padfGeoLocX;
    double           *padfGeoLocY;

    double           dfNoDataX;
    double           dfNoDataY;
    
    // geolocation <-> base image mapping.
    double           dfPIXEL_OFFSET;
    double           dfPIXEL_STEP;
    double           dfLINE_OFFSET;
    double           dfLINE_STEP;

    char **          papszGeolocationInfo;

} GDALGeoLocTransformInfo;

/************************************************************************/
/*                         GeoLocLoadFullData()                         */
/************************************************************************/

static int GeoLocLoadFullData( GDALGeoLocTransformInfo *psTransform )

{
    int nXSize, nYSize;

    int nXSize_XBand = GDALGetRasterXSize( psTransform->hDS_X );
    int nYSize_XBand = GDALGetRasterYSize( psTransform->hDS_X );
    int nXSize_YBand = GDALGetRasterXSize( psTransform->hDS_Y );
    int nYSize_YBand = GDALGetRasterYSize( psTransform->hDS_Y );
    if (nYSize_XBand == 1 && nYSize_YBand == 1)
    {
        nXSize = nXSize_XBand;
        nYSize = nXSize_YBand;
    }
    else
    {
        nXSize = nXSize_XBand;
        nYSize = nYSize_XBand;
    }

    psTransform->nGeoLocXSize = nXSize;
    psTransform->nGeoLocYSize = nYSize;
    
    psTransform->padfGeoLocY = (double *) 
        VSIMalloc3(sizeof(double), nXSize, nYSize);
    psTransform->padfGeoLocX = (double *) 
        VSIMalloc3(sizeof(double), nXSize, nYSize);
    
    if( psTransform->padfGeoLocX == NULL ||
        psTransform->padfGeoLocY == NULL )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "GeoLocLoadFullData : Out of memory");
        return FALSE;
    }

    if (nYSize_XBand == 1 && nYSize_YBand == 1)
    {
        /* Case of regular grid */
        /* The XBAND contains the x coordinates for all lines */
        /* The YBAND contains the y coordinates for all columns */

        double* padfTempX = (double*)VSIMalloc2(nXSize, sizeof(double));
        double* padfTempY = (double*)VSIMalloc2(nYSize, sizeof(double));
        if (padfTempX == NULL || padfTempY == NULL)
        {
            CPLFree(padfTempX);
            CPLFree(padfTempY);
            CPLError(CE_Failure, CPLE_OutOfMemory,
                 "GeoLocLoadFullData : Out of memory");
            return FALSE;
        }

        CPLErr eErr = CE_None;

        eErr = GDALRasterIO( psTransform->hBand_X, GF_Read, 
                             0, 0, nXSize, 1,
                             padfTempX, nXSize, 1, 
                             GDT_Float64, 0, 0 );

        int i,j;
        for(j=0;j<nYSize;j++)
        {
            memcpy( psTransform->padfGeoLocX + j * nXSize,
                    padfTempX,
                    nXSize * sizeof(double) );
        }

        if (eErr == CE_None)
        {
            eErr = GDALRasterIO( psTransform->hBand_Y, GF_Read, 
                                0, 0, nYSize, 1,
                                padfTempY, nYSize, 1, 
                                GDT_Float64, 0, 0 );

            for(j=0;j<nYSize;j++)
            {
                for(i=0;i<nXSize;i++)
                {
                    psTransform->padfGeoLocY[j * nXSize + i] = padfTempY[j];
                }
            }
        }

        CPLFree(padfTempX);
        CPLFree(padfTempY);

        if (eErr != CE_None)
            return FALSE;
    }
    else
    {
        if( GDALRasterIO( psTransform->hBand_X, GF_Read, 
                        0, 0, nXSize, nYSize,
                        psTransform->padfGeoLocX, nXSize, nYSize, 
                        GDT_Float64, 0, 0 ) != CE_None 
            || GDALRasterIO( psTransform->hBand_Y, GF_Read, 
                            0, 0, nXSize, nYSize,
                            psTransform->padfGeoLocY, nXSize, nYSize, 
                            GDT_Float64, 0, 0 ) != CE_None )
            return FALSE;
    }

    psTransform->dfNoDataX = GDALGetRasterNoDataValue( psTransform->hBand_X, 
                                                       NULL );
    psTransform->dfNoDataY = GDALGetRasterNoDataValue( psTransform->hBand_Y, 
                                                       NULL );

    return TRUE;
}

/************************************************************************/
/*                       GeoLocGenerateBackMap()                        */
/************************************************************************/

static int GeoLocGenerateBackMap( GDALGeoLocTransformInfo *psTransform )

{
    int nXSize = psTransform->nGeoLocXSize;
    int nYSize = psTransform->nGeoLocYSize;
    int nMaxIter = 3;

/* -------------------------------------------------------------------- */
/*      Scan forward map for lat/long extents.                          */
/* -------------------------------------------------------------------- */
    double dfMinX=0, dfMaxX=0, dfMinY=0, dfMaxY=0;
    int i, bInit = FALSE;

    for( i = nXSize * nYSize - 1; i >= 0; i-- )
    {
        if( psTransform->padfGeoLocX[i] != psTransform->dfNoDataX )
        {
            if( bInit )
            {
                dfMinX = MIN(dfMinX,psTransform->padfGeoLocX[i]);
                dfMaxX = MAX(dfMaxX,psTransform->padfGeoLocX[i]);
                dfMinY = MIN(dfMinY,psTransform->padfGeoLocY[i]);
                dfMaxY = MAX(dfMaxY,psTransform->padfGeoLocY[i]);
            }
            else
            {
                bInit = TRUE;
                dfMinX = dfMaxX = psTransform->padfGeoLocX[i];
                dfMinY = dfMaxY = psTransform->padfGeoLocY[i];
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Decide on resolution for backmap.  We aim for slightly          */
/*      higher resolution than the source but we can't easily           */
/*      establish how much dead space there is in the backmap, so it    */
/*      is approximate.                                                 */
/* -------------------------------------------------------------------- */
    double dfTargetPixels = (nXSize * nYSize * 1.3);
    double dfPixelSize = sqrt((dfMaxX - dfMinX) * (dfMaxY - dfMinY) 
                              / dfTargetPixels);
    int nBMXSize, nBMYSize;

    nBMYSize = psTransform->nBackMapHeight = 
        (int) ((dfMaxY - dfMinY) / dfPixelSize + 1);
    nBMXSize= psTransform->nBackMapWidth =  
        (int) ((dfMaxX - dfMinX) / dfPixelSize + 1);

    if (nBMXSize > INT_MAX / nBMYSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Int overflow : %d x %d",
                 nBMXSize, nBMYSize);
        return FALSE;
    }

    dfMinX -= dfPixelSize/2.0;
    dfMaxY += dfPixelSize/2.0;

    psTransform->adfBackMapGeoTransform[0] = dfMinX;
    psTransform->adfBackMapGeoTransform[1] = dfPixelSize;
    psTransform->adfBackMapGeoTransform[2] = 0.0;
    psTransform->adfBackMapGeoTransform[3] = dfMaxY;
    psTransform->adfBackMapGeoTransform[4] = 0.0;
    psTransform->adfBackMapGeoTransform[5] = -dfPixelSize;

/* -------------------------------------------------------------------- */
/*      Allocate backmap, and initialize to nodata value (-1.0).        */
/* -------------------------------------------------------------------- */
    GByte  *pabyValidFlag;

    pabyValidFlag = (GByte *) 
        VSICalloc(nBMXSize, nBMYSize); 

    psTransform->pafBackMapX = (float *) 
        VSIMalloc3(nBMXSize, nBMYSize, sizeof(float)); 
    psTransform->pafBackMapY = (float *) 
        VSIMalloc3(nBMXSize, nBMYSize, sizeof(float)); 

    if( pabyValidFlag == NULL ||
        psTransform->pafBackMapX == NULL ||
        psTransform->pafBackMapY == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, 
                  "Unable to allocate %dx%d back-map for geolocation array transformer.",
                  nBMXSize, nBMYSize );
        CPLFree( pabyValidFlag );
        return FALSE;
    }

    for( i = nBMXSize * nBMYSize - 1; i >= 0; i-- )
    {
        psTransform->pafBackMapX[i] = -1.0;
        psTransform->pafBackMapY[i] = -1.0;
    }

/* -------------------------------------------------------------------- */
/*      Run through the whole geoloc array forward projecting and       */
/*      pushing into the backmap.                                       */
/*      Initialise to the nMaxIter+1 value so we can spot genuinely     */
/*      valid pixels in the hole-filling loop.                          */
/* -------------------------------------------------------------------- */
    int iBMX, iBMY;
    int iX, iY;

    for( iY = 0; iY < nYSize; iY++ )
    {
        for( iX = 0; iX < nXSize; iX++ )
        {
            if( psTransform->padfGeoLocX[iX + iY * nXSize] 
                == psTransform->dfNoDataX )
                continue;

            i = iX + iY * nXSize;

            iBMX = (int) ((psTransform->padfGeoLocX[i] - dfMinX) / dfPixelSize);
            iBMY = (int) ((dfMaxY - psTransform->padfGeoLocY[i]) / dfPixelSize);

            if( iBMX < 0 || iBMY < 0 || iBMX >= nBMXSize || iBMY >= nBMYSize )
                continue;

            psTransform->pafBackMapX[iBMX + iBMY * nBMXSize] = 
                (float)(iX * psTransform->dfPIXEL_STEP + psTransform->dfPIXEL_OFFSET);
            psTransform->pafBackMapY[iBMX + iBMY * nBMXSize] = 
                (float)(iY * psTransform->dfLINE_STEP + psTransform->dfLINE_OFFSET);

            pabyValidFlag[iBMX + iBMY * nBMXSize] = (GByte) (nMaxIter+1);

        }
    }

/* -------------------------------------------------------------------- */
/*      Now, loop over the backmap trying to fill in holes with         */
/*      nearby values.                                                  */
/* -------------------------------------------------------------------- */
    int iIter;
    int nNumValid, nExpectedValid;

    for( iIter = 0; iIter < nMaxIter; iIter++ )
    {
        nNumValid = 0;
        nExpectedValid = (nBMYSize - 2) * (nBMXSize - 2);
        for( iBMY = 1; iBMY < nBMYSize-1; iBMY++ )
        {
            for( iBMX = 1; iBMX < nBMXSize-1; iBMX++ )
            {
                // if this point is already set, ignore it. 
                if( pabyValidFlag[iBMX + iBMY*nBMXSize] )
                {
                    nNumValid++;
                    continue;
                }

                int nCount = 0;
                double dfXSum = 0.0, dfYSum = 0.0;
                int nMarkedAsGood = nMaxIter - iIter;

                // left?
                if( pabyValidFlag[iBMX-1+iBMY*nBMXSize] > nMarkedAsGood )
                {
                    dfXSum += psTransform->pafBackMapX[iBMX-1+iBMY*nBMXSize];
                    dfYSum += psTransform->pafBackMapY[iBMX-1+iBMY*nBMXSize];
                    nCount++;
                }
                // right?
                if( pabyValidFlag[iBMX+1+iBMY*nBMXSize] > nMarkedAsGood )
                {
                    dfXSum += psTransform->pafBackMapX[iBMX+1+iBMY*nBMXSize];
                    dfYSum += psTransform->pafBackMapY[iBMX+1+iBMY*nBMXSize];
                    nCount++;
                }
                // top?
                if( pabyValidFlag[iBMX+(iBMY-1)*nBMXSize] > nMarkedAsGood )
                {
                    dfXSum += psTransform->pafBackMapX[iBMX+(iBMY-1)*nBMXSize];
                    dfYSum += psTransform->pafBackMapY[iBMX+(iBMY-1)*nBMXSize];
                    nCount++;
                }
                // bottom?
                if( pabyValidFlag[iBMX+(iBMY+1)*nBMXSize] > nMarkedAsGood )
                {
                    dfXSum += psTransform->pafBackMapX[iBMX+(iBMY+1)*nBMXSize];
                    dfYSum += psTransform->pafBackMapY[iBMX+(iBMY+1)*nBMXSize];
                    nCount++;
                }
                // top-left?
                if( pabyValidFlag[iBMX-1+(iBMY-1)*nBMXSize] > nMarkedAsGood )
                {
                    dfXSum += psTransform->pafBackMapX[iBMX-1+(iBMY-1)*nBMXSize];
                    dfYSum += psTransform->pafBackMapY[iBMX-1+(iBMY-1)*nBMXSize];
                    nCount++;
                }
                // top-right?
                if( pabyValidFlag[iBMX+1+(iBMY-1)*nBMXSize] > nMarkedAsGood )
                {
                    dfXSum += psTransform->pafBackMapX[iBMX+1+(iBMY-1)*nBMXSize];
                    dfYSum += psTransform->pafBackMapY[iBMX+1+(iBMY-1)*nBMXSize];
                    nCount++;
                }
                // bottom-left?
                if( pabyValidFlag[iBMX-1+(iBMY+1)*nBMXSize] > nMarkedAsGood )
                {
                    dfXSum += psTransform->pafBackMapX[iBMX-1+(iBMY+1)*nBMXSize];
                    dfYSum += psTransform->pafBackMapY[iBMX-1+(iBMY+1)*nBMXSize];
                    nCount++;
                }
                // bottom-right?
                if( pabyValidFlag[iBMX+1+(iBMY+1)*nBMXSize] > nMarkedAsGood )
                {
                    dfXSum += psTransform->pafBackMapX[iBMX+1+(iBMY+1)*nBMXSize];
                    dfYSum += psTransform->pafBackMapY[iBMX+1+(iBMY+1)*nBMXSize];
                    nCount++;
                }

                if( nCount > 0 )
                {
                    psTransform->pafBackMapX[iBMX + iBMY * nBMXSize] = (float)(dfXSum/nCount);
                    psTransform->pafBackMapY[iBMX + iBMY * nBMXSize] = (float)(dfYSum/nCount);
                    // genuinely valid points will have value iMaxIter+1
                    // On each iteration mark newly valid points with a
                    // descending value so that it will not be used on the
                    // current iteration only on subsequent ones.
                    pabyValidFlag[iBMX+iBMY*nBMXSize] = (GByte) (nMaxIter - iIter);
                }
            }
        }
        if (nNumValid == nExpectedValid)
            break;
    }

#ifdef notdef
    GDALDatasetH hBMDS = GDALCreate( GDALGetDriverByName( "GTiff" ),
                                     "backmap.tif", nBMXSize, nBMYSize, 2, 
                                     GDT_Float32, NULL );
    GDALSetGeoTransform( hBMDS, psTransform->adfBackMapGeoTransform );
    GDALRasterIO( GDALGetRasterBand(hBMDS,1), GF_Write, 
                  0, 0, nBMXSize, nBMYSize, 
                  psTransform->pafBackMapX, nBMXSize, nBMYSize, 
                  GDT_Float32, 0, 0 );
    GDALRasterIO( GDALGetRasterBand(hBMDS,2), GF_Write, 
                  0, 0, nBMXSize, nBMYSize, 
                  psTransform->pafBackMapY, nBMXSize, nBMYSize, 
                  GDT_Float32, 0, 0 );
    GDALClose( hBMDS );
#endif

    CPLFree( pabyValidFlag );

    return TRUE;
}

/************************************************************************/
/*                         FindGeoLocPosition()                         */
/************************************************************************/

#ifdef notdef 

/*
This searching approach has been abandoned because it is too sensitive
to discontinuities in the data.  Left in case it might be revived in 
the future.
 */

static int FindGeoLocPosition( GDALGeoLocTransformInfo *psTransform,
                               double dfGeoX, double dfGeoY,
                               int nStartX, int nStartY, 
                               double *pdfFoundX, double *pdfFoundY )

{
    double adfPathX[5000], adfPathY[5000];

    if( psTransform->padfGeoLocX == NULL )
        return FALSE;

    int nXSize = psTransform->nGeoLocXSize;
    int nYSize = psTransform->nGeoLocYSize;
    int nStepCount = 0;

    // Start in center if we don't have any provided info.
    if( nStartX < 0 || nStartY < 0 
        || nStartX >= nXSize || nStartY >= nYSize )
    {
        nStartX = nXSize / 2;
        nStartY = nYSize / 2;
    }

    nStartX = MIN(nStartX,nXSize-2);
    nStartY = MIN(nStartY,nYSize-2);

    int iX = nStartX, iY = nStartY;
    int iLastX = -1, iLastY = -1;
    int iSecondLastX = -1, iSecondLastY = -1;

    while( nStepCount < MAX(nXSize,nYSize) )
    {
        int iXNext = -1, iYNext = -1;
        double dfDeltaXRight, dfDeltaYRight, dfDeltaXDown, dfDeltaYDown;

        double *padfThisX = psTransform->padfGeoLocX + iX + iY * nXSize;
        double *padfThisY = psTransform->padfGeoLocY + iX + iY * nXSize;

        double dfDeltaX = dfGeoX - *padfThisX;
        double dfDeltaY = dfGeoY - *padfThisY;

        if( iX == nXSize-1 )
        {
            dfDeltaXRight = *(padfThisX) - *(padfThisX-1);
            dfDeltaYRight = *(padfThisY) - *(padfThisY-1);
        }
        else
        {
            dfDeltaXRight = *(padfThisX+1) - *padfThisX;
            dfDeltaYRight = *(padfThisY+1) - *padfThisY;
        }

        if( iY == nYSize - 1 )
        {
            dfDeltaXDown = *(padfThisX) - *(padfThisX-nXSize);
            dfDeltaYDown = *(padfThisY) - *(padfThisY-nXSize);
        }
        else
        {
            dfDeltaXDown = *(padfThisX+nXSize) - *padfThisX;
            dfDeltaYDown = *(padfThisY+nXSize) - *padfThisY;
        }

        double dfRightProjection = 
            (dfDeltaXRight * dfDeltaX + dfDeltaYRight * dfDeltaY) 
            / (dfDeltaXRight*dfDeltaXRight + dfDeltaYRight*dfDeltaYRight);

        double dfDownProjection = 
            (dfDeltaXDown * dfDeltaX + dfDeltaYDown * dfDeltaY) 
            / (dfDeltaXDown*dfDeltaXDown + dfDeltaYDown*dfDeltaYDown);

        // Are we in our target cell?
        if( dfRightProjection >= 0.0 && dfRightProjection < 1.0 
            && dfDownProjection >= 0.0 && dfDownProjection < 1.0 )
        {
            *pdfFoundX = iX + dfRightProjection;
            *pdfFoundY = iY + dfDownProjection;

            return TRUE;
        }
            
        if( ABS(dfRightProjection) > ABS(dfDownProjection) )
        {
            // Do we want to move right? 
            if( dfRightProjection > 1.0 && iX < nXSize-1 )
            {
                iXNext = iX + MAX(1,(int)(dfRightProjection - nStepCount)/2);
                iYNext = iY;
            }
            
            // Do we want to move left? 
            else if( dfRightProjection < 0.0 && iX > 0 )
            {
                iXNext = iX - MAX(1,(int)(ABS(dfRightProjection) - nStepCount)/2);
                iYNext = iY;
            }
            
            // Do we want to move down.
            else if( dfDownProjection > 1.0 && iY < nYSize-1 )
            {
                iXNext = iX;
                iYNext = iY + MAX(1,(int)(dfDownProjection - nStepCount)/2);
            }
            
            // Do we want to move up? 
            else if( dfDownProjection < 0.0 && iY > 0 )
            {
                iXNext = iX;
                iYNext = iY - MAX(1,(int)(ABS(dfDownProjection) - nStepCount)/2);
            }
            
            // We aren't there, and we have no where to go
            else
            {
                return FALSE;
            }
        }
        else
        {
            // Do we want to move down.
            if( dfDownProjection > 1.0 && iY < nYSize-1 )
            {
                iXNext = iX;
                iYNext = iY + MAX(1,(int)(dfDownProjection - nStepCount)/2);
            }
            
            // Do we want to move up? 
            else if( dfDownProjection < 0.0 && iY > 0 )
            {
                iXNext = iX;
                iYNext = iY - MAX(1,(int)(ABS(dfDownProjection) - nStepCount)/2);
            }
            
            // Do we want to move right? 
            else if( dfRightProjection > 1.0 && iX < nXSize-1 )
            {
                iXNext = iX + MAX(1,(int)(dfRightProjection - nStepCount)/2);
                iYNext = iY;
            }
            
            // Do we want to move left? 
            else if( dfRightProjection < 0.0 && iX > 0 )
            {
                iXNext = iX - MAX(1,(int)(ABS(dfRightProjection) - nStepCount)/2);
                iYNext = iY;
            }
            
            // We aren't there, and we have no where to go
            else
            {
                return FALSE;
            }
        }
                adfPathX[nStepCount] = iX;
        adfPathY[nStepCount] = iY;

        nStepCount++;
        iX = MAX(0,MIN(iXNext,nXSize-1));
        iY = MAX(0,MIN(iYNext,nYSize-1));

        if( iX == iSecondLastX && iY == iSecondLastY )
        {
            // Are we *near* our target cell?
            if( dfRightProjection >= -1.0 && dfRightProjection < 2.0
                && dfDownProjection >= -1.0 && dfDownProjection < 2.0 )
            {
                *pdfFoundX = iX + dfRightProjection;
                *pdfFoundY = iY + dfDownProjection;
                
                return TRUE;
            }

#ifdef SHAPE_DEBUG
            if( hSHP != NULL )
            {
                SHPObject *hObj;
                
                hObj = SHPCreateSimpleObject( SHPT_ARC, nStepCount,
                                              adfPathX, adfPathY, NULL );
                SHPWriteObject( hSHP, -1, hObj );
                SHPDestroyObject( hObj );
                
                int iShape = DBFGetRecordCount( hDBF );
                DBFWriteDoubleAttribute( hDBF, iShape, 0, dfGeoX );
                DBFWriteDoubleAttribute( hDBF, iShape, 1, dfGeoY );
            }
#endif             
            //CPLDebug( "GeoL", "Looping at step (%d) on search for %g,%g.", 
            //          nStepCount, dfGeoX, dfGeoY );
            return FALSE;
        }

        iSecondLastX = iLastX;
        iSecondLastY = iLastY;

        iLastX = iX;
        iLastY = iY;

    }

    //CPLDebug( "GeoL", "Exceeded step count max (%d) on search for %g,%g.", 
    //          MAX(nXSize,nYSize), 
    //          dfGeoX, dfGeoY );
    
#ifdef SHAPE_DEBUG
    if( hSHP != NULL )
    {
        SHPObject *hObj;

        hObj = SHPCreateSimpleObject( SHPT_ARC, nStepCount,
                                      adfPathX, adfPathY, NULL );
        SHPWriteObject( hSHP, -1, hObj );
        SHPDestroyObject( hObj );

        int iShape = DBFGetRecordCount( hDBF );
        DBFWriteDoubleAttribute( hDBF, iShape, 0, dfGeoX );
        DBFWriteDoubleAttribute( hDBF, iShape, 1, dfGeoY );
    }
#endif
              
    return FALSE;
}
#endif /* def notdef */



/************************************************************************/
/*                    GDALCreateGeoLocTransformer()                     */
/************************************************************************/

void *GDALCreateGeoLocTransformer( GDALDatasetH hBaseDS, 
                                   char **papszGeolocationInfo,
                                   int bReversed )

{
    GDALGeoLocTransformInfo *psTransform;

    if( CSLFetchNameValue(papszGeolocationInfo,"PIXEL_OFFSET") == NULL
        || CSLFetchNameValue(papszGeolocationInfo,"LINE_OFFSET") == NULL
        || CSLFetchNameValue(papszGeolocationInfo,"PIXEL_STEP") == NULL
        || CSLFetchNameValue(papszGeolocationInfo,"LINE_STEP") == NULL
        || CSLFetchNameValue(papszGeolocationInfo,"X_BAND") == NULL
        || CSLFetchNameValue(papszGeolocationInfo,"Y_BAND") == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Missing some geolocation fields in GDALCreateGeoLocTransformer()" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Initialize core info.                                           */
/* -------------------------------------------------------------------- */
    psTransform = (GDALGeoLocTransformInfo *) 
        CPLCalloc(sizeof(GDALGeoLocTransformInfo),1);

    psTransform->bReversed = bReversed;

    strcpy( psTransform->sTI.szSignature, "GTI" );
    psTransform->sTI.pszClassName = "GDALGeoLocTransformer";
    psTransform->sTI.pfnTransform = GDALGeoLocTransform;
    psTransform->sTI.pfnCleanup = GDALDestroyGeoLocTransformer;
    psTransform->sTI.pfnSerialize = GDALSerializeGeoLocTransformer;
    psTransform->papszGeolocationInfo = CSLDuplicate( papszGeolocationInfo );

/* -------------------------------------------------------------------- */
/*      Pull geolocation info from the options/metadata.                */
/* -------------------------------------------------------------------- */
    psTransform->dfPIXEL_OFFSET = atof(CSLFetchNameValue( papszGeolocationInfo,
                                                          "PIXEL_OFFSET" ));
    psTransform->dfLINE_OFFSET = atof(CSLFetchNameValue( papszGeolocationInfo,
                                                         "LINE_OFFSET" ));
    psTransform->dfPIXEL_STEP = atof(CSLFetchNameValue( papszGeolocationInfo,
                                                        "PIXEL_STEP" ));
    psTransform->dfLINE_STEP = atof(CSLFetchNameValue( papszGeolocationInfo,
                                                       "LINE_STEP" ));

/* -------------------------------------------------------------------- */
/*      Establish access to geolocation dataset(s).                     */
/* -------------------------------------------------------------------- */
    const char *pszDSName = CSLFetchNameValue( papszGeolocationInfo, 
                                               "X_DATASET" );
    if( pszDSName != NULL )
    {
        psTransform->hDS_X = GDALOpenShared( pszDSName, GA_ReadOnly );
    }
    else
    {
        psTransform->hDS_X = hBaseDS;
        GDALReferenceDataset( psTransform->hDS_X );
        psTransform->papszGeolocationInfo = 
            CSLSetNameValue( psTransform->papszGeolocationInfo, 
                             "X_DATASET", 
                             GDALGetDescription( hBaseDS ) );
    }

    pszDSName = CSLFetchNameValue( papszGeolocationInfo, "Y_DATASET" );
    if( pszDSName != NULL )
    {
        psTransform->hDS_Y = GDALOpenShared( pszDSName, GA_ReadOnly );
    }
    else
    {
        psTransform->hDS_Y = hBaseDS;
        GDALReferenceDataset( psTransform->hDS_Y );
        psTransform->papszGeolocationInfo = 
            CSLSetNameValue( psTransform->papszGeolocationInfo, 
                             "Y_DATASET", 
                             GDALGetDescription( hBaseDS ) );
    }

    if (psTransform->hDS_X == NULL ||
        psTransform->hDS_Y == NULL)
    {
        GDALDestroyGeoLocTransformer( psTransform );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Get the band handles.                                           */
/* -------------------------------------------------------------------- */
    int nBand;

    nBand = MAX(1,atoi(CSLFetchNameValue( papszGeolocationInfo, "X_BAND" )));
    psTransform->hBand_X = GDALGetRasterBand( psTransform->hDS_X, nBand );

    nBand = MAX(1,atoi(CSLFetchNameValue( papszGeolocationInfo, "Y_BAND" )));
    psTransform->hBand_Y = GDALGetRasterBand( psTransform->hDS_Y, nBand );

    if (psTransform->hBand_X == NULL ||
        psTransform->hBand_Y == NULL)
    {
        GDALDestroyGeoLocTransformer( psTransform );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*     Check that X and Y bands have the same dimensions                */
/* -------------------------------------------------------------------- */
    int nXSize_XBand = GDALGetRasterXSize( psTransform->hDS_X );
    int nYSize_XBand = GDALGetRasterYSize( psTransform->hDS_X );
    int nXSize_YBand = GDALGetRasterXSize( psTransform->hDS_Y );
    int nYSize_YBand = GDALGetRasterYSize( psTransform->hDS_Y );
    if (nYSize_XBand == 1 || nYSize_YBand == 1)
    {
        if (nYSize_XBand != 1 || nYSize_YBand != 1)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                 "X_BAND and Y_BAND should have both nYSize == 1");
            GDALDestroyGeoLocTransformer( psTransform );
            return NULL;
        }
    }
    else if (nXSize_XBand != nXSize_YBand ||
             nYSize_XBand != nYSize_YBand )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "X_BAND and Y_BAND do not have the same dimensions");
        GDALDestroyGeoLocTransformer( psTransform );
        return NULL;
    }

    if (nXSize_XBand > INT_MAX / nYSize_XBand)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Int overflow : %d x %d",
                 nXSize_XBand, nYSize_XBand);
        GDALDestroyGeoLocTransformer( psTransform );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Load the geolocation array.                                     */
/* -------------------------------------------------------------------- */
    if( !GeoLocLoadFullData( psTransform ) 
        || !GeoLocGenerateBackMap( psTransform ) )
    {
        GDALDestroyGeoLocTransformer( psTransform );
        return NULL;
    }

    return psTransform;
}

/************************************************************************/
/*                    GDALDestroyGeoLocTransformer()                    */
/************************************************************************/

void GDALDestroyGeoLocTransformer( void *pTransformAlg )

{
    GDALGeoLocTransformInfo *psTransform = 
        (GDALGeoLocTransformInfo *) pTransformAlg;

    CPLFree( psTransform->pafBackMapX );
    CPLFree( psTransform->pafBackMapY );
    CSLDestroy( psTransform->papszGeolocationInfo );
    CPLFree( psTransform->padfGeoLocX );
    CPLFree( psTransform->padfGeoLocY );
             
    if( psTransform->hDS_X != NULL 
        && GDALDereferenceDataset( psTransform->hDS_X ) == 0 )
            GDALClose( psTransform->hDS_X );

    if( psTransform->hDS_Y != NULL 
        && GDALDereferenceDataset( psTransform->hDS_Y ) == 0 )
            GDALClose( psTransform->hDS_Y );

    CPLFree( pTransformAlg );
}

/************************************************************************/
/*                        GDALGeoLocTransform()                         */
/************************************************************************/

int GDALGeoLocTransform( void *pTransformArg, int bDstToSrc, 
                         int nPointCount, 
                         double *padfX, double *padfY, double *padfZ,
                         int *panSuccess )

{
    GDALGeoLocTransformInfo *psTransform = 
        (GDALGeoLocTransformInfo *) pTransformArg;

    if( psTransform->bReversed )
        bDstToSrc = !bDstToSrc;

/* -------------------------------------------------------------------- */
/*      Do original pixel line to target geox/geoy.                     */
/* -------------------------------------------------------------------- */
    if( !bDstToSrc )
    {
        int i, nXSize = psTransform->nGeoLocXSize;

        for( i = 0; i < nPointCount; i++ )
        {
            if( padfX[i] == HUGE_VAL || padfY[i] == HUGE_VAL )
            {
                panSuccess[i] = FALSE;
                continue;
            }

            double dfGeoLocPixel = (padfX[i] - psTransform->dfPIXEL_OFFSET) 
                / psTransform->dfPIXEL_STEP;
            double dfGeoLocLine = (padfY[i] - psTransform->dfLINE_OFFSET) 
                / psTransform->dfLINE_STEP;

            int iX, iY;

            iX = MAX(0,(int) dfGeoLocPixel);
            iX = MIN(iX,psTransform->nGeoLocXSize-2);
            iY = MAX(0,(int) dfGeoLocLine);
            iY = MIN(iY,psTransform->nGeoLocYSize-2);

            double *padfGLX = psTransform->padfGeoLocX + iX + iY * nXSize;
            double *padfGLY = psTransform->padfGeoLocY + iX + iY * nXSize;

            // This assumes infinite extension beyond borders of available
            // data based on closest grid square.

            padfX[i] = padfGLX[0] 
                + (dfGeoLocPixel-iX) * (padfGLX[1] - padfGLX[0])
                + (dfGeoLocLine -iY) * (padfGLX[nXSize] - padfGLX[0]);
            padfY[i] = padfGLY[0] 
                + (dfGeoLocPixel-iX) * (padfGLY[1] - padfGLY[0])
                + (dfGeoLocLine -iY) * (padfGLY[nXSize] - padfGLY[0]);

            panSuccess[i] = TRUE;
        }
    }

/* -------------------------------------------------------------------- */
/*      geox/geoy to pixel/line using backmap.                          */
/* -------------------------------------------------------------------- */
    else
    {
        int i;

        for( i = 0; i < nPointCount; i++ )
        {
            if( padfX[i] == HUGE_VAL || padfY[i] == HUGE_VAL )
            {
                panSuccess[i] = FALSE;
                continue;
            }

            int iBMX, iBMY;

            iBMX = (int) ((padfX[i] - psTransform->adfBackMapGeoTransform[0])
                          / psTransform->adfBackMapGeoTransform[1]);
            iBMY = (int) ((padfY[i] - psTransform->adfBackMapGeoTransform[3])
                          / psTransform->adfBackMapGeoTransform[5]);

            int iBM = iBMX + iBMY * psTransform->nBackMapWidth;

            if( iBMX < 0 || iBMY < 0 
                || iBMX >= psTransform->nBackMapWidth
                || iBMY >= psTransform->nBackMapHeight 
                || psTransform->pafBackMapX[iBM] < 0 )
            {
                panSuccess[i] = FALSE;
                padfX[i] = HUGE_VAL;
                padfY[i] = HUGE_VAL;
                continue;
            }

            padfX[i] = psTransform->pafBackMapX[iBM];
            padfY[i] = psTransform->pafBackMapY[iBM];
            panSuccess[i] = TRUE;
        }
    }

/* -------------------------------------------------------------------- */
/*      geox/geoy to pixel/line using search algorithm.                 */
/* -------------------------------------------------------------------- */
#ifdef notdef
    else
    {
        int i;
        int nStartX = -1, nStartY = -1;

#ifdef SHAPE_DEBUG
        hSHP = SHPCreate( "tracks.shp", SHPT_ARC );
        hDBF = DBFCreate( "tracks.dbf" );
        DBFAddField( hDBF, "GEOX", FTDouble, 10, 4 );
        DBFAddField( hDBF, "GEOY", FTDouble, 10, 4 );
#endif
        for( i = 0; i < nPointCount; i++ )
        {
            double dfGeoLocX, dfGeoLocY;

            if( padfX[i] == HUGE_VAL || padfY[i] == HUGE_VAL )
            {
                panSuccess[i] = FALSE;
                continue;
            }

            if( !FindGeoLocPosition( psTransform, padfX[i], padfY[i], 
                                     -1, -1, &dfGeoLocX, &dfGeoLocY ) )
            {
                padfX[i] = HUGE_VAL;
                padfY[i] = HUGE_VAL;

                panSuccess[i] = FALSE;
                continue;
            }
            nStartX = (int) dfGeoLocX;
            nStartY = (int) dfGeoLocY;

            padfX[i] = dfGeoLocX * psTransform->dfPIXEL_STEP 
                + psTransform->dfPIXEL_OFFSET;
            padfY[i] = dfGeoLocY * psTransform->dfLINE_STEP 
                + psTransform->dfLINE_OFFSET;

            panSuccess[i] = TRUE;
        }

#ifdef SHAPE_DEBUG
        if( hSHP != NULL )
        {
            DBFClose( hDBF );
            hDBF = NULL;
            
            SHPClose( hSHP );
            hSHP = NULL;
        }
#endif
    }
#endif

    return TRUE;
}

/************************************************************************/
/*                   GDALSerializeGeoLocTransformer()                   */
/************************************************************************/

CPLXMLNode *GDALSerializeGeoLocTransformer( void *pTransformArg )

{
    VALIDATE_POINTER1( pTransformArg, "GDALSerializeGeoLocTransformer", NULL );

    CPLXMLNode *psTree;
    GDALGeoLocTransformInfo *psInfo = 
        (GDALGeoLocTransformInfo *)(pTransformArg);

    psTree = CPLCreateXMLNode( NULL, CXT_Element, "GeoLocTransformer" );

/* -------------------------------------------------------------------- */
/*      Serialize bReversed.                                            */
/* -------------------------------------------------------------------- */
    CPLCreateXMLElementAndValue( 
        psTree, "Reversed", 
        CPLString().Printf( "%d", psInfo->bReversed ) );
                                 
/* -------------------------------------------------------------------- */
/*      geoloc metadata.                                                */
/* -------------------------------------------------------------------- */
    char **papszMD = psInfo->papszGeolocationInfo;
    CPLXMLNode *psMD= CPLCreateXMLNode( psTree, CXT_Element, 
                                        "Metadata" );

    for( int i = 0; papszMD != NULL && papszMD[i] != NULL; i++ )
    {
        const char *pszRawValue;
        char *pszKey;
        CPLXMLNode *psMDI;
                
        pszRawValue = CPLParseNameValue( papszMD[i], &pszKey );
                
        psMDI = CPLCreateXMLNode( psMD, CXT_Element, "MDI" );
        CPLSetXMLValue( psMDI, "#key", pszKey );
        CPLCreateXMLNode( psMDI, CXT_Text, pszRawValue );
                
        CPLFree( pszKey );
    }

    return psTree;
}

/************************************************************************/
/*                   GDALDeserializeGeoLocTransformer()                 */
/************************************************************************/

void *GDALDeserializeGeoLocTransformer( CPLXMLNode *psTree )

{
    void *pResult;
    int bReversed;
    char **papszMD = NULL;
    CPLXMLNode *psMDI, *psMetadata;

/* -------------------------------------------------------------------- */
/*      Collect metadata.                                               */
/* -------------------------------------------------------------------- */
    psMetadata = CPLGetXMLNode( psTree, "Metadata" );

    if( psMetadata == NULL ||
        psMetadata->eType != CXT_Element
        || !EQUAL(psMetadata->pszValue,"Metadata") )
        return NULL;
    
    for( psMDI = psMetadata->psChild; psMDI != NULL; 
         psMDI = psMDI->psNext )
    {
        if( !EQUAL(psMDI->pszValue,"MDI") 
            || psMDI->eType != CXT_Element 
            || psMDI->psChild == NULL 
            || psMDI->psChild->psNext == NULL 
            || psMDI->psChild->eType != CXT_Attribute
            || psMDI->psChild->psChild == NULL )
            continue;
        
        papszMD = 
            CSLSetNameValue( papszMD, 
                             psMDI->psChild->psChild->pszValue, 
                             psMDI->psChild->psNext->pszValue );
    }

/* -------------------------------------------------------------------- */
/*      Get other flags.                                                */
/* -------------------------------------------------------------------- */
    bReversed = atoi(CPLGetXMLValue(psTree,"Reversed","0"));

/* -------------------------------------------------------------------- */
/*      Generate transformation.                                        */
/* -------------------------------------------------------------------- */
    pResult = GDALCreateGeoLocTransformer( NULL, papszMD, bReversed );
    
/* -------------------------------------------------------------------- */
/*      Cleanup GCP copy.                                               */
/* -------------------------------------------------------------------- */
    CSLDestroy( papszMD );

    return pResult;
}
