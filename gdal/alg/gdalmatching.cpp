/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  GDAL Wrapper for image matching via correlation algorithm.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 * Author:   Andrew Migal, migal.drew@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2012, Frank Warmerdam
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

#include "gdal_alg.h"
#include "gdal_simplesurf.h"

//! @cond Doxygen_Suppress
CPL_CVSID("$Id$")
//! @endcond

// TODO(schwehr): What?  This below: "0,001"

/**
 * @file
 * @author Andrew Migal migal.drew@gmail.com
 * @brief Algorithms for searching corresponding points on images.
 * @details This implementation is  based on an simplified version
 * of SURF algorithm (Speeded Up Robust Features).
 * Provides capability for detection feature points
 * and finding equal points on different images.
 * As original, this realization is scale invariant, but sensitive to rotation.
 * Images should have similar rotation angles (maximum difference is up to 10-15
 * degrees), otherwise algorithm produces incorrect and very unstable results.
 */

/**
 * Detect feature points on provided image. Please carefully read documentation
 * below.
 *
 * @param poDataset Image on which feature points will be detected
 * @param panBands Array of 3 raster bands numbers, for Red, Green, Blue bands
 * (in that order)
 * @param nOctaveStart Number of bottom octave. Octave numbers starts from one.
 * This value directly and strongly affects to amount of recognized points
 * @param nOctaveEnd Number of top octave. Should be equal or greater than
 * octaveStart
 * @param dfThreshold Value from 0 to 1. Threshold for feature point
 * recognition.  Number of detected points is larger if threshold is lower
 *
 * @see GDALFeaturePoint, GDALSimpleSURF class for details.
 *
 * @note Every octave finds points in specific size. For small images
 * use small octave numbers, for high resolution - large.
 * For 1024x1024 images it's normal to use any octave numbers from range 1-6.
 * (for example, octave start - 1, octave end - 3, or octave start - 2, octave
 * end - 2.)
 * For larger images, try 1-10 range or even higher.
 * Pay attention that number of detected point decreases quickly per octave for
 * particular image. Algorithm finds more points in case of small octave number.
 * If method detects nothing, reduce octave start value.
 * In addition, if many feature points are required (the largest possible
 * amount), use the lowest octave start value (1) and wide octave range.
 *
 * @note Typical threshold's value is 0,001. It's pretty good for all images.
 * But this value depends on image's nature and may be various in each
 * particular case.
 * For example, value can be 0,002 or 0,005.
 * Notice that number of detected points is larger if threshold is lower.
 * But with high threshold feature points will be better - "stronger", more
 * "unique" and distinctive.
 *
 * Feel free to experiment with parameters, because character, robustness and
 * number of points entirely depend on provided range of octaves and threshold.
 *
 * NOTICE that every octave requires time to compute. Use a little range
 * or only one octave, if execution time is significant.
 *
 * @return CE_None or CE_Failure if error occurs.
 */

static std::vector<GDALFeaturePoint> *
GatherFeaturePoints( GDALDataset* poDataset, int* panBands,
                     int nOctaveStart, int nOctaveEnd, double dfThreshold )
{
    if( poDataset == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "GDALDataset isn't specified");
        return NULL;
    }

    if( panBands == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Raster bands are not specified");
        return NULL;
    }

    if( nOctaveStart <= 0 || nOctaveEnd < 0 ||
        nOctaveStart > nOctaveEnd )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Octave numbers are invalid");
        return NULL;
    }

    if( dfThreshold < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Threshold have to be greater than zero");
        return NULL;
    }

    GDALRasterBand *poRstRedBand = poDataset->GetRasterBand(panBands[0]);
    GDALRasterBand *poRstGreenBand = poDataset->GetRasterBand(panBands[1]);
    GDALRasterBand *poRstBlueBand = poDataset->GetRasterBand(panBands[2]);

    const int nWidth = poRstRedBand->GetXSize();
    const int nHeight = poRstRedBand->GetYSize();

    if( nWidth == 0 || nHeight == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Must have non-zero width and height.");
        return NULL;
    }

    // Allocate memory for grayscale image.
    double **padfImg = new double*[nHeight];
    for( int i = 0; ; )
    {
        padfImg[i] = new double[nWidth];
        for( int j = 0; j < nWidth; ++j )
          padfImg[i][j] = 0.0;
        ++i;
        if( i == nHeight )
            break;
    }

    // Create grayscale image.
    GDALSimpleSURF::ConvertRGBToLuminosity(
        poRstRedBand, poRstGreenBand, poRstBlueBand, nWidth, nHeight,
        padfImg, nHeight, nWidth);

    // Prepare integral image.
    GDALIntegralImage *poImg = new GDALIntegralImage();
    poImg->Initialize((const double**)padfImg, nHeight, nWidth);

    // Get feature points.
    GDALSimpleSURF *poSurf = new GDALSimpleSURF(nOctaveStart, nOctaveEnd);

    std::vector<GDALFeaturePoint> *poCollection =
        poSurf->ExtractFeaturePoints(poImg, dfThreshold);

    // Clean up.
    delete poImg;
    delete poSurf;

    for( int i = 0; i < nHeight; ++i )
        delete[] padfImg[i];

    delete[] padfImg;

    return poCollection;
}

/************************************************************************/
/*                     GDALComputeMatchingPoints()                      */
/************************************************************************/

/** GDALComputeMatchingPoints. TODO document */
GDAL_GCP CPL_DLL *
GDALComputeMatchingPoints( GDALDatasetH hFirstImage,
                           GDALDatasetH hSecondImage,
                           char **papszOptions,
                           int *pnGCPCount )
{
    *pnGCPCount = 0;

/* -------------------------------------------------------------------- */
/*      Override default algorithm parameters.                          */
/* -------------------------------------------------------------------- */
    int nOctaveStart, nOctaveEnd;
    double dfSURFThreshold;

    nOctaveStart =atoi(CSLFetchNameValueDef(papszOptions, "OCTAVE_START", "2"));
    nOctaveEnd = atoi(CSLFetchNameValueDef(papszOptions, "OCTAVE_END", "2"));

    dfSURFThreshold = CPLAtof(
        CSLFetchNameValueDef(papszOptions, "SURF_THRESHOLD", "0.001"));
    const double dfMatchingThreshold = CPLAtof(
        CSLFetchNameValueDef(papszOptions, "MATCHING_THRESHOLD", "0.015"));

/* -------------------------------------------------------------------- */
/*      Identify the bands to use.  For now we are effectively          */
/*      limited to using RGB input so if we have one band only treat    */
/*      it as red=green=blue=band 1.  Disallow non eightbit imagery.    */
/* -------------------------------------------------------------------- */
    int anBandMap1[3] = { 1, 1, 1 };
    if( GDALGetRasterCount(hFirstImage) >= 3 )
    {
        anBandMap1[1] = 2;
        anBandMap1[2] = 3;
    }

    int anBandMap2[3] = { 1, 1, 1 };
    if( GDALGetRasterCount(hSecondImage) >= 3 )
    {
        anBandMap2[1] = 2;
        anBandMap2[2] = 3;
    }

/* -------------------------------------------------------------------- */
/*      Collect reference points on each image.                         */
/* -------------------------------------------------------------------- */
    std::vector<GDALFeaturePoint> *poFPCollection1 =
        GatherFeaturePoints(reinterpret_cast<GDALDataset *>(hFirstImage),
                            anBandMap1,
                            nOctaveStart, nOctaveEnd, dfSURFThreshold);
    if( poFPCollection1 == NULL )
        return NULL;

    std::vector<GDALFeaturePoint> *poFPCollection2 =
        GatherFeaturePoints(reinterpret_cast<GDALDataset *>(hSecondImage),
                            anBandMap2,
                            nOctaveStart, nOctaveEnd,
                            dfSURFThreshold);

    if( poFPCollection2 == NULL )
    {
        delete poFPCollection1;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to find corresponding locations.                            */
/* -------------------------------------------------------------------- */
    std::vector<GDALFeaturePoint *> oMatchPairs;

    if( CE_None != GDALSimpleSURF::MatchFeaturePoints(
        &oMatchPairs, poFPCollection1, poFPCollection2,
        dfMatchingThreshold ))
    {
        delete poFPCollection1;
        delete poFPCollection2;
        return NULL;
    }

    *pnGCPCount = static_cast<int>(oMatchPairs.size()) / 2;

/* -------------------------------------------------------------------- */
/*      Translate these into GCPs - but with the output coordinate      */
/*      system being pixel/line on the second image.                    */
/* -------------------------------------------------------------------- */
    GDAL_GCP *pasGCPList =
        static_cast<GDAL_GCP*>(CPLCalloc(*pnGCPCount, sizeof(GDAL_GCP)));

    GDALInitGCPs(*pnGCPCount, pasGCPList);

    for( int i=0; i < *pnGCPCount; i++ )
    {
        GDALFeaturePoint *poPoint1 = oMatchPairs[i*2  ];
        GDALFeaturePoint *poPoint2 = oMatchPairs[i*2+1];

        pasGCPList[i].dfGCPPixel = poPoint1->GetX() + 0.5;
        pasGCPList[i].dfGCPLine = poPoint1->GetY() + 0.5;

        pasGCPList[i].dfGCPX = poPoint2->GetX() + 0.5;
        pasGCPList[i].dfGCPY = poPoint2->GetY() + 0.5;
        pasGCPList[i].dfGCPZ = 0.0;
    }

    // Cleanup the feature point lists.
    delete poFPCollection1;
    delete poFPCollection2;

/* -------------------------------------------------------------------- */
/*      Optionally transform into the georef coordinates of the         */
/*      output image.                                                   */
/* -------------------------------------------------------------------- */
    const bool bGeorefOutput =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "OUTPUT_GEOREF", "NO"));

    if( bGeorefOutput )
    {
        double adfGeoTransform[6] = {};

        GDALGetGeoTransform( hSecondImage, adfGeoTransform );

        for( int i=0; i < *pnGCPCount; i++ )
        {
            GDALApplyGeoTransform(adfGeoTransform,
                                  pasGCPList[i].dfGCPX,
                                  pasGCPList[i].dfGCPY,
                                  &(pasGCPList[i].dfGCPX),
                                  &(pasGCPList[i].dfGCPY));
        }
    }

    return pasGCPList;
}
