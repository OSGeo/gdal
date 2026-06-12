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
#include "gdalalg_vector_write.h"

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

GDALVectorLayerAlgebraAlgorithm::GDALVectorLayerAlgebraAlgorithm(
    bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(
          NAME, DESCRIPTION, HELP_URL,
          ConstructorOptions()
              .SetStandaloneStep(standaloneStep)
              .SetInputDatasetMaxCount(1)
              .SetAddInputLayerNameArgument(false)
              .SetAddDefaultArguments(false)
              .SetAddUpsertArgument(false)
              .SetAddSkipErrorsArgument(false)
              .SetOutputLayerNameAvailableInPipelineStep(true)
              .SetOutputFormatCreateCapability(GDAL_DCAP_CREATE))
{
    if (standaloneStep)
    {
        AddProgressArg();
    }

    auto &opArg =
        AddArg("operation", 0, _("Operation to perform"), &m_operation)
            .SetChoices("union", "intersection", "sym-difference", "identity",
                        "update", "clip", "erase")
            .SetRequired();
    if (standaloneStep)
        opArg.SetPositional();

    if (standaloneStep)
    {
        AddVectorInputArgs(false);
    }
    else
    {
        AddVectorHiddenInputDatasetArg();
    }

    {
        auto &arg = AddArg("method", 0, _("Method vector dataset"),
                           &m_methodDataset, GDAL_OF_VECTOR)
                        .SetRequired();
        if (standaloneStep)
            arg.SetPositional();

        SetAutoCompleteFunctionForFilename(arg, GDAL_OF_VECTOR);
    }

    if (standaloneStep)
    {
        AddVectorOutputArgs(false, false);
    }
    else
    {
        AddOutputLayerNameArg(/* hiddenForCLI = */ false,
                              /* shortNameOutputLayerAllowed = */ false);
    }

    AddArg(GDAL_ARG_NAME_INPUT_LAYER, 0, _("Input layer name"),
           &m_inputLayerName);

    AddArg("method-layer", 0, _("Method layer name"), &m_methodLayerName);

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
/*         GDALVectorLayerAlgebraAlgorithm::CanHandleNextStep()         */
/************************************************************************/

bool GDALVectorLayerAlgebraAlgorithm::CanHandleNextStep(
    GDALPipelineStepAlgorithm *poNextStep) const
{
    return poNextStep->GetName() == GDALVectorWriteAlgorithm::NAME &&
           poNextStep->GetOutputFormat() != "stream";
}

/************************************************************************/
/*               GDALRasterPolygonizeAlgorithm::RunImpl()               */
/************************************************************************/

bool GDALVectorLayerAlgebraAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                              void *pProgressData)
{
    GDALPipelineStepRunContext stepCtxt;
    stepCtxt.m_pfnProgress = pfnProgress;
    stepCtxt.m_pProgressData = pProgressData;
    return RunPreStepPipelineValidations() && RunStep(stepCtxt);
}

/************************************************************************/
/*              GDALVectorLayerAlgebraAlgorithm::RunImpl()              */
/************************************************************************/

bool GDALVectorLayerAlgebraAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
#ifdef HAVE_GEOS
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);
    auto poMethodDS = m_methodDataset.GetDatasetRef();
    CPLAssert(poMethodDS);

    if (poSrcDS == poMethodDS)
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Input and method datasets must be different");
        return false;
    }

    auto poWriteStep = ctxt.m_poNextUsableStep ? ctxt.m_poNextUsableStep : this;

    GDALDataset *poDstDS = nullptr;
    bool bTemporaryFile = false;
    std::unique_ptr<GDALDataset> poNewRetDS;
    std::string outputLayerName;
    OGRLayer *poDstLayer = nullptr;
    if (!CreateDatasetSingleOutputLayerIfNeeded(ctxt, "output", poDstDS,
                                                bTemporaryFile, poNewRetDS,
                                                outputLayerName, poDstLayer))
    {
        return false;
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

    if (!poDstLayer)
    {
        const CPLStringList aosLayerCreationOptions(
            poWriteStep->GetLayerCreationOptions());

        const OGRwkbGeometryType eType =
            !m_geometryType.empty() ? OGRFromOGCGeomType(m_geometryType.c_str())
                                    : poInputLayer->GetGeomType();
        poDstLayer = poDstDS->CreateLayer(outputLayerName.c_str(),
                                          poInputLayer->GetSpatialRef(), eType,
                                          aosLayerCreationOptions.List());
        if (!poDstLayer)
            return false;
    }

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
    bool bOK = (poInputLayer->*pFunc)(poMethodLayer, poDstLayer,
                                      aosOptions.List(), ctxt.m_pfnProgress,
                                      ctxt.m_pProgressData) == OGRERR_NONE;
    if (bOK && poNewRetDS)
    {
        if (bTemporaryFile)
        {
            bOK = poNewRetDS->FlushCache() == CE_None;
#if !defined(__APPLE__)
            // For some unknown reason, unlinking the file on MacOSX
            // leads to later "disk I/O error". See https://github.com/OSGeo/gdal/issues/13794
            VSIUnlink(poNewRetDS->GetDescription());
#endif
        }

        m_outputDataset.Set(std::move(poNewRetDS));
    }

    return bOK;
#else
    (void)ctxt;
    ReportError(CE_Failure, CPLE_NotSupported,
                "This algorithm is only supported for builds against GEOS");
    return false;
#endif
}

GDALVectorLayerAlgebraAlgorithmStandalone::
    ~GDALVectorLayerAlgebraAlgorithmStandalone() = default;

//! @endcond
