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
#include "ogr_p.h"

constexpr char OGR_SCHEMA_UNDEFINED_VALUE[] = "ogr_schema_undefined_value";

void OGRSchemaOverride::AddLayerOverride(
    const OGRLayerSchemaOverride &oLayerOverride)
{
    m_aoLayerOverrides.push_back(oLayerOverride);
}

bool OGRSchemaOverride::LoadFromJSON(const std::string &osJSON,
                                     bool bAllowGeometryFields)
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
                            const CPLString osNullable(
                                CPLString(
                                    oField.GetString(
                                        "nullable", OGR_SCHEMA_UNDEFINED_VALUE))
                                    .tolower());
                            const CPLString osUnique(
                                CPLString(oField.GetString(
                                              "uniqueConstraint",
                                              OGR_SCHEMA_UNDEFINED_VALUE))
                                    .tolower());
                            const CPLString osDefaultValue(CPLString(
                                oField.GetString("defaultValue",
                                                 OGR_SCHEMA_UNDEFINED_VALUE)));
                            const CPLString osAlias(CPLString(oField.GetString(
                                "alias", OGR_SCHEMA_UNDEFINED_VALUE)));
                            const CPLString osComment(
                                CPLString(oField.GetString(
                                    "comment", OGR_SCHEMA_UNDEFINED_VALUE)));
                            const CPLString osDomain(CPLString(oField.GetString(
                                "domainName", OGR_SCHEMA_UNDEFINED_VALUE)));
                            const CPLString osTimeZone(
                                CPLString(oField.GetString(
                                    "timezone", OGR_SCHEMA_UNDEFINED_VALUE)));
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

                            if (!EQUAL(osAlias, OGR_SCHEMA_UNDEFINED_VALUE))
                            {
                                oFieldOverride.SetAlias(osAlias);
                            }

                            if (!EQUAL(osComment, OGR_SCHEMA_UNDEFINED_VALUE))
                            {
                                oFieldOverride.SetComment(osComment);
                            }

                            if (!EQUAL(osDomain, OGR_SCHEMA_UNDEFINED_VALUE))
                            {
                                oFieldOverride.SetDomainName(osDomain);
                            }

                            if (!EQUAL(osTimeZone, OGR_SCHEMA_UNDEFINED_VALUE))
                            {
                                oFieldOverride.SetTimezone(osTimeZone);
                            }

                            if (!EQUAL(osDefaultValue,
                                       OGR_SCHEMA_UNDEFINED_VALUE))
                            {
                                oFieldOverride.SetDefaultValue(osDefaultValue);
                            }

                            if (!EQUAL(osNullable, OGR_SCHEMA_UNDEFINED_VALUE))
                            {
                                if (osNullable == "true")
                                {
                                    oFieldOverride.SetNullable(true);
                                }
                                else if (osNullable == "false")
                                {
                                    oFieldOverride.SetNullable(false);
                                }
                                else
                                {
                                    CPLError(CE_Failure, CPLE_AppDefined,
                                             "Invalid value for nullable "
                                             "attribute for field %s: %s",
                                             osFieldName.c_str(),
                                             osNullable.c_str());
                                    return false;
                                }
                            }

                            if (!EQUAL(osUnique, OGR_SCHEMA_UNDEFINED_VALUE))
                            {
                                if (osUnique == "true")
                                {
                                    oFieldOverride.SetUnique(true);
                                }
                                else if (osUnique == "false")
                                {
                                    oFieldOverride.SetUnique(false);
                                }
                                else
                                {
                                    CPLError(
                                        CE_Failure, CPLE_AppDefined,
                                        "Invalid value for uniqueConstraint "
                                        "attribute for field %s: %s",
                                        osFieldName.c_str(), osUnique.c_str());
                                    return false;
                                }
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

                    const auto oGeometryLayerFields =
                        oLayer.GetArray("geometryFields");
                    if (oGeometryLayerFields.Size() > 0 &&
                        !bAllowGeometryFields)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Geometry fields are not allowed in "
                                 "OGR_SCHEMA overrides");
                        return false;
                    }
                    else if (oGeometryLayerFields.Size() > 0)
                    {
                        for (const auto &oGeometryField : oGeometryLayerFields)
                        {
                            OGRGeomFieldDefnOverride oGeomFieldOverride;
                            const auto osGeomFieldName =
                                oGeometryField.GetString("name");
                            oGeomFieldOverride.SetFieldName(osGeomFieldName);
                            const CPLString oGeometryType(
                                CPLString(oGeometryField.GetString("type"))
                                    .tolower());
                            if (!oGeometryType.empty())
                            {
                                const OGRwkbGeometryType eType =
                                    OGRFromOGCGeomType(oGeometryType.c_str());
                                if (eType == wkbUnknown)
                                {
                                    CPLError(CE_Failure, CPLE_AppDefined,
                                             "Unsupported geometry field type: "
                                             "%s for geometry field %s",
                                             oGeometryType.c_str(),
                                             osGeomFieldName.c_str());
                                    return false;
                                }
                                oGeomFieldOverride.SetGeometryType(eType);

                                // SRS
                                const auto osSRS =
                                    oGeometryField.GetObj("coordinateSystem");
                                if (!osSRS.GetString("wkt").empty() ||
                                    !osSRS.GetString("projjson").empty())
                                {
                                    OGRSpatialReference oSRS;
                                    oSRS.SetAxisMappingStrategy(
                                        OAMS_TRADITIONAL_GIS_ORDER);
                                    std::string srs;
                                    if (const auto wkt = osSRS.GetString("wkt");
                                        !wkt.empty())
                                    {
                                        srs = wkt;
                                    }
                                    else if (const auto projjson =
                                                 osSRS.GetString("projjson");
                                             !projjson.empty())
                                    {
                                        srs = projjson;
                                    }

                                    if (!srs.empty())
                                    {
                                        if (oSRS.SetFromUserInput(
                                                srs.c_str()) != OGRERR_NONE)
                                        {
                                            CPLError(CE_Failure,
                                                     CPLE_AppDefined,
                                                     "Failed to parse SRS "
                                                     "definition for geometry "
                                                     "field %s.",
                                                     osGeomFieldName.c_str());
                                            return false;
                                        }
                                        oGeomFieldOverride.SetSRS(oSRS);
                                    }
                                    else
                                    {
                                        // No SRS, assuming it's ok, just issue a warning
                                        CPLError(CE_Warning, CPLE_AppDefined,
                                                 "CRS definition is missing "
                                                 "for geometry field %s.",
                                                 osGeomFieldName.c_str());
                                    }
                                }

                                oLayerOverride.AddGeometryFieldOverride(
                                    oGeomFieldOverride);
                            }
                            else
                            {
                                CPLError(CE_Failure, CPLE_AppDefined,
                                         "Geometry field %s has no type",
                                         osGeomFieldName.c_str());
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

const std::vector<OGRLayerSchemaOverride> &
OGRSchemaOverride::GetLayerOverrides() const
{
    return m_aoLayerOverrides;
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

const OGRLayerSchemaOverride &
OGRSchemaOverride::GetLayerOverride(const std::string &osLayerName) const
{
    for (const auto &oLayerOverride : m_aoLayerOverrides)
    {
        if (oLayerOverride.GetLayerName() == osLayerName)
        {
            return oLayerOverride;
        }
    }
    static const OGRLayerSchemaOverride emptyOverride{};
    return emptyOverride;
}

void OGRLayerSchemaOverride::SetLayerName(const std::string &osLayerName)
{
    m_osLayerName = osLayerName;
}

void OGRLayerSchemaOverride::AddNamedFieldOverride(
    const std::string &osFieldName, const OGRFieldDefnOverride &oFieldOverride)
{
    m_oNamedFieldOverrides[osFieldName] = oFieldOverride;
}

void OGRLayerSchemaOverride::AddUnnamedFieldOverride(
    const OGRFieldDefnOverride &oFieldOverride)
{
    m_aoUnnamedFieldOverrides.push_back(oFieldOverride);
}

const std::string &OGRLayerSchemaOverride::GetLayerName() const
{
    return m_osLayerName;
}

const std::map<std::string, OGRFieldDefnOverride> &
OGRLayerSchemaOverride::GetNamedFieldOverrides() const
{
    return m_oNamedFieldOverrides;
}

const std::vector<OGRFieldDefnOverride> &
OGRLayerSchemaOverride::GetUnnamedFieldOverrides() const
{
    return m_aoUnnamedFieldOverrides;
}

void OGRLayerSchemaOverride::AddGeometryFieldOverride(
    const OGRGeomFieldDefnOverride &oGeomFieldOverride)
{
    m_aoGeomFieldOverrides.push_back(oGeomFieldOverride);
}

const std::vector<OGRGeomFieldDefnOverride> &
OGRLayerSchemaOverride::GetGeometryFieldOverrides() const
{
    return m_aoGeomFieldOverrides;
}

std::vector<OGRFieldDefn> OGRLayerSchemaOverride::GetFieldDefinitions() const
{
    std::vector<OGRFieldDefn> ret;
    for (const auto &kv : m_oNamedFieldOverrides)
    {
        ret.push_back(kv.second.ToFieldDefn(kv.first));
    }
    return ret;
}

std::vector<OGRGeomFieldDefn>
OGRLayerSchemaOverride::GetGeomFieldDefinitions() const
{
    std::vector<OGRGeomFieldDefn> ret;
    for (const auto &oGeomFieldOverride : m_aoGeomFieldOverrides)
    {
        ret.push_back(oGeomFieldOverride.ToGeometryFieldDefn("geom"));
    }
    return ret;
}

bool OGRLayerSchemaOverride::IsFullOverride() const
{
    return m_bIsFullOverride;
}

void OGRLayerSchemaOverride::SetFullOverride(bool bIsFullOverride)
{
    m_bIsFullOverride = bIsFullOverride;
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

bool OGRLayerSchemaOverride::empty() const
{
    return m_osLayerName.empty() && m_oNamedFieldOverrides.empty() &&
           m_aoUnnamedFieldOverrides.empty() && !m_bIsFullOverride;
}

bool OGRFieldDefnOverride::IsValid() const
{
    return m_osName.has_value() || m_eType.has_value() ||
           m_eSubType.has_value() || m_eSrcType.has_value() ||
           m_eSrcSubType.has_value() || m_nWidth.has_value() ||
           m_nPrecision.has_value();
}

OGRFieldDefn
OGRFieldDefnOverride::ToFieldDefn(const std::string &osDefaultName) const
{

    OGRFieldDefn oFieldDefn(m_osName.value_or(osDefaultName).c_str(),
                            m_eType.value_or(OFTString));

    oFieldDefn.SetName(m_osName.value_or(osDefaultName).c_str());

    if (m_eSubType.has_value())
        oFieldDefn.SetSubType(m_eSubType.value());
    if (m_nWidth.has_value())
        oFieldDefn.SetWidth(m_nWidth.value());
    if (m_nPrecision.has_value())
        oFieldDefn.SetPrecision(m_nPrecision.value());
    if (m_osName.has_value())
        oFieldDefn.SetName(m_osName.value().c_str());
    if (m_bNullable.has_value())
        oFieldDefn.SetNullable(m_bNullable.value());
    if (m_bUnique.has_value())
        oFieldDefn.SetUnique(m_bUnique.value());
    if (m_osComment.has_value())
        oFieldDefn.SetComment(m_osComment.value().c_str());
    if (m_osAlias.has_value())
        oFieldDefn.SetAlternativeName(m_osAlias.value().c_str());
    if (m_osTimezone.has_value())
    {
        const auto tzValue{m_osTimezone.value().c_str()};
        if (EQUAL(tzValue, "UTC"))
        {
            oFieldDefn.SetTZFlag(OGR_TZFLAG_UTC);
        }
        else if (EQUAL(tzValue, "localtime"))
        {
            oFieldDefn.SetTZFlag(OGR_TZFLAG_LOCALTIME);
        }
        else if (EQUAL(tzValue, "mixed timezones"))
        {
            oFieldDefn.SetTZFlag(OGR_TZFLAG_MIXED_TZ);
        }
        else
        {
            const auto tzFlag{OGRTimezoneToTZFlag(
                tzValue, /* bEmitErrorIfUnhandledFormat */ false)};
            oFieldDefn.SetTZFlag(tzFlag);
            if (tzFlag == OGR_TZFLAG_UNKNOWN)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Invalid timezone value: %s. Ignoring it.", tzValue);
            }
        }
    }
    if (m_osDomainName.has_value())
        oFieldDefn.SetDomainName(m_osDomainName.value().c_str());
    if (m_osDefaultValue.has_value())
        oFieldDefn.SetDefault(m_osDefaultValue.value().c_str());
    return oFieldDefn;
}

OGRGeomFieldDefn OGRGeomFieldDefnOverride::ToGeometryFieldDefn(
    const std::string &osDefaultName) const
{

    OGRGeomFieldDefn oGeomFieldDefn{m_osName.value_or(osDefaultName).c_str(),
                                    m_eType.value_or(wkbUnknown)};

    if (m_bNullable.has_value())
    {
        oGeomFieldDefn.SetNullable(m_bNullable.value());
    }

    if (m_oSRS.has_value())
    {
        std::unique_ptr<OGRSpatialReference> poSRS =
            std::make_unique<OGRSpatialReference>(m_oSRS.value());
        oGeomFieldDefn.SetSpatialRef(poSRS.release());
    }

    return oGeomFieldDefn;
}

//! @endcond
