/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector compare" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_compare.h"

#include "cpl_conv.h"
#include "cpl_enumerate.h"
#include "gdal_dataset.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*       GDALVectorCompareAlgorithm::GDALVectorCompareAlgorithm()       */
/************************************************************************/

GDALVectorCompareAlgorithm::GDALVectorCompareAlgorithm(bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      ConstructorOptions()
                                          .SetStandaloneStep(standaloneStep)
                                          .SetInputDatasetMaxCount(1)
                                          .SetAddDefaultArguments(false))
{
    if (standaloneStep)
    {
        AddProgressArg();
    }
    else
    {
        AddVectorHiddenInputDatasetArg();
    }

    auto &referenceDatasetArg = AddArg("reference", 0, _("Reference dataset"),
                                       &m_referenceDataset, GDAL_OF_VECTOR)
                                    .SetPositional()
                                    .SetRequired();

    SetAutoCompleteFunctionForFilename(referenceDatasetArg, GDAL_OF_VECTOR);

    if (standaloneStep)
    {
        AddVectorInputArgs(/* hiddenForCLI = */ false);
    }

    AddArg("lax-geometry", 0, _("Lax geometry comparison"),
           &m_laxGeometryComparison);

    AddArg("skip-all-optional", 0, _("Skip all optional comparisons"),
           &m_skipAllOptional);
    AddArg("skip-binary", 0, _("Skip binary file comparison"), &m_skipBinary);
    AddArg("skip-crs", 0, _("Skip CRS comparison"), &m_skipCRS);
    AddArg("skip-metadata", 0, _("Skip metadata comparison"), &m_skipMetadata);
    AddArg("skip-fid", 0, _("Skip FID comparison"), &m_skipFID);

    AddOutputStringArg(&m_output);

    AddArg("return-code", 0, _("Return code"), &m_retCode)
        .SetHiddenForCLI()
        .SetIsInput(false)
        .SetIsOutput(true);
}

/************************************************************************/
/*                GDALVectorCompareAlgorithm::RunStep()                 */
/************************************************************************/

bool GDALVectorCompareAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto poRefDS = m_referenceDataset.GetDatasetRef();
    CPLAssert(poRefDS);

    CPLAssert(m_inputDataset.size() == 1);
    auto poInputDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poInputDS);

    if (m_skipAllOptional)
    {
        m_skipBinary = true;
        m_skipCRS = true;
        m_skipMetadata = true;
        m_skipFID = true;
    }

    if (poRefDS == poInputDS)
    {
        return true;
    }

    std::vector<std::string> aosReport;

    if (!m_skipBinary && poRefDS->GetDriver() == poInputDS->GetDriver() &&
        CPLStringList(poRefDS->GetFileList()).size() == 1 &&
        CPLStringList(poInputDS->GetFileList()).size() == 1)
    {
        if (BinaryComparison(this, aosReport, poRefDS, poInputDS))
        {
            return true;
        }
    }

    if (!m_skipMetadata)
    {
        MetadataComparison(aosReport, "(dataset default metadata domain)",
                           poRefDS->GetMetadata(), poInputDS->GetMetadata());
    }

    if (m_inputLayerNames.empty())
    {
        const int nRefCount = poRefDS->GetLayerCount();
        const int nInputCount = poInputDS->GetLayerCount();
        if (nRefCount != nInputCount)
        {
            aosReport.push_back(CPLSPrintf("Reference dataset has %d layer(s), "
                                           "whereas input dataset has %d",
                                           nRefCount, nInputCount));
        }

        if (nRefCount == 1 && nInputCount == 1)
        {
            // Special case to compare for example 2 shapefiles whose layer name
            // is related to the filename
            if (!CompareLayer(aosReport, poRefDS->GetLayer(0),
                              poInputDS->GetLayer(0), ctxt.m_pfnProgress,
                              ctxt.m_pProgressData))
            {
                return false;
            }
        }
        else
        {
            int iCurLayer = 0;
            for (auto *poRefLayer : poRefDS->GetLayers())
            {
                auto *poInputLayer =
                    poInputDS->GetLayerByName(poRefLayer->GetName());
                if (poInputLayer)
                {
                    std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)>
                        pScaledProgress(
                            GDALCreateScaledProgress(
                                static_cast<double>(iCurLayer) / nRefCount,
                                static_cast<double>(iCurLayer + 1) / nRefCount,
                                ctxt.m_pfnProgress, ctxt.m_pProgressData),
                            GDALDestroyScaledProgress);
                    ++iCurLayer;
                    if (!CompareLayer(aosReport, poRefLayer, poInputLayer,
                                      pScaledProgress ? GDALScaledProgress
                                                      : nullptr,
                                      pScaledProgress.get()))
                    {
                        return false;
                    }
                }
                else
                {
                    aosReport.push_back(
                        CPLSPrintf("Layer %s present in reference dataset is "
                                   "absent from input dataset",
                                   poRefLayer->GetName()));
                }
            }

            for (auto *poInputLayer : poInputDS->GetLayers())
            {
                if (!poRefDS->GetLayerByName(poInputLayer->GetName()))
                {
                    aosReport.push_back(
                        CPLSPrintf("Layer %s present in input dataset is "
                                   "absent from reference dataset",
                                   poInputLayer->GetName()));
                }
            }
        }
    }
    else
    {
        for (const auto &[i, osLayerName] : cpl::enumerate(m_inputLayerNames))
        {
            auto *poRefLayer = poRefDS->GetLayerByName(osLayerName.c_str());
            auto *poInputLayer = poInputDS->GetLayerByName(osLayerName.c_str());
            CPLAssert(poInputLayer);  // guaranteed by GDALAlgorithm
            if (poRefLayer)
            {
                std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)>
                    pScaledProgress(
                        GDALCreateScaledProgress(
                            static_cast<double>(i) /
                                static_cast<double>(m_inputLayerNames.size()),
                            static_cast<double>(i + 1) /
                                static_cast<double>(m_inputLayerNames.size()),
                            ctxt.m_pfnProgress, ctxt.m_pProgressData),
                        GDALDestroyScaledProgress);
                if (!CompareLayer(aosReport, poRefLayer, poInputLayer,
                                  pScaledProgress ? GDALScaledProgress
                                                  : nullptr,
                                  pScaledProgress.get()))
                {
                    return false;
                }
            }
            else
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Layer %s present in input dataset is absent from "
                            "reference dataset",
                            osLayerName.c_str());
                return false;
            }
        }
    }

    // Ignore difference related to DBF_DATE_LAST_UPDATE if no other difference
    if (aosReport.size() == 1 &&
        aosReport[0].find("DBF_DATE_LAST_UPDATE") != std::string::npos)
        aosReport.clear();

    for (const auto &s : aosReport)
    {
        m_output += s;
        m_output += '\n';
    }

    m_retCode = static_cast<int>(aosReport.size());

    return true;
}

/************************************************************************/
/*              GDALVectorCompareAlgorithm::CompareLayer()              */
/************************************************************************/

bool GDALVectorCompareAlgorithm::CompareLayer(
    std::vector<std::string> &aosReport, OGRLayer *poRefLayer,
    OGRLayer *poInputLayer, GDALProgressFunc pfnProgressFunc,
    void *pProgressData)
{
    const bool bSameLayerName =
        EQUAL(poRefLayer->GetName(), poInputLayer->GetName());
    const std::string osLayerCtxt =
        bSameLayerName ? CPLSPrintf("Layer %s: ", poRefLayer->GetName()) : "";

    if (!m_skipMetadata)
    {
        MetadataComparison(
            aosReport,
            CPLSPrintf(
                "(layer%s default metadata domain)",
                bSameLayerName
                    ? std::string(" ").append(poRefLayer->GetName()).c_str()
                    : ""),
            poRefLayer->GetMetadata(), poInputLayer->GetMetadata());
    }

    const OGRFeatureDefn *poRefDefn = poRefLayer->GetLayerDefn();
    const OGRFeatureDefn *poInputDefn = poInputLayer->GetLayerDefn();

    // Compare attribute field definitions
    const int nRefFieldCount = poRefDefn->GetFieldCount();
    const int nInputFieldCount = poInputDefn->GetFieldCount();
    if (nRefFieldCount != nInputFieldCount)
    {
        aosReport.push_back(CPLSPrintf("%sReference layer has %d attribute "
                                       "field(s), whereas input layer has %d",
                                       osLayerCtxt.c_str(), nRefFieldCount,
                                       nInputFieldCount));
    }

    std::vector<int> anMapRefToInputFields;
    for (const auto *poRefFieldDefn : poRefDefn->GetFields())
    {
        const int nInputFieldDefnIdx =
            poInputDefn->GetFieldIndex(poRefFieldDefn->GetNameRef());
        anMapRefToInputFields.push_back(nInputFieldDefnIdx);
        if (nInputFieldDefnIdx >= 0)
        {
            const auto *poInputFieldDefn =
                poInputDefn->GetFieldDefn(nInputFieldDefnIdx);

            if (poRefFieldDefn->GetType() != poInputFieldDefn->GetType())
            {
                aosReport.push_back(CPLSPrintf(
                    "%sField '%s' has type %s in reference layer, but %s in "
                    "input layer",
                    osLayerCtxt.c_str(), poInputFieldDefn->GetNameRef(),
                    OGR_GetFieldTypeName(poRefFieldDefn->GetType()),
                    OGR_GetFieldTypeName(poInputFieldDefn->GetType())));
            }

            if (poRefFieldDefn->GetSubType() != poInputFieldDefn->GetSubType())
            {
                aosReport.push_back(CPLSPrintf(
                    "%sField '%s' has subtype %s in reference layer, but %s in "
                    "input layer",
                    osLayerCtxt.c_str(), poInputFieldDefn->GetNameRef(),
                    OGR_GetFieldSubTypeName(poRefFieldDefn->GetSubType()),
                    OGR_GetFieldSubTypeName(poInputFieldDefn->GetSubType())));
            }

            if (poRefFieldDefn->GetWidth() != poInputFieldDefn->GetWidth())
            {
                aosReport.push_back(CPLSPrintf(
                    "%sField '%s' has width %d in reference layer, but %d in "
                    "input layer",
                    osLayerCtxt.c_str(), poInputFieldDefn->GetNameRef(),
                    poRefFieldDefn->GetWidth(), poInputFieldDefn->GetWidth()));
            }

            if (poRefFieldDefn->GetPrecision() !=
                poInputFieldDefn->GetPrecision())
            {
                aosReport.push_back(CPLSPrintf(
                    "%sField '%s' has precision %d in reference layer, but %d "
                    "in "
                    "input layer",
                    osLayerCtxt.c_str(), poInputFieldDefn->GetNameRef(),
                    poRefFieldDefn->GetPrecision(),
                    poInputFieldDefn->GetPrecision()));
            }

            if (!EQUAL(poRefFieldDefn->GetAlternativeNameRef(),
                       poInputFieldDefn->GetAlternativeNameRef()))
            {
                aosReport.push_back(CPLSPrintf(
                    "%sField '%s' has '%s' as alternative name in reference "
                    "layer, but '%s' in input layer",
                    osLayerCtxt.c_str(), poInputFieldDefn->GetNameRef(),
                    poRefFieldDefn->GetAlternativeNameRef(),
                    poInputFieldDefn->GetAlternativeNameRef()));
            }

            if (poRefFieldDefn->IsNullable() != poInputFieldDefn->IsNullable())
            {
                aosReport.push_back(CPLSPrintf(
                    "%sField '%s' has nullable=%d in reference layer, but "
                    "nullable=%d in input layer",
                    osLayerCtxt.c_str(), poInputFieldDefn->GetNameRef(),
                    poRefFieldDefn->IsNullable(),
                    poInputFieldDefn->IsNullable()));
            }

            if (poRefFieldDefn->IsUnique() != poInputFieldDefn->IsUnique())
            {
                aosReport.push_back(CPLSPrintf(
                    "%sField '%s' has unique constraint=%d in reference layer, "
                    "but unique constraint=%d in input layer",
                    osLayerCtxt.c_str(), poInputFieldDefn->GetNameRef(),
                    poRefFieldDefn->IsUnique(), poInputFieldDefn->IsUnique()));
            }

            if (poRefFieldDefn->IsGenerated() !=
                poInputFieldDefn->IsGenerated())
            {
                aosReport.push_back(CPLSPrintf(
                    "%sField '%s' has generated status=%d in reference layer, "
                    "but generated status=%d in input layer",
                    osLayerCtxt.c_str(), poInputFieldDefn->GetNameRef(),
                    poRefFieldDefn->IsGenerated(),
                    poInputFieldDefn->IsGenerated()));
            }

            if (poRefFieldDefn->GetComment() != poInputFieldDefn->GetComment())
            {
                aosReport.push_back(CPLSPrintf(
                    "%sField '%s' has '%s' as comment in reference layer, but "
                    "'%s' in input layer",
                    osLayerCtxt.c_str(), poInputFieldDefn->GetNameRef(),
                    poRefFieldDefn->GetComment().c_str(),
                    poInputFieldDefn->GetComment().c_str()));
            }

            if (poRefFieldDefn->GetDomainName() !=
                poInputFieldDefn->GetDomainName())
            {
                aosReport.push_back(CPLSPrintf(
                    "%sField '%s' has '%s' as domain name in reference layer, "
                    "but '%s' in input layer",
                    osLayerCtxt.c_str(), poInputFieldDefn->GetNameRef(),
                    poRefFieldDefn->GetDomainName().c_str(),
                    poInputFieldDefn->GetDomainName().c_str()));
            }

            const char *pszRefDefault = poRefFieldDefn->GetDefault();
            const char *pszInputDefault = poInputFieldDefn->GetDefault();
            if (((pszRefDefault != nullptr) != (pszInputDefault != nullptr)) ||
                (pszRefDefault && pszInputDefault &&
                 !EQUAL(pszRefDefault, pszInputDefault)))
            {
                aosReport.push_back(CPLSPrintf(
                    "%sField '%s' has '%s' as default in reference layer, but "
                    "'%s' in input layer",
                    osLayerCtxt.c_str(), poInputFieldDefn->GetNameRef(),
                    pszRefDefault ? pszRefDefault : "(null)",
                    pszInputDefault ? pszInputDefault : "(null)"));
            }
        }
        else
        {
            aosReport.push_back(CPLSPrintf("%sReference layer has field %s, "
                                           "which is absent in input layer",
                                           osLayerCtxt.c_str(),
                                           poRefFieldDefn->GetNameRef()));
        }
    }

    for (const auto *poInputFieldDefn : poInputDefn->GetFields())
    {
        if (poRefDefn->GetFieldIndex(poInputFieldDefn->GetNameRef()) < 0)
        {
            aosReport.push_back(CPLSPrintf("%sInput layer has field %s, which "
                                           "is absent in reference layer",
                                           osLayerCtxt.c_str(),
                                           poInputFieldDefn->GetNameRef()));
        }
    }

    // Compare geometry field definitions
    const int nRefGeomFieldCount = poRefDefn->GetGeomFieldCount();
    const int nInputGeomFieldCount = poInputDefn->GetGeomFieldCount();
    if (nRefGeomFieldCount != nInputGeomFieldCount)
    {
        aosReport.push_back(CPLSPrintf("%sReference layer has %d geometry "
                                       "field(s), whereas input layer has %d",
                                       osLayerCtxt.c_str(), nRefGeomFieldCount,
                                       nInputGeomFieldCount));
    }

    const bool bSingleGeomField =
        nRefGeomFieldCount == 1 && nInputGeomFieldCount == 1;

    std::vector<int> anMapRefToInputGeomFields;
    for (const auto *poRefFieldDefn : poRefDefn->GetGeomFields())
    {
        const int nInputFieldDefnIdx =
            bSingleGeomField
                ? 0
                : poInputDefn->GetGeomFieldIndex(poRefFieldDefn->GetNameRef());
        anMapRefToInputGeomFields.push_back(nInputFieldDefnIdx);
        if (nInputFieldDefnIdx >= 0)
        {
            const auto *poInputFieldDefn =
                poInputDefn->GetGeomFieldDefn(nInputFieldDefnIdx);

            if (poRefFieldDefn->GetType() != poInputFieldDefn->GetType())
            {
                aosReport.push_back(CPLSPrintf(
                    "%sGeometry field '%s' has geometry type %s in reference "
                    "layer, but %s in input layer",
                    osLayerCtxt.c_str(), poInputFieldDefn->GetNameRef(),
                    OGRGeometryTypeToName(poRefFieldDefn->GetType()),
                    OGRGeometryTypeToName(poInputFieldDefn->GetType())));
            }

            if (poRefFieldDefn->IsNullable() != poInputFieldDefn->IsNullable())
            {
                aosReport.push_back(CPLSPrintf(
                    "%sGeometry field '%s' has nullable=%d in reference layer, "
                    "but nullable=%d in input layer",
                    osLayerCtxt.c_str(), poInputFieldDefn->GetNameRef(),
                    poRefFieldDefn->IsNullable(),
                    poInputFieldDefn->IsNullable()));
            }

            if (!m_skipCRS)
            {
                const auto poRefSRS = poRefFieldDefn->GetSpatialRef();
                const auto poInputSRS = poInputFieldDefn->GetSpatialRef();
                if (((poRefSRS != nullptr) != (poInputSRS != nullptr)) ||
                    (poRefSRS && poInputSRS && !poRefSRS->IsSame(poInputSRS)))
                {
                    const char *apszOptions[] = {"FORMAT=WKT2_2019", nullptr};
                    aosReport.push_back(CPLSPrintf(
                        "%sGeometry field '%s' has different CRS in reference "
                        "and input layers. Value in reference layer is %s, "
                        "whereas it is %s in input layer",
                        osLayerCtxt.c_str(), poInputFieldDefn->GetNameRef(),
                        poRefSRS ? poRefSRS->exportToWkt(apszOptions).c_str()
                                 : "null",
                        poInputSRS
                            ? poInputSRS->exportToWkt(apszOptions).c_str()
                            : "null"));
                }
            }
        }
        else
        {
            aosReport.push_back(
                CPLSPrintf("%sReference layer has geometry field '%s', which "
                           "is absent in input layer",
                           osLayerCtxt.c_str(), poRefFieldDefn->GetNameRef()));
        }
    }

    if (!bSingleGeomField)
    {
        for (const auto *poInputFieldDefn : poInputDefn->GetGeomFields())
        {
            if (poRefDefn->GetGeomFieldIndex(poInputFieldDefn->GetNameRef()) <
                0)
            {
                aosReport.push_back(CPLSPrintf(
                    "%sInput layer has geometry field '%s', which is absent in "
                    "reference layer",
                    osLayerCtxt.c_str(), poInputFieldDefn->GetNameRef()));
            }
        }
    }

    const GIntBig nRefFeatureCount =
        pfnProgressFunc ? poRefLayer->GetFeatureCount() : -1;
    const GIntBig nInputFeatureCount =
        pfnProgressFunc ? poInputLayer->GetFeatureCount() : -1;
    if (nRefFeatureCount != nInputFeatureCount && nRefFeatureCount != -1 &&
        nInputFeatureCount != -1)
    {
        aosReport.push_back(CPLSPrintf(
            "%sReference layer has " CPL_FRMT_GIB
            " feature(s), whereas input layer has " CPL_FRMT_GIB,
            osLayerCtxt.c_str(), nRefFeatureCount, nInputFeatureCount));
    }

    // Compare features
    poRefLayer->ResetReading();
    poInputLayer->ResetReading();

    GIntBig nCount = 0;
    while (true)
    {
        auto poRefFeature =
            std::unique_ptr<OGRFeature>(poRefLayer->GetNextFeature());
        auto poInputFeature =
            std::unique_ptr<OGRFeature>(poInputLayer->GetNextFeature());
        if (!poRefFeature)
        {
            if (poInputFeature &&
                (nRefFeatureCount < 0 || nInputFeatureCount < 0))
            {
                GIntBig nExtraFeatures = 1;
                while (
                    std::unique_ptr<OGRFeature>(poInputLayer->GetNextFeature()))
                {
                    nExtraFeatures++;
                }
                aosReport.push_back(CPLSPrintf(
                    "%sInput layer has " CPL_FRMT_GIB
                    " feature(s), whereas reference layer has " CPL_FRMT_GIB,
                    osLayerCtxt.c_str(), nCount + nExtraFeatures, nCount));
            }
            break;
        }

        if (!poInputFeature)
        {
            if (nRefFeatureCount < 0 || nInputFeatureCount < 0)
            {
                GIntBig nExtraFeatures = 1;
                while (
                    std::unique_ptr<OGRFeature>(poRefLayer->GetNextFeature()))
                {
                    nExtraFeatures++;
                }
                aosReport.push_back(CPLSPrintf(
                    "%sReference layer has " CPL_FRMT_GIB
                    " feature(s), whereas input layer has " CPL_FRMT_GIB,
                    osLayerCtxt.c_str(), nCount + nExtraFeatures, nCount));
            }
            break;
        }

        if (!m_skipFID && poRefFeature->GetFID() != poInputFeature->GetFID())
        {
            aosReport.push_back(
                CPLSPrintf("%sFeature at index " CPL_FRMT_GIB
                           " has feature id " CPL_FRMT_GIB
                           " in reference layer, whereas it is " CPL_FRMT_GIB
                           " in input layer",
                           osLayerCtxt.c_str(), nCount, poRefFeature->GetFID(),
                           poInputFeature->GetFID()));
        }

        // Compare attribute field values
        for (int i = 0; i < nRefFieldCount; ++i)
        {
            const int j = anMapRefToInputFields[i];
            if (j >= 0)
            {
                if (!OGRFeature::IsSameFieldValue(poRefFeature.get(), i,
                                                  poInputFeature.get(), j))
                {
                    aosReport.push_back(
                        CPLSPrintf("%sFeature at index " CPL_FRMT_GIB
                                   " has value '%s' for field %s in reference "
                                   "layer, whereas it is '%s' in input layer",
                                   osLayerCtxt.c_str(), nCount,
                                   poRefFeature->GetFieldAsString(i),
                                   poRefDefn->GetFieldDefn(i)->GetNameRef(),
                                   poInputFeature->GetFieldAsString(j)));
                }
            }
        }

        // Compare geometry field values
        for (int i = 0; i < nRefGeomFieldCount; ++i)
        {
            const int j = anMapRefToInputGeomFields[i];
            if (j >= 0)
            {
                const auto poRefGeom = poRefFeature->GetGeomFieldRef(i);
                const auto poInputGeom = poInputFeature->GetGeomFieldRef(j);
                if ((poRefGeom != nullptr) != (poInputGeom != nullptr) ||
                    (poRefGeom && poInputGeom &&
                     !poRefGeom->Equals(poInputGeom)))
                {
                    bool bSame = false;
                    if (poRefGeom && poInputGeom && m_laxGeometryComparison)
                    {
                        if (wkbFlatten(OGR_GT_GetCollection(
                                poRefGeom->getGeometryType())) ==
                            poInputGeom->getGeometryType())
                        {
                            auto poNewGeom = OGRGeometryFactory::forceTo(
                                std::unique_ptr<OGRGeometry>(
                                    poRefGeom->clone()),
                                poInputGeom->getGeometryType());
                            bSame = poNewGeom && poNewGeom->Equals(poInputGeom);
                        }
                        else if (wkbFlatten(OGR_GT_GetCollection(
                                     poInputGeom->getGeometryType())) ==
                                 poRefGeom->getGeometryType())
                        {
                            auto poNewGeom = OGRGeometryFactory::forceTo(
                                std::unique_ptr<OGRGeometry>(
                                    poInputGeom->clone()),
                                poRefGeom->getGeometryType());
                            bSame = poNewGeom && poNewGeom->Equals(poRefGeom);
                        }
                    }
                    if (!bSame)
                    {
                        aosReport.push_back(CPLSPrintf(
                            "%sFeature at index " CPL_FRMT_GIB
                            " has value %s for geometry field '%s' in "
                            "reference layer, whereas it is %s in input layer",
                            osLayerCtxt.c_str(), nCount,
                            poRefGeom ? poRefGeom->exportToWkt().c_str()
                                      : "null",
                            poRefDefn->GetGeomFieldDefn(i)->GetNameRef(),
                            poInputGeom ? poInputGeom->exportToWkt().c_str()
                                        : "null"));
                    }
                }
            }
        }

        if (pfnProgressFunc && nRefFeatureCount > 0 &&
            (nCount < 1000 || (nCount % 128) == 0) &&
            !pfnProgressFunc(static_cast<double>(nCount) /
                                 static_cast<double>(nRefFeatureCount),
                             "", pProgressData))
        {
            ReportError(CE_Failure, CPLE_UserInterrupt, "Interrupted by user");
            return false;
        }

        ++nCount;
    }

    if (pfnProgressFunc && !pfnProgressFunc(1.0, "", pProgressData))
    {
        ReportError(CE_Failure, CPLE_UserInterrupt, "Interrupted by user");
        return false;
    }
    return true;
}

/************************************************************************/
/*               ~GDALVectorCompareAlgorithmStandalone()                */
/************************************************************************/

GDALVectorCompareAlgorithmStandalone::~GDALVectorCompareAlgorithmStandalone() =
    default;

//! @endcond
