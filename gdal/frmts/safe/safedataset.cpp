/******************************************************************************
 * $Id$
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

#include "gdal_pam.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_SAFE(void);
CPL_C_END

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
    virtual int         CloseDependentDatasets();

    static CPLXMLNode * GetMetaDataObject(CPLXMLNode *, const char *);
    
    static CPLXMLNode * GetDataObject(CPLXMLNode *, const char *);
    static CPLXMLNode * GetDataObject(CPLXMLNode *, CPLXMLNode *, const char *);
    
  public:
            SAFEDataset();
           ~SAFEDataset();
    
    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

    
    virtual const char *GetProjectionRef(void);
    virtual CPLErr GetGeoTransform( double * );

#ifdef notdef
    virtual char      **GetMetadataDomainList();
    virtual char **GetMetadata( const char * pszDomain = "" );
#endif
    virtual char **GetFileList(void);

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
                               const char *pszPole, 
                               GDALDataset *poBandFile );
    virtual     ~SAFERasterBand();
    
    virtual CPLErr IReadBlock( int, int, void * );

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/*                            SAFERasterBand                            */
/************************************************************************/

SAFERasterBand::SAFERasterBand( SAFEDataset *poDSIn,
                              GDALDataType eDataTypeIn,
                              const char *pszPolarisation, 
                              GDALDataset *poBandFileIn )

{
    GDALRasterBand *poSrcBand;

    poDS = poDSIn;
    poBandFile = poBandFileIn;

    poSrcBand = poBandFile->GetRasterBand( 1 );

    poSrcBand->GetBlockSize( &nBlockXSize, &nBlockYSize );
    
    eDataType = eDataTypeIn;

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
        GDALClose( (GDALRasterBandH) poBandFile );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr SAFERasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    int nRequestYSize;
    int nRequestXSize;

/* -------------------------------------------------------------------- */
/*      If the last strip is partial, we need to avoid                  */
/*      over-requesting.  We also need to initialize the extra part     */
/*      of the block to zero.                                           */
/* -------------------------------------------------------------------- */
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
        CPLErr eErr;

        eErr = 
            poBandFile->RasterIO( GF_Read, 
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
    else
    {
        CPLAssert( FALSE );
        return CE_Failure;
    }
}

/************************************************************************/
/* ==================================================================== */
/*                              SAFEDataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             SAFEDataset()                            */
/************************************************************************/

SAFEDataset::SAFEDataset()
{
    psManifest = NULL;

    nGCPCount = 0;
    pasGCPList = NULL;
    pszGCPProjection = CPLStrdup("");
    pszProjection = CPLStrdup("");
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    bHaveGeoTransform = FALSE;

    papszSubDatasets = NULL;
    papszExtraFiles = NULL;
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
        CPLXMLNode *psMetaDataObjects, const char *metadataObjectId) {
    
    
/* -------------------------------------------------------------------- */
/*      Look for DataObject Element by ID.                              */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psMDO;

    for( psMDO = psMetaDataObjects->psChild;
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
        CPLXMLNode *psDataObjects, const char *dataObjectId) {
    
    
/* -------------------------------------------------------------------- */
/*      Look for DataObject Element by ID.                              */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psDO;

    for( psDO = psDataObjects->psChild;
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
        const char *metadataObjectId) {
    
    
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
    if (EQUALN("SENTINEL_1_CALIB:",poOpenInfo->pszFilename,17)) {
        return 1;
    }

    /* Check for directory access when there is a manifest.safe file in the
       directory. */
    if( poOpenInfo->bIsDirectory )
    {
        VSIStatBufL sStat;

        CPLString osMDFilename = 
            CPLFormCIFilename( poOpenInfo->pszFilename, "manifest.safe", NULL );
        
        if( VSIStatL( osMDFilename, &sStat ) == 0 )
            return TRUE;
        else
            return FALSE;
    }

    /* otherwise, do our normal stuff */
    if( strlen(poOpenInfo->pszFilename) < 13
        || !EQUAL(poOpenInfo->pszFilename + strlen(poOpenInfo->pszFilename)-13,
                  "manifest.safe") )
        return 0;

    if( poOpenInfo->nHeaderBytes < 100 )
        return 0;

    if( strstr((const char *) poOpenInfo->pabyHeader, "<xfdu:XFDU" ) == NULL)
        return 0;

    return 1;

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
    const char *pszFilename = poOpenInfo->pszFilename;

    if( poOpenInfo->bIsDirectory )
    {
        osMDFilename = 
            CPLFormCIFilename( pszFilename, "manifest.safe", NULL );
    }
    else
        osMDFilename = pszFilename;

/* -------------------------------------------------------------------- */
/*      Ingest the manifest.safe file.                                  */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psManifest, *psAnnotation = NULL;
    //TODO REMOVE CPLXMLNode *psImageAttributes, *psImageGenerationParameters;
    CPLXMLNode *psContentUnits, *psMetaDataObjects, *psDataObjects;

    psManifest = CPLParseXMLFile( osMDFilename );
    if( psManifest == NULL )
        return NULL;
    
    char *pszPath = CPLStrdup(CPLGetPath( osMDFilename ));
    
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
    psContentUnits = CPLGetXMLNode(
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
    psMetaDataObjects = CPLGetXMLNode(
            psManifest, 
            "=xfdu:XFDU.metadataSection" );
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
    psDataObjects = CPLGetXMLNode(psManifest, "=xfdu:XFDU.dataObjectSection" );
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

    GDALDataType eDataType;
    const char *pszAnnotation  = NULL;
    const char *pszCalibration = NULL;
    const char *pszMeasurement = NULL;

    
/* -------------------------------------------------------------------- */
/*      Look for "Measurement Data Unit" contentUnit elements.          */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psContentUnit;

    for( psContentUnit = psContentUnits->psChild;
         psContentUnit != NULL;
         psContentUnit = psContentUnit->psNext )
    {
        if( psContentUnit->eType != CXT_Element 
            || !(EQUAL(psContentUnit->pszValue,"xfdu:contentUnit")) ) {
            continue;
        }
        
        const char *pszUnitType = CPLGetXMLValue( psContentUnit, 
                "unitType", "" );
        
        pszAnnotation  = NULL;
        pszCalibration = NULL;
        pszMeasurement = NULL;
        
        if ( EQUAL(pszUnitType, "Measurement Data Unit") ) {
            /* Get dmdID and dataObjectID */
            const char *pszDmdID;
            const char *pszDataObjectID;
            
            pszDmdID = CPLGetXMLValue(psContentUnit, "dmdID", "");
            
            pszDataObjectID = CPLGetXMLValue( 
                    psContentUnit, 
                    "dataObjectPointer.dataObjectID", "");
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

            if (CSLCount( papszTokens ) == 0) {
                continue;
            }
            
            int j;
            for( j = 0; j < CSLCount( papszTokens ); j++ ) {
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
                const char *pszRepId = CPLGetXMLValue( psDO, "repID", "" );

                if ( EQUAL(pszRepId, "s1Level1ProductSchema") ) {
                    /* Get annotation filename */
                    pszAnnotation = CPLGetXMLValue( 
                            psDO, "byteStream.fileLocation.href", "");
                    if( *pszAnnotation == '\0' ) {
                        continue;
                    }

                } else if ( EQUAL(pszRepId, "s1Level1CalibrationSchema") ) {
                    pszCalibration = CPLGetXMLValue( 
                            psDO, "byteStream.fileLocation.href", "");
                    if( *pszCalibration == '\0' ) {
                        continue;
                    }

                } else {
                    continue;
                }

            }
            
            CSLDestroy(papszTokens);
            
            if (pszAnnotation == NULL || pszCalibration == NULL
                    || pszMeasurement == NULL) {
                continue;
            }
            
            //open Annotation XML file
            CPLString osAnnotationFilePath = CPLFormFilename( pszPath, 
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
            
            const char *pszProductType, *pszMissionId, *pszPolarisation,
                    *pszMode, *pszSwath;
            
            pszProductType = CPLGetXMLValue( 
                psAnnotation, "=product.adsHeader.productType", "UNK" );
            pszMissionId = CPLGetXMLValue( 
                psAnnotation, "=product.adsHeader.missionId", "UNK" );
            pszPolarisation = CPLGetXMLValue( 
                psAnnotation, "=product.adsHeader.polarisation", "UNK" );
            pszMode = CPLGetXMLValue( 
                psAnnotation, "=product.adsHeader.mode", "UNK" );
            pszSwath = CPLGetXMLValue( 
                psAnnotation, "=product.adsHeader.swath", "UNK" );

            poDS->SetMetadataItem("PRODUCT_TYPE", pszProductType);
            poDS->SetMetadataItem("MISSION_ID", pszMissionId);
            poDS->SetMetadataItem("MODE", pszMode);
            poDS->SetMetadataItem("SWATH", pszSwath);
            
/* -------------------------------------------------------------------- */
/*      Get dataType (so we can recognize complex data), and the        */
/*      bitsPerSample.                                                  */
/* -------------------------------------------------------------------- */

            const char *pszDataType;
            
            pszDataType = CPLGetXMLValue( 
                psAnnotation, 
                "=product.imageAnnotation.imageInformation.outputPixels", 
                "" );

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
                CPLStrdup(CPLFormFilename( pszPath, pszMeasurement, NULL ));

/* -------------------------------------------------------------------- */
/*      Try and open the file.                                          */
/* -------------------------------------------------------------------- */
            GDALDataset *poBandFile;

            poBandFile = (GDALDataset *) GDALOpen( pszFullname, GA_ReadOnly );
            if( poBandFile == NULL )
            {
                CPLFree(pszFullname);
            } 
            else
            if (poBandFile->GetRasterCount() == 0)
            {
                GDALClose( (GDALRasterBandH) poBandFile );
                CPLFree(pszFullname);
            }
            else {

                poDS->papszExtraFiles = CSLAddString( poDS->papszExtraFiles,
                                                  pszFullname );

/* -------------------------------------------------------------------- */
/*      Create the band.                                                */
/* -------------------------------------------------------------------- */
                SAFERasterBand *poBand;
                poBand = new SAFERasterBand( poDS, eDataType,
                                            pszPolarisation, 
                                            poBandFile ); 

                poDS->SetBand( poDS->GetRasterCount() + 1, poBand );
            }
            CPLFree( pszFullname );
            
            
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

    const char *pszItem;
    
/* -------------------------------------------------------------------- */
/*      Platform information                                            */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psPlatformAttrs = SAFEDataset::GetMetaDataObject(
        psMetaDataObjects, 
        "platform");
    
    if (psPlatformAttrs != NULL) {
        pszItem = CPLGetXMLValue( 
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
        psMetaDataObjects, 
        "acquisitionPeriod");
    
    if (psAcquisitionAttrs != NULL) {
        pszItem = CPLGetXMLValue( 
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
        psMetaDataObjects, 
        "processing");
    
    if (psProcessingAttrs != NULL) {
        pszItem = CPLGetXMLValue( 
            psProcessingAttrs, 
            "metadataWrap.xmlData.safe:processing.safe:facility.name", "UNK" );
        poDS->SetMetadataItem( "FACILITY_IDENTIFIER", pszItem );
    }
    
/* -------------------------------------------------------------------- */
/*      Measurement Orbit Reference information                         */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psOrbitAttrs = SAFEDataset::GetMetaDataObject(
        psMetaDataObjects, 
        "measurementOrbitReference");
    
    if (psOrbitAttrs != NULL) {
        pszItem = CPLGetXMLValue( psOrbitAttrs,
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
        const char *pszEllipsoidName;
        double minor_axis, major_axis, inv_flattening;
        OGRSpatialReference oLL, oPrj;

        pszEllipsoidName = CPLGetXMLValue( 
            psProcessingInfo, "ellipsoidName", "" );
        minor_axis = CPLAtof(CPLGetXMLValue( 
            psProcessingInfo, "ellipsoidSemiMinorAxis", "0.0" ));
        major_axis = CPLAtof(CPLGetXMLValue( 
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
            inv_flattening = major_axis/(major_axis - minor_axis);
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
        CPLXMLNode *psNode;
        
        for( psNode = psGeoGrid->psChild; psNode != NULL;
             psNode = psNode->psNext )
        {
            if( EQUAL(psNode->pszValue,"geolocationGridPoint") )
                poDS->nGCPCount++ ;
        }

        poDS->pasGCPList = (GDAL_GCP *) 
            CPLCalloc(sizeof(GDAL_GCP),poDS->nGCPCount);
        
        poDS->nGCPCount = 0;
        
        for( psNode = psGeoGrid->psChild; psNode != NULL;
             psNode = psNode->psNext )
        {
            char    szID[32];
            GDAL_GCP   *psGCP = poDS->pasGCPList + poDS->nGCPCount;
            
            if( !EQUAL(psNode->pszValue,"geolocationGridPoint") )
                continue;

            poDS->nGCPCount++ ;

            sprintf( szID, "%d", poDS->nGCPCount );
            psGCP->pszId = CPLStrdup( szID );
            psGCP->pszInfo = CPLStrdup("");
            psGCP->dfGCPPixel = 
                CPLAtof(CPLGetXMLValue(psNode,"pixel","0"));
            psGCP->dfGCPLine = 
                CPLAtof(CPLGetXMLValue(psNode,"line","0"));
            psGCP->dfGCPX = 
                CPLAtof(CPLGetXMLValue(psNode,"longitude",""));
            psGCP->dfGCPY = 
                CPLAtof(CPLGetXMLValue(psNode,"latitude",""));
            psGCP->dfGCPZ = 
                CPLAtof(CPLGetXMLValue(psNode,"height",""));
        }
    }

    CPLFree( pszPath );
    
    CPLDestroyXMLNode(psAnnotation);
    
/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    CPLString osDescription;

    osDescription = osMDFilename;

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

    return( poDS );
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
    return( pszProjection );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr SAFEDataset::GetGeoTransform( double * padfTransform )
{
    memcpy( padfTransform,  adfGeoTransform, sizeof(double) * 6 ); 

    if (bHaveGeoTransform)
        return( CE_None );
   
    return( CE_Failure );
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
    if( pszDomain != NULL && EQUALN( pszDomain, "SUBDATASETS", 11 ) &&
        papszSubDatasets != NULL)
        return papszSubDatasets;
    else
        return GDALDataset::GetMetadata( pszDomain );
}
#endif

/************************************************************************/
/*                         GDALRegister_SAFE()                          */
/************************************************************************/

void GDALRegister_SAFE()
{
    GDALDriver *poDriver;

    if( GDALGetDriverByName( "SAFE" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "SAFE" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" ); 
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Sentinel SAFE Product" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_safe.html" );
        poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "NO" );

        poDriver->pfnOpen = SAFEDataset::Open;
        poDriver->pfnIdentify = SAFEDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
