/******************************************************************************
 * $Id$
 *
 * Name:     ogdidataset.cpp
 * Project:  OGDI Bridge
 * Purpose:  Main driver for OGDI.
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
 *****************************************************************************
 *
 * $Log$
 * Revision 1.1  1999/01/11 15:29:16  warmerda
 * New
 *
 */

#include "frmts/ogdi/ogdidataset.h"

static GDALDriver	*poOGDIDriver = NULL;

/************************************************************************/
/*                           OGDIRasterBand()                            */
/************************************************************************/

OGDIRasterBand::OGDIRasterBand( OGDIDataset *poDS, int nBand )

{
    ecs_Result	*psResult;
    
    this->poDS = poDS;
    this->nBand = nBand;

/* -------------------------------------------------------------------- */
/*      Get the raster info.                                            */
/* -------------------------------------------------------------------- */
    psResult = cln_GetRasterInfo( poDS->nClientID );
    if( ECSERROR(psResult) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", psResult->message );
    }
    
/* -------------------------------------------------------------------- */
/*      Get the GDAL data type.  Eventually we might use the            */
/*      category info to establish what to do here.                     */
/* -------------------------------------------------------------------- */
    eDataType = GDT_Byte;
    
/* -------------------------------------------------------------------- */
/*	Currently only works for strips 				*/
/* -------------------------------------------------------------------- */
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr OGDIRasterBand::IReadBlock( int, int nBlockYOff, void * pImage )

{
    ecs_Result	*psResult;
    OGDIDataset	*poODS = (OGDIDataset *) poDS;
    char	szId[12];
    int		i;
    GByte	*pabyImage = (GByte *) pImage;

/* -------------------------------------------------------------------- */
/*      Read the requested scanline.                                    */
/* -------------------------------------------------------------------- */
    sprintf( szId, "%d", nBlockYOff );
    psResult = cln_GetObject( poODS->nClientID, szId );
    if( ECSERROR(psResult) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", psResult->message );
        return( CE_Failure );
    }

/* -------------------------------------------------------------------- */
/*      Transform to GByte.                                             */
/* -------------------------------------------------------------------- */
    for( i = 0; i < poODS->nRasterXSize; i++ )
    {
        pabyImage[i] = ECSRASTER(psResult)[i];
    }
    
    return( CE_None );
}


/************************************************************************/
/* ==================================================================== */
/*                            OGDIDataset                               */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            OGDIDataset()                            */
/************************************************************************/

OGDIDataset::OGDIDataset()

{
    nClientID = -1;
}

/************************************************************************/
/*                           ~OGDIDataset()                            */
/************************************************************************/

OGDIDataset::~OGDIDataset()

{
    cln_DestroyClient( nClientID );
}


/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *OGDIDataset::Open( GDALOpenInfo * poOpenInfo )

{
    ecs_Result	*psResult;
    int		nClientID;
    ecs_LayerSelection sSelection;
    
    if( !EQUALN(poOpenInfo->pszFilename,"gltp:",5) )
        return( NULL );

/* -------------------------------------------------------------------- */
/*      Open the client interface.                                      */
/* -------------------------------------------------------------------- */
    psResult = cln_CreateClient( &nClientID, poOpenInfo->pszFilename);
    if( ECSERROR(psResult) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", psResult->message );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    OGDIDataset 	*poDS;

    poDS = new OGDIDataset();

    poDS->nClientID = nClientID;
    poDS->poDriver = poOGDIDriver;

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    psResult = cln_GetGlobalBound( nClientID );
    if( ECSERROR(psResult) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", psResult->message );
        return NULL;
    }

    poDS->sGlobalBounds = ECSREGION(psResult);

    psResult = cln_GetServerProjection(nClientID);
    if( ECSERROR(psResult) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", psResult->message );
        return NULL;
    }
    poDS->pszProjection = CPLStrdup( ECSTEXT(psResult) );

/* -------------------------------------------------------------------- */
/*      Select the global region.                                       */
/* -------------------------------------------------------------------- */
    psResult = cln_SelectRegion( nClientID, &(poDS->sGlobalBounds) );
    if( ECSERROR(psResult) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", psResult->message );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Establish raster info.                                          */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = (int) 
        ((poDS->sGlobalBounds.east - poDS->sGlobalBounds.west)
         / poDS->sGlobalBounds.ew_res);
        
    poDS->nRasterYSize = (int) 
        ((poDS->sGlobalBounds.north - poDS->sGlobalBounds.south)
         / poDS->sGlobalBounds.ns_res);

    poDS->nBands = 1;
        
/* -------------------------------------------------------------------- */
/*      For now we hardcode for one layer.  Access it now using any     */
/*      old name ... this should work OK with some OGDI translators     */
/*      that access one raster file at a time.                          */
/* -------------------------------------------------------------------- */
    sSelection.Select = "xx";
    sSelection.F = Matrix;
    
    psResult = cln_SelectLayer( nClientID, &sSelection );
    if( ECSERROR(psResult) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", psResult->message );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->papoBands = (GDALRasterBand **)VSICalloc(sizeof(GDALRasterBand *),1);
    poDS->papoBands[0] = new OGDIRasterBand( poDS, 1 );

    return( poDS );
}


/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *OGDIDataset::GetProjectionRef()

{
    return( pszProjection );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr OGDIDataset::GetGeoTransform( double * padfTransform )

{
    padfTransform[0] = sGlobalBounds.west;
    padfTransform[1] = sGlobalBounds.ew_res;
    padfTransform[2] = 0.0;

    padfTransform[3] = sGlobalBounds.north;
    padfTransform[4] = 0.0;
    padfTransform[5] = -sGlobalBounds.ns_res;

    return( CE_None );
}

/************************************************************************/
/*                         GetInternalHandle()                          */
/************************************************************************/

void *OGDIDataset::GetInternalHandle( const char * pszRequest )

{
    if( EQUAL(pszRequest,"ClientID") )
        return (void *) nClientID;
    else
        return NULL;
}

/************************************************************************/
/*                          GDALRegister_OGDI()                        */
/************************************************************************/

void GDALRegister_OGDI()

{
    GDALDriver	*poDriver;

    if( poOGDIDriver == NULL )
    {
        poOGDIDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "OGDI";
        poDriver->pszLongName = "OGDI Bridge";
        
        poDriver->pfnOpen = OGDIDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

