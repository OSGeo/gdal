/******************************************************************************
 * $Id$
 *
 * Project:  GDAL 
 * Purpose:  GDALGeorefPamDataset with helper to read georeferencing and other
 *           metadata from JP2Boxes
 * Author:   Even Rouault <even dot rouault at mines-paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdaljp2abstractdataset.h"
#include "gdaljp2metadata.h"
#include "ogrsf_frmts.h"

/************************************************************************/
/*                     GDALJP2AbstractDataset()                         */
/************************************************************************/

GDALJP2AbstractDataset::GDALJP2AbstractDataset()
{
    pszWldFilename = NULL;
    poMemDS = NULL;
}

/************************************************************************/
/*                     ~GDALJP2AbstractDataset()                        */
/************************************************************************/

GDALJP2AbstractDataset::~GDALJP2AbstractDataset()
{
    CPLFree(pszWldFilename);
    CloseDependentDatasets();
}

/************************************************************************/
/*                      CloseDependentDatasets()                        */
/************************************************************************/

int GDALJP2AbstractDataset::CloseDependentDatasets()
{
    int bRet = GDALGeorefPamDataset::CloseDependentDatasets();
    if( poMemDS )
    {
        GDALClose(poMemDS);
        poMemDS = NULL;
        bRet = TRUE;
    }
    return bRet;
}

/************************************************************************/
/*                          LoadJP2Metadata()                           */
/************************************************************************/

void GDALJP2AbstractDataset::LoadJP2Metadata(GDALOpenInfo* poOpenInfo,
                                             const char* pszOverideFilenameIn)
{
    const char* pszOverideFilename = pszOverideFilenameIn;
    if( pszOverideFilename == NULL )
        pszOverideFilename = poOpenInfo->pszFilename;

/* -------------------------------------------------------------------- */
/*      Check for georeferencing information.                           */
/* -------------------------------------------------------------------- */
    GDALJP2Metadata oJP2Geo;

    if( (poOpenInfo->fpL != NULL && pszOverideFilenameIn == NULL &&
         oJP2Geo.ReadAndParse(poOpenInfo->fpL) ) ||
        (!(poOpenInfo->fpL != NULL && pszOverideFilenameIn == NULL) &&
         oJP2Geo.ReadAndParse( pszOverideFilename )) )
    {
        CPLFree(pszProjection);
        pszProjection = CPLStrdup(oJP2Geo.pszProjection);
        bGeoTransformValid = oJP2Geo.bHaveGeoTransform;
        memcpy( adfGeoTransform, oJP2Geo.adfGeoTransform, 
                sizeof(double) * 6 );
        nGCPCount = oJP2Geo.nGCPCount;
        pasGCPList =
            GDALDuplicateGCPs( oJP2Geo.nGCPCount, oJP2Geo.pasGCPList );

        if( oJP2Geo.bPixelIsPoint )
            GDALDataset::SetMetadataItem(GDALMD_AREA_OR_POINT, GDALMD_AOP_POINT);
        if( oJP2Geo.papszRPCMD )
            GDALDataset::SetMetadata( oJP2Geo.papszRPCMD, "RPC" );
    }

/* -------------------------------------------------------------------- */
/*      Report XML UUID box in a dedicated metadata domain              */
/* -------------------------------------------------------------------- */
    if (oJP2Geo.pszXMPMetadata)
    {
        char *apszMDList[2];
        apszMDList[0] = (char *) oJP2Geo.pszXMPMetadata;
        apszMDList[1] = NULL;
        GDALDataset::SetMetadata(apszMDList, "xml:XMP");
    }

/* -------------------------------------------------------------------- */
/*      Do we have any XML boxes we would like to treat as special      */
/*      domain metadata? (Note: the GDAL multidomain metadata XML box   */
/*      has been excluded and is dealt a few lines below.               */
/* -------------------------------------------------------------------- */
    int iBox;

    for( iBox = 0; 
            oJP2Geo.papszGMLMetadata
                && oJP2Geo.papszGMLMetadata[iBox] != NULL; 
            iBox++ )
    {
        char *pszName = NULL;
        const char *pszXML = 
            CPLParseNameValue( oJP2Geo.papszGMLMetadata[iBox], 
                                &pszName );
        CPLString osDomain;
        char *apszMDList[2];

        osDomain.Printf( "xml:%s", pszName );
        apszMDList[0] = (char *) pszXML;
        apszMDList[1] = NULL;

        GDALDataset::SetMetadata( apszMDList, osDomain );

        CPLFree( pszName );
    }

/* -------------------------------------------------------------------- */
/*      Do we have GDAL metadata?                                       */
/* -------------------------------------------------------------------- */
    if( oJP2Geo.pszGDALMultiDomainMetadata != NULL )
    {
        CPLXMLNode* psXMLNode = CPLParseXMLString(oJP2Geo.pszGDALMultiDomainMetadata);
        if( psXMLNode )
        {
            GDALMultiDomainMetadata oLocalMDMD;
            oLocalMDMD.XMLInit(psXMLNode, FALSE);
            char** papszDomainList = oLocalMDMD.GetDomainList();
            char** papszIter = papszDomainList;
            GDALDataset::SetMetadata(oLocalMDMD.GetMetadata());
            while( papszIter && *papszIter )
            {
                if( !EQUAL(*papszIter, "") && !EQUAL(*papszIter, "IMAGE_STRUCTURE") )
                {
                    if( GDALDataset::GetMetadata(*papszIter) != NULL )
                    {
                        CPLDebug("GDALJP2",
                                 "GDAL metadata overrides metadata in %s domain over metadata read from other boxes",
                                 *papszIter);
                    }
                    GDALDataset::SetMetadata(oLocalMDMD.GetMetadata(*papszIter), *papszIter);
                }
                papszIter ++;
            }
            CPLDestroyXMLNode(psXMLNode);
        }
        else
            CPLErrorReset();
    }

/* -------------------------------------------------------------------- */
/*      Do we have other misc metadata (from resd box for now) ?        */
/* -------------------------------------------------------------------- */
    if( oJP2Geo.papszMetadata != NULL )
    {
        char **papszMD = CSLDuplicate(GDALDataset::GetMetadata());

        papszMD = CSLMerge( papszMD, oJP2Geo.papszMetadata );
        GDALDataset::SetMetadata( papszMD );

        CSLDestroy( papszMD );
    }

/* -------------------------------------------------------------------- */
/*      Do we have XML IPR ?                                            */
/* -------------------------------------------------------------------- */
    if( oJP2Geo.pszXMLIPR != NULL )
    {
        char* apszMD[2] = { NULL, NULL };
        apszMD[0] = oJP2Geo.pszXMLIPR;
        GDALDataset::SetMetadata( apszMD, "xml:IPR" );
    }

/* -------------------------------------------------------------------- */
/*      Check for world file.                                           */
/* -------------------------------------------------------------------- */
    if( !bGeoTransformValid )
    {
        bGeoTransformValid |=
            GDALReadWorldFile2( pszOverideFilename, NULL,
                                adfGeoTransform,
                                poOpenInfo->GetSiblingFiles(), &pszWldFilename )
            || GDALReadWorldFile2( pszOverideFilename, ".wld",
                                   adfGeoTransform,
                                   poOpenInfo->GetSiblingFiles(), &pszWldFilename );
    }
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **GDALJP2AbstractDataset::GetFileList()

{
    char **papszFileList = GDALGeorefPamDataset::GetFileList();

    if( pszWldFilename != NULL &&
        CSLFindString( papszFileList, pszWldFilename ) == -1 )
    {
        papszFileList = CSLAddString( papszFileList, pszWldFilename );
    }
    return papszFileList;
}

/************************************************************************/
/*                        LoadVectorLayers()                            */
/************************************************************************/

void GDALJP2AbstractDataset::LoadVectorLayers(int bOpenRemoteResources)
{
    char** papszGMLJP2 = GetMetadata("xml:gml.root-instance");
    if( papszGMLJP2 == NULL )
        return;
    GDALDriver* poMemDriver = (GDALDriver*)GDALGetDriverByName("Memory");
    if( poMemDriver == NULL )
        return;
    CPLXMLNode* psRoot = CPLParseXMLString(papszGMLJP2[0]);
    if( psRoot == NULL )
        return;
    CPLXMLNode* psCC = CPLGetXMLNode(psRoot, "=gmljp2:GMLJP2CoverageCollection");
    if( psCC == NULL )
    {
        CPLDestroyXMLNode(psRoot);
        return;
    }

    // Find feature collections
    CPLXMLNode* psCCChildIter = psCC->psChild;
    int nLayersAtCC = 0;
    int nLayersAtGC = 0;
    for( ; psCCChildIter != NULL; psCCChildIter = psCCChildIter->psNext )
    {
        if( psCCChildIter->eType != CXT_Element ||
            strcmp(psCCChildIter->pszValue, "gmljp2:featureMember") != 0 ||
            psCCChildIter->psChild == NULL ||
            psCCChildIter->psChild->eType != CXT_Element )
            continue;

        CPLXMLNode* psGCorGMLJP2Features = psCCChildIter->psChild;
        int bIsGC = ( strstr(psGCorGMLJP2Features->pszValue, "GridCoverage") != NULL );

        CPLXMLNode* psGCorGMLJP2FeaturesChildIter = psGCorGMLJP2Features->psChild;
        for( ; psGCorGMLJP2FeaturesChildIter != NULL;
               psGCorGMLJP2FeaturesChildIter = psGCorGMLJP2FeaturesChildIter->psNext )
        {
            if( psGCorGMLJP2FeaturesChildIter->eType != CXT_Element ||
                strcmp(psGCorGMLJP2FeaturesChildIter->pszValue, "gmljp2:feature") != 0 ||
                psGCorGMLJP2FeaturesChildIter->psChild == NULL )
                continue;

            CPLXMLNode* psFC = NULL;
            int bFreeFC = FALSE;
            CPLString osGMLTmpFile;

            CPLXMLNode* psChild = psGCorGMLJP2FeaturesChildIter->psChild;
            if( psChild->eType == CXT_Attribute &&
                strcmp(psChild->pszValue, "xlink:href") == 0 &&
                strncmp(psChild->psChild->pszValue,
                        "gmljp2://xml/", strlen("gmljp2://xml/")) == 0 )
            {
                const char* pszBoxName = psChild->psChild->pszValue + strlen("gmljp2://xml/");
                char** papszBoxData = GetMetadata(CPLSPrintf("xml:%s", pszBoxName));
                if( papszBoxData != NULL )
                {
                    psFC = CPLParseXMLString(papszBoxData[0]);
                    bFreeFC = TRUE;
                }
                else
                {
                    CPLDebug("GMLJP2",
                             "gmljp2:feature references %s, but no corresponding box found",
                             psChild->psChild->pszValue);
                }
            }
            if( psChild->eType == CXT_Attribute &&
                strcmp(psChild->pszValue, "xlink:href") == 0 &&
                (strncmp(psChild->psChild->pszValue, "http://", strlen("http://")) == 0 ||
                 strncmp(psChild->psChild->pszValue, "https://", strlen("https://")) == 0) )
            {
                if( !bOpenRemoteResources )
                    CPLDebug("GMLJP2", "Remote feature collection %s mentionned in GMLJP2 box",
                             psChild->psChild->pszValue);
                else
                    osGMLTmpFile = "/vsicurl/" + CPLString(psChild->psChild->pszValue);
            }
            else if( psChild->eType == CXT_Element &&
                     strstr(psChild->pszValue, "FeatureCollection") != NULL )
            {
                psFC = psChild;
            }
            if( psFC == NULL && osGMLTmpFile.size() == 0 )
                continue;

            if( psFC != NULL )
            {
                osGMLTmpFile = CPLSPrintf("/vsimem/gmljp2/%p/my.gml", this);
                // Create temporary .gml file
                CPLSerializeXMLTreeToFile(psFC, osGMLTmpFile);
            }

            CPLDebug("GMLJP2", "Found a FeatureCollection at %s level",
                     (bIsGC) ? "GridCoverage" : "CoverageCollection");

            CPLString osXSDTmpFile;

            if( psFC )
            {
                // Try to localize its .xsd schema in a GMLJP2 auxiliary box
                const char* pszSchemaLocation = CPLGetXMLValue(psFC, "xsi:schemaLocation", NULL);
                if( pszSchemaLocation )
                {
                    char **papszTokens = CSLTokenizeString2(
                            pszSchemaLocation, " \t\n", 
                            CSLT_HONOURSTRINGS | CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES);

                    if( (CSLCount(papszTokens) % 2) == 0 )
                    {
                        for(char** papszIter = papszTokens; *papszIter; papszIter += 2 )
                        {
                            if( strncmp(papszIter[1], "gmljp2://xml/", strlen("gmljp2://xml/")) == 0 )
                            {
                                const char* pszBoxName = papszIter[1] + strlen("gmljp2://xml/");
                                char** papszBoxData = GetMetadata(CPLSPrintf("xml:%s", pszBoxName));
                                if( papszBoxData != NULL )
                                {
                                    osXSDTmpFile = CPLSPrintf("/vsimem/gmljp2/%p/my.xsd", this);
                                    VSIFCloseL(VSIFileFromMemBuffer(osXSDTmpFile,
                                                                    (GByte*)papszBoxData[0],
                                                                    strlen(papszBoxData[0]),
                                                                    FALSE));
                                }
                                else
                                {
                                    CPLDebug("GMLJP2",
                                            "Feature collection references %s, but no corresponding box found",
                                            papszIter[1]);
                                }
                                break;
                            }
                        }
                    }
                    CSLDestroy(papszTokens);
                }
                if( bFreeFC )
                {
                    CPLDestroyXMLNode(psFC);
                    psFC = NULL;
                }
            }

            GDALDriverH hDrv = GDALIdentifyDriver(osGMLTmpFile, NULL);
            GDALDriverH hGMLDrv = GDALGetDriverByName("GML");
            if( hDrv != NULL && hDrv == hGMLDrv )
            {
                char* apszOpenOptions[2];
                apszOpenOptions[0] = (char*) "FORCE_SRS_DETECTION=YES";
                apszOpenOptions[1] = NULL;
                GDALDataset* poTmpDS = (GDALDataset*)GDALOpenEx( osGMLTmpFile,
                                        GDAL_OF_VECTOR, NULL, apszOpenOptions, NULL );
                if( poTmpDS )
                {
                    int nLayers = poTmpDS->GetLayerCount();
                    for(int i=0;i<nLayers;i++)
                    {
                        if( poMemDS == NULL )
                            poMemDS = poMemDriver->Create("", 0, 0, 0, GDT_Unknown, NULL);
                        OGRLayer* poSrcLyr = poTmpDS->GetLayer(i);
                        const char* pszLayerName;
                        if( bIsGC )
                            pszLayerName = CPLSPrintf("FC_GridCoverage_%d_%s",
                                                    ++nLayersAtGC, poSrcLyr->GetName());
                        else
                            pszLayerName = CPLSPrintf("FC_CoverageCollection_%d_%s",
                                                    ++nLayersAtCC, poSrcLyr->GetName());
                        poMemDS->CopyLayer(poSrcLyr, pszLayerName, NULL);
                    }
                    GDALClose(poTmpDS);

                    // In case we don't have a schema, a .gfs might have been generated
                    VSIUnlink(CPLSPrintf("/vsimem/gmljp2/%p/my.gfs", this));
                }
            }
            else
            {
                CPLDebug("GMLJP2", "No GML driver found to read feature collection");
            }

            if( strncmp(osGMLTmpFile, "/vsicurl/", strlen("/vsicurl/")) != 0 )
                VSIUnlink(osGMLTmpFile);
            if( osXSDTmpFile.size() )
                VSIUnlink(osXSDTmpFile);
        }
    }

    // Find annotations
    psCCChildIter = psCC->psChild;
    int nAnnotations = 0;
    for( ; psCCChildIter != NULL; psCCChildIter = psCCChildIter->psNext )
    {
        if( psCCChildIter->eType != CXT_Element ||
            strcmp(psCCChildIter->pszValue, "gmljp2:featureMember") != 0 ||
            psCCChildIter->psChild == NULL ||
            psCCChildIter->psChild->eType != CXT_Element )
            continue;
        CPLXMLNode* psGCorGMLJP2Features = psCCChildIter->psChild;
        int bIsGC = ( strstr(psGCorGMLJP2Features->pszValue, "GridCoverage") != NULL );
        if( !bIsGC )
            continue;
        CPLXMLNode* psGCorGMLJP2FeaturesChildIter = psGCorGMLJP2Features->psChild;
        for( ; psGCorGMLJP2FeaturesChildIter != NULL;
               psGCorGMLJP2FeaturesChildIter = psGCorGMLJP2FeaturesChildIter->psNext )
        {
            if( psGCorGMLJP2FeaturesChildIter->eType != CXT_Element ||
                strcmp(psGCorGMLJP2FeaturesChildIter->pszValue, "gmljp2:annotation") != 0 ||
                psGCorGMLJP2FeaturesChildIter->psChild == NULL ||
                psGCorGMLJP2FeaturesChildIter->psChild->eType != CXT_Element ||
                strstr(psGCorGMLJP2FeaturesChildIter->psChild->pszValue, "kml") == NULL )
                continue;
            CPLDebug("GMLJP2", "Found a KML annotation");

            // Create temporary .kml file
            CPLXMLNode* psKML = psGCorGMLJP2FeaturesChildIter->psChild;
            CPLString osKMLTmpFile(CPLSPrintf("/vsimem/gmljp2/%p/my.kml", this));
            CPLSerializeXMLTreeToFile(psKML, osKMLTmpFile);

            GDALDataset* poTmpDS = (GDALDataset*)GDALOpenEx( osKMLTmpFile,
                                    GDAL_OF_VECTOR, NULL, NULL, NULL );
            if( poTmpDS )
            {
                int nLayers = poTmpDS->GetLayerCount();
                for(int i=0;i<nLayers;i++)
                {
                    if( poMemDS == NULL )
                        poMemDS = poMemDriver->Create("", 0, 0, 0, GDT_Unknown, NULL);
                    OGRLayer* poSrcLyr = poTmpDS->GetLayer(i);
                    const char* pszLayerName;
                    pszLayerName = CPLSPrintf("Annotation_%d_%s",
                                                ++nAnnotations, poSrcLyr->GetName());
                    poMemDS->CopyLayer(poSrcLyr, pszLayerName, NULL);
                }
                GDALClose(poTmpDS);
            }
            else
            {
                CPLDebug("GMLJP2", "No KML/LIBKML driver found to read annotation");
            }

            VSIUnlink(osKMLTmpFile);
        }
    }

    CPLDestroyXMLNode(psRoot);
}

/************************************************************************/
/*                           GetLayerCount()                            */
/************************************************************************/

int GDALJP2AbstractDataset::GetLayerCount()
{
    return (poMemDS != NULL) ? poMemDS->GetLayerCount() : 0;
}

/************************************************************************/
/*                             GetLayer()                               */
/************************************************************************/

OGRLayer* GDALJP2AbstractDataset::GetLayer(int i)
{
    return (poMemDS != NULL) ? poMemDS->GetLayer(i) : NULL;
}
