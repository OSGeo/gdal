/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector create" subcommand
 * Author:   Alessandro Pasotti <elpaso at itopen dot it>
 *
 ******************************************************************************
 * Copyright (c) 2026, Alessandro Pasotti <elpaso at itopen dot it>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include <regex>
#include "gdalalg_vector_create.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*        GDALVectorCreateAlgorithm::GDALVectorCreateAlgorithm()        */
/************************************************************************/

GDALVectorCreateAlgorithm::GDALVectorCreateAlgorithm(bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(
          NAME, DESCRIPTION, HELP_URL,
          ConstructorOptions()
              .SetStandaloneStep(standaloneStep)
              .SetOutputFormatCreateCapability(GDAL_DCAP_CREATE)
              .SetAddDefaultArguments(false)
              .SetAddAppendLayerArgument(false)
              .SetAddUpdateArgument(true)
              .SetAddUpsertArgument(false)
              .SetAddSkipErrorsArgument(false)
              .SetAddOverwriteLayerArgument(true)
              .SetAddInputLayerNameArgument(false)
              .SetInputDatasetRequired(false)
              .SetInputDatasetPositional(false)
              .SetInputDatasetMaxCount(1)
              .SetAddOutputLayerNameArgument(true)),
      m_crs(), m_geometryType(), m_geometryFieldName("geom"),
      m_fieldDefinitions(), m_fieldStrDefinitions()
{

    AddVectorOutputArgs(/* hiddenForCLI = */ false,
                        /* shortNameOutputLayerAllowed=*/true);
    AddGeometryTypeArg(&m_geometryType, _("Layer geometry type"));

    // Add optional geometry field name argument, not all drivers support it, and if not specified, the default "geom" name will be used.
    AddArg("geometry-field", 0,
           _("Name of the geometry field to create (if supported by the output "
             "format)"),
           &m_geometryFieldName)
        .SetMetaVar("GEOMETRY-FIELD")
        .SetDefault("geom");

    AddArg("crs", 0, _("Set CRS"), &m_crs)
        .AddHiddenAlias("srs")
        .SetRequired()
        .SetIsCRSArg(/*noneAllowed=*/false);

    // Add field definition argument
    AddFieldDefinitionArg(&m_fieldStrDefinitions, &m_fieldDefinitions,
                          _("Add a field definition to the output layer"))
        .SetRepeatedArgAllowed(true);
}

/************************************************************************/
/*                 GDALVectorCreateAlgorithm::RunStep()                 */
/************************************************************************/

bool GDALVectorCreateAlgorithm::RunStep(GDALPipelineStepRunContext &)
{

    const auto datasetName = m_outputDataset.GetName();
    auto outputLayerName =
        m_outputLayerName.empty() ? datasetName : m_outputLayerName;
    std::unique_ptr<GDALDataset> poDstDS;

    if (m_standaloneStep)
    {
        if (m_format.empty())
        {
            const auto aosFormats =
                CPLStringList(GDALGetOutputDriversForDatasetName(
                    m_outputDataset.GetName().c_str(), GDAL_OF_VECTOR,
                    /* bSingleMatch = */ true,
                    /* bWarn = */ true));
            if (aosFormats.size() != 1)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Cannot guess driver for %s",
                            m_outputDataset.GetName().c_str());
                return false;
            }
            m_format = aosFormats[0];
        }
    }
    else
    {
        m_format = "MEM";
    }

    auto poDstDriver =
        GetGDALDriverManager()->GetDriverByName(m_format.c_str());
    if (!poDstDriver)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Cannot find driver %s",
                    m_format.c_str());
        return false;
    }

    const OGRwkbGeometryType eDstType =
        m_geometryType.empty() ? wkbUnknown
                               : OGRFromOGCGeomType(m_geometryType.c_str());

    poDstDS.reset(poDstDriver->Create(datasetName.c_str(), 0, 0, 0, GDT_Unknown,
                                      CPLStringList(m_creationOptions)));

    if (poDstDriver && EQUAL(poDstDriver->GetDescription(), "ESRI Shapefile") &&
        EQUAL(CPLGetExtensionSafe(poDstDS->GetDescription()).c_str(), "shp") &&
        poDstDS->GetLayerCount() <= 1)
    {
        outputLayerName = CPLGetBasenameSafe(poDstDS->GetDescription());
    }

    auto poDstLayer = poDstDS->GetLayerByName(outputLayerName.c_str());
    if (poDstLayer)
    {
        if (GetOverwriteLayer())
        {
            int iLayer = -1;
            const int nLayerCount = poDstDS->GetLayerCount();
            for (iLayer = 0; iLayer < nLayerCount; iLayer++)
            {
                if (poDstDS->GetLayer(iLayer) == poDstLayer)
                    break;
            }

            if (iLayer < nLayerCount)
            {
                if (poDstDS->DeleteLayer(iLayer) != OGRERR_NONE)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Cannot delete layer '%s'",
                                outputLayerName.c_str());
                    return false;
                }
            }
            poDstLayer = nullptr;
        }
        else if (!GetAppendLayer())
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Layer '%s' already exists. Specify the "
                        "--%s option to overwrite it, or --%s "
                        "to append to it.",
                        outputLayerName.c_str(), GDAL_ARG_NAME_OVERWRITE_LAYER,
                        GDAL_ARG_NAME_APPEND);
            return false;
        }
    }
    else if (GetAppendLayer() || GetOverwriteLayer())
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Cannot find layer '%s'",
                    outputLayerName.c_str());
        return false;
    }

    std::unique_ptr<OGRSpatialReference> poSRS =
        std::make_unique<OGRSpatialReference>();
    if (!m_crs.empty())
    {
        if (poSRS->SetFromUserInput(m_crs.c_str()) != OGRERR_NONE)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot parse CRS definition: '%s'", m_crs.c_str());
            return false;
        }
    }

    if (!poDstLayer)
    {
        std::unique_ptr<OGRGeomFieldDefn> poGeomFieldDefn =
            std::make_unique<OGRGeomFieldDefn>(m_geometryFieldName.c_str(),
                                               eDstType);
        poGeomFieldDefn->SetSpatialRef(poSRS.release());
        poDstLayer = poDstDS->CreateLayer(
            outputLayerName.c_str(), poGeomFieldDefn.release(),
            CPLStringList(GetLayerCreationOptions()).List());
        if (!poDstLayer)
        {
            ReportError(CE_Failure, CPLE_AppDefined, "Cannot create layer '%s'",
                        outputLayerName.c_str());
            return false;
        }

        // Get all fields definitions
        const auto fieldDefinitions = GetOutputFields();

        for (const auto &oFieldDefn : fieldDefinitions)
        {

            if (poDstLayer->CreateField(&oFieldDefn) != OGRERR_NONE)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Cannot create field '%s' in layer '%s'",
                            oFieldDefn.GetNameRef(), outputLayerName.c_str());
                return false;
            }
        }
    }

    m_outputDataset.Set(std::move(poDstDS));
    return true;
}

/************************************************************************/
/*                 GDALVectorCreateAlgorithm::RunImpl()                 */
/************************************************************************/
bool GDALVectorCreateAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                        void *pProgressData)
{
    GDALPipelineStepRunContext stepCtxt;
    stepCtxt.m_pfnProgress = pfnProgress;
    stepCtxt.m_pProgressData = pProgressData;
    return RunPreStepPipelineValidations() && RunStep(stepCtxt);
}

/************************************************************************/
/*             GDALVectorCreateAlgorithm::GetOutputFields()             */
/************************************************************************/
std::vector<OGRFieldDefn> GDALVectorCreateAlgorithm::GetOutputFields() const
{
    // TODO: read OGR_SCHEMA and populate m_fieldDefinitions accordingly, before applying -field arguments on top of it.
    return m_fieldDefinitions;
}

/************************************************************************/
/*                ~GDALVectorCreateAlgorithmStandalone()                */
/************************************************************************/
GDALVectorCreateAlgorithmStandalone::~GDALVectorCreateAlgorithmStandalone() =
    default;

//! @endcond
