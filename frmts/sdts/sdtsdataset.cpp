/******************************************************************************
 * $Id$
 *
 * Project:  CEOS Translator
 * Purpose:  GDALDataset driver for SDTS Raster translator.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 * Revision 1.1  1999/06/03 13:46:07  warmerda
 * New
 *
 */

#include "sdts_al.h"
#include "gdal_priv.h"

static GDALDriver	*poSDTSDriver = NULL;

CPL_C_START
void	GDALRegister_SDTS(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				SDTSDataset				*/
/* ==================================================================== */
/************************************************************************/

class SDTSRasterBand;

class SDTSDataset : public GDALDataset
{
    friend	SDTSRasterBand;
    
    SDTSTransfer *poTransfer;

  public:
    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            SDTSRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class SDTSRasterBand : public GDALRasterBand
{
    friend	SDTSDataset;

    SDTSRasterReader *poRL;
    
  public:

    		SDTSRasterBand( SDTSDataset *, int, SDTSRasterReader * );
    
    virtual CPLErr IReadBlock( int, int, void * );
};


/************************************************************************/
/*                           SDTSRasterBand()                            */
/************************************************************************/

SDTSRasterBand::SDTSRasterBand( SDTSDataset *poDS, int nBand,
                                SDTSRasterReader * poRL )

{
    this->poDS = poDS;
    this->nBand = nBand;
    this->poRL = poRL;

    CPLAssert( poRL->GetRasterType() == 1 );
    eDataType = GDT_Int16;

    nBlockXSize = poRL->GetBlockXSize();
    nBlockYSize = poRL->GetBlockYSize();
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr SDTSRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    if( poRL->GetBlock( nBlockXOff, nBlockYOff, pImage ) )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *SDTSDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		i;
    
/* -------------------------------------------------------------------- */
/*      Before trying SDTSOpen() we first verify that the first         */
/*      record is in fact a SDTS file descriptor record.                */
/* -------------------------------------------------------------------- */
    char	*pachLeader = (char *) poOpenInfo->pabyHeader;
    
    if( poOpenInfo->fp == NULL || poOpenInfo->nHeaderBytes < 24 )
        return NULL;

    if( pachLeader[5] != '1' && pachLeader[5] != '2' && pachLeader[5] != '3' )
        return NULL;

    if( pachLeader[6] != 'L' )
        return NULL;

    if( pachLeader[8] != '1' && pachLeader[8] != ' ' )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    SDTSTransfer	*poTransfer = new SDTSTransfer;
    
    if( !poTransfer->Open( poOpenInfo->pszFilename ) )
    {
        delete poTransfer;
        return NULL;
    }
        
/* -------------------------------------------------------------------- */
/*      Find the first raster layer.  If there are none, abort          */
/*      returning an error.                                             */
/* -------------------------------------------------------------------- */
    SDTSRasterReader	*poRL = NULL;

    for( i = 0; i < poTransfer->GetLayerCount(); i++ )
    {
        if( poTransfer->GetLayerType( i ) == SLTRaster )
        {
            printf( "Get raster reader.\n" );
            poRL = poTransfer->GetLayerRasterReader( i );
            break;
        }
    }

    if( poRL == NULL )
    {
        delete poTransfer;
        
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s is an SDTS transfer, but has no raster cell layers.\n"
                  "Perhaps it is a vector transfer?\n",
                  poOpenInfo->pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Initialize a corresponding GDALDataset.                         */
/* -------------------------------------------------------------------- */
    SDTSDataset	*poDS = new SDTSDataset();

    poDS->poTransfer = poTransfer;
    poDS->poDriver = poSDTSDriver;
    
/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = poRL->GetXSize();
    poDS->nRasterYSize = poRL->GetYSize();
    
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->nBands = 1;
    poDS->papoBands = (GDALRasterBand **)
        VSICalloc(sizeof(GDALRasterBand *),poDS->nBands);

    for( i = 0; i < poDS->nBands; i++ )
        poDS->papoBands[i] = new SDTSRasterBand( poDS, i+1, poRL );

    return( poDS );
}

/************************************************************************/
/*                         GDALRegister_SDTS()                          */
/************************************************************************/

void GDALRegister_SDTS()

{
    GDALDriver	*poDriver;

    if( poSDTSDriver == NULL )
    {
        poSDTSDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "SDTS";
        poDriver->pszLongName = "SDTS Raster";
        
        poDriver->pfnOpen = SDTSDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

