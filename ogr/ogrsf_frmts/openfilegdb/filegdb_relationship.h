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

inline std::unique_ptr<GDALRelationship>
ParseXMLRelationshipDef(const std::string &domainDef)
{
    CPLXMLTreeCloser oTree(CPLParseXMLString(domainDef.c_str()));
    if (!oTree.get())
    {
        return nullptr;
    }

    const CPLXMLNode *psRelationship =
        CPLGetXMLNode(oTree.get(), "=DERelationshipClassInfo");
    if (psRelationship == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find root 'Relationship' node");
        return nullptr;
    }

    const char *pszName = CPLGetXMLValue(psRelationship, "Name", "");

    const char *pszOriginTableName =
        CPLGetXMLValue(psRelationship, "OriginClassNames.Name", nullptr);
    if (pszOriginTableName == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find OriginClassName table node");
        return nullptr;
    }

    const char *pszDestinationTableName =
        CPLGetXMLValue(psRelationship, "DestinationClassNames.Name", nullptr);
    if (pszDestinationTableName == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find DestinationClassNames table node");
        return nullptr;
    }

    const char *pszCardinality =
        CPLGetXMLValue(psRelationship, "Cardinality", "");
    if (pszCardinality == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find Cardinality node");
        return nullptr;
    }

    GDALRelationshipCardinality eCardinality = GRC_ONE_TO_MANY;
    if (EQUAL(pszCardinality, "esriRelCardinalityOneToOne"))
    {
        eCardinality = GRC_ONE_TO_ONE;
    }
    else if (EQUAL(pszCardinality, "esriRelCardinalityOneToMany"))
    {
        eCardinality = GRC_ONE_TO_MANY;
    }
    else if (EQUAL(pszCardinality, "esriRelCardinalityManyToMany"))
    {
        eCardinality = GRC_MANY_TO_MANY;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unknown cardinality: %s",
                 pszCardinality);
        return nullptr;
    }

    std::unique_ptr<GDALRelationship> poRelationship(new GDALRelationship(
        pszName, pszOriginTableName, pszDestinationTableName, eCardinality));

    if (eCardinality == GRC_MANY_TO_MANY)
    {
        // seems to be that the middle table name always follows the
        // relationship name?
        poRelationship->SetMappingTableName(pszName);
    }

    std::vector<std::string> aosOriginKeys;
    std::vector<std::string> aosMappingOriginKeys;
    std::vector<std::string> aosDestinationKeys;
    std::vector<std::string> aosMappingDestinationKeys;

    const CPLXMLNode *psOriginClassKeys =
        CPLGetXMLNode(psRelationship, "OriginClassKeys");
    if (psOriginClassKeys == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find OriginClassKeys node");
        return nullptr;
    }
    for (const CPLXMLNode *psIter = psOriginClassKeys->psChild; psIter;
         psIter = psIter->psNext)
    {
        if (psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "RelationshipClassKey") == 0)
        {
            const char *pszObjectKeyName =
                CPLGetXMLValue(psIter, "ObjectKeyName", "");
            if (pszObjectKeyName == nullptr)
            {
                continue;
            }

            const char *pszKeyRole = CPLGetXMLValue(psIter, "KeyRole", "");
            if (pszKeyRole == nullptr)
            {
                continue;
            }
            if (EQUAL(pszKeyRole, "esriRelKeyRoleOriginPrimary"))
            {
                aosOriginKeys.emplace_back(pszObjectKeyName);
            }
            else if (EQUAL(pszKeyRole, "esriRelKeyRoleOriginForeign"))
            {
                if (eCardinality == GRC_MANY_TO_MANY)
                    aosMappingOriginKeys.emplace_back(pszObjectKeyName);
                else
                    aosDestinationKeys.emplace_back(pszObjectKeyName);
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Unknown KeyRole: %s",
                         pszKeyRole);
                return nullptr;
            }
        }
    }

    const CPLXMLNode *psDestinationClassKeys =
        CPLGetXMLNode(psRelationship, "DestinationClassKeys");
    if (psDestinationClassKeys != nullptr)
    {
        for (const CPLXMLNode *psIter = psDestinationClassKeys->psChild; psIter;
             psIter = psIter->psNext)
        {
            if (psIter->eType == CXT_Element &&
                strcmp(psIter->pszValue, "RelationshipClassKey") == 0)
            {
                const char *pszObjectKeyName =
                    CPLGetXMLValue(psIter, "ObjectKeyName", "");
                if (pszObjectKeyName == nullptr)
                {
                    continue;
                }

                const char *pszKeyRole = CPLGetXMLValue(psIter, "KeyRole", "");
                if (pszKeyRole == nullptr)
                {
                    continue;
                }
                if (EQUAL(pszKeyRole, "esriRelKeyRoleDestinationPrimary"))
                {
                    aosDestinationKeys.emplace_back(pszObjectKeyName);
                }
                else if (EQUAL(pszKeyRole, "esriRelKeyRoleDestinationForeign"))
                {
                    aosMappingDestinationKeys.emplace_back(pszObjectKeyName);
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Unknown KeyRole: %s",
                             pszKeyRole);
                    return nullptr;
                }
            }
        }
    }

    poRelationship->SetLeftTableFields(aosOriginKeys);
    poRelationship->SetLeftMappingTableFields(aosMappingOriginKeys);
    poRelationship->SetRightTableFields(aosDestinationKeys);
    poRelationship->SetRightMappingTableFields(aosMappingDestinationKeys);

    const char *pszForwardPathLabel =
        CPLGetXMLValue(psRelationship, "ForwardPathLabel", "");
    if (pszForwardPathLabel != nullptr)
    {
        poRelationship->SetForwardPathLabel(pszForwardPathLabel);
    }
    const char *pszBackwardPathLabel =
        CPLGetXMLValue(psRelationship, "BackwardPathLabel", "");
    if (pszBackwardPathLabel != nullptr)
    {
        poRelationship->SetBackwardPathLabel(pszBackwardPathLabel);
    }

    const char *pszIsComposite =
        CPLGetXMLValue(psRelationship, "IsComposite", "");
    if (pszIsComposite != nullptr && EQUAL(pszIsComposite, "true"))
    {
        poRelationship->SetType(GRT_COMPOSITE);
    }
    else
    {
        poRelationship->SetType(GRT_ASSOCIATION);
    }

    const char *pszIsAttachmentRelationship =
        CPLGetXMLValue(psRelationship, "IsAttachmentRelationship", "");
    if (pszIsAttachmentRelationship != nullptr &&
        EQUAL(pszIsAttachmentRelationship, "true"))
    {
        poRelationship->SetRelatedTableType("media");
    }
    else
    {
        poRelationship->SetRelatedTableType("features");
    }
    return poRelationship;
}

/************************************************************************/
/*                      BuildXMLRelationshipDef()                       */
/************************************************************************/

inline std::string
BuildXMLRelationshipDef(const GDALRelationship *poRelationship, int iDsid,
                        const std::string &osMappingTableOidName,
                        std::string &failureReason)
{
    std::string osNS = "typens";
    const char *pszRootElt = "DERelationshipClassInfo";

    CPLXMLTreeCloser oTree(CPLCreateXMLNode(nullptr, CXT_Element, pszRootElt));
    CPLXMLNode *psRoot = oTree.get();

    CPLAddXMLAttributeAndValue(psRoot, "xsi:type",
                               "typens:DERelationshipClassInfo");

    CPLAddXMLAttributeAndValue(psRoot, "xmlns:xsi",
                               "http://www.w3.org/2001/XMLSchema-instance");
    CPLAddXMLAttributeAndValue(psRoot, "xmlns:xs",
                               "http://www.w3.org/2001/XMLSchema");
    CPLAddXMLAttributeAndValue(psRoot, ("xmlns:" + osNS).c_str(),
                               "http://www.esri.com/schemas/ArcGIS/10.1");

    CPLCreateXMLElementAndValue(psRoot, "CatalogPath",
                                ("\\" + poRelationship->GetName()).c_str());
    CPLCreateXMLElementAndValue(psRoot, "Name",
                                poRelationship->GetName().c_str());
    CPLCreateXMLElementAndValue(psRoot, "ChildrenExpanded", "false");
    CPLCreateXMLElementAndValue(psRoot, "DatasetType",
                                "esriDTRelationshipClass");
    CPLCreateXMLElementAndValue(psRoot, "DSID",
                                CPLString().Printf("%d", iDsid));
    CPLCreateXMLElementAndValue(psRoot, "Versioned", "false");
    CPLCreateXMLElementAndValue(psRoot, "CanVersion", "false");
    CPLCreateXMLElementAndValue(psRoot, "ConfigurationKeyword", "");
    CPLCreateXMLElementAndValue(psRoot, "RequiredGeodatabaseClientVersion",
                                "10.0");
    CPLCreateXMLElementAndValue(psRoot, "HasOID", "false");

    auto psGPFieldInfoExs =
        CPLCreateXMLNode(psRoot, CXT_Element, "GPFieldInfoExs");
    CPLAddXMLAttributeAndValue(psGPFieldInfoExs, "xsi:type",
                               "typens:ArrayOfGPFieldInfoEx");

    // for many-to-many relationships this is the OID field from the mapping
    // table
    if (poRelationship->GetCardinality() ==
        GDALRelationshipCardinality::GRC_MANY_TO_MANY)
    {
        CPLCreateXMLElementAndValue(psRoot, "OIDFieldName",
                                    osMappingTableOidName.c_str());

        // field info from mapping table

        // OID field
        auto psGPFieldInfoEx =
            CPLCreateXMLNode(psGPFieldInfoExs, CXT_Element, "GPFieldInfoEx");
        CPLAddXMLAttributeAndValue(psGPFieldInfoEx, "xsi:type",
                                   "typens:GPFieldInfoEx");
        CPLCreateXMLElementAndValue(psGPFieldInfoEx, "Name",
                                    osMappingTableOidName.c_str());

        // hopefully not required...
        // CPLCreateXMLElementAndValue(psGPFieldInfoEx, "AliasName", "false");
        // CPLCreateXMLElementAndValue(psGPFieldInfoEx, "FieldType", "false");
        // CPLCreateXMLElementAndValue(psGPFieldInfoEx, "IsNullable", "false");

        // origin foreign key field
        psGPFieldInfoEx =
            CPLCreateXMLNode(psGPFieldInfoExs, CXT_Element, "GPFieldInfoEx");
        CPLAddXMLAttributeAndValue(psGPFieldInfoEx, "xsi:type",
                                   "typens:GPFieldInfoEx");
        if (!poRelationship->GetLeftMappingTableFields().empty())
        {
            CPLCreateXMLElementAndValue(
                psGPFieldInfoEx, "Name",
                poRelationship->GetLeftMappingTableFields()[0].c_str());
        }

        // destination foreign key field
        psGPFieldInfoEx =
            CPLCreateXMLNode(psGPFieldInfoExs, CXT_Element, "GPFieldInfoEx");
        CPLAddXMLAttributeAndValue(psGPFieldInfoEx, "xsi:type",
                                   "typens:GPFieldInfoEx");
        if (!poRelationship->GetRightMappingTableFields().empty())
        {
            CPLCreateXMLElementAndValue(
                psGPFieldInfoEx, "Name",
                poRelationship->GetRightMappingTableFields()[0].c_str());
        }
    }
    else
    {
        CPLCreateXMLElementAndValue(psRoot, "OIDFieldName", "");
    }

    CPLCreateXMLElementAndValue(psRoot, "CLSID", "");
    CPLCreateXMLElementAndValue(psRoot, "EXTCLSID", "");

    auto psRelationshipClassNames =
        CPLCreateXMLNode(psRoot, CXT_Element, "RelationshipClassNames");
    CPLAddXMLAttributeAndValue(psRelationshipClassNames, "xsi:type",
                               "typens:Names");

    CPLCreateXMLElementAndValue(psRoot, "AliasName", "");
    CPLCreateXMLElementAndValue(psRoot, "ModelName", "");
    CPLCreateXMLElementAndValue(psRoot, "HasGlobalID", "false");
    CPLCreateXMLElementAndValue(psRoot, "GlobalIDFieldName", "");
    CPLCreateXMLElementAndValue(psRoot, "RasterFieldName", "");

    auto psExtensionProperties =
        CPLCreateXMLNode(psRoot, CXT_Element, "ExtensionProperties");
    CPLAddXMLAttributeAndValue(psExtensionProperties, "xsi:type",
                               "typens:PropertySet");
    auto psPropertyArray =
        CPLCreateXMLNode(psExtensionProperties, CXT_Element, "PropertyArray");
    CPLAddXMLAttributeAndValue(psPropertyArray, "xsi:type",
                               "typens:ArrayOfPropertySetProperty");

    auto psControllerMemberships =
        CPLCreateXMLNode(psRoot, CXT_Element, "ControllerMemberships");
    CPLAddXMLAttributeAndValue(psControllerMemberships, "xsi:type",
                               "typens:ArrayOfControllerMembership");

    CPLCreateXMLElementAndValue(psRoot, "EditorTrackingEnabled", "false");
    CPLCreateXMLElementAndValue(psRoot, "CreatorFieldName", "");
    CPLCreateXMLElementAndValue(psRoot, "CreatedAtFieldName", "");
    CPLCreateXMLElementAndValue(psRoot, "EditorFieldName", "");
    CPLCreateXMLElementAndValue(psRoot, "EditedAtFieldName", "");
    CPLCreateXMLElementAndValue(psRoot, "IsTimeInUTC", "true");

    switch (poRelationship->GetCardinality())
    {
        case GDALRelationshipCardinality::GRC_ONE_TO_ONE:
            CPLCreateXMLElementAndValue(psRoot, "Cardinality",
                                        "esriRelCardinalityOneToOne");
            break;
        case GDALRelationshipCardinality::GRC_ONE_TO_MANY:
            CPLCreateXMLElementAndValue(psRoot, "Cardinality",
                                        "esriRelCardinalityOneToMany");
            break;
        case GDALRelationshipCardinality::GRC_MANY_TO_MANY:
            CPLCreateXMLElementAndValue(psRoot, "Cardinality",
                                        "esriRelCardinalityManyToMany");
            break;
        case GDALRelationshipCardinality::GRC_MANY_TO_ONE:
            failureReason = "Many to one relationships are not supported";
            return {};
    }

    CPLCreateXMLElementAndValue(psRoot, "Notification",
                                "esriRelNotificationNone");
    CPLCreateXMLElementAndValue(psRoot, "IsAttributed", "false");

    switch (poRelationship->GetType())
    {
        case GDALRelationshipType::GRT_ASSOCIATION:
            CPLCreateXMLElementAndValue(psRoot, "IsComposite", "false");
            break;

        case GDALRelationshipType::GRT_COMPOSITE:
            CPLCreateXMLElementAndValue(psRoot, "IsComposite", "true");
            break;

        case GDALRelationshipType::GRT_AGGREGATION:
            failureReason = "Aggregate relationships are not supported";
            return {};
    }

    auto psOriginClassNames =
        CPLCreateXMLNode(psRoot, CXT_Element, "OriginClassNames");
    CPLAddXMLAttributeAndValue(psOriginClassNames, "xsi:type", "typens:Names");
    CPLCreateXMLElementAndValue(psOriginClassNames, "Name",
                                poRelationship->GetLeftTableName().c_str());

    auto psDestinationClassNames =
        CPLCreateXMLNode(psRoot, CXT_Element, "DestinationClassNames");
    CPLAddXMLAttributeAndValue(psDestinationClassNames, "xsi:type",
                               "typens:Names");
    CPLCreateXMLElementAndValue(psDestinationClassNames, "Name",
                                poRelationship->GetRightTableName().c_str());

    CPLCreateXMLElementAndValue(psRoot, "KeyType", "esriRelKeyTypeSingle");
    CPLCreateXMLElementAndValue(psRoot, "ClassKey", "esriRelClassKeyUndefined");
    CPLCreateXMLElementAndValue(psRoot, "ForwardPathLabel",
                                poRelationship->GetForwardPathLabel().c_str());
    CPLCreateXMLElementAndValue(psRoot, "BackwardPathLabel",
                                poRelationship->GetBackwardPathLabel().c_str());

    CPLCreateXMLElementAndValue(psRoot, "IsReflexive", "false");

    auto psOriginClassKeys =
        CPLCreateXMLNode(psRoot, CXT_Element, "OriginClassKeys");
    CPLAddXMLAttributeAndValue(psOriginClassKeys, "xsi:type",
                               "typens:ArrayOfRelationshipClassKey");

    auto psRelationshipClassKeyOrigin = CPLCreateXMLNode(
        psOriginClassKeys, CXT_Element, "RelationshipClassKey");
    CPLAddXMLAttributeAndValue(psRelationshipClassKeyOrigin, "xsi:type",
                               "typens:RelationshipClassKey");
    if (!poRelationship->GetLeftTableFields().empty())
    {
        CPLCreateXMLElementAndValue(
            psRelationshipClassKeyOrigin, "ObjectKeyName",
            poRelationship->GetLeftTableFields()[0].c_str());
    }
    CPLCreateXMLElementAndValue(psRelationshipClassKeyOrigin, "ClassKeyName",
                                "");
    CPLCreateXMLElementAndValue(psRelationshipClassKeyOrigin, "KeyRole",
                                "esriRelKeyRoleOriginPrimary");

    if (poRelationship->GetCardinality() ==
        GDALRelationshipCardinality::GRC_MANY_TO_MANY)
    {
        auto psRelationshipClassKeyOriginManyToMany = CPLCreateXMLNode(
            psOriginClassKeys, CXT_Element, "RelationshipClassKey");
        CPLAddXMLAttributeAndValue(psRelationshipClassKeyOriginManyToMany,
                                   "xsi:type", "typens:RelationshipClassKey");
        if (!poRelationship->GetLeftMappingTableFields().empty())
        {
            CPLCreateXMLElementAndValue(
                psRelationshipClassKeyOriginManyToMany, "ObjectKeyName",
                poRelationship->GetLeftMappingTableFields()[0].c_str());
        }
        CPLCreateXMLElementAndValue(psRelationshipClassKeyOriginManyToMany,
                                    "ClassKeyName", "");
        CPLCreateXMLElementAndValue(psRelationshipClassKeyOriginManyToMany,
                                    "KeyRole", "esriRelKeyRoleOriginForeign");
    }

    if (poRelationship->GetCardinality() !=
        GDALRelationshipCardinality::GRC_MANY_TO_MANY)
    {
        auto psRelationshipClassKeyForeign = CPLCreateXMLNode(
            psOriginClassKeys, CXT_Element, "RelationshipClassKey");
        CPLAddXMLAttributeAndValue(psRelationshipClassKeyForeign, "xsi:type",
                                   "typens:RelationshipClassKey");
        if (!poRelationship->GetRightTableFields().empty())
        {
            CPLCreateXMLElementAndValue(
                psRelationshipClassKeyForeign, "ObjectKeyName",
                poRelationship->GetRightTableFields()[0].c_str());
        }
        CPLCreateXMLElementAndValue(psRelationshipClassKeyForeign,
                                    "ClassKeyName", "");
        CPLCreateXMLElementAndValue(psRelationshipClassKeyForeign, "KeyRole",
                                    "esriRelKeyRoleOriginForeign");
    }
    else
    {
        auto psDestinationClassKeys =
            CPLCreateXMLNode(psRoot, CXT_Element, "DestinationClassKeys");
        CPLAddXMLAttributeAndValue(psDestinationClassKeys, "xsi:type",
                                   "typens:ArrayOfRelationshipClassKey");

        auto psRelationshipClassKeyForeign = CPLCreateXMLNode(
            psDestinationClassKeys, CXT_Element, "RelationshipClassKey");
        CPLAddXMLAttributeAndValue(psRelationshipClassKeyForeign, "xsi:type",
                                   "typens:RelationshipClassKey");
        if (!poRelationship->GetRightTableFields().empty())
        {
            CPLCreateXMLElementAndValue(
                psRelationshipClassKeyForeign, "ObjectKeyName",
                poRelationship->GetRightTableFields()[0].c_str());
        }
        CPLCreateXMLElementAndValue(psRelationshipClassKeyForeign,
                                    "ClassKeyName", "");
        CPLCreateXMLElementAndValue(psRelationshipClassKeyForeign, "KeyRole",
                                    "esriRelKeyRoleDestinationPrimary");

        auto psRelationshipClassKeyForeignManyToMany = CPLCreateXMLNode(
            psDestinationClassKeys, CXT_Element, "RelationshipClassKey");
        CPLAddXMLAttributeAndValue(psRelationshipClassKeyForeignManyToMany,
                                   "xsi:type", "typens:RelationshipClassKey");
        if (!poRelationship->GetRightMappingTableFields().empty())
        {
            CPLCreateXMLElementAndValue(
                psRelationshipClassKeyForeignManyToMany, "ObjectKeyName",
                poRelationship->GetRightMappingTableFields()[0].c_str());
        }
        CPLCreateXMLElementAndValue(psRelationshipClassKeyForeignManyToMany,
                                    "ClassKeyName", "");
        CPLCreateXMLElementAndValue(psRelationshipClassKeyForeignManyToMany,
                                    "KeyRole",
                                    "esriRelKeyRoleDestinationForeign");
    }

    auto psRelationshipRules =
        CPLCreateXMLNode(psRoot, CXT_Element, "RelationshipRules");
    CPLAddXMLAttributeAndValue(psRelationshipRules, "xsi:type",
                               "typens:ArrayOfRelationshipRule");

    CPLCreateXMLElementAndValue(
        psRoot, "IsAttachmentRelationship",
        poRelationship->GetRelatedTableType() == "media" ? "true" : "false");
    CPLCreateXMLElementAndValue(psRoot, "ChangeTracked", "false");
    CPLCreateXMLElementAndValue(psRoot, "ReplicaTracked", "false");

    char *pszXML = CPLSerializeXMLTree(oTree.get());
    const std::string osXML(pszXML);
    CPLFree(pszXML);
    return osXML;
}

/************************************************************************/
/*                      BuildXMLRelationshipItemInfo()                  */
/************************************************************************/

inline std::string
BuildXMLRelationshipItemInfo(const GDALRelationship *poRelationship,
                             std::string & /*failureReason*/)
{
    CPLXMLTreeCloser oTree(
        CPLCreateXMLNode(nullptr, CXT_Element, "ESRI_ItemInformation"));
    CPLXMLNode *psRoot = oTree.get();

    CPLAddXMLAttributeAndValue(psRoot, "culture", "");

    CPLCreateXMLElementAndValue(psRoot, "name",
                                poRelationship->GetName().c_str());
    CPLCreateXMLElementAndValue(psRoot, "catalogPath",
                                ("\\" + poRelationship->GetName()).c_str());
    CPLCreateXMLElementAndValue(psRoot, "snippet", "");
    CPLCreateXMLElementAndValue(psRoot, "description", "");
    CPLCreateXMLElementAndValue(psRoot, "summary", "");
    CPLCreateXMLElementAndValue(psRoot, "title",
                                poRelationship->GetName().c_str());
    CPLCreateXMLElementAndValue(psRoot, "tags", "");
    CPLCreateXMLElementAndValue(psRoot, "type",
                                "File Geodatabase Relationship Class");

    auto psTypeKeywords = CPLCreateXMLNode(psRoot, CXT_Element, "typeKeywords");
    CPLCreateXMLElementAndValue(psTypeKeywords, "typekeyword", "Data");
    CPLCreateXMLElementAndValue(psTypeKeywords, "typekeyword", "Dataset");
    CPLCreateXMLElementAndValue(psTypeKeywords, "typekeyword", "Vector Data");
    CPLCreateXMLElementAndValue(psTypeKeywords, "typekeyword", "Feature Data");
    CPLCreateXMLElementAndValue(psTypeKeywords, "typekeyword",
                                "File Geodatabase");
    CPLCreateXMLElementAndValue(psTypeKeywords, "typekeyword", "GDB");
    CPLCreateXMLElementAndValue(psTypeKeywords, "typekeyword",
                                "Relationship Class");

    CPLCreateXMLElementAndValue(psRoot, "url", "");
    CPLCreateXMLElementAndValue(psRoot, "datalastModifiedTime", "");

    auto psExtent = CPLCreateXMLNode(psRoot, CXT_Element, "extent");
    CPLCreateXMLElementAndValue(psExtent, "xmin", "");
    CPLCreateXMLElementAndValue(psExtent, "ymin", "");
    CPLCreateXMLElementAndValue(psExtent, "xmax", "");
    CPLCreateXMLElementAndValue(psExtent, "ymax", "");

    CPLCreateXMLElementAndValue(psRoot, "minScale", "0");
    CPLCreateXMLElementAndValue(psRoot, "maxScale", "0");
    CPLCreateXMLElementAndValue(psRoot, "spatialReference", "");
    CPLCreateXMLElementAndValue(psRoot, "accessInformation", "");
    CPLCreateXMLElementAndValue(psRoot, "licenseInfo", "");
    CPLCreateXMLElementAndValue(psRoot, "typeID", "fgdb_relationship");
    CPLCreateXMLElementAndValue(psRoot, "isContainer", "false");
    CPLCreateXMLElementAndValue(psRoot, "browseDialogOnly", "false");
    CPLCreateXMLElementAndValue(psRoot, "propNames", "");
    CPLCreateXMLElementAndValue(psRoot, "propValues", "");

    char *pszXML = CPLSerializeXMLTree(oTree.get());
    const std::string osXML(pszXML);
    CPLFree(pszXML);
    return osXML;
}

/************************************************************************/
/*                 BuildXMLRelationshipDocumentation()                  */
/************************************************************************/

inline std::string
BuildXMLRelationshipDocumentation(const GDALRelationship * /*poRelationship*/,
                                  std::string & /*failureReason*/)
{
    CPLXMLTreeCloser oTree(CPLCreateXMLNode(nullptr, CXT_Element, "metadata"));
    CPLXMLNode *psRoot = oTree.get();

    CPLAddXMLAttributeAndValue(psRoot, "xml:lang", "en");

    auto psEsri = CPLCreateXMLNode(psRoot, CXT_Element, "Esri");
    CPLCreateXMLElementAndValue(psEsri, "CreaDate", "");
    CPLCreateXMLElementAndValue(psEsri, "CreaTime", "");
    CPLCreateXMLElementAndValue(psEsri, "ArcGISFormat", "1.0");
    CPLCreateXMLElementAndValue(psEsri, "SyncOnce", "TRUE");

    auto psDataProperties =
        CPLCreateXMLNode(psEsri, CXT_Element, "DataProperties");
    CPLCreateXMLNode(psDataProperties, CXT_Element, "lineage");

    char *pszXML = CPLSerializeXMLTree(oTree.get());
    const std::string osXML(pszXML);
    CPLFree(pszXML);
    return osXML;
}

#endif  // FILEGDB_RELATIONSHIP_H
