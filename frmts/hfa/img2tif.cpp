/******************************************************************************
 * $Id$
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Mainline for Imagine to TIFF translation.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Intergraph Corporation
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.11  1999/03/15 20:18:29  warmerda
 * Fixed data range function.
 *
 * Revision 1.10  1999/03/15 20:06:36  warmerda
 * Changed TIFF_BuildOverviews() to TIFFBuildOverviews().
 *
 * Revision 1.9  1999/03/11 20:56:34  warmerda
 * Patched up Usage a bit.
 *
 * Revision 1.8  1999/03/08 19:22:31  warmerda
 * Added support to set TIFFTAG_SAMPLEFORMAT.  Removed all byte swapping logic.
 *
 * Revision 1.7  1999/03/02 16:19:06  warmerda
 * Fixed bug with -v list termination, and added support for ``-v 0''.
 *
 * Revision 1.6  1999/03/02 14:18:32  warmerda
 * Fixed bug with writing min/max sample value
 *
 * Revision 1.5  1999/02/15 19:33:05  warmerda
 * Added reporting, and logic to -v 98 and -v 99.
 *
 * Revision 1.3  1999/01/28 16:25:46  warmerda
 * Added compression, usage message, error checking and other cleanup.
 *
 * Revision 1.2  1999/01/27 16:22:19  warmerda
 * Added RGB Support
 *
 * Revision 1.1  1999/01/22 17:45:13  warmerda
 * New
 */

#include "hfa_p.h"

#include "tiffiop.h"
#include "xtiffio.h"
#include <ctype.h>

CPL_C_START
CPLErr ImagineToGeoTIFFProjection( HFAHandle hHFA, TIFF * hTIFF );
CPLErr CopyPyramidsToTiff( HFAHandle, HFABand *, TIFF *, int );
void   TIFFBuildOverviews( const char *, int, int * );
CPL_C_END

static void ImagineToGeoTIFF( HFAHandle, HFABand *, HFABand *, HFABand *,
                              const char *, int, int );
static CPLErr RGBComboValidate( HFAHandle, int, int, int );
static int    ValidateDataType( HFAHandle, int );
static void   ReportOnBand( HFABand * poBand );
static void   ReportOnProjection( HFABand * poBand );

int	gnReportOn = TRUE;

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

void Usage()

{
    printf(
"Usage: img2tif [-i img_filename] [-o tif_basename] [-c] [-v n...]\n"
"       [-rgb [red_band green_band blue_band]] [-?] [-quiet]\n"
"\n"
"Arguments:\n"
"    -i    <input .img file>\n"
"    -o    <output base file name>\n"
"          Output files will be named base_name1.tif ... base_nameN.tif,\n"
"          where N = no. of bands.\n"
"    -rgb  produce an RGB image file from the indicated band numbers\n"
"          within an existing imagine file.\n"
"    -c    packbits compress flag (def=uncompressed)\n"
"    -v    overview sampling increment(s) (0=single, 98=full set minus 2x,\n"
"          99=full set)  Examples: -v 2 4 8   -v 0   -v 99\n"
"    -quiet Don't produce a translation report.\n"
"    -?    Print explanation of command line arguments\n" );
    exit( 1 );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    int		i, nBandCount, nBand, nRed=0, nGreen=0, nBlue=0;
    const char	*pszSrcFilename = NULL;
    const char	*pszDstBasename = NULL;
    HFAHandle   hHFA;
    int		nCompressFlag = COMPRESSION_NONE;
    int		nOverviewCount=0, anOverviews[100];
    int		bDictDump = FALSE, bTreeDump = FALSE;
        
/* -------------------------------------------------------------------- */
/*      Parse commandline options.                                      */
/* -------------------------------------------------------------------- */
    for( i = 1; i < nArgc; i++ )
    {
        if( EQUAL(papszArgv[i],"-i") && i+1 < nArgc )
        {
            pszSrcFilename = papszArgv[i+1];
            i++;
        }
        else if( EQUAL(papszArgv[i],"-o") && i+1 < nArgc )
        {
            pszDstBasename = papszArgv[i+1];
            i++;
        }
        else if( EQUAL(papszArgv[i],"-c") )
        {
            nCompressFlag = COMPRESSION_PACKBITS;
        }
        else if( EQUAL(papszArgv[i],"-v") )
        {
            while( i+1 < nArgc
                   && isdigit(papszArgv[i+1][0]) > 0 )
            {
                anOverviews[nOverviewCount++] = atoi(papszArgv[i+1]);
                i++;
            }
        }
        else if( EQUAL(papszArgv[i],"-quiet") )
        {
            gnReportOn = FALSE;
        }
        else if( EQUAL(papszArgv[i],"-dd") )
        {
            bDictDump = TRUE;
        }
        else if( EQUAL(papszArgv[i],"-dt") )
        {
            bTreeDump = TRUE;
        }
        else if( EQUAL(papszArgv[i],"-rgb") && i+3 < nArgc )
        {
            nRed = atoi(papszArgv[++i]);
            nGreen = atoi(papszArgv[++i]);
            nBlue = atoi(papszArgv[++i]);
        }
        else if( EQUAL(papszArgv[i],"-?") )
        {
            Usage();
        }
        else
        {
            printf( "Unexpected argument: %s\n\n", papszArgv[i] );
            Usage();
        }
    }

    if( pszSrcFilename == NULL )
    {
        printf( "No source file provided.\n\n" );
        Usage();
    }
    
/* -------------------------------------------------------------------- */
/*      Open the imagine file.                                          */
/* -------------------------------------------------------------------- */
    hHFA = HFAOpen( pszSrcFilename, "r" );

    if( hHFA == NULL )
    {
        exit( 100 );
    }

/* -------------------------------------------------------------------- */
/*      Do we want to walk the tree dumping out general information?    */
/* -------------------------------------------------------------------- */
    if( bDictDump )
    {
        HFADumpDictionary( hHFA, stdout );
    }

/* -------------------------------------------------------------------- */
/*      Do we want to walk the tree dumping out general information?    */
/* -------------------------------------------------------------------- */
    if( bTreeDump )
    {
        HFADumpTree( hHFA, stdout );
    }

/* -------------------------------------------------------------------- */
/*      Report general information on the source file.                  */
/* -------------------------------------------------------------------- */
    if( gnReportOn )
    {
        printf( "Imagine file: %s  Raster Size: %dP x %dL x %dB\n",
                pszSrcFilename, hHFA->nXSize, hHFA->nYSize, hHFA->nBands );
    }

/* -------------------------------------------------------------------- */
/*	If the user has requested `98', or `99' for the overviews, 	*/
/*	figure out how many that will be.				*/    
/* -------------------------------------------------------------------- */
    if( nOverviewCount == 1
        && (anOverviews[0] == 98 || anOverviews[0] == 99) )
    {
        int		nXSize = hHFA->nXSize;
        int		nYSize = hHFA->nYSize;
        int		nRes = 2;

        nOverviewCount = 0;
        if( anOverviews[0] == 98 )
        {
            nXSize /= 2;
            nYSize /= 2;
            nRes = 4;
        }
        
        while( nXSize > 30 || nYSize > 30 )
        {
            anOverviews[nOverviewCount++] = nRes;
            nRes = nRes * 2;
            nXSize = nXSize / 2;
            nYSize = nYSize / 2;
        }
    }

/* -------------------------------------------------------------------- */
/*      A zero is translated into the largest integer downsampled       */
/*      overview smaller than 1 million pixels.                         */
/* -------------------------------------------------------------------- */
    if( nOverviewCount == 1 && anOverviews[0] == 0 )
    {
        int		nXSize = hHFA->nXSize/2;
        int		nYSize = hHFA->nYSize/2;
        int		nRes = 2;

        while( nXSize * nYSize > 1000000 )
        {
            nRes += 1;
            nXSize = hHFA->nXSize / nRes;
            nYSize = hHFA->nYSize / nRes;
        }

        if( hHFA->nXSize * hHFA->nYSize < 1000000 )
        {
            nOverviewCount = 0;
        }
        else
        {
            nOverviewCount = 1;
            anOverviews[0] = nRes;
        }
    }

/* -------------------------------------------------------------------- */
/*      If there is no specified destination file, then report a        */
/*      report on the input file.                                       */
/* -------------------------------------------------------------------- */
    if( pszDstBasename == NULL )
    {
        if( !gnReportOn )
            exit( 0 );
        
        for( i = 0; i < hHFA->nBands; i++ )
        {
            printf( "Band %d\n", i+1 );
            ReportOnBand( hHFA->papoBand[i] );
        }
        ReportOnProjection( hHFA->papoBand[0] );
        
        exit( 0 );
    }
    
/* -------------------------------------------------------------------- */
/*      Loop over all bands, generating each TIFF file.                 */
/* -------------------------------------------------------------------- */
    HFAGetRasterInfo( hHFA, NULL, NULL, &nBandCount );

/* -------------------------------------------------------------------- */
/*      Has the user requested an RGB image?                            */
/* -------------------------------------------------------------------- */
    if( nRed > 0 )
    {
        char	szFilename[512];

        if( RGBComboValidate( hHFA, nRed, nGreen, nBlue ) == CE_Failure )
            exit( 1 );

        if( strstr(pszDstBasename,".") == NULL )
            sprintf( szFilename, "%s.tif", pszDstBasename );
        else
            sprintf( szFilename, "%s", pszDstBasename );

        if( gnReportOn )
            printf( "Translating bands %d,%d,%d to an RGB TIFF file %s.\n",
                    nRed, nGreen, nBlue, szFilename );
        
        ImagineToGeoTIFF( hHFA,
                          hHFA->papoBand[nRed-1],
                          hHFA->papoBand[nGreen-1],
                          hHFA->papoBand[nBlue-1],
                          szFilename,
                          nCompressFlag,
                          nOverviewCount == 0 );

        if( nOverviewCount > 0 )
        {
            if( gnReportOn )
                printf( "  Building %d overviews.\n", nOverviewCount );
            
            TIFFBuildOverviews( szFilename, nOverviewCount, anOverviews );
        }
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we translate each band.                               */
/* -------------------------------------------------------------------- */
    else
    {
        for( nBand = 1; nBand <= nBandCount; nBand++ )
        {
            char	szFilename[512];

            if( !ValidateDataType( hHFA, nBand ) )
                continue;

            if( nBandCount == 1 && strstr(pszDstBasename,".tif") != NULL )
                sprintf( szFilename, "%s", pszDstBasename );
            else if( nBandCount == 1 )
                sprintf( szFilename, "%s.tif", pszDstBasename );
            else
                sprintf( szFilename, "%s%d.tif", pszDstBasename, nBand );
        
            if( gnReportOn )
                printf( "Translating band %d to an TIFF file %s.\n",
                        nBand, szFilename );
        
            ImagineToGeoTIFF( hHFA, hHFA->papoBand[nBand-1], NULL, NULL,
                              szFilename, nCompressFlag,
                              nOverviewCount == 0 );

            if( nOverviewCount > 0 )
            {
                if( gnReportOn )
                    printf( "  Building %d overviews.\n", nOverviewCount );
            
                TIFFBuildOverviews( szFilename, nOverviewCount, anOverviews );
            }
        }
    }

    HFAClose( hHFA );

    return 0;
}

/************************************************************************/
/*                            ReportOnBand()                            */
/************************************************************************/

static void ReportOnBand( HFABand * poBand )

{
    HFAEntry		*poBinInfo, *poSubNode;

    printf( "  Data Type: %s   Raster Size: %dx%d\n",
	    poBand->poNode->GetStringField( "pixelType" ),
	    poBand->poNode->GetIntField( "width" ),
	    poBand->poNode->GetIntField( "height" ) );

/* -------------------------------------------------------------------- */
/*      Report min/max                                                  */
/* -------------------------------------------------------------------- */
    poBinInfo = poBand->poNode->GetNamedChild("Statistics" );
    if( poBinInfo != NULL )
    {
        printf( "  Pixel Values - Minimum=%g, Maximum=%g\n",
                poBinInfo->GetDoubleField( "minimum" ),
                poBinInfo->GetDoubleField( "maximum" ) );
    }

/* -------------------------------------------------------------------- */
/*      Report overviews.                                               */
/* -------------------------------------------------------------------- */
    for( poSubNode = poBand->poNode->GetChild();
         poSubNode != NULL;
         poSubNode = poSubNode->GetNext() )
    {
        if( !EQUAL(poSubNode->GetType(),"Eimg_Layer_SubSample") )
            continue;

        printf( "  Overview: %s\n", poSubNode->GetName() );
    }
}

/************************************************************************/
/*                         ReportOnProjection()                         */
/*                                                                      */
/*      Report on the projection of a given band.                       */
/************************************************************************/

static void ReportOnProjection( HFABand * poBand )

{
    HFAEntry		*poDatum, *poProParameters;

    poProParameters = poBand->poNode->GetNamedChild( "Projection" );
    if( poProParameters == NULL )
        return;

    printf( "\n" );
    printf( "  ProjectionName = %s\n",
            poProParameters->GetStringField( "proName" ) );
    printf( "  ProjectionZone = %d\n", 
            poProParameters->GetIntField( "proZone" ) );

    printf( "  Spheroid = %s (major=%.2f, minor=%.2f)\n",
            poProParameters->GetStringField( "proSpheroid.sphereName" ),
            poProParameters->GetDoubleField( "proSpheroid.a" ),
            poProParameters->GetDoubleField( "proSpheroid.b" ) );

/* -------------------------------------------------------------------- */
/*      Report on datum.                                                */
/* -------------------------------------------------------------------- */
    poDatum = poProParameters->GetNamedChild( "Datum" );
    if( poDatum == NULL )
        return;

    printf( "  Datum Name = %s\n",
            poDatum->GetStringField( "datumname" ) );
}

/************************************************************************/
/*                          ValidateDataType()                          */
/*                                                                      */
/*      Will we write this dataset to TIFF?  Some that are              */
/*      considered illegal could be done, but are outside the scope     */
/*      of what Intergraph wants.                                       */
/************************************************************************/

static int ValidateDataType( HFAHandle hHFA, int nBand )

{
    HFABand	*poBand;
    
    poBand = hHFA->papoBand[nBand-1];

    if( poBand->nDataType == EPT_f32 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
      "Band %d is of type `float', and is not supported for translation.\n",
                  nBand );
        return FALSE;
    }
    else if( poBand->nDataType == EPT_f64 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
      "Band %d is of type `double', and is not supported for translation.\n",
                  nBand );
        return FALSE;
    }
    else if( poBand->nDataType == EPT_c128 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
    "Band %d is of type `complex', and is not supported for translation.\n",
                  nBand );
        return FALSE;
    }
    
    return TRUE;
}


/************************************************************************/
/*                      ImagineToGeoTIFFPalette()                       */
/************************************************************************/

static
void ImagineToGeoTIFFPalette( HFABand *poBand, TIFF * hTIFF )

{
    unsigned short	anTRed[256], anTGreen[256], anTBlue[256];
    double	*padfRed, *padfGreen, *padfBlue;
    int		nColors, i;

    poBand->GetPCT( &nColors, &padfRed, &padfGreen, &padfBlue );
    CPLAssert( nColors > 0 );

    for( i = 0; i < 256; i++ )
    {
        if( i < nColors )
        {
            anTRed[i] = (unsigned short) (65535 * padfRed[i]);
            anTGreen[i] = (unsigned short) (65535 * padfGreen[i]);
            anTBlue[i] = (unsigned short) (65535 * padfBlue[i]);
        }
        else
        {
            anTRed[i] = 0;
            anTGreen[i] = 0;
            anTBlue[i] = 0;
        }
    }

    TIFFSetField( hTIFF, TIFFTAG_COLORMAP, anTRed, anTGreen, anTBlue );
}

/************************************************************************/
/*                     ImagineToGeoTIFFDataRange()                      */
/************************************************************************/

static CPLErr ImagineToGeoTIFFDataRange( HFABand * poBand, TIFF *hTIFF)

{
    double		dfMin, dfMax;
    unsigned short	nTMin, nTMax;
    HFAEntry		*poBinInfo;
    
    poBinInfo = poBand->poNode->GetNamedChild("Statistics" );

    if( poBinInfo == NULL )
        return( CE_Failure );

    dfMin = poBinInfo->GetDoubleField( "minimum" );
    dfMax = poBinInfo->GetDoubleField( "maximum" );

    if( dfMax < dfMin )
        return CE_Failure;
    
    if( dfMin < 0 || dfMin > 65536 || dfMax < 0 || dfMax > 65535
        || dfMin >= dfMax )
        return( CE_Failure );

    nTMin = (unsigned short) dfMin;
    nTMax = (unsigned short) dfMax;
    
    TIFFSetField( hTIFF, TIFFTAG_MINSAMPLEVALUE, nTMin );
    TIFFSetField( hTIFF, TIFFTAG_MAXSAMPLEVALUE, nTMax );

    return( CE_None );
}

/************************************************************************/
/*                            CopyOneBand()                             */
/*                                                                      */
/*      copy just the imagery tiles from an Imagine band (full res,     */
/*      or overview) to a sample of a TIFF file.                        */
/************************************************************************/

static CPLErr CopyOneBand( HFABand * poBand, TIFF * hTIFF, int nSample )

{
    void	*pData;
    int		nTileSize;
    
/* -------------------------------------------------------------------- */
/*	Allocate a block buffer.					*/
/* -------------------------------------------------------------------- */
    nTileSize = TIFFTileSize( hTIFF );
    pData = VSIMalloc(nTileSize);
    if( pData == NULL )
    {
        printf( "Out of memory allocating working tile of %d bytes.\n",
                nTileSize );
        return( CE_Failure );
    }

/* -------------------------------------------------------------------- */
/*      Write each of the tiles.                                        */
/* -------------------------------------------------------------------- */
    int		iBlockX, iBlockY;
     
    for( iBlockY = 0; iBlockY < poBand->nBlocksPerColumn; iBlockY++ )
    {
        for( iBlockX = 0; iBlockX < poBand->nBlocksPerRow; iBlockX++ )
        {
            int	iTile;

            if( poBand->GetRasterBlock( iBlockX, iBlockY, pData ) != CE_None )
                return( CE_Failure );

            iTile = TIFFComputeTile( hTIFF,
                                     iBlockX*poBand->nBlockXSize, 
                                     iBlockY*poBand->nBlockYSize,
                                     0, nSample );
            
            if( TIFFWriteEncodedTile( hTIFF, iTile, pData, nTileSize ) < 1 )
                return( CE_Failure );
        }
    }

    VSIFree( pData );

    return( CE_None );
}

/************************************************************************/
/*                          ImagineToGeoTIFF()                          */
/************************************************************************/

static void ImagineToGeoTIFF( HFAHandle hHFA,
                              HFABand * poRedBand,
                              HFABand * poGreenBand,
                              HFABand * poBlueBand,
                              const char * pszDstFilename,
                              int nCompressFlag, int bCopyOverviews )

{
    TIFF	*hTIFF;
    int		nXSize, nYSize, nBlockXSize, nBlockYSize, nDataType;
    int		nBlocksPerRow, nBlocksPerColumn;
    double	*padfRed, *padfGreen, *padfBlue;
    int		nColors;

    HFAGetRasterInfo( hHFA, &nXSize, &nYSize, NULL );

    nDataType = poRedBand->nDataType;
    nBlockXSize = poRedBand->nBlockXSize;
    nBlockYSize = poRedBand->nBlockYSize;
    
/* -------------------------------------------------------------------- */
/*      Verify some conditions of similarity on the bands.  These       */
/*      should be checked before calling this function with a user      */
/*      error.  This is just an extra check.                            */
/* -------------------------------------------------------------------- */

    CPLAssert( poBlueBand == NULL
               || (poBlueBand->nDataType == nDataType
                   && poGreenBand->nDataType == nDataType) );

    CPLAssert( poBlueBand == NULL
               || (poBlueBand->nBlockXSize == nBlockXSize
                   && poGreenBand->nBlockXSize == nBlockXSize
                   && poGreenBand->nBlockYSize == nBlockYSize
                   && poGreenBand->nBlockYSize == nBlockYSize) );

    if( poBlueBand == NULL )
        poRedBand->GetPCT( &nColors, &padfRed, &padfGreen, &padfBlue );
    else
        nColors = 0;

    nBlocksPerRow = (nXSize + nBlockXSize - 1) / nBlockXSize;
    nBlocksPerColumn = (nYSize + nBlockYSize - 1) / nBlockYSize;
    
/* -------------------------------------------------------------------- */
/*      Create the new file.                                            */
/* -------------------------------------------------------------------- */
    hTIFF = XTIFFOpen( pszDstFilename, "w+" );

/* -------------------------------------------------------------------- */
/*      Write standard header fields.                                   */
/* -------------------------------------------------------------------- */
    TIFFSetField( hTIFF, TIFFTAG_IMAGEWIDTH, nXSize );
    TIFFSetField( hTIFF, TIFFTAG_IMAGELENGTH, nYSize );
    TIFFSetField( hTIFF, TIFFTAG_COMPRESSION, nCompressFlag );
    TIFFSetField( hTIFF, TIFFTAG_BITSPERSAMPLE,
                  HFAGetDataTypeBits(nDataType) );

    if( nDataType == EPT_s16 || nDataType == EPT_s8 )
        TIFFSetField( hTIFF, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_INT );

    if( poBlueBand == NULL )
    {
        TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, 1 );
        TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG );
    }
    else
    {
        TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, 3 );
        TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG, PLANARCONFIG_SEPARATE );
    }
        
    TIFFSetField( hTIFF, TIFFTAG_SUBFILETYPE, 0 );

    TIFFSetField( hTIFF, TIFFTAG_TILEWIDTH, nBlockXSize );
    TIFFSetField( hTIFF, TIFFTAG_TILELENGTH, nBlockYSize );

    if( nColors > 0 )
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE );
    else if( poBlueBand == NULL )
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK );
    else
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB );
    
/* -------------------------------------------------------------------- */
/*	Do we have min/max value information?				*/
/* -------------------------------------------------------------------- */
    if( poBlueBand == NULL )
        ImagineToGeoTIFFDataRange( poRedBand, hTIFF );

/* -------------------------------------------------------------------- */
/*      Copy over one, or three bands of raster data.                   */
/* -------------------------------------------------------------------- */
    CopyOneBand( poRedBand, hTIFF, 0 );

    if( poBlueBand != NULL )
    {
        CopyOneBand( poGreenBand, hTIFF, 1 );
        CopyOneBand( poBlueBand, hTIFF, 2 );
    }
    
/* -------------------------------------------------------------------- */
/*      Write Geotiff information.                                      */
/* -------------------------------------------------------------------- */
    ImagineToGeoTIFFProjection( hHFA, hTIFF );

/* -------------------------------------------------------------------- */
/*      Write Palette                                                   */
/* -------------------------------------------------------------------- */
    if( nColors > 0 )
        ImagineToGeoTIFFPalette( poRedBand, hTIFF );

/* -------------------------------------------------------------------- */
/*      Write overviews                                                 */
/* -------------------------------------------------------------------- */
    if( bCopyOverviews )
        CopyPyramidsToTiff( hHFA, poRedBand, hTIFF, nCompressFlag );

    XTIFFClose( hTIFF );
}

/************************************************************************/
/*                              RRD2Tiff()                              */
/*                                                                      */
/*      Copy one reduced resolution layer to a TIFF file.               */
/************************************************************************/

static
CPLErr RRD2Tiff( HFABand * poBand, TIFF * hTIFF,
                 int nPhotometricInterp,
                 int nCompression )

{
    if( poBand->nBlockXSize % 16 != 0 || poBand->nBlockYSize % 16 != 0 )
        return( CE_Failure );

    TIFFWriteDirectory( hTIFF );

    TIFFSetField( hTIFF, TIFFTAG_IMAGEWIDTH, poBand->nWidth );
    TIFFSetField( hTIFF, TIFFTAG_IMAGELENGTH, poBand->nHeight );
    TIFFSetField( hTIFF, TIFFTAG_BITSPERSAMPLE,
                  HFAGetDataTypeBits(poBand->nDataType) );

    TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, 1 );
    TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG );

    TIFFSetField( hTIFF, TIFFTAG_TILEWIDTH, poBand->nBlockXSize );
    TIFFSetField( hTIFF, TIFFTAG_TILELENGTH, poBand->nBlockYSize );

    TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, nPhotometricInterp );
    TIFFSetField( hTIFF, TIFFTAG_COMPRESSION, nCompression );
   
    TIFFSetField( hTIFF, TIFFTAG_SUBFILETYPE, FILETYPE_REDUCEDIMAGE );
   
    return( CopyOneBand( poBand, hTIFF, 0 ) );
}


/************************************************************************/
/*                         CopyPyramidsToTiff()                         */
/*                                                                      */
/*      Copy reduced resolution layers to the TIFF file as              */
/*      overviews.                                                      */
/************************************************************************/

CPLErr CopyPyramidsToTiff( HFAHandle psInfo, HFABand *poBand, TIFF * hTIFF,
                           int nCompressFlag )

{
    HFAEntry	*poBandNode = poBand->poNode;
    HFAEntry	*poSubNode;
    int		nColors, nPhotometric;
    double	*padfRed, *padfGreen, *padfBlue;

    poBand->GetPCT( &nColors, &padfRed, &padfGreen, &padfBlue );
    if( nColors == 0 )
        nPhotometric = PHOTOMETRIC_MINISBLACK;
    else
        nPhotometric = PHOTOMETRIC_PALETTE;

    for( poSubNode = poBandNode->GetChild();
         poSubNode != NULL;
         poSubNode = poSubNode->GetNext() )
    {
        HFABand		*poOverviewBand;
        
        if( !EQUAL(poSubNode->GetType(),"Eimg_Layer_SubSample") )
            continue;

        poOverviewBand = new HFABand( psInfo, poSubNode );
        
        if( RRD2Tiff( poOverviewBand, hTIFF, nPhotometric, nCompressFlag )
						            == CE_None
            && nColors > 0 )
            ImagineToGeoTIFFPalette( poBand, hTIFF );
        
        delete poOverviewBand;
    }

    return CE_None;
}

/************************************************************************/
/*                          RGBComboValidate()                          */
/*                                                                      */
/*      Validate the users selection of band numbers for an RGB         */
/*      image.                                                          */
/************************************************************************/

static CPLErr RGBComboValidate( HFAHandle hHFA,
                                int nRed, int nBlue, int nGreen )

{
    int		nBandCount;
    HFABand     *poRed, *poGreen, *poBlue;
    
    HFAGetRasterInfo( hHFA, NULL, NULL, &nBandCount );

/* -------------------------------------------------------------------- */
/*      Check that band numbers exist.                                  */
/* -------------------------------------------------------------------- */
    if( nRed < 1 || nRed > nBandCount )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Selected red band (%d) not legal.  Only %d bands are "
                  "available.\n", nRed );
        return CE_Failure;
    }

    if( nGreen < 1 || nGreen > nBandCount )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Selected green band (%d) not legal.  Only %d bands are "
                  "available.\n", nGreen );
        return CE_Failure;
    }

    if( nBlue < 1 || nBlue > nBandCount )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Selected blue band (%d) not legal.  Only %d bands are "
                  "available.\n", nBlue );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Verify that all the bands have the same datatype, tile size,    */
/*      and so forth.                                                   */
/* -------------------------------------------------------------------- */
    poRed = hHFA->papoBand[nRed-1];
    poGreen = hHFA->papoBand[nGreen-1];
    poBlue = hHFA->papoBand[nBlue-1];

    if( poRed->nDataType != poGreen->nDataType
        || poRed->nDataType != poBlue->nDataType )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Datatypes of different bands do not match.  They are\n"
                  "%d (red), %d (green), and %d (blue).\n",
                  poRed->nDataType, poGreen->nDataType, poBlue->nDataType );
        return CE_Failure;
    }
    
    if( poRed->nBlockXSize != poGreen->nBlockXSize
        || poRed->nBlockXSize != poBlue->nBlockXSize 
        || poRed->nBlockYSize != poGreen->nBlockYSize
        || poRed->nBlockYSize != poBlue->nBlockYSize )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Tile sizes of different bands do not match.  They are\n"
                  "%dx%d (red), %dx%d (green), and %dx%d (blue).\n",
                  poRed->nBlockXSize, poRed->nBlockYSize,
                  poGreen->nBlockXSize, poGreen->nBlockYSize,
                  poBlue->nBlockXSize, poBlue->nBlockYSize );
        return CE_Failure;
    }
    
/* -------------------------------------------------------------------- */
/*	Verify that each of the bands is legal.				*/
/* -------------------------------------------------------------------- */
    if( !ValidateDataType( hHFA, nRed )
        || !ValidateDataType( hHFA, nGreen )
        || !ValidateDataType( hHFA, nBlue ) )
        return CE_Failure;

    return CE_None;
}
