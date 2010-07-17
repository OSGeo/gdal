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
 ****************************************************************************/

#include <vector>

#include "gdal_alg.h"
#include "gdal_alg_priv.h"
#include "gdal_priv.h"
#include "ogr_api.h"
#include "ogr_geometry.h"
#include "ogr_spatialref.h"

#ifdef OGR_ENABLED
#include "ogrsf_frmts.h"
#endif

/************************************************************************/
/*                           gvBurnScanline()                           */
/************************************************************************/

void gvBurnScanline( void *pCBData, int nY, int nXStart, int nXEnd,
                        double dfVariant )

{
    GDALRasterizeInfo *psInfo = (GDALRasterizeInfo *) pCBData;
    int iBand;

    if( nXStart > nXEnd )
        return;

    CPLAssert( nY >= 0 && nY < psInfo->nYSize );
    CPLAssert( nXStart <= nXEnd );
    CPLAssert( nXStart < psInfo->nXSize );
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
                ( psInfo->padfBurnValue[iBand] +
                  ( (psInfo->eBurnValueSource == GBV_UserBurnValue)?
                             0 : dfVariant ) );
            
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
            float   fBurnValue = (float)
                ( psInfo->padfBurnValue[iBand] +
                  ( (psInfo->eBurnValueSource == GBV_UserBurnValue)?
                             0 : dfVariant ) );
            
            pafInsert = ((float *) psInfo->pabyChunkBuf) 
                + iBand * psInfo->nXSize * psInfo->nYSize
                + nY * psInfo->nXSize + nXStart;

            while( nPixels-- > 0 )
                *(pafInsert++) = fBurnValue;
        }
    }
}

/************************************************************************/
/*                            gvBurnPoint()                             */
/************************************************************************/

void gvBurnPoint( void *pCBData, int nY, int nX, double dfVariant )

{
    GDALRasterizeInfo *psInfo = (GDALRasterizeInfo *) pCBData;
    int iBand;

    CPLAssert( nY >= 0 && nY < psInfo->nYSize );
    CPLAssert( nX >= 0 && nX < psInfo->nXSize );

    if( psInfo->eType == GDT_Byte )
    {
        for( iBand = 0; iBand < psInfo->nBands; iBand++ )
        {
            unsigned char *pbyInsert = psInfo->pabyChunkBuf 
                                      + iBand * psInfo->nXSize * psInfo->nYSize
                                      + nY * psInfo->nXSize + nX;

            *pbyInsert = (unsigned char)( psInfo->padfBurnValue[iBand] +
                          ( (psInfo->eBurnValueSource == GBV_UserBurnValue)?
                             0 : dfVariant ) );
        }
    }
    else
    {
        for( iBand = 0; iBand < psInfo->nBands; iBand++ )
        {
            float   *pfInsert = ((float *) psInfo->pabyChunkBuf) 
                                + iBand * psInfo->nXSize * psInfo->nYSize
                                + nY * psInfo->nXSize + nX;

            *pfInsert = (float)( psInfo->padfBurnValue[iBand] +
                         ( (psInfo->eBurnValueSource == GBV_UserBurnValue)?
                            0 : dfVariant ) );
        }
    }
}

/************************************************************************/
/*                    GDALCollectRingsFromGeometry()                    */
/************************************************************************/

static void GDALCollectRingsFromGeometry(
    OGRGeometry *poShape, 
    std::vector<double> &aPointX, std::vector<double> &aPointY, 
    std::vector<double> &aPointVariant, 
    std::vector<int> &aPartSize, GDALBurnValueSrc eBurnValueSrc)

{
    if( poShape == NULL )
        return;

    OGRwkbGeometryType eFlatType = wkbFlatten(poShape->getGeometryType());
    int i;

    if ( eFlatType == wkbPoint )
    {
        OGRPoint    *poPoint = (OGRPoint *) poShape;
        int nNewCount = aPointX.size() + 1;

        aPointX.reserve( nNewCount );
        aPointY.reserve( nNewCount );
        aPointX.push_back( poPoint->getX() );
        aPointY.push_back( poPoint->getY() );
        aPartSize.push_back( 1 );
        if( eBurnValueSrc != GBV_UserBurnValue )
        {
            /*switch( eBurnValueSrc )
            {
            case GBV_Z:*/
                aPointVariant.reserve( nNewCount );
                aPointVariant.push_back( poPoint->getZ() );
                /*break;
            case GBV_M:
                aPointVariant.reserve( nNewCount );
                aPointVariant.push_back( poPoint->getM() );
            }*/
        }
    }
    else if ( eFlatType == wkbLineString )
    {
        OGRLineString   *poLine = (OGRLineString *) poShape;
        int nCount = poLine->getNumPoints();
        int nNewCount = aPointX.size() + nCount;

        aPointX.reserve( nNewCount );
        aPointY.reserve( nNewCount );
        if( eBurnValueSrc != GBV_UserBurnValue )
            aPointVariant.reserve( nNewCount );
        for ( i = nCount - 1; i >= 0; i-- )
        {
            aPointX.push_back( poLine->getX(i) );
            aPointY.push_back( poLine->getY(i) );
            if( eBurnValueSrc != GBV_UserBurnValue )
            {
                /*switch( eBurnValueSrc )
                {
                    case GBV_Z:*/
                        aPointVariant.push_back( poLine->getZ(i) );
                        /*break;
                    case GBV_M:
                        aPointVariant.push_back( poLine->getM(i) );
                }*/
            }
        }
        aPartSize.push_back( nCount );
    }
    else if ( EQUAL(poShape->getGeometryName(),"LINEARRING") )
    {
        OGRLinearRing *poRing = (OGRLinearRing *) poShape;
        int nCount = poRing->getNumPoints();
        int nNewCount = aPointX.size() + nCount;

        aPointX.reserve( nNewCount );
        aPointY.reserve( nNewCount );
        if( eBurnValueSrc != GBV_UserBurnValue )
            aPointVariant.reserve( nNewCount );
        for ( i = nCount - 1; i >= 0; i-- )
        {
            aPointX.push_back( poRing->getX(i) );
            aPointY.push_back( poRing->getY(i) );
        }
        if( eBurnValueSrc != GBV_UserBurnValue )
        {
            /*switch( eBurnValueSrc )
            {
            case GBV_Z:*/
                aPointVariant.push_back( poRing->getZ(i) );
                /*break;
            case GBV_M:
                aPointVariant.push_back( poRing->getM(i) );
            }*/
        }
        aPartSize.push_back( nCount );
    }
    else if( eFlatType == wkbPolygon )
    {
        OGRPolygon *poPolygon = (OGRPolygon *) poShape;
        
        GDALCollectRingsFromGeometry( poPolygon->getExteriorRing(), 
                                      aPointX, aPointY, aPointVariant, 
                                      aPartSize, eBurnValueSrc );

        for( i = 0; i < poPolygon->getNumInteriorRings(); i++ )
            GDALCollectRingsFromGeometry( poPolygon->getInteriorRing(i), 
                                          aPointX, aPointY, aPointVariant, 
                                          aPartSize, eBurnValueSrc );
    }
    
    else if( eFlatType == wkbMultiPoint
             || eFlatType == wkbMultiLineString
             || eFlatType == wkbMultiPolygon
             || eFlatType == wkbGeometryCollection )
    {
        OGRGeometryCollection *poGC = (OGRGeometryCollection *) poShape;

        for( i = 0; i < poGC->getNumGeometries(); i++ )
            GDALCollectRingsFromGeometry( poGC->getGeometryRef(i),
                                          aPointX, aPointY, aPointVariant, 
                                          aPartSize, eBurnValueSrc );
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
gv_rasterize_one_shape( unsigned char *pabyChunkBuf, int nYOff,
                        int nXSize, int nYSize,
                        int nBands, GDALDataType eType, int bAllTouched,
                        OGRGeometry *poShape, double *padfBurnValue, 
                        GDALBurnValueSrc eBurnValueSrc,
                        GDALTransformerFunc pfnTransformer, 
                        void *pTransformArg )

{
    GDALRasterizeInfo sInfo;

    if (poShape == NULL)
        return;

    sInfo.nXSize = nXSize;
    sInfo.nYSize = nYSize;
    sInfo.nBands = nBands;
    sInfo.pabyChunkBuf = pabyChunkBuf;
    sInfo.eType = eType;
    sInfo.padfBurnValue = padfBurnValue;
    sInfo.eBurnValueSource = eBurnValueSrc;

/* -------------------------------------------------------------------- */
/*      Transform polygon geometries into a set of rings and a part     */
/*      size list.                                                      */
/* -------------------------------------------------------------------- */
    std::vector<double> aPointX;
    std::vector<double> aPointY;
    std::vector<double> aPointVariant;
    std::vector<int> aPartSize;

    GDALCollectRingsFromGeometry( poShape, aPointX, aPointY, aPointVariant,
                                    aPartSize, eBurnValueSrc );

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
/*      Shift to account for the buffer offset of this buffer.          */
/* -------------------------------------------------------------------- */
    unsigned int i;

    for( i = 0; i < aPointY.size(); i++ )
        aPointY[i] -= nYOff;

/* -------------------------------------------------------------------- */
/*      Perform the rasterization.                                      */
/*      According to the C++ Standard/23.2.4, elements of a vector are  */
/*      stored in continuous memory block.                              */
/* -------------------------------------------------------------------- */
    unsigned int j,n;

    // TODO - mloskot: Check if vectors are empty, otherwise it may
    // lead to undefined behavior by returning non-referencable pointer.
    // if (!aPointX.empty())
    //    /* fill polygon */
    // else
    //    /* How to report this problem? */    
    switch ( wkbFlatten(poShape->getGeometryType()) )
    {
        case wkbPoint:
        case wkbMultiPoint:
            GDALdllImagePoint( sInfo.nXSize, nYSize, 
                               aPartSize.size(), &(aPartSize[0]), 
                               &(aPointX[0]), &(aPointY[0]), 
                               (eBurnValueSrc == GBV_UserBurnValue)?
                                   NULL : &(aPointVariant[0]),
                               gvBurnPoint, &sInfo );
            break;
        case wkbLineString:
        case wkbMultiLineString:
        {
            if( bAllTouched )
                GDALdllImageLineAllTouched( sInfo.nXSize, nYSize, 
                                            aPartSize.size(), &(aPartSize[0]), 
                                            &(aPointX[0]), &(aPointY[0]), 
                                            (eBurnValueSrc == GBV_UserBurnValue)?
                                                NULL : &(aPointVariant[0]),
                                            gvBurnPoint, &sInfo );
            else
                GDALdllImageLine( sInfo.nXSize, nYSize, 
                                  aPartSize.size(), &(aPartSize[0]), 
                                  &(aPointX[0]), &(aPointY[0]), 
                                  (eBurnValueSrc == GBV_UserBurnValue)?
                                      NULL : &(aPointVariant[0]),
                                  gvBurnPoint, &sInfo );
        }
        break;

        default:
        {
            GDALdllImageFilledPolygon( sInfo.nXSize, nYSize, 
                                       aPartSize.size(), &(aPartSize[0]), 
                                       &(aPointX[0]), &(aPointY[0]), 
                                       (eBurnValueSrc == GBV_UserBurnValue)?
                                           NULL : &(aPointVariant[0]),
                                       gvBurnScanline, &sInfo );
            if( bAllTouched )
            {
                /* Reverting the variants to the first value because the
                   polygon is filled using the variant from the first point of
                   the first segment. Should be removed when the code to full
                   polygons more appropriately is added. */
                if(eBurnValueSrc == GBV_UserBurnValue)
                {
                GDALdllImageLineAllTouched( sInfo.nXSize, nYSize, 
                                            aPartSize.size(), &(aPartSize[0]), 
                                            &(aPointX[0]), &(aPointY[0]), 
                                            NULL,
                                            gvBurnPoint, &sInfo );
                }
                else
                {
                for( i = 0, n = 0; i < aPartSize.size(); i++ )
                    for( j = 0; j < aPartSize[i]; j++ )
                        aPointVariant[n++] = aPointVariant[0];
                   
                GDALdllImageLineAllTouched( sInfo.nXSize, nYSize, 
                                            aPartSize.size(), &(aPartSize[0]), 
                                            &(aPointX[0]), &(aPointY[0]), 
                                            &(aPointVariant[0]),
                                            gvBurnPoint, &sInfo );
                }
            }
        }
        break;
    }
}

/************************************************************************/
/*                      GDALRasterizeGeometries()                       */
/************************************************************************/

/**
 * Burn geometries into raster.
 *
 * Rasterize a list of geometric objects into a raster dataset.  The
 * geometries are passed as an array of OGRGeometryH handlers.  
 *
 * If the geometries are in the georferenced coordinates of the raster
 * dataset, then the pfnTransform may be passed in NULL and one will be
 * derived internally from the geotransform of the dataset.  The transform
 * needs to transform the geometry locations into pixel/line coordinates
 * on the raster dataset.
 *
 * The output raster may be of any GDAL supported datatype, though currently
 * internally the burning is done either as GDT_Byte or GDT_Float32.  This
 * may be improved in the future.  An explicit list of burn values for
 * each geometry for each band must be passed in. 
 *
 * The papszOption list of options currently only supports one option. The
 * "ALL_TOUCHED" option may be enabled by setting it to "TRUE".
 *
 * @param hDS output data, must be opened in update mode.
 * @param nBandCount the number of bands to be updated.
 * @param panBandList the list of bands to be updated. 
 * @param nGeomCount the number of geometries being passed in pahGeometries.
 * @param pahGeometries the array of geometries to burn in. 
 * @param pfnTransformer transformation to apply to geometries to put into 
 * pixel/line coordinates on raster.  If NULL a geotransform based one will
 * be created internally.
 * @param pTransformerArg callback data for transformer.
 * @param padfGeomBurnValue the array of values to burn into the raster.  
 * There should be nBandCount values for each geometry. 
 * @param papszOptions special options controlling rasterization
 * <dl>
 * <dt>"ALL_TOUCHED":</dt> <dd>May be set to TRUE to set all pixels touched 
 * by the line or polygons, not just those whose center is within the polygon
 * or that are selected by brezenhams line algorithm.  Defaults to FALSE.</dd>
 * <dt>"BURN_VALUE_FROM":</dt> <dd>May be set to "Z" to use the Z values of the
 * geometries. dfBurnValue is added to this before burning.
 * Defaults to GDALBurnValueSrc.GBV_UserBurnValue in which case just the
 * dfBurnValue is burned. This is implemented only for points and lines for
 * now. The M value may be supported in the future.</dd>
 * </dl>
 * @param pfnProgress the progress function to report completion.
 * @param pProgressArg callback data for progress function.
 *
 * @return CE_None on success or CE_Failure on error.
 */

CPLErr GDALRasterizeGeometries( GDALDatasetH hDS, 
                                int nBandCount, int *panBandList,
                                int nGeomCount, OGRGeometryH *pahGeometries,
                                GDALTransformerFunc pfnTransformer, 
                                void *pTransformArg, 
                                double *padfGeomBurnValue,
                                char **papszOptions,
                                GDALProgressFunc pfnProgress, 
                                void *pProgressArg )

{
    GDALDataType   eType;
    int            nYChunkSize, nScanlineBytes;
    unsigned char *pabyChunkBuf;
    int            iY;
    GDALDataset *poDS = (GDALDataset *) hDS;

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      Do some rudimentary arg checking.                               */
/* -------------------------------------------------------------------- */
    if( nBandCount == 0 || nGeomCount == 0 )
        return CE_None;

    // prototype band.
    GDALRasterBand *poBand = poDS->GetRasterBand( panBandList[0] );
    if (poBand == NULL)
        return CE_Failure;

    int bAllTouched = CSLFetchBoolean( papszOptions, "ALL_TOUCHED", FALSE );
    const char *pszOpt = CSLFetchNameValue( papszOptions, "BURN_VALUE_FROM" );
    GDALBurnValueSrc eBurnValueSource = GBV_UserBurnValue;
    if( pszOpt )
    {
        if( EQUAL(pszOpt,"Z"))
            eBurnValueSource = GBV_Z;
        /*else if( EQUAL(pszOpt,"M"))
            eBurnValueSource = GBV_M;*/
    }

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
    if( nYChunkSize > poDS->GetRasterYSize() )
        nYChunkSize = poDS->GetRasterYSize();

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

    pfnProgress( 0.0, NULL, pProgressArg );

    for( iY = 0; 
         iY < poDS->GetRasterYSize() && eErr == CE_None; 
         iY += nYChunkSize )
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
            gv_rasterize_one_shape( pabyChunkBuf, iY,
                                    poDS->GetRasterXSize(), nThisYChunkSize,
                                    nBandCount, eType, bAllTouched,
                                    (OGRGeometry *) pahGeometries[iShape],
                                    padfGeomBurnValue + iShape*nBandCount,
                                    eBurnValueSource,
                                    pfnTransformer, pTransformArg );
        }

        eErr = 
            poDS->RasterIO( GF_Write, 0, iY,
                            poDS->GetRasterXSize(), nThisYChunkSize, 
                            pabyChunkBuf,
                            poDS->GetRasterXSize(), nThisYChunkSize, 
                            eType, nBandCount, panBandList, 0, 0, 0 );

        if( !pfnProgress((iY+nThisYChunkSize)/((double)poDS->GetRasterYSize()),
                         "", pProgressArg ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }
    
/* -------------------------------------------------------------------- */
/*      cleanup                                                         */
/* -------------------------------------------------------------------- */
    VSIFree( pabyChunkBuf );
    
    if( bNeedToFreeTransformer )
        GDALDestroyTransformer( pTransformArg );

    return eErr;
}

/************************************************************************/
/*                        GDALRasterizeLayers()                         */
/************************************************************************/

/**
 * Burn geometries from the specified list of layers into raster.
 *
 * Rasterize all the geometric objects from a list of layers into a raster
 * dataset.  The layers are passed as an array of OGRLayerH handlers.
 *
 * If the geometries are in the georferenced coordinates of the raster
 * dataset, then the pfnTransform may be passed in NULL and one will be
 * derived internally from the geotransform of the dataset.  The transform
 * needs to transform the geometry locations into pixel/line coordinates
 * on the raster dataset.
 *
 * The output raster may be of any GDAL supported datatype, though currently
 * internally the burning is done either as GDT_Byte or GDT_Float32.  This
 * may be improved in the future.  An explicit list of burn values for
 * each layer for each band must be passed in. 
 *
 * @param hDS output data, must be opened in update mode.
 * @param nBandCount the number of bands to be updated.
 * @param panBandList the list of bands to be updated. 
 * @param nLayerCount the number of layers being passed in pahLayers array.
 * @param pahLayers the array of layers to burn in. 
 * @param pfnTransformer transformation to apply to geometries to put into 
 * pixel/line coordinates on raster.  If NULL a geotransform based one will
 * be created internally.
 * @param pTransformerArg callback data for transformer.
 * @param padfLayerBurnValues the array of values to burn into the raster.  
 * There should be nBandCount values for each layer. 
 * @param papszOption special options controlling rasterization:
 * <dl>
 * <dt>"ATTRIBUTE":</dt> <dd>Identifies an attribute field on the features to be
 * used for a burn in value. The value will be burned into all output
 * bands. If specified, padfLayerBurnValues will not be used and can be a NULL
 * pointer.</dd>
 * <dt>"CHUNKYSIZE":</dt> <dd>The height in lines of the chunk to operate on.
 * The larger the chunk size the less times we need to make a pass through all
 * the shapes. If it is not set or set to zero the default chunk size will be
 * used. Default size will be estimated based on the GDAL cache buffer size
 * using formula: cache_size_bytes/scanline_size_bytes, so the chunk will
 * not exceed the cache.</dd>
 * <dt>"ALL_TOUCHED":</dt> <dd>May be set to TRUE to set all pixels touched 
 * by the line or polygons, not just those whose center is within the polygon
 * or that are selected by brezenhams line algorithm.  Defaults to FALSE.</dd>
 * <dt>"BURN_VALUE_FROM":</dt> <dd>May be set to "Z" to use the Z values of the
 * geometries. The value from padfLayerBurnValues or the attribute field value
 * is added to this before burning. In default case dfBurnValue is burned as it
 * is. This is implemented properly only for points and lines for now. Polygons
 * will be burned using the Z value from the first point. The M value may be
 * supported in the future.</dd>
 * @param pfnProgress the progress function to report completion.
 * @param pProgressArg callback data for progress function.
 *
 * @return CE_None on success or CE_Failure on error.
 */

#ifdef OGR_ENABLED

CPLErr GDALRasterizeLayers( GDALDatasetH hDS, 
                            int nBandCount, int *panBandList,
                            int nLayerCount, OGRLayerH *pahLayers,
                            GDALTransformerFunc pfnTransformer, 
                            void *pTransformArg, 
                            double *padfLayerBurnValues,
                            char **papszOptions,
                            GDALProgressFunc pfnProgress, 
                            void *pProgressArg )

{
    GDALDataType   eType;
    unsigned char *pabyChunkBuf;
    GDALDataset *poDS = (GDALDataset *) hDS;

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      Do some rudimentary arg checking.                               */
/* -------------------------------------------------------------------- */
    if( nBandCount == 0 || nLayerCount == 0 )
        return CE_None;

    // prototype band.
    GDALRasterBand *poBand = poDS->GetRasterBand( panBandList[0] );
    if (poBand == NULL)
        return CE_Failure;

    int bAllTouched = CSLFetchBoolean( papszOptions, "ALL_TOUCHED", FALSE );
    const char *pszOpt = CSLFetchNameValue( papszOptions, "BURN_VALUE_FROM" );
    GDALBurnValueSrc eBurnValueSource = GBV_UserBurnValue;
    if( pszOpt )
    {
        if( EQUAL(pszOpt,"Z"))
            eBurnValueSource = GBV_Z;
        /*else if( EQUAL(pszOpt,"M"))
            eBurnValueSource = GBV_M;*/
    }

/* -------------------------------------------------------------------- */
/*      Establish a chunksize to operate on.  The larger the chunk      */
/*      size the less times we need to make a pass through all the      */
/*      shapes.                                                         */
/* -------------------------------------------------------------------- */
    int         nYChunkSize, nScanlineBytes;
    const char  *pszYChunkSize =
        CSLFetchNameValue( papszOptions, "CHUNKYSIZE" );

    if( poBand->GetRasterDataType() == GDT_Byte )
        eType = GDT_Byte;
    else
        eType = GDT_Float32;

    nScanlineBytes = nBandCount * poDS->GetRasterXSize()
        * (GDALGetDataTypeSize(eType)/8);

    if ( pszYChunkSize && (nYChunkSize = atoi(pszYChunkSize)) )
        ;
    else
    {
        GIntBig nYChunkSize64 = GDALGetCacheMax64() / nScanlineBytes;
        if (nYChunkSize64 > INT_MAX)
            nYChunkSize = INT_MAX;
        else
            nYChunkSize = (int)nYChunkSize64;
    }

    if( nYChunkSize < 1 )
        nYChunkSize = 1;
    if( nYChunkSize > poDS->GetRasterYSize() )
        nYChunkSize = poDS->GetRasterYSize();

    pabyChunkBuf = (unsigned char *) VSIMalloc(nYChunkSize * nScanlineBytes);
    if( pabyChunkBuf == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, 
                  "Unable to allocate rasterization buffer." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Read the image once for all layers if user requested to render  */
/*      the whole raster in single chunk.                               */
/* -------------------------------------------------------------------- */
    if ( nYChunkSize == poDS->GetRasterYSize() )
    {
        if ( poDS->RasterIO( GF_Read, 0, 0, poDS->GetRasterXSize(),
                             nYChunkSize, pabyChunkBuf,
                             poDS->GetRasterXSize(), nYChunkSize,
                             eType, nBandCount, panBandList, 0, 0, 0 )
             != CE_None )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory, 
                      "Unable to read buffer." );
            CPLFree( pabyChunkBuf );
            return CE_Failure;
        }
    }

/* ==================================================================== */
/*      Read the specified layers transfoming and rasterizing           */
/*      geometries.                                                     */
/* ==================================================================== */
    CPLErr      eErr = CE_None;
    int         iLayer;
    const char  *pszBurnAttribute =
        CSLFetchNameValue( papszOptions, "ATTRIBUTE" );

    pfnProgress( 0.0, NULL, pProgressArg );

    for( iLayer = 0; iLayer < nLayerCount; iLayer++ )
    {
        int         iBurnField = -1;
        double      *padfBurnValues = NULL;
        OGRLayer    *poLayer = (OGRLayer *) pahLayers[iLayer];

        if ( !poLayer )
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "Layer element number %d is NULL, skipping.\n", iLayer );
            continue;
        }

/* -------------------------------------------------------------------- */
/*      If the layer does not contain any features just skip it.        */
/*      Do not force the feature count, so if driver doesn't know       */
/*      exact number of features, go down the normal way.               */
/* -------------------------------------------------------------------- */
        if ( poLayer->GetFeatureCount(FALSE) == 0 )
            continue;

        if ( pszBurnAttribute )
        {
            iBurnField =
                poLayer->GetLayerDefn()->GetFieldIndex( pszBurnAttribute );
            if ( iBurnField == -1 )
            {
                CPLError( CE_Warning, CPLE_AppDefined, 
                          "Failed to find field %s on layer %s, skipping.\n",
                          pszBurnAttribute, 
                          poLayer->GetLayerDefn()->GetName() );
                continue;
            }
        }
        else
            padfBurnValues = padfLayerBurnValues + iLayer * nBandCount;

/* -------------------------------------------------------------------- */
/*      If we have no transformer, create the one from input file       */
/*      projection. Note that each layer can be georefernced            */
/*      separately.                                                     */
/* -------------------------------------------------------------------- */
        int bNeedToFreeTransformer = FALSE;

        if( pfnTransformer == NULL )
        {
            char    *pszProjection = NULL;
            bNeedToFreeTransformer = TRUE;

            OGRSpatialReference *poSRS = poLayer->GetSpatialRef();
            if ( !poSRS )
            {
                CPLError( CE_Warning, CPLE_AppDefined, 
                          "Failed to fetch spatial reference on layer %s "
                          "to build transformer, assuming matching coordinate systems.\n",
                          poLayer->GetLayerDefn()->GetName() );
            }
            else
                poSRS->exportToWkt( &pszProjection );

            pTransformArg = 
                GDALCreateGenImgProjTransformer( NULL, pszProjection,
                                                 hDS, NULL, FALSE, 0.0, 0 );
            pfnTransformer = GDALGenImgProjTransform;

            CPLFree( pszProjection );
        }

        OGRFeature *poFeat;

        poLayer->ResetReading();

/* -------------------------------------------------------------------- */
/*      Loop over image in designated chunks.                           */
/* -------------------------------------------------------------------- */
        int     iY;
        for( iY = 0; 
             iY < poDS->GetRasterYSize() && eErr == CE_None; 
             iY += nYChunkSize )
        {
            int	nThisYChunkSize;

            nThisYChunkSize = nYChunkSize;
            if( nThisYChunkSize + iY > poDS->GetRasterYSize() )
                nThisYChunkSize = poDS->GetRasterYSize() - iY;

            // Only re-read image if not a single chunk is being rendered
            if ( nYChunkSize < poDS->GetRasterYSize() )
            {
                eErr = 
                    poDS->RasterIO( GF_Read, 0, iY,
                                    poDS->GetRasterXSize(), nThisYChunkSize, 
                                    pabyChunkBuf,
                                    poDS->GetRasterXSize(), nThisYChunkSize,
                                    eType, nBandCount, panBandList, 0, 0, 0 );
                if( eErr != CE_None )
                    break;
            }

            double *padfAttrValues = (double *) VSIMalloc(sizeof(double) * nBandCount);
            while( (poFeat = poLayer->GetNextFeature()) != NULL )
            {
                OGRGeometry *poGeom = poFeat->GetGeometryRef();

                if ( pszBurnAttribute )
                {
                    int         iBand;
                    double      dfAttrValue;

                    dfAttrValue = poFeat->GetFieldAsDouble( iBurnField );
                    for (iBand = 0 ; iBand < nBandCount ; iBand++)
                        padfAttrValues[iBand] = dfAttrValue;

                    padfBurnValues = padfAttrValues;
                }
                
                gv_rasterize_one_shape( pabyChunkBuf, iY,
                                        poDS->GetRasterXSize(),
                                        nThisYChunkSize,
                                        nBandCount, eType, bAllTouched, poGeom,
                                        padfBurnValues, eBurnValueSource,
                                        pfnTransformer, pTransformArg );

                delete poFeat;
            }
            VSIFree( padfAttrValues );

            // Only write image if not a single chunk is being rendered
            if ( nYChunkSize < poDS->GetRasterYSize() )
            {
                eErr = 
                    poDS->RasterIO( GF_Write, 0, iY,
                                    poDS->GetRasterXSize(), nThisYChunkSize, 
                                    pabyChunkBuf,
                                    poDS->GetRasterXSize(), nThisYChunkSize, 
                                    eType, nBandCount, panBandList, 0, 0, 0 );
            }

            poLayer->ResetReading();

            if( !pfnProgress((iY+nThisYChunkSize)/((double)poDS->GetRasterYSize()),
                             "", pProgressArg) )
            {
                CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
                eErr = CE_Failure;
            }
        }

        if ( bNeedToFreeTransformer )
        {
            GDALDestroyTransformer( pTransformArg );
            pTransformArg = NULL;
            pfnTransformer = NULL;
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Write out the image once for all layers if user requested       */
/*      to render the whole raster in single chunk.                     */
/* -------------------------------------------------------------------- */
    if ( nYChunkSize == poDS->GetRasterYSize() )
    {
        poDS->RasterIO( GF_Write, 0, 0,
                                poDS->GetRasterXSize(), nYChunkSize, 
                                pabyChunkBuf,
                                poDS->GetRasterXSize(), nYChunkSize, 
                                eType, nBandCount, panBandList, 0, 0, 0 );
    }

/* -------------------------------------------------------------------- */
/*      cleanup                                                         */
/* -------------------------------------------------------------------- */
    VSIFree( pabyChunkBuf );
    
    return eErr;
}

#endif /* def OGR_ENABLED */

/************************************************************************/
/*                        GDALRasterizeLayersBuf()                      */
/************************************************************************/

/**
 * Burn geometries from the specified list of layer into raster.
 *
 * Rasterize all the geometric objects from a list of layers into supplied
 * raster buffer.  The layers are passed as an array of OGRLayerH handlers.
 *
 * If the geometries are in the georferenced coordinates of the raster
 * dataset, then the pfnTransform may be passed in NULL and one will be
 * derived internally from the geotransform of the dataset.  The transform
 * needs to transform the geometry locations into pixel/line coordinates
 * of the target raster.
 *
 * The output raster may be of any GDAL supported datatype, though currently
 * internally the burning is done either as GDT_Byte or GDT_Float32.  This
 * may be improved in the future. 
 *
 * @param pData pointer to the output data array.
 *
 * @param nBufXSize width of the output data array in pixels.
 *
 * @param nBufYSize height of the output data array in pixels. 
 *
 * @param eBufType data type of the output data array.
 *
 * @param nPixelSpace The byte offset from the start of one pixel value in
 * pData to the start of the next pixel value within a scanline.  If defaulted
 * (0) the size of the datatype eBufType is used.
 *
 * @param nLineSpace The byte offset from the start of one scanline in
 * pData to the start of the next.  If defaulted the size of the datatype
 * eBufType * nBufXSize is used.
 *
 * @param nLayerCount the number of layers being passed in pahLayers array.
 *
 * @param pahLayers the array of layers to burn in. 
 *
 * @param pszDstProjection WKT defining the coordinate system of the target
 * raster.
 *
 * @param padfDstGeoTransform geotransformation matrix of the target raster.
 *
 * @param pfnTransformer transformation to apply to geometries to put into 
 * pixel/line coordinates on raster.  If NULL a geotransform based one will
 * be created internally.
 *
 * @param pTransformerArg callback data for transformer.
 *
 * @param dfBurnValue the value to burn into the raster.  
 *
 * @param papszOption special options controlling rasterization:
 * <dl>
 * <dt>"ATTRIBUTE":</dt> <dd>Identifies an attribute field on the features to be
 * used for a burn in value. The value will be burned into all output
 * bands. If specified, padfLayerBurnValues will not be used and can be a NULL
 * pointer.</dd>
 * <dt>"ALL_TOUCHED":</dt> <dd>May be set to TRUE to set all pixels touched 
 * by the line or polygons, not just those whose center is within the polygon
 * or that are selected by brezenhams line algorithm.  Defaults to FALSE.</dd>
 * </dl>
 * <dt>"BURN_VALUE_FROM":</dt> <dd>May be set to "Z" to use
 * the Z values of the geometries. dfBurnValue or the attribute field value is
 * added to this before burning. In default case dfBurnValue is burned as it
 * is. This is implemented properly only for points and lines for now. Polygons
 * will be burned using the Z value from the first point. The M value may
 * be supported in the future.</dd>
 * </dl>
 *
 * @param pfnProgress the progress function to report completion.
 *
 * @param pProgressArg callback data for progress function.
 *
 *
 * @return CE_None on success or CE_Failure on error.
 */

#ifdef OGR_ENABLED

CPLErr GDALRasterizeLayersBuf( void *pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nPixelSpace, int nLineSpace,
                               int nLayerCount, OGRLayerH *pahLayers,
                               const char *pszDstProjection,
                               double *padfDstGeoTransform,
                               GDALTransformerFunc pfnTransformer, 
                               void *pTransformArg, double dfBurnValue,
                               char **papszOptions,
                               GDALProgressFunc pfnProgress,
                               void *pProgressArg )

{
/* -------------------------------------------------------------------- */
/*      If pixel and line spaceing are defaulted assign reasonable      */
/*      value assuming a packed buffer.                                 */
/* -------------------------------------------------------------------- */
    if( nPixelSpace == 0 )
        nPixelSpace = GDALGetDataTypeSize( eBufType ) / 8;
    
    if( nLineSpace == 0 )
        nLineSpace = nPixelSpace * nBufXSize;

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      Do some rudimentary arg checking.                               */
/* -------------------------------------------------------------------- */
    if( nLayerCount == 0 )
        return CE_None;

    int bAllTouched = CSLFetchBoolean( papszOptions, "ALL_TOUCHED", FALSE );
    const char *pszOpt = CSLFetchNameValue( papszOptions, "BURN_VALUE_FROM" );
    GDALBurnValueSrc eBurnValueSource = GBV_UserBurnValue;
    if( pszOpt )
    {
        if( EQUAL(pszOpt,"Z"))
            eBurnValueSource = GBV_Z;
        /*else if( EQUAL(pszOpt,"M"))
            eBurnValueSource = GBV_M;*/
    }

/* ==================================================================== */
/*      Read thes pecified layers transfoming and rasterizing           */
/*      geometries.                                                     */
/* ==================================================================== */
    CPLErr      eErr = CE_None;
    int         iLayer;
    const char  *pszBurnAttribute =
        CSLFetchNameValue( papszOptions, "ATTRIBUTE" );

    pfnProgress( 0.0, NULL, pProgressArg );

    for( iLayer = 0; iLayer < nLayerCount; iLayer++ )
    {
        int         iBurnField = -1;
        OGRLayer    *poLayer = (OGRLayer *) pahLayers[iLayer];

        if ( !poLayer )
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "Layer element number %d is NULL, skipping.\n", iLayer );
            continue;
        }

/* -------------------------------------------------------------------- */
/*      If the layer does not contain any features just skip it.        */
/*      Do not force the feature count, so if driver doesn't know       */
/*      exact number of features, go down the normal way.               */
/* -------------------------------------------------------------------- */
        if ( poLayer->GetFeatureCount(FALSE) == 0 )
            continue;

        if ( pszBurnAttribute )
        {
            iBurnField =
                poLayer->GetLayerDefn()->GetFieldIndex( pszBurnAttribute );
            if ( iBurnField == -1 )
            {
                CPLError( CE_Warning, CPLE_AppDefined, 
                          "Failed to find field %s on layer %s, skipping.\n",
                          pszBurnAttribute, 
                          poLayer->GetLayerDefn()->GetName() );
                continue;
            }
        }

/* -------------------------------------------------------------------- */
/*      If we have no transformer, create the one from input file       */
/*      projection. Note that each layer can be georefernced            */
/*      separately.                                                     */
/* -------------------------------------------------------------------- */
        int bNeedToFreeTransformer = FALSE;

        if( pfnTransformer == NULL )
        {
            char    *pszProjection = NULL;
            bNeedToFreeTransformer = TRUE;

            OGRSpatialReference *poSRS = poLayer->GetSpatialRef();
            if ( !poSRS )
            {
                CPLError( CE_Warning, CPLE_AppDefined, 
                          "Failed to fetch spatial reference on layer %s "
                          "to build transformer, assuming matching coordinate systems.\n",
                          poLayer->GetLayerDefn()->GetName() );
            }
            else
                poSRS->exportToWkt( &pszProjection );

            pTransformArg =
                GDALCreateGenImgProjTransformer3( pszProjection, NULL,
                                                  pszDstProjection,
                                                  padfDstGeoTransform );
            pfnTransformer = GDALGenImgProjTransform;

            CPLFree( pszProjection );
        }

        OGRFeature *poFeat;

        poLayer->ResetReading();

        while( (poFeat = poLayer->GetNextFeature()) != NULL )
        {
            OGRGeometry *poGeom = poFeat->GetGeometryRef();

            if ( pszBurnAttribute )
                dfBurnValue = poFeat->GetFieldAsDouble( iBurnField );
            
            gv_rasterize_one_shape( (unsigned char *) pData, 0,
                                    nBufXSize, nBufYSize,
                                    1, eBufType, bAllTouched, poGeom,
                                    &dfBurnValue, eBurnValueSource,
                                    pfnTransformer, pTransformArg );

            delete poFeat;
        }

        poLayer->ResetReading();

        if( !pfnProgress(1, "", pProgressArg) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }

        if ( bNeedToFreeTransformer )
        {
            GDALDestroyTransformer( pTransformArg );
            pTransformArg = NULL;
            pfnTransformer = NULL;
        }
    }
    
    return eErr;
}

#endif /* def OGR_ENABLED */
