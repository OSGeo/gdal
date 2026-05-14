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

#ifndef OGR_SCHEMA_OVERRIDE_H_INCLUDED
#define OGR_SCHEMA_OVERRIDE_H_INCLUDED

//! @cond Doxygen_Suppress

#include <functional>
#include <string>
#include <map>
#include <optional>
#include <ogr_api.h>
#include <ogr_feature.h>
#include <cpl_vsi.h>
#include <cpl_json.h>

/** Class that holds the schema override options for a single field */
class CPL_DLL OGRFieldDefnOverride
{
  public:
    OGRFieldDefnOverride() = default;

    void SetFieldName(const std::string &osName)
    {
        m_osName = osName;
    }

    void SetSrcFieldType(OGRFieldType eType)
    {
        m_eSrcType = eType;
    }

    void SetSrcFieldSubType(OGRFieldSubType eSubType)
    {
        m_eSrcSubType = eSubType;
    }

    void SetFieldType(OGRFieldType eType)
    {
        m_eType = eType;
    }

    void SetFieldSubType(OGRFieldSubType eSubType)
    {
        m_eSubType = eSubType;
    }

    void SetFieldWidth(int nWidth)
    {
        m_nWidth = nWidth;
    }

    void SetFieldPrecision(int nPrecision)
    {
        m_nPrecision = nPrecision;
    }

    void SetNullable(bool bNullable)
    {
        m_bNullable = bNullable;
    }

    void SetUnique(bool bUnique)
    {
        m_bUnique = bUnique;
    }

    void SetComment(const std::string &osComment)
    {
        m_osComment = osComment;
    }

    void SetAlias(const std::string &osAlias)
    {
        m_osAlias = osAlias;
    }

    void SetTimezone(const std::string &osTimezone)
    {
        m_osTimezone = osTimezone;
    }

    void SetDomainName(const std::string &osDomainName)
    {
        m_osDomainName = osDomainName;
    }

    void SetDefaultValue(const std::string &osDefaultValue)
    {
        m_osDefaultValue = osDefaultValue;
    }

    std::optional<std::string> GetFieldName() const
    {
        return m_osName;
    }

    std::optional<OGRFieldType> GetSrcFieldType() const
    {
        return m_eSrcType;
    }

    std::optional<OGRFieldSubType> GetSrcFieldSubType() const
    {
        return m_eSrcSubType;
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

    std::optional<int> GetNullable() const
    {
        return m_bNullable;
    }

    std::optional<int> GetUnique() const
    {
        return m_bUnique;
    }

    std::optional<std::string> GetComment() const
    {
        return m_osComment;
    }

    std::optional<std::string> GetAlias() const
    {
        return m_osAlias;
    }

    std::optional<std::string> GetTimezone() const
    {
        return m_osTimezone;
    }

    std::optional<std::string> GetDomainName() const
    {
        return m_osDomainName;
    }

    // Considered valid if it carries any change information, otherwise it's considered a no-op
    bool IsValid() const;

    /**
     *  Build an OGRFieldDefn based on the override information.
     *  \a osDefaultName is used as field name if the override doesn't specify one.
     */
    OGRFieldDefn ToFieldDefn(const std::string &osDefaultName) const;

  private:
    std::optional<std::string> m_osName{};
    std::optional<OGRFieldType> m_eSrcType{};
    std::optional<OGRFieldSubType> m_eSrcSubType{};
    std::optional<OGRFieldType> m_eType{};
    std::optional<OGRFieldSubType> m_eSubType{};
    std::optional<int> m_nWidth{};
    std::optional<int> m_nPrecision{};
    std::optional<bool> m_bUnique{};
    std::optional<bool> m_bNullable{};
    std::optional<std::string> m_osComment{};
    std::optional<std::string> m_osAlias{};
    std::optional<std::string> m_osTimezone{};
    std::optional<std::string> m_osDomainName{};
    std::optional<std::string> m_osDefaultValue{};
};

/**
 *  Class that holds the schema override options for a single geometry field
 */
class CPL_DLL OGRGeomFieldDefnOverride
{
  public:
    OGRGeomFieldDefnOverride() = default;

    void SetFieldName(const std::string &osName)
    {
        m_osName = osName;
    }

    void SetGeometryType(OGRwkbGeometryType eType)
    {
        m_eType = eType;
    }

    void SetSRS(const OGRSpatialReference &oSRS)
    {
        m_oSRS = oSRS;
    }

    void SetNullable(bool bNullable)
    {
        m_bNullable = bNullable;
    }

    std::optional<std::string> GetFieldName() const
    {
        return m_osName;
    }

    std::optional<OGRwkbGeometryType> GetGeometryType() const
    {
        return m_eType;
    }

    std::optional<OGRSpatialReference> GetSRS() const
    {
        return m_oSRS;
    }

    std::optional<bool> GetNullable() const
    {
        return m_bNullable;
    }

    /**
     *  Build an OGRGeometryFieldDefn based on the override information.
     *  \a osDefaultName is used as field name if the override doesn't specify one.
     *  If the override doesn't specify a type, wkbUnknown is used as default type.
     */
    OGRGeomFieldDefn
    ToGeometryFieldDefn(const std::string &osDefaultName) const;

  private:
    std::optional<bool> m_bNullable{};
    std::optional<std::string> m_osName{};
    std::optional<OGRwkbGeometryType> m_eType{};
    std::optional<OGRSpatialReference> m_oSRS{};
};

/** Class that holds the schema override options for a single layer */
class CPL_DLL OGRLayerSchemaOverride
{
  public:
    OGRLayerSchemaOverride() = default;

    void SetLayerName(const std::string &osLayerName);

    void SetFIDColumnName(const std::string &osFIDColumnName);

    void AddNamedFieldOverride(const std::string &osFieldName,
                               const OGRFieldDefnOverride &oFieldOverride);

    void AddUnnamedFieldOverride(const OGRFieldDefnOverride &oFieldOverride);

    const std::string &GetLayerName() const;

    const std::string &GetFIDColumnName() const;

    const std::map<std::string, OGRFieldDefnOverride> &
    GetNamedFieldOverrides() const;

    const std::vector<OGRFieldDefnOverride> &GetUnnamedFieldOverrides() const;

    void AddGeometryFieldOverride(
        const OGRGeomFieldDefnOverride &oGeomFieldOverride);

    const std::vector<OGRGeomFieldDefnOverride> &
    GetGeometryFieldOverrides() const;

    std::vector<OGRFieldDefn> GetFieldDefinitions() const;

    std::vector<OGRGeomFieldDefn> GetGeomFieldDefinitions() const;

    bool IsFullOverride() const;

    void SetFullOverride(bool bIsFullOverride);

    bool IsValid() const;

    bool empty() const;

  private:
    std::string m_osLayerName{};
    std::string m_osFIDColumnName{};
    std::map<std::string, OGRFieldDefnOverride> m_oNamedFieldOverrides{};
    std::vector<OGRFieldDefnOverride> m_aoUnnamedFieldOverrides{};
    std::vector<OGRGeomFieldDefnOverride> m_aoGeomFieldOverrides{};
    bool m_bIsFullOverride = false;
};

class GDALDataset;

/** Class that holds the schema override options for a datasource */
class CPL_DLL OGRSchemaOverride
{
  public:
    OGRSchemaOverride() = default;

    void AddLayerOverride(const OGRLayerSchemaOverride &oLayerOverride);

    /**
     * Load an override schema from JSON string that follows OGR_SCHEMA specification.
     * @param osJSON JSON string
     * @param bAllowGeometryFields Whether to allow a geometry fields in the JSON (normally not overridable but allowed if the schema is applied to a dataset that doesn't have any geometry field, so that it can be used to create a geometry field in that case)
     * @return TRUE if the JSON was successfully parsed and the schema override is valid, FALSE otherwise
     */
    bool LoadFromJSON(const std::string &osJSON,
                      bool bAllowGeometryFields = false);

    const std::vector<OGRLayerSchemaOverride> &GetLayerOverrides() const;

    bool IsValid() const;

    /**
     *  Default implementation to apply the overrides to a dataset
     *  \note geometry fields are ignored (not overridable)
     */
    bool DefaultApply(
        GDALDataset *poDS, const char *pszDebugKey,
        std::function<void(OGRLayer *, int)> callbackWhenRemovingField =
            [](OGRLayer *, int) {}) const;

    const OGRLayerSchemaOverride &
    GetLayerOverride(const std::string &osLayerName) const;

  private:
    std::vector<OGRLayerSchemaOverride> m_aoLayerOverrides{};
};

//! @endcond

#endif /* ndef OGR_FEATURE_H_INCLUDED */
