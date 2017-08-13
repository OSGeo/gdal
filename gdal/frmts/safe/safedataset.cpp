/******************************************************************************
 *
 * Project:  Sentinel SAFE products
 * Purpose:  Sentinel Products (manifest.safe) driver
 * Author:   Delfim Rego, delfimrego@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2015, Delfim Rego <delfimrego@gmail.com>
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
#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "ogr_spatialref.h"
#include <set>
#include <map>

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                               SAFEDataset                            */
/* ==================================================================== */
/************************************************************************/

class SAFEDataset : public GDALPamDataset
{
    CPLXMLNode *psManifest;

    int           nGCPCount;
    GDAL_GCP     *pasGCPList;
    char         *pszGCPProjection;
    char        **papszSubDatasets;
    char         *pszProjection;
    double        adfGeoTransform[6];
    bool          bHaveGeoTransform;

    char        **papszExtraFiles;

  protected:
    virtual int         CloseDependentDatasets() override;

    static CPLXMLNode * GetMetaDataObject(CPLXMLNode *, const char *);

    static CPLXMLNode * GetDataObject(CPLXMLNode *, const char *);
    static CPLXMLNode * GetDataObject(CPLXMLNode *, CPLXMLNode *, const char *);

    static void AddSubDataset(SAFEDataset *poDS, int iDSNum, CPLString osName, CPLString osDesc);

  public:
            SAFEDataset();
    virtual ~SAFEDataset();

    virtual int    GetGCPCount() override;
    virtual const char *GetGCPProjection() override;
    virtual const GDAL_GCP *GetGCPs() override;

    virtual const char *GetProjectionRef(void) override;
    virtual CPLErr GetGeoTransform( double * ) override;

#ifdef notdef
    virtual char      **GetMetadataDomainList();
    virtual char **GetMetadata( const char * pszDomain = "" );
#endif
    virtual char **GetFileList(void) override;

    static GDALDataset *Open( GDALOpenInfo * );
    static int Identify( GDALOpenInfo * );

    CPLXMLNode *GetManifest() { return psManifest; }
};

/************************************************************************/
/* ==================================================================== */
/*                    SAFERasterBand                                    */
/* ==================================================================== */
/************************************************************************/

class SAFERasterBand : public GDALPamRasterBand
{
    GDALDataset     *poBandFile;

  public:
            SAFERasterBand( SAFEDataset *poDSIn,
                               GDALDataType eDataTypeIn,
                               const char *pszSwath,
                               const char *pszPol,
                               GDALDataset *poBandFile );
    virtual     ~SAFERasterBand();

    virtual CPLErr IReadBlock( int, int, void * ) override;

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/*                            SAFERasterBand                            */
/************************************************************************/

SAFERasterBand::SAFERasterBand( SAFEDataset *poDSIn,
                                GDALDataType eDataTypeIn,
                                const char *pszSwath,
                                const char *pszPolarisation,
                                GDALDataset *poBandFileIn ) :
    poBandFile(poBandFileIn)
{
    poDS = poDSIn;

    GDALRasterBand *poSrcBand = poBandFile->GetRasterBand( 1 );

    poSrcBand->GetBlockSize( &nBlockXSize, &nBlockYSize );

    eDataType = eDataTypeIn;

    if( *pszSwath != '\0' ) {
        SetMetadataItem( "SWATH", pszSwath );
    }
    if( *pszPolarisation != '\0' ) {
        SetMetadataItem( "POLARISATION", pszPolarisation );
    }
}

/************************************************************************/
/*                            RSRasterBand()                            */
/************************************************************************/

SAFERasterBand::~SAFERasterBand()

{
    if( poBandFile != NULL )
        GDALClose( reinterpret_cast<GDALRasterBandH>( poBandFile ) );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr SAFERasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
/* -------------------------------------------------------------------- */
/*      If the last strip is partial, we need to avoid                  */
/*      over-requesting.  We also need to initialize the extra part     */
/*      of the block to zero.                                           */
/* -------------------------------------------------------------------- */
    int nRequestYSize;
    if( (nBlockYOff + 1) * nBlockYSize > nRasterYSize )
    {
        nRequestYSize = nRasterYSize - nBlockYOff * nBlockYSize;
        memset( pImage, 0, (GDALGetDataTypeSize( eDataType ) / 8) *
            nBlockXSize * nBlockYSize );
    }
    else
    {
        nRequestYSize = nBlockYSize;
    }

/*-------------------------------------------------------------------- */
/*      If the input imagery is tiled, also need to avoid over-        */
/*      requesting in the X-direction.                                 */
/* ------------------------------------------------------------------- */
    int nRequestXSize;
    if( (nBlockXOff + 1) * nBlockXSize > nRasterXSize )
    {
        nRequestXSize = nRasterXSize - nBlockXOff * nBlockXSize;
        memset( pImage, 0, (GDALGetDataTypeSize( eDataType ) / 8) *
            nBlockXSize * nBlockYSize );
    }
    else
    {
        nRequestXSize = nBlockXSize;
    }
    if( eDataType == GDT_CInt16 && poBandFile->GetRasterCount() == 2 )
        return
            poBandFile->RasterIO( GF_Read,
                                  nBlockXOff * nBlockXSize,
                                  nBlockYOff * nBlockYSize,
                                  nRequestXSize, nRequestYSize,
                                  pImage, nRequestXSize, nRequestYSize,
                                  GDT_Int16,
                                  2, NULL, 4, nBlockXSize * 4, 2, NULL );

/* -------------------------------------------------------------------- */
/*      File has one sample marked as sample format void, a 32bits.     */
/* -------------------------------------------------------------------- */
    else if( eDataType == GDT_CInt16 && poBandFile->GetRasterCount() == 1 )
    {
        CPLErr eErr
            = poBandFile->RasterIO( GF_Read,
                                  nBlockXOff * nBlockXSize,
                                  nBlockYOff * nBlockYSize,
                                  nRequestXSize, nRequestYSize,
                                  pImage, nRequestXSize, nRequestYSize,
                                  GDT_UInt32,
                                  1, NULL, 4, nBlockXSize * 4, 0, NULL );

#ifdef CPL_LSB
        /* First, undo the 32bit swap. */
        GDALSwapWords( pImage, 4, nBlockXSize * nBlockYSize, 4 );

        /* Then apply 16 bit swap. */
        GDALSwapWords( pImage, 2, nBlockXSize * nBlockYSize * 2, 2 );
#endif

        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      The 16bit case is straight forward.  The underlying file        */
/*      looks like a 16bit unsigned data too.                           */
/* -------------------------------------------------------------------- */
    else if( eDataType == GDT_UInt16 )
        return
            poBandFile->RasterIO( GF_Read,
                                  nBlockXOff * nBlockXSize,
                                  nBlockYOff * nBlockYSize,
                                  nRequestXSize, nRequestYSize,
                                  pImage, nRequestXSize, nRequestYSize,
                                  GDT_UInt16,
                                  1, NULL, 2, nBlockXSize * 2, 0, NULL );
    else if ( eDataType == GDT_Byte )
        return
            poBandFile->RasterIO( GF_Read,
                                  nBlockXOff * nBlockXSize,
                                  nBlockYOff * nBlockYSize,
                                  nRequestXSize, nRequestYSize,
                                  pImage, nRequestXSize, nRequestYSize,
                                  GDT_Byte,
                                  1, NULL, 1, nBlockXSize, 0, NULL );

    CPLAssert( false );
    return CE_Failure;
}

/************************************************************************/
/* ==================================================================== */
/*                              SAFEDataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             SAFEDataset()                            */
/************************************************************************/

SAFEDataset::SAFEDataset() :
    psManifest(NULL),
    nGCPCount(0),
    pasGCPList(NULL),
    pszGCPProjection(CPLStrdup("")),
    papszSubDatasets(NULL),
    pszProjection(CPLStrdup("")),
    bHaveGeoTransform(false),
    papszExtraFiles(NULL)
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~SAFEDataset()                            */
/************************************************************************/

SAFEDataset::~SAFEDataset()

{
    FlushCache();

    CPLDestroyXMLNode( psManifest );
    CPLFree( pszProjection );

    CPLFree( pszGCPProjection );
    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

    CloseDependentDatasets();

    CSLDestroy( papszSubDatasets );
    CSLDestroy( papszExtraFiles );
}

/************************************************************************/
/*                      CloseDependentDatasets()                        */
/************************************************************************/

int SAFEDataset::CloseDependentDatasets()
{
    int bHasDroppedRef = GDALPamDataset::CloseDependentDatasets();

    if (nBands != 0)
        bHasDroppedRef = TRUE;

    for( int iBand = 0; iBand < nBands; iBand++ )
    {
       delete papoBands[iBand];
    }
    nBands = 0;

    return bHasDroppedRef;
}

/************************************************************************/
/*                      GetMetaDataObject()                             */
/************************************************************************/

CPLXMLNode * SAFEDataset::GetMetaDataObject(
    CPLXMLNode *psMetaDataObjects, const char *metadataObjectId)
{
/* -------------------------------------------------------------------- */
/*      Look for DataObject Element by ID.                              */
/* -------------------------------------------------------------------- */
    for( CPLXMLNode *psMDO = psMetaDataObjects->psChild;
         psMDO != NULL;
         psMDO = psMDO->psNext )
    {
        if( psMDO->eType != CXT_Element
            || !(EQUAL(psMDO->pszValue,"metadataObject")) ) {
            continue;
        }

        const char *pszElementID = CPLGetXMLValue( psMDO, "ID", "" );

        if (EQUAL(pszElementID, metadataObjectId)) {
            return psMDO;
        }
    }

    CPLError( CE_Warning, CPLE_AppDefined,
              "MetadataObject not found with ID=%s",
              metadataObjectId);

    return NULL;
}

/************************************************************************/
/*                      GetDataObject()                                 */
/************************************************************************/

CPLXMLNode * SAFEDataset::GetDataObject(
    CPLXMLNode *psDataObjects, const char *dataObjectId)
{
/* -------------------------------------------------------------------- */
/*      Look for DataObject Element by ID.                              */
/* -------------------------------------------------------------------- */
    for( CPLXMLNode *psDO = psDataObjects->psChild;
         psDO != NULL;
         psDO = psDO->psNext )
    {
        if( psDO->eType != CXT_Element
            || !(EQUAL(psDO->pszValue,"dataObject")) ) {
            continue;
        }

        const char *pszElementID = CPLGetXMLValue( psDO, "ID", "" );

        if (EQUAL(pszElementID, dataObjectId)) {
            return psDO;
        }
    }

    CPLError( CE_Warning, CPLE_AppDefined,
              "DataObject not found with ID=%s",
              dataObjectId);

    return NULL;
}

CPLXMLNode * SAFEDataset::GetDataObject(
    CPLXMLNode *psMetaDataObjects, CPLXMLNode *psDataObjects,
    const char *metadataObjectId)
{
/* -------------------------------------------------------------------- */
/*      Look for MetadataObject Element by ID.                          */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psMDO = SAFEDataset::GetMetaDataObject(
            psMetaDataObjects, metadataObjectId);

    if (psMDO!=NULL) {
        const char *dataObjectId = CPLGetXMLValue(
            psMDO, "dataObjectPointer.dataObjectID", "" );
        if( *dataObjectId != '\0' ) {
            return SAFEDataset::GetDataObject(psDataObjects, dataObjectId);
        }
    }

    CPLError( CE_Warning, CPLE_AppDefined,
              "DataObject not found with MetaID=%s",
              metadataObjectId);

    return NULL;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **SAFEDataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();

    papszFileList = CSLInsertStrings( papszFileList, -1, papszExtraFiles );

    return papszFileList;
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int SAFEDataset::Identify( GDALOpenInfo *poOpenInfo )
{
    /* Check for the case where we're trying to read the calibrated data: */
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "SENTINEL1_CALIB:")) {
        return TRUE;
    }

    /* Check for the case where we're trying to read the subdatasets: */
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "SENTINEL1_DS:")) {
        return TRUE;
    }

    /* Check for directory access when there is a manifest.safe file in the
       directory. */
    if( poOpenInfo->bIsDirectory )
    {
        VSIStatBufL sStat;

        CPLString osMDFilename =
            CPLFormCIFilename( poOpenInfo->pszFilename, "manifest.safe", NULL );

        if( VSIStatL( osMDFilename, &sStat ) == 0 && VSI_ISREG(sStat.st_mode) )
        {
            GDALOpenInfo oOpenInfo( osMDFilename, GA_ReadOnly, NULL );
            return Identify(&oOpenInfo);
        }

        return FALSE;
    }

    /* otherwise, do our normal stuff */
    if( !EQUAL(CPLGetFilename(poOpenInfo->pszFilename), "manifest.safe") )
        return FALSE;

    if( poOpenInfo->nHeaderBytes < 100 )
        return FALSE;

    if( strstr((const char *) poOpenInfo->pabyHeader, "<xfdu:XFDU" ) == NULL)
        return FALSE;

    // This driver doesn't handle Sentinel-2 data
    if( strstr((const char *) poOpenInfo->pabyHeader, "sentinel-2" ) != NULL)
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *SAFEDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Is this a SENTINEL-1 manifest.safe definition?                  */
/* -------------------------------------------------------------------- */
    if ( !SAFEDataset::Identify( poOpenInfo ) ) {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*        Get subdataset information, if relevant                       */
/* -------------------------------------------------------------------- */
    CPLString osMDFilename;

    //Subdataset 1st level selection (ex: for swath selection)
    CPLString osSelectedSubDS1;
    //Subdataset 2nd level selection (ex: for polarisation selection)
    CPLString osSelectedSubDS2;

    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "SENTINEL1_DS:"))
    {
      osMDFilename = poOpenInfo->pszFilename + strlen("SENTINEL1_DS:");
      const char* pszSelection1 = strrchr(osMDFilename.c_str(), ':');
      if (pszSelection1 == NULL || pszSelection1 == osMDFilename.c_str() )
      {
          CPLError(CE_Failure, CPLE_AppDefined, "Invalid syntax for SENTINEL1_DS:");
          return NULL;
      }
      osMDFilename.resize( pszSelection1 - osMDFilename.c_str() );
      osSelectedSubDS1 = pszSelection1 + strlen(":");

      const char* pszSelection2 = strchr(osSelectedSubDS1.c_str(), '_');
      if (pszSelection2 != NULL && pszSelection2 != pszSelection1 )
      {
          osSelectedSubDS1.resize( pszSelection2 - osSelectedSubDS1.c_str() );
          osSelectedSubDS2 = pszSelection2 + strlen("_");
      }

      //update directory check:
      VSIStatBufL  sStat;
      if( VSIStatL( osMDFilename.c_str(), &sStat ) == 0 )
          poOpenInfo->bIsDirectory = VSI_ISDIR( sStat.st_mode );
    }
    else
    {
      osMDFilename = poOpenInfo->pszFilename;
    }

    if( poOpenInfo->bIsDirectory )
    {
        osMDFilename =
            CPLFormCIFilename( osMDFilename.c_str(), "manifest.safe", NULL );
    }

/* -------------------------------------------------------------------- */
/*      Ingest the manifest.safe file.                                  */
/* -------------------------------------------------------------------- */
    //TODO REMOVE CPLXMLNode *psImageAttributes, *psImageGenerationParameters;
    CPLXMLNode *psManifest = CPLParseXMLFile( osMDFilename );
    if( psManifest == NULL )
        return NULL;

    CPLString osPath(CPLGetPath( osMDFilename ));

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLDestroyXMLNode( psManifest );
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The SAFE driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Get contentUnit parent element.                                 */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psContentUnits = CPLGetXMLNode(
            psManifest,
            "=xfdu:XFDU.informationPackageMap.xfdu:contentUnit" );
    if( psContentUnits == NULL )
    {
        CPLDestroyXMLNode( psManifest );
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to find <xfdu:XFDU><informationPackageMap>"
                  "<xfdu:contentUnit> in manifest file." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Get Metadata Objects element.                                   */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psMetaDataObjects
        = CPLGetXMLNode( psManifest, "=xfdu:XFDU.metadataSection" );
    if( psMetaDataObjects == NULL )
    {
        CPLDestroyXMLNode( psManifest );
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to find <xfdu:XFDU><metadataSection>"
                  "in manifest file." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Get Data Objects element.                                       */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psDataObjects
        = CPLGetXMLNode( psManifest, "=xfdu:XFDU.dataObjectSection" );
    if( psDataObjects == NULL )
    {
        CPLDestroyXMLNode( psManifest );
        CPLError( CE_Failure, CPLE_OpenFailed,
                "Failed to find <xfdu:XFDU><dataObjectSection> in document." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    SAFEDataset *poDS = new SAFEDataset();

    poDS->psManifest = psManifest;

/* -------------------------------------------------------------------- */
/*      Look for "Measurement Data Unit" contentUnit elements.          */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psAnnotation = NULL;
    //Map with all measures aggregated by swath
    std::map<CPLString, std::set<CPLString> > oMapSwaths2Pols;

    for( CPLXMLNode *psContentUnit = psContentUnits->psChild;
         psContentUnit != NULL;
         psContentUnit = psContentUnit->psNext )
    {
        if( psContentUnit->eType != CXT_Element
            || !(EQUAL(psContentUnit->pszValue,"xfdu:contentUnit")) ) {
            continue;
        }

        const char *pszUnitType = CPLGetXMLValue( psContentUnit,
                "unitType", "" );

        const char *pszAnnotation  = NULL;
        const char *pszCalibration = NULL;
        const char *pszMeasurement = NULL;

        if ( EQUAL(pszUnitType, "Measurement Data Unit") ) {
            /* Get dmdID and dataObjectID */
            const char *pszDmdID = CPLGetXMLValue(psContentUnit, "dmdID", "");

            const char *pszDataObjectID = CPLGetXMLValue(
                psContentUnit,
                "dataObjectPointer.dataObjectID", "" );
            if( *pszDataObjectID == '\0' || *pszDmdID == '\0' ) {
                continue;
            }

            CPLXMLNode *psDataObject = SAFEDataset::GetDataObject(
                    psDataObjects, pszDataObjectID);

            const char *pszRepId = CPLGetXMLValue( psDataObject, "repID", "" );
            if ( !EQUAL(pszRepId, "s1Level1MeasurementSchema") ) {
                continue;
            }
            pszMeasurement = CPLGetXMLValue(
                    psDataObject, "byteStream.fileLocation.href", "");
            if( *pszMeasurement == '\0' ) {
                continue;
            }

            char** papszTokens = CSLTokenizeString2( pszDmdID, " ",
                CSLT_ALLOWEMPTYTOKENS | CSLT_STRIPLEADSPACES
                | CSLT_STRIPENDSPACES );

            for( int j = 0; j < CSLCount( papszTokens ); j++ ) {
                const char* pszId = papszTokens[j];
                if( *pszId == '\0' ) {
                    continue;
                }

                //Map the metadata ID to the object element
                CPLXMLNode *psDO = SAFEDataset::GetDataObject(
                        psMetaDataObjects, psDataObjects, pszId);

                if (psDO == NULL) {
                    continue;
                }

                //check object type
                pszRepId = CPLGetXMLValue( psDO, "repID", "" );

                if( EQUAL(pszRepId, "s1Level1ProductSchema") )
                {
                    /* Get annotation filename */
                    pszAnnotation = CPLGetXMLValue(
                            psDO, "byteStream.fileLocation.href", "");
                    if( *pszAnnotation == '\0' )
                    {
                        continue;
                    }
                }
                else if( EQUAL(pszRepId, "s1Level1CalibrationSchema") )
                {
                    pszCalibration = CPLGetXMLValue(
                            psDO, "byteStream.fileLocation.href", "");
                    if( *pszCalibration == '\0' ) {
                        continue;
                    }
                }
                else
                {
                    continue;
                }
            }

            CSLDestroy(papszTokens);

            if (pszAnnotation == NULL || pszCalibration == NULL ) {
                continue;
            }

            //open Annotation XML file
            CPLString osAnnotationFilePath = CPLFormFilename( osPath,
                                                       pszAnnotation, NULL );
            if( psAnnotation )
                CPLDestroyXMLNode(psAnnotation);
            psAnnotation = CPLParseXMLFile( osAnnotationFilePath );
            if( psAnnotation == NULL )
                continue;

/* -------------------------------------------------------------------- */
/*      Get overall image information.                                  */
/* -------------------------------------------------------------------- */
            poDS->nRasterXSize =
                atoi(CPLGetXMLValue( psAnnotation,
                    "=product.imageAnnotation.imageInformation.numberOfSamples",
                    "-1" ));
            poDS->nRasterYSize =
                atoi(CPLGetXMLValue( psAnnotation,
                    "=product.imageAnnotation.imageInformation.numberOfLines",
                    "-1" ));
            if (poDS->nRasterXSize <= 1 || poDS->nRasterYSize <= 1) {
                CPLError( CE_Failure, CPLE_OpenFailed,
                    "Non-sane raster dimensions provided in manifest.safe. "
                    "If this is a valid SENTINEL-1 scene, please contact your "
                    "data provider for a corrected dataset." );
                delete poDS;
                CPLDestroyXMLNode(psAnnotation);
                return NULL;
            }

            CPLString osProductType = CPLGetXMLValue(
                psAnnotation, "=product.adsHeader.productType", "UNK" );
            CPLString osMissionId = CPLGetXMLValue(
                psAnnotation, "=product.adsHeader.missionId", "UNK" );
            CPLString osPolarisation = CPLGetXMLValue(
                psAnnotation, "=product.adsHeader.polarisation", "UNK" );
            CPLString osMode = CPLGetXMLValue(
                psAnnotation, "=product.adsHeader.mode", "UNK" );
            CPLString osSwath = CPLGetXMLValue(
                psAnnotation, "=product.adsHeader.swath", "UNK" );

            oMapSwaths2Pols[osSwath].insert(osPolarisation);

            if (osSelectedSubDS1.empty()) {
              // If not subdataset was selected,
              // open the first one we can find.
              osSelectedSubDS1 = osSwath;
            }

            if (!EQUAL(osSelectedSubDS1.c_str(), osSwath.c_str())) {
              //do not mix swath, otherwise it does not work for SLC products
              continue;
            }

            if (!osSelectedSubDS2.empty()
              && (osSelectedSubDS2.find(osPolarisation)== std::string::npos)) {
              // Add only selected polarisations.
              continue;
            }

            poDS->SetMetadataItem("PRODUCT_TYPE", osProductType.c_str());
            poDS->SetMetadataItem("MISSION_ID", osMissionId.c_str());
            poDS->SetMetadataItem("MODE", osMode.c_str());
            poDS->SetMetadataItem("SWATH", osSwath.c_str());

/* -------------------------------------------------------------------- */
/*      Get dataType (so we can recognize complex data), and the        */
/*      bitsPerSample.                                                  */
/* -------------------------------------------------------------------- */

            const char *pszDataType = CPLGetXMLValue(
                psAnnotation,
                "=product.imageAnnotation.imageInformation.outputPixels",
                "" );

            GDALDataType eDataType;
            if( EQUAL(pszDataType,"16 bit Signed Integer") )
                eDataType = GDT_CInt16;
            else if( EQUAL(pszDataType,"16 bit Unsigned Integer") )
                eDataType = GDT_UInt16;
            else
            {
                delete poDS;
                CPLError( CE_Failure, CPLE_AppDefined,
                          "dataType=%s: not a supported configuration.",
                          pszDataType );
                CPLDestroyXMLNode(psAnnotation);
                return NULL;
            }

            /* Extract pixel spacing information */
            const char *pszPixelSpacing = CPLGetXMLValue(
                psAnnotation,
                "=product.imageAnnotation.imageInformation.rangePixelSpacing",
                "UNK" );
            poDS->SetMetadataItem( "PIXEL_SPACING", pszPixelSpacing );

            const char *pszLineSpacing = CPLGetXMLValue(
                psAnnotation,
                "=product.imageAnnotation.imageInformation.azimuthPixelSpacing",
                "UNK" );
            poDS->SetMetadataItem( "LINE_SPACING", pszLineSpacing );

/* -------------------------------------------------------------------- */
/*      Form full filename (path of manifest.safe + measurement file).  */
/* -------------------------------------------------------------------- */
            char *pszFullname =
                CPLStrdup(CPLFormFilename( osPath, pszMeasurement, NULL ));

/* -------------------------------------------------------------------- */
/*      Try and open the file.                                          */
/* -------------------------------------------------------------------- */
            GDALDataset *poBandFile = reinterpret_cast<GDALDataset *>(
                GDALOpen( pszFullname, GA_ReadOnly ) );
            if( poBandFile == NULL )
            {
                // NOP
            }
            else
            if (poBandFile->GetRasterCount() == 0)
            {
                GDALClose( (GDALRasterBandH) poBandFile );
             }
            else {
                poDS->papszExtraFiles = CSLAddString( poDS->papszExtraFiles,
                                                  osAnnotationFilePath );
                poDS->papszExtraFiles = CSLAddString( poDS->papszExtraFiles,
                                                  pszFullname );

/* -------------------------------------------------------------------- */
/*      Create the band.                                                */
/* -------------------------------------------------------------------- */
                SAFERasterBand *poBand
                    = new SAFERasterBand( poDS, eDataType,
                                          osSwath.c_str(),
                                          osPolarisation.c_str(),
                                          poBandFile );

                poDS->SetBand( poDS->GetRasterCount() + 1, poBand );
            }

            CPLFree( pszFullname );
        }
    }

    //loop through all Swath/pols to add subdatasets
    int iSubDS = 1;
    for (std::map<CPLString, std::set<CPLString> >::iterator iterSwath=oMapSwaths2Pols.begin();
         iterSwath!=oMapSwaths2Pols.end(); ++iterSwath)
    {
        CPLString osSubDS1 = iterSwath->first;
        CPLString osSubDS2;

        for (std::set<CPLString>::iterator iterPol=iterSwath->second.begin();
            iterPol!=iterSwath->second.end(); ++iterPol)
        {
            if (!osSubDS2.empty()) {
                osSubDS2 += "+";
            }
            osSubDS2 += *iterPol;

            //Create single band SubDataset
            SAFEDataset::AddSubDataset(poDS, iSubDS,
                CPLSPrintf("SENTINEL1_DS:%s:%s_%s",
                    osPath.c_str(),
                    osSubDS1.c_str(),
                    (*iterPol).c_str()),
                CPLSPrintf("Single band with %s swath and %s polarisation",
                    osSubDS1.c_str(),
                    (*iterPol).c_str())
            );
            iSubDS++;
        }

        if (iterSwath->second.size()>1) {
            //Create single band SubDataset with all polarisations
            SAFEDataset::AddSubDataset(poDS, iSubDS,
                CPLSPrintf("SENTINEL1_DS:%s:%s",
                    osPath.c_str(),
                    osSubDS1.c_str()),
                CPLSPrintf("%s swath with all polarisations as bands",
                    osSubDS1.c_str())
            );
            iSubDS++;
        }
    }

    if (poDS->GetRasterCount() == 0) {
        CPLError( CE_Failure, CPLE_OpenFailed, "Measurement bands not found." );
        delete poDS;
        if( psAnnotation )
            CPLDestroyXMLNode(psAnnotation);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Collect more metadata elements                                  */
/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- */
/*      Platform information                                            */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psPlatformAttrs = SAFEDataset::GetMetaDataObject(
        psMetaDataObjects, "platform");

    if (psPlatformAttrs != NULL) {
        const char *pszItem = CPLGetXMLValue(
                psPlatformAttrs,
                "metadataWrap.xmlData.safe:platform"
                ".safe:familyName", "" );
        poDS->SetMetadataItem( "SATELLITE_IDENTIFIER", pszItem );

        pszItem = CPLGetXMLValue(
                psPlatformAttrs,
                "metadataWrap.xmlData.safe:platform"
                ".safe:instrument.safe:familyName.abbreviation", "" );
        poDS->SetMetadataItem( "SENSOR_IDENTIFIER", pszItem );

        pszItem = CPLGetXMLValue(
                psPlatformAttrs,
                "metadataWrap.xmlData.safe:platform"
                ".safe:instrument.safe:extension"
                ".s1sarl1:instrumentMode.s1sarl1:mode", "UNK" );
        poDS->SetMetadataItem( "BEAM_MODE", pszItem );

        pszItem = CPLGetXMLValue(
                psPlatformAttrs,
                "metadataWrap.xmlData.safe:platform"
                ".safe:instrument.safe:extension"
                ".s1sarl1:instrumentMode.s1sarl1:swath", "UNK" );
        poDS->SetMetadataItem( "BEAM_SWATH", pszItem );
    }

/* -------------------------------------------------------------------- */
/*      Acquisition Period information                                  */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psAcquisitionAttrs = SAFEDataset::GetMetaDataObject(
        psMetaDataObjects, "acquisitionPeriod");

    if (psAcquisitionAttrs != NULL) {
            const char *pszItem = CPLGetXMLValue(
            psAcquisitionAttrs,
            "metadataWrap.xmlData.safe:acquisitionPeriod"
            ".safe:startTime", "UNK" );
        poDS->SetMetadataItem( "ACQUISITION_START_TIME", pszItem );
        pszItem = CPLGetXMLValue(
            psAcquisitionAttrs,
            "metadataWrap.xmlData.safe:acquisitionPeriod"
            ".safe:stopTime", "UNK" );
        poDS->SetMetadataItem( "ACQUISITION_STOP_TIME", pszItem );
    }

/* -------------------------------------------------------------------- */
/*      Processing information                                          */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psProcessingAttrs = SAFEDataset::GetMetaDataObject(
        psMetaDataObjects, "processing");

    if (psProcessingAttrs != NULL) {
        const char *pszItem = CPLGetXMLValue(
            psProcessingAttrs,
            "metadataWrap.xmlData.safe:processing.safe:facility.name", "UNK" );
        poDS->SetMetadataItem( "FACILITY_IDENTIFIER", pszItem );
    }

/* -------------------------------------------------------------------- */
/*      Measurement Orbit Reference information                         */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psOrbitAttrs = SAFEDataset::GetMetaDataObject(
        psMetaDataObjects, "measurementOrbitReference");

    if (psOrbitAttrs != NULL) {
        const char *pszItem = CPLGetXMLValue( psOrbitAttrs,
            "metadataWrap.xmlData.safe:orbitReference"
            ".safe:orbitNumber", "UNK" );
        poDS->SetMetadataItem( "ORBIT_NUMBER", pszItem );
        pszItem = CPLGetXMLValue( psOrbitAttrs,
            "metadataWrap.xmlData.safe:orbitReference"
            ".safe:extension.s1:orbitProperties.s1:pass", "UNK" );
        poDS->SetMetadataItem( "ORBIT_DIRECTION", pszItem );
    }

/* -------------------------------------------------------------------- */
/*      Collect Annotation Processing Information                       */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psProcessingInfo =
        CPLGetXMLNode( psAnnotation,
                       "=product.imageAnnotation.processingInformation" );

    if ( psProcessingInfo != NULL ) {
        OGRSpatialReference oLL, oPrj;

        const char *pszEllipsoidName = CPLGetXMLValue(
            psProcessingInfo, "ellipsoidName", "" );
        const double minor_axis = CPLAtof(CPLGetXMLValue(
            psProcessingInfo, "ellipsoidSemiMinorAxis", "0.0" ));
        const double major_axis = CPLAtof(CPLGetXMLValue(
            psProcessingInfo, "ellipsoidSemiMajorAxis", "0.0" ));

        if ( EQUAL(pszEllipsoidName, "") || ( minor_axis == 0.0 ) ||
             ( major_axis == 0.0 ) )
        {
            CPLError(CE_Warning,CPLE_AppDefined,"Warning- incomplete"
                     " ellipsoid information.  Using wgs-84 parameters.\n");
            oLL.SetWellKnownGeogCS( "WGS84" );
            oPrj.SetWellKnownGeogCS( "WGS84" );
        }
        else if ( EQUAL( pszEllipsoidName, "WGS84" ) ) {
            oLL.SetWellKnownGeogCS( "WGS84" );
            oPrj.SetWellKnownGeogCS( "WGS84" );
        }
        else {
            const double inv_flattening = major_axis/(major_axis - minor_axis);
            oLL.SetGeogCS( "","",pszEllipsoidName, major_axis,
                           inv_flattening);
            oPrj.SetGeogCS( "","",pszEllipsoidName, major_axis,
                            inv_flattening);
        }

        CPLFree( poDS->pszGCPProjection );
        poDS->pszGCPProjection = NULL;
        oLL.exportToWkt( &(poDS->pszGCPProjection) );
    }

/* -------------------------------------------------------------------- */
/*      Collect GCPs.                                                   */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psGeoGrid =
        CPLGetXMLNode( psAnnotation,
                       "=product.geolocationGrid.geolocationGridPointList" );

    if( psGeoGrid != NULL ) {
        /* count GCPs */
        poDS->nGCPCount = 0;

        for( CPLXMLNode *psNode = psGeoGrid->psChild; psNode != NULL;
             psNode = psNode->psNext )
        {
            if( EQUAL(psNode->pszValue,"geolocationGridPoint") )
                poDS->nGCPCount++ ;
        }

        poDS->pasGCPList = reinterpret_cast<GDAL_GCP *>(
            CPLCalloc( sizeof(GDAL_GCP), poDS->nGCPCount ) );

        poDS->nGCPCount = 0;

        for( CPLXMLNode *psNode = psGeoGrid->psChild; psNode != NULL;
             psNode = psNode->psNext )
        {
            GDAL_GCP *psGCP = poDS->pasGCPList + poDS->nGCPCount;

            if( !EQUAL(psNode->pszValue,"geolocationGridPoint") )
                continue;

            poDS->nGCPCount++ ;

            char szID[32];
            snprintf( szID, sizeof(szID), "%d", poDS->nGCPCount );
            psGCP->pszId = CPLStrdup( szID );
            psGCP->pszInfo = CPLStrdup("");
            psGCP->dfGCPPixel = CPLAtof(CPLGetXMLValue(psNode,"pixel","0"));
            psGCP->dfGCPLine = CPLAtof(CPLGetXMLValue(psNode,"line","0"));
            psGCP->dfGCPX = CPLAtof(CPLGetXMLValue(psNode,"longitude",""));
            psGCP->dfGCPY = CPLAtof(CPLGetXMLValue(psNode,"latitude",""));
            psGCP->dfGCPZ = CPLAtof(CPLGetXMLValue(psNode,"height",""));
        }
    }

    CPLDestroyXMLNode(psAnnotation);

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    const CPLString osDescription = osMDFilename;

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( osDescription );

    poDS->SetPhysicalFilename( osMDFilename );
    poDS->SetSubdatasetName( osDescription );

    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, ":::VIRTUAL:::" );

    return poDS;
}

/************************************************************************/
/*                            AddSubDataset()                           */
/************************************************************************/
void SAFEDataset::AddSubDataset(SAFEDataset *poDS, int iDSNum, CPLString osName, CPLString osDesc)
{
    //Create SubDataset
    poDS->GDALDataset::SetMetadataItem(
        CPLSPrintf("SUBDATASET_%d_NAME", iDSNum),
        osName.c_str(),
        "SUBDATASETS");

    poDS->GDALDataset::SetMetadataItem(
        CPLSPrintf("SUBDATASET_%d_DESC", iDSNum),
        osDesc.c_str(),
        "SUBDATASETS");
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int SAFEDataset::GetGCPCount()
{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *SAFEDataset::GetGCPProjection()
{
    return pszGCPProjection;
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *SAFEDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *SAFEDataset::GetProjectionRef()
{
    return pszProjection;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr SAFEDataset::GetGeoTransform( double * padfTransform )
{
    memcpy( padfTransform,  adfGeoTransform, sizeof(double) * 6 );

    if (bHaveGeoTransform)
        return CE_None;

    return CE_Failure;
}

#ifdef notdef
/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **SAFEDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "SUBDATASETS", NULL);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **SAFEDataset::GetMetadata( const char *pszDomain )
{
    if( pszDomain != NULL && STARTS_WITH_CI(pszDomain, "SUBDATASETS") &&
        papszSubDatasets != NULL)
        return papszSubDatasets;

    return GDALDataset::GetMetadata( pszDomain );
}
#endif

/************************************************************************/
/*                         GDALRegister_SAFE()                          */
/************************************************************************/

void GDALRegister_SAFE()
{
    if( GDALGetDriverByName( "SAFE" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "SAFE" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Sentinel-1 SAR SAFE Product" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_safe.html" );

    poDriver->pfnOpen = SAFEDataset::Open;
    poDriver->pfnIdentify = SAFEDataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
