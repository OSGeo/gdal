/******************************************************************************
 * $Id$
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Mapping Imagine georeferencing to GeoTIFF georeferencing.
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
 * Revision 1.1  1999/01/22 17:40:43  warmerda
 * New
 *
 */

#include "hfa_p.h"
#include "cpl_conv.h"
#include "tiffio.h"

CPL_C_START
void ImagineToGeoTIFFPalette( HFAHandle hHFA, int nBand, TIFF * hTIFF );
CPL_C_END

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
    void	*pData;

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
   
/* -------------------------------------------------------------------- */
/*	Allocate a block buffer.					*/
/* -------------------------------------------------------------------- */
    int		nTileSize;
    
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
            int	iBlock = iBlockX + iBlockY * poBand->nBlocksPerRow;

            if( poBand->GetRasterBlock( iBlockX, iBlockY, pData )
                != CE_None )
                return( CE_Failure );

            if( HFAGetDataTypeBits(poBand->nDataType) == 16 )
            {
                int		ii;

                for( ii = 0;
                     ii < poBand->nBlockXSize*poBand->nBlockYSize;
                     ii++ )
                {
                    unsigned char *pabyData = (unsigned char *) pData;
                    int		nTemp;

                    nTemp = pabyData[ii*2];
                    pabyData[ii*2] = pabyData[ii*2+1];
                    pabyData[ii*2+1] = nTemp;
                }
            }

            if( TIFFWriteEncodedTile( hTIFF, iBlock, pData, nTileSize ) < 1 )
                return( CE_Failure );
        }
    }

    VSIFree( pData );

    return( CE_None );
}


/************************************************************************/
/*                         CopyPyramidsToTiff()                         */
/*                                                                      */
/*      Copy reduced resolution layers to the TIFF file as              */
/*      overviews.                                                      */
/************************************************************************/

CPL_C_START
CPLErr CopyPyramidsToTiff( HFAHandle, int, TIFF * );
CPL_C_END

CPLErr CopyPyramidsToTiff( HFAHandle psInfo, int nBand, TIFF * hTIFF )

{
    HFABand	*poBand = psInfo->papoBand[nBand-1];
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
        
        if( RRD2Tiff( poOverviewBand, hTIFF, nPhotometric, COMPRESSION_NONE )
						            == CE_None
            && nColors > 0 )
            ImagineToGeoTIFFPalette( psInfo, nBand, hTIFF );
        
        delete poOverviewBand;
    }

    return CE_None;
}

