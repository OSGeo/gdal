/******************************************************************************
 * $Id$
 *
 * Project:  ElasticSearch Translator
 * Purpose:
 * Author:
 *
 ******************************************************************************
 * Copyright (c) 2011, Adam Estrada
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

#include "ogr_elastic.h"
#include "cpl_conv.h"
#include "cpl_minixml.h"
#include "ogr_api.h"
#include "ogr_p.h"
#include <jsonc/json.h> // JSON-C

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRElasticLayer()                          */
/************************************************************************/

OGRElasticLayer::OGRElasticLayer(const char* pszFilename,
        const char* pszLayerName,
        OGRElasticDataSource* poDS,
        OGRSpatialReference *poSRSIn,
        int bWriteMode) {
    this->pszLayerName = CPLStrdup(pszLayerName);
    this->poDS = poDS;
    this->pAttributes = NULL;

    // If we are overwriting, then delete the current index if it exists
    if (poDS->bOverwrite) {
        poDS->DeleteIndex(CPLSPrintf("%s/%s", poDS->GetName(), pszLayerName));
    }

    // Create the index
    poDS->UploadFile(CPLSPrintf("%s/%s", poDS->GetName(), pszLayerName), "");

    // If we have a user specified mapping, then go ahead and update it now
    if (poDS->pszMapping != NULL) {
        poDS->UploadFile(CPLSPrintf("%s/%s/FeatureCollection/_mapping", poDS->GetName(), pszLayerName),
                poDS->pszMapping);
    }

    poFeatureDefn = new OGRFeatureDefn(pszLayerName);
    poFeatureDefn->Reference();

    poSRS = poSRSIn;
    if (poSRS)
        poSRS->Reference();

    ResetReading();
    return;
}

/************************************************************************/
/*                         ~OGRElasticLayer()                           */
/************************************************************************/

OGRElasticLayer::~OGRElasticLayer() {
    PushIndex();

    CPLFree(pszLayerName);

    poFeatureDefn->Release();

    if (poSRS != NULL)
        poSRS->Release();
}


/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn * OGRElasticLayer::GetLayerDefn() {
    return poFeatureDefn;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRElasticLayer::ResetReading() {
    return;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRElasticLayer::GetNextFeature() {
    CPLError(CE_Failure, CPLE_NotSupported,
            "Cannot read features when writing a Elastic file");
    return NULL;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRElasticLayer::GetSpatialRef() {
    return poSRS;
}

/************************************************************************/
/*                            AppendGroup()                             */
/************************************************************************/

json_object *AppendGroup(json_object *parent, const CPLString &name) {
    json_object *obj = json_object_new_object();
    json_object *properties = json_object_new_object();
    json_object_object_add(parent, name, obj);
    json_object_object_add(obj, "properties", properties);
    return properties;
}

/************************************************************************/
/*                           AddPropertyMap()                           */
/************************************************************************/

json_object *AddPropertyMap(const CPLString &type, const CPLString &format = "") {
    json_object *obj = json_object_new_object();
    json_object_object_add(obj, "store", json_object_new_string("yes"));
    json_object_object_add(obj, "type", json_object_new_string(type.c_str()));
    if (!format.empty()) {
        json_object_object_add(obj, "format", json_object_new_string(format.c_str()));
    }
    return obj;
}

/************************************************************************/
/*                             BuildMap()                               */
/************************************************************************/

CPLString OGRElasticLayer::BuildMap() {
    json_object *map = json_object_new_object();
    json_object *properties = json_object_new_object();

    json_object *Feature = AppendGroup(map, "FeatureCollection");
    json_object_object_add(Feature, "type", AddPropertyMap("string"));
    json_object_object_add(Feature, "properties", properties);
    if (pAttributes) json_object_object_add(properties, "properties", (json_object *) pAttributes);
    json_object *geometry = AppendGroup(Feature, "geometry");
    json_object_object_add(geometry, "type", AddPropertyMap("string"));
    json_object_object_add(geometry, "coordinates", AddPropertyMap("geo_point"));

    CPLString jsonMap(json_object_to_json_string(map));
    json_object_put(map);

    // The attribute's were freed from the deletion of the map object
	// because we added it as a child of one of the map object attributes
    if (pAttributes) {
        pAttributes = NULL;
    }

    return jsonMap;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRElasticLayer::CreateFeature(OGRFeature *poFeature) {

    // Check to see if the user has elected to only write out the mapping file
    // This method will only write out one layer from the vector file in cases where there are multiple layers
    if (poDS->pszWriteMap != NULL) {
        if (pAttributes) {
            CPLString map = BuildMap();

            // Write the map to a file
            FILE *f = fopen(poDS->pszWriteMap, "wb");
            if (f) {
                fwrite(map.c_str(), 1, map.length(), f);
                fclose(f);
            }
        }
        return OGRERR_NONE;
    }

    // Check to see if we have any fields to upload to this index
    if (poDS->pszMapping == NULL && pAttributes) {
        poDS->UploadFile(CPLSPrintf("%s/%s/FeatureCollection/_mapping", poDS->GetName(), pszLayerName), BuildMap());
    }

    // Get the center point of the geometry
    OGREnvelope env;
	if (!poFeature->GetGeometryRef()) {
		return OGRERR_FAILURE;
	}
    poFeature->GetGeometryRef()->getEnvelope(&env);

    json_object *fieldObject = json_object_new_object();
    json_object *geometry = json_object_new_object();
    json_object *coordinates = json_object_new_array();
    json_object *properties = json_object_new_object();

    json_object_object_add(fieldObject, "geometry", geometry);
    json_object_object_add(geometry, "type", json_object_new_string("POINT"));
    json_object_object_add(geometry, "coordinates", coordinates);
    json_object_array_add(coordinates, json_object_new_double((env.MaxX + env.MinX)*0.5));
    json_object_array_add(coordinates, json_object_new_double((env.MaxY + env.MinY)*0.5));
    json_object_object_add(fieldObject, "type", json_object_new_string("Feature"));
    json_object_object_add(fieldObject, "properties", properties);

    // For every field that
    int fieldCount = poFeatureDefn->GetFieldCount();
    for (int i = 0; i < fieldCount; i++) {
		if(!poFeature->IsFieldSet( i ) ) {
			continue;
		}
        switch (poFeatureDefn->GetFieldDefn(i)->GetType()) {
            case OFTInteger:
                json_object_object_add(properties,
                        poFeatureDefn->GetFieldDefn(i)->GetNameRef(),
                        json_object_new_int(poFeature->GetFieldAsInteger(i)));
                break;
            case OFTReal:
                json_object_object_add(properties,
                        poFeatureDefn->GetFieldDefn(i)->GetNameRef(),
                        json_object_new_double(poFeature->GetFieldAsDouble(i)));
                break;
            default:
            {
                CPLString tmp = poFeature->GetFieldAsString(i);
                json_object_object_add(properties,
                        poFeatureDefn->GetFieldDefn(i)->GetNameRef(),
                        json_object_new_string(tmp));
            }
        }
    }

    // Build the field string
    CPLString fields(json_object_to_json_string(fieldObject));
    json_object_put(fieldObject);

    // Check to see if we're using bulk uploading
    if (poDS->nBulkUpload > 0) {
        sIndex += CPLSPrintf("{\"index\" :{\"_index\":\"%s\", \"_type\":\"FeatureCollection\"}}\n", pszLayerName) +
                fields + "\n\n";

        // Only push the data if we are over our bulk upload limit
        if ((int) sIndex.length() > poDS->nBulkUpload) {
            PushIndex();
        }

    } else { // Fall back to using single item upload for every feature
        poDS->UploadFile(CPLSPrintf("%s/%s/FeatureCollection/", poDS->GetName(), pszLayerName), fields);
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                             PushIndex()                              */
/************************************************************************/

void OGRElasticLayer::PushIndex() {
    if (sIndex.empty()) {
        return;
    }

    poDS->UploadFile(CPLSPrintf("%s/_bulk", poDS->GetName()), sIndex);
    sIndex.clear();
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRElasticLayer::CreateField(OGRFieldDefn *poFieldDefn, int bApproxOK) {
    if (!pAttributes) {
        pAttributes = json_object_new_object();
    }

    switch (poFieldDefn->GetType()) {
        case OFTInteger:
            json_object_object_add((json_object *) pAttributes, poFieldDefn->GetNameRef(), AddPropertyMap("integer"));
            break;
        case OFTReal:
            json_object_object_add((json_object *) pAttributes, poFieldDefn->GetNameRef(), AddPropertyMap("float"));
            break;
        case OFTString:
            json_object_object_add((json_object *) pAttributes, poFieldDefn->GetNameRef(), AddPropertyMap("string"));
            break;
        case OFTDateTime:
        case OFTDate:
            json_object_object_add((json_object *) pAttributes, poFieldDefn->GetNameRef(), AddPropertyMap("date", "yyyy/MM/dd HH:mm:ss||yyyy/MM/dd"));
            break;
        default:

            // These types are mapped as strings and may not be correct
            /*
                            OFTTime:
                            OFTIntegerList = 1,
                            OFTRealList = 3,
                            OFTStringList = 5,
                            OFTWideString = 6,
                            OFTWideStringList = 7,
                            OFTBinary = 8,
                            OFTMaxType = 11
             */
            json_object_object_add((json_object *) pAttributes, poFieldDefn->GetNameRef(), AddPropertyMap("string"));
    }

    poFeatureDefn->AddFieldDefn(poFieldDefn);
    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRElasticLayer::TestCapability(const char * pszCap) {
    if (EQUAL(pszCap, OLCFastFeatureCount))
        return FALSE;

    else if (EQUAL(pszCap, OLCStringsAsUTF8))
        return TRUE;

    else if (EQUAL(pszCap, OLCSequentialWrite))
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRElasticLayer::GetFeatureCount(int bForce) {
    CPLError(CE_Failure, CPLE_NotSupported,
            "Cannot read features when writing a Elastic file");
    return 0;
}
