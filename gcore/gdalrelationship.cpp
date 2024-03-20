/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Represent table relationships
 * Author:   Nyall Dawson, <nyall dot dawson at gmail dot com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Nyall Dawson <nyall dot dawson at gmail dot comg>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_port.h"
#include "gdal.h"
#include "gdal_priv.h"

/**
 * \class GDALRelationship
 *
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

/************************************************************************/
/*                  GDALRelationshipCreate()                            */
/************************************************************************/

/**
 * \brief Creates a new relationship.
 *
 * This function is the same as the C++ method
 * GDALRelationship::GDALRelationship()
 *
 * @param pszName relationship name
 * @param pszLeftTableName left table name
 * @param pszRightTableName right table name
 * @param eCardinality cardinality of relationship
 *
 * @return a new handle that should be freed with GDALDestroyRelationship(),
 *         or NULL in case of error.
 */
GDALRelationshipH
GDALRelationshipCreate(const char *pszName, const char *pszLeftTableName,
                       const char *pszRightTableName,
                       GDALRelationshipCardinality eCardinality)
{
    VALIDATE_POINTER1(pszName, __func__, nullptr);
    VALIDATE_POINTER1(pszLeftTableName, __func__, nullptr);
    VALIDATE_POINTER1(pszRightTableName, __func__, nullptr);

    return GDALRelationship::ToHandle(new GDALRelationship(
        pszName, pszLeftTableName, pszRightTableName, eCardinality));
}

/************************************************************************/
/*                  GDALDestroyRelationship()                           */
/************************************************************************/

/**
 * \brief Destroys a relationship.
 *
 * This function is the same as the C++ method
 * GDALRelationship::~GDALRelationship()
 */
void CPL_STDCALL GDALDestroyRelationship(GDALRelationshipH hRelationship)
{
    if (hRelationship != nullptr)
        delete GDALRelationship::FromHandle(hRelationship);
}

/************************************************************************/
/*                  GDALRelationshipGetName()                           */
/************************************************************************/

/**
 * \brief Get the name of the relationship.
 *
 * This function is the same as the C++ method
 * GDALRelationship::GetName().
 *
 * @return name.
 */
const char *GDALRelationshipGetName(GDALRelationshipH hRelationship)
{
    VALIDATE_POINTER1(hRelationship, "GDALRelationshipGetName", nullptr);

    return GDALRelationship::FromHandle(hRelationship)->GetName().c_str();
}

/************************************************************************/
/*                  GDALRelationshipGetCardinality()                    */
/************************************************************************/

/**
 * \brief Get the cardinality of the relationship.
 *
 * This function is the same as the C++ method
 * GDALRelationship::GetCardinality().
 *
 * @return cardinality.
 */
GDALRelationshipCardinality
GDALRelationshipGetCardinality(GDALRelationshipH hRelationship)
{
    return GDALRelationship::FromHandle(hRelationship)->GetCardinality();
}

/************************************************************************/
/*                  GDALRelationshipGetLeftTableName()                  */
/************************************************************************/

/**
 * \brief Get the name of the left (or base/origin) table in the relationship.
 *
 * This function is the same as the C++ method
 * GDALRelationship::GetLeftTableName().
 *
 * @return left table name.
 */
const char *GDALRelationshipGetLeftTableName(GDALRelationshipH hRelationship)
{
    VALIDATE_POINTER1(hRelationship, "GDALRelationshipGetLeftTableName",
                      nullptr);

    return GDALRelationship::FromHandle(hRelationship)
        ->GetLeftTableName()
        .c_str();
}

/************************************************************************/
/*                  GDALRelationshipGetRightTableName()                 */
/************************************************************************/

/**
 * \brief Get the name of the right (or related/destination) table in the
 * relationship.
 *
 * This function is the same as the C++ method
 * GDALRelationship::GetRightTableName().
 *
 * @return right table name.
 */
const char *GDALRelationshipGetRightTableName(GDALRelationshipH hRelationship)
{
    VALIDATE_POINTER1(hRelationship, "GDALRelationshipGetRightTableName",
                      nullptr);

    return GDALRelationship::FromHandle(hRelationship)
        ->GetRightTableName()
        .c_str();
}

/************************************************************************/
/*                  GDALRelationshipGetMappingTableName()               */
/************************************************************************/

/**
 * \brief Get the name of the mapping table for many-to-many relationships.
 *
 * This function is the same as the C++ method
 * GDALRelationship::GetMappingTableName().
 *
 * @return mapping table name.
 *
 * @see GDALRelationshipSetMappingTableName
 */
const char *GDALRelationshipGetMappingTableName(GDALRelationshipH hRelationship)
{
    VALIDATE_POINTER1(hRelationship, "GDALRelationshipGetMappingTableName",
                      nullptr);

    return GDALRelationship::FromHandle(hRelationship)
        ->GetMappingTableName()
        .c_str();
}

/************************************************************************/
/*                  GDALRelationshipSetMappingTableName()               */
/************************************************************************/
/**
 * \brief Sets the name of the mapping table for many-to-many relationships.
 *
 * This function is the same as the CPP method
 * GDALRelationship::SetMappingTableName().
 *
 * @param hRelationship handle to the relationship to apply the new mapping name
 * to.
 * @param pszName the mapping table name to set.
 *
 * @see GDALRelationshipGetMappingTableName
 */
void GDALRelationshipSetMappingTableName(GDALRelationshipH hRelationship,
                                         const char *pszName)

{
    GDALRelationship::FromHandle(hRelationship)->SetMappingTableName(pszName);
}

/************************************************************************/
/*                  GDALRelationshipGetLeftTableFields()                */
/************************************************************************/

/**
 * \brief Get the names of the participating fields from the left table in the
 * relationship.
 *
 * This function is the same as the C++ method
 * GDALRelationship::GetLeftTableFields().
 *
 * @return the field names, to be freed with CSLDestroy()
 *
 * @see GDALRelationshipGetRightTableFields
 * @see GDALRelationshipSetLeftTableFields
 */
char **GDALRelationshipGetLeftTableFields(GDALRelationshipH hRelationship)
{
    VALIDATE_POINTER1(hRelationship, "GDALRelationshipGetLeftTableFields",
                      nullptr);

    const auto &fields =
        GDALRelationship::FromHandle(hRelationship)->GetLeftTableFields();
    return CPLStringList(fields).StealList();
}

/************************************************************************/
/*                 GDALRelationshipGetRightTableFields()                */
/************************************************************************/

/**
 * \brief Get the names of the participating fields from the right table in the
 * relationship.
 *
 * This function is the same as the C++ method
 * GDALRelationship::GetRightTableFields().
 *
 * @return the field names, to be freed with CSLDestroy()
 *
 * @see GDALRelationshipGetLeftTableFields
 * @see GDALRelationshipSetRightTableFields
 */
char **GDALRelationshipGetRightTableFields(GDALRelationshipH hRelationship)
{
    VALIDATE_POINTER1(hRelationship, "GDALRelationshipGetRightTableFields",
                      nullptr);

    const auto &fields =
        GDALRelationship::FromHandle(hRelationship)->GetRightTableFields();
    return CPLStringList(fields).StealList();
}

/************************************************************************/
/*                  GDALRelationshipSetLeftTableFields()                */
/************************************************************************/

/**
 * \brief Sets the names of the participating fields from the left table in the
 * relationship.
 *
 * This function is the same as the C++ method
 * GDALRelationship::GetLeftTableFields().
 *
 * @param hRelationship handle to the relationship to apply the left table
 * fields to.
 * @param papszFields the names of the fields.
 *
 * @see GDALRelationshipGetLeftTableFields
 * @see GDALRelationshipSetRightTableFields
 */
void GDALRelationshipSetLeftTableFields(GDALRelationshipH hRelationship,
                                        CSLConstList papszFields)
{
    GDALRelationship::FromHandle(hRelationship)
        ->SetLeftTableFields(cpl::ToVector(papszFields));
}

/************************************************************************/
/*                 GDALRelationshipSetRightTableFields()                */
/************************************************************************/

/**
 * \brief Sets the names of the participating fields from the right table in the
 * relationship.
 *
 * This function is the same as the C++ method
 * GDALRelationship::SetRightTableFields().
 *
 * @param hRelationship handle to the relationship to apply the right table
 * fields to.
 * @param papszFields the names of the fields.
 *
 * @see GDALRelationshipGetRightTableFields
 * @see GDALRelationshipSetLeftTableFields
 */
void GDALRelationshipSetRightTableFields(GDALRelationshipH hRelationship,
                                         CSLConstList papszFields)
{
    GDALRelationship::FromHandle(hRelationship)
        ->SetRightTableFields(cpl::ToVector(papszFields));
}

/************************************************************************/
/*           GDALRelationshipGetLeftMappingTableFields()                */
/************************************************************************/

/**
 * \brief Get the names of the mapping table fields which correspond to the
 * participating fields from the left table in the relationship.
 *
 * This function is the same as the C++ method
 * GDALRelationship::GetLeftMappingTableFields().
 *
 * @return the field names, to be freed with CSLDestroy()
 *
 * @see GDALRelationshipGetRightMappingTableFields
 * @see GDALRelationshipSetLeftMappingTableFields
 */
char **
GDALRelationshipGetLeftMappingTableFields(GDALRelationshipH hRelationship)
{
    VALIDATE_POINTER1(hRelationship,
                      "GDALRelationshipGetLeftMappingTableFields", nullptr);

    const auto &fields = GDALRelationship::FromHandle(hRelationship)
                             ->GetLeftMappingTableFields();
    return CPLStringList(fields).StealList();
}

/************************************************************************/
/*          GDALRelationshipGetRightMappingTableFields()                */
/************************************************************************/

/**
 * \brief Get the names of the mapping table fields which correspond to the
 * participating fields from the right table in the relationship.
 *
 * This function is the same as the C++ method
 * GDALRelationship::GetRightMappingTableFields().
 *
 * @return the field names, to be freed with CSLDestroy()
 *
 * @see GDALRelationshipGetLeftMappingTableFields
 * @see GDALRelationshipSetRightMappingTableFields
 */
char **
GDALRelationshipGetRightMappingTableFields(GDALRelationshipH hRelationship)
{
    VALIDATE_POINTER1(hRelationship,
                      "GDALRelationshipGetRightMappingTableFields", nullptr);

    const auto &fields = GDALRelationship::FromHandle(hRelationship)
                             ->GetRightMappingTableFields();
    return CPLStringList(fields).StealList();
}

/************************************************************************/
/*           GDALRelationshipSetLeftMappingTableFields()                */
/************************************************************************/

/**
 * \brief Sets the names of the mapping table fields which correspond to the
 * participating fields from the left table in the relationship.
 *
 * This function is the same as the C++ method
 * GDALRelationship::SetLeftMappingTableFields().
 *
 * @param hRelationship handle to the relationship to apply the left table
 * fields to.
 * @param papszFields the names of the fields.
 *
 * @see GDALRelationshipGetLeftMappingTableFields
 * @see GDALRelationshipSetRightMappingTableFields
 */
void GDALRelationshipSetLeftMappingTableFields(GDALRelationshipH hRelationship,
                                               CSLConstList papszFields)
{
    GDALRelationship::FromHandle(hRelationship)
        ->SetLeftMappingTableFields(cpl::ToVector(papszFields));
}

/************************************************************************/
/*              GDALRelationshipSetRightMappingTableFields()            */
/************************************************************************/

/**
 * \brief Sets the names of the mapping table fields which correspond to the
 * participating fields from the right table in the relationship.
 *
 * This function is the same as the C++ method
 * GDALRelationship::SetRightMappingTableFields().
 *
 * @param hRelationship handle to the relationship to apply the right table
 * fields to.
 * @param papszFields the names of the fields.
 *
 * @see GDALRelationshipGetRightMappingTableFields
 * @see GDALRelationshipSetLeftMappingTableFields
 */
void GDALRelationshipSetRightMappingTableFields(GDALRelationshipH hRelationship,
                                                CSLConstList papszFields)
{
    GDALRelationship::FromHandle(hRelationship)
        ->SetRightMappingTableFields(cpl::ToVector(papszFields));
}

/************************************************************************/
/*                  GDALRelationshipGetType()                           */
/************************************************************************/

/**
 * \brief Get the type of the relationship.
 *
 * This function is the same as the C++ method
 * GDALRelationship::GetType().
 *
 * @return relationship type.
 *
 * @see GDALRelationshipSetType
 */
GDALRelationshipType GDALRelationshipGetType(GDALRelationshipH hRelationship)
{
    return GDALRelationship::FromHandle(hRelationship)->GetType();
}

/************************************************************************/
/*                  GDALRelationshipSetType()                           */
/************************************************************************/

/**
 * \brief Sets the type of the relationship.
 *
 * This function is the same as the C++ method
 * GDALRelationship::SetType().
 *
 * @see GDALRelationshipGetType
 */
void GDALRelationshipSetType(GDALRelationshipH hRelationship,
                             GDALRelationshipType eType)
{
    return GDALRelationship::FromHandle(hRelationship)->SetType(eType);
}

/************************************************************************/
/*                  GDALRelationshipGetForwardPathLabel()               */
/************************************************************************/

/**
 * \brief Get the label of the forward path for the relationship.
 *
 * This function is the same as the C++ method
 * GDALRelationship::GetForwardPathLabel().
 *
 * The forward and backward path labels are free-form, user-friendly strings
 * which can be used to generate descriptions of the relationship between
 * features from the right and left tables.
 *
 * E.g. when the left table contains buildings and the right table contains
 * furniture, the forward path label could be "contains" and the backward path
 * label could be "is located within". A client could then generate a
 * user friendly description string such as "fire hose 1234 is located within
 * building 15a".
 *
 * @return forward path label
 *
 * @see GDALRelationshipSetForwardPathLabel()
 * @see GDALRelationshipGetBackwardPathLabel()
 */
const char *GDALRelationshipGetForwardPathLabel(GDALRelationshipH hRelationship)
{
    VALIDATE_POINTER1(hRelationship, "GDALRelationshipGetForwardPathLabel",
                      nullptr);

    return GDALRelationship::FromHandle(hRelationship)
        ->GetForwardPathLabel()
        .c_str();
}

/************************************************************************/
/*                  GDALRelationshipSetForwardPathLabel()               */
/************************************************************************/
/**
 * \brief Sets the label of the forward path for the relationship.
 *
 * This function is the same as the CPP method
 * GDALRelationship::SetForwardPathLabel().
 *
 * The forward and backward path labels are free-form, user-friendly strings
 * which can be used to generate descriptions of the relationship between
 * features from the right and left tables.
 *
 * E.g. when the left table contains buildings and the right table contains
 * furniture, the forward path label could be "contains" and the backward path
 * label could be "is located within". A client could then generate a
 * user friendly description string such as "fire hose 1234 is located within
 * building 15a".
 *
 * @param hRelationship handle to the relationship to apply the new label to.
 * @param pszLabel the label to set.
 *
 * @see GDALRelationshipGetForwardPathLabel
 * @see GDALRelationshipSetBackwardPathLabel
 */
void GDALRelationshipSetForwardPathLabel(GDALRelationshipH hRelationship,
                                         const char *pszLabel)

{
    GDALRelationship::FromHandle(hRelationship)->SetForwardPathLabel(pszLabel);
}

/************************************************************************/
/*                  GDALRelationshipGetForwardPathLabel()               */
/************************************************************************/

/**
 * \brief Get the label of the backward path for the relationship.
 *
 * This function is the same as the C++ method
 * GDALRelationship::GetBackwardPathLabel().
 *
 * The forward and backward path labels are free-form, user-friendly strings
 * which can be used to generate descriptions of the relationship between
 * features from the right and left tables.
 *
 * E.g. when the left table contains buildings and the right table contains
 * furniture, the forward path label could be "contains" and the backward path
 * label could be "is located within". A client could then generate a
 * user friendly description string such as "fire hose 1234 is located within
 * building 15a".
 *
 * @return backward path label
 *
 * @see GDALRelationshipSetBackwardPathLabel()
 * @see GDALRelationshipGetForwardPathLabel()
 */
const char *
GDALRelationshipGetBackwardPathLabel(GDALRelationshipH hRelationship)
{
    VALIDATE_POINTER1(hRelationship, "GDALRelationshipGetBackwardPathLabel",
                      nullptr);

    return GDALRelationship::FromHandle(hRelationship)
        ->GetBackwardPathLabel()
        .c_str();
}

/************************************************************************/
/*                 GDALRelationshipSetBackwardPathLabel()               */
/************************************************************************/
/**
 * \brief Sets the label of the backward path for the relationship.
 *
 * This function is the same as the CPP method
 * GDALRelationship::SetBackwardPathLabel().
 *
 * The forward and backward path labels are free-form, user-friendly strings
 * which can be used to generate descriptions of the relationship between
 * features from the right and left tables.
 *
 * E.g. when the left table contains buildings and the right table contains
 * furniture, the forward path label could be "contains" and the backward path
 * label could be "is located within". A client could then generate a
 * user friendly description string such as "fire hose 1234 is located within
 * building 15a".
 *
 * @param hRelationship handle to the relationship to apply the new label to.
 * @param pszLabel the label to set.
 *
 * @see GDALRelationshipGetBackwardPathLabel
 * @see GDALRelationshipSetForwardPathLabel
 */
void GDALRelationshipSetBackwardPathLabel(GDALRelationshipH hRelationship,
                                          const char *pszLabel)
{
    GDALRelationship::FromHandle(hRelationship)->SetBackwardPathLabel(pszLabel);
}

/************************************************************************/
/*                  GDALRelationshipGetRelatedTableType()               */
/************************************************************************/

/**
 * \brief Get the type string of the related table.
 *
 * This function is the same as the C++ method
 * GDALRelationship::GetRelatedTableType().
 *
 * This a free-form string representing the type of related features, where the
 * exact interpretation is format dependent. For instance, table types from
 * GeoPackage relationships will directly reflect the categories from the
 * GeoPackage related tables extension (i.e. "media", "simple attributes",
 * "features", "attributes" and "tiles").
 *
 * @return related table type
 *
 * @see GDALRelationshipSetRelatedTableType
 */
const char *GDALRelationshipGetRelatedTableType(GDALRelationshipH hRelationship)
{
    VALIDATE_POINTER1(hRelationship, "GDALRelationshipGetRelatedTableType",
                      nullptr);

    return GDALRelationship::FromHandle(hRelationship)
        ->GetRelatedTableType()
        .c_str();
}

/************************************************************************/
/*                 GDALRelationshipSetRelatedTableType()                */
/************************************************************************/
/**
 * \brief Sets the type string of the related table.
 *
 * This function is the same as the CPP method
 * GDALRelationship::SetRelatedTableType().
 *
 * This a free-form string representing the type of related features, where the
 * exact interpretation is format dependent. For instance, table types from
 * GeoPackage relationships will directly reflect the categories from the
 * GeoPackage related tables extension (i.e. "media", "simple attributes",
 * "features", "attributes" and "tiles").
 *
 * @param hRelationship handle to the relationship to apply the new type to.
 * @param pszType the type to set.
 *
 * @see GDALRelationshipGetRelatedTableType
 */
void GDALRelationshipSetRelatedTableType(GDALRelationshipH hRelationship,
                                         const char *pszType)
{
    GDALRelationship::FromHandle(hRelationship)->SetRelatedTableType(pszType);
}
