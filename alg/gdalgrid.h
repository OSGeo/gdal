/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Gridding API.
 * Purpose:  Prototypes, and definitions for of GDAL scattered data gridder.
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2007, Andrey Kiselev <dron@ak4719.spb.edu>
 * Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef GDALGRID_H_INCLUDED
#define GDALGRID_H_INCLUDED

/**
 * \file gdalgrid.h
 *
 * GDAL gridder related entry points and definitions.
 */

#include "gdal_alg.h"

/*
 *  GridCreate Algorithm names
 */

static const char szAlgNameInvDist[] = "invdist";
static const char szAlgNameInvDistNearestNeighbor[] = "invdistnn";
static const char szAlgNameAverage[] = "average";
static const char szAlgNameNearest[] = "nearest";
static const char szAlgNameMinimum[] = "minimum";
static const char szAlgNameMaximum[] = "maximum";
static const char szAlgNameRange[] = "range";
static const char szAlgNameCount[] = "count";
static const char szAlgNameAverageDistance[] = "average_distance";
static const char szAlgNameAverageDistancePts[] = "average_distance_pts";
static const char szAlgNameLinear[] = "linear";

CPL_C_START

/*! @cond Doxygen_Suppress */
typedef CPLErr (*GDALGridFunction)( const void *, GUInt32,
                                    const double *, const double *,
                                    const double *,
                                    double, double, double *,
                                    void* );
/*! @endcond */

CPLErr
GDALGridInverseDistanceToAPower( const void *, GUInt32,
                                 const double *, const double *,
                                 const double *,
                                 double, double, double *,
                                 void* );
CPLErr
GDALGridInverseDistanceToAPowerNearestNeighbor( const void *, GUInt32,
                                 const double *, const double *,
                                 const double *,
                                 double, double, double *,
                                 void* );
CPLErr
GDALGridInverseDistanceToAPowerNoSearch( const void *, GUInt32,
                                         const double *, const double *,
                                         const double *,
                                         double, double, double *,
                                         void*  );
CPLErr
GDALGridMovingAverage( const void *, GUInt32,
                       const double *, const double *, const double *,
                       double, double, double *,
                       void*  );
CPLErr
GDALGridNearestNeighbor( const void *, GUInt32,
                         const double *, const double *, const double *,
                         double, double, double *,
                         void* );
CPLErr
GDALGridDataMetricMinimum( const void *, GUInt32,
                           const double *, const double *, const double *,
                           double, double, double *,
                           void*  );
CPLErr
GDALGridDataMetricMaximum( const void *, GUInt32,
                           const double *, const double *, const double *,
                           double, double, double *,
                           void*  );
CPLErr
GDALGridDataMetricRange( const void *, GUInt32,
                         const double *, const double *, const double *,
                         double, double, double *,
                         void*  );
CPLErr
GDALGridDataMetricCount( const void *, GUInt32,
                         const double *, const double *, const double *,
                         double, double, double *,
                         void*  );
CPLErr
GDALGridDataMetricAverageDistance( const void *, GUInt32,
                                   const double *, const double *,
                                   const double *, double, double, double *,
                                   void* );
CPLErr
GDALGridDataMetricAverageDistancePts( const void *, GUInt32,
                                      const double *, const double *,
                                      const double *, double, double,
                                      double *,
                                      void*  );
CPLErr
GDALGridLinear( const void *, GUInt32,
                                 const double *, const double *,
                                 const double *,
                                 double, double, double *,
                                 void* );

CPLErr CPL_DLL
ParseAlgorithmAndOptions( const char *,
                          GDALGridAlgorithm *,
                          void ** );
CPL_C_END

#endif /* GDALGRID_H_INCLUDED */
