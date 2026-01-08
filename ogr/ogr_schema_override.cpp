/******************************************************************************
 * Project:  OGR_SCHEMA open options handling
 * Purpose:  Class for representing a layer schema override.
 * Author:   Alessandro Pasotti, elpaso@itopen.it
 *
 ******************************************************************************
 * Copyright (c) 2024, Alessandro Pasotti <elpaso at itopen dot it>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

//! @cond Doxygen_Suppress

#include "ogr_schema_override.h"
#include "ogrsf_frmts.h"

bool OGRSchemaOverride::LoadFromJSON(const std::string &osJSON)
{
    std::string osFieldsSchemaOverride;
    bool bFieldsSchemaOverrideIsFilePath{false};

    // Try to load the content of the file
    GByte *pabyRet = nullptr;
    if (VSIIngestFile(nullptr, osJSON.c_str(), &pabyRet, nullptr, -1) == TRUE)
    {
        bFieldsSchemaOverrideIsFilePath = true;
        osFieldsSchemaOverride = std::string(reinterpret_cast<char *>(pabyRet));
        VSIFree(pabyRet);
    }

    if (!bFieldsSchemaOverrideIsFilePath)
    {
        osFieldsSchemaOverride = osJSON;
    }

    CPLJSONDocument oSchemaDoc;
    if (oSchemaDoc.LoadMemory(osFieldsSchemaOverride))
    {
        const CPLJSONObject oRoot = oSchemaDoc.GetRoot();
        if (oRoot.IsValid())
        {
            const auto aoLayers = oRoot.GetArray("layers");
            // Loop through layer names and get the field details for each field.
            for (const auto &oLayer : aoLayers)
            {
                if (oLayer.IsValid())
                {
                    const auto oLayerFields = oLayer.GetArray("fields");
                    // Parse fields
                    const auto osLayerName = oLayer.GetString("name");
                    const auto osSchemaType = oLayer.GetString("schemaType");
                    // Default schemaType is "Patch"
                    const auto bSchemaFullOverride =
                        CPLString(osSchemaType).tolower() == "full";
                    OGRLayerSchemaOverride oLayerOverride;
                    oLayerOverride.SetLayerName(osLayerName);
                    oLayerOverride.SetFullOverride(bSchemaFullOverride);

                    if (oLayerFields.Size() > 0 && !osLayerName.empty())
                    {
                        for (const auto &oField : oLayerFields)
                        {
                            const auto osFieldName = oField.GetString("name");
                            OGRFieldDefnOverride oFieldOverride;

                            const CPLString oSrcType(
                                CPLString(oField.GetString("srcType"))
                                    .tolower());
                            const CPLString oSrcSubType(
                                CPLString(oField.GetString("srcSubType"))
                                    .tolower());
                            const CPLString oType(
                                CPLString(oField.GetString("type")).tolower());
                            const CPLString oSubType(
                                CPLString(oField.GetString("subType"))
                                    .tolower());
                            const CPLString osNewName(
                                CPLString(oField.GetString("newName"))
                                    .tolower());
                            const auto nWidth = oField.GetInteger("width", 0);
                            const auto nPrecision =
                                oField.GetInteger("precision", 0);

                            if (!osNewName.empty())
                            {
                                oFieldOverride.SetFieldName(osNewName);
                            }

                            if (!oSrcType.empty())
                            {
                                if (bSchemaFullOverride)
                                {
                                    CPLError(CE_Failure, CPLE_AppDefined,
                                             "Non-patch OGR_SCHEMA definition "
                                             "is not allowed with specifying "
                                             "source field type");
                                    return false;
                                }
                                if (!osFieldName.empty() || !osNewName.empty())
                                {
                                    CPLError(CE_Warning, CPLE_AppDefined,
                                             "Field name and source field type "
                                             "are mutually exclusive");
                                    return false;
                                }
                                const OGRFieldType eType =
                                    OGRFieldDefn::GetFieldTypeByName(
                                        oSrcType.c_str());
                                // Check if the field type is valid
                                if (eType == OFTString && oSrcType != "string")
                                {
                                    CPLError(CE_Failure, CPLE_AppDefined,
                                             "Unsupported source field type: "
                                             "%s",
                                             oSrcType.c_str());
                                    return false;
                                }
                                oFieldOverride.SetSrcFieldType(eType);
                            }

                            if (!oSrcSubType.empty())
                            {
                                if (bSchemaFullOverride)
                                {
                                    CPLError(CE_Failure, CPLE_AppDefined,
                                             "Non-patch OGR_SCHEMA definition "
                                             "is not allowed with specifying "
                                             "source field subtype");
                                    return false;
                                }
                                if (!osFieldName.empty() || !osNewName.empty())
                                {
                                    CPLError(CE_Warning, CPLE_AppDefined,
                                             "Field name and source field "
                                             "subtype are mutually exclusive");
                                    return false;
                                }
                                const OGRFieldSubType eSubType =
                                    OGRFieldDefn::GetFieldSubTypeByName(
                                        oSubType.c_str());
                                // Check if the field subType is valid
                                if (eSubType == OFSTNone &&
                                    oSrcSubType != "none")
                                {
                                    CPLError(CE_Failure, CPLE_AppDefined,
                                             "Unsupported source field subType:"
                                             " %s",
                                             oSubType.c_str());
                                    return false;
                                }
                                oFieldOverride.SetSrcFieldSubType(eSubType);
                            }

                            if (oSrcType.empty() && oSrcSubType.empty() &&
                                osFieldName.empty())
                            {
                                CPLError(CE_Warning, CPLE_AppDefined,
                                         "Field name is missing");
                                return false;
                            }

                            if (!oType.empty())
                            {
                                const OGRFieldType eType =
                                    OGRFieldDefn::GetFieldTypeByName(
                                        oType.c_str());
                                // Check if the field type is valid
                                if (eType == OFTString && oType != "string")
                                {
                                    CPLError(CE_Failure, CPLE_AppDefined,
                                             "Unsupported field type: %s "
                                             "for field %s",
                                             oType.c_str(),
                                             osFieldName.c_str());
                                    return false;
                                }
                                oFieldOverride.SetFieldType(eType);
                            }

                            if (!oSubType.empty())
                            {
                                const OGRFieldSubType eSubType =
                                    OGRFieldDefn::GetFieldSubTypeByName(
                                        oSubType.c_str());
                                // Check if the field subType is valid
                                if (eSubType == OFSTNone && oSubType != "none")
                                {
                                    CPLError(CE_Failure, CPLE_AppDefined,
                                             "Unsupported field subType: "
                                             "%s for field %s",
                                             oSubType.c_str(),
                                             osFieldName.c_str());
                                    return false;
                                }
                                oFieldOverride.SetFieldSubType(eSubType);
                            }

                            if (nWidth != 0)
                            {
                                oFieldOverride.SetFieldWidth(nWidth);
                            }

                            if (nPrecision != 0)
                            {
                                oFieldOverride.SetFieldPrecision(nPrecision);
                            }

                            if (bSchemaFullOverride || oFieldOverride.IsValid())
                            {
                                if (osFieldName.empty())
                                {
                                    oLayerOverride.AddUnnamedFieldOverride(
                                        oFieldOverride);
                                }
                                else
                                {
                                    oLayerOverride.AddNamedFieldOverride(
                                        osFieldName, oFieldOverride);
                                }
                            }
                            else
                            {
                                CPLError(CE_Failure, CPLE_AppDefined,
                                         "Field %s has no valid overrides "
                                         "and schemaType is not \"Full\"",
                                         osFieldName.c_str());
                                return false;
                            }
                        }
                    }

                    if (oLayerOverride.IsValid())
                    {
                        AddLayerOverride(oLayerOverride);
                    }
                    else
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Layer %s has no valid overrides",
                                 osLayerName.c_str());
                        return false;
                    }
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "SCHEMA info is invalid JSON");
                    return false;
                }
            }
            return true;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "SCHEMA info is invalid JSON");
            return false;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "SCHEMA info is invalid JSON");
        return false;
    }
}

bool OGRSchemaOverride::IsValid() const
{
    bool isValid = !m_aoLayerOverrides.empty();
    for (const auto &oLayerOverride : m_aoLayerOverrides)
    {
        isValid &= oLayerOverride.IsValid();
    }
    return isValid;
}

bool OGRSchemaOverride::DefaultApply(
    GDALDataset *poDS, const char *pszDebugKey,
    std::function<void(OGRLayer *, int)> callbackWhenRemovingField) const
{
    const auto &oLayerOverrides = GetLayerOverrides();
    for (const auto &oLayerFieldOverride : oLayerOverrides)
    {
        const auto &osLayerName = oLayerFieldOverride.GetLayerName();
        const bool bIsFullOverride{oLayerFieldOverride.IsFullOverride()};
        auto oNamedFieldOverrides =
            oLayerFieldOverride.GetNamedFieldOverrides();
        const auto &oUnnamedFieldOverrides =
            oLayerFieldOverride.GetUnnamedFieldOverrides();

        const auto ProcessLayer =
            [&callbackWhenRemovingField, &osLayerName, &oNamedFieldOverrides,
             &oUnnamedFieldOverrides, bIsFullOverride](OGRLayer *poLayer)
        {
            std::vector<OGRFieldDefn *> aoFields;
            // Patch field definitions
            auto poLayerDefn = poLayer->GetLayerDefn();
            for (int i = 0; i < poLayerDefn->GetFieldCount(); i++)
            {
                auto poFieldDefn = poLayerDefn->GetFieldDefn(i);

                const auto PatchFieldDefn =
                    [poFieldDefn](const OGRFieldDefnOverride &oFieldOverride)
                {
                    if (oFieldOverride.GetFieldType().has_value())
                        whileUnsealing(poFieldDefn)
                            ->SetType(oFieldOverride.GetFieldType().value());
                    if (oFieldOverride.GetFieldWidth().has_value())
                        whileUnsealing(poFieldDefn)
                            ->SetWidth(oFieldOverride.GetFieldWidth().value());
                    if (oFieldOverride.GetFieldPrecision().has_value())
                        whileUnsealing(poFieldDefn)
                            ->SetPrecision(
                                oFieldOverride.GetFieldPrecision().value());
                    if (oFieldOverride.GetFieldSubType().has_value())
                        whileUnsealing(poFieldDefn)
                            ->SetSubType(
                                oFieldOverride.GetFieldSubType().value());
                    if (oFieldOverride.GetFieldName().has_value())
                        whileUnsealing(poFieldDefn)
                            ->SetName(
                                oFieldOverride.GetFieldName().value().c_str());
                };

                auto oFieldOverrideIter =
                    oNamedFieldOverrides.find(poFieldDefn->GetNameRef());
                if (oFieldOverrideIter != oNamedFieldOverrides.cend())
                {
                    const auto &oFieldOverride = oFieldOverrideIter->second;
                    PatchFieldDefn(oFieldOverride);

                    if (bIsFullOverride)
                    {
                        aoFields.push_back(poFieldDefn);
                    }
                    oNamedFieldOverrides.erase(oFieldOverrideIter);
                }
                else
                {
                    for (const auto &oFieldOverride : oUnnamedFieldOverrides)
                    {
                        if ((!oFieldOverride.GetSrcFieldType().has_value() ||
                             oFieldOverride.GetSrcFieldType().value() ==
                                 poFieldDefn->GetType()) &&
                            (!oFieldOverride.GetSrcFieldSubType().has_value() ||
                             oFieldOverride.GetSrcFieldSubType().value() ==
                                 poFieldDefn->GetSubType()))
                        {
                            PatchFieldDefn(oFieldOverride);
                            break;
                        }
                    }
                }
            }

            // Error if any field override is not found
            if (!oNamedFieldOverrides.empty())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Field %s not found in layer %s",
                         oNamedFieldOverrides.cbegin()->first.c_str(),
                         osLayerName.c_str());
                return false;
            }

            // Remove fields not in the override
            if (bIsFullOverride)
            {
                for (int i = poLayerDefn->GetFieldCount() - 1; i >= 0; i--)
                {
                    auto poFieldDefn = poLayerDefn->GetFieldDefn(i);
                    if (std::find(aoFields.begin(), aoFields.end(),
                                  poFieldDefn) == aoFields.end())
                    {
                        callbackWhenRemovingField(poLayer, i);
                        whileUnsealing(poLayerDefn)->DeleteFieldDefn(i);
                    }
                }
            }

            return true;
        };

        CPLDebug(pszDebugKey, "Applying schema override for layer %s",
                 osLayerName.c_str());

        if (osLayerName == "*")
        {
            for (auto *poLayer : poDS->GetLayers())
            {
                if (!ProcessLayer(poLayer))
                    return false;
            }
        }
        else
        {
            // Fail if the layer name does not exist
            auto poLayer = poDS->GetLayerByName(osLayerName.c_str());
            if (poLayer == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Layer %s not found",
                         osLayerName.c_str());
                return false;
            }
            if (!ProcessLayer(poLayer))
                return false;
        }
    }

    return true;
}

bool OGRLayerSchemaOverride::IsValid() const
{
    bool isValid =
        !m_osLayerName.empty() &&
        (!m_oNamedFieldOverrides.empty() || !m_aoUnnamedFieldOverrides.empty());
    for (const auto &oFieldOverrideIter : m_oNamedFieldOverrides)
    {
        isValid &= !oFieldOverrideIter.first.empty();
        // When schemaType is "full" override we don't need to check if the field
        // overrides are valid: a list of fields to keep is enough.
        if (!m_bIsFullOverride)
        {
            isValid &= oFieldOverrideIter.second.IsValid();
        }
    }
    return isValid;
}

bool OGRFieldDefnOverride::IsValid() const
{
    return m_osName.has_value() || m_eType.has_value() ||
           m_eSubType.has_value() || m_eSrcType.has_value() ||
           m_eSrcSubType.has_value() || m_nWidth.has_value() ||
           m_nPrecision.has_value();
}

//! @endcond
