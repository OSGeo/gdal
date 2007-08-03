/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Gridder.
 * Purpose:  Implementation of GDAL scattered data gridder.
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2007, Andrey Kiselev <dron@ak4719.spb.edu>
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

#include "gdalgridder.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                   GDALGridInverseDistanceToAPower()                  */
/************************************************************************/

/**
 * Inverse distance to a power.
 *
 * The Inverse Distance to a Power gridding method is a weighted average
 * interpolator. You should supply the input arrays with the scattered data
 * values including coordinates of every data point and output grid geometry.
 * The function will calculate interpolated value for the given position in
 * output grid.
 *
 * @param poOptions Algorithm parameters. This should point to
 * GDALGridInverseDistanceToAPowerOptions object. 
 * @param nPoints Number of elements in input arrays.
 * @param dfX Input array of X coordinates. 
 * @param dfY Input array of Y coordinates. 
 * @param dfZ Input array of Z values. 
 * @param dfXMin Lowest X border of output grid.
 * @param dfXMax Highest X border of output grid.
 * @param dfYMin Lowest Y border of output grid.
 * @param dfYMax Highest Y border of output grid.
 * @param nXSize Number of columns in output grid.
 * @param nYSize Number of rows in output grid.
 * @param nXPoint X position of the point to calculate.  
 * @param nYPoint Y position of the point to calculate.
 * @param pdfValue Pointer to variable where the calculated value will be
 * returned.
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr
GDALGridInverseDistanceToAPower( void *poOptions, GUInt32 nPoints,
                                 double *dfX, double *dfY, double *dfZ,
                                 double dfXMin, double dfXMax,
                                 double dfYMin, double dfYMax,
                                 GUInt32 nXSize, GUInt32 nYSize,
                                 GUInt32 nXPoint, GUInt32 nYPoint,
                                 double *pdfValue )
{
    double  dfNominator = 0.0, dfDenominator = 0.0;
    GUInt32 i = 0;
    double  dfDeltaX = ( dfXMax - dfXMin ) / nXSize;
    double  dfDeltaY = ( dfYMax - dfYMin ) / nYSize;
    double  dfXBase = dfXMin + dfDeltaX / 2;
    double  dfYBase = dfYMin + dfDeltaY / 2;
    double dfPower =
        ((GDALGridInverseDistanceToAPowerOptions *)poOptions)->dfPower;

    while ( i < nPoints )
    {
        double  dfRX = dfXBase + nXPoint * dfDeltaX - dfX[i];
        double  dfRY = dfYBase + nYPoint * dfDeltaY - dfY[i];
        if ( dfRX == 0.0 && dfRY == 0.0 )
        {
            (*pdfValue) = dfZ[i];
            return CE_None;
        }
        double  dfH = pow( sqrt(dfRX * dfRX + dfRY * dfRY), dfPower );
        dfNominator += dfZ[i] / dfH;
        dfDenominator += 1.0 / dfH;
        i++;
    }

    (*pdfValue) = dfNominator / dfDenominator;

    return CE_None;
}

/************************************************************************/
/*                            GDALGridCreate()                          */
/************************************************************************/

/**
 * Create regular grid from the scattered data.
 *
 * This fucntion takes the arrays of X and Y coordinates and corresponding Z
 * values as input and computes regular grid (or call it a raster) from these
 * scattered data. You should supply geometry and extent of the output grid
 * and allocate array sufficient to hold such a grid.
 *
 * @param eAlgorithm Gridding method. 
 * @param poOptions Options to control choosen gridding method.
 * @param nPoints Number of elements in input arrays.
 * @param dfX Input array of X coordinates. 
 * @param dfY Input array of Y coordinates. 
 * @param dfZ Input array of Z values. 
 * @param dfXMin Lowest X border of output grid.
 * @param dfXMax Highest X border of output grid.
 * @param dfYMin Lowest Y border of output grid.
 * @param dfYMax Highest Y border of output grid.
 * @param nXSize Number of columns in output grid.
 * @param nYSize Number of rows in output grid.
 * @param eType Data type of output array.  
 * @param pData Pointer to array where the calculated grid will be stored.
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 * @param pProgressArg argument to be passed to pfnProgress.  May be NULL.
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr
GDALGridCreate( GDALGridAlgorithm eAlgorithm, void *poOptions,
                GUInt32 nPoints, double *dfX, double *dfY, double *dfZ,
                double dfXMin, double dfXMax, double dfYMin, double dfYMax,
                GUInt32 nXSize, GUInt32 nYSize, GDALDataType eType, void *pData,
                GDALProgressFunc pfnProgress, void *pProgressArg )
{
    CPLAssert( poOptions );
    CPLAssert( dfX );
    CPLAssert( dfY );
    CPLAssert( dfZ );
    CPLAssert( pData );

    GDALGridFunction    pfnGDALGridMethod;

    switch ( eAlgorithm )
    {
        case GGA_InverseDistanceToAPower:
            pfnGDALGridMethod = GDALGridInverseDistanceToAPower;
            break;

        default:
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "GDAL does not support gridding method %d", eAlgorithm );
	    return CE_Failure;
    }

    GUInt32             nXPoint, nYPoint;

    for ( nYPoint = 0; nYPoint < nYSize; nYPoint++ )
    {
        for ( nXPoint = 0; nXPoint < nXSize; nXPoint++ )
        {
            double  dfValue = 0.0;
            if ( (*pfnGDALGridMethod)( poOptions, nPoints, dfX, dfY, dfZ,
                                       dfXMin, dfXMax, dfYMin, dfYMax,
                                       nXSize, nYSize,
                                       nXPoint, nYPoint, &dfValue ) != CE_None )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Gridding failed at X position %lu, Y position %lu",
                          nXPoint, nYPoint );
                return CE_Failure;
            }

            if ( eType == GDT_Byte )
                ((GByte *)pData)[nYPoint * nXSize + nXPoint] = (GByte)dfValue;
            else if ( eType == GDT_UInt16 )
                ((GUInt16 *)pData)[nYPoint * nXSize + nXPoint] = (GUInt16)dfValue;
            else if ( eType == GDT_Int16 )
                ((GInt16 *)pData)[nYPoint * nXSize + nXPoint] = (GInt16)dfValue;
            else if ( eType == GDT_UInt32 )
                ((GUInt32 *)pData)[nYPoint * nXSize + nXPoint] = (GUInt32)dfValue;
            else if ( eType == GDT_Int32 )
                ((GInt32 *)pData)[nYPoint * nXSize + nXPoint] = (GInt32)dfValue;
            else if ( eType == GDT_Float32 )
                ((float *)pData)[nYPoint * nXSize + nXPoint] = (float)dfValue;
            else if ( eType == GDT_Float64 )
                ((double *)pData)[nYPoint * nXSize + nXPoint] = dfValue;
        }

	if( !pfnProgress( (double)nYPoint / (nYSize - 1), NULL, pProgressArg ) )
	{
	    CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
	    return CE_Failure;
	}
    }

    return CE_None;
}


