/******************************************************************************
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
#include "mdreader/reader_pleiades.h"
#include "vrtdataset.h"
#include <map>

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                              DIMAPDataset                            */
/* ==================================================================== */
/************************************************************************/

class DIMAPDataset : public GDALPamDataset
{
    CPLXMLNode *psProduct;

    CPLXMLNode *psProductDim;  // DIMAP2, DIM_<product_id>.XML
    CPLXMLNode *psProductStrip;  // DIMAP2, STRIP_<product_id>.XML
    CPLString   osRPCFilename;  // DIMAP2, RPC_<product_id>.XML

    VRTDataset    *poVRTDS;

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
    virtual int         CloseDependentDatasets() override;

    int ReadImageInformation();
    int ReadImageInformation2();  // DIMAP 2.

    void SetMetadataFromXML( CPLXMLNode *psProduct,
                             const char * const apszMetadataTranslation[] );

  public:
            DIMAPDataset();
    virtual ~DIMAPDataset();

    virtual const char *GetProjectionRef(void) override;
    virtual CPLErr GetGeoTransform( double * ) override;
    virtual int    GetGCPCount() override;
    virtual const char *GetGCPProjection() override;
    virtual const GDAL_GCP *GetGCPs() override;
    virtual char      **GetMetadataDomainList() override;
    virtual char **GetMetadata( const char *pszDomain ) override;
    virtual char **GetFileList(void) override;

    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );

    CPLXMLNode *GetProduct() { return psProduct; }
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
    poVRTDS(NULL),
    nGCPCount(0),
    pasGCPList(NULL),
    pszGCPProjection(CPLStrdup("")),
    bHaveGeoTransform(FALSE),
    nProductVersion(1),
    papszXMLDimapMetadata(NULL)
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 0.0;
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

    if( poVRTDS != NULL )
    {
        delete poVRTDS;
        poVRTDS = NULL;
        bHasDroppedRef = TRUE;
    }

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
        if( papszXMLDimapMetadata == NULL )
        {
            papszXMLDimapMetadata = reinterpret_cast<char **>(
                CPLCalloc(sizeof(char*), 2) );
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
    if( !osProjection.empty() )
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
    char **papszImageFiles = poVRTDS->GetFileList();

    papszFileList = CSLInsertStrings( papszFileList, -1, papszImageFiles );

    CSLDestroy( papszImageFiles );

    return papszFileList;
}

/************************************************************************/
/* ==================================================================== */
/*                            DIMAPRasterBand                           */
/* ==================================================================== */
/************************************************************************/

class DIMAPRasterBand : public GDALPamRasterBand
{
    friend class DIMAPDataset;

    VRTSourcedRasterBand *poVRTBand;

  public:
                   DIMAPRasterBand( DIMAPDataset *, int, VRTSourcedRasterBand * );
    virtual       ~DIMAPRasterBand() {}

    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg ) override;
    virtual int GetOverviewCount() override;
    virtual GDALRasterBand *GetOverview( int ) override;
    virtual CPLErr ComputeRasterMinMax( int bApproxOK,
                                        double adfMinMax[2] ) override;
    virtual CPLErr ComputeStatistics( int bApproxOK,
                                      double *pdfMin, double *pdfMax,
                                      double *pdfMean, double *pdfStdDev,
                                      GDALProgressFunc, void *pProgressData ) override;

    virtual CPLErr  GetHistogram( double dfMin, double dfMax,
                                  int nBuckets, GUIntBig * panHistogram,
                                  int bIncludeOutOfRange, int bApproxOK,
                                  GDALProgressFunc, void *pProgressData ) override;
};

/************************************************************************/
/*                          DIMAPRasterBand()                           */
/************************************************************************/

DIMAPRasterBand::DIMAPRasterBand( DIMAPDataset *poDIMAPDS, int nBandIn,
                                  VRTSourcedRasterBand *poVRTBandIn ) :
    poVRTBand(poVRTBandIn)
{
    poDS = poDIMAPDS;
    nBand = nBandIn;
    eDataType = poVRTBandIn->GetRasterDataType();

    poVRTBandIn->GetBlockSize( &nBlockXSize, &nBlockYSize );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr DIMAPRasterBand::IReadBlock( int iBlockX, int iBlockY, void *pBuffer )

{
    return poVRTBand->ReadBlock( iBlockX, iBlockY, pBuffer );
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr DIMAPRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                   int nXOff, int nYOff, int nXSize, int nYSize,
                                   void * pData, int nBufXSize, int nBufYSize,
                                   GDALDataType eBufType,
                                   GSpacing nPixelSpace, GSpacing nLineSpace,
                                   GDALRasterIOExtraArg* psExtraArg )

{
    if( GDALPamRasterBand::GetOverviewCount() > 0 )
    {
        return GDALPamRasterBand::IRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType,
            nPixelSpace, nLineSpace, psExtraArg );
    }

    // If not exist DIMAP overviews, try to use band source overviews.
    return poVRTBand->IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                 pData, nBufXSize, nBufYSize, eBufType,
                                 nPixelSpace, nLineSpace, psExtraArg );
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int DIMAPRasterBand::GetOverviewCount()
{
    if( GDALPamRasterBand::GetOverviewCount() > 0 )
    {
        return GDALPamRasterBand::GetOverviewCount();
    }
    return poVRTBand->GetOverviewCount();
}

/************************************************************************/
/*                             GetOverview()                            */
/************************************************************************/

GDALRasterBand *DIMAPRasterBand::GetOverview( int iOvr )
{
    if( GDALPamRasterBand::GetOverviewCount() > 0 )
    {
        return GDALPamRasterBand::GetOverview(iOvr);
    }
    return poVRTBand->GetOverview(iOvr);
}

/************************************************************************/
/*                         ComputeRasterMinMax()                        */
/************************************************************************/

CPLErr DIMAPRasterBand::ComputeRasterMinMax( int bApproxOK,
                                            double adfMinMax[2] )
{
    if( GDALPamRasterBand::GetOverviewCount() > 0 )
    {
        return GDALPamRasterBand::ComputeRasterMinMax(bApproxOK, adfMinMax);
    }
    return poVRTBand->ComputeRasterMinMax(bApproxOK, adfMinMax);
}

/************************************************************************/
/*                          ComputeStatistics()                         */
/************************************************************************/

CPLErr DIMAPRasterBand::ComputeStatistics( int bApproxOK,
                                           double *pdfMin, double *pdfMax,
                                           double *pdfMean, double *pdfStdDev,
                                           GDALProgressFunc pfnProgress,
                                           void *pProgressData )
{
    if( GDALPamRasterBand::GetOverviewCount() > 0 )
    {
        return GDALPamRasterBand::ComputeStatistics(
            bApproxOK, pdfMin, pdfMax, pdfMean, pdfStdDev,
            pfnProgress, pProgressData);
    }
    return poVRTBand->ComputeStatistics(bApproxOK, pdfMin, pdfMax, pdfMean,
                                        pdfStdDev, pfnProgress, pProgressData);
}

/************************************************************************/
/*                            GetHistogram()                            */
/************************************************************************/

CPLErr DIMAPRasterBand::GetHistogram( double dfMin, double dfMax,
                                      int nBuckets, GUIntBig *panHistogram,
                                      int bIncludeOutOfRange, int bApproxOK,
                                      GDALProgressFunc pfnProgress,
                                      void *pProgressData )
{
    if( GDALPamRasterBand::GetOverviewCount() > 0 )
    {
        return GDALPamRasterBand::GetHistogram(
            dfMin, dfMax, nBuckets, panHistogram, bIncludeOutOfRange,
            bApproxOK, pfnProgress, pProgressData);
    }
    return poVRTBand->GetHistogram(dfMin, dfMax, nBuckets, panHistogram,
            bIncludeOutOfRange, bApproxOK, pfnProgress, pProgressData);
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int DIMAPDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes >= 100 )
    {
        if( ( strstr(reinterpret_cast<char *>(poOpenInfo->pabyHeader),
                     "<Dimap_Document" ) == NULL ) &&
            ( strstr(reinterpret_cast<char *>(poOpenInfo->pabyHeader),
                     "<PHR_DIMAP_Document" ) == NULL ) )
            return FALSE;

        return TRUE;
    }
    else if( poOpenInfo->bIsDirectory )
    {
        // DIMAP file.
        CPLString osMDFilename =
            CPLFormCIFilename( poOpenInfo->pszFilename, "METADATA.DIM", NULL );

        VSIStatBufL sStat;
        if( VSIStatL( osMDFilename, &sStat ) == 0 )
        {
            // Make sure this is really a Dimap format.
            GDALOpenInfo  oOpenInfo( osMDFilename, GA_ReadOnly, NULL );
            if( oOpenInfo.nHeaderBytes >= 100 )
            {
                if( strstr(reinterpret_cast<char *>(oOpenInfo.pabyHeader),
                           "<Dimap_Document" ) == NULL )
                    return FALSE;

                return TRUE;
            }
        }
        else
        {
            // DIMAP 2 file.
            osMDFilename =
                CPLFormCIFilename( poOpenInfo->pszFilename,
                                   "VOL_PHR.XML", NULL );

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
                  "The DIMAP driver does not support update access to existing "
                  " datasets." );
        return NULL;
    }
/* -------------------------------------------------------------------- */
/*      Get the metadata filename.                                      */
/* -------------------------------------------------------------------- */
    CPLString osMDFilename;

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
    {
        osMDFilename = poOpenInfo->pszFilename;
    }

/* -------------------------------------------------------------------- */
/*      Ingest the xml file.                                            */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psProduct = CPLParseXMLFile( osMDFilename );
    if( psProduct == NULL )
        return NULL;

    CPLXMLNode *psDoc = CPLGetXMLNode( psProduct, "=Dimap_Document" );
    if( !psDoc )
        psDoc = CPLGetXMLNode( psProduct, "=PHR_DIMAP_Document" );

    // We check the for the tag Metadata_Identification.METADATA_FORMAT.
    // The metadata will be set to 2.0 for DIMAP2.
    double dfMetadataFormatVersion =
        CPLAtof( CPLGetXMLValue(
            CPLGetXMLNode(psDoc, "Metadata_Identification.METADATA_FORMAT"),
            "version", "1") );

    const int nProductVersion = dfMetadataFormatVersion >= 2.0 ? 2 : 1;

    CPLString osImageDSFilename;
    CPLString osDIMAPFilename;
    CPLString osRPCFilename;
    CPLXMLNode *psProductDim = NULL;
    CPLXMLNode *psProductStrip = NULL;

    // Check needed information for the DIMAP format.
    if( nProductVersion == 1 )
    {
        CPLXMLNode *psImageAttributes =
            CPLGetXMLNode( psDoc, "Raster_Dimensions" );
        if( psImageAttributes == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Failed to find <Raster_Dimensions> in document." );
            CPLDestroyXMLNode(psProduct);
            return NULL;
        }
    }
    else  // DIMAP2.
    {
        // Verify the presence of the DIMAP product file.
        CPLXMLNode *psDatasetComponents =
            CPLGetXMLNode(psDoc, "Dataset_Content.Dataset_Components");

        if( psDatasetComponents == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Failed to find <Dataset_Components> in document." );
            CPLDestroyXMLNode(psProduct);
            return NULL;
        }

        if( CPLGetXMLNode(psDoc, "Raster_Data") )
        {
            osDIMAPFilename = osMDFilename;
        }

        for( CPLXMLNode *psDatasetComponent = psDatasetComponents->psChild;
             osDIMAPFilename.empty() && psDatasetComponent != NULL;
             psDatasetComponent = psDatasetComponent->psNext )
        {
            const char* pszComponentType =
                CPLGetXMLValue(psDatasetComponent, "COMPONENT_TYPE","");
            if( strcmp(pszComponentType, "DIMAP") == 0 )
            {
                const char *pszHref = CPLGetXMLValue(
                        psDatasetComponent, "COMPONENT_PATH.href", "" );

                if( strlen(pszHref) > 0 )  // DIMAP product found.
                {
                    if( poOpenInfo->bIsDirectory )
                    {
                        osDIMAPFilename =
                            CPLFormCIFilename( poOpenInfo->pszFilename,
                                               pszHref, NULL );
                    }
                    else
                    {
                        CPLString osPath = CPLGetPath(osMDFilename);
                        osDIMAPFilename =
                            CPLFormFilename( osPath, pszHref, NULL );
                    }

                    // Data file might be specified there.
                    const char *pszDataFileHref = CPLGetXMLValue(
                        psDatasetComponent,
                        "Data_Files.Data_File.DATA_FILE_PATH.href",
                        "" );

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

        // We need the {STRIP|RPC}_<product_id>.XML file for a few metadata.
        CPLXMLNode *psDocDim = CPLGetXMLNode( psProductDim, "=Dimap_Document" );
        if( !psDocDim )
            psDocDim = CPLGetXMLNode( psProductDim, "=PHR_DIMAP_Document" );

        CPLXMLNode *psDatasetSources =
            CPLGetXMLNode(psDocDim, "Dataset_Sources");
        if( psDatasetSources != NULL )
        {
            CPLString osSTRIPFilename;

            for( CPLXMLNode *psDatasetSource = psDatasetSources->psChild;
                 psDatasetSource != NULL;
                 psDatasetSource = psDatasetSource->psNext )
            {
                const char* pszSourceType =
                    CPLGetXMLValue(psDatasetSource, "SOURCE_TYPE","");
                if( strcmp(pszSourceType, "Strip_Source") == 0 )
                {
                    const char *pszHref = CPLGetXMLValue(
                        psDatasetSource, "Component.COMPONENT_PATH.href",
                        "" );

                    if( strlen(pszHref) > 0 )  // STRIP product found.
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

        CPLXMLNode *psDatasetRFMComponents =
            CPLGetXMLNode(
                psDocDim,
                "Geoposition.Geoposition_Models.Rational_Function_Model");
        if( psDatasetRFMComponents != NULL )
        {
           for( CPLXMLNode *psDatasetRFMComponent =
                    psDatasetRFMComponents->psChild;
                psDatasetRFMComponent != NULL;
                psDatasetRFMComponent = psDatasetRFMComponent->psNext )
           {
              const char* pszComponentTitle =
                  CPLGetXMLValue(psDatasetRFMComponent, "COMPONENT_TITLE", "");
              if( strcmp(pszComponentTitle, "RPC Model") == 0 )
              {
                 const char *pszHref = CPLGetXMLValue(
                     psDatasetRFMComponent, "COMPONENT_PATH.href", "");

                 if( strlen(pszHref) > 0 )  // RPC product found.
                 {
                    CPLString osPath = CPLGetPath(osDIMAPFilename);
                    osRPCFilename =
                       CPLFormCIFilename(osPath, pszHref, NULL);

                    break;
                 }
              }
           }
        }
    }

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    DIMAPDataset *poDS = new DIMAPDataset();

    poDS->psProduct = psProduct;
    poDS->psProductDim = psProductDim;
    poDS->psProductStrip = psProductStrip;
    poDS->osRPCFilename = osRPCFilename;
    poDS->nProductVersion = nProductVersion;
    poDS->osMDFilename = osMDFilename;
    poDS->osImageDSFilename = osImageDSFilename;
    poDS->osDIMAPFilename = osDIMAPFilename;

    const int res = (nProductVersion == 2) ?
        poDS->ReadImageInformation2() :
        poDS->ReadImageInformation();

    if( res == FALSE )
    {
        delete poDS;
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*               ReadImageInformation() DIMAP Version 1                 */
/************************************************************************/

int DIMAPDataset::ReadImageInformation()
{
    CPLXMLNode *psDoc = CPLGetXMLNode( psProduct, "=Dimap_Document" );
    if( !psDoc )
        psDoc = CPLGetXMLNode( psProduct, "=PHR_DIMAP_Document" );

/* -------------------------------------------------------------------- */
/*      Get overall image information.                                  */
/* -------------------------------------------------------------------- */

    // TODO: DIMAP 1 probably handle mosaics? Like DIMAP 2?

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

    GDALDataset* poImageDS =
        static_cast<GDALDataset *>( GDALOpen( osImageFilename, GA_ReadOnly ) );
    if( poImageDS == NULL )
    {
        return FALSE;
    }
    nRasterXSize = poImageDS->GetRasterXSize();
    nRasterYSize = poImageDS->GetRasterYSize();

/* -------------------------------------------------------------------- */
/*      Create and initialize the corresponding VRT dataset used to     */
/*      manage the tiled data access.                                   */
/* -------------------------------------------------------------------- */
    poVRTDS = new VRTDataset(nRasterXSize,nRasterYSize);

    // Don't try to write a VRT file.
    poVRTDS->SetWritable(FALSE);

    GDALDataset *poTileDS =
          new GDALProxyPoolDataset( osImageFilename, nRasterXSize, nRasterYSize,
                                    GA_ReadOnly, TRUE );

    for( int iBand = 0; iBand < poImageDS->GetRasterCount(); iBand++ )
    {
        poVRTDS->AddBand(
            poImageDS->GetRasterBand(iBand+1)->GetRasterDataType(), NULL );

        reinterpret_cast<GDALProxyPoolDataset *>( poTileDS )->
            AddSrcBandDescription(
                poImageDS->GetRasterBand(iBand+1)->GetRasterDataType(),
                nRasterXSize, 1 );

        GDALRasterBand *poSrcBand = poTileDS->GetRasterBand(iBand+1);

        VRTSourcedRasterBand *poVRTBand =
            reinterpret_cast<VRTSourcedRasterBand *>(
                poVRTDS->GetRasterBand(iBand+1) );

        poVRTBand->AddSimpleSource( poSrcBand,
                                    0, 0,
                                    nRasterXSize, nRasterYSize,
                                    0, 0,
                                    nRasterXSize, nRasterYSize );
    }

    poTileDS->Dereference();

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 1; iBand <= poVRTDS->GetRasterCount(); iBand++ )
    {
        SetBand(
            iBand,
            new DIMAPRasterBand(
                this,
                iBand,
                static_cast<VRTSourcedRasterBand*>(
                    poVRTDS->GetRasterBand(iBand)) ) );
    }

/* -------------------------------------------------------------------- */
/*      Try to collect simple insertion point.                          */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psGeoLoc =
        CPLGetXMLNode( psDoc, "Geoposition.Geoposition_Insert" );

    if( psGeoLoc != NULL )
    {
        bHaveGeoTransform = TRUE;
        adfGeoTransform[0] = CPLAtof(CPLGetXMLValue(psGeoLoc, "ULXMAP", "0"));
        adfGeoTransform[1] = CPLAtof(CPLGetXMLValue(psGeoLoc, "XDIM", "0"));
        adfGeoTransform[2] = 0.0;
        adfGeoTransform[3] = CPLAtof(CPLGetXMLValue(psGeoLoc, "ULYMAP", "0"));
        adfGeoTransform[4] = 0.0;
        adfGeoTransform[5] = -CPLAtof(CPLGetXMLValue(psGeoLoc, "YDIM", "0"));
    }
    else
    {
        // Try to get geotransform from underlying raster.
        if( poImageDS->GetGeoTransform(adfGeoTransform) == CE_None )
            bHaveGeoTransform = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Collect GCPs.                                                   */
/* -------------------------------------------------------------------- */
    psGeoLoc = CPLGetXMLNode( psDoc, "Geoposition.Geoposition_Points" );

    if( psGeoLoc != NULL )
    {
        // Count gcps.
        nGCPCount = 0;
        for( CPLXMLNode *psNode = psGeoLoc->psChild;
             psNode != NULL;
             psNode = psNode->psNext )
        {
            if( EQUAL(psNode->pszValue,"Tie_Point") )
                nGCPCount++ ;
        }

        pasGCPList = static_cast<GDAL_GCP *>(
            CPLCalloc(sizeof(GDAL_GCP),nGCPCount) );

        nGCPCount = 0;

        for( CPLXMLNode *psNode = psGeoLoc->psChild;
             psNode != NULL;
             psNode = psNode->psNext )
        {
            GDAL_GCP *psGCP = pasGCPList + nGCPCount;

            if( !EQUAL(psNode->pszValue,"Tie_Point") )
                continue;

            nGCPCount++;

            char szID[32] = {};
            snprintf( szID, sizeof(szID), "%d", nGCPCount );
            psGCP->pszId = CPLStrdup( szID );
            psGCP->pszInfo = CPLStrdup("");
            psGCP->dfGCPPixel =
                CPLAtof(CPLGetXMLValue(psNode, "TIE_POINT_DATA_X", "0")) - 0.5;
            psGCP->dfGCPLine =
                CPLAtof(CPLGetXMLValue(psNode, "TIE_POINT_DATA_Y", "0")) - 0.5;
            psGCP->dfGCPX =
                CPLAtof(CPLGetXMLValue(psNode, "TIE_POINT_CRS_X", ""));
            psGCP->dfGCPY =
                CPLAtof(CPLGetXMLValue(psNode, "TIE_POINT_CRS_Y", ""));
            psGCP->dfGCPZ =
                CPLAtof(CPLGetXMLValue(psNode, "TIE_POINT_CRS_Z", ""));
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
        if( poImageDS->GetProjectionRef() )
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
    if( psImageInterpretationNode != NULL )
    {
        CPLXMLNode *psSpectralBandInfoNode = psImageInterpretationNode->psChild;
        while( psSpectralBandInfoNode != NULL )
        {
            if( psSpectralBandInfoNode->eType == CXT_Element &&
                EQUAL(psSpectralBandInfoNode->pszValue, "Spectral_Band_Info") )
            {
                CPLXMLNode *psTag = psSpectralBandInfoNode->psChild;
                int nBandIndex = 0;
                while( psTag != NULL )
                {
                    if( psTag->eType == CXT_Element && psTag->psChild != NULL &&
                        psTag->psChild->eType ==
                        CXT_Text && psTag->pszValue != NULL)
                    {
                        if( EQUAL(psTag->pszValue, "BAND_INDEX") )
                        {
                            nBandIndex = atoi(psTag->psChild->pszValue);
                            if( nBandIndex <= 0 ||
                                nBandIndex > poImageDS->GetRasterCount() )
                            {
                                CPLError(
                                    CE_Warning, CPLE_AppDefined,
                                    "Bad BAND_INDEX value : %s",
                                    psTag->psChild->pszValue);
                                nBandIndex = 0;
                            }
                        }
                        else if( nBandIndex >= 1 )
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

    GDALClose(poImageDS);

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    SetDescription( osMDFilename );
    TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    oOvManager.Initialize( this, osMDFilename );

    // CID 163546 - poTileDS dereferenced above.
    // coverity[leaked_storage]
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

    CPLXMLNode *psImageAttributes =
        CPLGetXMLNode( psDoc, "Raster_Data.Raster_Dimensions" );
    if( psImageAttributes == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to find <Raster_Dimensions> in document." );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Get overall image information.                                  */
/* -------------------------------------------------------------------- */

  /*
      <Raster_Dimensions>
         <NROWS>30</NROWS>
         <NCOLS>20</NCOLS>
         <NBANDS>4</NBANDS>
         <Tile_Set>
            <NTILES>2</NTILES>
            <Regular_Tiling>
               <NTILES_SIZE nrows="20" ncols="20"/>
               <NTILES_COUNT ntiles_R="2" ntiles_C="1"/>
               <OVERLAP_ROW>0</OVERLAP_ROW>
               <OVERLAP_COL>0</OVERLAP_COL>
            </Regular_Tiling>
         </Tile_Set>
      </Raster_Dimensions>
    */

    const int l_nBands =
        atoi(CPLGetXMLValue( psImageAttributes, "NBANDS", "-1" ));
    nRasterXSize =
        atoi(CPLGetXMLValue( psImageAttributes, "NCOLS", "-1" ));
    nRasterYSize =
        atoi(CPLGetXMLValue( psImageAttributes, "NROWS", "-1" ));
    int nTileWidth = atoi( CPLGetXMLValue( psImageAttributes,
                           "Tile_Set.Regular_Tiling.NTILES_SIZE.ncols", "-1" ));
    int nTileHeight = atoi( CPLGetXMLValue( psImageAttributes,
                            "Tile_Set.Regular_Tiling.NTILES_SIZE.nrows", "-1" ));
    int nOverlapRow = atoi( CPLGetXMLValue( psImageAttributes,
                            "Tile_Set.Regular_Tiling.OVERLAP_ROW", "-1" ));
    int nOverlapCol = atoi( CPLGetXMLValue( psImageAttributes,
                            "Tile_Set.Regular_Tiling.OVERLAP_COL", "-1" ));
    const int nBits = atoi(
        CPLGetXMLValue( psDoc, "Raster_Data.Raster_Encoding.NBITS", "-1") );
    CPLString osDataFormat =
        CPLGetXMLValue( psDoc, "Raster_Data.Data_Access.DATA_FILE_FORMAT", "" );
    if( osDataFormat == "image/jp2" )
    {
        SetMetadataItem( "COMPRESSION", "JPEG2000", "IMAGE_STRUCTURE" );
    }

/* -------------------------------------------------------------------- */
/*      Get the name of the underlying file.                            */
/* -------------------------------------------------------------------- */

    CPLXMLNode *psDataFiles =
        CPLGetXMLNode(psDoc, "Raster_Data.Data_Access.Data_Files" );
    /*  <Data_Files>
            <Data_File tile_R="1" tile_C="1">
               <DATA_FILE_PATH href="IMG_foo_R1C1.TIF"/>
            </Data_File>
            <Data_File tile_R="2" tile_C="1">
               <DATA_FILE_PATH href="IMG_foo_R2C1.TIF"/>
            </Data_File>
         </Data_Files>
    */
    std::map< std::pair<int,int>, CPLString > oMapRowColumnToName;
    if( psDataFiles )
    {
        int nRows = 1;
        int nCols = 1;
        CPLString osPath = CPLGetPath( osDIMAPFilename );
        for( CPLXMLNode* psDataFile = psDataFiles->psChild;
                         psDataFile; psDataFile = psDataFile->psNext )
        {
            if( psDataFile->eType == CXT_Element &&
                strcmp( psDataFile->pszValue, "Data_File") == 0 )
            {
                const char* pszR = CPLGetXMLValue( psDataFile, "tile_R", NULL );
                const char* pszC = CPLGetXMLValue( psDataFile, "tile_C", NULL );
                const char* pszHref =
                    CPLGetXMLValue(psDataFile, "DATA_FILE_PATH.href", NULL );
                if( pszR && pszC && pszHref )
                {
                    int nRow = atoi(pszR);
                    int nCol = atoi(pszC);
                    if( nRow == 1 && nCol == 1 )
                        osImageDSFilename =
                            CPLFormCIFilename( osPath, pszHref, NULL );
                    if( nRow > nRows ) nRows = nRow;
                    if( nCol > nCols ) nCols = nCol;
                    oMapRowColumnToName[ std::pair<int,int>(nRow, nCol) ] =
                          CPLFormCIFilename( osPath, pszHref, NULL );
                }
            }
        }
        if( nOverlapRow > 0 || nOverlapCol > 0 )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Overlap between tiles is not handled currently. "
                     "Only taking into account top left tile");
            oMapRowColumnToName.clear();
            oMapRowColumnToName[ std::pair<int,int>(1,1) ] = osImageDSFilename;
        }
    }
    else
    {
        oMapRowColumnToName[ std::pair<int,int>(1,1) ] = osImageDSFilename;
    }

    if( osImageDSFilename.empty() )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to find <DATA_FILE_PATH> in document." );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Try and open the file.                                          */
/* -------------------------------------------------------------------- */
    GDALDataset* poImageDS = static_cast<GDALDataset *>(
        GDALOpen( osImageDSFilename, GA_ReadOnly ) );
    if( poImageDS == NULL )
    {
        return FALSE;
    }
    if( poImageDS->GetRasterCount() != l_nBands )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Inconsistent band count");
        GDALClose(poImageDS);
        return FALSE;
    }

    if( oMapRowColumnToName.size() == 1 )
    {
        nTileWidth = poImageDS->GetRasterXSize();
        nTileHeight = poImageDS->GetRasterYSize();
    }

/* -------------------------------------------------------------------- */
/*      Create and initialize the corresponding VRT dataset used to     */
/*      manage the tiled data access.                                   */
/* -------------------------------------------------------------------- */
    poVRTDS = new VRTDataset(nRasterXSize, nRasterYSize);

    // Don't try to write a VRT file.
    poVRTDS->SetWritable(FALSE);

    std::map< std::pair<int,int>, GDALDataset* > oMapRowColumnToProxyPoolDataset;
    std::map< std::pair<int,int>, CPLString>::iterator oIterRCN =
        oMapRowColumnToName.begin();
    for( ; oIterRCN != oMapRowColumnToName.end(); ++oIterRCN )
    {
        const int nRow = oIterRCN->first.first;
        const int nCol = oIterRCN->first.second;
        if( (nRow - 1) * nTileHeight < nRasterYSize &&
            (nCol - 1) * nTileWidth < nRasterXSize )
        {
            int nHeight = nTileHeight;
            if( nRow * nTileHeight > nRasterYSize )
            {
                nHeight = nRasterYSize - (nRow - 1) * nTileHeight;
            }
            int nWidth = nTileWidth;
            if( nCol * nTileWidth > nRasterXSize )
            {
                nWidth = nRasterXSize - (nCol - 1) * nTileWidth;
            }
            GDALProxyPoolDataset* poPPDs = new GDALProxyPoolDataset(
                oIterRCN->second, nWidth, nHeight, GA_ReadOnly, TRUE );
            oMapRowColumnToProxyPoolDataset[ oIterRCN->first ] = poPPDs;

            for( int iBand = 0; iBand < poImageDS->GetRasterCount(); iBand++ )
            {
                poPPDs->AddSrcBandDescription(
                    poImageDS->GetRasterBand(iBand+1)->GetRasterDataType(),
                    nRasterXSize, 1 );
            }
        }
    }

    for( int iBand = 0; iBand < poImageDS->GetRasterCount(); iBand++ )
    {
        poVRTDS->AddBand(
            poImageDS->GetRasterBand(iBand+1)->GetRasterDataType(), NULL );

        VRTSourcedRasterBand *poVRTBand =
            reinterpret_cast<VRTSourcedRasterBand *>(
                poVRTDS->GetRasterBand(iBand+1) );
        if( nBits > 0 && nBits != 8 && nBits != 16 )
        {
            poVRTBand->SetMetadataItem(
                "NBITS", CPLSPrintf("%d", nBits), "IMAGE_STRUCTURE" );
        }

        std::map< std::pair<int,int>, GDALDataset*>::iterator oIterRCP =
            oMapRowColumnToProxyPoolDataset.begin();
        for( ; oIterRCP != oMapRowColumnToProxyPoolDataset.end(); ++oIterRCP )
        {
            GDALRasterBand *poSrcBand =
                oIterRCP->second->GetRasterBand(iBand+1);

            const int nRow = oIterRCP->first.first;
            const int nCol = oIterRCP->first.second;
            int nHeight = nTileHeight;
            if( nRow * nTileHeight > nRasterYSize )
            {
                nHeight = nRasterYSize - (nRow - 1) * nTileHeight;
            }
            int nWidth = nTileWidth;
            if( nCol * nTileWidth > nRasterXSize )
            {
                nWidth = nRasterXSize - (nCol - 1) * nTileWidth;
            }

            poVRTBand->AddSimpleSource( poSrcBand,
                                        0, 0,
                                        nWidth, nHeight,
                                        (nCol - 1) * nTileWidth,
                                        (nRow - 1) * nTileHeight,
                                        nWidth, nHeight );
        }
    }

    std::map< std::pair<int,int>, GDALDataset*>::iterator oIterRCP =
        oMapRowColumnToProxyPoolDataset.begin();
    for( ; oIterRCP != oMapRowColumnToProxyPoolDataset.end(); ++oIterRCP )
    {
        oIterRCP->second->Dereference();
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 1; iBand <= poVRTDS->GetRasterCount(); iBand++ )
    {
        GDALRasterBand* poBand = new DIMAPRasterBand(
            this, iBand,
            static_cast<VRTSourcedRasterBand*>(poVRTDS->GetRasterBand(iBand)) );
        if( nBits > 0 && nBits != 8 && nBits != 16 )
        {
            poBand->SetMetadataItem(
                "NBITS", CPLSPrintf("%d", nBits), "IMAGE_STRUCTURE" );
        }
        SetBand(iBand, poBand);
    }

/* -------------------------------------------------------------------- */
/*      Try to collect simple insertion point.                          */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psGeoLoc =
        CPLGetXMLNode( psDoc, "Geoposition.Geoposition_Insert" );

    if( psGeoLoc != NULL )
    {
        bHaveGeoTransform = TRUE;
        adfGeoTransform[0] = CPLAtof(CPLGetXMLValue(psGeoLoc, "ULXMAP", "0"));
        adfGeoTransform[1] = CPLAtof(CPLGetXMLValue(psGeoLoc, "XDIM", "0"));
        adfGeoTransform[2] = 0.0;
        adfGeoTransform[3] = CPLAtof(CPLGetXMLValue(psGeoLoc, "ULYMAP", "0"));
        adfGeoTransform[4] = 0.0;
        adfGeoTransform[5] = -CPLAtof(CPLGetXMLValue(psGeoLoc, "YDIM", "0"));
    }
    else
    {
        // Try to get geotransform from underlying raster,
        // but make sure it is a real geotransform.
        if( poImageDS->GetGeoTransform(adfGeoTransform) == CE_None &&
            !(adfGeoTransform[0] == 0.5 && adfGeoTransform[3] == 1.5 &&
              adfGeoTransform[1] == 1.0 && adfGeoTransform[5] == -1.0) )
        {
            bHaveGeoTransform = TRUE;
        }
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
        if( poImageDS->GetProjectionRef() )
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
        "Catalog.Full_Strip.Notations.Cloud_And_Quality_Notation."
        "Data_Strip_Notation", "CLOUDCOVER_",
        "Acquisition_Configuration.Platform_Configuration."
        "Ephemeris_Configuration", "EPHEMERIS_",
        NULL, NULL
    };

    if( psProductStrip != NULL )
        SetMetadataFromXML(psProductStrip, apszMetadataTranslationStrip);

    if( !osRPCFilename.empty() )
    {
        GDALMDReaderPleiades* poReader =
            GDALMDReaderPleiades::CreateReaderForRPC(osRPCFilename);
        char** papszRPC = poReader->LoadRPCXmlFile();
        delete poReader;
        if( papszRPC )
            SetMetadata(papszRPC, "RPC");
        CSLDestroy(papszRPC);
    }

/* -------------------------------------------------------------------- */
/*      Set Band metadata from the <Band_Radiance> and                  */
/*                                <Band_Spectral_Range> content         */
/* -------------------------------------------------------------------- */

    CPLXMLNode *psImageInterpretationNode =
        CPLGetXMLNode(
            psDoc,
            "Radiometric_Data.Radiometric_Calibration.Instrument_Calibration."
            "Band_Measurement_List" );
    if( psImageInterpretationNode != NULL )
    {
        CPLXMLNode *psSpectralBandInfoNode = psImageInterpretationNode->psChild;
        while( psSpectralBandInfoNode != NULL )
        {
            if( psSpectralBandInfoNode->eType == CXT_Element &&
                (EQUAL(psSpectralBandInfoNode->pszValue,
                       "Band_Radiance") ||
                 EQUAL(psSpectralBandInfoNode->pszValue,
                       "Band_Spectral_Range") ||
                 EQUAL(psSpectralBandInfoNode->pszValue,
                       "Band_Solar_Irradiance")) )
            {
                CPLString osName;

                if( EQUAL(psSpectralBandInfoNode->pszValue, "Band_Radiance") )
                    osName = "RADIANCE_";
                else if( EQUAL(psSpectralBandInfoNode->pszValue,
                               "Band_Spectral_Range"))
                    osName = "SPECTRAL_RANGE_";
                else if( EQUAL(psSpectralBandInfoNode->pszValue,
                               "Band_Solar_Irradiance") )
                    osName = "SOLAR_IRRADIANCE_";

                CPLXMLNode *psTag = psSpectralBandInfoNode->psChild;
                int nBandIndex = 0;
                while( psTag != NULL )
                {
                    if( psTag->eType == CXT_Element && psTag->psChild != NULL &&
                        psTag->psChild->eType == CXT_Text &&
                        psTag->pszValue != NULL )
                    {
                        if( EQUAL(psTag->pszValue, "BAND_ID") )
                        {
                            // BAND_ID is: B0, B1, .... P
                            if( !EQUAL(psTag->psChild->pszValue, "P") )
                            {
                                if( strlen(psTag->psChild->pszValue) < 2)
                                {
                                    // Should not happen.
                                    CPLError(
                                        CE_Warning, CPLE_AppDefined,
                                        "Bad BAND_INDEX value : %s",
                                        psTag->psChild->pszValue);
                                    nBandIndex = 0;
                                }
                                else
                                {
                                    nBandIndex =
                                        atoi(&psTag->psChild->pszValue[1]) + 1;
                                    if( nBandIndex <= 0 ||
                                    nBandIndex > poImageDS->GetRasterCount() )
                                    {
                                        CPLError(
                                            CE_Warning, CPLE_AppDefined,
                                            "Bad BAND_INDEX value : %s",
                                            psTag->psChild->pszValue);
                                        nBandIndex = 0;
                                    }
                                }
                            }
                        }
                        else if( nBandIndex >= 1 )
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

    GDALClose(poImageDS);

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

void DIMAPDataset::SetMetadataFromXML(
    CPLXMLNode *psProductIn, const char * const apszMetadataTranslation[] )
{
    CPLXMLNode *psDoc = CPLGetXMLNode( psProductIn, "=Dimap_Document" );
    if( psDoc == NULL )
    {
        psDoc = CPLGetXMLNode( psProductIn, "=PHR_DIMAP_Document" );
    }

    for( int iTrItem = 0;
         apszMetadataTranslation[iTrItem] != NULL;
         iTrItem += 2 )
    {
        CPLXMLNode *psParent =
            CPLGetXMLNode( psDoc, apszMetadataTranslation[iTrItem] );

        if( psParent == NULL )
            continue;

        // Hackey logic to support directly access a name/value entry
        // or a parent element with many name/values.

        CPLXMLNode *psTarget = NULL;
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

                if( psTarget->psChild->eType == CXT_Text )
                {
                    osName += psTarget->pszValue;
                    SetMetadataItem( osName, psTarget->psChild->pszValue );
                }
                else if( psTarget->psChild->eType == CXT_Attribute )
                {
                    // find the tag value, at the end of the attributes.
                    for( CPLXMLNode *psNode = psTarget->psChild;
                         psNode != NULL;
                         psNode = psNode->psNext )
                    {
                        if( psNode->eType == CXT_Attribute )
                            continue;
                        else if( psNode->eType == CXT_Text )
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
