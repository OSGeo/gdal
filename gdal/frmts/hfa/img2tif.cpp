/******************************************************************************
 * $Id$
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Mainline for Imagine to TIFF translation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 ****************************************************************************/

#include "hfa_p.h"

#include "tiffiop.h"
#include "xtiffio.h"
#include <ctype.h>
#include <assert.h>

CPL_CVSID("$Id$");

CPL_C_START
CPLErr ImagineToGeoTIFFProjection( HFAHandle hHFA, TIFF * hTIFF );
CPLErr CopyPyramidsToTiff( HFAHandle, HFABand *, TIFF *, int );
void   TIFFBuildOverviews( const char *, int, int * );
CPL_C_END

static void ImagineToGeoTIFF( HFAHandle, HFABand **, int, int *,
                              const char *, int, int, int );
static CPLErr RGBComboValidate( HFAHandle, HFABand **, int, int * );
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
"    -s    output file is in strips (tiles is default)\n"
"    -c    packbits compress flag (def=uncompressed)\n"
"    -v    overview sampling increment(s) (0=single, 98=full set minus 2x,\n"
"          99=full set)  Examples: -v 2 4 8   -v 0   -v 99\n"
"    -quiet Don't produce a translation report.\n"
"    -?    Print this explanation of command line arguments\n"
"\n"
"Visit http://gdal.velocet.ca/projects/imagine/hfa_index.html for more info.\n"
"\n"
"Author: Frank Warmerdam (warmerdam@pobox.com)\n"
"Special thanks to Intergraph Corporation for funding this project\n" );
    exit( 1 );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    int		i, nHFABandCount, nBand;
    const char	*pszSrcFilename = NULL;
    const char	*pszDstBasename = NULL;
    HFAHandle   hHFA;
    int		nCompressFlag = COMPRESSION_NONE;
    int		nOverviewCount=0, anOverviews[100];
    int		bDictDump = FALSE, bTreeDump = FALSE, bWriteInStrips = FALSE;
    int		nBandCount=0, anBandList[512];
        
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
        else if( EQUAL(papszArgv[i],"-s") )
        {
            bWriteInStrips = TRUE;
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
            nBandCount = 3;
            anBandList[0] = atoi(papszArgv[++i]) - 1;
            anBandList[1] = atoi(papszArgv[++i]) - 1;
            anBandList[2] = atoi(papszArgv[++i]) - 1;
        }
        else if( EQUAL(papszArgv[i],"-rgbn") && i+3 < nArgc )
        {
            while( i+1 < nArgc && atoi(papszArgv[i+1]) > 0 )
                anBandList[nBandCount++] = atoi(papszArgv[++i]) - 1;
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
    HFAGetRasterInfo( hHFA, NULL, NULL, &nHFABandCount );

/* -------------------------------------------------------------------- */
/*      Has the user requested an RGB image?                            */
/* -------------------------------------------------------------------- */
    if( nBandCount > 0 )
    {
        char	szFilename[512];

        if( RGBComboValidate( hHFA, hHFA->papoBand,
                              nBandCount, anBandList ) == CE_Failure )
            exit( 1 );

        if( strstr(pszDstBasename,".") == NULL )
            sprintf( szFilename, "%s.tif", pszDstBasename );
        else
            sprintf( szFilename, "%s", pszDstBasename );

        if( gnReportOn )
        {
            printf( "Translating bands " );
            for( i = 0; i < nBandCount; i++ )
            {
                if( i != 0 )
                    printf( "," );
                printf( "%d", anBandList[i]+1 );
            }
            
            printf( " to an RGB TIFF file %s.\n",
                    szFilename );
        }
        
        ImagineToGeoTIFF( hHFA, hHFA->papoBand, nBandCount, anBandList,
                          szFilename,
                          nCompressFlag,
                          nOverviewCount == 0,
                          bWriteInStrips );

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
        for( nBand = 1; nBand <= nHFABandCount; nBand++ )
        {
            char	szFilename[512];

            if( !ValidateDataType( hHFA, nBand ) )
                continue;

            if( nHFABandCount == 1 && strstr(pszDstBasename,".tif") != NULL )
                sprintf( szFilename, "%s", pszDstBasename );
            else if( nHFABandCount == 1 )
                sprintf( szFilename, "%s.tif", pszDstBasename );
            else
                sprintf( szFilename, "%s%d.tif", pszDstBasename, nBand );
        
            if( gnReportOn )
                printf( "Translating band %d to an TIFF file %s.\n",
                        nBand, szFilename );

            anBandList[0] = nBand - 1;
            ImagineToGeoTIFF( hHFA, hHFA->papoBand, 1, anBandList,
                              szFilename, nCompressFlag,
                              nOverviewCount == 0,
                              bWriteInStrips );

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
    double	*padfRed, *padfGreen, *padfBlue, *padfAlpha;
    int		nColors, i;

    poBand->GetPCT( &nColors, &padfRed, &padfGreen, &padfBlue, &padfAlpha );
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
/*                           LoadRowOfTiles()                           */
/*                                                                      */
/*      Helper function for CopyOneBandToStrips() to load a row of      */
/*      tiles into a line (rather than tile) interleaved strip but      */
/*      with the height of a tile rather than the eventual strip        */
/*      size.  Note that unneeded data in the last tile is              */
/*      discarded.                                                      */
/************************************************************************/

static CPLErr LoadRowOfTiles( HFABand * poBand, unsigned char * pabyRowOfTiles,
                              int nTileXSize, int nTileYSize,
                              int nStripWidth, int nDataBits, int nTileRow,
                              int nSample )

{
    unsigned char	*pabyTile;
    int			iTileX;

    pabyTile = (unsigned char *) VSIMalloc(nTileXSize*nTileYSize*nDataBits/8);
    if( pabyTile == NULL )
        return CE_Failure;

    for( iTileX = 0; iTileX*nTileXSize < nStripWidth; iTileX++ )
    {
        int	nCopyBytes, nRowOffset, iTileLine;
        
        if( poBand->GetRasterBlock( iTileX, nTileRow, pabyTile ) != CE_None )
            return( CE_Failure );

        if( (iTileX+1) * nTileXSize > nStripWidth )
            nCopyBytes = (nStripWidth - iTileX * nTileXSize) * nDataBits / 8;
        else
            nCopyBytes = nTileXSize * nDataBits / 8;

        nRowOffset = iTileX * nTileXSize * nDataBits / 8;
        
        for( iTileLine = 0; iTileLine < nTileYSize; iTileLine++ )
        {
            memcpy( pabyRowOfTiles + nRowOffset
                    + iTileLine * nStripWidth * nDataBits / 8,
                    pabyTile + iTileLine * nTileXSize * nDataBits / 8,
                    nCopyBytes );
        }
    }

    VSIFree( pabyTile );

    return( CE_None );
}


/************************************************************************/
/*                        CopyOneBandToStrips()                         */
/*                                                                      */
/*      copy just the imagery tiles from an Imagine band (full res,     */
/*      or overview) to a sample of a TIFF file with stripped,          */
/*      rather than tiled organization..                                */
/************************************************************************/

static CPLErr CopyOneBandToStrips( HFABand * poBand, TIFF * hTIFF, int nSample)

{
    unsigned char *pabyRowOfTiles;
    unsigned char *pabyStrip;
    int		nTileXSize, nTileYSize, nStripWidth, nDataBits, iStrip;
    int		nLoadedTileRow;
    uint32	nRowsPerStrip;
    
/* -------------------------------------------------------------------- */
/*	Collect various information in local variables.			*/
/* -------------------------------------------------------------------- */
    nTileXSize = poBand->nBlockXSize;
    nTileYSize = poBand->nBlockYSize;
    nStripWidth = poBand->nWidth;
    nDataBits = HFAGetDataTypeBits( poBand->nDataType );

    TIFFGetField( hTIFF, TIFFTAG_ROWSPERSTRIP, &(nRowsPerStrip) );

/* -------------------------------------------------------------------- */
/*      Verify that scanlines in tiles, and strips fall on byte         */
/*      boundaries.                                                     */
/* -------------------------------------------------------------------- */
    assert( (nTileXSize * nDataBits) % 8 == 0 );
    assert( (nStripWidth * nDataBits) % 8 == 0 );

/* -------------------------------------------------------------------- */
/*      Allocate a buffer big enough to hold a whole row of tiles,      */
/*      and another big enough to hold a strip on the output file.      */
/* -------------------------------------------------------------------- */
    pabyRowOfTiles = (unsigned char *)
        VSIMalloc((nStripWidth * nTileYSize * nDataBits) / 8);
    pabyStrip = (unsigned char *)
        VSIMalloc((nStripWidth * nRowsPerStrip * nDataBits) / 8);

    if( pabyRowOfTiles == NULL || pabyStrip == NULL )
    {
        fprintf( stderr,
                 "Out of memory allocating working buffer(s).\n" );
        return( CE_Failure );
    }

/* -------------------------------------------------------------------- */
/*	Loop through image one strip at a time. 			*/
/* -------------------------------------------------------------------- */
    nLoadedTileRow = -1;
    for( iStrip = 0; iStrip * nRowsPerStrip < poBand->nHeight; iStrip++ )
    {
        int	iStripLine, nStripLines, iStripOffset;
        int	iStripId;

        iStripOffset = iStrip * nRowsPerStrip;
        nStripLines = nRowsPerStrip;
        if( iStrip + nRowsPerStrip > poBand->nHeight )
            nStripLines = poBand->nHeight - iStrip;

/* -------------------------------------------------------------------- */
/*      Fill in the strip one line at a time, triggering the load of    */
/*      a new row of tiles when a tile boundary is crossed.             */
/* -------------------------------------------------------------------- */
        for( iStripLine = 0; iStripLine < nStripLines; iStripLine++ )
        {
            int		iLineWithinTiles;

            iLineWithinTiles =
                iStripLine + iStripOffset - nLoadedTileRow * nTileYSize;
            
            if( iStripLine + iStripOffset >= (nLoadedTileRow+1) * nTileYSize )
            {
                nLoadedTileRow++;
                iLineWithinTiles = 0;
                LoadRowOfTiles( poBand, pabyRowOfTiles, nTileXSize, nTileYSize,
                                nStripWidth, nDataBits, nLoadedTileRow,
                                nSample );
            }

            memcpy( pabyStrip + (iStripLine * nStripWidth * nDataBits)/8,
                    pabyRowOfTiles
                    	     + (iLineWithinTiles * nStripWidth * nDataBits)/8,
                    nStripWidth * nDataBits / 8 );
        }

/* -------------------------------------------------------------------- */
/*      Write out the strip.                                            */
/* -------------------------------------------------------------------- */
        iStripId = TIFFComputeStrip( hTIFF, iStrip * nRowsPerStrip, nSample );
        
        if( TIFFWriteEncodedStrip( hTIFF, iStripId, pabyStrip,
                            (nStripLines * nStripWidth * nDataBits)/ 8) < 0 )
            return( CE_Failure );
    }

    VSIFree( pabyStrip );
    VSIFree( pabyRowOfTiles );

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
                              HFABand ** papoBandList,
                              int nBandCount, int * panBandList,
                              const char * pszDstFilename,
                              int nCompressFlag, int bCopyOverviews,
                              int bWriteInStrips )

{
    TIFF	*hTIFF;
    int		nXSize, nYSize, nBlockXSize, nBlockYSize, nDataType;
    int		nBlocksPerRow, nBlocksPerColumn;
    double	*padfRed, *padfGreen, *padfBlue, *padfAlpha;
    int		nColors;

    HFAGetRasterInfo( hHFA, &nXSize, &nYSize, NULL );

    nDataType = papoBandList[panBandList[0]]->nDataType;
    nBlockXSize = papoBandList[panBandList[0]]->nBlockXSize;
    nBlockYSize = papoBandList[panBandList[0]]->nBlockYSize;

/* -------------------------------------------------------------------- */
/*      Tile sizes must be a multiple of 16.                            */
/* -------------------------------------------------------------------- */
    if( (nBlockXSize % 16) != 0 || (nBlockYSize % 16) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                 "Tile sizes must be multiple of 16.  Imagine file tile size\n"
                  "of %dx%d is not.  Translation aborted.\n",
                  nBlockXSize, nBlockYSize );
        
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Fetch PCT, if available.                                        */
/* -------------------------------------------------------------------- */
    if( nBandCount == 1 )
        papoBandList[panBandList[0]]->GetPCT( &nColors, &padfRed,
                                              &padfGreen, &padfBlue, &padfAlpha );
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

    if( nBandCount == 1 )
    {
        TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, 1 );
        TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG );
    }
    else
    {
        TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, nBandCount );
        TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG, PLANARCONFIG_SEPARATE );
    }
        
    TIFFSetField( hTIFF, TIFFTAG_SUBFILETYPE, 0 );

    if( bWriteInStrips )
    {
        TIFFSetField( hTIFF, TIFFTAG_ROWSPERSTRIP,
                      TIFFDefaultStripSize( hTIFF, 0 ) );
    }
    else
    {
        TIFFSetField( hTIFF, TIFFTAG_TILEWIDTH, nBlockXSize );
        TIFFSetField( hTIFF, TIFFTAG_TILELENGTH, nBlockYSize );
    }

    if( nColors > 0 )
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE );
    else if( nBandCount < 3 )
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK );
    else
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB );
    
/* -------------------------------------------------------------------- */
/*	Do we have min/max value information?				*/
/* -------------------------------------------------------------------- */
    if( nBandCount == 1 )
        ImagineToGeoTIFFDataRange( papoBandList[panBandList[0]], hTIFF );

/* -------------------------------------------------------------------- */
/*      Copy over one, or three bands of raster data.                   */
/* -------------------------------------------------------------------- */

    if( bWriteInStrips )
    {
        int		iBand;
        
        for( iBand = 0; iBand < nBandCount; iBand++ )
            CopyOneBandToStrips( papoBandList[panBandList[iBand]], hTIFF,
                                 iBand );
    }
    else
    {
        int		iBand;
        
        for( iBand = 0; iBand < nBandCount; iBand++ )
            CopyOneBand( papoBandList[panBandList[iBand]], hTIFF, iBand );
    }
    
/* -------------------------------------------------------------------- */
/*      Write Geotiff information.                                      */
/* -------------------------------------------------------------------- */
    ImagineToGeoTIFFProjection( hHFA, hTIFF );

/* -------------------------------------------------------------------- */
/*      Write Palette                                                   */
/* -------------------------------------------------------------------- */
    if( nColors > 0 )
        ImagineToGeoTIFFPalette( papoBandList[panBandList[0]], hTIFF );

/* -------------------------------------------------------------------- */
/*      Write overviews                                                 */
/* -------------------------------------------------------------------- */
    if( bCopyOverviews )
        CopyPyramidsToTiff( hHFA, papoBandList[panBandList[0]], hTIFF,
                            nCompressFlag );

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
    double	*padfRed, *padfGreen, *padfBlue, *padfAlpha;

    poBand->GetPCT( &nColors, &padfRed, &padfGreen, &padfBlue, &padfAlpha );
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
                                HFABand ** papoBands,
                                int nBandCount, int * panBandList )

{
    int		nHFABandCount, iBand;
    
    HFAGetRasterInfo( hHFA, NULL, NULL, &nHFABandCount );

/* -------------------------------------------------------------------- */
/*      Check that band numbers exist.                                  */
/* -------------------------------------------------------------------- */
    for( iBand = 0; iBand < nBandCount; iBand++ )
    {
        if( panBandList[iBand] < 0 || panBandList[iBand] >= nHFABandCount )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Selected band (%d) not legal.  Only %d bands are "
                      "available.\n", panBandList[iBand]+1, nHFABandCount );
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Verify that all the bands have the same datatype, tile size,    */
/*      and so forth.                                                   */
/* -------------------------------------------------------------------- */
    int		bBandsMatch = TRUE;

    for( iBand = 1; iBand < nBandCount; iBand++ )
    {
        HFABand	*poBandTest = papoBands[panBandList[iBand]];
        HFABand	*poBandBase = papoBands[panBandList[iBand]];
        
        if( poBandTest->nDataType != poBandBase->nDataType )
            bBandsMatch = FALSE;
    }
    
    if( !bBandsMatch )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Datatypes of different bands do not match.\n" );
        return CE_Failure;
    }

    for( iBand = 1; iBand < nBandCount; iBand++ )
    {
        HFABand	*poBandTest = papoBands[panBandList[iBand]];
        HFABand	*poBandBase = papoBands[panBandList[iBand]];
        
        if( poBandTest->nBlockXSize != poBandBase->nBlockXSize
         || poBandTest->nBlockYSize != poBandBase->nBlockYSize )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Tile sizes of different bands do not match.\n" );
            return CE_Failure;
        }
    }
    
/* -------------------------------------------------------------------- */
/*	Verify that each of the bands is legal.				*/
/* -------------------------------------------------------------------- */
    for( iBand = 1; iBand < nBandCount; iBand++ )
    {
        if( !ValidateDataType( hHFA, panBandList[iBand]+1 ) )
            return CE_Failure;
    }

    return CE_None;
}
