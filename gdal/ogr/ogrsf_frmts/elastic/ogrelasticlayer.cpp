/******************************************************************************
 * $Id$
 *
 * Project:  ElasticSearch Translator
 * Purpose:
 * Author:
 *
 ******************************************************************************
 * Copyright (c) 2011, Adam Estrada
 * Copyright (c) 2012-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include <json.h> // JSON-C
#include "../geojson/ogrgeojsonwriter.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRElasticLayer()                          */
/************************************************************************/

OGRElasticLayer::OGRElasticLayer(CPL_UNUSED const char* pszFilename,
                                 const char* pszLayerName,
                                 OGRElasticDataSource* poDS,
                                 int bWriteMode,
                                 char** papszOptions) {
    this->pszLayerName = CPLStrdup(pszLayerName);
    this->poDS = poDS;
    this->pAttributes = NULL;
    eGeomTypeMapping = ES_GEOMTYPE_AUTO;
    const char* pszESGeomType = CSLFetchNameValue(papszOptions, "GEOM_MAPPING_TYPE");
    if( pszESGeomType != NULL )
    {
        if( EQUAL(pszESGeomType, "GEO_POINT") )
            eGeomTypeMapping = ES_GEOMTYPE_GEO_POINT;
        else if( EQUAL(pszESGeomType, "GEO_SHAPE") )
            eGeomTypeMapping = ES_GEOMTYPE_GEO_SHAPE;
    }
    nBulkUpload = poDS->nBulkUpload;
    if( CSLFetchBoolean(papszOptions, "BULK_INSERT", FALSE) )
        nBulkUpload = 10000;

    poFeatureDefn = new OGRFeatureDefn(pszLayerName);
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);
    bMappingWritten = FALSE;

    ResetReading();
    return;
}

/************************************************************************/
/*                         ~OGRElasticLayer()                           */
/************************************************************************/

OGRElasticLayer::~OGRElasticLayer() {
    SyncToDisk();

    CPLFree(pszLayerName);

    for(int i=0;i<(int)m_apoCT.size();i++)
        delete m_apoCT[i];

    poFeatureDefn->Release();
}

/************************************************************************/
/*                              SyncToDisk()                            */
/************************************************************************/

OGRErr OGRElasticLayer::SyncToDisk()
{
    if( !PushIndex() )
        return OGRERR_FAILURE;

    return OGRERR_NONE;
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
    if( poFeatureDefn->GetGeomFieldCount() == 1 &&
        (eGeomTypeMapping == ES_GEOMTYPE_GEO_POINT ||
         (eGeomTypeMapping == ES_GEOMTYPE_AUTO && wkbFlatten(poFeatureDefn->GetGeomType()) == wkbPoint)) )
    {
        json_object *geometry = AppendGroup(Feature, "geometry");
        json_object_object_add(geometry, "type", AddPropertyMap("string"));
        json_object_object_add(geometry, "coordinates", AddPropertyMap("geo_point"));
    }
    else if( poFeatureDefn->GetGeomFieldCount() > 0 &&
        (eGeomTypeMapping == ES_GEOMTYPE_GEO_SHAPE ||
         poFeatureDefn->GetGeomFieldCount() > 1 ||
         (eGeomTypeMapping == ES_GEOMTYPE_AUTO && wkbFlatten(poFeatureDefn->GetGeomType()) != wkbPoint)) )
    {
        for(int i=0;i<poFeatureDefn->GetGeomFieldCount();i++)
        {
            json_object *geometry = json_object_new_object();
            json_object_object_add(Feature,
                                   poFeatureDefn->GetGeomFieldDefn(i)->GetNameRef(),
                                   geometry);
            json_object_object_add(geometry, "type", json_object_new_string("geo_shape"));
        }
    }

    CPLString jsonMap(json_object_to_json_string(map));
    json_object_put(map);

    // The attribute's were freed from the deletion of the map object
	// because we added it as a child of one of the map object attributes
    if (pAttributes) {
        pAttributes = NULL;
    }

    return jsonMap;
}

static void BuildGeoJSONGeometry(json_object* geometry, OGRGeometry* poGeom)
{
    const int nPrecision = 10;
    double dfEps = pow(10.0, -(double)nPrecision);
    const char* pszGeomType = "";
    switch( wkbFlatten(poGeom->getGeometryType()) )
    {
        case wkbPoint: pszGeomType = "point"; break;
        case wkbLineString: pszGeomType = "linestring"; break;
        case wkbPolygon: pszGeomType = "polygon"; break;
        case wkbMultiPoint: pszGeomType = "multipoint"; break;
        case wkbMultiLineString: pszGeomType = "multilinestring"; break;
        case wkbMultiPolygon: pszGeomType = "multipolygon"; break;
        case wkbGeometryCollection: pszGeomType = "geometrycollection"; break;
        default: break;
    }
    json_object_object_add(geometry, "type", json_object_new_string(pszGeomType));
    
    switch( wkbFlatten(poGeom->getGeometryType()) )
    {
        case wkbPoint:
        {
            OGRPoint* poPoint = (OGRPoint*)poGeom;
            json_object *coordinates = json_object_new_array();
            json_object_object_add(geometry, "coordinates", coordinates);
            json_object_array_add(coordinates, json_object_new_double_with_precision(poPoint->getX(), nPrecision));
            json_object_array_add(coordinates, json_object_new_double_with_precision(poPoint->getY(), nPrecision));
            break;
        }
        
        case wkbLineString:
        {
            OGRLineString* poLS = (OGRLineString*)poGeom;
            json_object *coordinates = json_object_new_array();
            json_object_object_add(geometry, "coordinates", coordinates);
            for(int i=0;i<poLS->getNumPoints();i++)
            {
                json_object *point = json_object_new_array();
                json_object_array_add(coordinates, point);
                json_object_array_add(point, json_object_new_double_with_precision(poLS->getX(i), nPrecision));
                json_object_array_add(point, json_object_new_double_with_precision(poLS->getY(i), nPrecision));
            }
            break;
        }

        case wkbPolygon:
        {
            OGRPolygon* poPoly = (OGRPolygon*)poGeom;
            json_object *coordinates = json_object_new_array();
            json_object_object_add(geometry, "coordinates", coordinates);
            for(int i=0;i<1+poPoly->getNumInteriorRings();i++)
            {
                json_object *ring = json_object_new_array();
                json_object_array_add(coordinates, ring);
                OGRLineString* poLS = (i==0)?poPoly->getExteriorRing():poPoly->getInteriorRing(i-1);
                for(int j=0;j<poLS->getNumPoints();j++)
                {
                    if( j > 0 && fabs(poLS->getX(j) - poLS->getX(j-1)) < dfEps &&
                        fabs(poLS->getY(j) - poLS->getY(j-1)) < dfEps )
                        continue;
                    json_object *point = json_object_new_array();
                    json_object_array_add(ring, point);
                    json_object_array_add(point, json_object_new_double_with_precision(poLS->getX(j), nPrecision));
                    json_object_array_add(point, json_object_new_double_with_precision(poLS->getY(j), nPrecision));
                }
            }
            break;
        }
        
        case wkbMultiPoint:
        {
            OGRMultiPoint* poMP = (OGRMultiPoint*)poGeom;
            json_object *coordinates = json_object_new_array();
            json_object_object_add(geometry, "coordinates", coordinates);
            for(int i=0;i<poMP->getNumGeometries();i++)
            {
                json_object *point = json_object_new_array();
                json_object_array_add(coordinates, point);
                OGRPoint* poPoint = (OGRPoint*) poMP->getGeometryRef(i);
                json_object_array_add(point, json_object_new_double_with_precision(poPoint->getX(), nPrecision));
                json_object_array_add(point, json_object_new_double_with_precision(poPoint->getY(), nPrecision));
            }
            break;
        }
        
        case wkbMultiLineString:
        {
            OGRMultiLineString* poMLS = (OGRMultiLineString*)poGeom;
            json_object *coordinates = json_object_new_array();
            json_object_object_add(geometry, "coordinates", coordinates);
            for(int i=0;i<poMLS->getNumGeometries();i++)
            {
                json_object *ls = json_object_new_array();
                json_object_array_add(coordinates, ls);
                OGRLineString* poLS = (OGRLineString*) poMLS->getGeometryRef(i);
                for(int j=0;j<poLS->getNumPoints();j++)
                {
                    json_object *point = json_object_new_array();
                    json_object_array_add(ls, point);
                    json_object_array_add(point, json_object_new_double_with_precision(poLS->getX(j), nPrecision));
                    json_object_array_add(point, json_object_new_double_with_precision(poLS->getY(j), nPrecision));
                }
            }
            break;
        }
                
        case wkbMultiPolygon:
        {
            OGRMultiPolygon* poMP = (OGRMultiPolygon*)poGeom;
            json_object *coordinates = json_object_new_array();
            json_object_object_add(geometry, "coordinates", coordinates);
            for(int i=0;i<poMP->getNumGeometries();i++)
            {
                json_object *poly = json_object_new_array();
                json_object_array_add(coordinates, poly);
                OGRPolygon* poPoly = (OGRPolygon*) poMP->getGeometryRef(i);
                for(int j=0;j<1+poPoly->getNumInteriorRings();j++)
                {
                    json_object *ring = json_object_new_array();
                    json_object_array_add(poly, ring);
                    OGRLineString* poLS = (j==0)?poPoly->getExteriorRing():poPoly->getInteriorRing(j-1);
                    for(int k=0;k<poLS->getNumPoints();k++)
                    {
                        if( k > 0 && fabs(poLS->getX(k)- poLS->getX(k-1)) < dfEps &&
                            fabs(poLS->getY(k) - poLS->getY(k-1)) < dfEps )
                            continue;
                        json_object *point = json_object_new_array();
                        json_object_array_add(ring, point);
                        json_object_array_add(point, json_object_new_double_with_precision(poLS->getX(k), nPrecision));
                        json_object_array_add(point, json_object_new_double_with_precision(poLS->getY(k), nPrecision));
                    }
                }
            }
            break;
        }
                        
        case wkbGeometryCollection:
        {
            OGRGeometryCollection* poGC = (OGRGeometryCollection*)poGeom;
            json_object *geometries = json_object_new_array();
            json_object_object_add(geometry, "geometries", geometries);
            for(int i=0;i<poGC->getNumGeometries();i++)
            {
                json_object *subgeom = json_object_new_object();
                json_object_array_add(geometries, subgeom);
                BuildGeoJSONGeometry(subgeom, poGC->getGeometryRef(i));
            }
            break;
        }
        
        default:
            break;
    }
    
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRElasticLayer::ICreateFeature(OGRFeature *poFeature) {

    // Check to see if the user has elected to only write out the mapping file
    // This method will only write out one layer from the vector file in cases where there are multiple layers
    if (poDS->pszWriteMap != NULL) {
        if (!bMappingWritten) {
            bMappingWritten = TRUE;
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
    if (poDS->pszMapping == NULL && !bMappingWritten ) {
        bMappingWritten = TRUE;
        if( !poDS->UploadFile(CPLSPrintf("%s/%s/FeatureCollection/_mapping", poDS->GetName(), pszLayerName), BuildMap()) )
        {
            return OGRERR_FAILURE;
        }
    }

    json_object *fieldObject = json_object_new_object();
    
    OGRGeometry* poGeom = poFeature->GetGeometryRef();
    if( poGeom &&
        poFeatureDefn->GetGeomFieldCount() == 1 &&
        (eGeomTypeMapping == ES_GEOMTYPE_GEO_POINT ||
         (eGeomTypeMapping == ES_GEOMTYPE_AUTO && wkbFlatten(poFeatureDefn->GetGeomType()) == wkbPoint)) )
    {
        // Get the center point of the geometry
        OGREnvelope env;
        poGeom->getEnvelope(&env);

        json_object *geometry = json_object_new_object();
        json_object *coordinates = json_object_new_array();

        json_object_object_add(fieldObject, "geometry", geometry);
        json_object_object_add(geometry, "type", json_object_new_string("POINT"));
        json_object_object_add(geometry, "coordinates", coordinates);
        json_object_array_add(coordinates, json_object_new_double((env.MaxX + env.MinX)*0.5));
        json_object_array_add(coordinates, json_object_new_double((env.MaxY + env.MinY)*0.5));
        json_object_object_add(fieldObject, "type", json_object_new_string("Feature"));
    }
    else if( poFeatureDefn->GetGeomFieldCount() > 0 &&
            (eGeomTypeMapping == ES_GEOMTYPE_GEO_SHAPE ||
             poFeatureDefn->GetGeomFieldCount() > 1 ||
             (eGeomTypeMapping == ES_GEOMTYPE_AUTO && wkbFlatten(poFeatureDefn->GetGeomType()) != wkbPoint)) )
    {
        for(int i=0;i<poFeature->GetGeomFieldCount();i++)
        {
            poGeom = poFeature->GetGeomFieldRef(i);
            if( poGeom != NULL )
            {
                if( m_apoCT[i] != NULL )
                    poGeom->transform( m_apoCT[i] );

                json_object *geometry = json_object_new_object();
                json_object_object_add(fieldObject, poFeatureDefn->GetGeomFieldDefn(i)->GetNameRef(), geometry);
                BuildGeoJSONGeometry(geometry, poGeom);
            }
        }
    }

    json_object *properties = json_object_new_object();
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
            case OFTInteger64:
                json_object_object_add(properties,
                        poFeatureDefn->GetFieldDefn(i)->GetNameRef(),
                        json_object_new_int64(poFeature->GetFieldAsInteger64(i)));
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
    if (nBulkUpload > 0) {
        sIndex += CPLSPrintf("{\"index\" :{\"_index\":\"%s\", \"_type\":\"FeatureCollection\"}}\n", pszLayerName) +
                fields + "\n\n";

        // Only push the data if we are over our bulk upload limit
        if ((int) sIndex.length() > nBulkUpload) {
            if( !PushIndex() )
            {
                return OGRERR_FAILURE;
            }
        }

    } else { // Fall back to using single item upload for every feature
        if( !poDS->UploadFile(CPLSPrintf("%s/%s/FeatureCollection/", poDS->GetName(), pszLayerName), fields) )
        {
            return OGRERR_FAILURE;
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                             PushIndex()                              */
/************************************************************************/

int OGRElasticLayer::PushIndex() {
    if (sIndex.empty()) {
        return TRUE;
    }

    int bRet = poDS->UploadFile(CPLSPrintf("%s/_bulk", poDS->GetName()), sIndex);
    sIndex.clear();
    
    return bRet;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRElasticLayer::CreateField(OGRFieldDefn *poFieldDefn,
                                    CPL_UNUSED int bApproxOK) {
    if (!pAttributes) {
        pAttributes = json_object_new_object();
    }

    switch (poFieldDefn->GetType()) {
        case OFTInteger:
            json_object_object_add((json_object *) pAttributes, poFieldDefn->GetNameRef(), AddPropertyMap("integer"));
            break;
        case OFTInteger64:
            json_object_object_add((json_object *) pAttributes, poFieldDefn->GetNameRef(), AddPropertyMap("long"));
            break;
        case OFTReal:
            json_object_object_add((json_object *) pAttributes, poFieldDefn->GetNameRef(), AddPropertyMap("double"));
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
/*                           CreateGeomField()                          */
/************************************************************************/

OGRErr OGRElasticLayer::CreateGeomField( OGRGeomFieldDefn *poFieldIn, CPL_UNUSED int bApproxOK )

{
    if( poFeatureDefn->GetGeomFieldIndex(poFieldIn->GetNameRef()) >= 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CreateGeomField() called with an already existing field name: %s",
                  poFieldIn->GetNameRef());
        return OGRERR_FAILURE;
    }
    
    if( eGeomTypeMapping == ES_GEOMTYPE_GEO_POINT &&
        poFeatureDefn->GetGeomFieldCount() > 1 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "ES_GEOM_TYPE=GEO_POINT only supported for single geometry field");
        return OGRERR_FAILURE;
    }

    OGRGeomFieldDefn oFieldDefn(poFieldIn);
    if( EQUAL(oFieldDefn.GetNameRef(), "") )
        oFieldDefn.SetName("geometry");

    poFeatureDefn->AddGeomFieldDefn( &oFieldDefn );
    
    OGRCoordinateTransformation* poCT = NULL;
    if( oFieldDefn.GetSpatialRef() != NULL )
    {
        OGRSpatialReference oSRS_WGS84;
        oSRS_WGS84.SetFromUserInput(SRS_WKT_WGS84);
        if( !oSRS_WGS84.IsSame(oFieldDefn.GetSpatialRef()) )
        {
            poCT = OGRCreateCoordinateTransformation( oFieldDefn.GetSpatialRef(), &oSRS_WGS84 );
            if( poCT == NULL )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "On-the-fly reprojection to WGS84 longlat would be needed, but instanciation of transformer failed");
            }
        }
    }
    m_apoCT.push_back(poCT);

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
    else if (EQUAL(pszCap, OLCCreateField) ||
             EQUAL(pszCap, OLCCreateGeomField) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRElasticLayer::GetFeatureCount(CPL_UNUSED int bForce) {
    CPLError(CE_Failure, CPLE_NotSupported,
            "Cannot read features when writing a Elastic file");
    return 0;
}
