/******************************************************************************
 * $Id$
 *
 * Project:  WMS Client Driver
 * Purpose:  Declaration of GDALWMSMetaDataset class
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef WMS_METADATASET_H_INCLUDED
#define WMS_METADATASET_H_INCLUDED

#include "gdal_pam.h"
#include "cpl_string.h"
#include "cpl_http.h"
#include <map>

class WMSCTileSetDesc
{
  public:
    CPLString osLayers;
    CPLString osSRS;
    CPLString osMinX, osMinY, osMaxX, osMaxY;
    double dfMinX, dfMinY, dfMaxX, dfMaxY;
    int nResolutions;
    double dfMinResolution;
    CPLString osFormat;
    CPLString osStyle;
    int nTileWidth, nTileHeight;
};

/************************************************************************/
/* ==================================================================== */
/*                          GDALWMSMetaDataset                          */
/* ==================================================================== */
/************************************************************************/

class GDALWMSMetaDataset final : public GDALPamDataset
{
  private:
    CPLString osGetURL;
    CPLString osVersion;
    CPLString osXMLEncoding;
    char **papszSubDatasets;

    typedef std::pair<CPLString, CPLString> WMSCKeyType;
    std::map<WMSCKeyType, WMSCTileSetDesc> osMapWMSCTileSet;

    void AddSubDataset(const char *pszName, const char *pszDesc);

    void AddSubDataset(const char *pszLayerName, const char *pszTitle,
                       const char *pszAbstract, const char *pszSRS,
                       const char *pszMinX, const char *pszMinY,
                       const char *pszMaxX, const char *pszMaxY,
                       const std::string &osFormat,
                       const std::string &osTransparent);

    void
    ExploreLayer(CPLXMLNode *psXML, const CPLString &osFormat,
                 const CPLString &osTransparent,
                 const CPLString &osPreferredSRS, const char *pszSRS = nullptr,
                 const char *pszMinX = nullptr, const char *pszMinY = nullptr,
                 const char *pszMaxX = nullptr, const char *pszMaxY = nullptr);

    // tiledWMS only
    void AddTiledSubDataset(const char *pszTiledGroupName, const char *pszTitle,
                            const char *const *papszChanges);

    // tiledWMS only
    void AnalyzeGetTileServiceRecurse(CPLXMLNode *psXML,
                                      GDALOpenInfo *poOpenInfo);

    // WMS-C only
    void AddWMSCSubDataset(WMSCTileSetDesc &oWMSCTileSetDesc,
                           const char *pszTitle,
                           const CPLString &osTransparent);

    // WMS-C only
    void ParseWMSCTileSets(CPLXMLNode *psXML);

  public:
    GDALWMSMetaDataset();
    virtual ~GDALWMSMetaDataset();

    virtual char **GetMetadataDomainList() override;
    virtual char **GetMetadata(const char *pszDomain = "") override;

    static GDALDataset *
    AnalyzeGetCapabilities(CPLXMLNode *psXML,
                           const std::string &osFormat = std::string(),
                           const std::string &osTransparent = std::string(),
                           const std::string &osPreferredSRS = std::string());
    static GDALDataset *AnalyzeTileMapService(CPLXMLNode *psXML);

    static GDALDataset *DownloadGetCapabilities(GDALOpenInfo *poOpenInfo);

    // tiledWMS only
    static GDALDataset *DownloadGetTileService(GDALOpenInfo *poOpenInfo);
    // tiledWMS only
    static GDALDataset *AnalyzeGetTileService(CPLXMLNode *psXML,
                                              GDALOpenInfo *poOpenInfo);
};

#endif  // WMS_METADATASET_H_INCLUDED
