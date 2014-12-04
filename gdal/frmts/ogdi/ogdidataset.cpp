/******************************************************************************
 * $Id$
 *
 * Name:     ogdidataset.cpp
 * Project:  OGDI Bridge
 * Purpose:  Main driver for OGDI.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
 ****************************************************************************/

#include <math.h>
#include "ecs.h"
#include "gdal_priv.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_OGDI(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				OGDIDataset				*/
/* ==================================================================== */
/************************************************************************/

class OGDIRasterBand;

class CPL_DLL OGDIDataset : public GDALDataset
{
    friend class OGDIRasterBand;
    
    int		nClientID;

    ecs_Region	sGlobalBounds;
    ecs_Region  sCurrentBounds;
    int         nCurrentBand;
    int         nCurrentIndex;

    char	*pszProjection;

    static CPLErr CollectLayers(int, char***,char***);
    static CPLErr OverrideGlobalInfo(OGDIDataset*,const char *);

    void        AddSubDataset( const char *pszType, const char *pszLayer );
    char	**papszSubDatasets;

  public:
    		OGDIDataset();
    		~OGDIDataset();
                
    static GDALDataset *Open( GDALOpenInfo * );

    int		GetClientID() { return nClientID; }

    virtual const char *GetProjectionRef(void);
    virtual CPLErr GetGeoTransform( double * );

    virtual void *GetInternalHandle( const char * );

    virtual char      **GetMetadataDomainList();
    virtual char **GetMetadata( const char * pszDomain = "" );
};

/************************************************************************/
/* ==================================================================== */
/*                            OGDIRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class OGDIRasterBand : public GDALRasterBand
{
    friend class OGDIDataset;

    int		nOGDIImageType; /* ie. 1 for RGB */

    char	*pszLayerName;
    ecs_Family  eFamily;

    int		nComponent; /* varies only for RGB layers */

    GDALColorTable *poCT;

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing nPixelSpace,
                              GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg );

    CPLErr         EstablishAccess( int nXOff, int nYOff, 
                                    int nXSize, int nYSize,
                                    int nBufXSize, int nBufYSize );

  public:

                   OGDIRasterBand( OGDIDataset *, int, const char *,
                                   ecs_Family, int );
                   ~OGDIRasterBand();

    virtual CPLErr IReadBlock( int, int, void * );
    virtual int    HasArbitraryOverviews();
    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();

    virtual CPLErr AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                               int nBufXSize, int nBufYSize, 
                               GDALDataType eDT, char **papszOptions );

};

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
    EstablishAccess( 0, 0, 
                     poDS->GetRasterXSize(), poDS->GetRasterYSize(), 
                     poDS->GetRasterXSize(), poDS->GetRasterYSize() );

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
    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);

    return IRasterIO( GF_Read, 0, nBlockYOff, nBlockXSize, 1, 
                      pImage, nBlockXSize, 1, eDataType, 
                      GDALGetDataTypeSize(eDataType)/8, 0, &sExtraArg );
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr OGDIRasterBand::IRasterIO( CPL_UNUSED GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType,
                                  GSpacing nPixelSpace,
                                  GSpacing nLineSpace,
                                  GDALRasterIOExtraArg* psExtraArg )

{
    OGDIDataset	*poODS = (OGDIDataset *) poDS;
    CPLErr    eErr;
#ifdef notdef
    CPLDebug( "OGDIRasterBand", 
              "RasterIO(%d,%d,%d,%d -> %dx%d)", 
              nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize );
#endif

/* -------------------------------------------------------------------- */
/*      Establish access at the desired resolution.                     */
/* -------------------------------------------------------------------- */
    eErr = EstablishAccess( nXOff, nYOff, nXSize, nYSize, 
                            nBufXSize, nBufYSize );
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

CPLErr OGDIRasterBand::EstablishAccess( int nXOff, int nYOff, 
                                        int nWinXSize, int nWinYSize, 
                                        int nBufXSize, int nBufYSize )

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
        
        CPLDebug( "OGDIRasterBand", "<EstablishAccess: SelectLayer(%s)>",
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
    double       dfNSTolerance = 0.0000001;

    sWin.west = nXOff * poODS->sGlobalBounds.ew_res
        + poODS->sGlobalBounds.west;
    sWin.east = (nXOff+nWinXSize) * poODS->sGlobalBounds.ew_res
        + poODS->sGlobalBounds.west;
    sWin.ew_res = poODS->sGlobalBounds.ew_res*(nWinXSize/(double)nBufXSize);

    sWin.north = poODS->sGlobalBounds.north 
        - nYOff*poODS->sGlobalBounds.ns_res;
    if( nBufYSize == 1 && nWinYSize == 1 )
    {
        sWin.ns_res = sWin.ew_res 
            * (poODS->sGlobalBounds.ns_res / poODS->sGlobalBounds.ew_res);
        nWinYSize = (int) ((sWin.north - poODS->sGlobalBounds.south + sWin.ns_res*0.9)
                                / sWin.ns_res);

        sWin.south = sWin.north - nWinYSize * sWin.ns_res;
        dfNSTolerance = MAX(poODS->sCurrentBounds.ns_res,sWin.ns_res);
    }
    else if( nBufYSize == 1 )
    {
        sWin.ns_res = poODS->sGlobalBounds.ns_res
            *(nWinYSize/(double)nBufYSize);
        nWinYSize = (int) ((sWin.north - poODS->sGlobalBounds.south + sWin.ns_res*0.9)
                                / sWin.ns_res);

        sWin.south = sWin.north - nWinYSize * sWin.ns_res;
        dfNSTolerance = MAX(poODS->sCurrentBounds.ns_res,sWin.ns_res);
    }
    else
    {
        sWin.ns_res = poODS->sGlobalBounds.ns_res
            *(nWinYSize/(double)nBufYSize);
        sWin.south = sWin.north - nWinYSize * sWin.ns_res;
        dfNSTolerance = sWin.ns_res * 0.001;
    }

    if( poODS->nCurrentIndex != 0
        || ABS(sWin.west - poODS->sCurrentBounds.west) > 0.0001 
        || ABS(sWin.east - poODS->sCurrentBounds.east) > 0.0001 
        || ABS(sWin.north - (poODS->sCurrentBounds.north - poODS->nCurrentIndex * poODS->sCurrentBounds.ns_res)) > dfNSTolerance 
        || ABS(sWin.ew_res/poODS->sCurrentBounds.ew_res - 1.0) > 0.0001
        || ABS(sWin.ns_res - poODS->sCurrentBounds.ns_res) > dfNSTolerance )
    {
        CPLDebug( "OGDIRasterBand", 
                  "<EstablishAccess: Set Region(%d,%d,%d,%d,%d,%d>",
                  nXOff, nYOff, nWinXSize, nWinYSize, nBufXSize, nBufYSize );

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
    if( poCT != NULL )
        return GCI_PaletteIndex;
    else if( nOGDIImageType == 1 && eFamily == Image && nComponent == 0 )
        return GCI_RedBand;
    else if( nOGDIImageType == 1 && eFamily == Image && nComponent == 1 )
        return GCI_GreenBand;
    else if( nOGDIImageType == 1 && eFamily == Image && nComponent == 2 )
        return GCI_BlueBand;
    else if( nOGDIImageType == 1 && eFamily == Image && nComponent == 3 )
        return GCI_AlphaBand;
    else
        return GCI_Undefined;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *OGDIRasterBand::GetColorTable()

{
    return poCT;
}

/************************************************************************/
/*                             AdviseRead()                             */
/*                                                                      */
/*      Allow the application to give us a hint in advance how they     */
/*      want the data.                                                  */
/************************************************************************/

CPLErr OGDIRasterBand::AdviseRead( int nXOff, int nYOff, 
                                   int nXSize, int nYSize,
                                   int nBufXSize, int nBufYSize, 
                                   CPL_UNUSED GDALDataType eDT,
                                   CPL_UNUSED char **papszOptions )

{
    return EstablishAccess( nXOff, nYOff, nXSize, nYSize, 
                            nBufXSize, nBufYSize );
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
    papszSubDatasets = NULL;
}

/************************************************************************/
/*                           ~OGDIDataset()                            */
/************************************************************************/

OGDIDataset::~OGDIDataset()

{
    cln_DestroyClient( nClientID );
    CSLDestroy( papszSubDatasets );
    CPLFree( pszProjection );
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **OGDIDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "SUBDATASETS", NULL);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **OGDIDataset::GetMetadata( const char *pszDomain )

{
    if( pszDomain != NULL && EQUAL(pszDomain,"SUBDATASETS") )
        return papszSubDatasets;
    else
        return GDALDataset::GetMetadata( pszDomain );
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
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The OGDI driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Has the user hardcoded a layer and family in the URL?           */
/*      Honour quoted strings for the layer name, since some layers     */
/*      (ie. RPF/CADRG) have embedded colons.                           */
/* -------------------------------------------------------------------- */
    int       nC1=-1, nC2=-1, i, bInQuotes = FALSE;
    char      *pszURL = CPLStrdup(poOpenInfo->pszFilename);

    for( i = strlen(pszURL)-1; i > 0; i-- )
    {
        if( pszURL[i] == '/' )
            break;

        if( pszURL[i] == '"' && pszURL[i-1] != '\\' )
            bInQuotes = !bInQuotes;
            
        else if( pszURL[i] == ':' && !bInQuotes )
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
        CPLFree(pszURL);
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
        char	*pszLayerName = CPLStrdup( pszURL+nC2+1 );
        
        if( pszLayerName[0] == '"' )
        {
            int		nOut = 0;

            for( i = 1; pszLayerName[i] != '\0'; i++ )
            {
                if( pszLayerName[i+1] == '"' && pszLayerName[i] == '\\' )
                    pszLayerName[nOut++] = pszLayerName[++i];
                else if( pszLayerName[i] != '"' )
                    pszLayerName[nOut++] = pszLayerName[i];
                else 
                    break;
            }
            pszLayerName[nOut] = '\0';
        }

        if( EQUAL(pszURL+nC1+1,"Image") )
            papszImages = CSLAddString( papszImages, pszLayerName );
        else
            papszMatrices = CSLAddString( papszMatrices, pszLayerName );

        CPLFree( pszLayerName );
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
    
    if( (ECSERROR(psResult) || CPLAtof(ECSTEXT(psResult)) >= 3.1)
        && CSLCount(papszMatrices) == 0 
        && CSLCount(papszImages) == 0 )
    {
        CPLDebug( "OGDIDataset",
                  "While this is an OGDI datastore, it does not appear to\n"
                  "have any identifiable raster layers.  Perhaps it is a\n"
                  "vector datastore?" );
        cln_DestroyClient( nClientID );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    OGDIDataset 	*poDS;

    poDS = new OGDIDataset();

    poDS->nClientID = nClientID;
    poDS->SetDescription( poOpenInfo->pszFilename );

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
/*      If we have only one layer try to find the corresponding         */
/*      capabilities, and override the global bounds and resolution     */
/*      based on it.                                                    */
/* -------------------------------------------------------------------- */
    if( CSLCount(papszMatrices) + CSLCount(papszImages) == 1 )
    {
        if( CSLCount(papszMatrices) == 1 )
            OverrideGlobalInfo( poDS, papszMatrices[0] );
        else
            OverrideGlobalInfo( poDS, papszImages[0] );
    }

/* -------------------------------------------------------------------- */
/*      Otherwise setup a subdataset list.                              */
/* -------------------------------------------------------------------- */
    else
    {
        int	i;

        for( i = 0; papszMatrices != NULL && papszMatrices[i] != NULL; i++ )
            poDS->AddSubDataset( "Matrix", papszMatrices[i] );

        for( i = 0; papszImages != NULL && papszImages[i] != NULL; i++ )
            poDS->AddSubDataset( "Image", papszImages[i] );
    }
    
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
        if( CSLFindString( papszImages, papszMatrices[i] ) == -1 )
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

    CSLDestroy( papszMatrices );
    CSLDestroy( papszImages );

    return( poDS );
}

/************************************************************************/
/*                           AddSubDataset()                            */
/************************************************************************/

void OGDIDataset::AddSubDataset( const char *pszType, const char *pszLayer )

{
    char	szName[80];
    int		nCount = CSLCount( papszSubDatasets ) / 2;

    sprintf( szName, "SUBDATASET_%d_NAME", nCount+1 );
    papszSubDatasets = 
        CSLSetNameValue( papszSubDatasets, szName, 
              CPLSPrintf( "%s:\"%s\":%s", GetDescription(), pszLayer, pszType ) );

    sprintf( szName, "SUBDATASET_%d_DESC", nCount+1 );
    papszSubDatasets = 
        CSLSetNameValue( papszSubDatasets, szName, 
              CPLSPrintf( "%s as %s", pszLayer, pszType ) );
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
        {
            *ppapszImages = CSLAddString( *ppapszImages, psLayer->name );
        }
        if( psLayer->families[Matrix] )
        {
            *ppapszMatrices = CSLAddString( *ppapszMatrices, psLayer->name );
        }
    }

    return CE_None;
}

/************************************************************************/
/*                         OverrideGlobalInfo()                         */
/*                                                                      */
/*      Override the global bounds and resolution based on a layers     */
/*      capabilities, if possible.                                      */
/************************************************************************/

CPLErr OGDIDataset::OverrideGlobalInfo( OGDIDataset *poDS,
                                        const char *pszLayer )

{
    const ecs_LayerCapabilities	*psLayer;
    int		iLayer;

    for( iLayer = 0; 
         (psLayer = cln_GetLayerCapabilities(poDS->nClientID,iLayer)) != NULL;
         iLayer++ )
    {
        if( EQUAL(psLayer->name, pszLayer) )
        {
            poDS->sGlobalBounds.north = psLayer->srs_north;
            poDS->sGlobalBounds.south = psLayer->srs_south;
            poDS->sGlobalBounds.east = psLayer->srs_east;
            poDS->sGlobalBounds.west = psLayer->srs_west;
            poDS->sGlobalBounds.ew_res = psLayer->srs_ewres;
            poDS->sGlobalBounds.ns_res = psLayer->srs_nsres;
        }
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

    if (! GDAL_CHECK_VERSION("GDAL/OGDI driver"))
        return;
    
    if( GDALGetDriverByName( "OGDI" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "OGDI" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "OGDI Bridge" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_ogdi.html" );
        poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );

        poDriver->pfnOpen = OGDIDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

