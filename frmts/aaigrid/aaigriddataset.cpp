/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  Implements Arc/Info ASCII Grid Format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam (warmerdam@pobox.com)
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
 * Revision 1.1  2001/03/12 15:15:30  warmerda
 * New
 *
 */

#include <ctype.h>
#include "gdal_priv.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"

CPL_C_START
void	GDALRegister_AAIGrid(void);
CPL_C_END


/************************************************************************/
/* ==================================================================== */
/*				AAIGDataset				*/
/* ==================================================================== */
/************************************************************************/

class AAIGRasterBand;

class CPL_DLL AAIGDataset : public GDALDataset
{
    friend	AAIGRasterBand;
    
    FILE	*fp;

    double	adfGeoTransform[6];
    char	**papszPrj;
    char	*pszProjection;

    int		bNoDataSet;
    double	dfNoDataValue;

  public:
                AAIGDataset();
                ~AAIGDataset();

    static GDALDataset *Open( GDALOpenInfo * );

    virtual CPLErr GetGeoTransform( double * );
    virtual const char *GetProjectionRef(void);
};

/************************************************************************/
/* ==================================================================== */
/*                            AAIGRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class AAIGRasterBand : public GDALRasterBand
{
    friend	AAIGDataset;

    int		*panLineOffset;

  public:

                   AAIGRasterBand( AAIGDataset *, int, GDALDataType );
    virtual       ~AAIGRasterBand();

    virtual double GetNoDataValue( int * );
    virtual CPLErr IReadBlock( int, int, void * );
};

static GDALDriver	*poAAIGDriver = NULL;

/************************************************************************/
/*                           AAIGRasterBand()                            */
/************************************************************************/

AAIGRasterBand::AAIGRasterBand( AAIGDataset *poDS, int nDataStart, 
                                GDALDataType eTypeIn )

{
    this->poDS = poDS;

    nBand = 1;
    eDataType = eTypeIn;

    nBlockXSize = poDS->nRasterXSize;
    nBlockYSize = 1;

    panLineOffset = (int *) CPLCalloc(poDS->nRasterYSize,sizeof(int));
    panLineOffset[0] = nDataStart;
}

/************************************************************************/
/*                          ~AAIGRasterBand()                           */
/************************************************************************/

AAIGRasterBand::~AAIGRasterBand()

{
    CPLFree( panLineOffset );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr AAIGRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    AAIGDataset	*poODS = (AAIGDataset *) poDS;
    const char	*pszLine;
    char	**papszTokens;
    int		i;

    if( nBlockYOff < 0 || nBlockYOff > poODS->nRasterYSize - 1 
        || nBlockXOff != 0 )
        return CE_Failure;

    if( panLineOffset[nBlockYOff] == 0 )
        IReadBlock( nBlockXOff, nBlockYOff-1, NULL );

    if( panLineOffset[nBlockYOff] == 0 )
        return CE_Failure;

    if( VSIFSeek( poODS->fp, panLineOffset[nBlockYOff], SEEK_SET ) != 0 )
        return CE_Failure;

    pszLine = CPLReadLine( poODS->fp );
    if( pszLine == NULL )
        return CE_Failure;

    if( nBlockYOff < poODS->nRasterYSize - 1 )
        panLineOffset[nBlockYOff+1] = VSIFTell( poODS->fp );

    if( pImage == NULL )
        return CE_None;

    papszTokens = CSLTokenizeString( pszLine );
    if( papszTokens == NULL )
        return CE_Failure;

    for( i = 0; i < poODS->nRasterXSize && papszTokens[i] != NULL; i++ )
    {
        if( eDataType == GDT_Float32 )
            ((float *) pImage)[i] = atof(papszTokens[i]);
        else
            ((GInt16 *) pImage)[i] = atoi(papszTokens[i]);
    }

    CSLDestroy( papszTokens );
    
    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double AAIGRasterBand::GetNoDataValue( int * pbSuccess )

{
    AAIGDataset	*poODS = (AAIGDataset *) poDS;

    if( pbSuccess )
        *pbSuccess = poODS->bNoDataSet;

    return poODS->dfNoDataValue;
}

/************************************************************************/
/* ==================================================================== */
/*                            AAIGDataset                               */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            AAIGDataset()                            */
/************************************************************************/

AAIGDataset::AAIGDataset()

{
    papszPrj = NULL;
    pszProjection = CPLStrdup("");
    fp = NULL;
    bNoDataSet = FALSE;
    dfNoDataValue = -9999.0;

    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = -1.0;
}

/************************************************************************/
/*                           ~AAIGDataset()                            */
/************************************************************************/

AAIGDataset::~AAIGDataset()

{
    if( fp != NULL )
        VSIFClose( fp );

    CPLFree( pszProjection );
    CSLDestroy( papszPrj );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *AAIGDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		i;
    GDALDataType eDataType;
    char	**papszTokens;

/* -------------------------------------------------------------------- */
/*      Does this look like an AI grid file?                            */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 100
        || poOpenInfo->fp == NULL
        || !EQUALN((const char *) poOpenInfo->pabyHeader,"ncols",5) )
        return NULL;

    papszTokens =  
        CSLTokenizeStringComplex( (const char *) poOpenInfo->pabyHeader,
                                  " \n\r", FALSE, FALSE );
    if( !EQUAL(papszTokens[0],"ncols")
        || !EQUAL(papszTokens[2],"nrows") )
    {
        CSLDestroy( papszTokens );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    AAIGDataset 	*poDS;

    poDS = new AAIGDataset();

    poDS->poDriver = poAAIGDriver;

    poDS->fp = poOpenInfo->fp;
    poOpenInfo->fp = NULL;
    
/* -------------------------------------------------------------------- */
/*      Parse the header.                                               */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = atoi(papszTokens[1]);
    poDS->nRasterYSize = atoi(papszTokens[3]);
    poDS->nBands = 1;

    if( EQUAL(papszTokens[4],"xllcorner") 
        && EQUAL(papszTokens[6],"yllcorner") 
        && EQUAL(papszTokens[8],"cellsize") )
    {
        double	dfCellSize = atof( papszTokens[9] );

        poDS->adfGeoTransform[0] = atof( papszTokens[5] );
        poDS->adfGeoTransform[1] = dfCellSize;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = atof( papszTokens[7] )
            + poDS->nRasterYSize * dfCellSize;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = - dfCellSize;
    }
    else if( EQUAL(papszTokens[4],"xllcenter") 
             && EQUAL(papszTokens[4],"yllcenter") 
             && EQUAL(papszTokens[8],"cellsize") )
    {
        double	dfCellSize = atof( papszTokens[9] );

        poDS->adfGeoTransform[0] = atof( papszTokens[5] ) + 0.5 * dfCellSize;
        poDS->adfGeoTransform[1] = dfCellSize;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = atof( papszTokens[7] )
            + poDS->nRasterYSize * dfCellSize - 0.5 * dfCellSize;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = - dfCellSize;
    }

    if( CSLCount( papszTokens ) >= 12 )
    {
        if( EQUAL(papszTokens[10],"NODATA_value") )
        {
            poDS->bNoDataSet = TRUE;
            poDS->dfNoDataValue = atof(papszTokens[11]);
        }
    }

    CSLDestroy( papszTokens );

/* -------------------------------------------------------------------- */
/*      Find the start of real data.                                    */
/* -------------------------------------------------------------------- */
    int		nStartOfData;

    for( i = 2; TRUE ; i++ )
    {
        if( poOpenInfo->pabyHeader[i] == '\0' )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Couldn't fine data values in ASCII Grid file.\n" );
            return NULL;			
        }

        if( poOpenInfo->pabyHeader[i-1] == '\n' 
            || poOpenInfo->pabyHeader[i-2] == '\n' )
        {
            if( !isalpha(poOpenInfo->pabyHeader[i]) )
            {
                nStartOfData = i;
                if( strstr((const char *)poOpenInfo->pabyHeader+i,".") != NULL)
                    eDataType = GDT_Float32;
                else
                    eDataType = GDT_Int16;

                break;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->SetBand( 1, new AAIGRasterBand( poDS, nStartOfData, eDataType ) );

/* -------------------------------------------------------------------- */
/*	Try to read projection file.					*/
/* -------------------------------------------------------------------- */
    char	*pszDirname, *pszBasename;
    const char	*pszPrjFilename;
    VSIStatBuf   sStatBuf;

    pszDirname = CPLStrdup(CPLGetPath(poOpenInfo->pszFilename));
    pszBasename = CPLStrdup(CPLGetBasename(poOpenInfo->pszFilename));

    pszPrjFilename = CPLFormFilename( pszDirname, pszBasename, "prf" );
    if( VSIStat( pszPrjFilename, &sStatBuf ) == 0 )
    {
        OGRSpatialReference	oSRS;

        poDS->papszPrj = CSLLoad( pszPrjFilename );

        if( oSRS.importFromESRI( poDS->papszPrj ) == OGRERR_NONE )
        {
            CPLFree( poDS->pszProjection );
            oSRS.exportToWkt( &(poDS->pszProjection) );
        }
    }

    CPLFree( pszDirname );
    CPLFree( pszBasename );

    return( poDS );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr AAIGDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
    return( CE_None );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *AAIGDataset::GetProjectionRef()

{
    return pszProjection;
}

/************************************************************************/
/*                          GDALRegister_AAIG()                        */
/************************************************************************/

void GDALRegister_AAIGrid()

{
    GDALDriver	*poDriver;

    if( poAAIGDriver == NULL )
    {
        poAAIGDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "AAIGrid";
        poDriver->pszLongName = "Arc/Info ASCII Grid";
        
        poDriver->pfnOpen = AAIGDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

