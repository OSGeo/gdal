/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRGeoJSONDriver class (OGR GeoJSON Driver).
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
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
#include "ogr_geojson.h"
#include <cpl_conv.h>
#include "cpl_http.h"

class OGRESRIFeatureServiceDataset;

/************************************************************************/
/*                      OGRESRIFeatureServiceLayer                      */
/************************************************************************/

class OGRESRIFeatureServiceLayer: public OGRLayer
{
        OGRESRIFeatureServiceDataset* poDS;
        OGRFeatureDefn* poFeatureDefn;
        GIntBig         nFeaturesRead;
        GIntBig         nLastFID;
        int             bOtherPage;
        int             bUseSequentialFID;

    public:
        OGRESRIFeatureServiceLayer(OGRESRIFeatureServiceDataset* poDS);
       ~OGRESRIFeatureServiceLayer();

        void ResetReading();
        OGRFeature* GetNextFeature();
        GIntBig GetFeatureCount( int bForce = TRUE );
        OGRErr              GetExtent(OGREnvelope *psExtent, int bForce = TRUE);
        int TestCapability( const char* pszCap );
        OGRFeatureDefn* GetLayerDefn() { return poFeatureDefn; }
};

/************************************************************************/
/*                       OGRESRIFeatureServiceDataset                   */
/************************************************************************/

class OGRESRIFeatureServiceDataset: public GDALDataset
{
        CPLString              osURL;
        GIntBig                nFirstOffset, nLastOffset;
        OGRGeoJSONDataSource* poCurrent;
        OGRESRIFeatureServiceLayer* poLayer;
        
        int                     LoadPage();

    public:
        OGRESRIFeatureServiceDataset(const CPLString &osURL,
                                     OGRGeoJSONDataSource* poFirst);
       ~OGRESRIFeatureServiceDataset();

        int GetLayerCount() { return 1; }
        OGRLayer* GetLayer( int nLayer ) { return (nLayer == 0) ? poLayer : NULL; }
        
        OGRLayer* GetUnderlyingLayer() { return poCurrent->GetLayer(0); }

        int ResetReading();
        int LoadNextPage();
        
        const CPLString&                GetURL() { return osURL; }
};

/************************************************************************/
/*                       OGRESRIFeatureServiceLayer()                   */
/************************************************************************/

OGRESRIFeatureServiceLayer::OGRESRIFeatureServiceLayer(OGRESRIFeatureServiceDataset* poDS)
{
    this->poDS = poDS;
    OGRFeatureDefn* poSrcFeatDefn = poDS->GetUnderlyingLayer()->GetLayerDefn();
    poFeatureDefn = new OGRFeatureDefn(poSrcFeatDefn->GetName());
    SetDescription(poFeatureDefn->GetName());
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);
    for(int i=0;i<poSrcFeatDefn->GetFieldCount();i++)
        poFeatureDefn->AddFieldDefn(poSrcFeatDefn->GetFieldDefn(i));
    for(int i=0;i<poSrcFeatDefn->GetGeomFieldCount();i++)
        poFeatureDefn->AddGeomFieldDefn(poSrcFeatDefn->GetGeomFieldDefn(i));
    nFeaturesRead = 0;
    nLastFID = 0;
    bOtherPage = FALSE;
    bUseSequentialFID = FALSE;
}

/************************************************************************/
/*                      ~OGRESRIFeatureServiceLayer()                   */
/************************************************************************/

OGRESRIFeatureServiceLayer::~OGRESRIFeatureServiceLayer()
{
    poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRESRIFeatureServiceLayer::ResetReading()
{
    poDS->ResetReading();
    nFeaturesRead = 0;
    nLastFID = 0;
    bOtherPage = FALSE;
    bUseSequentialFID = FALSE;
}

/************************************************************************/
/*                            GetNextFeature()                          */
/************************************************************************/

OGRFeature* OGRESRIFeatureServiceLayer::GetNextFeature()
{
    while( TRUE )
    {
        int bWasInFirstPage = !bOtherPage;
        OGRFeature* poSrcFeat = poDS->GetUnderlyingLayer()->GetNextFeature();
        if( poSrcFeat == NULL )
        {
            if( !poDS->LoadNextPage() )
                return NULL;
            poSrcFeat = poDS->GetUnderlyingLayer()->GetNextFeature();
            if( poSrcFeat == NULL )
                return NULL;
            bOtherPage = TRUE;
        }
        if( bOtherPage && bWasInFirstPage && poSrcFeat->GetFID() == 0 &&
            nLastFID == nFeaturesRead - 1 )
        {
            bUseSequentialFID = TRUE;
        }

        OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
        poFeature->SetFrom(poSrcFeat);
        if( bUseSequentialFID )
            poFeature->SetFID(nFeaturesRead);
        else
            poFeature->SetFID(poSrcFeat->GetFID());
        nLastFID = poFeature->GetFID();
        nFeaturesRead ++;
        delete poSrcFeat;
        
        if((m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeometryRef() ) )
        && (m_poAttrQuery == NULL
            || m_poAttrQuery->Evaluate( poFeature )) )
        {
            return poFeature;
        }
        delete poFeature;
    }
}

/************************************************************************/
/*                          TestCapability()                            */
/************************************************************************/

int OGRESRIFeatureServiceLayer::TestCapability( const char* pszCap )
{
    if( EQUAL(pszCap, OLCFastFeatureCount) )
        return m_poAttrQuery == NULL && m_poFilterGeom == NULL;
    if( EQUAL(pszCap, OLCFastGetExtent) )
        return FALSE;
    return poDS->GetUnderlyingLayer()->TestCapability(pszCap);
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRESRIFeatureServiceLayer::GetFeatureCount( int bForce )
{
    GIntBig nFeatureCount = -1;
    if( m_poAttrQuery == NULL && m_poFilterGeom == NULL ) 
    {
        CPLString osNewURL = CPLURLAddKVP(poDS->GetURL(), "returnCountOnly", "true");
        CPLHTTPResult* pResult = NULL;
        CPLErrorReset();
        pResult = CPLHTTPFetch( osNewURL, NULL );
        if( pResult != NULL && pResult->nDataLen != 0 && CPLGetLastErrorNo() == 0 &&
            pResult->nStatus == 0 )
        {
            const char* pszCount = strstr((const char*)pResult->pabyData, "\"count\"");
            if( pszCount )
            {
                pszCount = strchr(pszCount, ':');
                if( pszCount )
                {
                    pszCount++;
                    nFeatureCount = CPLAtoGIntBig(pszCount);
                }
            }
        }
        CPLHTTPDestroyResult( pResult );
    }
    if( nFeatureCount < 0 )
        nFeatureCount = OGRLayer::GetFeatureCount(bForce);
    return nFeatureCount;
}

/************************************************************************/
/*                               GetExtent()                            */
/************************************************************************/

OGRErr OGRESRIFeatureServiceLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    OGRErr eErr = OGRERR_FAILURE;
    CPLString osNewURL = CPLURLAddKVP(poDS->GetURL(), "returnExtentOnly", "true");
    osNewURL = CPLURLAddKVP(osNewURL, "f", "geojson");
    CPLHTTPResult* pResult = NULL;
    CPLErrorReset();
    pResult = CPLHTTPFetch( osNewURL, NULL );
    if( pResult != NULL && pResult->nDataLen != 0 && CPLGetLastErrorNo() == 0 &&
        pResult->nStatus == 0 )
    {
        const char* pszBBox = strstr((const char*)pResult->pabyData, "\"bbox\"");
        if( pszBBox )
        {
            pszBBox = strstr(pszBBox, ":[");
            if( pszBBox )
            {
                pszBBox+=2;
                char** papszTokens = CSLTokenizeString2(pszBBox, ",", 0);
                if( CSLCount(papszTokens) >= 4 )
                {
                    psExtent->MinX = CPLAtof(papszTokens[0]);
                    psExtent->MinY = CPLAtof(papszTokens[1]);
                    psExtent->MaxX = CPLAtof(papszTokens[2]);
                    psExtent->MaxY = CPLAtof(papszTokens[3]);
                    eErr = OGRERR_NONE;
                }
                CSLDestroy(papszTokens);
            }
        }
    }
    CPLHTTPDestroyResult( pResult );
    if( eErr == OGRERR_FAILURE )
        eErr = OGRLayer::GetExtent(psExtent, bForce);
    return eErr;
}

/************************************************************************/
/*                      OGRESRIFeatureServiceDataset()                  */
/************************************************************************/

OGRESRIFeatureServiceDataset::OGRESRIFeatureServiceDataset(const CPLString &osURL,
                                                           OGRGeoJSONDataSource* poFirst)
{
    poCurrent = poFirst;
    poLayer = new OGRESRIFeatureServiceLayer(this);
    this->osURL = osURL;
    if( CPLURLGetValue(this->osURL, "resultRecordCount").size() == 0 )
    {
        // We assume that if the server sets the exceededTransferLimit, the
        // and resultRecordCount is not set, the number of features returned
        // in our first request is the maximum allowed by the server
        // So set it for following requests
        this->osURL = CPLURLAddKVP(this->osURL, "resultRecordCount",
                CPLSPrintf("%d", (int)poFirst->GetLayer(0)->GetFeatureCount()));
    }
    else
    {
        int nUserSetRecordCount = atoi(CPLURLGetValue(this->osURL, "resultRecordCount"));
        if( nUserSetRecordCount > poFirst->GetLayer(0)->GetFeatureCount() )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Specificied resultRecordCount=%d is greater than the maximum %d supported by the server",
                     nUserSetRecordCount, (int)poFirst->GetLayer(0)->GetFeatureCount() );
        }
    }
    nFirstOffset = CPLAtoGIntBig(CPLURLGetValue(this->osURL, "resultOffset"));
    nLastOffset = nFirstOffset;
}

/************************************************************************/
/*                      ~OGRESRIFeatureServiceDataset()                 */
/************************************************************************/

OGRESRIFeatureServiceDataset::~OGRESRIFeatureServiceDataset()
{
    delete poCurrent;
    delete poLayer;
}

/************************************************************************/
/*                             ResetReading()                           */
/************************************************************************/

int OGRESRIFeatureServiceDataset::ResetReading()
{
    if( nLastOffset > nFirstOffset )
    {
        nLastOffset = nFirstOffset;
        return LoadPage();
    }
    else
    {
        poCurrent->GetLayer(0)->ResetReading();
        return TRUE;
    }
}

/************************************************************************/
/*                             LoadNextPage()                           */
/************************************************************************/

int OGRESRIFeatureServiceDataset::LoadNextPage()
{
    if( !poCurrent->HasOtherPages() )
        return FALSE;
    nLastOffset += poCurrent->GetLayer(0)->GetFeatureCount();
    return LoadPage();
}

/************************************************************************/
/*                                 LoadPage()                           */
/************************************************************************/

int OGRESRIFeatureServiceDataset::LoadPage()
{
    CPLString osNewURL = CPLURLAddKVP(osURL, "resultOffset",
                                        CPLSPrintf(CPL_FRMT_GIB, nLastOffset));
    OGRGeoJSONDataSource* poDS = NULL;
    poDS = new OGRGeoJSONDataSource();
    GDALOpenInfo oOpenInfo(osNewURL, GA_ReadOnly);
    if( !poDS->Open( &oOpenInfo, GeoJSONGetSourceType( &oOpenInfo ) ) ||
        poDS->GetLayerCount() == 0 )
    {
        delete poDS;
        poDS= NULL;
        return FALSE;
    }
    delete poCurrent;
    poCurrent = poDS;
    return TRUE;
}
    

/************************************************************************/
/*                        OGRGeoJSONDriverIdentify()                    */
/************************************************************************/

static int OGRGeoJSONDriverIdentifyInternal( GDALOpenInfo* poOpenInfo,
                                     GeoJSONSourceType& nSrcType )
{
/* -------------------------------------------------------------------- */
/*      Determine type of data source: text file (.geojson, .json),     */
/*      Web Service or text passed directly and load data.              */
/* -------------------------------------------------------------------- */

    nSrcType = GeoJSONGetSourceType( poOpenInfo );
    if( nSrcType == eGeoJSONSourceUnknown )
        return FALSE;
    if( nSrcType == eGeoJSONSourceService )
        return -1;
    return TRUE;
}

/************************************************************************/
/*                        OGRGeoJSONDriverIdentify()                    */
/************************************************************************/

static int OGRGeoJSONDriverIdentify( GDALOpenInfo* poOpenInfo )
{
    GeoJSONSourceType nSrcType;
    return OGRGeoJSONDriverIdentifyInternal(poOpenInfo, nSrcType);
}

/************************************************************************/
/*                           Open()                                     */
/************************************************************************/

static GDALDataset* OGRGeoJSONDriverOpen( GDALOpenInfo* poOpenInfo )
{
    GeoJSONSourceType nSrcType;
    if( OGRGeoJSONDriverIdentifyInternal(poOpenInfo, nSrcType) == FALSE )
        return NULL;

    OGRGeoJSONDataSource* poDS = NULL;
    poDS = new OGRGeoJSONDataSource();

/* -------------------------------------------------------------------- */
/*      Processing configuration options.                               */
/* -------------------------------------------------------------------- */

    // TODO: Currently, options are based on environment variables.
    //       This is workaround for not yet implemented Andrey's concept
    //       described in document 'RFC 10: OGR Open Parameters'.

    poDS->SetGeometryTranslation( OGRGeoJSONDataSource::eGeometryPreserve );
    const char* pszOpt = CPLGetConfigOption("GEOMETRY_AS_COLLECTION", NULL);
    if( NULL != pszOpt && EQUALN(pszOpt, "YES", 3) )
    {
            poDS->SetGeometryTranslation(
                OGRGeoJSONDataSource::eGeometryAsCollection );
    }

    poDS->SetAttributesTranslation( OGRGeoJSONDataSource::eAtributesPreserve );
    pszOpt = CPLGetConfigOption("ATTRIBUTES_SKIP", NULL);
    if( NULL != pszOpt && EQUALN(pszOpt, "YES", 3) )
    {
        poDS->SetAttributesTranslation( 
            OGRGeoJSONDataSource::eAtributesSkip );
    }

/* -------------------------------------------------------------------- */
/*      Open and start processing GeoJSON datasoruce to OGR objects.    */
/* -------------------------------------------------------------------- */
    if( !poDS->Open( poOpenInfo, nSrcType ) )
    {
        delete poDS;
        poDS= NULL;
    }

    if( NULL != poDS && poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "GeoJSON Driver doesn't support update." );
        delete poDS;
        return NULL;
    }
    
    if( poDS != NULL && poDS->HasOtherPages() )
    {
        const char* pszFSP = CSLFetchNameValue(poOpenInfo->papszOpenOptions,
                                               "FEATURE_SERVER_PAGING");
        int bHasResultOffset = CPLURLGetValue(poOpenInfo->pszFilename, "resultOffset").size() > 0;
        if( (!bHasResultOffset && (pszFSP == NULL || CSLTestBoolean(pszFSP))) ||
            (bHasResultOffset && pszFSP != NULL && CSLTestBoolean(pszFSP)) )
        {
            return new OGRESRIFeatureServiceDataset(poOpenInfo->pszFilename,
                                                    poDS);
        }
    }

    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGRGeoJSONDriverCreate( const char * pszName,
                                            CPL_UNUSED int nBands,
                                            CPL_UNUSED int nXSize,
                                            CPL_UNUSED int nYSize,
                                            CPL_UNUSED GDALDataType eDT,
                                            char **papszOptions )
{
    OGRGeoJSONDataSource* poDS = new OGRGeoJSONDataSource();

    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                               Delete()                               */
/************************************************************************/

static CPLErr OGRGeoJSONDriverDelete( const char *pszFilename )
{
    if( VSIUnlink( pszFilename ) == 0 )
    {
        return CE_None;
    }
    
    CPLDebug( "GeoJSON", "Failed to delete \'%s\'", pszFilename);

    return CE_Failure;
}

/************************************************************************/
/*                           RegisterOGRGeoJSON()                       */
/************************************************************************/

void RegisterOGRGeoJSON()
{
    if( !GDAL_CHECK_VERSION("OGR/GeoJSON driver") )
        return;

    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "GeoJSON" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "GeoJSON" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "GeoJSON" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "json geojson topojson" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_geojson.html" );

        poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='FLATTEN_NESTED_ATTRIBUTES' type='boolean' description='Whether to recursively explore nested objects and produce flatten OGR attributes' default='NO'/>"
"  <Option name='NESTED_ATTRIBUTE_SEPARATOR' type='string' description='Separator between components of nested attributes' default='_'/>"
"  <Option name='FEATURE_SERVER_PAGING' type='boolean' description='Whether to automatically scroll through results with a ArcGIS Feature Service endpoint'/>"
"</OpenOptionList>");

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, "<CreationOptionList/>");

        poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"  <Option name='WRITE_BBOX' type='boolean' description='whether to write a bbox property with the bounding box of the geometries at the feature and feature collection level' default='NO'/>"
"  <Option name='COORDINATE_PRECISION' type='int' description='Number of decimal for coordinates' default='10'/>"
"</LayerCreationOptionList>");

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
        
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES, "Integer Integer64 Real String IntegerList Integer64List RealList StringList" );

        poDriver->pfnOpen = OGRGeoJSONDriverOpen;
        poDriver->pfnIdentify = OGRGeoJSONDriverIdentify;
        poDriver->pfnCreate = OGRGeoJSONDriverCreate;
        poDriver->pfnDelete = OGRGeoJSONDriverDelete;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
