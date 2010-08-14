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
/*                      DescribeFeatureType()                           */
/************************************************************************/

OGRFeatureDefn* OGRWFSLayer::DescribeFeatureType()
{
    CPLString osURL(pszBaseURL);
    osURL = AddKVToURL(osURL, "SERVICE", "WFS");
    osURL = AddKVToURL(osURL, "VERSION", poDS->GetVersion());
    osURL = AddKVToURL(osURL, "REQUEST", "DescribeFeatureType");
    osURL = AddKVToURL(osURL, "TYPENAME", pszName);
    osURL = AddKVToURL(osURL, "PROPERTYNAME", NULL);

    if (pszNS && poDS->GetNeedNAMESPACE())
    {
        /* Older Deegree version require NAMESPACE (e.g. http://www.nokis.org/deegree2/ogcwebservice) */
        /* This has been now corrected */
        CPLString osValue("xmlns(");
        osValue += pszNS;
        osValue += "=";
        osValue += pszNSVal;
        osValue += ")";
        osURL = AddKVToURL(osURL, "NAMESPACE", osValue);
    }

    CPLDebug("WFS", "%s", osURL.c_str());

    CPLHTTPResult* psResult = CPLHTTPFetch( osURL, NULL);
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
    VSIFPrintfL(fp, "<?xml version=\"1.0\"?><foo xmlns:gml=\"http://www.opengis.net/gml\"></foo>");
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
/*                             EscapeURL()                              */
/************************************************************************/

static CPLString EscapeURL(CPLString osURL)
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
    osURL = AddKVToURL(osURL, "SERVICE", "WFS");
    osURL = AddKVToURL(osURL, "VERSION", poDS->GetVersion());
    osURL = AddKVToURL(osURL, "REQUEST", "GetFeature");
    osURL = AddKVToURL(osURL, "TYPENAME", pszName);

    if (nMaxFeatures)
    {
        osURL = AddKVToURL(osURL, "MAXFEATURES", CPLSPrintf("%d", nMaxFeatures));
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
        osURL = AddKVToURL(osURL, "NAMESPACE", osValue);
    }

    delete poFetchedFilterGeom;
    poFetchedFilterGeom = NULL;

    if (m_poFilterGeom != NULL && osGeometryColumnName.size() > 0)
    {
        OGREnvelope oEnvelope;
        m_poFilterGeom->getEnvelope(&oEnvelope);

        poFetchedFilterGeom = m_poFilterGeom->clone();

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
        osFilter += "<BBOX>";
        osFilter += "<PropertyName>";
        if (pszNS)
        {
            osFilter += pszNS;
            osFilter += ":";
        }
        osFilter += osGeometryColumnName;
        osFilter += "</PropertyName>";
        osFilter += "<gml:Box>";
        osFilter += "<gml:coordinates>";
        if (bAxisOrderAlreadyInverted)
        {
            /* We can go here in WFS 1.1 with geographic coordinate systems */
            /* that are natively return in lat,long order, but as we have */
            /* presented long,lat order to the user, we must switch back */
            /* for the server... */
            osFilter += CPLSPrintf("%.16f,%.16f %.16f,%.16f", oEnvelope.MinY, oEnvelope.MinX, oEnvelope.MaxY, oEnvelope.MaxX);
        }
        else
            osFilter += CPLSPrintf("%.16f,%.16f %.16f,%.16f", oEnvelope.MinX, oEnvelope.MinY, oEnvelope.MaxX, oEnvelope.MaxY);
        osFilter += "</gml:coordinates>";
        osFilter += "</gml:Box>";
        osFilter += "</BBOX>";
        osFilter += "</Filter>";

        osURL = AddKVToURL(osURL, "FILTER", osFilter);
        osURL = EscapeURL(osURL);
    }
    if (bRequestHits)
    {
        osURL = AddKVToURL(osURL, "RESULTTYPE", "hits");
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
    if (psResult->nStatus != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                 psResult->pszErrBuf);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    int bJSON = FALSE;
    if (strstr(psResult->pszContentType, "application/json") != NULL &&
        strcmp(FetchValueFromURL(osURL, "OUTPUTFORMAT"), "json") == 0)
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
    if (strcmp(FetchValueFromURL(pszBaseURL, "OUTPUTFORMAT"), "json") != 0)
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

    const char* pszPropertyName = FetchValueFromURL(pszBaseURL, "PROPERTYNAME");

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
    //bReloadNeeded = TRUE;
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

    if (m_poAttrQuery == NULL && poDS->GetFeatureSupportHits() &&
        strcmp(FetchValueFromURL(pszBaseURL, "OUTPUTFORMAT"), "json") != 0)
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
