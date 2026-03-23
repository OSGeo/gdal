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
#include "gdal_utils.h"
#include "ogr_schema_override.h"

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

              // Remove defaults because input is the optional template
              .SetAddDefaultArguments(false)

              // For --like input template
              .SetAutoOpenInputDatasets(true)
              .SetInputDatasetAlias("like")
              .SetInputDatasetRequired(false)
              .SetInputDatasetPositional(false)
              .SetInputDatasetMaxCount(1)
              .SetInputDatasetMetaVar("TEMPLATE-DATASET")

              // Remove arguments that don't make sense in a create context
              // Note: this is required despite SetAddDefaultArguments(false)
              .SetAddUpsertArgument(false)
              .SetAddSkipErrorsArgument(false)
              .SetAddAppendLayerArgument(false)

              ),
      m_crs(), m_geometryType(), m_geometryFieldName("geom"),
      m_schemaJsonOrPath(), m_fieldDefinitions(), m_fieldStrDefinitions()
{

    AddVectorInputArgs(false);
    AddVectorOutputArgs(/* hiddenForCLI = */ false,
                        /* shortNameOutputLayerAllowed=*/false);
    AddGeometryTypeArg(&m_geometryType, _("Layer geometry type"));

    // Add optional geometry field name argument, not all drivers support it, and if not specified, the default "geom" name will be used.
    const auto &geomFieldNameArg =
        AddArg("geometry-field", 0,
               _("Name of the geometry field to create (if supported by the "
                 "output "
                 "format)"),
               &m_geometryFieldName)
            .SetMetaVar("GEOMETRY-FIELD")
            .SetDefault("geom");

    AddArg("crs", 0, _("Set CRS"), &m_crs)
        .AddHiddenAlias("srs")
        .SetIsCRSArg(/*noneAllowed=*/false);

    constexpr auto inputMutexGroup = "like-schema-field";

    // Apply mutex to GDAL_ARG_NAME_INPUT
    // This is hackish and I really don't like const_cast but I couldn't find another way.
    const_cast<GDALAlgorithmArgDecl &>(
        GetArg(GDAL_ARG_NAME_INPUT)->GetDeclaration())
        .SetMutualExclusionGroup(inputMutexGroup);

    // Add --schema argument to read OGR_SCHEMA and populate field definitions from it. It is mutually exclusive with --like and --field arguments.
    AddArg("schema", 0,
           _("Read OGR_SCHEMA and populate field definitions from it"),
           &m_schemaJsonOrPath)
        .SetMetaVar("SCHEMA_JSON")
        .SetRepeatedArgAllowed(false)
        .SetMutualExclusionGroup(inputMutexGroup);

    // Add field definition argument
    AddFieldDefinitionArg(&m_fieldStrDefinitions, &m_fieldDefinitions,
                          _("Add a field definition to the output layer"))
        .SetMetaVar("<NAME>:<TYPE>[(,<WIDTH>[,<PRECISION>])]")
        .SetPackedValuesAllowed(false)
        .SetRepeatedArgAllowed(true)
        .SetMutualExclusionGroup(inputMutexGroup);

    AddValidationAction(
        [this, &geomFieldNameArg]()
        {
            if ((!m_schemaJsonOrPath.empty() || !m_inputDataset.empty()) &&
                ((!m_geometryFieldName.empty() &&
                  geomFieldNameArg.IsExplicitlySet()) ||
                 !m_geometryType.empty() || !m_fieldDefinitions.empty() ||
                 !m_crs.empty()))
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "When --schema or --like is specified, "
                            "--geometry-field, --geometry-type, "
                            "--field and --crs options must not be specified.");
                return false;
            }
            if (!m_geometryType.empty() && m_crs.empty())
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "When --geometry-type is specified, --crs must "
                            "also be specified");
                return false;
            }
            return true;
        });
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
    poDstDS.reset(GDALDataset::Open(datasetName.c_str(),
                                    GDAL_OF_VECTOR | GDAL_OF_UPDATE, nullptr,
                                    nullptr, nullptr));

    if (poDstDS && !m_update)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Dataset %s already exists. Specify the "
                    "--%s option to open it in update mode.",
                    datasetName.c_str(), GDAL_ARG_NAME_UPDATE);
        return false;
    }

    GDALDataset *poSrcDS = m_inputDataset.empty()
                               ? nullptr
                               : m_inputDataset.front().GetDatasetRef();

    OGRSchemaOverride oSchemaOverride;

    const auto loadJSON = [this,
                           &oSchemaOverride](const std::string &source) -> bool
    {
        // This error count is necessary because LoadFromJSON tries to load
        // the content as a file first (and set an error it if fails) then tries
        // to load as a JSON string but even if it succeeds an error is still
        // set and not cleared.
        const auto nErrorCount = CPLGetErrorCounter();
        if (!oSchemaOverride.LoadFromJSON(source,
                                          /* allowGeometryFields */ true))
        {
            // Get the last error message and report it, since LoadFromJSON doesn't do it itself.
            if (nErrorCount != CPLGetErrorCounter())
            {
                const std::string lastErrorMsg = CPLGetLastErrorMsg();
                CPLErrorReset();
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Cannot parse OGR_SCHEMA: %s.",
                            lastErrorMsg.c_str());
            }
            else
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Cannot parse OGR_SCHEMA (unknown error).");
            }
            return false;
        }
        else if (nErrorCount != CPLGetErrorCounter())
        {
            CPLErrorReset();
        }
        return true;
    };

    // Use the input dataset as to create an OGR_SCHEMA
    if (poSrcDS)
    {
        // Export the schema using GDALVectorInfo
        CPLStringList aosOptions;

        aosOptions.AddString("-schema");

        // Must be last, as positional
        aosOptions.AddString("dummy");
        aosOptions.AddString("-al");

        GDALVectorInfoOptions *psInfo =
            GDALVectorInfoOptionsNew(aosOptions.List(), nullptr);

        char *ret = GDALVectorInfo(GDALDataset::ToHandle(poSrcDS), psInfo);
        GDALVectorInfoOptionsFree(psInfo);
        if (!ret)
            return false;

        if (!loadJSON(ret))
        {
            CPLFree(ret);
            return false;
        }
        CPLFree(ret);
    }
    else if (!m_schemaJsonOrPath.empty() && !loadJSON(m_schemaJsonOrPath))
    {
        return false;
    }

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
        ReportError(CE_Failure, CPLE_AppDefined, "Cannot find driver %s.",
                    m_format.c_str());
        return false;
    }

    if (!poDstDS)
        poDstDS.reset(poDstDriver->Create(datasetName.c_str(), 0, 0, 0,
                                          GDT_Unknown,
                                          CPLStringList(m_creationOptions)));

    if (!poDstDS)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Cannot create dataset %s.",
                    datasetName.c_str());
        return false;
    }

    if (EQUAL(poDstDriver->GetDescription(), "ESRI Shapefile") &&
        EQUAL(CPLGetExtensionSafe(poDstDS->GetDescription()).c_str(), "shp") &&
        poDstDS->GetLayerCount() <= 1)
    {
        outputLayerName = CPLGetBasenameSafe(outputLayerName.c_str());
    }

    // An OGR_SCHEMA has been provided
    if (!oSchemaOverride.GetLayerOverrides().empty())
    {
        // Checks if input layer names were specified and the layers exists in the schema
        if (!m_inputLayerNames.empty())
        {
            for (const auto &inputLayerName : m_inputLayerNames)
            {
                if (!oSchemaOverride.GetLayerOverride(inputLayerName).IsValid())
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "The specified input layer name '%s' doesn't "
                                "exist in the provided template or schema.",
                                inputLayerName.c_str());
                    return false;
                }
            }
        }

        // If there are multiple layers check if the destination format supports
        // multiple layers, and if not, error out.
        if (oSchemaOverride.GetLayerOverrides().size() > 1 &&
            !GDALGetMetadataItem(poDstDriver, GDAL_DCAP_MULTIPLE_VECTOR_LAYERS,
                                 nullptr) &&
            m_inputLayerNames.size() != 1)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "The output format %s doesn't support multiple layers.",
                        poDstDriver->GetDescription());
            return false;
        }

        // If output layer name was specified and there is more than one layer in the schema,
        // error out since we won't know which layer to apply it to
        if (!m_outputLayerName.empty() &&
            oSchemaOverride.GetLayerOverrides().size() > 1 &&
            m_inputLayerNames.size() != 1)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Output layer name should not be specified when there "
                        "are multiple layers in the schema.");
            return false;
        }

        std::vector<std::string> layersToBeCreated;
        for (const auto &oLayerOverride : oSchemaOverride.GetLayerOverrides())
        {

            if (!m_inputLayerNames.empty() &&
                std::find(m_inputLayerNames.begin(), m_inputLayerNames.end(),
                          oLayerOverride.GetLayerName()) ==
                    m_inputLayerNames.end())
            {
                // This layer is not in the list of input layers to consider, so skip it
                continue;
            }
            layersToBeCreated.push_back(oLayerOverride.GetLayerName());
        }

        // Loop over layers in the OGR_SCHEMA and create them
        for (const auto &layerToCreate : layersToBeCreated)
        {
            const auto oLayerOverride =
                oSchemaOverride.GetLayerOverride(layerToCreate);
            if (!oLayerOverride.IsValid())
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Invalid layer override for layer '%s'.",
                            layerToCreate.c_str());
                return false;
            }

            // We can use the defined layer name only if there is a single layer to be created
            const std::string userSpecifiedNewName =
                !m_outputLayerName.empty() ? m_outputLayerName
                                           : oLayerOverride.GetLayerName();
            const std::string outputLayerNewName =
                layersToBeCreated.size() > 1 ? oLayerOverride.GetLayerName()
                                             : userSpecifiedNewName;

            if (!CreateLayer(poDstDS.get(), outputLayerNewName,
                             oLayerOverride.GetFieldDefinitions(),
                             oLayerOverride.GetGeomFieldDefinitions()))
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Cannot create layer '%s'",
                            oLayerOverride.GetLayerName().c_str());
                return false;
            }
        }
    }
    else
    {
        std::vector<OGRGeomFieldDefn> geometryFieldDefinitions;
        if (!m_geometryType.empty())
        {
            const OGRwkbGeometryType eDstType =
                OGRFromOGCGeomType(m_geometryType.c_str());
            if (eDstType == wkbUnknown)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Unsupported geometry type: '%s'.",
                            m_geometryType.c_str());
                return false;
            }
            else
            {
                OGRGeomFieldDefn oGeomFieldDefn(m_geometryFieldName.c_str(),
                                                eDstType);
                std::unique_ptr<OGRSpatialReference> poSRS =
                    std::make_unique<OGRSpatialReference>();
                if (!m_crs.empty())
                {
                    if (poSRS->SetFromUserInput(m_crs.c_str()) != OGRERR_NONE)
                    {
                        ReportError(CE_Failure, CPLE_AppDefined,
                                    "Cannot parse CRS definition: '%s'.",
                                    m_crs.c_str());
                        return false;
                    }
                    else
                    {
                        oGeomFieldDefn.SetSpatialRef(poSRS.release());
                    }
                }
                geometryFieldDefinitions.push_back(oGeomFieldDefn);
            }
        }

        if (EQUAL(poDstDriver->GetDescription(), "ESRI Shapefile") &&
            EQUAL(CPLGetExtensionSafe(poDstDS->GetDescription()).c_str(),
                  "shp") &&
            poDstDS->GetLayerCount() <= 1)
        {
            outputLayerName = CPLGetBasenameSafe(poDstDS->GetDescription());
        }

        if (!CreateLayer(poDstDS.get(), outputLayerName, GetOutputFields(),
                         geometryFieldDefinitions))
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot create layer '%s'.", outputLayerName.c_str());
            return false;
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
    // This is where we will eventually implement override logic to modify field
    // definitions based on input dataset and/or OGR_SCHEMA, but for now we just
    // return the field definitions as specified by the user through --field arguments.
    return m_fieldDefinitions;
}

/************************************************************************/
/*               GDALVectorCreateAlgorithm::CreateLayer()               */
/************************************************************************/
bool GDALVectorCreateAlgorithm::CreateLayer(
    GDALDataset *poDstDS, const std::string &layerName,
    const std::vector<OGRFieldDefn> &fieldDefinitions,
    const std::vector<OGRGeomFieldDefn> &geometryFieldDefinitions) const
{

    auto poDstLayer = poDstDS->GetLayerByName(layerName.c_str());

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
                                "Cannot delete layer '%s'.", layerName.c_str());
                    return false;
                }
            }
            poDstLayer = nullptr;
        }
        else
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Layer '%s' already exists. Specify the "
                        "--%s option to overwrite it.",
                        layerName.c_str(), GDAL_ARG_NAME_OVERWRITE_LAYER);
            return false;
        }
    }
    else if (GetOverwriteLayer())
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Cannot find layer '%s'.",
                    layerName.c_str());
        return false;
    }

    // Get the geometry field definition, if any
    std::unique_ptr<OGRGeomFieldDefn> poGeomFieldDefn;
    if (!geometryFieldDefinitions.empty())
    {
        if (geometryFieldDefinitions.size() > 1)
        {
            // NOTE: this limitation may eventually be removed,
            // but for now we don't want to deal with the complexity
            // of creating multiple geometry fields with various drivers that
            // may or may not support it
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Multiple geometry fields are not supported.");
            return false;
        }
        poGeomFieldDefn =
            std::make_unique<OGRGeomFieldDefn>(geometryFieldDefinitions[0]);
    }

    if (!poDstLayer)
    {

        poDstLayer = poDstDS->CreateLayer(
            layerName.c_str(), poGeomFieldDefn.release(),
            CPLStringList(GetLayerCreationOptions()).List());
    }

    if (!poDstLayer)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Cannot create layer '%s'.",
                    layerName.c_str());
        return false;
    }

    for (const auto &oFieldDefn : fieldDefinitions)
    {
        if (poDstLayer->CreateField(&oFieldDefn) != OGRERR_NONE)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot create field '%s' in layer '%s'.",
                        oFieldDefn.GetNameRef(), layerName.c_str());
            return false;
        }
    }

    return true;
}

/************************************************************************/
/*                ~GDALVectorCreateAlgorithmStandalone()                */
/************************************************************************/
GDALVectorCreateAlgorithmStandalone::~GDALVectorCreateAlgorithmStandalone() =
    default;

//! @endcond
