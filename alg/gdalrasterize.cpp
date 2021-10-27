/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Vector rasterization.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include <cfloat>
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
/*                        gvBurnScanlineBasic()                         */
/************************************************************************/
template<typename T>
static inline
void gvBurnScanlineBasic( GDALRasterizeInfo *psInfo,
                          int nY, int nXStart, int nXEnd,
                          double dfVariant )

{
    for( int iBand = 0; iBand < psInfo->nBands; iBand++ )
    {
        const double burnValue = ( psInfo->padfBurnValue[iBand] +
                ( (psInfo->eBurnValueSource == GBV_UserBurnValue)?
                            0 : dfVariant ) );

        unsigned char *pabyInsert = psInfo->pabyChunkBuf
                                    + iBand * psInfo->nBandSpace
                                    + nY * psInfo->nLineSpace + nXStart * psInfo->nPixelSpace;
        int nPixels = nXEnd - nXStart + 1;
        if( psInfo->eMergeAlg == GRMA_Add ) {
            while( nPixels-- > 0 ) 
            {
                *reinterpret_cast<T*>(pabyInsert) += static_cast<T>(burnValue);
                pabyInsert += psInfo->nPixelSpace;
            }
        } else {
            while( nPixels-- > 0 ) 
            {
                *reinterpret_cast<T*>(pabyInsert) = static_cast<T>(burnValue);
                pabyInsert += psInfo->nPixelSpace;
            }
        }
    }
}


/************************************************************************/
/*                           gvBurnScanline()                           */
/************************************************************************/
static
void gvBurnScanline( void *pCBData, int nY, int nXStart, int nXEnd,
                     double dfVariant )

{
    GDALRasterizeInfo *psInfo = static_cast<GDALRasterizeInfo *>(pCBData);

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

    switch (psInfo->eType)
    {
        case GDT_Byte:
            gvBurnScanlineBasic<GByte>( psInfo, nY, nXStart, nXEnd, dfVariant );
            break;
        case GDT_Int16:
            gvBurnScanlineBasic<GInt16>( psInfo, nY, nXStart, nXEnd, dfVariant );
            break;
        case GDT_UInt16:
            gvBurnScanlineBasic<GUInt16>( psInfo, nY, nXStart, nXEnd, dfVariant );
            break;
        case GDT_Int32:
            gvBurnScanlineBasic<GInt32>( psInfo, nY, nXStart, nXEnd, dfVariant );
            break;
        case GDT_UInt32:
            gvBurnScanlineBasic<GUInt32>( psInfo, nY, nXStart, nXEnd, dfVariant );
            break;
        case GDT_Float32:
            gvBurnScanlineBasic<float>( psInfo, nY, nXStart, nXEnd, dfVariant );
            break;
        case GDT_Float64:
            gvBurnScanlineBasic<double>( psInfo, nY, nXStart, nXEnd, dfVariant );
            break;
        default:
            CPLAssert(false);
            break;
    }
}

/************************************************************************/
/*                        gvBurnPointBasic()                            */
/************************************************************************/
template<typename T>
static inline
void gvBurnPointBasic( GDALRasterizeInfo *psInfo,
                       int nY, int nX, double dfVariant )

{
    constexpr double dfMinVariant = std::numeric_limits<T>::lowest();
    constexpr double dfMaxVariant = std::numeric_limits<T>::max();

    for( int iBand = 0; iBand < psInfo->nBands; iBand++ )
    {
        double burnValue = ( psInfo->padfBurnValue[iBand] +
                ( (psInfo->eBurnValueSource == GBV_UserBurnValue)?
                            0 : dfVariant ) );
        unsigned char *pbyInsert = psInfo->pabyChunkBuf
                                 + iBand * psInfo->nBandSpace
                                 + nY * psInfo->nLineSpace + nX * psInfo->nPixelSpace;

        T* pbyPixel = reinterpret_cast<T*>(pbyInsert);
        burnValue += ( psInfo->eMergeAlg != GRMA_Add ) ? 0 : *pbyPixel;
        *pbyPixel = static_cast<T>(
                    ( dfMinVariant > burnValue ) ? dfMinVariant :
                    ( dfMaxVariant < burnValue ) ? dfMaxVariant :
                    burnValue );
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

    switch( psInfo->eType )
    {
        case GDT_Byte:
            gvBurnPointBasic<GByte>( psInfo, nY, nX, dfVariant );
            break;
        case GDT_Int16:
            gvBurnPointBasic<GInt16>( psInfo, nY, nX, dfVariant );
            break;
        case GDT_UInt16:
            gvBurnPointBasic<GUInt16>( psInfo, nY, nX, dfVariant );
            break;
        case GDT_Int32:
            gvBurnPointBasic<GInt32>( psInfo, nY, nX, dfVariant );
            break;
        case GDT_UInt32:
            gvBurnPointBasic<GUInt32>( psInfo, nY, nX, dfVariant );
            break;
        case GDT_Float32:
            gvBurnPointBasic<float>( psInfo, nY, nX, dfVariant );
            break;
        case GDT_Float64:
            gvBurnPointBasic<double>( psInfo, nY, nX, dfVariant );
            break;
        default:
            CPLAssert(false);
    }
}

/************************************************************************/
/*                    GDALCollectRingsFromGeometry()                    */
/************************************************************************/

static void GDALCollectRingsFromGeometry(
    const OGRGeometry *poShape,
    std::vector<double> &aPointX, std::vector<double> &aPointY,
    std::vector<double> &aPointVariant,
    std::vector<int> &aPartSize, GDALBurnValueSrc eBurnValueSrc)

{
    if( poShape == nullptr || poShape->IsEmpty() )
        return;

    const OGRwkbGeometryType eFlatType = wkbFlatten(poShape->getGeometryType());

    if( eFlatType == wkbPoint )
    {
        const auto poPoint = poShape->toPoint();

        aPointX.push_back( poPoint->getX() );
        aPointY.push_back( poPoint->getY() );
        aPartSize.push_back( 1 );
        if( eBurnValueSrc != GBV_UserBurnValue )
        {
            // TODO(schwehr): Why not have the option for M r18164?
            // switch( eBurnValueSrc )
            // {
            // case GBV_Z:*/
            aPointVariant.push_back( poPoint->getZ() );
            // break;
            // case GBV_M:
            //    aPointVariant.reserve( nNewCount );
            //    aPointVariant.push_back( poPoint->getM() );
        }
    }
    else if( EQUAL(poShape->getGeometryName(), "LINEARRING") )
    {
        const auto poRing = poShape->toLinearRing();
        const int nCount = poRing->getNumPoints();
        const size_t nNewCount = aPointX.size() + static_cast<size_t>(nCount);

        aPointX.reserve( nNewCount );
        aPointY.reserve( nNewCount );
        if( eBurnValueSrc != GBV_UserBurnValue )
            aPointVariant.reserve( nNewCount );
        if( poRing->isClockwise() )
        {
            for( int i = 0; i < nCount; i++ )
            {
                aPointX.push_back( poRing->getX(i) );
                aPointY.push_back( poRing->getY(i) );
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
            }
        }
        else
        {
            for( int i = nCount - 1; i >= 0; i-- )
            {
                aPointX.push_back( poRing->getX(i) );
                aPointY.push_back( poRing->getY(i) );
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

            }
        }
        aPartSize.push_back( nCount );
    }
    else if( eFlatType == wkbLineString )
    {
        const auto poLine = poShape->toLineString();
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
    else if( eFlatType == wkbPolygon )
    {
        const auto poPolygon = poShape->toPolygon();

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
        const auto poGC = poShape->toGeometryCollection();
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
                        int nBands, GDALDataType eType,
                        int nPixelSpace, GSpacing nLineSpace, GSpacing nBandSpace,
                        int bAllTouched,
                        const OGRGeometry *poShape,
                        const double *padfBurnValue,
                        GDALBurnValueSrc eBurnValueSrc,
                        GDALRasterMergeAlg eMergeAlg,
                        GDALTransformerFunc pfnTransformer,
                        void *pTransformArg )

{
    if( poShape == nullptr || poShape->IsEmpty() )
        return;
    const auto eGeomType = wkbFlatten(poShape->getGeometryType());

    if( (eGeomType == wkbMultiLineString ||
         eGeomType == wkbMultiPolygon ||
         eGeomType == wkbGeometryCollection) &&
        eMergeAlg == GRMA_Replace )
    {
        // Speed optimization: in replace mode, we can rasterize each part of
        // a geometry collection separately.
        const auto poGC = poShape->toGeometryCollection();
        for( const auto poPart: *poGC )
        {
            gv_rasterize_one_shape(pabyChunkBuf, nXOff, nYOff,
                                   nXSize, nYSize,
                                   nBands, eType,
                                   nPixelSpace, nLineSpace, nBandSpace,
                                   bAllTouched,
                                   poPart,
                                   padfBurnValue,
                                   eBurnValueSrc,
                                   eMergeAlg,
                                   pfnTransformer,
                                   pTransformArg);
        }
        return;
    }

    if(nPixelSpace == 0)
    {
        nPixelSpace = GDALGetDataTypeSizeBytes(eType);
    }
    if(nLineSpace == 0)
    {
        nLineSpace = static_cast<GSpacing>(nXSize) * nPixelSpace;
    }
    if(nBandSpace == 0)
    {
        nBandSpace = nYSize * nLineSpace;
    }

    GDALRasterizeInfo sInfo;
    sInfo.nXSize = nXSize;
    sInfo.nYSize = nYSize;
    sInfo.nBands = nBands;
    sInfo.pabyChunkBuf = pabyChunkBuf;
    sInfo.eType = eType;
    sInfo.nPixelSpace = nPixelSpace;
    sInfo.nLineSpace = nLineSpace;
    sInfo.nBandSpace = nBandSpace;
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
    if( pfnTransformer != nullptr )
    {
        int *panSuccess =
            static_cast<int *>(CPLCalloc(sizeof(int), aPointX.size()));

        // TODO: We need to add all appropriate error checking at some point.
        pfnTransformer( pTransformArg, FALSE, static_cast<int>(aPointX.size()),
                        aPointX.data(), aPointY.data(), nullptr, panSuccess );
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

    switch( eGeomType )
    {
      case wkbPoint:
      case wkbMultiPoint:
        GDALdllImagePoint( sInfo.nXSize, nYSize,
                           static_cast<int>(aPartSize.size()), aPartSize.data(),
                           aPointX.data(), aPointY.data(),
                           (eBurnValueSrc == GBV_UserBurnValue)?
                           nullptr : aPointVariant.data(),
                           gvBurnPoint, &sInfo );
        break;
      case wkbLineString:
      case wkbMultiLineString:
      {
          if( bAllTouched )
              GDALdllImageLineAllTouched( sInfo.nXSize, nYSize,
                                          static_cast<int>(aPartSize.size()),
                                          aPartSize.data(),
                                          aPointX.data(), aPointY.data(),
                                          (eBurnValueSrc == GBV_UserBurnValue)?
                                          nullptr : aPointVariant.data(),
                                          gvBurnPoint, &sInfo,
                                          eMergeAlg == GRMA_Add );
          else
              GDALdllImageLine( sInfo.nXSize, nYSize,
                                static_cast<int>(aPartSize.size()),
                                aPartSize.data(),
                                aPointX.data(), aPointY.data(),
                                (eBurnValueSrc == GBV_UserBurnValue)?
                                nullptr : aPointVariant.data(),
                                gvBurnPoint, &sInfo );
      }
      break;

      default:
      {
          GDALdllImageFilledPolygon(
              sInfo.nXSize, nYSize,
              static_cast<int>(aPartSize.size()), aPartSize.data(),
              aPointX.data(), aPointY.data(),
              (eBurnValueSrc == GBV_UserBurnValue)?
              nullptr : aPointVariant.data(),
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
                      static_cast<int>(aPartSize.size()), aPartSize.data(),
                      aPointX.data(), aPointY.data(),
                      nullptr,
                      gvBurnPoint, &sInfo,
                      eMergeAlg == GRMA_Add );
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
                      static_cast<int>(aPartSize.size()), aPartSize.data(),
                      aPointX.data(), aPointY.data(),
                      aPointVariant.data(),
                      gvBurnPoint, &sInfo,
                      eMergeAlg == GRMA_Add );
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
 * The output raster may be of any GDAL supported datatype. An explicit list
 * of burn values for each geometry for each band must be passed in.
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
 *
 * <strong>Example</strong><br>
 * GDALRasterizeGeometries rasterize output to MEM Dataset :<br>
 * @code
 *     int nBufXSize      = 1024;
 *     int nBufYSize      = 1024;
 *     int nBandCount     = 1;
 *     GDALDataType eType = GDT_Byte;
 *     int nDataTypeSize  = GDALGetDataTypeSizeBytes(eType);
 *
 *     void* pData = CPLCalloc( nBufXSize*nBufYSize*nBandCount, nDataTypeSize );
 *     char memdsetpath[1024];
 *     sprintf(memdsetpath,"MEM:::DATAPOINTER=0x%p,PIXELS=%d,LINES=%d,"
 *             "BANDS=%d,DATATYPE=%s,PIXELOFFSET=%d,LINEOFFSET=%d",
 *             pData,nBufXSize,nBufYSize,nBandCount,GDALGetDataTypeName(eType),
 *             nBandCount*nDataTypeSize, nBufXSize*nBandCount*nDataTypeSize );
 *
 *      // Open Memory Dataset
 *      GDALDatasetH hMemDset = GDALOpen(memdsetpath, GA_Update);
 *      // or create it as follows
 *      // GDALDriverH hMemDriver = GDALGetDriverByName("MEM");
 *      // GDALDatasetH hMemDset = GDALCreate(hMemDriver, "", nBufXSize, nBufYSize, nBandCount, eType, NULL);
 *
 *      double adfGeoTransform[6];
 *      // Assign GeoTransform parameters,Omitted here.
 *
 *      GDALSetGeoTransform(hMemDset,adfGeoTransform);
 *      GDALSetProjection(hMemDset,pszProjection); // Can not
 *      
 *      // Do something ...
 *      // Need an array of OGRGeometry objects,The assumption here is pahGeoms
 *      
 *      int bandList[3] = { 1, 2, 3};
 *      std::vector<double> geomBurnValue(nGeomCount*nBandCount,255.0);
 *      CPLErr err = GDALRasterizeGeometries(hMemDset, nBandCount, bandList,
 *                              nGeomCount, pahGeoms, pfnTransformer, pTransformArg,
 *                              geomBurnValue.data(), papszOptions,
 *                              pfnProgress, pProgressArg);
 *      if( err != CE_None )
 *      {
 *          // Do something ...
 *      }
 *      GDALClose(hMemDset);
 *      CPLFree(pData);
 *@endcode
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

    if( pfnProgress == nullptr )
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
    if( poBand == nullptr )
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

    if( pfnTransformer == nullptr )
    {
        bNeedToFreeTransformer = true;

        char** papszTransformerOptions = nullptr;
        double adfGeoTransform[6] = { 0.0 };
        if( poDS->GetGeoTransform( adfGeoTransform ) != CE_None &&
            poDS->GetGCPCount() == 0 &&
            poDS->GetMetadata("RPC") == nullptr )
        {
            papszTransformerOptions = CSLSetNameValue(
                papszTransformerOptions, "DST_METHOD", "NO_GEOTRANSFORM");
        }

        pTransformArg =
            GDALCreateGenImgProjTransformer2( nullptr, hDS,
                                                papszTransformerOptions );
        CSLDestroy( papszTransformerOptions );

        pfnTransformer = GDALGenImgProjTransform;
        if( pTransformArg == nullptr )
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
        // TODO make more tests with various inputs/outputs to adjust the parameters
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
        const GDALDataType eType = GDALGetNonComplexDataType(poBand->GetRasterDataType());

        const int nScanlineBytes =
            nBandCount * poDS->GetRasterXSize() * GDALGetDataTypeSizeBytes(eType);

        int nYChunkSize = 0;
        const char *pszYChunkSize = CSLFetchNameValue(papszOptions, "CHUNKYSIZE");
        if( pszYChunkSize == nullptr || ((nYChunkSize = atoi(pszYChunkSize))) == 0)
        {
            const GIntBig nYChunkSize64 = GDALGetCacheMax64() / nScanlineBytes;
            const int knIntMax = std::numeric_limits<int>::max();
            nYChunkSize = nYChunkSize64 > knIntMax ? knIntMax
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
        if( pabyChunkBuf == nullptr )
        {
            if( bNeedToFreeTransformer )
                GDALDestroyTransformer( pTransformArg );
            return CE_Failure;
        }

/* ==================================================================== */
/*      Loop over image in designated chunks.                           */
/* ==================================================================== */
        pfnProgress( 0.0, nullptr, pProgressArg );

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
                               0, 0, 0, nullptr);
            if( eErr != CE_None )
                break;

            for( int iShape = 0; iShape < nGeomCount; iShape++ )
            {
                gv_rasterize_one_shape( pabyChunkBuf, 0, iY,
                                        poDS->GetRasterXSize(), nThisYChunkSize,
                                        nBandCount, eType,
                                        0, 0, 0,
                                        bAllTouched,
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
                                eType, nBandCount, panBandList, 0, 0, 0, nullptr);

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
        const int knIntMax = std::numeric_limits<int>::max();
        const int nbMaxBlocks = static_cast<int>(std::min(
            static_cast<GIntBig>(knIntMax / nPixelSize / nYBlockSize / nXBlockSize), nbMaxBlocks64));
        const int nbBlocsX = std::max(1, std::min(static_cast<int>(sqrt(static_cast<double>(nbMaxBlocks))), nXBlocks));
        const int nbBlocsY = std::max(1, std::min(nbMaxBlocks / nbBlocsX, nYBlocks));

        const int nScanblocks =
            nXBlockSize * nbBlocsX * nYBlockSize * nbBlocsY;

        pabyChunkBuf = static_cast<unsigned char *>( VSI_MALLOC2_VERBOSE(nPixelSize, nScanblocks) );
        if( pabyChunkBuf == nullptr )
        {
            if( bNeedToFreeTransformer )
                GDALDestroyTransformer( pTransformArg );
            return CE_Failure;
        }

        int * panSuccessTransform = static_cast<int *>(CPLCalloc(sizeof(int), 2));

/* -------------------------------------------------------------------- */
/*      loop over the vectorial geometries                              */
/* -------------------------------------------------------------------- */
        pfnProgress( 0.0, nullptr, pProgressArg );
        for( int iShape = 0; iShape < nGeomCount; iShape++ )
        {

            OGRGeometry * poGeometry = reinterpret_cast<OGRGeometry *>(pahGeometries[iShape]);
            if ( poGeometry == nullptr || poGeometry->IsEmpty() )
              continue;
/* -------------------------------------------------------------------- */
/*      get the envelope of the geometry and transform it to pixels coo */
/* -------------------------------------------------------------------- */
            OGREnvelope psGeomEnvelope;
            poGeometry->getEnvelope(&psGeomEnvelope);
            if( pfnTransformer != nullptr )
            {
                double apCorners[4];
                apCorners[0] = psGeomEnvelope.MinX;
                apCorners[1] = psGeomEnvelope.MaxX;
                apCorners[2] = psGeomEnvelope.MinY;
                apCorners[3] = psGeomEnvelope.MaxY;
                // TODO: need to add all appropriate error checking
                pfnTransformer( pTransformArg, FALSE, 2, &(apCorners[0]),
                                &(apCorners[2]), nullptr, panSuccessTransform );
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
                                       0, 0, 0, nullptr);
                    if( eErr != CE_None )
                        break;

                    gv_rasterize_one_shape( pabyChunkBuf, xB * nXBlockSize, yB * nYBlockSize,
                                            nThisXChunkSize, nThisYChunkSize,
                                            nBandCount, eType,
                                            0, 0, 0,
                                            bAllTouched,
                                            reinterpret_cast<OGRGeometry *>(pahGeometries[iShape]),
                                            padfGeomBurnValue + iShape*nBandCount,
                                            eBurnValueSource, eMergeAlg,
                                            pfnTransformer, pTransformArg );

                    eErr = poDS->RasterIO(GF_Write, xB * nXBlockSize, yB * nYBlockSize, nThisXChunkSize, nThisYChunkSize,
                                       pabyChunkBuf, nThisXChunkSize, nThisYChunkSize, eType, nBandCount, panBandList,
                                       0, 0, 0, nullptr);
                    if( eErr != CE_None )
                        break;
                }
            }

            if( !pfnProgress(iShape / static_cast<double>(nGeomCount), "", pProgressArg ) )
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
 * The output raster may be of any GDAL supported datatype. An explicit list
 * of burn values for each layer for each band must be passed in.
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

    if( pfnProgress == nullptr )
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      Do some rudimentary arg checking.                               */
/* -------------------------------------------------------------------- */
    if( nBandCount == 0 || nLayerCount == 0 )
        return CE_None;

    GDALDataset *poDS = reinterpret_cast<GDALDataset *>(hDS);

    // Prototype band.
    GDALRasterBand *poBand = poDS->GetRasterBand( panBandList[0] );
    if( poBand == nullptr )
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

    const GDALDataType eType = poBand->GetRasterDataType();

    const int nScanlineBytes =
        nBandCount * poDS->GetRasterXSize() * GDALGetDataTypeSizeBytes(eType);

    int nYChunkSize = 0;
    if( !(pszYChunkSize && ((nYChunkSize = atoi(pszYChunkSize))) != 0) )
    {
        const GIntBig nYChunkSize64 = GDALGetCacheMax64() / nScanlineBytes;
        const int knIntMax = std::numeric_limits<int>::max();
        if( nYChunkSize64 > knIntMax )
            nYChunkSize = knIntMax;
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
    if( pabyChunkBuf == nullptr )
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
                            eType, nBandCount, panBandList, 0, 0, 0, nullptr )
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

    pfnProgress( 0.0, nullptr, pProgressArg );

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
        double *padfBurnValues = nullptr;

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

        if( pfnTransformer == nullptr )
        {
            char *pszProjection = nullptr;
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

            char** papszTransformerOptions = nullptr;
            if( pszProjection != nullptr )
                papszTransformerOptions = CSLSetNameValue(
                        papszTransformerOptions, "SRC_SRS", pszProjection );
            double adfGeoTransform[6] = {};
            if( poDS->GetGeoTransform( adfGeoTransform ) != CE_None &&
                poDS->GetGCPCount() == 0 &&
                poDS->GetMetadata("RPC") == nullptr )
            {
                papszTransformerOptions = CSLSetNameValue(
                    papszTransformerOptions, "DST_METHOD", "NO_GEOTRANSFORM");
            }

            pTransformArg =
                GDALCreateGenImgProjTransformer2( nullptr, hDS,
                                                  papszTransformerOptions );
            pfnTransformer = GDALGenImgProjTransform;

            CPLFree( pszProjection );
            CSLDestroy( papszTransformerOptions );
            if( pTransformArg == nullptr )
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
        if( padfAttrValues == nullptr )
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
                                    0, 0, 0, nullptr );
                if( eErr != CE_None )
                    break;
            }

            for( auto& poFeat: poLayer )
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
                                        nBandCount, eType, 0, 0, 0, bAllTouched, poGeom,
                                        padfBurnValues, eBurnValueSource,
                                        eMergeAlg,
                                        pfnTransformer, pTransformArg );
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
                                    0, 0, 0, nullptr );
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
            pTransformArg = nullptr;
            pfnTransformer = nullptr;
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
                                eType, nBandCount, panBandList, 0, 0, 0, nullptr );
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
 * The output raster may be of any GDAL supported datatype(non complex).
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
/*           check eType, Avoid not supporting data types               */
/* -------------------------------------------------------------------- */
    if( GDALDataTypeIsComplex(eBufType) ||
       eBufType <= GDT_Unknown || eBufType >= GDT_TypeCount )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
            "GDALRasterizeLayersBuf(): unsupported data type of eBufType");
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      If pixel and line spaceing are defaulted assign reasonable      */
/*      value assuming a packed buffer.                                 */
/* -------------------------------------------------------------------- */
    int nTypeSizeBytes = GDALGetDataTypeSizeBytes( eBufType );
    if( nPixelSpace == 0 )
    {
        nPixelSpace = nTypeSizeBytes;
    }
    if( nPixelSpace < nTypeSizeBytes )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GDALRasterizeLayersBuf(): unsupported value of nPixelSpace");
        return CE_Failure;
    }

    if( nLineSpace == 0 )
    {
        nLineSpace = nPixelSpace * nBufXSize;
    }
    if( nLineSpace < nPixelSpace * nBufXSize )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GDALRasterizeLayersBuf(): unsupported value of nLineSpace");
        return CE_Failure;
    }

    if( pfnProgress == nullptr )
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

    pfnProgress( 0.0, nullptr, pProgressArg );

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

        if( pfnTransformer == nullptr )
        {
            char *pszProjection = nullptr;
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
                GDALCreateGenImgProjTransformer3( pszProjection, nullptr,
                                                  pszDstProjection,
                                                  padfDstGeoTransform );
            pfnTransformer = GDALGenImgProjTransform;

            CPLFree( pszProjection );
        }

        for( auto& poFeat: poLayer )
        {
            OGRGeometry *poGeom = poFeat->GetGeometryRef();

            if( pszBurnAttribute )
                dfBurnValue = poFeat->GetFieldAsDouble( iBurnField );

            gv_rasterize_one_shape( static_cast<unsigned char *>(pData), 0, 0,
                                    nBufXSize, nBufYSize,
                                    1, eBufType,
                                    nPixelSpace, nLineSpace, 0, bAllTouched, poGeom,
                                    &dfBurnValue, eBurnValueSource,
                                    eMergeAlg,
                                    pfnTransformer, pTransformArg );
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
            pTransformArg = nullptr;
            pfnTransformer = nullptr;
        }
    }

    return eErr;
}
