/******************************************************************************
 *
 * Project:  Polarimetric Workstation
 * Purpose:  Radarsat 2 - XML Products (product.xml) driver
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogr_spatialref.h"

CPL_CVSID("$Id$")

typedef enum eCalibration_t {
    Sigma0 = 0,
    Gamma,
    Beta0,
    Uncalib,
    None
} eCalibration;

/*** Function to test for valid LUT files ***/
static bool IsValidXMLFile( const char *pszPath, const char *pszLut)
{
    /* Return true for valid xml file, false otherwise */
    char *pszLutFile
        = VSIStrdup(CPLFormFilename(pszPath, pszLut, NULL));

    CPLXMLTreeCloser psLut(CPLParseXMLFile(pszLutFile));

    CPLFree(pszLutFile);

    return psLut.get() != NULL;
}

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
    char        **papszSubDatasets;
    char          *pszProjection;
    double      adfGeoTransform[6];
    bool        bHaveGeoTransform;

    char        **papszExtraFiles;

  protected:
    virtual int         CloseDependentDatasets() override;

  public:
            RS2Dataset();
    virtual ~RS2Dataset();

    virtual int    GetGCPCount() override;
    virtual const char *GetGCPProjection() override;
    virtual const GDAL_GCP *GetGCPs() override;

    virtual const char *GetProjectionRef(void) override;
    virtual CPLErr GetGeoTransform( double * ) override;

    virtual char      **GetMetadataDomainList() override;
    virtual char **GetMetadata( const char * pszDomain = "" ) override;
    virtual char **GetFileList(void) override;

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

    virtual CPLErr IReadBlock( int, int, void * ) override;

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/*                            RS2RasterBand                             */
/************************************************************************/

RS2RasterBand::RS2RasterBand( RS2Dataset *poDSIn,
                              GDALDataType eDataTypeIn,
                              const char *pszPole,
                              GDALDataset *poBandFileIn ) :
    poBandFile(poBandFileIn)
{
    poDS = poDSIn;

    GDALRasterBand *poSrcBand = poBandFile->GetRasterBand( 1 );

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
        GDALClose( reinterpret_cast<GDALRasterBandH>( poBandFile ) );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr RS2RasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
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
        CPLErr eErr =
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
        /* Ticket #2104: Support for ScanSAR products */
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
/*                         RS2CalibRasterBand                           */
/* ==================================================================== */
/************************************************************************/
/* Returns data that has been calibrated to sigma nought, gamma         */
/* or beta nought.                                                      */
/************************************************************************/

class RS2CalibRasterBand : public GDALPamRasterBand {
private:
    // eCalibration m_eCalib;
    GDALDataset *m_poBandDataset;
    GDALDataType m_eType; /* data type of data being ingested */
    float *m_nfTable;
    int m_nTableSize;
    float m_nfOffset;
    char *m_pszLUTFile;

    void ReadLUT();
public:
    RS2CalibRasterBand(
        RS2Dataset *poDataset, const char *pszPolarization,
        GDALDataType eType, GDALDataset *poBandDataset, eCalibration eCalib,
        const char *pszLUT );
    ~RS2CalibRasterBand();

    CPLErr IReadBlock( int nBlockXOff, int nBlockYOff, void *pImage ) override;
};

/************************************************************************/
/*                            ReadLUT()                                 */
/************************************************************************/
/* Read the provided LUT in to m_ndTable                                */
/************************************************************************/
void RS2CalibRasterBand::ReadLUT() {
    CPLXMLNode *psLUT = CPLParseXMLFile(m_pszLUTFile);

    this->m_nfOffset = static_cast<float>(
        CPLAtof( CPLGetXMLValue( psLUT, "=lut.offset", "0.0" ) ) );

    char **papszLUTList = CSLTokenizeString2( CPLGetXMLValue(psLUT,
        "=lut.gains", ""), " ", CSLT_HONOURSTRINGS);

    m_nTableSize = CSLCount(papszLUTList);

    m_nfTable = reinterpret_cast<float *>(
        CPLMalloc( sizeof(float) * m_nTableSize ) );

    for (int i = 0; i < m_nTableSize; i++) {
        m_nfTable[i] = static_cast<float>( CPLAtof(papszLUTList[i]) );
    }

    CPLDestroyXMLNode(psLUT);

    CSLDestroy(papszLUTList);
}

/************************************************************************/
/*                        RS2CalibRasterBand()                          */
/************************************************************************/

RS2CalibRasterBand::RS2CalibRasterBand(
    RS2Dataset *poDataset, const char *pszPolarization, GDALDataType eType,
    GDALDataset *poBandDataset, eCalibration /* eCalib */,
    const char *pszLUT ) :
    // m_eCalib(eCalib),
    m_poBandDataset(poBandDataset),
    m_eType(eType),
    m_nfTable(NULL),
    m_nTableSize(0),
    m_nfOffset(0),
    m_pszLUTFile(VSIStrdup(pszLUT))
{
    poDS = poDataset;

    if (*pszPolarization != '\0') {
        SetMetadataItem( "POLARIMETRIC_INTERP", pszPolarization );
    }

    if (eType == GDT_CInt16)
        eDataType = GDT_CFloat32;
    else
        eDataType = GDT_Float32;

    GDALRasterBand *poRasterBand = poBandDataset->GetRasterBand( 1 );
    poRasterBand->GetBlockSize( &nBlockXSize, &nBlockYSize );

    ReadLUT();
}

/************************************************************************/
/*                       ~RS2CalibRasterBand()                          */
/************************************************************************/

RS2CalibRasterBand::~RS2CalibRasterBand() {
    CPLFree(m_nfTable);
    CPLFree(m_pszLUTFile);
    GDALClose( m_poBandDataset );
}

/************************************************************************/
/*                        IReadBlock()                                  */
/************************************************************************/

CPLErr RS2CalibRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
    void *pImage )
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

    CPLErr eErr;
    if (m_eType == GDT_CInt16) {
        /* read in complex values */
        GInt16 *pnImageTmp
            = reinterpret_cast<GInt16 *>(
                CPLMalloc( 2 * nBlockXSize * nBlockYSize
                           * GDALGetDataTypeSize( GDT_Int16 ) / 8 ) );
        if (m_poBandDataset->GetRasterCount() == 2) {
            eErr = m_poBandDataset->RasterIO( GF_Read,
                                  nBlockXOff * nBlockXSize,
                                  nBlockYOff * nBlockYSize,
                                  nBlockXSize, nRequestYSize,
                                  pnImageTmp, nBlockXSize, nRequestYSize,
                                  GDT_Int16,
                                  2, NULL, 4, nBlockXSize * 4, 2, NULL );
        }
        else
        {
            eErr =
                m_poBandDataset->RasterIO( GF_Read,
                                      nBlockXOff * nBlockXSize,
                                      nBlockYOff * nBlockYSize,
                                      nBlockXSize, nRequestYSize,
                                      pnImageTmp, nBlockXSize, nRequestYSize,
                                      GDT_UInt32,
                                      1, NULL, 4, nBlockXSize * 4, 0, NULL );

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
                int nPixOff = (2 * (i * nBlockXSize)) + (j * 2);

                reinterpret_cast<float *>( pImage )[nPixOff]
                    = static_cast<float>( pnImageTmp[nPixOff] )
                    / (m_nfTable[nBlockXOff + j]);
                reinterpret_cast<float *>( pImage )[nPixOff + 1] =
                    static_cast<float>( pnImageTmp[nPixOff + 1] )
                    / (m_nfTable[nBlockXOff + j]);
            }
        }
        CPLFree(pnImageTmp);
    }
    else if (m_eType == GDT_UInt16) {
        /* read in detected values */
        GUInt16 *pnImageTmp = reinterpret_cast<GUInt16 *>(
            CPLMalloc(nBlockXSize * nBlockYSize
                      * GDALGetDataTypeSize( GDT_UInt16 ) / 8) );
        eErr = m_poBandDataset->RasterIO( GF_Read,
                              nBlockXOff * nBlockXSize,
                              nBlockYOff * nBlockYSize,
                              nBlockXSize, nRequestYSize,
                              pnImageTmp, nBlockXSize, nRequestYSize,
                              GDT_UInt16,
                              1, NULL, 2, nBlockXSize * 2, 0, NULL );

        /* iterate over detected values */
        for (int i = 0; i < nBlockYSize; i++) {
            for (int j = 0; j < nBlockXSize; j++) {
                int nPixOff = (i * nBlockXSize) + j;

                reinterpret_cast<float *>( pImage )[nPixOff]
                    = ((static_cast<float>( pnImageTmp[nPixOff] ) *
                       static_cast<float>( pnImageTmp[nPixOff] ) ) +
                       m_nfOffset)
                    / m_nfTable[nBlockXOff + j];
            }
        }
        CPLFree(pnImageTmp);
    } /* Ticket #2104: Support for ScanSAR products */
    else if (m_eType == GDT_Byte) {
        GByte *pnImageTmp
            = reinterpret_cast<GByte *>(
                CPLMalloc(nBlockXSize * nBlockYSize *
                          GDALGetDataTypeSize( GDT_Byte ) / 8) );
        eErr = m_poBandDataset->RasterIO( GF_Read,
                            nBlockXOff * nBlockXSize,
                            nBlockYOff * nBlockYSize,
                            nBlockXSize, nRequestYSize,
                            pnImageTmp, nBlockXSize, nRequestYSize,
                            GDT_Byte,
                            1, NULL, 1, 1, 0, NULL);

        /* iterate over detected values */
        for (int i = 0; i < nBlockYSize; i++) {
            for (int j = 0; j < nBlockXSize; j++) {
                int nPixOff = (i * nBlockXSize) + j;

                reinterpret_cast<float *>( pImage )[nPixOff]
                    = ((pnImageTmp[nPixOff] * pnImageTmp[nPixOff]) +
                    m_nfOffset)/m_nfTable[nBlockXOff + j];
            }
        }
        CPLFree(pnImageTmp);
    }
    else {
        CPLAssert( false );
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

RS2Dataset::RS2Dataset() :
    psProduct(NULL),
    nGCPCount(0),
    pasGCPList(NULL),
    pszGCPProjection(CPLStrdup("")),
    papszSubDatasets(NULL),
    pszProjection(CPLStrdup("")),
    bHaveGeoTransform(FALSE),
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
/*                            ~RS2Dataset()                             */
/************************************************************************/

RS2Dataset::~RS2Dataset()

{
    FlushCache();

    CPLDestroyXMLNode( psProduct );
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

int RS2Dataset::CloseDependentDatasets()
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
/*                            GetFileList()                             */
/************************************************************************/

char **RS2Dataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();

    papszFileList = CSLInsertStrings( papszFileList, -1, papszExtraFiles );

    return papszFileList;
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int RS2Dataset::Identify( GDALOpenInfo *poOpenInfo )
{
   /* Check for the case where we're trying to read the calibrated data: */
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "RADARSAT_2_CALIB:")) {
        return TRUE;
    }

    /* Check for directory access when there is a product.xml file in the
       directory. */
    if( poOpenInfo->bIsDirectory )
    {
        CPLString osMDFilename =
            CPLFormCIFilename( poOpenInfo->pszFilename, "product.xml", NULL );

        VSIStatBufL sStat;
        if( VSIStatL( osMDFilename, &sStat ) == 0 )
            return TRUE;

        return FALSE;
    }

    /* otherwise, do our normal stuff */
    if( strlen(poOpenInfo->pszFilename) < 11
        || !EQUAL(poOpenInfo->pszFilename + strlen(poOpenInfo->pszFilename)-11,
                  "product.xml") )
        return FALSE;

    if( poOpenInfo->nHeaderBytes < 100 )
        return FALSE;

    if( strstr((const char *) poOpenInfo->pabyHeader, "/rs2" ) == NULL
        || strstr((const char *) poOpenInfo->pabyHeader, "<product" ) == NULL)
        return FALSE;

    return TRUE;
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

    if (STARTS_WITH_CI(pszFilename, "RADARSAT_2_CALIB:")) {
        pszFilename += 17;

        if (STARTS_WITH_CI(pszFilename, "BETA0"))
            eCalib = Beta0;
        else if (STARTS_WITH_CI(pszFilename, "SIGMA0"))
            eCalib = Sigma0;
        else if (STARTS_WITH_CI(pszFilename, "GAMMA"))
            eCalib = Gamma;
        else if (STARTS_WITH_CI(pszFilename, "UNCALIB"))
            eCalib = Uncalib;
        else
            eCalib = None;

        /* advance the pointer to the actual filename */
        while ( *pszFilename != '\0' && *pszFilename != ':' )
            pszFilename++;

        if (*pszFilename == ':')
            pszFilename++;

        //need to redo the directory check:
        //the GDALOpenInfo check would have failed because of the calibration string on the filename
        VSIStatBufL  sStat;
        if( VSIStatL( pszFilename, &sStat ) == 0 )
            poOpenInfo->bIsDirectory = VSI_ISDIR( sStat.st_mode );
    }

    CPLString osMDFilename;
    if( poOpenInfo->bIsDirectory )
    {
        osMDFilename =
            CPLFormCIFilename( pszFilename, "product.xml", NULL );
    }
    else
        osMDFilename = pszFilename;

/* -------------------------------------------------------------------- */
/*      Ingest the Product.xml file.                                    */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psProduct = CPLParseXMLFile( osMDFilename );
    if( psProduct == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLDestroyXMLNode( psProduct );
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The RS2 driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }

    CPLXMLNode *psImageAttributes = CPLGetXMLNode(psProduct, "=product.imageAttributes" );
    if( psImageAttributes == NULL )
    {
        CPLDestroyXMLNode( psProduct );
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to find <imageAttributes> in document." );
        return NULL;
    }

    CPLXMLNode *psImageGenerationParameters = CPLGetXMLNode( psProduct,
                                                 "=product.imageGenerationParameters" );
    if (psImageGenerationParameters == NULL) {
        CPLDestroyXMLNode( psProduct );
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to find <imageGenerationParameters> in document." );
        return NULL;
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
    if (poDS->nRasterXSize <= 1 || poDS->nRasterYSize <= 1) {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Non-sane raster dimensions provided in product.xml. If this is "
                  "a valid RADARSAT-2 scene, please contact your data provider for "
                  "a corrected dataset." );
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Check product type, as to determine if there are LUTs for       */
/*      calibration purposes.                                           */
/* -------------------------------------------------------------------- */

    const char *pszProductType = CPLGetXMLValue( psImageGenerationParameters,
                                                 "generalProcessingInformation.productType",
                                                 "UNK" );

    poDS->SetMetadataItem("PRODUCT_TYPE", pszProductType);

    /* the following cases can be assumed to have no LUTs, as per
     * RN-RP-51-2713, but also common sense
     */
    bool bCanCalib = false;
    if (!(STARTS_WITH_CI(pszProductType, "UNK") ||
          STARTS_WITH_CI(pszProductType, "SSG") ||
          STARTS_WITH_CI(pszProductType, "SPG")))
    {
        bCanCalib = true;
    }

/* -------------------------------------------------------------------- */
/*      Get dataType (so we can recognise complex data), and the        */
/*      bitsPerSample.                                                  */
/* -------------------------------------------------------------------- */
    const char *pszDataType =
        CPLGetXMLValue( psImageAttributes, "rasterAttributes.dataType",
                        "" );
    const int nBitsPerSample =
        atoi( CPLGetXMLValue( psImageAttributes,
                              "rasterAttributes.bitsPerSample", "" ) );

    GDALDataType eDataType;
    if( nBitsPerSample == 16 && EQUAL(pszDataType,"Complex") )
        eDataType = GDT_CInt16;
    else if( nBitsPerSample == 16 && STARTS_WITH_CI(pszDataType, "Mag") )
        eDataType = GDT_UInt16;
    else if( nBitsPerSample == 8 && STARTS_WITH_CI(pszDataType, "Mag") )
        eDataType = GDT_Byte;
    else
    {
        delete poDS;
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
    char *pszBeta0LUT = NULL;
    char *pszGammaLUT = NULL;
    char *pszSigma0LUT = NULL;

    char *pszPath = CPLStrdup(CPLGetPath( osMDFilename ));
    const int nFLen = static_cast<int>(osMDFilename.size());

    CPLXMLNode *psNode = psImageAttributes->psChild;
    for( ;
         psNode != NULL;
         psNode = psNode->psNext )
    {
        if( psNode->eType != CXT_Element
            || !(EQUAL(psNode->pszValue,"fullResolutionImageData")
                 || EQUAL(psNode->pszValue,"lookupTable")) )
            continue;

        if ( EQUAL(psNode->pszValue, "lookupTable") && bCanCalib ) {
            /* Determine which incidence angle correction this is */
            const char *pszLUTType = CPLGetXMLValue( psNode,
                                                     "incidenceAngleCorrection", "" );
            const char *pszLUTFile = CPLGetXMLValue( psNode, "", "" );
            CPLString osLUTFilePath = CPLFormFilename( pszPath, pszLUTFile,
                                                       NULL );

            if (EQUAL(pszLUTType, ""))
                continue;
            else if (EQUAL(pszLUTType, "Beta Nought") &&
                     IsValidXMLFile(pszPath,pszLUTFile))
            {
                poDS->papszExtraFiles =
                    CSLAddString( poDS->papszExtraFiles, osLUTFilePath );

                const size_t nBufLen = nFLen + 27;
                char *pszBuf = reinterpret_cast<char *>( CPLMalloc(nBufLen) );
                pszBeta0LUT = VSIStrdup( pszLUTFile );
                poDS->SetMetadataItem( "BETA_NOUGHT_LUT", pszLUTFile );

                snprintf(pszBuf, nBufLen, "RADARSAT_2_CALIB:BETA0:%s",
                        osMDFilename.c_str() );
                poDS->papszSubDatasets = CSLSetNameValue(
                    poDS->papszSubDatasets, "SUBDATASET_3_NAME", pszBuf );
                poDS->papszSubDatasets = CSLSetNameValue(
                    poDS->papszSubDatasets, "SUBDATASET_3_DESC",
                    "Beta Nought calibrated" );
                CPLFree(pszBuf);
            }
            else if (EQUAL(pszLUTType, "Sigma Nought") &&
                     IsValidXMLFile(pszPath,pszLUTFile))
            {
                poDS->papszExtraFiles =
                    CSLAddString( poDS->papszExtraFiles, osLUTFilePath );

                const size_t nBufLen = nFLen + 27;
                char *pszBuf = reinterpret_cast<char *>( CPLMalloc(nBufLen) );
                pszSigma0LUT = VSIStrdup( pszLUTFile );
                poDS->SetMetadataItem( "SIGMA_NOUGHT_LUT", pszLUTFile );

                snprintf(pszBuf, nBufLen,"RADARSAT_2_CALIB:SIGMA0:%s",
                        osMDFilename.c_str() );
                poDS->papszSubDatasets = CSLSetNameValue(
                    poDS->papszSubDatasets, "SUBDATASET_2_NAME", pszBuf );
                poDS->papszSubDatasets = CSLSetNameValue(
                    poDS->papszSubDatasets, "SUBDATASET_2_DESC",
                    "Sigma Nought calibrated" );
                CPLFree(pszBuf);
            }
            else if (EQUAL(pszLUTType, "Gamma") &&
                     IsValidXMLFile(pszPath,pszLUTFile))
            {
                poDS->papszExtraFiles =
                    CSLAddString( poDS->papszExtraFiles, osLUTFilePath );

                const size_t nBufLen = nFLen + 27;
                char *pszBuf = reinterpret_cast<char *>( CPLMalloc(nBufLen) );
                pszGammaLUT = VSIStrdup( pszLUTFile );
                poDS->SetMetadataItem( "GAMMA_LUT", pszLUTFile );
                snprintf(pszBuf, nBufLen,"RADARSAT_2_CALIB:GAMMA:%s",
                        osMDFilename.c_str());
                poDS->papszSubDatasets = CSLSetNameValue(
                    poDS->papszSubDatasets, "SUBDATASET_4_NAME", pszBuf );
                poDS->papszSubDatasets = CSLSetNameValue(
                    poDS->papszSubDatasets, "SUBDATASET_4_DESC",
                    "Gamma calibrated" );
                CPLFree(pszBuf);
            }
            continue;
        }

/* -------------------------------------------------------------------- */
/*      Fetch filename.                                                 */
/* -------------------------------------------------------------------- */
        const char *pszBasename = CPLGetXMLValue( psNode, "", "" );
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
        GDALDataset *poBandFile = reinterpret_cast<GDALDataset *>(
            GDALOpen( pszFullname, GA_ReadOnly ) );
        if( poBandFile == NULL )
        {
            CPLFree(pszFullname);
            continue;
        }
        if (poBandFile->GetRasterCount() == 0)
        {
            GDALClose( reinterpret_cast<GDALRasterBandH>( poBandFile ) );
            CPLFree(pszFullname);
            continue;
        }

        poDS->papszExtraFiles = CSLAddString( poDS->papszExtraFiles,
                                              pszFullname );

/* -------------------------------------------------------------------- */
/*      Create the band.                                                */
/* -------------------------------------------------------------------- */
        if (eCalib == None || eCalib == Uncalib) {
            RS2RasterBand *poBand
                = new RS2RasterBand( poDS, eDataType,
                                     CPLGetXMLValue( psNode, "pole", "" ),
                                     poBandFile );

            poDS->SetBand( poDS->GetRasterCount() + 1, poBand );
        }
        else {
            const char *pszLUT = NULL;
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
            RS2CalibRasterBand *poBand
                = new RS2CalibRasterBand( poDS, CPLGetXMLValue( psNode,
                                                                "pole", "" ), eDataType, poBandFile, eCalib,
                                          CPLFormFilename(pszPath, pszLUT, NULL) );
            poDS->SetBand( poDS->GetRasterCount() + 1, poBand );
        }

        CPLFree( pszFullname );
    }

    if (poDS->papszSubDatasets != NULL && eCalib == None) {
        const size_t nBufLen = nFLen + 28;
        char *pszBuf = reinterpret_cast<char *>( CPLMalloc(nBufLen) );
        snprintf(pszBuf, nBufLen, "RADARSAT_2_CALIB:UNCALIB:%s",
                osMDFilename.c_str() );
        poDS->papszSubDatasets = CSLSetNameValue( poDS->papszSubDatasets,
                                                  "SUBDATASET_1_NAME", pszBuf );
        poDS->papszSubDatasets = CSLSetNameValue( poDS->papszSubDatasets,
                                                  "SUBDATASET_1_DESC", "Uncalibrated digital numbers" );
        CPLFree(pszBuf);
    }
    else if (poDS->papszSubDatasets != NULL) {
        CSLDestroy( poDS->papszSubDatasets );
        poDS->papszSubDatasets = NULL;
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
    const char *pszItem = CPLGetXMLValue( psSourceAttrs,
                              "satellite", "" );
    poDS->SetMetadataItem( "SATELLITE_IDENTIFIER", pszItem );

    pszItem = CPLGetXMLValue( psSourceAttrs,
                              "sensor", "" );
    poDS->SetMetadataItem( "SENSOR_IDENTIFIER", pszItem );

    if (psSourceAttrs != NULL) {
        /* Get beam mode mnemonic */
        pszItem = CPLGetXMLValue( psSourceAttrs, "beamModeMnemonic", "UNK" );
        poDS->SetMetadataItem( "BEAM_MODE", pszItem );
        pszItem = CPLGetXMLValue( psSourceAttrs, "rawDataStartTime", "UNK" );
        poDS->SetMetadataItem( "ACQUISITION_START_TIME", pszItem );

        pszItem = CPLGetXMLValue( psSourceAttrs, "inputDatasetFacilityId", "UNK" );
        poDS->SetMetadataItem( "FACILITY_IDENTIFIER", pszItem );

        pszItem = CPLGetXMLValue( psSourceAttrs,
            "orbitAndAttitude.orbitInformation.passDirection", "UNK" );
        poDS->SetMetadataItem( "ORBIT_DIRECTION", pszItem );
        pszItem = CPLGetXMLValue( psSourceAttrs,
            "orbitAndAttitude.orbitInformation.orbitDataSource", "UNK" );
        poDS->SetMetadataItem( "ORBIT_DATA_SOURCE", pszItem );
        pszItem = CPLGetXMLValue( psSourceAttrs,
            "orbitAndAttitude.orbitInformation.orbitDataFile", "UNK" );
        poDS->SetMetadataItem( "ORBIT_DATA_FILE", pszItem );
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

        pszItem = CPLGetXMLValue( psSarProcessingInformation,
                                  "sarProcessingInformation.zeroDopplerTimeFirstLine", "UNK" );
        poDS->SetMetadataItem( "FIRST_LINE_TIME", pszItem );

        pszItem = CPLGetXMLValue( psSarProcessingInformation,
                                  "sarProcessingInformation.zeroDopplerTimeLastLine", "UNK" );
        poDS->SetMetadataItem( "LAST_LINE_TIME", pszItem );

        pszItem = CPLGetXMLValue( psSarProcessingInformation,
                                  "generalProcessingInformation.productType", "UNK" );
        poDS->SetMetadataItem( "PRODUCT_TYPE", pszItem );

        pszItem = CPLGetXMLValue( psSarProcessingInformation,
                                  "generalProcessingInformation.processingFacility", "UNK" );
        poDS->SetMetadataItem( "PROCESSING_FACILITY", pszItem );

        pszItem = CPLGetXMLValue( psSarProcessingInformation,
                                  "generalProcessingInformation.processingTime", "UNK" );
        poDS->SetMetadataItem( "PROCESSING_TIME", pszItem );
    }

/*--------------------------------------------------------------------- */
/*      Collect Map projection/Geotransform information, if present     */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psMapProjection =
        CPLGetXMLNode( psImageAttributes,
                       "geographicInformation.mapProjection" );

    if ( psMapProjection != NULL ) {
        CPLXMLNode *psPos =
            CPLGetXMLNode( psMapProjection, "positioningInformation" );

        pszItem = CPLGetXMLValue( psMapProjection,
                                  "mapProjectionDescriptor", "UNK" );
        poDS->SetMetadataItem( "MAP_PROJECTION_DESCRIPTOR", pszItem );

        pszItem = CPLGetXMLValue( psMapProjection,
                                  "mapProjectionOrientation", "UNK" );
        poDS->SetMetadataItem( "MAP_PROJECTION_ORIENTATION", pszItem );

        pszItem = CPLGetXMLValue( psMapProjection,
                                  "resamplingKernel", "UNK" );
        poDS->SetMetadataItem( "RESAMPLING_KERNEL", pszItem );

        pszItem = CPLGetXMLValue( psMapProjection,
                                  "satelliteHeading", "UNK" );
        poDS->SetMetadataItem( "SATELLITE_HEADING", pszItem );

        if (psPos != NULL) {
            const double tl_x = CPLStrtod(CPLGetXMLValue(
                psPos, "upperLeftCorner.mapCoordinate.easting", "0.0" ), NULL);
            const double tl_y = CPLStrtod(CPLGetXMLValue(
                psPos, "upperLeftCorner.mapCoordinate.northing", "0.0" ), NULL);
            const double bl_x = CPLStrtod(CPLGetXMLValue(
                psPos, "lowerLeftCorner.mapCoordinate.easting", "0.0" ), NULL);
            const double bl_y = CPLStrtod(CPLGetXMLValue(
                psPos, "lowerLeftCorner.mapCoordinate.northing", "0.0" ), NULL);
            const double tr_x = CPLStrtod(CPLGetXMLValue(
                psPos, "upperRightCorner.mapCoordinate.easting", "0.0" ), NULL);
            const double tr_y = CPLStrtod(CPLGetXMLValue(
                psPos, "upperRightCorner.mapCoordinate.northing", "0.0" ), NULL);
            poDS->adfGeoTransform[1] = (tr_x - tl_x)/(poDS->nRasterXSize - 1);
            poDS->adfGeoTransform[4] = (tr_y - tl_y)/(poDS->nRasterXSize - 1);
            poDS->adfGeoTransform[2] = (bl_x - tl_x)/(poDS->nRasterYSize - 1);
            poDS->adfGeoTransform[5] = (bl_y - tl_y)/(poDS->nRasterYSize - 1);
            poDS->adfGeoTransform[0] = (tl_x - 0.5*poDS->adfGeoTransform[1]
                                        - 0.5*poDS->adfGeoTransform[2]);
            poDS->adfGeoTransform[3] = (tl_y - 0.5*poDS->adfGeoTransform[4]
                                        - 0.5*poDS->adfGeoTransform[5]);

            /* Use bottom right pixel to test geotransform */
            const double br_x = CPLStrtod(CPLGetXMLValue(
                psPos, "lowerRightCorner.mapCoordinate.easting", "0.0"  ), NULL);
            const double br_y = CPLStrtod(CPLGetXMLValue(
                psPos, "lowerRightCorner.mapCoordinate.northing", "0.0"  ), NULL);
            const double testx = poDS->adfGeoTransform[0] + poDS->adfGeoTransform[1] *
                (poDS->nRasterXSize - 0.5) + poDS->adfGeoTransform[2] *
                (poDS->nRasterYSize - 0.5);
            const double testy = poDS->adfGeoTransform[3] + poDS->adfGeoTransform[4] *
                (poDS->nRasterXSize - 0.5) + poDS->adfGeoTransform[5] *
                (poDS->nRasterYSize - 0.5);

            /* Give 1/4 pixel numerical error leeway in calculating location
               based on affine transform */
            if ( (fabs(testx - br_x) >
                  fabs(0.25*(poDS->adfGeoTransform[1]+poDS->adfGeoTransform[2])))
                 || (fabs(testy - br_y) > fabs(0.25*(poDS->adfGeoTransform[4] +
                                                     poDS->adfGeoTransform[5]))) )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Unexpected error in calculating affine transform: "
                          "corner coordinates inconsistent.");
            }
            else
            {
                poDS->bHaveGeoTransform = TRUE;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Collect Projection String Information                           */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psEllipsoid =
        CPLGetXMLNode( psImageAttributes,
                       "geographicInformation.referenceEllipsoidParameters" );

    if ( psEllipsoid != NULL ) {
        OGRSpatialReference oLL, oPrj;

        const char *pszEllipsoidName
            = CPLGetXMLValue( psEllipsoid, "ellipsoidName", "" );
        double minor_axis
            = CPLAtof(CPLGetXMLValue( psEllipsoid, "semiMinorAxis", "0.0" ));
        double major_axis
            = CPLAtof(CPLGetXMLValue( psEllipsoid, "semiMajorAxis", "0.0" ));

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

        if ( psMapProjection != NULL ) {
            const char *pszProj = CPLGetXMLValue(
                psMapProjection, "mapProjectionDescriptor", "" );
            bool bUseProjInfo = false;

            CPLXMLNode *psUtmParams =
                CPLGetXMLNode( psMapProjection,
                               "utmProjectionParameters" );

            CPLXMLNode *psNspParams =
                CPLGetXMLNode( psMapProjection,
                               "nspProjectionParameters" );

            if ((psUtmParams != NULL) && poDS->bHaveGeoTransform ) {
                /* double origEasting, origNorthing; */
                bool bNorth = true;

                const int utmZone = atoi(CPLGetXMLValue( psUtmParams, "utmZone", "" ));
                const char *pszHemisphere = CPLGetXMLValue(
                    psUtmParams, "hemisphere", "" );
#if 0
                origEasting = CPLStrtod(CPLGetXMLValue(
                    psUtmParams, "mapOriginFalseEasting", "0.0" ), NULL);
                origNorthing = CPLStrtod(CPLGetXMLValue(
                    psUtmParams, "mapOriginFalseNorthing", "0.0" ), NULL);
#endif
                if ( STARTS_WITH_CI(pszHemisphere, "southern") )
                    bNorth = FALSE;

                if (STARTS_WITH_CI(pszProj, "UTM")) {
                    oPrj.SetUTM(utmZone, bNorth);
                    bUseProjInfo = true;
                }
            }
            else if ((psNspParams != NULL) && poDS->bHaveGeoTransform) {
                const double origEasting = CPLStrtod(CPLGetXMLValue(
                    psNspParams, "mapOriginFalseEasting", "0.0" ), NULL);
                const double origNorthing = CPLStrtod(CPLGetXMLValue(
                    psNspParams, "mapOriginFalseNorthing", "0.0" ), NULL);
                const double copLong = CPLStrtod(CPLGetXMLValue(
                    psNspParams, "centerOfProjectionLongitude", "0.0" ), NULL);
                const double copLat = CPLStrtod(CPLGetXMLValue(
                    psNspParams, "centerOfProjectionLatitude", "0.0" ), NULL);
                const double sP1 = CPLStrtod(CPLGetXMLValue( psNspParams,
                                             "standardParallels1", "0.0" ), NULL);
                const double sP2 = CPLStrtod(CPLGetXMLValue( psNspParams,
                                             "standardParallels2", "0.0" ), NULL);

                if (STARTS_WITH_CI(pszProj, "ARC")) {
                    /* Albers Conical Equal Area */
                    oPrj.SetACEA(sP1, sP2, copLat, copLong, origEasting,
                                 origNorthing);
                    bUseProjInfo = true;
                }
                else if (STARTS_WITH_CI(pszProj, "LCC")) {
                    /* Lambert Conformal Conic */
                    oPrj.SetLCC(sP1, sP2, copLat, copLong, origEasting,
                                origNorthing);
                    bUseProjInfo = true;
                }
                else if (STARTS_WITH_CI(pszProj,"STPL")) {
                    /* State Plate
                       ASSUMPTIONS: "zone" in product.xml matches USGS
                       definition as expected by ogr spatial reference; NAD83
                       zones (versus NAD27) are assumed. */

                    const int nSPZone = atoi(CPLGetXMLValue( psNspParams,
                                                             "zone", "1" ));

                    oPrj.SetStatePlane( nSPZone, TRUE, NULL, 0.0 );
                    bUseProjInfo = true;
                }
            }

            if (bUseProjInfo) {
                CPLFree( poDS->pszProjection );
                poDS->pszProjection = NULL;
                oPrj.exportToWkt( &(poDS->pszProjection) );
            }
            else {
                CPLError(CE_Warning,CPLE_AppDefined,"Unable to interpret "
                         "projection information; check mapProjection info in "
                         "product.xml!");
            }
        }

        CPLFree( poDS->pszGCPProjection );
        poDS->pszGCPProjection = NULL;
        oLL.exportToWkt( &(poDS->pszGCPProjection) );
    }

/* -------------------------------------------------------------------- */
/*      Collect GCPs.                                                   */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psGeoGrid =
        CPLGetXMLNode( psImageAttributes,
                       "geographicInformation.geolocationGrid" );

    if( psGeoGrid != NULL ) {
        /* count GCPs */
        poDS->nGCPCount = 0;

        for( psNode = psGeoGrid->psChild; psNode != NULL;
             psNode = psNode->psNext )
        {
            if( EQUAL(psNode->pszValue,"imageTiePoint") )
                poDS->nGCPCount++ ;
        }

        poDS->pasGCPList = reinterpret_cast<GDAL_GCP *>(
            CPLCalloc( sizeof(GDAL_GCP), poDS->nGCPCount ) );

        poDS->nGCPCount = 0;

        for( psNode = psGeoGrid->psChild; psNode != NULL;
             psNode = psNode->psNext )
        {
            GDAL_GCP   *psGCP = poDS->pasGCPList + poDS->nGCPCount;

            if( !EQUAL(psNode->pszValue,"imageTiePoint") )
                continue;

            poDS->nGCPCount++ ;

            char szID[32];
            snprintf( szID, sizeof(szID), "%d", poDS->nGCPCount );
            psGCP->pszId = CPLStrdup( szID );
            psGCP->pszInfo = CPLStrdup("");
            psGCP->dfGCPPixel =
                CPLAtof(CPLGetXMLValue(psNode,"imageCoordinate.pixel","0"));
            psGCP->dfGCPLine =
                CPLAtof(CPLGetXMLValue(psNode,"imageCoordinate.line","0"));
            psGCP->dfGCPX =
                CPLAtof(CPLGetXMLValue(psNode,"geodeticCoordinate.longitude",""));
            psGCP->dfGCPY =
                CPLAtof(CPLGetXMLValue(psNode,"geodeticCoordinate.latitude",""));
            psGCP->dfGCPZ =
                CPLAtof(CPLGetXMLValue(psNode,"geodeticCoordinate.height",""));
        }
    }

    CPLFree( pszPath );
    if (pszBeta0LUT) CPLFree(pszBeta0LUT);
    if (pszSigma0LUT) CPLFree(pszSigma0LUT);
    if (pszGammaLUT) CPLFree(pszGammaLUT);

/* -------------------------------------------------------------------- */
/*      Collect RPC.                                                   */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psRationalFunctions =
        CPLGetXMLNode( psImageAttributes,
                       "geographicInformation.rationalFunctions" );
    if( psRationalFunctions != NULL ) {
        char** papszRPC = NULL;
        static const char* const apszXMLToGDALMapping[] =
        {
            "biasError", "ERR_BIAS",
            "randomError", "ERR_RAND",
            //"lineFitQuality", "????",
            //"pixelFitQuality", "????",
            "lineOffset", "LINE_OFF",
            "pixelOffset", "SAMP_OFF",
            "latitudeOffset", "LAT_OFF",
            "longitudeOffset", "LONG_OFF",
            "heightOffset", "HEIGHT_OFF",
            "lineScale", "LINE_SCALE",
            "pixelScale", "SAMP_SCALE",
            "latitudeScale", "LAT_SCALE",
            "longitudeScale", "LONG_SCALE",
            "heightScale", "HEIGHT_SCALE",
            "lineNumeratorCoefficients", "LINE_NUM_COEFF",
            "lineDenominatorCoefficients", "LINE_DEN_COEFF",
            "pixelNumeratorCoefficients", "SAMP_NUM_COEFF",
            "pixelDenominatorCoefficients", "SAMP_DEN_COEFF",
        };
        for( size_t i = 0; i < CPL_ARRAYSIZE(apszXMLToGDALMapping); i+=2 )
        {
            const char* pszValue = CPLGetXMLValue(psRationalFunctions, apszXMLToGDALMapping[i], NULL);
            if( pszValue )
                papszRPC = CSLSetNameValue(papszRPC, apszXMLToGDALMapping[i+1], pszValue);
        }
        poDS->GDALDataset::SetMetadata(papszRPC, "RPC");
        CSLDestroy(papszRPC);
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    CPLString osDescription;

    switch (eCalib) {
      case Sigma0:
        osDescription.Printf( "RADARSAT_2_CALIB:SIGMA0:%s",
                              osMDFilename.c_str() );
        break;
      case Beta0:
        osDescription.Printf( "RADARSAT_2_CALIB:BETA0:%s",
                              osMDFilename.c_str());
        break;
      case Gamma:
        osDescription.Printf( "RADARSAT_2_CALIB:GAMMA0:%s",
                              osMDFilename.c_str() );
        break;
      case Uncalib:
        osDescription.Printf( "RADARSAT_2_CALIB:UNCALIB:%s",
                              osMDFilename.c_str() );
        break;
      default:
        osDescription = osMDFilename;
    }

    if( eCalib != None )
        poDS->papszExtraFiles =
            CSLAddString( poDS->papszExtraFiles, osMDFilename );

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
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *RS2Dataset::GetProjectionRef()

{
    return pszProjection;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr RS2Dataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform,  adfGeoTransform, sizeof(double) * 6 );

    if (bHaveGeoTransform)
        return CE_None;

    return CE_Failure;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **RS2Dataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "SUBDATASETS", NULL);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **RS2Dataset::GetMetadata( const char *pszDomain )

{
    if( pszDomain != NULL && STARTS_WITH_CI(pszDomain, "SUBDATASETS") &&
        papszSubDatasets != NULL)
        return papszSubDatasets;

    return GDALDataset::GetMetadata( pszDomain );
}

/************************************************************************/
/*                         GDALRegister_RS2()                          */
/************************************************************************/

void GDALRegister_RS2()

{
    if( GDALGetDriverByName( "RS2" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "RS2" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "RadarSat 2 XML Product" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_rs2.html" );
    poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = RS2Dataset::Open;
    poDriver->pfnIdentify = RS2Dataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
