/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "update" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_update.h"

#include "ogr_p.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*        GDALVectorUpdateAlgorithm::GDALVectorUpdateAlgorithm()        */
/************************************************************************/

GDALVectorUpdateAlgorithm::GDALVectorUpdateAlgorithm(bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      ConstructorOptions()
                                          .SetStandaloneStep(standaloneStep)
                                          .SetInputDatasetMaxCount(1)
                                          .SetAddInputLayerNameArgument(false)
                                          .SetAddDefaultArguments(false))
{
    if (standaloneStep)
    {
        AddVectorInputArgs(false);
    }
    else
    {
        AddVectorHiddenInputDatasetArg();
    }

    {
        auto &layerArg = AddArg(GDAL_ARG_NAME_INPUT_LAYER, 0,
                                _("Input layer name"), &m_inputLayerNames)
                             .SetMaxCount(1);
        auto inputArg = GetArg(GDAL_ARG_NAME_INPUT);
        if (inputArg)
            SetAutoCompleteFunctionForLayerName(layerArg, *inputArg);
    }

    AddProgressArg();

    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_VECTOR)
        .SetDatasetInputFlags(GADV_NAME | GADV_OBJECT);
    AddOutputOpenOptionsArg(&m_outputOpenOptions);
    AddOutputLayerNameArg(&m_outputLayerName);

    m_update = true;
    AddUpdateArg(&m_update).SetDefault(true).SetHidden();

    AddArg("mode", 0, _("Set update mode"), &m_mode)
        .SetDefault(m_mode)
        .SetChoices(MODE_MERGE, MODE_UPDATE_ONLY, MODE_APPEND_ONLY);

    AddArg("key", 0, _("Field(s) used as a key to identify features"), &m_key)
        .SetPackedValuesAllowed(false);
}

/************************************************************************/
/*                 GDALVectorUpdateAlgorithm::RunStep()                 */
/************************************************************************/

bool GDALVectorUpdateAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    auto poDstDS = m_outputDataset.GetDatasetRef();
    CPLAssert(poDstDS);
    CPLAssert(poDstDS->GetAccess() == GA_Update);

    auto poSrcDriver = poSrcDS->GetDriver();
    auto poDstDriver = poDstDS->GetDriver();
    if (poSrcDS == poDstDS ||
        (poSrcDriver && poDstDriver &&
         !EQUAL(poSrcDriver->GetDescription(), "MEM") &&
         !EQUAL(poDstDriver->GetDescription(), "MEM") &&
         strcmp(poSrcDS->GetDescription(), poDstDS->GetDescription()) == 0))
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Input and output datasets must be different");
        return false;
    }

    if (m_inputLayerNames.empty() && poSrcDS->GetLayerCount() == 1)
    {
        m_inputLayerNames.push_back(poSrcDS->GetLayer(0)->GetName());
    }
    if (m_outputLayerName.empty() && poDstDS->GetLayerCount() == 1)
    {
        m_outputLayerName = poDstDS->GetLayer(0)->GetName();
    }

    if (m_inputLayerNames.empty())
    {
        if (!m_outputLayerName.empty())
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Please specify the 'input-layer' argument.");
            return false;
        }
        else
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Please specify the 'input-layer' and 'output-layer' "
                        "arguments.");
            return false;
        }
    }

    auto poSrcLayer = poSrcDS->GetLayerByName(m_inputLayerNames[0].c_str());
    if (!poSrcLayer)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "No layer named '%s' in input dataset.",
                    m_inputLayerNames[0].c_str());
        return false;
    }

    if (m_outputLayerName.empty())
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Please specify the 'output-layer' argument.");
        return false;
    }

    auto poDstLayer = poDstDS->GetLayerByName(m_outputLayerName.c_str());
    if (!poDstLayer)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "No layer named '%s' in output dataset",
                    m_outputLayerName.c_str());
        return false;
    }

    std::vector<int> srcKeyFieldIndices;
    std::vector<OGRFieldType> keyFieldTypes;
    if (m_key.empty())
        m_key.push_back(SpecialFieldNames[SPF_FID]);
    for (const std::string &key : m_key)
    {
        if (EQUAL(key.c_str(), SpecialFieldNames[SPF_FID]))
        {
            srcKeyFieldIndices.push_back(
                poSrcLayer->GetLayerDefn()->GetFieldCount() + SPF_FID);
            keyFieldTypes.push_back(OFTInteger64);
            continue;
        }

        const int nSrcIdx =
            poSrcLayer->GetLayerDefn()->GetFieldIndex(key.c_str());
        if (nSrcIdx < 0)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot find field '%s' in input layer", key.c_str());
            return false;
        }
        srcKeyFieldIndices.push_back(nSrcIdx);
        const auto poSrcFieldDefn =
            poSrcLayer->GetLayerDefn()->GetFieldDefn(nSrcIdx);
        const auto eType = poSrcFieldDefn->GetType();
        const OGRFieldType aeAllowedTypes[] = {OFTString, OFTInteger,
                                               OFTInteger64, OFTReal};
        if (std::find(std::begin(aeAllowedTypes), std::end(aeAllowedTypes),
                      eType) == std::end(aeAllowedTypes))
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Type of field '%s' is not one of those supported for "
                        "a key field: String, Integer, Integer64, Real",
                        key.c_str());
            return false;
        }

        const int nDstIdx =
            poDstLayer->GetLayerDefn()->GetFieldIndex(key.c_str());
        if (nDstIdx < 0)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot find field '%s' in output layer", key.c_str());
            return false;
        }
        const auto poDstFieldDefn =
            poDstLayer->GetLayerDefn()->GetFieldDefn(nDstIdx);
        if (poDstFieldDefn->GetType() != eType)
        {
            ReportError(
                CE_Failure, CPLE_NotSupported,
                "Type of field '%s' is not the same in input and output layers",
                key.c_str());
            return false;
        }
        keyFieldTypes.push_back(eType);
    }

    const bool bFIDMatch = m_key.size() == 1 &&
                           EQUAL(m_key[0].c_str(), SpecialFieldNames[SPF_FID]);
    const GIntBig nFeatureCount =
        ctxt.m_pfnProgress ? poSrcLayer->GetFeatureCount(true) : -1;

    std::string osFilter;
    int nIter = 0;
    bool bRet = true;
    for (const auto &poSrcFeature : *poSrcLayer)
    {
        ++nIter;
        if (ctxt.m_pfnProgress && nFeatureCount > 0 &&
            !ctxt.m_pfnProgress(static_cast<double>(nIter) / nFeatureCount, "",
                                ctxt.m_pProgressData))
        {
            ReportError(CE_Failure, CPLE_UserInterrupt, "Interrupted by user");
            bRet = false;
            break;
        }

        std::unique_ptr<OGRFeature> poDstFeature;
        if (bFIDMatch)
        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            poDstFeature.reset(poDstLayer->GetFeature(poSrcFeature->GetFID()));
        }
        else
        {
            bool bSkip = false;
            osFilter.clear();
            for (size_t iField = 0; iField < srcKeyFieldIndices.size();
                 ++iField)
            {
                const int nSrcFieldIdx = srcKeyFieldIndices[iField];
                if (!poSrcFeature->IsFieldSet(nSrcFieldIdx))
                {
                    bSkip = true;
                    break;
                }
                if (!osFilter.empty())
                    osFilter += " AND ";
                osFilter += CPLString(m_key[iField]).SQLQuotedIdentifier();
                osFilter += " = ";
                switch (keyFieldTypes[iField])
                {
                    case OFTString:
                    {
                        osFilter += CPLString(poSrcFeature->GetFieldAsString(
                                                  nSrcFieldIdx))
                                        .SQLQuotedLiteral();
                        break;
                    }

                    case OFTReal:
                    {
                        osFilter += CPLSPrintf(
                            "%.17g",
                            poSrcFeature->GetFieldAsDouble(nSrcFieldIdx));
                        break;
                    }

                    default:
                    {
                        osFilter += CPLSPrintf(
                            CPL_FRMT_GIB,
                            poSrcFeature->GetFieldAsInteger64(nSrcFieldIdx));
                        break;
                    }
                }
            }
            if (bSkip)
                continue;
            if (poDstLayer->SetAttributeFilter(osFilter.c_str()) != OGRERR_NONE)
            {
                bRet = false;
                break;
            }
            poDstFeature.reset(poDstLayer->GetNextFeature());
            if (poDstFeature)
            {
                // Check there is only one feature matching the criterion
                if (std::unique_ptr<OGRFeature>(poDstLayer->GetNextFeature()))
                {
                    poDstFeature.reset();
                }
                else
                {
                    CPLDebugOnly("GDAL",
                                 "Updating output feature " CPL_FRMT_GIB
                                 " with src input " CPL_FRMT_GIB,
                                 poDstFeature->GetFID(),
                                 poSrcFeature->GetFID());
                }
            }
        }

        if (poDstFeature)
        {
            if (m_mode != MODE_APPEND_ONLY)
            {
                auto poDstFeatureOri =
                    std::unique_ptr<OGRFeature>(poDstFeature->Clone());
                const auto nDstFID = poDstFeature->GetFID();
                poDstFeature->SetFrom(poSrcFeature.get());
                // restore FID unset by SetFrom()
                poDstFeature->SetFID(nDstFID);
                if (!poDstFeature->Equal(poDstFeatureOri.get()) &&
                    poDstLayer->SetFeature(poDstFeature.get()) != OGRERR_NONE)
                {
                    bRet = false;
                    break;
                }
            }
        }
        else if (m_mode != MODE_UPDATE_ONLY)
        {
            poDstFeature =
                std::make_unique<OGRFeature>(poDstLayer->GetLayerDefn());
            poDstFeature->SetFrom(poSrcFeature.get());
            if (poDstLayer->CreateFeature(poDstFeature.get()) != OGRERR_NONE)
            {
                bRet = false;
                break;
            }
        }
    }

    poDstLayer->SetAttributeFilter(nullptr);

    return bRet;
}

/************************************************************************/
/*                ~GDALVectorUpdateAlgorithmStandalone()                */
/************************************************************************/

GDALVectorUpdateAlgorithmStandalone::~GDALVectorUpdateAlgorithmStandalone() =
    default;

/************************************************************************/
/*            GDALVectorUpdateAlgorithmStandalone::RunImpl()            */
/************************************************************************/

bool GDALVectorUpdateAlgorithmStandalone::RunImpl(GDALProgressFunc pfnProgress,
                                                  void *pProgressData)
{
    GDALPipelineStepRunContext stepCtxt;
    stepCtxt.m_pfnProgress = pfnProgress;
    stepCtxt.m_pProgressData = pProgressData;
    return RunStep(stepCtxt);
}

//! @endcond
