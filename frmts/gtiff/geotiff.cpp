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
 * Revision 1.2  1998/12/03 18:37:58  warmerda
 * Use CPLErr, not GBSErr.
 *
 * Revision 1.1  1998/11/29 22:41:12  warmerda
 * New
 *
 */

#include "tiffio.h"
#include "gdal_priv.h"

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

    uint32	nPlanarConfig;
    uint32	nSamplesPerPixel;
    uint32	nBitsPerSample;
    
  public:
    static GDALDataset *Open( GDALOpenInfo * );
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

                   GTiffRasterBand::GTiffRasterBand( GTiffDataset *, int );
    
    // should override RasterIO eventually.
    
    virtual CPLErr ReadBlock( int, int, void * );
    virtual CPLErr WriteBlock( int, int, void * ); 
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
/*      Set the access flag.  For now we set it the same as the         */
/*      whole dataset, but eventually this should take account of       */
/*      locked channels, or read-only secondary data files.             */
/* -------------------------------------------------------------------- */
    /* ... */
}

/************************************************************************/
/*                             ReadBlock()                              */
/************************************************************************/

CPLErr GTiffRasterBand::ReadBlock( int nBlockXOff, int nBlockYOff,
                                 void * pImage )

{
    GTiffDataset	*poGTiff_DS = (GTiffDataset *) poDS;

    

    return CE_None;
}

/************************************************************************/
/*                             WriteBlock()                             */
/************************************************************************/

CPLErr GTiffRasterBand::WriteBlock( int nBlockXOff, int nBlockYOff,
                                 void * pImage )

{
    GTiffDataset	*poGTiff_DS = (GTiffDataset *) poDS;

    

    return CE_None;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GTiffDataset::Open( GDALOpenInfo * poOpenInfo )

{
    TIFF	*hTIFF;
    uint32	nXSize, nYSize;

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_ReadOnly )
        hTIFF = TIFFOpen( poOpenInfo->pszFilename, "r" );
    else
        hTIFF = TIFFOpen( poOpenInfo->pszFilename, "r+" );
    
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
    
/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    TIFFGetField( hTIFF, TIFFTAG_IMAGEWIDTH, &nXSize );
    TIFFGetField( hTIFF, TIFFTAG_IMAGELENGTH, &nYSize );
    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;

    if( !TIFFGetField(hTIFF, TIFFTAG_SAMPLESPERPIXEL,
                      &(poDS->nSamplesPerPixel)) )
        poDS->nSamplesPerPixel = 1;
    
    poDS->nBands = poDS->nSamplesPerPixel;
    
    if( !TIFFGetField(hTIFF, TIFFTAG_BITSPERSAMPLE, &(poDS->nBitsPerSample)) )
        poDS->nBitsPerSample = 1;
    
    if( !TIFFGetField( hTIFF, TIFFTAG_IMAGEWIDTH, &(poDS->nPlanarConfig) ) )
        poDS->nPlanarConfig = PLANARCONFIG_CONTIG;
    
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

    return( poDS );
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

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

