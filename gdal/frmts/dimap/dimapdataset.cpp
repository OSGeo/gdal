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
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_minixml.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_proxy.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*                              DIMAPDataset                            */
/* ==================================================================== */
/************************************************************************/

class DIMAPDataset : public GDALPamDataset
{
    CPLXMLNode *psProduct;

    CPLXMLNode *psProductDim; /* DIMAP2, DIM_<product_id>.XML */
    CPLXMLNode *psProductStrip; /* DIMAP2, STRIP_<product_id>.XML */

    GDALDataset   *poImageDS;

    int           nGCPCount;
    GDAL_GCP      *pasGCPList;
    char          *pszGCPProjection;

    CPLString     osProjection;

    int           bHaveGeoTransform;
    double        adfGeoTransform[6];

    CPLString     osMDFilename;
    CPLString     osImageDSFilename;
    CPLString     osDIMAPFilename;
    int           nProductVersion;

    char          **papszXMLDimapMetadata;

  protected:
    virtual int         CloseDependentDatasets();

    int ReadImageInformation();
    int ReadImageInformation2(); /* DIMAP 2 */

    void SetMetadataFromXML(CPLXMLNode *psProduct, const char * const apszMetadataTranslation[]);
  public:
            DIMAPDataset();
            ~DIMAPDataset();

    virtual const char *GetProjectionRef(void);
    virtual CPLErr GetGeoTransform( double * );
    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();
    virtual char      **GetMetadataDomainList();
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
    DIMAPWrapperRasterBand( GDALRasterBand* poBaseBandIn )
        {
            this->poBaseBand = poBaseBandIn;
            eDataType = poBaseBand->GetRasterDataType();
            poBaseBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
        }
    ~DIMAPWrapperRasterBand() {}
};
/************************************************************************/
/* ==================================================================== */
/*                              DIMAPDataset                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             DIMAPDataset()                            */
/************************************************************************/

DIMAPDataset::DIMAPDataset() :
    psProduct(NULL),
    psProductDim(NULL),
    psProductStrip(NULL),
    poImageDS(NULL),
    nGCPCount(0),
    pasGCPList(NULL),
    bHaveGeoTransform(FALSE),
    nProductVersion(1),
    papszXMLDimapMetadata(NULL)
{
    pszGCPProjection = CPLStrdup("");
}

/************************************************************************/
/*                            ~DIMAPDataset()                           */
/************************************************************************/

DIMAPDataset::~DIMAPDataset()

{
    FlushCache();

    CPLDestroyXMLNode( psProduct );

    if( psProductDim != NULL )
        CPLDestroyXMLNode( psProductDim );
    if( psProductStrip != NULL )
        CPLDestroyXMLNode( psProductStrip );

    CPLFree( pszGCPProjection );
    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

    CSLDestroy(papszXMLDimapMetadata);

    CloseDependentDatasets();
}


/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int DIMAPDataset::CloseDependentDatasets()
{
    int bHasDroppedRef = GDALPamDataset::CloseDependentDatasets();

    if( poImageDS != NULL )
    {
        delete poImageDS;
        poImageDS = NULL;
        bHasDroppedRef = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Disconnect the bands so our destructor doesn't try and          */
/*      delete them since they really belonged to poImageDS.            */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < nBands; iBand++ )
        delete papoBands[iBand];
    nBands = 0;

    return bHasDroppedRef;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **DIMAPDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "xml:dimap", NULL);
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

    return GDALPamDataset::GetMetadata( pszDomain );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *DIMAPDataset::GetProjectionRef()

{
    if( strlen(osProjection) > 0 )
        return osProjection;

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
      if( ( strstr((const char *) poOpenInfo->pabyHeader,
                   "<Dimap_Document" ) == NULL ) &&
          ( strstr((const char *) poOpenInfo->pabyHeader,
                   "<PHR_DIMAP_Document" ) == NULL ) )
            return FALSE;
        else
            return TRUE;
    }
    else if( poOpenInfo->bIsDirectory )
    {
        /* DIMAP file */
        CPLString osMDFilename =
            CPLFormCIFilename( poOpenInfo->pszFilename, "METADATA.DIM", NULL );

        VSIStatBufL sStat;
        if( VSIStatL( osMDFilename, &sStat ) == 0 )
        {
            /* Make sure this is really a Dimap format */
            GDALOpenInfo  oOpenInfo( osMDFilename, GA_ReadOnly, NULL );
            if( oOpenInfo.nHeaderBytes >= 100 )
            {
                if( strstr((const char *) oOpenInfo.pabyHeader,
                           "<Dimap_Document" ) == NULL )
                    return FALSE;

                return TRUE;
            }
        }
        else
        {
            /* DIMAP 2 file */
            osMDFilename =
                    CPLFormCIFilename( poOpenInfo->pszFilename, "VOL_PHR.XML", NULL );

            if( VSIStatL( osMDFilename, &sStat ) == 0 )
                    return TRUE;

            return FALSE;
        }
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
    CPLString osMDFilename, osImageDSFilename, osDIMAPFilename;

    if( poOpenInfo->bIsDirectory )
    {
        VSIStatBufL sStat;

        osMDFilename =
            CPLFormCIFilename( poOpenInfo->pszFilename, "METADATA.DIM", NULL );

        /* DIMAP2 */
        if( VSIStatL( osMDFilename, &sStat ) != 0 )
        osMDFilename =
                CPLFormCIFilename( poOpenInfo->pszFilename, "VOL_PHR.XML", NULL );
    }
    else
        osMDFilename = poOpenInfo->pszFilename;

/* -------------------------------------------------------------------- */
/*      Ingest the xml file.                                            */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psProductDim = NULL, *psProductStrip = NULL;

    CPLXMLNode *psProduct = CPLParseXMLFile( osMDFilename );
    if( psProduct == NULL )
        return NULL;

    CPLXMLNode *psDoc = CPLGetXMLNode( psProduct, "=Dimap_Document" );
    if( !psDoc )
        psDoc = CPLGetXMLNode( psProduct, "=PHR_DIMAP_Document" );

    /* We check the for the tag Metadata_Identification.METADATA_FORMAT.
    *  The metadata will be set to 2.0 for DIMAP2 */
    int nProductVersion = 1;

    double dfMetadataFormatVersion
        = CPLAtof( CPLGetXMLValue(
            CPLGetXMLNode(psDoc, "Metadata_Identification.METADATA_FORMAT"),
            "version", "1") );

    if( dfMetadataFormatVersion >= 2.0 )
    {
        nProductVersion = 2;
    }

    /* Check needed information for the DIMAP format */
    if (nProductVersion == 1)
    {
        CPLXMLNode *psImageAttributes
            = CPLGetXMLNode( psDoc, "Raster_Dimensions" );
        if( psImageAttributes == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                "Failed to find <Raster_Dimensions> in document." );
            CPLDestroyXMLNode(psProduct);
            return NULL;
        }
    }
    else /* DIMAP2 */
    {
        /* Verify the presence of the DIMAP product file */
        CPLXMLNode *psDatasetComponents = CPLGetXMLNode(psDoc, "Dataset_Content.Dataset_Components");

        if( psDatasetComponents == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                "Failed to find <Dataset_Components> in document." );
            CPLDestroyXMLNode(psProduct);
            return NULL;
        }

        CPLXMLNode *psDatasetComponent = psDatasetComponents->psChild;

        if( CPLGetXMLNode(psDoc, "Raster_Data") )
        {
            osDIMAPFilename = osMDFilename;
        }

        for( ; osDIMAPFilename.size() == 0 && psDatasetComponent != NULL;
                psDatasetComponent = psDatasetComponent->psNext )
        {
            const char* pszComponentType = CPLGetXMLValue(psDatasetComponent, "COMPONENT_TYPE","");
            if( strcmp(pszComponentType, "DIMAP") == 0 )
            {
                const char *pszHref = CPLGetXMLValue(
                        psDatasetComponent, "COMPONENT_PATH.href", "" );

                if( strlen(pszHref) > 0 ) /* DIMAP product found*/
                {
                    if( poOpenInfo->bIsDirectory )
                    {
                        osDIMAPFilename =
                            CPLFormCIFilename( poOpenInfo->pszFilename, pszHref, NULL );
                    }
                    else
                    {
                        CPLString osPath = CPLGetPath(osMDFilename);
                        osDIMAPFilename =
                            CPLFormFilename( osPath, pszHref, NULL );
                    }

                    /* Data file might be specified there */
                    const char *pszDataFileHref = CPLGetXMLValue(
                        psDatasetComponent, "Data_Files.Data_File.DATA_FILE_PATH.href", "" );

                    if( strlen(pszDataFileHref) > 0 )
                    {
                        CPLString osPath = CPLGetPath(osMDFilename);
                        osImageDSFilename =
                            CPLFormFilename( osPath, pszDataFileHref, NULL );
                    }

                    break;
                }
            }
        }

        psProductDim = CPLParseXMLFile( osDIMAPFilename );
        if( psProductDim == NULL )
        {
            CPLDestroyXMLNode(psProduct);
            return NULL;
        }

        /* We need the STRIP_<product_id>.XML file for a few metadata */
        CPLXMLNode *psDocDim = CPLGetXMLNode( psProductDim, "=Dimap_Document" );
        if( !psDocDim )
            psDocDim = CPLGetXMLNode( psProductDim, "=PHR_DIMAP_Document" );

        CPLXMLNode *psDatasetSources = CPLGetXMLNode(psDocDim, "Dataset_Sources");
        if( psDatasetSources != NULL )
        {
            CPLString osSTRIPFilename;
            CPLXMLNode *psDatasetSource = psDatasetSources->psChild;

            for( ; psDatasetSource != NULL; psDatasetSource = psDatasetSource->psNext )
            {
                const char* pszSourceType = CPLGetXMLValue(psDatasetSource, "SOURCE_TYPE","");
                if( strcmp(pszSourceType, "Strip_Source") == 0 )
                {
                    const char *pszHref = CPLGetXMLValue(
                        psDatasetSource, "Component.COMPONENT_PATH.href", "" );

                    if( strlen(pszHref) > 0 ) /* STRIP product found*/
                    {
                        CPLString osPath = CPLGetPath(osDIMAPFilename);
                        osSTRIPFilename =
                            CPLFormCIFilename( osPath, pszHref, NULL );

                        break;
                    }
                }
            }

            psProductStrip = CPLParseXMLFile( osSTRIPFilename );
        }
    }

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    DIMAPDataset *poDS = new DIMAPDataset();

    poDS->psProduct = psProduct;
    poDS->psProductDim = psProductDim;
    poDS->psProductStrip = psProductStrip;
    poDS->nProductVersion = nProductVersion;
    poDS->osMDFilename = osMDFilename;
    poDS->osImageDSFilename = osImageDSFilename;
    poDS->osDIMAPFilename = osDIMAPFilename;

    int res = TRUE;
    if( nProductVersion == 2 )
        res = poDS->ReadImageInformation2();
    else
        res = poDS->ReadImageInformation();

    if( res == FALSE )
    {
        delete poDS;
        return NULL;
    }

    return( poDS );
}


/************************************************************************/
/*               ReadImageInformation() DIMAP Version 1                 */
/************************************************************************/

int DIMAPDataset::ReadImageInformation()
{
    CPLXMLNode *psDoc = CPLGetXMLNode( psProduct, "=Dimap_Document" );
    if( !psDoc )
        psDoc = CPLGetXMLNode( psProduct, "=PHR_DIMAP_Document" );

    CPLXMLNode *psImageAttributes = CPLGetXMLNode( psDoc, "Raster_Dimensions" );

/* -------------------------------------------------------------------- */
/*      Get overall image information.                                  */
/* -------------------------------------------------------------------- */
#ifdef DEBUG
    int l_nBands =
        atoi(CPLGetXMLValue( psImageAttributes, "NBANDS", "-1" ));
#endif

    nRasterXSize =
        atoi(CPLGetXMLValue( psImageAttributes, "NCOLS", "-1" ));
    nRasterYSize =
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

    poImageDS = (GDALDataset *) GDALOpen( osImageFilename, GA_ReadOnly );
    if( poImageDS == NULL )
    {
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Attach the bands.                                               */
/* -------------------------------------------------------------------- */
    CPLAssert( l_nBands == poImageDS->GetRasterCount() );

    for( int iBand = 1; iBand <= poImageDS->GetRasterCount(); iBand++ )
        SetBand( iBand, new DIMAPWrapperRasterBand(poImageDS->GetRasterBand( iBand )) );

/* -------------------------------------------------------------------- */
/*      Try to collect simple insertion point.                          */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psGeoLoc =
        CPLGetXMLNode( psDoc, "Geoposition.Geoposition_Insert" );

    if( psGeoLoc != NULL )
    {
        bHaveGeoTransform = TRUE;
        adfGeoTransform[0] = CPLAtof(CPLGetXMLValue(psGeoLoc,"ULXMAP","0"));
        adfGeoTransform[1] = CPLAtof(CPLGetXMLValue(psGeoLoc,"XDIM","0"));
        adfGeoTransform[2] = 0.0;
        adfGeoTransform[3] = CPLAtof(CPLGetXMLValue(psGeoLoc,"ULYMAP","0"));
        adfGeoTransform[4] = 0.0;
        adfGeoTransform[5] = -CPLAtof(CPLGetXMLValue(psGeoLoc,"YDIM","0"));
    }
    else
    {
        // Try to get geotransform from underlying raster.
        if ( poImageDS->GetGeoTransform(adfGeoTransform) == CE_None )
            bHaveGeoTransform = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Collect GCPs.                                                   */
/* -------------------------------------------------------------------- */
    psGeoLoc = CPLGetXMLNode( psDoc, "Geoposition.Geoposition_Points" );

    if( psGeoLoc != NULL )
    {
        CPLXMLNode *psNode;

        // count gcps.
        nGCPCount = 0;
        for( psNode = psGeoLoc->psChild; psNode != NULL;
             psNode = psNode->psNext )
        {
            if( EQUAL(psNode->pszValue,"Tie_Point") )
                nGCPCount++ ;
        }

        pasGCPList = (GDAL_GCP *)
            CPLCalloc(sizeof(GDAL_GCP),nGCPCount);

        nGCPCount = 0;

        for( psNode = psGeoLoc->psChild; psNode != NULL;
             psNode = psNode->psNext )
        {
            char    szID[32];
            GDAL_GCP   *psGCP = pasGCPList + nGCPCount;

            if( !EQUAL(psNode->pszValue,"Tie_Point") )
                continue;

            nGCPCount++ ;

            snprintf( szID, sizeof(szID), "%d", nGCPCount );
            psGCP->pszId = CPLStrdup( szID );
            psGCP->pszInfo = CPLStrdup("");
            psGCP->dfGCPPixel =
                CPLAtof(CPLGetXMLValue(psNode,"TIE_POINT_DATA_X","0"))-0.5;
            psGCP->dfGCPLine =
                CPLAtof(CPLGetXMLValue(psNode,"TIE_POINT_DATA_Y","0"))-0.5;
            psGCP->dfGCPX =
                CPLAtof(CPLGetXMLValue(psNode,"TIE_POINT_CRS_X",""));
            psGCP->dfGCPY =
                CPLAtof(CPLGetXMLValue(psNode,"TIE_POINT_CRS_Y",""));
            psGCP->dfGCPZ =
                CPLAtof(CPLGetXMLValue(psNode,"TIE_POINT_CRS_Z",""));
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
            if( nGCPCount > 0 )
            {
                CPLFree(pszGCPProjection);
                oSRS.exportToWkt( &(pszGCPProjection) );
            }
            else
            {
                char *pszProjection = NULL;
                oSRS.exportToWkt( &pszProjection );
                osProjection = pszProjection;
                CPLFree( pszProjection );
            }
        }
    }
    else
    {
        // Check underlying raster for SRS. We have cases where
        // HORIZONTAL_CS_CODE is empty and the underlying raster
        // is georeferenced (rprinceley).
        if ( poImageDS->GetProjectionRef() )
        {
            osProjection = poImageDS->GetProjectionRef();
        }
    }

/* -------------------------------------------------------------------- */
/*      Translate other metadata of interest.                           */
/* -------------------------------------------------------------------- */
    static const char * const apszMetadataTranslation[] =
        {
            "Production", "",
            "Production.Facility", "FACILITY_",
            "Dataset_Sources.Source_Information.Scene_Source", "",
            "Data_Processing", "",
            "Image_Interpretation.Spectral_Band_Info", "SPECTRAL_",
            NULL, NULL
        };

    SetMetadataFromXML(psProduct, apszMetadataTranslation);

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
                                nBandIndex > poImageDS->GetRasterCount())
                            {
                                CPLError(CE_Warning, CPLE_AppDefined,
                                         "Bad BAND_INDEX value : %s", psTag->psChild->pszValue);
                                nBandIndex = 0;
                            }
                        }
                        else if (nBandIndex >= 1)
                        {
                            GetRasterBand(nBandIndex)->SetMetadataItem(
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
    SetDescription( osMDFilename );
    TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    oOvManager.Initialize( this, osMDFilename );

    return TRUE;
}


/************************************************************************/
/*               ReadImageInformation() DIMAP Version 2                 */
/************************************************************************/

int DIMAPDataset::ReadImageInformation2()
{
    CPLXMLNode *psDoc = CPLGetXMLNode( psProductDim, "=Dimap_Document" );
    if( !psDoc )
        psDoc = CPLGetXMLNode( psProductDim, "=PHR_DIMAP_Document" );

    CPLXMLNode *psImageAttributes = CPLGetXMLNode( psDoc, "Raster_Data.Raster_Dimensions" );
    if( psImageAttributes == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
            "Failed to find <Raster_Dimensions> in document." );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Get overall image information.                                  */
/* -------------------------------------------------------------------- */
#ifdef DEBUG
    int l_nBands =
        atoi(CPLGetXMLValue( psImageAttributes, "NBANDS", "-1" ));
#endif

    nRasterXSize =
        atoi(CPLGetXMLValue( psImageAttributes, "NCOLS", "-1" ));
    nRasterYSize =
        atoi(CPLGetXMLValue( psImageAttributes, "NROWS", "-1" ));

/* -------------------------------------------------------------------- */
/*      Get the name of the underlying file.                            */
/* -------------------------------------------------------------------- */

    /* If the data file was not in the product file, it should be here */
    if ( osImageDSFilename.size() == 0 )
    {
        const char *pszHref = CPLGetXMLValue(
                            psDoc, "Raster_Data.Data_Access.Data_Files.Data_File.DATA_FILE_PATH.href", "" );
        if( strlen(pszHref) > 0 )
        {
            CPLString osPath = CPLGetPath( osDIMAPFilename );
            osImageDSFilename =
                CPLFormCIFilename( osPath, pszHref, NULL );
        }
        else
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                "Failed to find <DATA_FILE_PATH> in document." );
            return FALSE;
        }
    }


/* -------------------------------------------------------------------- */
/*      Try and open the file.                                          */
/* -------------------------------------------------------------------- */
    poImageDS = (GDALDataset *) GDALOpen( osImageDSFilename, GA_ReadOnly );
    if( poImageDS == NULL )
    {
        return FALSE;
    }


/* -------------------------------------------------------------------- */
/*      Attach the bands.                                               */
/* -------------------------------------------------------------------- */
    CPLAssert( l_nBands == poImageDS->GetRasterCount() );

    for( int iBand = 1; iBand <= poImageDS->GetRasterCount(); iBand++ )
        SetBand( iBand, new DIMAPWrapperRasterBand(poImageDS->GetRasterBand( iBand )) );

/* -------------------------------------------------------------------- */
/*      Try to collect simple insertion point.                          */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psGeoLoc =
        CPLGetXMLNode( psDoc, "Geoposition.Geoposition_Insert" );

    if( psGeoLoc != NULL )
    {
        bHaveGeoTransform = TRUE;
        adfGeoTransform[0] = CPLAtof(CPLGetXMLValue(psGeoLoc,"ULXMAP","0"));
        adfGeoTransform[1] = CPLAtof(CPLGetXMLValue(psGeoLoc,"XDIM","0"));
        adfGeoTransform[2] = 0.0;
        adfGeoTransform[3] = CPLAtof(CPLGetXMLValue(psGeoLoc,"ULYMAP","0"));
        adfGeoTransform[4] = 0.0;
        adfGeoTransform[5] = -CPLAtof(CPLGetXMLValue(psGeoLoc,"YDIM","0"));
    }
    else
    {
        // Try to get geotransform from underlying raster.
        if ( poImageDS->GetGeoTransform(adfGeoTransform) == CE_None )
            bHaveGeoTransform = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Collect the CRS.  For now we look only for EPSG codes.          */
/* -------------------------------------------------------------------- */
    const char *pszSRS = CPLGetXMLValue(
        psDoc,
        "Coordinate_Reference_System.Projected_CRS.PROJECTED_CRS_CODE",
        NULL );
    if( pszSRS == NULL )
        pszSRS = CPLGetXMLValue(
            psDoc,
            "Coordinate_Reference_System.Geodetic_CRS.GEODETIC_CRS_CODE",
            NULL );

    if( pszSRS != NULL )
    {
        OGRSpatialReference oSRS;
        if( oSRS.SetFromUserInput( pszSRS ) == OGRERR_NONE )
        {
            if( nGCPCount > 0 )
            {
                CPLFree(pszGCPProjection);
                oSRS.exportToWkt( &(pszGCPProjection) );
            }
            else
            {
                char *pszProjection = NULL;
                oSRS.exportToWkt( &pszProjection );
                osProjection = pszProjection;
                CPLFree( pszProjection );
            }
        }
    }
    else
    {
        // Check underlying raster for SRS. We have cases where
        // HORIZONTAL_CS_CODE is empty and the underlying raster
        // is georeferenced (rprinceley).
        if ( poImageDS->GetProjectionRef() )
        {
            osProjection = poImageDS->GetProjectionRef();
        }
    }

/* -------------------------------------------------------------------- */
/*      Translate other metadata of interest: DIM_<product_name>.XML    */
/* -------------------------------------------------------------------- */

    static const char * const apszMetadataTranslationDim[] =
    {
        "Product_Information.Delivery_Identification", "DATASET_",
            "Product_Information.Producer_Information", "DATASET_",
        "Dataset_Sources.Source_Identification.Strip_Source", "",
        "Processing_Information.Production_Facility", "FACILITY_",
        "Processing_Information.Product_Settings", "",
        "Processing_Information.Product_Settings.Geometric_Settings", "GEOMETRIC_",
        "Quality_Assessment.Imaging_Quality_Measurement", "CLOUDCOVER_",
            NULL, NULL
    };

    SetMetadataFromXML(psProductDim, apszMetadataTranslationDim);

/* -------------------------------------------------------------------- */
/*      Translate other metadata of interest: STRIP_<product_name>.XML    */
/* -------------------------------------------------------------------- */

    static const char * const apszMetadataTranslationStrip[] =
    {
        "Catalog.Full_Strip.Notations.Cloud_And_Quality_Notation.Data_Strip_Notation", "CLOUDCOVER_",
        "Acquisition_Configuration.Platform_Configuration.Ephemeris_Configuration", "EPHEMERIS_",
            NULL, NULL
    };

    if( psProductStrip != NULL )
        SetMetadataFromXML(psProductStrip, apszMetadataTranslationStrip);

/* -------------------------------------------------------------------- */
/*      Set Band metadata from the <Band_Radiance> and                  */
/*                                <Band_Spectral_Range> content         */
/* -------------------------------------------------------------------- */

    CPLXMLNode *psImageInterpretationNode =
        CPLGetXMLNode( psDoc,
                    "Radiometric_Data.Radiometric_Calibration.Instrument_Calibration.Band_Measurement_List" );
    if (psImageInterpretationNode != NULL)
    {
        CPLXMLNode *psSpectralBandInfoNode = psImageInterpretationNode->psChild;
        while (psSpectralBandInfoNode != NULL)
        {
            if (psSpectralBandInfoNode->eType == CXT_Element &&
                (EQUAL(psSpectralBandInfoNode->pszValue, "Band_Radiance") ||
                 EQUAL(psSpectralBandInfoNode->pszValue, "Band_Spectral_Range") ||
                 EQUAL(psSpectralBandInfoNode->pszValue, "Band_Solar_Irradiance")))
            {
                CPLString osName;

                if (EQUAL(psSpectralBandInfoNode->pszValue, "Band_Radiance"))
                    osName = "RADIANCE_";
                else if (EQUAL(psSpectralBandInfoNode->pszValue, "Band_Spectral_Range"))
                    osName = "SPECTRAL_RANGE_";
                else if (EQUAL(psSpectralBandInfoNode->pszValue, "Band_Solar_Irradiance"))
                    osName = "SOLAR_IRRADIANCE_";

                CPLXMLNode *psTag = psSpectralBandInfoNode->psChild;
                int nBandIndex = 0;
                while(psTag != NULL)
                {
                    if (psTag->eType == CXT_Element && psTag->psChild != NULL &&
                        psTag->psChild->eType == CXT_Text && psTag->pszValue != NULL)
                    {
                        if (EQUAL(psTag->pszValue, "BAND_ID"))
                        {
                            /* BAND_ID is: B0, B1, .... P */
                            if (!EQUAL(psTag->psChild->pszValue, "P"))
                            {
                                if (strlen(psTag->psChild->pszValue) < 2) /* should not happen */
                                {
                                    CPLError(CE_Warning, CPLE_AppDefined,
                                        "Bad BAND_INDEX value : %s", psTag->psChild->pszValue);
                                    nBandIndex = 0;
                                }
                                else
                                {
                                    nBandIndex = atoi(&psTag->psChild->pszValue[1]) + 1;
                                    if (nBandIndex <= 0 ||
                                    nBandIndex > poImageDS->GetRasterCount())
                                    {
                                    CPLError(CE_Warning, CPLE_AppDefined,
                                        "Bad BAND_INDEX value : %s", psTag->psChild->pszValue);
                                    nBandIndex = 0;
                                    }
                                }
                            }
                        }
                        else if (nBandIndex >= 1)
                        {
                            CPLString osMDName = osName;
                            osMDName += psTag->pszValue;

                            GetRasterBand(nBandIndex)->SetMetadataItem(
                                osMDName, psTag->psChild->pszValue);
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
    SetDescription( osMDFilename );
    TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    oOvManager.Initialize( this, osMDFilename );

    return TRUE;
}

/************************************************************************/
/*                          SetMetadataFromXML()                        */
/************************************************************************/

void DIMAPDataset::SetMetadataFromXML(CPLXMLNode *psProductIn, const char * const apszMetadataTranslation[])
{
    CPLXMLNode *psDoc = CPLGetXMLNode( psProductIn, "=Dimap_Document" );
    if( psDoc == NULL )
    {
      psDoc = CPLGetXMLNode( psProductIn, "=PHR_DIMAP_Document" );
    }

    for( int iTrItem = 0; apszMetadataTranslation[iTrItem] != NULL; iTrItem += 2 )
    {
        CPLXMLNode *psParent =
            CPLGetXMLNode( psDoc, apszMetadataTranslation[iTrItem] );

        if( psParent == NULL )
            continue;

        // Hackey logic to support directly access a name/value entry
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
                && psTarget->psChild != NULL)
            {
                CPLString osName = apszMetadataTranslation[iTrItem+1];

                if (psTarget->psChild->eType == CXT_Text)
                {
                    osName += psTarget->pszValue;
                    SetMetadataItem( osName, psTarget->psChild->pszValue );
                }
                else if (psTarget->psChild->eType == CXT_Attribute)
                {
                    /* find the tag value, at the end of the attributes */
                    CPLXMLNode *psNode = psTarget->psChild;
                    for( ; psNode != NULL;  psNode = psNode->psNext )
                    {
                        if (psNode->eType == CXT_Attribute)
                            continue;
                        else if (psNode->eType == CXT_Text)
                        {
                            osName += psTarget->pszValue;
                            SetMetadataItem( osName, psNode->pszValue );
                        }
                    }
                }
            }
        }
    }
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
    if( GDALGetDriverByName( "DIMAP" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "DIMAP" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "SPOT DIMAP" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "frmt_various.html#DIMAP" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = DIMAPDataset::Open;
    poDriver->pfnIdentify = DIMAPDataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
