/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Vector rasterization.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "gdal_alg.h"
#include "gdal_alg_priv.h"

#include <climits>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                           gvBurnScanline()                           */
/************************************************************************/
static
void gvBurnScanline( void *pCBData, int nY, int nXStart, int nXEnd,
                     double dfVariant )

{
    if( nXStart > nXEnd )
        return;

    GDALRasterizeInfo *psInfo = static_cast<GDALRasterizeInfo *>(pCBData);

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
        for( int iBand = 0; iBand < psInfo->nBands; iBand++ )
        {
            unsigned char nBurnValue = (unsigned char)
                ( psInfo->padfBurnValue[iBand] +
                  ( (psInfo->eBurnValueSource == GBV_UserBurnValue)?
                             0 : dfVariant ) );

            unsigned char *pabyInsert =
                psInfo->pabyChunkBuf
                + iBand * psInfo->nXSize * psInfo->nYSize
                + nY * psInfo->nXSize + nXStart;

            if( psInfo->eMergeAlg == GRMA_Add ) {
                int nPixels = nXEnd - nXStart + 1;
                while( nPixels-- > 0 )
                    *(pabyInsert++) += nBurnValue;
            } else {
                memset( pabyInsert, nBurnValue, nXEnd - nXStart + 1 );
            }
        }
    }
    else if( psInfo->eType == GDT_Float64 )
    {
        for( int iBand = 0; iBand < psInfo->nBands; iBand++ )
        {
            int nPixels = nXEnd - nXStart + 1;
            const double dfBurnValue =
                ( psInfo->padfBurnValue[iBand] +
                  ( (psInfo->eBurnValueSource == GBV_UserBurnValue)?
                             0 : dfVariant ) );

            double *padfInsert =
                ((double *) psInfo->pabyChunkBuf)
                + iBand * psInfo->nXSize * psInfo->nYSize
                + nY * psInfo->nXSize + nXStart;

            if( psInfo->eMergeAlg == GRMA_Add ) {
                while( nPixels-- > 0 )
                    *(padfInsert++) += dfBurnValue;
            } else {
                while( nPixels-- > 0 )
                    *(padfInsert++) = dfBurnValue;
            }
        }
    }
    else {
        CPLAssert(false);
    }
}

/************************************************************************/
/*                            gvBurnPoint()                             */
/************************************************************************/
static
void gvBurnPoint( void *pCBData, int nY, int nX, double dfVariant )

{
    GDALRasterizeInfo *psInfo = static_cast<GDALRasterizeInfo *>(pCBData);

    CPLAssert( nY >= 0 && nY < psInfo->nYSize );
    CPLAssert( nX >= 0 && nX < psInfo->nXSize );

    if( psInfo->eType == GDT_Byte )
    {
        for( int iBand = 0; iBand < psInfo->nBands; iBand++ )
        {
            unsigned char *pbyInsert = psInfo->pabyChunkBuf
                                      + iBand * psInfo->nXSize * psInfo->nYSize
                                      + nY * psInfo->nXSize + nX;
            double dfVal;
            if( psInfo->eMergeAlg == GRMA_Add ) {
                dfVal = *pbyInsert + ( psInfo->padfBurnValue[iBand] +
                          ( (psInfo->eBurnValueSource == GBV_UserBurnValue)?
                             0 : dfVariant ) );
            } else {
                dfVal = psInfo->padfBurnValue[iBand] +
                          ( (psInfo->eBurnValueSource == GBV_UserBurnValue)?
                             0 : dfVariant );
            }
            if( dfVal > 255.0 )
                *pbyInsert = 255;
            else if( dfVal < 0.0 )
                *pbyInsert = 0;
            else
                *pbyInsert = (unsigned char)( dfVal );
        }
    }
    else if( psInfo->eType == GDT_Float64 )
    {
        for( int iBand = 0; iBand < psInfo->nBands; iBand++ )
        {
            double *pdfInsert = (double *) psInfo->pabyChunkBuf
                                + iBand * psInfo->nXSize * psInfo->nYSize
                                + nY * psInfo->nXSize + nX;

            if( psInfo->eMergeAlg == GRMA_Add ) {
                *pdfInsert += ( psInfo->padfBurnValue[iBand] +
                         ( (psInfo->eBurnValueSource == GBV_UserBurnValue)?
                            0 : dfVariant ) );
            } else {
                *pdfInsert = ( psInfo->padfBurnValue[iBand] +
                         ( (psInfo->eBurnValueSource == GBV_UserBurnValue)?
                            0 : dfVariant ) );
            }
        }
    }
    else {
        CPLAssert(false);
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
    if( poShape == NULL || poShape->IsEmpty() )
        return;

    const OGRwkbGeometryType eFlatType = wkbFlatten(poShape->getGeometryType());

    if( eFlatType == wkbPoint )
    {
        OGRPoint *poPoint = dynamic_cast<OGRPoint *>(poShape);
        CPLAssert(poPoint != NULL);
        const size_t nNewCount = aPointX.size() + 1;

        aPointX.reserve( nNewCount );
        aPointY.reserve( nNewCount );
        aPointX.push_back( poPoint->getX() );
        aPointY.push_back( poPoint->getY() );
        aPartSize.push_back( 1 );
        if( eBurnValueSrc != GBV_UserBurnValue )
        {
            // TODO(schwehr): Why not have the option for M r18164?
            // switch( eBurnValueSrc )
            // {
            // case GBV_Z:*/
            aPointVariant.reserve( nNewCount );
            aPointVariant.push_back( poPoint->getZ() );
            // break;
            // case GBV_M:
            //    aPointVariant.reserve( nNewCount );
            //    aPointVariant.push_back( poPoint->getM() );
        }
    }
    else if( eFlatType == wkbLineString )
    {
        OGRLineString *poLine = dynamic_cast<OGRLineString *>(poShape);
        CPLAssert(poLine != NULL);
        const int nCount = poLine->getNumPoints();
        const size_t nNewCount = aPointX.size() + static_cast<size_t>(nCount);

        aPointX.reserve( nNewCount );
        aPointY.reserve( nNewCount );
        if( eBurnValueSrc != GBV_UserBurnValue )
            aPointVariant.reserve( nNewCount );
        for( int i = nCount - 1; i >= 0; i-- )
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
    else if( EQUAL(poShape->getGeometryName(), "LINEARRING") )
    {
        OGRLinearRing *poRing = dynamic_cast<OGRLinearRing *>(poShape);
        CPLAssert(poRing != NULL);
        const int nCount = poRing->getNumPoints();
        const size_t nNewCount = aPointX.size() + static_cast<size_t>(nCount);

        aPointX.reserve( nNewCount );
        aPointY.reserve( nNewCount );
        if( eBurnValueSrc != GBV_UserBurnValue )
            aPointVariant.reserve( nNewCount );
        int i = nCount - 1;  // Used after for.
        for( ; i >= 0; i-- )
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
        OGRPolygon *poPolygon = dynamic_cast<OGRPolygon *>(poShape);
        CPLAssert(poPolygon != NULL);

        GDALCollectRingsFromGeometry( poPolygon->getExteriorRing(),
                                      aPointX, aPointY, aPointVariant,
                                      aPartSize, eBurnValueSrc );

        for( int i = 0; i < poPolygon->getNumInteriorRings(); i++ )
            GDALCollectRingsFromGeometry( poPolygon->getInteriorRing(i),
                                          aPointX, aPointY, aPointVariant,
                                          aPartSize, eBurnValueSrc );
    }
    else if( eFlatType == wkbMultiPoint
             || eFlatType == wkbMultiLineString
             || eFlatType == wkbMultiPolygon
             || eFlatType == wkbGeometryCollection )
    {
        OGRGeometryCollection *poGC = dynamic_cast<OGRGeometryCollection *>(poShape);
        CPLAssert(poGC != NULL);

        for( int i = 0; i < poGC->getNumGeometries(); i++ )
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
gv_rasterize_one_shape( unsigned char *pabyChunkBuf, int nXOff, int nYOff,
                        int nXSize, int nYSize,
                        int nBands, GDALDataType eType, int bAllTouched,
                        OGRGeometry *poShape, double *padfBurnValue,
                        GDALBurnValueSrc eBurnValueSrc,
                        GDALRasterMergeAlg eMergeAlg,
                        GDALTransformerFunc pfnTransformer,
                        void *pTransformArg )

{
    if( poShape == NULL || poShape->IsEmpty() )
        return;

    GDALRasterizeInfo sInfo;
    sInfo.nXSize = nXSize;
    sInfo.nYSize = nYSize;
    sInfo.nBands = nBands;
    sInfo.pabyChunkBuf = pabyChunkBuf;
    sInfo.eType = eType;
    sInfo.padfBurnValue = padfBurnValue;
    sInfo.eBurnValueSource = eBurnValueSrc;
    sInfo.eMergeAlg = eMergeAlg;

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
        int *panSuccess =
            static_cast<int *>(CPLCalloc(sizeof(int), aPointX.size()));

        // TODO: We need to add all appropriate error checking at some point.
        pfnTransformer( pTransformArg, FALSE, static_cast<int>(aPointX.size()),
                        &(aPointX[0]), &(aPointY[0]), NULL, panSuccess );
        CPLFree( panSuccess );
    }

/* -------------------------------------------------------------------- */
/*      Shift to account for the buffer offset of this buffer.          */
/* -------------------------------------------------------------------- */
    for( unsigned int i = 0; i < aPointX.size(); i++ )
        aPointX[i] -= nXOff;
    for( unsigned int i = 0; i < aPointY.size(); i++ )
        aPointY[i] -= nYOff;

/* -------------------------------------------------------------------- */
/*      Perform the rasterization.                                      */
/*      According to the C++ Standard/23.2.4, elements of a vector are  */
/*      stored in continuous memory block.                              */
/* -------------------------------------------------------------------- */

    // TODO - mloskot: Check if vectors are empty, otherwise it may
    // lead to undefined behavior by returning non-referencable pointer.
    // if( !aPointX.empty() )
    //    // Fill polygon.
    // else
    //    // How to report this problem?
    switch( wkbFlatten(poShape->getGeometryType()) )
    {
      case wkbPoint:
      case wkbMultiPoint:
        GDALdllImagePoint( sInfo.nXSize, nYSize,
                           static_cast<int>(aPartSize.size()), &(aPartSize[0]),
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
                                          static_cast<int>(aPartSize.size()),
                                          &(aPartSize[0]),
                                          &(aPointX[0]), &(aPointY[0]),
                                          (eBurnValueSrc == GBV_UserBurnValue)?
                                          NULL : &(aPointVariant[0]),
                                          gvBurnPoint, &sInfo );
          else
              GDALdllImageLine( sInfo.nXSize, nYSize,
                                static_cast<int>(aPartSize.size()),
                                &(aPartSize[0]),
                                &(aPointX[0]), &(aPointY[0]),
                                (eBurnValueSrc == GBV_UserBurnValue)?
                                NULL : &(aPointVariant[0]),
                                gvBurnPoint, &sInfo );
      }
      break;

      default:
      {
          GDALdllImageFilledPolygon(
              sInfo.nXSize, nYSize,
              static_cast<int>(aPartSize.size()), &(aPartSize[0]),
              &(aPointX[0]), &(aPointY[0]),
              (eBurnValueSrc == GBV_UserBurnValue)?
              NULL : &(aPointVariant[0]),
              gvBurnScanline, &sInfo );
          if( bAllTouched )
          {
              // Reverting the variants to the first value because the
              // polygon is filled using the variant from the first point of
              // the first segment. Should be removed when the code to full
              // polygons more appropriately is added.
              if( eBurnValueSrc == GBV_UserBurnValue )
              {
                  GDALdllImageLineAllTouched(
                      sInfo.nXSize, nYSize,
                      static_cast<int>(aPartSize.size()), &(aPartSize[0]),
                      &(aPointX[0]), &(aPointY[0]),
                      NULL,
                      gvBurnPoint, &sInfo );
              }
              else
              {
                  for( unsigned int i = 0, n = 0;
                       i < static_cast<unsigned int>(aPartSize.size());
                       i++ )
                  {
                      for( int j = 0; j < aPartSize[i]; j++ )
                          aPointVariant[n++] = aPointVariant[0];
                  }

                  GDALdllImageLineAllTouched(
                      sInfo.nXSize, nYSize,
                      static_cast<int>(aPartSize.size()), &(aPartSize[0]),
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
/*                        GDALRasterizeOptions()                        */
/*                                                                      */
/*      Recognise a few rasterize options used by all three entry       */
/*      points.                                                         */
/************************************************************************/

static CPLErr GDALRasterizeOptions( char **papszOptions,
                                    int *pbAllTouched,
                                    GDALBurnValueSrc *peBurnValueSource,
                                    GDALRasterMergeAlg *peMergeAlg,
                                    GDALRasterizeOptim *peOptim)
{
    *pbAllTouched = CPLFetchBool( papszOptions, "ALL_TOUCHED", false );

    const char *pszOpt = CSLFetchNameValue( papszOptions, "BURN_VALUE_FROM" );
    *peBurnValueSource = GBV_UserBurnValue;
    if( pszOpt )
    {
        if( EQUAL(pszOpt, "Z"))
        {
            *peBurnValueSource = GBV_Z;
        }
        // else if( EQUAL(pszOpt, "M"))
        //     eBurnValueSource = GBV_M;
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unrecognized value '%s' for BURN_VALUE_FROM.",
                      pszOpt );
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      MERGE_ALG=[REPLACE]/ADD                                         */
/* -------------------------------------------------------------------- */
    *peMergeAlg = GRMA_Replace;
    pszOpt = CSLFetchNameValue( papszOptions, "MERGE_ALG" );
    if( pszOpt )
    {
        if( EQUAL(pszOpt, "ADD"))
        {
            *peMergeAlg = GRMA_Add;
        }
        else if( EQUAL(pszOpt, "REPLACE"))
        {
            *peMergeAlg = GRMA_Replace;
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unrecognized value '%s' for MERGE_ALG.",
                      pszOpt );
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      OPTIM=[AUTO]/RASTER/VECTOR                               */
/* -------------------------------------------------------------------- */
    *peOptim = GRO_Auto;
    pszOpt = CSLFetchNameValue( papszOptions, "OPTIM" );
    if( pszOpt )
    {
        if( EQUAL(pszOpt, "RASTER"))
        {
            *peOptim = GRO_Raster;
        }
        else if( EQUAL(pszOpt, "VECTOR"))
        {
            *peOptim = GRO_Vector;
        }
        else if( EQUAL(pszOpt, "AUTO"))
        {
            *peOptim = GRO_Auto;
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unrecognized value '%s' for OPTIM.",
                      pszOpt );
            return CE_Failure;
        }
    }

    return CE_None;
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
 * If the geometries are in the georeferenced coordinates of the raster
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
 * @param pTransformArg callback data for transformer.
 * @param padfGeomBurnValue the array of values to burn into the raster.
 * There should be nBandCount values for each geometry.
 * @param papszOptions special options controlling rasterization
 * <ul>
 * <li>"ALL_TOUCHED": May be set to TRUE to set all pixels touched
 * by the line or polygons, not just those whose center is within the polygon
 * or that are selected by brezenhams line algorithm.  Defaults to FALSE.</li>
 * <li>"BURN_VALUE_FROM": May be set to "Z" to use the Z values of the
 * geometries. dfBurnValue is added to this before burning.
 * Defaults to GDALBurnValueSrc.GBV_UserBurnValue in which case just the
 * dfBurnValue is burned. This is implemented only for points and lines for
 * now. The M value may be supported in the future.</li>
 * <li>"MERGE_ALG": May be REPLACE (the default) or ADD.  REPLACE results in
 * overwriting of value, while ADD adds the new value to the existing raster,
 * suitable for heatmaps for instance.</li>
 * <li>"CHUNKYSIZE": The height in lines of the chunk to operate on.
 * The larger the chunk size the less times we need to make a pass through all
 * the shapes. If it is not set or set to zero the default chunk size will be
 * used. Default size will be estimated based on the GDAL cache buffer size
 * using formula: cache_size_bytes/scanline_size_bytes, so the chunk will
 * not exceed the cache. Not used in OPTIM=RASTER mode.</li>
 * </ul>
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
    VALIDATE_POINTER1( hDS, "GDALRasterizeGeometries", CE_Failure);

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

    GDALDataset *poDS = reinterpret_cast<GDALDataset *>(hDS);

/* -------------------------------------------------------------------- */
/*      Do some rudimentary arg checking.                               */
/* -------------------------------------------------------------------- */
    if( nBandCount == 0 || nGeomCount == 0 )
    {
        pfnProgress(1.0, "", pProgressArg );
        return CE_None;
    }

    // Prototype band.
    GDALRasterBand *poBand = poDS->GetRasterBand( panBandList[0] );
    if( poBand == NULL )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Options                                                         */
/* -------------------------------------------------------------------- */
    int bAllTouched = FALSE;
    GDALBurnValueSrc eBurnValueSource = GBV_UserBurnValue;
    GDALRasterMergeAlg eMergeAlg = GRMA_Replace;
    GDALRasterizeOptim eOptim = GRO_Auto;
    if( GDALRasterizeOptions(papszOptions, &bAllTouched,
                             &eBurnValueSource, &eMergeAlg,
                             &eOptim) == CE_Failure )
    {
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      If we have no transformer, assume the geometries are in file    */
/*      georeferenced coordinates, and create a transformer to          */
/*      convert that to pixel/line coordinates.                         */
/*                                                                      */
/*      We really just need to apply an affine transform, but for       */
/*      simplicity we use the more general GenImgProjTransformer.       */
/* -------------------------------------------------------------------- */
    bool bNeedToFreeTransformer = false;

    if( pfnTransformer == NULL )
    {
        bNeedToFreeTransformer = true;

        char** papszTransformerOptions = NULL;
        double adfGeoTransform[6] = { 0.0 };
        if( poDS->GetGeoTransform( adfGeoTransform ) != CE_None &&
            poDS->GetGCPCount() == 0 &&
            poDS->GetMetadata("RPC") == NULL )
        {
            papszTransformerOptions = CSLSetNameValue(
                papszTransformerOptions, "DST_METHOD", "NO_GEOTRANSFORM");
        }

        pTransformArg =
            GDALCreateGenImgProjTransformer2( NULL, hDS,
                                                papszTransformerOptions );
        CSLDestroy( papszTransformerOptions );

        pfnTransformer = GDALGenImgProjTransform;
        if( pTransformArg == NULL )
        {
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Choice of optimisation in auto mode. Use vector optim :         */
/*      1) if output is tiled                                           */
/*      2) if large number of features is present (>10000)              */
/*      3) if the nb of pixels > 50 * nb of features (not-too-small ft) */
/* -------------------------------------------------------------------- */
    int nXBlockSize, nYBlockSize;
    poBand->GetBlockSize(&nXBlockSize, &nYBlockSize);

    if( eOptim == GRO_Auto )
    {
        eOptim = GRO_Raster;
        // TODO make more tests with various inputs/outputs to ajust the parameters
        if( nYBlockSize > 1 && nGeomCount > 10000 && (poBand->GetXSize() * static_cast<long long>(poBand->GetYSize()) / nGeomCount > 50) )
        {
            eOptim = GRO_Vector;
            CPLDebug("GDAL", "The vector optim has been chosen automatically");
        }
    }


/* -------------------------------------------------------------------- */
/*      The original algorithm                                          */
/*      Optimized for raster writing                                    */
/*      (optimal on a small number of large vectors)                    */
/* -------------------------------------------------------------------- */
    unsigned char *pabyChunkBuf;
    CPLErr eErr = CE_None;
    if( eOptim == GRO_Raster )
    {
/* -------------------------------------------------------------------- */
/*      Establish a chunksize to operate on.  The larger the chunk      */
/*      size the less times we need to make a pass through all the      */
/*      shapes.                                                         */
/* -------------------------------------------------------------------- */
        const GDALDataType eType =
            poBand->GetRasterDataType() == GDT_Byte ? GDT_Byte : GDT_Float64;
    
        const int nScanlineBytes =
            nBandCount * poDS->GetRasterXSize() * GDALGetDataTypeSizeBytes(eType);
    
        int nYChunkSize = 0;
        const char *pszYChunkSize = CSLFetchNameValue(papszOptions, "CHUNKYSIZE");
        if( pszYChunkSize == NULL || ((nYChunkSize = atoi(pszYChunkSize))) == 0)
        {
            const GIntBig nYChunkSize64 = GDALGetCacheMax64() / nScanlineBytes;
            nYChunkSize = (nYChunkSize64 > INT_MAX) ? INT_MAX 
                          : static_cast<int>(nYChunkSize64);
        }
    
        if( nYChunkSize < 1 )
            nYChunkSize = 1;
        if( nYChunkSize > poDS->GetRasterYSize() )
            nYChunkSize = poDS->GetRasterYSize();
    
        CPLDebug( "GDAL", "Rasterizer operating on %d swaths of %d scanlines.",
                  (poDS->GetRasterYSize() + nYChunkSize - 1) / nYChunkSize,
                  nYChunkSize );
    
        pabyChunkBuf = static_cast<unsigned char *>(
            VSI_MALLOC2_VERBOSE(nYChunkSize, nScanlineBytes));
        if( pabyChunkBuf == NULL )
        {
            if( bNeedToFreeTransformer )
                GDALDestroyTransformer( pTransformArg );
            return CE_Failure;
        }

/* ==================================================================== */
/*      Loop over image in designated chunks.                           */
/* ==================================================================== */
        pfnProgress( 0.0, NULL, pProgressArg );
    
        for( int iY = 0;
             iY < poDS->GetRasterYSize() && eErr == CE_None;
             iY += nYChunkSize )
        {
            int nThisYChunkSize = nYChunkSize;
            if( nThisYChunkSize + iY > poDS->GetRasterYSize() )
                nThisYChunkSize = poDS->GetRasterYSize() - iY;
    
            eErr =
                poDS->RasterIO(GF_Read,
                               0, iY, poDS->GetRasterXSize(), nThisYChunkSize,
                               pabyChunkBuf,
                               poDS->GetRasterXSize(), nThisYChunkSize,
                               eType, nBandCount, panBandList,
                               0, 0, 0, NULL);
            if( eErr != CE_None )
                break;
    
            for( int iShape = 0; iShape < nGeomCount; iShape++ )
            {
                gv_rasterize_one_shape( pabyChunkBuf, 0, iY,
                                        poDS->GetRasterXSize(), nThisYChunkSize,
                                        nBandCount, eType, bAllTouched,
                                        reinterpret_cast<OGRGeometry *>(
                                                            pahGeometries[iShape]),
                                        padfGeomBurnValue + iShape*nBandCount,
                                        eBurnValueSource, eMergeAlg,
                                        pfnTransformer, pTransformArg );
            }
    
            eErr =
                poDS->RasterIO( GF_Write, 0, iY,
                                poDS->GetRasterXSize(), nThisYChunkSize,
                                pabyChunkBuf,
                                poDS->GetRasterXSize(), nThisYChunkSize,
                                eType, nBandCount, panBandList, 0, 0, 0, NULL);
    
            if( !pfnProgress((iY + nThisYChunkSize) /
                             static_cast<double>(poDS->GetRasterYSize()),
                             "", pProgressArg ) )
            {
                CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
                eErr = CE_Failure;
            }
        }
    }
/* -------------------------------------------------------------------- */
/*      The new algorithm                                               */
/*      Optimized to minimize the vector computation                    */
/*      (optimal on a large number of vectors & tiled raster)           */
/* -------------------------------------------------------------------- */
    else
    {
/* -------------------------------------------------------------------- */
/*      Establish a chunksize to operate on.  Its size is defined by    */
/*      the block size of the output file.                              */
/* -------------------------------------------------------------------- */
        const int nXBlocks = (poBand->GetXSize() + nXBlockSize - 1) / nXBlockSize;
        const int nYBlocks = (poBand->GetYSize() + nYBlockSize - 1) / nYBlockSize;


        const GDALDataType eType =
            poBand->GetRasterDataType() == GDT_Byte ? GDT_Byte : GDT_Float64;

        const int nPixelSize = nBandCount * GDALGetDataTypeSizeBytes(eType);

        // rem: optimized for square blocks
        const GIntBig nbMaxBlocks64 = GDALGetCacheMax64() / nPixelSize / nYBlockSize / nXBlockSize;
        const int nbMaxBlocks = (nbMaxBlocks64 > INT_MAX ) ? INT_MAX : static_cast<int>(nbMaxBlocks64);
        const int nbBlocsX = std::max(1, std::min(static_cast<int>(sqrt(static_cast<double>(nbMaxBlocks))), nXBlocks));
        const int nbBlocsY = std::max(1, std::min(nbMaxBlocks / nbBlocsX, nYBlocks));

        const int nScanblocks =
            nXBlockSize * nbBlocsX * nYBlockSize * nbBlocsY;

        pabyChunkBuf = static_cast<unsigned char *>( VSI_MALLOC2_VERBOSE(nPixelSize, nScanblocks) );
        if( pabyChunkBuf == NULL )
        {
            if( bNeedToFreeTransformer )
                GDALDestroyTransformer( pTransformArg );
            return CE_Failure;
        }

        int * panSuccessTransform = (int *) CPLCalloc(sizeof(int), 2);
        
/* -------------------------------------------------------------------- */
/*      loop over the vectorial geometries                              */
/* -------------------------------------------------------------------- */
        pfnProgress( 0.0, NULL, pProgressArg );
        for( int iShape = 0; iShape < nGeomCount; iShape++ )
        {

            OGRGeometry * poGeometry = reinterpret_cast<OGRGeometry *>(pahGeometries[iShape]);
            if ( poGeometry == NULL || poGeometry->IsEmpty() )
              continue;
/* -------------------------------------------------------------------- */
/*      get the envelope of the geometry and transform it to pixels coo */
/* -------------------------------------------------------------------- */
            OGREnvelope psGeomEnvelope;
            poGeometry->getEnvelope(&psGeomEnvelope);
            if( pfnTransformer != NULL )
            {
                double apCorners[4];
                apCorners[0] = psGeomEnvelope.MinX;
                apCorners[1] = psGeomEnvelope.MaxX;
                apCorners[2] = psGeomEnvelope.MinY;
                apCorners[3] = psGeomEnvelope.MaxY;
                // TODO: need to add all appropriate error checking 
                pfnTransformer( pTransformArg, FALSE, 2, &(apCorners[0]),
                                &(apCorners[2]), NULL, panSuccessTransform );
                psGeomEnvelope.MinX = std::min(apCorners[0], apCorners[1]);
                psGeomEnvelope.MaxX = std::max(apCorners[0], apCorners[1]);
                psGeomEnvelope.MinY = std::min(apCorners[2], apCorners[3]);
                psGeomEnvelope.MaxY = std::max(apCorners[2], apCorners[3]);
            }


            int minBlockX = std::max(0, int(psGeomEnvelope.MinX) / nXBlockSize );
            int minBlockY = std::max(0, int(psGeomEnvelope.MinY) / nYBlockSize );
            int maxBlockX = std::min(nXBlocks-1, int(psGeomEnvelope.MaxX+1) / nXBlockSize );
            int maxBlockY = std::min(nYBlocks-1, int(psGeomEnvelope.MaxY+1) / nYBlockSize );

            

/* -------------------------------------------------------------------- */
/*      loop over the blocks concerned by the geometry                  */
/*      (by packs of nbBlocsX x nbBlocsY)                                 */
/* -------------------------------------------------------------------- */

            for(int xB = minBlockX; xB <= maxBlockX; xB += nbBlocsX)
            {
                for(int yB = minBlockY; yB <= maxBlockY; yB += nbBlocsY)
                {

/* -------------------------------------------------------------------- */
/*      ensure to stay in the image                                     */
/* -------------------------------------------------------------------- */
                    int remSBX = std::min(maxBlockX - xB + 1, nbBlocsX);
                    int remSBY = std::min(maxBlockY - yB + 1, nbBlocsY);
                    int nThisXChunkSize = nXBlockSize * remSBX;
                    int nThisYChunkSize = nYBlockSize * remSBY;
                    if( xB * nXBlockSize + nThisXChunkSize > poDS->GetRasterXSize() )
                        nThisXChunkSize = poDS->GetRasterXSize() - xB * nXBlockSize;
                    if( yB * nYBlockSize + nThisYChunkSize > poDS->GetRasterYSize() )
                        nThisYChunkSize = poDS->GetRasterYSize() - yB * nYBlockSize;

/* -------------------------------------------------------------------- */
/*      read image / process buffer / write buffer                      */
/* -------------------------------------------------------------------- */
                    eErr = poDS->RasterIO(GF_Read, xB * nXBlockSize, yB * nYBlockSize, nThisXChunkSize, nThisYChunkSize,
                                       pabyChunkBuf, nThisXChunkSize, nThisYChunkSize, eType, nBandCount, panBandList,
                                       0, 0, 0, NULL);
                    if( eErr != CE_None )
                        break;

                    gv_rasterize_one_shape( pabyChunkBuf, xB * nXBlockSize, yB * nYBlockSize,
                                            nThisXChunkSize, nThisYChunkSize,
                                            nBandCount, eType, bAllTouched,
                                            reinterpret_cast<OGRGeometry *>(pahGeometries[iShape]),
                                            padfGeomBurnValue + iShape*nBandCount,
                                            eBurnValueSource, eMergeAlg,
                                            pfnTransformer, pTransformArg );
            
                    eErr = poDS->RasterIO(GF_Write, xB * nXBlockSize, yB * nYBlockSize, nThisXChunkSize, nThisYChunkSize,
                                       pabyChunkBuf, nThisXChunkSize, nThisYChunkSize, eType, nBandCount, panBandList,
                                       0, 0, 0, NULL);
                    if( eErr != CE_None )
                        break;
                }
            }

            if( !pfnProgress(iShape / (double) nGeomCount, "", pProgressArg ) )
            {
                CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
                eErr = CE_Failure;
            }

        }

        CPLFree( panSuccessTransform );

        if( !pfnProgress(1., "", pProgressArg ) )
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
 * If the geometries are in the georeferenced coordinates of the raster
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
 * @param pTransformArg callback data for transformer.
 * @param padfLayerBurnValues the array of values to burn into the raster.
 * There should be nBandCount values for each layer.
 * @param papszOptions special options controlling rasterization:
 * <ul>
 * <li>"ATTRIBUTE": Identifies an attribute field on the features to be
 * used for a burn in value. The value will be burned into all output
 * bands. If specified, padfLayerBurnValues will not be used and can be a NULL
 * pointer.</li>
 * <li>"CHUNKYSIZE": The height in lines of the chunk to operate on.
 * The larger the chunk size the less times we need to make a pass through all
 * the shapes. If it is not set or set to zero the default chunk size will be
 * used. Default size will be estimated based on the GDAL cache buffer size
 * using formula: cache_size_bytes/scanline_size_bytes, so the chunk will
 * not exceed the cache.</li>
 * <li>"ALL_TOUCHED": May be set to TRUE to set all pixels touched
 * by the line or polygons, not just those whose center is within the polygon
 * or that are selected by brezenhams line algorithm.  Defaults to FALSE.
 * <li>"BURN_VALUE_FROM": May be set to "Z" to use the Z values of the</li>
 * geometries. The value from padfLayerBurnValues or the attribute field value
 * is added to this before burning. In default case dfBurnValue is burned as it
 * is. This is implemented properly only for points and lines for now. Polygons
 * will be burned using the Z value from the first point. The M value may be
 * supported in the future.</li>
 * <li>"MERGE_ALG": May be REPLACE (the default) or ADD.  REPLACE results in
 * overwriting of value, while ADD adds the new value to the existing raster,
 * suitable for heatmaps for instance.</li>
 * </ul>
 * @param pfnProgress the progress function to report completion.
 * @param pProgressArg callback data for progress function.
 *
 * @return CE_None on success or CE_Failure on error.
 */

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
    VALIDATE_POINTER1( hDS, "GDALRasterizeLayers", CE_Failure);

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      Do some rudimentary arg checking.                               */
/* -------------------------------------------------------------------- */
    if( nBandCount == 0 || nLayerCount == 0 )
        return CE_None;

    GDALDataset *poDS = reinterpret_cast<GDALDataset *>(hDS);

    // Prototype band.
    GDALRasterBand *poBand = poDS->GetRasterBand( panBandList[0] );
    if( poBand == NULL )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Options                                                         */
/* -------------------------------------------------------------------- */
    int bAllTouched = FALSE;
    GDALBurnValueSrc eBurnValueSource = GBV_UserBurnValue;
    GDALRasterMergeAlg eMergeAlg = GRMA_Replace;
    GDALRasterizeOptim eOptim = GRO_Auto;
    if( GDALRasterizeOptions(papszOptions, &bAllTouched,
                             &eBurnValueSource, &eMergeAlg,
                             &eOptim) == CE_Failure )
    {
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Establish a chunksize to operate on.  The larger the chunk      */
/*      size the less times we need to make a pass through all the      */
/*      shapes.                                                         */
/* -------------------------------------------------------------------- */
    const char  *pszYChunkSize =
        CSLFetchNameValue( papszOptions, "CHUNKYSIZE" );

    const GDALDataType eType =
        poBand->GetRasterDataType() == GDT_Byte ? GDT_Byte : GDT_Float64;

    const int nScanlineBytes =
        nBandCount * poDS->GetRasterXSize() * GDALGetDataTypeSizeBytes(eType);

    int nYChunkSize = 0;
    if( !(pszYChunkSize && ((nYChunkSize = atoi(pszYChunkSize))) != 0) )
    {
        const GIntBig nYChunkSize64 = GDALGetCacheMax64() / nScanlineBytes;
        if( nYChunkSize64 > INT_MAX )
            nYChunkSize = INT_MAX;
        else
          nYChunkSize = static_cast<int>(nYChunkSize64);
    }

    if( nYChunkSize < 1 )
        nYChunkSize = 1;
    if( nYChunkSize > poDS->GetRasterYSize() )
        nYChunkSize = poDS->GetRasterYSize();

    CPLDebug( "GDAL", "Rasterizer operating on %d swaths of %d scanlines.",
              (poDS->GetRasterYSize() + nYChunkSize - 1) / nYChunkSize,
              nYChunkSize );
    unsigned char *pabyChunkBuf = static_cast<unsigned char *>(
        VSI_MALLOC2_VERBOSE(nYChunkSize, nScanlineBytes));
    if( pabyChunkBuf == NULL )
    {
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Read the image once for all layers if user requested to render  */
/*      the whole raster in single chunk.                               */
/* -------------------------------------------------------------------- */
    if( nYChunkSize == poDS->GetRasterYSize() )
    {
        if( poDS->RasterIO( GF_Read, 0, 0, poDS->GetRasterXSize(),
                            nYChunkSize, pabyChunkBuf,
                            poDS->GetRasterXSize(), nYChunkSize,
                            eType, nBandCount, panBandList, 0, 0, 0, NULL )
             != CE_None )
        {
            CPLFree( pabyChunkBuf );
            return CE_Failure;
        }
    }

/* ==================================================================== */
/*      Read the specified layers transforming and rasterizing          */
/*      geometries.                                                     */
/* ==================================================================== */
    CPLErr eErr = CE_None;
    const char *pszBurnAttribute = CSLFetchNameValue(papszOptions, "ATTRIBUTE");

    pfnProgress( 0.0, NULL, pProgressArg );

    for( int iLayer = 0; iLayer < nLayerCount; iLayer++ )
    {
        OGRLayer *poLayer = reinterpret_cast<OGRLayer *>(pahLayers[iLayer]);

        if( !poLayer )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Layer element number %d is NULL, skipping.", iLayer );
            continue;
        }

/* -------------------------------------------------------------------- */
/*      If the layer does not contain any features just skip it.        */
/*      Do not force the feature count, so if driver doesn't know       */
/*      exact number of features, go down the normal way.               */
/* -------------------------------------------------------------------- */
        if( poLayer->GetFeatureCount(FALSE) == 0 )
            continue;

        int iBurnField = -1;
        double *padfBurnValues = NULL;

        if( pszBurnAttribute )
        {
            iBurnField =
                poLayer->GetLayerDefn()->GetFieldIndex( pszBurnAttribute );
            if( iBurnField == -1 )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Failed to find field %s on layer %s, skipping.",
                          pszBurnAttribute,
                          poLayer->GetLayerDefn()->GetName() );
                continue;
            }
        }
        else
        {
            padfBurnValues = padfLayerBurnValues + iLayer * nBandCount;
        }

/* -------------------------------------------------------------------- */
/*      If we have no transformer, create the one from input file       */
/*      projection. Note that each layer can be georefernced            */
/*      separately.                                                     */
/* -------------------------------------------------------------------- */
        bool bNeedToFreeTransformer = false;

        if( pfnTransformer == NULL )
        {
            char *pszProjection = NULL;
            bNeedToFreeTransformer = true;

            OGRSpatialReference *poSRS = poLayer->GetSpatialRef();
            if( !poSRS )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Failed to fetch spatial reference on layer %s "
                          "to build transformer, assuming matching coordinate "
                          "systems.",
                          poLayer->GetLayerDefn()->GetName() );
            }
            else
            {
                poSRS->exportToWkt( &pszProjection );
            }

            char** papszTransformerOptions = NULL;
            if( pszProjection != NULL )
                papszTransformerOptions = CSLSetNameValue(
                        papszTransformerOptions, "SRC_SRS", pszProjection );
            double adfGeoTransform[6] = {};
            if( poDS->GetGeoTransform( adfGeoTransform ) != CE_None &&
                poDS->GetGCPCount() == 0 &&
                poDS->GetMetadata("RPC") == NULL )
            {
                papszTransformerOptions = CSLSetNameValue(
                    papszTransformerOptions, "DST_METHOD", "NO_GEOTRANSFORM");
            }

            pTransformArg =
                GDALCreateGenImgProjTransformer2( NULL, hDS,
                                                  papszTransformerOptions );
            pfnTransformer = GDALGenImgProjTransform;

            CPLFree( pszProjection );
            CSLDestroy( papszTransformerOptions );
            if( pTransformArg == NULL )
            {
                CPLFree( pabyChunkBuf );
                return CE_Failure;
            }
        }

        poLayer->ResetReading();

/* -------------------------------------------------------------------- */
/*      Loop over image in designated chunks.                           */
/* -------------------------------------------------------------------- */

        double *padfAttrValues = static_cast<double *>(
            VSI_MALLOC_VERBOSE(sizeof(double) * nBandCount));
        if( padfAttrValues == NULL )
            eErr = CE_Failure;

        for( int iY = 0;
             iY < poDS->GetRasterYSize() && eErr == CE_None;
             iY += nYChunkSize )
        {
            int nThisYChunkSize = nYChunkSize;
            if( nThisYChunkSize + iY > poDS->GetRasterYSize() )
                nThisYChunkSize = poDS->GetRasterYSize() - iY;

            // Only re-read image if not a single chunk is being rendered.
            if( nYChunkSize < poDS->GetRasterYSize() )
            {
                eErr =
                    poDS->RasterIO( GF_Read, 0, iY,
                                    poDS->GetRasterXSize(), nThisYChunkSize,
                                    pabyChunkBuf,
                                    poDS->GetRasterXSize(), nThisYChunkSize,
                                    eType, nBandCount, panBandList,
                                    0, 0, 0, NULL );
                if( eErr != CE_None )
                    break;
            }

            OGRFeature *poFeat = NULL;
            while( (poFeat = poLayer->GetNextFeature()) != NULL )
            {
                OGRGeometry *poGeom = poFeat->GetGeometryRef();

                if( pszBurnAttribute )
                {
                    const double dfAttrValue =
                        poFeat->GetFieldAsDouble( iBurnField );
                    for( int iBand = 0 ; iBand < nBandCount ; iBand++)
                        padfAttrValues[iBand] = dfAttrValue;

                    padfBurnValues = padfAttrValues;
                }

                gv_rasterize_one_shape( pabyChunkBuf, 0, iY,
                                        poDS->GetRasterXSize(),
                                        nThisYChunkSize,
                                        nBandCount, eType, bAllTouched, poGeom,
                                        padfBurnValues, eBurnValueSource,
                                        eMergeAlg,
                                        pfnTransformer, pTransformArg );

                delete poFeat;
            }

            // Only write image if not a single chunk is being rendered.
            if( nYChunkSize < poDS->GetRasterYSize() )
            {
                eErr =
                    poDS->RasterIO( GF_Write, 0, iY,
                                    poDS->GetRasterXSize(), nThisYChunkSize,
                                    pabyChunkBuf,
                                    poDS->GetRasterXSize(), nThisYChunkSize,
                                    eType, nBandCount, panBandList,
                                    0, 0, 0, NULL );
            }

            poLayer->ResetReading();

            if( !pfnProgress((iY + nThisYChunkSize) /
                             static_cast<double>(poDS->GetRasterYSize()),
                             "", pProgressArg) )
            {
                CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
                eErr = CE_Failure;
            }
        }

        VSIFree( padfAttrValues );

        if( bNeedToFreeTransformer )
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
    if( eErr == CE_None && nYChunkSize == poDS->GetRasterYSize() )
    {
        eErr = poDS->RasterIO( GF_Write, 0, 0,
                                poDS->GetRasterXSize(), nYChunkSize,
                                pabyChunkBuf,
                                poDS->GetRasterXSize(), nYChunkSize,
                                eType, nBandCount, panBandList, 0, 0, 0, NULL );
    }

/* -------------------------------------------------------------------- */
/*      cleanup                                                         */
/* -------------------------------------------------------------------- */
    VSIFree( pabyChunkBuf );

    return eErr;
}

/************************************************************************/
/*                        GDALRasterizeLayersBuf()                      */
/************************************************************************/

/**
 * Burn geometries from the specified list of layer into raster.
 *
 * Rasterize all the geometric objects from a list of layers into supplied
 * raster buffer.  The layers are passed as an array of OGRLayerH handlers.
 *
 * If the geometries are in the georeferenced coordinates of the raster
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
 * @param pTransformArg callback data for transformer.
 *
 * @param dfBurnValue the value to burn into the raster.
 *
 * @param papszOptions special options controlling rasterization:
 * <ul>
 * <li>"ATTRIBUTE": Identifies an attribute field on the features to be
 * used for a burn in value. The value will be burned into all output
 * bands. If specified, padfLayerBurnValues will not be used and can be a NULL
 * pointer.</li>
 * <li>"ALL_TOUCHED": May be set to TRUE to set all pixels touched
 * by the line or polygons, not just those whose center is within the polygon
 * or that are selected by brezenhams line algorithm.  Defaults to FALSE.</li>
 * <li>"BURN_VALUE_FROM": May be set to "Z" to use
 * the Z values of the geometries. dfBurnValue or the attribute field value is
 * added to this before burning. In default case dfBurnValue is burned as it
 * is. This is implemented properly only for points and lines for now. Polygons
 * will be burned using the Z value from the first point. The M value may
 * be supported in the future.</li>
 * <li>"MERGE_ALG": May be REPLACE (the default) or ADD.  REPLACE
 * results in overwriting of value, while ADD adds the new value to the
 * existing raster, suitable for heatmaps for instance.</li>
 * </ul>
 *
 * @param pfnProgress the progress function to report completion.
 *
 * @param pProgressArg callback data for progress function.
 *
 *
 * @return CE_None on success or CE_Failure on error.
 */

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
    if( nPixelSpace != 0 )
    {
        nPixelSpace = GDALGetDataTypeSizeBytes( eBufType );
    }
    if( nPixelSpace != GDALGetDataTypeSizeBytes( eBufType ) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GDALRasterizeLayersBuf(): unsupported value of nPixelSpace");
        return CE_Failure;
    }

    if( nLineSpace == 0 )
    {
        nLineSpace = nPixelSpace * nBufXSize;
    }
    if( nLineSpace != nPixelSpace * nBufXSize )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GDALRasterizeLayersBuf(): unsupported value of nLineSpace");
        return CE_Failure;
    }

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      Do some rudimentary arg checking.                               */
/* -------------------------------------------------------------------- */
    if( nLayerCount == 0 )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Options                                                         */
/* -------------------------------------------------------------------- */
    int bAllTouched = FALSE;
    GDALBurnValueSrc eBurnValueSource = GBV_UserBurnValue;
    GDALRasterMergeAlg eMergeAlg = GRMA_Replace;
    GDALRasterizeOptim eOptim = GRO_Auto;
    if( GDALRasterizeOptions(papszOptions, &bAllTouched,
                             &eBurnValueSource, &eMergeAlg,
                             &eOptim) == CE_Failure )
    {
        return CE_Failure;
    }

/* ==================================================================== */
/*      Read the specified layers transforming and rasterizing          */
/*      geometries.                                                     */
/* ==================================================================== */
    CPLErr eErr = CE_None;
    const char  *pszBurnAttribute =
        CSLFetchNameValue( papszOptions, "ATTRIBUTE" );

    pfnProgress( 0.0, NULL, pProgressArg );

    for( int iLayer = 0; iLayer < nLayerCount; iLayer++ )
    {
        OGRLayer *poLayer = reinterpret_cast<OGRLayer *>(pahLayers[iLayer]);

        if( !poLayer )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Layer element number %d is NULL, skipping.", iLayer );
            continue;
        }

/* -------------------------------------------------------------------- */
/*      If the layer does not contain any features just skip it.        */
/*      Do not force the feature count, so if driver doesn't know       */
/*      exact number of features, go down the normal way.               */
/* -------------------------------------------------------------------- */
        if( poLayer->GetFeatureCount(FALSE) == 0 )
            continue;

        int iBurnField = -1;
        if( pszBurnAttribute )
        {
            iBurnField =
                poLayer->GetLayerDefn()->GetFieldIndex( pszBurnAttribute );
            if( iBurnField == -1 )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Failed to find field %s on layer %s, skipping.",
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
        bool bNeedToFreeTransformer = false;

        if( pfnTransformer == NULL )
        {
            char *pszProjection = NULL;
            bNeedToFreeTransformer = true;

            OGRSpatialReference *poSRS = poLayer->GetSpatialRef();
            if( !poSRS )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Failed to fetch spatial reference on layer %s "
                          "to build transformer, assuming matching coordinate "
                          "systems.",
                          poLayer->GetLayerDefn()->GetName() );
            }
            else
            {
                poSRS->exportToWkt( &pszProjection );
            }

            pTransformArg =
                GDALCreateGenImgProjTransformer3( pszProjection, NULL,
                                                  pszDstProjection,
                                                  padfDstGeoTransform );
            pfnTransformer = GDALGenImgProjTransform;

            CPLFree( pszProjection );
        }

        poLayer->ResetReading();

        {
            OGRFeature *poFeat = NULL;
            while( (poFeat = poLayer->GetNextFeature()) != NULL )
            {
                OGRGeometry *poGeom = poFeat->GetGeometryRef();

                if( pszBurnAttribute )
                    dfBurnValue = poFeat->GetFieldAsDouble( iBurnField );

                gv_rasterize_one_shape( static_cast<unsigned char *>(pData), 0, 0,
                                        nBufXSize, nBufYSize,
                                        1, eBufType, bAllTouched, poGeom,
                                        &dfBurnValue, eBurnValueSource,
                                        eMergeAlg,
                                        pfnTransformer, pTransformArg );

                delete poFeat;
            }
        }

        poLayer->ResetReading();

        if( !pfnProgress(1, "", pProgressArg) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }

        if( bNeedToFreeTransformer )
        {
            GDALDestroyTransformer( pTransformArg );
            pTransformArg = NULL;
            pfnTransformer = NULL;
        }
    }

    return eErr;
}
