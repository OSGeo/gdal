/*******************************************************************************
 *  Project: NextGIS Web Driver
 *  Purpose: Implements NextGIS Web Driver
 *  Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2018-2025, NextGIS <info@nextgis.com>
 *
 * SPDX-License-Identifier: MIT
 *******************************************************************************/

#include "ogr_ngw.h"

/*
 * OGRGeometryToWKT()
 */
static std::string OGRGeometryToWKT(OGRGeometry *poGeom)
{
    std::string osOut;
    if (nullptr == poGeom)
    {
        return osOut;
    }

    char *pszWkt = nullptr;
    if (poGeom->exportToWkt(&pszWkt) == OGRERR_NONE)
    {
        osOut = pszWkt;
    }
    CPLFree(pszWkt);

    return osOut;
}

/*
 * JSONToFeature()
 */
static OGRFeature *JSONToFeature(const CPLJSONObject &featureJson,
                                 OGRFeatureDefn *poFeatureDefn,
                                 bool bCheckIgnoredFields = false,
                                 bool bStoreExtensionData = false)
{
    OGRFeature *poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetFID(featureJson.GetLong("id"));
    CPLJSONObject oFields = featureJson.GetObj("fields");
    for (int iField = 0; iField < poFeatureDefn->GetFieldCount(); ++iField)
    {
        OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn(iField);
        if (bCheckIgnoredFields && poFieldDefn->IsIgnored())
        {
            continue;
        }
        CPLJSONObject oJSONField = oFields[poFieldDefn->GetNameRef()];
        if (oJSONField.IsValid() &&
            oJSONField.GetType() != CPLJSONObject::Type::Null)
        {
            switch (poFieldDefn->GetType())
            {
                case OFTInteger:
                    poFeature->SetField(iField, oJSONField.ToInteger());
                    break;
                case OFTInteger64:
                    poFeature->SetField(iField, oJSONField.ToLong());
                    break;
                case OFTReal:
                    poFeature->SetField(iField, oJSONField.ToDouble());
                    break;
                case OFTBinary:
                    // Not supported.
                    break;
                case OFTString:
                case OFTIntegerList:
                case OFTInteger64List:
                case OFTRealList:
                case OFTStringList:
                    poFeature->SetField(iField, oJSONField.ToString().c_str());
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
                    poFeature->SetField(iField, nYear, nMonth, nDay, nHour,
                                        nMinute, float(nSecond));
                    break;
                }
                default:
                    break;
            }
        }
    }

    bool bFillGeometry =
        !(bCheckIgnoredFields && poFeatureDefn->IsGeometryIgnored());

    if (bFillGeometry)
    {
        OGRGeometry *poGeometry = nullptr;
        OGRGeometryFactory::createFromWkt(featureJson.GetString("geom").c_str(),
                                          nullptr, &poGeometry);
        if (poGeometry != nullptr)
        {
            const OGRSpatialReference *poSpatialRef =
                poFeatureDefn->GetGeomFieldDefn(0)->GetSpatialRef();
            if (poSpatialRef != nullptr)
            {
                poGeometry->assignSpatialReference(poSpatialRef);
            }
            poFeature->SetGeomFieldDirectly(0, poGeometry);
        }
    }

    // Get extensions key and store it in native data.
    if (bStoreExtensionData)
    {
        CPLJSONObject oExtensions = featureJson.GetObj("extensions");
        if (oExtensions.IsValid() &&
            oExtensions.GetType() != CPLJSONObject::Type::Null)
        {
            poFeature->SetNativeData(
                oExtensions.Format(CPLJSONObject::PrettyFormat::Plain).c_str());
            poFeature->SetNativeMediaType("application/json");
        }
    }

    return poFeature;
}

/*
 * FeatureToJson()
 */
static CPLJSONObject FeatureToJson(OGRFeature *poFeature)
{
    CPLJSONObject oFeatureJson;
    if (poFeature == nullptr)
    {
        // Should not happen.
        return oFeatureJson;
    }

    if (poFeature->GetFID() >= 0)
    {
        oFeatureJson.Add("id", poFeature->GetFID());
    }

    OGRGeometry *poGeom = poFeature->GetGeometryRef();
    std::string osGeomWKT = OGRGeometryToWKT(poGeom);
    if (!osGeomWKT.empty())
    {
        oFeatureJson.Add("geom", osGeomWKT);
    }

    OGRFeatureDefn *poFeatureDefn = poFeature->GetDefnRef();
    CPLJSONObject oFieldsJson("fields", oFeatureJson);
    for (int iField = 0; iField < poFeatureDefn->GetFieldCount(); ++iField)
    {
        OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn(iField);
        if (poFeature->IsFieldNull(iField) == TRUE)
        {
            oFieldsJson.AddNull(poFieldDefn->GetNameRef());
            continue;
        }

        if (poFeature->IsFieldSet(iField) == TRUE)
        {
            switch (poFieldDefn->GetType())
            {
                case OFTInteger:
                    oFieldsJson.Add(poFieldDefn->GetNameRef(),
                                    poFeature->GetFieldAsInteger(iField));
                    break;
                case OFTInteger64:
                    oFieldsJson.Add(
                        poFieldDefn->GetNameRef(),
                        static_cast<GInt64>(
                            poFeature->GetFieldAsInteger64(iField)));
                    break;
                case OFTReal:
                    oFieldsJson.Add(poFieldDefn->GetNameRef(),
                                    poFeature->GetFieldAsDouble(iField));
                    break;
                case OFTBinary:
                    // Not supported.
                    break;
                case OFTString:
                case OFTIntegerList:
                case OFTInteger64List:
                case OFTRealList:
                case OFTStringList:
                    oFieldsJson.Add(poFieldDefn->GetNameRef(),
                                    poFeature->GetFieldAsString(iField));
                    break;
                case OFTDate:
                case OFTTime:
                case OFTDateTime:
                {
                    int nYear, nMonth, nDay, nHour, nMinute, nSecond, nTZFlag;
                    if (poFeature->GetFieldAsDateTime(
                            iField, &nYear, &nMonth, &nDay, &nHour, &nMinute,
                            &nSecond, &nTZFlag) == TRUE)
                    {
                        // TODO: Convert timestamp to UTC.
                        if (nTZFlag == 0 || nTZFlag == 100)
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

    if (poFeature->GetNativeData())
    {
        CPLJSONDocument oExtensions;
        if (oExtensions.LoadMemory(poFeature->GetNativeData()))
        {
            oFeatureJson.Add("extensions", oExtensions.GetRoot());
        }
    }

    return oFeatureJson;
}

/*
 * FeatureToJsonString()
 */
static std::string FeatureToJsonString(OGRFeature *poFeature)
{
    return FeatureToJson(poFeature).Format(CPLJSONObject::PrettyFormat::Plain);
}

/*
 * FeaturesIDToJsonString()
 */
static std::string
FeaturesIDToJsonString(const std::vector<GIntBig> &vFeaturesID)
{
    CPLJSONArray oFeaturesIDJsonArray;

    for (GIntBig nFeatureID : vFeaturesID)
    {
        CPLJSONObject oFeatureIDJson;
        oFeatureIDJson.Add("id", nFeatureID);

        oFeaturesIDJsonArray.Add(oFeatureIDJson);
    }

    return oFeaturesIDJsonArray.Format(CPLJSONObject::PrettyFormat::Plain);
}

/*
 * FreeMap()
 */
static void FreeMap(std::map<GIntBig, OGRFeature *> &moFeatures)
{
    // cppcheck-suppress constVariableReference
    for (auto &oPair : moFeatures)
    {
        OGRFeature::DestroyFeature(oPair.second);
    }

    moFeatures.clear();
}

static bool CheckFieldNameUnique(OGRFeatureDefn *poFeatureDefn, int iField,
                                 const char *pszFieldName)
{
    for (int i = 0; i < poFeatureDefn->GetFieldCount(); ++i)
    {
        if (i == iField)
        {
            continue;
        }

        OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn(i);
        if (poFieldDefn && EQUAL(poFieldDefn->GetNameRef(), pszFieldName))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Field name %s already present in field %d.", pszFieldName,
                     i);
            return false;
        }
    }
    return true;
}

/*
 * TranslateSQLToFilter()
 */
std::string OGRNGWLayer::TranslateSQLToFilter(swq_expr_node *poNode)
{
    if (nullptr == poNode)
    {
        return "";
    }

    if (poNode->eNodeType == SNT_OPERATION)
    {
        if (poNode->nOperation == SWQ_AND && poNode->nSubExprCount == 2)
        {
            std::string osFilter1 =
                TranslateSQLToFilter(poNode->papoSubExpr[0]);
            std::string osFilter2 =
                TranslateSQLToFilter(poNode->papoSubExpr[1]);

            if (osFilter1.empty() || osFilter2.empty())
            {
                return "";
            }
            return osFilter1 + "&" + osFilter2;
        }
        else if ((poNode->nOperation == SWQ_EQ ||
                  poNode->nOperation == SWQ_NE ||
                  poNode->nOperation == SWQ_GE ||
                  poNode->nOperation == SWQ_LE ||
                  poNode->nOperation == SWQ_LT ||
                  poNode->nOperation == SWQ_GT ||
                  poNode->nOperation == SWQ_LIKE ||
                  poNode->nOperation == SWQ_ILIKE) &&
                 poNode->nSubExprCount == 2 &&
                 poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
                 poNode->papoSubExpr[1]->eNodeType == SNT_CONSTANT)
        {
            if (poNode->papoSubExpr[0]->string_value == nullptr)
            {
                return "";
            }
            char *pszNameEncoded = CPLEscapeString(
                poNode->papoSubExpr[0]->string_value, -1, CPLES_URL);
            std::string osFieldName = "fld_" + std::string(pszNameEncoded);
            CPLFree(pszNameEncoded);

            switch (poNode->nOperation)
            {
                case SWQ_EQ:
                    osFieldName += "__eq";
                    break;
                case SWQ_NE:
                    osFieldName += "__ne";
                    break;
                case SWQ_GE:
                    osFieldName += "__ge";
                    break;
                case SWQ_LE:
                    osFieldName += "__le";
                    break;
                case SWQ_LT:
                    osFieldName += "__lt";
                    break;
                case SWQ_GT:
                    osFieldName += "__gt";
                    break;
                case SWQ_LIKE:
                    osFieldName += "__like";
                    break;
                case SWQ_ILIKE:
                    osFieldName += "__ilike";
                    break;
                default:
                    CPLAssert(false);
                    break;
            }

            std::string osVal;
            switch (poNode->papoSubExpr[1]->field_type)
            {
                case SWQ_INTEGER64:
                case SWQ_INTEGER:
                    osVal = std::to_string(poNode->papoSubExpr[1]->int_value);
                    break;
                case SWQ_FLOAT:
                    osVal = std::to_string(poNode->papoSubExpr[1]->float_value);
                    break;
                case SWQ_STRING:
                    if (poNode->papoSubExpr[1]->string_value)
                    {
                        char *pszValueEncoded = CPLEscapeString(
                            poNode->papoSubExpr[1]->string_value, -1,
                            CPLES_URL);
                        osVal = pszValueEncoded;
                        CPLFree(pszValueEncoded);
                    }
                    break;
                case SWQ_DATE:
                case SWQ_TIME:
                case SWQ_TIMESTAMP:
                    if (poNode->papoSubExpr[1]->string_value)
                    {
                        char *pszValueEncoded = CPLEscapeString(
                            poNode->papoSubExpr[1]->string_value, -1,
                            CPLES_URL);
                        osVal = pszValueEncoded;
                        CPLFree(pszValueEncoded);
                    }
                    break;
                default:
                    break;
            }
            if (osFieldName.empty() || osVal.empty())
            {
                CPLDebug("NGW", "Unsupported filter operation for server side");
                return "";
            }

            return osFieldName + "=" + osVal;
        }
        else
        {
            CPLDebug("NGW", "Unsupported filter operation for server side");
            return "";
        }
    }
    return "";
}

/*
 * OGRNGWLayer()
 */
OGRNGWLayer::OGRNGWLayer(OGRNGWDataset *poDSIn,
                         const CPLJSONObject &oResourceJsonObject)
    : osResourceId(oResourceJsonObject.GetString("resource/id", "-1")),
      poDS(poDSIn), bFetchedPermissions(false), poFeatureDefn(nullptr),
      nFeatureCount(-1), oNextPos(moFeatures.begin()), nPageStart(0),
      bNeedSyncData(false), bNeedSyncStructure(false),
      bClientSideAttributeFilter(false)
{
    Fill(oResourceJsonObject);
}

/*
 * OGRNGWLayer()
 */
OGRNGWLayer::OGRNGWLayer(const std::string &osResourceIdIn,
                         OGRNGWDataset *poDSIn,
                         const NGWAPI::Permissions &stPermissionsIn,
                         OGRFeatureDefn *poFeatureDefnIn,
                         GIntBig nFeatureCountIn, const OGREnvelope &stExtentIn)
    : osResourceId(osResourceIdIn), poDS(poDSIn),
      stPermissions(stPermissionsIn), bFetchedPermissions(true),
      poFeatureDefn(poFeatureDefnIn), nFeatureCount(nFeatureCountIn),
      stExtent(stExtentIn), oNextPos(moFeatures.begin()), nPageStart(0),
      bNeedSyncData(false), bNeedSyncStructure(false),
      bClientSideAttributeFilter(false)
{
    poFeatureDefn->Reference();
    SetDescription(poFeatureDefn->GetName());
}

/*
 * OGRNGWLayer()
 */
OGRNGWLayer::OGRNGWLayer(OGRNGWDataset *poDSIn, const std::string &osNameIn,
                         OGRSpatialReference *poSpatialRef,
                         OGRwkbGeometryType eGType, const std::string &osKeyIn,
                         const std::string &osDescIn)
    : osResourceId("-1"), poDS(poDSIn), bFetchedPermissions(false),
      nFeatureCount(0), oNextPos(moFeatures.begin()), nPageStart(0),
      bNeedSyncData(false), bNeedSyncStructure(false),
      bClientSideAttributeFilter(false)
{
    poFeatureDefn = new OGRFeatureDefn(osNameIn.c_str());
    poFeatureDefn->Reference();

    poFeatureDefn->SetGeomType(eGType);

    if (poSpatialRef)
    {
        if (poFeatureDefn->GetGeomFieldCount() != 0)
        {
            poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSpatialRef);
        }
    }

    if (!osDescIn.empty())
    {
        OGRLayer::SetMetadataItem("description", osDescIn.c_str());
    }
    if (!osKeyIn.empty())
    {
        OGRLayer::SetMetadataItem("keyname", osKeyIn.c_str());
    }

    SetDescription(poFeatureDefn->GetName());

    stPermissions.bDatastructCanRead = true;
    stPermissions.bDatastructCanWrite = true;
    stPermissions.bDataCanRead = true;
    stPermissions.bDataCanWrite = true;
    stPermissions.bMetadataCanRead = true;
    stPermissions.bMetadataCanWrite = true;
}

/*
 * ~OGRNGWLayer()
 */
OGRNGWLayer::~OGRNGWLayer()
{
    FreeFeaturesCache(true);
    if (poFeatureDefn != nullptr)
    {
        poFeatureDefn->Release();
    }
}

/*
 * FreeFeaturesCache()
 */
void OGRNGWLayer::FreeFeaturesCache(bool bForce)
{
    if (!soChangedIds.empty())
    {
        bNeedSyncData = true;
    }

    if (SyncFeatures() == OGRERR_NONE || bForce)  // Try sync first
    {
        // Free only if synced with server successfully or executed from
        // destructor.
        FreeMap(moFeatures);
    }
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
    if (osResourceId == "-1")
    {
        return true;
    }

    // Headers free in DeleteResource method.
    return NGWAPI::DeleteResource(poDS->GetUrl(), osResourceId,
                                  poDS->GetHeaders(false));
}

/*
 * Rename()
 */
OGRErr OGRNGWLayer::Rename(const char *pszNewName)
{
    bool bResult = true;
    if (osResourceId != "-1")
    {
        bResult = NGWAPI::RenameResource(poDS->GetUrl(), osResourceId,
                                         pszNewName, poDS->GetHeaders(false));
    }
    if (bResult)
    {
        poFeatureDefn->SetName(pszNewName);
        SetDescription(poFeatureDefn->GetName());
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Rename layer to %s failed",
                 pszNewName);
    }
    return bResult ? OGRERR_NONE : OGRERR_FAILURE;
}

/*
 * ResetReading()
 */
void OGRNGWLayer::ResetReading()
{
    SyncToDisk();
    FreeFeaturesCache();
    if (poDS->GetPageSize() > 0)
    {
        nPageStart = 0;
    }
    oNextPos = moFeatures.begin();
}

CPLJSONObject OGRNGWLayer::LoadUrl(const std::string &osUrl) const
{
    CPLErrorReset();
    auto aosHTTPOptions = poDS->GetHeaders(false);
    CPLJSONDocument oFeatureReq;

    bool bResult = oFeatureReq.LoadUrl(osUrl, aosHTTPOptions);
    CPLJSONObject oRoot = oFeatureReq.GetRoot();
    if (NGWAPI::CheckRequestResult(bResult, oRoot,
                                   "GetFeatures request failed"))
    {
        CPLErrorReset();  // If we are here no error occurred
        return oRoot;
    }

    return CPLJSONObject();
}

/*
 * FillFeatures()
 */
bool OGRNGWLayer::FillFeatures(const std::string &osUrl)
{
    CPLDebug("NGW", "GetNextFeature: Url: %s", osUrl.c_str());

    CPLJSONObject oRoot = LoadUrl(osUrl);
    if (!oRoot.IsValid())
    {
        return false;
    }

    CPLJSONArray aoJSONFeatures = oRoot.ToArray();
    for (int i = 0; i < aoJSONFeatures.Size(); ++i)
    {
        OGRFeature *poFeature = JSONToFeature(aoJSONFeatures[i], poFeatureDefn,
                                              true, poDS->IsExtInNativeData());
        moFeatures[poFeature->GetFID()] = poFeature;
    }

    return true;
}

/*
 * SetNextByIndex()
 */
OGRErr OGRNGWLayer::SetNextByIndex(GIntBig nIndex)
{
    SyncToDisk();
    if (nIndex < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Feature index must be greater or equal 0. Got " CPL_FRMT_GIB,
                 nIndex);
        return OGRERR_FAILURE;
    }
    if (poDS->GetPageSize() > 0)
    {
        // Check if index is in current cache
        if (nPageStart > nIndex && nIndex <= nPageStart - poDS->GetPageSize())
        {
            if (moFeatures.empty() ||
                static_cast<GIntBig>(moFeatures.size()) <= nIndex)
            {
                oNextPos = moFeatures.end();
            }
            else
            {
                oNextPos = moFeatures.begin();
                std::advance(oNextPos, static_cast<size_t>(nIndex));
            }
        }
        else
        {
            ResetReading();
            nPageStart = nIndex;
        }
    }
    else
    {
        if (moFeatures.empty() && GetMaxFeatureCount(false) > 0)
        {
            std::string osUrl;
            if (poDS->HasFeaturePaging())
            {
                osUrl = NGWAPI::GetFeaturePageURL(
                    poDS->GetUrl(), osResourceId, 0, 0, osFields, osWhere,
                    osSpatialFilter, poDS->Extensions(),
                    poFeatureDefn->IsGeometryIgnored() == TRUE);
            }
            else
            {
                osUrl = NGWAPI::GetFeatureURL(poDS->GetUrl(), osResourceId);
            }

            if (!FillFeatures(osUrl))
            {
                return OGRERR_FAILURE;
            }
        }

        if (moFeatures.empty() ||
            static_cast<GIntBig>(moFeatures.size()) <= nIndex)
        {
            oNextPos = moFeatures.end();
        }
        else
        {
            oNextPos = moFeatures.begin();
            std::advance(oNextPos, static_cast<size_t>(nIndex));
        }
    }
    return OGRERR_NONE;
}

/*
 * GetNextFeature()
 */
OGRFeature *OGRNGWLayer::GetNextFeature()
{
    std::string osUrl;

    if (poDS->GetPageSize() > 0)
    {
        if (oNextPos == moFeatures.end() &&
            nPageStart < GetMaxFeatureCount(false))
        {
            FreeFeaturesCache();

            osUrl = NGWAPI::GetFeaturePageURL(
                poDS->GetUrl(), osResourceId, nPageStart, poDS->GetPageSize(),
                osFields, osWhere, osSpatialFilter, poDS->Extensions(),
                poFeatureDefn->IsGeometryIgnored() == TRUE);
            nPageStart += poDS->GetPageSize();
        }
    }
    else if (moFeatures.empty() && GetMaxFeatureCount(false) > 0)
    {
        if (poDS->HasFeaturePaging())
        {
            osUrl = NGWAPI::GetFeaturePageURL(
                poDS->GetUrl(), osResourceId, 0, 0, osFields, osWhere,
                osSpatialFilter, poDS->Extensions(),
                poFeatureDefn->IsGeometryIgnored() == TRUE);
        }
        else
        {
            osUrl = NGWAPI::GetFeatureURL(poDS->GetUrl(), osResourceId);
        }
    }

    bool bFinalRead = true;
    if (!osUrl.empty())
    {
        if (!FillFeatures(osUrl))
        {
            return nullptr;
        }

        oNextPos = moFeatures.begin();

        if (poDS->GetPageSize() < 1)
        {
            // Without paging we read all features at once.
            m_nFeaturesRead = moFeatures.size();
        }
        else
        {
            if (poDS->GetPageSize() - moFeatures.size() == 0)
            {
                m_nFeaturesRead = nPageStart;
                bFinalRead = false;
            }
            else
            {
                m_nFeaturesRead =
                    nPageStart - poDS->GetPageSize() + moFeatures.size();
            }
        }
    }

    while (oNextPos != moFeatures.end())
    {
        OGRFeature *poFeature = oNextPos->second;
        ++oNextPos;

        if (poFeature == nullptr)  // Feature may be deleted.
        {
            continue;
        }

        // Check local filters only for new features which not send to server
        // yet or if attribute filter process on client side.
        if (poFeature->GetFID() < 0 || bClientSideAttributeFilter)
        {
            if ((m_poFilterGeom == nullptr ||
                 FilterGeometry(poFeature->GetGeometryRef())) &&
                (m_poAttrQuery == nullptr ||
                 m_poAttrQuery->Evaluate(poFeature)))
            {
                return poFeature->Clone();
            }
        }
        else
        {
            return poFeature->Clone();
        }
    }

    if (poDS->GetPageSize() > 0 && !bFinalRead)
    {
        return GetNextFeature();
    }
    return nullptr;
}

/*
 * GetFeature()
 */
OGRFeature *OGRNGWLayer::GetFeature(GIntBig nFID)
{
    // Check feature in cache.
    if (moFeatures[nFID] != nullptr)
    {
        return moFeatures[nFID]->Clone();
    }
    auto osUrl = NGWAPI::GetFeatureURL(poDS->GetUrl(), osResourceId) +
                 std::to_string(nFID);
    CPLJSONObject oRoot = LoadUrl(osUrl);
    if (!oRoot.IsValid())
    {
        return nullptr;
    }

    // Don't store feature in cache. This can broke sequence read.
    return JSONToFeature(oRoot, poFeatureDefn, true, poDS->IsExtInNativeData());
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
int OGRNGWLayer::TestCapability(const char *pszCap)
{
    FetchPermissions();
    if (EQUAL(pszCap, OLCRandomRead))
        return TRUE;
    else if (EQUAL(pszCap, OLCSequentialWrite))
        return stPermissions.bDataCanWrite && poDS->IsUpdateMode();
    else if (EQUAL(pszCap, OLCRandomWrite))
        return stPermissions.bDataCanWrite && poDS->IsUpdateMode();
    else if (EQUAL(pszCap, OLCFastFeatureCount))
        return m_poFilterGeom == nullptr && m_poAttrQuery == nullptr;
    else if (EQUAL(pszCap, OLCFastGetExtent))
        return TRUE;
    else if (EQUAL(pszCap, OLCAlterFieldDefn))
        return stPermissions.bDatastructCanWrite && poDS->IsUpdateMode();
    else if (EQUAL(pszCap, OLCDeleteFeature))
        return stPermissions.bDataCanWrite && poDS->IsUpdateMode();
    else if (EQUAL(pszCap, OLCStringsAsUTF8))
        return TRUE;
    else if (EQUAL(pszCap, OLCFastSetNextByIndex))
        return TRUE;
    else if (EQUAL(pszCap, OLCCreateField))
        return stPermissions.bDatastructCanWrite && poDS->IsUpdateMode();
    else if (EQUAL(pszCap, OLCIgnoreFields))
        return poDS->HasFeaturePaging();  // Ignore fields, paging support and
                                          // attribute/spatial filters were
                                          // introduced in NGW v3.1
    else if (EQUAL(pszCap, OLCFastSpatialFilter))
        return poDS->HasFeaturePaging();
    else if (EQUAL(pszCap, OLCRename))
        return poDS->IsUpdateMode();
    else if (EQUAL(pszCap, OLCZGeometries))
        return TRUE;
    return FALSE;
}

/*
 * Fill()
 */
void OGRNGWLayer::Fill(const CPLJSONObject &oRootObject)
{
    auto osName = oRootObject.GetString("resource/display_name");
    CPLStringList soIgnoredFieldsNames;
    if (nullptr != poFeatureDefn)
    {
        while (poFeatureDefn->GetFieldCount() > 0)
        {
            OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn(0);
            if (poFieldDefn->IsIgnored())
            {
                soIgnoredFieldsNames.AddString(poFieldDefn->GetNameRef());
            }
            poFeatureDefn->DeleteFieldDefn(0);
        }
        poFeatureDefn->SetName(osName.c_str());

        poFeatureDefn->SetGeomType(NGWAPI::NGWGeomTypeToOGRGeomType(
            oRootObject.GetString("vector_layer/geometry_type")));
    }
    else
    {
        poFeatureDefn = new OGRFeatureDefn(osName.c_str());
        poFeatureDefn->Reference();

        poFeatureDefn->SetGeomType(NGWAPI::NGWGeomTypeToOGRGeomType(
            oRootObject.GetString("vector_layer/geometry_type")));

        OGRSpatialReference *poSRS = new OGRSpatialReference;
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        int nEPSG = oRootObject.GetInteger(
            "vector_layer/srs/id",
            3857);  // Default NGW SRS is Web mercator EPSG:3857.
        if (poSRS->importFromEPSG(nEPSG) == OGRERR_NONE)
        {
            if (poFeatureDefn->GetGeomFieldCount() != 0)
            {
                poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
            }
        }
        poSRS->Release();
    }

    CPLJSONArray oFields = oRootObject.GetArray("feature_layer/fields");
    FillFields(oFields, soIgnoredFieldsNames);
    FillMetadata(oRootObject);

    auto osDescription = oRootObject.GetString("resource/description");
    SetDescription(osDescription.c_str());
    // SetDescription(poFeatureDefn->GetName());
}

/*
 * FillMetadata()
 */
void OGRNGWLayer::FillMetadata(const CPLJSONObject &oRootObject)
{
    std::string osCreateDate = oRootObject.GetString("resource/creation_date");
    if (!osCreateDate.empty())
    {
        OGRLayer::SetMetadataItem("creation_date", osCreateDate.c_str());
    }
    std::string osDescription = oRootObject.GetString("resource/description");
    if (!osDescription.empty())
    {
        OGRLayer::SetMetadataItem("description", osDescription.c_str());
    }
    std::string osKeyName = oRootObject.GetString("resource/keyname");
    if (!osKeyName.empty())
    {
        OGRLayer::SetMetadataItem("keyname", osKeyName.c_str());
    }
    std::string osResourceType = oRootObject.GetString("resource/cls");
    if (!osResourceType.empty())
    {
        OGRLayer::SetMetadataItem("resource_type", osResourceType.c_str());
    }
    std::string osResourceParentId =
        oRootObject.GetString("resource/parent/id");
    if (!osResourceParentId.empty())
    {
        OGRLayer::SetMetadataItem("parent_id", osResourceParentId.c_str());
    }
    OGRLayer::SetMetadataItem("id", osResourceId.c_str());

    std::vector<CPLJSONObject> items =
        oRootObject.GetObj("resmeta/items").GetChildren();

    for (const CPLJSONObject &item : items)
    {
        std::string osSuffix = NGWAPI::GetResmetaSuffix(item.GetType());
        OGRLayer::SetMetadataItem((item.GetName() + osSuffix).c_str(),
                                  item.ToString().c_str(), "NGW");
    }
}

/*
 * FillFields()
 */
void OGRNGWLayer::FillFields(const CPLJSONArray &oFields,
                             const CPLStringList &soIgnoredFieldsNames)
{
    for (const auto &oField : oFields)
    {
        std::string osFieldName = oField.GetString("keyname");
        OGRFieldType eFieldtype =
            NGWAPI::NGWFieldTypeToOGRFieldType(oField.GetString("datatype"));

        OGRFieldDefn oFieldDefn(osFieldName.c_str(), eFieldtype);
        std::string osFieldAlias = oField.GetString("display_name");
        oFieldDefn.SetAlternativeName(osFieldAlias.c_str());

        // Add additional parameters to comment as JSON string
        CPLJSONObject oFieldComment;
        oFieldComment.Add("id", oField.GetLong("id"));
        oFieldComment.Add("label_field", oField.GetBool("label_field"));
        oFieldComment.Add("grid_visibility", oField.GetBool("grid_visibility"));
        oFieldComment.Add("text_search", oField.GetBool("text_search"));
        oFieldDefn.SetComment(
            oFieldComment.Format(CPLJSONObject::PrettyFormat::Plain));

        // Domain
        auto nDomainID = oField.GetInteger("lookup_table/id", -1);
        if (nDomainID != -1)
        {
            auto oDom = poDS->GetDomainByID(nDomainID);
            auto pOgrDom = oDom.ToFieldDomain(eFieldtype);
            if (pOgrDom != nullptr)
            {
                oFieldDefn.SetDomainName(pOgrDom->GetName());
            }
        }

        poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }

    OGRLayer::SetIgnoredFields(soIgnoredFieldsNames);
}

/*
 * GetMaxFeatureCount()
 */
GIntBig OGRNGWLayer::GetMaxFeatureCount(bool bForce)
{
    if (nFeatureCount < 0 || bForce)
    {
        CPLJSONObject oRoot =
            LoadUrl(NGWAPI::GetFeatureCount(poDS->GetUrl(), osResourceId));
        if (!oRoot.IsValid())
        {
            return nFeatureCount;
        }

        nFeatureCount = oRoot.GetLong("total_count");
        nFeatureCount += GetNewFeaturesCount();
    }
    return nFeatureCount;
}

/*
 * GetFeatureCount()
 */
GIntBig OGRNGWLayer::GetFeatureCount(int bForce)
{
    if (m_poFilterGeom == nullptr && m_poAttrQuery == nullptr)
    {
        return GetMaxFeatureCount(CPL_TO_BOOL(bForce));
    }
    else
    {
        return OGRLayer::GetFeatureCount(bForce);
    }
}

/*
 * IGetExtent()
 */
OGRErr OGRNGWLayer::IGetExtent(int /* iGeomField */, OGREnvelope *psExtent,
                               bool bForce)
{
    if (!stExtent.IsInit() || CPL_TO_BOOL(bForce))
    {
        auto aosHTTPOptions = poDS->GetHeaders(false);
        bool bResult = NGWAPI::GetExtent(poDS->GetUrl(), osResourceId,
                                         aosHTTPOptions, 3857, stExtent);
        if (!bResult)
        {
            return OGRERR_FAILURE;
        }
    }
    *psExtent = stExtent;
    return OGRERR_NONE;
}

/*
 * FetchPermissions()
 */
void OGRNGWLayer::FetchPermissions()
{
    if (bFetchedPermissions || osResourceId == "-1")
    {
        return;
    }

    if (poDS->IsUpdateMode())
    {
        auto aosHTTPOptions = poDS->GetHeaders(false);
        stPermissions = NGWAPI::CheckPermissions(
            poDS->GetUrl(), osResourceId, aosHTTPOptions, poDS->IsUpdateMode());
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

/*
 * CreateField()
 */
OGRErr OGRNGWLayer::CreateField(const OGRFieldDefn *poField,
                                CPL_UNUSED int bApproxOK)
{
    CPLAssert(nullptr != poField);

    if (!CheckFieldNameUnique(poFeatureDefn, -1, poField->GetNameRef()))
    {
        return OGRERR_FAILURE;
    }

    OGRFieldDefn oModFieldDefn(poField);
    poFeatureDefn->AddFieldDefn(&oModFieldDefn);
    bNeedSyncStructure = true;
    return OGRERR_NONE;
}

/*
 * DeleteField()
 */
OGRErr OGRNGWLayer::DeleteField(int iField)
{
    if (osResourceId != "-1")
    {
        auto poFieldDefn = poFeatureDefn->GetFieldDefn(iField);
        if (poFieldDefn == nullptr)
        {
            return OGRERR_FAILURE;
        }
        // Get field NGW ID
        CPLJSONDocument oComment;
        if (oComment.LoadMemory(poFieldDefn->GetComment()))
        {
            auto oRoot = oComment.GetRoot();
            auto nNGWID = oRoot.GetLong("id", -1);
            if (nNGWID != -1)
            {
                soDeletedFieldsIds.insert(nNGWID);
            }
        }
    }
    return poFeatureDefn->DeleteFieldDefn(iField);
}

/*
 * ReorderFields()
 */
OGRErr OGRNGWLayer::ReorderFields(int *panMap)
{
    return poFeatureDefn->ReorderFieldDefns(panMap);
}

/*
 * AlterFieldDefn()
 */
OGRErr OGRNGWLayer::AlterFieldDefn(int iField, OGRFieldDefn *poNewFieldDefn,
                                   int nFlagsIn)
{
    CPLDebug("NGW", "AlterFieldDefn fld #%d", iField);
    CPLAssert(nullptr != poNewFieldDefn);

    if (iField < 0 || iField >= poFeatureDefn->GetFieldCount())
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid field index");
        return OGRERR_FAILURE;
    }

    FetchPermissions();
    if (!stPermissions.bDatastructCanWrite)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Insufficient permissions");
        return OGRERR_FAILURE;
    }

    if (!poDS->IsUpdateMode())
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Layer is read only");
        return OGRERR_FAILURE;
    }

    OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn(iField);
    // Check new field name is not equal for another fields.
    if (!CheckFieldNameUnique(poFeatureDefn, iField,
                              poNewFieldDefn->GetNameRef()))
    {
        return OGRERR_FAILURE;
    }

    if (osResourceId == "-1")  // Can full alter field only on new layers
                               // (not synced with server).
    {
        // Field name 'id' forbidden.
        OGRFieldDefn oModFieldDefn(poNewFieldDefn);

        poFieldDefn->SetName(oModFieldDefn.GetNameRef());
        poFieldDefn->SetAlternativeName(oModFieldDefn.GetAlternativeNameRef());
        poFieldDefn->SetComment(oModFieldDefn.GetComment());
        poFieldDefn->SetType(oModFieldDefn.GetType());
        poFieldDefn->SetSubType(oModFieldDefn.GetSubType());
        poFieldDefn->SetWidth(oModFieldDefn.GetWidth());
        poFieldDefn->SetPrecision(oModFieldDefn.GetPrecision());
    }
    else
    {
        if (nFlagsIn & ALTER_NAME_FLAG)
        {
            bNeedSyncStructure = true;
            poFieldDefn->SetName(poNewFieldDefn->GetNameRef());
        }
        if (nFlagsIn & ALTER_DOMAIN_FLAG)
        {
            bNeedSyncStructure = true;
            poFieldDefn->SetDomainName(poNewFieldDefn->GetDomainName());
        }
        if (nFlagsIn & ALTER_ALTERNATIVE_NAME_FLAG)
        {
            bNeedSyncStructure = true;
            poFieldDefn->SetAlternativeName(
                poNewFieldDefn->GetAlternativeNameRef());
        }
        if (nFlagsIn & ALTER_COMMENT_FLAG)
        {
            bNeedSyncStructure = true;
            poFieldDefn->SetComment(poNewFieldDefn->GetComment());
        }
    }
    ResetReading();
    return OGRERR_NONE;
}

/*
 * SetMetadata()
 */
CPLErr OGRNGWLayer::SetMetadata(char **papszMetadata, const char *pszDomain)
{
    bNeedSyncStructure = true;
    return OGRLayer::SetMetadata(papszMetadata, pszDomain);
}

/*
 * SetMetadataItem()
 */
CPLErr OGRNGWLayer::SetMetadataItem(const char *pszName, const char *pszValue,
                                    const char *pszDomain)
{
    bNeedSyncStructure = true;
    return OGRLayer::SetMetadataItem(pszName, pszValue, pszDomain);
}

/*
 * CreateNGWResourceJson()
 */
std::string OGRNGWLayer::CreateNGWResourceJson()
{
    CPLJSONObject oResourceJson;

    // Add resource json item.
    CPLJSONObject oResource("resource", oResourceJson);
    oResource.Add("cls", "vector_layer");
    CPLJSONObject oResourceParent("parent", oResource);
    oResourceParent.Add("id",
                        static_cast<GIntBig>(std::stol(poDS->GetResourceId())));
    oResource.Add("display_name", GetName());
    const char *pszKeyName = GetMetadataItem("keyname");
    if (pszKeyName)
    {
        oResource.Add("keyname", pszKeyName);
    }
    const char *pszDescription = GetMetadataItem("description");
    if (pszDescription)
    {
        oResource.Add("description", pszDescription);
    }

    // Add vector_layer json item.
    CPLJSONObject oVectorLayer("vector_layer", oResourceJson);
    CPLJSONObject oVectorLayerSrs("srs", oVectorLayer);

    OGRSpatialReference *poSpatialRef = GetSpatialRef();
    int nEPSG = 3857;
    if (poSpatialRef)
    {
        poSpatialRef->AutoIdentifyEPSG();
        const char *pszEPSG = poSpatialRef->GetAuthorityCode(nullptr);
        if (pszEPSG != nullptr)
        {
            nEPSG = atoi(pszEPSG);
        }
    }
    oVectorLayerSrs.Add("id", nEPSG);
    // In OGRNGWDataset::ICreateLayer we limit supported geometry types.
    oVectorLayer.Add("geometry_type",
                     NGWAPI::OGRGeomTypeToNGWGeomType(GetGeomType()));

    // Fill fields
    CPLJSONArray oVectorLayerFields;
    for (int iField = 0; iField < poFeatureDefn->GetFieldCount(); ++iField)
    {
        OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn(iField);
        CPLJSONObject oField;
        auto const &sComment = poFieldDefn->GetComment();
        if (!sComment.empty())
        {
            CPLJSONDocument oComment;
            if (oComment.LoadMemory(sComment))
            {
                oField = oComment.GetRoot();
            }
        }
        oField.Add("keyname", poFieldDefn->GetNameRef());
        oField.Add("datatype",
                   NGWAPI::OGRFieldTypeToNGWFieldType(poFieldDefn->GetType()));
        std::string osFieldAliasName = poFieldDefn->GetAlternativeNameRef();
        // Get alias from metadata.
        if (osFieldAliasName.empty())
        {
            osFieldAliasName = poFieldDefn->GetNameRef();
        }
        oField.Add("display_name", osFieldAliasName);

        if (poFieldDefn->GetDomainName().empty())
        {
            oField.AddNull("lookup_table");
        }
        else
        {
            CPLJSONObject oFieldDom("lookup_table", oField);
            auto nDomId = poDS->GetDomainIdByName(poFieldDefn->GetDomainName());
            oFieldDom.Add("id", nDomId);
        }

        oVectorLayerFields.Add(oField);
    }
    for (auto nFieldID : soDeletedFieldsIds)
    {
        CPLJSONObject oField;
        oField.Add("id", nFieldID);
        oField.Add("delete", true);
        oVectorLayerFields.Add(oField);
    }

    CPLJSONObject oFeatureLayer("feature_layer", oResourceJson);
    oFeatureLayer.Add("fields", oVectorLayerFields);

    // Add resmeta json item.
    NGWAPI::FillResmeta(oResourceJson, GetMetadata("NGW"));

    return oResourceJson.Format(CPLJSONObject::PrettyFormat::Plain);
}

/*
 * SyncFeatures()
 */
OGRErr OGRNGWLayer::SyncFeatures()
{
    if (!bNeedSyncData)
    {
        return OGRERR_NONE;
    }

    CPLJSONArray oFeatureJsonArray;
    std::vector<GIntBig> aoPatchedFIDs;
    for (GIntBig nFID : soChangedIds)
    {
        if (moFeatures[nFID] != nullptr)
        {
            oFeatureJsonArray.Add(FeatureToJson(moFeatures[nFID]));
            aoPatchedFIDs.push_back(nFID);
        }
    }

    if (!aoPatchedFIDs.empty())
    {
        auto osIDs = NGWAPI::PatchFeatures(
            poDS->GetUrl(), osResourceId,
            oFeatureJsonArray.Format(CPLJSONObject::PrettyFormat::Plain),
            poDS->GetHeaders());
        if (!osIDs.empty())
        {
            bNeedSyncData = false;
            nFeatureCount += GetNewFeaturesCount();
            soChangedIds.clear();
            if (osIDs.size() !=
                aoPatchedFIDs.size())  // Expected equal identifier count.
            {
                CPLDebug("NGW", "Patched feature count is not equal. Reload "
                                "features from server.");
                FreeMap(moFeatures);
            }
            else  // Just update identifiers.
            {
                int nCounter = 0;
                for (GIntBig nFID : aoPatchedFIDs)
                {
                    GIntBig nNewFID = osIDs[nCounter++];
                    OGRFeature *poFeature = moFeatures[nFID];
                    poFeature->SetFID(nNewFID);
                    moFeatures.erase(nFID);
                    moFeatures[nNewFID] = poFeature;
                }
            }
        }
        else
        {
            // Error message should set in NGWAPI::PatchFeatures function.
            if (CPLGetLastErrorNo() != 0)
            {
                return OGRERR_FAILURE;
            }
        }
    }
    return OGRERR_NONE;
}

/*
 * SyncToDisk()
 */
OGRErr OGRNGWLayer::SyncToDisk()
{
    if (osResourceId == "-1")  // Create vector layer at NextGIS Web.
    {
        bNeedSyncData = !moFeatures.empty();
        std::string osResourceIdInt = NGWAPI::CreateResource(
            poDS->GetUrl(), CreateNGWResourceJson(), poDS->GetHeaders());
        if (osResourceIdInt == "-1")
        {
            // Error message should set in CreateResource.
            return OGRERR_FAILURE;
        }
        osResourceId = std::move(osResourceIdInt);
        OGRLayer::SetMetadataItem("id", osResourceId.c_str());
        FetchPermissions();
        bNeedSyncStructure = false;
    }
    else if (bNeedSyncStructure)  // Update vector layer at NextGIS Web.
    {
        if (!NGWAPI::UpdateResource(poDS->GetUrl(), GetResourceId(),
                                    CreateNGWResourceJson(),
                                    poDS->GetHeaders()))
        {
            // Error message should set in UpdateResource.
            return OGRERR_FAILURE;
        }
        bNeedSyncStructure = false;
        soDeletedFieldsIds.clear();
    }

    auto oRoot = LoadUrl(NGWAPI::GetResourceURL(poDS->GetUrl(), osResourceId));
    if (!oRoot.IsValid())
    {
        return OGRERR_FAILURE;
    }
    Fill(oRoot);

    // Update NGW layer state
    FetchPermissions();

    // Sync features.
    return SyncFeatures();
}

/*
 * DeleteFeature()
 */
OGRErr OGRNGWLayer::DeleteFeature(GIntBig nFID)
{
    CPLErrorReset();
    if (nFID < 0)
    {
        if (moFeatures[nFID] != nullptr)
        {
            OGRFeature::DestroyFeature(moFeatures[nFID]);
            moFeatures[nFID] = nullptr;
            nFeatureCount--;
            soChangedIds.erase(nFID);
            return OGRERR_NONE;
        }
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Feature with id " CPL_FRMT_GIB " not found.", nFID);
        return OGRERR_FAILURE;
    }
    else
    {
        FetchPermissions();
        if (stPermissions.bDataCanWrite && poDS->IsUpdateMode())
        {
            bool bResult = NGWAPI::DeleteFeature(poDS->GetUrl(), osResourceId,
                                                 std::to_string(nFID),
                                                 poDS->GetHeaders(false));
            if (bResult)
            {
                if (moFeatures[nFID] != nullptr)
                {
                    OGRFeature::DestroyFeature(moFeatures[nFID]);
                    moFeatures[nFID] = nullptr;
                }
                nFeatureCount--;
                soChangedIds.erase(nFID);
                return OGRERR_NONE;
            }
            return OGRERR_FAILURE;
        }
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Delete feature " CPL_FRMT_GIB " operation is not permitted.",
                 nFID);
        return OGRERR_FAILURE;
    }
}

/*
 * DeleteFeatures()
 */
OGRErr OGRNGWLayer::DeleteFeatures(const std::vector<GIntBig> &vFeaturesID)
{
    CPLErrorReset();

    // Try to delete local features (not synchronized with NGW)
    std::vector<GIntBig> vFeaturesIDInt(vFeaturesID);
    for (size_t i = 0; i < vFeaturesIDInt.size(); i++)
    {
        auto nFID = vFeaturesIDInt[i];
        if (nFID < 0)
        {
            if (moFeatures[nFID] != nullptr)
            {
                OGRFeature::DestroyFeature(moFeatures[nFID]);
                moFeatures[nFID] = nullptr;
                nFeatureCount--;
                soChangedIds.erase(nFID);
                vFeaturesIDInt.erase(vFeaturesIDInt.begin() + i);
            }
        }
    }

    FetchPermissions();
    if (stPermissions.bDataCanWrite && poDS->IsUpdateMode())
    {
        bool bResult = NGWAPI::DeleteFeatures(
            poDS->GetUrl(), osResourceId,
            FeaturesIDToJsonString(vFeaturesIDInt), poDS->GetHeaders(false));
        if (bResult)
        {
            for (GIntBig nFID : vFeaturesIDInt)
            {
                if (moFeatures[nFID] != nullptr)
                {
                    OGRFeature::DestroyFeature(moFeatures[nFID]);
                    moFeatures[nFID] = nullptr;
                    nFeatureCount--;
                    soChangedIds.erase(nFID);
                }
                nFeatureCount--;
                soChangedIds.erase(nFID);
            }
            return OGRERR_NONE;
        }
    }
    CPLError(CE_Failure, CPLE_AppDefined, "Delete features failed");
    return OGRERR_FAILURE;
}

/*
 * DeleteAllFeatures()
 */
bool OGRNGWLayer::DeleteAllFeatures()
{
    if (osResourceId == "-1")
    {
        soChangedIds.clear();
        bNeedSyncData = false;
        FreeFeaturesCache();
        nFeatureCount = 0;
        return true;
    }
    else
    {
        FetchPermissions();
        if (stPermissions.bDataCanWrite && poDS->IsUpdateMode())
        {
            bool bResult = NGWAPI::DeleteFeature(poDS->GetUrl(), osResourceId,
                                                 "", poDS->GetHeaders(false));
            if (bResult)
            {
                soChangedIds.clear();
                bNeedSyncData = false;
                FreeFeaturesCache();
                nFeatureCount = 0;
            }
            return bResult;
        }
    }
    CPLErrorReset();
    CPLError(CE_Failure, CPLE_AppDefined,
             "Delete all features operation is not permitted.");
    return false;
}

/*
 * ISetFeature()
 */
OGRErr OGRNGWLayer::ISetFeature(OGRFeature *poFeature)
{
    if (poDS->IsBatchMode())
    {
        if (moFeatures[poFeature->GetFID()] == nullptr)
        {
            if (poFeature->GetFID() < 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot update not existing feature " CPL_FRMT_GIB,
                         poFeature->GetFID());
                return OGRERR_FAILURE;
            }
        }
        else
        {
            OGRFeature::DestroyFeature(moFeatures[poFeature->GetFID()]);
        }
        moFeatures[poFeature->GetFID()] = poFeature->Clone();
        soChangedIds.insert(poFeature->GetFID());

        if (soChangedIds.size() > static_cast<size_t>(poDS->GetBatchSize()))
        {
            bNeedSyncData = true;
        }

        return SyncToDisk();
    }
    else
    {
        OGRErr eResult =
            SyncToDisk();  // For create new layer if not yet created.
        if (eResult == OGRERR_NONE)
        {
            if (poFeature->GetFID() < 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot update not existing feature " CPL_FRMT_GIB,
                         poFeature->GetFID());
                return OGRERR_FAILURE;
            }

            bool bResult = NGWAPI::UpdateFeature(
                poDS->GetUrl(), osResourceId,
                std::to_string(poFeature->GetFID()),
                FeatureToJsonString(poFeature), poDS->GetHeaders());
            if (bResult)
            {
                CPLDebug("NGW", "ISetFeature with FID " CPL_FRMT_GIB,
                         poFeature->GetFID());

                OGRFeature::DestroyFeature(moFeatures[poFeature->GetFID()]);
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
    if (poDS->IsBatchMode())
    {
        GIntBig nNewFID = -1;
        if (!soChangedIds.empty())
        {
            nNewFID = *(soChangedIds.begin()) - 1;
        }
        poFeature->SetFID(nNewFID);
        moFeatures[nNewFID] = poFeature->Clone();
        soChangedIds.insert(nNewFID);
        nFeatureCount++;

        if (soChangedIds.size() > static_cast<size_t>(poDS->GetBatchSize()))
        {
            bNeedSyncData = true;
        }

        return SyncToDisk();
    }
    else
    {
        OGRErr eResult =
            SyncToDisk();  // For create new layer if not yet created.
        if (eResult == OGRERR_NONE)
        {
            GIntBig nNewFID = NGWAPI::CreateFeature(
                poDS->GetUrl(), osResourceId, FeatureToJsonString(poFeature),
                poDS->GetHeaders());
            if (nNewFID >= 0)
            {
                poFeature->SetFID(nNewFID);
                moFeatures[nNewFID] = poFeature->Clone();
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

/*
 * SetIgnoredFields()
 */
OGRErr OGRNGWLayer::SetIgnoredFields(CSLConstList papszFields)
{
    OGRErr eResult = OGRLayer::SetIgnoredFields(papszFields);
    if (eResult != OGRERR_NONE)
    {
        return eResult;
    }

    osFields.clear();

    for (int iField = 0; iField < poFeatureDefn->GetFieldCount(); ++iField)
    {
        OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn(iField);
        if (poFieldDefn->IsIgnored())
        {
            CPLDebug("NGW", "SetIgnoredFields: Field '%s' set to ignored",
                     poFieldDefn->GetNameRef());
            continue;
        }

        if (osFields.empty())
        {
            osFields = poFieldDefn->GetNameRef();
        }
        else
        {
            osFields += "," + std::string(poFieldDefn->GetNameRef());
        }
    }

    // Encode osFields string for URL
    if (!osFields.empty())
    {
        char *pszValuesEncoded = CPLEscapeString(
            osFields.c_str(), static_cast<int>(osFields.size()), CPLES_URL);
        osFields = pszValuesEncoded;
        CPLFree(pszValuesEncoded);
    }

    CPLDebug("NGW", "SetIgnoredFields: NGW fields filter set to '%s'",
             osFields.c_str());

    ResetReading();
    return OGRERR_NONE;
}

/*
 * ISetSpatialFilter()
 */
OGRErr OGRNGWLayer::ISetSpatialFilter(int iGeomField, const OGRGeometry *poGeom)
{
    OGRLayer::ISetSpatialFilter(iGeomField, poGeom);

    if (nullptr == m_poFilterGeom)
    {
        CPLDebug("NGW", "Spatial filter unset");
        osSpatialFilter.clear();
    }
    else
    {
        OGREnvelope sEnvelope;
        m_poFilterGeom->getEnvelope(&sEnvelope);

        OGREnvelope sBigEnvelope;
        sBigEnvelope.MinX = -40000000.0;
        sBigEnvelope.MinY = -40000000.0;
        sBigEnvelope.MaxX = 40000000.0;
        sBigEnvelope.MaxY = 40000000.0;

        // Case for infinity filter
        if (sEnvelope.Contains(sBigEnvelope) == TRUE)
        {
            CPLDebug("NGW", "Spatial filter unset as filter envelope covers "
                            "whole features.");
            osSpatialFilter.clear();
        }
        else
        {
            if (sEnvelope.MinX == sEnvelope.MaxX &&
                sEnvelope.MinY == sEnvelope.MaxY)
            {
                OGRPoint p(sEnvelope.MinX, sEnvelope.MinY);
                InstallFilter(&p);
            }

            osSpatialFilter = OGRGeometryToWKT(m_poFilterGeom);
            CPLDebug("NGW", "Spatial filter: %s", osSpatialFilter.c_str());
            char *pszSpatFilterEncoded = CPLEscapeString(
                osSpatialFilter.c_str(),
                static_cast<int>(osSpatialFilter.size()), CPLES_URL);
            osSpatialFilter = pszSpatFilterEncoded;
            CPLFree(pszSpatFilterEncoded);
        }
    }

    ResetReading();

    return OGRERR_NONE;
}

/*
 * SetAttributeFilter()
 */
OGRErr OGRNGWLayer::SetAttributeFilter(const char *pszQuery)
{
    OGRErr eResult = OGRERR_NONE;
    if (nullptr == pszQuery)
    {
        eResult = OGRLayer::SetAttributeFilter(pszQuery);
        osWhere.clear();
        bClientSideAttributeFilter = false;
    }
    else if (STARTS_WITH_CI(pszQuery,
                            "NGW:"))  // Already formatted for NGW REST API
    {
        osWhere = pszQuery + strlen("NGW:");
        bClientSideAttributeFilter = false;
    }
    else
    {
        eResult = OGRLayer::SetAttributeFilter(pszQuery);
        if (eResult == OGRERR_NONE && m_poAttrQuery != nullptr)
        {
            swq_expr_node *poNode =
                reinterpret_cast<swq_expr_node *>(m_poAttrQuery->GetSWQExpr());
            std::string osWhereIn = TranslateSQLToFilter(poNode);
            if (osWhereIn.empty())
            {
                osWhere.clear();
                bClientSideAttributeFilter = true;
                CPLDebug(
                    "NGW",
                    "Attribute filter '%s' will be evaluated on client side.",
                    pszQuery);
            }
            else
            {
                bClientSideAttributeFilter = false;
                CPLDebug("NGW", "Attribute filter: %s", osWhereIn.c_str());
                osWhere = osWhereIn;
            }
        }
    }

    ResetReading();
    return eResult;
}

/*
 * SetSelectedFields()
 */
OGRErr OGRNGWLayer::SetSelectedFields(const std::set<std::string> &aosFields)
{
    CPLStringList aosIgnoreFields;
    for (int iField = 0; iField < poFeatureDefn->GetFieldCount(); ++iField)
    {
        OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn(iField);
        if (aosFields.find(poFieldDefn->GetNameRef()) != aosFields.end())
        {
            continue;
        }
        aosIgnoreFields.AddString(poFieldDefn->GetNameRef());
    }
    return SetIgnoredFields(aosIgnoreFields.List());
}

/*
 * Clone()
 */
OGRNGWLayer *OGRNGWLayer::Clone() const
{
    return new OGRNGWLayer(osResourceId, poDS, stPermissions,
                           poFeatureDefn->Clone(), nFeatureCount, stExtent);
}

/*
 * GetNewFeaturesCount()
 */
GIntBig OGRNGWLayer::GetNewFeaturesCount() const
{
    if (soChangedIds.empty())
    {
        return 0;
    }

    if (*soChangedIds.begin() >= 0)
    {
        return 0;
    }

    // The lowest negative identifier equal new feature count
    return *soChangedIds.begin() * -1;
}
