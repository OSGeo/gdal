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
/*                            gvBurnPoint()                             */
/************************************************************************/

void gvBurnPoint( void *pCBData, int nY, int nX )

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

            *pbyInsert = (unsigned char) psInfo->padfBurnValue[iBand];
        }
    }
    else
    {
        for( iBand = 0; iBand < psInfo->nBands; iBand++ )
        {
            float   *pfInsert = ((float *) psInfo->pabyChunkBuf) 
                + iBand * psInfo->nXSize * psInfo->nYSize
                + nY * psInfo->nXSize + nX;

            *pfInsert = (float) psInfo->padfBurnValue[iBand];
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

    if ( eFlatType == wkbPoint )
    {
        OGRPoint    *poPoint = (OGRPoint *) poShape;
        int nNewCount = aPointX.size() + 1;

        aPointX.reserve( nNewCount );
        aPointY.reserve( nNewCount );
        aPointX.push_back( poPoint->getX() );
        aPointY.push_back( poPoint->getY() );
        aPartSize.push_back( 1 );
    }
    else if ( eFlatType == wkbLineString )
    {
        OGRLineString   *poLine = (OGRLineString *) poShape;
        int nCount = poLine->getNumPoints();
        int nNewCount = aPointX.size() + nCount;

        aPointX.reserve( nNewCount );
        aPointY.reserve( nNewCount );
        for ( i = nCount - 1; i >= 0; i-- )
        {
            aPointX.push_back( poLine->getX(i) );
            aPointY.push_back( poLine->getY(i) );
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
        for ( i = nCount - 1; i >= 0; i-- )
        {
            aPointX.push_back( poRing->getX(i) );
            aPointY.push_back( poRing->getY(i) );
        }
        aPartSize.push_back( nCount );
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
    
    else if( eFlatType == wkbMultiPoint
             || eFlatType == wkbMultiLineString
             || eFlatType == wkbMultiPolygon
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
                               gvBurnPoint, &sInfo );
            break;
        case wkbLineString:
        case wkbMultiLineString:
            GDALdllImageLine( sInfo.nXSize, nYSize, 
                              aPartSize.size(), &(aPartSize[0]), 
                              &(aPointX[0]), &(aPointY[0]),
                              gvBurnPoint, &sInfo );
            break;
        default:
            GDALdllImageFilledPolygon( sInfo.nXSize, nYSize, 
                                       aPartSize.size(), &(aPartSize[0]), 
                                       &(aPointX[0]), &(aPointY[0]),
                                       gvBurnScanline, &sInfo );
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
 * @param papszOption special options controlling rasterization, currently
 * none are defined.
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
            gv_rasterize_new_one_shape( pabyChunkBuf, iY, nThisYChunkSize,
                                        nBandCount, eType, poDS,
                                        (OGRGeometry *) pahGeometries[iShape],
                                        padfGeomBurnValue + iShape*nBandCount, 
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
 * Burn geometries from the specified layer into raster.
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
 * <dt>"ATTRIBUTE":<dt> <dd>Identifies an attribute field on the features to be
 * used for a burn in value. The value will be burned into all output
 * bands. If specified, padfLayerBurnValues will not be used and can be a NULL
 * pointer.</dd>
 * <dt>"CHUNKYSIZE":<dt> <dd>The height in lines of the chunk to operate on.
 * The larger the chunk size the less times we need to make a pass through all
 * the shapes. If it is not set or set to zero the default chunk size will be
 * used. Default size will be estimated based on the GDAL cache buffer size
 * using formula: cache_size_bytes/scanline_size_bytes, so the chunk will
 * not exceed the cache.</dd>
 * </dl>
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
    int            iY;
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
        nYChunkSize = GDALGetCacheMax() / nScanlineBytes;

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

/* ==================================================================== */
/*      Read thespecified layers transfoming and rasterizing            */
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
            char    *pszProjection;
            bNeedToFreeTransformer = TRUE;

            OGRSpatialReference *poSRS = poLayer->GetSpatialRef();
            if ( !poSRS )
            {
                CPLError( CE_Warning, CPLE_AppDefined, 
                          "Failed to fetch spatial reference on layer %s "
                          "to build transformer, skipping.\n",
                          poLayer->GetLayerDefn()->GetName() );
                continue;
            }

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
       for( iY = 0; 
            iY < poDS->GetRasterYSize() && eErr == CE_None; 
            iY += nYChunkSize )
        {
            int	nThisYChunkSize;

            nThisYChunkSize = nYChunkSize;
            if( nThisYChunkSize + iY > poDS->GetRasterYSize() )
                nThisYChunkSize = poDS->GetRasterYSize() - iY;

            eErr = 
                poDS->RasterIO( GF_Read, 0, iY,
                                poDS->GetRasterXSize(), nThisYChunkSize, 
                                pabyChunkBuf,
                                poDS->GetRasterXSize(), nThisYChunkSize,
                                eType, nBandCount, panBandList, 0, 0, 0 );
            if( eErr != CE_None )
                break;

            while( (poFeat = poLayer->GetNextFeature()) != NULL )
            {
                OGRGeometry *poGeom = poFeat->GetGeometryRef();
                double      dfBurnValue;

                if ( pszBurnAttribute )
                {
                    dfBurnValue = poFeat->GetFieldAsDouble( iBurnField );
                    padfBurnValues = &dfBurnValue;
                }
                
                gv_rasterize_new_one_shape( pabyChunkBuf, iY, nThisYChunkSize,
                                            nBandCount, eType, poDS, poGeom,
                                            padfBurnValues,
                                            pfnTransformer, pTransformArg );

                delete poFeat;
            }

            eErr = 
                poDS->RasterIO( GF_Write, 0, iY,
                                poDS->GetRasterXSize(), nThisYChunkSize, 
                                pabyChunkBuf,
                                poDS->GetRasterXSize(), nThisYChunkSize, 
                                eType, nBandCount, panBandList, 0, 0, 0 );

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
/*      cleanup                                                         */
/* -------------------------------------------------------------------- */
    VSIFree( pabyChunkBuf );
    
    return eErr;
}

#endif /* def OGR_ENABLED */
