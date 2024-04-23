/******************************************************************************
 * Project:  OGR
 * Purpose:  OGRGMLASDriver implementation
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 * Initial development funded by the European Earth observation programme
 * Copernicus
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault, <even dot rouault at spatialys dot com>
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

#include "ogr_gmlas.h"

#include "ogr_mem.h"
#include "cpl_sha256.h"

#include <algorithm>

/************************************************************************/
/*                 XercesInitializer::XercesInitializer()               */
/************************************************************************/

OGRGMLASDataSource::XercesInitializer::XercesInitializer()
{
    OGRInitializeXerces();
}

/************************************************************************/
/*                 XercesInitializer::~XercesInitializer()              */
/************************************************************************/

OGRGMLASDataSource::XercesInitializer::~XercesInitializer()
{
    OGRDeinitializeXerces();
}

/************************************************************************/
/*                          OGRGMLASDataSource()                        */
/************************************************************************/

OGRGMLASDataSource::OGRGMLASDataSource()
    : m_poFieldsMetadataLayer(std::make_unique<OGRMemLayer>(
          szOGR_FIELDS_METADATA, nullptr, wkbNone)),
      m_poLayersMetadataLayer(std::make_unique<OGRMemLayer>(
          szOGR_LAYERS_METADATA, nullptr, wkbNone)),
      m_poRelationshipsLayer(std::make_unique<OGRMemLayer>(
          szOGR_LAYER_RELATIONSHIPS, nullptr, wkbNone)),
      m_poOtherMetadataLayer(
          std::make_unique<OGRMemLayer>(szOGR_OTHER_METADATA, nullptr, wkbNone))
{
    // Initialize m_poFieldsMetadataLayer
    {
        OGRFieldDefn oFieldDefn(szLAYER_NAME, OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_INDEX, OFTInteger);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_NAME, OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_XPATH, OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_TYPE, OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_IS_LIST, OFTInteger);
        oFieldDefn.SetSubType(OFSTBoolean);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_MIN_OCCURS, OFTInteger);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_MAX_OCCURS, OFTInteger);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_REPETITION_ON_SEQUENCE, OFTInteger);
        oFieldDefn.SetSubType(OFSTBoolean);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_DEFAULT_VALUE, OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_FIXED_VALUE, OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_CATEGORY, OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_RELATED_LAYER, OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_JUNCTION_LAYER, OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_DOCUMENTATION, OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }

    // Initialize m_poLayersMetadataLayer
    {
        OGRFieldDefn oFieldDefn(szLAYER_NAME, OFTString);
        m_poLayersMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szLAYER_XPATH, OFTString);
        m_poLayersMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szLAYER_CATEGORY, OFTString);
        m_poLayersMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szLAYER_PKID_NAME, OFTString);
        m_poLayersMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szLAYER_PARENT_PKID_NAME, OFTString);
        m_poLayersMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szLAYER_DOCUMENTATION, OFTString);
        m_poLayersMetadataLayer->CreateField(&oFieldDefn);
    }

    // Initialize m_poRelationshipsLayer
    {
        OGRFieldDefn oFieldDefn(szPARENT_LAYER, OFTString);
        m_poRelationshipsLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szPARENT_PKID, OFTString);
        m_poRelationshipsLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szPARENT_ELEMENT_NAME, OFTString);
        m_poRelationshipsLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szCHILD_LAYER, OFTString);
        m_poRelationshipsLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szCHILD_PKID, OFTString);
        m_poRelationshipsLayer->CreateField(&oFieldDefn);
    }

    // Initialize m_poOtherMetadataLayer
    {
        OGRFieldDefn oFieldDefn(szKEY, OFTString);
        m_poOtherMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szVALUE, OFTString);
        m_poOtherMetadataLayer->CreateField(&oFieldDefn);
    }
}

/************************************************************************/
/*                            GetLayerCount()                           */
/************************************************************************/

int OGRGMLASDataSource::GetLayerCount()
{
    return static_cast<int>(m_apoLayers.size() +
                            m_apoRequestedMetadataLayers.size());
}

/************************************************************************/
/*                                GetLayer()                            */
/************************************************************************/

OGRLayer *OGRGMLASDataSource::GetLayer(int i)
{
    const int nBaseLayers = static_cast<int>(m_apoLayers.size());
    if (i >= nBaseLayers)
    {
        RunFirstPassIfNeeded(nullptr, nullptr, nullptr);
        if (i - nBaseLayers <
            static_cast<int>(m_apoRequestedMetadataLayers.size()))
            return m_apoRequestedMetadataLayers[i - nBaseLayers];
    }

    if (i < 0 || i >= nBaseLayers)
        return nullptr;
    return m_apoLayers[i].get();
}

/************************************************************************/
/*                             GetLayerByName()                         */
/************************************************************************/

OGRLayer *OGRGMLASDataSource::GetLayerByName(const char *pszName)
{
    if (OGRLayer *poLayer = GDALDataset::GetLayerByName(pszName))
        return poLayer;

    OGRLayer *apoLayers[] = {
        m_poFieldsMetadataLayer.get(), m_poLayersMetadataLayer.get(),
        m_poRelationshipsLayer.get(), m_poOtherMetadataLayer.get()};
    for (auto *poLayer : apoLayers)
    {
        if (EQUAL(pszName, poLayer->GetName()))
        {
            if (std::find(m_apoRequestedMetadataLayers.begin(),
                          m_apoRequestedMetadataLayers.end(),
                          poLayer) == m_apoRequestedMetadataLayers.end())
            {
                m_apoRequestedMetadataLayers.push_back(poLayer);
            }
            RunFirstPassIfNeeded(nullptr, nullptr, nullptr);
            return poLayer;
        }
    }

    return nullptr;
}

/************************************************************************/
/*                           TranslateClasses()                         */
/************************************************************************/

void OGRGMLASDataSource::TranslateClasses(OGRGMLASLayer *poParentLayer,
                                          const GMLASFeatureClass &oFC)
{
    const std::vector<GMLASFeatureClass> &aoClasses = oFC.GetNestedClasses();

    // CPLDebug("GMLAS", "TranslateClasses(%s,%s)",
    //          oFC.GetName().c_str(), oFC.GetXPath().c_str());

    m_apoLayers.emplace_back(std::make_unique<OGRGMLASLayer>(
        this, oFC, poParentLayer, m_oConf.m_bAlwaysGenerateOGRId));
    auto poLayer = m_apoLayers.back().get();

    for (size_t i = 0; i < aoClasses.size(); ++i)
    {
        TranslateClasses(poLayer, aoClasses[i]);
    }
}

/************************************************************************/
/*                         GMLASTopElementParser                        */
/************************************************************************/

class GMLASTopElementParser : public DefaultHandler
{
    std::vector<PairURIFilename> m_aoFilenames{};
    int m_nStartElementCounter = 0;
    bool m_bFinish = false;
    bool m_bFoundSWE = false;
    std::map<CPLString, CPLString> m_oMapDocNSURIToPrefix{};

  public:
    GMLASTopElementParser() = default;

    void Parse(const CPLString &osFilename,
               const std::shared_ptr<VSIVirtualHandle> &fp);

    const std::vector<PairURIFilename> &GetXSDs() const
    {
        return m_aoFilenames;
    }

    bool GetSWE() const
    {
        return m_bFoundSWE;
    }

    const std::map<CPLString, CPLString> &GetMapDocNSURIToPrefix() const
    {
        return m_oMapDocNSURIToPrefix;
    }

    virtual void startElement(const XMLCh *const uri,
                              const XMLCh *const localname,
                              const XMLCh *const qname,
                              const Attributes &attrs) override;
};

/************************************************************************/
/*                               Parse()                                */
/************************************************************************/

void GMLASTopElementParser::Parse(const CPLString &osFilename,
                                  const std::shared_ptr<VSIVirtualHandle> &fp)
{
    auto poSAXReader =
        std::unique_ptr<SAX2XMLReader>(XMLReaderFactory::createXMLReader());

    poSAXReader->setFeature(XMLUni::fgSAX2CoreNameSpaces, true);
    poSAXReader->setFeature(XMLUni::fgSAX2CoreNameSpacePrefixes, true);

    poSAXReader->setContentHandler(this);
    poSAXReader->setLexicalHandler(this);
    poSAXReader->setDTDHandler(this);

    poSAXReader->setFeature(XMLUni::fgXercesLoadSchema, false);

    GMLASErrorHandler oErrorHandler;
    poSAXReader->setErrorHandler(&oErrorHandler);

    GMLASInputSource oIS(osFilename, fp);

    try
    {
        XMLPScanToken oToFill;
        if (poSAXReader->parseFirst(oIS, oToFill))
        {
            while (!m_bFinish && poSAXReader->parseNext(oToFill))
            {
                // do nothing
            }
        }
    }
    catch (const XMLException &toCatch)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                 transcode(toCatch.getMessage()).c_str());
    }
    catch (const SAXException &toCatch)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                 transcode(toCatch.getMessage()).c_str());
    }
}

/************************************************************************/
/*                             startElement()                           */
/************************************************************************/

void GMLASTopElementParser::startElement(const XMLCh *const /*uri*/,
                                         const XMLCh *const /*localname*/,
                                         const XMLCh *const /*qname*/,
                                         const Attributes &attrs)
{
    m_nStartElementCounter++;

    for (unsigned int i = 0; i < attrs.getLength(); i++)
    {
        const std::string osAttrURIPrefix(transcode(attrs.getURI(i)));
        const std::string osAttrLocalname(transcode(attrs.getLocalName(i)));
        const std::string osAttrValue(transcode(attrs.getValue(i)));

        if (osAttrURIPrefix == szXSI_URI &&
            osAttrLocalname == szSCHEMA_LOCATION)
        {
            CPLDebug("GMLAS", "%s=%s", szSCHEMA_LOCATION, osAttrValue.c_str());

            const CPLStringList aosTokens(
                CSLTokenizeString2(osAttrValue.c_str(), " ", 0));
            const int nTokens = aosTokens.size();
            if ((nTokens % 2) == 0)
            {
                for (int j = 0; j < nTokens; j += 2)
                {
                    if (!STARTS_WITH(aosTokens[j], szWFS_URI) &&
                        !(EQUAL(aosTokens[j], szGML_URI) ||
                          STARTS_WITH(aosTokens[j],
                                      (CPLString(szGML_URI) + "/").c_str())))
                    {
                        CPLDebug("GMLAS", "Schema to analyze: %s -> %s",
                                 aosTokens[j], aosTokens[j + 1]);
                        m_aoFilenames.push_back(
                            PairURIFilename(aosTokens[j], aosTokens[j + 1]));
                    }
                }
            }
        }
        else if (osAttrURIPrefix == szXSI_URI &&
                 osAttrLocalname == szNO_NAMESPACE_SCHEMA_LOCATION)
        {
            CPLDebug("GMLAS", "%s=%s", szNO_NAMESPACE_SCHEMA_LOCATION,
                     osAttrValue.c_str());
            m_aoFilenames.push_back(PairURIFilename("", osAttrValue));
        }
        else if (osAttrURIPrefix == szXMLNS_URI && osAttrValue == szSWE_URI)
        {
            CPLDebug("GMLAS", "SWE namespace found");
            m_bFoundSWE = true;
        }
        else if (osAttrURIPrefix == szXMLNS_URI && !osAttrValue.empty() &&
                 !osAttrLocalname.empty())
        {
#ifdef DEBUG_VERBOSE
            CPLDebug("GMLAS", "Namespace %s = %s", osAttrLocalname.c_str(),
                     osAttrValue.c_str());
#endif
            m_oMapDocNSURIToPrefix[osAttrValue] = osAttrLocalname;
        }
    }

    if (m_nStartElementCounter == 1)
        m_bFinish = true;
}

/************************************************************************/
/*                         FillOtherMetadataLayer()                     */
/************************************************************************/

void OGRGMLASDataSource::FillOtherMetadataLayer(
    GDALOpenInfo *poOpenInfo, const CPLString &osConfigFile,
    const std::vector<PairURIFilename> &aoXSDs,
    const std::set<CPLString> &oSetSchemaURLs)
{
    // 2 "secret" options just used for tests
    const bool bKeepRelativePathsForMetadata = CPLTestBool(
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                             szKEEP_RELATIVE_PATHS_FOR_METADATA_OPTION, "NO"));

    const bool bExposeConfiguration = CPLTestBool(
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                             szEXPOSE_CONFIGURATION_IN_METADATA_OPTION, "YES"));

    const bool bExposeSchemaNames = CPLTestBool(
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                             szEXPOSE_SCHEMAS_NAME_IN_METADATA_OPTION, "YES"));

    OGRFeatureDefn *poFDefn = m_poOtherMetadataLayer->GetLayerDefn();

    if (!osConfigFile.empty() && bExposeConfiguration)
    {
        if (STARTS_WITH(osConfigFile, "<Configuration"))
        {
            OGRFeature oFeature(poFDefn);
            oFeature.SetField(szKEY, szCONFIGURATION_INLINED);
            oFeature.SetField(szVALUE, osConfigFile.c_str());
            CPL_IGNORE_RET_VAL(
                m_poOtherMetadataLayer->CreateFeature(&oFeature));
        }
        else
        {
            {
                OGRFeature oFeature(poFDefn);
                oFeature.SetField(szKEY, szCONFIGURATION_FILENAME);
                char *pszCurDir = CPLGetCurrentDir();
                if (!bKeepRelativePathsForMetadata &&
                    CPLIsFilenameRelative(osConfigFile) && pszCurDir != nullptr)
                {
                    oFeature.SetField(
                        szVALUE,
                        CPLFormFilename(pszCurDir, osConfigFile, nullptr));
                }
                else
                {
                    oFeature.SetField(szVALUE, osConfigFile.c_str());
                }
                CPLFree(pszCurDir);
                CPL_IGNORE_RET_VAL(
                    m_poOtherMetadataLayer->CreateFeature(&oFeature));
            }

            GByte *pabyRet = nullptr;
            if (VSIIngestFile(nullptr, osConfigFile, &pabyRet, nullptr, -1))
            {
                OGRFeature oFeature(poFDefn);
                oFeature.SetField(szKEY, szCONFIGURATION_INLINED);
                oFeature.SetField(szVALUE, reinterpret_cast<char *>(pabyRet));
                CPL_IGNORE_RET_VAL(
                    m_poOtherMetadataLayer->CreateFeature(&oFeature));
            }
            VSIFree(pabyRet);
        }
    }

    const char *const apszMeaningfulOptionsToStoreInMD[] = {
        szSWAP_COORDINATES_OPTION, szREMOVE_UNUSED_LAYERS_OPTION,
        szREMOVE_UNUSED_FIELDS_OPTION};
    for (size_t i = 0; i < CPL_ARRAYSIZE(apszMeaningfulOptionsToStoreInMD); ++i)
    {
        const char *pszKey = apszMeaningfulOptionsToStoreInMD[i];
        const char *pszVal =
            CSLFetchNameValue(poOpenInfo->papszOpenOptions, pszKey);
        if (pszVal)
        {
            OGRFeature oFeature(poFDefn);
            oFeature.SetField(szKEY, pszKey);
            oFeature.SetField(szVALUE, pszVal);
            CPL_IGNORE_RET_VAL(
                m_poOtherMetadataLayer->CreateFeature(&oFeature));
        }
    }

    CPLString osAbsoluteGMLFilename;
    if (!m_osGMLFilename.empty())
    {
        OGRFeature oFeature(poFDefn);
        oFeature.SetField(szKEY, szDOCUMENT_FILENAME);
        char *pszCurDir = CPLGetCurrentDir();
        if (!bKeepRelativePathsForMetadata &&
            CPLIsFilenameRelative(m_osGMLFilename) && pszCurDir != nullptr)
        {
            osAbsoluteGMLFilename =
                CPLFormFilename(pszCurDir, m_osGMLFilename, nullptr);
        }
        else
            osAbsoluteGMLFilename = m_osGMLFilename;
        oFeature.SetField(szVALUE, osAbsoluteGMLFilename.c_str());
        CPLFree(pszCurDir);
        CPL_IGNORE_RET_VAL(m_poOtherMetadataLayer->CreateFeature(&oFeature));
    }

    int nNSIdx = 1;
    std::set<CPLString> oSetVisitedURI;
    for (int i = 0; i < static_cast<int>(aoXSDs.size()); i++)
    {
        const CPLString osURI(aoXSDs[i].first);
        const CPLString osXSDFilename(aoXSDs[i].second);

        oSetVisitedURI.insert(osURI);

        if (osURI == szOGRGMLAS_URI)
            continue;

        {
            OGRFeature oFeature(poFDefn);
            oFeature.SetField(szKEY, CPLSPrintf(szNAMESPACE_URI_FMT, nNSIdx));
            oFeature.SetField(szVALUE, osURI.c_str());
            CPL_IGNORE_RET_VAL(
                m_poOtherMetadataLayer->CreateFeature(&oFeature));
        }

        {
            OGRFeature oFeature(poFDefn);
            oFeature.SetField(szKEY,
                              CPLSPrintf(szNAMESPACE_LOCATION_FMT, nNSIdx));

            const CPLString osAbsoluteXSDFilename(
                (osXSDFilename.find("http://") != 0 &&
                 osXSDFilename.find("https://") != 0 &&
                 CPLIsFilenameRelative(osXSDFilename))
                    ? CPLString(
                          CPLFormFilename(CPLGetDirname(osAbsoluteGMLFilename),
                                          osXSDFilename, nullptr))
                    : osXSDFilename);
            oFeature.SetField(szVALUE, osAbsoluteXSDFilename.c_str());
            CPL_IGNORE_RET_VAL(
                m_poOtherMetadataLayer->CreateFeature(&oFeature));
        }

        if (m_oMapURIToPrefix.find(osURI) != m_oMapURIToPrefix.end())
        {
            OGRFeature oFeature(poFDefn);
            oFeature.SetField(szKEY,
                              CPLSPrintf(szNAMESPACE_PREFIX_FMT, nNSIdx));
            oFeature.SetField(szVALUE, m_oMapURIToPrefix[osURI].c_str());
            CPL_IGNORE_RET_VAL(
                m_poOtherMetadataLayer->CreateFeature(&oFeature));
        }

        nNSIdx++;
    }

    for (const auto &oIter : m_oMapURIToPrefix)
    {
        const CPLString &osURI(oIter.first);
        const CPLString &osPrefix(oIter.second);

        if (oSetVisitedURI.find(osURI) == oSetVisitedURI.end() &&
            osURI != szXML_URI && osURI != szXS_URI && osURI != szXSI_URI &&
            osURI != szXMLNS_URI && osURI != szOGRGMLAS_URI)
        {
            {
                OGRFeature oFeature(poFDefn);
                oFeature.SetField(szKEY,
                                  CPLSPrintf(szNAMESPACE_URI_FMT, nNSIdx));
                oFeature.SetField(szVALUE, osURI.c_str());
                CPL_IGNORE_RET_VAL(
                    m_poOtherMetadataLayer->CreateFeature(&oFeature));
            }

            {
                OGRFeature oFeature(poFDefn);
                oFeature.SetField(szKEY,
                                  CPLSPrintf(szNAMESPACE_PREFIX_FMT, nNSIdx));
                oFeature.SetField(szVALUE, osPrefix.c_str());
                CPL_IGNORE_RET_VAL(
                    m_poOtherMetadataLayer->CreateFeature(&oFeature));
            }

            nNSIdx++;
        }
    }

    if (!m_osGMLVersionFound.empty())
    {
        OGRFeature oFeature(poFDefn);
        oFeature.SetField(szKEY, szGML_VERSION);
        oFeature.SetField(szVALUE, m_osGMLVersionFound);
        CPL_IGNORE_RET_VAL(m_poOtherMetadataLayer->CreateFeature(&oFeature));
    }

    int nSchemaIdx = 1;
    if (bExposeSchemaNames)
    {
        for (const auto &osSchemaURL : oSetSchemaURLs)
        {
            OGRFeature oFeature(poFDefn);
            oFeature.SetField(szKEY, CPLSPrintf(szSCHEMA_NAME_FMT, nSchemaIdx));
            oFeature.SetField(szVALUE, osSchemaURL.c_str());
            CPL_IGNORE_RET_VAL(
                m_poOtherMetadataLayer->CreateFeature(&oFeature));

            nSchemaIdx++;
        }
    }
}

/************************************************************************/
/*                         BuildXSDVector()                             */
/************************************************************************/

std::vector<PairURIFilename>
OGRGMLASDataSource::BuildXSDVector(const CPLString &osXSDFilenames)
{
    std::vector<PairURIFilename> aoXSDs;
    char **papszTokens = CSLTokenizeString2(osXSDFilenames, ",", 0);
    char *pszCurDir = CPLGetCurrentDir();
    for (int i = 0; papszTokens != nullptr && papszTokens[i] != nullptr; i++)
    {
        if (!STARTS_WITH(papszTokens[i], "http://") &&
            !STARTS_WITH(papszTokens[i], "https://") &&
            CPLIsFilenameRelative(papszTokens[i]) && pszCurDir != nullptr)
        {
            aoXSDs.push_back(PairURIFilename(
                "", CPLFormFilename(pszCurDir, papszTokens[i], nullptr)));
        }
        else
        {
            aoXSDs.push_back(PairURIFilename("", papszTokens[i]));
        }
    }
    CPLFree(pszCurDir);
    CSLDestroy(papszTokens);
    return aoXSDs;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

bool OGRGMLASDataSource::Open(GDALOpenInfo *poOpenInfo)
{
    CPLString osConfigFile = CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                                                  szCONFIG_FILE_OPTION, "");
    if (osConfigFile.empty())
    {
        const char *pszConfigFile =
            CPLFindFile("gdal", szDEFAULT_CONF_FILENAME);
        if (pszConfigFile)
            osConfigFile = pszConfigFile;
    }
    if (osConfigFile.empty())
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "No configuration file found. Using hard-coded defaults");
        m_oConf.Finalize();
    }
    else
    {
        if (!m_oConf.Load(osConfigFile))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Loading of configuration failed");
            return false;
        }
    }

    m_oCache.SetCacheDirectory(m_oConf.m_osXSDCacheDirectory);
    const bool bRefreshCache(CPLTestBool(CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, szREFRESH_CACHE_OPTION, "NO")));
    m_oCache.SetRefreshMode(bRefreshCache);
    m_oCache.SetAllowDownload(m_oConf.m_bAllowRemoteSchemaDownload);

    m_oIgnoredXPathMatcher.SetRefXPaths(m_oConf.m_oMapPrefixToURIIgnoredXPaths,
                                        m_oConf.m_aosIgnoredXPaths);

    {
        std::vector<CPLString> oVector;
        for (const auto &oIter : m_oConf.m_oMapChildrenElementsConstraints)
            oVector.push_back(oIter.first);
        m_oChildrenElementsConstraintsXPathMatcher.SetRefXPaths(
            m_oConf.m_oMapPrefixToURITypeConstraints, oVector);
    }

    m_oForcedFlattenedXPathMatcher.SetRefXPaths(
        m_oConf.m_oMapPrefixToURIFlatteningRules,
        m_oConf.m_osForcedFlattenedXPath);

    m_oDisabledFlattenedXPathMatcher.SetRefXPaths(
        m_oConf.m_oMapPrefixToURIFlatteningRules,
        m_oConf.m_osDisabledFlattenedXPath);

    GMLASSchemaAnalyzer oAnalyzer(
        m_oIgnoredXPathMatcher, m_oChildrenElementsConstraintsXPathMatcher,
        m_oConf.m_oMapChildrenElementsConstraints,
        m_oForcedFlattenedXPathMatcher, m_oDisabledFlattenedXPathMatcher);
    oAnalyzer.SetUseArrays(m_oConf.m_bUseArrays);
    oAnalyzer.SetUseNullState(m_oConf.m_bUseNullState);
    oAnalyzer.SetInstantiateGMLFeaturesOnly(
        m_oConf.m_bInstantiateGMLFeaturesOnly);
    oAnalyzer.SetIdentifierMaxLength(m_oConf.m_nIdentifierMaxLength);
    oAnalyzer.SetCaseInsensitiveIdentifier(
        m_oConf.m_bCaseInsensitiveIdentifier);
    oAnalyzer.SetPGIdentifierLaundering(m_oConf.m_bPGIdentifierLaundering);
    oAnalyzer.SetMaximumFieldsForFlattening(
        m_oConf.m_nMaximumFieldsForFlattening);
    oAnalyzer.SetAlwaysGenerateOGRId(m_oConf.m_bAlwaysGenerateOGRId);

    m_osGMLFilename =
        STARTS_WITH_CI(poOpenInfo->pszFilename, szGMLAS_PREFIX)
            ? CPLExpandTilde(poOpenInfo->pszFilename + strlen(szGMLAS_PREFIX))
            : poOpenInfo->pszFilename;

    CPLString osXSDFilenames =
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, szXSD_OPTION, "");

    std::shared_ptr<VSIVirtualHandle> fpGML;
    if (!m_osGMLFilename.empty())
    {
        fpGML.reset(VSIFOpenL(m_osGMLFilename, "rb"), VSIVirtualHandleCloser{});
        if (fpGML == nullptr)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s",
                     m_osGMLFilename.c_str());
            return false;
        }
    }
    else if (osXSDFilenames.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s open option must be provided when no "
                 "XML data file is passed",
                 szXSD_OPTION);
        return false;
    }

    GMLASTopElementParser topElementParser;
    if (!m_osGMLFilename.empty())
    {
        topElementParser.Parse(m_osGMLFilename, fpGML);
        if (m_oConf.m_eSWEActivationMode ==
            GMLASConfiguration::SWE_ACTIVATE_IF_NAMESPACE_FOUND)
        {
            m_bFoundSWE = topElementParser.GetSWE();
        }
        else if (m_oConf.m_eSWEActivationMode ==
                 GMLASConfiguration::SWE_ACTIVATE_TRUE)
        {
            m_bFoundSWE = true;
        }
        oAnalyzer.SetMapDocNSURIToPrefix(
            topElementParser.GetMapDocNSURIToPrefix());
    }
    std::vector<PairURIFilename> aoXSDs;
    if (osXSDFilenames.empty())
    {
        aoXSDs = topElementParser.GetXSDs();
    }
    else
    {
        aoXSDs = BuildXSDVector(osXSDFilenames);
    }
    if (fpGML)
    {
        m_osHash =
            CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "HASH", "");
        if (m_osHash.empty())
        {
            fpGML->Seek(0, SEEK_SET);
            std::string osBuffer;
            osBuffer.resize(8192);
            size_t nRead = fpGML->Read(&osBuffer[0], 1, 8192);
            osBuffer.resize(nRead);
            size_t nPos = osBuffer.find("timeStamp=\"");
            if (nPos != std::string::npos)
            {
                size_t nPos2 =
                    osBuffer.find('"', nPos + strlen("timeStamp=\""));
                if (nPos2 != std::string::npos)
                    osBuffer.replace(nPos, nPos2 - nPos + 1, nPos2 - nPos + 1,
                                     ' ');
            }
            CPL_SHA256Context ctxt;
            CPL_SHA256Init(&ctxt);
            CPL_SHA256Update(&ctxt, osBuffer.data(), osBuffer.size());

            VSIStatBufL sStat;
            if (VSIStatL(m_osGMLFilename, &sStat) == 0)
            {
                m_nFileSize = sStat.st_size;
                GUInt64 nFileSizeLittleEndian =
                    static_cast<GUInt64>(sStat.st_size);
                CPL_LSBPTR64(&nFileSizeLittleEndian);
                CPL_SHA256Update(&ctxt, &nFileSizeLittleEndian,
                                 sizeof(nFileSizeLittleEndian));
            }

            GByte abyHash[CPL_SHA256_HASH_SIZE];
            CPL_SHA256Final(&ctxt, abyHash);
            // Half of the hash should be enough for our purpose
            char *pszHash = CPLBinaryToHex(CPL_SHA256_HASH_SIZE / 2, abyHash);
            m_osHash = pszHash;
            CPLFree(pszHash);
        }

        fpGML->Seek(0, SEEK_SET);
        PushUnusedGMLFilePointer(fpGML);
    }

    if (aoXSDs.empty())
    {
        if (osXSDFilenames.empty())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "No schema locations found when analyzing data file: "
                     "%s open option must be provided",
                     szXSD_OPTION);
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "No schema locations found");
        }
        return false;
    }

    m_bSchemaFullChecking = CPLFetchBool(poOpenInfo->papszOpenOptions,
                                         szSCHEMA_FULL_CHECKING_OPTION,
                                         m_oConf.m_bSchemaFullChecking);

    m_bHandleMultipleImports = CPLFetchBool(poOpenInfo->papszOpenOptions,
                                            szHANDLE_MULTIPLE_IMPORTS_OPTION,
                                            m_oConf.m_bHandleMultipleImports);

    bool bRet =
        oAnalyzer.Analyze(m_oCache, CPLGetDirname(m_osGMLFilename), aoXSDs,
                          m_bSchemaFullChecking, m_bHandleMultipleImports);
    if (!bRet)
    {
        return false;
    }

    if (!osXSDFilenames.empty())
        m_aoXSDsManuallyPassed = aoXSDs;

    m_oMapURIToPrefix = oAnalyzer.GetMapURIToPrefix();

    m_osGMLVersionFound = oAnalyzer.GetGMLVersionFound();

    const std::set<CPLString> &oSetSchemaURLs = oAnalyzer.GetSchemaURLS();

    FillOtherMetadataLayer(poOpenInfo, osConfigFile, aoXSDs, oSetSchemaURLs);

    if (CPLFetchBool(poOpenInfo->papszOpenOptions,
                     szEXPOSE_METADATA_LAYERS_OPTION,
                     m_oConf.m_bExposeMetadataLayers))
    {
        m_apoRequestedMetadataLayers.push_back(m_poFieldsMetadataLayer.get());
        m_apoRequestedMetadataLayers.push_back(m_poLayersMetadataLayer.get());
        m_apoRequestedMetadataLayers.push_back(m_poRelationshipsLayer.get());
        m_apoRequestedMetadataLayers.push_back(m_poOtherMetadataLayer.get());
    }

    const char *pszSwapCoordinates = CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, szSWAP_COORDINATES_OPTION, "AUTO");
    if (EQUAL(pszSwapCoordinates, "AUTO"))
    {
        m_eSwapCoordinates = GMLAS_SWAP_AUTO;
    }
    else if (CPLTestBool(pszSwapCoordinates))
    {
        m_eSwapCoordinates = GMLAS_SWAP_YES;
    }
    else
    {
        m_eSwapCoordinates = GMLAS_SWAP_NO;
    }

    const std::vector<GMLASFeatureClass> &aoClasses = oAnalyzer.GetClasses();

    // First "standard" tables
    for (auto &oClass : aoClasses)
    {
        if (oClass.GetParentXPath().empty())
            TranslateClasses(nullptr, oClass);
    }
    // Then junction tables
    for (auto &oClass : aoClasses)
    {
        if (!oClass.GetParentXPath().empty())
            TranslateClasses(nullptr, oClass);
    }

    // And now do initialization since we need to have instantiated everything
    // to be able to do cross-layer links
    for (auto &poLayer : m_apoLayers)
    {
        poLayer->PostInit(m_oConf.m_bIncludeGeometryXML);
    }
    m_bLayerInitFinished = true;

    // Do optional validation
    m_bValidate = CPLFetchBool(poOpenInfo->papszOpenOptions, szVALIDATE_OPTION,
                               m_oConf.m_bValidate);

    m_bRemoveUnusedLayers = CPLFetchBool(poOpenInfo->papszOpenOptions,
                                         szREMOVE_UNUSED_LAYERS_OPTION,
                                         m_oConf.m_bRemoveUnusedLayers);

    m_bRemoveUnusedFields = CPLFetchBool(poOpenInfo->papszOpenOptions,
                                         szREMOVE_UNUSED_FIELDS_OPTION,
                                         m_oConf.m_bRemoveUnusedFields);

    m_oXLinkResolver.SetConf(m_oConf.m_oXLinkResolution);
    m_oXLinkResolver.SetRefreshMode(bRefreshCache);

    if (m_bValidate || m_bRemoveUnusedLayers ||
        (m_bFoundSWE &&
         (m_oConf.m_bSWEProcessDataRecord || m_oConf.m_bSWEProcessDataArray)))
    {
        CPLErrorReset();
        RunFirstPassIfNeeded(nullptr, nullptr, nullptr);
        if (CPLFetchBool(poOpenInfo->papszOpenOptions,
                         szFAIL_IF_VALIDATION_ERROR_OPTION,
                         m_oConf.m_bFailIfValidationError) &&
            CPLGetLastErrorType() != CE_None)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Validation errors encountered");
            return false;
        }
    }
    if (CPLGetLastErrorType() == CE_Failure)
        CPLErrorReset();

    return true;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGMLASDataSource::TestCapability(const char *pszCap)
{
    return EQUAL(pszCap, ODsCRandomLayerRead);
}

/************************************************************************/
/*                           CreateReader()                             */
/************************************************************************/

GMLASReader *
OGRGMLASDataSource::CreateReader(std::shared_ptr<VSIVirtualHandle> &fpGML,
                                 GDALProgressFunc pfnProgress,
                                 void *pProgressData)
{
    if (fpGML == nullptr)
    {
        // Try recycling an already opened and unused file pointer
        fpGML = PopUnusedGMLFilePointer();
        if (fpGML == nullptr)
            fpGML.reset(VSIFOpenL(GetGMLFilename(), "rb"),
                        VSIVirtualHandleCloser{});
        if (fpGML == nullptr)
            return nullptr;
    }

    auto poReader = std::make_unique<GMLASReader>(
        GetCache(), GetIgnoredXPathMatcher(), m_oXLinkResolver);
    poReader->Init(GetGMLFilename(), fpGML, GetMapURIToPrefix(), GetLayers(),
                   false, std::vector<PairURIFilename>(), m_bSchemaFullChecking,
                   m_bHandleMultipleImports);

    poReader->SetSwapCoordinates(GetSwapCoordinates());

    poReader->SetFileSize(m_nFileSize);

    if (!RunFirstPassIfNeeded(poReader.get(), pfnProgress, pProgressData))
    {
        return nullptr;
    }

    poReader->SetMapIgnoredXPathToWarn(GetMapIgnoredXPathToWarn());

    poReader->SetHash(m_osHash);

    return poReader.release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGMLASDataSource::ResetReading()
{
    m_poReader.reset();
    for (auto *poLayer : m_apoRequestedMetadataLayers)
        poLayer->ResetReading();
    m_bEndOfReaderLayers = false;
    m_nCurMetadataLayerIdx = -1;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGMLASDataSource::GetNextFeature(OGRLayer **ppoBelongingLayer,
                                               double *pdfProgressPct,
                                               GDALProgressFunc pfnProgress,
                                               void *pProgressData)
{
    if (m_bEndOfReaderLayers)
    {
        if (m_nCurMetadataLayerIdx >= 0 &&
            m_nCurMetadataLayerIdx <
                static_cast<int>(m_apoRequestedMetadataLayers.size()))
        {
            while (true)
            {
                OGRLayer *poLayer =
                    m_apoRequestedMetadataLayers[m_nCurMetadataLayerIdx];
                OGRFeature *poFeature = poLayer->GetNextFeature();
                if (poFeature != nullptr)
                {
                    if (pdfProgressPct != nullptr)
                        *pdfProgressPct = 1.0;
                    if (ppoBelongingLayer != nullptr)
                        *ppoBelongingLayer = poLayer;
                    return poFeature;
                }
                if (m_nCurMetadataLayerIdx + 1 <
                    static_cast<int>(m_apoRequestedMetadataLayers.size()))
                {
                    m_nCurMetadataLayerIdx++;
                }
                else
                {
                    m_nCurMetadataLayerIdx = -1;
                    break;
                }
            }
        }

        if (pdfProgressPct != nullptr)
            *pdfProgressPct = 1.0;
        if (ppoBelongingLayer != nullptr)
            *ppoBelongingLayer = nullptr;
        return nullptr;
    }

    const double dfInitialScanRatio = 0.1;
    if (m_poReader == nullptr)
    {
        void *pScaledProgress = GDALCreateScaledProgress(
            0.0, dfInitialScanRatio, pfnProgress, pProgressData);

        m_poReader.reset(CreateReader(
            m_fpGMLParser, pScaledProgress ? GDALScaledProgress : nullptr,
            pScaledProgress));

        GDALDestroyScaledProgress(pScaledProgress);

        if (m_poReader == nullptr)
        {
            if (pdfProgressPct != nullptr)
                *pdfProgressPct = 1.0;
            if (ppoBelongingLayer != nullptr)
                *ppoBelongingLayer = nullptr;
            m_bEndOfReaderLayers = true;
            if (!m_apoRequestedMetadataLayers.empty())
            {
                m_nCurMetadataLayerIdx = 0;
                return GetNextFeature(ppoBelongingLayer, pdfProgressPct,
                                      pfnProgress, pProgressData);
            }
            else
            {
                return nullptr;
            }
        }
    }

    void *pScaledProgress = GDALCreateScaledProgress(
        dfInitialScanRatio, 1.0, pfnProgress, pProgressData);

    while (true)
    {
        OGRGMLASLayer *poBelongingLayer = nullptr;
        auto poFeature = std::unique_ptr<OGRFeature>(m_poReader->GetNextFeature(
            &poBelongingLayer, pScaledProgress ? GDALScaledProgress : nullptr,
            pScaledProgress));
        if (poFeature == nullptr ||
            poBelongingLayer->EvaluateFilter(poFeature.get()))
        {
            if (ppoBelongingLayer != nullptr)
                *ppoBelongingLayer = poBelongingLayer;
            if (pdfProgressPct != nullptr)
            {
                const vsi_l_offset nOffset = m_fpGMLParser->Tell();
                if (nOffset == m_nFileSize)
                    *pdfProgressPct = 1.0;
                else
                    *pdfProgressPct =
                        dfInitialScanRatio +
                        (1.0 - dfInitialScanRatio) * nOffset / m_nFileSize;
            }
            GDALDestroyScaledProgress(pScaledProgress);
            if (poFeature == nullptr)
            {
                m_bEndOfReaderLayers = true;
                if (!m_apoRequestedMetadataLayers.empty())
                {
                    m_nCurMetadataLayerIdx = 0;
                    return GetNextFeature(ppoBelongingLayer, pdfProgressPct,
                                          pfnProgress, pProgressData);
                }
                else
                {
                    return nullptr;
                }
            }
            else
                return poFeature.release();
        }
    }
}

/************************************************************************/
/*                          GetLayerByXPath()                           */
/************************************************************************/

OGRGMLASLayer *OGRGMLASDataSource::GetLayerByXPath(const CPLString &osXPath)
{
    for (auto &poLayer : m_apoLayers)
    {
        if (poLayer->GetFeatureClass().GetXPath() == osXPath)
        {
            return poLayer.get();
        }
    }
    return nullptr;
}

/************************************************************************/
/*                       PushUnusedGMLFilePointer()                     */
/************************************************************************/

void OGRGMLASDataSource::PushUnusedGMLFilePointer(
    std::shared_ptr<VSIVirtualHandle> &fpGML)
{
    if (m_fpGML == nullptr)
    {
        std::swap(m_fpGML, fpGML);
    }
    else
    {
        fpGML.reset();
    }
}

/************************************************************************/
/*                        PopUnusedGMLFilePointer()                     */
/************************************************************************/

std::shared_ptr<VSIVirtualHandle> OGRGMLASDataSource::PopUnusedGMLFilePointer()
{
    std::shared_ptr<VSIVirtualHandle> fpGML;
    std::swap(fpGML, m_fpGML);
    return fpGML;
}

/************************************************************************/
/*                    InitReaderWithFirstPassElements()                 */
/************************************************************************/

void OGRGMLASDataSource::InitReaderWithFirstPassElements(GMLASReader *poReader)
{
    if (poReader != nullptr)
    {
        poReader->SetMapSRSNameToInvertedAxis(m_oMapSRSNameToInvertedAxis);
        poReader->SetMapGeomFieldDefnToSRSName(m_oMapGeomFieldDefnToSRSName);
        poReader->SetProcessDataRecord(m_bFoundSWE &&
                                       m_oConf.m_bSWEProcessDataRecord);
        poReader->SetSWEDataArrayLayersRef(m_apoSWEDataArrayLayersRef);
        poReader->SetMapElementIdToLayer(m_oMapElementIdToLayer);
        poReader->SetMapElementIdToPKID(m_oMapElementIdToPKID);
        poReader->SetDefaultSrsDimension(m_nDefaultSrsDimension);
    }
}

/************************************************************************/
/*                          RunFirstPassIfNeeded()                      */
/************************************************************************/

bool OGRGMLASDataSource::RunFirstPassIfNeeded(GMLASReader *poReader,
                                              GDALProgressFunc pfnProgress,
                                              void *pProgressData)
{
    if (m_bFirstPassDone)
    {
        InitReaderWithFirstPassElements(poReader);
        return true;
    }

    m_bFirstPassDone = true;

    // Determine if we have geometry fields in any layer
    // If so, do an initial pass to determine the SRS of those geometry fields.
    bool bHasGeomFields = false;
    for (auto &poLayer : m_apoLayers)
    {
        poLayer->SetLayerDefnFinalized(true);
        if (poLayer->GetLayerDefn()->GetGeomFieldCount() > 0)
        {
            bHasGeomFields = true;
            break;
        }
    }

    bool bSuccess = true;
    const bool bHasURLSpecificRules =
        !m_oXLinkResolver.GetConf().m_aoURLSpecificRules.empty();
    if (bHasGeomFields || m_bValidate || m_bRemoveUnusedLayers ||
        m_bRemoveUnusedFields || bHasURLSpecificRules ||
        m_oXLinkResolver.GetConf().m_bResolveInternalXLinks ||
        (m_bFoundSWE &&
         (m_oConf.m_bSWEProcessDataRecord || m_oConf.m_bSWEProcessDataArray)))
    {
        bool bJustOpenedFiled = false;
        std::shared_ptr<VSIVirtualHandle> fp;
        if (poReader)
            fp = poReader->GetFP();
        else
        {
            fp.reset(VSIFOpenL(GetGMLFilename(), "rb"),
                     VSIVirtualHandleCloser{});
            if (fp == nullptr)
            {
                return false;
            }
            bJustOpenedFiled = true;
        }

        auto poReaderFirstPass = std::make_unique<GMLASReader>(
            m_oCache, m_oIgnoredXPathMatcher, m_oXLinkResolver);
        poReaderFirstPass->Init(GetGMLFilename(), fp, GetMapURIToPrefix(),
                                GetLayers(), m_bValidate,
                                m_aoXSDsManuallyPassed, m_bSchemaFullChecking,
                                m_bHandleMultipleImports);

        poReaderFirstPass->SetProcessDataRecord(
            m_bFoundSWE && m_oConf.m_bSWEProcessDataRecord);

        poReaderFirstPass->SetFileSize(m_nFileSize);

        poReaderFirstPass->SetMapIgnoredXPathToWarn(
            m_oConf.m_oMapIgnoredXPathToWarn);

        poReaderFirstPass->SetHash(m_osHash);

        // No need to warn afterwards
        m_oConf.m_oMapIgnoredXPathToWarn.clear();

        std::set<CPLString> aoSetRemovedLayerNames;
        bSuccess = poReaderFirstPass->RunFirstPass(
            pfnProgress, pProgressData, m_bRemoveUnusedLayers,
            m_bRemoveUnusedFields,
            m_bFoundSWE && m_oConf.m_bSWEProcessDataArray,
            m_poFieldsMetadataLayer.get(), m_poLayersMetadataLayer.get(),
            m_poRelationshipsLayer.get(), aoSetRemovedLayerNames);

        std::vector<std::unique_ptr<OGRGMLASLayer>> apoSWEDataArrayLayers =
            poReaderFirstPass->StealSWEDataArrayLayersOwned();
        for (auto &poLayer : apoSWEDataArrayLayers)
        {
            poLayer->SetDataSource(this);
            m_apoSWEDataArrayLayersRef.push_back(poLayer.get());
            m_apoLayers.emplace_back(std::move(poLayer));
        }

        // If we have removed layers, we also need to cleanup our special
        // metadata layers
        if (!aoSetRemovedLayerNames.empty())
        {
            // Removing features while iterating works here given the layers
            // are MEM layers
            m_poLayersMetadataLayer->ResetReading();
            for (auto &poFeature : *m_poLayersMetadataLayer)
            {
                const char *pszLayerName =
                    poFeature->GetFieldAsString(szLAYER_NAME);
                if (aoSetRemovedLayerNames.find(pszLayerName) !=
                    aoSetRemovedLayerNames.end())
                {
                    CPL_IGNORE_RET_VAL(m_poLayersMetadataLayer->DeleteFeature(
                        poFeature->GetFID()));
                }
            }
            m_poLayersMetadataLayer->ResetReading();

            m_poFieldsMetadataLayer->ResetReading();
            for (auto &poFeature : *m_poFieldsMetadataLayer)
            {
                const char *pszLayerName =
                    poFeature->GetFieldAsString(szLAYER_NAME);
                const char *pszRelatedLayerName =
                    poFeature->GetFieldAsString(szFIELD_RELATED_LAYER);
                if (aoSetRemovedLayerNames.find(pszLayerName) !=
                        aoSetRemovedLayerNames.end() ||
                    aoSetRemovedLayerNames.find(pszRelatedLayerName) !=
                        aoSetRemovedLayerNames.end())
                {
                    CPL_IGNORE_RET_VAL(m_poFieldsMetadataLayer->DeleteFeature(
                        poFeature->GetFID()));
                }
            }
            m_poFieldsMetadataLayer->ResetReading();

            m_poRelationshipsLayer->ResetReading();
            for (auto &poFeature : *m_poRelationshipsLayer)
            {
                const char *pszParentLayerName =
                    poFeature->GetFieldAsString(szPARENT_LAYER);
                const char *pszChildLayerName =
                    poFeature->GetFieldAsString(szCHILD_LAYER);
                if (aoSetRemovedLayerNames.find(pszParentLayerName) !=
                        aoSetRemovedLayerNames.end() ||
                    aoSetRemovedLayerNames.find(pszChildLayerName) !=
                        aoSetRemovedLayerNames.end())
                {
                    CPL_IGNORE_RET_VAL(m_poRelationshipsLayer->DeleteFeature(
                        poFeature->GetFID()));
                }
            }
            m_poRelationshipsLayer->ResetReading();
        }

        // Store  maps to reinject them in real readers
        m_oMapSRSNameToInvertedAxis =
            poReaderFirstPass->GetMapSRSNameToInvertedAxis();
        m_oMapGeomFieldDefnToSRSName =
            poReaderFirstPass->GetMapGeomFieldDefnToSRSName();

        m_oMapElementIdToLayer = poReaderFirstPass->GetMapElementIdToLayer();
        m_oMapElementIdToPKID = poReaderFirstPass->GetMapElementIdToPKID();
        m_nDefaultSrsDimension = poReaderFirstPass->GetDefaultSrsDimension();

        poReaderFirstPass.reset();

        fp->Seek(0, SEEK_SET);
        if (bJustOpenedFiled)
            PushUnusedGMLFilePointer(fp);

        InitReaderWithFirstPassElements(poReader);
    }

    return bSuccess;
}
