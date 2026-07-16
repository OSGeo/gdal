/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector index" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_index.h"

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gdal_utils_priv.h"
#include "ogrsf_frmts.h"
#include "commonutils.h"

#include <algorithm>
#include <cassert>
#include <utility>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*         GDALVectorIndexAlgorithm::GDALVectorIndexAlgorithm()         */
/************************************************************************/

GDALVectorIndexAlgorithm::GDALVectorIndexAlgorithm()
    : GDALVectorOutputAbstractAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddProgressArg();
    AddInputDatasetArg(&m_inputDatasets, GDAL_OF_VECTOR)
        .SetAutoOpenDataset(false)
        .SetDatasetInputFlags(GADV_NAME);
    GDALVectorOutputAbstractAlgorithm::AddAllOutputArgs();

    AddArg("recursive", 0,
           _("Whether input directories should be explored recursively."),
           &m_recursive);
    AddArg("filename-filter", 0,
           _("Pattern that the filenames in input directories should follow "
             "('*' and '?' wildcard)"),
           &m_filenameFilter);
    AddArg("location-name", 0, _("Name of the field with the vector path"),
           &m_locationName)
        .SetDefault(m_locationName)
        .SetMinCharCount(1);
    AddAbsolutePathArg(
        &m_writeAbsolutePaths,
        _("Whether the path to the input datasets should be stored as an "
          "absolute path"));
    AddArg("dst-crs", 0, _("Destination CRS"), &m_crs)
        .SetIsCRSArg()
        .AddHiddenAlias("t_srs");

    {
        auto &arg =
            AddArg("metadata", 0, _("Add dataset metadata item"), &m_metadata)
                .SetMetaVar("<KEY>=<VALUE>")
                .SetPackedValuesAllowed(false);
        arg.AddValidationAction([this, &arg]()
                                { return ParseAndValidateKeyValue(arg); });
        arg.AddHiddenAlias("mo");
    }
    AddArg("source-crs-field-name", 0,
           _("Name of the field to store the CRS of each dataset"),
           &m_sourceCrsName)
        .SetMinCharCount(1);
    auto &sourceCRSFormatArg =
        AddArg("source-crs-format", 0,
               _("Format in which the CRS of each dataset must be written"),
               &m_sourceCrsFormat)
            .SetMinCharCount(1)
            .SetDefault(m_sourceCrsFormat)
            .SetChoices("auto", "WKT", "EPSG", "PROJ");
    AddArg("source-layer-name", 0,
           _("Add layer of specified name from each source file in the tile "
             "index"),
           &m_layerNames);
    AddArg("source-layer-index", 0,
           _("Add layer of specified index (0-based) from each source file in "
             "the tile index"),
           &m_layerIndices);
    AddArg("accept-different-crs", 0,
           _("Whether layers with different CRS are accepted"),
           &m_acceptDifferentCRS);
    AddArg("accept-different-schemas", 0,
           _("Whether layers with different schemas are accepted"),
           &m_acceptDifferentSchemas);
    AddArg("dataset-name-only", 0,
           _("Whether to write the dataset name only, instead of suffixed with "
             "the layer index"),
           &m_datasetNameOnly);

    // Hidden
    AddArg("called-from-ogrtindex", 0,
           _("Whether we are called from ogrtindex"), &m_calledFromOgrTIndex)
        .SetHidden();
    // Hidden. For compatibility with ogrtindex
    AddArg("skip-different-crs", 0,
           _("Skip layers that are not in the same CRS as the first layer"),
           &m_skipDifferentCRS)
        .SetHidden();

    AddValidationAction(
        [this, &sourceCRSFormatArg]()
        {
            if (m_acceptDifferentCRS && m_skipDifferentCRS)
            {
                ReportError(CE_Failure, CPLE_IllegalArg,
                            "Options 'accept-different-crs' and "
                            "'skip-different-crs' are mutually exclusive");
                return false;
            }

            if (sourceCRSFormatArg.IsExplicitlySet() && m_sourceCrsName.empty())
            {
                ReportError(CE_Failure, CPLE_IllegalArg,
                            "Option 'source-crs-name' must be specified when "
                            "'source-crs-format' is specified");
                return false;
            }

            if (!m_crs.empty() && m_skipDifferentCRS)
            {
                ReportError(
                    CE_Warning, CPLE_AppDefined,
                    "--skip-different-crs ignored when --dst-crs specified");
            }

            return true;
        });
}

/************************************************************************/
/*                      GDALVectorDatasetIterator                       */
/************************************************************************/

struct GDALVectorDatasetIterator
{
    const std::vector<GDALArgDatasetValue> &inputs;
    const bool bRecursive;
    const std::vector<std::string> &filenameFilters;
    const std::vector<std::string> &aosLayerNamesOfInterest;
    const std::vector<int> &aosLayerIndicesOfInterest;
    std::string osCurDir{};
    size_t iCurSrc = 0;
    VSIDIR *psDir = nullptr;

    CPL_DISALLOW_COPY_ASSIGN(GDALVectorDatasetIterator)

    GDALVectorDatasetIterator(
        const std::vector<GDALArgDatasetValue> &inputsIn, bool bRecursiveIn,
        const std::vector<std::string> &filenameFiltersIn,
        const std::vector<std::string> &aosLayerNamesOfInterestIn,
        const std::vector<int> &aosLayerIndicesOfInterestIn)
        : inputs(inputsIn), bRecursive(bRecursiveIn),
          filenameFilters(filenameFiltersIn),
          aosLayerNamesOfInterest(aosLayerNamesOfInterestIn),
          aosLayerIndicesOfInterest(aosLayerIndicesOfInterestIn)
    {
    }

    void reset()
    {
        if (psDir)
            VSICloseDir(psDir);
        psDir = nullptr;
        iCurSrc = 0;
    }

    std::vector<int> GetLayerIndices(GDALDataset *poDS) const
    {
        std::vector<int> ret;
        const int nLayerCount = poDS->GetLayerCount();
        for (int i = 0; i < nLayerCount; ++i)
        {
            auto poLayer = poDS->GetLayer(i);
            if ((aosLayerNamesOfInterest.empty() &&
                 aosLayerIndicesOfInterest.empty()) ||
                (!aosLayerNamesOfInterest.empty() &&
                 std::find(aosLayerNamesOfInterest.begin(),
                           aosLayerNamesOfInterest.end(),
                           poLayer->GetDescription()) !=
                     aosLayerNamesOfInterest.end()) ||
                (!aosLayerIndicesOfInterest.empty() &&
                 std::find(aosLayerIndicesOfInterest.begin(),
                           aosLayerIndicesOfInterest.end(),
                           i) != aosLayerIndicesOfInterest.end()))
            {
                ret.push_back(i);
            }
        }
        return ret;
    }

    bool MatchPattern(const std::string &filename) const
    {
        for (const auto &osFilter : filenameFilters)
        {
            if (GDALPatternMatch(filename.c_str(), osFilter.c_str()))
            {
                return true;
            }
        }
        return filenameFilters.empty();
    }

    std::pair<std::unique_ptr<GDALDataset>, std::vector<int>> next()
    {
        std::pair<std::unique_ptr<GDALDataset>, std::vector<int>> emptyRet;

        while (true)
        {
            if (!psDir)
            {
                if (iCurSrc == inputs.size())
                {
                    break;
                }

                VSIStatBufL sStatBuf;
                const std::string &osCurName = inputs[iCurSrc++].GetName();
                if (MatchPattern(osCurName))
                {
                    auto poSrcDS = std::unique_ptr<GDALDataset>(
                        GDALDataset::Open(osCurName.c_str(), GDAL_OF_VECTOR,
                                          nullptr, nullptr, nullptr));
                    if (poSrcDS)
                    {
                        auto anLayerIndices = GetLayerIndices(poSrcDS.get());
                        if (!anLayerIndices.empty())
                        {
                            return {std::move(poSrcDS),
                                    std::move(anLayerIndices)};
                        }
                    }
                }

                if (VSIStatL(osCurName.c_str(), &sStatBuf) == 0 &&
                    VSI_ISDIR(sStatBuf.st_mode) &&
                    !cpl::ends_with(osCurName, ".gdb"))
                {
                    osCurDir = osCurName;
                    psDir = VSIOpenDir(osCurDir.c_str(),
                                       /*nDepth=*/bRecursive ? -1 : 0, nullptr);
                    if (!psDir)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Cannot open directory %s", osCurDir.c_str());
                        return emptyRet;
                    }
                }
                else
                {
                    return emptyRet;
                }
            }

            auto psEntry = VSIGetNextDirEntry(psDir);
            if (!psEntry)
            {
                VSICloseDir(psDir);
                psDir = nullptr;
                continue;
            }

            if (!MatchPattern(CPLGetFilename(psEntry->pszName)))
            {
                continue;
            }

            const std::string osFilename = CPLFormFilenameSafe(
                osCurDir.c_str(), psEntry->pszName, nullptr);
            auto poSrcDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
                osFilename.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr));
            if (poSrcDS)
            {
                auto anLayerIndices = GetLayerIndices(poSrcDS.get());
                if (!anLayerIndices.empty())
                {
                    return {std::move(poSrcDS), std::move(anLayerIndices)};
                }
            }
        }
        return emptyRet;
    }
};

/************************************************************************/
/*                 GDALVectorIndexAlgorithm::RunImpl()                  */
/************************************************************************/

bool GDALVectorIndexAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                       void *pProgressData)
{
    CPLStringList aosSources;
    for (auto &srcDS : m_inputDatasets)
    {
        if (srcDS.GetDatasetRef())
        {
            ReportError(
                CE_Failure, CPLE_IllegalArg,
                "Input datasets must be provided by name, not as object");
            return false;
        }
        aosSources.push_back(srcDS.GetName());
    }

    std::string osCWD;
    if (m_writeAbsolutePaths)
    {
        char *pszCurrentPath = CPLGetCurrentDir();
        if (pszCurrentPath == nullptr)
        {
            ReportError(
                CE_Failure, CPLE_AppDefined,
                "This system does not support the CPLGetCurrentDir call.");
            return false;
        }
        osCWD = pszCurrentPath;
        CPLFree(pszCurrentPath);
    }

    auto setupRet = SetupOutputDataset();
    if (!setupRet.outDS)
        return false;

    const auto poOutDrv = setupRet.outDS->GetDriver();

    GDALVectorDatasetIterator oIterator(m_inputDatasets, m_recursive,
                                        m_filenameFilter, m_layerNames,
                                        m_layerIndices);

    if (m_outputLayerName.empty())
        m_outputLayerName = "tileindex";

    std::unique_ptr<OGRSpatialReference, OGRSpatialReferenceReleaser>
        poTargetCRS{};
    if (!m_crs.empty())
    {
        poTargetCRS.reset(std::make_unique<OGRSpatialReference>().release());
        poTargetCRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        CPL_IGNORE_RET_VAL(poTargetCRS->SetFromUserInput(m_crs.c_str()));
    }

    std::set<std::string> setAlreadyReferencedLayers;

    const size_t nMaxFieldSize = [poOutDrv]()
    {
        const char *pszVal =
            poOutDrv ? poOutDrv->GetMetadataItem(GDAL_DMD_MAX_STRING_LENGTH)
                     : nullptr;
        return pszVal ? atoi(pszVal) : 0;
    }();

    OGRLayer *poDstLayer = setupRet.layer;
    int nLocationFieldIdx = -1;
    int nSourceCRSFieldIdx = -1;

    struct OGRFeatureDefnReleaser
    {
        void operator()(OGRFeatureDefn *poFDefn)
        {
            if (poFDefn)
                poFDefn->Release();
        }
    };

    std::unique_ptr<OGRFeatureDefn, OGRFeatureDefnReleaser> poRefFeatureDefn;
    if (poDstLayer)
    {
        nLocationFieldIdx =
            poDstLayer->GetLayerDefn()->GetFieldIndex(m_locationName.c_str());
        if (nLocationFieldIdx < 0)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Unable to find field '%s' in output layer.",
                        m_locationName.c_str());
            return false;
        }

        if (!m_sourceCrsName.empty())
        {
            nSourceCRSFieldIdx = poDstLayer->GetLayerDefn()->GetFieldIndex(
                m_sourceCrsName.c_str());
            if (nSourceCRSFieldIdx < 0)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Unable to find field '%s' in output layer.",
                            m_sourceCrsName.c_str());
                return false;
            }
        }

        if (!poTargetCRS)
        {
            const auto poSrcCRS = poDstLayer->GetSpatialRef();
            if (poSrcCRS)
                poTargetCRS.reset(poSrcCRS->Clone());
        }

        for (auto &&poFeature : poDstLayer)
        {
            std::string osLocation =
                poFeature->GetFieldAsString(nLocationFieldIdx);

            if (!poRefFeatureDefn)
            {
                const auto nCommaPos = osLocation.rfind(',');
                if (nCommaPos != std::string::npos)
                {
                    auto poDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
                        osLocation.substr(0, nCommaPos).c_str(),
                        GDAL_OF_VECTOR));
                    if (poDS)
                    {
                        if (auto poLayer = poDS->GetLayer(
                                atoi(osLocation.substr(nCommaPos + 1).c_str())))
                        {
                            poRefFeatureDefn.reset(
                                poLayer->GetLayerDefn()->Clone());
                        }
                    }
                }
            }

            setAlreadyReferencedLayers.insert(std::move(osLocation));
        }
    }
    else
    {
        auto [poSrcDS, anLayerIndices] = oIterator.next();
        oIterator.reset();
        if (!poSrcDS)
        {
            ReportError(CE_Failure, CPLE_AppDefined, "No layer to index");
            return false;
        }

        if (!poTargetCRS)
        {
            const auto poSrcCRS =
                poSrcDS->GetLayer(anLayerIndices[0])->GetSpatialRef();
            if (poSrcCRS)
                poTargetCRS.reset(poSrcCRS->Clone());
        }

        poDstLayer = setupRet.outDS->CreateLayer(m_outputLayerName.c_str(),
                                                 poTargetCRS.get(), wkbPolygon);
        if (!poDstLayer)
            return false;

        OGRFieldDefn oLocation(m_locationName.c_str(), OFTString);
        oLocation.SetWidth(static_cast<int>(nMaxFieldSize));
        if (poDstLayer->CreateField(&oLocation) != OGRERR_NONE)
            return false;
        nLocationFieldIdx = poDstLayer->GetLayerDefn()->GetFieldCount() - 1;

        if (!m_sourceCrsName.empty())
        {
            OGRFieldDefn oSrcSRSNameField(m_sourceCrsName.c_str(), OFTString);
            if (poDstLayer->CreateField(&oSrcSRSNameField) != OGRERR_NONE)
                return false;
            nSourceCRSFieldIdx =
                poDstLayer->GetLayerDefn()->GetFieldCount() - 1;
        }

        if (!m_metadata.empty())
        {
            poDstLayer->SetMetadata(CPLStringList(m_metadata).List());
        }
    }

    double dfPct = 0;
    double dfIncrement = 0.1;
    int nRemainingIters = 5;

    bool bOK = true;
    bool bFirstWarningForNonMatchingAttributes = false;
    while (bOK)
    {
        auto [poSrcDS, anLayerIndices] = oIterator.next();
        if (!poSrcDS)
            break;

        dfPct += dfIncrement;
        if (pfnProgress && !pfnProgress(dfPct, "", pProgressData))
        {
            bOK = false;
            break;
        }
        --nRemainingIters;
        if (nRemainingIters == 0)
        {
            dfIncrement /= 2;
            nRemainingIters = 5;
        }

        std::string osFilename = poSrcDS->GetDescription();
        VSIStatBufL sStatBuf;
        if (m_writeAbsolutePaths && CPLIsFilenameRelative(osFilename.c_str()) &&
            VSIStatL(osFilename.c_str(), &sStatBuf) == 0)
        {
            osFilename =
                CPLFormFilenameSafe(osCWD.c_str(), osFilename.c_str(), nullptr)
                    .c_str();
        }

        for (int iLayer : anLayerIndices)
        {
            auto poSrcLayer = poSrcDS->GetLayer(iLayer);

            const std::string osLocation =
                m_datasetNameOnly
                    ? osFilename
                    : CPLOPrintf("%s,%d", osFilename.c_str(), iLayer);
            if (cpl::contains(setAlreadyReferencedLayers, osLocation))
            {
                ReportError(CE_Warning, CPLE_AppDefined,
                            "'%s' already referenced in tile index",
                            osLocation.c_str());
                continue;
            }

            const OGRSpatialReference *poSrcCRS = poSrcLayer->GetSpatialRef();
            // If not set target srs, test that the current file uses same
            // projection as others.
            if (m_crs.empty())
            {
                if ((poTargetCRS && poSrcCRS &&
                     !poTargetCRS->IsSame(poSrcCRS)) ||
                    ((poTargetCRS != nullptr) != (poSrcCRS != nullptr)))
                {
                    ReportError(
                        CE_Warning, CPLE_AppDefined,
                        "Warning: layer %s of %s is not using the same "
                        "CRS as other files in the "
                        "tileindex. This may cause problems when using it "
                        "in MapServer for example%s",
                        poSrcLayer->GetDescription(), poSrcDS->GetDescription(),
                        m_skipDifferentCRS || !m_acceptDifferentCRS
                            ? ". Skipping it"
                        : !m_skipDifferentCRS && m_calledFromOgrTIndex
                            ? ". You may specify -skip_differerence_srs to "
                              "skip it"
                            : "");
                    if (m_skipDifferentCRS || !m_acceptDifferentCRS)
                        continue;
                }
            }

            OGRFeature oFeat(poDstLayer->GetLayerDefn());
            oFeat.SetField(nLocationFieldIdx, osLocation.c_str());

            if (nSourceCRSFieldIdx >= 0 && poSrcCRS)
            {
                const char *pszAuthorityCode =
                    poSrcCRS->GetAuthorityCode(nullptr);
                const char *pszAuthorityName =
                    poSrcCRS->GetAuthorityName(nullptr);
                const std::string osWKT = poSrcCRS->exportToWkt();
                if (m_sourceCrsFormat == "auto")
                {
                    if (pszAuthorityName != nullptr &&
                        pszAuthorityCode != nullptr)
                    {
                        oFeat.SetField(nSourceCRSFieldIdx,
                                       CPLSPrintf("%s:%s", pszAuthorityName,
                                                  pszAuthorityCode));
                    }
                    else if (nMaxFieldSize == 0 ||
                             osWKT.size() <= nMaxFieldSize)
                    {
                        oFeat.SetField(nSourceCRSFieldIdx, osWKT.c_str());
                    }
                    else
                    {
                        char *pszProj4 = nullptr;
                        if (poSrcCRS->exportToProj4(&pszProj4) == OGRERR_NONE)
                        {
                            oFeat.SetField(nSourceCRSFieldIdx, pszProj4);
                        }
                        else
                        {
                            oFeat.SetField(nSourceCRSFieldIdx, osWKT.c_str());
                        }
                        CPLFree(pszProj4);
                    }
                }
                else if (m_sourceCrsFormat == "WKT")
                {
                    if (nMaxFieldSize == 0 || osWKT.size() <= nMaxFieldSize)
                    {
                        oFeat.SetField(nSourceCRSFieldIdx, osWKT.c_str());
                    }
                    else
                    {
                        ReportError(
                            CE_Warning, CPLE_AppDefined,
                            "Cannot write WKT for file %s as it is too long",
                            osFilename.c_str());
                    }
                }
                else if (m_sourceCrsFormat == "PROJ")
                {
                    char *pszProj4 = nullptr;
                    if (poSrcCRS->exportToProj4(&pszProj4) == OGRERR_NONE)
                    {
                        oFeat.SetField(nSourceCRSFieldIdx, pszProj4);
                    }
                    CPLFree(pszProj4);
                }
                else
                {
                    CPLAssert(m_sourceCrsFormat == "EPSG");
                    if (pszAuthorityName != nullptr &&
                        pszAuthorityCode != nullptr)
                    {
                        oFeat.SetField(nSourceCRSFieldIdx,
                                       CPLSPrintf("%s:%s", pszAuthorityName,
                                                  pszAuthorityCode));
                    }
                }
            }

            // Check if all layers in dataset have the same attributes schema
            if (poRefFeatureDefn == nullptr)
            {
                poRefFeatureDefn.reset(poSrcLayer->GetLayerDefn()->Clone());
            }
            else if (!m_acceptDifferentSchemas)
            {
                const OGRFeatureDefn *poFeatureDefnCur =
                    poSrcLayer->GetLayerDefn();
                assert(nullptr != poFeatureDefnCur);

                const auto EmitHint =
                    [this, &bFirstWarningForNonMatchingAttributes]()
                {
                    if (bFirstWarningForNonMatchingAttributes)
                    {
                        ReportError(
                            CE_Warning, CPLE_AppDefined,
                            "Note : you can override this "
                            "behavior with %s option, "
                            "but this may result in a tileindex incompatible "
                            "with MapServer",
                            m_calledFromOgrTIndex
                                ? "-accept_different_schemas"
                                : "--accept-different-schemas");
                        bFirstWarningForNonMatchingAttributes = false;
                    }
                };

                const int fieldCount = poFeatureDefnCur->GetFieldCount();
                if (fieldCount != poRefFeatureDefn->GetFieldCount())
                {
                    ReportError(CE_Warning, CPLE_AppDefined,
                                "Number of attributes of layer %s of %s "
                                "does not match. Skipping it.",
                                poSrcLayer->GetDescription(),
                                poSrcDS->GetDescription());
                    EmitHint();
                    continue;
                }

                bool bSkip = false;
                for (int fn = 0; fn < fieldCount; fn++)
                {
                    const OGRFieldDefn *poFieldThis =
                        poFeatureDefnCur->GetFieldDefn(fn);
                    const OGRFieldDefn *poFieldRef =
                        poRefFeatureDefn->GetFieldDefn(fn);
                    if (!(poFieldThis->GetType() == poFieldRef->GetType() &&
                          poFieldThis->GetWidth() == poFieldRef->GetWidth() &&
                          poFieldThis->GetPrecision() ==
                              poFieldRef->GetPrecision() &&
                          strcmp(poFieldThis->GetNameRef(),
                                 poFieldRef->GetNameRef()) == 0))
                    {
                        ReportError(CE_Warning, CPLE_AppDefined,
                                    "Schema of attributes of layer %s of %s "
                                    "does not match. Skipping it.",
                                    poSrcLayer->GetDescription(),
                                    poSrcDS->GetDescription());
                        EmitHint();
                        bSkip = true;
                        break;
                    }
                }

                if (bSkip)
                    continue;
            }

            // Get layer extents, and create a corresponding polygon.
            OGREnvelope sExtents;
            if (poSrcLayer->GetExtent(&sExtents, TRUE) != OGRERR_NONE)
            {
                ReportError(CE_Warning, CPLE_AppDefined,
                            "GetExtent() failed on layer %s of %s, skipping.",
                            poSrcLayer->GetDescription(),
                            poSrcDS->GetDescription());
                continue;
            }

            OGRPolygon oExtentGeom(sExtents);

            // If set target srs, do the forward transformation of all points.
            if (!m_crs.empty() && poSrcCRS && poTargetCRS &&
                !poSrcCRS->IsSame(poTargetCRS.get()))
            {
                auto poCT = std::unique_ptr<OGRCoordinateTransformation>(
                    OGRCreateCoordinateTransformation(poSrcCRS,
                                                      poTargetCRS.get()));
                if (poCT == nullptr ||
                    oExtentGeom.transform(poCT.get()) == OGRERR_FAILURE)
                {
                    ReportError(CE_Warning, CPLE_AppDefined,
                                "Cannot reproject extent of layer %s of %s to "
                                "the target CRS, skipping.",
                                poSrcLayer->GetDescription(),
                                poSrcDS->GetDescription());
                    continue;
                }
            }

            oFeat.SetGeometry(&oExtentGeom);

            bOK = bOK && (poDstLayer->CreateFeature(&oFeat) == OGRERR_NONE);
        }
    }

    if (bOK && pfnProgress)
        pfnProgress(1.0, "", pProgressData);

    if (bOK && setupRet.newDS && !m_outputDataset.GetDatasetRef())
    {
        m_outputDataset.Set(std::move(setupRet.newDS));
    }

    return bOK;
}

//! @endcond
