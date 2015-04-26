/******************************************************************************
 * $Id$
 *
 * Project:  CSW Translator
 * Purpose:  Implements OGRCSWDriver.
 * Author:   Even Rouault, Even Rouault <even dot rouault at spatialys dot com>
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

#include "ogrsf_frmts.h"
#include "cpl_conv.h"
#include "cpl_http.h"
#include "ogr_wfs.h"

CPL_CVSID("$Id$");

extern "C" void RegisterOGRCSW();

/************************************************************************/
/*                             OGRCSWLayer                              */
/************************************************************************/

class OGRCSWDataSource;

class OGRCSWLayer : public OGRLayer
{
    OGRCSWDataSource*   poDS;
    OGRFeatureDefn*     poFeatureDefn;

    GDALDataset        *poBaseDS;
    OGRLayer           *poBaseLayer;
    int                 bReloadNeeded;
    int                 bHasFetched;

    int                 bPagingActive;
    int                 nPagingStartIndex;
    int                 nFeatureRead;
    int                 nFeaturesInCurrentPage;

    CPLString           osQuery, osCSWWhere;

    GDALDataset*        FetchGetRecords();
    GIntBig             GetFeatureCountWithHits();
    void                BuildQuery();

  public:
                        OGRCSWLayer(OGRCSWDataSource* poDS);
                        ~OGRCSWLayer();

    virtual void                ResetReading();
    virtual OGRFeature*         GetNextFeature();
    virtual GIntBig             GetFeatureCount(int bForce = FALSE);

    virtual OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    virtual int                 TestCapability( const char * ) { return FALSE; }

    virtual void                SetSpatialFilter( OGRGeometry * );
    virtual OGRErr              SetAttributeFilter( const char * );
};

/************************************************************************/
/*                           OGRCSWDataSource                           */
/************************************************************************/

class OGRCSWDataSource : public OGRDataSource
{
    char*               pszName;
    CPLString           osBaseURL;
    CPLString           osVersion;
    CPLString           osElementSetName;

    OGRCSWLayer*        poLayer;
    int                 bFullExtentRecordsAsNonSpatial;

    CPLHTTPResult*      SendGetCapabilities();

  public:
                        OGRCSWDataSource();
                        ~OGRCSWDataSource();

    int                 Open( const char * pszFilename,
                              char** papszOpenOptions );

    virtual const char*         GetName() { return pszName; }

    virtual int                 GetLayerCount() { return poLayer != NULL; }
    virtual OGRLayer*           GetLayer( int );

    virtual int                 TestCapability( const char * ) { return FALSE; }

    CPLHTTPResult*              HTTPFetch( const char* pszURL, const char* pszPost );

    const CPLString&            GetBaseURL() { return osBaseURL; }
    const CPLString&            GetVersion() { return osVersion; }
    const CPLString&            GetElementSetName() { return osElementSetName; }
    int                         FullExtentRecordsAsNonSpatial() { return bFullExtentRecordsAsNonSpatial; }
};

/************************************************************************/
/*                           OGRCSWLayer()                              */
/************************************************************************/

OGRCSWLayer::OGRCSWLayer(OGRCSWDataSource* poDS)
{
    this->poDS = poDS;
    poFeatureDefn = new OGRFeatureDefn("records");
    SetDescription(poFeatureDefn->GetName());
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbPolygon);
    OGRSpatialReference* poSRS = new OGRSpatialReference(SRS_WKT_WGS84);
    poFeatureDefn->GetGeomFieldDefn(0)->SetName("boundingbox");
    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
    {
        OGRFieldDefn oField("identifier", OFTString);
        poFeatureDefn->AddFieldDefn(&oField);
    }
    {
        OGRFieldDefn oField("other_identifiers", OFTStringList);
        poFeatureDefn->AddFieldDefn(&oField);
    }
    {
        OGRFieldDefn oField("type", OFTString);
        poFeatureDefn->AddFieldDefn(&oField);
    }
    {
        OGRFieldDefn oField("subject", OFTString);
        poFeatureDefn->AddFieldDefn(&oField);
    }
    {
        OGRFieldDefn oField("other_subjects", OFTStringList);
        poFeatureDefn->AddFieldDefn(&oField);
    }
    {
        OGRFieldDefn oField("references", OFTString);
        poFeatureDefn->AddFieldDefn(&oField);
    }
    {
        OGRFieldDefn oField("other_references", OFTStringList);
        poFeatureDefn->AddFieldDefn(&oField);
    }
    {
        OGRFieldDefn oField("modified", OFTString);
        poFeatureDefn->AddFieldDefn(&oField);
    }
    {
        OGRFieldDefn oField("abstract", OFTString);
        poFeatureDefn->AddFieldDefn(&oField);
    }
    {
        OGRFieldDefn oField("date", OFTString);
        poFeatureDefn->AddFieldDefn(&oField);
    }
    {
        OGRFieldDefn oField("language", OFTString);
        poFeatureDefn->AddFieldDefn(&oField);
    }
    {
        OGRFieldDefn oField("rights", OFTString);
        poFeatureDefn->AddFieldDefn(&oField);
    }
    {
        OGRFieldDefn oField("format", OFTString);
        poFeatureDefn->AddFieldDefn(&oField);
    }
    {
        OGRFieldDefn oField("other_formats", OFTStringList);
        poFeatureDefn->AddFieldDefn(&oField);
    }
    {
        OGRFieldDefn oField("creator", OFTString);
        poFeatureDefn->AddFieldDefn(&oField);
    }
    {
        OGRFieldDefn oField("source", OFTString);
        poFeatureDefn->AddFieldDefn(&oField);
    }
    {
        OGRFieldDefn oField("anytext", OFTString);
        poFeatureDefn->AddFieldDefn(&oField);
    }

    poBaseDS = NULL;
    poBaseLayer = NULL;
    bReloadNeeded = FALSE;
    bHasFetched = FALSE;

    bPagingActive = FALSE;
    nPagingStartIndex = 0;
    nFeatureRead = 0;
    nFeaturesInCurrentPage = 0;

    poSRS->Release();
}

/************************************************************************/
/*                          ~OGRCSWLayer()                              */
/************************************************************************/

OGRCSWLayer::~OGRCSWLayer()
{
    poFeatureDefn->Release();
    GDALClose(poBaseDS);
    CPLString osTmpDirName = CPLSPrintf("/vsimem/tempcsw_%p", this);
    OGRWFSRecursiveUnlink(osTmpDirName);
}

/************************************************************************/
/*                          ResetReading()                              */
/************************************************************************/

void OGRCSWLayer::ResetReading()
{
    if (bPagingActive)
        bReloadNeeded = TRUE;
    nPagingStartIndex = 0;
    nFeatureRead = 0;
    nFeaturesInCurrentPage = 0;
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
/*                          GetNextFeature()                            */
/************************************************************************/

OGRFeature* OGRCSWLayer::GetNextFeature()
{
    while(TRUE)
    {
        if (bPagingActive && nFeatureRead == nPagingStartIndex + nFeaturesInCurrentPage)
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
            poBaseDS = FetchGetRecords();
            if (poBaseDS)
            {
                poBaseLayer = poBaseDS->GetLayer(0);
                poBaseLayer->ResetReading();
                nFeaturesInCurrentPage = (int)poBaseLayer->GetFeatureCount();
            }
        }
        if (!poBaseLayer)
            return NULL;

        OGRFeature* poSrcFeature = poBaseLayer->GetNextFeature();
        if (poSrcFeature == NULL)
            return NULL;
        nFeatureRead ++;

        OGRFeature* poNewFeature = new OGRFeature(poFeatureDefn);

        for(int i=0;i<poFeatureDefn->GetFieldCount();i++)
        {
            const char* pszFieldname = poFeatureDefn->GetFieldDefn(i)->GetNameRef();
            int iSrcField = poSrcFeature->GetFieldIndex(pszFieldname);
            /* http://www.paikkatietohakemisto.fi/geonetwork/srv/en/csw returns URI ... */
            if( iSrcField < 0 && strcmp(pszFieldname, "references") == 0 )
                iSrcField = poSrcFeature->GetFieldIndex("URI");
            if( iSrcField >= 0 && poSrcFeature->IsFieldSet(iSrcField) )
            {
                OGRFieldType eType = poFeatureDefn->GetFieldDefn(i)->GetType();
                OGRFieldType eSrcType = poSrcFeature->GetFieldDefnRef(iSrcField)->GetType();
                if( eType == eSrcType )
                {
                    poNewFeature->SetField(i, poSrcFeature->GetRawFieldRef(iSrcField));
                }
                else
                {
                    if( eType == OFTString && eSrcType == OFTStringList &&
                        strcmp(pszFieldname, "identifier") == 0 )
                    {
                        char** papszValues = poSrcFeature->GetFieldAsStringList(iSrcField);
                        poNewFeature->SetField("identifier", *papszValues);
                        if( papszValues[1] )
                            poNewFeature->SetField("other_identifiers", papszValues + 1);
                    }
                    else if( eType == OFTString && eSrcType == OFTStringList &&
                             strcmp(pszFieldname, "subject") == 0 )
                    {
                        char** papszValues = poSrcFeature->GetFieldAsStringList(iSrcField);
                        poNewFeature->SetField("subject", *papszValues);
                        if( papszValues[1] )
                            poNewFeature->SetField("other_subjects", papszValues + 1);
                    }
                    else if( eType == OFTString && eSrcType == OFTStringList &&
                             strcmp(pszFieldname, "references") == 0 )
                    {
                        char** papszValues = poSrcFeature->GetFieldAsStringList(iSrcField);
                        poNewFeature->SetField("references", *papszValues);
                        if( papszValues[1] )
                            poNewFeature->SetField("other_references", papszValues + 1);
                    }
                    else if( eType == OFTString && eSrcType == OFTStringList &&
                             strcmp(pszFieldname, "format") == 0 )
                    {
                        char** papszValues = poSrcFeature->GetFieldAsStringList(iSrcField);
                        poNewFeature->SetField("format", *papszValues);
                        if( papszValues[1] )
                            poNewFeature->SetField("other_formats", papszValues + 1);
                    }
                    else
                        poNewFeature->SetField(i, poSrcFeature->GetFieldAsString(iSrcField));
                }
            }
        }

        OGRGeometry* poGeom = poSrcFeature->StealGeometry();
        if( poGeom )
        {
            if( poDS->FullExtentRecordsAsNonSpatial() )
            {
                OGREnvelope sEnvelope;
                poGeom->getEnvelope(&sEnvelope);
                if( sEnvelope.MinX == -180 && sEnvelope.MinY == -90 &&
                    sEnvelope.MaxX == 180 && sEnvelope.MaxY == 90 )
                {
                    delete poGeom;
                    poGeom = NULL;
                }
            }
            if( poGeom )
            {
                poGeom->assignSpatialReference(poFeatureDefn->GetGeomFieldDefn(0)->GetSpatialRef());
                poNewFeature->SetGeometryDirectly(poGeom);
            }
        }

        poNewFeature->SetFID(nFeatureRead);
        delete poSrcFeature;

        if( osCSWWhere.size() == 0 &&
            m_poAttrQuery != NULL &&
            !m_poAttrQuery->Evaluate( poNewFeature ) )
        {
            delete poNewFeature;
        }
        else
        {
            return poNewFeature;
        }
    }
}

/************************************************************************/
/*                         GetFeatureCount()                            */
/************************************************************************/

GIntBig OGRCSWLayer::GetFeatureCount(int bForce)
{
    GIntBig nFeatures = GetFeatureCountWithHits();
    if( nFeatures >= 0 )
        return nFeatures;
    return OGRLayer::GetFeatureCount(bForce);
}

/************************************************************************/
/*                        GetFeatureCountWithHits()                     */
/************************************************************************/

GIntBig OGRCSWLayer::GetFeatureCountWithHits()
{
    CPLString osPost = CPLSPrintf(
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
"<csw:GetRecords resultType=\"hits\" service=\"CSW\" version=\"%s\""
" xmlns:csw=\"http://www.opengis.net/cat/csw/2.0.2\""
" xmlns:gml=\"http://www.opengis.net/gml\""
" xmlns:dc=\"http://purl.org/dc/elements/1.1/\""
" xmlns:dct=\"http://purl.org/dc/terms/\""
" xmlns:ogc=\"http://www.opengis.net/ogc\""
" xmlns:ows=\"http://www.opengis.net/ows\""
" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
" xsi:schemaLocation=\"http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd\">"
"<csw:Query typeNames=\"csw:Record\">"
"<csw:ElementSetName>%s</csw:ElementSetName>"
"%s"
"</csw:Query>"
"</csw:GetRecords>",
            poDS->GetVersion().c_str(),
            poDS->GetElementSetName().c_str(),
            osQuery.c_str());

    CPLHTTPResult* psResult = poDS->HTTPFetch( poDS->GetBaseURL(), osPost);
    if (psResult == NULL)
    {
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
    CPLHTTPDestroyResult(psResult);
    psResult = NULL;

    GIntBig nFeatures = CPLAtoGIntBig(CPLGetXMLValue(psXML,
        "=GetRecordsResponse.SearchResults.numberOfRecordsMatched", "-1"));

    CPLDestroyXMLNode(psXML);
    return nFeatures;
}

/************************************************************************/
/*                         FetchGetRecords()                            */
/************************************************************************/

GDALDataset* OGRCSWLayer::FetchGetRecords()
{
    CPLHTTPResult* psResult = NULL;

    bPagingActive = TRUE;

    CPLString osPost = CPLSPrintf(
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
"<csw:GetRecords resultType=\"results\" service=\"CSW\" version=\"%s\""
" startPosition=\"%d\""
" xmlns:csw=\"http://www.opengis.net/cat/csw/2.0.2\""
" xmlns:gml=\"http://www.opengis.net/gml\""
" xmlns:dc=\"http://purl.org/dc/elements/1.1/\""
" xmlns:dct=\"http://purl.org/dc/terms/\""
" xmlns:ogc=\"http://www.opengis.net/ogc\""
" xmlns:ows=\"http://www.opengis.net/ows\""
" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
" xsi:schemaLocation=\"http://www.opengis.net/cat/csw/2.0.2 http://schemas.opengis.net/csw/2.0.2/CSW-discovery.xsd\">"
"<csw:Query typeNames=\"csw:Record\">"
"<csw:ElementSetName>%s</csw:ElementSetName>"
"%s"
"</csw:Query>"
"</csw:GetRecords>",
            poDS->GetVersion().c_str(),
            nPagingStartIndex + 1,
            poDS->GetElementSetName().c_str(),
            osQuery.c_str());

    psResult = poDS->HTTPFetch( poDS->GetBaseURL(), osPost);
    if (psResult == NULL)
    {
        return NULL;
    }

    CPLString osTmpDirName = CPLSPrintf("/vsimem/tempcsw_%p", this);
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
    //CPLDebug("CSW", "%s", (const char*)pabyData);

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
        if( strstr((const char*)pabyData, "<csw:GetRecordsResponse") == NULL &&
            strstr((const char*)pabyData, "<GetRecordsResponse") == NULL )
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
/*                         SetSpatialFilter()                           */
/************************************************************************/

void OGRCSWLayer::SetSpatialFilter( OGRGeometry * poGeom )
{
    OGRLayer::SetSpatialFilter(poGeom);
    bReloadNeeded = TRUE;
    ResetReading();
    BuildQuery();
}

/************************************************************************/
/*                         OGRCSWAddRightPrefixes()                     */
/************************************************************************/

static void OGRCSWAddRightPrefixes(swq_expr_node* poNode)
{
    if( poNode->eNodeType == SNT_COLUMN )
    {
        if( EQUAL(poNode->string_value, "identifier") ||
            EQUAL(poNode->string_value, "title") ||
            EQUAL(poNode->string_value, "type") ||
            EQUAL(poNode->string_value, "subject") ||
            EQUAL(poNode->string_value, "date") ||
            EQUAL(poNode->string_value, "language") ||
            EQUAL(poNode->string_value, "rights") ||
            EQUAL(poNode->string_value, "format") ||
            EQUAL(poNode->string_value, "creator") ||
            EQUAL(poNode->string_value, "source") )
        {
            char* pszNewVal = CPLStrdup(CPLSPrintf("dc:%s", poNode->string_value));
            CPLFree(poNode->string_value);
            poNode->string_value = pszNewVal;
        }
        else if( EQUAL(poNode->string_value, "references") ||
                 EQUAL(poNode->string_value, "modified") ||
                 EQUAL(poNode->string_value, "abstract") )
        {
            char* pszNewVal = CPLStrdup(CPLSPrintf("dct:%s", poNode->string_value));
            CPLFree(poNode->string_value);
            poNode->string_value = pszNewVal;
        }
        else if( EQUAL(poNode->string_value, "other_identifiers") )
        {
            CPLFree(poNode->string_value);
            poNode->string_value = CPLStrdup("dc:identifier");
        }
        else if( EQUAL(poNode->string_value, "other_subjects") )
        {
            CPLFree(poNode->string_value);
            poNode->string_value = CPLStrdup("dc:subject");
        }
        else if( EQUAL(poNode->string_value, "other_references") )
        {
            CPLFree(poNode->string_value);
            poNode->string_value = CPLStrdup("dct:references");
        }
        else if( EQUAL(poNode->string_value, "other_formats") )
        {
            CPLFree(poNode->string_value);
            poNode->string_value = CPLStrdup("dc:format");
        }
        else if( EQUAL(poNode->string_value, "AnyText") )
        {
            CPLFree(poNode->string_value);
            poNode->string_value = CPLStrdup("csw:AnyText");
        }
        else if( EQUAL(poNode->string_value, "boundingbox") )
        {
            CPLFree(poNode->string_value);
            poNode->string_value = CPLStrdup("ows:BoundingBox");
        }
    }
    else if( poNode->eNodeType == SNT_OPERATION )
    {
        for(int i=0;i < poNode->nSubExprCount;i++)
            OGRCSWAddRightPrefixes(poNode->papoSubExpr[i]);
    }
}

/************************************************************************/
/*                        SetAttributeFilter()                          */
/************************************************************************/

OGRErr OGRCSWLayer::SetAttributeFilter( const char * pszFilter )
{
    if (pszFilter != NULL && pszFilter[0] == 0)
        pszFilter = NULL;

    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = (pszFilter) ? CPLStrdup(pszFilter) : NULL;

    delete m_poAttrQuery;
    m_poAttrQuery = NULL;
    
    if( pszFilter != NULL )
    {
        m_poAttrQuery = new OGRFeatureQuery();

        OGRErr eErr = m_poAttrQuery->Compile( GetLayerDefn(), pszFilter, TRUE,
                                              WFSGetCustomFuncRegistrar() );
        if( eErr != OGRERR_NONE )
        {
            delete m_poAttrQuery;
            m_poAttrQuery = NULL;
            return eErr;
        }
    }

    if (m_poAttrQuery != NULL )
    {
        swq_expr_node* poNode = (swq_expr_node*) m_poAttrQuery->GetSWQExpr();
        swq_expr_node* poNodeClone = poNode->Clone();
        poNodeClone->ReplaceBetweenByGEAndLERecurse();
        OGRCSWAddRightPrefixes(poNodeClone);

        int bNeedsNullCheck = FALSE;
        if( poNode->field_type != SWQ_BOOLEAN )
            osCSWWhere = "";
        else
            osCSWWhere = WFS_TurnSQLFilterToOGCFilter(poNodeClone,
                                                      NULL,
                                                      NULL,
                                                      110,
                                                      FALSE,
                                                      FALSE,
                                                      FALSE,
                                                      "ogc:",
                                                      &bNeedsNullCheck);
        delete poNodeClone;
    }
    else
        osCSWWhere = "";

    if (m_poAttrQuery != NULL && osCSWWhere.size() == 0)
    {
        CPLDebug("CSW", "Using client-side only mode for filter \"%s\"", pszFilter);
        OGRErr eErr = OGRLayer::SetAttributeFilter(pszFilter);
        if (eErr != OGRERR_NONE)
            return eErr;
    }

    bReloadNeeded = TRUE;
    ResetReading();
    BuildQuery();

    return OGRERR_NONE;
}

/************************************************************************/
/*                                BuildQuery()                          */
/************************************************************************/

void OGRCSWLayer::BuildQuery()
{
    if( m_poFilterGeom != NULL || osCSWWhere.size() != 0 )
    {
        osQuery = "<csw:Constraint version=\"1.1.0\">";
        osQuery += "<ogc:Filter>";
        if( m_poFilterGeom != NULL && osCSWWhere.size() != 0 )
            osQuery += "<ogc:And>";
        if( m_poFilterGeom != NULL )
        {
            osQuery += "<ogc:BBOX>";
            osQuery += "<ogc:PropertyName>ows:BoundingBox</ogc:PropertyName>";
            osQuery += "<gml:Envelope srsName=\"urn:ogc:def:crs:EPSG::4326\">";
            OGREnvelope sEnvelope;
            m_poFilterGeom->getEnvelope(&sEnvelope);
            if( CSLTestBoolean(
                    CPLGetConfigOption("GML_INVERT_AXIS_ORDER_IF_LAT_LONG", "YES")) )
            {
                osQuery += CPLSPrintf("<gml:lowerCorner>%.16g %.16g</gml:lowerCorner>", sEnvelope.MinY, sEnvelope.MinX);
                osQuery += CPLSPrintf("<gml:upperCorner>%.16g %.16g</gml:upperCorner>", sEnvelope.MaxY, sEnvelope.MaxX);
            }
            else
            {
                osQuery += CPLSPrintf("<gml:lowerCorner>%.16g %.16g</gml:lowerCorner>", sEnvelope.MinX, sEnvelope.MinY);
                osQuery += CPLSPrintf("<gml:upperCorner>%.16g %.16g</gml:upperCorner>", sEnvelope.MaxX, sEnvelope.MaxY);
            }
            osQuery += "</gml:Envelope>";
            osQuery += "</ogc:BBOX>";
        }
        osQuery += osCSWWhere;
        if( m_poFilterGeom != NULL && osCSWWhere.size() != 0 )
            osQuery += "</ogc:And>";
        osQuery += "</ogc:Filter>";
        osQuery += "</csw:Constraint>";
    }
    else
        osQuery = "";
}

/************************************************************************/
/*                          OGRCSWDataSource()                          */
/************************************************************************/

OGRCSWDataSource::OGRCSWDataSource()
{
    pszName = NULL;
    poLayer = NULL;
    bFullExtentRecordsAsNonSpatial = FALSE;
}

/************************************************************************/
/*                         ~OGRCSWDataSource()                          */
/************************************************************************/

OGRCSWDataSource::~OGRCSWDataSource()
{
    delete poLayer;
    CPLFree( pszName );
}

/************************************************************************/
/*                          SendGetCapabilities()                       */
/************************************************************************/

CPLHTTPResult* OGRCSWDataSource::SendGetCapabilities()
{
    CPLString osURL(osBaseURL);

    osURL = CPLURLAddKVP(osURL, "SERVICE", "CSW");
    osURL = CPLURLAddKVP(osURL, "REQUEST", "GetCapabilities");

    CPLHTTPResult* psResult;

    CPLDebug("CSW", "%s", osURL.c_str());

    psResult = HTTPFetch( osURL, NULL);
    if (psResult == NULL)
    {
        return NULL;
    }

    if (strstr((const char*)psResult->pabyData,
                                    "<ServiceExceptionReport") != NULL ||
        strstr((const char*)psResult->pabyData,
                                    "<ows:ExceptionReport") != NULL ||
        strstr((const char*)psResult->pabyData,
                                    "<ExceptionReport") != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    return psResult;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRCSWDataSource::Open( const char * pszFilename,
                            char** papszOpenOptions )
{
    const char* pszBaseURL = CSLFetchNameValue(papszOpenOptions, "URL");
    if( pszBaseURL == NULL )
    {
        pszBaseURL = pszFilename;
        if (EQUALN(pszFilename, "CSW:", 4))
            pszBaseURL += 4;
        if( pszBaseURL[0] == '\0' )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Missing URL open option");
            return FALSE;
        }
    }
    osBaseURL = pszBaseURL;
    osElementSetName = CSLFetchNameValueDef(papszOpenOptions, "ELEMENTSETNAME",
                                            "full");
    bFullExtentRecordsAsNonSpatial = CSLFetchBoolean(papszOpenOptions,
                                                     "FULL_EXTENT_RECORDS_AS_NON_SPATIAL",
                                                     FALSE);

    if (strncmp(osBaseURL, "http://", 7) != 0 &&
        strncmp(osBaseURL, "https://", 8) != 0 &&
        strncmp(osBaseURL, "/vsimem/", strlen("/vsimem/")) != 0)
        return FALSE;

    CPLHTTPResult* psResult = SendGetCapabilities();
    if( psResult == NULL )
        return FALSE;
    
    CPLXMLNode* psXML = CPLParseXMLString( (const char*) psResult->pabyData );
    if (psXML == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid XML content : %s",
                psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return FALSE;
    }
    CPLStripXMLNamespace( psXML, NULL, TRUE );
    CPLHTTPDestroyResult(psResult);
    psResult = NULL;

    const char* pszVersion = CPLGetXMLValue( psXML, "=Capabilities.version", NULL);
    if( pszVersion == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find Capabilities.version");
        CPLDestroyXMLNode(psXML);
        return FALSE;
    }
    if( !EQUAL(pszVersion, "2.0.2") )
        CPLDebug("CSW", "Presumably only work properly with 2.0.2. Reported version is %s", pszVersion);
    osVersion = pszVersion;
    CPLDestroyXMLNode(psXML);

    poLayer = new OGRCSWLayer(this);

    return TRUE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRCSWDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= ((poLayer != NULL) ? 1 : 0) )
        return NULL;
    else
        return poLayer;
}

/************************************************************************/
/*                            HTTPFetch()                               */
/************************************************************************/

CPLHTTPResult* OGRCSWDataSource::HTTPFetch( const char* pszURL, const char* pszPost )
{
    char** papszOptions = NULL;
    if( pszPost )
    {
        papszOptions = CSLAddNameValue(papszOptions, "POSTFIELDS", pszPost);
        papszOptions = CSLAddNameValue(papszOptions, "HEADERS",
                                    "Content-Type: application/xml; charset=UTF-8");
    }
    CPLHTTPResult* psResult = CPLHTTPFetch( pszURL, papszOptions );
    CSLDestroy(papszOptions);

    if (psResult == NULL)
    {
        return NULL;
    }
    if (psResult->nStatus != 0 || psResult->pszErrBuf != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s (%d)",
                 (psResult->pszErrBuf) ? psResult->pszErrBuf : "unknown", psResult->nStatus);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    if (psResult->pabyData == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Empty content returned by server");
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    return psResult;
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

static int OGRCSWDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    return EQUALN(poOpenInfo->pszFilename, "CSW:", 4);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRCSWDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( !OGRCSWDriverIdentify(poOpenInfo) || poOpenInfo->eAccess == GA_Update )
        return NULL;

    OGRCSWDataSource   *poDS = new OGRCSWDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename,
                     poOpenInfo->papszOpenOptions ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRCSW()                           */
/************************************************************************/

void RegisterOGRCSW()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "CSW" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "CSW" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "OGC CSW (Catalog  Service for the Web)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_csw.html" );

        poDriver->SetMetadataItem( GDAL_DMD_CONNECTION_PREFIX, "CSW:" );

        poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='URL' type='string' description='URL to the CSW server endpoint' required='true'/>"
"  <Option name='ELEMENTSETNAME' type='string-select' description='Level of details of properties' default='full'>"
"    <Value>brief</Value>"
"    <Value>summary</Value>"
"    <Value>full</Value>"
"  </Option>"
"  <Option name='FULL_EXTENT_RECORDS_AS_NON_SPATIAL' type='boolean' description='Whether records with (-180,-90,180,90) extent should be considered non-spatial' default='false'/>"
"</OpenOptionList>" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnIdentify = OGRCSWDriverIdentify;
        poDriver->pfnOpen = OGRCSWDriverOpen;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

