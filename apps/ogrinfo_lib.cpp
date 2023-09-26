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

#include "cpl_port.h"
#include "cpl_json.h"
#include "ogrgeojsonreader.h"
#include "cpl_string.h"
#include "gdal_utils.h"
#include "gdal_utils_priv.h"
#include "gdal_priv.h"
#include "ogr_feature.h"
#include "ogrsf_frmts.h"
#include "ogr_geometry.h"
#include "commonutils.h"

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
    std::string osFilename{};
    std::string osWHERE{};
    CPLStringList aosLayers{};
    std::unique_ptr<OGRGeometry> poSpatialFilter;
    bool bAllLayers = false;
    std::string osSQLStatement{};
    std::string osDialect{};
    std::string osGeomField{};
    CPLStringList aosExtraMDDomains{};
    bool bListMDD = false;
    bool bShowMetadata = true;
    bool bFeatureCount = true;
    bool bExtent = true;
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

    char **papszMetadata = GDALGetMetadata(hObject, pszDomain);
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
        char **papszMDDList = GDALGetMetadataDomainList(hObject);
        char **papszIter = papszMDDList;

        CPLJSONArray metadataDomains;

        if (papszMDDList != nullptr && !bJson)
            Concat(osRet, psOptions->bStdoutOutput, "%sMetadata domains:\n",
                   pszIndent);
        while (papszIter != nullptr && *papszIter != nullptr)
        {
            if (EQUAL(*papszIter, ""))
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
                    metadataDomains.Add(*papszIter);
                else
                    Concat(osRet, psOptions->bStdoutOutput, "%s  %s\n",
                           pszIndent, *papszIter);
            }
            papszIter++;
        }
        CSLDestroy(papszMDDList);

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
        char **papszExtraMDDomainsExpanded = nullptr;

        if (EQUAL(papszExtraMDDomains[0], "all") &&
            papszExtraMDDomains[1] == nullptr)
        {
            char **papszMDDList = GDALGetMetadataDomainList(hObject);
            char **papszIter = papszMDDList;

            while (papszIter != nullptr && *papszIter != nullptr)
            {
                if (!EQUAL(*papszIter, "") && !EQUAL(*papszIter, "SUBDATASETS"))
                {
                    papszExtraMDDomainsExpanded =
                        CSLAddString(papszExtraMDDomainsExpanded, *papszIter);
                }
                papszIter++;
            }
            CSLDestroy(papszMDDList);
        }
        else
        {
            papszExtraMDDomainsExpanded = CSLDuplicate(papszExtraMDDomains);
        }

        for (int iMDD = 0; papszExtraMDDomainsExpanded != nullptr &&
                           papszExtraMDDomainsExpanded[iMDD] != nullptr;
             iMDD++)
        {
            char pszDisplayedname[256];
            snprintf(pszDisplayedname, 256, "Metadata (%s)",
                     papszExtraMDDomainsExpanded[iMDD]);
            GDALVectorInfoPrintMetadata(osRet, oMetadata, psOptions, hObject,
                                        papszExtraMDDomainsExpanded[iMDD],
                                        pszDisplayedname, pszIndent);
        }

        CSLDestroy(papszExtraMDDomainsExpanded);
    }
    GDALVectorInfoPrintMetadata(osRet, oMetadata, psOptions, hObject,
                                "SUBDATASETS", "Subdatasets", pszIndent);
}

/************************************************************************/
/*                           ReportOnLayer()                            */
/************************************************************************/

static void ReportOnLayer(CPLString &osRet, CPLJSONObject oLayer,
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

        OGREnvelope oExt;
        if (bJson || nGeomFieldCount > 1)
        {
            CPLJSONArray oGeometryFields;
            if (bJson)
                oLayer.Add("geometryFields", oGeometryFields);
            for (int iGeom = 0; iGeom < nGeomFieldCount; iGeom++)
            {
                OGRGeomFieldDefn *poGFldDefn =
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
                    if (psOptions->bExtent &&
                        poLayer->GetExtent(iGeom, &oExt, TRUE) == OGRERR_NONE)
                    {
                        CPLJSONArray oBbox;
                        oBbox.Add(oExt.MinX);
                        oBbox.Add(oExt.MinY);
                        oBbox.Add(oExt.MaxX);
                        oBbox.Add(oExt.MaxY);
                        oGeometryField.Add("extent", oBbox);
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
                            CPLErrorHandlerPusher oPusher(CPLQuietErrorHandler);
                            CPLErrorStateBackuper oCPLErrorHandlerPusher;
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
        else if (!bJson && psOptions->bExtent &&
                 poLayer->GetExtent(&oExt, TRUE) == OGRERR_NONE)
        {
            Concat(osRet, psOptions->bStdoutOutput,
                   "Extent: (%f, %f) - (%f, %f)\n", oExt.MinX, oExt.MinY,
                   oExt.MaxX, oExt.MaxY);
        }

        const auto displayExtraInfoSRS =
            [&osRet, &psOptions](const OGRSpatialReference *poSRS)
        {
            const double dfCoordinateEpoch = poSRS->GetCoordinateEpoch();
            if (dfCoordinateEpoch > 0)
            {
                std::string osCoordinateEpoch =
                    CPLSPrintf("%f", dfCoordinateEpoch);
                if (osCoordinateEpoch.find('.') != std::string::npos)
                {
                    while (osCoordinateEpoch.back() == '0')
                        osCoordinateEpoch.resize(osCoordinateEpoch.size() - 1);
                }
                Concat(osRet, psOptions->bStdoutOutput,
                       "Coordinate epoch: %s\n", osCoordinateEpoch.c_str());
            }

            const auto mapping = poSRS->GetDataAxisToSRSAxisMapping();
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
            if (bJson)
            {
                CPLJSONObject oField;
                oFields.Add(oField);
                oField.Set("name", poField->GetNameRef());
                oField.Set("type",
                           OGRFieldDefn::GetFieldTypeName(poField->GetType()));
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
                Concat(osRet, psOptions->bStdoutOutput, "%s: %s (%d.%d)",
                       poField->GetNameRef(), pszType, poField->GetWidth(),
                       poField->GetPrecision());
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
            for (auto &poFeature : poLayer)
            {
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

                    if (nGeomFields == 0)
                        oFeature.SetNull("geometry");
                    else
                    {
                        if (const auto poGeom = poFeature->GetGeometryRef())
                        {
                            char *pszSerialized =
                                wkbFlatten(poGeom->getGeometryType()) <=
                                        wkbGeometryCollection
                                    ? poGeom->exportToJson()
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
                                            ? poGeom->exportToJson()
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

    GDALDriver *poDriver = poDS->GetDriver();

    CPLString osRet;
    CPLJSONObject oRoot;
    const std::string osFilename(!psOptions->osFilename.empty()
                                     ? psOptions->osFilename
                                     : std::string(poDS->GetDescription()));

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

            for (CSLConstList papszIter = papszLayers; *papszIter != nullptr;
                 ++papszIter)
            {
                OGRLayer *poLayer = poDS->GetLayerByName(*papszIter);

                if (poLayer == nullptr)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Couldn't fetch requested layer %s.", *papszIter);
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
    }

    return VSI_STRDUP_VERBOSE(osRet);
}

/************************************************************************/
/*                      GDALVectorInfoOptionsNew()                      */
/************************************************************************/

/**
 * Allocates a GDALVectorInfoOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including
 * filename and open options too), or NULL. The accepted options are the ones of
 * the <a href="/programs/ogrinfo.html">ogrinfo</a> utility.
 * @param psOptionsForBinary (output) may be NULL (and should generally be
 * NULL), otherwise (ogrinfo_bin.cpp use case) must be allocated with
 *                           GDALVectorInfoOptionsForBinaryNew() prior to this
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
    auto psOptions = cpl::make_unique<GDALVectorInfoOptions>();
    bool bGotFilename = false;
    bool bFeatures = false;
    bool bSummary = false;

    /* -------------------------------------------------------------------- */
    /*      Parse arguments.                                                */
    /* -------------------------------------------------------------------- */
    for (int iArg = 0; papszArgv != nullptr && papszArgv[iArg] != nullptr;
         iArg++)
    {
        if (EQUAL(papszArgv[iArg], "-json"))
        {
            psOptions->eFormat = FORMAT_JSON;
            psOptions->bAllLayers = true;
            psOptions->bSummaryOnly = true;
        }
        else if (EQUAL(papszArgv[iArg], "-ro"))
        {
            if (psOptionsForBinary)
                psOptionsForBinary->bReadOnly = true;
        }
        else if (EQUAL(papszArgv[iArg], "-update"))
        {
            if (psOptionsForBinary)
                psOptionsForBinary->bUpdate = true;
        }
        else if (EQUAL(papszArgv[iArg], "-q") ||
                 EQUAL(papszArgv[iArg], "-quiet"))
        {
            psOptions->bVerbose = false;
            if (psOptionsForBinary)
                psOptionsForBinary->bVerbose = false;
        }
        else if (EQUAL(papszArgv[iArg], "-qq"))
        {
            /* Undocumented: mainly only useful for AFL testing */
            psOptions->bVerbose = false;
            if (psOptionsForBinary)
                psOptionsForBinary->bVerbose = false;
            psOptions->bSuperQuiet = true;
        }
        else if (EQUAL(papszArgv[iArg], "-fid") &&
                 papszArgv[iArg + 1] != nullptr)
        {
            psOptions->nFetchFID = CPLAtoGIntBig(papszArgv[++iArg]);
        }
        else if (EQUAL(papszArgv[iArg], "-spat") &&
                 papszArgv[iArg + 1] != nullptr &&
                 papszArgv[iArg + 2] != nullptr &&
                 papszArgv[iArg + 3] != nullptr &&
                 papszArgv[iArg + 4] != nullptr)
        {
            OGRLinearRing oRing;
            oRing.addPoint(CPLAtof(papszArgv[iArg + 1]),
                           CPLAtof(papszArgv[iArg + 2]));
            oRing.addPoint(CPLAtof(papszArgv[iArg + 1]),
                           CPLAtof(papszArgv[iArg + 4]));
            oRing.addPoint(CPLAtof(papszArgv[iArg + 3]),
                           CPLAtof(papszArgv[iArg + 4]));
            oRing.addPoint(CPLAtof(papszArgv[iArg + 3]),
                           CPLAtof(papszArgv[iArg + 2]));
            oRing.addPoint(CPLAtof(papszArgv[iArg + 1]),
                           CPLAtof(papszArgv[iArg + 2]));

            auto poPolygon = cpl::make_unique<OGRPolygon>();
            poPolygon->addRing(&oRing);
            psOptions->poSpatialFilter.reset(poPolygon.release());
            iArg += 4;
        }
        else if (EQUAL(papszArgv[iArg], "-geomfield") &&
                 papszArgv[iArg + 1] != nullptr)
        {
            psOptions->osGeomField = papszArgv[++iArg];
        }
        else if (EQUAL(papszArgv[iArg], "-where") &&
                 papszArgv[iArg + 1] != nullptr)
        {
            iArg++;
            GByte *pabyRet = nullptr;
            if (papszArgv[iArg][0] == '@' &&
                VSIIngestFile(nullptr, papszArgv[iArg] + 1, &pabyRet, nullptr,
                              1024 * 1024))
            {
                GDALRemoveBOM(pabyRet);
                psOptions->osWHERE = reinterpret_cast<char *>(pabyRet);
                VSIFree(pabyRet);
            }
            else
            {
                psOptions->osWHERE = papszArgv[iArg];
            }
        }
        else if (EQUAL(papszArgv[iArg], "-sql") &&
                 papszArgv[iArg + 1] != nullptr)
        {
            iArg++;
            GByte *pabyRet = nullptr;
            if (papszArgv[iArg][0] == '@' &&
                VSIIngestFile(nullptr, papszArgv[iArg] + 1, &pabyRet, nullptr,
                              1024 * 1024))
            {
                GDALRemoveBOM(pabyRet);
                char *pszSQLStatement = reinterpret_cast<char *>(pabyRet);
                psOptions->osSQLStatement =
                    GDALRemoveSQLComments(pszSQLStatement);
                VSIFree(pszSQLStatement);
            }
            else
            {
                psOptions->osSQLStatement = papszArgv[iArg];
            }
        }
        else if (EQUAL(papszArgv[iArg], "-dialect") &&
                 papszArgv[iArg + 1] != nullptr)
        {
            psOptions->osDialect = papszArgv[++iArg];
        }
        else if (EQUAL(papszArgv[iArg], "-rc") &&
                 papszArgv[iArg + 1] != nullptr)
        {
            // Only for fuzzing purposes!
            psOptions->nRepeatCount = atoi(papszArgv[++iArg]);
        }
        else if (EQUAL(papszArgv[iArg], "-al"))
        {
            psOptions->bAllLayers = true;
        }
        else if (EQUAL(papszArgv[iArg], "-so") ||
                 EQUAL(papszArgv[iArg], "-summary"))
        {
            bSummary = true;
        }
        else if (EQUAL(papszArgv[iArg], "-features"))
        {
            bFeatures = true;
        }
        else if (STARTS_WITH_CI(papszArgv[iArg], "-fields="))
        {
            psOptions->aosOptions.SetNameValue(
                "DISPLAY_FIELDS", papszArgv[iArg] + strlen("-fields="));
        }
        else if (STARTS_WITH_CI(papszArgv[iArg], "-geom="))
        {
            psOptions->aosOptions.SetNameValue(
                "DISPLAY_GEOMETRY", papszArgv[iArg] + strlen("-geom="));
        }
        else if (EQUAL(papszArgv[iArg], "-oo") &&
                 papszArgv[iArg + 1] != nullptr)
        {
            ++iArg;
            if (psOptionsForBinary)
                psOptionsForBinary->aosOpenOptions.AddString(papszArgv[iArg]);
        }
        else if (EQUAL(papszArgv[iArg], "-nomd"))
        {
            psOptions->bShowMetadata = false;
        }
        else if (EQUAL(papszArgv[iArg], "-listmdd"))
        {
            psOptions->bListMDD = true;
        }
        else if (EQUAL(papszArgv[iArg], "-mdd") &&
                 papszArgv[iArg + 1] != nullptr)
        {
            psOptions->aosExtraMDDomains.AddString(papszArgv[++iArg]);
        }
        else if (EQUAL(papszArgv[iArg], "-nocount"))
        {
            psOptions->bFeatureCount = false;
        }
        else if (EQUAL(papszArgv[iArg], "-noextent"))
        {
            psOptions->bExtent = false;
        }
        else if (EQUAL(papszArgv[iArg], "-nogeomtype"))
        {
            psOptions->bGeomType = false;
        }
        else if (EQUAL(papszArgv[iArg], "-rl"))
        {
            psOptions->bDatasetGetNextFeature = true;
        }
        else if (EQUAL(papszArgv[iArg], "-wkt_format") &&
                 papszArgv[iArg + 1] != nullptr)
        {
            psOptions->osWKTFormat = papszArgv[++iArg];
        }
        else if (EQUAL(papszArgv[iArg], "-fielddomain") &&
                 papszArgv[iArg + 1] != nullptr)
        {
            psOptions->osFieldDomain = papszArgv[++iArg];
        }

        else if (EQUAL(papszArgv[iArg], "-if") &&
                 papszArgv[iArg + 1] != nullptr)
        {
            iArg++;
            if (psOptionsForBinary)
            {
                if (GDALGetDriverByName(papszArgv[iArg]) == nullptr)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "%s is not a recognized driver", papszArgv[iArg]);
                }
                psOptionsForBinary->aosAllowInputDrivers.AddString(
                    papszArgv[iArg]);
            }
        }
        /* Not documented: used by gdalinfo_bin.cpp only */
        else if (EQUAL(papszArgv[iArg], "-stdout"))
            psOptions->bStdoutOutput = true;
        else if (papszArgv[iArg][0] == '-')
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Unknown option name '%s'",
                     papszArgv[iArg]);
            return nullptr;
        }
        else if (!bGotFilename)
        {
            bGotFilename = true;
            psOptions->osFilename = papszArgv[iArg];
            if (psOptionsForBinary)
                psOptionsForBinary->osFilename = psOptions->osFilename;
        }
        else
        {
            psOptions->aosLayers.AddString(papszArgv[iArg]);
            psOptions->bAllLayers = false;
        }
    }

    if (bSummary && bFeatures)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "-so or -summary are incompatible with -features");
        return nullptr;
    }

    if (bSummary)
        psOptions->bSummaryOnly = true;
    else if (bFeatures)
        psOptions->bSummaryOnly = false;

    if (psOptionsForBinary)
        psOptionsForBinary->osSQLStatement = psOptions->osSQLStatement;

    if (!psOptions->osDialect.empty() && !psOptions->osWHERE.empty() &&
        psOptions->osSQLStatement.empty())
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "-dialect is ignored with -where. Use -sql instead");
    }

    if (psOptions->bDatasetGetNextFeature && !psOptions->osSQLStatement.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "-rl is incompatible with -sql");
        return nullptr;
    }

    if (psOptions->eFormat == FORMAT_JSON)
    {
        if (psOptions->aosExtraMDDomains.size() == 0)
            psOptions->aosExtraMDDomains.AddString("all");
        psOptions->bStdoutOutput = false;
    }

    return psOptions.release();
}
