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

/************************************************************************/
/*                     GDALJP2AbstractDataset()                         */
/************************************************************************/

GDALJP2AbstractDataset::GDALJP2AbstractDataset()
{
    pszWldFilename = NULL;
}

/************************************************************************/
/*                     ~GDALJP2AbstractDataset()                        */
/************************************************************************/

GDALJP2AbstractDataset::~GDALJP2AbstractDataset()
{
    CPLFree(pszWldFilename);
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
