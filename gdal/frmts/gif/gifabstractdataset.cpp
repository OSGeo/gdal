/******************************************************************************
 * $Id$
 *
 * Project:  GIF Driver
 * Purpose:  GIF Abstract Dataset
 * Author:   Even Rouault <even dot rouault at mines dash paris dot org>
 *
 ****************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

static const int InterlacedOffset[] = { 0, 4, 2, 1 }; 
static const int InterlacedJumps[] = { 8, 8, 4, 2 };  

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
        myDGifCloseFile( hGifFile );

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
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **GIFAbstractDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "xml:XMP", NULL);
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
        GDALReadWorldFile2( poOpenInfo->pszFilename, NULL,
                           adfGeoTransform, poOpenInfo->GetSiblingFiles(), NULL );
    if ( !bGeoTransformValid )
    {
        bGeoTransformValid =
            GDALReadWorldFile2( poOpenInfo->pszFilename, ".wld",
                               adfGeoTransform, poOpenInfo->GetSiblingFiles(), NULL );
    }
}

/************************************************************************/
/*                            myDGifOpen()                              */
/************************************************************************/

GifFileType* GIFAbstractDataset::myDGifOpen( void *userPtr, InputFunc readFunc )
{
#if defined(GIFLIB_MAJOR) && GIFLIB_MAJOR >= 5
    int nErrorCode;
    return DGifOpen( userPtr, readFunc, &nErrorCode );
#else
    return DGifOpen( userPtr, readFunc );
#endif
}

/************************************************************************/
/*                          myDGifCloseFile()                           */
/************************************************************************/

int GIFAbstractDataset::myDGifCloseFile( GifFileType *hGifFile )
{
#if defined(GIFLIB_MAJOR) && ((GIFLIB_MAJOR == 5 && GIFLIB_MINOR >= 1) || GIFLIB_MAJOR > 5)
    int nErrorCode;
    return DGifCloseFile( hGifFile, &nErrorCode );
#else
    return DGifCloseFile( hGifFile );
#endif
}

/************************************************************************/
/*                          myEGifCloseFile()                           */
/************************************************************************/

int GIFAbstractDataset::myEGifCloseFile( GifFileType *hGifFile )
{
#if defined(GIFLIB_MAJOR) && ((GIFLIB_MAJOR == 5 && GIFLIB_MINOR >= 1) || GIFLIB_MAJOR > 5)
    int nErrorCode;
    return EGifCloseFile( hGifFile, &nErrorCode );
#else
    return EGifCloseFile( hGifFile );
#endif
}

/************************************************************************/
/*                           VSIGIFReadFunc()                           */
/*                                                                      */
/*      Proxy function for reading from GIF file.                       */
/************************************************************************/

int GIFAbstractDataset::ReadFunc( GifFileType *psGFile, GifByteType *pabyBuffer, 
                                        int nBytesToRead )

{
    return VSIFReadL( pabyBuffer, 1, nBytesToRead, 
                      (VSILFILE *) psGFile->UserData );
}

/************************************************************************/
/*                        GIFAbstractRasterBand()                       */
/************************************************************************/

GIFAbstractRasterBand::GIFAbstractRasterBand(
                              GIFAbstractDataset *poDS, int nBand, 
                              SavedImage *psSavedImage, int nBackground,
                              int bAdvertizeInterlacedMDI )

{
    this->poDS = poDS;
    this->nBand = nBand;

    eDataType = GDT_Byte;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

    psImage = psSavedImage;

    poColorTable = NULL;
    panInterlaceMap = NULL;
    nTransparentColor = 0;

    if (psImage == NULL)
        return;

/* -------------------------------------------------------------------- */
/*      Setup interlacing map if required.                              */
/* -------------------------------------------------------------------- */
    panInterlaceMap = NULL;
    if( psImage->ImageDesc.Interlace )
    {
        int     i, j, iLine = 0;
        
        if( bAdvertizeInterlacedMDI )
            poDS->SetMetadataItem( "INTERLACED", "YES", "IMAGE_STRUCTURE" );

        panInterlaceMap = (int *) CPLCalloc(poDS->nRasterYSize,sizeof(int));

        for (i = 0; i < 4; i++)
        {
            for (j = InterlacedOffset[i]; 
                 j < poDS->nRasterYSize;
                 j += InterlacedJumps[i]) 
                panInterlaceMap[j] = iLine++;
        }
    }
    else if( bAdvertizeInterlacedMDI )
        poDS->SetMetadataItem( "INTERLACED", "NO", "IMAGE_STRUCTURE" );

/* -------------------------------------------------------------------- */
/*      Check for transparency.  We just take the first graphic         */
/*      control extension block we find, if any.                        */
/* -------------------------------------------------------------------- */
    int iExtBlock;

    nTransparentColor = -1;
    for( iExtBlock = 0; iExtBlock < psImage->ExtensionBlockCount; iExtBlock++ )
    {
        unsigned char *pExtData;

        if( psImage->ExtensionBlocks[iExtBlock].Function != 0xf9 )
            continue;

        pExtData = (unsigned char *) psImage->ExtensionBlocks[iExtBlock].Bytes;

        /* check if transparent color flag is set */
        if( !(pExtData[0] & 0x1) )
            continue;

        nTransparentColor = pExtData[3];
    }

/* -------------------------------------------------------------------- */
/*      Setup colormap.                                                 */
/* -------------------------------------------------------------------- */
    ColorMapObject      *psGifCT = psImage->ImageDesc.ColorMap;
    if( psGifCT == NULL )
        psGifCT = poDS->hGifFile->SColorMap;

    poColorTable = new GDALColorTable();
    for( int iColor = 0; iColor < psGifCT->ColorCount; iColor++ )
    {
        GDALColorEntry  oEntry;

        oEntry.c1 = psGifCT->Colors[iColor].Red;
        oEntry.c2 = psGifCT->Colors[iColor].Green;
        oEntry.c3 = psGifCT->Colors[iColor].Blue;

        if( iColor == nTransparentColor )
            oEntry.c4 = 0;
        else
            oEntry.c4 = 255;

        poColorTable->SetColorEntry( iColor, &oEntry );
    }

/* -------------------------------------------------------------------- */
/*      If we have a background value, return it here.  Some            */
/*      applications might want to treat this as transparent, but in    */
/*      many uses this is inappropriate so we don't return it as        */
/*      nodata or transparent.                                          */
/* -------------------------------------------------------------------- */
    if( nBackground != 255 )
    {
        char szBackground[10];
        
        sprintf( szBackground, "%d", nBackground );
        SetMetadataItem( "GIF_BACKGROUND", szBackground );
    }
}

/************************************************************************/
/*                       ~GIFAbstractRasterBand()                       */
/************************************************************************/

GIFAbstractRasterBand::~GIFAbstractRasterBand()

{
    if( poColorTable != NULL )
        delete poColorTable;

    CPLFree( panInterlaceMap );
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp GIFAbstractRasterBand::GetColorInterpretation()

{
    return GCI_PaletteIndex;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *GIFAbstractRasterBand::GetColorTable()

{
    return poColorTable;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double GIFAbstractRasterBand::GetNoDataValue( int *pbSuccess )

{
    if( pbSuccess != NULL )
        *pbSuccess = nTransparentColor != -1;

    return nTransparentColor;
}
