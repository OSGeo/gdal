/******************************************************************************
 *
 * Project:  AmigoCloud Translator
 * Purpose:  Implements OGRAmigoCloudTableLayer class.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Victor Chernetsky, <victor at amigocloud dot com>
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

#include "ogr_amigocloud.h"
#include "ogr_p.h"
#include "ogr_pgdump.h"
#include "ogrgeojsonreader.h"
#include <sstream>
#include <iomanip>

CPL_CVSID("$Id$")

/************************************************************************/
/*                    OGRAMIGOCLOUDEscapeIdentifier( )                     */
/************************************************************************/

CPLString OGRAMIGOCLOUDEscapeIdentifier(const char* pszStr)
{
    CPLString osStr;

    osStr += "\"";

    char ch = '\0';
    for( int i = 0; (ch = pszStr[i]) != '\0'; i++ )
    {
        if (ch == '"')
            osStr.append(1, ch);
        osStr.append(1, ch);
    }

    osStr += "\"";

    return osStr;
}

std::string OGRAMIGOCLOUDJsonEncode(const std::string &s) {
    std::ostringstream o;
    for (auto c = s.cbegin(); c != s.cend(); c++) {
        switch (*c) {
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if (*c <= '\x1f') {
                    o << "\\u"
                      << std::hex << std::setw(4) << std::setfill('0') << (int)*c;
                } else {
                    o << *c;
                }
        }
    }
    return o.str();
}

/************************************************************************/
/*                        OGRAmigoCloudTableLayer()                        */
/************************************************************************/

OGRAmigoCloudTableLayer::OGRAmigoCloudTableLayer(
    OGRAmigoCloudDataSource* poDSIn,
    const char* pszName ) :
    OGRAmigoCloudLayer(poDSIn),
    osDatasetId(CPLString(pszName)),
    nNextFID(-1),
    bDeferredCreation(FALSE)
{
    osTableName = CPLString("dataset_") + osDatasetId;
    SetDescription( osDatasetId );
    osName = osDatasetId;
    nMaxChunkSize = atoi(
        CPLGetConfigOption(
            "AMIGOCLOUD_MAX_CHUNK_SIZE", "15" ) ) * 1024 * 1024;
}

/************************************************************************/
/*                    ~OGRAmigoCloudTableLayer()                           */
/************************************************************************/

OGRAmigoCloudTableLayer::~OGRAmigoCloudTableLayer()

{
    if( bDeferredCreation ) RunDeferredCreationIfNecessary();
    FlushDeferredInsert();
}

/************************************************************************/
/*                        GetLayerDefnInternal()                        */
/************************************************************************/

OGRFeatureDefn * OGRAmigoCloudTableLayer::GetLayerDefnInternal(CPL_UNUSED json_object* poObjIn)
{
    if( poFeatureDefn != nullptr )
    {
        return poFeatureDefn;
    }

    osBaseSQL.Printf("SELECT * FROM %s", OGRAMIGOCLOUDEscapeIdentifier(osTableName).c_str());
    EstablishLayerDefn(osTableName, nullptr);
    osBaseSQL = "";

    if( !osFIDColName.empty() )
    {
        CPLString sql;
        sql.Printf("SELECT %s FROM %s", OGRAMIGOCLOUDEscapeIdentifier(osFIDColName).c_str(), OGRAMIGOCLOUDEscapeIdentifier(osTableName).c_str());
        json_object* poObj = poDS->RunSQL(sql);
        if( poObj != nullptr && json_object_get_type(poObj) == json_type_object)
        {
            json_object* poRows = CPL_json_object_object_get(poObj, "data");

            if(poRows!=nullptr && json_object_get_type(poRows) == json_type_array)
            {
                mFIDs.clear();
                const auto nLength = json_object_array_length(poRows);
                for(auto i = decltype(nLength){0}; i < nLength; i++)
                {
                    json_object *obj = json_object_array_get_idx(poRows, i);

                    json_object_iter it;
                    it.key = nullptr;
                    it.val = nullptr;
                    it.entry = nullptr;
                    json_object_object_foreachC(obj, it)
                    {
                        const char *pszColName = it.key;
                        if(it.val != nullptr)
                        {
                            if(EQUAL(pszColName, osFIDColName.c_str()))
                            {
                                std::string amigo_id = json_object_get_string(it.val);
                                OGRAmigoCloudFID aFID(amigo_id, iNext);
                                mFIDs[aFID.iFID] = aFID;
                            }
                        }
                    }
                }
            }
            json_object_put(poObj);
        }
    }

    if( !osFIDColName.empty() )
    {
        osBaseSQL = "SELECT ";
        osBaseSQL += OGRAMIGOCLOUDEscapeIdentifier(osFIDColName);
    }
    for(int i=0; i<poFeatureDefn->GetGeomFieldCount(); i++)
    {
        if( osBaseSQL.empty() )
            osBaseSQL = "SELECT ";
        else
            osBaseSQL += ", ";
        osBaseSQL += OGRAMIGOCLOUDEscapeIdentifier(poFeatureDefn->GetGeomFieldDefn(i)->GetNameRef());
    }
    for(int i=0; i<poFeatureDefn->GetFieldCount(); i++)
    {
        if( osBaseSQL.empty() )
            osBaseSQL = "SELECT ";
        else
            osBaseSQL += ", ";
        osBaseSQL += OGRAMIGOCLOUDEscapeIdentifier(poFeatureDefn->GetFieldDefn(i)->GetNameRef());
    }
    if( osBaseSQL.empty() )
        osBaseSQL = "SELECT *";
    osBaseSQL += " FROM ";
    osBaseSQL += OGRAMIGOCLOUDEscapeIdentifier(osTableName);

    osSELECTWithoutWHERE = osBaseSQL;

    return poFeatureDefn;
}

/************************************************************************/
/*                        FetchNewFeatures()                            */
/************************************************************************/

json_object* OGRAmigoCloudTableLayer::FetchNewFeatures(GIntBig iNextIn)
{
    if( !osFIDColName.empty() )
    {
        CPLString osSQL;

        if(!osWHERE.empty())
        {
            osSQL.Printf("%s WHERE %s ",
                         osSELECTWithoutWHERE.c_str(),
                         (!osWHERE.empty()) ? CPLSPrintf("%s", osWHERE.c_str()) : "");
        } else
        {
            osSQL.Printf("%s", osSELECTWithoutWHERE.c_str());
        }

        if (osSQL.ifind("SELECT") != std::string::npos &&
            osSQL.ifind(" LIMIT ") == std::string::npos)
        {
            osSQL += " LIMIT ";
            osSQL += CPLSPrintf("%d", GetFeaturesToFetch());
            osSQL += " OFFSET ";
            osSQL += CPLSPrintf(CPL_FRMT_GIB, iNextIn);
        }
        return poDS->RunSQL(osSQL);
    }
    else
        return OGRAmigoCloudLayer::FetchNewFeatures(iNextIn);
}

/************************************************************************/
/*                           GetNextRawFeature()                        */
/************************************************************************/

OGRFeature  *OGRAmigoCloudTableLayer::GetNextRawFeature()
{
    if( bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return nullptr;
    FlushDeferredInsert();
    return OGRAmigoCloudLayer::GetNextRawFeature();
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRAmigoCloudTableLayer::SetAttributeFilter( const char *pszQuery )

{
    GetLayerDefn();

    if( pszQuery == nullptr )
        osQuery = "";
    else
    {
        osQuery = "(";
        osQuery += pszQuery;
        osQuery += ")";
    }

    BuildWhere();

    ResetReading();

    return OGRERR_NONE;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRAmigoCloudTableLayer::SetSpatialFilter( int iGeomField, OGRGeometry * poGeomIn )

{
    if( iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() ||
        GetLayerDefn()->GetGeomFieldDefn(iGeomField)->GetType() == wkbNone )
    {
        if( iGeomField != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
        }
        return;
    }
    m_iGeomFieldFilter = iGeomField;

    if( InstallFilter( poGeomIn ) )
    {
        BuildWhere();

        ResetReading();
    }
}

/************************************************************************/
/*                         FlushDeferredInsert()                          */
/************************************************************************/

void OGRAmigoCloudTableLayer::FlushDeferredInsert()

{
    if(vsDeferredInsertChangesets.empty())
        return;

    std::stringstream url;
    url << std::string(poDS->GetAPIURL()) << "/users/0/projects/" + std::string(poDS->GetProjectId()) + "/datasets/"+ osDatasetId +"/submit_change";

    std::stringstream query;

    query << "{\"type\":\"DML\",\"entity\":\"" << osTableName << "\",";
    query << "\"parent\":null,\"action\":\"INSERT\",\"data\":[";

    int counter=0;
    for(size_t i=0; i < vsDeferredInsertChangesets.size(); i++)
    {
        if(counter>0)
            query << ",";
        query << vsDeferredInsertChangesets[i].c_str();
        counter++;
    }
    query << "]}";

    std::stringstream changeset;
    changeset << "{\"change\": \"" << OGRAMIGOCLOUDJsonEncode(query.str()) << "\"}";

    json_object* poObj = poDS->RunPOST(url.str().c_str(), changeset.str().c_str());
    if( poObj != nullptr )
        json_object_put(poObj);

    vsDeferredInsertChangesets.clear();
    nNextFID = -1;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRAmigoCloudTableLayer::CreateField( OGRFieldDefn *poFieldIn,
                                          CPL_UNUSED int bApproxOK )
{
    GetLayerDefn();

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    OGRFieldDefn oField(poFieldIn);
/* -------------------------------------------------------------------- */
/*      Create the new field.                                           */
/* -------------------------------------------------------------------- */

    if( !bDeferredCreation )
    {
        CPLString osSQL;
        osSQL.Printf( "ALTER TABLE %s ADD COLUMN %s %s",
                    OGRAMIGOCLOUDEscapeIdentifier(osTableName).c_str(),
                    OGRAMIGOCLOUDEscapeIdentifier(oField.GetNameRef()).c_str(),
                    OGRPGCommonLayerGetType(oField, false, true).c_str() );
        if( !oField.IsNullable() )
            osSQL += " NOT NULL";
        if( oField.GetDefault() != nullptr && !oField.IsDefaultDriverSpecific() )
        {
            osSQL += " DEFAULT ";
            osSQL += OGRPGCommonLayerGetPGDefault(&oField);
        }

        json_object* poObj = poDS->RunSQL(osSQL);
        if( poObj == nullptr )
            return OGRERR_FAILURE;
        json_object_put(poObj);
    }

    poFeatureDefn->AddFieldDefn( &oField );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRAmigoCloudTableLayer::ICreateFeature( OGRFeature *poFeature )

{
    if( bDeferredCreation )
    {
        if( RunDeferredCreationIfNecessary() != OGRERR_NONE )
            return OGRERR_FAILURE;
    }

    GetLayerDefn();

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    std::stringstream record;

    record << "{\"new\":{";

    int counter=0;

    // Add geometry field
    for( int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
    {
        if( poFeature->GetGeomFieldRef(i) == nullptr )
            continue;

        record << "\"" << OGRAMIGOCLOUDJsonEncode(poFeatureDefn->GetGeomFieldDefn(i)->GetNameRef()) << "\":";

        OGRGeometry* poGeom = poFeature->GetGeomFieldRef(i);
        if( poGeom == nullptr )
            continue;

        OGRAmigoCloudGeomFieldDefn* poGeomFieldDefn =
                (OGRAmigoCloudGeomFieldDefn *)poFeatureDefn->GetGeomFieldDefn(i);
        int nSRID = poGeomFieldDefn->nSRID;
        if( nSRID == 0 )
            nSRID = 4326;
        char* pszEWKB = nullptr;
        if( wkbFlatten(poGeom->getGeometryType()) == wkbPolygon &&
            wkbFlatten(GetGeomType()) == wkbMultiPolygon )
        {
            OGRMultiPolygon* poNewGeom = new OGRMultiPolygon();
            poNewGeom->addGeometry(poGeom);
            pszEWKB = OGRGeometryToHexEWKB(poNewGeom, nSRID, 2, 1);
            delete poNewGeom;
        }
        else

            pszEWKB = OGRGeometryToHexEWKB(poGeom, nSRID, 2, 1);
        record << "\"" << pszEWKB << "\"";
        CPLFree(pszEWKB);

        counter++;
    }

    std::string amigo_id_value;

    // Add non-geometry field
    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        std::string name = poFeatureDefn->GetFieldDefn(i)->GetNameRef();
        std::string value = poFeature->GetFieldAsString(i);

        if(name=="amigo_id")
        {
            amigo_id_value = value;
            continue;
        }
        if( !poFeature->IsFieldSet(i) )
            continue;

        if(counter > 0)
            record << ",";

        record << OGRAMIGOCLOUDEscapeIdentifier(name.c_str()) << ":";

        if( !poFeature->IsFieldNull(i) )
        {
            OGRFieldType eType = poFeatureDefn->GetFieldDefn(i)->GetType();
            if( eType == OFTString || eType == OFTDateTime || eType == OFTDate || eType == OFTTime )
            {
                record << "\"" << OGRAMIGOCLOUDJsonEncode(value.c_str()) << "\"";
            } else
                record << OGRAMIGOCLOUDJsonEncode(value.c_str());
        }
        else
            record << "null";

        counter++;
    }

    record << "},";

    if(!amigo_id_value.empty())
    {
        record << "\"amigo_id\":\"" << amigo_id_value << "\"";
    } else
    {
        record << "\"amigo_id\":null";
    }

    record << "}";

    vsDeferredInsertChangesets.push_back(record.str());

    return OGRERR_NONE;
}

/************************************************************************/
/*                            ISetFeature()                              */
/************************************************************************/

OGRErr OGRAmigoCloudTableLayer::ISetFeature( OGRFeature *poFeature )

{
    OGRErr eRet = OGRERR_FAILURE;

    if( bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;
    FlushDeferredInsert();

    GetLayerDefn();

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    if (poFeature->GetFID() == OGRNullFID)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "FID required on features given to SetFeature()." );
        return OGRERR_FAILURE;
    }

    std::map<GIntBig, OGRAmigoCloudFID>::iterator it = mFIDs.find( poFeature->GetFID() );
    if(it!=mFIDs.end())
    {
        OGRAmigoCloudFID &aFID = it->second;

        CPLString osSQL;
        osSQL.Printf("UPDATE %s SET ", OGRAMIGOCLOUDEscapeIdentifier(osTableName).c_str());
        bool bMustComma = false;
        for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
        {
            if( !poFeature->IsFieldSet(i) )
                continue;

            if( bMustComma )
                osSQL += ", ";
            else
                bMustComma = true;

            osSQL += OGRAMIGOCLOUDEscapeIdentifier(poFeatureDefn->GetFieldDefn(i)->GetNameRef());
            osSQL += " = ";

            if(poFeature->IsFieldNull(i))
            {
                osSQL += "NULL";
            }
            else
            {
                OGRFieldType eType = poFeatureDefn->GetFieldDefn(i)->GetType();
                if(eType == OFTString || eType == OFTDateTime || eType == OFTDate || eType == OFTTime)
                {
                    osSQL += "'";
                    osSQL += OGRAMIGOCLOUDJsonEncode(poFeature->GetFieldAsString(i));
                    osSQL += "'";
                }
                else if((eType == OFTInteger || eType == OFTInteger64) &&
                        poFeatureDefn->GetFieldDefn(i)->GetSubType() == OFSTBoolean)
                {
                    osSQL += poFeature->GetFieldAsInteger(i) ? "'t'" : "'f'";
                }
                else
                    osSQL += poFeature->GetFieldAsString(i);
            }
        }

        for( int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
        {
            if( bMustComma )
                osSQL += ", ";
            else
                bMustComma = true;

            osSQL += OGRAMIGOCLOUDEscapeIdentifier(poFeatureDefn->GetGeomFieldDefn(i)->GetNameRef());
            osSQL += " = ";

            OGRGeometry *poGeom = poFeature->GetGeomFieldRef(i);
            if(poGeom == nullptr)
            {
                osSQL += "NULL";
            }
            else
            {
                OGRAmigoCloudGeomFieldDefn *poGeomFieldDefn =
                        (OGRAmigoCloudGeomFieldDefn *) poFeatureDefn->GetGeomFieldDefn(i);
                int nSRID = poGeomFieldDefn->nSRID;
                if(nSRID == 0)
                    nSRID = 4326;
                char *pszEWKB = OGRGeometryToHexEWKB(poGeom, nSRID, 2, 1);
                osSQL += "'";
                osSQL += pszEWKB;
                osSQL += "'";
                CPLFree(pszEWKB);
            }
        }

        if( !bMustComma ) // nothing to do
            return OGRERR_NONE;

        osSQL += CPLSPrintf(" WHERE %s = '%s'",
                            OGRAMIGOCLOUDEscapeIdentifier(osFIDColName).c_str(),
                            aFID.osAmigoId.c_str());

        std::stringstream changeset;
        changeset << "{\"query\": \"" << OGRAMIGOCLOUDJsonEncode(osSQL) << "\"}";
        std::stringstream url;
        url << std::string(poDS->GetAPIURL()) << "/users/0/projects/" + std::string(poDS->GetProjectId()) + "/sql";
        json_object *poObj = poDS->RunPOST(url.str().c_str(), changeset.str().c_str());

        if(poObj != nullptr)
        {
            json_object *poTotalRows = CPL_json_object_object_get(poObj, "total_rows");
            if(poTotalRows != nullptr && json_object_get_type(poTotalRows) == json_type_int)
            {
                int nTotalRows = json_object_get_int(poTotalRows);
                if(nTotalRows > 0)
                {
                    eRet = OGRERR_NONE;
                }
                else
                    eRet = OGRERR_NON_EXISTING_FEATURE;
            }
            json_object_put(poObj);
        }
    }
    return eRet;
}

/************************************************************************/
/*                          DeleteFeature()                             */
/************************************************************************/

OGRErr OGRAmigoCloudTableLayer::DeleteFeature( GIntBig nFID )

{
    OGRErr eRet = OGRERR_FAILURE;

    if( bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;
    FlushDeferredInsert();

    GetLayerDefn();

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    if( osFIDColName.empty() )
        return OGRERR_FAILURE;

    std::map<GIntBig, OGRAmigoCloudFID>::iterator it = mFIDs.find(nFID);
    if(it!=mFIDs.end())
    {
        OGRAmigoCloudFID &aFID = it->second;

        CPLString osSQL;
        osSQL.Printf("DELETE FROM %s WHERE %s = '%s'" ,
                     OGRAMIGOCLOUDEscapeIdentifier(osTableName).c_str(),
                     OGRAMIGOCLOUDEscapeIdentifier(osFIDColName).c_str(),
                     aFID.osAmigoId.c_str());

        std::stringstream changeset;
        changeset << "{\"query\": \"" << OGRAMIGOCLOUDJsonEncode(osSQL) << "\"}";
        std::stringstream url;
        url << std::string(poDS->GetAPIURL()) << "/users/0/projects/" + std::string(poDS->GetProjectId()) + "/sql";
        json_object *poObj = poDS->RunPOST(url.str().c_str(), changeset.str().c_str());
        if(poObj != nullptr)
        {
            json_object_put(poObj);
            eRet = OGRERR_NONE;
        }
    }
    return eRet;
}

/************************************************************************/
/*                             GetSRS_SQL()                             */
/************************************************************************/

CPLString OGRAmigoCloudTableLayer::GetSRS_SQL(const char* pszGeomCol)
{
    CPLString osSQL;

    osSQL.Printf("SELECT srid, srtext FROM spatial_ref_sys WHERE srid IN "
                "(SELECT Find_SRID('%s', '%s', '%s'))",
                 OGRAMIGOCLOUDJsonEncode(poDS->GetCurrentSchema()).c_str(),
                 OGRAMIGOCLOUDJsonEncode(osTableName).c_str(),
                 OGRAMIGOCLOUDJsonEncode(pszGeomCol).c_str());

    return osSQL;
}

/************************************************************************/
/*                             BuildWhere()                             */
/*                                                                      */
/*      Build the WHERE statement appropriate to the current set of     */
/*      criteria (spatial and attribute queries).                       */
/************************************************************************/

void OGRAmigoCloudTableLayer::BuildWhere()

{
    osWHERE = "";

    if( m_poFilterGeom != nullptr &&
        m_iGeomFieldFilter >= 0 &&
        m_iGeomFieldFilter < poFeatureDefn->GetGeomFieldCount() )
    {
        OGREnvelope  sEnvelope;

        m_poFilterGeom->getEnvelope( &sEnvelope );

        CPLString osGeomColumn(poFeatureDefn->GetGeomFieldDefn(m_iGeomFieldFilter)->GetNameRef());

        char szBox3D_1[128];
        char szBox3D_2[128];
        char* pszComma = nullptr;

        CPLsnprintf(szBox3D_1, sizeof(szBox3D_1), "%.18g %.18g", sEnvelope.MinX, sEnvelope.MinY);
        while((pszComma = strchr(szBox3D_1, ',')) != nullptr)
            *pszComma = '.';
        CPLsnprintf(szBox3D_2, sizeof(szBox3D_2), "%.18g %.18g", sEnvelope.MaxX, sEnvelope.MaxY);
        while((pszComma = strchr(szBox3D_2, ',')) != nullptr)
            *pszComma = '.';
        osWHERE.Printf("(%s && 'BOX3D(%s, %s)'::box3d)",
                       OGRAMIGOCLOUDEscapeIdentifier(osGeomColumn).c_str(),
                       szBox3D_1, szBox3D_2 );
    }

    if( !osQuery.empty() )
    {
        if( !osWHERE.empty() )
            osWHERE += " AND ";
        osWHERE += osQuery;
    }

    if( osFIDColName.empty() )
    {
        osBaseSQL = osSELECTWithoutWHERE;
        if( !osWHERE.empty() )
        {
            osBaseSQL += " WHERE ";
            osBaseSQL += osWHERE;
        }
    }
}

/************************************************************************/
/*                              GetFeature()                            */
/************************************************************************/

OGRFeature* OGRAmigoCloudTableLayer::GetFeature( GIntBig nFeatureId )
{

    if( bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return nullptr;
    FlushDeferredInsert();

    GetLayerDefn();

    if( osFIDColName.empty() )
        return OGRAmigoCloudLayer::GetFeature(nFeatureId);

    std::map<GIntBig, OGRAmigoCloudFID>::iterator it = mFIDs.find(nFeatureId);
    if(it!=mFIDs.end()) {
        OGRAmigoCloudFID &aFID = it->second;

        CPLString osSQL = osSELECTWithoutWHERE;
        osSQL += " WHERE ";
        osSQL += OGRAMIGOCLOUDEscapeIdentifier(osFIDColName).c_str();
        osSQL += " = ";
        osSQL += CPLSPrintf("'%s'", aFID.osAmigoId.c_str());

        json_object *poObj = poDS->RunSQL(osSQL);
        json_object *poRowObj = OGRAMIGOCLOUDGetSingleRow(poObj);
        if (poRowObj == nullptr) {
            if (poObj != nullptr)
                json_object_put(poObj);
            return OGRAmigoCloudLayer::GetFeature(nFeatureId);
        }

        OGRFeature *poFeature = BuildFeature(poRowObj);
        json_object_put(poObj);

        return poFeature;
    }
    return nullptr;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRAmigoCloudTableLayer::GetFeatureCount(int bForce)
{

    if( bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return 0;
    FlushDeferredInsert();

    GetLayerDefn();

    CPLString osSQL(CPLSPrintf("SELECT COUNT(*) FROM %s",
                               OGRAMIGOCLOUDEscapeIdentifier(osTableName).c_str()));
    if( !osWHERE.empty() )
    {
        osSQL += " WHERE ";
        osSQL += osWHERE;
    }

    json_object* poObj = poDS->RunSQL(osSQL);
    json_object* poRowObj = OGRAMIGOCLOUDGetSingleRow(poObj);
    if( poRowObj == nullptr )
    {
        if( poObj != nullptr )
            json_object_put(poObj);
        return OGRAmigoCloudLayer::GetFeatureCount(bForce);
    }

    json_object* poCount = CPL_json_object_object_get(poRowObj, "count");
    if( poCount == nullptr || json_object_get_type(poCount) != json_type_int )
    {
        json_object_put(poObj);
        return OGRAmigoCloudLayer::GetFeatureCount(bForce);
    }

    GIntBig nRet = (GIntBig)json_object_get_int64(poCount);

    json_object_put(poObj);

    return nRet;
}

/************************************************************************/
/*                             GetExtent()                              */
/*                                                                      */
/*      For PostGIS use internal Extend(geometry) function              */
/*      in other cases we use standard OGRLayer::GetExtent()            */
/************************************************************************/

OGRErr OGRAmigoCloudTableLayer::GetExtent( int iGeomField, OGREnvelope *psExtent, int bForce )
{
    CPLString   osSQL;

    if( bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;
    FlushDeferredInsert();

    if( iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() ||
        GetLayerDefn()->GetGeomFieldDefn(iGeomField)->GetType() == wkbNone )
    {
        if( iGeomField != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
        }
        return OGRERR_FAILURE;
    }

    OGRGeomFieldDefn* poGeomFieldDefn =
        poFeatureDefn->GetGeomFieldDefn(iGeomField);

    /* Do not take the spatial filter into account */
    osSQL.Printf( "SELECT ST_Extent(%s) FROM %s",
                  OGRAMIGOCLOUDEscapeIdentifier(poGeomFieldDefn->GetNameRef()).c_str(),
                  OGRAMIGOCLOUDEscapeIdentifier(osTableName).c_str());

    json_object* poObj = poDS->RunSQL(osSQL);
    json_object* poRowObj = OGRAMIGOCLOUDGetSingleRow(poObj);
    if( poRowObj != nullptr )
    {
        json_object* poExtent = CPL_json_object_object_get(poRowObj, "st_extent");
        if( poExtent != nullptr && json_object_get_type(poExtent) == json_type_string )
        {
            const char* pszBox = json_object_get_string(poExtent);
            const char * ptr, *ptrEndParenthesis;
            char szVals[64*6+6];

            ptr = strchr(pszBox, '(');
            if (ptr)
                ptr ++;
            if (ptr == nullptr ||
                (ptrEndParenthesis = strchr(ptr, ')')) == nullptr ||
                ptrEndParenthesis - ptr > (int)(sizeof(szVals) - 1))
            {
                CPLError( CE_Failure, CPLE_IllegalArg,
                            "Bad extent representation: '%s'", pszBox);

                json_object_put(poObj);
                return OGRERR_FAILURE;
            }

            strncpy(szVals,ptr,ptrEndParenthesis - ptr);
            szVals[ptrEndParenthesis - ptr] = '\0';

            char ** papszTokens = CSLTokenizeString2(szVals," ,",CSLT_HONOURSTRINGS);
            int nTokenCnt = 4;

            if ( CSLCount(papszTokens) != nTokenCnt )
            {
                CPLError( CE_Failure, CPLE_IllegalArg,
                            "Bad extent representation: '%s'", pszBox);
                CSLDestroy(papszTokens);

                json_object_put(poObj);
                return OGRERR_FAILURE;
            }

            // Take X,Y coords
            // For PostGIS ver >= 1.0.0 -> Tokens: X1 Y1 X2 Y2 (nTokenCnt = 4)
            // For PostGIS ver < 1.0.0 -> Tokens: X1 Y1 Z1 X2 Y2 Z2 (nTokenCnt = 6)
            // =>   X2 index calculated as nTokenCnt/2
            //      Y2 index calculated as nTokenCnt/2+1

            psExtent->MinX = CPLAtof( papszTokens[0] );
            psExtent->MinY = CPLAtof( papszTokens[1] );
            psExtent->MaxX = CPLAtof( papszTokens[nTokenCnt/2] );
            psExtent->MaxY = CPLAtof( papszTokens[nTokenCnt/2+1] );

            CSLDestroy(papszTokens);

            json_object_put(poObj);
            return OGRERR_NONE;
        }
    }

    if( poObj != nullptr )
        json_object_put(poObj);

    if( iGeomField == 0 )
        return OGRLayer::GetExtent( psExtent, bForce );
    else
        return OGRLayer::GetExtent( iGeomField, psExtent, bForce );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRAmigoCloudTableLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap, OLCFastFeatureCount) )
        return TRUE;
    if( EQUAL(pszCap, OLCFastGetExtent) )
        return TRUE;
    if( EQUAL(pszCap, OLCRandomRead) )
    {
        GetLayerDefn();
        return !osFIDColName.empty();
    }

    if(
        EQUAL(pszCap,OLCSequentialWrite)
     || EQUAL(pszCap,OLCRandomWrite)
     || EQUAL(pszCap,OLCDeleteFeature)
     || EQUAL(pszCap,ODsCCreateLayer)
     || EQUAL(pszCap,ODsCDeleteLayer)
       )
    {
        return poDS->IsReadWrite();
    }

    return OGRAmigoCloudLayer::TestCapability(pszCap);
}

/************************************************************************/
/*                        SetDeferredCreation()                          */
/************************************************************************/

void OGRAmigoCloudTableLayer::SetDeferredCreation(OGRwkbGeometryType eGType,
                                     OGRSpatialReference *poSRS,
                                     int bGeomNullable)
{
    bDeferredCreation = TRUE;
    nNextFID = 1;
    CPLAssert(poFeatureDefn == nullptr);
    poFeatureDefn = new OGRFeatureDefn(osTableName);
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);
    if( eGType == wkbPolygon )
        eGType = wkbMultiPolygon;
    else if( eGType == wkbPolygon25D )
        eGType = wkbMultiPolygon25D;
    if( eGType != wkbNone )
    {
        auto poFieldDefn =
            cpl::make_unique<OGRAmigoCloudGeomFieldDefn>("wkb_geometry", eGType);
        poFieldDefn->SetNullable(bGeomNullable);
        if( poSRS != nullptr )
        {
            poFieldDefn->nSRID = poDS->FetchSRSId( poSRS );
            poFieldDefn->SetSpatialRef(poSRS);
        }
        poFeatureDefn->AddGeomFieldDefn(std::move(poFieldDefn));
    }

    osBaseSQL.Printf("SELECT * FROM %s",
                     OGRAMIGOCLOUDEscapeIdentifier(osTableName).c_str());
}

CPLString OGRAmigoCloudTableLayer::GetAmigoCloudType(OGRFieldDefn& oField)
{
    char                szFieldType[256];

/* -------------------------------------------------------------------- */
/*      AmigoCloud supported types.                                   */
/* -------------------------------------------------------------------- */
    if( oField.GetType() == OFTInteger )
    {
            strcpy( szFieldType, "integer" );
    }
    else if( oField.GetType() == OFTInteger64 )
    {
        strcpy( szFieldType, "bigint" );
    }
    else if( oField.GetType() == OFTReal )
    {
       strcpy( szFieldType, "float" );
    }
    else if( oField.GetType() == OFTString )
    {
        strcpy( szFieldType, "string");
    }
    else if( oField.GetType() == OFTDate )
    {
        strcpy( szFieldType, "date" );
    }
    else if( oField.GetType() == OFTTime )
    {
        strcpy( szFieldType, "time" );
    }
    else if( oField.GetType() == OFTDateTime )
    {
        strcpy( szFieldType, "datetime" );
    } else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't create field %s with type %s on PostgreSQL layers.",
                  oField.GetNameRef(),
                  OGRFieldDefn::GetFieldTypeName(oField.GetType()) );
        strcpy( szFieldType, "");
    }

    return szFieldType;
}

bool OGRAmigoCloudTableLayer::IsDatasetExists()
{
    std::stringstream url;
    url << std::string(poDS->GetAPIURL()) << "/users/0/projects/" + std::string(poDS->GetProjectId()) + "/datasets/"+ osDatasetId;
    json_object* result = poDS->RunGET(url.str().c_str());
    if( result == nullptr )
        return false;

    {
        int type = json_object_get_type(result);
        if(type == json_type_object)
        {
            json_object *poId = CPL_json_object_object_get(result, "id");
            if(poId != nullptr)
            {
                json_object_put(result);
                return true;
            }
        }
        json_object_put(result);
    }

    // Sleep 3 sec
    CPLSleep(3);

    return false;
}

/************************************************************************/
/*                      RunDeferredCreationIfNecessary()                 */
/************************************************************************/

OGRErr OGRAmigoCloudTableLayer::RunDeferredCreationIfNecessary()
{
    if( !bDeferredCreation )
        return OGRERR_NONE;
    bDeferredCreation = FALSE;
    std::stringstream json;
    json << "{ \"name\":\"" << osDatasetId << "\",";
    json << "\"schema\": \"[";
    int counter=0;
    OGRwkbGeometryType eGType = GetGeomType();
    if( eGType != wkbNone )
    {
        CPLString osGeomType = OGRToOGCGeomType(eGType);
        if( wkbHasZ(eGType) )
            osGeomType += "Z";

        OGRAmigoCloudGeomFieldDefn *poFieldDefn =
                (OGRAmigoCloudGeomFieldDefn *)poFeatureDefn->GetGeomFieldDefn(0);

        json << "{\\\"name\\\":\\\"" << poFieldDefn->GetNameRef() << "\\\",";
        json << "\\\"type\\\":\\\"geometry\\\",";
        json << "\\\"geometry_type\\\":\\\"" << osGeomType << "\\\",";

        if( !poFieldDefn->IsNullable() )
            json << "\\\"nullable\\\":false,";
        else
            json << "\\\"nullable\\\":true,";

        json << "\\\"visible\\\": true}";

        counter++;
    }

    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        OGRFieldDefn* poFieldDefn = poFeatureDefn->GetFieldDefn(i);
        if( strcmp(poFieldDefn->GetNameRef(), osFIDColName) != 0 )
        {
            if(counter>0)
                json << ",";

            json << "{\\\"name\\\":\\\"" << poFieldDefn->GetNameRef() << "\\\",";
            json << "\\\"type\\\":\\\"" << GetAmigoCloudType(*poFieldDefn) << "\\\",";
            if( !poFieldDefn->IsNullable() )
                json << "\\\"nullable\\\":false,";
            else
                json << "\\\"nullable\\\":true,";

            if( poFieldDefn->GetDefault() != nullptr && !poFieldDefn->IsDefaultDriverSpecific() )
            {
                json << "\\\"default\\\":\\\"" << poFieldDefn->GetDefault() << "\\\",";
            }
            json << "\\\"visible\\\": true}";
            counter++;
        }
    }

    json << " ] \" }";

    std::stringstream url;
    url << std::string(poDS->GetAPIURL()) << "/users/0/projects/" + std::string(poDS->GetProjectId()) + "/datasets/create";

    json_object* result = poDS->RunPOST(url.str().c_str(), json.str().c_str());
    if( result != nullptr )
    {
        if(json_object_get_type(result) == json_type_object)
        {
            json_object *poName = CPL_json_object_object_get(result, "name");
            if(poName!=nullptr)
            {
                osName = json_object_to_json_string(poName);
            }

            json_object *poId = CPL_json_object_object_get(result, "id");
            if(poId!=nullptr) {
                osTableName = CPLString("dataset_") + json_object_to_json_string(poId);
                osDatasetId = json_object_to_json_string(poId);
                int retry = 10;
                while (!IsDatasetExists() && retry >= 0) {
                    retry--;
                }
                json_object_put(result);
                return OGRERR_NONE;
            }
        }
    }
    return OGRERR_FAILURE;
}
