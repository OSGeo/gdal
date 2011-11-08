/******************************************************************************
 * $Id $
 *
 * Project:  SPOT Dimap Driver
 * Purpose:  Implementation of SPOT Dimap driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 * Docs: http://www.spotimage.fr/dimap/spec/documentation/refdoc.htm
 * 
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
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

#include "gdal_pam.h"
#include "cpl_minixml.h"
#include "ogr_spatialref.h"
#include "gdal_proxy.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_DIMAP(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				DIMAPDataset				*/
/* ==================================================================== */
/************************************************************************/

class DIMAPDataset : public GDALPamDataset
{
    CPLXMLNode *psProduct;

    GDALDataset   *poImageDS;

    int           nGCPCount;
    GDAL_GCP      *pasGCPList;
    char          *pszGCPProjection;

    CPLString     osProjection;

    int           bHaveGeoTransform;
    double        adfGeoTransform[6];

    char          **papszXMLDimapMetadata;

  public:
		DIMAPDataset();
	        ~DIMAPDataset();
    
    virtual const char *GetProjectionRef(void);
    virtual CPLErr GetGeoTransform( double * );
    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();
    virtual char **GetMetadata( const char *pszDomain );
    virtual char **GetFileList(void);

    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );

    CPLXMLNode *GetProduct() { return psProduct; }
};

/************************************************************************/
/* ==================================================================== */
/*                        DIMAPWrapperRasterBand                        */
/* ==================================================================== */
/************************************************************************/
class DIMAPWrapperRasterBand : public GDALProxyRasterBand
{
  GDALRasterBand* poBaseBand;

  protected:
    virtual GDALRasterBand* RefUnderlyingRasterBand() { return poBaseBand; }

  public:
    DIMAPWrapperRasterBand( GDALRasterBand* poBaseBand )
        {
            this->poBaseBand = poBaseBand;
            eDataType = poBaseBand->GetRasterDataType();
            poBaseBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
        }
    ~DIMAPWrapperRasterBand() {}
};
/************************************************************************/
/* ==================================================================== */
/*				DIMAPDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             DIMAPDataset()                             */
/************************************************************************/

DIMAPDataset::DIMAPDataset()
{
    psProduct = NULL;

    nGCPCount = 0;
    pasGCPList = NULL;
    pszGCPProjection = CPLStrdup("");

    poImageDS = NULL;
    bHaveGeoTransform = FALSE;

    papszXMLDimapMetadata = NULL;
}

/************************************************************************/
/*                            ~DIMAPDataset()                             */
/************************************************************************/

DIMAPDataset::~DIMAPDataset()

{
    FlushCache();

    CPLDestroyXMLNode( psProduct );

    CPLFree( pszGCPProjection );
    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

    if( poImageDS != NULL )
        delete poImageDS;

    CSLDestroy(papszXMLDimapMetadata);
}

/************************************************************************/
/*                            GetMetadata()                             */
/*                                                                      */
/*      We implement special support for fetching the full product      */
/*      metadata as xml.                                                */
/************************************************************************/

char **DIMAPDataset::GetMetadata( const char *pszDomain )

{
    if( pszDomain && EQUAL(pszDomain,"xml:dimap") )
    {
        if (papszXMLDimapMetadata == NULL)
        {
            papszXMLDimapMetadata = (char **) CPLCalloc(sizeof(char*),2);
            papszXMLDimapMetadata[0] = CPLSerializeXMLTree( psProduct );
        }
        return papszXMLDimapMetadata;
    }
    else
        return GDALPamDataset::GetMetadata( pszDomain );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *DIMAPDataset::GetProjectionRef()

{
    if( strlen(osProjection) > 0 )
        return osProjection;
    else
        return GDALPamDataset::GetProjectionRef();
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr DIMAPDataset::GetGeoTransform( double *padfGeoTransform )

{
    if( bHaveGeoTransform )
    {
        memcpy( padfGeoTransform, adfGeoTransform, sizeof(double)*6 );
        return CE_None;
    }
    else
        return GDALPamDataset::GetGeoTransform( padfGeoTransform );
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **DIMAPDataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();
    char **papszImageFiles = poImageDS->GetFileList();

    papszFileList = CSLInsertStrings( papszFileList, -1, papszImageFiles );

    CSLDestroy( papszImageFiles );
    
    return papszFileList;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int DIMAPDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes >= 100 )
    {
        if( strstr((const char *) poOpenInfo->pabyHeader, 
                   "<Dimap_Document" ) == NULL )
            return FALSE;
        else
            return TRUE;
    }
    else if( poOpenInfo->bIsDirectory )
    {
        VSIStatBufL sStat;

        CPLString osMDFilename = 
            CPLFormCIFilename( poOpenInfo->pszFilename, "METADATA.DIM", NULL );
        
        if( VSIStatL( osMDFilename, &sStat ) == 0 )
            return TRUE;
        else
            return FALSE;
    }

    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *DIMAPDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify( poOpenInfo ) )
        return NULL;
        
/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The DIMAP driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }
/* -------------------------------------------------------------------- */
/*      Get the metadata filename.                                      */
/* -------------------------------------------------------------------- */
    CPLString osMDFilename;

    if( poOpenInfo->bIsDirectory )
    {
        osMDFilename = 
            CPLFormCIFilename( poOpenInfo->pszFilename, "METADATA.DIM", NULL );
    }
    else
        osMDFilename = poOpenInfo->pszFilename;

/* -------------------------------------------------------------------- */
/*      Ingest the xml file.                                            */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psProduct, *psImageAttributes;

    psProduct = CPLParseXMLFile( osMDFilename );
    if( psProduct == NULL )
        return NULL;

    CPLXMLNode *psDoc = CPLGetXMLNode( psProduct, "=Dimap_Document" );
    psImageAttributes = CPLGetXMLNode( psDoc, "Raster_Dimensions" );
    if( psImageAttributes == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to find <Raster_Dimensions> in document." );
        return NULL;
    }


/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    DIMAPDataset *poDS = new DIMAPDataset();

    poDS->psProduct = psProduct;

/* -------------------------------------------------------------------- */
/*      Get overall image information.                                  */
/* -------------------------------------------------------------------- */
#ifdef DEBUG
    int nBands = 
        atoi(CPLGetXMLValue( psImageAttributes, "NBANDS", "-1" ));
#endif

    poDS->nRasterXSize = 
        atoi(CPLGetXMLValue( psImageAttributes, "NCOLS", "-1" ));
    poDS->nRasterYSize = 
        atoi(CPLGetXMLValue( psImageAttributes, "NROWS", "-1" ));

/* -------------------------------------------------------------------- */
/*      Get the name of the underlying file.                            */
/* -------------------------------------------------------------------- */
    const char *pszHref = CPLGetXMLValue(
        psDoc, "Data_Access.Data_File.DATA_FILE_PATH.href", "" );
    CPLString osPath = CPLGetPath(osMDFilename);
    CPLString osImageFilename = 
        CPLFormFilename( osPath, pszHref, NULL );
                                   
/* -------------------------------------------------------------------- */
/*      Try and open the file.                                          */
/* -------------------------------------------------------------------- */
    poDS->poImageDS = (GDALDataset *) GDALOpen( osImageFilename, GA_ReadOnly );
    if( poDS->poImageDS == NULL )
    {
        delete poDS;
        return NULL;
    }
        
/* -------------------------------------------------------------------- */
/*      Attach the bands.                                               */
/* -------------------------------------------------------------------- */
    int iBand;
    CPLAssert( nBands == poDS->poImageDS->GetRasterCount() );

    for( iBand = 1; iBand <= poDS->poImageDS->GetRasterCount(); iBand++ )
        poDS->SetBand( iBand, new DIMAPWrapperRasterBand(poDS->poImageDS->GetRasterBand( iBand )) );

/* -------------------------------------------------------------------- */
/*      Try to collect simple insertion point.                          */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psGeoLoc =  
        CPLGetXMLNode( psDoc, "Geoposition.Geoposition_Insert" );

    if( psGeoLoc != NULL )
    {
        poDS->bHaveGeoTransform = TRUE;
        poDS->adfGeoTransform[0] = atof(CPLGetXMLValue(psGeoLoc,"ULXMAP","0"));
        poDS->adfGeoTransform[1] = atof(CPLGetXMLValue(psGeoLoc,"XDIM","0"));
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = atof(CPLGetXMLValue(psGeoLoc,"ULYMAP","0"));
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = -atof(CPLGetXMLValue(psGeoLoc,"YDIM","0"));
    }

/* -------------------------------------------------------------------- */
/*      Collect GCPs.                                                   */
/* -------------------------------------------------------------------- */
    psGeoLoc = CPLGetXMLNode( psDoc, "Geoposition.Geoposition_Points" );

    if( psGeoLoc != NULL )
    {
        CPLXMLNode *psNode;					       

        // count gcps.
        poDS->nGCPCount = 0;
        for( psNode = psGeoLoc->psChild; psNode != NULL;
             psNode = psNode->psNext )
        {
            if( EQUAL(psNode->pszValue,"Tie_Point") )
                poDS->nGCPCount++ ;
        }

        poDS->pasGCPList = (GDAL_GCP *) 
            CPLCalloc(sizeof(GDAL_GCP),poDS->nGCPCount);
        
        poDS->nGCPCount = 0;
        
        for( psNode = psGeoLoc->psChild; psNode != NULL;
             psNode = psNode->psNext )
        {
            char	szID[32];
            GDAL_GCP   *psGCP = poDS->pasGCPList + poDS->nGCPCount;
            
            if( !EQUAL(psNode->pszValue,"Tie_Point") )
                continue;

            poDS->nGCPCount++ ;

            sprintf( szID, "%d", poDS->nGCPCount );
            psGCP->pszId = CPLStrdup( szID );
            psGCP->pszInfo = CPLStrdup("");
            psGCP->dfGCPPixel = 
                atof(CPLGetXMLValue(psNode,"TIE_POINT_DATA_X","0"))-0.5;
            psGCP->dfGCPLine = 
                atof(CPLGetXMLValue(psNode,"TIE_POINT_DATA_Y","0"))-0.5;
            psGCP->dfGCPX = 
                atof(CPLGetXMLValue(psNode,"TIE_POINT_CRS_X",""));
            psGCP->dfGCPY = 
                atof(CPLGetXMLValue(psNode,"TIE_POINT_CRS_Y",""));
            psGCP->dfGCPZ = 
                atof(CPLGetXMLValue(psNode,"TIE_POINT_CRS_Z",""));
        }
    }

/* -------------------------------------------------------------------- */
/*      Collect the CRS.  For now we look only for EPSG codes.          */
/* -------------------------------------------------------------------- */
    const char *pszSRS = CPLGetXMLValue( 
        psDoc, 
        "Coordinate_Reference_System.Horizontal_CS.HORIZONTAL_CS_CODE", 
        NULL );

    if( pszSRS != NULL )
    {
        OGRSpatialReference oSRS;
        if( oSRS.SetFromUserInput( pszSRS ) == OGRERR_NONE )
        {
            if( poDS->nGCPCount > 0 )
            {
                CPLFree(poDS->pszGCPProjection);
                oSRS.exportToWkt( &(poDS->pszGCPProjection) );
            }
            else
            {
                char *pszProjection = NULL;
                oSRS.exportToWkt( &pszProjection );
                poDS->osProjection = pszProjection;
                CPLFree( pszProjection );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Translate other metadata of interest.                           */
/* -------------------------------------------------------------------- */
    static const char *apszMetadataTranslation[] = 
        {
            "Production", "", 
            "Production.Facility", "FACILITY_", 
            "Dataset_Sources.Source_Information.Scene_Source", "",
            "Data_Processing", "",
            "Image_Interpretation.Spectral_Band_Info", "SPECTRAL_", 
            NULL, NULL
        };

    int iTrItem;
    
    for( iTrItem = 0; apszMetadataTranslation[iTrItem] != NULL; iTrItem += 2 )
    {
        CPLXMLNode *psParent = 
            CPLGetXMLNode( psDoc, apszMetadataTranslation[iTrItem] );

        if( psParent == NULL )
            continue;

        // hackey logic to support directly access a name/value entry
        // or a parent element with many name/values. 

        CPLXMLNode *psTarget;
        if( psParent->psChild != NULL 
            && psParent->psChild->eType == CXT_Text )
            psTarget = psParent;
        else
            psTarget = psParent->psChild;

        for( ; psTarget != NULL && psTarget != psParent; 
             psTarget = psTarget->psNext ) 
        {
            if( psTarget->eType == CXT_Element
                && psTarget->psChild != NULL
                && psTarget->psChild->eType == CXT_Text )
            {
                CPLString osName = apszMetadataTranslation[iTrItem+1];

                osName += psTarget->pszValue;
                poDS->SetMetadataItem( osName, psTarget->psChild->pszValue );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Set Band metadata from the <Spectral_Band_Info> content         */
/* -------------------------------------------------------------------- */

    CPLXMLNode *psImageInterpretationNode = 
            CPLGetXMLNode( psDoc, "Image_Interpretation" );
    if (psImageInterpretationNode != NULL)
    {
        CPLXMLNode *psSpectralBandInfoNode = psImageInterpretationNode->psChild;
        while (psSpectralBandInfoNode != NULL)
        {
            if (psSpectralBandInfoNode->eType == CXT_Element &&
                EQUAL(psSpectralBandInfoNode->pszValue, "Spectral_Band_Info"))
            {
                CPLXMLNode *psTag = psSpectralBandInfoNode->psChild;
                int nBandIndex = 0;
                while(psTag != NULL)
                {
                    if (psTag->eType == CXT_Element && psTag->psChild != NULL &&
                        psTag->psChild->eType == CXT_Text && psTag->pszValue != NULL)
                    {
                        if (EQUAL(psTag->pszValue, "BAND_INDEX"))
                        {
                            nBandIndex = atoi(psTag->psChild->pszValue);
                            if (nBandIndex <= 0 ||
                                nBandIndex > poDS->poImageDS->GetRasterCount())
                            {
                                CPLError(CE_Warning, CPLE_AppDefined,
                                         "Bad BAND_INDEX value : %s", psTag->psChild->pszValue);
                                nBandIndex = 0;
                            }
                        }
                        else if (nBandIndex >= 1)
                        {
                            poDS->GetRasterBand(nBandIndex)->SetMetadataItem(
                                    psTag->pszValue, psTag->psChild->pszValue);
                        }
                    }
                    psTag = psTag->psNext;
                }
            }
            psSpectralBandInfoNode = psSpectralBandInfoNode->psNext;
        }
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( osMDFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, osMDFilename );

    return( poDS );
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int DIMAPDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *DIMAPDataset::GetGCPProjection()

{
    return pszGCPProjection;
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *DIMAPDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                         GDALRegister_DIMAP()                         */
/************************************************************************/

void GDALRegister_DIMAP()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "DIMAP" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "DIMAP" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "SPOT DIMAP" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#DIMAP" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = DIMAPDataset::Open;
        poDriver->pfnIdentify = DIMAPDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

