/******************************************************************************
 *
 * Project:  S-101 driver
 * Purpose:  Header file for OGRS101FeatureCatalog
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_S101_FEATURE_CATALOG_H_INCLUDED
#define OGR_S101_FEATURE_CATALOG_H_INCLUDED

struct CPLXMLNode;

#include <map>
#include <set>
#include <string>
#include <utility>

namespace OGRS101FeatureCatalogTypes
{
struct SimpleAttribute
{
    std::string code{};  // short string
    std::string name{};  // longer string
    std::string type{};
    std::map<int, std::string> enumeratedValues{};
};

struct ComplexAttribute
{
    std::string code{};  // short string
    std::string name{};  // longer string
    std::set<std::string> attributeBindings{};
};

struct InformationType
{
    std::string code{};  // short string
    std::string name{};  // longer string
    std::string definition{};
    std::string alias{};
    std::set<std::string> attributeBindings{};
};

struct FeatureType
{
    std::string code{};  // short string
    std::string name{};  // longer string
    std::string definition{};
    std::string alias{};
    std::set<std::string> attributeBindings{};
    std::set<std::string> permittedPrimitives{};
};
}  // namespace OGRS101FeatureCatalogTypes

/************************************************************************/
/*                        OGRS101FeatureCatalog                         */
/************************************************************************/

class OGRS101FeatureCatalog
{
  public:
    using SimpleAttribute = OGRS101FeatureCatalogTypes::SimpleAttribute;
    using ComplexAttribute = OGRS101FeatureCatalogTypes::ComplexAttribute;
    using InformationType = OGRS101FeatureCatalogTypes::InformationType;
    using FeatureType = OGRS101FeatureCatalogTypes::FeatureType;

    // Valid values for <permittedPrimitives> element of <S100_FC_FeatureType>
    static constexpr const char *PERMITTED_PRIMITIVE_NO_GEOMETRY = "noGeometry";
    static constexpr const char *PERMITTED_PRIMITIVE_POINT = "point";
    static constexpr const char *PERMITTED_PRIMITIVE_POINTSET = "pointSet";
    static constexpr const char *PERMITTED_PRIMITIVE_CURVE = "curve";
    static constexpr const char *PERMITTED_PRIMITIVE_SURFACE = "surface";

    // Valid values for <valueType> element of <S100_FC_SimpleAttribute>
    static constexpr const char *VALUE_TYPE_BOOLEAN = "boolean";
    static constexpr const char *VALUE_TYPE_ENUMERATION = "enumeration";
    static constexpr const char *VALUE_TYPE_INTEGER = "integer";
    static constexpr const char *VALUE_TYPE_REAL = "real";
    static constexpr const char *VALUE_TYPE_TRUNCATED_DATE =
        "S100_TruncatedDate";
    static constexpr const char *VALUE_TYPE_TEXT = "text";
    static constexpr const char *VALUE_TYPE_TIME = "time";
    static constexpr const char *VALUE_TYPE_URI = "URI";
    static constexpr const char *VALUE_TYPE_URN = "URN";

    explicit OGRS101FeatureCatalog(bool bStrict);

    static void CleanupSingletonFeatureCatalog();

    enum class LoadingStatus
    {
        UNINIT,
        OK,
        SKIPPED,
        ERROR
    };

    static std::pair<LoadingStatus, const OGRS101FeatureCatalog *>
    GetSingletonFeatureCatalog(bool bStrict);

    std::string GetFilename(bool &bError) const;

    LoadingStatus Load();

    /* Return a map from the code of a simple attribute to its definition */
    const std::map<std::string, SimpleAttribute> &GetSimpleAttributes() const
    {
        return m_simpleAttributes;
    }

    /* Return a map from the code of a complex attribute to its definition */
    const std::map<std::string, ComplexAttribute> &GetComplexAttributes() const
    {
        return m_complexAttributes;
    }

    /* Return a map from the code of an information type to its definition */
    const std::map<std::string, InformationType> &GetInformationTypes() const
    {
        return m_informationTypes;
    }

    /* Return a map from the code of a feature type to its definition */
    const std::map<std::string, FeatureType> &GetFeatureTypes() const
    {
        return m_featureTypes;
    }

  private:
    const bool m_bStrict;

    std::map<std::string, SimpleAttribute> m_simpleAttributes{};
    std::map<std::string, ComplexAttribute> m_complexAttributes{};
    std::map<std::string, InformationType> m_informationTypes{};
    std::map<std::string, FeatureType> m_featureTypes{};

    static bool EmitErrorOrWarning(const char *pszFile, const char *pszFunc,
                                   int nLine, const char *pszMsg, bool bError,
                                   bool bRecoverable);

    bool LoadSimpleAttributes(const char *pszFC, const CPLXMLNode *psRoot);
    bool LoadComplexAttributes(const char *pszFC, const CPLXMLNode *psRoot);
    bool LoadInformationTypes(const char *pszFC, const CPLXMLNode *psRoot);
    bool LoadFeatureTypes(const char *pszFC, const CPLXMLNode *psRoot);
};

#endif  // OGR_S101_FEATURE_CATALOG_H_INCLUDED
