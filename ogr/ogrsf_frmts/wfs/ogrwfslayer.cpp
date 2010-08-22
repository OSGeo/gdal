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

#include <vector>

#include "ogr_wfs.h"

#include "ogr_api.h"
#include "cpl_minixml.h"
#include "cpl_http.h"
#include "parsexsd.h"

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
    poGMLFeatureClass = NULL;
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

    nExpectedInserts = 0;
    bInTransaction = FALSE;
}

/************************************************************************/
/*                            ~OGRWFSLayer()                            */
/************************************************************************/

OGRWFSLayer::~OGRWFSLayer()

{
    if (bInTransaction)
        CommitTransaction();

    if( poSRS != NULL )
        poSRS->Release();

    if (poFeatureDefn != NULL)
        poFeatureDefn->Release();
    delete poGMLFeatureClass;

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

CPLString OGRWFSLayer::GetDescribeFeatureTypeURL(int bWithNS)
{
    CPLString osURL(pszBaseURL);
    osURL = WFS_AddKVToURL(osURL, "SERVICE", "WFS");
    osURL = WFS_AddKVToURL(osURL, "VERSION", poDS->GetVersion());
    osURL = WFS_AddKVToURL(osURL, "REQUEST", "DescribeFeatureType");
    osURL = WFS_AddKVToURL(osURL, "TYPENAME", pszName);
    osURL = WFS_AddKVToURL(osURL, "PROPERTYNAME", NULL);
    osURL = WFS_AddKVToURL(osURL, "MAXFEATURES", NULL);
    osURL = WFS_AddKVToURL(osURL, "FILTER", NULL);

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

    return osURL;
}

/************************************************************************/
/*                      DescribeFeatureType()                           */
/************************************************************************/

OGRFeatureDefn* OGRWFSLayer::DescribeFeatureType()
{
    CPLString osURL = GetDescribeFeatureTypeURL(TRUE);

    CPLDebug("WFS", "%s", osURL.c_str());

    CPLHTTPResult* psResult = OGRWFSHTTPFetch( osURL, NULL);
    if (psResult == NULL)
    {
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
    CPLHTTPDestroyResult(psResult);

    CPLXMLNode* psSchema = WFSFindNode(psXML, "schema");
    if (psSchema == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find <Schema>");
        CPLDestroyXMLNode( psXML );

        return NULL;
    }

    OGRFeatureDefn* poFDefn = ParseSchema(psSchema);
    if (poFDefn)
        poDS->SaveLayerSchema(pszName, psSchema);

    CPLDestroyXMLNode( psXML );
    return poFDefn;
}

/************************************************************************/
/*                            ParseSchema()                             */
/************************************************************************/

OGRFeatureDefn* OGRWFSLayer::ParseSchema(CPLXMLNode* psSchema)
{
    osTargetNamespace = CPLGetXMLValue(psSchema, "targetNamespace", "");

    CPLString osTmpFileName;

    osTmpFileName = CPLSPrintf("/vsimem/tempwfs_%p.xsd", this);
    CPLSerializeXMLTreeToFile(psSchema, osTmpFileName);

    std::vector<GMLFeatureClass*> aosClasses;
    int bHaveSchema = GMLParseXSD( osTmpFileName, aosClasses );

    if (bHaveSchema && aosClasses.size() == 1)
    {
        poGMLFeatureClass = aosClasses[0];

        OGRFeatureDefn* poFDefn = new OGRFeatureDefn(poGMLFeatureClass->GetName());
        poFDefn->SetGeomType( (OGRwkbGeometryType)poGMLFeatureClass->GetGeometryType() );

    /* -------------------------------------------------------------------- */
    /*      Added attributes (properties).                                  */
    /* -------------------------------------------------------------------- */
        OGRFieldDefn oField( "gml_id", OFTString );
        poFDefn->AddFieldDefn( &oField );

        for( int iField = 0; iField < poGMLFeatureClass->GetPropertyCount(); iField++ )
        {
            GMLPropertyDefn *poProperty = poGMLFeatureClass->GetProperty( iField );
            OGRFieldType eFType;

            if( poProperty->GetType() == GMLPT_Untyped )
                eFType = OFTString;
            else if( poProperty->GetType() == GMLPT_String )
                eFType = OFTString;
            else if( poProperty->GetType() == GMLPT_Integer )
                eFType = OFTInteger;
            else if( poProperty->GetType() == GMLPT_Real )
                eFType = OFTReal;
            else if( poProperty->GetType() == GMLPT_StringList )
                eFType = OFTStringList;
            else if( poProperty->GetType() == GMLPT_IntegerList )
                eFType = OFTIntegerList;
            else if( poProperty->GetType() == GMLPT_RealList )
                eFType = OFTRealList;
            else
                eFType = OFTString;

            OGRFieldDefn oField( poProperty->GetName(), eFType );
            if ( EQUALN(oField.GetNameRef(), "ogr:", 4) )
                oField.SetName(poProperty->GetName()+4);
            if( poProperty->GetWidth() > 0 )
                oField.SetWidth( poProperty->GetWidth() );
            if( poProperty->GetPrecision() > 0 )
                oField.SetPrecision( poProperty->GetPrecision() );

            poFDefn->AddFieldDefn( &oField );
        }

        const char* pszGeometryColumnName = poGMLFeatureClass->GetGeometryElement();
        if (pszGeometryColumnName)
            osGeometryColumnName = pszGeometryColumnName;

        return poFDefn;
    }
    else if (bHaveSchema)
    {
        std::vector<GMLFeatureClass*>::const_iterator iter = aosClasses.begin();
        std::vector<GMLFeatureClass*>::const_iterator eiter = aosClasses.end();
        while (iter != eiter)
        {
            GMLFeatureClass* poClass = *iter;
            iter ++;
            delete poClass;
        }
    }

    VSIUnlink(osTmpFileName);

    return NULL;
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

    CPLHTTPResult* psResult = OGRWFSHTTPFetch( osURL, NULL);
    if (psResult == NULL)
    {
        return NULL;
    }

    int bJSON = FALSE;
    if (strstr(psResult->pszContentType, "application/json") != NULL &&
        strcmp(WFS_FetchValueFromURL(osURL, "OUTPUTFORMAT"), "json") == 0)
    {
        bJSON = TRUE;
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

    return BuildLayerDefn();
}

/************************************************************************/
/*                          BuildLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn * OGRWFSLayer::BuildLayerDefn(CPLXMLNode* psSchema)
{
    poFeatureDefn = new OGRFeatureDefn( pszName );
    poFeatureDefn->Reference();

    OGRDataSource* poDS = NULL;

    OGRFeatureDefn* poSrcFDefn = NULL;
    if (psSchema)
        poSrcFDefn = ParseSchema(psSchema);
    else if (strcmp(WFS_FetchValueFromURL(pszBaseURL, "OUTPUTFORMAT"), "json") != 0)
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
        OGRDataSource::DestroyDataSource(poBaseDS);
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
        OGRDataSource::DestroyDataSource(poBaseDS);
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
            CPLAssert(poFeatureDefn->GetFieldCount() == poSrcFeature->GetFieldCount() );

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
    if (eErr != CE_None)
        return eErr;

    CPLString osOldWFSWhere(osWFSWhere);
    if (poDS->HasMinOperators() && pszFilter != NULL)
    {
        int bNeedsNullCheck = FALSE;
        int nVersion = (strcmp(poDS->GetVersion(),"1.0.0") == 0) ? 100 : 110;
        osWFSWhere = WFS_TurnSQLFilterToOGCFilter(pszFilter,
                                              nVersion,
                                              poDS->PropertyIsNotEqualToSupported(),
                                              poDS->UseFeatureId(),
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

    return CE_None;
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
             EQUAL(pszCap, OLCDeleteFeature) ||
             EQUAL(pszCap, OLCRandomWrite) )
    {
        GetLayerDefn();
        return poDS->SupportTransactions() && poDS->UpdateMode() &&
               poFeatureDefn->GetFieldIndex("gml_id") == 0;
    }
    else if ( EQUAL(pszCap, OLCTransactions) )
    {
        return poDS->SupportTransactions() && poDS->UpdateMode();
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

    CPLHTTPResult* psResult = OGRWFSHTTPFetch( osURL, NULL);
    if (psResult == NULL)
    {
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
        
        poDS->DisableSupportHits();
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
/*                          GetPostHeader()                             */
/************************************************************************/

CPLString OGRWFSLayer::GetPostHeader()
{
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
                    GetDescribeFeatureTypeURL(FALSE), -1, CPLES_XML);
    osPost += pszXMLEncoded;
    CPLFree(pszXMLEncoded);

    osPost += "\">\n";

    return osPost;
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

    if (poGMLFeatureClass == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot insert feature because we didn't manage to parse the .XSD schema");
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
                 "Cannot insert a feature when gml_id field is already set");
        return OGRERR_FAILURE;
    }

    CPLString osPost;

    const char* pszShortName = GetShortName();

    if (!bInTransaction)
    {
        osPost += GetPostHeader();
        osPost += "  <wfs:Insert>\n";
    }
    osPost += "    <feature:"; osPost += pszShortName; osPost += " xmlns:feature=\"";
    osPost += osTargetNamespace; osPost += "\">\n";

    int i;
    for(i=1; i <= poFeature->GetFieldCount(); i++)
    {
        if (poGMLFeatureClass->GetGeometryAttributeIndex() == i - 1)
        {
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
        }
        if (i == poFeature->GetFieldCount())
            break;

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
                char* pszXMLEncoded = CPLEscapeString(poFeature->GetFieldAsString(i),
                                                -1, CPLES_XML);
                osPost += pszXMLEncoded;
                CPLFree(pszXMLEncoded);
            }
            osPost += "</feature:";
            osPost += poFDefn->GetNameRef();
            osPost += ">\n";
        }

    }
    
    osPost += "    </feature:"; osPost += pszShortName; osPost += ">\n";

    if (!bInTransaction)
    {
        osPost += "  </wfs:Insert>\n";
        osPost += "</wfs:Transaction>\n";
    }
    else
    {
        osGlobalInsert += osPost;
        nExpectedInserts ++;
        return OGRERR_NONE;
    }

    CPLDebug("WFS", "Post : %s", osPost.c_str());

    char** papszOptions = NULL;
    papszOptions = CSLAddNameValue(papszOptions, "POSTFIELDS", osPost.c_str());
    papszOptions = CSLAddNameValue(papszOptions, "HEADERS",
                                   "Content-Type: application/xml; charset=UTF-8");

    CPLHTTPResult* psResult = OGRWFSHTTPFetch(poDS->GetPostTransactionURL(), papszOptions);
    CSLDestroy(papszOptions);

    if (psResult == NULL)
    {
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
    int bUse100Schema = FALSE;
    CPLXMLNode* psRoot = CPLGetXMLNode( psXML, "=TransactionResponse" );
    if (psRoot == NULL)
    {
        psRoot = CPLGetXMLNode( psXML, "=WFS_TransactionResponse" );
        if (psRoot)
            bUse100Schema = TRUE;
    }

    if (psRoot == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find <TransactionResponse>");
        CPLDestroyXMLNode( psXML );
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    CPLXMLNode* psFeatureID = NULL;

    if (bUse100Schema)
    {
        if (CPLGetXMLNode( psRoot, "TransactionResult.Status.FAILED" ))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Insert failed : %s",
                     psResult->pabyData);
            CPLDestroyXMLNode( psXML );
            CPLHTTPDestroyResult(psResult);
            return OGRERR_FAILURE;
        }

        psFeatureID =
            CPLGetXMLNode( psRoot, "InsertResult.FeatureId");
        if (psFeatureID == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot find InsertResult.FeatureId");
            CPLDestroyXMLNode( psXML );
            CPLHTTPDestroyResult(psResult);
            return OGRERR_FAILURE;
        }
    }
    else
    {
        psFeatureID =
            CPLGetXMLNode( psRoot, "InsertResults.Feature.FeatureId");
        if (psFeatureID == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot find InsertResults.Feature.FeatureId");
            CPLDestroyXMLNode( psXML );
            CPLHTTPDestroyResult(psResult);
            return OGRERR_FAILURE;
        }
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

    /* Invalidate layer */
    bReloadNeeded = TRUE;
    nFeatures = -1;

    return OGRERR_NONE;
}


/************************************************************************/
/*                             SetFeature()                             */
/************************************************************************/

OGRErr OGRWFSLayer::SetFeature( OGRFeature *poFeature )
{
    if (!TestCapability(OLCRandomWrite))
    {
        if (!poDS->SupportTransactions())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "SetFeature() not supported: no WMS-T features advertized by server");
        else if (!poDS->UpdateMode())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "SetFeature() not supported: datasource opened as read-only");
        return OGRERR_FAILURE;
    }

    if (poFeatureDefn->GetFieldIndex("gml_id") != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find gml_id field");
        return OGRERR_FAILURE;
    }

    if (poFeature->IsFieldSet(0) == FALSE)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot update a feature when gml_id field is not set");
        return OGRERR_FAILURE;
    }

    if (bInTransaction)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "SetFeature() not yet dealt in transaction. Issued immediately");
    }

    const char* pszShortName = GetShortName();

    CPLString osPost;
    osPost += GetPostHeader();

    osPost += "  <wfs:Update typeName=\"feature:"; osPost += pszShortName; osPost +=  "\" xmlns:feature=\"";
    osPost += osTargetNamespace; osPost += "\">\n";

    OGRGeometry* poGeom = poFeature->GetGeometryRef();
    if ( osGeometryColumnName.size() != 0 )
    {
        osPost += "    <wfs:Property>\n";
        osPost += "      <wfs:Name>"; osPost += osGeometryColumnName; osPost += "</wfs:Name>\n";
        if (poGeom != NULL)
        {
            if (poGeom->getSpatialReference() == NULL)
                poGeom->assignSpatialReference(poSRS);
            char* pszGML = OGR_G_ExportToGML((OGRGeometryH)poGeom);
            osPost += "      <wfs:Value>";
            osPost += pszGML;
            osPost += "</wfs:Value>\n";
            CPLFree(pszGML);
        }
        osPost += "    </wfs:Property>\n";
    }

    int i;
    for(i=1; i < poFeature->GetFieldCount(); i++)
    {
        OGRFieldDefn* poFDefn = poFeature->GetFieldDefnRef(i);

        osPost += "    <wfs:Property>\n";
        osPost += "      <wfs:Name>"; osPost += poFDefn->GetNameRef(); osPost += "</wfs:Name>\n";
        if (poFeature->IsFieldSet(i))
        {
            osPost += "      <wfs:Value>";
            if (poFDefn->GetType() == OFTInteger)
                osPost += CPLSPrintf("%d", poFeature->GetFieldAsInteger(i));
            else if (poFDefn->GetType() == OFTReal)
                osPost += CPLSPrintf("%.16g", poFeature->GetFieldAsDouble(i));
            else
            {
                char* pszXMLEncoded = CPLEscapeString(poFeature->GetFieldAsString(i),
                                                -1, CPLES_XML);
                osPost += pszXMLEncoded;
                CPLFree(pszXMLEncoded);
            }
            osPost += "</wfs:Value>\n";
        }
        osPost += "    </wfs:Property>\n";
    }
    osPost += "    <ogc:Filter>\n";
    if (poDS->UseFeatureId())
        osPost += "      <ogc:FeatureId fid=\"";
    else
        osPost += "      <ogc:GmlObjectId gml:id=\"";
    osPost += poFeature->GetFieldAsString(0); osPost += "\"/>\n";
    osPost += "    </ogc:Filter>\n";
    osPost += "  </wfs:Update>\n";
    osPost += "</wfs:Transaction>\n";

    CPLDebug("WFS", "Post : %s", osPost.c_str());

    char** papszOptions = NULL;
    papszOptions = CSLAddNameValue(papszOptions, "POSTFIELDS", osPost.c_str());
    papszOptions = CSLAddNameValue(papszOptions, "HEADERS",
                                   "Content-Type: application/xml; charset=UTF-8");

    CPLHTTPResult* psResult = OGRWFSHTTPFetch(poDS->GetPostTransactionURL(), papszOptions);
    CSLDestroy(papszOptions);

    if (psResult == NULL)
    {
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
    int bUse100Schema = FALSE;
    CPLXMLNode* psRoot = CPLGetXMLNode( psXML, "=TransactionResponse" );
    if (psRoot == NULL)
    {
        psRoot = CPLGetXMLNode( psXML, "=WFS_TransactionResponse" );
        if (psRoot)
            bUse100Schema = TRUE;
    }
    if (psRoot == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find <TransactionResponse>");
        CPLDestroyXMLNode( psXML );
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    if (bUse100Schema)
    {
        if (CPLGetXMLNode( psRoot, "TransactionResult.Status.FAILED" ))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Update failed : %s",
                     psResult->pabyData);
            CPLDestroyXMLNode( psXML );
            CPLHTTPDestroyResult(psResult);
            return OGRERR_FAILURE;
        }
    }

    CPLDestroyXMLNode( psXML );
    CPLHTTPDestroyResult(psResult);

    /* Invalidate layer */
    bReloadNeeded = TRUE;
    nFeatures = -1;

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
        CPLString osVal = CPLSPrintf("gml_id = '%s.%ld'", GetShortName(), nFID);
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
/*                         DeleteFromFilter()                           */
/************************************************************************/

OGRErr OGRWFSLayer::DeleteFromFilter( CPLString osOGCFilter )
{
    if (!TestCapability(OLCDeleteFeature))
    {
        if (!poDS->SupportTransactions())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "DeleteFromFilter() not supported: no WMS-T features advertized by server");
        else if (!poDS->UpdateMode())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "DeleteFromFilter() not supported: datasource opened as read-only");
        return OGRERR_FAILURE;
    }

    if (poFeatureDefn->GetFieldIndex("gml_id") != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find gml_id field");
        return OGRERR_FAILURE;
    }
    const char* pszShortName = GetShortName();

    CPLString osPost;
    osPost += GetPostHeader();

    osPost += "  <wfs:Delete xmlns:feature=\""; osPost += osTargetNamespace;
    osPost += "\" typeName=\"feature:"; osPost += pszShortName; osPost += "\">\n";
    osPost += "    <ogc:Filter>\n";
    osPost += osOGCFilter;
    osPost += "    </ogc:Filter>\n";
    osPost += "  </wfs:Delete>\n";
    osPost += "</wfs:Transaction>\n";

    CPLDebug("WFS", "Post : %s", osPost.c_str());

    char** papszOptions = NULL;
    papszOptions = CSLAddNameValue(papszOptions, "POSTFIELDS", osPost.c_str());
    papszOptions = CSLAddNameValue(papszOptions, "HEADERS",
                                   "Content-Type: application/xml; charset=UTF-8");

    CPLHTTPResult* psResult = OGRWFSHTTPFetch(poDS->GetPostTransactionURL(), papszOptions);
    CSLDestroy(papszOptions);

    if (psResult == NULL)
    {
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
    int bUse100Schema = FALSE;
    CPLXMLNode* psRoot = CPLGetXMLNode( psXML, "=TransactionResponse" );
    if (psRoot == NULL)
    {
        psRoot = CPLGetXMLNode( psXML, "=WFS_TransactionResponse" );
        if (psRoot)
            bUse100Schema = TRUE;
    }
    if (psRoot == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find <TransactionResponse>");
        CPLDestroyXMLNode( psXML );
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    if (bUse100Schema)
    {
        if (CPLGetXMLNode( psRoot, "TransactionResult.Status.FAILED" ))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Delete failed : %s",
                     psResult->pabyData);
            CPLDestroyXMLNode( psXML );
            CPLHTTPDestroyResult(psResult);
            return OGRERR_FAILURE;
        }
    }

    CPLDestroyXMLNode( psXML );
    CPLHTTPDestroyResult(psResult);

    /* Invalidate layer */
    bReloadNeeded = TRUE;
    nFeatures = -1;

    return OGRERR_NONE;
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

    if (bInTransaction)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "DeleteFeature() not yet dealt in transaction. Issued immediately");
    }

    CPLString osGMLID = pszGMLID;
    pszGMLID = NULL;
    delete poFeature;
    poFeature = NULL;

    CPLString osFilter;
    osFilter = "<ogc:FeatureId fid=\""; osFilter += osGMLID; osFilter += "\"/>\n";
    return DeleteFromFilter(osFilter);
}


/************************************************************************/
/*                         StartTransaction()                           */
/************************************************************************/

OGRErr OGRWFSLayer::StartTransaction()
{
    if (!TestCapability(OLCTransactions))
    {
        if (!poDS->SupportTransactions())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "StartTransaction() not supported: no WMS-T features advertized by server");
        else if (!poDS->UpdateMode())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "StartTransaction() not supported: datasource opened as read-only");
        return OGRERR_FAILURE;
    }

    if (bInTransaction)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                     "StartTransaction() has already been called");
        return OGRERR_FAILURE;
    }

    bInTransaction = TRUE;
    osGlobalInsert = "";
    nExpectedInserts = 0;
    aosFIDList.resize(0);

    return OGRERR_NONE;
}

/************************************************************************/
/*                        CommitTransaction()                           */
/************************************************************************/

OGRErr OGRWFSLayer::CommitTransaction()
{
    if (!TestCapability(OLCTransactions))
    {
        if (!poDS->SupportTransactions())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "CommitTransaction() not supported: no WMS-T features advertized by server");
        else if (!poDS->UpdateMode())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "CommitTransaction() not supported: datasource opened as read-only");
        return OGRERR_FAILURE;
    }

    if (!bInTransaction)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                     "StartTransaction() has not yet been called");
        return OGRERR_FAILURE;
    }

    if (osGlobalInsert.size() != 0)
    {
        CPLString osPost = GetPostHeader();
        osPost += "  <wfs:Insert>\n";
        osPost += osGlobalInsert;
        osPost += "  </wfs:Insert>\n";
        osPost += "</wfs:Transaction>\n";

        bInTransaction = FALSE;
        osGlobalInsert = "";
        int nExpectedInserts = this->nExpectedInserts;
        this->nExpectedInserts = 0;

        CPLDebug("WFS", "Post : %s", osPost.c_str());

        char** papszOptions = NULL;
        papszOptions = CSLAddNameValue(papszOptions, "POSTFIELDS", osPost.c_str());
        papszOptions = CSLAddNameValue(papszOptions, "HEADERS",
                                    "Content-Type: application/xml; charset=UTF-8");

        CPLHTTPResult* psResult = OGRWFSHTTPFetch(poDS->GetPostTransactionURL(), papszOptions);
        CSLDestroy(papszOptions);

        if (psResult == NULL)
        {
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
        int bUse100Schema = FALSE;
        CPLXMLNode* psRoot = CPLGetXMLNode( psXML, "=TransactionResponse" );
        if (psRoot == NULL)
        {
            psRoot = CPLGetXMLNode( psXML, "=WFS_TransactionResponse" );
            if (psRoot)
                bUse100Schema = TRUE;
        }

        if (psRoot == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot find <TransactionResponse>");
            CPLDestroyXMLNode( psXML );
            CPLHTTPDestroyResult(psResult);
            return OGRERR_FAILURE;
        }

        if (bUse100Schema)
        {
            if (CPLGetXMLNode( psRoot, "TransactionResult.Status.FAILED" ))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Insert failed : %s",
                        psResult->pabyData);
                CPLDestroyXMLNode( psXML );
                CPLHTTPDestroyResult(psResult);
                return OGRERR_FAILURE;
            }

            /* TODO */
        }
        else
        {
            int nGotInserted = atoi(CPLGetXMLValue(psRoot, "TransactionSummary.totalInserted", ""));
            if (nGotInserted != nExpectedInserts)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Only %d features were inserted whereas %d where expected",
                         nGotInserted, nExpectedInserts);
                CPLDestroyXMLNode( psXML );
                CPLHTTPDestroyResult(psResult);
                return OGRERR_FAILURE;
            }

            CPLXMLNode* psInsertResults =
                CPLGetXMLNode( psRoot, "InsertResults");
            if (psInsertResults == NULL)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Cannot find node InsertResults");
                CPLDestroyXMLNode( psXML );
                CPLHTTPDestroyResult(psResult);
                return OGRERR_FAILURE;
            }

            aosFIDList.resize(0);

            CPLXMLNode* psChild = psInsertResults->psChild;
            while(psChild)
            {
                const char* pszFID = CPLGetXMLValue(psChild, "FeatureId.fid", NULL);
                if (pszFID == NULL)
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Cannot find fid");
                    CPLDestroyXMLNode( psXML );
                    CPLHTTPDestroyResult(psResult);
                    return OGRERR_FAILURE;
                }
                aosFIDList.push_back(pszFID);

                psChild = psChild->psNext;
            }

            if ((int)aosFIDList.size() != nGotInserted)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Inconsistant InsertResults: did not get expected FID count");
                CPLDestroyXMLNode( psXML );
                CPLHTTPDestroyResult(psResult);
                return OGRERR_FAILURE;
            }
        }

        CPLDestroyXMLNode( psXML );
        CPLHTTPDestroyResult(psResult);
    }

    bInTransaction = FALSE;
    osGlobalInsert = "";
    nExpectedInserts = 0;

    return OGRERR_NONE;
}

/************************************************************************/
/*                      RollbackTransaction()                           */
/************************************************************************/

OGRErr OGRWFSLayer::RollbackTransaction()
{
    if (!TestCapability(OLCTransactions))
    {
        if (!poDS->SupportTransactions())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "RollbackTransaction() not supported: no WMS-T features advertized by server");
        else if (!poDS->UpdateMode())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "RollbackTransaction() not supported: datasource opened as read-only");
        return OGRERR_FAILURE;
    }

    if (!bInTransaction)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                     "StartTransaction() has not yet been called");
        return OGRERR_FAILURE;
    }

    bInTransaction = FALSE;
    osGlobalInsert = "";
    nExpectedInserts = 0;

    return OGRERR_NONE;
}
