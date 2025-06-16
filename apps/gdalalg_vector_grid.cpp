/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector grid" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_grid.h"
#include "gdalalg_vector_grid_average.h"
#include "gdalalg_vector_grid_data_metrics.h"
#include "gdalalg_vector_grid_invdist.h"
#include "gdalalg_vector_grid_invdistnn.h"
#include "gdalalg_vector_grid_linear.h"
#include "gdalalg_vector_grid_nearest.h"

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*            GDALVectorGridAlgorithm::GDALVectorGridAlgorithm()        */
/************************************************************************/

GDALVectorGridAlgorithm::GDALVectorGridAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    RegisterSubAlgorithm<GDALVectorGridAverageAlgorithm>();
    RegisterSubAlgorithm<GDALVectorGridInvdistAlgorithm>();
    RegisterSubAlgorithm<GDALVectorGridInvdistNNAlgorithm>();
    RegisterSubAlgorithm<GDALVectorGridLinearAlgorithm>();
    RegisterSubAlgorithm<GDALVectorGridNearestAlgorithm>();
    RegisterSubAlgorithm<GDALVectorGridMinimumAlgorithm>();
    RegisterSubAlgorithm<GDALVectorGridMaximumAlgorithm>();
    RegisterSubAlgorithm<GDALVectorGridRangeAlgorithm>();
    RegisterSubAlgorithm<GDALVectorGridCountAlgorithm>();
    RegisterSubAlgorithm<GDALVectorGridAverageDistanceAlgorithm>();
    RegisterSubAlgorithm<GDALVectorGridAverageDistancePointsAlgorithm>();
}

/************************************************************************/
/*                GDALVectorGeomAlgorithm::RunImpl()                    */
/************************************************************************/

bool GDALVectorGridAlgorithm::RunImpl(GDALProgressFunc, void *)
{
    CPLError(CE_Failure, CPLE_AppDefined,
             "The Run() method should not be called directly on the \"gdal "
             "vector grid\" program.");
    return false;
}

/************************************************************************/
/*                 GDALVectorGridAbstractAlgorithm()                    */
/************************************************************************/

GDALVectorGridAbstractAlgorithm::GDALVectorGridAbstractAlgorithm(
    const std::string &name, const std::string &description,
    const std::string &helpURL)
    : GDALAlgorithm(name, description, helpURL)
{
    AddProgressArg();
    AddOutputFormatArg(&m_outputFormat)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_DCAP_RASTER, GDAL_DCAP_CREATE});
    AddOpenOptionsArg(&m_openOptions);
    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_VECTOR});
    AddInputDatasetArg(&m_inputDataset, GDAL_OF_VECTOR);
    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_RASTER);
    AddCreationOptionsArg(&m_creationOptions);
    AddArg("extent", 0, _("Set the target georeferenced extent"),
           &m_targetExtent)
        .SetMinCount(4)
        .SetMaxCount(4)
        .SetRepeatedArgAllowed(false)
        .SetMetaVar("<xmin>,<ymin>,<xmax>,<ymax>");
    AddArg("resolution", 0, _("Set the target resolution"), &m_targetResolution)
        .SetMinCount(2)
        .SetMaxCount(2)
        .SetRepeatedArgAllowed(false)
        .SetMetaVar("<xres>,<yres>")
        .SetMutualExclusionGroup("size-or-resolution");
    AddArg("size", 0, _("Set the target size in pixels and lines"),
           &m_targetSize)
        .SetMinCount(2)
        .SetMaxCount(2)
        .SetRepeatedArgAllowed(false)
        .SetMetaVar("<xsize>,<ysize>")
        .SetMutualExclusionGroup("size-or-resolution");
    AddOutputDataTypeArg(&m_outputType).SetDefault(m_outputType);
    AddArg("crs", 0, _("Override the projection for the output file"), &m_crs)
        .AddHiddenAlias("srs")
        .SetIsCRSArg(/*noneAllowed=*/false);
    AddOverwriteArg(&m_overwrite);
    AddLayerNameArg(&m_layers).SetMutualExclusionGroup("layer-sql");
    AddArg("sql", 0, _("SQL statement"), &m_sql)
        .SetReadFromFileAtSyntaxAllowed()
        .SetMetaVar("<statement>|@<filename>")
        .SetRemoveSQLCommentsEnabled()
        .SetMutualExclusionGroup("layer-sql");
    AddBBOXArg(
        &m_bbox,
        _("Select only points contained within the specified bounding box"));
    AddArg("zfield", 0, _("Field name from which to get Z values."), &m_zField)
        .AddHiddenAlias("z-field");
    AddArg("zoffset", 0,
           _("Value to add to the Z field value (applied before zmultiply)"),
           &m_zOffset)
        .SetDefault(m_zOffset)
        .AddHiddenAlias("z-offset");
    AddArg("zmultiply", 0,
           _("Multiplication factor for the Z field value (applied after "
             "zoffset)"),
           &m_zMultiply)
        .SetDefault(m_zMultiply)
        .AddHiddenAlias("z-multiply");

    AddValidationAction(
        [this]()
        {
            bool ret = true;
            if (!m_targetResolution.empty() && m_targetExtent.empty())
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "'resolution' should be defined when 'extent' is.");
                ret = false;
            }
            return ret;
        });
}

/************************************************************************/
/*               GDALVectorGridAbstractAlgorithm::RunImpl()             */
/************************************************************************/

bool GDALVectorGridAbstractAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                              void *pProgressData)
{
    auto poSrcDS = m_inputDataset.GetDatasetRef();
    CPLAssert(poSrcDS);
    CPLAssert(!m_outputDataset.GetDatasetRef());

    CPLStringList aosOptions;

    if (!m_outputFormat.empty())
    {
        aosOptions.AddString("-of");
        aosOptions.AddString(m_outputFormat.c_str());
    }

    for (const auto &co : m_creationOptions)
    {
        aosOptions.AddString("-co");
        aosOptions.AddString(co.c_str());
    }

    if (!m_targetExtent.empty())
    {
        aosOptions.AddString("-txe");
        aosOptions.AddString(CPLSPrintf("%.17g", m_targetExtent[0]));
        aosOptions.AddString(CPLSPrintf("%.17g", m_targetExtent[2]));
        aosOptions.AddString("-tye");
        aosOptions.AddString(CPLSPrintf("%.17g", m_targetExtent[1]));
        aosOptions.AddString(CPLSPrintf("%.17g", m_targetExtent[3]));
    }

    if (!m_bbox.empty())
    {
        aosOptions.AddString("-clipsrc");
        for (double v : m_bbox)
        {
            aosOptions.AddString(CPLSPrintf("%.17g", v));
        }
    }

    if (!m_targetResolution.empty())
    {
        aosOptions.AddString("-tr");
        for (double targetResolution : m_targetResolution)
        {
            aosOptions.AddString(CPLSPrintf("%.17g", targetResolution));
        }
    }

    if (!m_targetSize.empty())
    {
        aosOptions.AddString("-outsize");
        for (int targetSize : m_targetSize)
        {
            aosOptions.AddString(CPLSPrintf("%d", targetSize));
        }
    }

    if (!m_outputType.empty())
    {
        aosOptions.AddString("-ot");
        aosOptions.AddString(m_outputType.c_str());
    }

    if (!m_crs.empty())
    {
        aosOptions.AddString("-a_srs");
        aosOptions.AddString(m_crs.c_str());
    }

    if (m_sql.empty())
    {
        for (const std::string &layer : m_layers)
        {
            aosOptions.AddString("-l");
            aosOptions.AddString(layer.c_str());
        }
    }
    else
    {
        aosOptions.AddString("-sql");
        aosOptions.AddString(m_sql.c_str());
    }

    if (m_zOffset != 0)
    {
        aosOptions.AddString("-z_increase");
        aosOptions.AddString(CPLSPrintf("%.17g", m_zOffset));
    }

    if (m_zMultiply != 0)
    {
        aosOptions.AddString("-z_multiply");
        aosOptions.AddString(CPLSPrintf("%.17g", m_zMultiply));
    }

    if (!m_zField.empty())
    {
        aosOptions.AddString("-zfield");
        aosOptions.AddString(m_zField.c_str());
    }
    else if (m_sql.empty())
    {
        const auto CheckLayer = [this](OGRLayer *poLayer)
        {
            auto poFeat =
                std::unique_ptr<OGRFeature>(poLayer->GetNextFeature());
            poLayer->ResetReading();
            if (poFeat)
            {
                const auto poGeom = poFeat->GetGeometryRef();
                if (!poGeom || !poGeom->Is3D())
                {
                    ReportError(
                        CE_Warning, CPLE_AppDefined,
                        "At least one geometry of layer '%s' lacks a Z "
                        "component. You may need to set the 'zfield' argument",
                        poLayer->GetName());
                    return false;
                }
            }
            return true;
        };

        if (m_layers.empty())
        {
            for (auto poLayer : poSrcDS->GetLayers())
            {
                if (!CheckLayer(poLayer))
                    break;
            }
        }
        else
        {
            for (const std::string &layerName : m_layers)
            {
                auto poLayer = poSrcDS->GetLayerByName(layerName.c_str());
                if (poLayer && !CheckLayer(poLayer))
                    break;
            }
        }
    }

    aosOptions.AddString("-a");
    aosOptions.AddString(GetGridAlgorithm().c_str());

    std::unique_ptr<GDALGridOptions, decltype(&GDALGridOptionsFree)> psOptions{
        GDALGridOptionsNew(aosOptions.List(), nullptr), GDALGridOptionsFree};

    if (!psOptions)
    {
        return false;
    }

    GDALGridOptionsSetProgress(psOptions.get(), pfnProgress, pProgressData);

    auto poRetDS = std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(
        GDALGrid(m_outputDataset.GetName().c_str(),
                 GDALDataset::ToHandle(poSrcDS), psOptions.get(), nullptr)));

    if (poRetDS)
    {
        m_outputDataset.Set(std::move(poRetDS));
    }

    return m_outputDataset.GetDatasetRef() != nullptr;
}

/************************************************************************/
/*            GDALVectorGridAbstractAlgorithm::AddRadiusArg()           */
/************************************************************************/

GDALInConstructionAlgorithmArg &GDALVectorGridAbstractAlgorithm::AddRadiusArg()
{
    return AddArg("radius", 0, _("Radius of the search circle"), &m_radius)
        .SetMutualExclusionGroup("radius");
}

/************************************************************************/
/*       GDALVectorGridAbstractAlgorithm::AddRadius1AndRadius2Arg()      */
/************************************************************************/

void GDALVectorGridAbstractAlgorithm::AddRadius1AndRadius2Arg()
{
    AddArg("radius1", 0, _("First axis of the search ellipse"), &m_radius1)
        .SetMutualExclusionGroup("radius");
    AddArg("radius2", 0, _("Second axis of the search ellipse"), &m_radius2);

    AddValidationAction(
        [this]()
        {
            bool ret = true;
            if (m_radius1 > 0 && m_radius2 == 0)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "'radius2' should be defined when 'radius1' is.");
                ret = false;
            }
            else if (m_radius2 > 0 && m_radius1 == 0)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "'radius1' should be defined when 'radius2' is.");
                ret = false;
            }
            return ret;
        });
}

/************************************************************************/
/*            GDALVectorGridAbstractAlgorithm::AddAngleArg()            */
/************************************************************************/

GDALInConstructionAlgorithmArg &GDALVectorGridAbstractAlgorithm::AddAngleArg()
{
    return AddArg("angle", 0,
                  _("Angle of search ellipse rotation in degrees (counter "
                    "clockwise)"),
                  &m_angle)
        .SetDefault(m_angle);
}

/************************************************************************/
/*           GDALVectorGridAbstractAlgorithm::AddMinPointsArg()         */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALVectorGridAbstractAlgorithm::AddMinPointsArg()
{
    return AddArg("min-points", 0, _("Minimum number of data points to use"),
                  &m_minPoints)
        .SetDefault(m_minPoints);
}

/************************************************************************/
/*           GDALVectorGridAbstractAlgorithm::AddMaxPointsArg()         */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALVectorGridAbstractAlgorithm::AddMaxPointsArg()
{
    return AddArg("max-points", 0, _("Maximum number of data points to use"),
                  &m_maxPoints)
        .SetDefault(m_maxPoints);
}

/************************************************************************/
/*    GDALVectorGridAbstractAlgorithm::AddMinMaxPointsPerQuadrantArg()  */
/************************************************************************/

void GDALVectorGridAbstractAlgorithm::AddMinMaxPointsPerQuadrantArg()
{
    AddArg("min-points-per-quadrant", 0,
           _("Minimum number of data points to use per quadrant"),
           &m_minPointsPerQuadrant)
        .SetDefault(m_minPointsPerQuadrant);
    AddArg("max-points-per-quadrant", 0,
           _("Maximum number of data points to use per quadrant"),
           &m_maxPointsPerQuadrant)
        .SetDefault(m_maxPointsPerQuadrant);
}

/************************************************************************/
/*            GDALVectorGridAbstractAlgorithm::AddNodataArg()           */
/************************************************************************/

GDALInConstructionAlgorithmArg &GDALVectorGridAbstractAlgorithm::AddNodataArg()
{
    return AddArg("nodata", 0, _("Target nodata value"), &m_nodata)
        .SetDefault(m_nodata);
}

//! @endcond
