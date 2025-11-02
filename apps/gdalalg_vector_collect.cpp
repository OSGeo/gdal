/******************************************************************************
*
 * Project:  GDAL
 * Purpose:  "gdal vector collect" subcommand
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_collect.h"

#include "cpl_error.h"
#include "gdal_priv.h"
#include "gdalalg_vector_geom.h"
#include "ogr_geometry.h"

#include <cinttypes>

#ifndef _
#define _(x) (x)
#endif

//! @cond Doxygen_Suppress

GDALVectorCollectAlgorithm::GDALVectorCollectAlgorithm(bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddArg("group-by", 0,
           _("Names of field(s) by which inputs should be grouped"), &m_groupBy)
        .AddValidationAction(
            [this]()
            {
                auto fields = m_groupBy;

                std::sort(fields.begin(), fields.end());
                if (std::adjacent_find(fields.begin(), fields.end()) !=
                    fields.end())
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "--group-by must be a list of unique field names.");
                    return false;
                }
                return true;
            });
}

namespace
{
class GDALVectorCollectDataset : public GDALVectorNonStreamingAlgorithmDataset
{
  public:
    explicit GDALVectorCollectDataset(const std::vector<std::string> &groupBy)
        : m_groupBy(groupBy)
    {
    }

    bool Process(OGRLayer &srcLayer, OGRLayer &dstLayer) override
    {
        std::map<std::vector<std::string>, std::unique_ptr<OGRFeature>>
            oDstFeatures{};
        const int nGeomFields = srcLayer.GetLayerDefn()->GetGeomFieldCount();

        std::vector<int> srcFieldIndices;
        for (const auto &fieldName : m_groupBy)
        {
            // RunStep already checked that the field exists
            srcFieldIndices.push_back(
                srcLayer.GetLayerDefn()->GetFieldIndex(fieldName.c_str()));
        }

        for (const auto &srcFeature : srcLayer)
        {
            std::vector<std::string> fieldValues(srcFieldIndices.size());
            for (size_t iDstField = 0; iDstField < srcFieldIndices.size();
                 iDstField++)
            {
                const int iSrcField = srcFieldIndices[iDstField];
                fieldValues[iDstField] =
                    srcFeature->GetFieldAsString(iSrcField);
            }

            OGRFeature *dstFeature;

            if (auto it = oDstFeatures.find(fieldValues);
                it == oDstFeatures.end())
            {
                oDstFeatures[fieldValues] =
                    std::make_unique<OGRFeature>(dstLayer.GetLayerDefn());
                dstFeature = oDstFeatures[fieldValues].get();

                // TODO compute field index from:to map and reuse
                dstFeature->SetFrom(srcFeature.get());

                for (int iGeomField = 0; iGeomField < nGeomFields; iGeomField++)
                {
                    const OGRGeomFieldDefn *poGeomDefn =
                        dstLayer.GetLayerDefn()->GetGeomFieldDefn(iGeomField);
                    const auto eGeomType = poGeomDefn->GetType();

                    dstFeature->SetGeomFieldDirectly(
                        iGeomField,
                        OGRGeometryFactory::createGeometry(eGeomType));
                }
            }
            else
            {
                dstFeature = it->second.get();
            }

            for (int iGeomField = 0; iGeomField < nGeomFields; iGeomField++)
            {
                std::unique_ptr<OGRGeometry> poSrcGeom(
                    srcFeature->StealGeometry(iGeomField));
                if (poSrcGeom != nullptr)
                {
                    OGRGeometryCollection *poDstGeom =
                        cpl::down_cast<OGRGeometryCollection *>(
                            dstFeature->GetGeomFieldRef(iGeomField));
                    poDstGeom->addGeometry(std::move(poSrcGeom));
                }
            }
        }

        for (const auto &[_, poDstFeature] : oDstFeatures)
        {
            if (dstLayer.CreateFeature(poDstFeature.get()) != OGRERR_NONE)
            {
                return false;
            }
        }

        return true;
    }

  private:
    std::vector<std::string> m_groupBy{};
};
}  // namespace

bool GDALVectorCollectAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    auto poDstDS = std::make_unique<GDALVectorCollectDataset>(m_groupBy);

    for (auto &&poSrcLayer : poSrcDS->GetLayers())
    {
        if (m_inputLayerNames.empty() ||
            std::find(m_inputLayerNames.begin(), m_inputLayerNames.end(),
                      poSrcLayer->GetDescription()) != m_inputLayerNames.end())
        {
            const auto poSrcLayerDefn = poSrcLayer->GetLayerDefn();
            if (poSrcLayerDefn->GetGeomFieldCount() == 0)
            {
                if (m_inputLayerNames.empty())
                    continue;
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Specified layer '%s' has no geometry field",
                            poSrcLayer->GetDescription());
                return false;
            }

            OGRFeatureDefn dstDefn(poSrcLayerDefn->GetName());

            // Copy attribute fields specified with --group-by, discard others
            for (const auto &fieldName : m_groupBy)
            {
                const int iSrcFieldIndex =
                    poSrcLayerDefn->GetFieldIndex(fieldName.c_str());
                if (iSrcFieldIndex == -1)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Specified attribute field '%s' does not exist "
                                "in layer '%s'",
                                fieldName.c_str(),
                                poSrcLayer->GetDescription());
                    return false;
                }

                const OGRFieldDefn *poFieldDefn =
                    poSrcLayerDefn->GetFieldDefn(iSrcFieldIndex);
                dstDefn.AddFieldDefn(poFieldDefn);
            }

            // Copy all geometry fields, upgrading the type to the corresponding
            // collection type.
            for (int iGeomField = 0;
                 iGeomField < poSrcLayerDefn->GetGeomFieldCount(); iGeomField++)
            {
                const OGRGeomFieldDefn *srcGeomDefn =
                    poSrcLayerDefn->GetGeomFieldDefn(iGeomField);
                const auto eSrcGeomType = srcGeomDefn->GetType();
                auto eDstGeomType = OGR_GT_GetCollection(eSrcGeomType);
                if (eDstGeomType == wkbUnknown)
                {
                    eDstGeomType = wkbGeometryCollection;
                }

                if (iGeomField == 0)
                {
                    dstDefn.DeleteGeomFieldDefn(0);
                }

                auto dstGeomDefn = std::make_unique<OGRGeomFieldDefn>(
                    srcGeomDefn->GetNameRef(), eDstGeomType);
                dstGeomDefn->SetSpatialRef(srcGeomDefn->GetSpatialRef());
                dstDefn.AddGeomFieldDefn(std::move(dstGeomDefn));
            }
            //poDstDS->SetSourceGeometryField(geomFieldIndex);

            if (!poDstDS->AddProcessedLayer(*poSrcLayer, dstDefn))
            {
                return false;
            }
        }
    }

    m_outputDataset.Set(std::move(poDstDS));

    return true;
}

GDALVectorCollectAlgorithmStandalone::~GDALVectorCollectAlgorithmStandalone() =
    default;

//! @endcond
