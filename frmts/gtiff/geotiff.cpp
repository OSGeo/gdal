/******************************************************************************
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
 * geotiff.cpp
 *
 * The GeoTIFF driver implemenation.
 * 
 * $Log$
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
    int		nStripsPerBand;

    int		nLoadedStrip;
    int		bLoadedStripDirty;
    GByte	*pabyStripBuf;

    char	*pszProjection;
    double	adfGeoTransform[6];

  public:
                 GTiffDataset();
                 ~GTiffDataset();

    virtual const char *GetProjectionRef(void);
    virtual CPLErr GetGeoTransform( double * );

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
    if( poDS->nBitsPerSample <= 8 )
        eDataType = GDT_Byte;
    else if( poDS->nBitsPerSample <= 16 )
        eDataType = GDT_UInt16;
    else
        eDataType = GDT_Unknown;

/* -------------------------------------------------------------------- */
/*	Currently only works for strips 				*/
/* -------------------------------------------------------------------- */
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = poDS->nRowsPerStrip;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GTiffRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                    void * pImage )

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;
    int			nStripBufSize, nStripId;
    CPLErr		eErr = CE_None;

    CPLAssert( nBlockXOff == 0 );

    nStripBufSize = TIFFStripSize( poGDS->hTIFF );

    if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
        nStripId = nBlockYOff + (nBand-1) * poGDS->nStripsPerBand;
    else
        nStripId = nBlockYOff;
    
/* -------------------------------------------------------------------- */
/*	Handle the case of a strip in a writable file that doesn't	*/
/*	exist yet, but that we want to read.  Just set to zeros and	*/
/*	return.								*/
/* -------------------------------------------------------------------- */
    if( poGDS->eAccess == GA_Update
        && (((int) poGDS->hTIFF->tif_dir.td_nstrips) <= nStripId
            || poGDS->hTIFF->tif_dir.td_stripbytecount[nStripId] == 0) )
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
        if( TIFFReadEncodedStrip( poGDS->hTIFF, nStripId, pImage,
                                  nStripBufSize ) == -1 )
        {
            memset( pImage, 0, nStripBufSize );
            CPLError( CE_Failure, CPLE_AppDefined,
                      "TIFFReadEncodedStrip() failed.\n" );
                      
            eErr = CE_Failure;
        }

        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Allocate a temporary buffer for this strip.                     */
/* -------------------------------------------------------------------- */
    if( poGDS->pabyStripBuf == NULL )
    {
        poGDS->pabyStripBuf = (GByte *) VSICalloc( 1, nStripBufSize );
        if( poGDS->pabyStripBuf == NULL )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory,
                   "Unable to allocate %d bytes for a temporary strip buffer\n"
                      "in GeoTIFF driver.",
                      nStripBufSize );
            
            return( CE_Failure );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Read the strip                                                  */
/* -------------------------------------------------------------------- */
    if( poGDS->nLoadedStrip != nStripId
        && TIFFReadEncodedStrip(poGDS->hTIFF, nStripId, poGDS->pabyStripBuf,
                                nStripBufSize) == -1 )
    {
        /* Once TIFFError() is properly hooked, this can go away */
        CPLError( CE_Failure, CPLE_AppDefined,
                  "TIFFReadEncodedStrip() failed." );

        memset( poGDS->pabyStripBuf, 0, nStripBufSize );
        
        eErr = CE_Failure;
    }

    poGDS->nLoadedStrip = nStripId;
                              
/* -------------------------------------------------------------------- */
/*      Handle simple case of eight bit data, and pixel interleaving.   */
/* -------------------------------------------------------------------- */
    if( poGDS->nBitsPerSample == 8 )
    {
        int	i;
        GByte	*pabyImage;
        
        pabyImage = poGDS->pabyStripBuf + nBand - 1;
        
        for( i = 0; i < (int) (poGDS->nRasterXSize*poGDS->nRowsPerStrip); i++ )
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
/************************************************************************/

CPLErr GTiffRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                     void * pImage )

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;
    int		nStripId, nStripBufSize;

    CPLAssert( poGDS != NULL
               && nBlockXOff == 0
               && nBlockYOff >= 0
               && pImage != NULL );

    CPLAssert( nBlockXOff == 0 );
    CPLAssert( eDataType == GDT_Byte );

    nStripBufSize = TIFFStripSize( poGDS->hTIFF );
    nStripId = nBlockYOff + (nBand-1) * poGDS->nStripsPerBand;
    
    TIFFWriteEncodedStrip( poGDS->hTIFF, nStripId, pImage, nStripBufSize );
    
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
    nLoadedStrip = -1;
    bLoadedStripDirty = FALSE;
    pabyStripBuf = NULL;
    hTIFF = NULL;
}

/************************************************************************/
/*                           ~GTiffDataset()                            */
/************************************************************************/

GTiffDataset::~GTiffDataset()

{
    FlushCache();

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

    CPLFree( pabyStripBuf );
    pabyStripBuf = NULL;
    nLoadedStrip = -1;
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
    if( !TIFFGetField( hTIFF, TIFFTAG_ROWSPERSTRIP, &(poDS->nRowsPerStrip) ) )
        poDS->nRowsPerStrip = 1; /* dummy value */

    poDS->nStripsPerBand = nYSize / poDS->nRowsPerStrip;
        
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
        poDS->adfGeoTransform[5] = padfScale[1];
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
        poDS->pszProjection = GTIFGetProj4Defn( &sGTIFDefn );
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
        
/* -------------------------------------------------------------------- */
/*      Setup some standard flags.                                      */
/* -------------------------------------------------------------------- */
    TIFFSetField( hTIFF, TIFFTAG_IMAGEWIDTH, nXSize );
    TIFFSetField( hTIFF, TIFFTAG_IMAGELENGTH, nYSize );
    TIFFSetField( hTIFF, TIFFTAG_BITSPERSAMPLE,
                  GDALGetDataTypeSize( eType ) );
    poDS->nBitsPerSample = GDALGetDataTypeSize( eType );

    TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, nBands );

    TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG );
    poDS->nPlanarConfig = PLANARCONFIG_CONTIG;

    if( nBands == 3 )
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB );
    else
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK );

    poDS->nRowsPerStrip = TIFFDefaultStripSize(hTIFF,0);
    TIFFSetField( hTIFF, TIFFTAG_ROWSPERSTRIP, poDS->nRowsPerStrip );
    
    poDS->nStripsPerBand = nYSize / poDS->nRowsPerStrip;
    
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
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GTiffDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );

    return( CE_None );
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
