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
 * Revision 1.3  2000/08/25 14:28:04  warmerda
 * preliminary support with IRasterIO
 *
 * Revision 1.2  2000/02/28 16:32:20  warmerda
 * use SetBand method
 *
 * Revision 1.1  1999/01/11 15:29:16  warmerda
 * New
 *
 */

#include "ogdidataset.h"
#include "cpl_string.h"

static GDALDriver	*poOGDIDriver = NULL;

/************************************************************************/
/*                           OGDIRasterBand()                            */
/************************************************************************/

OGDIRasterBand::OGDIRasterBand( OGDIDataset *poDS, int nBand, 
                                const char * pszName, ecs_Family eFamily )

{
    ecs_Result	*psResult;
    
    this->poDS = poDS;
    this->nBand = nBand;
    this->eFamily = eFamily;
    this->pszLayerName = CPLStrdup(pszName);

/* -------------------------------------------------------------------- */
/*      Make this layer current.                                        */
/* -------------------------------------------------------------------- */
    EstablishAccess( 0, poDS->GetRasterXSize(), poDS->GetRasterXSize() );

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
/*                          ~OGDIRasterBand()                           */
/************************************************************************/

OGDIRasterBand::~OGDIRasterBand()

{
    FlushCache();
    CPLFree( pszLayerName );
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
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr OGDIRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType,
                                  int nPixelSpace, int nLineSpace )

{
    OGDIDataset	*poODS = (OGDIDataset *) poDS;
    CPLErr    eErr;

    CPLDebug( "OGDIRasterBand", 
              "RasterIO(%d,%d,%d,%d -> %dx%d)\n", 
              nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize );

/* -------------------------------------------------------------------- */
/*      Establish access at the desired resolution.                     */
/* -------------------------------------------------------------------- */
    eErr = EstablishAccess( nXOff, nXSize, nBufXSize );
    if( eErr != CE_None )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Establish the start scanline we want.                           */
/* -------------------------------------------------------------------- */
    double	dfNorthEdge;
    int         nStartIndex;

    dfNorthEdge =  poODS->sGlobalBounds.north 
        - nYOff * poODS->sGlobalBounds.ns_res;
    
    nStartIndex = (int) ((poODS->sCurrentBounds.north - dfNorthEdge)
                         / poODS->sCurrentBounds.ns_res + 0.01);
    
/* -------------------------------------------------------------------- */
/*      Read back one scanline at a time, till request is satisfied.    */
/* -------------------------------------------------------------------- */
    int      iScanline;

    for( iScanline = 0; iScanline < nBufYSize; iScanline++ )
    {
        char		szId[32];
        ecs_Result	*psResult;
        void		*pLineData;

        pLineData = ((unsigned char *) pData) + iScanline * nLineSpace;

        sprintf( szId, "%d", iScanline + nStartIndex );

        psResult = cln_GetObject( poODS->nClientID, szId );
        if( ECSERROR(psResult) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s", psResult->message );
            return( CE_Failure );
        }
        
        if( eFamily == Matrix )
        {
            GDALCopyWords( ECSRASTER(psResult), GDT_UInt32, 4, 
                           pLineData, eBufType, nPixelSpace,
                           nBufXSize );
        }
    }

    return CE_None;
}

/************************************************************************/
/*                       HasArbitraryOverviews()                        */
/************************************************************************/

int OGDIRasterBand::HasArbitraryOverviews()

{
    return TRUE;
}

/************************************************************************/
/*                          EstablishAccess()                           */
/************************************************************************/

CPLErr OGDIRasterBand::EstablishAccess( int nWinXOff, int nWinXSize, 
                                        int nBufXSize )

{
    ecs_Result	 *psResult;
    OGDIDataset  *poODS = (OGDIDataset *) poDS;

/* -------------------------------------------------------------------- */
/*      Is this already the current band?  If not, make it so now.      */
/* -------------------------------------------------------------------- */
    if( poODS->nCurrentBand != nBand )
    {
        ecs_LayerSelection sSelection;
        
        sSelection.Select = pszLayerName;
        sSelection.F = eFamily;
        
        psResult = cln_SelectLayer( poODS->nClientID, &sSelection );
        if( ECSERROR(psResult) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s", psResult->message );
            return CE_Failure;
        }

        poODS->nCurrentBand = nBand;
    }
        
/* -------------------------------------------------------------------- */
/*      What region would represent this resolution and window?         */
/* -------------------------------------------------------------------- */
    ecs_Region   sWin;
    int          nYSize;

    sWin.west = nWinXOff * poODS->sGlobalBounds.ew_res
        + poODS->sGlobalBounds.west;
    sWin.east = (nWinXOff+nWinXSize) * poODS->sGlobalBounds.ew_res
        + poODS->sGlobalBounds.west;
    sWin.ew_res = poODS->sGlobalBounds.ew_res*(nWinXSize/(double)nBufXSize);

    sWin.north = poODS->sGlobalBounds.north;
    sWin.ns_res = sWin.ew_res;

    nYSize = (int) ((poODS->sGlobalBounds.north - poODS->sGlobalBounds.south
                     + sWin.ns_res*0.9) / sWin.ns_res);
    sWin.south = sWin.north - nYSize * sWin.ns_res;

    if( ABS(sWin.west - poODS->sCurrentBounds.west) > 0.0001 
        || ABS(sWin.east - poODS->sCurrentBounds.east) > 0.0001 
        || ABS(sWin.north - poODS->sCurrentBounds.east) > 0.0001 
        || ABS(sWin.south - poODS->sCurrentBounds.east) > 0.0001 
        || ABS(sWin.ew_res/poODS->sCurrentBounds.ew_res - 1.0) > 0.0001
        || ABS(sWin.ns_res/poODS->sCurrentBounds.ns_res - 1.0) > 0.0001 )
    {
        psResult = cln_SelectRegion( poODS->nClientID, &sWin );
        if( ECSERROR(psResult) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s", psResult->message );
            return CE_Failure;
        }
        
        poODS->sCurrentBounds = sWin;
    }

    return CE_None;
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
    nCurrentBand = -1;
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
    char        **papszImages=NULL, **papszMatrices=NULL;
    
    if( !EQUALN(poOpenInfo->pszFilename,"gltp:",5) )
        return( NULL );

/* -------------------------------------------------------------------- */
/*      Has the user hardcoded a layer and family in the URL?           */
/* -------------------------------------------------------------------- */
    int       nC1=-1, nC2=-1, i;
    char      *pszURL = CPLStrdup(poOpenInfo->pszFilename);

    for( i = strlen(pszURL)-1; i > 0; i-- )
    {
        if( pszURL[i] == '/' )
            break;
        
        if( pszURL[i] == ':' )
        {
            if( nC1 == -1 )
                nC1 = i;
            else if( nC2 == -1 )
                nC2 = i;
        }
    }	

    if( nC2 == -1 
        || (!EQUAL(pszURL+nC1,":Image") && !EQUAL(pszURL+nC1,":Matrix") ))
    {
        CollectLayers( &papszImages, &papszMatrices );
    }
    else
    {
        pszURL[nC1] = '\0';

        if( EQUAL(pszURL+nC1+1,"Image") )
            papszImages = CSLAddString( papszImages, pszURL+nC2+1 );
        else
            papszMatrices = CSLAddString( papszImages, pszURL+nC2+1 );

        pszURL[nC2] = '\0';
    }
        
/* -------------------------------------------------------------------- */
/*      Open the client interface.                                      */
/* -------------------------------------------------------------------- */
    psResult = cln_CreateClient( &nClientID, pszURL );
    CPLFree( pszURL );

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

    poDS->sCurrentBounds = poDS->sGlobalBounds;
    
/* -------------------------------------------------------------------- */
/*      Establish raster info.                                          */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = (int) 
        ((poDS->sGlobalBounds.east - poDS->sGlobalBounds.west)
         / poDS->sGlobalBounds.ew_res);
        
    poDS->nRasterYSize = (int) 
        ((poDS->sGlobalBounds.north - poDS->sGlobalBounds.south)
         / poDS->sGlobalBounds.ns_res);

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( i=0; papszMatrices != NULL && papszMatrices[i] != NULL; i++)
    {
        poDS->SetBand( poDS->GetRasterCount()+1, 
                       new OGDIRasterBand( poDS, poDS->GetRasterCount()+1, 
                                           papszMatrices[i], Matrix ) );
    }

    for( i=0; papszImages != NULL && papszImages[i] != NULL; i++)
    {
        poDS->SetBand( poDS->GetRasterCount()+1, 
                       new OGDIRasterBand( poDS, poDS->GetRasterCount()+1, 
                                           papszImages[i], Image ) );
    }

    return( poDS );
}

/************************************************************************/
/*                           CollectLayers()                            */
/************************************************************************/

CPLErr OGDIDataset::CollectLayers( char ***, char *** )

{
    CPLError( CE_Failure, CPLE_AppDefined, 
              "CollectLayers() not implemented yet" );

    return CE_Failure;
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

