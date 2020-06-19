/******************************************************************************
 * Project:  GDAL
 * Purpose:  Correlator - GDALSimpleSURF and GDALFeaturePoint classes.
 * Author:   Andrew Migal, migal.drew@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2012, Andrew Migal
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

#include "gdal_simplesurf.h"

#include <algorithm>

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                            GDALFeaturePoint                          */
/* ==================================================================== */
/************************************************************************/

GDALFeaturePoint::GDALFeaturePoint() :
    nX(-1),
    nY(-1),
    nScale(-1),
    nRadius(-1),
    nSign(-1),
    padfDescriptor(new double[DESC_SIZE])
{}

GDALFeaturePoint::GDALFeaturePoint( const GDALFeaturePoint& fp ) :
    nX(fp.nX),
    nY(fp.nY),
    nScale(fp.nScale),
    nRadius(fp.nRadius),
    nSign(fp.nSign),
    padfDescriptor(new double[DESC_SIZE])
{
    for( int i = 0; i < DESC_SIZE; i++ )
        padfDescriptor[i] = fp.padfDescriptor[i];
}

GDALFeaturePoint::GDALFeaturePoint( int nXIn, int nYIn,
                                    int nScaleIn, int nRadiusIn, int nSignIn ) :
    nX(nXIn),
    nY(nYIn),
    nScale(nScaleIn),
    nRadius(nRadiusIn),
    nSign(nSignIn),
    padfDescriptor(new double[DESC_SIZE])
{}

GDALFeaturePoint& GDALFeaturePoint::operator=( const GDALFeaturePoint& point )
{
    if( this == &point )
        return *this;

    nX = point.nX;
    nY = point.nY;
    nScale = point.nScale;
    nRadius = point.nRadius;
    nSign = point.nSign;

    // Free memory.
    delete[] padfDescriptor;

    // Copy descriptor values.
    padfDescriptor = new double[DESC_SIZE];
    for( int i = 0; i < DESC_SIZE; i++ )
        padfDescriptor[i] = point.padfDescriptor[i];

    return *this;
}

int  GDALFeaturePoint::GetX() const { return nX; }
void GDALFeaturePoint::SetX( int nXIn ) { nX = nXIn; }

int  GDALFeaturePoint::GetY() const { return nY; }
void GDALFeaturePoint::SetY( int nYIn ) { nY = nYIn; }

int  GDALFeaturePoint::GetScale() const { return nScale; }
void GDALFeaturePoint::SetScale( int nScaleIn ) { nScale = nScaleIn; }

int  GDALFeaturePoint::GetRadius() const { return nRadius; }
void GDALFeaturePoint::SetRadius( int nRadiusIn ) { nRadius = nRadiusIn; }

int  GDALFeaturePoint::GetSign() const { return nSign; }
void GDALFeaturePoint::SetSign( int nSignIn ) { nSign = nSignIn; }

double& GDALFeaturePoint::operator [] (int nIndex)
{
    if( nIndex < 0 || nIndex >= DESC_SIZE )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Descriptor index is out of range");
    }

    return padfDescriptor[nIndex];
}

GDALFeaturePoint::~GDALFeaturePoint() {
    delete[] padfDescriptor;
}

/************************************************************************/
/* ==================================================================== */
/*                            GDALSimpleSurf                            */
/* ==================================================================== */
/************************************************************************/

GDALSimpleSURF::GDALSimpleSURF( int nOctaveStartIn, int nOctaveEndIn ) :
    octaveStart(nOctaveStartIn),
    octaveEnd(nOctaveEndIn),
    // Initialize Octave map with custom range.
    poOctMap(new GDALOctaveMap(nOctaveStartIn, nOctaveEndIn))

{}

CPLErr GDALSimpleSURF::ConvertRGBToLuminosity(
    GDALRasterBand *red, GDALRasterBand *green, GDALRasterBand *blue,
    int nXSize, int nYSize, double **padfImg, int nHeight, int nWidth )
{
    if( red == nullptr || green == nullptr || blue == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Raster bands are not specified");
        return CE_Failure;
    }

    if( nXSize > red->GetXSize() || nYSize > red->GetYSize() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Red band has less size than has been requested");
        return CE_Failure;
    }

    if( padfImg == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Buffer isn't specified");
        return CE_Failure;
    }

    const double forRed = 0.21;
    const double forGreen = 0.72;
    const double forBlue = 0.07;

    const GDALDataType eRedType = red->GetRasterDataType();
    const GDALDataType eGreenType = green->GetRasterDataType();
    const GDALDataType eBlueType = blue->GetRasterDataType();

    const int dataRedSize = GDALGetDataTypeSizeBytes(eRedType);
    const int dataGreenSize = GDALGetDataTypeSizeBytes(eGreenType);
    const int dataBlueSize = GDALGetDataTypeSizeBytes(eBlueType);

    void *paRedLayer = CPLMalloc(dataRedSize * nWidth * nHeight);
    void *paGreenLayer = CPLMalloc(dataGreenSize * nWidth * nHeight);
    void *paBlueLayer = CPLMalloc(dataBlueSize * nWidth * nHeight);

    CPLErr eErr = red->RasterIO(GF_Read, 0, 0, nXSize, nYSize, paRedLayer,
                                nWidth, nHeight, eRedType, 0, 0, nullptr);
    if( eErr == CE_None )
        eErr = green->RasterIO(GF_Read, 0, 0, nXSize, nYSize, paGreenLayer,
                               nWidth, nHeight, eGreenType, 0, 0, nullptr);
    if( eErr == CE_None )
        eErr = blue->RasterIO(GF_Read, 0, 0, nXSize, nYSize, paBlueLayer,
                              nWidth, nHeight, eBlueType, 0, 0, nullptr);

    double maxValue = 255.0;
    for( int row = 0; row < nHeight && eErr == CE_None; row++ )
        for( int col = 0; col < nWidth; col++ )
        {
            // Get RGB values.
            const double dfRedVal = SRCVAL(paRedLayer, eRedType,
                                           nWidth * row + col * dataRedSize);
            const double dfGreenVal =
                SRCVAL(paGreenLayer, eGreenType,
                       nWidth * row + col * dataGreenSize);
            const double dfBlueVal = SRCVAL(paBlueLayer, eBlueType,
                                            nWidth * row + col * dataBlueSize);
            // Compute luminosity value.
            padfImg[row][col] = (
                dfRedVal * forRed +
                dfGreenVal * forGreen +
                dfBlueVal * forBlue) / maxValue;
        }

    CPLFree(paRedLayer);
    CPLFree(paGreenLayer);
    CPLFree(paBlueLayer);

    return eErr;
}

std::vector<GDALFeaturePoint>*
GDALSimpleSURF::ExtractFeaturePoints( GDALIntegralImage *poImg,
                                      double dfThreshold )
{
    std::vector<GDALFeaturePoint>* poCollection =
        new std::vector<GDALFeaturePoint>();

    // Calc Hessian values for layers.
    poOctMap->ComputeMap(poImg);

    // Search for extremum points.
    for( int oct = octaveStart; oct <= octaveEnd; oct++ )
    {
        for( int k = 0; k < GDALOctaveMap::INTERVALS - 2; k++ )
        {
            GDALOctaveLayer *bot = poOctMap->pMap[oct - 1][k];
            GDALOctaveLayer *mid = poOctMap->pMap[oct - 1][k + 1];
            GDALOctaveLayer *top = poOctMap->pMap[oct - 1][k + 2];

            for( int i = 0; i < mid->height; i++ )
            {
                for( int j = 0; j < mid->width; j++ )
                {
                    if( poOctMap->PointIsExtremum(i, j, bot, mid, top,
                                                  dfThreshold) )
                    {
                        GDALFeaturePoint oFP(j, i, mid->scale,
                                             mid->radius, mid->signs[i][j]);
                        SetDescriptor(&oFP, poImg);
                        poCollection->push_back(oFP);
                    }
                }
            }
        }
    }

    return poCollection;
}

double GDALSimpleSURF::GetEuclideanDistance(
    GDALFeaturePoint &firstPoint, GDALFeaturePoint &secondPoint)
{
    double sum = 0.0;

    for( int i = 0; i < GDALFeaturePoint::DESC_SIZE; i++ )
        sum += (firstPoint[i] - secondPoint[i]) *
               (firstPoint[i] - secondPoint[i]);

    return sqrt(sum);
}

void GDALSimpleSURF::NormalizeDistances(std::list<MatchedPointPairInfo> *poList)
{
    double max = 0.0;

    std::list<MatchedPointPairInfo>::iterator i;
    for( i = poList->begin(); i != poList->end(); ++i )
        if( (*i).euclideanDist > max )
            max = (*i).euclideanDist;

    if( max != 0.0 )
    {
        for( i = poList->begin(); i != poList->end(); ++i )
            (*i).euclideanDist /= max;
    }
}

void GDALSimpleSURF::SetDescriptor(
    GDALFeaturePoint *poPoint, GDALIntegralImage *poImg )
{
    // Affects to the descriptor area.
    const int haarScale = 20;

    // Side of the Haar wavelet.
    const int haarFilterSize = 2 * poPoint->GetScale();

    // Length of the side of the descriptor area.
    const int descSide = haarScale * poPoint->GetScale();

    // Side of the quadrant in 4x4 grid.
    const int quadStep = descSide / 4;

    // Side of the sub-quadrant in 5x5 regular grid of quadrant.
    const int subQuadStep = quadStep / 5;

    const int leftTop_row = poPoint->GetY() - (descSide / 2);
    const int leftTop_col = poPoint->GetX() - (descSide / 2);

    int count = 0;

    for( int r = leftTop_row; r < leftTop_row + descSide; r += quadStep )
        for( int c = leftTop_col; c < leftTop_col + descSide; c += quadStep )
        {
            double dx = 0;
            double dy = 0;
            double abs_dx = 0;
            double abs_dy = 0;

            for( int sub_r = r; sub_r < r + quadStep; sub_r += subQuadStep )
                for( int sub_c = c; sub_c < c + quadStep; sub_c += subQuadStep )
                {
                    // Approximate center of sub quadrant.
                    const int cntr_r = sub_r + subQuadStep / 2;
                    const int cntr_c = sub_c + subQuadStep / 2;

                    // Left top point for Haar wavelet computation.
                    const int cur_r = cntr_r - haarFilterSize / 2;
                    const int cur_c = cntr_c - haarFilterSize / 2;

                    // Gradients.
                    const double cur_dx =
                        poImg->HaarWavelet_X(cur_r, cur_c, haarFilterSize);
                    const double cur_dy =
                        poImg->HaarWavelet_Y(cur_r, cur_c, haarFilterSize);

                    dx += cur_dx;
                    dy += cur_dy;
                    abs_dx += fabs(cur_dx);
                    abs_dy += fabs(cur_dy);
                }

            // Fills point's descriptor.
            (*poPoint)[count++] = dx;
            (*poPoint)[count++] = dy;
            (*poPoint)[count++] = abs_dx;
            (*poPoint)[count++] = abs_dy;
        }
}

// TODO(schwehr): What does "value is 0,1." mean?  Is that 0 to 1 or 0.1?
// TODO(schwehr): 0,001?


CPLErr GDALSimpleSURF::MatchFeaturePoints(
    std::vector<GDALFeaturePoint*> *poMatchPairs,
    std::vector<GDALFeaturePoint> *poFirstCollect,
    std::vector<GDALFeaturePoint> *poSecondCollect,
    double dfThreshold )
{
/* -------------------------------------------------------------------- */
/*      Validate parameters.                                            */
/* -------------------------------------------------------------------- */
    if( poMatchPairs == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Matched points collection isn't specified" );
        return CE_Failure;
    }

    if( poFirstCollect == nullptr || poSecondCollect == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Feature point collections are not specified");
        return CE_Failure;
    }

/* ==================================================================== */
/*      Matching algorithm.                                             */
/* ==================================================================== */
    // Affects to false matching pruning.
    const double ratioThreshold = 0.8;

    int len_1 = static_cast<int>(poFirstCollect->size());
    int len_2 = static_cast<int>(poSecondCollect->size());

    const int minLength = std::min(len_1, len_2);

    // Temporary pointers. Used to swap collections.
    std::vector<GDALFeaturePoint> *p_1;
    std::vector<GDALFeaturePoint> *p_2;

    bool isSwap = false;

    // Assign p_1 - collection with minimal number of points.
    if( minLength == len_2 )
    {
        p_1 = poSecondCollect;
        p_2 = poFirstCollect;

        std::swap(len_1, len_2);
        isSwap = true;
    }
    else
    {
        // Assignment 'as is'.
        p_1 = poFirstCollect;
        p_2 = poSecondCollect;
        isSwap = false;
    }

    // Stores matched point indexes and their euclidean distances.
    std::list<MatchedPointPairInfo> *poPairInfoList =
        new std::list<MatchedPointPairInfo>();

    // Flags that points in the 2nd collection are matched or not.
    bool *alreadyMatched = new bool[len_2];
    for( int i = 0; i < len_2; i++ )
        alreadyMatched[i] = false;

    for( int i = 0; i < len_1; i++ )
    {
        // Distance to the nearest point.
        double bestDist = -1;
        // Index of the nearest point in p_2 collection.
        int bestIndex = -1;

        // Distance to the 2nd nearest point.
        double bestDist_2 = -1;

        // Find the nearest and 2nd nearest points.
        for( int j = 0; j < len_2; j++ )
            if( !alreadyMatched[j] )
                if( p_1->at(i).GetSign() == p_2->at(j).GetSign() )
                {
                    // Get distance between two feature points.
                    double curDist = GetEuclideanDistance(
                        p_1->at(i), p_2->at(j));

                    if( bestDist == -1 )
                    {
                        bestDist = curDist;
                        bestIndex = j;
                    }
                    else
                    {
                        if( curDist < bestDist )
                        {
                            bestDist = curDist;
                            bestIndex = j;
                        }
                    }

                    // Findes the 2nd nearest point.
                    if( bestDist_2 < 0 )
                        bestDist_2 = curDist;
                    else
                        if( curDist > bestDist && curDist < bestDist_2 )
                            bestDist_2 = curDist;
                }
/* -------------------------------------------------------------------- */
/*      False matching pruning.                                         */
/* If ratio bestDist to bestDist_2 greater than 0.8 =>                  */
/*     consider as false detection.                                     */
/* Otherwise, add points as matched pair.                               */
/*----------------------------------------------------------------------*/
        if( bestDist_2 > 0 && bestDist >= 0 )
            if( bestDist / bestDist_2 < ratioThreshold )
            {
                MatchedPointPairInfo info(i, bestIndex, bestDist);
                poPairInfoList->push_back(info);
                alreadyMatched[bestIndex] = true;
            }
    }

/* -------------------------------------------------------------------- */
/*      Pruning based on the provided threshold                         */
/* -------------------------------------------------------------------- */

    NormalizeDistances(poPairInfoList);

    std::list<MatchedPointPairInfo>::const_iterator iter;
    for( iter = poPairInfoList->begin(); iter != poPairInfoList->end(); ++iter )
    {
        if( (*iter).euclideanDist <= dfThreshold )
        {
            const int i_1 = (*iter).ind_1;
            const int i_2 = (*iter).ind_2;

            // Add copies into MatchedCollection.
            if( !isSwap )
            {
                poMatchPairs->push_back( &(p_1->at(i_1)) );
                poMatchPairs->push_back( &(p_2->at(i_2)) );
            }
            else
            {
                poMatchPairs->push_back( &(p_2->at(i_2)) );
                poMatchPairs->push_back( &(p_1->at(i_1)) );
            }
        }
    }

    // Clean up.
    delete[] alreadyMatched;
    delete poPairInfoList;

    return CE_None;
}

GDALSimpleSURF::~GDALSimpleSURF()
{
    delete poOctMap;
}
