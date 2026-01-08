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

    // Considered valid if it carries any change information, otherwise it's considered a no-op
    bool IsValid() const;

  private:
    std::optional<std::string> m_osName{};
    std::optional<OGRFieldType> m_eSrcType{};
    std::optional<OGRFieldSubType> m_eSrcSubType{};
    std::optional<OGRFieldType> m_eType{};
    std::optional<OGRFieldSubType> m_eSubType{};
    std::optional<int> m_nWidth{};
    std::optional<int> m_nPrecision{};
};

/** Class that holds the schema override options for a single layer */
class CPL_DLL OGRLayerSchemaOverride
{
  public:
    OGRLayerSchemaOverride() = default;

    void SetLayerName(const std::string &osLayerName)
    {
        m_osLayerName = osLayerName;
    }

    void AddNamedFieldOverride(const std::string &osFieldName,
                               const OGRFieldDefnOverride &oFieldOverride)
    {
        m_oNamedFieldOverrides[osFieldName] = oFieldOverride;
    }

    void AddUnnamedFieldOverride(const OGRFieldDefnOverride &oFieldOverride)
    {
        m_aoUnnamedFieldOverrides.push_back(oFieldOverride);
    }

    const std::string &GetLayerName() const
    {
        return m_osLayerName;
    }

    const std::map<std::string, OGRFieldDefnOverride> &
    GetNamedFieldOverrides() const
    {
        return m_oNamedFieldOverrides;
    }

    const std::vector<OGRFieldDefnOverride> &GetUnnamedFieldOverrides() const
    {
        return m_aoUnnamedFieldOverrides;
    }

    bool IsFullOverride() const
    {
        return m_bIsFullOverride;
    }

    void SetFullOverride(bool bIsFullOverride)
    {
        m_bIsFullOverride = bIsFullOverride;
    }

    bool IsValid() const;

  private:
    std::string m_osLayerName{};
    std::map<std::string, OGRFieldDefnOverride> m_oNamedFieldOverrides{};
    std::vector<OGRFieldDefnOverride> m_aoUnnamedFieldOverrides{};
    bool m_bIsFullOverride = false;
};

class GDALDataset;

/** Class that holds the schema override options for a datasource */
class CPL_DLL OGRSchemaOverride
{
  public:
    OGRSchemaOverride() = default;

    void AddLayerOverride(const OGRLayerSchemaOverride &oLayerOverride)
    {
        m_aoLayerOverrides.push_back(oLayerOverride);
    }

    bool LoadFromJSON(const std::string &osJSON);

    const std::vector<OGRLayerSchemaOverride> &GetLayerOverrides() const
    {
        return m_aoLayerOverrides;
    }

    bool IsValid() const;

    // Default implementation to apply the overrides to a dataset
    bool DefaultApply(
        GDALDataset *poDS, const char *pszDebugKey,
        std::function<void(OGRLayer *, int)> callbackWhenRemovingField =
            [](OGRLayer *, int) {}) const;

  private:
    std::vector<OGRLayerSchemaOverride> m_aoLayerOverrides{};
};

//! @endcond

#endif /* ndef OGR_FEATURE_H_INCLUDED */
