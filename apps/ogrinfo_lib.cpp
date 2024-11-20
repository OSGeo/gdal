/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Simple client for viewing OGR driver data.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "cpl_json.h"
#include "ogrlibjsonutils.h"
#include "cpl_string.h"
#include "gdal_utils.h"
#include "gdal_utils_priv.h"
#include "gdal_priv.h"
#include "ogr_feature.h"
#include "ogrsf_frmts.h"
#include "ogr_geometry.h"
#include "commonutils.h"
#include "gdalargumentparser.h"

#include <cmath>
#include <set>

/*! output format */
typedef enum
{
    /*! output in text format */ FORMAT_TEXT = 0,
    /*! output in json format */ FORMAT_JSON = 1
} GDALVectorInfoFormat;

struct GDALVectorInfoOptions
{
    GDALVectorInfoFormat eFormat = FORMAT_TEXT;
    std::string osWHERE{};
    CPLStringList aosLayers{};
    std::unique_ptr<OGRGeometry> poSpatialFilter{};
    bool bAllLayers = false;
    std::string osSQLStatement{};
    std::string osDialect{};
    std::string osGeomField{};
    CPLStringList aosExtraMDDomains{};
    bool bListMDD = false;
    bool bShowMetadata = true;
    bool bFeatureCount = true;
    bool bExtent = true;
    bool bExtent3D = false;
    bool bGeomType = true;
    bool bDatasetGetNextFeature = false;
    bool bVerbose = true;
    bool bSuperQuiet = false;
    bool bSummaryOnly = false;
    GIntBig nFetchFID = OGRNullFID;
    std::string osWKTFormat = "WKT2";
    std::string osFieldDomain{};
    CPLStringList aosOptions{};
    bool bStdoutOutput = false;  // only set by ogrinfo_bin
    int nRepeatCount = 1;

    /*! Maximum number of features, or -1 if no limit. */
    GIntBig nLimit = -1;

    // Only used during argument parsing
    bool bSummaryParser = false;
    bool bFeaturesParser = false;
};

/************************************************************************/
/*                     GDALVectorInfoOptionsFree()                      */
/************************************************************************/

/**
 * Frees the GDALVectorInfoOptions struct.
 *
 * @param psOptions the options struct for GDALVectorInfo().
 *
 * @since GDAL 3.7
 */

void GDALVectorInfoOptionsFree(GDALVectorInfoOptions *psOptions)
{
    delete psOptions;
}

/************************************************************************/
/*                            Concat()                                  */
/************************************************************************/

#ifndef Concat_defined
#define Concat_defined
static void Concat(CPLString &osRet, bool bStdoutOutput, const char *pszFormat,
                   ...) CPL_PRINT_FUNC_FORMAT(3, 4);

static void Concat(CPLString &osRet, bool bStdoutOutput, const char *pszFormat,
                   ...)
{
    va_list args;
    va_start(args, pszFormat);

    if (bStdoutOutput)
    {
        vfprintf(stdout, pszFormat, args);
    }
    else
    {
        try
        {
            CPLString osTarget;
            osTarget.vPrintf(pszFormat, args);

            osRet += osTarget;
        }
        catch (const std::bad_alloc &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
        }
    }

    va_end(args);
}
#endif

static void ConcatStr(CPLString &osRet, bool bStdoutOutput, const char *pszStr)
{
    if (bStdoutOutput)
        fwrite(pszStr, 1, strlen(pszStr), stdout);
    else
        osRet += pszStr;
}

/************************************************************************/
/*                        ReportFieldDomain()                           */
/************************************************************************/

static void ReportFieldDomain(CPLString &osRet, CPLJSONObject &oDomains,
                              const GDALVectorInfoOptions *psOptions,
                              const OGRFieldDomain *poDomain)
{
    const bool bJson = psOptions->eFormat == FORMAT_JSON;
    CPLJSONObject oDomain;
    oDomains.Add(poDomain->GetName(), oDomain);
    Concat(osRet, psOptions->bStdoutOutput, "Domain %s:\n",
           poDomain->GetName().c_str());
    const std::string &osDesc = poDomain->GetDescription();
    if (!osDesc.empty())
    {
        if (bJson)
            oDomain.Set("description", osDesc);
        else
            Concat(osRet, psOptions->bStdoutOutput, "  Description: %s\n",
                   osDesc.c_str());
    }
    const char *pszType = "";
    switch (poDomain->GetDomainType())
    {
        case OFDT_CODED:
            pszType = "coded";
            break;
        case OFDT_RANGE:
            pszType = "range";
            break;
        case OFDT_GLOB:
            pszType = "glob";
            break;
    }
    if (bJson)
    {
        oDomain.Set("type", pszType);
    }
    else
    {
        Concat(osRet, psOptions->bStdoutOutput, "  Type: %s\n", pszType);
    }
    const char *pszFieldType =
        OGRFieldDefn::GetFieldTypeName(poDomain->GetFieldType());
    const char *pszFieldSubType =
        OGRFieldDefn::GetFieldSubTypeName(poDomain->GetFieldSubType());
    if (bJson)
    {
        oDomain.Set("fieldType", pszFieldType);
        if (poDomain->GetFieldSubType() != OFSTNone)
            oDomain.Set("fieldSubType", pszFieldSubType);
    }
    else
    {
        const char *pszFieldTypeDisplay =
            (poDomain->GetFieldSubType() != OFSTNone)
                ? CPLSPrintf("%s(%s)", pszFieldType, pszFieldSubType)
                : pszFieldType;
        Concat(osRet, psOptions->bStdoutOutput, "  Field type: %s\n",
               pszFieldTypeDisplay);
    }

    const char *pszSplitPolicy = "";
    switch (poDomain->GetSplitPolicy())
    {
        case OFDSP_DEFAULT_VALUE:
            pszSplitPolicy = "default value";
            break;
        case OFDSP_DUPLICATE:
            pszSplitPolicy = "duplicate";
            break;
        case OFDSP_GEOMETRY_RATIO:
            pszSplitPolicy = "geometry ratio";
            break;
    }
    if (bJson)
    {
        oDomain.Set("splitPolicy", pszSplitPolicy);
    }
    else
    {
        Concat(osRet, psOptions->bStdoutOutput, "  Split policy: %s\n",
               pszSplitPolicy);
    }

    const char *pszMergePolicy = "";
    switch (poDomain->GetMergePolicy())
    {
        case OFDMP_DEFAULT_VALUE:
            pszMergePolicy = "default value";
            break;
        case OFDMP_SUM:
            pszMergePolicy = "sum";
            break;
        case OFDMP_GEOMETRY_WEIGHTED:
            pszMergePolicy = "geometry weighted";
            break;
    }
    if (bJson)
    {
        oDomain.Set("mergePolicy", pszMergePolicy);
    }
    else
    {
        Concat(osRet, psOptions->bStdoutOutput, "  Merge policy: %s\n",
               pszMergePolicy);
    }

    switch (poDomain->GetDomainType())
    {
        case OFDT_CODED:
        {
            const auto poCodedFieldDomain =
                cpl::down_cast<const OGRCodedFieldDomain *>(poDomain);
            const OGRCodedValue *enumeration =
                poCodedFieldDomain->GetEnumeration();
            if (!bJson)
                Concat(osRet, psOptions->bStdoutOutput, "  Coded values:\n");
            CPLJSONObject oCodedValues;
            oDomain.Add("codedValues", oCodedValues);
            for (int i = 0; enumeration[i].pszCode != nullptr; ++i)
            {
                if (enumeration[i].pszValue)
                {
                    if (bJson)
                    {
                        oCodedValues.Set(enumeration[i].pszCode,
                                         enumeration[i].pszValue);
                    }
                    else
                    {
                        Concat(osRet, psOptions->bStdoutOutput, "    %s: %s\n",
                               enumeration[i].pszCode, enumeration[i].pszValue);
                    }
                }
                else
                {
                    if (bJson)
                    {
                        oCodedValues.SetNull(enumeration[i].pszCode);
                    }
                    else
                    {
                        Concat(osRet, psOptions->bStdoutOutput, "    %s\n",
                               enumeration[i].pszCode);
                    }
                }
            }
            break;
        }

        case OFDT_RANGE:
        {
            const auto poRangeFieldDomain =
                cpl::down_cast<const OGRRangeFieldDomain *>(poDomain);
            bool bMinIsIncluded = false;
            const OGRField &sMin = poRangeFieldDomain->GetMin(bMinIsIncluded);
            bool bMaxIsIncluded = false;
            const OGRField &sMax = poRangeFieldDomain->GetMax(bMaxIsIncluded);
            if (poDomain->GetFieldType() == OFTInteger)
            {
                if (!OGR_RawField_IsUnset(&sMin))
                {
                    if (bJson)
                    {
                        oDomain.Set("minValue", sMin.Integer);
                        oDomain.Set("minValueIncluded", bMinIsIncluded);
                    }
                    else
                    {
                        Concat(osRet, psOptions->bStdoutOutput,
                               "  Minimum value: %d%s\n", sMin.Integer,
                               bMinIsIncluded ? "" : " (excluded)");
                    }
                }
                if (!OGR_RawField_IsUnset(&sMax))
                {
                    if (bJson)
                    {
                        oDomain.Set("maxValue", sMax.Integer);
                        oDomain.Set("maxValueIncluded", bMaxIsIncluded);
                    }
                    else
                    {
                        Concat(osRet, psOptions->bStdoutOutput,
                               "  Maximum value: %d%s\n", sMax.Integer,
                               bMaxIsIncluded ? "" : " (excluded)");
                    }
                }
            }
            else if (poDomain->GetFieldType() == OFTInteger64)
            {
                if (!OGR_RawField_IsUnset(&sMin))
                {
                    if (bJson)
                    {
                        oDomain.Set("minValue", sMin.Integer64);
                        oDomain.Set("minValueIncluded", bMinIsIncluded);
                    }
                    else
                    {
                        Concat(osRet, psOptions->bStdoutOutput,
                               "  Minimum value: " CPL_FRMT_GIB "%s\n",
                               sMin.Integer64,
                               bMinIsIncluded ? "" : " (excluded)");
                    }
                }
                if (!OGR_RawField_IsUnset(&sMax))
                {
                    if (bJson)
                    {
                        oDomain.Set("maxValue", sMax.Integer64);
                        oDomain.Set("maxValueIncluded", bMaxIsIncluded);
                    }
                    else
                    {
                        Concat(osRet, psOptions->bStdoutOutput,
                               "  Maximum value: " CPL_FRMT_GIB "%s\n",
                               sMax.Integer64,
                               bMaxIsIncluded ? "" : " (excluded)");
                    }
                }
            }
            else if (poDomain->GetFieldType() == OFTReal)
            {
                if (!OGR_RawField_IsUnset(&sMin))
                {
                    if (bJson)
                    {
                        oDomain.Set("minValue", sMin.Real);
                        oDomain.Set("minValueIncluded", bMinIsIncluded);
                    }
                    else
                    {
                        Concat(osRet, psOptions->bStdoutOutput,
                               "  Minimum value: %g%s\n", sMin.Real,
                               bMinIsIncluded ? "" : " (excluded)");
                    }
                }
                if (!OGR_RawField_IsUnset(&sMax))
                {
                    if (bJson)
                    {
                        oDomain.Set("maxValue", sMax.Real);
                        oDomain.Set("maxValueIncluded", bMaxIsIncluded);
                    }
                    else
                    {
                        Concat(osRet, psOptions->bStdoutOutput,
                               "  Maximum value: %g%s\n", sMax.Real,
                               bMaxIsIncluded ? "" : " (excluded)");
                    }
                }
            }
            else if (poDomain->GetFieldType() == OFTDateTime)
            {
                if (!OGR_RawField_IsUnset(&sMin))
                {
                    const char *pszVal = CPLSPrintf(
                        "%04d-%02d-%02dT%02d:%02d:%02d", sMin.Date.Year,
                        sMin.Date.Month, sMin.Date.Day, sMin.Date.Hour,
                        sMin.Date.Minute,
                        static_cast<int>(sMin.Date.Second + 0.5));
                    if (bJson)
                    {
                        oDomain.Set("minValue", pszVal);
                        oDomain.Set("minValueIncluded", bMinIsIncluded);
                    }
                    else
                    {
                        Concat(osRet, psOptions->bStdoutOutput,
                               "  Minimum value: %s%s\n", pszVal,
                               bMinIsIncluded ? "" : " (excluded)");
                    }
                }
                if (!OGR_RawField_IsUnset(&sMax))
                {
                    const char *pszVal = CPLSPrintf(
                        "%04d-%02d-%02dT%02d:%02d:%02d", sMax.Date.Year,
                        sMax.Date.Month, sMax.Date.Day, sMax.Date.Hour,
                        sMax.Date.Minute,
                        static_cast<int>(sMax.Date.Second + 0.5));
                    if (bJson)
                    {
                        oDomain.Set("maxValue", pszVal);
                        oDomain.Set("maxValueIncluded", bMaxIsIncluded);
                    }
                    else
                    {
                        Concat(osRet, psOptions->bStdoutOutput,
                               "  Maximum value: %s%s\n", pszVal,
                               bMaxIsIncluded ? "" : " (excluded)");
                    }
                }
            }
            break;
        }

        case OFDT_GLOB:
        {
            const auto poGlobFieldDomain =
                cpl::down_cast<const OGRGlobFieldDomain *>(poDomain);
            if (bJson)
                oDomain.Set("glob", poGlobFieldDomain->GetGlob());
            else
                Concat(osRet, psOptions->bStdoutOutput, "  Glob: %s\n",
                       poGlobFieldDomain->GetGlob().c_str());
            break;
        }
    }
}

/************************************************************************/
/*                       ReportRelationships()                          */
/************************************************************************/

static void ReportRelationships(CPLString &osRet, CPLJSONObject &oRoot,
                                const GDALVectorInfoOptions *psOptions,
                                const GDALDataset *poDS)
{
    const bool bJson = psOptions->eFormat == FORMAT_JSON;
    CPLJSONObject oRelationships;
    if (bJson)
        oRoot.Add("relationships", oRelationships);

    const auto aosRelationshipNames = poDS->GetRelationshipNames();
    for (const std::string &osRelationshipName : aosRelationshipNames)
    {
        const auto poRelationship = poDS->GetRelationship(osRelationshipName);
        if (!poRelationship)
            continue;

        const char *pszType = "";
        switch (poRelationship->GetType())
        {
            case GRT_COMPOSITE:
                pszType = "Composite";
                break;
            case GRT_ASSOCIATION:
                pszType = "Association";
                break;
            case GRT_AGGREGATION:
                pszType = "Aggregation";
                break;
        }

        const char *pszCardinality = "";
        switch (poRelationship->GetCardinality())
        {
            case GRC_ONE_TO_ONE:
                pszCardinality = "OneToOne";
                break;
            case GRC_ONE_TO_MANY:
                pszCardinality = "OneToMany";
                break;
            case GRC_MANY_TO_ONE:
                pszCardinality = "ManyToOne";
                break;
            case GRC_MANY_TO_MANY:
                pszCardinality = "ManyToMany";
                break;
        }

        const auto &aosLeftTableFields = poRelationship->GetLeftTableFields();
        const auto &aosRightTableFields = poRelationship->GetRightTableFields();
        const auto &osMappingTableName = poRelationship->GetMappingTableName();
        const auto &aosLeftMappingTableFields =
            poRelationship->GetLeftMappingTableFields();
        const auto &aosRightMappingTableFields =
            poRelationship->GetRightMappingTableFields();

        if (bJson)
        {
            CPLJSONObject oRelationship;
            oRelationships.Add(osRelationshipName, oRelationship);

            oRelationship.Add("type", pszType);
            oRelationship.Add("related_table_type",
                              poRelationship->GetRelatedTableType());
            oRelationship.Add("cardinality", pszCardinality);
            oRelationship.Add("left_table_name",
                              poRelationship->GetLeftTableName());
            oRelationship.Add("right_table_name",
                              poRelationship->GetRightTableName());

            CPLJSONArray oLeftTableFields;
            oRelationship.Add("left_table_fields", oLeftTableFields);
            for (const auto &osName : aosLeftTableFields)
                oLeftTableFields.Add(osName);

            CPLJSONArray oRightTableFields;
            oRelationship.Add("right_table_fields", oRightTableFields);
            for (const auto &osName : aosRightTableFields)
                oRightTableFields.Add(osName);

            if (!osMappingTableName.empty())
            {
                oRelationship.Add("mapping_table_name", osMappingTableName);

                CPLJSONArray oLeftMappingTableFields;
                oRelationship.Add("left_mapping_table_fields",
                                  oLeftMappingTableFields);
                for (const auto &osName : aosLeftMappingTableFields)
                    oLeftMappingTableFields.Add(osName);

                CPLJSONArray oRightMappingTableFields;
                oRelationship.Add("right_mapping_table_fields",
                                  oRightMappingTableFields);
                for (const auto &osName : aosRightMappingTableFields)
                    oRightMappingTableFields.Add(osName);
            }

            oRelationship.Add("forward_path_label",
                              poRelationship->GetForwardPathLabel());
            oRelationship.Add("backward_path_label",
                              poRelationship->GetBackwardPathLabel());
        }
        else
        {
            const auto ConcatStringList =
                [&osRet, psOptions](const std::vector<std::string> &aosList)
            {
                bool bFirstName = true;
                for (const auto &osName : aosList)
                {
                    if (!bFirstName)
                        ConcatStr(osRet, psOptions->bStdoutOutput, ", ");
                    bFirstName = false;
                    ConcatStr(osRet, psOptions->bStdoutOutput, osName.c_str());
                }
                Concat(osRet, psOptions->bStdoutOutput, "\n");
            };

            if (!psOptions->bAllLayers)
            {
                Concat(osRet, psOptions->bStdoutOutput,
                       "Relationship: %s (%s, %s, %s)\n",
                       osRelationshipName.c_str(), pszType,
                       poRelationship->GetLeftTableName().c_str(),
                       poRelationship->GetRightTableName().c_str());
                continue;
            }
            Concat(osRet, psOptions->bStdoutOutput, "\nRelationship: %s\n",
                   osRelationshipName.c_str());
            Concat(osRet, psOptions->bStdoutOutput, "  Type: %s\n", pszType);
            Concat(osRet, psOptions->bStdoutOutput,
                   "  Related table type: %s\n",
                   poRelationship->GetRelatedTableType().c_str());
            Concat(osRet, psOptions->bStdoutOutput, "  Cardinality: %s\n",
                   pszCardinality);
            Concat(osRet, psOptions->bStdoutOutput, "  Left table name: %s\n",
                   poRelationship->GetLeftTableName().c_str());
            Concat(osRet, psOptions->bStdoutOutput, "  Right table name: %s\n",
                   poRelationship->GetRightTableName().c_str());
            Concat(osRet, psOptions->bStdoutOutput, "  Left table fields: ");
            ConcatStringList(aosLeftTableFields);
            Concat(osRet, psOptions->bStdoutOutput, "  Right table fields: ");
            ConcatStringList(aosRightTableFields);

            if (!osMappingTableName.empty())
            {
                Concat(osRet, psOptions->bStdoutOutput,
                       "  Mapping table name: %s\n",
                       osMappingTableName.c_str());

                Concat(osRet, psOptions->bStdoutOutput,
                       "  Left mapping table fields: ");
                ConcatStringList(aosLeftMappingTableFields);

                Concat(osRet, psOptions->bStdoutOutput,
                       "  Right mapping table fields: ");
                ConcatStringList(aosRightMappingTableFields);
            }

            Concat(osRet, psOptions->bStdoutOutput,
                   "  Forward path label: %s\n",
                   poRelationship->GetForwardPathLabel().c_str());
            Concat(osRet, psOptions->bStdoutOutput,
                   "  Backward path label: %s\n",
                   poRelationship->GetBackwardPathLabel().c_str());
        }
    }
}

/************************************************************************/
/*                     GDALVectorInfoPrintMetadata()                    */
/************************************************************************/

static void
GDALVectorInfoPrintMetadata(CPLString &osRet, CPLJSONObject &oMetadata,
                            const GDALVectorInfoOptions *psOptions,
                            GDALMajorObjectH hObject, const char *pszDomain,
                            const char *pszDisplayedname, const char *pszIndent)
{
    const bool bJsonOutput = psOptions->eFormat == FORMAT_JSON;
    bool bIsxml = false;
    bool bMDIsJson = false;

    if (pszDomain != nullptr && STARTS_WITH_CI(pszDomain, "xml:"))
        bIsxml = true;
    else if (pszDomain != nullptr && STARTS_WITH_CI(pszDomain, "json:"))
        bMDIsJson = true;

    CSLConstList papszMetadata = GDALGetMetadata(hObject, pszDomain);
    if (CSLCount(papszMetadata) > 0)
    {
        CPLJSONObject oMetadataDomain;
        if (!bJsonOutput)
            Concat(osRet, psOptions->bStdoutOutput, "%s%s:\n", pszIndent,
                   pszDisplayedname);
        for (int i = 0; papszMetadata[i] != nullptr; i++)
        {
            if (bJsonOutput)
            {
                if (bIsxml)
                {
                    oMetadata.Add(pszDomain, papszMetadata[i]);
                    return;
                }
                else if (bMDIsJson)
                {
                    CPLJSONDocument oDoc;
                    if (oDoc.LoadMemory(papszMetadata[i]))
                        oMetadata.Add(pszDomain, oDoc.GetRoot());
                    return;
                }
                else
                {
                    char *pszKey = nullptr;
                    const char *pszValue =
                        CPLParseNameValue(papszMetadata[i], &pszKey);
                    if (pszKey)
                    {
                        oMetadataDomain.Add(pszKey, pszValue);
                        CPLFree(pszKey);
                    }
                }
            }
            else if (bIsxml)
                Concat(osRet, psOptions->bStdoutOutput, "%s%s\n", pszIndent,
                       papszMetadata[i]);
            else
                Concat(osRet, psOptions->bStdoutOutput, "%s  %s\n", pszIndent,
                       papszMetadata[i]);
        }
        if (bJsonOutput)
        {
            oMetadata.Add(pszDomain ? pszDomain : "", oMetadataDomain);
        }
    }
}

/************************************************************************/
/*                    GDALVectorInfoReportMetadata()                    */
/************************************************************************/

static void GDALVectorInfoReportMetadata(CPLString &osRet, CPLJSONObject &oRoot,
                                         const GDALVectorInfoOptions *psOptions,
                                         GDALMajorObject *poMajorObject,
                                         bool bListMDD, bool bShowMetadata,
                                         CSLConstList papszExtraMDDomains)
{
    const char *pszIndent = "";
    auto hObject = GDALMajorObject::ToHandle(poMajorObject);

    const bool bJson = psOptions->eFormat == FORMAT_JSON;
    /* -------------------------------------------------------------------- */
    /*      Report list of Metadata domains                                 */
    /* -------------------------------------------------------------------- */
    if (bListMDD)
    {
        const CPLStringList aosMDDList(GDALGetMetadataDomainList(hObject));

        CPLJSONArray metadataDomains;

        if (!aosMDDList.empty() && !bJson)
            Concat(osRet, psOptions->bStdoutOutput, "%sMetadata domains:\n",
                   pszIndent);
        for (const char *pszDomain : aosMDDList)
        {
            if (EQUAL(pszDomain, ""))
            {
                if (bJson)
                    metadataDomains.Add("");
                else
                    Concat(osRet, psOptions->bStdoutOutput, "%s  (default)\n",
                           pszIndent);
            }
            else
            {
                if (bJson)
                    metadataDomains.Add(pszDomain);
                else
                    Concat(osRet, psOptions->bStdoutOutput, "%s  %s\n",
                           pszIndent, pszDomain);
            }
        }

        if (bJson)
            oRoot.Add("metadataDomains", metadataDomains);
    }

    if (!bShowMetadata)
        return;

    /* -------------------------------------------------------------------- */
    /*      Report default Metadata domain.                                 */
    /* -------------------------------------------------------------------- */
    CPLJSONObject oMetadata;
    oRoot.Add("metadata", oMetadata);
    GDALVectorInfoPrintMetadata(osRet, oMetadata, psOptions, hObject, nullptr,
                                "Metadata", pszIndent);

    /* -------------------------------------------------------------------- */
    /*      Report extra Metadata domains                                   */
    /* -------------------------------------------------------------------- */
    if (papszExtraMDDomains != nullptr)
    {
        CPLStringList aosExtraMDDomainsExpanded;

        if (EQUAL(papszExtraMDDomains[0], "all") &&
            papszExtraMDDomains[1] == nullptr)
        {
            const CPLStringList aosMDDList(GDALGetMetadataDomainList(hObject));
            for (const char *pszDomain : aosMDDList)
            {
                if (!EQUAL(pszDomain, "") && !EQUAL(pszDomain, "SUBDATASETS"))
                {
                    aosExtraMDDomainsExpanded.AddString(pszDomain);
                }
            }
        }
        else
        {
            aosExtraMDDomainsExpanded = CSLDuplicate(papszExtraMDDomains);
        }

        for (const char *pszDomain : aosExtraMDDomainsExpanded)
        {
            const std::string osDisplayedName =
                std::string("Metadata (").append(pszDomain).append(")");
            GDALVectorInfoPrintMetadata(osRet, oMetadata, psOptions, hObject,
                                        pszDomain, osDisplayedName.c_str(),
                                        pszIndent);
        }
    }
    GDALVectorInfoPrintMetadata(osRet, oMetadata, psOptions, hObject,
                                "SUBDATASETS", "Subdatasets", pszIndent);
}

/************************************************************************/
/*                           ReportOnLayer()                            */
/************************************************************************/

static void ReportOnLayer(CPLString &osRet, CPLJSONObject &oLayer,
                          const GDALVectorInfoOptions *psOptions,
                          OGRLayer *poLayer, bool bForceSummary,
                          bool bTakeIntoAccountWHERE,
                          bool bTakeIntoAccountSpatialFilter,
                          bool bTakeIntoAccountGeomField)
{
    const bool bJson = psOptions->eFormat == FORMAT_JSON;
    OGRFeatureDefn *poDefn = poLayer->GetLayerDefn();

    oLayer.Set("name", poLayer->GetName());

    /* -------------------------------------------------------------------- */
    /*      Set filters if provided.                                        */
    /* -------------------------------------------------------------------- */
    if (bTakeIntoAccountWHERE && !psOptions->osWHERE.empty())
    {
        if (poLayer->SetAttributeFilter(psOptions->osWHERE.c_str()) !=
            OGRERR_NONE)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "SetAttributeFilter(%s) failed.",
                     psOptions->osWHERE.c_str());
            return;
        }
    }

    if (bTakeIntoAccountSpatialFilter && psOptions->poSpatialFilter != nullptr)
    {
        if (bTakeIntoAccountGeomField && !psOptions->osGeomField.empty())
        {
            const int iGeomField =
                poDefn->GetGeomFieldIndex(psOptions->osGeomField.c_str());
            if (iGeomField >= 0)
                poLayer->SetSpatialFilter(iGeomField,
                                          psOptions->poSpatialFilter.get());
            else
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Cannot find geometry field %s.",
                         psOptions->osGeomField.c_str());
        }
        else
        {
            poLayer->SetSpatialFilter(psOptions->poSpatialFilter.get());
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Report various overall information.                             */
    /* -------------------------------------------------------------------- */
    if (!bJson && !psOptions->bSuperQuiet)
    {
        Concat(osRet, psOptions->bStdoutOutput, "\n");
        Concat(osRet, psOptions->bStdoutOutput, "Layer name: %s\n",
               poLayer->GetName());
    }

    GDALVectorInfoReportMetadata(osRet, oLayer, psOptions, poLayer,
                                 psOptions->bListMDD, psOptions->bShowMetadata,
                                 psOptions->aosExtraMDDomains.List());

    if (psOptions->bVerbose)
    {
        const int nGeomFieldCount =
            psOptions->bGeomType ? poLayer->GetLayerDefn()->GetGeomFieldCount()
                                 : 0;

        CPLString osWKTFormat("FORMAT=");
        osWKTFormat += psOptions->osWKTFormat;
        const char *const apszWKTOptions[] = {osWKTFormat.c_str(),
                                              "MULTILINE=YES", nullptr};

        if (bJson || nGeomFieldCount > 1)
        {
            CPLJSONArray oGeometryFields;
            if (bJson)
                oLayer.Add("geometryFields", oGeometryFields);
            for (int iGeom = 0; iGeom < nGeomFieldCount; iGeom++)
            {
                const OGRGeomFieldDefn *poGFldDefn =
                    poLayer->GetLayerDefn()->GetGeomFieldDefn(iGeom);
                if (bJson)
                {
                    CPLJSONObject oGeometryField;
                    oGeometryFields.Add(oGeometryField);
                    oGeometryField.Set("name", poGFldDefn->GetNameRef());
                    oGeometryField.Set(
                        "type", OGRToOGCGeomType(poGFldDefn->GetType(),
                                                 /*bCamelCase=*/true,
                                                 /*bAddZm=*/true,
                                                 /*bSpaceBeforeZM=*/false));
                    oGeometryField.Set("nullable",
                                       CPL_TO_BOOL(poGFldDefn->IsNullable()));
                    if (psOptions->bExtent3D)
                    {
                        OGREnvelope3D oExt;
                        if (poLayer->GetExtent3D(iGeom, &oExt, TRUE) ==
                            OGRERR_NONE)
                        {
                            {
                                CPLJSONArray oBbox;
                                oBbox.Add(oExt.MinX);
                                oBbox.Add(oExt.MinY);
                                oBbox.Add(oExt.MaxX);
                                oBbox.Add(oExt.MaxY);
                                oGeometryField.Add("extent", oBbox);
                            }
                            {
                                CPLJSONArray oBbox;
                                oBbox.Add(oExt.MinX);
                                oBbox.Add(oExt.MinY);
                                if (std::isfinite(oExt.MinZ))
                                    oBbox.Add(oExt.MinZ);
                                else
                                    oBbox.AddNull();
                                oBbox.Add(oExt.MaxX);
                                oBbox.Add(oExt.MaxY);
                                if (std::isfinite(oExt.MaxZ))
                                    oBbox.Add(oExt.MaxZ);
                                else
                                    oBbox.AddNull();
                                oGeometryField.Add("extent3D", oBbox);
                            }
                        }
                    }
                    else if (psOptions->bExtent)
                    {
                        OGREnvelope oExt;
                        if (poLayer->GetExtent(iGeom, &oExt, TRUE) ==
                            OGRERR_NONE)
                        {
                            CPLJSONArray oBbox;
                            oBbox.Add(oExt.MinX);
                            oBbox.Add(oExt.MinY);
                            oBbox.Add(oExt.MaxX);
                            oBbox.Add(oExt.MaxY);
                            oGeometryField.Add("extent", oBbox);
                        }
                    }
                    const OGRSpatialReference *poSRS =
                        poGFldDefn->GetSpatialRef();
                    if (poSRS)
                    {
                        CPLJSONObject oCRS;
                        oGeometryField.Add("coordinateSystem", oCRS);
                        char *pszWKT = nullptr;
                        poSRS->exportToWkt(&pszWKT, apszWKTOptions);
                        if (pszWKT)
                        {
                            oCRS.Set("wkt", pszWKT);
                            CPLFree(pszWKT);
                        }

                        {
                            char *pszProjJson = nullptr;
                            // PROJJSON requires PROJ >= 6.2
                            CPLErrorStateBackuper oCPLErrorHandlerPusher(
                                CPLQuietErrorHandler);
                            CPL_IGNORE_RET_VAL(
                                poSRS->exportToPROJJSON(&pszProjJson, nullptr));
                            if (pszProjJson)
                            {
                                CPLJSONDocument oDoc;
                                if (oDoc.LoadMemory(pszProjJson))
                                {
                                    oCRS.Add("projjson", oDoc.GetRoot());
                                }
                                CPLFree(pszProjJson);
                            }
                        }

                        const auto &anAxes =
                            poSRS->GetDataAxisToSRSAxisMapping();
                        CPLJSONArray oAxisMapping;
                        for (const auto nAxis : anAxes)
                        {
                            oAxisMapping.Add(nAxis);
                        }
                        oCRS.Add("dataAxisToSRSAxisMapping", oAxisMapping);

                        const double dfCoordinateEpoch =
                            poSRS->GetCoordinateEpoch();
                        if (dfCoordinateEpoch > 0)
                            oCRS.Set("coordinateEpoch", dfCoordinateEpoch);
                    }
                    else
                    {
                        oGeometryField.SetNull("coordinateSystem");
                    }

                    const auto &srsList = poLayer->GetSupportedSRSList(iGeom);
                    if (!srsList.empty())
                    {
                        CPLJSONArray oSupportedSRSList;
                        for (const auto &poSupportedSRS : srsList)
                        {
                            const char *pszAuthName =
                                poSupportedSRS->GetAuthorityName(nullptr);
                            const char *pszAuthCode =
                                poSupportedSRS->GetAuthorityCode(nullptr);
                            CPLJSONObject oSupportedSRS;
                            if (pszAuthName && pszAuthCode)
                            {
                                CPLJSONObject id;
                                id.Set("authority", pszAuthName);
                                id.Set("code", pszAuthCode);
                                oSupportedSRS.Add("id", id);
                                oSupportedSRSList.Add(oSupportedSRS);
                            }
                            else
                            {
                                char *pszWKT = nullptr;
                                poSupportedSRS->exportToWkt(&pszWKT,
                                                            apszWKTOptions);
                                if (pszWKT)
                                {
                                    oSupportedSRS.Add("wkt", pszWKT);
                                    oSupportedSRSList.Add(oSupportedSRS);
                                }
                                CPLFree(pszWKT);
                            }
                        }
                        oGeometryField.Add("supportedSRSList",
                                           oSupportedSRSList);
                    }

                    const auto &oCoordPrec =
                        poGFldDefn->GetCoordinatePrecision();
                    if (oCoordPrec.dfXYResolution !=
                        OGRGeomCoordinatePrecision::UNKNOWN)
                    {
                        oGeometryField.Add("xyCoordinateResolution",
                                           oCoordPrec.dfXYResolution);
                    }
                    if (oCoordPrec.dfZResolution !=
                        OGRGeomCoordinatePrecision::UNKNOWN)
                    {
                        oGeometryField.Add("zCoordinateResolution",
                                           oCoordPrec.dfZResolution);
                    }
                    if (oCoordPrec.dfMResolution !=
                        OGRGeomCoordinatePrecision::UNKNOWN)
                    {
                        oGeometryField.Add("mCoordinateResolution",
                                           oCoordPrec.dfMResolution);
                    }

                    // For example set by OpenFileGDB driver
                    if (!oCoordPrec.oFormatSpecificOptions.empty())
                    {
                        CPLJSONObject oFormatSpecificOptions;
                        for (const auto &formatOptionsPair :
                             oCoordPrec.oFormatSpecificOptions)
                        {
                            CPLJSONObject oThisFormatSpecificOptions;
                            for (const auto &[pszKey, pszValue] :
                                 cpl::IterateNameValue(
                                     formatOptionsPair.second))
                            {
                                const auto eValueType =
                                    CPLGetValueType(pszValue);
                                if (eValueType == CPL_VALUE_INTEGER)
                                {
                                    oThisFormatSpecificOptions.Add(
                                        pszKey, CPLAtoGIntBig(pszValue));
                                }
                                else if (eValueType == CPL_VALUE_REAL)
                                {
                                    oThisFormatSpecificOptions.Add(
                                        pszKey, CPLAtof(pszValue));
                                }
                                else
                                {
                                    oThisFormatSpecificOptions.Add(pszKey,
                                                                   pszValue);
                                }
                            }
                            oFormatSpecificOptions.Add(
                                formatOptionsPair.first,
                                oThisFormatSpecificOptions);
                        }
                        oGeometryField.Add(
                            "coordinatePrecisionFormatSpecificOptions",
                            oFormatSpecificOptions);
                    }
                }
                else
                {
                    Concat(osRet, psOptions->bStdoutOutput,
                           "Geometry (%s): %s\n", poGFldDefn->GetNameRef(),
                           OGRGeometryTypeToName(poGFldDefn->GetType()));
                }
            }
        }
        else if (psOptions->bGeomType)
        {
            Concat(osRet, psOptions->bStdoutOutput, "Geometry: %s\n",
                   OGRGeometryTypeToName(poLayer->GetGeomType()));
        }

        if (psOptions->bFeatureCount)
        {
            if (bJson)
                oLayer.Set("featureCount", poLayer->GetFeatureCount());
            else
            {
                Concat(osRet, psOptions->bStdoutOutput,
                       "Feature Count: " CPL_FRMT_GIB "\n",
                       poLayer->GetFeatureCount());
            }
        }

        if (!bJson && psOptions->bExtent && nGeomFieldCount > 1)
        {
            for (int iGeom = 0; iGeom < nGeomFieldCount; iGeom++)
            {
                if (psOptions->bExtent3D)
                {
                    OGREnvelope3D oExt;
                    if (poLayer->GetExtent3D(iGeom, &oExt, TRUE) == OGRERR_NONE)
                    {
                        OGRGeomFieldDefn *poGFldDefn =
                            poLayer->GetLayerDefn()->GetGeomFieldDefn(iGeom);
                        Concat(osRet, psOptions->bStdoutOutput,
                               "Extent (%s): (%f, %f, %s) - (%f, %f, %s)\n",
                               poGFldDefn->GetNameRef(), oExt.MinX, oExt.MinY,
                               std::isfinite(oExt.MinZ)
                                   ? CPLSPrintf("%f", oExt.MinZ)
                                   : "none",
                               oExt.MaxX, oExt.MaxY,
                               std::isfinite(oExt.MaxZ)
                                   ? CPLSPrintf("%f", oExt.MaxZ)
                                   : "none");
                    }
                }
                else
                {
                    OGREnvelope oExt;
                    if (poLayer->GetExtent(iGeom, &oExt, TRUE) == OGRERR_NONE)
                    {
                        OGRGeomFieldDefn *poGFldDefn =
                            poLayer->GetLayerDefn()->GetGeomFieldDefn(iGeom);
                        Concat(osRet, psOptions->bStdoutOutput,
                               "Extent (%s): (%f, %f) - (%f, %f)\n",
                               poGFldDefn->GetNameRef(), oExt.MinX, oExt.MinY,
                               oExt.MaxX, oExt.MaxY);
                    }
                }
            }
        }
        else if (!bJson && psOptions->bExtent)
        {
            if (psOptions->bExtent3D)
            {
                OGREnvelope3D oExt;
                if (poLayer->GetExtent3D(0, &oExt, TRUE) == OGRERR_NONE)
                {
                    Concat(
                        osRet, psOptions->bStdoutOutput,
                        "Extent: (%f, %f, %s) - (%f, %f, %s)\n", oExt.MinX,
                        oExt.MinY,
                        std::isfinite(oExt.MinZ) ? CPLSPrintf("%f", oExt.MinZ)
                                                 : "none",
                        oExt.MaxX, oExt.MaxY,
                        std::isfinite(oExt.MaxZ) ? CPLSPrintf("%f", oExt.MaxZ)
                                                 : "none");
                }
            }
            else
            {
                OGREnvelope oExt;
                if (poLayer->GetExtent(&oExt, TRUE) == OGRERR_NONE)
                {
                    Concat(osRet, psOptions->bStdoutOutput,
                           "Extent: (%f, %f) - (%f, %f)\n", oExt.MinX,
                           oExt.MinY, oExt.MaxX, oExt.MaxY);
                }
            }
        }

        const auto displayExtraInfoSRS =
            [&osRet, &psOptions](const OGRSpatialReference *poSRS)
        {
            const double dfCoordinateEpoch = poSRS->GetCoordinateEpoch();
            if (dfCoordinateEpoch > 0)
            {
                std::string osCoordinateEpoch =
                    CPLSPrintf("%f", dfCoordinateEpoch);
                const size_t nDotPos = osCoordinateEpoch.find('.');
                if (nDotPos != std::string::npos)
                {
                    while (osCoordinateEpoch.size() > nDotPos + 2 &&
                           osCoordinateEpoch.back() == '0')
                        osCoordinateEpoch.pop_back();
                }
                Concat(osRet, psOptions->bStdoutOutput,
                       "Coordinate epoch: %s\n", osCoordinateEpoch.c_str());
            }

            const auto &mapping = poSRS->GetDataAxisToSRSAxisMapping();
            Concat(osRet, psOptions->bStdoutOutput,
                   "Data axis to CRS axis mapping: ");
            for (size_t i = 0; i < mapping.size(); i++)
            {
                if (i > 0)
                {
                    Concat(osRet, psOptions->bStdoutOutput, ",");
                }
                Concat(osRet, psOptions->bStdoutOutput, "%d", mapping[i]);
            }
            Concat(osRet, psOptions->bStdoutOutput, "\n");
        };

        const auto DisplaySupportedCRSList = [&](int iGeomField)
        {
            const auto &srsList = poLayer->GetSupportedSRSList(iGeomField);
            if (!srsList.empty())
            {
                Concat(osRet, psOptions->bStdoutOutput, "Supported SRS: ");
                bool bFirst = true;
                for (const auto &poSupportedSRS : srsList)
                {
                    const char *pszAuthName =
                        poSupportedSRS->GetAuthorityName(nullptr);
                    const char *pszAuthCode =
                        poSupportedSRS->GetAuthorityCode(nullptr);
                    if (!bFirst)
                        Concat(osRet, psOptions->bStdoutOutput, ", ");
                    bFirst = false;
                    if (pszAuthName && pszAuthCode)
                    {
                        Concat(osRet, psOptions->bStdoutOutput, "%s:%s",
                               pszAuthName, pszAuthCode);
                    }
                    else
                    {
                        ConcatStr(osRet, psOptions->bStdoutOutput,
                                  poSupportedSRS->GetName());
                    }
                }
                Concat(osRet, psOptions->bStdoutOutput, "\n");
            }
        };

        if (!bJson && nGeomFieldCount > 1)
        {

            for (int iGeom = 0; iGeom < nGeomFieldCount; iGeom++)
            {
                OGRGeomFieldDefn *poGFldDefn =
                    poLayer->GetLayerDefn()->GetGeomFieldDefn(iGeom);
                const OGRSpatialReference *poSRS = poGFldDefn->GetSpatialRef();
                char *pszWKT = nullptr;
                if (poSRS == nullptr)
                {
                    pszWKT = CPLStrdup("(unknown)");
                }
                else
                {
                    poSRS->exportToWkt(&pszWKT, apszWKTOptions);
                }

                Concat(osRet, psOptions->bStdoutOutput, "SRS WKT (%s):\n%s\n",
                       poGFldDefn->GetNameRef(), pszWKT);
                CPLFree(pszWKT);
                if (poSRS)
                {
                    displayExtraInfoSRS(poSRS);
                }
                DisplaySupportedCRSList(iGeom);
            }
        }
        else if (!bJson)
        {
            char *pszWKT = nullptr;
            auto poSRS = poLayer->GetSpatialRef();
            if (poSRS == nullptr)
            {
                pszWKT = CPLStrdup("(unknown)");
            }
            else
            {
                poSRS->exportToWkt(&pszWKT, apszWKTOptions);
            }

            Concat(osRet, psOptions->bStdoutOutput, "Layer SRS WKT:\n%s\n",
                   pszWKT);
            CPLFree(pszWKT);
            if (poSRS)
            {
                displayExtraInfoSRS(poSRS);
            }
            DisplaySupportedCRSList(0);
        }

        const char *pszFIDColumn = poLayer->GetFIDColumn();
        if (pszFIDColumn[0] != '\0')
        {
            if (bJson)
                oLayer.Set("fidColumnName", pszFIDColumn);
            else
            {
                Concat(osRet, psOptions->bStdoutOutput, "FID Column = %s\n",
                       pszFIDColumn);
            }
        }

        for (int iGeom = 0; !bJson && iGeom < nGeomFieldCount; iGeom++)
        {
            OGRGeomFieldDefn *poGFldDefn =
                poLayer->GetLayerDefn()->GetGeomFieldDefn(iGeom);
            if (nGeomFieldCount == 1 && EQUAL(poGFldDefn->GetNameRef(), "") &&
                poGFldDefn->IsNullable())
                break;
            Concat(osRet, psOptions->bStdoutOutput, "Geometry Column ");
            if (nGeomFieldCount > 1)
                Concat(osRet, psOptions->bStdoutOutput, "%d ", iGeom + 1);
            if (!poGFldDefn->IsNullable())
                Concat(osRet, psOptions->bStdoutOutput, "NOT NULL ");
            Concat(osRet, psOptions->bStdoutOutput, "= %s\n",
                   poGFldDefn->GetNameRef());
        }

        CPLJSONArray oFields;
        if (bJson)
            oLayer.Add("fields", oFields);
        for (int iAttr = 0; iAttr < poDefn->GetFieldCount(); iAttr++)
        {
            const OGRFieldDefn *poField = poDefn->GetFieldDefn(iAttr);
            const char *pszAlias = poField->GetAlternativeNameRef();
            const std::string &osDomain = poField->GetDomainName();
            const std::string &osComment = poField->GetComment();
            const auto eType = poField->GetType();
            std::string osTimeZone;
            if (eType == OFTTime || eType == OFTDate || eType == OFTDateTime)
            {
                const int nTZFlag = poField->GetTZFlag();
                if (nTZFlag == OGR_TZFLAG_LOCALTIME)
                {
                    osTimeZone = "localtime";
                }
                else if (nTZFlag == OGR_TZFLAG_MIXED_TZ)
                {
                    osTimeZone = "mixed timezones";
                }
                else if (nTZFlag == OGR_TZFLAG_UTC)
                {
                    osTimeZone = "UTC";
                }
                else if (nTZFlag > 0)
                {
                    char chSign;
                    const int nOffset = (nTZFlag - OGR_TZFLAG_UTC) * 15;
                    int nHours =
                        static_cast<int>(nOffset / 60);  // Round towards zero.
                    const int nMinutes = std::abs(nOffset - nHours * 60);

                    if (nOffset < 0)
                    {
                        chSign = '-';
                        nHours = std::abs(nHours);
                    }
                    else
                    {
                        chSign = '+';
                    }
                    osTimeZone =
                        CPLSPrintf("%c%02d:%02d", chSign, nHours, nMinutes);
                }
            }

            if (bJson)
            {
                CPLJSONObject oField;
                oFields.Add(oField);
                oField.Set("name", poField->GetNameRef());
                oField.Set("type", OGRFieldDefn::GetFieldTypeName(eType));
                if (poField->GetSubType() != OFSTNone)
                    oField.Set("subType", OGRFieldDefn::GetFieldSubTypeName(
                                              poField->GetSubType()));
                if (poField->GetWidth() > 0)
                    oField.Set("width", poField->GetWidth());
                if (poField->GetPrecision() > 0)
                    oField.Set("precision", poField->GetPrecision());
                oField.Set("nullable", CPL_TO_BOOL(poField->IsNullable()));
                oField.Set("uniqueConstraint",
                           CPL_TO_BOOL(poField->IsUnique()));
                if (poField->GetDefault() != nullptr)
                    oField.Set("defaultValue", poField->GetDefault());
                if (pszAlias != nullptr && pszAlias[0])
                    oField.Set("alias", pszAlias);
                if (!osDomain.empty())
                    oField.Set("domainName", osDomain);
                if (!osComment.empty())
                    oField.Set("comment", osComment);
                if (!osTimeZone.empty())
                    oField.Set("timezone", osTimeZone);
            }
            else
            {
                const char *pszType =
                    (poField->GetSubType() != OFSTNone)
                        ? CPLSPrintf("%s(%s)",
                                     OGRFieldDefn::GetFieldTypeName(
                                         poField->GetType()),
                                     OGRFieldDefn::GetFieldSubTypeName(
                                         poField->GetSubType()))
                        : OGRFieldDefn::GetFieldTypeName(poField->GetType());
                Concat(osRet, psOptions->bStdoutOutput, "%s: %s",
                       poField->GetNameRef(), pszType);
                if (eType == OFTTime || eType == OFTDate ||
                    eType == OFTDateTime)
                {
                    if (!osTimeZone.empty())
                        Concat(osRet, psOptions->bStdoutOutput, " (%s)",
                               osTimeZone.c_str());
                }
                else
                {
                    Concat(osRet, psOptions->bStdoutOutput, " (%d.%d)",
                           poField->GetWidth(), poField->GetPrecision());
                }
                if (poField->IsUnique())
                    Concat(osRet, psOptions->bStdoutOutput, " UNIQUE");
                if (!poField->IsNullable())
                    Concat(osRet, psOptions->bStdoutOutput, " NOT NULL");
                if (poField->GetDefault() != nullptr)
                    Concat(osRet, psOptions->bStdoutOutput, " DEFAULT %s",
                           poField->GetDefault());
                if (pszAlias != nullptr && pszAlias[0])
                    Concat(osRet, psOptions->bStdoutOutput,
                           ", alternative name=\"%s\"", pszAlias);
                if (!osDomain.empty())
                    Concat(osRet, psOptions->bStdoutOutput, ", domain name=%s",
                           osDomain.c_str());
                if (!osComment.empty())
                    Concat(osRet, psOptions->bStdoutOutput, ", comment=%s",
                           osComment.c_str());
                Concat(osRet, psOptions->bStdoutOutput, "\n");
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Read, and dump features.                                        */
    /* -------------------------------------------------------------------- */

    if (psOptions->nFetchFID == OGRNullFID && !bForceSummary &&
        !psOptions->bSummaryOnly)
    {
        if (!psOptions->bSuperQuiet)
        {
            CPLJSONArray oFeatures;
            const bool bDisplayFields =
                CPLTestBool(psOptions->aosOptions.FetchNameValueDef(
                    "DISPLAY_FIELDS", "YES"));
            const int nFields =
                bDisplayFields ? poLayer->GetLayerDefn()->GetFieldCount() : 0;
            const bool bDisplayGeometry =
                CPLTestBool(psOptions->aosOptions.FetchNameValueDef(
                    "DISPLAY_GEOMETRY", "YES"));
            const int nGeomFields =
                bDisplayGeometry ? poLayer->GetLayerDefn()->GetGeomFieldCount()
                                 : 0;
            if (bJson)
                oLayer.Add("features", oFeatures);
            GIntBig nFeatureCount = 0;
            for (auto &poFeature : poLayer)
            {
                if (psOptions->nLimit >= 0 &&
                    nFeatureCount >= psOptions->nLimit)
                {
                    break;
                }
                ++nFeatureCount;

                if (bJson)
                {
                    CPLJSONObject oFeature;
                    CPLJSONObject oProperties;
                    oFeatures.Add(oFeature);
                    oFeature.Add("type", "Feature");
                    oFeature.Add("properties", oProperties);
                    oFeature.Add("fid", poFeature->GetFID());
                    for (int i = 0; i < nFields; ++i)
                    {
                        const auto poFDefn = poFeature->GetFieldDefnRef(i);
                        const auto eType = poFDefn->GetType();
                        if (!poFeature->IsFieldSet(i))
                            continue;
                        if (poFeature->IsFieldNull(i))
                        {
                            oProperties.SetNull(poFDefn->GetNameRef());
                        }
                        else if (eType == OFTInteger)
                        {
                            if (poFDefn->GetSubType() == OFSTBoolean)
                                oProperties.Add(
                                    poFDefn->GetNameRef(),
                                    CPL_TO_BOOL(
                                        poFeature->GetFieldAsInteger(i)));
                            else
                                oProperties.Add(
                                    poFDefn->GetNameRef(),
                                    poFeature->GetFieldAsInteger(i));
                        }
                        else if (eType == OFTInteger64)
                        {
                            oProperties.Add(poFDefn->GetNameRef(),
                                            poFeature->GetFieldAsInteger64(i));
                        }
                        else if (eType == OFTReal)
                        {
                            oProperties.Add(poFDefn->GetNameRef(),
                                            poFeature->GetFieldAsDouble(i));
                        }
                        else if ((eType == OFTString &&
                                  poFDefn->GetSubType() != OFSTJSON) ||
                                 eType == OFTDate || eType == OFTTime ||
                                 eType == OFTDateTime)
                        {
                            oProperties.Add(poFDefn->GetNameRef(),
                                            poFeature->GetFieldAsString(i));
                        }
                        else
                        {
                            char *pszSerialized =
                                poFeature->GetFieldAsSerializedJSon(i);
                            if (pszSerialized)
                            {
                                const auto eStrType =
                                    CPLGetValueType(pszSerialized);
                                if (eStrType == CPL_VALUE_INTEGER)
                                {
                                    oProperties.Add(
                                        poFDefn->GetNameRef(),
                                        CPLAtoGIntBig(pszSerialized));
                                }
                                else if (eStrType == CPL_VALUE_REAL)
                                {
                                    oProperties.Add(poFDefn->GetNameRef(),
                                                    CPLAtof(pszSerialized));
                                }
                                else
                                {
                                    CPLJSONDocument oDoc;
                                    if (oDoc.LoadMemory(pszSerialized))
                                        oProperties.Add(poFDefn->GetNameRef(),
                                                        oDoc.GetRoot());
                                }
                                CPLFree(pszSerialized);
                            }
                        }
                    }

                    const auto GetGeoJSONOptions = [poLayer](int iGeomField)
                    {
                        CPLStringList aosGeoJSONOptions;
                        const auto &oCoordPrec =
                            poLayer->GetLayerDefn()
                                ->GetGeomFieldDefn(iGeomField)
                                ->GetCoordinatePrecision();
                        if (oCoordPrec.dfXYResolution !=
                            OGRGeomCoordinatePrecision::UNKNOWN)
                        {
                            aosGeoJSONOptions.SetNameValue(
                                "XY_COORD_PRECISION",
                                CPLSPrintf("%d",
                                           OGRGeomCoordinatePrecision::
                                               ResolutionToPrecision(
                                                   oCoordPrec.dfXYResolution)));
                        }
                        if (oCoordPrec.dfZResolution !=
                            OGRGeomCoordinatePrecision::UNKNOWN)
                        {
                            aosGeoJSONOptions.SetNameValue(
                                "Z_COORD_PRECISION",
                                CPLSPrintf("%d",
                                           OGRGeomCoordinatePrecision::
                                               ResolutionToPrecision(
                                                   oCoordPrec.dfZResolution)));
                        }
                        return aosGeoJSONOptions;
                    };

                    if (nGeomFields == 0)
                        oFeature.SetNull("geometry");
                    else
                    {
                        if (const auto poGeom = poFeature->GetGeometryRef())
                        {
                            char *pszSerialized =
                                wkbFlatten(poGeom->getGeometryType()) <=
                                        wkbGeometryCollection
                                    ? poGeom->exportToJson(
                                          GetGeoJSONOptions(0).List())
                                    : nullptr;
                            if (pszSerialized)
                            {
                                CPLJSONDocument oDoc;
                                if (oDoc.LoadMemory(pszSerialized))
                                    oFeature.Add("geometry", oDoc.GetRoot());
                                CPLFree(pszSerialized);
                            }
                            else
                            {
                                CPLJSONObject oGeometry;
                                oFeature.SetNull("geometry");
                                oFeature.Add("wkt_geometry",
                                             poGeom->exportToWkt());
                            }
                        }
                        else
                            oFeature.SetNull("geometry");

                        if (nGeomFields > 1)
                        {
                            CPLJSONArray oGeometries;
                            oFeature.Add("geometries", oGeometries);
                            for (int i = 0; i < nGeomFields; ++i)
                            {
                                auto poGeom = poFeature->GetGeomFieldRef(i);
                                if (poGeom)
                                {
                                    char *pszSerialized =
                                        wkbFlatten(poGeom->getGeometryType()) <=
                                                wkbGeometryCollection
                                            ? poGeom->exportToJson(
                                                  GetGeoJSONOptions(i).List())
                                            : nullptr;
                                    if (pszSerialized)
                                    {
                                        CPLJSONDocument oDoc;
                                        if (oDoc.LoadMemory(pszSerialized))
                                            oGeometries.Add(oDoc.GetRoot());
                                        CPLFree(pszSerialized);
                                    }
                                    else
                                    {
                                        CPLJSONObject oGeometry;
                                        oGeometries.Add(poGeom->exportToWkt());
                                    }
                                }
                                else
                                    oGeometries.AddNull();
                            }
                        }
                    }
                }
                else
                {
                    ConcatStr(
                        osRet, psOptions->bStdoutOutput,
                        poFeature
                            ->DumpReadableAsString(psOptions->aosOptions.List())
                            .c_str());
                }
            }
        }
    }
    else if (!bJson && psOptions->nFetchFID != OGRNullFID)
    {
        OGRFeature *poFeature = poLayer->GetFeature(psOptions->nFetchFID);
        if (poFeature == nullptr)
        {
            Concat(osRet, psOptions->bStdoutOutput,
                   "Unable to locate feature id " CPL_FRMT_GIB
                   " on this layer.\n",
                   psOptions->nFetchFID);
        }
        else
        {
            ConcatStr(
                osRet, psOptions->bStdoutOutput,
                poFeature->DumpReadableAsString(psOptions->aosOptions.List())
                    .c_str());
            OGRFeature::DestroyFeature(poFeature);
        }
    }
}

/************************************************************************/
/*                           PrintLayerSummary()                        */
/************************************************************************/

static void PrintLayerSummary(CPLString &osRet, CPLJSONObject &oLayer,
                              const GDALVectorInfoOptions *psOptions,
                              OGRLayer *poLayer, bool bIsPrivate)
{
    const bool bJson = psOptions->eFormat == FORMAT_JSON;
    if (bJson)
        oLayer.Set("name", poLayer->GetName());
    else
        ConcatStr(osRet, psOptions->bStdoutOutput, poLayer->GetName());

    const char *pszTitle = poLayer->GetMetadataItem("TITLE");
    if (pszTitle)
    {
        if (bJson)
            oLayer.Set("title", pszTitle);
        else
            Concat(osRet, psOptions->bStdoutOutput, " (title: %s)", pszTitle);
    }

    const int nGeomFieldCount =
        psOptions->bGeomType ? poLayer->GetLayerDefn()->GetGeomFieldCount() : 0;
    if (bJson || nGeomFieldCount > 1)
    {
        if (!bJson)
            Concat(osRet, psOptions->bStdoutOutput, " (");
        CPLJSONArray oGeometryFields;
        oLayer.Add("geometryFields", oGeometryFields);
        for (int iGeom = 0; iGeom < nGeomFieldCount; iGeom++)
        {
            OGRGeomFieldDefn *poGFldDefn =
                poLayer->GetLayerDefn()->GetGeomFieldDefn(iGeom);
            if (bJson)
            {
                oGeometryFields.Add(
                    OGRGeometryTypeToName(poGFldDefn->GetType()));
            }
            else
            {
                if (iGeom > 0)
                    Concat(osRet, psOptions->bStdoutOutput, ", ");
                ConcatStr(osRet, psOptions->bStdoutOutput,
                          OGRGeometryTypeToName(poGFldDefn->GetType()));
            }
        }
        if (!bJson)
            Concat(osRet, psOptions->bStdoutOutput, ")");
    }
    else if (psOptions->bGeomType && poLayer->GetGeomType() != wkbUnknown)
        Concat(osRet, psOptions->bStdoutOutput, " (%s)",
               OGRGeometryTypeToName(poLayer->GetGeomType()));

    if (bIsPrivate)
    {
        if (bJson)
            oLayer.Set("isPrivate", true);
        else
            Concat(osRet, psOptions->bStdoutOutput, " [private]");
    }

    if (!bJson)
        Concat(osRet, psOptions->bStdoutOutput, "\n");
}

/************************************************************************/
/*                       ReportHiearchicalLayers()                      */
/************************************************************************/

static void ReportHiearchicalLayers(CPLString &osRet, CPLJSONObject &oRoot,
                                    const GDALVectorInfoOptions *psOptions,
                                    const GDALGroup *group,
                                    const std::string &indent, bool bGeomType)
{
    const bool bJson = psOptions->eFormat == FORMAT_JSON;
    const auto aosVectorLayerNames = group->GetVectorLayerNames();
    CPLJSONArray oLayerNames;
    oRoot.Add("layerNames", oLayerNames);
    for (const auto &osVectorLayerName : aosVectorLayerNames)
    {
        OGRLayer *poLayer = group->OpenVectorLayer(osVectorLayerName);
        if (poLayer)
        {
            CPLJSONObject oLayer;
            if (!bJson)
            {
                Concat(osRet, psOptions->bStdoutOutput,
                       "%sLayer: ", indent.c_str());
                PrintLayerSummary(osRet, oLayer, psOptions, poLayer,
                                  /* bIsPrivate=*/false);
            }
            else
            {
                oLayerNames.Add(poLayer->GetName());
            }
        }
    }

    const std::string subIndent(indent + "  ");
    auto aosSubGroupNames = group->GetGroupNames();
    CPLJSONArray oGroupArray;
    oRoot.Add("groups", oGroupArray);
    for (const auto &osSubGroupName : aosSubGroupNames)
    {
        auto poSubGroup = group->OpenGroup(osSubGroupName);
        if (poSubGroup)
        {
            CPLJSONObject oGroup;
            if (!bJson)
            {
                Concat(osRet, psOptions->bStdoutOutput, "Group %s",
                       indent.c_str());
                Concat(osRet, psOptions->bStdoutOutput, "%s:\n",
                       osSubGroupName.c_str());
            }
            else
            {
                oGroupArray.Add(oGroup);
                oGroup.Set("name", osSubGroupName);
            }
            ReportHiearchicalLayers(osRet, oGroup, psOptions, poSubGroup.get(),
                                    subIndent, bGeomType);
        }
    }
}

/************************************************************************/
/*                           GDALVectorInfo()                           */
/************************************************************************/

/**
 * Lists various information about a GDAL supported vector dataset.
 *
 * This is the equivalent of the <a href="/programs/ogrinfo.html">ogrinfo</a>
 * utility.
 *
 * GDALVectorInfoOptions* must be allocated and freed with
 * GDALVectorInfoOptionsNew() and GDALVectorInfoOptionsFree() respectively.
 *
 * @param hDataset the dataset handle.
 * @param psOptions the options structure returned by GDALVectorInfoOptionsNew()
 * or NULL.
 * @return string corresponding to the information about the raster dataset
 * (must be freed with CPLFree()), or NULL in case of error.
 *
 * @since GDAL 3.7
 */
char *GDALVectorInfo(GDALDatasetH hDataset,
                     const GDALVectorInfoOptions *psOptions)
{
    auto poDS = GDALDataset::FromHandle(hDataset);
    if (poDS == nullptr)
        return nullptr;

    const GDALVectorInfoOptions sDefaultOptions;
    if (!psOptions)
        psOptions = &sDefaultOptions;

    GDALDriver *poDriver = poDS->GetDriver();

    CPLString osRet;
    CPLJSONObject oRoot;
    const std::string osFilename(poDS->GetDescription());

    const bool bJson = psOptions->eFormat == FORMAT_JSON;
    CPLJSONArray oLayerArray;
    if (bJson)
    {
        oRoot.Set("description", poDS->GetDescription());
        if (poDriver)
        {
            oRoot.Set("driverShortName", poDriver->GetDescription());
            oRoot.Set("driverLongName",
                      poDriver->GetMetadataItem(GDAL_DMD_LONGNAME));
        }
        oRoot.Add("layers", oLayerArray);
    }

    /* -------------------------------------------------------------------- */
    /*      Some information messages.                                      */
    /* -------------------------------------------------------------------- */
    if (!bJson && psOptions->bVerbose)
    {
        Concat(osRet, psOptions->bStdoutOutput,
               "INFO: Open of `%s'\n"
               "      using driver `%s' successful.\n",
               osFilename.c_str(),
               poDriver ? poDriver->GetDescription() : "(null)");
    }

    if (!bJson && psOptions->bVerbose &&
        !EQUAL(osFilename.c_str(), poDS->GetDescription()))
    {
        Concat(osRet, psOptions->bStdoutOutput,
               "INFO: Internal data source name `%s'\n"
               "      different from user name `%s'.\n",
               poDS->GetDescription(), osFilename.c_str());
    }

    GDALVectorInfoReportMetadata(osRet, oRoot, psOptions, poDS,
                                 psOptions->bListMDD, psOptions->bShowMetadata,
                                 psOptions->aosExtraMDDomains.List());

    CPLJSONObject oDomains;
    oRoot.Add("domains", oDomains);
    if (!psOptions->osFieldDomain.empty())
    {
        auto poDomain = poDS->GetFieldDomain(psOptions->osFieldDomain);
        if (poDomain == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Domain %s cannot be found.",
                     psOptions->osFieldDomain.c_str());
            return nullptr;
        }
        if (!bJson)
            Concat(osRet, psOptions->bStdoutOutput, "\n");
        ReportFieldDomain(osRet, oDomains, psOptions, poDomain);
        if (!bJson)
            Concat(osRet, psOptions->bStdoutOutput, "\n");
    }
    else if (bJson)
    {
        for (const auto &osDomainName : poDS->GetFieldDomainNames())
        {
            auto poDomain = poDS->GetFieldDomain(osDomainName);
            if (poDomain)
            {
                ReportFieldDomain(osRet, oDomains, psOptions, poDomain);
            }
        }
    }

    int nRepeatCount = psOptions->nRepeatCount;
    if (psOptions->bDatasetGetNextFeature)
    {
        nRepeatCount = 0;  // skip layer reporting.

        /* --------------------------------------------------------------------
         */
        /*      Set filters if provided. */
        /* --------------------------------------------------------------------
         */
        if (!psOptions->osWHERE.empty() ||
            psOptions->poSpatialFilter != nullptr)
        {
            for (int iLayer = 0; iLayer < poDS->GetLayerCount(); iLayer++)
            {
                OGRLayer *poLayer = poDS->GetLayer(iLayer);

                if (poLayer == nullptr)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Couldn't fetch advertised layer %d.", iLayer);
                    return nullptr;
                }

                if (!psOptions->osWHERE.empty())
                {
                    if (poLayer->SetAttributeFilter(
                            psOptions->osWHERE.c_str()) != OGRERR_NONE)
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "SetAttributeFilter(%s) failed on layer %s.",
                                 psOptions->osWHERE.c_str(),
                                 poLayer->GetName());
                    }
                }

                if (psOptions->poSpatialFilter != nullptr)
                {
                    if (!psOptions->osGeomField.empty())
                    {
                        OGRFeatureDefn *poDefn = poLayer->GetLayerDefn();
                        const int iGeomField = poDefn->GetGeomFieldIndex(
                            psOptions->osGeomField.c_str());
                        if (iGeomField >= 0)
                            poLayer->SetSpatialFilter(
                                iGeomField, psOptions->poSpatialFilter.get());
                        else
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "Cannot find geometry field %s.",
                                     psOptions->osGeomField.c_str());
                    }
                    else
                    {
                        poLayer->SetSpatialFilter(
                            psOptions->poSpatialFilter.get());
                    }
                }
            }
        }

        std::set<OGRLayer *> oSetLayers;
        while (true)
        {
            OGRLayer *poLayer = nullptr;
            OGRFeature *poFeature =
                poDS->GetNextFeature(&poLayer, nullptr, nullptr, nullptr);
            if (poFeature == nullptr)
                break;
            if (psOptions->aosLayers.empty() || poLayer == nullptr ||
                CSLFindString(psOptions->aosLayers.List(),
                              poLayer->GetName()) >= 0)
            {
                if (psOptions->bVerbose && poLayer != nullptr &&
                    oSetLayers.find(poLayer) == oSetLayers.end())
                {
                    oSetLayers.insert(poLayer);
                    CPLJSONObject oLayer;
                    oLayerArray.Add(oLayer);
                    ReportOnLayer(osRet, oLayer, psOptions, poLayer,
                                  /*bForceSummary = */ true,
                                  /*bTakeIntoAccountWHERE = */ false,
                                  /*bTakeIntoAccountSpatialFilter = */ false,
                                  /*bTakeIntoAccountGeomField = */ false);
                }
                if (!psOptions->bSuperQuiet && !psOptions->bSummaryOnly)
                    poFeature->DumpReadable(
                        nullptr,
                        const_cast<char **>(psOptions->aosOptions.List()));
            }
            OGRFeature::DestroyFeature(poFeature);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Special case for -sql clause.  No source layers required.       */
    /* -------------------------------------------------------------------- */
    else if (!psOptions->osSQLStatement.empty())
    {
        nRepeatCount = 0;  // skip layer reporting.

        if (!bJson && !psOptions->aosLayers.empty())
            Concat(osRet, psOptions->bStdoutOutput,
                   "layer names ignored in combination with -sql.\n");

        CPLErrorReset();
        OGRLayer *poResultSet = poDS->ExecuteSQL(
            psOptions->osSQLStatement.c_str(),
            psOptions->osGeomField.empty() ? psOptions->poSpatialFilter.get()
                                           : nullptr,
            psOptions->osDialect.empty() ? nullptr
                                         : psOptions->osDialect.c_str());

        if (poResultSet != nullptr)
        {
            if (!psOptions->osWHERE.empty())
            {
                if (poResultSet->SetAttributeFilter(
                        psOptions->osWHERE.c_str()) != OGRERR_NONE)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "SetAttributeFilter(%s) failed.",
                             psOptions->osWHERE.c_str());
                    return nullptr;
                }
            }

            CPLJSONObject oLayer;
            oLayerArray.Add(oLayer);
            if (!psOptions->osGeomField.empty())
                ReportOnLayer(osRet, oLayer, psOptions, poResultSet,
                              /*bForceSummary = */ false,
                              /*bTakeIntoAccountWHERE = */ false,
                              /*bTakeIntoAccountSpatialFilter = */ true,
                              /*bTakeIntoAccountGeomField = */ true);
            else
                ReportOnLayer(osRet, oLayer, psOptions, poResultSet,
                              /*bForceSummary = */ false,
                              /*bTakeIntoAccountWHERE = */ false,
                              /*bTakeIntoAccountSpatialFilter = */ false,
                              /*bTakeIntoAccountGeomField = */ false);

            poDS->ReleaseResultSet(poResultSet);
        }
        else if (CPLGetLastErrorType() != CE_None)
        {
            return nullptr;
        }
    }

    // coverity[tainted_data]
    auto papszLayers = psOptions->aosLayers.List();
    for (int iRepeat = 0; iRepeat < nRepeatCount; iRepeat++)
    {
        if (papszLayers == nullptr || papszLayers[0] == nullptr)
        {
            const int nLayerCount = poDS->GetLayerCount();
            if (iRepeat == 0)
                CPLDebug("OGR", "GetLayerCount() = %d\n", nLayerCount);

            bool bDone = false;
            auto poRootGroup = poDS->GetRootGroup();
            if ((bJson || !psOptions->bAllLayers) && poRootGroup &&
                (!poRootGroup->GetGroupNames().empty() ||
                 !poRootGroup->GetVectorLayerNames().empty()))
            {
                CPLJSONObject oGroup;
                oRoot.Add("rootGroup", oGroup);
                ReportHiearchicalLayers(osRet, oGroup, psOptions,
                                        poRootGroup.get(), std::string(),
                                        psOptions->bGeomType);
                if (!bJson)
                    bDone = true;
            }

            /* --------------------------------------------------------------------
             */
            /*      Process each data source layer. */
            /* --------------------------------------------------------------------
             */
            for (int iLayer = 0; !bDone && iLayer < nLayerCount; iLayer++)
            {
                OGRLayer *poLayer = poDS->GetLayer(iLayer);

                if (poLayer == nullptr)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Couldn't fetch advertised layer %d.", iLayer);
                    return nullptr;
                }

                CPLJSONObject oLayer;
                oLayerArray.Add(oLayer);
                if (!psOptions->bAllLayers)
                {
                    if (!bJson)
                        Concat(osRet, psOptions->bStdoutOutput,
                               "%d: ", iLayer + 1);
                    PrintLayerSummary(osRet, oLayer, psOptions, poLayer,
                                      poDS->IsLayerPrivate(iLayer));
                }
                else
                {
                    if (iRepeat != 0)
                        poLayer->ResetReading();

                    ReportOnLayer(osRet, oLayer, psOptions, poLayer,
                                  /*bForceSummary = */ false,
                                  /*bTakeIntoAccountWHERE = */ true,
                                  /*bTakeIntoAccountSpatialFilter = */ true,
                                  /*bTakeIntoAccountGeomField = */ true);
                }
            }
        }
        else
        {
            /* --------------------------------------------------------------------
             */
            /*      Process specified data source layers. */
            /* --------------------------------------------------------------------
             */

            for (const char *pszLayer : cpl::Iterate(papszLayers))
            {
                OGRLayer *poLayer = poDS->GetLayerByName(pszLayer);

                if (poLayer == nullptr)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Couldn't fetch requested layer %s.", pszLayer);
                    return nullptr;
                }

                if (iRepeat != 0)
                    poLayer->ResetReading();

                CPLJSONObject oLayer;
                oLayerArray.Add(oLayer);
                ReportOnLayer(osRet, oLayer, psOptions, poLayer,
                              /*bForceSummary = */ false,
                              /*bTakeIntoAccountWHERE = */ true,
                              /*bTakeIntoAccountSpatialFilter = */ true,
                              /*bTakeIntoAccountGeomField = */ true);
            }
        }
    }

    if (!papszLayers)
    {
        ReportRelationships(osRet, oRoot, psOptions, poDS);
    }

    if (bJson)
    {
        osRet.clear();
        ConcatStr(
            osRet, psOptions->bStdoutOutput,
            json_object_to_json_string_ext(
                static_cast<struct json_object *>(oRoot.GetInternalHandle()),
                JSON_C_TO_STRING_PRETTY
#ifdef JSON_C_TO_STRING_NOSLASHESCAPE
                    | JSON_C_TO_STRING_NOSLASHESCAPE
#endif
                ));
        ConcatStr(osRet, psOptions->bStdoutOutput, "\n");
    }

    return VSI_STRDUP_VERBOSE(osRet);
}

/************************************************************************/
/*                    GDALVectorInfoOptionsGetParser()                  */
/************************************************************************/

static std::unique_ptr<GDALArgumentParser> GDALVectorInfoOptionsGetParser(
    GDALVectorInfoOptions *psOptions,
    GDALVectorInfoOptionsForBinary *psOptionsForBinary)
{
    auto argParser = std::make_unique<GDALArgumentParser>(
        "ogrinfo", /* bForBinary=*/psOptionsForBinary != nullptr);

    argParser->add_description(
        _("Lists information about an OGR-supported data source."));

    argParser->add_epilog(
        _("For more details, consult https://gdal.org/programs/ogrinfo.html"));

    argParser->add_argument("-json")
        .flag()
        .action(
            [psOptions](const std::string &)
            {
                psOptions->eFormat = FORMAT_JSON;
                psOptions->bAllLayers = true;
                psOptions->bSummaryOnly = true;
            })
        .help(_("Display the output in json format."));

    argParser->add_argument("-ro")
        .flag()
        .action(
            [psOptionsForBinary](const std::string &)
            {
                if (psOptionsForBinary)
                    psOptionsForBinary->bReadOnly = true;
            })
        .help(_("Open the data source in read-only mode."));

    argParser->add_argument("-update")
        .flag()
        .action(
            [psOptionsForBinary](const std::string &)
            {
                if (psOptionsForBinary)
                    psOptionsForBinary->bUpdate = true;
            })
        .help(_("Open the data source in update mode."));

    argParser->add_argument("-q", "--quiet")
        .flag()
        .action(
            [psOptions, psOptionsForBinary](const std::string &)
            {
                psOptions->bVerbose = false;
                if (psOptionsForBinary)
                    psOptionsForBinary->bVerbose = false;
            })
        .help(_("Quiet mode. No progress message is emitted on the standard "
                "output."));

#ifdef __AFL_HAVE_MANUAL_CONTROL
    /* Undocumented: mainly only useful for AFL testing */
    argParser->add_argument("-qq")
        .flag()
        .hidden()
        .action(
            [psOptions, psOptionsForBinary](const std::string &)
            {
                psOptions->bVerbose = false;
                if (psOptionsForBinary)
                    psOptionsForBinary->bVerbose = false;
                psOptions->bSuperQuiet = true;
            })
        .help(_("Super quiet mode."));
#endif

    argParser->add_argument("-fid")
        .metavar("<FID>")
        .store_into(psOptions->nFetchFID)
        .help(_("Only the feature with this feature id will be reported."));

    argParser->add_argument("-spat")
        .metavar("<xmin> <ymin> <xmax> <ymax>")
        .nargs(4)
        .scan<'g', double>()
        .help(_("The area of interest. Only features within the rectangle will "
                "be reported."));

    argParser->add_argument("-geomfield")
        .metavar("<field>")
        .store_into(psOptions->osGeomField)
        .help(_("Name of the geometry field on which the spatial filter "
                "operates."));

    argParser->add_argument("-where")
        .metavar("<restricted_where>")
        .store_into(psOptions->osWHERE)
        .help(_("An attribute query in a restricted form of the queries used "
                "in the SQL WHERE statement."));

    {
        auto &group = argParser->add_mutually_exclusive_group();
        group.add_argument("-sql")
            .metavar("<statement|@filename>")
            .store_into(psOptions->osSQLStatement)
            .help(_(
                "Execute the indicated SQL statement and return the result."));

        group.add_argument("-rl")
            .store_into(psOptions->bDatasetGetNextFeature)
            .help(_("Enable random layer reading mode."));
    }

    argParser->add_argument("-dialect")
        .metavar("<dialect>")
        .store_into(psOptions->osDialect)
        .help(_("SQL dialect."));

    // Only for fuzzing
    argParser->add_argument("-rc")
        .hidden()
        .metavar("<count>")
        .store_into(psOptions->nRepeatCount)
        .help(_("Repeat count"));

    argParser->add_argument("-al")
        .store_into(psOptions->bAllLayers)
        .help(_("List all layers (used instead of having to give layer names "
                "as arguments)"));

    {
        auto &group = argParser->add_mutually_exclusive_group();
        group.add_argument("-so", "-summary")
            .store_into(psOptions->bSummaryParser)
            .help(_("Summary only: list all layers (used instead of having to "
                    "give layer names as arguments)"));

        group.add_argument("-features")
            .store_into(psOptions->bFeaturesParser)
            .help(_("Enable listing of features"));
    }

    argParser->add_argument("-limit")
        .metavar("<nb_features>")
        .store_into(psOptions->nLimit)
        .help(_("Limit the number of features per layer."));

    argParser->add_argument("-fields")
        .choices("YES", "NO")
        .metavar("YES|NO")
        .action(
            [psOptions](const std::string &s) {
                psOptions->aosOptions.SetNameValue("DISPLAY_FIELDS", s.c_str());
            })
        .help(
            _("If set to NO, the feature dump will not display field values."));

    argParser->add_argument("-geom")
        .choices("YES", "NO", "SUMMARY", "WKT", "ISO_WKT")
        .metavar("YES|NO|SUMMARY|WKT|ISO_WKT")
        .action(
            [psOptions](const std::string &s) {
                psOptions->aosOptions.SetNameValue("DISPLAY_GEOMETRY",
                                                   s.c_str());
            })
        .help(_("How to display geometries in feature dump."));

    argParser->add_argument("-oo")
        .append()
        .metavar("<NAME=VALUE>")
        .action(
            [psOptionsForBinary](const std::string &s)
            {
                if (psOptionsForBinary)
                    psOptionsForBinary->aosOpenOptions.AddString(s.c_str());
            })
        .help(_("Dataset open option (format-specific)"));

    argParser->add_argument("-nomd")
        .flag()
        .action([psOptions](const std::string &)
                { psOptions->bShowMetadata = false; })
        .help(_("Suppress metadata printing"));

    argParser->add_argument("-listmdd")
        .store_into(psOptions->bListMDD)
        .help(_("List all metadata domains available for the dataset."));

    argParser->add_argument("-mdd")
        .append()
        .metavar("<domain>")
        .action([psOptions](const std::string &s)
                { psOptions->aosExtraMDDomains.AddString(s.c_str()); })
        .help(_("List metadata in the specified domain."));

    argParser->add_argument("-nocount")
        .flag()
        .action([psOptions](const std::string &)
                { psOptions->bFeatureCount = false; })
        .help(_("Suppress feature count printing."));

    argParser->add_argument("-noextent")
        .flag()
        .action([psOptions](const std::string &)
                { psOptions->bExtent = false; })
        .help(_("Suppress spatial extent printing."));

    argParser->add_argument("-extent3D")
        .store_into(psOptions->bExtent3D)
        .help(_("Request a 3D extent to be reported."));

    argParser->add_argument("-nogeomtype")
        .flag()
        .action([psOptions](const std::string &)
                { psOptions->bGeomType = false; })
        .help(_("Suppress layer geometry type printing."));

    argParser->add_argument("-wkt_format")
        .store_into(psOptions->osWKTFormat)
        .metavar("WKT1|WKT2|WKT2_2015|WKT2_2019")
        .help(_("The WKT format used to display the SRS."));

    argParser->add_argument("-fielddomain")
        .store_into(psOptions->osFieldDomain)
        .metavar("<name>")
        .help(_("Display details about a field domain."));

    argParser->add_argument("-if")
        .append()
        .metavar("<format>")
        .action(
            [psOptionsForBinary](const std::string &s)
            {
                if (psOptionsForBinary)
                {
                    if (GDALGetDriverByName(s.c_str()) == nullptr)
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "%s is not a recognized driver", s.c_str());
                    }
                    psOptionsForBinary->aosAllowInputDrivers.AddString(
                        s.c_str());
                }
            })
        .help(_("Format/driver name(s) to try when opening the input file."));

    auto &argFilename = argParser->add_argument("filename")
                            .action(
                                [psOptionsForBinary](const std::string &s)
                                {
                                    if (psOptionsForBinary)
                                        psOptionsForBinary->osFilename = s;
                                })
                            .help(_("The data source to open."));
    if (!psOptionsForBinary)
        argFilename.nargs(argparse::nargs_pattern::optional);

    argParser->add_argument("layer")
        .remaining()
        .metavar("<layer_name>")
        .help(_("Layer name."));

    return argParser;
}

/************************************************************************/
/*                       GDALVectorInfoGetParserUsage()                 */
/************************************************************************/

std::string GDALVectorInfoGetParserUsage()
{
    try
    {
        GDALVectorInfoOptions sOptions;
        GDALVectorInfoOptionsForBinary sOptionsForBinary;
        auto argParser =
            GDALVectorInfoOptionsGetParser(&sOptions, &sOptionsForBinary);
        return argParser->usage();
    }
    catch (const std::exception &err)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unexpected exception: %s",
                 err.what());
        return std::string();
    }
}

/************************************************************************/
/*                      GDALVectorInfoOptionsNew()                      */
/************************************************************************/

/**
 * Allocates a GDALVectorInfoOptions struct.
 *
 * Note that  when this function is used a library function, and not from the
 * ogrinfo utility, a dataset name must be specified if any layer names(s) are
 * specified (if no layer name is specific, passing a dataset name is not
 * needed). That dataset name may be a dummy one, as the dataset taken into
 * account is the hDS parameter passed to GDALVectorInfo().
 * Similarly the -oo switch in a non-ogrinfo context will be ignored, and it
 * is the responsibility of the user to apply them when opening the hDS parameter
 * passed to GDALVectorInfo().
 *
 * @param papszArgv NULL terminated list of options (potentially including
 * filename and open options too), or NULL. The accepted options are the ones of
 * the <a href="/programs/ogrinfo.html">ogrinfo</a> utility.
 * @param psOptionsForBinary (output) may be NULL (and should generally be
 * NULL), otherwise (ogrinfo_bin.cpp use case) must be allocated with
 * GDALVectorInfoOptionsForBinaryNew() prior to this
 * function. Will be filled with potentially present filename, open options,
 * subdataset number...
 * @return pointer to the allocated GDALVectorInfoOptions struct. Must be freed
 * with GDALVectorInfoOptionsFree().
 *
 * @since GDAL 3.7
 */

GDALVectorInfoOptions *
GDALVectorInfoOptionsNew(char **papszArgv,
                         GDALVectorInfoOptionsForBinary *psOptionsForBinary)
{
    auto psOptions = std::make_unique<GDALVectorInfoOptions>();

    try
    {
        auto argParser =
            GDALVectorInfoOptionsGetParser(psOptions.get(), psOptionsForBinary);

        /* Special pre-processing to rewrite -fields=foo as "-fields" "FOO", and
     * same for -geom=foo. */
        CPLStringList aosArgv;
        for (CSLConstList papszIter = papszArgv; papszIter && *papszIter;
             ++papszIter)
        {
            if (STARTS_WITH(*papszIter, "-fields="))
            {
                aosArgv.AddString("-fields");
                aosArgv.AddString(
                    CPLString(*papszIter + strlen("-fields=")).toupper());
            }
            else if (STARTS_WITH(*papszIter, "-geom="))
            {
                aosArgv.AddString("-geom");
                aosArgv.AddString(
                    CPLString(*papszIter + strlen("-geom=")).toupper());
            }
            else
            {
                aosArgv.AddString(*papszIter);
            }
        }

        argParser->parse_args_without_binary_name(aosArgv.List());

        auto layers = argParser->present<std::vector<std::string>>("layer");
        if (layers)
        {
            for (const auto &layer : *layers)
            {
                psOptions->aosLayers.AddString(layer.c_str());
                psOptions->bAllLayers = false;
            }
        }

        if (auto oSpat = argParser->present<std::vector<double>>("-spat"))
        {
            OGRLinearRing oRing;
            const double dfMinX = (*oSpat)[0];
            const double dfMinY = (*oSpat)[1];
            const double dfMaxX = (*oSpat)[2];
            const double dfMaxY = (*oSpat)[3];

            oRing.addPoint(dfMinX, dfMinY);
            oRing.addPoint(dfMinX, dfMaxY);
            oRing.addPoint(dfMaxX, dfMaxY);
            oRing.addPoint(dfMaxX, dfMinY);
            oRing.addPoint(dfMinX, dfMinY);

            auto poPolygon = std::make_unique<OGRPolygon>();
            poPolygon->addRing(&oRing);
            psOptions->poSpatialFilter.reset(poPolygon.release());
        }

        if (!psOptions->osWHERE.empty() && psOptions->osWHERE[0] == '@')
        {
            GByte *pabyRet = nullptr;
            if (VSIIngestFile(nullptr, psOptions->osWHERE.substr(1).c_str(),
                              &pabyRet, nullptr, 1024 * 1024))
            {
                GDALRemoveBOM(pabyRet);
                psOptions->osWHERE = reinterpret_cast<const char *>(pabyRet);
                VSIFree(pabyRet);
            }
            else
            {
                CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s",
                         psOptions->osWHERE.substr(1).c_str());
                return nullptr;
            }
        }

        if (!psOptions->osSQLStatement.empty() &&
            psOptions->osSQLStatement[0] == '@')
        {
            GByte *pabyRet = nullptr;
            if (VSIIngestFile(nullptr,
                              psOptions->osSQLStatement.substr(1).c_str(),
                              &pabyRet, nullptr, 1024 * 1024))
            {
                GDALRemoveBOM(pabyRet);
                char *pszSQLStatement = reinterpret_cast<char *>(pabyRet);
                psOptions->osSQLStatement =
                    GDALRemoveSQLComments(pszSQLStatement);
                VSIFree(pabyRet);
            }
            else
            {
                CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s",
                         psOptions->osSQLStatement.substr(1).c_str());
                return nullptr;
            }
        }

        if (psOptionsForBinary)
        {
            psOptions->bStdoutOutput = true;
            psOptionsForBinary->osSQLStatement = psOptions->osSQLStatement;
        }

        if (psOptions->bSummaryParser)
            psOptions->bSummaryOnly = true;
        else if (psOptions->bFeaturesParser)
            psOptions->bSummaryOnly = false;

        if (!psOptions->osDialect.empty() && !psOptions->osWHERE.empty() &&
            psOptions->osSQLStatement.empty())
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "-dialect is ignored with -where. Use -sql instead");
        }

        if (psOptions->eFormat == FORMAT_JSON)
        {
            if (psOptions->aosExtraMDDomains.empty())
                psOptions->aosExtraMDDomains.AddString("all");
            psOptions->bStdoutOutput = false;
        }

        return psOptions.release();
    }
    catch (const std::exception &err)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", err.what());
        return nullptr;
    }
}
