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
 * gxfdataset.cpp
 *
 * The GXF driver, and dataset implementation.
 * 
 * $Log$
 * Revision 1.2  1998/12/15 19:06:49  warmerda
 * IReadBlock(), and GXFGetRawInfo()
 *
 * Revision 1.1  1998/12/06 02:53:22  warmerda
 * New
 *
 */

#include "gxfopen.h"
#include "gdal_priv.h"

static GDALDriver	*poGXFDriver = NULL;

CPL_C_START
void	GDALRegister_GXF(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				GXFDataset				*/
/* ==================================================================== */
/************************************************************************/

class GXFRasterBand;

class GXFDataset : public GDALDataset
{
    friend	GXFRasterBand;
    
    GXFHandle	hGXF;

  public:
    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            GXFRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class GXFRasterBand : public GDALRasterBand
{
    friend	GXFDataset;
    
  public:

    		GXFRasterBand( GXFDataset *, int );
    
    virtual CPLErr IReadBlock( int, int, void * );
};


/************************************************************************/
/*                           GXFRasterBand()                            */
/************************************************************************/

GXFRasterBand::GXFRasterBand( GXFDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;
    
    eDataType = GDT_Float32;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GXFRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    GXFDataset	*poGXF_DS = (GXFDataset *) poDS;
    double	*padfBuffer;
    float	*pafBuffer = (float *) pImage;
    int		i;
    CPLErr	eErr;

    CPLAssert( nBlockXOff == 0 );

    padfBuffer = (double *) CPLMalloc(sizeof(double) * nBlockXSize);
    eErr = GXFGetRawScanline( poGXF_DS->hGXF, nBlockYOff, padfBuffer );
    
    for( i = 0; i < nBlockXSize; i++ )
        pafBuffer[i] = padfBuffer[i];

    CPLFree( padfBuffer );
    
    return eErr;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GXFDataset::Open( GDALOpenInfo * poOpenInfo )

{
    GXFHandle	hGXF;
    
/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    hGXF = GXFOpen( poOpenInfo->pszFilename );
    
    if( hGXF == NULL )
        return( NULL );

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GXFDataset 	*poDS;

    poDS = new GXFDataset();

    poDS->hGXF = hGXF;
    poDS->poDriver = poGXFDriver;
    
/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    GXFGetRawInfo( hGXF, &(poDS->nRasterXSize), &(poDS->nRasterYSize), NULL,
                   NULL, NULL, NULL );
    
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->nBands = 1;
    poDS->papoBands = (GDALRasterBand **)VSICalloc(sizeof(GDALRasterBand *),1);

    poDS->papoBands[0] = new GXFRasterBand( poDS, 1 );

    return( poDS );
}

/************************************************************************/
/*                          GDALRegister_GTiff()                        */
/************************************************************************/

void GDALRegister_GXF()

{
    GDALDriver	*poDriver;

    if( poGXFDriver == NULL )
    {
        poGXFDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "GXF";
        poDriver->pszLongName = "GeoSoft Grid Exchange Format";
        
        poDriver->pfnOpen = GXFDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

