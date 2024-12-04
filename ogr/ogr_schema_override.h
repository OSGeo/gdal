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
    std::optional<std::string> m_osName;
    std::optional<OGRFieldType> m_eType;
    std::optional<OGRFieldSubType> m_eSubType;
    std::optional<int> m_nWidth;
    std::optional<int> m_nPrecision;
};

/** Class that holds the schema override options for a single layer */
class CPL_DLL OGRLayerSchemaOverride
{
  public:
    OGRLayerSchemaOverride() : m_osLayerName(), m_moFieldOverrides()
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

    bool IsValid() const;

  private:
    std::string m_osLayerName;
    std::map<std::string, OGRFieldDefnOverride> m_moFieldOverrides;
    bool m_bIsFullOverride = false;
};

/** Class that holds the schema override options for a datasource */
class CPL_DLL OGRSchemaOverride
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

    bool LoadFromJSON(const std::string &osJSON);

    const std::map<std::string, OGRLayerSchemaOverride> &
    GetLayerOverrides() const
    {
        return m_moLayerOverrides;
    }

    bool IsValid() const;

  private:
    std::map<std::string, OGRLayerSchemaOverride> m_moLayerOverrides;
};

//! @endcond

#endif /* ndef OGR_FEATURE_H_INCLUDED */
