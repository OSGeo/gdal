/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector layer-algebra" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_layer_algebra.h"

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#include "ogr_api.h"
#include "ogrsf_frmts.h"

#include <algorithm>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                  GDALVectorLayerAlgebraAlgorithm()                   */
/************************************************************************/

GDALVectorLayerAlgebraAlgorithm::GDALVectorLayerAlgebraAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddProgressArg();

    AddArg("operation", 0, _("Operation to perform"), &m_operation)
        .SetChoices("union", "intersection", "sym-difference", "identity",
                    "update", "clip", "erase")
        .SetRequired()
        .SetPositional();

    AddOutputFormatArg(&m_format).AddMetadataItem(
        GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_VECTOR, GDAL_DCAP_CREATE});
    AddOpenOptionsArg(&m_openOptions);
    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_VECTOR});
    AddInputDatasetArg(&m_inputDataset, GDAL_OF_VECTOR);

    {
        auto &arg = AddArg("method", 0, _("Method vector dataset"),
                           &m_methodDataset, GDAL_OF_VECTOR)
                        .SetPositional()
                        .SetRequired();

        SetAutoCompleteFunctionForFilename(arg, GDAL_OF_VECTOR);
    }
    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_VECTOR)
        .SetDatasetInputFlags(GADV_NAME | GADV_OBJECT);
    AddCreationOptionsArg(&m_creationOptions);
    AddLayerCreationOptionsArg(&m_layerCreationOptions);
    AddOverwriteArg(&m_overwrite);
    AddUpdateArg(&m_update);
    AddOverwriteLayerArg(&m_overwriteLayer);
    AddAppendLayerArg(&m_appendLayer);

    AddArg(GDAL_ARG_NAME_INPUT_LAYER, 0, _("Input layer name"),
           &m_inputLayerName);
    AddArg("method-layer", 0, _("Method layer name"), &m_methodLayerName);
    AddOutputLayerNameArg(&m_outputLayerName)
        .AddHiddenAlias("nln");  // For ogr2ogr nostalgic people

    AddGeometryTypeArg(&m_geometryType);

    AddArg("input-prefix", 0,
           _("Prefix for fields corresponding to input layer"), &m_inputPrefix)
        .SetCategory(GAAC_ADVANCED);
    AddArg("input-field", 0, _("Input field(s) to add to output layer"),
           &m_inputFields)
        .SetCategory(GAAC_ADVANCED)
        .SetMutualExclusionGroup("input-field");
    AddArg("no-input-field", 0, _("Do not add any input field to output layer"),
           &m_noInputFields)
        .SetCategory(GAAC_ADVANCED)
        .SetMutualExclusionGroup("input-field");
    AddArg("all-input-field", 0, _("Add all input fields to output layer"),
           &m_allInputFields)
        .SetCategory(GAAC_ADVANCED)
        .SetMutualExclusionGroup("input-field");

    AddArg("method-prefix", 0,
           _("Prefix for fields corresponding to method layer"),
           &m_methodPrefix)
        .SetCategory(GAAC_ADVANCED);
    AddArg("method-field", 0, _("Method field(s) to add to output layer"),
           &m_methodFields)
        .SetCategory(GAAC_ADVANCED)
        .SetMutualExclusionGroup("method-field");
    AddArg("no-method-field", 0,
           _("Do not add any method field to output layer"), &m_noMethodFields)
        .SetCategory(GAAC_ADVANCED)
        .SetMutualExclusionGroup("method-field");
    AddArg("all-method-field", 0, _("Add all method fields to output layer"),
           &m_allMethodFields)
        .SetCategory(GAAC_ADVANCED)
        .SetMutualExclusionGroup("method-field");
}

/************************************************************************/
/*              GDALVectorLayerAlgebraAlgorithm::RunImpl()              */
/************************************************************************/

bool GDALVectorLayerAlgebraAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                              void *pProgressData)
{
#ifdef HAVE_GEOS
    auto poSrcDS = m_inputDataset.GetDatasetRef();
    CPLAssert(poSrcDS);
    auto poMethodDS = m_methodDataset.GetDatasetRef();
    CPLAssert(poMethodDS);

    if (poSrcDS == poMethodDS)
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Input and method datasets must be different");
        return false;
    }

    auto poDstDS = m_outputDataset.GetDatasetRef();
    std::unique_ptr<GDALDataset> poDstDSUniquePtr;
    const bool bNewDataset = poDstDS == nullptr;
    if (poDstDS == nullptr)
    {
        if (m_format.empty())
        {
            const CPLStringList aosFormats(GDALGetOutputDriversForDatasetName(
                m_outputDataset.GetName().c_str(), GDAL_OF_VECTOR,
                /* bSingleMatch = */ true,
                /* bEmitWarning = */ true));
            if (aosFormats.size() != 1)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Cannot guess driver for %s",
                            m_outputDataset.GetName().c_str());
                return false;
            }
            m_format = aosFormats[0];
        }

        auto poOutDrv =
            GetGDALDriverManager()->GetDriverByName(m_format.c_str());
        if (!poOutDrv)
        {
            // shouldn't happen given checks done in GDALAlgorithm unless
            // someone deregister the driver between ParseCommandLineArgs() and
            // Run()
            ReportError(CE_Failure, CPLE_AppDefined, "Driver %s does not exist",
                        m_format.c_str());
            return false;
        }

        const CPLStringList aosCreationOptions(m_creationOptions);
        poDstDSUniquePtr.reset(
            poOutDrv->Create(m_outputDataset.GetName().c_str(), 0, 0, 0,
                             GDT_Unknown, aosCreationOptions.List()));
        poDstDS = poDstDSUniquePtr.get();
        if (!poDstDS)
            return false;
    }

    OGRLayer *poDstLayer = nullptr;

    if (m_outputLayerName.empty())
    {
        if (bNewDataset)
        {
            auto poOutDrv = poDstDS->GetDriver();
            if (poOutDrv && EQUAL(poOutDrv->GetDescription(), "ESRI Shapefile"))
                m_outputLayerName =
                    CPLGetBasenameSafe(m_outputDataset.GetName().c_str());
            else
                m_outputLayerName = "output";
        }
        else if (m_appendLayer)
        {
            if (poDstDS->GetLayerCount() == 1)
                poDstLayer = poDstDS->GetLayer(0);
            else
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "--output-layer should be specified");
                return false;
            }
        }
        else if (m_overwriteLayer)
        {
            if (poDstDS->GetLayerCount() == 1)
            {
                if (poDstDS->DeleteLayer(0) != OGRERR_NONE)
                {
                    return false;
                }
            }
            else
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "--output-layer should be specified");
                return false;
            }
        }
        else
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "--output-layer should be specified");
            return false;
        }
    }
    else if (m_overwriteLayer)
    {
        const int nLayerIdx = poDstDS->GetLayerIndex(m_outputLayerName.c_str());
        if (nLayerIdx < 0)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Layer '%s' does not exist", m_outputLayerName.c_str());
            return false;
        }
        if (poDstDS->DeleteLayer(nLayerIdx) != OGRERR_NONE)
        {
            return false;
        }
    }
    else if (m_appendLayer)
    {
        poDstLayer = poDstDS->GetLayerByName(m_outputLayerName.c_str());
        if (!poDstLayer)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Layer '%s' does not exist", m_outputLayerName.c_str());
            return false;
        }
    }

    if (!bNewDataset && m_update && !m_appendLayer && !m_overwriteLayer)
    {
        poDstLayer = poDstDS->GetLayerByName(m_outputLayerName.c_str());
        if (poDstLayer)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Output layer '%s' already exists. Specify "
                        "--%s, --%s, --%s or "
                        "--%s + --output-layer with a different name",
                        m_outputLayerName.c_str(), GDAL_ARG_NAME_OVERWRITE,
                        GDAL_ARG_NAME_OVERWRITE_LAYER, GDAL_ARG_NAME_APPEND,
                        GDAL_ARG_NAME_UPDATE);
            return false;
        }
    }

    OGRLayer *poInputLayer;
    if (m_inputLayerName.empty() && poSrcDS->GetLayerCount() == 1)
        poInputLayer = poSrcDS->GetLayer(0);
    else
        poInputLayer = poSrcDS->GetLayerByName(m_inputLayerName.c_str());
    if (!poInputLayer)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Cannot get input layer '%s'",
                    m_inputLayerName.c_str());
        return false;
    }

    OGRLayer *poMethodLayer;
    if (m_methodLayerName.empty() && poMethodDS->GetLayerCount() == 1)
        poMethodLayer = poMethodDS->GetLayer(0);
    else
        poMethodLayer = poMethodDS->GetLayerByName(m_methodLayerName.c_str());
    if (!poMethodLayer)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Cannot get method layer '%s'",
                    m_methodLayerName.c_str());
        return false;
    }

    if (bNewDataset || !m_appendLayer)
    {
        const CPLStringList aosLayerCreationOptions(m_layerCreationOptions);

        const OGRwkbGeometryType eType =
            !m_geometryType.empty() ? OGRFromOGCGeomType(m_geometryType.c_str())
                                    : poInputLayer->GetGeomType();
        poDstLayer = poDstDS->CreateLayer(m_outputLayerName.c_str(),
                                          poInputLayer->GetSpatialRef(), eType,
                                          aosLayerCreationOptions.List());
    }
    if (!poDstLayer)
        return false;

    CPLStringList aosOptions;

    if (m_inputFields.empty() && !m_noInputFields)
        m_allInputFields = true;

    if (m_methodFields.empty() && !m_noMethodFields && !m_allMethodFields)
    {
        if (m_operation == "update" || m_operation == "clip" ||
            m_operation == "erase")
            m_noMethodFields = true;
        else
            m_allMethodFields = true;
    }

    if (m_noInputFields && m_noMethodFields)
    {
        aosOptions.SetNameValue("ADD_INPUT_FIELDS", "NO");
        aosOptions.SetNameValue("ADD_METHOD_FIELDS", "NO");
    }
    else
    {
        // Copy fields from input or method layer to output layer
        const auto CopyFields =
            [poDstLayer](OGRLayer *poSrcLayer, const std::string &prefix,
                         const std::vector<std::string> &srcFields)
        {
            const auto contains =
                [](const std::vector<std::string> &v, const std::string &s)
            { return std::find(v.begin(), v.end(), s) != v.end(); };

            const auto poOutFDefn = poDstLayer->GetLayerDefn();
            const auto poFDefn = poSrcLayer->GetLayerDefn();
            const int nCount = poFDefn->GetFieldCount();
            for (int i = 0; i < nCount; ++i)
            {
                const auto poSrcFieldDefn = poFDefn->GetFieldDefn(i);
                const char *pszName = poSrcFieldDefn->GetNameRef();
                if (srcFields.empty() || contains(srcFields, pszName))
                {
                    OGRFieldDefn oField(*poSrcFieldDefn);
                    const std::string outName = prefix + pszName;
                    whileUnsealing(&oField)->SetName(outName.c_str());
                    if (poOutFDefn->GetFieldIndex(outName.c_str()) < 0 &&
                        poDstLayer->CreateField(&oField) != OGRERR_NONE)
                    {
                        return false;
                    }
                }
            }
            return true;
        };

        if (!m_noInputFields)
        {
            if (!GetArg("input-prefix")->IsExplicitlySet() &&
                m_inputPrefix.empty() && !m_noMethodFields)
            {
                m_inputPrefix = "input_";
            }
            if (!m_inputPrefix.empty())
            {
                aosOptions.SetNameValue("INPUT_PREFIX", m_inputPrefix.c_str());
            }
            if (!CopyFields(poInputLayer, m_inputPrefix, m_inputFields))
                return false;
        }

        if (!m_noMethodFields)
        {
            if (!GetArg("method-prefix")->IsExplicitlySet() &&
                m_methodPrefix.empty() && !m_noInputFields)
            {
                m_methodPrefix = "method_";
            }
            if (!m_methodPrefix.empty())
            {
                aosOptions.SetNameValue("METHOD_PREFIX",
                                        m_methodPrefix.c_str());
            }
            if (!CopyFields(poMethodLayer, m_methodPrefix, m_methodFields))
                return false;
        }
    }

    if (OGR_GT_IsSubClassOf(poDstLayer->GetGeomType(), wkbGeometryCollection))
    {
        aosOptions.SetNameValue("PROMOTE_TO_MULTI", "YES");
    }

    const std::map<std::string, decltype(&OGRLayer::Union)>
        mapOperationToMethod = {
            {"union", &OGRLayer::Union},
            {"intersection", &OGRLayer::Intersection},
            {"sym-difference", &OGRLayer::SymDifference},
            {"identity", &OGRLayer::Identity},
            {"update", &OGRLayer::Update},
            {"clip", &OGRLayer::Clip},
            {"erase", &OGRLayer::Erase},
        };

    const auto oIter = mapOperationToMethod.find(m_operation);
    CPLAssert(oIter != mapOperationToMethod.end());
    const auto pFunc = oIter->second;
    const bool bOK =
        (poInputLayer->*pFunc)(poMethodLayer, poDstLayer, aosOptions.List(),
                               pfnProgress, pProgressData) == OGRERR_NONE;
    if (bOK && !m_outputDataset.GetDatasetRef())
    {
        m_outputDataset.Set(std::move(poDstDSUniquePtr));
    }

    return bOK;
#else
    (void)pfnProgress;
    (void)pProgressData;
    ReportError(CE_Failure, CPLE_NotSupported,
                "This algorithm is only supported for builds against GEOS");
    return false;
#endif
}

//! @endcond
