/******************************************************************************
 * $Id$
 *
 * Project:  Polarimetric Workstation
 * Purpose:  Radarsat 2 - XML Products (product.xml) driver
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_RS2(void);
CPL_C_END

typedef enum eCalibration_t {
    Sigma0 = 0,
    Gamma,
    Beta0,
    Uncalib,
    None
} eCalibration;

/************************************************************************/
/* ==================================================================== */
/*                               RS2Dataset                             */
/* ==================================================================== */
/************************************************************************/

class RS2Dataset : public GDALPamDataset
{
    CPLXMLNode *psProduct;

    int           nGCPCount;
    GDAL_GCP      *pasGCPList;
    char          *pszGCPProjection;
  public:
            RS2Dataset();
                ~RS2Dataset();
    
    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

    static GDALDataset *Open( GDALOpenInfo * );
    static int Identify( GDALOpenInfo * );

    CPLXMLNode *GetProduct() { return psProduct; }
};

/************************************************************************/
/* ==================================================================== */
/*                    RS2RasterBand                           */
/* ==================================================================== */
/************************************************************************/

class RS2RasterBand : public GDALPamRasterBand
{
    GDALDataset     *poBandFile;

  public:
            RS2RasterBand( RS2Dataset *poDSIn, 
                               GDALDataType eDataTypeIn,
                               const char *pszPole, 
                               GDALDataset *poBandFile );
    virtual     ~RS2RasterBand();
    
    virtual CPLErr IReadBlock( int, int, void * );

    static GDALDataset *Open( GDALOpenInfo * );
};


/************************************************************************/
/*                            RS2RasterBand                             */
/************************************************************************/

RS2RasterBand::RS2RasterBand( RS2Dataset *poDSIn,
                              GDALDataType eDataTypeIn,
                              const char *pszPole, 
                              GDALDataset *poBandFileIn )

{
    GDALRasterBand *poSrcBand;

    poDS = poDSIn;
    poBandFile = poBandFileIn;

    poSrcBand = poBandFile->GetRasterBand( 1 );

    poSrcBand->GetBlockSize( &nBlockXSize, &nBlockYSize );
    
    eDataType = eDataTypeIn;

    if( *pszPole != '\0' )
        SetMetadataItem( "POLARIMETRIC_INTERP", pszPole );
}

/************************************************************************/
/*                            RSRasterBand()                            */
/************************************************************************/

RS2RasterBand::~RS2RasterBand()

{
    if( poBandFile != NULL )
        GDALClose( (GDALRasterBandH) poBandFile );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr RS2RasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    int nRequestYSize;

/* -------------------------------------------------------------------- */
/*      If the last strip is partial, we need to avoid                  */
/*      over-requesting.  We also need to initialize the extra part     */
/*      of the block to zero.                                           */
/* -------------------------------------------------------------------- */
    if( (nBlockYOff + 1) * nBlockYSize > nRasterYSize )
    {
        nRequestYSize = nRasterYSize - nBlockYOff * nBlockYSize;
        memset( pImage, 0, (GDALGetDataTypeSize( eDataType ) / 8) * nBlockXSize * nBlockYSize );
    }
    else
    {
        nRequestYSize = nBlockYSize;
    }

/* -------------------------------------------------------------------- */
/*      Each complex component is a seperate sample in the TIFF file    */
/*      (old way)                                                       */
/* -------------------------------------------------------------------- */
    if( eDataType == GDT_CInt16 && poBandFile->GetRasterCount() == 2 )
        return 
            poBandFile->RasterIO( GF_Read, 
                                  nBlockXOff * nBlockXSize, 
                                  nBlockYOff * nBlockYSize,
                                  nBlockXSize, nRequestYSize,
                                  pImage, nBlockXSize, nRequestYSize, 
                                  GDT_Int16,
                                  2, NULL, 4, nBlockXSize * 4, 2 );

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
                                  nBlockXSize, nRequestYSize, 
                                  pImage, nBlockXSize, nRequestYSize, 
                                  GDT_UInt32,
                                  1, NULL, 4, nBlockXSize * 4, 0 );

#ifdef CPL_LSB
        // First, undo the 32bit swap. 
        GDALSwapWords( pImage, 4, nBlockXSize * nBlockYSize, 4 );

        // Then apply 16 bit swap. 
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
                                  nBlockXSize, nRequestYSize, 
                                  pImage, nBlockXSize, nRequestYSize,
                                  GDT_UInt16,
                                  1, NULL, 2, nBlockXSize * 2, 0 );
    else if ( eDataType == GDT_Byte ) 
        /* Ticket #2104: Support for ScanSAR products */
        return
            poBandFile->RasterIO( GF_Read,
                                  nBlockXOff * nBlockXSize,
                                  nBlockYOff * nBlockYSize,
                                  nBlockXSize, nRequestYSize,
                                  pImage, nBlockXSize, nRequestYSize,
                                  GDT_Byte,
                                  1, NULL, 1, nBlockXSize, 0 );
    else
    {
        CPLAssert( FALSE );
        return CE_Failure;
    }
}

/************************************************************************/
/* ==================================================================== */
/*                         RS2CalibRasterBand                           */
/* ==================================================================== */
/************************************************************************/
/* Returns data that has been calibrated to sigma nought, gamma         */
/* or beta nought.                                                      */
/************************************************************************/

class RS2CalibRasterBand : public GDALPamRasterBand {
private:
    eCalibration m_eCalib;
    GDALDataset *m_poBandDataset;
    GDALDataType m_eType; /* data type of data being ingested */
    float *m_nfTable;
    int m_nTableSize;
    float m_nfOffset;
    char *m_pszLUTFile;

    void ReadLUT();
public:
    RS2CalibRasterBand( RS2Dataset *poDataset, const char *pszPolarization,
        GDALDataType eType, GDALDataset *poBandDataset, eCalibration eCalib,
        const char *pszLUT);
    ~RS2CalibRasterBand();

    CPLErr IReadBlock( int nBlockXOff, int nBlockYOff, void *pImage);
};

/************************************************************************/
/*                            ReadLUT()                                 */
/************************************************************************/
/* Read the provided LUT in to m_ndTable                                */
/************************************************************************/
void RS2CalibRasterBand::ReadLUT() {
    CPLXMLNode *psLUT;
    char **papszLUTList;

    psLUT = CPLParseXMLFile(m_pszLUTFile);
    
    this->m_nfOffset = CPLAtof(CPLGetXMLValue(psLUT, "=lut.offset",
        "0.0"));

    papszLUTList = CSLTokenizeString2( CPLGetXMLValue(psLUT,
        "=lut.gains", ""), " ", CSLT_HONOURSTRINGS);

    this->m_nTableSize = CSLCount(papszLUTList);

    this->m_nfTable = (float *)CPLMalloc(sizeof(float) * this->m_nTableSize);

    for (int i = 0; i < this->m_nTableSize; i++) {
        m_nfTable[i] = CPLAtof(papszLUTList[i]);
    }

    CSLDestroy(papszLUTList);
}

/************************************************************************/
/*                        RS2CalibRasterBand()                          */
/************************************************************************/

RS2CalibRasterBand::RS2CalibRasterBand( RS2Dataset *poDataset, 
    const char *pszPolarization, GDALDataType eType, GDALDataset *poBandDataset,
    eCalibration eCalib, const char *pszLUT )
{
    this->poDS = poDataset;

    if (*pszPolarization != '\0') {
        this->SetMetadataItem( "POLARIMETRIC_INTERP", pszPolarization );
    }

    this->m_eType = eType;
    this->m_poBandDataset = poBandDataset;
    this->m_eCalib = eCalib;
    this->m_pszLUTFile = VSIStrdup(pszLUT);

    this->m_nfTable = NULL;
    this->m_nTableSize = 0;

    if (eType == GDT_CInt16) 
        this->eDataType = GDT_CFloat32;
    else
        this->eDataType = GDT_Float32;

    GDALRasterBand *poRasterBand = poBandDataset->GetRasterBand( 1 );
    poRasterBand->GetBlockSize( &nBlockXSize, &nBlockYSize );
    

    this->ReadLUT();
}

/************************************************************************/
/*                       ~RS2CalibRasterBand()                          */
/************************************************************************/

RS2CalibRasterBand::~RS2CalibRasterBand() {
    if (this->m_nfTable != NULL) 
        CPLFree(this->m_nfTable);

    if (this->m_pszLUTFile != NULL)
        CPLFree(this->m_pszLUTFile);

    if (this->m_poBandDataset != NULL)
        GDALClose( this->m_poBandDataset );
}

/************************************************************************/
/*                        IReadBlock()                                  */
/************************************************************************/

CPLErr RS2CalibRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
    void *pImage )
{
    CPLErr eErr;
    int nRequestYSize;

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

    if (this->m_eType == GDT_CInt16) {
        GInt16 *pnImageTmp;
        /* read in complex values */
        pnImageTmp = (GInt16 *)CPLMalloc(2 * nBlockXSize * nBlockYSize * 
            GDALGetDataTypeSize( GDT_Int16 ) / 8);
        if (m_poBandDataset->GetRasterCount() == 2) {
            eErr = m_poBandDataset->RasterIO( GF_Read, 
                                  nBlockXOff * nBlockXSize, 
                                  nBlockYOff * nBlockYSize,
                                  nBlockXSize, nRequestYSize,
                                  pnImageTmp, nBlockXSize, nRequestYSize, 
                                  GDT_Int16,
                                  2, NULL, 4, nBlockXSize * 4, 2 );

        }
        else {
            eErr = 
                m_poBandDataset->RasterIO( GF_Read, 
                                      nBlockXOff * nBlockXSize, 
                                      nBlockYOff * nBlockYSize,
                                      nBlockXSize, nRequestYSize, 
                                      pnImageTmp, nBlockXSize, nRequestYSize, 
                                      GDT_UInt32,
                                      1, NULL, 4, nBlockXSize * 4, 0 );

#ifdef CPL_LSB
            /* First, undo the 32bit swap. */ 
            GDALSwapWords( pImage, 4, nBlockXSize * nBlockYSize, 4 );

            /* Then apply 16 bit swap. */
            GDALSwapWords( pImage, 2, nBlockXSize * nBlockYSize * 2, 2 );
#endif        
        }

        /* calibrate the complex values */
        for (int i = 0; i < nBlockYSize; i++) {
            for (int j = 0; j < nBlockXSize; j++) {
                /* calculate pixel offset in memory*/
                int nPixOff = (2 * (i * nBlockYSize)) + (j * 2);

                ((float *)pImage)[nPixOff] = (float)pnImageTmp[nPixOff]/
                    (m_nfTable[nBlockXOff + j]);
                ((float *)pImage)[nPixOff + 1] = 
                    (float)pnImageTmp[nPixOff + 1]/(m_nfTable[nBlockXOff + j]);
            }
        }
        CPLFree(pnImageTmp);
    }
    else if (this->m_eType == GDT_UInt16) {
        GUInt16 *pnImageTmp;
        /* read in detected values */
        pnImageTmp = (GUInt16 *)CPLMalloc(nBlockXSize * nBlockYSize *
            GDALGetDataTypeSize( GDT_UInt16 ) / 8);
        eErr = m_poBandDataset->RasterIO( GF_Read, 
                              nBlockXOff * nBlockXSize, 
                              nBlockYOff * nBlockYSize,
                              nBlockXSize, nRequestYSize, 
                              pnImageTmp, nBlockXSize, nRequestYSize,
                              GDT_UInt16,
                              1, NULL, 2, nBlockXSize * 2, 0 );

        /* iterate over detected values */
        for (int i = 0; i < nBlockYSize; i++) {
            for (int j = 0; j < nBlockXSize; j++) {
                int nPixOff = (i * nBlockYSize) + j;

                ((float *)pImage)[nPixOff] = (((float)pnImageTmp[nPixOff] * 
                    (float)pnImageTmp[nPixOff]) +
                    this->m_nfOffset)/m_nfTable[nBlockXOff + j];
            }
        }
        CPLFree(pnImageTmp);
    } /* Ticket #2104: Support for ScanSAR products */
    else if (this->m_eType == GDT_Byte) {
        GByte *pnImageTmp;
        pnImageTmp = (GByte *)CPLMalloc(nBlockXSize * nBlockYSize *
            GDALGetDataTypeSize( GDT_Byte ) / 8);
        eErr = m_poBandDataset->RasterIO( GF_Read,
                            nBlockXOff * nBlockXSize,
                            nBlockYOff * nBlockYSize,
                            nBlockXSize, nRequestYSize,
                            pnImageTmp, nBlockXSize, nRequestYSize,
                            GDT_Byte,
                            1, NULL, 1, 1, 0);

        /* iterate over detected values */
        for (int i = 0; i < nBlockYSize; i++) {
            for (int j = 0; j < nBlockXSize; j++) {
                int nPixOff = (i * nBlockYSize) + j;

                ((float *)pImage)[nPixOff] = ((pnImageTmp[nPixOff] *
                    pnImageTmp[nPixOff]) +
                    this->m_nfOffset)/m_nfTable[nBlockXOff + j];
            }
        }
        CPLFree(pnImageTmp);
    }
    else {
        CPLAssert( FALSE );
        return CE_Failure;
    }
    return eErr;
}


/************************************************************************/
/* ==================================================================== */
/*                              RS2Dataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             RS2Dataset()                             */
/************************************************************************/

RS2Dataset::RS2Dataset()
{
    psProduct = NULL;

    nGCPCount = 0;
    pasGCPList = NULL;
    pszGCPProjection = CPLStrdup("");

}

/************************************************************************/
/*                            ~RS2Dataset()                             */
/************************************************************************/

RS2Dataset::~RS2Dataset()

{
    FlushCache();

    CPLDestroyXMLNode( psProduct );

    CPLFree( pszGCPProjection );
    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int RS2Dataset::Identify( GDALOpenInfo *poOpenInfo ) 
{

    /* Check for the case where we're trying to read the calibrated data: */
    if (EQUALN("RADARSAT_2_CALIB:",poOpenInfo->pszFilename,17)) {
        return 1;
    }

    /* otherwise, do our normal stuff */
    if( strlen(poOpenInfo->pszFilename) < 11
        || !EQUAL(poOpenInfo->pszFilename + strlen(poOpenInfo->pszFilename)-11,
                  "product.xml") )
        return 0;

    if( poOpenInfo->nHeaderBytes < 100 )
        return 0;

    if( strstr((const char *) poOpenInfo->pabyHeader, "/rs2" ) == NULL
        || strstr((const char *) poOpenInfo->pabyHeader, "<product" ) == NULL)
        return 0;

    return 1;

}


/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *RS2Dataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Is this a RADARSAT-2 Product.xml definition?                   */
/* -------------------------------------------------------------------- */
    if ( !RS2Dataset::Identify( poOpenInfo ) ) {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*        Get subdataset information, if relevant                            */
/* -------------------------------------------------------------------- */
    const char *pszFilename = poOpenInfo->pszFilename;
    eCalibration eCalib = None;

    if (EQUALN("RADARSAT_2_CALIB:",pszFilename,17)) {
        pszFilename += 17;

        if (EQUALN("BETA0",pszFilename,5))
            eCalib = Beta0;
        else if (EQUALN("SIGMA0",pszFilename,6))
            eCalib = Sigma0;
        else if (EQUALN("GAMMA", pszFilename,5))
            eCalib = Gamma;
        else if (EQUALN("UNCALIB", pszFilename,7))
            eCalib = Uncalib;
        else
            eCalib = None;

        /* advance the pointer to the actual filename */
        while ( *pszFilename != '\0' && *pszFilename != ':' ) 
            pszFilename++;

        if (*pszFilename == ':')
            pszFilename++;
    }

/* -------------------------------------------------------------------- */
/*      Ingest the Product.xml file.                                    */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psProduct, *psImageAttributes, *psImageGenerationParameters;

    psProduct = CPLParseXMLFile( pszFilename );
    if( psProduct == NULL )
        return NULL;

    psImageAttributes = CPLGetXMLNode(psProduct, "=product.imageAttributes" );
    if( psImageAttributes == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to find <imageAttributes> in document." );
        return NULL;
    }

    psImageGenerationParameters = CPLGetXMLNode( psProduct, 
        "=product.imageGenerationParameters" );
    if (psImageGenerationParameters == NULL) {
        CPLError( CE_Failure, CPLE_OpenFailed,
            "Failed to find <imageGenerationParameters> in document." );
    }

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    RS2Dataset *poDS = new RS2Dataset();

    poDS->psProduct = psProduct;

/* -------------------------------------------------------------------- */
/*      Get overall image information.                                  */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = 
        atoi(CPLGetXMLValue( psImageAttributes, 
                             "rasterAttributes.numberOfSamplesPerLine", 
                             "-1" ));
    poDS->nRasterYSize = 
        atoi(CPLGetXMLValue( psImageAttributes, 
                             "rasterAttributes.numberofLines", 
                             "-1" ));

/* -------------------------------------------------------------------- */
/*      Check product type, as to determine if there are LUTs for       */
/*      calibration purposes.                                           */
/* -------------------------------------------------------------------- */

    char *pszBeta0LUT = NULL;
    char *pszGammaLUT = NULL;
    char *pszSigma0LUT = NULL;
    int bCanCalib = 0;

    const char *pszProductType = CPLGetXMLValue( psImageGenerationParameters,
            "generalProcessingInformation.productType",
            "UNK" );

    poDS->SetMetadataItem("PRODUCT_TYPE", pszProductType);

    /* the following cases can be assumed to have no LUTs, as per 
     * RN-RP-51-2713, but also common sense
     */
    if (!(EQUALN(pszProductType, "UNK", 3) || 
        EQUALN(pszProductType, "SSG", 3) ||
        EQUALN(pszProductType, "SPG", 3)))
    {
        bCanCalib = 1;
    }

/* -------------------------------------------------------------------- */
/*      Get dataType (so we can recognise complex data), and the        */
/*      bitsPerSample.                                                  */
/* -------------------------------------------------------------------- */
    GDALDataType eDataType;

   const char *pszDataType = 
           CPLGetXMLValue( psImageAttributes, "rasterAttributes.dataType", 
                        "" );
    int nBitsPerSample = 
        atoi( CPLGetXMLValue( psImageAttributes, 
                                 "rasterAttributes.bitsPerSample", "" ) );

    if( nBitsPerSample == 16 && EQUAL(pszDataType,"Complex") )
        eDataType = GDT_CInt16;
    else if( nBitsPerSample == 16 && EQUALN(pszDataType,"Mag",3) )
        eDataType = GDT_UInt16;
    else if( nBitsPerSample == 8 && EQUALN(pszDataType,"Mag",3) )
        eDataType = GDT_Byte;
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
               "dataType=%s, bitsPerSample=%d: not a supported configuration.",
                pszDataType, nBitsPerSample );
        return NULL;
    }

    /* while we're at it, extract the pixel spacing information */
    const char *pszPixelSpacing = CPLGetXMLValue( psImageAttributes,
        "rasterAttributes.sampledPixelSpacing", "UNK" );
    poDS->SetMetadataItem( "PIXEL_SPACING", pszPixelSpacing );

    const char *pszLineSpacing = CPLGetXMLValue( psImageAttributes,
        "rasterAttributes.sampledLineSpacing", "UNK" );
    poDS->SetMetadataItem( "LINE_SPACING", pszLineSpacing );

/* -------------------------------------------------------------------- */
/*      Open each of the data files as a complex band.                  */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psNode;
    char *pszPath = CPLStrdup(CPLGetPath( pszFilename ));
    char **papszSubdatasets = NULL;
    CPLString sBuf;

    for( psNode = psImageAttributes->psChild;
         psNode != NULL;
         psNode = psNode->psNext )
    {
        const char *pszBasename;
        if( psNode->eType != CXT_Element 
            || !(EQUAL(psNode->pszValue,"fullResolutionImageData") 
                 || EQUAL(psNode->pszValue,"lookupTable")) )
            continue;

        if ( EQUAL(psNode->pszValue, "lookupTable") && bCanCalib ) {
            /* Determine which incidence angle correction this is */
            const char *pszLUTType = CPLGetXMLValue( psNode,
                "incidenceAngleCorrection", "" );
            const char *pszLUTFile = CPLGetXMLValue( psNode, "", "" );

            if (EQUAL(pszLUTType, "")) 
                continue;
            else if (EQUAL(pszLUTType, "Beta Nought")) {
                pszBeta0LUT = VSIStrdup( pszLUTFile );
                poDS->SetMetadataItem( "BETA_NOUGHT_LUT", pszLUTFile );

                sBuf.Printf("RADARSAT_2_CALIB:BETA0:%s", pszFilename);
                papszSubdatasets = CSLSetNameValue( papszSubdatasets,
                    "SUBDATASET_3_NAME", sBuf );
                papszSubdatasets = CSLSetNameValue( papszSubdatasets,
                    "SUBDATASET_3_DESC", "Beta Nought calibrated" );
            }
            else if (EQUAL(pszLUTType, "Sigma Nought")) {
                pszSigma0LUT = VSIStrdup( pszLUTFile );
                poDS->SetMetadataItem( "SIGMA_NOUGHT_LUT", pszLUTFile );

                sBuf.Printf("RADARSAT_2_CALIB:SIGMA0:%s", pszFilename);
                papszSubdatasets = CSLSetNameValue( papszSubdatasets, 
                    "SUBDATASET_2_NAME", sBuf );
                papszSubdatasets = CSLSetNameValue( papszSubdatasets,
                    "SUBDATASET_2_DESC", "Sigma Nought calibrated" );
            }
            else if (EQUAL(pszLUTType, "Gamma")) {
                pszGammaLUT = VSIStrdup( pszLUTFile );
                poDS->SetMetadataItem( "GAMMA_LUT", pszLUTFile );
                sBuf.Printf("RADARSAT_2_CALIB:GAMMA:%s", pszFilename);
                papszSubdatasets = CSLSetNameValue( papszSubdatasets,
                    "SUBDATASET_4_NAME", sBuf );
                papszSubdatasets = CSLSetNameValue( papszSubdatasets,
                    "SUBDATASET_4_DESC", "Gamma calibrated" );
            }
            continue;
        }

/* -------------------------------------------------------------------- */
/*      Fetch filename.                                                 */
/* -------------------------------------------------------------------- */
        pszBasename = CPLGetXMLValue( psNode, "", "" );
        if( *pszBasename == '\0' )
            continue;

/* -------------------------------------------------------------------- */
/*      Form full filename (path of product.xml + basename).            */
/* -------------------------------------------------------------------- */
        char *pszFullname = 
            CPLStrdup(CPLFormFilename( pszPath, pszBasename, NULL ));

/* -------------------------------------------------------------------- */
/*      Try and open the file.                                          */
/* -------------------------------------------------------------------- */
        GDALDataset *poBandFile;

        poBandFile = (GDALDataset *) GDALOpen( pszFullname, GA_ReadOnly );
        if( poBandFile == NULL )
            continue;
        
/* -------------------------------------------------------------------- */
/*      Create the band.                                                */
/* -------------------------------------------------------------------- */
        if (eCalib == None || eCalib == Uncalib) {
            RS2RasterBand *poBand;
            poBand = new RS2RasterBand( poDS, eDataType,
                                        CPLGetXMLValue( psNode, "pole", "" ), 
                                        poBandFile ); 
    
            poDS->SetBand( poDS->GetRasterCount() + 1, poBand );
        }
        else {
            const char *pszLUT;
            switch (eCalib) {
                case Sigma0:
                    pszLUT = pszSigma0LUT;
                    break;
                case Beta0:
                    pszLUT = pszBeta0LUT;
                    break;
                case Gamma:
                    pszLUT = pszGammaLUT;
                    break;
                default:
                    /* we should bomb gracefully... */
                    pszLUT = pszSigma0LUT;
            }
            RS2CalibRasterBand *poBand;
            poBand = new RS2CalibRasterBand( poDS, CPLGetXMLValue( psNode,
                        "pole", "" ), eDataType, poBandFile, eCalib, 
                        CPLFormFilename(pszPath, pszLUT, NULL) );
            poDS->SetBand( poDS->GetRasterCount() + 1, poBand );
        }

        CPLFree( pszFullname );
    }

    if (papszSubdatasets != NULL && eCalib == None) {
        CPLString sBuf;
        sBuf.Printf("RADARSAT_2_CALIB:UNCALIB:%s", pszFilename);
        papszSubdatasets = CSLSetNameValue( papszSubdatasets,
            "SUBDATASET_1_NAME", sBuf );
        papszSubdatasets = CSLSetNameValue( papszSubdatasets,
            "SUBDATASET_1_DESC", "Uncalibrated digital numbers" );

        poDS->GDALMajorObject::SetMetadata( papszSubdatasets, "SUBDATASETS" );
        CSLDestroy( papszSubdatasets );
    }
    else if (papszSubdatasets != NULL) {
        CSLDestroy( papszSubdatasets ); /* a subdataset was selected already */
    }

/* -------------------------------------------------------------------- */
/*      Set the appropriate MATRIX_REPRESENTATION.                      */
/* -------------------------------------------------------------------- */

    if ( poDS->GetRasterCount() == 4 && (eDataType == GDT_CInt16 ||
         eDataType == GDT_CFloat32) ) 
    {
        poDS->SetMetadataItem( "MATRIX_REPRESENTATION", "SCATTERING" );
    }
 
/* -------------------------------------------------------------------- */
/*      Collect a few useful metadata items                             */
/* -------------------------------------------------------------------- */

    CPLXMLNode *psSourceAttrs = CPLGetXMLNode( psProduct,
                                               "=product.sourceAttributes");
    const char *pszItem;

    if (psSourceAttrs != NULL) {
        /* Get beam mode mnemonic */
        pszItem = CPLGetXMLValue( psSourceAttrs, "beamModeMnemonic", "UNK" );
        poDS->SetMetadataItem( "BEAM_MODE", pszItem );
    }

    CPLXMLNode *psSarProcessingInformation =
        CPLGetXMLNode( psProduct, "=product.imageGenerationParameters" );

    if (psSarProcessingInformation != NULL) {
        /* Get incidence angle information */
        pszItem = CPLGetXMLValue( psSarProcessingInformation, 
            "sarProcessingInformation.incidenceAngleNearRange", "UNK" );
        poDS->SetMetadataItem( "NEAR_RANGE_INCIDENCE_ANGLE", pszItem );

        pszItem = CPLGetXMLValue( psSarProcessingInformation,
            "sarProcessingInformation.incidenceAngleFarRange", "UNK" );
        poDS->SetMetadataItem( "FAR_RANGE_INCIDENCE_ANGLE", pszItem );

        pszItem = CPLGetXMLValue( psSarProcessingInformation,
            "sarProcessingInformation.slantRangeNearEdge", "UNK" );
        poDS->SetMetadataItem( "SLANT_RANGE_NEAR_EDGE", pszItem );
    }

/* -------------------------------------------------------------------- */
/*      Collect GCPs.                                                   */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psGeoGrid = 
        CPLGetXMLNode( psImageAttributes, 
                       "geographicInformation.geolocationGrid" );

    if( psGeoGrid != NULL )
    {
        // count gcps.
        poDS->nGCPCount = 0;
        
        for( psNode = psGeoGrid->psChild; psNode != NULL;
             psNode = psNode->psNext )
        {
            if( EQUAL(psNode->pszValue,"imageTiePoint") )
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
            
            if( !EQUAL(psNode->pszValue,"imageTiePoint") )
                continue;

            poDS->nGCPCount++ ;

            sprintf( szID, "%d", poDS->nGCPCount );
            psGCP->pszId = CPLStrdup( szID );
            psGCP->pszInfo = CPLStrdup("");
            psGCP->dfGCPPixel = 
                atof(CPLGetXMLValue(psNode,"imageCoordinate.pixel","0"));
            psGCP->dfGCPLine = 
                atof(CPLGetXMLValue(psNode,"imageCoordinate.line","0"));
            psGCP->dfGCPX = 
                atof(CPLGetXMLValue(psNode,"geodeticCoordinate.longitude",""));
            psGCP->dfGCPY = 
                atof(CPLGetXMLValue(psNode,"geodeticCoordinate.latitude",""));
            psGCP->dfGCPZ = 
                atof(CPLGetXMLValue(psNode,"geodeticCoordinate.height",""));
        }
    }

    CPLFree( pszPath );
    if (pszBeta0LUT) CPLFree(pszBeta0LUT);
    if (pszSigma0LUT) CPLFree(pszSigma0LUT);
    if (pszGammaLUT) CPLFree(pszGammaLUT);

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, pszFilename );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( pszFilename );
    poDS->TryLoadXML();

    return( poDS );
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int RS2Dataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *RS2Dataset::GetGCPProjection()

{
    return pszGCPProjection;
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *RS2Dataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                         GDALRegister_RS2()                          */
/************************************************************************/

void GDALRegister_RS2()

{
    GDALDriver *poDriver;

    if( GDALGetDriverByName( "RS2" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "RS2" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "RadarSat 2 XML Product" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_rs2.html" );

        poDriver->pfnOpen = RS2Dataset::Open;
        poDriver->pfnIdentify = RS2Dataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

