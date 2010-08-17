/******************************************************************************
 * $Id$
 *
 * Project:  WFS Translator
 * Purpose:  Implements OGRWFSLayer class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault <even dot rouault at mines dash paris dot org>
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

#include "ogr_api.h"
#include "cpl_minixml.h"
#include "cpl_http.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            OGRWFSLayer()                             */
/************************************************************************/

OGRWFSLayer::OGRWFSLayer( OGRWFSDataSource* poDS,
                          OGRSpatialReference* poSRS,
                          int bAxisOrderAlreadyInverted,
                          const char* pszBaseURL,
                          const char* pszName,
                          const char* pszNS,
                          const char* pszNSVal )

{
    this->poDS = poDS;
    this->poSRS = poSRS;
    this->bAxisOrderAlreadyInverted = bAxisOrderAlreadyInverted;
    this->pszBaseURL = CPLStrdup(pszBaseURL);
    this->pszName = CPLStrdup(pszName);
    this->pszNS = pszNS ? CPLStrdup(pszNS) : NULL;
    this->pszNSVal = pszNSVal ? CPLStrdup(pszNSVal) : NULL;

    poFeatureDefn = NULL;
    bGotApproximateLayerDefn = FALSE;

    poBaseDS = NULL;
    poBaseLayer = NULL;
    bReloadNeeded = FALSE;
    bHasFetched = FALSE;
    eGeomType = wkbUnknown;
    nFeatures = -1;

    dfMinX = dfMinY = dfMaxX = dfMaxY = 0;
    bHasExtents = FALSE;
    poFetchedFilterGeom = NULL;
}

/************************************************************************/
/*                            ~OGRWFSLayer()                            */
/************************************************************************/

OGRWFSLayer::~OGRWFSLayer()

{
    if( poSRS != NULL )
        poSRS->Release();

    if (poFeatureDefn != NULL)
        poFeatureDefn->Release();

    CPLFree(pszBaseURL);
    CPLFree(pszName);
    CPLFree(pszNS);
    CPLFree(pszNSVal);

    OGRDataSource::DestroyDataSource(poBaseDS);

    delete poFetchedFilterGeom;

    CPLString osTmpFileName;
    osTmpFileName = CPLSPrintf("/vsimem/tempwfs_%p.gfs", this);
    VSIUnlink(osTmpFileName.c_str());
    osTmpFileName = CPLSPrintf("/vsimem/tempwfs_%p.gml", this);
    VSIUnlink(osTmpFileName.c_str());
    osTmpFileName = CPLSPrintf("/vsimem/tempwfs_%p.xsd", this);
    VSIUnlink(osTmpFileName.c_str());
    osTmpFileName = CPLSPrintf("/vsimem/tempwfs_%p.geojson", this);
    VSIUnlink(osTmpFileName.c_str());
}

/************************************************************************/
/*                    GetDescribeFeatureTypeURL()                       */
/************************************************************************/

CPLString OGRWFSLayer::GetDescribeFeatureTypeURL()
{
    CPLString osURL(pszBaseURL);
    osURL = WFS_AddKVToURL(osURL, "SERVICE", "WFS");
    osURL = WFS_AddKVToURL(osURL, "VERSION", poDS->GetVersion());
    osURL = WFS_AddKVToURL(osURL, "REQUEST", "DescribeFeatureType");
    osURL = WFS_AddKVToURL(osURL, "TYPENAME", pszName);
    osURL = WFS_AddKVToURL(osURL, "PROPERTYNAME", NULL);
    return osURL;
}

/************************************************************************/
/*                      DescribeFeatureType()                           */
/************************************************************************/

OGRFeatureDefn* OGRWFSLayer::DescribeFeatureType()
{
    CPLString osURL = GetDescribeFeatureTypeURL();

    if (pszNS && poDS->GetNeedNAMESPACE())
    {
        /* Older Deegree version require NAMESPACE (e.g. http://www.nokis.org/deegree2/ogcwebservice) */
        /* This has been now corrected */
        CPLString osValue("xmlns(");
        osValue += pszNS;
        osValue += "=";
        osValue += pszNSVal;
        osValue += ")";
        osURL = WFS_AddKVToURL(osURL, "NAMESPACE", osValue);
    }

    CPLDebug("WFS", "%s", osURL.c_str());

    CPLHTTPResult* psResult = CPLHTTPFetch( osURL, NULL);
    if (psResult == NULL)
    {
        return NULL;
    }
    if (psResult->nStatus != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                 psResult->pszErrBuf);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    if (strstr(psResult->pszContentType, "xml") == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Non XML content returned by server : %s, %s",
                 psResult->pszContentType, psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    if (strstr((const char*)psResult->pabyData, "<ServiceExceptionReport") != NULL)
    {
        if (poDS->IsOldDeegree((const char*)psResult->pabyData))
        {
            CPLHTTPDestroyResult(psResult);
            return DescribeFeatureType();
        }
        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                 psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    CPLXMLNode* psXML = CPLParseXMLString( (const char*) psResult->pabyData );
    if (psXML == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid XML content : %s",
                psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    CPLStripXMLNamespace( psXML, NULL, TRUE );
    CPLXMLNode* psRoot = CPLGetXMLNode( psXML, "=Schema" );
    if (psRoot == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find <Schema>");
        CPLDestroyXMLNode( psXML );
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    osTargetNamespace = CPLGetXMLValue(psRoot, "targetNamespace", "");
    
    CPLDestroyXMLNode(psXML);

    CPLString osTmpFileName;
    FILE* fp;

    osTmpFileName = CPLSPrintf("/vsimem/tempwfs_%p.xsd", this);
    fp = VSIFileFromMemBuffer( osTmpFileName, psResult->pabyData,
                                     psResult->nDataLen, TRUE);
    VSIFCloseL(fp);
    psResult->pabyData = NULL;
    CPLHTTPDestroyResult(psResult);

    osTmpFileName = CPLSPrintf("/vsimem/tempwfs_%p.gml", this);
    fp = VSIFOpenL(osTmpFileName, "wb");
    VSIFPrintfL(fp, "<?xml version=\"1.0\"?><wfs:FeatureCollection xmlns:gml=\"http://www.opengis.net/gml\"></wfs:FeatureCollection>");
    VSIFCloseL(fp);

    OGRDataSource* poDS = (OGRDataSource*) OGROpen(osTmpFileName, FALSE, NULL);
    if (poDS == NULL)
    {
        return NULL;
    }
    if (poDS->GetLayerCount() != 1)
    {
        OGRDataSource::DestroyDataSource(poDS);
        return NULL;
    }

    OGRLayer* poLayer = poDS->GetLayer(0);
    OGRFeatureDefn* poFeatureDefn = poLayer->GetLayerDefn()->Clone();
    osGeometryColumnName = poLayer->GetGeometryColumn();
    OGRDataSource::DestroyDataSource(poDS);

    VSIUnlink(osTmpFileName);

    return poFeatureDefn;
}

/************************************************************************/
/*                         WFS_EscapeURL()                              */
/************************************************************************/

static CPLString WFS_EscapeURL(CPLString osURL)
{
    CPLString osNewURL;
    size_t i;
    for(i=0;i<osURL.size();i++)
    {
        char ch = osURL[i];
        if (ch == '<')
            osNewURL += "%3C";
        else if (ch == '>')
            osNewURL += "%3E";
        else if (ch == ' ')
            osNewURL += "%20";
        else if (ch == '"')
            osNewURL += "%22";
        else if (ch == '%')
            osNewURL += "%25";
        else
            osNewURL += ch;
    }
    return osNewURL;
}

/************************************************************************/
/*                       MakeGetFeatureURL()                            */
/************************************************************************/

CPLString OGRWFSLayer::MakeGetFeatureURL(int nMaxFeatures, int bRequestHits)
{
    CPLString osURL(pszBaseURL);
    osURL = WFS_AddKVToURL(osURL, "SERVICE", "WFS");
    osURL = WFS_AddKVToURL(osURL, "VERSION", poDS->GetVersion());
    osURL = WFS_AddKVToURL(osURL, "REQUEST", "GetFeature");
    osURL = WFS_AddKVToURL(osURL, "TYPENAME", pszName);

    if (nMaxFeatures)
    {
        osURL = WFS_AddKVToURL(osURL, "MAXFEATURES", CPLSPrintf("%d", nMaxFeatures));
    }
    if (pszNS && poDS->GetNeedNAMESPACE())
    {
        /* Older Deegree version require NAMESPACE (e.g. http://www.nokis.org/deegree2/ogcwebservice) */
        /* This has been now corrected */
        CPLString osValue("xmlns(");
        osValue += pszNS;
        osValue += "=";
        osValue += pszNSVal;
        osValue += ")";
        osURL = WFS_AddKVToURL(osURL, "NAMESPACE", osValue);
    }

    delete poFetchedFilterGeom;
    poFetchedFilterGeom = NULL;

    CPLString osGeomFilter;

    if (m_poFilterGeom != NULL && osGeometryColumnName.size() > 0)
    {
        OGREnvelope oEnvelope;
        m_poFilterGeom->getEnvelope(&oEnvelope);

        poFetchedFilterGeom = m_poFilterGeom->clone();

        osGeomFilter = "<BBOX>";
        osGeomFilter += "<PropertyName>";
        if (pszNS)
        {
            osGeomFilter += pszNS;
            osGeomFilter += ":";
        }
        osGeomFilter += osGeometryColumnName;
        osGeomFilter += "</PropertyName>";
        osGeomFilter += "<gml:Box>";
        osGeomFilter += "<gml:coordinates>";
        if (bAxisOrderAlreadyInverted)
        {
            /* We can go here in WFS 1.1 with geographic coordinate systems */
            /* that are natively return in lat,long order, but as we have */
            /* presented long,lat order to the user, we must switch back */
            /* for the server... */
            osGeomFilter += CPLSPrintf("%.16f,%.16f %.16f,%.16f", oEnvelope.MinY, oEnvelope.MinX, oEnvelope.MaxY, oEnvelope.MaxX);
        }
        else
            osGeomFilter += CPLSPrintf("%.16f,%.16f %.16f,%.16f", oEnvelope.MinX, oEnvelope.MinY, oEnvelope.MaxX, oEnvelope.MaxY);
        osGeomFilter += "</gml:coordinates>";
        osGeomFilter += "</gml:Box>";
        osGeomFilter += "</BBOX>";
    }

    if (osGeomFilter.size() != 0 || osWFSWhere.size() != 0)
    {
        CPLString osFilter = "<Filter ";
        if (pszNS)
        {
            osFilter += "xmlns:";
            osFilter += pszNS;
            osFilter += "=\"";
            osFilter += pszNSVal;
            osFilter += "\"";
        }
        osFilter += " xmlns:gml=\"http://www.opengis.net/gml\">";
        if (osGeomFilter.size() != 0 && osWFSWhere.size() != 0)
            osFilter += "<And>";
        osFilter += osWFSWhere;
        osFilter += osGeomFilter;
        if (osGeomFilter.size() != 0 && osWFSWhere.size() != 0)
            osFilter += "</And>";
        osFilter += "</Filter>";

        osURL = WFS_AddKVToURL(osURL, "FILTER", osFilter);
        osURL = WFS_EscapeURL(osURL);
    }
        
    if (bRequestHits)
    {
        osURL = WFS_AddKVToURL(osURL, "RESULTTYPE", "hits");
    }

    return osURL;
}

/************************************************************************/
/*                         FetchGetFeature()                            */
/************************************************************************/

OGRDataSource* OGRWFSLayer::FetchGetFeature(int nMaxFeatures)
{

    CPLString osURL = MakeGetFeatureURL(nMaxFeatures, FALSE);
    CPLDebug("WFS", "%s", osURL.c_str());

    CPLHTTPResult* psResult = CPLHTTPFetch( osURL, NULL);
    if (psResult == NULL)
    {
        return NULL;
    }
    if (psResult->nStatus != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                 psResult->pszErrBuf);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    int bJSON = FALSE;
    if (strstr(psResult->pszContentType, "application/json") != NULL &&
        strcmp(WFS_FetchValueFromURL(osURL, "OUTPUTFORMAT"), "json") == 0)
    {
        bJSON = TRUE;
    }
    else if (strstr(psResult->pszContentType, "xml") == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Non XML content returned by server : %s, %s",
                 psResult->pszContentType, psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    if (strstr((const char*)psResult->pabyData, "<ServiceExceptionReport") != NULL)
    {
        if (poDS->IsOldDeegree((const char*)psResult->pabyData))
        {
            CPLHTTPDestroyResult(psResult);
            return FetchGetFeature(nMaxFeatures);
        }

        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                 psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    /* Deegree server does not support PropertyIsNotEqualTo */
    /* We have to turn it into <Not><PropertyIsEqualTo> */
    if (osWFSWhere.size() != 0 && poDS->PropertyIsNotEqualToSupported() &&
        strstr((const char*)psResult->pabyData, "Unknown comparison operation: 'PropertyIsNotEqualTo'") != NULL)
    {
        poDS->SetPropertyIsNotEqualToUnSupported();

        SetAttributeFilter(osSQLWhere);
        bHasFetched = TRUE;
        bReloadNeeded = FALSE;
        
        CPLHTTPDestroyResult(psResult);
        return FetchGetFeature(nMaxFeatures);
    }

    CPLString osTmpFileName;

    osTmpFileName = CPLSPrintf("/vsimem/tempwfs_%p.gfs", this);
    VSIUnlink(osTmpFileName.c_str());

    if (bJSON)
        osTmpFileName = CPLSPrintf("/vsimem/tempwfs_%p.geojson", this);
    else
        osTmpFileName = CPLSPrintf("/vsimem/tempwfs_%p.gml", this);

    GByte* pData = psResult->pabyData;
    FILE *fp = VSIFileFromMemBuffer( osTmpFileName, psResult->pabyData,
                                     psResult->nDataLen, TRUE);
    VSIFCloseL(fp);
    psResult->pabyData = NULL;
    CPLHTTPDestroyResult(psResult);

    OGRDataSource* poDS = (OGRDataSource*) OGROpen(osTmpFileName, FALSE, NULL);
    if (poDS == NULL)
    {
        if (!bJSON && strstr((const char*)pData, "<wfs:FeatureCollection") == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Error: %s", pData);
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
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn * OGRWFSLayer::GetLayerDefn()
{
    if (poFeatureDefn)
        return poFeatureDefn;

    poFeatureDefn = new OGRFeatureDefn( pszName );
    poFeatureDefn->Reference();

    OGRDataSource* poDS = NULL;

    OGRFeatureDefn* poSrcFDefn = NULL;
    if (strcmp(WFS_FetchValueFromURL(pszBaseURL, "OUTPUTFORMAT"), "json") != 0)
        poSrcFDefn = DescribeFeatureType();
    if (poSrcFDefn == NULL)
    {
        poDS = FetchGetFeature(1);
        if (poDS == NULL)
        {
            return poFeatureDefn;
        }
        poSrcFDefn = poDS->GetLayer(0)->GetLayerDefn();
        bGotApproximateLayerDefn = TRUE;
    }

    const char* pszPropertyName = WFS_FetchValueFromURL(pszBaseURL, "PROPERTYNAME");

    int i;
    poFeatureDefn->SetGeomType(poSrcFDefn->GetGeomType());
    for(i=0;i<poSrcFDefn->GetFieldCount();i++)
    {
        if (pszPropertyName[0] != 0)
        {
            if (strstr(pszPropertyName,
                       poSrcFDefn->GetFieldDefn(i)->GetNameRef()) != NULL)
                poFeatureDefn->AddFieldDefn(poSrcFDefn->GetFieldDefn(i));
            else
                bGotApproximateLayerDefn = TRUE;
        }
        else
        {
            poFeatureDefn->AddFieldDefn(poSrcFDefn->GetFieldDefn(i));
        }
    }

    if (poDS)
        OGRDataSource::DestroyDataSource(poDS);
    else
        delete poSrcFDefn;

    return poFeatureDefn;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRWFSLayer::GetSpatialRef()
{
    GetLayerDefn();
    return poSRS;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRWFSLayer::ResetReading()

{
    GetLayerDefn();
    if (bReloadNeeded)
    {
        delete poBaseDS;
        poBaseDS = NULL;
        poBaseLayer = NULL;
        bHasFetched = FALSE;
        bReloadNeeded = FALSE;
    }
    if (poBaseLayer)
        poBaseLayer->ResetReading();
}


/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRWFSLayer::GetNextFeature()
{
    GetLayerDefn();
    if (bReloadNeeded)
    {
        delete poBaseDS;
        poBaseDS = NULL;
        poBaseLayer = NULL;
        bHasFetched = FALSE;
        bReloadNeeded = FALSE;
    }
    if (poBaseDS == NULL && !bHasFetched)
    {
        bHasFetched = TRUE;
        poBaseDS = FetchGetFeature(0);
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
        OGRGeometry* poGeom = poSrcFeature->GetGeometryRef();
        if( m_poFilterGeom != NULL && poGeom != NULL &&
            !FilterGeometry( poGeom ) )
        {
            delete poSrcFeature;
            continue;
        }
        if( m_poAttrQuery != NULL
            && !m_poAttrQuery->Evaluate( poSrcFeature ) )
        {
            delete poSrcFeature;
            continue;
        }
        OGRFeature* poNewFeature = new OGRFeature(poFeatureDefn);
        if (bGotApproximateLayerDefn)
        {
            poNewFeature->SetFrom(poSrcFeature);
        }
        else
        {
            int iField;
            for(iField = 0;iField < poFeatureDefn->GetFieldCount(); iField++)
                poNewFeature->SetField( iField, poSrcFeature->GetRawFieldRef(iField) );
            poNewFeature->SetStyleString(poSrcFeature->GetStyleString());
            poNewFeature->SetGeometryDirectly(poSrcFeature->StealGeometry());
        }
        poNewFeature->SetFID(poSrcFeature->GetFID());
        poGeom = poNewFeature->GetGeometryRef();

        if (bAxisOrderAlreadyInverted &&
            strcmp(poBaseDS->GetDriver()->GetName(), "GML") != 0)
        {
            poGeom->swapXY();
        }

        if (poGeom && poSRS)
            poGeom->assignSpatialReference(poSRS);
        delete poSrcFeature;
        return poNewFeature;
    }
}

/************************************************************************/
/*                         SetSpatialFilter()                           */
/************************************************************************/

void OGRWFSLayer::SetSpatialFilter( OGRGeometry * poGeom )
{
    if (poFetchedFilterGeom == NULL && poBaseDS != NULL)
    {
        /* If there was no filter set, and that we set one */
        /* the new result set can only be a subset of the whole */
        /* so no need to reload from source */
        bReloadNeeded = FALSE;
    }
    else if (poFetchedFilterGeom != NULL && poGeom != NULL && poBaseDS != NULL)
    {
        OGREnvelope oOldEnvelope, oNewEnvelope;
        poFetchedFilterGeom->getEnvelope(&oOldEnvelope);
        poGeom->getEnvelope(&oNewEnvelope);
        /* Optimization : we don't need to request the server */
        /* if the new BBOX is inside the old BBOX as we have */
        /* already all the features */
        bReloadNeeded = ! oOldEnvelope.Contains(oNewEnvelope);
    }
    else
        bReloadNeeded = TRUE;
    nFeatures = -1;
    OGRLayer::SetSpatialFilter(poGeom);
    ResetReading();
}

/************************************************************************/
/*                        SetAttributeFilter()                          */
/************************************************************************/

OGRErr OGRWFSLayer::SetAttributeFilter( const char * pszFilter )
{
    OGRErr eErr = OGRLayer::SetAttributeFilter(pszFilter);
    CPLString osOldWFSWhere(osWFSWhere);
    if (poDS->HasMinOperators() && pszFilter != NULL)
    {
        int bNeedsNullCheck = FALSE;
        int nVersion = (strcmp(poDS->GetVersion(),"1.0.0") == 0) ? 100 : 110;
        osWFSWhere = WFS_TurnSQLFilterToOGCFilter(pszFilter,
                                              nVersion,
                                              poDS->PropertyIsNotEqualToSupported(),
                                              &bNeedsNullCheck);
        if (bNeedsNullCheck && !poDS->HasNullCheck())
            osWFSWhere = "";
        if (osWFSWhere.size() == 0)
        {
            CPLDebug("WFS", "Using client-side only mode for filter \"%s\"", pszFilter);
        }
    }
    else
        osWFSWhere = "";

    osSQLWhere = (pszFilter) ? pszFilter : "";

    if (osWFSWhere != osOldWFSWhere)
        bReloadNeeded = TRUE;
    else
        bReloadNeeded = FALSE;
    nFeatures = -1;
    return eErr;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRWFSLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCFastFeatureCount) )
    {
        if (nFeatures >= 0)
            return TRUE;

        return poBaseLayer != NULL && m_poFilterGeom == NULL &&
               m_poAttrQuery == NULL &&  poBaseLayer->TestCapability(pszCap);
    }

    else if( EQUAL(pszCap,OLCFastGetExtent) )
    {
        if (bHasExtents && m_poFilterGeom == NULL)
            return TRUE;

        return poBaseLayer != NULL && m_poFilterGeom == NULL &&
               poBaseLayer->TestCapability(pszCap);
    }

    else if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return poBaseLayer != NULL && poBaseLayer->TestCapability(pszCap);

    else if( EQUAL(pszCap, OLCSequentialWrite) ||
             EQUAL(pszCap, OLCDeleteFeature) )
    {
        GetLayerDefn();
        return poDS->SupportTransactions() && poDS->UpdateMode() && poFeatureDefn->GetFieldIndex("gml_id") == 0;
    }

    return FALSE;
}

/************************************************************************/
/*                  ExecuteGetFeatureResultTypeHits()                   */
/************************************************************************/

int OGRWFSLayer::ExecuteGetFeatureResultTypeHits()
{
    CPLString osURL = MakeGetFeatureURL(0, TRUE);
    CPLDebug("WFS", "%s", osURL.c_str());

    CPLHTTPResult* psResult = CPLHTTPFetch( osURL, NULL);
    if (psResult == NULL)
    {
        return -1;
    }
    if (psResult->nStatus != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                psResult->pszErrBuf);
        CPLHTTPDestroyResult(psResult);
        return -1;
    }

    if (strstr((const char*)psResult->pabyData, "<ServiceExceptionReport") != NULL)
    {
        if (poDS->IsOldDeegree((const char*)psResult->pabyData))
        {
            CPLHTTPDestroyResult(psResult);
            return ExecuteGetFeatureResultTypeHits();
        }
        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                 psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return -1;
    }

    if (strstr(psResult->pszContentType, "xml") == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Non XML content returned by server : %s, %s",
                psResult->pszContentType, psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return -1;
    }

    CPLXMLNode* psXML = CPLParseXMLString( (const char*) psResult->pabyData );
    if (psXML == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid XML content : %s",
                psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return -1;
    }

    CPLStripXMLNamespace( psXML, NULL, TRUE );
    CPLXMLNode* psRoot = CPLGetXMLNode( psXML, "=FeatureCollection" );
    if (psRoot == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find <FeatureCollection>");
        CPLDestroyXMLNode( psXML );
        CPLHTTPDestroyResult(psResult);
        return -1;
    }

    const char* pszValue = CPLGetXMLValue(psRoot, "numberOfFeatures", NULL);
    if (pszValue == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find numberOfFeatures");
        CPLDestroyXMLNode( psXML );
        CPLHTTPDestroyResult(psResult);
        return -1;
    }

    int nFeatures = atoi(pszValue);

    CPLDestroyXMLNode( psXML );
    CPLHTTPDestroyResult(psResult);

    return nFeatures;
}

/************************************************************************/
/*                           GetFeatureCount()                          */
/************************************************************************/

int OGRWFSLayer::GetFeatureCount( int bForce )
{
    if (nFeatures >= 0)
        return nFeatures;

    if (TestCapability(OLCFastFeatureCount))
        return poBaseLayer->GetFeatureCount(bForce);

    if ((m_poAttrQuery == NULL || osWFSWhere.size() != 0) &&
         poDS->GetFeatureSupportHits() &&
         strcmp(WFS_FetchValueFromURL(pszBaseURL, "OUTPUTFORMAT"), "json") != 0)
    {
        nFeatures = ExecuteGetFeatureResultTypeHits();
        if (nFeatures >= 0)
            return nFeatures;
    }
    
    nFeatures = OGRLayer::GetFeatureCount(bForce);
    return nFeatures;
}


/************************************************************************/
/*                              SetExtent()                             */
/************************************************************************/

void OGRWFSLayer::SetExtents(double dfMinX, double dfMinY, double dfMaxX, double dfMaxY)
{
    this->dfMinX = dfMinX;
    this->dfMinY = dfMinY;
    this->dfMaxX = dfMaxX;
    this->dfMaxY = dfMaxY;
    bHasExtents = TRUE;
}

/************************************************************************/
/*                              GetExtent()                             */
/************************************************************************/

OGRErr OGRWFSLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    if (bHasExtents && m_poFilterGeom == NULL)
    {
        psExtent->MinX = dfMinX;
        psExtent->MinY = dfMinY;
        psExtent->MaxX = dfMaxX;
        psExtent->MaxY = dfMaxY;
        return OGRERR_NONE;
    }

    if (TestCapability(OLCFastGetExtent))
        return poBaseLayer->GetExtent(psExtent, bForce);

    return OGRLayer::GetExtent(psExtent, bForce);
}

/************************************************************************/
/*                          GetShortName()                              */
/************************************************************************/

const char* OGRWFSLayer::GetShortName()
{
    const char* pszShortName = strchr(pszName, ':');
    if (pszShortName == NULL)
        pszShortName = pszName;
    else
        pszShortName ++;
    return pszShortName;
}

/************************************************************************/
/*                          CreateFeature()                             */
/************************************************************************/

OGRErr OGRWFSLayer::CreateFeature( OGRFeature *poFeature )
{
    if (!TestCapability(OLCSequentialWrite))
    {
        if (!poDS->SupportTransactions())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "CreateFeature() not supported: no WMS-T features advertized by server");
        else if (!poDS->UpdateMode())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "CreateFeature() not supported: datasource opened as read-only");
        return OGRERR_FAILURE;
    }

    if (poFeatureDefn->GetFieldIndex("gml_id") != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find gml_id field");
        return OGRERR_FAILURE;
    }

    if (poFeature->IsFieldSet(0))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot insert a feature when gml_id field is already seet");
        return OGRERR_FAILURE;
    }

    CPLString osPost;
    osPost += "<?xml version=\"1.0\"?>\n";
    osPost += "<wfs:Transaction xmlns:wfs=\"http://www.opengis.net/wfs\"\n";
    osPost += "                 xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n";
    osPost += "                 service=\"WFS\" version=\""; osPost += poDS->GetVersion(); osPost += "\"\n";
    osPost += "                 xmlns:gml=\"http://www.opengis.net/gml\"\n";
    osPost += "                 xmlns:ogc=\"http://www.opengis.net/ogc\"\n";
    osPost += "                 xsi:schemaLocation=\"http://www.opengis.net/wfs http://schemas.opengis.net/wfs/";
    osPost += poDS->GetVersion();
    osPost += "/wfs.xsd ";
    osPost += osTargetNamespace;
    osPost += " ";

    char* pszXMLEncoded = CPLEscapeString(
                    GetDescribeFeatureTypeURL(), -1, CPLES_XML);
    osPost += pszXMLEncoded;
    CPLFree(pszXMLEncoded);

    const char* pszShortName = GetShortName();
    
    osPost += "\">\n";
    osPost += "  <wfs:Insert>\n";
    osPost += "    <feature:"; osPost += pszShortName; osPost += " xmlns:feature=\"";
    osPost += osTargetNamespace; osPost += "\">\n";

    int i;
    for(i=1; i < poFeature->GetFieldCount(); i++)
    {
        if (poFeature->IsFieldSet(i))
        {
            OGRFieldDefn* poFDefn = poFeature->GetFieldDefnRef(i);
            osPost += "      <feature:";
            osPost += poFDefn->GetNameRef();
            osPost += ">";
            if (poFDefn->GetType() == OFTInteger)
                osPost += CPLSPrintf("%d", poFeature->GetFieldAsInteger(i));
            else if (poFDefn->GetType() == OFTReal)
                osPost += CPLSPrintf("%.16g", poFeature->GetFieldAsDouble(i));
            else
            {
                pszXMLEncoded = CPLEscapeString(poFeature->GetFieldAsString(i),
                                                -1, CPLES_XML);
                osPost += pszXMLEncoded;
                CPLFree(pszXMLEncoded);
            }
            osPost += "</feature:";
            osPost += poFDefn->GetNameRef();
            osPost += ">\n";
        }
    }

    OGRGeometry* poGeom = poFeature->GetGeometryRef();
    if (poGeom != NULL && osGeometryColumnName.size() != 0)
    {
        if (poGeom->getSpatialReference() == NULL)
            poGeom->assignSpatialReference(poSRS);
        char* pszGML = OGR_G_ExportToGML((OGRGeometryH)poGeom);
        osPost += "      <feature:"; osPost += osGeometryColumnName; osPost += ">";
        osPost += pszGML;
        osPost += "</feature:"; osPost += osGeometryColumnName; osPost += ">\n";
        CPLFree(pszGML);
    }
    
    osPost += "    </feature:"; osPost += pszShortName; osPost += ">\n";
    osPost += "  </wfs:Insert>\n";
    osPost += "</wfs:Transaction>\n";

    CPLDebug("WFS", "Post : %s", osPost.c_str());

    char** papszOptions = NULL;
    papszOptions = CSLAddNameValue(papszOptions, "POSTFIELDS", osPost.c_str());
    papszOptions = CSLAddNameValue(papszOptions, "HEADERS",
                                   "Content-Type: application/xml; charset=UTF-8");
    CPLString osURL(pszBaseURL);
    const char* pszEsperluet = strchr(pszBaseURL, '?');
    if (pszEsperluet)
        osURL.resize(pszEsperluet - pszBaseURL);
    CPLHTTPResult* psResult = CPLHTTPFetch(osURL.c_str(), papszOptions);
    CSLDestroy(papszOptions);

    if (psResult == NULL)
    {
        return OGRERR_FAILURE;
    }
    if (psResult->nStatus != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                psResult->pszErrBuf);
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    if (strstr((const char*)psResult->pabyData,
                                    "<ServiceExceptionReport") != NULL ||
        strstr((const char*)psResult->pabyData,
                                    "<ows:ExceptionReport") != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                 psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    if (strstr(psResult->pszContentType, "xml") == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Non XML content returned by server : %s, %s",
                psResult->pszContentType, psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    CPLDebug("WFS", "Response: %s", psResult->pabyData);

    CPLXMLNode* psXML = CPLParseXMLString( (const char*) psResult->pabyData );
    if (psXML == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid XML content : %s",
                psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    CPLStripXMLNamespace( psXML, NULL, TRUE );
    CPLXMLNode* psRoot = CPLGetXMLNode( psXML, "=TransactionResponse" );
    if (psRoot == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find <TransactionResponse>");
        CPLDestroyXMLNode( psXML );
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    CPLXMLNode* psFeatureID =
    CPLGetXMLNode( psRoot, "InsertResults.Feature.FeatureId");
    if (psFeatureID == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find InsertResults.Feature.FeatureId");
        CPLDestroyXMLNode( psXML );
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    const char* pszFID = CPLGetXMLValue(psFeatureID, "fid", NULL);
    if (pszFID == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find fid");
        CPLDestroyXMLNode( psXML );
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    poFeature->SetField("gml_id", pszFID);

    /* If the returned fid is of the form layer_name.num, then use */
    /* num as the OGR FID */
    if (strncmp(pszFID, pszShortName, strlen(pszShortName)) == 0 &&
        pszFID[strlen(pszShortName)] == '.')
        poFeature->SetFID(atoi(pszFID + strlen(pszShortName) + 1));

    CPLDebug("WFS", "Got FID = %ld", poFeature->GetFID());

    CPLDestroyXMLNode( psXML );
    CPLHTTPDestroyResult(psResult);

    return OGRERR_NONE;
}
/************************************************************************/
/*                               GetFeature()                           */
/************************************************************************/

OGRFeature* OGRWFSLayer::GetFeature(long nFID)
{
    GetLayerDefn();
    if (poBaseLayer == NULL && poFeatureDefn->GetFieldIndex("gml_id") == 0)
    {
        /* This is lovely hackish. We assume that then gml_id will be */
        /* layer_name.number. This is actually what we can observe with */
        /* GeoServer and TinyOWS */
        CPLString osVal = CPLSPrintf("gml_id = %s.%ld", GetShortName(), nFID);
        CPLString osOldSQLWhere(osSQLWhere);
        SetAttributeFilter(osVal);
        OGRFeature* poFeature = GetNextFeature();
        const char* pszOldFilter = osOldSQLWhere.size() ? osOldSQLWhere.c_str() : NULL;
        SetAttributeFilter(pszOldFilter);
        if (poFeature)
            return poFeature;
    }

    return OGRLayer::GetFeature(nFID);
}

/************************************************************************/
/*                            DeleteFeature()                           */
/************************************************************************/

OGRErr OGRWFSLayer::DeleteFeature( long nFID )
{
    if (!TestCapability(OLCDeleteFeature))
    {
        if (!poDS->SupportTransactions())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "DeleteFeature() not supported: no WMS-T features advertized by server");
        else if (!poDS->UpdateMode())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "DeleteFeature() not supported: datasource opened as read-only");
        return OGRERR_FAILURE;
    }

    if (poFeatureDefn->GetFieldIndex("gml_id") != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find gml_id field");
        return OGRERR_FAILURE;
    }

    OGRFeature* poFeature = GetFeature(nFID);
    if (poFeature == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find feature %ld", nFID);
        return OGRERR_FAILURE;
    }

    const char* pszGMLID = poFeature->GetFieldAsString("gml_id");
    if (pszGMLID == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot delete a feature with gml_id unset");
        delete poFeature;
        return OGRERR_FAILURE;
    }

    CPLString osGMLID = pszGMLID;
    pszGMLID = NULL;
    delete poFeature;
    poFeature = NULL;

    CPLString osRootURL(pszBaseURL);
    const char* pszFirstSlashAfterHttpRadix = strchr(pszBaseURL + 7, '/');
    if (pszFirstSlashAfterHttpRadix)
        osRootURL.resize(pszFirstSlashAfterHttpRadix - pszBaseURL + 1);

    CPLString osPost;
    osPost += "<?xml version=\"1.0\"?>\n";
    osPost += "<wfs:Transaction xmlns:wfs=\"http://www.opengis.net/wfs\"\n";
    osPost += "                 xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n";
    osPost += "                 service=\"WFS\" version=\""; osPost += poDS->GetVersion(); osPost += "\"\n";
    osPost += "                 xmlns:gml=\"http://www.opengis.net/gml\"\n";
    osPost += "                 xmlns:ogc=\"http://www.opengis.net/ogc\"\n";
    osPost += "                 xsi:schemaLocation=\"http://www.opengis.net/wfs http://schemas.opengis.net/wfs/";
    osPost += poDS->GetVersion();
    osPost += "/wfs.xsd ";
    osPost += osTargetNamespace;
    osPost += " ";

    char* pszXMLEncoded = CPLEscapeString(
                    GetDescribeFeatureTypeURL(), -1, CPLES_XML);
    osPost += pszXMLEncoded;
    CPLFree(pszXMLEncoded);

    const char* pszShortName = GetShortName();

    osPost += "\">\n";

    osPost += "  <wfs:Delete xmlns:feature=\""; osPost += osTargetNamespace;
    osPost += "\" typeName=\"feature:"; osPost += pszShortName; osPost += "\">\n";
    osPost += "    <ogc:Filter>\n";
    osPost += "      <ogc:FeatureId fid=\""; osPost += osGMLID; osPost += "\"/>\n";
    osPost += "    </ogc:Filter>\n";
    osPost += "  </wfs:Delete>\n";
    osPost += "</wfs:Transaction>\n";

    CPLDebug("WFS", "Post : %s", osPost.c_str());

    char** papszOptions = NULL;
    papszOptions = CSLAddNameValue(papszOptions, "POSTFIELDS", osPost.c_str());
    papszOptions = CSLAddNameValue(papszOptions, "HEADERS",
                                   "Content-Type: application/xml; charset=UTF-8");
    CPLString osURL(pszBaseURL);
    const char* pszEsperluet = strchr(pszBaseURL, '?');
    if (pszEsperluet)
        osURL.resize(pszEsperluet - pszBaseURL);
    CPLHTTPResult* psResult = CPLHTTPFetch(osURL.c_str(), papszOptions);
    CSLDestroy(papszOptions);

    if (psResult == NULL)
    {
        return OGRERR_FAILURE;
    }
    if (psResult->nStatus != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                psResult->pszErrBuf);
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    if (strstr((const char*)psResult->pabyData, "<ServiceExceptionReport") != NULL ||
        strstr((const char*)psResult->pabyData, "<ows:ExceptionReport") != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                 psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    if (strstr(psResult->pszContentType, "xml") == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Non XML content returned by server : %s, %s",
                psResult->pszContentType, psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    CPLDebug("WFS", "Response: %s", psResult->pabyData);

    CPLXMLNode* psXML = CPLParseXMLString( (const char*) psResult->pabyData );
    if (psXML == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid XML content : %s",
                psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    CPLStripXMLNamespace( psXML, NULL, TRUE );
    CPLXMLNode* psRoot = CPLGetXMLNode( psXML, "=TransactionResponse" );
    if (psRoot == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find <TransactionResponse>");
        CPLDestroyXMLNode( psXML );
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    CPLDestroyXMLNode( psXML );
    CPLHTTPDestroyResult(psResult);

    return OGRERR_NONE;
}
