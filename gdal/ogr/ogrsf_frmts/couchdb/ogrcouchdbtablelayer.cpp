/******************************************************************************
 * $Id$
 *
 * Project:  CouchDB Translator
 * Purpose:  Implements OGRCouchDBTableLayer class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines dash paris dot org>
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

#include "ogr_couchdb.h"
#include "ogrgeojsonreader.h"
#include "ogrgeojsonwriter.h"
#include "json_object_private.h" // json_object_iter, complete type required
#include "swq.h"

#include <algorithm>

CPL_CVSID("$Id$");

/************************************************************************/
/*                       OGRCouchDBTableLayer()                         */
/************************************************************************/

OGRCouchDBTableLayer::OGRCouchDBTableLayer(OGRCouchDBDataSource* poDS,
                                           const char* pszName) :
                                                        OGRCouchDBLayer(poDS)

{
    osName = pszName;
    char* pszEscapedName = CPLEscapeString(pszName, -1, CPLES_URL);
    osEscapedName = pszEscapedName;
    CPLFree(pszEscapedName);

    bInTransaction = FALSE;

    eGeomType = wkbUnknown;

    nNextFIDForCreate = -1;
    bHasLoadedMetadata = FALSE;
    bMustWriteMetadata = FALSE;

    bMustRunSpatialFilter = FALSE;
    bServerSideSpatialFilteringWorks = TRUE;
    bHasOGRSpatial = -1;
    bHasGeocouchUtilsMinimalSpatialView = FALSE;

    bServerSideAttributeFilteringWorks = TRUE;
    bHasInstalledAttributeFilter = FALSE;

    nUpdateSeq = -1;
    bAlwaysValid = FALSE;

    bExtentValid = FALSE;
    bExtentSet = FALSE;
    dfMinX = 0;
    dfMinY = 0;
    dfMaxX = 0;
    dfMaxY = 0;

    nCoordPrecision = atoi(CPLGetConfigOption("OGR_COUCHDB_COORDINATE_PRECISION", "-1"));
}

/************************************************************************/
/*                      ~OGRCouchDBTableLayer()                         */
/************************************************************************/

OGRCouchDBTableLayer::~OGRCouchDBTableLayer()

{
    if( bMustWriteMetadata )
        WriteMetadata();

    for(int i=0;i<(int)aoTransactionFeatures.size();i++)
    {
        json_object_put(aoTransactionFeatures[i]);
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRCouchDBTableLayer::ResetReading()

{
    OGRCouchDBLayer::ResetReading();

    json_object_put(poFeatures);
    poFeatures = NULL;
    aoFeatures.resize(0);

    bMustRunSpatialFilter = m_poFilterGeom != NULL;
    aosIdsToFetch.resize(0);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRCouchDBTableLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCFastFeatureCount) )
        return m_poFilterGeom == NULL && m_poAttrQuery == NULL;

    else if( EQUAL(pszCap,OLCFastGetExtent) )
        return bExtentValid;

    else if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCSequentialWrite)
             || EQUAL(pszCap,OLCRandomWrite)
             || EQUAL(pszCap,OLCDeleteFeature) )
        return poDS->IsReadWrite();

    else if( EQUAL(pszCap,OLCCreateField) )
        return poDS->IsReadWrite();

    else if( EQUAL(pszCap, OLCTransactions) )
        return poDS->IsReadWrite();

    return OGRCouchDBLayer::TestCapability(pszCap);
}

/************************************************************************/
/*                   RunSpatialFilterQueryIfNecessary()                 */
/************************************************************************/

int OGRCouchDBTableLayer::RunSpatialFilterQueryIfNecessary()
{
    if (!bMustRunSpatialFilter)
        return TRUE;

    bMustRunSpatialFilter = FALSE;

    CPLAssert(nOffset == 0);

    aosIdsToFetch.resize(0);

    const char* pszSpatialFilter = NULL;
    if (bHasOGRSpatial < 0 || bHasOGRSpatial == FALSE)
    {
        pszSpatialFilter = CPLGetConfigOption("COUCHDB_SPATIAL_FILTER" , NULL);
        if (pszSpatialFilter)
            bHasOGRSpatial = FALSE;
    }

    if (bHasOGRSpatial < 0)
    {
        CPLString osURI("/");
        osURI += osEscapedName;
        osURI += "/_design/ogr_spatial";

        json_object* poAnswerObj = poDS->GET(osURI);
        bHasOGRSpatial = (poAnswerObj != NULL &&
            json_object_is_type(poAnswerObj, json_type_object) &&
            json_object_object_get(poAnswerObj, "spatial") != NULL);
        json_object_put(poAnswerObj);

        if (!bHasOGRSpatial)
        {
            /* Test if we have the 'minimal' spatial view provided by https://github.com/maxogden/geocouch-utils */
            osURI = "/";
            osURI += osEscapedName;
            osURI += "/_design/geo";

            json_object* poSpatialObj;
            poAnswerObj = poDS->GET(osURI);
            bHasGeocouchUtilsMinimalSpatialView = (poAnswerObj != NULL &&
                json_object_is_type(poAnswerObj, json_type_object) &&
                (poSpatialObj = json_object_object_get(poAnswerObj, "spatial")) != NULL &&
                json_object_is_type(poSpatialObj, json_type_object) &&
                json_object_object_get(poSpatialObj, "minimal") != NULL);
            json_object_put(poAnswerObj);

            if (!bHasGeocouchUtilsMinimalSpatialView)
            {
                CPLDebug("CouchDB",
                            "Geocouch not working --> client-side spatial filtering");
                bServerSideSpatialFilteringWorks = FALSE;
                return FALSE;
            }
        }
    }

    OGREnvelope sEnvelope;
    m_poFilterGeom->getEnvelope( &sEnvelope );

    if (bHasOGRSpatial)
        pszSpatialFilter = "_design/ogr_spatial/_spatial/spatial";
    else if (bHasGeocouchUtilsMinimalSpatialView)
        pszSpatialFilter = "_design/geo/_spatial/minimal";

    CPLString osURI("/");
    osURI += osEscapedName;
    osURI += "/";
    osURI += pszSpatialFilter;
    osURI += "?bbox=";
    osURI += CPLSPrintf("%.9f,%.9f,%.9f,%.9f",
                        sEnvelope.MinX, sEnvelope.MinY,
                        sEnvelope.MaxX, sEnvelope.MaxY);

    json_object* poAnswerObj = poDS->GET(osURI);
    if (poAnswerObj == NULL)
    {
        CPLDebug("CouchDB",
                    "Geocouch not working --> client-side spatial filtering");
        bServerSideSpatialFilteringWorks = FALSE;
        return FALSE;
    }

    if ( !json_object_is_type(poAnswerObj, json_type_object) )
    {
        CPLDebug("CouchDB",
                    "Geocouch not working --> client-side spatial filtering");
        bServerSideSpatialFilteringWorks = FALSE;
        CPLError(CE_Failure, CPLE_AppDefined,
                    "FetchNextRowsSpatialFilter() failed");
        json_object_put(poAnswerObj);
        return FALSE;
    }

    /* Catch error for a non geocouch database */
    json_object* poError = json_object_object_get(poAnswerObj, "error");
    json_object* poReason = json_object_object_get(poAnswerObj, "reason");

    const char* pszError = json_object_get_string(poError);
    const char* pszReason = json_object_get_string(poReason);

    if (pszError && pszReason && strcmp(pszError, "not_found") == 0 &&
        strcmp(pszReason, "Document is missing attachment") == 0)
    {
        CPLDebug("CouchDB",
                    "Geocouch not working --> client-side spatial filtering");
        bServerSideSpatialFilteringWorks = FALSE;
        json_object_put(poAnswerObj);
        return FALSE;
    }

    if (poDS->IsError(poAnswerObj, "FetchNextRowsSpatialFilter() failed"))
    {
        CPLDebug("CouchDB",
                    "Geocouch not working --> client-side spatial filtering");
        bServerSideSpatialFilteringWorks = FALSE;
        json_object_put(poAnswerObj);
        return FALSE;
    }

    json_object* poRows = json_object_object_get(poAnswerObj, "rows");
    if (poRows == NULL ||
        !json_object_is_type(poRows, json_type_array))
    {
        CPLDebug("CouchDB",
                    "Geocouch not working --> client-side spatial filtering");
        bServerSideSpatialFilteringWorks = FALSE;
        CPLError(CE_Failure, CPLE_AppDefined,
                    "FetchNextRowsSpatialFilter() failed");
        json_object_put(poAnswerObj);
        return FALSE;
    }

    int nRows = json_object_array_length(poRows);
    for(int i=0;i<nRows;i++)
    {
        json_object* poRow = json_object_array_get_idx(poRows, i);
        if ( poRow == NULL ||
            !json_object_is_type(poRow, json_type_object) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "FetchNextRowsSpatialFilter() failed");
            json_object_put(poAnswerObj);
            return FALSE;
        }

        json_object* poId = json_object_object_get(poRow, "id");
        const char* pszId = json_object_get_string(poId);
        if (pszId != NULL)
        {
            aosIdsToFetch.push_back(pszId);
        }
    }

    std::sort(aosIdsToFetch.begin(), aosIdsToFetch.end());

    json_object_put(poAnswerObj);

    return TRUE;
}

/************************************************************************/
/*                   FetchNextRowsSpatialFilter()                       */
/************************************************************************/

int OGRCouchDBTableLayer::FetchNextRowsSpatialFilter()
{
    if (!RunSpatialFilterQueryIfNecessary())
        return FALSE;

    CPLString osContent("{\"keys\":[");
    int nLimit = MIN(nOffset + GetFeaturesToFetch(), (int)aosIdsToFetch.size());
    for(int i=nOffset;i<nLimit;i++)
    {
        if (i > nOffset)
            osContent += ",";
        osContent += "\"";
        osContent += aosIdsToFetch[i];
        osContent += "\"";
    }
    osContent += "]}";

    CPLString osURI("/");
    osURI += osEscapedName;
    osURI += "/_all_docs?include_docs=true";
    json_object* poAnswerObj = poDS->POST(osURI, osContent);
    return FetchNextRowsAnalyseDocs(poAnswerObj);
}

/************************************************************************/
/*                HasFilterOnFieldOrCreateIfNecessary()                 */
/************************************************************************/

int OGRCouchDBTableLayer::HasFilterOnFieldOrCreateIfNecessary(const char* pszFieldName)
{
    std::map<CPLString, int>::iterator oIter = oMapFilterFields.find(pszFieldName);
    if (oIter != oMapFilterFields.end())
        return oIter->second;

    CPLString osURI("/");
    osURI += osEscapedName;
    osURI += "/_design/ogr_filter_";
    osURI += pszFieldName;

    int bFoundFilter = FALSE;

    json_object* poAnswerObj = poDS->GET(osURI);
    if (poAnswerObj &&
        json_object_is_type(poAnswerObj, json_type_object) &&
        json_object_object_get(poAnswerObj, "views") != NULL)
    {
        bFoundFilter = TRUE;
    }
    json_object_put(poAnswerObj);

    if (!bFoundFilter)
    {
        json_object* poDoc = json_object_new_object();
        json_object* poViews = json_object_new_object();
        json_object* poFilter = json_object_new_object();

        CPLString osMap;

        OGRFieldDefn* poFieldDefn = poFeatureDefn->GetFieldDefn(
                            poFeatureDefn->GetFieldIndex(pszFieldName));
        CPLAssert(poFieldDefn);
        int bIsNumeric = poFieldDefn->GetType() == OFTInteger ||
                         poFieldDefn->GetType() == OFTReal;

        if (bGeoJSONDocument)
        {
            osMap = "function(doc) { if (doc.properties && doc.properties.";
            osMap += pszFieldName;
            if (bIsNumeric)
            {
                osMap += " && typeof doc.properties.";
                osMap += pszFieldName;
                osMap += " == \"number\"";
            }
            osMap += ") emit(";
            osMap += "doc.properties.";
            osMap += pszFieldName;
            osMap +=", ";
            if (bIsNumeric)
            {
                osMap += "doc.properties.";
                osMap += pszFieldName;
            }
            else
                osMap +="null";
            osMap += "); }";
        }
        else
        {
            osMap = "function(doc) { if (doc.";
            osMap += pszFieldName;
            if (bIsNumeric)
            {
                osMap += " && typeof doc.";
                osMap += pszFieldName;
                osMap += " == \"number\"";
            }
            osMap += ") emit(";
            osMap += "doc.";
            osMap += pszFieldName;
            osMap +=", ";
            if (bIsNumeric)
            {
                osMap += "doc.";
                osMap += pszFieldName;
            }
            else
                osMap +="null";
            osMap += "); }";
        }

        json_object_object_add(poDoc, "views", poViews);
        json_object_object_add(poViews, "filter", poFilter);
        json_object_object_add(poFilter, "map", json_object_new_string(osMap));

        if (bIsNumeric)
            json_object_object_add(poFilter, "reduce", json_object_new_string("_stats"));
        else
            json_object_object_add(poFilter, "reduce", json_object_new_string("_count"));

        json_object* poAnswerObj = poDS->PUT(osURI,
                                            json_object_to_json_string(poDoc));

        json_object_put(poDoc);

        if (poDS->IsOK(poAnswerObj, "Filter creation failed"))
        {
            bFoundFilter = TRUE;
            if (!bAlwaysValid)
                bMustWriteMetadata = TRUE;
            nUpdateSeq++;
        }

        json_object_put(poAnswerObj);
    }

    oMapFilterFields[pszFieldName] = bFoundFilter;

    return bFoundFilter;
}

/************************************************************************/
/*                         OGRCouchDBGetOpStr()                         */
/************************************************************************/

static const char* OGRCouchDBGetOpStr(int nOperation,
                                      int& bOutHasStrictComparisons)
{
    bOutHasStrictComparisons = FALSE;

    switch(nOperation)
    {
        case SWQ_EQ: return "=";
        case SWQ_GE: return ">=";
        case SWQ_LE: return "<=";
        case SWQ_GT: bOutHasStrictComparisons = TRUE; return ">";
        case SWQ_LT: bOutHasStrictComparisons = TRUE; return "<";
        default:     return "unknown op";
    }
}

/************************************************************************/
/*                        OGRCouchDBGetValue()                          */
/************************************************************************/

static CPLString OGRCouchDBGetValue(swq_field_type eType,
                                    swq_expr_node* poNode)
{
    if (eType == SWQ_STRING)
    {
        CPLString osVal("\"");
        osVal += poNode->string_value;
        osVal += "\"";
        return osVal;
    }
    else if (eType == SWQ_INTEGER)
    {
        return CPLSPrintf("%d", poNode->int_value);
    }
    else if (eType == SWQ_FLOAT)
    {
        return CPLSPrintf("%.9f", poNode->float_value);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Handled case! File a bug!");
        return "";
    }
}

/************************************************************************/
/*                        OGRCouchDBGetKeyName()                          */
/************************************************************************/

static const char* OGRCouchDBGetKeyName(int nOperation)
{
    if (nOperation == SWQ_EQ)
    {
        return "key";
    }
    else if (nOperation == SWQ_GE ||
             nOperation == SWQ_GT)
    {
        return "startkey";
    }
    else if (nOperation == SWQ_LE ||
             nOperation == SWQ_LT)
    {
        return "endkey";
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Handled case! File a bug!");
        return "";
    }
}
/************************************************************************/
/*                         BuildAttrQueryURI()                          */
/************************************************************************/

CPLString OGRCouchDBTableLayer::BuildAttrQueryURI(int& bOutHasStrictComparisons)
{
    CPLString osURI = "";

    bOutHasStrictComparisons = FALSE;

    int bCanHandleFilter = FALSE;

    swq_expr_node * pNode = (swq_expr_node *) m_poAttrQuery->GetSWGExpr();
    if (pNode->eNodeType == SNT_OPERATION &&
        (pNode->nOperation == SWQ_EQ ||
            pNode->nOperation == SWQ_GE ||
            pNode->nOperation == SWQ_LE ||
            pNode->nOperation == SWQ_GT ||
            pNode->nOperation == SWQ_LT) &&
        pNode->nSubExprCount == 2 &&
        pNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
        pNode->papoSubExpr[1]->eNodeType == SNT_CONSTANT)
    {
        int nIndex = pNode->papoSubExpr[0]->field_index;
        swq_field_type eType = pNode->papoSubExpr[1]->field_type;
        const char* pszFieldName = poFeatureDefn->GetFieldDefn(nIndex)->GetNameRef();

        if (pNode->nOperation == SWQ_EQ &&
            nIndex == _ID_FIELD && eType == SWQ_STRING)
        {
            bCanHandleFilter = TRUE;

            osURI = "/";
            osURI += osEscapedName;
            osURI += "/_all_docs?";
        }
        else if (nIndex >= FIRST_FIELD &&
            (eType == SWQ_STRING || eType == SWQ_INTEGER || eType == SWQ_FLOAT))
        {
            int bFoundFilter = HasFilterOnFieldOrCreateIfNecessary(pszFieldName);
            if (bFoundFilter)
            {
                bCanHandleFilter = TRUE;

                osURI = "/";
                osURI += osEscapedName;
                osURI += "/_design/ogr_filter_";
                osURI += pszFieldName;
                osURI += "/_view/filter?";
            }
        }

        if (bCanHandleFilter)
        {
            const char* pszOp = OGRCouchDBGetOpStr(pNode->nOperation, bOutHasStrictComparisons);
            CPLString osVal = OGRCouchDBGetValue(eType, pNode->papoSubExpr[1]);
            CPLDebug("CouchDB", "Evaluating %s %s %s", pszFieldName, pszOp, osVal.c_str());

            osURI += OGRCouchDBGetKeyName(pNode->nOperation);
            osURI += "=";
            osURI += osVal;
        }
    }
    else if (pNode->eNodeType == SNT_OPERATION &&
                pNode->nOperation == SWQ_AND &&
                pNode->nSubExprCount == 2 &&
                pNode->papoSubExpr[0]->eNodeType == SNT_OPERATION &&
                pNode->papoSubExpr[1]->eNodeType == SNT_OPERATION &&
                (((pNode->papoSubExpr[0]->nOperation == SWQ_GE ||
                pNode->papoSubExpr[0]->nOperation == SWQ_GT) &&
                (pNode->papoSubExpr[1]->nOperation == SWQ_LE ||
                pNode->papoSubExpr[1]->nOperation == SWQ_LT)) ||
                ((pNode->papoSubExpr[0]->nOperation == SWQ_LE ||
                pNode->papoSubExpr[0]->nOperation == SWQ_LT) &&
                (pNode->papoSubExpr[1]->nOperation == SWQ_GE ||
                pNode->papoSubExpr[1]->nOperation == SWQ_GT))) &&
            pNode->papoSubExpr[0]->nSubExprCount == 2 &&
            pNode->papoSubExpr[1]->nSubExprCount == 2 &&
            pNode->papoSubExpr[0]->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
            pNode->papoSubExpr[0]->papoSubExpr[1]->eNodeType == SNT_CONSTANT &&
            pNode->papoSubExpr[1]->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
            pNode->papoSubExpr[1]->papoSubExpr[1]->eNodeType == SNT_CONSTANT)
    {
        int nIndex0 = pNode->papoSubExpr[0]->papoSubExpr[0]->field_index;
        swq_field_type eType0 = pNode->papoSubExpr[0]->papoSubExpr[1]->field_type;
        int nIndex1 = pNode->papoSubExpr[1]->papoSubExpr[0]->field_index;
        swq_field_type eType1 = pNode->papoSubExpr[1]->papoSubExpr[1]->field_type;
        const char* pszFieldName = poFeatureDefn->GetFieldDefn(nIndex0)->GetNameRef();

        if (nIndex0 == nIndex1 && eType0 == eType1 &&
            nIndex0 == _ID_FIELD && eType0 == SWQ_STRING)
        {
            bCanHandleFilter = TRUE;

            osURI = "/";
            osURI += osEscapedName;
            osURI += "/_all_docs?";
        }
        else if (nIndex0 == nIndex1 && eType0 == eType1 &&
            nIndex0 >= FIRST_FIELD &&
            (eType0 == SWQ_STRING || eType0 == SWQ_INTEGER || eType0 == SWQ_FLOAT))
        {
            int bFoundFilter = HasFilterOnFieldOrCreateIfNecessary(pszFieldName);
            if (bFoundFilter)
            {
                bCanHandleFilter = TRUE;

                osURI = "/";
                osURI += osEscapedName;
                osURI += "/_design/ogr_filter_";
                osURI += pszFieldName;
                osURI += "/_view/filter?";
            }
        }

        if (bCanHandleFilter)
        {
            swq_field_type eType = eType0;
            CPLString osVal0 = OGRCouchDBGetValue(eType, pNode->papoSubExpr[0]->papoSubExpr[1]);
            CPLString osVal1 = OGRCouchDBGetValue(eType, pNode->papoSubExpr[1]->papoSubExpr[1]);

            int nOperation0 = pNode->papoSubExpr[0]->nOperation;
            int nOperation1 = pNode->papoSubExpr[1]->nOperation;

            const char* pszOp0 = OGRCouchDBGetOpStr(nOperation0, bOutHasStrictComparisons);
            const char* pszOp1 = OGRCouchDBGetOpStr(nOperation1, bOutHasStrictComparisons);

            CPLDebug("CouchDB", "Evaluating %s %s %s AND %s %s %s",
                            pszFieldName, pszOp0, osVal0.c_str(),
                            pszFieldName, pszOp1, osVal1.c_str());

            osURI += OGRCouchDBGetKeyName(nOperation0);
            osURI += "=";
            osURI += osVal0;
            osURI += "&";
            osURI += OGRCouchDBGetKeyName(nOperation1);
            osURI += "=";
            osURI += osVal1;
        }
    }
    else if (pNode->eNodeType == SNT_OPERATION &&
                pNode->nOperation == SWQ_BETWEEN &&
                pNode->nSubExprCount == 3 &&
                pNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
                pNode->papoSubExpr[1]->eNodeType == SNT_CONSTANT &&
                pNode->papoSubExpr[2]->eNodeType == SNT_CONSTANT)
    {
        int nIndex = pNode->papoSubExpr[0]->field_index;
        swq_field_type eType = pNode->papoSubExpr[0]->field_type;
        const char* pszFieldName = poFeatureDefn->GetFieldDefn(nIndex)->GetNameRef();

        if (nIndex == _ID_FIELD && eType == SWQ_STRING)
        {
            bCanHandleFilter = TRUE;

            osURI = "/";
            osURI += osEscapedName;
            osURI += "/_all_docs?";
        }
        else if (nIndex >= FIRST_FIELD &&
            (eType == SWQ_STRING || eType == SWQ_INTEGER || eType == SWQ_FLOAT))
        {
            int bFoundFilter = HasFilterOnFieldOrCreateIfNecessary(pszFieldName);
            if (bFoundFilter)
            {
                bCanHandleFilter = TRUE;

                osURI = "/";
                osURI += osEscapedName;
                osURI += "/_design/ogr_filter_";
                osURI += pszFieldName;
                osURI += "/_view/filter?";
            }
        }

        if (bCanHandleFilter)
        {
            CPLString osVal0 = OGRCouchDBGetValue(eType, pNode->papoSubExpr[1]);
            CPLString osVal1 = OGRCouchDBGetValue(eType, pNode->papoSubExpr[2]);

            CPLDebug("CouchDB", "Evaluating %s BETWEEN %s AND %s",
                        pszFieldName, osVal0.c_str(), osVal1.c_str());

            osURI += OGRCouchDBGetKeyName(SWQ_GE);
            osURI += "=";
            osURI += osVal0;
            osURI += "&";
            osURI += OGRCouchDBGetKeyName(SWQ_LE);
            osURI += "=";
            osURI += osVal1;
        }
    }

    return osURI;
}

/************************************************************************/
/*                   FetchNextRowsAttributeFilter()                     */
/************************************************************************/

int OGRCouchDBTableLayer::FetchNextRowsAttributeFilter()
{
    if (bHasInstalledAttributeFilter)
    {
        bHasInstalledAttributeFilter = FALSE;

        CPLAssert(nOffset == 0);

        int bOutHasStrictComparisons = FALSE;
        osURIAttributeFilter = BuildAttrQueryURI(bOutHasStrictComparisons);

        if (osURIAttributeFilter.size() == 0)
        {
            CPLDebug("CouchDB",
                     "Turning to client-side attribute filtering");
            bServerSideAttributeFilteringWorks = FALSE;
            return FALSE;
        }
    }

    CPLString osURI(osURIAttributeFilter);
    osURI += CPLSPrintf("&limit=%d&skip=%d&include_docs=true",
                        GetFeaturesToFetch(), nOffset);
    if (strstr(osURI, "/_all_docs?") == NULL)
        osURI += "&reduce=false";
    json_object* poAnswerObj = poDS->GET(osURI);
    return FetchNextRowsAnalyseDocs(poAnswerObj);
}

/************************************************************************/
/*                           FetchNextRows()                            */
/************************************************************************/

int OGRCouchDBTableLayer::FetchNextRows()
{
    json_object_put(poFeatures);
    poFeatures = NULL;
    aoFeatures.resize(0);

    if( m_poFilterGeom != NULL && bServerSideSpatialFilteringWorks )
    {
        int bRet = FetchNextRowsSpatialFilter();
        if (bRet || bServerSideSpatialFilteringWorks)
            return bRet;
    }

    if( m_poAttrQuery != NULL && bServerSideAttributeFilteringWorks )
    {
        int bRet = FetchNextRowsAttributeFilter();
        if (bRet || bServerSideAttributeFilteringWorks)
            return bRet;
    }

    CPLString osURI("/");
    osURI += osEscapedName;
    osURI += CPLSPrintf("/_all_docs?limit=%d&skip=%d&include_docs=true",
                        GetFeaturesToFetch(), nOffset);
    json_object* poAnswerObj = poDS->GET(osURI);
    return FetchNextRowsAnalyseDocs(poAnswerObj);
}


/************************************************************************/
/*                            GetFeature()                              */
/************************************************************************/

OGRFeature * OGRCouchDBTableLayer::GetFeature( long nFID )
{
    GetLayerDefn();

    return GetFeature(CPLSPrintf("%09d", (int)nFID));
}

/************************************************************************/
/*                            GetFeature()                              */
/************************************************************************/

OGRFeature * OGRCouchDBTableLayer::GetFeature( const char* pszId )
{
    GetLayerDefn();

    CPLString osURI("/");
    osURI += osEscapedName;
    osURI += "/";
    osURI += pszId;
    json_object* poAnswerObj = poDS->GET(osURI);
    if (poAnswerObj == NULL)
        return NULL;

    if ( !json_object_is_type(poAnswerObj, json_type_object) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "GetFeature(%s) failed",
                 pszId);
        json_object_put(poAnswerObj);
        return NULL;
    }

    if ( poDS->IsError(poAnswerObj, CPLSPrintf("GetFeature(%s) failed", pszId)) )
    {
        json_object_put(poAnswerObj);
        return NULL;
    }

    OGRFeature* poFeature = TranslateFeature( poAnswerObj );

    json_object_put( poAnswerObj );

    return poFeature;
}

/************************************************************************/
/*                           GetLayerDefn()                             */
/************************************************************************/

OGRFeatureDefn * OGRCouchDBTableLayer::GetLayerDefn()
{
    if (poFeatureDefn == NULL)
        LoadMetadata();

    if (poFeatureDefn == NULL)
    {
        poFeatureDefn = new OGRFeatureDefn( osName );
        poFeatureDefn->Reference();

        poFeatureDefn->SetGeomType(eGeomType);

        OGRFieldDefn oFieldId("_id", OFTString);
        poFeatureDefn->AddFieldDefn(&oFieldId);

        OGRFieldDefn oFieldRev("_rev", OFTString);
        poFeatureDefn->AddFieldDefn(&oFieldRev);

        if (nNextFIDForCreate == 0)
        {
            return poFeatureDefn;
        }

        CPLString osURI("/");
        osURI += osEscapedName;
        osURI += "/_all_docs?limit=10&include_docs=true";
        json_object* poAnswerObj = poDS->GET(osURI);
        if (poAnswerObj == NULL)
            return poFeatureDefn;

        BuildFeatureDefnFromRows(poAnswerObj);

        eGeomType = poFeatureDefn->GetGeomType();

        json_object_put(poAnswerObj);
    }

    return poFeatureDefn;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRCouchDBTableLayer::GetFeatureCount(int bForce)
{
    GetLayerDefn();

    if (m_poFilterGeom == NULL && m_poAttrQuery != NULL)
    {
        int bOutHasStrictComparisons = FALSE;
        CPLString osURI = BuildAttrQueryURI(bOutHasStrictComparisons);
        if (!bOutHasStrictComparisons && osURI.size() != 0 &&
            strstr(osURI, "/_all_docs?") == NULL)
        {
            osURI += "&reduce=true";
            json_object* poAnswerObj = poDS->GET(osURI);
            json_object* poRows = NULL;
            if (poAnswerObj != NULL &&
                json_object_is_type(poAnswerObj, json_type_object) &&
                (poRows = json_object_object_get(poAnswerObj, "rows")) != NULL &&
                json_object_is_type(poRows, json_type_array))
            {
                int nLength = json_object_array_length(poRows);
                if (nLength == 0)
                {
                    json_object_put(poAnswerObj);
                    return 0;
                }
                else if (nLength == 1)
                {
                    json_object* poRow = json_object_array_get_idx(poRows, 0);
                    if (poRow && json_object_is_type(poRow, json_type_object))
                    {
                        /* for string fields */
                        json_object* poValue = json_object_object_get(poRow, "value");
                        if (poValue != NULL && json_object_is_type(poValue, json_type_int))
                        {
                            int nVal = json_object_get_int(poValue);
                            json_object_put(poAnswerObj);
                            return nVal;
                        }
                        else if (poValue != NULL && json_object_is_type(poValue, json_type_object))
                        {
                            /* for numeric fields */
                            json_object* poCount = json_object_object_get(poValue, "count");
                            if (poCount != NULL && json_object_is_type(poCount, json_type_int))
                            {
                                int nVal = json_object_get_int(poCount);
                                json_object_put(poAnswerObj);
                                return nVal;
                            }
                        }
                    }
                }
            }
            json_object_put(poAnswerObj);
        }
    }

    if (m_poFilterGeom != NULL && m_poAttrQuery == NULL &&
        wkbFlatten(eGeomType) == wkbPoint)
    {
        /* Only optimize for wkbPoint case. Otherwise the result might be higher */
        /* than the real value since the intersection of the bounding box of the */
        /* geometry of a feature does not necessary mean the intersection of the */
        /* geometry itself */
        RunSpatialFilterQueryIfNecessary();
        if (bServerSideSpatialFilteringWorks)
        {
            return (int)aosIdsToFetch.size();
        }
    }

    if (m_poFilterGeom != NULL || m_poAttrQuery != NULL)
        return OGRCouchDBLayer::GetFeatureCount(bForce);

    return GetTotalFeatureCount();
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRCouchDBTableLayer::GetTotalFeatureCount()
{
    int nTotalRows = -1;

    CPLString osURI("/");
    osURI += osEscapedName;
    osURI += "/_all_docs?startkey_docid=_&endkey_docid=_zzzzzzzzzzzzzzz";
    json_object* poAnswerObj = poDS->GET(osURI);
    if (poAnswerObj == NULL)
        return nTotalRows;

    if ( !json_object_is_type(poAnswerObj, json_type_object) )
    {
        json_object_put(poAnswerObj);
        return nTotalRows;
    }

    json_object* poTotalRows = json_object_object_get(poAnswerObj,
                                                        "total_rows");
    if (poTotalRows != NULL &&
        json_object_is_type(poTotalRows, json_type_int))
    {
        nTotalRows = json_object_get_int(poTotalRows);
    }

    json_object* poRows = json_object_object_get(poAnswerObj, "rows");
    if (poRows == NULL ||
        !json_object_is_type(poRows, json_type_array))
    {
        json_object_put(poAnswerObj);
        return nTotalRows;
    }

    bHasOGRSpatial = FALSE;

    int nSpecialRows = json_object_array_length(poRows);
    for(int i=0;i<nSpecialRows;i++)
    {
        json_object* poRow = json_object_array_get_idx(poRows, i);
        if ( poRow != NULL &&
             json_object_is_type(poRow, json_type_object) )
        {
            json_object* poId = json_object_object_get(poRow, "id");
            const char* pszId = json_object_get_string(poId);
            if ( pszId && strcmp(pszId, "_design/ogr_spatial") == 0)
            {
                bHasOGRSpatial = TRUE;
            }
        }
    }

    if (!bHasOGRSpatial)
    {
        bServerSideSpatialFilteringWorks = FALSE;
    }

    if (nTotalRows >= nSpecialRows)
        nTotalRows -= nSpecialRows;

    json_object_put(poAnswerObj);

    return nTotalRows;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRCouchDBTableLayer::CreateField( OGRFieldDefn *poField,
                                 int bApproxOK )
{

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    GetLayerDefn();

    poFeatureDefn->AddFieldDefn(poField);

    bMustWriteMetadata = TRUE;

    return OGRERR_NONE;
}

/************************************************************************/
/*                         OGRCouchDBWriteFeature                       */
/************************************************************************/

static json_object* OGRCouchDBWriteFeature( OGRFeature* poFeature,
                                            OGRwkbGeometryType eGeomType,
                                            int bGeoJSONDocument,
                                            int nCoordPrecision )
{
    CPLAssert( NULL != poFeature );

    json_object* poObj = json_object_new_object();
    CPLAssert( NULL != poObj );

    if (poFeature->IsFieldSet(_ID_FIELD))
    {
        const char* pszId = poFeature->GetFieldAsString(_ID_FIELD);
        json_object_object_add( poObj, "_id",
                                json_object_new_string(pszId) );

        if ( poFeature->GetFID() != OGRNullFID &&
             strcmp(CPLSPrintf("%09ld", poFeature->GetFID()), pszId) != 0 )
        {
            CPLDebug("CouchDB",
                     "_id field = %s, but FID = %09ld --> taking into account _id field only",
                     pszId,
                     poFeature->GetFID());
        }
    }
    else if ( poFeature->GetFID() != OGRNullFID )
    {
        json_object_object_add( poObj, "_id",
                                json_object_new_string(CPLSPrintf("%09ld", poFeature->GetFID())) );
    }

    if (poFeature->IsFieldSet(_REV_FIELD))
    {
        const char* pszRev = poFeature->GetFieldAsString(_REV_FIELD);
        json_object_object_add( poObj, "_rev",
                                json_object_new_string(pszRev) );
    }

    if (bGeoJSONDocument)
    {
        json_object_object_add( poObj, "type",
                                json_object_new_string("Feature") );
    }

/* -------------------------------------------------------------------- */
/*      Write feature attributes to GeoJSON "properties" object.        */
/* -------------------------------------------------------------------- */
    json_object* poObjProps = NULL;

    poObjProps = OGRGeoJSONWriteAttributes( poFeature );
    if (poObjProps)
    {
        json_object_object_del(poObjProps, "_id");
        json_object_object_del(poObjProps, "_rev");
    }

    if (bGeoJSONDocument)
    {
        json_object_object_add( poObj, "properties", poObjProps );
    }
    else
    {
        json_object_iter it;
        it.key = NULL;
        it.val = NULL;
        it.entry = NULL;
        json_object_object_foreachC( poObjProps, it )
        {
            json_object_object_add( poObj, it.key, json_object_get(it.val) );
        }
        json_object_put(poObjProps);
    }

/* -------------------------------------------------------------------- */
/*      Write feature geometry to GeoJSON "geometry" object.            */
/*      Null geometries are allowed, according to the GeoJSON Spec.     */
/* -------------------------------------------------------------------- */
    if (eGeomType != wkbNone)
    {
        json_object* poObjGeom = NULL;

        OGRGeometry* poGeometry = poFeature->GetGeometryRef();
        if ( NULL != poGeometry )
        {
            poObjGeom = OGRGeoJSONWriteGeometry( poGeometry, nCoordPrecision );
            if ( poObjGeom != NULL &&
                 wkbFlatten(poGeometry->getGeometryType()) != wkbPoint &&
                 !poGeometry->IsEmpty() )
            {
                OGREnvelope sEnvelope;
                poGeometry->getEnvelope(&sEnvelope);

                json_object* bbox = json_object_new_array();
                json_object_array_add(bbox, json_object_new_double_with_precision(sEnvelope.MinX, nCoordPrecision));
                json_object_array_add(bbox, json_object_new_double_with_precision(sEnvelope.MinY, nCoordPrecision));
                json_object_array_add(bbox, json_object_new_double_with_precision(sEnvelope.MaxX, nCoordPrecision));
                json_object_array_add(bbox, json_object_new_double_with_precision(sEnvelope.MaxY, nCoordPrecision));
                json_object_object_add( poObjGeom, "bbox", bbox );
            }
        }

        json_object_object_add( poObj, "geometry", poObjGeom );
    }

    return poObj;
}

/************************************************************************/
/*                           GetMaximumId()                             */
/************************************************************************/

int OGRCouchDBTableLayer::GetMaximumId()
{
    CPLString osURI("/");
    osURI += osEscapedName;
    osURI += "/_all_docs?startkey_docid=999999999&endkey_docid=000000000&descending=true&limit=1";
    json_object* poAnswerObj = poDS->GET(osURI);
    if (poAnswerObj == NULL)
        return -1;

    if ( !json_object_is_type(poAnswerObj, json_type_object) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "GetMaximumId() failed");
        json_object_put(poAnswerObj);
        return -1;
    }

    if (poDS->IsError(poAnswerObj, "GetMaximumId() failed"))
    {
        json_object_put(poAnswerObj);
        return -1;
    }

    json_object* poRows = json_object_object_get(poAnswerObj, "rows");
    if (poRows == NULL ||
        !json_object_is_type(poRows, json_type_array))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "GetMaximumId() failed");
        json_object_put(poAnswerObj);
        return -1;
    }

    int nRows = json_object_array_length(poRows);
    if (nRows != 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "GetMaximumId() failed");
        json_object_put(poAnswerObj);
        return -1;
    }
    
    json_object* poRow = json_object_array_get_idx(poRows, 0);
    if ( poRow == NULL ||
            !json_object_is_type(poRow, json_type_object) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "GetMaximumId() failed");
        json_object_put(poAnswerObj);
        return -1;
    }

    json_object* poId = json_object_object_get(poRow, "id");
    const char* pszId = json_object_get_string(poId);
    if (pszId != NULL)
    {
        int nId = atoi(pszId);
        json_object_put(poAnswerObj);
        return nId;
    }

    json_object_put(poAnswerObj);
    return -1;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRCouchDBTableLayer::CreateFeature( OGRFeature *poFeature )

{
    GetLayerDefn();

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    if (poFeature->IsFieldSet(_REV_FIELD))
    {
        static int bOnce = FALSE;
        if (!bOnce)
        {
            bOnce = TRUE;
            CPLDebug("CouchDB", "CreateFeature() should be called with an unset _rev field. Ignoring it");
        }
        poFeature->UnsetField(_REV_FIELD);
    }

    if (nNextFIDForCreate < 0)
    {
        nNextFIDForCreate = GetMaximumId();
        if (nNextFIDForCreate >= 0)
            nNextFIDForCreate ++;
        else
            nNextFIDForCreate = GetTotalFeatureCount();
    }

    OGRGeometry* poGeom = poFeature->GetGeometryRef();
    if (bExtentValid && poGeom != NULL && !poGeom->IsEmpty())
    {
        OGREnvelope sEnvelope;
        poGeom->getEnvelope(&sEnvelope);
        if (!bExtentSet)
        {
            dfMinX = sEnvelope.MinX;
            dfMinY = sEnvelope.MinY;
            dfMaxX = sEnvelope.MaxX;
            dfMaxY = sEnvelope.MaxY;
            bExtentSet = TRUE;
        }
        if (sEnvelope.MinX < dfMinX)
            dfMinX = sEnvelope.MinX;
        if (sEnvelope.MinY < dfMinY)
            dfMinY = sEnvelope.MinY;
        if (sEnvelope.MaxX > dfMaxX)
            dfMaxX = sEnvelope.MaxX;
        if (sEnvelope.MaxY > dfMaxY)
            dfMaxY = sEnvelope.MaxY;
    }

    if (bExtentValid && eGeomType != wkbNone)
        bMustWriteMetadata = TRUE;

    int nFID = nNextFIDForCreate ++;
    CPLString osFID;
    if (!poFeature->IsFieldSet(_ID_FIELD) ||
        !CSLTestBoolean(CPLGetConfigOption("COUCHDB_PRESERVE_ID_ON_INSERT", "FALSE")))
    {
        if (poFeature->GetFID() != OGRNullFID)
        {
            nFID = (int)poFeature->GetFID();
        }
        osFID = CPLSPrintf("%09d", nFID);

        poFeature->SetField(_ID_FIELD, osFID);
        poFeature->SetFID(nFID);
    }
    else
    {
        const char* pszId = poFeature->GetFieldAsString(_ID_FIELD);
        osFID = pszId;
    }

    json_object* poObj = OGRCouchDBWriteFeature(poFeature, eGeomType,
                                                bGeoJSONDocument,
                                                nCoordPrecision);

    if (bInTransaction)
    {
        aoTransactionFeatures.push_back(poObj);

        return OGRERR_NONE;
    }

    const char* pszJson = json_object_to_json_string( poObj );
    CPLString osURI("/");
    osURI += osEscapedName;
    osURI += "/";
    osURI += osFID;
    json_object* poAnswerObj = poDS->PUT(osURI, pszJson);
    json_object_put( poObj );

    if (poAnswerObj == NULL)
        return OGRERR_FAILURE;

    if (!poDS->IsOK(poAnswerObj, "Feature creation failed"))
    {
        json_object_put(poAnswerObj);
        return OGRERR_FAILURE;
    }

    json_object* poId = json_object_object_get(poAnswerObj, "id");
    json_object* poRev = json_object_object_get(poAnswerObj, "rev");

    const char* pszId = json_object_get_string(poId);
    const char* pszRev = json_object_get_string(poRev);

    if (pszId)
    {
        poFeature->SetField(_ID_FIELD, pszId);

        int nFID = atoi(pszId);
        const char* pszFID = CPLSPrintf("%09d", nFID);
        if (strcmp(pszId, pszFID) == 0)
            poFeature->SetFID(nFID);
        else
            poFeature->SetFID(-1);
    }
    if (pszRev)
    {
        poFeature->SetField(_REV_FIELD, pszRev);
    }

    json_object_put(poAnswerObj);

    nUpdateSeq ++;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           SetFeature()                               */
/************************************************************************/

OGRErr      OGRCouchDBTableLayer::SetFeature( OGRFeature *poFeature )
{
    GetLayerDefn();

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    if (!poFeature->IsFieldSet(_ID_FIELD))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "SetFeature() requires non null _id field");
        return OGRERR_FAILURE;
    }

    json_object* poObj = OGRCouchDBWriteFeature(poFeature, eGeomType,
                                                bGeoJSONDocument,
                                                nCoordPrecision);

    const char* pszJson = json_object_to_json_string( poObj );
    CPLString osURI("/");
    osURI += osEscapedName;
    osURI += "/";
    osURI += poFeature->GetFieldAsString(_ID_FIELD);
    json_object* poAnswerObj = poDS->PUT(osURI, pszJson);
    json_object_put( poObj );

    if (poAnswerObj == NULL)
        return OGRERR_FAILURE;

    if (!poDS->IsOK(poAnswerObj, "Feature update failed"))
    {
        json_object_put(poAnswerObj);
        return OGRERR_FAILURE;
    }

    json_object* poRev = json_object_object_get(poAnswerObj, "rev");
    const char* pszRev = json_object_get_string(poRev);
    poFeature->SetField(_REV_FIELD, pszRev);

    json_object_put(poAnswerObj);

    if (bExtentValid && eGeomType != wkbNone)
    {
        bExtentValid = FALSE;
        bMustWriteMetadata = TRUE;
    }
    nUpdateSeq ++;

    return OGRERR_NONE;
}

/************************************************************************/
/*                          DeleteFeature()                             */
/************************************************************************/

OGRErr OGRCouchDBTableLayer::DeleteFeature( long nFID )
{
    GetLayerDefn();

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    OGRFeature* poFeature = GetFeature(nFID);
    if (poFeature == NULL)
        return OGRERR_FAILURE;

    return DeleteFeature(poFeature);
}

/************************************************************************/
/*                          DeleteFeature()                             */
/************************************************************************/

OGRErr OGRCouchDBTableLayer::DeleteFeature( const char* pszId )
{
    GetLayerDefn();

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    OGRFeature* poFeature = GetFeature(pszId);
    if (poFeature == NULL)
        return OGRERR_FAILURE;

    return DeleteFeature(poFeature);
}

/************************************************************************/
/*                          DeleteFeature()                             */
/************************************************************************/

OGRErr OGRCouchDBTableLayer::DeleteFeature( OGRFeature* poFeature )
{
    if (!poFeature->IsFieldSet(_ID_FIELD) ||
        !poFeature->IsFieldSet(_REV_FIELD))
    {
        delete poFeature;
        return OGRERR_FAILURE;
    }

    const char* pszId = poFeature->GetFieldAsString(_ID_FIELD);
    const char* pszRev = poFeature->GetFieldAsString(_REV_FIELD);

    CPLString osURI("/");
    osURI += osEscapedName;
    osURI += "/";
    osURI += CPLSPrintf("%s?rev=%s", pszId, pszRev);

    if (bExtentValid && eGeomType != wkbNone)
        bMustWriteMetadata = TRUE;

    OGRGeometry* poGeom = poFeature->GetGeometryRef();
    if (bExtentValid && bExtentSet && poGeom != NULL && !poGeom->IsEmpty())
    {
        OGREnvelope sEnvelope;
        poGeom->getEnvelope(&sEnvelope);
        if (dfMinX == sEnvelope.MinX ||
            dfMinY == sEnvelope.MinY ||
            dfMaxX == sEnvelope.MaxX ||
            dfMaxY == sEnvelope.MaxY)
        {
            bExtentValid = FALSE;
        }
    }

    delete poFeature;

    json_object* poAnswerObj = poDS->DELETE(osURI);

    if (poAnswerObj == NULL)
        return OGRERR_FAILURE;

    if (!poDS->IsOK(poAnswerObj, "Feature deletion failed"))
    {
        json_object_put(poAnswerObj);
        return OGRERR_FAILURE;
    }

    nUpdateSeq ++;

    json_object_put(poAnswerObj);

    return OGRERR_NONE;
}

/************************************************************************/
/*                         StartTransaction()                           */
/************************************************************************/

OGRErr OGRCouchDBTableLayer::StartTransaction()
{
    GetLayerDefn();

    if (bInTransaction)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Already in transaction");
        return OGRERR_FAILURE;
    }

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    bInTransaction = TRUE;

    return OGRERR_NONE;
}

/************************************************************************/
/*                         CommitTransaction()                          */
/************************************************************************/

OGRErr OGRCouchDBTableLayer::CommitTransaction()
{
    GetLayerDefn();

    if (!bInTransaction)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Should be in transaction");
        return OGRERR_FAILURE;
    }

    bInTransaction = FALSE;

    if (aoTransactionFeatures.size() == 0)
        return OGRERR_NONE;

    CPLString osPost("{ \"docs\": [");
    for(int i=0;i<(int)aoTransactionFeatures.size();i++)
    {
        if (i>0) osPost += ",";
        const char* pszJson = json_object_to_json_string( aoTransactionFeatures[i] );
        osPost += pszJson;
        json_object_put(aoTransactionFeatures[i]);
    }
    osPost += "] }";
    aoTransactionFeatures.resize(0);

    CPLString osURI("/");
    osURI += osEscapedName;
    osURI += "/_bulk_docs";
    json_object* poAnswerObj = poDS->POST(osURI, osPost);

    if (poAnswerObj == NULL)
        return OGRERR_FAILURE;

    if ( json_object_is_type(poAnswerObj, json_type_object) )
    {
        poDS->IsError(poAnswerObj, "Bulk feature creation failed");

        json_object_put(poAnswerObj);
        return OGRERR_FAILURE;
    }

    if ( !json_object_is_type(poAnswerObj, json_type_array) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Bulk feature creation failed");
        json_object_put(poAnswerObj);

        return OGRERR_FAILURE;
    }

    int nRows = json_object_array_length(poAnswerObj);
    for(int i=0;i<nRows;i++)
    {
        json_object* poRow = json_object_array_get_idx(poAnswerObj, i);
        if ( poRow != NULL &&
             json_object_is_type(poRow, json_type_object) )
        {
            json_object* poId = json_object_object_get(poRow, "id");
            json_object* poRev = json_object_object_get(poRow, "rev");
            json_object* poError = json_object_object_get(poRow, "error");
            json_object* poReason = json_object_object_get(poRow, "reason");

            const char* pszId = json_object_get_string(poId);

            if (poError != NULL)
            {
                const char* pszError = json_object_get_string(poError);
                const char* pszReason = json_object_get_string(poReason);

                CPLError(CE_Failure, CPLE_AppDefined,
                         "Bulk feature creation failed : for %s: %s, %s",
                         pszId ? pszId : "",
                         pszError ? pszError : "",
                         pszReason ? pszReason : "");
            }
            else if (poRev != NULL)
            {
                /*const char* pszRev = json_object_get_string(poRev);
                CPLDebug("CouchDB", "id = %s, rev = %s",
                         pszId ? pszId : "", pszRev ? pszRev : "");*/

                nUpdateSeq ++;
            }
        }
    }

    json_object_put(poAnswerObj);

    return OGRERR_NONE;
}

/************************************************************************/
/*                        RollbackTransaction()                         */
/************************************************************************/

OGRErr OGRCouchDBTableLayer::RollbackTransaction()
{
    GetLayerDefn();

    if (!bInTransaction)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Should be in transaction");
        return OGRERR_FAILURE;
    }
    bInTransaction = FALSE;
    for(int i=0;i<(int)aoTransactionFeatures.size();i++)
    {
        json_object_put(aoTransactionFeatures[i]);
    }
    aoTransactionFeatures.resize(0);
    return OGRERR_NONE;
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRCouchDBTableLayer::SetAttributeFilter( const char *pszQuery )

{
    GetLayerDefn();

    bServerSideAttributeFilteringWorks = TRUE;

    OGRErr eErr = OGRCouchDBLayer::SetAttributeFilter(pszQuery);

    if (eErr == OGRERR_NONE)
    {
        bHasInstalledAttributeFilter = TRUE;
    }

    return eErr;
}


/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRCouchDBTableLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    GetLayerDefn();

    if( InstallFilter( poGeomIn ) )
    {
        bMustRunSpatialFilter = TRUE;

        ResetReading();
    }
}

/************************************************************************/
/*                          SetInfoAfterCreation()                      */
/************************************************************************/

void OGRCouchDBTableLayer::SetInfoAfterCreation(OGRwkbGeometryType eGType,
                                                OGRSpatialReference* poSRSIn,
                                                int nUpdateSeqIn,
                                                int bGeoJSONDocumentIn)
{
    eGeomType = eGType;
    nNextFIDForCreate = 0;
    bMustWriteMetadata = TRUE;
    bExtentValid = TRUE;
    bHasLoadedMetadata = TRUE;
    nUpdateSeq = nUpdateSeqIn;
    bGeoJSONDocument = bGeoJSONDocumentIn;

    CPLAssert(poSRS == NULL);
    poSRS = poSRSIn;
    if (poSRS)
        poSRS->Reference();
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

OGRSpatialReference* OGRCouchDBTableLayer::GetSpatialRef()
{
    LoadMetadata();

    return poSRS;
}

/************************************************************************/
/*                          LoadMetadata()                              */
/************************************************************************/

void OGRCouchDBTableLayer::LoadMetadata()
{
    if (bHasLoadedMetadata)
        return;

    bHasLoadedMetadata = TRUE;

    CPLString osURI("/");
    osURI += osEscapedName;
    osURI += "/_design/ogr_metadata";
    json_object* poAnswerObj = poDS->GET(osURI);
    if (poAnswerObj == NULL)
        return;

    if ( !json_object_is_type(poAnswerObj, json_type_object) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "LoadMetadata() failed");
        json_object_put(poAnswerObj);
        return;
    }

    json_object* poRev = json_object_object_get(poAnswerObj, "_rev");
    const char* pszRev = json_object_get_string(poRev);
    if (pszRev)
        osMetadataRev = pszRev;

    json_object* poError = json_object_object_get(poAnswerObj, "error");
    const char* pszError = json_object_get_string(poError);
    if (pszError && strcmp(pszError, "not_found") == 0)
    {
        json_object_put(poAnswerObj);
        return;
    }

    if (poDS->IsError(poAnswerObj, "LoadMetadata() failed"))
    {
        json_object_put(poAnswerObj);
        return;
    }

    json_object* poJsonSRS = json_object_object_get(poAnswerObj, "srs");
    const char* pszSRS = json_object_get_string(poJsonSRS);
    if (pszSRS != NULL)
    {
        poSRS = new OGRSpatialReference();
        if (poSRS->importFromWkt((char**)&pszSRS) != OGRERR_NONE)
        {
            delete poSRS;
            poSRS = NULL;
        }
    }

    json_object* poGeomType = json_object_object_get(poAnswerObj, "geomtype");
    const char* pszGeomType = json_object_get_string(poGeomType);

    if (pszGeomType)
    {
        if (EQUAL(pszGeomType, "NONE"))
        {
            eGeomType = wkbNone;
            bExtentValid = TRUE;
        }
        else
        {
            eGeomType = OGRFromOGCGeomType(pszGeomType);

            json_object* poIs25D = json_object_object_get(poAnswerObj, "is_25D");
            if (poIs25D && json_object_get_boolean(poIs25D))
                eGeomType = (OGRwkbGeometryType) (eGeomType | wkb25DBit);

            json_object* poExtent = json_object_object_get(poAnswerObj, "extent");
            if (poExtent && json_object_get_type(poExtent) == json_type_object)
            {
                json_object* poUpdateSeq =
                    json_object_object_get(poExtent, "validity_update_seq");
                if (poUpdateSeq && json_object_get_type(poUpdateSeq) == json_type_int)
                {
                    int nValidityUpdateSeq = json_object_get_int(poUpdateSeq);
                    if (nValidityUpdateSeq <= 0)
                    {
                        bAlwaysValid = TRUE;
                    }
                    else
                    {
                        if (nUpdateSeq < 0)
                            nUpdateSeq = FetchUpdateSeq();
                        if (nUpdateSeq != nValidityUpdateSeq)
                        {
                            CPLDebug("CouchDB",
                                    "_design/ogr_metadata.extent.validity_update_seq "
                                    "doesn't match database update_seq --> ignoring stored extent");
                            poUpdateSeq = NULL;
                        }
                    }
                }
                else
                    poUpdateSeq = NULL;

                json_object* poBbox = json_object_object_get(poExtent, "bbox");
                if (poUpdateSeq && poBbox &&
                    json_object_get_type(poBbox) == json_type_array &&
                    json_object_array_length(poBbox) == 4 &&
                    json_object_get_type(json_object_array_get_idx(poBbox, 0)) == json_type_double &&
                    json_object_get_type(json_object_array_get_idx(poBbox, 1)) == json_type_double &&
                    json_object_get_type(json_object_array_get_idx(poBbox, 2)) == json_type_double &&
                    json_object_get_type(json_object_array_get_idx(poBbox, 3)) == json_type_double)
                {
                    dfMinX = json_object_get_double(json_object_array_get_idx(poBbox, 0));
                    dfMinY = json_object_get_double(json_object_array_get_idx(poBbox, 1));
                    dfMaxX = json_object_get_double(json_object_array_get_idx(poBbox, 2));
                    dfMaxY = json_object_get_double(json_object_array_get_idx(poBbox, 3));
                    bExtentValid = bExtentSet = TRUE;
                }
            }
        }
    }

    json_object* poGeoJSON = json_object_object_get(poAnswerObj, "geojson_documents");
    if (poGeoJSON && json_object_is_type(poGeoJSON, json_type_boolean))
        bGeoJSONDocument = json_object_get_boolean(poGeoJSON);

    json_object* poFields = json_object_object_get(poAnswerObj, "fields");
    if (poFields && json_object_is_type(poFields, json_type_array))
    {
        poFeatureDefn = new OGRFeatureDefn( osName );
        poFeatureDefn->Reference();

        poFeatureDefn->SetGeomType(eGeomType);

        OGRFieldDefn oFieldId("_id", OFTString);
        poFeatureDefn->AddFieldDefn(&oFieldId);

        OGRFieldDefn oFieldRev("_rev", OFTString);
        poFeatureDefn->AddFieldDefn(&oFieldRev);

        int nFields = json_object_array_length(poFields);
        for(int i=0;i<nFields;i++)
        {
            json_object* poField = json_object_array_get_idx(poFields, i);
            if (poField && json_object_is_type(poField, json_type_object))
            {
                json_object* poName = json_object_object_get(poField, "name");
                const char* pszName = json_object_get_string(poName);
                if (pszName)
                {
                    json_object* poType = json_object_object_get(poField, "type");
                    const char* pszType = json_object_get_string(poType);
                    OGRFieldType eType = OFTString;
                    if (pszType)
                    {
                        if (strcmp(pszType, "integer") == 0)
                            eType = OFTInteger;
                        else if (strcmp(pszType, "integerlist") == 0)
                            eType = OFTIntegerList;
                        else if (strcmp(pszType, "real") == 0)
                            eType = OFTReal;
                        else if (strcmp(pszType, "reallist") == 0)
                            eType = OFTRealList;
                        else if (strcmp(pszType, "string") == 0)
                            eType = OFTString;
                        else if (strcmp(pszType, "stringlist") == 0)
                            eType = OFTStringList;
                    }

                    OGRFieldDefn oField(pszName, eType);
                    poFeatureDefn->AddFieldDefn(&oField);
                }
            }
        }
    }

    json_object_put(poAnswerObj);

    return;
}

/************************************************************************/
/*                          WriteMetadata()                             */
/************************************************************************/

void OGRCouchDBTableLayer::WriteMetadata()
{
    GetLayerDefn();

    CPLString osURI;
    osURI = "/";
    osURI += osEscapedName;
    osURI += "/_design/ogr_metadata";

    json_object* poDoc = json_object_new_object();

    if (osMetadataRev.size() > 0)
    {
        json_object_object_add(poDoc, "_rev",
                               json_object_new_string(osMetadataRev));
    }

    if (poSRS)
    {
        char* pszWKT = NULL;
        poSRS->exportToWkt(&pszWKT);
        if (pszWKT)
        {
            json_object_object_add(poDoc, "srs",
                                   json_object_new_string(pszWKT));
            CPLFree(pszWKT);
        }
    }

    if (eGeomType != wkbNone)
    {
        json_object_object_add(poDoc, "geomtype",
                    json_object_new_string(OGRToOGCGeomType(eGeomType)));
        if (poFeatureDefn->GetGeomType() & wkb25DBit)
        {
            json_object_object_add(poDoc, "is_25D",
                               json_object_new_boolean(TRUE));
        }

        if (bExtentValid && bExtentSet && nUpdateSeq >= 0)
        {
            json_object* poExtent = json_object_new_object();
            json_object_object_add(poDoc, "extent", poExtent);

            json_object_object_add(poExtent, "validity_update_seq",
                                   json_object_new_int((bAlwaysValid) ? -1 : nUpdateSeq + 1));

            json_object* poBbox = json_object_new_array();
            json_object_object_add(poExtent, "bbox", poBbox);
            json_object_array_add(poBbox, json_object_new_double_with_precision(dfMinX, nCoordPrecision));
            json_object_array_add(poBbox, json_object_new_double_with_precision(dfMinY, nCoordPrecision));
            json_object_array_add(poBbox, json_object_new_double_with_precision(dfMaxX, nCoordPrecision));
            json_object_array_add(poBbox, json_object_new_double_with_precision(dfMaxY, nCoordPrecision));
        }
    }
    else
    {
        json_object_object_add(poDoc, "geomtype",
                               json_object_new_string("NONE"));
    }

    json_object_object_add(poDoc, "geojson_documents",
                           json_object_new_boolean(bGeoJSONDocument));

    json_object* poFields = json_object_new_array();
    json_object_object_add(poDoc, "fields", poFields);


    for(int i=FIRST_FIELD;i<poFeatureDefn->GetFieldCount();i++)
    {
        json_object* poField = json_object_new_object();
        json_object_array_add(poFields, poField);

        json_object_object_add(poField, "name",
            json_object_new_string(poFeatureDefn->GetFieldDefn(i)->GetNameRef()));

        const char* pszType = NULL;
        switch (poFeatureDefn->GetFieldDefn(i)->GetType())
        {
            case OFTInteger: pszType = "integer"; break;
            case OFTReal: pszType = "real"; break;
            case OFTString: pszType = "string"; break;
            case OFTIntegerList: pszType = "integerlist"; break;
            case OFTRealList: pszType = "reallist"; break;
            case OFTStringList: pszType = "stringlist"; break;
            default: pszType = "string"; break;
        }

        json_object_object_add(poField, "type",
                               json_object_new_string(pszType));
    }

    json_object* poAnswerObj = poDS->PUT(osURI,
                                         json_object_to_json_string(poDoc));

    json_object_put(poDoc);

    if (poDS->IsOK(poAnswerObj, "Metadata creation failed"))
    {
        nUpdateSeq++;

        json_object* poRev = json_object_object_get(poAnswerObj, "_rev");
        const char* pszRev = json_object_get_string(poRev);
        if (pszRev)
            osMetadataRev = pszRev;
    }

    json_object_put(poAnswerObj);
}

/************************************************************************/
/*                            GetExtent()                               */
/************************************************************************/

OGRErr OGRCouchDBTableLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    LoadMetadata();

    if (!bExtentValid)
        return OGRCouchDBLayer::GetExtent(psExtent, bForce);

    psExtent->MinX = 0.0;
    psExtent->MaxX = 0.0;
    psExtent->MinY = 0.0;
    psExtent->MaxY = 0.0;

    if (!bExtentSet)
        return OGRERR_FAILURE;

    psExtent->MinX = dfMinX;
    psExtent->MaxX = dfMaxX;
    psExtent->MinY = dfMinY;
    psExtent->MaxY = dfMaxY;

    return OGRERR_NONE;
}

/************************************************************************/
/*                          FetchUpdateSeq()                            */
/************************************************************************/

int OGRCouchDBTableLayer::FetchUpdateSeq()
{
    if (nUpdateSeq >= 0)
        return nUpdateSeq;

    CPLString osURI("/");
    osURI += osEscapedName;
    osURI += "/";

    json_object* poAnswerObj = poDS->GET(osURI);
    if (poAnswerObj != NULL &&
        json_object_is_type(poAnswerObj, json_type_object) &&
        json_object_object_get(poAnswerObj, "update_seq") != NULL)
    {
        nUpdateSeq = json_object_get_int(json_object_object_get(poAnswerObj,
                                                                "update_seq"));
    }
    else
    {
        poDS->IsError(poAnswerObj, "FetchUpdateSeq() failed");
    }

    json_object_put(poAnswerObj);

    return nUpdateSeq;
}
