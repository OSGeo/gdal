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
 * Revision 1.8  2001/04/18 17:58:41  warmerda
 * fixed raster size calculation
 *
 * Revision 1.7  2001/04/17 21:46:04  warmerda
 * complete Image support, utilize cln_GetLayerCapabilities()
 *
 * Revision 1.6  2000/08/28 21:30:17  warmerda
 * restructure to use cln_GetNextObject
 *
 * Revision 1.5  2000/08/28 20:15:07  warmerda
 * added projection translation
 *
 * Revision 1.4  2000/08/25 21:31:04  warmerda
 * added colortable support
 *
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
#include "ogr_spatialref.h"

static GDALDriver	*poOGDIDriver = NULL;

/************************************************************************/
/*                           OGDIRasterBand()                            */
/************************************************************************/

OGDIRasterBand::OGDIRasterBand( OGDIDataset *poDS, int nBand, 
                                const char * pszName, ecs_Family eFamily,
                                int nComponent )

{
    ecs_Result	*psResult;
    
    this->poDS = poDS;
    this->nBand = nBand;
    this->eFamily = eFamily;
    this->pszLayerName = CPLStrdup(pszName);
    this->nComponent = nComponent;
    poCT = NULL;

/* -------------------------------------------------------------------- */
/*      Make this layer current.                                        */
/* -------------------------------------------------------------------- */
    EstablishAccess( 0, 0, poDS->GetRasterXSize(), poDS->GetRasterXSize() );

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
/*      Establish if we have meaningful colortable information.         */
/* -------------------------------------------------------------------- */
    if( eFamily == Matrix )
    {
        int     i;

        poCT = new GDALColorTable();
        
        for( i = 0; i < (int) ECSRASTERINFO(psResult).cat.cat_len; i++ ) {
            GDALColorEntry   sEntry;

            sEntry.c1 = ECSRASTERINFO(psResult).cat.cat_val[i].r;
            sEntry.c2 = ECSRASTERINFO(psResult).cat.cat_val[i].g;
            sEntry.c3 = ECSRASTERINFO(psResult).cat.cat_val[i].b;
            sEntry.c4 = 255;

            poCT->SetColorEntry( ECSRASTERINFO(psResult).cat.cat_val[i].no_cat, 
                                 &sEntry );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Get the GDAL data type.  Eventually we might use the            */
/*      category info to establish what to do here.                     */
/* -------------------------------------------------------------------- */
    if( eFamily == Matrix )
        eDataType = GDT_Byte;
    else if( ECSRASTERINFO(psResult).width == 1 )
        eDataType = GDT_Byte;
    else if( ECSRASTERINFO(psResult).width == 2 )
        eDataType = GDT_Byte;
    else if( ECSRASTERINFO(psResult).width == 3 )
        eDataType = GDT_UInt16;
    else if( ECSRASTERINFO(psResult).width == 4 )
        eDataType = GDT_Int16;
    else if( ECSRASTERINFO(psResult).width == 5 )
        eDataType = GDT_Int32;
    else
        eDataType = GDT_UInt32;

    nOGDIImageType = ECSRASTERINFO(psResult).width;
    
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
    return IRasterIO( GF_Read, 0, nBlockYOff, nBlockXSize, 1, 
                      pImage, nBlockXSize, 1, eDataType, 
                      GDALGetDataTypeSize(eDataType)/8, 0 );
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
    eErr = EstablishAccess( nYOff, nXOff, nXSize, nBufXSize );
    if( eErr != CE_None )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Read back one scanline at a time, till request is satisfied.    */
/* -------------------------------------------------------------------- */
    int      iScanline;

    for( iScanline = 0; iScanline < nBufYSize; iScanline++ )
    {
        ecs_Result	*psResult;
        void		*pLineData;
        pLineData = ((unsigned char *) pData) + iScanline * nLineSpace;

        poODS->nCurrentIndex++;
        psResult = cln_GetNextObject( poODS->nClientID );

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
        else if( nOGDIImageType == 1 )
        {
            GDALCopyWords( ((GByte *) ECSRASTER(psResult)) + nComponent, 
                           GDT_Byte, 4,
                           pLineData, eBufType, nPixelSpace, nBufXSize );

            if( nComponent == 3 )
            {
                int	i;

                for( i = 0; i < nBufXSize; i++ )
                {
                    if( ((GByte *) pLineData)[i] != 0 )
                        ((GByte *) pLineData)[i] = 255;
                    else
                        ((GByte *) pLineData)[i] = 0;
                    
                }
            }
        }
        else if( nOGDIImageType == 2 )
        {
            GDALCopyWords( ECSRASTER(psResult), GDT_Byte, 1,
                           pLineData, eBufType, nPixelSpace,
                           nBufXSize );
        }
        else if( nOGDIImageType == 3 )
        {
            GDALCopyWords( ECSRASTER(psResult), GDT_UInt16, 2,
                           pLineData, eBufType, nPixelSpace,
                           nBufXSize );
        }
        else if( nOGDIImageType == 4 )
        {
            GDALCopyWords( ECSRASTER(psResult), GDT_Int16, 2,
                           pLineData, eBufType, nPixelSpace,
                           nBufXSize );
        }
        else if( nOGDIImageType == 5 )
        {
            GDALCopyWords( ECSRASTER(psResult), GDT_Int32, 4,
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

CPLErr OGDIRasterBand::EstablishAccess( int nYOff, 
                                        int nWinXOff, int nWinXSize, 
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
        
        CPLDebug( "OGDIRasterBand", "<EstablishAccess: SelectLayer(%s)>\n",
                  pszLayerName );
        psResult = cln_SelectLayer( poODS->nClientID, &sSelection );
        if( ECSERROR(psResult) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s", psResult->message );
            return CE_Failure;
        }

        poODS->nCurrentBand = nBand;
        poODS->nCurrentIndex = -1;
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

    sWin.north = poODS->sGlobalBounds.north 
        - nYOff*poODS->sGlobalBounds.ns_res;
    sWin.ns_res = sWin.ew_res 
        * (poODS->sGlobalBounds.ns_res / poODS->sGlobalBounds.ew_res);

    nYSize = (int) ((sWin.north - poODS->sGlobalBounds.south + sWin.ns_res*0.9)
                    / sWin.ns_res);
    sWin.south = sWin.north - nYSize * sWin.ns_res;

    if( poODS->nCurrentIndex == -1 
        || ABS(sWin.west - poODS->sCurrentBounds.west) > 0.0001 
        || ABS(sWin.east - poODS->sCurrentBounds.east) > 0.0001 
        || ABS(sWin.north+poODS->nCurrentIndex*sWin.ns_res
               -poODS->sCurrentBounds.north) > 0.0001
        || ABS(sWin.ew_res/poODS->sCurrentBounds.ew_res - 1.0) > 0.0001
        || ABS(sWin.ns_res/poODS->sCurrentBounds.ns_res - 1.0) > 0.0001 )
    {
        CPLDebug( "OGDIRasterBand", "<EstablishAccess: Set Region>\n" );
        psResult = cln_SelectRegion( poODS->nClientID, &sWin );
        if( ECSERROR(psResult) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s", psResult->message );
            return CE_Failure;
        }
        
        poODS->sCurrentBounds = sWin;
        poODS->nCurrentIndex = 0;
    }

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp OGDIRasterBand::GetColorInterpretation()

{
    if( poCT == NULL )
        return GCI_Undefined;
    else
        return GCI_PaletteIndex;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *OGDIRasterBand::GetColorTable()

{
    return poCT;
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
    nCurrentIndex = -1;
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
            {
                nC1 = i;
                pszURL[nC1] = '\0';
            }
            else if( nC2 == -1 )
            {
                nC2 = i;
                pszURL[nC2] = '\0';
            }
        }
    }	

/* -------------------------------------------------------------------- */
/*      If we got a "family", and it is a vector family then return     */
/*      quietly.                                                        */
/* -------------------------------------------------------------------- */
    if( nC2 != -1 
        && !EQUAL(pszURL+nC1+1,"Matrix") 
        && !EQUAL(pszURL+nC1+1,"Image") )
    {
        CPLFree( pszURL );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open the client interface.                                      */
/* -------------------------------------------------------------------- */
    psResult = cln_CreateClient( &nClientID, pszURL );

    if( ECSERROR(psResult) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", psResult->message );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      collect the list of images and matrices available.              */
/* -------------------------------------------------------------------- */
    if( nC2 == -1 )
    {
        CollectLayers( nClientID, &papszImages, &papszMatrices );
    }
    else
    {

        if( EQUAL(pszURL+nC1+1,"Image") )
            papszImages = CSLAddString( papszImages, pszURL+nC2+1 );
        else
            papszMatrices = CSLAddString( papszMatrices, pszURL+nC2+1 );
    }

    CPLFree( pszURL );

/* -------------------------------------------------------------------- */
/*      If this is a 3.1 server (ie, it support                         */
/*      cln_GetLayerCapabilities()) and it has no raster layers then    */
/*      we can assume it must be a vector datastore.  End without an    */
/*      error in case the application wants to try this through         */
/*      OGR.                                                            */
/* -------------------------------------------------------------------- */
    psResult = cln_GetVersion(nClientID);
    
    if( (ECSERROR(psResult) || atof(ECSTEXT(psResult)) >= 3.1)
        && CSLCount(papszMatrices) == 0 
        && CSLCount(papszImages) == 0 )
    {
        CPLDebug( "OGDIDataset",
                  "While this is an OGDI datastore, it does not appear to\n"
                  "have any identifiable raster layers.  Perhaps it is a\n"
                  "vector datastore?" );
        CPLFree( pszURL );
        cln_DestroyClient( nClientID );
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

    OGRSpatialReference  oSRS;

    if( oSRS.importFromProj4( ECSTEXT(psResult) ) == OGRERR_NONE )
    {
        poDS->pszProjection = NULL;
        oSRS.exportToWkt( &(poDS->pszProjection) );
    }
    else
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                  "untranslatable PROJ.4 projection: %s\n", 
                  ECSTEXT(psResult) );
        poDS->pszProjection = CPLStrdup("");
    }

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
        (((poDS->sGlobalBounds.east - poDS->sGlobalBounds.west)
          / poDS->sGlobalBounds.ew_res) + 0.5);
    
    poDS->nRasterYSize = (int) 
        (((poDS->sGlobalBounds.north - poDS->sGlobalBounds.south)
          / poDS->sGlobalBounds.ns_res) + 0.5);

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( i=0; papszMatrices != NULL && papszMatrices[i] != NULL; i++)
    {
        poDS->SetBand( poDS->GetRasterCount()+1, 
                       new OGDIRasterBand( poDS, poDS->GetRasterCount()+1, 
                                           papszMatrices[i], Matrix, 0 ) );
    }

    for( i=0; papszImages != NULL && papszImages[i] != NULL; i++)
    {
        OGDIRasterBand	*poBand;

        poBand = new OGDIRasterBand( poDS, poDS->GetRasterCount()+1, 
                                     papszImages[i], Image, 0 );

        poDS->SetBand( poDS->GetRasterCount()+1, poBand );

        /* special case for RGBt Layers */
        if( poBand->nOGDIImageType == 1 )
        {
            poDS->SetBand( poDS->GetRasterCount()+1, 
                           new OGDIRasterBand( poDS, poDS->GetRasterCount()+1, 
                                               papszImages[i], Image, 1 ));
            poDS->SetBand( poDS->GetRasterCount()+1, 
                           new OGDIRasterBand( poDS, poDS->GetRasterCount()+1, 
                                               papszImages[i], Image, 2 ));
            poDS->SetBand( poDS->GetRasterCount()+1, 
                           new OGDIRasterBand( poDS, poDS->GetRasterCount()+1, 
                                               papszImages[i], Image, 3 ));
        }
    }

    return( poDS );
}

/************************************************************************/
/*                           CollectLayers()                            */
/************************************************************************/

CPLErr OGDIDataset::CollectLayers( int nClientID, 
                                   char ***ppapszImages, 
                                   char ***ppapszMatrices )

{
    const ecs_LayerCapabilities	*psLayer;
    int		iLayer;

    for( iLayer = 0; 
         (psLayer = cln_GetLayerCapabilities(nClientID,iLayer)) != NULL;
         iLayer++ )
    {
        if( psLayer->families[Image] )
            *ppapszImages = CSLAddString( *ppapszImages, psLayer->name );
        else if( psLayer->families[Matrix] )
            *ppapszMatrices = CSLAddString( *ppapszImages, psLayer->name );
    }

    return CE_None;
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

