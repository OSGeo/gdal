/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Implements special parsing of Imagine citation strings, and
 *           to encode PE String info in citation fields as needed.
 * Author:   Xiuguang Zhou (ESRI)
 *
 ******************************************************************************
 * Copyright (c) 2008, Xiuguang Zhou (ESRI)
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GT_CITATION_H_INCLUDED
#define GT_CITATION_H_INCLUDED

#include "cpl_port.h"
#include "geo_normalize.h"
#include "ogr_spatialref.h"

#include <string>
#include <map>

char *ImagineCitationTranslation(char *psCitation, geokey_t keyID);
char **CitationStringParse(char *psCitation, geokey_t keyID);

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

OGRBoolean CheckCitationKeyForStatePlaneUTM(GTIF *hGTIF, GTIFDefn *psDefn,
                                            OGRSpatialReference *poSRS,
                                            OGRBoolean *pLinearUnitIsSet);

void SetLinearUnitCitation(std::map<geokey_t, std::string> &oMapAsciiKeys,
                           const char *pszLinearUOMName);
void SetGeogCSCitation(GTIF *psGTIF,
                       std::map<geokey_t, std::string> &oMapAsciiKeys,
                       const OGRSpatialReference *poSRS,
                       const char *angUnitName, int nDatum, short nSpheroid);
OGRBoolean SetCitationToSRS(GTIF *hGTIF, char *szCTString, int nCTStringLen,
                            geokey_t geoKey, OGRSpatialReference *poSRS,
                            OGRBoolean *linearUnitIsSet);
void GetGeogCSFromCitation(char *szGCSName, int nGCSName, geokey_t geoKey,
                           char **ppszGeogName, char **ppszDatumName,
                           char **ppszPMName, char **ppszSpheroidName,
                           char **ppszAngularUnits);
void CheckUTM(GTIFDefn *psDefn, const char *pszCtString);

#endif  // GT_CITATION_H_INCLUDED
