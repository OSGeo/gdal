/******************************************************************************
 * $Id$
 *
 * Project:  NITF Read/Write Translator
 * Purpose:  NITFDataset and driver related implementations.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at mines-paris dot org>
 *
 * Portions Copyright (c) Her majesty the Queen in right of Canada as
 * represented by the Minister of National Defence, 2006.
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

#include "nitfdataset.h"
#include "cpl_string.h"
#include "cpl_csv.h"

CPL_CVSID("$Id$");

static void NITFPatchImageLength( const char *pszFilename,
                                  GUIntBig nImageOffset, 
                                  GIntBig nPixelCount, const char *pszIC );
static int NITFWriteCGMSegments( const char *pszFilename, char **papszList );
static void NITFWriteTextSegments( const char *pszFilename, char **papszList );

#ifdef JPEG_SUPPORTED
static int NITFWriteJPEGImage( GDALDataset *, VSILFILE *, vsi_l_offset, char **,
                               GDALProgressFunc pfnProgress, 
                               void * pProgressData );
#endif

#ifdef ESRI_BUILD
static void SetBandMetadata( NITFImage *psImage, GDALRasterBand *poBand, int nBand );
#endif

/************************************************************************/
/* ==================================================================== */
/*                             NITFDataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            NITFDataset()                             */
/************************************************************************/

NITFDataset::NITFDataset()

{
    psFile = NULL;
    psImage = NULL;
    bGotGeoTransform = FALSE;
    pszProjection = CPLStrdup("");
    poJ2KDataset = NULL;
    bJP2Writing = FALSE;
    poJPEGDataset = NULL;

    panJPEGBlockOffset = NULL;
    pabyJPEGBlock = NULL;
    nQLevel = 0;

    nGCPCount = 0;
    pasGCPList = NULL;
    pszGCPProjection = NULL;

    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    
    poDriver = (GDALDriver*) GDALGetDriverByName("NITF");

    papszTextMDToWrite = NULL;
    papszCgmMDToWrite = NULL;
    
    bInLoadXML = FALSE;
    bExposeUnderlyingJPEGDatasetOverviews = FALSE;
}

/************************************************************************/
/*                            ~NITFDataset()                            */
/************************************************************************/

NITFDataset::~NITFDataset()

{
    CloseDependentDatasets();

/* -------------------------------------------------------------------- */
/*      Free datastructures.                                            */
/* -------------------------------------------------------------------- */
    CPLFree( pszProjection );

    GDALDeinitGCPs( nGCPCount, pasGCPList );
    CPLFree( pasGCPList );
    CPLFree( pszGCPProjection );

    CPLFree( panJPEGBlockOffset );
    CPLFree( pabyJPEGBlock );
}

/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int NITFDataset::CloseDependentDatasets()
{
    FlushCache();

    int bHasDroppedRef = GDALPamDataset::CloseDependentDatasets();

/* -------------------------------------------------------------------- */
/*      If we have been writing to a JPEG2000 file, check if the        */
/*      color interpretations were set.  If so, apply the settings      */
/*      to the NITF file.                                               */
/* -------------------------------------------------------------------- */
    if( poJ2KDataset != NULL && bJP2Writing )
    {
        int i;

        for( i = 0; i < nBands && papoBands != NULL; i++ )
        {
            if( papoBands[i]->GetColorInterpretation() != GCI_Undefined )
                NITFSetColorInterpretation( psImage, i+1, 
                                papoBands[i]->GetColorInterpretation() );
        }
    }

/* -------------------------------------------------------------------- */
/*      Close the underlying NITF file.                                 */
/* -------------------------------------------------------------------- */
    GUIntBig nImageStart = 0;
    if( psFile != NULL )
    {
        if (psFile->nSegmentCount > 0)
            nImageStart = psFile->pasSegmentInfo[0].nSegmentStart;

        NITFClose( psFile );
        psFile = NULL;
    }

/* -------------------------------------------------------------------- */
/*      If we have a jpeg2000 output file, make sure it gets closed     */
/*      and flushed out.                                                */
/* -------------------------------------------------------------------- */
    if( poJ2KDataset != NULL )
    {
        GDALClose( (GDALDatasetH) poJ2KDataset );
        poJ2KDataset = NULL;
        bHasDroppedRef = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Update file length, and COMRAT for JPEG2000 files we are        */
/*      writing to.                                                     */
/* -------------------------------------------------------------------- */
    if( bJP2Writing )
    {
        GIntBig nPixelCount = nRasterXSize * ((GIntBig) nRasterYSize) * 
            nBands;

        NITFPatchImageLength( GetDescription(), nImageStart, nPixelCount, 
                              "C8" );
    }

    bJP2Writing = FALSE;

/* -------------------------------------------------------------------- */
/*      If we have a jpeg output file, make sure it gets closed         */
/*      and flushed out.                                                */
/* -------------------------------------------------------------------- */
    if( poJPEGDataset != NULL )
    {
        GDALClose( (GDALDatasetH) poJPEGDataset );
        poJPEGDataset = NULL;
        bHasDroppedRef = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      If the dataset was opened by Create(), we may need to write     */
/*      the CGM and TEXT segments                                       */
/* -------------------------------------------------------------------- */
    NITFWriteCGMSegments( GetDescription(), papszCgmMDToWrite );
    NITFWriteTextSegments( GetDescription(), papszTextMDToWrite );

    CSLDestroy(papszTextMDToWrite);
    papszTextMDToWrite = NULL;
    CSLDestroy(papszCgmMDToWrite);
    papszCgmMDToWrite = NULL;

/* -------------------------------------------------------------------- */
/*      Destroy the raster bands if they exist.                         */
/* We must do it now since the rasterbands can be NITFWrapperRasterBand */
/* that derive from the GDALProxyRasterBand object, which keeps         */
/* a reference on the JPEG/JP2K dataset, so any later call to           */
/* FlushCache() would result in FlushCache() being called on a          */
/* already destroyed object                                             */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
       delete papoBands[iBand];
    }
    nBands = 0;

    return bHasDroppedRef;
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void NITFDataset::FlushCache()

{
    // If the JPEG/JP2K dataset has dirty pam info, then we should consider 
    // ourselves to as well.
    if( poJPEGDataset != NULL 
        && (poJPEGDataset->GetPamFlags() & GPF_DIRTY) )
        MarkPamDirty();
    if( poJ2KDataset != NULL 
        && (poJ2KDataset->GetPamFlags() & GPF_DIRTY) )
        MarkPamDirty();

    if( poJ2KDataset != NULL && bJP2Writing)
        poJ2KDataset->FlushCache();

    GDALPamDataset::FlushCache();
}

#ifdef ESRI_BUILD

/************************************************************************/
/*                           ExtractEsriMD()                            */
/*                                                                      */
/*      Extracts ESRI-specific required meta data from metadata         */
/*      string list papszStrList.                                       */
/************************************************************************/

static char **ExtractEsriMD( char **papszMD )
{
    char **papszEsriMD = NULL;

    if( papszMD )
    {
        // These are the current generic ESRI metadata.
        const char *const pEsriMDAcquisitionDate   = "ESRI_MD_ACQUISITION_DATE";
        const char *const pEsriMDAngleToNorth      = "ESRI_MD_ANGLE_TO_NORTH";
        const char *const pEsriMDCircularError     = "ESRI_MD_CE";
        const char *const pEsriMDDataType          = "ESRI_MD_DATA_TYPE";
        const char *const pEsriMDIsCloudCover      = "ESRI_MD_ISCLOUDCOVER";
        const char *const pEsriMDLinearError       = "ESRI_MD_LE";
        const char *const pEsriMDOffNaDir          = "ESRI_MD_OFF_NADIR";
        const char *const pEsriMDPercentCloudCover = "ESRI_MD_PERCENT_CLOUD_COVER";
        const char *const pEsriMDProductName       = "ESRI_MD_PRODUCT_NAME";
        const char *const pEsriMDSensorAzimuth     = "ESRI_MD_SENSOR_AZIMUTH";
        const char *const pEsriMDSensorElevation   = "ESRI_MD_SENSOR_ELEVATION";
        const char *const pEsriMDSensorName        = "ESRI_MD_SENSOR_NAME";
        const char *const pEsriMDSunAzimuth        = "ESRI_MD_SUN_AZIMUTH";
        const char *const pEsriMDSunElevation      = "ESRI_MD_SUN_ELEVATION";

        char         szField[11];
        const char  *pCCImageSegment = CSLFetchNameValue( papszMD, "NITF_IID1" );
        std::string  ccSegment("false");

        if( ( pCCImageSegment != NULL ) && ( strlen(pCCImageSegment) <= 10 ) )
        {
            szField[0] = '\0';
            strncpy( szField, pCCImageSegment, strlen(pCCImageSegment) );
            szField[strlen(pCCImageSegment)] = '\0';

            // Trim white off tag.
            while( ( strlen(szField) > 0 ) && ( szField[strlen(szField)-1] == ' ' ) )
                szField[strlen(szField)-1] = '\0';

            if ((strlen(szField) == 2) && (EQUALN(szField, "CC", 2))) ccSegment.assign("true");
        }

        const char *pAcquisitionDate   = CSLFetchNameValue( papszMD, "NITF_FDT" );
        const char *pAngleToNorth      = CSLFetchNameValue( papszMD, "NITF_CSEXRA_ANGLE_TO_NORTH" );
        const char *pCircularError     = CSLFetchNameValue( papszMD, "NITF_CSEXRA_CIRCL_ERR" );      // Unit in feet.
        const char *pLinearError       = CSLFetchNameValue( papszMD, "NITF_CSEXRA_LINEAR_ERR" );     // Unit in feet.
        const char *pPercentCloudCover = CSLFetchNameValue( papszMD, "NITF_PIAIMC_CLOUDCVR" );
        const char *pProductName       = CSLFetchNameValue( papszMD, "NITF_CSDIDA_PRODUCT_ID" );
        const char *pSensorName        = CSLFetchNameValue( papszMD, "NITF_PIAIMC_SENSNAME" );
        const char *pSunAzimuth        = CSLFetchNameValue( papszMD, "NITF_CSEXRA_SUN_AZIMUTH" );
        const char *pSunElevation      = CSLFetchNameValue( papszMD, "NITF_CSEXRA_SUN_ELEVATION" );

        // Get ESRI_MD_DATA_TYPE.
        const char *pDataType        = NULL;
        const char *pImgSegFieldICAT = CSLFetchNameValue( papszMD, "NITF_ICAT" );

        if( ( pImgSegFieldICAT != NULL ) && ( EQUALN(pImgSegFieldICAT, "DTEM", 4) ) )
            pDataType = "Elevation";
        else
            pDataType = "Generic";

        if( pAngleToNorth == NULL )
            pAngleToNorth = CSLFetchNameValue( papszMD, "NITF_USE00A_ANGLE_TO_NORTH" );

        // Percent cloud cover == 999 means that the information is not available.
        if( (pPercentCloudCover != NULL) &&  (EQUALN(pPercentCloudCover, "999", 3)) )
            pPercentCloudCover = NULL;

        pAngleToNorth = CSLFetchNameValue( papszMD, "NITF_USE00A_ANGLE_TO_NORTH" );

        if( pSunAzimuth == NULL )
            pSunAzimuth = CSLFetchNameValue( papszMD, "NITF_USE00A_SUN_AZ" );

        if( pSunElevation == NULL )
            pSunElevation = CSLFetchNameValue( papszMD, "NITF_USE00A_SUN_EL" );

        // CSLAddNameValue will not add the key/value pair if the value is NULL.
        papszEsriMD = CSLAddNameValue( papszEsriMD, pEsriMDAcquisitionDate,   pAcquisitionDate );
        papszEsriMD = CSLAddNameValue( papszEsriMD, pEsriMDAngleToNorth,      pAngleToNorth );
        papszEsriMD = CSLAddNameValue( papszEsriMD, pEsriMDCircularError,     pCircularError );
        papszEsriMD = CSLAddNameValue( papszEsriMD, pEsriMDDataType,          pDataType );
        papszEsriMD = CSLAddNameValue( papszEsriMD, pEsriMDIsCloudCover,      ccSegment.c_str() );
        papszEsriMD = CSLAddNameValue( papszEsriMD, pEsriMDLinearError,       pLinearError );
        papszEsriMD = CSLAddNameValue( papszEsriMD, pEsriMDProductName,       pProductName );
        papszEsriMD = CSLAddNameValue( papszEsriMD, pEsriMDPercentCloudCover, pPercentCloudCover );
        papszEsriMD = CSLAddNameValue( papszEsriMD, pEsriMDSensorName,        pSensorName );
        papszEsriMD = CSLAddNameValue( papszEsriMD, pEsriMDSunAzimuth,        pSunAzimuth );
        papszEsriMD = CSLAddNameValue( papszEsriMD, pEsriMDSunElevation,      pSunElevation );
    }

    return (papszEsriMD);
}

/************************************************************************/
/*                          SetBandMetadata()                           */
/************************************************************************/

static void SetBandMetadata( NITFImage *psImage, GDALRasterBand *poBand, int nBand )
{
    if( (psImage != NULL) && (poBand != NULL) && (nBand > 0) )
    {
        NITFBandInfo *psBandInfo = psImage->pasBandInfo + nBand - 1;

        if( psBandInfo != NULL )
        {
            // Set metadata BandName, WavelengthMax and WavelengthMin.

            if ( psBandInfo->szIREPBAND != NULL )
            {
                if( EQUAL(psBandInfo->szIREPBAND,"B") )
                {
                    poBand->SetMetadataItem( "BandName", "Blue" );
                    poBand->SetMetadataItem( "WavelengthMax", psBandInfo->szISUBCAT );
                    poBand->SetMetadataItem( "WavelengthMin", psBandInfo->szISUBCAT );
                }
                else if( EQUAL(psBandInfo->szIREPBAND,"G") )
                {
                    poBand->SetMetadataItem( "BandName", "Green" );
                    poBand->SetMetadataItem( "WavelengthMax", psBandInfo->szISUBCAT );
                    poBand->SetMetadataItem( "WavelengthMin", psBandInfo->szISUBCAT );
                }
                else if( EQUAL(psBandInfo->szIREPBAND,"R") )
                {
                    poBand->SetMetadataItem( "BandName", "Red" );
                    poBand->SetMetadataItem( "WavelengthMax", psBandInfo->szISUBCAT );
                    poBand->SetMetadataItem( "WavelengthMin", psBandInfo->szISUBCAT );
                }
                else if( EQUAL(psBandInfo->szIREPBAND,"N") )
                {
                    poBand->SetMetadataItem( "BandName", "NearInfrared" );
                    poBand->SetMetadataItem( "WavelengthMax", psBandInfo->szISUBCAT );
                    poBand->SetMetadataItem( "WavelengthMin", psBandInfo->szISUBCAT );
                }
                else if( ( EQUAL(psBandInfo->szIREPBAND,"M") ) || ( ( psImage->szIREP != NULL ) && ( EQUAL(psImage->szIREP,"MONO") ) ) )
                {
                    poBand->SetMetadataItem( "BandName", "Panchromatic" );
                }
                else
                {
                    if( ( psImage->szICAT != NULL ) && ( EQUAL(psImage->szICAT,"IR") ) )
                    {
                        poBand->SetMetadataItem( "BandName", "Infrared" );
                        poBand->SetMetadataItem( "WavelengthMax", psBandInfo->szISUBCAT );
                        poBand->SetMetadataItem( "WavelengthMin", psBandInfo->szISUBCAT );
                    }
                }
            }
        }
    }
}

#endif /* def ESRI_BUILD */

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int NITFDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    const char *pszFilename = poOpenInfo->pszFilename;

/* -------------------------------------------------------------------- */
/*      Is this a dataset selector? If so, it is obviously NITF.        */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszFilename, "NITF_IM:",8) )
        return TRUE;

/* -------------------------------------------------------------------- */
/*      Avoid that on Windows, JPEG_SUBFILE:x,y,z,data/../tmp/foo.ntf   */
/*      to be recognized by the NITF driver, because                    */
/*      'JPEG_SUBFILE:x,y,z,data' is considered as a (valid) directory  */
/*      and thus the whole filename is evaluated as tmp/foo.ntf         */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszFilename,"JPEG_SUBFILE:",13) )
        return FALSE;
        
/* -------------------------------------------------------------------- */
/*	First we check to see if the file has the expected header	*/
/*	bytes.								*/    
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 4 )
        return FALSE;
    
    if( !EQUALN((char *) poOpenInfo->pabyHeader,"NITF",4) 
        && !EQUALN((char *) poOpenInfo->pabyHeader,"NSIF",4)
        && !EQUALN((char *) poOpenInfo->pabyHeader,"NITF",4) )
        return FALSE;

    int i;
    /* Check that it's not in fact a NITF A.TOC file, which is handled by the RPFTOC driver */
    for(i=0;i<(int)poOpenInfo->nHeaderBytes-(int)strlen("A.TOC");i++)
    {
        if (EQUALN((const char*)poOpenInfo->pabyHeader + i, "A.TOC", strlen("A.TOC")))
            return FALSE;
    }

    return TRUE;
}
        
/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *NITFDataset::Open( GDALOpenInfo * poOpenInfo )
{
    return OpenInternal(poOpenInfo, NULL, FALSE);
}

GDALDataset *NITFDataset::OpenInternal( GDALOpenInfo * poOpenInfo,
                                GDALDataset *poWritableJ2KDataset,
                                int bOpenForCreate)

{
    int nIMIndex = -1;
    const char *pszFilename = poOpenInfo->pszFilename;

    if( !Identify( poOpenInfo ) )
        return NULL;
        
/* -------------------------------------------------------------------- */
/*      Select a specific subdataset.                                   */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszFilename, "NITF_IM:",8) )
    {
        pszFilename += 8;
        nIMIndex = atoi(pszFilename);
        
        while( *pszFilename != '\0' && *pszFilename != ':' )
            pszFilename++;

        if( *pszFilename == ':' )
            pszFilename++;
    }

/* -------------------------------------------------------------------- */
/*      Open the file with library.                                     */
/* -------------------------------------------------------------------- */
    NITFFile *psFile;

    psFile = NITFOpen( pszFilename, poOpenInfo->eAccess == GA_Update );
    if( psFile == NULL )
    {
        return NULL;
    }

    if (!bOpenForCreate)
    {
        NITFCollectAttachments( psFile );
        NITFReconcileAttachments( psFile );
    }

/* -------------------------------------------------------------------- */
/*      Is there an image to operate on?                                */
/* -------------------------------------------------------------------- */
    int iSegment, nThisIM = 0;
    NITFImage *psImage = NULL;

    for( iSegment = 0; iSegment < psFile->nSegmentCount; iSegment++ )
    {
        if( EQUAL(psFile->pasSegmentInfo[iSegment].szSegmentType,"IM") 
            && (nThisIM++ == nIMIndex || nIMIndex == -1) )
        {
            psImage = NITFImageAccess( psFile, iSegment );
            if( psImage == NULL )
            {
                NITFClose( psFile );
                return NULL;
            }
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      If no image segments found report this to the user.             */
/* -------------------------------------------------------------------- */
    if( psImage == NULL )
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "The file %s appears to be an NITF file, but no image\n"
                  "blocks were found on it.", 
                  poOpenInfo->pszFilename );
    }
    
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    NITFDataset 	*poDS;

    poDS = new NITFDataset();

    poDS->psFile = psFile;
    poDS->psImage = psImage;
    poDS->eAccess = poOpenInfo->eAccess;
    poDS->osNITFFilename = pszFilename;
    poDS->nIMIndex = nIMIndex;

    if( psImage )
    {
        if (psImage->nCols <= 0 || psImage->nRows <= 0 || 
            psImage->nBlockWidth <= 0 || psImage->nBlockHeight <= 0) 
        { 
            CPLError( CE_Failure, CPLE_AppDefined,  
                      "Bad values in NITF image : nCols=%d, nRows=%d, nBlockWidth=%d, nBlockHeight=%d", 
                      psImage->nCols, psImage->nRows, psImage->nBlockWidth, psImage->nBlockHeight); 
            delete poDS; 
            return NULL; 
        } 

        poDS->nRasterXSize = psImage->nCols;
        poDS->nRasterYSize = psImage->nRows;
    }
    else
    {
        poDS->nRasterXSize = 1;
        poDS->nRasterYSize = 1;
    }
        
    /* Can be set to NO to avoid opening the underlying JPEG2000/JPEG */
    /* stream. Might speed up operations when just metadata is needed */
    int bOpenUnderlyingDS = CSLTestBoolean(
            CPLGetConfigOption("NITF_OPEN_UNDERLYING_DS", "YES"));

/* -------------------------------------------------------------------- */
/*      If the image is JPEG2000 (C8) compressed, we will need to       */
/*      open the image data as a JPEG2000 dataset.                      */
/* -------------------------------------------------------------------- */
    int nUsableBands = 0;
    int iBand;
    int bSetColorInterpretation = TRUE;
    int bSetColorTable = FALSE;

    if( psImage )
        nUsableBands = psImage->nBands;

    if( bOpenUnderlyingDS && psImage != NULL && EQUAL(psImage->szIC,"C8") )
    {
        CPLString osDSName;

        osDSName.Printf( "/vsisubfile/" CPL_FRMT_GUIB "_" CPL_FRMT_GUIB ",%s", 
                         psFile->pasSegmentInfo[iSegment].nSegmentStart,
                         psFile->pasSegmentInfo[iSegment].nSegmentSize,
                         pszFilename );
    
        if( poWritableJ2KDataset != NULL )
        {
            poDS->poJ2KDataset = (GDALPamDataset *) poWritableJ2KDataset; 
            poDS->bJP2Writing = TRUE;
            poWritableJ2KDataset = NULL;
        }
        else
        {
            /* We explicitely list the allowed drivers to avoid hostile content */
            /* to be opened by a random driver, and also to make sure that */
            /* a future new JPEG2000 compatible driver derives from GDALPamDataset */
            static const char * const apszDrivers[] = { "JP2KAK", "JP2ECW", "JP2MRSID",
                                                        "JPEG2000", "JP2OPENJPEG", NULL };
            poDS->poJ2KDataset = (GDALPamDataset *)
                GDALOpenInternal( osDSName, GA_ReadOnly, apszDrivers);

            if( poDS->poJ2KDataset == NULL )
            {
                int bFoundJPEG2000Driver = FALSE;
                for(int iDriver=0;apszDrivers[iDriver]!=NULL;iDriver++)
                {
                    if (GDALGetDriverByName(apszDrivers[iDriver]) != NULL)
                        bFoundJPEG2000Driver = TRUE;
                }

                CPLError( CE_Failure, CPLE_AppDefined,
                        "Unable to open JPEG2000 image within NITF file.\n%s\n%s",
                         (!bFoundJPEG2000Driver) ?
                            "No JPEG2000 capable driver (JP2KAK, JP2ECW, JP2MRSID, JP2OPENJPEG, etc...) is available." :
                            "One or several JPEG2000 capable drivers are available but the datastream could not be opened successfully.",
                         "You can define the NITF_OPEN_UNDERLYING_DS configuration option to NO, in order to just get the metadata.");
                delete poDS;
                return NULL;
            }

            poDS->poJ2KDataset->SetPamFlags( 
                poDS->poJ2KDataset->GetPamFlags() | GPF_NOSAVE );
        }

        if( poDS->GetRasterXSize() != poDS->poJ2KDataset->GetRasterXSize()
            || poDS->GetRasterYSize() != poDS->poJ2KDataset->GetRasterYSize())
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "JPEG2000 data stream has not the same dimensions as the NITF file.");
            delete poDS;
            return NULL;
        }
        
        if ( nUsableBands == 1)
        {
            const char* pszIREP = CSLFetchNameValue(psImage->papszMetadata, "NITF_IREP");
            if (pszIREP != NULL && EQUAL(pszIREP, "RGB/LUT"))
            {
                if (poDS->poJ2KDataset->GetRasterCount() == 3)
                {
/* Test case : http://www.gwg.nga.mil/ntb/baseline/software/testfile/Jpeg2000/jp2_09/file9_jp2_2places.ntf */
/* 256-entry palette/LUT in both JP2 Header and image Subheader */
/* In this case, the JPEG2000 driver will probably do the RGB expension */
                    nUsableBands = 3;
                    bSetColorInterpretation = FALSE;
                }
                else if (poDS->poJ2KDataset->GetRasterCount() == 1 &&
                         psImage->pasBandInfo[0].nSignificantLUTEntries > 0)
                {
/* Test case : http://www.gwg.nga.mil/ntb/baseline/software/testfile/Jpeg2000/jp2_09/file9_j2c.ntf */
/* 256-entry/LUT in Image Subheader, JP2 header completely removed */
/* The JPEG2000 driver will decode it as a grey band */
/* So we must set the color table on the wrapper band */
/* or for file9_jp2_2places.ntf as well if the J2K driver does do RGB expension */
                    bSetColorTable = TRUE;
                }
            }
        }

        if( poDS->poJ2KDataset->GetRasterCount() < nUsableBands )
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "JPEG2000 data stream has less useful bands than expected, likely\n"
                      "because some channels have differing resolutions." );
            
            nUsableBands = poDS->poJ2KDataset->GetRasterCount();
        }
    }

/* -------------------------------------------------------------------- */
/*      If the image is JPEG (C3) compressed, we will need to open      */
/*      the image data as a JPEG dataset.                               */
/* -------------------------------------------------------------------- */
    else if( bOpenUnderlyingDS && psImage != NULL
             && EQUAL(psImage->szIC,"C3") 
             && psImage->nBlocksPerRow == 1
             && psImage->nBlocksPerColumn == 1 )
    {
        GUIntBig nJPEGStart = psFile->pasSegmentInfo[iSegment].nSegmentStart;

        poDS->nQLevel = poDS->ScanJPEGQLevel( &nJPEGStart );

        CPLString osDSName;

        osDSName.Printf( "JPEG_SUBFILE:Q%d," CPL_FRMT_GUIB "," CPL_FRMT_GUIB ",%s", 
                         poDS->nQLevel, nJPEGStart,
                         psFile->pasSegmentInfo[iSegment].nSegmentSize
                         - (nJPEGStart - psFile->pasSegmentInfo[iSegment].nSegmentStart),
                         pszFilename );

        CPLDebug( "GDAL", 
                  "NITFDataset::Open() as IC=C3 (JPEG compressed)\n");

        poDS->poJPEGDataset = (GDALPamDataset*) GDALOpen(osDSName,GA_ReadOnly);
        if( poDS->poJPEGDataset == NULL )
        {
            int bFoundJPEGDriver = GDALGetDriverByName("JPEG") != NULL;
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Unable to open JPEG image within NITF file.\n%s\n%s",
                     (!bFoundJPEGDriver) ?
                        "The JPEG driver is not available." :
                        "The JPEG driver is available but the datastream could not be opened successfully.",
                     "You can define the NITF_OPEN_UNDERLYING_DS configuration option to NO, in order to just get the metadata.");
            delete poDS;
            return NULL;
        }

        /* In some circumstances, the JPEG image can be larger than the NITF */
        /* (NCOLS, NROWS) dimensions (#5001), so accept it as a valid case */
        /* But reject when it is smaller than the NITF dimensions. */
        if( poDS->GetRasterXSize() > poDS->poJPEGDataset->GetRasterXSize()
            || poDS->GetRasterYSize() > poDS->poJPEGDataset->GetRasterYSize())
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "JPEG data stream has smaller dimensions than the NITF file.");
            delete poDS;
            return NULL;
        }

        poDS->poJPEGDataset->SetPamFlags( 
            poDS->poJPEGDataset->GetPamFlags() | GPF_NOSAVE );

        if( poDS->poJPEGDataset->GetRasterCount() < nUsableBands )
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "JPEG data stream has less useful bands than expected, likely\n"
                      "because some channels have differing resolutions." );
            
            nUsableBands = poDS->poJPEGDataset->GetRasterCount();
        }
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */

    GDALDataset*    poBaseDS = NULL;
    if (poDS->poJ2KDataset != NULL)
        poBaseDS = poDS->poJ2KDataset;
    else if (poDS->poJPEGDataset != NULL)
        poBaseDS = poDS->poJPEGDataset;

    for( iBand = 0; iBand < nUsableBands; iBand++ )
    {
        if( poBaseDS != NULL)
        {
            GDALRasterBand* poBaseBand =
                poBaseDS->GetRasterBand(iBand+1);

#ifdef ESRI_BUILD
            SetBandMetadata( psImage, poBaseBand, iBand+1 );
#endif

            NITFWrapperRasterBand* poBand =
                new NITFWrapperRasterBand(poDS, poBaseBand, iBand+1 );
                
            NITFBandInfo *psBandInfo = psImage->pasBandInfo + iBand;
            if (bSetColorInterpretation)
            {
                /* FIXME? Does it make sense if the JPEG/JPEG2000 driver decodes */
                /* YCbCr data as RGB. We probably don't want to set */
                /* the color interpretation as Y, Cb, Cr */
                if( EQUAL(psBandInfo->szIREPBAND,"R") )
                    poBand->SetColorInterpretation( GCI_RedBand );
                if( EQUAL(psBandInfo->szIREPBAND,"G") )
                    poBand->SetColorInterpretation( GCI_GreenBand );
                if( EQUAL(psBandInfo->szIREPBAND,"B") )
                    poBand->SetColorInterpretation( GCI_BlueBand );
                if( EQUAL(psBandInfo->szIREPBAND,"M") )
                    poBand->SetColorInterpretation( GCI_GrayIndex );
                if( EQUAL(psBandInfo->szIREPBAND,"Y") )
                    poBand->SetColorInterpretation( GCI_YCbCr_YBand );
                if( EQUAL(psBandInfo->szIREPBAND,"Cb") )
                    poBand->SetColorInterpretation( GCI_YCbCr_CbBand );
                if( EQUAL(psBandInfo->szIREPBAND,"Cr") )
                    poBand->SetColorInterpretation( GCI_YCbCr_CrBand );
            }
            if (bSetColorTable)
            {
                poBand->SetColorTableFromNITFBandInfo();
                poBand->SetColorInterpretation( GCI_PaletteIndex );
            }
            
            poDS->SetBand( iBand+1, poBand );
        }
        else
        {
            GDALRasterBand* poBand = new NITFRasterBand( poDS, iBand+1 );
            if (poBand->GetRasterDataType() == GDT_Unknown)
            {
                delete poBand;
                delete poDS;
                return NULL;
            }

#ifdef ESRI_BUILD
            SetBandMetadata( psImage, poBand, iBand+1 );
#endif

            poDS->SetBand( iBand+1, poBand );
        }
    }

/* -------------------------------------------------------------------- */
/*      Report problems with odd bit sizes.                             */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update &&
        psImage != NULL 
        && (psImage->nBitsPerSample % 8 != 0) 
        && poDS->poJPEGDataset == NULL
        && poDS->poJ2KDataset == NULL )
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "Image with %d bits per sample cannot be opened in update mode.", 
                  psImage->nBitsPerSample );
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Process the projection from the ICORDS.                         */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRSWork;

    if( psImage == NULL )
    {
        /* nothing */
    }
    else if( psImage->chICORDS == 'G' || psImage->chICORDS == 'D' )
    {
        CPLFree( poDS->pszProjection );
        poDS->pszProjection = NULL;
        
        oSRSWork.SetWellKnownGeogCS( "WGS84" );
        oSRSWork.exportToWkt( &(poDS->pszProjection) );
    }
    else if( psImage->chICORDS == 'C' )
    {
        CPLFree( poDS->pszProjection );
        poDS->pszProjection = NULL;
        
        oSRSWork.SetWellKnownGeogCS( "WGS84" );
        oSRSWork.exportToWkt( &(poDS->pszProjection) );

        /* convert latitudes from geocentric to geodetic form. */
        
        psImage->dfULY = 
            NITF_WGS84_Geocentric_Latitude_To_Geodetic_Latitude( 
                psImage->dfULY );
        psImage->dfLLY = 
            NITF_WGS84_Geocentric_Latitude_To_Geodetic_Latitude( 
                psImage->dfLLY );
        psImage->dfURY = 
            NITF_WGS84_Geocentric_Latitude_To_Geodetic_Latitude( 
                psImage->dfURY );
        psImage->dfLRY = 
            NITF_WGS84_Geocentric_Latitude_To_Geodetic_Latitude( 
                psImage->dfLRY );
    }
    else if( psImage->chICORDS == 'S' || psImage->chICORDS == 'N' )
    {
        CPLFree( poDS->pszProjection );
        poDS->pszProjection = NULL;

        oSRSWork.SetUTM( psImage->nZone, psImage->chICORDS == 'N' );
        oSRSWork.SetWellKnownGeogCS( "WGS84" );
        oSRSWork.exportToWkt( &(poDS->pszProjection) );
    }
    else if( psImage->chICORDS == 'U' && psImage->nZone != 0 )
    {
        CPLFree( poDS->pszProjection );
        poDS->pszProjection = NULL;

        oSRSWork.SetUTM( ABS(psImage->nZone), psImage->nZone > 0 );
        oSRSWork.SetWellKnownGeogCS( "WGS84" );
        oSRSWork.exportToWkt( &(poDS->pszProjection) );
    }


/* -------------------------------------------------------------------- */
/*      Try looking for a .nfw file.                                    */
/* -------------------------------------------------------------------- */
    if( psImage
        && GDALReadWorldFile( pszFilename, "nfw", 
                              poDS->adfGeoTransform ) )
    {
        const char *pszHDR;
        VSILFILE *fpHDR;
        char **papszLines;
        int isNorth;
        int zone;
        
        poDS->bGotGeoTransform = TRUE;

        /* If nfw found, try looking for a header with projection info */
        /* in space imaging style format                               */
        pszHDR = CPLResetExtension( pszFilename, "hdr" );
        
        fpHDR = VSIFOpenL( pszHDR, "rt" );

        if( fpHDR == NULL && VSIIsCaseSensitiveFS(pszHDR) )
        {
            pszHDR = CPLResetExtension( pszFilename, "HDR" );
            fpHDR = VSIFOpenL( pszHDR, "rt" );
        }
    
        if( fpHDR != NULL )
        {
            VSIFCloseL( fpHDR );
            papszLines=CSLLoad2(pszHDR, 16, 200, NULL);
            if (CSLCount(papszLines) == 16)
            {

                if (psImage->chICORDS == 'N')
                    isNorth=1;
                else if (psImage->chICORDS =='S')
                    isNorth=0;
                else if (psImage->chICORDS == 'G' || psImage->chICORDS == 'D' || psImage->chICORDS == 'C')
                {
                    if (psImage->dfLLY+psImage->dfLRY+psImage->dfULY+psImage->dfURY < 0)
                        isNorth=0;
                    else
                        isNorth=1;
                }
                else if (psImage->chICORDS == 'U')
                {
                    isNorth = psImage->nZone >= 0;
                }
                else
                {
                    isNorth = 1; /* arbitrarly suppose we are in northern hemisphere */

                    /* unless we have other information to determine the hemisphere */
                    char** papszUSE00A_MD = NITFReadSTDIDC( psImage );
                    if( papszUSE00A_MD != NULL )
                    {
                        const char* pszLocation = CSLFetchNameValue(papszUSE00A_MD, "NITF_STDIDC_LOCATION");
                        if (pszLocation && strlen(pszLocation) == 11)
                        {
                            isNorth = (pszLocation[4] == 'N');
                        }
                        CSLDestroy( papszUSE00A_MD );
                    }
                    else
                    {
                        NITFRPC00BInfo sRPCInfo;
                        if( NITFReadRPC00B( psImage, &sRPCInfo ) && sRPCInfo.SUCCESS )
                        {
                            isNorth = (sRPCInfo.LAT_OFF >= 0);
                        }
                    }
                }

                if( (EQUALN(papszLines[7],
                            "Selected Projection: Universal Transverse Mercator",50)) &&
                    (EQUALN(papszLines[8],"Zone: ",6)) &&
                    (strlen(papszLines[8]) >= 7))
                {
                    CPLFree( poDS->pszProjection );
                    poDS->pszProjection = NULL;
                    zone=atoi(&(papszLines[8][6]));
                    oSRSWork.Clear();
                    oSRSWork.SetUTM( zone, isNorth );
                    oSRSWork.SetWellKnownGeogCS( "WGS84" );
                    oSRSWork.exportToWkt( &(poDS->pszProjection) );
                }
                else
                {
                    /* Couldn't find associated projection info.
                       Go back to original file for geotransform.
                    */
                    poDS->bGotGeoTransform = FALSE;
                }
            }
            else
                poDS->bGotGeoTransform = FALSE;
            CSLDestroy(papszLines);
        }
        else
            poDS->bGotGeoTransform = FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Does this look like a CADRG polar tile ? (#2940)                */
/* -------------------------------------------------------------------- */
    const char* pszIID1 = (psImage) ? CSLFetchNameValue(psImage->papszMetadata, "NITF_IID1") : NULL;
    const char* pszITITLE = (psImage) ? CSLFetchNameValue(psImage->papszMetadata, "NITF_ITITLE") : NULL;
    if( psImage != NULL && !poDS->bGotGeoTransform &&
        (psImage->chICORDS == 'G' || psImage->chICORDS == 'D') &&
        pszIID1 != NULL && EQUAL(pszIID1, "CADRG") &&
        pszITITLE != NULL && strlen(pszITITLE) >= 12 
        && (pszITITLE[strlen(pszITITLE) - 1] == '9' 
            || pszITITLE[strlen(pszITITLE) - 1] == 'J') )
    {
        /* To get a perfect rectangle in Azimuthal Equidistant projection, we must use */
        /* the sphere and not WGS84 ellipsoid. That's a bit strange... */
        const char* pszNorthPolarProjection = "+proj=aeqd +lat_0=90 +lon_0=0 +x_0=0 +y_0=0 +a=6378137 +b=6378137 +units=m +no_defs";
        const char* pszSouthPolarProjection = "+proj=aeqd +lat_0=-90 +lon_0=0 +x_0=0 +y_0=0 +a=6378137 +b=6378137 +units=m +no_defs";

        OGRSpatialReference oSRS_AEQD, oSRS_WGS84;

        const char *pszPolarProjection = (psImage->dfULY > 0) ? pszNorthPolarProjection : pszSouthPolarProjection;
        oSRS_AEQD.importFromProj4(pszPolarProjection);

        oSRS_WGS84.SetWellKnownGeogCS( "WGS84" );

        CPLPushErrorHandler( CPLQuietErrorHandler );
        OGRCoordinateTransformationH hCT =
            (OGRCoordinateTransformationH)OGRCreateCoordinateTransformation(&oSRS_WGS84, &oSRS_AEQD);
        CPLPopErrorHandler();
        if (hCT)
        {
            double dfULX_AEQD = psImage->dfULX;
            double dfULY_AEQD = psImage->dfULY;
            double dfURX_AEQD = psImage->dfURX;
            double dfURY_AEQD = psImage->dfURY;
            double dfLLX_AEQD = psImage->dfLLX;
            double dfLLY_AEQD = psImage->dfLLY;
            double dfLRX_AEQD = psImage->dfLRX;
            double dfLRY_AEQD = psImage->dfLRY;
            double z = 0;
            int bSuccess = TRUE;
            bSuccess &= OCTTransform(hCT, 1, &dfULX_AEQD, &dfULY_AEQD, &z);
            bSuccess &= OCTTransform(hCT, 1, &dfURX_AEQD, &dfURY_AEQD, &z);
            bSuccess &= OCTTransform(hCT, 1, &dfLLX_AEQD, &dfLLY_AEQD, &z);
            bSuccess &= OCTTransform(hCT, 1, &dfLRX_AEQD, &dfLRY_AEQD, &z);
            if (bSuccess)
            {
                /* Check that the coordinates of the 4 corners in Azimuthal Equidistant projection */
                /* are a rectangle */
                if (fabs((dfULX_AEQD - dfLLX_AEQD) / dfLLX_AEQD) < 1e-6 &&
                    fabs((dfURX_AEQD - dfLRX_AEQD) / dfLRX_AEQD) < 1e-6 &&
                    fabs((dfULY_AEQD - dfURY_AEQD) / dfURY_AEQD) < 1e-6 &&
                    fabs((dfLLY_AEQD - dfLRY_AEQD) / dfLRY_AEQD) < 1e-6)
                {
                    CPLFree(poDS->pszProjection);
                    oSRS_AEQD.exportToWkt( &(poDS->pszProjection) );

                    poDS->bGotGeoTransform = TRUE;
                    poDS->adfGeoTransform[0] = dfULX_AEQD;
                    poDS->adfGeoTransform[1] = (dfURX_AEQD - dfULX_AEQD) / poDS->nRasterXSize;
                    poDS->adfGeoTransform[2] = 0;
                    poDS->adfGeoTransform[3] = dfULY_AEQD;
                    poDS->adfGeoTransform[4] = 0;
                    poDS->adfGeoTransform[5] = (dfLLY_AEQD - dfULY_AEQD) / poDS->nRasterYSize;
                }
            }
            OCTDestroyCoordinateTransformation(hCT);
        }
        else
        {
            // if we cannot instantiate the transformer, then we
            // will at least attempt to record what we believe the
            // natural coordinate system of the image is.  This is 
            // primarily used by ArcGIS (#3337)

            CPLErrorReset();

            CPLError( CE_Warning, CPLE_AppDefined,
                      "Failed to instantiate coordinate system transformer, likely PROJ.DLL/libproj.so is not available.  Returning image corners as lat/long GCPs as a fallback." );

            char *pszAEQD = NULL;
            oSRS_AEQD.exportToWkt( &(pszAEQD) );
            poDS->SetMetadataItem( "GCPPROJECTIONX", pszAEQD, "IMAGE_STRUCTURE" );
            CPLFree( pszAEQD );
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we have RPCs?                                                */
/* -------------------------------------------------------------------- */
    int            bHasRPC00 = FALSE;
    NITFRPC00BInfo sRPCInfo;
    memset(&sRPCInfo, 0, sizeof(sRPCInfo)); /* To avoid warnings from not clever compilers */

    if( psImage && NITFReadRPC00B( psImage, &sRPCInfo ) && sRPCInfo.SUCCESS )
        bHasRPC00 = TRUE;
        
/* -------------------------------------------------------------------- */
/*      Do we have IGEOLO data that can be treated as a                 */
/*      geotransform?  Our approach should support images in an         */
/*      affine rotated frame of reference.                              */
/* -------------------------------------------------------------------- */
    int nGCPCount = 0;
    GDAL_GCP    *psGCPs = NULL;

    if( psImage && !poDS->bGotGeoTransform && psImage->chICORDS != ' ' )
    {
        nGCPCount = 4;

        psGCPs = (GDAL_GCP *) CPLMalloc(sizeof(GDAL_GCP) * nGCPCount);
        GDALInitGCPs( nGCPCount, psGCPs );

        if( psImage->bIsBoxCenterOfPixel ) 
        {
            psGCPs[0].dfGCPPixel	= 0.5;
            psGCPs[0].dfGCPLine		= 0.5;
            psGCPs[1].dfGCPPixel = poDS->nRasterXSize-0.5;
            psGCPs[1].dfGCPLine = 0.5;
            psGCPs[2].dfGCPPixel = poDS->nRasterXSize-0.5;
            psGCPs[2].dfGCPLine = poDS->nRasterYSize-0.5;
            psGCPs[3].dfGCPPixel = 0.5;
            psGCPs[3].dfGCPLine = poDS->nRasterYSize-0.5;
        }
        else
        {
            psGCPs[0].dfGCPPixel	= 0.0;
            psGCPs[0].dfGCPLine		= 0.0;
            psGCPs[1].dfGCPPixel = poDS->nRasterXSize;
            psGCPs[1].dfGCPLine = 0.0;
            psGCPs[2].dfGCPPixel = poDS->nRasterXSize;
            psGCPs[2].dfGCPLine = poDS->nRasterYSize;
            psGCPs[3].dfGCPPixel = 0.0;
            psGCPs[3].dfGCPLine = poDS->nRasterYSize;
        }

        psGCPs[0].dfGCPX		= psImage->dfULX;
        psGCPs[0].dfGCPY		= psImage->dfULY;

        psGCPs[1].dfGCPX		= psImage->dfURX;
        psGCPs[1].dfGCPY		= psImage->dfURY;

        psGCPs[2].dfGCPX		= psImage->dfLRX;
        psGCPs[2].dfGCPY		= psImage->dfLRY;

        psGCPs[3].dfGCPX		= psImage->dfLLX;
        psGCPs[3].dfGCPY		= psImage->dfLLY;

/* -------------------------------------------------------------------- */
/*      ESRI desires to use the RPCs to produce a denser and more       */
/*      accurate set of GCPs in this case.  Details are unclear at      */
/*      this time.                                                      */
/* -------------------------------------------------------------------- */
#ifdef ESRI_BUILD
        if( bHasRPC00
            &&  ( (psImage->chICORDS == 'G') || (psImage->chICORDS == 'C') ) )
        {
            if( nGCPCount == 4 )
                NITFDensifyGCPs( &psGCPs, &nGCPCount );

            NITFUpdateGCPsWithRPC( &sRPCInfo, psGCPs, &nGCPCount );
        }
#endif /* def ESRI_BUILD */
    }

/* -------------------------------------------------------------------- */
/*      Convert the GCPs into a geotransform definition, if possible.   */
/* -------------------------------------------------------------------- */
    if( !psImage )
    {
        /* nothing */
    }
    else if( poDS->bGotGeoTransform == FALSE 
             && nGCPCount > 0 
             && GDALGCPsToGeoTransform( nGCPCount, psGCPs, 
                                        poDS->adfGeoTransform, FALSE ) )
    {	
        poDS->bGotGeoTransform = TRUE;
    } 

/* -------------------------------------------------------------------- */
/*      If we have IGEOLO that isn't north up, return it as GCPs.       */
/* -------------------------------------------------------------------- */
    else if( (psImage->dfULX != 0 || psImage->dfURX != 0 
              || psImage->dfLRX != 0 || psImage->dfLLX != 0)
             && psImage->chICORDS != ' ' && 
             ( poDS->bGotGeoTransform == FALSE ) &&
             nGCPCount >= 4 )
    {
        CPLDebug( "GDAL", 
                  "NITFDataset::Open() wasn't able to derive a first order\n"
                  "geotransform.  It will be returned as GCPs.");

        poDS->nGCPCount = nGCPCount;
        poDS->pasGCPList = psGCPs;

        psGCPs = NULL;
        nGCPCount = 0;

        CPLFree( poDS->pasGCPList[0].pszId );
        poDS->pasGCPList[0].pszId = CPLStrdup( "UpperLeft" );

        CPLFree( poDS->pasGCPList[1].pszId );
        poDS->pasGCPList[1].pszId = CPLStrdup( "UpperRight" );

        CPLFree( poDS->pasGCPList[2].pszId );
        poDS->pasGCPList[2].pszId = CPLStrdup( "LowerRight" );

        CPLFree( poDS->pasGCPList[3].pszId );
        poDS->pasGCPList[3].pszId = CPLStrdup( "LowerLeft" );

        poDS->pszGCPProjection = CPLStrdup( poDS->pszProjection );
    }

    // This cleans up the original copy of the GCPs used to test if 
    // this IGEOLO could be used for a geotransform if we did not
    // steal the to use as primary gcps.
    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, psGCPs );
        CPLFree( psGCPs );
    }

/* -------------------------------------------------------------------- */
/*      Do we have PRJPSB and MAPLOB TREs to get better                 */
/*      georeferencing from?                                            */
/* -------------------------------------------------------------------- */
    if (psImage)
        poDS->CheckGeoSDEInfo();

/* -------------------------------------------------------------------- */
/*      Do we have metadata.                                            */
/* -------------------------------------------------------------------- */
    char **papszMergedMD;
    char **papszTRE_MD;

    // File and Image level metadata.
    papszMergedMD = CSLDuplicate( poDS->psFile->papszMetadata );

    if( psImage )
    {
        papszMergedMD = CSLInsertStrings( papszMergedMD, 
                                          CSLCount( papszMergedMD ),
                                          psImage->papszMetadata );

        // Comments.
        if( psImage->pszComments != NULL && strlen(psImage->pszComments) != 0 )
            papszMergedMD = CSLSetNameValue( 
                papszMergedMD, "NITF_IMAGE_COMMENTS", psImage->pszComments );
        
        // Compression code. 
        papszMergedMD = CSLSetNameValue( papszMergedMD, "NITF_IC", 
                                         psImage->szIC );
        
        // IMODE
        char szIMODE[2];
        szIMODE[0] = psImage->chIMODE;
        szIMODE[1] = '\0';
        papszMergedMD = CSLSetNameValue( papszMergedMD, "NITF_IMODE", szIMODE );

        // ILOC/Attachment info
        if( psImage->nIDLVL != 0 )
        {
            NITFSegmentInfo *psSegInfo 
                = psFile->pasSegmentInfo + psImage->iSegment;

            papszMergedMD = 
                CSLSetNameValue( papszMergedMD, "NITF_IDLVL", 
                                 CPLString().Printf("%d",psImage->nIDLVL) );
            papszMergedMD = 
                CSLSetNameValue( papszMergedMD, "NITF_IALVL", 
                                 CPLString().Printf("%d",psImage->nIALVL) );
            papszMergedMD = 
                CSLSetNameValue( papszMergedMD, "NITF_ILOC_ROW", 
                                 CPLString().Printf("%d",psImage->nILOCRow) );
            papszMergedMD = 
                CSLSetNameValue( papszMergedMD, "NITF_ILOC_COLUMN", 
                                 CPLString().Printf("%d",psImage->nILOCColumn));
            papszMergedMD = 
                CSLSetNameValue( papszMergedMD, "NITF_CCS_ROW", 
                                 CPLString().Printf("%d",psSegInfo->nCCS_R) );
            papszMergedMD = 
                CSLSetNameValue( papszMergedMD, "NITF_CCS_COLUMN", 
                                 CPLString().Printf("%d", psSegInfo->nCCS_C));
            papszMergedMD = 
                CSLSetNameValue( papszMergedMD, "NITF_IMAG", 
                                 psImage->szIMAG );
        }

        papszMergedMD = NITFGenericMetadataRead(papszMergedMD, psFile, psImage, NULL);

        // BLOCKA 
        papszTRE_MD = NITFReadBLOCKA( psImage );
        if( papszTRE_MD != NULL )
        {
            papszMergedMD = CSLInsertStrings( papszMergedMD, 
                                              CSLCount( papszTRE_MD ),
                                              papszTRE_MD );
            CSLDestroy( papszTRE_MD );
        }
    }
        
#ifdef ESRI_BUILD
    // Extract ESRI generic metadata.
    char **papszESRI_MD = ExtractEsriMD( papszMergedMD );
    if( papszESRI_MD != NULL )
    {
        papszMergedMD = CSLInsertStrings( papszMergedMD, 
                                          CSLCount( papszESRI_MD ),
                                          papszESRI_MD );
        CSLDestroy( papszESRI_MD );
    }
#endif

    poDS->SetMetadata( papszMergedMD );
    CSLDestroy( papszMergedMD );

/* -------------------------------------------------------------------- */
/*      Image structure metadata.                                       */
/* -------------------------------------------------------------------- */
    if( psImage == NULL )
        /* do nothing */;
    else if( psImage->szIC[1] == '1' )
        poDS->SetMetadataItem( "COMPRESSION", "BILEVEL", 
                               "IMAGE_STRUCTURE" );
    else if( psImage->szIC[1] == '2' )
        poDS->SetMetadataItem( "COMPRESSION", "ARIDPCM", 
                               "IMAGE_STRUCTURE" );
    else if( psImage->szIC[1] == '3' )
        poDS->SetMetadataItem( "COMPRESSION", "JPEG", 
                               "IMAGE_STRUCTURE" );
    else if( psImage->szIC[1] == '4' )
        poDS->SetMetadataItem( "COMPRESSION", "VECTOR QUANTIZATION", 
                               "IMAGE_STRUCTURE" );
    else if( psImage->szIC[1] == '5' )
        poDS->SetMetadataItem( "COMPRESSION", "LOSSLESS JPEG", 
                               "IMAGE_STRUCTURE" );
    else if( psImage->szIC[1] == '8' )
        poDS->SetMetadataItem( "COMPRESSION", "JPEG2000", 
                               "IMAGE_STRUCTURE" );
    
/* -------------------------------------------------------------------- */
/*      Do we have RPC info.                                            */
/* -------------------------------------------------------------------- */
    if( psImage && bHasRPC00 )
    {
        char szValue[1280];
        int  i;

        sprintf( szValue, "%.16g", sRPCInfo.LINE_OFF );
        poDS->SetMetadataItem( "LINE_OFF", szValue, "RPC" );

        sprintf( szValue, "%.16g", sRPCInfo.LINE_SCALE );
        poDS->SetMetadataItem( "LINE_SCALE", szValue, "RPC" );

        sprintf( szValue, "%.16g", sRPCInfo.SAMP_OFF );
        poDS->SetMetadataItem( "SAMP_OFF", szValue, "RPC" );

        sprintf( szValue, "%.16g", sRPCInfo.SAMP_SCALE );
        poDS->SetMetadataItem( "SAMP_SCALE", szValue, "RPC" );

        sprintf( szValue, "%.16g", sRPCInfo.LONG_OFF );
        poDS->SetMetadataItem( "LONG_OFF", szValue, "RPC" );

        sprintf( szValue, "%.16g", sRPCInfo.LONG_SCALE );
        poDS->SetMetadataItem( "LONG_SCALE", szValue, "RPC" );

        sprintf( szValue, "%.16g", sRPCInfo.LAT_OFF );
        poDS->SetMetadataItem( "LAT_OFF", szValue, "RPC" );

        sprintf( szValue, "%.16g", sRPCInfo.LAT_SCALE );
        poDS->SetMetadataItem( "LAT_SCALE", szValue, "RPC" );

        sprintf( szValue, "%.16g", sRPCInfo.HEIGHT_OFF );
        poDS->SetMetadataItem( "HEIGHT_OFF", szValue, "RPC" );

        sprintf( szValue, "%.16g", sRPCInfo.HEIGHT_SCALE );
        poDS->SetMetadataItem( "HEIGHT_SCALE", szValue, "RPC" );

        szValue[0] = '\0'; 
        for( i = 0; i < 20; i++ )
            sprintf( szValue+strlen(szValue), "%.16g ",  
                     sRPCInfo.LINE_NUM_COEFF[i] );
        poDS->SetMetadataItem( "LINE_NUM_COEFF", szValue, "RPC" );

        szValue[0] = '\0'; 
        for( i = 0; i < 20; i++ )
            sprintf( szValue+strlen(szValue), "%.16g ",  
                     sRPCInfo.LINE_DEN_COEFF[i] );
        poDS->SetMetadataItem( "LINE_DEN_COEFF", szValue, "RPC" );
        
        szValue[0] = '\0'; 
        for( i = 0; i < 20; i++ )
            sprintf( szValue+strlen(szValue), "%.16g ",  
                     sRPCInfo.SAMP_NUM_COEFF[i] );
        poDS->SetMetadataItem( "SAMP_NUM_COEFF", szValue, "RPC" );
        
        szValue[0] = '\0'; 
        for( i = 0; i < 20; i++ )
            sprintf( szValue+strlen(szValue), "%.16g ",  
                     sRPCInfo.SAMP_DEN_COEFF[i] );
        poDS->SetMetadataItem( "SAMP_DEN_COEFF", szValue, "RPC" );

        sprintf( szValue, "%.16g", 
                 sRPCInfo.LONG_OFF - ( sRPCInfo.LONG_SCALE / 2.0 ) );
        poDS->SetMetadataItem( "MIN_LONG", szValue, "RPC" );

        sprintf( szValue, "%.16g",
                 sRPCInfo.LONG_OFF + ( sRPCInfo.LONG_SCALE / 2.0 ) );
        poDS->SetMetadataItem( "MAX_LONG", szValue, "RPC" );

        sprintf( szValue, "%.16g", 
                 sRPCInfo.LAT_OFF - ( sRPCInfo.LAT_SCALE / 2.0 ) );
        poDS->SetMetadataItem( "MIN_LAT", szValue, "RPC" );

        sprintf( szValue, "%.16g", 
                 sRPCInfo.LAT_OFF + ( sRPCInfo.LAT_SCALE / 2.0 ) );
        poDS->SetMetadataItem( "MAX_LAT", szValue, "RPC" );
    }

/* -------------------------------------------------------------------- */
/*      Do we have Chip info?                                            */
/* -------------------------------------------------------------------- */
    NITFICHIPBInfo sChipInfo;

    if( psImage
        && NITFReadICHIPB( psImage, &sChipInfo ) && sChipInfo.XFRM_FLAG == 0 )
    {
        char szValue[1280];

        sprintf( szValue, "%.16g", sChipInfo.SCALE_FACTOR );
        poDS->SetMetadataItem( "ICHIP_SCALE_FACTOR", szValue );

        sprintf( szValue, "%d", sChipInfo.ANAMORPH_CORR );
        poDS->SetMetadataItem( "ICHIP_ANAMORPH_CORR", szValue );

        sprintf( szValue, "%d", sChipInfo.SCANBLK_NUM );
        poDS->SetMetadataItem( "ICHIP_SCANBLK_NUM", szValue );

        sprintf( szValue, "%.16g", sChipInfo.OP_ROW_11 );
        poDS->SetMetadataItem( "ICHIP_OP_ROW_11", szValue );

        sprintf( szValue, "%.16g", sChipInfo.OP_COL_11 );
        poDS->SetMetadataItem( "ICHIP_OP_COL_11", szValue );

        sprintf( szValue, "%.16g", sChipInfo.OP_ROW_12 );
        poDS->SetMetadataItem( "ICHIP_OP_ROW_12", szValue );

        sprintf( szValue, "%.16g", sChipInfo.OP_COL_12 );
        poDS->SetMetadataItem( "ICHIP_OP_COL_12", szValue );

        sprintf( szValue, "%.16g", sChipInfo.OP_ROW_21 );
        poDS->SetMetadataItem( "ICHIP_OP_ROW_21", szValue );

        sprintf( szValue, "%.16g", sChipInfo.OP_COL_21 );
        poDS->SetMetadataItem( "ICHIP_OP_COL_21", szValue );

        sprintf( szValue, "%.16g", sChipInfo.OP_ROW_22 );
        poDS->SetMetadataItem( "ICHIP_OP_ROW_22", szValue );

        sprintf( szValue, "%.16g", sChipInfo.OP_COL_22 );
        poDS->SetMetadataItem( "ICHIP_OP_COL_22", szValue );

        sprintf( szValue, "%.16g", sChipInfo.FI_ROW_11 );
        poDS->SetMetadataItem( "ICHIP_FI_ROW_11", szValue );

        sprintf( szValue, "%.16g", sChipInfo.FI_COL_11 );
        poDS->SetMetadataItem( "ICHIP_FI_COL_11", szValue );

        sprintf( szValue, "%.16g", sChipInfo.FI_ROW_12 );
        poDS->SetMetadataItem( "ICHIP_FI_ROW_12", szValue );

        sprintf( szValue, "%.16g", sChipInfo.FI_COL_12 );
        poDS->SetMetadataItem( "ICHIP_FI_COL_12", szValue );

        sprintf( szValue, "%.16g", sChipInfo.FI_ROW_21 );
        poDS->SetMetadataItem( "ICHIP_FI_ROW_21", szValue );

        sprintf( szValue, "%.16g", sChipInfo.FI_COL_21 );
        poDS->SetMetadataItem( "ICHIP_FI_COL_21", szValue );

        sprintf( szValue, "%.16g", sChipInfo.FI_ROW_22 );
        poDS->SetMetadataItem( "ICHIP_FI_ROW_22", szValue );

        sprintf( szValue, "%.16g", sChipInfo.FI_COL_22 );
        poDS->SetMetadataItem( "ICHIP_FI_COL_22", szValue );

        sprintf( szValue, "%d", sChipInfo.FI_ROW );
        poDS->SetMetadataItem( "ICHIP_FI_ROW", szValue );

        sprintf( szValue, "%d", sChipInfo.FI_COL );
        poDS->SetMetadataItem( "ICHIP_FI_COL", szValue );

    }
    
    const NITFSeries* series = NITFGetSeriesInfo(pszFilename);
    if (series)
    {
        poDS->SetMetadataItem("NITF_SERIES_ABBREVIATION",
                              (series->abbreviation) ? series->abbreviation : "Unknown");
        poDS->SetMetadataItem("NITF_SERIES_NAME",
                              (series->name) ? series->name : "Unknown");
    }

/* -------------------------------------------------------------------- */
/*      If there are multiple image segments, and we are the zeroth,    */
/*      then setup the subdataset metadata.                             */
/* -------------------------------------------------------------------- */
    int nSubDSCount = 0;

    if( nIMIndex == -1 )
    {
        char **papszSubdatasets = NULL;
        int nIMCounter = 0;

        for( iSegment = 0; iSegment < psFile->nSegmentCount; iSegment++ )
        {
            if( EQUAL(psFile->pasSegmentInfo[iSegment].szSegmentType,"IM") )
            {
                CPLString oName;
                CPLString oValue;

                oName.Printf( "SUBDATASET_%d_NAME", nIMCounter+1 );
                oValue.Printf( "NITF_IM:%d:%s", nIMCounter, pszFilename );
                papszSubdatasets = CSLSetNameValue( papszSubdatasets, 
                                                    oName, oValue );

                oName.Printf( "SUBDATASET_%d_DESC", nIMCounter+1 );
                oValue.Printf( "Image %d of %s", nIMCounter+1, pszFilename );
                papszSubdatasets = CSLSetNameValue( papszSubdatasets, 
                                                    oName, oValue );

                nIMCounter++;
            }
        }

        nSubDSCount = CSLCount(papszSubdatasets) / 2;
        if( nSubDSCount > 1 )
            poDS->GDALMajorObject::SetMetadata( papszSubdatasets, 
                                                "SUBDATASETS" );
        
        CSLDestroy( papszSubdatasets );
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    
    if( nSubDSCount > 1 || nIMIndex != -1 )
    {
        if( nIMIndex == -1 )
            nIMIndex = 0;

        poDS->SetSubdatasetName( CPLString().Printf("%d",nIMIndex) );
        poDS->SetPhysicalFilename( pszFilename );
    }

    poDS->bInLoadXML = TRUE;
    poDS->TryLoadXML();
    poDS->bInLoadXML = FALSE;

/* -------------------------------------------------------------------- */
/*      Do we have a special overview file?  If not, do we have         */
/*      RSets that should be treated as an overview file?               */
/* -------------------------------------------------------------------- */
    const char *pszOverviewFile = 
        poDS->GetMetadataItem( "OVERVIEW_FILE", "OVERVIEWS" );

    if( pszOverviewFile == NULL )
    {
        if( poDS->CheckForRSets(pszFilename) )
            pszOverviewFile = poDS->osRSetVRT;
    }        

/* -------------------------------------------------------------------- */
/*      If we have jpeg or jpeg2000 bands we may need to set the        */
/*      overview file on their dataset. (#3276)                         */
/* -------------------------------------------------------------------- */
    GDALDataset *poSubDS = poDS->poJ2KDataset;
    if( poDS->poJPEGDataset )
        poSubDS = poDS->poJPEGDataset;

    if( poSubDS && pszOverviewFile != NULL )
    {
        poSubDS->SetMetadataItem( "OVERVIEW_FILE", 
                                  pszOverviewFile,
                                  "OVERVIEWS" );
    }

/* -------------------------------------------------------------------- */
/*      If we have jpeg, or jpeg2000 bands we may need to clear         */
/*      their PAM dirty flag too.                                       */
/* -------------------------------------------------------------------- */
    if( poDS->poJ2KDataset != NULL )
        poDS->poJ2KDataset->SetPamFlags( 
            poDS->poJ2KDataset->GetPamFlags() & ~GPF_DIRTY );
    if( poDS->poJPEGDataset != NULL )
        poDS->poJPEGDataset->SetPamFlags( 
            poDS->poJPEGDataset->GetPamFlags() & ~GPF_DIRTY );

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    if( !EQUAL(poOpenInfo->pszFilename,pszFilename) )
        poDS->oOvManager.Initialize( poDS, ":::VIRTUAL:::" );
    else
        poDS->oOvManager.Initialize( poDS, pszFilename );

    /* If there are PAM overviews, don't expose the underlying JPEG dataset */
    /* overviews (in case of monoblock C3) */
    if( poDS->GetRasterCount() > 0 && poDS->GetRasterBand(1) != NULL )
        poDS->bExposeUnderlyingJPEGDatasetOverviews =
            ((GDALPamRasterBand*)poDS->GetRasterBand(1))->
                            GDALPamRasterBand::GetOverviewCount() == 0;

    return( poDS );
}

/************************************************************************/
/*                            LoadDODDatum()                            */
/*                                                                      */
/*      Try to turn a US military datum name into a datum definition.   */
/************************************************************************/

static OGRErr LoadDODDatum( OGRSpatialReference *poSRS,
                            const char *pszDatumName )

{
/* -------------------------------------------------------------------- */
/*      The most common case...                                         */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszDatumName,"WGE ",4) )
    {
        poSRS->SetWellKnownGeogCS( "WGS84" );
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      All the rest we will try and load from gt_datum.csv             */
/*      (Geotrans datum file).                                          */
/* -------------------------------------------------------------------- */
    char szExpanded[6];
    const char *pszGTDatum = CSVFilename( "gt_datum.csv" );

    strncpy( szExpanded, pszDatumName, 3 );
    szExpanded[3] = '\0';
    if( pszDatumName[3] != ' ' )
    {
        int nLen;
        strcat( szExpanded, "-" );
        nLen = strlen(szExpanded);
        szExpanded[nLen] = pszDatumName[3];
        szExpanded[nLen + 1] = '\0';
    }

    CPLString osDName = CSVGetField( pszGTDatum, "CODE", szExpanded, 
                                     CC_ApproxString, "NAME" );
    if( strlen(osDName) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to find datum %s/%s in gt_datum.csv.",
                  pszDatumName, szExpanded );
        return OGRERR_FAILURE;
    }
        
    CPLString osEllipseCode = CSVGetField( pszGTDatum, "CODE", szExpanded, 
                                           CC_ApproxString, "ELLIPSOID" );
    double dfDeltaX = CPLAtof(CSVGetField( pszGTDatum, "CODE", szExpanded, 
                                           CC_ApproxString, "DELTAX" ) );
    double dfDeltaY = CPLAtof(CSVGetField( pszGTDatum, "CODE", szExpanded, 
                                           CC_ApproxString, "DELTAY" ) );
    double dfDeltaZ = CPLAtof(CSVGetField( pszGTDatum, "CODE", szExpanded, 
                                           CC_ApproxString, "DELTAZ" ) );

/* -------------------------------------------------------------------- */
/*      Lookup the ellipse code.                                        */
/* -------------------------------------------------------------------- */
    const char *pszGTEllipse = CSVFilename( "gt_ellips.csv" );
    
    CPLString osEName = CSVGetField( pszGTEllipse, "CODE", osEllipseCode,
                                     CC_ApproxString, "NAME" );
    if( strlen(osEName) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to find datum %s in gt_ellips.csv.",
                  osEllipseCode.c_str() );
        return OGRERR_FAILURE;
    }    
    
    double dfA = CPLAtof(CSVGetField( pszGTEllipse, "CODE", osEllipseCode,
                                      CC_ApproxString, "A" ));
    double dfInvF = CPLAtof(CSVGetField( pszGTEllipse, "CODE", osEllipseCode,
                                         CC_ApproxString, "RF" ));

/* -------------------------------------------------------------------- */
/*      Create geographic coordinate system.                            */
/* -------------------------------------------------------------------- */
    poSRS->SetGeogCS( osDName, osDName, osEName, dfA, dfInvF );

    poSRS->SetTOWGS84( dfDeltaX, dfDeltaY, dfDeltaZ );

    return OGRERR_NONE;
}

/************************************************************************/
/*                          CheckGeoSDEInfo()                           */
/*                                                                      */
/*      Check for GeoSDE TREs (GEOPSB/PRJPSB and MAPLOB).  If we        */
/*      have them, use them to override our coordinate system and       */
/*      geotransform info.                                              */
/************************************************************************/

void NITFDataset::CheckGeoSDEInfo()

{
    if( !psImage )
        return;

/* -------------------------------------------------------------------- */
/*      Do we have the required TREs?                                   */
/* -------------------------------------------------------------------- */
    const char *pszGEOPSB , *pszPRJPSB, *pszMAPLOB;
    OGRSpatialReference oSRS;
    char szName[81];
    int nGEOPSBSize, nPRJPSBSize, nMAPLOBSize;

    pszGEOPSB = NITFFindTRE( psFile->pachTRE, psFile->nTREBytes,"GEOPSB",&nGEOPSBSize);
    pszPRJPSB = NITFFindTRE( psFile->pachTRE, psFile->nTREBytes,"PRJPSB",&nPRJPSBSize);
    pszMAPLOB = NITFFindTRE(psImage->pachTRE,psImage->nTREBytes,"MAPLOB",&nMAPLOBSize);

    if( pszGEOPSB == NULL || pszPRJPSB == NULL || pszMAPLOB == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Collect projection parameters.                                  */
/* -------------------------------------------------------------------- */

    char szParm[16];
    if (nPRJPSBSize < 82 + 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot read PRJPSB TRE. Not enough bytes");
        return;
    }
    int nParmCount = atoi(NITFGetField(szParm,pszPRJPSB,82,1));
    int i;
    double adfParm[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    double dfFN;
    double dfFE;
    if (nPRJPSBSize < 83+15*nParmCount+15+15)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot read PRJPSB TRE. Not enough bytes");
        return;
    }
    for( i = 0; i < nParmCount; i++ )
        adfParm[i] = atof(NITFGetField(szParm,pszPRJPSB,83+15*i,15));

    dfFE = atof(NITFGetField(szParm,pszPRJPSB,83+15*nParmCount,15));
    dfFN = atof(NITFGetField(szParm,pszPRJPSB,83+15*nParmCount+15,15));

/* -------------------------------------------------------------------- */
/*      Try to handle the projection.                                   */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszPRJPSB+80,"AC",2) )
        oSRS.SetACEA( adfParm[1], adfParm[2], adfParm[3], adfParm[0], 
                      dfFE, dfFN );

    else if( EQUALN(pszPRJPSB+80,"AK",2) )
        oSRS.SetLAEA( adfParm[1], adfParm[0], dfFE, dfFN );

    else if( EQUALN(pszPRJPSB+80,"AL",2) )
        oSRS.SetAE( adfParm[1], adfParm[0], dfFE, dfFN );

    else if( EQUALN(pszPRJPSB+80,"BF",2) )
        oSRS.SetBonne( adfParm[1], adfParm[0], dfFE, dfFN );

    else if( EQUALN(pszPRJPSB+80,"CP",2) )
        oSRS.SetEquirectangular( adfParm[1], adfParm[0], dfFE, dfFN );

    else if( EQUALN(pszPRJPSB+80,"CS",2) )
        oSRS.SetCS( adfParm[1], adfParm[0], dfFE, dfFN );

    else if( EQUALN(pszPRJPSB+80,"EF",2) )
        oSRS.SetEckertIV( adfParm[0], dfFE, dfFN );

    else if( EQUALN(pszPRJPSB+80,"ED",2) )
        oSRS.SetEckertVI( adfParm[0], dfFE, dfFN );

    else if( EQUALN(pszPRJPSB+80,"GN",2) )
        oSRS.SetGnomonic( adfParm[1], adfParm[0], dfFE, dfFN );

    else if( EQUALN(pszPRJPSB+80,"HX",2) )
        oSRS.SetHOM2PNO( adfParm[1], 
                         adfParm[3], adfParm[2],
                         adfParm[5], adfParm[4],
                         adfParm[0], dfFE, dfFN );

    else if( EQUALN(pszPRJPSB+80,"KA",2) )
        oSRS.SetEC( adfParm[1], adfParm[2], adfParm[3], adfParm[0], 
                    dfFE, dfFN );

    else if( EQUALN(pszPRJPSB+80,"LE",2) )
        oSRS.SetLCC( adfParm[1], adfParm[2], adfParm[3], adfParm[0], 
                     dfFE, dfFN );

    else if( EQUALN(pszPRJPSB+80,"LI",2) )
        oSRS.SetCEA( adfParm[1], adfParm[0], dfFE, dfFN );

    else if( EQUALN(pszPRJPSB+80,"MC",2) )
        oSRS.SetMercator( adfParm[2], adfParm[1], 1.0, dfFE, dfFN );

    else if( EQUALN(pszPRJPSB+80,"MH",2) )
        oSRS.SetMC( 0.0, adfParm[1], dfFE, dfFN );

    else if( EQUALN(pszPRJPSB+80,"MP",2) )
        oSRS.SetMollweide( adfParm[0], dfFE, dfFN );

    else if( EQUALN(pszPRJPSB+80,"NT",2) )
        oSRS.SetNZMG( adfParm[1], adfParm[0], dfFE, dfFN );

    else if( EQUALN(pszPRJPSB+80,"OD",2) )
        oSRS.SetOrthographic( adfParm[1], adfParm[0], dfFE, dfFN );

    else if( EQUALN(pszPRJPSB+80,"PC",2) )
        oSRS.SetPolyconic( adfParm[1], adfParm[0], dfFE, dfFN );

    else if( EQUALN(pszPRJPSB+80,"PG",2) )
        oSRS.SetPS( adfParm[1], adfParm[0], 1.0, dfFE, dfFN );

    else if( EQUALN(pszPRJPSB+80,"RX",2) )
        oSRS.SetRobinson( adfParm[0], dfFE, dfFN );

    else if( EQUALN(pszPRJPSB+80,"SA",2) )
        oSRS.SetSinusoidal( adfParm[0], dfFE, dfFN );

    else if( EQUALN(pszPRJPSB+80,"TC",2) )
        oSRS.SetTM( adfParm[2], adfParm[0], adfParm[1], dfFE, dfFN );

    else if( EQUALN(pszPRJPSB+80,"VA",2) )
        oSRS.SetVDG( adfParm[0], dfFE, dfFN );

    else
        oSRS.SetLocalCS( NITFGetField(szName,pszPRJPSB,0,80) );

/* -------------------------------------------------------------------- */
/*      Try to apply the datum.                                         */
/* -------------------------------------------------------------------- */
    if (nGEOPSBSize < 86 + 4)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot read GEOPSB TRE. Not enough bytes");
        return;
    }
    LoadDODDatum( &oSRS, NITFGetField(szParm,pszGEOPSB,86,4) );

/* -------------------------------------------------------------------- */
/*      Get the geotransform                                            */
/* -------------------------------------------------------------------- */
    double adfGT[6];
    double dfMeterPerUnit = 1.0;

    if (nMAPLOBSize < 28 + 15)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot read MAPLOB TRE. Not enough bytes");
        return;
    }
    
    if( EQUALN(pszMAPLOB+0,"DM ",3) )
        dfMeterPerUnit = 0.1;
    else if( EQUALN(pszMAPLOB+0,"CM ",3) )
        dfMeterPerUnit = 0.01;
    else if( EQUALN(pszMAPLOB+0,"MM ",3) )
        dfMeterPerUnit = 0.001;
    else if( EQUALN(pszMAPLOB+0,"UM ",3) )
        dfMeterPerUnit = 0.000001;
    else if( EQUALN(pszMAPLOB+0,"KM ",3) )
        dfMeterPerUnit = 1000.0;
    else if( EQUALN(pszMAPLOB+0,"M  ",3) )
        dfMeterPerUnit = 1.0;
    else
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "MAPLOB Unit=%3.3s not regonised, geolocation may be wrong.",
                  pszMAPLOB+0 );
    }
    
    adfGT[0] = atof(NITFGetField(szParm,pszMAPLOB,13,15));
    adfGT[1] = atof(NITFGetField(szParm,pszMAPLOB,3,5)) * dfMeterPerUnit;
    adfGT[2] = 0.0;
    adfGT[3] = atof(NITFGetField(szParm,pszMAPLOB,28,15));
    adfGT[4] = 0.0;
    adfGT[5] = -atof(NITFGetField(szParm,pszMAPLOB,8,5)) * dfMeterPerUnit;

/* -------------------------------------------------------------------- */
/*      Apply back to dataset.                                          */
/* -------------------------------------------------------------------- */
    CPLFree( pszProjection );
    pszProjection = NULL;

    oSRS.exportToWkt( &pszProjection );

    memcpy( adfGeoTransform, adfGT, sizeof(double)*6 );
    bGotGeoTransform = TRUE;
}

/************************************************************************/
/*                             AdviseRead()                             */
/************************************************************************/

CPLErr NITFDataset::AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                                int nBufXSize, int nBufYSize, 
                                GDALDataType eDT, 
                                int nBandCount, int *panBandList,
                                char **papszOptions )
    
{
    if( poJ2KDataset == NULL )
        return GDALDataset::AdviseRead( nXOff, nYOff, nXSize, nYSize, 
                                        nBufXSize, nBufYSize, eDT, 
                                        nBandCount, panBandList, 
                                        papszOptions);
    else if( poJPEGDataset != NULL )
        return poJPEGDataset->AdviseRead( nXOff, nYOff, nXSize, nYSize, 
                                          nBufXSize, nBufYSize, eDT, 
                                          nBandCount, panBandList, 
                                          papszOptions);
    else
        return poJ2KDataset->AdviseRead( nXOff, nYOff, nXSize, nYSize, 
                                         nBufXSize, nBufYSize, eDT, 
                                         nBandCount, panBandList, 
                                         papszOptions);
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr NITFDataset::IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType, 
                               int nBandCount, int *panBandMap,
                               int nPixelSpace, int nLineSpace, int nBandSpace)
    
{
    if( poJ2KDataset != NULL )
        return poJ2KDataset->RasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                       pData, nBufXSize, nBufYSize, eBufType,
                                       nBandCount, panBandMap, 
                                       nPixelSpace, nLineSpace, nBandSpace );
    else if( poJPEGDataset != NULL )
        return poJPEGDataset->RasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                        pData, nBufXSize, nBufYSize, eBufType,
                                        nBandCount, panBandMap, 
                                        nPixelSpace, nLineSpace, nBandSpace );
    else 
        return GDALDataset::IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                       pData, nBufXSize, nBufYSize, eBufType,
                                       nBandCount, panBandMap, 
                                       nPixelSpace, nLineSpace, nBandSpace );
}


/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr NITFDataset::GetGeoTransform( double *padfGeoTransform )

{
    memcpy( padfGeoTransform, adfGeoTransform, sizeof(double) * 6 );

    if( bGotGeoTransform )
        return CE_None;
    else
        return GDALPamDataset::GetGeoTransform( padfGeoTransform );
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr NITFDataset::SetGeoTransform( double *padfGeoTransform )

{
    double dfIGEOLOULX, dfIGEOLOULY, dfIGEOLOURX, dfIGEOLOURY, 
           dfIGEOLOLRX, dfIGEOLOLRY, dfIGEOLOLLX, dfIGEOLOLLY;

    bGotGeoTransform = TRUE;
    /* Valgrind would complain because SetGeoTransform() is called */
    /* from SetProjection() with adfGeoTransform as argument */
    if (adfGeoTransform != padfGeoTransform)
        memcpy( adfGeoTransform, padfGeoTransform, sizeof(double) * 6 );

    dfIGEOLOULX = padfGeoTransform[0] + 0.5 * padfGeoTransform[1] 
                                      + 0.5 * padfGeoTransform[2];
    dfIGEOLOULY = padfGeoTransform[3] + 0.5 * padfGeoTransform[4] 
                                      + 0.5 * padfGeoTransform[5];
    dfIGEOLOURX = dfIGEOLOULX + padfGeoTransform[1] * (nRasterXSize - 1);
    dfIGEOLOURY = dfIGEOLOULY + padfGeoTransform[4] * (nRasterXSize - 1);
    dfIGEOLOLRX = dfIGEOLOULX + padfGeoTransform[1] * (nRasterXSize - 1)
                              + padfGeoTransform[2] * (nRasterYSize - 1);
    dfIGEOLOLRY = dfIGEOLOULY + padfGeoTransform[4] * (nRasterXSize - 1)
                              + padfGeoTransform[5] * (nRasterYSize - 1);
    dfIGEOLOLLX = dfIGEOLOULX + padfGeoTransform[2] * (nRasterYSize - 1);
    dfIGEOLOLLY = dfIGEOLOULY + padfGeoTransform[5] * (nRasterYSize - 1);

    if( NITFWriteIGEOLO( psImage, psImage->chICORDS, 
                         psImage->nZone, 
                         dfIGEOLOULX, dfIGEOLOULY, dfIGEOLOURX, dfIGEOLOURY, 
                         dfIGEOLOLRX, dfIGEOLOLRY, dfIGEOLOLLX, dfIGEOLOLLY ) )
        return CE_None;
    else
        return GDALPamDataset::SetGeoTransform( padfGeoTransform );
}

/************************************************************************/
/*                               SetGCPs()                              */
/************************************************************************/

CPLErr NITFDataset::SetGCPs( int nGCPCountIn, const GDAL_GCP *pasGCPListIn,
                              const char *pszGCPProjectionIn )
{
    if( nGCPCountIn != 4 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "NITF only supports writing 4 GCPs.");
        return CE_Failure;
    }
    
    /* Free previous GCPs */
    GDALDeinitGCPs( nGCPCount, pasGCPList );
    CPLFree( pasGCPList );
    
    /* Duplicate in GCPs */
    nGCPCount = nGCPCountIn;
    pasGCPList = GDALDuplicateGCPs(nGCPCount, pasGCPListIn);
    
    CPLFree(pszGCPProjection);
    pszGCPProjection = CPLStrdup(pszGCPProjectionIn);

    int iUL = -1, iUR = -1, iLR = -1, iLL = -1;

#define EPS_GCP 1e-5
    for(int i = 0; i < 4; i++ )
    {
        if (fabs(pasGCPList[i].dfGCPPixel - 0.5) < EPS_GCP &&
            fabs(pasGCPList[i].dfGCPLine - 0.5) < EPS_GCP)
            iUL = i;

        else if (fabs(pasGCPList[i].dfGCPPixel - (nRasterXSize - 0.5)) < EPS_GCP &&
                 fabs(pasGCPList[i].dfGCPLine - 0.5) < EPS_GCP)
            iUR = i;

        else if (fabs(pasGCPList[i].dfGCPPixel - (nRasterXSize - 0.5)) < EPS_GCP &&
                 fabs(pasGCPList[i].dfGCPLine - (nRasterYSize - 0.5)) < EPS_GCP )
            iLR = i;

        else if (fabs(pasGCPList[i].dfGCPPixel - 0.5) < EPS_GCP &&
                 fabs(pasGCPList[i].dfGCPLine - (nRasterYSize - 0.5)) < EPS_GCP)
            iLL = i;
    }

    if (iUL < 0 || iUR < 0 || iLR < 0 || iLL < 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The 4 GCPs image coordinates must be exactly "
                 "at the *center* of the 4 corners of the image "
                 "( (%.1f, %.1f), (%.1f %.1f), (%.1f %.1f), (%.1f %.1f) ).",
                 0.5, 0.5,
                 nRasterYSize - 0.5, 0.5,
                 nRasterXSize - 0.5, nRasterYSize - 0.5,
                 nRasterXSize - 0.5, 0.5);
        return CE_Failure;
    }

    double dfIGEOLOULX = pasGCPList[iUL].dfGCPX;
    double dfIGEOLOULY = pasGCPList[iUL].dfGCPY;
    double dfIGEOLOURX = pasGCPList[iUR].dfGCPX;
    double dfIGEOLOURY = pasGCPList[iUR].dfGCPY;
    double dfIGEOLOLRX = pasGCPList[iLR].dfGCPX;
    double dfIGEOLOLRY = pasGCPList[iLR].dfGCPY;
    double dfIGEOLOLLX = pasGCPList[iLL].dfGCPX;
    double dfIGEOLOLLY = pasGCPList[iLL].dfGCPY;

    /* To recompute the zone */
    char* pszProjectionBack = pszProjection ? CPLStrdup(pszProjection) : NULL;
    CPLErr eErr = SetProjection(pszGCPProjection);
    CPLFree(pszProjection);
    pszProjection = pszProjectionBack;
    
    if (eErr != CE_None)
        return eErr;
    
    if( NITFWriteIGEOLO( psImage, psImage->chICORDS, 
                         psImage->nZone, 
                         dfIGEOLOULX, dfIGEOLOULY, dfIGEOLOURX, dfIGEOLOURY, 
                         dfIGEOLOLRX, dfIGEOLOLRY, dfIGEOLOLLX, dfIGEOLOLLY ) )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *NITFDataset::GetProjectionRef()

{
    if( bGotGeoTransform )
        return pszProjection;
    else
        return GDALPamDataset::GetProjectionRef();
}

/************************************************************************/
/*                            SetProjection()                           */
/************************************************************************/

CPLErr NITFDataset::SetProjection(const char* _pszProjection)

{
    int    bNorth;
    OGRSpatialReference oSRS, oSRS_WGS84;
    char *pszWKT = (char *) _pszProjection;

    if( pszWKT != NULL )
        oSRS.importFromWkt( &pszWKT );
    else
        return CE_Failure;

    oSRS_WGS84.SetWellKnownGeogCS( "WGS84" );
    if ( oSRS.IsSameGeogCS(&oSRS_WGS84) == FALSE)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "NITF only supports WGS84 geographic and UTM projections.\n");
        return CE_Failure;
    }

    if( oSRS.IsGeographic() && oSRS.GetPrimeMeridian() == 0.0)
    {
        if (psImage->chICORDS != 'G' && psImage->chICORDS != 'D')
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "NITF file should have been created with creation option 'ICORDS=G' (or 'ICORDS=D').\n");
            return CE_Failure;
        }
    }
    else if( oSRS.GetUTMZone( &bNorth ) > 0)
    {
        if (bNorth && psImage->chICORDS != 'N')
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "NITF file should have been created with creation option 'ICORDS=N'.\n");
            return CE_Failure;
        }
        else if (!bNorth && psImage->chICORDS != 'S')
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "NITF file should have been created with creation option 'ICORDS=S'.\n");
            return CE_Failure;
        }

        psImage->nZone = oSRS.GetUTMZone( NULL );
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "NITF only supports WGS84 geographic and UTM projections.\n");
        return CE_Failure;
    }

    CPLFree(pszProjection);
    pszProjection = CPLStrdup(_pszProjection);

    if (bGotGeoTransform)
        SetGeoTransform(adfGeoTransform);

    return CE_None;
}

#ifdef ESRI_BUILD
/************************************************************************/
/*                       InitializeNITFDESMetadata()                    */
/************************************************************************/

void NITFDataset::InitializeNITFDESMetadata()
{
    static const char   *pszDESMetadataDomain       = "NITF_DES_METADATA";
    static const char   *pszDESsDomain              = "NITF_DES";
    static const char   *pszMDXmlDataContentDESDATA = "NITF_DES_XML_DATA_CONTENT_DESDATA";
    static const char   *pszXmlDataContent          = "XML_DATA_CONTENT";
    static const int     idxXmlDataContentDESDATA   = 973;
    static const int     sizeXmlDataContent         = (int)strlen(pszXmlDataContent);

    char **ppszDESMetadataList = oSpecialMD.GetMetadata( pszDESMetadataDomain );

    if( ppszDESMetadataList != NULL ) return;

    char **ppszDESsList = this->GetMetadata( pszDESsDomain );

    if( ppszDESsList == NULL ) return;

    bool          foundXmlDataContent = false;
    char         *pachNITFDES         = NULL;

    // Set metadata "NITF_DES_XML_DATA_CONTENT_DESDATA".
    // NOTE: There should only be one instance of XML_DATA_CONTENT DES.

    while( ((pachNITFDES = *ppszDESsList) != NULL) && (!foundXmlDataContent) )
    {
        // The data stream has been Base64 encoded, need to decode it.
        // NOTE: The actual length of the DES data stream is appended at the beginning of the encoded
        //       data and is separated by a space.

        const char* pszSpace = strchr(pachNITFDES, ' ');

        char* pszData = NULL;
        int   nDataLen = 0;
        if( pszSpace )
        {
            pszData = CPLStrdup( pszSpace+1 );
            nDataLen = CPLBase64DecodeInPlace((GByte*)pszData);
            pszData[nDataLen] = 0;
        }

        if ( nDataLen > 2 + sizeXmlDataContent && EQUALN(pszData, "DE", 2) )
        {
            // Check to see if this is a XML_DATA_CONTENT DES.
            if ( EQUALN(pszData + 2, pszXmlDataContent, sizeXmlDataContent) &&
                 nDataLen > idxXmlDataContentDESDATA )
            {
                foundXmlDataContent = true;

                // Get the value of the DESDATA field and set metadata "NITF_DES_XML_DATA_CONTENT_DESDATA".
                const char* pszXML = pszData + idxXmlDataContentDESDATA;

                // Set the metadata.
                oSpecialMD.SetMetadataItem( pszMDXmlDataContentDESDATA, pszXML, pszDESMetadataDomain );
            }
        }

        CPLFree(pszData);

        pachNITFDES   = NULL;
        ppszDESsList += 1;
    }
}


/************************************************************************/
/*                       InitializeNITFDESs()                           */
/************************************************************************/

void NITFDataset::InitializeNITFDESs()
{
    static const char *pszDESsDomain = "NITF_DES";

    char **ppszDESsList = oSpecialMD.GetMetadata( pszDESsDomain );

    if( ppszDESsList != NULL ) return;

/* -------------------------------------------------------------------- */
/*  Go through all the segments and process all DES segments.           */
/* -------------------------------------------------------------------- */

    char               *pachDESData  = NULL;
    int                 nDESDataSize = 0;
    std::string         encodedDESData("");
    CPLStringList       aosList;

    for( int iSegment = 0; iSegment < psFile->nSegmentCount; iSegment++ )
    {
        NITFSegmentInfo *psSegInfo = psFile->pasSegmentInfo + iSegment;

        if( EQUAL(psSegInfo->szSegmentType,"DE") )
        {
            nDESDataSize = psSegInfo->nSegmentHeaderSize + psSegInfo->nSegmentSize;
            pachDESData  = (char*) VSIMalloc( nDESDataSize + 1 );

            if (pachDESData == NULL)
            {
                CPLError( CE_Failure, CPLE_OutOfMemory, "Cannot allocate memory for DES segment" );
                return;
            }

            if( VSIFSeekL( psFile->fp, psSegInfo->nSegmentHeaderStart,
                          SEEK_SET ) != 0
                || (int)VSIFReadL( pachDESData, 1, nDESDataSize,
                             psFile->fp ) != nDESDataSize )
            {
                CPLError( CE_Failure, CPLE_FileIO,
                          "Failed to read %d byte DES subheader from " CPL_FRMT_GUIB ".",
                          nDESDataSize,
                          psSegInfo->nSegmentHeaderStart );
                CPLFree( pachDESData );
                return;
            }

            pachDESData[nDESDataSize] = '\0';

/* -------------------------------------------------------------------- */
/*          Accumulate all the DES segments.                            */
/* -------------------------------------------------------------------- */

            char* pszBase64 = CPLBase64Encode( nDESDataSize, (const GByte *)pachDESData );
            encodedDESData = pszBase64;
            CPLFree(pszBase64);

            CPLFree( pachDESData );
            pachDESData = NULL;

            if( encodedDESData.empty() )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Failed to encode DES subheader data!");
                return;
            }

            // The length of the DES subheader data plus a space is append to the beginning of the encoded
            // string so that we can recover the actual length of the image subheader when we decode it.

            char buffer[20];

            sprintf(buffer, "%d", nDESDataSize);

            std::string desSubheaderStr(buffer);
            desSubheaderStr.append(" ");
            desSubheaderStr.append(encodedDESData);

            aosList.AddString(desSubheaderStr.c_str() );
        }
    }

    if (aosList.size() > 0)
        oSpecialMD.SetMetadata( aosList.List(), pszDESsDomain );
}

/************************************************************************/
/*                       InitializeNITFTREs()                           */
/************************************************************************/

void NITFDataset::InitializeNITFTREs()
{
    static const char *pszFileHeaderTREsDomain   = "NITF_FILE_HEADER_TRES";
    static const char *pszImageSegmentTREsDomain = "NITF_IMAGE_SEGMENT_TRES";

    char **ppszFileHeaderTREsList   = oSpecialMD.GetMetadata( pszFileHeaderTREsDomain );
    char **ppszImageSegmentTREsList = oSpecialMD.GetMetadata( pszImageSegmentTREsDomain );

    if( (ppszFileHeaderTREsList != NULL) && (ppszImageSegmentTREsList != NULL ) ) return;

/* -------------------------------------------------------------------- */
/*      Loop over TRE sources (file and image).                         */
/* -------------------------------------------------------------------- */

    for( int nTRESrc = 0; nTRESrc < 2; nTRESrc++ )
    {
        int                 nTREBytes  = 0;
        char               *pszTREData = NULL;
        const char         *pszTREsDomain = NULL;
        CPLStringList       aosList;

/* -------------------------------------------------------------------- */
/*      Extract file header or image segment TREs.                      */
/* -------------------------------------------------------------------- */

        if( nTRESrc == 0 )
        {
            if( ppszFileHeaderTREsList != NULL ) continue;

            nTREBytes     = psFile->nTREBytes;
            pszTREData    = psFile->pachTRE;
            pszTREsDomain = pszFileHeaderTREsDomain;
        }
        else
        {
            if( ppszImageSegmentTREsList != NULL ) continue;

            if( psImage )
            {
                nTREBytes     = psImage->nTREBytes;
                pszTREData    = psImage->pachTRE;
                pszTREsDomain = pszImageSegmentTREsDomain;
            }
            else
            {
                nTREBytes  = 0;
                pszTREData = NULL;
            }
        }

/* -------------------------------------------------------------------- */
/*      Loop over TREs.                                                 */
/* -------------------------------------------------------------------- */

        while( nTREBytes >= 11 )
        {
            char szTemp[100];
            char szTag[7];
            char *pszEscapedData = NULL;
            int nThisTRESize = atoi(NITFGetField(szTemp, pszTREData, 6, 5 ));

            if (nThisTRESize < 0)
            {
                NITFGetField(szTemp, pszTREData, 0, 6 );
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid size (%d) for TRE %s",
                        nThisTRESize, szTemp);
                return;
            }

            if (nThisTRESize > nTREBytes - 11)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Not enough bytes in TRE");
                return;
            }

            strncpy( szTag, pszTREData, 6 );
            szTag[6] = '\0';

            // trim white off tag.
            while( strlen(szTag) > 0 && szTag[strlen(szTag)-1] == ' ' )
                szTag[strlen(szTag)-1] = '\0';

            // escape data.
            pszEscapedData = CPLEscapeString( pszTREData + 6,
                                              nThisTRESize + 5,
                                              CPLES_BackslashQuotable );

            char * pszLine = (char *) CPLMalloc( strlen(szTag)+strlen(pszEscapedData)+2 );
            sprintf( pszLine, "%s=%s", szTag, pszEscapedData );
            aosList.AddString(pszLine);
            CPLFree(pszLine);
            pszLine        = NULL;

            CPLFree( pszEscapedData );
            pszEscapedData = NULL;

            nTREBytes  -= (nThisTRESize + 11);
            pszTREData += (nThisTRESize + 11);
        }

        if (aosList.size() > 0)
            oSpecialMD.SetMetadata( aosList.List(), pszTREsDomain );
    }
}
#endif

/************************************************************************/
/*                       InitializeNITFMetadata()                        */
/************************************************************************/

void NITFDataset::InitializeNITFMetadata()

{
    static const char *pszDomainName            = "NITF_METADATA";
    static const char *pszTagNITFFileHeader     = "NITFFileHeader";
    static const char *pszTagNITFImageSubheader = "NITFImageSubheader";

    if( oSpecialMD.GetMetadata( pszDomainName ) != NULL )
        return;

    // nHeaderLenOffset is the number of bytes to skip from the beginning of the NITF file header
    // in order to get to the field HL (NITF file header length).

    int nHeaderLen       = 0;
    int nHeaderLenOffset = 0;

    // Get the NITF file header length.

    if( psFile->pachHeader != NULL )
    {
        if ( (strncmp(psFile->pachHeader, "NITF02.10", 9) == 0) || (strncmp(psFile->pachHeader, "NSIF01.00", 9) == 0) )
            nHeaderLenOffset = 354;
        else if ( (strncmp(psFile->pachHeader, "NITF01.10", 9) == 0) || (strncmp(psFile->pachHeader, "NITF02.00", 9) == 0) )
            nHeaderLenOffset = ( strncmp((psFile->pachHeader+280), "999998", 6 ) == 0 ) ? 394 : 354;
    }

    char fieldHL[7];

    if( nHeaderLenOffset > 0 )
    {
        char *pszFieldHL = psFile->pachHeader + nHeaderLenOffset;

        memcpy(fieldHL, pszFieldHL, 6);
        fieldHL[6] = '\0';
        nHeaderLen = atoi(fieldHL);
    }

    if( nHeaderLen <= 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Zero length NITF file header!");
        return;
    }

    char *encodedHeader = CPLBase64Encode(nHeaderLen, 
                                          (GByte*)psFile->pachHeader);

    if (encodedHeader == NULL || strlen(encodedHeader) == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, 
                 "Failed to encode NITF file header!");
        return;
    }

    // The length of the NITF file header plus a space is append to the beginning of the encoded string so
    // that we can recover the length of the NITF file header when we decode it without having to pull it
    // out the HL field again.

    std::string nitfFileheaderStr(fieldHL);
    nitfFileheaderStr.append(" ");
    nitfFileheaderStr.append(encodedHeader);

    CPLFree( encodedHeader );

    oSpecialMD.SetMetadataItem( pszTagNITFFileHeader, nitfFileheaderStr.c_str(), pszDomainName );

    // Get the image subheader length.

    int nImageSubheaderLen = 0;
    
    for( int i = 0; i < psFile->nSegmentCount; ++i )
    {
        if (strncmp(psFile->pasSegmentInfo[i].szSegmentType, "IM", 2) == 0)
        {
            nImageSubheaderLen = psFile->pasSegmentInfo[i].nSegmentHeaderSize;
            break;
        }
    }

    if( nImageSubheaderLen < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid length NITF image subheader!");
        return;
    }

    if( nImageSubheaderLen > 0 )
    {
        char *encodedImageSubheader = CPLBase64Encode(nImageSubheaderLen,(GByte*) psImage->pachHeader);
    
        if( encodedImageSubheader == NULL || strlen(encodedImageSubheader) ==0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, 
                     "Failed to encode image subheader!");
            return;
        }

        // The length of the image subheader plus a space is append to the beginning of the encoded string so
        // that we can recover the actual length of the image subheader when we decode it.
      
        char buffer[20];

        sprintf(buffer, "%d", nImageSubheaderLen);

        std::string imageSubheaderStr(buffer);
        imageSubheaderStr.append(" ");
        imageSubheaderStr.append(encodedImageSubheader);

        CPLFree( encodedImageSubheader );

        oSpecialMD.SetMetadataItem( pszTagNITFImageSubheader, imageSubheaderStr.c_str(), pszDomainName );
    }
}

/************************************************************************/
/*                       InitializeCGMMetadata()                        */
/************************************************************************/

void NITFDataset::InitializeCGMMetadata()

{
    if( oSpecialMD.GetMetadataItem( "SEGMENT_COUNT", "CGM" ) != NULL )
        return;

    int iSegment;
    int iCGM = 0;
    char **papszCGMMetadata = NULL;

    papszCGMMetadata = 
        CSLSetNameValue( papszCGMMetadata, "SEGMENT_COUNT", "0" );

/* ==================================================================== */
/*      Process all graphics segments.                                  */
/* ==================================================================== */
    for( iSegment = 0; iSegment < psFile->nSegmentCount; iSegment++ )
    {
        NITFSegmentInfo *psSegment = psFile->pasSegmentInfo + iSegment;

        if( !EQUAL(psSegment->szSegmentType,"GR") 
            && !EQUAL(psSegment->szSegmentType,"SY") )
            continue;

        papszCGMMetadata = 
            CSLSetNameValue( papszCGMMetadata, 
                             CPLString().Printf("SEGMENT_%d_SLOC_ROW", iCGM), 
                             CPLString().Printf("%d",psSegment->nLOC_R) );
        papszCGMMetadata = 
            CSLSetNameValue( papszCGMMetadata, 
                             CPLString().Printf("SEGMENT_%d_SLOC_COL", iCGM), 
                             CPLString().Printf("%d",psSegment->nLOC_C) );

        papszCGMMetadata = 
            CSLSetNameValue( papszCGMMetadata, 
                             CPLString().Printf("SEGMENT_%d_CCS_ROW", iCGM), 
                             CPLString().Printf("%d",psSegment->nCCS_R) );
        papszCGMMetadata = 
            CSLSetNameValue( papszCGMMetadata, 
                             CPLString().Printf("SEGMENT_%d_CCS_COL", iCGM), 
                             CPLString().Printf("%d",psSegment->nCCS_C) );

        papszCGMMetadata = 
            CSLSetNameValue( papszCGMMetadata, 
                             CPLString().Printf("SEGMENT_%d_SDLVL", iCGM), 
                             CPLString().Printf("%d",psSegment->nDLVL) );
        papszCGMMetadata = 
            CSLSetNameValue( papszCGMMetadata, 
                             CPLString().Printf("SEGMENT_%d_SALVL", iCGM), 
                             CPLString().Printf("%d",psSegment->nALVL) );

/* -------------------------------------------------------------------- */
/*      Load the raw CGM data itself.                                   */
/* -------------------------------------------------------------------- */
        char *pabyCGMData, *pszEscapedCGMData;

        pabyCGMData = (char *) VSICalloc(1,(size_t)psSegment->nSegmentSize);
        if (pabyCGMData == NULL)
        {
            CPLError( CE_Failure, CPLE_OutOfMemory, "Out of memory");
            CSLDestroy( papszCGMMetadata );
            return;
        }
        if( VSIFSeekL( psFile->fp, psSegment->nSegmentStart, 
                       SEEK_SET ) != 0 
            || VSIFReadL( pabyCGMData, 1, (size_t)psSegment->nSegmentSize, 
                          psFile->fp ) != psSegment->nSegmentSize )
        {
            CPLError( CE_Warning, CPLE_FileIO, 
                      "Failed to read " CPL_FRMT_GUIB " bytes of graphic data at " CPL_FRMT_GUIB ".", 
                      psSegment->nSegmentSize,
                      psSegment->nSegmentStart );
            CPLFree(pabyCGMData);
            CSLDestroy( papszCGMMetadata );
            return;
        }

        pszEscapedCGMData = CPLEscapeString( pabyCGMData, 
                                             (int)psSegment->nSegmentSize, 
                                             CPLES_BackslashQuotable );
        if (pszEscapedCGMData == NULL)
        {
            CPLError( CE_Failure, CPLE_OutOfMemory, "Out of memory");
            CPLFree(pabyCGMData);
            CSLDestroy( papszCGMMetadata );
            return;
        }

        papszCGMMetadata = 
            CSLSetNameValue( papszCGMMetadata, 
                             CPLString().Printf("SEGMENT_%d_DATA", iCGM), 
                             pszEscapedCGMData );
        CPLFree( pszEscapedCGMData );
        CPLFree( pabyCGMData );

        iCGM++;
    }

/* -------------------------------------------------------------------- */
/*      Record the CGM segment count.                                   */
/* -------------------------------------------------------------------- */
    papszCGMMetadata = 
        CSLSetNameValue( papszCGMMetadata, 
                         "SEGMENT_COUNT", 
                         CPLString().Printf( "%d", iCGM ) );

    oSpecialMD.SetMetadata( papszCGMMetadata, "CGM" );

    CSLDestroy( papszCGMMetadata );
}

/************************************************************************/
/*                       InitializeTextMetadata()                       */
/************************************************************************/

void NITFDataset::InitializeTextMetadata()

{
    if( oSpecialMD.GetMetadata( "TEXT" ) != NULL )
        return;

    int iSegment;
    int iText = 0;

/* ==================================================================== */
/*      Process all text segments.                                  */
/* ==================================================================== */
    for( iSegment = 0; iSegment < psFile->nSegmentCount; iSegment++ )
    {
        NITFSegmentInfo *psSegment = psFile->pasSegmentInfo + iSegment;

        if( !EQUAL(psSegment->szSegmentType,"TX") )
            continue;

/* -------------------------------------------------------------------- */
/*      Load the text header                                            */
/* -------------------------------------------------------------------- */

        /* Allocate one extra byte for the NULL terminating character */
        char *pabyHeaderData = (char *) CPLCalloc(1,
                (size_t) psSegment->nSegmentHeaderSize + 1);
        if (VSIFSeekL(psFile->fp, psSegment->nSegmentHeaderStart,
                      SEEK_SET) != 0 ||
            VSIFReadL(pabyHeaderData, 1, (size_t) psSegment->nSegmentHeaderSize,
                      psFile->fp) != psSegment->nSegmentHeaderSize)
        {
            CPLError( CE_Warning, CPLE_FileIO,
                      "Failed to read %d bytes of text header data at " CPL_FRMT_GUIB ".",
                      psSegment->nSegmentHeaderSize,
                      psSegment->nSegmentHeaderStart);
            CPLFree(pabyHeaderData);
            return;
        }

        oSpecialMD.SetMetadataItem( CPLString().Printf("HEADER_%d", iText),
                                    pabyHeaderData, "TEXT");
        CPLFree(pabyHeaderData);

/* -------------------------------------------------------------------- */
/*      Load the raw TEXT data itself.                                  */
/* -------------------------------------------------------------------- */
        char *pabyTextData;

        /* Allocate one extra byte for the NULL terminating character */
        pabyTextData = (char *) VSICalloc(1,(size_t)psSegment->nSegmentSize+1);
        if (pabyTextData == NULL)
        {
            CPLError( CE_Failure, CPLE_OutOfMemory, "Out of memory");
            return;
        }
        if( VSIFSeekL( psFile->fp, psSegment->nSegmentStart, 
                       SEEK_SET ) != 0 
            || VSIFReadL( pabyTextData, 1, (size_t)psSegment->nSegmentSize, 
                          psFile->fp ) != psSegment->nSegmentSize )
        {
            CPLError( CE_Warning, CPLE_FileIO, 
                      "Failed to read " CPL_FRMT_GUIB " bytes of text data at " CPL_FRMT_GUIB ".", 
                      psSegment->nSegmentSize,
                      psSegment->nSegmentStart );
            CPLFree( pabyTextData );
            return;
        }

        oSpecialMD.SetMetadataItem( CPLString().Printf( "DATA_%d", iText),
                                    pabyTextData, "TEXT" );
        CPLFree( pabyTextData );

        iText++;
    }
}

/************************************************************************/
/*                       InitializeTREMetadata()                        */
/************************************************************************/

void NITFDataset::InitializeTREMetadata()

{
    if( oSpecialMD.GetMetadata( "TRE" ) != NULL )
        return;

    CPLXMLNode* psTresNode = CPLCreateXMLNode(NULL, CXT_Element, "tres");

/* -------------------------------------------------------------------- */
/*      Loop over TRE sources (file and image).                         */
/* -------------------------------------------------------------------- */
    int nTRESrc;

    for( nTRESrc = 0; nTRESrc < 2; nTRESrc++ )
    {
        int nTREBytes;
        char *pszTREData;

        if( nTRESrc == 0 )
        {
            nTREBytes = psFile->nTREBytes;
            pszTREData = psFile->pachTRE;
        }
        else
        {
            if( psImage ) 
            {
                nTREBytes = psImage->nTREBytes;
                pszTREData = psImage->pachTRE;
            }
            else
            {
                nTREBytes = 0;
                pszTREData = NULL;
            }
        }

/* -------------------------------------------------------------------- */
/*      Loop over TREs.                                                 */
/* -------------------------------------------------------------------- */

        while( nTREBytes >= 11 )
        {
            char szTemp[100];
            char szTag[7];
            char *pszEscapedData;
            int nThisTRESize = atoi(NITFGetField(szTemp, pszTREData, 6, 5 ));

            if (nThisTRESize < 0)
            {
                NITFGetField(szTemp, pszTREData, 0, 6 );
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid size (%d) for TRE %s",
                        nThisTRESize, szTemp);
                return;
            }
            if (nThisTRESize > nTREBytes - 11)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Not enough bytes in TRE");
                return;
            }

            strncpy( szTag, pszTREData, 6 );
            szTag[6] = '\0';

            // trim white off tag. 
            while( strlen(szTag) > 0 && szTag[strlen(szTag)-1] == ' ' )
                szTag[strlen(szTag)-1] = '\0';

            CPLXMLNode* psTreNode = NITFCreateXMLTre(psFile, szTag, pszTREData + 11,nThisTRESize);
            if (psTreNode)
            {
                CPLCreateXMLNode(CPLCreateXMLNode(psTreNode, CXT_Attribute, "location"),
                                 CXT_Text, nTRESrc == 0 ? "file" : "image");
                CPLAddXMLChild(psTresNode, psTreNode);
            }

            // escape data. 
            pszEscapedData = CPLEscapeString( pszTREData + 11,
                                              nThisTRESize,
                                              CPLES_BackslashQuotable );
            if (pszEscapedData == NULL)
            {
                CPLError( CE_Failure, CPLE_OutOfMemory, "Out of memory");
                return;
            }

            char szUniqueTag[32];
            strcpy(szUniqueTag, szTag);
            int nCountUnique = 2;
            while(oSpecialMD.GetMetadataItem( szUniqueTag, "TRE") != NULL)
            {
                sprintf(szUniqueTag, "%s_%d", szTag, nCountUnique);
                nCountUnique ++;
            }
            oSpecialMD.SetMetadataItem( szUniqueTag, pszEscapedData, "TRE" );
            CPLFree( pszEscapedData );
            
            nTREBytes -= (nThisTRESize + 11);
            pszTREData += (nThisTRESize + 11);
        }
    }

/* -------------------------------------------------------------------- */
/*      Loop over TRE in DES                                            */
/* -------------------------------------------------------------------- */
    int iSegment;
    for( iSegment = 0; iSegment < psFile->nSegmentCount; iSegment++ )
    {
        NITFSegmentInfo *psSegInfo = psFile->pasSegmentInfo + iSegment;
        NITFDES *psDES;
        int nOffset = 0;
        char szTREName[7];
        int nThisTRESize;

        if( !EQUAL(psSegInfo->szSegmentType,"DE") )
            continue;

        psDES = NITFDESAccess( psFile, iSegment );
        if( psDES == NULL )
            continue;

        char* pabyTREData = NULL;
        nOffset = 0;
        while (NITFDESGetTRE( psDES, nOffset, szTREName, &pabyTREData, &nThisTRESize))
        {
            char* pszEscapedData = CPLEscapeString( pabyTREData, nThisTRESize,
                                                CPLES_BackslashQuotable );
            if (pszEscapedData == NULL)
            {
                CPLError( CE_Failure, CPLE_OutOfMemory, "Out of memory");
                NITFDESFreeTREData(pabyTREData);
                NITFDESDeaccess(psDES);
                return;
            }

            // trim white off tag. 
            while( strlen(szTREName) > 0 && szTREName[strlen(szTREName)-1] == ' ' )
                szTREName[strlen(szTREName)-1] = '\0';

            CPLXMLNode* psTreNode = NITFCreateXMLTre(psFile, szTREName, pabyTREData,nThisTRESize);
            if (psTreNode)
            {
                const char* pszDESID = CSLFetchNameValue(psDES->papszMetadata, "NITF_DESID");
                CPLCreateXMLNode(CPLCreateXMLNode(psTreNode, CXT_Attribute, "location"),
                                 CXT_Text, pszDESID ? CPLSPrintf("des %s", pszDESID) : "des");
                CPLAddXMLChild(psTresNode, psTreNode);
            }

            char szUniqueTag[32];
            strcpy(szUniqueTag, szTREName);
            int nCountUnique = 2;
            while(oSpecialMD.GetMetadataItem( szUniqueTag, "TRE") != NULL)
            {
                sprintf(szUniqueTag, "%s_%d", szTREName, nCountUnique);
                nCountUnique ++;
            }
            oSpecialMD.SetMetadataItem( szUniqueTag, pszEscapedData, "TRE" );

            CPLFree(pszEscapedData);

            nOffset += 11 + nThisTRESize;

            NITFDESFreeTREData(pabyTREData);
        }

        NITFDESDeaccess(psDES);
    }

    if (psTresNode->psChild != NULL)
    {
        char* pszXML = CPLSerializeXMLTree(psTresNode);
        char* apszMD[2];
        apszMD[0] = pszXML;
        apszMD[1] = NULL;
        oSpecialMD.SetMetadata( apszMD, "xml:TRE" );
        CPLFree(pszXML);
    }
    CPLDestroyXMLNode(psTresNode);
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **NITFDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "NITF_METADATA", "NITF_DES", "NITF_DES_METADATA",
                                   "NITF_FILE_HEADER_TRES", "NITF_IMAGE_SEGMENT_TRES",
                                   "CGM", "TEXT", "TRE", "xml:TRE", "OVERVIEWS", NULL);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **NITFDataset::GetMetadata( const char * pszDomain )

{
    if( pszDomain != NULL && EQUAL(pszDomain,"NITF_METADATA") )
    {
        // InitializeNITFMetadata retrieves the NITF file header and all image segment file headers. (NOTE: The returned strings are base64-encoded).

        InitializeNITFMetadata();
        return oSpecialMD.GetMetadata( pszDomain );
    }

#ifdef ESRI_BUILD
    if( pszDomain != NULL && EQUAL(pszDomain,"NITF_DES") )
    {
        // InitializeNITFDESs retrieves all the DES file headers (NOTE: The returned strings are base64-encoded).

        InitializeNITFDESs();
        return oSpecialMD.GetMetadata( pszDomain );
    }

    if( pszDomain != NULL && EQUAL(pszDomain,"NITF_DES_METADATA") )
    {
        // InitializeNITFDESs retrieves all the DES file headers (NOTE: The returned strings are base64-encoded).

        InitializeNITFDESMetadata();
        return oSpecialMD.GetMetadata( pszDomain );
    }

    if( pszDomain != NULL && EQUAL(pszDomain,"NITF_FILE_HEADER_TRES") )
    {
        // InitializeNITFTREs retrieves all the TREs that are resides in the NITF file header and all the
        // TREs that are resides in the current image segment.
        // NOTE: the returned strings are backslash-escaped

        InitializeNITFTREs();
        return oSpecialMD.GetMetadata( pszDomain );
    }

    if( pszDomain != NULL && EQUAL(pszDomain,"NITF_IMAGE_SEGMENT_TRES") )
    {
        // InitializeNITFTREs retrieves all the TREs that are resides in the NITF file header and all the
        // TREs that are resides in the current image segment.
        // NOTE: the returned strings are backslash-escaped

        InitializeNITFTREs();
        return oSpecialMD.GetMetadata( pszDomain );
    }
#endif

    if( pszDomain != NULL && EQUAL(pszDomain,"CGM") )
    {
        InitializeCGMMetadata();
        return oSpecialMD.GetMetadata( pszDomain );
    }

    if( pszDomain != NULL && EQUAL(pszDomain,"TEXT") )
    {
        InitializeTextMetadata();
        return oSpecialMD.GetMetadata( pszDomain );
    }

    if( pszDomain != NULL && EQUAL(pszDomain,"TRE") )
    {
        InitializeTREMetadata();
        return oSpecialMD.GetMetadata( pszDomain );
    }

    if( pszDomain != NULL && EQUAL(pszDomain,"xml:TRE") )
    {
        InitializeTREMetadata();
        return oSpecialMD.GetMetadata( pszDomain );
    }

    return GDALPamDataset::GetMetadata( pszDomain );
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *NITFDataset::GetMetadataItem(const char * pszName,
                                         const char * pszDomain )

{
    if( pszDomain != NULL && EQUAL(pszDomain,"NITF_METADATA") )
    {
        // InitializeNITFMetadata retrieves the NITF file header and all image segment file headers. (NOTE: The returned strings are base64-encoded).

        InitializeNITFMetadata();
        return oSpecialMD.GetMetadataItem( pszName, pszDomain );
    }

#ifdef ESRI_BUILD
    if( pszDomain != NULL && EQUAL(pszDomain,"NITF_DES_METADATA") )
    {
        // InitializeNITFDESs retrieves all the DES file headers (NOTE: The returned strings are base64-encoded).

        InitializeNITFDESMetadata();
        return oSpecialMD.GetMetadataItem( pszName, pszDomain );
    }

    if( pszDomain != NULL && EQUAL(pszDomain,"NITF_FILE_HEADER_TRES") )
    {
        // InitializeNITFTREs retrieves all the TREs that are resides in the NITF file header and all the
        // TREs that are resides in the current image segment.
        // NOTE: the returned strings are backslash-escaped

        InitializeNITFTREs();
        return oSpecialMD.GetMetadataItem( pszName, pszDomain );
    }

    if( pszDomain != NULL && EQUAL(pszDomain,"NITF_IMAGE_SEGMENT_TRES") )
    {
        // InitializeNITFTREs retrieves all the TREs that are resides in the NITF file header and all the
        // TREs that are resides in the current image segment.
        // NOTE: the returned strings are backslash-escaped

        InitializeNITFTREs();
        return oSpecialMD.GetMetadataItem( pszName, pszDomain );
    }
#endif

    if( pszDomain != NULL && EQUAL(pszDomain,"CGM") )
    {
        InitializeCGMMetadata();
        return oSpecialMD.GetMetadataItem( pszName, pszDomain );
    }

    if( pszDomain != NULL && EQUAL(pszDomain,"TEXT") )
    {
        InitializeTextMetadata();
        return oSpecialMD.GetMetadataItem( pszName, pszDomain );
    }

    if( pszDomain != NULL && EQUAL(pszDomain,"TRE") )
    {
        InitializeTREMetadata();
        return oSpecialMD.GetMetadataItem( pszName, pszDomain );
    }

    if( pszDomain != NULL && EQUAL(pszDomain,"OVERVIEWS") 
        && osRSetVRT.size() > 0 )
        return osRSetVRT;

    return GDALPamDataset::GetMetadataItem( pszName, pszDomain );
}


/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int NITFDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *NITFDataset::GetGCPProjection()

{
    if( nGCPCount > 0 && pszGCPProjection != NULL )
        return pszGCPProjection;
    else
        return "";
}

/************************************************************************/
/*                               GetGCP()                               */
/************************************************************************/

const GDAL_GCP *NITFDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                           CheckForRSets()                            */
/*                                                                      */
/*      Check for reduced resolution images in .r<n> files and if       */
/*      found return filename for a virtual file wrapping them as an    */
/*      overview file. (#3457)                                          */
/************************************************************************/

int NITFDataset::CheckForRSets( const char *pszNITFFilename )

{
    bool isR0File = EQUAL(CPLGetExtension(pszNITFFilename),"r0");

/* -------------------------------------------------------------------- */
/*      Check to see if we have RSets.                                  */
/* -------------------------------------------------------------------- */
    std::vector<CPLString> aosRSetFilenames;
    int i;

    for( i = 1; i <= 5; i++ )
    {
        CPLString osTarget;
        VSIStatBufL sStat;

        if ( isR0File )
        {
          osTarget = pszNITFFilename;
          osTarget[osTarget.size()-1] = (char) ('0' + i );
        }
        else
          osTarget.Printf( "%s.r%d", pszNITFFilename, i );

        if( VSIStatL( osTarget, &sStat ) != 0 )
            break;

        aosRSetFilenames.push_back( osTarget );
    }
   
    if( aosRSetFilenames.size() == 0 )
        return FALSE;
    
/* -------------------------------------------------------------------- */
/*      We do, so try to create a wrapping VRT file.                    */
/* -------------------------------------------------------------------- */
    CPLString osFragment;
    int iBand;

    osRSetVRT.Printf( "<VRTDataset rasterXSize=\"%d\" rasterYSize=\"%d\">\n",
                  GetRasterXSize()/2, GetRasterYSize()/2 );

    for( iBand = 0; iBand < GetRasterCount(); iBand++ )
    {
        GDALRasterBand *poBand = GetRasterBand(iBand+1);

        osRSetVRT += osFragment.
            Printf( "  <VRTRasterBand dataType=\"%s\" band=\"%d\">\n", 
                    GDALGetDataTypeName( poBand->GetRasterDataType() ),
                    iBand+1 );

        for( i = 0; i < (int) aosRSetFilenames.size(); i++ )
        {
            char* pszEscaped = CPLEscapeString(aosRSetFilenames[i].c_str(), -1, CPLES_XML);
            if( i == 0 )
                osRSetVRT += osFragment.Printf(
                    "    <SimpleSource><SourceFilename>%s</SourceFilename><SourceBand>%d</SourceBand></SimpleSource>\n", 
                    pszEscaped, iBand+1 );
            else
                osRSetVRT += osFragment.Printf(
                    "    <Overview><SourceFilename>%s</SourceFilename><SourceBand>%d</SourceBand></Overview>\n", 
                    pszEscaped, iBand+1 );
            CPLFree(pszEscaped);
        }
        osRSetVRT += osFragment.
            Printf( "  </VRTRasterBand>\n" );
    }

    osRSetVRT += "</VRTDataset>\n";

    return TRUE;
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr NITFDataset::IBuildOverviews( const char *pszResampling, 
                                     int nOverviews, int *panOverviewList, 
                                     int nListBands, int *panBandList,
                                     GDALProgressFunc pfnProgress, 
                                     void * pProgressData )
    
{
/* -------------------------------------------------------------------- */
/*      If we have been using RSets we will need to clear them first.   */
/* -------------------------------------------------------------------- */
    if( osRSetVRT.size() > 0 )
    {
        oOvManager.CleanOverviews();
        osRSetVRT = "";
    }

    bExposeUnderlyingJPEGDatasetOverviews = FALSE;

/* -------------------------------------------------------------------- */
/*      If we have an underlying JPEG2000 dataset (hopefully via        */
/*      JP2KAK) we will try and build zero overviews as a way of        */
/*      tricking it into clearing existing overviews-from-jpeg2000.     */
/* -------------------------------------------------------------------- */
    if( poJ2KDataset != NULL 
        && !poJ2KDataset->GetMetadataItem( "OVERVIEW_FILE", "OVERVIEWS" ) )
        poJ2KDataset->IBuildOverviews( pszResampling, 0, NULL, 
                                       nListBands, panBandList, 
                                       GDALDummyProgress, NULL );

/* -------------------------------------------------------------------- */
/*      Use the overview manager to build requested overviews.          */
/* -------------------------------------------------------------------- */
    CPLErr eErr = GDALPamDataset::IBuildOverviews( pszResampling, 
                                                   nOverviews, panOverviewList,
                                                   nListBands, panBandList,
                                                   pfnProgress, pProgressData );

/* -------------------------------------------------------------------- */
/*      If we are working with jpeg or jpeg2000, let the underlying     */
/*      dataset know about the overview file.                           */
/* -------------------------------------------------------------------- */
    GDALDataset *poSubDS = poJ2KDataset;
    if( poJPEGDataset )
        poSubDS = poJPEGDataset;

    const char *pszOverviewFile = 
        GetMetadataItem( "OVERVIEW_FILE", "OVERVIEWS" );

    if( poSubDS && pszOverviewFile != NULL && eErr == CE_None
        && poSubDS->GetMetadataItem( "OVERVIEW_FILE", "OVERVIEWS") == NULL )
    {
        poSubDS->SetMetadataItem( "OVERVIEW_FILE", 
                                  pszOverviewFile,
                                  "OVERVIEWS" );
    }

    return eErr;
}

/************************************************************************/
/*                           ScanJPEGQLevel()                           */
/*                                                                      */
/*      Search the NITF APP header in the jpeg data stream to find      */
/*      out what predefined Q level tables should be used (or -1 if     */
/*      they are inline).                                               */
/************************************************************************/

int NITFDataset::ScanJPEGQLevel( GUIntBig *pnDataStart )

{
    GByte abyHeader[100];

    if( VSIFSeekL( psFile->fp, *pnDataStart,
                   SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Seek error to jpeg data stream." );
        return 0;
    }
        
    if( VSIFReadL( abyHeader, 1, sizeof(abyHeader), psFile->fp ) 
        < sizeof(abyHeader) )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Read error to jpeg data stream." );
        return 0;
    }

/* -------------------------------------------------------------------- */
/*      Scan ahead for jpeg magic code.  In some files (eg. NSIF)       */
/*      there seems to be some extra junk before the image data stream. */
/* -------------------------------------------------------------------- */
    GUInt32 nOffset = 0;
    while( nOffset < sizeof(abyHeader) - 23 
           && (abyHeader[nOffset+0] != 0xff
               || abyHeader[nOffset+1] != 0xd8
               || abyHeader[nOffset+2] != 0xff) )
        nOffset++;

    if( nOffset >= sizeof(abyHeader) - 23 )
        return 0;

    *pnDataStart += nOffset;

    if( nOffset > 0 )
        CPLDebug( "NITF", 
                  "JPEG data stream at offset %d from start of data segement, NSIF?", 
                  nOffset );

/* -------------------------------------------------------------------- */
/*      Do we have an NITF app tag?  If so, pull out the Q level.       */
/* -------------------------------------------------------------------- */
    if( !EQUAL((char *)abyHeader+nOffset+6,"NITF") )
        return 0;

    return abyHeader[22+nOffset];
}

/************************************************************************/
/*                           ScanJPEGBlocks()                           */
/************************************************************************/

CPLErr NITFDataset::ScanJPEGBlocks()

{
    int iBlock;
    GUIntBig nJPEGStart = 
        psFile->pasSegmentInfo[psImage->iSegment].nSegmentStart;

    nQLevel = ScanJPEGQLevel( &nJPEGStart );

/* -------------------------------------------------------------------- */
/*      Allocate offset array                                           */
/* -------------------------------------------------------------------- */
    panJPEGBlockOffset = (GIntBig *) 
        VSICalloc(sizeof(GIntBig),
                  psImage->nBlocksPerRow*psImage->nBlocksPerColumn);
    if (panJPEGBlockOffset == NULL)
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, "Out of memory");
        return CE_Failure;
    }
    panJPEGBlockOffset[0] = nJPEGStart;

    if ( psImage->nBlocksPerRow * psImage->nBlocksPerColumn == 1)
        return CE_None;

    for( iBlock = psImage->nBlocksPerRow * psImage->nBlocksPerColumn - 1;
         iBlock > 0; iBlock-- )
        panJPEGBlockOffset[iBlock] = -1;
    
/* -------------------------------------------------------------------- */
/*      Scan through the whole image data stream identifying all        */
/*      block boundaries.  Each block starts with 0xFFD8 (SOI).         */
/*      They also end with 0xFFD9, but we don't currently look for      */
/*      that.                                                           */
/* -------------------------------------------------------------------- */
    int iNextBlock = 1;
    GIntBig iSegOffset = 2;
    GIntBig iSegSize = psFile->pasSegmentInfo[psImage->iSegment].nSegmentSize
        - (nJPEGStart - psFile->pasSegmentInfo[psImage->iSegment].nSegmentStart);
    GByte abyBlock[512];
    int ignoreBytes = 0;

    while( iSegOffset < iSegSize-1 )
    {
        size_t nReadSize = MIN((size_t)sizeof(abyBlock),(size_t)(iSegSize - iSegOffset));
        size_t i;

        if( VSIFSeekL( psFile->fp, panJPEGBlockOffset[0] + iSegOffset, 
                       SEEK_SET ) != 0 )
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                      "Seek error to jpeg data stream." );
            return CE_Failure;
        }
        
        if( VSIFReadL( abyBlock, 1, nReadSize, psFile->fp ) < (size_t)nReadSize)
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                      "Read error to jpeg data stream." );
            return CE_Failure;
        }

        for( i = 0; i < nReadSize-1; i++ )
        {
            if (ignoreBytes == 0)
            {
                if( abyBlock[i] == 0xff )
                {
                    /* start-of-image marker */ 
                    if ( abyBlock[i+1] == 0xd8 )
                    {
                        panJPEGBlockOffset[iNextBlock++] 
                             = panJPEGBlockOffset[0] + iSegOffset + i; 

                        if( iNextBlock == psImage->nBlocksPerRow*psImage->nBlocksPerColumn) 
                        {
                            return CE_None;
                        }
                    }
                    /* Skip application-specific data to avoid false positive while detecting */ 
                    /* start-of-image markers (#2927). The size of the application data is */
                    /* found in the two following bytes */
                    /* We need this complex mechanism of ignoreBytes for dealing with */
                    /* application data crossing several abyBlock ... */
                    else if ( abyBlock[i+1] >= 0xe0 && abyBlock[i+1] < 0xf0 ) 
                    {
                        ignoreBytes = -2;
                    }
                }
            }
            else if (ignoreBytes < 0)
            {
                if (ignoreBytes == -1)
                {
                    /* Size of the application data */
                    ignoreBytes = abyBlock[i]*256 + abyBlock[i+1];
                }
                else
                    ignoreBytes++;
            }
            else
            {
                ignoreBytes--;
            }
        }

        iSegOffset += nReadSize - 1;
    }

    return CE_None;
}

/************************************************************************/
/*                           ReadJPEGBlock()                            */
/************************************************************************/

CPLErr NITFDataset::ReadJPEGBlock( int iBlockX, int iBlockY )

{
    CPLErr eErr;

/* -------------------------------------------------------------------- */
/*      If this is our first request, do a scan for block boundaries.   */
/* -------------------------------------------------------------------- */
    if( panJPEGBlockOffset == NULL )
    {
        if (EQUAL(psImage->szIC,"M3"))
        {
/* -------------------------------------------------------------------- */
/*      When a data mask subheader is present, we don't need to scan    */
/*      the whole file. We just use the psImage->panBlockStart table    */
/* -------------------------------------------------------------------- */
            panJPEGBlockOffset = (GIntBig *) 
                VSICalloc(sizeof(GIntBig),
                        psImage->nBlocksPerRow*psImage->nBlocksPerColumn);
            if (panJPEGBlockOffset == NULL)
            {
                CPLError( CE_Failure, CPLE_OutOfMemory, "Out of memory");
                return CE_Failure;
            }
            int i;
            for (i=0;i< psImage->nBlocksPerRow*psImage->nBlocksPerColumn;i++)
            {
                panJPEGBlockOffset[i] = psImage->panBlockStart[i];
                if (panJPEGBlockOffset[i] != -1 && panJPEGBlockOffset[i] != 0xffffffff)
                {
                    GUIntBig nOffset = panJPEGBlockOffset[i];
                    nQLevel = ScanJPEGQLevel(&nOffset);
                    /* The beginning of the JPEG stream should be the offset */
                    /* from the panBlockStart table */
                    if (nOffset != (GUIntBig)panJPEGBlockOffset[i])
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "JPEG block doesn't start at expected offset");
                        return CE_Failure;
                    }
                }
            }
        }
        else /* 'C3' case */
        {
/* -------------------------------------------------------------------- */
/*      Scan through the whole image data stream identifying all        */
/*      block boundaries.                                               */
/* -------------------------------------------------------------------- */
            eErr = ScanJPEGBlocks();
            if( eErr != CE_None )
                return eErr;
        }
    }
    
/* -------------------------------------------------------------------- */
/*    Allocate image data block (where the uncompressed image will go)  */
/* -------------------------------------------------------------------- */
    if( pabyJPEGBlock == NULL )
    {
        /* Allocate enough memory to hold 12bit JPEG data */
        pabyJPEGBlock = (GByte *) 
            VSICalloc(psImage->nBands,
                      psImage->nBlockWidth * psImage->nBlockHeight * 2);
        if (pabyJPEGBlock == NULL)
        {
            CPLError( CE_Failure, CPLE_OutOfMemory, "Out of memory");
            return CE_Failure;
        }
    }


/* -------------------------------------------------------------------- */
/*      Read JPEG Chunk.                                                */
/* -------------------------------------------------------------------- */
    CPLString osFilename;
    int iBlock = iBlockX + iBlockY * psImage->nBlocksPerRow;
    GDALDataset *poDS;
    int anBands[3] = { 1, 2, 3 };

    if (panJPEGBlockOffset[iBlock] == -1 || panJPEGBlockOffset[iBlock] == 0xffffffff)
    {
        memset(pabyJPEGBlock, 0, psImage->nBands*psImage->nBlockWidth*psImage->nBlockHeight*2);
        return CE_None;
    }

    osFilename.Printf( "JPEG_SUBFILE:Q%d," CPL_FRMT_GIB ",%d,%s", 
                       nQLevel,
                       panJPEGBlockOffset[iBlock], 0, 
                       osNITFFilename.c_str() );

    poDS = (GDALDataset *) GDALOpen( osFilename, GA_ReadOnly );
    if( poDS == NULL )
        return CE_Failure;

    if( poDS->GetRasterXSize() != psImage->nBlockWidth
        || poDS->GetRasterYSize() != psImage->nBlockHeight )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "JPEG block %d not same size as NITF blocksize.", 
                  iBlock );
        delete poDS;
        return CE_Failure;
    }

    if( poDS->GetRasterCount() < psImage->nBands )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "JPEG block %d has not enough bands.", 
                  iBlock );
        delete poDS;
        return CE_Failure;
    }

    if( poDS->GetRasterBand(1)->GetRasterDataType() != GetRasterBand(1)->GetRasterDataType())
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "JPEG block %d data type (%s) not consistant with band data type (%s).", 
                  iBlock, GDALGetDataTypeName(poDS->GetRasterBand(1)->GetRasterDataType()),
                  GDALGetDataTypeName(GetRasterBand(1)->GetRasterDataType()) );
        delete poDS;
        return CE_Failure;
    }

    eErr = poDS->RasterIO( GF_Read, 
                           0, 0, 
                           psImage->nBlockWidth, psImage->nBlockHeight,
                           pabyJPEGBlock, 
                           psImage->nBlockWidth, psImage->nBlockHeight,
                           GetRasterBand(1)->GetRasterDataType(), psImage->nBands, anBands, 0, 0, 0 );

    delete poDS;

    return eErr;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **NITFDataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();

/* -------------------------------------------------------------------- */
/*      Check for .imd file.                                            */
/* -------------------------------------------------------------------- */
    papszFileList = AddFile( papszFileList, "IMD", "imd" );

/* -------------------------------------------------------------------- */
/*      Check for .rpb file.                                            */
/* -------------------------------------------------------------------- */
    papszFileList = AddFile( papszFileList, "RPB", "rpb" );

/* -------------------------------------------------------------------- */
/*      Check for other files.                                          */
/* -------------------------------------------------------------------- */
    papszFileList = AddFile( papszFileList, "ATT", "att" );
    papszFileList = AddFile( papszFileList, "EPH", "eph" );
    papszFileList = AddFile( papszFileList, "GEO", "geo" );
    papszFileList = AddFile( papszFileList, "XML", "xml" );

    return papszFileList;
}

/************************************************************************/
/*                              AddFile()                               */
/*                                                                      */
/*      Helper method for GetFileList()                                 */
/************************************************************************/
char **NITFDataset::AddFile(char **papszFileList, const char* EXTENSION, const char* extension)
{
    VSIStatBufL sStatBuf;
    CPLString osTarget = CPLResetExtension( osNITFFilename, EXTENSION );
    if( VSIStatL( osTarget, &sStatBuf ) == 0 )
        papszFileList = CSLAddString( papszFileList, osTarget );
    else
    {
        osTarget = CPLResetExtension( osNITFFilename, extension );
        if( VSIStatL( osTarget, &sStatBuf ) == 0 )
            papszFileList = CSLAddString( papszFileList, osTarget );
    }

    return papszFileList;
}

/************************************************************************/
/*                         GDALToNITFDataType()                         */
/************************************************************************/

static const char *GDALToNITFDataType( GDALDataType eType )

{
    const char *pszPVType;

    switch( eType )
    {
      case GDT_Byte:
      case GDT_UInt16:
      case GDT_UInt32:
        pszPVType = "INT";
        break;

      case GDT_Int16:
      case GDT_Int32:
        pszPVType = "SI";
        break;

      case GDT_Float32:
      case GDT_Float64:
        pszPVType = "R";
        break;

      case GDT_CInt16:
      case GDT_CInt32:
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "NITF format does not support complex integer data." );
        return NULL;

      case GDT_CFloat32:
        pszPVType = "C";
        break;

      default:
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unsupported raster pixel type (%s).", 
                  GDALGetDataTypeName(eType) );
        return NULL;
    }

    return pszPVType;
}

/************************************************************************/
/*                          NITFJP2ECWOptions()                         */
/*                                                                      */
/*      Prepare JP2-in-NITF creation options based in part of the       */
/*      NITF creation options.                                          */
/************************************************************************/

static char **NITFJP2ECWOptions( char **papszOptions )

{
    int i;
    char** papszJP2Options = NULL;
    
    papszJP2Options = CSLAddString(papszJP2Options, "PROFILE=NPJE");
    papszJP2Options = CSLAddString(papszJP2Options, "CODESTREAM_ONLY=TRUE");
    
    for( i = 0; papszOptions != NULL && papszOptions[i] != NULL; i++ )
    {
        if( EQUALN(papszOptions[i],"PROFILE=",8) )
        {
            CPLFree(papszJP2Options[0]);
            papszJP2Options[0] = CPLStrdup(papszOptions[i]);
        }
        else if( EQUALN(papszOptions[i],"TARGET=",7) )
            papszJP2Options = CSLAddString(papszJP2Options, papszOptions[i]);
    }

    return papszJP2Options;
}
/************************************************************************/
/*                           NITFJP2KAKOptions()                        */
/*                                                                      */
/*      Prepare JP2-in-NITF creation options based in part of the       */
/*      NITF creation options.                                          */
/************************************************************************/

static char **NITFJP2KAKOptions( char **papszOptions )

{
    int i;
    char** papszKAKOptions = NULL;
    
    for( i = 0; papszOptions != NULL && papszOptions[i] != NULL; i++ )
    {
       if(      EQUALN(papszOptions[i],"QUALITY=", 8) )
          papszKAKOptions = CSLAddString(papszKAKOptions, papszOptions[i]);
       else if (EQUALN(papszOptions[i],"BLOCKXSIZE=", 11) )
          papszKAKOptions = CSLAddString(papszKAKOptions, papszOptions[i]);
       else if (EQUALN(papszOptions[i],"BLOCKYSIZE=", 11) )
          papszKAKOptions = CSLAddString(papszKAKOptions, papszOptions[i]);
       else if (EQUALN(papszOptions[i],"GMLPJ2=", 7) )
          papszKAKOptions = CSLAddString(papszKAKOptions, papszOptions[i]);
       else if (EQUALN(papszOptions[i],"GeoJP2=", 7) )
          papszKAKOptions = CSLAddString(papszKAKOptions, papszOptions[i]);
       else if (EQUALN(papszOptions[i],"LAYERS=", 7) )
          papszKAKOptions = CSLAddString(papszKAKOptions, papszOptions[i]);
       else if (EQUALN(papszOptions[i],"ROI=", 4) )
          papszKAKOptions = CSLAddString(papszKAKOptions, papszOptions[i]);
    }

    return papszKAKOptions;
}



/************************************************************************/
/*              NITFExtractTEXTAndCGMCreationOption()                   */
/************************************************************************/

static char** NITFExtractTEXTAndCGMCreationOption( GDALDataset* poSrcDS,
                                                   char **papszOptions,
                                                   char ***ppapszTextMD,
                                                   char ***ppapszCgmMD )
{
    char** papszFullOptions = CSLDuplicate(papszOptions);

/* -------------------------------------------------------------------- */
/*      Prepare for text segments.                                      */
/* -------------------------------------------------------------------- */
    int iOpt, nNUMT = 0;
    char **papszTextMD = CSLFetchNameValueMultiple (papszOptions, "TEXT");
    // Notice: CSLFetchNameValueMultiple remove the leading "TEXT=" when
    // returning the list, which is what we want.

    // Use TEXT information from original image if no creation option is passed in.
    if (poSrcDS != NULL && papszTextMD == NULL)
    {
        // Read CGM adata from original image, duplicate the list becuase
        // we frees papszCgmMD at end of the function.
        papszTextMD = CSLDuplicate( poSrcDS->GetMetadata( "TEXT" ));
    }

    for( iOpt = 0; 
         papszTextMD != NULL && papszTextMD[iOpt] != NULL; 
         iOpt++ )
    {
        if( !EQUALN(papszTextMD[iOpt],"DATA_",5) )
            continue;

        nNUMT++;
    }

    if( nNUMT > 0 )
    {
        papszFullOptions = CSLAddString( papszFullOptions, 
                                         CPLString().Printf( "NUMT=%d", 
                                                             nNUMT ) );
    }

/* -------------------------------------------------------------------- */
/*      Prepare for CGM segments.                                       */
/* -------------------------------------------------------------------- */
    const char *pszNUMS; // graphic segment option string
    int nNUMS = 0;

    char **papszCgmMD = CSLFetchNameValueMultiple (papszOptions, "CGM");
    // Notice: CSLFetchNameValueMultiple remove the leading "CGM=" when
    // returning the list, which is what we want.

    // Use CGM information from original image if no creation option is passed in.
    if (poSrcDS != NULL && papszCgmMD == NULL)
    {
        // Read CGM adata from original image, duplicate the list becuase
        // we frees papszCgmMD at end of the function.
        papszCgmMD = CSLDuplicate( poSrcDS->GetMetadata( "CGM" ));
    }

    // Set NUMS based on the number of segments
    if (papszCgmMD != NULL)
    {
        pszNUMS = CSLFetchNameValue(papszCgmMD, "SEGMENT_COUNT");

        if (pszNUMS != NULL) {
            nNUMS = atoi(pszNUMS);
        }
        papszFullOptions = CSLAddString(papszFullOptions,
                                        CPLString().Printf("NUMS=%d", nNUMS));
    }

    *ppapszTextMD = papszTextMD;
    *ppapszCgmMD = papszCgmMD;

    return papszFullOptions;
}

/************************************************************************/
/*                         NITFDatasetCreate()                          */
/************************************************************************/

GDALDataset *
NITFDataset::NITFDatasetCreate( const char *pszFilename, int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char **papszOptions )

{
    const char *pszPVType = GDALToNITFDataType( eType );
    const char *pszIC = CSLFetchNameValue( papszOptions, "IC" );

    if( pszPVType == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      We disallow any IC value except NC when creating this way.      */
/* -------------------------------------------------------------------- */
    GDALDriver *poJ2KDriver = NULL;

    if( pszIC != NULL && EQUAL(pszIC,"C8") )
    {
        int bHasCreate = FALSE;
       
        poJ2KDriver = GetGDALDriverManager()->GetDriverByName( "JP2ECW" );
        if( poJ2KDriver != NULL )
            bHasCreate = poJ2KDriver->GetMetadataItem( GDAL_DCAP_CREATE, 
                                                       NULL ) != NULL;
        if( !bHasCreate )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unable to create JPEG2000 encoded NITF files.  The\n"
                      "JP2ECW driver is unavailable, or missing Create support." );
           return NULL;
        }
    }

    else if( pszIC != NULL && !EQUAL(pszIC,"NC") )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unsupported compression (IC=%s) used in direct\n"
                  "NITF File creation", 
                  pszIC );
        return NULL;
    }

    const char* pszSDE_TRE = CSLFetchNameValue(papszOptions, "SDE_TRE");
    if (pszSDE_TRE != NULL)
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "SDE_TRE creation option ignored by Create() method (only valid in CreateCopy())" );
    }
    

/* -------------------------------------------------------------------- */
/*      Prepare for text and CGM segments.                              */
/* -------------------------------------------------------------------- */
    char **papszTextMD = NULL;
    char **papszCgmMD = NULL;
    char **papszFullOptions = NITFExtractTEXTAndCGMCreationOption( NULL,
                                                          papszOptions,
                                                          &papszTextMD,
                                                          &papszCgmMD );

/* -------------------------------------------------------------------- */
/*      Create the file.                                                */
/* -------------------------------------------------------------------- */

    if( !NITFCreate( pszFilename, nXSize, nYSize, nBands, 
                     GDALGetDataTypeSize( eType ), pszPVType, 
                     papszFullOptions ) )
    {
        CSLDestroy(papszTextMD);
        CSLDestroy(papszCgmMD);
        CSLDestroy(papszFullOptions);
        return NULL;
    }

    CSLDestroy(papszFullOptions);
    papszFullOptions = NULL;

/* -------------------------------------------------------------------- */
/*      Various special hacks related to JPEG2000 encoded files.        */
/* -------------------------------------------------------------------- */
    GDALDataset* poWritableJ2KDataset = NULL;
    if( poJ2KDriver )
    {
        NITFFile *psFile = NITFOpen( pszFilename, TRUE );
        if (psFile == NULL)
        {
            CSLDestroy(papszTextMD);
            CSLDestroy(papszCgmMD);
            return NULL;
        }
        GUIntBig nImageOffset = psFile->pasSegmentInfo[0].nSegmentStart;

        CPLString osDSName;

        osDSName.Printf("/vsisubfile/" CPL_FRMT_GUIB "_%d,%s", nImageOffset, -1, pszFilename);

        NITFClose( psFile );

        char** papszJP2Options = NITFJP2ECWOptions(papszOptions);
        poWritableJ2KDataset = 
            poJ2KDriver->Create( osDSName, nXSize, nYSize, nBands, eType, 
                                 papszJP2Options );
        CSLDestroy(papszJP2Options);

        if( poWritableJ2KDataset == NULL )
        {
            CSLDestroy(papszTextMD);
            CSLDestroy(papszCgmMD);
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Open the dataset in update mode.                                */
/* -------------------------------------------------------------------- */
    GDALOpenInfo oOpenInfo( pszFilename, GA_Update );
    NITFDataset* poDS = (NITFDataset*)
            NITFDataset::OpenInternal(&oOpenInfo, poWritableJ2KDataset, TRUE);
    if (poDS)
    {
        poDS->papszTextMDToWrite = papszTextMD;
        poDS->papszCgmMDToWrite = papszCgmMD;
    }
    else
    {
        CSLDestroy(papszTextMD);
        CSLDestroy(papszCgmMD);
    }
    return poDS;
}

/************************************************************************/
/*                           NITFCreateCopy()                           */
/************************************************************************/

GDALDataset *
NITFDataset::NITFCreateCopy( 
    const char *pszFilename, GDALDataset *poSrcDS,
    int bStrict, char **papszOptions, 
    GDALProgressFunc pfnProgress, void * pProgressData )

{
    GDALDataType eType;
    GDALRasterBand *poBand1;
    int   bJPEG2000 = FALSE;
    int   bJPEG = FALSE;
    NITFDataset *poDstDS = NULL;
    GDALDriver *poJ2KDriver = NULL;

    int  nBands = poSrcDS->GetRasterCount();
    if( nBands == 0 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Unable to export files with zero bands." );
        return NULL;
    }

    poBand1 = poSrcDS->GetRasterBand(1);
    if( poBand1 == NULL )
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Only allow supported compression values.                        */
/* -------------------------------------------------------------------- */
    const char* pszIC = CSLFetchNameValue( papszOptions, "IC" );
    if( pszIC != NULL )
    {
        if( EQUAL(pszIC,"NC") )
            /* ok */;
        else if( EQUAL(pszIC,"C8") )
        {
            poJ2KDriver = 
                GetGDALDriverManager()->GetDriverByName( "JP2ECW" );
            if( poJ2KDriver == NULL || 
                poJ2KDriver->GetMetadataItem( GDAL_DCAP_CREATECOPY, NULL ) == NULL )
            {
                /* Try with  JP2KAK as an alternate driver */
                poJ2KDriver = 
                    GetGDALDriverManager()->GetDriverByName(  "JP2KAK" );
            }
            if( poJ2KDriver == NULL )
            {
                /* Try with Jasper as an alternate driver */
                poJ2KDriver = 
                    GetGDALDriverManager()->GetDriverByName( "JPEG2000" );
            }
            if( poJ2KDriver == NULL )
            {
                CPLError( 
                    CE_Failure, CPLE_AppDefined, 
                    "Unable to write JPEG2000 compressed NITF file.\n"
                    "No 'subfile' JPEG2000 write supporting drivers are\n"
                    "configured." );
                return NULL;
            }
            bJPEG2000 = TRUE;
        }
        else if( EQUAL(pszIC,"C3") || EQUAL(pszIC,"M3") )
        {
            bJPEG = TRUE;
#ifndef JPEG_SUPPORTED
            CPLError( 
                CE_Failure, CPLE_AppDefined, 
                "Unable to write JPEG compressed NITF file.\n"
                "Libjpeg is not configured into build." );
            return NULL;
#endif
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Only IC=NC (uncompressed), IC=C3/M3 (JPEG) and IC=C8 (JPEG2000)\n"
                      "allowed with NITF CreateCopy method." );
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Get the data type.  Complex integers isn't supported by         */
/*      NITF, so map that to complex float if we aren't in strict       */
/*      mode.                                                           */
/* -------------------------------------------------------------------- */
    eType = poBand1->GetRasterDataType();
    if( !bStrict && (eType == GDT_CInt16 || eType == GDT_CInt32) )
        eType = GDT_CFloat32;

/* -------------------------------------------------------------------- */
/*      Prepare for text and CGM segments.                              */
/* -------------------------------------------------------------------- */
    char **papszTextMD = NULL;
    char **papszCgmMD = NULL;
    char **papszFullOptions = NITFExtractTEXTAndCGMCreationOption( poSrcDS,
                                                         papszOptions,
                                                         &papszTextMD,
                                                         &papszCgmMD );

/* -------------------------------------------------------------------- */
/*      Copy over other source metadata items as creation options       */
/*      that seem useful.                                               */
/* -------------------------------------------------------------------- */
    char **papszSrcMD = poSrcDS->GetMetadata();
    int iMD;

    for( iMD = 0; papszSrcMD && papszSrcMD[iMD]; iMD++ )
    {
        if( EQUALN(papszSrcMD[iMD],"NITF_BLOCKA",11) 
            || EQUALN(papszSrcMD[iMD],"NITF_FHDR",9) )
        {
            char *pszName = NULL;
            const char *pszValue = CPLParseNameValue( papszSrcMD[iMD], 
                                                      &pszName );
            if( pszName != NULL &&
                CSLFetchNameValue( papszFullOptions, pszName+5 ) == NULL )
                papszFullOptions = 
                    CSLSetNameValue( papszFullOptions, pszName+5, pszValue );
            CPLFree(pszName);
        }
    }

/* -------------------------------------------------------------------- */
/*      Copy TRE definitions as creation options.                       */
/* -------------------------------------------------------------------- */
    papszSrcMD = poSrcDS->GetMetadata( "TRE" );

    for( iMD = 0; papszSrcMD && papszSrcMD[iMD]; iMD++ )
    {
        CPLString osTRE;

        if (EQUALN(papszSrcMD[iMD], "RPFHDR", 6) ||
            EQUALN(papszSrcMD[iMD], "RPFIMG", 6) ||
            EQUALN(papszSrcMD[iMD], "RPFDES", 6))
        {
            /* Do not copy RPF TRE. They contain absolute offsets */
            /* No chance that they make sense in the new NITF file */
            continue;
        }

        osTRE = "TRE=";
        osTRE += papszSrcMD[iMD];

        papszFullOptions = CSLAddString( papszFullOptions, osTRE );
    }

/* -------------------------------------------------------------------- */
/*      Set if we can set IREP.                                         */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue(papszFullOptions,"IREP") == NULL )
    {
        if ( ((poSrcDS->GetRasterCount() == 3 && bJPEG) ||
              (poSrcDS->GetRasterCount() >= 3 && !bJPEG)) && eType == GDT_Byte &&
             poSrcDS->GetRasterBand(1)->GetColorInterpretation() == GCI_RedBand &&
             poSrcDS->GetRasterBand(2)->GetColorInterpretation() == GCI_GreenBand &&
             poSrcDS->GetRasterBand(3)->GetColorInterpretation() == GCI_BlueBand)
        {
            if( bJPEG )
                papszFullOptions = 
                    CSLSetNameValue( papszFullOptions, "IREP", "YCbCr601" );
            else
                papszFullOptions = 
                    CSLSetNameValue( papszFullOptions, "IREP", "RGB" );
        }
        else if( poSrcDS->GetRasterCount() == 1 && eType == GDT_Byte
                 && poBand1->GetColorTable() != NULL )
        {
            papszFullOptions = 
                CSLSetNameValue( papszFullOptions, "IREP", "RGB/LUT" );
            papszFullOptions = 
                CSLSetNameValue( papszFullOptions, "LUT_SIZE", 
                                 CPLString().Printf(
                                     "%d", poBand1->GetColorTable()->GetColorEntryCount()) );
        }
        else if( GDALDataTypeIsComplex(eType) )
            papszFullOptions = 
                CSLSetNameValue( papszFullOptions, "IREP", "NODISPLY" );
        
        else
            papszFullOptions = 
                CSLSetNameValue( papszFullOptions, "IREP", "MONO" );
    }

/* -------------------------------------------------------------------- */
/*      Do we have lat/long georeferencing information?                 */
/* -------------------------------------------------------------------- */
    double adfGeoTransform[6];
    int    bWriteGeoTransform = FALSE;
    int    bWriteGCPs = FALSE;
    int    bNorth, nZone = 0;
    OGRSpatialReference oSRS, oSRS_WGS84;
    char *pszWKT = (char *) poSrcDS->GetProjectionRef();
    if( pszWKT == NULL || pszWKT[0] == '\0' )
        pszWKT = (char *) poSrcDS->GetGCPProjection();

    if( pszWKT != NULL && pszWKT[0] != '\0' )
    {
        oSRS.importFromWkt( &pszWKT );

        /* NITF is only WGS84 */
        oSRS_WGS84.SetWellKnownGeogCS( "WGS84" );
        if ( oSRS.IsSameGeogCS(&oSRS_WGS84) == FALSE)
        {
            CPLError((bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                    "NITF only supports WGS84 geographic and UTM projections.\n");
            if (bStrict)
            {
                CSLDestroy(papszFullOptions);
                CSLDestroy(papszCgmMD);
                CSLDestroy(papszTextMD);
                return NULL;
            }
        }

        const char* pszICORDS = CSLFetchNameValue(papszFullOptions, "ICORDS");

/* -------------------------------------------------------------------- */
/*      Should we write DIGEST Spatial Data Extension TRE ?             */
/* -------------------------------------------------------------------- */
        const char* pszSDE_TRE = CSLFetchNameValue(papszFullOptions, "SDE_TRE");
        int bSDE_TRE = pszSDE_TRE && CSLTestBoolean(pszSDE_TRE);
        if (bSDE_TRE)
        {
            if( oSRS.IsGeographic() && oSRS.GetPrimeMeridian() == 0.0
                && poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None &&
                adfGeoTransform[2] == 0.0 && adfGeoTransform[4] == 0.0 &&
                adfGeoTransform[5] < 0.0)
            {
                /* Override ICORDS to G if necessary */
                if (pszICORDS != NULL && EQUAL(pszICORDS, "D"))
                {
                    papszFullOptions =
                        CSLSetNameValue( papszFullOptions, "ICORDS", "G" );
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Forcing ICORDS=G when writing GEOLOB");
                }
                else
                {
                    /* Code a bit below will complain with other ICORDS value */
                }

                if (CSLPartialFindString(papszFullOptions, "TRE=GEOLOB=") != - 1)
                {
                    CPLDebug("NITF", "GEOLOB TRE was explicitely defined before. "
                             "Overriding it with current georefencing info.");
                }

                /* Structure of SDE TRE documented here */
                /*http://www.gwg.nga.mil/ntb/baseline/docs/digest/part2_annex_d.pdf */

/* -------------------------------------------------------------------- */
/*      Write GEOLOB TRE                                                */
/* -------------------------------------------------------------------- */
                char szGEOLOB[48+1];
                char* pszGEOLOB = szGEOLOB;
                double dfARV = 360.0 / adfGeoTransform[1];
                double dfBRV = 360.0 / -adfGeoTransform[5];
                double dfLSO = adfGeoTransform[0];
                double dfPSO = adfGeoTransform[3];
                sprintf(pszGEOLOB, "%09d", (int)(dfARV + 0.5)); pszGEOLOB += 9;
                sprintf(pszGEOLOB, "%09d", (int)(dfBRV + 0.5)); pszGEOLOB += 9;
                sprintf(pszGEOLOB, "%#+015.10f", dfLSO); pszGEOLOB += 15;
                sprintf(pszGEOLOB, "%#+015.10f", dfPSO); pszGEOLOB += 15;
                CPLAssert(pszGEOLOB == szGEOLOB + 48);

                CPLString osGEOLOB("TRE=GEOLOB=");
                osGEOLOB += szGEOLOB;
                papszFullOptions = CSLAddString( papszFullOptions, osGEOLOB ) ;

/* -------------------------------------------------------------------- */
/*      Write GEOPSB TRE if not already explicitely provided            */
/* -------------------------------------------------------------------- */
                if (CSLPartialFindString(papszFullOptions, "FILE_TRE=GEOPSB=") == -1 &&
                    CSLPartialFindString(papszFullOptions, "TRE=GEOPSB=") == -1)
                {
                    char szGEOPSB[443+1];
                    memset(szGEOPSB, ' ', 443);
                    szGEOPSB[443] = 0;
    #define WRITE_STR_NOSZ(dst, src) memcpy(dst, src, strlen(src))
                    char* pszGEOPSB = szGEOPSB;
                    WRITE_STR_NOSZ(pszGEOPSB, "GEO"); pszGEOPSB += 3;
                    WRITE_STR_NOSZ(pszGEOPSB, "DEG"); pszGEOPSB += 3;
                    WRITE_STR_NOSZ(pszGEOPSB, "World Geodetic System 1984"); pszGEOPSB += 80;
                    WRITE_STR_NOSZ(pszGEOPSB, "WGE"); pszGEOPSB += 4;
                    WRITE_STR_NOSZ(pszGEOPSB, "World Geodetic System 1984"); pszGEOPSB += 80;
                    WRITE_STR_NOSZ(pszGEOPSB, "WE"); pszGEOPSB += 3;
                    WRITE_STR_NOSZ(pszGEOPSB, "Geodetic"); pszGEOPSB += 80; /* DVR */
                    WRITE_STR_NOSZ(pszGEOPSB, "GEOD"); pszGEOPSB += 4; /* VDCDVR */
                    WRITE_STR_NOSZ(pszGEOPSB, "Mean Sea"); pszGEOPSB += 80; /* SDA */
                    WRITE_STR_NOSZ(pszGEOPSB, "MSL"); pszGEOPSB += 4; /* VDCSDA */
                    WRITE_STR_NOSZ(pszGEOPSB, "000000000000000"); pszGEOPSB += 15; /* ZOR */
                    pszGEOPSB += 3; /* GRD */
                    pszGEOPSB += 80; /* GRN */
                    WRITE_STR_NOSZ(pszGEOPSB, "0000"); pszGEOPSB += 4; /* ZNA */
                    CPLAssert(pszGEOPSB == szGEOPSB + 443);

                    CPLString osGEOPSB("FILE_TRE=GEOPSB=");
                    osGEOPSB += szGEOPSB;
                    papszFullOptions = CSLAddString( papszFullOptions, osGEOPSB ) ;
                }
                else
                {
                    CPLDebug("NITF", "GEOPSB TRE was explicitely defined before. Keeping it.");
                }

            }
            else
            {
                CPLError((bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                    "Georeferencing info isn't compatible with writing a GEOLOB TRE (only geographic SRS handled for now)");
                if (bStrict)
                {
                    CSLDestroy(papszFullOptions);
                    return NULL;
                }
            }
        }

        bWriteGeoTransform = ( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None );
        bWriteGCPs = ( !bWriteGeoTransform && poSrcDS->GetGCPCount() == 4 );

        if( oSRS.IsGeographic() && oSRS.GetPrimeMeridian() == 0.0 )
        {
            if (pszICORDS == NULL)
            {
                papszFullOptions = 
                    CSLSetNameValue( papszFullOptions, "ICORDS", "G" );
            }
            else if (EQUAL(pszICORDS, "G") || EQUAL(pszICORDS, "D"))
            {
                /* Do nothing */
            }
            else
            {
                CPLError((bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                    "Inconsistant ICORDS value with SRS : %s%s.\n", pszICORDS,
                    (!bStrict) ? ". Setting it to G instead" : "");
                if (bStrict)
                {
                    CSLDestroy(papszFullOptions);
                    return NULL;
                }
                papszFullOptions = 
                    CSLSetNameValue( papszFullOptions, "ICORDS", "G" );
            }
        }

        else if( oSRS.GetUTMZone( &bNorth ) > 0 )
        {
            if( bNorth )
                papszFullOptions = 
                    CSLSetNameValue( papszFullOptions, "ICORDS", "N" );
            else
                papszFullOptions = 
                    CSLSetNameValue( papszFullOptions, "ICORDS", "S" );

            nZone = oSRS.GetUTMZone( NULL );
        }
        else
        {
            CPLError((bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                    "NITF only supports WGS84 geographic and UTM projections.\n");
            if (bStrict)
            {
                CSLDestroy(papszFullOptions);
                CSLDestroy(papszCgmMD);
                CSLDestroy(papszTextMD);
                return NULL;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */
    int nXSize = poSrcDS->GetRasterXSize();
    int nYSize = poSrcDS->GetRasterYSize();
    const char *pszPVType = GDALToNITFDataType( eType );

    if( pszPVType == NULL )
    {
        CSLDestroy(papszFullOptions);
        CSLDestroy(papszCgmMD);
        CSLDestroy(papszTextMD);
        return NULL;
    }

    if (!NITFCreate( pszFilename, nXSize, nYSize, poSrcDS->GetRasterCount(),
                GDALGetDataTypeSize( eType ), pszPVType, 
                papszFullOptions ))
    {
        CSLDestroy( papszFullOptions );
        CSLDestroy(papszCgmMD);
        CSLDestroy(papszTextMD);
        return NULL;
    }

    CSLDestroy( papszFullOptions );
    papszFullOptions = NULL;

/* ==================================================================== */
/*      JPEG2000 case.  We need to write the data through a J2K         */
/*      driver in pixel interleaved form.                               */
/* ==================================================================== */
    if( bJPEG2000 )
    {
        NITFFile *psFile = NITFOpen( pszFilename, TRUE );
        if (psFile == NULL)
        {
            CSLDestroy(papszCgmMD);
            CSLDestroy(papszTextMD);
            return NULL;
        }

        GDALDataset *poJ2KDataset = NULL;
        GUIntBig nImageOffset = psFile->pasSegmentInfo[0].nSegmentStart;
        CPLString osDSName;

        NITFClose( psFile );

        osDSName.Printf( "/vsisubfile/" CPL_FRMT_GUIB "_%d,%s", 
                         nImageOffset, -1,
                         pszFilename );
                             
        if (EQUAL(poJ2KDriver->GetDescription(), "JP2ECW"))
        {
            char** papszJP2Options = NITFJP2ECWOptions(papszOptions);
            poJ2KDataset = 
                poJ2KDriver->CreateCopy( osDSName, poSrcDS, FALSE,
                                         papszJP2Options,
                                         pfnProgress, pProgressData );
            CSLDestroy(papszJP2Options);
        }
        else if (EQUAL(poJ2KDriver->GetDescription(), "JP2KAK"))
        {
           char** papszKAKOptions = NITFJP2KAKOptions(papszOptions);
            poJ2KDataset = 
                poJ2KDriver->CreateCopy( osDSName, poSrcDS, FALSE,
                                         papszKAKOptions,
                                         pfnProgress, pProgressData );
            CSLDestroy(papszKAKOptions);
            
        }
        else
        {
            /* Jasper case */
            const char* apszOptions[] = { "FORMAT=JPC", NULL };
            poJ2KDataset = 
                poJ2KDriver->CreateCopy( osDSName, poSrcDS, FALSE,
                                         (char **)apszOptions,
                                         pfnProgress, pProgressData );
        }
        if( poJ2KDataset == NULL )
        {
            CSLDestroy(papszCgmMD);
            CSLDestroy(papszTextMD);
            return NULL;
        }

        delete poJ2KDataset;

        // Now we need to figure out the actual length of the file
        // and correct the image segment size information.
        GIntBig nPixelCount = nXSize * ((GIntBig) nYSize) * 
            poSrcDS->GetRasterCount();

        NITFPatchImageLength( pszFilename, nImageOffset, nPixelCount, "C8" );
        NITFWriteCGMSegments( pszFilename, papszCgmMD );
        NITFWriteTextSegments( pszFilename, papszTextMD );

        GDALOpenInfo oOpenInfo( pszFilename, GA_Update );
        poDstDS = (NITFDataset *) Open( &oOpenInfo );

        if( poDstDS == NULL )
        {
            CSLDestroy(papszCgmMD);
            CSLDestroy(papszTextMD);
            return NULL;
        }
    }

/* ==================================================================== */
/*      Loop copying bands to an uncompressed file.                     */
/* ==================================================================== */
    else if( bJPEG )
    {
#ifdef JPEG_SUPPORTED
        NITFFile *psFile = NITFOpen( pszFilename, TRUE );
        if (psFile == NULL)
        {
            CSLDestroy(papszCgmMD);
            CSLDestroy(papszTextMD);
            return NULL;
        }
        GUIntBig nImageOffset = psFile->pasSegmentInfo[0].nSegmentStart;
        int bSuccess;
        
        bSuccess = 
            NITFWriteJPEGImage( poSrcDS, psFile->fp, nImageOffset,
                                papszOptions,
                                pfnProgress, pProgressData );
        
        if( !bSuccess )
        {
            NITFClose( psFile );
            CSLDestroy(papszCgmMD);
            CSLDestroy(papszTextMD);
            return NULL;
        }

        // Now we need to figure out the actual length of the file
        // and correct the image segment size information.
        GIntBig nPixelCount = nXSize * ((GIntBig) nYSize) * 
            poSrcDS->GetRasterCount();

        NITFClose( psFile );

        NITFPatchImageLength( pszFilename, nImageOffset,
                              nPixelCount, pszIC );

        NITFWriteCGMSegments( pszFilename, papszCgmMD );
        NITFWriteTextSegments( pszFilename, papszTextMD );
        
        GDALOpenInfo oOpenInfo( pszFilename, GA_Update );
        poDstDS = (NITFDataset *) Open( &oOpenInfo );

        if( poDstDS == NULL )
        {
            CSLDestroy(papszCgmMD);
            CSLDestroy(papszTextMD);
            return NULL;
        }
#endif /* def JPEG_SUPPORTED */
    }

/* ==================================================================== */
/*      Loop copying bands to an uncompressed file.                     */
/* ==================================================================== */
    else
    {
        NITFWriteCGMSegments( pszFilename, papszCgmMD );
        NITFWriteTextSegments( pszFilename, papszTextMD );

        GDALOpenInfo oOpenInfo( pszFilename, GA_Update );
        poDstDS = (NITFDataset *) Open( &oOpenInfo );
        if( poDstDS == NULL )
        {
            CSLDestroy(papszCgmMD);
            CSLDestroy(papszTextMD);
            return NULL;
        }
        
        void  *pData = VSIMalloc2(nXSize, (GDALGetDataTypeSize(eType) / 8));
        if (pData == NULL)
        {
            delete poDstDS;
            CSLDestroy(papszCgmMD);
            CSLDestroy(papszTextMD);
            return NULL;
        }
        
        CPLErr eErr = CE_None;

        for( int iBand = 0; eErr == CE_None && iBand < poSrcDS->GetRasterCount(); iBand++ )
        {
            GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand+1 );
            GDALRasterBand *poDstBand = poDstDS->GetRasterBand( iBand+1 );

/* -------------------------------------------------------------------- */
/*      Do we need to copy a colortable or other metadata?              */
/* -------------------------------------------------------------------- */
            GDALColorTable *poCT;

            poCT = poSrcBand->GetColorTable();
            if( poCT != NULL )
                poDstBand->SetColorTable( poCT );

/* -------------------------------------------------------------------- */
/*      Copy image data.                                                */
/* -------------------------------------------------------------------- */
            for( int iLine = 0; iLine < nYSize; iLine++ )
            {
                eErr = poSrcBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                            pData, nXSize, 1, eType, 0, 0 );
                if( eErr != CE_None )
                    break;   
                    
                eErr = poDstBand->RasterIO( GF_Write, 0, iLine, nXSize, 1, 
                                            pData, nXSize, 1, eType, 0, 0 );

                if( eErr != CE_None )
                    break;   

                if( !pfnProgress( (iBand + (iLine+1) / (double) nYSize)
                                  / (double) poSrcDS->GetRasterCount(), 
                                  NULL, pProgressData ) )
                {
                    CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
                    eErr = CE_Failure;
                    break;
                }
            }
        }

        CPLFree( pData );
        
        if ( eErr != CE_None )
        {
            delete poDstDS;
            CSLDestroy(papszCgmMD);
            CSLDestroy(papszTextMD);
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Set the georeferencing.                                         */
/* -------------------------------------------------------------------- */
    if( bWriteGeoTransform )
    {
        poDstDS->psImage->nZone = nZone;
        poDstDS->SetGeoTransform( adfGeoTransform );
    }
    else if( bWriteGCPs )
    {
        poDstDS->psImage->nZone = nZone;
        poDstDS->SetGCPs( poSrcDS->GetGCPCount(),
                          poSrcDS->GetGCPs(),
                          poSrcDS->GetGCPProjection() );
    }

    poDstDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );

    CSLDestroy(papszCgmMD);
    CSLDestroy(papszTextMD);

    return poDstDS;
}

/************************************************************************/
/*                        NITFPatchImageLength()                        */
/*                                                                      */
/*      Fixup various stuff we don't know till we have written the      */
/*      imagery.  In particular the file length, image data length      */
/*      and the compression ratio achieved.                             */
/************************************************************************/

static void NITFPatchImageLength( const char *pszFilename,
                                  GUIntBig nImageOffset,
                                  GIntBig nPixelCount,
                                  const char *pszIC )

{
    VSILFILE *fpVSIL = VSIFOpenL( pszFilename, "r+b" );
    if( fpVSIL == NULL )
        return;
    
    VSIFSeekL( fpVSIL, 0, SEEK_END );
    GUIntBig nFileLen = VSIFTellL( fpVSIL );

/* -------------------------------------------------------------------- */
/*      Update total file length.                                       */
/* -------------------------------------------------------------------- */
    if (nFileLen >= (GUIntBig)(1e12 - 1))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too big file : " CPL_FRMT_GUIB ". Truncating to 999999999998",
                 nFileLen);
        nFileLen = (GUIntBig)(1e12 - 2);
    }
    VSIFSeekL( fpVSIL, 342, SEEK_SET );
    CPLString osLen = CPLString().Printf("%012" CPL_FRMT_GB_WITHOUT_PREFIX "u",nFileLen);
    VSIFWriteL( (void *) osLen.c_str(), 1, 12, fpVSIL );
    
/* -------------------------------------------------------------------- */
/*      Update the image data length.                                   */
/* -------------------------------------------------------------------- */
    GUIntBig nImageSize = nFileLen-nImageOffset;
    if (GUINTBIG_TO_DOUBLE(nImageSize) >= 1e10 - 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too big image size : " CPL_FRMT_GUIB". Truncating to 9999999998",
                 nImageSize);
        nImageSize = (GUIntBig)(1e10 - 2);
    }
    VSIFSeekL( fpVSIL, 369, SEEK_SET );
    osLen = CPLString().Printf("%010" CPL_FRMT_GB_WITHOUT_PREFIX "u",nImageSize);
    VSIFWriteL( (void *) osLen.c_str(), 1, 10, fpVSIL );

/* -------------------------------------------------------------------- */
/*      Update COMRAT, the compression rate variable.  We have to       */
/*      take into account the presence of graphic and text segments,    */
/*      the optional presence of IGEOLO and ICOM to find its position.  */
/* -------------------------------------------------------------------- */
    char szICBuf[2];
    char achNUM[4]; // buffer for segment size.  3 digits plus null character
    achNUM[3] = '\0';

    // get number of graphic and text segment so we can calculate offset for
    // image IC.
    int nNumIOffset = 360;
    VSIFSeekL( fpVSIL, nNumIOffset, SEEK_SET );
    VSIFReadL( achNUM, 1, 3, fpVSIL );
    int nIM = atoi(achNUM); // number of image segment

    int nNumSOffset = nNumIOffset + 3 + nIM * 16;
    VSIFSeekL( fpVSIL,  nNumSOffset, SEEK_SET );
    VSIFReadL( achNUM, 1, 3, fpVSIL );
    int nGS = atoi(achNUM); // number of graphic segment

    int nNumTOffset = nNumSOffset + 3 + 10 * nGS + 3;
    VSIFSeekL( fpVSIL, nNumTOffset, SEEK_SET );
    VSIFReadL( achNUM, 1, 3, fpVSIL );
    int nTS = atoi(achNUM); // number of text segment

    int nAdditionalOffset = nGS * 10 + nTS * 9;

    /* Read ICORDS */
    VSIFSeekL( fpVSIL, 775 + nAdditionalOffset , SEEK_SET );
    char chICORDS;
    VSIFReadL( &chICORDS, 1, 1, fpVSIL );
    if (chICORDS != ' ')
        VSIFSeekL( fpVSIL, 60, SEEK_CUR); /* skip IGEOLO */

    /* Read NICOM */
    char achNICOM[2];
    VSIFReadL( achNICOM, 1, 1, fpVSIL );
    achNICOM[1] = 0;
    int nNICOM = atoi(achNICOM);
    VSIFSeekL( fpVSIL, nNICOM * 80, SEEK_CUR); /* skip comments */

    /* Read IC */
    VSIFReadL( szICBuf, 2, 1, fpVSIL );

    /* The following line works around a "feature" of *BSD libc (at least PC-BSD 7.1) */
    /* that makes the position of the file offset unreliable when executing a */
    /* "seek, read and write" sequence. After the read(), the file offset seen by */
    /* the write() is approximatively the size of a block further... */
    VSIFSeekL( fpVSIL, VSIFTellL( fpVSIL ), SEEK_SET );
    
    if( !EQUALN(szICBuf,pszIC,2) )
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "Unable to locate COMRAT to update in NITF header." );
    }
    else
    {
        char szCOMRAT[5];

        if( EQUAL(pszIC,"C8") ) /* jpeg2000 */
        {
            double dfRate = (GIntBig)(nFileLen-nImageOffset) * 8 / (double) nPixelCount;
            dfRate = MAX(0.01,MIN(99.99,dfRate));
        
            // We emit in wxyz format with an implicit decimal place
            // between wx and yz as per spec for lossy compression. 
            // We really should have a special case for lossless compression.
            sprintf( szCOMRAT, "%04d", (int) (dfRate * 100));
        }
        else if( EQUAL(pszIC, "C3") || EQUAL(pszIC, "M3") ) /* jpeg */
        {
            strcpy( szCOMRAT, "00.0" );
        }

        VSIFWriteL( szCOMRAT, 4, 1, fpVSIL );
    }
    
    VSIFCloseL( fpVSIL );
}

/************************************************************************/
/*                       NITFWriteCGMSegments()                        */
/************************************************************************/
static int NITFWriteCGMSegments( const char *pszFilename, char **papszList)
{
    char errorMessage[255] = "";

    // size of each Cgm header entry (LS (4) + LSSH (6))
    const int nCgmHdrEntrySz = 10;
    
    if (papszList == NULL)
        return TRUE;

    int nNUMS = 0;
    const char *pszNUMS;
    pszNUMS = CSLFetchNameValue(papszList, "SEGMENT_COUNT");
    if (pszNUMS != NULL)
    {
        nNUMS = atoi(pszNUMS);
    }

    /* -------------------------------------------------------------------- */
    /*      Open the target file.                                           */
    /* -------------------------------------------------------------------- */
    VSILFILE *fpVSIL = VSIFOpenL(pszFilename, "r+b");

    if (fpVSIL == NULL)
        return FALSE;

    // Calculates the offset for NUMS so we can update header data
    char achNUMI[4]; // 3 digits plus null character
    achNUMI[3] = '\0';

    // NUMI offset is at a fixed offset 363
    int nNumIOffset = 360;
    VSIFSeekL(fpVSIL, nNumIOffset, SEEK_SET );
    VSIFReadL(achNUMI, 1, 3, fpVSIL);
    int nIM = atoi(achNUMI);

    // 6 for size of LISH and 10 for size of LI
    // NUMS offset is NumI offset plus the size of NumI + size taken up each
    // the header data multiply by the number of data

    int nNumSOffset = nNumIOffset + 3+ nIM * (6 + 10);

    /* -------------------------------------------------------------------- */
    /*      Confirm that the NUMS in the file header already matches the    */
    /*      number of graphic segments we want to write                     */
    /* -------------------------------------------------------------------- */
    char achNUMS[4];

    VSIFSeekL( fpVSIL, nNumSOffset, SEEK_SET );
    VSIFReadL( achNUMS, 1, 3, fpVSIL );
    achNUMS[3] = '\0';

    if( atoi(achNUMS) != nNUMS )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "It appears an attempt was made to add or update graphic\n"
                  "segments on an NITF file with existing segments.  This\n"
                  "is not currently supported by the GDAL NITF driver." );

        VSIFCloseL( fpVSIL );
        return FALSE;
    }


    // allocate space for graphic header.
    // Size of LS = 4, size of LSSH = 6, and 1 for null character
    char *pachLS = (char *) CPLCalloc(nNUMS * nCgmHdrEntrySz + 1, 1);

    /* -------------------------------------------------------------------- */
    /*	Assume no extended data such as SXSHDL, SXSHD						*/
    /* -------------------------------------------------------------------- */

    /* ==================================================================== */
    /*      Write the Graphics segments at the end of the file.             */
    /* ==================================================================== */

    #define PLACE(location,name,text)  strncpy(location,text,strlen(text))

    for (int i = 0; i < nNUMS; i++)
    {

        // Get all the fields for current CGM segment
        const char *pszSlocRow = CSLFetchNameValue(papszList,
                        CPLString().Printf("SEGMENT_%d_SLOC_ROW", i));
        const char *pszSlocCol = CSLFetchNameValue(papszList,
                        CPLString().Printf("SEGMENT_%d_SLOC_COL", i));
        const char *pszSdlvl = CSLFetchNameValue(papszList,
                        CPLString().Printf("SEGMENT_%d_SDLVL", i));
        const char *pszSalvl = CSLFetchNameValue(papszList,
                        CPLString().Printf("SEGMENT_%d_SALVL", i));
        const char *pszData = CSLFetchNameValue(papszList,
                        CPLString().Printf("SEGMENT_%d_DATA", i));

        // Error checking
        if (pszSlocRow == NULL)
        {
            sprintf(errorMessage, "NITF graphic segment writing error: SLOC_ROW for segment %d is not defined",i);
            break;
        }
        if (pszSlocCol == NULL)
        {
            sprintf(errorMessage, "NITF graphic segment writing error: SLOC_COL for segment %d is not defined",i);
            break;
        }
        if (pszSdlvl == NULL)
        {
            sprintf(errorMessage, "NITF graphic segment writing error: SDLVL for segment %d is not defined", i);
            break;
        }
        if (pszSalvl == NULL)
        {
            sprintf(errorMessage, "NITF graphic segment writing error: SALVLfor segment %d is not defined", i);
            break;
        }
        if (pszData == NULL)
        {
            sprintf(errorMessage, "NITF graphic segment writing error: DATA for segment %d is not defined", i);
            break;
        }

        int nSlocCol = atoi(pszSlocRow);
        int nSlocRow = atoi(pszSlocCol);
        int nSdlvl = atoi(pszSdlvl);
        int nSalvl = atoi(pszSalvl);

        // Create a buffer for graphics segment header, 258 is the size of
        // the header that we will be writing.
        char achGSH[258];

        memset(achGSH, ' ', sizeof(achGSH));


        PLACE( achGSH+ 0, SY , "SY" );
        PLACE( achGSH+ 2, SID ,CPLSPrintf("%010d", i) );
        PLACE( achGSH+ 12, SNAME , "DEFAULT NAME        " );
        PLACE( achGSH+32, SSCLAS , "U" );
        PLACE( achGSH+33, SSCLASY , "0" );
        PLACE( achGSH+199, ENCRYP , "0" );
        PLACE( achGSH+200, SFMT , "C" );
        PLACE( achGSH+201, SSTRUCT , "0000000000000" );
        PLACE( achGSH+214, SDLVL , CPLSPrintf("%03d",nSdlvl)); // size3
        PLACE( achGSH+217, SALVL , CPLSPrintf("%03d",nSalvl)); // size3
        PLACE( achGSH+220, SLOC , CPLSPrintf("%05d%05d",nSlocRow,nSlocCol) ); // size 10
        PLACE( achGSH+230, SBAND1 , "0000000000" );
        PLACE( achGSH+240, SCOLOR, "C" );
        PLACE( achGSH+241, SBAND2, "0000000000" );
        PLACE( achGSH+251, SRES2, "00" );
        PLACE( achGSH+253, SXSHDL, "00000" );

        // Move to the end of the file
        VSIFSeekL(fpVSIL, 0, SEEK_END );
        VSIFWriteL(achGSH, 1, sizeof(achGSH), fpVSIL);

        /* -------------------------------------- ------------------------------ */
        /*      Prepare and write CGM segment data.                            */
        /* -------------------------------------------------------------------- */
        int nCGMSize = 0;
        char *pszCgmToWrite = CPLUnescapeString(pszData, &nCGMSize,
                        CPLES_BackslashQuotable);

        if (nCGMSize > 999998)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Length of SEGMENT_%d_DATA is %d, which is greater than 999998. Truncating...",
                     i + 1, nCGMSize);
            nCGMSize = 999998;
        }

        VSIFWriteL(pszCgmToWrite, 1, nCGMSize, fpVSIL);

        /* -------------------------------------------------------------------- */
        /*      Update the subheader and data size info in the file header.     */
        /* -------------------------------------------------------------------- */
        sprintf( pachLS + nCgmHdrEntrySz * i, "%04d%06d",(int) sizeof(achGSH), nCGMSize );

        CPLFree(pszCgmToWrite);

    } // End For


    /* -------------------------------------------------------------------- */
    /*      Write out the graphic segment info.                             */
    /* -------------------------------------------------------------------- */

    VSIFSeekL(fpVSIL, nNumSOffset + 3, SEEK_SET );
    VSIFWriteL(pachLS, 1, nNUMS * nCgmHdrEntrySz, fpVSIL);

    /* -------------------------------------------------------------------- */
    /*      Update total file length.                                       */
    /* -------------------------------------------------------------------- */
    VSIFSeekL(fpVSIL, 0, SEEK_END );
    GUIntBig nFileLen = VSIFTellL(fpVSIL);
    // Offset to file length entry
    VSIFSeekL(fpVSIL, 342, SEEK_SET );
    if (GUINTBIG_TO_DOUBLE(nFileLen) >= 1e12 - 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                        "Too big file : " CPL_FRMT_GUIB ". Truncating to 999999999998",
                        nFileLen);
        nFileLen = (GUIntBig) (1e12 - 2);
    }
    CPLString osLen = CPLString().Printf("%012" CPL_FRMT_GB_WITHOUT_PREFIX "u",
                    nFileLen);
    VSIFWriteL((void *) osLen.c_str(), 1, 12, fpVSIL);

    VSIFCloseL(fpVSIL);

    CPLFree(pachLS);

    if (strlen(errorMessage) != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", errorMessage);
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                       NITFWriteTextSegments()                        */
/************************************************************************/

static void NITFWriteTextSegments( const char *pszFilename,
                                   char **papszList )

{
/* -------------------------------------------------------------------- */
/*      Count the number of apparent text segments to write.  There     */
/*      is nothing at all to do if there are none to write.             */
/* -------------------------------------------------------------------- */
    int iOpt, nNUMT = 0;

    for( iOpt = 0; papszList != NULL && papszList[iOpt] != NULL; iOpt++ )
    {
        if( EQUALN(papszList[iOpt],"DATA_",5) )
            nNUMT++;
    }

    if( nNUMT == 0 )
        return;

/* -------------------------------------------------------------------- */
/*      Open the target file.                                           */
/* -------------------------------------------------------------------- */
    VSILFILE *fpVSIL = VSIFOpenL( pszFilename, "r+b" );

    if( fpVSIL == NULL )
        return;

    // Get number of text field.  Since there there could be multiple images
    // or graphic segment, the  offset need to be calculated dynamically.

    char achNUMI[4]; // 3 digits plus null character
    achNUMI[3] = '\0';
    // NUMI offset is at a fixed offset 363
    int nNumIOffset = 360;
    VSIFSeekL( fpVSIL, nNumIOffset, SEEK_SET );
    VSIFReadL( achNUMI, 1, 3, fpVSIL );
    int nIM = atoi(achNUMI);

    char achNUMG[4]; // 3 digits plus null character
    achNUMG[3] = '\0';

    // 3 for size of NUMI.  6 and 10 are the field size for LISH and LI
    int nNumGOffset = nNumIOffset + 3 + nIM * (6 + 10);
    VSIFSeekL( fpVSIL, nNumGOffset, SEEK_SET );
    VSIFReadL( achNUMG, 1, 3, fpVSIL );
    int nGS = atoi(achNUMG);

    // NUMT offset
    // 3 for size of NUMG.  4 and 6 are filed size of LSSH and LS.
    // the last + 3 is for NUMX field, which is not used
    int nNumTOffset = nNumGOffset + 3 + nGS * (4 + 6) + 3;

    /* -------------------------------------------------------------------- */
    /*      Confirm that the NUMT in the file header already matches the    */
    /*      number of text segements we want to write, and that the         */
    /*      segment header/data size info is blank.                         */
    /* -------------------------------------------------------------------- */
    char achNUMT[4];
    char *pachLT = (char *) CPLCalloc(nNUMT * 9 + 1, 1);

    VSIFSeekL( fpVSIL, nNumTOffset, SEEK_SET );
    VSIFReadL( achNUMT, 1, 3, fpVSIL );
    achNUMT[3] = '\0';

    VSIFReadL( pachLT, 1, nNUMT * 9, fpVSIL );

    if( atoi(achNUMT) != nNUMT )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "It appears an attempt was made to add or update text\n"
                  "segments on an NITF file with existing segments.  This\n"
                  "is not currently supported by the GDAL NITF driver." );

        VSIFCloseL( fpVSIL );
        CPLFree( pachLT );
        return;
    }

    if( !EQUALN(pachLT,"         ",9) )
    {
        CPLFree( pachLT );
        // presumably the text segments are already written, do nothing.
        VSIFCloseL( fpVSIL );
        return;
    }

/* -------------------------------------------------------------------- */
/*      At this point we likely ought to confirm NUMDES, NUMRES,        */
/*      UDHDL and XHDL are zero.  Consider adding later...              */
/* -------------------------------------------------------------------- */

/* ==================================================================== */
/*      Write the text segments at the end of the file.                 */
/* ==================================================================== */
#define PLACE(location,name,text)  strncpy(location,text,strlen(text))
    int iTextSeg = 0;
    
    for( iOpt = 0; papszList != NULL && papszList[iOpt] != NULL; iOpt++ )
    {
        const char *pszTextToWrite;

        if( !EQUALN(papszList[iOpt],"DATA_",5) )
            continue;

        const char *pszHeaderBuffer = NULL;

        pszTextToWrite = CPLParseNameValue( papszList[iOpt], NULL );
        if( pszTextToWrite == NULL )
            continue;

/* -------------------------------------------------------------------- */
/*      Locate corresponding header data in the buffer                  */
/* -------------------------------------------------------------------- */

        for( int iOpt2 = 0; papszList != NULL && papszList[iOpt2] != NULL; iOpt2++ ) {
            if( !EQUALN(papszList[iOpt2],"HEADER_",7) )
                continue;

            char *pszHeaderKey = NULL, *pszDataKey = NULL;
            CPLParseNameValue( papszList[iOpt2], &pszHeaderKey );
            CPLParseNameValue( papszList[iOpt], &pszDataKey );
            if( pszHeaderKey == NULL || pszDataKey == NULL )
            {
                CPLFree(pszHeaderKey);
                CPLFree(pszDataKey);
                continue;
            }

            char *pszHeaderId, *pszDataId; //point to header and data number
            pszHeaderId = pszHeaderKey + 7;
            pszDataId = pszDataKey + 5;

            bool bIsSameId = strcmp(pszHeaderId, pszDataId) == 0;
            CPLFree(pszHeaderKey);
            CPLFree(pszDataKey);

            // if ID matches, read the header information and exit the loop
            if (bIsSameId) {
            	pszHeaderBuffer = CPLParseNameValue( papszList[iOpt2], NULL);
            	break;
            }
        }

/* -------------------------------------------------------------------- */
/*      Prepare and write text header.                                  */
/* -------------------------------------------------------------------- */
        char achTSH[282];
        memset( achTSH, ' ', sizeof(achTSH) );
        VSIFSeekL( fpVSIL, 0, SEEK_END );

        if (pszHeaderBuffer!= NULL) {
            memcpy( achTSH, pszHeaderBuffer, MIN(strlen(pszHeaderBuffer), sizeof(achTSH)) );

            // Take care NITF2.0 date format changes
            char chTimeZone = achTSH[20];

            // Check for Zulu time zone character.  IpachLTf that exist, then
            // it's NITF2.0 format.
            if (chTimeZone == 'Z') {
                char *achOrigDate=achTSH+12;  // original date string

                // The date value taken from default NITF file date
                char achNewDate[]="20021216151629";
                char achYear[3];
                int nYear;

                // Offset to the year
                strncpy(achYear,achOrigDate+12, 2);
                achYear[2] = '\0';
                nYear = atoi(achYear);

                // Set century.
                // Since NITF2.0 does not track the century, we are going to
                // assume any year number greater then 94 (the year NITF2.0
                // spec published), will be 1900s, otherwise, it's 2000s.
                if (nYear > 94) strncpy(achNewDate,"19",2);
                else strncpy(achNewDate,"20",2);

                strncpy(achNewDate+6, achOrigDate,8); // copy cover DDhhmmss
                strncpy(achNewDate+2, achOrigDate+12,2); // copy over years

                // Perform month conversion
                char *pszOrigMonth = achOrigDate+9;
                char *pszNewMonth = achNewDate+4;

                if (strncmp(pszOrigMonth,"JAN",3) == 0) strncpy(pszNewMonth,"01",2);
                else if (strncmp(pszOrigMonth,"FEB",3) == 0) strncpy(pszNewMonth,"02",2);
                else if (strncmp(pszOrigMonth,"MAR",3) == 0) strncpy(pszNewMonth,"03",2);
                else if (strncmp(pszOrigMonth,"APR",3) == 0) strncpy(pszNewMonth,"04",2);
                else if (strncmp(pszOrigMonth,"MAY",3) == 0) strncpy(pszNewMonth,"05",2);
                else if (strncmp(pszOrigMonth,"JUN",3) == 0) strncpy(pszNewMonth,"07",2);
                else if (strncmp(pszOrigMonth,"AUG",3) == 0) strncpy(pszNewMonth,"08",2);
                else if (strncmp(pszOrigMonth,"SEP",3) == 0) strncpy(pszNewMonth,"09",2);
                else if (strncmp(pszOrigMonth,"OCT",3) == 0) strncpy(pszNewMonth,"10",2);
                else if (strncmp(pszOrigMonth,"NOV",3) == 0) strncpy(pszNewMonth,"11",2);
                else if (strncmp(pszOrigMonth,"DEC",3) == 0) strncpy(pszNewMonth,"12",2);

                PLACE( achTSH+ 12, TXTDT         , achNewDate          		);

            }
        } else { // Use default value if header information is not found
            PLACE( achTSH+  0, TE            , "TE"                          );
            PLACE( achTSH+  9, TXTALVL       , "000"                         );
            PLACE( achTSH+ 12, TXTDT         , "20021216151629"              );
            PLACE( achTSH+106, TSCLAS        , "U"                           );
            PLACE( achTSH+273, ENCRYP        , "0"                           );
            PLACE( achTSH+274, TXTFMT        , "STA"                         );
            PLACE( achTSH+277, TXSHDL        , "00000"                       );
        }


        VSIFWriteL( achTSH, 1, sizeof(achTSH), fpVSIL );

/* -------------------------------------------------------------------- */
/*      Prepare and write text segment data.                            */
/* -------------------------------------------------------------------- */

        int nTextLength = (int) strlen(pszTextToWrite);
        if (nTextLength > 99998)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Length of DATA_%d is %d, which is greater than 99998. Truncating...",
                     iTextSeg + 1, nTextLength);
            nTextLength = 99998;
        }

        VSIFWriteL( pszTextToWrite, 1, nTextLength, fpVSIL );
        
/* -------------------------------------------------------------------- */
/*      Update the subheader and data size info in the file header.     */
/* -------------------------------------------------------------------- */
        sprintf( pachLT + 9*iTextSeg+0, "%04d%05d",
                 (int) sizeof(achTSH), nTextLength );

        iTextSeg++;
    }

/* -------------------------------------------------------------------- */
/*      Write out the text segment info.                                */
/* -------------------------------------------------------------------- */

    VSIFSeekL( fpVSIL, nNumTOffset + 3, SEEK_SET );
    VSIFWriteL( pachLT, 1, nNUMT * 9, fpVSIL );

/* -------------------------------------------------------------------- */
/*      Update total file length.                                       */
/* -------------------------------------------------------------------- */
    VSIFSeekL( fpVSIL, 0, SEEK_END );
    GUIntBig nFileLen = VSIFTellL( fpVSIL );

    VSIFSeekL( fpVSIL, 342, SEEK_SET );
    if (GUINTBIG_TO_DOUBLE(nFileLen) >= 1e12 - 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too big file : " CPL_FRMT_GUIB ". Truncating to 999999999998",
                 nFileLen);
        nFileLen = (GUIntBig)(1e12 - 2);
    }
    CPLString osLen = CPLString().Printf("%012" CPL_FRMT_GB_WITHOUT_PREFIX "u",nFileLen);
    VSIFWriteL( (void *) osLen.c_str(), 1, 12, fpVSIL );
    
    VSIFCloseL( fpVSIL );
    CPLFree( pachLT );
}
        
/************************************************************************/
/*                         NITFWriteJPEGImage()                         */
/************************************************************************/

#ifdef JPEG_SUPPORTED

int 
NITFWriteJPEGBlock( GDALDataset *poSrcDS, VSILFILE *fp,
                    int nBlockXOff, int nBlockYOff,
                    int nBlockXSize, int nBlockYSize,
                    int bProgressive, int nQuality,
                    const GByte* pabyAPP6, int nRestartInterval,
                    GDALProgressFunc pfnProgress, void * pProgressData );

static int 
NITFWriteJPEGImage( GDALDataset *poSrcDS, VSILFILE *fp, vsi_l_offset nStartOffset,
                    char **papszOptions,
                    GDALProgressFunc pfnProgress, void * pProgressData )
{
    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();
    int  nQuality = 75;
    int  bProgressive = FALSE;
    int  nRestartInterval = -1;

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Some some rudimentary checks                                    */
/* -------------------------------------------------------------------- */
    if( nBands != 1 && nBands != 3 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "JPEG driver doesn't support %d bands.  Must be 1 (grey) "
                  "or 3 (RGB) bands.\n", nBands );

        return FALSE;
    }

    GDALDataType eDT = poSrcDS->GetRasterBand(1)->GetRasterDataType();

#if defined(JPEG_LIB_MK1) || defined(JPEG_DUAL_MODE_8_12)
    if( eDT != GDT_Byte && eDT != GDT_UInt16 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "JPEG driver doesn't support data type %s. "
                  "Only eight and twelve bit bands supported (Mk1 libjpeg).\n",
                  GDALGetDataTypeName( 
                      poSrcDS->GetRasterBand(1)->GetRasterDataType()) );

        return FALSE;
    }

    if( eDT == GDT_UInt16 || eDT == GDT_Int16 )
        eDT = GDT_UInt16;
    else
        eDT = GDT_Byte;

#else
    if( eDT != GDT_Byte )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "JPEG driver doesn't support data type %s. "
                  "Only eight bit byte bands supported.\n", 
                  GDALGetDataTypeName( 
                      poSrcDS->GetRasterBand(1)->GetRasterDataType()) );

        return FALSE;
    }
    
    eDT = GDT_Byte; // force to 8bit. 
#endif

/* -------------------------------------------------------------------- */
/*      What options has the user selected?                             */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue(papszOptions,"QUALITY") != NULL )
    {
        nQuality = atoi(CSLFetchNameValue(papszOptions,"QUALITY"));
        if( nQuality < 10 || nQuality > 100 )
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "QUALITY=%s is not a legal value in the range 10-100.",
                      CSLFetchNameValue(papszOptions,"QUALITY") );
            return FALSE;
        }
    }

    if( CSLFetchNameValue(papszOptions,"RESTART_INTERVAL") != NULL )
    {
        nRestartInterval = atoi(CSLFetchNameValue(papszOptions,"RESTART_INTERVAL"));
    }

    bProgressive = CSLFetchBoolean( papszOptions, "PROGRESSIVE", FALSE );

/* -------------------------------------------------------------------- */
/*      Compute blocking factors                                        */
/* -------------------------------------------------------------------- */
    int nNPPBH = nXSize;
    int nNPPBV = nYSize;

    if( CSLFetchNameValue( papszOptions, "BLOCKSIZE" ) != NULL )
        nNPPBH = nNPPBV = atoi(CSLFetchNameValue( papszOptions, "BLOCKSIZE" ));

    if( CSLFetchNameValue( papszOptions, "BLOCKXSIZE" ) != NULL )
        nNPPBH = atoi(CSLFetchNameValue( papszOptions, "BLOCKXSIZE" ));

    if( CSLFetchNameValue( papszOptions, "BLOCKYSIZE" ) != NULL )
        nNPPBV = atoi(CSLFetchNameValue( papszOptions, "BLOCKYSIZE" ));
    
    if( CSLFetchNameValue( papszOptions, "NPPBH" ) != NULL )
        nNPPBH = atoi(CSLFetchNameValue( papszOptions, "NPPBH" ));
    
    if( CSLFetchNameValue( papszOptions, "NPPBV" ) != NULL )
        nNPPBV = atoi(CSLFetchNameValue( papszOptions, "NPPBV" ));
    
    if( nNPPBH <= 0 || nNPPBV <= 0 ||
        nNPPBH > 9999 || nNPPBV > 9999  )
        nNPPBH = nNPPBV = 256;

    int nNBPR = (nXSize + nNPPBH - 1) / nNPPBH;
    int nNBPC = (nYSize + nNPPBV - 1) / nNPPBV;

/* -------------------------------------------------------------------- */
/*  Creates APP6 NITF application segment (required by MIL-STD-188-198) */
/*  see #3345                                                           */
/* -------------------------------------------------------------------- */
    GByte abyAPP6[23];
    GUInt16 nUInt16;
    int nOffset = 0;

    memcpy(abyAPP6, "NITF", 4);
    abyAPP6[4] = 0;
    nOffset += 5;

    /* Version : 2.0 */
    nUInt16 = 0x0200;
    CPL_MSBPTR16(&nUInt16);
    memcpy(abyAPP6 + nOffset, &nUInt16, sizeof(nUInt16));
    nOffset += sizeof(nUInt16);

    /* IMODE */
    abyAPP6[nOffset] = (nBands == 1) ? 'B' : 'P';
    nOffset ++;

    /* Number of image blocks per row */
    nUInt16 = (GUInt16) nNBPR;
    CPL_MSBPTR16(&nUInt16);
    memcpy(abyAPP6 + nOffset, &nUInt16, sizeof(nUInt16));
    nOffset += sizeof(nUInt16);

    /* Number of image blocks per column */
    nUInt16 = (GUInt16) nNBPC;
    CPL_MSBPTR16(&nUInt16);
    memcpy(abyAPP6 + nOffset, &nUInt16, sizeof(nUInt16));
    nOffset += sizeof(nUInt16);

    /* Image color */
    abyAPP6[nOffset] = (nBands == 1) ? 0 : 1;
    nOffset ++;

    /* Original sample precision */
    abyAPP6[nOffset] = (eDT == GDT_UInt16) ? 12 : 8;
    nOffset ++;

    /* Image class */
    abyAPP6[nOffset] = 0;
    nOffset ++;

    /* JPEG coding process */
    abyAPP6[nOffset] = (eDT == GDT_UInt16) ? 4 : 1;
    nOffset ++;

    /* Quality */
    abyAPP6[nOffset] = 0;
    nOffset ++;

    /* Stream color */
    abyAPP6[nOffset] = (nBands == 1) ? 0 /* Monochrome */ : 2 /* YCbCr*/ ;
    nOffset ++;

    /* Stream bits */
    abyAPP6[nOffset] = (eDT == GDT_UInt16) ? 12 : 8;
    nOffset ++;

    /* Horizontal filtering */
    abyAPP6[nOffset] = 1;
    nOffset ++;

    /* Vertical filtering */
    abyAPP6[nOffset] = 1;
    nOffset ++;

    /* Reserved */
    abyAPP6[nOffset] = 0;
    nOffset ++;
    abyAPP6[nOffset] = 0;
    nOffset ++;

    CPLAssert(nOffset == sizeof(abyAPP6));

/* -------------------------------------------------------------------- */
/*      Prepare block map if necessary                                  */
/* -------------------------------------------------------------------- */

    VSIFSeekL( fp, nStartOffset, SEEK_SET );

    const char* pszIC = CSLFetchNameValue( papszOptions, "IC" );
    GUInt32  nIMDATOFF = 0;
    if (EQUAL(pszIC, "M3"))
    {
        GUInt32  nIMDATOFF_MSB;
        GUInt16  nBMRLNTH, nTMRLNTH, nTPXCDLNTH;

        /* Prepare the block map */
#define BLOCKMAP_HEADER_SIZE    (4 + 2 + 2 + 2)
        nIMDATOFF_MSB = nIMDATOFF = BLOCKMAP_HEADER_SIZE + nNBPC * nNBPR * 4;
        nBMRLNTH = 4;
        nTMRLNTH = 0;
        nTPXCDLNTH = 0;

        CPL_MSBPTR32( &nIMDATOFF_MSB );
        CPL_MSBPTR16( &nBMRLNTH );
        CPL_MSBPTR16( &nTMRLNTH );
        CPL_MSBPTR16( &nTPXCDLNTH );

        VSIFWriteL( &nIMDATOFF_MSB, 1, 4, fp );
        VSIFWriteL( &nBMRLNTH, 1, 2, fp );
        VSIFWriteL( &nTMRLNTH, 1, 2, fp );
        VSIFWriteL( &nTPXCDLNTH, 1, 2, fp );

        /* Reserve space for the table itself */
        VSIFSeekL( fp, nNBPC * nNBPR * 4, SEEK_CUR );
    }

/* -------------------------------------------------------------------- */
/*      Copy each block                                                 */
/* -------------------------------------------------------------------- */
    int nBlockXOff, nBlockYOff;
    for(nBlockYOff=0;nBlockYOff<nNBPC;nBlockYOff++)
    {
        for(nBlockXOff=0;nBlockXOff<nNBPR;nBlockXOff++)
        {
            /*CPLDebug("NITF", "nBlockXOff=%d/%d, nBlockYOff=%d/%d",
                     nBlockXOff, nNBPR, nBlockYOff, nNBPC);*/
            if (EQUAL(pszIC, "M3"))
            {
                /* Write block offset for current block */

                GUIntBig nCurPos = VSIFTellL(fp);
                VSIFSeekL( fp, nStartOffset + BLOCKMAP_HEADER_SIZE + 4 * (nBlockYOff * nNBPR + nBlockXOff), SEEK_SET );
                GUIntBig nBlockOffset = nCurPos - nStartOffset - nIMDATOFF;
                GUInt32 nBlockOffset32 = (GUInt32)nBlockOffset;
                if (nBlockOffset == (GUIntBig)nBlockOffset32)
                {
                    CPL_MSBPTR32( &nBlockOffset32 );
                    VSIFWriteL( &nBlockOffset32, 1, 4, fp );
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                            "Offset for block (%d, %d) = " CPL_FRMT_GUIB ". Cannot fit into 32 bits...",
                            nBlockXOff, nBlockYOff, nBlockOffset);

                    nBlockOffset32 = 0xffffffff;
                    int i;
                    for(i=nBlockYOff * nNBPR + nBlockXOff; i < nNBPC * nNBPR; i++)
                    {
                        VSIFWriteL( &nBlockOffset32, 1, 4, fp );
                    }
                    return FALSE;
                }
                VSIFSeekL( fp, nCurPos, SEEK_SET );
            }

            if (!NITFWriteJPEGBlock(poSrcDS, fp,
                                    nBlockXOff, nBlockYOff,
                                    nNPPBH, nNPPBV,
                                    bProgressive, nQuality,
                                    (nBlockXOff == 0 && nBlockYOff == 0) ? abyAPP6 : NULL,
                                    nRestartInterval,
                                    pfnProgress, pProgressData))
            {
                return FALSE;
            }
        }
    }
    return TRUE;
}

#endif /* def JPEG_SUPPORTED */

/************************************************************************/
/*                          GDALRegister_NITF()                         */
/************************************************************************/

typedef struct
{
    int         nMaxLen;
    const char* pszName;
    const char* pszDescription;
} NITFFieldDescription;

/* Keep in sync with NITFCreate */
static const NITFFieldDescription asFieldDescription [] =
{
    { 2, "CLEVEL", "Complexity level" } ,
    { 10, "OSTAID", "Originating Station ID" } ,
    { 14, "FDT", "File Date and Time" } ,
    { 80, "FTITLE", "File Title" } ,
    { 1, "FSCLAS", "File Security Classification" } ,
    { 2, "FSCLSY", "File Classification Security System" } ,
    { 11, "FSCODE", "File Codewords" } ,
    { 2, "FSCTLH", "File Control and Handling" } ,
    { 20, "FSREL", "File Releasing Instructions" } ,
    { 2, "FSDCTP", "File Declassification Type" } ,
    { 8, "FSDCDT", "File Declassification Date" } ,
    { 4, "FSDCXM", "File Declassification Exemption" } ,
    { 1, "FSDG", "File Downgrade" } ,
    { 8, "FSDGDT", "File Downgrade Date" } ,
    { 43, "FSCLTX", "File Classification Text" } ,
    { 1, "FSCATP", "File Classification Authority Type" } ,
    { 40, "FSCAUT", "File Classification Authority" } ,
    { 1, "FSCRSN", "File Classification Reason" } ,
    { 8, "FSSRDT", "File Security Source Date" } ,
    { 15, "FSCTLN", "File Security Control Number" } ,
    { 5, "FSCOP", "File Copy Number" } ,
    { 5, "FSCPYS", "File Number of Copies" } ,
    { 24, "ONAME", "Originator Name" } ,
    { 18, "OPHONE", "Originator Phone Number" } ,
    { 10, "IID1", "Image Identifier 1" } ,
    { 14, "IDATIM", "Image Date and Time" } ,
    { 17, "TGTID", "Target Identifier" } ,
    { 80, "IID2", "Image Identifier 2" } ,
    {  1, "ISCLAS", "Image Security Classification" } ,
    {  2, "ISCLSY", "Image Classification Security System" } ,
    { 11, "ISCODE", "Image Codewords" } ,
    {  2, "ISCTLH", "Image Control and Handling" } ,
    { 20, "ISREL", "Image Releasing Instructions" } ,
    {  2, "ISDCTP", "Image Declassification Type" } ,
    {  8, "ISDCDT", "Image Declassification Date" } ,
    {  4, "ISDCXM", "Image Declassification Exemption" } ,
    {  1, "ISDG", "Image Downgrade" } ,
    {  8, "ISDGDT", "Image Downgrade Date" } ,
    { 43, "ISCLTX", "Image Classification Text" } ,
    {  1, "ISCATP", "Image Classification Authority Type" } ,
    { 40, "ISCAUT", "Image Classification Authority" } ,
    {  1, "ISCRSN", "Image Classification Reason" } ,
    {  8, "ISSRDT", "Image Security Source Date" } ,
    { 15, "ISCTLN", "Image Security Control Number" } ,
    { 42, "ISORCE", "Image Source" } ,
    {  8, "ICAT", "Image Category" } ,
    {  2, "ABPP", "Actual Bits-Per-Pixel Per Band" } ,
    {  1, "PJUST", "Pixel Justification" } ,
    {780, "ICOM", "Image Comments (up to 9x80 characters)" } ,
};

/* Keep in sync with NITFWriteBLOCKA */
static const char *apszFieldsBLOCKA[] = { 
        "BLOCK_INSTANCE", "0", "2",
        "N_GRAY",         "2", "5",
        "L_LINES",        "7", "5",
        "LAYOVER_ANGLE",  "12", "3",
        "SHADOW_ANGLE",   "15", "3",
        "BLANKS",         "18", "16",
        "FRLC_LOC",       "34", "21",
        "LRLC_LOC",       "55", "21",
        "LRFC_LOC",       "76", "21",
        "FRFC_LOC",       "97", "21",
        NULL,             NULL, NULL };

void GDALRegister_NITF()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "NITF" ) == NULL )
    {
        unsigned int i;
        CPLString osCreationOptions;

        osCreationOptions =
"<CreationOptionList>"
"   <Option name='IC' type='string-select' default='NC' description='Compression mode. NC=no compression. "
#ifdef JPEG_SUPPORTED
                "C3/M3=JPEG compression. "
#endif
                "C8=JP2 compression through the JP2ECW driver"
                "'>"
"       <Value>NC</Value>"
#ifdef JPEG_SUPPORTED
"       <Value>C3</Value>"
"       <Value>M3</Value>"
#endif
"       <Value>C8</Value>"
"   </Option>"
#ifdef JPEG_SUPPORTED
"   <Option name='QUALITY' type='int' description='JPEG quality 10-100' default='75'/>"
"   <Option name='PROGRESSIVE' type='boolean' description='JPEG progressive mode'/>"
"   <Option name='RESTART_INTERVAL' type='int' description='Restart interval (in MCUs). -1 for auto, 0 for none, > 0 for user specified' default='-1'/>"
#endif
"   <Option name='NUMI' type='int' default='1' description='Number of images to create (1-999). Only works with IC=NC'/>"
"   <Option name='TARGET' type='float' description='For JP2 only. Compression Percentage'/>"
"   <Option name='PROFILE' type='string-select' description='For JP2 only.'>"
"       <Value>BASELINE_0</Value>"
"       <Value>BASELINE_1</Value>"
"       <Value>BASELINE_2</Value>"
"       <Value>NPJE</Value>"
"       <Value>EPJE</Value>"
"   </Option>"
"   <Option name='ICORDS' type='string-select' description='To ensure that space will be reserved for geographic corner coordinates in DMS (G), in decimal degrees (D), UTM North (N) or UTM South (S)'>"
"       <Value>G</Value>"
"       <Value>D</Value>"
"       <Value>N</Value>"
"       <Value>S</Value>"
"   </Option>"
"   <Option name='FHDR' type='string-select' description='File version' default='NITF02.10'>"
"       <Value>NITF02.10</Value>"
"       <Value>NSIF01.00</Value>"
"   </Option>"
"   <Option name='IREP' type='string' description='Set to RGB/LUT to reserve space for a color table for each output band. (Only needed for Create() method, not CreateCopy())'/>"
"   <Option name='IREPBAND' type='string' description='Comma separated list of band IREPBANDs in band order'/>"
"   <Option name='ISUBCAT' type='string' description='Comma separated list of band ISUBCATs in band order'/>" 
"   <Option name='LUT_SIZE' type='integer' description='Set to control the size of pseudocolor tables for RGB/LUT bands' default='256'/>"
"   <Option name='BLOCKXSIZE' type='int' description='Set the block width'/>"
"   <Option name='BLOCKYSIZE' type='int' description='Set the block height'/>"
"   <Option name='BLOCKSIZE' type='int' description='Set the block with and height. Overridden by BLOCKXSIZE and BLOCKYSIZE'/>"
"   <Option name='TEXT' type='string' description='TEXT options as text-option-name=text-option-content'/>"
"   <Option name='CGM' type='string' description='CGM options in cgm-option-name=cgm-option-content'/>";

        for(i=0;i<sizeof(asFieldDescription) / sizeof(asFieldDescription[0]); i++)
        {
            osCreationOptions += CPLString().Printf("   <Option name='%s' type='string' description='%s' maxsize='%d'/>",
                    asFieldDescription[i].pszName, asFieldDescription[i].pszDescription, asFieldDescription[i].nMaxLen);
        }

        osCreationOptions +=
"   <Option name='TRE' type='string' description='Under the format TRE=tre-name,tre-contents'/>"
"   <Option name='FILE_TRE' type='string' description='Under the format FILE_TRE=tre-name,tre-contents'/>"
"   <Option name='BLOCKA_BLOCK_COUNT' type='int'/>";

        for(i=0; apszFieldsBLOCKA[i] != NULL; i+=3)
        {
            char szFieldDescription[128];
            sprintf(szFieldDescription, "   <Option name='BLOCKA_%s_*' type='string' maxsize='%d'/>",
                    apszFieldsBLOCKA[i], atoi(apszFieldsBLOCKA[i+2]));
            osCreationOptions += szFieldDescription;
        }
        osCreationOptions +=
"   <Option name='SDE_TRE' type='boolean' description='Write GEOLOB and GEOPSB TREs (only geographic SRS for now)' default='NO'/>";
        osCreationOptions += "</CreationOptionList>";

        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "NITF" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "National Imagery Transmission Format" );
        
        poDriver->pfnIdentify = NITFDataset::Identify;
        poDriver->pfnOpen = NITFDataset::Open;
        poDriver->pfnCreate = NITFDataset::NITFDatasetCreate;
        poDriver->pfnCreateCopy = NITFDataset::NITFCreateCopy;

        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_nitf.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "ntf" );
        poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte UInt16 Int16 UInt32 Int32 Float32" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, osCreationOptions);
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
