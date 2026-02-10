/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster pixelinfo" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_pixel_info.h"

#include "cpl_conv.h"
#include "cpl_json.h"
#include "cpl_minixml.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#include "ogr_spatialref.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <vector>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*     GDALRasterPixelInfoAlgorithm::GDALRasterPixelInfoAlgorithm()     */
/************************************************************************/

GDALRasterPixelInfoAlgorithm::GDALRasterPixelInfoAlgorithm(bool standaloneStep)
    : GDALPipelineStepAlgorithm(
          NAME, DESCRIPTION, HELP_URL,
          ConstructorOptions()
              .SetStandaloneStep(standaloneStep)
              .SetAddAppendLayerArgument(false)
              .SetAddOverwriteLayerArgument(false)
              .SetAddUpdateArgument(false)
              .SetAddUpsertArgument(false)
              .SetAddSkipErrorsArgument(false)
              .SetOutputFormatCreateCapability(GDAL_DCAP_CREATE))
{
    if (standaloneStep)
    {
        AddOutputFormatArg(&m_format, false, false,
                           _("Output format (default is 'GeoJSON' if "
                             "'position-dataset' not specified)"))
            .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                             {GDAL_DCAP_VECTOR, GDAL_DCAP_CREATE});
        AddOpenOptionsArg(&m_openOptions);
        AddInputFormatsArg(&m_inputFormats)
            .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_RASTER});
    }

    AddInputDatasetArg(&m_inputDataset, GDAL_OF_RASTER)
        .AddAlias("dataset")
        .SetMinCount(1)
        .SetMaxCount(1);

    {
        auto &coordinateDatasetArg =
            AddArg("position-dataset", '\0',
                   _("Vector dataset with coordinates"), &m_vectorDataset,
                   GDAL_OF_VECTOR)
                .SetMutualExclusionGroup("position-dataset-pos");
        if (!standaloneStep)
            coordinateDatasetArg.SetPositional().SetRequired();

        SetAutoCompleteFunctionForFilename(coordinateDatasetArg,
                                           GDAL_OF_VECTOR);

        auto &layerArg = AddArg(GDAL_ARG_NAME_INPUT_LAYER, 'l',
                                _("Input layer name"), &m_inputLayerNames)
                             .SetMaxCount(1)
                             .AddAlias("layer");
        SetAutoCompleteFunctionForLayerName(layerArg, coordinateDatasetArg);
    }

    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_VECTOR,
                        /* positionalAndRequired = */ false)
        .SetHiddenForCLI(!standaloneStep);
    if (standaloneStep)
    {
        AddCreationOptionsArg(&m_creationOptions);
        AddLayerCreationOptionsArg(&m_layerCreationOptions);
        AddOverwriteArg(&m_overwrite);
        AddOutputStringArg(&m_output).SetHiddenForCLI();
    }

    AddBandArg(&m_band);
    AddArg("overview", 0, _("Which overview level of source file must be used"),
           &m_overview)
        .SetMinValueIncluded(0);

    if (standaloneStep)
    {
        AddArg("position", 'p', _("Pixel position"), &m_pos)
            .AddAlias("pos")
            .SetMetaVar("<column,line> or <X,Y>")
            .SetPositional()
            .SetMutualExclusionGroup("position-dataset-pos")
            .AddValidationAction(
                [this]
                {
                    if ((m_pos.size() % 2) != 0)
                    {
                        ReportError(
                            CE_Failure, CPLE_IllegalArg,
                            "An even number of values must be specified "
                            "for 'position' argument");
                        return false;
                    }
                    return true;
                });
    }

    AddArg("position-crs", 0,
           _("CRS of position (default is 'pixel' if 'position-dataset' not "
             "specified)"),
           &m_posCrs)
        .SetIsCRSArg(false, {"pixel", "dataset"})
        .AddHiddenAlias("l_srs");

    AddArg("resampling", 'r', _("Resampling algorithm for interpolation"),
           &m_resampling)
        .SetDefault(m_resampling)
        .SetChoices("nearest", "bilinear", "cubic", "cubicspline")
        .SetHiddenChoices("near");

    AddArg(
        "promote-pixel-value-to-z", 0,
        _("Whether to set the pixel value as Z component of GeoJSON geometry"),
        &m_promotePixelValueToZ);

    AddArg("include-field", 0,
           _("Fields from coordinate dataset to include in output (special "
             "values: ALL and NONE)"),
           &m_includeFields)
        .SetDefault("ALL");

    AddValidationAction(
        [this]
        {
            if (m_inputDataset.size() == 1)
            {
                if (auto poSrcDS = m_inputDataset[0].GetDatasetRef())
                {
                    if (poSrcDS->GetRasterCount() > 0)
                    {
                        const int nOvrCount =
                            poSrcDS->GetRasterBand(1)->GetOverviewCount();
                        if (m_overview >= 0 && poSrcDS->GetRasterCount() > 0 &&
                            m_overview >= nOvrCount)
                        {
                            if (nOvrCount == 0)
                            {
                                ReportError(CE_Failure, CPLE_IllegalArg,
                                            "Source dataset has no overviews. "
                                            "Argument 'overview' must not be "
                                            "specified.");
                            }
                            else
                            {
                                ReportError(
                                    CE_Failure, CPLE_IllegalArg,
                                    "Source dataset has only %d overview "
                                    "level%s. "
                                    "'overview' "
                                    "value must be strictly lower than this "
                                    "number.",
                                    nOvrCount, nOvrCount > 1 ? "s" : "");
                            }
                            return false;
                        }
                    }
                    else
                    {
                        ReportError(CE_Failure, CPLE_IllegalArg,
                                    "Source dataset has no raster band.");
                        return false;
                    }
                }
            }
            return true;
        });
}

/************************************************************************/
/*    GDALRasterPixelInfoAlgorithm::~GDALRasterPixelInfoAlgorithm()     */
/************************************************************************/

GDALRasterPixelInfoAlgorithm::~GDALRasterPixelInfoAlgorithm()
{
    if (!m_osTmpFilename.empty())
    {
        VSIRmdir(CPLGetPathSafe(m_osTmpFilename.c_str()).c_str());
        VSIUnlink(m_osTmpFilename.c_str());
    }
}

/************************************************************************/
/*               GDALRasterPixelInfoAlgorithm::RunImpl()                */
/************************************************************************/

bool GDALRasterPixelInfoAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                           void *pProgressData)
{
    GDALPipelineStepRunContext stepCtxt;
    stepCtxt.m_pfnProgress = pfnProgress;
    stepCtxt.m_pProgressData = pProgressData;
    return RunPreStepPipelineValidations() && RunStep(stepCtxt);
}

/************************************************************************/
/*               GDALRasterPixelInfoAlgorithm::RunStep()                */
/************************************************************************/

bool GDALRasterPixelInfoAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    auto poVectorSrcDS = m_vectorDataset.GetDatasetRef();

    if (m_pos.empty() && !poVectorSrcDS && !IsCalledFromCommandLine())
    {
        ReportError(
            CE_Failure, CPLE_AppDefined,
            "Argument 'position' or 'position-dataset' must be specified.");
        return false;
    }

    if (!m_standaloneStep)
    {
        m_format = "MEM";
    }
    else if (m_outputDataset.GetName().empty())
    {
        if (m_format.empty())
        {
            m_format = "GeoJSON";
        }
        else if (!EQUAL(m_format.c_str(), "CSV") &&
                 !EQUAL(m_format.c_str(), "GeoJSON"))
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Only CSV or GeoJSON output format is allowed when "
                        "'output' dataset is not specified.");
            return false;
        }
    }
    else if (m_format.empty())
    {
        const std::string osExt =
            CPLGetExtensionSafe(m_outputDataset.GetName().c_str());
        if (EQUAL(osExt.c_str(), "csv"))
            m_format = "CSV";
        else if (EQUAL(osExt.c_str(), "json"))
            m_format = "GeoJSON";
    }

    const bool bIsCSV = EQUAL(m_format.c_str(), "CSV");
    const bool bIsGeoJSON = EQUAL(m_format.c_str(), "GeoJSON");

    OGRLayer *poSrcLayer = nullptr;
    std::vector<int> anSrcFieldIndicesToInclude;
    std::vector<int> anMapSrcToDstFields;

    if (poVectorSrcDS)
    {
        if (m_inputLayerNames.empty())
        {
            const int nLayerCount = poVectorSrcDS->GetLayerCount();
            if (nLayerCount == 0)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Dataset '%s' has no vector layer",
                            poVectorSrcDS->GetDescription());
                return false;
            }
            else if (nLayerCount > 1)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Dataset '%s' has more than one vector layer. "
                            "Please specify the 'input-layer' argument",
                            poVectorSrcDS->GetDescription());
                return false;
            }
            poSrcLayer = poVectorSrcDS->GetLayer(0);
        }
        else
        {
            poSrcLayer =
                poVectorSrcDS->GetLayerByName(m_inputLayerNames[0].c_str());
        }
        if (!poSrcLayer)
        {
            ReportError(
                CE_Failure, CPLE_AppDefined, "Cannot find layer '%s' in '%s'",
                m_inputLayerNames[0].c_str(), poVectorSrcDS->GetDescription());
            return false;
        }
        if (poSrcLayer->GetGeomType() == wkbNone)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Layer '%s' of '%s' has no geometry column",
                        m_inputLayerNames[0].c_str(),
                        poVectorSrcDS->GetDescription());
            return false;
        }

        if (!GetFieldIndices(m_includeFields, OGRLayer::ToHandle(poSrcLayer),
                             anSrcFieldIndicesToInclude))
        {
            return false;
        }

        if (m_posCrs.empty())
        {
            const auto poVectorLayerSRS = poSrcLayer->GetSpatialRef();
            if (poVectorLayerSRS)
            {
                const char *const apszOptions[] = {"FORMAT=WKT2", nullptr};
                m_posCrs = poVectorLayerSRS->exportToWkt(apszOptions);
            }
        }
    }

    if (m_posCrs.empty())
        m_posCrs = "pixel";

    const GDALRIOResampleAlg eInterpolation =
        GDALRasterIOGetResampleAlg(m_resampling.c_str());

    const auto poSrcCRS = poSrcDS->GetSpatialRef();
    GDALGeoTransform gt;
    const bool bHasGT = poSrcDS->GetGeoTransform(gt) == CE_None;
    GDALGeoTransform invGT;

    if (m_posCrs != "pixel")
    {
        if (!poSrcCRS)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Dataset has no CRS. Only 'position-crs' = 'pixel' is "
                        "supported.");
            return false;
        }

        if (!bHasGT)
        {
            ReportError(CE_Failure, CPLE_AppDefined, "Cannot get geotransform");
            return false;
        }

        if (!gt.GetInverse(invGT))
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot invert geotransform");
            return false;
        }
    }

    std::unique_ptr<OGRCoordinateTransformation> poCT;
    OGRSpatialReference oUserCRS;
    if (m_posCrs != "pixel" && m_posCrs != "dataset")
    {
        oUserCRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        // Already validated due SetIsCRSArg()
        CPL_IGNORE_RET_VAL(oUserCRS.SetFromUserInput(m_posCrs.c_str()));
        poCT.reset(OGRCreateCoordinateTransformation(&oUserCRS, poSrcCRS));
        if (!poCT)
            return false;
    }

    if (m_band.empty())
    {
        for (int i = 1; i <= poSrcDS->GetRasterCount(); ++i)
            m_band.push_back(i);
    }

    std::unique_ptr<GDALDataset> poOutDS;
    OGRLayer *poOutLayer = nullptr;
    std::unique_ptr<OGRCoordinateTransformation> poCTSrcCRSToOutCRS;

    const bool isInteractive =
        m_pos.empty() && m_outputDataset.GetName().empty() &&
        IsCalledFromCommandLine() && CPLIsInteractive(stdin);

    std::string osOutFilename = m_outputDataset.GetName();
    if (osOutFilename.empty() && bIsCSV)
    {
        if (isInteractive)
        {
            osOutFilename = "/vsistdout/";
        }
        else
        {
            osOutFilename = VSIMemGenerateHiddenFilename("subdir");
            osOutFilename += "/out.csv";
            VSIMkdir(CPLGetPathSafe(osOutFilename.c_str()).c_str(), 0755);
            m_osTmpFilename = osOutFilename;
        }
    }

    if (!osOutFilename.empty() || !m_standaloneStep)
    {
        if (bIsGeoJSON)
        {
            m_outputFile.reset(VSIFOpenL(osOutFilename.c_str(), "wb"));
            if (!m_outputFile)
            {
                ReportError(CE_Failure, CPLE_FileIO, "Cannot create '%s'",
                            osOutFilename.c_str());
                return false;
            }
        }
        else
        {
            if (m_format.empty())
            {
                const auto aosFormats =
                    CPLStringList(GDALGetOutputDriversForDatasetName(
                        osOutFilename.c_str(), GDAL_OF_VECTOR,
                        /* bSingleMatch = */ true,
                        /* bWarn = */ true));
                if (aosFormats.size() != 1)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Cannot guess driver for %s",
                                osOutFilename.c_str());
                    return false;
                }
                m_format = aosFormats[0];
            }

            auto poOutDrv =
                GetGDALDriverManager()->GetDriverByName(m_format.c_str());
            if (!poOutDrv)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Cannot find driver %s", m_format.c_str());
                return false;
            }
            poOutDS.reset(
                poOutDrv->Create(osOutFilename.c_str(), 0, 0, 0, GDT_Unknown,
                                 CPLStringList(m_creationOptions).List()));
            if (!poOutDS)
                return false;

            std::string osOutLayerName;
            if (EQUAL(m_format.c_str(), "ESRI Shapefile") || bIsCSV ||
                !poSrcLayer)
            {
                osOutLayerName = CPLGetBasenameSafe(osOutFilename.c_str());
            }
            else
                osOutLayerName = poSrcLayer->GetName();

            const OGRSpatialReference *poOutCRS = nullptr;
            if (!oUserCRS.IsEmpty())
                poOutCRS = &oUserCRS;
            else if (poSrcLayer)
            {
                poOutCRS = poSrcLayer->GetSpatialRef();
                if (!poOutCRS)
                    poOutCRS = poSrcCRS;
            }
            poOutLayer = poOutDS->CreateLayer(
                osOutLayerName.c_str(), poOutCRS, wkbPoint,
                CPLStringList(m_layerCreationOptions).List());
            if (!poOutLayer)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Cannot create layer '%s' in '%s'",
                            osOutLayerName.c_str(), osOutFilename.c_str());
                return false;
            }
            if (poSrcCRS && poOutCRS)
            {
                poCTSrcCRSToOutCRS.reset(
                    OGRCreateCoordinateTransformation(poSrcCRS, poOutCRS));
                if (!poCTSrcCRSToOutCRS)
                    return false;
            }
        }
    }

    if (poOutLayer)
    {
        bool bOK = true;

        if (bIsCSV)
        {
            OGRFieldDefn oFieldLine("geom_x", OFTReal);
            bOK &= poOutLayer->CreateField(&oFieldLine) == OGRERR_NONE;

            OGRFieldDefn oFieldColumn("geom_y", OFTReal);
            bOK &= poOutLayer->CreateField(&oFieldColumn) == OGRERR_NONE;
        }

        if (poSrcLayer)
        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            const auto poSrcLayerDefn = poSrcLayer->GetLayerDefn();
            auto poOutLayerDefn = poOutLayer->GetLayerDefn();

            anMapSrcToDstFields.resize(poSrcLayerDefn->GetFieldCount(), -1);

            for (int nSrcIdx : anSrcFieldIndicesToInclude)
            {
                const auto poSrcFieldDefn =
                    poSrcLayerDefn->GetFieldDefn(nSrcIdx);
                if (poOutLayer->CreateField(poSrcFieldDefn) == OGRERR_NONE)
                {
                    const int nDstIdx = poOutLayerDefn->GetFieldIndex(
                        poSrcFieldDefn->GetNameRef());
                    if (nDstIdx >= 0)
                        anMapSrcToDstFields[nSrcIdx] = nDstIdx;
                }
            }
        }
        else
        {
            OGRFieldDefn oFieldExtraInput("extra_content", OFTString);
            bOK &= poOutLayer->CreateField(&oFieldExtraInput) == OGRERR_NONE;
        }

        OGRFieldDefn oFieldColumn("column", OFTReal);
        bOK &= poOutLayer->CreateField(&oFieldColumn) == OGRERR_NONE;

        OGRFieldDefn oFieldLine("line", OFTReal);
        bOK &= poOutLayer->CreateField(&oFieldLine) == OGRERR_NONE;

        for (int nBand : m_band)
        {
            auto hBand =
                GDALRasterBand::ToHandle(poSrcDS->GetRasterBand(nBand));
            const bool bIsComplex = CPL_TO_BOOL(
                GDALDataTypeIsComplex(GDALGetRasterDataType(hBand)));
            if (bIsComplex)
            {
                OGRFieldDefn oFieldReal(CPLSPrintf("band_%d_real_value", nBand),
                                        OFTReal);
                bOK &= poOutLayer->CreateField(&oFieldReal) == OGRERR_NONE;

                OGRFieldDefn oFieldImag(
                    CPLSPrintf("band_%d_imaginary_value", nBand), OFTReal);
                bOK &= poOutLayer->CreateField(&oFieldImag) == OGRERR_NONE;
            }
            else
            {
                OGRFieldDefn oFieldRaw(CPLSPrintf("band_%d_raw_value", nBand),
                                       OFTReal);
                bOK &= poOutLayer->CreateField(&oFieldRaw) == OGRERR_NONE;

                OGRFieldDefn oFieldUnscaled(
                    CPLSPrintf("band_%d_unscaled_value", nBand), OFTReal);
                bOK &= poOutLayer->CreateField(&oFieldUnscaled) == OGRERR_NONE;
            }
        }

        if (!bOK)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot create fields in output layer");
            return false;
        }
    }

    CPLJSONObject oCollection;
    oCollection.Add("type", "FeatureCollection");
    std::unique_ptr<OGRCoordinateTransformation> poCTToWGS84;
    bool canOutputGeoJSONGeom = false;
    if (poSrcCRS && bHasGT)
    {
        const char *pszAuthName = poSrcCRS->GetAuthorityName(nullptr);
        const char *pszAuthCode = poSrcCRS->GetAuthorityCode(nullptr);
        if (pszAuthName && pszAuthCode && EQUAL(pszAuthName, "EPSG"))
        {
            canOutputGeoJSONGeom = true;
            CPLJSONObject jCRS;
            CPLJSONObject oProperties;
            if (EQUAL(pszAuthCode, "4326"))
                oProperties.Add("name", "urn:ogc:def:crs:OGC:1.3:CRS84");
            else
                oProperties.Add(
                    "name",
                    std::string("urn:ogc:def:crs:EPSG::").append(pszAuthCode));
            jCRS.Add("type", "name");
            jCRS.Add("properties", oProperties);
            oCollection.Add("crs", jCRS);
        }
        else
        {
            OGRSpatialReference oCRS;
            oCRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            oCRS.importFromEPSG(4326);
            poCTToWGS84.reset(
                OGRCreateCoordinateTransformation(poSrcCRS, &oCRS));
            if (poCTToWGS84)
            {
                canOutputGeoJSONGeom = true;
                CPLJSONObject jCRS;
                CPLJSONObject oProperties;
                oProperties.Add("name", "urn:ogc:def:crs:OGC:1.3:CRS84");
                jCRS.Add("type", "name");
                jCRS.Add("properties", oProperties);
                oCollection.Add("crs", jCRS);
            }
        }
    }
    CPLJSONArray oFeatures;
    oCollection.Add("features", oFeatures);

    char szLine[1024];
    int nLine = 0;
    size_t iVal = 0;
    do
    {
        double x = 0, y = 0;
        std::string osExtraContent;
        std::unique_ptr<OGRFeature> poSrcFeature;
        if (poSrcLayer)
        {
            poSrcFeature.reset(poSrcLayer->GetNextFeature());
            if (!poSrcFeature)
                break;
            const auto poGeom = poSrcFeature->GetGeometryRef();
            if (!poGeom || poGeom->IsEmpty())
                continue;
            if (wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
            {
                x = poGeom->toPoint()->getX();
                y = poGeom->toPoint()->getY();
            }
            else
            {
                OGRPoint oPoint;
                if (poGeom->Centroid(&oPoint) != OGRERR_NONE)
                    return false;
                x = oPoint.getX();
                y = oPoint.getY();
            }
        }
        else if (iVal + 1 < m_pos.size())
        {
            x = m_pos[iVal++];
            y = m_pos[iVal++];
        }
        else
        {
            if (CPLIsInteractive(stdin))
            {
                if (m_posCrs != "pixel")
                {
                    fprintf(stderr, "Enter X Y values separated by space, and "
                                    "press Return.\n");
                }
                else
                {
                    fprintf(stderr,
                            "Enter pixel line values separated by space, "
                            "and press Return.\n");
                }
            }

            if (fgets(szLine, sizeof(szLine) - 1, stdin) && szLine[0] != '\n')
            {
                const CPLStringList aosTokens(CSLTokenizeString(szLine));
                const int nCount = aosTokens.size();

                ++nLine;
                if (nCount < 2)
                {
                    fprintf(stderr, "Not enough values at line %d\n", nLine);
                    return false;
                }
                else
                {
                    x = CPLAtof(aosTokens[0]);
                    y = CPLAtof(aosTokens[1]);

                    for (int i = 2; i < nCount; ++i)
                    {
                        if (!osExtraContent.empty())
                            osExtraContent += ' ';
                        osExtraContent += aosTokens[i];
                    }
                    while (!osExtraContent.empty() &&
                           isspace(static_cast<int>(osExtraContent.back())))
                    {
                        osExtraContent.pop_back();
                    }
                }
            }
            else
            {
                break;
            }
        }

        const double xOri = x;
        const double yOri = y;
        double dfPixel{0}, dfLine{0};

        if (poCT)
        {
            if (!poCT->Transform(1, &x, &y, nullptr))
                return false;
        }

        if (m_posCrs != "pixel")
        {
            invGT.Apply(x, y, &dfPixel, &dfLine);
        }
        else
        {
            dfPixel = x;
            dfLine = y;
        }
        const int iPixel = static_cast<int>(
            std::clamp(std::floor(dfPixel), static_cast<double>(INT_MIN),
                       static_cast<double>(INT_MAX)));
        const int iLine = static_cast<int>(
            std::clamp(std::floor(dfLine), static_cast<double>(INT_MIN),
                       static_cast<double>(INT_MAX)));

        CPLJSONObject oFeature;
        CPLJSONObject oProperties;
        std::unique_ptr<OGRFeature> poFeature;
        if (bIsGeoJSON)
        {
            oFeature.Add("type", "Feature");
            oFeature.Add("properties", oProperties);
            {
                CPLJSONArray oArray;
                oArray.Add(xOri);
                oArray.Add(yOri);
                oProperties.Add("input_coordinate", oArray);
            }
            if (!osExtraContent.empty())
                oProperties.Add("extra_content", osExtraContent);
            oProperties.Add("column", dfPixel);
            oProperties.Add("line", dfLine);

            if (poSrcFeature)
            {
                for (int i : anSrcFieldIndicesToInclude)
                {
                    const auto *poFieldDefn = poSrcFeature->GetFieldDefnRef(i);
                    const char *pszName = poFieldDefn->GetNameRef();
                    const auto eType = poFieldDefn->GetType();
                    switch (eType)
                    {
                        case OFTInteger:
                        case OFTInteger64:
                        {
                            if (poFieldDefn->GetSubType() == OFSTBoolean)
                            {
                                oProperties.Add(
                                    pszName,
                                    poSrcFeature->GetFieldAsInteger(i) != 0);
                            }
                            else
                            {
                                oProperties.Add(
                                    pszName,
                                    poSrcFeature->GetFieldAsInteger64(i));
                            }
                            break;
                        }

                        case OFTReal:
                        {
                            oProperties.Add(pszName,
                                            poSrcFeature->GetFieldAsDouble(i));
                            break;
                        }

                        case OFTString:
                        {
                            if (poFieldDefn->GetSubType() != OFSTJSON)
                            {
                                oProperties.Add(
                                    pszName, poSrcFeature->GetFieldAsString(i));
                                break;
                            }
                            else
                            {
                                [[fallthrough]];
                            }
                        }

                        default:
                        {
                            char *pszJSON =
                                poSrcFeature->GetFieldAsSerializedJSon(i);
                            CPLJSONDocument oDoc;
                            if (oDoc.LoadMemory(pszJSON))
                                oProperties.Add(pszName, oDoc.GetRoot());
                            CPLFree(pszJSON);
                            break;
                        }
                    }
                }
            }
        }
        else
        {
            CPLAssert(poOutLayer);
            poFeature =
                std::make_unique<OGRFeature>(poOutLayer->GetLayerDefn());

            if (poSrcFeature)
            {
                CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
                poFeature->SetFrom(poSrcFeature.get(),
                                   anMapSrcToDstFields.data());
            }
            else if (!osExtraContent.empty())
            {
                poFeature->SetField("extra_content", osExtraContent.c_str());
            }

            if (poOutLayer->GetSpatialRef() == nullptr)
            {
                if (bIsCSV)
                {
                    poFeature->SetField("geom_x", xOri);
                    poFeature->SetField("geom_y", yOri);
                }
                else
                {
                    poFeature->SetGeometry(
                        std::make_unique<OGRPoint>(xOri, yOri));
                }
            }
            else if (bHasGT && poCTSrcCRSToOutCRS)
            {
                gt.Apply(dfPixel, dfLine, &x, &y);
                if (poCTSrcCRSToOutCRS->Transform(1, &x, &y))
                {
                    if (bIsCSV)
                    {
                        poFeature->SetField("geom_x", x);
                        poFeature->SetField("geom_y", y);
                    }
                    else
                    {
                        poFeature->SetGeometry(
                            std::make_unique<OGRPoint>(x, y));
                    }
                }
            }

            poFeature->SetField("column", dfPixel);
            poFeature->SetField("line", dfLine);
        }

        CPLJSONArray oBands;

        double zValue = std::numeric_limits<double>::quiet_NaN();
        for (int nBand : m_band)
        {
            CPLJSONObject oBand;
            oBand.Add("band_number", nBand);

            auto hBand =
                GDALRasterBand::ToHandle(poSrcDS->GetRasterBand(nBand));

            int iPixelToQuery = iPixel;
            int iLineToQuery = iLine;

            double dfPixelToQuery = dfPixel;
            double dfLineToQuery = dfLine;

            if (m_overview >= 0 && hBand != nullptr)
            {
                GDALRasterBandH hOvrBand = GDALGetOverview(hBand, m_overview);
                if (hOvrBand != nullptr)
                {
                    int nOvrXSize = GDALGetRasterBandXSize(hOvrBand);
                    int nOvrYSize = GDALGetRasterBandYSize(hOvrBand);
                    iPixelToQuery = static_cast<int>(
                        0.5 +
                        1.0 * iPixel / poSrcDS->GetRasterXSize() * nOvrXSize);
                    iLineToQuery = static_cast<int>(
                        0.5 +
                        1.0 * iLine / poSrcDS->GetRasterYSize() * nOvrYSize);
                    if (iPixelToQuery >= nOvrXSize)
                        iPixelToQuery = nOvrXSize - 1;
                    if (iLineToQuery >= nOvrYSize)
                        iLineToQuery = nOvrYSize - 1;
                    dfPixelToQuery =
                        dfPixel / poSrcDS->GetRasterXSize() * nOvrXSize;
                    dfLineToQuery =
                        dfLine / poSrcDS->GetRasterYSize() * nOvrYSize;
                }
                else
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Cannot get overview %d of band %d", m_overview,
                                nBand);
                    return false;
                }
                hBand = hOvrBand;
            }

            double adfPixel[2] = {0, 0};
            const bool bIsComplex = CPL_TO_BOOL(
                GDALDataTypeIsComplex(GDALGetRasterDataType(hBand)));
            int bIgnored;
            const double dfOffset = GDALGetRasterOffset(hBand, &bIgnored);
            const double dfScale = GDALGetRasterScale(hBand, &bIgnored);
            if (GDALRasterInterpolateAtPoint(
                    hBand, dfPixelToQuery, dfLineToQuery, eInterpolation,
                    &adfPixel[0], &adfPixel[1]) == CE_None)
            {
                if (!bIsComplex)
                {
                    const double dfUnscaledVal =
                        adfPixel[0] * dfScale + dfOffset;
                    if (m_band.size() == 1 && m_promotePixelValueToZ)
                        zValue = dfUnscaledVal;
                    if (bIsGeoJSON)
                    {
                        if (GDALDataTypeIsInteger(GDALGetRasterDataType(hBand)))
                        {
                            oBand.Add("raw_value",
                                      static_cast<GInt64>(adfPixel[0]));
                        }
                        else
                        {
                            oBand.Add("raw_value", adfPixel[0]);
                        }

                        oBand.Add("unscaled_value", dfUnscaledVal);
                    }
                    else
                    {
                        poFeature->SetField(
                            CPLSPrintf("band_%d_raw_value", nBand),
                            adfPixel[0]);
                        poFeature->SetField(
                            CPLSPrintf("band_%d_unscaled_value", nBand),
                            dfUnscaledVal);
                    }
                }
                else
                {
                    if (bIsGeoJSON)
                    {
                        CPLJSONObject oValue;
                        oValue.Add("real", adfPixel[0]);
                        oValue.Add("imaginary", adfPixel[1]);
                        oBand.Add("value", oValue);
                    }
                    else
                    {
                        poFeature->SetField(
                            CPLSPrintf("band_%d_real_value", nBand),
                            adfPixel[0]);
                        poFeature->SetField(
                            CPLSPrintf("band_%d_imaginary_value", nBand),
                            adfPixel[1]);
                    }
                }
            }

            // Request location info for this location (just a few drivers,
            // like the VRT driver actually supports this).
            CPLString osItem;
            osItem.Printf("Pixel_%d_%d", iPixelToQuery, iLineToQuery);

            if (const char *pszLI =
                    GDALGetMetadataItem(hBand, osItem, "LocationInfo"))
            {
                CPLXMLTreeCloser oTree(CPLParseXMLString(pszLI));

                if (oTree && oTree->psChild != nullptr &&
                    oTree->eType == CXT_Element &&
                    EQUAL(oTree->pszValue, "LocationInfo"))
                {
                    CPLJSONArray oFiles;

                    for (const CPLXMLNode *psNode = oTree->psChild;
                         psNode != nullptr; psNode = psNode->psNext)
                    {
                        if (psNode->eType == CXT_Element &&
                            EQUAL(psNode->pszValue, "File") &&
                            psNode->psChild != nullptr)
                        {
                            char *pszUnescaped = CPLUnescapeString(
                                psNode->psChild->pszValue, nullptr, CPLES_XML);
                            oFiles.Add(pszUnescaped);
                            CPLFree(pszUnescaped);
                        }
                    }

                    oBand.Add("files", oFiles);
                }
                else
                {
                    oBand.Add("location_info", pszLI);
                }
            }

            oBands.Add(oBand);
        }

        if (bIsGeoJSON)
        {
            oProperties.Add("bands", oBands);

            if (canOutputGeoJSONGeom)
            {
                x = dfPixel;
                y = dfLine;

                gt.Apply(x, y, &x, &y);

                if (poCTToWGS84)
                    poCTToWGS84->Transform(1, &x, &y);

                CPLJSONObject oGeometry;
                oFeature.Add("geometry", oGeometry);
                oGeometry.Add("type", "Point");
                CPLJSONArray oCoordinates;
                oCoordinates.Add(x);
                oCoordinates.Add(y);
                if (!std::isnan(zValue))
                    oCoordinates.Add(zValue);
                oGeometry.Add("coordinates", oCoordinates);
            }
            else
            {
                oFeature.AddNull("geometry");
            }

            if (isInteractive)
            {
                CPLJSONDocument oDoc;
                oDoc.SetRoot(oFeature);
                printf("%s\n", oDoc.SaveAsString().c_str());
            }
            else
            {
                oFeatures.Add(oFeature);
            }
        }
        else
        {
            if (poOutLayer->CreateFeature(std::move(poFeature)) != OGRERR_NONE)
            {
                return false;
            }
        }

    } while (m_pos.empty() || iVal + 1 < m_pos.size());

    if (bIsGeoJSON && !isInteractive)
    {
        CPLJSONDocument oDoc;
        oDoc.SetRoot(oCollection);
        std::string osRet = oDoc.SaveAsString();
        if (m_outputFile)
            m_outputFile->Write(osRet.data(), osRet.size());
        else
            m_output = std::move(osRet);
    }
    else if (poOutDS)
    {
        if (m_outputDataset.GetName().empty() && m_standaloneStep)
        {
            poOutDS.reset();
            if (!isInteractive)
            {
                CPLAssert(!m_osTmpFilename.empty());
                const GByte *pabyData =
                    VSIGetMemFileBuffer(m_osTmpFilename.c_str(), nullptr,
                                        /* bUnlinkAndSeize = */ false);
                m_output = reinterpret_cast<const char *>(pabyData);
            }
        }
        else
        {
            m_outputDataset.Set(std::move(poOutDS));
        }
    }

    bool bRet = true;
    if (m_outputFile)
    {
        bRet = m_outputFile->Close() == 0;
        if (bRet)
        {
            poOutDS.reset(
                GDALDataset::Open(m_outputDataset.GetName().c_str(),
                                  GDAL_OF_VECTOR | GDAL_OF_VERBOSE_ERROR));
            bRet = poOutDS != nullptr;
            m_outputDataset.Set(std::move(poOutDS));
        }
    }

    return bRet;
}

GDALRasterPixelInfoAlgorithmStandalone::
    ~GDALRasterPixelInfoAlgorithmStandalone() = default;

//! @endcond
