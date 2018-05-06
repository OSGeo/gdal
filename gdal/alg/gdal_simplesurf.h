/******************************************************************************
 * $Id$
 * Project:  GDAL
 * Purpose:  Correlator
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

/**
 * @file
 * @author Andrew Migal migal.drew@gmail.com
 * @brief Class for searching corresponding points on images.
 */

#ifndef GDALSIMPLESURF_H_
#define GDALSIMPLESURF_H_

#include "gdal_priv.h"
#include "cpl_conv.h"
#include <list>

/**
 * @brief Class of "feature point" in raster. Used by SURF-based algorithm.
 *
 * @details This point presents coordinates of distinctive pixel in image.
 * In computer vision, feature points - the most "strong" and "unique"
 * pixels (or areas) in picture, which can be distinguished from others.
 * For more details, see FAST corner detector, SIFT, SURF and similar algorithms.
 */
class GDALFeaturePoint
{
public:
    /**
     * Standard constructor. Initializes all parameters with negative numbers
     * and allocates memory for descriptor.
     */
    GDALFeaturePoint();

    /**
     * Copy constructor
     * @param fp Copied instance of GDALFeaturePoint class
     */
    GDALFeaturePoint(const GDALFeaturePoint& fp);

    /**
     * Create instance of GDALFeaturePoint class
     *
     * @param nX X-coordinate (pixel)
     * @param nY Y-coordinate (line)
     * @param nScale Scale which contains this point (2, 4, 8, 16 and so on)
     * @param nRadius Half of the side of descriptor area
     * @param nSign Sign of Hessian determinant for this point
     *
     * @note This constructor normally is invoked by SURF-based algorithm,
     * which provides all necessary parameters.
     */
    GDALFeaturePoint(int nX, int nY, int nScale, int nRadius, int nSign);
    virtual ~GDALFeaturePoint();

    /** Assignment operator */
    GDALFeaturePoint& operator=(const GDALFeaturePoint& point);

    /**
     * Provide access to point's descriptor.
     *
     * @param nIndex Position of descriptor's value.
     * nIndex should be within range from 0 to DESC_SIZE (in current version - 64)
     *
     * @return Reference to value of descriptor in 'nIndex' position.
     * If index is out of range then behaviour is undefined.
     */
    double& operator[](int nIndex);

    /** Descriptor length */
    static const int DESC_SIZE = 64;

    /**
     * Fetch X-coordinate (pixel) of point
     *
     * @return X-coordinate in pixels
     */
    int GetX() const;

    /**
     * Set X coordinate of point
     *
     * @param nX X coordinate in pixels
     */
    void SetX(int nX);

    /**
     * Fetch Y-coordinate (line) of point.
     *
     * @return Y-coordinate in pixels.
     */
    int GetY() const;

    /**
     * Set Y coordinate of point.
     *
     * @param nY Y coordinate in pixels.
     */
    void SetY(int nY);

    /**
     * Fetch scale of point.
     *
     * @return Scale for this point.
     */
    int GetScale() const ;

    /**
     * Set scale of point.
     *
     * @param nScale Scale for this point.
     */
    void SetScale(int nScale);

    /**
     * Fetch radius of point.
     *
     * @return Radius for this point.
     */
    int  GetRadius() const;

    /**
     * Set radius of point.
     *
     * @param nRadius Radius for this point.
     */
    void SetRadius(int nRadius);

    /**
     * Fetch sign of Hessian determinant of point.
     *
     * @return Sign for this point.
     */
    int GetSign() const;

    /**
     * Set sign of point.
     *
     * @param nSign Sign of Hessian determinant for this point.
     */
    void SetSign(int nSign);

private:
    // Coordinates of point in image
    int nX;
    int nY;
    // --------------------
    int nScale;
    int nRadius;
    int nSign;
    // Descriptor array
    double *padfDescriptor;
};

/**
 * @author Andrew Migal migal.drew@gmail.com
 * @brief Integral image class (summed area table).
 * @details Integral image is a table for fast computing the sum of
 * values in rectangular subarea. In more detail, for 2-dimensional array
 * of numbers this class provides capability to get sum of values in
 * rectangular arbitrary area with any size in constant time.
 * Integral image is constructed from grayscale picture.
 */
class GDALIntegralImage
{
    CPL_DISALLOW_COPY_ASSIGN(GDALIntegralImage)

public:
    GDALIntegralImage();
    virtual ~GDALIntegralImage();

    /**
     * Compute integral image for specified array. Result is stored internally.
     *
     * @param padfImg Pointer to 2-dimensional array of values
     * @param nHeight Number of rows in array
     * @param nWidth Number of columns in array
     */
    void Initialize(const double **padfImg, int nHeight, int nWidth);

    /**
     * Fetch value of specified position in integral image.
     *
     * @param nRow Row of this position
     * @param nCol Column of this position
     *
     * @return Value in specified position or zero if parameters are out of range.
     */
    double GetValue(int nRow, int nCol);

    /**
     * Get sum of values in specified rectangular grid. Rectangle is constructed
     * from left top point.
     *
     * @param nRow Row of left top point of rectangle
     * @param nCol Column of left top point of rectangle
     * @param nWidth Width of rectangular area (number of columns)
     * @param nHeight Height of rectangular area (number of rows)
     *
     * @return Sum of values in specified grid.
     */
    double GetRectangleSum(int nRow, int nCol, int nWidth, int nHeight);

    /**
     * Get value of horizontal Haar wavelet in specified square grid.
     *
     * @param nRow Row of left top point of square
     * @param nCol Column of left top point of square
     * @param nSize Side of the square
     *
     * @return Value of horizontal Haar wavelet in specified square grid.
     */
    double HaarWavelet_X(int nRow, int nCol, int nSize);

    /**
     * Get value of vertical Haar wavelet in specified square grid.
     *
     * @param nRow Row of left top point of square
     * @param nCol Column of left top point of square
     * @param nSize Side of the square
     *
     * @return Value of vertical Haar wavelet in specified square grid.
     */
    double HaarWavelet_Y(int nRow, int nCol, int nSize);

    /**
     * Fetch height of integral image.
     *
     * @return Height of integral image (number of rows).
     */
    int GetHeight();

    /**
     * Fetch width of integral image.
     *
     * @return Width of integral image (number of columns).
     */
    int GetWidth();

private:
    double **pMatrix = nullptr;
    int nWidth = 0;
    int nHeight = 0;
};

/**
 * @author Andrew Migal migal.drew@gmail.com
 * @brief Class for computation and storage of Hessian values in SURF-based algorithm.
 *
 * @details SURF-based algorithm normally uses this class for searching
 * feature points on raster images. Class also contains traces of Hessian matrices
 * to provide fast computations.
 */
class GDALOctaveLayer
{
    CPL_DISALLOW_COPY_ASSIGN(GDALOctaveLayer)

public:
    GDALOctaveLayer();

    /**
     * Create instance with provided parameters.
     *
     * @param nOctave Number of octave which contains this layer
     * @param nInterval Number of position in octave
     *
     * @note Normally constructor is invoked only by SURF-based algorithm.
     */
    GDALOctaveLayer(int nOctave, int nInterval);
    virtual ~GDALOctaveLayer();

    /**
     * Perform calculation of Hessian determinants and their signs
     * for specified integral image. Result is stored internally.
     *
     * @param poImg Integral image object, which provides all necessary
     * data for computation
     *
     * @note Normally method is invoked only by SURF-based algorithm.
     */
    void ComputeLayer(GDALIntegralImage *poImg);

    /**
     * Octave which contains this layer (1,2,3...)
     */
    int octaveNum;
    /**
     * Length of the side of filter
     */
    int filterSize;
    /**
     * Length of the border
     */
    int radius;
    /**
     * Scale for this layer
     */
    int scale;
    /**
     * Image width in pixels
     */
    int width;
    /**
     * Image height in pixels
     */
    int height;
    /**
     * Hessian values for image pixels
     */
    double **detHessians;
    /**
     * Hessian signs for speeded matching
     */
    int **signs;
};

/**
 * @author Andrew Migal migal.drew@gmail.com
 * @brief Class for handling octave layers in SURF-based algorithm.
 * @details Class contains OctaveLayers and provides capability to construct octave space and distinguish
 * feature points. Normally this class is used only by SURF-based algorithm.
 */
class GDALOctaveMap
{
    CPL_DISALLOW_COPY_ASSIGN( GDALOctaveMap )

public:
    /**
     * Create octave space. Octave numbers are start with one. (1, 2, 3, 4, ... )
     *
     * @param nOctaveStart Number of bottom octave
     * @param nOctaveEnd Number of top octave. Should be equal or greater than OctaveStart
     */
    GDALOctaveMap(int nOctaveStart, int nOctaveEnd);
    virtual ~GDALOctaveMap();

    /**
     * Calculate Hessian values for octave space
     * (for all stored octave layers) using specified integral image
     * @param poImg Integral image instance which provides necessary data
     * @see GDALOctaveLayer
     */
    void ComputeMap(GDALIntegralImage *poImg);

    /**
     * Method makes decision that specified point
     * in middle octave layer is maximum among all points
     * from 3x3x3 neighbourhood (surrounding points in
     * bottom, middle and top layers). Provided layers should be from the same octave's interval.
     * Detects feature points.
     *
     * @param row Row of point, which is candidate to be feature point
     * @param col Column of point, which is candidate to be feature point
     * @param bot Bottom octave layer
     * @param mid Middle octave layer
     * @param top Top octave layer
     * @param threshold Threshold for feature point recognition. Detected feature point
     * will have Hessian value greater than this provided threshold.
     *
     * @return TRUE if candidate was evaluated as feature point or FALSE otherwise.
     */
    static bool PointIsExtremum(int row, int col, GDALOctaveLayer *bot,
                         GDALOctaveLayer *mid, GDALOctaveLayer *top, double threshold);

    /**
     * 2-dimensional array of octave layers
     */
    GDALOctaveLayer ***pMap;

    /**
     * Value for constructing internal octave space
     */
    static const int INTERVALS = 4;

    /**
     * Number of bottom octave
     */
    int octaveStart;

    /**
     * Number of top octave. Should be equal or greater than OctaveStart
     */
    int octaveEnd;
};

/**
 * @author Andrew Migal migal.drew@gmail.com
 * @brief Class for searching corresponding points on images.
 * @details Provides capability for detection feature points
 * and finding equal points on different images.
 * Class implements simplified version of SURF algorithm (Speeded Up Robust Features).
 * As original, this realization is scale invariant, but sensitive to rotation.
 * Images should have similar rotation angles (maximum difference is up to 10-15 degrees),
 * otherwise algorithm produces incorrect and very unstable results.
 */

class GDALSimpleSURF
{
private:
    /**
     * Class stores indexes of pair of point
     * and distance between them.
     */
    class MatchedPointPairInfo
    {
    public:
        MatchedPointPairInfo(int nInd_1, int nInd_2, double dfDist):
            ind_1(nInd_1), ind_2(nInd_2), euclideanDist(dfDist) {}

        int ind_1;
        int ind_2;
        double euclideanDist;
    };

    CPL_DISALLOW_COPY_ASSIGN( GDALSimpleSURF )

public:
    /**
     * Prepare class according to specified parameters. Octave numbers affects
     * to amount of detected points and their robustness.
     * Range between bottom and top octaves also affects to required time of detection points
     * (if range is large, algorithm should perform more operations).
     * @param nOctaveStart Number of bottom octave. Octave numbers starts with one
     * @param nOctaveEnd Number of top octave. Should be equal or greater than OctaveStart
     *
     * @note
     * Every octave finds points with specific size. For small images
     * use small octave numbers, for high resolution - large.
     * For 1024x1024 images it's normal to use any octave numbers from range 1-6.
     * (for example, octave start - 1, octave end - 3, or octave start - 2, octave end - 2.)
     * For larger images, try 1-10 range or even higher.
     * Pay attention that number of detected point decreases quickly per octave
     * for particular image. Algorithm finds more points in case of small octave numbers.
     * If method detects nothing, reduce bottom bound of octave range.
     *
     * NOTICE that every octave requires time to compute. Use a little range
     * or only one octave if execution time is significant.
     */
    GDALSimpleSURF(int nOctaveStart, int nOctaveEnd);
    virtual ~GDALSimpleSURF();

    /**
     * Convert image with RGB channels to grayscale using "luminosity" method.
     * Result is used in SURF-based algorithm, but may be used anywhere where
     * grayscale images with nice contrast are required.
     *
     * @param red Image's red channel
     * @param green Image's green channel
     * @param blue Image's blue channel
     * @param nXSize Width of initial image
     * @param nYSize Height of initial image
     * @param padfImg Array for resulting grayscale image
     * @param nHeight Height of resulting image
     * @param nWidth Width of resulting image
     *
     * @return CE_None or CE_Failure if error occurs.
     */
    static CPLErr ConvertRGBToLuminosity(
        GDALRasterBand *red,
        GDALRasterBand *green,
        GDALRasterBand *blue,
        int nXSize, int nYSize,
        double **padfImg, int nHeight, int nWidth);

    /**
     * Find feature points using specified integral image.
     *
     * @param poImg Integral image to be used
     * @param dfThreshold Threshold for feature point recognition. Detected feature point
     * will have Hessian value greater than this provided threshold.
     *
     * @note Typical threshold's value is 0,001. But this value
     * can be various in each case and depends on image's nature.
     * For example, value can be 0.002 or 0.005.
     * Fill free to experiment with it.
     * If threshold is high, than number of detected feature points is small,
     * and vice versa.
     */
    std::vector<GDALFeaturePoint>*
    ExtractFeaturePoints(GDALIntegralImage *poImg, double dfThreshold);

    /**
     * Find corresponding points (equal points in two collections).
     *
     * @param poMatchPairs Resulting collection for matched points
     * @param poFirstCollect Points on the first image
     * @param poSecondCollect Points on the second image
     * @param dfThreshold Value from 0 to 1. Threshold affects to number of
     * matched points. If threshold is lower, amount of corresponding
     * points is larger, and vice versa
     *
     * @return CE_None or CE_Failure if error occurs.
     */
    static CPLErr MatchFeaturePoints(
        std::vector<GDALFeaturePoint*> *poMatchPairs,
        std::vector<GDALFeaturePoint> *poFirstCollect,
        std::vector<GDALFeaturePoint> *poSecondCollect,
        double dfThreshold);

private:
    /**
     * Compute euclidean distance between descriptors of two feature points.
     * It's used in comparison and matching of points.
     *
     * @param firstPoint First feature point to be compared
     * @param secondPoint Second feature point to be compared
     *
     * @return Euclidean distance between descriptors.
     */
    static double GetEuclideanDistance(
        GDALFeaturePoint &firstPoint, GDALFeaturePoint &secondPoint);

    /**
     * Set provided distance values to range from 0 to 1.
     *
     * @param poList List of distances to be normalized
     */
    static void NormalizeDistances(std::list<MatchedPointPairInfo> *poList);

    /**
     * Compute descriptor for specified feature point.
     *
     * @param poPoint Feature point instance
     * @param poImg image where feature point was found
     */
    static void SetDescriptor(GDALFeaturePoint *poPoint, GDALIntegralImage *poImg);

private:
    int octaveStart;
    int octaveEnd;
    GDALOctaveMap *poOctMap;
};

#endif /* GDALSIMPLESURF_H_ */
