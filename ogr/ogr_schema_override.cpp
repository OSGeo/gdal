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
                            if (osFieldName.empty())
                            {
                                CPLError(CE_Warning, CPLE_AppDefined,
                                         "Field name is missing");
                                return false;
                            }
                            OGRFieldDefnOverride oFieldOverride;

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
                                oLayerOverride.AddFieldOverride(osFieldName,
                                                                oFieldOverride);
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
                        AddLayerOverride(osLayerName, oLayerOverride);
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
    bool isValid = !m_moLayerOverrides.empty();
    for (const auto &oLayerOverride : m_moLayerOverrides)
    {
        isValid &= oLayerOverride.second.IsValid();
    }
    return isValid;
}

bool OGRLayerSchemaOverride::IsValid() const
{
    bool isValid = !m_osLayerName.empty() && !m_moFieldOverrides.empty();
    for (const auto &oFieldOverride : m_moFieldOverrides)
    {
        isValid &= !oFieldOverride.first.empty();
        // When schemaType is "full" override we don't need to check if the field
        // overrides are valid: a list of fields to keep is enough.
        if (!m_bIsFullOverride)
        {
            isValid &= oFieldOverride.second.IsValid();
        }
    }
    return isValid;
}

bool OGRFieldDefnOverride::IsValid() const
{
    return m_osName.has_value() || m_eType.has_value() ||
           m_eSubType.has_value() || m_nWidth.has_value() ||
           m_nPrecision.has_value();
}

//! @endcond
