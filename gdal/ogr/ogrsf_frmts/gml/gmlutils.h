/******************************************************************************
 * $Id$
 *
 * Project:  GML Utils
 * Purpose:  GML reader
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef CPL_GMLUTILS_H_INCLUDED
#define CPL_GMLUTILS_H_INCLUDED

#include <vector>
#include <string>
#include "cpl_minixml.h"

#include "ogr_geometry.h"

typedef enum
{
    GML_SWAP_AUTO,
    GML_SWAP_YES,
    GML_SWAP_NO,
} GMLSwapCoordinatesEnum;

typedef enum
{
    SRSNAME_SHORT,
    SRSNAME_OGC_URN,
    SRSNAME_OGC_URL
} OGRGMLSRSNameFormat;

const char* GML_ExtractSrsNameFromGeometry(const CPLXMLNode* const * papsGeometry,
                                     std::string& osWork,
                                     bool bConsiderEPSGAsURN);

bool GML_IsSRSLatLongOrder(const char* pszSRSName);
bool GML_IsLegitSRSName(const char* pszSRSName);

void* GML_BuildOGRGeometryFromList_CreateCache();
void GML_BuildOGRGeometryFromList_DestroyCache(void* hCacheSRS);

OGRGeometry* GML_BuildOGRGeometryFromList(const CPLXMLNode* const * papsGeometry,
                                          bool bTryToMakeMultipolygons,
                                          bool bInvertAxisOrderIfLatLong,
                                          const char* pszDefaultSRSName,
                                          bool bConsiderEPSGAsURN,
                                          GMLSwapCoordinatesEnum eSwapCoordinates,
                                          int nPseudoBoolGetSecondaryGeometryOption,
                                          void* hCacheSRS,
                                          bool bFaceHoleNegative = false );

char* GML_GetSRSName(const OGRSpatialReference* poSRS, OGRGMLSRSNameFormat eSRSNameFormat, bool *pbCoordSwap);

#endif /* _CPL_GMLREADERP_H_INCLUDED */
