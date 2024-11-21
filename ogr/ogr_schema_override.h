/******************************************************************************
 * $Id$
 *
 * Project:  OGR_SCHEMA open options handling
 * Purpose:  Class for representing a layer schema override.
 * Author:   Alessandro Pasotti, elpaso@itopen.it
 *
 ******************************************************************************
 * Copyright (c) 2024, Alessandro Pasotti <elpaso at itopen dot it>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_SCHEMA_OVERRIDE_H_INCLUDED
#define OGR_SCHEMA_OVERRIDE_H_INCLUDED

//! @cond Doxygen_Suppress

#include <string>
#include <map>

#include <optional>
#include <ogr_api.h>
#include <ogr_feature.h>
#include <cpl_vsi.h>
#include <cpl_json.h>

/** Class that holds the schema override options for a single field */
class OGRFieldDefnOverride
{
  public:
    OGRFieldDefnOverride()
        : m_osName(), m_eType(), m_eSubType(), m_nWidth(), m_nPrecision()
    {
    }

    void SetFieldName(const std::string &osName)
    {
        m_osName = osName;
    }

    void SetFieldType(OGRFieldType eType)
    {
        m_eType = eType;
    }

    void SetFieldSubType(OGRFieldSubType eSubType)
    {
        m_eSubType = eSubType;
    }

    std::optional<std::string> GetFieldName() const
    {
        return m_osName;
    }

    std::optional<OGRFieldType> GetFieldType() const
    {
        return m_eType;
    }

    std::optional<OGRFieldSubType> GetFieldSubType() const
    {
        return m_eSubType;
    }

    std::optional<int> GetFieldWidth() const
    {
        return m_nWidth;
    }

    std::optional<int> GetFieldPrecision() const
    {
        return m_nPrecision;
    }

    // Considered valid if it carries any change information, otherwise it's considered a no-op
    bool IsValid() const
    {
        return m_osName.has_value() || m_eType.has_value() ||
               m_eSubType.has_value();
    }

  private:
    std::optional<std::string> m_osName;
    std::optional<OGRFieldType> m_eType;
    std::optional<OGRFieldSubType> m_eSubType;
    std::optional<int> m_nWidth;
    std::optional<int> m_nPrecision;
};

/** Class that holds the schema override options for a single layer */
class OGRLayerSchemaOverride
{
  public:
    OGRLayerSchemaOverride() : m_osLayerName(), m_moFieldOverrides()
    {
    }

    OGRLayerSchemaOverride(const std::string &osLayerName)
        : m_osLayerName(osLayerName), m_moFieldOverrides()
    {
    }

    void SetLayerName(const std::string &osLayerName)
    {
        m_osLayerName = osLayerName;
    }

    void AddFieldOverride(const std::string &osFieldName,
                          const OGRFieldDefnOverride &oFieldOverride)
    {
        m_moFieldOverrides[osFieldName] = oFieldOverride;
    }

    const std::string &GetLayerName() const
    {
        return m_osLayerName;
    }

    const std::map<std::string, OGRFieldDefnOverride> &GetFieldOverrides() const
    {
        return m_moFieldOverrides;
    }

    bool IsFullOverride() const
    {
        return m_bIsFullOverride;
    }

    void SetFullOverride(bool bIsFullOverride)
    {
        m_bIsFullOverride = bIsFullOverride;
    }

    bool IsValid() const
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

  private:
    std::string m_osLayerName;
    std::map<std::string, OGRFieldDefnOverride> m_moFieldOverrides;
    bool m_bIsFullOverride = false;
};

/** Class that holds the schema override options for a datasource */
class OGRSchemaOverride
{
  public:
    OGRSchemaOverride() : m_moLayerOverrides()
    {
    }

    void AddLayerOverride(const std::string &osLayerName,
                          const OGRLayerSchemaOverride &oLayerOverride)
    {
        m_moLayerOverrides[osLayerName] = oLayerOverride;
    }

    bool LoadFromJSON(const std::string &osJSON)
    {
        std::string osFieldsSchemaOverride;
        bool bFieldsSchemaOverrideIsFilePath{false};

        // Try to load the content of the file
        GByte *pabyRet = nullptr;
        if (VSIIngestFile(nullptr, osJSON.c_str(), &pabyRet, nullptr, -1) ==
            TRUE)
        {
            bFieldsSchemaOverrideIsFilePath = true;
            osFieldsSchemaOverride =
                std::string(reinterpret_cast<char *>(pabyRet));
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
                        const auto osSchemaType =
                            oLayer.GetString("schemaType");
                        // Default schemaType is "Patch"
                        const auto bSchemaFullOverride =
                            CPLString(osSchemaType).tolower() == "full";
                        OGRLayerSchemaOverride oLayerOverride(osLayerName);
                        oLayerOverride.SetFullOverride(bSchemaFullOverride);

                        if (oLayerFields.Size() > 0 && !osLayerName.empty())
                        {
                            for (const auto &oField : oLayerFields)
                            {
                                const auto osFieldName =
                                    oField.GetString("name");
                                if (osFieldName.empty())
                                {
                                    CPLError(CE_Warning, CPLE_AppDefined,
                                             "Field name is missing");
                                    return false;
                                }
                                OGRFieldDefnOverride oFieldOverride;

                                const auto oType =
                                    CPLString(oField.GetString("type"))
                                        .tolower();
                                const auto oSubType =
                                    CPLString(oField.GetString("subType"))
                                        .tolower();
                                const auto osNewName =
                                    CPLString(oField.GetString("newName"))
                                        .tolower();

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
                                    if (eSubType == OFSTNone &&
                                        oSubType != "none")
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

                                if (bSchemaFullOverride ||
                                    oFieldOverride.IsValid())
                                {
                                    oLayerOverride.AddFieldOverride(
                                        osFieldName, oFieldOverride);
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
            CPLError(CE_Failure, CPLE_AppDefined,
                     "SCHEMA info is invalid JSON");
            return false;
        }
    }

    const std::map<std::string, OGRLayerSchemaOverride> &
    GetLayerOverrides() const
    {
        return m_moLayerOverrides;
    }

    bool IsValid() const
    {
        bool isValid = !m_moLayerOverrides.empty();
        for (const auto &oLayerOverride : m_moLayerOverrides)
        {
            isValid &= oLayerOverride.second.IsValid();
        }
        return isValid;
    }

  private:
    std::map<std::string, OGRLayerSchemaOverride> m_moLayerOverrides;
};

//! @endcond

#endif /* ndef OGR_FEATURE_H_INCLUDED */
