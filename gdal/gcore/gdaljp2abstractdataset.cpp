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
}

/************************************************************************/
/*                     ~GDALJP2AbstractDataset()                        */
/************************************************************************/

GDALJP2AbstractDataset::~GDALJP2AbstractDataset()
{
}

/************************************************************************/
/*                          LoadJP2Metadata()                           */
/************************************************************************/

void GDALJP2AbstractDataset::LoadJP2Metadata(GDALOpenInfo* poOpenInfo,
                                             const char* pszOverideFilename)
{
    if( pszOverideFilename == NULL )
        pszOverideFilename = poOpenInfo->pszFilename;

/* -------------------------------------------------------------------- */
/*      Check for georeferencing information.                           */
/* -------------------------------------------------------------------- */
    GDALJP2Metadata oJP2Geo;

    if( oJP2Geo.ReadAndParse( pszOverideFilename ) )
    {
        pszProjection = CPLStrdup(oJP2Geo.pszProjection);
        bGeoTransformValid = oJP2Geo.bHaveGeoTransform;
        memcpy( adfGeoTransform, oJP2Geo.adfGeoTransform, 
                sizeof(double) * 6 );
        nGCPCount = oJP2Geo.nGCPCount;
        pasGCPList =
            GDALDuplicateGCPs( oJP2Geo.nGCPCount, oJP2Geo.pasGCPList );
    }

    if (oJP2Geo.pszXMPMetadata)
    {
        char *apszMDList[2];
        apszMDList[0] = (char *) oJP2Geo.pszXMPMetadata;
        apszMDList[1] = NULL;
        GDALPamDataset::SetMetadata(apszMDList, "xml:XMP");
    }

/* -------------------------------------------------------------------- */
/*      Do we have any XML boxes we would like to treat as special      */
/*      domain metadata?                                                */
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

        GDALPamDataset::SetMetadata( apszMDList, osDomain );

        CPLFree( pszName );
    }

/* -------------------------------------------------------------------- */
/*      Do we have other misc metadata?                                 */
/* -------------------------------------------------------------------- */
    if( oJP2Geo.papszMetadata != NULL )
    {
        char **papszMD = CSLDuplicate(GDALPamDataset::GetMetadata());

        papszMD = CSLMerge( papszMD, oJP2Geo.papszMetadata );
        GDALPamDataset::SetMetadata( papszMD );

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
                                poOpenInfo->papszSiblingFiles, NULL )
            || GDALReadWorldFile2( pszOverideFilename, ".wld",
                                   adfGeoTransform,
                                   poOpenInfo->papszSiblingFiles, NULL );
    }
}
