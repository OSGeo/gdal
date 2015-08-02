/******************************************************************************
 * $Id$
 *
 * Project:  IDF Translator
 * Purpose:  Implements OGRIDFDriver.
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
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

#include "ogr_idf.h"
#include "cpl_conv.h"
#include <map>

CPL_CVSID("$Id$");

// g++ ogr/ogrsf_frmts/idf/*.cpp -Wall -g -fPIC -shared -o ogr_IDF.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts/idf -Iogr/ogrsf_frmts

extern "C" void RegisterOGRIDF();

typedef enum
{
    LAYER_OTHER,
    LAYER_NODE,
    LAYER_LINK,
    LAYER_LINKCOORDINATE
} IDFLayerType;

/************************************************************************/
/*                           OGRIDFDataSource()                         */
/************************************************************************/

OGRIDFDataSource::OGRIDFDataSource(VSILFILE* fpL) : fpL(fpL)
{
    bHasParsed = FALSE;
    poMemDS = NULL;
}

/************************************************************************/
/*                          ~OGRIDFDataSource()                         */
/************************************************************************/

OGRIDFDataSource::~OGRIDFDataSource()
{
    delete poMemDS;
    if( fpL )
        VSIFCloseL(fpL);
}

/************************************************************************/
/*                                Parse()                               */
/************************************************************************/

void OGRIDFDataSource::Parse()
{
    bHasParsed = TRUE;
    GDALDriver* poMemDRV = (GDALDriver*)GDALGetDriverByName("MEMORY");
    if( poMemDRV == NULL )
        return;
    poMemDS = poMemDRV->Create("", 0, 0, 0, GDT_Unknown, NULL);
    OGRLayer* poCurLayer = NULL;
    std::map<GIntBig, std::pair<double,double> > oMapNode; // map from NODE_ID to (X,Y)
    std::map<GIntBig, OGRLineString*> oMapLinkCoordinate; // map from LINK_ID to OGRLineString*
    CPLString osTablename, osAtr, osFrm;
    int iX = -1, iY = -1;
    int bAdvertizeUTF8 = FALSE;
    int bRecodeFromLatin1 = FALSE;
    int iNodeID = -1;
    int iLinkID = -1;
    int iCount = -1;
    int iFromNode = -1;
    int iToNode = -1;
    IDFLayerType eLayerType = LAYER_OTHER;
    
    // We assume that layers are in the order Node, Link, LinkCoordinate
    
    while(TRUE)
    {
        const char* pszLine = CPLReadLineL(fpL);
        if( pszLine == NULL )
            break;
        
        if( strcmp(pszLine, "chs;ISO_LATIN_1") == 0)
        {
            bAdvertizeUTF8 = TRUE;
            bRecodeFromLatin1 = TRUE;
        }
        else if( strncmp(pszLine, "tbl;", 4) == 0 )
        {
            poCurLayer = NULL;
            osTablename = pszLine + 4;
            osAtr = "";
            osFrm = "";
            iX = iY = iNodeID = iLinkID = iCount = iFromNode = iToNode = -1;
            eLayerType = LAYER_OTHER;
        }
        else if( strncmp(pszLine, "atr;", 4) == 0 )
        {
            osAtr = pszLine + 4;
        }
        else if( strncmp(pszLine, "frm;", 4) == 0 )
        {
            osFrm = pszLine + 4;
        }
        else if( strncmp(pszLine, "rec;", 4) == 0 )
        {
            if( poCurLayer == NULL )
            {
                char** papszAtr = CSLTokenizeStringComplex(osAtr,";",TRUE,TRUE);
                char** papszFrm = CSLTokenizeStringComplex(osFrm,";",TRUE,TRUE);
                char* apszOptions[2] = { NULL, NULL };
                if( bAdvertizeUTF8 )
                    apszOptions[0] = (char*)"ADVERTIZE_UTF8=YES";
                
                if( EQUAL(osTablename, "Node") &&
                    (iX = CSLFindString(papszAtr, "X")) >= 0 &&
                    (iY = CSLFindString(papszAtr, "Y")) >= 0 )
                {
                    eLayerType = LAYER_NODE;
                    iNodeID = CSLFindString(papszAtr, "NODE_ID");
                    OGRSpatialReference* poSRS = new OGRSpatialReference(SRS_WKT_WGS84);
                    poCurLayer = poMemDS->CreateLayer(osTablename, poSRS, wkbPoint, apszOptions);
                    poSRS->Release();
                }
                else if( EQUAL(osTablename, "Link") &&
                        (iLinkID = CSLFindString(papszAtr, "LINK_ID")) >= 0 &&
                        ((iFromNode = CSLFindString(papszAtr, "FROM_NODE")) >= 0) &&
                        ((iToNode = CSLFindString(papszAtr, "TO_NODE")) >= 0) )
                {
                    eLayerType = LAYER_LINK;
                    OGRSpatialReference* poSRS = new OGRSpatialReference(SRS_WKT_WGS84);
                    poCurLayer = poMemDS->CreateLayer(osTablename, poSRS, wkbLineString, apszOptions);
                    poSRS->Release();
                }
                else if( EQUAL(osTablename, "LinkCoordinate") &&
                        (iLinkID = CSLFindString(papszAtr, "LINK_ID")) >= 0 &&
                        (iCount = CSLFindString(papszAtr, "COUNT")) >= 0 &&
                        (iX = CSLFindString(papszAtr, "X")) >= 0 &&
                        (iY = CSLFindString(papszAtr, "Y")) >= 0 )
                {
                    eLayerType = LAYER_LINKCOORDINATE;
                    OGRSpatialReference* poSRS = new OGRSpatialReference(SRS_WKT_WGS84);
                    poCurLayer = poMemDS->CreateLayer(osTablename, poSRS, wkbPoint, apszOptions);
                    poSRS->Release();
                }
                else
                {
                    poCurLayer = poMemDS->CreateLayer(osTablename, NULL, wkbNone, apszOptions);
                }
                
                if( CSLCount(papszAtr) == CSLCount(papszFrm) )
                {
                    for(int i=0; papszAtr[i]; i++)
                    {
                        OGRFieldType eType = OFTString;
                        if( EQUALN(papszFrm[i], "decimal", strlen("decimal")) )
                        {
                            if( papszFrm[i][strlen("decimal")] == '(' )
                            {
                                if( strchr(papszFrm[i], ',') )
                                    eType = OFTReal;
                                else
                                {
                                    int nWidth = atoi(papszFrm[i] + strlen("decimal") + 1);
                                    if( nWidth >= 10 )
                                        eType = OFTInteger64;
                                    else
                                        eType = OFTInteger;
                                }
                            }
                        }
                        OGRFieldDefn oFieldDefn(papszAtr[i], eType);
                        poCurLayer->CreateField(&oFieldDefn);
                    }
                }
                CSLDestroy(papszAtr);
                CSLDestroy(papszFrm);
            }
            
            OGRErr eErr = OGRERR_NONE;
            char** papszTokens = CSLTokenizeStringComplex(pszLine + 4,";",TRUE,TRUE);
            OGRFeatureDefn* poFDefn = poCurLayer->GetLayerDefn();
            if( CSLCount(papszTokens) >= poFDefn->GetFieldCount() )
            {
                OGRFeature* poFeature = new OGRFeature(poFDefn);
                for(int i=0, j=0; papszTokens[i] != NULL ;i++)
                {
                    if( papszTokens[i][0] && j < poFDefn->GetFieldCount() )
                    {
                        if( bRecodeFromLatin1 &&
                            poFDefn->GetFieldDefn(j)->GetType() == OFTString )
                        {
                            char* pszRecoded = CPLRecode(papszTokens[i],
                                             CPL_ENC_ISO8859_1, CPL_ENC_UTF8);
                            poFeature->SetField(j, pszRecoded);
                            CPLFree(pszRecoded);
                        }
                        else
                        {
                            poFeature->SetField(j, papszTokens[i]);
                        }
                    }
                    j++;
                }

                if( eLayerType == LAYER_NODE )
                {
                    double dfX = poFeature->GetFieldAsDouble(iX);
                    double dfY = poFeature->GetFieldAsDouble(iY);
                    oMapNode[ poFeature->GetFieldAsInteger64(iNodeID) ] =
                                            std::pair<double,double>(dfX, dfY);
                    OGRGeometry* poGeom = new OGRPoint( dfX, dfY );
                    poGeom->assignSpatialReference(poFDefn->GetGeomFieldDefn(0)->GetSpatialRef());
                    poFeature->SetGeometryDirectly(poGeom);
                }
                else if( eLayerType == LAYER_LINK )
                {
                    GIntBig nFromNode = poFeature->GetFieldAsInteger64(iFromNode);
                    GIntBig nToNode = poFeature->GetFieldAsInteger64(iToNode);
                    std::map<GIntBig, std::pair<double,double> >::iterator
                                            oIterFrom = oMapNode.find(nFromNode);
                    std::map<GIntBig, std::pair<double,double> >::iterator
                                            oIterTo = oMapNode.find(nToNode);
                    if( oIterFrom != oMapNode.end() && oIterTo != oMapNode.end() )
                    {
                        OGRLineString* poLS = new OGRLineString();
                        poLS->addPoint( oIterFrom->second.first, oIterFrom->second.second );
                        poLS->addPoint( oIterTo->second.first, oIterTo->second.second );
                        poLS->assignSpatialReference(poFDefn->GetGeomFieldDefn(0)->GetSpatialRef());
                        poFeature->SetGeometryDirectly(poLS);
                    }
                }
                else if( eLayerType == LAYER_LINKCOORDINATE )
                {
                    double dfX = poFeature->GetFieldAsDouble(iX);
                    double dfY = poFeature->GetFieldAsDouble(iY);
                    OGRGeometry* poGeom = new OGRPoint( dfX, dfY );
                    poGeom->assignSpatialReference(poFDefn->GetGeomFieldDefn(0)->GetSpatialRef());
                    poFeature->SetGeometryDirectly(poGeom);
                    
                    GIntBig nCurLinkID = poFeature->GetFieldAsInteger64(iLinkID);
                    std::map<GIntBig, OGRLineString*>::iterator
                        oMapLinkCoordinateIter = oMapLinkCoordinate.find(nCurLinkID);
                    if( oMapLinkCoordinateIter == oMapLinkCoordinate.end() )
                    {
                        OGRLineString* poLS = new OGRLineString();
                        poLS->addPoint(dfX, dfY);
                        oMapLinkCoordinate[nCurLinkID] = poLS;
                    }
                    else
                    {
                        oMapLinkCoordinateIter->second->addPoint(dfX, dfY);
                    }
                }
                eErr = poCurLayer->CreateFeature(poFeature);
                delete poFeature;
            }
            CSLDestroy(papszTokens);
            
            if( eErr == OGRERR_FAILURE )
                break;
        }
    }

    // Patch Link geometries with the intermediate points of LinkCoordinate
    OGRLayer* poLinkLyr = poMemDS->GetLayerByName("Link");
    if( poLinkLyr )
    {
        OGRFeature* poFeat;
        int iLinkID = poLinkLyr->GetLayerDefn()->GetFieldIndex("LINK_ID");
        if( iLinkID >= 0 )
        {
            poLinkLyr->ResetReading();
            OGRSpatialReference* poSRS =
                poLinkLyr->GetLayerDefn()->GetGeomFieldDefn(0)->GetSpatialRef();
            while( (poFeat = poLinkLyr->GetNextFeature()) != NULL )
            {
                int nLinkID  = poFeat->GetFieldAsInteger64(iLinkID);
                std::map<GIntBig, OGRLineString*>::iterator
                    oMapLinkCoordinateIter = oMapLinkCoordinate.find(nLinkID);
                OGRLineString* poLS = (OGRLineString*)poFeat->GetGeometryRef();
                if( poLS != NULL && oMapLinkCoordinateIter != oMapLinkCoordinate.end() )
                {
                    OGRLineString* poLSIntermediate = oMapLinkCoordinateIter->second;
                    OGRLineString* poLSNew = new OGRLineString();
                    poLSNew->addPoint(poLS->getX(0), poLS->getY(0));
                    for(int i=0;i<poLSIntermediate->getNumPoints();i++)
                    {
                        poLSNew->addPoint(poLSIntermediate->getX(i),
                                          poLSIntermediate->getY(i));
                    }
                    poLSNew->addPoint(poLS->getX(1), poLS->getY(1));
                    poLSNew->assignSpatialReference(poSRS);
                    poFeat->SetGeometryDirectly(poLSNew);
                    poLinkLyr->SetFeature(poFeat);
                }
                delete poFeat;
            }
            poLinkLyr->ResetReading();
        }
    }
    
    std::map<GIntBig, OGRLineString*>::iterator oMapLinkCoordinateIter =
                                                    oMapLinkCoordinate.begin();
    for(; oMapLinkCoordinateIter != oMapLinkCoordinate.end(); ++oMapLinkCoordinateIter)
        delete oMapLinkCoordinateIter->second;
}

/************************************************************************/
/*                           GetLayerCount()                            */
/************************************************************************/

int OGRIDFDataSource::GetLayerCount()
{
    if( !bHasParsed )
        Parse();
    if( poMemDS == NULL )
        return 0;
    return poMemDS->GetLayerCount();
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer* OGRIDFDataSource::GetLayer( int iLayer )
{
    if( iLayer < 0 || iLayer >= GetLayerCount() )
        return NULL;
    if( poMemDS == NULL )
        return NULL;
    return poMemDS->GetLayer(iLayer);
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

static int OGRIDFDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    return (poOpenInfo->nHeaderBytes > 0 &&
            strstr((const char*)poOpenInfo->pabyHeader, "\ntbl;") != NULL &&
            strstr((const char*)poOpenInfo->pabyHeader, "\natr;") != NULL &&
            strstr((const char*)poOpenInfo->pabyHeader, "\nfrm;") != NULL);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRIDFDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( !OGRIDFDriverIdentify(poOpenInfo) ||
        poOpenInfo->eAccess == GA_Update || poOpenInfo->fpL == NULL )
    {
        return NULL;
    }

    VSILFILE* fpL = poOpenInfo->fpL;
    poOpenInfo->fpL = NULL;
    return new OGRIDFDataSource(fpL);
}

/************************************************************************/
/*                         RegisterOGRIDF()                             */
/************************************************************************/

void RegisterOGRIDF()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "IDF" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "IDF" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "INTREST Data Format" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_idf.html" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnIdentify = OGRIDFDriverIdentify;
        poDriver->pfnOpen = OGRIDFDriverOpen;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

