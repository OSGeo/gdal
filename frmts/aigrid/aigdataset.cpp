/******************************************************************************
 * $Id$
 *
 * Project:  Arc/Info Binary Grid Driver
 * Purpose:  Implements GDAL interface to underlying library.
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
 *****************************************************************************
 *
 * $Log$
 * Revision 1.4  1999/08/13 03:27:50  warmerda
 * added support for GDT_Int32 and GDT_Float32 access
 *
 * Revision 1.3  1999/07/23 14:28:26  warmerda
 * supress errors in AIGOpen() since we don't know if it's really AIG yet
 *
 * Revision 1.2  1999/06/26 21:00:38  warmerda
 * Relax checking that filename is a directory ... AIGOpen() now handles.
 *
 * Revision 1.1  1999/02/04 22:15:44  warmerda
 * New
 *
 */

#include "gdal_priv.h"
#include "aigrid.h"

CPL_C_START
void	GDALRegister_AIGrid(void);
CPL_C_END


/************************************************************************/
/* ==================================================================== */
/*				AIGDataset				*/
/* ==================================================================== */
/************************************************************************/

class AIGRasterBand;

class CPL_DLL AIGDataset : public GDALDataset
{
    friend	AIGRasterBand;
    
    AIGInfo_t	*psInfo;

  public:
                AIGDataset();
                ~AIGDataset();

    static GDALDataset *Open( GDALOpenInfo * );

    virtual CPLErr GetGeoTransform( double * );
};

/************************************************************************/
/* ==================================================================== */
/*                            AIGRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class AIGRasterBand : public GDALRasterBand
{
    friend	AIGDataset;

  public:

                   AIGRasterBand( AIGDataset *, int );

    virtual CPLErr IReadBlock( int, int, void * );
};

static GDALDriver	*poAIGDriver = NULL;

/************************************************************************/
/*                           AIGRasterBand()                            */
/************************************************************************/

AIGRasterBand::AIGRasterBand( AIGDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;

    nBlockXSize = poDS->psInfo->nBlockXSize;
    nBlockYSize = poDS->psInfo->nBlockYSize;

    if( poDS->psInfo->nCellType == AIG_CELLTYPE_INT
        && poDS->psInfo->dfMin >= 0.0 && poDS->psInfo->dfMax <= 255.0 )
    {
        eDataType = GDT_Byte;
    }
    else if( poDS->psInfo->nCellType == AIG_CELLTYPE_INT )
    {
        eDataType = GDT_Int32;
    }
    else
    {
        eDataType = GDT_Float32;
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr AIGRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    AIGDataset	*poODS = (AIGDataset *) poDS;
    GUInt32	*panGridRaster;
    int		i;

    if( poODS->psInfo->nCellType == AIG_CELLTYPE_INT )
    {
        panGridRaster = (GUInt32 *) CPLMalloc(4*nBlockXSize*nBlockYSize);
        if( AIGReadTile( poODS->psInfo, nBlockXOff, nBlockYOff, panGridRaster )
            != CE_None )
        {
            CPLFree( panGridRaster );
            return CE_Failure;
        }

        if( eDataType == GDT_Byte )
        {
            for( i = 0; i < nBlockXSize * nBlockYSize; i++ )
                ((GByte *) pImage)[i] = panGridRaster[i];
        }
        else
        {
            for( i = 0; i < nBlockXSize * nBlockYSize; i++ )
                ((GInt32 *) pImage)[i] = panGridRaster[i];
        }
        
        CPLFree( panGridRaster );

        return CE_None;
    }
    else
    {
        return AIGReadFloatTile( poODS->psInfo, nBlockXOff, nBlockYOff,
                                 (float *) pImage );
    }
}


/************************************************************************/
/* ==================================================================== */
/*                            AIGDataset                               */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            AIGDataset()                            */
/************************************************************************/

AIGDataset::AIGDataset()

{
    psInfo = NULL;
}

/************************************************************************/
/*                           ~AIGDataset()                            */
/************************************************************************/

AIGDataset::~AIGDataset()

{
    if( psInfo != NULL )
        AIGClose( psInfo );
}


/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *AIGDataset::Open( GDALOpenInfo * poOpenInfo )

{
    AIGInfo_t	*psInfo;
    CPLErrorHandler pfnOldHandler;
    
/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    pfnOldHandler = CPLSetErrorHandler( NULL );
    psInfo = AIGOpen( poOpenInfo->pszFilename, "r" );
    CPLSetErrorHandler( pfnOldHandler );
    
    if( psInfo == NULL )
    {
        CPLErrorReset();
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    AIGDataset 	*poDS;

    poDS = new AIGDataset();

    poDS->psInfo = psInfo;
    poDS->poDriver = poAIGDriver;

/* -------------------------------------------------------------------- */
/*      Establish raster info.                                          */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = psInfo->nPixels;
    poDS->nRasterYSize = psInfo->nLines;
    poDS->nBands = 1;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->papoBands = (GDALRasterBand **)VSICalloc(sizeof(GDALRasterBand *),
                                                   poDS->nBands);
    poDS->papoBands[0] = new AIGRasterBand( poDS, 1 );

    return( poDS );
}


/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr AIGDataset::GetGeoTransform( double * padfTransform )

{
    padfTransform[0] = psInfo->dfLLX;
    padfTransform[1] = psInfo->dfCellSizeX;
    padfTransform[2] = 0;

    padfTransform[3] = psInfo->dfURY;
    padfTransform[4] = 0;
    padfTransform[5] = psInfo->dfCellSizeY;
    
    return( CE_Failure );
}

/************************************************************************/
/*                          GDALRegister_AIG()                        */
/************************************************************************/

void GDALRegister_AIGrid()

{
    GDALDriver	*poDriver;

    if( poAIGDriver == NULL )
    {
        poAIGDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "AIGrid";
        poDriver->pszLongName = "Arc/Info Binary Grid";
        
        poDriver->pfnOpen = AIGDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

