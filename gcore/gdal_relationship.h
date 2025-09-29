/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Declaration of GDALRelationship class
 * Author:   Nyall Dawson, <nyall dot dawson at gmail dot com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Nyall Dawson <nyall dot dawson at gmail dot comg>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALRELATIONSHIP_H_INCLUDED
#define GDALRELATIONSHIP_H_INCLUDED

#include "cpl_port.h"
#include "gdal.h"

#include <string>
#include <vector>

/************************************************************************/
/*                           Relationships                              */
/************************************************************************/

/**
 * Definition of a table relationship.
 *
 * GDALRelationship describes the relationship between two tables, including
 * properties such as the cardinality of the relationship and the participating
 * tables.
 *
 * Not all relationship properties are supported by all data formats.
 *
 * @since GDAL 3.6
 */
class CPL_DLL GDALRelationship
{
  protected:
    /*! @cond Doxygen_Suppress */
    std::string m_osName{};
    std::string m_osLeftTableName{};
    std::string m_osRightTableName{};
    GDALRelationshipCardinality m_eCardinality =
        GDALRelationshipCardinality::GRC_ONE_TO_MANY;
    std::string m_osMappingTableName{};
    std::vector<std::string> m_osListLeftTableFields{};
    std::vector<std::string> m_osListRightTableFields{};
    std::vector<std::string> m_osListLeftMappingTableFields{};
    std::vector<std::string> m_osListRightMappingTableFields{};
    GDALRelationshipType m_eType = GDALRelationshipType::GRT_ASSOCIATION;
    std::string m_osForwardPathLabel{};
    std::string m_osBackwardPathLabel{};
    std::string m_osRelatedTableType{};

    /*! @endcond */

  public:
    /**
     * Constructor for a relationship between two tables.
     * @param osName relationship name
     * @param osLeftTableName left table name
     * @param osRightTableName right table name
     * @param eCardinality cardinality of relationship
     */
    GDALRelationship(const std::string &osName,
                     const std::string &osLeftTableName,
                     const std::string &osRightTableName,
                     GDALRelationshipCardinality eCardinality =
                         GDALRelationshipCardinality::GRC_ONE_TO_MANY)
        : m_osName(osName), m_osLeftTableName(osLeftTableName),
          m_osRightTableName(osRightTableName), m_eCardinality(eCardinality)
    {
    }

    /** Get the name of the relationship */
    const std::string &GetName() const
    {
        return m_osName;
    }

    /** Get the cardinality of the relationship */
    GDALRelationshipCardinality GetCardinality() const
    {
        return m_eCardinality;
    }

    /** Get the name of the left (or base/origin) table in the relationship.
     *
     * @see GetRightTableName()
     */
    const std::string &GetLeftTableName() const
    {
        return m_osLeftTableName;
    }

    /** Get the name of the right (or related/destination) table in the
     * relationship */
    const std::string &GetRightTableName() const
    {
        return m_osRightTableName;
    }

    /** Get the name of the mapping table for many-to-many relationships.
     *
     * @see SetMappingTableName()
     */
    const std::string &GetMappingTableName() const
    {
        return m_osMappingTableName;
    }

    /** Sets the name of the mapping table for many-to-many relationships.
     *
     * @see GetMappingTableName()
     */
    void SetMappingTableName(const std::string &osName)
    {
        m_osMappingTableName = osName;
    }

    /** Get the names of the participating fields from the left table in the
     * relationship.
     *
     * @see GetRightTableFields()
     * @see SetLeftTableFields()
     */
    const std::vector<std::string> &GetLeftTableFields() const
    {
        return m_osListLeftTableFields;
    }

    /** Get the names of the participating fields from the right table in the
     * relationship.
     *
     * @see GetLeftTableFields()
     * @see SetRightTableFields()
     */
    const std::vector<std::string> &GetRightTableFields() const
    {
        return m_osListRightTableFields;
    }

    /** Sets the names of the participating fields from the left table in the
     * relationship.
     *
     * @see GetLeftTableFields()
     * @see SetRightTableFields()
     */
    void SetLeftTableFields(const std::vector<std::string> &osListFields)
    {
        m_osListLeftTableFields = osListFields;
    }

    /** Sets the names of the participating fields from the right table in the
     * relationship.
     *
     * @see GetRightTableFields()
     * @see SetLeftTableFields()
     */
    void SetRightTableFields(const std::vector<std::string> &osListFields)
    {
        m_osListRightTableFields = osListFields;
    }

    /** Get the names of the mapping table fields which correspond to the
     * participating fields from the left table in the relationship.
     *
     * @see GetRightMappingTableFields()
     * @see SetLeftMappingTableFields()
     */
    const std::vector<std::string> &GetLeftMappingTableFields() const
    {
        return m_osListLeftMappingTableFields;
    }

    /** Get the names of the mapping table fields which correspond to the
     * participating fields from the right table in the relationship.
     *
     * @see GetLeftMappingTableFields()
     * @see SetRightMappingTableFields()
     */
    const std::vector<std::string> &GetRightMappingTableFields() const
    {
        return m_osListRightMappingTableFields;
    }

    /** Sets the names of the mapping table fields which correspond to the
     * participating fields from the left table in the relationship.
     *
     * @see GetLeftMappingTableFields()
     * @see SetRightMappingTableFields()
     */
    void SetLeftMappingTableFields(const std::vector<std::string> &osListFields)
    {
        m_osListLeftMappingTableFields = osListFields;
    }

    /** Sets the names of the mapping table fields which correspond to the
     * participating fields from the right table in the relationship.
     *
     * @see GetRightMappingTableFields()
     * @see SetLeftMappingTableFields()
     */
    void
    SetRightMappingTableFields(const std::vector<std::string> &osListFields)
    {
        m_osListRightMappingTableFields = osListFields;
    }

    /** Get the type of the relationship.
     *
     * @see SetType()
     */
    GDALRelationshipType GetType() const
    {
        return m_eType;
    }

    /** Sets the type of the relationship.
     *
     * @see GetType()
     */
    void SetType(GDALRelationshipType eType)
    {
        m_eType = eType;
    }

    /** Get the label of the forward path for the relationship.
     *
     * The forward and backward path labels are free-form, user-friendly strings
     * which can be used to generate descriptions of the relationship between
     * features from the right and left tables.
     *
     * E.g. when the left table contains buildings and the right table contains
     * furniture, the forward path label could be "contains" and the backward
     * path label could be "is located within". A client could then generate a
     * user friendly description string such as "fire hose 1234 is located
     * within building 15a".
     *
     * @see SetForwardPathLabel()
     * @see GetBackwardPathLabel()
     */
    const std::string &GetForwardPathLabel() const
    {
        return m_osForwardPathLabel;
    }

    /** Sets the label of the forward path for the relationship.
     *
     * The forward and backward path labels are free-form, user-friendly strings
     * which can be used to generate descriptions of the relationship between
     * features from the right and left tables.
     *
     * E.g. when the left table contains buildings and the right table contains
     * furniture, the forward path label could be "contains" and the backward
     * path label could be "is located within". A client could then generate a
     * user friendly description string such as "fire hose 1234 is located
     * within building 15a".
     *
     * @see GetForwardPathLabel()
     * @see SetBackwardPathLabel()
     */
    void SetForwardPathLabel(const std::string &osLabel)
    {
        m_osForwardPathLabel = osLabel;
    }

    /** Get the label of the backward path for the relationship.
     *
     * The forward and backward path labels are free-form, user-friendly strings
     * which can be used to generate descriptions of the relationship between
     * features from the right and left tables.
     *
     * E.g. when the left table contains buildings and the right table contains
     * furniture, the forward path label could be "contains" and the backward
     * path label could be "is located within". A client could then generate a
     * user friendly description string such as "fire hose 1234 is located
     * within building 15a".
     *
     * @see SetBackwardPathLabel()
     * @see GetForwardPathLabel()
     */
    const std::string &GetBackwardPathLabel() const
    {
        return m_osBackwardPathLabel;
    }

    /** Sets the label of the backward path for the relationship.
     *
     * The forward and backward path labels are free-form, user-friendly strings
     * which can be used to generate descriptions of the relationship between
     * features from the right and left tables.
     *
     * E.g. when the left table contains buildings and the right table contains
     * furniture, the forward path label could be "contains" and the backward
     * path label could be "is located within". A client could then generate a
     * user friendly description string such as "fire hose 1234 is located
     * within building 15a".
     *
     * @see GetBackwardPathLabel()
     * @see SetForwardPathLabel()
     */
    void SetBackwardPathLabel(const std::string &osLabel)
    {
        m_osBackwardPathLabel = osLabel;
    }

    /** Get the type string of the related table.
     *
     * This a free-form string representing the type of related features, where
     * the exact interpretation is format dependent. For instance, table types
     * from GeoPackage relationships will directly reflect the categories from
     * the GeoPackage related tables extension (i.e. "media", "simple
     * attributes", "features", "attributes" and "tiles").
     *
     * @see SetRelatedTableType()
     */
    const std::string &GetRelatedTableType() const
    {
        return m_osRelatedTableType;
    }

    /** Sets the type string of the related table.
     *
     * This a free-form string representing the type of related features, where
     * the exact interpretation is format dependent. For instance, table types
     * from GeoPackage relationships will directly reflect the categories from
     * the GeoPackage related tables extension (i.e. "media", "simple
     * attributes", "features", "attributes" and "tiles").
     *
     * @see GetRelatedTableType()
     */
    void SetRelatedTableType(const std::string &osType)
    {
        m_osRelatedTableType = osType;
    }

    /** Convert a GDALRelationship* to a GDALRelationshipH.
     */
    static inline GDALRelationshipH ToHandle(GDALRelationship *poRelationship)
    {
        return static_cast<GDALRelationshipH>(poRelationship);
    }

    /** Convert a GDALRelationshipH to a GDALRelationship*.
     */
    static inline GDALRelationship *FromHandle(GDALRelationshipH hRelationship)
    {
        return static_cast<GDALRelationship *>(hRelationship);
    }
};

#endif
