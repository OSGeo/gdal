/******************************************************************************
 *
 * Project:  PlanetLabs scene driver
 * Purpose:  Implements OGRPLScenesV1Layer
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2016, Planet Labs
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

#include "ogr_plscenes.h"
#include "ogrgeojsonreader.h"
#include <algorithm>

CPL_CVSID("$Id$");

/************************************************************************/
/*                           GetFieldCount()                            */
/************************************************************************/

int OGRPLScenesV1FeatureDefn::GetFieldCount()
{
    if( nFieldCount == 0 && m_poLayer != NULL )
        m_poLayer->EstablishLayerDefn();
    return nFieldCount;
}

/************************************************************************/
/*                          OGRPLScenesV1Layer()                        */
/************************************************************************/

OGRPLScenesV1Layer::OGRPLScenesV1Layer( OGRPLScenesV1Dataset* poDS,
                                        const char* pszName,
                                        const char* pszSpecURL,
                                        const char* pszItemsURL,
                                        GIntBig nCount ) :
    m_poDS(poDS),
    m_bFeatureDefnEstablished(false),
    m_poSRS(new OGRSpatialReference(SRS_WKT_WGS84)),
    m_osSpecURL(pszSpecURL),
    m_osItemsURL(pszItemsURL),
    m_nTotalFeatures(nCount),
    m_nNextFID(1),
    m_bEOF(false),
    m_bStillInFirstPage(true),
    m_nPageSize(atoi(CPLGetConfigOption("PLSCENES_PAGE_SIZE", "250"))),
    m_bInFeatureCountOrGetExtent(false),
    m_poPageObj(NULL),
    m_poFeatures(NULL),
    m_nFeatureIdx(0),
    m_bFilterMustBeClientSideEvaluated(false)
{
    // Cannot be moved to initializer list because of use of this, which MSVC 2008 doesn't like
    m_poFeatureDefn = new OGRPLScenesV1FeatureDefn(this, pszName);

    SetDescription(pszName);
    m_poFeatureDefn->SetGeomType(wkbMultiPolygon);
    m_poFeatureDefn->Reference();
    m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(m_poSRS);
    ResetReading();
}

/************************************************************************/
/*                          ~OGRPLScenesV1Layer()                       */
/************************************************************************/

OGRPLScenesV1Layer::~OGRPLScenesV1Layer()
{
    m_poFeatureDefn->DropRefToLayer();
    m_poFeatureDefn->Release();
    m_poSRS->Release();
    if( m_poPageObj != NULL )
        json_object_put(m_poPageObj);
}

/************************************************************************/
/*                             GetLayerDefn()                           */
/************************************************************************/

OGRFeatureDefn* OGRPLScenesV1Layer::GetLayerDefn()
{
    return m_poFeatureDefn;
}

/************************************************************************/
/*                       ResolveRefIfNecessary()                        */
/************************************************************************/

json_object* OGRPLScenesV1Layer::ResolveRefIfNecessary(json_object* poObj,
                                                      json_object* poMain)
{
    json_object* poRef = CPL_json_object_object_get(poObj, "$ref");
    if( poRef == NULL )
        return poObj;
    if( json_object_get_type(poRef) != json_type_string )
        return NULL;
    const char* pszRef = json_object_get_string(poRef);
    if( strncmp(pszRef, "#/", 2) != 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Cannot expand ref %s",
                 pszRef);
        return NULL;
    }
    char** papszPath = CSLTokenizeStringComplex(pszRef+2, "/", FALSE, FALSE );
    json_object* poCurNode = poMain;
    for(int i=0; papszPath != NULL && papszPath[i] != NULL; i++)
    {
        poCurNode = CPL_json_object_object_get(poCurNode, papszPath[i]);
        if( poCurNode == NULL || json_object_get_type(poCurNode) != json_type_object )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find object '%s' of '%s'",
                     papszPath[i], pszRef);
            CSLDestroy(papszPath);
            return NULL;
        }
    }
    CSLDestroy(papszPath);
    return poCurNode;
}

/************************************************************************/
/*                          RegisterField()                             */
/************************************************************************/

void OGRPLScenesV1Layer::RegisterField(OGRFieldDefn* poFieldDefn,
                                       const char* pszQueriableJSonName,
                                       const char* pszPrefixedJSonName)
{
    const int nIdx = m_poFeatureDefn->GetFieldCount();
    m_oMapPrefixedJSonFieldNameToFieldIdx[pszPrefixedJSonName] = nIdx;
    if( pszQueriableJSonName &&
        m_oSetQueriable.find(pszQueriableJSonName) != m_oSetQueriable.end() )
    {
        m_oMapFieldIdxToQueriableJSonFieldName[nIdx] = pszQueriableJSonName;
    }
    m_poFeatureDefn->AddFieldDefn(poFieldDefn);
}

/************************************************************************/
/*                         EstablishLayerDefn()                         */
/************************************************************************/

void OGRPLScenesV1Layer::EstablishLayerDefn()
{
    if( m_bFeatureDefnEstablished )
        return;
    m_bFeatureDefnEstablished = true;

    json_object* poSpec = m_poDS->RunRequest(m_osSpecURL);
    if( poSpec == NULL )
        return;

    // Check page size
    json_object* poMaximum = json_ex_get_object_by_path(poSpec, "parameters.qPageSize.maximum");
    if( poMaximum != NULL && json_object_get_type(poMaximum) == json_type_int )
    {
        int nMaximum = json_object_get_int(poMaximum);
        if( m_nPageSize > nMaximum )
        {
            CPLDebug("PLScenes", "Limiting page size to its maximum: %d", nMaximum);
            m_nPageSize = nMaximum;
            m_osRequestURL = BuildRequestURL();
        }
    }

    json_object* poPaths = CPL_json_object_object_get(poSpec, "paths");
    if( poPaths == NULL || json_object_get_type(poPaths) != json_type_object )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find paths");
        json_object_put(poSpec);
        return;
    }

/* Parse
    "paths": {
        "/catalogs/my_catalog/items/" : {
            "get": {
                "responses": {
                    "200": {
                        "schema": {
                            "$ref": "#/definitions/ItemPage"
                        }
                    }
                }
            }
        }
    }
*/
    json_object_iter it;
    it.key = NULL;
    it.val = NULL;
    it.entry = NULL;
    json_object* poItemsDef = NULL;
    json_object_object_foreachC( poPaths, it )
    {
        if( m_osItemsURL.size() >= strlen(it.key) &&
            strcmp( m_osItemsURL.c_str() + m_osItemsURL.size() - strlen(it.key), it.key ) == 0 &&
            it.val != NULL &&
            json_object_get_type(it.val) == json_type_object )
        {
            poItemsDef = it.val;
            break;
        }
    }
    if( poItemsDef == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find path for %s", m_osItemsURL.c_str());
        json_object_put(poSpec);
        return;
    }

    json_object* poSchema = json_ex_get_object_by_path(poItemsDef, "get.responses.200.schema");
    if( poSchema == NULL || json_object_get_type(poSchema) != json_type_object )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find schema for %s", m_osItemsURL.c_str());
        json_object_put(poSpec);
        return;
    }
    poSchema = ResolveRefIfNecessary(poSchema, poSpec);
    if( poSchema == NULL )
    {
        json_object_put(poSpec);
        return;
    }

/* Parse:
    "ItemPage": {
      "type": "object",
      "allOf": [
        {
          "$ref": "#/definitions/GeoJSONFeatureCollection"
        },
        {
          "required": [
            "features",
            "_links"
          ],
          "type": "object",
          "properties": {
            "_links": {
              "$ref": "#/definitions/PageLinks"
            },
            "features": {
              "items": {
                "$ref": "#/definitions/Item"
              },
              "type": "array"
            }
          }
        }
      ]
    }
*/
    json_object* poProperties = CPL_json_object_object_get(poSchema, "properties");
    if( poProperties == NULL )
    {
        json_object* poAllOf = CPL_json_object_object_get(poSchema, "allOf");
        if( poAllOf == NULL || json_object_get_type(poAllOf) != json_type_array )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot find ItemPage allOf for %s", m_osItemsURL.c_str());
            json_object_put(poSpec);
            return;
        }

        const int nAllOfSize = json_object_array_length(poAllOf);
        for(int i=0;i<nAllOfSize;i++)
        {
            json_object* poAllOfItem = json_object_array_get_idx(poAllOf, i);
            if( poAllOfItem != NULL && json_object_get_type(poAllOfItem) == json_type_object )
            {
                poProperties = CPL_json_object_object_get(poAllOfItem, "properties");
                if( poProperties != NULL )
                    break;
            }
        }
    }

    if( poProperties == NULL || json_object_get_type(poProperties) != json_type_object )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find ItemPage properties for %s", m_osItemsURL.c_str());
        json_object_put(poSpec);
        return;
    }

    json_object* poItems = json_ex_get_object_by_path(poProperties, "features.items");
    if( poItems == NULL || json_object_get_type(poItems) != json_type_object )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find ItemPage properties.features.items for %s", m_osItemsURL.c_str());
        json_object_put(poSpec);
        return;
    }
    poItems = ResolveRefIfNecessary(poItems, poSpec);
    if( poItems == NULL )
    {
        json_object_put(poSpec);
        return;
    }

    // Look for parameters (fields that can be queried)
    json_object* poParameters = json_ex_get_object_by_path(poItemsDef, "get.parameters");
    if( poParameters == NULL || json_object_get_type(poParameters) != json_type_array )
    {
        CPLDebug("PLSCENES", "No queryable parameters found");
    }
    else
    {
        const int nParameters = json_object_array_length(poParameters);
        for(int i=0;i<nParameters;i++)
        {
            json_object* poParameter = json_object_array_get_idx(poParameters, i);
            if( poParameter == NULL || json_object_get_type(poParameter) != json_type_object )
                continue;
            poParameter = ResolveRefIfNecessary(poParameter, poSpec);
            if( poParameter == NULL )
                continue;
            json_object* poName = CPL_json_object_object_get(poParameter, "name");
            if( poName == NULL || json_object_get_type(poName) != json_type_string )
                continue;
            const char* pszName = json_object_get_string(poName);
            json_object* poIn = CPL_json_object_object_get(poParameter, "in");
            if( poIn == NULL || json_object_get_type(poIn) != json_type_string )
                continue;
            const char* pszIn = json_object_get_string(poIn);
            if( !EQUAL(pszIn, "query") )
                continue;
            if( EQUAL(pszName, "_sort") )
            {
                // TODO
            }
            else
            {
                m_oSetQueriable.insert(pszName);
            }
        }
    }

/* Parse:
    "Item": {
      "type": "object",
      "allOf": [
        {
          "$ref": "#/definitions/GeoJSONFeature"
        },
        {
          "required": [
            "id",
            "properties",
            "_links"
          ],
          "type": "object",
          "properties": {
            "_embeds": {
              "$ref": "#/definitions/ItemEmbeds"
            },
            "_links": {
              "$ref": "#/definitions/ItemLinks"
            },
            "id": {
              "type": "string",
              "description": "Identifier of this Item.\n"
            },
            "properties": {
              "$ref": "#/definitions/ItemProperties"
            }
          }
        }
      ]
    },
*/

    poProperties = CPL_json_object_object_get(poItems, "properties");
    if( poProperties == NULL )
    {
        json_object* poAllOf = CPL_json_object_object_get(poItems, "allOf");
        if( poAllOf == NULL || json_object_get_type(poAllOf) != json_type_array )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot find Item allOf for %s", m_osItemsURL.c_str());
            json_object_put(poSpec);
            return;
        }

        const int nItemsAllOfSize = json_object_array_length(poAllOf);
        for(int i=0;i<nItemsAllOfSize;i++)
        {
            json_object* poAllOfItem = json_object_array_get_idx(poAllOf, i);
            if( poAllOfItem != NULL && json_object_get_type(poAllOfItem) == json_type_object )
            {
                poProperties = CPL_json_object_object_get(poAllOfItem, "properties");
                if( poProperties != NULL )
                    break;
            }
        }
    }

    if( poProperties == NULL || json_object_get_type(poProperties) != json_type_object )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Item properties for %s", m_osItemsURL.c_str());
        json_object_put(poSpec);
        return;
    }

    CPLString osPropertiesDesc = "{";

    json_object* poId = CPL_json_object_object_get(poProperties, "id");
    if( poId != NULL )
    {
        json_object_object_add(poId, "src_field", json_object_new_string("id") );
        json_object_object_add(poId, "server_queryable", json_object_new_boolean(TRUE) );

        osPropertiesDesc += "\"id\"";
        osPropertiesDesc += ":";
        osPropertiesDesc += json_object_to_json_string(poId);

        OGRFieldDefn oFieldDefn("id", OFTString);
        RegisterField(&oFieldDefn, NULL, "id");
    }

    json_object* poLinks = CPL_json_object_object_get(poProperties, "_links");
    if( poLinks != NULL && json_object_get_type(poLinks) == json_type_object )
        poLinks = ResolveRefIfNecessary(poLinks, poSpec);
    else
        poLinks = NULL;
    if( poLinks != NULL )
    {
        ParseProperties(poLinks, poSpec, osPropertiesDesc, "_links");
    }

    poProperties = CPL_json_object_object_get(poProperties, "properties");
    if( poProperties == NULL || json_object_get_type(poProperties) != json_type_object )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Item properties.properties for %s", m_osItemsURL.c_str());
        json_object_put(poSpec);
        return;
    }

    poProperties = ResolveRefIfNecessary(poProperties, poSpec);
    if( poProperties == NULL )
    {
        json_object_put(poSpec);
        return;
    }

    ParseProperties(poProperties, poSpec, osPropertiesDesc, "properties");

    // Find asset categories
    /* Parse
    "PermissionFilter": {
      "allOf": [
        {
          "$ref": "#/definitions/Filter"
        },
        {
          "properties": {
            "config": {
              "items": {
                "enum": [
                  "assets:download",
                  "assets.visual:download",
                  "assets.analytic:download"
                ],
                "type": "string"
              },
              "type": "array"
            }
          },
          "required": [
            "config"
          ],
          "type": "object"
        }
      ]
    },
    */
    json_object* poPermissionFilterAllOf = json_ex_get_object_by_path(poSpec, "definitions.PermissionFilter.allOf");
    poProperties = NULL;
    if( poPermissionFilterAllOf != NULL && json_object_get_type(poPermissionFilterAllOf) == json_type_array )
    {
        const int nItemsAllOfSize = json_object_array_length(poPermissionFilterAllOf);
        for(int i=0;i<nItemsAllOfSize;i++)
        {
            json_object* poAllOfItem = json_object_array_get_idx(poPermissionFilterAllOf, i);
            if( poAllOfItem != NULL && json_object_get_type(poAllOfItem) == json_type_object )
            {
                poProperties = CPL_json_object_object_get(poAllOfItem, "properties");
                if( poProperties != NULL )
                    break;
            }
        }
    }
    if( poProperties != NULL && json_object_get_type(poProperties) == json_type_object )
    {
        json_object* poEnum = json_ex_get_object_by_path(poProperties, "config.items.enum");
        if( poEnum != NULL && json_object_get_type(poEnum) == json_type_array )
        {
            const int nEnumCount = json_object_array_length(poEnum);
            for(int i=0;i<nEnumCount;i++)
            {
                json_object* poItem = json_object_array_get_idx(poEnum, i);
                if( poItem != NULL && json_object_get_type(poItem) == json_type_string )
                {
                    const char* pszItem = json_object_get_string(poItem);
                    const char* pszColumn = strchr(pszItem, ':');
                    if( STARTS_WITH(pszItem, "assets.") && pszColumn != NULL )
                    {
                        CPLString osAssetCategory( pszItem + strlen("assets.") );
                        osAssetCategory.resize( pszColumn - pszItem - strlen("assets.") );
                        m_aoAssetCategories.push_back(osAssetCategory);
                    }
                }
            }
        }
    }

    if( m_poDS->DoesFollowLinks() && !m_aoAssetCategories.empty() )
    {
        ParseAssetProperties( poSpec, osPropertiesDesc );
    }

    osPropertiesDesc += "}";

    // Prettify description
    json_object* poPropertiesDesc = NULL;
    if( OGRJSonParse(osPropertiesDesc, &poPropertiesDesc, false) )
    {
        osPropertiesDesc = json_object_to_json_string_ext( poPropertiesDesc, JSON_C_TO_STRING_PRETTY );
        json_object_put(poPropertiesDesc);
    }

    SetMetadataItem("FIELDS_DESCRIPTION", osPropertiesDesc.c_str());

    json_object_put(poSpec);
}

/************************************************************************/
/*                             GetMetadata()                            */
/************************************************************************/

char **OGRPLScenesV1Layer::GetMetadata( const char * pszDomain  )
{
    if( pszDomain == NULL || EQUAL(pszDomain, "") )
    {
        EstablishLayerDefn();
    }
    return OGRLayer::GetMetadata(pszDomain);
}

/************************************************************************/
/*                           GetMetadataItem()                          */
/************************************************************************/

const char *OGRPLScenesV1Layer::GetMetadataItem( const char * pszName, const char* pszDomain )
{
    if( pszDomain == NULL || EQUAL(pszDomain, "") )
    {
        EstablishLayerDefn();
    }
    return OGRLayer::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                      OGRPLScenesV1LayerGetFieldType()                */
/************************************************************************/

static OGRFieldType OGRPLScenesV1LayerGetFieldType(json_object* poObj,
                                                   const char* pszJSonFieldName )
{
    json_object* poType = CPL_json_object_object_get(poObj, "type");
    OGRFieldType eType = OFTString;
    if( poType != NULL && json_object_get_type(poType) == json_type_string )
    {
        const char* pszType = json_object_get_string(poType);
        if( EQUAL(pszType, "string") )
            eType = OFTString;
        else if( EQUAL(pszType, "number") )
            eType = OFTReal;
        else if( EQUAL(pszType, "integer") )
            eType = OFTInteger;
        else if( EQUAL(pszType, "array") )
            eType = OFTString;
        else
        {
            CPLDebug("PLSCENES", "Unknown type '%s' for '%s'",
                      pszType, pszJSonFieldName);
        }
        json_object* poFormat = CPL_json_object_object_get(poObj, "format");
        if( poFormat != NULL && json_object_get_type(poFormat) == json_type_string )
        {
            const char* pszFormat = json_object_get_string(poFormat);
            if( EQUAL(pszFormat, "date-time") )
                eType = OFTDateTime;
            else if( EQUAL(pszFormat, "int32") )
                eType = OFTInteger;
            else if( EQUAL(pszFormat, "int64") )
                eType = OFTInteger64;
            else if( EQUAL(pszFormat, "float") )
                eType = OFTReal;
            else
            {
                CPLDebug("PLSCENES", "Unknown type '%s' for '%s'",
                          pszFormat, pszJSonFieldName);
            }
        }
    }
    return eType;
}

/************************************************************************/
/*                           ParseProperties()                          */
/************************************************************************/

void OGRPLScenesV1Layer::ParseProperties(json_object* poProperties,
                                         json_object* poSpec,
                                         CPLString& osPropertiesDesc,
                                         const char* pszCategory)
{
    json_object* poAllOf = CPL_json_object_object_get(poProperties, "allOf");
    if( poAllOf != NULL && json_object_get_type(poAllOf) == json_type_array )
    {
        const int nAllOfSize = json_object_array_length(poAllOf);
        for(int i=0;i<nAllOfSize;i++)
        {
            json_object* poAllOfItem = json_object_array_get_idx(poAllOf, i);
            if( poAllOfItem != NULL && json_object_get_type(poAllOfItem) == json_type_object )
            {
                poAllOfItem = ResolveRefIfNecessary(poAllOfItem, poSpec);
                if( poAllOfItem != NULL )
                    ParseProperties(poAllOfItem, poSpec, osPropertiesDesc, pszCategory);
            }
        }
        return;
    }

    poProperties = CPL_json_object_object_get(poProperties, "properties");
    if( poProperties == NULL || json_object_get_type(poProperties) != json_type_object )
        return;

    json_object_iter it;
    it.key = NULL;
    it.val = NULL;
    it.entry = NULL;
    json_object_object_foreachC( poProperties, it )
    {
        if( it.val != NULL && json_object_get_type(it.val) == json_type_object )
        {
            const char* pszJSonFieldName = it.key;
            const char* pszOGRFieldName = pszJSonFieldName;
            if( EQUAL(pszCategory, "_links") )
            {
                if( EQUAL(pszJSonFieldName, "_self") )
                    pszOGRFieldName = "self_link";
                else if( EQUAL(pszJSonFieldName, "assets") )
                    pszOGRFieldName = "assets_link";
            }
            else if( strstr(pszOGRFieldName, "catalog::") == pszOGRFieldName &&
                m_poFeatureDefn->GetFieldIndex(pszOGRFieldName + strlen("catalog::")) < 0 )
            {
                pszOGRFieldName += strlen("catalog::");
            }

            json_object* poKey = json_object_new_string(pszOGRFieldName);
            const char* pszKeySerialized = json_object_to_json_string(poKey);
            if( osPropertiesDesc != "{" )
                osPropertiesDesc += ",";
            osPropertiesDesc += pszKeySerialized;
            osPropertiesDesc += ":";
            json_object_put(poKey);

            json_object_object_add( it.val, "src_field",
                json_object_new_string( (CPLString(pszCategory) + CPLString(".") + pszJSonFieldName).c_str() ) );

             json_object_object_add( it.val, "server_queryable",
                json_object_new_boolean( EQUAL(pszCategory, "properties") &&
                    m_oSetQueriable.find(pszJSonFieldName) != m_oSetQueriable.end() ) );

            osPropertiesDesc += json_object_to_json_string(it.val);

            const OGRFieldType eType =
                  OGRPLScenesV1LayerGetFieldType(it.val, pszJSonFieldName);
            OGRFieldDefn oFieldDefn(pszOGRFieldName, eType);
            RegisterField(&oFieldDefn,
                          EQUAL(pszCategory, "_links") ? NULL : pszJSonFieldName,
                          (CPLString(pszCategory) + CPLString(".") + pszJSonFieldName).c_str());
        }
    }
}

/************************************************************************/
/*                       ParseAssetProperties()                         */
/************************************************************************/

void OGRPLScenesV1Layer::ParseAssetProperties(json_object* poSpec,
                                              CPLString& osPropertiesDesc)
{
    /* Parse
    "Asset": {
      "properties": {
        "_links": {
          "$ref": "#/definitions/SelfLink"
        },
        "_permissions": {
          "items": {
            "enum": [
              "download"
            ],
            "type": "string"
          },
          "type": "array",
          "uniqueItems": true
        },
        "files": {
          "additionalProperties": {
            "$ref": "#/definitions/AssetFile"
          },
          "description": "Various AssetFiles indicating how a user may download the image data associated with this Asset. The keys of this object reflect the type of each available AssetFile.",
          "type": "object"
        },
        "mimetype": {
          "description": "The MIME type of the underlying asset file.",
          "type": "string"
        },
        "type": {
          "description": "Type identifier of this Asset.",
          "type": "string"
        }
      },
      "required": [
        "type",
        "mimetype",
        "files",
        "_links",
        "_permissions"
      ],
      "type": "object"
    },
    */

    json_object* poProperties = json_ex_get_object_by_path(poSpec, "definitions.Asset.properties");
    if( poProperties == NULL || json_object_get_type(poProperties) != json_type_object )
        return;

    json_object* poPropertiesAssetFile = json_ex_get_object_by_path(poSpec, "definitions.AssetFile.properties");
    if( poPropertiesAssetFile != NULL && json_object_get_type(poPropertiesAssetFile) != json_type_object )
    {
        poPropertiesAssetFile = NULL;
    }

    bool bFoundLinks = false;
    for(size_t i = 0; i < m_aoAssetCategories.size(); i++ )
    {
        json_object_iter it;
        it.key = NULL;
        it.val = NULL;
        it.entry = NULL;
        json_object_object_foreachC( poProperties, it )
        {
            if( it.val != NULL && json_object_get_type(it.val) == json_type_object )
            {
                const char* pszJSonFieldName = it.key;
                if( strcmp(pszJSonFieldName, "type") == 0 )
                    continue; // type is contained in the field name
                if( strcmp(pszJSonFieldName, "files") == 0 && poPropertiesAssetFile != NULL )
                {
                    ProcessAssetFileProperties( poPropertiesAssetFile, m_aoAssetCategories[i], osPropertiesDesc );
                    continue;
                }

                const char* pszOGRFieldName = NULL;
                CPLString osSrcField(CPLString("/assets.") + m_aoAssetCategories[i] + CPLString("."));
                json_object* poLinksRef = NULL;
                if( EQUAL(pszJSonFieldName, "_links") &&
                    (bFoundLinks ||
                     ((poLinksRef = CPL_json_object_object_get( it.val, "$ref" )) != NULL &&
                     json_object_get_type(poLinksRef) == json_type_string &&
                     strcmp(json_object_get_string(poLinksRef), "#/definitions/SelfLink") == 0)) )
                {
                    bFoundLinks = true;
                    pszOGRFieldName = CPLSPrintf("asset_%s_self_link", m_aoAssetCategories[i].c_str());
                    osSrcField += "_links._self";
                }
                else
                {
                    if( EQUAL( pszJSonFieldName, "_permissions") )
                        pszOGRFieldName = CPLSPrintf("asset_%s_permissions", m_aoAssetCategories[i].c_str());
                    else
                        pszOGRFieldName = CPLSPrintf("asset_%s_%s", m_aoAssetCategories[i].c_str(), pszJSonFieldName);
                    osSrcField += pszJSonFieldName;
                }
                json_object* poKey = json_object_new_string(pszOGRFieldName);
                const char* pszKeySerialized = json_object_to_json_string(poKey);
                if( osPropertiesDesc != "{" )
                    osPropertiesDesc += ",";
                osPropertiesDesc += pszKeySerialized;
                osPropertiesDesc += ":";
                json_object_put(poKey);

                if( EQUAL(pszJSonFieldName, "_links") && bFoundLinks )
                {
                    json_object_object_del( it.val, "$ref" );
                    json_object_object_add( it.val, "description",
                            json_object_new_string("RFC 3986 URI representing the canonical location of this asset.") );
                    json_object_object_add( it.val, "type", json_object_new_string("string") );
                }

                json_object_object_add( it.val, "src_field",
                    json_object_new_string( osSrcField.c_str() ) );

                json_object_object_add( it.val, "server_queryable",
                                        json_object_new_boolean( FALSE ) );

                osPropertiesDesc += json_object_to_json_string(it.val);

                const OGRFieldType eType =
                  OGRPLScenesV1LayerGetFieldType(it.val, pszJSonFieldName);
                OGRFieldDefn oFieldDefn(pszOGRFieldName, eType);
                RegisterField(&oFieldDefn,
                              NULL,
                              osSrcField.c_str());
            }
        }
    }
}

/************************************************************************/
/*                       ProcessAssetFileProperties()                   */
/************************************************************************/

void OGRPLScenesV1Layer::ProcessAssetFileProperties( json_object* poPropertiesAssetFile,
                                                     const CPLString& osAssetCategory,
                                                     CPLString& osPropertiesDesc )
{
    /* Parse
        "AssetFile": {
      "description": "An AssetFile describes the means of downloading the image data associated with a specific Asset",
      "properties": {
        "_links": {
          "properties": {
            "activate": {
              "description": "If present, RFC 3986 URI indicating where an authenticated user may trigger activation of this AssetFile via a POST request. A 202 response indicates the activation request has been accepted. A 204 response indicates the AssetFile is already active. After successful activation, this AssetFile will have a non-empty location.",
              "type": "string"
            }
          },
          "type": "object"
        },
        "expires_at": {
          "description": "If present, RFC 3339 timestamp indicating when this AssetFile will become inactive and will require reactivation.",
          "format": "date-time",
          "type": "string"
        },
        "location": {
          "description": "If present, RFC 3986 URI that indicates a location that will yield image data. Consult the documentation of the AssetFile type to understand how to use this URI.",
          "type": "string"
        },
        "status": {
          "description": "Current status of the AssetFile. \"inactive\" indicates that the AssetFile is not currently available for download, but may be after activation. \"activating\" indicates the AssetFile is currently undergoing activation, and may be available for download shortly. \"active\" indicates the AssetFile has been activated, and may currently be available for download if the authentication context permits.",
          "enum": [
            "inactive",
            "activating",
            "active"
          ],
          "type": "string"
        },
        "type": {
          "description": "An identifier of the methodology that must be used to download the image data from the indicated location. In the case of \"http\", the user must make an HTTP GET request against the provided URL.",
          "enum": [
            "http"
          ],
          "type": "string"
        }
      },
      "required": [
        "type",
        "status",
        "_links"
      ],
      "type": "object"
    },
    */
    json_object_iter it;
    it.key = NULL;
    it.val = NULL;
    it.entry = NULL;
    json_object_object_foreachC( poPropertiesAssetFile, it )
    {
        if( it.val != NULL && json_object_get_type(it.val) == json_type_object )
        {
            const char* pszJSonFieldName = it.key;
            json_object* poFieldObj = it.val;
            if( strcmp(pszJSonFieldName, "type") == 0 )
                continue; // "http" not really interesting

            const char* pszOGRFieldName = NULL;
            CPLString osSrcField(CPLString("/assets.") + osAssetCategory + CPLString(".files."));
            if( EQUAL(pszJSonFieldName, "_links") )
            {
                poFieldObj = json_ex_get_object_by_path(poFieldObj, "properties.activate");
                if( poFieldObj == NULL )
                    continue;
                pszOGRFieldName = CPLSPrintf("asset_%s_activate_link", osAssetCategory.c_str());
                osSrcField += "_links.activate";
            }
            else
            {
                if( EQUAL(pszJSonFieldName, "location") )
                    pszOGRFieldName = CPLSPrintf("asset_%s_product_link", osAssetCategory.c_str());
                else if( EQUAL(pszJSonFieldName, "status") )
                    pszOGRFieldName = CPLSPrintf("asset_%s_product_link_status", osAssetCategory.c_str());
                else
                    pszOGRFieldName = CPLSPrintf("asset_%s_%s", osAssetCategory.c_str(), pszJSonFieldName);
                osSrcField += pszJSonFieldName;
            }
            json_object* poKey = json_object_new_string(pszOGRFieldName);
            const char* pszKeySerialized = json_object_to_json_string(poKey);
            if( osPropertiesDesc != "{" )
                osPropertiesDesc += ",";
            osPropertiesDesc += pszKeySerialized;
            osPropertiesDesc += ":";
            json_object_put(poKey);

            json_object_object_add( poFieldObj, "src_field",
                json_object_new_string( osSrcField.c_str() ) );

            json_object_object_add( poFieldObj, "server_queryable",
                                    json_object_new_boolean( FALSE ) );

            osPropertiesDesc += json_object_to_json_string(poFieldObj);

            const OGRFieldType eType =
              OGRPLScenesV1LayerGetFieldType(poFieldObj, pszJSonFieldName);
            OGRFieldDefn oFieldDefn(pszOGRFieldName, eType);
            RegisterField(&oFieldDefn,
                          NULL,
                          osSrcField.c_str());
        }
    }
}

/************************************************************************/
/*                              GetNextPage()                           */
/************************************************************************/

bool OGRPLScenesV1Layer::GetNextPage()
{
    if( m_poPageObj != NULL )
        json_object_put(m_poPageObj);
    m_poPageObj = NULL;
    m_poFeatures = NULL;
    m_nFeatureIdx = 0;

    if( m_osRequestURL.empty() )
    {
        m_bEOF = true;
        return false;
    }

    json_object* poObj =
        m_osRequestURL == m_poDS->GetBaseURL() + GetName() + "/quick-search"
        ? m_poDS->RunRequest(m_osRequestURL, FALSE, "POST", true,
                             m_poDS->GetFilter())
        : m_poDS->RunRequest(m_osRequestURL);
    if( poObj == NULL )
    {
        m_bEOF = true;
        return false;
    }

    json_object* poFeatures = CPL_json_object_object_get(poObj, "features");
    if( poFeatures == NULL ||
        json_object_get_type(poFeatures) != json_type_array ||
        json_object_array_length(poFeatures) == 0 )
    {
        // If this is a single item, then wrap it in a features array
        json_object* poProperties = CPL_json_object_object_get(poObj, "properties");
        if( poProperties != NULL )
        {
            m_poPageObj = json_object_new_object();
            poFeatures = json_object_new_array();
            json_object_array_add(poFeatures, poObj);
            json_object_object_add(m_poPageObj, "features", poFeatures);
            poObj = m_poPageObj;
        }
        else
        {
            json_object_put(poObj);
            m_bEOF = true;
            return false;
        }
    }

    m_poPageObj = poObj;
    m_poFeatures = poFeatures;

    // Get URL of next page
    m_osNextURL = "";
    json_object* poLinks = CPL_json_object_object_get(poObj, "_links");
    if( poLinks && json_object_get_type(poLinks) == json_type_object )
    {
        json_object* poNext = CPL_json_object_object_get(poLinks, "_next");
        if( poNext && json_object_get_type(poNext) == json_type_string )
        {
            m_osNextURL = json_object_get_string(poNext);
        }
    }

    return true;
}

/************************************************************************/
/*                             ResetReading()                           */
/************************************************************************/

void OGRPLScenesV1Layer::ResetReading()
{
    m_bEOF = false;

    if( m_poFeatures != NULL && m_bStillInFirstPage )
        m_nFeatureIdx = 0;
    else
        m_poFeatures = NULL;
    m_nNextFID = 1;
    m_bStillInFirstPage = true;
    m_osRequestURL = BuildRequestURL();
}

/************************************************************************/
/*                          BuildRequestURL()                           */
/************************************************************************/

CPLString OGRPLScenesV1Layer::BuildRequestURL()
{
    const CPLString& osFilter = m_poDS->GetFilter();
    if( !osFilter.empty() && osFilter[0] == '{' && osFilter.back() == '}' )
    {
        // Quick search
        return m_poDS->GetBaseURL() + GetName() + "/quick-search";
    }

    CPLString osURL = m_osItemsURL;
    osURL += CPLSPrintf("?_page_size=%d", m_nPageSize);
    if( !osFilter.empty() )
        osURL += "&" + osFilter;

    if( m_poFilterGeom != NULL )
    {
        OGREnvelope sEnvelope;
        m_poFilterGeom->getEnvelope(&sEnvelope);
        if( !(sEnvelope.MinX <= -180 && sEnvelope.MinY <= -90 &&
              sEnvelope.MaxX >= 180 && sEnvelope.MaxY >= 90) )
        {
            char* pszWKT = NULL;
            if( sEnvelope.MinX == sEnvelope.MaxX && sEnvelope.MinY == sEnvelope.MaxY )
            {
                pszWKT = CPLStrdup(CPLSPrintf("POINT(%.18g %.18g)",
                                            sEnvelope.MinX, sEnvelope.MinY));
            }
            else
                m_poFilterGeom->exportToWkt(&pszWKT);

            osURL += "&geometry=";
            char* pszWKTEscaped = CPLEscapeString(pszWKT, -1, CPLES_URL);
            osURL += pszWKTEscaped;
            CPLFree(pszWKTEscaped);
            CPLFree(pszWKT);
        }
    }

    if( !m_osFilterURLPart.empty() )
    {
        if( m_osFilterURLPart[0] == '&' )
            osURL += m_osFilterURLPart;
        else
            osURL = m_osItemsURL + m_osFilterURLPart;
    }

    return osURL;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRPLScenesV1Layer::SetSpatialFilter(  OGRGeometry *poGeomIn )
{
    m_poFeatures = NULL;

    if( poGeomIn )
    {
        OGREnvelope sEnvelope;
        poGeomIn->getEnvelope(&sEnvelope);
        if( sEnvelope.MinX == sEnvelope.MaxX && sEnvelope.MinY == sEnvelope.MaxY )
        {
            OGRPoint p(sEnvelope.MinX, sEnvelope.MinY);
            InstallFilter(&p);
        }
        else
            InstallFilter( poGeomIn );
    }
    else
        InstallFilter( poGeomIn );

    ResetReading();
}

/************************************************************************/
/*                          FlattendAndOperands()                       */
/************************************************************************/

/*((a AND (b OR c)) AND d) --> [a, b OR c, d] */

void OGRPLScenesV1Layer::FlattendAndOperands(swq_expr_node* poNode,
                                             std::vector<swq_expr_node*>& oVector)
{
    if( poNode->eNodeType == SNT_OPERATION && poNode->nOperation == SWQ_AND &&
        poNode->nSubExprCount == 2 )
    {
        FlattendAndOperands(poNode->papoSubExpr[0], oVector);
        FlattendAndOperands(poNode->papoSubExpr[1], oVector);
    }
    else
    {
        oVector.push_back(poNode);
    }
}

/************************************************************************/
/*                      OGRPLScenesV1ParseDateTime()                    */
/************************************************************************/

static bool OGRPLScenesV1ParseDateTime(const char* pszValue,
                                       int& nYear, int &nMonth, int &nDay,
                                       int& nHour, int &nMinute, int &nSecond)
{
    return ( sscanf(pszValue,"%04d/%02d/%02d %02d:%02d:%02d",
                    &nYear, &nMonth, &nDay, &nHour, &nMinute, &nSecond) >= 3 ||
             sscanf(pszValue,"%04d-%02d-%02dT%02d:%02d:%02d",
                    &nYear, &nMonth, &nDay, &nHour, &nMinute, &nSecond) >= 3 );
}

/************************************************************************/
/*                    OGRPLScenesV1LayerFormatValue()                   */
/************************************************************************/

static const char* OGRPLScenesV1LayerFormatValue(const swq_expr_node* poNode)
{
    if (poNode->field_type == SWQ_FLOAT)
        return CPLSPrintf("%.8f", poNode->float_value);
    else if (poNode->field_type == SWQ_INTEGER)
        return CPLSPrintf(CPL_FRMT_GIB, poNode->int_value);
    else if (poNode->field_type == SWQ_STRING)
        return poNode->string_value;
    else if (poNode->field_type == SWQ_TIMESTAMP)
    {
        int nYear = 0, nMonth = 0, nDay = 0, nHour = 0, nMinute = 0, nSecond = 0;
        if( OGRPLScenesV1ParseDateTime(poNode->string_value,
                    nYear, nMonth, nDay, nHour, nMinute, nSecond)  )
        {
            return CPLSPrintf("%04d-%02d-%02dT%02d:%02d:%02dZ",
                                    nYear, nMonth, nDay, nHour, nMinute, nSecond);
        }
        else
        {
            return poNode->string_value;
        }
    }
    else
        return ""; /* should not happen */
}

/************************************************************************/
/*                          IsSimpleComparison()                        */
/************************************************************************/

bool OGRPLScenesV1Layer::IsSimpleComparison(const swq_expr_node* poNode)
{
    return  poNode->eNodeType == SNT_OPERATION &&
            (poNode->nOperation == SWQ_EQ ||
             poNode->nOperation == SWQ_LT ||
             poNode->nOperation == SWQ_LE ||
             poNode->nOperation == SWQ_GT ||
             poNode->nOperation == SWQ_GE) &&
             poNode->nSubExprCount == 2 &&
             poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
             poNode->papoSubExpr[1]->eNodeType == SNT_CONSTANT &&
             m_oMapFieldIdxToQueriableJSonFieldName.find(poNode->papoSubExpr[0]->field_index) !=
                                m_oMapFieldIdxToQueriableJSonFieldName.end();
}

/************************************************************************/
/*                  OGRPLScenesV1LayerExprComparator()                  */
/************************************************************************/

/* We want to have things sorted like :
    varA >= xxx AND varA <= yyy AND varB >= zzz AND varB <= ttt AND (other_expr_not_server_side_compatible)
    so as to be able to generate a filter like:
    varA=[xxx:yyy]&varB=[zzz:ttt]
*/

struct OGRPLScenesV1LayerExprComparator
{
    OGRPLScenesV1Layer* m_poLayer;

    explicit OGRPLScenesV1LayerExprComparator(OGRPLScenesV1Layer* poLayer) :
                m_poLayer(poLayer) {}

    bool operator() (const swq_expr_node* poNode1,
                                       const swq_expr_node* poNode2)
    {
        const bool bIsSimpleComparison1 = m_poLayer->IsSimpleComparison(poNode1);
        const bool bIsSimpleComparison2 = m_poLayer->IsSimpleComparison(poNode2);

        if( bIsSimpleComparison1 && bIsSimpleComparison2 )
        {
            if( poNode1->papoSubExpr[0]->field_index == poNode2->papoSubExpr[0]->field_index )
            {
                return (poNode1->nOperation == SWQ_GT || poNode1->nOperation == SWQ_GE) &&
                       (poNode2->nOperation == SWQ_LT || poNode2->nOperation == SWQ_LE);
            }
            return poNode1->papoSubExpr[0]->field_index <
                                            poNode2->papoSubExpr[0]->field_index;
        }
        else if( bIsSimpleComparison1 )
            return true;
        else
            return false;
    }
};

/************************************************************************/
/*                             BuildFilter()                            */
/************************************************************************/

CPLString OGRPLScenesV1Layer::BuildFilter(swq_expr_node* poNode)
{
    std::vector<swq_expr_node*> oVector;
    FlattendAndOperands(poNode, oVector);
    OGRPLScenesV1LayerExprComparator oComparator(this);
    std::sort(oVector.begin(), oVector.end(), oComparator);

    bool bOnlyServerSide = true;
    CPLString osFilter = "";
    for(size_t i=0; i<oVector.size(); i++)
    {
        if( !IsSimpleComparison( oVector[i] ) )
        {
            bOnlyServerSide = false;
            // We break since that, given the sorting, there can not be
            // a following simple comparison
            break;
        }

        const int iFieldIdx = oVector[i]->papoSubExpr[0]->field_index;
        const CPLString& osJSonName = m_oMapFieldIdxToQueriableJSonFieldName[iFieldIdx];

        if( i+1 < oVector.size() && IsSimpleComparison( oVector[i+1] ) &&
            iFieldIdx == oVector[i+1]->papoSubExpr[0]->field_index )
        {
            const int nOp1 = oVector[i]->nOperation;
            const int nOp2 = oVector[i+1]->nOperation;
            if( !((nOp1 == SWQ_GT || nOp1 == SWQ_GE ) && (nOp2 == SWQ_LT || nOp2 == SWQ_LE)) )
            {
                CPLDebug("PLSCENES", "Field %s used but not with >/>= AND </<= comparisons",
                            osJSonName.c_str());
                bOnlyServerSide = false;
                continue;
            }
            if( i + 2 < oVector.size() && iFieldIdx == oVector[i+2]->papoSubExpr[0]->field_index )
            {
                CPLDebug("PLSCENES", "Field %s used more than twice in same expression",
                            osJSonName.c_str());
                bOnlyServerSide = false;
                continue;
            }

            if( !osFilter.empty() )
                osFilter += "&";

            osFilter += osJSonName;
            osFilter += "=[";
            osFilter += OGRPLScenesV1LayerFormatValue(oVector[i]->papoSubExpr[1]);
            osFilter += ":";
            osFilter += OGRPLScenesV1LayerFormatValue(oVector[i+1]->papoSubExpr[1]);
            osFilter += "]";

            i++;
        }
        else if( oVector[i]->nOperation == SWQ_EQ )
        {
            if( !osFilter.empty() )
                osFilter += "&";

            osFilter += osJSonName;
            osFilter += "=";

            int nYear = 0, nMonth = 0, nDay = 0, nHour = 0, nMinute = 0, nSecond = 0;
            if (oVector[i]->papoSubExpr[1]->field_type == SWQ_TIMESTAMP &&
                OGRPLScenesV1ParseDateTime(oVector[i]->papoSubExpr[1]->string_value,
                    nYear, nMonth, nDay, nHour, nMinute, nSecond)  )
            {
                osFilter += "[";
                osFilter += OGRPLScenesV1LayerFormatValue(oVector[i]->papoSubExpr[1]);
                osFilter += ":";
                nSecond ++;
                if( nSecond == 60 ) { nSecond = 0; nMinute ++; }
                if( nMinute == 60 ) { nMinute = 0; nHour ++; }
                if( nHour == 24 ) { nHour = 0; nDay ++; }
                osFilter += CPLSPrintf("%04d-%02d-%02dT%02d:%02d:%02dZ",
                                        nYear, nMonth, nDay, nHour, nMinute, nSecond);
                osFilter += "]";
            }
            else
            {
                osFilter += OGRPLScenesV1LayerFormatValue(oVector[i]->papoSubExpr[1]);
            }
        }
        else if( oVector[i]->nOperation == SWQ_GT || oVector[i]->nOperation == SWQ_GE )
        {
            if( !osFilter.empty() )
                osFilter += "&";

            osFilter += osJSonName;
            osFilter += "=[";
            osFilter += OGRPLScenesV1LayerFormatValue(oVector[i]->papoSubExpr[1]);
            osFilter += ":]";
        }
        else if( oVector[i]->nOperation == SWQ_LT || oVector[i]->nOperation == SWQ_LE )
        {
            if( !osFilter.empty() )
                osFilter += "&";

            osFilter += osJSonName;
            osFilter += "=[:";
            osFilter += OGRPLScenesV1LayerFormatValue(oVector[i]->papoSubExpr[1]);
            osFilter += "]";
        }
        else
        {
            CPLDebug("PLSCENES", "Should not happen");
            bOnlyServerSide = false;
            osFilter = "";
            break;
        }
    }

    if( osFilter.empty() && !m_bFilterMustBeClientSideEvaluated )
    {
        m_bFilterMustBeClientSideEvaluated = true;
        CPLDebug("PLSCENES",
                 "Full filter will be evaluated on client side.");
    }
    else if( !bOnlyServerSide && !m_bFilterMustBeClientSideEvaluated )
    {
        m_bFilterMustBeClientSideEvaluated = true;
        CPLDebug("PLSCENES",
                 "Only part of the filter will be evaluated on server side.");
    }

    return osFilter;
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRPLScenesV1Layer::SetAttributeFilter( const char *pszQuery )

{
    m_poFeatures = NULL;

    OGRErr eErr = OGRLayer::SetAttributeFilter(pszQuery);

    m_osFilterURLPart = "";
    m_bFilterMustBeClientSideEvaluated = false;
    if( m_poAttrQuery != NULL )
    {
        swq_expr_node* poNode = (swq_expr_node*) m_poAttrQuery->GetSWQExpr();

        poNode->ReplaceBetweenByGEAndLERecurse();

        if( poNode->eNodeType == SNT_OPERATION &&
            poNode->nOperation == SWQ_EQ && poNode->nSubExprCount == 2 &&
            poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
            poNode->papoSubExpr[0]->field_index == m_poFeatureDefn->GetFieldIndex("id") &&
            poNode->papoSubExpr[1]->eNodeType == SNT_CONSTANT &&
            poNode->papoSubExpr[1]->field_type == SWQ_STRING )
        {
            m_osFilterURLPart = poNode->papoSubExpr[1]->string_value;
        }
        else
        {
            CPLString osFilter = BuildFilter(poNode);
            if( !osFilter.empty() )
            {
                m_osFilterURLPart = "&";
                m_osFilterURLPart += osFilter;
            }
        }
    }

    ResetReading();

    return eErr;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRPLScenesV1Layer::GetNextFeature()
{
    while( true )
    {
        OGRFeature  *poFeature = GetNextRawFeature();
        if (poFeature == NULL)
            return NULL;

        if((m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeometryRef() ) )
        && (m_poAttrQuery == NULL
            || m_poAttrQuery->Evaluate( poFeature )) )
        {
            return poFeature;
        }
        else
            delete poFeature;
    }
}

/************************************************************************/
/*                            GetNextRawFeature()                       */
/************************************************************************/

OGRFeature* OGRPLScenesV1Layer::GetNextRawFeature()
{
    EstablishLayerDefn();
    if( m_bEOF )
        return NULL;

    if( m_poFeatures == NULL )
    {
        if( !GetNextPage() )
            return NULL;
    }

    if( m_nFeatureIdx == json_object_array_length(m_poFeatures) )
    {
        m_osRequestURL = m_osNextURL;
        m_bStillInFirstPage = false;
        if( !GetNextPage() )
            return NULL;
    }
    json_object* poJSonFeature = json_object_array_get_idx(m_poFeatures, m_nFeatureIdx);
    m_nFeatureIdx ++;
    if( poJSonFeature == NULL || json_object_get_type(poJSonFeature) != json_type_object )
    {
        m_bEOF = true;
        return NULL;
    }

    OGRFeature* poFeature = new OGRFeature(m_poFeatureDefn);
    poFeature->SetFID(m_nNextFID++);

    json_object* poJSonGeom = CPL_json_object_object_get(poJSonFeature, "geometry");
    if( poJSonGeom != NULL && json_object_get_type(poJSonGeom) == json_type_object )
    {
        OGRGeometry* poGeom = OGRGeoJSONReadGeometry(poJSonGeom);
        if( poGeom != NULL )
        {
            if( poGeom->getGeometryType() == wkbPolygon )
            {
                OGRMultiPolygon* poMP = new OGRMultiPolygon();
                poMP->addGeometryDirectly(poGeom);
                poGeom = poMP;
            }
            poGeom->assignSpatialReference(m_poSRS);
            poFeature->SetGeometryDirectly(poGeom);
        }
    }

    json_object* poId = CPL_json_object_object_get(poJSonFeature, "id");
    if( poId != NULL && json_object_get_type(poId) == json_type_string )
    {
        std::map<CPLString, int>::iterator oIter = m_oMapPrefixedJSonFieldNameToFieldIdx.find("id");
        if( oIter != m_oMapPrefixedJSonFieldNameToFieldIdx.end() )
        {
            const int iField = oIter->second;
            poFeature->SetField(iField, json_object_get_string(poId));
        }
    }

    for(int i=0;i<2;i++)
    {
        const char* pszFeaturePart = (i == 0) ? "properties": "_links";
        json_object* poProperties = CPL_json_object_object_get(poJSonFeature, pszFeaturePart);
        if( poProperties != NULL && json_object_get_type(poProperties) == json_type_object )
        {
            json_object_iter it;
            it.key = NULL;
            it.val = NULL;
            it.entry = NULL;
            json_object_object_foreachC( poProperties, it )
            {
                SetFieldFromPrefixedJSonFieldName(
                    poFeature, CPLString(pszFeaturePart) + CPLString(".") + it.key,
                    it.val);
            }
        }
    }

    json_object* poAssets = NULL;
    if( m_poDS->DoesFollowLinks() && (!m_bInFeatureCountOrGetExtent || m_poAttrQuery != NULL)  )
    {
        std::map<CPLString, int>::iterator oIter = m_oMapPrefixedJSonFieldNameToFieldIdx.find(
                                                                                  "_links.assets");
        if( oIter != m_oMapPrefixedJSonFieldNameToFieldIdx.end() )
        {
            const int iField = oIter->second;
            if( poFeature->IsFieldSetAndNotNull( iField ) )
            {
                const char* pszAssetURL = poFeature->GetFieldAsString( iField );
                poAssets = m_poDS->RunRequest(pszAssetURL);
            }
        }
    }
    if( poAssets != NULL )
    {
        json_object_iter itAsset;
        itAsset.key = NULL;
        itAsset.val = NULL;
        itAsset.entry = NULL;
        json_object_object_foreachC( poAssets, itAsset )
        {
            json_object* poAsset = itAsset.val;
            if( poAsset != NULL && json_object_get_type(poAsset) == json_type_object )
            {
                json_object_iter it;
                it.key = NULL;
                it.val = NULL;
                it.entry = NULL;
                json_object_object_foreachC( poAsset, it )
                {
                    if( it.val == NULL ) continue;
                    CPLString osPrefixedJSonFieldName("/assets." + CPLString(itAsset.key));
                    osPrefixedJSonFieldName += "." + CPLString(it.key);
                    json_object* poHTTP = NULL;
                    if( strcmp(it.key, "_links") == 0 &&
                        json_object_get_type(it.val) == json_type_object &&
                        CPL_json_object_object_get(it.val, "_self") != NULL )
                    {
                        osPrefixedJSonFieldName += "._self";
                        SetFieldFromPrefixedJSonFieldName(
                            poFeature, osPrefixedJSonFieldName, CPL_json_object_object_get(it.val, "_self"));
                    }
                    else if( strcmp(it.key, "files") == 0 &&
                             json_object_get_type(it.val) == json_type_object &&
                             (poHTTP = CPL_json_object_object_get(it.val, "http")) != NULL &&
                             json_object_get_type(poHTTP) == json_type_object )
                    {
                        json_object_iter itFiles;
                        itFiles.key = NULL;
                        itFiles.val = NULL;
                        itFiles.entry = NULL;
                        json_object_object_foreachC( poHTTP, itFiles )
                        {
                            json_object* poActivate = NULL;
                            if( strcmp(itFiles.key, "_links") == 0 &&
                                json_object_get_type(itFiles.val) == json_type_object &&
                                (poActivate = CPL_json_object_object_get(itFiles.val, "activate")) != NULL )
                            {
                                SetFieldFromPrefixedJSonFieldName(
                                  poFeature, osPrefixedJSonFieldName + "._links.activate", poActivate);
                            }
                            else
                            {
                                SetFieldFromPrefixedJSonFieldName(
                                    poFeature, osPrefixedJSonFieldName + "." + CPLString(itFiles.key), itFiles.val);
                            }
                        }
                    }
                    else
                    {
                        SetFieldFromPrefixedJSonFieldName(
                            poFeature, osPrefixedJSonFieldName, it.val);
                    }
                }
            }
        }
        json_object_put(poAssets);
    }

    return poFeature;
}

/************************************************************************/
/*                    SetFieldFromPrefixedJSonFieldName()               */
/************************************************************************/

void OGRPLScenesV1Layer::SetFieldFromPrefixedJSonFieldName(
                                  OGRFeature* poFeature,
                                  const CPLString& osPrefixedJSonFieldName,
                                  json_object* poVal )
{
    std::map<CPLString, int>::iterator oIter = m_oMapPrefixedJSonFieldNameToFieldIdx.find(
            osPrefixedJSonFieldName);
    if( poVal != NULL && oIter != m_oMapPrefixedJSonFieldNameToFieldIdx.end() )
    {
        const int iField = oIter->second;
        json_type eJSonType = json_object_get_type(poVal);
        if( eJSonType == json_type_int )
        {
            poFeature->SetField(iField,
                    static_cast<GIntBig>(json_object_get_int64(poVal)));
        }
        else if( eJSonType == json_type_double )
        {
            poFeature->SetField(iField, json_object_get_double(poVal));
        }
        else if( eJSonType == json_type_string )
        {
            poFeature->SetField(iField, json_object_get_string(poVal));
        }
        else
        {
            poFeature->SetField(iField, json_object_to_json_string_ext( poVal, 0 ));
        }
    }
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRPLScenesV1Layer::GetFeatureCount(int bForce)
{
    if( m_nTotalFeatures > 0 && m_poFilterGeom == NULL && m_poAttrQuery == NULL &&
        m_osRequestURL != m_poDS->GetBaseURL() + GetName() + "/quick-search" )
    {
        return m_nTotalFeatures;
    }

    m_bInFeatureCountOrGetExtent = true;
    GIntBig nRes = OGRLayer::GetFeatureCount(bForce);
    m_bInFeatureCountOrGetExtent = false;
    return nRes;
}

/************************************************************************/
/*                                GetExtent()                           */
/************************************************************************/

OGRErr OGRPLScenesV1Layer::GetExtent( OGREnvelope *psExtent, int bForce )
{
    if( m_poFilterGeom != NULL ||
        ( m_nTotalFeatures > 0 && m_poFeatures != NULL &&
          m_bStillInFirstPage &&
          json_object_array_length(m_poFeatures) < m_nTotalFeatures) )
    {
        m_bInFeatureCountOrGetExtent = true;
        OGRErr eErr = OGRLayer::GetExtentInternal(0, psExtent, bForce);
        m_bInFeatureCountOrGetExtent = false;
        return eErr;
    }

    psExtent->MinX = -180;
    psExtent->MinY = -90;
    psExtent->MaxX = 180;
    psExtent->MaxY = 90;
    return OGRERR_NONE;
}

/************************************************************************/
/*                              TestCapability()                        */
/************************************************************************/

int OGRPLScenesV1Layer::TestCapability(const char* pszCap)
{
    if( EQUAL(pszCap, OLCFastFeatureCount) )
        return !m_bFilterMustBeClientSideEvaluated;
    if( EQUAL(pszCap, OLCStringsAsUTF8) )
        return TRUE;
    return FALSE;
}
