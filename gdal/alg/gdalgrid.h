/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Gridder.
 * Purpose:  Prototypes, and definitions for of GDAL scattered data gridder.
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

#ifndef GDALGRIDDER_H_INCLUDED
#define GDALGRIDDER_H_INCLUDED

/**
 * \file gdalgridder.h
 *
 * GDAL gridder related entry points and definitions.
 */

#include "gdal_alg.h"

CPL_C_START

typedef CPLErr (*GDALGridFunction)(void *, GUInt32,
                                   double *, double *, double *,
                                   double, double, double *);
CPLErr
GDALGridInverseDistanceToAPower( void *, GUInt32,
                                 double *, double *, double *,
                                 double, double, double * );
CPLErr
GDALGridMovingAverage( void *, GUInt32,
                       double *, double *, double *,
                       double, double, double * );
CPLErr
GDALGridNearestNeighbor( void *, GUInt32,
                         double *, double *, double *,
                         double, double, double * );
CPL_C_END

#endif /* GDALGRIDDER_H_INCLUDED */
