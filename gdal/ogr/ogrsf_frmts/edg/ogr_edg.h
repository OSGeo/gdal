/******************************************************************************
 * $Id$
 *
 * Project:  Anatrack Ranges Edge File Translator
 * Purpose:  Definition of classes for OGR Anatrack Ranges edge file driver.
 * Author:   Nick Casey, nick@anatrack.com
 *
 ******************************************************************************
 * Copyright (c) 2020, Nick Casey <nick at anatrack.com>
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

#ifndef OGREDG_H_INCLUDED
#define OGREDG_H_INCLUDED

#include "ogrsf_frmts.h"

#include <map>
#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <sstream>

 /************************************************************************/
 /*                             EdgAppendix                              */
 /* Class for managing the Ranges Edge file appendix                     */
 /************************************************************************/

class EdgAppendix
{
    bool                                    bAppendixLoaded;
    std::vector<std::string>                vsIds;
    std::vector<std::string>                vsCores;
    std::vector<std::string>                vsAgeLabels;
    std::vector<std::string>                vsSexLabels;
    std::vector<std::string>                vsMetaDataStrings;

    std::string                             sReferenceEllipsoid;
    int                                     iLongitudeZone;
    char                                    cLatitudeZone;

    OGREnvelope                             oEnvelope;
    
public:
    EdgAppendix();
    ~EdgAppendix();
    
    bool                                    ReadAppendix(VSILFILE* fp);
    const std::vector<std::string>&         GetIds() const { return vsIds; }
    const std::vector<std::string>&         GetCores() const { return vsCores; }
    const std::vector<int>                  GetOrderedCores();
    const std::vector<std::string>&         GetAgeLabels() const { return vsAgeLabels; }
    const std::vector<std::string>&         GetSexLabels() const { return vsSexLabels; }
    const std::vector<std::string>&         GetMetaDataStrings() const { return vsMetaDataStrings; }

    void                                    AddId(std::string psId) { vsIds.push_back(psId); }
    void                                    AddCore(std::string piCore) { vsCores.push_back(piCore); }
    void                                    AddAgeLabel(std::string psAgeLabel) { vsAgeLabels.push_back(psAgeLabel); }
    void                                    AddSexLabel(std::string psSexLabel) { vsSexLabels.push_back(psSexLabel); }
    void                                    UpdateMetaDataString(std::string sMetaDataString, int piPosition);
    int                                     GetHemisphereFromUTMLatitudeZone();
    std::string                             GetAppendixString();
    
    OGRSpatialReference*                    GetSpatialReference();
    void                                    GrowExtents(OGREnvelope *psGeomBounds);
    void                                    setUTMZone(char pcLatitudeZone, int piLongitudeZone) { cLatitudeZone = pcLatitudeZone; iLongitudeZone = piLongitudeZone; };

};

/************************************************************************/
/*                             OGREdgLayer                              */
/************************************************************************/

class OGREdgLayer : public OGRLayer
{
    OGRFeatureDefn                          *poFeatureDefn = nullptr;
    CPLString                               psFilename;
    VSILFILE                                *fp;
    int                                     nNextFID;
    bool                                    bWriter;
    OGRSpatialReference                     *poSRSIn;
    OGRCoordinateTransformation             *poCT;
    bool                                    bCTSet;
    EdgAppendix*                            poEdgAppendix;

    std::map<std::pair<int, int>, std::vector<std::string>>  geometryMap;

    void                                    SetupFeatureDefinition();
    void                                    InitialiseReading();
    bool                                    CollectGeometry(OGRGeometry *poGeometry, std::vector<std::string> &pvsGeometryPolygons);
    static void                             CollectGeometryLine(const OGRLineString *poLine, bool bIsHole, std::vector<std::string> &pvsGeometryPolygons);
    static int                              GetRangeParameterFromField(CPLString pszRaw, std::vector<std::string> lookupVector, std::function<void(std::string)> updateFunction);
    void                                    WriteEdgFile();
    static void                             GetUTMZone(double pdLat, double pdLon, char *pcLatZone, int *piLonZone, int *pbNorth);
    void                                    CreateCoordinateTransform(OGREnvelope sSourceGeomBounds);

public:
    OGREdgLayer(const char * pszFilenameP,
                    OGRSpatialReference *poSRSInP,
                    bool bWriterInP);
    ~OGREdgLayer();

    void                                    ResetReading();
    OGRFeature *                            GetNextFeature();
    OGRFeatureDefn *                        GetLayerDefn() { return poFeatureDefn; }
    int                                     TestCapability(const char *) { return FALSE; }
    OGRErr                                  ICreateFeature(OGRFeature* poFeature) override;
};

/************************************************************************/
/*                           OGREdgDataSource                           */
/************************************************************************/

class OGREdgDataSource final : public OGRDataSource
{
    char                                    *pszName;
    OGREdgLayer                             *poLayer;
    int                                     nLayers;
    CPLString                               psDestinationFilename;
    bool                                    bUpdate;

public:
    OGREdgDataSource();
    virtual ~OGREdgDataSource();

    int                                     Open(const char *pszFilename);
    int                                     Create(const char *pszFilename, char **papszOptions);

    const char                              *GetName() override { return pszName; }
    int                                     GetLayerCount() override { return nLayers; }
    OGRLayer                                *GetLayer(int) override;

    virtual OGRLayer                        *ICreateLayer(const char *,
                                                    OGRSpatialReference * = nullptr,
                                                    OGRwkbGeometryType = wkbUnknown,
                                                    char ** = nullptr) override;
    int                                     TestCapability(const char *) override;
    
};

#endif  // ndef OGREDG_H_INCLUDED
