/******************************************************************************
 * $Id$
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Implements special parsing of Imagine citation strings, and
 *           to encode PE String info in citation fields as needed.
 * Author:   Xiuguang Zhou (ESRI)
 *
 ******************************************************************************
 * Copyright (c) 2008, Xiuguang Zhou (ESRI)
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef GT_CITATION_H_INCLUDED
#define GT_CITATION_H_INCLUDED

#include "cpl_port.h"
#include "geo_normalize.h"
#include "ogr_spatialref.h"

char* ImagineCitationTranslation(char* psCitation, geokey_t keyID);
char** CitationStringParse(char* psCitation, geokey_t keyID);

#define nCitationNameTypes 9
typedef enum 
{
  CitCsName = 0,
  CitPcsName = 1,
  CitProjectionName = 2,
  CitLUnitsName = 3,
  CitGcsName = 4,
  CitDatumName = 5,
  CitEllipsoidName = 6,
  CitPrimemName = 7,
  CitAUnitsName = 8
} CitationNameType;

OGRBoolean CheckCitationKeyForStatePlaneUTM(GTIF* hGTIF, GTIFDefn* psDefn, OGRSpatialReference* poSRS, OGRBoolean* pLinearUnitIsSet);
//char* ImagineCitationTranslation(char* psCitation, geokey_t keyID);
//char** CitationStringParse(char* psCitation, geokey_t keyID);
void SetLinearUnitCitation(GTIF* psGTIF, char* pszLinearUOMName);
void SetGeogCSCitation(GTIF * psGTIF, OGRSpatialReference *poSRS, char* angUnitName, int nDatum, short nSpheroid);
OGRBoolean SetCitationToSRS(GTIF* hGTIF, char* szCTString, int nCTStringLen,
                            geokey_t geoKey, OGRSpatialReference* poSRS, OGRBoolean* linearUnitIsSet);
void GetGeogCSFromCitation(char* szGCSName, int nGCSName,
                           geokey_t geoKey, 
                          char	**ppszGeogName,
                          char	**ppszDatumName,
                          char	**ppszPMName,
                          char	**ppszSpheroidName,
                          char	**ppszAngularUnits);
void CheckUTM( GTIFDefn * psDefn, const char * pszCtString );


#endif // GT_CITATION_H_INCLUDED
