/******************************************************************************
 * $Id$
 *
 * Project:  GeoTIFF Driver
 * Purpose:  GDAL GeoTIFF support.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
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
 * Revision 1.29  2000/06/19 18:48:29  warmerda
 * added IBuildOverviews() implementation on GTiffDataset
 *
 * Revision 1.28  2000/06/19 14:18:01  warmerda
 * added help link
 *
 * Revision 1.27  2000/06/09 14:21:53  warmerda
 * Don't try to write geotiff info to read-only files.
 *
 * Revision 1.26  2000/05/04 13:56:28  warmerda
 * Avoid having a block ysize larger than the whole file for stripped files.
 *
 * Revision 1.25  2000/05/01 01:53:33  warmerda
 * added various compress options
 *
 * Revision 1.24  2000/04/21 21:53:42  warmerda
 * rewrote metadata support, do flush before changing directories
 *
 * Revision 1.23  2000/03/31 14:12:38  warmerda
 * added CPL based handling of libtiff warnings and errors
 *
 * Revision 1.22  2000/03/31 13:36:07  warmerda
 * added gcp's and metadata
 *
 * Revision 1.21  2000/03/23 18:47:52  warmerda
 * added support for writing tiled TIFF
 *
 * Revision 1.20  2000/03/23 16:54:35  warmerda
 * fixed up geotransform initialization
 *
 * Revision 1.19  2000/03/14 15:16:21  warmerda
 * initialize adfGeoTransform[]
 *
 * Revision 1.18  2000/03/13 14:33:01  warmerda
 * avoid ambiguity with Open
 *
 * Revision 1.17  2000/03/06 02:23:08  warmerda
 * added overviews, and colour tables
 *
 * Revision 1.16  2000/02/28 16:32:20  warmerda
 * use SetBand method
 *
 * Revision 1.15  2000/01/06 14:55:53  warmerda
 * Removed testing printf.
 *
 * Revision 1.14  2000/01/06 14:51:27  warmerda
 * Support reading non eight bit data directly if SEPARATE or 1 band.
 *
 * Revision 1.13  1999/12/30 02:43:12  warmerda
 * allow an empty string in SetProjection
 *
 * Revision 1.12  1999/11/17 16:16:53  warmerda
 * Fixed X/Y mixup in setting xblocksize for stripped files.
 *
 * Revision 1.11  1999/10/29 17:28:11  warmerda
 * Added projection support, and odd pixel data types
 *

 * Revision 1.10  1999/08/12 18:23:15  warmerda
 * Fixed the ability to write non GDT_Byte data.
 *
 * Revision 1.9  1999/07/29 18:03:05  warmerda
 * return OGIS WKT format, instead of Proj.4 format
 *
 * Revision 1.8  1999/07/23 19:22:04  warmerda
 * Added support for writing geotransform information to newly created tiff
 * files.
 *
 * Revision 1.7  1999/05/17 01:36:17  warmerda
 * Added support for reading tiled tiff files.
 *
 * Revision 1.6  1999/02/24 16:22:36  warmerda
 * Added use of geo_normalize
 *
 * Revision 1.5  1999/01/27 20:29:16  warmerda
 * Added constructor/destructor declarations
 *
 * Revision 1.4  1999/01/11 15:30:44  warmerda
 * pixel interleaved case
 *
 * Revision 1.3  1999/01/05 16:53:38  warmerda
 * Added working creation support.
 *
 * Revision 1.2  1998/12/03 18:37:58  warmerda
 *
 * Use CPLErr, not GBSErr.
 *
 * Revision 1.1  1998/11/29 22:41:12  warmerda
 * New
 *
 */

#include "tiffiop.h"
#include "xtiffio.h"
#include "geotiff.h"
#include "gdal_priv.h"
#include "geo_normalize.h"
#include "tif_ovrcache.h"
#include "cpl_string.h"

static GDALDriver	*poGTiffDriver = NULL;

CPL_C_START
void	GDALRegister_GTiff(void);
char *  GTIFGetOGISDefn( GTIFDefn * );
int     GTIFSetFromOGISDefn( GTIF *, const char * );
CPL_C_END


/************************************************************************/
/* ==================================================================== */
/*				GTiffDataset				*/
/* ==================================================================== */
/************************************************************************/

class GTiffRasterBand;

class GTiffDataset : public GDALDataset
{
    friend	GTiffRasterBand;
    
    TIFF	*hTIFF;

    uint32      nDirOffset;
    int		bBase;

    uint16	nPlanarConfig;
    uint16	nSamplesPerPixel;
    uint16	nBitsPerSample;
    uint32	nRowsPerStrip;
    uint16	nPhotometric;
    uint16      nSampleFormat;
    
    int		nBlocksPerBand;

    uint32      nBlockXSize;
    uint32	nBlockYSize;

    int		nLoadedBlock;		/* or tile */
    int		bLoadedStripDirty;
    GByte	*pabyBlockBuf;

    char	*pszProjection;
    double	adfGeoTransform[6];
    int		bGeoTransformValid;

    int		bNewDataset;            /* product of Create() */

    GDALColorTable *poColorTable;

    void	WriteGeoTIFFInfo();
    int		SetDirectory( uint32 nDirOffset = 0 );

    int		nOverviewCount;
    GTiffDataset **papoOverviewDS;

    int		nGCPCount;
    GDAL_GCP	*pasGCPList;

  public:
                 GTiffDataset();
                 ~GTiffDataset();

    virtual const char *GetProjectionRef(void);
    virtual CPLErr SetProjection( const char * );
    virtual CPLErr GetGeoTransform( double * );
    virtual CPLErr SetGeoTransform( double * );

    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

    virtual CPLErr IBuildOverviews( const char *, int, int *, int, int *, 
                                    GDALProgressFunc, void * );

    CPLErr	   OpenOffset( TIFF *, uint32 nDirOffset, int, GDALAccess );

    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );

    virtual void FlushCache( void );
};

/************************************************************************/
/* ==================================================================== */
/*                            GTiffRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class GTiffRasterBand : public GDALRasterBand
{
    friend	GTiffDataset;

  public:

                   GTiffRasterBand( GTiffDataset *, int );

    // should override RasterIO eventually.
    
    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * ); 

    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();

    virtual int    GetOverviewCount();
    virtual GDALRasterBand *GetOverview( int );
};


/************************************************************************/
/*                           GTiffRasterBand()                            */
/************************************************************************/

GTiffRasterBand::GTiffRasterBand( GTiffDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;

/* -------------------------------------------------------------------- */
/*      Get the GDAL data type.                                         */
/* -------------------------------------------------------------------- */
    uint16		nSampleFormat = poDS->nSampleFormat;

    if( poDS->nBitsPerSample <= 8 )
        eDataType = GDT_Byte;
    else if( poDS->nBitsPerSample <= 16 )
    {
        if( nSampleFormat == SAMPLEFORMAT_INT )
            eDataType = GDT_Int16;
        else
            eDataType = GDT_UInt16;
    }
    else if( poDS->nBitsPerSample == 32 )
    {
        if( nSampleFormat == SAMPLEFORMAT_IEEEFP )
            eDataType = GDT_Float32;
        else if( nSampleFormat == SAMPLEFORMAT_INT )
            eDataType = GDT_Int32;
        else
            eDataType = GDT_UInt32;
    }
    else
        eDataType = GDT_Unknown;

/* -------------------------------------------------------------------- */
/*	Establish block size for strip or tiles.			*/
/* -------------------------------------------------------------------- */
    nBlockXSize = poDS->nBlockXSize;
    nBlockYSize = poDS->nBlockYSize;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GTiffRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                    void * pImage )

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;
    int			nBlockBufSize, nBlockId;
    CPLErr		eErr = CE_None;

    poGDS->SetDirectory();

    if( TIFFIsTiled(poGDS->hTIFF) )
        nBlockBufSize = TIFFTileSize( poGDS->hTIFF );
    else
    {
        CPLAssert( nBlockXOff == 0 );
        nBlockBufSize = TIFFStripSize( poGDS->hTIFF );
    }
        
    if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
        nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow
                   + (nBand-1) * poGDS->nBlocksPerBand;
    else
        nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;
        
/* -------------------------------------------------------------------- */
/*	Handle the case of a strip in a writable file that doesn't	*/
/*	exist yet, but that we want to read.  Just set to zeros and	*/
/*	return.								*/
/* -------------------------------------------------------------------- */
    if( poGDS->eAccess == GA_Update
        && (((int) poGDS->hTIFF->tif_dir.td_nstrips) <= nBlockId
            || poGDS->hTIFF->tif_dir.td_stripbytecount[nBlockId] == 0) )
    {
        memset( pImage, 0,
                nBlockXSize * nBlockYSize
                * GDALGetDataTypeSize(eDataType) / 8 );
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Handle simple case (separate, onesampleperpixel)		*/
/* -------------------------------------------------------------------- */
    if( poGDS->nBands == 1
        || poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
    {
        if( TIFFIsTiled( poGDS->hTIFF ) )
        {
            if( TIFFReadEncodedTile( poGDS->hTIFF, nBlockId, pImage,
                                     nBlockBufSize ) == -1 )
            {
                memset( pImage, 0, nBlockBufSize );
                CPLError( CE_Failure, CPLE_AppDefined,
                          "TIFFReadEncodedStrip() failed.\n" );
                
                eErr = CE_Failure;
            }
        }
        else
        {
            if( TIFFReadEncodedStrip( poGDS->hTIFF, nBlockId, pImage,
                                      nBlockBufSize ) == -1 )
            {
                memset( pImage, 0, nBlockBufSize );
                CPLError( CE_Failure, CPLE_AppDefined,
                          "TIFFReadEncodedStrip() failed.\n" );
                
                eErr = CE_Failure;
            }
        }

        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Allocate a temporary buffer for this strip.                     */
/* -------------------------------------------------------------------- */
    if( poGDS->pabyBlockBuf == NULL )
    {
        poGDS->pabyBlockBuf = (GByte *) VSICalloc( 1, nBlockBufSize );
        if( poGDS->pabyBlockBuf == NULL )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory,
                   "Unable to allocate %d bytes for a temporary strip buffer\n"
                      "in GeoTIFF driver.",
                      nBlockBufSize );
            
            return( CE_Failure );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Read the strip                                                  */
/* -------------------------------------------------------------------- */
    if( poGDS->nLoadedBlock != nBlockId )
    {
        if( TIFFIsTiled( poGDS->hTIFF ) )
        {
            if( TIFFReadEncodedTile(poGDS->hTIFF, nBlockId,
                                    poGDS->pabyBlockBuf,
                                    nBlockBufSize) == -1 )
            {
                /* Once TIFFError() is properly hooked, this can go away */
                CPLError( CE_Failure, CPLE_AppDefined,
                          "TIFFReadEncodedStrip() failed." );
                
                memset( poGDS->pabyBlockBuf, 0, nBlockBufSize );
                
                eErr = CE_Failure;
            }
        }
        else
        {
            if( TIFFReadEncodedStrip(poGDS->hTIFF, nBlockId,
                                     poGDS->pabyBlockBuf,
                                     nBlockBufSize) == -1 )
            {
                /* Once TIFFError() is properly hooked, this can go away */
                CPLError( CE_Failure, CPLE_AppDefined,
                          "TIFFReadEncodedStrip() failed." );
                
                memset( poGDS->pabyBlockBuf, 0, nBlockBufSize );
                
                eErr = CE_Failure;
            }
        }
    }

    poGDS->nLoadedBlock = nBlockId;
                              
/* -------------------------------------------------------------------- */
/*      Handle simple case of eight bit data, and pixel interleaving.   */
/* -------------------------------------------------------------------- */
    if( poGDS->nBitsPerSample == 8 )
    {
        int	i, nBlockPixels;
        GByte	*pabyImage;
        
        pabyImage = poGDS->pabyBlockBuf + nBand - 1;

        nBlockPixels = nBlockXSize * nBlockYSize;
        for( i = 0; i < nBlockPixels; i++ )
        {
            ((GByte *) pImage)[i] = *pabyImage;
            pabyImage += poGDS->nBands;
        }
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Non eight bit samples not supported yet.\n" );
        eErr = CE_Failure;
    }

    return eErr;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/*                                                                      */
/*      This is still limited to writing stripped datasets.             */
/************************************************************************/

CPLErr GTiffRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                     void * pImage )

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;
    int		nBlockId, nBlockBufSize;

    poGDS->SetDirectory();

    CPLAssert( poGDS != NULL
               && nBlockXOff >= 0
               && nBlockYOff >= 0
               && pImage != NULL );

    CPLAssert( poGDS->nBands == 1 
               || poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE );

    nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow
        + (nBand-1) * poGDS->nBlocksPerBand;
        
    if( TIFFIsTiled(poGDS->hTIFF) )
    {
        nBlockBufSize = TIFFTileSize( poGDS->hTIFF );
        TIFFWriteEncodedTile( poGDS->hTIFF, nBlockId, pImage, nBlockBufSize );
    }
    else
    {
        nBlockBufSize = TIFFStripSize( poGDS->hTIFF );
        TIFFWriteEncodedStrip( poGDS->hTIFF, nBlockId, pImage, nBlockBufSize );
    }

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp GTiffRasterBand::GetColorInterpretation()

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;

    if( poGDS->nPhotometric == PHOTOMETRIC_RGB )
    {
        if( nBand == 1 )
            return GCI_RedBand;
        else if( nBand == 2 )
            return GCI_GreenBand;
        else if( nBand == 3 )
            return GCI_BlueBand;
        else if( nBand == 4 )
            return GCI_AlphaBand;
        else
            return GCI_Undefined;
    }
    else if( poGDS->nPhotometric == PHOTOMETRIC_PALETTE )
    {
        return GCI_PaletteIndex;
    }
    else
        return GCI_GrayIndex;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *GTiffRasterBand::GetColorTable()

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;

    if( nBand == 1 )
        return poGDS->poColorTable;
    else
        return NULL;
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int GTiffRasterBand::GetOverviewCount()

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;

    return poGDS->nOverviewCount;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *GTiffRasterBand::GetOverview( int i )

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;

    if( i < 0 || i >= poGDS->nOverviewCount )
        return NULL;
    else
        return poGDS->papoOverviewDS[i]->GetRasterBand(nBand);
}



/************************************************************************/
/* ==================================================================== */
/*      GTiffDataset                                                    */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            GTiffDataset()                            */
/************************************************************************/

GTiffDataset::GTiffDataset()

{
    nLoadedBlock = -1;
    bLoadedStripDirty = FALSE;
    pabyBlockBuf = NULL;
    hTIFF = NULL;
    bNewDataset = FALSE;
    poColorTable = NULL;
    pszProjection = NULL;
    bBase = TRUE;
    nOverviewCount = 0;
    papoOverviewDS = NULL;
    nDirOffset = 0;

    bGeoTransformValid = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    nGCPCount = 0;
    pasGCPList = NULL;
}

/************************************************************************/
/*                           ~GTiffDataset()                            */
/************************************************************************/

GTiffDataset::~GTiffDataset()

{
    FlushCache();

    if( bBase )
    {
        for( int i = 0; i < nOverviewCount; i++ )
        {
            delete papoOverviewDS[i];
        }
        CPLFree( papoOverviewDS );
    }

    SetDirectory();

    if( poColorTable != NULL )
        delete poColorTable;

    if( bNewDataset )
        WriteGeoTIFFInfo();

    if( bBase )
    {
        XTIFFClose( hTIFF );
    }

    if( nGCPCount > 0 )
    {
        for( int i = 0; i < nGCPCount; i++ )
            CPLFree( pasGCPList[i].pszId );

        CPLFree( pasGCPList );
    }
}

/************************************************************************/
/*                             FlushCache()                             */
/*                                                                      */
/*      We override this so we can also flush out local tiff strip      */
/*      cache if need be.                                               */
/************************************************************************/

void GTiffDataset::FlushCache()

{
    GDALDataset::FlushCache();

    if( bLoadedStripDirty )
    {
        
        
        bLoadedStripDirty = FALSE;
    }

    CPLFree( pabyBlockBuf );
    pabyBlockBuf = NULL;
    nLoadedBlock = -1;
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr GTiffDataset::IBuildOverviews( 
    const char * pszResampling, 
    int nOverviews, int * panOverviewList,
    int nBands, int * panBandList,
    GDALProgressFunc pfnProgress, void * pProgressData )

{
    CPLErr       eErr = CE_None;
    int          i;
    GTiffDataset *poODS;

/* -------------------------------------------------------------------- */
/*      Our TIFF overview support currently only works safely if all    */
/*      bands are handled at the same time.                             */
/* -------------------------------------------------------------------- */
    if( nBands != GetRasterCount() )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Generation of overviews in TIFF currently only"
                  " supported when operating on all bands.\n" 
                  "Operation failed.\n" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Establish which of the overview levels we already have, and     */
/*      which are new.  We assume that band 1 of the file is            */
/*      representative.                                                 */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nOverviews; i++ )
    {
        int   j;

        for( j = 0; j < nOverviewCount; j++ )
        {
            int    nOvFactor;

            poODS = papoOverviewDS[j];

            nOvFactor = (int) 
                (0.5 + GetRasterXSize() / (double) poODS->GetRasterXSize());

            if( nOvFactor == panOverviewList[i] )
                panOverviewList[i] *= -1;
        }

        if( panOverviewList[i] > 0 )
        {
            uint32	nOverviewOffset;
            int         nOXSize, nOYSize;

            nOXSize = (GetRasterXSize() + panOverviewList[i] - 1) 
                / panOverviewList[i];
            nOYSize = (GetRasterYSize() + panOverviewList[i] - 1)
                / panOverviewList[i];

            nOverviewOffset = 
                TIFF_WriteOverview( hTIFF, nOXSize, nOYSize, 
                                    nBitsPerSample, nSamplesPerPixel, 
                                    128, 128, TRUE, COMPRESSION_NONE, 
                                    nPhotometric, nSampleFormat, 
                                    NULL, NULL, NULL, FALSE );

            poODS = new GTiffDataset();
            if( poODS->OpenOffset( hTIFF, nOverviewOffset, FALSE, 
                                   GA_Update ) != CE_None )
            {
                delete poODS;
            }
            else
            {
                nOverviewCount++;
                papoOverviewDS = (GTiffDataset **)
                    CPLRealloc(papoOverviewDS, 
                               nOverviewCount * (sizeof(void*)));
                papoOverviewDS[nOverviewCount-1] = poODS;
            }
        }
        else
            panOverviewList[i] *= -1;
    }

/* -------------------------------------------------------------------- */
/*      Refresh old overviews that were listed.                         */
/* -------------------------------------------------------------------- */
    GDALRasterBand **papoOverviewBands;

    papoOverviewBands = (GDALRasterBand **) 
        CPLCalloc(sizeof(void*),nOverviews);

    for( int iBand = 0; iBand < nBands && eErr == CE_None; iBand++ )
    {
        GDALRasterBand *poBand;
        int            nNewOverviews;

        poBand = GetRasterBand( panBandList[iBand] );

        nNewOverviews = 0;
        for( i = 0; i < nOverviews && poBand != NULL; i++ )
        {
            int   j;
            
            for( j = 0; j < poBand->GetOverviewCount(); j++ )
            {
                int    nOvFactor;
                GDALRasterBand * poOverview = poBand->GetOverview( j );

                nOvFactor = (int) 
                  (0.5 + poBand->GetXSize() / (double) poOverview->GetXSize());

                if( nOvFactor == panOverviewList[i] )
                {
                    papoOverviewBands[nNewOverviews++] = poOverview;
                }
            }
        }

        eErr = GDALRegenerateOverviews( poBand,
                                        nNewOverviews, papoOverviewBands,
                                        pszResampling, NULL, NULL );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree( papoOverviewBands );

    return eErr;
}


/************************************************************************/
/*                          WriteGeoTIFFInfo()                          */
/************************************************************************/

void GTiffDataset::WriteGeoTIFFInfo()

{
/* -------------------------------------------------------------------- */
/*      If the geotransform is the default, don't bother writing it.    */
/* -------------------------------------------------------------------- */
    if( adfGeoTransform[0] == 0.0 && adfGeoTransform[1] == 1.0
        && adfGeoTransform[2] == 0.0 && adfGeoTransform[3] == 0.0
        && adfGeoTransform[4] == 0.0 && ABS(adfGeoTransform[5]) == 1.0 )
        return;

/* -------------------------------------------------------------------- */
/*      Write the transform.  We ignore the rotational coefficients     */
/*      for now.  We will fix this up later. (notdef)                   */
/* -------------------------------------------------------------------- */
    double	adfPixelScale[3], adfTiePoints[6];

    adfPixelScale[0] = adfGeoTransform[1];
    adfPixelScale[1] = fabs(adfGeoTransform[5]);
    adfPixelScale[2] = 0.0;

    TIFFSetField( hTIFF, TIFFTAG_GEOPIXELSCALE, 3, adfPixelScale );

    adfTiePoints[0] = 0.0;
    adfTiePoints[1] = 0.0;
    adfTiePoints[2] = 0.0;
    adfTiePoints[3] = adfGeoTransform[0];
    adfTiePoints[4] = adfGeoTransform[3];
    adfTiePoints[5] = 0.0;
        
    TIFFSetField( hTIFF, TIFFTAG_GEOTIEPOINTS, 6, adfTiePoints );

/* -------------------------------------------------------------------- */
/*	Write out projection definition.				*/
/* -------------------------------------------------------------------- */
    if( !EQUAL(pszProjection,"") )
    {
        GTIF	*psGTIF;

        psGTIF = GTIFNew( hTIFF );
        GTIFSetFromOGISDefn( psGTIF, pszProjection );
        GTIFWriteKeys( psGTIF );
        GTIFFree( psGTIF );
    }
}

/************************************************************************/
/*                            SetDirectory()                            */
/************************************************************************/

int GTiffDataset::SetDirectory( uint32 nNewOffset )

{
    if( nNewOffset == 0 )
        nNewOffset = nDirOffset;

    if( nNewOffset == 0)
        return TRUE;

    if( TIFFCurrentDirOffset(hTIFF) == nNewOffset )
        return TRUE;

    if( GetAccess() == GA_Update )
        TIFFFlush( hTIFF );
    
    return TIFFSetSubDirectory( hTIFF, nNewOffset );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GTiffDataset::Open( GDALOpenInfo * poOpenInfo )

{
    TIFF	*hTIFF;

/* -------------------------------------------------------------------- */
/*	First we check to see if the file has the expected header	*/
/*	bytes.								*/    
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 2 )
        return NULL;

    if( (poOpenInfo->pabyHeader[0] != 'I' || poOpenInfo->pabyHeader[1] != 'I')
     && (poOpenInfo->pabyHeader[0] != 'M' || poOpenInfo->pabyHeader[1] != 'M'))
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_ReadOnly )
	hTIFF = XTIFFOpen( poOpenInfo->pszFilename, "r" );
    else
        hTIFF = XTIFFOpen( poOpenInfo->pszFilename, "r+" );
    
    if( hTIFF == NULL )
        return( NULL );

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GTiffDataset 	*poDS;

    poDS = new GTiffDataset();

    if( poDS->OpenOffset(hTIFF,TIFFCurrentDirOffset(hTIFF), TRUE,
                         poOpenInfo->eAccess ) != CE_None )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                             OpenOffset()                             */
/*                                                                      */
/*      Initialize the GTiffDataset based on a passed in file           */
/*      handle, and directory offset to utilize.  This is called for    */
/*      full res, and overview pages.                                   */
/************************************************************************/

CPLErr GTiffDataset::OpenOffset( TIFF *hTIFFIn, uint32 nDirOffsetIn, 
				 int bBaseIn, GDALAccess eAccess )

{
    uint32	nXSize, nYSize;

    hTIFF = hTIFFIn;

    poDriver = poGTiffDriver;
    nDirOffset = nDirOffsetIn;

    SetDirectory( nDirOffsetIn );

    bBase = bBaseIn;

    this->eAccess = eAccess;

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    TIFFGetField( hTIFF, TIFFTAG_IMAGEWIDTH, &nXSize );
    TIFFGetField( hTIFF, TIFFTAG_IMAGELENGTH, &nYSize );
    nRasterXSize = nXSize;
    nRasterYSize = nYSize;

    if( !TIFFGetField(hTIFF, TIFFTAG_SAMPLESPERPIXEL, &nSamplesPerPixel ) )
        nBands = 1;
    else
        nBands = nSamplesPerPixel;
    
    if( !TIFFGetField(hTIFF, TIFFTAG_BITSPERSAMPLE, &(nBitsPerSample)) )
        nBitsPerSample = 1;
    
    if( !TIFFGetField( hTIFF, TIFFTAG_PLANARCONFIG, &(nPlanarConfig) ) )
        nPlanarConfig = PLANARCONFIG_CONTIG;
    
    if( !TIFFGetField( hTIFF, TIFFTAG_PHOTOMETRIC, &(nPhotometric) ) )
        nPhotometric = PHOTOMETRIC_MINISBLACK;
    
    if( !TIFFGetField( hTIFF, TIFFTAG_SAMPLEFORMAT, &(nSampleFormat) ) )
        nSampleFormat = SAMPLEFORMAT_UINT;
    
/* -------------------------------------------------------------------- */
/*      Get strip/tile layout.                                          */
/* -------------------------------------------------------------------- */
    if( TIFFIsTiled(hTIFF) )
    {
        TIFFGetField( hTIFF, TIFFTAG_TILEWIDTH, &(nBlockXSize) );
        TIFFGetField( hTIFF, TIFFTAG_TILELENGTH, &(nBlockYSize) );
    }
    else
    {
        if( !TIFFGetField( hTIFF, TIFFTAG_ROWSPERSTRIP,
                           &(nRowsPerStrip) ) )
            nRowsPerStrip = 1; /* dummy value */

        nBlockXSize = nRasterXSize;
        nBlockYSize = MIN(nRowsPerStrip,nYSize);
    }
        
    nBlocksPerBand =
        ((nYSize + nBlockYSize - 1) / nBlockYSize)
      * ((nXSize + nBlockXSize  - 1) / nBlockXSize);
        
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        SetBand( iBand+1, new GTiffRasterBand( this, iBand+1 ) );
    }

/* -------------------------------------------------------------------- */
/*      Get the transform.                                              */
/* -------------------------------------------------------------------- */
    double	*padfTiePoints, *padfScale;
    int16	nCount;

    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    
    if( TIFFGetField(hTIFF,TIFFTAG_GEOPIXELSCALE,&nCount,&padfScale )
        && nCount >= 2 )
    {
        adfGeoTransform[1] = padfScale[0];
        adfGeoTransform[5] = - ABS(padfScale[1]);

        
        if( TIFFGetField(hTIFF,TIFFTAG_GEOTIEPOINTS,&nCount,&padfTiePoints )
            && nCount >= 6 )
        {
            adfGeoTransform[0] =
                padfTiePoints[3] - padfTiePoints[0] * adfGeoTransform[1];
            adfGeoTransform[3] =
                padfTiePoints[4] - padfTiePoints[1] * adfGeoTransform[5];

            bGeoTransformValid = TRUE;
        }
    }

    else if( TIFFGetField(hTIFF,TIFFTAG_GEOTIEPOINTS,&nCount,&padfTiePoints )
            && nCount >= 6 )
    {
        nGCPCount = nCount / 6;
        pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),nGCPCount);
        
        for( int iGCP = 0; iGCP < nGCPCount; iGCP++ )
        {
            char	szID[32];

            sprintf( szID, "%d", iGCP+1 );
            pasGCPList[iGCP].pszId = CPLStrdup( szID );
            pasGCPList[iGCP].pszInfo = "";
            pasGCPList[iGCP].dfGCPPixel = padfTiePoints[iGCP*6+0];
            pasGCPList[iGCP].dfGCPLine = padfTiePoints[iGCP*6+1];
            pasGCPList[iGCP].dfGCPX = padfTiePoints[iGCP*6+3];
            pasGCPList[iGCP].dfGCPY = padfTiePoints[iGCP*6+4];
            pasGCPList[iGCP].dfGCPZ = padfTiePoints[iGCP*6+5];
        }
    }
        
/* -------------------------------------------------------------------- */
/*      Capture the color table if there is one.                        */
/* -------------------------------------------------------------------- */
    unsigned short	*panRed, *panGreen, *panBlue;

    if( nPhotometric != PHOTOMETRIC_PALETTE 
        || TIFFGetField( hTIFF, TIFFTAG_COLORMAP, 
                         &panRed, &panGreen, &panBlue) == 0 )
    {
        poColorTable = NULL;
    }
    else
    {
        int	nColorCount;
        GDALColorEntry oEntry;

        poColorTable = new GDALColorTable();

        nColorCount = 1 << nBitsPerSample;

        for( int iColor = nColorCount - 1; iColor >= 0; iColor-- )
        {
            oEntry.c1 = panRed[iColor] / 256;
            oEntry.c2 = panGreen[iColor] / 256;
            oEntry.c3 = panBlue[iColor] / 256;
            oEntry.c4 = 255;

            poColorTable->SetColorEntry( iColor, &oEntry );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Capture the projection.                                         */
/* -------------------------------------------------------------------- */
    GTIF 	*hGTIF;
    GTIFDefn	sGTIFDefn;
    
    hGTIF = GTIFNew(hTIFF);

    if( GTIFGetDefn( hGTIF, &sGTIFDefn ) )
    {
//        pszProjection = GTIFGetProj4Defn( &sGTIFDefn );
        pszProjection = GTIFGetOGISDefn( &sGTIFDefn );
    }
    else
    {
        pszProjection = CPLStrdup( "" );
    }
    
    GTIFFree( hGTIF );

/* -------------------------------------------------------------------- */
/*      Capture some other potentially interesting information.         */
/* -------------------------------------------------------------------- */
    char	*pszText;

    if( TIFFGetField( hTIFF, TIFFTAG_DOCUMENTNAME, &pszText ) )
        SetMetadataItem( "TIFFTAG_DOCUMENTNAME",  pszText );

    if( TIFFGetField( hTIFF, TIFFTAG_IMAGEDESCRIPTION, &pszText ) )
        SetMetadataItem( "TIFFTAG_IMAGEDESCRIPTION", pszText );

    if( TIFFGetField( hTIFF, TIFFTAG_SOFTWARE, &pszText ) )
        SetMetadataItem( "TIFFTAG_SOFTWARE", pszText );

    if( TIFFGetField( hTIFF, TIFFTAG_DATETIME, &pszText ) )
        SetMetadataItem(  "TIFFTAG_DATETIME", pszText );

/* -------------------------------------------------------------------- */
/*      If this is a "base" raster, we should scan for any              */
/*      associated overviews.                                           */
/* -------------------------------------------------------------------- */
    if( bBase )
    {
        while( TIFFReadDirectory( hTIFF ) != 0 )
        {
            uint32	nThisDir = TIFFCurrentDirOffset(hTIFF);
            uint32	nSubType;

            if( TIFFGetField(hTIFF, TIFFTAG_SUBFILETYPE, &nSubType)
                && (nSubType & FILETYPE_REDUCEDIMAGE) )
            {
                GTiffDataset	*poODS;

                poODS = new GTiffDataset();
                if( poODS->OpenOffset( hTIFF, nThisDir, FALSE, 
                                       eAccess ) != CE_None 
                    || poODS->GetRasterCount() != GetRasterCount() )
                {
                    delete poODS;
                }
                else
                {
                    CPLDebug( "GTiff", "Opened %dx%d overview.\n", 
                            poODS->GetRasterXSize(), poODS->GetRasterYSize());
                    nOverviewCount++;
                    papoOverviewDS = (GTiffDataset **)
                        CPLRealloc(papoOverviewDS, 
                                   nOverviewCount * (sizeof(void*)));
                    papoOverviewDS[nOverviewCount-1] = poODS;
                }
            }

            SetDirectory( nThisDir );
        }
    }

    return( CE_None );
}

/************************************************************************/
/*                               Create()                               */
/*                                                                      */
/*      Create a new GeoTIFF or TIFF file.                              */
/************************************************************************/

GDALDataset *GTiffDataset::Create( const char * pszFilename,
                                   int nXSize, int nYSize, int nBands,
                                   GDALDataType eType,
                                   char **papszParmList )

{
    GTiffDataset *	poDS;
    TIFF		*hTIFF;
    int                 nBlockXSize = 0, nBlockYSize = 0;
    int                 bTiled = FALSE;
    int                 nCompression = COMPRESSION_NONE;
    
/* -------------------------------------------------------------------- */
/*	Setup values based on options.					*/
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue(papszParmList,"TILED") != NULL )
        bTiled = TRUE;

    if( CSLFetchNameValue(papszParmList,"BLOCKXSIZE")  != NULL )
        nBlockXSize = atoi(CSLFetchNameValue(papszParmList,"BLOCKXSIZE"));

    if( CSLFetchNameValue(papszParmList,"BLOCKYSIZE")  != NULL )
        nBlockYSize = atoi(CSLFetchNameValue(papszParmList,"BLOCKYSIZE"));

    if( CSLFetchNameValue(papszParmList,"COMPRESS")  != NULL )
    {
        if( EQUAL(CSLFetchNameValue(papszParmList,"COMPRESS"),"JPEG") )
            nCompression = COMPRESSION_JPEG;
        else if( EQUAL(CSLFetchNameValue(papszParmList,"COMPRESS"),"LZW") )
            nCompression = COMPRESSION_LZW;
        else if( EQUAL(CSLFetchNameValue(papszParmList,"COMPRESS"),"PACKBITS"))
            nCompression = COMPRESSION_PACKBITS;
        else if( EQUAL(CSLFetchNameValue(papszParmList,"COMPRESS"),"DEFLATE")
               || EQUAL(CSLFetchNameValue(papszParmList,"COMPRESS"),"ZIP"))
            nCompression = COMPRESSION_DEFLATE;
        else
            CPLError( CE_Warning, CPLE_IllegalArg, 
                      "COMPRESS=%s value not recognised, ignoring.", 
                      CSLFetchNameValue(papszParmList,"COMPRESS") );
    }

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    hTIFF = XTIFFOpen( pszFilename, "w+" );
    if( hTIFF == NULL )
    {
        if( CPLGetLastErrorNo() == 0 )
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Attempt to create new tiff file `%s'\n"
                      "failed in XTIFFOpen().\n",
                      pszFilename );
    }

/* -------------------------------------------------------------------- */
/*      Create the new GTiffDataset object.                             */
/* -------------------------------------------------------------------- */
    poDS = new GTiffDataset();
    poDS->hTIFF = hTIFF;
    poDS->poDriver = poGTiffDriver;

    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->eAccess = GA_Update;
    poDS->bNewDataset = TRUE;
    poDS->pszProjection = CPLStrdup("");
        
/* -------------------------------------------------------------------- */
/*      Setup some standard flags.                                      */
/* -------------------------------------------------------------------- */
    TIFFSetField( hTIFF, TIFFTAG_COMPRESSION, nCompression );
    TIFFSetField( hTIFF, TIFFTAG_IMAGEWIDTH, nXSize );
    TIFFSetField( hTIFF, TIFFTAG_IMAGELENGTH, nYSize );
    TIFFSetField( hTIFF, TIFFTAG_BITSPERSAMPLE,
                  GDALGetDataTypeSize( eType ) );
    poDS->nBitsPerSample = GDALGetDataTypeSize( eType );

    if( eType == GDT_Int16 || eType == GDT_Int32 )
    {
        TIFFSetField( hTIFF, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_INT );
    }
    else if( eType == GDT_Float32 || eType == GDT_Float64 )
    {
        TIFFSetField( hTIFF, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP );
    }

    TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, nBands );

    if( nBands > 1 )
    {
        TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG, PLANARCONFIG_SEPARATE );
        poDS->nPlanarConfig = PLANARCONFIG_SEPARATE;
    }
    else
    {
        TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG );
        poDS->nPlanarConfig = PLANARCONFIG_CONTIG;
    }

    if( nBands == 3 )
        poDS->nPhotometric = PHOTOMETRIC_RGB;
    else
        poDS->nPhotometric = PHOTOMETRIC_MINISBLACK;
    
    TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, poDS->nPhotometric);

    if( bTiled )
    {
        if( nBlockXSize == 0 )
            nBlockXSize = 256;
        
        if( nBlockYSize == 0 )
            nBlockYSize = 256;

        TIFFSetField( hTIFF, TIFFTAG_TILEWIDTH, nBlockXSize );
        TIFFSetField( hTIFF, TIFFTAG_TILELENGTH, nBlockYSize );

        poDS->nBlockXSize = nBlockXSize;
        poDS->nBlockYSize = nBlockYSize;
    }
    else
    {
        if( nBlockYSize == 0 )
            poDS->nRowsPerStrip = MIN(nYSize,
                                      (int)TIFFDefaultStripSize(hTIFF,0));
        else
            poDS->nRowsPerStrip = nBlockYSize;

        TIFFSetField( hTIFF, TIFFTAG_ROWSPERSTRIP, poDS->nRowsPerStrip );

        poDS->nBlockXSize = nXSize;
        poDS->nBlockYSize = poDS->nRowsPerStrip;
    }
    
    poDS->nBlocksPerBand =
        ((nYSize + poDS->nBlockYSize - 1) / poDS->nBlockYSize)
        * ((nXSize + poDS->nBlockXSize - 1) / poDS->nBlockXSize);

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int		iBand;

    for( iBand = 0; iBand < nBands; iBand++ )
    {
        poDS->SetBand( iBand+1, new GTiffRasterBand( poDS, iBand+1 ) );
    }

    return( poDS );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *GTiffDataset::GetProjectionRef()

{
    return( pszProjection );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

CPLErr GTiffDataset::SetProjection( const char * pszNewProjection )

{
    if( !EQUALN(pszNewProjection,"GEOGCS",6)
        && !EQUALN(pszNewProjection,"PROJCS",6)
        && !EQUAL(pszNewProjection,"") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "Only OGC WKT Projections supported for writing to GeoTIFF.\n"
                "%s not supported.",
                  pszNewProjection );
        
        return CE_Failure;
    }
    
    CPLFree( pszProjection );
    pszProjection = CPLStrdup( pszNewProjection );

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GTiffDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );

    if( bGeoTransformValid )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr GTiffDataset::SetGeoTransform( double * padfTransform )

{
    if( bNewDataset )
    {
        memcpy( adfGeoTransform, padfTransform, sizeof(double)*6 );
        bGeoTransformValid = TRUE;
        return( CE_None );
    }
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
      "SetGeoTransform() is only supported on newly created GeoTIFF files." );
        return CE_Failure;
    }
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int GTiffDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *GTiffDataset::GetGCPProjection()

{
    if( nGCPCount > 0 )
        return pszProjection;
    else
        return "";
}

/************************************************************************/
/*                               GetGCP()                               */
/************************************************************************/

const GDAL_GCP *GTiffDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                        GTiffWarningHandler()                         */
/************************************************************************/
void
GTiffWarningHandler(const char* module, const char* fmt, va_list ap )
{
    char	szModFmt[128];

    if( strstr(fmt,"unknown field") != NULL )
        return;

    sprintf( szModFmt, "%s:%s", module, fmt );
    CPLErrorV( CE_Warning, CPLE_AppDefined, szModFmt, ap );
}

/************************************************************************/
/*                        GTiffWarningHandler()                         */
/************************************************************************/
void
GTiffErrorHandler(const char* module, const char* fmt, va_list ap )
{
    char	szModFmt[128];

    sprintf( szModFmt, "%s:%s", module, fmt );
    CPLErrorV( CE_Failure, CPLE_AppDefined, szModFmt, ap );
}

/************************************************************************/
/*                          GDALRegister_GTiff()                        */
/************************************************************************/

void GDALRegister_GTiff()

{
    GDALDriver	*poDriver;

    if( poGTiffDriver == NULL )
    {
        poGTiffDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "GTiff";
        poDriver->pszLongName = "GeoTIFF";
        poDriver->pszHelpTopic = "frmt_gtiff.html";
        
        poDriver->pfnOpen = GTiffDataset::Open;
        poDriver->pfnCreate = GTiffDataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );

        TIFFSetWarningHandler( GTiffWarningHandler );
        TIFFSetErrorHandler( GTiffErrorHandler );
    }
}
