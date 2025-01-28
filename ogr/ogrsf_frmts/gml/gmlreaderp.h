/******************************************************************************
 *
 * Project:  GML Reader
 * Purpose:  Private Declarations for OGR free GML Reader code.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_GMLREADERP_H_INCLUDED
#define CPL_GMLREADERP_H_INCLUDED

#if defined(HAVE_XERCES)

#include "xercesc_headers.h"
#include "ogr_xerces.h"

#endif /* HAVE_XERCES */

#include "cpl_string.h"
#include "gmlreader.h"
#include "ogr_api.h"
#include "cpl_vsi.h"
#include "cpl_multiproc.h"
#include "gmlutils.h"

#include <map>
#include <string>
#include <vector>

#define PARSER_BUF_SIZE (10 * 8192)

class GMLReader;

typedef struct _GeometryNamesStruct GeometryNamesStruct;

bool OGRGMLIsGeometryElement(const char *pszElement);

/************************************************************************/
/*                        GFSTemplateList                               */
/************************************************************************/

class GFSTemplateItem;

class GFSTemplateList
{
  private:
    bool m_bSequentialLayers;
    GFSTemplateItem *pFirst;
    GFSTemplateItem *pLast;
    GFSTemplateItem *Insert(const char *pszName);

    CPL_DISALLOW_COPY_ASSIGN(GFSTemplateList)

  public:
    GFSTemplateList();
    ~GFSTemplateList();
    void Update(const char *pszName, int bHasGeom);

    GFSTemplateItem *GetFirst()
    {
        return pFirst;
    }

    bool HaveSequentialLayers()
    {
        return m_bSequentialLayers;
    }

    int GetClassCount();
};

void gmlUpdateFeatureClasses(GFSTemplateList *pCC, GMLReader *pReader,
                             int *pnHasSequentialLayers);

/************************************************************************/
/*                              GMLHandler                              */
/************************************************************************/

#define STACK_SIZE 5

typedef enum
{
    STATE_TOP,
    STATE_DEFAULT,
    STATE_FEATURE,
    STATE_PROPERTY,
    STATE_FEATUREPROPERTY,
    STATE_GEOMETRY,
    STATE_IGNORED_FEATURE,
    STATE_BOUNDED_BY,
    STATE_BOUNDED_BY_IN_FEATURE,
    STATE_CITYGML_ATTRIBUTE
} HandlerState;

typedef struct
{
    CPLXMLNode *psNode;
    CPLXMLNode *psLastChild;
} NodeLastChild;

typedef enum
{
    APPSCHEMA_GENERIC,
    APPSCHEMA_CITYGML,
    APPSCHEMA_AIXM,
    APPSCHEMA_MTKGML /* format of National Land Survey Finnish */
} GMLAppSchemaType;

class GMLHandler
{
    char *m_pszCurField = nullptr;
    unsigned int m_nCurFieldAlloc = 0;
    unsigned int m_nCurFieldLen = 0;
    bool m_bInCurField = false;
    int m_nAttributeIndex = -1;
    int m_nAttributeDepth = 0;

    char *m_pszGeometry = nullptr;
    unsigned int m_nGeomAlloc = 0;
    unsigned int m_nGeomLen = 0;
    int m_nGeometryDepth = 0;
    bool m_bAlreadyFoundGeometry = false;
    int m_nGeometryPropertyIndex = 0;
    std::map<std::string, CPLXMLNode *> m_oMapElementToSubstitute{};

    int m_nDepth = 0;
    int m_nDepthFeature = 0;
    int m_nUnlimitedDepth = -1;  // -1 unknown, 0=false, 1=true

    int m_inBoundedByDepth = 0;

    char *m_pszCityGMLGenericAttrName = nullptr;
    int m_inCityGMLGenericAttrDepth = 0;

    bool m_bReportHref = false;
    char *m_pszHref = nullptr;
    char *m_pszUom = nullptr;
    char *m_pszValue = nullptr;
    char *m_pszKieli = nullptr;

    GeometryNamesStruct *pasGeometryNames = nullptr;

    std::vector<NodeLastChild> apsXMLNode{};

    int m_nSRSDimensionIfMissing = 0;

    OGRErr startElementTop(const char *pszName, int nLenName, void *attr);

    OGRErr endElementIgnoredFeature();

    OGRErr startElementBoundedBy(const char *pszName, int nLenName, void *attr);
    OGRErr endElementBoundedBy();

    OGRErr endElementBoundedByInFeature();

    OGRErr startElementFeatureAttribute(const char *pszName, int nLenName,
                                        void *attr);
    OGRErr endElementFeature();

    OGRErr startElementCityGMLGenericAttr(const char *pszName, int nLenName,
                                          void *attr);
    OGRErr endElementCityGMLGenericAttr();

    OGRErr startElementGeometry(const char *pszName, int nLenName, void *attr);
    void ParseAIXMElevationProperties(const CPLXMLNode *);
    CPLXMLNode *ParseAIXMElevationPoint(CPLXMLNode *);
    OGRErr endElementGeometry();
    OGRErr dataHandlerGeometry(const char *data, int nLen);

    OGRErr endElementAttribute();
    OGRErr dataHandlerAttribute(const char *data, int nLen);

    OGRErr startElementDefault(const char *pszName, int nLenName, void *attr);
    OGRErr endElementDefault();

    OGRErr startElementFeatureProperty(const char *pszName, int nLenName,
                                       void *attr);
    OGRErr endElementFeatureProperty();

    void DealWithAttributes(const char *pszName, int nLenName, void *attr);
    bool IsConditionMatched(const char *pszCondition, void *attr);
    int FindRealPropertyByCheckingConditions(int nIdx, void *attr);

    CPL_DISALLOW_COPY_ASSIGN(GMLHandler)

  protected:
    GMLReader *m_poReader = nullptr;
    GMLAppSchemaType eAppSchemaType = APPSCHEMA_GENERIC;

    int nStackDepth = 0;
    HandlerState stateStack[STACK_SIZE];

    CPLString m_osFID{};
    virtual const char *GetFID(void *attr) = 0;

    virtual CPLXMLNode *AddAttributes(CPLXMLNode *psNode, void *attr) = 0;

    OGRErr startElement(const char *pszName, int nLenName, void *attr);
    OGRErr endElement();
    OGRErr dataHandler(const char *data, int nLen);

    bool IsGeometryElement(const char *pszElement);

  public:
    explicit GMLHandler(GMLReader *poReader);
    virtual ~GMLHandler();

    virtual char *GetAttributeValue(void *attr,
                                    const char *pszAttributeName) = 0;
    virtual char *GetAttributeByIdx(void *attr, unsigned int idx,
                                    char **ppszKey) = 0;

    GMLAppSchemaType GetAppSchemaType() const
    {
        return eAppSchemaType;
    }
};

#if defined(HAVE_XERCES)

/************************************************************************/
/*                         GMLXercesHandler                             */
/************************************************************************/
class GMLXercesHandler final : public DefaultHandler, public GMLHandler
{
    int m_nEntityCounter = 0;
    CPLString m_osElement{};
    CPLString m_osCharacters{};
    CPLString m_osAttrName{};
    CPLString m_osAttrValue{};

  public:
    explicit GMLXercesHandler(GMLReader *poReader);

    void startElement(const XMLCh *const uri, const XMLCh *const localname,
                      const XMLCh *const qname,
                      const Attributes &attrs) override;
    void endElement(const XMLCh *const uri, const XMLCh *const localname,
                    const XMLCh *const qname) override;
    void characters(const XMLCh *const chars, const XMLSize_t length) override;

    void fatalError(const SAXParseException &) override;

    void startEntity(const XMLCh *const name) override;

    virtual const char *GetFID(void *attr) override;
    virtual CPLXMLNode *AddAttributes(CPLXMLNode *psNode, void *attr) override;
    virtual char *GetAttributeValue(void *attr,
                                    const char *pszAttributeName) override;
    virtual char *GetAttributeByIdx(void *attr, unsigned int idx,
                                    char **ppszKey) override;
};

#endif

#if defined(HAVE_EXPAT)

#include "ogr_expat.h"

/************************************************************************/
/*                           GMLExpatHandler                            */
/************************************************************************/
class GMLExpatHandler final : public GMLHandler
{
    XML_Parser m_oParser = nullptr;
    bool m_bStopParsing = false;
    int m_nDataHandlerCounter = 0;

    void DealWithError(OGRErr eErr);

    CPL_DISALLOW_COPY_ASSIGN(GMLExpatHandler)

  public:
    GMLExpatHandler(GMLReader *poReader, XML_Parser oParser);

    bool HasStoppedParsing()
    {
        return m_bStopParsing;
    }

    void ResetDataHandlerCounter()
    {
        m_nDataHandlerCounter = 0;
    }

    virtual const char *GetFID(void *attr) override;
    virtual CPLXMLNode *AddAttributes(CPLXMLNode *psNode, void *attr) override;
    virtual char *GetAttributeValue(void *attr,
                                    const char *pszAttributeName) override;
    virtual char *GetAttributeByIdx(void *attr, unsigned int idx,
                                    char **ppszKey) override;

    static void XMLCALL startElementCbk(void *pUserData, const char *pszName,
                                        const char **ppszAttr);

    static void XMLCALL endElementCbk(void *pUserData, const char *pszName);

    static void XMLCALL dataHandlerCbk(void *pUserData, const char *data,
                                       int nLen);
};

#endif

/************************************************************************/
/*                             GMLReadState                             */
/************************************************************************/

class GMLReadState
{
    std::vector<std::string> aosPathComponents{};

    CPL_DISALLOW_COPY_ASSIGN(GMLReadState)

  public:
    GMLReadState() = default;
    ~GMLReadState() = default;

    void PushPath(const char *pszElement, int nLen = -1);
    void PopPath();

    const char *GetLastComponent() const
    {
        return (m_nPathLength == 0)
                   ? ""
                   : aosPathComponents[m_nPathLength - 1].c_str();
    }

    size_t GetLastComponentLen() const
    {
        return (m_nPathLength == 0)
                   ? 0
                   : aosPathComponents[m_nPathLength - 1].size();
    }

    void Reset();

    GMLFeature *m_poFeature = nullptr;
    GMLReadState *m_poParentState = nullptr;

    std::string osPath{};  // element path ... | as separator.
    int m_nPathLength = 0;
};

/************************************************************************/
/*                              GMLReader                               */
/************************************************************************/

class GMLReader final : public IGMLReader
{
  private:
    bool m_bClassListLocked = false;

    int m_nClassCount = 0;
    GMLFeatureClass **m_papoClass = nullptr;
    bool m_bLookForClassAtAnyLevel = false;

    char *m_pszFilename = nullptr;

#ifndef HAVE_XERCES
    bool bUseExpatReader = true;
#else
    bool bUseExpatReader = false;
#endif

    GMLHandler *m_poGMLHandler = nullptr;

#if defined(HAVE_XERCES)
    SAX2XMLReader *m_poSAXReader = nullptr;
    XMLPScanToken m_oToFill{};
    GMLFeature *m_poCompleteFeature = nullptr;
    InputSource *m_GMLInputSource = nullptr;
    bool m_bEOF = false;
    bool m_bXercesInitialized = false;
    bool SetupParserXerces();
    GMLFeature *NextFeatureXerces();
#endif

#if defined(HAVE_EXPAT)
    XML_Parser oParser = nullptr;
    GMLFeature **ppoFeatureTab = nullptr;
    int nFeatureTabLength = 0;
    int nFeatureTabIndex = 0;
    int nFeatureTabAlloc = 0;
    bool SetupParserExpat();
    GMLFeature *NextFeatureExpat();
    char *pabyBuf = nullptr;
    CPLString m_osErrorMessage{};
#endif

    VSILFILE *fpGML = nullptr;
    bool m_bReadStarted = false;

    GMLReadState *m_poState = nullptr;
    GMLReadState *m_poRecycledState = nullptr;

    bool m_bStopParsing = false;

    bool SetupParser();
    void CleanupParser();

    bool m_bFetchAllGeometries = false;

    bool m_bInvertAxisOrderIfLatLong = false;
    bool m_bConsiderEPSGAsURN = false;
    GMLSwapCoordinatesEnum m_eSwapCoordinates = GML_SWAP_AUTO;
    bool m_bGetSecondaryGeometryOption = false;

    int ParseFeatureType(CPLXMLNode *psSchemaNode, const char *pszName,
                         const char *pszType);

    char *m_pszGlobalSRSName = nullptr;
    bool m_bCanUseGlobalSRSName = false;

    char *m_pszFilteredClassName = nullptr;
    int m_nFilteredClassIndex = -1;

    int m_nHasSequentialLayers = -1;

    std::string osElemPath{};

    bool m_bFaceHoleNegative = false;

    bool m_bSetWidthFlag = true;

    bool m_bReportAllAttributes = false;

    bool m_bIsWFSJointLayer = false;

    bool m_bEmptyAsNull = true;

    bool m_bUseBBOX = false;

    bool ParseXMLHugeFile(const char *pszOutputFilename,
                          const bool bSqliteIsTempFile,
                          const int iSqliteCacheMB);

    CPL_DISALLOW_COPY_ASSIGN(GMLReader)

  public:
    GMLReader(bool bExpatReader, bool bInvertAxisOrderIfLatLong,
              bool bConsiderEPSGAsURN, GMLSwapCoordinatesEnum eSwapCoordinates,
              bool bGetSecondaryGeometryOption);
    virtual ~GMLReader();

    bool IsClassListLocked() const override
    {
        return m_bClassListLocked;
    }

    void SetClassListLocked(bool bFlag) override
    {
        m_bClassListLocked = bFlag;
    }

    void SetSourceFile(const char *pszFilename) override;
    void SetFP(VSILFILE *fp) override;
    const char *GetSourceFileName() override;

    int GetClassCount() const override
    {
        return m_nClassCount;
    }

    GMLFeatureClass *GetClass(int i) const override;
    GMLFeatureClass *GetClass(const char *pszName) const override;

    int AddClass(GMLFeatureClass *poClass) override;
    void ClearClasses() override;

    GMLFeature *NextFeature() override;

    bool LoadClasses(const char *pszFile = nullptr) override;
    bool SaveClasses(const char *pszFile = nullptr) override;

    bool ResolveXlinks(const char *pszFile, bool *pbOutIsTempFile,
                       char **papszSkip = nullptr,
                       const bool bStrict = false) override;

    bool HugeFileResolver(const char *pszFile, bool bSqliteIsTempFile,
                          int iSqliteCacheMB) override;

    bool PrescanForSchema(bool bGetExtents = true,
                          bool bOnlyDetectSRS = false) override;
    bool PrescanForTemplate() override;
    bool ReArrangeTemplateClasses(GFSTemplateList *pCC);
    void ResetReading() override;

    // ---

    GMLReadState *GetState() const
    {
        return m_poState;
    }

    void PopState();
    void PushState(GMLReadState *);

    bool ShouldLookForClassAtAnyLevel()
    {
        return m_bLookForClassAtAnyLevel;
    }

    int GetFeatureElementIndex(const char *pszElement, int nLen,
                               GMLAppSchemaType eAppSchemaType);
    int GetAttributeElementIndex(const char *pszElement, int nLen,
                                 const char *pszAttrKey = nullptr);
    bool IsCityGMLGenericAttributeElement(const char *pszElement, void *attr);

    void PushFeature(const char *pszElement, const char *pszFID,
                     int nClassIndex);

    void SetFeaturePropertyDirectly(const char *pszElement, char *pszValue,
                                    int iPropertyIn,
                                    GMLPropertyType eType = GMLPT_Untyped);

    void SetWidthFlag(bool bFlag)
    {
        m_bSetWidthFlag = bFlag;
    }

    bool HasStoppedParsing() override
    {
        return m_bStopParsing;
    }

    bool FetchAllGeometries()
    {
        return m_bFetchAllGeometries;
    }

    void SetGlobalSRSName(const char *pszGlobalSRSName) override;

    const char *GetGlobalSRSName() override
    {
        return m_pszGlobalSRSName;
    }

    bool CanUseGlobalSRSName() override
    {
        return m_bCanUseGlobalSRSName;
    }

    bool SetFilteredClassName(const char *pszClassName) override;

    const char *GetFilteredClassName() override
    {
        return m_pszFilteredClassName;
    }

    int GetFilteredClassIndex()
    {
        return m_nFilteredClassIndex;
    }

    bool IsSequentialLayers() const override
    {
        return m_nHasSequentialLayers == TRUE;
    }

    void SetReportAllAttributes(bool bFlag)
    {
        m_bReportAllAttributes = bFlag;
    }

    bool ReportAllAttributes() const
    {
        return m_bReportAllAttributes;
    }

    void SetIsWFSJointLayer(bool bFlag)
    {
        m_bIsWFSJointLayer = bFlag;
    }

    bool IsWFSJointLayer() const
    {
        return m_bIsWFSJointLayer;
    }

    void SetEmptyAsNull(bool bFlag)
    {
        m_bEmptyAsNull = bFlag;
    }

    bool IsEmptyAsNull() const
    {
        return m_bEmptyAsNull;
    }

    void SetUseBBOX(bool bFlag)
    {
        m_bUseBBOX = bFlag;
    }

    bool UseBBOX() const
    {
        return m_bUseBBOX;
    }

    static CPLMutex *hMutex;
};

#endif /* CPL_GMLREADERP_H_INCLUDED */
