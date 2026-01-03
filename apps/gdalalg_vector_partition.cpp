/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "partition" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_partition.h"

#include "cpl_vsi.h"
#include "cpl_mem_cache.h"

#include <algorithm>
#include <set>
#include <string_view>

#ifndef _
#define _(x) (x)
#endif

//! @cond Doxygen_Suppress

constexpr int DIRECTORY_CREATION_MODE = 0755;

constexpr const char *NULL_MARKER = "__HIVE_DEFAULT_PARTITION__";

constexpr const char *DEFAULT_PATTERN_HIVE = "part_%010d";
constexpr const char *DEFAULT_PATTERN_FLAT = "{LAYER_NAME}_{FIELD_VALUE}_%010d";

constexpr char DIGIT_ZERO = '0';

/************************************************************************/
/*                        GetConstructorOptions()                       */
/************************************************************************/

/* static */
GDALVectorPartitionAlgorithm::ConstructorOptions
GDALVectorPartitionAlgorithm::GetConstructorOptions(bool standaloneStep)
{
    GDALVectorPartitionAlgorithm::ConstructorOptions options;
    options.SetStandaloneStep(standaloneStep);
    options.SetAddInputLayerNameArgument(false);
    options.SetAddDefaultArguments(false);
    return options;
}

/************************************************************************/
/*      GDALVectorPartitionAlgorithm::GDALVectorPartitionAlgorithm()    */
/************************************************************************/

GDALVectorPartitionAlgorithm::GDALVectorPartitionAlgorithm(bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      GetConstructorOptions(standaloneStep))
{
    if (standaloneStep)
    {
        AddVectorInputArgs(false);
    }
    AddProgressArg();

    AddArg(GDAL_ARG_NAME_OUTPUT, 'o', _("Output directory"), &m_output)
        .SetRequired()
        .SetIsInput()
        .SetMinCharCount(1)
        .SetPositional();

    constexpr const char *OVERWRITE_APPEND_EXCLUSION_GROUP = "overwrite-append";
    AddOverwriteArg(&m_overwrite)
        .SetMutualExclusionGroup(OVERWRITE_APPEND_EXCLUSION_GROUP);
    AddAppendLayerArg(&m_appendLayer)
        .SetMutualExclusionGroup(OVERWRITE_APPEND_EXCLUSION_GROUP);
    AddUpdateArg(&m_update).SetHidden();

    AddOutputFormatArg(&m_format, /* bStreamAllowed = */ false,
                       /* bGDALGAllowed = */ false)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_DCAP_VECTOR, GDAL_DCAP_CREATE});
    AddCreationOptionsArg(&m_creationOptions);
    AddLayerCreationOptionsArg(&m_layerCreationOptions);

    AddArg("field", 0,
           _("Attribute or geometry field(s) on which to partition"), &m_fields)
        .SetRequired();
    AddArg("scheme", 0, _("Partitioning scheme"), &m_scheme)
        .SetChoices(SCHEME_HIVE, SCHEME_FLAT)
        .SetDefault(m_scheme);
    AddArg("pattern", 0,
           _("Filename pattern ('part_%010d' for scheme=hive, "
             "'{LAYER_NAME}_{FIELD_VALUE}_%010d' for scheme=flat)"),
           &m_pattern)
        .SetMinCharCount(1)
        .AddValidationAction(
            [this]()
            {
                if (!m_pattern.empty())
                {
                    const auto nPercentPos = m_pattern.find('%');
                    if (nPercentPos == std::string::npos)
                    {
                        ReportError(CE_Failure, CPLE_IllegalArg, "%s",
                                    "Missing '%' character in pattern");
                        return false;
                    }
                    if (nPercentPos + 1 < m_pattern.size() &&
                        m_pattern.find('%', nPercentPos + 1) !=
                            std::string::npos)
                    {
                        ReportError(
                            CE_Failure, CPLE_IllegalArg, "%s",
                            "A single '%' character is expected in pattern");
                        return false;
                    }
                    bool percentFound = false;
                    for (size_t i = nPercentPos + 1; i < m_pattern.size(); ++i)
                    {
                        if (m_pattern[i] >= DIGIT_ZERO && m_pattern[i] <= '9')
                        {
                            // ok
                        }
                        else if (m_pattern[i] == 'd')
                        {
                            percentFound = true;
                            break;
                        }
                        else
                        {
                            break;
                        }
                    }
                    if (!percentFound)
                    {
                        ReportError(
                            CE_Failure, CPLE_IllegalArg, "%s",
                            "pattern value must include a single "
                            "'%[0]?[1-9]?[0]?d' part number specification");
                        return false;
                    }
                    m_partDigitCount =
                        atoi(m_pattern.c_str() + nPercentPos + 1);
                    if (m_partDigitCount > 10)
                    {
                        ReportError(CE_Failure, CPLE_IllegalArg,
                                    "Number of digits in part number "
                                    "specifiation should be in [1,10] range");
                        return false;
                    }
                    m_partDigitLeadingZeroes =
                        m_pattern[nPercentPos + 1] == DIGIT_ZERO;
                }
                return true;
            });
    AddArg("feature-limit", 0, _("Maximum number of features per file"),
           &m_featureLimit)
        .SetMinValueExcluded(0);
    AddArg("max-file-size", 0,
           _("Maximum file size (MB or GB suffix can be used)"),
           &m_maxFileSizeStr)
        .AddValidationAction(
            [this]()
            {
                bool ok;
                {
                    CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
                    ok = CPLParseMemorySize(m_maxFileSizeStr.c_str(),
                                            &m_maxFileSize,
                                            nullptr) == CE_None &&
                         m_maxFileSize > 0;
                }
                if (!ok)
                {
                    ReportError(CE_Failure, CPLE_IllegalArg,
                                "Invalid value for max-file-size");
                    return false;
                }
                else if (m_maxFileSize < 1024 * 1024)
                {
                    ReportError(CE_Failure, CPLE_IllegalArg,
                                "max-file-size should be at least one MB");
                    return false;
                }
                return true;
            });
    AddArg("omit-partitioned-field", 0,
           _("Whether to omit partitioned fields from target layer definition"),
           &m_omitPartitionedFields);
    AddArg("skip-errors", 0, _("Skip errors when writing features"),
           &m_skipErrors);

    // Hidden for now

    AddArg("max-cache-size", 0,
           _("Maximum number of datasets simultaneously opened"),
           &m_maxCacheSize)
        .SetMinValueIncluded(0)  // 0 = unlimited
        .SetDefault(m_maxCacheSize)
        .SetHidden();

    AddArg("transaction-size", 0,
           _("Maximum number of features per transaction"), &m_transactionSize)
        .SetMinValueIncluded(1)
        .SetDefault(m_transactionSize)
        .SetHidden();
}

/************************************************************************/
/*                              PercentEncode()                         */
/************************************************************************/

static void PercentEncode(std::string &out, const std::string_view &s)
{
    for (unsigned char c : s)
    {
        if (c > 32 && c <= 127 && c != ':' && c != '/' && c != '\\' &&
            c != '>' && c != '%' && c != '=')
        {
            out += c;
        }
        else
        {
            out += CPLSPrintf("%%%02X", c);
        }
    }
}

static std::string PercentEncode(const std::string_view &s)
{
    std::string out;
    PercentEncode(out, s);
    return out;
}

/************************************************************************/
/*                       GetEstimatedFeatureSize()                      */
/************************************************************************/

static size_t GetEstimatedFeatureSize(
    const OGRFeature *poFeature, const std::vector<bool> &abPartitionedFields,
    const bool omitPartitionedFields,
    const std::vector<OGRFieldType> &aeSrcFieldTypes, bool bIsBinary)
{
    size_t nSize = 16;
    const int nFieldCount = poFeature->GetFieldCount();
    nSize += 4 * nFieldCount;
    for (int i = 0; i < nFieldCount; ++i)
    {
        if (!(omitPartitionedFields && abPartitionedFields[i]))
        {
            switch (aeSrcFieldTypes[i])
            {
                case OFTInteger:
                    nSize += bIsBinary ? sizeof(int) : 11;
                    break;
                case OFTInteger64:
                    nSize += bIsBinary ? sizeof(int64_t) : 21;
                    break;
                case OFTReal:
                    // Decimal representation
                    nSize += bIsBinary ? sizeof(double) : 15;
                    break;
                case OFTString:
                    nSize += 4 + strlen(poFeature->GetFieldAsStringUnsafe(i));
                    break;
                case OFTBinary:
                {
                    int nCount = 0;
                    CPL_IGNORE_RET_VAL(poFeature->GetFieldAsBinary(i, &nCount));
                    nSize += 4 + nCount;
                    break;
                }
                case OFTIntegerList:
                {
                    int nCount = 0;
                    CPL_IGNORE_RET_VAL(
                        poFeature->GetFieldAsIntegerList(i, &nCount));
                    nSize += 4 + (bIsBinary ? sizeof(int) : 11) * nCount;
                    break;
                }
                case OFTInteger64List:
                {
                    int nCount = 0;
                    CPL_IGNORE_RET_VAL(
                        poFeature->GetFieldAsInteger64List(i, &nCount));
                    nSize += 4 + (bIsBinary ? sizeof(int64_t) : 21) * nCount;
                    break;
                }
                case OFTRealList:
                {
                    int nCount = 0;
                    CPL_IGNORE_RET_VAL(
                        poFeature->GetFieldAsDoubleList(i, &nCount));
                    nSize += 4 + (bIsBinary ? sizeof(double) : 15) * nCount;
                    break;
                }
                case OFTStringList:
                {
                    CSLConstList papszIter = poFeature->GetFieldAsStringList(i);
                    nSize += 4;
                    for (; papszIter && *papszIter; ++papszIter)
                        nSize += 4 + strlen(*papszIter);
                    break;
                }
                case OFTTime:
                    // Decimal representation
                    nSize += 4 + sizeof("HH:MM:SS.sss");
                    break;
                case OFTDate:
                    // Decimal representation
                    nSize += 4 + sizeof("YYYY-MM-DD");
                    break;
                case OFTDateTime:
                    // Decimal representation
                    nSize += 4 + sizeof("YYYY-MM-DDTHH:MM:SS.sss+HH:MM");
                    break;
                case OFTWideString:
                case OFTWideStringList:
                    break;
            }
        }
    }

    const int nGeomFieldCount = poFeature->GetGeomFieldCount();
    nSize += 4 * nGeomFieldCount;
    for (int i = 0; i < nGeomFieldCount; ++i)
    {
        const auto poGeom = poFeature->GetGeomFieldRef(i);
        if (poGeom)
            nSize += poGeom->WkbSize();
    }

    return nSize;
}

/************************************************************************/
/*                      GetCurrentOutputLayer()                         */
/************************************************************************/

constexpr int MIN_FILE_SIZE = 65536;

namespace
{
struct Layer
{
    bool bUseTransactions = false;
    std::unique_ptr<GDALDataset> poDS{};
    OGRLayer *poLayer = nullptr;
    GIntBig nFeatureCount = 0;
    int nFileCounter = 1;
    GIntBig nFileSize = MIN_FILE_SIZE;

    ~Layer()
    {
        if (poDS)
        {
            CPL_IGNORE_RET_VAL(poDS->CommitTransaction());
        }
    }
};
}  // namespace

static bool GetCurrentOutputLayer(
    GDALAlgorithm *const alg, const OGRFeatureDefn *const poSrcFeatureDefn,
    OGRLayer *const poSrcLayer, const std::string &osKey,
    const std::vector<OGRwkbGeometryType> &aeGeomTypes,
    const std::string &osLayerDir, const std::string &osScheme,
    const std::string &osPatternIn, bool partDigitLeadingZeroes,
    size_t partDigitCount, const int featureLimit, const GIntBig maxFileSize,
    const bool omitPartitionedFields,
    const std::vector<bool> &abPartitionedFields,
    const std::vector<bool> &abPartitionedGeomFields, const char *pszExtension,
    GDALDriver *const poOutDriver, const CPLStringList &datasetCreationOptions,
    const CPLStringList &layerCreationOptions,
    const OGRFeatureDefn *const poFeatureDefnWithoutPartitionedFields,
    const int nSpatialIndexPerFeatureConstant,
    const int nSpatialIndexPerLog2FeatureCountConstant, bool bUseTransactions,
    lru11::Cache<std::string, std::shared_ptr<Layer>> &oCacheOutputLayer,
    std::shared_ptr<Layer> &outputLayer)
{
    const std::string osPattern =
        !osPatternIn.empty() ? osPatternIn
        : osScheme == GDALVectorPartitionAlgorithm::SCHEME_HIVE
            ? DEFAULT_PATTERN_HIVE
            : DEFAULT_PATTERN_FLAT;

    bool bLimitReached = false;
    bool bOpenOrCreateNewFile = true;
    if (oCacheOutputLayer.tryGet(osKey, outputLayer))
    {
        if (featureLimit > 0 && outputLayer->nFeatureCount >= featureLimit)
        {
            bLimitReached = true;
        }
        else if (maxFileSize > 0 &&
                 outputLayer->nFileSize +
                         (nSpatialIndexPerFeatureConstant > 0
                              ? (outputLayer->nFeatureCount *
                                     nSpatialIndexPerFeatureConstant +
                                 static_cast<int>(std::ceil(
                                     log2(outputLayer->nFeatureCount)))) *
                                    nSpatialIndexPerLog2FeatureCountConstant
                              : 0) >=
                     maxFileSize)
        {
            bLimitReached = true;
        }
        else
        {
            bOpenOrCreateNewFile = false;
        }
    }
    else
    {
        outputLayer = std::make_unique<Layer>();
        outputLayer->bUseTransactions = bUseTransactions;
    }

    const auto SubstituteVariables = [&osKey, poSrcLayer](const std::string &s)
    {
        CPLString ret(s);
        ret.replaceAll("{LAYER_NAME}",
                       PercentEncode(poSrcLayer->GetDescription()));

        if (ret.find("{FIELD_VALUE}") != std::string::npos)
        {
            std::string fieldValue;
            const CPLStringList aosTokens(
                CSLTokenizeString2(osKey.c_str(), "/", 0));
            for (int i = 0; i < aosTokens.size(); ++i)
            {
                const CPLStringList aosFieldNameValue(
                    CSLTokenizeString2(aosTokens[i], "=", 0));
                if (!fieldValue.empty())
                    fieldValue += '_';
                fieldValue +=
                    aosFieldNameValue.size() == 2
                        ? (strcmp(aosFieldNameValue[1], NULL_MARKER) == 0
                               ? std::string("__NULL__")
                               : aosFieldNameValue[1])
                        : std::string("__EMPTY__");
            }
            ret.replaceAll("{FIELD_VALUE}", fieldValue);
        }
        return ret;
    };

    const auto nPercentPos = osPattern.find('%');
    CPLAssert(nPercentPos !=
              std::string::npos);  // checked by validation action
    const std::string osPatternPrefix =
        SubstituteVariables(osPattern.substr(0, nPercentPos));
    const auto nAfterDPos = osPattern.find('d', nPercentPos + 1) + 1;
    const std::string osPatternSuffix =
        nAfterDPos < osPattern.size()
            ? SubstituteVariables(osPattern.substr(nAfterDPos))
            : std::string();

    const auto GetBasenameFromCounter = [partDigitCount, partDigitLeadingZeroes,
                                         &osPatternPrefix,
                                         &osPatternSuffix](int nCounter)
    {
        const std::string sCounter(CPLSPrintf("%d", nCounter));
        std::string s(osPatternPrefix);
        if (sCounter.size() < partDigitCount)
        {
            s += std::string(partDigitCount - sCounter.size(),
                             partDigitLeadingZeroes ? DIGIT_ZERO : ' ');
        }
        s += sCounter;
        s += osPatternSuffix;
        return s;
    };

    if (bOpenOrCreateNewFile)
    {
        std::string osDatasetDir =
            osScheme == GDALVectorPartitionAlgorithm::SCHEME_HIVE
                ? CPLFormFilenameSafe(osLayerDir.c_str(), osKey.c_str(),
                                      nullptr)
                : osLayerDir;
        outputLayer->nFeatureCount = 0;

        bool bCreateNewFile = true;
        if (bLimitReached)
        {
            ++outputLayer->nFileCounter;
        }
        else
        {
            outputLayer->nFileCounter = 1;

            VSIStatBufL sStat;
            if (VSIStatL(osDatasetDir.c_str(), &sStat) != 0)
            {
                if (VSIMkdirRecursive(osDatasetDir.c_str(),
                                      DIRECTORY_CREATION_MODE) != 0)
                {
                    alg->ReportError(CE_Failure, CPLE_AppDefined,
                                     "Cannot create directory '%s'",
                                     osDatasetDir.c_str());
                    return false;
                }
            }

            int nMaxCounter = 0;
            std::unique_ptr<VSIDIR, decltype(&VSICloseDir)> psDir(
                VSIOpenDir(osDatasetDir.c_str(), 0, nullptr), VSICloseDir);
            if (psDir)
            {
                while (const auto *psEntry = VSIGetNextDirEntry(psDir.get()))
                {
                    const std::string osName(
                        CPLGetBasenameSafe(psEntry->pszName));
                    if (cpl::starts_with(osName, osPatternPrefix) &&
                        cpl::ends_with(osName, osPatternSuffix))
                    {
                        nMaxCounter = std::max(
                            nMaxCounter,
                            atoi(osName
                                     .substr(osPatternPrefix.size(),
                                             osName.size() -
                                                 osPatternPrefix.size() -
                                                 osPatternSuffix.size())
                                     .c_str()));
                    }
                }
            }

            if (nMaxCounter > 0)
            {
                outputLayer->nFileCounter = nMaxCounter;

                const std::string osFilename = CPLFormFilenameSafe(
                    osDatasetDir.c_str(),
                    GetBasenameFromCounter(nMaxCounter).c_str(), pszExtension);
                auto poDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
                    osFilename.c_str(),
                    GDAL_OF_VECTOR | GDAL_OF_UPDATE | GDAL_OF_VERBOSE_ERROR));
                if (!poDS)
                    return false;
                auto poDstLayer = poDS->GetLayer(0);
                if (!poDstLayer)
                {
                    alg->ReportError(CE_Failure, CPLE_AppDefined,
                                     "No layer in %s", osFilename.c_str());
                    return false;
                }

                // Check if the existing output layer has the expected layer
                // definition
                const auto poRefFeatureDefn =
                    poFeatureDefnWithoutPartitionedFields
                        ? poFeatureDefnWithoutPartitionedFields
                        : poSrcFeatureDefn;
                const auto poDstFeatureDefn = poDstLayer->GetLayerDefn();
                bool bSameDefinition = (poDstFeatureDefn->GetFieldCount() ==
                                        poRefFeatureDefn->GetFieldCount());
                for (int i = 0;
                     bSameDefinition && i < poRefFeatureDefn->GetFieldCount();
                     ++i)
                {
                    const auto poRefFieldDefn =
                        poRefFeatureDefn->GetFieldDefn(i);
                    const auto poDstFieldDefn =
                        poDstFeatureDefn->GetFieldDefn(i);
                    bSameDefinition =
                        EQUAL(poRefFieldDefn->GetNameRef(),
                              poDstFieldDefn->GetNameRef()) &&
                        poRefFieldDefn->GetType() == poDstFieldDefn->GetType();
                }
                bSameDefinition =
                    bSameDefinition && (poDstFeatureDefn->GetGeomFieldCount() ==
                                        poRefFeatureDefn->GetGeomFieldCount());
                for (int i = 0; bSameDefinition &&
                                i < poRefFeatureDefn->GetGeomFieldCount();
                     ++i)
                {
                    const auto poRefFieldDefn =
                        poRefFeatureDefn->GetGeomFieldDefn(i);
                    const auto poDstFieldDefn =
                        poDstFeatureDefn->GetGeomFieldDefn(i);
                    bSameDefinition =
                        (poRefFeatureDefn->GetGeomFieldCount() == 1 ||
                         EQUAL(poRefFieldDefn->GetNameRef(),
                               poDstFieldDefn->GetNameRef()));
                }

                if (!bSameDefinition)
                {
                    alg->ReportError(CE_Failure, CPLE_AppDefined,
                                     "%s does not have the same feature "
                                     "definition as the source layer",
                                     osFilename.c_str());
                    return false;
                }

                if (VSIStatL(osFilename.c_str(), &sStat) == 0)
                {
                    outputLayer->nFileSize = sStat.st_size;
                }

                GIntBig nFeatureCount = 0;
                if (((featureLimit == 0 ||
                      (nFeatureCount = poDstLayer->GetFeatureCount(true)) <
                          featureLimit)) &&
                    (maxFileSize == 0 || outputLayer->nFileSize < maxFileSize))
                {
                    bCreateNewFile = false;
                    outputLayer->poDS = std::move(poDS);
                    outputLayer->poLayer = poDstLayer;
                    outputLayer->nFeatureCount = nFeatureCount;

                    if (bUseTransactions)
                    {
                        if (outputLayer->poDS->StartTransaction() !=
                            OGRERR_NONE)
                        {
                            return false;
                        }
                    }
                }
                else
                {
                    ++outputLayer->nFileCounter;
                }
            }
        }

        if (bCreateNewFile)
        {
            outputLayer->nFileSize = MIN_FILE_SIZE;

            if (bUseTransactions && outputLayer->poDS &&
                outputLayer->poDS->CommitTransaction() != OGRERR_NONE)
            {
                return false;
            }

            const std::string osFilename = CPLFormFilenameSafe(
                osDatasetDir.c_str(),
                GetBasenameFromCounter(outputLayer->nFileCounter).c_str(),
                pszExtension);
            outputLayer->poDS.reset(
                poOutDriver->Create(osFilename.c_str(), 0, 0, 0, GDT_Unknown,
                                    datasetCreationOptions.List()));
            if (!outputLayer->poDS)
            {
                alg->ReportError(CE_Failure, CPLE_AppDefined,
                                 "Cannot create dataset '%s'",
                                 osFilename.c_str());
                return false;
            }

            CPLStringList modLayerCreationOptions(layerCreationOptions);
            const char *pszSrcFIDColumn = poSrcLayer->GetFIDColumn();
            if (pszSrcFIDColumn[0])
            {
                const char *pszLCO = poOutDriver->GetMetadataItem(
                    GDAL_DS_LAYER_CREATIONOPTIONLIST);
                if (pszLCO && strstr(pszLCO, "'FID'") &&
                    layerCreationOptions.FetchNameValue("FID") == nullptr)
                    modLayerCreationOptions.SetNameValue("FID",
                                                         pszSrcFIDColumn);
            }

            std::unique_ptr<OGRGeomFieldDefn> poFirstGeomFieldDefn;
            if (poSrcFeatureDefn->GetGeomFieldCount())
            {
                poFirstGeomFieldDefn = std::make_unique<OGRGeomFieldDefn>(
                    *poSrcFeatureDefn->GetGeomFieldDefn(0));
                if (abPartitionedGeomFields[0])
                {
                    if (aeGeomTypes[0] == wkbNone)
                        poFirstGeomFieldDefn.reset();
                    else
                        whileUnsealing(poFirstGeomFieldDefn.get())
                            ->SetType(aeGeomTypes[0]);
                }
            }
            auto poLayer = outputLayer->poDS->CreateLayer(
                poSrcLayer->GetDescription(), poFirstGeomFieldDefn.get(),
                modLayerCreationOptions.List());
            if (!poLayer)
            {
                return false;
            }
            outputLayer->poLayer = poLayer;
            int iField = -1;
            for (const auto *poFieldDefn : poSrcFeatureDefn->GetFields())
            {
                ++iField;
                if (omitPartitionedFields && abPartitionedFields[iField])
                    continue;
                if (poLayer->CreateField(poFieldDefn) != OGRERR_NONE)
                {
                    alg->ReportError(CE_Failure, CPLE_AppDefined,
                                     "Cannot create field '%s'",
                                     poFieldDefn->GetNameRef());
                    return false;
                }
            }
            int iGeomField = -1;
            for (const auto *poGeomFieldDefn :
                 poSrcFeatureDefn->GetGeomFields())
            {
                ++iGeomField;
                if (iGeomField > 0)
                {
                    OGRGeomFieldDefn oClone(poGeomFieldDefn);
                    if (abPartitionedGeomFields[iGeomField])
                    {
                        if (aeGeomTypes[iGeomField] == wkbNone)
                            continue;
                        whileUnsealing(&oClone)->SetType(
                            aeGeomTypes[iGeomField]);
                    }
                    if (poLayer->CreateGeomField(&oClone) != OGRERR_NONE)
                    {
                        alg->ReportError(CE_Failure, CPLE_AppDefined,
                                         "Cannot create geometry field '%s'",
                                         poGeomFieldDefn->GetNameRef());
                        return false;
                    }
                }
            }

            if (bUseTransactions)
            {
                if (outputLayer->poDS->StartTransaction() != OGRERR_NONE)
                    return false;
            }
        }

        const auto nCounter = CPLGetErrorCounter();
        oCacheOutputLayer.insert(osKey, outputLayer);
        // In case insertion caused an eviction and old dataset
        // flushing caused an error
        if (CPLGetErrorCounter() != nCounter)
            return false;
    }

    return true;
}

/************************************************************************/
/*                GDALVectorPartitionAlgorithm::RunStep()               */
/************************************************************************/

bool GDALVectorPartitionAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    auto poOutDriver = poSrcDS->GetDriver();
    const char *pszExtensions =
        poOutDriver ? poOutDriver->GetMetadataItem(GDAL_DMD_EXTENSIONS)
                    : nullptr;
    if (m_format.empty())
    {
        if (!pszExtensions)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot infer output format. Please specify "
                        "'output-format' argument");
            return false;
        }
    }
    else
    {
        poOutDriver = GetGDALDriverManager()->GetDriverByName(m_format.c_str());
        if (!(poOutDriver && (pszExtensions = poOutDriver->GetMetadataItem(
                                  GDAL_DMD_EXTENSIONS)) != nullptr))
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Output driver has no known file extension");
            return false;
        }
    }
    CPLAssert(poOutDriver);

    const bool bFormatSupportsAppend =
        poOutDriver->GetMetadataItem(GDAL_DCAP_UPDATE) ||
        poOutDriver->GetMetadataItem(GDAL_DCAP_APPEND);
    if (m_appendLayer && !bFormatSupportsAppend)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Driver '%s' does not support update",
                    poOutDriver->GetDescription());
        return false;
    }

    const bool bParquetOutput = EQUAL(poOutDriver->GetDescription(), "PARQUET");
    if (bParquetOutput && m_scheme == SCHEME_HIVE)
    {
        // Required for Parquet Hive partitioning
        m_omitPartitionedFields = true;
    }

    const CPLStringList aosExtensions(CSLTokenizeString(pszExtensions));
    const char *pszExtension = aosExtensions[0];

    const CPLStringList datasetCreationOptions(m_creationOptions);
    const CPLStringList layerCreationOptions(m_layerCreationOptions);

    // We don't have driver metadata for that (and that would be a bit
    // tricky because some formats are half-text/half-binary), so...
    const bool bOutputFormatIsBinary =
        bParquetOutput || EQUAL(poOutDriver->GetDescription(), "GPKG") ||
        EQUAL(poOutDriver->GetDescription(), "SQLite") ||
        EQUAL(poOutDriver->GetDescription(), "FlatGeoBuf");

    // Below values have been experimentally determined and are not based
    // on rocket science...
    int nSpatialIndexPerFeatureConstant = 0;
    int nSpatialIndexPerLog2FeatureCountConstant = 0;
    if (CPLTestBool(
            layerCreationOptions.FetchNameValueDef("SPATIAL_INDEX", "YES")))
    {
        if (EQUAL(poOutDriver->GetDescription(), "GPKG"))
        {
            nSpatialIndexPerFeatureConstant =
                static_cast<int>(sizeof(double) * 4 + sizeof(uint32_t));
            nSpatialIndexPerLog2FeatureCountConstant = 1;
        }
        else if (EQUAL(poOutDriver->GetDescription(), "FlatGeoBuf"))
        {
            nSpatialIndexPerFeatureConstant = 1;
            nSpatialIndexPerLog2FeatureCountConstant =
                static_cast<int>(sizeof(double) * 4 + sizeof(uint64_t));
        }
    }

    const bool bUseTransactions =
        (EQUAL(poOutDriver->GetDescription(), "GPKG") ||
         EQUAL(poOutDriver->GetDescription(), "SQLite")) &&
        !m_skipErrors;

    VSIStatBufL sStat;
    if (VSIStatL(m_output.c_str(), &sStat) == 0)
    {
        if (m_overwrite)
        {
            bool emptyDir = true;
            bool hasDirLevel1WithEqual = false;

            // Do a sanity check to verify that this looks like a directory
            // generated by partition

            if (m_scheme == SCHEME_HIVE)
            {
                std::unique_ptr<VSIDIR, decltype(&VSICloseDir)> psDir(
                    VSIOpenDir(m_output.c_str(), -1, nullptr), VSICloseDir);
                if (psDir)
                {
                    while (const auto *psEntry =
                               VSIGetNextDirEntry(psDir.get()))
                    {
                        emptyDir = false;
                        if (VSI_ISDIR(psEntry->nMode))
                        {
                            std::string_view v(psEntry->pszName);
                            if (std::count_if(v.begin(), v.end(),
                                              [](char c) {
                                                  return c == '/' || c == '\\';
                                              }) == 1)
                            {
                                const auto nPosDirSep = v.find_first_of("/\\");
                                const auto nPosEqual = v.find('=', nPosDirSep);
                                if (nPosEqual != std::string::npos)
                                {
                                    hasDirLevel1WithEqual = true;
                                    break;
                                }
                            }
                        }
                    }
                }

                if (!hasDirLevel1WithEqual && !emptyDir)
                {
                    ReportError(
                        CE_Failure, CPLE_AppDefined,
                        "Rejecting removing '%s' as it does not look like "
                        "a directory generated by this utility. If you are "
                        "sure, remove it manually and re-run",
                        m_output.c_str());
                    return false;
                }
            }
            else
            {
                bool hasSubDir = false;
                std::unique_ptr<VSIDIR, decltype(&VSICloseDir)> psDir(
                    VSIOpenDir(m_output.c_str(), 0, nullptr), VSICloseDir);
                if (psDir)
                {
                    while (const auto *psEntry =
                               VSIGetNextDirEntry(psDir.get()))
                    {
                        if (VSI_ISDIR(psEntry->nMode))
                        {
                            hasSubDir = true;
                            break;
                        }
                    }
                }

                if (hasSubDir)
                {
                    ReportError(
                        CE_Failure, CPLE_AppDefined,
                        "Rejecting removing '%s' as it does not look like "
                        "a directory generated by this utility. If you are "
                        "sure, remove it manually and re-run",
                        m_output.c_str());
                    return false;
                }
            }

            if (VSIRmdirRecursive(m_output.c_str()) != 0)
            {
                ReportError(CE_Failure, CPLE_AppDefined, "Cannot remove '%s'",
                            m_output.c_str());
                return false;
            }
        }
        else if (!m_appendLayer)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "'%s' already exists. Specify --overwrite or --append",
                        m_output.c_str());
            return false;
        }
    }
    if (VSIStatL(m_output.c_str(), &sStat) != 0)
    {
        if (VSIMkdir(m_output.c_str(), DIRECTORY_CREATION_MODE) != 0)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot create directory '%s'", m_output.c_str());
            return false;
        }
    }

    for (OGRLayer *poSrcLayer : poSrcDS->GetLayers())
    {
        const std::string osLayerDir =
            m_scheme == SCHEME_HIVE
                ? CPLFormFilenameSafe(
                      m_output.c_str(),
                      PercentEncode(poSrcLayer->GetDescription()).c_str(),
                      nullptr)
                : m_output;
        if (m_scheme == SCHEME_HIVE &&
            VSIStatL(osLayerDir.c_str(), &sStat) != 0)
        {
            if (VSIMkdir(osLayerDir.c_str(), DIRECTORY_CREATION_MODE) != 0)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Cannot create directory '%s'", osLayerDir.c_str());
                return false;
            }
        }

        const auto poSrcFeatureDefn = poSrcLayer->GetLayerDefn();

        struct Field
        {
            int nIdx{};
            bool bIsGeom = false;
            std::string encodedFieldName{};
            OGRFieldType eType{};
        };

        std::vector<Field> asFields;
        std::vector<bool> abPartitionedFields(poSrcFeatureDefn->GetFieldCount(),
                                              false);
        std::vector<bool> abPartitionedGeomFields(
            poSrcFeatureDefn->GetGeomFieldCount(), false);
        for (const std::string &fieldName : m_fields)
        {
            int nIdx = poSrcFeatureDefn->GetFieldIndex(fieldName.c_str());
            if (nIdx < 0)
            {
                if (fieldName == "OGR_GEOMETRY" &&
                    poSrcFeatureDefn->GetGeomFieldCount() > 0)
                    nIdx = 0;
                else
                    nIdx =
                        poSrcFeatureDefn->GetGeomFieldIndex(fieldName.c_str());
                if (nIdx < 0)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Cannot find field '%s' in layer '%s'",
                                fieldName.c_str(),
                                poSrcLayer->GetDescription());
                    return false;
                }
                else
                {
                    abPartitionedGeomFields[nIdx] = true;
                    Field f;
                    f.nIdx = nIdx;
                    f.bIsGeom = true;
                    if (fieldName.empty())
                        f.encodedFieldName = "OGR_GEOMETRY";
                    else
                        f.encodedFieldName = PercentEncode(fieldName);
                    asFields.push_back(std::move(f));
                }
            }
            else
            {
                const auto eType =
                    poSrcFeatureDefn->GetFieldDefn(nIdx)->GetType();
                if (eType != OFTString && eType != OFTInteger &&
                    eType != OFTInteger64)
                {
                    ReportError(
                        CE_Failure, CPLE_NotSupported,
                        "Field '%s' not valid for partitioning. Only fields of "
                        "type String, Integer or Integer64, or geometry fields,"
                        " are accepted",
                        fieldName.c_str());
                    return false;
                }
                abPartitionedFields[nIdx] = true;
                Field f;
                f.nIdx = nIdx;
                f.bIsGeom = false;
                f.encodedFieldName = PercentEncode(fieldName);
                f.eType = eType;
                asFields.push_back(std::move(f));
            }
        }

        std::vector<OGRFieldType> aeSrcFieldTypes;
        for (const auto *poFieldDefn : poSrcFeatureDefn->GetFields())
        {
            aeSrcFieldTypes.push_back(poFieldDefn->GetType());
        }

        std::unique_ptr<OGRFeatureDefn> poFeatureDefnWithoutPartitionedFields(
            poSrcFeatureDefn->Clone());
        std::vector<int> anMapForSetFrom;
        if (m_omitPartitionedFields)
        {
            // Sort fields by descending index (so we can delete them easily)
            std::vector<Field> sortedFields(asFields);
            std::sort(sortedFields.begin(), sortedFields.end(),
                      [](const Field &a, const Field &b)
                      { return a.nIdx > b.nIdx; });
            for (const auto &field : sortedFields)
            {
                if (!field.bIsGeom)
                    poFeatureDefnWithoutPartitionedFields->DeleteFieldDefn(
                        field.nIdx);
            }
            anMapForSetFrom =
                poFeatureDefnWithoutPartitionedFields->ComputeMapForSetFrom(
                    poSrcFeatureDefn);
        }

        lru11::Cache<std::string, std::shared_ptr<Layer>> oCacheOutputLayer(
            m_maxCacheSize, 0);
        std::shared_ptr<Layer> outputLayer = std::make_unique<Layer>();
        outputLayer->bUseTransactions = bUseTransactions;

        GIntBig nTotalFeatures = 1;
        GIntBig nFeatureIter = 0;
        if (ctxt.m_pfnProgress)
            nTotalFeatures = poSrcLayer->GetFeatureCount(true);
        const double dfInvTotalFeatures =
            1.0 / static_cast<double>(std::max<GIntBig>(1, nTotalFeatures));

        std::string osAttrQueryString;
        if (const char *pszAttrQueryString = poSrcLayer->GetAttrQueryString())
            osAttrQueryString = pszAttrQueryString;

        std::string osKeyTmp;
        std::vector<OGRwkbGeometryType> aeGeomTypesTmp;
        const auto BuildKey =
            [&osKeyTmp, &aeGeomTypesTmp](const std::vector<Field> &fields,
                                         const OGRFeature *poFeature)
            -> std::pair<const std::string &,
                         const std::vector<OGRwkbGeometryType> &>
        {
            osKeyTmp.clear();
            aeGeomTypesTmp.resize(poFeature->GetDefnRef()->GetGeomFieldCount());
            for (const auto &field : fields)
            {
                if (!osKeyTmp.empty())
                    osKeyTmp += '/';
                osKeyTmp += field.encodedFieldName;
                osKeyTmp += '=';
                if (field.bIsGeom)
                {
                    const auto poGeom = poFeature->GetGeomFieldRef(field.nIdx);
                    if (poGeom)
                    {
                        aeGeomTypesTmp[field.nIdx] = poGeom->getGeometryType();
                        osKeyTmp += poGeom->getGeometryName();
                        if (poGeom->Is3D())
                            osKeyTmp += 'Z';
                        if (poGeom->IsMeasured())
                            osKeyTmp += 'M';
                    }
                    else
                    {
                        aeGeomTypesTmp[field.nIdx] = wkbNone;
                        osKeyTmp += NULL_MARKER;
                    }
                }
                else if (poFeature->IsFieldSetAndNotNull(field.nIdx))
                {
                    if (field.eType == OFTString)
                    {
                        PercentEncode(
                            osKeyTmp,
                            poFeature->GetFieldAsStringUnsafe(field.nIdx));
                    }
                    else if (field.eType == OFTInteger)
                    {
                        osKeyTmp += CPLSPrintf(
                            "%d",
                            poFeature->GetFieldAsIntegerUnsafe(field.nIdx));
                    }
                    else
                    {
                        osKeyTmp += CPLSPrintf(
                            CPL_FRMT_GIB,
                            poFeature->GetFieldAsInteger64Unsafe(field.nIdx));
                    }
                }
                else
                {
                    osKeyTmp += NULL_MARKER;
                }
            }
            return {osKeyTmp, aeGeomTypesTmp};
        };

        std::set<std::string> oSetKeys;
        if (!bFormatSupportsAppend)
        {
            CPLDebug(
                "GDAL",
                "First pass to determine all distinct partitioned values...");

            if (asFields.size() == 1 && !asFields[0].bIsGeom)
            {
                std::string osSQL = "SELECT DISTINCT \"";
                osSQL += CPLString(m_fields[0]).replaceAll('"', "\"\"");
                osSQL += "\" FROM \"";
                osSQL += CPLString(poSrcLayer->GetDescription())
                             .replaceAll('"', "\"\"");
                osSQL += '"';
                if (!osAttrQueryString.empty())
                {
                    osSQL += " WHERE ";
                    osSQL += osAttrQueryString;
                }
                auto poSQLLayer =
                    poSrcDS->ExecuteSQL(osSQL.c_str(), nullptr, nullptr);
                if (!poSQLLayer)
                    return false;
                std::vector<Field> asSingleField{asFields[0]};
                asSingleField[0].nIdx = 0;
                for (auto &poFeature : *poSQLLayer)
                {
                    const auto sPair = BuildKey(asFields, poFeature.get());
                    const std::string &osKey = sPair.first;
                    oSetKeys.insert(osKey);
#ifdef DEBUG_VERBOSE
                    CPLDebug("GDAL", "Found %s", osKey.c_str());
#endif
                }
                poSrcDS->ReleaseResultSet(poSQLLayer);

                if (!osAttrQueryString.empty())
                {
                    poSrcLayer->SetAttributeFilter(osAttrQueryString.c_str());
                }
            }
            else
            {
                for (auto &poFeature : *poSrcLayer)
                {
                    const auto sPair = BuildKey(asFields, poFeature.get());
                    const std::string &osKey = sPair.first;
                    if (oSetKeys.insert(osKey).second)
                    {
#ifdef DEBUG_VERBOSE
                        CPLDebug("GDAL", "Found %s", osKey.c_str());
#endif
                    }
                }
            }
            CPLDebug("GDAL",
                     "End of first pass: %d unique partitioning keys found -> "
                     "%d pass(es) needed",
                     static_cast<int>(oSetKeys.size()),
                     static_cast<int>((oSetKeys.size() + m_maxCacheSize - 1) /
                                      m_maxCacheSize));

            // If we have less distinct values as the maximum cache size, we
            // can do a single iteration.
            if (oSetKeys.size() <= static_cast<size_t>(m_maxCacheSize))
                oSetKeys.clear();
        }

        std::set<std::string> oSetOutputDatasets;
        auto oSetKeysIter = oSetKeys.begin();
        while (true)
        {
            // Determine which keys are allowed for the current pass
            std::set<std::string> oSetKeysAllowedInThisPass;
            if (!oSetKeys.empty())
            {
                while (oSetKeysAllowedInThisPass.size() <
                           static_cast<size_t>(m_maxCacheSize) &&
                       oSetKeysIter != oSetKeys.end())
                {
                    oSetKeysAllowedInThisPass.insert(*oSetKeysIter);
                    ++oSetKeysIter;
                }
                if (oSetKeysAllowedInThisPass.empty())
                    break;
            }

            for (auto &poFeature : *poSrcLayer)
            {
                const auto sPair = BuildKey(asFields, poFeature.get());
                const std::string &osKey = sPair.first;
                const auto &aeGeomTypes = sPair.second;

                if (!oSetKeysAllowedInThisPass.empty() &&
                    !cpl::contains(oSetKeysAllowedInThisPass, osKey))
                {
                    continue;
                }

                if (!GetCurrentOutputLayer(
                        this, poSrcFeatureDefn, poSrcLayer, osKey, aeGeomTypes,
                        osLayerDir, m_scheme, m_pattern,
                        m_partDigitLeadingZeroes, m_partDigitCount,
                        m_featureLimit, m_maxFileSize, m_omitPartitionedFields,
                        abPartitionedFields, abPartitionedGeomFields,
                        pszExtension, poOutDriver, datasetCreationOptions,
                        layerCreationOptions,
                        poFeatureDefnWithoutPartitionedFields.get(),
                        poFeature->GetGeometryRef()
                            ? nSpatialIndexPerFeatureConstant
                            : 0,
                        nSpatialIndexPerLog2FeatureCountConstant,
                        bUseTransactions, oCacheOutputLayer, outputLayer))
                {
                    return false;
                }

                if (bParquetOutput)
                {
                    oSetOutputDatasets.insert(
                        outputLayer->poDS->GetDescription());
                }

                if (m_appendLayer)
                    poFeature->SetFID(OGRNullFID);

                OGRErr eErr;
                if (m_omitPartitionedFields ||
                    std::find(aeGeomTypes.begin(), aeGeomTypes.end(),
                              wkbNone) != aeGeomTypes.end())
                {
                    OGRFeature oFeat(outputLayer->poLayer->GetLayerDefn());
                    oFeat.SetFrom(poFeature.get(), anMapForSetFrom.data());
                    oFeat.SetFID(poFeature->GetFID());
                    eErr = outputLayer->poLayer->CreateFeature(&oFeat);
                }
                else
                {
                    poFeature->SetFDefnUnsafe(
                        outputLayer->poLayer->GetLayerDefn());
                    eErr = outputLayer->poLayer->CreateFeature(poFeature.get());
                }
                if (eErr != OGRERR_NONE)
                {
                    ReportError(m_skipErrors ? CE_Warning : CE_Failure,
                                CPLE_AppDefined,
                                "Cannot insert feature " CPL_FRMT_GIB,
                                poFeature->GetFID());
                    if (m_skipErrors)
                        continue;
                    return false;
                }
                ++outputLayer->nFeatureCount;

                if (bUseTransactions &&
                    (outputLayer->nFeatureCount % m_transactionSize) == 0)
                {
                    if (outputLayer->poDS->CommitTransaction() != OGRERR_NONE ||
                        outputLayer->poDS->StartTransaction() != OGRERR_NONE)
                    {
                        return false;
                    }
                }

                // Compute a rough estimate of the space taken by the feature
                if (m_maxFileSize > 0)
                {
                    outputLayer->nFileSize += GetEstimatedFeatureSize(
                        poFeature.get(), abPartitionedFields,
                        m_omitPartitionedFields, aeSrcFieldTypes,
                        bOutputFormatIsBinary);
                }

                ++nFeatureIter;
                if (ctxt.m_pfnProgress &&
                    !ctxt.m_pfnProgress(
                        std::min(1.0, static_cast<double>(nFeatureIter) *
                                          dfInvTotalFeatures),
                        "", ctxt.m_pProgressData))
                {
                    ReportError(CE_Failure, CPLE_UserInterrupt,
                                "Interrupted by user");
                    return false;
                }
            }

            if (oSetKeysIter == oSetKeys.end())
                break;
        }

        const auto nCounter = CPLGetErrorCounter();
        outputLayer.reset();
        oCacheOutputLayer.clear();
        if (CPLGetErrorCounter() != nCounter)
            return false;

        // For Parquet output, create special "_metadata" file that contains
        // the schema and references the individual files
        if (bParquetOutput && !oSetOutputDatasets.empty())
        {
            auto poAlg =
                GDALGlobalAlgorithmRegistry::GetSingleton().Instantiate(
                    "driver", "parquet", "create-metadata-file");
            if (poAlg)
            {
                auto inputArg = poAlg->GetArg(GDAL_ARG_NAME_INPUT);
                auto outputArg = poAlg->GetArg(GDAL_ARG_NAME_OUTPUT);
                if (inputArg && inputArg->GetType() == GAAT_DATASET_LIST &&
                    outputArg && outputArg->GetType() == GAAT_DATASET)
                {
                    std::vector<std::string> asInputFilenames;
                    asInputFilenames.insert(asInputFilenames.end(),
                                            oSetOutputDatasets.begin(),
                                            oSetOutputDatasets.end());
                    inputArg->Set(asInputFilenames);
                    outputArg->Set(CPLFormFilenameSafe(osLayerDir.c_str(),
                                                       "_metadata", nullptr));
                    if (!poAlg->Run())
                        return false;
                }
            }
        }
    }

    return true;
}

/************************************************************************/
/*                GDALVectorPartitionAlgorithm::RunImpl()               */
/************************************************************************/

bool GDALVectorPartitionAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                           void *pProgressData)
{
    GDALPipelineStepRunContext stepCtxt;
    stepCtxt.m_pfnProgress = pfnProgress;
    stepCtxt.m_pProgressData = pProgressData;
    return RunStep(stepCtxt);
}

GDALVectorPartitionAlgorithmStandalone::
    ~GDALVectorPartitionAlgorithmStandalone() = default;
//! @endcond
