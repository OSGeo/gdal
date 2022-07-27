/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements FileGDB relationship handling.
 * Author:   Nyall Dawson, <nyall dot dawson at gmail dot com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Nyall Dawson <nyall dot dawson at gmail dot com>
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

#ifndef FILEGDB_RELATIONSHIP_H
#define FILEGDB_RELATIONSHIP_H

#include "cpl_minixml.h"
#include "filegdb_gdbtoogrfieldtype.h"
#include "gdal.h"

/************************************************************************/
/*                      ParseXMLFieldDomainDef()                        */
/************************************************************************/

inline std::unique_ptr<GDALRelationship> ParseXMLRelationshipDef(const std::string& domainDef)
{
    CPLXMLTreeCloser oTree(CPLParseXMLString(domainDef.c_str()));
    if( !oTree.get() )
    {
        return nullptr;
    }

    const CPLXMLNode* psRelationship = CPLGetXMLNode(oTree.get(), "=DERelationshipClassInfo");
    if( psRelationship == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find root 'Relationship' node");
        return nullptr;
    }

    const char* pszName = CPLGetXMLValue(psRelationship, "Name", "");

    const char* pszOriginTableName = CPLGetXMLValue(psRelationship, "OriginClassNames.Name", nullptr);
    if ( pszOriginTableName == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find OriginClassName table node");
        return nullptr;
    }

    const char* pszDestinationTableName = CPLGetXMLValue(psRelationship, "DestinationClassNames.Name", nullptr);
    if ( pszDestinationTableName == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find DestinationClassNames table node");
        return nullptr;
    }

    const char* pszCardinality = CPLGetXMLValue(psRelationship, "Cardinality", "");
    if ( pszCardinality == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Cardinality node");
        return nullptr;
    }

    GDALRelationshipCardinality eCardinality = GRC_ONE_TO_MANY;
    if ( EQUAL(pszCardinality, "esriRelCardinalityOneToOne"))
    {
        eCardinality = GRC_ONE_TO_ONE;
    }
    else if ( EQUAL(pszCardinality, "esriRelCardinalityOneToMany"))
    {
        eCardinality = GRC_ONE_TO_MANY;
    }
    else if ( EQUAL(pszCardinality, "esriRelCardinalityManyToMany"))
    {
        eCardinality = GRC_MANY_TO_MANY;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unknown cardinality: %s", pszCardinality);
        return nullptr;
    }

    std::unique_ptr< GDALRelationship > poRelationship( new GDALRelationship( pszName,
                                                                              pszOriginTableName,
                                                                              pszDestinationTableName,
                                                                              eCardinality ) );

    if ( eCardinality == GRC_MANY_TO_MANY )
    {
        // seems to be that the middle table name always follows the relationship name?
        poRelationship->SetMappingTableName(pszName);
    }

    std::vector<std::string> aosOriginKeys;
    std::vector<std::string> aosMappingOriginKeys;
    std::vector<std::string> aosDestinationKeys;
    std::vector<std::string> aosMappingDestinationKeys;

    const CPLXMLNode* psOriginClassKeys = CPLGetXMLNode(psRelationship, "OriginClassKeys");
    if ( psOriginClassKeys == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find OriginClassKeys node");
        return nullptr;
    }
    for( const CPLXMLNode* psIter = psOriginClassKeys->psChild;
                                    psIter; psIter = psIter->psNext )
    {
        if( psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "RelationshipClassKey") == 0 )
        {
            const char* pszObjectKeyName = CPLGetXMLValue(psIter, "ObjectKeyName", "");
            if ( pszObjectKeyName == nullptr )
            {
                continue;
            }

            const char* pszKeyRole = CPLGetXMLValue(psIter, "KeyRole", "");
            if ( pszKeyRole == nullptr )
            {
                continue;
            }
            if ( EQUAL(pszKeyRole, "esriRelKeyRoleOriginPrimary"))
            {
                aosOriginKeys.emplace_back(pszObjectKeyName);
            }
            else if ( EQUAL(pszKeyRole, "esriRelKeyRoleOriginForeign"))
            {
                if ( eCardinality == GRC_MANY_TO_MANY )
                  aosMappingOriginKeys.emplace_back(pszObjectKeyName);
                else
                  aosDestinationKeys.emplace_back(pszObjectKeyName);
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unknown KeyRole: %s", pszKeyRole);
                return nullptr;
            }
        }
    }

    const CPLXMLNode* psDestinationClassKeys = CPLGetXMLNode(psRelationship, "DestinationClassKeys");
    if ( psDestinationClassKeys != nullptr )
    {
        for( const CPLXMLNode* psIter = psDestinationClassKeys->psChild;
                                        psIter; psIter = psIter->psNext )
        {
            if( psIter->eType == CXT_Element &&
                strcmp(psIter->pszValue, "RelationshipClassKey") == 0 )
            {
                const char* pszObjectKeyName = CPLGetXMLValue(psIter, "ObjectKeyName", "");
                if ( pszObjectKeyName == nullptr )
                {
                    continue;
                }

                const char* pszKeyRole = CPLGetXMLValue(psIter, "KeyRole", "");
                if ( pszKeyRole == nullptr )
                {
                    continue;
                }
                if ( EQUAL(pszKeyRole, "esriRelKeyRoleDestinationPrimary"))
                {
                    aosDestinationKeys.emplace_back(pszObjectKeyName);
                }
                else if ( EQUAL(pszKeyRole, "esriRelKeyRoleDestinationForeign"))
                {
                    aosMappingDestinationKeys.emplace_back(pszObjectKeyName);
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Unknown KeyRole: %s", pszKeyRole);
                    return nullptr;
                }
            }
        }
    }

    poRelationship->SetLeftTableFields(aosOriginKeys);
    poRelationship->SetLeftMappingTableFields(aosMappingOriginKeys);
    poRelationship->SetRightTableFields(aosDestinationKeys);
    poRelationship->SetRightMappingTableFields(aosMappingDestinationKeys);

    const char* pszForwardPathLabel = CPLGetXMLValue(psRelationship, "ForwardPathLabel", "");
    if ( pszForwardPathLabel != nullptr )
    {
        poRelationship->SetForwardPathLabel(pszForwardPathLabel);
    }
    const char* pszBackwardPathLabel = CPLGetXMLValue(psRelationship, "BackwardPathLabel", "");
    if ( pszBackwardPathLabel != nullptr )
    {
        poRelationship->SetBackwardPathLabel(pszBackwardPathLabel);
    }

    const char* pszIsComposite = CPLGetXMLValue(psRelationship, "IsComposite", "");
    if ( pszIsComposite != nullptr && EQUAL(pszIsComposite, "true") )
    {
        poRelationship->SetType(GRT_COMPOSITE);
    }
    else
    {
        poRelationship->SetType(GRT_ASSOCIATION);
    }

    const char* pszIsAttachmentRelationship = CPLGetXMLValue(psRelationship, "IsAttachmentRelationship", "");
    if ( pszIsAttachmentRelationship != nullptr && EQUAL(pszIsAttachmentRelationship, "true") )
    {
        poRelationship->SetRelatedTableType("media");
    }
    else
    {
        poRelationship->SetRelatedTableType("feature");
    }

    return poRelationship;
}

#endif // FILEGDB_RELATIONSHIP_H
