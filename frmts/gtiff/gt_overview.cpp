/******************************************************************************
 * $Id$
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Code to build overviews of external databases as a TIFF file. 
 *           Only used by the GDALDefaultOverviews::BuildOverviews() method.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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
 * Revision 1.14  2006/04/05 00:06:14  fwarmerdam
 * fix contact info
 *
 * Revision 1.13  2005/05/22 16:58:12  dron
 * Added PlanarConfiguration parameter to the TIFF_WriteOverview().
 *
 * Revision 1.12  2004/09/09 18:59:21  fwarmerdam
 * Adjusted to properly support colormaps with up to 65536 entries.
 *
 * Revision 1.11  2004/04/14 09:51:36  dron
 * Added support for COMPRESS_OVERVIEW option.
 *
 * Revision 1.10  2002/07/20 12:27:13  dron
 * Fixed building with externally preinstalled TIFF library.
 *
 * Revision 1.9  2001/10/12 15:06:05  warmerda
 * various build improvements to avoid internal/external conflicts
 *
 * Revision 1.8  2001/07/18 04:51:56  warmerda
 * added CPL_CVSID
 *
 * Revision 1.7  2001/06/25 14:44:15  warmerda
 * Fixed to return success if no bands requested to be processed.
 *
 * Revision 1.6  2000/09/25 21:16:24  warmerda
 * Avoid initialization warnings.
 *
 * Revision 1.5  2000/08/14 18:37:24  warmerda
 * added (untested) support for writing palettes to overviews
 *
 * Revision 1.4  2000/07/17 17:09:30  warmerda
 * added support for complex data
 *
 * Revision 1.3  2000/06/26 22:18:33  warmerda
 * added scaled progress support
 *
 * Revision 1.2  2000/06/19 18:47:24  warmerda
 * fixed header
 *
 * Revision 1.1  2000/04/21 21:53:15  warmerda
 * New
 *
 */

#include "gdal_priv.h"
#define CPL_SERV_H_INCLUDED

#include "tiffio.h"
#include "xtiffio.h"
#include "geotiff.h"
#include "tif_ovrcache.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        GTIFFBuildOverviews()                         */
/************************************************************************/

CPLErr 
GTIFFBuildOverviews( const char * pszFilename,
                     int nBands, GDALRasterBand **papoBandList, 
                     int nOverviews, int * panOverviewList,
                     const char * pszResampling, 
                     GDALProgressFunc pfnProgress, void * pProgressData )

{
    TIFF    *hOTIFF;
    int     nBitsPerPixel=0, nCompression=COMPRESSION_NONE, nPhotometric=0;
    int     nSampleFormat=0, nPlanarConfig, iOverview, iBand;
    int     nXSize=0, nYSize=0;

    if( nBands == 0 || nOverviews == 0 )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Verify that the list of bands is suitable for emitting in       */
/*      TIFF file.                                                      */
/* -------------------------------------------------------------------- */
    for( iBand = 0; iBand < nBands; iBand++ )
    {
        int     nBandBits, nBandFormat;
        GDALRasterBand *hBand = papoBandList[iBand];

        switch( hBand->GetRasterDataType() )
        {
          case GDT_Byte:
            nBandBits = 8;
            nBandFormat = SAMPLEFORMAT_UINT;
            break;

          case GDT_UInt16:
            nBandBits = 16;
            nBandFormat = SAMPLEFORMAT_UINT;
            break;

          case GDT_Int16:
            nBandBits = 16;
            nBandFormat = SAMPLEFORMAT_INT;
            break;

          case GDT_UInt32:
            nBandBits = 32;
            nBandFormat = SAMPLEFORMAT_UINT;
            break;

          case GDT_Int32:
            nBandBits = 32;
            nBandFormat = SAMPLEFORMAT_INT;
            break;

          case GDT_Float32:
            nBandBits = 32;
            nBandFormat = SAMPLEFORMAT_IEEEFP;
            break;

          case GDT_Float64:
            nBandBits = 64;
            nBandFormat = SAMPLEFORMAT_IEEEFP;
            break;

          case GDT_CInt16:
            nBandBits = 32;
            nBandFormat = SAMPLEFORMAT_COMPLEXINT;
            break;

          case GDT_CFloat32:
            nBandBits = 64;
            nBandFormat = SAMPLEFORMAT_COMPLEXIEEEFP;
            break;

          case GDT_CFloat64:
            nBandBits = 128;
            nBandFormat = SAMPLEFORMAT_COMPLEXIEEEFP;
            break;

          default:
            CPLAssert( FALSE );
            return CE_Failure;
        }

        if( iBand == 0 )
        {
            nBitsPerPixel = nBandBits;
            nSampleFormat = nBandFormat;
            nXSize = hBand->GetXSize();
            nYSize = hBand->GetYSize();
        }
        else if( nBitsPerPixel != nBandBits || nSampleFormat != nBandFormat )
        {
            CPLError( CE_Failure, CPLE_NotSupported, 
                      "GTIFFBuildOverviews() doesn't support a mixture of band"
                      " data types." );
            return CE_Failure;
        }
        else if( hBand->GetColorTable() != NULL )
        {
            CPLError( CE_Failure, CPLE_NotSupported, 
                      "GTIFFBuildOverviews() doesn't support building"
                      " overviews of multiple colormapped bands." );
            return CE_Failure;
        }
        else if( hBand->GetXSize() != nXSize 
                 || hBand->GetYSize() != nYSize )
        {
            CPLError( CE_Failure, CPLE_NotSupported, 
                      "GTIFFBuildOverviews() doesn't support building"
                      " overviews of different sized bands." );
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Use specified compression method.                               */
/* -------------------------------------------------------------------- */
    const char *pszCompress = CPLGetConfigOption( "COMPRESS_OVERVIEW", NULL );

    if( pszCompress )
    {
        if( EQUAL( pszCompress, "JPEG" ) )
            nCompression = COMPRESSION_JPEG;
        else if( EQUAL( pszCompress, "LZW" ) )
            nCompression = COMPRESSION_LZW;
        else if( EQUAL( pszCompress, "PACKBITS" ))
            nCompression = COMPRESSION_PACKBITS;
        else if( EQUAL( pszCompress, "DEFLATE" ) || EQUAL( pszCompress, "ZIP" ))
            nCompression = COMPRESSION_ADOBE_DEFLATE;
        else
            CPLError( CE_Warning, CPLE_IllegalArg, 
                      "COMPRESS_OVERVIEW=%s value not recognised, ignoring.",
                      pszCompress );
    }

/* -------------------------------------------------------------------- */
/*      Figure out the planar configuration to use.                     */
/* -------------------------------------------------------------------- */
    if( nBands == 1 )
        nPlanarConfig = PLANARCONFIG_CONTIG;
    else
        nPlanarConfig = PLANARCONFIG_SEPARATE;

/* -------------------------------------------------------------------- */
/*      Figure out the photometric interpretation to use.               */
/* -------------------------------------------------------------------- */
    if( nBands == 3 )
        nPhotometric = PHOTOMETRIC_RGB;
    else if( papoBandList[0]->GetColorTable() != NULL )
    {
        nPhotometric = PHOTOMETRIC_PALETTE;
        /* should set the colormap up at this point too! */
    }
    else
        nPhotometric = PHOTOMETRIC_MINISBLACK;

/* -------------------------------------------------------------------- */
/*      Create the file, if it does not already exist.                  */
/* -------------------------------------------------------------------- */
    VSIStatBuf  sStatBuf;

    if( VSIStat( pszFilename, &sStatBuf ) != 0 )
    {
        hOTIFF = XTIFFOpen( pszFilename, "w+" );
        if( hOTIFF == NULL )
        {
            if( CPLGetLastErrorNo() == 0 )
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "Attempt to create new tiff file `%s'\n"
                          "failed in XTIFFOpen().\n",
                          pszFilename );

            return CE_Failure;
        }
    }
/* -------------------------------------------------------------------- */
/*      Otherwise just open it for update access.                       */
/* -------------------------------------------------------------------- */
    else 
    {
        hOTIFF = XTIFFOpen( pszFilename, "r+" );
        if( hOTIFF == NULL )
        {
            if( CPLGetLastErrorNo() == 0 )
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "Attempt to create new tiff file `%s'\n"
                          "failed in XTIFFOpen().\n",
                          pszFilename );

            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we have a palette?  If so, create a TIFF compatible version. */
/* -------------------------------------------------------------------- */
    unsigned short	anTRed[65536], anTGreen[65536], anTBlue[65536];
    unsigned short      *panRed=NULL, *panGreen=NULL, *panBlue=NULL;

    if( nPhotometric == PHOTOMETRIC_PALETTE )
    {
        GDALColorTable *poCT = papoBandList[0]->GetColorTable();
        int nColorCount = MIN(65536,poCT->GetColorEntryCount());

        memset( anTRed, 0, 65536 * 2 );
        memset( anTGreen, 0, 65536 * 2 );
        memset( anTBlue, 0, 65536 * 2 );

        for( int iColor = 0; iColor < nColorCount; iColor++ )
        {
            GDALColorEntry  sRGB;

            poCT->GetColorEntryAsRGB( iColor, &sRGB );

            anTRed[iColor] = (unsigned short) (256 * sRGB.c1);
            anTGreen[iColor] = (unsigned short) (256 * sRGB.c2);
            anTBlue[iColor] = (unsigned short) (256 * sRGB.c3);
        }

        panRed = anTRed;
        panGreen = anTGreen;
        panBlue = anTBlue;
    }
        
/* -------------------------------------------------------------------- */
/*      Loop, creating overviews.                                       */
/* -------------------------------------------------------------------- */
    for( iOverview = 0; iOverview < nOverviews; iOverview++ )
    {
        int    nOXSize, nOYSize;
        uint32 nDirOffset;

        nOXSize = (nXSize + panOverviewList[iOverview] - 1) 
            / panOverviewList[iOverview];
        nOYSize = (nYSize + panOverviewList[iOverview] - 1) 
            / panOverviewList[iOverview];

        nDirOffset = 
            TIFF_WriteOverview( hOTIFF, nOXSize, nOYSize, nBitsPerPixel, 
                                nPlanarConfig, nBands,
                                128, 128, TRUE, nCompression,
                                nPhotometric, nSampleFormat, 
                                panRed, panGreen, panBlue, 
                                FALSE );
    }

    XTIFFClose( hOTIFF );

/* -------------------------------------------------------------------- */
/*      Open the overview dataset so that we can get at the overview    */
/*      bands.                                                          */
/* -------------------------------------------------------------------- */
    GDALDataset *hODS;

    hODS = (GDALDataset *) GDALOpen( pszFilename, GA_Update );
    if( hODS == NULL )
        return CE_Failure;
    
/* -------------------------------------------------------------------- */
/*      Loop writing overview data.                                     */
/* -------------------------------------------------------------------- */
    GDALRasterBand   **papoOverviews;

    papoOverviews = (GDALRasterBand **) CPLCalloc(sizeof(void*),128);

    for( iBand = 0; iBand < nBands; iBand++ )
    {
        GDALRasterBand    *hSrcBand = papoBandList[iBand];
        GDALRasterBand    *hDstBand;
        int               nDstOverviews;
        CPLErr            eErr;

        hDstBand = hODS->GetRasterBand( iBand+1 );
        
        papoOverviews[0] = hDstBand;
        nDstOverviews = hDstBand->GetOverviewCount() + 1;
        CPLAssert( nDstOverviews < 128 );
        nDstOverviews = MIN(128,nDstOverviews);

        for( int i = 0; i < nDstOverviews-1; i++ )
        {
            papoOverviews[i+1] = hDstBand->GetOverview(i);
        }

        void         *pScaledProgressData;

        pScaledProgressData = 
            GDALCreateScaledProgress( iBand / (double) nBands, 
                                      (iBand+1) / (double) nBands,
                                      pfnProgress, pProgressData );

        eErr = 
            GDALRegenerateOverviews( hSrcBand, nDstOverviews, papoOverviews, 
                                     pszResampling,pfnProgress,pProgressData);

        GDALDestroyScaledProgress( pScaledProgressData );

        if( eErr != CE_None )
        {
            delete hODS;
            return eErr;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    hODS->FlushCache();
    delete hODS;

    pfnProgress( 1.0, NULL, pProgressData );

    return CE_None;
}
    
