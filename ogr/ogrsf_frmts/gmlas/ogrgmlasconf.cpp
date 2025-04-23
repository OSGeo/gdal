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
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_gmlas.h"

#include "cpl_minixml.h"

#include <algorithm>

#ifdef EMBED_RESOURCE_FILES
#include "embedded_resources.h"
#endif

/************************************************************************/
/*                              Finalize()                              */
/************************************************************************/

void GMLASConfiguration::Finalize()
{
    if (m_bAllowXSDCache && m_osXSDCacheDirectory.empty())
    {
        m_osXSDCacheDirectory = GDALGetCacheDirectory();
        if (m_osXSDCacheDirectory.empty())
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Could not determine a directory for GMLAS XSD cache");
        }
        else
        {
            m_osXSDCacheDirectory = CPLFormFilenameSafe(
                m_osXSDCacheDirectory, "gmlas_xsd_cache", nullptr);
            CPLDebug("GMLAS", "XSD cache directory: %s",
                     m_osXSDCacheDirectory.c_str());
        }
    }
}

/************************************************************************/
/*                          CPLGetXMLBoolValue()                        */
/************************************************************************/

static bool CPLGetXMLBoolValue(CPLXMLNode *psNode, const char *pszKey,
                               bool bDefault)
{
    const char *pszVal = CPLGetXMLValue(psNode, pszKey, nullptr);
    if (pszVal)
        return CPLTestBool(pszVal);
    else
        return bDefault;
}

/************************************************************************/
/*                            IsValidXPath()                            */
/************************************************************************/

static bool IsValidXPath(const CPLString &osXPath)
{
    // Check that the XPath syntax belongs to the subset we
    // understand
    bool bOK = !osXPath.empty();
    for (size_t i = 0; i < osXPath.size(); ++i)
    {
        const char chCur = osXPath[i];
        if (chCur == '/')
        {
            // OK
        }
        else if (chCur == '@' && (i == 0 || osXPath[i - 1] == '/') &&
                 i < osXPath.size() - 1 &&
                 isalpha(static_cast<unsigned char>(osXPath[i + 1])))
        {
            // OK
        }
        else if (chCur == '_' || isalpha(static_cast<unsigned char>(chCur)))
        {
            // OK
        }
        else if (isdigit(static_cast<unsigned char>(chCur)) && i > 0 &&
                 (isalnum(static_cast<unsigned char>(osXPath[i - 1])) ||
                  osXPath[i - 1] == '_'))
        {
            // OK
        }
        else if (chCur == ':' && i > 0 &&
                 (isalnum(static_cast<unsigned char>(osXPath[i - 1])) ||
                  osXPath[i - 1] == '_') &&
                 i < osXPath.size() - 1 &&
                 isalpha(static_cast<unsigned char>(osXPath[i + 1])))
        {
            // OK
        }
        else
        {
            bOK = false;
            break;
        }
    }
    return bOK;
}

/************************************************************************/
/*                    GMLASConfigurationErrorHandler()                  */
/************************************************************************/

static void CPL_STDCALL GMLASConfigurationErrorHandler(CPLErr /*eErr*/,
                                                       CPLErrorNum /*nType*/,
                                                       const char *pszMsg)
{
    std::vector<CPLString> *paosErrors =
        static_cast<std::vector<CPLString> *>(CPLGetErrorHandlerUserData());
    paosErrors->push_back(pszMsg);
}

/************************************************************************/
/*                           ParseNamespaces()                          */
/************************************************************************/

static void ParseNamespaces(CPLXMLNode *psContainerNode,
                            std::map<CPLString, CPLString> &oMap)
{
    CPLXMLNode *psNamespaces = CPLGetXMLNode(psContainerNode, "Namespaces");
    if (psNamespaces != nullptr)
    {
        for (CPLXMLNode *psIter = psNamespaces->psChild; psIter != nullptr;
             psIter = psIter->psNext)
        {
            if (psIter->eType == CXT_Element &&
                EQUAL(psIter->pszValue, "Namespace"))
            {
                const std::string osPrefix =
                    CPLGetXMLValue(psIter, "prefix", "");
                const std::string osURI = CPLGetXMLValue(psIter, "uri", "");
                if (!osPrefix.empty() && !osURI.empty())
                {
                    if (oMap.find(osPrefix) == oMap.end())
                    {
                        oMap[osPrefix] = osURI;
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Prefix %s was already mapped to %s. "
                                 "Attempt to map it to %s ignored",
                                 osPrefix.c_str(), oMap[osPrefix].c_str(),
                                 osURI.c_str());
                    }
                }
            }
        }
    }
}

/************************************************************************/
/*                           GetDefaultConfFile()                       */
/************************************************************************/

/* static */
std::string GMLASConfiguration::GetDefaultConfFile(bool &bUnlinkAfterUse)
{
    bUnlinkAfterUse = false;
#if !defined(USE_ONLY_EMBEDDED_RESOURCE_FILES)
    const char *pszConfigFile = CPLFindFile("gdal", szDEFAULT_CONF_FILENAME);
    if (pszConfigFile)
        return pszConfigFile;
#endif
#ifdef EMBED_RESOURCE_FILES
    static const bool bOnce [[maybe_unused]] = []()
    {
        CPLDebug("GMLAS", "Using embedded %s", szDEFAULT_CONF_FILENAME);
        return true;
    }();
    bUnlinkAfterUse = true;
    const std::string osTmpFilename =
        VSIMemGenerateHiddenFilename(szDEFAULT_CONF_FILENAME);
    VSIFCloseL(VSIFileFromMemBuffer(
        osTmpFilename.c_str(),
        const_cast<GByte *>(
            reinterpret_cast<const GByte *>(GMLASConfXMLGetFileContent())),
        static_cast<int>(strlen(GMLASConfXMLGetFileContent())),
        /* bTakeOwnership = */ false));
    return osTmpFilename;
#else
    return std::string();
#endif
}

/************************************************************************/
/*                                 Load()                               */
/************************************************************************/

bool GMLASConfiguration::Load(const char *pszFilename)
{
    // Allow configuration to be inlined
    CPLXMLNode *psRoot = STARTS_WITH(pszFilename, "<Configuration")
                             ? CPLParseXMLString(pszFilename)
                             : CPLParseXMLFile(pszFilename);
    if (psRoot == nullptr)
    {
        Finalize();
        return false;
    }
    CPLXMLTreeCloser oCloser(psRoot);
    CPL_IGNORE_RET_VAL(oCloser);

    // Validate the configuration file
    if (CPLTestBool(CPLGetConfigOption("GDAL_XML_VALIDATION", "YES")))
    {
#ifdef EMBED_RESOURCE_FILES
        std::string osTmpFilename;
        CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
#endif
#ifdef USE_ONLY_EMBEDDED_RESOURCE_FILES
        const char *pszXSD = nullptr;
#else
        const char *pszXSD = CPLFindFile("gdal", "gmlasconf.xsd");
#endif
#ifdef EMBED_RESOURCE_FILES
        if (!pszXSD)
        {
            static const bool bOnce [[maybe_unused]] = []()
            {
                CPLDebug("GMLAS", "Using embedded gmlasconf.xsd");
                return true;
            }();
            osTmpFilename = VSIMemGenerateHiddenFilename("gmlasconf.xsd");
            pszXSD = osTmpFilename.c_str();
            VSIFCloseL(VSIFileFromMemBuffer(
                osTmpFilename.c_str(),
                const_cast<GByte *>(reinterpret_cast<const GByte *>(
                    GMLASConfXSDGetFileContent())),
                static_cast<int>(strlen(GMLASConfXSDGetFileContent())),
                /* bTakeOwnership = */ false));
        }
#else
        if (pszXSD)
#endif
        {
            std::vector<CPLString> aosErrors;
            const CPLErr eErrClass = CPLGetLastErrorType();
            const CPLErrorNum nErrNum = CPLGetLastErrorNo();
            const CPLString osErrMsg = CPLGetLastErrorMsg();
            CPLPushErrorHandlerEx(GMLASConfigurationErrorHandler, &aosErrors);
            int bRet = CPLValidateXML(pszFilename, pszXSD, nullptr);
            CPLPopErrorHandler();
            if (!bRet && !aosErrors.empty() &&
                strstr(aosErrors[0].c_str(), "missing libxml2 support") ==
                    nullptr)
            {
                for (size_t i = 0; i < aosErrors.size(); i++)
                {
                    CPLError(CE_Warning, CPLE_AppDefined, "%s",
                             aosErrors[i].c_str());
                }
            }
            else
            {
                CPLErrorSetState(eErrClass, nErrNum, osErrMsg);
            }
        }

#ifdef EMBED_RESOURCE_FILES
        if (!osTmpFilename.empty())
            VSIUnlink(osTmpFilename.c_str());
#endif
    }

    m_bAllowRemoteSchemaDownload =
        CPLGetXMLBoolValue(psRoot, "=Configuration.AllowRemoteSchemaDownload",
                           ALLOW_REMOTE_SCHEMA_DOWNLOAD_DEFAULT);

    m_bAllowXSDCache = CPLGetXMLBoolValue(
        psRoot, "=Configuration.SchemaCache.enabled", ALLOW_XSD_CACHE_DEFAULT);
    if (m_bAllowXSDCache)
    {
        m_osXSDCacheDirectory =
            CPLGetXMLValue(psRoot, "=Configuration.SchemaCache.Directory", "");
    }

    m_bSchemaFullChecking = CPLGetXMLBoolValue(
        psRoot, "=Configuration.SchemaAnalysisOptions.SchemaFullChecking",
        SCHEMA_FULL_CHECKING_DEFAULT);

    m_bHandleMultipleImports = CPLGetXMLBoolValue(
        psRoot, "=Configuration.SchemaAnalysisOptions.HandleMultipleImports",
        HANDLE_MULTIPLE_IMPORTS_DEFAULT);

    m_bValidate = CPLGetXMLBoolValue(
        psRoot, "=Configuration.Validation.enabled", VALIDATE_DEFAULT);

    if (m_bValidate)
    {
        m_bFailIfValidationError =
            CPLGetXMLBoolValue(psRoot, "=Configuration.Validation.FailIfError",
                               FAIL_IF_VALIDATION_ERROR_DEFAULT);
    }

    m_bExposeMetadataLayers =
        CPLGetXMLBoolValue(psRoot, "=Configuration.ExposeMetadataLayers",
                           EXPOSE_METADATA_LAYERS_DEFAULT);

    m_bAlwaysGenerateOGRId = CPLGetXMLBoolValue(
        psRoot, "=Configuration.LayerBuildingRules.AlwaysGenerateOGRId",
        ALWAYS_GENERATE_OGR_ID_DEFAULT);

    m_bRemoveUnusedLayers = CPLGetXMLBoolValue(
        psRoot, "=Configuration.LayerBuildingRules.RemoveUnusedLayers",
        REMOVE_UNUSED_LAYERS_DEFAULT);

    m_bRemoveUnusedFields = CPLGetXMLBoolValue(
        psRoot, "=Configuration.LayerBuildingRules.RemoveUnusedFields",
        REMOVE_UNUSED_FIELDS_DEFAULT);

    m_bUseArrays = CPLGetXMLBoolValue(
        psRoot, "=Configuration.LayerBuildingRules.UseArrays",
        USE_ARRAYS_DEFAULT);
    m_bUseNullState = CPLGetXMLBoolValue(
        psRoot, "=Configuration.LayerBuildingRules.UseNullState",
        USE_NULL_STATE_DEFAULT);
    m_bIncludeGeometryXML = CPLGetXMLBoolValue(
        psRoot, "=Configuration.LayerBuildingRules.GML.IncludeGeometryXML",
        INCLUDE_GEOMETRY_XML_DEFAULT);
    m_bInstantiateGMLFeaturesOnly = CPLGetXMLBoolValue(
        psRoot,
        "=Configuration.LayerBuildingRules.GML.InstantiateGMLFeaturesOnly",
        INSTANTIATE_GML_FEATURES_ONLY_DEFAULT);
    m_nIdentifierMaxLength = atoi(CPLGetXMLValue(
        psRoot, "=Configuration.LayerBuildingRules.IdentifierMaxLength", "0"));
    m_bCaseInsensitiveIdentifier = CPLGetXMLBoolValue(
        psRoot, "=Configuration.LayerBuildingRules.CaseInsensitiveIdentifier",
        CASE_INSENSITIVE_IDENTIFIER_DEFAULT);
    m_bPGIdentifierLaundering = CPLGetXMLBoolValue(
        psRoot,
        "=Configuration.LayerBuildingRules.PostgreSQLIdentifierLaundering",
        PG_IDENTIFIER_LAUNDERING_DEFAULT);

    CPLXMLNode *psFlatteningRules = CPLGetXMLNode(
        psRoot, "=Configuration.LayerBuildingRules.FlatteningRules");
    if (psFlatteningRules)
    {
        m_nMaximumFieldsForFlattening = atoi(CPLGetXMLValue(
            psFlatteningRules, "MaximumNumberOfFields",
            CPLSPrintf("%d", MAXIMUM_FIELDS_FLATTENING_DEFAULT)));

        ParseNamespaces(psFlatteningRules, m_oMapPrefixToURIFlatteningRules);

        for (CPLXMLNode *psIter = psFlatteningRules->psChild; psIter != nullptr;
             psIter = psIter->psNext)
        {
            if (psIter->eType == CXT_Element &&
                EQUAL(psIter->pszValue, "ForceFlatteningXPath"))
            {
                m_osForcedFlattenedXPath.push_back(
                    CPLGetXMLValue(psIter, "", ""));
            }
            else if (psIter->eType == CXT_Element &&
                     EQUAL(psIter->pszValue, "DisableFlatteningXPath"))
            {
                m_osDisabledFlattenedXPath.push_back(
                    CPLGetXMLValue(psIter, "", ""));
            }
        }
    }

    const char *pszSWEProcessingActivation = CPLGetXMLValue(
        psRoot, "=Configuration.LayerBuildingRules.SWEProcessing.Activation",
        "ifSWENamespaceFoundInTopElement");
    if (EQUAL(pszSWEProcessingActivation, "ifSWENamespaceFoundInTopElement"))
        m_eSWEActivationMode = SWE_ACTIVATE_IF_NAMESPACE_FOUND;
    else if (CPLTestBool(pszSWEProcessingActivation))
        m_eSWEActivationMode = SWE_ACTIVATE_TRUE;
    else
        m_eSWEActivationMode = SWE_ACTIVATE_FALSE;
    m_bSWEProcessDataRecord = CPLTestBool(CPLGetXMLValue(
        psRoot,
        "=Configuration.LayerBuildingRules.SWEProcessing.ProcessDataRecord",
        "true"));
    m_bSWEProcessDataArray = CPLTestBool(CPLGetXMLValue(
        psRoot,
        "=Configuration.LayerBuildingRules.SWEProcessing.ProcessDataArray",
        "true"));

    CPLXMLNode *psTypingConstraints =
        CPLGetXMLNode(psRoot, "=Configuration.TypingConstraints");
    if (psTypingConstraints)
    {
        ParseNamespaces(psTypingConstraints, m_oMapPrefixToURITypeConstraints);

        for (CPLXMLNode *psIter = psTypingConstraints->psChild;
             psIter != nullptr; psIter = psIter->psNext)
        {
            if (psIter->eType == CXT_Element &&
                EQUAL(psIter->pszValue, "ChildConstraint"))
            {
                const CPLString &osXPath(
                    CPLGetXMLValue(psIter, "ContainerXPath", ""));
                CPLXMLNode *psChildrenTypes =
                    CPLGetXMLNode(psIter, "ChildrenElements");
                if (IsValidXPath(osXPath))
                {
                    for (CPLXMLNode *psIter2 = psChildrenTypes
                                                   ? psChildrenTypes->psChild
                                                   : nullptr;
                         psIter2 != nullptr; psIter2 = psIter2->psNext)
                    {
                        if (psIter2->eType == CXT_Element &&
                            EQUAL(psIter2->pszValue, "Element"))
                        {
                            m_oMapChildrenElementsConstraints[osXPath]
                                .push_back(CPLGetXMLValue(psIter2, "", ""));
                        }
                    }
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "XPath syntax %s not supported", osXPath.c_str());
                }
            }
        }
    }

    CPLXMLNode *psIgnoredXPaths =
        CPLGetXMLNode(psRoot, "=Configuration.IgnoredXPaths");
    if (psIgnoredXPaths)
    {
        const bool bGlobalWarnIfIgnoredXPathFound = CPLGetXMLBoolValue(
            psIgnoredXPaths, "WarnIfIgnoredXPathFoundInDocInstance",
            WARN_IF_EXCLUDED_XPATH_FOUND_DEFAULT);

        ParseNamespaces(psIgnoredXPaths, m_oMapPrefixToURIIgnoredXPaths);

        for (CPLXMLNode *psIter = psIgnoredXPaths->psChild; psIter != nullptr;
             psIter = psIter->psNext)
        {
            if (psIter->eType == CXT_Element &&
                EQUAL(psIter->pszValue, "XPath"))
            {
                const CPLString &osXPath(CPLGetXMLValue(psIter, "", ""));
                if (IsValidXPath(osXPath))
                {
                    m_aosIgnoredXPaths.push_back(osXPath);

                    const bool bWarnIfIgnoredXPathFound = CPLGetXMLBoolValue(
                        psIter, "warnIfIgnoredXPathFoundInDocInstance",
                        bGlobalWarnIfIgnoredXPathFound);
                    m_oMapIgnoredXPathToWarn[osXPath] =
                        bWarnIfIgnoredXPathFound;
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "XPath syntax %s not supported", osXPath.c_str());
                }
            }
        }
    }

    CPLXMLNode *psXLinkResolutionNode =
        CPLGetXMLNode(psRoot, "=Configuration.XLinkResolution");
    if (psXLinkResolutionNode != nullptr)
        m_oXLinkResolution.LoadFromXML(psXLinkResolutionNode);

    // Parse WriterConfig
    CPLXMLNode *psWriterConfig =
        CPLGetXMLNode(psRoot, "=Configuration.WriterConfig");
    if (psWriterConfig != nullptr)
    {
        m_nIndentSize =
            atoi(CPLGetXMLValue(psWriterConfig, "IndentationSize",
                                CPLSPrintf("%d", INDENT_SIZE_DEFAULT)));
        m_nIndentSize =
            std::min(INDENT_SIZE_MAX, std::max(INDENT_SIZE_MIN, m_nIndentSize));

        m_osComment = CPLGetXMLValue(psWriterConfig, "Comment", "");

        m_osLineFormat = CPLGetXMLValue(psWriterConfig, "LineFormat", "");

        m_osSRSNameFormat = CPLGetXMLValue(psWriterConfig, "SRSNameFormat", "");

        m_osWrapping = CPLGetXMLValue(psWriterConfig, "Wrapping",
                                      szWFS2_FEATURECOLLECTION);

        m_osTimestamp = CPLGetXMLValue(psWriterConfig, "Timestamp", "");

        m_osWFS20SchemaLocation = CPLGetXMLValue(
            psWriterConfig, "WFS20SchemaLocation", szWFS20_SCHEMALOCATION);
    }

    Finalize();

    return true;
}

/************************************************************************/
/*                               LoadFromXML()                          */
/************************************************************************/

bool GMLASXLinkResolutionConf::LoadFromXML(CPLXMLNode *psRoot)
{
    m_nTimeOut = atoi(CPLGetXMLValue(psRoot, "Timeout", "0"));

    m_nMaxFileSize = atoi(CPLGetXMLValue(
        psRoot, "MaxFileSize", CPLSPrintf("%d", MAX_FILE_SIZE_DEFAULT)));

    m_nMaxGlobalResolutionTime =
        atoi(CPLGetXMLValue(psRoot, "MaxGlobalResolutionTime", "0"));

    m_osProxyServerPort = CPLGetXMLValue(psRoot, "ProxyServerPort", "");
    m_osProxyUserPassword = CPLGetXMLValue(psRoot, "ProxyUserPassword", "");
    m_osProxyAuth = CPLGetXMLValue(psRoot, "ProxyAuth", "");

    m_osCacheDirectory = CPLGetXMLValue(psRoot, "CacheDirectory", "");
    if (m_osCacheDirectory.empty())
    {
        m_osCacheDirectory = GDALGetCacheDirectory();
        if (!m_osCacheDirectory.empty())
        {
            m_osCacheDirectory = CPLFormFilenameSafe(
                m_osCacheDirectory, "xlink_resolved_cache", nullptr);
        }
    }

    m_bDefaultResolutionEnabled =
        CPLGetXMLBoolValue(psRoot, "DefaultResolution.enabled",
                           DEFAULT_RESOLUTION_ENABLED_DEFAULT);

    m_bDefaultAllowRemoteDownload =
        CPLGetXMLBoolValue(psRoot, "DefaultResolution.AllowRemoteDownload",
                           ALLOW_REMOTE_DOWNLOAD_DEFAULT);

    // TODO when we support other modes
    // m_eDefaultResolutionMode =

    m_nDefaultResolutionDepth =
        atoi(CPLGetXMLValue(psRoot, "DefaultResolution.ResolutionDepth", "1"));

    m_bDefaultCacheResults = CPLGetXMLBoolValue(
        psRoot, "DefaultResolution.CacheResults", CACHE_RESULTS_DEFAULT);

    CPLXMLNode *psIterURL = psRoot->psChild;
    for (; psIterURL != nullptr; psIterURL = psIterURL->psNext)
    {
        if (psIterURL->eType == CXT_Element &&
            strcmp(psIterURL->pszValue, "URLSpecificResolution") == 0)
        {
            GMLASXLinkResolutionConf::URLSpecificResolution oItem;
            oItem.m_osURLPrefix = CPLGetXMLValue(psIterURL, "URLPrefix", "");

            oItem.m_bAllowRemoteDownload =
                CPLGetXMLBoolValue(psIterURL, "AllowRemoteDownload",
                                   ALLOW_REMOTE_DOWNLOAD_DEFAULT);

            const char *pszResolutionModel =
                CPLGetXMLValue(psIterURL, "ResolutionMode", "RawContent");
            if (EQUAL(pszResolutionModel, "RawContent"))
                oItem.m_eResolutionMode = RawContent;
            else
                oItem.m_eResolutionMode = FieldsFromXPath;

            oItem.m_nResolutionDepth =
                atoi(CPLGetXMLValue(psIterURL, "ResolutionDepth", "1"));

            oItem.m_bCacheResults = CPLGetXMLBoolValue(
                psIterURL, "CacheResults", CACHE_RESULTS_DEFAULT);

            CPLXMLNode *psIter = psIterURL->psChild;
            for (; psIter != nullptr; psIter = psIter->psNext)
            {
                if (psIter->eType == CXT_Element &&
                    strcmp(psIter->pszValue, "HTTPHeader") == 0)
                {
                    CPLString osName(CPLGetXMLValue(psIter, "Name", ""));
                    CPLString osValue(CPLGetXMLValue(psIter, "Value", ""));
                    oItem.m_aosNameValueHTTPHeaders.push_back(
                        std::pair<CPLString, CPLString>(osName, osValue));
                }
                else if (psIter->eType == CXT_Element &&
                         strcmp(psIter->pszValue, "Field") == 0)
                {
                    URLSpecificResolution::XPathDerivedField oField;
                    oField.m_osName = CPLGetXMLValue(psIter, "Name", "");
                    oField.m_osType = CPLGetXMLValue(psIter, "Type", "");
                    oField.m_osXPath = CPLGetXMLValue(psIter, "XPath", "");
                    oItem.m_aoFields.push_back(oField);
                }
            }

            m_aoURLSpecificRules.push_back(oItem);
        }
    }

    m_bResolveInternalXLinks = CPLGetXMLBoolValue(
        psRoot, "ResolveInternalXLinks", INTERNAL_XLINK_RESOLUTION_DEFAULT);

    return true;
}
