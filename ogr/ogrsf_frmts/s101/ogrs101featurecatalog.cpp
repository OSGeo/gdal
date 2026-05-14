/******************************************************************************
 *
 * Project:  S-101 driver
 * Purpose:  Implements OGRS101FeatureCatalog
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_s101.h"
#include "ogrs101featurecatalog.h"
#include "cpl_http.h"
#include "cpl_minixml.h"
#include "cpl_multiproc.h"

#include <algorithm>
#include <cinttypes>

#include <mutex>

static OGRS101FeatureCatalog::LoadingStatus geStatus =
    OGRS101FeatureCatalog::LoadingStatus::UNINIT;
static OGRS101FeatureCatalog *gpoFeatureCatalog = nullptr;

// Matches S-101 2.0 spec
constexpr const char *FEATURE_CATALOG_URL =
    "https://raw.githubusercontent.com/iho-ohi/S-101-Documentation-and-FC/"
    "fbcf8aaa72331a33668aa554e702e2a28f27cbc0/S-101FC/FeatureCatalogue.xml";
constexpr const char *FEATURE_CATALOG_FILENAME =
    "101_Feature_Catalogue_2.0.0.xml";
constexpr int FEATURE_CATALOG_EXPECTED_FILE_SIZE = 2017402;

/************************************************************************/
/*                              GetMutex()                              */
/************************************************************************/

static std::mutex &GetMutex()
{
    static std::mutex goMutex;
    return goMutex;
}

/************************************************************************/
/*                     GetSingletonFeatureCatalog()                     */
/************************************************************************/

/** Get the global feature catalog instance */

/* static */
std::pair<OGRS101FeatureCatalog::LoadingStatus, const OGRS101FeatureCatalog *>
OGRS101FeatureCatalog::GetSingletonFeatureCatalog(bool bStrict)
{
    std::lock_guard lock(GetMutex());
    if (geStatus == LoadingStatus::UNINIT)
    {
        auto poFeatureCatalog =
            std::make_unique<OGRS101FeatureCatalog>(bStrict);
        geStatus = poFeatureCatalog->Load();
        if (geStatus == LoadingStatus::OK)
            gpoFeatureCatalog = poFeatureCatalog.release();
    }
    return {geStatus, gpoFeatureCatalog};
}

/************************************************************************/
/*                   CleanupSingletonFeatureCatalog()                   */
/************************************************************************/

/** Clean the global feature catalog instance */

/* static */
void OGRS101FeatureCatalog::CleanupSingletonFeatureCatalog()
{
    std::lock_guard lock(GetMutex());
    geStatus = LoadingStatus::UNINIT;
    // Delete global object
    CPL_IGNORE_RET_VAL(
        std::unique_ptr<OGRS101FeatureCatalog>(gpoFeatureCatalog));
    gpoFeatureCatalog = nullptr;
}

/************************************************************************/
/*                       OGRS101FeatureCatalog()                        */
/************************************************************************/

/** Constructor */
OGRS101FeatureCatalog::OGRS101FeatureCatalog(bool bStrict) : m_bStrict(bStrict)
{
}

/************************************************************************/
/*                         EmitErrorOrWarning()                         */
/************************************************************************/

/*static*/ bool OGRS101FeatureCatalog::EmitErrorOrWarning(
    const char *pszFile, const char *pszFunc, int nLine, const char *pszMsg,
    bool bError, bool bRecoverable)
{
    return OGRS101Reader::EmitErrorOrWarning(pszFile, pszFunc, nLine, pszMsg,
                                             bError, bRecoverable);
}

/************************************************************************/
/*                                Load()                                */
/************************************************************************/

/** Load the feature catalog XML file. */
OGRS101FeatureCatalog::LoadingStatus OGRS101FeatureCatalog::Load()
{
    bool bError = false;
    const std::string osFilename = GetFilename(bError);
    if (bError)
        return LoadingStatus::ERROR;
    if (osFilename.empty())
        return LoadingStatus::SKIPPED;

    CPLDebugOnce("S101", "Reading feature catalog from %s", osFilename.c_str());
    CPLXMLTreeCloser oTree(CPLParseXMLFile(osFilename.c_str()));
    if (!oTree)
    {
        return m_bStrict ? LoadingStatus::ERROR : LoadingStatus::SKIPPED;
    }
    if (!CPLGetXMLNode(oTree.get(), "=S100FC:S100_FC_FeatureCatalogue"))
    {
        return EMIT_ERROR_OR_WARNING(CPLSPrintf(
                   "Cannot find S100FC:S100_FC_FeatureCatalogue in %s",
                   osFilename.c_str()))
                   ? LoadingStatus::SKIPPED
                   : LoadingStatus::ERROR;
    }
    CPLStripXMLNamespace(oTree.get(), "S100FC", /* bRecurse = */ true);
    const auto psRoot = CPLGetXMLNode(oTree.get(), "=S100_FC_FeatureCatalogue");
    CPLAssert(psRoot);

    return LoadSimpleAttributes(osFilename.c_str(), psRoot) &&
                   LoadComplexAttributes(osFilename.c_str(), psRoot) &&
                   LoadInformationTypes(osFilename.c_str(), psRoot) &&
                   LoadFeatureTypes(osFilename.c_str(), psRoot)
               ? LoadingStatus::OK
           : m_bStrict ? LoadingStatus::ERROR
                       : LoadingStatus::SKIPPED;
}

/************************************************************************/
/*                            GetFilename()                             */
/************************************************************************/

/** Get XML feature catalog filename. */
std::string OGRS101FeatureCatalog::GetFilename(bool &bError) const
{
    bError = false;

    // First try path given by GDAL_S101_FEATURE_CATALOG config option
    if (const char *pszFC =
            CPLGetConfigOption("GDAL_S101_FEATURE_CATALOG", nullptr))
    {
        if (EQUAL(pszFC, "") || EQUAL(pszFC, "NO") || EQUAL(pszFC, "FALSE") ||
            EQUAL(pszFC, "OFF") || EQUAL(pszFC, "0"))
            return {};

        VSIStatBufL sStat;
        if (VSIStatL(pszFC, &sStat) != 0)
        {
            bError = !EMIT_ERROR_OR_WARNING(
                CPLSPrintf("Feature catalog %s cannot be found", pszFC));
            return {};
        }
        return pszFC;
    }

    // Then try file cached in ~/.gdal directory
    const std::string osCacheDir = GDALGetCacheDirectory();
    if (!osCacheDir.empty())
    {
        std::string osTmpFilename = CPLFormFilenameSafe(
            osCacheDir.c_str(), FEATURE_CATALOG_FILENAME, nullptr);
        VSIStatBufL sStat;
        if (VSIStatL(osTmpFilename.c_str(), &sStat) == 0)
        {
            return osTmpFilename;
        }
    }

    // Then try file in ${prefix}/share/gdal/
    {
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        if (const char *pszFC = CPLFindFile("gdal", FEATURE_CATALOG_FILENAME))
        {
            return pszFC;
        }
    }

    // Finally try do download file from the web
    if (!osCacheDir.empty() && CPLHTTPEnabled())
    {
        // Try to download catalog from FEATURE_CATALOG_URL only
        // once per process
        static const std::string osStaticString = [&osCacheDir]()
        {
            std::string osFilenameLocal;
            CPLHTTPResult *psResult;
            {
                CPLDebug("S101", "Downloading feature catalog from %s",
                         FEATURE_CATALOG_URL);
                CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
                const char *const apszOptions[] = {"TIMEOUT=1", nullptr};
                psResult = CPLHTTPFetch(FEATURE_CATALOG_URL, apszOptions);
            }
            if (psResult)
            {
                if (psResult->nStatus == 0 &&
                    psResult->nDataLen == FEATURE_CATALOG_EXPECTED_FILE_SIZE)
                {
                    VSIStatBufL sStat;
                    if (VSIStatL(osCacheDir.c_str(), &sStat) != 0)
                        VSIMkdir(osCacheDir.c_str(), 0755);

                    std::string osFilename = CPLFormFilenameSafe(
                        osCacheDir.c_str(), FEATURE_CATALOG_FILENAME, nullptr);
                    const std::string osFilenameTmp =
                        osFilename + ".tmp" +
                        CPLSPrintf("_%" PRId64,
                                   static_cast<int64_t>(CPLGetPID()));
                    VSILFILE *f = VSIFOpenL(osFilenameTmp.c_str(), "wb");
                    if (f)
                    {
                        bool bOK = VSIFWriteL(psResult->pabyData,
                                              psResult->nDataLen, 1, f) == 1;
                        bOK = VSIFCloseL(f) == 0 && bOK;
                        bOK = bOK && VSIRename(osFilenameTmp.c_str(),
                                               osFilename.c_str()) == 0;
                        if (bOK)
                        {
                            osFilenameLocal = std::move(osFilename);
                            CPLDebug("S101",
                                     "Feature catalog successfully "
                                     "written in %s",
                                     osFilenameLocal.c_str());
                        }
                        else
                        {
                            VSIUnlink(osFilenameTmp.c_str());
                        }
                    }
                    else
                    {
                        CPLDebug("S101", "Cannot write temporary file %s",
                                 osFilenameTmp.c_str());
                    }
                }
                else
                {
                    CPLDebug(
                        "S101",
                        "Downloading of %s failed (status=%d, filesize=%d)",
                        FEATURE_CATALOG_URL, psResult->nStatus,
                        psResult->nDataLen);
                }
                CPLHTTPDestroyResult(psResult);
            }
            return osFilenameLocal;
        }();

        return osStaticString;
    }

    CPLDebugOnce("S101", "Cannot find feature catalog XML");
    return {};
}

/************************************************************************/
/*                        LoadSimpleAttributes()                        */
/************************************************************************/

bool OGRS101FeatureCatalog::LoadSimpleAttributes(const char *pszFC,
                                                 const CPLXMLNode *psRoot)
{
    const CPLXMLNode *psSimpleAttrs =
        CPLGetXMLNode(psRoot, "S100_FC_SimpleAttributes");
    if (!psSimpleAttrs)
    {
        return EMIT_ERROR_OR_WARNING(
            CPLSPrintf("Cannot find S100_FC_SimpleAttributes in %s", pszFC));
    }
    for (const CPLXMLNode *psSimpleAttr = psSimpleAttrs->psChild; psSimpleAttr;
         psSimpleAttr = psSimpleAttr->psNext)
    {
        if (psSimpleAttr->eType == CXT_Element &&
            strcmp(psSimpleAttr->pszValue, "S100_FC_SimpleAttribute") == 0)
        {
            const char *pszCode = CPLGetXMLValue(psSimpleAttr, "code", nullptr);
            const char *pszValueType =
                CPLGetXMLValue(psSimpleAttr, "valueType", nullptr);
            if (pszCode && pszValueType)
            {
                SimpleAttribute attr;
                attr.code = pszCode;
                attr.name = CPLGetXMLValue(psSimpleAttr, "name", "");
                attr.type = pszValueType;

                constexpr const char *const apszValueTypes[] = {
                    VALUE_TYPE_BOOLEAN,
                    VALUE_TYPE_ENUMERATION,
                    VALUE_TYPE_INTEGER,
                    VALUE_TYPE_REAL,
                    VALUE_TYPE_TRUNCATED_DATE,
                    VALUE_TYPE_TEXT,
                    VALUE_TYPE_TIME,
                    VALUE_TYPE_URI,
                    VALUE_TYPE_URN,
                };
                if (std::find_if(std::begin(apszValueTypes),
                                 std::end(apszValueTypes),
                                 [pszValueType](const char *x)
                                 { return strcmp(x, pszValueType) == 0; }) ==
                    std::end(apszValueTypes))
                {
                    if (!EMIT_ERROR_OR_WARNING(
                            CPLSPrintf("In %s: unknown valueType %s in "
                                       "simple attribute of code %s",
                                       pszFC, pszValueType, pszCode)))
                    {
                        return false;
                    }
                }

                if (strcmp(pszValueType, "enumeration") == 0)
                {
                    if (const CPLXMLNode *psListedValues =
                            CPLGetXMLNode(psSimpleAttr, "listedValues"))
                    {
                        for (const CPLXMLNode *psListedValue =
                                 psListedValues->psChild;
                             psListedValue;
                             psListedValue = psListedValue->psNext)
                        {
                            if (psListedValue->eType == CXT_Element &&
                                strcmp(psListedValue->pszValue,
                                       "listedValue") == 0)
                            {
                                const char *pszEnumLabel = CPLGetXMLValue(
                                    psListedValue, "label", nullptr);
                                const char *pszEnumCode = CPLGetXMLValue(
                                    psListedValue, "code", nullptr);
                                if (pszEnumCode && pszEnumLabel &&
                                    CPLGetValueType(pszEnumCode) ==
                                        CPL_VALUE_INTEGER)
                                {
                                    if (!attr.enumeratedValues
                                             .insert(
                                                 std::pair<int, std::string>(
                                                     atoi(pszEnumCode),
                                                     std::move(pszEnumLabel)))
                                             .second)
                                    {
                                        if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                                                "%s: several listedValue with "
                                                "code "
                                                "%s in %s",
                                                pszFC, pszEnumCode, pszCode)))
                                        {
                                            return false;
                                        }
                                    }
                                }
                                else if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                                             "%s: invalid listedValue in "
                                             "simple attribute %s",
                                             pszFC, pszCode)))
                                {
                                    return false;
                                }
                            }
                        }
                    }
                }
                if (!m_simpleAttributes
                         .insert(std::pair<std::string, SimpleAttribute>(
                             pszCode, std::move(attr)))
                         .second)
                {
                    if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                            "%s: several simple attributes with code %s", pszFC,
                            pszCode)))
                    {
                        return false;
                    }
                }
            }
            else if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                         "In %s: invalid S100_FC_SimpleAttribute", pszFC)))
            {
                return false;
            }
        }
    }

    return true;
}

/************************************************************************/
/*                       LoadComplexAttributes()                        */
/************************************************************************/

bool OGRS101FeatureCatalog::LoadComplexAttributes(const char *pszFC,
                                                  const CPLXMLNode *psRoot)
{
    const CPLXMLNode *psComplexAttrs =
        CPLGetXMLNode(psRoot, "S100_FC_ComplexAttributes");
    if (!psComplexAttrs)
    {
        return EMIT_ERROR_OR_WARNING(
            CPLSPrintf("Cannot find S100_FC_ComplexAttributes in %s", pszFC));
    }
    for (const CPLXMLNode *psComplexAttr = psComplexAttrs->psChild;
         psComplexAttr; psComplexAttr = psComplexAttr->psNext)
    {
        if (psComplexAttr->eType == CXT_Element &&
            strcmp(psComplexAttr->pszValue, "S100_FC_ComplexAttribute") == 0)
        {
            const char *pszCode =
                CPLGetXMLValue(psComplexAttr, "code", nullptr);
            if (pszCode)
            {
                ComplexAttribute attr;
                attr.code = pszCode;
                attr.name = CPLGetXMLValue(psComplexAttr, "name", "");

                const CPLXMLNode *psSubAttributeBinding =
                    CPLGetXMLNode(psComplexAttr, "subAttributeBinding");
                for (; psSubAttributeBinding;
                     psSubAttributeBinding = psSubAttributeBinding->psNext)
                {
                    if (psSubAttributeBinding->eType == CXT_Element &&
                        strcmp(psSubAttributeBinding->pszValue,
                               "subAttributeBinding") == 0)
                    {
                        const char *pszAttributeRef = CPLGetXMLValue(
                            psSubAttributeBinding, "attribute.ref", nullptr);
                        if (pszAttributeRef)
                        {
                            attr.attributeBindings.insert(pszAttributeRef);
                        }
                    }
                }
                if (!m_complexAttributes
                         .insert(std::pair<std::string, ComplexAttribute>(
                             pszCode, std::move(attr)))
                         .second)
                {
                    if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                            "In %s: several complex attributes with code %s",
                            pszFC, pszCode)))
                    {
                        return false;
                    }
                }
            }
            else if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                         "In %s: invalid S100_FC_ComplexAttribute", pszFC)))
            {
                return false;
            }
        }
    }

    for (const auto &[code, def] : m_complexAttributes)
    {
        for (const auto &attrCode : def.attributeBindings)
        {
            if (!cpl::contains(m_simpleAttributes, attrCode) &&
                !cpl::contains(m_complexAttributes, attrCode))
            {
                if (!EMIT_ERROR_OR_WARNING(
                        CPLSPrintf("In %s: complex attribute %s refers to "
                                   "attribute %s which does not exist",
                                   pszFC, code.c_str(), attrCode.c_str())))
                {
                    return false;
                }
            }
        }
    }

    return true;
}

/************************************************************************/
/*                        LoadInformationTypes()                        */
/************************************************************************/

bool OGRS101FeatureCatalog::LoadInformationTypes(const char *pszFC,
                                                 const CPLXMLNode *psRoot)
{
    const CPLXMLNode *psInformationTypes =
        CPLGetXMLNode(psRoot, "S100_FC_InformationTypes");
    if (!psInformationTypes)
    {
        return EMIT_ERROR_OR_WARNING(
            CPLSPrintf("Cannot find S100_FC_InformationTypes in %s", pszFC));
    }
    for (const CPLXMLNode *psInformationType = psInformationTypes->psChild;
         psInformationType; psInformationType = psInformationType->psNext)
    {
        if (psInformationType->eType == CXT_Element &&
            strcmp(psInformationType->pszValue, "S100_FC_InformationType") == 0)
        {
            const char *pszCode =
                CPLGetXMLValue(psInformationType, "code", nullptr);
            if (pszCode)
            {
                InformationType featureType;
                featureType.code = pszCode;
                featureType.name =
                    CPLGetXMLValue(psInformationType, "name", "");
                featureType.definition =
                    CPLGetXMLValue(psInformationType, "definition", "");
                featureType.alias =
                    CPLGetXMLValue(psInformationType, "alias", "");

                const CPLXMLNode *psAttributeBinding =
                    CPLGetXMLNode(psInformationType, "attributeBinding");
                for (; psAttributeBinding;
                     psAttributeBinding = psAttributeBinding->psNext)
                {
                    if (psAttributeBinding->eType == CXT_Element &&
                        strcmp(psAttributeBinding->pszValue,
                               "attributeBinding") == 0)
                    {
                        const char *pszAttributeRef = CPLGetXMLValue(
                            psAttributeBinding, "attribute.ref", nullptr);
                        if (pszAttributeRef)
                        {
                            if (!cpl::contains(m_simpleAttributes,
                                               pszAttributeRef) &&
                                !cpl::contains(m_complexAttributes,
                                               pszAttributeRef))
                            {
                                if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                                        "In %s: information type %s refers to "
                                        "attribute %s which does not exist",
                                        pszFC, pszCode, pszAttributeRef)))
                                {
                                    return false;
                                }
                            }

                            featureType.attributeBindings.insert(
                                pszAttributeRef);
                        }
                    }
                }

                if (!m_informationTypes
                         .insert(std::pair<std::string, InformationType>(
                             pszCode, std::move(featureType)))
                         .second)
                {
                    if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                            "%s: several information types with code %s", pszFC,
                            pszCode)))
                    {
                        return false;
                    }
                }
            }
            else if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                         "%s: invalid S100_FC_InformationType", pszFC)))
            {
                return false;
            }
        }
    }

    return true;
}

/************************************************************************/
/*                          LoadFeatureTypes()                          */
/************************************************************************/

bool OGRS101FeatureCatalog::LoadFeatureTypes(const char *pszFC,
                                             const CPLXMLNode *psRoot)
{
    const CPLXMLNode *psFeatureTypes =
        CPLGetXMLNode(psRoot, "S100_FC_FeatureTypes");
    if (!psFeatureTypes)
    {
        return EMIT_ERROR_OR_WARNING(
            CPLSPrintf("Cannot find S100_FC_FeatureTypes in %s", pszFC));
    }
    for (const CPLXMLNode *psFeatureType = psFeatureTypes->psChild;
         psFeatureType; psFeatureType = psFeatureType->psNext)
    {
        if (psFeatureType->eType == CXT_Element &&
            strcmp(psFeatureType->pszValue, "S100_FC_FeatureType") == 0)
        {
            const char *pszCode =
                CPLGetXMLValue(psFeatureType, "code", nullptr);
            if (pszCode)
            {
                FeatureType featureType;
                featureType.code = pszCode;
                featureType.name = CPLGetXMLValue(psFeatureType, "name", "");
                featureType.definition =
                    CPLGetXMLValue(psFeatureType, "definition", "");
                featureType.alias = CPLGetXMLValue(psFeatureType, "alias", "");

                const CPLXMLNode *psAttributeBinding =
                    CPLGetXMLNode(psFeatureType, "attributeBinding");
                for (; psAttributeBinding;
                     psAttributeBinding = psAttributeBinding->psNext)
                {
                    if (psAttributeBinding->eType == CXT_Element &&
                        strcmp(psAttributeBinding->pszValue,
                               "attributeBinding") == 0)
                    {
                        const char *pszAttributeRef = CPLGetXMLValue(
                            psAttributeBinding, "attribute.ref", nullptr);
                        if (pszAttributeRef)
                        {
                            if (!cpl::contains(m_simpleAttributes,
                                               pszAttributeRef) &&
                                !cpl::contains(m_complexAttributes,
                                               pszAttributeRef))
                            {
                                if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                                        "In %s: feature type %s refers to "
                                        "attribute %s which does not exist",
                                        pszFC, pszCode, pszAttributeRef)))
                                {
                                    return false;
                                }
                            }

                            featureType.attributeBindings.insert(
                                pszAttributeRef);
                        }
                    }
                }

                const CPLXMLNode *psPermittedPrimitives =
                    CPLGetXMLNode(psFeatureType, "permittedPrimitives");
                for (; psPermittedPrimitives;
                     psPermittedPrimitives = psPermittedPrimitives->psNext)
                {
                    if (psPermittedPrimitives->eType == CXT_Element &&
                        strcmp(psPermittedPrimitives->pszValue,
                               "permittedPrimitives") == 0 &&
                        psPermittedPrimitives->psChild &&
                        psPermittedPrimitives->psChild->eType == CXT_Text &&
                        psPermittedPrimitives->psChild->pszValue)
                    {
                        const char *pszPermittedPrimitive =
                            psPermittedPrimitives->psChild->pszValue;
                        constexpr const char *const apszPermittedPrimitives[] =
                            {
                                PERMITTED_PRIMITIVE_NO_GEOMETRY,
                                PERMITTED_PRIMITIVE_POINT,
                                PERMITTED_PRIMITIVE_POINTSET,
                                PERMITTED_PRIMITIVE_CURVE,
                                PERMITTED_PRIMITIVE_SURFACE,
                            };
                        if (std::find_if(
                                std::begin(apszPermittedPrimitives),
                                std::end(apszPermittedPrimitives),
                                [pszPermittedPrimitive](const char *x)
                                {
                                    return strcmp(x, pszPermittedPrimitive) ==
                                           0;
                                }) != std::end(apszPermittedPrimitives))
                        {
                            featureType.permittedPrimitives.insert(
                                pszPermittedPrimitive);
                        }
                        else if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                                     "In %s: unknown permittedPrimitives %s in "
                                     "feature type with code %s",
                                     pszFC, pszPermittedPrimitive, pszCode)))
                        {
                            return false;
                        }
                    }
                }

                if (!m_featureTypes
                         .insert(std::pair<std::string, FeatureType>(
                             pszCode, std::move(featureType)))
                         .second)
                {
                    if (!EMIT_ERROR_OR_WARNING(
                            CPLSPrintf("%s: several feature types with code %s",
                                       pszFC, pszCode)))
                    {
                        return false;
                    }
                }
            }
            else if (!EMIT_ERROR_OR_WARNING(
                         CPLSPrintf("%s: invalid S100_FC_FeatureType", pszFC)))
            {
                return false;
            }
        }
    }

    return true;
}
