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

    uint16	nPlanarConfig;
    uint16	nSamplesPerPixel;
    uint16	nBitsPerSample;
    uint32	nRowsPerStrip;
    
    int		nBlocksPerBand;

    uint32      nBlockXSize;
    uint32	nBlockYSize;

    int		nLoadedBlock;		/* or tile */
    int		bLoadedStripDirty;
    GByte	*pabyBlockBuf;

    char	*pszProjection;
    double	adfGeoTransform[6];

    int		bNewDataset;            /* product of Create() */

    void	WriteGeoTIFFInfo();

  public:
                 GTiffDataset();
                 ~GTiffDataset();

    virtual const char *GetProjectionRef(void);
    virtual CPLErr SetProjection( const char * );
    virtual CPLErr GetGeoTransform( double * );
    virtual CPLErr SetGeoTransform( double * );

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
    int16		nCount;
    uint16		nSampleFormat;

    if( !TIFFGetField(poDS->hTIFF,TIFFTAG_SAMPLEFORMAT,
                      &nCount,&nSampleFormat) )
        nSampleFormat = SAMPLEFORMAT_UINT;
        
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
    if( !TIFFIsTiled(poGDS->hTIFF)
        && poGDS->eAccess == GA_Update
        && (((int) poGDS->hTIFF->tif_dir.td_nstrips) <= nBlockId
            || poGDS->hTIFF->tif_dir.td_stripbytecount[nBlockId] == 0) )
    {
        memset( pImage, 0,
                nBlockXSize * nBlockYSize
                * GDALGetDataTypeSize(eDataType) / 8 );
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Handle simple case (separate, onesampleperpixel), and eight     */
/*      bit data.                                                       */
/* -------------------------------------------------------------------- */
    if( poGDS->nBitsPerSample == 8
        && (poGDS->nBands == 1
            || poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE) )
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

    CPLAssert( poGDS != NULL
               && nBlockXOff == 0
               && nBlockYOff >= 0
               && pImage != NULL );

    CPLAssert( nBlockXOff == 0 );

    nBlockBufSize = TIFFStripSize( poGDS->hTIFF );
    nBlockId = nBlockYOff + (nBand-1) * poGDS->nBlocksPerBand;
    
    TIFFWriteEncodedStrip( poGDS->hTIFF, nBlockId, pImage, nBlockBufSize );
    
    return CE_Failure;
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
}

/************************************************************************/
/*                           ~GTiffDataset()                            */
/************************************************************************/

GTiffDataset::~GTiffDataset()

{
    FlushCache();

    WriteGeoTIFFInfo();

    XTIFFClose( hTIFF );
    hTIFF = NULL;
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
/*                          WriteGeoTIFFInfo()                          */
/************************************************************************/

void GTiffDataset::WriteGeoTIFFInfo()

{
/* -------------------------------------------------------------------- */
/*      If the geotransform is the default, don't bother writing it.    */
/* -------------------------------------------------------------------- */
    if( adfGeoTransform[0] == 0.0 && adfGeoTransform[1] == 1.0
        && adfGeoTransform[2] == 0.0 && adfGeoTransform[3] == 0.0
        && adfGeoTransform[4] == 0.0 && adfGeoTransform[5] == -1.0 )
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
/*                                Open()                                */
/************************************************************************/

GDALDataset *GTiffDataset::Open( GDALOpenInfo * poOpenInfo )

{
    TIFF	*hTIFF;
    uint32	nXSize, nYSize;
    uint16	nSamplesPerPixel;

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

    TIFFReadDirectory( hTIFF );

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GTiffDataset 	*poDS;

    poDS = new GTiffDataset();

    poDS->hTIFF = hTIFF;
    poDS->poDriver = poGTiffDriver;
    poDS->pszProjection = NULL;

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    TIFFGetField( hTIFF, TIFFTAG_IMAGEWIDTH, &nXSize );
    TIFFGetField( hTIFF, TIFFTAG_IMAGELENGTH, &nYSize );
    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;

    if( !TIFFGetField(hTIFF, TIFFTAG_SAMPLESPERPIXEL, &nSamplesPerPixel ) )
        poDS->nBands = 1;
    else
        poDS->nBands = nSamplesPerPixel;
    
    if( !TIFFGetField(hTIFF, TIFFTAG_BITSPERSAMPLE, &(poDS->nBitsPerSample)) )
        poDS->nBitsPerSample = 1;
    
    if( !TIFFGetField( hTIFF, TIFFTAG_PLANARCONFIG, &(poDS->nPlanarConfig) ) )
        poDS->nPlanarConfig = PLANARCONFIG_CONTIG;
    
/* -------------------------------------------------------------------- */
/*      Get strip layout (won't work for tiled images!)                 */
/* -------------------------------------------------------------------- */
    if( TIFFIsTiled(poDS->hTIFF) )
    {
        TIFFGetField( hTIFF, TIFFTAG_TILEWIDTH, &(poDS->nBlockXSize) );
        TIFFGetField( hTIFF, TIFFTAG_TILELENGTH, &(poDS->nBlockYSize) );
    }
    else
    {
        if( !TIFFGetField( hTIFF, TIFFTAG_ROWSPERSTRIP,
                           &(poDS->nRowsPerStrip) ) )
            poDS->nRowsPerStrip = 1; /* dummy value */

        poDS->nBlockXSize = poDS->nRasterXSize;
        poDS->nBlockYSize = poDS->nRowsPerStrip;
    }
        
    poDS->nBlocksPerBand =
        ((nYSize + poDS->nBlockYSize - 1) / poDS->nBlockYSize)
      * ((nXSize + poDS->nBlockXSize  - 1) / poDS->nBlockXSize);
        
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int		iBand;

    poDS->papoBands = (GDALRasterBand **) VSICalloc(sizeof(GDALRasterBand *),
                                                    poDS->nBands);

    for( iBand = 0; iBand < poDS->nBands; iBand++ )
    {
        poDS->papoBands[iBand] = new GTiffRasterBand( poDS, iBand+1 );
    }

/* -------------------------------------------------------------------- */
/*      Get the transform.                                              */
/* -------------------------------------------------------------------- */
    double	*padfTiePoints, *padfScale;
    int16	nCount;

    poDS->adfGeoTransform[0] = 0.0;
    poDS->adfGeoTransform[1] = 1.0;
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[3] = 0.0;
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = 1.0;
    
    if( TIFFGetField(hTIFF,TIFFTAG_GEOPIXELSCALE,&nCount,&padfScale )
        && nCount >= 2 )
    {
        poDS->adfGeoTransform[1] = padfScale[0];
        poDS->adfGeoTransform[5] = - ABS(padfScale[1]);
    }
        
    if( TIFFGetField(hTIFF,TIFFTAG_GEOTIEPOINTS,&nCount,&padfTiePoints )
        && nCount >= 6 )
    {
        poDS->adfGeoTransform[0] =
            padfTiePoints[3] - padfTiePoints[0] * poDS->adfGeoTransform[1];
        poDS->adfGeoTransform[3] =
            padfTiePoints[4] - padfTiePoints[1] * poDS->adfGeoTransform[5];
    }
        
/* -------------------------------------------------------------------- */
/*	Try and print out some useful names from the GeoTIFF file.	*/
/* -------------------------------------------------------------------- */
    GTIF 	*hGTIF;
    GTIFDefn	sGTIFDefn;
    
    hGTIF = GTIFNew(hTIFF);

    if( GTIFGetDefn( hGTIF, &sGTIFDefn ) )
    {
//        poDS->pszProjection = GTIFGetProj4Defn( &sGTIFDefn );
        poDS->pszProjection = GTIFGetOGISDefn( &sGTIFDefn );
    }
    else
    {
        poDS->pszProjection = CPLStrdup( "" );
    }
    
    GTIFFree( hGTIF );

    return( poDS );
}

/************************************************************************/
/*                               Create()                               */
/*                                                                      */
/*      Create a new GeoTIFF or TIFF file.                              */
/************************************************************************/

GDALDataset *GTiffDataset::Create( const char * pszFilename,
                                   int nXSize, int nYSize, int nBands,
                                   GDALDataType eType,
                                   char ** /* notdef: papszParmList */ )

{
    GTiffDataset *	poDS;
    TIFF		*hTIFF;
    
/* -------------------------------------------------------------------- */
/*	Setup values based on options.					*/
/* -------------------------------------------------------------------- */

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

    TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG );
    poDS->nPlanarConfig = PLANARCONFIG_CONTIG;

    if( nBands == 3 )
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB );
    else
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK );

    poDS->nRowsPerStrip = TIFFDefaultStripSize(hTIFF,0);
    TIFFSetField( hTIFF, TIFFTAG_ROWSPERSTRIP, poDS->nRowsPerStrip );
    
    poDS->nBlocksPerBand =
        ((nYSize + poDS->nRowsPerStrip - 1) / poDS->nRowsPerStrip);

    poDS->nBlockXSize = nXSize;
    poDS->nBlockYSize = poDS->nRowsPerStrip;
    
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int		iBand;

    poDS->nBands = nBands;
    poDS->papoBands = (GDALRasterBand **) VSICalloc(sizeof(GDALRasterBand *),
                                                    nBands);

    for( iBand = 0; iBand < nBands; iBand++ )
    {
        poDS->papoBands[iBand] = new GTiffRasterBand( poDS, iBand+1 );
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
        && !EQUALN(pszNewProjection,"PROJCS",6) )
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

    return( CE_None );
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr GTiffDataset::SetGeoTransform( double * padfTransform )

{
    if( bNewDataset )
    {
        memcpy( adfGeoTransform, padfTransform, sizeof(double)*6 );
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
        
        poDriver->pfnOpen = GTiffDataset::Open;
        poDriver->pfnCreate = GTiffDataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
