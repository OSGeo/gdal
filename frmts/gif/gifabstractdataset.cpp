/******************************************************************************
 * $Id$
 *
 * Project:  GIF Driver
 * Purpose:  GIF Abstract Dataset
 * Author:   Even Rouault <even dot rouault at mines dash paris dot org>
 *
 ****************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines dash paris dot org>
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

#include "gifabstractdataset.h"

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*                         GIFAbstractDataset                           */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                         GIFAbstractDataset()                         */
/************************************************************************/

GIFAbstractDataset::GIFAbstractDataset()

{
    hGifFile = NULL;
    fp = NULL;

    pszProjection = NULL;
    bGeoTransformValid = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    nGCPCount = 0;
    pasGCPList = NULL;

    bHasReadXMPMetadata = FALSE;
}

/************************************************************************/
/*                        ~GIFAbstractDataset()                         */
/************************************************************************/

GIFAbstractDataset::~GIFAbstractDataset()

{
    FlushCache();

    if ( pszProjection )
        CPLFree( pszProjection );

    if ( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

    if( hGifFile )
        DGifCloseFile( hGifFile );

    if( fp != NULL )
        VSIFCloseL( fp );
}


/************************************************************************/
/*                       GIFCollectXMPMetadata()                        */
/************************************************************************/

/* See §2.1.2 of http://wwwimages.adobe.com/www.adobe.com/content/dam/Adobe/en/devnet/xmp/pdfs/XMPSpecificationPart3.pdf */

static CPLString GIFCollectXMPMetadata(VSILFILE* fp)

{
    CPLString osXMP;

    /* Save current position to avoid disturbing GIF stream decoding */
    vsi_l_offset nCurOffset = VSIFTellL(fp);

    char abyBuffer[2048+1];

    VSIFSeekL( fp, 0, SEEK_SET );

    /* Loop over file */

    int iStartSearchOffset = 1024;
    while(TRUE)
    {
        int nRead = VSIFReadL( abyBuffer + 1024, 1, 1024, fp );
        if (nRead <= 0)
            break;
        abyBuffer[1024 + nRead] = 0;

        int i;
        int iFoundOffset = -1;
        for(i=iStartSearchOffset;i<1024+nRead - 14;i++)
        {
            if (memcmp(abyBuffer + i, "\x21\xff\x0bXMP DataXMP", 14) == 0)
            {
                iFoundOffset = i + 14;
                break;
            }
        }

        iStartSearchOffset = 0;

        if (iFoundOffset >= 0)
        {
            int nSize = 1024 + nRead - iFoundOffset;
            char* pszXMP = (char*)VSIMalloc(nSize + 1);
            if (pszXMP == NULL)
                break;

            pszXMP[nSize] = 0;
            memcpy(pszXMP, abyBuffer + iFoundOffset, nSize);

            /* Read from file until we find a NUL character */
            int nLen = (int)strlen(pszXMP);
            while(nLen == nSize)
            {
                char* pszNewXMP = (char*)VSIRealloc(pszXMP, nSize + 1024 + 1);
                if (pszNewXMP == NULL)
                    break;
                pszXMP = pszNewXMP;

                nRead = VSIFReadL( pszXMP + nSize, 1, 1024, fp );
                if (nRead <= 0)
                    break;

                pszXMP[nSize + nRead] = 0;
                nLen += (int)strlen(pszXMP + nSize);
                nSize += nRead;
            }

            if (nLen > 256 && pszXMP[nLen - 1] == '\x01' &&
                pszXMP[nLen - 2] == '\x02' && pszXMP[nLen - 255] == '\xff' &&
                pszXMP[nLen - 256] == '\x01')
            {
                pszXMP[nLen - 256] = 0;

                osXMP = pszXMP;
            }

            VSIFree(pszXMP);

            break;
        }

        if (nRead != 1024)
            break;

        memcpy(abyBuffer, abyBuffer + 1024, 1024);
    }

    VSIFSeekL( fp, nCurOffset, SEEK_SET );

    return osXMP;
}

/************************************************************************/
/*                       CollectXMPMetadata()                           */
/************************************************************************/

void GIFAbstractDataset::CollectXMPMetadata()

{
    if (fp == NULL || bHasReadXMPMetadata)
        return;

    CPLString osXMP = GIFCollectXMPMetadata(fp);
    if (osXMP.size())
    {
        /* Avoid setting the PAM dirty bit just for that */
        int nOldPamFlags = nPamFlags;

        char *apszMDList[2];
        apszMDList[0] = (char*) osXMP.c_str();
        apszMDList[1] = NULL;
        SetMetadata(apszMDList, "xml:XMP");

        nPamFlags = nOldPamFlags;
    }

    bHasReadXMPMetadata = TRUE;
}

/************************************************************************/
/*                           GetMetadata()                              */
/************************************************************************/

char  **GIFAbstractDataset::GetMetadata( const char * pszDomain )
{
    if (fp == NULL)
        return NULL;
    if (eAccess == GA_ReadOnly && !bHasReadXMPMetadata &&
        (pszDomain != NULL && EQUAL(pszDomain, "xml:XMP")))
        CollectXMPMetadata();
    return GDALPamDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                        GetProjectionRef()                            */
/************************************************************************/

const char *GIFAbstractDataset::GetProjectionRef()

{
    if ( pszProjection && bGeoTransformValid )
        return pszProjection;
    else
        return GDALPamDataset::GetProjectionRef();
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GIFAbstractDataset::GetGeoTransform( double * padfTransform )

{
    if( bGeoTransformValid )
    {
        memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );
        return CE_None;
    }
    else
        return GDALPamDataset::GetGeoTransform( padfTransform );
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int GIFAbstractDataset::GetGCPCount()

{
    if (nGCPCount > 0)
        return nGCPCount;
    else
        return GDALPamDataset::GetGCPCount();
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *GIFAbstractDataset::GetGCPProjection()

{
    if ( pszProjection && nGCPCount > 0 )
        return pszProjection;
    else
        return GDALPamDataset::GetGCPProjection();
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *GIFAbstractDataset::GetGCPs()

{
    if (nGCPCount > 0)
        return pasGCPList;
    else
        return GDALPamDataset::GetGCPs();
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int GIFAbstractDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes < 8 )
        return FALSE;

    if( strncmp((const char *) poOpenInfo->pabyHeader, "GIF87a",5) != 0
        && strncmp((const char *) poOpenInfo->pabyHeader, "GIF89a",5) != 0 )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                         DetectGeoreferencing()                       */
/************************************************************************/

void GIFAbstractDataset::DetectGeoreferencing( GDALOpenInfo * poOpenInfo )
{
    bGeoTransformValid =
        GDALReadWorldFile( poOpenInfo->pszFilename, NULL,
                           adfGeoTransform );
    if ( !bGeoTransformValid )
    {
        bGeoTransformValid =
            GDALReadWorldFile( poOpenInfo->pszFilename, ".wld",
                               adfGeoTransform );

        if ( !bGeoTransformValid )
        {
            int bOziFileOK =
                GDALReadOziMapFile( poOpenInfo->pszFilename,
                                    adfGeoTransform,
                                    &pszProjection,
                                    &nGCPCount, &pasGCPList );

            if ( bOziFileOK && nGCPCount == 0 )
                 bGeoTransformValid = TRUE;
        }
    }
}
