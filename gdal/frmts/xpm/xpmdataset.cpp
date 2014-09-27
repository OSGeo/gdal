/******************************************************************************
 * $Id$
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


CPL_CVSID("$Id$");

static unsigned char *ParseXPM( const char *pszInput,
                                int *pnXSize, int *pnYSize, 
                                GDALColorTable **ppoRetTable );


/************************************************************************/
/* ==================================================================== */
/*				XPMDataset				*/
/* ==================================================================== */
/************************************************************************/

class XPMDataset : public GDALPamDataset
{
  public:
                 XPMDataset();
                 ~XPMDataset();

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/*                            XPMDataset()                            */
/************************************************************************/

XPMDataset::XPMDataset()

{
}

/************************************************************************/
/*                            ~XPMDataset()                             */
/************************************************************************/

XPMDataset::~XPMDataset()

{
    FlushCache();
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *XPMDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      First we check to see if the file has the expected header       */
/*      bytes.  For now we expect the XPM file to start with a line     */
/*      containing the letters XPM, and to have "static" in the         */
/*      header.                                                         */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 32 
        || strstr((const char *) poOpenInfo->pabyHeader,"XPM") == NULL 
        || strstr((const char *) poOpenInfo->pabyHeader,"static") == NULL )
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
    unsigned int nFileSize;
    char *pszFileContents;
    VSILFILE *fp = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    if( fp == NULL )
        return NULL;

    VSIFSeekL( fp, 0, SEEK_END );
    nFileSize = (unsigned int) VSIFTellL( fp );
    
    pszFileContents = (char *) VSIMalloc(nFileSize+1);
    if( pszFileContents == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, 
                  "Insufficient memory for loading XPM file %s into memory.", 
                  poOpenInfo->pszFilename );
        VSIFCloseL(fp);
        return NULL;
    }
    pszFileContents[nFileSize] = '\0';
    
    VSIFSeekL( fp, 0, SEEK_SET );

    if( VSIFReadL( pszFileContents, 1, nFileSize, fp ) != nFileSize)
    {
        CPLFree( pszFileContents );
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Failed to read all %d bytes from file %s.",
                  nFileSize, poOpenInfo->pszFilename );
        VSIFCloseL(fp);
        return NULL;
    }
    
    VSIFCloseL(fp);
    fp = NULL;

/* -------------------------------------------------------------------- */
/*      Convert into a binary image.                                    */
/* -------------------------------------------------------------------- */
    GByte *pabyImage;
    int   nXSize, nYSize;
    GDALColorTable *poCT = NULL;

    CPLErrorReset();

    pabyImage = ParseXPM( pszFileContents, &nXSize, &nYSize, &poCT );
    CPLFree( pszFileContents );

    if( pabyImage == NULL )
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    XPMDataset 	*poDS;

    poDS = new XPMDataset();

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    MEMRasterBand *poBand;

    poBand = new MEMRasterBand( poDS, 1, pabyImage, GDT_Byte, 1, nXSize, 
                                TRUE );
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
               CPL_UNUSED GDALDataset *poSrcDS,
               int bStrict,
               CPL_UNUSED char ** papszOptions,
               CPL_UNUSED GDALProgressFunc pfnProgress,
               CPL_UNUSED void * pProgressData )
{
    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();
    GDALColorTable *poCT;

/* -------------------------------------------------------------------- */
/*      Some some rudimentary checks                                    */
/* -------------------------------------------------------------------- */
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
    GDALRasterBand	*poBand = poSrcDS->GetRasterBand(1);
    int                 i;
    GDALColorTable      oGreyTable;

    poCT = poBand->GetColorTable();
    if( poCT == NULL )
    {
        poCT = &oGreyTable;

        for( i = 0; i < 256; i++ )
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
    const char *pszColorCodes = " abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()-+=[]|:;,.<>?/";

    int  anPixelMapping[256];
    GDALColorEntry asPixelColor[256];
    int  nActiveColors = MIN(poCT->GetColorEntryCount(),256);

    // Setup initial colortable and pixel value mapping. 
    memset( anPixelMapping+0, 0, sizeof(int) * 256 );
    for( i = 0; i < nActiveColors; i++ )
    {
        poCT->GetColorEntryAsRGB( i, asPixelColor + i );
        anPixelMapping[i] = i;
    }

/* ==================================================================== */
/*      Iterate merging colors until we are under our limit (about 85). */
/* ==================================================================== */
    while( nActiveColors > (int) strlen(pszColorCodes) )
    {
        int nClosestDistance = 768;
        int iClose1 = -1, iClose2 = -1;
        int iColor1, iColor2;

        // Find the closest pair of colors. 
        for( iColor1 = 0; iColor1 < nActiveColors; iColor1++ )
        {
            for( iColor2 = iColor1+1; iColor2 < nActiveColors; iColor2++ )
            {
                int	nDistance;

                if( asPixelColor[iColor1].c4 < 128 
                    && asPixelColor[iColor2].c4 < 128 )
                    nDistance = 0;
                else
                    nDistance = 
                        ABS(asPixelColor[iColor1].c1-asPixelColor[iColor2].c1)
                      + ABS(asPixelColor[iColor1].c2-asPixelColor[iColor2].c2)
                      + ABS(asPixelColor[iColor1].c3-asPixelColor[iColor2].c3);

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
        for( i = 0; i < 256; i++ )
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
    VSILFILE	*fpPBM;

    fpPBM = VSIFOpenL( pszFilename, "wb+" );
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
    VSIFPrintfL( fpPBM, "/* XPM */\n" );
    VSIFPrintfL( fpPBM, "static char *%s[] = {\n", 
             CPLGetBasename( pszFilename ) );
    VSIFPrintfL( fpPBM, "/* width height num_colors chars_per_pixel */\n" );
    VSIFPrintfL( fpPBM, "\"  %3d   %3d     %3d             1\",\n",
             nXSize, nYSize, nActiveColors );
    VSIFPrintfL( fpPBM, "/* colors */\n" );

/* -------------------------------------------------------------------- */
/*      Write the color table.                                          */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nActiveColors; i++ )
    {
        if( asPixelColor[i].c4 < 128 )
            VSIFPrintfL( fpPBM, "\"%c c None\",\n", pszColorCodes[i] );
        else
            VSIFPrintfL( fpPBM, 
                     "\"%c c #%02x%02x%02x\",\n",
                     pszColorCodes[i],
                     asPixelColor[i].c1, 
                     asPixelColor[i].c2, 
                     asPixelColor[i].c3 );
    }

/* -------------------------------------------------------------------- */
/*	Dump image.							*/
/* -------------------------------------------------------------------- */
    int iLine;
    GByte 	*pabyScanline;

    pabyScanline = (GByte *) CPLMalloc( nXSize );
    for( iLine = 0; iLine < nYSize; iLine++ )
    {
        poBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                          (void *) pabyScanline, nXSize, 1, GDT_Byte, 0, 0 );
        
        VSIFPutcL( '"', fpPBM );
        for( int iPixel = 0; iPixel < nXSize; iPixel++ )
            VSIFPutcL( pszColorCodes[anPixelMapping[pabyScanline[iPixel]]], 
                   fpPBM);
        VSIFPrintfL( fpPBM, "\",\n" );
    }
    
    CPLFree( pabyScanline );

/* -------------------------------------------------------------------- */
/*      cleanup                                                         */
/* -------------------------------------------------------------------- */
    VSIFPrintfL( fpPBM, "};\n" );
    VSIFCloseL( fpPBM );

/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxilary pam information.         */
/* -------------------------------------------------------------------- */
    GDALPamDataset *poDS = (GDALPamDataset *) 
        GDALOpen( pszFilename, GA_ReadOnly );

    if( poDS )
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );

    return poDS;
}

/************************************************************************/
/*                          GDALRegister_XPM()                          */
/************************************************************************/

void GDALRegister_XPM()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "XPM" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "XPM" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "X11 PixMap Format" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#XPM" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "xpm" );
        poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/x-xpixmap" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = XPMDataset::Open;
        poDriver->pfnCreateCopy = XPMCreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

/************************************************************************/
/*                              ParseXPM()                              */
/************************************************************************/

static unsigned char *
ParseXPM( const char *pszInput, int *pnXSize, int *pnYSize, 
          GDALColorTable **ppoRetTable )

{
/* ==================================================================== */
/*      Parse input into an array of strings from within the first C    */
/*      initializer (list os comma separated strings in braces).        */
/* ==================================================================== */
    char **papszXPMList = NULL;
    const char *pszNext = pszInput;
    int  i;

    // Skip till after open brace.
    while( *pszNext != '\0' && *pszNext != '{' ) 
        pszNext++;

    if( *pszNext == '\0' )
        return NULL;

    pszNext++;

    // Read lines till close brace.
    
    while( *pszNext != '\0' && *pszNext != '}' )
    {
        // skip whole comment. 
        if( EQUALN(pszNext,"/*",2) )
        {
            pszNext += 2;
            while( *pszNext != '\0' && !EQUALN(pszNext,"*/",2) )
                pszNext++;
        }

        // reading string constants
        else if( *pszNext == '"' )
        {
            char   *pszLine;

            pszNext++;
            i = 0;

            while( pszNext[i] != '\0' && pszNext[i] != '"' )
                i++;

            if( pszNext[i] == '\0' )
            {
                CSLDestroy( papszXPMList );
                return NULL;
            }

            pszLine = (char *) CPLMalloc(i+1);
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

    if( CSLCount(papszXPMList) < 3 || *pszNext != '}' )
    {
        CSLDestroy( papszXPMList );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Get the image information.                                      */
/* -------------------------------------------------------------------- */
    int nColorCount, nCharsPerPixel;

    if( sscanf( papszXPMList[0], "%d %d %d %d", 
                pnXSize, pnYSize, &nColorCount, &nCharsPerPixel ) != 4 )
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
    int iColor;
    int anCharLookup[256];
    GDALColorTable oCTable;

    for( i = 0; i < 256; i++ ) 
        anCharLookup[i] = -1;

    for( iColor = 0; iColor < nColorCount; iColor++ )
    {
        char **papszTokens = CSLTokenizeString( papszXPMList[iColor+1]+1 );
        GDALColorEntry sColor;
        int            nRed, nGreen, nBlue;

        if( CSLCount(papszTokens) != 2 || !EQUAL(papszTokens[0],"c") )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Ill formed color definition (%s) in XPM header.", 
                      papszXPMList[iColor+1] );
            CSLDestroy( papszXPMList );
            CSLDestroy( papszTokens );
            return NULL;
        }

        anCharLookup[(int)papszXPMList[iColor+1][0]] = iColor;
        
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
            sColor.c1 = (short) nRed;
            sColor.c2 = (short) nGreen;
            sColor.c3 = (short) nBlue;
            sColor.c4 = 255;
        }

        oCTable.SetColorEntry( iColor, &sColor );

        CSLDestroy( papszTokens );
    }

/* -------------------------------------------------------------------- */
/*      Prepare image buffer.                                           */
/* -------------------------------------------------------------------- */
    GByte *pabyImage;

    pabyImage = (GByte *) VSIMalloc2(*pnXSize, *pnYSize);
    if( pabyImage == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, 
                  "Insufficient memory for %dx%d XPM image buffer.", 
                  *pnXSize, *pnYSize );
        CSLDestroy( papszXPMList );
        return NULL;
    }

    memset( pabyImage, 0, *pnXSize * *pnYSize );

/* -------------------------------------------------------------------- */
/*      Parse image.                                                    */
/* -------------------------------------------------------------------- */
    for( int iLine = 0; iLine < *pnYSize; iLine++ )
    {
        const char *pszInLine = papszXPMList[iLine + nColorCount + 1];

        if( pszInLine == NULL )
        {
            CPLFree( pabyImage );
            CSLDestroy( papszXPMList );
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Insufficient imagery lines in XPM image." );
            return NULL;
        }

        for( int iPixel = 0; 
             pszInLine[iPixel] != '\0' && iPixel < *pnXSize; 
             iPixel++ )
        {
            int nPixelValue = anCharLookup[(int)pszInLine[iPixel]];
            if( nPixelValue != -1 )
                pabyImage[iLine * *pnXSize + iPixel] = (GByte) nPixelValue;
        }
    }

    CSLDestroy( papszXPMList );

    *ppoRetTable = oCTable.Clone();

    return pabyImage;
}
