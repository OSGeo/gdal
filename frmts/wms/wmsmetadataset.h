/******************************************************************************
 * $Id$
 *
 * Project:  WMS Client Driver
 * Purpose:  Declaration of GDALWMSMetaDataset class
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault, <even dot rouault at mines dash paris dot org>
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

#ifndef _WMS_METADATASET_H_INCLUDED
#define _WMS_METADATASET_H_INCLUDED

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
        double    dfMinX, dfMinY, dfMaxX, dfMaxY;
        int       nResolutions;
        double    dfMinResolution;
        CPLString osFormat;
        CPLString osStyle;
        int       nTileWidth, nTileHeight;
};

/************************************************************************/
/* ==================================================================== */
/*                          GDALWMSMetaDataset                          */
/* ==================================================================== */
/************************************************************************/

class GDALWMSMetaDataset : public GDALPamDataset
{
  private:
    CPLString osGetURL;
    CPLString osVersion;
    CPLString osXMLEncoding;
    char** papszSubDatasets;

    typedef std::pair<CPLString, CPLString> WMSCKeyType;
    std::map<WMSCKeyType, WMSCTileSetDesc> osMapWMSCTileSet;

    void                AddSubDataset(const char* pszName,
                                      const char* pszDesc);

    void                AddSubDataset(const char* pszLayerName,
                                      const char* pszTitle,
                                      const char* pszAbstract,
                                      const char* pszSRS,
                                      const char* pszMinX,
                                      const char* pszMinY,
                                      const char* pszMaxX,
                                      const char* pszMaxY,
                                      CPLString osFormat,
                                      CPLString osTransparent);

    void                ExploreLayer(CPLXMLNode* psXML,
                                     CPLString osFormat,
                                     CPLString osTransparent,
                                     CPLString osPreferredSRS,
                                     const char* pszSRS = NULL,
                                     const char* pszMinX = NULL,
                                     const char* pszMinY = NULL,
                                     const char* pszMaxX = NULL,
                                     const char* pszMaxY = NULL);

    void                AddTiledSubDataset(const char* pszTiledGroupName,
                                           const char* pszTitle);

    void                AnalyzeGetTileServiceRecurse(CPLXMLNode* psXML);

    void                AddWMSCSubDataset(WMSCTileSetDesc& oWMSCTileSetDesc,
                                          const char* pszTitle,
                                          CPLString osTransparent);

    void                ParseWMSCTileSets(CPLXMLNode* psXML);

  public:
        GDALWMSMetaDataset();
       ~GDALWMSMetaDataset();

    virtual char      **GetMetadata( const char * pszDomain = "" );

    static GDALDataset* AnalyzeGetCapabilities(CPLXMLNode* psXML,
                                               CPLString osFormat = "",
                                               CPLString osTransparent = "",
                                               CPLString osPreferredSRS = "");
    static GDALDataset* AnalyzeGetTileService(CPLXMLNode* psXML);
    static GDALDataset* AnalyzeTileMapService(CPLXMLNode* psXML);

    static GDALDataset* DownloadGetCapabilities(GDALOpenInfo *poOpenInfo);
    static GDALDataset* DownloadGetTileService(GDALOpenInfo *poOpenInfo);
};

#endif // _WMS_METADATASET_H_INCLUDED
