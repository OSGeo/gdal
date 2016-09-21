/******************************************************************************
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

#include "gdal_simplesurf.h"

CPL_CVSID("$Id");

/************************************************************************/
/* ==================================================================== */
/*                          GDALIntegralImage                           */
/* ==================================================================== */
/************************************************************************/

GDALIntegralImage::GDALIntegralImage()
{
    pMatrix = NULL;
    nHeight = 0;
    nWidth = 0;
}

int GDALIntegralImage::GetHeight() { return nHeight; }

int GDALIntegralImage::GetWidth() { return nWidth; }

void GDALIntegralImage::Initialize(const double **padfImg, int nHeightIn, int nWidthIn)
{
    //Memory allocation
    pMatrix = new double*[nHeightIn];
    for (int i = 0; i < nHeightIn; i++)
        pMatrix[i] = new double[nWidthIn];

    nHeight = nHeightIn;
    nWidth = nWidthIn;

    //Integral image calculation
    for (int i = 0; i < nHeight; i++)
        for (int j = 0; j < nWidth; j++)
        {
            double val = padfImg[i][j];
            double a = 0, b = 0, c = 0;

            if (i - 1 >= 0 && j - 1 >= 0)
                a = pMatrix[i - 1][j - 1];
            if (j - 1 >= 0)
                b = pMatrix[i][j - 1];
            if (i - 1 >= 0)
                c = pMatrix[i - 1][j];

            //New value based on previous calculations
            pMatrix[i][j] = val - a + b + c;
        }
}

/*
 * Returns value of specified cell
 */
double GDALIntegralImage::GetValue(int nRow, int nCol)
{
    if ((nRow >= 0 && nRow < nHeight) && (nCol >= 0 && nCol < nWidth))
        return pMatrix[nRow][nCol];
    else
        return 0;
}

double GDALIntegralImage::GetRectangleSum(int nRow, int nCol, int nWidthIn, int nHeightIn)
{
    double a = 0, b = 0, c = 0, d = 0;

    //Left top point of rectangle is first
    int w = nWidthIn - 1;
    int h = nHeightIn - 1;

    int row = nRow;
    int col = nCol;

    //Left top point
    int lt_row = (row <= nHeight) ? (row - 1) : -1;
    int lt_col = (col <= nWidth) ? (col - 1) : -1;
    //Right bottom point of the rectangle
    int rb_row = (row + h < nHeight) ? (row + h) : (nHeight - 1);
    int rb_col = (col + w < nWidth) ? (col + w) : (nWidth - 1);

    if (lt_row >= 0 && lt_col >= 0)
        a = this->GetValue(lt_row, lt_col);

    if (lt_row >= 0 && rb_col >= 0)
        b = this->GetValue(lt_row, rb_col);

    if (rb_row >= 0 && rb_col >= 0)
        c = this->GetValue(rb_row, rb_col);

    if (rb_row >= 0 && lt_col >= 0)
        d = this->GetValue(rb_row, lt_col);

    double res = a + c - b - d;

    return (res > 0) ? res : 0;
}

double GDALIntegralImage::HaarWavelet_X(int nRow, int nCol, int nSize)
{
    return GetRectangleSum(nRow, nCol + nSize / 2, nSize / 2, nSize)
        - GetRectangleSum(nRow, nCol, nSize / 2, nSize);
}

double GDALIntegralImage::HaarWavelet_Y(int nRow, int nCol, int nSize)
{
    return GetRectangleSum(nRow + nSize / 2, nCol, nSize, nSize / 2)
        - GetRectangleSum(nRow, nCol, nSize, nSize / 2);
}

GDALIntegralImage::~GDALIntegralImage()
{
    //Clean up memory
    for (int i = 0; i < nHeight; i++)
        delete[] pMatrix[i];

    delete[] pMatrix;
}



/************************************************************************/
/* ==================================================================== */
/*                           GDALOctaveLayer                            */
/* ==================================================================== */
/************************************************************************/

GDALOctaveLayer::GDALOctaveLayer(int nOctave, int nInterval)
{
    this->octaveNum = nOctave;
    this->filterSize = 3 * ((int)pow(2.0, nOctave) * nInterval + 1);
    this->radius = (this->filterSize - 1) / 2;
    this->scale = (int)pow(2.0, nOctave);
    this->width = 0;
    this->height = 0;

    this->detHessians = NULL;
    this->signs = NULL;
}

void GDALOctaveLayer::ComputeLayer(GDALIntegralImage *poImg)
{
    this->width = poImg->GetWidth();
    this->height = poImg->GetHeight();

    //Allocate memory for arrays
    this->detHessians = new double*[this->height];
    this->signs = new int*[this->height];

    for (int i = 0; i < this->height; i++)
    {
        this->detHessians[i] = new double[this->width];
        this->signs[i] = new int[this->width];
    }
    //End Allocate memory for arrays

    //Values of Fast Hessian filters
    double dxx, dyy, dxy;
    // 1/3 of filter side
    int lobe = filterSize / 3;

    //Length of the longer side of the lobe in dxx and dyy filters
    int longPart = 2 * lobe - 1;

    int normalization = filterSize * filterSize;

    //Loop over image pixels
    //Filter should remain into image borders
    for (int r = radius; r <= height - radius; r++)
        for (int c = radius; c <= width - radius; c++)
        {
            dxx = poImg->GetRectangleSum(r - lobe + 1, c - radius, filterSize, longPart)
                - 3 * poImg->GetRectangleSum(r - lobe + 1, c - (lobe - 1) / 2, lobe, longPart);
            dyy = poImg->GetRectangleSum(r - radius, c - lobe - 1, longPart, filterSize)
                - 3 * poImg->GetRectangleSum(r - lobe + 1, c - lobe + 1, longPart, lobe);
            dxy = poImg->GetRectangleSum(r - lobe, c - lobe, lobe, lobe)
                + poImg->GetRectangleSum(r + 1, c + 1, lobe, lobe)
                - poImg->GetRectangleSum(r - lobe, c + 1, lobe, lobe)
                - poImg->GetRectangleSum(r + 1, c - lobe, lobe, lobe);

            dxx /= normalization;
            dyy /= normalization;
            dxy /= normalization;

            //Memorize Hessian values and their signs
            detHessians[r][c] = dxx * dyy - 0.9 * 0.9 * dxy * dxy;
            signs[r][c] = (dxx + dyy >= 0) ? 1 : -1;
        }
}

GDALOctaveLayer::~GDALOctaveLayer()
{
    for (int i = 0; i < height; i++)
    {
        delete[] detHessians[i];
        delete[] signs[i];
    }

    delete[] detHessians;
    delete[] signs;
}

/************************************************************************/
/* ==================================================================== */
/*                            GDALOctaveMap                             */
/* ==================================================================== */
/************************************************************************/

GDALOctaveMap::GDALOctaveMap(int nOctaveStart, int nOctaveEnd)
{
    this->octaveStart = nOctaveStart;
    this->octaveEnd = nOctaveEnd;

    pMap = new GDALOctaveLayer**[octaveEnd];

    for (int i = 0; i < nOctaveEnd; i++)
        pMap[i] = new GDALOctaveLayer*[INTERVALS];

    for (int oct = octaveStart; oct <= octaveEnd; oct++)
        for (int i = 1; i <= INTERVALS; i++)
            pMap[oct - 1][i - 1] = new GDALOctaveLayer(oct, i);
}

void GDALOctaveMap::ComputeMap(GDALIntegralImage *poImg)
{
    for (int oct = octaveStart; oct <= octaveEnd; oct++)
        for (int i = 1; i <= INTERVALS; i++)
            pMap[oct - 1][i - 1]->ComputeLayer(poImg);
}

bool GDALOctaveMap::PointIsExtremum(int row, int col, GDALOctaveLayer *bot,
                                    GDALOctaveLayer *mid, GDALOctaveLayer *top, double threshold)
{
    // Check that point in middle layer has all neighbors.
    if (row <= top->radius || col <= top->radius ||
        row + top->radius >= top->height || col + top->radius >= top->width)
        return false;

    double curPoint = mid->detHessians[row][col];

    // Hessian should be higher than threshold.
    if (curPoint < threshold)
        return false;

    // Hessian should be higher than Hessians of all neighbors
    for (int i = -1; i <= 1; i++)
        for (int j = -1; j <= 1; j++)
        {
            double topPoint = top->detHessians[row + i][col + j];
            double midPoint = mid->detHessians[row + i][col + j];
            double botPoint = bot->detHessians[row + i][col + j];

            if (topPoint >= curPoint || botPoint >= curPoint)
                return false;

            if (i != 0 || j != 0)
                if (midPoint >= curPoint)
                    return false;
        }

    return true;
}

GDALOctaveMap::~GDALOctaveMap()
{
    // Clean up Octave layers
    for (int oct = octaveStart; oct <= octaveEnd; oct++)
        for(int i = 0; i < INTERVALS; i++)
            delete pMap[oct - 1][i];

    //Clean up allocated memory
    for (int oct = 0; oct < octaveEnd; oct++)
        delete[] pMap[oct];

    delete[] pMap;
}
