/******************************************************************************
 * $Id$
 *
 * Project:  GML Utils
 * Purpose:  GML reader
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef _CPL_GMLUTILS_H_INCLUDED
#define _CPL_GMLUTILS_H_INCLUDED

#include <vector>
#include <string>
#include "cpl_minixml.h"

#include "ogr_geometry.h"

const char* GML_ExtractSrsNameFromGeometry(const CPLXMLNode* const * papsGeometry,
                                     std::string& osWork,
                                     int bConsiderEPSGAsURN);

int GML_IsSRSLatLongOrder(const char* pszSRSName);

void* GML_BuildOGRGeometryFromList_CreateCache();
void GML_BuildOGRGeometryFromList_DestroyCache(void* hCacheSRS);

OGRGeometry* GML_BuildOGRGeometryFromList(const CPLXMLNode* const * papsGeometry,
                                          int bTryToMakeMultipolygons,
                                          int bInvertAxisOrderIfLatLong,
                                          const char* pszDefaultSRSName,
                                          int bConsiderEPSGAsURN,
                                          int bGetSecondaryGeometryOption,
                                          void* hCacheSRS,
                                          int bFaceHoleNegative = FALSE );

char* GML_GetSRSName(const OGRSpatialReference* poSRS, int bLongSRS, int *pbCoordSwap);

#endif /* _CPL_GMLREADERP_H_INCLUDED */
