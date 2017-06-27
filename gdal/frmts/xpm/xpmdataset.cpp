/******************************************************************************
 *
 * Project:  XPM Driver
 * Purpose:  Implement GDAL XPM Support
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2008-2010, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdal_pam.h"
#include "cpl_string.h"
#include "memdataset.h"
#include "gdal_frmts.h"

#include <cstdlib>
#include <algorithm>

CPL_CVSID("$Id$")

static unsigned char *ParseXPM( const char *pszInput,
                                unsigned int nFileSize,
                                int *pnXSize, int *pnYSize,
                                GDALColorTable **ppoRetTable );

/************************************************************************/
/* ==================================================================== */
/*                              XPMDataset                              */
/* ==================================================================== */
/************************************************************************/

class XPMDataset : public GDALPamDataset
{
  public:
                 XPMDataset() {}
                 ~XPMDataset();

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
};

/************************************************************************/
/*                            ~XPMDataset()                             */
/************************************************************************/

XPMDataset::~XPMDataset()

{
    FlushCache();
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int XPMDataset::Identify( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      First we check to see if the file has the expected header       */
/*      bytes.  For now we expect the XPM file to start with a line     */
/*      containing the letters XPM, and to have "static" in the         */
/*      header.                                                         */
/* -------------------------------------------------------------------- */
    return poOpenInfo->nHeaderBytes >= 32 &&
           strstr(reinterpret_cast<const char *>( poOpenInfo->pabyHeader ),
                  "XPM") != NULL &&
           strstr(reinterpret_cast<const char *>( poOpenInfo->pabyHeader ),
                  "static") != NULL;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *XPMDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify(poOpenInfo) )
        return NULL;

    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The XPM driver does not support update access to existing"
                  " files." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the whole file into a memory strings.                      */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    if( fp == NULL )
        return NULL;

    if( VSIFSeekL( fp, 0, SEEK_END ) != 0 )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
        return NULL;
    }
    unsigned int nFileSize = static_cast<unsigned int>( VSIFTellL( fp ) );

    char *pszFileContents = reinterpret_cast<char *>( VSI_MALLOC_VERBOSE(nFileSize+1) );
    if( pszFileContents == NULL  )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
        return NULL;
    }
    pszFileContents[nFileSize] = '\0';

    if( VSIFSeekL( fp, 0, SEEK_SET ) != 0 ||
        VSIFReadL( pszFileContents, 1, nFileSize, fp ) != nFileSize)
    {
        CPLFree( pszFileContents );
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read all %d bytes from file %s.",
                  nFileSize, poOpenInfo->pszFilename );
        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
        return NULL;
    }

    CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
    fp = NULL;

/* -------------------------------------------------------------------- */
/*      Convert into a binary image.                                    */
/* -------------------------------------------------------------------- */
    CPLErrorReset();

    int nXSize;
    int nYSize;
    GDALColorTable *poCT = NULL;

    GByte *pabyImage = ParseXPM( pszFileContents, nFileSize, &nXSize, &nYSize, &poCT );

    CPLFree( pszFileContents );

    if( pabyImage == NULL )
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    XPMDataset *poDS = new XPMDataset();

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    MEMRasterBand *poBand = new MEMRasterBand( poDS, 1, pabyImage, GDT_Byte, 1,
                                               nXSize, TRUE );
    poBand->SetColorTable( poCT );
    poDS->SetBand( 1, poBand );

    delete poCT;

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Support overviews.                                              */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return poDS;
}

/************************************************************************/
/*                           XPMCreateCopy()                            */
/************************************************************************/

static GDALDataset *
XPMCreateCopy( const char * pszFilename,
               GDALDataset *poSrcDS,
               int bStrict,
               char ** /* papszOptions */,
               GDALProgressFunc /* pfnProgress */,
               void * /* pProgressData */)
{
/* -------------------------------------------------------------------- */
/*      Some some rudimentary checks                                    */
/* -------------------------------------------------------------------- */
    const int nBands = poSrcDS->GetRasterCount();
    if( nBands != 1 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "XPM driver only supports one band images.\n" );

        return NULL;
    }

    if( poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte
        && bStrict )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "XPM driver doesn't support data type %s. "
                  "Only eight bit bands supported.\n",
                  GDALGetDataTypeName(
                      poSrcDS->GetRasterBand(1)->GetRasterDataType()) );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      If there is no colortable on the source image, create a         */
/*      greyscale one with 64 levels of grey.                           */
/* -------------------------------------------------------------------- */
    GDALRasterBand *poBand = poSrcDS->GetRasterBand(1);

    GDALColorTable oGreyTable;
    GDALColorTable *poCT = poBand->GetColorTable();

    if( poCT == NULL )
    {
        poCT = &oGreyTable;

        for( int i = 0; i < 256; i++ )
        {
            GDALColorEntry sColor;

            sColor.c1 = (short) i;
            sColor.c2 = (short) i;
            sColor.c3 = (short) i;
            sColor.c4 = 255;

            poCT->SetColorEntry( i, &sColor );
        }
    }

/* -------------------------------------------------------------------- */
/*      Build list of active colors, and the mapping from pixels to     */
/*      our active colormap.                                            */
/* -------------------------------------------------------------------- */
    const char *pszColorCodes
        = " abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@"
        "#$%^&*()-+=[]|:;,.<>?/";

    int  anPixelMapping[256];
    GDALColorEntry asPixelColor[256];
    int nActiveColors = std::min(poCT->GetColorEntryCount(),256);

    // Setup initial colortable and pixel value mapping.
    memset( anPixelMapping+0, 0, sizeof(int) * 256 );
    for( int i = 0; i < nActiveColors; i++ )
    {
        poCT->GetColorEntryAsRGB( i, asPixelColor + i );
        anPixelMapping[i] = i;
    }

/* ==================================================================== */
/*      Iterate merging colors until we are under our limit (about 85). */
/* ==================================================================== */
    while( nActiveColors > static_cast<int>( strlen(pszColorCodes) ) )
    {
        int nClosestDistance = 768;
        int iClose1 = -1;
        int iClose2 = -1;

        // Find the closest pair of colors.
        for( int iColor1 = 0; iColor1 < nActiveColors; iColor1++ )
        {
            for( int iColor2 = iColor1+1; iColor2 < nActiveColors; iColor2++ )
            {
                int nDistance;

                if( asPixelColor[iColor1].c4 < 128
                    && asPixelColor[iColor2].c4 < 128 )
                    nDistance = 0;
                else
                    nDistance =
                        std::abs(asPixelColor[iColor1].c1 -
                                 asPixelColor[iColor2].c1)
                        + std::abs(asPixelColor[iColor1].c2 -
                                   asPixelColor[iColor2].c2)
                        + std::abs(asPixelColor[iColor1].c3 -
                                   asPixelColor[iColor2].c3);

                if( nDistance < nClosestDistance )
                {
                    nClosestDistance = nDistance;
                    iClose1 = iColor1;
                    iClose2 = iColor2;
                }
            }

            if( nClosestDistance < 8 )
                break;
        }

        // This should never happen!
        if( iClose1 == -1 )
            break;

        // Merge two selected colors - shift icolor2 into icolor1 and
        // move the last active color into icolor2's slot.
        for( int i = 0; i < 256; i++ )
        {
            if( anPixelMapping[i] == iClose2 )
                anPixelMapping[i] = iClose1;
            else if( anPixelMapping[i] == nActiveColors-1 )
                anPixelMapping[i] = iClose2;
        }

        asPixelColor[iClose2] = asPixelColor[nActiveColors-1];
        nActiveColors--;
    }

/* ==================================================================== */
/*      Write the output image.                                         */
/* ==================================================================== */
    VSILFILE *fpPBM = VSIFOpenL( pszFilename, "wb+" );
    if( fpPBM == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to create file `%s'.",
                  pszFilename );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Write the header lines.                                         */
/* -------------------------------------------------------------------- */
    bool bOK = VSIFPrintfL( fpPBM, "/* XPM */\n" ) >= 0;
    bOK &= VSIFPrintfL( fpPBM, "static char *%s[] = {\n",
             CPLGetBasename( pszFilename ) ) >= 0;
    bOK &= VSIFPrintfL( fpPBM, "/* width height num_colors chars_per_pixel */\n" ) >= 0;

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();

    bOK &= VSIFPrintfL( fpPBM, "\"  %3d   %3d     %3d             1\",\n",
             nXSize, nYSize, nActiveColors ) >= 0;

    bOK &= VSIFPrintfL( fpPBM, "/* colors */\n" ) >= 0;

/* -------------------------------------------------------------------- */
/*      Write the color table.                                          */
/* -------------------------------------------------------------------- */
    for( int i = 0; bOK && i < nActiveColors; i++ )
    {
        if( asPixelColor[i].c4 < 128 )
            bOK &= VSIFPrintfL( fpPBM, "\"%c c None\",\n", pszColorCodes[i] ) >= 0;
        else
            bOK &= VSIFPrintfL( fpPBM,
                     "\"%c c #%02x%02x%02x\",\n",
                     pszColorCodes[i],
                     asPixelColor[i].c1,
                     asPixelColor[i].c2,
                     asPixelColor[i].c3 ) >= 0;
    }

/* -------------------------------------------------------------------- */
/*      Dump image.                                                     */
/* -------------------------------------------------------------------- */
    GByte *pabyScanline = reinterpret_cast<GByte *>( CPLMalloc( nXSize ) );

    for( int iLine = 0; bOK && iLine < nYSize; iLine++ )
    {
        if( poBand->RasterIO(
               GF_Read, 0, iLine, nXSize, 1,
               reinterpret_cast<void *>( pabyScanline), nXSize, 1, GDT_Byte,
               0, 0, NULL ) != CE_None )
        {
            CPLFree( pabyScanline );
            CPL_IGNORE_RET_VAL(VSIFCloseL( fpPBM ));
            return NULL;
        }

        bOK &= VSIFPutcL( '"', fpPBM ) >= 0;
        for( int iPixel = 0; iPixel < nXSize; iPixel++ )
            bOK &= VSIFPutcL( pszColorCodes[anPixelMapping[pabyScanline[iPixel]]],
                   fpPBM) >= 0;
        bOK &= VSIFPrintfL( fpPBM, "\",\n" ) >= 0;
    }

    CPLFree( pabyScanline );

/* -------------------------------------------------------------------- */
/*      cleanup                                                         */
/* -------------------------------------------------------------------- */
    bOK &= VSIFPrintfL( fpPBM, "};\n" ) >= 0;
    if( VSIFCloseL( fpPBM ) != 0 )
        bOK = false;

    if( !bOK )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxiliary pam information.         */
/* -------------------------------------------------------------------- */
    GDALPamDataset *poDS = reinterpret_cast<GDALPamDataset *>(
        GDALOpen( pszFilename, GA_ReadOnly ) );

    if( poDS )
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );

    return poDS;
}

/************************************************************************/
/*                          GDALRegister_XPM()                          */
/************************************************************************/

void GDALRegister_XPM()

{
    if( GDALGetDriverByName( "XPM" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "XPM" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "X11 PixMap Format" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_various.html#XPM" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "xpm" );
    poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/x-xpixmap" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = XPMDataset::Open;
    poDriver->pfnIdentify = XPMDataset::Identify;
    poDriver->pfnCreateCopy = XPMCreateCopy;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}

/************************************************************************/
/*                              ParseXPM()                              */
/************************************************************************/

static unsigned char *
ParseXPM( const char *pszInput,
          unsigned int nFileSize,
          int *pnXSize, int *pnYSize,
          GDALColorTable **ppoRetTable )

{
/* ==================================================================== */
/*      Parse input into an array of strings from within the first C    */
/*      initializer (list os comma separated strings in braces).        */
/* ==================================================================== */
    const char *pszNext = pszInput;

    // Skip till after open brace.
    while( *pszNext != '\0' && *pszNext != '{' )
        pszNext++;

    if( *pszNext == '\0' )
        return NULL;

    pszNext++;

    // Read lines till close brace.

    char **papszXPMList = NULL;
    int  i = 0;

    while( *pszNext != '\0' && *pszNext != '}' )
    {
        // skip whole comment.
        if( STARTS_WITH_CI(pszNext, "/*") )
        {
            pszNext += 2;
            while( *pszNext != '\0' && !STARTS_WITH_CI(pszNext, "*/") )
                pszNext++;
        }

        // reading string constants
        else if( *pszNext == '"' )
        {
            pszNext++;
            i = 0;

            while( pszNext[i] != '\0' && pszNext[i] != '"' )
                i++;

            if( pszNext[i] == '\0' )
            {
                CSLDestroy( papszXPMList );
                return NULL;
            }

            char *pszLine = reinterpret_cast<char *>( CPLMalloc(i+1) );
            strncpy( pszLine, pszNext, i );
            pszLine[i] = '\0';

            papszXPMList = CSLAddString( papszXPMList, pszLine );
            CPLFree( pszLine );
            pszNext = pszNext + i + 1;
        }

        // just ignore everything else (whitespace, commas, newlines, etc).
        else
            pszNext++;
    }

    if( papszXPMList == NULL || CSLCount(papszXPMList) < 3 || *pszNext != '}' )
    {
        CSLDestroy( papszXPMList );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Get the image information.                                      */
/* -------------------------------------------------------------------- */
    int nColorCount, nCharsPerPixel;

    if( sscanf( papszXPMList[0], "%d %d %d %d",
                pnXSize, pnYSize, &nColorCount, &nCharsPerPixel ) != 4 ||
        *pnXSize <= 0 || *pnYSize <= 0 || nColorCount <= 0 || nColorCount > 256 ||
        static_cast<GUIntBig>(*pnXSize) * static_cast<GUIntBig>(*pnYSize) > nFileSize )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Image definition (%s) not well formed.",
                  papszXPMList[0] );
        CSLDestroy( papszXPMList );
        return NULL;
    }

    if( nCharsPerPixel != 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Only one character per pixel XPM images supported by GDAL at this time." );
        CSLDestroy( papszXPMList );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Parse out colors.                                               */
/* -------------------------------------------------------------------- */
    int anCharLookup[256];
    GDALColorTable oCTable;

    for( i = 0; i < 256; i++ )
        anCharLookup[i] = -1;

    for( int iColor = 0; iColor < nColorCount; iColor++ )
    {
        if( papszXPMList[iColor+1] == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Missing color definition for %d in XPM header.",
                      iColor+1 );
            CSLDestroy( papszXPMList );
            return NULL;
        }

        char **papszTokens = CSLTokenizeString( papszXPMList[iColor+1]+1 );

        if( CSLCount(papszTokens) != 2 || !EQUAL(papszTokens[0],"c") )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Ill formed color definition (%s) in XPM header.",
                      papszXPMList[iColor+1] );
            CSLDestroy( papszXPMList );
            CSLDestroy( papszTokens );
            return NULL;
        }

        anCharLookup[*(reinterpret_cast<GByte*>(papszXPMList[iColor+1]))] = iColor;

        GDALColorEntry sColor;
        unsigned int nRed, nGreen, nBlue;

        if( EQUAL(papszTokens[1],"None") )
        {
            sColor.c1 = 0;
            sColor.c2 = 0;
            sColor.c3 = 0;
            sColor.c4 = 0;
        }
        else if( sscanf( papszTokens[1], "#%02x%02x%02x",
                         &nRed, &nGreen, &nBlue ) != 3 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Ill formed color definition (%s) in XPM header.",
                      papszXPMList[iColor+1] );
            CSLDestroy( papszXPMList );
            CSLDestroy( papszTokens );
            return NULL;
        }
        else
        {
            sColor.c1 = static_cast<short>( nRed );
            sColor.c2 = static_cast<short>( nGreen );
            sColor.c3 = static_cast<short>( nBlue );
            sColor.c4 = 255;
        }

        oCTable.SetColorEntry( iColor, &sColor );

        CSLDestroy( papszTokens );
    }

/* -------------------------------------------------------------------- */
/*      Prepare image buffer.                                           */
/* -------------------------------------------------------------------- */
    GByte *pabyImage
        = reinterpret_cast<GByte *>( VSI_CALLOC_VERBOSE(*pnXSize, *pnYSize) );
    if( pabyImage == NULL )
    {
        CSLDestroy( papszXPMList );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Parse image.                                                    */
/* -------------------------------------------------------------------- */
    for( int iLine = 0; iLine < *pnYSize; iLine++ )
    {
        const GByte *pabyInLine = reinterpret_cast<GByte*>(
                                papszXPMList[iLine + nColorCount + 1]);

        if( pabyInLine == NULL )
        {
            CPLFree( pabyImage );
            CSLDestroy( papszXPMList );
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Insufficient imagery lines in XPM image." );
            return NULL;
        }

        for( int iPixel = 0; iPixel < *pnXSize; iPixel++ )
        {
            if( pabyInLine[iPixel] == '\0' )
                break;
            const int nPixelValue
                = anCharLookup[pabyInLine[iPixel]];
            if( nPixelValue != -1 )
                pabyImage[iLine * *pnXSize + iPixel]
                    = static_cast<GByte>( nPixelValue );
        }
    }

    CSLDestroy( papszXPMList );

    *ppoRetTable = oCTable.Clone();

    return pabyImage;
}
