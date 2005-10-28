/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  Vector rasterization.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.1  2005/10/28 17:46:08  fwarmerdam
 * New
 *
 */

#include "gdal_alg.h"
#include "gdal_priv.h"
#include "ogr_geometry.h"
#include "ogr_api.h"
#include <vector>

typedef struct {
    unsigned char * pabyChunkBuf;
    int nXSize;
    int nYSize;
    int nBands;
    GDALDataType eType;
    double *padfBurnValue;
} GDALRasterizeInfo;

/************************************************************************/
/*                           gvBurnScanline()                           */
/************************************************************************/

void gvBurnScanline( void *pCBData, int nY, int nXStart, int nXEnd )

{
    GDALRasterizeInfo *psInfo = (GDALRasterizeInfo *) pCBData;
    int iBand;

    if( nXStart > nXEnd )
        return;

    CPLAssert( nY >= 0 && nY < psInfo->nYSize );
    CPLAssert( nXStart <= nXEnd );
    CPLAssert( nXStart < psInfo->nXSize );

    /*  CPLAssert( nXEnd > 0 ); */
    CPLAssert( nXEnd >= 0 );

    if( nXStart < 0 )
        nXStart = 0;
    if( nXEnd >= psInfo->nXSize )
        nXEnd = psInfo->nXSize - 1;

    if( psInfo->eType == GDT_Byte )
    {
        for( iBand = 0; iBand < psInfo->nBands; iBand++ )
        {
            unsigned char *pabyInsert;
            unsigned char nBurnValue = (unsigned char) 
                psInfo->padfBurnValue[iBand];
            
            pabyInsert = psInfo->pabyChunkBuf 
                + iBand * psInfo->nXSize * psInfo->nYSize
                + nY * psInfo->nXSize + nXStart;
                
            memset( pabyInsert, nBurnValue, nXEnd - nXStart + 1 );
        }
    }
    else
    {
        for( iBand = 0; iBand < psInfo->nBands; iBand++ )
        {
            int	nPixels = nXEnd - nXStart + 1;
            float   *pafInsert;
            float   fBurnValue = (float) psInfo->padfBurnValue[iBand];
            
            pafInsert = ((float *) psInfo->pabyChunkBuf) 
                + iBand * psInfo->nXSize * psInfo->nYSize
                + nY * psInfo->nXSize + nXStart;

            while( nPixels-- > 0 )
                *(pafInsert++) = fBurnValue;
        }
    }
}

/************************************************************************/
/*                    GDALCollectRingsFromGeometry()                    */
/************************************************************************/

static void GDALCollectRingsFromGeometry(
    OGRGeometry *poShape, 
    std::vector<double> &aPointX, std::vector<double> &aPointY, 
    std::vector<int> &aPartSize )

{
    if( poShape == NULL )
        return;

    OGRwkbGeometryType eFlatType = wkbFlatten(poShape->getGeometryType());
    int i;

    if( EQUAL(poShape->getGeometryName(),"LINEARRING") )
    {
        OGRLinearRing *poRing = (OGRLinearRing *) poShape;
        int nOldCount = aPointX.size();

        aPointX.reserve(nOldCount + poRing->getNumPoints());
        aPointY.reserve(nOldCount + poRing->getNumPoints());
        for( i = poRing->getNumPoints()-1; i >= 0; i-- )
        {
            aPointX.push_back( poRing->getX(i) );
            aPointY.push_back( poRing->getY(i) );
        }
        aPartSize.push_back( poRing->getNumPoints() );
    }
    else if( eFlatType == wkbPolygon )
    {
        OGRPolygon *poPolygon = (OGRPolygon *) poShape;
        
        GDALCollectRingsFromGeometry( poPolygon->getExteriorRing(), 
                                      aPointX, aPointY, aPartSize );

        for( i = 0; i < poPolygon->getNumInteriorRings(); i++ )
            GDALCollectRingsFromGeometry( poPolygon->getInteriorRing(i), 
                                          aPointX, aPointY, aPartSize );
    }
    
    else if( eFlatType == wkbMultiPolygon
             || eFlatType == wkbGeometryCollection )
    {
        OGRGeometryCollection *poGC = (OGRGeometryCollection *) poShape;

        for( i = 0; i < poGC->getNumGeometries(); i++ )
            GDALCollectRingsFromGeometry( poGC->getGeometryRef(i),
                                          aPointX, aPointY, aPartSize );
    }
    else
    {
        CPLDebug( "GDAL", "Rasterizer ignoring non-polygonal geometry." );
    }
}

/************************************************************************/
/*                       gv_rasterize_one_shape()                       */
/************************************************************************/
static void 
gv_rasterize_new_one_shape( unsigned char *pabyChunkBuf, int nYOff, int nYSize,
                            int nBands, GDALDataType eType, GDALDataset *poDS,
                            OGRGeometry *poShape, double *padfBurnValue, 
                            GDALTransformerFunc pfnTransformer, 
                            void *pTransformArg )

{
    GDALRasterizeInfo sInfo;

    sInfo.nXSize = poDS->GetRasterXSize();
    sInfo.nYSize = nYSize;
    sInfo.nBands = nBands;
    sInfo.pabyChunkBuf = pabyChunkBuf;
    sInfo.eType = eType;
    sInfo.padfBurnValue = padfBurnValue;

/* -------------------------------------------------------------------- */
/*      Transform polygon geometries into a set of rings and a part     */
/*      size list.                                                      */
/* -------------------------------------------------------------------- */
    std::vector<double> aPointX;
    std::vector<double> aPointY;
    std::vector<int> aPartSize;

    GDALCollectRingsFromGeometry( poShape, aPointX, aPointY, aPartSize );

/* -------------------------------------------------------------------- */
/*      Transform points if needed.                                     */
/* -------------------------------------------------------------------- */
    if( pfnTransformer != NULL )
    {
        int *panSuccess = (int *) CPLCalloc(sizeof(int),aPointX.size());

        // TODO: we need to add all appropriate error checking at some point.
        pfnTransformer( pTransformArg, FALSE, aPointX.size(), 
                        &(aPointX[0]), &(aPointY[0]), NULL, panSuccess );
        CPLFree( panSuccess );
    }

/* -------------------------------------------------------------------- */
/*      Perform the rasterization.  We assume that the vector<> will    */
/*      be contiguous.  I'm not convinced that is guaranteed.           */
/* -------------------------------------------------------------------- */
    GDALdllImageFilledPolygon( sInfo.nXSize, nYSize, 
                               aPartSize.size(), &(aPartSize[0]), 
                               &(aPointX[0]), &(aPointY[0]),
                               gvBurnScanline, &sInfo );
}

/************************************************************************/
/*                      GDALRasterizeGeometries()                       */
/************************************************************************/

CPLErr GDALRasterizeGeometries( GDALDatasetH hDS, 
                                int nBandCount, int *panBandList,
                                int nGeomCount, OGRGeometryH *pahGeometries,
                                GDALTransformerFunc pfnTransformer, 
                                void *pTransformArg, 
                                double *padfGeomBurnValue,
                                char **papszOptions )

{
    GDALDataType   eType;
    int            nYChunkSize, nScanlineBytes;
    unsigned char *pabyChunkBuf;
    int            iY;
    GDALDataset *poDS = (GDALDataset *) hDS;

/* -------------------------------------------------------------------- */
/*      Do some rudimentary testing.                                    */
/* -------------------------------------------------------------------- */
    if( nBandCount == 0 || nGeomCount == 0 )
        return CE_None;

    // prototype band.
    GDALRasterBand *poBand = poDS->GetRasterBand( panBandList[0] );

/* -------------------------------------------------------------------- */
/*      If we have no transformer, assume the geometries are in file    */
/*      georeferenced coordinates, and create a transformer to          */
/*      convert that to pixel/line coordinates.                         */
/*                                                                      */
/*      We really just need to apply an affine transform, but for       */
/*      simplicity we use the more general GenImgProjTransformer.       */
/* -------------------------------------------------------------------- */
    int bNeedToFreeTransformer = FALSE;

    if( pfnTransformer == NULL )
    {
        bNeedToFreeTransformer = TRUE;

        pTransformArg = 
            GDALCreateGenImgProjTransformer( NULL, NULL, hDS, NULL, 
                                             FALSE, 0.0, 0);
        pfnTransformer = GDALGenImgProjTransform;
    }

/* -------------------------------------------------------------------- */
/*      Establish a chunksize to operate on.  The larger the chunk      */
/*      size the less times we need to make a pass through all the      */
/*      shapes.                                                         */
/* -------------------------------------------------------------------- */
    if( poBand->GetRasterDataType() == GDT_Byte )
        eType = GDT_Byte;
    else
        eType = GDT_Float32;

    nScanlineBytes = nBandCount * poDS->GetRasterXSize()
        * (GDALGetDataTypeSize(eType)/8);
    nYChunkSize = 10000000 / nScanlineBytes;
    if( nYChunkSize > poBand->GetYSize() )
        nYChunkSize = poBand->GetYSize();

    pabyChunkBuf = (unsigned char *) VSIMalloc(nYChunkSize * nScanlineBytes);
    if( pabyChunkBuf == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, 
                  "Unable to allocate rasterization buffer." );
        return CE_Failure;
    }

/* ==================================================================== */
/*      Loop over image in designated chunks.                           */
/* ==================================================================== */
    CPLErr  eErr = CE_None;

    for( iY = 0; iY < poDS->GetRasterYSize(); iY += nYChunkSize )
    {
        int	nThisYChunkSize;
        int     iShape;

        nThisYChunkSize = nYChunkSize;
        if( nThisYChunkSize + iY > poDS->GetRasterYSize() )
            nThisYChunkSize = poDS->GetRasterYSize() - iY;

        eErr = 
            poDS->RasterIO(GF_Read, 
                           0, iY, poDS->GetRasterXSize(), nThisYChunkSize, 
                           pabyChunkBuf,poDS->GetRasterXSize(),nThisYChunkSize,
                           eType, nBandCount, panBandList,
                           0, 0, 0 );
        if( eErr != CE_None )
            break;

        for( iShape = 0; iShape < nGeomCount; iShape++ )
        {
            gv_rasterize_new_one_shape( pabyChunkBuf, iY, nThisYChunkSize,
                                        nBandCount, eType, poDS,
                                        (OGRGeometry *) pahGeometries[iShape],
                                        padfGeomBurnValue + iShape*nBandCount, 
                                        pfnTransformer, pTransformArg );
        }

        eErr = 
            poDS->RasterIO( GF_Write, 
                            0, iY, poBand->GetXSize(), nThisYChunkSize, 
                            pabyChunkBuf, poBand->GetXSize(), nThisYChunkSize, 
                            eType, nBandCount, panBandList,
                            0, 0, 0 );
        if( eErr != CE_None )
            break;
    }
    
/* -------------------------------------------------------------------- */
/*      cleanup                                                         */
/* -------------------------------------------------------------------- */
    VSIFree( pabyChunkBuf );
    
    if( bNeedToFreeTransformer )
        GDALDestroyTransformer( pTransformArg );

    return CE_None;
}

