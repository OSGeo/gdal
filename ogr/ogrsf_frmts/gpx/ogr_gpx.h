/******************************************************************************
 * $Id$
 *
 * Project:  GPX Translator
 * Purpose:  Definition of classes for OGR .gpx driver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2007-2010, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGR_GPX_H_INCLUDED
#define OGR_GPX_H_INCLUDED

#include "cpl_vsi_virtual.h"
#include "ogrsf_frmts.h"

#ifdef HAVE_EXPAT
#include "ogr_expat.h"
#endif

#include <limits>
#include <deque>

class OGRGPXDataSource;

typedef enum
{
    GPX_NONE,
    GPX_WPT,
    GPX_TRACK,
    GPX_ROUTE,
    GPX_ROUTE_POINT,
    GPX_TRACK_POINT,
} GPXGeometryType;

constexpr int PARSER_BUF_SIZE = 8192;

/************************************************************************/
/*                             OGRGPXLayer                              */
/************************************************************************/

class OGRGPXLayer final : public OGRLayer
{
    OGRFeatureDefn *m_poFeatureDefn = nullptr;
    OGRSpatialReference *m_poSRS = nullptr;
    OGRGPXDataSource *m_poDS = nullptr;

    GPXGeometryType m_gpxGeomType = GPX_NONE;

    int m_nGPXFields = 0;

    bool m_bWriteMode = false;
    GIntBig m_nNextFID = 0;
    VSIVirtualHandleUniquePtr m_fpGPX{};
#ifdef HAVE_EXPAT
    XML_Parser m_oParser = nullptr;
    XML_Parser m_oSchemaParser = nullptr;
#endif
    bool m_inInterestingElement = false;
    bool m_hasFoundLat = false;
    bool m_hasFoundLon = false;
#ifdef HAVE_EXPAT
    double m_latVal = 0;
    double m_lonVal = 0;
#endif
    std::string m_osSubElementName{};
    std::string m_osSubElementValue{};
#ifdef HAVE_EXPAT
    int m_iCurrentField = 0;
#endif

    std::unique_ptr<OGRFeature> m_poFeature{};
    std::deque<std::unique_ptr<OGRFeature>> m_oFeatureQueue{};

    std::unique_ptr<OGRMultiLineString> m_multiLineString{};
    std::unique_ptr<OGRLineString> m_lineString{};

    int m_depthLevel = 0;
    int m_interestingDepthLevel = 0;

#ifdef HAVE_EXPAT
    OGRFieldDefn *m_currentFieldDefn = nullptr;
    bool m_inExtensions = false;
    int m_extensionsDepthLevel = 0;

    bool m_inLink = 0;
    int m_iCountLink = 0;
#endif
    int m_nMaxLinks = 0;

    bool m_bEleAs25D = false;

    int m_trkFID = 0;
    int m_trkSegId = 0;
    int m_trkSegPtId = 0;

    int m_rteFID = 0;
    int m_rtePtId = 0;

#ifdef HAVE_EXPAT
    bool m_bStopParsing = false;
    int m_nWithoutEventCounter = 0;
    int m_nDataHandlerCounter = 0;
#endif

    int m_iFirstGPXField = 0;

    CPL_DISALLOW_COPY_ASSIGN(OGRGPXLayer)

  private:
    void WriteFeatureAttributes(const OGRFeature *poFeature,
                                int nIdentLevel = 1);
    void LoadExtensionsSchema();
#ifdef HAVE_EXPAT
    void AddStrToSubElementValue(const char *pszStr);
#endif
    bool OGRGPX_WriteXMLExtension(const char *pszTagName,
                                  const char *pszContent);

  public:
    OGRGPXLayer(const char *pszFilename, const char *layerName,
                GPXGeometryType gpxGeomType, OGRGPXDataSource *poDS,
                bool bWriteMode, CSLConstList papszOpenOptions);
    ~OGRGPXLayer();

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;

    OGRErr ICreateFeature(OGRFeature *poFeature) override;
    OGRErr CreateField(const OGRFieldDefn *poField, int bApproxOK) override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poFeatureDefn;
    }

    int TestCapability(const char *) override;

    GDALDataset *GetDataset() override;

#ifdef HAVE_EXPAT
    void startElementCbk(const char *pszName, const char **ppszAttr);
    void endElementCbk(const char *pszName);
    void dataHandlerCbk(const char *data, int nLen);

    void startElementLoadSchemaCbk(const char *pszName, const char **ppszAttr);
    void endElementLoadSchemaCbk(const char *pszName);
    void dataHandlerLoadSchemaCbk(const char *data, int nLen);
#endif

    static OGRErr CheckAndFixCoordinatesValidity(double *pdfLatitude,
                                                 double *pdfLongitude);
};

/************************************************************************/
/*                           OGRGPXDataSource                           */
/************************************************************************/

typedef enum
{
    GPX_VALIDITY_UNKNOWN,
    GPX_VALIDITY_INVALID,
    GPX_VALIDITY_VALID
} OGRGPXValidity;

class OGRGPXDataSource final : public GDALDataset
{
    std::vector<std::unique_ptr<OGRGPXLayer>> m_apoLayers{};

    /*  Export related */
    VSIVirtualHandleUniquePtr m_fpOutput{};
    bool m_bIsBackSeekable = true;
    const char *m_pszEOL = "\n";
    vsi_l_offset m_nOffsetBounds = 0;
    double m_dfMinLat = std::numeric_limits<double>::infinity();
    double m_dfMinLon = std::numeric_limits<double>::infinity();
    double m_dfMaxLat = -std::numeric_limits<double>::infinity();
    double m_dfMaxLon = -std::numeric_limits<double>::infinity();

    GPXGeometryType m_lastGPXGeomTypeWritten = GPX_NONE;

    bool m_bUseExtensions = false;
    std::string m_osExtensionsNS{};

#ifdef HAVE_EXPAT
    OGRGPXValidity m_validity = GPX_VALIDITY_UNKNOWN;
    std::string m_osVersion{};
    XML_Parser m_oCurrentParser = nullptr;
    int m_nDataHandlerCounter = 0;
    bool m_bInMetadata = false;
    bool m_bInMetadataAuthor = false;
    bool m_bInMetadataAuthorLink = false;
    bool m_bInMetadataCopyright = false;
    bool m_bInMetadataLink = false;
    int m_nMetadataLinkCounter = 0;
    int m_nDepth = 0;
    std::string m_osMetadataKey{};
    std::string m_osMetadataValue{};
#endif

    CPL_DISALLOW_COPY_ASSIGN(OGRGPXDataSource)

  public:
    OGRGPXDataSource() = default;
    ~OGRGPXDataSource();

    int m_nLastRteId = -1;
    int m_nLastTrkId = -1;
    int m_nLastTrkSegId = -1;

    int Open(GDALOpenInfo *poOpenInfo);

    int Create(const char *pszFilename, char **papszOptions);

    int GetLayerCount() override
    {
        return static_cast<int>(m_apoLayers.size());
    }

    OGRLayer *GetLayer(int) override;

    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;

    int TestCapability(const char *) override;

    VSILFILE *GetOutputFP()
    {
        return m_fpOutput.get();
    }

    void SetLastGPXGeomTypeWritten(GPXGeometryType gpxGeomType)
    {
        m_lastGPXGeomTypeWritten = gpxGeomType;
    }

    GPXGeometryType GetLastGPXGeomTypeWritten() const
    {
        return m_lastGPXGeomTypeWritten;
    }

    bool GetUseExtensions() const
    {
        return m_bUseExtensions;
    }

    const std::string &GetExtensionsNS() const
    {
        return m_osExtensionsNS;
    }

#ifdef HAVE_EXPAT
    void startElementValidateCbk(const char *pszName, const char **ppszAttr);
    void endElementValidateCbk(const char *pszName);
    void dataHandlerValidateCbk(const char *data, int nLen);

    const char *GetVersion() const
    {
        return m_osVersion.c_str();
    }
#endif

    void AddCoord(double dfLon, double dfLat);

    void PrintLine(const char *fmt, ...) CPL_PRINT_FUNC_FORMAT(2, 3);
};

#endif /* ndef OGR_GPX_H_INCLUDED */
