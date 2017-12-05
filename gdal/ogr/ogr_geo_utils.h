/******************************************************************************
 * $Id$
 *
 * Project:  X-Plane aeronautical data reader
 * Purpose:  Definition of geo-computation functions
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2008, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef OGR_GEO_UTILS_H_INCLUDED
#define OGR_GEO_UTILS_H_INCLUDED

/** The following functions provide computations based on great-circle/
 * orthodromic path, on a sphere with a radius of ~6366707 m.
 * The computations are not necessarily implemented in a very accurate/
 * stable way, and shouldn't be used for points that are too close (less than
 * one meter).
 * They are good enough for example to compute the coordinates of the polygon
 * for an airport runway, from its extreme points, track and length.
 */

double OGR_GreatCircle_Distance(double dfLatA_deg, double dfLonA_deg,
                                double dfLatB_deg, double dfLonB_deg);

double OGR_GreatCircle_InitialHeading(double dfLatA_deg, double dfLonA_deg,
                                      double dfLatB_deg, double dfLonB_deg);

/* such as ExtendPosition(A, Distance(A,B), InitialHeading(A,B)) ~= B */
int OGR_GreatCircle_ExtendPosition(double dfLatA_deg, double dfLonA_deg,
                                   double dfDistance, double dfHeadingInA,
                                   double* pdfLatB_deg, double* pdfLonB_deg);

#endif /* ndef OGR_GEO_UTILS_H_INCLUDED */
