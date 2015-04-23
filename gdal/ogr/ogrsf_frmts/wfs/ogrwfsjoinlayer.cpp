/******************************************************************************
 * $Id$
 *
 * Project:  WFS Translator
 * Purpose:  Implements OGRWFSJoinLayer class.
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
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

#include "ogr_wfs.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRWFSJoinLayer()                           */
/************************************************************************/

OGRWFSJoinLayer::OGRWFSJoinLayer(OGRWFSDataSource* poDS,
                                 const swq_select* psSelectInfo,
                                 const CPLString& osGlobalFilter)
{
    this->poDS = poDS;
    this->osGlobalFilter = osGlobalFilter;
    poFeatureDefn = NULL;
    bPagingActive = FALSE;
    nPagingStartIndex = 0;
    nFeatureRead = 0;
    nFeatureCountRequested = 0;
    poBaseDS = NULL;
    poBaseLayer = NULL;
    bReloadNeeded = FALSE;
    bHasFetched = FALSE;

    CPLString osName("join_");
    CPLString osLayerName;
    osLayerName = psSelectInfo->table_defs[0].table_name;
    apoLayers.push_back((OGRWFSLayer*)poDS->GetLayerByName(osLayerName));
    osName += osLayerName;
    for(int i=0;i<psSelectInfo->join_count;i++)
    {
        osName += "_";
        osLayerName = psSelectInfo->table_defs[
                        psSelectInfo->join_defs[i].secondary_table].table_name;
        apoLayers.push_back((OGRWFSLayer*)poDS->GetLayerByName(osLayerName));
        osName += osLayerName;
    }
    
    osFeatureTypes = "(";
    for(int i=0;i<(int)apoLayers.size();i++)
    {
        if( i > 0 )
            osFeatureTypes += ",";
        osFeatureTypes += apoLayers[i]->GetName();
    }
    osFeatureTypes += ")";

    SetDescription(osName);

    poFeatureDefn = new OGRFeatureDefn(GetDescription());
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);

    for( int i = 0; i < psSelectInfo->result_columns; i++ )
    {
        swq_col_def *def = psSelectInfo->column_defs + i;
        int table_index;
        if( def->table_index >= 0 )
            table_index = def->table_index;
        else
        {
            CPLAssert(def->expr->eNodeType == SNT_OPERATION && def->expr->nOperation == SWQ_CAST);
            table_index = def->expr->papoSubExpr[0]->table_index;
        }
        OGRWFSLayer* poLayer = apoLayers[table_index];
        const char* pszTableAlias = psSelectInfo->table_defs[table_index].table_alias;
        const char* pszTablePrefix = pszTableAlias ? pszTableAlias : poLayer->GetShortName();
        int idx = poLayer->GetLayerDefn()->GetFieldIndex(def->field_name);
        if( idx >= 0 )
        {
            OGRFieldDefn oFieldDefn(poLayer->GetLayerDefn()->GetFieldDefn(idx));
            const char* pszSrcFieldname = CPLSPrintf("%s.%s",
                                                     poLayer->GetShortName(),
                                                     oFieldDefn.GetNameRef());
            const char* pszFieldname = CPLSPrintf("%s.%s",
                                                     pszTablePrefix,
                                                     oFieldDefn.GetNameRef());
            aoSrcFieldNames.push_back(pszSrcFieldname);
            oFieldDefn.SetName(def->field_alias ? def->field_alias : pszFieldname);
            if( def->expr && def->expr->eNodeType == SNT_OPERATION && def->expr->nOperation == SWQ_CAST)
            {
                switch( def->field_type )
                {
                    case SWQ_INTEGER: oFieldDefn.SetType(OFTInteger); break;
                    case SWQ_INTEGER64: oFieldDefn.SetType(OFTInteger64); break;
                    case SWQ_FLOAT: oFieldDefn.SetType(OFTReal); break;
                    case SWQ_STRING: oFieldDefn.SetType(OFTString); break;
                    case SWQ_BOOLEAN: oFieldDefn.SetType(OFTInteger); oFieldDefn.SetSubType(OFSTBoolean); break;
                    case SWQ_DATE: oFieldDefn.SetType(OFTDate); break;
                    case SWQ_TIME: oFieldDefn.SetType(OFTTime); break;
                    case SWQ_TIMESTAMP: oFieldDefn.SetType(OFTDateTime); break;
                    default: break;
                }
            }
            poFeatureDefn->AddFieldDefn(&oFieldDefn);
        }
        else
        {
            idx = poLayer->GetLayerDefn()->GetGeomFieldIndex(def->field_name);
            if (idx >= 0 )
            {
                OGRGeomFieldDefn oFieldDefn(poLayer->GetLayerDefn()->GetGeomFieldDefn(idx));
                const char* pszSrcFieldname = CPLSPrintf("%s.%s",
                                                     poLayer->GetShortName(),
                                                     oFieldDefn.GetNameRef());
                const char* pszFieldname = CPLSPrintf("%s.%s",
                                                     pszTablePrefix,
                                                     oFieldDefn.GetNameRef());
                aoSrcGeomFieldNames.push_back(pszSrcFieldname);
                oFieldDefn.SetName(def->field_alias ? def->field_alias : pszFieldname);
                poFeatureDefn->AddGeomFieldDefn(&oFieldDefn);
            }
        }
    }

    CPLXMLNode* psGlobalSchema = CPLCreateXMLNode( NULL, CXT_Element, "Schema" );
    for(int i=0;i<(int)apoLayers.size();i++)
    {
        CPLString osTmpFileName = CPLSPrintf("/vsimem/tempwfs_%p/file.xsd", apoLayers[i]);
        CPLPushErrorHandler(CPLQuietErrorHandler);
        CPLXMLNode* psSchema = CPLParseXMLFile(osTmpFileName);
        CPLPopErrorHandler();
        if( psSchema == NULL )
        {
            CPLDestroyXMLNode(psGlobalSchema);
            psGlobalSchema = NULL;
            break;
        }
        CPLXMLNode* psIter;
        for(psIter = psSchema->psChild; psIter != NULL; psIter = psIter->psNext )
        {
            if( psIter->eType == CXT_Element )
                break;
        }
        CPLAddXMLChild(psGlobalSchema, CPLCloneXMLTree(psIter));
        CPLDestroyXMLNode(psSchema);
    }
    if( psGlobalSchema )
    {
        CPLString osTmpFileName = CPLSPrintf("/vsimem/tempwfs_%p/file.xsd", this);
        CPLSerializeXMLTreeToFile(psGlobalSchema, osTmpFileName);
        CPLDestroyXMLNode(psGlobalSchema);
    }
}

/************************************************************************/
/*                          ~OGRWFSJoinLayer()                          */
/************************************************************************/

OGRWFSJoinLayer::~OGRWFSJoinLayer()
{
    if( poFeatureDefn != NULL )
        poFeatureDefn->Release();
    if( poBaseDS != NULL )
        GDALClose(poBaseDS);

    CPLString osTmpFileName = CPLSPrintf("/vsimem/tempwfs_%p/file.xsd", this);
    VSIUnlink(osTmpFileName);
}

/************************************************************************/
/*                    OGRWFSRemoveReferenceToTableAlias()               */
/************************************************************************/

static void OGRWFSRemoveReferenceToTableAlias(swq_expr_node* poNode,
                                              const swq_select* psSelectInfo)
{
    if( poNode->eNodeType == SNT_COLUMN )
    {
        if( poNode->table_name != NULL )
        {
            for(int i=0;i < psSelectInfo->table_count;i++)
            {
                if( psSelectInfo->table_defs[i].table_alias != NULL &&
                    EQUAL(poNode->table_name, psSelectInfo->table_defs[i].table_alias) )
                {
                    CPLFree(poNode->table_name);
                    poNode->table_name = CPLStrdup(psSelectInfo->table_defs[i].table_name);
                    break;
                }
            }
        }
    }
    else if( poNode->eNodeType == SNT_OPERATION )
    {
        for(int i=0;i < poNode->nSubExprCount;i++)
            OGRWFSRemoveReferenceToTableAlias(poNode->papoSubExpr[i],
                                              psSelectInfo);
    }
}

/************************************************************************/
/*                             Build()                                  */
/************************************************************************/

OGRWFSJoinLayer* OGRWFSJoinLayer::Build(OGRWFSDataSource* poDS,
                                        const swq_select* psSelectInfo)
{
    CPLString osGlobalFilter;
    
    for( int i = 0; i < psSelectInfo->result_columns; i++ )
    {
        swq_col_def *def = psSelectInfo->column_defs + i;
        if( !(def->col_func == SWQCF_NONE &&
              (def->expr == NULL ||
               def->expr->eNodeType == SNT_COLUMN ||
               (def->expr->eNodeType == SNT_OPERATION && def->expr->nOperation == SWQ_CAST))) )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Only column names supported in column selection");
            return NULL;
        }
    }

    if( psSelectInfo->join_count > 1 || psSelectInfo->where_expr != NULL )
        osGlobalFilter += "<And>";
    for(int i=0;i<psSelectInfo->join_count;i++)
    {
        OGRWFSRemoveReferenceToTableAlias(psSelectInfo->join_defs[i].poExpr,
                                          psSelectInfo);
        int bOutNeedsNullCheck;
        CPLString osFilter = WFS_TurnSQLFilterToOGCFilter(
                                      psSelectInfo->join_defs[i].poExpr,
                                      poDS,
                                      NULL,
                                      200,
                                      TRUE, /* bPropertyIsNotEqualToSupported */
                                      FALSE, /* bUseFeatureId */
                                      FALSE, /* bGmlObjectIdNeedsGMLPrefix */
                                      &bOutNeedsNullCheck);
        if( osFilter.size() == 0 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported JOIN clause");
            return NULL;
        }
        osGlobalFilter += osFilter;
    }
    if( psSelectInfo->where_expr != NULL )
    {
        OGRWFSRemoveReferenceToTableAlias(psSelectInfo->where_expr,
                                          psSelectInfo);
        int bOutNeedsNullCheck;
        CPLString osFilter = WFS_TurnSQLFilterToOGCFilter(
                                      psSelectInfo->where_expr,
                                      poDS,
                                      NULL,
                                      200,
                                      TRUE, /* bPropertyIsNotEqualToSupported */
                                      FALSE, /* bUseFeatureId */
                                      FALSE, /* bGmlObjectIdNeedsGMLPrefix */
                                      &bOutNeedsNullCheck);
        if( osFilter.size() == 0 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported WHERE clause");
            return NULL;
        }
        osGlobalFilter += osFilter;
    }
    if( psSelectInfo->join_count > 1 || psSelectInfo->where_expr != NULL )
        osGlobalFilter += "</And>";
    CPLDebug("WFS", "osGlobalFilter = %s", osGlobalFilter.c_str());

    OGRWFSJoinLayer* poLayer = new OGRWFSJoinLayer(poDS, psSelectInfo,
                                                   osGlobalFilter);
    return poLayer;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRWFSJoinLayer::ResetReading()
{
    if (bPagingActive)
        bReloadNeeded = TRUE;
    nPagingStartIndex = 0;
    nFeatureRead = 0;
    nFeatureCountRequested = 0;
    if (bReloadNeeded)
    {
        GDALClose(poBaseDS);
        poBaseDS = NULL;
        poBaseLayer = NULL;
        bHasFetched = FALSE;
        bReloadNeeded = FALSE;
    }
    if (poBaseLayer)
        poBaseLayer->ResetReading();
}

/************************************************************************/
/*                       MakeGetFeatureURL()                            */
/************************************************************************/

CPLString OGRWFSJoinLayer::MakeGetFeatureURL(int bRequestHits)
{
    CPLString osURL(poDS->GetBaseURL());
    osURL = CPLURLAddKVP(osURL, "SERVICE", "WFS");
    osURL = CPLURLAddKVP(osURL, "VERSION", poDS->GetVersion());
    osURL = CPLURLAddKVP(osURL, "REQUEST", "GetFeature");
    osURL = CPLURLAddKVP(osURL, "TYPENAMES", WFS_EscapeURL(osFeatureTypes));

    int nRequestMaxFeatures = 0;
    if (poDS->IsPagingAllowed() && !bRequestHits &&
        CPLURLGetValue(osURL, "COUNT").size() == 0 )
    {
        osURL = CPLURLAddKVP(osURL, "STARTINDEX",
            CPLSPrintf("%d", nPagingStartIndex +
                                poDS->GetBaseStartIndex()));
        nRequestMaxFeatures = poDS->GetPageSize();
        nFeatureCountRequested = nRequestMaxFeatures;
        bPagingActive = TRUE;
    }

    if (nRequestMaxFeatures)
    {
        osURL = CPLURLAddKVP(osURL,
                             "COUNT",
                             CPLSPrintf("%d", nRequestMaxFeatures));
    }

    CPLString osFilter;
    osFilter = "<Filter xmlns=\"http://www.opengis.net/fes/2.0\"";
    
    std::map<CPLString, CPLString> oMapNS;
    for(int i=0;i<(int)apoLayers.size();i++)
    {
        const char* pszNS = apoLayers[i]->GetNamespacePrefix();
        const char* pszNSVal = apoLayers[i]->GetNamespaceName();
        if( pszNS && pszNSVal )
            oMapNS[pszNS] = pszNSVal;
    }
    std::map<CPLString, CPLString>::iterator oIter = oMapNS.begin();
    for(; oIter != oMapNS.end(); ++oIter )
    {
        osFilter += " xmlns:";
        osFilter += oIter->first;
        osFilter += "=\"";
        osFilter += oIter->second;
        osFilter += "\"";
    }
    osFilter += " xmlns:gml=\"http://www.opengis.net/gml/3.2\">";
    osFilter += osGlobalFilter;
    osFilter += "</Filter>";

    osURL = CPLURLAddKVP(osURL, "FILTER", WFS_EscapeURL(osFilter));

    if (bRequestHits)
    {
        osURL = CPLURLAddKVP(osURL, "RESULTTYPE", "hits");
    }

    return osURL;
}

/************************************************************************/
/*                         FetchGetFeature()                            */
/************************************************************************/

GDALDataset* OGRWFSJoinLayer::FetchGetFeature()
{

    CPLString osURL = MakeGetFeatureURL();
    CPLDebug("WFS", "%s", osURL.c_str());

    CPLHTTPResult* psResult = NULL;

    /* Try streaming when the output format is GML and that we have a .xsd */
    /* that we are able to understand */
    CPLString osXSDFileName = CPLSPrintf("/vsimem/tempwfs_%p/file.xsd", this);
    VSIStatBufL sBuf;
    if (CSLTestBoolean(CPLGetConfigOption("OGR_WFS_USE_STREAMING", "YES")) &&
        VSIStatL(osXSDFileName, &sBuf) == 0 && GDALGetDriverByName("GML") != NULL)
    {
        const char* pszStreamingName = CPLSPrintf("/vsicurl_streaming/%s",
                                                    osURL.c_str());
        if( strncmp(osURL, "/vsimem/", strlen("/vsimem/")) == 0 &&
            CSLTestBoolean(CPLGetConfigOption("CPL_CURL_ENABLE_VSIMEM", "FALSE")) )
        {
            pszStreamingName = osURL.c_str();
        }

        const char* const apszAllowedDrivers[] = { "GML", NULL };
        const char* apszOpenOptions[2] = { NULL, NULL };
        apszOpenOptions[0] = CPLSPrintf("XSD=%s", osXSDFileName.c_str());
        GDALDataset* poGML_DS = (GDALDataset*)
                GDALOpenEx(pszStreamingName, GDAL_OF_VECTOR, apszAllowedDrivers,
                           apszOpenOptions, NULL);
        if (poGML_DS)
        {
            //bStreamingDS = TRUE;
            return poGML_DS;
        }

        /* In case of failure, read directly the content to examine */
        /* it, if it is XML error content */
        char szBuffer[2048];
        int nRead = 0;
        VSILFILE* fp = VSIFOpenL(pszStreamingName, "rb");
        if (fp)
        {
            nRead = (int)VSIFReadL(szBuffer, 1, sizeof(szBuffer) - 1, fp);
            szBuffer[nRead] = '\0';
            VSIFCloseL(fp);
        }

        if (nRead != 0)
        {
            if (strstr(szBuffer, "<ServiceExceptionReport") != NULL ||
                strstr(szBuffer, "<ows:ExceptionReport") != NULL)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                            szBuffer);
                return NULL;
            }
        }
    }

    //bStreamingDS = FALSE;
    psResult = poDS->HTTPFetch( osURL, NULL);
    if (psResult == NULL)
    {
        return NULL;
    }

    CPLString osTmpDirName = CPLSPrintf("/vsimem/tempwfs_%p", this);
    VSIMkdir(osTmpDirName, 0);

    GByte *pabyData = psResult->pabyData;
    int    nDataLen = psResult->nDataLen;

    if (strstr((const char*)pabyData, "<ServiceExceptionReport") != NULL ||
        strstr((const char*)pabyData, "<ows:ExceptionReport") != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                 pabyData);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    CPLString osTmpFileName;

    osTmpFileName = osTmpDirName + "/file.gfs";
    VSIUnlink(osTmpFileName);

    osTmpFileName = osTmpDirName + "/file.gml";

    VSILFILE *fp = VSIFileFromMemBuffer( osTmpFileName, pabyData,
                                    nDataLen, TRUE);
    VSIFCloseL(fp);
    psResult->pabyData = NULL;

    CPLHTTPDestroyResult(psResult);

    OGRDataSource* poDS;

    poDS = (OGRDataSource*) OGROpen(osTmpFileName, FALSE, NULL);
    if (poDS == NULL)
    {
        if (pabyData != NULL &&
            strstr((const char*)pabyData, "<wfs:FeatureCollection") == NULL &&
            strstr((const char*)pabyData, "<gml:FeatureCollection") == NULL)
        {
            if (nDataLen > 1000)
                pabyData[1000] = 0;
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Error: cannot parse %s", pabyData);
        }
        return NULL;
    }

    OGRLayer* poLayer = poDS->GetLayer(0);
    if (poLayer == NULL)
    {
        OGRDataSource::DestroyDataSource(poDS);
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature* OGRWFSJoinLayer::GetNextFeature()
{
    if (bPagingActive && nFeatureRead == nPagingStartIndex + nFeatureCountRequested)
    {
        bReloadNeeded = TRUE;
        nPagingStartIndex = nFeatureRead;
    }
    if (bReloadNeeded)
    {
        GDALClose(poBaseDS);
        poBaseDS = NULL;
        poBaseLayer = NULL;
        bHasFetched = FALSE;
        bReloadNeeded = FALSE;
    }
    if (poBaseDS == NULL && !bHasFetched)
    {
        bHasFetched = TRUE;
        poBaseDS = FetchGetFeature();
        if (poBaseDS)
        {
            poBaseLayer = poBaseDS->GetLayer(0);
            poBaseLayer->ResetReading();
        }
    }
    if (!poBaseLayer)
        return NULL;

    while(TRUE)
    {
        OGRFeature* poSrcFeature = poBaseLayer->GetNextFeature();
        if (poSrcFeature == NULL)
            return NULL;
        nFeatureRead ++;

        OGRFeature* poNewFeature = new OGRFeature(poFeatureDefn);
        for(int i=0;i<(int)aoSrcFieldNames.size();i++)
        {
            int iSrcField = poSrcFeature->GetFieldIndex(aoSrcFieldNames[i]);
            if( iSrcField >= 0 && poSrcFeature->IsFieldSet(iSrcField) )
            {
                OGRFieldType eType = poFeatureDefn->GetFieldDefn(i)->GetType();
                if( eType == poSrcFeature->GetFieldDefnRef(iSrcField)->GetType() )
                {
                    poNewFeature->SetField(i, poSrcFeature->GetRawFieldRef(iSrcField));
                }
                else if( eType == OFTString )
                    poNewFeature->SetField(i, poSrcFeature->GetFieldAsString(iSrcField));
                else if( eType == OFTInteger )
                    poNewFeature->SetField(i, poSrcFeature->GetFieldAsInteger(iSrcField));
                else if( eType == OFTInteger64 )
                    poNewFeature->SetField(i, poSrcFeature->GetFieldAsInteger64(iSrcField));
                else if( eType == OFTReal )
                    poNewFeature->SetField(i, poSrcFeature->GetFieldAsDouble(iSrcField));
                else
                    poNewFeature->SetField(i, poSrcFeature->GetFieldAsString(iSrcField));
            }
        }
        for(int i=0;i<(int)aoSrcGeomFieldNames.size();i++)
        {
            int iSrcField = poSrcFeature->GetGeomFieldIndex(aoSrcGeomFieldNames[i]);
            if( iSrcField >= 0)
            {
                OGRGeometry* poGeom = poSrcFeature->StealGeometry(iSrcField);
                if( poGeom )
                {
                    poNewFeature->SetGeomFieldDirectly(i, poGeom);
                    poGeom->assignSpatialReference(poFeatureDefn->GetGeomFieldDefn(i)->GetSpatialRef());
                }
            }
        }

        poNewFeature->SetFID(nFeatureRead);
        delete poSrcFeature;
        return poNewFeature;
    }
}

/************************************************************************/
/*                  ExecuteGetFeatureResultTypeHits()                   */
/************************************************************************/

GIntBig OGRWFSJoinLayer::ExecuteGetFeatureResultTypeHits()
{
    char* pabyData = NULL;
    CPLString osURL = MakeGetFeatureURL(TRUE);
    CPLDebug("WFS", "%s", osURL.c_str());

    CPLHTTPResult* psResult = poDS->HTTPFetch( osURL, NULL);
    if (psResult == NULL)
    {
        return -1;
    }

    pabyData = (char*) psResult->pabyData;
    psResult->pabyData = NULL;

    if (strstr(pabyData, "<ServiceExceptionReport") != NULL ||
        strstr(pabyData, "<ows:ExceptionReport") != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                 pabyData);
        CPLHTTPDestroyResult(psResult);
        CPLFree(pabyData);
        return -1;
    }

    CPLXMLNode* psXML = CPLParseXMLString( pabyData );
    if (psXML == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid XML content : %s",
                pabyData);
        CPLHTTPDestroyResult(psResult);
        CPLFree(pabyData);
        return -1;
    }

    CPLStripXMLNamespace( psXML, NULL, TRUE );
    CPLXMLNode* psRoot = CPLGetXMLNode( psXML, "=FeatureCollection" );
    if (psRoot == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find <FeatureCollection>");
        CPLDestroyXMLNode( psXML );
        CPLHTTPDestroyResult(psResult);
        CPLFree(pabyData);
        return -1;
    }

    const char* pszValue = CPLGetXMLValue(psRoot, "numberMatched", NULL); /* WFS 2.0.0 */
    if (pszValue == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find numberMatched");
        CPLDestroyXMLNode( psXML );
        CPLHTTPDestroyResult(psResult);
        CPLFree(pabyData);
        return -1;
    }

    GIntBig nFeatures = CPLAtoGIntBig(pszValue);

    CPLDestroyXMLNode( psXML );
    CPLHTTPDestroyResult(psResult);
    CPLFree(pabyData);

    return nFeatures;
}

/************************************************************************/
/*                           GetFeatureCount()                          */
/************************************************************************/

GIntBig OGRWFSJoinLayer::GetFeatureCount( int bForce )
{
    GIntBig nFeatures = ExecuteGetFeatureResultTypeHits();
    if (nFeatures >= 0)
        return nFeatures;

    nFeatures = OGRLayer::GetFeatureCount(bForce);
    return nFeatures;
}

/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn* OGRWFSJoinLayer::GetLayerDefn()
{
    return poFeatureDefn;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRWFSJoinLayer::TestCapability( const char * )
{
    return FALSE;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRWFSJoinLayer::SetSpatialFilter( OGRGeometry * poGeom )
{
    if( poGeom != NULL )
        CPLError(CE_Failure, CPLE_NotSupported,
                "Setting a spatial filter on a layer resulting from a WFS join is unsupported");
}

/************************************************************************/
/*                          SetAttributeFilter()                        */
/************************************************************************/

OGRErr OGRWFSJoinLayer::SetAttributeFilter( const char *pszFilter )
{
    if( pszFilter != NULL )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                "Setting an attribute filter on a layer resulting from a WFS join is unsupported");
        return OGRERR_FAILURE;
    }
    return OGRERR_NONE;
}
