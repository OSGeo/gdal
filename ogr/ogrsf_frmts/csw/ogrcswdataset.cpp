/******************************************************************************
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
#include "ogr_p.h"
#include "gmlutils.h"

CPL_CVSID("$Id$")

extern "C" void RegisterOGRCSW();

/************************************************************************/
/*                             OGRCSWLayer                              */
/************************************************************************/

class OGRCSWDataSource;

class OGRCSWLayer final: public OGRLayer
{
    OGRCSWDataSource*   poDS;
    OGRFeatureDefn*     poFeatureDefn;

    GDALDataset        *poBaseDS;
    OGRLayer           *poBaseLayer;

    int                 nPagingStartIndex;
    int                 nFeatureRead;
    int                 nFeaturesInCurrentPage;

    CPLString           osQuery;
    CPLString           osCSWWhere;

    GDALDataset*        FetchGetRecords();
    GIntBig             GetFeatureCountWithHits();
    void                BuildQuery();

  public:
               explicit OGRCSWLayer( OGRCSWDataSource* poDS );
               virtual ~OGRCSWLayer();

    virtual void                ResetReading() override;
    virtual OGRFeature*         GetNextFeature() override;
    virtual GIntBig             GetFeatureCount( int bForce = FALSE ) override;

    virtual OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    virtual int                 TestCapability( const char * ) override { return FALSE; }

    virtual void                SetSpatialFilter( OGRGeometry * ) override;
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom ) override
                { OGRLayer::SetSpatialFilter(iGeomField, poGeom); }
    virtual OGRErr              SetAttributeFilter( const char * ) override;
};

/************************************************************************/
/*                           OGRCSWDataSource                           */
/************************************************************************/

class OGRCSWDataSource final: public OGRDataSource
{
    char*               pszName;
    CPLString           osBaseURL;
    CPLString           osVersion;
    CPLString           osElementSetName;
    CPLString           osOutputSchema;
    int                 nMaxRecords;

    OGRCSWLayer*        poLayer;
    bool                bFullExtentRecordsAsNonSpatial;

    CPLHTTPResult*      SendGetCapabilities();

  public:
                        OGRCSWDataSource();
               virtual ~OGRCSWDataSource();

    int                 Open( const char * pszFilename,
                              char** papszOpenOptions );

    virtual const char*         GetName() override { return pszName; }

    virtual int                 GetLayerCount() override { return poLayer != nullptr; }
    virtual OGRLayer*           GetLayer( int ) override;

    virtual int                 TestCapability( const char * ) override { return FALSE; }

    CPLHTTPResult*              HTTPFetch( const char* pszURL, const char* pszPost );

    const CPLString&            GetBaseURL() { return osBaseURL; }
    const CPLString&            GetVersion() { return osVersion; }
    const CPLString&            GetElementSetName() { return osElementSetName; }
    const CPLString&            GetOutputSchema() { return osOutputSchema; }
    bool                        FullExtentRecordsAsNonSpatial() { return bFullExtentRecordsAsNonSpatial; }
    int                         GetMaxRecords() { return nMaxRecords; }
};

/************************************************************************/
/*                           OGRCSWLayer()                              */
/************************************************************************/

OGRCSWLayer::OGRCSWLayer( OGRCSWDataSource* poDSIn ) :
    poDS(poDSIn),
    poFeatureDefn(new OGRFeatureDefn("records")),
    poBaseDS(nullptr),
    poBaseLayer(nullptr),
    nPagingStartIndex(0),
    nFeatureRead(0),
    nFeaturesInCurrentPage(0)
{
    SetDescription(poFeatureDefn->GetName());
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbPolygon);
    OGRSpatialReference* poSRS = new OGRSpatialReference(SRS_WKT_WGS84_LAT_LONG);
    poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
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
    if( !poDS->GetOutputSchema().empty() )
    {
        OGRFieldDefn oField("raw_xml", OFTString);
        poFeatureDefn->AddFieldDefn(&oField);
    }

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
    nPagingStartIndex = 0;
    nFeatureRead = 0;
    nFeaturesInCurrentPage = 0;
    GDALClose(poBaseDS);
    poBaseDS = nullptr;
    poBaseLayer = nullptr;
}

/************************************************************************/
/*                          GetNextFeature()                            */
/************************************************************************/

OGRFeature* OGRCSWLayer::GetNextFeature()
{
    while( true )
    {
        if (nFeatureRead == nPagingStartIndex + nFeaturesInCurrentPage)
        {
            nPagingStartIndex = nFeatureRead;

            GDALClose(poBaseDS);
            poBaseLayer = nullptr;

            poBaseDS = FetchGetRecords();
            if (poBaseDS)
            {
                poBaseLayer = poBaseDS->GetLayer(0);
                poBaseLayer->ResetReading();
                nFeaturesInCurrentPage = (int)poBaseLayer->GetFeatureCount();
            }
        }
        if (!poBaseLayer)
            return nullptr;

        OGRFeature* poSrcFeature = poBaseLayer->GetNextFeature();
        if (poSrcFeature == nullptr)
            return nullptr;
        nFeatureRead ++;

        OGRFeature* poNewFeature = new OGRFeature(poFeatureDefn);

        for(int i=0;i<poFeatureDefn->GetFieldCount();i++)
        {
            const char* pszFieldname = poFeatureDefn->GetFieldDefn(i)->GetNameRef();
            int iSrcField = poSrcFeature->GetFieldIndex(pszFieldname);
            /* http://www.paikkatietohakemisto.fi/geonetwork/srv/en/csw returns URI ... */
            if( iSrcField < 0 && strcmp(pszFieldname, "references") == 0 )
                iSrcField = poSrcFeature->GetFieldIndex("URI");
            if( iSrcField >= 0 && poSrcFeature->IsFieldSetAndNotNull(iSrcField) )
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
                    poGeom = nullptr;
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

        if( osCSWWhere.empty() &&
            m_poAttrQuery != nullptr &&
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
    if (psResult == nullptr)
    {
        return -1;
    }

    CPLXMLNode* psXML = CPLParseXMLString( (const char*) psResult->pabyData );
    if (psXML == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid XML content : %s",
                psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return -1;
    }
    CPLStripXMLNamespace( psXML, nullptr, TRUE );
    CPLHTTPDestroyResult(psResult);
    psResult = nullptr;

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
    CPLHTTPResult* psResult = nullptr;

    CPLString osOutputSchema = poDS->GetOutputSchema();
    if( !osOutputSchema.empty() )
        osOutputSchema = " outputSchema=\"" + osOutputSchema + "\"";

    CPLString osPost = CPLSPrintf(
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
"<csw:GetRecords resultType=\"results\" service=\"CSW\" version=\"%s\""
"%s"
" startPosition=\"%d\""
" maxRecords=\"%d\""
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
            osOutputSchema.c_str(),
            nPagingStartIndex + 1,
            poDS->GetMaxRecords(),
            poDS->GetElementSetName().c_str(),
            osQuery.c_str());

    psResult = poDS->HTTPFetch( poDS->GetBaseURL(), osPost);
    if (psResult == nullptr)
    {
        return nullptr;
    }

    CPLString osTmpDirName = CPLSPrintf("/vsimem/tempcsw_%p", this);
    VSIMkdir(osTmpDirName, 0);

    GByte *pabyData = psResult->pabyData;
    int    nDataLen = psResult->nDataLen;

    if (strstr((const char*)pabyData, "<ServiceExceptionReport") != nullptr ||
        strstr((const char*)pabyData, "<ows:ExceptionReport") != nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                 pabyData);
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }
    //CPLDebug("CSW", "%s", (const char*)pabyData);

    CPLString osTmpFileName;

    osTmpFileName = osTmpDirName + "/file.gfs";
    VSIUnlink(osTmpFileName);

    osTmpFileName = osTmpDirName + "/file.gml";

    VSILFILE *fp = VSIFileFromMemBuffer( osTmpFileName, pabyData,
                                         nDataLen, TRUE);
    VSIFCloseL(fp);
    psResult->pabyData = nullptr;

    CPLHTTPDestroyResult(psResult);

    GDALDataset* l_poBaseDS = nullptr;

    if( !poDS->GetOutputSchema().empty() )
    {
        GDALDriver* poDrv = (GDALDriver*)GDALGetDriverByName("Memory");
        if( poDrv == nullptr )
            return nullptr;
        CPLXMLNode* psRoot = CPLParseXMLFile(osTmpFileName);
        if( psRoot == nullptr )
        {
            if( strstr((const char*)pabyData, "<csw:GetRecordsResponse") == nullptr &&
                strstr((const char*)pabyData, "<GetRecordsResponse") == nullptr )
            {
                if (nDataLen > 1000)
                    pabyData[1000] = 0;
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Error: cannot parse %s", pabyData);
            }
            return nullptr;
        }
        CPLXMLNode* psSearchResults = CPLGetXMLNode(psRoot, "=csw:GetRecordsResponse.csw:SearchResults");
        if( psSearchResults == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find GetRecordsResponse.SearchResults");
            CPLDestroyXMLNode(psRoot);
            return nullptr;
        }

        l_poBaseDS = poDrv->Create("", 0, 0, 0, GDT_Unknown, nullptr);
        OGRLayer* poLyr = l_poBaseDS->CreateLayer("records");
        OGRFieldDefn oField("raw_xml", OFTString);
        poLyr->CreateField(&oField);
        for( CPLXMLNode* psIter = psSearchResults->psChild; psIter; psIter = psIter->psNext )
        {
            if( psIter->eType == CXT_Element )
            {
                OGRFeature* poFeature = new OGRFeature(poLyr->GetLayerDefn());

                CPLXMLNode* psNext = psIter->psNext;
                psIter->psNext = nullptr;
                char* pszXML = CPLSerializeXMLTree(psIter);

                const char* pszWest = nullptr;
                const char* pszEast = nullptr;
                const char* pszSouth = nullptr;
                const char* pszNorth = nullptr;
                CPLXMLNode* psBBox = CPLSearchXMLNode( psIter, "gmd:EX_GeographicBoundingBox");
                if( psBBox )
                {
                    /* ISO 19115/19119: http://www.isotc211.org/2005/gmd */
                    pszWest = CPLGetXMLValue(psBBox, "gmd:westBoundLongitude.gco:Decimal", nullptr);
                    pszEast = CPLGetXMLValue(psBBox, "gmd:eastBoundLongitude.gco:Decimal", nullptr);
                    pszSouth = CPLGetXMLValue(psBBox, "gmd:southBoundLatitude.gco:Decimal", nullptr);
                    pszNorth = CPLGetXMLValue(psBBox, "gmd:northBoundLatitude.gco:Decimal", nullptr);
                }
                else if( (psBBox = CPLSearchXMLNode( psIter, "spdom") ) != nullptr )
                {
                    /* FGDC: http://www.opengis.net/cat/csw/csdgm */
                    pszWest = CPLGetXMLValue(psBBox, "bounding.westbc", nullptr);
                    pszEast = CPLGetXMLValue(psBBox, "bounding.eastbc", nullptr);
                    pszSouth = CPLGetXMLValue(psBBox, "bounding.southbc", nullptr);
                    pszNorth = CPLGetXMLValue(psBBox, "bounding.northbc", nullptr);
                }
                if( pszWest && pszEast && pszSouth && pszNorth )
                {
                    double dfMinX = CPLAtof(pszWest);
                    double dfMaxX = CPLAtof(pszEast);
                    double dfMinY = CPLAtof(pszSouth);
                    double dfMaxY = CPLAtof(pszNorth);
                    OGRLinearRing* poLR = new OGRLinearRing();
                    poLR->addPoint(dfMinX, dfMinY);
                    poLR->addPoint(dfMinX, dfMaxY);
                    poLR->addPoint(dfMaxX, dfMaxY);
                    poLR->addPoint(dfMaxX, dfMinY);
                    poLR->addPoint(dfMinX, dfMinY);
                    OGRPolygon* poPoly = new OGRPolygon();
                    poPoly->addRingDirectly(poLR);
                    poFeature->SetGeometryDirectly(poPoly);
                }
                else if( (psBBox = CPLSearchXMLNode( psIter, "ows:BoundingBox") ) != nullptr )
                {
                    CPLFree(psBBox->pszValue);
                    psBBox->pszValue = CPLStrdup("gml:Envelope");
                    CPLString osSRS = CPLGetXMLValue(psBBox, "crs", "");
                    OGRGeometry* poGeom =
                        GML2OGRGeometry_XMLNode( psBBox,
                                                 FALSE,
                                                 0, 0, false, true,
                                                 false );
                    if( poGeom )
                    {
                        bool bLatLongOrder = true;
                        if( !osSRS.empty() )
                            bLatLongOrder = GML_IsSRSLatLongOrder(osSRS);
                        if( bLatLongOrder && CPLTestBool(
                                CPLGetConfigOption("GML_INVERT_AXIS_ORDER_IF_LAT_LONG", "YES")) )
                            poGeom->swapXY();
                        poFeature->SetGeometryDirectly(poGeom);
                    }
                }

                psIter->psNext = psNext;

                poFeature->SetField(0, pszXML);
                CPL_IGNORE_RET_VAL(poLyr->CreateFeature(poFeature));
                CPLFree(pszXML);
                delete poFeature;
            }
        }
        CPLDestroyXMLNode(psRoot);
    }
    else
    {
        l_poBaseDS = (GDALDataset*) OGROpen(osTmpFileName, FALSE, nullptr);
        if (l_poBaseDS == nullptr)
        {
            if( strstr((const char*)pabyData, "<csw:GetRecordsResponse") == nullptr &&
                strstr((const char*)pabyData, "<GetRecordsResponse") == nullptr )
            {
                if (nDataLen > 1000)
                    pabyData[1000] = 0;
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Error: cannot parse %s", pabyData);
            }
            return nullptr;
        }
    }

    OGRLayer* poLayer = l_poBaseDS->GetLayer(0);
    if (poLayer == nullptr)
    {
        GDALClose(l_poBaseDS);
        return nullptr;
    }

    return l_poBaseDS;
}

/************************************************************************/
/*                         SetSpatialFilter()                           */
/************************************************************************/

void OGRCSWLayer::SetSpatialFilter( OGRGeometry * poGeom )
{
    OGRLayer::SetSpatialFilter(poGeom);
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
    if (pszFilter != nullptr && pszFilter[0] == 0)
        pszFilter = nullptr;

    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = (pszFilter) ? CPLStrdup(pszFilter) : nullptr;

    delete m_poAttrQuery;
    m_poAttrQuery = nullptr;

    if( pszFilter != nullptr )
    {
        m_poAttrQuery = new OGRFeatureQuery();

        OGRErr eErr = m_poAttrQuery->Compile( GetLayerDefn(), pszFilter, TRUE,
                                              WFSGetCustomFuncRegistrar() );
        if( eErr != OGRERR_NONE )
        {
            delete m_poAttrQuery;
            m_poAttrQuery = nullptr;
            return eErr;
        }
    }

    if (m_poAttrQuery != nullptr )
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
                                                      nullptr,
                                                      nullptr,
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

    if (m_poAttrQuery != nullptr && osCSWWhere.empty())
    {
        CPLDebug("CSW", "Using client-side only mode for filter \"%s\"", pszFilter);
        OGRErr eErr = OGRLayer::SetAttributeFilter(pszFilter);
        if (eErr != OGRERR_NONE)
            return eErr;
    }

    ResetReading();
    BuildQuery();

    return OGRERR_NONE;
}

/************************************************************************/
/*                                BuildQuery()                          */
/************************************************************************/

void OGRCSWLayer::BuildQuery()
{
    if( m_poFilterGeom != nullptr || !osCSWWhere.empty() )
    {
        osQuery = "<csw:Constraint version=\"1.1.0\">";
        osQuery += "<ogc:Filter>";
        if( m_poFilterGeom != nullptr && !osCSWWhere.empty() )
            osQuery += "<ogc:And>";
        if( m_poFilterGeom != nullptr )
        {
            osQuery += "<ogc:BBOX>";
            osQuery += "<ogc:PropertyName>ows:BoundingBox</ogc:PropertyName>";
            osQuery += "<gml:Envelope srsName=\"urn:ogc:def:crs:EPSG::4326\">";
            OGREnvelope sEnvelope;
            m_poFilterGeom->getEnvelope(&sEnvelope);
            if( CPLTestBool(
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
        if( m_poFilterGeom != nullptr && !osCSWWhere.empty() )
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

OGRCSWDataSource::OGRCSWDataSource() :
    pszName(nullptr),
    nMaxRecords(500),
    poLayer(nullptr),
    bFullExtentRecordsAsNonSpatial(false)
{}

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

    CPLDebug("CSW", "%s", osURL.c_str());

    CPLHTTPResult* psResult = HTTPFetch( osURL, nullptr);
    if (psResult == nullptr)
    {
        return nullptr;
    }

    if (strstr((const char*)psResult->pabyData,
                                    "<ServiceExceptionReport") != nullptr ||
        strstr((const char*)psResult->pabyData,
                                    "<ows:ExceptionReport") != nullptr ||
        strstr((const char*)psResult->pabyData,
                                    "<ExceptionReport") != nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }

    return psResult;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRCSWDataSource::Open( const char * pszFilename,
                            char** papszOpenOptionsIn )
{
    const char* pszBaseURL = CSLFetchNameValue(papszOpenOptionsIn, "URL");
    if( pszBaseURL == nullptr )
    {
        pszBaseURL = pszFilename;
        if (STARTS_WITH_CI(pszFilename, "CSW:"))
            pszBaseURL += 4;
        if( pszBaseURL[0] == '\0' )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Missing URL open option");
            return FALSE;
        }
    }
    osBaseURL = pszBaseURL;
    osElementSetName = CSLFetchNameValueDef(papszOpenOptionsIn, "ELEMENTSETNAME",
                                            "full");
    bFullExtentRecordsAsNonSpatial =
        CPLFetchBool(papszOpenOptionsIn,
                     "FULL_EXTENT_RECORDS_AS_NON_SPATIAL", false);
    osOutputSchema = CSLFetchNameValueDef(papszOpenOptionsIn, "OUTPUT_SCHEMA", "");
    if( EQUAL(osOutputSchema, "gmd") )
        osOutputSchema = "http://www.isotc211.org/2005/gmd";
    else if( EQUAL(osOutputSchema, "csw") )
        osOutputSchema = "http://www.opengis.net/cat/csw/2.0.2";
    nMaxRecords = atoi(CSLFetchNameValueDef(papszOpenOptionsIn, "MAX_RECORDS", "500"));

    if (!STARTS_WITH(osBaseURL, "http://") &&
        !STARTS_WITH(osBaseURL, "https://") &&
        !STARTS_WITH(osBaseURL, "/vsimem/"))
        return FALSE;

    CPLHTTPResult* psResult = SendGetCapabilities();
    if( psResult == nullptr )
        return FALSE;

    CPLXMLNode* psXML = CPLParseXMLString( (const char*) psResult->pabyData );
    if (psXML == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid XML content : %s",
                psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return FALSE;
    }
    CPLStripXMLNamespace( psXML, nullptr, TRUE );
    CPLHTTPDestroyResult(psResult);
    psResult = nullptr;

    const char* pszVersion = CPLGetXMLValue( psXML, "=Capabilities.version", nullptr);
    if( pszVersion == nullptr )
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
    if( iLayer < 0 || iLayer >= ((poLayer != nullptr) ? 1 : 0) )
        return nullptr;
    else
        return poLayer;
}

/************************************************************************/
/*                            HTTPFetch()                               */
/************************************************************************/

CPLHTTPResult* OGRCSWDataSource::HTTPFetch( const char* pszURL, const char* pszPost )
{
    char** papszOptions = nullptr;
    if( pszPost )
    {
        papszOptions = CSLAddNameValue(papszOptions, "POSTFIELDS", pszPost);
        papszOptions = CSLAddNameValue(papszOptions, "HEADERS",
                                    "Content-Type: application/xml; charset=UTF-8");
    }
    CPLHTTPResult* psResult = CPLHTTPFetch( pszURL, papszOptions );
    CSLDestroy(papszOptions);

    if (psResult == nullptr)
    {
        return nullptr;
    }
    if (psResult->nStatus != 0 || psResult->pszErrBuf != nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s (%d)",
                 (psResult->pszErrBuf) ? psResult->pszErrBuf : "unknown", psResult->nStatus);
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }
    if (psResult->pabyData == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Empty content returned by server");
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }
    return psResult;
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

static int OGRCSWDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    return STARTS_WITH_CI(poOpenInfo->pszFilename, "CSW:");
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRCSWDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( !OGRCSWDriverIdentify(poOpenInfo) || poOpenInfo->eAccess == GA_Update )
        return nullptr;

    OGRCSWDataSource   *poDS = new OGRCSWDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename,
                     poOpenInfo->papszOpenOptions ) )
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRCSW()                           */
/************************************************************************/

void RegisterOGRCSW()

{
    if( GDALGetDriverByName( "CSW" ) != nullptr )
        return;

    GDALDriver  *poDriver = new GDALDriver();

    poDriver->SetDescription( "CSW" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "OGC CSW (Catalog  Service for the Web)" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/csw.html" );

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
"  <Option name='OUTPUT_SCHEMA' type='string' description='Value of outputSchema parameter'/>"
"  <Option name='MAX_RECORDS' type='int' description='Maximum number of records to retrieve in a single time' default='500'/>"
"</OpenOptionList>" );

    poDriver->pfnIdentify = OGRCSWDriverIdentify;
    poDriver->pfnOpen = OGRCSWDriverOpen;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
