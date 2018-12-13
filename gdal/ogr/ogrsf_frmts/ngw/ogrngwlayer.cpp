/*******************************************************************************
 *  Project: NextGIS Web Driver
 *  Purpose: Implements NextGIS Web Driver
 *  Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2018, NextGIS
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *******************************************************************************/

 #include "ogr_ngw.h"

/*
 * CheckRequestResult()
 */
static bool CheckRequestResult(bool bResult, const CPLJSONObject &oRoot,
    const std::string &osErrorMessage)
{
    if( !bResult )
    {
        if( oRoot.IsValid() )
        {
            std::string osErrorMessageInt = oRoot.GetString("message");
            if( !osErrorMessageInt.empty() )
            {
                CPLErrorSetState(CE_Failure, CPLE_AppDefined,
                    osErrorMessageInt.c_str());
            }
        }
        return false;
    }

    if( !oRoot.IsValid() )
    {
        CPLErrorSetState(CE_Failure, CPLE_AppDefined, osErrorMessage.c_str());
        return false;
    }

    return true;
}

/*
 * JSONToFeature()
 */
static OGRFeature *JSONToFeature( const CPLJSONObject &featureJson,
    OGRFeatureDefn *poFeatureDefn, bool bCheckIgnoredFields = false )
{
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    poFeature->SetFID( featureJson.GetLong("id") );
    CPLJSONObject oFields = featureJson.GetObj("fields");
    for( int iField = 0; iField < poFeatureDefn->GetFieldCount(); ++iField )
    {
        OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn(iField);
        if( bCheckIgnoredFields && poFieldDefn->IsIgnored() )
        {
            continue;
        }
        CPLJSONObject oJSONField = oFields[poFieldDefn->GetNameRef()];
        if( oJSONField.IsValid() )
        {
            switch( poFieldDefn->GetType() )
            {
                case OFTInteger:
                    poFeature->SetField( iField, oJSONField.ToInteger() );
                    break;
                case OFTInteger64:
                    poFeature->SetField( iField, oJSONField.ToLong() );
                    break;
                case OFTReal:
                    poFeature->SetField( iField, oJSONField.ToDouble() );
                    break;
                case OFTBinary:
                    // Not supported.
                    break;
                case OFTString:
                case OFTIntegerList:
                case OFTInteger64List:
                case OFTRealList:
                case OFTStringList:
                    poFeature->SetField( iField, oJSONField.ToString().c_str() );
                    break;
                case OFTDate:
                case OFTTime:
                case OFTDateTime:
                {
                    int nYear = oJSONField.GetInteger("year");
                    int nMonth = oJSONField.GetInteger("month");
                    int nDay = oJSONField.GetInteger("day");
                    int nHour = oJSONField.GetInteger("hour");
                    int nMinute = oJSONField.GetInteger("minute");
                    int nSecond = oJSONField.GetInteger("second");
                    poFeature->SetField( iField, nYear, nMonth, nDay, nHour,
                        nMinute, float(nSecond) );
                    break;
                }
                default:
                    break;
            }
        }
    }
    OGRGeometry *poGeometry = nullptr;
    OGRGeometryFactory::createFromWkt( featureJson.GetString("geom").c_str(),
        nullptr, &poGeometry );
    if( poGeometry != nullptr )
    {
        OGRSpatialReference *poSpatialRef =
            poFeatureDefn->GetGeomFieldDefn( 0 )->GetSpatialRef();
        if( poSpatialRef != nullptr)
        {
            poGeometry->assignSpatialReference( poSpatialRef );
        }
        poFeature->SetGeomFieldDirectly( 0, poGeometry );
    }

    // FIXME: Do we need extension/description and extension/attachment storing in feature?

    return poFeature;
}

/*
 * FeatureToJson()
 */
static CPLJSONObject FeatureToJson(OGRFeature *poFeature)
{
    CPLJSONObject oFeaturesJson;
    if( poFeature == nullptr )
    {
        // Should not happen.
        return oFeaturesJson;
    }

    if( poFeature->GetFID() >= 0 )
    {
        oFeaturesJson.Add("id", poFeature->GetFID());
    }

    OGRGeometry *poGeom = poFeature->GetGeometryRef();
    if( poGeom )
    {
        char *pszWkt = nullptr;
        if( poGeom->exportToWkt( &pszWkt ) == OGRERR_NONE )
        {
            oFeaturesJson.Add("geom", std::string(pszWkt));
            CPLFree(pszWkt);
        }
    }

    OGRFeatureDefn *poFeatureDefn = poFeature->GetDefnRef();
    CPLJSONObject oFieldsJson("fields", oFeaturesJson);
    for( int iField = 0; iField < poFeatureDefn->GetFieldCount(); ++iField )
    {
        OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn(iField);
        if( poFeature->IsFieldNull(iField) == TRUE )
        {
            oFieldsJson.AddNull(poFieldDefn->GetNameRef());
            continue;
        }

        if( poFeature->IsFieldSet(iField) == TRUE )
        {
            switch( poFieldDefn->GetType() )
            {
                case OFTInteger:
                    oFieldsJson.Add( poFieldDefn->GetNameRef(),
                        poFeature->GetFieldAsInteger(iField) );
                    break;
                case OFTInteger64:
                    oFieldsJson.Add( poFieldDefn->GetNameRef(),
                        static_cast<GInt64>(
                            poFeature->GetFieldAsInteger64(iField)) );
                    break;
                case OFTReal:
                    oFieldsJson.Add( poFieldDefn->GetNameRef(),
                        poFeature->GetFieldAsDouble(iField) );
                    break;
                case OFTBinary:
                    // Not supported.
                    break;
                case OFTString:
                case OFTIntegerList:
                case OFTInteger64List:
                case OFTRealList:
                case OFTStringList:
                    oFieldsJson.Add( poFieldDefn->GetNameRef(),
                        poFeature->GetFieldAsString(iField) );
                    break;
                case OFTDate:
                case OFTTime:
                case OFTDateTime:
                {
                    int nYear, nMonth, nDay, nHour, nMinute, nSecond, nTZFlag;
                    if( poFeature->GetFieldAsDateTime(iField, &nYear, &nMonth,
                        &nDay, &nHour, &nMinute, &nSecond, &nTZFlag) == TRUE)
                    {
                        // TODO: Convert timestamp to UTC.
                        if( nTZFlag == 0 || nTZFlag == 100 )
                        {
                            CPLJSONObject oDateJson(poFieldDefn->GetNameRef(),
                                oFieldsJson);

                            oDateJson.Add("year", nYear);
                            oDateJson.Add("month", nMonth);
                            oDateJson.Add("day", nDay);
                            oDateJson.Add("hour", nHour);
                            oDateJson.Add("minute", nMinute);
                            oDateJson.Add("second", nSecond);
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    return oFeaturesJson;
}

/*
 * FeatureToJsonString()
 */
static std::string FeatureToJsonString(OGRFeature *poFeature)
{
    return FeatureToJson(poFeature).Format(CPLJSONObject::Plain);
}

 /*
  * OGRNGWLayer()
  */
OGRNGWLayer::OGRNGWLayer( OGRNGWDataset *poDSIn,
    const CPLJSONObject &oResourceJsonObject ) :
    osResourceId(oResourceJsonObject.GetString("resource/id", "-1")),
    poDS(poDSIn),
    bFetchedPermissions(false),
    nFeatureCount(-1),
    oNextPos(moFeatures.begin()),
    nPageStart(0),
    bNeedSyncData(false),
    bNeedSyncStructure(false)
{
    std::string osName = oResourceJsonObject.GetString("resource/display_name");
    poFeatureDefn = new OGRFeatureDefn( osName.c_str() );
    poFeatureDefn->Reference();

    poFeatureDefn->SetGeomType( NGWAPI::NGWGeomTypeToOGRGeomType(
        oResourceJsonObject.GetString("vector_layer/geometry_type")) );

    OGRSpatialReference *poSRS = new OGRSpatialReference;
    int nEPSG = oResourceJsonObject.GetInteger("vector_layer/srs/id", 3857); // Default NGW SRS is Web mercator EPSG:3857.
    if( poSRS->importFromEPSG( nEPSG ) == OGRERR_NONE )
    {
        if( poFeatureDefn->GetGeomFieldCount() != 0 )
        {
            poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef( poSRS );
        }
    }
    poSRS->Release();

    CPLJSONArray oFields = oResourceJsonObject.GetArray("feature_layer/fields");
    FillFields( oFields );
    FillMetadata( oResourceJsonObject );

    SetDescription( poFeatureDefn->GetName() );
}

/*
 * OGRNGWLayer()
 */
OGRNGWLayer::OGRNGWLayer( OGRNGWDataset *poDSIn, const std::string &osNameIn,
    OGRSpatialReference *poSpatialRef, OGRwkbGeometryType eGType,
    const std::string &osKeyIn, const std::string &osDescIn ) :
    osResourceId("-1"),
    poDS(poDSIn),
    bFetchedPermissions(false),
    nFeatureCount(0),
    oNextPos(moFeatures.begin()),
    nPageStart(0),
    bNeedSyncData(false),
    bNeedSyncStructure(false)
{
    poFeatureDefn = new OGRFeatureDefn( osNameIn.c_str() );
    poFeatureDefn->Reference();

    poFeatureDefn->SetGeomType( eGType );

    if( poSpatialRef )
    {
        if( poFeatureDefn->GetGeomFieldCount() != 0 )
        {
            poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef( poSpatialRef );
        }
    }

    if( !osDescIn.empty() )
    {
        OGRLayer::SetMetadataItem( "description", osDescIn.c_str() );
    }
    if( !osKeyIn.empty() )
    {
        OGRLayer::SetMetadataItem( "keyname", osKeyIn.c_str() );
    }

    SetDescription( poFeatureDefn->GetName() );
}

/*
 * ~OGRNGWLayer()
 */
OGRNGWLayer::~OGRNGWLayer()
{
    if( !soChangedIds.empty() )
    {
        bNeedSyncData = true;
    }
    SyncFeatures();
    FreeFeaturesCache();
    if( poFeatureDefn != nullptr )
    {
        poFeatureDefn->Release();
    }
}

/*
 * FreeFeaturesCache()
 */
void OGRNGWLayer::FreeFeaturesCache()
{
    for( auto &oPair: moFeatures )
    {
        OGRFeature::DestroyFeature( oPair.second );
    }
    moFeatures.clear();
}

/*
 * GetResourceId()
 */
std::string OGRNGWLayer::GetResourceId() const
{
    return osResourceId;
}

/*
 * Delete()
 */
bool OGRNGWLayer::Delete()
{
    if( osResourceId == "-1")
    {
        return true;
    }

    // Headers free in DeleteResource method.
    return NGWAPI::DeleteResource(poDS->GetUrl(), osResourceId, poDS->GetHeaders());
}

/*
 * ResetReading()
 */
void OGRNGWLayer::ResetReading()
{
    SyncToDisk();
    if( poDS->GetPageSize() != -1 )
    {
        FreeFeaturesCache();
        nPageStart = 0;
    }
    oNextPos = moFeatures.begin();
}

/*
 * SetNextByIndex()
 */
OGRErr OGRNGWLayer::SetNextByIndex( GIntBig nIndex )
{
    SyncToDisk();
    if( nIndex < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Feature index must be greater or equal 0. Got " CPL_FRMT_GIB, nIndex);
        return OGRERR_FAILURE;
    }
    if( poDS->GetPageSize() != -1 )
    {
        if( nPageStart < nIndex && nIndex >= nPageStart - poDS->GetPageSize() )
        {
            oNextPos = moFeatures.begin();
            std::advance(oNextPos, nIndex);
        }
        else
        {
            nPageStart = nIndex / poDS->GetPageSize() * poDS->GetPageSize();
            oNextPos = moFeatures.end();
        }
    }
    else
    {
        oNextPos = moFeatures.begin();
        std::advance(oNextPos, nIndex);
    }
    return OGRERR_NONE;
}

/*
 * GetNextFeature()
 */
OGRFeature *OGRNGWLayer::GetNextFeature()
{
    std::string osUrl;
    if( poDS->GetPageSize() != -1 )
    {
        if( oNextPos == moFeatures.end() && nPageStart < GetFeatureCount(FALSE) )
        {
            FreeFeaturesCache();

            osUrl = NGWAPI::GetFeaturePage( poDS->GetUrl(), osResourceId,
                nPageStart, poDS->GetPageSize() );
            nPageStart += poDS->GetPageSize();
        }
    }
    else if( moFeatures.empty() && GetFeatureCount(FALSE) > 0 )
    {
        osUrl = NGWAPI::GetFeature( poDS->GetUrl(), osResourceId );
    }

    if( !osUrl.empty() )
    {
        CPLErrorReset();
        CPLJSONDocument oFeatureReq;
        char **papszHTTPOptions = poDS->GetHeaders();
        bool bResult = oFeatureReq.LoadUrl( osUrl, papszHTTPOptions );
        CSLDestroy( papszHTTPOptions );

        CPLJSONObject oRoot = oFeatureReq.GetRoot();
        if( !CheckRequestResult(bResult, oRoot, "GetFeatures request failed") )
        {
            return nullptr;
        }

        CPLJSONArray aoJSONFeatures = oRoot.ToArray();
        for( int i = 0; i < aoJSONFeatures.Size(); ++i)
        {
            OGRFeature *poFeature = JSONToFeature( aoJSONFeatures[i],
                poFeatureDefn, false );
            moFeatures[poFeature->GetFID()] = poFeature;
        }

        oNextPos = moFeatures.begin();

        // Set m_nFeaturesRead for GetFeaturesRead.
        if( poDS->GetPageSize() != -1 )
        {
            m_nFeaturesRead = moFeatures.size();
        }
        else
        {
            m_nFeaturesRead = nPageStart;
        }
    }

    while( oNextPos != moFeatures.end() )
    {
        OGRFeature *poFeature = oNextPos->second;
        ++oNextPos;

        if( poFeature == nullptr ) // May be deleted.
        {
            continue;
        }

        if ((m_poFilterGeom == nullptr
            || FilterGeometry(poFeature->GetGeometryRef()))
            && (m_poAttrQuery == nullptr
            || m_poAttrQuery->Evaluate(poFeature)))
        {
            return poFeature->Clone();
        }
    }

    if( poDS->GetPageSize() != -1 && m_nFeaturesRead < GetFeatureCount(FALSE) )
    {
        return GetNextFeature();
    }
    return nullptr;
}

/*
 * GetFeature()
 */
OGRFeature *OGRNGWLayer::GetFeature( GIntBig nFID )
{
    // Check feature in cache.
    if( moFeatures[nFID] != nullptr )
    {
        return moFeatures[nFID]->Clone();
    }
    std::string osUrl = NGWAPI::GetFeature( poDS->GetUrl(), osResourceId) +
        std::to_string(nFID);
    CPLErrorReset();
    CPLJSONDocument oFeatureReq;
    char **papszHTTPOptions = poDS->GetHeaders();
    bool bResult = oFeatureReq.LoadUrl( osUrl, papszHTTPOptions );
    CSLDestroy( papszHTTPOptions );

    CPLJSONObject oRoot = oFeatureReq.GetRoot();
    if( !CheckRequestResult(bResult, oRoot, "GetFeature #" + std::to_string(nFID) +
        " response is invalid") )
    {
        return nullptr;
    }

    // Don't store feature in cache. This can broke sequence read.
    return JSONToFeature( oRoot, poFeatureDefn, true );
}

/*
 * GetLayerDefn()
 */
OGRFeatureDefn *OGRNGWLayer::GetLayerDefn()
{
    return poFeatureDefn;
}

/*
 * TestCapability()
 */
int OGRNGWLayer::TestCapability( const char *pszCap )
{
    FetchPermissions();
    if( EQUAL(pszCap, OLCRandomRead) )
        return TRUE;
    else if( EQUAL(pszCap, OLCSequentialWrite) )
        return stPermissions.bDataCanWrite && poDS->IsUpdateMode();
    else if( EQUAL(pszCap, OLCRandomWrite) )
        return stPermissions.bDataCanWrite && poDS->IsUpdateMode();
    else if( EQUAL(pszCap, OLCFastFeatureCount) )
        return TRUE;
    else if( EQUAL(pszCap, OLCFastGetExtent) )
        return TRUE;
    else if( EQUAL(pszCap, OLCAlterFieldDefn) ) // Only field name and alias can be altered.
        return stPermissions.bDatastructCanWrite && poDS->IsUpdateMode();
    else if( EQUAL(pszCap, OLCDeleteFeature) )
        return stPermissions.bDataCanWrite && poDS->IsUpdateMode();
    else if( EQUAL(pszCap, OLCStringsAsUTF8) )
        return TRUE;
    else if( EQUAL(pszCap, OLCFastSetNextByIndex) )
        return TRUE;
    else if( EQUAL(pszCap,OLCCreateField) )
        return poDS->IsUpdateMode();
    return FALSE;
}

/*
 * FillMetadata()
 */
void OGRNGWLayer::FillMetadata( const CPLJSONObject &oRootObject )
{
    std::string osCreateDate = oRootObject.GetString("resource/creation_date");
    if( !osCreateDate.empty() )
    {
        OGRLayer::SetMetadataItem( "creation_date", osCreateDate.c_str() );
    }
    std::string osDescription = oRootObject.GetString("resource/description");
    if( !osDescription.empty() )
    {
        OGRLayer::SetMetadataItem( "description", osDescription.c_str() );
    }
    std::string osKeyName = oRootObject.GetString("resource/keyname");
    if( !osKeyName.empty() )
    {
        OGRLayer::SetMetadataItem( "keyname", osKeyName.c_str() );
    }
    OGRLayer::SetMetadataItem( "id", osResourceId.c_str() );

    std::vector<CPLJSONObject> items =
        oRootObject.GetObj("resmeta/items").GetChildren();

    for( const CPLJSONObject &item : items )
    {
        std::string osSuffix = NGWAPI::GetResmetaSuffix( item.GetType() );
        OGRLayer::SetMetadataItem( (item.GetName() + osSuffix).c_str(),
            item.ToString().c_str(), "NGW" );
    }
}

/*
 * FillFields()
 */
void OGRNGWLayer::FillFields( const CPLJSONArray &oFields )
{
    for( int i = 0; i < oFields.Size(); ++i )
    {
        CPLJSONObject oField = oFields[i];
        std::string osFieldName = oField.GetString("keyname");
        OGRFieldType eFieldtype = NGWAPI::NGWFieldTypeToOGRFieldType(
            oField.GetString("datatype"));
        OGRFieldDefn oFieldDefn(osFieldName.c_str(), eFieldtype);
        poFeatureDefn->AddFieldDefn(&oFieldDefn);
        std::string osFieldId = oField.GetString("id");
        std::string osFieldAlias = oField.GetString("display_name");
        std::string osFieldIsLabel = oField.GetString("label_field");
        std::string osFieldGridVisible = oField.GetString("grid_visibility");

        std::string osFieldAliasName = "FIELD_" + std::to_string(i) + "_ALIAS";
        std::string osFieldIdName = "FIELD_" + std::to_string(i) + "_ID";
        std::string osFieldIsLabelName = "FIELD_" + std::to_string(i) + "_LABEL_FIELD";
        std::string osFieldGridVisibleName = "FIELD_" + std::to_string(i) + "_GRID_VISIBILITY";

        OGRLayer::SetMetadataItem(osFieldAliasName.c_str(),
            osFieldAlias.c_str(), "");
        OGRLayer::SetMetadataItem(osFieldIdName.c_str(),
            osFieldId.c_str(), "");
        OGRLayer::SetMetadataItem(osFieldIsLabelName.c_str(),
            osFieldIsLabel.c_str(), "");
        OGRLayer::SetMetadataItem(osFieldGridVisibleName.c_str(),
            osFieldGridVisible.c_str(), "");
    }

    // FIXME: Do we need extension/description and extension/attachment fields?
}

/*
 * GetFeatureCount()
 */
GIntBig OGRNGWLayer::GetFeatureCount( int bForce )
{
    if (m_poFilterGeom == nullptr && m_poAttrQuery == nullptr)
    {
        if( nFeatureCount < 0 || CPL_TO_BOOL(bForce) )
        {
            CPLErrorReset();
            CPLJSONDocument oCountReq;
            char **papszHTTPOptions = poDS->GetHeaders();
            bool bResult = oCountReq.LoadUrl( NGWAPI::GetFeatureCount( poDS->GetUrl(),
                osResourceId ), papszHTTPOptions );
            CSLDestroy( papszHTTPOptions );
            if( bResult )
            {
                CPLJSONObject oRoot = oCountReq.GetRoot();
                if( oRoot.IsValid() )
                {
                    nFeatureCount = oRoot.GetLong("total_count");
                    for(GIntBig nFID : soChangedIds)
                    {
                        if(nFID < 0)
                        {
                            nFeatureCount++; // If we have new features not sent to server.
                        }
                    }
                }
            }
        }
        return nFeatureCount;
    }
    else
    {
        return OGRLayer::GetFeatureCount(bForce);
    }
}

/*
 * GetExtent()
 */
OGRErr OGRNGWLayer::GetExtent( OGREnvelope *psExtent, int bForce )
{
    if( !stExtent.IsInit() || CPL_TO_BOOL(bForce) )
    {
        char **papszHTTPOptions = poDS->GetHeaders();
        bool bResult = NGWAPI::GetExtent(poDS->GetUrl(), osResourceId,
            papszHTTPOptions, 3857, stExtent);
        CSLDestroy( papszHTTPOptions );
        if( !bResult )
        {
            return OGRERR_FAILURE;
        }
    }
    *psExtent = stExtent;
    return OGRERR_NONE;
}

OGRErr OGRNGWLayer::GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce)
{
    return OGRLayer::GetExtent(iGeomField, psExtent, bForce);
}

/*
 * FetchPermissions()
 */
void OGRNGWLayer::FetchPermissions()
{
    if(bFetchedPermissions)
    {
        return;
    }

    if( poDS->IsUpdateMode() )
    {
        char **papszHTTPOptions = poDS->GetHeaders();
        stPermissions = NGWAPI::CheckPermissions( poDS->GetUrl(), osResourceId,
            papszHTTPOptions, poDS->IsUpdateMode() );
        CSLDestroy( papszHTTPOptions );
    }
    else
    {
        stPermissions.bDataCanRead = true;
        stPermissions.bResourceCanRead = true;
        stPermissions.bDatastructCanRead = true;
        stPermissions.bMetadataCanRead = true;
    }
    bFetchedPermissions = true;
}

OGRErr OGRNGWLayer::CreateField( OGRFieldDefn *poField, CPL_UNUSED int bApproxOK )
{
    if( osResourceId == "-1" ) // Can create field only on new layers (not synced with server).
    {
        poFeatureDefn->AddFieldDefn( poField );
        return OGRERR_NONE;
    }
    return OGRLayer::CreateField( poField, bApproxOK );
}

OGRErr OGRNGWLayer::DeleteField( int iField )
{
    if( osResourceId == "-1" ) // Can delete field only on new layers (not synced with server).
    {
        return poFeatureDefn->DeleteFieldDefn( iField );
    }
    return OGRLayer::DeleteField( iField );
}

OGRErr OGRNGWLayer::ReorderFields( int *panMap )
{
    if( osResourceId == "-1" ) // Can reorder fields only on new layers (not synced with server).
    {
        return poFeatureDefn->ReorderFieldDefns( panMap );
    }
    return OGRLayer::ReorderFields( panMap );
}

OGRErr OGRNGWLayer::AlterFieldDefn( int iField, OGRFieldDefn *poNewFieldDefn,
    int nFlagsIn )
{
    OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn( iField );
    if( poFieldDefn )
    {
        if( osResourceId == "-1" ) // Can alter field only on new layers (not synced with server).
        {
            poFieldDefn->SetName( poNewFieldDefn->GetNameRef() );
            poFieldDefn->SetType( poNewFieldDefn->GetType() );
            poFieldDefn->SetSubType( poNewFieldDefn->GetSubType() );
            poFieldDefn->SetWidth( poNewFieldDefn->GetWidth() );
            poFieldDefn->SetPrecision( poNewFieldDefn->GetPrecision() );
        }
        else if( nFlagsIn & ALTER_NAME_FLAG ) // Can only rename field, not change it type.
        {
            bNeedSyncStructure = true;
            poFieldDefn->SetName(poNewFieldDefn->GetNameRef());
        }
    }
    return OGRLayer::AlterFieldDefn( iField, poNewFieldDefn, nFlagsIn );
}

CPLErr OGRNGWLayer::SetMetadata(char **papszMetadata, const char *pszDomain)
{
    bNeedSyncStructure = true;
    return OGRLayer::SetMetadata(papszMetadata, pszDomain);
}

CPLErr OGRNGWLayer::SetMetadataItem(const char *pszName, const char *pszValue,
    const char *pszDomain)
{
    bNeedSyncStructure = true;
    return OGRLayer::SetMetadataItem(pszName, pszValue, pszDomain);
}

std::string OGRNGWLayer::CreateNGWResourceJson()
{
    CPLJSONObject oResourceJson;

    // Add resource json item.
    CPLJSONObject oResource("resource", oResourceJson);
    oResource.Add("cls", "vector_layer");
    CPLJSONObject oResourceParent("parent", oResource);
    oResourceParent.Add("id", static_cast<GIntBig>(std::stol( poDS->GetResourceId() )));
    oResource.Add("display_name", GetName());
    const char *pszKeyName = GetMetadataItem( "keyname" );
    if( pszKeyName )
    {
        oResource.Add("keyname", pszKeyName);
    }
    const char *pszDescription = GetMetadataItem( "description" );
    if( pszDescription )
    {
        oResource.Add("description", pszDescription);
    }

    // Add vector_layer json item.
    CPLJSONObject oVectorLayer("vector_layer", oResourceJson);
    CPLJSONObject oVectorLayerSrs("srs", oVectorLayer);

    OGRSpatialReference *poSpatialRef = GetSpatialRef();
    int nEPSG = 3857;
    if( poSpatialRef )
    {
        poSpatialRef->AutoIdentifyEPSG();
        const char *pszEPSG = poSpatialRef->GetAuthorityCode( nullptr );
        if( pszEPSG != nullptr )
        {
            nEPSG = atoi( pszEPSG );
        }
    }
    oVectorLayerSrs.Add("id", nEPSG);
    // In OGRNGWDataset::ICreateLayer we limit supported geometry types.
    oVectorLayer.Add("geometry_type", NGWAPI::OGRGeomTypeToNGWGeomType(
        GetGeomType() ));
    CPLJSONArray oVectorLayerFields;
    for( int iField = 0; iField < poFeatureDefn->GetFieldCount(); ++iField )
    {
        OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn( iField );

        CPLJSONObject oField;
        oField.Add("keyname", poFieldDefn->GetNameRef());
        oField.Add("datatype", NGWAPI::OGRFieldTypeToNGWFieldType(
            poFieldDefn->GetType() ));
        // Get alias from metadata.
        std::string osFieldAliasName = "FIELD_" + std::to_string(iField) + "_ALIAS";
        const char *pszFieldAlias = GetMetadataItem( osFieldAliasName.c_str() );
        if( pszFieldAlias )
        {
            oField.Add("display_name", pszFieldAlias);
        }
        oVectorLayerFields.Add(oField);
    }
    oVectorLayer.Add("fields", oVectorLayerFields);

    // Add resmeta json item.
    NGWAPI::FillResmeta(oResourceJson, GetMetadata("NGW"));

    return oResourceJson.Format(CPLJSONObject::Plain);
}

OGRErr OGRNGWLayer::SyncFeatures()
{
    if( !bNeedSyncData )
    {
        return OGRERR_NONE;
    }
    bool bResult = NGWAPI::PatchFeatures( poDS->GetUrl(), osResourceId,
        FeaturesToJson(), poDS->GetHeaders() );
    if( bResult )
    {
        bNeedSyncData = false;
        nFeatureCount += soChangedIds.size();
        soChangedIds.clear();
        FreeFeaturesCache(); // While patching we cannot receive new features FIDs.
    }
    else
    {
        // Error message should set in NGWAPI::PatchFeatures function.
        return OGRERR_FAILURE;
    }
    return OGRERR_NONE;
}

OGRErr OGRNGWLayer::SyncToDisk()
{
    if( osResourceId == "-1" ) // Create vector layer at NextGIS Web.
    {
        bNeedSyncData = !moFeatures.empty();
        std::string osResourceIdInt = NGWAPI::CreateResource( poDS->GetUrl(),
            CreateNGWResourceJson(), poDS->GetHeaders() );
        if( osResourceIdInt == "-1" )
        {
            // Error message should set in CreateResource.
            return OGRERR_FAILURE;
        }
        osResourceId = osResourceIdInt;
        OGRLayer::SetMetadataItem( "id", osResourceId.c_str() );
        bNeedSyncStructure = false;
    }
    else if( bNeedSyncStructure ) // Update vector layer at NextGIS Web.
    {
        if( !NGWAPI::UpdateResource( poDS->GetUrl(), GetResourceId(),
            CreateNGWResourceJson(), poDS->GetHeaders() ) )
        {
            // Error message should set in UpdateResource.
            return OGRERR_FAILURE;
        }
        bNeedSyncStructure = false;
    }

    // Sync features.
    return SyncFeatures();
}

/*
 * DeleteFeature()
 */
OGRErr OGRNGWLayer::DeleteFeature(GIntBig nFID)
{
    if( nFID < 0 )
    {
        if( moFeatures[nFID] != nullptr )
        {
            OGRFeature::DestroyFeature( moFeatures[nFID] );
            moFeatures[nFID] = nullptr;
            nFeatureCount--;
            soChangedIds.erase(nFID);
            return OGRERR_NONE;
        }
        CPLError(CE_Failure, CPLE_AppDefined, "Feature with id #" CPL_FRMT_GIB " not found.", nFID);
        return OGRERR_FAILURE;
    }
    else
    {
        FetchPermissions();
        if( stPermissions.bDataCanWrite && poDS->IsUpdateMode() )
        {
            bool bResult = NGWAPI::DeleteFeature(poDS->GetUrl(), osResourceId,
                std::to_string(nFID), poDS->GetHeaders());
            if( bResult )
            {
                if( moFeatures[nFID] != nullptr )
                {
                    OGRFeature::DestroyFeature( moFeatures[nFID] );
                    moFeatures[nFID] = nullptr;
                }
                nFeatureCount--;
                soChangedIds.erase(nFID);
                return OGRERR_NONE;
            }
            return OGRERR_FAILURE;
        }
        CPLErrorReset();
        CPLError(CE_Failure, CPLE_AppDefined, "Delete feature #" CPL_FRMT_GIB " operation is not permitted.", nFID);
        return OGRERR_FAILURE;
    }
}

/*
 * DeleteAllFeatures()
 */
bool OGRNGWLayer::DeleteAllFeatures()
{
    if( osResourceId == "-1" )
    {
        FreeFeaturesCache();
        nFeatureCount = 0;
        soChangedIds.clear();
        return true;
    }
    else
    {
        FetchPermissions();
        if( stPermissions.bDataCanWrite && poDS->IsUpdateMode() )
        {
            bool bResult = NGWAPI::DeleteFeature(poDS->GetUrl(), osResourceId, "",
                poDS->GetHeaders());
            if( bResult )
            {
                FreeFeaturesCache();
                nFeatureCount = 0;
                soChangedIds.clear();
            }
            return bResult;
        }
    }
    CPLErrorReset();
    CPLError(CE_Failure, CPLE_AppDefined, "Delete all features operation is not permitted.");
    return false;
}

/*
 * FeaturesToJson()
 */
std::string OGRNGWLayer::FeaturesToJson()
{
    CPLJSONArray oFeatureJsonArray;
    for(GIntBig nFID : soChangedIds)
    {
        if( moFeatures[nFID] != nullptr )
        {
            oFeatureJsonArray.Add( FeatureToJson( moFeatures[nFID] ) );
        }
    }
    return oFeatureJsonArray.Format(CPLJSONObject::Plain);
}

/*
 * ISetFeature()
 */
OGRErr OGRNGWLayer::ISetFeature(OGRFeature *poFeature)
{
    if( poDS->IsBatchMode() )
    {
        if(moFeatures[poFeature->GetFID()])
        {
            OGRFeature::DestroyFeature( moFeatures[poFeature->GetFID()] );
        }
        moFeatures[poFeature->GetFID()] = poFeature->Clone();
        soChangedIds.insert(poFeature->GetFID());

        if( soChangedIds.size() > static_cast<size_t>(poDS->GetBatchSize()) )
        {
            bNeedSyncData = true;
        }

        return SyncToDisk();
    }
    else
    {
        OGRErr eResult = SyncToDisk(); // For create new layer if not yet created.
        if(eResult == OGRERR_NONE)
        {
            if(poFeature->GetFID() < 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot update not exist feature #" CPL_FRMT_GIB, poFeature->GetFID());
                return OGRERR_FAILURE;
            }

            bool bResult = NGWAPI::UpdateFeature(poDS->GetUrl(), osResourceId,
                std::to_string(poFeature->GetFID()),
                FeatureToJsonString(poFeature), poDS->GetHeaders());
            if( bResult )
            {
                CPLDebug("NGW", "ISetFeature with FID #" CPL_FRMT_GIB, poFeature->GetFID());

                OGRFeature::DestroyFeature( moFeatures[poFeature->GetFID()] );
                moFeatures[poFeature->GetFID()] = poFeature->Clone();
                return OGRERR_NONE;
            }
            else
            {
                // CPLError should be set in NGWAPI::UpdateFeature.
                return OGRERR_FAILURE;
            }
        }
        else
        {
            return eResult;
        }
    }
}

/*
 * ICreateFeature()
 */
OGRErr OGRNGWLayer::ICreateFeature(OGRFeature *poFeature)
{
    if( poDS->IsBatchMode() )
    {
        GIntBig nNewFID = -1;
        if( !soChangedIds.empty() )
        {
            nNewFID = *(soChangedIds.begin()) - 1;
        }
        poFeature->SetFID(nNewFID);
        OGRFeature *poFeatureClone = poFeature->Clone();
        moFeatures[nNewFID] = poFeatureClone;
        soChangedIds.insert(nNewFID);
        nFeatureCount++;

        if( soChangedIds.size() > static_cast<size_t>(poDS->GetBatchSize()) )
        {
            bNeedSyncData = true;
        }

        return SyncToDisk();
    }
    else
    {
        OGRErr eResult = SyncToDisk(); // For create new layer if not yet created.
        if(eResult == OGRERR_NONE)
        {
            GIntBig nNewFID = NGWAPI::CreateFeature(poDS->GetUrl(), osResourceId,
                FeatureToJsonString(poFeature), poDS->GetHeaders());
            if(nNewFID >= 0)
            {
                poFeature->SetFID(nNewFID);
                OGRFeature *poFeatureClone = poFeature->Clone();
                moFeatures[nNewFID] = poFeatureClone;
                nFeatureCount++;
                return OGRERR_NONE;
            }
            else
            {
                // CPLError should be set in NGWAPI::CreateFeature.
                return OGRERR_FAILURE;
            }
        }
        else
        {
            return eResult;
        }
    }
}
